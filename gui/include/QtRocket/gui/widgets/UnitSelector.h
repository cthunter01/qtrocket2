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
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;

/// The unit of a number field, and the choice of another (OpenRocket's UnitSelector, swing
/// gui/components/): a flat button showing the symbol of the selected unit (getSelectedUnit()). A
/// press of any mouse button (Java: any click on its label) opens a menu of the unit group's units,
/// in the group's order, and the unit chosen becomes the selected one (setSelectedUnit()). A group
/// of one unit opens no menu, and the button does not react to the mouse (Java's label shows no
/// hover border then). The button takes no focus, as Java's label.
///
/// It is made in one of two modes, as Java's:
/// - over a DoubleBinding (Java: UnitSelector(DoubleModel)): the selected unit is the binding's
///   current unit, so that the spin box, the slider and the selector of that binding show it; the
///   group's default unit stays, and the choice is not remembered (the next dialog starts at the
///   default, which only the preferences change);
/// - over a bare unit group (Java: UnitSelector(UnitGroup), the plots' and the CSV export's): the
///   selector keeps the selected unit itself, the group's default unit to begin with and after
///   setUnitGroup() gives it another group; the group must outlive the selector (the core's unit
///   groups and a flight data type's live as long as the process).
/// Every setSelectedUnit(), the menu's included, tells of it with unitSelected() (Java's
/// ItemEvent SELECTED, fired also when the unit is the one selected already); setUnitGroup()
/// tells of nothing (Java fires no event there).
///
/// Enabled: setFieldEnabled() and, over a binding, the binding's isEnabled() together, as
/// UnitSpinBox (the follower of an "override" check box disables the selector as well).
///
/// Deviations from OpenRocket: the menu opens on the press (Qt's menus), where Java's opens on
/// the click; a disabled selector opens none. A unit chosen in the menu is taken only while it is
/// one of the group's (the menu may outlive a change of the group). Over a binding, the selector
/// follows a unit the binding gets from elsewhere (Java's keeps the unit it had unless it shows the
/// value). Not ported: the selector showing the value (Java's showValue: the component analysis,
/// Milestone 7) and the extra actions no caller passes.
class UnitSelector : public QToolButton
{
    Q_OBJECT

public:
    /// A selector of the current unit of @p binding.
    /// @throws BugError when @p binding is null
    UnitSelector(std::shared_ptr<DoubleBinding> binding, QWidget* parent);
    /// A selector of a unit of @p group, its default unit selected (Java: UnitSelector(UnitGroup)).
    UnitSelector(const UnitGroup& group, QWidget* parent);
    ~UnitSelector() override;
    UnitSelector(const UnitSelector&)            = delete;
    UnitSelector& operator=(const UnitSelector&) = delete;
    UnitSelector(UnitSelector&&)                 = delete;
    UnitSelector& operator=(UnitSelector&&)      = delete;

    /// The binding, or null over a bare unit group.
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& binding() const noexcept
    {
        return m_binding;
    }

    /// Shows and sets the unit of @p binding from now on (Java: setModel()), also when the selector
    /// was made over a bare unit group; an open menu closes.
    /// @throws BugError when @p binding is null
    void setBinding(std::shared_ptr<DoubleBinding> binding);

    /// The unit group: the binding's, or the bare one (Java: getUnitGroup()).
    [[nodiscard]] const UnitGroup& getUnitGroup() const noexcept;
    /// Gives a selector over a bare unit group the group @p group, its default unit selected
    /// (Java: setUnitGroup()); nothing when it is the group the selector has (the same object).
    /// An open menu closes.
    /// @throws BugError for a selector over a binding (Java: IllegalStateException)
    void setUnitGroup(const UnitGroup& group);

    /// The selected unit: the binding's current unit, or the bare group's selected one (Java:
    /// getSelectedUnit()).
    [[nodiscard]] const Unit& getSelectedUnit() const noexcept;
    /// Selects @p unit (over a binding: its current unit) and tells of it with unitSelected(), also
    /// when it is the selected one already (Java: setSelectedUnit()).
    /// @throws BugError when @p unit is not one of the group's (Java: IllegalArgumentException)
    void setSelectedUnit(const Unit& unit);

    /// The menu of the units, while or after it was opened: it is made again each time.
    [[nodiscard]] QMenu* unitMenu() const noexcept { return m_menu; }

    /// Opens the menu of the units below the button (what a press does).
    void showUnitMenu();

    /// Whether the field may be edited as far as the dialog is concerned (see UnitSpinBox).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

Q_SIGNALS:
    /// setSelectedUnit() selected a unit (see the class comment).
    void unitSelected();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    /// The button's look and manners, the same in both modes.
    void setUp();
    void connectBinding();
    /// Shows the selected unit and the enabled state.
    void refresh();
    /// Whether the group has more than one unit to choose from.
    [[nodiscard]] bool hasChoice() const;
    /// The menu's choice @p unit, a unit of the group when the menu was made.
    void chooseUnit(const Unit& unit);

    std::shared_ptr<DoubleBinding> m_binding;
    Signal<>::ScopedConnection     m_connection;
    /// The bare unit group and its selected unit; null over a binding.
    const UnitGroup* m_group{nullptr};
    const Unit*      m_unit{nullptr};
    QMenu*           m_menu{nullptr};
    bool             m_fieldEnabled{true};
};

}  // namespace QtRocket::Gui
