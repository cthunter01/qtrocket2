#pragma once

#include <memory>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

/// The common part of the value bindings (DoubleBinding, and the others of binding/): the model
/// between the widgets of a field and a property of the selected components (Java: DoubleModel,
/// IntegerModel, ... with ModelInvalidator). A binding caches what it shows, re-reads it once per
/// event of the rocket (ComponentSelection's Phase::READ: Qt asks a spin box for its state on
/// every paint, and a getter can be expensive, the override CD's a whole force analysis), and
/// tells its widgets through its change signal (Phase::NOTIFY) only when that changed. It also
/// tells them when the selection becomes invalid, after which it shows nothing and accepts
/// nothing.
///
/// A binding without a selection holds a value of its own (Java's DoubleModel(value), used as
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

    /// True for a binding of its own value; otherwise whether its selection is valid.
    [[nodiscard]] bool isValid() const noexcept;

    /// The selection the binding edits; null for a binding of its own value.
    [[nodiscard]] const std::shared_ptr<ComponentSelection>& getSelection() const noexcept
    {
        return m_selection;
    }

    /// Adds a listener told whenever what the binding shows changed, and once when its selection
    /// becomes invalid (Java: addChangeListener()).
    Signal<>::Connection addChangeListener(Signal<>::Slot slot);

    /// Whether the binding is telling its listeners right now (see the class comment).
    [[nodiscard]] bool isFiring() const noexcept { return m_firing > 0; }

protected:
    /// A binding of the components of @p selection, or of its own value when it is null. The
    /// subclass reads its state at the end of its constructor (readState()).
    explicit Binding(std::shared_ptr<ComponentSelection> selection);

    /// Re-reads the cached state from the selection (only called while it is valid); true when
    /// anything the widgets show changed.
    virtual bool readState() = 0;

    /// Tells the listeners, with the firing guard up.
    void fireChanged();

private:
    void selectionChanged(ComponentSelection::Phase phase);

    std::shared_ptr<ComponentSelection>               m_selection;
    ComponentSelection::PhaseSignal::ScopedConnection m_selectionConnection;
    Signal<>                                          m_changed;
    int                                               m_firing{0};
    /// Set by Phase::READ when the state changed, cleared by Phase::NOTIFY.
    bool m_changePending{false};
};

}  // namespace QtRocket::Gui
