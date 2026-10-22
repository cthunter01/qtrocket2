#pragma once

#include <memory>
#include <vector>

#include <QCheckBox>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/util/Signal.h"

class QWidget;

namespace QtRocket::Gui
{

class Binding;
class DoubleBinding;

/// The check box of a property (OpenRocket: a JCheckBox on a BooleanModel, its Action: "Override
/// mass", "Clipped", "This component is a motor mount") or of a number field's automatic state
/// (a JCheckBox on DoubleModel.getAutomaticAction(): the "Automatic" of a radius, a packed
/// length, a line length).
///
/// It shows its binding's value (BoolBinding::value(), DoubleBinding::isAutomatic()) and writes a
/// click of the user's (QAbstractButton::clicked: the mouse, the space key, click()) through the
/// binding (BoolBinding::setChecked(), DoubleBinding::setAutomatic(); Java: actionPerformed() and
/// AutomaticActionModel.putValue()); after the write, whatever came of it, it shows the binding's
/// value again (a write the binding drops, or a setter that refuses the value, fires nothing). It
/// also shows it whenever the binding tells of a change.
///
/// Enabled: what the binding allows AND setFieldEnabled() (the dialog's part, FieldEnabling). The
/// binding allows a property's check box while its source's isEnabled says so
/// (BoolBinding::isEnabled(): Transition::isClippedEnabled()), an automatic check box while the
/// number field has an automatic state and can be edited (Java's AutomaticActionModel.isEnabled():
/// isAutomaticAvailable()); neither once the binding's selection is invalid.
///
/// Enable followers (Java: BooleanModel.addEnableComponent() and removeEnableComponent()): the
/// widgets a property's check box enables, the spin box, unit selector and slider of an override,
/// the "Override for all subcomponents" check box. A follower is enabled exactly while the value
/// equals its enable state (FieldEnabling::setFieldEnabled(), so that its own binding keeps its
/// say), told at once and on every change of the value, and disabled once the selection is
/// invalid (BoolBinding::addEnableFollower()). A follower that is destroyed is removed; one that
/// is removed keeps the state it has.
///
/// Deviations from OpenRocket: the automatic check box is also disabled while the number field
/// cannot be edited (its source's isEnabled; Java's DoubleModel has no such state), and with the
/// binding's selection invalid (Java's dialog closes).
class BoolCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    /// A check box of @p binding showing @p text.
    /// @throws BugError when @p binding is null
    BoolCheckBox(std::shared_ptr<BoolBinding> binding, const QString& text, QWidget* parent);

    /// The automatic check box of @p numberBinding showing @p text.
    /// @throws BugError when @p numberBinding is null
    BoolCheckBox(std::shared_ptr<DoubleBinding> numberBinding, const QString& text,
                 QWidget* parent);

    ~BoolCheckBox() override;
    BoolCheckBox(const BoolCheckBox&)            = delete;
    BoolCheckBox& operator=(const BoolCheckBox&) = delete;
    BoolCheckBox(BoolCheckBox&&)                 = delete;
    BoolCheckBox& operator=(BoolCheckBox&&)      = delete;

    /// The binding of a property's check box; null for an automatic check box.
    [[nodiscard]] const std::shared_ptr<BoolBinding>& binding() const noexcept { return m_binding; }

    /// The number field's binding of an automatic check box; null for a property's check box.
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& numberBinding() const noexcept
    {
        return m_numberBinding;
    }

    /// Whether the check box may be used as far as the dialog is concerned (see the class
    /// comment).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

    /// Makes @p follower enabled exactly while the value is @p enableState (see the class
    /// comment), and sets it so at once; a follower added again takes the new enable state.
    /// @throws BugError for an automatic check box (Java's automatic action has no followers)
    void addEnableFollower(QWidget& follower, bool enableState = true);

    /// Stops @p follower following the value; it keeps the state it has. Nothing when it is no
    /// follower.
    void removeEnableFollower(const QWidget& follower);

    /// Whether @p widget follows the value.
    [[nodiscard]] bool hasEnableFollower(const QWidget& widget) const;

private:
    struct Follower
    {
        QWidget*                                      widget{nullptr};
        BoolBinding::FollowerSignal::ScopedConnection connection;
        QMetaObject::Connection                       destroyed;
    };

    /// The common part of the constructors: listens to @p binding and shows its state.
    void setUp(const std::shared_ptr<Binding>& binding);
    void refresh();
    /// A click of the user's, to the binding.
    void               clickedByUser(bool checked);
    [[nodiscard]] bool bindingValue() const;
    [[nodiscard]] bool bindingEnabled() const;
    void               forgetFollower(const QObject* follower);

    std::shared_ptr<BoolBinding>   m_binding;
    std::shared_ptr<DoubleBinding> m_numberBinding;
    Signal<>::ScopedConnection     m_connection;
    std::vector<Follower>          m_followers;
    bool                           m_fieldEnabled{true};
};

}  // namespace QtRocket::Gui
