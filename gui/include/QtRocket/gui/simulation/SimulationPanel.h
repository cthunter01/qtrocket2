#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"

class QAction;
class QLabel;
class QMenu;
class QStackedWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class IconButton;
class SimulationClipboard;
class SimulationColumnVisibility;
class SimulationEditing;
class SimulationTableModel;
class SimulationTableView;

/// The "Flight simulations" tab of a design window (OpenRocket's SimulationPanel, swing
/// gui/main/SimulationPanel.java): a row of five buttons, New simulation, Edit simulation, Run
/// simulations, Delete simulations and Plot / Export (IconButtons, centred, kParagraphGap apart:
/// "skip 1, gapright para" in "[grow][][][][][][][grow]"), and under them a card: the help
/// ("helpLabel") while no flight configuration of the rocket has motors, else the simulations
/// table ("simulationTable": SimulationTableView over SimulationTableModel, with its column
/// chooser SimulationColumnVisibility). The operations on the simulations are
/// SimulationEditing's (editing()); the dialogs come through the services:
/// GuiServices::simulationDialogs() (the settings dialog, util/SimulationDialogs) and
/// UserPrompts::runSimulations() (the run dialog, with GuiServices::simulationExecutor()), as
/// decision O-M4-8 has it.
///
/// THE ACTIONS (Java's SimulationAction fields, their objectNames; the popup's items, which show
/// their texts, icons, tooltips and keys):
/// - New simulation ("newSimulationAction", FILE_NEW): enabled while a configuration has motors
///   (hasValidConfig()); newSimulation().
/// - Edit ("editSimulationAction", EDIT_EDIT, Ctrl+E): a row selected and a configuration with
///   motors; editSimulation().
/// - Cut ("cutSimulationAction", EDIT_CUT, the Cut key), Copy ("copySimulationAction",
///   EDIT_COPY, the Copy key) and Duplicate ("duplicateSimulationAction", EDIT_DUPLICATE, Ctrl+D):
///   a row selected; cutSimulations(), copySimulations(), duplicateSimulations().
/// - Paste ("pasteSimulationAction", EDIT_PASTE, the Paste key): simulations on the clipboard,
///   and a row selected or the table shown (see the deviations); pasteSimulations().
/// - Delete ("deleteSimulationAction", EDIT_DELETE, Delete): a row selected and a configuration
///   with motors; deleteSimulations().
/// - Run ("runSimulationAction", SIM_RUN): a row selected and a configuration with motors;
///   runSimulation().
/// - Plot / Export ("plotSimulationAction", SIM_PLOT): exactly one row selected and a
///   configuration with motors; plotSimulation().
/// - Export simulation table as CSV file ("simTableExportAction", SIM_TABLE_EXPORT): the table
///   has rows; the File menu's item (BasicFrame takes it from
///   getExportSimulationTableAsCSVAction()). Triggered, it asks for the export
///   (csvExportRequested(false)).
/// - Export simulation(s) as CSV file ("selectedSimsExportAction", SIM_TABLE_EXPORT): the table
///   has rows and a row is selected; csvExportRequested(true).
/// Their states follow their rules at every change those read (updateActions(): the table's
/// selection, its rows, the clipboard, the configurations' motors). The keys are only shown: the
/// actions are added to the popup menu alone, whose keys Qt answers nowhere (the window's Edit
/// menu has them, as RocketActions' accelerators have them in OpenRocket, where SimulationPanel
/// binds them to "none" in the table so that the menu bar's keys act; the table ignores them,
/// SimulationTableView). The window's Edit menu reaches the simulations through these actions
/// or the operations below (RocketActions: getEditSimulationAction(), ...).
///
/// THE BUTTONS ("newButton", "editButton", "runButton", "deleteButton", "plotButton"): each shows
/// the button text of RocketActions.tieActionToButton(button, action, text) ("New simulation",
/// "Edit simulation", "Run simulations", "Delete simulations", "Plot / Export"), its action's
/// icon, the button's tooltip (setToolTipText(): "Add a new simulation", "Edit the selected
/// simulation", "Re-run the selected simulations", "Delete the selected simulations"; Plot /
/// Export keeps its action's, "Plot or Export the selected simulation(s)"), is enabled as its
/// action is and triggers it.
///
/// THE POPUP MENU ("pm", doPopup(): the table's contextMenuRequested(), a click of the right
/// button on a row): Edit, Cut, Copy, Paste, Duplicate, Delete | Run, Plot / Export, Export
/// simulation(s) as CSV file, with their tooltips shown.
///
/// THE TABLE'S MOUSE (SimulationPanel's mouseClicked() listener over the view's cellClicked() and
/// cellDoubleClicked()): a click of the left button on the status column runs the selected
/// simulations when the table's status of the first selected one is NOT_SIMULATED or OUTDATED
/// (runSimulation()); a double click on the warnings column opens the settings dialog of the
/// first selected simulation on its Warnings tab; on the status column it does nothing; on any
/// other column it opens that simulation's dialog (the view has made it the only selected row),
/// on its Plot data tab when it has data whose status is up to date, else on its Launch
/// conditions tab (openDialog(Simulation)).
///
/// THE OPERATIONS (Java's methods of the same names), each a no-op without the selection it
/// needs:
/// - newSimulation(): SimulationEditing::newSimulation(), the selection remembered
///   (updatePreviousSelection()), then the settings dialog of the new simulation with
///   EditRequest::newSimulation (its Cancel undoes the addition).
/// - editSimulation(): the settings dialog of the selected simulations (a multi-edit when several
///   are).
/// - runSimulation(): the run dialog of the selected simulations (UserPrompts::runSimulations()
///   with the window (window()), the document, them and the executor; nothing without an
///   executor), then the selection kept and the spotlight taken.
/// - plotSimulation(): the first selected simulation selected alone; the run dialog of it when it
///   has no data; then the dialog of it on its Plot data tab (its Launch conditions tab when the
///   run left it without data).
/// - cutSimulations(), copySimulations(), pasteSimulations(), duplicateSimulations(),
///   deleteSimulations(): SimulationEditing's, Cut and Delete remembering the empty selection and
///   taking the spotlight when they deleted (Java's deleteSimulations()).
/// After a dialog the panel keeps its selection and re-reads its table (fireMaintainSelection():
/// SimulationTableModel::refresh()) and takes the spotlight (takeTheSpotlight()). A dialog or a
/// question runs an event loop: when the panel goes meanwhile, nothing more is done; a
/// simulation that left the document meanwhile gets no dialog.
///
/// THE WINDOW'S PART (BasicFrame.stateChanged()): updatePreviousSelection() when the tab stops
/// being the current one; takeTheSpotlight() and then activating() when it becomes it.
/// activating() runs every simulation of the table that is NOT_SIMULATED or OUTDATED, in the
/// table's order, through the run dialog, while the preferences ask for it
/// (Preferences::getAutoRunSimulations()). The table's selection model
/// (table().selectionModel(): getSimulationListSelectionModel(),
/// addSimulationTableListSelectionListener()) tells of every change of the selection.
///
/// The help (updateMotorState(), at its making and at each MOTOR or TREE change of the rocket,
/// an undo among them): "No configuration with motors defined. Please create a valid
/// configuration in the 'Motors & Configuration' tab." (simpanel.lbl.noConfiguration, centred) in
/// place of the table while no flight configuration of the rocket has motors
/// (Rocket::hasMotors() of each id); the table, with every button that needs a configuration
/// with motors enabled again, once one has.
///
/// The panel keeps no simulation across a document change but the ones a running operation holds
/// (std::shared_ptr) and the remembered selection (std::weak_ptr). The document, the services'
/// objects and the clipboard must outlive it.
///
/// Deviations from OpenRocket:
/// - No Monte Carlo button and no "Monte Carlo analysis..." item (Milestone 7, U-M4-3).
/// - OpenRocket bug 24 (Plot / Export): Java selects the first selected row's MODEL index as a
///   VIEW row (convertRowIndexToModel() then addRowSelectionInterval()), so in a sorted table the
///   selection moves to another simulation while the plotted one is the right one; here the
///   plotted simulation itself is selected.
/// - OpenRocket bug 27 (Paste without a selected row): Paste needs simulations on the clipboard
///   and a selected row or the table shown (a configuration with motors); OpenRocket needs a
///   selected row. Without one, the simulations go to the end of the list (SimulationEditing);
///   a design whose table is hidden behind the help takes none, as nothing pasted would be seen.
/// - OpenRocket bug 28 (the remembered selection): updatePreviousSelection() remembers the
///   selected simulations themselves, and takeTheSpotlight() selects those still in the table;
///   only when none of them is (an undo or a redo replaced every simulation) does it select the
///   remembered row numbers, as OpenRocket always does (its rows may show other simulations after
///   a new order).
/// - Plot / Export opens the dialog on its Launch conditions tab when the simulation has no data
///   after its run (a cancelled or failed first run keeps no data: bug 40's fix, and QtRocket's
///   failures record no partial flight); Java switches to its plot tab, which such a dialog does
///   not have.
/// - The buttons have no mnemonic (Java's New simulation, Edit simulation and Delete simulations
///   take Alt+N, Alt+E and Alt+Delete from their actions; the Edit menu has Alt+E too): an
///   IconButton shows its action's text without one, as the design tab's buttons do. The popup's
///   items have their mnemonics where the text holds the letter (Edit, Copy, Duplicate).
/// - The actions' enabled states follow their rules at every change those read; OpenRocket
///   updates them when the selection changes, the motors come or go, and through RocketActions
///   (the clipboard, the rocket), so its Export item can stay disabled for a while after the
///   first simulation came.
/// - Java's simTableExportButton ("Run then Dump simulations") is made but never shown; it is
///   left out.
class SimulationPanel final : public QWidget
{
    Q_OBJECT

public:
    using Column = SimulationColumns::Column;

    /// MigLayout's "para" gap (M2's rule: 12 pixels): between the buttons, and between them and
    /// the card.
    static constexpr int kParagraphGap = 12;

    /// The tab of @p document, asking through @p services and copying to and pasting from
    /// @p clipboard (the application's OpenRocketClipboard), all of which outlive it.
    SimulationPanel(OpenRocketDocument& document, const GuiServices& services,
                    SimulationClipboard& clipboard, QWidget* parent = nullptr);
    ~SimulationPanel() override;

    SimulationPanel(const SimulationPanel&)            = delete;
    SimulationPanel& operator=(const SimulationPanel&) = delete;
    SimulationPanel(SimulationPanel&&)                 = delete;
    SimulationPanel& operator=(SimulationPanel&&)      = delete;

    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return *m_document; }
    [[nodiscard]] const GuiServices&   services() const noexcept { return m_services; }
    [[nodiscard]] SimulationClipboard& clipboard() const noexcept { return *m_clipboard; }

    // ------------------------------------------------------------------ the parts

    /// The table's model, its view ("simulationTable"), its column chooser and the operations on
    /// its simulations.
    [[nodiscard]] SimulationTableModel&       tableModel() const noexcept { return *m_model; }
    [[nodiscard]] SimulationTableView&        table() const noexcept { return *m_table; }
    [[nodiscard]] SimulationColumnVisibility& columnVisibility() const noexcept
    {
        return *m_columnVisibility;
    }
    [[nodiscard]] SimulationEditing& editing() const noexcept { return *m_editing; }

    /// The cards ("cardPanel"): the help ("helpLabel") or the table.
    [[nodiscard]] QStackedWidget* cards() const noexcept { return m_cards; }
    [[nodiscard]] QLabel*         helpLabel() const noexcept { return m_helpLabel; }
    /// Whether the help is the card shown.
    [[nodiscard]] bool isHelpShown() const;
    /// Whether a flight configuration of the rocket has motors (Java's hasValidConfig), as the
    /// last MOTOR or TREE change left it.
    [[nodiscard]] bool hasValidConfig() const noexcept { return m_hasValidConfig; }

    /// The help's text (simpanel.lbl.noConfiguration with the name of the tab "Motors &
    /// Configuration").
    [[nodiscard]] static QString noConfigurationText();

    /// The five buttons.
    [[nodiscard]] IconButton* newButton() const noexcept { return m_newButton; }
    [[nodiscard]] IconButton* editButton() const noexcept { return m_editButton; }
    [[nodiscard]] IconButton* runButton() const noexcept { return m_runButton; }
    [[nodiscard]] IconButton* deleteButton() const noexcept { return m_deleteButton; }
    [[nodiscard]] IconButton* plotButton() const noexcept { return m_plotButton; }

    /// The popup menu ("pm").
    [[nodiscard]] QMenu& popupMenu() const noexcept { return *m_popup; }

    // ------------------------------------------------------------------ the actions

    [[nodiscard]] QAction* newSimulationAction() const noexcept { return m_newAction; }
    [[nodiscard]] QAction* editSimulationAction() const noexcept { return m_editAction; }
    [[nodiscard]] QAction* cutSimulationAction() const noexcept { return m_cutAction; }
    [[nodiscard]] QAction* copySimulationAction() const noexcept { return m_copyAction; }
    [[nodiscard]] QAction* pasteSimulationAction() const noexcept { return m_pasteAction; }
    [[nodiscard]] QAction* duplicateSimulationAction() const noexcept { return m_duplicateAction; }
    [[nodiscard]] QAction* deleteSimulationAction() const noexcept { return m_deleteAction; }
    [[nodiscard]] QAction* runSimulationAction() const noexcept { return m_runAction; }
    [[nodiscard]] QAction* plotSimulationAction() const noexcept { return m_plotAction; }
    /// "Export simulation table as CSV file" (getExportSimulationTableAsCSVAction()), the File
    /// menu's.
    [[nodiscard]] QAction* exportSimulationTableAction() const noexcept
    {
        return m_exportTableAction;
    }
    /// "Export simulation(s) as CSV file", the popup's.
    [[nodiscard]] QAction* exportSelectedSimulationsAction() const noexcept
    {
        return m_exportSelectedAction;
    }

    /// Enables every action by its rule (see the class comment), with the selection, the rows,
    /// the clipboard and the configurations as they are now (updateActions()).
    void updateActions();

    // ------------------------------------------------------------------ the window's part

    /// Remembers the selected simulations (and their rows) for takeTheSpotlight()
    /// (updatePreviousSelection(): the window calls it as the tab stops being the current one).
    void updatePreviousSelection();

    /// In the next turn of the event loop (Java's invokeLater()): gives the table the focus, and
    /// when the table has rows and none is selected, selects the remembered ones (see the
    /// deviations), or the first row when none was remembered (takeTheSpotlight(): the window
    /// calls it when the tab becomes the current one).
    void takeTheSpotlight();

    /// Runs the simulations that are not simulated or out of date, while the preferences ask for
    /// it (activating(): the window calls it when the tab becomes the current one, after
    /// takeTheSpotlight()).
    void activating();

    /// Shows the popup menu at @p globalPosition (doPopup()).
    void showPopupMenu(const QPoint& globalPosition);

    // ------------------------------------------------------------------ the operations

    /// New simulation (see the class comment).
    void newSimulation();
    /// Edit of the selected simulations.
    void editSimulation();
    /// Run of the selected simulations.
    void runSimulation();
    /// Plot / Export of the first selected simulation.
    void plotSimulation();
    /// Cut, Copy, Paste, Duplicate and Delete of the selection (SimulationEditing's).
    void cutSimulations();
    void copySimulations();
    void pasteSimulations();
    void duplicateSimulations();
    void deleteSimulations();

Q_SIGNALS:
    /// An export of the table as a CSV file was asked for (exportSimulationsToCSV(onlySelected)):
    /// of the selected rows' simulations when @p onlySelected, else of every row.
    void csvExportRequested(bool onlySelected);

private:
    /// The selection updatePreviousSelection() remembered.
    struct PreviousSelection
    {
        /// The selected simulations, from the top.
        std::vector<std::weak_ptr<Simulation>> simulations;
        /// Their rows, ascending.
        std::vector<int> rows;
    };

    /// Names the actions, sets their texts, icons, tooltips and keys, and connects them.
    void makeActions();
    /// The row of the five buttons.
    void makeButtons(QWidget& row);
    /// The popup menu.
    void makePopupMenu();
    /// The table's mouse and selection, the clipboard and the rocket.
    void connectParts();
    /// The help or the table (updateMotorState()).
    void updateMotorState();
    /// What takeTheSpotlight() does in the next turn.
    void spotlight();
    /// The simulations takeTheSpotlight() selects when none is selected.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> rememberedSimulations() const;
    /// The table's click of the left button on @p column (see the class comment).
    void cellClicked(const std::shared_ptr<Simulation>& simulation, Column column);
    /// The table's double click on @p column (see the class comment).
    void cellDoubleClicked(const std::shared_ptr<Simulation>& simulation, Column column);
    /// The dialog of @p simulation: its Plot data tab when its data is up to date, else its
    /// Launch conditions tab (openDialog(Simulation)).
    void openDialog(const std::shared_ptr<Simulation>& simulation);
    /// The settings dialog of @p simulations on @p tab (openDialog(plotMode, isNewSimulation,
    /// sims)), then the selection kept and the spotlight taken.
    void openDialog(SimulationDialogs::Tab tab, bool newSimulation,
                    std::vector<std::shared_ptr<Simulation>> simulations);
    /// The run dialog of @p simulations (new SimulationRunDialog(window, document, sims)); false
    /// when the panel went during it.
    bool runSimulations(std::vector<std::shared_ptr<Simulation>> simulations);
    /// fireMaintainSelection(): the table read again; its selection stays on its simulations.
    void maintainSelection();
    /// Whether the status @p row's simulation had at the table's last reading is one a click on
    /// the status column, and activating(), run (NOT_SIMULATED, OUTDATED).
    [[nodiscard]] bool needsRun(int row) const;
    /// Whether @p simulation is a row of the table, has data, and its status at the table's last
    /// reading is up to date (hasSimulationData() && isStatusUpToDate(getStatus())).
    [[nodiscard]] bool isDataUpToDate(const Simulation& simulation) const;

    OpenRocketDocument*                     m_document;
    GuiServices                             m_services;
    SimulationClipboard*                    m_clipboard;
    SimulationTableModel*                   m_model;
    QStackedWidget*                         m_cards;
    QLabel*                                 m_helpLabel;
    SimulationTableView*                    m_table;
    SimulationColumnVisibility*             m_columnVisibility;
    SimulationEditing*                      m_editing;
    QMenu*                                  m_popup;
    QAction*                                m_newAction;
    QAction*                                m_editAction;
    QAction*                                m_cutAction;
    QAction*                                m_copyAction;
    QAction*                                m_pasteAction;
    QAction*                                m_duplicateAction;
    QAction*                                m_deleteAction;
    QAction*                                m_runAction;
    QAction*                                m_plotAction;
    QAction*                                m_exportTableAction;
    QAction*                                m_exportSelectedAction;
    IconButton*                             m_newButton{nullptr};
    IconButton*                             m_editButton{nullptr};
    IconButton*                             m_runButton{nullptr};
    IconButton*                             m_deleteButton{nullptr};
    IconButton*                             m_plotButton{nullptr};
    bool                                    m_hasValidConfig{false};
    PreviousSelection                       m_previousSelection;
    ComponentChangeSignal::ScopedConnection m_rocketChanges;
};

}  // namespace QtRocket::Gui
