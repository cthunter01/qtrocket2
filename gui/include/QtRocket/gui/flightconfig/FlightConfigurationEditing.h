#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class AxialStage;
class MotorMount;
class OpenRocketDocument;
class Preferences;
class RecoveryDevice;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// Every operation of the "Motors & Configuration" tab on a document (the operation half of
/// OpenRocket's FlightConfigurationPanel, MotorConfigurationPanel, RecoveryConfigurationPanel,
/// SeparationConfigurationPanel and MotorMountTableModel.setValueAt()): New, Duplicate, Rename
/// and Delete configuration; Select motor, Delete motor(s), Select and Reset ignition; Select and
/// Reset deployment; Select and Reset separation; the motor mount check box. The panels (and
/// their buttons, popup items, keys and double clicks) call them with their selection; this class
/// holds no table, no selection and no component pointer.
///
/// THE CELLS. An operation of a sub-panel takes the configuration ids of the selected rows (in
/// row order, each once: getSelectedConfigurationIds()) and the component ids of the selected
/// columns (in column order, each once: getSelectedComponents()), and acts on every row x column
/// pair; the first pair is the one a question is asked about. The configuration operations take
/// the ids alone. With no id or no component an operation does nothing and returns false or
/// nothing, as Java's do. A component id must name a component of the operation's kind in the
/// rocket (a MotorMount, a RecoveryDevice, an AxialStage), and Select motor refuses the default
/// configuration's id (Java's IllegalStateException): both are BugErrors. After a question (a
/// modal dialog, whose event loop may deliver a TREE or UNDO change) the components are looked up
/// again by id, and one that has gone is left out.
///
/// THE QUESTIONS go through the services' UserPrompts with the dialog parent: askIgnition(),
/// askDeployment(), askSeparation() (each with the request ignitionRequest(),
/// deploymentRequest() or separationRequest() makes for the first cell), askRenameConfiguration()
/// and chooseMotor(). Each answer is applied as the dialog's OK action and then the panel's loop
/// over the other cells apply it in Java; Cancel (nullopt) changes nothing.
///
/// UNDO. Each operation that changes the design first asks for the component configuration
/// dialog to close (closeConfigDialogRequested(): its Cancel undoes once, which would undo the
/// tab's step), then adds its undo position, before its question: "Select motor", "Delete
/// motor(s)", "Select ignition", "Reset ignition", "Select deployment", "Reset deployment",
/// "Select separation", "Reset separation", "Rename configuration(s)" and "Remove
/// configuration(s)" (Java's literal English, which messages.properties does not have); "New
/// Configuration", "Duplicate Configuration" and "Modify motor mount" are QtRocket's (bug 5,
/// below). Never call an operation from a slot of the rocket's change events: it adds an undo
/// position.
///
/// EVENTS. Each operation fires one change event of the rocket of its own (Java's panels fire one
/// per panel for New, Duplicate, Rename and Delete: FlightConfigurationPanel.configurationChanged()
/// calls the three panels' fireTableDataChanged()), only when it changed something, naming the
/// configurations it changed: MOTOR_CHANGE for the motor operations, AERODYNAMIC_CHANGE for the
/// deployment, AEROMASS_CHANGE for the separation, NONFUNCTIONAL_CHANGE for Rename and Delete.
/// An answer for every configuration (the "Change all configurations using the default ..."
/// radio button) names all of them (no ids), because the default reaches every configuration
/// without an override, and QtRocket's FlightConfiguration keeps copies of the motor
/// configurations that an event refreshes only for the ids it names (Rocket.cpp; Java's
/// configurations share the objects and Java names only the selected rows). The core's own events
/// come as in Java: setFlightConfiguration(), setSelectedConfiguration() and
/// removeFlightConfiguration() fire NONFUNCTIONAL_CHANGE, setMotorMount() MOTOR_CHANGE on the
/// mount.
///
/// The operations, in Java's order:
/// - newConfiguration() (FlightConfigurationPanel.newOrDuplicateConfigAction(false)): a new
///   configuration with a new random id and the default name, stored and selected, a new
///   Simulation for it (named getNextSimulationName(), with the preferences' default options,
///   as Java's: the simulations tab comes with Milestone 4), MOTOR_CHANGE for its id; then the
///   caller's follow-up (Java: the current sub-tab's Select motor, Select deployment or Select
///   separation over the new configuration's row, which the table has then selected in the
///   current column, or the last one), then NONFUNCTIONAL_CHANGE for all (Java's "trigger
///   select").
/// - duplicateConfigurations() (newOrDuplicateConfigAction(true)): for each id a copy with a new
///   random id (FlightConfiguration::copy(), which copies the motors into their mounts) and
///   copyFlightConfiguration() on every FlightConfigurableComponent of the rocket (also on the
///   mounts that do not act as one), stored and selected, a Simulation for each; the first copy
///   selected again; MOTOR_CHANGE for the copies' ids, then NONFUNCTIONAL_CHANGE for all.
/// - renameConfigurations() (renameConfigurationAction()): the undo position before the question
///   (also when it is cancelled, as Java's), RenameConfigDialog over the first id; RENAME sets its
///   name to the text (empty: the default name), RESET restores the default name; then every
///   other id whose raw name differs from the first one's new raw name takes it;
///   NONFUNCTIONAL_CHANGE when a name changed. "Save as default" (the request's callback): pins the
///   configuration names of every open document (OpenRocketDocument::pinFlightConfigurationNames(),
///   O-M3-7: setOpenDocuments()), shows "This change will only affect new flight configurations."
///   over the dialog, then stores the text as the preferences' default configuration name, so that
///   only new configurations take it, as in Java (whose configurations store the default name
///   they were made with).
/// - removeConfigurations() (removeConfigurationAction()): each configuration with its
///   simulations (OpenRocketDocument::removeFlightConfigurationAndSimulations(): the rocket
///   selects the default configuration when the selected one goes), then NONFUNCTIONAL_CHANGE.
/// - selectMotor() (MotorConfigurationPanel.selectMotor()): the motor chooser for the first cell;
///   with a motor, every cell whose configuration (its own, or the mount's default) differs from
///   the choice in its motor (by identity), ejection delay or nozzle exit diameter gets a new
///   MotorConfiguration made from that one (its ignition and override flag kept) with the choice.
/// - deleteMotors() (deleteMotor()): every cell's motor configuration removed (setMotorConfig(
///   nullopt)); MOTOR_CHANGE always.
/// - selectIgnition() (selectIgnition() with IgnitionSelectionDialog): for every configuration,
///   the mount's default configuration and every configuration of the mount get the event and the
///   delay (the overridden ones too, as in Java); for this configuration only, the cell's own
///   configuration (made from the mount's default when the cell has none) gets them, through
///   the setters (so its ignition is overridden), each value only when it differs from the
///   question's (Java's dialog edits the configuration live, and a value left as it was is not
///   set). The other cells then take the event and the delay; with this configuration only, a
///   cell without a configuration of its own first gets one made from the mount's default. A
///   change, or another cell, fires MOTOR_CHANGE.
/// - resetIgnition() (resetIgnition()): MotorConfiguration::useDefaultIgnition() on each cell's
///   own configuration (AUTOMATIC, no delay, through the setters: the ignition stays overridden,
///   as in Java); a cell without a configuration of its own is left alone.
/// - selectDeployment() (selectDeployment() with DeploymentSelectionDialog): for every
///   configuration the device's default deployment (setDefault(): the overrides stay), for this
///   configuration only its override; then each other cell differing from the first cell's
///   deployment takes its event, altitude and delay, with this configuration only after an
///   override made from the device's default for a cell without one. AERODYNAMIC_CHANGE when
///   something changed.
/// - resetDeployment() (resetDeployment()): every cell's override removed; AERODYNAMIC_CHANGE
///   when a deployment changed.
/// - selectSeparation() (selectSeparation() with SeparationSelectionDialog): for every
///   configuration all of the stage's overrides are removed and the default set (as Java's), for
///   this configuration only its override; then each other cell differing from the first's takes
///   its event and delay (not its altitude, as in Java), with an override made from the stage's
///   default first as for the deployment. AEROMASS_CHANGE when something changed.
/// - resetSeparation() (resetSeparation()): every cell's override removed; AEROMASS_CHANGE when a
///   separation changed.
/// - setMotorMount() (MotorMountTableModel.setValueAt(), through the model's
///   motorMountChangeRequested()): the mount acts as a motor mount or not (its own MOTOR_CHANGE).
/// Each operation of a sub-panel returns whether it changed the design (Java's "update": the
/// panel then refreshes the tables, else it gives its table the focus).
///
/// Deviations from OpenRocket (the bugs are those of plan 10.3, the user's U-M3-5):
/// - Bug 5: New, Duplicate and the motor mount check box add an undo position ("New
///   Configuration", "Duplicate Configuration", "Modify motor mount"); Java's add none, so their
///   change becomes part of the step before (its description shows in the Edit menu) and the
///   check box's toggle is undone with the step before it.
/// - Bugs 1 and 2: the questions start on the radio button of the cell's state
///   (ignitionRequest(), deploymentRequest(), separationRequest()): "this configuration" when the
///   cell has an override of its own (an ignition: a motor configuration of its own whose
///   ignition is overridden), else "all configurations". Java's deployment and separation
///   dialogs always start on "this configuration" (FlightConfigurableParameterSet.isDefault(fcid)
///   compares by identity), its ignition dialog on "all" exactly when the cell has no motor.
/// - Bug 3: Cancel and the window's close button change nothing, and nothing reaches the other
///   selected cells (the value dialogs answer nullopt). Java's ignition Cancel
///   sets the start values back through the setters, which marks the ignition overridden (on a
///   cell without a configuration, the mount's default's), its close button keeps the live
///   edits, its separation dialog edits an existing override in place (Cancel and close keep the
///   edits), and its panels' loops copy the first cell's values into the other cells after a
///   Cancel too.
/// - Bug 4: an ignition for this configuration only on a cell without a motor configuration of
///   its own creates that configuration (from the mount's default) and never edits the default:
///   Java's dialog edits mount.getMotorConfig(fcid), the mount's default for such a cell, and its
///   loop over the other cells reads each cell's configuration before it creates the override, so
///   it writes the values into the default too (MotorConfigurationPanel.java:400-417). Reset
///   ignition leaves such a cell alone, where Java's resets the mount's default ignition. The
///   configuration created belongs to the cell's flight configuration (its id); Java's loop
///   stores a clone of the default that keeps the default's id.
/// - One event per operation (see EVENTS), all ids for the "all configurations" answers.
/// - An answer that changes the design always fires its event: each operation compares the values
///   it may change (every configuration of the mounts, devices or stages of the cells, the
///   defaults, the override flags, the separation's altitude) before and after. Java's panels fire
///   only when their "update" says so, which looks at the first cell's value (and compares
///   separations without their altitude) and at the other cells, so some changes fire nothing
///   there and miss the tables, the configurations' copies of the motors and the undo history:
///   "all configurations" for an ignition that sets the values the configurations have (their
///   override flags become true), for a deployment on a cell with an override of its own (only
///   the default changes), for a separation whose first cell equals the new default (the other
///   overrides go), and a separation whose altitude alone changes.
/// - Selection by id: the components are looked up by id before the operation (a BugError for an
///   id that is not one of the kind) and again after its question (one that went is left out; the
///   operation does nothing when the first cell's went), where Java's panels hold the objects.
class FlightConfigurationEditing final : public QObject
{
    Q_OBJECT

public:
    /// The documents whose configurations "Save as default" pins: every open design.
    using OpenDocuments = std::function<std::vector<OpenRocketDocument*>()>;
    /// What New does over the new configuration once it is selected (Java: the current
    /// sub-tab's selectMotor(), selectDeployment() or selectSeparation()).
    using NewConfigurationFollowUp = std::function<void(const FlightConfigurationId& id)>;

    /// The operations on @p document (which must outlive this object), asking through @p services
    /// (prompts(), preferences(), motors()) with @p dialogParent as the questions' parent (it may
    /// go first: the questions then have none).
    FlightConfigurationEditing(OpenRocketDocument& document, const GuiServices& services,
                               QWidget* dialogParent = nullptr, QObject* parent = nullptr);
    ~FlightConfigurationEditing() override;

    FlightConfigurationEditing(const FlightConfigurationEditing&)            = delete;
    FlightConfigurationEditing& operator=(const FlightConfigurationEditing&) = delete;
    FlightConfigurationEditing(FlightConfigurationEditing&&)                 = delete;
    FlightConfigurationEditing& operator=(FlightConfigurationEditing&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&             rocket() const noexcept;
    [[nodiscard]] const GuiServices&  services() const noexcept { return m_services; }

    /// The parent of the questions (the tab, or its window); may be null.
    [[nodiscard]] QWidget* dialogParent() const noexcept { return m_dialogParent; }
    void                   setDialogParent(QWidget* parent) noexcept { m_dialogParent = parent; }

    /// Sets the documents "Save as default" pins (the window manager's documents); an empty
    /// function (the default) pins this document alone.
    void setOpenDocuments(OpenDocuments documents);
    /// The documents "Save as default" pins now, this one among them.
    [[nodiscard]] std::vector<OpenRocketDocument*> openDocuments() const;

    // ------------------------------------------------------------------ the configurations

    /// New Configuration (see the class comment); @p followUp is called with the new id before
    /// the last event (an empty one: nothing). The new id.
    std::optional<FlightConfigurationId> newConfiguration(
        const NewConfigurationFollowUp& followUp = {});

    /// Duplicate Configuration of @p ids (see the class comment); the copies' ids, in order (none
    /// for no ids).
    std::vector<FlightConfigurationId> duplicateConfigurations(
        std::span<const FlightConfigurationId> ids);

    /// Rename Configuration of @p ids, asked for the first (see the class comment); whether a
    /// name changed.
    bool renameConfigurations(std::span<const FlightConfigurationId> ids);

    /// Delete Configuration of @p ids with their simulations; whether anything was removed (false
    /// for no ids).
    bool removeConfigurations(std::span<const FlightConfigurationId> ids);

    // ------------------------------------------------------------------ the motors

    /// Select motor for the cells @p ids x @p mounts (MotorMounts).
    /// @throws BugError for the default configuration's id or an id that is not a MotorMount's.
    bool selectMotor(std::span<const FlightConfigurationId> ids, std::span<const Uuid> mounts);
    /// Delete motor(s) of the cells @p ids x @p mounts.
    bool deleteMotors(std::span<const FlightConfigurationId> ids, std::span<const Uuid> mounts);
    /// Select ignition for the cells @p ids x @p mounts.
    bool selectIgnition(std::span<const FlightConfigurationId> ids, std::span<const Uuid> mounts);
    /// Reset ignition of the cells @p ids x @p mounts.
    bool resetIgnition(std::span<const FlightConfigurationId> ids, std::span<const Uuid> mounts);

    // ------------------------------------------------------------------ recovery and stages

    /// Select deployment for the cells @p ids x @p devices (RecoveryDevices).
    bool selectDeployment(std::span<const FlightConfigurationId> ids,
                          std::span<const Uuid>                  devices);
    /// Reset deployment of the cells @p ids x @p devices.
    bool resetDeployment(std::span<const FlightConfigurationId> ids, std::span<const Uuid> devices);
    /// Select separation for the cells @p ids x @p stages (AxialStages).
    bool selectSeparation(std::span<const FlightConfigurationId> ids, std::span<const Uuid> stages);
    /// Reset separation of the cells @p ids x @p stages.
    bool resetSeparation(std::span<const FlightConfigurationId> ids, std::span<const Uuid> stages);

    // ------------------------------------------------------------------ the motor mounts

    /// Makes the component @p mount (a MotorMount) act as a motor mount or not, with its own
    /// undo step (bug 5); false, with nothing done, when it already does as asked.
    /// @throws BugError when @p mount is not a MotorMount's id.
    bool setMotorMount(const Uuid& mount, bool motorMount);

    // ------------------------------------------------------------------ the questions

    /// What IgnitionSelectionDialog is asked for the cell (@p mount, @p id) of @p rocket: the
    /// configuration's name, the cell's ignition event and delay (its own configuration's, else
    /// the mount's default's), and "this configuration" exactly when the cell has a motor
    /// configuration of its own with an ignition override (bug 2).
    [[nodiscard]] static UserPrompts::IgnitionRequest ignitionRequest(
        const Rocket& rocket, const MotorMount& mount, const FlightConfigurationId& id,
        const Preferences& preferences);
    /// What DeploymentSelectionDialog is asked for the cell (@p device, @p id): the name, the
    /// cell's deployment (its override, else the default), "this configuration" exactly when the
    /// device has an override for @p id (bug 1), and whether the device is in the bottom stage.
    [[nodiscard]] static UserPrompts::DeploymentRequest deploymentRequest(
        const Rocket& rocket, const RecoveryDevice& device, const FlightConfigurationId& id,
        const Preferences& preferences);
    /// What SeparationSelectionDialog is asked for the cell (@p stage, @p id), as
    /// deploymentRequest().
    [[nodiscard]] static UserPrompts::SeparationRequest separationRequest(
        const Rocket& rocket, const AxialStage& stage, const FlightConfigurationId& id,
        const Preferences& preferences);

Q_SIGNALS:
    /// An operation is about to add an undo position: the component configuration dialog is to
    /// close first (MainWindow::requestConfigDialogClose()).
    void closeConfigDialogRequested();

private:
    /// Closes the configuration dialog, then adds the undo position @p description.
    void startStep(const QString& description);
    /// "Save as default" of the rename question with @p name, over @p dialog.
    void saveAsDefaultName(QWidget* dialog, const QString& name) const;
    /// The selection's components of the type @p T, looked up by id; @p strict throws BugError for
    /// an id that is not one (before a question), else leaves it out (after one).
    template <typename T>
    [[nodiscard]] std::vector<T*> find(std::span<const Uuid> ids, bool strict) const;
    /// Applies the ignition answer @p choice to the first cell (@p mount, @p id) as the dialog's
    /// OK action, from the question's @p initial values.
    static void applyIgnition(MotorMount& mount, const FlightConfigurationId& id,
                              const UserPrompts::IgnitionChoice& choice,
                              const UserPrompts::IgnitionChoice& initial);
    /// The rocket's change event @p type for @p ids, or for all configurations when nullopt.
    void fire(int type, std::optional<std::span<const FlightConfigurationId>> ids) const;

    OpenRocketDocument* m_document;
    GuiServices         m_services;
    QPointer<QWidget>   m_dialogParent;
    OpenDocuments       m_openDocuments;
};

}  // namespace QtRocket::Gui
