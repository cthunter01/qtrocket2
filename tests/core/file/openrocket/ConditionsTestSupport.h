#pragma once

// What the tests of the handlers of a simulation's <conditions> share: a way to run a
// <conditions> element through SimulationConditionsHandler under the preferences OpenRocket's
// tests run under, and a description of the outcome in the notation of the Java probe the
// expectations come from. Test-only.
//
// The probe is CondProbe (run 9b, part S1): it runs the same element through OpenRocket's
// SimulationConditionsHandler alone, as runConditions() does here, and prints the warnings, the
// exception, what the parent's closeElement() was given, the flight configuration id and every
// option. describeOutcome() prints the same, with one difference: a line that says what an
// empty <conditions/> also gives (kBaselineOptions, "closed=conditions {} []" and the error
// id) is left out, so that a case shows what its elements changed and nothing else.

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"
#include "TestTempDir.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// What stands for the directory of a test's files in the XML of a case and in its outcome.
inline constexpr std::string_view kDirectoryMark = "{DIR}";
/// What stands for the current directory of the process in an outcome.
inline constexpr std::string_view kCurrentDirectoryMark = "{CWD}";
/// The directory under a test's directory that runConditions() makes the current one.
inline constexpr std::string_view kWorkingDirectoryName = "cwd";

/// The options an empty <conditions/> gives under OpenRocket's test preferences, as CondProbe
/// prints them (the case "s1: empty conditions"): one line per group of options.
inline constexpr std::array<std::string_view, 10> kBaselineOptions{
    "rod=0.0 intoWind=false angle=0.0 dir=0.0",
    "wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)",
    "site=0.0,0.0,0.0 geo=FLAT",
    "stepper=RK4 dt=0.0 tmax=1200.0 maxAngle=0.05235987755982988",
    "seedFixed=false",
    "atmosphere=isa:false T=288.15 p=101325.0 hum=0.0",
    "gravity=WGS/9.807",
    "thresholds=20.0/3.048/30.48/15.24",
    "drag=path:null table:null rows:null",
    "stability=path:null table:null rows:null",
};

/// @p text with every @p from replaced by @p to.
[[nodiscard]] inline std::string replaceAll(std::string text, std::string_view from,
                                            std::string_view to)
{
    for (std::size_t at = text.find(from); at != std::string::npos;
         at             = text.find(from, at + to.size()))
    {
        text.replace(at, from.size(), to);
    }
    return text;
}

/// @p path as UTF-8 text with '/' between its elements on every platform.
[[nodiscard]] inline std::string genericUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.generic_u8string();
    return {text.begin(), text.end()};
}

/// @p text with the directory @p directory written as kDirectoryMark, the current directory of
/// the process as kCurrentDirectoryMark and every backslash as a slash, so that a text that
/// names files reads the same on every platform and in every directory. An empty @p directory
/// stands for no directory. One of the two directories may lie in the other (the current one in
/// the test's, or the system's temporary directory in the current one), and where the temporary
/// directory is reached through a link the current directory is spelled without it: the longer
/// name is replaced first, so that neither is taken for the start of the other.
[[nodiscard]] inline std::string neutralPaths(std::string                  text,
                                              const std::filesystem::path& directory)
{
    text                         = replaceAll(std::move(text), "\\", "/");
    const std::string current    = genericUtf8(absolutePath({}));
    const std::string ofTheTest  = directory.empty() ? std::string() : genericUtf8(directory);
    const bool        testsFirst = ofTheTest.size() >= current.size();
    if (testsFirst && !ofTheTest.empty())
    {
        text = replaceAll(std::move(text), ofTheTest, kDirectoryMark);
    }
    text = replaceAll(std::move(text), current, kCurrentDirectoryMark);
    if (!testsFirst && !ofTheTest.empty())
    {
        text = replaceAll(std::move(text), ofTheTest, kDirectoryMark);
    }
    return text;
}

/// A double as Java's string concatenation prints it.
[[nodiscard]] inline std::string javaNumber(double value)
{
    return Strings::javaDoubleToString(value);
}

/// A lookup table as CondProbe prints one: whether it has angles of attack, its Mach range and
/// the value of every column of @p columns halfway through that range at no angle of attack.
[[nodiscard]] inline std::string describeTable(const std::shared_ptr<const MachAoALookup>& table,
                                               std::initializer_list<std::string_view>     columns)
{
    if (table == nullptr)
    {
        return "null";
    }
    const double mid = (table->getMinMach() + table->getMaxMach()) / 2;
    std::string  text =
        std::format("[aoa={} mach={}..{}", table->hasAoA(), javaNumber(table->getMinMach()),
                    javaNumber(table->getMaxMach()));
    for (const std::string_view column : columns)
    {
        text += std::format(" {}={}", column, javaNumber(table->interpolate(mid, 0, column)));
    }
    return text + "]";
}

/// The file of a lookup table as CondProbe prints one: in full (neutralPaths() then names the
/// directory).
[[nodiscard]] inline std::string describeLookupPath(
    const std::optional<std::filesystem::path>& path)
{
    return path.has_value() ? genericUtf8(*path) : "null";
}

/// The CSV rows of a lookup table as CondProbe prints them: joined with '|', a tab written as
/// "<TAB>".
[[nodiscard]] inline std::string describeLookupRows(
    const std::optional<std::vector<std::string>>& rows)
{
    return rows.has_value()
               ? std::format("[{}]", replaceAll(Strings::join("|", *rows), "\t", "<TAB>"))
               : "null";
}

/// Every option of @p options as CondProbe prints them: the ten lines of kBaselineOptions,
/// with the doubles as Java prints them. The rod direction is the stored one, whether or not
/// the rod points into the wind (the probe reads Java's private field).
[[nodiscard]] inline std::vector<std::string> describeConditions(const SimulationOptions& options)
{
    // The stored direction only shows when not launching into the wind.
    SimulationOptions notIntoWind(options);
    notIntoWind.setLaunchIntoWind(false);

    const PinkNoiseWindModel&           average = options.getAverageWindModel();
    const MultiLevelPinkNoiseWindModel& multi   = options.getMultiLevelWindModel();
    std::string                         levels;
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : multi.getLevels())
    {
        levels += std::format("({},{},{},{})", javaNumber(level->getAltitude()),
                              javaNumber(level->getSpeed()), javaNumber(level->getDirection()),
                              javaNumber(level->getStandardDeviation()));
    }

    std::vector<std::string> lines;
    lines.push_back(
        std::format("rod={} intoWind={} angle={} dir={}", javaNumber(options.getLaunchRodLength()),
                    options.getLaunchIntoWind(), javaNumber(options.getLaunchRodAngle()),
                    javaNumber(notIntoWind.getLaunchRodDirection())));
    lines.push_back(
        std::format("wind={} avg=({},{},{}) multi={}{}",
                    windModelTypeName(options.getWindModelType()), javaNumber(average.getAverage()),
                    javaNumber(average.getDirection()), javaNumber(average.getStandardDeviation()),
                    Strings::toUpper(toString(multi.getAltitudeReference())), levels));
    lines.push_back(std::format("site={},{},{} geo={}", javaNumber(options.getLaunchAltitude()),
                                javaNumber(options.getLaunchLatitude()),
                                javaNumber(options.getLaunchLongitude()),
                                name(options.getGeodeticComputation())));
    lines.push_back(
        std::format("stepper={} dt={} tmax={} maxAngle={}",
                    simulationStepperMethodName(options.getSimulationStepperMethodChoice()),
                    javaNumber(options.getTimeStep()), javaNumber(options.getMaxSimulationTime()),
                    javaNumber(options.getMaximumStepAngle())));
    lines.push_back(options.isRandomSeedFixed()
                        ? std::format("seedFixed=true seed={}", options.getRandomSeed())
                        : "seedFixed=false");
    lines.push_back(std::format("atmosphere=isa:{} T={} p={} hum={}", options.isIsaAtmosphere(),
                                javaNumber(options.getLaunchTemperature()),
                                javaNumber(options.getLaunchPressure()),
                                javaNumber(options.getLaunchRelativeHumidity())));
    lines.push_back(std::format("gravity={}/{}",
                                gravityModelTypeName(options.getGravityModelType()),
                                javaNumber(options.getConstantGravity())));
    lines.push_back(std::format("thresholds={}/{}/{}/{}",
                                javaNumber(options.getRecoverySpeedWarning()),
                                javaNumber(options.getDrogueLowSpeedWarning()),
                                javaNumber(options.getRecoveryDrogueMainHighSpeedWarning()),
                                javaNumber(options.getRecoveryDrogueMainLowSpeedWarning())));
    lines.push_back(std::format("drag=path:{} table:{} rows:{}",
                                describeLookupPath(options.getDragLookupCsvPath()),
                                describeTable(options.getDragLookupTable(), {"cd"}),
                                describeLookupRows(options.getDragLookupCsvRows())));
    lines.push_back(
        std::format("stability=path:{} table:{} rows:{}",
                    describeLookupPath(options.getStabilityLookupCsvPath()),
                    describeTable(options.getStabilityLookupTable(), {"cn", "cm", "cp"}),
                    describeLookupRows(options.getStabilityLookupCsvRows())));
    return lines;
}

/// The attributes @p attributes as Java prints a TreeMap: "{a=1, b=2}".
[[nodiscard]] inline std::string describeAttributes(const ElementHandler::Attributes& attributes)
{
    std::string text;
    for (const auto& [key, value] : attributes)
    {
        text += std::format("{}{}={}", text.empty() ? "" : ", ", key, value);
    }
    return "{" + text + "}";
}

/// One warning as CondProbe prints it: "W[Other,NORMAL] <text>" (the class is "Other" for a
/// text-only warning).
[[nodiscard]] inline std::string describeWarning(const Warning& warning)
{
    const bool isOther = dynamic_cast<const Warning::Other*>(&warning) != nullptr;
    return std::format("W[{},{}] {}", isOther ? "Other" : "?", exportLabel(warning.priority()),
                       warning.toString());
}

/// What running a <conditions> element through @p handler gave (@p run), as CondProbe prints
/// it: a line end, then one line each, indented by two spaces as in the probe's output (so that
/// an expectation written as a raw string starts on a line of its own):
/// - "W[Other,NORMAL] <text>" for every warning, in order;
/// - "FAILED <code> [<message>]" for a failure. (The probe prints "THROWN <exception>:
///   <message>"; for the two exceptions that fail a load with their message,
///   IllegalArgumentException and NumberFormatException, the expectations of the cases have it
///   as "FAILED INVALID_ARGUMENT [<message>]", which is the failure decision L2 asks for.)
/// - "closed=<element> {<attributes>} [<text, trimmed>]", what the parent's closeElement() was
///   given, or "closed=(not closed)" after a failure; left out when it is "closed=conditions
///   {} []";
/// - "fcid=<id>", left out for the error id;
/// - the lines of describeConditions() that differ from kBaselineOptions.
/// Files are named as neutralPaths() names them, with @p directory as the test's directory.
[[nodiscard]] inline std::string describeOutcome(const HandlerRun&                  run,
                                                 const SimulationConditionsHandler& handler,
                                                 const std::filesystem::path&       directory = {})
{
    std::string text = "\n";
    for (const Warning& warning : run.warnings)
    {
        text += std::format("  {}\n", describeWarning(warning));
    }
    if (!run.result.has_value())
    {
        text += std::format("  FAILED {} [{}]\n", toString(run.result.error().code),
                            run.result.error().message);
    }
    const std::string closed = run.element.empty() ? "(not closed)"
                                                   : std::format("{} {} [{}]", run.element,
                                                                 describeAttributes(run.attributes),
                                                                 Strings::trim(run.content));
    if (closed != "conditions {} []")
    {
        text += std::format("  closed={}\n", closed);
    }
    if (handler.getIdToSet() != FlightConfigurationId::errorId())
    {
        text += std::format("  fcid={}\n", handler.getIdToSet().toFullKey());
    }
    const std::vector<std::string> options = describeConditions(handler.getConditions());
    for (std::size_t i = 0; i < options.size(); i++)
    {
        if (i >= kBaselineOptions.size() || options.at(i) != kBaselineOptions.at(i))
        {
            text += std::format("  {}\n", options.at(i));
        }
    }
    return neutralPaths(std::move(text), directory);
}

/// The CSV files the cases refer to as "{DIR}/tables/...": a drag table, a stability table and
/// a drag table with a NaN. (The Java probe had the same files.)
inline void writeLookupFiles(const TempDir& directory)
{
    EXPECT_TRUE(
        std::filesystem::exists(directory.write("tables/drag.csv", "Mach,Cd\n0,0.3\n1,0.5\n")));
    EXPECT_TRUE(std::filesystem::exists(
        directory.write("tables/stability.csv", "Mach,Cn,Cm,Cp\n0,1,2,3\n2,3,4,5\n")));
    EXPECT_TRUE(
        std::filesystem::exists(directory.write("tables/nan.csv", "Mach,Cd\n0,0.3\n1,NaN\n")));
}

/// A HandlerFixture whose preference store holds what OpenRocket's tests run under
/// (storeJavaTestPreferences()), the store CondProbe ran with.
class ConditionsFixture : public HandlerFixture
{
public:
    ConditionsFixture() { storeJavaTestPreferences(preferences()); }
};

/// Runs @p xml, a <conditions> element, through a new SimulationConditionsHandler under
/// OpenRocket's test preferences and returns describeOutcome(). kDirectoryMark in @p xml
/// stands for a temporary directory that holds the files of writeLookupFiles(), which are only
/// written for an element that names it. The element is read with an empty directory of the
/// test's own as the current directory (kWorkingDirectoryName under the temporary one), so
/// that a file the element names by a relative name is missing whatever lies in the directory
/// the tests are run from.
[[nodiscard]] inline std::string runConditions(std::string_view xml)
{
    ConditionsFixture fixture;
    const TempDir     directory;
    std::string       document(xml);
    if (document.contains(kDirectoryMark))
    {
        writeLookupFiles(directory);
        document = replaceAll(std::move(document), kDirectoryMark, genericUtf8(directory.path()));
    }
    const CurrentDirectoryGuard workingDirectory(directory.resolve(kWorkingDirectoryName));
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run = runHandler(handler, document);
    return describeOutcome(run, handler, directory.path());
}

/// One <conditions> element that CondProbe ran through OpenRocket, with what OpenRocket made
/// of it and, where QtRocket's handlers do something else on purpose, what they make of it.
/// The tables of the handler tests are written by the probe's script (gen_tables.py) from the
/// probe's output; what QtRocket does differently is written by hand from the decision that
/// asks for it.
struct ConditionsCase
{
    /// The probe's name of the case ("cond: ..." for the cases of the scout's EdgeProbe,
    /// "s1: ..." for the ones added with the handlers).
    std::string_view name;
    /// The <conditions> element; kDirectoryMark stands for the directory of the lookup files.
    std::string_view xml;
    /// What OpenRocket makes of it, as describeOutcome() prints an outcome.
    std::string_view java;
    /// What QtRocket makes of it, when that is not what OpenRocket makes of it; else empty.
    std::string_view qtrocket;
    /// Why QtRocket differs; empty when it does not.
    std::string_view why;
};

/// The name of the test of @p caseName: its words run together, each with a capital, and a
/// minus sign that starts a word as "Minus" ("cond: random seeds" is "CondRandomSeeds",
/// "nf: timestep -Infinity" is "NfTimestepMinusInfinity").
[[nodiscard]] inline std::string conditionsTestName(std::string_view caseName)
{
    std::string name;
    bool        startOfWord = true;
    for (const char c : caseName)
    {
        const bool isLower = c >= 'a' && c <= 'z';
        const bool isAlnum = isLower || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (isAlnum)
        {
            name.push_back(startOfWord && isLower ? static_cast<char>(c - 'a' + 'A') : c);
        }
        else if (c == '-' && startOfWord)
        {
            name += "Minus";
        }
        startOfWord = !isAlnum;
    }
    return name;
}

/// conditionsTestName() of a case, for INSTANTIATE_TEST_SUITE_P.
[[nodiscard]] inline std::string conditionsCaseTestName(
    const ::testing::TestParamInfo<ConditionsCase>& info)
{
    return conditionsTestName(info.param.name);
}

/// Expects that running the element of @p conditionsCase gives what the case says: OpenRocket's
/// outcome, or QtRocket's own where the case states one with its reason.
inline void expectConditionsCase(const ConditionsCase& conditionsCase)
{
    const std::string_view expected =
        conditionsCase.qtrocket.empty() ? conditionsCase.java : conditionsCase.qtrocket;
    EXPECT_EQ(runConditions(conditionsCase.xml), std::string(expected)) << conditionsCase.name;
    // A deviation comes with its reason, and only a deviation has one.
    EXPECT_EQ(conditionsCase.qtrocket.empty(), conditionsCase.why.empty()) << conditionsCase.name;
    EXPECT_NE(conditionsCase.qtrocket, conditionsCase.java) << conditionsCase.name;
}

}  // namespace QtRocket::Test
