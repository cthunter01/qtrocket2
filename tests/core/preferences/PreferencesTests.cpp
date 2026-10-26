#include "QtRocket/preferences/Preferences.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Signal.h"
#include "TestTempDir.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

namespace Keys = QtRocket::PreferenceKeys;
using QtRocket::absolutePath;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentClassChain;
using QtRocket::ComponentDefaults;
using QtRocket::DegreeUnit;
using QtRocket::GeneralUnit;
using QtRocket::InMemoryPreferences;
using QtRocket::LineStyle;
using QtRocket::Preferences;
using QtRocket::Unit;
using QtRocket::unitGroup;
using QtRocket::UnitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::TempDir;

constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Component class chains as the rocket group will build them: the class and its ancestors up to
// RocketComponent.
constexpr std::array<std::string_view, 5> kBodyTubeChain{
    "BodyTube", "SymmetricComponent", "BodyComponent", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kMassComponentChain{
    "MassComponent", "MassObject", "InternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kTrapezoidFinSetChain{
    "TrapezoidFinSet", "FinSet", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 2> kStageChain{"AxialStage", "RocketComponent"};

/// The default unit index of every process-wide unit group, in UnitGroupId order.
std::vector<int> defaultUnitIndices()
{
    std::vector<int> indices;
    indices.reserve(QtRocket::kAllUnitGroupIds.size());
    for (const UnitGroupId id : QtRocket::kAllUnitGroupIds)
    {
        indices.push_back(unitGroup(id).getDefaultUnitIndex());
    }
    return indices;
}

/// The unit of the process-wide group @p id named @p name, which must exist.
const Unit& groupUnit(UnitGroupId id, std::string_view name)
{
    const Unit* const unit = unitGroup(id).getUnit(name);
    QTROCKET_ASSERT(unit != nullptr);
    return *unit;
}

/// Counts the emissions of changed().
class ChangeCounter
{
public:
    explicit ChangeCounter(Preferences& prefs)
      : m_connection(prefs.changed().connect([this] { ++m_count; }))
    {
    }
    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// The typed layer is tested through the in-memory store, the only concrete one in the core.
// OpenRocket has no ApplicationPreferences tests; every expectation here comes from reading
// ApplicationPreferences.java.

// ------------------------------------------------------------- store conversions

TEST(Preferences, GetBooleanReadsTrueAndFalseIgnoringCase)
{
    InMemoryPreferences prefs;
    EXPECT_TRUE(prefs.getBoolean("k", true));
    EXPECT_FALSE(prefs.getBoolean("k", false));

    // Each spelling read with the default false, then with the default true.
    using Read = std::pair<bool, bool>;
    std::vector<Read> parsed;
    for (const std::string_view text : {"true", "TRUE", "True", "tRuE", "false", "FALSE", "False",
                                        "", "1", "0", "yes", " true", "true ", "t"})
    {
        prefs.put("k", text);
        parsed.emplace_back(prefs.getBoolean("k", false), prefs.getBoolean("k", true));
    }
    constexpr Read kTrue{true, true};
    constexpr Read kFalse{false, false};
    // Anything else is the default (AbstractPreferences.getBoolean).
    constexpr Read kDefault{false, true};
    EXPECT_EQ(parsed,
              (std::vector<Read>{kTrue, kTrue, kTrue, kTrue, kFalse, kFalse, kFalse, kDefault,
                                 kDefault, kDefault, kDefault, kDefault, kDefault, kDefault}));
}

TEST(Preferences, PutBooleanStoresTrueOrFalse)
{
    InMemoryPreferences prefs;
    prefs.putBoolean("k", true);
    EXPECT_EQ(prefs.get("k"), "true");
    prefs.putBoolean("k", false);
    EXPECT_EQ(prefs.get("k"), "false");
    EXPECT_FALSE(prefs.getBoolean("k", true));
}

TEST(Preferences, IntRoundTrips)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getInt("k", 7), 7);
    prefs.putInt("k", -12);
    EXPECT_EQ(prefs.get("k"), "-12");
    EXPECT_EQ(prefs.getInt("k", 7), -12);
    prefs.putInt("k", std::numeric_limits<int>::min());
    EXPECT_EQ(prefs.getInt("k", 0), std::numeric_limits<int>::min());
    prefs.putInt("k", std::numeric_limits<int>::max());
    EXPECT_EQ(prefs.getInt("k", 0), std::numeric_limits<int>::max());
    prefs.put("k", "+5");
    EXPECT_EQ(prefs.getInt("k", 0), 5);
}

TEST(Preferences, GetIntRejectsWhatIntegerParseIntRejects)
{
    // Integer.parseInt: a sign and digits only, in range; otherwise the default.
    InMemoryPreferences prefs;
    std::vector<int>    parsed;
    for (const std::string_view text : {"", "1.0", "1e3", " 1", "1 ", "abc", "2147483648", "0x10"})
    {
        prefs.put("k", text);
        parsed.push_back(prefs.getInt("k", 7));
    }
    EXPECT_EQ(parsed, std::vector<int>(8, 7));
}

TEST(Preferences, GetDoubleParsesAsJava)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getDouble("k", 1.5), 1.5);

    // Double.parseDouble takes what OpenRocket and Java write.
    prefs.put("k", "0.05");
    EXPECT_EQ(prefs.getDouble("k", 0), 0.05);
    prefs.put("k", "1.0E-5");
    EXPECT_EQ(prefs.getDouble("k", 0), 1.0e-5);
    prefs.put("k", " 3 ");
    EXPECT_EQ(prefs.getDouble("k", 0), 3.0);
    prefs.put("k", "NaN");
    EXPECT_TRUE(std::isnan(prefs.getDouble("k", 0)));
    prefs.put("k", "Infinity");
    EXPECT_EQ(prefs.getDouble("k", 0), kInf);
    prefs.put("k", "-Infinity");
    EXPECT_EQ(prefs.getDouble("k", 0), -kInf);
}

TEST(Preferences, GetDoubleRejectsWhatDoubleParseDoubleRejects)
{
    InMemoryPreferences prefs;
    std::vector<double> parsed;
    // "Inf" is OpenRocket's .ork spelling (Strings::parseDouble takes it); java.util.prefs, which
    // parses with Double.parseDouble, gives the default for it.
    for (const std::string_view text :
         {"", "abc", "1,5", "0x10", "1.5x", "inf", "nan", "Inf", "-Inf", "+Inf", " Inf "})
    {
        prefs.put("k", text);
        parsed.push_back(prefs.getDouble("k", 7.5));
    }
    EXPECT_EQ(parsed, std::vector<double>(11, 7.5));
}

TEST(Preferences, PutDoubleRoundTripsEveryValue)
{
    InMemoryPreferences prefs;
    const std::array    values{0.0,
                               -0.0,
                               1.0,
                               0.05,
                               1200.0,
                               1e-5,
                               1e20,
                               1e-320,
                               28.61,
                               -80.60,
                               kPi,
                               std::numbers::pi / 2,
                               std::numeric_limits<double>::max(),
                               std::numeric_limits<double>::min(),
                               std::numeric_limits<double>::denorm_min(),
                               std::numeric_limits<double>::epsilon(),
                               0.1 + 0.2};
    std::vector<double> back;
    std::vector<bool>   signs;
    std::vector<bool>   expectedSigns;
    for (const double value : values)
    {
        prefs.putDouble("k", value);
        back.push_back(prefs.getDouble("k", kNaN));
        signs.push_back(std::signbit(back.back()));
        expectedSigns.push_back(std::signbit(value));
    }
    EXPECT_EQ(back, std::vector<double>(values.begin(), values.end()));
    EXPECT_EQ(signs, expectedSigns);  // -0.0 reads back as -0.0

    prefs.putDouble("k", kNaN);
    EXPECT_TRUE(std::isnan(prefs.getDouble("k", 0)));
    prefs.putDouble("k", kInf);
    EXPECT_EQ(prefs.getDouble("k", 0), kInf);
    prefs.putDouble("k", -kInf);
    EXPECT_EQ(prefs.getDouble("k", 0), -kInf);
}

TEST(Preferences, PutDoubleSpellsNonFiniteValuesAsJava)
{
    InMemoryPreferences prefs;
    prefs.putDouble("k", kNaN);
    EXPECT_EQ(prefs.get("k"), "NaN");
    prefs.putDouble("k", kInf);
    EXPECT_EQ(prefs.get("k"), "Infinity");
    prefs.putDouble("k", -kInf);
    EXPECT_EQ(prefs.get("k"), "-Infinity");
    // Finite values: the shortest round-trip digits, not Java's "1.0" / "1.0E-5".
    prefs.putDouble("k", 0.05);
    EXPECT_EQ(prefs.get("k"), "0.05");
    prefs.putDouble("k", 1200);
    EXPECT_EQ(prefs.get("k"), "1200");
}

TEST(Preferences, StringForms)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getString("k", "dflt"), "dflt");
    EXPECT_EQ(prefs.get("k"), std::nullopt);
    prefs.putString("k", "value");
    EXPECT_EQ(prefs.getString("k", "dflt"), "value");
    EXPECT_EQ(prefs.get("k"), "value");
    prefs.putString("k", "");
    EXPECT_EQ(prefs.getString("k", "dflt"), "");
    // putString(key, null) removes the key (SwingPreferences).
    prefs.putString("k", std::nullopt);
    EXPECT_EQ(prefs.get("k"), std::nullopt);
    EXPECT_EQ(prefs.getString("k", "dflt"), "dflt");
    // A std::string temporary converts.
    prefs.putString("k", std::string("temp"));
    EXPECT_EQ(prefs.get("k"), "temp");
}

TEST(Preferences, DirectoryFormsUseAChildNode)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getInNode("componentColors", "BodyTube"), std::nullopt);
    EXPECT_EQ(prefs.getString("componentColors", "BodyTube", "-"), "-");
    // Reading never creates the node.
    EXPECT_EQ(prefs.childrenNames(), std::vector<std::string>{});

    prefs.putString("componentColors", "BodyTube", "1,2,3");
    EXPECT_EQ(prefs.childrenNames(), std::vector<std::string>{"componentColors"});
    EXPECT_EQ(prefs.getInNode("componentColors", "BodyTube"), "1,2,3");
    EXPECT_EQ(prefs.getString("componentColors", "BodyTube", "-"), "1,2,3");
    EXPECT_EQ(prefs.getNode("componentColors").get("BodyTube"), "1,2,3");
    // The key is not in the parent.
    EXPECT_EQ(prefs.get("BodyTube"), std::nullopt);

    prefs.putString("componentColors", "BodyTube", std::nullopt);
    EXPECT_EQ(prefs.getInNode("componentColors", "BodyTube"), std::nullopt);
    EXPECT_EQ(prefs.childrenNames(), std::vector<std::string>{"componentColors"});
}

// ------------------------------------------------------------------ generic helpers

TEST(Preferences, GetChoiceInt)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getChoice("k", 2, 1), 1);
    prefs.putChoice("k", 2);
    EXPECT_EQ(prefs.get("k"), "2");
    EXPECT_EQ(prefs.getChoice("k", 2, 1), 2);
    prefs.putChoice("k", 0);
    EXPECT_EQ(prefs.getChoice("k", 2, 1), 0);
    prefs.putChoice("k", 3);  // above max
    EXPECT_EQ(prefs.getChoice("k", 2, 1), 1);
    prefs.putChoice("k", -1);  // negative
    EXPECT_EQ(prefs.getChoice("k", 2, 1), 1);
}

TEST(Preferences, GetChoiceDouble)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getChoice("k", 0.9, 0.3), 0.3);
    prefs.putDouble("k", 0.9);
    EXPECT_EQ(prefs.getChoice("k", 0.9, 0.3), 0.9);
    prefs.putDouble("k", 0.0);
    EXPECT_EQ(prefs.getChoice("k", 0.9, 0.3), 0.0);
    prefs.putDouble("k", 0.91);
    EXPECT_EQ(prefs.getChoice("k", 0.9, 0.3), 0.3);
    prefs.putDouble("k", -0.1);
    EXPECT_EQ(prefs.getChoice("k", 0.9, 0.3), 0.3);
    // A NaN passes both range tests, as in Java.
    prefs.putDouble("k", kNaN);
    EXPECT_TRUE(std::isnan(prefs.getChoice("k", 0.9, 0.3)));
}

TEST(Preferences, EnumNames)
{
    InMemoryPreferences                       prefs;
    constexpr std::array<std::string_view, 2> kNames{"RK4", "RK6"};
    EXPECT_EQ(prefs.getEnumName("k", kNames, "RK4"), "RK4");
    prefs.putEnumName("k", "RK6");
    EXPECT_EQ(prefs.get("k"), "RK6");
    EXPECT_EQ(prefs.getEnumName("k", kNames, "RK4"), "RK6");
    // Enum.valueOf is exact: another case or an unknown name gives the default.
    prefs.put("k", "rk6");
    EXPECT_EQ(prefs.getEnumName("k", kNames, "RK4"), "RK4");
    prefs.put("k", "RK45");
    EXPECT_EQ(prefs.getEnumName("k", kNames, "RK4"), "RK4");
    prefs.put("k", "");
    EXPECT_EQ(prefs.getEnumName("k", kNames, "RK6"), "RK6");
    // putEnum(null) removes.
    prefs.putEnumName("k", std::nullopt);
    EXPECT_EQ(prefs.get("k"), std::nullopt);
}

TEST(Preferences, ParseColor)
{
    EXPECT_EQ(Preferences::parseColor("255,128,64"), Color(255, 128, 64));
    EXPECT_EQ(Preferences::parseColor("0,0,0"), Color(0, 0, 0));
    EXPECT_EQ(Preferences::parseColor("+1,+2,+3"), Color(1, 2, 3));
    EXPECT_EQ(Preferences::parseColor("300,-5,256"), Color(255, 0, 255));  // clamped
    EXPECT_EQ(Preferences::parseColor("1,2,3,"), Color(1, 2, 3));          // Java's split
    EXPECT_EQ(Preferences::parseColor("1,2,3,,"), Color(1, 2, 3));

    // Unlike DocumentPreferences.parseColor this one does not trim.
    EXPECT_EQ(Preferences::parseColor(" 1,2,3"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("1, 2,3"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor(""), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("1,2"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("1,2,3,4"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor(",1,2,3"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("1,,3"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("a,b,c"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("1.0,2,3"), std::nullopt);
    EXPECT_EQ(Preferences::parseColor("Color [r=1, g=2, b=3, a=255]"), std::nullopt);
}

TEST(Preferences, StringifyColorDropsAlpha)
{
    EXPECT_EQ(Preferences::stringifyColor(Color(255, 128, 64)), "255,128,64");
    EXPECT_EQ(Preferences::stringifyColor(Color(1, 2, 3, 0)), "1,2,3");
    EXPECT_EQ(Preferences::parseColor(Preferences::stringifyColor(Color(9, 8, 7, 6))),
              Color(9, 8, 7, 255));
}

TEST(Preferences, GetAndPutColor)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getColor("k", Color(1, 2, 3)), Color(1, 2, 3));
    prefs.putColor("k", Color(200, 0, 0));
    EXPECT_EQ(prefs.get("k"), "200,0,0");
    EXPECT_EQ(prefs.getColor("k", Color(1, 2, 3)), Color(200, 0, 0));
    prefs.put("k", "garbage");
    EXPECT_EQ(prefs.getColor("k", Color(1, 2, 3)), Color(1, 2, 3));
}

// --------------------------------------------------------------------- defaults

TEST(Preferences, DefaultsOfAnEmptyStoreAreOpenRockets)
{
    InMemoryPreferences prefs;

    // Welcome dialog and updates
    EXPECT_FALSE(prefs.getIgnoreWelcome("22.02"));
    EXPECT_TRUE(prefs.getCheckUpdates());
    EXPECT_FALSE(prefs.isUpdateCheckPermissionSet());
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), std::vector<std::string>{""});
    EXPECT_TRUE(prefs.getCheckBetaUpdates());
    EXPECT_TRUE(prefs.getCheckMotorDatabaseUpdates());
    EXPECT_FALSE(prefs.getAutoInstallMotorDatabaseUpdates());
    EXPECT_EQ(prefs.getIgnoreMotorDatabaseUpdateVersions(), std::vector<std::string>{""});

    // 3D
    EXPECT_FALSE(prefs.shouldReduceEffectsDuring3DInteraction());
    EXPECT_TRUE(prefs.isMsaaEnabled());

    // Units, files, flight configurations
    EXPECT_TRUE(prefs.isDisplaySecondaryStability());
    EXPECT_EQ(prefs.getDefaultDirectory(), std::nullopt);
    EXPECT_EQ(prefs.getDefaultFlightConfigName(), "[{motors}]");

    // Simulation flags
    EXPECT_TRUE(prefs.getConfirmSimDeletion());
    EXPECT_FALSE(prefs.getAutoRunSimulations());
    EXPECT_TRUE(prefs.getLaunchIntoWind());
    EXPECT_TRUE(prefs.getShowRasaeroFormatWarning());
    EXPECT_TRUE(prefs.getShowRocksimFormatWarning());
    EXPECT_FALSE(prefs.getExportUserDirectories());
    EXPECT_FALSE(prefs.getExportWindowInformation());

    // Simulation defaults
    EXPECT_EQ(prefs.getDefaultMach(), 0.3);
    EXPECT_EQ(prefs.getLaunchRodLength(), 1.0);
    EXPECT_EQ(prefs.getLaunchRodAngle(), 0.0);
    EXPECT_EQ(prefs.getLaunchRodDirection(), kPi / 2);
    EXPECT_EQ(prefs.getWindAverage(), 2.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.1);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.2);
    EXPECT_EQ(prefs.getWindDirection(), kPi / 2);
    EXPECT_EQ(prefs.getLaunchAltitude(), 0.0);
    EXPECT_EQ(prefs.getLaunchLatitude(), 28.61);
    EXPECT_EQ(prefs.getLaunchLongitude(), -80.60);
    EXPECT_EQ(prefs.getLaunchTemperature(), 288.15);
    EXPECT_EQ(prefs.getLaunchPressure(), 101325.0);
    EXPECT_EQ(prefs.getLaunchRelativeHumidity(), 0.0);
    EXPECT_TRUE(prefs.isIsaAtmosphere());
    EXPECT_EQ(prefs.getGeodeticComputationName(), "SPHERICAL");
    EXPECT_EQ(prefs.getGravityModelName(), "WGS");
    EXPECT_EQ(prefs.getConstantGravityValue(), 9.807);
    EXPECT_EQ(prefs.getSimulationStepperMethodName(), "RK4");
    EXPECT_EQ(prefs.getRandomSeed(), 0);
    EXPECT_FALSE(prefs.isRandomSeedFixed());
    EXPECT_EQ(prefs.getTimeStep(), 0.05);
    EXPECT_EQ(prefs.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(prefs.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(prefs.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(prefs.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(prefs.getRecoveryDrogueMainLowSpeedWarning(), 15.24);

    // GUI behaviour
    EXPECT_FALSE(prefs.isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_FALSE(prefs.isAlwaysOpenLeftmostTab());
    EXPECT_TRUE(prefs.isShowDiscardConfirmation());
    EXPECT_TRUE(prefs.isShowSaveRocketInfo());
    EXPECT_TRUE(prefs.isShowDiscardSimulationConfirmation());
    EXPECT_TRUE(prefs.isShowDiscardPreferencesConfirmation());
    EXPECT_FALSE(prefs.isShowMarkers());
    EXPECT_TRUE(prefs.isMatchForeDiameter());
    EXPECT_TRUE(prefs.isMatchAftDiameter());
    EXPECT_TRUE(prefs.getMotorNameColumn());

    // SVG export
    EXPECT_EQ(prefs.getSvgStrokeColor(), Color::black());
    EXPECT_EQ(prefs.getSvgStrokeWidth(), 0.1);
    EXPECT_FALSE(prefs.isSvgDrawCrosshair());
    EXPECT_EQ(prefs.getSvgCrosshairColor(), Color(128, 128, 128));
    EXPECT_EQ(prefs.getSvgCrosshairSize(), 2.0);
    EXPECT_TRUE(prefs.isSvgShowLabels());
    EXPECT_EQ(prefs.getSvgLabelColor(), Color::black());
    EXPECT_EQ(prefs.getSvgPartSpacing(), 0.01);

    // Texture generation
    EXPECT_EQ(prefs.getTextureGenerationDpi(), 300.0);
    EXPECT_TRUE(prefs.isTextureGenerationDrawOutline());
    EXPECT_EQ(prefs.getTextureGenerationOutlinePx(), 1);
    EXPECT_TRUE(prefs.isTextureGenerationResetTransforms());
    EXPECT_EQ(prefs.getTextureGenerationOutlineColor(), Color(0, 0, 0));

    // Multi-level wind CSV import
    EXPECT_TRUE(prefs.isMultiLevelWindCsvImportHeader());
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeColumn(), "altitude");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeColumnIndex(), 0);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedColumn(), "speed");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedColumnIndex(), 1);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionColumn(), "direction");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionColumnIndex(), 2);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportStddevColumn(), "stddev");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportStddevColumnIndex(), 3);

    // Per component class
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::SOLID);
    EXPECT_EQ(prefs.getDefaultLineStyle(kMassComponentChain), LineStyle::DASHED);
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color(0, 0, 240));
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), std::nullopt);

    // Reading the defaults stored nothing: even the launch rod direction sync of
    // getLaunchRodDirection() found the rod already at the wind's default direction.
    EXPECT_EQ(prefs.keys(), std::vector<std::string>{});
    EXPECT_TRUE(prefs.empty());
}

TEST(Preferences, ConstantsMatchOpenRocket)
{
    EXPECT_EQ(Preferences::kMaxLaunchRodAngle, kPi / 3);
    EXPECT_EQ(Preferences::kStandardTemperature, 288.15);
    EXPECT_EQ(Preferences::kStandardPressure, 101325.0);
    EXPECT_EQ(Preferences::kStandardRelativeHumidity, 0.0);
    EXPECT_EQ(Preferences::kRecommendedTimeStep, 0.05);
    EXPECT_EQ(Preferences::kRecommendedMaxTime, 1200.0);
    EXPECT_EQ(Preferences::kDefaultConfigName, "[{motors}]");
    EXPECT_EQ(Preferences::kGeodeticComputationNames,
              (std::array<std::string_view, 3>{"FLAT", "SPHERICAL", "WGS84"}));
    EXPECT_EQ(Preferences::kGravityModelNames,
              (std::array<std::string_view, 2>{"WGS", "CONSTANT"}));
    EXPECT_EQ(Preferences::kSimulationStepperMethodNames,
              (std::array<std::string_view, 2>{"RK4", "RK6"}));
}

TEST(Preferences, KeysMatchOpenRocket)
{
    // A sample of PreferenceKeys against ApplicationPreferences.java; the values are what a
    // stored preference is keyed by.
    EXPECT_EQ(Keys::kBodyComponentInsertPositionKey, "BodyComponentInsertPosition");
    EXPECT_EQ(Keys::kDefaultMachNumber, "DefaultMachNumber");
    EXPECT_EQ(Keys::kExportFieldNameComment, "ExportFieldDescriptionComment");
    EXPECT_EQ(Keys::kUserLocal, "locale");
    EXPECT_EQ(Keys::kIgnoreWelcome, "IgnoreWelcome");
    EXPECT_EQ(Keys::kMotorDiameterFilter, "MotorDiameterMatch");
    EXPECT_EQ(Keys::kMultiLevelWindCsvImportHasHeader, "MultiLevelWindCSVImportHasHeader");
    EXPECT_EQ(Keys::kShowDiscardConfirmation, "IgnoreDiscardEditingWarning");
    EXPECT_EQ(Keys::kOpenglEnableMsaa, "OpenGLMultisampleAntialiasingIsEnabled");
    EXPECT_EQ(Keys::kOpenglUseFbo, "OpenGLUseFBO");
    EXPECT_EQ(Keys::kLaunchUseIsa, "LaunchUseISA");
    EXPECT_EQ(Keys::kGeodeticComputation, "GeodeticComputationStrategy");
    EXPECT_EQ(Keys::kUiTheme, "UITheme");
    EXPECT_EQ(Keys::kObjSrgb, "sRGB");
    EXPECT_EQ(Keys::kObjLod, "LOD");
    EXPECT_EQ(Keys::kObjOrigZOffs, "OrigZOffs");
    EXPECT_EQ(Keys::kSvgDrawCrosshair, "SVGDrawCrosshair");
    EXPECT_EQ(Keys::kTextureGenerationDpi, "TextureGenerationDPI");
    EXPECT_EQ(Keys::kComponentStyleNode, "componentStyle");
    EXPECT_EQ(Keys::kComponentMaterialsNode, "componentMaterials");
    EXPECT_EQ(Keys::kComponentColorsNode, "componentColors");
    EXPECT_EQ(Keys::kSplitCharacter, "|");
}

// ------------------------------------------------------------- welcome dialog, updates

TEST(Preferences, IgnoreWelcomeIsPerVersion)
{
    InMemoryPreferences prefs;
    prefs.setIgnoreWelcome("22.02", true);
    EXPECT_EQ(prefs.get("IgnoreWelcome_22.02"), "true");
    EXPECT_TRUE(prefs.getIgnoreWelcome("22.02"));
    EXPECT_FALSE(prefs.getIgnoreWelcome("23.09"));
    prefs.setIgnoreWelcome("22.02", false);
    EXPECT_FALSE(prefs.getIgnoreWelcome("22.02"));
}

TEST(Preferences, UpdateCheckPermissionAppliesToBothCheckers)
{
    InMemoryPreferences prefs;
    prefs.setUpdateCheckPermission(false);
    EXPECT_TRUE(prefs.isUpdateCheckPermissionSet());
    EXPECT_FALSE(prefs.getCheckUpdates());
    EXPECT_FALSE(prefs.getCheckMotorDatabaseUpdates());
    EXPECT_EQ(prefs.get(Keys::kUpdateCheckPermission), "false");

    prefs.setUpdateCheckPermission(true);
    EXPECT_TRUE(prefs.getCheckUpdates());
    EXPECT_TRUE(prefs.getCheckMotorDatabaseUpdates());
    EXPECT_TRUE(prefs.isUpdateCheckPermissionSet());

    prefs.setCheckUpdates(false);
    prefs.setCheckBetaUpdates(false);
    prefs.setCheckMotorDatabaseUpdates(false);
    prefs.setAutoInstallMotorDatabaseUpdates(true);
    EXPECT_FALSE(prefs.getCheckUpdates());
    EXPECT_FALSE(prefs.getCheckBetaUpdates());
    EXPECT_FALSE(prefs.getCheckMotorDatabaseUpdates());
    EXPECT_TRUE(prefs.getAutoInstallMotorDatabaseUpdates());
}

TEST(Preferences, IgnoredVersionsAreNewlineJoined)
{
    InMemoryPreferences            prefs;
    const std::vector<std::string> versions{"23.09", "24.12.beta.01"};
    prefs.setIgnoreUpdateVersions(versions);
    EXPECT_EQ(prefs.get(Keys::kIgnoreUpdateVersions), "23.09\n24.12.beta.01");
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), versions);

    prefs.setIgnoreMotorDatabaseUpdateVersions(versions);
    EXPECT_EQ(prefs.get(Keys::kIgnoreMotorDatabaseUpdateVersions), "23.09\n24.12.beta.01");
    EXPECT_EQ(prefs.getIgnoreMotorDatabaseUpdateVersions(), versions);

    // An empty list stores "" and reads back as Java's "".split("\n"): one empty string.
    prefs.setIgnoreUpdateVersions(std::vector<std::string>{});
    EXPECT_EQ(prefs.get(Keys::kIgnoreUpdateVersions), "");
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), std::vector<std::string>{""});
    // A single version has no separator.
    prefs.setIgnoreUpdateVersions(std::vector<std::string>{"1.0"});
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), std::vector<std::string>{"1.0"});
    // Trailing empty lines are dropped by Java's split, a leading one is kept.
    prefs.put(Keys::kIgnoreUpdateVersions, "\n1.0\n\n");
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), (std::vector<std::string>{"", "1.0"}));
    // Only newlines: every field is a trailing empty one, so there are none.
    prefs.put(Keys::kIgnoreUpdateVersions, "\n\n");
    EXPECT_EQ(prefs.getIgnoreUpdateVersions(), std::vector<std::string>{});
    prefs.put(Keys::kIgnoreMotorDatabaseUpdateVersions, "\n");
    EXPECT_EQ(prefs.getIgnoreMotorDatabaseUpdateVersions(), std::vector<std::string>{});
}

/// The abstract store interface implemented over an inner in-memory node, as a GUI store wraps
/// its settings object; the tests below derive from it to observe or override one thing.
class ForwardingPreferences : public Preferences
{
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const override
    {
        return m_store.get(key);
    }
    void put(std::string_view key, std::string_view value) override { m_store.put(key, value); }
    void remove(std::string_view key) override { m_store.remove(key); }
    void clear() override { m_store.clear(); }
    [[nodiscard]] std::vector<std::string> keys() const override { return m_store.keys(); }
    [[nodiscard]] std::vector<std::string> childrenNames() const override
    {
        return m_store.childrenNames();
    }
    [[nodiscard]] Preferences& getNode(std::string_view name) override
    {
        return m_store.getNode(name);
    }
    [[nodiscard]] const Preferences* findNode(std::string_view name) const noexcept override
    {
        return m_store.findNode(name);
    }

protected:
    ForwardingPreferences() = default;

private:
    InMemoryPreferences m_store;
};

/// Records the order in which put() is called.
class RecordingPreferences final : public ForwardingPreferences
{
public:
    void put(std::string_view key, std::string_view value) override
    {
        m_putOrder.emplace_back(key);
        ForwardingPreferences::put(key, value);
    }
    [[nodiscard]] const std::vector<std::string>& putOrder() const noexcept { return m_putOrder; }

private:
    std::vector<std::string> m_putOrder;
};

TEST(Preferences, UpdateCheckPermissionIsStoredAfterTheCheckers)
{
    // The permission is the last key written: an interrupted write leaves it unset, so the
    // question is asked again rather than a checker going online without a recorded answer.
    RecordingPreferences prefs;
    prefs.setUpdateCheckPermission(true);
    EXPECT_EQ(prefs.putOrder(),
              (std::vector<std::string>{std::string(Keys::kCheckUpdates),
                                        std::string(Keys::kCheckMotorDatabaseUpdates),
                                        std::string(Keys::kUpdateCheckPermission)}));
}

// -------------------------------------------------------------------- 3D interaction

TEST(Preferences, InteractionFlagsEmitChangedWhenTheyChange)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setReduceEffectsDuring3DInteraction(false);  // the default: no change
    EXPECT_EQ(counter.count(), 0);
    prefs.setReduceEffectsDuring3DInteraction(true);
    EXPECT_TRUE(prefs.shouldReduceEffectsDuring3DInteraction());
    EXPECT_EQ(counter.count(), 1);
    prefs.setReduceEffectsDuring3DInteraction(true);
    EXPECT_EQ(counter.count(), 1);

    prefs.setMsaaEnabled(true);  // the default
    EXPECT_EQ(counter.count(), 1);
    prefs.setMsaaEnabled(false);
    EXPECT_FALSE(prefs.isMsaaEnabled());
    EXPECT_EQ(counter.count(), 2);
}

// ---------------------------------------------------------------------- units, files

TEST(Preferences, DefaultDirectoryIsStoredAbsolute)
{
    InMemoryPreferences prefs;
    prefs.setDefaultDirectory(std::filesystem::path("designs"));
    const std::filesystem::path stored =
        prefs.getDefaultDirectory().value_or(std::filesystem::path{});
    ASSERT_FALSE(stored.empty());
    EXPECT_TRUE(stored.is_absolute());
    EXPECT_EQ(stored.filename(), "designs");
    EXPECT_EQ(stored, std::filesystem::absolute("designs"));

    const std::filesystem::path absolute = std::filesystem::current_path() / "elsewhere";
    prefs.setDefaultDirectory(absolute);
    EXPECT_EQ(prefs.getDefaultDirectory(), absolute);

    prefs.setDefaultDirectory(std::nullopt);
    EXPECT_EQ(prefs.getDefaultDirectory(), std::nullopt);
    EXPECT_EQ(prefs.get(Keys::kDefaultDirectory), std::nullopt);

    // new File("").getAbsolutePath() is the current directory.
    prefs.setDefaultDirectory(std::filesystem::path{});
    EXPECT_EQ(prefs.getDefaultDirectory(), std::filesystem::current_path());
}

TEST(Preferences, DefaultDirectoryKeepsNonAsciiCharacters)
{
    // Stored as UTF-8 on every platform: a name outside the Windows code page (which
    // path::string() would reduce to '?') reads back as the same path.
    InMemoryPreferences         prefs;
    const std::filesystem::path relative(u8"Entw\u00FCrfe");
    prefs.setDefaultDirectory(relative);
    EXPECT_EQ(prefs.getDefaultDirectory(), std::filesystem::absolute(relative));
    const std::string stored = prefs.get(Keys::kDefaultDirectory).value_or(std::string{});
    EXPECT_TRUE(
        stored.ends_with("Entw\xC3\xBC"
                         "rfe"))
        << stored;

    const std::filesystem::path japanese =
        std::filesystem::current_path() / std::filesystem::path(u8"\u8a2d\u8a08");
    prefs.setDefaultDirectory(japanese);
    EXPECT_EQ(prefs.getDefaultDirectory(), japanese);
}

// ------------------------------------------------ user thrust curves and presets
//
// The stored texts and lists below are what OpenRocket's ApplicationPreferences stores and gives
// back for the same calls: PrefsProbe.java of the M3 run 1 probes of part C3 (section "file
// lists"), run against the compiled core and swing of commit 5f164fd0e (JDK 17, SwingPreferences
// over java.util.prefs) with absolute paths under a directory of its own. Here they are made under
// a test directory, so that they are absolute on every platform; "./x/../y" is left out, since
// absolutePath() resolves the dots on Windows (util/FileIo.h).

/// @p path as UTF-8 with '/' for the platform's separator.
[[nodiscard]] std::string spelled(const std::filesystem::path& path)
{
    std::string text = QtRocket::pathToUtf8(path);
    std::ranges::replace(text, '\\', '/');
    return text;
}

[[nodiscard]] std::vector<std::string> spelled(const std::vector<std::filesystem::path>& paths)
{
    std::vector<std::string> texts;
    texts.reserve(paths.size());
    for (const std::filesystem::path& path : paths)
    {
        texts.push_back(spelled(path));
    }
    return texts;
}

/// The UTF-8 texts of @p paths joined by @p separator.
[[nodiscard]] std::string joined(const std::vector<std::filesystem::path>& paths, char separator)
{
    std::string text;
    for (const std::filesystem::path& path : paths)
    {
        if (!text.empty())
        {
            text += separator;
        }
        text += QtRocket::pathToUtf8(path);
    }
    return text;
}

// The probe's "TC absent" and "CP absent", without the directory Java makes.
TEST(Preferences, UserFilesDefaultToTheGivenDirectoryWhichNoGetterMakes)
{
    const TempDir               directory;
    const std::filesystem::path home         = directory.resolve("QtRocket");
    const std::filesystem::path thrustCurves = home / Preferences::kUserThrustCurvesDirectoryName;
    const std::filesystem::path presets = home / Preferences::kUserComponentPresetsDirectoryName;

    const InMemoryPreferences prefs;
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(thrustCurves)),
              std::vector{spelled(thrustCurves)});
    EXPECT_EQ(prefs.getUserThrustCurveFilesAsString(thrustCurves),
              QtRocket::pathToUtf8(absolutePath(thrustCurves)));
    EXPECT_EQ(spelled(prefs.getUserComponentPresetFiles(presets)), std::vector{spelled(presets)});
    EXPECT_EQ(prefs.getUserComponentPresetFilesAsString(presets),
              QtRocket::pathToUtf8(absolutePath(presets)));
    EXPECT_FALSE(std::filesystem::exists(home));
    EXPECT_TRUE(prefs.empty());

    EXPECT_EQ(Keys::kUserThrustCurvesKey, "UserThrustCurves");
    EXPECT_EQ(Keys::kUserComponentPresetsKey, "UserComponentPresets");
    EXPECT_EQ(Preferences::kUserThrustCurvesDirectoryName, "ThrustCurves");
    EXPECT_EQ(Preferences::kUserComponentPresetsDirectoryName, "Components");
}

// The probe's "TC set three", "TC set empty" and "TC set null".
TEST(Preferences, UserThrustCurveFilesAreStoredAbsoluteAndJoinedByBars)
{
    const TempDir               directory;
    const CurrentDirectoryGuard cwd(directory.resolve("cwd"));
    const std::filesystem::path base     = QtRocket::withoutRedundantSeparators(directory.path());
    const std::filesystem::path current  = std::filesystem::current_path();
    const std::filesystem::path fallback = base / "default";

    InMemoryPreferences                      prefs;
    const ChangeCounter                      counter(prefs);
    const std::vector<std::filesystem::path> files{
        base / "data" / "a.eng", std::filesystem::path("rel/dir"),
        QtRocket::pathFromUtf8(QtRocket::pathToUtf8(base) + "/c//d/")};
    prefs.setUserThrustCurveFiles(files);
    const std::vector<std::filesystem::path> absolute{base / "data" / "a.eng",
                                                      current / "rel" / "dir", base / "c" / "d"};
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), joined(absolute, '|'));
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(fallback)), spelled(absolute));
    EXPECT_EQ(prefs.getUserThrustCurveFilesAsString(fallback), joined(absolute, ';'));

    // An empty list is the empty text: no file at all, not the default directory.
    prefs.setUserThrustCurveFiles({});
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), "");
    EXPECT_EQ(prefs.getUserThrustCurveFiles(fallback), std::vector<std::filesystem::path>{});
    EXPECT_EQ(prefs.getUserThrustCurveFilesAsString(fallback), "");

    // Java's setUserThrustCurveFiles(null).
    prefs.resetUserThrustCurveFiles();
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), std::nullopt);
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(fallback)), std::vector{spelled(fallback)});

    // new File("").getAbsolutePath() is the current directory (the probe's "TC set empty path").
    const std::vector<std::filesystem::path> empty{std::filesystem::path{}};
    prefs.setUserThrustCurveFiles(empty);
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), QtRocket::pathToUtf8(current));

    // putString() fires no change event.
    EXPECT_EQ(counter.count(), 0);
}

// The probe's "TC set bar inside" and "TC set semicolon inside": the separators are not
// escaped, so a '|' in a path splits it when it is read back, and a ';' when the preferences page
// reads its text.
TEST(Preferences, UserFilesAreNotEscaped)
{
    const TempDir               directory;
    const CurrentDirectoryGuard cwd(directory.resolve("cwd"));
    const std::filesystem::path base     = QtRocket::withoutRedundantSeparators(directory.path());
    const std::filesystem::path current  = std::filesystem::current_path();
    const std::filesystem::path fallback = base / "default";

    InMemoryPreferences                      prefs;
    const std::vector<std::filesystem::path> bar{base / "a|b"};
    prefs.setUserThrustCurveFiles(bar);
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), QtRocket::pathToUtf8(base / "a|b"));
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(fallback)),
              (std::vector{spelled(base / "a"), std::string("b")}));
    EXPECT_EQ(prefs.getUserThrustCurveFilesAsString(fallback),
              joined({base / "a", current / "b"}, ';'));

    const std::vector<std::filesystem::path> semicolon{base / "x;y"};
    prefs.setUserThrustCurveFiles(semicolon);
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(fallback)), std::vector{spelled(base / "x;y")});
    const std::string text = prefs.getUserThrustCurveFilesAsString(fallback);
    EXPECT_EQ(text, QtRocket::pathToUtf8(base / "x;y"));
    EXPECT_EQ(spelled(Preferences::parseUserFilesText(text)),
              (std::vector{spelled(base / "x"), std::string("y")}));
}

// Stored as UTF-8 on every platform, as the default directory is.
TEST(Preferences, UserFilesKeepNonAsciiCharacters)
{
    const TempDir               directory;
    const std::filesystem::path base = QtRocket::withoutRedundantSeparators(directory.path());
    // An accented Latin name and a CJK character, as UTF-8.
    const std::filesystem::path file =
        base / QtRocket::pathFromUtf8("\xC3\xA9t\xC3\xA9") / QtRocket::pathFromUtf8("\xE6\x96\x87");

    InMemoryPreferences                      prefs;
    const std::vector<std::filesystem::path> files{file};
    prefs.setUserComponentPresetFiles(files);
    const std::string stored = prefs.get(Keys::kUserComponentPresetsKey).value_or("");
    EXPECT_TRUE(stored.ends_with("\xC3\xA9t\xC3\xA9/\xE6\x96\x87") ||
                stored.ends_with("\xC3\xA9t\xC3\xA9\\\xE6\x96\x87"))
        << stored;
    EXPECT_EQ(prefs.getUserComponentPresetFiles(base), files);
}

/// getUserThrustCurveFiles(@p fallback) after storing @p stored, spelled().
[[nodiscard]] std::vector<std::string> storedThrustCurveFiles(Preferences&                 prefs,
                                                              std::string_view             stored,
                                                              const std::filesystem::path& fallback)
{
    prefs.put(Keys::kUserThrustCurvesKey, stored);
    return spelled(prefs.getUserThrustCurveFiles(fallback));
}

// The probe's raw stored texts: each part trimmed as String.trim() does, the empty ones skipped,
// each normalised as new File() does; the getters make nothing absolute.
TEST(Preferences, StoredUserFilesAreTrimmedAndNormalised)
{
    const TempDir               directory;
    const CurrentDirectoryGuard cwd(directory.resolve("cwd"));
    const std::filesystem::path fallback = directory.resolve("default");

    InMemoryPreferences prefs;
    using Texts = std::vector<std::string>;
    EXPECT_EQ(storedThrustCurveFiles(prefs, " /a | |/b/ |", fallback), (Texts{"/a", "/b"}));
    EXPECT_EQ(storedThrustCurveFiles(prefs, "|||", fallback), Texts{});
    EXPECT_EQ(storedThrustCurveFiles(prefs, "", fallback), Texts{});
    EXPECT_EQ(storedThrustCurveFiles(prefs, " ", fallback), Texts{});
    EXPECT_EQ(storedThrustCurveFiles(prefs, "\t/x\x01|  ", fallback), Texts{"/x"});
    EXPECT_EQ(storedThrustCurveFiles(prefs, "/a;/b", fallback), Texts{"/a;/b"});
    EXPECT_EQ(storedThrustCurveFiles(prefs, "/ /lead|/trail ", fallback),
              (Texts{"/ /lead", "/trail"}));
    EXPECT_EQ(storedThrustCurveFiles(prefs, "rel", fallback), Texts{"rel"});
    EXPECT_EQ(prefs.getUserThrustCurveFilesAsString(fallback),
              QtRocket::pathToUtf8(std::filesystem::current_path() / "rel"));
}

// GeneralPreferencesPanel's DocumentListener: the text split at ';'.
TEST(Preferences, ParseUserFilesTextSplitsAtSemicolons)
{
    using Texts = std::vector<std::string>;
    EXPECT_EQ(spelled(Preferences::parseUserFilesText(" /x ; ;/y/ ")), (Texts{"/x", "/y"}));
    EXPECT_EQ(spelled(Preferences::parseUserFilesText("")), Texts{});
    EXPECT_EQ(spelled(Preferences::parseUserFilesText(";;")), Texts{});
    EXPECT_EQ(spelled(Preferences::parseUserFilesText("a|b; c//d")), (Texts{"a|b", "c/d"}));
}

// The probe's "CP set two", "CP set empty" and "CP set null": the same rules, under a key of
// their own.
TEST(Preferences, UserComponentPresetFilesHaveAKeyOfTheirOwn)
{
    const TempDir               directory;
    const CurrentDirectoryGuard cwd(directory.resolve("cwd"));
    const std::filesystem::path base     = QtRocket::withoutRedundantSeparators(directory.path());
    const std::filesystem::path current  = std::filesystem::current_path();
    const std::filesystem::path fallback = base / "Components";

    InMemoryPreferences                      prefs;
    const std::vector<std::filesystem::path> two{base / "presets" / "one.orc",
                                                 std::filesystem::path("more")};
    prefs.setUserComponentPresetFiles(two);
    const std::vector<std::filesystem::path> absolute{base / "presets" / "one.orc",
                                                      current / "more"};
    EXPECT_EQ(prefs.get(Keys::kUserComponentPresetsKey), joined(absolute, '|'));
    EXPECT_EQ(prefs.getUserComponentPresetFilesAsString(fallback), joined(absolute, ';'));
    EXPECT_EQ(prefs.get(Keys::kUserThrustCurvesKey), std::nullopt);
    EXPECT_EQ(spelled(prefs.getUserThrustCurveFiles(base)), std::vector{spelled(base)});

    prefs.setUserComponentPresetFiles({});
    EXPECT_EQ(prefs.get(Keys::kUserComponentPresetsKey), "");
    EXPECT_EQ(prefs.getUserComponentPresetFiles(fallback), std::vector<std::filesystem::path>{});

    prefs.resetUserComponentPresetFiles();
    EXPECT_EQ(prefs.get(Keys::kUserComponentPresetsKey), std::nullopt);
    EXPECT_EQ(prefs.getUserComponentPresetFilesAsString(fallback), QtRocket::pathToUtf8(fallback));
}

TEST(Preferences, PlainSettersStoreWithoutEmitting)
{
    // These setters call putX directly in Java, with no fireChangeEvent().
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setDisplaySecondaryStability(false);
    prefs.setDefaultFlightConfigName("Config");
    prefs.setConfirmSimDeletion(false);
    prefs.setAutoRunSimulations(true);
    prefs.setLaunchIntoWind(false);
    prefs.setShowRasaeroFormatWarning(false);
    prefs.setShowRocksimFormatWarning(false);
    prefs.setExportUserDirectories(true);
    prefs.setExportWindowInformation(true);
    prefs.setAutoOpenLastDesignOnStartup(true);
    prefs.setAlwaysOpenLeftmostTab(true);
    prefs.setShowDiscardConfirmation(false);
    prefs.setShowSaveRocketInfo(false);
    prefs.setShowDiscardSimulationConfirmation(false);
    prefs.setShowDiscardPreferencesConfirmation(false);
    prefs.setShowMarkers(true);
    prefs.setMatchForeDiameter(false);
    prefs.setMatchAftDiameter(false);
    prefs.setMotorNameColumn(false);
    prefs.setGeodeticComputationName("WGS84");
    prefs.setGravityModelName("CONSTANT");
    prefs.setSimulationStepperMethodName("RK6");
    prefs.putBoolean("raw", true);

    EXPECT_FALSE(prefs.isDisplaySecondaryStability());
    EXPECT_EQ(prefs.getDefaultFlightConfigName(), "Config");
    EXPECT_FALSE(prefs.getConfirmSimDeletion());
    EXPECT_TRUE(prefs.getAutoRunSimulations());
    EXPECT_FALSE(prefs.getLaunchIntoWind());
    EXPECT_FALSE(prefs.getShowRasaeroFormatWarning());
    EXPECT_FALSE(prefs.getShowRocksimFormatWarning());
    EXPECT_TRUE(prefs.getExportUserDirectories());
    EXPECT_TRUE(prefs.getExportWindowInformation());
    EXPECT_TRUE(prefs.isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_TRUE(prefs.isAlwaysOpenLeftmostTab());
    EXPECT_FALSE(prefs.isShowDiscardConfirmation());
    EXPECT_FALSE(prefs.isShowSaveRocketInfo());
    EXPECT_FALSE(prefs.isShowDiscardSimulationConfirmation());
    EXPECT_FALSE(prefs.isShowDiscardPreferencesConfirmation());
    EXPECT_TRUE(prefs.isShowMarkers());
    EXPECT_FALSE(prefs.isMatchForeDiameter());
    EXPECT_FALSE(prefs.isMatchAftDiameter());
    EXPECT_FALSE(prefs.getMotorNameColumn());
    EXPECT_EQ(prefs.getGeodeticComputationName(), "WGS84");
    EXPECT_EQ(prefs.getGravityModelName(), "CONSTANT");
    EXPECT_EQ(prefs.getSimulationStepperMethodName(), "RK6");
    EXPECT_EQ(counter.count(), 0);

    // The stored spellings are the enum names.
    EXPECT_EQ(prefs.get(Keys::kGeodeticComputation), "WGS84");
    EXPECT_EQ(prefs.get(Keys::kGravityModel), "CONSTANT");
    EXPECT_EQ(prefs.get(Keys::kSimulationStepperMethod), "RK6");
}

TEST(Preferences, EnumSettersRejectUnknownNames)
{
    InMemoryPreferences prefs;
    EXPECT_THROW(prefs.setGeodeticComputationName("ROUND"), BugError);
    EXPECT_THROW(prefs.setGravityModelName("wgs"), BugError);
    EXPECT_THROW(prefs.setSimulationStepperMethodName(""), BugError);
    // A stored value that is not a constant name reads as the default.
    prefs.put(Keys::kGeodeticComputation, "ROUND");
    prefs.put(Keys::kGravityModel, "wgs");
    prefs.put(Keys::kSimulationStepperMethod, "RK45");
    EXPECT_EQ(prefs.getGeodeticComputationName(), "SPHERICAL");
    EXPECT_EQ(prefs.getGravityModelName(), "WGS");
    EXPECT_EQ(prefs.getSimulationStepperMethodName(), "RK4");
    for (const std::string_view name : Preferences::kGeodeticComputationNames)
    {
        prefs.setGeodeticComputationName(name);
        EXPECT_EQ(prefs.getGeodeticComputationName(), name);
    }
}

// --------------------------------------------------------- simulation defaults (SI)

TEST(Preferences, DefaultMachIsAChoice)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setDefaultMach(0.3);  // the default: no change
    EXPECT_EQ(counter.count(), 0);
    prefs.setDefaultMach(0.5);
    EXPECT_EQ(prefs.getDefaultMach(), 0.5);
    EXPECT_EQ(counter.count(), 1);
    prefs.setDefaultMach(0.5 + 1e-12);  // MathUtil.equals
    EXPECT_EQ(counter.count(), 1);
    // A stored value outside 0..0.9 reads as the default, and setting then compares to it.
    prefs.putDouble(Keys::kDefaultMachNumber, 2.0);
    EXPECT_EQ(prefs.getDefaultMach(), 0.3);
    prefs.setDefaultMach(0.3);
    EXPECT_EQ(counter.count(), 1);
    EXPECT_EQ(prefs.get(Keys::kDefaultMachNumber), "2");
}

TEST(Preferences, LaunchRodSetters)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setLaunchRodLength(1.0);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setLaunchRodLength(1.5);
    EXPECT_EQ(prefs.getLaunchRodLength(), 1.5);
    EXPECT_EQ(counter.count(), 1);
    prefs.setLaunchRodLength(1.5);
    EXPECT_EQ(counter.count(), 1);

    // The angle is clamped to +-pi/3 (SimulationOptions.MAX_LAUNCH_ROD_ANGLE).
    prefs.setLaunchRodAngle(0.2);
    EXPECT_EQ(prefs.getLaunchRodAngle(), 0.2);
    EXPECT_EQ(counter.count(), 2);
    prefs.setLaunchRodAngle(2.0);
    EXPECT_EQ(prefs.getLaunchRodAngle(), kPi / 3);
    prefs.setLaunchRodAngle(-2.0);
    EXPECT_EQ(prefs.getLaunchRodAngle(), -kPi / 3);
    EXPECT_EQ(counter.count(), 4);
    prefs.setLaunchRodAngle(-5.0);  // clamps to the same value: no change
    EXPECT_EQ(counter.count(), 4);
}

TEST(Preferences, LaunchRodDirectionIsReducedTo2Pi)
{
    InMemoryPreferences prefs;
    prefs.setLaunchIntoWind(false);  // keep the rod independent of the wind
    const ChangeCounter counter(prefs);

    prefs.setLaunchRodDirection(kPi / 2);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setLaunchRodDirection(-kPi / 2);
    EXPECT_DOUBLE_EQ(prefs.getLaunchRodDirection(), 3 * kPi / 2);
    EXPECT_EQ(counter.count(), 1);
    prefs.setLaunchRodDirection((2 * kPi) + 1.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchRodDirection(), 1.0);
    EXPECT_EQ(counter.count(), 2);
    prefs.setLaunchRodDirection(1.0);
    EXPECT_EQ(counter.count(), 2);
}

TEST(Preferences, LaunchIntoWindSyncsTheRodWithTheWind)
{
    InMemoryPreferences prefs;
    EXPECT_TRUE(prefs.getLaunchIntoWind());
    prefs.setLaunchRodDirection(1.0);
    prefs.setWindDirection(2.5);
    const ChangeCounter counter(prefs);

    // The getter stores the wind direction as the rod direction (and emits, as in Java).
    EXPECT_EQ(prefs.getLaunchRodDirection(), 2.5);
    EXPECT_EQ(prefs.getDouble(Keys::kLaunchRodDirection, 0), 2.5);
    EXPECT_EQ(counter.count(), 1);
    EXPECT_EQ(prefs.getLaunchRodDirection(), 2.5);
    EXPECT_EQ(counter.count(), 1);  // already in sync

    // The stored wind direction is returned as it is, even where reduce2Pi would change it: the
    // wind setter reduces, so only a raw put can store such a value.
    prefs.putDouble(Keys::kWindDirection, -1.0);
    EXPECT_EQ(prefs.getLaunchRodDirection(), -1.0);
    EXPECT_DOUBLE_EQ(prefs.getDouble(Keys::kLaunchRodDirection, 0), (2 * kPi) - 1.0);

    prefs.setLaunchIntoWind(false);
    prefs.setLaunchRodDirection(0.5);
    prefs.setWindDirection(1.5);
    EXPECT_EQ(prefs.getLaunchRodDirection(), 0.5);
}

// The wind setters emit changed(); Java's ApplicationPreferences does not on a wind change (a
// documented deviation, see Preferences.h), so the counts below are this port's.

TEST(Preferences, WindAverage)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setWindAverage(2.0);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setWindAverage(5.0);
    EXPECT_EQ(prefs.getWindAverage(), 5.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.1);  // kept
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.5);
    EXPECT_EQ(counter.count(), 1);

    // A negative average is the positive one from the opposite direction.
    prefs.setWindAverage(-3.0);
    EXPECT_EQ(prefs.getWindAverage(), 3.0);
    EXPECT_DOUBLE_EQ(prefs.getWindDirection(), 3 * kPi / 2);
    EXPECT_EQ(counter.count(), 3);  // direction and average
    prefs.setWindAverage(-3.0);     // same magnitude: the direction still turns
    EXPECT_EQ(prefs.getWindAverage(), 3.0);
    EXPECT_DOUBLE_EQ(prefs.getWindDirection(), kPi / 2);
    EXPECT_EQ(counter.count(), 4);
    prefs.setWindAverage(0.0);
    EXPECT_EQ(prefs.getWindAverage(), 0.0);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.0);
}

TEST(Preferences, TurbulenceIntensitySurvivesAZeroAverage)
{
    // Documented deviation: PinkNoiseWindModel keeps the deviation, so in OpenRocket
    // setAverage(0) stores an intensity of 0 and setAverage(2) afterwards reads it back as 0.
    // Here the intensity is what is stored, and a zero average leaves it alone.
    InMemoryPreferences prefs;
    prefs.setWindTurbulenceIntensity(0.3);
    prefs.setWindAverage(0.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.3);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.0);
    prefs.setWindAverage(2.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.3);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.6);

    // An intensity set while there is no wind is stored as given (Java: the deviation it implies
    // is 0, so 0 is what survives).
    prefs.setWindAverage(0.0);
    prefs.setWindTurbulenceIntensity(0.45);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.45);
    EXPECT_EQ(prefs.get(Keys::kWindTurbulence), "0.45");
    prefs.setWindAverage(4.0);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 1.8);
}

TEST(Preferences, WindTurbulenceAndDeviation)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setWindTurbulenceIntensity(0.1);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setWindTurbulenceIntensity(0.25);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.25);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 0.5);
    EXPECT_EQ(counter.count(), 1);
    prefs.setWindTurbulenceIntensity(-1.0);  // never negative
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.0);
    EXPECT_EQ(counter.count(), 2);
    prefs.setWindTurbulenceIntensity(-2.0);  // still zero: no change
    EXPECT_EQ(counter.count(), 2);

    // The deviation sets the intensity it implies (average is 2).
    prefs.setWindStandardDeviation(1.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.5);
    EXPECT_EQ(prefs.getWindStandardDeviation(), 1.0);
    EXPECT_EQ(counter.count(), 3);
    prefs.setWindStandardDeviation(-1.0);  // max(deviation, 0)
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.0);

    // With no wind the intensity is 0 for no deviation and 1 otherwise.
    prefs.setWindAverage(0.0);
    prefs.setWindStandardDeviation(0.0);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 0.0);
    prefs.setWindStandardDeviation(0.7);
    EXPECT_EQ(prefs.getWindTurbulenceIntensity(), 1.0);
}

TEST(Preferences, WindDirectionIsReducedTo2Pi)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setWindDirection(kPi / 2);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setWindDirection(-kPi);
    EXPECT_DOUBLE_EQ(prefs.getWindDirection(), kPi);
    EXPECT_EQ(counter.count(), 1);
    prefs.setWindDirection(3 * kPi);
    EXPECT_DOUBLE_EQ(prefs.getWindDirection(), kPi);
    EXPECT_EQ(counter.count(), 1);
}

TEST(Preferences, LaunchAltitudeFollowsTheIsaWhenInUse)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);

    prefs.setLaunchAltitude(0.0);  // the default
    EXPECT_EQ(counter.count(), 0);

    // ExtendedISAModel().getConditions(1000): a level of the 500 m table, so its exact
    // conditions (troposphere lapse rate over the geopotential altitude).
    prefs.setLaunchAltitude(1000.0);
    EXPECT_EQ(prefs.getLaunchAltitude(), 1000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 281.6510223716947);
    EXPECT_NEAR(prefs.getLaunchPressure(), 89876.28248259629, 1e-5);
    EXPECT_EQ(prefs.getLaunchRelativeHumidity(), 0.0);
    // temperature, pressure (the humidity did not change) and the altitude itself
    EXPECT_EQ(counter.count(), 3);

    // Halfway between two levels: the linear interpolation of 1000 and 1500.
    prefs.setLaunchAltitude(1250.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 280.02666126355723);
    EXPECT_NEAR(prefs.getLaunchPressure(), 87217.97766854524, 1e-5);

    prefs.setLaunchAltitude(3000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 268.65919845164115);
    EXPECT_NEAR(prefs.getLaunchPressure(), 70121.15575785963, 1e-5);

    // Into the tropopause: the temperature no longer falls.
    prefs.setLaunchAltitude(20000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 216.65);
    EXPECT_NEAR(prefs.getLaunchPressure(), 5530.179819886914, 1e-6);

    // Above the table: the top level.
    prefs.setLaunchAltitude(100000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 187.9233248811066);
    EXPECT_NEAR(prefs.getLaunchPressure(), 0.4083884911216268, 1e-12);
    prefs.setLaunchAltitude(86000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 187.9233248811066);

    // Below sea level: the sea level conditions, exactly.
    prefs.setLaunchAltitude(-10.0);
    EXPECT_EQ(prefs.getLaunchTemperature(), 288.15);
    EXPECT_EQ(prefs.getLaunchPressure(), 101325.0);
    prefs.setLaunchAltitude(0.0);
    EXPECT_EQ(prefs.getLaunchTemperature(), 288.15);
    EXPECT_EQ(prefs.getLaunchPressure(), 101325.0);
}

TEST(Preferences, NaNLaunchAltitudeStoresNaNConditions)
{
    // Java: the interpolation of the level table at NaN is NaN throughout, and MathUtil.equals
    // is never true for a NaN, so every value is stored (and stored again on a repeat).
    InMemoryPreferences prefs;
    prefs.setLaunchRelativeHumidity(0.5);
    const ChangeCounter counter(prefs);
    prefs.setLaunchAltitude(kNaN);
    EXPECT_TRUE(std::isnan(prefs.getLaunchAltitude()));
    EXPECT_TRUE(std::isnan(prefs.getLaunchTemperature()));
    EXPECT_TRUE(std::isnan(prefs.getLaunchPressure()));
    EXPECT_TRUE(std::isnan(prefs.getLaunchRelativeHumidity()));
    EXPECT_EQ(prefs.get(Keys::kLaunchTemperature), "NaN");
    EXPECT_EQ(counter.count(), 4);
    prefs.setLaunchAltitude(kNaN);
    EXPECT_EQ(counter.count(), 8);

    // A real altitude again restores the conditions.
    prefs.setLaunchAltitude(0.0);
    EXPECT_EQ(prefs.getLaunchTemperature(), 288.15);
    EXPECT_EQ(prefs.getLaunchPressure(), 101325.0);
    EXPECT_EQ(prefs.getLaunchRelativeHumidity(), 0.0);
}

TEST(Preferences, IsaAtmosphereSwitch)
{
    InMemoryPreferences prefs;
    prefs.setLaunchAltitude(1000.0);
    const ChangeCounter counter(prefs);

    prefs.setIsaAtmosphere(true);  // the default
    EXPECT_EQ(counter.count(), 0);
    prefs.setIsaAtmosphere(false);
    EXPECT_FALSE(prefs.isIsaAtmosphere());
    EXPECT_EQ(counter.count(), 1);

    // Off: the altitude no longer touches the conditions, which can be set freely.
    prefs.setLaunchAltitude(3000.0);
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 281.6510223716947);
    EXPECT_EQ(counter.count(), 2);
    prefs.setLaunchTemperature(300.0);
    prefs.setLaunchPressure(100000.0);
    prefs.setLaunchRelativeHumidity(0.5);
    EXPECT_EQ(prefs.getLaunchTemperature(), 300.0);
    EXPECT_EQ(prefs.getLaunchPressure(), 100000.0);
    EXPECT_EQ(prefs.getLaunchRelativeHumidity(), 0.5);
    EXPECT_EQ(counter.count(), 5);

    // On again: the conditions of the current altitude are stored (humidity back to 0).
    prefs.setIsaAtmosphere(true);
    EXPECT_TRUE(prefs.isIsaAtmosphere());
    EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), 268.65919845164115);
    EXPECT_NEAR(prefs.getLaunchPressure(), 70121.15575785963, 1e-5);
    EXPECT_EQ(prefs.getLaunchRelativeHumidity(), 0.0);
    EXPECT_EQ(counter.count(), 9);  // temperature, pressure, humidity, the flag
}

TEST(Preferences, LaunchConditionSettersCompareWithMathUtilEquals)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setIsaAtmosphere(false);
    EXPECT_EQ(counter.count(), 1);

    prefs.setLaunchTemperature(288.15);  // the default
    prefs.setLaunchPressure(101325);
    prefs.setLaunchRelativeHumidity(0);
    EXPECT_EQ(counter.count(), 1);
    prefs.setLaunchTemperature(288.15 * (1 + 1e-9));
    prefs.setLaunchPressure(101325 * (1 + 1e-9));
    EXPECT_EQ(counter.count(), 1);
    prefs.setLaunchRelativeHumidity(1e-9);  // near zero: compared absolutely to EPSILON / 2
    EXPECT_EQ(counter.count(), 1);
    prefs.setLaunchRelativeHumidity(0.3);
    EXPECT_EQ(counter.count(), 2);
}

TEST(Preferences, LatitudeAndLongitudeAreClamped)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setLaunchLatitude(28.61);
    prefs.setLaunchLongitude(-80.60);
    EXPECT_EQ(counter.count(), 0);

    prefs.setLaunchLatitude(100.0);
    EXPECT_EQ(prefs.getLaunchLatitude(), 90.0);
    prefs.setLaunchLatitude(-100.0);
    EXPECT_EQ(prefs.getLaunchLatitude(), -90.0);
    prefs.setLaunchLatitude(45.5);
    EXPECT_EQ(prefs.getLaunchLatitude(), 45.5);
    EXPECT_EQ(counter.count(), 3);

    prefs.setLaunchLongitude(200.0);
    EXPECT_EQ(prefs.getLaunchLongitude(), 180.0);
    prefs.setLaunchLongitude(-200.0);
    EXPECT_EQ(prefs.getLaunchLongitude(), -180.0);
    prefs.setLaunchLongitude(10.25);
    EXPECT_EQ(prefs.getLaunchLongitude(), 10.25);
    EXPECT_EQ(counter.count(), 6);
}

TEST(Preferences, GravityAndStepper)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setConstantGravityValue(9.807);
    EXPECT_EQ(counter.count(), 0);
    prefs.setConstantGravityValue(1.62);
    EXPECT_EQ(prefs.getConstantGravityValue(), 1.62);
    EXPECT_EQ(counter.count(), 1);
}

TEST(Preferences, RandomSeed)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setRandomSeed(0);
    EXPECT_EQ(counter.count(), 0);
    prefs.setRandomSeed(-123456789);
    EXPECT_EQ(prefs.getRandomSeed(), -123456789);
    EXPECT_EQ(counter.count(), 1);
    prefs.setRandomSeed(-123456789);
    EXPECT_EQ(counter.count(), 1);

    prefs.setRandomSeedFixed(false);
    EXPECT_EQ(counter.count(), 1);
    prefs.setRandomSeedFixed(true);
    EXPECT_TRUE(prefs.isRandomSeedFixed());
    EXPECT_EQ(counter.count(), 2);
    prefs.setRandomSeedFixed(true);
    EXPECT_EQ(counter.count(), 2);
}

TEST(Preferences, TimeStepAndMaxTime)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setTimeStep(0.05);
    prefs.setMaxSimulationTime(1200);
    EXPECT_EQ(counter.count(), 0);

    prefs.setTimeStep(0.01);
    EXPECT_EQ(prefs.getTimeStep(), 0.01);
    prefs.setMaxSimulationTime(600);
    EXPECT_EQ(prefs.getMaxSimulationTime(), 600.0);
    EXPECT_EQ(counter.count(), 2);

    // A stored zero reads as the recommended maximum.
    prefs.setMaxSimulationTime(0);
    EXPECT_EQ(prefs.get(Keys::kSimulationMaxTime), "0");
    EXPECT_EQ(prefs.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(counter.count(), 3);
    prefs.setMaxSimulationTime(0);
    EXPECT_EQ(counter.count(), 3);
}

TEST(Preferences, RecoveryWarningThresholds)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setRecoverySpeedWarning(20.0);
    prefs.setDrogueLowSpeedWarning(3.048);
    prefs.setRecoveryDrogueMainHighSpeedWarning(30.48);
    prefs.setRecoveryDrogueMainLowSpeedWarning(15.24);
    EXPECT_EQ(counter.count(), 0);

    prefs.setRecoverySpeedWarning(25.0);
    prefs.setDrogueLowSpeedWarning(4.0);
    prefs.setRecoveryDrogueMainHighSpeedWarning(35.0);
    prefs.setRecoveryDrogueMainLowSpeedWarning(10.0);
    EXPECT_EQ(prefs.getRecoverySpeedWarning(), 25.0);
    EXPECT_EQ(prefs.getDrogueLowSpeedWarning(), 4.0);
    EXPECT_EQ(prefs.getRecoveryDrogueMainHighSpeedWarning(), 35.0);
    EXPECT_EQ(prefs.getRecoveryDrogueMainLowSpeedWarning(), 10.0);
    EXPECT_EQ(counter.count(), 4);
}

// ------------------------------------------------ per component class defaults

TEST(Preferences, DefaultLineStyleWalksTheClassChain)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::SOLID);
    EXPECT_EQ(prefs.getDefaultLineStyle(kMassComponentChain), LineStyle::DASHED);
    EXPECT_EQ(prefs.getDefaultLineStyle(kStageChain), LineStyle::SOLID);
    EXPECT_EQ(prefs.getDefaultLineStyle(ComponentClassChain{}), LineStyle::SOLID);
    constexpr std::array<std::string_view, 1> kOrphan{"NotAComponent"};
    EXPECT_EQ(prefs.getDefaultLineStyle(kOrphan), LineStyle::SOLID);

    prefs.setDefaultLineStyle("BodyTube", LineStyle::DOTTED);
    EXPECT_EQ(prefs.getInNode("componentStyle", "BodyTube"), "DOTTED");
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::DOTTED);
    EXPECT_EQ(prefs.getDefaultLineStyle(kTrapezoidFinSetChain), LineStyle::SOLID);

    // A stored ancestor entry wins over the built-in default of a nearer class.
    prefs.setDefaultLineStyle("RocketComponent", LineStyle::DASHDOT);
    EXPECT_EQ(prefs.getDefaultLineStyle(kMassComponentChain), LineStyle::DASHDOT);
    EXPECT_EQ(prefs.getDefaultLineStyle(kTrapezoidFinSetChain), LineStyle::DASHDOT);
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::DOTTED);  // nearer entry

    // A stored name that is not a style reads as SOLID.
    prefs.putString("componentStyle", "BodyTube", "WAVY");
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::SOLID);

    // Documented deviation: the .ork spelling and any case are taken too, where Java's
    // LineStyle.valueOf would throw and give SOLID.
    prefs.putString("componentStyle", "BodyTube", "dashed");
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::DASHED);
    prefs.putString("componentStyle", "BodyTube", "DashDot");
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::DASHDOT);
    prefs.putString("componentStyle", "BodyTube", " dotted ");
    EXPECT_EQ(prefs.getDefaultLineStyle(kBodyTubeChain), LineStyle::DOTTED);
}

TEST(Preferences, DefaultLineStylesTable)
{
    const ComponentDefaults table = Preferences::defaultLineStyles();
    ASSERT_EQ(table.size(), 2U);
    EXPECT_EQ(table[0],
              (std::pair<std::string_view, std::string_view>{"RocketComponent", "SOLID"}));
    EXPECT_EQ(table[1], (std::pair<std::string_view, std::string_view>{"MassObject", "DASHED"}));
}

TEST(Preferences, DefaultColorWalksTheClassChain)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color(0, 0, 240));
    EXPECT_EQ(prefs.getDefaultColor(kTrapezoidFinSetChain), Color(0, 0, 200));
    EXPECT_EQ(prefs.getDefaultColor(kMassComponentChain), Color(0, 0, 0));
    EXPECT_EQ(prefs.getDefaultColor(kStageChain), Color::black());
    EXPECT_EQ(prefs.getDefaultColor(ComponentClassChain{}), Color::black());

    prefs.setDefaultColor("BodyTube", Color(10, 20, 30, 40));
    EXPECT_EQ(prefs.getInNode("componentColors", "BodyTube"), "10,20,30");
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color(10, 20, 30));
    prefs.setDefaultColor("RocketComponent", Color(1, 1, 1));
    EXPECT_EQ(prefs.getDefaultColor(kTrapezoidFinSetChain), Color(1, 1, 1));
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color(10, 20, 30));

    prefs.putString("componentColors", "BodyTube", "not a colour");
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color::black());
}

TEST(Preferences, LightThemeColorTable)
{
    const InMemoryPreferences prefs;
    const ComponentDefaults   table = prefs.defaultComponentColors();
    ASSERT_EQ(table.size(), 10U);
    using Entry = std::pair<std::string_view, std::string_view>;
    EXPECT_EQ(table[0], (Entry{"BodyComponent", "0,0,240"}));
    EXPECT_EQ(table[1], (Entry{"TubeFinSet", "0,0,200"}));
    EXPECT_EQ(table[2], (Entry{"FinSet", "0,0,200"}));
    EXPECT_EQ(table[3], (Entry{"LaunchLug", "0,0,180"}));
    EXPECT_EQ(table[4], (Entry{"RailButton", "0,0,180"}));
    EXPECT_EQ(table[5], (Entry{"InternalComponent", "170,0,100"}));
    EXPECT_EQ(table[6], (Entry{"MassObject", "0,0,0"}));
    EXPECT_EQ(table[7], (Entry{"RecoveryDevice", "255,0,0"}));
    EXPECT_EQ(table[8], (Entry{"PodSet", "160,160,215"}));
    EXPECT_EQ(table[9], (Entry{"ParallelStage", "198,163,184"}));
}

/// A store with its own colour table, as a themed GUI store would be.
class ThemedPreferences final : public ForwardingPreferences
{
public:
    [[nodiscard]] ComponentDefaults defaultComponentColors() const noexcept override
    {
        return kDarkColors;
    }

private:
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 1> kDarkColors{
        {{"BodyComponent", "200,200,255"}}};
};

TEST(Preferences, AStoreMayOverrideTheColorTable)
{
    ThemedPreferences prefs;
    EXPECT_EQ(prefs.getDefaultColor(kBodyTubeChain), Color(200, 200, 255));
    EXPECT_EQ(prefs.getDefaultColor(kTrapezoidFinSetChain), Color::black());  // not in the table
    prefs.setDefaultColor("FinSet", Color(5, 6, 7));
    EXPECT_EQ(prefs.getDefaultColor(kTrapezoidFinSetChain), Color(5, 6, 7));
    // The rest of the typed layer works over the delegated store.
    prefs.setLaunchRodLength(2.5);
    EXPECT_EQ(prefs.getLaunchRodLength(), 2.5);
    EXPECT_EQ(prefs.get(Keys::kLaunchRodLength), "2.5");
    EXPECT_EQ(prefs.getInNode("componentColors", "FinSet"), "5,6,7");
}

TEST(Preferences, DefaultComponentMaterialStrings)
{
    InMemoryPreferences prefs;
    const std::string   cardboard = "BULK|Cardboard|680.0|0.0|Paper";
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), std::nullopt);

    prefs.setDefaultComponentMaterialString("BodyComponent", cardboard);
    EXPECT_EQ(prefs.getInNode("componentMaterials", "BodyComponent"), cardboard);
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), cardboard);  // ancestor
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kTrapezoidFinSetChain), std::nullopt);

    const std::string balsa = "BULK|Balsa|170.0|0.0|Wood";
    prefs.setDefaultComponentMaterialString("BodyTube", balsa);
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), balsa);  // nearer

    prefs.setDefaultComponentMaterialString("BodyTube", std::nullopt);
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), cardboard);
    prefs.setDefaultComponentMaterialString("BodyComponent", std::nullopt);
    EXPECT_EQ(prefs.getDefaultComponentMaterialString(kBodyTubeChain), std::nullopt);
}

// -------------------------------------------------- SVG, textures, CSV import

TEST(Preferences, SvgExportRoundTrip)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs);
    prefs.setSvgStrokeColor(Color(1, 2, 3));
    prefs.setSvgStrokeWidth(0.25);
    prefs.setSvgDrawCrosshair(true);
    prefs.setSvgCrosshairColor(Color(4, 5, 6));
    prefs.setSvgCrosshairSize(3.5);
    prefs.setSvgShowLabels(false);
    prefs.setSvgLabelColor(Color(7, 8, 9));
    prefs.setSvgPartSpacing(0.02);

    EXPECT_EQ(prefs.getSvgStrokeColor(), Color(1, 2, 3));
    EXPECT_EQ(prefs.getSvgStrokeWidth(), 0.25);
    EXPECT_TRUE(prefs.isSvgDrawCrosshair());
    EXPECT_EQ(prefs.getSvgCrosshairColor(), Color(4, 5, 6));
    EXPECT_EQ(prefs.getSvgCrosshairSize(), 3.5);
    EXPECT_FALSE(prefs.isSvgShowLabels());
    EXPECT_EQ(prefs.getSvgLabelColor(), Color(7, 8, 9));
    EXPECT_EQ(prefs.getSvgPartSpacing(), 0.02);
    EXPECT_EQ(prefs.get(Keys::kSvgStrokeColor), "1,2,3");
    EXPECT_EQ(counter.count(), 0);
}

TEST(Preferences, TextureGenerationRoundTrip)
{
    InMemoryPreferences prefs;
    prefs.setTextureGenerationDpi(150);
    prefs.setTextureGenerationDrawOutline(false);
    prefs.setTextureGenerationOutlinePx(3);
    prefs.setTextureGenerationResetTransforms(false);
    prefs.setTextureGenerationOutlineColor(Color(9, 8, 7));

    EXPECT_EQ(prefs.getTextureGenerationDpi(), 150.0);
    EXPECT_FALSE(prefs.isTextureGenerationDrawOutline());
    EXPECT_EQ(prefs.getTextureGenerationOutlinePx(), 3);
    EXPECT_FALSE(prefs.isTextureGenerationResetTransforms());
    EXPECT_EQ(prefs.getTextureGenerationOutlineColor(), Color(9, 8, 7));

    // The outline thickness is never negative.
    prefs.setTextureGenerationOutlinePx(-4);
    EXPECT_EQ(prefs.getTextureGenerationOutlinePx(), 0);
    EXPECT_EQ(prefs.get(Keys::kTextureGenerationOutlinePx), "0");
}

TEST(Preferences, MultiLevelWindCsvImportRoundTrip)
{
    InMemoryPreferences prefs;
    prefs.setMultiLevelWindCsvImportHeader(false);
    prefs.setMultiLevelWindCsvImportAltitudeColumn("alt");
    prefs.setMultiLevelWindCsvImportAltitudeColumnIndex(4);
    prefs.setMultiLevelWindCsvImportSpeedColumn("spd");
    prefs.setMultiLevelWindCsvImportSpeedColumnIndex(5);
    prefs.setMultiLevelWindCsvImportDirectionColumn("dir");
    prefs.setMultiLevelWindCsvImportDirectionColumnIndex(6);
    prefs.setMultiLevelWindCsvImportStddevColumn("sd");
    prefs.setMultiLevelWindCsvImportStddevColumnIndex(7);

    EXPECT_FALSE(prefs.isMultiLevelWindCsvImportHeader());
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeColumn(), "alt");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeColumnIndex(), 4);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedColumn(), "spd");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedColumnIndex(), 5);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionColumn(), "dir");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionColumnIndex(), 6);
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportStddevColumn(), "sd");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportStddevColumnIndex(), 7);
    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportStddevColumnIndex), "7");
}

TEST(Preferences, MultiLevelWindCsvImportUnitsDefaultToSiUnitsAndDegrees)
{
    const DefaultUnitsGuard   defaults;
    const InMemoryPreferences prefs;
    // The fallbacks are fixed units, whatever default unit the user chose for the group.
    UnitGroup::setDefaultImperialUnits();
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportAltitudeUnit(),
              &unitGroup(UnitGroupId::DISTANCE).getSIUnit());
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeUnit().getUnit(), "m");
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportSpeedUnit(),
              &unitGroup(UnitGroupId::WINDSPEED).getSIUnit());
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedUnit().getUnit(), "m/s");
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportStddevUnit(),
              &unitGroup(UnitGroupId::WINDSPEED).getSIUnit());
    // Java: new DegreeUnit(), which equals the ANGLE group's first unit.
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionUnit(), DegreeUnit{});
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportDirectionUnit(),
              &unitGroup(UnitGroupId::ANGLE).getUnit(0));
}

TEST(Preferences, MultiLevelWindCsvImportUnitsAreStoredByName)
{
    InMemoryPreferences prefs;
    prefs.setMultiLevelWindCsvImportAltitudeUnit(groupUnit(UnitGroupId::DISTANCE, "ft"));
    prefs.setMultiLevelWindCsvImportSpeedUnit(groupUnit(UnitGroupId::WINDSPEED, "kt"));
    prefs.setMultiLevelWindCsvImportDirectionUnit(groupUnit(UnitGroupId::ANGLE, "rad"));
    prefs.setMultiLevelWindCsvImportStddevUnit(groupUnit(UnitGroupId::WINDSPEED, "mph"));

    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportAltitudeUnit), "ft");
    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportSpeedUnit), "kt");
    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportDirectionUnit), "rad");
    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportStddevUnit), "mph");
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportAltitudeUnit(),
              &groupUnit(UnitGroupId::DISTANCE, "ft"));
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportSpeedUnit(),
              &groupUnit(UnitGroupId::WINDSPEED, "kt"));
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportDirectionUnit(),
              &groupUnit(UnitGroupId::ANGLE, "rad"));
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportStddevUnit(),
              &groupUnit(UnitGroupId::WINDSPEED, "mph"));

    // Any unit is stored by its name (Unit.toString()); the getter finds it in its own group.
    prefs.setMultiLevelWindCsvImportAltitudeUnit(GeneralUnit(1609.344, "mi"));
    EXPECT_EQ(prefs.get(Keys::kMultiLevelWindCsvImportAltitudeUnit), "mi");
    EXPECT_EQ(&prefs.getMultiLevelWindCsvImportAltitudeUnit(),
              &groupUnit(UnitGroupId::DISTANCE, "mi"));
}

TEST(Preferences, MultiLevelWindCsvImportUnitsFallBackOnUnknownNames)
{
    // Deviation: Java's UnitGroup.getUnit throws IllegalArgumentException for these.
    InMemoryPreferences prefs;
    prefs.put(Keys::kMultiLevelWindCsvImportAltitudeUnit, "km/h");  // another group's unit
    prefs.put(Keys::kMultiLevelWindCsvImportSpeedUnit, "KT");       // names match exactly
    prefs.put(Keys::kMultiLevelWindCsvImportDirectionUnit, "deg");
    prefs.put(Keys::kMultiLevelWindCsvImportStddevUnit, "");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportAltitudeUnit().getUnit(), "m");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportSpeedUnit().getUnit(), "m/s");
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportDirectionUnit(), DegreeUnit{});
    EXPECT_EQ(prefs.getMultiLevelWindCsvImportStddevUnit().getUnit(), "m/s");
}

// ------------------------------------------------------------------ default units

TEST(Preferences, StoreDefaultUnitsWritesEveryGroupWithAChoice)
{
    const DefaultUnitsGuard defaults;
    InMemoryPreferences     prefs;
    UnitGroup::setDefaultMetricUnits();
    prefs.storeDefaultUnits();
    const Preferences* const units = prefs.findNode(Keys::kUnitsNode);
    ASSERT_NE(units, nullptr);
    EXPECT_EQ(units->get("LENGTH"), "cm");
    EXPECT_EQ(units->get("MOTOR_DIMENSIONS"), "mm");
    EXPECT_EQ(units->get("VELOCITY"), "m/s");
    EXPECT_EQ(units->get("ANGLE"), "°");
    EXPECT_EQ(units->get("FLIGHT_TIME"), "s");  // UNITS_LONG_TIME's key
    EXPECT_EQ(units->get("TEMPERATURE"), "°C");
    EXPECT_EQ(units->get("STABILITY"), "cal");
    // The 41 groups of UnitGroup.UNITS less the five with a single unit (NONE, SHORT_TIME,
    // COEFFICIENT, MOMENTUM, SCALING); SHAPE_PARAMETER and STABILITY_CALIBERS are not in the map.
    EXPECT_EQ(units->keys().size(), 36U);
    EXPECT_EQ(units->get("NONE"), std::nullopt);
    EXPECT_EQ(units->get("SHORT_TIME"), std::nullopt);
    EXPECT_EQ(units->get("SHAPE_PARAMETER"), std::nullopt);
    EXPECT_EQ(units->get("STABILITY_CALIBERS"), std::nullopt);
    EXPECT_EQ(units->get("LONG_TIME"), std::nullopt);

    // Storing again overwrites.
    UnitGroup::setDefaultImperialUnits();
    prefs.storeDefaultUnits();
    EXPECT_EQ(units->get("LENGTH"), "in");
    EXPECT_EQ(units->get("WINDSPEED"), "mph");
    EXPECT_EQ(units->get("TEMPERATURE"), "°F");
    EXPECT_EQ(units->keys().size(), 36U);
}

TEST(Preferences, LoadDefaultUnitsAppliesTheStoredNames)
{
    const DefaultUnitsGuard defaults;
    InMemoryPreferences     prefs;
    prefs.loadDefaultUnits();  // nothing stored: nothing changes, and no node is created
    EXPECT_EQ(prefs.findNode(Keys::kUnitsNode), nullptr);
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");

    Preferences& units = prefs.getNode(Keys::kUnitsNode);
    units.put("LENGTH", "in");
    units.put("FLIGHT_TIME", "min");
    units.put("ANGLE", "rad");
    units.put("VELOCITY", "furlong/fortnight");  // not a unit of the group: skipped
    units.put("MASS", "OZ");                     // names match exactly: skipped
    units.put("LONG_TIME", "min");               // not a key of UnitGroup.UNITS: skipped
    units.put("SPEED", "m/s");                   // no such group: skipped
    prefs.loadDefaultUnits();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_EQ(unitGroup(UnitGroupId::LONG_TIME).getDefaultUnit().getUnit(), "min");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).getDefaultUnit().getUnit(), "rad");
    EXPECT_EQ(unitGroup(UnitGroupId::VELOCITY).getDefaultUnit().getUnit(), "m/s");
    EXPECT_EQ(unitGroup(UnitGroupId::MASS).getDefaultUnit().getUnit(), "g");
}

TEST(Preferences, DefaultUnitsRoundTripThroughTheStore)
{
    const DefaultUnitsGuard defaults;
    InMemoryPreferences     prefs;
    UnitGroup::setDefaultImperialUnits();
    const std::vector<int> imperial = defaultUnitIndices();
    prefs.storeDefaultUnits();

    UnitGroup::resetDefaultUnits();
    EXPECT_NE(defaultUnitIndices(), imperial);
    prefs.loadDefaultUnits();
    EXPECT_EQ(defaultUnitIndices(), imperial);
}

// ----------------------------------------------------------------- change signal

TEST(Preferences, ChangedSlotsMayDisconnectThemselves)
{
    InMemoryPreferences prefs;
    int                 calls = 0;
    prefs.changed().connect([&calls] { ++calls; });
    {
        const ChangeCounter scoped(prefs);
        prefs.setLaunchRodLength(2.0);
        EXPECT_EQ(scoped.count(), 1);
        EXPECT_EQ(prefs.changed().size(), 2U);
    }
    EXPECT_EQ(prefs.changed().size(), 1U);
    prefs.setLaunchRodLength(3.0);
    EXPECT_EQ(calls, 2);
}

}  // namespace
