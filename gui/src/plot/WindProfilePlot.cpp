#include "QtRocket/gui/plot/WindProfilePlot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QIconEngine>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QRgb>
#include <QSizeF>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <rocketplot/Axis.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/ScatterSeries.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "QtRocket/gui/plot/PlotInputBindings.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

using Level = WindProfilePlot::Level;

/// A level's arrow (Java's drawWindArrow()), centred on the dot in an icon kArrowIconSide pixels
/// across, drawn at the size the icon is painted at.
class ArrowIconEngine final : public QIconEngine
{
public:
    ArrowIconEngine(double direction, QRgb color) : m_direction(direction), m_color(color) { }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode /*mode*/,
               QIcon::State /*state*/) override
    {
        const double  scale = rect.width() / WindProfilePlot::kArrowIconSide;
        const QPointF dot   = QRectF(rect).center();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QColor::fromRgba(m_color), scale));
        painter->drawLine(WindProfilePlot::arrowLine(m_direction, dot, scale));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor::fromRgba(m_color));
        painter->drawPolygon(WindProfilePlot::arrowHead(m_direction, dot, scale));
        painter->restore();
    }

    [[nodiscard]] QIconEngine* clone() const override
    {
        return makeQtOwned<ArrowIconEngine>(m_direction, m_color);
    }

    [[nodiscard]] QString key() const override { return QStringLiteral("QtRocketWindArrow"); }

private:
    double m_direction;
    QRgb   m_color;
};

/// The icon of a level's tool tip: it draws nothing, but a plot shows the tool tip of the place
/// an icon takes.
class BlankIconEngine final : public QIconEngine
{
public:
    void paint(QPainter* /*painter*/, const QRect& /*rect*/, QIcon::Mode /*mode*/,
               QIcon::State /*state*/) override
    {
    }

    [[nodiscard]] QIconEngine* clone() const override { return makeQtOwned<BlankIconEngine>(); }

    [[nodiscard]] QString key() const override { return QStringLiteral("QtRocketBlank"); }
};

/// The name of the altitude by @p reference (MultiLevelWindTable.col.Altitude*, which the tool tip
/// uses).
[[nodiscard]] QString altitudeName(WindModel::AltitudeReference reference)
{
    if (reference == WindModel::AltitudeReference::AGL)
    {
        //: MultiLevelWindTable.col.AltitudeAGL
        return WindProfilePlot::tr("Altitude AGL");
    }
    //: MultiLevelWindTable.col.AltitudeMSL
    return WindProfilePlot::tr("Altitude MSL");
}

/// "<name>: <value with its unit>".
[[nodiscard]] QString toolTipLine(const QString& name, const Unit& unit, double value)
{
    return name + QStringLiteral(": ") + QtStrings::unitTextWithUnit(unit, value);
}

void checkUnits(const WindProfilePlot::Units& units)
{
    if (units.altitude == nullptr || units.speed == nullptr || units.direction == nullptr ||
        units.deviation == nullptr)
    {
        bug("The wind profile needs every unit");
    }
}

}  // namespace

WindProfilePlot::WindProfilePlot(MultiLevelPinkNoiseWindModel& model, QWidget* parent)
  : QWidget(parent),
    m_model(&model),
    m_plot(makeChild<rocketplot::PlotWidget>(this)),
    //: WindProfilePanel.checkbox.ShowDirections
    m_showDirections(makeChild<QCheckBox>(this, tr("Show wind direction vectors"))),
    m_units{
        .altitude  = &unitGroup(UnitGroupId::DISTANCE).getDefaultUnit(),
        .speed     = &unitGroup(UnitGroupId::WINDSPEED).getDefaultUnit(),
        .direction = &unitGroup(UnitGroupId::ANGLE).getDefaultUnit(),
        .deviation = &unitGroup(UnitGroupId::WINDSPEED).getDefaultUnit(),
    }
{
    setObjectName(QStringLiteral("visualization"));
    m_plot->setObjectName(QStringLiteral("windProfileVisualization"));
    m_showDirections->setObjectName(QStringLiteral("showDirectionsCheckBox"));
    m_showDirections->setChecked(true);

    // Java: the visualization in the centre, below it a panel with 5 pixels of padding holding the
    // check box at its left.
    auto* const layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_plot, 1);
    auto* const   controls       = makeChild<QWidget>(this);
    auto* const   controlsLayout = makeChild<QHBoxLayout>(controls);
    constexpr int kPadding       = 5;
    controlsLayout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    controlsLayout->addWidget(m_showDirections);
    controlsLayout->addStretch(1);
    layout->addWidget(controls);

    PlotInputBindings::setNoInteraction(*m_plot);
    m_plot->legend()->setVisible(false);
    connect(m_showDirections, &QCheckBox::toggled, this,
            guarded([this] { refresh(); }, QStringLiteral("Show wind direction vectors")));
    m_connection = model.changed().connect([this] { refresh(); });
    refresh();
}

WindProfilePlot::~WindProfilePlot() = default;

void WindProfilePlot::setUnits(const Units& units)
{
    checkUnits(units);
    m_units = units;
    refresh();
}

void WindProfilePlot::setSelectedLevel(std::weak_ptr<const Level> level)
{
    m_selected = std::move(level);
    refresh();
}

bool WindProfilePlot::hasModel() const noexcept
{
    return m_connection.connected();
}

void WindProfilePlot::refresh()
{
    if (!hasModel())
    {
        return;
    }
    const std::vector<const Level*> levels = sortedLevels();
    m_plot->clearAnnotations();
    m_plot->clearSeries();
    updateAxes(levels);
    if (!levels.empty())
    {
        addLevels(levels);
    }
}

std::vector<const WindProfilePlot::Level*> WindProfilePlot::sortedLevels() const
{
    std::vector<const Level*> levels = std::as_const(*m_model).getLevels();
    std::ranges::stable_sort(levels, [](const Level* a, const Level* b) {
        return MathUtil::javaDoubleCompare(a->getAltitude(), b->getAltitude()) < 0;
    });
    return levels;
}

void WindProfilePlot::updateAxes(const std::vector<const Level*>& levels)
{
    double lowestAltitude  = 0;
    double highestAltitude = 0;
    double highestSpeed    = 0;
    for (const Level* const level : levels)
    {
        lowestAltitude  = std::min(lowestAltitude, level->getAltitude());
        highestAltitude = std::max(highestAltitude, level->getAltitude());
        highestSpeed    = std::max(highestSpeed, level->getSpeed());
    }
    const auto [speedMin, speedMax] = axisRange(0, highestSpeed, *m_units.speed);
    m_plot->xAxis()->setRange(speedMin, speedMax);
    m_plot->xAxis()->setLabel(rocketplot::literalText(speedAxisLabel(*m_units.speed)));
    const auto [altitudeMin, altitudeMax] =
        axisRange(lowestAltitude, highestAltitude, *m_units.altitude);
    m_plot->yAxis()->setRange(altitudeMin, altitudeMax);
    m_plot->yAxis()->setLabel(rocketplot::literalText(
        altitudeAxisLabel(*m_units.altitude, m_model->getAltitudeReference())));
}

void WindProfilePlot::addLevels(const std::vector<const Level*>& levels)
{
    const Level* const  selected = m_selected.lock().get();
    std::vector<double> speeds;
    std::vector<double> altitudes;
    std::vector<QRgb>   colors;
    std::vector<double> sizes;
    for (const Level* const level : levels)
    {
        speeds.push_back(m_units.speed->toUnit(level->getSpeed()));
        altitudes.push_back(m_units.altitude->toUnit(level->getAltitude()));
        colors.push_back(level == selected ? kSelectedLevelColor : kLevelColor);
        sizes.push_back(level == selected ? kSelectedDotSize : kDotSize);
    }
    rocketplot::LineSeries* const line = m_plot->addLine(speeds, altitudes);
    line->setColor(QColor::fromRgba(kLineColor));
    line->setLineWidth(1);
    line->setMarker(rocketplot::Marker::NONE);
    rocketplot::ScatterSeries* const dots = m_plot->addScatter(speeds, altitudes);
    dots->setMarker(rocketplot::Marker::CIRCLE);
    dots->setMarkerRingWidth(0);
    dots->setColor(QColor::fromRgba(kLevelColor));
    dots->setColors(colors);
    dots->setSizes(sizes);
    const auto selectedAt = std::ranges::find(levels, selected);
    if (selected != nullptr && selectedAt != levels.end())
    {
        // Java: drawOval in black around the selected dot.
        const auto at = static_cast<std::size_t>(std::ranges::distance(levels.begin(), selectedAt));
        rocketplot::ScatterSeries* const ring = m_plot->addScatter(
            std::vector<double>{speeds.at(at)}, std::vector<double>{altitudes.at(at)});
        ring->setMarker(rocketplot::Marker::CIRCLE);
        ring->setMarkerFilled(false);
        ring->setMarkerSize(kSelectedDotSize);
        ring->setMarkerRingWidth(0);
        ring->setColor(QColor::fromRgba(kRingColor));
    }
    for (std::size_t i = 0; i < levels.size() && m_showDirections->isChecked(); ++i)
    {
        const Level* const                level = levels.at(i);
        rocketplot::IconAnnotation* const arrow =
            m_plot->addIcon(speeds.at(i), altitudes.at(i),
                            QIcon(makeQtOwned<ArrowIconEngine>(
                                level->getDirection(),
                                level == selected ? kSelectedDirectionColor : kDirectionColor)));
        arrow->setSize(QSizeF(kArrowIconSide, kArrowIconSide));
    }
    // The tool tips last, over the arrows: the plot shows the tool tip of the icon on top.
    const WindModel::AltitudeReference reference = m_model->getAltitudeReference();
    for (std::size_t i = 0; i < levels.size(); ++i)
    {
        const Level* const level = levels.at(i);
        // Java: within twice the dot's radius.
        const double side = 2 * (level == selected ? kSelectedDotSize : kDotSize);
        rocketplot::IconAnnotation* const tip =
            m_plot->addIcon(speeds.at(i), altitudes.at(i), QIcon(makeQtOwned<BlankIconEngine>()));
        tip->setSize(QSizeF(side, side));
        tip->setToolTip(toolTipText(*level, m_units, reference));
    }
}

QString WindProfilePlot::toolTipText(const Level& level, const Units& units,
                                     WindModel::AltitudeReference reference)
{
    checkUnits(units);
    return QStringLiteral("<html>") +
           toolTipLine(altitudeName(reference), *units.altitude, level.getAltitude()) +
           QStringLiteral("<br>") +
           //: MultiLevelWindTable.col.Speed
           toolTipLine(tr("Speed"), *units.speed, level.getSpeed()) + QStringLiteral("<br>") +
           //: MultiLevelWindTable.col.Direction
           toolTipLine(tr("Direction"), *units.direction, level.getDirection()) +
           QStringLiteral("<br>") +
           //: MultiLevelWindTable.col.StandardDeviation
           toolTipLine(tr("Deviation"), *units.deviation, level.getStandardDeviation()) +
           QStringLiteral("</html>");
}

QString WindProfilePlot::speedAxisLabel(const Unit& unit)
{
    //: WindProfilePanel.lbl.WindSpeed
    return tr("Wind speed") + QStringLiteral(" (") + QtStrings::unitSymbol(unit) +
           QStringLiteral(")");
}

QString WindProfilePlot::altitudeAxisLabel(const Unit& unit, WindModel::AltitudeReference reference)
{
    QString name;
    if (reference == WindModel::AltitudeReference::AGL)
    {
        //: WindProfilePanel.lbl.AltitudeAGL
        name = tr("Altitude AGL");
    }
    else
    {
        //: WindProfilePanel.lbl.AltitudeMSL
        name = tr("Altitude MSL");
    }
    return name + QStringLiteral(" (") + QtStrings::unitSymbol(unit) + QStringLiteral(")");
}

std::pair<double, double> WindProfilePlot::axisRange(double lowest, double highest,
                                                     const Unit& unit)
{
    const double low  = lowest < 0 ? unit.toUnit(kRangeExtension * lowest) : 0.0;
    const double high = highest > 0 ? unit.toUnit(kRangeExtension * highest) : 0.0;
    if (!(high > low))
    {
        return {0.0, 1.0};
    }
    return {low, high};
}

QLineF WindProfilePlot::arrowLine(double direction, QPointF dot, double scale)
{
    // Java: from (x, y) to (x - 15 sin d, y + 15 cos d), y downward: the way the wind blows.
    const QPointF downwind(-std::sin(direction), std::cos(direction));
    return {dot, dot + (downwind * (kArrowLength * scale))};
}

QPolygonF WindProfilePlot::arrowHead(double direction, QPointF dot, double scale)
{
    // Java: drawFilledArrowHead(tip, arrowSize, direction + pi/2), the tip arrowSize - 1 beyond the
    // line's end, the sides at pi/6 either way from the arrow.
    const QPointF downwind(-std::sin(direction), std::cos(direction));
    const QPointF tip   = dot + (downwind * (kArrowReach * scale));
    const double  angle = direction + (std::numbers::pi / 2);
    const double  side  = kArrowHeadSize * scale;
    const QPointF left(std::cos(angle - (std::numbers::pi / 6)),
                       std::sin(angle - (std::numbers::pi / 6)));
    const QPointF right(std::cos(angle + (std::numbers::pi / 6)),
                        std::sin(angle + (std::numbers::pi / 6)));
    return QPolygonF({tip, tip - (left * side), tip - (right * side)});
}

}  // namespace QtRocket::Gui
