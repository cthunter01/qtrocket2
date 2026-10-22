#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"

#include <cstddef>
#include <memory>
#include <span>
#include <utility>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/CommonStrings.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using DeployEvent = DeploymentConfiguration::DeployEvent;

/// @p component as a RecoveryDevice.
/// @throws BugError when it is none
[[nodiscard]] const RecoveryDevice& deviceOf(const RocketComponent& component)
{
    const auto* device = dynamic_cast<const RecoveryDevice*>(&component);
    if (device == nullptr)
    {
        bug("The configuration of a recovery device needs a recovery device");
    }
    return *device;
}

/// The default deployment configuration of @p component, a RecoveryDevice.
[[nodiscard]] const DeploymentConfiguration& defaultDeployment(const RocketComponent& component)
{
    return deviceOf(component).getDeploymentConfigurations().getDefault();
}

/// Calls @p change on the default deployment configuration of @p device and fires EVENT_CHANGE
/// on the device when the configuration changed (see RecoveryDeviceConfig).
template <class Change>
void changeDefaultDeployment(RecoveryDevice& device, const Change& change)
{
    DeploymentConfiguration&      configuration = device.getDeploymentConfigurations().getDefault();
    const DeploymentConfiguration before        = configuration;
    change(configuration);
    if (configuration != before)
    {
        device.fireComponentChangeEvent(ComponentChangeEvent::kEventChange);
    }
}

/// The selection of @p panel, which must be valid and edit a recovery device.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& deviceSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a recovery device needs a valid selection");
    }
    static_cast<void>(deviceOf(selection->getMain()));
    return selection;
}

}  // namespace

EnumSource<DeploymentConfiguration::DeployEvent> RecoveryDeviceConfig::deployEventSource()
{
    return EnumSource<DeployEvent>::of<RecoveryDevice>({
        .get =
            [](const RecoveryDevice& device) { return defaultDeployment(device).getDeployEvent(); },
        .set =
            [](RecoveryDevice& device, const DeployEvent& event) {
                changeDefaultDeployment(device, [event](DeploymentConfiguration& configuration) {
                    configuration.setDeployEvent(event);
                });
            },
    });
}

DoubleSource RecoveryDeviceConfig::deployDelaySource()
{
    return DoubleSource::of<RecoveryDevice>({
        .get =
            [](const RecoveryDevice& device) { return defaultDeployment(device).getDeployDelay(); },
        .set =
            [](RecoveryDevice& device, const double& delay) {
                changeDefaultDeployment(device, [delay](DeploymentConfiguration& configuration) {
                    configuration.setDeployDelay(delay);
                });
            },
    });
}

DoubleSource RecoveryDeviceConfig::deployAltitudeSource()
{
    return DoubleSource::of<RecoveryDevice>({
        .get =
            [](const RecoveryDevice& device) {
                return defaultDeployment(device).getDeployAltitude();
            },
        .set =
            [](RecoveryDevice& device, const double& altitude) {
                changeDefaultDeployment(device, [altitude](DeploymentConfiguration& configuration) {
                    configuration.setDeployAltitude(altitude);
                });
            },
    });
}

DoubleSource RecoveryDeviceConfig::cdSource()
{
    return DoubleSource::of<RecoveryDevice>({
        .get          = [](const RecoveryDevice& device) { return device.getCD(); },
        .set          = [](RecoveryDevice& device, const double& cd) { device.setCD(cd); },
        .isAutomatic  = [](const RecoveryDevice& device) { return device.isCDAutomatic(); },
        .setAutomatic = [](RecoveryDevice& device,
                           bool            automatic) { device.setCDAutomatic(automatic); },
    });
}

bool RecoveryDeviceConfig::isInBottomStage(const RocketComponent& component)
{
    const std::size_t count = component.getRocket().getStageCount();
    return static_cast<std::size_t>(component.getStageNumber()) + 1 == count;
}

RecoveryDeviceConfig::RecoveryDeviceConfig(OpenRocketDocument&                 document,
                                           const GuiServices&                  services,
                                           std::shared_ptr<ComponentSelection> selection,
                                           QWidget*                            parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
    static_cast<void>(deviceSelection(*this));
}

bool RecoveryDeviceConfig::isAltitudeEnabled() const
{
    const std::shared_ptr<ComponentSelection>& valid = selection();
    if (valid == nullptr || !valid->isValid())
    {
        return false;
    }
    return defaultDeployment(valid->getMain()).getDeployEvent() == DeployEvent::ALTITUDE;
}

void RecoveryDeviceConfig::updateFields()
{
    RocketComponentConfig::updateFields();
    const ConfigFields::NumberWidgets& altitude = m_deployment.altitude;
    if (altitude.spinBox == nullptr)
    {
        return;  // before makeDeploymentPanel()
    }
    const bool enabled = isAltitudeEnabled();
    altitude.label->setEnabled(enabled);
    altitude.spinBox->setFieldEnabled(enabled);
    altitude.unitSelector->setFieldEnabled(enabled);
    altitude.slider->setFieldEnabled(enabled);
}

QGroupBox& RecoveryDeviceConfig::makeDeploymentPanel(QWidget& parent)
{
    if (m_deployment.group != nullptr)
    {
        bug("The deployment panel of a recovery device is made once");
    }
    const std::shared_ptr<ComponentSelection>& selection = deviceSelection(*this);
    const RocketComponent&                     main      = selection->getMain();
    BindingScope&                              bindings  = scope();

    //: ParachuteCfg.lbl.Deployment (StreamerCfg.lbl.Deployment)
    m_deployment.group = makeChild<QGroupBox>(&parent, tr("Deployment"));
    auto* grid         = makeChild<QGridLayout>(m_deployment.group);

    // The event; the bottom stage has no lower stage to separate from.
    //: ParachuteCfg.lbl.Deploysat (StreamerCfg.lbl.Deploysat)
    grid->addWidget(makeChild<QLabel>(m_deployment.group, tr("Deploys at:") + QLatin1Char(' ') +
                                                              CommonStrings::dagger()),
                    0, 0);
    const auto event = bindings.make<EnumBinding<DeployEvent>>(
        selection, deployEventSource(),
        std::span<const DeployEvent>(DeploymentConfiguration::kAllDeployEvents));
    if (isInBottomStage(main))
    {
        event->removeElement(DeployEvent::LOWER_STAGE_SEPARATION);
    }
    m_deployment.event = makeChild<EnumComboBox>(m_deployment.group, event);
    m_deployment.event->setObjectName(QStringLiteral("DeployEvent"));
    grid->addWidget(m_deployment.event, 0, 1, 1, 3);
    // Java's item listener: the altitude follows the event (disconnected with this part of the
    // panel).
    m_eventConnection = event->addChangeListener(
        [this] { Guard::run([this] { updateFields(); }, QStringLiteral("DeployEvent")); });

    // "plus <delay> seconds", the delay without a unit, at least 0.
    //: ParachuteCfg.lbl.plusdelay (StreamerCfg.lbl.plusdelay)
    auto* plus = makeChild<QLabel>(m_deployment.group, tr("plus"));
    plus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(plus, 1, 0);
    auto* delayRow    = makeChild<QWidget>(m_deployment.group);
    auto* delayLayout = makeChild<QHBoxLayout>(delayRow);
    delayLayout->setContentsMargins(0, 0, 0, 0);
    m_deployment.delay = makeChild<UnitSpinBox>(
        delayRow,
        bindings.make<DoubleBinding>(selection, deployDelaySource(), unitGroup(UnitGroupId::NONE),
                                     DoubleBinding::Options{.min = 0.0}));
    m_deployment.delay->setObjectName(QStringLiteral("DeployDelay"));
    delayLayout->addWidget(m_deployment.delay);
    //: ParachuteCfg.lbl.seconds (StreamerCfg.lbl.seconds)
    delayLayout->addWidget(makeChild<QLabel>(delayRow, tr("seconds")));
    delayLayout->addStretch(1);
    grid->addWidget(delayRow, 1, 1, 1, 3);

    // The altitude, at least 0 (Java: the dagger without a space).
    const auto altitude   = bindings.make<DoubleBinding>(selection, deployAltitudeSource(),
                                                         unitGroup(UnitGroupId::DISTANCE),
                                                         DoubleBinding::Options{.min = 0.0});
    m_deployment.altitude = ConfigFields::addNumberRow(
        *grid, 2,
        //: ParachuteCfg.lbl.Altitude (StreamerCfg.lbl.Altitude)
        tr("Altitude:") + CommonStrings::dagger(), altitude, QStringLiteral("DeployAltitude"),
        {.slider = SliderMapping::linear(100, 1000)});

    m_deployment.note =
        makeChild<StyledLabel>(m_deployment.group, CommonStrings::overrideDescription(), -1.0);
    grid->addWidget(m_deployment.note, 3, 0, 1, 4);

    // The drogue flag, read-only: the stage's Recovery tab sets it.
    //: RecoveryDeviceCfg.checkbox.IsDrogue
    m_deployment.drogue = makeChild<QCheckBox>(m_deployment.group, tr("This is a drogue"));
    m_deployment.drogue->setObjectName(QStringLiteral("Drogue"));
    //: RecoveryDeviceCfg.checkbox.IsDrogue.managedByStage.ttip
    m_deployment.drogue->setToolTip(
        tr("<html>The drogue designation is managed from the stage Recovery configuration tab."));
    m_deployment.drogue->setChecked(deviceOf(main).isDrogue());
    m_deployment.drogue->setEnabled(false);
    grid->addWidget(m_deployment.drogue, 4, 0, 1, 4);
    m_deployment.drogueNote = makeChild<StyledLabel>(
        m_deployment.group,
        //: RecoveryDeviceCfg.lbl.IsDrogue.managedByStage
        tr("Set from the stage Recovery tab."), -1.0, StyledLabel::Style::ITALIC);
    grid->addWidget(m_deployment.drogueNote, 5, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    return *m_deployment.group;
}

}  // namespace QtRocket::Gui
