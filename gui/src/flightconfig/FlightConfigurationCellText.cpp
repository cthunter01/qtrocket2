#include "QtRocket/gui/flightconfig/FlightConfigurationCellText.h"

#include <QString>

#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Chars.h"

namespace QtRocket::Gui
{

namespace
{

/// The delays and altitudes the renderers leave out: Java's "delay > 0.001".
constexpr double kShownDelay = 0.001;

/// " + <delay>" in the default unit of the short time (UNITS_SHORT_TIME.toStringUnit()).
QString delaySuffix(double delay)
{
    return QStringLiteral(" + ") + QtStrings::defaultUnitText(UnitGroupId::SHORT_TIME, delay);
}

}  // namespace

QString FlightConfigurationCellText::configurationHeader()
{
    //: edtmotorconfdlg.col.configuration
    return tr("Configuration");
}

QString FlightConfigurationCellText::componentHeader(const RocketComponent& component)
{
    return QtStrings::fromUtf8(component.toString());
}

QString FlightConfigurationCellText::configurationName(const Rocket&                rocket,
                                                       const FlightConfigurationId& id,
                                                       const Preferences&           preferences)
{
    return QtStrings::fromUtf8(rocket.getFlightConfiguration(id).getName(preferences));
}

QString FlightConfigurationCellText::motor(const MotorMount& mount, const FlightConfigurationId& id,
                                           const Preferences& preferences)
{
    const MotorConfiguration& configuration = mount.getMotorConfig(id);
    if (configuration.isEmpty())
    {
        //: edtmotorconfdlg.tbl.None
        return tr("None");
    }
    QString text = QtStrings::fromUtf8(
        configuration.getMotor()->getMotorName(preferences, configuration.getEjectionDelay()));
    const int count = configuration.getMount().getMotorCountIncludingAssemblyCopies();
    if (count > 1)
    {
        text = QString::number(count) + QtStrings::fromUtf8(Chars::kTimes) + QStringLiteral(" ") +
               text;
    }
    return text;
}

FlightConfigurationCellText::Ignition FlightConfigurationCellText::ignition(
    const MotorMount& mount, const FlightConfigurationId& id)
{
    const MotorConfiguration& current     = mount.getMotorConfig(id);
    const bool                usesDefault = !current.hasIgnitionOverride();
    const MotorConfiguration& shown       = usesDefault ? mount.getDefaultMotorConfig() : current;
    const IgnitionEvent       event       = shown.getIgnitionEvent();
    const double              delay       = shown.getIgnitionDelay();

    QString text = translatedEnumText(shortDisplayKey(event), shortDisplayName(event));
    if (event != IgnitionEvent::NEVER && delay > kShownDelay)
    {
        text += delaySuffix(delay);
    }
    if (usesDefault)
    {
        //: MotorConfigurationTableModel.table.ignition.default
        text = tr("Default ({0})").replace(QStringLiteral("{0}"), text);
    }
    return {.text = text, .usesDefault = usesDefault};
}

QString FlightConfigurationCellText::deployment(const DeploymentConfiguration& configuration)
{
    const DeploymentConfiguration::DeployEvent event = configuration.getDeployEvent();
    QString text = translatedEnumText(shortDisplayKey(event), shortDisplayName(event));
    if (event == DeploymentConfiguration::DeployEvent::ALTITUDE)
    {
        text += QStringLiteral(" ") + QtStrings::defaultUnitText(UnitGroupId::DISTANCE,
                                                                 configuration.getDeployAltitude());
    }
    if (configuration.getDeployDelay() > kShownDelay)
    {
        text += delaySuffix(configuration.getDeployDelay());
    }
    return text;
}

QString FlightConfigurationCellText::separation(const StageSeparationConfiguration& configuration)
{
    QString text = enumText(configuration.getSeparationEvent());
    if (configuration.getSeparationDelay() > kShownDelay)
    {
        text += delaySuffix(configuration.getSeparationDelay());
    }
    return text;
}

bool FlightConfigurationCellText::deploymentIsDefault(const RecoveryDevice&        device,
                                                      const FlightConfigurationId& id)
{
    return !device.getDeploymentConfigurations().containsId(id);
}

bool FlightConfigurationCellText::separationIsDefault(const AxialStage&            stage,
                                                      const FlightConfigurationId& id)
{
    return !stage.getSeparationConfigurations().containsId(id);
}

}  // namespace QtRocket::Gui
