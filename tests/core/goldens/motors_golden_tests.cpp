// Motor golden tests: every thrust curve OpenRocket loads from the bundled database and from the
// motor test files, what it makes of the malformed and edge-case files and the database in
// tests/data/motors-edge (motors or error messages), and its RockSimMotorWriter output for every
// bundled motor, compared with tests/data/goldens/motors.json, which
// tools/openrocket-goldens/motors/dump-motors.sh writes from OpenRocket's own code.
//
// Digests, strings and point counts must match exactly. The derived values come from +, -, *, /
// only (no transcendental function), so they match OpenRocket to the last bit on every platform;
// they are compared with a relative tolerance of 1e-9 all the same, the golden files' policy.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/file/motor/RockSimMotorWriter.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Md5.h"
#include "TestPaths.h"

namespace
{

using nlohmann::json;
using QtRocket::GeneralMotorLoader;
using QtRocket::SqliteMotorDatabaseReader;
using QtRocket::ThrustCurveMotor;

/// The relative tolerance of derived values.
constexpr double kRelativeTolerance = 1e-9;

/// Curves reported in detail before a test stops listing mismatches.
constexpr int kMaxReported = 20;

[[nodiscard]] const json& golden()
{
    static const json kGolden = [] {
        const QtRocket::Result<std::string> text =
            QtRocket::readTextFile(QtRocket::Test::testDataDir() / "goldens" / "motors.json");
        if (!text)
        {
            return json();
        }
        return json::parse(*text);
    }();
    return kGolden;
}

/// A JSON number, or one of Java's non-finite values written as a string.
[[nodiscard]] double number(const json& value)
{
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        if (text == "Infinity")
        {
            return std::numeric_limits<double>::infinity();
        }
        if (text == "-Infinity")
        {
            return -std::numeric_limits<double>::infinity();
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value.get<double>();
}

[[nodiscard]] bool close(double expected, double actual)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (std::isinf(expected) || std::isinf(actual))
    {
        return expected == actual;
    }
    return std::abs(actual - expected) <= kRelativeTolerance * std::abs(expected);
}

/// The mismatches between @p motor and the golden @p expected, one per line; empty when it matches.
[[nodiscard]] std::string compare(const json& expected, const ThrustCurveMotor& motor)
{
    std::string mismatches;
    const auto  text = [&](const char* field, const std::string& actual) {
        if (expected.at(field).get<std::string>() != actual)
        {
            mismatches += std::format("  {}: expected \"{}\", got \"{}\"\n", field,
                                      expected.at(field).get<std::string>(), actual);
        }
    };
    const auto value = [&](const char* field, double actual) {
        if (!close(number(expected.at(field)), actual))
        {
            mismatches += std::format("  {}: expected {}, got {}\n", field,
                                      number(expected.at(field)), actual);
        }
    };

    text("digest", motor.getDigest());
    text("manufacturer", motor.getManufacturer().getDisplayName());
    text("manufacturerSimpleName", motor.getManufacturer().getSimpleName());
    text("code", motor.getCode());
    text("designation", motor.getDesignation());
    text("commonName", motor.getCommonName());
    text("description", motor.getDescription());
    text("type", std::string(QtRocket::enumName(motor.getMotorType())));
    text("caseInfo", motor.getCaseInfo());
    text("propellantInfo", motor.getPropellantInfo());
    text("tcMotorId", motor.getTcMotorId());
    text("infoUrl", motor.getInfoUrl());
    text("updatedOn", motor.getUpdatedOn());
    text("dataSource", motor.getDataSource());

    const std::optional<int> dataFiles =
        expected.at("dataFiles").is_null()
            ? std::nullopt
            : std::optional<int>(expected.at("dataFiles").get<int>());
    if (dataFiles != motor.getDataFiles())
    {
        mismatches += "  dataFiles differ\n";
    }
    if (expected.at("sparky").get<bool>() != motor.isSparky())
    {
        mismatches += "  sparky differs\n";
    }
    if (expected.at("available").get<bool>() != motor.isAvailable())
    {
        mismatches += "  available differs\n";
    }
    if (expected.at("points").get<std::size_t>() != motor.getTimePoints().size())
    {
        mismatches +=
            std::format("  points: expected {}, got {}\n", expected.at("points").get<std::size_t>(),
                        motor.getTimePoints().size());
    }

    const json&                delays = expected.at("delays");
    const std::vector<double>& actual = motor.getStandardDelays();
    if (delays.size() != actual.size() ||
        !std::ranges::equal(delays, actual,
                            [](const json& e, double a) { return close(number(e), a); }))
    {
        mismatches +=
            std::format("  delays: expected {}, got {} values\n", delays.dump(), actual.size());
    }

    value("diameter", motor.getDiameter());
    value("length", motor.getLength());
    value("initialMass", motor.getInitialMass());
    value("totalImpulse", motor.getTotalImpulseEstimate());
    value("averageThrust", motor.getAverageThrustEstimate());
    value("maxThrust", motor.getMaxThrustEstimate());
    value("burnTimeEstimate", motor.getBurnTimeEstimate());
    value("burnTime", motor.getBurnTime());
    value("launchMass", motor.getLaunchMass());
    value("burnoutMass", motor.getBurnoutMass());
    value("launchCG", motor.getLaunchCGx());
    value("burnoutCG", motor.getBurnoutCGx());
    return mismatches;
}

/// A JSON string, or "" for null (a Java null string).
[[nodiscard]] std::string textOrEmpty(const json& value)
{
    return value.is_null() ? std::string() : value.get<std::string>();
}

[[nodiscard]] std::string_view reasonName(SqliteMotorDatabaseReader::SkippedCurve::Reason reason)
{
    using Reason = SqliteMotorDatabaseReader::SkippedCurve::Reason;
    switch (reason)
    {
        case Reason::NO_CURVES:
            return "NO_CURVES";
        case Reason::NO_DATA:
            return "NO_DATA";
        case Reason::INVALID_DATA:
            return "INVALID_DATA";
        case Reason::INVALID_MOTOR:
            return "INVALID_MOTOR";
    }
    return "?";
}

/// A skipped curve as one comparable line.
[[nodiscard]] std::string describe(std::string_view reason, std::optional<int> curveId,
                                   std::string_view designation, std::string_view message)
{
    return std::format("{} curve {} ({}): {}", reason,
                       curveId.has_value() ? std::to_string(*curveId) : std::string("-"),
                       designation, message);
}

/// The mismatches between what GeneralMotorLoader makes of the golden file @p entry, in the test
/// data directory @p directory, and what OpenRocket made of it; empty when they agree. A failure
/// must carry OpenRocket's message (the golden has "<exception class>: <message>").
[[nodiscard]] std::string compareFile(const json& entry, const GeneralMotorLoader& loader,
                                      std::string_view directory)
{
    const std::string                              name = entry.at("file").get<std::string>();
    const QtRocket::Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / directory / name);
    if (!bytes)
    {
        return bytes.error().toString();
    }
    const QtRocket::Result<std::vector<ThrustCurveMotor::Builder>> loaded =
        loader.load(*bytes, name);
    if (!entry.at("error").is_null())
    {
        const std::string error   = entry.at("error").get<std::string>();
        const std::size_t colon   = error.find(": ");
        const std::string message = colon == std::string::npos ? error : error.substr(colon + 2);
        if (loaded)
        {
            return "OpenRocket failed: " + error;
        }
        return loaded.error().message == message
                   ? ""
                   : std::format(R"(failed with "{}", OpenRocket with "{}")",
                                 loaded.error().message, message);
    }
    if (!loaded)
    {
        return loaded.error().toString();
    }
    const json& curves = entry.at("curves");
    if (loaded->size() != curves.size())
    {
        return std::format("{} curves, OpenRocket {}", loaded->size(), curves.size());
    }
    std::string mismatches;
    for (std::size_t i = 0; i < curves.size(); i++)
    {
        const QtRocket::Result<ThrustCurveMotor> motor = (*loaded)[i].build();
        if (curves[i].contains("buildError"))
        {
            const std::string expected = curves[i].at("buildError").get<std::string>();
            if (motor || motor.error().message != expected)
            {
                mismatches += std::format("curve {}: expected the build error {}\n", i, expected);
            }
            continue;
        }
        mismatches += motor ? compare(curves[i], *motor)
                            : std::format("curve {}: {}\n", i, motor.error().message);
    }
    return mismatches;
}

TEST(MotorsGolden, GoldenFileIsPresent)
{
    ASSERT_TRUE(golden().is_object()) << "tests/data/goldens/motors.json is missing or empty";
    EXPECT_EQ(golden().at("database").at("curves").size(), 1591U);
}

TEST(MotorsGolden, DatabaseCurvesMatchOpenRocket)
{
    ASSERT_TRUE(golden().is_object());
    const QtRocket::Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(QtRocket::Test::dataDir() / "motors" /
                                                "initial_motors.db");
    ASSERT_TRUE(contents) << contents.error().toString();

    const json& curves = golden().at("database").at("curves");
    ASSERT_EQ(contents->motors.size(), curves.size());
    int reported = 0;
    for (std::size_t i = 0; i < curves.size() && reported < kMaxReported; i++)
    {
        const std::string mismatches = compare(curves[i], *contents->motors[i]);
        if (!mismatches.empty())
        {
            ADD_FAILURE() << std::format("database curve {} ({} {}):\n{}", i,
                                         curves[i].at("manufacturer").get<std::string>(),
                                         curves[i].at("code").get<std::string>(), mismatches);
            reported++;
        }
    }
}

TEST(MotorsGolden, DatabaseSkipsTheCurvesOpenRocketSkips)
{
    ASSERT_TRUE(golden().is_object());
    const QtRocket::Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(QtRocket::Test::dataDir() / "motors" /
                                                "initial_motors.db");
    ASSERT_TRUE(contents) << contents.error().toString();

    std::vector<std::string> expected;
    for (const json& entry : golden().at("database").at("skipped"))
    {
        const std::optional<int> curveId = entry.at("curveId").is_null()
                                               ? std::nullopt
                                               : std::optional<int>(entry.at("curveId").get<int>());
        expected.push_back(describe(entry.at("reason").get<std::string>(), curveId,
                                    textOrEmpty(entry.at("designation")),
                                    textOrEmpty(entry.at("message"))));
    }
    std::vector<std::string> actual;
    for (const SqliteMotorDatabaseReader::SkippedCurve& skipped : contents->skipped)
    {
        actual.push_back(describe(reasonName(skipped.reason), skipped.curveId, skipped.designation,
                                  skipped.message));
    }
    EXPECT_EQ(actual, expected);
}

TEST(MotorsGolden, MotorFilesMatchOpenRocket)
{
    ASSERT_TRUE(golden().is_object());
    const GeneralMotorLoader loader;
    for (const json& entry : golden().at("files"))
    {
        EXPECT_EQ(compareFile(entry, loader, "motors"), "") << entry.at("file").get<std::string>();
    }
}

TEST(MotorsGolden, EdgeCaseFilesMatchOpenRocket)
{
    ASSERT_TRUE(golden().is_object());
    const GeneralMotorLoader loader;
    ASSERT_FALSE(golden().at("edgeFiles").empty());
    for (const json& entry : golden().at("edgeFiles"))
    {
        EXPECT_EQ(compareFile(entry, loader, "motors-edge"), "")
            << entry.at("file").get<std::string>();
    }
}

/// The names of the files in the test data directory @p directory, sorted; the databases (*.db)
/// or the others.
[[nodiscard]] std::vector<std::string> filesIn(std::string_view directory, bool databases)
{
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& file :
         std::filesystem::directory_iterator(QtRocket::Test::testDataDir() / directory))
    {
        if (file.is_regular_file() && (file.path().extension() == ".db") == databases)
        {
            const std::u8string name = file.path().filename().u8string();
            names.emplace_back(name.begin(), name.end());
        }
    }
    std::ranges::sort(names);
    return names;
}

[[nodiscard]] std::vector<std::string> goldenFiles(const char* key)
{
    std::vector<std::string> names;
    for (const json& entry : golden().at(key))
    {
        names.push_back(entry.at("file").get<std::string>());
    }
    std::ranges::sort(names);
    return names;
}

TEST(MotorsGolden, CoversEveryMotorTestFile)
{
    ASSERT_TRUE(golden().is_object());
    // A new motor test file needs tools/openrocket-goldens/motors/dump-motors.sh to be run again.
    EXPECT_EQ(filesIn("motors", false), goldenFiles("files"));
    EXPECT_EQ(filesIn("motors-edge", false), goldenFiles("edgeFiles"));
    EXPECT_EQ(filesIn("motors-edge", true),
              std::vector<std::string>{golden().at("edgeDatabase").at("file").get<std::string>()});
}

/// The edge-case database as the reader reads it; fails the test when it does not read.
[[nodiscard]] SqliteMotorDatabaseReader::Contents readEdgeDatabase()
{
    const QtRocket::Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(
            QtRocket::Test::testDataDir() / "motors-edge" /
            golden().at("edgeDatabase").at("file").get<std::string>());
    EXPECT_TRUE(contents) << contents.error().toString();
    return contents ? *contents : SqliteMotorDatabaseReader::Contents{};
}

TEST(MotorsGolden, EdgeCaseDatabaseCurvesMatchOpenRocket)
{
    ASSERT_TRUE(golden().is_object());
    const SqliteMotorDatabaseReader::Contents contents = readEdgeDatabase();
    const json&                               curves   = golden().at("edgeDatabase").at("curves");
    ASSERT_EQ(contents.motors.size(), curves.size());
    for (std::size_t i = 0; i < curves.size(); i++)
    {
        EXPECT_EQ(compare(curves[i], *contents.motors[i]), "") << "edge database curve " << i;
    }
}

TEST(MotorsGolden, EdgeCaseDatabaseSkipsTheCurvesOpenRocketSkips)
{
    ASSERT_TRUE(golden().is_object());
    const SqliteMotorDatabaseReader::Contents contents = readEdgeDatabase();
    std::vector<std::string>                  expected;
    for (const json& entry : golden().at("edgeDatabase").at("skipped"))
    {
        const std::optional<int> curveId = entry.at("curveId").is_null()
                                               ? std::nullopt
                                               : std::optional<int>(entry.at("curveId").get<int>());
        expected.push_back(describe(entry.at("reason").get<std::string>(), curveId,
                                    textOrEmpty(entry.at("designation")),
                                    textOrEmpty(entry.at("message"))));
    }
    std::vector<std::string> actual;
    actual.reserve(contents.skipped.size());
    for (const SqliteMotorDatabaseReader::SkippedCurve& skipped : contents.skipped)
    {
        actual.push_back(describe(reasonName(skipped.reason), skipped.curveId, skipped.designation,
                                  skipped.message));
    }
    EXPECT_EQ(actual, expected);
}

TEST(MotorsGolden, WriterOutputMatchesOpenRocket)
{
    ASSERT_TRUE(golden().is_object());
    const QtRocket::Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(QtRocket::Test::dataDir() / "motors" /
                                                "initial_motors.db");
    ASSERT_TRUE(contents) << contents.error().toString();

    const json& digests = golden().at("writer");
    ASSERT_EQ(contents->motors.size(), digests.size());
    int reported = 0;
    for (std::size_t i = 0; i < digests.size() && reported < kMaxReported; i++)
    {
        const std::string document = QtRocket::RockSimMotorWriter::write(*contents->motors[i]);
        const std::string actual =
            QtRocket::toHex(QtRocket::md5(std::as_bytes(std::span(document))));
        if (actual != digests[i].get<std::string>())
        {
            ADD_FAILURE() << std::format("writer output for database curve {} ({}) differs", i,
                                         contents->motors[i]->getCode());
            reported++;
        }
    }
}

}  // namespace
