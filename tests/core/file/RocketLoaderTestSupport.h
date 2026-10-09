#pragma once

// What the tests of the top of the .ork loader share (OpenRocketHandler, OpenRocketLoader,
// GeneralRocketLoader): the environment a loader is given, a count of the events of a load,
// and the state of a loaded document in the lines the Java probe of these tests prints
// (TopProbe.java of the probes of tier 9c, part "loader"). Test-only.

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "file/RocketLoaderCases.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// @p text on one line, as the probe prints a text: a line feed as "\n" and a carriage return
/// as "\r", spelled out.
[[nodiscard]] inline std::string onOneLine(std::string_view text)
{
    std::string line;
    for (const char c : text)
    {
        if (c == '\n')
        {
            line += "\\n";
        }
        else if (c == '\r')
        {
            line += "\\r";
        }
        else
        {
            line += c;
        }
    }
    return line;
}

/// The lines for a failure and for the warnings of a load, as the probe prints them: "FAILED
/// <code>: <message>" and one "W <text>" per warning.
[[nodiscard]] inline std::string failureLine(const Error& error)
{
    return std::format("FAILED {}: {}\n", toString(error.code), onOneLine(error.message));
}
[[nodiscard]] inline std::string warningLines(const WarningSet& warnings)
{
    std::string lines;
    for (const Warning& warning : warnings)
    {
        lines += std::format("W {}\n", onOneLine(warning.toString()));
    }
    return lines;
}

/// The environment a loader under test is given, with everything the handlers ask for:
/// - the preference store OpenRocket's tests read (storeJavaTestPreferences()), which names no
///   default material;
/// - OpenRocket's own materials as the application's;
/// - a ScriptedMotorFinder, which finds no motor until a test gives it one;
/// - the bundled extension providers;
/// - no component presets.
/// The members are declared so that the preference store outlives everything, the documents a
/// test loads into its own variables excepted: declare those after the environment.
class LoaderEnvironment
{
public:
    LoaderEnvironment() : m_extensions(SimulationExtensionRegistry::bundled())
    {
        storeJavaTestPreferences(m_preferences);
        addBuiltinMaterials(m_materials);
        m_context.setMotorFinder(&m_motorFinder);
        m_context.setApplicationMaterials(&m_materials);
        m_context.setPreferences(&m_preferences);
        m_context.setSimulationExtensionRegistry(&m_extensions);
    }
    ~LoaderEnvironment() = default;

    // The context points at the members.
    LoaderEnvironment(const LoaderEnvironment&)            = delete;
    LoaderEnvironment& operator=(const LoaderEnvironment&) = delete;
    LoaderEnvironment(LoaderEnvironment&&)                 = delete;
    LoaderEnvironment& operator=(LoaderEnvironment&&)      = delete;

    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_context; }
    [[nodiscard]] ScriptedMotorFinder&    motorFinder() noexcept { return m_motorFinder; }
    [[nodiscard]] InMemoryPreferences&    preferences() noexcept { return m_preferences; }

    /// Makes the motor finder answer every query with a motor, so that a flight configuration
    /// with a <motor> has one (the probe finds the motors in OpenRocket's database).
    void findEveryMotor()
    {
        m_motorFinder.setMotor(makeEmbeddedTestMotor("A8", 10.0, "test-digest"));
    }

private:
    InMemoryPreferences         m_preferences;
    MaterialStorage             m_materials;
    ScriptedMotorFinder         m_motorFinder;
    SimulationExtensionRegistry m_extensions;
    DocumentLoadingContext      m_context;
};

/// How many events a document emitted while it was loaded.
struct LoadEvents
{
    /// The change events of the rocket.
    int rocket{0};
    /// documentChanged() of the document.
    int document{0};
    /// undoRedoChanged() of the document.
    int undoRedo{0};
};

/// Options of a loader that count into @p events what the document of a load emits from the
/// moment it is made. The listeners stay connected to the document, so @p events is shared.
[[nodiscard]] inline GeneralRocketLoader::Options countingOptions(
    const std::shared_ptr<LoadEvents>& events)
{
    GeneralRocketLoader::Options options;
    options.beforeReading = [events](const DocumentLoadingContext& context) {
        // The connections are the document's from here on.
        OpenRocketDocument& document = *context.getOpenRocketDocument();
        static_cast<void>(document.getRocket().addComponentChangeListener(
            [events](const ComponentChangeEvent& /*event*/) { events->rocket++; }));
        static_cast<void>(document.documentChanged().connect(
            [events](const DocumentChangeEvent& /*event*/) { events->document++; }));
        static_cast<void>(document.undoRedoChanged().connect([events] { events->undoRedo++; }));
    };
    return options;
}

/// "true" or "false", as Java prints a boolean.
[[nodiscard]] inline std::string_view javaBoolean(bool value) noexcept
{
    return value ? "true" : "false";
}

/// The name of a material type as Java's Material.Type prints itself: "Bulk", "Surface",
/// "Line", "Custom".
[[nodiscard]] inline std::string_view materialTypeText(Material::Type type) noexcept
{
    switch (type)
    {
        case Material::Type::BULK:
            return "Bulk";
        case Material::Type::SURFACE:
            return "Surface";
        case Material::Type::LINE:
            return "Line";
        default:
            return "Custom";
    }
}

/// The extensions of @p simulation, as the probe prints them: the simple names of their Java
/// classes, a scripting extension with "(enabled)" or "(disabled)", separated by spaces. An
/// extension of an unknown id, which OpenRocket does not keep, is "UnknownSimulationExtension",
/// also when its id is one of OpenRocket's package.
[[nodiscard]] inline std::string extensionNames(const Simulation& simulation)
{
    std::string names;
    for (const std::shared_ptr<SimulationExtension>& extension :
         simulation.getSimulationExtensions())
    {
        if (!names.empty())
        {
            names += ' ';
        }
        const std::string id = extension->getId();
        if (const auto* const script = dynamic_cast<const ScriptingExtension*>(extension.get()))
        {
            names += script->isEnabled() ? "ScriptingExtension(enabled)"
                                         : "ScriptingExtension(disabled)";
        }
        else if (const std::size_t dot = id.rfind('.');
                 dynamic_cast<const UnknownSimulationExtension*>(extension.get()) == nullptr &&
                 dot != std::string::npos && id.starts_with("info.openrocket.core."))
        {
            names += id.substr(dot + 1);
        }
        else
        {
            names += "UnknownSimulationExtension";
        }
    }
    return names;
}

/// The lines of the flight configurations of @p rocket: the default one and then each in the
/// order of Rocket::getIds(), with a 't' or an 'f' for every stage number of the rocket
/// (FlightConfiguration::isStageActive()).
[[nodiscard]] inline std::string configurationLines(const Rocket& rocket)
{
    const auto activeness = [&rocket](const FlightConfiguration& config) {
        std::string active;
        for (int stage = 0; std::cmp_less(stage, rocket.getStageCount()); stage++)
        {
            active += config.isStageActive(stage) ? 't' : 'f';
        }
        return active;
    };
    std::string lines =
        std::format("config default active={}\n", activeness(rocket.getEmptyConfiguration()));
    int index = 0;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        lines += std::format("config {} active={}\n", index++,
                             activeness(rocket.getFlightConfiguration(id)));
    }
    return lines;
}

/// "default" when the selected configuration of @p rocket is the default one, else its index
/// in Rocket::getIds().
[[nodiscard]] inline std::string selectedConfiguration(const Rocket& rocket)
{
    const FlightConfiguration& selected = rocket.getSelectedConfiguration();
    if (&selected == &rocket.getEmptyConfiguration())
    {
        return "default";
    }
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    const auto                               at  = std::ranges::find(ids, selected.getId());
    return at == ids.end() ? "-1" : std::to_string(std::ranges::distance(ids.begin(), at));
}

/// The document materials of @p document as the probe prints OpenRocket's: "name:Type:density"
/// of each, separated by spaces, in the order of the database of all of them
/// (DocumentPreferences.getAllMaterials(), here MaterialStorage::allMaterials()), which is the
/// order a save writes them in (Material::compareTo(): by name, then by density, and materials
/// that compare equal in the order in which the document got them).
[[nodiscard]] inline std::string documentMaterials(const OpenRocketDocument& document)
{
    std::string text;
    for (const Material& material : document.getDocumentMaterials().allMaterials())
    {
        text += std::format("{}{}:{}:{}", text.empty() ? "" : " ", material.getName(),
                            materialTypeText(material.getType()),
                            Strings::javaDoubleToString(material.getDensity()));
    }
    return text;
}

/// The state of the loaded @p document in the lines of the probe: the rocket, its flight
/// configurations, the simulations, the storage options, the saved and undo state, how the
/// rocket's modification ids stand to each other, the document materials, the events of the
/// load (when @p events is given) and the custom expressions, photo settings and decal
/// images. Each line ends with a line feed.
[[nodiscard]] inline std::string documentLines(OpenRocketDocument& document,
                                               const LoadEvents*   events)
{
    const Rocket& rocket     = document.getRocket();
    std::size_t   components = 0;
    rocket.forEach([&components](const RocketComponent& /*component*/) { components++; });
    std::string lines =
        std::format("rocket '{}' stages={} components={} configurations={} selected={}\n",
                    onOneLine(rocket.getName()), rocket.getStageCount(), components,
                    rocket.getFlightConfigurationCount(), selectedConfiguration(rocket));
    lines += configurationLines(rocket);
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        const std::shared_ptr<FlightData>& data = simulation->getSimulatedData();
        lines += std::format("sim '{}' status={} branches={} ext=[{}]\n",
                             onOneLine(simulation->getName()), name(simulation->getStatus()),
                             data == nullptr ? "none" : std::to_string(data->getBranchCount()),
                             extensionNames(*simulation));
    }
    const StorageOptions& storage = document.getDefaultStorageOptions();
    lines += std::format("storage fileType={} saveSimulationData={} explicitlySet={}\n",
                         name(storage.getFileType()), javaBoolean(storage.getSaveSimulationData()),
                         javaBoolean(storage.isExplicitlySet()));
    const OpenRocketDocument::UndoDetail undo = document.getUndoDetail();
    lines += std::format(
        "saved={} undoAvailable={} redoAvailable={} undoDescription={} history={} position={} "
        "clean={} file={}\n",
        javaBoolean(document.isSaved()), javaBoolean(document.isUndoAvailable()),
        javaBoolean(document.isRedoAvailable()), document.getUndoDescription().value_or("null"),
        undo.descriptions.size(), undo.position, javaBoolean(undo.clean),
        document.getFile().has_value() ? pathToUtf8(*document.getFile()) : "null");
    const ModId mod = rocket.getModId();
    lines += std::format("modids mass={} aero={} tree={} functional={} snapshot={}\n",
                         javaBoolean(rocket.getMassModId() == mod),
                         javaBoolean(rocket.getAerodynamicModId() == mod),
                         javaBoolean(rocket.getTreeModId() == mod),
                         javaBoolean(rocket.getFunctionalModId() == mod), javaBoolean(undo.clean));
    lines += std::format("materials=[{}]\n", documentMaterials(document));
    if (events != nullptr)
    {
        lines += std::format("events rocket={} document={} undoRedo={} eventsEnabled={}\n",
                             events->rocket, events->document, events->undoRedo,
                             javaBoolean(rocket.isEventsEnabled()));
    }
    std::string photo;
    for (const auto& [key, value] : document.getPhotoSettings())
    {
        photo += std::format("{}{}={}", photo.empty() ? "" : ", ", key, value);
    }
    std::vector<std::string> decals;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        decals.push_back(image->getName());
    }
    std::ranges::sort(decals);
    std::string decalNames;
    for (const std::string& decal : decals)
    {
        decalNames += (decalNames.empty() ? "" : ", ") + decal;
    }
    lines += std::format("expressions={} photo={{{}}} decals=[{}]\n",
                         document.getCustomExpressions().size(), photo, decalNames);
    return lines;
}

/// What a load gave, in the lines of the probe: the failure, or the warnings and the state of
/// the document (documentLines()).
[[nodiscard]] inline std::string loadLines(const Result<LoadedDocument>& loaded,
                                           const LoadEvents*             events)
{
    if (!loaded)
    {
        return failureLine(loaded.error());
    }
    return warningLines(loaded->warnings) + documentLines(*loaded->document, events);
}

/// Loads @p document, the text of a design file, from memory with a loader of @p environment
/// that counts the events, and gives loadLines() of it.
[[nodiscard]] inline std::string loadAndDescribe(LoaderEnvironment& environment,
                                                 std::string_view   document)
{
    const std::shared_ptr<LoadEvents> events = std::make_shared<LoadEvents>();
    const GeneralRocketLoader         loader(environment.context(), countingOptions(events));
    const Result<LoadedDocument>      loaded = loader.load(stringToBytes(document));
    return loadLines(loaded, events.get());
}

/// The document of the case @p name of kLoadCases.
/// @throws BugError when the table has no such case
[[nodiscard]] inline std::string_view documentOfLoadCase(std::string_view name)
{
    // Through a span: an iterator of an array is a pointer with some standard libraries.
    const std::span<const RocketLoaderCase> cases(kLoadCases);
    const auto found = std::ranges::find(cases, name, &RocketLoaderCase::name);
    if (found == cases.end())
    {
        bug("no case " + std::string(name));
    }
    return found->document;
}

/// loadAndDescribe() in an environment of its own whose motor finder finds every motor.
[[nodiscard]] inline std::string loadLinesOf(std::string_view document)
{
    LoaderEnvironment environment;
    environment.findEveryMotor();
    return loadAndDescribe(environment, document);
}

/// What OpenRocketHandler alone makes of @p document, read by SimpleSax as bytes into a new
/// document, in the lines of the probe: the failure, the warnings, and "version=<n>", the file
/// version the context has afterwards (it starts at 0).
[[nodiscard]] inline std::string rootHandlerLinesOf(std::string_view document)
{
    HandlerFixture fixture;
    fixture.context().setFileVersion(0);
    OpenRocketHandler            handler(fixture.context());
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(document);
    const Result<void>           read  = SimpleSax::readXml(bytes, handler, warnings);
    return (read ? std::string() : failureLine(read.error())) + warningLines(warnings) +
           std::format("version={}\n", fixture.context().getFileVersion());
}

/// The cases of @p cases for which @p linesOf does not give what the case expects, each with
/// its name, its document and the lines expected and found; empty when every case agrees.
template <class LinesOf>
[[nodiscard]] std::string failedLoaderCases(std::span<const RocketLoaderCase> cases,
                                            LinesOf                           linesOf)
{
    std::string report;
    for (const RocketLoaderCase& one : cases)
    {
        const std::string found = linesOf(one.document);
        if (found != one.expected)
        {
            report += std::format("case {}\n{}\nexpected:\n{}found:\n{}\n", one.name, one.document,
                                  one.expected, found);
        }
    }
    return report;
}

/// The cases of @p cases, each followed by what @p linesOf gives for it, as the probe prints
/// them: "=== <name>" and the lines with @p mode and a space in front. The text a DISABLED_
/// test prints for a comparison with the probe's output.
template <class LinesOf>
[[nodiscard]] std::string printedLoaderCases(std::span<const RocketLoaderCase> cases, char mode,
                                             LinesOf linesOf)
{
    std::string text;
    for (const RocketLoaderCase& one : cases)
    {
        text += std::format("=== {}\n", one.name);
        const std::string lines = linesOf(one.document);
        std::size_t       begin = 0;
        while (begin < lines.size())
        {
            const std::size_t end = lines.find('\n', begin);
            text += std::format("{} {}\n", mode, lines.substr(begin, end - begin));
            begin = end == std::string::npos ? lines.size() : end + 1;
        }
    }
    return text;
}

}  // namespace QtRocket::Test
