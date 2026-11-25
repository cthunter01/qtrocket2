#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <QAction>
#include <QMenu>
#include <QMetaObject>
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
class SimulationPanel;
class UserPrompts;

/// The actions of a design window on its components and simulations (OpenRocket's
/// RocketActions): Edit, Cut, Copy, Paste, Duplicate, Delete, Deselect all, Move up, Move down,
/// Hide/Show selected, Show all and the stages' Disable/Enable, with their texts, keys, icons and
/// tooltips, the rules that enable them, and the component tree's popup menu made of them.
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
/// event of the rocket (RocketActions.java:116-176); and Edit, Cut, Copy, Paste, Duplicate and
/// Delete when the simulations table's selection or one of the simulations tab's actions they
/// follow changes (see THE SIMULATIONS). A triggered action works on the selection as it is then;
/// the editing actions are ComponentEditing's (Cut, Copy, Paste, Duplicate, Delete, Move up, Move
/// down), which this object holds.
///
/// The configuration dialog is the window's (configdialog/): this object asks for it by signal
/// (decision O12), editRequested() for Edit, and closeConfigDialogRequested() first where Java's
/// action disposes the dialog (ComponentConfigDialog.disposeDialog()): Edit, Cut, Paste,
/// Duplicate, Delete, Move up and Move down. Copy, Deselect all, the visibility actions and the
/// stage action leave it open, as in Java.
///
/// The window's tabs are the window's too: after Cut, Copy, Paste, Duplicate and Delete have done
/// their work this object asks for the design tab (designTabRequested(): Java's
/// parentFrame.selectTab(BasicFrame.DESIGN_TAB)), so that an edit made from the Edit menu while
/// another tab is shown shows its result in the component tree. A Paste that fits nowhere asks for
/// it too (Java's pasteComponents() goes on to its end then, selecting nothing); an operation
/// whose rule says no, or that QtRocket's bounds refuse (ComponentEditing), asks for nothing, and
/// the other actions leave the tab as it is, as Java's.
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
/// THE SIMULATIONS (setSimulationPanel(): the window's "Flight simulations" tab, a
/// SimulationPanel; RocketActions.java:478-1000): while simulations are selected in its table
/// (Java's isSimulationSelected(); the component selection is empty then, as the window keeps
/// the two exclusive: a component selected too is a BugError, Java's IllegalStateException),
/// Edit, Cut, Copy, Paste, Duplicate and Delete do the tab's operation of the same name
/// (SimulationPanel::editSimulation(), cutSimulations(), ...: Java's actionPerformed() of the
/// tab's action) in place of the components', and then ask for the simulations tab
/// (simulationTabRequested(): selectTab(SIMULATION_TAB)); they close no configuration dialog.
/// Each of them is enabled when its component rule or the tab's action of the same name is
/// (Java's clipboardChanged()). The texts and tooltips stay the component ones (OpenRocket's: Cut,
/// Paste and Delete name "components or simulations"). Paste also goes to the simulations
/// without a selected one while the tab is the window's current one and its Paste is enabled
/// (see the deviations), and is enabled for that only then.
///
/// No component pointer is kept: the selection is read when it is needed; nor a simulation.
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
/// - An action whose rule says no does nothing (Java's actions cannot be triggered then, and
///   QActions cannot either).
/// - OpenRocket bug 27 (the Edit menu's half; the tab's is SimulationPanel's): Paste pastes the
///   simulations on the clipboard into the design also while none is selected, when the
///   simulations tab is the window's current one and its Paste is enabled (simulations on the
///   clipboard, and a configuration with motors: an empty list too), as the tab's own Paste does
///   (U-M4-13); Java's Paste reaches the simulations only through a selected one, so it pastes
///   nothing then (its tab's Paste is disabled without a selected row). On another tab, with no
///   simulation selected, Paste stays the components' and is enabled only by their rule, so that
///   it never shows enabled while it would paste nothing.
/// - The states of Edit, Cut, Copy, Paste, Duplicate and Delete follow the tab's actions'
///   (QAction::enabledChanged()) whenever those change, where Java's updateActions() calls the
///   tab's updateActions() first: the same states, in whatever order the rocket's listeners
///   come.
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

    /// The window's simulations tab, whose operations Edit, Cut, Copy, Paste, Duplicate and
    /// Delete do while simulations are selected (see THE SIMULATIONS), and what tells whether it
    /// is the window's current tab (@p isCurrentTab; an empty function says it never is). Whoever
    /// calls this calls updateActions() whenever what @p isCurrentTab tells changes (the
    /// WindowManager: at every change of the window's tab). Null (the default) for none: then
    /// every action is the components' alone. The panel may go first (then it is left alone).
    void setSimulationPanel(SimulationPanel* panel, std::function<bool()> isCurrentTab);
    /// The simulations tab set by setSimulationPanel(), or null.
    [[nodiscard]] SimulationPanel* simulationPanel() const noexcept;

    /// Whether simulations are selected in the simulations tab's table (Java's
    /// isSimulationSelected()): then Edit, Cut, Copy, Paste, Duplicate and Delete act on them.
    /// @throws BugError when components are selected too (Java's IllegalStateException "Both
    ///         simulation and component selected")
    [[nodiscard]] bool isSimulationSelected() const;
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

    /// Cut, Copy, Paste, Duplicate or Delete did its work: the window is to show its design tab
    /// (BasicFrame.selectTab(DESIGN_TAB)).
    void designTabRequested();

    /// Edit, Cut, Copy, Paste, Duplicate or Delete did the simulations tab's operation: the
    /// window is to show its simulations tab (BasicFrame.selectTab(SIMULATION_TAB)).
    void simulationTabRequested();

private:
    /// Makes the actions this object owns.
    void createOwnActions();
    /// Connects the actions to what they do.
    void connectActions();
    /// Makes the popup menu.
    void createContextMenu();

    /// Edit (EditAction.actionPerformed()).
    void edit();
    /// When simulations are selected (and for Paste, see the deviations: @p paste), does
    /// @p operation of the simulations tab, then asks for the tab; whether it did (Java's
    /// isSimulationSelected() branch of each action).
    bool runOnSimulations(const std::function<void(SimulationPanel& panel)>& operation,
                          bool                                               paste = false);
    /// Whether the simulations table has a selection (no check of the components: for the
    /// rules, which may run while the window makes the two selections exclusive again).
    [[nodiscard]] bool hasSimulationSelection() const;
    /// Whether Paste goes to the simulations without a selected one (see the deviations).
    [[nodiscard]] bool pastesIntoSimulationTab() const;
    /// Deselect all.
    void deselectAll();
    /// Runs @p operation of the editing (one of Cut, Paste, Duplicate, Delete, Move up, Move
    /// down) when @p allowed, after asking for the configuration dialog to close; whether it did
    /// its work (false when not allowed).
    bool runClosingDialog(bool allowed, bool (ComponentEditing::*operation)());
    /// Copy, then the design tab when it copied.
    void copy();
    /// Paste, then the design tab when it pasted or nothing fitted (see the class comment).
    void paste();
    /// Hide/Show (ToggleVisibilityAction.actionPerformed()).
    void toggleVisibility();
    /// Show all.
    void showAll();
    /// Disable/Enable (ToggleActiveAction.actionPerformed()).
    void toggleActive();

    /// The enabled states of Edit, Cut, Copy, Paste, Duplicate and Delete: the components' rule
    /// or the simulations tab's action.
    void updateEditActions();
    /// The text, tooltip, icon and enabled state of Hide/Show selected and the popup's Hide/Show.
    void updateVisibilityActions();
    /// The text, tooltip, icon and enabled state of Disable/Enable.
    void updateToggleActiveAction();
    /// The rocket's change event.
    void rocketChanged(const ComponentChangeEvent& event);

    DocumentSelectionModel*   m_selection;
    ComponentEditing          m_editing;
    QPointer<QAction>         m_edit;
    QPointer<QAction>         m_cut;
    QPointer<QAction>         m_copy;
    QPointer<QAction>         m_paste;
    QPointer<QAction>         m_duplicate;
    QPointer<QAction>         m_delete;
    QPointer<QAction>         m_deselectAll;
    QPointer<QAction>         m_toggleVisibility;
    QPointer<QAction>         m_showAll;
    QAction*                  m_moveUp{nullptr};
    QAction*                  m_moveDown{nullptr};
    QAction*                  m_contextToggleVisibility{nullptr};
    QAction*                  m_toggleActive{nullptr};
    QPointer<QMenu>           m_contextMenu;
    QPointer<SimulationPanel> m_simulationPanel;
    std::function<bool()>     m_isSimulationTabCurrent;
    /// The connections to the simulations tab's table and actions.
    std::vector<QMetaObject::Connection>    m_simulationConnections;
    ComponentChangeSignal::ScopedConnection m_rocketChanges;
};

}  // namespace QtRocket::Gui
