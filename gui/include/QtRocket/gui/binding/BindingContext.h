#pragma once

#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

/// What the value bindings (Binding and its subclasses) read and write through, and when they
/// re-read it: the components of a configuration dialog (ComponentSelection) or one object that
/// is no component but has a change signal (ObjectSelection: the simulation options, a wind
/// model, a wind level, the preferences). OpenRocket's DoubleModel and its kin take any object
/// and find its methods by reflection; they listen to it when it is a ChangeSource and are
/// invalidated with their dialog (ModelInvalidator). Here a binding's PropertySource says how a
/// property is read and written (PropertySource::of() for a component class,
/// PropertySource::ofObject() for any other), and the context the binding is made over says what
/// it reads from and when it changed.
///
/// A context refreshes its bindings in two phases on every change of what it holds: Phase::READ
/// (each binding re-reads what it shows, so that a binding whose bound is another binding sees
/// that one's new value), then Phase::NOTIFY (each binding tells its widgets what changed). It
/// sends Phase::INVALIDATED once when it ends through a change it is told of (a TREE or UNDO
/// change of the components; ObjectSelection::invalidate()). Once it is invalid, every accessor
/// of what it holds is a BugError and its bindings drop every write a widget makes.
///
/// An interface: not copyable or movable (the bindings' slots refer to the context); GUI thread
/// only.
class BindingContext
{
public:
    /// What a listener is told (see the class comment).
    enum class Phase
    {
        /// Re-read the cached state from what the context holds; tell nobody yet.
        READ,
        /// Every binding has read: tell the widgets what changed.
        NOTIFY,
        /// What the context held is gone, or going: show nothing.
        INVALIDATED,
    };

    using PhaseSignal = Signal<Phase>;

    virtual ~BindingContext();
    BindingContext(const BindingContext&)            = delete;
    BindingContext& operator=(const BindingContext&) = delete;
    BindingContext(BindingContext&&)                 = delete;
    BindingContext& operator=(BindingContext&&)      = delete;

    /// Whether what the context holds may still be read and written.
    [[nodiscard]] virtual bool isValid() const noexcept = 0;

    /// Refreshes the bindings (Phase::READ, then Phase::NOTIFY), as a change of what the context
    /// holds does: for a change that announces nothing. Nothing happens once the context is
    /// invalid.
    virtual void refresh() = 0;

    /// Adds a listener of the phases (the bindings).
    virtual PhaseSignal::Connection addListener(PhaseSignal::Slot slot) = 0;

    /// Ends the context on purpose, before what it holds can go (a dialog that closes): it lets
    /// go of it but tells its listeners nothing, so that nothing is refreshed while the dialog
    /// goes. Its bindings are invalid from then on. Nothing happens when it is invalid already.
    virtual void release() noexcept = 0;

protected:
    BindingContext() = default;
};

}  // namespace QtRocket::Gui
