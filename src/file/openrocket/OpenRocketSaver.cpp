#include "QtRocket/file/openrocket/OpenRocketSaver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/ComponentSaver.h"
#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/PhotoStudioSaver.h"
#include "QtRocket/file/openrocket/SimulationSaver.h"
#include "QtRocket/file/openrocket/WarningHandler.h"
#include "QtRocket/file/openrocket/WindHandler.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Version.h"

namespace QtRocket
{

namespace
{

/// "major.minor" as major * divisor + minor (the loader's reading of a version), or -1 for a text
/// of another form.
consteval int versionNumber(std::string_view text)
{
    const std::size_t point = text.find('.');
    if (point == std::string_view::npos || point == 0 || point + 1 == text.size())
    {
        return -1;
    }
    int major = 0;
    for (const char c : text.substr(0, point))
    {
        major = (major * 10) + (c - '0');
    }
    int minor = 0;
    for (const char c : text.substr(point + 1))
    {
        minor = (minor * 10) + (c - '0');
    }
    return (major * DocumentConfig::kFileVersionDivisor) + minor;
}

// Java's note in calculateNecessaryFileVersion(): "Remember to update the supported versions in
// DocumentConfig as well!" Here the compiler remembers it.
static_assert(versionNumber(DocumentConfig::kSupportedVersions.back()) ==
                  OpenRocketSaver::kFileVersion,
              "the saver writes the newest version the loader knows");

/// The lines OpenRocket's saver of @p component's class gives (Java: the getElements() that
/// findGetElementsMethod() finds): the open tag, the parameter lines and the close tag, or none
/// (ComponentSaver). Texts go through the writer's escape(); the preferences decide whether a
/// flight configuration's name is its own (FlightConfiguration::isNameOverridden()).
[[nodiscard]] std::vector<std::string> componentElements(const RocketComponent& component,
                                                         OrkWriter&             writer,
                                                         const Preferences&     preferences)
{
    return ComponentSaver::getElements(component, writer, preferences);
}

/// A component whose subcomponents are being written: Java's saveComponent() between the
/// <subcomponents> of its children and its close tag.
struct OpenComponent
{
    std::vector<RocketComponent*> children;
    /// The child to write next.
    std::size_t next{0};
    /// The last line of the component's saver.
    std::string closeTag;
};

/// Java's saveComponent() up to the children: the open tag and the parameter lines of
/// @p component, then, when it has children, a blank line and <subcomponents> (the component
/// is returned to have them written); without children its close tag.
[[nodiscard]] std::optional<OpenComponent> openComponent(RocketComponent&   component,
                                                         OrkWriter&         writer,
                                                         const Preferences& preferences)
{
    std::vector<std::string> list = componentElements(component, writer, preferences);
    if (list.empty())
    {
        return std::nullopt;  // Nothing to do (Java)
    }
    // Java: RuntimeException "BUG, component data length less than two lines."
    QTROCKET_ASSERT(list.size() >= 2);

    writer.line(list.front());
    writer.indent();
    for (std::size_t i = 1; i + 1 < list.size(); ++i)
    {
        writer.line(list[i]);
    }
    if (component.getChildCount() > 0)
    {
        writer.blankLine();
        writer.line("<subcomponents>");
        writer.indent();
        return OpenComponent{
            .children = component.getChildren(), .next = 0, .closeTag = std::move(list.back())};
    }
    writer.outdent();
    writer.line(list.back());
    return std::nullopt;
}

/// Java's recursive saveComponent() of @p rocket and everything below it. The recursion is a
/// loop over a stack here, so that a tree of any depth that a caller built is written; the
/// order of the lines and of the savers' calls is Java's.
void saveComponents(RocketComponent& rocket, OrkWriter& writer, const Preferences& preferences)
{
    std::vector<OpenComponent> open;
    if (std::optional<OpenComponent> first = openComponent(rocket, writer, preferences))
    {
        open.push_back(std::move(*first));
    }
    while (!open.empty())
    {
        OpenComponent& top = open.back();
        if (top.next < top.children.size())
        {
            // A blank line between two children (Java: emptyline), whatever the first wrote.
            if (top.next > 0)
            {
                writer.blankLine();
            }
            RocketComponent& child = *top.children[top.next];
            ++top.next;
            if (std::optional<OpenComponent> opened = openComponent(child, writer, preferences))
            {
                open.push_back(std::move(*opened));  // top is not used after this
            }
            continue;
        }
        writer.outdent();
        writer.line("</subcomponents>");
        writer.outdent();
        writer.line(top.closeTag);
        open.pop_back();
    }
}

/// Java's saveCustomDatatypes() and saveCustomExpressionDatatype().
void saveCustomDatatypes(const OpenRocketDocument& document, OrkWriter& writer)
{
    const std::vector<CustomExpression>& expressions = document.getCustomExpressions();
    if (expressions.empty())
    {
        return;
    }
    writer.line("<datatypes>");
    writer.indent();
    for (const CustomExpression& expression : expressions)
    {
        writer.line("<type source=\"customexpression\">");
        writer.indent();
        writer.element("name", expression.getName());
        writer.element("symbol", expression.getSymbol());
        // "auto": the unit type is determined from the unit's text.
        writer.line(
            std::format("<unit unittype=\"auto\">{}</unit>", writer.escape(expression.getUnit())));
        writer.element("expression", expression.getExpressionString());
        writer.outdent();
        writer.line("</type>");
    }
    writer.outdent();
    writer.line("</datatypes>");
    writer.blankLine();
}

/// The simulations: <simulations> with a blank line between two of them, each written by
/// SimulationSaver (Java's saveSimulation()).
void saveSimulations(const OpenRocketDocument& document, bool saveSimulationData, OrkWriter& writer,
                     WarningSet& warnings)
{
    writer.line("<simulations>");
    writer.indent();
    bool first = true;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        if (!first)
        {
            writer.blankLine();
        }
        first = false;
        SimulationSaver::saveSimulation(*simulation, saveSimulationData, writer, warnings);
    }
    writer.outdent();
    writer.line("</simulations>");
}

/// Java's savePhotoSettings(): written also when there are no settings.
void savePhotoSettings(const std::map<std::string, std::string>& settings, OrkWriter& writer)
{
    writer.line("<photostudio>");
    writer.indent();
    for (const std::string& element : PhotoStudioSaver::getElements(settings, writer))
    {
        writer.line(element);
    }
    writer.outdent();
    writer.line("</photostudio>");
}

/// The Config value of a document preference: the same Java type (Boolean, Integer, Double,
/// String), which writeEntry() prints alike.
[[nodiscard]] Config::Value configValueOf(const DocumentPreferences::Value& value)
{
    return std::visit([](const auto& held) { return Config::Value(held); }, value);
}

/// Java's saveDocumentPreferences(): the preferences, then the document materials.
void saveDocumentPreferences(const OpenRocketDocument& document, OrkWriter& writer)
{
    writer.line("<docprefs>");
    writer.indent();

    // Normal preferences
    for (const auto& [key, preference] : document.getDocumentPreferences().preferences())
    {
        writer.entry("pref", key, configValueOf(preference.value()), true);
    }

    // Document materials
    if (document.getDocumentMaterials().totalMaterialCount() > 0)
    {
        writer.line("<docmaterials>");
        writer.indent();
        const MaterialDatabase materials = document.getDocumentMaterials().allMaterials();
        for (const Material& material : materials)
        {
            writer.element("material", material.toStorableString());
        }
        writer.outdent();
        writer.line("</docmaterials>");
    }

    writer.outdent();
    writer.line("</docprefs>");
}

/// The save warnings of decisions E3 and O12 and of the user's answer of 2026-10-16 (see the
/// header) for the text @p writer holds, which is finished, of @p document.
void addSaveWarnings(const OpenRocketDocument& document, const OrkWriter& writer,
                     WarningSet& warnings)
{
    if (writer.droppedCharacters() > 0)
    {
        warnings.add(OpenRocketSaver::kDroppedCharacters);
    }
    if (writer.size() > GeneralRocketLoader::kMaxDocumentBytes)
    {
        warnings.add(OpenRocketSaver::documentBeyondLoaderBound(writer.size()));
    }
    for (const std::string& warning : OpenRocketSaver::beyondLoaderBounds(document))
    {
        warnings.add(warning);
    }
}

// ------------------------------------------------------------------------ the loader's bounds

/// a * b, or the largest number for a product beyond it (DocumentConfig's arithmetic).
[[nodiscard]] constexpr std::uint64_t saturatingProduct(std::uint64_t a, std::uint64_t b) noexcept
{
    constexpr std::uint64_t kSaturated = std::numeric_limits<std::uint64_t>::max();
    return a != 0 && b > kSaturated / a ? kSaturated : a * b;
}

/// a + b, or the largest number for a sum beyond it.
[[nodiscard]] constexpr std::uint64_t saturatingSum(std::uint64_t a, std::uint64_t b) noexcept
{
    constexpr std::uint64_t kSaturated = std::numeric_limits<std::uint64_t>::max();
    return b > kSaturated - a ? kSaturated : a + b;
}

/// An instance count as the loader's budget counts it: a count below 1 is none
/// (DocumentConfig::instanceLoad()).
[[nodiscard]] std::uint64_t instancesCounted(int count) noexcept
{
    return count > 0 ? static_cast<std::uint64_t>(count) : 0;
}

/// The instance count a component of @p kind has when the loader attaches it: what its
/// constructor gives it (DocumentConfig::createComponent(), which ComponentHandler makes it
/// with), 2 for a pod set and a booster set, 3 for a fin set. The rocket, which the loader does
/// not make, has 1.
[[nodiscard]] std::uint64_t instancesOnAttaching(ComponentKind kind)
{
    static const std::array<std::uint64_t, kAllComponentKinds.size()> kCounts = [] {
        std::array<std::uint64_t, kAllComponentKinds.size()> counts{};
        for (std::size_t i = 0; i < kAllComponentKinds.size(); ++i)
        {
            const std::unique_ptr<RocketComponent> made =
                DocumentConfig::createComponent(xmlName(kAllComponentKinds.at(i)));
            counts.at(i) = made != nullptr ? instancesCounted(made->getInstanceCount()) : 1;
        }
        return counts;
    }();
    for (std::size_t i = 0; i < kAllComponentKinds.size(); ++i)
    {
        if (kAllComponentKinds.at(i) == kind)
        {
            return kCounts.at(i);
        }
    }
    QTROCKET_UNREACHABLE();
}

/// Whether the loader refuses an instance count of a component of @p kind above
/// DocumentConfig::kMaxCount: the IntSetters of DocumentConfig's table that have that maximum
/// (LaunchLug, RailButton, RadiusRingComponent, PodSet and ParallelStage "instancecount"). A
/// fin set keeps 8 of a larger count, as it does itself, and an inner tube sets its count
/// through its cluster configuration.
[[nodiscard]] bool instanceCountIsBounded(ComponentKind kind) noexcept
{
    constexpr std::array<ComponentKind, 6> kBounded{
        ComponentKind::LAUNCH_LUG, ComponentKind::RAIL_BUTTON, ComponentKind::CENTERING_RING,
        ComponentKind::BULKHEAD,   ComponentKind::POD_SET,     ComponentKind::PARALLEL_STAGE};
    return std::ranges::find(kBounded, kind) != kBounded.end();
}

/// The count of @p component that the loader holds to DocumentConfig::kMaxCount (see
/// instanceCountIsBounded(); the line count of a parachute too), or none.
[[nodiscard]] std::optional<int> boundedCountOf(const RocketComponent& component)
{
    if (instanceCountIsBounded(component.kind()))
    {
        return component.getInstanceCount();
    }
    if (const auto* const parachute = dynamic_cast<const Parachute*>(&component))
    {
        return parachute->getLineCount();
    }
    return std::nullopt;
}

/// What the loader counts of the components of a rocket, for each bound by itself (see the
/// header): ComponentHandler's checks in the order of the file, which is the pre-order of the
/// tree.
struct ComponentCounts
{
    /// The components below the rocket (DocumentConfig::componentCount()).
    std::size_t components{0};
    /// Their stages, booster sets and pod sets (DocumentConfig::assemblyCount()).
    std::size_t assemblies{0};
    /// The level of the deepest component: 1 for a stage.
    std::size_t deepest{0};
    /// The counts above DocumentConfig::kMaxCount (boundedCountOf()), and the largest of them.
    std::size_t countsAboveBound{0};
    int         largestCount{0};
    /// The instances of the whole tree (instancesOfSubtree() of the rocket).
    std::uint64_t instances{0};
    /// The most instances the tree has while the loader builds it: each component is attached
    /// with instancesOnAttaching() and gets its own count afterwards.
    std::uint64_t instancePeak{0};
};

/// One component the walk of countComponents() has still to visit.
struct PendingComponent
{
    const RocketComponent* component;
    /// The instances of its parent: the instance counts from the rocket down to the parent,
    /// multiplied.
    std::uint64_t parentInstances;
    /// Its level below the rocket.
    std::size_t depth;
};

/// Counts @p pending, a component, into @p counts (see ComponentCounts).
void countComponent(const PendingComponent& pending, ComponentCounts& counts)
{
    const RocketComponent& component = *pending.component;
    ++counts.components;
    if (isAssembly(component.kind()))
    {
        ++counts.assemblies;
    }
    counts.deepest = std::max(counts.deepest, pending.depth);
    if (const std::optional<int> count = boundedCountOf(component);
        count.has_value() && *count > DocumentConfig::kMaxCount)
    {
        ++counts.countsAboveBound;
        counts.largestCount = std::max(counts.largestCount, *count);
    }
    // DocumentConfig::childFits() when the component is attached, instanceCountFits() when its
    // count is set (only a count above the one it has is asked about).
    const std::uint64_t own  = instancesCounted(component.getInstanceCount());
    const std::uint64_t most = saturatingProduct(
        pending.parentInstances, std::max(own, instancesOnAttaching(component.kind())));
    counts.instancePeak = std::max(counts.instancePeak, saturatingSum(counts.instances, most));
    counts.instances =
        saturatingSum(counts.instances, saturatingProduct(pending.parentInstances, own));
}

/// The counts of the components of @p rocket (see ComponentCounts). The tree is walked with a
/// stack of its own, as saveComponents() writes it, so that a tree of any depth is counted.
[[nodiscard]] ComponentCounts countComponents(const Rocket& rocket)
{
    ComponentCounts counts;
    counts.instances    = instancesCounted(rocket.getInstanceCount());
    counts.instancePeak = counts.instances;
    std::vector<PendingComponent> pending;
    const auto pushChildren = [&pending](const RocketComponent& parent, std::uint64_t instances,
                                         std::size_t depth) {
        // In reverse, so that the first child is visited first.
        const std::vector<const RocketComponent*> children = parent.getChildren();
        for (const RocketComponent* child : std::views::reverse(children))
        {
            pending.push_back(PendingComponent{
                .component = child, .parentInstances = instances, .depth = depth + 1});
        }
    };
    pushChildren(rocket, counts.instances, 0);
    while (!pending.empty())
    {
        const PendingComponent next = pending.back();
        pending.pop_back();
        countComponent(next, counts);
        pushChildren(*next.component,
                     saturatingProduct(next.parentInstances,
                                       instancesCounted(next.component->getInstanceCount())),
                     next.depth);
    }
    return counts;
}

/// The flight configurations the loader makes of the simulations of @p document: those they
/// name and the rocket lacks, each once (SingleSimulationHandler).
[[nodiscard]] std::size_t configurationsOfSimulations(const OpenRocketDocument& document)
{
    const Rocket&                   rocket = document.getRocket();
    std::set<FlightConfigurationId> missing;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        if (!rocket.containsFlightConfigurationId(simulation->getId()))
        {
            missing.insert(simulation->getId());
        }
    }
    return missing.size();
}

/// The levels of @p wind that the loader takes from a <windlevel> each: those whose numbers are
/// finite and lie in the ranges a simulation holds them to (WindHandler, which passes over the
/// others without counting them).
[[nodiscard]] std::size_t windLevelsTaken(const MultiLevelPinkNoiseWindModel& wind)
{
    std::size_t taken = 0;
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : wind.getLevels())
    {
        const std::array<double, 4> values{level->getAltitude(), level->getSpeed(),
                                           level->getDirection(), level->getStandardDeviation()};
        if (!std::ranges::all_of(values, [](double value) { return std::isfinite(value); }))
        {
            continue;
        }
        // WindHandler's level: what addWindLevel() would make of the numbers.
        PinkNoiseWindModel model(0);
        model.setDirection(level->getDirection());
        model.setAverage(level->getSpeed());
        model.setStandardDeviation(level->getStandardDeviation());
        if (SimulationInputRanges::kWindLevelAltitude.contains(level->getAltitude()) &&
            WindHandler::inRange(model))
        {
            ++taken;
        }
    }
    return taken;
}

/// @p text, the description of a stored warning, as WarningHandler reads it back from the
/// <description> element SimulationSaver writes it in: without what OrkWriter::sanitizeText()
/// leaves out, with its line ends as the XML parser hands them on ("\r\n" and "\r" become "\n",
/// XML 1.0's rule; OrkWriter writes a carriage return as it is), and trimmed.
[[nodiscard]] std::string descriptionAsRead(std::string_view text)
{
    std::size_t       dropped = 0;
    const std::string kept    = OrkWriter::sanitizeText(text, dropped);
    std::string       lines;
    lines.reserve(kept.size());
    for (std::size_t i = 0; i < kept.size(); ++i)
    {
        if (kept[i] != '\r')
        {
            lines.push_back(kept[i]);
            continue;
        }
        lines.push_back('\n');
        if (i + 1 < kept.size() && kept[i + 1] == '\n')
        {
            ++i;
        }
    }
    return std::string(Strings::trim(lines));
}

/// @p warning as WarningHandler reads it back from the <warning> element SimulationSaver writes
/// for it: the warning that its type name, its number (which the text of a double gives back
/// exactly) and its description make (WarningHandler::makeWarning()), with its id, its priority
/// and its sources (a source is compared by the id of its component, which the loaded rocket
/// has, or keeps for a component it lacks).
[[nodiscard]] std::unique_ptr<Warning> warningAsRead(const Warning& warning)
{
    double parameter = std::numeric_limits<double>::quiet_NaN();
    if (const auto* const largeAoa = dynamic_cast<const Warning::LargeAOA*>(&warning))
    {
        parameter = largeAoa->aoa();
    }
    else if (const auto* const highSpeed =
                 dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(&warning))
    {
        parameter = highSpeed->speed();
    }
    std::unique_ptr<Warning> read = WarningHandler::makeWarning(
        warning.typeName(), parameter, descriptionAsRead(warning.messageDescription()));
    read->setId(warning.id());
    read->setPriority(warning.priority());
    read->setSources(warning.sources());
    return read;
}

/// How many of the stored warnings @p warnings the capacity of the loader's set counts when it
/// reads them back (FlightDataHandler, MessageSet::setCapacity()): the warnings as WarningHandler
/// reads them (warningAsRead()), of which two that are equal are one (two texts that differ only
/// in what a design file cannot hold, in their line ends or in white space at their ends, two
/// warnings of classes the loader reads as plain texts), all but one equal to the message the
/// set adds when it is full.
[[nodiscard]] std::size_t storedWarningsCounted(const WarningSet& warnings)
{
    WarningSet read;
    for (const Warning& warning : warnings)
    {
        read.add(*warningAsRead(warning));
    }
    const Warning::Other overflow = Warning::fromString(LoaderWarnings::tooManyStoredWarnings());
    return read.size() - (read.contains(overflow) ? 1U : 0U);
}

/// How deep lists stand in lists in @p value: 0 for a value that is no list, 1 for a list of
/// such values (ConfigHandler counts a list entry of the configuration as one list deep).
[[nodiscard]] std::size_t listDepth(const Config::Value& value)
{
    const Config::List* const list = std::get_if<Config::List>(&value.variant());
    if (list == nullptr)
    {
        return 0;
    }
    std::size_t deepest = 0;
    for (const Config::Value& element : *list)
    {
        deepest = std::max(deepest, listDepth(element));
    }
    return deepest + 1;
}

/// The deepest list of the configurations of the extensions of @p simulation (listDepth()).
[[nodiscard]] std::size_t deepestList(const Simulation& simulation)
{
    std::size_t deepest = 0;
    for (const std::shared_ptr<SimulationExtension>& extension :
         simulation.getSimulationExtensions())
    {
        if (extension == nullptr)
        {
            continue;  // SimulationSaver writes none
        }
        const Config config = extension->getConfig();
        for (const std::string& key : config.keySet())
        {
            if (const std::optional<Config::Value> value = config.get(key))
            {
                deepest = std::max(deepest, listDepth(*value));
            }
        }
    }
    return deepest;
}

/// What the loader counts of the simulations of a document, for the bounds that hold for one
/// simulation (see the header): the largest count of any simulation.
struct SimulationCounts
{
    /// The levels of a multi-level wind model the loader takes (windLevelsTaken()).
    std::size_t windLevels{0};
    /// The stored warnings of a flight (storedWarningsCounted()).
    std::size_t storedWarnings{0};
    /// The deepest list of an extension's configuration (deepestList()).
    std::size_t listDepth{0};
};

/// The counts of the simulations of @p document (see SimulationCounts).
[[nodiscard]] SimulationCounts countSimulations(const OpenRocketDocument& document)
{
    SimulationCounts counts;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        counts.windLevels = std::max(
            counts.windLevels, windLevelsTaken(simulation->getOptions().getMultiLevelWindModel()));
        if (const std::shared_ptr<FlightData>& data = simulation->getSimulatedData())
        {
            counts.storedWarnings =
                std::max(counts.storedWarnings, storedWarningsCounted(data->getWarningSet()));
        }
        counts.listDepth = std::max(counts.listDepth, deepestList(*simulation));
    }
    return counts;
}

/// How many decal images the loader registers for the decals of @p rocket: one per name
/// rocket.ork gives the image of a decal (OrkWriter::decalImageName()), which is the name of an
/// entry of the archive (the attachments of an archive are found by their exact name).
[[nodiscard]] std::size_t decalImagesOf(const Rocket& rocket)
{
    std::set<std::string, std::less<>> stored;
    for (const std::string& name : OpenRocketSaver::decalImageNames(rocket))
    {
        std::size_t dropped = 0;
        stored.insert(OrkWriter::decalImageName(name, dropped));
    }
    return stored.size();
}

/// The name of the image of the decal of @p appearance, appended to @p names when there is one.
void addDecalName(std::vector<std::string>& names, const std::optional<Appearance>& appearance)
{
    if (appearance.has_value() && appearance->getTexture().has_value())
    {
        names.push_back(appearance->getTexture()->getImageName());
    }
}

}  // namespace

std::string OpenRocketSaver::documentBeyondLoaderBound(std::size_t bytes)
{
    return std::format(
        "The saved design is {} bytes long; QtRocket loads a design of at most {} bytes and will "
        "not open this file.",
        bytes, GeneralRocketLoader::kMaxDocumentBytes);
}

std::string OpenRocketSaver::componentsBeyondLoaderBound(std::size_t components)
{
    return std::format(
        "The saved rocket has {} components; QtRocket loads at most {} and will ignore the ones "
        "beyond when it opens this file.",
        components, DocumentConfig::kMaxComponents);
}

std::string OpenRocketSaver::nestingBeyondLoaderBound(std::size_t depth)
{
    return std::format(
        "The saved rocket has components {} levels below the rocket; QtRocket loads components at "
        "most {} levels deep and will ignore the deeper ones, with everything in them, when it "
        "opens this file.",
        depth, ComponentHandler::kMaxDepth);
}

std::string OpenRocketSaver::assembliesBeyondLoaderBound(std::size_t assemblies)
{
    return std::format(
        "The saved rocket has {} stages, booster sets and pod sets; QtRocket loads at most {} and "
        "will ignore the ones beyond, with everything in them, when it opens this file.",
        assemblies, DocumentConfig::kMaxAssemblies);
}

std::string OpenRocketSaver::countsBeyondLoaderBound(std::size_t counts, int largest)
{
    return std::format(
        "Counts of instances or of parachute lines in the saved rocket are above what QtRocket "
        "loads: {} above {}, the largest {}. QtRocket refuses such a count when it opens this "
        "file, and the component keeps the count it is made with.",
        counts, DocumentConfig::kMaxCount, largest);
}

std::string OpenRocketSaver::instancesBeyondLoaderBound(std::uint64_t instances)
{
    return std::format(
        "The saved rocket comes to {} component instances over its flight configurations while "
        "it is read; QtRocket loads at most {} and will ignore the components, counts and flight "
        "configurations that would go beyond when it opens this file.",
        instances, DocumentConfig::kMaxInstances);
}

std::string OpenRocketSaver::flightConfigurationsBeyondLoaderBound(std::size_t configurations)
{
    return std::format(
        "The saved design has {} flight configurations; QtRocket loads at most {} and will ignore "
        "the ones beyond, with their motors and simulations, when it opens this file.",
        configurations, DocumentConfig::kMaxFlightConfigurations);
}

std::string OpenRocketSaver::simulationsBeyondLoaderBound(std::size_t simulations)
{
    return std::format(
        "The saved design has {} simulations; QtRocket loads at most {} and will ignore the ones "
        "beyond when it opens this file.",
        simulations, DocumentConfig::kMaxSimulations);
}

std::string OpenRocketSaver::windLevelsBeyondLoaderBound(std::size_t levels)
{
    return std::format(
        "A simulation of the saved design has a wind model of {} levels; QtRocket loads at most {} "
        "levels of a wind model and will ignore the ones beyond when it opens this file.",
        levels, DocumentConfig::kMaxWindLevels);
}

std::string OpenRocketSaver::decalImagesBeyondLoaderBound(std::size_t images)
{
    return std::format(
        "The saved rocket shows {} decal images; QtRocket loads at most {} and will ignore the "
        "decals of the images beyond when it opens this file.",
        images, DocumentConfig::kMaxDecalImages);
}

std::string OpenRocketSaver::storedWarningsBeyondLoaderBound(std::size_t warnings)
{
    return std::format(
        "A simulation of the saved design stores {} warnings with its flight; QtRocket loads at "
        "most {} warnings of a flight and will drop the ones beyond when it opens this file.",
        warnings, DocumentConfig::kMaxWarnings);
}

std::string OpenRocketSaver::listNestingBeyondLoaderBound(std::size_t depth)
{
    return std::format(
        "A simulation extension of the saved design has lists nested {} deep in its "
        "configuration; QtRocket loads lists nested at most {} deep and will ignore the deeper "
        "ones when it opens this file.",
        depth, ConfigHandler::kMaxListDepth);
}

std::vector<std::string> OpenRocketSaver::beyondLoaderBounds(const OpenRocketDocument& document)
{
    const Rocket&          rocket      = document.getRocket();
    const ComponentCounts  components  = countComponents(rocket);
    const SimulationCounts simulations = countSimulations(document);
    const auto             ownConfigurations =
        static_cast<std::size_t>(std::max(rocket.getFlightConfigurationCount(), 0));
    const std::size_t configurations = ownConfigurations + configurationsOfSimulations(document);
    // The loader makes the configurations of the rocket before its components, and those of
    // the simulations after them; every configuration, the default one too, holds every
    // instance.
    const std::uint64_t instances =
        std::max(saturatingProduct(components.instancePeak, std::uint64_t{ownConfigurations} + 1),
                 saturatingProduct(components.instances, std::uint64_t{configurations} + 1));
    const std::size_t decalImages = decalImagesOf(rocket);

    std::vector<std::string> warnings;
    if (components.components > DocumentConfig::kMaxComponents)
    {
        warnings.push_back(componentsBeyondLoaderBound(components.components));
    }
    if (std::cmp_greater(components.deepest, ComponentHandler::kMaxDepth))
    {
        warnings.push_back(nestingBeyondLoaderBound(components.deepest));
    }
    if (components.assemblies > DocumentConfig::kMaxAssemblies)
    {
        warnings.push_back(assembliesBeyondLoaderBound(components.assemblies));
    }
    if (components.countsAboveBound > 0)
    {
        warnings.push_back(
            countsBeyondLoaderBound(components.countsAboveBound, components.largestCount));
    }
    if (instances > DocumentConfig::kMaxInstances)
    {
        warnings.push_back(instancesBeyondLoaderBound(instances));
    }
    if (configurations > DocumentConfig::kMaxFlightConfigurations)
    {
        warnings.push_back(flightConfigurationsBeyondLoaderBound(configurations));
    }
    if (document.getSimulationCount() > DocumentConfig::kMaxSimulations)
    {
        warnings.push_back(simulationsBeyondLoaderBound(document.getSimulationCount()));
    }
    if (simulations.windLevels > DocumentConfig::kMaxWindLevels)
    {
        warnings.push_back(windLevelsBeyondLoaderBound(simulations.windLevels));
    }
    if (decalImages > DocumentConfig::kMaxDecalImages)
    {
        warnings.push_back(decalImagesBeyondLoaderBound(decalImages));
    }
    if (simulations.storedWarnings > DocumentConfig::kMaxWarnings)
    {
        warnings.push_back(storedWarningsBeyondLoaderBound(simulations.storedWarnings));
    }
    if (std::cmp_greater(simulations.listDepth, ConfigHandler::kMaxListDepth))
    {
        warnings.push_back(listNestingBeyondLoaderBound(simulations.listDepth));
    }
    return warnings;
}

std::vector<std::string> OpenRocketSaver::decalImageNames(const Rocket& rocket)
{
    std::vector<std::string> names;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        addDecalName(names, component.getAppearance());
        if (const auto* const inside = dynamic_cast<const InsideColorComponent*>(&component))
        {
            addDecalName(names, inside->getInsideColorComponentHandler().getInsideAppearance());
        }
    }
    std::ranges::sort(names, [](const std::string& a, const std::string& b) {
        return Strings::javaCompareTo(a, b) < 0;
    });
    const auto duplicates = std::ranges::unique(names);
    names.erase(duplicates.begin(), duplicates.end());
    return names;
}

std::string OpenRocketSaver::unsupportedFileType(StorageOptions::FileType fileType)
{
    return std::format(
        "OpenRocketSaver writes the OpenRocket format; the storage options ask for {}.",
        name(fileType));
}

Result<std::string> OpenRocketSaver::save(OpenRocketDocument&   document,
                                          const StorageOptions& options,
                                          const Preferences& preferences, WarningSet& warnings,
                                          ErrorSet& /*errors*/)
{
    if (options.getFileType() != StorageOptions::FileType::OPENROCKET)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT, unsupportedFileType(options.getFileType()));
    }

    OrkWriter writer;

    // Select file version number
    const int fileVersion = calculateNecessaryFileVersion(document, options);

    writer.line("<?xml version='1.0' encoding='utf-8'?>");
    writer.line(std::format(R"(<openrocket version="{}" creator="{}">)",
                            fileVersionText(fileVersion), writer.escape(creatorString())));
    writer.indent();

    // Recursively save the rocket structure
    saveComponents(document.getRocket(), writer, preferences);

    writer.blankLine();

    // Save custom expressions
    saveCustomDatatypes(document, writer);

    // Save all simulations
    saveSimulations(document, options.getSaveSimulationData(), writer, warnings);

    // Save PhotoSettings
    savePhotoSettings(document.getPhotoSettings(), writer);

    // Save document preferences
    saveDocumentPreferences(document, writer);

    writer.outdent();
    writer.line("</openrocket>");

    addSaveWarnings(document, writer, warnings);
    return writer.release();
}

Result<std::string> OpenRocketSaver::save(OpenRocketDocument& document,
                                          const Preferences& preferences, WarningSet& warnings,
                                          ErrorSet& errors)
{
    // The options are read only; nothing a save does changes them.
    return save(document, document.getDefaultStorageOptions(), preferences, warnings, errors);
}

std::int64_t OpenRocketSaver::estimateFileSize(const OpenRocketDocument& document,
                                               const StorageOptions&     options)
{
    std::int64_t size = 0;

    // HOOK(decals): Java's "TODO - estimate decals".

    // Size per component (Java: rocket.iterator(true), the rocket included)
    std::int64_t componentCount = 0;
    for ([[maybe_unused]] const RocketComponent& component : document.getRocket().subtree(true))
    {
        ++componentCount;
    }
    size += componentCount * kBytesPerComponentCompressed;

    // Size per simulation
    size +=
        static_cast<std::int64_t>(document.getSimulationCount()) * kBytesPerSimulationCompressed;

    // Size per flight data point
    std::int64_t pointCount = 0;
    if (options.getSaveSimulationData())
    {
        for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
        {
            const std::shared_ptr<FlightData>& data = simulation->getSimulatedData();
            if (data == nullptr)
            {
                continue;
            }
            for (std::size_t i = 0; i < data->getBranchCount(); ++i)
            {
                // Java's countFlightDataBranchPoints(): the rows of the branch, 0 without types.
                pointCount += static_cast<std::int64_t>(data->getBranch(i).getLength());
            }
        }
    }
    size += pointCount * kBytesPerDatapointCompressed;

    return size;
}

int OpenRocketSaver::calculateNecessaryFileVersion(const OpenRocketDocument& /*document*/,
                                                   const StorageOptions& /*options*/)
{
    // NOTE: Remember to update the supported versions in DocumentConfig as well! (Java; the
    // static_assert above does.)
    return kFileVersion;
}

std::string OpenRocketSaver::fileVersionText(int fileVersion)
{
    return std::format("{}.{}", fileVersion / kFileVersionDivisor,
                       fileVersion % kFileVersionDivisor);
}

}  // namespace QtRocket
