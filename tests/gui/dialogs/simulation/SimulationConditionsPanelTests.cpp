// The launch conditions (OpenRocket's SimulationConditionsPanel): the "Launch conditions" tab of
// the simulation settings dialog over the first simulation of "A simple model rocket", and the
// panel of the Launch preference page over the launch preferences. What OpenRocket shows and
// does is the transcript of the probe tools/openrocket-probes/simulation-conditions
// (SimulationConditionsPins.h): its walks of the panel and its steps, which these tests take in
// the same order, a text typed as the probe sets it on a spinner model (DoubleBinding::
// commitText(): read in the field's unit, clamped to its bounds) and a click as doClick(). Where
// QtRocket differs on purpose, the pinned line is edited here, with the reason: OpenRocket bug 35
// (the missing text of the lowest temperatures), bug 36 (the multi-level summary) and the maxima
// of the user's decision U-M4-7.

#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindEditDialog.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/dialogs/simulation/WindLevelSummary.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/simulation/LaunchPreferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestPaths.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationConditionsPins.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationConditionsPanelTest = Test::GuiTest;
using Variant                       = SimulationConditionsPanel::Variant;
using Row                           = ConfigFields::NumberWidgets;
using Rows                          = std::initializer_list<const Row*>;
namespace Pins                      = Test::SimulationConditionsPins;

// ---------------------------------------------------------------------------------- the panels

/// The tab over the first simulation ("Simulation 1") of the example "A simple model rocket"
/// (data/examples), as the probe loads it; the services are the tests' (a fresh preference
/// store, as the probe's), so that "Reset to default" and "Save as default" use their defaults.
/// The window holds the panel; the scope its bindings, released before the window goes.
class ExampleTab
{
public:
    ExampleTab()
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork");
        if (loaded.has_value())
        {
            m_document   = std::move(loaded->document);
            m_simulation = m_document->getSimulation(0);
            m_options    = &m_simulation->getOptions();
        }
        else
        {
            ADD_FAILURE() << loaded.error().message;
        }
        m_panel = makeChild<SimulationConditionsPanel>(&m_window, *m_options, Variant::SIMULATION,
                                                       m_services.services(), m_scope);
    }
    ~ExampleTab() { m_scope.release(); }
    ExampleTab(const ExampleTab&)            = delete;
    ExampleTab& operator=(const ExampleTab&) = delete;
    ExampleTab(ExampleTab&&)                 = delete;
    ExampleTab& operator=(ExampleTab&&)      = delete;

    [[nodiscard]] SimulationConditionsPanel& panel() const noexcept { return *m_panel; }
    [[nodiscard]] SimulationOptions&         options() const noexcept { return *m_options; }
    [[nodiscard]] Test::TestGuiServices&     services() noexcept { return m_services; }
    [[nodiscard]] QWidget&                   window() noexcept { return m_window; }
    [[nodiscard]] BindingScope&              scope() noexcept { return m_scope; }
    /// The example's document; null, with a test failure, when it cannot be loaded.
    [[nodiscard]] OpenRocketDocument* document() const noexcept { return m_document.get(); }

private:
    Test::TestGuiServices                 m_services;
    ThrustCurveMotorSetDatabase           m_motors = QtRocket::Test::estesA8Database();
    QtRocket::Test::DesignFileEnvironment m_environment{
        QtRocket::Test::DesignFileEnvironment::Presets::NONE, m_motors};
    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
    /// The simulation's options; others, with a test failure, when the example cannot be loaded.
    SimulationOptions          m_fallback;
    SimulationOptions*         m_options{&m_fallback};
    QWidget                    m_window;
    BindingScope               m_scope{m_window, nullptr};
    SimulationConditionsPanel* m_panel{nullptr};
};

/// The Launch preference page's panel over the launch preferences of the tests' fresh store.
class LaunchPage
{
public:
    LaunchPage()
      : m_panel(makeChild<SimulationConditionsPanel>(
            &m_window, m_launch, Variant::LAUNCH_PREFERENCES, m_services.services(), m_scope))
    {
    }
    ~LaunchPage() { m_scope.release(); }
    LaunchPage(const LaunchPage&)            = delete;
    LaunchPage& operator=(const LaunchPage&) = delete;
    LaunchPage(LaunchPage&&)                 = delete;
    LaunchPage& operator=(LaunchPage&&)      = delete;

    [[nodiscard]] SimulationConditionsPanel& panel() const noexcept { return *m_panel; }
    [[nodiscard]] Test::TestGuiServices&     services() noexcept { return m_services; }
    [[nodiscard]] LaunchPreferences&         launch() noexcept { return m_launch; }

private:
    Test::TestGuiServices      m_services;
    LaunchPreferences          m_launch{m_services.preferences()};
    QWidget                    m_window;
    BindingScope               m_scope{m_window, nullptr};
    SimulationConditionsPanel* m_panel;
};

// ------------------------------------------------------------------------------- the transcript

/// "true" or "false", as Java prints a boolean (and std::format).
[[nodiscard]] std::string javaBool(bool value)
{
    return std::format("{}", value);
}

/// The text of @p row's label, as the probe names a row.
[[nodiscard]] std::string labelOf(const Row& row)
{
    return QtStrings::toUtf8(row.label->text());
}

/// What the probe does to type @p text into @p row: its spinner model's setValue(text).
void enter(const Row& row, std::string_view text)
{
    static_cast<void>(row.spinBox->binding()->commitText(text));
}

/// The probe's type(): enters @p text into @p row and gives the step's line with the state of
/// @p shown.
[[nodiscard]] std::string typed(std::string_view step, const Row& row, std::string_view text,
                                Rows shown)
{
    enter(row, text);
    std::string line = "STEP " + std::string(step) + " typed=[" + std::string(text) + "]";
    for (const Row* shownRow : shown)
    {
        line += " | " + Test::numberRowState(labelOf(*shownRow), *shownRow);
    }
    return line;
}

/// Each of @p texts entered into @p row in turn, with the step's line of @p row's state.
void typeEach(std::vector<std::string>& lines, std::string_view step, const Row& row,
              std::span<const std::string_view> texts, Rows shown)
{
    for (const std::string_view text : texts)
    {
        lines.push_back(typed(step, row, text, shown));
    }
}

/// The walk of @p widget, each line with the probe's prefix "WALK ".
void appendWalk(std::vector<std::string>& lines, const QWidget& widget)
{
    for (const std::string& line : Test::dialogWalk(widget))
    {
        lines.push_back("WALK " + line);
    }
}

/// The pinned lines of @p sections in order, without their MODEL and BOX lines (not pinned) and
/// without the SUMMARY lines (WindLevelSummaryTests).
[[nodiscard]] std::vector<std::string> pinnedSteps(
    std::initializer_list<std::span<const std::string_view>> sections)
{
    std::vector<std::string> lines;
    for (const std::span<const std::string_view> section : sections)
    {
        for (const std::string_view line : section)
        {
            if (!line.starts_with("SUMMARY "))
            {
                lines.emplace_back(line);
            }
        }
    }
    return lines;
}

using Gui::Test::replacedIn;
using Gui::Test::without;

// --------------------------------------------------------------------------------- the sections

/// The probe's "the atmosphere" and "atmosphere with the ISA off".
[[nodiscard]] std::vector<std::string> atmosphereSteps(const SimulationConditionsPanel& panel)
{
    std::vector<std::string> lines;
    const Row&               temperature = panel.temperature();
    const Row&               pressure    = panel.pressure();
    const Row&               humidity    = panel.relativeHumidity();
    const Row&               altitude    = panel.altitude();
    const auto               isaState    = [&](std::string_view step) {
        return "STEP " + std::string(step) +
               " selected=" + javaBool(panel.isaCheckBox().isChecked()) + " | " +
               Test::numberRowState("T", temperature) + " | " +
               Test::numberRowState("p", pressure) + " | " + Test::numberRowState("h", humidity);
    };
    panel.isaCheckBox().click();
    lines.push_back(isaState("isa off"));
    appendWalk(lines, panel.atmosphereBox());
    constexpr auto kTemperatures =
        std::to_array<std::string_view>({"15", "-39.99", "-40", "-79.99", "49.99", "50", "79.99",
                                         "80", "120", "-80", "-100", "-273.15", "-300", "20"});
    typeEach(lines, "temperature", temperature, kTemperatures, {&temperature});
    constexpr auto kPressuresAtSeaLevel =
        std::to_array<std::string_view>({"759.9", "760", "1266.5", "1266.6", "1013.25"});
    typeEach(lines, "pressure at 0 m", pressure, kPressuresAtSeaLevel, {&pressure});
    lines.push_back(typed("altitude", altitude, "1000", {&altitude, &pressure}));
    constexpr auto kPressuresAt1000 =
        std::to_array<std::string_view>({"674", "675", "1123", "1124", "500"});
    typeEach(lines, "pressure at 1000 m", pressure, kPressuresAt1000, {&pressure});
    panel.isaCheckBox().click();
    lines.push_back(isaState("isa on"));
    lines.push_back(typed("altitude", altitude, "0", {&altitude, &temperature, &pressure}));
    panel.isaCheckBox().click();
    constexpr auto kHumidities = std::to_array<std::string_view>({"50", "100", "150", "-5"});
    typeEach(lines, "humidity", humidity, kHumidities, {&humidity});
    panel.isaCheckBox().click();
    return lines;
}

/// The probe's "the launch site".
[[nodiscard]] std::vector<std::string> launchSiteSteps(const SimulationConditionsPanel& panel)
{
    std::vector<std::string> lines;
    constexpr auto           kAltitudes = std::to_array<std::string_view>(
        {"100", "250", "500", "1000", "5000", "11018", "12000", "-10", "0"});
    typeEach(lines, "altitude", panel.altitude(), kAltitudes, {&panel.altitude()});
    constexpr auto kLatitudes = std::to_array<std::string_view>({"-90", "0", "33.3", "90", "95"});
    typeEach(lines, "latitude", panel.latitude(), kLatitudes, {&panel.latitude()});
    constexpr auto kLongitudes = std::to_array<std::string_view>({"-180", "-80.6", "180", "200"});
    typeEach(lines, "longitude", panel.longitude(), kLongitudes, {&panel.longitude()});
    return lines;
}

/// The probe's turbulence steps with the intensity's description.
void appendIntensitySteps(std::vector<std::string>& lines, const SimulationConditionsPanel& panel)
{
    constexpr auto kIntensities = std::to_array<std::string_view>(
        {"0", "0.09", "0.1", "4.99", "5", "9.99", "10", "15", "20", "25", "30"});
    for (const std::string_view text : kIntensities)
    {
        enter(panel.turbulenceIntensity(), text);
        lines.push_back("STEP turbulence typed=[" + std::string(text) + "] | " +
                        Test::numberRowState("turbulence", panel.turbulenceIntensity()) +
                        " | intensity=" + QtStrings::toUtf8(panel.intensityLabel().text()));
    }
}

/// The probe's "the wind".
[[nodiscard]] std::vector<std::string> windSteps(const SimulationConditionsPanel& panel)
{
    std::vector<std::string> lines;
    const Row&               average    = panel.averageWind();
    const Row&               deviation  = panel.standardDeviation();
    const Row&               turbulence = panel.turbulenceIntensity();
    const Row&               direction  = panel.windDirection();
    const Rows               all{&average, &deviation, &turbulence};
    constexpr auto           kAverages =
        std::to_array<std::string_view>({"9.99", "10", "19.99", "20", "25", "2"});
    typeEach(lines, "average", average, kAverages, all);
    lines.push_back(typed("deviation", deviation, "5", all));
    lines.push_back(typed("deviation", deviation, "0.5", all));
    lines.push_back(typed("turbulence", turbulence, "50", all));
    lines.push_back(typed("turbulence", turbulence, "150", all));
    lines.push_back(typed("average", average, "4", all));
    lines.push_back(typed("average", average, "0", all));
    lines.push_back(
        typed("average", average, "-3", {&average, &deviation, &turbulence, &direction}));
    lines.push_back(
        typed("average", average, "2", {&average, &deviation, &turbulence, &direction}));
    lines.push_back(typed("turbulence", turbulence, "10", all));
    appendIntensitySteps(lines, panel);
    lines.push_back(typed("turbulence", turbulence, "10", {&turbulence}));
    constexpr auto kDirections =
        std::to_array<std::string_view>({"0", "90", "359", "360", "400", "-90", "90"});
    typeEach(lines, "wind direction", direction, kDirections, {&direction, &panel.rodDirection()});
    return lines;
}

/// The probe's "the launch rod" and "launch rod with into wind off".
[[nodiscard]] std::vector<std::string> launchRodSteps(const SimulationConditionsPanel& panel)
{
    std::vector<std::string> lines;
    const Row&               length    = panel.rodLength();
    const Row&               angle     = panel.rodAngle();
    const Row&               direction = panel.rodDirection();
    constexpr auto           kLengths =
        std::to_array<std::string_view>({"0", "50", "100", "200", "500", "1000", "-5", "100"});
    typeEach(lines, "length", length, kLengths, {&length});
    constexpr auto kAngles =
        std::to_array<std::string_view>({"44.99", "45", "59.99", "60", "-45", "-60", "75", "0"});
    typeEach(lines, "angle", angle, kAngles, {&angle});
    panel.intoWindCheckBox().click();
    lines.push_back(
        "STEP into wind off selected=" + javaBool(panel.intoWindCheckBox().isChecked()) + " | " +
        Test::numberRowState("direction", direction));
    appendWalk(lines, panel.launchRodBox());
    lines.push_back(typed("rod direction", direction, "45", {&direction, &panel.windDirection()}));
    panel.intoWindCheckBox().click();
    lines.push_back("STEP into wind on selected=" + javaBool(panel.intoWindCheckBox().isChecked()) +
                    " | " + Test::numberRowState("direction", direction));
    lines.push_back(typed("wind direction", panel.windDirection(), "180",
                          {&panel.windDirection(), &direction}));
    return lines;
}

/// The probe's "the wind model" and the two walks that follow it.
[[nodiscard]] std::vector<std::string> windModelSteps(const SimulationConditionsPanel& panel,
                                                      SimulationOptions&               options)
{
    std::vector<std::string> lines;
    panel.multiLevelWindButton()->click();
    lines.push_back("STEP multi-level type=" +
                    std::string(windModelTypeName(options.getWindModelType())));
    appendWalk(lines, panel.windBox());
    EXPECT_TRUE(options.getMultiLevelWindModel().addWindLevel(300, 6, 0).has_value());
    appendWalk(lines, panel.windBox());
    panel.averageWindButton()->click();
    lines.push_back("STEP average type=" +
                    std::string(windModelTypeName(options.getWindModelType())));
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    lines.push_back("STEP set multi-level in code: average=" +
                    javaBool(panel.averageWindButton()->isChecked()) +
                    " multi-level=" + javaBool(panel.multiLevelWindButton()->isChecked()));
    return lines;
}

/// The probe's "the buttons" and "tab after Reset to default", from the multi-level wind the
/// wind model's steps leave.
[[nodiscard]] std::vector<std::string> buttonSteps(const SimulationConditionsPanel& panel,
                                                   const SimulationOptions&         options,
                                                   const Preferences&               preferences)
{
    std::vector<std::string> lines;
    lines.push_back(typed("average", panel.averageWind(), "5", {&panel.averageWind()}));
    lines.push_back(typed("latitude", panel.latitude(), "10", {&panel.latitude()}));
    panel.resetButton()->click();
    lines.push_back(
        "STEP reset type=" + std::string(windModelTypeName(options.getWindModelType())) +
        " average=" + javaBool(panel.averageWindButton()->isChecked()) +
        " timeStep=" + Strings::javaDoubleToString(options.getTimeStep()) +
        " maxTime=" + Strings::javaDoubleToString(options.getMaxSimulationTime()));
    appendWalk(lines, panel);
    lines.push_back(typed("average", panel.averageWind(), "3.5", {&panel.averageWind()}));
    lines.push_back(typed("length", panel.rodLength(), "150", {&panel.rodLength()}));
    panel.saveButton()->click();
    lines.push_back(
        "STEP save windAverage=" + Strings::javaDoubleToString(preferences.getWindAverage()) +
        " rodLength=" + Strings::javaDoubleToString(preferences.getLaunchRodLength()));
    return lines;
}

// ------------------------------------------------------------------------- the fields' models

/// A slider's model as the probe's MODEL line gives it.
struct SliderPin
{
    bool   linear;
    double min;
    double mid;
    double max;
    double linearPosition;
};

/// What the probe's MODEL line of a field says of its DoubleModel: the unit group, the bounds
/// and the slider's model.
struct FieldPin
{
    const Row*  row;
    UnitGroupId units;
    double      min;
    double      max;
    SliderPin   slider;
};

/// The name of @p units, one of the process-wide groups (unitGroupName()).
[[nodiscard]] std::string unitsName(const UnitGroup& units)
{
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        if (&unitGroup(id) == &units)
        {
            return std::string(unitGroupName(id));
        }
    }
    return "?";
}

/// A field's model in the notation of the probe's MODEL lines, the doubles as Java prints them.
[[nodiscard]] std::string modelText(const UnitGroup& units, double min, double max,
                                    const SliderPin& slider)
{
    return "units=" + unitsName(units) + " min=" + Strings::javaDoubleToString(min) +
           " max=" + Strings::javaDoubleToString(max) +
           " slider=" + (slider.linear ? "linear" : "quadratic") +
           " min=" + Strings::javaDoubleToString(slider.min) +
           " mid=" + Strings::javaDoubleToString(slider.mid) +
           " max=" + Strings::javaDoubleToString(slider.max) +
           " linearPosition=" + Strings::javaDoubleToString(slider.linearPosition);
}

/// The model of @p row's field: its binding's unit group and bounds and its slider's mapping.
[[nodiscard]] std::string fieldModelText(const Row& row)
{
    const DoubleBinding& binding = *row.spinBox->binding();
    const SliderMapping& mapping = row.slider->mapping();
    return modelText(binding.getUnitGroup(), binding.minimum(), binding.maximum(),
                     {.linear         = mapping.isLinear(),
                      .min            = mapping.getMin(),
                      .mid            = mapping.getMid(),
                      .max            = mapping.getMax(),
                      .linearPosition = mapping.getLinearPosition()});
}

/// A linear slider's model: its mid is its max (Java's ValueSliderModel(min, max)).
[[nodiscard]] constexpr SliderPin linearSlider(double min, double max)
{
    return {.linear = true, .min = min, .mid = max, .max = max, .linearPosition = 1};
}

/// The fields' models of @p panel, as the probe's MODEL lines give them, but for the maxima of
/// U-M4-7 (each marked) and the standard deviation's, which is the average's binding (checked
/// apart).
[[nodiscard]] std::vector<FieldPin> pinnedFieldModels(const SimulationConditionsPanel& panel)
{
    constexpr double kPi       = std::numbers::pi;
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    // "MODEL LaunchRodAngle ... min=-1.0471975511965976 max=1.0471975511965976": pi / 3.
    constexpr double kMaxAngle = kPi / 3;
    return {
        // Java's max: 1.7976931348623157E308 (U-M4-7: SimulationInputRanges' 1000 m/s).
        {.row    = &panel.averageWind(),
         .units  = UnitGroupId::WINDSPEED,
         .min    = 0,
         .max    = 1000,
         .slider = linearSlider(0, 10)},
        {.row    = &panel.turbulenceIntensity(),
         .units  = UnitGroupId::RELATIVE,
         .min    = 0,
         .max    = 1,
         .slider = linearSlider(0, 1)},
        {.row    = &panel.windDirection(),
         .units  = UnitGroupId::ANGLE,
         .min    = 0,
         .max    = 2 * kPi,
         .slider = linearSlider(0, 2 * kPi)},
        // Java's max: Infinity (U-M4-7: 1000 K).
        {.row    = &panel.temperature(),
         .units  = UnitGroupId::TEMPERATURE,
         .min    = 0,
         .max    = 1000,
         .slider = linearSlider(253.15, 308.15)},
        // Java's max: Infinity (U-M4-7: 1000000 Pa).
        {.row    = &panel.pressure(),
         .units  = UnitGroupId::PRESSURE,
         .min    = 0.001,
         .max    = 1000000,
         .slider = linearSlider(95000, 105000)},
        // Java's max: Infinity (U-M4-7: 1).
        {.row    = &panel.relativeHumidity(),
         .units  = UnitGroupId::RELATIVE,
         .min    = 0,
         .max    = 1,
         .slider = linearSlider(0, 1)},
        {.row    = &panel.latitude(),
         .units  = UnitGroupId::LATITUDE,
         .min    = -90,
         .max    = 90,
         .slider = linearSlider(-90, 90)},
        {.row    = &panel.longitude(),
         .units  = UnitGroupId::LONGITUDE,
         .min    = -180,
         .max    = 180,
         .slider = linearSlider(-180, 180)},
        {.row    = &panel.altitude(),
         .units  = UnitGroupId::DISTANCE,
         .min    = 0,
         .max    = 11018.064362274883,
         .slider = {.linear = false, .min = 0, .mid = 250, .max = 1000, .linearPosition = 0.5}},
        // Java's max: Infinity (U-M4-7: 1000 m).
        {.row    = &panel.rodLength(),
         .units  = UnitGroupId::LENGTH,
         .min    = 0,
         .max    = 1000,
         .slider = {.linear = false, .min = 0, .mid = 1, .max = 5, .linearPosition = 0.5}},
        {.row    = &panel.rodAngle(),
         .units  = UnitGroupId::ANGLE,
         .min    = -kMaxAngle,
         .max    = kMaxAngle,
         .slider = linearSlider(-kMaxAngle, kMaxAngle)},
        // Java's min: -Infinity (the Integer 0 its DoubleModel is given is no bound).
        {.row    = &panel.rodDirection(),
         .units  = UnitGroupId::ANGLE,
         .min    = -kInfinity,
         .max    = 2 * kPi,
         .slider = linearSlider(0, 2 * kPi)},
    };
}

void expectPinnedFieldModels(const SimulationConditionsPanel& panel)
{
    for (const FieldPin& pin : pinnedFieldModels(panel))
    {
        const Row& row = *pin.row;
        EXPECT_EQ(fieldModelText(row),
                  modelText(unitGroup(pin.units), pin.min, pin.max, pin.slider))
            << QtStrings::toUtf8(row.spinBox->objectName());
        // The three widgets of a field carry the Java property's name.
        EXPECT_EQ(row.unitSelector->objectName(), row.spinBox->objectName());
        EXPECT_EQ(row.slider->objectName(), row.spinBox->objectName());
    }
}

/// "MODEL StandardDeviation ... units=UNITS_WINDSPEED min=0.0 max=model:Average ...
/// slider=linear min=0.0 mid=model:Average max=model:Average linearPosition=1.0": the average
/// speed's binding is the maximum and the slider's (dynamic) maximum.
void expectDeviationModel(const SimulationConditionsPanel& panel)
{
    const Row&                            deviation = panel.standardDeviation();
    const std::shared_ptr<DoubleBinding>& average   = panel.averageWind().spinBox->binding();
    const double                          speed     = average->value();
    EXPECT_EQ(fieldModelText(deviation),
              modelText(unitGroup(UnitGroupId::WINDSPEED), 0, speed, linearSlider(0, speed)));
    EXPECT_EQ(deviation.slider->dynamicBounds().max, average);
    EXPECT_EQ(deviation.slider->dynamicBounds().min, nullptr);
}

// ------------------------------------------------------------------------------------ geometry

/// @p widget's rectangle in @p panel's coordinates.
[[nodiscard]] QRect rectIn(const QWidget& widget, const QWidget& panel)
{
    return {widget.mapTo(&panel, QPoint(0, 0)), widget.size()};
}

/// A relation of the layout and whether it holds.
struct Relation
{
    std::string_view name;
    bool             holds;
};

/// The names of the relations of @p relations that do not hold, each followed by "; ".
[[nodiscard]] std::string unmet(std::initializer_list<Relation> relations)
{
    std::string names;
    for (const Relation& relation : relations)
    {
        if (!relation.holds)
        {
            names += std::string(relation.name) + "; ";
        }
    }
    return names;
}

/// The probe's BOX lines of the tab as relations ("BOX BORDER Wind x=6 y=6", "Atmospheric
/// conditions x=6 y=236", "Launch site x=380 y=6", "Launch rod x=380 y=139", "BUTTON Reset to
/// default x=421 y=403", "Save as default x=574 y=403"): the wind above the atmosphere on the
/// left, the launch site above the launch rod on the right, the rod's top above the atmosphere's
/// (the columns do not share their heights), the buttons below both, on the right.
[[nodiscard]] std::string unmetTabRelations(const SimulationConditionsPanel& panel)
{
    const QRect wind  = rectIn(panel.windBox(), panel);
    const QRect air   = rectIn(panel.atmosphereBox(), panel);
    const QRect site  = rectIn(panel.launchSiteBox(), panel);
    const QRect rod   = rectIn(panel.launchRodBox(), panel);
    const QRect reset = rectIn(*panel.resetButton(), panel);
    const QRect save  = rectIn(*panel.saveButton(), panel);
    return unmet({
        {.name = "the atmosphere in the wind's column", .holds = air.left() == wind.left()},
        {.name = "the atmosphere below the wind", .holds = air.top() > wind.bottom()},
        {.name = "the rod in the site's column", .holds = rod.left() == site.left()},
        {.name = "the rod below the site", .holds = rod.top() > site.bottom()},
        {.name = "the site right of the wind", .holds = site.left() > wind.right()},
        {.name = "the site level with the wind", .holds = site.top() == wind.top()},
        {.name = "the rod's top above the atmosphere's", .holds = rod.top() < air.top()},
        {.name = "the buttons below the boxes", .holds = reset.top() > air.bottom()},
        {.name = "the buttons on one line", .holds = reset.top() == save.top()},
        {.name = "the buttons on the right", .holds = reset.left() > wind.right()},
        {.name = "Reset left of Save", .holds = reset.right() < save.left()},
    });
}

/// A row's relations: label, spin box, unit and slider from left to right on one line.
[[nodiscard]] std::string unmetRowRelations(const Row& row, const QWidget& panel)
{
    const QRect label   = rectIn(*row.label, panel);
    const QRect spinBox = rectIn(*row.spinBox, panel);
    const QRect unit    = rectIn(*row.unitSelector, panel);
    const QRect slider  = rectIn(*row.slider, panel);
    const int   middle  = spinBox.center().y();
    const int   half    = spinBox.height() / 2;
    return unmet({
        {.name = "the label left of the spin box", .holds = label.right() < spinBox.left()},
        {.name = "the spin box left of the unit", .holds = spinBox.right() < unit.left()},
        {.name = "the unit left of the slider", .holds = unit.right() < slider.left()},
        {.name  = "the label level with the spin box",
         .holds = std::abs(label.center().y() - middle) < half},
        {.name  = "the slider level with the spin box",
         .holds = std::abs(slider.center().y() - middle) < half},
    });
}

/// The rows' relations, and the intensity's description in the spin boxes' column below the
/// turbulence intensity ("BOX LABEL Medium x=161 y=159" under "BOX SPINNER 10 x=161 y=129").
[[nodiscard]] std::string unmetRowsRelations(const SimulationConditionsPanel& panel)
{
    std::string names;
    for (const Row* row :
         {&panel.averageWind(), &panel.temperature(), &panel.latitude(), &panel.rodDirection()})
    {
        names += unmetRowRelations(*row, panel);
    }
    const QRect turbulence = rectIn(*panel.turbulenceIntensity().spinBox, panel);
    const QRect intensity  = rectIn(panel.intensityLabel(), panel);
    const QRect direction  = rectIn(*panel.windDirection().spinBox, panel);
    return names + unmet({
                       {.name  = "the description in the spin boxes' column",
                        .holds = intensity.left() == turbulence.left()},
                       {.name  = "the description below the turbulence intensity",
                        .holds = intensity.top() > turbulence.bottom()},
                       {.name  = "the description above the wind direction",
                        .holds = intensity.bottom() < direction.top()},
                   });
}

/// The probe's wind model steps with QtRocket's summary after the level was added: OpenRocket
/// bug 36, Java's summary still shows the one level it showed before; QtRocket's follows the
/// levels (the first walk's one-level text stays).
[[nodiscard]] std::vector<std::string> windModelExpectation()
{
    std::vector<std::string> expected = pinnedSteps({Pins::kWindModel, Pins::kWindWithMultiLevel});
    std::vector<std::string> added    = replacedIn(
        pinnedSteps({Pins::kWindAfterLevelAdded}),
        "1 wind level(s)<br>Altitude: 0 m<br>Speed: 3.8 m/s",
        "2 wind level(s)<br>Altitude range: 0 m - 300 m<br>Speed range: 3.8 m/s - 6 m/s", 1);
    expected.insert(expected.end(), added.begin(), added.end());
    return expected;
}

/// Closes every modal dialog that opens while it lives, and counts them: a key that clicks a
/// button opening the wind editor fails the test, where the editor's exec() would wait.
class ModalDialogCounter
{
public:
    ModalDialogCounter()
    {
        m_timer.setInterval(std::chrono::milliseconds(1));
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { closeModalDialog(); });
        m_timer.start();
    }

    /// How many modal dialogs were closed.
    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    void closeModalDialog()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr && dialog->isVisible())
        {
            ++m_count;
            dialog->reject();
        }
    }

    QTimer m_timer;
    int    m_count = 0;
};

/// Gives each of @p widgets the focus and presses Return there.
void pressReturnInEach(std::initializer_list<QWidget*> widgets)
{
    for (QWidget* const widget : widgets)
    {
        widget->setFocus();
        QTest::keyClick(widget, Qt::Key_Return);
    }
}

/// The texts of @p dialog's push buttons that are its default button or would become it.
[[nodiscard]] std::vector<std::string> defaultButtonsOf(const QDialog& dialog)
{
    std::vector<std::string> texts;
    for (const QPushButton* const button : dialog.findChildren<QPushButton*>())
    {
        if (button->isDefault() || button->autoDefault())
        {
            texts.push_back(QtStrings::toUtf8(button->text()));
        }
    }
    return texts;
}

/// Options launching into a multi-level wind of three levels with the directions 0, 90 and 180
/// degrees at 0, 100 and 200 m.
void launchIntoThreeWindLevels(SimulationOptions& options)
{
    options.setLaunchIntoWind(true);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    MultiLevelPinkNoiseWindModel& wind = options.getMultiLevelWindModel();
    wind.clearLevels();
    EXPECT_TRUE(wind.addWindLevel(0, 4, 0).has_value());
    EXPECT_TRUE(wind.addWindLevel(100, 4, std::numbers::pi / 2).has_value());
    EXPECT_TRUE(wind.addWindLevel(200, 4, std::numbers::pi).has_value());
}

// -------------------------------------------------------------------------------------- tests

/// What the test of "Edit levels" does with the wind editor: checks it (over the panel's window,
/// modal for the application, a row per level), adds a level and closes it.
struct WindEditorAnswer
{
    const QWidget* window = nullptr;
    std::size_t    levels = 0;
    bool*          shown  = nullptr;

    void operator()(QWidget& dialog) const
    {
        auto* const editor = qobject_cast<MultiLevelWindEditDialog*>(&dialog);
        ASSERT_NE(editor, nullptr);
        *shown = true;
        EXPECT_EQ(editor->parentWidget(), window);
        EXPECT_EQ(editor->windowModality(), Qt::ApplicationModal);
        EXPECT_EQ(editor->tableModel().rowCount(), static_cast<int>(levels));
        editor->addButton().click();
        editor->closeButton().click();
    }
};

TEST_F(SimulationConditionsPanelTest, TheTabShowsWhatOpenRocketsTabShows)
{
    const ExampleTab tab;
    EXPECT_EQ(Gui::Test::dialogWalk(tab.panel()), Gui::Test::linesWithPrefix(Pins::kTab, "WALK "));
}

TEST_F(SimulationConditionsPanelTest, TheLaunchPageShowsWhatOpenRocketsLaunchPageShows)
{
    const LaunchPage page;
    EXPECT_EQ(Gui::Test::dialogWalk(page.panel()),
              Gui::Test::linesWithPrefix(Pins::kLaunchPage, "WALK "));
    EXPECT_EQ(page.panel().averageWindButton(), nullptr);
    EXPECT_EQ(page.panel().windCards(), nullptr);
    EXPECT_EQ(page.panel().windLevelSummary(), nullptr);
    EXPECT_EQ(page.panel().resetButton(), nullptr);
    EXPECT_EQ(page.panel().saveButton(), nullptr);
}

TEST_F(SimulationConditionsPanelTest, TheFieldsHaveJavasUnitsBoundsAndSliders)
{
    const ExampleTab tab;
    expectPinnedFieldModels(tab.panel());
    expectDeviationModel(tab.panel());
    const LaunchPage page;
    expectPinnedFieldModels(page.panel());
    expectDeviationModel(page.panel());
}

TEST_F(SimulationConditionsPanelTest, TheBoxesAndRowsAreLaidOutAsJavas)
{
    ExampleTab tab;
    ASSERT_TRUE(Gui::Test::showActiveWith(tab.window(), {&tab.panel()}));
    EXPECT_EQ(unmetTabRelations(tab.panel()), "");
    EXPECT_EQ(unmetRowsRelations(tab.panel()), "");
}

TEST_F(SimulationConditionsPanelTest, TheAtmosphereFollowsTheStandardAtmosphereAndValidates)
{
    const ExampleTab tab;
    // OpenRocket bug 35: Java's key has no text; its translator reports it once (the bug report
    // dialog: the probe's HANDLER line) and the tooltip shows the key.
    std::vector<std::string> expected = replacedIn(
        without(pinnedSteps({Pins::kAtmosphere, Pins::kAtmosphereWithIsaOff}),
                "HANDLER Can not find translation for 'simedtdlg.error.temperatureTooLow' "
                "locale=en_US (java.util.MissingResourceException)"),
        "<html>simedtdlg.error.temperatureTooLow</html>", "<html>Extremely low temperature</html>",
        4);
    // U-M4-7: the humidity's maximum is 1 (Java has none).
    expected = replacedIn(std::move(expected), "Relative humidity:=[150] at 1000",
                          "Relative humidity:=[100] at 1000", 1);
    EXPECT_EQ(atmosphereSteps(tab.panel()), expected);
}

TEST_F(SimulationConditionsPanelTest, TheLaunchSiteFieldsEditTheOptions)
{
    const ExampleTab tab;
    EXPECT_EQ(launchSiteSteps(tab.panel()), pinnedSteps({Pins::kLaunchSite}));
    EXPECT_EQ(tab.options().getLaunchLongitude(), 180.0);
    EXPECT_EQ(tab.options().getLaunchLatitude(), 90.0);
}

TEST_F(SimulationConditionsPanelTest, TheWindFieldsEditTheAverageWindModel)
{
    const ExampleTab tab;
    EXPECT_EQ(windSteps(tab.panel()), pinnedSteps({Pins::kWind}));
    EXPECT_DOUBLE_EQ(tab.options().getAverageWindModel().getAverage(), 2.0);
}

TEST_F(SimulationConditionsPanelTest, TheLaunchRodFieldsEditTheOptions)
{
    const ExampleTab tab;
    EXPECT_EQ(launchRodSteps(tab.panel()),
              pinnedSteps({Pins::kLaunchRod, Pins::kLaunchRodWithIntoWindOff}));
}

TEST_F(SimulationConditionsPanelTest, TheWindModelChoiceFollowsTheOptions)
{
    const ExampleTab tab;
    EXPECT_EQ(windModelSteps(tab.panel(), tab.options()), windModelExpectation());
    // The options' multi-level wind, set in code, shows its card.
    EXPECT_EQ(tab.panel().windCards()->currentWidget(), tab.panel().windLevelSummary());
}

TEST_F(SimulationConditionsPanelTest, TheMultiLevelSummaryFollowsAnUndoAndResetToDefault)
{
    // OpenRocket bug 36: Java writes the summary again only when the wind editor closes.
    const ExampleTab tab;
    ASSERT_NE(tab.document(), nullptr);
    tab.document()->addUndoPosition("Edit Simulation 1");
    tab.options().setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_TRUE(tab.options().getMultiLevelWindModel().addWindLevel(300, 6, 0).has_value());
    const QLabel& summary = tab.panel().windLevelSummary()->summaryLabel();
    EXPECT_EQ(summary.text(), QStringLiteral("<html>2 wind level(s)<br>Altitude range: 0 m - 300 m"
                                             "<br>Speed range: 3.8 m/s - 6 m/s</html>"));
    tab.document()->undo();
    EXPECT_EQ(summary.text(),
              QStringLiteral("<html>1 wind level(s)<br>Altitude: 0 m<br>Speed: 3.8 m/s</html>"));
    EXPECT_TRUE(tab.panel().averageWindButton()->isChecked());
    EXPECT_TRUE(tab.options().getMultiLevelWindModel().addWindLevel(300, 6, 0).has_value());
    // The default options' level: the preferences' wind, 2 m/s.
    tab.panel().resetButton()->click();
    EXPECT_EQ(summary.text(),
              QStringLiteral("<html>1 wind level(s)<br>Altitude: 0 m<br>Speed: 2 m/s</html>"));
}

TEST_F(SimulationConditionsPanelTest, ResetAndSaveAsDefaultUseTheDefaultOptionFactory)
{
    ExampleTab tab;
    // Where the probe's wind model steps leave the options.
    tab.options().setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(buttonSteps(tab.panel(), tab.options(), tab.services().preferences()),
              pinnedSteps({Pins::kButtons, Pins::kTabAfterReset}));
}

TEST_F(SimulationConditionsPanelTest, ThePanelShowsChangesMadeElsewhere)
{
    const ExampleTab tab;
    tab.options().setLaunchRodLength(2.5);
    tab.options().setIsaAtmosphere(false);
    tab.options().setLaunchTemperature(300);
    tab.options().getAverageWindModel().setAverage(12);
    tab.options().setLaunchIntoWind(false);
    tab.options().setLaunchRodDirection(std::numbers::pi);
    EXPECT_EQ(tab.panel().rodLength().spinBox->text(), QStringLiteral("250"));
    EXPECT_TRUE(tab.panel().temperature().spinBox->isEnabled());
    EXPECT_EQ(tab.panel().temperature().spinBox->text(), QStringLiteral("26.85"));
    EXPECT_EQ(tab.panel().averageWind().spinBox->text(), QStringLiteral("12"));
    EXPECT_EQ(FieldValidator::outlineOf(*tab.panel().averageWind().spinBox),
              FieldValidator::Outline::WARNING);
    EXPECT_FALSE(tab.panel().intoWindCheckBox().isChecked());
    EXPECT_TRUE(tab.panel().rodDirection().spinBox->isEnabled());
    EXPECT_EQ(tab.panel().rodDirection().spinBox->text(), QStringLiteral("180"));
}

TEST_F(SimulationConditionsPanelTest, TheEditLevelsButtonOpensTheWindEditor)
{
    // Java: a MultiLevelWindEditDialog over the options' multi-level wind, modal for the
    // application, over the panel's window; the summary follows what it changes (bug 36).
    ExampleTab       tab;
    const QSignalSpy requests(&tab.panel(), &SimulationConditionsPanel::editWindLevelsRequested);
    const MultiLevelPinkNoiseWindModel& wind   = tab.options().getMultiLevelWindModel();
    const std::size_t                   levels = wind.getLevels().size();
    bool                                shown  = false;
    const Gui::Test::NextModalDialog    answer(
        WindEditorAnswer{.window = &tab.window(), .levels = levels, .shown = &shown});
    tab.panel().windLevelSummary()->editButton().click();
    EXPECT_EQ(requests.count(), 1);
    EXPECT_TRUE(shown);
    EXPECT_EQ(wind.getLevels().size(), levels + 1);
    EXPECT_EQ(tab.panel().windLevelSummary()->summaryLabel().text(),
              WindLevelSummary::summaryText(wind));
}

TEST_F(SimulationConditionsPanelTest, ReturnClicksNoButtonOfThePanel)
{
    // Java's settings dialog has no default button, and Enter clicks none of the panel's plain
    // JButtons; a QPushButton in a QDialog is autoDefault, and a dialog without a default button
    // makes the first one in the focus chain its default ("Edit levels") when it is shown.
    Gui::Test::TestGuiServices services;
    SimulationOptions          options;
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setLaunchRodLength(7);
    QDialog      dialog;
    BindingScope scope(dialog, nullptr);
    auto* const  name  = makeChild<QLineEdit>(&dialog);
    auto* const  panel = makeChild<SimulationConditionsPanel>(&dialog, options, Variant::SIMULATION,
                                                              services.services(), scope);
    auto* const  layout = makeChild<QVBoxLayout>(&dialog);
    layout->addWidget(name);
    layout->addWidget(panel);
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    ASSERT_EQ(panel->windCards()->currentWidget(), panel->windLevelSummary());
    EXPECT_EQ(defaultButtonsOf(dialog), std::vector<std::string>());
    const QSignalSpy         requests(panel, &SimulationConditionsPanel::editWindLevelsRequested);
    const ModalDialogCounter editors;
    pressReturnInEach({name, panel->multiLevelWindButton(), &panel->isaCheckBox(),
                       panel->resetButton(), panel->saveButton(),
                       &panel->windLevelSummary()->editButton()});
    EXPECT_EQ(requests.count(), 0);
    EXPECT_EQ(editors.count(), 0);
    EXPECT_EQ(defaultButtonsOf(dialog), std::vector<std::string>());
    // Neither "Reset to default" nor "Save as default" was clicked, and the dialog stays.
    EXPECT_DOUBLE_EQ(options.getLaunchRodLength(), 7.0);
    EXPECT_DOUBLE_EQ(services.preferences().getLaunchRodLength(), 1.0);
    EXPECT_TRUE(dialog.isVisible());
    scope.release();
}

TEST_F(SimulationConditionsPanelTest, TheRodDirectionFollowsAWindLevelTheTableMoves)
{
    // OpenRocket bug 32's fix sorts the levels after the wind editor's table moved one: the rod
    // direction, launching into the wind, follows the sorted levels, as a run does (Java's model
    // keeps its order, which its panel shows).
    Gui::Test::TestGuiServices services;
    SimulationOptions          options;
    launchIntoThreeWindLevels(options);
    QWidget                         window;
    BindingScope                    scope(window, nullptr);
    const SimulationConditionsPanel panel(options, Variant::SIMULATION, services.services(), scope,
                                          nullptr);
    EXPECT_EQ(panel.rodDirection().spinBox->text(), QStringLiteral("0"));
    MultiLevelWindTableModel table(options.getMultiLevelWindModel(), services.preferences(),
                                   nullptr);
    ASSERT_TRUE(table.setData(table.index(0, MultiLevelWindTableModel::kAltitudeColumn),
                              QStringLiteral("150"), Qt::EditRole));
    EXPECT_EQ(table.cellText(0, MultiLevelWindTableModel::kAltitudeColumn), QStringLiteral("100"));
    // The wind at the launch site, below every level, is the lowest level's: now the 100 m one's.
    EXPECT_NEAR(options.getLaunchRodDirection(), std::numbers::pi / 2, 1e-9);
    EXPECT_EQ(panel.rodDirection().spinBox->text(), QStringLiteral("90"));
    // The field and its slider show what a new panel shows.
    const SimulationConditionsPanel fresh(options, Variant::SIMULATION, services.services(), scope,
                                          nullptr);
    EXPECT_EQ(panel.rodDirection().slider->value(), fresh.rodDirection().slider->value());
    EXPECT_EQ(panel.rodDirection().spinBox->text(), fresh.rodDirection().spinBox->text());
    scope.release();
}

TEST_F(SimulationConditionsPanelTest, TypingFortyTwoIntoTheTurbulenceIntensityShowsTheAnswer)
{
    ExampleTab tab;
    ASSERT_TRUE(Gui::Test::showActiveWith(tab.window(), {&tab.panel()}));
    UnitSpinBox& turbulence = *tab.panel().turbulenceIntensity().spinBox;
    // Java's keyTyped(): the text before the key and the key's character make "42" at the "2"
    // of "42" typed over the field's text, not at the "2" of "142".
    Gui::Test::typeInto(turbulence, QStringLiteral("142"));
    EXPECT_TRUE(tab.services().prompts().prompts().empty());
    Gui::Test::typeInto(turbulence, QStringLiteral("42"));
    ASSERT_EQ(tab.services().prompts().prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message =
        tab.services().prompts().prompts().front();
    EXPECT_EQ(message.kind, Gui::Test::ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("42"));
    // The double records the severity, then the lines.
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("INFORMATION"),
                           QStringLiteral("The answer to the ultimate question of life, the "
                                          "universe, and everything.")}));
}

TEST_F(SimulationConditionsPanelTest, AValidatorShowsItsMessageWhileTheFieldIsShown)
{
    ExampleTab tab;
    ASSERT_TRUE(Gui::Test::showActiveWith(tab.window(), {&tab.panel()}));
    enter(tab.panel().averageWind(), "20");
    const QLabel* popup = tab.panel().windSpeedValidator().messagePopup();
    ASSERT_NE(popup, nullptr);
    EXPECT_EQ(popup->text(), QStringLiteral("<html>Extremely high wind speed</html>"));
    EXPECT_EQ(popup->window(), &tab.window());
    tab.panel().windSpeedValidator().hideMessagePopup();
}

TEST_F(SimulationConditionsPanelTest, TheLaunchPageEditsTheLaunchPreferences)
{
    LaunchPage page;
    enter(page.panel().averageWind(), "5");
    enter(page.panel().rodLength(), "150");
    page.panel().isaCheckBox().click();
    enter(page.panel().temperature(), "30");
    enter(page.panel().windDirection(), "180");
    const Preferences& preferences = page.services().preferences();
    EXPECT_DOUBLE_EQ(preferences.getWindAverage(), 5.0);
    EXPECT_DOUBLE_EQ(preferences.getLaunchRodLength(), 1.5);
    EXPECT_FALSE(preferences.isIsaAtmosphere());
    EXPECT_DOUBLE_EQ(preferences.getLaunchTemperature(), 303.15);
    // Launching into the wind, the rod's direction is the wind's.
    EXPECT_TRUE(page.panel().intoWindCheckBox().isChecked());
    EXPECT_EQ(page.panel().rodDirection().spinBox->text(), QStringLiteral("180"));
    EXPECT_FALSE(page.panel().rodDirection().spinBox->isEnabled());
}

TEST_F(SimulationConditionsPanelTest, TheSimulationVariantNeedsSimulationOptions)
{
    Gui::Test::TestGuiServices services;
    LaunchPreferences          launch(services.preferences());
    QWidget                    window;
    BindingScope               scope(window, nullptr);
    EXPECT_THROW(
        {
            const SimulationConditionsPanel panel(launch, Variant::SIMULATION, services.services(),
                                                  scope, nullptr);
        },
        BugError);
    scope.release();
}

TEST_F(SimulationConditionsPanelTest, OptionsThatGoLeaveTheFieldsAndTheButtonsInert)
{
    const Gui::Test::TestGuiServices services;
    auto                             options = std::make_unique<SimulationOptions>();
    QWidget                          window;
    BindingScope                     scope(window, nullptr);
    const SimulationConditionsPanel panel(*options, Variant::SIMULATION, services.services(), scope,
                                          nullptr);
    const QString                   summary = panel.windLevelSummary()->summaryLabel().text();
    options.reset();
    EXPECT_FALSE(panel.windLevelSummary()->hasModel());
    panel.windLevelSummary()->refresh();
    EXPECT_EQ(panel.windLevelSummary()->summaryLabel().text(), summary);
    // Nothing reaches the options that have gone (ASan checks that nothing reads them either).
    panel.resetButton()->click();
    panel.saveButton()->click();
    panel.multiLevelWindButton()->click();
    EXPECT_FALSE(panel.rodLength().spinBox->binding()->commitText("50"));
    scope.release();
}

TEST_F(SimulationConditionsPanelTest, AReleasedScopeEndsTheFieldsWrites)
{
    ExampleTab tab;
    tab.scope().release();
    EXPECT_FALSE(tab.panel().rodLength().spinBox->binding()->commitText("50"));
    tab.panel().isaCheckBox().click();
    EXPECT_DOUBLE_EQ(tab.options().getLaunchRodLength(), 1.0);
    EXPECT_TRUE(tab.options().isIsaAtmosphere());
    // A change of the options reaches the panel no more.
    tab.options().setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_TRUE(tab.panel().averageWindButton()->isChecked());
}

TEST_F(SimulationConditionsPanelTest, ThePanelMayGoBeforeItsScope)
{
    const Gui::Test::TestGuiServices services;
    SimulationOptions                options;
    QWidget                          window;
    BindingScope                     scope(window, nullptr);
    {
        const SimulationConditionsPanel panel(options, Variant::SIMULATION, services.services(),
                                              scope, nullptr);
        EXPECT_FALSE(panel.multiLevelWindButton()->isChecked());
    }
    // The scope still holds the bindings and selections; the options' changes reach no widget.
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.getAverageWindModel().setTurbulenceIntensity(0.5);
    options.getAverageWindModel().setAverage(3);
    EXPECT_EQ(scope.bindingCount(), 15U);
    scope.release();
}

}  // namespace

}  // namespace QtRocket::Gui
