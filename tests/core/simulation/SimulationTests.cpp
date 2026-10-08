#include "QtRocket/simulation/Simulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationOptionsSupport.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationExtension;
using QtRocket::BugError;
using QtRocket::CloneableSimulationListener;
using QtRocket::Color;
using QtRocket::ErrorCode;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::InMemoryPreferences;
using QtRocket::LandingDispersionSettings;
using QtRocket::LineStyle;
using QtRocket::ModId;
using QtRocket::MotorClusterState;
using QtRocket::PlotAppearance;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::Test::bugText;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::newBranch;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using Status = Simulation::Status;

// A simulation stays where it is: the conditions of a run and the options' listener point at
// it. Copies are made by the copying methods.
static_assert(!std::is_copy_constructible_v<Simulation>);
static_assert(!std::is_move_constructible_v<Simulation>);
static_assert(!std::is_copy_assignable_v<Simulation>);
static_assert(!std::is_move_assignable_v<Simulation>);

// ------------------------------------------------------------------- SimulationTest.java
//
// The cases that need neither a run of the engine nor a document. The cases that run a
// simulation follow further down (SimulationRunTest).

/// SimulationTest.EPSILON
constexpr double kEpsilon = 0.0001;

/// SimulationTest's fixture (setUpSim()). Java's `new Simulation(rocket)` takes the default
/// options of the application preferences; here those are an empty preference store.
class SimulationTest : public ::testing::Test
{
protected:
    SimulationTest() : m_simulation(*m_alpha.rocket, m_preferences)
    {
        m_simulation.setFlightConfigurationId(testFcid(0));
        m_simulation.getOptions().setIsaAtmosphere(true);
        m_simulation.getOptions().setTimeStep(0.05);
    }

    [[nodiscard]] Rocket&     rocket() const { return *m_alpha.rocket; }
    [[nodiscard]] Simulation& simulation() { return m_simulation; }

private:
    InMemoryPreferences m_preferences;
    TestEstesAlphaIII   m_alpha;
    Simulation          m_simulation;
};

// SimulationTest.testBasicSimulationCreation
TEST_F(SimulationTest, BasicSimulationCreation)
{
    EXPECT_EQ(&rocket(), &simulation().getRocket());
    EXPECT_EQ(Status::NOT_SIMULATED, simulation().getStatus());
    EXPECT_EQ("", simulation().getName());
}

// SimulationTest.testSimulationName
TEST_F(SimulationTest, SimulationName)
{
    const std::string testName = "Test Flight #1";
    simulation().setName(testName);
    EXPECT_EQ(testName, simulation().getName());

    // Java: setName(null), which yields an empty name.
    simulation().setName("");
    EXPECT_EQ("", simulation().getName());
}

// SimulationTest.testSimulationCopy
TEST_F(SimulationTest, SimulationCopy)
{
    simulation().setName("Original Sim");
    simulation().getOptions().setLaunchRodLength(2.0);
    simulation().getOptions().setLaunchRodAngle(45.0);

    const std::unique_ptr<Simulation> copy = simulation().copy();

    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(simulation().getName(), copy->getName());
    EXPECT_NEAR(simulation().getOptions().getLaunchRodLength(),
                copy->getOptions().getLaunchRodLength(), kEpsilon);
    EXPECT_NEAR(simulation().getOptions().getLaunchRodAngle(),
                copy->getOptions().getLaunchRodAngle(), kEpsilon);
    EXPECT_EQ(Status::NOT_SIMULATED, copy->getStatus());

    // Verify copy is independent
    copy->setName("Modified Copy");
    EXPECT_NE(simulation().getName(), copy->getName());
}

/// What Java's test builds with MonteCarloSettings.builder(): 250 runs, the seed @p seed and a
/// uniform uncertainty of the wind speed of 1.5, as a file states such settings. (Java's
/// settings also have a number of threads, 2, which a file does not store.)
[[nodiscard]] LandingDispersionSettings landingDispersionSettings(std::string_view seed)
{
    return LandingDispersionSettings(
        {{"runs", "250"}, {"seed", std::string(seed)}},
        {{{"parameter", "windspeed"}, {"distribution", "uniform"}, {"spread", "1.5"}}});
}

// SimulationTest.testLandingDispersionSettingsAreOptionalAndCopied, with the settings as a
// design file states them (LandingDispersionSettings) in place of Java's MonteCarloSettings,
// which the Monte Carlo milestone ports.
TEST_F(SimulationTest, LandingDispersionSettingsAreOptionalAndCopied)
{
    EXPECT_FALSE(simulation().getLandingDispersionSettings().has_value());

    const LandingDispersionSettings settings = landingDispersionSettings("12345");
    simulation().setLandingDispersionSettings(settings);

    // Java: assertSame; the settings are a value here.
    EXPECT_TRUE(simulation().getLandingDispersionSettings() == settings);
    EXPECT_TRUE(simulation().copy()->getLandingDispersionSettings() == settings);
    EXPECT_TRUE(simulation().clone()->getLandingDispersionSettings() == settings);
    const std::shared_ptr<Rocket> rocketCopy = rocket().copyRocketWithOriginalId();
    EXPECT_TRUE(simulation().duplicateSimulation(*rocketCopy)->getLandingDispersionSettings() ==
                settings);
    EXPECT_TRUE(simulation().duplicateForIndependentSimulation()->getLandingDispersionSettings() ==
                settings);

    Simulation loaded(rocket());
    loaded.loadFrom(simulation());
    EXPECT_TRUE(loaded.getLandingDispersionSettings() == settings);

    const std::unique_ptr<Simulation> different = simulation().copy();
    different->setLandingDispersionSettings(landingDispersionSettings("54321"));
    EXPECT_FALSE(simulation() == *different);

    simulation().setLandingDispersionSettings(std::nullopt);
    EXPECT_FALSE(simulation().getLandingDispersionSettings().has_value());
}

// Not in SimulationTest.java: the copy of the undo history has the settings too, a copy with
// the same settings equals the simulation, and the setter tells of a change and of nothing
// else (Java: Objects.equals() before fireChangeEvent()).
TEST_F(SimulationTest, LandingDispersionSettingsChangeTheSimulationOnlyWhenTheyDiffer)
{
    const ChangeCounter changes(simulation().changed());
    simulation().setLandingDispersionSettings(std::nullopt);
    EXPECT_EQ(changes.count(), 0);

    simulation().setLandingDispersionSettings(landingDispersionSettings("12345"));
    EXPECT_EQ(changes.count(), 1);
    simulation().setLandingDispersionSettings(landingDispersionSettings("12345"));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_TRUE(simulation().cloneForUndo()->getLandingDispersionSettings() ==
                landingDispersionSettings("12345"));
    EXPECT_TRUE(simulation() == *simulation().copy());

    simulation().setLandingDispersionSettings(landingDispersionSettings("54321"));
    EXPECT_EQ(changes.count(), 2);
    simulation().setLandingDispersionSettings(std::nullopt);
    EXPECT_EQ(changes.count(), 3);

    // loadFrom() takes the settings without an event of its own, as in Java; a simulation
    // without settings takes them away.
    Simulation source(rocket());
    simulation().setLandingDispersionSettings(landingDispersionSettings("1"));
    simulation().loadFrom(source);
    EXPECT_FALSE(simulation().getLandingDispersionSettings().has_value());
}

// SimulationTest.testConfigurationManagement
TEST_F(SimulationTest, ConfigurationManagement)
{
    const FlightConfigurationId newId;
    simulation().setFlightConfigurationId(newId);
    EXPECT_EQ(newId, simulation().getFlightConfigurationId());

    const FlightConfiguration& config = simulation().getActiveConfiguration();
    EXPECT_EQ(newId, config.getFlightConfigurationId());
}

// SimulationTest.testOptionsModification
TEST_F(SimulationTest, OptionsModification)
{
    SimulationOptions& options = simulation().getOptions();

    // Modify some options
    const double rodLength = 2.0;
    const double rodAngle  = 4 * std::numbers::pi / 13;

    options.setLaunchRodLength(rodLength);
    options.setLaunchRodAngle(rodAngle);

    // Verify changes are reflected
    EXPECT_NEAR(rodLength, options.getLaunchRodLength(), kEpsilon);
    EXPECT_NEAR(rodAngle, options.getLaunchRodAngle(), kEpsilon);

    // Status should be outdated after options change
    EXPECT_EQ(Status::NOT_SIMULATED, simulation().getStatus());
}

// SimulationTest.testCopySimulationOptionsFromCopiesStepperMethodChoice
TEST_F(SimulationTest, CopySimulationOptionsFromCopiesStepperMethodChoice)
{
    InMemoryPreferences preferences;
    Simulation          target(rocket(), preferences);
    target.setFlightConfigurationId(testFcid(0));

    simulation().getOptions().setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    target.copySimulationOptionsFrom(simulation().getOptions());

    EXPECT_EQ(SimulationStepperMethod::RK6, target.getOptions().getSimulationStepperMethodChoice())
        << "Bulk option copies must preserve the selected simulation stepper";
}

// ============================================================================ test support
//
// The expectations below are those of probes/tier8b-status/SimulationProbe.java and
// StatusBranchProbe.java (package info.openrocket.core.document), quoted where they are used.

/// An extension that records its initialize() calls in a log the copies share, and fails when
/// its configuration says so.
class Ext final : public AbstractSimulationExtension
{
public:
    explicit Ext(std::shared_ptr<std::vector<std::string>> log = nullptr, std::string label = "ext")
      : AbstractSimulationExtension("test.Ext"), m_log(std::move(log)), m_label(std::move(label))
    {
    }

    void initialize(SimulationConditions& conditions) override
    {
        if (m_log != nullptr)
        {
            m_log->push_back(m_label);
        }
        // What an extension does: it changes the conditions.
        conditions.setLaunchVelocity(QtRocket::Coordinate{0, 0, 1});
        if (m_config.getBoolean("fail", false))
        {
            throw SimulationException("extension failed");
        }
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<Ext>(*this);
    }

    void set(std::string_view key, std::string_view value)
    {
        m_config.put(key, std::string{value});
    }
    void setFailing() { m_config.put("fail", true); }

private:
    std::shared_ptr<std::vector<std::string>> m_log;
    std::string                               m_label;
};

/// Flight data with one branch per element of @p causes; a branch whose cause is given has a
/// SIM_ABORT event for it (SimulationProbe.abortedData()).
[[nodiscard]] std::shared_ptr<FlightData> abortedData(
    std::initializer_list<std::optional<SimulationAbort::Cause>> causes)
{
    std::vector<std::shared_ptr<FlightDataBranch>> branches;
    for (const std::optional<SimulationAbort::Cause>& cause : causes)
    {
        const std::shared_ptr<FlightDataBranch> branch =
            newBranch("b" + std::to_string(branches.size()));
        if (cause.has_value())
        {
            branch->addEvent(
                FlightEvent(FlightEvent::Type::SIM_ABORT, 1.0, nullptr, SimulationAbort(*cause)));
        }
        branches.push_back(branch);
    }
    return std::make_shared<FlightData>(
        std::span<const std::shared_ptr<FlightDataBranch>>(branches));
}

/// An Estes Alpha III whose selected configuration is the first test configuration, which has a
/// motor (the probes' rocket()).
struct SelectedAlpha
{
    TestEstesAlphaIII alpha;

    SelectedAlpha() { alpha.rocket->setSelectedConfiguration(testFcid(0)); }

    [[nodiscard]] Rocket& rocket() const { return *alpha.rocket; }
};

/// Branch 0 of the simulated data of @p simulation, which a test has just run.
[[nodiscard]] const FlightDataBranch& simulatedBranch(const Simulation& simulation)
{
    return simulatedData(simulation).getBranch(0);
}

/// A simulation as a file describes it (the probes' loaded()): named "Loaded", with new options,
/// without extensions.
[[nodiscard]] std::unique_ptr<Simulation> loaded(Rocket& rocket, Status status,
                                                 std::shared_ptr<FlightData> data = nullptr)
{
    return std::make_unique<Simulation>(nullptr, rocket, status, "Loaded", SimulationOptions(),
                                        std::vector<std::shared_ptr<SimulationExtension>>{},
                                        std::move(data));
}

/// The statuses whose data counts as up to date.
constexpr std::array<Status, 3> kUpToDate{Status::UPTODATE, Status::LOADED, Status::EXTERNAL};

// ============================================================================ the status enum

TEST(SimulationStatusEnum, TheNamesAndTextsAreOpenRockets)
{
    // "== status texts": name() | toString() | getDescription(a simulation that did not abort)
    EXPECT_EQ(name(Status::UPTODATE), "UPTODATE");
    EXPECT_EQ(displayName(Status::UPTODATE), "Up To Date");
    EXPECT_EQ(description(Status::UPTODATE), "<i>Up to date</i>");
    EXPECT_EQ(name(Status::LOADED), "LOADED");
    EXPECT_EQ(displayName(Status::LOADED), "Loaded From File");
    EXPECT_EQ(description(Status::LOADED), "<i>Loaded from file</i>");
    EXPECT_EQ(name(Status::OUTDATED), "OUTDATED");
    EXPECT_EQ(displayName(Status::OUTDATED), "Out of Date");
    EXPECT_EQ(description(Status::OUTDATED), "<i>Out of date</i>");
    EXPECT_EQ(name(Status::EXTERNAL), "EXTERNAL");
    EXPECT_EQ(displayName(Status::EXTERNAL), "Imported External Data");
    EXPECT_EQ(description(Status::EXTERNAL), "<i>Imported data</i>");
    EXPECT_EQ(name(Status::NOT_SIMULATED), "NOT_SIMULATED");
    EXPECT_EQ(displayName(Status::NOT_SIMULATED), "Not Simulated Yet");
    EXPECT_EQ(description(Status::NOT_SIMULATED),
              "<i>Not simulated yet</i> <br>Click <i><b>Run simulations</b></i> to simulate.");
    EXPECT_EQ(name(Status::CANT_RUN), "CANT_RUN");
    EXPECT_EQ(displayName(Status::CANT_RUN), "Simulation Can't Be Run");
    EXPECT_EQ(description(Status::CANT_RUN), "<i>Errors in simulation prevent running</i>");
    EXPECT_EQ(name(Status::ABORTED), "ABORTED");
    EXPECT_EQ(displayName(Status::ABORTED), "<i><b>ABORTED</b></i>");
    EXPECT_EQ(description(Status::ABORTED), "<i><b>Simulation Aborted</b></i>");
}

TEST(SimulationStatusEnum, TheOrderIsJavas)
{
    EXPECT_EQ(
        Simulation::kAllStatuses,
        (std::array<Status, 7>{Status::UPTODATE, Status::LOADED, Status::OUTDATED, Status::EXTERNAL,
                               Status::NOT_SIMULATED, Status::CANT_RUN, Status::ABORTED}));
    EXPECT_EQ(static_cast<int>(Status::UPTODATE), 0);
    EXPECT_EQ(static_cast<int>(Status::ABORTED), 6);
}

TEST(SimulationStatusEnum, UpToDateAreTheStatusesWithCurrentData)
{
    EXPECT_TRUE(Simulation::isStatusUpToDate(Status::UPTODATE));
    EXPECT_TRUE(Simulation::isStatusUpToDate(Status::LOADED));
    EXPECT_TRUE(Simulation::isStatusUpToDate(Status::EXTERNAL));
    EXPECT_FALSE(Simulation::isStatusUpToDate(Status::OUTDATED));
    EXPECT_FALSE(Simulation::isStatusUpToDate(Status::NOT_SIMULATED));
    EXPECT_FALSE(Simulation::isStatusUpToDate(Status::CANT_RUN));
    EXPECT_FALSE(Simulation::isStatusUpToDate(Status::ABORTED));
}

// ============================================================================ a new simulation

TEST(Simulation, ANewSimulationTakesTheSelectedConfigurationAndHasNothingSimulated)
{
    // "fcid default true error false selected same true", "status CANT_RUN name [] data null
    // conditions null description null warnings null", "hasSimulationData false hasSummaryData
    // false hasErrors false extensions 0 appearances 0"
    TestEstesAlphaIII alpha;
    Simulation        sim(*alpha.rocket);
    EXPECT_TRUE(sim.getFlightConfigurationId().isDefaultId());
    EXPECT_EQ(sim.getId(), sim.getFlightConfigurationId());
    EXPECT_EQ(sim.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(sim.getStatus(), Status::CANT_RUN) << "the default configuration has no motor";
    EXPECT_EQ(sim.getName(), "");
    EXPECT_EQ(sim.getSimulatedData(), nullptr);
    EXPECT_EQ(sim.getSimulatedConditions(), nullptr);
    EXPECT_FALSE(sim.getSimulatedConfigurationDescription().has_value());
    EXPECT_EQ(sim.getSimulatedWarnings(), nullptr);
    EXPECT_EQ(sim.getSimulatedConfigurationModId(), ModId::invalid());
    EXPECT_FALSE(sim.hasSimulationData());
    EXPECT_FALSE(sim.hasSummaryData());
    EXPECT_FALSE(sim.hasErrors());
    EXPECT_TRUE(sim.getSimulationExtensions().empty());
    EXPECT_TRUE(sim.getPlotAppearances().empty());
    EXPECT_EQ(sim.getDocument(), nullptr);
    EXPECT_EQ(sim.getPreferences(), nullptr);
    EXPECT_EQ(sim.hashCode(), 0);

    // The selected configuration, when there is one.
    alpha.rocket->setSelectedConfiguration(testFcid(2));
    const Simulation selected(*alpha.rocket);
    EXPECT_EQ(selected.getFlightConfigurationId(), testFcid(2));
    const Simulation& constant = selected;
    EXPECT_EQ(&constant.getRocket(), alpha.rocket.get());
    EXPECT_EQ(constant.getActiveConfiguration().getFlightConfigurationId(), testFcid(2));
}

TEST(Simulation, WithoutPreferencesTheOptionsAreTheBuiltInDefaults)
{
    // The design decision of this port (see Simulation): SimulationOptions(), which is calm.
    TestEstesAlphaIII        alpha;
    const Simulation         sim(*alpha.rocket);
    const SimulationOptions  defaults;
    const SimulationOptions& options = sim.getOptions();
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(options.getLaunchRodLength(), defaults.getLaunchRodLength());
    EXPECT_EQ(options.getLaunchRodAngle(), defaults.getLaunchRodAngle());
    EXPECT_EQ(options.getLaunchIntoWind(), defaults.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchLatitude(), defaults.getLaunchLatitude());
    EXPECT_EQ(options.getLaunchLongitude(), defaults.getLaunchLongitude());
    EXPECT_EQ(options.getLaunchAltitude(), defaults.getLaunchAltitude());
    EXPECT_EQ(options.isIsaAtmosphere(), defaults.isIsaAtmosphere());
    EXPECT_EQ(options.getTimeStep(), 0.05);
    EXPECT_EQ(options.getMaxSimulationTime(), 1200.0);
    EXPECT_FALSE(options.isRandomSeedFixed());
}

TEST(Simulation, WithPreferencesTheOptionsAreTheFactoryDefaults)
{
    // Java: options.copyConditionsFrom(factory.getDefault()). An empty store gives OpenRocket's
    // default launch conditions, with their wind of 2 m/s (SimulationConditionsTest
    // .testDefaultSimulationOptionFactory).
    InMemoryPreferences preferences;
    TestEstesAlphaIII   alpha;
    const Simulation    sim(*alpha.rocket, preferences);
    EXPECT_EQ(sim.getPreferences(), &preferences);
    const SimulationOptions& options = sim.getOptions();
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 2.0);
    EXPECT_NEAR(options.getAverageWindModel().getStandardDeviation(), 0.2, 1e-12);
    EXPECT_EQ(options.getAverageWindModel().getDirection(), std::numbers::pi / 2);
    EXPECT_EQ(options.getLaunchLatitude(), 28.61);
    EXPECT_EQ(options.getLaunchLongitude(), -80.60);
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_TRUE(options.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchRodLength(), 1.0);

    // The store's own values.
    preferences.setLaunchRodLength(2.5);
    preferences.setTimeStep(0.02);
    const Simulation other(*alpha.rocket, preferences);
    EXPECT_EQ(other.getOptions().getLaunchRodLength(), 2.5);
    EXPECT_EQ(other.getOptions().getTimeStep(), 0.02);
}

TEST(Simulation, SetFlightConfigurationIdCreatesTheConfigurationInTheRocket)
{
    // "error id: IllegalArgumentException: Attempted to set the configuration to an error id.
    // Not Allowed!", "rocket has fresh false", "after set: rocket has fresh true events 1
    // active id same true", "set again: events 1"
    TestEstesAlphaIII alpha;
    Simulation        sim(*alpha.rocket);
    EXPECT_EQ(bugText([&] { sim.setFlightConfigurationId(FlightConfigurationId::errorId()); }),
              "Attempted to set the configuration to an error id. Not Allowed!");
    EXPECT_TRUE(sim.getFlightConfigurationId().isDefaultId()) << "nothing changed";

    const FlightConfigurationId fresh;
    EXPECT_FALSE(alpha.rocket->containsFlightConfigurationId(fresh));
    const ChangeCounter events(sim.changed());
    sim.setFlightConfigurationId(fresh);
    EXPECT_TRUE(alpha.rocket->containsFlightConfigurationId(fresh));
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(sim.getActiveConfiguration().getFlightConfigurationId(), fresh);
    sim.setFlightConfigurationId(fresh);
    EXPECT_EQ(events.count(), 1) << "the same id";
}

TEST(Simulation, ChangesAreAnnounced)
{
    // "name set twice: events 2" (after the id), "name null: [] events 3", "option changed:
    // events 4", "option same: events 4", "syncModID: events 5", "copy own options: events 5"
    TestEstesAlphaIII   alpha;
    Simulation          sim(*alpha.rocket);
    const ChangeCounter events(sim.changed());
    sim.setName("x");
    sim.setName("x");
    EXPECT_EQ(events.count(), 1);
    sim.setName("");
    EXPECT_EQ(sim.getName(), "");
    EXPECT_EQ(events.count(), 2);
    sim.getOptions().setLaunchRodLength(2.5);
    EXPECT_EQ(events.count(), 3) << "the options' change is the simulation's";
    sim.getOptions().setLaunchRodLength(2.5);
    EXPECT_EQ(events.count(), 3);
    sim.syncModId();
    EXPECT_EQ(events.count(), 4);
    EXPECT_EQ(sim.getSimulatedConfigurationModId(), sim.getActiveConfiguration().getModId());
    sim.copySimulationOptionsFrom(sim.getOptions());
    EXPECT_EQ(events.count(), 4) << "nothing differs";
}

// ============================================================================ getStatus()

/// getStatus() of a simulation loaded with @p status and nothing else.
[[nodiscard]] Status statusOfLoaded(Status status)
{
    const SelectedAlpha s;
    return loaded(s.rocket(), status)->getStatus();
}

TEST(SimulationGetStatus, ALoadedStatusStaysWhileNothingChanges)
{
    // StatusBranchProbe: "loaded UPTODATE: UPTODATE" ... "loaded ABORTED: ABORTED"
    EXPECT_EQ(statusOfLoaded(Status::UPTODATE), Status::UPTODATE);
    EXPECT_EQ(statusOfLoaded(Status::LOADED), Status::LOADED);
    EXPECT_EQ(statusOfLoaded(Status::OUTDATED), Status::OUTDATED);
    EXPECT_EQ(statusOfLoaded(Status::EXTERNAL), Status::EXTERNAL);
    EXPECT_EQ(statusOfLoaded(Status::NOT_SIMULATED), Status::NOT_SIMULATED);
    EXPECT_EQ(statusOfLoaded(Status::CANT_RUN), Status::CANT_RUN) << "although it could run";
    EXPECT_EQ(statusOfLoaded(Status::ABORTED), Status::ABORTED) << "although it has no data";
}

/// The parameter: a status whose data is up to date.
class SimulationUpToDateStatus : public ::testing::TestWithParam<Status>
{ };

INSTANTIATE_TEST_SUITE_P(Statuses, SimulationUpToDateStatus, ::testing::ValuesIn(kUpToDate),
                         [](const ::testing::TestParamInfo<Status>& paramInfo) {
                             return std::string{name(paramInfo.param)};
                         });

TEST_P(SimulationUpToDateStatus, AChangedOptionMakesItOutdatedForGood)
{
    // "<status>: options changed OUTDATED, changed back OUTDATED"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    l->getOptions().setTimeStep(0.123);
    EXPECT_EQ(l->getStatus(), Status::OUTDATED);
    l->getOptions().setTimeStep(l->getSimulatedConditions()->getTimeStep());
    EXPECT_EQ(l->getStatus(), Status::OUTDATED) << "the status found is where the next call starts";
}

TEST_P(SimulationUpToDateStatus, AnOptionThatIsNotComparedChangesNothing)
{
    // "<status>: launchIntoWind toggled <status>": SimulationOptions' equality ignores it.
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    l->getOptions().setLaunchIntoWind(!l->getOptions().getLaunchIntoWind());
    EXPECT_EQ(l->getStatus(), GetParam());
}

TEST_P(SimulationUpToDateStatus, AChangedRocketMakesItOutdated)
{
    // "<status>: rocket changed OUTDATED, after syncModID OUTDATED"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    s.alpha.nose->setLength(0.08);
    EXPECT_EQ(l->getStatus(), Status::OUTDATED);
    l->syncModId();
    EXPECT_EQ(l->getStatus(), Status::OUTDATED) << "synchronising comes too late";
}

TEST_P(SimulationUpToDateStatus, SyncModIdBeforeTheStatusIsAskedKeepsIt)
{
    // "<status>: rocket changed and synced before the first getStatus <status>"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    s.alpha.nose->setLength(0.08);
    l->syncModId();
    EXPECT_EQ(l->getStatus(), GetParam());
}

TEST_P(SimulationUpToDateStatus, AChangeThatIsNotFunctionalChangesNothing)
{
    // "<status>: comment changed <status>, configuration renamed <status>"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    s.alpha.stage->setComment("a comment");
    EXPECT_EQ(l->getStatus(), GetParam());
    s.rocket().getFlightConfiguration(testFcid(0)).setName("renamed");
    EXPECT_EQ(l->getStatus(), GetParam());
}

TEST_P(SimulationUpToDateStatus, AnotherConfigurationMakesItOutdated)
{
    // "<status>: other configuration OUTDATED, back OUTDATED"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    l->setFlightConfigurationId(testFcid(1));
    EXPECT_EQ(l->getStatus(), Status::OUTDATED);
    l->setFlightConfigurationId(testFcid(0));
    EXPECT_EQ(l->getStatus(), Status::OUTDATED);
}

TEST_P(SimulationUpToDateStatus, WithoutMotorsItCannotRun)
{
    // "<status>: motors cleared CANT_RUN"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    s.rocket().getFlightConfiguration(testFcid(0)).clearAllMotors();
    EXPECT_EQ(l->getStatus(), Status::CANT_RUN);
}

TEST_P(SimulationUpToDateStatus, TheStagesOfTheConfigurationCount)
{
    // "<status>: setOnlyStage(0) OUTDATED, clearAllStages CANT_RUN": a stage flag set draws a
    // new modification id for the configuration, and without an active stage there is no motor.
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), GetParam());
    s.rocket().getFlightConfiguration(testFcid(0)).setOnlyStage(0);
    EXPECT_EQ(l->getStatus(), Status::OUTDATED);
    s.rocket().getFlightConfiguration(testFcid(0)).clearAllStages();
    EXPECT_EQ(l->getStatus(), Status::CANT_RUN);
}

TEST(SimulationGetStatus, CannotRunIsKeptWhenTheMotorsComeBack)
{
    // "new on the default configuration: CANT_RUN, switched to a configuration with motors:
    // CANT_RUN", "new on a configuration with motors: NOT_SIMULATED, motors cleared: CANT_RUN"
    TestEstesAlphaIII alpha;
    Simulation        fresh(*alpha.rocket);
    EXPECT_EQ(fresh.getStatus(), Status::CANT_RUN);
    fresh.setFlightConfigurationId(testFcid(1));
    EXPECT_EQ(fresh.getStatus(), Status::CANT_RUN) << "the status found is kept";

    const SelectedAlpha s;
    Simulation          withMotors(s.rocket());
    EXPECT_EQ(withMotors.getStatus(), Status::NOT_SIMULATED);
    s.rocket().getFlightConfiguration(testFcid(0)).clearAllMotors();
    EXPECT_EQ(withMotors.getStatus(), Status::CANT_RUN);
}

TEST(SimulationGetStatus, CopiesStartFromTheirOwnStatus)
{
    // "copy of UPTODATE: NOT_SIMULATED, clone: UPTODATE, original: UPTODATE"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), Status::UPTODATE);
    EXPECT_EQ(l->copy()->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(l->clone()->getStatus(), Status::UPTODATE);
    EXPECT_EQ(l->cloneForUndo()->getStatus(), Status::UPTODATE);
    EXPECT_EQ(l->getStatus(), Status::UPTODATE);
}

// ---------------------------------------------------------------------------- aborted data

/// getStatus() of a simulation loaded with @p status and data whose branch aborted.
[[nodiscard]] Status statusOfAborted(Status status)
{
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l =
        loaded(s.rocket(), status, abortedData({SimulationAbort::Cause::NO_MOTORS_DEFINED}));
    EXPECT_TRUE(l->hasErrors());
    EXPECT_TRUE(l->hasErrors(0));
    return l->getStatus();
}

TEST(SimulationGetStatus, DataWithAnAbortEventIsAbortedWhateverTheStatusWas)
{
    // "aborted data, <status>: ABORTED hasErrors true hasErrors(0) true", for every status
    EXPECT_EQ(statusOfAborted(Status::UPTODATE), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::LOADED), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::OUTDATED), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::EXTERNAL), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::NOT_SIMULATED), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::CANT_RUN), Status::ABORTED);
    EXPECT_EQ(statusOfAborted(Status::ABORTED), Status::ABORTED);
}

TEST(SimulationGetStatus, TheDescriptionOfAnAbortedSimulationNamesTheAborts)
{
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(
        s.rocket(), Status::UPTODATE, abortedData({SimulationAbort::Cause::NO_MOTORS_DEFINED}));
    const std::string abort = "<i>: No motors defined in the simulation</i><br>";
    // "description: <i><b>Simulation Aborted</b></i><i>: No motors defined in the simulation
    // </i><br>"
    EXPECT_EQ(l->getStatusDescription(), "<i><b>Simulation Aborted</b></i>" + abort);
    // "<status>.getDescription(aborted)": the description of the status asked, not of ABORTED
    EXPECT_EQ(getDescription(Status::UPTODATE, *l), "<i>Up to date</i>" + abort);
    EXPECT_EQ(getDescription(Status::LOADED, *l), "<i>Loaded from file</i>" + abort);
    EXPECT_EQ(getDescription(Status::OUTDATED, *l), "<i>Out of date</i>" + abort);
    EXPECT_EQ(getDescription(Status::EXTERNAL, *l), "<i>Imported data</i>" + abort);
    EXPECT_EQ(
        getDescription(Status::NOT_SIMULATED, *l),
        "<i>Not simulated yet</i> <br>Click <i><b>Run simulations</b></i> to simulate." + abort);
    EXPECT_EQ(getDescription(Status::CANT_RUN, *l),
              "<i>Errors in simulation prevent running</i>" + abort);
    EXPECT_EQ(getDescription(Status::ABORTED, *l), "<i><b>Simulation Aborted</b></i>" + abort);
}

TEST(SimulationGetStatus, EveryBranchThatAbortedIsDescribed)
{
    // "three branches: ABORTED truefalsetrue description: ..."
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l =
        loaded(s.rocket(), Status::UPTODATE,
               abortedData({SimulationAbort::Cause::NO_LIFTOFF, std::nullopt,
                            SimulationAbort::Cause::TUMBLE_UNDER_THRUST}));
    EXPECT_EQ(l->getStatus(), Status::ABORTED);
    EXPECT_TRUE(l->hasErrors(0));
    EXPECT_FALSE(l->hasErrors(1));
    EXPECT_TRUE(l->hasErrors(2));
    EXPECT_EQ(l->getStatusDescription(),
              "<i><b>Simulation Aborted</b></i><i>: <html>Motor burnout without liftoff. <br>Use "
              "more (powerful) motors, or decrease the rocket mass.</html></i><br>"
              "<i><b>Simulation Aborted</b></i><i>: Stage began to tumble under thrust.</i><br>");
}

TEST(SimulationGetStatus, DataWithoutAnAbortIsDescribedByTheStatus)
{
    // "no abort: UPTODATE hasSimulationData true description: <i>Up to date</i>", "empty data:
    // UPTODATE hasSimulationData false hasSummaryData true warnings same true"
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l =
        loaded(s.rocket(), Status::UPTODATE, abortedData({std::nullopt}));
    EXPECT_EQ(l->getStatus(), Status::UPTODATE);
    EXPECT_TRUE(l->hasSimulationData());
    EXPECT_FALSE(l->hasErrors());
    EXPECT_EQ(l->getStatusDescription(), "<i>Up to date</i>");
    EXPECT_EQ(getDescription(Status::NOT_SIMULATED, *l),
              "<i>Not simulated yet</i> <br>Click <i><b>Run simulations</b></i> to simulate.");

    const std::shared_ptr<FlightData> empty = std::make_shared<FlightData>();
    const std::unique_ptr<Simulation> e     = loaded(s.rocket(), Status::UPTODATE, empty);
    EXPECT_EQ(e->getStatus(), Status::UPTODATE);
    EXPECT_FALSE(e->hasSimulationData());
    EXPECT_TRUE(e->hasSummaryData());
    EXPECT_EQ(e->getSimulatedWarnings(), &empty->getWarningSet());
    const Simulation& constant = *e;
    EXPECT_EQ(constant.getSimulatedWarnings(), &empty->getWarningSet());
}

TEST(SimulationGetStatus, ABranchCannotBeAskedWithoutData)
{
    // "hasErrors(0) without data: NullPointerException"
    TestEstesAlphaIII alpha;
    const Simulation  sim(*alpha.rocket);
    EXPECT_EQ(bugText([&] { static_cast<void>(sim.hasErrors(0)); }),
              "The simulation has no simulated data");
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l =
        loaded(s.rocket(), Status::UPTODATE, abortedData({std::nullopt}));
    EXPECT_THROW(static_cast<void>(l->hasErrors(1)), BugError) << "a branch the data lacks";
}

// ================================================================== the loading constructor

TEST(Simulation, TheLoadingConstructorTakesWhatAFileDescribes)
{
    // "fcid selected true name From file options same true conditions same false conditions
    // equal true data same true extension same true list same false appearances [a, color]
    // description null", "options changed: events 1 status OUTDATED"
    TestEstesAlphaIII alpha;
    alpha.rocket->setSelectedConfiguration(testFcid(2));
    SimulationOptions options;
    options.setTimeStep(0.011);
    const ChangeCounter                   optionEvents(options.changed());
    const std::shared_ptr<Ext>            ext  = std::make_shared<Ext>();
    const std::shared_ptr<FlightData>     data = std::make_shared<FlightData>();
    std::map<std::string, PlotAppearance> appearances;
    appearances.insert({"a", PlotAppearance(Color{1, 2, 3}, LineStyle::DASHED)});
    appearances.insert({"empty", PlotAppearance(std::nullopt, std::nullopt)});
    appearances.insert({"color", PlotAppearance(Color{4, 5, 6, 7}, std::nullopt)});
    InMemoryPreferences preferences;

    Simulation l(nullptr, *alpha.rocket, Status::EXTERNAL, "From file", std::move(options), {ext},
                 data, appearances, &preferences);

    EXPECT_EQ(l.getFlightConfigurationId(), testFcid(2));
    EXPECT_EQ(l.getName(), "From file");
    EXPECT_EQ(l.getStoredStatus(), Status::EXTERNAL);
    EXPECT_EQ(l.getOptions().getTimeStep(), 0.011);
    ASSERT_NE(l.getSimulatedConditions(), nullptr);
    EXPECT_NE(l.getSimulatedConditions(), &l.getOptions()) << "a copy";
    EXPECT_TRUE(*l.getSimulatedConditions() == l.getOptions());
    EXPECT_EQ(l.getSimulatedData(), data);
    ASSERT_EQ(l.getSimulationExtensions().size(), 1U);
    EXPECT_EQ(l.getSimulationExtensions()[0], ext);
    EXPECT_FALSE(l.getSimulatedConfigurationDescription().has_value());
    EXPECT_EQ(l.getSimulatedConfigurationModId(),
              alpha.rocket->getSelectedConfiguration().getModId());
    EXPECT_EQ(l.getPreferences(), &preferences);
    EXPECT_EQ(l.getStatus(), Status::EXTERNAL);

    // The appearances that are not empty.
    const std::map<std::string, PlotAppearance> stored = l.getPlotAppearances();
    ASSERT_EQ(stored.size(), 2U);
    EXPECT_EQ(stored.at("a"), PlotAppearance(Color{1, 2, 3}, LineStyle::DASHED));
    EXPECT_EQ(stored.at("color"), PlotAppearance(Color{4, 5, 6, 7}, std::nullopt));

    // The options came with their connections, and their changes are the simulation's.
    const ChangeCounter events(l.changed());
    l.getOptions().setTimeStep(0.321);
    EXPECT_EQ(optionEvents.count(), 1);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(l.getStatus(), Status::OUTDATED);
}

// ------------------------------------- SimulationTest.java: the cases that run a simulation

/// SimulationTest's fixture (setUpSim()) for the cases that run the simulation: Java's
/// `new Simulation(rocket)` under the preferences of OpenRocket's test set-up, with the random
/// seed of the options fixed (Java: whatever seed the new options drew).
class SimulationRunTest : public ::testing::Test
{
protected:
    SimulationRunTest() : m_simulation(*m_alpha.rocket, m_preferences.store)
    {
        m_simulation.setFlightConfigurationId(testFcid(0));
        m_simulation.getOptions().setIsaAtmosphere(true);
        m_simulation.getOptions().setTimeStep(0.05);
        m_simulation.getOptions().setRandomSeed(0);
    }

    [[nodiscard]] Rocket&     rocket() const { return *m_alpha.rocket; }
    [[nodiscard]] Simulation& simulation() { return m_simulation; }

    /// The body of testSimulationWithNoMotors and its _RK6 variant.
    void simulationWithNoMotors(SimulationStepperMethod method)
    {
        // Create configuration without motors
        FlightConfiguration& config = rocket().getFlightConfiguration(testFcid(0));
        config.clearAllMotors();
        simulation().getOptions().setSimulationStepperMethodChoice(method);

        simulateOrFail(simulation());

        // Verify simulation aborted due to no motors
        const FlightData& data = simulatedData(simulation());
        ASSERT_GE(data.getBranchCount(), 1U);
        const FlightEvent* abort = data.getBranch(0).getLastEvent(FlightEvent::Type::SIM_ABORT);
        ASSERT_NE(abort, nullptr) << "Simulation without motors should abort";
        ASSERT_NE(abort->getAbort(), nullptr);
        EXPECT_EQ(SimulationAbort::Cause::NO_MOTORS_DEFINED, abort->getAbort()->cause());
    }

    /// The body of testBasicSimulationExecution and its _RK6 variant.
    void basicSimulationExecution(SimulationStepperMethod method)
    {
        simulation().getOptions().setSimulationStepperMethodChoice(method);
        simulateOrFail(simulation());

        const std::shared_ptr<FlightData>& data = simulation().getSimulatedData();
        ASSERT_NE(data, nullptr) << "Simulation data should not be null";
        EXPECT_TRUE(data->getMaxAltitude() > 0) << "Max altitude should be positive";
        EXPECT_TRUE(data->getMaxVelocity() > 0) << "Max velocity should be positive";
        EXPECT_TRUE(data->getFlightTime() > 0) << "Flight time should be positive";
        EXPECT_EQ(Status::UPTODATE, simulation().getStatus());
    }

    /// The body of testAltitudeAboveSeaLevel and its _RK6 variant.
    void altitudeAboveSeaLevel(SimulationStepperMethod method)
    {
        const double launchAltitude = 123;
        simulation().getOptions().setLaunchAltitude(launchAltitude);
        simulation().getOptions().setSimulationStepperMethodChoice(method);

        simulateOrFail(simulation());

        const FlightData& flightData = simulatedData(simulation());
        ASSERT_GE(flightData.getBranchCount(), 1U);
        const FlightDataBranch& branch = flightData.getBranch(0);

        const std::vector<double>* altitudeData =
            branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
        const std::vector<double>* altitudeAslData =
            branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE_ABOVE_SEA));

        ASSERT_NE(altitudeData, nullptr);
        ASSERT_NE(altitudeAslData, nullptr);
        expectAltitudesAboveSeaLevel(*altitudeData, *altitudeAslData, launchAltitude);
    }

private:
    /// The second half of testAltitudeAboveSeaLevel: the comparison of the two columns.
    static void expectAltitudesAboveSeaLevel(const std::vector<double>& altitudeData,
                                             const std::vector<double>& altitudeAslData,
                                             double                     launchAltitude)
    {
        ASSERT_EQ(altitudeData.size(), altitudeAslData.size());
        ASSERT_FALSE(altitudeData.empty());

        // Verify that altitude above sea level = altitude + launch altitude for each data
        // point
        EXPECT_EQ(altitudeMismatches(altitudeData, altitudeAslData, launchAltitude),
                  std::vector<std::string>{});

        // Additionally verify max altitudes
        const double maxAltitude    = *std::ranges::max_element(altitudeData);
        const double maxAltitudeAsl = *std::ranges::max_element(altitudeAslData);
        EXPECT_NEAR(maxAltitude + launchAltitude, maxAltitudeAsl, 0.001)
            << "Maximum altitude above sea level should equal maximum altitude + launch altitude";
    }

    /// The data points at which the altitude above sea level is not the altitude plus
    /// @p launchAltitude, within 0.001.
    [[nodiscard]] static std::vector<std::string> altitudeMismatches(
        const std::vector<double>& altitudeData, const std::vector<double>& altitudeAslData,
        double launchAltitude)
    {
        std::vector<std::string> mismatches;
        for (std::size_t i = 0; i < altitudeData.size(); i++)
        {
            const double altitude    = altitudeData[i];
            const double altitudeAsl = altitudeAslData[i];
            if (!(std::abs(altitude + launchAltitude - altitudeAsl) <= 0.001))
            {
                mismatches.push_back(std::format(
                    "Altitude above sea level should equal altitude + launch altitude at index "
                    "{}: {} + {} != {}",
                    i, altitude, launchAltitude, altitudeAsl));
            }
        }
        return mismatches;
    }

    JavaTestPreferences m_preferences;
    TestEstesAlphaIII   m_alpha;
    Simulation          m_simulation;
};

// SimulationTest.testSimulationWithNoMotors
TEST_F(SimulationRunTest, SimulationWithNoMotors)
{
    simulationWithNoMotors(SimulationStepperMethod::RK4);
}

// SimulationTest.testSimulationWithNoMotors_RK6
TEST_F(SimulationRunTest, SimulationWithNoMotorsRk6)
{
    simulationWithNoMotors(SimulationStepperMethod::RK6);
}

// SimulationTest.testBasicSimulationExecution
TEST_F(SimulationRunTest, BasicSimulationExecution)
{
    basicSimulationExecution(SimulationStepperMethod::RK4);
}

// SimulationTest.testBasicSimulationExecution_RK6
TEST_F(SimulationRunTest, BasicSimulationExecutionRk6)
{
    basicSimulationExecution(SimulationStepperMethod::RK6);
}

// SimulationTest.testAltitudeAboveSeaLevel
TEST_F(SimulationRunTest, AltitudeAboveSeaLevel)
{
    altitudeAboveSeaLevel(SimulationStepperMethod::RK4);
}

// SimulationTest.testAltitudeAboveSeaLevel_RK6
TEST_F(SimulationRunTest, AltitudeAboveSeaLevelRk6)
{
    altitudeAboveSeaLevel(SimulationStepperMethod::RK6);
}

// ============================================================================ simulate()

TEST(SimulationSimulate, AnImportedSimulationCannotBeSimulated)
{
    // "SimulationException: Cannot simulate imported simulation.", "after: data null status
    // UPTODATE events 1 conditions step 0.07 description [[A8-0]] extension initialized 0": the
    // data is dropped and the bookkeeping of a run is done all the same.
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l =
        loaded(s.rocket(), Status::EXTERNAL, abortedData({std::nullopt}));
    const auto log = std::make_shared<std::vector<std::string>>();
    l->getSimulationExtensions().push_back(std::make_shared<Ext>(log));
    l->getOptions().setTimeStep(0.07);
    const ChangeCounter events(l->changed());

    const Result<void> result = l->simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "Cannot simulate imported simulation.");
    EXPECT_EQ(l->getSimulatedData(), nullptr);
    EXPECT_EQ(l->getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(l->getStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    ASSERT_NE(l->getSimulatedConditions(), nullptr);
    EXPECT_EQ(l->getSimulatedConditions()->getTimeStep(), 0.07);
    EXPECT_EQ(l->getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_EQ(l->getSimulatedConfigurationModId(), l->getActiveConfiguration().getModId());
    EXPECT_TRUE(log->empty()) << "no extension was initialised";
}

TEST(SimulationSimulate, AnExtensionThatFailsEndsTheRunBeforeTheEngine)
{
    // "SimulationException: extension failed", "after failing extension: data null status
    // UPTODATE events 1 initialized 2 description [[A8-0]] conditions true"
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setIsaAtmosphere(true);
    const auto log     = std::make_shared<std::vector<std::string>>();
    const auto failing = std::make_shared<Ext>(log, "second");
    failing->setFailing();
    l.getSimulationExtensions().push_back(std::make_shared<Ext>(log, "first"));
    l.getSimulationExtensions().push_back(failing);
    l.getSimulationExtensions().push_back(std::make_shared<Ext>(log, "third"));
    const ChangeCounter events(l.changed());

    const Result<void> result = l.simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "extension failed");
    EXPECT_EQ(*log, (std::vector<std::string>{"first", "second"})) << "in order, up to the failure";
    EXPECT_EQ(l.getSimulatedData(), nullptr);
    EXPECT_EQ(l.getStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(l.getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_NE(l.getSimulatedConditions(), nullptr);
}

TEST(SimulationSimulate, ARefusedAtmosphereIsTheError)
{
    // "IllegalArgumentException: Temperature must be positive (Kelvin)", "after refused
    // atmosphere: data null status UPTODATE events 1 conditions true"
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setIsaAtmosphere(false);
    l.getOptions().setLaunchTemperature(-5);
    const auto log = std::make_shared<std::vector<std::string>>();
    l.getSimulationExtensions().push_back(std::make_shared<Ext>(log));
    const ChangeCounter events(l.changed());

    const Result<void> result = l.simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message, "Temperature must be positive (Kelvin)");
    EXPECT_TRUE(log->empty()) << "there were no conditions to initialise an extension with";
    EXPECT_EQ(l.getSimulatedData(), nullptr);
    EXPECT_EQ(l.getStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    EXPECT_NE(l.getSimulatedConditions(), nullptr);
}

TEST(SimulationSimulate, TheStopTokenOverloadRunsTheSameChecks)
{
    const SelectedAlpha               s;
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), Status::EXTERNAL);
    // (Held through a pointer: a stop source must not be const, see system_listener_tests.cpp.)
    const auto         source = std::make_shared<std::stop_source>();
    const Result<void> result = l->simulate(source->get_token());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "Cannot simulate imported simulation.");

    const std::unique_ptr<Simulation> again  = loaded(s.rocket(), Status::EXTERNAL);
    const Result<void>                listed = again->simulate({});
    EXPECT_FALSE(listed.has_value());
}

/// Writes its label down when the simulation starts, and may then fail the run with a bug.
/// The log is shared by the clones.
class StartRecorder final : public CloneableSimulationListener<StartRecorder>
{
public:
    StartRecorder(std::shared_ptr<std::vector<std::string>> log, std::string label,
                  bool buggy = false)
      : m_log(std::move(log)), m_label(std::move(label)), m_buggy(buggy)
    {
    }

    void startSimulation(SimulationStatus& /*status*/) override { m_log->push_back(m_label); }

    void endSimulationBranch(SimulationStatus& /*status*/,
                             const SimulationException* exception) override
    {
        m_log->push_back(m_label + (exception == nullptr ? " branch ended" : " branch failed"));
    }

    void postStep(SimulationStatus& /*status*/) override
    {
        if (m_buggy)
        {
            QtRocket::bug("a listener with a bug");
        }
    }

private:
    std::shared_ptr<std::vector<std::string>> m_log;
    std::string                               m_label;
    bool                                      m_buggy;
};

/// An extension that adds a listener to the conditions it is initialised with.
class ListeningExt final : public AbstractSimulationExtension
{
public:
    explicit ListeningExt(std::shared_ptr<std::vector<std::string>> log)
      : AbstractSimulationExtension("test.ListeningExt"), m_log(std::move(log))
    {
    }

    void initialize(SimulationConditions& conditions) override
    {
        conditions.getSimulationListenerList().push_back(
            std::make_shared<StartRecorder>(m_log, "the extension's listener"));
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<ListeningExt>(*this);
    }

private:
    std::shared_ptr<std::vector<std::string>> m_log;
};

TEST(SimulationSimulate, ARunRecordsWhatWasSimulated)
{
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setIsaAtmosphere(true);
    l.getOptions().setTimeStep(0.03);
    l.getOptions().setRandomSeed(0);
    const auto log = std::make_shared<std::vector<std::string>>();
    l.getSimulationExtensions().push_back(std::make_shared<Ext>(log, "first"));
    l.getSimulationExtensions().push_back(std::make_shared<Ext>(log, "second"));
    const ChangeCounter events(l.changed());
    EXPECT_EQ(l.getStatus(), Status::NOT_SIMULATED);

    const Result<void> result = l.simulate();

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(*log, (std::vector<std::string>{"first", "second"}));
    EXPECT_EQ(l.getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(l.getStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    ASSERT_NE(l.getSimulatedConditions(), nullptr);
    EXPECT_EQ(l.getSimulatedConditions()->getTimeStep(), 0.03);
    EXPECT_EQ(l.getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_EQ(l.getSimulatedConfigurationModId(), l.getActiveConfiguration().getModId());

    const std::shared_ptr<FlightData>& data = l.getSimulatedData();
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(l.hasSimulationData());
    EXPECT_TRUE(l.hasSummaryData());
    EXPECT_FALSE(l.hasErrors());
    EXPECT_EQ(l.getSimulatedWarnings(), &data->getWarningSet());
    ASSERT_EQ(data->getBranchCount(), 1U);
    EXPECT_GT(data->getMaxAltitude(), 0.0);
    // The extensions changed the conditions the engine ran on (a launch velocity of 1 m/s):
    // the first record of the flight has it.
    const FlightDataBranch& branch = data->getBranch(0);
    EXPECT_EQ(branch.getByIndex(FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z), 0),
              1.0);

    // A change of the options afterwards makes the data out of date.
    l.getOptions().setTimeStep(0.04);
    EXPECT_EQ(l.getStatus(), Status::OUTDATED);
}

TEST(SimulationSimulate, TheListenersOfTheExtensionsComeBeforeTheAdditionalOnes)
{
    // Java: the extensions are initialised, then the additional listeners are appended.
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setRandomSeed(0);
    const auto log = std::make_shared<std::vector<std::string>>();
    l.getSimulationExtensions().push_back(std::make_shared<ListeningExt>(log));

    const Result<void> result = l.simulate({std::make_shared<StartRecorder>(log, "additional 1"),
                                            std::make_shared<StartRecorder>(log, "additional 2")});

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(*log,
              (std::vector<std::string>{"the extension's listener", "additional 1", "additional 2",
                                        "the extension's listener branch ended",
                                        "additional 1 branch ended", "additional 2 branch ended"}));
}

TEST(SimulationSimulate, AnAbortedRunSucceedsAndTheStatusSaysSo)
{
    // A simulation that aborts is not a failure of simulate(): the data hold the SIM_ABORT.
    TestEstesAlphaIII alpha;
    alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllMotors();
    Simulation l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));

    const Result<void> result = l.simulate();

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(l.getStoredStatus(), Status::UPTODATE);
    EXPECT_TRUE(l.hasErrors());
    EXPECT_TRUE(l.hasErrors(0));
    // Java's getStatus(): no motors makes it CANT_RUN, then the abort in the data ABORTED.
    EXPECT_EQ(l.getStatus(), Status::ABORTED);
}

/// A listener that fails the run after its first step.
class Failing final : public CloneableSimulationListener<Failing>
{
public:
    void postStep(SimulationStatus& /*status*/) override
    {
        throw SimulationException("the listener gave up");
    }
};

TEST(SimulationSimulate, AListenersExceptionIsTheErrorAndTheDataStay)
{
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setRandomSeed(0);
    const ChangeCounter events(l.changed());

    const Result<void> result = l.simulate({std::make_shared<Failing>()});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "the listener gave up");
    // Java's finally block: the data the engine got to are the simulated data.
    EXPECT_EQ(l.getStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    const std::shared_ptr<FlightData>& data = l.getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightEvent* exception = data->getBranch(0).getLastEvent(FlightEvent::Type::EXCEPTION);
    ASSERT_NE(exception, nullptr);
    ASSERT_NE(exception->getMessage(), nullptr);
    EXPECT_EQ(*exception->getMessage(), "the listener gave up");
}

TEST(SimulationSimulate, AStopRequestCancelsTheRunAfterAStep)
{
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setRandomSeed(0);
    // (Held through a pointer: a stop source must not be const, see system_listener_tests.cpp.)
    const auto source = std::make_shared<std::stop_source>();
    source->request_stop();
    const auto log = std::make_shared<std::vector<std::string>>();

    const std::vector<std::shared_ptr<SimulationListener>> listeners{
        std::make_shared<StartRecorder>(log, "additional")};
    const Result<void> result = l.simulate(source->get_token(), listeners);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::CANCELLED);
    EXPECT_EQ(result.error().message, "The simulation was interrupted.");
    EXPECT_EQ(*log, (std::vector<std::string>{"additional", "additional branch failed"}));
    EXPECT_EQ(l.getStatus(), Status::UPTODATE);
    const std::shared_ptr<FlightData>& data = l.getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    // One step was taken before the interrupt listener looked at the token.
    EXPECT_EQ(data->getBranch(0).getLength(), 1U);
    EXPECT_NE(data->getBranch(0).getLastEvent(FlightEvent::Type::EXCEPTION), nullptr);

    // A token nobody stops lets the run finish.
    const auto         calm     = std::make_shared<std::stop_source>();
    const Result<void> finished = l.simulate(calm->get_token());
    EXPECT_TRUE(finished.has_value());
    EXPECT_NE(simulatedBranch(l).getLastEvent(FlightEvent::Type::GROUND_HIT), nullptr);
}

TEST(SimulationSimulate, ABugErrorPassesThroughAfterTheBookkeeping)
{
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setIsaAtmosphere(true);
    l.getOptions().setTimeStep(0.03);
    l.getOptions().setRandomSeed(0);
    const auto          log = std::make_shared<std::vector<std::string>>();
    const ChangeCounter events(l.changed());

    Result<void> result;
    EXPECT_EQ(bugText([&] {
                  result = l.simulate({std::make_shared<StartRecorder>(log, "buggy", true)});
              }),
              "a listener with a bug");
    EXPECT_TRUE(result.has_value()) << "nothing was returned";

    // Java's finally blocks ran: the branch's end was announced (without an exception: it is
    // no SimulationException), and the simulation recorded what the engine got to.
    EXPECT_EQ(*log, (std::vector<std::string>{"buggy", "buggy branch ended"}));
    EXPECT_EQ(l.getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(events.count(), 1);
    ASSERT_NE(l.getSimulatedConditions(), nullptr);
    EXPECT_EQ(l.getSimulatedConditions()->getTimeStep(), 0.03);
    EXPECT_EQ(l.getSimulatedConfigurationDescription(), "[A8-0]");
    const std::shared_ptr<FlightData>& data = l.getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    EXPECT_EQ(data->getBranch(0).getLength(), 1U);
    // No EXCEPTION event: that is for SimulationExceptions.
    EXPECT_EQ(data->getBranch(0).getLastEvent(FlightEvent::Type::EXCEPTION), nullptr);
    EXPECT_NE(data->getSimulatedRocket(), nullptr);
}

/// How simulate() of the Alpha III ends after @p change has had the options: "ok", "error:
/// <message>" for a failure that is returned, "bug: <message>" for a BugError that leaves.
[[nodiscard]] std::string outcomeWith(const std::function<void(SimulationOptions&)>& change)
{
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setRandomSeed(0);
    change(l.getOptions());
    std::string       outcome = "ok";
    const std::string bug     = bugText([&l, &outcome] {
        const Result<void> result = l.simulate();
        if (!result.has_value())
        {
            outcome = "error: " + result.error().message;
        }
    });
    return bug == "<none>" ? outcome : "bug: " + bug;
}

// The contract of simulate() for inputs that are not finite (see its comment and
// validateInputs()): they are refused before anything runs, as an ordinary error with
// ErrorCode::INVALID_ARGUMENT and a text that names the value. OpenRocket does not validate,
// and until QtRocket did, this test pinned where these three values surfaced: the latitude as
// a BugError before the first step ("Simulation resulted in not-a-number (NaN) value for
// gravity, please report a bug."), the time step as one in the stepper, and the rod length as
// the SIMULATION_ABORTED error of the engine's NaN check. A BugError means a defect of the
// program, and these values come from outside it. simulation_input_validation_tests.cpp goes
// through every input.
TEST(SimulationSimulate, NonFiniteInputsAreRefused)
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(outcomeWith([](SimulationOptions& /*options*/) { }), "ok");
    EXPECT_EQ(outcomeWith([](SimulationOptions& o) { o.setLaunchLatitude(kNaN); }),
              "error: Cannot simulate: the launch latitude is not finite (NaN).");
    EXPECT_EQ(outcomeWith([](SimulationOptions& o) { o.setTimeStep(kInf); }),
              "error: Cannot simulate: the time step is not finite (Infinity).");
    EXPECT_EQ(outcomeWith([](SimulationOptions& o) { o.setLaunchRodLength(kNaN); }),
              "error: Cannot simulate: the launch rod length is not finite (NaN).");

    // The error is one of the caller's arguments, not an aborted simulation, and the engine
    // made no data.
    TestEstesAlphaIII alpha;
    Simulation        l(*alpha.rocket);
    l.setFlightConfigurationId(testFcid(0));
    l.getOptions().setLaunchRodLength(-kInf);
    const Result<void> result = l.simulate();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message,
              "Cannot simulate: the launch rod length is not finite (-Infinity).");
    EXPECT_EQ(l.getSimulatedData(), nullptr);
    EXPECT_EQ(l.getStoredStatus(), Status::UPTODATE);
}

/// Everything @p data refer to, read through: the simulated rocket, and per branch its name, its
/// last record, and every event with its source, the mount and the motor of its motor state
/// and the sources of its warning; then the warnings of the flight.
[[nodiscard]] std::string walked(const FlightData& data)
{
    std::string text = data.getSimulatedRocket()->getName() + "\n";
    for (std::size_t b = 0; b < data.getBranchCount(); b++)
    {
        const FlightDataBranch& branch = data.getBranch(b);
        text += std::format("{} {} {}\n", branch.getName(), branch.getLength(),
                            branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)));
        for (const FlightEvent& event : branch.getEvents())
        {
            text += event.toString();
            if (event.getSource() != nullptr)
            {
                text += " from " + event.getSource()->getName() + " of " +
                        event.getSource()->getRocket().getName();
            }
            if (const std::shared_ptr<MotorClusterState> state = event.getMotorState())
            {
                text += " motor " + state->toDescription() + " in " +
                        asComponent(state->getMount()).getName();
            }
            text += '\n';
        }
    }
    return text + data.getWarningSet().toString();
}

// The flight data stay usable after the simulation, the engine and the caller's rocket are
// gone: they co-own the rocket that was simulated, which the sources of the events and the
// mounts of the motor states are components of, and the copies made for a clone or an undo
// share it. The address sanitizer is what finds a reference that does not hold.
TEST(SimulationSimulate, TheFlightDataOutliveTheSimulationAndTheCallersRocket)
{
    std::shared_ptr<FlightData> data;
    std::shared_ptr<FlightData> cloned;
    std::string                 before;
    {
        TestBeta beta;
        beta.rocket->getSelectedConfiguration().setAllStages();
        Simulation l(*beta.rocket);
        l.setFlightConfigurationId(
            beta.rocket->getSelectedConfiguration().getFlightConfigurationId());
        l.getOptions().setRandomSeed(0);
        ASSERT_TRUE(l.simulate().has_value());
        data = l.getSimulatedData();
        ASSERT_NE(data, nullptr);
        const std::unique_ptr<Simulation> clone = l.clone(true);
        cloned                                  = clone->getSimulatedData();
        ASSERT_NE(cloned, nullptr);
        before = walked(*data);
    }
    ASSERT_EQ(data->getBranchCount(), 2U);
    EXPECT_EQ(walked(*data), before);
    EXPECT_EQ(walked(*cloned), before);
    EXPECT_TRUE(before.starts_with("Kit-bash Beta\nSustainer Stage ")) << before;
    EXPECT_TRUE(before.contains(" from Booster MMT of Kit-bash Beta motor ")) << before;
}

TEST(SimulationSimulate, TheDescriptionIsTheNameOfTheSimulatedConfiguration)
{
    // The name of the configuration with its motors substituted, as the preferences name them.
    const SelectedAlpha s;
    s.rocket().getFlightConfiguration(testFcid(0)).setName("Flight {motors} of {manufacturers}");
    const std::unique_ptr<Simulation> l = loaded(s.rocket(), Status::EXTERNAL);
    EXPECT_FALSE(l->simulate().has_value());
    const InMemoryPreferences defaults;
    EXPECT_EQ(l->getSimulatedConfigurationDescription(),
              s.rocket().getFlightConfiguration(testFcid(0)).getName(defaults));
    EXPECT_TRUE(
        l->getSimulatedConfigurationDescription().value_or("").starts_with("Flight A8-0 of "));
}

// ============================================================================ the copies

/// A simulation that is up to date, with a name, an extension, a plot appearance and data
/// (the probe's `orig`).
struct Original
{
    SelectedAlpha               s;
    std::shared_ptr<FlightData> data = abortedData({std::nullopt});
    std::shared_ptr<Ext>        ext  = std::make_shared<Ext>();
    std::unique_ptr<Simulation> sim  = loaded(s.rocket(), Status::UPTODATE, data);

    Original()
    {
        sim->setName("Original");
        ext->set("k", "v");
        sim->getSimulationExtensions().push_back(ext);
        sim->setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE),
                               PlotAppearance(Color{1, 2, 3}, LineStyle::DASHED));
    }
};

/// What every copy has in common with its original: the name, the configuration id, the
/// rocket, equal options of its own, and a clone of the extension with its configuration.
/// Gives what is wrong, one line each.
[[nodiscard]] std::string commonCopyProblems(const Simulation& copy, const Original& orig)
{
    std::string problems;
    const auto  expect = [&problems](bool holds, std::string_view what) {
        if (!holds)
        {
            problems += what;
            problems += '\n';
        }
    };
    expect(copy.getName() == "Original", "the name");
    expect(copy.getFlightConfigurationId() == orig.sim->getFlightConfigurationId(), "the id");
    expect(&copy.getRocket() == &orig.s.rocket(), "the same rocket");
    expect(&copy.getOptions() != &orig.sim->getOptions(), "options of its own");
    expect(copy.getOptions() == orig.sim->getOptions(), "equal options");
    if (copy.getSimulationExtensions().size() != 1U)
    {
        return problems + "one extension";
    }
    expect(copy.getSimulationExtensions()[0] != orig.ext, "a clone of the extension");
    expect(copy.getSimulationExtensions()[0]->getConfig().getString("k", "?") == "v",
           "the extension's configuration");
    expect(copy == *orig.sim, "equal to the original");
    expect(*orig.sim == copy, "the original equal to it");
    return problems;
}

/// The plot appearances are one map for a copy made with Java's Object.clone() and its
/// original: an appearance set on @p copy shows in @p orig, and only the copy announces it.
void expectSharedAppearances(Simulation& copy, Simulation& orig)
{
    const FlightDataType& type = FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z);
    const ChangeCounter   copyEvents(copy.changed());
    const ChangeCounter   origEvents(orig.changed());
    copy.setPlotAppearance(type, PlotAppearance(std::nullopt, LineStyle::DOTTED));
    EXPECT_EQ(orig.getPlotAppearance(type), PlotAppearance(std::nullopt, LineStyle::DOTTED));
    EXPECT_EQ(copyEvents.count(), 1);
    EXPECT_EQ(origEvents.count(), 0);
    orig.setPlotAppearance(type, std::nullopt);
    EXPECT_FALSE(copy.getPlotAppearance(type).has_value());
}

TEST(SimulationCopies, CopyKeepsTheSettingsAndDropsWhatWasSimulated)
{
    // "copy: name Original fcid same true rocket same true status NOT_SIMULATED options same
    // false options equal true conditions null data null description null extension clone ext
    // config v equals true true", "option of the copy changed: its events 0 original's 0 equals
    // false", "appearance set on the copy: original has it true its events 1 original's 0",
    // "renamed: original Original equals false"
    const Original                    orig;
    const std::unique_ptr<Simulation> k = orig.sim->copy();
    EXPECT_EQ(commonCopyProblems(*k, orig), "");
    EXPECT_EQ(k->getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(k->getSimulatedConditions(), nullptr);
    EXPECT_EQ(k->getSimulatedData(), nullptr);
    EXPECT_FALSE(k->getSimulatedConfigurationDescription().has_value());
    EXPECT_EQ(k->getSimulatedConfigurationModId(), ModId::invalid());

    const ChangeCounter events(k->changed());
    const ChangeCounter origEvents(orig.sim->changed());
    k->getOptions().setLaunchRodLength(k->getOptions().getLaunchRodLength() + 1);
    EXPECT_EQ(events.count(), 0) << "as in Java, the copy's options have no listener";
    EXPECT_EQ(origEvents.count(), 0);
    EXPECT_FALSE(*k == *orig.sim);

    expectSharedAppearances(*k, *orig.sim);
    k->setName("Changed");
    EXPECT_EQ(orig.sim->getName(), "Original");
}

TEST(SimulationCopies, CloneKeepsEverythingAndClonesTheData)
{
    // "clone: ... status UPTODATE ... conditions copy data copy ...", "option of the clone
    // changed: its events 1 original's 0 equals false", "appearance set on the clone: original
    // has it true"
    const Original                    orig;
    const std::unique_ptr<Simulation> k = orig.sim->clone();
    EXPECT_EQ(commonCopyProblems(*k, orig), "");
    EXPECT_EQ(k->getStoredStatus(), Status::UPTODATE);
    ASSERT_NE(k->getSimulatedConditions(), nullptr);
    EXPECT_NE(k->getSimulatedConditions(), orig.sim->getSimulatedConditions());
    EXPECT_TRUE(*k->getSimulatedConditions() == *orig.sim->getSimulatedConditions());
    ASSERT_NE(k->getSimulatedData(), nullptr);
    EXPECT_NE(k->getSimulatedData(), orig.data) << "a clone of the data";
    EXPECT_EQ(k->getSimulatedData()->getBranchCount(), 1U);
    EXPECT_EQ(k->getSimulatedConfigurationModId(), orig.sim->getSimulatedConfigurationModId());

    const ChangeCounter events(k->changed());
    const ChangeCounter origEvents(orig.sim->changed());
    k->getOptions().setLaunchRodLength(k->getOptions().getLaunchRodLength() + 1);
    EXPECT_EQ(events.count(), 1) << "the clone's options have their listener";
    EXPECT_EQ(origEvents.count(), 0);
    EXPECT_FALSE(*k == *orig.sim);

    expectSharedAppearances(*k, *orig.sim);
}

TEST(SimulationCopies, CloneWithoutTheDataHasNone)
{
    // "clone(false): ... status UPTODATE ... conditions copy data null ..."
    const Original                    orig;
    const std::unique_ptr<Simulation> k = orig.sim->clone(false);
    EXPECT_EQ(commonCopyProblems(*k, orig), "");
    EXPECT_EQ(k->getStoredStatus(), Status::UPTODATE);
    EXPECT_NE(k->getSimulatedConditions(), nullptr);
    EXPECT_EQ(k->getSimulatedData(), nullptr);
    EXPECT_EQ(orig.sim->getSimulatedData(), orig.data);

    // A simulation without data clones to one without.
    TestEstesAlphaIII alpha;
    const Simulation  bare(*alpha.rocket);
    EXPECT_EQ(bare.clone()->getSimulatedData(), nullptr);
    EXPECT_EQ(bare.clone()->getSimulatedConditions(), nullptr);
}

TEST(SimulationCopies, CloneForUndoSharesTheData)
{
    // "cloneForUndo: ... status UPTODATE ... conditions copy data same ...", "option of the
    // cloneForUndo changed: its events 0 original's 0 equals false"
    const Original                    orig;
    const std::unique_ptr<Simulation> k = orig.sim->cloneForUndo();
    EXPECT_EQ(commonCopyProblems(*k, orig), "");
    EXPECT_EQ(k->getStoredStatus(), Status::UPTODATE);
    EXPECT_NE(k->getSimulatedConditions(), nullptr);
    EXPECT_EQ(k->getSimulatedData(), orig.data);

    const ChangeCounter events(k->changed());
    k->getOptions().setLaunchRodLength(k->getOptions().getLaunchRodLength() + 1);
    EXPECT_EQ(events.count(), 0) << "as in Java, the undo copy's options have no listener";
    EXPECT_FALSE(*k == *orig.sim);

    expectSharedAppearances(*k, *orig.sim);
}

TEST(SimulationCopies, DuplicateSimulationIsANewSimulationOfAnotherRocket)
{
    // "duplicateSimulation: name Original fcid same true rocket true status NOT_SIMULATED
    // options equal false ... conditions null data null description null extension clone
    // appearances 1 equals false", "duplicate changed: its events 2 original has the appearance
    // false"
    const Original                    orig;
    const std::unique_ptr<Rocket>     other = orig.s.rocket().copyRocketWithOriginalId();
    const std::unique_ptr<Simulation> dup   = orig.sim->duplicateSimulation(*other);
    EXPECT_EQ(dup->getName(), "Original");
    EXPECT_EQ(dup->getFlightConfigurationId(), orig.sim->getFlightConfigurationId());
    EXPECT_EQ(&dup->getRocket(), other.get());
    EXPECT_EQ(dup->getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(dup->getStatus(), Status::NOT_SIMULATED);
    // The conditions were copied onto options of the duplicate's own, whose average wind model
    // keeps its own seed.
    EXPECT_FALSE(dup->getOptions() == orig.sim->getOptions());
    EXPECT_EQ(dup->getOptions().getLaunchRodLength(), orig.sim->getOptions().getLaunchRodLength());
    EXPECT_EQ(dup->getOptions().getTimeStep(), orig.sim->getOptions().getTimeStep());
    EXPECT_EQ(dup->getSimulatedConditions(), nullptr);
    EXPECT_EQ(dup->getSimulatedData(), nullptr);
    EXPECT_FALSE(dup->getSimulatedConfigurationDescription().has_value());
    ASSERT_EQ(dup->getSimulationExtensions().size(), 1U);
    EXPECT_NE(dup->getSimulationExtensions()[0], orig.ext);
    EXPECT_EQ(dup->getPlotAppearances(), orig.sim->getPlotAppearances());
    EXPECT_EQ(dup->getPlotAppearances().size(), 1U);
    EXPECT_FALSE(*dup == *orig.sim);

    // Its own listener and its own appearances.
    const FlightDataType& type = FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z);
    const ChangeCounter   events(dup->changed());
    dup->getOptions().setLaunchRodLength(9);
    dup->setPlotAppearance(type, PlotAppearance(std::nullopt, LineStyle::DOTTED));
    EXPECT_EQ(events.count(), 2);
    EXPECT_FALSE(orig.sim->getPlotAppearance(type).has_value());
}

TEST(SimulationCopies, DuplicateSimulationCopiesTheDescriptionAndKeepsThePreferences)
{
    InMemoryPreferences preferences;
    preferences.setLaunchRodLength(3.5);
    const SelectedAlpha s;
    Simulation          sim(s.rocket(), preferences);
    static_cast<void>(sim.getStatus());
    const std::unique_ptr<Simulation> external = loaded(s.rocket(), Status::EXTERNAL);
    EXPECT_FALSE(external->simulate().has_value());
    ASSERT_TRUE(external->getSimulatedConfigurationDescription().has_value());

    const std::unique_ptr<Simulation> dup = external->duplicateSimulation(s.rocket());
    EXPECT_EQ(dup->getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_EQ(dup->getPreferences(), nullptr);

    const std::unique_ptr<Simulation> withPreferences = sim.duplicateSimulation(s.rocket());
    EXPECT_EQ(withPreferences->getPreferences(), &preferences);
    EXPECT_EQ(withPreferences->getOptions().getLaunchRodLength(), 3.5);
}

TEST(SimulationCopies, TheIndependentDuplicateOwnsACopyOfTheRocket)
{
    // "duplicateForIndependentSimulation: name Original fcid same true rocket same false rocket
    // id same true status NOT_SIMULATED options equal true conditions null data null description
    // null extension clone appearances 0 equals false", "independent changed: its events 1
    // original rod 0.0"
    std::unique_ptr<Simulation> ind;
    QtRocket::Uuid              rocketId;
    FlightConfigurationId       fcid;
    {
        const Original orig;
        rocketId = orig.s.rocket().getId();
        fcid     = orig.sim->getFlightConfigurationId();
        ind      = orig.sim->duplicateForIndependentSimulation();
        EXPECT_NE(&ind->getRocket(), &orig.s.rocket());
        EXPECT_TRUE(ind->getOptions() == orig.sim->getOptions()) << "a full copy of the options";
        EXPECT_NE(ind->getSimulationExtensions()[0], orig.ext);
        EXPECT_FALSE(*ind == *orig.sim) << "the plot appearances are not copied";

        const double        rod = orig.sim->getOptions().getLaunchRodLength();
        const ChangeCounter events(ind->changed());
        ind->getOptions().setLaunchRodLength(7);
        EXPECT_EQ(events.count(), 1);
        EXPECT_EQ(orig.sim->getOptions().getLaunchRodLength(), rod);
    }
    // The original and its rocket are gone; the duplicate lives on with its own rocket.
    EXPECT_EQ(ind->getName(), "Original");
    EXPECT_EQ(ind->getFlightConfigurationId(), fcid);
    EXPECT_EQ(ind->getRocket().getId(), rocketId);
    EXPECT_EQ(ind->getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(ind->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(ind->getSimulatedConditions(), nullptr);
    EXPECT_EQ(ind->getSimulatedData(), nullptr);
    EXPECT_FALSE(ind->getSimulatedConfigurationDescription().has_value());
    EXPECT_TRUE(ind->getPlotAppearances().empty());
    EXPECT_EQ(ind->getDocument(), nullptr);
    EXPECT_EQ(ind->getActiveConfiguration().getFlightConfigurationId(), fcid);
    EXPECT_TRUE(ind->getActiveConfiguration().hasMotors());

    // And so do its own copies, which share that rocket.
    const std::unique_ptr<Simulation> copy   = ind->copy();
    const Rocket*                     rocket = &ind->getRocket();
    ind.reset();
    EXPECT_EQ(&copy->getRocket(), rocket);
    EXPECT_EQ(copy->getRocket().getId(), rocketId);
    EXPECT_EQ(copy->getStatus(), Status::NOT_SIMULATED);
}

// ============================================================================ loadFrom()

TEST(SimulationLoadFrom, TakesTheSettingsAndSharesWhatWasSimulated)
{
    // "target: name Source fcid3 true step 0.011 conditions copy equal true data same true
    // extension same true lists same false appearances 0 events 3 status OUTDATED source status
    // OUTDATED equals false", "again: events 2 status OUTDATED"
    const SelectedAlpha               s;
    const std::shared_ptr<FlightData> data   = abortedData({std::nullopt});
    const std::unique_ptr<Simulation> source = loaded(s.rocket(), Status::UPTODATE, data);
    source->setName("Source");
    source->setFlightConfigurationId(testFcid(3));
    source->getOptions().setTimeStep(0.011);
    const std::shared_ptr<Ext> ext = std::make_shared<Ext>();
    source->getSimulationExtensions().push_back(ext);
    source->setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE),
                              PlotAppearance(std::nullopt, LineStyle::DASHED));

    InMemoryPreferences preferences;
    Simulation          target(s.rocket(), preferences);
    ChangeCounter       events(target.changed());
    target.loadFrom(*source);

    EXPECT_EQ(target.getName(), "Source");
    EXPECT_EQ(target.getFlightConfigurationId(), testFcid(3));
    EXPECT_EQ(target.getOptions().getTimeStep(), 0.011);
    ASSERT_NE(target.getSimulatedConditions(), nullptr);
    EXPECT_NE(target.getSimulatedConditions(), source->getSimulatedConditions());
    EXPECT_TRUE(*target.getSimulatedConditions() == *source->getSimulatedConditions());
    EXPECT_EQ(target.getSimulatedData(), data);
    ASSERT_EQ(target.getSimulationExtensions().size(), 1U);
    EXPECT_EQ(target.getSimulationExtensions()[0], ext) << "the same extension object";
    EXPECT_TRUE(target.getPlotAppearances().empty()) << "the plot appearances stay";
    // The average wind model, the multi-level wind model, and the options themselves.
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(target.getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(target.getSimulatedConfigurationModId(),
              s.rocket().getFlightConfiguration(testFcid(3)).getModId())
        << "an up-to-date status takes the modification id of this simulation's configuration";
    // The options keep the seed of their own average wind model, so they are not the simulated
    // conditions.
    EXPECT_EQ(target.getStatus(), Status::OUTDATED);
    EXPECT_EQ(source->getStatus(), Status::OUTDATED);
    EXPECT_FALSE(target == *source);

    events.reset();
    target.loadFrom(*source);
    EXPECT_EQ(events.count(), 2) << "the average wind model, whose seed never comes along";
    EXPECT_EQ(target.getStatus(), Status::OUTDATED);
}

TEST(SimulationLoadFrom, ASimulationLoadedFromItselfLosesItsExtensions)
{
    // "from itself: extensions 0": the list is cleared before it is read, as in Java.
    const Original orig;
    orig.sim->loadFrom(*orig.sim);
    EXPECT_TRUE(orig.sim->getSimulationExtensions().empty());
    EXPECT_EQ(orig.sim->getName(), "Original");
    EXPECT_EQ(orig.sim->getSimulatedData(), orig.data);
}

TEST(SimulationLoadFrom, ANewSimulationLoadsNothingSimulated)
{
    // "from a new one: conditions null data null status NOT_SIMULATED name [] fcid default
    // false"
    const Original orig;
    Simulation     empty(orig.s.rocket());
    orig.sim->loadFrom(empty);
    EXPECT_EQ(orig.sim->getSimulatedConditions(), nullptr);
    EXPECT_EQ(orig.sim->getSimulatedData(), nullptr);
    EXPECT_EQ(orig.sim->getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(orig.sim->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(orig.sim->getName(), "");
    EXPECT_EQ(orig.sim->getFlightConfigurationId(), testFcid(0));
    EXPECT_TRUE(orig.sim->getSimulationExtensions().empty());
    EXPECT_FALSE(orig.sim->getPlotAppearances().empty()) << "the plot appearances stay";
}

TEST(SimulationLoadFrom, TheSimulatedConditionsAreCopiedIntoTheOnesThereAre)
{
    // Java: this.simulatedConditions.copyConditionsFrom(simulation.simulatedConditions) when
    // both simulations have some.
    const SelectedAlpha s;
    SimulationOptions   options;
    options.setTimeStep(0.011);
    const Simulation source(nullptr, s.rocket(), Status::LOADED, "Source", std::move(options),
                            std::vector<std::shared_ptr<SimulationExtension>>{}, nullptr);
    const std::unique_ptr<Simulation> target     = loaded(s.rocket(), Status::LOADED);
    const SimulationOptions*          conditions = target->getSimulatedConditions();
    const SimulationOptions*          simulated  = source.getSimulatedConditions();
    ASSERT_NE(conditions, nullptr);
    ASSERT_NE(simulated, nullptr);
    EXPECT_EQ(simulated->getTimeStep(), 0.011) << "what the source was loaded with";
    EXPECT_NE(conditions->getTimeStep(), 0.011);

    target->loadFrom(source);
    EXPECT_EQ(target->getSimulatedConditions(), conditions) << "the same object";
    EXPECT_EQ(conditions->getTimeStep(), 0.011);
    EXPECT_EQ(target->getOptions().getTimeStep(), 0.011);
    // copyConditionsFrom() takes the seed together with a change.
    EXPECT_EQ(conditions->getRandomSeed(), simulated->getRandomSeed());
    EXPECT_EQ(target->getOptions().getRandomSeed(), source.getOptions().getRandomSeed());
}

// ============================================================================ equality

TEST(SimulationEquality, ComparesTheSettingsNotWhatWasSimulated)
{
    // "two new: false self true ... hash 0", "clone: true", "extension count differs: false",
    // "equal extensions: true", "config differs: false", "config same again: true", "same
    // extension object: true", "other fcid: false", "clone shares appearances, so equal: true",
    // "duplicate: false"
    TestEstesAlphaIII alpha;
    Simulation        a(*alpha.rocket);
    const Simulation  b(*alpha.rocket);
    EXPECT_FALSE(a == b) << "two wind models never have the same seed";
    EXPECT_TRUE(a == a);
    EXPECT_EQ(a.hashCode(), 0);

    const std::unique_ptr<Simulation> k = a.clone();
    EXPECT_TRUE(a == *k);

    const std::shared_ptr<Ext> e1 = std::make_shared<Ext>();
    const std::shared_ptr<Ext> e2 = std::make_shared<Ext>();
    a.getSimulationExtensions().push_back(e1);
    EXPECT_FALSE(a == *k) << "the number of extensions";
    k->getSimulationExtensions().push_back(e2);
    EXPECT_TRUE(a == *k) << "the same id and configuration";
    e2->set("k", "v");
    EXPECT_FALSE(a == *k) << "another configuration";
    e1->set("k", "v");
    EXPECT_TRUE(a == *k);
    k->getSimulationExtensions()[0] = e1;
    EXPECT_TRUE(a == *k) << "the same extension object";

    const std::unique_ptr<Simulation> k2 = a.clone();
    k2->setFlightConfigurationId(testFcid(1));
    EXPECT_FALSE(a == *k2);
    const std::unique_ptr<Simulation> k3 = a.clone();
    k3->setName("other");
    EXPECT_FALSE(a == *k3);
    EXPECT_FALSE(a == *a.duplicateSimulation(*alpha.rocket));
}

TEST(SimulationEquality, AnExtensionWithAnotherIdDiffers)
{
    /// An extension with an id of its own.
    class Other final : public AbstractSimulationExtension
    {
    public:
        Other() : AbstractSimulationExtension("test.Other") { }
        void initialize(SimulationConditions& /*conditions*/) override { }
        [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
        {
            return std::make_unique<Other>(*this);
        }
    };

    TestEstesAlphaIII                 alpha;
    Simulation                        a(*alpha.rocket);
    const std::unique_ptr<Simulation> k = a.clone();
    a.getSimulationExtensions().push_back(std::make_shared<Ext>());
    k->getSimulationExtensions().push_back(std::make_shared<Other>());
    EXPECT_FALSE(a == *k);
    // A null extension equals only itself.
    a.getSimulationExtensions()[0] = nullptr;
    EXPECT_FALSE(a == *k);
    k->getSimulationExtensions()[0] = nullptr;
    EXPECT_TRUE(a == *k);
}

TEST(SimulationEquality, ThePlotAppearancesCount)
{
    // "duplicateForIndependentSimulation: ... appearances 0 equals false"
    const Original                    orig;
    const std::unique_ptr<Simulation> ind = orig.sim->duplicateForIndependentSimulation();
    EXPECT_FALSE(*ind == *orig.sim);
    ind->setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE),
                           PlotAppearance(Color{1, 2, 3}, LineStyle::DASHED));
    EXPECT_TRUE(*ind == *orig.sim) << "the status and the data do not count";
    ind->setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE),
                           PlotAppearance(Color{1, 2, 3}, LineStyle::DOTTED));
    EXPECT_FALSE(*ind == *orig.sim);
}

// ============================================================================ plot appearances

TEST(SimulationPlotAppearances, AreStoredBySymbolAndAnnounced)
{
    // "none: null", "set: same object false equal true events 1 keys [h]", "caller changes its
    // object: stored DASHDOT", "empty removes: null events 2", "null on nothing: events 3",
    // "altitude symbol [h]"
    TestEstesAlphaIII     alpha;
    Simulation            s(*alpha.rocket);
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const ChangeCounter   events(s.changed());
    EXPECT_FALSE(s.getPlotAppearance(altitude).has_value());

    PlotAppearance app(Color{10, 20, 30}, LineStyle::DASHDOT);
    s.setPlotAppearance(altitude, app);
    EXPECT_EQ(s.getPlotAppearance(altitude), app);
    EXPECT_EQ(events.count(), 1);
    ASSERT_EQ(s.getPlotAppearances().size(), 1U);
    EXPECT_EQ(s.getPlotAppearances().begin()->first, "h");
    EXPECT_EQ(altitude.getSymbol(), "h");

    app.setLineStyle(LineStyle::SOLID);
    EXPECT_EQ(s.getPlotAppearance(altitude).value_or(app).getLineStyle(), LineStyle::DASHDOT)
        << "the simulation stored a copy";

    s.setPlotAppearance(altitude, PlotAppearance(std::nullopt, std::nullopt));
    EXPECT_FALSE(s.getPlotAppearance(altitude).has_value()) << "an empty appearance removes";
    EXPECT_EQ(events.count(), 2);
    s.setPlotAppearance(altitude, std::nullopt);
    EXPECT_EQ(events.count(), 3) << "announced although nothing was there";
    EXPECT_TRUE(s.getPlotAppearances().empty());
}

TEST(SimulationPlotAppearances, TheMapHandedOutIsACopy)
{
    TestEstesAlphaIII alpha;
    Simulation        s(*alpha.rocket);
    s.setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE),
                        PlotAppearance(Color{1, 2, 3}, std::nullopt));
    s.setPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z),
                        PlotAppearance(std::nullopt, LineStyle::DOTTED));
    std::map<std::string, PlotAppearance> copy = s.getPlotAppearances();
    ASSERT_EQ(copy.size(), 2U);
    copy.clear();
    EXPECT_EQ(s.getPlotAppearances().size(), 2U);
}

// ============================================================================ extensions

TEST(SimulationExtensions, CopyExtensionsFromTakesTheVeryObjects)
{
    // "copied: 2 same objects true list same false", "own list: 0"
    TestEstesAlphaIII                                       alpha;
    Simulation                                              s(*alpha.rocket);
    const std::shared_ptr<Ext>                              e1 = std::make_shared<Ext>();
    const std::shared_ptr<Ext>                              e2 = std::make_shared<Ext>();
    const std::vector<std::shared_ptr<SimulationExtension>> list{e1, e2};
    const ChangeCounter                                     events(s.changed());
    s.copyExtensionsFrom(list);
    ASSERT_EQ(s.getSimulationExtensions().size(), 2U);
    EXPECT_EQ(s.getSimulationExtensions()[0], e1);
    EXPECT_EQ(s.getSimulationExtensions()[1], e2);
    EXPECT_EQ(events.count(), 0) << "as in Java, nothing is announced";

    s.copyExtensionsFrom(s.getSimulationExtensions());
    EXPECT_TRUE(s.getSimulationExtensions().empty()) << "cleared before it is read, as in Java";
}

TEST(SimulationExtensions, ANullExtensionCannotBeCloned)
{
    TestEstesAlphaIII alpha;
    Simulation        s(*alpha.rocket);
    s.setFlightConfigurationId(testFcid(0));
    s.getSimulationExtensions().push_back(nullptr);
    const std::string text = "The simulation holds a null extension";
    EXPECT_EQ(bugText([&] { static_cast<void>(s.copy()); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(s.clone()); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(s.cloneForUndo()); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(s.duplicateSimulation(*alpha.rocket)); }), text);
    EXPECT_EQ(bugText([&] { static_cast<void>(s.duplicateForIndependentSimulation()); }), text);
    Result<void> result;
    EXPECT_EQ(bugText([&] { result = s.simulate(); }), text);
}

}  // namespace
