// The designs of tests/data/ork (OpenRocket 0.9.3 to 23.09, the file formats 1.0 to 1.9, plain
// XML, gzip and zip; tests/data/ork/README.md) loaded as files through GeneralRocketLoader, each
// compared with what OpenRocket's GeneralRocketLoader makes of the same file in the same
// environment (DesignFileEnvironment: the bundled motor database, the six component presets of
// the example designs, the bundled simulation extensions, OpenRocket's test preferences).
//
// The expectations are OpenRocket's own answers, printed by the Java probe RecordedProbe of the
// probes of tier 9c, part "recorded-files" and put into RecordedOrkFiles.h by its script. The
// state of a file as loaded, before anything is settled, is these lines:
//
//     version=<n>             the file version OpenRocketHandler leaves in the loading context
//     W <text>                the warnings of the load, in order
//     rocket ... decals=[..]  documentLines() of RocketLoaderTestSupport.h: the rocket, the
//                             stages each flight configuration has active, the simulations
//                             with status, branches and extensions, the storage options, the
//                             saved and undo state, the modification ids, the document
//                             materials, the photo settings and the decal images
//     events rocket=<n>       the change events of the rocket during the load
//     | <line>                describeRocket() of ComponentHandlerTestSupport.h: every component
//                             in depth-first order with its class, its name and what it holds,
//                             the selected and every flight configuration with its id, its
//                             stages and its motors
//     name <n> '<name>'       the name each flight configuration shows
//     simulation <n> config=<n>   the place of each simulation's configuration in the rocket
//     motor ...               every motor mount in every flight configuration: its motor
//                             with manufacturer, designation, digest and ejection delay, or
//                             "none"
//     decal '<name>' ...      every decal image and the number of its bytes, or "unreadable"
//       sim[<n>] ...          describeSimulation() of SimulationTestSupport.h: the options, the
//                             extensions with their configuration, the stored summary, the
//                             stored warnings, and every branch with its types, its events and
//                             a digest of every column
//
// and, from a second load of the file on which nothing else was asked before (an automatic
// dimension stores what it computes, so the order of the questions is part of the state): the
// structure mass and its centre with every stage active, the launch mass and its centre of
// every flight configuration with every stage active, and the length, compared to a relative
// 1e-9 (the positions of the lugs and fins go through sines and cosines).
//
// The component presets. OpenRocket's own preset database holds 5228 presets, among them six
// that these files name and the tests do not have (five parts of SEMROC in
// v1.5-preset-usage.ork and v1.6-preset-usage-decals-first.ork, one of FlisKits in
// v1.7-tube-fin.ork; reading .orc preset files is Milestone 3). With its whole database
// OpenRocket therefore gives one warning for v1.5-preset-usage.ork (the nose cone's BNC-55F,
// which it does not find either) and none for v1.7-tube-fin.ork, which is what
// tests/data/ork/README.md records. With the six presets of the examples, as here, OpenRocket
// gives the six and the one warning that QtRocket gives, and the components keep what the
// files store.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/RecordedOrkFiles.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::DecalImage;
using QtRocket::DocumentLoadingContext;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::GeneralRocketLoader;
using QtRocket::JavaCode;
using QtRocket::LoadedDocument;
using QtRocket::MassCalculator;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketHandler;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ScriptingExtension;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::UnknownSimulationExtension;
using QtRocket::WarningSet;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::kOwnRecordedLines;
using QtRocket::Test::kRecordedOrkFiles;
using QtRocket::Test::LoadEvents;
using QtRocket::Test::RecordedOrkFile;

using Presets = DesignFileEnvironment::Presets;
using Lines   = std::vector<std::string>;

[[nodiscard]] std::filesystem::path orkDir()
{
    return QtRocket::Test::testDataDir() / "ork";
}

/// The design @p file of tests/data/ork, loaded as a file with a loader of @p environment; with
/// @p events the events of the load are counted there.
[[nodiscard]] Result<LoadedDocument> loadRecorded(
    const DesignFileEnvironment& environment, std::string_view file,
    const std::shared_ptr<LoadEvents>& events = nullptr)
{
    const GeneralRocketLoader loader(environment.context(),
                                     events == nullptr ? GeneralRocketLoader::Options{}
                                                       : QtRocket::Test::countingOptions(events));
    return loader.load(orkDir() / file);
}

/// "version=<n>": the file version OpenRocketHandler leaves in a loading context of
/// @p environment when it has read the document @p text (it starts at 0); a failure of the
/// read comes first, on a line of its own.
[[nodiscard]] std::string versionLine(const DesignFileEnvironment& environment,
                                      std::string_view             text)
{
    DocumentLoadingContext                    context = environment.context();
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createEmptyRocket();
    context.setOpenRocketDocument(document.get());
    context.setFileVersion(0);
    OpenRocketHandler            handler(context);
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = QtRocket::stringToBytes(text);
    const Result<void>           read  = QtRocket::SimpleSax::readXml(bytes, handler, warnings);
    return (read ? std::string() : QtRocket::Test::failureLine(read.error())) +
           std::format("version={}\n", context.getFileVersion());
}

/// "name default '<name>'" and "name <n> '<name>'": the name each flight configuration of
/// @p rocket shows (FlightConfiguration::getName()), the default one first and then in the
/// order of Rocket::getIds(); then "simulation <n> config=<n>" for every simulation of
/// @p document: the place of its configuration in that order, or -1.
[[nodiscard]] std::string nameLines(const OpenRocketDocument& document,
                                    const Preferences&        preferences)
{
    const Rocket& rocket = document.getRocket();
    std::string   lines =
        std::format("name default '{}'\n",
                    QtRocket::Test::onOneLine(rocket.getEmptyConfiguration().getName(preferences)));
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    for (std::size_t index = 0; index < ids.size(); index++)
    {
        lines += std::format("name {} '{}'\n", index,
                             QtRocket::Test::onOneLine(
                                 rocket.getFlightConfiguration(ids[index]).getName(preferences)));
    }
    std::size_t number = 0;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        const auto at = std::ranges::find(ids, simulation->getFlightConfigurationId());
        lines += std::format("simulation {} config={}\n", number++,
                             at == ids.end() ? -1 : std::ranges::distance(ids.begin(), at));
    }
    return lines;
}

/// A motor mount of a rocket and its place among the lines of describeRocket() (the rocket is
/// 0).
struct PlacedMount
{
    int               place;
    const MotorMount* mount;
};

/// The components of @p rocket that are motor mounts (MotorMount::isMotorMount()), in the order
/// of the tree.
[[nodiscard]] std::vector<PlacedMount> motorMounts(const Rocket& rocket)
{
    std::vector<PlacedMount> mounts;
    int                      place = 0;
    rocket.forEach([&mounts, &place](const RocketComponent& component) {
        const auto* const mount = dynamic_cast<const MotorMount*>(&component);
        if (mount != nullptr && mount->isMotorMount())
        {
            mounts.push_back({.place = place, .mount = mount});
        }
        place++;
    });
    return mounts;
}

/// What @p motorConfig holds, as a motor line ends: "<manufacturer>|<designation>|<digest>|
/// delay=<ejection delay>", or "none" without a motor.
[[nodiscard]] std::string motorText(const MotorConfiguration& motorConfig)
{
    const auto* const motor = dynamic_cast<const ThrustCurveMotor*>(motorConfig.getMotor().get());
    if (motor == nullptr)
    {
        return "none";
    }
    return std::format("{}|{}|{}|delay={}", motor->getManufacturer().getDisplayName(),
                       motor->getDesignation(), motor->getDigest(),
                       QtRocket::Strings::javaDoubleToString(motorConfig.getEjectionDelay()));
}

/// "motor config=<n> mount=#<place> <motorText()>" for every motor mount of @p rocket in every
/// flight configuration: by configuration in the order of Rocket::getIds(), then by mount in
/// the order of the tree.
[[nodiscard]] std::string motorLines(const Rocket& rocket)
{
    const std::vector<PlacedMount> mounts = motorMounts(rocket);
    std::string                    lines;
    int                            config = 0;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        for (const PlacedMount& placed : mounts)
        {
            lines += std::format("motor config={} mount=#{} {}\n", config, placed.place,
                                 motorText(placed.mount->getMotorConfig(id)));
        }
        config++;
    }
    return lines;
}

/// "decal '<name>' bytes=<n>" for every image of the decal registry of @p document, in the
/// order of the names, or "decal '<name>' unreadable" for one whose bytes cannot be read.
[[nodiscard]] std::string decalLines(const OpenRocketDocument& document)
{
    Lines lines;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        const Result<std::vector<std::byte>> bytes = image->getBytes();
        lines.push_back(std::format(
            "decal '{}' {}\n", image->getName(),
            bytes ? std::format("bytes={}", bytes->size()) : std::string("unreadable")));
    }
    std::ranges::sort(lines);
    std::string text;
    for (const std::string& line : lines)
    {
        text += line;
    }
    return text;
}

/// describeSimulation() of every simulation of @p document, the columns of the stored branches
/// as digests.
[[nodiscard]] std::string simulationLines(const OpenRocketDocument& document)
{
    std::string text;
    std::size_t index = 0;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        text += QtRocket::Test::describeSimulation(index++, *simulation, document.getRocket(),
                                                   {.allIds = false, .digest = true});
    }
    return text;
}

/// @p text as its lines, without the line feeds.
[[nodiscard]] Lines linesOf(std::string_view text)
{
    Lines lines = QtRocket::Strings::split(text, '\n');
    if (!lines.empty() && lines.back().empty())
    {
        lines.pop_back();
    }
    return lines;
}

/// The state of the design @p file as @p environment loads it, in the lines the top of this
/// file lists; a load that fails gives its failure after the version.
[[nodiscard]] Lines stateOf(DesignFileEnvironment& environment, std::string_view file)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const std::string document = QtRocket::Test::documentOfDesignFile(orkDir() / file);
    std::string       text     = versionLine(environment, document);
    const std::shared_ptr<LoadEvents> events = std::make_shared<LoadEvents>();
    const Result<LoadedDocument>      loaded = loadRecorded(environment, file, events);
    if (!loaded)
    {
        return linesOf(text + QtRocket::Test::failureLine(loaded.error()));
    }
    const Rocket& rocket = loaded->document->getRocket();
    text += QtRocket::Test::warningLines(loaded->warnings);
    text += QtRocket::Test::documentLines(*loaded->document, nullptr);
    text += std::format("events rocket={}\n", events->rocket);
    for (const std::string& line : QtRocket::Test::describeRocket(
             rocket, environment.preferences(), QtRocket::Test::knownIds(document)))
    {
        text += "| " + line + "\n";
    }
    text += nameLines(*loaded->document, environment.preferences());
    text += motorLines(rocket);
    text += decalLines(*loaded->document);
    text += simulationLines(*loaded->document);
    return linesOf(text);
}

/// A number of a design, with its name.
struct Number
{
    std::string name;
    double      value;
};

/// What mass and length @p rocket has as it was loaded, asked in this order: the structure
/// mass and its centre with every stage active (a copy of the default flight configuration),
/// then the launch mass and the place of its centre for a copy of every flight configuration
/// with every stage active, then the length of the first copy.
[[nodiscard]] std::vector<Number> numbersOf(const Rocket& rocket)
{
    FlightConfiguration all = rocket.getEmptyConfiguration().clone();
    all.setAllStages();
    const RigidBody     structure = MassCalculator::calculateStructure(all);
    std::vector<Number> numbers{
        {.name = "structure.mass", .value = structure.getMass()},
        {.name = "structure.cm.x", .value = structure.getCM().x},
        {.name = "structure.cm.y", .value = structure.getCM().y},
        {.name = "structure.cm.z", .value = structure.getCM().z},
    };
    int index = 0;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        FlightConfiguration config = rocket.getFlightConfiguration(id).clone();
        config.setAllStages();
        const RigidBody launch = MassCalculator::calculateLaunch(config);
        numbers.push_back(
            {.name = std::format("launch[{}].mass", index), .value = launch.getMass()});
        numbers.push_back(
            {.name = std::format("launch[{}].cm.x", index), .value = launch.getCM().x});
        index++;
    }
    numbers.push_back({.name = "length", .value = all.getLength()});
    return numbers;
}

/// Where @p found is not @p expected: "" when they are the same lines, else the first line
/// that differs, numbered from 1, with both texts, and the two numbers of lines.
[[nodiscard]] std::string firstDifference(std::span<const std::string_view> expected,
                                          const Lines&                      found)
{
    const std::size_t common = std::min(expected.size(), found.size());
    for (std::size_t i = 0; i < common; i++)
    {
        if (expected[i] != found[i])
        {
            return std::format("line {} of {} (found {}):\nexpected: {}\nfound:    {}", i + 1,
                               expected.size(), found.size(), expected[i], found[i]);
        }
    }
    if (expected.size() == found.size())
    {
        return "";
    }
    return std::format("{} lines expected, {} found; the first one more: {}", expected.size(),
                       found.size(),
                       expected.size() > common ? std::string(expected[common]) : found[common]);
}

/// Whether @p found is @p expected to a relative 1e-9 (and to 1e-15 around zero).
[[nodiscard]] bool isCloseTo(double expected, double found) noexcept
{
    return std::abs(found - expected) <= (1e-9 * std::abs(expected)) + 1e-15;
}

/// The numbers of @p found that are not the ones @p expected lists ("<name>=<value>", in the
/// same order), one line each; "" when all of them are.
[[nodiscard]] std::string wrongNumbers(std::span<const std::string_view> expected,
                                       const std::vector<Number>&        found)
{
    if (expected.size() != found.size())
    {
        return std::format("{} numbers expected, {} found", expected.size(), found.size());
    }
    std::string report;
    for (std::size_t i = 0; i < found.size(); i++)
    {
        const std::size_t           equals = expected[i].find('=');
        const std::optional<double> value =
            QtRocket::Strings::javaParseDouble(expected[i].substr(equals + 1));
        if (expected[i].substr(0, equals) != found[i].name || !value.has_value() ||
            !isCloseTo(*value, found[i].value))
        {
            report += std::format("expected {}, found {}={}\n", expected[i], found[i].name,
                                  QtRocket::Strings::javaDoubleToString(found[i].value));
        }
    }
    return report;
}

/// The warnings of a load of @p file with @p environment, as "W <text>" lines; the failure of
/// a load that fails.
[[nodiscard]] Lines warningsOf(const DesignFileEnvironment& environment, std::string_view file)
{
    const Result<LoadedDocument> loaded = loadRecorded(environment, file);
    return linesOf(loaded ? QtRocket::Test::warningLines(loaded->warnings)
                          : QtRocket::Test::failureLine(loaded.error()));
}

/// An environment whose motor database holds nothing: every motor of a design is missing.
/// For the tests that do not look at motors (the bundled database is read once per test
/// process, and ctest runs every test in a process of its own).
class EnvironmentWithoutMotors
{
public:
    explicit EnvironmentWithoutMotors(Presets presets = Presets::EXAMPLES)
      : m_environment(presets, m_motors)
    {
    }

    [[nodiscard]] const DesignFileEnvironment& get() const noexcept { return m_environment; }

private:
    ThrustCurveMotorSetDatabase m_motors;
    DesignFileEnvironment       m_environment;
};

/// The designs of the table whose state as loaded is not the one of the table, each with the
/// first line that differs; "" when every state is.
[[nodiscard]] std::string wrongStates()
{
    std::string report;
    for (const RecordedOrkFile& file : kRecordedOrkFiles)
    {
        DesignFileEnvironment environment;
        const std::string difference = firstDifference(file.state, stateOf(environment, file.file));
        if (!difference.empty())
        {
            report += std::format("{}: {}\n", file.file, difference);
        }
    }
    return report;
}

/// The designs of the table whose masses, centres of mass or length, asked of a freshly loaded
/// document before anything else, are not the ones of the table; "" when all are.
[[nodiscard]] std::string wrongMassesAndLengths()
{
    std::string report;
    for (const RecordedOrkFile& file : kRecordedOrkFiles)
    {
        const DesignFileEnvironment  environment;
        const Result<LoadedDocument> loaded = loadRecorded(environment, file.file);
        const std::string            wrong =
            loaded ? wrongNumbers(file.numbers, numbersOf(loaded->document->getRocket()))
                   : loaded.error().toString();
        if (!wrong.empty())
        {
            report += std::format("{}:\n{}\n", file.file, wrong);
        }
    }
    return report;
}

/// The designs of the table that give other warnings than the table lists when they are loaded
/// without component presets and without a motor; "" when none does.
[[nodiscard]] std::string wrongBareWarnings()
{
    std::string report;
    for (const RecordedOrkFile& file : kRecordedOrkFiles)
    {
        const EnvironmentWithoutMotors environment(Presets::NONE);
        const std::string              difference =
            firstDifference(file.bareWarnings, warningsOf(environment.get(), file.file));
        if (!difference.empty())
        {
            report += std::format("{}: {}\n", file.file, difference);
        }
    }
    return report;
}

// The state of each of the 18 files as loaded is OpenRocket's: the file version, the warnings
// with their texts, every component with what it holds, the flight configurations with their
// ids, names and motors, the simulations with their status, options, extensions, stored
// summaries, warnings, branches, events and columns, the decal images, the storage options and
// the undo state (see the top of this file). One test for all files: each test is a process of
// its own under ctest, and the bundled motor database is read once per process.
TEST(RecordedOrkFiles, LoadIntoOpenRocketsState)
{
    EXPECT_EQ(wrongStates(), "");
}

// Asked of a freshly loaded document before anything else: the structure mass and its centre,
// the launch mass and its centre in every flight configuration, and the length are
// OpenRocket's. They are computed from the values the load left, stale automatic ones
// included, which the settled goldens cannot show.
TEST(RecordedOrkFiles, HaveOpenRocketsMassAndLengthAsLoaded)
{
    EXPECT_EQ(wrongMassesAndLengths(), "");
}

// Without component presets and with a motor database that holds nothing, each file gives the
// warnings OpenRocket gives then, in OpenRocket's order: one "No motor with designation ..."
// per motor the design names (once for a motor that several configurations use), between the
// "No matching ComponentPreset ..." warnings where the motor mount stands among the components
// with a preset.
TEST(RecordedOrkFiles, WarnAsOpenRocketWithoutPresetsAndMotors)
{
    EXPECT_EQ(wrongBareWarnings(), "");
}

/// The names of the designs of the table, sorted.
[[nodiscard]] Lines recordedNames()
{
    Lines names;
    for (const RecordedOrkFile& file : kRecordedOrkFiles)
    {
        names.emplace_back(file.file);
    }
    std::ranges::sort(names);
    return names;
}

/// The names of the designs of tests/data/ork, sorted.
[[nodiscard]] Lines designsOfTheDirectory()
{
    Lines names;
    for (const auto& entry : std::filesystem::directory_iterator(orkDir()))
    {
        if (entry.path().extension() == ".ork")
        {
            names.push_back(entry.path().filename().string());
        }
    }
    std::ranges::sort(names);
    return names;
}

// The table is the directory: every design of tests/data/ork is loaded and compared.
TEST(RecordedOrkFilesTable, HoldsEveryDesignOfTheDirectory)
{
    EXPECT_EQ(recordedNames(), designsOfTheDirectory());
    EXPECT_EQ(recordedNames().size(), 18U);
}

/// The warnings of a load of @p file in the environment of the tests, as texts.
[[nodiscard]] Lines loadWarnings(std::string_view file, Presets presets = Presets::EXAMPLES)
{
    const DesignFileEnvironment  environment(presets);
    const Result<LoadedDocument> loaded = loadRecorded(environment, file);
    if (!loaded)
    {
        return {"FAILED: " + loaded.error().message};
    }
    return QtRocket::Test::warningTexts(loaded->warnings);
}

/// The warnings of a design whose seven <preset> elements name six parts of SEMROC: one per
/// part that no preset database of the tests has (the two centering rings give one).
[[nodiscard]] Lines semrocPresetWarnings()
{
    const std::string_view start = "No matching ComponentPreset for component ";
    const std::string_view maker = " found matching SEMROC Astronautics ";
    Lines                  warnings;
    for (const auto& [component, part] :
         {std::pair<std::string_view, std::string_view>{"Nose cone", "BNC-55F"},
          {"Body tube", "BT-55"},
          {"Centering ring", "RA-5055"},
          {"Inner Tube", "BT-50J"},
          {"Launch lug", "LL-117"},
          {"Parachute", "PN-18"}})
    {
        warnings.push_back(std::format("{}{}{}{}", start, component, maker, part));
    }
    return warnings;
}

// The warnings of the 18 loads, written out (the table holds the same): the numbers the scouts
// of tier 9 measured with OpenRocket, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0 for the
// 17 legacy files in the order of the README and 1 for simplerocket.ork, with three
// exceptions. The designs with presets of SEMROC and FlisKits, which the six presets of the
// tests do not hold (see the top of this file and the next test): 6 for v1.5-preset-usage.ork
// (not 1), 1 for v1.7-tube-fin.ork (not 0), in OpenRocket too when it has those six presets
// only. And the design with the scripts, for which QtRocket has a warning of its own.
TEST(RecordedOrkFilesWarnings, AreTheOnesOpenRocketGives)
{
    const std::string_view missingD7 =
        "No motor with designation 'D7' for manufacturer 'WECO Feuerwerk' found.";
    EXPECT_EQ(loadWarnings("simplerocket.ork"),
              Lines{"Multiple motors with designation 'A8' for manufacturer 'Estes' found, one "
                    "chosen arbitrarily."});
    EXPECT_EQ(loadWarnings("v1.0-roll-stabilized.ork"), Lines{std::string(missingD7)});
    EXPECT_EQ(loadWarnings("v1.4-roll-stabilized.ork"), Lines{std::string(missingD7)});
    EXPECT_EQ(loadWarnings("v1.6-a-simple-model-rocket.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.6-boosted-dart.ork"),
              Lines{"No motor with designation 'J1000-LW' for manufacturer 'Loki Research' "
                    "found."});
    EXPECT_EQ(loadWarnings("v1.6-high-power-airstart.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.6-simulation-listeners.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.6-tarc-payloader.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.6-three-stage-rocket.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.6-apocd.ork"),
              (Lines{"Unknown attributes in element 'ambient', ignoring.",
                     "Unknown attributes in element 'diffuse', ignoring.",
                     "Unknown attributes in element 'specular', ignoring."}));
    // OpenRocket gives the first of the two; the second is QtRocket's, which trusts no script.
    EXPECT_EQ(loadWarnings("v1.7-simulation-extensions-and-scripting.ork"),
              (Lines{"Simulation extension with id "
                     "'info.openrocket.core.simulation.extension.impl.AirStart' not found.",
                     "Untrusted scripts have been disabled.  You need to manually enable them in "
                     "the Simulation options."}));
    EXPECT_EQ(loadWarnings("v1.8-logo-rocket.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.8-parallel-staging-example.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.8-pods-example.ork"), Lines{});
    EXPECT_EQ(loadWarnings("v1.9-chute-release.ork"), Lines{});

    // The designs with presets the tests do not have: OpenRocket's warning for each part, with
    // the six presets of the examples and without any, as OpenRocket gives them in both
    // set-ups (with its whole database it finds them all but the nose cone's).
    const std::string_view tube =
        "No matching ComponentPreset for component Body tube found matching FlisKits BT-50-18";
    EXPECT_EQ(loadWarnings("v1.5-preset-usage.ork"), semrocPresetWarnings());
    EXPECT_EQ(loadWarnings("v1.5-preset-usage.ork", Presets::NONE), semrocPresetWarnings());
    EXPECT_EQ(loadWarnings("v1.6-preset-usage-decals-first.ork"), semrocPresetWarnings());
    EXPECT_EQ(loadWarnings("v1.7-tube-fin.ork"), Lines{std::string(tube)});
    EXPECT_EQ(loadWarnings("v1.7-tube-fin.ork", Presets::NONE), Lines{std::string(tube)});
}

// Without a motor in the database every motor a design names is missing, with OpenRocket's
// warning, once per motor and in the order of the document: the three stages of
// v1.6-three-stage-rocket.ork hold A8 and C6 (the sustainer) and B6 and C6 (the boosters).
TEST(RecordedOrkFilesWarnings, NameEveryMotorThatIsNotFound)
{
    const EnvironmentWithoutMotors environment;
    EXPECT_EQ(warningsOf(environment.get(), "v1.6-three-stage-rocket.ork"),
              (Lines{"W No motor with designation 'A8' for manufacturer 'Estes' found.",
                     "W No motor with designation 'C6' for manufacturer 'Estes' found.",
                     "W No motor with designation 'B6' for manufacturer 'Estes' found."}));
    EXPECT_EQ(warningsOf(environment.get(), "simplerocket.ork"),
              Lines{"W No motor with designation 'A8' for manufacturer 'Estes' found."});
}

/// The extensions of the simulations of @p document, one line per simulation: its name and
/// "<class>:<name of the extension>" for each of its extensions.
[[nodiscard]] Lines extensionsOf(const OpenRocketDocument& document)
{
    Lines lines;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        std::string line = simulation->getName() + ":";
        for (const std::shared_ptr<SimulationExtension>& extension :
             simulation->getSimulationExtensions())
        {
            line += std::format(" [{}:{}]", QtRocket::Test::extensionClassName(*extension),
                                extension->getName());
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

// The three <listener> elements of v1.6-simulation-listeners.ork are JavaCode extensions that
// keep the class names of the file, as in OpenRocket; nothing is run.
TEST(RecordedOrkFilesExtensions, AListenerIsAJavaCodeExtension)
{
    const EnvironmentWithoutMotors environment;
    const Result<LoadedDocument>   loaded =
        loadRecorded(environment.get(), "v1.6-simulation-listeners.ork");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const std::string_view package = "net.sf.openrocket.simulation.listeners.example.";
    EXPECT_EQ(extensionsOf(*loaded->document),
              (Lines{"No controlling:",
                     std::format("Active roll control: [JavaCode:Java code: {}RollControlListener]",
                                 package),
                     std::format("Roll control + air-start: [JavaCode:Java code: "
                                 "{0}RollControlListener] [JavaCode:Java code: {0}AirStart]",
                                 package)}));
    const auto* const code = dynamic_cast<const JavaCode*>(
        loaded->document->getSimulation(1)->getSimulationExtensions().front().get());
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->getClassName(), std::string(package) + "RollControlListener");
}

// v1.7-simulation-extensions-and-scripting.ork names the extension
// net.sf.openrocket.simulation.extension.impl.AirStart, which the loader looks up as
// info.openrocket.core.simulation.extension.impl.AirStart: no provider knows that id (the class
// is example.AirStart today). OpenRocket warns and drops the extension; QtRocket warns and
// keeps it as an UnknownSimulationExtension with its configuration, so that a save can write
// it back (decision D11). The two scripts are stored as enabled. OpenRocket leaves them so,
// because it trusts the roll control script of its own example by its hash; QtRocket trusts no
// script, disables both and says so once (ScriptingExtension::documentLoaded()). (The motor
// database of this test holds nothing: the first warning is the design's motor.)
TEST(RecordedOrkFilesExtensions, AnIdNoProviderKnowsIsKeptAndTheScriptsAreDisabled)
{
    const EnvironmentWithoutMotors environment;
    const Result<LoadedDocument>   loaded =
        loadRecorded(environment.get(), "v1.7-simulation-extensions-and-scripting.ork");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(
        extensionsOf(*loaded->document),
        (Lines{"No controlling:", "Active roll control: [ScriptingExtension:JavaScript script]",
               "Roll control + air-start: [UnknownSimulationExtension:AirStart] "
               "[ScriptingExtension:JavaScript script]"}));
    const std::string_view unknown = "info.openrocket.core.simulation.extension.impl.AirStart";
    EXPECT_EQ(QtRocket::Test::warningTexts(loaded->warnings),
              (Lines{"No motor with designation 'L540' for manufacturer 'HyperTEK' found.",
                     UnknownSimulationExtension::notFoundText(unknown),
                     std::string(ScriptingExtension::kDisabledWarning)}));
    const std::shared_ptr<SimulationExtension>& kept =
        loaded->document->getSimulation(2)->getSimulationExtensions().front();
    EXPECT_EQ(kept->getId(), unknown);
    EXPECT_EQ(kept->getConfig().getDouble("launchAltitude", 0.0), 1000.0);
    const auto* const script = dynamic_cast<const ScriptingExtension*>(
        loaded->document->getSimulation(1)->getSimulationExtensions().front().get());
    ASSERT_NE(script, nullptr);
    EXPECT_FALSE(script->isEnabled());
    EXPECT_EQ(script->getScript().size(), 1233U);
}

/// The decal images of @p document, "<name> <number of bytes>" each, in the order of the names;
/// the failure in place of the number for an image that cannot be read.
[[nodiscard]] Lines decalSizes(const OpenRocketDocument& document)
{
    Lines lines;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        const Result<std::vector<std::byte>> bytes = image->getBytes();
        lines.push_back(image->getName() + " " +
                        (bytes ? std::to_string(bytes->size()) : bytes.error().message));
    }
    std::ranges::sort(lines);
    return lines;
}

// The first entry of v1.6-preset-usage-decals-first.ork is the directory "decals/". OpenRocket
// looks at the first entry only and returns the empty rocket, without a warning (a rocket
// "Rocket" without a stage). QtRocket takes the first entry named *.ork (decision T3): the
// "Preset Usage" example of OpenRocket 13.05 with its simulation, and its two decal images are
// read from the archive, one of them a stored entry.
TEST(RecordedOrkFilesArchive, TheDocumentBehindADirectoryEntryIsLoadedWithItsDecals)
{
    const EnvironmentWithoutMotors environment;
    const Result<LoadedDocument>   loaded =
        loadRecorded(environment.get(), "v1.6-preset-usage-decals-first.ork");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const Rocket& rocket = loaded->document->getRocket();
    EXPECT_EQ(rocket.getName(), "3FNC Using Presets");
    EXPECT_EQ(rocket.getStageCount(), 1U);
    EXPECT_EQ(rocket.getFlightConfigurationCount(), 1U);
    EXPECT_EQ(loaded->document->getSimulationCount(), 1U);
    EXPECT_EQ(decalSizes(*loaded->document),
              (Lines{"decals/beta.png 674", "decals/open.png 1901"}));
}

// A plain XML design looks for its decal images beside the file: v1.6-apocd.ork names five,
// and tests/data/ork has none of them. The load says nothing (as OpenRocket's); reading an
// image fails.
TEST(RecordedOrkFilesArchive, TheDecalsOfAPlainDocumentAreFilesBesideIt)
{
    const EnvironmentWithoutMotors environment;
    const Result<LoadedDocument>   loaded = loadRecorded(environment.get(), "v1.6-apocd.ork");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const std::vector<std::shared_ptr<DecalImage>> images = loaded->document->getDecalList();
    ASSERT_EQ(images.size(), 5U);
    EXPECT_EQ(images.front()->getName(), "decals/Apocalypse_CONE_pointsFrfl.jpg");
    EXPECT_FALSE(images.front()->getBytes().has_value());
    // The file it would be read from: decals/<name> in the directory of the design.
    const std::filesystem::path file =
        images.front()->getDecalFile().value_or(std::filesystem::path());
    EXPECT_EQ(file.filename(), "Apocalypse_CONE_pointsFrfl.jpg");
    EXPECT_EQ(file.parent_path().filename(), "decals");
    std::error_code failure;
    EXPECT_TRUE(std::filesystem::equivalent(file.parent_path().parent_path(), orkDir(), failure))
        << file << " " << failure.message();
}

/// The types of the first stored branch of simulation @p number of @p document that are not
/// built in, "<name>{<symbol>}" each, in the order of the columns.
[[nodiscard]] Lines foreignTypes(const OpenRocketDocument& document, std::size_t number)
{
    Lines                              types;
    const std::shared_ptr<FlightData>& data = document.getSimulation(number)->getSimulatedData();
    if (data == nullptr || data->getBranchCount() == 0)
    {
        return {"no branch"};
    }
    for (const FlightDataType* type : data->getBranch(0).getTypes())
    {
        if (QtRocket::Test::describeFlightDataType(*type).contains(",-,"))
        {
            types.push_back(std::format("{}{{{}}}", type->getName(), type->getSymbol()));
        }
    }
    return types;
}

// A stored column whose name today's OpenRocket does not know ("Position parallel to wind" of
// the formats 1.0 to 1.7, "Propellant mass" of 1.7) becomes a type of that name with the
// symbol "Unknown" and no unit, without a warning of the load, and keeps its values.
TEST(RecordedOrkFilesFlightData, AStoredTypeNobodyKnowsIsAnUnknownType)
{
    const EnvironmentWithoutMotors environment;
    const Result<LoadedDocument>   first =
        loadRecorded(environment.get(), "v1.0-roll-stabilized.ork");
    ASSERT_TRUE(first.has_value()) << first.error().toString();
    EXPECT_EQ(foreignTypes(*first->document, 0), Lines{"Position parallel to wind{Unknown}"});

    const Result<LoadedDocument> tube = loadRecorded(environment.get(), "v1.7-tube-fin.ork");
    ASSERT_TRUE(tube.has_value()) << tube.error().toString();
    EXPECT_EQ(foreignTypes(*tube->document, 0),
              (Lines{"Position parallel to wind{Unknown}", "Propellant mass{Unknown}"}));
    const FlightDataBranch& branch =
        tube->document->getSimulation(0)->getSimulatedData()->getBranch(0);
    EXPECT_EQ(branch.getLength(), 257U);
    EXPECT_EQ(branch.getTypes().size(), 54U);
}

// Every line of a state that is not OpenRocket's has its reason, and is a line the table
// holds: a deviation that the loader no longer makes fails here.
TEST(RecordedOrkFilesTable, EveryOwnLineHasItsReason)
{
    for (const QtRocket::Test::OwnRecordedLine& own : kOwnRecordedLines)
    {
        EXPECT_FALSE(own.why.empty()) << own.file;
        EXPECT_NE(own.openRocket, own.qtRocket) << own.file;
    }
}

// The states as the Java probe prints them, for a comparison with its output (the probes of
// tier 9c, part "recorded-files": scripts/compare.sh).
TEST(RecordedOrkFilesTable, DISABLED_PrintsTheStates)
{
    for (const RecordedOrkFile& file : kRecordedOrkFiles)
    {
        DesignFileEnvironment environment;
        std::cout << "=== " << file.file << "\n";
        for (const std::string& line : stateOf(environment, file.file))
        {
            std::cout << line << "\n";
        }
        std::cout << "#numbers\n";
        const Result<LoadedDocument> loaded = loadRecorded(environment, file.file);
        if (loaded)
        {
            for (const Number& number : numbersOf(loaded->document->getRocket()))
            {
                std::cout << number.name << "="
                          << QtRocket::Strings::javaDoubleToString(number.value) << "\n";
            }
        }
    }
}

}  // namespace
