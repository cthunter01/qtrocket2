#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/util/Signal.h"

class QCheckBox;
class QGroupBox;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class EnumComboBox;
class GuiServices;
class StyledLabel;
class UnitSpinBox;

/// The base of the configurations of the recovery devices, ParachuteConfig and StreamerConfig
/// (OpenRocket's RecoveryDeviceConfig, swing gui/configdialog/RecoveryDeviceConfig.java): what
/// both show of a device, the group "Deployment" of its DEFAULT deployment configuration
/// (RecoveryDevice::getDeploymentConfigurations().getDefault(); each Java panel writes it out
/// with its own keys and the same texts), and the rule that enables the altitude.
///
/// The group "Deployment" (makeDeploymentPanel()):
/// - "Deploys at: <dagger>" and the combo box of the deployment event (Java: EnumModel(
///   deploymentConfig, "DeployEvent")); on a device of the bottom stage (isInBottomStage(): the
///   main component's stage number + 1 is the rocket's stage count) without "Lower stage
///   separation", which cannot happen there (Java removes the item; a device that has it anyway
///   shows it apart, EnumComboBox);
/// - "plus <delay> seconds" (Java: DoubleModel(deploymentConfig, "DeployDelay", 0): no unit, at
///   least 0, no slider);
/// - "Altitude:<dagger>" (Java: DoubleModel(deploymentConfig, "DeployAltitude", UNITS_DISTANCE,
///   0): at least 0, slider 100 m to 1000 m), its label, spin box, unit selector and slider
///   enabled only while the main component's event is "Specific altitude during descent"
///   (updateFields(), which the event's change calls as Java's item listener does);
/// - the dagger's note ("This parameter can be overridden in each flight configuration.");
/// - "This is a drogue", read-only: checked as the main component is when the panel is made,
///   always disabled, with the italic note "Set from the stage Recovery tab." (the stage's
///   Recovery tab sets it, AxialStageConfig).
///
/// Every edit of the default deployment reaches each selected device's own default
/// configuration (the user's decision U3; Java links the default configurations of the selected
/// devices as config listeners, RecoveryDevice.addConfigListener()) and fires EVENT_CHANGE on that
/// device when its configuration changed, as the core prescribes
/// (rocket/DeploymentConfiguration.h). Deviation from OpenRocket (the user's decision U4): Java's
/// DeploymentConfiguration setters notify nobody, so an edit leaves the dialog unmodified and the
/// document saved, and Cancel cannot undo it (verified: the rocket's modification id stays); here
/// the event marks the dialog modified and the document changed, and Cancel undoes the edit with
/// the session's undo step.
class RecoveryDeviceConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The widgets of the group "Deployment"; null before makeDeploymentPanel().
    struct DeploymentWidgets
    {
        QGroupBox*                  group = nullptr;
        EnumComboBox*               event = nullptr;
        UnitSpinBox*                delay = nullptr;
        ConfigFields::NumberWidgets altitude;
        StyledLabel*                note       = nullptr;
        QCheckBox*                  drogue     = nullptr;
        StyledLabel*                drogueNote = nullptr;
    };

    /// The configuration of the recovery devices of @p selection, without tabs of its own yet
    /// (the subclasses add them).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         RecoveryDevice
    RecoveryDeviceConfig(OpenRocketDocument& document, const GuiServices& services,
                         std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const DeploymentWidgets& deploymentWidgets() const noexcept
    {
        return m_deployment;
    }

    /// Whether the altitude's widgets are enabled: the main component's default deployment event
    /// is ALTITUDE (Java: RecoveryDeviceConfig.updateFields()). False once the selection is
    /// invalid.
    [[nodiscard]] bool isAltitudeEnabled() const;

    /// The base class's refresh, then the altitude's widgets enabled or not (isAltitudeEnabled()).
    void updateFields() override;

    /// The event of a device's default deployment (Java: EnumModel(getDeploymentConfigurations()
    /// .getDefault(), "DeployEvent")); see the class comment for the event it fires.
    [[nodiscard]] static EnumSource<DeploymentConfiguration::DeployEvent> deployEventSource();
    /// The delay of a device's default deployment, in s (Java: DoubleModel(..., "DeployDelay")).
    [[nodiscard]] static DoubleSource deployDelaySource();
    /// The altitude of a device's default deployment, in m (Java: DoubleModel(...,
    /// "DeployAltitude")).
    [[nodiscard]] static DoubleSource deployAltitudeSource();
    /// The drag coefficient of a recovery device, with its automatic state (Java: DoubleModel(
    /// component, "CD"), isCDAutomatic() and setCDAutomatic()).
    [[nodiscard]] static DoubleSource cdSource();

    /// Whether @p component is in the rocket's bottom stage (Java: (component.getStageNumber() +
    /// 1) == rocket.getStageCount(); a booster set counts as a stage of its own).
    /// @throws BugError when @p component is not in a Rocket
    [[nodiscard]] static bool isInBottomStage(const RocketComponent& component);

protected:
    /// Makes the group "Deployment" (see the class comment) as a child of @p parent; the caller
    /// lays it out. Call it once.
    /// @throws BugError when called a second time
    QGroupBox& makeDeploymentPanel(QWidget& parent);

private:
    DeploymentWidgets          m_deployment;
    Signal<>::ScopedConnection m_eventConnection;
};

}  // namespace QtRocket::Gui
