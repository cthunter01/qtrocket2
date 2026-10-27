#pragma once

#include <QCoreApplication>
#include <QString>

namespace QtRocket
{
class AxialStage;
class DeploymentConfiguration;
class FlightConfigurationId;
class MotorMount;
class Preferences;
class RecoveryDevice;
class Rocket;
class RocketComponent;
class StageSeparationConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The texts of the cells of the "Motors & Configuration" tab's three tables, as OpenRocket's cell
/// renderers write them: FlightConfigurablePanel.FlightConfigurableCellRenderer (column 0, the
/// configuration's name), MotorConfigurationPanel.MotorTableCellRenderer (a motor mount's cell:
/// two labels, the motor and its ignition), RecoveryConfigurationPanel.RecoveryTableCellRenderer
/// (a recovery device's deployment) and SeparationConfigurationPanel.SeparationTableCellRenderer
/// (a stage's separation), and the headers of FlightConfigurableTableModel. The texts are
/// messages.properties' through tr() and the core's enum texts (enumText(), and the short event
/// names through translatedEnumText()); the delays and the altitude are in the default units of
/// the short time and the distance (UnitGroup's toStringUnit()).
///
/// The style of a cell: Java shades a text (italic, UITheme's TEXT_DIM) that shows a default value,
/// and writes the others regular. A motor cell's ignition is shaded when its configuration has no
/// ignition override of its own (ignition().usesDefault, Java's rule). A recovery or stage cell is
/// shaded when the configuration has no override of the device's deployment or the stage's
/// separation (deploymentIsDefault(), separationIsDefault(): !containsId(id)).
///
/// Deviation from OpenRocket (plan 10.3, OpenRocket bug 1 fixed): Java shades a recovery or stage
/// cell when the set's isDefault(id) holds, which compares the value objects by identity and is
/// false for an id without an override (its get(id) gives the default object, which is not the
/// null its map holds), so that those cells are never shaded (0 of the 135 recovery and stage cells
/// of the 16 examples and the 13 test rockets, while 124 of them show the default).
/// FlightConfigurableParameterSet::isDefault() stays Java-exact in the core.
class FlightConfigurationCellText
{
    Q_DECLARE_TR_FUNCTIONS(FlightConfigurationCellText)

public:
    FlightConfigurationCellText() = delete;

    /// The ignition label of a motor cell: its text and whether it is shaded.
    struct Ignition
    {
        /// "<event>" or "<event> + <delay>", inside "Default (...)" when usesDefault.
        QString text;
        /// Whether the configuration has no ignition override (the default's event and delay).
        bool usesDefault = false;
    };

    /// The header of column 0: "Configuration" (edtmotorconfdlg.col.configuration).
    [[nodiscard]] static QString configurationHeader();

    /// The header of a component's column: its name (Java: component.toString()).
    [[nodiscard]] static QString componentHeader(const RocketComponent& component);

    /// Column 0: the name of @p rocket's configuration @p id, its {motors}, {manufacturers} and
    /// {cases} substituted (RocketDescriptor.format(rocket, id), FlightConfiguration::getName()
    /// with @p preferences, which choose the default name and between the motors' designation and
    /// common name). The default's name for an id the rocket does not have.
    [[nodiscard]] static QString configurationName(const Rocket&                rocket,
                                                   const FlightConfigurationId& id,
                                                   const Preferences&           preferences);

    /// The motor label of @p mount's cell for @p id (getMotorSpecification() of
    /// getMotorConfig(id), the mount's default configuration when it has none for @p id): "None"
    /// (edtmotorconfdlg.tbl.None) without a motor, else the motor's name with the ejection delay
    /// (Motor::getMotorName(), designation or common name by @p preferences), "<n>" U+00D7 " "
    /// before it when the configuration's mount holds more than one motor counting the copies of
    /// its assemblies (getMotorCountIncludingAssemblyCopies()): "2× A10-0". Never shaded.
    [[nodiscard]] static QString motor(const MotorMount& mount, const FlightConfigurationId& id,
                                       const Preferences& preferences);

    /// The ignition label of @p mount's cell for @p id (getIgnitionEventString()): the event's
    /// short name (MotorMount.IgnitionEvent.short.<NAME>), with " + <delay>" when the event is not
    /// NEVER and the delay exceeds 0.001 s; when getMotorConfig(id) has no ignition override, the
    /// event and the delay of the mount's default configuration inside "Default ({0})"
    /// (MotorConfigurationTableModel.table.ignition.default), shaded.
    [[nodiscard]] static Ignition ignition(const MotorMount&            mount,
                                           const FlightConfigurationId& id);

    /// A recovery cell's text (getDeploymentSpecification()): the event's short name
    /// (RecoveryDevice.DeployEvent.short.<NAME>), " <altitude>" for ALTITUDE, " + <delay>" when
    /// the delay exceeds 0.001 s: "Altitude 150 m + 1.5 s".
    [[nodiscard]] static QString deployment(const DeploymentConfiguration& configuration);

    /// A stage cell's text (getSeparationSpecification()): the event's name
    /// (Stage.SeparationEvent.<NAME>, SeparationEvent.toString()), " + <delay>" when the delay
    /// exceeds 0.001 s.
    [[nodiscard]] static QString separation(const StageSeparationConfiguration& configuration);

    /// Whether @p device's cell for @p id shows its default deployment, shaded: the set has no
    /// override for @p id (!containsId(id); see the class comment for Java's isDefault()).
    [[nodiscard]] static bool deploymentIsDefault(const RecoveryDevice&        device,
                                                  const FlightConfigurationId& id);

    /// Whether @p stage's cell for @p id shows its default separation, shaded (as
    /// deploymentIsDefault()).
    [[nodiscard]] static bool separationIsDefault(const AxialStage&            stage,
                                                  const FlightConfigurationId& id);
};

}  // namespace QtRocket::Gui
