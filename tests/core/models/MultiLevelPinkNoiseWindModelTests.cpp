#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Signal.h"

namespace
{

namespace MathUtil = QtRocket::MathUtil;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::DegreeUnit;
using QtRocket::ErrorCode;
using QtRocket::InMemoryPreferences;
using QtRocket::ModId;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Result;
using QtRocket::Unit;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;
using QtRocket::WindModel;
using LevelWindModel = MultiLevelPinkNoiseWindModel::LevelWindModel;

constexpr double      kEpsilon    = MathUtil::kEpsilon;
constexpr double      kDeltaT     = PinkNoiseWindModel::kDeltaT;
constexpr std::size_t kSampleSize = 1000;
constexpr double      kPi         = std::numbers::pi;
/// The seed verifyWind() gives the levels: OpenRocket's test leaves them randomly seeded.
constexpr int kSeed = 20240917;

static_assert(QtRocket::Monitorable<MultiLevelPinkNoiseWindModel>);

/// A CSV file in the temporary directory, removed again when the test ends.
class TempCsv
{
public:
    explicit TempCsv(const std::vector<std::string>& lines, std::string_view lineEnd = "\n")
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_path = std::filesystem::temp_directory_path() /
                 std::format("qtrocket_{}_{}_{}.csv", info->test_suite_name(), info->name(),
                             std::random_device{}());
        std::string text;
        for (const std::string& line : lines)
        {
            text += line;
            text += lineEnd;
        }
        EXPECT_TRUE(QtRocket::writeTextFile(m_path, text).has_value());
    }
    ~TempCsv()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }
    TempCsv(const TempCsv&)            = delete;
    TempCsv& operator=(const TempCsv&) = delete;
    TempCsv(TempCsv&&)                 = delete;
    TempCsv& operator=(TempCsv&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

/// Counts the emissions of a signal while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

const Unit* siDistance()
{
    return &unitGroup(UnitGroupId::DISTANCE).getSIUnit();
}

const Unit* siWindSpeed()
{
    return &unitGroup(UnitGroupId::WINDSPEED).getSIUnit();
}

/// MultiLevelWindModelTest: OpenRocket's test preferences give 0 for every double, so the
/// initial level is at 0 m with no wind from the north.
class MultiLevelWindModelTest : public ::testing::Test
{
protected:
    MultiLevelWindModelTest() : m_model(zeroWind(m_prefs)) { }

    static const InMemoryPreferences& zeroWind(InMemoryPreferences& preferences)
    {
        preferences.setWindAverage(0.0);
        preferences.setWindTurbulenceIntensity(0.0);
        preferences.setWindDirection(0.0);
        return preferences;
    }

    /// MultiLevelWindModelTest.verifyWind: the mean speed and direction over kSampleSize samples.
    void verifyWind(double altitude, double expectedSpeed, double expectedDirection,
                    double standardDeviation)
    {
        m_model.setSeed(kSeed);  // deterministic, where Java's levels are randomly seeded
        double sumSpeed = 0;
        double sumSin   = 0;
        double sumCos   = 0;
        for (std::size_t i = 0; i < kSampleSize; ++i)
        {
            const Coordinate velocity =
                m_model.getWindVelocity(static_cast<double>(i) * kDeltaT, altitude);
            sumSpeed += velocity.length();
            const double direction = std::atan2(velocity.x, velocity.y);
            sumSin += std::sin(direction);
            sumCos += std::cos(direction);
        }
        const double avgSpeed     = sumSpeed / static_cast<double>(kSampleSize);
        const double avgDirection = std::atan2(sumSin, sumCos);

        EXPECT_NEAR(avgSpeed, expectedSpeed, standardDeviation)
            << "Average wind speed at altitude " << altitude;
        EXPECT_NEAR(avgDirection, expectedDirection, kEpsilon)
            << "Average wind direction at altitude " << altitude;
    }

    /// importLevelsFromCsv(file, fieldSeparator) that must succeed.
    void importCsv(const TempCsv& csv, std::string_view separator)
    {
        const Result<void> result = m_model.importLevelsFromCsv(csv.path(), separator);
        EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().toString());
    }

    void addLevel(double altitude, double speed, double direction,
                  std::optional<double> standardDeviation)
    {
        const Result<void> result =
            m_model.addWindLevel(altitude, speed, direction, standardDeviation);
        EXPECT_TRUE(result.has_value()) << altitude;
    }

    /// The error of importing @p lines with the default settings, which must fail.
    QtRocket::Error importError(const std::vector<std::string>& lines)
    {
        const TempCsv      csv(lines);
        const Result<void> result = m_model.importLevelsFromCsv(csv.path(), ",");
        EXPECT_FALSE(result.has_value());
        return result.has_value() ? QtRocket::Error{} : result.error();
    }

    /// Imports @p csv without headers: the altitude column @p altitude, speed 1, direction 2 and
    /// the deviation column @p stdDeviation.
    Result<void> importByIndex(const TempCsv& csv, std::string_view separator,
                               std::string_view altitude, std::string_view stdDeviation)
    {
        const DegreeUnit degrees;
        return m_model.importLevelsFromCsv(csv.path(), separator, altitude, "1", "2", stdDeviation,
                                           siDistance(), siWindSpeed(), &degrees, siWindSpeed(),
                                           false);
    }

    /// A change a slot makes to a model's levels.
    using LevelsChange = std::function<void(MultiLevelPinkNoiseWindModel&)>;

    /// What happened when a slot changed the levels during a level's setter.
    struct ReentryOutcome
    {
        bool        slotRan{false};
        std::size_t levelsAfter{0};
        double      lowestAltitude{std::numeric_limits<double>::quiet_NaN()};
        int         emissions{0};  ///< of the model's changed()
    };

    /// A model with levels at 0 m and 100 m (5 m/s, deviation 1), whose changed() slot makes
    /// @p change on its first call; then the 100 m level's setSpeed(6), which emits twice.
    [[nodiscard]] ReentryOutcome changeLevelsDuringSetSpeed(const LevelsChange& change) const
    {
        MultiLevelPinkNoiseWindModel model(m_prefs);
        EXPECT_TRUE(model.addWindLevel(100, 5, 0, 1.0).has_value());
        ReentryOutcome                             outcome;
        const QtRocket::Signal<>::ScopedConnection connection{model.changed().connect([&] {
            ++outcome.emissions;
            if (!outcome.slotRan)
            {
                outcome.slotRan = true;
                change(model);
            }
        })};

        model.getLevels()[1]->setSpeed(6);

        outcome.levelsAfter = model.getLevels().size();
        return outcome;
    }

    /// A model with levels at 0 m and 100 m, where a slot on the 100 m level's own changed()
    /// removes that level on its first call; then @p set on the level.
    [[nodiscard]] ReentryOutcome removeLevelFromItsOwnSlot(
        const std::function<void(LevelWindModel&)>& set) const
    {
        MultiLevelPinkNoiseWindModel model(m_prefs);
        EXPECT_TRUE(model.addWindLevel(100, 5, 0, 1.0).has_value());
        ReentryOutcome  outcome;
        LevelWindModel* level = model.getLevels()[1];
        level->changed().connect([&] {
            if (!outcome.slotRan)
            {
                outcome.slotRan = true;
                model.removeWindLevelIdx(1);
            }
        });

        set(*level);  // `level` dangles once the setter has returned

        const std::vector<LevelWindModel*> levels = model.getLevels();
        outcome.levelsAfter                       = levels.size();
        if (!levels.empty())
        {
            outcome.lowestAltitude = levels.front()->getAltitude();
        }
        return outcome;
    }

    InMemoryPreferences          m_prefs;
    MultiLevelPinkNoiseWindModel m_model;
};

// ---- Ported from MultiLevelWindModelTest.java ----

TEST_F(MultiLevelWindModelTest, AddAndRemoveWindLevels)
{
    addLevel(100, 5, kPi / 4, 1.0);
    addLevel(200, 10, kPi / 2, 1.0);
    EXPECT_EQ(m_model.getLevels().size(), 3U);

    m_model.removeWindLevel(100);
    ASSERT_EQ(m_model.getLevels().size(), 2U);
    EXPECT_NEAR(m_model.getLevels()[1]->getAltitude(), 200, kEpsilon);

    m_model.removeWindLevel(0);
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_NEAR(m_model.getLevels()[0]->getAltitude(), 200, kEpsilon);

    m_model.removeWindLevel(0);
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_NEAR(m_model.getLevels()[0]->getAltitude(), 200, kEpsilon);

    m_model.removeWindLevel(200);
    EXPECT_TRUE(m_model.getLevels().empty());
}

TEST_F(MultiLevelWindModelTest, AddingADuplicateAltitudeFails)
{
    EXPECT_FALSE(m_model.addWindLevel(0, 0, 0, 1.0).has_value());

    addLevel(100, 5, kPi / 4, 1.0);
    const Result<void> duplicate = m_model.addWindLevel(100, 10, kPi / 2, 1.0);
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(duplicate.error().message, "Wind level already exists for altitude: 100.0");
    EXPECT_EQ(m_model.getLevels().size(), 2U);
}

TEST_F(MultiLevelWindModelTest, GetWindVelocity)
{
    m_model.clearLevels();
    addLevel(0, 5, 0, 1.0);
    addLevel(1000, 10, kPi / 2, 2.0);

    verifyWind(0, 5, 0, 1);
    verifyWind(1000, 10, kPi / 2, 2);
}

TEST_F(MultiLevelWindModelTest, InterpolationBetweenLevels)
{
    // Speed interpolation
    m_model.getLevels()[0]->setSpeed(5);
    m_model.getLevels()[0]->setStandardDeviation(0.1);
    addLevel(1000, 10, 0, 0.3);

    verifyWind(200, 6, 0, 0.14);
    verifyWind(500, 7.5, 0, 0.2);
    verifyWind(900, 9.5, 0, 0.28);

    m_model.clearLevels();

    // Direction interpolation when the velocity vectors are parallel
    addLevel(0, 5, 0, 0.0);
    addLevel(1000, 5, kPi, 0.0);

    verifyWind(200, 3, 0, kEpsilon);
    verifyWind(501, 0, kPi, 0.01);
    verifyWind(900, 4, kPi, kEpsilon);

    m_model.clearLevels();

    // Direction interpolation when the velocity vectors are not parallel
    addLevel(0, 5, 0, 0.0);
    addLevel(1000, 5, kPi / 2, 0.0);

    verifyWind(200, 4.1231056256, 0.2449786631, kEpsilon);
    verifyWind(500, 3.5355339059, kPi / 4, kEpsilon);
    verifyWind(800, 4.1231056256, 1.3258176637, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ExtrapolationOutsideLevels)
{
    addLevel(100, 5, 0, 1.4);
    addLevel(200, 10, kPi / 2, 2.2);

    verifyWind(0, 0, 0, 0);
    verifyWind(300, 10, kPi / 2, 2.2);
    verifyWind(1000, 10, kPi / 2, 2.2);
}

TEST_F(MultiLevelWindModelTest, SortLevels)
{
    addLevel(200, 10, kPi / 2, 1.0);
    addLevel(100, 5, kPi / 4, 1.0);
    addLevel(300, 15, 3 * kPi / 4, 1.0);

    m_model.sortLevels();

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 4U);
    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[1]->getAltitude(), 100, kEpsilon);
    EXPECT_NEAR(levels[2]->getAltitude(), 200, kEpsilon);
    EXPECT_NEAR(levels[3]->getAltitude(), 300, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, Clone)
{
    addLevel(100, 5, kPi / 4, 1.0);
    addLevel(200, 10, kPi / 2, 2.0);

    const std::unique_ptr<WindModel> clone = m_model.clone();
    auto* clonedModel = dynamic_cast<MultiLevelPinkNoiseWindModel*>(clone.get());
    ASSERT_NE(clonedModel, nullptr);
    EXPECT_NE(clonedModel, &m_model);
    EXPECT_TRUE(m_model == *clonedModel);

    ASSERT_TRUE(clonedModel->addWindLevel(300, 15, 3 * kPi / 4, 1.0).has_value());
    EXPECT_FALSE(m_model == *clonedModel);
}

TEST_F(MultiLevelWindModelTest, LoadFrom)
{
    addLevel(100, 5, kPi / 4, 2.0);
    addLevel(200, 10, kPi / 2, 1.0);

    MultiLevelPinkNoiseWindModel newModel(m_prefs);
    newModel.loadFrom(m_model);

    EXPECT_TRUE(m_model == newModel);
}

TEST_F(MultiLevelWindModelTest, ModIdIsZero)
{
    EXPECT_EQ(m_model.modId(), ModId::zero());
}

TEST_F(MultiLevelWindModelTest, ChangeListeners)
{
    bool       listenerCalled = false;
    const auto connection = m_model.changed().connect([&listenerCalled] { listenerCalled = true; });

    m_model.fireChangeEvent();
    EXPECT_TRUE(listenerCalled);

    listenerCalled = false;
    EXPECT_TRUE(m_model.changed().disconnect(connection));
    m_model.fireChangeEvent();
    EXPECT_FALSE(listenerCalled);
}

TEST_F(MultiLevelWindModelTest, ImportLevelsFromCsv)
{
    const TempCsv csv(
        {"altitude,speed,direction,stddev", "0,5,0,1.5", "100,10,45,2.0", "1000,15,90,2.5"});
    importCsv(csv, ",");

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U);

    // First level
    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, kEpsilon);

    // Middle level
    EXPECT_NEAR(levels[1]->getAltitude(), 100, kEpsilon);
    EXPECT_NEAR(levels[1]->getSpeed(), 10, kEpsilon);
    EXPECT_NEAR(levels[1]->getDirection(), kPi / 4, kEpsilon);  // 45 degrees
    EXPECT_NEAR(levels[1]->getStandardDeviation(), 2.0, kEpsilon);

    // Importing with the wrong separator
    EXPECT_FALSE(m_model.importLevelsFromCsv(csv.path(), ";").has_value());
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithDifferentNumberFormats)
{
    const TempCsv csv({"altitude,speed,direction,stddev",
                       "0,5.5,0,1.5",          // Standard format
                       "100,10,2,2.0",         // Integer
                       "1000,15.0,90,2.5",     // With trailing zero
                       "3000,8.000,10,2.5"});  // Multiple trailing zeros
    importCsv(csv, ",");

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 4U);
    EXPECT_NEAR(levels[0]->getSpeed(), 5.5, kEpsilon);
    EXPECT_NEAR(levels[2]->getSpeed(), 15.0, kEpsilon);
    EXPECT_NEAR(levels[3]->getSpeed(), 8.0, kEpsilon);
    EXPECT_NEAR(levels[2]->getDirection(), kPi / 2, kEpsilon);  // 90 degrees
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithNegativeValues)
{
    const TempCsv csv({"altitude,speed,direction,stddev",
                       "0,-5,0,1.5",          // Negative speed
                       "100,10,-45,2.0",      // Negative direction
                       "1000,-15,-90,2.5"});  // Negative speed and direction
    importCsv(csv, ",");

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), kPi, kEpsilon);          // 180 degrees
    EXPECT_NEAR(levels[1]->getDirection(), 7 * kPi / 4, kEpsilon);  // 315 degrees
    EXPECT_NEAR(levels[2]->getSpeed(), 15, kEpsilon);
    EXPECT_NEAR(levels[2]->getDirection(), kPi / 2, kEpsilon);  // 90 degrees
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithMissingValue)
{
    const TempCsv csv({"altitude,speed,direction,stddev", "0,5,0,1.5",
                       "100,10,45",  // Missing value
                       "1000,15,90,2.5"});
    EXPECT_FALSE(m_model.importLevelsFromCsv(csv.path(), ",").has_value());
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithEuropeanNumberFormat)
{
    const TempCsv csv({"altitude;speed;direction;stddev",
                       "0;5;0;1,5",           // European format with comma
                       "100;10;45;2,0",       // European format
                       "1000;15,2;90;2,5"});  // European format with multiple commas
    importCsv(csv, ";");                      // Note: using semicolon as separator

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, kEpsilon);
    EXPECT_NEAR(levels[1]->getStandardDeviation(), 2.0, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportLevelsErrorCases)
{
    {
        // Missing speed column
        const TempCsv csv({"altitude,direction,stddev", "0,0,1.5"});
        EXPECT_FALSE(m_model.importLevelsFromCsv(csv.path(), ",").has_value());
    }
    {
        // Invalid number format
        const TempCsv csv({"altitude,speed,direction,stddev", "0,invalid,0,1.5"});
        EXPECT_FALSE(m_model.importLevelsFromCsv(csv.path(), ",").has_value());
    }
    {
        // Empty value
        const TempCsv csv({"altitude,speed,direction,stddev", "0,,0,1.5"});
        EXPECT_FALSE(m_model.importLevelsFromCsv(csv.path(), ",").has_value());
    }
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithDifferentColumnOrders)
{
    const TempCsv csv({"direction,speed,altitude,stddev",  // Different order
                       "0,5,0,1.5", "45,10,100,2.0"});
    importCsv(csv, ",");

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportLevelsWithExtraColumns)
{
    const TempCsv csv({"altitude,speed,direction,stddev,extra1,extra2",  // Extra columns
                       "0,5,0,1.5,100,200", "100,10,45,2.0,300,400"});
    importCsv(csv, ",");

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportWithCustomColumnNames)
{
    const TempCsv csv(
        {"height,velocity,angle,variation", "0,5,0,1.5", "100,10,45,2.0", "1000,15,90,2.5"});
    const DegreeUnit degrees;
    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ",", "height", "velocity", "angle",
                                         "variation", siDistance(), siWindSpeed(), &degrees,
                                         siWindSpeed(), true)
                    .has_value());

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U) << "Should have 3 wind levels";

    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, kEpsilon);

    EXPECT_NEAR(levels[1]->getAltitude(), 100, kEpsilon);
    EXPECT_NEAR(levels[1]->getSpeed(), 10, kEpsilon);
    EXPECT_NEAR(levels[1]->getDirection(), kPi / 4, kEpsilon);  // 45 degrees
    EXPECT_NEAR(levels[1]->getStandardDeviation(), 2.0, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportWithColumnIndices)
{
    // No headers
    const TempCsv    csv({"0,5,0,1.5", "100,10,45,2.0", "1000,15,90,2.5"});
    const DegreeUnit degrees;
    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ",", "0", "1", "2", "3", siDistance(),
                                         siWindSpeed(), &degrees, siWindSpeed(), false)
                    .has_value());

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U) << "Should have 3 wind levels";

    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, kEpsilon);

    EXPECT_NEAR(levels[1]->getAltitude(), 100, kEpsilon);
    EXPECT_NEAR(levels[1]->getSpeed(), 10, kEpsilon);
    EXPECT_NEAR(levels[1]->getDirection(), kPi / 4, kEpsilon);  // 45 degrees
}

TEST_F(MultiLevelWindModelTest, ImportWithCustomUnits)
{
    // Altitude in feet, speed in mph, direction in degrees, deviation in mph
    const TempCsv csv({"altitude,speed,direction,stddev",
                       "0,11.2,0,3.4",        // 0m, 5m/s, 0rad, 1.5m/s
                       "328,22.4,45,4.5",     // 100m, 10m/s, PI/4rad, 2.0m/s
                       "3280,33.6,90,5.6"});  // 1000m, 15m/s, PI/2rad, 2.5m/s

    const Unit* feetUnit = unitGroup(UnitGroupId::DISTANCE).getUnit("ft");
    const Unit* mphUnit  = unitGroup(UnitGroupId::WINDSPEED).getUnit("mph");
    ASSERT_NE(feetUnit, nullptr);
    ASSERT_NE(mphUnit, nullptr);
    const DegreeUnit degreeUnit;

    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ",", "altitude", "speed", "direction",
                                         "stddev", feetUnit, mphUnit, &degreeUnit, mphUnit, true)
                    .has_value());

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U) << "Should have 3 wind levels";

    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, 0.1);  // ~5 m/s
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, 0.1);  // ~1.5 m/s

    EXPECT_NEAR(levels[1]->getAltitude(), 100, 0.1);            // ~100 m
    EXPECT_NEAR(levels[1]->getSpeed(), 10, 0.1);                // ~10 m/s
    EXPECT_NEAR(levels[1]->getDirection(), kPi / 4, kEpsilon);  // 45 degrees
}

TEST_F(MultiLevelWindModelTest, ImportWithOptionalStdDevColumn)
{
    const TempCsv    csv({"altitude,speed,direction", "0,5,0", "100,10,45", "1000,15,90"});
    const DegreeUnit degrees;
    // An empty deviation column name, and no unit needed for it
    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ",", "altitude", "speed", "direction", "",
                                         siDistance(), siWindSpeed(), &degrees, nullptr, true)
                    .has_value());

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U) << "Should have 3 wind levels";

    // The standard deviation is PinkNoiseWindModel's default
    const double defaultStdDev = PinkNoiseWindModel().getStandardDeviation();

    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), defaultStdDev, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportWithInvalidColumnIndices)
{
    const TempCsv    csv({"altitude,speed,direction,stddev", "0,5,0,1.5"});
    const DegreeUnit degrees;

    // An out-of-bounds index (with headers, the "indices" are column names that do not exist)
    EXPECT_FALSE(m_model
                     .importLevelsFromCsv(csv.path(), ",", "0", "1", "5", "3", siDistance(),
                                          siWindSpeed(), &degrees, siWindSpeed(), true)
                     .has_value());

    // A non-numeric index without headers
    EXPECT_FALSE(m_model
                     .importLevelsFromCsv(csv.path(), ",", "0", "speed", "2", "3", siDistance(),
                                          siWindSpeed(), &degrees, siWindSpeed(), false)
                     .has_value());
}

TEST_F(MultiLevelWindModelTest, ImportWithEuropeanUnitsAndFormat)
{
    const TempCsv csv({"hohe;geschwindigkeit;richtung;abweichung", "0;5,0;0;1,5", "100;10,0;45;2,0",
                       "1000;15,0;90;2,5"});
    const DegreeUnit degrees;
    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ";", "hohe", "geschwindigkeit", "richtung",
                                         "abweichung", siDistance(), siWindSpeed(), &degrees,
                                         siWindSpeed(), true)
                    .has_value());

    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U) << "Should have 3 wind levels";

    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5.0, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 1.5, kEpsilon);
}

TEST_F(MultiLevelWindModelTest, ImportWithMixedColumnReferences)
{
    const TempCsv    csv({"height,velocity,angle,variation", "0,5,0,1.5", "100,10,45,2.0"});
    const DegreeUnit degrees;
    // A mix of names and indices: OpenRocket's test accepts either outcome.
    const Result<void> result =
        m_model.importLevelsFromCsv(csv.path(), ",", "height", "1", "angle", "3", siDistance(),
                                    siWindSpeed(), &degrees, siWindSpeed(), true);
    if (!result.has_value())
    {
        // With headers the columns are names only, as in OpenRocket.
        EXPECT_EQ(result.error().message, "speed column \"1\" not found in header row.");
        return;
    }
    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U) << "Should have 2 wind levels";
    EXPECT_NEAR(levels[0]->getAltitude(), 0, kEpsilon);
    EXPECT_NEAR(levels[0]->getSpeed(), 5, kEpsilon);
    EXPECT_NEAR(levels[0]->getDirection(), 0, kEpsilon);
}

// ---- QtRocket additions: levels and notifications ----

TEST_F(MultiLevelWindModelTest, InitialLevelComesFromThePreferences)
{
    // OpenRocket's defaults: 2 m/s, intensity 0.1, from the east.
    const InMemoryPreferences                defaults;
    const MultiLevelPinkNoiseWindModel       fromDefaults(defaults);
    const std::vector<const LevelWindModel*> levels = fromDefaults.getLevels();
    ASSERT_EQ(levels.size(), 1U);
    EXPECT_EQ(levels[0]->getAltitude(), 0.0);
    EXPECT_EQ(levels[0]->getSpeed(), 2.0);
    EXPECT_NEAR(levels[0]->getStandardDeviation(), 0.2, 1e-15);
    EXPECT_EQ(levels[0]->getDirection(), kPi / 2);
    EXPECT_EQ(fromDefaults.getAltitudeReference(), WindModel::AltitudeReference::MSL);

    // resetLevels() goes back to it.
    addLevel(500, 3, 1, 0.5);
    InMemoryPreferences windy;
    windy.setWindAverage(9.0);
    windy.setWindDirection(1.0);
    m_model.resetLevels(windy);
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_EQ(m_model.getLevels()[0]->getSpeed(), 9.0);
    EXPECT_EQ(m_model.getLevels()[0]->getDirection(), 1.0);
}

TEST_F(MultiLevelWindModelTest, LevelsStaySortedWhenAdded)
{
    addLevel(300, 1, 0, std::nullopt);
    addLevel(100, 1, 0, std::nullopt);
    addLevel(200, 1, 0, std::nullopt);
    addLevel(-50, 1, 0, std::nullopt);
    std::vector<double> altitudes;
    for (const LevelWindModel* level : m_model.getLevels())
    {
        altitudes.push_back(level->getAltitude());
    }
    EXPECT_EQ(altitudes, (std::vector<double>{-50, 0, 100, 200, 300}));

    // A level without a deviation keeps 0; a negative speed turns the direction.
    addLevel(400, -3, 0, std::nullopt);
    const LevelWindModel* top = m_model.getLevels().back();
    EXPECT_EQ(top->getStandardDeviation(), 0.0);
    EXPECT_EQ(top->getSpeed(), 3.0);
    EXPECT_EQ(top->getDirection(), kPi);
}

TEST_F(MultiLevelWindModelTest, SetAltitudeDoesNotResort)
{
    addLevel(100, 1, 0, std::nullopt);
    addLevel(200, 2, 0, std::nullopt);
    m_model.getLevels()[1]->setAltitude(500);  // the level at 100 m moves above 200 m
    EXPECT_EQ(m_model.getLevels()[1]->getAltitude(), 500.0);
    EXPECT_EQ(m_model.getLevels()[2]->getAltitude(), 200.0);
    m_model.sortLevels();
    EXPECT_EQ(m_model.getLevels()[1]->getAltitude(), 200.0);
    EXPECT_EQ(m_model.getLevels()[2]->getAltitude(), 500.0);
}

TEST_F(MultiLevelWindModelTest, RemoveWindLevelMatchesWithDoubleEquality)
{
    addLevel(100, 1, 0, std::nullopt);
    m_model.removeWindLevel(-0.0);  // == 0.0
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_EQ(m_model.getLevels()[0]->getAltitude(), 100.0);
    m_model.removeWindLevel(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(m_model.getLevels().size(), 1U);
}

TEST_F(MultiLevelWindModelTest, RemoveWindLevelByIndex)
{
    addLevel(100, 1, 0, std::nullopt);
    addLevel(200, 1, 0, std::nullopt);
    m_model.removeWindLevelIdx(1);
    ASSERT_EQ(m_model.getLevels().size(), 2U);
    EXPECT_EQ(m_model.getLevels()[1]->getAltitude(), 200.0);
    EXPECT_THROW(m_model.removeWindLevelIdx(2), BugError);
    EXPECT_EQ(m_model.getLevels().size(), 2U);
}

TEST_F(MultiLevelWindModelTest, LevelChangesAreAnnounced)
{
    addLevel(100, 5, 0, 1.0);
    const ChangeCounter counter(m_model.changed());
    LevelWindModel*     level = m_model.getLevels()[1];
    level->setSpeed(6);  // speed and, keeping the intensity, the deviation
    EXPECT_EQ(counter.count(), 2);
    level->setDirection(1);
    EXPECT_EQ(counter.count(), 3);
    level->setStandardDeviation(2);
    EXPECT_EQ(counter.count(), 4);
    level->setTurbulenceIntensity(0.5);
    EXPECT_EQ(counter.count(), 5);
    level->setSpeedPreservingStandardDeviation(12);
    EXPECT_EQ(counter.count(), 6);
    level->setAltitude(150);
    EXPECT_EQ(counter.count(), 7);
    level->fireChangeEvent();
    EXPECT_EQ(counter.count(), 8);
}

TEST_F(MultiLevelWindModelTest, ModelChangesAreAnnounced)
{
    const ChangeCounter counter(m_model.changed());
    addLevel(150, 5, 0, 1.0);
    EXPECT_EQ(counter.count(), 1);
    m_model.setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_EQ(counter.count(), 2);
    m_model.removeWindLevel(150);
    EXPECT_EQ(counter.count(), 3);
    m_model.removeWindLevel(12345);  // nothing removed, still announced
    EXPECT_EQ(counter.count(), 4);
    EXPECT_FALSE(m_model.addWindLevel(0, 1, 1).has_value());  // refused: nothing announced
    EXPECT_EQ(counter.count(), 4);
    m_model.resetLevels(m_prefs);  // the addition and the reset
    EXPECT_EQ(counter.count(), 6);
    m_model.clearLevels();
    EXPECT_EQ(counter.count(), 7);
}

TEST_F(MultiLevelWindModelTest, SortingSeedingAndLoadingAreNotAnnounced)
{
    addLevel(150, 5, 0, 1.0);
    const ChangeCounter counter(m_model.changed());
    m_model.sortLevels();
    m_model.setSeed(5);
    const MultiLevelPinkNoiseWindModel other(m_prefs);
    m_model.loadFrom(other);
    EXPECT_EQ(counter.count(), 0);

    // The loaded levels report to this model.
    m_model.getLevels()[0]->setSpeed(3);
    EXPECT_EQ(counter.count(), 1);
}

TEST_F(MultiLevelWindModelTest, LevelListenersSeeTheirLevelOnly)
{
    addLevel(100, 5, 0, 1.0);
    int        count      = 0;
    const auto connection = m_model.getLevels()[1]->changed().connect([&count] { ++count; });
    m_model.getLevels()[0]->setSpeed(4);
    EXPECT_EQ(count, 0);
    m_model.getLevels()[1]->setSpeed(4);
    EXPECT_GE(count, 1);
    const int before = count;
    m_model.getLevels()[1]->setAltitude(120);
    EXPECT_EQ(count, before + 1);
    EXPECT_TRUE(m_model.getLevels()[1]->changed().disconnect(connection));
}

// ---- QtRocket additions: slots that change the levels while a level announces a change ----

TEST_F(MultiLevelWindModelTest, ChangeSlotMayRemoveTheLevelWhoseSetterFired)
{
    // Java's garbage collector keeps a removed level alive until its setter returns; here the
    // setter keeps its level alive (run under the asan preset, this was a heap-use-after-free).
    addLevel(100, 5, 0, 1.0);
    int                                        count   = 0;
    bool                                       removed = false;
    const QtRocket::Signal<>::ScopedConnection connection{m_model.changed().connect([&] {
        ++count;
        if (!removed)
        {
            removed = true;
            m_model.removeWindLevel(100);
        }
    })};

    m_model.getLevels()[1]->setSpeed(6);  // emits for the deviation, then for the speed

    EXPECT_TRUE(removed);
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_EQ(m_model.getLevels()[0]->getAltitude(), 0.0);
    // As in OpenRocket: the deviation, the removal, then the speed, which the removed level
    // still reports to the model.
    EXPECT_EQ(count, 3);
}

TEST_F(MultiLevelWindModelTest, ChangeSlotMayRestructureTheLevelsDuringALevelSetter)
{
    const TempCsv                      csv({"altitude,speed,direction", "50,1,0"});
    const MultiLevelPinkNoiseWindModel source(m_prefs);
    struct Case
    {
        std::string_view name;
        LevelsChange     change;
        std::size_t      levelsAfter;
        int              emissions;  // OpenRocket's count
    };
    const std::vector<Case> cases{
        {.name        = "removeWindLevel",
         .change      = [](MultiLevelPinkNoiseWindModel& model) { model.removeWindLevel(100); },
         .levelsAfter = 1,
         .emissions   = 3},
        {.name        = "removeWindLevelIdx",
         .change      = [](MultiLevelPinkNoiseWindModel& model) { model.removeWindLevelIdx(1); },
         .levelsAfter = 1,
         .emissions   = 3},
        {.name        = "clearLevels",
         .change      = [](MultiLevelPinkNoiseWindModel& model) { model.clearLevels(); },
         .levelsAfter = 0,
         .emissions   = 3},
        {.name        = "resetLevels",
         .change      = [this](MultiLevelPinkNoiseWindModel& model) { model.resetLevels(m_prefs); },
         .levelsAfter = 1,
         .emissions   = 4},
        {.name        = "loadFrom",
         .change      = [&source](MultiLevelPinkNoiseWindModel& model) { model.loadFrom(source); },
         .levelsAfter = 1,
         .emissions   = 2},
        {.name = "importLevelsFromCsv",
         .change =
             [&csv](MultiLevelPinkNoiseWindModel& model) {
                 EXPECT_TRUE(model.importLevelsFromCsv(csv.path(), ",").has_value());
             },
         .levelsAfter = 1,
         .emissions   = 4},
    };
    for (const Case& c : cases)
    {
        SCOPED_TRACE(c.name);
        const ReentryOutcome outcome = changeLevelsDuringSetSpeed(c.change);
        EXPECT_TRUE(outcome.slotRan);
        EXPECT_EQ(outcome.levelsAfter, c.levelsAfter);
        EXPECT_EQ(outcome.emissions, c.emissions);
    }
}

TEST_F(MultiLevelWindModelTest, LevelSlotMayRemoveItsLevelDuringEverySetter)
{
    // A slot on the level itself (a GUI row) removes the level while any of its setters runs.
    struct Case
    {
        std::string_view                     name;
        std::function<void(LevelWindModel&)> set;
    };
    const std::vector<Case> cases{
        {.name = "setAltitude", .set = [](LevelWindModel& level) { level.setAltitude(150); }},
        {.name = "setSpeed", .set = [](LevelWindModel& level) { level.setSpeed(6); }},
        {.name = "setSpeedPreservingStandardDeviation",
         .set  = [](LevelWindModel& level) { level.setSpeedPreservingStandardDeviation(12); }},
        {.name = "setDirection", .set = [](LevelWindModel& level) { level.setDirection(1); }},
        {.name = "setStandardDeviation",
         .set  = [](LevelWindModel& level) { level.setStandardDeviation(2); }},
        {.name = "setTurbulenceIntensity",
         .set  = [](LevelWindModel& level) { level.setTurbulenceIntensity(0.5); }},
        {.name = "fireChangeEvent", .set = [](LevelWindModel& level) { level.fireChangeEvent(); }},
    };
    for (const Case& c : cases)
    {
        SCOPED_TRACE(c.name);
        const ReentryOutcome outcome = removeLevelFromItsOwnSlot(c.set);
        EXPECT_TRUE(outcome.slotRan);
        EXPECT_EQ(outcome.levelsAfter, 1U);
        EXPECT_EQ(outcome.lowestAltitude, 0.0);
    }
}

TEST_F(MultiLevelWindModelTest, CopyIsJavasClone)
{
    addLevel(100, 5, kPi / 4, 1.0);
    m_model.setAltitudeReference(WindModel::AltitudeReference::AGL);

    const MultiLevelPinkNoiseWindModel copy{m_model};
    EXPECT_TRUE(copy == m_model);
    EXPECT_EQ(copy.getAltitudeReference(), WindModel::AltitudeReference::AGL);
    EXPECT_EQ(copy.hashCode(), m_model.hashCode());
}

TEST_F(MultiLevelWindModelTest, CopyLevelsReportToTheCopy)
{
    addLevel(100, 5, kPi / 4, 1.0);
    const ChangeCounter counter(m_model.changed());

    // The copy's levels are its own and report to it, not to the original's listeners.
    MultiLevelPinkNoiseWindModel copy{m_model};
    const ChangeCounter          copyCounter(copy.changed());
    copy.getLevels()[1]->setAltitude(110);
    EXPECT_EQ(copyCounter.count(), 1);
    EXPECT_EQ(counter.count(), 0);
    EXPECT_EQ(m_model.getLevels()[1]->getAltitude(), 100.0);
    EXPECT_FALSE(copy == m_model);
}

TEST_F(MultiLevelWindModelTest, LoadFromItselfKeepsTheLevels)
{
    addLevel(100, 5, 0, 1.0);
    m_model.loadFrom(m_model);
    EXPECT_EQ(m_model.getLevels().size(), 2U);
}

// ---- QtRocket additions: velocities, seeds and determinism ----

TEST_F(MultiLevelWindModelTest, NoLevelsGiveNoWind)
{
    m_model.clearLevels();
    EXPECT_TRUE(m_model.getWindVelocity(1.0, 100).exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(m_model.getWindVelocity(1.0, 100, 50).exactlyEquals(Coordinate::kZero));
}

TEST_F(MultiLevelWindModelTest, AltitudeReferenceChoosesTheAltitude)
{
    m_model.clearLevels();
    addLevel(0, 2, kPi / 2, 0.0);
    addLevel(1000, 12, kPi / 2, 0.0);

    // MSL (the default) takes the first altitude, AGL the second.
    EXPECT_NEAR(m_model.getWindVelocity(0, 1000, 0).x, 12, 1e-12);
    m_model.setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_NEAR(m_model.getWindVelocity(0, 1000, 0).x, 2, 1e-12);
    EXPECT_NEAR(m_model.getWindVelocity(0, 0, 500).x, 7, 1e-12);
}

TEST_F(MultiLevelWindModelTest, WindDirectionIsInZeroToTwoPi)
{
    m_model.clearLevels();
    addLevel(0, 5, 3 * kPi / 2, 0.0);  // from the west: atan2 gives -pi/2
    EXPECT_NEAR(m_model.getWindDirection(0, 0), 3 * kPi / 2, 1e-12);
    m_model.getLevels()[0]->setDirection(kPi / 4);
    EXPECT_NEAR(m_model.getWindDirection(0, 100), kPi / 4, 1e-12);
}

TEST_F(MultiLevelWindModelTest, SetSeedGivesEachLevelItsOwnSeed)
{
    m_model.clearLevels();
    addLevel(0, 5, 0, 1.0);
    addLevel(1000, 8, 1, 2.0);
    m_model.setSeed(kSeed);

    // Level i is seeded with seed * 31 + i (Java int arithmetic).
    PinkNoiseWindModel level0(MathUtil::javaHashCombine(kSeed, 0));
    level0.setDirection(0);
    level0.setAverage(5);
    level0.setStandardDeviation(1.0);
    PinkNoiseWindModel level1(MathUtil::javaHashCombine(kSeed, 1));
    level1.setDirection(1);
    level1.setAverage(8);
    level1.setStandardDeviation(2.0);

    for (int i = 0; i < 100; ++i)
    {
        const double time = i * 0.07;
        EXPECT_TRUE(
            m_model.getWindVelocity(time, 0).exactlyEquals(level0.getWindVelocity(time, 0)));
        EXPECT_TRUE(
            m_model.getWindVelocity(time, 1000).exactlyEquals(level1.getWindVelocity(time, 1000)));
    }
}

TEST_F(MultiLevelWindModelTest, SameSeedGivesTheSameWind)
{
    addLevel(300, 5, 1, 1.5);
    addLevel(900, 9, 2, 2.5);
    MultiLevelPinkNoiseWindModel copy{m_model};
    m_model.setSeed(77);
    copy.setSeed(77);
    for (int i = 0; i < 400; ++i)
    {
        const double time     = i * kDeltaT;
        const double altitude = i * 3.0;
        EXPECT_TRUE(m_model.getWindVelocity(time, altitude)
                        .exactlyEquals(copy.getWindVelocity(time, altitude)))
            << i;
    }

    // Another seed gives other gusts.
    copy.setSeed(78);
    bool differs = false;
    for (int i = 0; i < 50 && !differs; ++i)
    {
        differs = !m_model.getWindVelocity(i * kDeltaT, 600)
                       .exactlyEquals(copy.getWindVelocity(i * kDeltaT, 600));
    }
    EXPECT_TRUE(differs);
}

TEST_F(MultiLevelWindModelTest, SeedsWrapAsJavaInts)
{
    // Integer.MAX_VALUE * 31 + i wraps to 2147483617 + i (printed by Java).
    addLevel(100, 5, 0, 1.0);
    m_model.setSeed(std::numeric_limits<int>::max());

    PinkNoiseWindModel level0(2147483617);
    level0.setDirection(0);
    const LevelWindModel expected0(0, PinkNoiseWindModel{level0});
    EXPECT_TRUE(*m_model.getLevels()[0] == expected0);

    PinkNoiseWindModel level1(2147483618);
    level1.setDirection(0);
    level1.setAverage(5);
    level1.setStandardDeviation(1.0);
    const LevelWindModel expected1(100, PinkNoiseWindModel{level1});
    EXPECT_TRUE(*m_model.getLevels()[1] == expected1);
}

TEST_F(MultiLevelWindModelTest, LevelEqualityAndHash)
{
    PinkNoiseWindModel wind(42);
    wind.setAverage(10.0);
    wind.setDirection(kPi / 4);
    wind.setStandardDeviation(2.0);
    const LevelWindModel level(100.0, PinkNoiseWindModel{wind});
    // Objects.hash(100.0, model) printed by OpenRocket on JDK 17.
    EXPECT_EQ(level.hashCode(), -1505047304);

    const std::shared_ptr<LevelWindModel> clone = level.clone();
    EXPECT_TRUE(*clone == level);
    EXPECT_EQ(clone->hashCode(), level.hashCode());
    EXPECT_FALSE(LevelWindModel(-0.0, PinkNoiseWindModel{wind}) ==
                 LevelWindModel(0.0, PinkNoiseWindModel{wind}));

    // The model's hash is Objects.hash(levels): 31 + the list hash.
    int listHash = 1;
    for (const LevelWindModel* each : m_model.getLevels())
    {
        listHash = MathUtil::javaHashCombine(listHash, each->hashCode());
    }
    EXPECT_EQ(m_model.hashCode(), MathUtil::javaHashCombine(1, listHash));
}

TEST_F(MultiLevelWindModelTest, NegativeTimeIsABug)
{
    EXPECT_THROW((void)m_model.getWindVelocity(-0.1, 0), BugError);
}

// ---- QtRocket additions: CSV import ----

TEST_F(MultiLevelWindModelTest, ImportSkipsCommentsAndBlankLinesAndReadsCrLf)
{
    const TempCsv csv({"# wind profile", "", "  altitude,speed,direction,stddev  ", "   ",
                       "0,5,0,1", "#100,1,1,1", "200,6,90,2"},
                      "\r\n");
    importCsv(csv, ",");
    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_EQ(levels[1]->getAltitude(), 200.0);
    EXPECT_NEAR(levels[1]->getDirection(), kPi / 2, 1e-15);
}

TEST_F(MultiLevelWindModelTest, ImportSortsTheLevels)
{
    const TempCsv csv({"altitude,speed,direction", "500,5,0", "100,1,0", "300,3,0"});
    importCsv(csv, ",");
    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 3U);
    EXPECT_EQ(levels[0]->getAltitude(), 100.0);
    EXPECT_EQ(levels[1]->getAltitude(), 300.0);
    EXPECT_EQ(levels[2]->getAltitude(), 500.0);
}

TEST_F(MultiLevelWindModelTest, ImportErrorMessages)
{
    struct Case
    {
        std::vector<std::string> lines;
        std::string              message;
    };
    const std::vector<Case> cases{
        {.lines = {}, .message = "The CSV file is empty."},
        {.lines = {"# only a comment"}, .message = "The CSV file is empty."},
        {.lines   = {"altitude,speed,direction,stddev"},
         .message = "No valid wind data found in the file."},
        {.lines   = {"altitude,direction"},
         .message = "speed column \"speed\" not found in header row."},
        {.lines   = {"altitude,speed,direction", "1,2,3", "4,5"},
         .message = "Line 3 does not have enough columns."},
        {.lines   = {"altitude,speed,direction", "1,,3"},
         .message = "Empty or null value in column speed."},
        {.lines   = {"altitude,speed,direction", "1,2x,3"},
         .message = "The file is not in the correct format. Make sure the data is correctly "
                    "formatted as a number.\nValue: '2x'"},
        {.lines   = {"altitude,speed,direction", "100,2,3", "100.0,4,5"},
         .message = "Wind level already exists for altitude: 100.0"},
    };
    for (const Case& c : cases)
    {
        EXPECT_EQ(importError(c.lines).message, c.message);
    }
}

TEST_F(MultiLevelWindModelTest, ImportErrorCodes)
{
    EXPECT_EQ(importError({"altitude,speed,direction", "100,2,3", "100.0,4,5"}).code,
              ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(importError({"altitude,speed,direction", "1,2,3", "1,2,abc"}).code, ErrorCode::PARSE);
    EXPECT_EQ(importError({"altitude,direction"}).code, ErrorCode::INVALID_ARGUMENT);
}

TEST_F(MultiLevelWindModelTest, ImportOfAMissingFileFails)
{
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "qtrocket_no_such_wind_file.csv";
    const Result<void> result = m_model.importLevelsFromCsv(missing, ",");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::IO);
    EXPECT_EQ(result.error().message, "Could not load the file. 'qtrocket_no_such_wind_file.csv'");
    // The levels were cleared before the file was read, as in Java.
    EXPECT_TRUE(m_model.getLevels().empty());
}

TEST_F(MultiLevelWindModelTest, FailedImportKeepsTheLevelsReadBeforeTheBadRow)
{
    // As in Java: the levels are cleared first and every row adds its level at once, so the
    // rows before the failing one stay (the GUI resets the model after a failed import).
    const TempCsv      csv({"altitude,speed,direction", "300,1,0", "100,2,0", "200,x,0"});
    const Result<void> result = m_model.importLevelsFromCsv(csv.path(), ",");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::PARSE);
    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_EQ(levels[0]->getAltitude(), 100.0);
    EXPECT_EQ(levels[0]->getSpeed(), 2.0);
    EXPECT_EQ(levels[1]->getAltitude(), 300.0);
}

TEST_F(MultiLevelWindModelTest, ImportWithAnEmptySeparatorChangesNothing)
{
    // Refused before the levels are cleared: the rejection exists only in QtRocket.
    addLevel(100, 5, 0, 1.0);
    const ChangeCounter counter(m_model.changed());
    const TempCsv       csv({"altitude,speed,direction", "300,1,0"});

    const Result<void> result = m_model.importLevelsFromCsv(csv.path(), "");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message, "The field separator is empty.");
    EXPECT_FALSE(importByIndex(csv, "", "0", "").has_value());

    EXPECT_EQ(counter.count(), 0);
    ASSERT_EQ(m_model.getLevels().size(), 2U);
    EXPECT_EQ(m_model.getLevels()[1]->getAltitude(), 100.0);
}

TEST_F(MultiLevelWindModelTest, ImportRejectsBadColumnSettings)
{
    const TempCsv csv({"0,5,0,1.5"});

    const Result<void> negative = importByIndex(csv, ",", "-1", "3");
    ASSERT_FALSE(negative.has_value());
    EXPECT_EQ(negative.error().message,
              "Invalid column index. Please enter numeric indices when the file has no headers.");
    EXPECT_FALSE(importByIndex(csv, ",", "0", "x").has_value());
    EXPECT_FALSE(importByIndex(csv, "", "0", "3").has_value());
    // A deviation index past the row fails: Java checks the widest index against every row.
    EXPECT_FALSE(importByIndex(csv, ",", "0", "7").has_value());
}

TEST_F(MultiLevelWindModelTest, ImportWithNegativeDeviationIndexHasNoDeviation)
{
    // A negative deviation index means no deviation column, as in Java.
    const TempCsv csv({"0,5,0,1.5"});
    ASSERT_TRUE(importByIndex(csv, ",", "0", "-1").has_value());
    EXPECT_EQ(m_model.getLevels()[0]->getStandardDeviation(), 0.0);
}

TEST_F(MultiLevelWindModelTest, ImportWithoutUnitsKeepsTheValues)
{
    const TempCsv csv({"altitude;speed;direction;stddev", "10;2,5;1,5;0,25"});
    ASSERT_TRUE(m_model
                    .importLevelsFromCsv(csv.path(), ";", "altitude", "speed", "direction",
                                         "stddev", nullptr, nullptr, nullptr, nullptr, true)
                    .has_value());
    const LevelWindModel* level = m_model.getLevels()[0];
    EXPECT_EQ(level->getAltitude(), 10.0);
    EXPECT_EQ(level->getSpeed(), 2.5);
    EXPECT_EQ(level->getDirection(), 1.5);  // radians, not degrees
    EXPECT_EQ(level->getStandardDeviation(), 0.25);
}

TEST_F(MultiLevelWindModelTest, ImportAcceptsJavaNumberSpellings)
{
    // Double.parseDouble takes exponents, a d suffix and surrounding blanks; the last comma of
    // a value that fails becomes the decimal point.
    const TempCsv csv({"altitude;speed;direction", " 1e2 ;2.5d;0", "2,5e2;1;0"});
    importCsv(csv, ";");
    const std::vector<LevelWindModel*> levels = m_model.getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_EQ(levels[0]->getAltitude(), 100.0);
    EXPECT_EQ(levels[0]->getSpeed(), 2.5);
    EXPECT_EQ(levels[1]->getAltitude(), 250.0);
}

TEST_F(MultiLevelWindModelTest, ImportWithMultiCharacterSeparator)
{
    const TempCsv csv({"altitude::speed::direction", "0::4::90"});
    importCsv(csv, "::");
    ASSERT_EQ(m_model.getLevels().size(), 1U);
    EXPECT_EQ(m_model.getLevels()[0]->getSpeed(), 4.0);
}

}  // namespace
