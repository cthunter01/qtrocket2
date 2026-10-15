#include "QtRocket/file/openrocket/AssemblySaver.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/RocketComponentSaver.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

namespace QtRocket
{

namespace
{

using Elements = AssemblySaver::Elements;

/// Java's OpenRocketSaver.INDENT.
constexpr std::string_view kIndent = "  ";

/// RocketSaver.addParams(): the designer and the other texts of the design.
void addDesignTexts(const Rocket& rocket, OrkWriter& writer, Elements& elements)
{
    if (!rocket.getDesigner().empty())
    {
        elements.push_back(
            RocketComponentSaver::textElement("designer", rocket.getDesigner(), writer));
    }
    elements.push_back(
        RocketComponentSaver::element("designtype", orkName(rocket.getDesignType())));
    if (!rocket.getKitName().empty())
    {
        elements.push_back(
            RocketComponentSaver::textElement("kitname", rocket.getKitName(), writer));
    }
    if (!rocket.getRevision().empty())
    {
        elements.push_back(
            RocketComponentSaver::textElement("revision", rocket.getRevision(), writer));
    }
}

/// RocketSaver.addParams(): the flight configurations.
void addFlightConfigurations(const Rocket& rocket, OrkWriter& writer,
                             const Preferences& preferences, Elements& elements)
{
    for (const FlightConfigurationId& fcid : rocket.getIds())
    {
        const FlightConfiguration& flightConfig = rocket.getFlightConfiguration(fcid);

        // these are actually FlightConfigurationIds, but the old tag name is preserved for
        // backwards-compatibility.
        std::string str = std::format(R"(<motorconfiguration configid="{}")", fcid.toString());
        // if the configuration is the default, add the tag (Java: FlightConfiguration.equals(),
        // which compares the ids)
        if (rocket.getSelectedConfiguration().getId() == flightConfig.getId())
        {
            str += R"( default="true")";
        }

        // close motorconfiguration opening tag
        str += '>';
        elements.push_back(std::move(str));

        // flight configuration name
        if (flightConfig.isNameOverridden(preferences))
        {
            elements.push_back(std::string(kIndent) +
                               RocketComponentSaver::textElement(
                                   "name", flightConfig.getNameRaw(preferences), writer));
        }
        // stage activeness
        for (const AxialStage* stage : rocket.getStageList())
        {
            elements.push_back(std::format(
                R"({}<stage number="{}" active="{}"/>)", kIndent, stage->getStageNumber(),
                flightConfig.isStageActive(stage->getStageNumber()) ? "true" : "false"));
        }

        elements.emplace_back("</motorconfiguration>");
    }
}

/// Java's RocketSaver.addParams().
void addRocketParams(const RocketComponent& c, OrkWriter& writer, const Preferences& preferences,
                     Elements& elements)
{
    RocketComponentSaver::addParams(c, writer, elements);

    const auto& rocket = RocketComponentSaver::as<Rocket>(c);

    addDesignTexts(rocket, writer, elements);

    // Flight configurations
    addFlightConfigurations(rocket, writer, preferences, elements);

    // Reference diameter
    elements.push_back(
        RocketComponentSaver::element("referencetype", orkName(rocket.getReferenceType())));
    if (rocket.getReferenceType() == ReferenceType::CUSTOM)
    {
        elements.push_back(RocketComponentSaver::doubleElement("customreference",
                                                               rocket.getCustomReferenceLength()));
    }
}

/// Java's addSeparationConfigParams(): the three lines of @p config, with a 4-space prefix when
/// @p indent.
void addSeparationConfigParams(const StageSeparationConfiguration& config, bool indent,
                               Elements& elements)
{
    const std::string_view prefix = indent ? "    " : "";
    elements.push_back(
        std::string(prefix) +
        RocketComponentSaver::element("separationevent", orkName(config.getSeparationEvent())));
    elements.push_back(
        std::string(prefix) +
        RocketComponentSaver::doubleElement("separationaltitude", config.getSeparationAltitude()));
    elements.push_back(std::string(prefix) + RocketComponentSaver::doubleElement(
                                                 "separationdelay", config.getSeparationDelay()));
}

/// Java's AxialStageSaver.addParams().
void addStageParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    RocketComponentSaver::addParams(c, writer, elements);
    const auto& stage = RocketComponentSaver::as<AxialStage>(c);

    if (stage.getStageNumber() > 0)
    {
        // NOTE: Default config must be BEFORE overridden config for proper backward compatibility
        // later on
        const FlightConfigurableParameterSet<StageSeparationConfiguration>& configurations =
            stage.getSeparationConfigurations();
        addSeparationConfigParams(configurations.getDefault(), false, elements);

        // Every configuration of the set, the default left out by getIds() (Java's commented-out
        // isDefault() test is not made).
        for (const FlightConfigurationId& fcid : configurations.getIds())
        {
            const StageSeparationConfiguration& curSepCfg = configurations.get(fcid);
            elements.push_back(
                std::format(R"(<separationconfiguration configid="{}">)", fcid.toString()));
            addSeparationConfigParams(curSepCfg, true, elements);
            elements.emplace_back("</separationconfiguration>");
        }
    }
}

}  // namespace

AssemblySaver::Elements AssemblySaver::rocketElements(const RocketComponent& component,
                                                      OrkWriter&             writer,
                                                      const Preferences&     preferences)
{
    Elements list;

    list.emplace_back("<rocket>");
    addRocketParams(component, writer, preferences, list);
    list.emplace_back("</rocket>");

    return list;
}

AssemblySaver::Elements AssemblySaver::axialStageElements(const RocketComponent& component,
                                                          OrkWriter&             writer,
                                                          const Preferences& /*preferences*/)
{
    Elements list;

    if (component.isAfter())
    {
        // yes, this test is redundant. I'm merely paranoid, and attempting to future-proof it
        // (Java: c.getClass().equals(AxialStage.class))
        if (component.kind() == ComponentKind::AXIAL_STAGE)
        {
            // kept as simply 'stage' for backward compatibility
            list.emplace_back("<stage>");
            addStageParams(component, writer, list);
            list.emplace_back("</stage>");
        }
    }
    else if (component.kind() == ComponentKind::PARALLEL_STAGE)
    {
        list.emplace_back("<parallelstage>");
        addStageParams(component, writer, list);
        list.emplace_back("</parallelstage>");
    }

    return list;
}

AssemblySaver::Elements AssemblySaver::componentAssemblyElements(const RocketComponent& component,
                                                                 OrkWriter&             writer,
                                                                 const Preferences& /*preferences*/)
{
    Elements list;

    // (Java's commented-out branch for a ParallelStage is not ported.)
    if (!component.isAfter() && component.kind() == ComponentKind::POD_SET)
    {
        list.emplace_back("<podset>");
        RocketComponentSaver::addParams(component, writer, list);
        list.emplace_back("</podset>");
    }

    return list;
}

}  // namespace QtRocket
