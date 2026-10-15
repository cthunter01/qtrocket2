#include "QtRocket/file/openrocket/InternalComponentSaver.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/RocketComponentSaver.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Elements = InternalComponentSaver::Elements;
using Saver    = RocketComponentSaver;

/// Java's StructuralComponentSaver.addParams() and RingComponentSaver.addParams() (the
/// InternalComponentSaver between them adds nothing).
void addRingParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    Saver::addParams(c, writer, elements);

    elements.push_back(
        Saver::materialParam(Saver::as<StructuralComponent>(c).getMaterial(), writer));

    const auto& ring = Saver::as<RingComponent>(c);

    elements.push_back(Saver::doubleElement("length", ring.getLength()));
    elements.push_back(Saver::doubleElement("radialposition", ring.getRadialPosition()));
    // Java: ring.getRadialDirection() * 180.0 / Math.PI
    elements.push_back(
        Saver::doubleElement("radialdirection", MathUtil::rad2deg(ring.getRadialDirection())));
}

/// Java's ThicknessRingComponentSaver.addParams().
void addThicknessRingParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    addRingParams(c, writer, elements);

    const auto& comp = Saver::as<ThicknessRingComponent>(c);
    if (comp.isOuterRadiusAutomatic())
    {
        elements.push_back(Saver::element("outerradius", "auto"));
    }
    else
    {
        elements.push_back(Saver::doubleElement("outerradius", comp.getOuterRadius()));
    }
    elements.push_back(Saver::doubleElement("thickness", comp.getThickness()));
}

/// Java's RadiusRingComponentSaver.addParams().
void addRadiusRingParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    addRingParams(c, writer, elements);

    const auto& comp = Saver::as<RadiusRingComponent>(c);
    if (comp.isOuterRadiusAutomatic())
    {
        elements.push_back(Saver::element("outerradius", "auto"));
    }
    else
    {
        elements.push_back(Saver::doubleElement("outerradius", comp.getOuterRadius()));
    }
    if (c.kind() != ComponentKind::BULKHEAD)
    {
        if (comp.isInnerRadiusAutomatic())
        {
            elements.push_back(Saver::element("innerradius", "auto"));
        }
        else
        {
            elements.push_back(Saver::doubleElement("innerradius", comp.getInnerRadius()));
        }
    }
}

/// Java's MassObjectSaver.addParams() (after InternalComponentSaver's, which adds nothing).
void addMassObjectParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    Saver::addParams(c, writer, elements);

    const auto& mass = Saver::as<MassObject>(c);

    elements.push_back(Saver::doubleElement("packedlength", mass.getLength()));
    if (mass.isRadiusAutomatic())
    {
        elements.push_back(Saver::element("packedradius",
                                          "auto " + Strings::javaDoubleToString(mass.getRadius())));
    }
    else
    {
        elements.push_back(Saver::doubleElement("packedradius", mass.getRadius()));
    }
    elements.push_back(Saver::doubleElement("radialposition", mass.getRadialPosition()));
    // Java: mass.getRadialDirection() * 180.0 / Math.PI
    elements.push_back(
        Saver::doubleElement("radialdirection", MathUtil::rad2deg(mass.getRadialDirection())));
}

/// Java's addDeploymentConfigurationParams(): the three lines of @p config, with a 2-space
/// prefix when @p indent.
void addDeploymentConfigurationParams(const DeploymentConfiguration& config, bool indent,
                                      Elements& elements)
{
    const std::string_view prefix = indent ? "  " : "";
    elements.push_back(std::string(prefix) +
                       Saver::element("deployevent", orkName(config.getDeployEvent())));
    elements.push_back(std::string(prefix) +
                       Saver::doubleElement("deployaltitude", config.getDeployAltitude()));
    elements.push_back(std::string(prefix) +
                       Saver::doubleElement("deploydelay", config.getDeployDelay()));
}

/// Java's RecoveryDeviceSaver.addParams().
void addRecoveryDeviceParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    addMassObjectParams(c, writer, elements);

    const auto& dev = Saver::as<RecoveryDevice>(c);

    if (dev.isCDAutomatic())
    {
        elements.push_back(Saver::element("cd", "auto"));
    }
    else
    {
        elements.push_back(Saver::doubleElement("cd", dev.getCD()));
    }
    elements.push_back(Saver::materialParam(dev.getMaterial(), writer));
    if (dev.isDrogue())
    {
        elements.emplace_back("<isdrogue>true</isdrogue>");
    }

    // NOTE: Default config must be BEFORE overridden config for proper backward compatibility
    // later on
    const FlightConfigurableParameterSet<DeploymentConfiguration>& configSet =
        dev.getDeploymentConfigurations();
    const DeploymentConfiguration& defaultConfig = configSet.getDefault();
    addDeploymentConfigurationParams(defaultConfig, false, elements);

    for (const FlightConfigurationId& fcid : configSet.getIds())
    {
        if (configSet.isDefault(fcid))
        {
            continue;
        }
        // only print configurations which override the default.
        const DeploymentConfiguration& deployConfig = configSet.get(fcid);
        elements.push_back(
            std::format(R"(<deploymentconfiguration configid="{}">)", fcid.toString()));
        addDeploymentConfigurationParams(deployConfig, true, elements);
        elements.emplace_back("</deploymentconfiguration>");
    }
}

}  // namespace

InternalComponentSaver::Elements InternalComponentSaver::tubeCouplerElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("tubecoupler", [&](Elements& elements) {
        addThicknessRingParams(component, writer, elements);
    });
}

InternalComponentSaver::Elements InternalComponentSaver::engineBlockElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("engineblock", [&](Elements& elements) {
        addThicknessRingParams(component, writer, elements);
    });
}

InternalComponentSaver::Elements InternalComponentSaver::innerTubeElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("innertube", [&](Elements& elements) {
        addThicknessRingParams(component, writer, elements);
        const auto& tube = Saver::as<InnerTube>(component);

        elements.push_back(
            Saver::element("clusterconfiguration", tube.getClusterConfiguration().getXmlName()));
        elements.push_back(Saver::doubleElement("clusterscale", tube.getClusterScale()));
        // Java: tube.getClusterRotation() * 180.0 / Math.PI
        elements.push_back(
            Saver::doubleElement("clusterrotation", MathUtil::rad2deg(tube.getClusterRotation())));

        if (tube.isMotorMount())
        {
            for (std::string& line : Saver::motorMountParams(tube, writer))
            {
                elements.push_back(std::move(line));
            }
        }
    });
}

InternalComponentSaver::Elements InternalComponentSaver::centeringRingElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("centeringring", [&](Elements& elements) {
        addRadiusRingParams(component, writer, elements);
    });
}

InternalComponentSaver::Elements InternalComponentSaver::bulkheadElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf(
        "bulkhead", [&](Elements& elements) { addRadiusRingParams(component, writer, elements); });
}

InternalComponentSaver::Elements InternalComponentSaver::massComponentElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("masscomponent", [&](Elements& elements) {
        addMassObjectParams(component, writer, elements);

        const auto& mass = Saver::as<MassComponent>(component);

        // Decision E1 of 2026-10-14: the component's own mass (Java: mass.getMass(), the override
        // mass when the mass is overridden).
        elements.push_back(Saver::doubleElement("mass", mass.getComponentMass()));

        const MassComponent::MassComponentType type = mass.getMassComponentType();
        elements.push_back(Saver::element("masscomponenttype", orkName(type)));
    });
}

InternalComponentSaver::Elements InternalComponentSaver::shockCordElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("shockcord", [&](Elements& elements) {
        addMassObjectParams(component, writer, elements);

        const auto& mass = Saver::as<ShockCord>(component);

        if (mass.isCordLengthAutomatic())
        {
            elements.push_back(Saver::element("cordlength", "auto"));
        }
        else
        {
            elements.push_back(Saver::doubleElement("cordlength", mass.getCordLength()));
        }
        elements.push_back(Saver::materialParam(mass.getMaterial(), writer));
    });
}

InternalComponentSaver::Elements InternalComponentSaver::parachuteElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("parachute", [&](Elements& elements) {
        addRecoveryDeviceParams(component, writer, elements);
        const auto& para = Saver::as<Parachute>(component);

        elements.push_back(Saver::doubleElement("diameter", para.getDiameter()));
        elements.push_back(Saver::integerElement("linecount", para.getLineCount()));
        if (para.isLineLengthAutomatic())
        {
            elements.push_back(Saver::element("linelength", "auto"));
        }
        else
        {
            elements.push_back(Saver::doubleElement("linelength", para.getLineLength()));
        }
        elements.push_back(Saver::materialParam("linematerial", para.getLineMaterial(), writer));
    });
}

InternalComponentSaver::Elements InternalComponentSaver::streamerElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("streamer", [&](Elements& elements) {
        addRecoveryDeviceParams(component, writer, elements);
        const auto& st = Saver::as<Streamer>(component);

        elements.push_back(Saver::doubleElement("striplength", st.getStripLength()));
        elements.push_back(Saver::doubleElement("stripwidth", st.getStripWidth()));
    });
}

}  // namespace QtRocket
