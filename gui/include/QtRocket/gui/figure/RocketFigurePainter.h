#pragma once

#include <optional>
#include <span>

#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QTransform>
#include <QtTypes>

#include "QtRocket/util/LineStyle.h"

class QPainter;

namespace QtRocket
{
class ComponentShape;
class FigureShape;
class Preferences;
class RocketFigureGeometry;
class Uuid;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// Paints the rocket of a RocketFigureGeometry with any QPainter (the drawing half of OpenRocket's
/// RocketFigure.paintComponent() and drawMotors()): the design view's RocketFigure, and later the
/// design's preview image (Milestone 5) and the printed figure (Milestone 8).
///
/// - The shapes in the geometry's order (the highest display order first, so the lowest ends up
///   on top), each outlined, never filled: in the component's colour, else the preferences'
///   default colour of its class (Preferences::getDefaultColor()); in the component's line style,
///   else the preferences' default of its class (Preferences::getDefaultLineStyle(): mass objects
///   dashed, the others solid). The pen is cosmetic, 1 pixel wide, 2 for a selected component,
///   with flat caps and bevelled joins; a style's dashes are in pixels (dashed 6 on 4 off, dotted 2
///   on 3 off, dash-dotted 8, 3, 2, 3), whatever the width. The shapes only hit tests see, the
///   shapes of hidden components (RocketComponent::isVisible()) and those in a colour of alpha 0
///   are not drawn.
/// - Then each motor filled in OR.colors.motor.fill and outlined in OR.colors.motor.border
///   (1 pixel), the colours of the palette's theme (ThemeColors).
///
/// The shapes are in metres, y up; paint() maps them with the projection and the painter's world
/// transform into the painter's device itself, and draws them with an identity world transform (a
/// cosmetic pen's dashes are in device pixels either way). A shape with a coordinate that is not
/// finite, or that maps beyond kMaxDeviceCoordinate pixels, is not drawn (a NaN or infinite size
/// typed into a component, an absurd zoom): no such coordinate reaches Qt.
///
/// Deviations from OpenRocket: the default line style comes from the preferences only. Java asks
/// its UI theme first (OR.defaults.lineStyle.generic SOLID and .massObject DASHED in every theme),
/// so the default a user saves in the appearance panel (Milestone 5) never shows in Java's figure;
/// here it does. Java draws every shape, also those of the colour ORColor.INVISIBLE (alpha 0),
/// which leaves no mark; here they are skipped, as are the shapes without extent (an empty path, or
/// one whose points are all the same, such as the tip of a nose cone in the back view: QPainterPath
/// drops such segments, and Java's stroke of them leaves no mark). The rendering hints of Java's
/// selected (STROKE_PURE) and normal (STROKE_NORMALIZE) strokes have no Qt counterpart; both are
/// drawn antialiased, as in Java.
class RocketFigurePainter
{
public:
    /// RocketFigure.NORMAL_WIDTH and SELECTED_WIDTH: the pen widths in pixels.
    static constexpr double kNormalWidth   = 1.0;
    static constexpr double kSelectedWidth = 2.0;
    /// The largest pixel coordinate handed to Qt (see the class comment).
    static constexpr double kMaxDeviceCoordinate = 1.0e9;

    /// A painter with the default colours and line styles of @p preferences (which must outlive
    /// it) and the motor colours of @p palette's theme.
    RocketFigurePainter(const Preferences& preferences, QPalette palette);

    /// Paints the shapes and the motors of @p geometry with @p painter, mapped by @p projection
    /// (metres to the painter's coordinates); the components whose id is in @p selection are drawn
    /// with the selected width. The painter's state is restored afterwards.
    void paint(QPainter& painter, const RocketFigureGeometry& geometry,
               const QTransform& projection, std::span<const Uuid> selection) const;

    /// The colour @p shape is drawn in: its own (the component's when it was made), else the
    /// preferences' default colour of the component's class.
    [[nodiscard]] QColor shapeColor(const ComponentShape& shape) const;
    /// The line style @p shape is drawn in: its own, else the preferences' default of its class.
    [[nodiscard]] LineStyle shapeLineStyle(const ComponentShape& shape) const;
    /// OR.colors.motor.fill and OR.colors.motor.border of the palette's theme.
    [[nodiscard]] QColor motorFillColor() const;
    [[nodiscard]] QColor motorBorderColor() const;

    /// The pen of a component's outline: cosmetic, kNormalWidth or kSelectedWidth wide, flat caps,
    /// bevelled joins, solid or dashed (dashPattern()).
    [[nodiscard]] static QPen componentPen(const QColor& color, LineStyle style, bool selected);
    /// The pen of a motor's outline: cosmetic, kNormalWidth, solid, flat caps, bevelled joins.
    [[nodiscard]] static QPen motorPen(const QColor& color);
    /// The dash pattern of @p style for a pen @p width pixels wide (Qt measures dashes in pen
    /// widths): Java's dashes in pixels divided by the width; empty for SOLID (a solid line).
    [[nodiscard]] static QList<qreal> dashPattern(LineStyle style, double width);

    /// @p shape (metres) mapped by @p toDevice, with its winding rule (NON_ZERO is
    /// Qt::WindingFill): its segments one for one. nullopt when a coordinate, before or after the
    /// mapping, is not finite or lands beyond kMaxDeviceCoordinate.
    [[nodiscard]] static std::optional<QPainterPath> devicePath(const FigureShape& shape,
                                                                const QTransform&  toDevice);
    /// Whether every coordinate of @p path is finite and within kMaxDeviceCoordinate.
    [[nodiscard]] static bool isDrawable(const QPainterPath& path) noexcept;

private:
    const Preferences* m_preferences;
    QPalette           m_palette;
};

}  // namespace QtRocket::Gui
