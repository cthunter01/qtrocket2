#include "goldens/GoldenResaves.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "goldens/AutomaticDimensions.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenSettings.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;
using Presets = DesignFileEnvironment::Presets;

// =============================================================================== the options

/// A number as the golden documents hold it: a JSON number, or the name of a value JSON has
/// none for (Json.java of the harness).
[[nodiscard]] json goldenForm(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    return value;
}

/// The average wind model @p wind as SimulationDumper.options() writes it.
[[nodiscard]] json averageWindDocument(const PinkNoiseWindModel& wind)
{
    return json{{"average", goldenForm(wind.getAverage())},
                {"standardDeviation", goldenForm(wind.getStandardDeviation())},
                {"turbulenceIntensity", goldenForm(wind.getTurbulenceIntensity())},
                {"direction", goldenForm(wind.getDirection())}};
}

/// The multi-level wind model @p wind as SimulationDumper.options() writes it.
[[nodiscard]] json multiLevelWindDocument(const MultiLevelPinkNoiseWindModel& wind)
{
    json levels = json::array();
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : wind.getLevels())
    {
        levels.push_back(json{{"altitude", goldenForm(level->getAltitude())},
                              {"speed", goldenForm(level->getSpeed())},
                              {"direction", goldenForm(level->getDirection())},
                              {"standardDeviation", goldenForm(level->getStandardDeviation())}});
    }
    const bool msl =
        wind.getAltitudeReference() == MultiLevelPinkNoiseWindModel::AltitudeReference::MSL;
    return json{{"altitudeReference", msl ? "MSL" : "AGL"}, {"levels", std::move(levels)}};
}

/// The options @p options as the golden harness writes the options of a simulation
/// (SimulationDumper.options(): the getters, the enums by their names).
[[nodiscard]] json optionsDocument(const SimulationOptions& options)
{
    return json{
        {"launchRodLength", goldenForm(options.getLaunchRodLength())},
        {"launchIntoWind", options.getLaunchIntoWind()},
        {"launchRodAngle", goldenForm(options.getLaunchRodAngle())},
        {"launchRodDirection", goldenForm(options.getLaunchRodDirection())},
        {"windModelType", windModelTypeName(options.getWindModelType())},
        {"averageWind", averageWindDocument(options.getAverageWindModel())},
        {"multiLevelWind", multiLevelWindDocument(options.getMultiLevelWindModel())},
        {"launchAltitude", goldenForm(options.getLaunchAltitude())},
        {"launchLatitude", goldenForm(options.getLaunchLatitude())},
        {"launchLongitude", goldenForm(options.getLaunchLongitude())},
        {"geodeticComputation", name(options.getGeodeticComputation())},
        {"isaAtmosphere", options.isIsaAtmosphere()},
        {"launchTemperature", goldenForm(options.getLaunchTemperature())},
        {"launchPressure", goldenForm(options.getLaunchPressure())},
        {"launchRelativeHumidity", goldenForm(options.getLaunchRelativeHumidity())},
        {"timeStep", goldenForm(options.getTimeStep())},
        {"maxSimulationTime", goldenForm(options.getMaxSimulationTime())},
        {"maximumStepAngle", goldenForm(options.getMaximumStepAngle())},
        {"randomSeed", options.getRandomSeed()},
        {"randomSeedFixed", options.isRandomSeedFixed()},
        {"gravityModelType", gravityModelTypeName(options.getGravityModelType())},
        {"constantGravity", goldenForm(options.getConstantGravity())},
        {"stepperMethod", simulationStepperMethodName(options.getSimulationStepperMethodChoice())},
        {"recoverySpeedWarning", goldenForm(options.getRecoverySpeedWarning())},
        {"drogueLowSpeedWarning", goldenForm(options.getDrogueLowSpeedWarning())},
        {"recoveryDrogueMainHighSpeedWarning",
         goldenForm(options.getRecoveryDrogueMainHighSpeedWarning())},
        {"recoveryDrogueMainLowSpeedWarning",
         goldenForm(options.getRecoveryDrogueMainLowSpeedWarning())},
        {"hasDragLookup", options.hasDragLookup()},
        {"hasStabilityLookup", options.hasStabilityLookup()},
    };
}

/// The launch rod direction of options that launch into the wind, as those of all fifty
/// simulations do, is the direction of the wind and not the direction stored for the rod
/// (which compareStoredRodDirection() holds). With the average wind it is a number of the
/// file, the direction of that wind, and is compared exactly. With a multi-level wind it is
/// computed with mathematical functions, the direction of the model's wind at the launch site
/// (an arc tangent of a sine and a cosine), and is compared within this relative tolerance.
constexpr double kComputedDirectionRelative = 1e-12;

/// The value of the golden options whose JSON pointer is this one.
constexpr std::string_view kLaunchRodDirection = "/launchRodDirection";

/// Whether the option @p actual is the golden @p expected: the same value, or, for a number
/// that is @p computed, one within kComputedDirectionRelative of it.
[[nodiscard]] bool isTheOption(const json& expected, const json& actual, bool computed)
{
    if (computed && expected.is_number() && actual.is_number())
    {
        const double golden = expected.get<double>();
        return std::abs(actual.get<double>() - golden) <=
               kComputedDirectionRelative * std::abs(golden);
    }
    return expected == actual;
}

// ==================================================== the comparison of a loaded test rocket

/// The entry of the component at @p path in the document @p geometry, for changing it; null
/// when the document has none.
[[nodiscard]] json* componentEntry(json& geometry, std::string_view path)
{
    for (json& component : geometry.at("components"))
    {
        if (component.at("path").get<std::string>() == path)
        {
            return &component;
        }
    }
    return nullptr;
}

/// The <conditions> of the simulations of the re-save @p file, in the order of the file (an
/// empty node for a simulation without one).
[[nodiscard]] std::vector<pugi::xml_node> savedConditions(const pugi::xml_document& file)
{
    std::vector<pugi::xml_node> conditions;
    for (const pugi::xml_node simulation :
         file.child("openrocket").child("simulations").children("simulation"))
    {
        conditions.push_back(simulation.child("conditions"));
    }
    return conditions;
}

/// Compares the direction that @p options store for the launch rod with the
/// <launchroddirection> of @p conditions, the element of their simulation in the file. Options
/// that launch into the wind give the direction of the wind in its place, and so do the golden
/// options, so the stored one is asked of a copy that does not launch into the wind. The file
/// has degrees, and the loader stores d * 2 pi / 360 reduced to one turn
/// (SimulationConditionsHandler and SimulationOptions::setLaunchRodDirection(), as
/// OpenRocket's): the same operations here, so exactly that double. Returns the number of
/// directions compared, 1, or 0 when the file has none; a mismatch is a line in @p report.
[[nodiscard]] int compareStoredRodDirection(std::string& report, const pugi::xml_node& conditions,
                                            const SimulationOptions& options)
{
    const std::optional<double> degrees =
        QtRocket::Strings::javaParseDouble(conditions.child_value("launchroddirection"));
    if (!degrees)
    {
        report += "  <launchroddirection>: not in the file, or no number\n";
        return 0;
    }
    SimulationOptions notIntoTheWind(options);
    notIntoTheWind.setLaunchIntoWind(false);
    const double expected = QtRocket::MathUtil::reduce2Pi(*degrees * 2.0 * std::numbers::pi / 360);
    const double stored   = notIntoTheWind.getLaunchRodDirection();
    if (stored != expected)
    {
        report += std::format("  <launchroddirection>: expected {}, got {}\n", expected, stored);
    }
    return 1;
}

/// The status OpenRocket's reload gives a simulation of a re-save: none has flight data, so it
/// cannot be run when its flight configuration has no motor, and is not simulated yet otherwise.
[[nodiscard]] Simulation::Status statusAfterTheLoad(bool hasMotors)
{
    return hasMotors ? Simulation::Status::NOT_SIMULATED : Simulation::Status::CANT_RUN;
}

/// Whether the flight configuration the golden simulation document @p golden names (by its
/// index among the configurations of @p geometry, with its id) has a motor; @p problem says
/// why when the two documents do not agree on the configuration.
[[nodiscard]] bool goldenConfigurationHasMotors(const json& golden, const json& geometry,
                                                std::string& problem)
{
    const json&       named          = golden.at("flightConfiguration");
    const json&       configurations = geometry.at("configurations");
    const std::size_t index          = named.at("index").get<std::size_t>();
    if (index >= configurations.size() || configurations.at(index).at("id") != named.at("id"))
    {
        problem += "  flightConfiguration: not a configuration of geometry.json\n";
        return false;
    }
    return configurations.at(index).at("hasMotors").get<bool>();
}

/// Compares the loaded @p simulation with its golden document @p golden: the name, the flight
/// configuration, the status after the load, no extension and no flight data, and the options;
/// and the stored direction of the launch rod with @p conditions, the <conditions> of the
/// simulation in the file. Adds what it found to @p compared, under @p what.
void compareSimulation(ComparedSimulations& compared, const std::string& what, const json& golden,
                       const json& geometry, Simulation& simulation,
                       const pugi::xml_node& conditions)
{
    std::string       report;
    const bool        hasMotors = goldenConfigurationHasMotors(golden, geometry, report);
    const auto        status    = simulation.getStatus();
    const std::string id        = simulation.getFlightConfigurationId().toString();
    compared.simulations++;
    compared.withoutMotors += hasMotors ? 0 : 1;
    compared.notSimulated += status == Simulation::Status::NOT_SIMULATED ? 1 : 0;
    compared.cannotRun += status == Simulation::Status::CANT_RUN ? 1 : 0;
    if (simulation.getName() != golden.at("name").get<std::string>())
    {
        report += std::format("  name: expected {}, got \"{}\"\n", golden.at("name").dump(),
                              simulation.getName());
    }
    if (id != golden.at("flightConfiguration").at("id").get<std::string>())
    {
        report += std::format("  flightConfiguration.id: expected {}, got \"{}\"\n",
                              golden.at("flightConfiguration").at("id").dump(), id);
    }
    if (status != statusAfterTheLoad(hasMotors))
    {
        report += std::format("  status: expected {}, got {}\n",
                              name(statusAfterTheLoad(hasMotors)), name(status));
    }
    if (simulation.getSimulationExtensions().size() != golden.at("extensions").size() ||
        simulation.hasSimulationData())
    {
        report += "  extensions or flight data: the file has neither\n";
    }
    const OptionsComparison options = compareOptions(golden.at("options"), simulation.getOptions());
    compared.optionValues += options.values;
    report += options.report;
    compared.intoTheWind += simulation.getOptions().getLaunchIntoWind() ? 1 : 0;
    compared.rodDirections +=
        compareStoredRodDirection(report, conditions, simulation.getOptions());
    if (!report.empty())
    {
        compared.problems += std::format("{}:\n{}", what, report);
    }
}

// ===================================================================== the text of a re-save

/// What a line is made of when its numbers are taken out: the text around them, with one '#' in
/// the place of each number, and the numbers.
struct NumbersOfALine
{
    std::string                   text;
    std::vector<std::string_view> numbers;
};

[[nodiscard]] bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// Whether @p c is an ASCII letter or digit, or a '.': a number does not start right after one.
[[nodiscard]] bool continuesAWord(char c) noexcept
{
    return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.';
}

/// The length of the digits of @p line from @p at on.
[[nodiscard]] std::size_t digitsAt(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end < line.size() && isDigit(line[end]))
    {
        ++end;
    }
    return end - at;
}

/// The length of the number that starts at @p at of @p line (see sameByValue()), 0 for none.
[[nodiscard]] std::size_t numberAt(std::string_view line, std::size_t at) noexcept
{
    if (at > 0 && continuesAWord(line[at - 1]))
    {
        return 0;
    }
    std::size_t end = at;
    if (end < line.size() && line[end] == '-')
    {
        ++end;
    }
    const std::size_t integer = digitsAt(line, end);
    if (integer == 0)
    {
        return 0;
    }
    end += integer;
    if (end + 1 < line.size() && line[end] == '.' && isDigit(line[end + 1]))
    {
        end += 1 + digitsAt(line, end + 1);
    }
    if (end < line.size() && line[end] == 'E')
    {
        const std::size_t sign     = end + 1 < line.size() && line[end + 1] == '-' ? 1 : 0;
        const std::size_t exponent = digitsAt(line, end + 1 + sign);
        if (exponent > 0)
        {
            end += 1 + sign + exponent;
        }
    }
    return end - at;
}

/// @p line with its numbers taken out.
[[nodiscard]] NumbersOfALine numbersOf(std::string_view line)
{
    NumbersOfALine split;
    for (std::size_t at = 0; at < line.size();)
    {
        const std::size_t length = numberAt(line, at);
        if (length == 0)
        {
            split.text += line[at];
            ++at;
            continue;
        }
        split.text += '#';
        split.numbers.push_back(line.substr(at, length));
        at += length;
    }
    return split;
}

/// Whether the number texts @p a and @p b read as the same double, bit for bit.
[[nodiscard]] bool sameNumber(std::string_view a, std::string_view b)
{
    const std::optional<double> x = Strings::javaParseDouble(a);
    const std::optional<double> y = Strings::javaParseDouble(b);
    return x.has_value() && y.has_value() &&
           std::bit_cast<std::uint64_t>(*x) == std::bit_cast<std::uint64_t>(*y);
}

/// @p line as a report shows it: a carriage return as "\r", a missing line as "(none)".
[[nodiscard]] std::string shown(const std::string_view* line)
{
    if (line == nullptr)
    {
        return "(none)";
    }
    std::string text;
    for (const char c : *line)
    {
        if (c == '\r')
        {
            text += "\\r";
        }
        else
        {
            text += c;
        }
    }
    return text;
}

/// Counts and reports in @p comparison whether one of @p ours and @p theirs ends with a line feed
/// and the other does not (lineViewsOf() gives the same lines for both).
void compareTheEnds(std::string_view ours, std::string_view theirs, OrkTextComparison& comparison)
{
    const bool oursEnds   = ours.ends_with('\n');
    const bool theirsEnds = theirs.ends_with('\n');
    if (oursEnds == theirsEnds)
    {
        return;
    }
    ++comparison.differing;
    if (comparison.differing <= kMaxReportedLines)
    {
        constexpr std::string_view kWith    = "a line feed at the end";
        constexpr std::string_view kWithout = "no line feed at the end";
        comparison.differences +=
            std::format("the end:\n  QtRocket:   {}\n  OpenRocket: {}\n",
                        oursEnds ? kWith : kWithout, theirsEnds ? kWith : kWithout);
    }
}

// ================================================================================= the loads

/// A new environment for a load of a test rocket: the seven motors of TestRockets, no component
/// presets, and OpenRocket's test preferences, which @p prepare changes first when it is given.
[[nodiscard]] std::unique_ptr<DesignFileEnvironment> testRocketEnvironment(
    PreparePreferences prepare)
{
    auto environment = std::make_unique<DesignFileEnvironment>(Presets::NONE, testRocketMotors());
    if (prepare != nullptr)
    {
        prepare(environment->preferences());
    }
    return environment;
}

/// @p loaded, a load with a loader of @p environment, moved out with that environment; or its
/// Error. (@p loaded is taken by reference: GCC's -Wmaybe-uninitialized, in the ci-gcc build,
/// takes the vector of warnings of a Result moved into a parameter for uninitialized once this
/// file inlines it.)
[[nodiscard]] Result<LoadedResave> asLoadedResave(
    std::unique_ptr<DesignFileEnvironment> environment, Result<LoadedDocument>& loaded)
{
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    return LoadedResave{.environment = std::move(environment), .loaded = std::move(*loaded)};
}

}  // namespace

std::vector<std::string_view> lineViewsOf(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty())
    {
        const std::size_t end = text.find('\n');
        lines.push_back(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    return lines;
}

std::string changedLines(std::string_view before, std::string_view after)
{
    const std::vector<std::string_view> a = lineViewsOf(before);
    const std::vector<std::string_view> b = lineViewsOf(after);
    if (a.size() != b.size())
    {
        return std::format("{} lines -> {} lines\n", a.size(), b.size());
    }
    std::string changes;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            const std::size_t from = a[i].find_first_not_of(' ');
            const std::size_t to   = b[i].find_first_not_of(' ');
            changes +=
                std::format("{} -> {}\n", a[i].substr(from == std::string_view::npos ? 0 : from),
                            b[i].substr(to == std::string_view::npos ? 0 : to));
        }
    }
    return changes;
}

// ================================================================================= the loads

/// The motor database of the loads: exactly the seven motors of TestRockets, added in this
/// order. Made once per test process; the motors are found by their digests.
const ThrustCurveMotorSetDatabase& testRocketMotors()
{
    static const ThrustCurveMotorSetDatabase kMotors = [] {
        ThrustCurveMotorSetDatabase database;
        for (const std::shared_ptr<const ThrustCurveMotor>& motor :
             {QtRocket::Test::motorA8(), QtRocket::Test::motorA10(), QtRocket::Test::motorB4(),
              QtRocket::Test::motorC6(), QtRocket::Test::motorD21(), QtRocket::Test::motorM1350(),
              QtRocket::Test::motorG77()})
        {
            database.addMotor(motor);
        }
        return database;
    }();
    return kMotors;
}

/// Loads the re-save of @p input against the seven motors of TestRockets, in an environment of
/// its own: the document as loaded, with the warnings of the load. @p prepare, when given,
/// changes the preference store of the environment first (OpenRocket's test preferences).
Result<LoadedResave> loadResave(const GoldenInput& input, PreparePreferences prepare)
{
    std::unique_ptr<DesignFileEnvironment> environment = testRocketEnvironment(prepare);
    const GeneralRocketLoader              loader(environment->context());
    Result<LoadedDocument> loaded = loader.load(QtRocket::Test::goldensDir() / input.resave);
    return asLoadedResave(std::move(environment), loaded);
}

Result<LoadedResave> loadTestRocketBytes(std::vector<std::byte> bytes, PreparePreferences prepare)
{
    std::unique_ptr<DesignFileEnvironment> environment = testRocketEnvironment(prepare);
    const GeneralRocketLoader              loader(environment->context());
    Result<LoadedDocument>                 loaded = loader.load(std::move(bytes));
    return asLoadedResave(std::move(environment), loaded);
}

/// The inputs of the manifest that are rockets of TestRockets.java, in its order; the reason
/// in @p problem when the manifest cannot be read.
std::vector<GoldenInput> testRocketInputs(std::string& problem)
{
    std::vector<GoldenInput>     inputs;
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    if (!manifest)
    {
        problem = manifest.error().toString();
        return inputs;
    }
    for (const GoldenInput& input : manifest->inputs)
    {
        if (input.kind == kTestRocketKind)
        {
            inputs.push_back(input);
        }
    }
    return inputs;
}

// =============================================================================== the options

/// Compares @p options with the golden "options" @p expected of a simulation document, value
/// for value: every value of the golden object, however deep, has to be there and equal (they
/// are settings: the numbers exactly), and @p options must give no value beyond them.
OptionsComparison compareOptions(const json& expected, const SimulationOptions& options)
{
    OptionsComparison result;
    const json        golden = expected.flatten();
    const json        actual = optionsDocument(options).flatten();
    const bool        computedDirection =
        options.getLaunchIntoWind() && options.getWindModelType() == WindModelType::MULTI_LEVEL;
    for (const auto& [pointer, value] : golden.items())
    {
        result.values++;
        if (!actual.contains(pointer))
        {
            result.report += std::format("  options{}: not compared\n", pointer);
        }
        else if (!isTheOption(value, actual.at(pointer),
                              computedDirection && pointer == kLaunchRodDirection))
        {
            result.report += std::format("  options{}: expected {}, got {}\n", pointer,
                                         value.dump(), actual.at(pointer).dump());
        }
    }
    for (const auto& [pointer, value] : actual.items())
    {
        if (!golden.contains(pointer))
        {
            result.report += std::format("  options{}: not in the golden options\n", pointer);
        }
    }
    return result;
}

// ==================================================== the comparison of a loaded test rocket

void changeToTheReload(std::string_view input, json& geometry, ReloadChanges& changes)
{
    for (const ReloadChange& change : kReloadChanges)
    {
        if (change.input != input)
        {
            continue;
        }
        json* const              component = componentEntry(geometry, change.path);
        const json::json_pointer pointer{std::string{change.value}};
        if (component == nullptr || !component->contains(pointer) ||
            !component->at(pointer).is_number() ||
            component->at(pointer).get<double>() != change.built)
        {
            changes.problems += std::format("{} {} {}: the golden value is not {}\n", input,
                                            change.path, change.value, change.built);
            continue;
        }
        component->at(pointer) = change.reloaded;
        changes.values++;
    }
}

ResaveRun compareLoadedResave(const LoadedResave& resave, const DesignGoldens& goldens,
                              const pugi::xml_document& file)
{
    ResaveRun run;
    run.golden                 = goldenCounts(goldens);
    Rocket& rocket             = resave.loaded.document->getRocket();
    run.loadWarnings           = static_cast<int>(resave.loaded.warnings.size());
    const Result<int> settling = settleAutomaticDimensions(rocket);
    if (!settling)
    {
        run.problem = settling.error().toString();
        return run;
    }
    run.settlingPasses = *settling;
    run.design         = compareLoadedDesign(rocket, resave.loaded.warnings, goldens,
                                             resave.environment->preferences());
    run.saved          = savedSettingsCounts(file);
    run.settings       = compareSavedSettings(rocket, goldens.geometry, file);
    return run;
}

void addResaveRun(ComparedResaves& compared, std::string_view name, const ResaveRun& run)
{
    compared.loadWarnings += run.loadWarnings;
    compared.settlingPasses += run.settlingPasses;
    compared.golden += run.golden;
    compared.compared += run.design.compared;
    compared.saved += run.saved;
    compared.settings += run.settings.compared;
    if (!run.problem.empty() || !run.design.report.empty() || !run.settings.report.empty())
    {
        compared.problems +=
            std::format("{}: {}\n{}{}", name, run.problem, run.design.report, run.settings.report);
    }
    if (run.design.compared != run.golden)
    {
        compared.problems += std::format("{}: compared {}; the files hold {}\n", name,
                                         toText(run.design.compared), toText(run.golden));
    }
    if (run.settings.compared != run.saved)
    {
        compared.problems += std::format("{}: compared {}; the re-save holds {}\n", name,
                                         toText(run.settings.compared), toText(run.saved));
    }
}

DesignCounts designCountsOfTheTestRockets()
{
    return DesignCounts{.components         = 151,
                        .ids                = 151,
                        .details            = 2323,
                        .configurations     = 47,
                        .motors             = 48,
                        .instanceEntries    = 571,
                        .instances          = 831,
                        .loadWarnings       = 0,
                        .massConfigurations = 47,
                        .aero               = {.configurations = 47,
                                               .points         = 1175,
                                               .components     = 9400,
                                               .worstCPs       = 329,
                                               .warnings       = 294}};
}

SettingsCounts settingsCountsOfTheTestRockets()
{
    return SettingsCounts{.deployEvents             = 9,
                          .deploymentConfigurations = 0,
                          .separationEvents         = 8,
                          .separationConfigurations = 1,
                          .motorMounts              = 16,
                          .motors                   = 48,
                          .ignitionConfigurations   = 48,
                          .presets                  = 0};
}

void compareSimulations(ComparedSimulations& compared, const GoldenInput& input,
                        const LoadedResave& resave, const json& geometry,
                        const pugi::xml_document& file)
{
    const std::vector<std::shared_ptr<Simulation>> simulations =
        resave.loaded.document->getSimulations();
    const std::vector<pugi::xml_node> conditions = savedConditions(file);
    if (simulations.size() != input.simulations.size() || conditions.size() != simulations.size())
    {
        compared.problems += std::format(
            "{}: {} simulations, the manifest lists {} and the file has {}\n", input.name,
            simulations.size(), input.simulations.size(), conditions.size());
        return;
    }
    for (std::size_t i = 0; i < simulations.size(); i++)
    {
        const std::string  what   = std::format("{} simulation {}", input.name, i);
        const Result<json> golden = loadGoldenJson(input.simulations.at(i).json);
        if (!golden)
        {
            compared.problems += std::format("{}: {}\n", what, golden.error().toString());
            continue;
        }
        compareSimulation(compared, what, *golden, geometry, *simulations.at(i), conditions.at(i));
    }
}

// ===================================================================== the text of a re-save

bool sameByValue(std::string_view ours, std::string_view theirs)
{
    const NumbersOfALine a = numbersOf(ours);
    const NumbersOfALine b = numbersOf(theirs);
    if (a.text != b.text || a.numbers.size() != b.numbers.size())
    {
        return false;
    }
    bool written = false;
    for (std::size_t i = 0; i < a.numbers.size(); ++i)
    {
        if (a.numbers[i] == b.numbers[i])
        {
            continue;
        }
        if (!sameNumber(a.numbers[i], b.numbers[i]))
        {
            return false;
        }
        written = true;
    }
    return written;
}

std::string withoutCreator(std::string_view text)
{
    constexpr std::string_view kAttribute = "creator=\"";
    std::string                result(text);
    const std::size_t          start = result.find(kAttribute);
    if (start == std::string::npos)
    {
        return result;
    }
    const std::size_t value = start + kAttribute.size();
    const std::size_t end   = result.find('"', value);
    if (end == std::string::npos)
    {
        return result;
    }
    return result.replace(value, end - value, "CREATOR");
}

std::string rocketElementOf(std::string_view text)
{
    constexpr std::string_view kOpen  = "  <rocket>\n";
    constexpr std::string_view kClose = "  </rocket>\n";
    std::size_t open = text.starts_with(kOpen) ? 0 : text.find("\n" + std::string(kOpen));
    if (open == std::string_view::npos)
    {
        return {};
    }
    open += text[open] == '\n' ? 1U : 0U;
    const std::size_t close = text.find("\n" + std::string(kClose), open);
    if (close == std::string_view::npos)
    {
        return {};
    }
    return std::string(text.substr(open, close + 1 + kClose.size() - open));
}

OrkTextComparison compareOrkTexts(std::string_view ours, std::string_view theirs)
{
    const std::string                   oursText   = withoutCreator(ours);
    const std::string                   theirsText = withoutCreator(theirs);
    const std::vector<std::string_view> oursLines  = lineViewsOf(oursText);
    const std::vector<std::string_view> theirLines = lineViewsOf(theirsText);
    OrkTextComparison                   comparison;
    comparison.lines = std::max(oursLines.size(), theirLines.size());
    for (std::size_t i = 0; i < comparison.lines; ++i)
    {
        const std::string_view* const a = i < oursLines.size() ? &oursLines[i] : nullptr;
        const std::string_view* const b = i < theirLines.size() ? &theirLines[i] : nullptr;
        if (a != nullptr && b != nullptr && *a == *b)
        {
            continue;
        }
        if (a != nullptr && b != nullptr && sameByValue(*a, *b))
        {
            ++comparison.byValue;
            continue;
        }
        ++comparison.differing;
        if (comparison.differing <= kMaxReportedLines)
        {
            comparison.differences += std::format("line {}:\n  QtRocket:   {}\n  OpenRocket: {}\n",
                                                  i + 1, shown(a), shown(b));
        }
    }
    compareTheEnds(oursText, theirsText, comparison);
    if (comparison.differing > kMaxReportedLines)
    {
        comparison.differences +=
            std::format("... and {} more lines\n", comparison.differing - kMaxReportedLines);
    }
    return comparison;
}

namespace
{

/// The length of a UUID as java.util.UUID.toString() writes it.
constexpr std::size_t kUuidLength = 36;

/// Whether @p c can be part of a UUID's text.
[[nodiscard]] bool isUuidChar(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
}

/// Whether @p text, kUuidLength characters, is a UUID as Java writes one: hexadecimal digits in
/// groups of 8, 4, 4, 4 and 12, separated by '-'.
[[nodiscard]] bool isUuidText(std::string_view text) noexcept
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash != (text[i] == '-') || !isUuidChar(text[i]))
        {
            return false;
        }
    }
    return text.size() == kUuidLength;
}

/// The line @p line without the spaces in front.
[[nodiscard]] std::string_view withoutIndentation(std::string_view line)
{
    const std::size_t start = line.find_first_not_of(' ');
    return start == std::string_view::npos ? std::string_view{} : line.substr(start);
}

/// The most pairs of lines lineChanges() compares (memory of four bytes each).
constexpr std::size_t kMaxComparedPairs = 25'000'000;

}  // namespace

std::string withDrawnIdsNumbered(std::string_view text, std::string_view original)
{
    const std::set<Uuid>       known = knownIds(original);
    std::map<std::string, int> numbers;
    std::string                result;
    std::size_t                i = 0;
    while (i < text.size())
    {
        std::size_t end = i;
        while (end < text.size() && isUuidChar(text[end]))
        {
            ++end;
        }
        if (end == i)
        {
            result += text[i++];
            continue;
        }
        const std::string_view run = text.substr(i, end - i);
        if (isUuidText(run) && !known.contains(Uuid::javaFromString(run).value_or(Uuid::nil())))
        {
            const auto [at, added] =
                numbers.try_emplace(std::string(run), static_cast<int>(numbers.size()) + 1);
            result += std::format("drawn-id-{}", at->second);
        }
        else
        {
            result += run;
        }
        i = end;
    }
    return result;
}

std::string lineChanges(std::span<const std::string_view> before,
                        std::span<const std::string_view> after)
{
    // The lines both start and end with stay out of the table.
    std::size_t start = 0;
    while (start < before.size() && start < after.size() && before[start] == after[start])
    {
        ++start;
    }
    std::size_t endBefore = before.size();
    std::size_t endAfter  = after.size();
    while (endBefore > start && endAfter > start && before[endBefore - 1] == after[endAfter - 1])
    {
        --endBefore;
        --endAfter;
    }
    const std::size_t n = endBefore - start;
    const std::size_t m = endAfter - start;
    if ((n + 1) * (m + 1) > kMaxComparedPairs)
    {
        return std::format("{} lines -> {} lines\n", before.size(), after.size());
    }
    // common[i * (m + 1) + j]: the length of a longest common subsequence of the lines of the
    // middle of before from i and of the middle of after from j.
    std::vector<std::uint32_t> common((n + 1) * (m + 1), 0);
    for (std::size_t i = n; i-- > 0;)
    {
        for (std::size_t j = m; j-- > 0;)
        {
            common[(i * (m + 1)) + j] =
                before[start + i] == after[start + j]
                    ? common[((i + 1) * (m + 1)) + j + 1] + 1
                    : std::max(common[((i + 1) * (m + 1)) + j], common[(i * (m + 1)) + j + 1]);
        }
    }
    std::string changes;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < n || j < m)
    {
        if (i < n && j < m && before[start + i] == after[start + j])
        {
            ++i;
            ++j;
        }
        else if (j == m ||
                 (i < n && common[((i + 1) * (m + 1)) + j] >= common[(i * (m + 1)) + j + 1]))
        {
            changes += std::format("- {}\n", withoutIndentation(before[start + i++]));
        }
        else
        {
            changes += std::format("+ {}\n", withoutIndentation(after[start + j++]));
        }
    }
    return changes;
}

std::string lineChanges(std::string_view before, std::string_view after)
{
    const std::vector<std::string_view> a = lineViewsOf(before);
    const std::vector<std::string_view> b = lineViewsOf(after);
    return lineChanges(a, b);
}

std::string archiveChanges(std::span<const std::byte> first, std::span<const std::byte> second,
                           std::size_t& entries)
{
    const Result<ZipArchive> a = ZipArchive::fromBytes(first);
    const Result<ZipArchive> b = ZipArchive::fromBytes(second);
    if (!a || !b)
    {
        return "an archive cannot be read\n";
    }
    entries += a->entries().size();
    if (a->entries().size() != b->entries().size())
    {
        return std::format("{} entries -> {} entries\n", a->entries().size(), b->entries().size());
    }
    std::string changes;
    for (std::size_t i = 0; i < a->entries().size(); ++i)
    {
        const ZipArchive::Entry& before = a->entries()[i];
        const ZipArchive::Entry& after  = b->entries()[i];
        if (before.name != after.name)
        {
            changes += std::format("entry {}: {} -> {}\n", i, before.name, after.name);
        }
        else if (before.data != after.data)
        {
            changes +=
                std::format("{} changed\n{}", before.name,
                            changedLines(bytesToString(before.data), bytesToString(after.data)));
        }
    }
    return changes;
}

// =============================== the re-saves of the legacy files and the coverage documents

namespace
{

constexpr std::string_view kResavesContext = R"(manifest.json "resaves")";

/// The string @p key of @p object into @p target; a missing one, or one that is no string, is
/// the failure. With @p optional, a missing one or a null leaves @p target empty.
[[nodiscard]] Result<void> readResaveString(const json& object, std::string_view key,
                                            std::string& target, bool optional = false)
{
    const auto value = object.find(key);
    if (value == object.end() || value->is_null())
    {
        if (optional)
        {
            return {};
        }
        return fail(ErrorCode::PARSE,
                    std::format(R"({}: missing string "{}")", kResavesContext, key));
    }
    if (!value->is_string())
    {
        return fail(ErrorCode::PARSE,
                    std::format(R"({}: "{}" is not a string)", kResavesContext, key));
    }
    target = value->get<std::string>();
    return {};
}

/// The strings of the list @p key of @p object into @p target; a list of other values, or
/// anything but a list or a null, is the failure. A missing list or a null leaves @p target
/// empty.
[[nodiscard]] Result<void> readResaveStrings(const json& object, std::string_view key,
                                             std::vector<std::string>& target)
{
    const auto value = object.find(key);
    if (value == object.end() || value->is_null())
    {
        return {};
    }
    if (!value->is_array() ||
        !std::ranges::all_of(*value, [](const json& item) { return item.is_string(); }))
    {
        return fail(ErrorCode::PARSE,
                    std::format(R"({}: "{}" is not a list of strings)", kResavesContext, key));
    }
    for (const json& item : *value)
    {
        target.push_back(item.get<std::string>());
    }
    return {};
}

/// The boolean @p key of @p object, false when it is missing; anything but a boolean is the
/// failure.
[[nodiscard]] Result<bool> readResaveFlag(const json& object, std::string_view key)
{
    const auto value = object.find(key);
    if (value == object.end())
    {
        return false;
    }
    if (!value->is_boolean())
    {
        return fail(ErrorCode::PARSE,
                    std::format(R"({}: "{}" is not a boolean)", kResavesContext, key));
    }
    return value->get<bool>();
}

/// One entry of "resaves.inputs".
[[nodiscard]] Result<ResaveInput> parseResaveInput(const json& object)
{
    if (!object.is_object())
    {
        return fail(ErrorCode::PARSE,
                    std::format("{}: an input is not an object", kResavesContext));
    }
    ResaveInput input;
    for (const auto& [key, target] :
         {std::pair{"name", &input.name}, std::pair{"kind", &input.kind},
          std::pair{"source", &input.source}, std::pair{"resave", &input.resave}})
    {
        if (Result<void> read = readResaveString(object, key, *target); !read)
        {
            return std::unexpected(read.error());
        }
    }
    if (input.kind != kLegacyKind && input.kind != kCoverageKind)
    {
        return fail(ErrorCode::PARSE, std::format("{}: {} is of the unknown kind {}",
                                                  kResavesContext, input.name, input.kind));
    }
    Result<bool> moved    = readResaveFlag(object, "documentEntryMovedFirst");
    Result<bool> withData = readResaveFlag(object, "saveSimulationData");
    if (!moved || !withData)
    {
        return std::unexpected(!moved ? moved.error() : withData.error());
    }
    input.documentEntryMovedFirst = *moved;
    input.saveSimulationData      = *withData;
    for (const Result<void>& read :
         {readResaveString(object, "sourceSha256", input.sourceSha256, true),
          readResaveString(object, "archiveError", input.archiveError, true),
          readResaveStrings(object, "archiveEntries", input.archiveEntries)})
    {
        if (!read)
        {
            return std::unexpected(read.error());
        }
    }
    return input;
}

/// One entry of "resaves.withData".
[[nodiscard]] Result<ResaveWithData> parseResaveWithData(const json& object)
{
    ResaveWithData data;
    if (!object.is_object())
    {
        return fail(ErrorCode::PARSE,
                    std::format("{}: a re-save with data is not an object", kResavesContext));
    }
    for (const auto& [key, target] :
         {std::pair{"input", &data.input}, std::pair{"resave", &data.resave}})
    {
        if (Result<void> read = readResaveString(object, key, *target); !read)
        {
            return std::unexpected(read.error());
        }
    }
    return data;
}

/// The list @p key of @p resaves, each entry parsed by @p parse into @p target.
template <typename T, typename Parse>
[[nodiscard]] Result<void> readResaveList(const json& resaves, std::string_view key,
                                          std::vector<T>& target, Parse parse)
{
    const auto list = resaves.find(key);
    if (list == resaves.end() || !list->is_array())
    {
        return fail(ErrorCode::PARSE,
                    std::format(R"({}: missing list "{}")", kResavesContext, key));
    }
    for (const json& item : *list)
    {
        Result<T> parsed = parse(item);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        target.push_back(std::move(*parsed));
    }
    return {};
}

}  // namespace

Result<ManifestResaves> parseManifestResaves(const json& manifest)
{
    const auto resaves = manifest.find("resaves");
    if (resaves == manifest.end() || !resaves->is_object())
    {
        return fail(ErrorCode::PARSE, R"(manifest.json: missing object "resaves")");
    }
    ManifestResaves result;
    if (Result<void> read =
            readResaveStrings(*resaves, "legacyPresetDigests", result.legacyPresetDigests);
        !read)
    {
        return std::unexpected(read.error());
    }
    if (Result<void> read =
            readResaveList(*resaves, "withData", result.withData, parseResaveWithData);
        !read)
    {
        return std::unexpected(read.error());
    }
    if (Result<void> read = readResaveList(*resaves, "inputs", result.inputs, parseResaveInput);
        !read)
    {
        return std::unexpected(read.error());
    }
    return result;
}

const Result<ManifestResaves>& manifestResaves()
{
    static const Result<ManifestResaves> kResaves = [] -> Result<ManifestResaves> {
        const Result<json> manifest = loadGoldenJson("manifest.json");
        if (!manifest)
        {
            return std::unexpected(manifest.error());
        }
        return parseManifestResaves(*manifest);
    }();
    return kResaves;
}

std::vector<ResaveInput> resaveInputsOf(std::string_view kind, std::string& problem)
{
    const Result<ManifestResaves>& resaves = manifestResaves();
    if (!resaves)
    {
        problem = resaves.error().toString();
        return {};
    }
    std::vector<ResaveInput> inputs;
    std::ranges::copy_if(resaves->inputs, std::back_inserter(inputs),
                         [kind](const ResaveInput& input) { return input.kind == kind; });
    return inputs;
}

const ResaveWithData* resaveWithDataOf(std::string_view input)
{
    const Result<ManifestResaves>& resaves = manifestResaves();
    if (!resaves)
    {
        return nullptr;
    }
    const auto found = std::ranges::find(resaves->withData, input, &ResaveWithData::input);
    return found == resaves->withData.end() ? nullptr : &*found;
}

std::filesystem::path legacyFileOf(const ResaveInput& input)
{
    constexpr std::string_view kTestData = "tests/data/";
    const std::string_view     source    = input.source;
    return testDataDir() /
           pathFromUtf8(source.starts_with(kTestData) ? source.substr(kTestData.size()) : source);
}

}  // namespace QtRocket::Test
