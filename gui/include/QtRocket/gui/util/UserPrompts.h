#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <QColor>
#include <QString>
#include <QStringList>

#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

class QObject;
class QWidget;

namespace QtRocket
{
class ComponentPreset;
class FlightConfigurationId;
class Motor;
class MotorMount;
class OpenRocketDocument;
class Simulation;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;
class SimulationExecutor;

/// Every modal question the GUI asks and every message box it shows, as one interface, so that
/// no code below the main windows opens a modal dialog itself: an offscreen test would hang in
/// it. The application's implementation shows OpenRocket's dialogs, with their texts (main/,
/// WidgetUserPrompts); the tests answer from a script (tests/gui/ScriptedUserPrompts.h). The
/// caller passes the dialog's parent window (or nullptr) and does what OpenRocket does with the
/// answer: storing a "do not ask again" preference, the chosen directory as the default
/// directory, ...
///
/// The texts of the fixed questions (insert position, discard, save changes, overwrite, storage
/// options, design information, the flight configuration dialogs, the motor chooser, the parts
/// library, the database load problems) belong to the implementation; the caller gives the
/// texts of the others.
///
/// The questions of Milestone 3 (the "Motors & Configuration" tab, the motor chooser, the parts
/// library, the preferences' user files and the database load problems) are all declared here,
/// with what OpenRocket's call sites pass their dialogs and read back from them: OpenRocket's
/// dialogs edit the rocket themselves (IgnitionSelectionDialog edits the mount's configuration
/// live, DeploymentSelectionDialog and SeparationSelectionDialog store their result in OK's
/// action, RenameConfigDialog renames), where here each question gives the user's answer as a
/// value and the caller applies it as the dialog's OK action did, after its undo position. Only
/// the motor chooser is shown a component (the mount, which it reads while it is open); no
/// answer refers to one, so no component pointer outlives a question.
///
/// The questions of Milestone 4 (the Simulations tab, the simulation settings dialog, its wind
/// editor and aerodynamic lookup, the plots and the exports) are all declared here too, with what
/// OpenRocket's call sites give their dialogs and read back:
/// - askDeleteSimulations(): SimulationPanel.verifyDeleteSimulation()'s "Delete simulations",
///   asked while Preferences::getConfirmSimDeletion() is on;
/// - askDiscardChanges() with SIMULATION_CHANGES or SIMULATION_ADDITION: SimulationConfigDialog.
///   cancelSimEdit()'s "Cancel operation", asked while isShowDiscardSimulationConfirmation() is
///   on;
/// - runSimulations(): SimulationRunDialog, which runs the simulations and returns when they are
///   done or cancelled;
/// - askWindCsvImportSettings(): CSVImportSettingsDialog ("Wind Data Import Settings");
/// - askColor(): ColorChooserButton's JColorChooser dialog;
/// - askYesNo(): the wind editor's "Overwrite wind levels?" (WindProfileEditorDlg.dlg.
///   overwriteLevels.title, .msg; WARNING), before Reset levels and before an import over levels;
/// - askOpenFiles(): the wind editor's "Import wind levels" (WindProfileEditorDlg.dlg.importLevels.
///   title; FileHelper.CSV_FILTER chosen beside "All Files", in the default directory, which the
///   caller then stores; no extension forced on the chosen file: OpenRocket's forceExtension() on
///   an open dialog is bug 37) and the aerodynamic lookup's "Load from file..."
///   (AerodynamicLookupDialog.title.choose "Select CSV file", AerodynamicLookupDialog.csvFilter
///   for *.csv *.txt *.dat, in the default directory with the name of the table's file filled
///   in: OpenFileRequest::fileName, as Java's setSelectedFile() and then setCurrentDirectory());
/// - askSaveFile() and askOverwrite(): the export tab's CSV file (SimulationExportPanel.doExport():
///   a SaveFileChooser, so refuseIllegalNames; FileHelper.CSV_FILTER beside "All Files", the
///   default directory, no proposed name; the caller forces ".csv" and then asks askOverwrite()),
///   the simulation table's CSV file (SimulationPanel.exportSimulationsToCSV():
///   simpanel.pop.exportToCSV.save.dialog.title "Save as CSV file", FileHelper.CSV_FILTER only, the
///   default directory, which is not stored after, "<rocket name>.csv" proposed,
///   refuseIllegalNames, and the CSV options asked after the file: SaveFileRequest::csvOptions),
///   and the plot dialog's Export Image (plot/PlotExport, as in Milestone 3).
/// Every file question of Milestone 4 goes through these (the critic's G8): only
/// WidgetFileDialogTest shows Qt's own file dialog. The settings dialog, the plot dialog, the wind
/// editor and the lookup dialog are dialogs of their own, not questions (util/SimulationDialogs;
/// ModalDialog children of the settings dialog).
///
/// The messages of Milestone 4 go through showMessage() with these texts (DetailDialog without
/// details, as M3's messages):
/// - the table export without rows (SimulationPanel.exportSimulationsToCSV()): INFORMATION, the
///   line simpanel.dlg.no.simulation.table.rows ("Simulation table has no entries... Please run a
///   simulation first.", with Java's ellipsis character), and JOptionPane's own title of a
///   message dialog, "Message" (Swing's OptionPane.messageDialogTitle);
/// - a Copy whose selection cannot be read (SimulationPanel.copySimulationValues()): ERROR, title
///   and line both simpanel.msg.invalidCopySelection ("Invalid copy selection");
/// - Plot with no variable on the y axis (SimulationPlotPanel.doPlot()): ERROR, title
///   SimulationPlotPanel.error.noPlotSelected.title ("Nothing to plot"), line
///   SimulationPlotPanel.error.noPlotSelected ("Please add one or more variables to plot on the
///   Y-axis.");
/// - a 16th y-axis variable (PlotPanel's "New Y axis plot type"): ERROR, title
///   simplotpanel.OptionPane.lbl2 ("Cannot add plot"), line simplotpanel.OptionPane.lbl1 ("A
///   maximum of 15 plots is allowed.");
/// - a run that failed (SimulationRunDialog's DetailDialog): ERROR, the simulation's name as the
///   title, the lines SimuRunDlg.msg.errorOccurred ("An error occurred during the simulation:"),
///   or for a calculation of a branch SimuRunDlg.msg.branchErrorOccurred ("An error occurred
///   during simulation branch") + " \"<branch name>\"", and then the error's message; one at a
///   time, the runs' poll paused meanwhile. A run that threw goes to Guard instead (OpenRocket's
///   "An exception occurred during the simulation");
/// - an import of wind levels that failed (MultiLevelWindEditDialog.importLevels()): ERROR, title
///   WindProfileEditorDlg.msg.importLevelsError.title ("Import Wind Levels Failed"), the lines
///   WindProfileEditorDlg.msg.importLevelsError ("An issue occurred while importing wind
///   levels:") and the error's message;
/// - the CSV import settings that cannot be used (CSVImportSettingsDialog's OK, from inside that
///   dialog, through the same prompts): ERROR, title CSVImportSettingsDialog.validationError
///   ("Incorrect Data Input"), the line CSVImportSettingsDialog.emptyColumnNameError,
///   .duplicateColumnNameError or .duplicateColumnError;
/// - a lookup table that cannot be loaded (AerodynamicLookupDialog.showLookupError()): ERROR, title
///   AerodynamicLookupDialog.error.title ("Failed to load CSV"), AerodynamicLookupDialog.error.msg
///   ("Failed to load %s:%n%s": the file, a line break, the reason); and a table's file that is
///   gone (refresh): WARNING, the same title, AerodynamicLookupDialog.error.fileNotFound ("The CSV
///   file does not exist:%n%s");
/// - an export that cannot be written (the export tab's and the table's, bug 53): ERROR, title
///   error.writing.title ("Error writing file"), the lines error.writing.desc ("An error occurred
///   while writing to the file:") and the error's message, as Milestone 3's exports;
/// - the launch conditions' easter egg (SimulationConditionsPanel.addEasterEgg(), O-M4-19):
///   INFORMATION, title "42", the line "The answer to the ultimate question of life, the universe,
///   and everything." (Java's literals).
class UserPrompts
{
public:
    // ------------------------------------------------------------------ the answers and requests

    /// Which add button asks where to insert the new component (ComponentAddButtons.askPosition):
    /// a body component's ("compaddbuttons.lbl.insertcomp", title "compaddbuttons.Selectcomppos")
    /// or a stage's ("compaddbuttons.lbl.insertstage", "compaddbuttons.Selectstagepos").
    enum class InsertPositionQuestion
    {
        BODY_COMPONENT,
        STAGE,
    };

    /// Insert here, Add to the end, or Cancel (closing the dialog cancels too).
    enum class InsertPosition
    {
        INSERT_HERE,
        ADD_TO_END,
        CANCEL,
    };

    struct InsertPositionAnswer
    {
        InsertPosition position = InsertPosition::CANCEL;
        /// "Do not ask me again" was ticked; OpenRocket stores the position as the preference
        /// unless the answer is CANCEL.
        bool doNotAskAgain = false;
    };

    /// Whose changes Cancel would discard: the configuration dialog's, editing a component
    /// ("RocketCompCfg.CancelOperation.msg.discardChanges") or a component just added
    /// ("RocketCompCfg.CancelOperation.msg.undoAdd"), both titled
    /// "RocketCompCfg.CancelOperation.title"; the preferences dialog's
    /// ("PreferencesDialog.CancelOperation.*"); or the simulation settings dialog's
    /// (SimulationConfigDialog.createCancelOperationContent()), editing a simulation
    /// ("SimulationConfigDialog.CancelOperation.msg.discardChanges") or a simulation just added
    /// ("SimulationConfigDialog.CancelOperation.msg.undoAdd"), both titled
    /// "SimulationConfigDialog.CancelOperation.title".
    enum class DiscardQuestion
    {
        COMPONENT_CHANGES,
        COMPONENT_ADDITION,
        PREFERENCE_CHANGES,
        SIMULATION_CHANGES,
        SIMULATION_ADDITION,
    };

    struct DiscardAnswer
    {
        /// Yes: discard the changes.
        bool discard = false;
        /// "Do not ask me again" was ticked. OpenRocket turns the confirmation off as soon as the
        /// box is ticked, whatever the answer.
        bool doNotAskAgain = false;
    };

    /// The answer to "Design '<name>' has not been saved. Do you want to save it?"
    /// (BasicFrame.dlg.*); closing the dialog is CANCEL.
    enum class SaveChangesAnswer
    {
        SAVE,
        DISCARD,
        CANCEL,
    };

    /// What the user chose in the internal error dialog.
    enum class InternalErrorAnswer
    {
        /// Go on working; closing the dialog answers this too.
        CONTINUE,
        /// End the application at once, without saving any design.
        QUIT,
    };

    /// The icon and kind of a message or question.
    enum class Severity
    {
        INFORMATION,
        WARNING,
        ERROR,
    };

    /// A yes/no question with the caller's texts (JOptionPane.showConfirmDialog YES_NO), such
    /// as "pref.dlg.clearCachedPreferences.*".
    struct Question
    {
        Severity severity = Severity::WARNING;
        QString  title;
        /// Plain or rich text (QMessageBox decides).
        QString text;
    };

    /// A message box with the caller's texts (JOptionPane.showMessageDialog): each line is a
    /// paragraph, as OpenRocket passes an Object[] of strings.
    struct Message
    {
        Severity    severity = Severity::INFORMATION;
        QString     title;
        QStringList lines;
    };

    /// OpenRocket's WarningDialog: the message lines above a list of warnings
    /// ("BasicFrame.WarningDialog.*" after a load or a save), each as Warning::toString().
    struct WarningsMessage
    {
        QString     title;
        QStringList lines;
        QStringList warnings;
    };

    /// OpenRocket's ErrorWarningDialog: the message lines above the list of errors and the list
    /// of warnings ("BasicFrame.ErrorWarningDialog.*" after a save whose saver reported errors),
    /// each as its toString().
    struct ErrorsAndWarningsMessage
    {
        QString     title;
        QStringList lines;
        QStringList errors;
        QStringList warnings;
    };

    /// What a file chooser lets the user pick (JFileChooser.setFileSelectionMode()).
    enum class OpenFileSelection
    {
        /// Existing files (FILES_ONLY, JFileChooser's default).
        FILES,
        /// An existing file or a directory (FILES_AND_DIRECTORIES: the preferences' user
        /// thrust curves and component presets, GeneralPreferencesPanel's Add). Qt's file
        /// dialogs pick either files or a directory, so the application asks first which of
        /// the two, with a menu "File..." / "Directory..." (the user's decision U-M3-6c), and
        /// then shows the platform's dialog for it.
        FILES_AND_DIRECTORIES,
    };

    /// The files to open (a JFileChooser in OPEN mode).
    struct OpenFileRequest
    {
        QString title;
        /// Qt's name filters, "OpenRocket designs (*.ork *.ork.gz)", in their order in the
        /// dialog.
        QStringList           nameFilters;
        std::filesystem::path directory;
        bool                  multipleFiles = false;
        /// A text shown beside the files (the image import's "CustomFinImport.description");
        /// empty for none.
        QString description;
        /// The filter chosen when the dialog opens, one of nameFilters (JFileChooser's
        /// setFileFilter()); empty for the first.
        QString selectedNameFilter;
        /// Files, or files and directories.
        OpenFileSelection selection = OpenFileSelection::FILES;
        /// The text of the button that accepts the choice (JFileChooser.showDialog(parent,
        /// approveButtonText): "pref.dlg.Add" for the preferences' user files); empty for the
        /// platform's own ("Open").
        QString approveButtonText;
        /// The name of the file selected in directory when the dialog opens (JFileChooser.
        /// setSelectedFile(): the aerodynamic lookup's current table file); empty for none.
        // NOLINTNEXTLINE(readability-redundant-member-init): designated initializers omit it
        QString fileName = {};
    };

    struct OpenFileAnswer
    {
        /// At least one file (with FILES_AND_DIRECTORIES: the one file or directory chosen).
        std::vector<std::filesystem::path> files;
        /// The directory the dialog was in, which OpenRocket stores as the default directory.
        std::filesystem::path directory;
    };

    /// The file to save to (a JFileChooser in SAVE mode). The dialog never asks itself whether
    /// to overwrite a file: the caller forces the extension first and then asks askOverwrite().
    struct SaveFileRequest
    {
        QString               title;
        QStringList           nameFilters;
        std::filesystem::path directory;
        /// The proposed name, "<design>.ork".
        QString fileName;
        /// For a design: the storage option the dialog offers with the files
        /// (StorageOptionChooser, "Simulated data to store"; WidgetUserPrompts asks it once the
        /// file is chosen), true for all simulated data; nullopt for no such option.
        std::optional<bool> saveSimulationData;
        /// OpenRocket's SaveFileChooser (Save As and most of its save dialogs, not a plain
        /// JFileChooser such as the fin points' CSV export): a name with one of the characters
        /// of SaveFileNames::illegalCharacter() is refused with the warning "Illegal filename"
        /// and asked for again (WidgetUserPrompts).
        bool refuseIllegalNames = false;
        /// For the simulation table's export: the CSV options the dialog asks once the file is
        /// chosen (CsvOptionPanel(SimulationTableCSVExport.class), the JFileChooser accessory of
        /// SimulationPanel.setUpSimExportCSVFileChooser(); WidgetUserPrompts asks them in a
        /// dialog after the file, U-M4-6), starting with these values (the caller's, from the
        /// preferences); nullopt for no such options. The table export's panel has no comment
        /// options: commentOptions come back as they are given.
        // NOLINTNEXTLINE(readability-redundant-member-init): designated initializers omit it
        std::optional<CsvOptionValues> csvOptions = {};
    };

    struct SaveFileAnswer
    {
        std::filesystem::path file;
        std::filesystem::path directory;
        /// The storage option chosen, when the request had one; the caller stores it in the
        /// document's storage options and marks them explicitly set.
        std::optional<bool> saveSimulationData;
        /// The name filter chosen in the dialog, one of the request's nameFilters (JFileChooser's
        /// getFileFilter()); empty when the dialog does not tell (the caller then takes the
        /// first). A plot's export gives a name without the suffix of a kind it writes the
        /// chosen filter's suffix (PlotExport).
        // NOLINTNEXTLINE(readability-redundant-member-init): designated initializers omit it
        QString selectedNameFilter = {};
        /// The CSV options chosen, when the request had them; the caller stores them in the
        /// preferences (CsvOptionPanel.storePreferences()) and exports with them.
        // NOLINTNEXTLINE(readability-redundant-member-init): designated initializers omit it
        std::optional<CsvOptionValues> csvOptions = {};
    };

    // ---------------------------------------------- the flight configuration dialogs (M3)

    /// Which flight configurations the choice of a value dialog (ignition, deployment,
    /// separation) is for: its two radio buttons under "Which flight configurations are
    /// affected:" (IgnitionSelectionDialog.opt.*, DeploymentSelectionDialog.opt.*,
    /// SeparationSelectionDialog.opt.*). Java's dialogs tell it by isOverrideDefault().
    enum class ConfigurationScope
    {
        /// "Change all configurations using the default ... event": the default value.
        ALL_CONFIGURATIONS,
        /// "Override for the {0} flight configuration only": an override of the
        /// configuration.
        THIS_CONFIGURATION,
    };

    /// The ignition of a motor mount (IgnitionSelectionDialog, "Select Ignition
    /// Configuration"). Applied as the dialog's OK action: for ALL_CONFIGURATIONS the mount's
    /// default configuration and every configuration of the mount get the event and delay (as
    /// OpenRocket does, the overridden ones too); for THIS_CONFIGURATION the configuration's own
    /// (made from the default when the cell has none, where OpenRocket's dialog edits the
    /// default: a fixed bug).
    struct IgnitionChoice
    {
        ConfigurationScope scope = ConfigurationScope::ALL_CONFIGURATIONS;
        /// "Ignition at:".
        IgnitionEvent event = IgnitionEvent::AUTOMATIC;
        /// "plus ... seconds": the ignition delay in s (at least 0).
        double delay = 0.0;
    };

    /// MotorConfigurationPanel.selectIgnition(): the dialog over the first selected cell.
    struct IgnitionRequest
    {
        /// The configuration's name in "Override for the {0} flight configuration only"
        /// (RocketDescriptor.format(rocket, fcid): FlightConfiguration::getName(), "[B4-4]").
        QString configurationName;
        /// What the dialog starts with: the radio button (OpenRocket starts on "all" when the
        /// cell has no motor, a bug the caller fixes: on "all" when the mount's configuration
        /// has no ignition override), and the cell's ignition event and delay (the mount's
        /// default configuration's for a cell without a configuration of its own).
        IgnitionChoice initial;
    };

    /// The deployment of a recovery device (DeploymentSelectionDialog, "Select Deployment
    /// Configuration"): the event ("Deploys at:"), the altitude ("Altitude:", for ALTITUDE)
    /// and the delay ("plus ... seconds"), as DeploymentConfiguration holds them. Applied as the
    /// dialog's OK action: the device's default deployment for ALL_CONFIGURATIONS (the
    /// overrides stay), the configuration's override for THIS_CONFIGURATION.
    struct DeploymentChoice
    {
        ConfigurationScope      scope = ConfigurationScope::ALL_CONFIGURATIONS;
        DeploymentConfiguration deployment;
    };

    /// RecoveryConfigurationPanel.selectDeployment(): the dialog over the first selected cell.
    struct DeploymentRequest
    {
        /// As IgnitionRequest's.
        QString configurationName;
        /// The radio button (OpenRocket always starts on "this configuration", a bug the
        /// caller fixes: on "all" when the device has no override for the configuration) and
        /// the configuration's deployment.
        DeploymentChoice initial;
        /// The device is in the bottom stage (stage number + 1 == the rocket's stage count):
        /// "Lower stage separation" is not offered.
        bool bottomStage = false;
    };

    /// The separation of a stage (SeparationSelectionDialog, "Select Separation
    /// Configuration"): the event ("Upper stage separates at:"), the altitude (for the two
    /// altitude events) and the delay, as StageSeparationConfiguration holds them. The dialog
    /// answers a delay of 0 for NEVER (its OK action). Applied as that action: for
    /// ALL_CONFIGURATIONS every override of the stage is removed and the default set, as
    /// OpenRocket does; for THIS_CONFIGURATION the configuration's override.
    struct SeparationChoice
    {
        ConfigurationScope           scope = ConfigurationScope::ALL_CONFIGURATIONS;
        StageSeparationConfiguration separation;
    };

    /// SeparationConfigurationPanel.selectSeparation(): the dialog over the first selected
    /// cell.
    struct SeparationRequest
    {
        /// As IgnitionRequest's.
        QString configurationName;
        /// The radio button (as DeploymentRequest's) and the configuration's separation.
        SeparationChoice initial;
    };

    /// How the Rename Configuration dialog was left.
    enum class RenameAction
    {
        /// OK: the configuration gets the text field's text as its name (an empty one is the
        /// default name, FlightConfiguration.setName()).
        RENAME,
        /// "Reset to default": the configuration gets the default name (setName(null)).
        RESET,
        /// Cancel, Escape or the window's close button: nothing changes.
        CANCEL,
    };

    /// FlightConfigurationPanel.renameConfigurationAction(): RenameConfigDialog over the first
    /// selected configuration.
    struct RenameConfigurationRequest
    {
        /// The text field's start: the configuration's raw name (getNameRaw(), "[{motors}]").
        QString name;
        /// "Save as default", which leaves the dialog open: called with the dialog (the parent
        /// of what the caller shows) and the text field's text each time the button is clicked.
        /// OpenRocket then shows "This change will only affect new flight configurations."
        /// (RenameConfigDialog.dlg.saveDefault.*, through showMessage()) and stores the text as
        /// the default configuration name; the caller first gives every configuration of the
        /// open designs that has no name of its own the old default as its name, as Java's
        /// configurations already have it. Never empty in the application.
        std::function<void(QWidget* dialog, const QString& name)> saveAsDefault;
    };

    struct RenameConfigurationAnswer
    {
        RenameAction action = RenameAction::CANCEL;
        /// The text field's text, for RENAME.
        QString name;
    };

    // ---------------------------------------------------------- the motor chooser (M3)

    /// The motor chosen in MotorChooserDialog ("Select a rocket motor") with OK: what
    /// MotorConfigurationPanel.selectMotor() writes into every selected cell that differs.
    struct MotorChoice
    {
        /// Never null (getSelectedMotor() is null without OK, and then nothing changes).
        std::shared_ptr<const Motor> motor;
        /// The ejection charge delay in s (Motor::kPluggedDelay for "Plugged").
        double delay = 0.0;
        /// The nozzle exit diameter in m, 0 when unknown (at most the motor's diameter).
        double nozzleExitDiameter = 0.0;
    };

    // ------------------------------------------------------------ the parts library (M3)

    /// RocketComponentConfig.selectPreset(): ComponentPresetChooserDialog ("Choose component
    /// preset") for a component, described by what the dialog reads of it.
    struct PresetRequest
    {
        /// The component's kind: its preset type is presetTypeOf(componentKind) (BODY_TUBE also
        /// for an inner tube and a tube fin set), and its English name (displayName(): "Body
        /// Tube", never a translated text) names the preferences of the "Always open this
        /// dialog when creating a new %1" box ("<name>AlwaysOpenPreset") and of the table's
        /// columns ("CmpPrst.<name>..."), as OpenRocket's getComponentName() does.
        ComponentKind componentKind = ComponentKind::BODY_TUBE;
        /// "Match fore diameter" (a symmetric component with a previous symmetric component and
        /// a type with a fore diameter column): that component's aft diameter in m; nullopt
        /// without the box.
        std::optional<double> foreDiameter;
        /// "Match aft diameter" (with a next symmetric component): its fore diameter in m;
        /// nullopt without the box. For a flipped nose cone OpenRocket names this box "Match
        /// fore diameter" (flippedNoseCone) and compares with the previous component's fore
        /// radius; the caller gives the neighbour's matching diameter (a fixed OpenRocket bug).
        std::optional<double> aftDiameter;
        /// The component is a flipped nose cone: the aft-diameter box has the fore-diameter
        /// box's text and tooltip (ComponentPresetChooserDialog.checkbox.filterForeDiameter*),
        /// as in OpenRocket.
        bool flippedNoseCone = false;
    };

    /// How the parts library was left.
    enum class PresetOutcome
    {
        /// A preset to apply: Close with a row selected, or a double click on a row (not on its
        /// favourite box). The caller applies it as PresetModel.setSelectedItem() does (an undo
        /// position "Use Preset <component name>", then loadPreset()).
        CHOSEN,
        /// Close with no row selected: the table's columns are stored, nothing is applied.
        NONE,
        /// Escape or the window's close button: nothing is stored or applied.
        CLOSED,
    };

    struct PresetAnswer
    {
        PresetOutcome outcome = PresetOutcome::CLOSED;
        /// The preset, for CHOSEN; null otherwise.
        std::shared_ptr<const ComponentPreset> preset;
    };

    // ------------------------------------------------ the database load problems (M3)

    /// The database a file was to be loaded into.
    enum class DatabaseKind
    {
        /// The user's thrust-curve files (MotorDatabaseLoader, "MotorDbLoaderDlg.*").
        THRUST_CURVES,
        /// The user's component preset files (ComponentPresetDatabaseLoader,
        /// "ComponentDbLoaderDlg.*").
        COMPONENT_PRESETS,
    };

    /// A file that could not be loaded at start: it is left out of its database.
    struct DatabaseProblem
    {
        DatabaseKind          database = DatabaseKind::THRUST_CURVES;
        std::filesystem::path file;
        /// What went wrong (the loader's Error message).
        QString message;
    };

    // ------------------------------------------------------------------ the simulations (M4)

    /// The answer to "Delete simulations" (SimulationPanel.verifyDeleteSimulation()).
    struct DeleteSimulationsAnswer
    {
        /// OK: delete them (Cancel, Escape and the close button do not).
        bool confirmed = false;
        /// "Do not ask me again" was ticked. OpenRocket then turns the confirmation off
        /// (setConfirmSimDeletion(false)) whatever the answer.
        bool doNotAskAgain = false;

        [[nodiscard]] bool operator==(const DeleteSimulationsAnswer& other) const = default;
    };

    /// The simulations the run dialog runs (SimulationRunDialog(window, document,
    /// simulations...)): the document's own; the dialog's batch (util/SimulationBatch) draws
    /// their seeds (randomizeSeedIfNotFixed()), cancels the window's background estimate of them
    /// (cancelBackgroundRuns), prepares a run of each (a duplicate), submits them to the
    /// executor, shows their progress, hands each result back to its simulation as the run ends
    /// (while the simulation is still in the document), shows each failure, and tells the
    /// document once when the batch is done.
    struct RunSimulationsRequest
    {
        /// The design window the runs belong to: the executor's jobs carry it, and the window
        /// abandons them as it is destroyed (SimulationExecutor::abandonAll()); the batch
        /// abandons its own as it goes, so its callbacks may capture what the window owns. Only
        /// compared.
        const QObject* window = nullptr;
        /// The document of the simulations; never null in the application.
        OpenRocketDocument* document = nullptr;
        /// The simulations to run, in their order, at least one (Java throws for none): the
        /// document's, shared so that a simulation removed during the run is still there to be
        /// asked (and then not given the run).
        std::vector<std::shared_ptr<Simulation>> simulations;
        /// What runs them (GuiServices::simulationExecutor()); never null in the application.
        SimulationExecutor* executor = nullptr;
        /// What cancels the window's background estimate (the design view's flight estimate,
        /// figure/BackgroundSimulation) of the simulations the batch runs, called by the batch on
        /// the GUI thread once, after their seeds are drawn and before their runs are prepared,
        /// with the request's simulations (the critic's G4, O-M4-16: a background run and a run
        /// of the batch of the same simulation would both be adopted, the later one winning). May
        /// be empty: nothing is cancelled then. The window that asks for the run fills it (run
        /// 3: the Simulations tab's Run and the settings dialog's OK, from the window's
        /// RocketPanel: rocketPanel.backgroundSimulation().cancelFor(simulations)).
        std::function<void(std::span<const std::shared_ptr<Simulation>> simulations)>
            cancelBackgroundRuns;
    };

    /// How the run dialog ended.
    struct RunSimulationsAnswer
    {
        /// The batch was cancelled (Cancel, Escape or the dialog's close button: Java's
        /// cancelSimulations()), or the dialog went with its parent: the runs that had not ended
        /// keep their old results and status (OpenRocket bug 40: Java records the partial flight
        /// as up to date), the queued ones never start.
        bool cancelled = true;
        /// SimulationRunDialog.isAllSimulationsSuccessful(): every run ended without an error or
        /// an exception, and the batch was not cancelled (SimulationBatch::isAllSuccessful()).
        bool allSuccessful = false;

        [[nodiscard]] bool operator==(const RunSimulationsAnswer& other) const = default;
    };

    /// One column of a wind CSV file, as CSVImportSettingsDialog shows it in a row (Altitude,
    /// Speed, Direction, "Standard Deviation (optional)") and Preferences stores it
    /// (getMultiLevelWindCsvImport...Column(), ...ColumnIndex(), ...Unit()).
    struct WindCsvColumn
    {
        /// "Column Name", trimmed (the file has a header row); empty for no standard deviation.
        QString name;
        /// "Column Index", from 0 (the file has no header row).
        int index = 0;
        /// "Unit": the unit the file's values are in, a unit of the column's process-wide group
        /// (DISTANCE for the altitude, WINDSPEED for the speed and the standard deviation, ANGLE
        /// for the direction); never null in an answer.
        const Unit* unit = nullptr;

        [[nodiscard]] bool operator==(const WindCsvColumn& other) const = default;
    };

    /// The settings of CSVImportSettingsDialog ("Wind Data Import Settings"): how the wind
    /// editor's "Import levels" reads a CSV file. The question starts with the stored ones
    /// (Preferences' MultiLevelWindCsvImport... and, for the separator, ExportFieldSeparator:
    /// FieldSeparatorComboBox.loadFromExportPreferences()); the caller stores the chosen ones
    /// (MultiLevelWindEditDialog.importLevels(), then asks for the file) and imports with them
    /// (MultiLevelPinkNoiseWindModel::importLevelsFromCsv(): a column is read by its name with a
    /// header row, by its index without).
    struct WindCsvImportSettings
    {
        /// "CSV file has a header row".
        bool          hasHeader = true;
        WindCsvColumn altitude;
        WindCsvColumn speed;
        WindCsvColumn direction;
        WindCsvColumn standardDeviation;
        /// "Field Separator": the separator string the fields are split at ("," ";" " " "\t"
        /// or what the user typed), never the combo box's text "SPACE" or "TAB" (which Java's
        /// dialog gives and stores as the export separator: OpenRocket bug 34).
        QString separator;

        [[nodiscard]] bool operator==(const WindCsvImportSettings& other) const = default;
    };

    // ------------------------------------------------------------------ the questions

    UserPrompts()                              = default;
    virtual ~UserPrompts()                     = default;
    UserPrompts(const UserPrompts&)            = delete;
    UserPrompts& operator=(const UserPrompts&) = delete;
    UserPrompts(UserPrompts&&)                 = delete;
    UserPrompts& operator=(UserPrompts&&)      = delete;

    /// Where the new component goes (ComponentAddButtons.askPosition), with "Do not ask me
    /// again" and "You can change the default operation in the preferences.".
    [[nodiscard]] virtual InsertPositionAnswer askInsertPosition(
        QWidget* parent, InsertPositionQuestion question) = 0;

    /// Whether Cancel discards the changes (RocketComponentConfig, PreferencesDialog), with
    /// "Do not ask me again".
    [[nodiscard]] virtual DiscardAnswer askDiscardChanges(QWidget*        parent,
                                                          DiscardQuestion question) = 0;

    /// Whether to save the design named @p designName before its window closes
    /// (BasicFrame.closeAction).
    [[nodiscard]] virtual SaveChangesAnswer askSaveChanges(QWidget*       parent,
                                                           const QString& designName) = 0;

    /// Whether to overwrite the existing file named @p fileName (FileHelper.confirmWrite,
    /// "error.fileExists.*"). True for yes.
    [[nodiscard]] virtual bool askOverwrite(QWidget* parent, const QString& fileName) = 0;

    /// The storage options before a save whose options are not set explicitly
    /// (StorageOptionChooser.verifyStorageOptions): true for all simulated data, false for the
    /// summary only, nullopt when cancelled. @p saveSimulationData is the current option.
    [[nodiscard]] virtual std::optional<bool> askStorageOptions(QWidget* parent,
                                                                bool     saveSimulationData) = 0;

    /// A yes/no question; true for yes.
    [[nodiscard]] virtual bool askYesNo(QWidget* parent, const Question& question) = 0;

    /// A decal's source file that is missing (DecalNotFoundDialog): asks whether to look for
    /// it, with @p message (the Error's text, rich text), and if so the file, starting in
    /// @p directory. nullopt when the user does not look for it or cancels the file dialog.
    [[nodiscard]] virtual std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) = 0;

    /// The files to open; nullopt when cancelled.
    [[nodiscard]] virtual std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) = 0;

    /// The file to save to; nullopt when cancelled.
    [[nodiscard]] virtual std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) = 0;

    /// The "Save Design Info" dialog before a Save As (BasicFrame.showSaveRocketInfoDialog):
    /// the rocket's name, designer, design type, kit name, revision and comment of @p document,
    /// edited in place, with "Don't show this dialog again"; @p services are the application's
    /// (the preferences the dialog stores, the materials and the questions it asks). The Save As
    /// goes on however the dialog closes (OpenRocket's, the user's decision R3-3).
    virtual void editDesignInformation(QWidget* parent, const GuiServices& services,
                                       OpenRocketDocument& document) = 0;

    /// The ignition of a motor mount in a flight configuration (IgnitionSelectionDialog): the
    /// scope, event and delay chosen with OK (also Escape, as OpenRocket's Escape clicks OK), or
    /// nullopt for Cancel and the window's close button (which change nothing, where
    /// OpenRocket's Cancel marks the ignition overridden: a fixed bug).
    [[nodiscard]] virtual std::optional<IgnitionChoice> askIgnition(
        QWidget* parent, const IgnitionRequest& request) = 0;

    /// The deployment of a recovery device in a flight configuration
    /// (DeploymentSelectionDialog): the choice with OK (also Escape), or nullopt for Cancel and
    /// the close button.
    [[nodiscard]] virtual std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) = 0;

    /// The separation of a stage in a flight configuration (SeparationSelectionDialog): the
    /// choice with OK (also Escape), or nullopt for Cancel and the close button (which change
    /// nothing, where OpenRocket's keep the edits of an existing override: a fixed bug).
    [[nodiscard]] virtual std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) = 0;

    /// The new name of a flight configuration (RenameConfigDialog, "Rename Configuration"),
    /// with "Save as default" through the request's callback; Escape and the close button
    /// cancel.
    [[nodiscard]] virtual RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) = 0;

    /// A motor for @p mount in the flight configuration @p configurationId (MotorChooserDialog
    /// after setMotorMountAndConfig(fcid, mount); open()): the motor, delay and nozzle exit
    /// diameter chosen with OK, or nullopt for OK without a motor selected, Cancel, Escape and
    /// the close button (which cancels, also after an earlier OK: a fixed OpenRocket bug). The
    /// chooser lists the motors of @p services (motors()), reads the mount while it is open (its
    /// configuration's motor, delay and nozzle, its size, the motors it uses) and stores its own
    /// preferences (the filters, the preferred thrust curve of a set); it changes no component.
    /// OpenRocket keeps one chooser per window, whose search and filters survive between
    /// questions; so does the application (one per top-level window of @p parent).
    [[nodiscard]] virtual std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) = 0;

    /// A preset for the component @p request describes (the parts library,
    /// ComponentPresetChooserDialog): it lists the presets of @p services (presets()) of the
    /// component's type and stores its own preferences (the favourites ticked in it, the match
    /// boxes, "Always open this dialog...", the table's columns on Close). The caller re-reads
    /// the favourites afterwards (its preset combo box) and applies a CHOSEN preset.
    [[nodiscard]] virtual PresetAnswer choosePreset(QWidget* parent, const GuiServices& services,
                                                    const PresetRequest& request) = 0;

    /// Whether to delete the @p count selected simulations (SimulationPanel.
    /// verifyDeleteSimulation(): "Delete simulations", "Delete the selected simulations?", "Do
    /// not ask me again" and, smaller, "You can change the default operation in the
    /// preferences.", with OK and Cancel). The caller asks only while the confirmation is on
    /// (Preferences::getConfirmSimDeletion()) and turns it off when the box was ticked.
    [[nodiscard]] virtual DeleteSimulationsAnswer askDeleteSimulations(QWidget*    parent,
                                                                       std::size_t count) = 0;

    /// Runs the simulations of @p request (SimulationRunDialog, "Running simulations...", modal
    /// to the application, with its progress and Cancel) and returns when they are all done or
    /// the batch was cancelled.
    [[nodiscard]] virtual RunSimulationsAnswer runSimulations(
        QWidget* parent, const RunSimulationsRequest& request) = 0;

    /// The settings of a wind CSV import (CSVImportSettingsDialog), starting with @p initial:
    /// those chosen with OK, or nullopt for Cancel, Escape and the close button.
    [[nodiscard]] virtual std::optional<WindCsvImportSettings> askWindCsvImportSettings(
        QWidget* parent, const WindCsvImportSettings& initial) = 0;

    /// A colour (ColorChooserButton's colour dialog, JColorChooser.createDialog(), titled
    /// @p title: OpenRocket's literal "Select color"), starting with @p initial: the colour chosen
    /// with OK, or nullopt for Cancel and the close button (Java's chooser then puts the initial
    /// colour back).
    [[nodiscard]] virtual std::optional<QColor> askColor(QWidget* parent, const QColor& initial,
                                                         const QString& title) = 0;

    // ------------------------------------------------------------------ the messages

    virtual void showMessage(QWidget* parent, const Message& message) = 0;

    virtual void showWarnings(QWidget* parent, const WarningsMessage& message) = 0;

    /// OpenRocket's ErrorWarningDialog. An implementation without a dialog of its own for it
    /// shows the errors and then the warnings as warnings (showWarnings()): the default.
    virtual void showErrorsAndWarnings(QWidget* parent, const ErrorsAndWarningsMessage& message)
    {
        showWarnings(parent, {.title    = message.title,
                              .lines    = message.lines,
                              .warnings = message.errors + message.warnings});
    }

    /// The files the start could not load into the motor and preset databases (@p problems, at
    /// least one), shown once after the first window, with the texts of OpenRocket's warnings
    /// (MotorDbLoaderDlg.* and ComponentDbLoaderDlg.*: "Thrust curve '<file>' will be ignored
    /// during import.", ...). OpenRocket shows a modeless warning per file from its loader
    /// thread; the application one message for all (O-M3-21).
    virtual void showDatabaseProblems(QWidget*                         parent,
                                      std::span<const DatabaseProblem> problems) = 0;

    /// The internal error dialog (OpenRocket's SwingExceptionHandler.showDialog(), in QtRocket's
    /// words: dialogs/InternalErrorDialog) for a failure the error hook (Guard) was given, with
    /// Continue and Quit. The application's error hook shows it (app/ApplicationSession) and
    /// acts on the answer.
    [[nodiscard]] virtual InternalErrorAnswer showInternalError(QWidget*              parent,
                                                                const Guard::Failure& failure) = 0;
};

}  // namespace QtRocket::Gui
