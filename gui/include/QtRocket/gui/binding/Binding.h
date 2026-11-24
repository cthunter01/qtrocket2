#pragma once

#include <memory>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

/// The common part of the value bindings (DoubleBinding, and the others of binding/): the model
/// between the widgets of a field and a property of the selected components or of another object
/// (Java: DoubleModel, IntegerModel, ... with ModelInvalidator). The binding is made over a
/// context (BindingContext): a ComponentSelection, whose main component its source reads and whose
/// selected components it writes, or an ObjectSelection, whose one object it reads and writes (the
/// simulation options, a wind model or level, the preferences). A binding caches what it shows,
/// re-reads it once per change the context is told of (its Phase::READ: an event of the rocket,
/// an emission of the object's signal; Qt asks a spin box for its state on every paint, and a
/// getter can be expensive, the override CD's a whole force analysis), and tells its widgets
/// through its change signal (Phase::NOTIFY) only when that changed. It also tells them when the
/// context becomes invalid through a change (Phase::INVALIDATED), after which it shows nothing and
/// accepts nothing. "The selection" in the bindings' comments is the context, whichever it is.
///
/// A binding without a context holds a value of its own (Java's DoubleModel(value), used as
/// a constant bound): it is always valid.
///
/// The firing guard: while the binding notifies its widgets (isFiring()), the writes that come
/// from widgets (a spin box's commit, a slider's move, a check box's toggle) are dropped, so that
/// a widget refreshing itself cannot write back what it was just told (Java: the counter
/// `firing` of DoubleModel and its spinner, slider and action models).
///
/// Not copyable or movable (the slots refer to it); GUI thread only. A dynamic bound of another
/// binding holds it by std::shared_ptr, so bindings are usually made with std::make_shared; one so
/// made keeps itself alive while it tells its listeners, so that a listener may drop its last
/// owner (a dialog closing).
class Binding : public std::enable_shared_from_this<Binding>
{
public:
    virtual ~Binding();
    Binding(const Binding&)            = delete;
    Binding& operator=(const Binding&) = delete;
    Binding(Binding&&)                 = delete;
    Binding& operator=(Binding&&)      = delete;

    /// True for a binding of its own value; otherwise whether its context is valid.
    [[nodiscard]] bool isValid() const noexcept;

    /// The context the binding edits (a ComponentSelection or an ObjectSelection); null for a
    /// binding of its own value.
    [[nodiscard]] const std::shared_ptr<BindingContext>& getContext() const noexcept
    {
        return m_context;
    }

    /// The component selection the binding edits; null for a binding of an object
    /// (ObjectSelection) and for a binding of its own value.
    [[nodiscard]] const std::shared_ptr<ComponentSelection>& getSelection() const noexcept
    {
        return m_selection;
    }

    /// Adds a listener told whenever what the binding shows changed, and once when its context
    /// becomes invalid through a change (Java: addChangeListener()).
    Signal<>::Connection addChangeListener(Signal<>::Slot slot);

    /// Whether the binding is telling its listeners right now (see the class comment).
    [[nodiscard]] bool isFiring() const noexcept { return m_firing > 0; }

protected:
    /// A binding over @p context (a ComponentSelection or an ObjectSelection), or of its own value
    /// when it is null. The subclass reads its state at the end of its constructor (readState()).
    explicit Binding(std::shared_ptr<BindingContext> context);

    /// Re-reads the cached state from the context (only called while it is valid); true when
    /// anything the widgets show changed.
    virtual bool readState() = 0;

    /// Tells the listeners, with the firing guard up.
    void fireChanged();

    /// Whether a write that comes from a widget may reach the components or the object: not while
    /// the binding tells its listeners (the firing guard) and not once the context is invalid.
    [[nodiscard]] bool isWidgetWriteAllowed() const noexcept { return !isFiring() && isValid(); }

private:
    void contextChanged(BindingContext::Phase phase);

    std::shared_ptr<BindingContext> m_context;
    /// m_context when it is a ComponentSelection, else null.
    std::shared_ptr<ComponentSelection>           m_selection;
    BindingContext::PhaseSignal::ScopedConnection m_contextConnection;
    Signal<>                                      m_changed;
    int                                           m_firing{0};
    /// Set by Phase::READ when the state changed, cleared by Phase::NOTIFY.
    bool m_changePending{false};
};

}  // namespace QtRocket::Gui
