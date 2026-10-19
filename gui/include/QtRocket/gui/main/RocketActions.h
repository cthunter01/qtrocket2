#pragma once

#include <optional>
#include <span>

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/gui/main/ComponentEditing.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
// RocketComponent is complete here, not declared: moc's metatype of a signal's
// RocketComponent* checks for completeness, and GCC's -Wsfinae-incomplete rejects a class that
// another moc file of the same compilation sees complete later.
#include "QtRocket/rocket/RocketComponent.h"

class QPoint;

namespace QtRocket
{
class FlightConfiguration;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DocumentSelectionModel;
class MainMenus;
class OpenRocketClipboard;
class UserPrompts;

/// The component actions of a design window (OpenRocket's RocketActions, its component half; the
/// simulations' come with Milestone 4): Edit, Cut, Copy, Paste, Duplicate, Delete, Deselect all,
/// Move up, Move down, Hide/Show selected, Show all and the stages' Disable/Enable, with their
/// texts, keys, icons and tooltips, the rules that enable them, and the component tree's popup
/// menu made of them.
///
/// The Edit menu's actions are the window's MainMenus' (Edit, Cut, Copy, Paste, Duplicate,
/// Delete, Deselect all, Hide selected, Show all), which made them with OpenRocket's texts, keys
/// and icons: this object connects them and sets their enabled states (and the changing text,
/// tooltip and icon of Hide/Show selected). Its own actions are those the Edit menu does not
/// have: Move up and Move down (the buttons beside the tree, DesignPanel), the popup's Hide/Show
/// (ContextMenuToggleVisibilityAction) and the stages' Disable/Enable (ToggleActiveAction).
///
/// Every action is re-evaluated (updateActions(), Java's updateActions()) when the selection
/// changes (DocumentSelectionModel::changed()), when the clipboard changes and at every change
/// event of the rocket (RocketActions.java:116-176). A triggered action works on the selection
/// as it is then; the editing actions are ComponentEditing's (Cut, Copy, Paste, Duplicate,
/// Delete, Move up, Move down), which this object holds.
///
/// The configuration dialog is the window's (configdialog/): this object asks for it by signal
/// (decision O12), editRequested() for Edit, and closeConfigDialogRequested() first where Java's
/// action disposes the dialog (ComponentConfigDialog.disposeDialog()): Edit, Cut, Paste,
/// Duplicate, Delete, Move up and Move down. Copy, Deselect all, the visibility actions and the
/// stage action leave it open, as in Java.
///
/// - Edit (EditAction): enabled when a component is selected; asks for the dialog of the first
///   selected component with the others (editRequested(first, true)).
/// - Deselect all (DeselectAllAction): enabled when a component is selected; selects nothing.
/// - Hide/Show selected (ToggleVisibilityAction), enabled when a component is selected: "Hide
///   all" / "Show all" by the Rocket's visibility when the Rocket or every other component is
///   selected, else "Hide selected" when a selected component is visible, "Show selected"
///   otherwise, with the tooltip and the icon (hidden / showing) that go with the text. With the
///   Rocket selected it toggles the Rocket and every component; otherwise it shows the selected
///   components when none of them is visible and hides them otherwise: each one that is not a
///   stage, after which its stage is visible exactly when a component below it is, and so is
///   the Rocket; then each selected stage (a booster set is a stage) with everything below it.
///   With nothing selected the text stays as it was (Java).
/// - The popup's Hide/Show (ContextMenuToggleVisibilityAction): the same, with the texts "Hide"
///   and "Show" by whether a selected component is visible (its tooltip and icon are the menu
///   action's).
/// - Show all (ShowAllComponentsAction): enabled when a component below the Rocket is hidden;
///   shows the Rocket and every component.
/// - Disable/Enable (ToggleActiveAction), for the selected stages (a booster set is one) in the
///   rocket's selected flight configuration: enabled when a stage is selected; "Disable" when a
///   selected stage is active or has no children, disabled then with the tooltip "Cannot disable
///   a stage with no components." for a stage without children and "Cannot disable the only
///   active stage." when no active stage would be left; else "Enable". Triggered, it makes
///   every selected stage active when none is, else inactive (FlightConfiguration::toggleStage()
///   where a stage differs, which sets the stages below it too), then fires one event
///   AEROMASS_CHANGE | MOTOR_CHANGE | TREE_CHANGE_CHILDREN for the configuration, as Java's
///   action and StageSelector do. No undo position (decision O17, as in Java). A selection of
///   stages the force analysis cannot do (an active booster set below two inactive stages) is
///   the user's to make: whoever shows a force analysis then shows its Error.
/// - The popup menu (contextMenu(), BasicFrame's popupMenu): Edit, Cut, Copy, Paste, Duplicate,
///   Delete | Select > Deselect all | Hide/Show, Disable/Enable; Disable/Enable is shown only
///   while every selected component is a stage (popupMenuWillBecomeVisible()).
///
/// No component pointer is kept: the selection is read when it is needed.
///
/// It belongs to the selection, its parent (makeChild<RocketActions>(selection, menus,
/// clipboard, prompts)), so that it never outlives it; the menus' actions are watched
/// (QPointer), the popup menu belongs to the selection's view.
///
/// Deviations from OpenRocket:
/// - The items of later milestones are left out of the popup menu, with the separator that would
///   stand alone: Select > Components of same color (Milestone 5: no default appearances yet),
///   Scale (Milestone 5), Export as OBJ (not in QtRocket) and Export as SVG (Milestone 8). The
///   Edit menu leaves out Components of same color too (MainMenus).
/// - Hide/Show and Show all freeze the rocket while they change the components' visibility, so
///   that the rocket fires one change event (GRAPHIC_CHANGE) instead of one for each component
///   it shows or hides (and two more, the stage's and the Rocket's, for each component): the
///   window repaints once, and no listener can change the tree between two changes. The result
///   is the same.
/// - The popup's Disable/Enable item is taken out of the menu and put back
///   (prepareContextMenu()), where Java hides the JMenuItem: a hidden QAction would be disabled
///   too (Qt 6), and the action's state is its rule's.
/// - The actions do not switch to the design tab (BasicFrame.selectTab(DESIGN_TAB)): Milestone
///   2 has no other tab.
/// - An action whose rule says no does nothing (Java's actions cannot be triggered then, and
///   QActions cannot either).
class RocketActions final : public QObject
{
    Q_OBJECT

public:
    /// The actions of the components @p selection (not null) selects, its child, driving the
    /// Edit menu's actions of @p menus (a design window's), with the editing of the selection
    /// through @p clipboard and @p prompts (whose messages have the selection's view as their
    /// parent). @p clipboard and @p prompts must outlive this object; the menus' actions may go
    /// first (then they are left alone).
    /// @throws BugError when @p selection is null or @p menus are not a design window's.
    RocketActions(MainMenus& menus, OpenRocketClipboard& clipboard, UserPrompts& prompts,
                  DocumentSelectionModel* selection);
    ~RocketActions() override;

    RocketActions(const RocketActions&)            = delete;
    RocketActions& operator=(const RocketActions&) = delete;
    RocketActions(RocketActions&&)                 = delete;
    RocketActions& operator=(RocketActions&&)      = delete;

    [[nodiscard]] DocumentSelectionModel& selection() const noexcept { return *m_selection; }
    /// The editing of the selection (Cut, Copy, Paste, Duplicate, Delete, Move up, Move down).
    [[nodiscard]] ComponentEditing& editing() noexcept { return m_editing; }

    // ------------------------------------------------------------------- the actions

    /// Edit (getEditAction()), the Edit menu's.
    [[nodiscard]] QAction* editAction() const noexcept { return m_edit; }
    [[nodiscard]] QAction* cutAction() const noexcept { return m_cut; }
    [[nodiscard]] QAction* copyAction() const noexcept { return m_copy; }
    [[nodiscard]] QAction* pasteAction() const noexcept { return m_paste; }
    [[nodiscard]] QAction* duplicateAction() const noexcept { return m_duplicate; }
    [[nodiscard]] QAction* deleteAction() const noexcept { return m_delete; }
    [[nodiscard]] QAction* deselectAllAction() const noexcept { return m_deselectAll; }
    /// Move up ("moveUpAction"), this object's.
    [[nodiscard]] QAction* moveUpAction() const noexcept { return m_moveUp; }
    /// Move down ("moveDownAction"), this object's.
    [[nodiscard]] QAction* moveDownAction() const noexcept { return m_moveDown; }
    /// Hide/Show selected, the Edit menu's Visibility item.
    [[nodiscard]] QAction* toggleVisibilityAction() const noexcept { return m_toggleVisibility; }
    /// The popup's Hide/Show ("toggleVisibilityContextMenuAction"), this object's.
    [[nodiscard]] QAction* toggleVisibilityContextMenuAction() const noexcept
    {
        return m_contextToggleVisibility;
    }
    /// Show all, the Edit menu's Visibility item.
    [[nodiscard]] QAction* showAllComponentsAction() const noexcept { return m_showAll; }
    /// Disable/Enable ("toggleActiveAction"), this object's.
    [[nodiscard]] QAction* toggleActiveAction() const noexcept { return m_toggleActive; }

    // ------------------------------------------------------------------- the popup menu

    /// The component tree's popup menu ("componentTreePopupMenu").
    [[nodiscard]] QMenu* contextMenu() const noexcept { return m_contextMenu; }

    /// Shows the popup menu at @p globalPosition (BasicFrame.doComponentTreePopup()): connect
    /// ComponentTreeView::contextMenuRequested() to it.
    void showContextMenu(const QPoint& globalPosition);

    /// Makes the popup menu's items what the selection asks for (popupMenuWillBecomeVisible()):
    /// Disable/Enable, its last item, is there only while every selected component is a stage.
    /// The menu does it each time before it shows.
    void prepareContextMenu();

    // ------------------------------------------------------------------- the rules

    /// Re-evaluates every action for the selection, the clipboard and the rocket as they are now
    /// (updateActions()).
    void updateActions();

    /// Whether @p selection counts as the whole rocket @p rocket for Hide/Show
    /// (ToggleVisibilityAction.isRocketSelected()): the Rocket is among it, or it has as many
    /// components as there are below the Rocket.
    [[nodiscard]] static bool isRocketSelected(const Rocket&                           rocket,
                                               std::span<const RocketComponent* const> selection);

    /// Why the stages among @p selection cannot be disabled in @p configuration of @p rocket
    /// (getCannotDisableReason()): "Cannot disable a stage with no components." when one has no
    /// children, "Cannot disable the only active stage." when no stage of the rocket (a booster
    /// set counts) would stay active; nullopt when they can.
    [[nodiscard]] static std::optional<QString> cannotDisableReason(
        const Rocket& rocket, const FlightConfiguration& configuration,
        std::span<const RocketComponent* const> selection);

Q_SIGNALS:
    /// Edit was triggered: open the configuration dialog of @p component (never null), the
    /// first selected one, for the whole selection (@p withSelection is always true here; the
    /// signal is the one of ComponentTreeView::editRequested(), so that one slot serves both).
    /// The pointer is valid during the emission only: connect directly.
    void editRequested(QtRocket::RocketComponent* component, bool withSelection);

    /// An action is about to change the design and the configuration dialog is to close first
    /// (ComponentConfigDialog.disposeDialog()): connect directly.
    void closeConfigDialogRequested();

private:
    /// Makes the actions this object owns.
    void createOwnActions();
    /// Connects the actions to what they do.
    void connectActions();
    /// Makes the popup menu.
    void createContextMenu();

    /// Edit (EditAction.editComponents()).
    void edit();
    /// Deselect all.
    void deselectAll();
    /// Runs @p operation of the editing (one of Cut, Paste, Duplicate, Delete, Move up, Move
    /// down) when @p allowed, after asking for the configuration dialog to close.
    void runClosingDialog(bool allowed, bool (ComponentEditing::*operation)());
    /// Hide/Show (ToggleVisibilityAction.actionPerformed()).
    void toggleVisibility();
    /// Show all.
    void showAll();
    /// Disable/Enable (ToggleActiveAction.actionPerformed()).
    void toggleActive();

    /// The text, tooltip, icon and enabled state of Hide/Show selected and the popup's Hide/Show.
    void updateVisibilityActions();
    /// The text, tooltip, icon and enabled state of Disable/Enable.
    void updateToggleActiveAction();
    /// The rocket's change event.
    void rocketChanged(const ComponentChangeEvent& event);

    DocumentSelectionModel*                 m_selection;
    ComponentEditing                        m_editing;
    QPointer<QAction>                       m_edit;
    QPointer<QAction>                       m_cut;
    QPointer<QAction>                       m_copy;
    QPointer<QAction>                       m_paste;
    QPointer<QAction>                       m_duplicate;
    QPointer<QAction>                       m_delete;
    QPointer<QAction>                       m_deselectAll;
    QPointer<QAction>                       m_toggleVisibility;
    QPointer<QAction>                       m_showAll;
    QAction*                                m_moveUp{nullptr};
    QAction*                                m_moveDown{nullptr};
    QAction*                                m_contextToggleVisibility{nullptr};
    QAction*                                m_toggleActive{nullptr};
    QPointer<QMenu>                         m_contextMenu;
    ComponentChangeSignal::ScopedConnection m_rocketChanges;
};

}  // namespace QtRocket::Gui
