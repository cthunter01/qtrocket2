#pragma once

#include <functional>
#include <memory>
#include <span>
#include <vector>

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The simulation dialogs that the Simulations tab (and the settings dialog's plot tab) open, as
/// an interface (O-M4-8), so that their callers' tests can see what is asked without showing
/// them: the settings dialog (OpenRocket's SimulationConfigDialog, which SimulationPanel opens
/// through openDialog() and the Warnings column's double click) and the plot dialog
/// (SimulationPlotDialog.getPlot() of the plot tab's doPlot()). One object serves the application
/// (app/ApplicationContext owns it, GuiServices::simulationDialogs() hands it out); the tests
/// record the requests (tests/gui/RecordingSimulationDialogs.h). Called on the GUI thread.
///
/// These are dialogs of their own, not questions: they go neither through UserPrompts nor through
/// its doubles (the critic's G14); their own questions (the discard question, the run dialog, the
/// file dialogs, the messages) do. The wind editor and the aerodynamic lookup dialog are children
/// of the settings dialog that it opens itself (ModalDialog).
class SimulationDialogs
{
public:
    /// The tab the settings dialog shows when it opens (SimulationConfigDialog's
    /// switchToSettingsTab(), switchToWarningsTab(), switchToPlotTab(), switchToExportTab()).
    enum class Tab
    {
        /// "Launch conditions", the first: New, Edit, and a double click on a simulation that
        /// is not up to date or has no data.
        LAUNCH_CONDITIONS,
        /// "Warnings": a double click on the Warnings column.
        WARNINGS,
        /// "Plot data": Plot / Export, and a double click on a simulation with data that is up
        /// to date.
        PLOT_DATA,
        /// "Export data".
        EXPORT_DATA,
    };

    /// What the settings dialog edits (SimulationConfigDialog(parent, document, isNewSimulation,
    /// simulations...)).
    struct EditRequest
    {
        /// The document's simulations to edit, at least one: the first is edited, and on OK the
        /// others get its conditions and extensions (multi-edit, "Multi-simulation edit").
        std::vector<std::shared_ptr<Simulation>> simulations;
        Tab                                      tab = Tab::LAUNCH_CONDITIONS;
        /// The simulation was just added (New simulation): Cancel asks whether to undo adding it
        /// (SimulationConfigDialog.CancelOperation.msg.undoAdd) and undoes the addition.
        bool newSimulation = false;
        /// What cancels the window's background estimate (figure/BackgroundSimulation::
        /// cancelFor()) of the simulations the dialog's OK runs: handed on to the run's request
        /// (UserPrompts::RunSimulationsRequest::cancelBackgroundRuns). Filled by the opener from
        /// the window's design view (O-M4-16); may be empty: nothing is cancelled then.
        std::function<void(std::span<const std::shared_ptr<Simulation>> simulations)>
            cancelBackgroundRuns;
        /// What pauses the window's background estimate while the dialog is open
        /// (figure/BackgroundSimulation::setPaused(), O-M4-16: no estimate starts or is adopted
        /// while a simulation dialog of the window is open): the dialog calls it with true as the
        /// last step of its making (after its undo position; never when its making fails), and
        /// with false once when it has closed (after the undo of a Cancel), never when it is
        /// destroyed open with its window. Filled by the opener; may be empty.
        std::function<void(bool paused)> pauseBackgroundRuns;
    };

    SimulationDialogs()                                    = default;
    virtual ~SimulationDialogs()                           = default;
    SimulationDialogs(const SimulationDialogs&)            = delete;
    SimulationDialogs& operator=(const SimulationDialogs&) = delete;
    SimulationDialogs(SimulationDialogs&&)                 = delete;
    SimulationDialogs& operator=(SimulationDialogs&&)      = delete;

    /// Shows the settings dialog over @p document's simulations of @p request, modal to the
    /// window of @p parent (DOCUMENT_MODAL in OpenRocket), and returns when it has closed (Java's
    /// setVisible(true)); the caller then keeps its selection (fireMaintainSelection()) and takes
    /// the spotlight again, as SimulationPanel.openDialog() does. The dialog changes the
    /// simulations and the document itself (an undo position at open, the edits live, Cancel's
    /// undo); no simulation pointer of the caller should be relied on afterwards but through the
    /// document.
    virtual void editSimulations(QWidget* parent, OpenRocketDocument& document,
                                 const EditRequest& request) = 0;

    /// Opens the plot dialog of @p simulation (which has data) for @p configuration (the plot
    /// tab's, copied: the dialog keeps its own), window-modal to @p parent (the settings dialog),
    /// and returns at once (QDialog::open()): SimulationPlotDialog.getPlot(parent, simulation,
    /// configuration).setVisible(true).
    virtual void plotSimulation(QWidget* parent, std::shared_ptr<Simulation> simulation,
                                const SimulationPlotConfiguration& configuration) = 0;
};

}  // namespace QtRocket::Gui
