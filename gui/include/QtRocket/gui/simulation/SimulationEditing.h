#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <QObject>
#include <QString>

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class SimulationClipboard;
class SimulationTableView;
class UserPrompts;

/// What the "Flight simulations" tab does with the simulations of its table (OpenRocket's
/// SimulationPanel: newSimulation(), duplicateSimulation(), copySimulationsAction() with
/// copySimulationValues(), pasteSimulationsAction(), deleteSimulations() with
/// verifyDeleteSimulation(), and CutSimulationAction): New, Duplicate, Copy, Cut, Paste and
/// Delete, each with OpenRocket's undo text and one undo position, and the table's selection
/// afterwards. The table's selection (SimulationTableView::selectedSimulations(): from the top) is
/// what they act on. The QActions, their texts, icons, keys and enable rules, the dialog New opens,
/// and the selection a panel restores afterwards (OpenRocket's updatePreviousSelection() and
/// takeTheSpotlight()) are SimulationPanel's; the window's Edit menu reaches these operations
/// through the panel, as OpenRocket's RocketActions does.
///
/// - New (newSimulation()): a new simulation of the document's rocket and its selected flight
///   configuration, with the options the application's preferences give a new simulation (the
///   launch conditions and the other defaults of DefaultSimulationOptionFactory, as Java's
///   new Simulation(document, rocket) takes them; Simulation(document, rocket, preferences)), named
///   OpenRocketDocument::getNextSimulationName(); the undo step "Add <name>"; added at the end of
///   the list and selected. The settings dialog that follows is the caller's (it gets the
///   simulation).
/// - Duplicate (duplicate()): of each selected simulation, from the top, a
///   Simulation::duplicateSimulation() of the document's rocket (not simulated; the name, the
///   flight configuration, the conditions, the extensions and the plot appearances along), a
///   numbered name ("Simulation 12": SimulationCopy::hasNumberedName()) replaced by the next free
///   one, added at the end of the list; the undo step "Duplicate <the first one's name>" for one
///   simulation, "Duplicate simulations" for more; the duplicates become the selection.
/// - Copy (copy()): the selected simulations on the clipboard (a SimulationCopy: detached copies,
///   see SimulationClipboard), and their lines of the table's text on the system clipboard beside
///   them (SimulationTableCsvExport::copyText(): a spreadsheet takes them), as Java puts both.
///   No undo step.
/// - Paste (paste()): the clipboard's simulations made into new simulations of this document
///   (SimulationCopy::insertInto(), which creates a flight configuration the rocket lacks, inside
///   the undo step), inserted after the topmost selected simulation in the document's list, the
///   numbered names replaced as by Duplicate; the undo step is Duplicate's (Java has no Paste
///   text: "Duplicate <the first copy's name>", "Duplicate simulations"); the pasted simulations
///   become the selection. The clipboard keeps them, so they can be pasted again.
/// - Delete (deleteSelection()): while the preferences ask for it (getConfirmSimDeletion()), the
///   question "Delete simulations" (UserPrompts::askDeleteSimulations(), with the table as the
///   parent); a ticked "Do not ask me again" turns the question off (setConfirmSimDeletion(false))
///   whatever the answer, and only OK deletes. The undo step "Delete <name>" for one simulation,
///   "Delete simulations" for more; then the selected simulations leave the list, and nothing is
///   selected.
/// - Cut (cut()): Copy, then Delete (with its question: a Cancel keeps the copy and the
///   simulations).
/// The undo texts are hard-coded English in OpenRocket; here through tr().
///
/// No simulation pointer is kept across a document change: the operations hold the simulations
/// they work on (std::shared_ptr) and find them in the document again where a change came between.
/// Delete's question runs an event loop: when this object (with its table) went meanwhile,
/// nothing more is done, and a selected simulation that left the document meanwhile is not
/// deleted again.
///
/// Deviations from OpenRocket:
/// - OpenRocket bug 24, fixed: Java mixes the table's view rows and the model's indices. With a
///   sorted table its Paste inserts at the first selected view row + 1 taken as a model index,
///   and New, Duplicate and Paste select the model indices of the new simulations as view rows,
///   so the selection shows other simulations. Here Paste inserts after the topmost selected
///   simulation's own place in the list, and the new simulations are selected themselves, wherever
///   the table's order shows them.
/// - OpenRocket bug 27, fixed: OpenRocket's Paste is enabled only while a row is selected, so
///   nothing can be pasted into a design without simulations, or without a selected row. Here
///   Paste needs only simulations on the clipboard (canPaste()); without a selected row they go
///   at the end of the list.
/// - OpenRocket bug 22, fixed (in the core, SimulationCopy): a pasted simulation belongs to the
///   design it was pasted into, also when it came from another design; OpenRocket's stays a
///   change listener of the design it was copied from, so its changes mark that design unsaved
///   and leave its own design saved.
/// - Operations with nothing to work on (no selected row, an empty clipboard) do nothing and
///   return false, as Java's methods return; enabling the actions is the panel's.
/// - OpenRocket's "Invalid copy selection" message (simpanel.msg.invalidCopySelection) cannot be
///   reached there (it compares the selected row count with the selected rows' number) and is
///   left out.
///
/// The table, its model's document, the clipboard, the preferences and the prompts must outlive
/// this object; make it a child of the table (makeChild<SimulationEditing>(&table, table,
/// clipboard, services.preferences(), services.prompts())).
class SimulationEditing final : public QObject
{
    Q_OBJECT

public:
    /// The editing of the simulations of @p table's model's document, through @p clipboard, with
    /// @p preferences (the application's: a new simulation's defaults, the deletion question's
    /// setting) and @p prompts (the deletion question).
    SimulationEditing(SimulationTableView& table, SimulationClipboard& clipboard,
                      Preferences& preferences, UserPrompts& prompts, QObject* parent);
    ~SimulationEditing() override;

    SimulationEditing(const SimulationEditing&)            = delete;
    SimulationEditing& operator=(const SimulationEditing&) = delete;
    SimulationEditing(SimulationEditing&&)                 = delete;
    SimulationEditing& operator=(SimulationEditing&&)      = delete;

    /// The table whose selection the operations act on.
    [[nodiscard]] SimulationTableView& table() const noexcept { return *m_table; }
    /// The clipboard.
    [[nodiscard]] SimulationClipboard& clipboard() const noexcept { return *m_clipboard; }
    /// The document of the table's simulations.
    [[nodiscard]] OpenRocketDocument& document() const noexcept;

    // ------------------------------------------------------------------- what they need

    /// A row selected: what Copy, Cut and Duplicate need (their actions' rule:
    /// getSelectedRowCount() > 0; Delete's also needs a valid configuration, the panel's).
    [[nodiscard]] bool hasSelection() const;

    /// Simulations on the clipboard: what Paste needs (bug 27, see the class comment; Java also
    /// needs a selected row).
    [[nodiscard]] bool canPaste() const;

    // ---------------------------------------------------------------------- the operations

    /// New (newSimulation() without its dialog): the new simulation, which is in the document and
    /// selected.
    std::shared_ptr<Simulation> newSimulation();

    /// Duplicate (duplicateSimulation(getSelectedSimulations())); false when no row is selected.
    bool duplicate();

    /// Copy (copySimulationsAction()); false when no row is selected.
    bool copy();

    /// Cut (CutSimulationAction): Copy, then Delete; true when the simulations were deleted (the
    /// copy is made also when Delete was cancelled), false also when no row is selected.
    bool cut();

    /// Paste (pasteSimulationsAction()); false when the clipboard holds no simulations.
    bool paste();

    /// Delete (deleteSimulations()); true when the simulations were deleted, false when no row is
    /// selected or the question was cancelled.
    bool deleteSelection();

private:
    /// The selected simulations, from the top.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> selected() const;
    /// Copy of @p simulations (copySimulationsAction()).
    void copySimulations(const std::vector<std::shared_ptr<Simulation>>& simulations);
    /// Delete of @p simulations (deleteSimulations(Simulation[])): the question, then the undo
    /// step and the removals. May destroy this object (the question's event loop).
    bool deleteSimulations(std::vector<std::shared_ptr<Simulation>> simulations);
    /// Whether to delete @p count simulations (verifyDeleteSimulation()); false also when this
    /// object went during the question.
    [[nodiscard]] bool confirmDeletion(std::size_t count);
    /// The undo step of a Duplicate or a Paste of @p count simulations, the first named
    /// @p firstName (duplicateSimulation(Simulation[], int)).
    void addDuplicateUndoPosition(std::size_t count, const QString& firstName) const;

    SimulationTableView* m_table;
    SimulationClipboard* m_clipboard;
    Preferences*         m_preferences;
    UserPrompts*         m_prompts;
};

}  // namespace QtRocket::Gui
