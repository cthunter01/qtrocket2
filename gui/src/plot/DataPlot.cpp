#include "QtRocket/gui/plot/DataPlot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <QColor>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/ReferenceLine.h>
#include <rocketplot/Series.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "QtRocket/gui/plot/PlotExport.h"
#include "QtRocket/gui/plot/PlotInputBindings.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui
{

namespace
{

/// rocketplot's bounds of a range an axis can show (core::isUsableRange() of its
/// src/core/AxisMapping.cpp, on a linear scale, which Axis::setHomeRange() checks): its span at
/// most kMaxShowableSpan, and more than kMinShowableShare of the larger magnitude of its ends.
constexpr double kMaxShowableSpan  = 1e300;
constexpr double kMinShowableShare = 1e-13;

/// @p range if rocketplot can show it as a home range, else nullopt (DataPlot::setHomeRanges()):
/// finite, its minimum at most its maximum (rocketplot::Range::isValid()), and of a span within
/// rocketplot's bounds and above the least normal double, as rocketplot checks it; any other
/// range Axis::setHomeRange() would ignore with a warning.
[[nodiscard]] std::optional<rocketplot::Range> showableRange(std::optional<rocketplot::Range> range)
{
    if (!range.has_value() || !range->isValid())
    {
        return std::nullopt;
    }
    const double span      = range->span();
    const double magnitude = std::max(std::abs(range->min), std::abs(range->max));
    if (span <= kMaxShowableSpan && span > magnitude * kMinShowableShare &&
        span > std::numeric_limits<double>::min())
    {
        return range;
    }
    return std::nullopt;
}

/// The dashes of @p style as a pen's pattern in pixels (Plot.createPlotStroke(): none for SOLID,
/// else LineStyle.getDashes()).
[[nodiscard]] QList<qreal> dashPatternOf(LineStyle style)
{
    QList<qreal> pattern;
    if (style == LineStyle::SOLID)
    {
        return pattern;
    }
    for (const double length : dashes(style))
    {
        pattern.append(length);
    }
    return pattern;
}

/// The readout's tool tip through QToolTip (DataPlot::ReadoutHook), over @p plot.
void showToolTip(rocketplot::PlotWidget* plot, const QPoint& globalPosition, const QString& text)
{
    if (text.isEmpty())
    {
        QToolTip::hideText();
    }
    else
    {
        QToolTip::showText(globalPosition, text, plot);
    }
}

}  // namespace

DataPlot::DataPlot(const GuiServices& services, bool showPoints, QWidget* parent)
  : QWidget(parent),
    m_services(&services),
    m_plot(makeChild<rocketplot::PlotWidget>(this)),
    m_showPoints(showPoints)
{
    // OpenRocket's names: the ChartPanel "chartPanel", its JFreeChart "chart".
    setObjectName(QStringLiteral("chartPanel"));
    m_plot->setObjectName(QStringLiteral("chart"));
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    // rocketplot's ThemeMode::SYSTEM with PlotTheme's rule (Application). JFreeChart's XYPlot
    // renders its datasets and their series in reverse order: the first series on top.
    m_plot->setSeriesDrawOrder(rocketplot::SeriesDrawOrder::REVERSE);
    m_plot->legend()->setVisible(true);
    m_plot->legend()->setAnchor(rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
    m_plot->legend()->setValuesVisible(false);
    m_plot->xAxis()->setGridVisible(true);
    m_plot->yAxis()->setGridVisible(true);
    m_plot->yAxis2()->setGridVisible(false);
    // An axis shows its home range (setHomeRanges()), which holds the axis fitting's own margin
    // and zero rule. One without (no data on it, or a range rocketplot cannot show) fits its data
    // as they are, without rocketplot's margin besides: rocketplot's zero rule and its alignment
    // of the y axes' zeros are off already (Axis::autoscaleIncludesZero() 0,
    // PlotWidget::areYAxesZeroAligned() false, its defaults).
    for (rocketplot::Axis* const axis : {m_plot->xAxis(), m_plot->yAxis(), m_plot->yAxis2()})
    {
        axis->setAutoscaleMargin(0.0);
    }

    // Plot: addRangeMarker(new ValueMarker(0)), on the first range axis, over the series.
    rocketplot::ReferenceLine* const zeroLine = m_plot->addHorizontalLine(0.0);
    zeroLine->setLineStyle(Qt::SolidLine);
    zeroLine->setLineWidth(kZeroLineWidth);
    QColor zeroColor = QColor::fromRgb(kZeroLineGrey, kZeroLineGrey, kZeroLineGrey);
    zeroColor.setAlphaF(static_cast<float>(kZeroLineAlpha));
    zeroLine->setColor(zeroColor);

    // SimulationChart.
    m_plot->setInputBindings(PlotInputBindings::simulationChart());
    m_plot->setCrosshairMode(rocketplot::CrosshairMode::SNAP);
    m_plot->setCrosshairEnabled(true);
    PlotExport::install(*m_plot, services);
    connect(m_plot, &rocketplot::PlotWidget::crosshairMoved, this,
            guarded([this] { updateReadout(); }));
}

DataPlot::~DataPlot()
{
    // The plot, a child, goes after this object's members: nothing it signals while it goes may
    // reach them.
    disconnect(m_plot, nullptr, this, nullptr);
    // A tool tip QToolTip shows for this plot goes with it.
    if (!m_readout.isEmpty() && !m_readoutHook)
    {
        QToolTip::hideText();
    }
}

rocketplot::LineSeries* DataPlot::series(std::size_t index) const
{
    QTROCKET_ASSERT(index < m_series.size());
    return m_series[index].data();
}

void DataPlot::setShowPoints(bool show)
{
    m_showPoints = show;
    for (std::size_t i = 0; i < m_series.size(); ++i)
    {
        if (rocketplot::LineSeries* const line = m_series[i].data(); line != nullptr)
        {
            applyPoints(*line, m_shapes[i]);
        }
    }
}

void DataPlot::setShowEvents(bool /*show*/)
{
    // Plot.setShowEvents(): "Default implementation does nothing".
}

QSize DataPlot::sizeHint() const
{
    return {kPreferredWidth, kPreferredHeight};
}

void DataPlot::zoomIn(bool domainOnly)
{
    m_plot->zoom(kZoomFactor, domainOnly ? Qt::Horizontal : Qt::Horizontal | Qt::Vertical);
}

void DataPlot::zoomOut(bool domainOnly)
{
    m_plot->zoom(1.0 / kZoomFactor, domainOnly ? Qt::Horizontal : Qt::Horizontal | Qt::Vertical);
}

void DataPlot::resetZoom()
{
    m_plot->resetView();
}

bool DataPlot::exportImage(const UserPrompts::SaveFileRequest& request)
{
    return PlotExport::exportPlot(*m_plot, *m_services, request);
}

void DataPlot::setReadoutHook(ReadoutHook hook)
{
    m_readoutHook = std::move(hook);
}

rocketplot::Marker DataPlot::pointShape(std::size_t shape) noexcept
{
    return kPointShapes.at(shape % kPointShapes.size());
}

double DataPlot::pointSize(rocketplot::Marker shape) noexcept
{
    const bool upright = shape == rocketplot::Marker::SQUARE ||
                         shape == rocketplot::Marker::RECTANGLE_WIDE ||
                         shape == rocketplot::Marker::RECTANGLE_TALL;
    return upright ? kPointSide / kRocketplotSquareScale : kPointSide;
}

void DataPlot::setTitles(const QString& title, const QString& subtitle)
{
    m_plot->setTitle(rocketplot::literalText(title));
    m_plot->setSubtitle(rocketplot::literalText(subtitle));
}

void DataPlot::setAxisLabels(const QString& domain, const QString& left, const QString& right)
{
    m_plot->xAxis()->setLabel(rocketplot::literalText(domain));
    m_plot->yAxis()->setLabel(rocketplot::literalText(left));
    m_plot->yAxis2()->setLabel(rocketplot::literalText(right));
}

rocketplot::LineSeries& DataPlot::addSeries(std::vector<double>&& x, std::vector<double>&& y,
                                            const SeriesLook& look)
{
    QTROCKET_ASSERT(look.axis == 0 || look.axis == 1);
    rocketplot::LineSeries* const line =
        m_plot->addLine(std::move(x), std::move(y), rocketplot::literalText(look.name));
    line->setLegendGroup(look.legendGroup);
    if (look.axis == 1)
    {
        line->setYAxis(m_plot->yAxis2());
    }
    line->setColor(look.color);
    line->setLineWidth(kLineWidth);
    line->setCapStyle(Qt::FlatCap);
    line->setDashPattern(dashPatternOf(look.lineStyle));
    m_series.emplace_back(line);
    m_shapes.push_back(look.shape);
    applyPoints(*line, look.shape);
    return *line;
}

void DataPlot::setHomeRanges(std::optional<rocketplot::Range> x, std::optional<rocketplot::Range> y,
                             std::optional<rocketplot::Range> y2)
{
    m_plot->xAxis()->setHomeRange(showableRange(x));
    m_plot->yAxis()->setHomeRange(showableRange(y));
    m_plot->yAxis2()->setHomeRange(showableRange(y2));
}

void DataPlot::updateReadout()
{
    const rocketplot::Series* const  snapped = m_plot->crosshairSeries();
    const std::optional<std::size_t> point   = m_plot->crosshairIndex();
    QString                          text;
    QPoint                           position;
    if (snapped != nullptr && point.has_value())
    {
        for (std::size_t i = 0; i < m_series.size(); ++i)
        {
            if (m_series[i].data() == snapped && *point < snapped->size())
            {
                text             = toolTipText(i, *point);
                const QPointF at = m_plot->mapFromData(
                    QPointF(snapped->x(*point), snapped->y(*point)), snapped->yAxis());
                position = m_plot->mapToGlobal(at.toPoint());
                break;
            }
        }
    }
    if (text.isEmpty() && m_readout.isEmpty())
    {
        return;
    }
    m_readout = text;
    if (m_readoutHook)
    {
        m_readoutHook(position, text);
    }
    else
    {
        showToolTip(m_plot, position, text);
    }
}

void DataPlot::applyPoints(rocketplot::LineSeries& series, std::size_t shape) const
{
    if (!m_showPoints)
    {
        series.setMarker(rocketplot::Marker::NONE);
        return;
    }
    const rocketplot::Marker marker = pointShape(shape);
    series.setMarker(marker);
    series.setMarkerSize(pointSize(marker));
    // rocketplot's ring of the background's colour around each marker would cut the line beside
    // it (rocketplot v0.6.0's N1); JFreeChart draws none. JFreeChart draws every point's shape.
    series.setMarkerRingWidth(0.0);
    series.setMarkersAlwaysVisible(true);
}

}  // namespace QtRocket::Gui
