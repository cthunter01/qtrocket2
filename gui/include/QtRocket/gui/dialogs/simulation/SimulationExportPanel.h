#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

#include <QObject>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"

class QComboBox;
class QGroupBox;

namespace QtRocket
{
class FlightDataType;
class Preferences;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class CsvExportPanel;

/// A simulation's "Export data" tab (OpenRocket's SimulationExportPanel, swing gui/simulation/
/// SimulationExportPanel.java), which SimulationConfigDialog shows for a simulation with flight
/// data: the choice of the variables of a CSV file of the simulation's flight data and its
/// options, and the export itself (doExport(), the settings dialog's "Export" button).
///
/// What it shows (Java's create()): a CsvExportPanel (widgets/, Java's superclass CSVExportPanel,
/// with the options in a column on the right, Java's separateRowForOptions false) of one row per
/// flight data type of the first branch of the simulation's data (Java's branch 0 getTypes(), in
/// their sorted order, whatever stage is exported), each named as the plots name it
/// (flightDataTypeText(): Java's getName(), which is translated), ticked as the preferences say
/// (CsvOptionPreferences::isFlightDataTypeExportSelected(): the node "exports", unticked when
/// nothing is stored) and in the default unit of its group; the CSV options (CsvOptionPanel of the
/// base class name "SimulationExportPanel", from the preferences) with the three comment options
/// "Include simulation description", "Include field descriptions" and "Include flight events"
/// (commentOptions(): SimExpPan.checkbox.*, their tooltips SimExpPan.checkbox.ttip.*); and, when
/// the data has two branches or more, below the options the group "Stage to export"
/// (SimExpPan.border.Stage, stagePanel) with a box of the branches' labels
/// (PlotUtil::generateSeriesLabels(): "Sustainer", "Booster stage(1)", "Booster stage(2)", ...;
/// stageSelection), the first chosen. The rows, the stages and the names are read once, when the
/// panel is made (as Java's).
///
/// The export (doExport(), Java's doExport() and SaveCSVWorker): the file question
/// (UserPrompts::askSaveFile() of exportRequest(): Java's SaveFileChooser, with no title of its
/// own, "Comma Separated Files (*.csv)" chosen beside "All Files", the default directory, no
/// proposed name, names with an illegal character refused); ".csv" added to the name unless it
/// ends in it (FileNames::forceExtension()); for a file that exists, the question whether to
/// overwrite it (UserPrompts::askOverwrite(), FileHelper.confirmWrite()); then the options stored
/// (CsvOptionPanel::storePreferences()), the dialog's directory stored as the default directory,
/// every row's tick stored (CsvOptionPreferences::setFlightDataTypeExportSelected()); and the
/// ticked rows' variables written in their units, in the rows' order, through the core's
/// CsvExport (its path overload: a binary file, '\n' line ends), from the branch of the stage
/// chosen, with the options shown (zero ticked variables write a file too, as in Java).
///
/// Lifetime: the panel holds the simulation by its std::shared_ptr and never a reference to its
/// flight data or a branch of it: the data is fetched again when the export writes (see bug 49
/// below). The flight data types of the rows live for the whole process (DataBranch). The
/// services' preferences and prompts must outlive the panel.
///
/// OpenRocket bugs fixed here (numbered as plan 11.4 and docs/gui-differences.md number them):
/// - Bug 49: the export writes the simulation's flight data as it is at the export, fetched
///   again then (Simulation::getSimulatedData()). Java's panel keeps the branch of the data the
///   dialog opened with, so that after the settings dialog's Export has run the outdated
///   simulation again (SimulationConfigDialog's OK), it wrote the old flight under the new
///   simulation's comments (its status, its warnings); in C++ a kept branch would dangle once
///   the run replaced the data. The stage chosen is looked up in the data of the export: when
///   that data has fewer branches than the stage's index, the first branch is written.
/// - Bug 53: an export that cannot be written shows "Error writing file" (error.writing.title)
///   with "An error occurred while writing to the file:" (error.writing.desc) and the reason
///   (UserPrompts::showMessage(), ERROR), whether the file could not be opened or a write
///   failed. Java shows its bug report dialog ("Uncaught exception") when the file cannot be
///   opened and nothing when a write fails (its PrintWriter swallows the error); its Cancel of a
///   long export deleted the file while the worker went on writing it.
///
/// Deviations from OpenRocket:
/// - The export runs on the GUI thread under a wait cursor (WaitCursor), without a progress
///   dialog or a Cancel (Java: SaveCSVWorker on a worker thread, SwingWorkerDialog's "Exporting
///   flight data" with Cancel once it takes two seconds or more).
/// - A ticked variable the exported branch does not have (a stage of another flight than the
///   first stage's, a run that changed the types) is reported as the error above, before the
///   file is opened (the core's CsvExport). For the stage, Java's export fails with a
///   NullPointerException in its worker at the first row (its bug report dialog; the file keeps
///   the comments written before); after a run, Java writes the branch it kept (bug 49).
/// - The tick of a variable is stored under the core's English name of its type (see
///   CsvOptionPreferences), Java's under its translated name.
/// - Not a CsvExportPanel itself (Java's SimulationExportPanel extends CSVExportPanel): it holds
///   one (exportPanel()), as a panel whose options and extra widgets the CsvExportPanel takes
///   over must exist before it.
class SimulationExportPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The tab of @p simulation (Java: create()), reading its ticks and options from @p services'
    /// preferences and asking its questions through @p services' prompts.
    /// @throws BugError when @p simulation is null or has no flight data, or no branch (Java's
    ///         IllegalArgumentException "No data for panel")
    SimulationExportPanel(const GuiServices& services, std::shared_ptr<Simulation> simulation,
                          QWidget* parent = nullptr);

    /// The three comment options of the CSV options (SimulationExportPanel.OPTION_SIMULATION_
    /// COMMENTS, OPTION_FIELD_DESCRIPTIONS, OPTION_FLIGHT_EVENTS: CsvOptionPreferences::
    /// kSimulationCommentsOption, ...), in that order, with their tooltips.
    [[nodiscard]] static std::vector<CsvOptionPanel::CommentOption> commentOptions();

    /// The file question of the export with @p preferences (see the class comment): no title,
    /// FileNames::csvFilter() then FileNames::allFilesFilter(), the default directory (none when
    /// none is stored), no proposed name, refuseIllegalNames.
    [[nodiscard]] static UserPrompts::SaveFileRequest exportRequest(const Preferences& preferences);

    /// The export (see the class comment): true when the file was written; false when the
    /// simulation has no flight data any more (nothing is asked), when a question was cancelled
    /// or answered No (nothing is stored), when the panel went during a question, or when the
    /// file could not be written (after the error message).
    bool doExport();

    /// The simulation.
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }
    /// The type of each row, in the rows' order.
    [[nodiscard]] const std::vector<const FlightDataType*>& types() const noexcept
    {
        return m_types;
    }
    /// The variables' table with the options and the stage group (objectName "csvExportPanel").
    [[nodiscard]] CsvExportPanel& exportPanel() const noexcept { return *m_exportPanel; }
    /// The CSV options (the export panel's).
    [[nodiscard]] CsvOptionPanel& csvOptions() const noexcept { return *m_csvOptions; }
    /// "Stage to export" and its box; null with one branch.
    [[nodiscard]] QGroupBox* stagePanel() const noexcept { return m_stagePanel; }
    [[nodiscard]] QComboBox* stageSelection() const noexcept { return m_stageSelection; }
    /// The index of the stage chosen (the branch exported): 0 without the stage box.
    [[nodiscard]] std::size_t stageIndex() const;

private:
    /// The stage group over @p labels, the branches' labels (two or more).
    [[nodiscard]] QGroupBox* makeStagePanel(const std::vector<QString>& labels);
    /// Writes the ticked rows' variables of the simulation's data as it is now to @p file with
    /// @p options (CsvOptionPanel::values()); false after the error message.
    bool writeFile(const std::filesystem::path& file, const CsvOptionValues& options);

    GuiServices                        m_services;
    std::shared_ptr<Simulation>        m_simulation;
    std::vector<const FlightDataType*> m_types;
    CsvOptionPanel*                    m_csvOptions{nullptr};
    QGroupBox*                         m_stagePanel{nullptr};
    QComboBox*                         m_stageSelection{nullptr};
    CsvExportPanel*                    m_exportPanel{nullptr};
};

}  // namespace QtRocket::Gui
