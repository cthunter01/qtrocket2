#include "QtRocket/gui/figure/RocketFigurePainter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include <QColor>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QTransform>
#include <Qt>
#include <QtTypes>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether @p value is a coordinate Qt may be given.
[[nodiscard]] bool isDrawableCoordinate(double value) noexcept
{
    return std::isfinite(value) && std::abs(value) <= RocketFigurePainter::kMaxDeviceCoordinate;
}

/// @p point (metres) mapped by @p toDevice, or nullopt when it is not drawable.
[[nodiscard]] std::optional<QPointF> devicePoint(const Point2D& point, const QTransform& toDevice)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
    {
        return std::nullopt;
    }
    const QPointF mapped = toDevice.map(QPointF(point.x, point.y));
    if (!isDrawableCoordinate(mapped.x()) || !isDrawableCoordinate(mapped.y()))
    {
        return std::nullopt;
    }
    return mapped;
}

/// Adds @p segment to @p path, its points mapped by @p toDevice; false when a point is not
/// drawable.
[[nodiscard]] bool addSegment(QPainterPath& path, const PathSegment& segment,
                              const QTransform& toDevice)
{
    std::array<QPointF, 3> points{};
    for (int index = 0; index < segment.pointCount(); ++index)
    {
        const auto at     = static_cast<std::size_t>(index);
        const auto mapped = devicePoint(segment.points.at(at), toDevice);
        if (!mapped)
        {
            return false;
        }
        points.at(at) = *mapped;
    }
    switch (segment.type)
    {
        case PathSegment::Type::MOVE_TO:
            path.moveTo(points[0]);
            break;
        case PathSegment::Type::LINE_TO:
            path.lineTo(points[0]);
            break;
        case PathSegment::Type::QUAD_TO:
            path.quadTo(points[0], points[1]);
            break;
        case PathSegment::Type::CUBIC_TO:
            path.cubicTo(points[0], points[1], points[2]);
            break;
        case PathSegment::Type::CLOSE:
            path.closeSubpath();
            break;
    }
    return true;
}

/// Whether the component with the id @p id is in @p selection.
[[nodiscard]] bool isSelected(const Uuid& id, std::span<const Uuid> selection) noexcept
{
    return std::ranges::find(selection, id) != selection.end();
}

}  // namespace

RocketFigurePainter::RocketFigurePainter(const Preferences& preferences, QPalette palette)
  : m_preferences(&preferences), m_palette(std::move(palette))
{
}

void RocketFigurePainter::paint(QPainter& painter, const RocketFigureGeometry& geometry,
                                const QTransform& projection, std::span<const Uuid> selection) const
{
    const QTransform toDevice = projection * painter.worldTransform();

    painter.save();
    painter.setWorldTransform(QTransform{});
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw all shapes
    painter.setBrush(Qt::NoBrush);
    for (const RocketFigureGeometry::InstanceShape& instanceShape : geometry.shapes())
    {
        const ComponentShape&  shape     = instanceShape.shape;
        const RocketComponent& component = shape.component();
        if (!shape.isVisible() || !component.isVisible())
        {
            continue;
        }
        const QColor color = shapeColor(shape);
        if (color.alpha() == 0)
        {
            continue;
        }
        const std::optional<QPainterPath> path = devicePath(shape.shape(), toDevice);
        if (!path || path->isEmpty())
        {
            continue;
        }
        painter.setPen(
            componentPen(color, shapeLineStyle(shape), isSelected(component.getId(), selection)));
        painter.drawPath(*path);
    }

    // Draw motors
    const QColor fillColor = motorFillColor();
    const QPen   border    = motorPen(motorBorderColor());
    for (const RocketFigureGeometry::MotorShape& motor : geometry.motors())
    {
        const std::optional<QPainterPath> path = devicePath(motor.shape, toDevice);
        if (!path || path->isEmpty())
        {
            continue;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawPath(*path);
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(*path);
    }

    painter.restore();
}

QColor RocketFigurePainter::shapeColor(const ComponentShape& shape) const
{
    if (const std::optional<Color>& color = shape.color(); color.has_value())
    {
        return ColorConversion::toQColor(*color);
    }
    return ColorConversion::toQColor(
        m_preferences->getDefaultColor(componentClassChain(shape.component().kind())));
}

LineStyle RocketFigurePainter::shapeLineStyle(const ComponentShape& shape) const
{
    if (const std::optional<LineStyle> style = shape.lineStyle(); style.has_value())
    {
        return *style;
    }
    return m_preferences->getDefaultLineStyle(componentClassChain(shape.component().kind()));
}

QColor RocketFigurePainter::motorFillColor() const
{
    return ThemeColors::color(ThemeColors::Key::MOTOR_FILL, m_palette);
}

QColor RocketFigurePainter::motorBorderColor() const
{
    return ThemeColors::color(ThemeColors::Key::MOTOR_BORDER, m_palette);
}

QPen RocketFigurePainter::componentPen(const QColor& color, LineStyle style, bool selected)
{
    const double width = selected ? kSelectedWidth : kNormalWidth;
    QPen         pen(color, width, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
    pen.setCosmetic(true);
    if (const QList<qreal> pattern = dashPattern(style, width); !pattern.isEmpty())
    {
        pen.setDashPattern(pattern);
    }
    return pen;
}

QPen RocketFigurePainter::motorPen(const QColor& color)
{
    QPen pen(color, kNormalWidth, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
    pen.setCosmetic(true);
    return pen;
}

QList<qreal> RocketFigurePainter::dashPattern(LineStyle style, double width)
{
    QList<qreal> pattern;
    if (style == LineStyle::SOLID)
    {
        return pattern;
    }
    for (const double dash : dashes(style))
    {
        pattern.append(dash / width);
    }
    return pattern;
}

std::optional<QPainterPath> RocketFigurePainter::devicePath(const FigureShape& shape,
                                                            const QTransform&  toDevice)
{
    QPainterPath path;
    path.setFillRule(shape.windingRule() == WindingRule::NON_ZERO ? Qt::WindingFill
                                                                  : Qt::OddEvenFill);
    for (const PathSegment& segment : shape.segments())
    {
        if (!addSegment(path, segment, toDevice))
        {
            return std::nullopt;
        }
    }
    return path;
}

bool RocketFigurePainter::isDrawable(const QPainterPath& path) noexcept
{
    for (int index = 0; index < path.elementCount(); ++index)
    {
        const QPainterPath::Element element = path.elementAt(index);
        if (!isDrawableCoordinate(element.x) || !isDrawableCoordinate(element.y))
        {
            return false;
        }
    }
    return true;
}

}  // namespace QtRocket::Gui
