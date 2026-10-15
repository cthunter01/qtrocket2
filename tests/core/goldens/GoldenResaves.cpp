#include "goldens/GoldenResaves.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
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

/// The lines of @p text, each without its '\n'.
[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view text)
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

}  // namespace

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
    auto environment = std::make_unique<DesignFileEnvironment>(Presets::NONE, testRocketMotors());
    if (prepare != nullptr)
    {
        prepare(environment->preferences());
    }
    const GeneralRocketLoader loader(environment->context());
    Result<LoadedDocument>    loaded = loader.load(QtRocket::Test::goldensDir() / input.resave);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    return LoadedResave{.environment = std::move(environment), .loaded = std::move(*loaded)};
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
    const std::vector<std::string_view> oursLines  = linesOf(oursText);
    const std::vector<std::string_view> theirLines = linesOf(theirsText);
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
    if (comparison.differing > kMaxReportedLines)
    {
        comparison.differences +=
            std::format("... and {} more lines\n", comparison.differing - kMaxReportedLines);
    }
    return comparison;
}

}  // namespace QtRocket::Test
