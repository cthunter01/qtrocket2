#include "settings/PreferencesTranscript.h"

#include <array>
#include <filesystem>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui::Test
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ------------------------------------------------------------------------- the values as text

[[nodiscard]] std::string text(bool value)
{
    return value ? "true" : "false";
}

[[nodiscard]] std::string text(int value)
{
    return std::format("{}", value);
}

[[nodiscard]] std::string text(double value)
{
    return std::format("{}", value);
}

[[nodiscard]] std::string text(const std::string& value)
{
    return std::format("\"{}\"", value);
}

[[nodiscard]] std::string text(const std::optional<std::string>& value)
{
    return value.has_value() ? text(*value) : std::string("<absent>");
}

[[nodiscard]] std::string text(const Color& color)
{
    return std::format("rgba({},{},{},{})", color.red(), color.green(), color.blue(),
                       color.alpha());
}

[[nodiscard]] std::string text(const std::vector<std::string>& values)
{
    std::string joined = "[";
    for (const std::string& value : values)
    {
        joined += text(value);
        joined += ' ';
    }
    return joined + "]";
}

[[nodiscard]] std::string text(const std::optional<std::filesystem::path>& path)
{
    return path.has_value() ? text(pathToUtf8(*path)) : std::string("<absent>");
}

[[nodiscard]] std::string text(LineStyle style)
{
    return std::string(lineStyleName(style));
}

[[nodiscard]] std::string text(const Unit& unit)
{
    return std::format("unit {}", unit.getUnit());
}

/// The lines of a transcript.
class Lines
{
public:
    template <class Value>
    void add(std::string_view name, const Value& value)
    {
        m_lines.push_back(std::format("{} = {}", name, text(value)));
    }

    [[nodiscard]] std::vector<std::string> take() { return std::move(m_lines); }

private:
    std::vector<std::string> m_lines;
};

/// The lines of the setters: each call with the number of changed() emissions after it.
class Calls
{
public:
    explicit Calls(Preferences& preferences)
      : m_connection(preferences.changed().connect([this] { ++m_changes; }))
    {
    }

    void add(std::string_view call)
    {
        m_lines.push_back(std::format("{} -> {} changes", call, m_changes));
    }

    [[nodiscard]] std::vector<std::string> take() { return std::move(m_lines); }

private:
    int                        m_changes{0};
    std::vector<std::string>   m_lines;
    Signal<>::ScopedConnection m_connection;
};

// --------------------------------------------------------------------------------- readers

void readStoreForms(const Preferences& p, Lines& lines)
{
    lines.add("getBoolean(flag)", p.getBoolean("flag", true));
    lines.add("getBoolean(text)", p.getBoolean("text", true));
    lines.add("getInt(count)", p.getInt("count", -1));
    lines.add("getInt(text)", p.getInt("text", -1));
    lines.add("getDouble(ratio)", p.getDouble("ratio", -1.5));
    lines.add("getDouble(nan)", p.getDouble("nan", -1.5));
    lines.add("getDouble(inf)", p.getDouble("inf", -1.5));
    lines.add("getString(text)", p.getString("text", "default"));
    lines.add("getString(empty)", p.getString("empty", "default"));
    lines.add("getInNode(dir, key)", p.getInNode("dir", "key"));
    lines.add("getInNode(dir/sub, key)", p.getInNode("dir/sub", "key"));
    lines.add("getString(dir, key)", p.getString("dir", "key", "default"));
    lines.add("getString(dir, missing)", p.getString("dir", "missing", "default"));
    lines.add("getChoice(choice, 3)", p.getChoice("choice", 3, 1));
    lines.add("getChoice(choice, 0.5)", p.getChoice("choice", 0.5, 0.25));
    lines.add("getEnumName(style)",
              p.getEnumName("style", std::array<std::string_view, 2>{"ONE", "TWO"}, "ONE"));
    lines.add("getColor(color)", p.getColor("color", Color(1, 2, 3)));
    lines.add("getIgnoreWelcome(1.0)", p.getIgnoreWelcome("1.0"));
    lines.add("getIgnoreWelcome(2.0)", p.getIgnoreWelcome("2.0"));
}

void readUpdatesAndUnits(const Preferences& p, Lines& lines)
{
    lines.add("getCheckUpdates", p.getCheckUpdates());
    lines.add("isUpdateCheckPermissionSet", p.isUpdateCheckPermissionSet());
    lines.add("getIgnoreUpdateVersions", p.getIgnoreUpdateVersions());
    lines.add("getCheckBetaUpdates", p.getCheckBetaUpdates());
    lines.add("getCheckMotorDatabaseUpdates", p.getCheckMotorDatabaseUpdates());
    lines.add("getAutoInstallMotorDatabaseUpdates", p.getAutoInstallMotorDatabaseUpdates());
    lines.add("getIgnoreMotorDatabaseUpdateVersions", p.getIgnoreMotorDatabaseUpdateVersions());
    lines.add("shouldReduceEffectsDuring3DInteraction", p.shouldReduceEffectsDuring3DInteraction());
    lines.add("isMsaaEnabled", p.isMsaaEnabled());
    lines.add("isDisplaySecondaryStability", p.isDisplaySecondaryStability());
    p.loadDefaultUnits();
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        lines.add(std::format("default unit of {}", unitGroupName(id)),
                  unitGroup(id).getDefaultUnit());
    }
    lines.add("getDefaultDirectory", p.getDefaultDirectory());
    lines.add("getDefaultFlightConfigName", p.getDefaultFlightConfigName());
}

void readSimulationFlags(const Preferences& p, Lines& lines)
{
    lines.add("getConfirmSimDeletion", p.getConfirmSimDeletion());
    lines.add("getAutoRunSimulations", p.getAutoRunSimulations());
    lines.add("getLaunchIntoWind", p.getLaunchIntoWind());
    lines.add("getShowRasaeroFormatWarning", p.getShowRasaeroFormatWarning());
    lines.add("getShowRocksimFormatWarning", p.getShowRocksimFormatWarning());
    lines.add("getExportUserDirectories", p.getExportUserDirectories());
    lines.add("getExportWindowInformation", p.getExportWindowInformation());
}

void readSimulationDefaults(const Preferences& p, Lines& lines)
{
    lines.add("getDefaultMach", p.getDefaultMach());
    lines.add("getLaunchRodLength", p.getLaunchRodLength());
    lines.add("getLaunchRodAngle", p.getLaunchRodAngle());
    lines.add("getWindAverage", p.getWindAverage());
    lines.add("getWindTurbulenceIntensity", p.getWindTurbulenceIntensity());
    lines.add("getWindStandardDeviation", p.getWindStandardDeviation());
    lines.add("getWindDirection", p.getWindDirection());
    lines.add("getLaunchAltitude", p.getLaunchAltitude());
    lines.add("getLaunchLatitude", p.getLaunchLatitude());
    lines.add("getLaunchLongitude", p.getLaunchLongitude());
    lines.add("getLaunchTemperature", p.getLaunchTemperature());
    lines.add("getLaunchPressure", p.getLaunchPressure());
    lines.add("getLaunchRelativeHumidity", p.getLaunchRelativeHumidity());
    lines.add("isIsaAtmosphere", p.isIsaAtmosphere());
    lines.add("getGeodeticComputationName", p.getGeodeticComputationName());
    lines.add("getGravityModelName", p.getGravityModelName());
    lines.add("getConstantGravityValue", p.getConstantGravityValue());
    lines.add("getSimulationStepperMethodName", p.getSimulationStepperMethodName());
    lines.add("getRandomSeed", p.getRandomSeed());
    lines.add("isRandomSeedFixed", p.isRandomSeedFixed());
    lines.add("getTimeStep", p.getTimeStep());
    lines.add("getMaxSimulationTime", p.getMaxSimulationTime());
    lines.add("getRecoverySpeedWarning", p.getRecoverySpeedWarning());
    lines.add("getDrogueLowSpeedWarning", p.getDrogueLowSpeedWarning());
    lines.add("getRecoveryDrogueMainHighSpeedWarning", p.getRecoveryDrogueMainHighSpeedWarning());
    lines.add("getRecoveryDrogueMainLowSpeedWarning", p.getRecoveryDrogueMainLowSpeedWarning());
}

void readGuiBehaviour(const Preferences& p, Lines& lines)
{
    lines.add("isAutoOpenLastDesignOnStartupEnabled", p.isAutoOpenLastDesignOnStartupEnabled());
    lines.add("isAlwaysOpenLeftmostTab", p.isAlwaysOpenLeftmostTab());
    lines.add("isShowDiscardConfirmation", p.isShowDiscardConfirmation());
    lines.add("isShowSaveRocketInfo", p.isShowSaveRocketInfo());
    lines.add("isShowDiscardSimulationConfirmation", p.isShowDiscardSimulationConfirmation());
    lines.add("isShowDiscardPreferencesConfirmation", p.isShowDiscardPreferencesConfirmation());
    lines.add("isShowMarkers", p.isShowMarkers());
    lines.add("isMatchForeDiameter", p.isMatchForeDiameter());
    lines.add("isMatchAftDiameter", p.isMatchAftDiameter());
    lines.add("getMotorNameColumn", p.getMotorNameColumn());
}

void readComponentDefaults(const Preferences& p, Lines& lines)
{
    for (const ComponentKind kind :
         {ComponentKind::BODY_TUBE, ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::MASS_COMPONENT,
          ComponentKind::PARACHUTE, ComponentKind::POD_SET})
    {
        const auto chain = componentClassChain(kind);
        lines.add(std::format("getDefaultLineStyle({})", className(kind)),
                  p.getDefaultLineStyle(chain));
        lines.add(std::format("getDefaultComponentMaterialString({})", className(kind)),
                  p.getDefaultComponentMaterialString(chain));
        lines.add(std::format("getDefaultColor({})", className(kind)), p.getDefaultColor(chain));
    }
}

void readExports(const Preferences& p, Lines& lines)
{
    lines.add("getSvgStrokeColor", p.getSvgStrokeColor());
    lines.add("getSvgStrokeWidth", p.getSvgStrokeWidth());
    lines.add("isSvgDrawCrosshair", p.isSvgDrawCrosshair());
    lines.add("getSvgCrosshairColor", p.getSvgCrosshairColor());
    lines.add("getSvgCrosshairSize", p.getSvgCrosshairSize());
    lines.add("isSvgShowLabels", p.isSvgShowLabels());
    lines.add("getSvgLabelColor", p.getSvgLabelColor());
    lines.add("getSvgPartSpacing", p.getSvgPartSpacing());
    lines.add("getTextureGenerationDpi", p.getTextureGenerationDpi());
    lines.add("isTextureGenerationDrawOutline", p.isTextureGenerationDrawOutline());
    lines.add("getTextureGenerationOutlinePx", p.getTextureGenerationOutlinePx());
    lines.add("isTextureGenerationResetTransforms", p.isTextureGenerationResetTransforms());
    lines.add("getTextureGenerationOutlineColor", p.getTextureGenerationOutlineColor());
}

void readWindCsvImport(const Preferences& p, Lines& lines)
{
    lines.add("isMultiLevelWindCsvImportHeader", p.isMultiLevelWindCsvImportHeader());
    lines.add("getMultiLevelWindCsvImportAltitudeColumn",
              p.getMultiLevelWindCsvImportAltitudeColumn());
    lines.add("getMultiLevelWindCsvImportAltitudeColumnIndex",
              p.getMultiLevelWindCsvImportAltitudeColumnIndex());
    lines.add("getMultiLevelWindCsvImportAltitudeUnit", p.getMultiLevelWindCsvImportAltitudeUnit());
    lines.add("getMultiLevelWindCsvImportSpeedColumn", p.getMultiLevelWindCsvImportSpeedColumn());
    lines.add("getMultiLevelWindCsvImportSpeedColumnIndex",
              p.getMultiLevelWindCsvImportSpeedColumnIndex());
    lines.add("getMultiLevelWindCsvImportSpeedUnit", p.getMultiLevelWindCsvImportSpeedUnit());
    lines.add("getMultiLevelWindCsvImportDirectionColumn",
              p.getMultiLevelWindCsvImportDirectionColumn());
    lines.add("getMultiLevelWindCsvImportDirectionColumnIndex",
              p.getMultiLevelWindCsvImportDirectionColumnIndex());
    lines.add("getMultiLevelWindCsvImportDirectionUnit",
              p.getMultiLevelWindCsvImportDirectionUnit());
    lines.add("getMultiLevelWindCsvImportStddevColumn", p.getMultiLevelWindCsvImportStddevColumn());
    lines.add("getMultiLevelWindCsvImportStddevColumnIndex",
              p.getMultiLevelWindCsvImportStddevColumnIndex());
    lines.add("getMultiLevelWindCsvImportStddevUnit", p.getMultiLevelWindCsvImportStddevUnit());
}

// --------------------------------------------------------------------------------- writers

/// The unit @p name of the process-wide group @p id.
[[nodiscard]] const Unit& unitNamed(UnitGroupId id, std::string_view name)
{
    const Unit* const unit = unitGroup(id).getUnit(name);
    QTROCKET_ASSERT(unit != nullptr);
    return *unit;
}

void writeStoreForms(Preferences& p, Calls& calls)
{
    p.putBoolean("flag", false);
    calls.add("putBoolean(flag, false)");
    p.putString("text", "not a number, \"quoted\"\n\xC2\xB0");
    calls.add("putString(text)");
    p.putString("empty", "");
    calls.add("putString(empty)");
    p.putInt("count", -42);
    calls.add("putInt(count, -42)");
    p.putDouble("ratio", 0.1 + 0.2);
    calls.add("putDouble(ratio, 0.1 + 0.2)");
    p.putDouble("nan", kNaN);
    calls.add("putDouble(nan, NaN)");
    p.putDouble("inf", -kInf);
    calls.add("putDouble(inf, -Infinity)");
    p.putString("dir", "key", "in a node");
    calls.add("putString(dir, key)");
    p.putString("dir/sub", "key", "deeper");
    calls.add("putString(dir/sub, key)");
    p.putString("dir", "gone", "removed again");
    p.putString("dir", "gone", std::nullopt);
    calls.add("putString(dir, gone) twice");
    p.putChoice("choice", 2);
    calls.add("putChoice(choice, 2)");
    p.putEnumName("style", "TWO");
    calls.add("putEnumName(style, TWO)");
    p.putColor("color", Color(10, 20, 30, 40));
    calls.add("putColor(color)");
    p.setIgnoreWelcome("1.0", true);
    calls.add("setIgnoreWelcome(1.0, true)");
}

void writeUpdatesAndUnits(Preferences& p, Calls& calls)
{
    p.setCheckUpdates(false);
    calls.add("setCheckUpdates(false)");
    p.setUpdateCheckPermission(true);
    calls.add("setUpdateCheckPermission(true)");
    const std::vector<std::string> versions{"23.09", "24.12", ""};
    p.setIgnoreUpdateVersions(versions);
    calls.add("setIgnoreUpdateVersions");
    p.setCheckBetaUpdates(false);
    calls.add("setCheckBetaUpdates(false)");
    p.setCheckMotorDatabaseUpdates(false);
    calls.add("setCheckMotorDatabaseUpdates(false)");
    p.setAutoInstallMotorDatabaseUpdates(true);
    calls.add("setAutoInstallMotorDatabaseUpdates(true)");
    p.setIgnoreMotorDatabaseUpdateVersions(versions);
    calls.add("setIgnoreMotorDatabaseUpdateVersions");
    p.setReduceEffectsDuring3DInteraction(false);
    calls.add("setReduceEffectsDuring3DInteraction(false)");
    p.setMsaaEnabled(true);
    calls.add("setMsaaEnabled(true)");
    p.setDisplaySecondaryStability(false);
    calls.add("setDisplaySecondaryStability(false)");
    p.storeDefaultUnits();
    calls.add("storeDefaultUnits()");
    p.setDefaultDirectory(std::filesystem::path(u8"/designs/résumé"));
    calls.add("setDefaultDirectory");
    p.setDefaultFlightConfigName("[{motors}] config");
    calls.add("setDefaultFlightConfigName");
}

void writeSimulationFlags(Preferences& p, Calls& calls)
{
    p.setConfirmSimDeletion(false);
    calls.add("setConfirmSimDeletion(false)");
    p.setAutoRunSimulations(true);
    calls.add("setAutoRunSimulations(true)");
    p.setLaunchIntoWind(true);
    calls.add("setLaunchIntoWind(true)");
    p.setShowRasaeroFormatWarning(false);
    calls.add("setShowRasaeroFormatWarning(false)");
    p.setShowRocksimFormatWarning(false);
    calls.add("setShowRocksimFormatWarning(false)");
    p.setExportUserDirectories(true);
    calls.add("setExportUserDirectories(true)");
    p.setExportWindowInformation(true);
    calls.add("setExportWindowInformation(true)");
}

void writeLaunchSite(Preferences& p, Calls& calls)
{
    p.setDefaultMach(0.5);
    calls.add("setDefaultMach(0.5)");
    p.setDefaultMach(0.5);
    calls.add("setDefaultMach(0.5) again");
    p.setLaunchRodLength(1.75);
    calls.add("setLaunchRodLength(1.75)");
    p.setLaunchRodAngle(2.0);
    calls.add("setLaunchRodAngle(2.0), clamped");
    p.setLaunchRodDirection(-std::numbers::pi / 4);
    calls.add("setLaunchRodDirection(-pi/4)");
    p.setWindAverage(-3.5);
    calls.add("setWindAverage(-3.5)");
    p.setWindTurbulenceIntensity(0.25);
    calls.add("setWindTurbulenceIntensity(0.25)");
    p.setWindStandardDeviation(0.7);
    calls.add("setWindStandardDeviation(0.7)");
    p.setWindDirection(7.0);
    calls.add("setWindDirection(7.0)");
    const double rodDirection = p.getLaunchRodDirection();
    calls.add(std::format("getLaunchRodDirection() = {}", rodDirection));
    p.setLaunchIntoWind(false);
    const double ownRodDirection = p.getLaunchRodDirection();
    calls.add(std::format("getLaunchRodDirection() = {} not into the wind", ownRodDirection));
    p.setLaunchAltitude(1234.5);
    calls.add("setLaunchAltitude(1234.5)");
    p.setLaunchLatitude(123.0);
    calls.add("setLaunchLatitude(123.0), clamped");
    p.setLaunchLongitude(-200.0);
    calls.add("setLaunchLongitude(-200.0), clamped");
    p.setIsaAtmosphere(false);
    calls.add("setIsaAtmosphere(false)");
    p.setLaunchTemperature(300.25);
    calls.add("setLaunchTemperature(300.25)");
    p.setLaunchPressure(95000.5);
    calls.add("setLaunchPressure(95000.5)");
    p.setLaunchRelativeHumidity(0.4);
    calls.add("setLaunchRelativeHumidity(0.4)");
}

void writeSimulationDefaults(Preferences& p, Calls& calls)
{
    p.setGeodeticComputationName("WGS84");
    calls.add("setGeodeticComputationName(WGS84)");
    p.setGravityModelName("CONSTANT");
    calls.add("setGravityModelName(CONSTANT)");
    p.setConstantGravityValue(9.81);
    calls.add("setConstantGravityValue(9.81)");
    p.setSimulationStepperMethodName("RK6");
    calls.add("setSimulationStepperMethodName(RK6)");
    p.setRandomSeed(-123456789);
    calls.add("setRandomSeed(-123456789)");
    p.setRandomSeedFixed(true);
    calls.add("setRandomSeedFixed(true)");
    p.setTimeStep(0.01);
    calls.add("setTimeStep(0.01)");
    p.setMaxSimulationTime(0);
    calls.add("setMaxSimulationTime(0)");
    p.setRecoverySpeedWarning(21.5);
    calls.add("setRecoverySpeedWarning(21.5)");
    p.setDrogueLowSpeedWarning(4.5);
    calls.add("setDrogueLowSpeedWarning(4.5)");
    p.setRecoveryDrogueMainHighSpeedWarning(31.5);
    calls.add("setRecoveryDrogueMainHighSpeedWarning(31.5)");
    p.setRecoveryDrogueMainLowSpeedWarning(16.5);
    calls.add("setRecoveryDrogueMainLowSpeedWarning(16.5)");
}

void writeGuiBehaviour(Preferences& p, Calls& calls)
{
    p.setAutoOpenLastDesignOnStartup(true);
    calls.add("setAutoOpenLastDesignOnStartup(true)");
    p.setAlwaysOpenLeftmostTab(true);
    calls.add("setAlwaysOpenLeftmostTab(true)");
    p.setShowDiscardConfirmation(false);
    calls.add("setShowDiscardConfirmation(false)");
    p.setShowSaveRocketInfo(false);
    calls.add("setShowSaveRocketInfo(false)");
    p.setShowDiscardSimulationConfirmation(false);
    calls.add("setShowDiscardSimulationConfirmation(false)");
    p.setShowDiscardPreferencesConfirmation(false);
    calls.add("setShowDiscardPreferencesConfirmation(false)");
    p.setShowMarkers(true);
    calls.add("setShowMarkers(true)");
    p.setMatchForeDiameter(false);
    calls.add("setMatchForeDiameter(false)");
    p.setMatchAftDiameter(false);
    calls.add("setMatchAftDiameter(false)");
    p.setMotorNameColumn(false);
    calls.add("setMotorNameColumn(false)");
}

void writeComponentDefaults(Preferences& p, Calls& calls)
{
    p.setDefaultLineStyle("BodyComponent", LineStyle::DASHDOT);
    calls.add("setDefaultLineStyle(BodyComponent, DASHDOT)");
    p.setDefaultLineStyle("FinSet", LineStyle::DOTTED);
    calls.add("setDefaultLineStyle(FinSet, DOTTED)");
    p.setDefaultComponentMaterialString("BodyTube", "BULK|Mine|1234.5|CUSTOM");
    calls.add("setDefaultComponentMaterialString(BodyTube)");
    p.setDefaultComponentMaterialString("FinSet", std::nullopt);
    calls.add("setDefaultComponentMaterialString(FinSet, none)");
    p.setDefaultColor("MassObject", Color(1, 2, 3, 4));
    calls.add("setDefaultColor(MassObject)");
    p.setDefaultColor("RocketComponent", Color(250, 251, 252));
    calls.add("setDefaultColor(RocketComponent)");
}

void writeExports(Preferences& p, Calls& calls)
{
    p.setSvgStrokeColor(Color(9, 8, 7));
    calls.add("setSvgStrokeColor");
    p.setSvgStrokeWidth(0.25);
    calls.add("setSvgStrokeWidth(0.25)");
    p.setSvgDrawCrosshair(true);
    calls.add("setSvgDrawCrosshair(true)");
    p.setSvgCrosshairColor(Color(6, 5, 4));
    calls.add("setSvgCrosshairColor");
    p.setSvgCrosshairSize(3.5);
    calls.add("setSvgCrosshairSize(3.5)");
    p.setSvgShowLabels(true);
    calls.add("setSvgShowLabels(true)");
    p.setSvgLabelColor(Color(3, 2, 1));
    calls.add("setSvgLabelColor");
    p.setSvgPartSpacing(0.02);
    calls.add("setSvgPartSpacing(0.02)");
    p.setTextureGenerationDpi(600);
    calls.add("setTextureGenerationDpi(600)");
    p.setTextureGenerationDrawOutline(true);
    calls.add("setTextureGenerationDrawOutline(true)");
    p.setTextureGenerationOutlinePx(-3);
    calls.add("setTextureGenerationOutlinePx(-3)");
    p.setTextureGenerationResetTransforms(true);
    calls.add("setTextureGenerationResetTransforms(true)");
    p.setTextureGenerationOutlineColor(Color(100, 110, 120));
    calls.add("setTextureGenerationOutlineColor");
}

void writeWindCsvImport(Preferences& p, Calls& calls)
{
    p.setMultiLevelWindCsvImportHeader(false);
    calls.add("setMultiLevelWindCsvImportHeader(false)");
    p.setMultiLevelWindCsvImportAltitudeColumn("alt (ft)");
    calls.add("setMultiLevelWindCsvImportAltitudeColumn");
    p.setMultiLevelWindCsvImportAltitudeColumnIndex(3);
    calls.add("setMultiLevelWindCsvImportAltitudeColumnIndex(3)");
    p.setMultiLevelWindCsvImportAltitudeUnit(unitNamed(UnitGroupId::DISTANCE, "ft"));
    calls.add("setMultiLevelWindCsvImportAltitudeUnit(ft)");
    p.setMultiLevelWindCsvImportSpeedColumn("speed");
    calls.add("setMultiLevelWindCsvImportSpeedColumn");
    p.setMultiLevelWindCsvImportSpeedColumnIndex(4);
    calls.add("setMultiLevelWindCsvImportSpeedColumnIndex(4)");
    p.setMultiLevelWindCsvImportSpeedUnit(unitNamed(UnitGroupId::WINDSPEED, "mph"));
    calls.add("setMultiLevelWindCsvImportSpeedUnit(mph)");
    p.setMultiLevelWindCsvImportDirectionColumn("dir");
    calls.add("setMultiLevelWindCsvImportDirectionColumn");
    p.setMultiLevelWindCsvImportDirectionColumnIndex(5);
    calls.add("setMultiLevelWindCsvImportDirectionColumnIndex(5)");
    p.setMultiLevelWindCsvImportDirectionUnit(unitNamed(UnitGroupId::ANGLE, "rad"));
    calls.add("setMultiLevelWindCsvImportDirectionUnit(rad)");
    p.setMultiLevelWindCsvImportStddevColumn("sigma");
    calls.add("setMultiLevelWindCsvImportStddevColumn");
    p.setMultiLevelWindCsvImportStddevColumnIndex(6);
    calls.add("setMultiLevelWindCsvImportStddevColumnIndex(6)");
    p.setMultiLevelWindCsvImportStddevUnit(unitNamed(UnitGroupId::WINDSPEED, "km/h"));
    calls.add("setMultiLevelWindCsvImportStddevUnit(km/h)");
}

}  // namespace

std::vector<std::string> readEveryPreference(const Preferences& preferences)
{
    Lines lines;
    readStoreForms(preferences, lines);
    readUpdatesAndUnits(preferences, lines);
    readSimulationFlags(preferences, lines);
    readSimulationDefaults(preferences, lines);
    readGuiBehaviour(preferences, lines);
    readComponentDefaults(preferences, lines);
    readExports(preferences, lines);
    readWindCsvImport(preferences, lines);
    return lines.take();
}

std::vector<std::string> writeEveryPreference(Preferences& preferences)
{
    Calls calls(preferences);
    writeStoreForms(preferences, calls);
    writeUpdatesAndUnits(preferences, calls);
    writeSimulationFlags(preferences, calls);
    writeLaunchSite(preferences, calls);
    writeSimulationDefaults(preferences, calls);
    writeGuiBehaviour(preferences, calls);
    writeComponentDefaults(preferences, calls);
    writeExports(preferences, calls);
    writeWindCsvImport(preferences, calls);
    return calls.take();
}

}  // namespace QtRocket::Gui::Test
