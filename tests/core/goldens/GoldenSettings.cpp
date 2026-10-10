#include "goldens/GoldenSettings.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The comparison collector of the golden tests.
using Mismatches = GoldenMismatches;

/// The names of the elements the comparison is about.
constexpr const char* kDeployEvent             = "deployevent";
constexpr const char* kDeploymentConfiguration = "deploymentconfiguration";
constexpr const char* kSeparationEvent         = "separationevent";
constexpr const char* kSeparationConfiguration = "separationconfiguration";
constexpr const char* kMotorMount              = "motormount";
constexpr const char* kMotor                   = "motor";
constexpr const char* kIgnitionConfiguration   = "ignitionconfiguration";
constexpr const char* kPreset                  = "preset";
constexpr const char* kConfigurationId         = "configid";

/// A flight configuration of the rocket and the id the re-save has for it.
struct SavedConfiguration
{
    FlightConfigurationId id;
    std::string           savedId;
};

/// The flight configurations of @p rocket, without the default one, each with the id of the
/// golden configuration at its place (the golden configurations are the rocket's in order, the
/// default first).
[[nodiscard]] std::vector<SavedConfiguration> savedConfigurations(const Rocket& rocket,
                                                                  const json&   geometry)
{
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    std::vector<SavedConfiguration>          configurations;
    configurations.reserve(ids.size());
    for (const json& golden : geometry.at("configurations"))
    {
        const int index = golden.at("index").get<int>();
        if (index >= 1 && static_cast<std::size_t>(index) <= ids.size())
        {
            configurations.push_back({.id      = ids[static_cast<std::size_t>(index) - 1],
                                      .savedId = golden.at("id").get<std::string>()});
        }
    }
    return configurations;
}

/// The number of children of @p parent that are named @p name.
[[nodiscard]] std::int64_t countChildren(const pugi::xml_node& parent, const char* name)
{
    const auto children = parent.children(name);
    return std::distance(children.begin(), children.end());
}

/// Compares the text of the one child @p name of @p parent with @p actual.
void compareText(Mismatches& m, std::string_view field, const pugi::xml_node& parent,
                 const char* name, std::string_view actual)
{
    m.integer(std::format("{} <{}>: number", field, name), 1, countChildren(parent, name));
    m.text(std::format("{} <{}>", field, name), parent.child_value(name), actual);
}

/// Compares the number the one child @p name of @p parent holds with @p actual: exactly, see the
/// top of the header.
void compareNumber(Mismatches& m, std::string_view field, const pugi::xml_node& parent,
                   const char* name, double actual)
{
    m.integer(std::format("{} <{}>: number", field, name), 1, countChildren(parent, name));
    const std::optional<double> expected = Strings::javaParseDouble(parent.child_value(name));
    if (!expected)
    {
        m.note(std::format("{} <{}>: \"{}\" is no number", field, name, parent.child_value(name)));
        return;
    }
    m.exact(std::format("{} <{}>", field, name), *expected, actual);
}

/// Compares the attribute @p name of the <preset> @p element with @p actual (an attribute that
/// the element lacks reads as an empty text, which no preset has for any of the four).
void comparePresetAttribute(Mismatches& m, const pugi::xml_node& element, const char* name,
                            std::string_view actual)
{
    m.text(std::format("<preset> {}", name), element.attribute(name).value(), actual);
}

/// The component preset of @p component against the <preset> of its element @p node
/// (RocketComponentSaver.addParams(): the element is there exactly when the component has a
/// preset, with the name of its type, the simple name of its manufacturer, its part number and
/// its digest). For a design that was loaded without the presets (@p presets) the component has
/// none, whatever the element names; an element is then counted as compared when that is so.
void comparePreset(Mismatches& m, SettingsCounts& counts, const pugi::xml_node& node,
                   const RocketComponent& component, SavedPresets presets)
{
    const ComponentPreset* const preset   = component.getPresetComponent();
    const std::int64_t           elements = countChildren(node, kPreset);
    if (presets == SavedPresets::NOT_FOUND)
    {
        m.integer("preset of the component, loaded without presets: number", 0,
                  preset != nullptr ? 1 : 0);
        counts.presets += preset == nullptr ? static_cast<int>(elements) : 0;
        return;
    }
    m.integer("<preset>: number", elements, preset != nullptr ? 1 : 0);
    const pugi::xml_node element = node.child(kPreset);
    if (element.empty() || preset == nullptr)
    {
        return;
    }
    comparePresetAttribute(m, element, "type", componentPresetTypeName(preset->getType()));
    comparePresetAttribute(m, element, "manufacturer", preset->getManufacturer().getSimpleName());
    comparePresetAttribute(m, element, "partno", preset->getPartNo());
    comparePresetAttribute(m, element, "digest", preset->getDigest());
    counts.presets++;
}

/// Compares the deployment @p actual with the three elements of it in @p parent
/// (RecoveryDeviceSaver.addDeploymentConfigurationParams()).
void compareDeployment(Mismatches& m, std::string_view field, const pugi::xml_node& parent,
                       const DeploymentConfiguration& actual)
{
    compareText(m, field, parent, kDeployEvent, orkName(actual.getDeployEvent()));
    compareNumber(m, field, parent, "deployaltitude", actual.getDeployAltitude());
    compareNumber(m, field, parent, "deploydelay", actual.getDeployDelay());
}

/// Compares the separation @p actual with the three elements of it in @p parent
/// (AxialStageSaver.addSeparationConfigParams()).
void compareSeparation(Mismatches& m, std::string_view field, const pugi::xml_node& parent,
                       const StageSeparationConfiguration& actual)
{
    compareText(m, field, parent, kSeparationEvent, orkName(actual.getSeparationEvent()));
    compareNumber(m, field, parent, "separationaltitude", actual.getSeparationAltitude());
    compareNumber(m, field, parent, "separationdelay", actual.getSeparationDelay());
}

/// Compares the children @p name of @p parent, one per flight configuration with a value of its
/// own, with the overrides of @p set: every element names a configuration of the rocket for
/// which the set has an override, which @p compare compares with it; no configuration is named
/// twice; and the set has no override beyond the elements. Returns the number of elements
/// compared.
template <class Parameter, class Compare>
[[nodiscard]] int compareOverrides(Mismatches& m, const pugi::xml_node& parent, const char* name,
                                   const FlightConfigurableParameterSet<Parameter>& set,
                                   std::span<const SavedConfiguration>              configurations,
                                   Compare                                          compare)
{
    std::vector<std::string_view> seen;
    int                           compared = 0;
    for (const pugi::xml_node element : parent.children(name))
    {
        const std::string_view savedId = element.attribute(kConfigurationId).value();
        const std::string      field   = std::format("<{} configid=\"{}\">", name, savedId);
        const auto             configuration =
            std::ranges::find(configurations, savedId, &SavedConfiguration::savedId);
        if (configuration == configurations.end())
        {
            m.note(field + ": no flight configuration of the rocket has this id");
        }
        else if (std::ranges::find(seen, savedId) != seen.end())
        {
            m.note(field + ": a second element for this flight configuration");
        }
        else if (!set.containsId(configuration->id) || set.isDefault(configuration->id))
        {
            m.note(field + ": the component has the default for this flight configuration");
        }
        else
        {
            compare(m, field, element, set.get(configuration->id));
            compared++;
        }
        seen.push_back(savedId);
    }
    m.integer(std::format("<{}>: number", name), countChildren(parent, name),
              static_cast<std::int64_t>(set.getIds().size()));
    return compared;
}

/// A recovery device: its default deployment and the deployments of single flight
/// configurations (RecoveryDeviceSaver.addParams()).
void compareRecoveryDevice(Mismatches& m, SettingsCounts& counts, const pugi::xml_node& node,
                           const RecoveryDevice&               device,
                           std::span<const SavedConfiguration> configurations)
{
    const FlightConfigurableParameterSet<DeploymentConfiguration>& deployments =
        device.getDeploymentConfigurations();
    compareDeployment(m, "default", node, deployments.getDefault());
    counts.deployEvents += node.child(kDeployEvent).empty() ? 0 : 1;
    const int overrides = compareOverrides(m, node, kDeploymentConfiguration, deployments,
                                           configurations, compareDeployment);
    counts.deployEvents += overrides;
    counts.deploymentConfigurations += overrides;
}

/// A stage: nothing for the first one, else its default separation and the separations of
/// single flight configurations (AxialStageSaver.addParams()).
void compareStage(Mismatches& m, SettingsCounts& counts, const pugi::xml_node& node,
                  const AxialStage& stage, std::span<const SavedConfiguration> configurations)
{
    if (stage.getStageNumber() <= 0)
    {
        m.integer("<separationevent> of the first stage: number", 0,
                  countChildren(node, kSeparationEvent));
        m.integer("<separationconfiguration> of the first stage: number", 0,
                  countChildren(node, kSeparationConfiguration));
        return;
    }
    const FlightConfigurableParameterSet<StageSeparationConfiguration>& separations =
        stage.getSeparationConfigurations();
    compareSeparation(m, "default", node, separations.getDefault());
    counts.separationEvents += node.child(kSeparationEvent).empty() ? 0 : 1;
    const int overrides = compareOverrides(m, node, kSeparationConfiguration, separations,
                                           configurations, compareSeparation);
    counts.separationEvents += overrides;
    counts.separationConfigurations += overrides;
}

/// Compares the motor @p instance with its element @p element: the designation, the digest and
/// the ejection delay, which the file writes as "none" for a plugged motor (the delay is then
/// Motor::kPluggedDelay, an infinity).
void compareMotorElement(Mismatches& m, const std::string& field, const pugi::xml_node& element,
                         const MotorConfiguration& instance)
{
    compareText(m, field, element, "designation", instance.getMotor()->getDesignation());
    compareText(m, field, element, "digest", instance.getMotor()->getDigest());
    const std::string_view      text  = element.child_value("delay");
    const std::optional<double> delay = text == "none" ? std::optional<double>{Motor::kPluggedDelay}
                                                       : Strings::javaParseDouble(text);
    m.integer(field + " <delay>: number", 1, countChildren(element, "delay"));
    if (!delay)
    {
        m.note(std::format("{} <delay>: \"{}\" is neither none nor a number", field, text));
        return;
    }
    m.exact(field + " <delay>", *delay, instance.getEjectionDelay());
}

/// The motor and the ignition of the flight configuration @p configuration in the motor mount
/// @p mount, whose element is @p element: nothing when the configuration gives the mount no
/// motor (the saver stores nothing then), else its <motor> and, as the motor is not the mount's
/// default instance, its <ignitionconfiguration>.
void compareMotorOfConfiguration(Mismatches& m, SettingsCounts& counts,
                                 const pugi::xml_node& element, const MotorMount& mount,
                                 const SavedConfiguration& configuration)
{
    const MotorConfiguration& instance = mount.getMotorConfig(configuration.id);
    if (instance.isEmpty())
    {
        return;
    }
    const std::string    id = configuration.savedId;
    const pugi::xml_node motor =
        element.find_child_by_attribute(kMotor, kConfigurationId, id.c_str());
    if (motor.empty())
    {
        m.note(std::format("<motor configid=\"{}\">: not in the re-save", id));
    }
    else
    {
        compareMotorElement(m, std::format("<motor configid=\"{}\">", id), motor, instance);
        counts.motors++;
    }
    if (instance == mount.getDefaultMotorConfig())
    {
        return;
    }
    const pugi::xml_node ignition =
        element.find_child_by_attribute(kIgnitionConfiguration, kConfigurationId, id.c_str());
    if (ignition.empty())
    {
        m.note(std::format("<ignitionconfiguration configid=\"{}\">: not in the re-save", id));
        return;
    }
    const std::string field = std::format("<ignitionconfiguration configid=\"{}\">", id);
    compareText(m, field, ignition, "ignitionevent", orkName(instance.getIgnitionEvent()));
    compareNumber(m, field, ignition, "ignitiondelay", instance.getIgnitionDelay());
    counts.ignitionConfigurations++;
}

/// A component that can be a motor mount: it has a <motormount> exactly when it is one, with
/// the default ignition, the overhang and the motors of the flight configurations of the rocket
/// (RocketComponentSaver.motorMountParams()).
void compareMotorMount(Mismatches& m, SettingsCounts& counts, const pugi::xml_node& node,
                       const MotorMount& mount, std::span<const SavedConfiguration> configurations)
{
    m.integer("<motormount>: number", countChildren(node, kMotorMount),
              mount.isMotorMount() ? 1 : 0);
    const pugi::xml_node element = node.child(kMotorMount);
    if (element.empty() || !mount.isMotorMount())
    {
        return;
    }
    const MotorConfiguration& defaultInstance = mount.getDefaultMotorConfig();
    compareText(m, "<motormount>", element, "ignitionevent",
                orkName(defaultInstance.getIgnitionEvent()));
    compareNumber(m, "<motormount>", element, "ignitiondelay", defaultInstance.getIgnitionDelay());
    compareNumber(m, "<motormount>", element, "overhang", mount.getMotorOverhang());
    counts.motorMounts++;

    SettingsCounts ofTheMount;
    for (const SavedConfiguration& configuration : configurations)
    {
        compareMotorOfConfiguration(m, ofTheMount, element, mount, configuration);
    }
    // The other direction: no <motor> and no <ignitionconfiguration> beyond those compared.
    m.integer("<motor>: number", countChildren(element, kMotor), ofTheMount.motors);
    m.integer("<ignitionconfiguration>: number", countChildren(element, kIgnitionConfiguration),
              ofTheMount.ignitionConfigurations);
    counts += ofTheMount;
}

/// Compares the preset and the settings of @p component with its element @p node.
void compareComponent(Mismatches& m, SettingsCounts& counts, const pugi::xml_node& node,
                      const RocketComponent&              component,
                      std::span<const SavedConfiguration> configurations, SavedPresets presets)
{
    comparePreset(m, counts, node, component, presets);
    if (const auto* device = dynamic_cast<const RecoveryDevice*>(&component))
    {
        compareRecoveryDevice(m, counts, node, *device, configurations);
    }
    if (const auto* stage = dynamic_cast<const AxialStage*>(&component))
    {
        compareStage(m, counts, node, *stage, configurations);
    }
    if (const auto* mount = dynamic_cast<const MotorMount*>(&component))
    {
        compareMotorMount(m, counts, node, *mount, configurations);
    }
}

/// Adds the settings elements of @p node and of everything below it to @p counts.
void countSettings(SettingsCounts& counts, const pugi::xml_node& node)
{
    const std::string_view name = node.name();
    counts.deployEvents += name == kDeployEvent ? 1 : 0;
    counts.deploymentConfigurations += name == kDeploymentConfiguration ? 1 : 0;
    counts.separationEvents += name == kSeparationEvent ? 1 : 0;
    counts.separationConfigurations += name == kSeparationConfiguration ? 1 : 0;
    counts.motorMounts += name == kMotorMount ? 1 : 0;
    counts.motors += name == kMotor ? 1 : 0;
    counts.ignitionConfigurations += name == kIgnitionConfiguration ? 1 : 0;
    counts.presets += name == kPreset ? 1 : 0;
    for (const pugi::xml_node child : node.children())
    {
        countSettings(counts, child);
    }
}

}  // namespace

std::string toText(const SettingsCounts& counts)
{
    return std::format(
        "{} deployevent, {} deploymentconfiguration, {} separationevent, "
        "{} separationconfiguration, {} motormount, {} motor, "
        "{} ignitionconfiguration, {} preset",
        counts.deployEvents, counts.deploymentConfigurations, counts.separationEvents,
        counts.separationConfigurations, counts.motorMounts, counts.motors,
        counts.ignitionConfigurations, counts.presets);
}

std::ostream& operator<<(std::ostream& out, const SettingsCounts& counts)
{
    return out << toText(counts);
}

SettingsCounts savedSettingsCounts(const pugi::xml_document& resave)
{
    SettingsCounts counts;
    countSettings(counts, resave);
    return counts;
}

SettingsComparison compareSavedSettings(const Rocket& rocket, const json& geometry,
                                        const pugi::xml_document& resave, SavedPresets presets)
{
    SettingsComparison                    result;
    const std::vector<SavedConfiguration> configurations = savedConfigurations(rocket, geometry);
    for (const RocketComponent& component : rocket.subtree())
    {
        const std::string path = goldenPathOf(component);
        Mismatches  m(std::format("settings of {} {} \"{}\"", className(component.kind()), path,
                                  component.getName()));
        const json* golden = findGoldenComponent(geometry, path);
        const pugi::xml_node node =
            golden != nullptr ? savedGoldenComponent(resave, golden->at("id").get<std::string>())
                              : pugi::xml_node{};
        if (node.empty())
        {
            m.note("not in the re-save");
        }
        else
        {
            compareComponent(m, result.compared, node, component, configurations, presets);
        }
        result.report += m.report();
    }
    return result;
}

}  // namespace QtRocket::Test
