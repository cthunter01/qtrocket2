#include "QtRocket/gui/plot/ThrustCurvePlot.h"

#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <QChar>
#include <QColor>
#include <QEvent>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/PlotExport.h"
#include "QtRocket/gui/plot/PlotInputBindings.h"
#include "QtRocket/gui/plot/PlotTheme.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// JFreeChart's NumberAxis fit of data from 0 (autoRangeStickyZero, the range axis'
/// autoRangeIncludesZero): [0, max + kAxisMargin * max]. rocketplot's rule that takes an axis to
/// zero from any distance (an infinite share) gives exactly that range.
void fitLikeJFreeChart(rocketplot::Axis& axis)
{
    axis.setAutoscaleMargin(ThrustCurvePlot::kAxisMargin);
    axis.setAutoscaleIncludesZero(std::numeric_limits<double>::infinity());
    axis.setGridVisible(true);
}

/// @p values, each converted from SI to @p unit (Unit::toUnit()).
[[nodiscard]] std::vector<double> inUnit(const std::vector<double>& values, const Unit& unit)
{
    std::vector<double> converted;
    converted.reserve(values.size());
    for (const double value : values)
    {
        converted.push_back(unit.toUnit(value));
    }
    return converted;
}

/// ThrustCurveMotorPlotDialog.generateSeries()'s name of @p motor: the manufacturer
/// (Manufacturer.toString(), its display name) and the designation, and " (n)" for @p number > 0.
[[nodiscard]] QString dialogName(const ThrustCurveMotor& motor, int number)
{
    QString name = QtStrings::fromUtf8(motor.getManufacturer().toString()) + QLatin1Char(' ') +
                   QtStrings::fromUtf8(motor.getDesignation());
    if (number > 0)
    {
        name += QStringLiteral(" (%1)").arg(number);
    }
    return name;
}

}  // namespace

ThrustCurvePlot::ThrustCurvePlot(QWidget* parent) : ThrustCurvePlot(Style::PREVIEW, parent) { }

ThrustCurvePlot::ThrustCurvePlot(const GuiServices& services, QWidget* parent)
  : ThrustCurvePlot(Style::DIALOG, parent)
{
    PlotExport::install(*m_plot, services);
}

ThrustCurvePlot::ThrustCurvePlot(Style style, QWidget* parent)
  : QWidget(parent), m_style(style), m_plot(makeChild<rocketplot::PlotWidget>(this))
{
    // OpenRocket's names: the ChartPanel "chartPanel", its JFreeChart "chart".
    setObjectName(QStringLiteral("chartPanel"));
    m_plot->setObjectName(QStringLiteral("chart"));
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    // The look and the curves' colours by one rule (rocketplot's SYSTEM mode has its own).
    PlotTheme::apply(*m_plot, palette());
    fitLikeJFreeChart(*m_plot->xAxis());
    fitLikeJFreeChart(*m_plot->yAxis());
    if (m_style == Style::PREVIEW)
    {
        //: TCMotorSelPan.title.Thrustcurve
        m_plot->setTitle(rocketplot::literalText(tr("Thrust curve:")));
        m_plot->legend()->setVisible(false);
        PlotInputBindings::setNoInteraction(*m_plot);
        // A ChartPanel without a popup menu shows none, nor its parent's.
        setContextMenuPolicy(Qt::PreventContextMenu);
    }
    else
    {
        //: Not translated in OpenRocket (ThrustCurveMotorPlotDialog.java): the chart's title
        m_plot->setTitle(rocketplot::literalText(tr("Motor thrust curves")));
        m_plot->legend()->setVisible(true);
        m_plot->legend()->setAnchor(rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
        m_plot->setInputBindings(PlotInputBindings::chartPanel());
        m_plot->setCrosshairEnabled(false);
        m_plot->setCrosshairMode(rocketplot::CrosshairMode::SNAP);
    }
    clear();
}

void ThrustCurvePlot::setCurves(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
                                int                                                      selected)
{
    QTROCKET_ASSERT(selected >= -1 && selected < std::ssize(motors));
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : motors)
    {
        QTROCKET_ASSERT(motor != nullptr);
    }
    if (motors.empty())
    {
        // No data to fit: JFreeChart's empty dataset, not rocketplot's [0, 1] of no data.
        clear();
        return;
    }
    m_plot->clearSeries();
    m_curves.clear();
    m_selected = selected;
    m_plot->xAxis()->setAutoscale(true);
    m_plot->yAxis()->setAutoscale(true);
    if (m_style == Style::PREVIEW)
    {
        addPreviewCurves(motors, selected);
    }
    else
    {
        updateAxisLabels();
        addDialogCurves(motors, selected);
    }
}

void ThrustCurvePlot::clear()
{
    m_plot->clearSeries();
    m_curves.clear();
    m_selected = -1;
    m_plot->xAxis()->setRange(0.0, kEmptyRangeMax);
    m_plot->yAxis()->setRange(0.0, kEmptyRangeMax);
    if (m_style == Style::DIALOG)
    {
        updateAxisLabels();
    }
}

void ThrustCurvePlot::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        // This widget's palette: the plot's own follows only after this event.
        PlotTheme::apply(*m_plot, palette());
        recolor();
    }
}

void ThrustCurvePlot::updateAxisLabels()
{
    const QString time = QtStrings::unitSymbol(unitGroup(UnitGroupId::SHORT_TIME).getDefaultUnit());
    const QString force = QtStrings::unitSymbol(unitGroup(UnitGroupId::FORCE).getDefaultUnit());
    //: Not translated in OpenRocket (ThrustCurveMotorPlotDialog.java): the time axis, %1 its unit
    m_plot->xAxis()->setLabel(rocketplot::literalText(tr("Time / %1").arg(time)));
    //: Not translated in OpenRocket (ThrustCurveMotorPlotDialog.java): the thrust axis, %1 its unit
    m_plot->yAxis()->setLabel(rocketplot::literalText(tr("Thrust / %1").arg(force)));
}

rocketplot::LineSeries& ThrustCurvePlot::addCurve(const ThrustCurveMotor& motor,
                                                  const QString& name, int colorIndex,
                                                  double lineWidth)
{
    rocketplot::LineSeries* series = nullptr;
    if (m_style == Style::PREVIEW)
    {
        series = m_plot->addLine(motor.getTimePoints(), motor.getThrustPoints(),
                                 rocketplot::literalText(name));
    }
    else
    {
        // The units the axes name (OpenRocket plots newtons whatever its label says: bug 12).
        series = m_plot->addLine(
            inUnit(motor.getTimePoints(), unitGroup(UnitGroupId::SHORT_TIME).getDefaultUnit()),
            inUnit(motor.getThrustPoints(), unitGroup(UnitGroupId::FORCE).getDefaultUnit()),
            rocketplot::literalText(name));
    }
    const QColor color = PlotColors::curveColor(colorIndex, palette());
    series->setColor(color);
    series->setLineWidth(lineWidth);
    // Java's BasicStroke ends its lines square.
    series->setCapStyle(Qt::SquareCap);
    series->setMarker(rocketplot::Marker::NONE);
    m_curves.push_back({.series = series, .colorIndex = colorIndex, .color = color});
    return *series;
}

void ThrustCurvePlot::addPreviewCurves(
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected)
{
    // From the last, so that the first is drawn on top (JFreeChart's REVERSE rendering order).
    for (std::size_t k = motors.size(); k-- > 0;)
    {
        const int index = static_cast<int>(k);
        //: TCMotorSelPan.title.Thrust
        const QString name = tr("Thrust") + QStringLiteral(" (%1)").arg(index);
        addCurve(*motors[k], name, index,
                 index == selected ? kPreviewSelectedLineWidth : kPreviewLineWidth);
    }
}

void ThrustCurvePlot::addDialogCurves(
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected)
{
    if (selected >= 0)
    {
        const ThrustCurveMotor& motor = *motors[static_cast<std::size_t>(selected)];
        rocketplot::LineSeries& chosen =
            addCurve(motor, dialogName(motor, 0), selected, kDialogLineWidth);
        chosen.setMarker(rocketplot::Marker::SQUARE);
        chosen.setMarkerSize(kDialogMarkerSize);
        // rocketplot's ring of background colour around each marker would erase the line beside
        // it (rocketplot v0.6.0's N1); JFreeChart draws none.
        chosen.setMarkerRingWidth(0.0);
        chosen.setMarkersAlwaysVisible(true);
    }
    // The others are numbered from 1, with a selection or without one (Java's n++).
    int number = 1;
    for (std::size_t k = 0; k < motors.size(); ++k)
    {
        const int index = static_cast<int>(k);
        if (index == selected)
        {
            continue;
        }
        addCurve(*motors[k], dialogName(*motors[k], number), index, kDialogLineWidth);
        ++number;
    }
}

void ThrustCurvePlot::recolor()
{
    for (Curve& curve : m_curves)
    {
        if (curve.series.isNull() || curve.series->color() != curve.color)
        {
            continue;
        }
        curve.color = PlotColors::curveColor(curve.colorIndex, palette());
        curve.series->setColor(curve.color);
    }
}

}  // namespace QtRocket::Gui
