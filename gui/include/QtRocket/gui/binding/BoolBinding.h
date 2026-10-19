#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

/// The model of a check box (Java: BooleanModel, swing gui/adaptors/BooleanModel.java, an Action
/// for a JCheckBox): "Override mass", "Clipped", "This component is a motor mount".
///
/// The value is the source's value of the main component (Java's getter is<Name> or get<Name>;
/// here the source's get). setValue() sets it on every selected component
/// (ComponentSelection::apply()). The binding caches the value and the enabled state, re-reads
/// them once per event of the rocket and tells its listeners when they changed. The enabled
/// state is the source's isEnabled (Java: the optional is<Name>Enabled(), which only
/// Transition::isClippedEnabled() provides among the dialogs: "Clipped" is enabled for the shapes
/// that can be clipped).
///
/// The check box writes with setChecked() (Java: actionPerformed()): dropped while the binding
/// notifies (a check box refreshing itself) and once the selection is invalid, and nothing when
/// it is the value shown already.
///
/// Enable followers (Java: addEnableComponent(component, enableState)): whatever the value
/// enables, the spin box, unit selector and slider of an override, the "Override subcomponents"
/// check box. A follower is a function told whether it is enabled: exactly when the value equals
/// its enable state. It is told when it is added and whenever the value changes (Java:
/// updateEnableStatus()), and false once the selection is invalid. isFollowerEnabled() gives the
/// same answer as data. A follower is removed by disconnecting the connection that
/// addEnableFollower() returns (Java: removeEnableComponent()); it may be a widget of a dialog
/// that goes before the binding, so hold the connection in a ScopedConnection beside the widget.
///
/// A binding without a selection holds a value of its own (Java's BooleanModel(boolean)): it is
/// always valid and enabled; setValue() and setChecked() change that value.
///
/// Deviations from OpenRocket:
/// - Java's BooleanModel tells its listeners of every event of the source; here only of a change
///   of the value or the enabled state.
/// - A follower is told it is disabled once the selection is invalid (Java leaves the components
///   alone; its dialog closes).
/// - A source without a setter is a BugError (Java refuses a source without set<Name>), as is a
///   source whose getChecked gives an Error when it does (only a DoubleBinding shows one).
class BoolBinding final : public Binding
{
public:
    /// The signal of the enable followers: the value shown, nullopt once the selection is
    /// invalid.
    using FollowerSignal = Signal<std::optional<bool>>;

    /// A binding of @p source on the components of @p selection.
    /// @throws BugError when @p selection is null, the source has no setter, or the source cannot
    ///         read the main component
    BoolBinding(std::shared_ptr<ComponentSelection> selection, BoolSource source);

    /// A binding of its own value @p value (Java: BooleanModel(initialValue)).
    explicit BoolBinding(bool value);

    ~BoolBinding() override;
    BoolBinding(const BoolBinding&)            = delete;
    BoolBinding& operator=(const BoolBinding&) = delete;
    BoolBinding(BoolBinding&&)                 = delete;
    BoolBinding& operator=(BoolBinding&&)      = delete;

    /// The cached value of the main component, or the binding's own value (Java: getValue());
    /// once the selection is invalid, the last value read.
    [[nodiscard]] bool value() const noexcept { return m_value; }

    /// Sets @p value on every selected component, also when it is the value shown already (Java:
    /// setValue(), whose setter forwards it to the others), or as the binding's own value.
    /// Nothing happens once the selection is invalid. Not guarded by isFiring(), as in Java.
    void setValue(bool value);

    /// Whether the check box can be used: the source's isEnabled of the main component (true
    /// without one, and for a binding of its own value); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    /// The check box's write (Java: actionPerformed()): sets @p checked unless it is value()
    /// already; false, with nothing set, then, while firing and once the selection is invalid.
    bool setChecked(bool checked);

    /// Adds an enable follower @p setEnabled, enabled when the value equals @p enableState (see
    /// the class comment), and tells it at once whether it is enabled.
    FollowerSignal::Connection addEnableFollower(std::function<void(bool enabled)> setEnabled,
                                                 bool enableState = true);

    /// Whether a follower of @p enableState is enabled now: the value equals it and the selection
    /// is valid.
    [[nodiscard]] bool isFollowerEnabled(bool enableState) const noexcept
    {
        return isValid() && m_value == enableState;
    }

protected:
    bool readState() override;

private:
    [[nodiscard]] std::optional<bool> followerState() const noexcept;
    void                              updateFollowers();

    std::optional<BoolSource>  m_source;
    bool                       m_value{false};
    bool                       m_enabled{true};
    FollowerSignal             m_followers;
    std::optional<bool>        m_followerState;
    Signal<>::ScopedConnection m_followerConnection;
};

}  // namespace QtRocket::Gui
