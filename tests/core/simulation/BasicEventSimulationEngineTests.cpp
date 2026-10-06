#include "QtRocket/simulation/BasicEventSimulationEngine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationEngine.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/exception/SimulationListenerException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestRockets.h"
#include "simulation/EngineTracePins.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::BasicEventSimulationEngine;
using QtRocket::CloneableSimulationListener;
using QtRocket::Coordinate;
using QtRocket::DeploymentConfiguration;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::Motor;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfigurationId;
using QtRocket::MotorMount;
using QtRocket::Parachute;
using QtRocket::RecoveryDevice;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationCalculationException;
using QtRocket::SimulationCancelledException;
using QtRocket::SimulationConditions;
using QtRocket::SimulationEngine;
using QtRocket::SimulationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationListenerException;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::StageSeparationConfiguration;
using QtRocket::Warning;
using QtRocket::Test::abortCauses;
using QtRocket::Test::bugText;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::EngineTracePin;
using QtRocket::Test::eventNames;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::kEngineTracePins;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMultiStageEventTestRocket;

using Type  = FlightEvent::Type;
using Cause = SimulationAbort::Cause;

// An engine is made for one run and stays where it is.
static_assert(!std::is_copy_constructible_v<BasicEventSimulationEngine>);
static_assert(!std::is_move_constructible_v<BasicEventSimulationEngine>);
static_assert(std::is_base_of_v<SimulationEngine, BasicEventSimulationEngine>);
static_assert(std::has_virtual_destructor_v<SimulationEngine>);

// =============================================================================== the trace
//
// The structure of a run, pinned against OpenRocket: probes/tier8b-engine/TraceProbe.java runs
// the scenarios below in Java with a user listener ("U") and a system listener ("S") that write
// down the hooks they are called with, and prints what the flight data hold afterwards
// (EngineTracePins.h). The same listeners and the same scenarios here have to give the same
// lines: the order of the listener calls, which of them reach the nested optimum-coast run (only
// the system listener's), the events of every branch in order with their sources and data, the
// warnings, and what happens to an exception.
//
// The per-step hooks and the ALTITUDE events are left out, and no line holds a step-dependent
// time, so the lines do not depend on the last bits of the mathematical functions.

/// What the two recording listeners of a run, and their clones, write to.
struct TraceLog
{
    std::vector<std::string> lines;
};

[[nodiscard]] std::string nameOf(const RocketComponent* component)
{
    return component == nullptr ? "null" : component->getName();
}

[[nodiscard]] std::string branchOf(const SimulationStatus& status)
{
    const std::shared_ptr<FlightDataBranch>& branch = status.getFlightDataBranch();
    return branch == nullptr ? "null" : branch->getName();
}

/// Java's e.getClass().getSimpleName() + "(" + e.getMessage() + ")", "null" for none.
[[nodiscard]] std::string exceptionText(const SimulationException* exception)
{
    if (exception == nullptr)
    {
        return "null";
    }
    std::string className = "SimulationException";
    if (dynamic_cast<const SimulationCalculationException*>(exception) != nullptr)
    {
        className = "SimulationCalculationException";
    }
    else if (dynamic_cast<const SimulationCancelledException*>(exception) != nullptr)
    {
        className = "SimulationCancelledException";
    }
    else if (dynamic_cast<const SimulationListenerException*>(exception) != nullptr)
    {
        className = "SimulationListenerException";
    }
    return std::format("{}({})", className, exception->getMessage().value_or("null"));
}

/// TraceProbe.Recorder: writes down the hooks. Its clones share the log.
class Recorder final : public CloneableSimulationListener<Recorder>
{
public:
    Recorder(std::string tag, std::shared_ptr<TraceLog> log, bool system)
      : m_tag(std::move(tag)), m_log(std::move(log)), m_system(system)
    {
    }

    [[nodiscard]] bool isSystemListener() const override { return m_system; }

    void startSimulation(SimulationStatus& status) override
    {
        add(std::format("startSimulation branch={}", branchOf(status)));
    }

    void endSimulation(SimulationStatus& status, const SimulationException* exception) override
    {
        add(std::format("endSimulation branch={} exception={}", branchOf(status),
                        exceptionText(exception)));
    }

    void startSimulationBranch(SimulationStatus& status) override
    {
        add(std::format("startSimulationBranch {}", branchOf(status)));
    }

    void endSimulationBranch(SimulationStatus&          status,
                             const SimulationException* exception) override
    {
        add(std::format("endSimulationBranch {} exception={}", branchOf(status),
                        exceptionText(exception)));
    }

    [[nodiscard]] bool addFlightEvent(SimulationStatus& /*status*/,
                                      const FlightEvent& event) override
    {
        if (event.getType() != Type::ALTITUDE)
        {
            add(std::format("addFlightEvent {} {}", name(event.getType()),
                            nameOf(event.getSource())));
        }
        return true;
    }

    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& event) override
    {
        if (event.getType() != Type::ALTITUDE)
        {
            add(std::format("handleFlightEvent {} {}", name(event.getType()),
                            nameOf(event.getSource())));
        }
        return true;
    }

    [[nodiscard]] bool motorIgnition(SimulationStatus& /*status*/,
                                     const MotorConfigurationId& /*motorId*/,
                                     const MotorMount& mount,
                                     MotorClusterState& /*instance*/) override
    {
        add(std::format("motorIgnition {}", asComponent(mount).getName()));
        return true;
    }

    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus& /*status*/,
                                                const RecoveryDevice& recoveryDevice) override
    {
        add(std::format("recoveryDeviceDeployment {}", recoveryDevice.getName()));
        return true;
    }

private:
    void add(const std::string& text) { m_log->lines.push_back(std::format("{} {}", m_tag, text)); }

    std::string               m_tag;
    std::shared_ptr<TraceLog> m_log;
    bool                      m_system;
};

/// TraceProbe.Thrower (SimulationBranchListenerTest's listener): throws at the third
/// postStep(). The count is a value member, as Java's int field: each clone goes on from the
/// count it was cloned with.
class Thrower final : public CloneableSimulationListener<Thrower>
{
public:
    void postStep(SimulationStatus& /*status*/) override
    {
        m_stepCount++;
        if (m_stepCount >= 3)
        {
            throw SimulationException("Test exception during branch execution");
        }
    }

private:
    int m_stepCount{0};
};

/// TraceProbe.VetoDeployment: refuses every recovery device deployment.
class VetoDeployment final : public CloneableSimulationListener<VetoDeployment>
{
public:
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus& /*status*/,
                                                const RecoveryDevice& /*device*/) override
    {
        return false;
    }
};

/// TraceProbe.VetoIgnition: refuses every motor ignition.
class VetoIgnition final : public CloneableSimulationListener<VetoIgnition>
{
public:
    [[nodiscard]] bool motorIgnition(SimulationStatus& /*status*/,
                                     const MotorConfigurationId& /*motorId*/,
                                     const MotorMount& /*mount*/,
                                     MotorClusterState& /*instance*/) override
    {
        return false;
    }
};

/// TraceProbe.NaNPosition: makes the position NaN after the third step.
class NaNPosition final : public CloneableSimulationListener<NaNPosition>
{
public:
    void postStep(SimulationStatus& status) override
    {
        m_stepCount++;
        if (m_stepCount >= 3)
        {
            status.setRocketPosition(Coordinate(std::numeric_limits<double>::quiet_NaN(), 0, 0));
        }
    }

private:
    int m_stepCount{0};
};

/// TraceProbe.ThrowAtStart: throws from startSimulation().
class ThrowAtStart final : public CloneableSimulationListener<ThrowAtStart>
{
public:
    void startSimulation(SimulationStatus& /*status*/) override
    {
        throw SimulationException("Thrown by startSimulation");
    }
};

/// TraceProbe.ThrowAtBranchStart: throws from startSimulationBranch().
class ThrowAtBranchStart final : public CloneableSimulationListener<ThrowAtBranchStart>
{
public:
    void startSimulationBranch(SimulationStatus& /*status*/) override
    {
        throw SimulationException("Thrown by startSimulationBranch");
    }
};

/// One scenario of TraceProbe: the rocket, the flight configuration, what is changed in the
/// options and the listeners that come before the two recorders.
struct Scenario
{
    std::unique_ptr<Rocket>                          rocket;
    FlightConfigurationId                            fcid{testFcid(0)};
    std::function<void(SimulationOptions&)>          tweak;
    std::vector<std::shared_ptr<SimulationListener>> first;
};

/// The Estes Alpha III in its first test configuration, after @p change has had the rocket.
[[nodiscard]] Scenario alphaScenario(const std::function<void(TestEstesAlphaIII&)>& change = {})
{
    TestEstesAlphaIII alpha;
    if (change)
    {
        change(alpha);
    }
    Scenario scenario;
    scenario.rocket = std::move(alpha.rocket);
    return scenario;
}

/// The Alpha III with @p listener before the recorders.
[[nodiscard]] Scenario alphaScenarioWith(std::shared_ptr<SimulationListener> listener)
{
    Scenario scenario = alphaScenario();
    scenario.first.push_back(std::move(listener));
    return scenario;
}

/// @p rocket with every stage of its selected configuration active, in that configuration.
[[nodiscard]] Scenario allStagesScenario(std::unique_ptr<Rocket> rocket)
{
    Scenario scenario;
    rocket->getSelectedConfiguration().setAllStages();
    scenario.fcid   = rocket->getSelectedConfiguration().getFlightConfigurationId();
    scenario.rocket = std::move(rocket);
    return scenario;
}

[[nodiscard]] Scenario alphaFcid3()
{
    Scenario scenario = alphaScenario();
    scenario.fcid     = testFcid(3);
    return scenario;
}

[[nodiscard]] Scenario beta()
{
    TestBeta fixture;
    return allStagesScenario(std::move(fixture.rocket));
}

[[nodiscard]] Scenario multiStage()
{
    TestMultiStageEventTestRocket fixture;
    return allStagesScenario(std::move(fixture.rocket));
}

[[nodiscard]] Scenario alphaNoMotors()
{
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllMotors();
    });
}

[[nodiscard]] Scenario alphaNoActiveStage()
{
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllStages();
    });
}

[[nodiscard]] Scenario alphaWithoutParachute()
{
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        static_cast<void>(alpha.body->removeChild(alpha.chute));
    });
}

[[nodiscard]] Scenario alphaDrogueOnly()
{
    return alphaScenario([](const TestEstesAlphaIII& alpha) { alpha.chute->setDrogue(true); });
}

[[nodiscard]] Scenario alphaDrogueAndMain()
{
    // a drogue and a main in the same stage: the main deploys fast
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        auto drogue = std::make_unique<Parachute>();
        drogue->setName("Drogue");
        drogue->setDrogue(true);
        DeploymentConfiguration atApogee;
        atApogee.setDeployEvent(DeploymentConfiguration::DeployEvent::APOGEE);
        drogue->getDeploymentConfigurations().setDefault(atApogee);
        alpha.body->addChild(std::move(drogue));
    });
}

[[nodiscard]] Scenario alphaDeploymentUnderThrust()
{
    // deployment at launch: before the launch rod is cleared and under thrust
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        DeploymentConfiguration atLaunch;
        atLaunch.setDeployEvent(DeploymentConfiguration::DeployEvent::LAUNCH);
        atLaunch.setDeployDelay(0.5);
        alpha.chute->getDeploymentConfigurations().setDefault(atLaunch);
    });
}

[[nodiscard]] Scenario alphaMaxTime()
{
    Scenario scenario = alphaScenario();
    scenario.tweak    = [](SimulationOptions& options) { options.setMaxSimulationTime(1.0); };
    return scenario;
}

[[nodiscard]] Scenario alphaRk6()
{
    Scenario scenario = alphaScenario();
    scenario.tweak    = [](SimulationOptions& options) {
        options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    };
    return scenario;
}

[[nodiscard]] Scenario alphaTooHeavy()
{
    // too heavy to lift off: the motor burns out on the ground
    return alphaScenario([](const TestEstesAlphaIII& alpha) {
        alpha.body->setMassOverridden(true);
        alpha.body->setOverrideMass(10.0);
    });
}

[[nodiscard]] Scenario betaSeparationAtLaunch()
{
    // a second stage that separates at launch, before the rod is cleared
    TestBeta                     fixture;
    Scenario                     scenario = allStagesScenario(std::move(fixture.rocket));
    StageSeparationConfiguration sep;
    sep.setSeparationEvent(StageSeparationConfiguration::SeparationEvent::LAUNCH);
    fixture.boosterStage->getSeparationConfigurations().setDefault(sep);
    fixture.boosterStage->getSeparationConfigurations().set(scenario.fcid, sep);
    scenario.tweak = [](SimulationOptions& options) {
        options.setLaunchRodLength(1.0);
        options.setMaxSimulationTime(5.0);
    };
    return scenario;
}

[[nodiscard]] Scenario betaBoosterOnly()
{
    // the booster of the Beta alone: a branch for a stage that is not the topmost one
    TestBeta fixture;
    Scenario scenario;
    fixture.rocket->getSelectedConfiguration().setOnlyStage(1);
    scenario.fcid   = fixture.rocket->getSelectedConfiguration().getFlightConfigurationId();
    scenario.rocket = std::move(fixture.rocket);
    return scenario;
}

/// The scenarios of TraceProbe.main(), by name.
[[nodiscard]] const std::map<std::string_view, std::function<Scenario()>>& scenarioMakers()
{
    static const std::map<std::string_view, std::function<Scenario()>> kMakers{
        {"alpha fcid0", [] { return alphaScenario(); }},
        {"alpha fcid3 (deployment after apogee)", alphaFcid3},
        {"beta", beta},
        {"multi stage", multiStage},
        {"alpha thrower", [] { return alphaScenarioWith(std::make_shared<Thrower>()); }},
        {"alpha no motors", alphaNoMotors},
        {"alpha no active stage", alphaNoActiveStage},
        {"alpha without parachute", alphaWithoutParachute},
        {"alpha drogue only", alphaDrogueOnly},
        {"alpha drogue and main", alphaDrogueAndMain},
        {"alpha deployment under thrust", alphaDeploymentUnderThrust},
        {"alpha max time 1 s", alphaMaxTime},
        {"alpha rk6", alphaRk6},
        {"alpha too heavy", alphaTooHeavy},
        {"alpha deployment refused",
         [] { return alphaScenarioWith(std::make_shared<VetoDeployment>()); }},
        {"alpha ignition refused",
         [] { return alphaScenarioWith(std::make_shared<VetoIgnition>()); }},
        {"alpha NaN position", [] { return alphaScenarioWith(std::make_shared<NaNPosition>()); }},
        {"alpha exception at start",
         [] { return alphaScenarioWith(std::make_shared<ThrowAtStart>()); }},
        {"alpha exception at branch start",
         [] { return alphaScenarioWith(std::make_shared<ThrowAtBranchStart>()); }},
        {"beta separation at launch", betaSeparationAtLaunch},
        {"beta booster only", betaBoosterOnly},
    };
    return kMakers;
}

/// TraceProbe.eventData(): what an event carries.
[[nodiscard]] std::string eventData(const FlightEvent& event, const FlightData& data)
{
    if (const std::shared_ptr<MotorClusterState> state = event.getMotorState())
    {
        const Motor& motor = *state->getMotor();
        return std::format("motor:{}", motor.getDesignation());
    }
    if (const SimulationAbort* abort = event.getAbort())
    {
        return std::format("abort:{}", causeName(abort->cause()));
    }
    if (const Warning* warning = data.findWarning(event))
    {
        return std::format("warning:{}", warning->typeName());
    }
    if (const std::string* text = event.getMessage())
    {
        return std::format("text:{}", *text);
    }
    return event.hasData() ? "other" : "null";
}

/// The lines of TraceProbe.run() after the listeners' lines: the outcome, the status and what
/// the flight data hold.
void addOutcome(std::vector<std::string>& lines, Simulation& sim, const Result<void>& result)
{
    if (result.has_value())
    {
        lines.emplace_back("thrown none");
    }
    else
    {
        lines.push_back(std::format(
            "thrown {}: {}",
            result.error().code == ErrorCode::CANCELLED ? "CANCELLED" : "SIMULATION_ABORTED",
            result.error().message));
    }
    lines.push_back(std::format("status {}", name(sim.getStatus())));
    const std::shared_ptr<FlightData>& data = sim.getSimulatedData();
    if (data == nullptr)
    {
        lines.emplace_back("data null");
        return;
    }
    lines.push_back(std::format("branches {}", data->getBranchCount()));
    for (std::size_t b = 0; b < data->getBranchCount(); b++)
    {
        const FlightDataBranch& branch = data->getBranch(b);
        lines.push_back(std::format("branch {} {} empty={} optimumAltitudeSet={}", b,
                                    branch.getName(), branch.getLength() == 0,
                                    !std::isnan(branch.getOptimumAltitude())));
        for (const FlightEvent& event : branch.getEvents())
        {
            lines.push_back(std::format("  event {} {} {}", name(event.getType()),
                                        nameOf(event.getSource()), eventData(event, *data)));
        }
    }
    for (const Warning& warning : data->getWarningSet())
    {
        const bool other = dynamic_cast<const Warning::Other*>(&warning) != nullptr;
        lines.push_back(std::format("warning {}{}", warning.typeName(),
                                    other ? ": " + warning.messageDescription() : ""));
    }
    lines.push_back(std::format("summary maxAltitudeNaN={} flightTimeNaN={} timeToApogeeNaN={}",
                                std::isnan(data->getMaxAltitude()),
                                std::isnan(data->getFlightTime()),
                                std::isnan(data->getTimeToApogee())));
}

/// TraceProbe.run(): the lines of the scenario @p name.
[[nodiscard]] std::vector<std::string> traceOf(std::string_view name)
{
    const auto maker = scenarioMakers().find(name);
    if (maker == scenarioMakers().end())
    {
        return {std::format("no scenario '{}'", name)};
    }
    const Scenario scenario = maker->second();

    JavaTestPreferences preferences;
    Simulation          sim(*scenario.rocket, preferences.store);
    sim.setFlightConfigurationId(scenario.fcid);
    sim.getOptions().setIsaAtmosphere(true);
    sim.getOptions().setTimeStep(0.05);
    sim.getOptions().setRandomSeed(0);
    if (scenario.tweak)
    {
        scenario.tweak(sim.getOptions());
    }

    const std::shared_ptr<TraceLog>                  log       = std::make_shared<TraceLog>();
    std::vector<std::shared_ptr<SimulationListener>> listeners = scenario.first;
    listeners.push_back(std::make_shared<Recorder>("U", log, false));
    listeners.push_back(std::make_shared<Recorder>("S", log, true));

    const Result<void> result = sim.simulate(listeners);

    std::vector<std::string> lines = log->lines;
    addOutcome(lines, sim, result);
    return lines;
}

/// The pinned lines of @p pin as strings.
[[nodiscard]] std::vector<std::string> pinnedLines(const EngineTracePin& pin)
{
    return {pin.lines.begin(), pin.lines.end()};
}

/// @p lines with every run of consecutive "addFlightEvent IGNITION" lines sorted. The motors
/// that one event ignites are queued in the order of the status's motor states: in Java the
/// hash order of the configuration's motor map, which differs from run to run (TraceProbe
/// prints either order), here the order of the mounts in the component tree. The order in which
/// the ignitions are then handled is that of the event queue, and is compared as it is.
[[nodiscard]] std::vector<std::string> withQueuedIgnitionsSorted(std::vector<std::string> lines)
{
    const auto isQueuedIgnition = [](const std::string& line) {
        return line.contains(" addFlightEvent IGNITION ");
    };
    auto first = std::ranges::find_if(lines, isQueuedIgnition);
    while (first != lines.end())
    {
        const auto last = std::find_if_not(first, lines.end(), isQueuedIgnition);
        std::sort(first, last);
        first = std::find_if(last, lines.end(), isQueuedIgnition);
    }
    return lines;
}

/// The scenario of @p pin as a test name.
[[nodiscard]] std::string testName(const EngineTracePin& pin)
{
    std::string text;
    bool        upper = true;
    for (const char c : pin.scenario)
    {
        const bool alnum =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum)
        {
            upper = true;
            continue;
        }
        text += (upper && c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
        upper = false;
    }
    return text;
}

class EngineTrace : public ::testing::TestWithParam<EngineTracePin>
{
private:
    /// A warning's text holds a speed or an angle in the default units.
    DefaultUnitsGuard m_units;
};

TEST_P(EngineTrace, TheRunHasTheStructureOfOpenRockets)
{
    EXPECT_EQ(withQueuedIgnitionsSorted(traceOf(GetParam().scenario)),
              withQueuedIgnitionsSorted(pinnedLines(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(Scenarios, EngineTrace, ::testing::ValuesIn(kEngineTracePins),
                         [](const ::testing::TestParamInfo<EngineTracePin>& paramInfo) {
                             return testName(paramInfo.param);
                         });

/// The names of the pinned scenarios, in order.
[[nodiscard]] std::vector<std::string> pinnedScenarios()
{
    std::vector<std::string> names;
    names.reserve(kEngineTracePins.size());
    for (const EngineTracePin& pin : kEngineTracePins)
    {
        names.emplace_back(pin.scenario);
    }
    return names;
}

/// The names of the scenarios this file can build, sorted.
[[nodiscard]] std::vector<std::string> knownScenarios()
{
    std::vector<std::string> names;
    names.reserve(scenarioMakers().size());
    for (const auto& [name, maker] : scenarioMakers())
    {
        names.emplace_back(name);
    }
    return names;
}

// The canonical order of the queued ignitions: only runs of them are sorted.
TEST(EngineTraceScenarios, OnlyRunsOfQueuedIgnitionsAreSorted)
{
    const std::vector<std::string> lines{
        "U handleFlightEvent LAUNCH Rocket", "U addFlightEvent IGNITION b",
        "S addFlightEvent IGNITION b",       "U addFlightEvent IGNITION a",
        "S addFlightEvent IGNITION a",       "U handleFlightEvent IGNITION b",
        "U addFlightEvent IGNITION z",       "U handleFlightEvent IGNITION a",
        "U addFlightEvent IGNITION y",       "U addFlightEvent IGNITION x",
    };
    const std::vector<std::string> expected{
        "U handleFlightEvent LAUNCH Rocket", "S addFlightEvent IGNITION a",
        "S addFlightEvent IGNITION b",       "U addFlightEvent IGNITION a",
        "U addFlightEvent IGNITION b",       "U handleFlightEvent IGNITION b",
        "U addFlightEvent IGNITION z",       "U handleFlightEvent IGNITION a",
        "U addFlightEvent IGNITION x",       "U addFlightEvent IGNITION y",
    };
    EXPECT_EQ(withQueuedIgnitionsSorted(lines), expected);
    EXPECT_EQ(withQueuedIgnitionsSorted({}), std::vector<std::string>{});
}

// A regenerated EngineTracePins.h cannot silently lose a scenario, or bring one this file does
// not build.
TEST(EngineTraceScenarios, ThePinsCoverTheScenarios)
{
    const std::vector<std::string> expected{
        "alpha fcid0",
        "alpha fcid3 (deployment after apogee)",
        "beta",
        "multi stage",
        "alpha thrower",
        "alpha no motors",
        "alpha no active stage",
        "alpha without parachute",
        "alpha drogue only",
        "alpha drogue and main",
        "alpha deployment under thrust",
        "alpha max time 1 s",
        "alpha rk6",
        "alpha too heavy",
        "alpha deployment refused",
        "alpha ignition refused",
        "alpha NaN position",
        "alpha exception at start",
        "alpha exception at branch start",
        "beta separation at launch",
        "beta booster only",
    };
    EXPECT_EQ(pinnedScenarios(), expected);

    std::vector<std::string> sorted = expected;
    std::ranges::sort(sorted);
    EXPECT_EQ(knownScenarios(), sorted);
}

// =============================================================================== the engine

/// A simulation of the Estes Alpha III as the Java tests set one up (SimulationTest.setUpSim()),
/// with the random seed 0, and the conditions of a run of it.
struct AlphaRun
{
    JavaTestPreferences                   preferences;
    TestEstesAlphaIII                     alpha;
    Simulation                            simulation;
    std::shared_ptr<SimulationConditions> conditions;

    explicit AlphaRun(int configuration = 0) : simulation(*alpha.rocket, preferences.store)
    {
        simulation.setFlightConfigurationId(testFcid(configuration));
        simulation.getOptions().setIsaAtmosphere(true);
        simulation.getOptions().setTimeStep(0.05);
        simulation.getOptions().setRandomSeed(0);
    }

    /// Makes the conditions of the simulation's options (what Simulation::simulate() does
    /// before it runs the engine).
    void makeConditions()
    {
        Result<SimulationConditions> made = simulation.getOptions().toSimulationConditions();
        ASSERT_TRUE(made.has_value());
        conditions = std::make_shared<SimulationConditions>(std::move(*made));
        conditions->setSimulation(&simulation);
    }
};

TEST(BasicEventSimulationEngine, HasNoFlightDataBeforeItRuns)
{
    const BasicEventSimulationEngine engine;
    EXPECT_EQ(engine.getFlightData(), nullptr);

    const SimulationEngine& base = engine;
    EXPECT_EQ(base.getFlightData(), nullptr);
}

TEST(BasicEventSimulationEngine, TheConstantsAreOpenRockets)
{
    // THRUST_TUMBLE_CONDITION, and the texts of BasicEventSimulationEngine.nullBranchName and
    // BasicEventSimulationEngine.error.NaNResult in messages.properties.
    EXPECT_EQ(BasicEventSimulationEngine::kThrustTumbleCondition, 0.01);
    EXPECT_STREQ(BasicEventSimulationEngine::kNullBranchName, "(null)");
    EXPECT_STREQ(BasicEventSimulationEngine::kNaNResult,
                 "Simulation resulted in not-a-number (NaN) value, please report a bug.");
}

TEST(BasicEventSimulationEngine, NeedsConditionsThatBelongToASimulation)
{
    BasicEventSimulationEngine engine;
    EXPECT_EQ(bugText([&engine] { engine.simulate(nullptr); }),
              "The simulation engine needs simulation conditions");
    EXPECT_EQ(engine.getFlightData(), nullptr);

    // Java: a NullPointerException, after the flight data were made.
    EXPECT_EQ(bugText([&engine] { engine.simulate(std::make_shared<SimulationConditions>()); }),
              "The simulation conditions have no simulation");
    ASSERT_NE(engine.getFlightData(), nullptr);
    EXPECT_EQ(engine.getFlightData()->getBranchCount(), 0U);
}

TEST(BasicEventSimulationEngine, SimulatesThroughTheEngineInterface)
{
    AlphaRun run;
    run.makeConditions();
    ASSERT_NE(run.conditions, nullptr);

    BasicEventSimulationEngine engine;
    SimulationEngine&          base = engine;
    base.simulate(run.conditions);

    const std::shared_ptr<FlightData>& data = base.getFlightData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(eventNames(*data, 0),
              (std::vector<std::string>{"LAUNCH", "IGNITION", "LIFTOFF", "LAUNCHROD", "BURNOUT",
                                        "EJECTION_CHARGE", "SIM_WARN", "RECOVERY_DEVICE_DEPLOYMENT",
                                        "APOGEE", "GROUND_HIT", "SIMULATION_END"}));
    // calculateInterestingValues() ran (Java: the finally block).
    EXPECT_GT(data->getMaxAltitude(), 0.0);
    EXPECT_GT(data->getFlightTime(), 0.0);
    // The branch of a finished run is immutable; the flight data are not (as in Java).
    EXPECT_FALSE(data->getBranch(0).isMutable());
    EXPECT_TRUE(data->isMutable());
}

/// The state of the caller's rocket that a simulation must leave alone.
struct RocketState
{
    ModId                 modId;
    ModId                 configurationModId;
    FlightConfigurationId selected;
    std::size_t           children;

    explicit RocketState(const Rocket& rocket)
      : modId(rocket.getModId()),
        configurationModId(rocket.getFlightConfiguration(testFcid(0)).getModId()),
        selected(rocket.getSelectedConfiguration().getFlightConfigurationId()),
        children(rocket.getChildCount())
    {
    }

    [[nodiscard]] bool operator==(const RocketState&) const = default;
};

TEST(BasicEventSimulationEngine, SimulatesACopyAndLeavesTheCallersRocketAlone)
{
    AlphaRun run;
    run.makeConditions();
    ASSERT_NE(run.conditions, nullptr);
    const Rocket&     rocket = *run.alpha.rocket;
    const RocketState before(rocket);
    ASSERT_NE(before.selected, testFcid(0)) << "the simulated configuration is not the selected";

    std::shared_ptr<FlightData> data;
    {
        BasicEventSimulationEngine engine;
        engine.simulate(run.conditions);
        data = engine.getFlightData();
    }
    ASSERT_NE(data, nullptr);

    // Nothing of the caller's rocket has changed: not the tree, not a configuration, not the
    // selection (the copy selects the simulated configuration, the original keeps its own).
    EXPECT_TRUE(RocketState(rocket) == before);

    // The flight data co-own the rocket that was simulated, and every source of an event is a
    // component of that rocket: the events are usable after the engine is gone.
    const std::shared_ptr<const Rocket>& simulated = data->getSimulatedRocket();
    ASSERT_NE(simulated, nullptr);
    EXPECT_NE(simulated.get(), &rocket);
    EXPECT_EQ(simulated->getId(), rocket.getId());
    EXPECT_EQ(simulated->getSelectedConfiguration().getFlightConfigurationId(), testFcid(0));
    for (const FlightEvent& event : data->getBranch(0).getEvents())
    {
        if (event.getSource() != nullptr)
        {
            EXPECT_EQ(&event.getSource()->getRocket(), simulated.get()) << event.toString();
        }
    }

    // Deviation: the LAUNCH event's source is the simulated copy (Java: the caller's rocket),
    // which has the same id.
    const FlightEvent* launch = data->getBranch(0).getFirstEvent(Type::LAUNCH);
    ASSERT_NE(launch, nullptr);
    EXPECT_EQ(launch->getSource(), simulated.get());
    EXPECT_EQ(launch->getSourceId(), rocket.getId());

    // The branch is named after the topmost stage and knows it by id.
    EXPECT_EQ(data->getBranch(0).getName(), run.alpha.stage->getName());
    EXPECT_EQ(data->getBranch(0).getSourceComponentId(), run.alpha.stage->getId());
}

/// Counts the aerodynamic calculations of the first simulation step. The count is shared by the
/// clones.
class FirstStepCounter final : public CloneableSimulationListener<FirstStepCounter>
{
public:
    struct Counts
    {
        int steps{0};
        int calculations{0};
    };

    explicit FirstStepCounter(std::shared_ptr<Counts> counts) : m_counts(std::move(counts)) { }

    void postStep(SimulationStatus& /*status*/) override { m_counts->steps++; }

    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& /*status*/, const AerodynamicForces& /*forces*/) override
    {
        if (m_counts->steps == 0)
        {
            m_counts->calculations++;
        }
        return std::nullopt;
    }

private:
    std::shared_ptr<Counts> m_counts;
};

/// The number of aerodynamic calculations in the first step of a run with @p method.
[[nodiscard]] int firstStepCalculations(SimulationStepperMethod method)
{
    AlphaRun run;
    run.simulation.getOptions().setSimulationStepperMethodChoice(method);
    const auto         counts = std::make_shared<FirstStepCounter::Counts>();
    const Result<void> result =
        run.simulation.simulate({std::make_shared<FirstStepCounter>(counts)});
    EXPECT_TRUE(result.has_value());
    return counts->calculations;
}

// The flight stepper is the options' choice: four force evaluations per step for RK4, seven
// for RK6.
TEST(BasicEventSimulationEngine, TakesTheFlightStepperOfTheOptions)
{
    EXPECT_EQ(firstStepCalculations(SimulationStepperMethod::RK4), 4);
    EXPECT_EQ(firstStepCalculations(SimulationStepperMethod::RK6), 7);
}

// An engine that is used again starts afresh: new flight data, the same flight.
TEST(BasicEventSimulationEngine, AnEngineUsedAgainStartsAfresh)
{
    AlphaRun run;
    run.makeConditions();
    ASSERT_NE(run.conditions, nullptr);

    BasicEventSimulationEngine engine;
    engine.simulate(run.conditions);
    const std::shared_ptr<FlightData> first = engine.getFlightData();
    engine.simulate(run.conditions);
    const std::shared_ptr<FlightData> second = engine.getFlightData();

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(first->getBranchCount(), 1U);
    EXPECT_EQ(second->getBranchCount(), 1U);
    EXPECT_EQ(eventNames(*first, 0), eventNames(*second, 0));
    EXPECT_NE(first->getSimulatedRocket(), second->getSimulatedRocket());
    // The two runs drew the same jitter (the stepper is reseeded) and saw the same calm air.
    EXPECT_EQ(first->getMaxAltitude(), second->getMaxAltitude());
    EXPECT_EQ(first->getFlightTime(), second->getFlightTime());
}

// When the recovery device opens before apogee, the optimum altitude is that of the nested
// coast run: higher and later than the apogee of the flight itself.
TEST(BasicEventSimulationEngine, TheOptimumAltitudeComesFromTheCoastRun)
{
    AlphaRun run;
    simulateOrFail(run.simulation);
    const FlightData&       data   = simulatedData(run.simulation);
    const FlightDataBranch& branch = data.getBranch(0);

    const FlightEvent* deployment = branch.getFirstEvent(Type::RECOVERY_DEVICE_DEPLOYMENT);
    const FlightEvent* apogee     = branch.getFirstEvent(Type::APOGEE);
    ASSERT_NE(deployment, nullptr);
    ASSERT_NE(apogee, nullptr);
    ASSERT_LT(deployment->getTime(), apogee->getTime());

    EXPECT_GT(branch.getOptimumAltitude(), data.getMaxAltitude() + 10.0);
    EXPECT_GT(branch.getTimeToOptimumAltitude(), data.getTimeToApogee() + 1.0);
    // FlightDataBranch::getOptimumDelay(): from the burnout to the optimum altitude.
    const FlightEvent* burnout = branch.getLastEvent(Type::BURNOUT);
    ASSERT_NE(burnout, nullptr);
    EXPECT_EQ(data.getOptimumDelay(), branch.getTimeToOptimumAltitude() - burnout->getTime());
}

// When the recovery device opens after apogee, the optimum altitude is the apogee of the
// flight: the maximum altitude, at the time of the step before the one that found it.
TEST(BasicEventSimulationEngine, TheOptimumAltitudeIsTheApogeeWhenNothingOpensBefore)
{
    AlphaRun run(3);
    simulateOrFail(run.simulation);
    const FlightData&       data   = simulatedData(run.simulation);
    const FlightDataBranch& branch = data.getBranch(0);

    const FlightEvent* deployment = branch.getFirstEvent(Type::RECOVERY_DEVICE_DEPLOYMENT);
    const FlightEvent* apogee     = branch.getFirstEvent(Type::APOGEE);
    ASSERT_NE(deployment, nullptr);
    ASSERT_NE(apogee, nullptr);
    ASSERT_GT(deployment->getTime(), apogee->getTime());

    EXPECT_EQ(branch.getOptimumAltitude(), data.getMaxAltitude());
    EXPECT_EQ(branch.getTimeToOptimumAltitude(), apogee->getTime());
}

// The end of a branch at the maximum simulation time: a SIMULATION_END event at the current
// time goes straight to the branch, and the time of the last record is below the limit.
TEST(BasicEventSimulationEngine, EndsABranchAtTheMaximumSimulationTime)
{
    AlphaRun run;
    run.simulation.getOptions().setMaxSimulationTime(1.0);
    simulateOrFail(run.simulation);
    const FlightDataBranch& branch = simulatedData(run.simulation).getBranch(0);

    const FlightEvent* end = branch.getLastEvent(Type::SIMULATION_END);
    ASSERT_NE(end, nullptr);
    EXPECT_GE(end->getTime(), 1.0);
    EXPECT_LT(end->getTime(), 1.0 + 0.05);
    EXPECT_LT(branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)), 1.0);
    EXPECT_EQ(run.simulation.getStatus(), Simulation::Status::UPTODATE);
}

// A simulation that cannot start: the aborts of the sanity checks, in OpenRocket's order.
TEST(BasicEventSimulationEngine, AbortsARocketWithoutMotors)
{
    AlphaRun run;
    run.alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllMotors();
    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    EXPECT_EQ(abortCauses(data, 0), std::vector<Cause>{Cause::NO_MOTORS_DEFINED});
    EXPECT_TRUE(run.simulation.hasErrors());
    // The abort stores the status once: one record.
    EXPECT_EQ(data.getBranch(0).getLength(), 1U);
}

TEST(BasicEventSimulationEngine, AbortsARocketWithoutAnActiveStage)
{
    AlphaRun run;
    run.alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllStages();
    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    ASSERT_EQ(data.getBranchCount(), 1U);
    EXPECT_EQ(data.getBranch(0).getName(), "(null)");
    EXPECT_EQ(data.getBranch(0).getSourceComponentId(), std::nullopt);
    // Handled in the order of the event queue: the three aborts compare equal (the same time,
    // no source, the same type) and leave the queue in the order of Java's heap.
    EXPECT_EQ(abortCauses(data, 0),
              (std::vector<Cause>{Cause::NO_MOTORS_DEFINED, Cause::ACTIVE_LENGTH_ZERO,
                                  Cause::NO_ACTIVE_STAGES}));
    EXPECT_TRUE(data.getWarningSet().contains(Warning::kNoRecoveryDevice));
}

/// A listener that raises its flag (shared by the clones) when it is asked to.
class StopAfterSteps final : public CloneableSimulationListener<StopAfterSteps>
{
public:
    StopAfterSteps(std::shared_ptr<int> steps, int limit)
      : m_steps(std::move(steps)), m_limit(limit)
    {
    }

    void postStep(SimulationStatus& /*status*/) override
    {
        if (++*m_steps >= m_limit)
        {
            throw SimulationCancelledException("stopped by the test");
        }
    }

private:
    std::shared_ptr<int> m_steps;
    int                  m_limit;
};

// An exception leaves simulate() with its dynamic type, after the flight data took what was
// simulated: the EXCEPTION event with the message, and the summary values.
TEST(BasicEventSimulationEngine, AnExceptionLeavesWithItsTypeAndTheDataSoFar)
{
    AlphaRun run;
    run.makeConditions();
    ASSERT_NE(run.conditions, nullptr);
    const auto steps = std::make_shared<int>(0);
    run.conditions->getSimulationListenerList().push_back(
        std::make_shared<StopAfterSteps>(steps, 20));

    BasicEventSimulationEngine engine;
    EXPECT_THROW(engine.simulate(run.conditions), SimulationCancelledException);
    EXPECT_EQ(*steps, 20);

    const std::shared_ptr<FlightData>& data = engine.getFlightData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightDataBranch& branch = data->getBranch(0);
    ASSERT_FALSE(branch.getEvents().empty());
    const FlightEvent& last = branch.getEvents().back();
    EXPECT_EQ(last.getType(), Type::EXCEPTION);
    ASSERT_NE(last.getMessage(), nullptr);
    EXPECT_EQ(*last.getMessage(), "stopped by the test");
    EXPECT_EQ(last.getSource(), data->getSimulatedRocket().get());
    EXPECT_EQ(last.getTime(),
              branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)) +
                  branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP)));
    // The branch of a run that failed stays mutable (Java does not reach immute()).
    EXPECT_TRUE(branch.isMutable());
    EXPECT_EQ(branch.getLength(), 20U);
    EXPECT_EQ(data->getFlightTime(),
              branch.getLast(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)));
}

/// A listener without a message in its exception.
class ThrowWithoutMessage final : public CloneableSimulationListener<ThrowWithoutMessage>
{
public:
    void postStep(SimulationStatus& /*status*/) override { throw SimulationException(); }
};

// The EXCEPTION event of an exception without a message carries no data (Java: a null
// message).
TEST(BasicEventSimulationEngine, AnExceptionWithoutAMessageGivesAnEventWithoutData)
{
    AlphaRun           run;
    const Result<void> result = run.simulation.simulate({std::make_shared<ThrowWithoutMessage>()});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);

    const FlightDataBranch&         branch = simulatedData(run.simulation).getBranch(0);
    const std::vector<FlightEvent>& events = branch.getEvents();
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().getType(), Type::EXCEPTION);
    EXPECT_FALSE(events.back().hasData());
}

// The exception of a computation that went wrong carries the branch that was being written.
TEST(BasicEventSimulationEngine, ANaNInTheStatusIsACalculationExceptionWithTheBranch)
{
    AlphaRun run;
    run.makeConditions();
    ASSERT_NE(run.conditions, nullptr);
    run.conditions->getSimulationListenerList().push_back(std::make_shared<NaNPosition>());

    BasicEventSimulationEngine        engine;
    std::shared_ptr<FlightDataBranch> carried;
    std::string                       message;
    try
    {
        engine.simulate(run.conditions);
    }
    catch (const SimulationCalculationException& e)
    {
        carried = e.getFlightDataBranch();
        message = e.what();
    }
    ASSERT_NE(engine.getFlightData(), nullptr);
    ASSERT_EQ(engine.getFlightData()->getBranchCount(), 1U);
    EXPECT_EQ(message, BasicEventSimulationEngine::kNaNResult);
    EXPECT_EQ(carried, engine.getFlightData()->getBranches().front());
}

/// Takes the listeners of the status's conditions down when it is first called, to see that
/// the engine reads the maximum simulation time from the conditions it was given.
class ShortenTheFlight final : public CloneableSimulationListener<ShortenTheFlight>
{
public:
    void postStep(SimulationStatus& status) override
    {
        // The conditions of the status are a clone: this changes the clone only.
        status.getSimulationConditions()->setMaxSimulationTime(0.1);
    }
};

// Java's handleEvents() reads the maximum simulation time from the conditions the engine was
// given, not from the clone the status of a branch holds.
TEST(BasicEventSimulationEngine, TheMaximumTimeIsThatOfTheConditionsGivenToTheEngine)
{
    AlphaRun           run;
    const Result<void> result = run.simulation.simulate({std::make_shared<ShortenTheFlight>()});
    ASSERT_TRUE(result.has_value());

    const FlightDataBranch& branch = simulatedData(run.simulation).getBranch(0);
    EXPECT_NE(branch.getFirstEvent(Type::GROUND_HIT), nullptr);
    EXPECT_GT(simulatedData(run.simulation).getFlightTime(), 10.0);
}

// Two simulations of two copies of a rocket give the same flight: nothing of an engine is
// shared between runs.
TEST(BasicEventSimulationEngine, TwoRunsOfTheSameDesignAreTheSameFlight)
{
    AlphaRun first;
    AlphaRun second;
    simulateOrFail(first.simulation);
    simulateOrFail(second.simulation);

    const FlightData& a = simulatedData(first.simulation);
    const FlightData& b = simulatedData(second.simulation);
    EXPECT_EQ(a.getBranch(0).getLength(), b.getBranch(0).getLength());
    EXPECT_EQ(a.getMaxAltitude(), b.getMaxAltitude());
    EXPECT_EQ(a.getMaxVelocity(), b.getMaxVelocity());
    EXPECT_EQ(a.getFlightTime(), b.getFlightTime());
    EXPECT_EQ(a.getGroundHitVelocity(), b.getGroundHitVelocity());
}

/// Runs the simulation of @p run and writes down whether it succeeded.
void simulateInto(AlphaRun& run, bool& succeeded)
{
    succeeded = run.simulation.simulate().has_value();
}

// Two simulations of two rockets run on two threads at the same time (plan section 7): an
// engine, its steppers and the calculators hold no static or shared state, so each run is the
// flight a run on its own gives.
TEST(BasicEventSimulationEngine, TwoEnginesRunAtTheSameTimeOnTwoRockets)
{
    AlphaRun alone;
    simulateOrFail(alone.simulation);

    // Everything a run touches is made before the threads start, each for one thread only.
    AlphaRun first;
    AlphaRun second(3);
    AlphaRun third;
    bool     firstSucceeded  = false;
    bool     secondSucceeded = false;
    bool     thirdSucceeded  = false;
    {
        const std::jthread a(simulateInto, std::ref(first), std::ref(firstSucceeded));
        const std::jthread b(simulateInto, std::ref(second), std::ref(secondSucceeded));
        const std::jthread c(simulateInto, std::ref(third), std::ref(thirdSucceeded));
    }  // joins
    ASSERT_TRUE(firstSucceeded);
    ASSERT_TRUE(secondSucceeded);
    ASSERT_TRUE(thirdSucceeded);

    const FlightData& expected = simulatedData(alone.simulation);
    for (const AlphaRun* run : {&first, &third})
    {
        const FlightData& data = simulatedData(run->simulation);
        EXPECT_EQ(data.getBranch(0).getLength(), expected.getBranch(0).getLength());
        EXPECT_EQ(data.getMaxAltitude(), expected.getMaxAltitude());
        EXPECT_EQ(data.getMaxVelocity(), expected.getMaxVelocity());
        EXPECT_EQ(data.getFlightTime(), expected.getFlightTime());
        EXPECT_EQ(data.getGroundHitVelocity(), expected.getGroundHitVelocity());
    }
    // The run of the other configuration, between the two, is a flight of its own.
    EXPECT_NE(simulatedData(second.simulation).getMaxAltitude(), expected.getMaxAltitude());
}

}  // namespace
