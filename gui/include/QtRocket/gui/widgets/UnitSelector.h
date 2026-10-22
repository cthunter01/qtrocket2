#pragma once

#include <memory>

#include <QObject>
#include <QToolButton>

#include "QtRocket/util/Signal.h"

class QMenu;
class QMouseEvent;
class QWidget;

namespace QtRocket
{
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;

/// The unit of a number field, and the choice of another (OpenRocket's UnitSelector on a
/// DoubleModel): a flat button showing the symbol of its DoubleBinding's current unit. A press of
/// any mouse button (Java: any click on its label) opens a menu of the binding's unit group, in
/// the group's order; the unit chosen becomes the binding's current unit, so that the spin box,
/// the slider and the selector of that binding show it; the group's default unit stays, and the
/// choice is not remembered (the next dialog starts at the default, which only the preferences
/// change). A group of one unit opens no menu, and the button does not react to the mouse
/// (Java's label shows no hover border then). The button takes no focus, as Java's label.
///
/// Enabled: the binding's isEnabled() and setFieldEnabled() together, as UnitSpinBox (the
/// follower of an "override" check box disables the selector as well).
///
/// Deviations from OpenRocket: the menu opens on the press (Qt's menus), where Java's opens on
/// the click; a disabled selector opens none. Not ported: the selector of a bare unit group and
/// the one showing the value (Java's UnitSelector(UnitGroup) and showValue: the plots, Milestone
/// 4, and the component analysis, Milestone 7), and the extra actions no caller passes.
class UnitSelector : public QToolButton
{
    Q_OBJECT

public:
    /// A selector of the current unit of @p binding.
    /// @throws BugError when @p binding is null
    UnitSelector(std::shared_ptr<DoubleBinding> binding, QWidget* parent);
    ~UnitSelector() override;
    UnitSelector(const UnitSelector&)            = delete;
    UnitSelector& operator=(const UnitSelector&) = delete;
    UnitSelector(UnitSelector&&)                 = delete;
    UnitSelector& operator=(UnitSelector&&)      = delete;

    [[nodiscard]] const std::shared_ptr<DoubleBinding>& binding() const noexcept
    {
        return m_binding;
    }

    /// Shows and sets the unit of @p binding from now on (Java: setModel()); an open menu closes.
    /// @throws BugError when @p binding is null
    void setBinding(std::shared_ptr<DoubleBinding> binding);

    /// The menu of the units, while or after it was opened: it is made again each time.
    [[nodiscard]] QMenu* unitMenu() const noexcept { return m_menu; }

    /// Opens the menu of the units below the button (what a press does).
    void showUnitMenu();

    /// Whether the field may be edited as far as the dialog is concerned (see UnitSpinBox).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void connectBinding();
    /// Shows the binding's current unit and enabled state.
    void refresh();
    /// Whether the binding's group has more than one unit to choose from.
    [[nodiscard]] bool hasChoice() const;
    /// The menu's choice @p unit, a unit of the binding's group when the menu was made.
    void chooseUnit(const Unit& unit);

    std::shared_ptr<DoubleBinding> m_binding;
    Signal<>::ScopedConnection     m_connection;
    QMenu*                         m_menu{nullptr};
    bool                           m_fieldEnabled{true};
};

}  // namespace QtRocket::Gui
