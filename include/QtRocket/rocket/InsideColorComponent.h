#pragma once

#include "QtRocket/rocket/InsideColorComponentHandler.h"

namespace QtRocket
{

/// A component that can have a different inside and outside appearance (OpenRocket's
/// InsideColorComponent: body tubes, transitions, nose cones, launch lugs, tube fin sets). Java's
/// marker interface with a handler getter becomes a mixin that owns the handler: a concrete
/// component class derives from RocketComponent (through its layers) and from this class, e.g.
/// `class BodyTube : public SymmetricComponent, public InsideColorComponent`.
///
/// clang-tidy's misc-multiple-inheritance flags such a class, since the mixin is not a pure
/// interface: its declaration is preceded by
/// `// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data`.
///
/// Copying: the mixin's copy constructor gives the copy a handler of its own with the source's
/// state, so the implicit copy constructor a component's cloneShallow() uses needs no extra code
/// (Java's RocketComponent.clone() did this for every InsideColorComponent), and
/// RocketComponent::copyFrom() copies the state when both sides are InsideColorComponents.
class InsideColorComponent
{
public:
    virtual ~InsideColorComponent();

    InsideColorComponent& operator=(const InsideColorComponent&) = delete;
    InsideColorComponent(InsideColorComponent&&)                 = delete;
    InsideColorComponent& operator=(InsideColorComponent&&)      = delete;

    [[nodiscard]] InsideColorComponentHandler& getInsideColorComponentHandler() noexcept
    {
        return m_handler;
    }
    [[nodiscard]] const InsideColorComponentHandler& getInsideColorComponentHandler() const noexcept
    {
        return m_handler;
    }

    /// Copies the state of @p handler into this component's own handler, firing nothing (Java
    /// replaces the handler object; the one used here stays bound to this component).
    void setInsideColorComponentHandler(const InsideColorComponentHandler& handler);

protected:
    InsideColorComponent() noexcept;

    /// A new handler for this object with @p other's state.
    InsideColorComponent(const InsideColorComponent& other);

private:
    InsideColorComponentHandler m_handler;
};

}  // namespace QtRocket
