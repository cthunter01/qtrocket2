#include "QtRocket/motor/SqliteMotorDatabaseReader.h"

#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::ErrorCode;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorDigest;
using QtRocket::Result;
using QtRocket::SqliteMotorDatabaseReader;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::ThrustCurveMotorSetDatabase;
using MotorPtr     = std::shared_ptr<const ThrustCurveMotor>;
using SkippedCurve = SqliteMotorDatabaseReader::SkippedCurve;
using Reason       = SkippedCurve::Reason;

constexpr double kInf = std::numeric_limits<double>::infinity();

[[nodiscard]] std::filesystem::path bundledDatabase()
{
    return QtRocket::Test::dataDir() / "motors" / "initial_motors.db";
}

[[nodiscard]] std::string utf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

/// A directory in the temporary directory, removed with its contents when the test ends
/// (JUnit's @TempDir).
class TempDir
{
public:
    TempDir()
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_path = std::filesystem::temp_directory_path() /
                 std::format("qtrocket_{}_{}_{}", info->test_suite_name(), info->name(),
                             std::random_device{}());
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] std::filesystem::path resolve(std::string_view name) const
    {
        return m_path / name;
    }

private:
    std::filesystem::path m_path;
};

/// Runs @p sql on the database @p file (created when missing), as the JUnit tests do through
/// JDBC.
void exec(const std::filesystem::path& file, const std::string& sql)
{
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open_v2(utf8(file).c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                              nullptr),
              SQLITE_OK);
    const int status = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    EXPECT_EQ(status, SQLITE_OK) << sqlite3_errmsg(db) << "\n" << sql;
    sqlite3_close(db);
}

/// The first column of the first row of @p sql on @p file, as text; nullopt for NULL or no row.
[[nodiscard]] std::optional<std::string> query(const std::filesystem::path& file,
                                               const std::string&           sql)
{
    sqlite3* db = nullptr;
    EXPECT_EQ(sqlite3_open_v2(utf8(file).c_str(), &db, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr), SQLITE_OK)
        << sqlite3_errmsg(db);
    std::optional<std::string> value;
    if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_type(statement, 0) != SQLITE_NULL)
    {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        // The C boundary: SQLite hands out its UTF-8 text as unsigned char.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        value = std::string(reinterpret_cast<const char*>(text));
    }
    sqlite3_finalize(statement);
    sqlite3_close(db);
    return value;
}

/// The schema of the bundled database (version 2), without the indices.
constexpr std::string_view kSchemaV2 =
    "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
    "CREATE TABLE manufacturers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, "
    "abbrev TEXT);"
    "CREATE TABLE motors (id INTEGER PRIMARY KEY AUTOINCREMENT, manufacturer_id INTEGER NOT NULL, "
    "tc_motor_id TEXT, designation TEXT NOT NULL, common_name TEXT, impulse_class TEXT, "
    "diameter REAL, length REAL, total_impulse REAL, avg_thrust REAL, max_thrust REAL, "
    "burn_time REAL, propellant_weight REAL, total_weight REAL, type TEXT, delays TEXT, "
    "case_info TEXT, prop_info TEXT, sparky INTEGER, info_url TEXT, data_files INTEGER, "
    "updated_on TEXT, FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id));"
    "CREATE TABLE thrust_curves (id INTEGER PRIMARY KEY AUTOINCREMENT, motor_id INTEGER NOT NULL, "
    "tc_simfile_id TEXT, source TEXT, format TEXT, license TEXT, info_url TEXT, data_url TEXT, "
    "total_impulse REAL, avg_thrust REAL, max_thrust REAL, burn_time REAL, "
    "FOREIGN KEY (motor_id) REFERENCES motors(id) ON DELETE CASCADE);"
    "CREATE TABLE thrust_data (id INTEGER PRIMARY KEY AUTOINCREMENT, curve_id INTEGER NOT NULL, "
    "time_seconds REAL NOT NULL, force_newtons REAL NOT NULL, "
    "FOREIGN KEY (curve_id) REFERENCES thrust_curves(id) ON DELETE CASCADE);"
    "INSERT INTO meta (key, value) VALUES ('schema_version', '2');"
    "INSERT INTO manufacturers (id, name, abbrev) VALUES (1, 'Estes Industries', 'Estes');";

/// A version-2 database at @p file with one motor per row of @p motorRows (the values of
/// id, manufacturer_id, designation, common_name, diameter, length, propellant_weight,
/// total_weight, type, delays) and the curves and points of @p data.
void createDatabase(const std::filesystem::path& file, const std::string& motorRows,
                    const std::string& data = "")
{
    exec(file, std::string(kSchemaV2) +
                   "INSERT INTO motors (id, manufacturer_id, designation, common_name, diameter, "
                   "length, propellant_weight, total_weight, type, delays) VALUES " +
                   motorRows + ";" + data);
}

[[nodiscard]] SqliteMotorDatabaseReader::Contents read(const std::filesystem::path& file)
{
    Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(file);
    EXPECT_TRUE(contents) << contents.error().toString();
    return contents.value_or(SqliteMotorDatabaseReader::Contents{});
}

[[nodiscard]] std::string readError(const std::filesystem::path& file)
{
    const Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(file);
    EXPECT_FALSE(contents);
    if (contents)
    {
        return {};
    }
    EXPECT_EQ(contents.error().code, ErrorCode::DATABASE);
    return contents.error().message;
}

[[nodiscard]] MotorPtr build(ThrustCurveMotor::Builder& builder)
{
    return std::make_shared<const ThrustCurveMotor>(builder.build().value());
}

[[nodiscard]] const ThrustCurveMotor& findByDesignation(const std::vector<MotorPtr>& motors,
                                                        std::string_view             designation)
{
    const auto found = std::ranges::find_if(motors, [designation](const MotorPtr& motor) {
        return motor->getDesignation() == designation;
    });
    EXPECT_NE(found, motors.end()) << "Motor not found: " << designation;
    return found == motors.end() ? *motors.front() : **found;
}

/// The points of @p coordinates that differ from (xValues[i], 0, 0, w = masses[i]) by more than
/// 1e-9 in any component, or "" when none does.
[[nodiscard]] std::string coordinateMismatches(const std::vector<Coordinate>& coordinates,
                                               const std::vector<double>&     xValues,
                                               const std::vector<double>&     masses)
{
    if (xValues.size() != coordinates.size() || masses.size() != coordinates.size())
    {
        return std::format("{} points", coordinates.size());
    }
    // Within 1e-9, NaN never.
    const auto  close = [](double a, double b) { return std::abs(a - b) <= 1.0e-9; };
    std::string text;
    for (std::size_t i = 0; i < coordinates.size(); i++)
    {
        const Coordinate& c = coordinates[i];
        if (!close(xValues[i], c.x) || !close(0.0, c.y) || !close(0.0, c.z) ||
            !close(masses[i], c.weight))
        {
            text += std::format("{}: ({}, {}, {}, w={}); ", i, c.x, c.y, c.z, c.weight);
        }
    }
    return text;
}

/// assertCoordinatesMatch() of ThrustCurveMotorSQLiteDatabaseTest.
void assertCoordinatesMatch(const std::vector<Coordinate>& coordinates,
                            const std::vector<double>& xValues, const std::vector<double>& masses)
{
    EXPECT_EQ(coordinateMismatches(coordinates, xValues, masses), "");
}

/// A motor as ThrustCurveMotorSQLiteDatabaseTest builds them.
[[nodiscard]] MotorPtr testMotor(std::string_view manufacturer, std::string_view designation,
                                 std::vector<double> delays, std::vector<double> thrust,
                                 double initialMass)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setCode(std::string(designation))
        .setCommonName(std::string(designation))
        .setDesignation(std::string(designation))
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays(std::move(delays))
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0.0, 0.5, 1.0})
        .setThrustPoints(std::move(thrust))
        .setCGPoints({Coordinate(0.035, 0, 0, initialMass),
                      Coordinate(0.035, 0, 0, initialMass - 0.005),
                      Coordinate(0.035, 0, 0, initialMass - 0.01)})
        .setInitialMass(initialMass);
    return build(builder);
}

// ---------------------------------------------------------------- InitialMotorsDatabaseFormatTest

TEST(InitialMotorsDatabaseFormat, InitialMotorsDbIsReadable)
{
    const Result<void> valid = SqliteMotorDatabaseReader::validateDatabase(bundledDatabase());
    ASSERT_TRUE(valid) << valid.error().toString();
    const SqliteMotorDatabaseReader::Contents contents = read(bundledDatabase());
    EXPECT_FALSE(contents.motors.empty()) << "initial_motors.db should contain motors";
    EXPECT_EQ(contents.motors.size(), 1591U);
    EXPECT_TRUE(contents.skipped.empty());
}

// ---------------------------------------------------------------- BundledMotorDatabaseTest

/// The bundled database in a ThrustCurveMotorSetDatabase (loadBundledDatabase()).
class BundledMotorDatabase : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        s_database = std::make_unique<ThrustCurveMotorSetDatabase>();
        for (const MotorPtr& motor : read(bundledDatabase()).motors)
        {
            s_database->addMotor(motor);
        }
    }
    static void TearDownTestSuite() { s_database.reset(); }

    /// The motor sets of @p manufacturer with @p designation.
    [[nodiscard]] static std::vector<const ThrustCurveMotorSet*> sets(std::string_view designation,
                                                                      std::string_view manufacturer)
    {
        std::vector<const ThrustCurveMotorSet*> found;
        for (const ThrustCurveMotorSet& set : s_database->getMotorSets())
        {
            if (set.getDesignation() == designation && set.getManufacturer()->matches(manufacturer))
            {
                found.push_back(&set);
            }
        }
        return found;
    }

    static std::unique_ptr<ThrustCurveMotorSetDatabase> s_database;
};

std::unique_ptr<ThrustCurveMotorSetDatabase> BundledMotorDatabase::s_database;

// Estes B6 must appear as exactly one motor set with designation "B6" and common name "B6", which
// guards against the B6 / B6-0 duplicate that arose because some data sources stored the motor
// with the delay embedded in the designation ("B6-0").
TEST_F(BundledMotorDatabase, EstesB6HasExactlyOneMotorSet)
{
    const std::vector<const ThrustCurveMotorSet*> estesB6Sets = sets("B6", "Estes");
    ASSERT_EQ(estesB6Sets.size(), 1U)
        << "Expected exactly one Estes B6 motor set, found: " << estesB6Sets.size();
    EXPECT_EQ(estesB6Sets.front()->getCommonName(), "B6");
}

// Quest B6W must have designation "B6W" and common name "B6".
TEST_F(BundledMotorDatabase, QuestB6WCommonName)
{
    const std::vector<const ThrustCurveMotorSet*> questB6WSets = sets("B6W", "Quest");
    ASSERT_FALSE(questB6WSets.empty()) << "No Quest B6W motor set found in the bundled database";
    EXPECT_EQ(questB6WSets.front()->getCommonName(), "B6");
}

// Estes C6 must have exactly two thrust curves and the delays 0, 3, 5, 7 and plugged.
TEST_F(BundledMotorDatabase, EstesC6HasTwoCurvesAndCorrectDelays)
{
    const std::vector<const ThrustCurveMotorSet*> c6Sets = sets("C6", "Estes");
    ASSERT_EQ(c6Sets.size(), 1U) << "Expected exactly one Estes C6 motor set";
    const ThrustCurveMotorSet& c6Set = *c6Sets.front();

    EXPECT_EQ(c6Set.getMotors().size(), 2U)
        << "Expected 2 thrust curves for Estes C6, found: " << c6Set.getMotors().size();

    // The set's delays are sorted, so containing each of 0, 3, 5, 7 and plugged is including them.
    const std::vector<double> required{0.0, 3.0, 5.0, 7.0, kInf};
    EXPECT_TRUE(std::ranges::includes(c6Set.getDelays(), required))
        << "Missing delay 0, 3, 5, 7 or PLUGGED in Estes C6";
}

TEST_F(BundledMotorDatabase, GroupsTheCurvesAsOpenRocket)
{
    // OpenRocket 5f164fd0e makes 1458 sets holding 1588 of the 1591 curves (three share a digest).
    EXPECT_EQ(s_database->getMotorSets().size(), 1458U);
    std::size_t motors = 0;
    for (const ThrustCurveMotorSet& set : s_database->getMotorSets())
    {
        motors += set.getMotorCount();
    }
    EXPECT_EQ(motors, 1588U);
}

// ----------------------------------------------------------------
// ThrustCurveMotorSQLiteDatabaseTest

TEST(SqliteMotorDatabaseReader, RoundTripPersistsMotors)
{
    const TempDir tempDir;

    ThrustCurveMotor::Builder builderA;
    builderA.setManufacturer(Manufacturer::getManufacturer("TestCo"))
        .setCode("T100")
        .setCommonName("T100")
        .setDesignation("T100")
        .setDescription("Test motor A")
        .setDataSource("manual")
        .setUpdatedOn("2025-01-01")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({3, 5})
        .setDiameter(0.03)
        .setLength(0.12)
        .setTimePoints({0.0, 0.5, 1.0})
        .setThrustPoints({0.0, 10.0, 0.0})
        .setCGPoints({Coordinate(0.05, 0, 0, 0.10), Coordinate(0.05, 0, 0, 0.09),
                      Coordinate(0.05, 0, 0, 0.08)})
        .setCaseInfo("Case A")
        .setPropellantInfo("Prop A")
        .setInitialMass(0.10);
    const MotorPtr motorA = build(builderA);

    ThrustCurveMotor::Builder builderB;
    builderB.setManufacturer(Manufacturer::getManufacturer("AnotherCo"))
        .setCode("R200")
        .setCommonName("R200")
        .setDesignation("R200")
        .setDescription("Test motor B")
        .setDataSource("thrustcurve.org")
        .setMotorType(Motor::Type::RELOAD)
        .setStandardDelays({})
        .setDiameter(0.05)
        .setLength(0.20)
        .setTimePoints({0.0, 0.3, 0.9})
        .setThrustPoints({0.0, 25.0, 0.0})
        .setCGPoints({Coordinate(0.07, 0, 0, 0.20), Coordinate(0.07, 0, 0, 0.18),
                      Coordinate(0.07, 0, 0, 0.16)})
        .setCaseInfo("Case B")
        .setPropellantInfo("Prop B")
        .setInitialMass(0.20);
    const MotorPtr motorB = build(builderB);

    const std::filesystem::path dbFile = tempDir.resolve("motors.db");
    const std::vector<MotorPtr> motors{motorA, motorB};
    const Result<void>          written = SqliteMotorDatabaseReader::writeDatabase(dbFile, motors);
    ASSERT_TRUE(written) << written.error().toString();

    const SqliteMotorDatabaseReader::Contents loaded = read(dbFile);
    ASSERT_EQ(loaded.motors.size(), 2U);

    const ThrustCurveMotor& loadedA = findByDesignation(loaded.motors, "T100");
    EXPECT_EQ(loadedA.getManufacturer().getDisplayName(), "TestCo");
    EXPECT_EQ(loadedA.getCode(), "T100");
    EXPECT_EQ(loadedA.getCommonName(), "T100");
    EXPECT_EQ(loadedA.getDesignation(), "T100");
    EXPECT_EQ(loadedA.getDescription(), "Test motor A");
    EXPECT_EQ(loadedA.getDataSource(), "manual");
    EXPECT_EQ(loadedA.getUpdatedOn(), "2025-01-01");
    EXPECT_EQ(loadedA.getMotorType(), Motor::Type::SINGLE);
    EXPECT_NEAR(loadedA.getDiameter(), 0.03, 1.0e-9);
    EXPECT_NEAR(loadedA.getLength(), 0.12, 1.0e-9);
    EXPECT_EQ(loadedA.getCaseInfo(), "Case A");
    EXPECT_EQ(loadedA.getPropellantInfo(), "Prop A");
    EXPECT_NEAR(loadedA.getInitialMass(), 0.10, 1.0e-9);
    EXPECT_EQ(MotorDigest::digestMotor(loadedA), loadedA.getDigest());
    EXPECT_TRUE(loadedA.isAvailable());
    EXPECT_EQ(loadedA.getStandardDelays(), (std::vector<double>{3, 5}));
    EXPECT_EQ(loadedA.getTimePoints(), (std::vector<double>{0.0, 0.5, 1.0}));
    EXPECT_EQ(loadedA.getThrustPoints(), (std::vector<double>{0.0, 10.0, 0.0}));
    // CG is reconstructed by the database loader (center of motor).
    assertCoordinatesMatch(loadedA.getCGPoints(), {0.06, 0.06, 0.06}, {0.10, 0.09, 0.08});

    const ThrustCurveMotor& loadedB = findByDesignation(loaded.motors, "R200");
    EXPECT_EQ(loadedB.getManufacturer().getDisplayName(), "AnotherCo");
    EXPECT_EQ(loadedB.getMotorType(), Motor::Type::RELOAD);
    EXPECT_TRUE(loadedB.isAvailable());
    EXPECT_EQ(loadedB.getDataSource(), "thrustcurve.org");
    EXPECT_EQ(MotorDigest::digestMotor(loadedB), loadedB.getDigest());
    EXPECT_TRUE(loadedB.getStandardDelays().empty());
    EXPECT_EQ(loadedB.getTimePoints(), (std::vector<double>{0.0, 0.3, 0.9}));
    EXPECT_EQ(loadedB.getThrustPoints(), (std::vector<double>{0.0, 25.0, 0.0}));
    assertCoordinatesMatch(loadedB.getCGPoints(), {0.10, 0.10, 0.10},
                           {0.20, 0.18666666666666668, 0.16});
}

TEST(SqliteMotorDatabaseReader, ValidateDatabaseMissingColumns)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("invalid.db");
    exec(dbFile,
         "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
         "INSERT INTO meta (key, value) VALUES ('schema_version', '3');"
         "CREATE TABLE motors (id INTEGER PRIMARY KEY);"
         "CREATE TABLE manufacturers (id INTEGER PRIMARY KEY);"
         "CREATE TABLE thrust_curves (id INTEGER PRIMARY KEY);"
         "CREATE TABLE thrust_data (id INTEGER PRIMARY KEY);");

    const Result<void> result = SqliteMotorDatabaseReader::validateDatabase(dbFile);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().message.contains("missing required columns"));
    EXPECT_EQ(result.error().message,
              "SQLite motor database table 'manufacturers' missing required columns: "
              "[name, abbrev]");
}

/// The tables of ThrustCurveMotorSQLiteDatabaseTest's schema tests, for @p schemaVersion, with
/// data_files as TEXT and nothing NOT NULL.
[[nodiscard]] std::string looseSchema(int schemaVersion)
{
    return std::format(
        "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
        "INSERT INTO meta (key, value) VALUES ('schema_version', '{}');"
        "CREATE TABLE manufacturers (id INTEGER PRIMARY KEY, name TEXT, abbrev TEXT);"
        "CREATE TABLE motors (id INTEGER PRIMARY KEY, manufacturer_id INTEGER, tc_motor_id TEXT, "
        "designation TEXT, common_name TEXT, impulse_class TEXT, diameter REAL, length REAL, "
        "total_impulse REAL, avg_thrust REAL, max_thrust REAL, burn_time REAL, "
        "propellant_weight REAL, total_weight REAL, type TEXT, delays TEXT, case_info TEXT, "
        "prop_info TEXT, sparky INTEGER, info_url TEXT, data_files TEXT, updated_on TEXT{});"
        "CREATE TABLE thrust_curves (id INTEGER PRIMARY KEY, motor_id INTEGER, tc_simfile_id TEXT, "
        "source TEXT, format TEXT, license TEXT, info_url TEXT, data_url TEXT, "
        "total_impulse REAL, avg_thrust REAL, max_thrust REAL, burn_time REAL);"
        "CREATE TABLE thrust_data (id INTEGER PRIMARY KEY, curve_id INTEGER, time_seconds REAL, "
        "force_newtons REAL);",
        schemaVersion, schemaVersion >= 3 ? ", description TEXT, source TEXT" : "");
}

TEST(SqliteMotorDatabaseReader, ValidateDatabaseSchemaV2AllColumns)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("v2.db");
    exec(dbFile, looseSchema(2));
    const Result<void> result = SqliteMotorDatabaseReader::validateDatabase(dbFile);
    EXPECT_TRUE(result) << result.error().toString();
}

TEST(SqliteMotorDatabaseReader, ValidateDatabaseSchemaV3AllColumns)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("v3.db");
    exec(dbFile, looseSchema(3));
    const Result<void> result = SqliteMotorDatabaseReader::validateDatabase(dbFile);
    EXPECT_TRUE(result) << result.error().toString();
}

// A motor stored with designation "B6-0" (delay in designation, as some API sources provide) is
// read back with designation "B6" so it groups with other B6 motors; the code keeps the original.
TEST(SqliteMotorDatabaseReader, DesignationDelayStrippedOnRead)
{
    const TempDir             tempDir;
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setCode("B6-0")
        .setCommonName("B6")
        .setDesignation("B6-0")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({})
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0.0, 0.5, 1.0})
        .setThrustPoints({0.0, 6.0, 0.0})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.022), Coordinate(0.035, 0, 0, 0.020),
                      Coordinate(0.035, 0, 0, 0.018)})
        .setInitialMass(0.022);
    const std::vector<MotorPtr> motors{build(builder)};

    const std::filesystem::path dbFile = tempDir.resolve("b6-0.db");
    ASSERT_TRUE(SqliteMotorDatabaseReader::writeDatabase(dbFile, motors));

    const SqliteMotorDatabaseReader::Contents loaded = read(dbFile);
    ASSERT_EQ(loaded.motors.size(), 1U);
    const ThrustCurveMotor& m = *loaded.motors.front();
    EXPECT_EQ(m.getDesignation(), "B6") << "Delay suffix should be stripped from designation";
    EXPECT_EQ(m.getCode(), "B6-0") << "Original designation should be preserved in code";
    EXPECT_EQ(m.getCommonName(), "B6");
}

TEST(SqliteMotorDatabaseReader, RoundTripPluggedDelay)
{
    const TempDir               tempDir;
    const std::vector<MotorPtr> motors{
        testMotor("TestCo", "B6", {0, 3, 5, Motor::kPluggedDelay}, {0.0, 10.0, 0.0}, 0.05)};
    const std::filesystem::path dbFile = tempDir.resolve("plugged.db");
    ASSERT_TRUE(SqliteMotorDatabaseReader::writeDatabase(dbFile, motors));

    const SqliteMotorDatabaseReader::Contents loaded = read(dbFile);
    ASSERT_EQ(loaded.motors.size(), 1U);
    const std::vector<double>& delays = loaded.motors.front()->getStandardDelays();
    ASSERT_EQ(delays.size(), 4U);
    EXPECT_EQ(delays[0], 0.0);
    EXPECT_EQ(delays[1], 3.0);
    EXPECT_EQ(delays[2], 5.0);
    EXPECT_TRUE(std::isinf(delays[3])) << "Last delay should be PLUGGED_DELAY (Infinity)";
    EXPECT_EQ(query(dbFile, "SELECT delays FROM motors"), "0,3,5,P");
}

// Legacy databases store delays dash-separated (e.g. "0-3-5-7-P").
TEST(SqliteMotorDatabaseReader, ParseDelaysDashSeparatedWithPlugged)
{
    const TempDir               tempDir;
    const std::vector<MotorPtr> motors{
        testMotor("TestCo", "C6", {0, 3, 5, 7}, {0.0, 20.0, 0.0}, 0.06)};
    const std::filesystem::path dbFile = tempDir.resolve("dashdel.db");
    ASSERT_TRUE(SqliteMotorDatabaseReader::writeDatabase(dbFile, motors));

    // Manually overwrite the delays column with legacy dash-separated format
    exec(dbFile, "UPDATE motors SET delays = '0-3-5-7-P'");

    const SqliteMotorDatabaseReader::Contents loaded = read(dbFile);
    ASSERT_EQ(loaded.motors.size(), 1U);
    const std::vector<double>& delays = loaded.motors.front()->getStandardDelays();
    ASSERT_EQ(delays.size(), 5U);
    EXPECT_EQ(delays[0], 0.0);
    EXPECT_EQ(delays[1], 3.0);
    EXPECT_EQ(delays[2], 5.0);
    EXPECT_EQ(delays[3], 7.0);
    EXPECT_TRUE(std::isinf(delays[4])) << "Last delay should be PLUGGED_DELAY (Infinity)";
}

// ---------------------------------------------------------------- validation failures

TEST(SqliteMotorDatabaseReader, ReportsAMissingFile)
{
    const TempDir               tempDir;
    const std::filesystem::path missing = tempDir.resolve("missing.db");
    EXPECT_EQ(readError(missing), "SQLite motor database not found: " + utf8(missing));
    // A directory is no database file either.
    EXPECT_EQ(readError(tempDir.resolve("")),
              "SQLite motor database not found: " + utf8(tempDir.resolve("")));
    EXPECT_FALSE(SqliteMotorDatabaseReader::validateDatabase(missing));
}

TEST(SqliteMotorDatabaseReader, RejectsAFileThatIsNoDatabase)
{
    const TempDir               tempDir;
    const std::filesystem::path garbage = tempDir.resolve("garbage.db");
    ASSERT_TRUE(QtRocket::writeTextFile(garbage, std::string(4096, 'x')));
    EXPECT_FALSE(readError(garbage).empty());
}

TEST(SqliteMotorDatabaseReader, RequiresEveryTable)
{
    const TempDir                                          tempDir;
    const std::vector<std::pair<std::string, std::string>> steps{
        {"meta", "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"},
        {"manufacturers", "CREATE TABLE manufacturers (id INTEGER PRIMARY KEY);"},
        {"motors", "CREATE TABLE motors (id INTEGER PRIMARY KEY);"},
        {"thrust_curves", "CREATE TABLE thrust_curves (id INTEGER PRIMARY KEY);"},
        {"thrust_data", "CREATE TABLE thrust_data (id INTEGER PRIMARY KEY);"},
    };
    std::string schema;
    for (const auto& [table, sql] : steps)
    {
        const std::filesystem::path dbFile = tempDir.resolve(table + ".db");
        // An empty file is a valid, empty SQLite database.
        ASSERT_TRUE(QtRocket::writeTextFile(dbFile, ""));
        if (!schema.empty())
        {
            exec(dbFile, schema);
        }
        EXPECT_EQ(readError(dbFile), "SQLite motor database missing " + table + " table");
        schema += sql;
    }
}

/// The failure of reading a version-3 database whose meta table holds only what @p insert puts
/// there.
[[nodiscard]] std::string schemaVersionError(const TempDir& tempDir, std::string_view name,
                                             std::string_view insert)
{
    const std::filesystem::path dbFile = tempDir.resolve(name);
    exec(dbFile, looseSchema(3) + "DELETE FROM meta;" + std::string(insert));
    return readError(dbFile);
}

TEST(SqliteMotorDatabaseReader, ChecksTheSchemaVersion)
{
    const TempDir tempDir;
    EXPECT_EQ(schemaVersionError(tempDir, "none.db", ""),
              "SQLite motor database missing schema_version metadata");
    EXPECT_EQ(
        schemaVersionError(tempDir, "v1.db", "INSERT INTO meta VALUES ('schema_version', '1');"),
        "Unsupported thrust curve database schema version: 1 (supported 2-3)");
    EXPECT_EQ(
        schemaVersionError(tempDir, "v4.db", "INSERT INTO meta VALUES ('schema_version', '4');"),
        "Unsupported thrust curve database schema version: 4 (supported 2-3)");
    // Integer.parseInt: no whitespace, no fraction.
    EXPECT_EQ(schemaVersionError(tempDir, "space.db",
                                 "INSERT INTO meta VALUES ('schema_version', ' 3');"),
              "SQLite motor database has invalid schema_version metadata");
    EXPECT_EQ(schemaVersionError(tempDir, "real.db",
                                 "INSERT INTO meta VALUES ('schema_version', '3.0');"),
              "SQLite motor database has invalid schema_version metadata");
}

TEST(SqliteMotorDatabaseReader, ChecksTheMotorColumnsOfTheVersion)
{
    const TempDir tempDir;
    // A version-2 database whose motors table lacks a column.
    const std::filesystem::path v2 = tempDir.resolve("v2missing.db");
    exec(v2, looseSchema(2) + "ALTER TABLE motors DROP COLUMN sparky;");
    EXPECT_EQ(readError(v2),
              "SQLite motor database table 'motors' missing required columns: [sparky]");
    // Version 3 needs description and source.
    const std::filesystem::path v3 = tempDir.resolve("v3missing.db");
    exec(v3, looseSchema(2) + "UPDATE meta SET value = '3';");
    EXPECT_EQ(readError(v3),
              "SQLite motor database table 'motors' missing required columns: "
              "[description, source]");
}

TEST(SqliteMotorDatabaseReader, ChecksForeignKeys)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("orphan.db");
    // No manufacturer 7: the motor's foreign key is broken.
    createDatabase(dbFile, "(1, 7, 'A8', 'A8', 18, 70, 3, 16, 'SU', '3')");
    EXPECT_EQ(readError(dbFile), "SQLite motor database failed foreign_key_check for table motors");
}

// ---------------------------------------------------------------- reading details

TEST(SqliteMotorDatabaseReader, ReportsTheCurvesItSkips)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("skips.db");
    createDatabase(dbFile,
                   "(1, 1, 'NOCURVE', 'N1', 18, 70, 3, 16, 'SU', ''),"
                   "(2, 1, 'A8-3', 'A8', 18, 70, 3, 16, 'SU', '3')",
                   "INSERT INTO thrust_curves (id, motor_id, source) VALUES "
                   "(10, 2, 'cert'), (11, 2, 'mfr'), (12, 2, 'user'), (13, 2, NULL);"
                   // 10: no points; 11: one point; 12: a thrust above the maximum; 13: fine
                   "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES "
                   "(11, 0.5, 3),"
                   "(12, 0, 0), (12, 0.5, 20000000), (12, 1, 0),"
                   "(13, 0, 0), (13, 0.5, 5), (13, 1, 0);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 1U);
    EXPECT_EQ(contents.motors[0]->getDesignation(), "A8");
    EXPECT_EQ(contents.motors[0]->getCode(), "A8-3");
    EXPECT_EQ(contents.motors[0]->getThrustPoints()[1], 5);

    const std::vector<SkippedCurve> expected{
        {.reason      = Reason::NO_CURVES,
         .motorId     = 1,
         .curveId     = std::nullopt,
         .designation = "NOCURVE",
         .message     = ""},
        {.reason      = Reason::NO_DATA,
         .motorId     = 2,
         .curveId     = 10,
         .designation = "A8-3",
         .message     = ""},
        {.reason      = Reason::INVALID_DATA,
         .motorId     = 2,
         .curveId     = 11,
         .designation = "A8-3",
         .message     = ""},
        {.reason      = Reason::INVALID_MOTOR,
         .motorId     = 2,
         .curveId     = 12,
         .designation = "A8-3",
         .message     = "Invalid thrust 2.0E7"},
    };
    EXPECT_EQ(contents.skipped, expected);
}

TEST(SqliteMotorDatabaseReader, OrdersCurvesBySource)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("order.db");
    createDatabase(dbFile, "(1, 1, 'A8', 'A8', 18, 70, 3, 16, 'SU', '3')",
                   "INSERT INTO thrust_curves (id, motor_id, source) VALUES "
                   "(1, 1, 'user'), (2, 1, 'other'), (3, 1, 'mfr'), (4, 1, 'cert');"
                   "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES "
                   "(1, 0, 0), (1, 0.5, 1), (1, 1, 0), (2, 0, 0), (2, 0.5, 2), (2, 1, 0),"
                   "(3, 0, 0), (3, 0.5, 3), (3, 1, 0), (4, 0, 0), (4, 0.5, 4), (4, 1, 0);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    std::vector<double>                       order;
    order.reserve(contents.motors.size());
    for (const MotorPtr& motor : contents.motors)
    {
        order.push_back(motor->getThrustPoints()[1]);
    }
    // cert, mfr, user, then any other source.
    EXPECT_EQ(order, (std::vector<double>{4, 3, 1, 2}));
}

TEST(SqliteMotorDatabaseReader, NormalisesThrustData)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("normalise.db");
    createDatabase(dbFile, "(1, 1, 'A8', 'A8', 18, 70, 3, 16, 'SU', '3')",
                   "INSERT INTO thrust_curves (id, motor_id, source) VALUES (1, 1, 'cert');"
                   // Starts late; a negative thrust; two points within 0.1 ms; stored out of order.
                   "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES "
                   "(1, 1.0, 0), (1, 0.2, -1), (1, 0.5, 4), (1, 0.50005, 6), (1, 0.50004, 5);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 1U);
    const ThrustCurveMotor& motor = *contents.motors.front();
    EXPECT_EQ(motor.getTimePoints(), (std::vector<double>{0, 0.2, 0.5, 1.0}));
    EXPECT_EQ(motor.getThrustPoints(), (std::vector<double>{0, 0, 6, 0}));
    EXPECT_EQ(motor.getDigest(), MotorDigest::digestMotor(motor));
}

/// Five motors whose rows and curves exercise calculateCGPoints() and the length default.
[[nodiscard]] SqliteMotorDatabaseReader::Contents readMassDatabase(const TempDir& tempDir)
{
    const std::filesystem::path dbFile = tempDir.resolve("mass.db");
    createDatabase(
        dbFile,
        "(1, 1, 'A', 'A', 18, 70, 4, 20, 'SU', ''),"    // impulse-proportional
        "(2, 1, 'B', 'B', 18, 70, 0, 20, 'SU', ''),"    // no propellant: constant
        "(3, 1, 'C', 'C', 18, 70, 4, 20, 'SU', ''),"    // no impulse: linear
        "(4, 1, 'D', 'D', 18, NULL, 4, 20, 'SU', ''),"  // no length: 3 diameters
        "(5, 1, 'E', 'E', NULL, 0, 4, 20, 'SU', '')",   // neither: 0.1 m
        "INSERT INTO thrust_curves (id, motor_id) VALUES (1, 1), (2, 2), (3, 3), (4, 4), "
        "(5, 5);"
        "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES "
        "(1, 0, 0), (1, 1, 10), (1, 3, 0),"
        "(2, 0, 0), (2, 1, 10), (2, 3, 0),"
        "(3, 0, 0), (3, 1, 0), (3, 2, 0),"
        "(4, 0, 0), (4, 1, 10), (4, 3, 0),"
        "(5, 0, 0), (5, 1, 10), (5, 3, 0);");
    return read(dbFile);
}

TEST(SqliteMotorDatabaseReader, MassFromTheMotorRow)
{
    const TempDir                             tempDir;
    const SqliteMotorDatabaseReader::Contents contents = readMassDatabase(tempDir);
    ASSERT_EQ(contents.motors.size(), 5U);

    // Impulse 5 then 10: a third and two thirds of the 4 g of propellant.
    assertCoordinatesMatch(contents.motors[0]->getCGPoints(), {0.035, 0.035, 0.035},
                           {0.020, 0.020 - (0.004 / 3), 0.016});
    // No propellant: constant; no impulse: linear in time.
    assertCoordinatesMatch(contents.motors[1]->getCGPoints(), {0.035, 0.035, 0.035},
                           {0.020, 0.020, 0.020});
    assertCoordinatesMatch(contents.motors[2]->getCGPoints(), {0.035, 0.035, 0.035},
                           {0.020, 0.018, 0.016});
}

TEST(SqliteMotorDatabaseReader, LengthFromTheMotorRow)
{
    const TempDir                             tempDir;
    const SqliteMotorDatabaseReader::Contents contents = readMassDatabase(tempDir);
    ASSERT_EQ(contents.motors.size(), 5U);
    EXPECT_DOUBLE_EQ(contents.motors[3]->getLength(), 0.054);
    EXPECT_DOUBLE_EQ(contents.motors[3]->getLaunchCGx(), 0.027);
    EXPECT_DOUBLE_EQ(contents.motors[4]->getLength(), 0.1);
    EXPECT_DOUBLE_EQ(contents.motors[4]->getDiameter(), 0.0);
}

TEST(SqliteMotorDatabaseReader, DigestAndInitialMassFromTheMotorRow)
{
    const TempDir                             tempDir;
    const SqliteMotorDatabaseReader::Contents contents = readMassDatabase(tempDir);
    ASSERT_EQ(contents.motors.size(), 5U);
    const auto digestedAsMotor = [](const MotorPtr& motor) {
        return motor->getDigest() == MotorDigest::digestMotor(*motor);
    };
    EXPECT_TRUE(std::ranges::all_of(contents.motors, digestedAsMotor));
    const auto initialMass = [](const MotorPtr& motor) { return motor->getInitialMass(); };
    EXPECT_TRUE(std::ranges::all_of(
        contents.motors, [](double mass) { return mass == 0.020; }, initialMass));
}

TEST(SqliteMotorDatabaseReader, ReadsTypesManufacturersAndLooseColumns)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("types.db");
    exec(dbFile,
         std::string(kSchemaV2) +
             "INSERT INTO manufacturers (id, name, abbrev) VALUES "
             "(2, 'Some Rocket Co', '  '), (3, '', NULL);"
             "INSERT INTO motors (id, manufacturer_id, designation, type, diameter, length, "
             "total_weight, propellant_weight, sparky, data_files, delays) VALUES "
             "(1, 1, 'M1', 'single', '29d', 100, 20, 4, '1', 2.7, 'S,M, 7 ,plugged'),"
             "(2, 2, 'M2', 'RE', 29, 100, 20, 4, 0, 'abc', NULL),"
             "(3, 3, 'M3', 'Hy', 29, 100, 20, 4, NULL, 5, '3,x'),"
             "(4, 1, 'M4', ' SU', 29, 100, 20, 4, 2, NULL, NULL),"
             "(5, 1, 'M5', NULL, 29, 100, 20, 4, NULL, '12', NULL);"
             "INSERT INTO thrust_curves (id, motor_id) VALUES (1, 1), (2, 2), (3, 3), "
             "(4, 4), (5, 5);"
             "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES "
             "(1, 0, 0), (1, 1, 5), (1, 2, 0), (2, 0, 0), (2, 1, 5), (2, 2, 0),"
             "(3, 0, 0), (3, 1, 5), (3, 2, 0), (4, 0, 0), (4, 1, 5), (4, 2, 0),"
             "(5, 0, 0), (5, 1, 5), (5, 2, 0);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 5U);
    const std::vector<MotorPtr>& m = contents.motors;

    EXPECT_EQ(m[0]->getMotorType(), Motor::Type::SINGLE);
    EXPECT_EQ(m[1]->getMotorType(), Motor::Type::RELOAD);
    EXPECT_EQ(m[2]->getMotorType(), Motor::Type::HYBRID);
    EXPECT_EQ(m[3]->getMotorType(), Motor::Type::UNKNOWN);  // not trimmed
    EXPECT_EQ(m[4]->getMotorType(), Motor::Type::UNKNOWN);

    // The abbreviation, else the name, else "Unknown".
    EXPECT_EQ(m[0]->getManufacturer().getSimpleName(), "Estes");
    EXPECT_EQ(m[1]->getManufacturer().getDisplayName(), "Some Rocket Co");
    EXPECT_EQ(m[2]->getManufacturer().getDisplayName(), "Unknown");

    // Text that SQLite does not take for a number is parsed as Java's Double.parseDouble does
    // ("29d"); a real where an int is read is truncated.
    EXPECT_DOUBLE_EQ(m[0]->getDiameter(), 0.029);
    EXPECT_TRUE(m[0]->isSparky());
    EXPECT_EQ(m[0]->getDataFiles(), 2);
    EXPECT_FALSE(m[1]->isSparky());
    EXPECT_EQ(m[1]->getDataFiles(), std::nullopt);
    EXPECT_FALSE(m[2]->isSparky());
    EXPECT_EQ(m[2]->getDataFiles(), 5);
    EXPECT_TRUE(m[3]->isSparky());
    EXPECT_EQ(m[4]->getDataFiles(), 12);

    EXPECT_EQ(m[0]->getStandardDelays(), (std::vector<double>{7, kInf}));
    EXPECT_TRUE(m[1]->getStandardDelays().empty());
    EXPECT_EQ(m[2]->getStandardDelays(), std::vector<double>{3});
}

TEST(SqliteMotorDatabaseReader, ReadsVersion3DescriptionAndSource)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("v3.db");
    exec(dbFile, looseSchema(3) +
                     "INSERT INTO manufacturers (id, name, abbrev) VALUES (1, 'Estes', 'Estes');"
                     "INSERT INTO motors (id, manufacturer_id, designation, diameter, length, "
                     "total_weight, propellant_weight, description, source) VALUES "
                     "(1, 1, 'A8', 18, 70, 16, 3, 'A description', 'manual');"
                     "INSERT INTO thrust_curves (id, motor_id) VALUES (1, 1);"
                     "INSERT INTO thrust_data (id, curve_id, time_seconds, force_newtons) VALUES "
                     "(1, 1, 0, 0), (2, 1, 0.5, 5), (3, 1, 1, 0);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 1U);
    EXPECT_EQ(contents.motors.front()->getDescription(), "A description");
    EXPECT_EQ(contents.motors.front()->getDataSource(), "manual");
}

TEST(SqliteMotorDatabaseReader, ReadsNullTextColumnsAsEmpty)
{
    const TempDir               tempDir;
    const std::filesystem::path dbFile = tempDir.resolve("nulls.db");
    exec(dbFile, looseSchema(2) +
                     "INSERT INTO manufacturers (id, name, abbrev) VALUES (1, NULL, NULL);"
                     "INSERT INTO motors (id, manufacturer_id, total_weight) VALUES (1, 1, 16);"
                     "INSERT INTO thrust_curves (id, motor_id) VALUES (1, 1);"
                     "INSERT INTO thrust_data (id, curve_id, time_seconds, force_newtons) VALUES "
                     "(1, 1, 0, 0), (2, 1, 0.5, 5), (3, 1, 1, 0);");
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 1U);
    const ThrustCurveMotor& motor = *contents.motors.front();
    EXPECT_EQ(motor.getCode(), "");
    EXPECT_EQ(motor.getDesignation(), "");
    EXPECT_EQ(motor.getCommonName(), "");
    EXPECT_EQ(motor.getCaseInfo(), "");
    EXPECT_EQ(motor.getManufacturer().getDisplayName(), "Unknown");
    EXPECT_EQ(motor.getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_TRUE(motor.getStandardDelays().empty());
    EXPECT_DOUBLE_EQ(motor.getLength(), 0.1);
    // No propellant weight: the mass stays at the total weight.
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.016);
    EXPECT_DOUBLE_EQ(motor.getBurnoutMass(), 0.016);
}

// ---------------------------------------------------------------- writing details

TEST(SqliteMotorDatabaseReader, WritesTheMetadataAndTotals)
{
    const TempDir             tempDir;
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setCode("C6")
        .setDesignation("C6")
        .setCommonName("C6")
        .setDescription("  two\r\nlines \t here  ")
        .setTcMotorId("  ")
        .setInfoUrl(" http://x ")
        .setDataFiles(3)
        .setSparky(true)
        .setMotorType(Motor::Type::HYBRID)
        .setStandardDelays({2.5, 5})
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0, 0.5, 2})
        .setThrustPoints({0, 12, 0})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.025), Coordinate(0.035, 0, 0, 0.02),
                      Coordinate(0.035, 0, 0, 0.0125)})
        .setInitialMass(0.025);
    const std::vector<MotorPtr> motors{build(builder)};

    // The directory is created, and an existing file replaced.
    const std::filesystem::path dbFile = tempDir.resolve("sub/dir/motors.db");
    ASSERT_TRUE(SqliteMotorDatabaseReader::writeDatabase(dbFile, motors));
    ASSERT_TRUE(SqliteMotorDatabaseReader::writeDatabase(dbFile, motors));

    EXPECT_EQ(query(dbFile, "SELECT value FROM meta WHERE key = 'schema_version'"), "3");
    EXPECT_EQ(query(dbFile, "SELECT value FROM meta WHERE key = 'motor_count'"), "1");
    const std::string generatedAt =
        query(dbFile, "SELECT value FROM meta WHERE key = 'generated_at'").value_or("");
    EXPECT_TRUE(generatedAt.ends_with("Z") && generatedAt.size() >= 20 && generatedAt[10] == 'T')
        << generatedAt;
    EXPECT_TRUE(query(dbFile, "SELECT value FROM meta WHERE key = 'database_version'").has_value());
    EXPECT_EQ(query(dbFile, "SELECT COUNT(*) FROM motors"), "1");
    EXPECT_EQ(query(dbFile, "SELECT COUNT(*) FROM thrust_data"), "3");
    EXPECT_EQ(query(dbFile, "SELECT source || '/' || format FROM thrust_curves"),
              "openrocket/internal");

    // Impulse 3 + 9 = 12 Ns: a C; burn time 2 s; the average 6 N.
    EXPECT_EQ(query(dbFile, "SELECT impulse_class FROM motors"), "C");
    EXPECT_EQ(query(dbFile, "SELECT total_impulse FROM motors"), "12.0");
    EXPECT_EQ(query(dbFile, "SELECT avg_thrust FROM motors"), "6.0");
    EXPECT_EQ(query(dbFile, "SELECT max_thrust FROM motors"), "12.0");
    EXPECT_EQ(query(dbFile, "SELECT burn_time FROM motors"), "2.0");
    EXPECT_EQ(query(dbFile, "SELECT type FROM motors"), "hybrid");
    EXPECT_EQ(query(dbFile, "SELECT delays FROM motors"), "2.5,5");
    EXPECT_EQ(query(dbFile, "SELECT tc_motor_id FROM motors"), std::nullopt);
    EXPECT_EQ(query(dbFile, "SELECT info_url FROM motors"), "http://x");
    EXPECT_EQ(query(dbFile, "SELECT description FROM motors"), "two lines here");
    EXPECT_EQ(query(dbFile, "SELECT source FROM motors"), std::nullopt);
    EXPECT_EQ(query(dbFile, "SELECT sparky FROM motors"), "1");
    EXPECT_EQ(query(dbFile, "SELECT data_files FROM motors"), "3");
    EXPECT_EQ(query(dbFile, "SELECT abbrev FROM manufacturers"), "Estes");

    // And it reads back.
    const SqliteMotorDatabaseReader::Contents contents = read(dbFile);
    ASSERT_EQ(contents.motors.size(), 1U);
    EXPECT_EQ(contents.motors.front()->getMotorType(), Motor::Type::HYBRID);
    EXPECT_EQ(contents.motors.front()->getDescription(), "two lines here");
    EXPECT_EQ(contents.motors.front()->getStandardDelays(), (std::vector<double>{2.5, 5}));
}

TEST(SqliteMotorDatabaseReader, DelaysAndImpulseClasses)
{
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays(""), std::vector<double>{});
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays("   "), std::vector<double>{});
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays("0,3,5"), (std::vector<double>{0, 3, 5}));
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays("0-3--5,,P"),
              (std::vector<double>{0, 3, 5, kInf}));
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays(" 4 , PLUGGED "),
              (std::vector<double>{4, kInf}));
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays("S,M,L"), std::vector<double>{});
    EXPECT_EQ(SqliteMotorDatabaseReader::parseDelays("2.5,1e1"), (std::vector<double>{2.5, 10}));

    EXPECT_EQ(SqliteMotorDatabaseReader::formatDelays({}), std::nullopt);
    EXPECT_EQ(SqliteMotorDatabaseReader::formatDelays(std::vector<double>{0, 3, kInf}), "0,3,P");
    EXPECT_EQ(SqliteMotorDatabaseReader::formatDelays(std::vector<double>{2.5, -kInf}), "2.5,P");

    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(0), std::nullopt);
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(-1), std::nullopt);
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(1.25), "1/4A");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(1.2500001), "1/2A");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(2.5), "1/2A");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(5), "A");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(10), "B");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(640), "H");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(40960), "N");
    EXPECT_EQ(SqliteMotorDatabaseReader::getImpulseClass(40960.5), "O");
}

}  // namespace
