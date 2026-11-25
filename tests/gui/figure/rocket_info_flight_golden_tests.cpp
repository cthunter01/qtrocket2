// The flight block of the design view's overlay against OpenRocket's (RocketInfoFlightPins.h:
// tests/data/goldens/gui/rocket-info/flight-blocks.json, the rocket-info family's
// RocketInfoFlightProbe): for each design of OpenDesignsExitGolden and the probe's own states,
// the design as loaded (the configuration's name, every simulation's status and maxima), what
// BackgroundSimulation makes of it when it is opened with the estimate on (which simulations it
// would run, with AutoRunSimulations off and on with the Simulations tab selected; what the
// overlay shows; calculating or not), and what RocketInfo then draws of the block: its texts, in
// which font and colour, on which row and in which column. Nothing runs: the runs wait in the
// tests' executor (no value of a fresh flight is pinned; plan 11.6).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QRect>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/BackgroundSimulation.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusText.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "RocketInfoFlightPins.h"
#include "ScopedApplicationPalette.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenResaves.h"
#include "motor/TestMotorDatabase.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FlightPinShown;
using Gui::Test::FlightPinSimulation;
using Gui::Test::FlightPinText;
using Gui::Test::ManualSimulationExecutor;
using Gui::Test::RocketInfoFlightPin;
using QtRocket::Test::DesignFileEnvironment;

/// The visible rectangle the probe painted into.
constexpr QRect kVisible{0, 0, 1000, 700};
/// Rows and columns are computed from the same numbers in Java (in float) and here.
constexpr double kRowTolerance    = 1e-6;
constexpr double kColumnTolerance = 1e-9;

/// A scenario's design, loaded as the probe loaded it (the six presets of the example designs;
/// the bundled motors, or the seven of TestRockets for the test rockets' re-saves; motors named by
/// their common name), in the probe's units, with the configuration it selected, shown in a
/// RocketFigure with its DesignPhysics.
class FlightScenario
{
public:
    explicit FlightScenario(const RocketInfoFlightPin& pin)
      : m_environment(DesignFileEnvironment::Presets::EXAMPLES,
                      pin.kind == "testrocket" ? QtRocket::Test::testRocketMotors()
                                               : QtRocket::Test::bundledMotorDatabase())
    {
        if (pin.imperial)
        {
            m_units.emplace();
            UnitGroup::setDefaultImperialUnits();
        }
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir().parent_path() / std::filesystem::path(pin.file));
        if (!loaded.has_value())
        {
            ADD_FAILURE() << "cannot load " << pin.file << ": " << loaded.error().message;
            m_document = OpenRocketDocumentFactory::createNewRocket();
        }
        else
        {
            m_document = std::move(loaded->document);
        }
        if (pin.selectConfigurationOf.has_value() &&
            *pin.selectConfigurationOf < m_document->getSimulationCount())
        {
            m_document->getRocket().setSelectedConfiguration(
                m_document->getSimulation(*pin.selectConfigurationOf)->getFlightConfigurationId());
        }
        m_figure =
            std::make_unique<RocketFigure>(m_document->getRocket(), preferences(), 96.0, nullptr);
        m_physics = std::make_unique<DesignPhysics>(*m_figure, preferences());
    }

    ~FlightScenario()
    {
        m_physics.reset();
        m_figure.reset();
    }
    FlightScenario(const FlightScenario&)            = delete;
    FlightScenario& operator=(const FlightScenario&) = delete;
    FlightScenario(FlightScenario&&)                 = delete;
    FlightScenario& operator=(FlightScenario&&)      = delete;

    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return *m_document; }
    [[nodiscard]] DesignPhysics&       physics() const noexcept { return *m_physics; }
    [[nodiscard]] RocketInfo&          info() const noexcept { return m_physics->info(); }
    [[nodiscard]] InMemoryPreferences& preferences() noexcept
    {
        return m_environment.preferences();
    }

private:
    std::optional<QtRocket::Test::DefaultUnitsGuard> m_units;
    DesignFileEnvironment                            m_environment;
    std::unique_ptr<OpenRocketDocument>              m_document;
    std::unique_ptr<RocketFigure>                    m_figure;
    std::unique_ptr<DesignPhysics>                   m_physics;
};

/// Whether @p actual is @p expected, NaN included (stored values round-trip exactly).
void expectSameValue(double actual, double expected, std::string_view what)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << what << ": " << actual;
        return;
    }
    EXPECT_EQ(actual, expected) << what;
}

/// Compares the maxima of @p summary with @p maxima (altitude, velocity, Mach, acceleration).
void expectMaxima(const RocketInfo::FlightSummary& summary, const std::array<double, 4>& maxima)
{
    expectSameValue(summary.maxAltitude, maxima.at(0), "apogee");
    expectSameValue(summary.maxVelocity, maxima.at(1), "maximum velocity");
    expectSameValue(summary.maxMachNumber, maxima.at(2), "maximum Mach number");
    expectSameValue(summary.maxAcceleration, maxima.at(3), "maximum acceleration");
}

/// Whether every number of @p summary is NaN (FlightData.NaN_DATA).
[[nodiscard]] bool isNotAvailable(const RocketInfo::FlightSummary& summary)
{
    return std::isnan(summary.maxAltitude) && std::isnan(summary.maxVelocity) &&
           std::isnan(summary.maxMachNumber) && std::isnan(summary.maxAcceleration);
}

/// Compares the simulation @p simulation of @p document with @p pin.
void expectSimulationAsJava(OpenRocketDocument& document, Simulation& simulation,
                            const FlightPinSimulation& pin)
{
    SCOPED_TRACE(pin.name);
    EXPECT_EQ(simulation.getName(), pin.name);
    EXPECT_EQ(std::string(name(simulation.getStatus())), pin.status);
    EXPECT_EQ(simulation.hasSummaryData(), pin.hasData);
    const Rocket& rocket = document.getRocket();
    EXPECT_EQ(simulation.getFlightConfigurationId() == rocket.getSelectedConfiguration().getId(),
              pin.selectedConfiguration);
    EXPECT_EQ(rocket.getFlightConfiguration(simulation.getFlightConfigurationId()).hasMotors(),
              pin.configurationHasMotors);
    if (pin.maxima.has_value() && simulation.getSimulatedData() != nullptr)
    {
        expectMaxima(RocketInfo::FlightSummary::of(*simulation.getSimulatedData()), *pin.maxima);
    }
}

/// The indices in @p document of @p simulations.
[[nodiscard]] std::vector<std::size_t> indicesOf(
    const OpenRocketDocument& document, const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    std::vector<std::size_t> indices;
    indices.reserve(simulations.size());
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        indices.push_back(
            document.getSimulationIndex(*simulation).value_or(document.getSimulationCount()));
    }
    return indices;
}

/// The simulations @p background runs or will run, in order.
[[nodiscard]] std::vector<std::size_t> scheduledOf(const BackgroundSimulation& background,
                                                   const OpenRocketDocument&   document)
{
    std::vector<std::shared_ptr<Simulation>> simulations;
    if (background.runningSimulation() != nullptr)
    {
        simulations.push_back(background.runningSimulation());
    }
    const std::vector<std::shared_ptr<Simulation>> chain = background.chain();
    simulations.insert(simulations.end(), chain.begin(), chain.end());
    return indicesOf(document, simulations);
}

/// Compares the flight data of @p info with @p pin's: none, "N/A" or the shown simulation's
/// maxima.
void expectFlightDataAsJava(const RocketInfo& info, const RocketInfoFlightPin& pin)
{
    const std::optional<RocketInfo::FlightSummary>& data = info.flightData();
    EXPECT_EQ(data.has_value(), pin.shown != FlightPinShown::NONE);
    if (pin.shown == FlightPinShown::NAN_DATA)
    {
        EXPECT_TRUE(isNotAvailable(data.value_or(RocketInfo::FlightSummary{})));
    }
    if (pin.shown == FlightPinShown::DATA)
    {
        const std::optional<std::array<double, 4>> maxima =
            pin.shownSimulation.has_value() && *pin.shownSimulation < pin.simulations.size()
                ? pin.simulations.at(*pin.shownSimulation).maxima
                : std::nullopt;
        ASSERT_TRUE(maxima.has_value());
        expectMaxima(data.value_or(RocketInfo::FlightSummary{}),
                     maxima.value_or(std::array<double, 4>{}));
    }
}

/// Compares the status line of @p info with @p pin's: the shown simulation's text when it is
/// ABORTED, else none.
void expectStatusAsJava(const RocketInfo& info, const RocketInfoFlightPin& pin)
{
    const bool aborted = pin.status.value_or(std::string()) == "ABORTED";
    EXPECT_EQ(info.abortedStatus().has_value(), aborted);
    EXPECT_EQ(QtStrings::toUtf8(info.abortedStatus().value_or(QString())),
              aborted ? pin.statusText.value_or(std::string()) : std::string());
}

/// Compares the state of @p info with @p pin's overlay: the flight data, calculating, the
/// configuration's name, the status line.
void expectOverlayAsJava(const RocketInfo& info, const RocketInfoFlightPin& pin)
{
    EXPECT_EQ(info.isCalculatingData(), pin.calculating);
    EXPECT_EQ(QtStrings::toUtf8(info.configurationName()), pin.configurationName);
    expectFlightDataAsJava(info, pin);
    expectStatusAsJava(info, pin);
}

/// The theme colour of @p key's name.
[[nodiscard]] std::optional<ThemeColors::Key> colourKeyOf(std::string_view key)
{
    if (key == "FLIGHTDATA_TEXT_ACTIVE")
    {
        return ThemeColors::Key::FLIGHTDATA_TEXT_ACTIVE;
    }
    if (key == "FLIGHTDATA_TEXT_INACTIVE")
    {
        return ThemeColors::Key::FLIGHTDATA_TEXT_INACTIVE;
    }
    if (key == "ERROR")
    {
        return ThemeColors::Key::ERROR;
    }
    if (key == "TEXT")
    {
        return ThemeColors::Key::TEXT;
    }
    ADD_FAILURE() << "no theme colour " << key;
    return std::nullopt;
}

/// The overlay's frame for the probe's visible rectangle: the left margin, the bottom row, the
/// line.
struct BottomFrame
{
    double x1;
    double y2;
    double line;
};

[[nodiscard]] BottomFrame bottomFrameOf(const Preferences& preferences)
{
    const RocketInfo::Fonts fonts = RocketInfo::fonts(QApplication::font(), preferences);
    const QFontMetricsF     metrics(fonts.regular);
    return {.x1   = kVisible.x() + RocketInfo::kMargin,
            .y2   = kVisible.y() + kVisible.height() - RocketInfo::kMargin,
            .line = metrics.lineSpacing() + metrics.descent()};
}

/// The items of @p info's layout that are its flight block (those a layout without flight data
/// and without calculating does not have: they come last).
[[nodiscard]] std::vector<RocketInfo::Item> flightItemsOf(RocketInfo& info)
{
    const std::optional<RocketInfo::FlightSummary> data        = info.flightData();
    const bool                                     calculating = info.isCalculatingData();
    info.setFlightData(std::nullopt);
    info.setCalculatingData(false);
    const std::size_t baseline = info.layout(kVisible, QApplication::font(), nullptr).size();
    info.setFlightData(data);
    info.setCalculatingData(calculating);
    std::vector<RocketInfo::Item> items = info.layout(kVisible, QApplication::font(), nullptr);
    items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(baseline));
    return items;
}

/// The values' column: 10 pixels right of the widest label of the block's @p items.
[[nodiscard]] std::optional<double> valueColumnOf(const std::vector<RocketInfo::Item>& items,
                                                  const RocketInfoFlightPin&           pin,
                                                  const BottomFrame&                   frame)
{
    double            widest = 0;
    std::size_t       labels = 0;
    const std::size_t count  = std::min(items.size(), pin.texts.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        // The labels: the left column's texts of the block's four rows, in the regular font.
        const FlightPinText& text = pin.texts.at(index);
        if (text.leftColumn && text.font == "REGULAR" && text.text.ends_with(": "))
        {
            widest = std::max(widest, items.at(index).width);
            ++labels;
        }
    }
    if (labels == 0)
    {
        return std::nullopt;
    }
    return frame.x1 + widest + RocketInfo::kFlightValueGap;
}

/// Compares the text, the font and the colour of the flight item @p item with @p pin.
void expectFlightTextAsJava(const RocketInfo::Item& item, const FlightPinText& pin)
{
    EXPECT_EQ(item.kind, RocketInfo::Item::Kind::TEXT);
    EXPECT_EQ(QtStrings::toUtf8(item.text), pin.text);
    const bool bold = pin.font == "BOLD";
    EXPECT_EQ(item.font,
              bold ? RocketInfo::Item::FontRole::BOLD : RocketInfo::Item::FontRole::REGULAR);
    const QColor expected(pin.rgba.at(0), pin.rgba.at(1), pin.rgba.at(2), pin.rgba.at(3));
    EXPECT_EQ(item.colour, expected);
    const std::optional<ThemeColors::Key> key = colourKeyOf(pin.colour);
    EXPECT_EQ(key.has_value() ? ThemeColors::themeColor(*key, ThemeColors::Theme::LIGHT) : QColor(),
              expected);
}

/// Compares the row and the column of the flight item @p item with @p pin.
void expectFlightPlaceAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                             const BottomFrame& frame, std::optional<double> valueColumn)
{
    EXPECT_NEAR((frame.y2 - item.position.y()) / frame.line, pin.rowFromBottom, kRowTolerance);
    EXPECT_TRUE(pin.leftColumn || valueColumn.has_value());
    const double column = pin.leftColumn ? frame.x1 : valueColumn.value_or(0.0);
    EXPECT_NEAR(item.position.x(), column, kColumnTolerance);
}

/// Compares the flight item @p item with @p pin.
void expectFlightItemAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                            const BottomFrame& frame, std::optional<double> valueColumn)
{
    SCOPED_TRACE(pin.text);
    expectFlightTextAsJava(item, pin);
    expectFlightPlaceAsJava(item, pin, frame, valueColumn);
}

/// Compares what @p info draws of the flight block with @p pin's texts.
void expectFlightBlockAsJava(RocketInfo& info, const Preferences& preferences,
                             const RocketInfoFlightPin& pin)
{
    const std::vector<RocketInfo::Item> items = flightItemsOf(info);
    ASSERT_EQ(items.size(), pin.texts.size());
    const BottomFrame           frame       = bottomFrameOf(preferences);
    const std::optional<double> valueColumn = valueColumnOf(items, pin, frame);
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        SCOPED_TRACE(index);
        expectFlightItemAsJava(items.at(index), pin.texts.at(index), frame, valueColumn);
    }
}

/// Gives @p info @p pin's overlay state for @p document (as the probe set it on RocketInfo).
void setPinnedState(RocketInfo& info, const OpenRocketDocument& document,
                    const RocketInfoFlightPin& pin)
{
    std::shared_ptr<Simulation> shown;
    if (pin.shownSimulation.has_value() && *pin.shownSimulation < document.getSimulationCount())
    {
        shown = document.getSimulation(*pin.shownSimulation);
    }
    switch (pin.shown)
    {
        case FlightPinShown::NONE:
            info.setFlightData(std::nullopt);
            break;
        case FlightPinShown::NAN_DATA:
            info.setFlightData(RocketInfo::FlightSummary::notAvailable());
            break;
        case FlightPinShown::DATA:
            if (shown != nullptr && shown->getSimulatedData() != nullptr)
            {
                info.setFlightData(RocketInfo::FlightSummary::of(*shown->getSimulatedData()));
            }
            break;
    }
    info.setCalculatingData(pin.calculating);
    info.setAbortedStatus(shown != nullptr && shown->getStatus() == Simulation::Status::ABORTED
                              ? std::optional(SimulationStatusText::plainDescription(*shown))
                              : std::nullopt);
}

/// The scenarios whose overlay is the estimate's (BackgroundSimulationGolden).
[[nodiscard]] std::vector<RocketInfoFlightPin> estimatedScenarios()
{
    std::vector<RocketInfoFlightPin> scenarios;
    std::ranges::copy_if(Gui::Test::rocketInfoFlightPins().scenarios, std::back_inserter(scenarios),
                         [](const RocketInfoFlightPin& pin) { return pin.estimated; });
    return scenarios;
}

/// Compares every simulation of @p document with @p pin's.
void expectSimulationsAsJava(OpenRocketDocument& document, const RocketInfoFlightPin& pin)
{
    ASSERT_EQ(document.getSimulationCount(), pin.simulations.size());
    for (std::size_t index = 0; index < pin.simulations.size(); ++index)
    {
        expectSimulationAsJava(document, *document.getSimulation(index), pin.simulations.at(index));
    }
}

/// The designs of the suite (not the probe's own states) whose estimate runs a simulation.
[[nodiscard]] std::vector<std::string> designsWhoseEstimateRuns()
{
    std::vector<std::string> running;
    for (const RocketInfoFlightPin& pin : Gui::Test::rocketInfoFlightPins().scenarios)
    {
        if (!pin.name.starts_with("special:") && !pin.runsCurrent.empty())
        {
            running.push_back(pin.name);
        }
    }
    return running;
}

class RocketInfoFlightGolden : public Gui::Test::GuiTest,
                               public ::testing::WithParamInterface<RocketInfoFlightPin>
{ };

class BackgroundSimulationGolden : public Gui::Test::GuiTest,
                                   public ::testing::WithParamInterface<RocketInfoFlightPin>
{ };

TEST(RocketInfoFlightPinsGolden, AreReadAndComeFromTheGoldensOpenRocket)
{
    const Gui::Test::RocketInfoFlightPins& pins = Gui::Test::rocketInfoFlightPins();
    EXPECT_EQ(pins.problem, "");
    EXPECT_EQ(pins.schema, Gui::Test::kRocketInfoFlightSchemaVersion);
    EXPECT_EQ(pins.openRocketCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    // The 66 designs of OpenDesignsExitGolden and the probe's 7 states.
    EXPECT_EQ(pins.scenarios.size(), 73U);
    EXPECT_EQ(estimatedScenarios().size(), 68U);
}

TEST(RocketInfoFlightPinsGolden, FourOpenDesignsRunTheirEstimate)
{
    // Plan 11.6 E5: 62 of the 66 designs show their stored estimate at once; four test rockets'
    // re-saves (beta, Falcon 9 Heavy, the multi-stage event rocket, the cluster pods) have a
    // simulation of the selected configuration that is not up to date, which the estimate runs.
    const std::vector<std::string> running = designsWhoseEstimateRuns();
    EXPECT_EQ(running, (std::vector<std::string>{"testrocket_beta", "testrocket_falcon_9_heavy",
                                                 "testrocket_multi_stage_event_test_rocket",
                                                 "testrocket_cluster_pods"}));
}

TEST_P(RocketInfoFlightGolden, LoadsTheDesignAsOpenRocket)
{
    const RocketInfoFlightPin& pin = GetParam();
    const FlightScenario       scenario(pin);
    OpenRocketDocument&        document = scenario.document();
    EXPECT_EQ(QtStrings::toUtf8(scenario.info().configurationName()), pin.configurationName);
    EXPECT_EQ(document.getRocket().getSelectedConfiguration().hasMotors(), pin.hasMotors);
    expectSimulationsAsJava(document, pin);
}

TEST_P(RocketInfoFlightGolden, PaintsTheFlightBlockAsOpenRocket)
{
    // The overlay in the state the probe gave OpenRocket's RocketInfo.
    const RocketInfoFlightPin&                pin = GetParam();
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    FlightScenario                            scenario(pin);
    setPinnedState(scenario.info(), scenario.document(), pin);
    expectOverlayAsJava(scenario.info(), pin);
    expectFlightBlockAsJava(scenario.info(), scenario.preferences(), pin);
}

TEST_P(BackgroundSimulationGolden, EstimatesAsOpenRocket)
{
    // RocketPanel.updateExtras() for the design just opened, with the estimate on and
    // AutoRunSimulations off: the runs it asks for (they wait), the overlay, and what it draws.
    const RocketInfoFlightPin&                pin = GetParam();
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    FlightScenario                            scenario(pin);
    ManualSimulationExecutor                  executor(ManualSimulationExecutor::Mode::NEVER);
    BackgroundSimulation                      background(scenario.physics(), scenario.document(),
                                                         scenario.preferences());
    background.setExecutor(&executor, nullptr);
    EXPECT_EQ(scheduledOf(background, scenario.document()), pin.runsCurrent);
    EXPECT_EQ(background.isCalculating(), pin.calculating);
    expectOverlayAsJava(scenario.info(), pin);
    expectFlightBlockAsJava(scenario.info(), scenario.preferences(), pin);
}

TEST_P(BackgroundSimulationGolden, RunsEverySimulationWithTheAutoRunOnTheSimulationsTab)
{
    // updateSims(true): with AutoRunSimulations on and the Simulations tab selected, every
    // simulation that is not up to date and has motors.
    const RocketInfoFlightPin& pin = GetParam();
    FlightScenario             scenario(pin);
    scenario.preferences().setAutoRunSimulations(true);
    ManualSimulationExecutor executor(ManualSimulationExecutor::Mode::NEVER);
    BackgroundSimulation     background(scenario.physics(), scenario.document(),
                                        scenario.preferences());
    background.setSimulationsTabSelected(true);
    background.setExecutor(&executor, nullptr);
    EXPECT_EQ(scheduledOf(background, scenario.document()), pin.runsAll);
}

/// The name of a scenario's tests.
[[nodiscard]] std::string scenarioTestName(
    const ::testing::TestParamInfo<RocketInfoFlightPin>& param)
{
    return param.param.testName;
}

INSTANTIATE_TEST_SUITE_P(Scenarios, RocketInfoFlightGolden,
                         ::testing::ValuesIn(Gui::Test::rocketInfoFlightPins().scenarios),
                         scenarioTestName);
INSTANTIATE_TEST_SUITE_P(Scenarios, BackgroundSimulationGolden,
                         ::testing::ValuesIn(estimatedScenarios()), scenarioTestName);

}  // namespace

}  // namespace QtRocket::Gui
