#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The components a configuration dialog edits: the main component, whose values the fields
/// show, the other selected components, which every edit reaches too, and their Rocket. It
/// replaces OpenRocket's "config listeners" (RocketComponent.addConfigListener() and the
/// forwarding loop at the top of every setter, 172 of them in 32 files), with which a dialog of
/// one component edits several at once.
///
/// Edits (the user's decision U3 of 2026-10-17): apply<C>(f) calls f on every selected component
/// that is a C, each its own call, the others first (in the order given) and the main component
/// last, as Java's setters forward to their listeners before they set themselves. The calls run
/// inside one freeze of the rocket (Rocket::freeze()), taken only when the rocket is not frozen
/// already (a second freeze is a BugError) and thawed also when f throws, so that one edit of
/// many components fires one merged event when it thaws. It never adds an undo position: the
/// dialog adds one per session, before the first edit, so that the edit of every component is
/// one undo step.
///
/// Deviations from OpenRocket (U3), documented here where the binding layer makes them:
/// - Java's setters forward the main component's argument but compute the values they derive
///   from it with the MAIN component's other values, and those derived values reach the others
///   through the forwarding of the setters they call. Here every component derives them from its
///   own values. The twelve setters whose results therefore differ:
///   1. BodyTube.setInnerRadius (setThickness(main's outer radius - r));
///   2. LaunchLug.setInnerRadius (setOuterRadius(r + main's thickness));
///   3. TubeFinSet.setInnerRadius (setThickness(main's outer radius - r));
///   4. ThicknessRingComponent.setInnerRadius (setThickness(main's outer radius - r));
///   5. RadiusRingComponent.setThickness (setInnerRadius(main's outer radius - t));
///   6. MassComponent.setDensity (setComponentMass(density * main's volume): every mass
///      component gets the main's mass);
///   7. TrapezoidFinSet.setSweepAngle (setSweep(tan(angle) * main's height): 16.1 degrees on a
///      fin set twice as high, for 30 typed);
///   8. RailButton.setOuterDiameter (setInnerDiameter(main's inner diameter));
///   9. Transition.setForeRadius and setAftRadius (the main's fore or aft shoulder radius);
///   10. Transition.setForeShoulderLength and setAftShoulderLength (from a length of 0 on the
///       main: the main's automatic shoulder radius and wall thickness);
///   11. NoseCone.setFlipped (the main's radii and shoulders);
///   12. ParallelStage.setRadiusMethod and PodSet.setRadiusMethod (setRadius(method, main's
///       radius)).
/// - Java drops the others' events and fires only the main component's, none at all when the
///   main's value does not change (the others then change without an event). Here every
///   component fires its own, and the freeze merges them into one event at the thaw, which also
///   fires when only the others changed.
/// - Java's clearPreset() is forwarded too, so an edit clears the preset of a component whose
///   value did not change; here a component keeps its preset unless its own setter clears it.
/// - Invalid text in a spinner sets nothing (DoubleBinding::commitText()); Java sets the main's
///   last value again, which its forwarding gives every other component.
/// - Dialog rules that Java decides by the main component alone (the automatic-radius check
///   boxes that switch "automatic" off) are the dialogs' business: each component by its own
///   neighbours.
/// Setters that freeze the rocket themselves (RocketComponent::loadPreset(), splitInstances(true),
/// FreeformFinSet::convertFinSet(.., true)) must never run inside apply(): the second freeze is a
/// BugError. Structural operations of several components call their non-freezing forms inside
/// one freeze of their own, without apply().
///
/// Lifetime: the selection listens to the rocket. On an event of the rocket that is a TREE or
/// UNDO change it becomes invalid: Rocket::loadFrom() (undo, redo) replaces every component object,
/// and a component taken out of the tree is destroyed once its event is delivered, so the
/// selection drops its pointers inside that event and never touches a component again. It is
/// invalid as well once the rocket has gone or its listeners were reset (Rocket::resetListeners()),
/// and once release() ended it (a closing dialog).
/// After that apply() and every accessor of the components are BugErrors; isValid() says which.
/// On every other event it refreshes its bindings in two phases: Phase::READ (each binding
/// re-reads what it shows, so that a binding whose bound is another binding sees that one's new
/// value), then Phase::NOTIFY (each binding tells its widgets what changed). Phase::INVALIDATED
/// is sent once, when it becomes invalid through an event.
///
/// Make it with std::make_shared: the bindings share it (Binding), and a listener may then drop
/// the last owner while it is being told (a dialog that closes on Phase::INVALIDATED): the
/// selection keeps itself alive until it has told every listener. Not copyable or movable (the
/// rocket's slot refers to it); GUI thread only, like the rocket.
class ComponentSelection : public std::enable_shared_from_this<ComponentSelection>
{
public:
    /// What a listener is told (see the class comment).
    enum class Phase
    {
        /// Re-read the cached state from the components; tell nobody yet.
        READ,
        /// Every binding has read: tell the widgets what changed.
        NOTIFY,
        /// The components are gone (a TREE or UNDO change): show nothing.
        INVALIDATED,
    };

    using PhaseSignal = Signal<Phase>;

    /// The selection of @p main and the other selected components @p others. An entry of
    /// @p others that is @p main or repeats an earlier one is left out (Java's addConfigListener()
    /// refuses the same).
    /// @throws BugError when @p main is not in a Rocket, or an entry of @p others is null or not in
    ///         the same rocket
    explicit ComponentSelection(RocketComponent&                  main,
                                std::span<RocketComponent* const> others = {});

    ~ComponentSelection();
    ComponentSelection(const ComponentSelection&)            = delete;
    ComponentSelection& operator=(const ComponentSelection&) = delete;
    ComponentSelection(ComponentSelection&&)                 = delete;
    ComponentSelection& operator=(ComponentSelection&&)      = delete;

    /// False once a TREE or UNDO change has reached the selection, the rocket has gone or reset
    /// its listeners, or release() ended it.
    [[nodiscard]] bool isValid() const noexcept;

    /// The component the fields show (Java: the dialog's component).
    /// @throws BugError when the selection is invalid
    [[nodiscard]] RocketComponent& getMain() const;

    /// The other selected components, in the order given (Java: the config listeners).
    /// @throws BugError when the selection is invalid
    [[nodiscard]] std::span<RocketComponent* const> getOthers() const;

    /// The rocket of the components.
    /// @throws BugError when the selection is invalid
    [[nodiscard]] Rocket& getRocket() const;

    /// Whether other components are selected besides the main one.
    /// @throws BugError when the selection is invalid
    [[nodiscard]] bool isMultiEdit() const;

    /// Whether every selected component is of the main component's class (Java:
    /// checkAllClassesEqual(), which picks the dialog of that class; otherwise the generic one).
    /// @throws BugError when the selection is invalid
    [[nodiscard]] bool isAllOfOneKind() const;

    /// Calls @p f on every selected component that is a C (dynamic_cast), the others first and
    /// the main component last, inside one freeze of the rocket (see the class comment). When
    /// no event of the rocket reached the selection during the calls (the rocket was frozen
    /// already, its events are disabled, or nothing changed), the selection refreshes its
    /// bindings itself afterwards, so that they are current when apply() returns.
    /// @throws BugError when the selection is invalid; whatever @p f throws, after the thaw
    template <class C, class F>
    void apply(const F& f)
    {
        applyToEach([&f](RocketComponent& component) {
            if (auto* target = dynamic_cast<C*>(&component))
            {
                std::invoke(f, *target);
            }
        });
    }

    /// apply() without the cast: @p f is called on every selected component.
    /// @throws BugError when the selection is invalid; whatever @p f throws, after the thaw
    void applyToEach(const std::function<void(RocketComponent&)>& f);

    /// Refreshes the bindings (Phase::READ, then Phase::NOTIFY), as an event of the rocket does:
    /// for a change that fires no event (a preference a getter reads, say). Nothing happens once
    /// the selection is invalid.
    void refresh();

    /// Adds a listener of the phases (the bindings; see the class comment).
    PhaseSignal::Connection addListener(PhaseSignal::Slot slot);

    /// Ends the selection on purpose, before its components can go (a dialog that closes:
    /// widgets/BindingScope::release()): it lets go of the components and the rocket as a TREE or
    /// UNDO change does, but tells its listeners nothing, so that nothing is refreshed while the
    /// dialog goes. Its bindings are invalid from then on and drop every write a widget makes (the
    /// focus loss of a closing dialog's field, which then reaches no component). Nothing happens
    /// when the selection is invalid already.
    void release() noexcept;

private:
    void componentChanged(const ComponentChangeEvent& event);
    /// Lets go of the components, the rocket and the rocket's connection.
    void dropComponents() noexcept;
    void invalidate();
    void checkValid() const;

    RocketComponent*                        m_main;
    std::vector<RocketComponent*>           m_others;
    Rocket*                                 m_rocket;
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
    PhaseSignal                             m_listeners;
    /// The events of the rocket received so far (apply() refreshes when none arrived).
    std::uint64_t m_eventCount{0};
    bool          m_invalidated{false};
};

}  // namespace QtRocket::Gui
