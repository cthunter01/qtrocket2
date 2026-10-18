#pragma once

#include <optional>
#include <vector>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket
{

class RocketComponent;

/// One shape a component contributes to the 2D figure, with what the painter and the hit test
/// need to know about it (OpenRocket's RocketComponentShapes): the shape, the component it
/// belongs to, and the component's colour and line style at the time the shape was made (unset
/// when the component has none, so that the painter uses the preferences' defaults for its
/// class).
///
/// An invisible shape is one OpenRocket gives the colour ORColor.INVISIBLE: it is never drawn,
/// but a hit test finds it, so that a click between the visible lines of a rail button or on the
/// square around a pod set's marker selects the component. Its color() is Color::invisible(), as
/// in Java.
///
/// The component is an observer: a shape is valid only as long as the component, so a holder
/// rebuilds its shapes after any TREE or UNDO event (Rocket::loadFrom replaces every component).
class ComponentShape
{
public:
    /// A visible shape of @p component, with the component's colour and line style (Java: new
    /// RocketComponentShapes(shape, component)).
    ComponentShape(FigureShape shape, const RocketComponent& component);

    /// A shape of @p component that only hit tests see (Java: a RocketComponentShapes whose
    /// colour is set to ORColor.INVISIBLE).
    [[nodiscard]] static ComponentShape invisible(FigureShape            shape,
                                                  const RocketComponent& component);

    /// Visible shapes of @p component, one per shape, in order (Java:
    /// RocketComponentShapes.toArray(shapes, component)).
    [[nodiscard]] static std::vector<ComponentShape> fromShapes(std::vector<FigureShape> shapes,
                                                                const RocketComponent&   component);

    [[nodiscard]] const FigureShape& shape() const noexcept { return m_shape; }

    /// The component the shape belongs to.
    [[nodiscard]] const RocketComponent& component() const noexcept { return *m_component; }

    /// False for a shape only hit tests see. This is the shape's own flag, independent of the
    /// component's RocketComponent::isVisible(): as in Java, a hidden component's shapes are
    /// still made and found by hit tests, and only the painter skips them.
    [[nodiscard]] bool isVisible() const noexcept { return m_visible; }

    /// The colour to draw with: the component's when the shape was made (unset when it has
    /// none), Color::invisible() for an invisible shape.
    [[nodiscard]] const std::optional<Color>& color() const noexcept { return m_color; }

    /// The component's line style when the shape was made (unset when it has none).
    [[nodiscard]] std::optional<LineStyle> lineStyle() const noexcept { return m_lineStyle; }

private:
    FigureShape              m_shape;
    const RocketComponent*   m_component;
    std::optional<Color>     m_color;
    std::optional<LineStyle> m_lineStyle;
    bool                     m_visible{true};
};

}  // namespace QtRocket
