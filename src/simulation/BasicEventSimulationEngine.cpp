#include "QtRocket/simulation/BasicEventSimulationEngine.h"

#include <cmath>
#include <deque>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/BasicLandingStepper.h"
#include "QtRocket/simulation/BasicTumbleStepper.h"
#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/FlightEventActivation.h"
#include "QtRocket/simulation/GroundStepper.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Rk6SimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/TumbleDetector.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/simulation/listeners/system/OptimumCoastListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Quaternion.h"

namespace QtRocket
{

namespace
{

using Type = FlightEvent::Type;

/// The builtin flight data type @p id.
[[nodiscard]] const FlightDataType& type(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

/// The flight data branch of @p status, which the engine gives every status (Java: a
/// NullPointerException without one).
[[nodiscard]] FlightDataBranch& branchOf(const SimulationStatus& status)
{
    const std::shared_ptr<FlightDataBranch>& branch = status.getFlightDataBranch();
    if (branch == nullptr)
    {
        bug("The simulation status has no flight data branch");
    }
    return *branch;
}

/// The conditions of @p status (Java: a NullPointerException without them).
[[nodiscard]] const SimulationConditions& conditionsOf(const SimulationStatus& status)
{
    const std::shared_ptr<SimulationConditions>& conditions = status.getSimulationConditions();
    if (conditions == nullptr)
    {
        bug("The simulation status has no simulation conditions");
    }
    return *conditions;
}

/// The aerodynamic calculator of the conditions of @p status (Java: a NullPointerException
/// without one).
[[nodiscard]] AerodynamicCalculator& calculatorOf(const SimulationStatus& status)
{
    const std::shared_ptr<AerodynamicCalculator>& calculator =
        conditionsOf(status).getAerodynamicCalculator();
    if (calculator == nullptr)
    {
        bug("The simulation conditions have no aerodynamic calculator");
    }
    return *calculator;
}

/// The motor state an IGNITION, BURNOUT or EJECTION_CHARGE event carries (Java: the cast of
/// event.getData(), and a NullPointerException for an event without data).
[[nodiscard]] std::shared_ptr<MotorClusterState> motorStateOf(const FlightEvent& event)
{
    std::shared_ptr<MotorClusterState> motorState = event.getMotorState();
    if (motorState == nullptr)
    {
        bug(std::format("The {} event carries no motor state", name(event.getType())));
    }
    return motorState;
}

/// The source of @p event (Java: a NullPointerException for an event without one).
[[nodiscard]] const RocketComponent& sourceOf(const FlightEvent& event)
{
    const RocketComponent* source = event.getSource();
    if (source == nullptr)
    {
        bug(std::format("The {} event has no source", name(event.getType())));
    }
    return *source;
}

/// The recovery device a RECOVERY_DEVICE_DEPLOYMENT event comes from (Java: the cast of
/// event.getSource()).
[[nodiscard]] const RecoveryDevice& recoveryDeviceOf(const FlightEvent& event)
{
    const auto* device = dynamic_cast<const RecoveryDevice*>(event.getSource());
    if (device == nullptr)
    {
        bug("The source of the RECOVERY_DEVICE_DEPLOYMENT event is not a recovery device");
    }
    return *device;
}

/// The flight stepper of @p stepperMethod.
/// @throws SimulationException for a method that is not supported
[[nodiscard]] std::unique_ptr<SimulationStepper> makeFlightStepper(
    SimulationStepperMethod stepperMethod)
{
    switch (stepperMethod)
    {
        case SimulationStepperMethod::RK4:
            return std::make_unique<Rk4SimulationStepper>();
        case SimulationStepperMethod::RK6:
            return std::make_unique<Rk6SimulationStepper>();
    }
    throw SimulationException(std::format("Unsupported simulation stepper method: {}",
                                          std::to_underlying(stepperMethod)));
}

/// Whether @p config has a drogue that deploys but no main parachute that does.
[[nodiscard]] bool hasDrogueWithoutMain(const FlightConfiguration&   config,
                                        const FlightConfigurationId& fcid)
{
    bool configHasDrogue = false;
    bool configHasMain   = false;
    for (const RocketComponent* comp : config.getActiveComponents())
    {
        const auto* rd = dynamic_cast<const RecoveryDevice*>(comp);
        if (rd == nullptr)
        {
            continue;
        }
        const DeploymentConfiguration& dc = rd->getDeploymentConfigurations().get(fcid);
        if (dc.getDeployEvent() != DeploymentConfiguration::DeployEvent::NEVER)
        {
            if (rd->isDrogue())
            {
                configHasDrogue = true;
            }
            else
            {
                configHasMain = true;
            }
        }
    }
    return configHasDrogue && !configHasMain;
}

/// Whether the stage @p deployingStage has, among the active components of @p config, a drogue
/// that deploys.
[[nodiscard]] bool stageHasDrogue(const FlightConfiguration&   config,
                                  const FlightConfigurationId& fcid,
                                  const AxialStage&            deployingStage)
{
    for (const RocketComponent* comp : config.getActiveComponents())
    {
        const auto* rd = dynamic_cast<const RecoveryDevice*>(comp);
        if (rd != nullptr && &rd->getStage() == &deployingStage)
        {
            const DeploymentConfiguration& dc = rd->getDeploymentConfigurations().get(fcid);
            if (dc.getDeployEvent() != DeploymentConfiguration::DeployEvent::NEVER &&
                rd->isDrogue())
            {
                return true;
            }
        }
    }
    return false;
}

/// Whether the queue of @p status holds a recovery device deployment within the next half
/// second.
[[nodiscard]] bool isRecoverySoon(const SimulationStatus& status)
{
    bool recoverySoon = false;
    for (const FlightEvent& e : status.getEventQueue())
    {
        if ((e.getType() == Type::RECOVERY_DEVICE_DEPLOYMENT) &&
            (e.getTime() < status.getSimulationTime() + 0.5))
        {
            recoverySoon = true;
        }
    }
    return recoverySoon;
}

}  // namespace

BasicEventSimulationEngine::BasicEventSimulationEngine()
  : m_flightStepper(std::make_unique<Rk4SimulationStepper>()),
    m_landingStepper(std::make_unique<BasicLandingStepper>()),
    m_tumbleStepper(std::make_unique<BasicTumbleStepper>()),
    m_groundStepper(std::make_unique<GroundStepper>())
{
}

BasicEventSimulationEngine::~BasicEventSimulationEngine() = default;

SimulationStatus& BasicEventSimulationEngine::status()
{
    if (!m_currentStatus.has_value())
    {
        bug("The simulation engine has no current status");
    }
    return *m_currentStatus;
}

void BasicEventSimulationEngine::simulate(
    const std::shared_ptr<SimulationConditions>& simulationConditions)
{
    if (simulationConditions == nullptr)
    {
        bug("The simulation engine needs simulation conditions");
    }

    // Whatever an earlier run of this engine left behind refers to that run's rocket.
    m_toSimulate.clear();
    m_currentStatus.reset();
    m_currentStepper = nullptr;
    m_simulationRocket.reset();

    // Set up flight data
    m_flightData = std::make_shared<FlightData>();

    // Choose which Runge Kutta Method to use according to the options.
    const Simulation* simulation = simulationConditions->getSimulation();
    if (simulation == nullptr)
    {
        bug("The simulation conditions have no simulation");
    }
    const SimulationStepperMethod stepperMethod =
        simulation->getOptions().getSimulationStepperMethodChoice();
    m_flightStepper = makeFlightStepper(stepperMethod);

    try
    {
        runSimulation(simulationConditions);
    }
    catch (...)
    {
        finishFlightData();
        throw;
    }
    finishFlightData();
}

void BasicEventSimulationEngine::finishFlightData()
{
    // The events of the flight data point into the rocket that was simulated.
    m_flightData->setSimulatedRocket(m_simulationRocket);
    m_flightData->calculateInterestingValues();
}

void BasicEventSimulationEngine::runSimulation(
    const std::shared_ptr<SimulationConditions>& simulationConditions)
{
    setUpFirstStatus(simulationConditions);

    // Java pushes the status and then fires the hook on it: the same object.
    SimulationListenerHelper::fireStartSimulation(status());
    m_toSimulate.push_front(std::move(status()));
    m_currentStatus.reset();

    // Java: do { if (toSimulate.peek() == null) break; ... } while (!toSimulate.isEmpty());
    while (!m_toSimulate.empty())
    {
        simulateBranch(*simulationConditions);
    }

    SimulationListenerHelper::fireEndSimulation(status(), nullptr);
}

void BasicEventSimulationEngine::setUpFirstStatus(
    const std::shared_ptr<SimulationConditions>& simulationConditions)
{
    // Set up rocket configuration
    m_fcid                                = simulationConditions->getFlightConfigurationId();
    const Rocket&              origRocket = simulationConditions->getRocket();
    const FlightConfiguration& origConfig = origRocket.getFlightConfiguration(m_fcid);
    m_simulationRocket                    = origRocket.copyRocketWithOriginalId();
    // Resolve configuration-dependent component positions on the private simulation copy.
    m_simulationRocket->setSelectedConfiguration(m_fcid);
    const std::shared_ptr<FlightConfiguration> simulationConfig =
        std::make_shared<FlightConfiguration>(origConfig.clone(*m_simulationRocket));
    simulationConfig->copyStages(origConfig);  // Clone the stage activation configuration

    m_currentStatus.emplace(simulationConfig, simulationConditions);
    SimulationStatus& currentStatus = status();
    // main simulation branch. Need to watch for pathological case with no stages defined
    const AxialStage* topStage =
        simulationConfig->getRocket().getTopmostStage(currentStatus.getConfiguration());
    const std::string branchName = topStage != nullptr ? topStage->getName() : kNullBranchName;
    const std::shared_ptr<FlightDataBranch> initialBranch = std::make_shared<FlightDataBranch>(
        branchName, topStage,
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{
            type(FlightDataTypeId::TYPE_TIME)});
    currentStatus.setWarnings(
        std::shared_ptr<WarningSet>(m_flightData, &m_flightData->getWarningSet()));
    currentStatus.setFlightDataBranch(initialBranch);

    // Sanity checks on design and configuration

    // Problems that keep us from simulating at all

    // No active stages
    if (topStage == nullptr)
    {
        currentStatus.abortSimulation(SimulationAbort::Cause::NO_ACTIVE_STAGES);
    }

    // No motors in configuration
    if (!simulationConfig->hasMotors())
    {
        currentStatus.abortSimulation(SimulationAbort::Cause::NO_MOTORS_DEFINED);
    }

    // Problems that let us simulate, but result is likely bad

    // No recovery device
    if (!simulationConfig->hasRecoveryDevice())
    {
        currentStatus.addWarning(Warning::kNoRecoveryDevice);
    }
    else if (hasDrogueWithoutMain(*simulationConfig, m_fcid))
    {
        // Check for drogue without main
        currentStatus.addWarning(Warning::RecoveryDrogueWithoutMain());
    }

    // Java's event points to the caller's rocket; see the class comment.
    currentStatus.addEvent(FlightEvent(Type::LAUNCH, 0, m_simulationRocket.get()));
}

void BasicEventSimulationEngine::simulateBranch(const SimulationConditions& simulationConditions)
{
    m_currentStatus.emplace(std::move(m_toSimulate.front()));
    m_toSimulate.pop_front();
    status().setWarnings(std::shared_ptr<WarningSet>(m_flightData, &m_flightData->getWarningSet()));
    const std::shared_ptr<FlightDataBranch> dataBranch = status().getFlightDataBranch();
    m_flightData->addBranch(dataBranch);

    try
    {
        SimulationListenerHelper::fireStartSimulationBranch(status());
        simulateLoop(simulationConditions);
    }
    catch (const SimulationException& e)
    {
        // Java's finally block; the exception propagates after it.
        SimulationListenerHelper::fireEndSimulationBranch(status(), &e);
        throw;
    }
    catch (...)
    {
        // Java's finally block on the way out of an exception that is no SimulationException.
        SimulationListenerHelper::fireEndSimulationBranch(status(), nullptr);
        throw;
    }
    SimulationListenerHelper::fireEndSimulationBranch(status(), nullptr);

    dataBranch->immute();

    // Did the branch generate any data?
    if (dataBranch->getLength() == 0)
    {
        m_flightData->getWarningSet().add(Warning::kEmptyBranch, dataBranch->getName());
    }
}

void BasicEventSimulationEngine::selectStepper()
{
    // Select the appropriate stepper:
    //     On the ground: ground stepper
    //     Tumbling: tumble stepper
    //     At least one recovery device deployed: landing stepper
    //     Otherwise: flight stepper

    if (status().isLanded())
    {
        m_currentStepper = m_groundStepper.get();
    }
    else if (status().isTumbling())
    {
        m_currentStepper = m_tumbleStepper.get();
    }
    else if (!status().getDeployedRecoveryDevices().empty())
    {
        m_currentStepper = m_landingStepper.get();
    }
    else
    {
        m_currentStepper = m_flightStepper.get();
    }
}

void BasicEventSimulationEngine::switchStepper(SimulationStepper& stepper)
{
    m_currentStepper = &stepper;
    // Java's `currentStatus = currentStepper.initialize(currentStatus)` keeps the old status when
    // initialize() throws (a listener that cannot be cloned), and the engine's finally block
    // still calls endSimulationBranch() on it. So the stepper gets Java's shallow clone, which
    // shares every object with the current status, and the current status stays whole until
    // the result replaces it.
    status() = m_currentStepper->initialize(status().clone());
}

void BasicEventSimulationEngine::simulateLoop(const SimulationConditions& simulationConditions)
{
    // Initialize the simulation.
    selectStepper();

    switchStepper(*m_currentStepper);
    double previousSimulationTime = status().getSimulationTime();

    // Get originating position (in case listener has modified launch position)
    const Coordinate origin         = status().getRocketPosition();
    const Coordinate originVelocity = status().getRocketVelocity();

    try
    {
        checkGeometry(status());

        // Start the simulation
        while (handleEvents(simulationConditions))
        {
            // Take the step
            const double oldAlt = status().getRocketPosition().z;

            takeStep();

            // Check for NaN values in the simulation status
            checkNaN();

            addStepEvents(oldAlt, origin, originVelocity, previousSimulationTime);

            previousSimulationTime = status().getSimulationTime();
        }
    }
    catch (const SimulationException& e)
    {
        SimulationListenerHelper::fireEndSimulation(status(), &e);

        // Add FlightEvent for exception.
        const std::optional<std::string> message = e.getMessage();
        branchOf(status()).addEvent(FlightEvent(
            Type::EXCEPTION, status().getSimulationTime(), &status().getConfiguration().getRocket(),
            message.has_value() ? FlightEvent::Data{*message} : FlightEvent::Data{}));

        const std::shared_ptr<WarningSet>& warnings = status().getWarnings();
        if (warnings == nullptr)
        {
            bug("The simulation status has no warning set");
        }
        m_flightData->getWarningSet().addAll(*warnings);

        throw;
    }
}

void BasicEventSimulationEngine::takeStep()
{
    if (SimulationListenerHelper::firePreStep(status()))
    {
        // Step at most to the next event
        double             maxStepTime = std::numeric_limits<double>::max();
        const FlightEvent* nextEvent   = status().getEventQueue().peek();

        if (nextEvent != nullptr)
        {
            maxStepTime = MathUtil::max(nextEvent->getTime() - status().getSimulationTime(), 0.001);
        }
        else if (status().isLanded())
        {
            maxStepTime = 0.0;
        }

        if (maxStepTime > MathUtil::kEpsilon)
        {
            m_currentStepper->step(status(), maxStepTime);
        }
    }
    SimulationListenerHelper::firePostStep(status());
}

void BasicEventSimulationEngine::addStepEvents(double oldAlt, const Coordinate& origin,
                                               const Coordinate& originVelocity,
                                               double            previousSimulationTime)
{
    // If we haven't hit the ground, add altitude event
    if (!status().isLanded())
    {
        status().addEvent(FlightEvent(
            Type::ALTITUDE, status().getSimulationTime(), &status().getConfiguration().getRocket(),
            FlightEvent::AltitudeChange{.previous = oldAlt,
                                        .current  = status().getRocketPosition().z}));
    }

    if (status().getRocketPosition().z > status().getMaxAlt())
    {
        status().setMaxAlt(status().getRocketPosition().z);
    }

    // Position relative to start location
    const Coordinate relativePosition = checkLiftoffAndGroundHit(origin, originVelocity);

    // Check for launch guide clearance
    if (status().isLiftoff() && !status().isLaunchRodCleared() &&
        relativePosition.length() > conditionsOf(status()).getLaunchRodLength())
    {
        status().addEvent(FlightEvent(Type::LAUNCHROD, status().getSimulationTime(), nullptr));
    }

    // Check for apogee
    if (!status().isApogeeReached() && status().getRocketPosition().z < status().getMaxAlt() - 0.01)
    {
        status().setMaxAltTime(previousSimulationTime);
        status().addEvent(FlightEvent(Type::APOGEE, previousSimulationTime,
                                      &status().getConfiguration().getRocket()));
    }

    checkTumbling();

    // If I'm on the ground and have no events in the queue, I'm done
    if (status().isLanded() && status().getEventQueue().empty())
    {
        status().addEvent(FlightEvent(Type::SIMULATION_END, status().getSimulationTime()));
    }
}

Coordinate BasicEventSimulationEngine::checkLiftoffAndGroundHit(const Coordinate& origin,
                                                                const Coordinate& originVelocity)
{
    // Position relative to start location
    Coordinate relativePosition = status().getRocketPosition().sub(origin);

    // Add appropriate events
    if (!status().isLiftoff())
    {
        // Avoid sinking into ground before liftoff
        if (relativePosition.z < 0)
        {
            status().setRocketPosition(origin);
            relativePosition = Coordinate::kZero;
            status().setRocketVelocity(originVelocity);
        }
        // Detect lift-off
        if (relativePosition.z > 0.02)
        {
            status().addEvent(FlightEvent(Type::LIFTOFF, status().getSimulationTime()));
        }
    }
    else
    {
        // Check ground hit after liftoff
        if ((status().getRocketPosition().z < MathUtil::kEpsilon) && !status().isLanded())
        {
            status().addEvent(FlightEvent(Type::GROUND_HIT, status().getSimulationTime()));
        }
    }

    return relativePosition;
}

void BasicEventSimulationEngine::checkTumbling()
{
    // Check for tumbling, and otherwise for fin stall.
    //
    // Tumbling is decided from how long a high angle of attack has persisted, measured against
    // the rocket's own pitch period, rather than from its value at a single step (see
    // TumbleDetector). A large instantaneous angle of attack still means the aerodynamic model
    // is out of its envelope. It is reported at low priority, but not for a separated stage: a
    // spent booster left to fall reaches a large angle of attack every time, by design.
    //
    // Inhibited if already tumbling, parachutes deployed, or on the ground
    if (!status().isTumbling() && status().getDeployedRecoveryDevices().empty() &&
        !status().isLanded())
    {
        const FlightDataBranch& branch     = branchOf(status());
        const double            aoa        = branch.getLast(type(FlightDataTypeId::TYPE_AOA));
        const double            stallAngle = calculatorOf(status()).getStallAngle();
        // FlightConditions does not reach here, but its velocity is recoverable:
        // the recorded Mach number is the air-relative speed over the speed of sound.
        const double airSpeed = branch.getLast(type(FlightDataTypeId::TYPE_MACH_NUMBER)) *
                                branch.getLast(type(FlightDataTypeId::TYPE_SPEED_OF_SOUND));

        const bool tumbling = status().getTumbleDetector().update(
            status().getSimulationTime(), status().isLaunchRodCleared(), aoa, airSpeed,
            branch.getLast(type(FlightDataTypeId::TYPE_AIR_DENSITY)),
            branch.getLast(type(FlightDataTypeId::TYPE_NATURAL_FREQUENCY)));

        if (tumbling)
        {
            status().addEvent(FlightEvent(Type::TUMBLE, status().getSimulationTime()));
        }
        else if (aoa > stallAngle && !status().isSeparatedStage() && status().recordWarnings())
        {
            status().addWarning(Warning::LargeAOA(aoa));
        }
    }
}

bool BasicEventSimulationEngine::handleEvents(const SimulationConditions& simulationConditions)
{
    bool ret = true;

    for (std::optional<FlightEvent> event = nextEvent(); event.has_value(); event = nextEvent())
    {
        handleEvent(*event, ret);
    }

    if (status().getSimulationTime() >= simulationConditions.getMaxSimulationTime())
    {
        ret = false;
        branchOf(status()).addEvent(
            FlightEvent(Type::SIMULATION_END, status().getSimulationTime()));
    }

    // If no motor has ignited, abort
    if (!status().isMotorIgnited())
    {
        status().abortSimulation(SimulationAbort::Cause::NO_MOTORS_FIRED);
    }

    return ret;
}

void BasicEventSimulationEngine::handleEvent(const FlightEvent& event, bool& ret)
{
    // Check for motor ignition events, add ignition events to queue
    queueIgnitions(event);

    // Call simulation listeners, allow aborting event handling
    if (!SimulationListenerHelper::fireHandleFlightEvent(status(), event))
    {
        return;
    }

    if (event.getType() == Type::RECOVERY_DEVICE_DEPLOYMENT)
    {
        const RecoveryDevice& device = recoveryDeviceOf(event);
        if (!SimulationListenerHelper::fireRecoveryDeviceDeployment(status(), device))
        {
            return;
        }
    }

    // Check for stage separation event
    queueSeparations(event);

    // Check for recovery device deployment, add events to queue
    queueDeployments(event);

    // Handle event
    if (!dispatchEvent(event, ret))
    {
        return;
    }

    // If I get an event other than ALTITUDE and SIMULATION_END after I'm on the ground, there's
    // a problem
    if (status().isLanded() && (event.getType() != Type::GROUND_HIT) &&
        (event.getType() != Type::ALTITUDE) && (event.getType() != Type::SIMULATION_END))
    {
        status().addWarning(Warning::EventAfterLanding(std::string{displayName(event.getType())}));
    }
}

void BasicEventSimulationEngine::queueIgnitions(const FlightEvent& event)
{
    for (const std::shared_ptr<MotorClusterState>& state : status().getActiveMotors())
    {
        if (FlightEventActivation::testForIgnition(*state, status().getConfiguration(), event))
        {
            const MotorMount&         mount         = state->getMount();
            const MotorConfiguration& motorInstance = mount.getMotorConfig(m_fcid);

            const double ignitionTime =
                status().getSimulationTime() + motorInstance.getIgnitionDelay();

            status().addEvent(
                FlightEvent(Type::IGNITION, ignitionTime, &asComponent(mount), state));
        }
    }
}

void BasicEventSimulationEngine::queueSeparations(const FlightEvent& event)
{
    for (const AxialStage* stage : status().getConfiguration().getActiveStages())
    {
        const int stageNo = stage->getStageNumber();
        if (stageNo == 0)
        {
            continue;
        }

        const StageSeparationConfiguration& separationConfig =
            stage->getSeparationConfigurations().get(m_fcid);
        if (FlightEventActivation::isSeparationEvent(separationConfig.getSeparationEvent(),
                                                     separationConfig, event, *stage))
        {
            status().addEvent(FlightEvent(Type::STAGE_SEPARATION,
                                          event.getTime() + separationConfig.getSeparationDelay(),
                                          stage));
        }
    }
}

void BasicEventSimulationEngine::queueDeployments(const FlightEvent& event)
{
    for (const RocketComponent* c : status().getConfiguration().getActiveComponents())
    {
        const auto* device = dynamic_cast<const RecoveryDevice*>(c);
        if (device == nullptr)
        {
            continue;
        }
        // A recovery device can only deploy once, so ignore any further triggers (for example
        // the ejection charge of a second motor in the same airframe).
        if (status().getDeployedRecoveryDevices().contains(device))
        {
            continue;
        }
        const DeploymentConfiguration& deployConfig =
            device->getDeploymentConfigurations().get(m_fcid);
        if (FlightEventActivation::isActivationEvent(deployConfig, event, *c))
        {
            // Delay event by at least 1ms to allow stage separation to occur first
            status().addEvent(FlightEvent(
                Type::RECOVERY_DEVICE_DEPLOYMENT,
                event.getTime() + MathUtil::javaMax(0.001, deployConfig.getDeployDelay()), c));
        }
    }
}

bool BasicEventSimulationEngine::dispatchEvent(const FlightEvent& event, bool& ret)
{
    switch (event.getType())
    {
        case Type::LAUNCH:
            branchOf(status()).addEvent(event);
            break;

        case Type::IGNITION:
            return handleIgnition(event);

        case Type::LIFTOFF:
            // Mark lift-off as occurred
            status().setLiftoff(true);
            branchOf(status()).addEvent(event);
            break;

        case Type::LAUNCHROD:
            // Mark launch rod as cleared
            status().setLaunchRodCleared(true);
            branchOf(status()).addEvent(event);
            break;

        case Type::BURNOUT:
            handleBurnout(event);
            break;

        case Type::EJECTION_CHARGE:
            handleEjectionCharge(event);
            break;

        case Type::STAGE_SEPARATION:
            handleStageSeparation(event);
            break;

        case Type::APOGEE:
            handleApogee(event);
            break;

        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            handleRecoveryDeviceDeployment(event);
            break;

        case Type::GROUND_HIT:
            handleGroundHit(event);
            break;

        case Type::SIM_ABORT:
            ret = false;

            // store current status to flight data.  Don't take one last simulation step,
            // as the SIM_ABORT may well be to avoid an exception
            status().storeData();

            branchOf(status()).addEvent(event);
            break;

        case Type::SIMULATION_END:
            ret = false;
            branchOf(status()).addEvent(event);
            break;

        case Type::ALTITUDE:
            // nothing special needs to be done for this event
            break;

        case Type::TUMBLE:
            handleTumble(event);
            break;

        case Type::SIM_WARN:
        case Type::EXCEPTION:
            // Java's switch has no case for these: nothing happens.
            break;
    }
    return true;
}

bool BasicEventSimulationEngine::handleIgnition(const FlightEvent& event)
{
    const std::shared_ptr<MotorClusterState> motorState = motorStateOf(event);

    // If there are multiple ignition events (as is the case if the preceding stage has several
    // burnout events, for instance) we get multiple ignition events for the upper stage motor.
    // Ignore all after the first.
    if (motorState->getIgnitionTime() < status().getSimulationTime())
    {
        return false;
    }
    motorState->ignite(event.getTime());

    // Ignite the motor
    status().setMotorIgnited(true);
    branchOf(status()).addEvent(event);

    // Java casts the source: the cast of null succeeds (an IGNITION that a listener queued
    // without a source; FlightEvent's validation allows it), and the listeners get a null
    // mount. The hook takes a reference here, so they get the mount of the motor state.
    const MotorConfigurationId motorId = motorState->getId();
    const auto*                mount   = dynamic_cast<const MotorMount*>(event.getSource());
    if (mount == nullptr && event.getSource() != nullptr)
    {
        bug("The source of the IGNITION event is not a motor mount");
    }
    const MotorMount& ignitedMount = mount != nullptr ? *mount : motorState->getMount();
    if (!SimulationListenerHelper::fireMotorIgnition(status(), motorId, ignitedMount, *motorState))
    {
        return false;
    }

    // Queue an altitude event for every point in the thrust curve to set the RK4 simulation
    // time steps
    const auto* motor = dynamic_cast<const ThrustCurveMotor*>(motorState->getMotor().get());
    if (motor == nullptr)
    {
        bug("The motor of the IGNITION event is not a thrust curve motor");
    }
    for (const double point : motor->getTimePoints())
    {
        status().addEvent(FlightEvent(Type::ALTITUDE, point, event.getSource()));
    }

    // and queue up the burnout for this motor, as well.
    const double duration = motorState->getBurnTime();
    const double burnout  = status().getSimulationTime() + duration;
    status().addEvent(FlightEvent(Type::BURNOUT, burnout, event.getSource(), motorState));
    return true;
}

void BasicEventSimulationEngine::handleBurnout(const FlightEvent& event)
{
    // If motor burnout occurs without lift-off, abort
    if (!status().isLiftoff())
    {
        status().abortSimulation(SimulationAbort::Cause::NO_LIFTOFF);
    }

    // Add ejection charge event
    const std::shared_ptr<MotorClusterState> motorState = motorStateOf(event);
    motorState->burnOut(event.getTime());

    const MotorMount& mount = motorState->getMount();
    const AxialStage& stage = asComponent(mount).getStage();

    const double delay = motorState->getEjectionDelay();
    if (motorState->hasEjectionCharge())
    {
        status().addEvent(FlightEvent(Type::EJECTION_CHARGE, status().getSimulationTime() + delay,
                                      &stage, event.getData()));
    }
    branchOf(status()).addEvent(event);
}

void BasicEventSimulationEngine::handleEjectionCharge(const FlightEvent& event)
{
    const std::shared_ptr<MotorClusterState> motorState = motorStateOf(event);
    motorState->expend(event.getTime());
    branchOf(status()).addEvent(event);
}

void BasicEventSimulationEngine::handleStageSeparation(const FlightEvent& event)
{
    const RocketComponent& boosterStage = sourceOf(event);
    const int              stageNumber  = boosterStage.getStageNumber();

    if (!status().getConfiguration().isStageActive(stageNumber - 1))
    {
        // upper stage is not active; not performing separation
        return;
    }

    // Record the event.
    branchOf(status()).addEvent(event);

    // If I've got something other than one active stage below the separation point,
    // flag a warning
    int numActiveBelow = 0;
    for (int i = stageNumber; i < status().getConfiguration().getStageCount(); i++)
    {
        if (status().getConfiguration().isStageActive(i))
        {
            numActiveBelow++;
        }
    }
    if (numActiveBelow != 1)
    {
        status().addWarning(Warning::kSeparationOrder);
    }

    // If I haven't cleared the rail yet, flag a warning
    if (!status().isLaunchRodCleared())
    {
        status().addWarning(Warning::kEarlySeparation);
    }

    // Create a new simulation branch for the booster
    SimulationStatus boosterStatus(status());
    boosterStatus.setSeparatedStage(true);

    // Prepare the new simulation branch
    boosterStatus.setFlightDataBranch(std::make_shared<FlightDataBranch>(
        boosterStage.getName(), &boosterStage, status().getFlightDataBranch().get()));
    branchOf(boosterStatus).addEvent(event);

    // Mark the current status as having dropped the current stage and all stages below it
    status().getConfiguration().clearStagesBelow(stageNumber);
    status().removeUnattachedEvents();

    // Mark the booster status as having no active stages above
    boosterStatus.getConfiguration().clearStagesAbove(stageNumber);
    boosterStatus.removeUnattachedEvents();

    m_toSimulate.push_front(std::move(boosterStatus));

    // Make sure upper stages can still be simulated
    checkGeometry(status());
}

void BasicEventSimulationEngine::handleApogee(const FlightEvent& event)
{
    // Mark apogee as reached
    status().setApogeeReached(true);
    branchOf(status()).addEvent(event);
    // This apogee event might be the optimum if recovery has not already happened.
    if (status().getDeployedRecoveryDevices().empty())
    {
        branchOf(status()).setOptimumAltitude(status().getMaxAlt());
        branchOf(status()).setTimeToOptimumAltitude(status().getMaxAltTime());
    }
}

void BasicEventSimulationEngine::handleRecoveryDeviceDeployment(const FlightEvent& event)
{
    const RecoveryDevice&  deployingDevice = recoveryDeviceOf(event);
    const RocketComponent& c               = deployingDevice;
    const int              n               = c.getStageNumber();

    // Ignore event if stage not active, or if the device is already deployed.  The latter
    // can still happen after the check made when the event was queued, since two motors may
    // fire their ejection charges at the very same instant.
    if (!status().getConfiguration().isStageActive(n) ||
        status().getDeployedRecoveryDevices().contains(&deployingDevice))
    {
        return;
    }

    // Check whether any motor in the active stages is active anymore
    for (const std::shared_ptr<MotorClusterState>& state : status().getActiveMotors())
    {
        if (state->getThrust(status().getSimulationTime()) > MathUtil::kEpsilon)
        {
            status().abortSimulation(SimulationAbort::Cause::DEPLOY_UNDER_THRUST);
        }
    }

    // Check for launch rod
    if (!status().isLaunchRodCleared())
    {
        status().addWarning(Warning::kRecoveryLaunchRod);
    }

    // Check current velocity against configured warning thresholds
    addDeploymentSpeedWarnings(deployingDevice);

    status().setLiftoff(true);
    status().getDeployedRecoveryDevices().add(&deployingDevice);

    // If we haven't already reached apogee, then we need to compute the actual coast time
    // to determine the optimum altitude.
    if (!status().isApogeeReached())
    {
        const std::shared_ptr<FlightData> coastStatus = computeCoastTime();

        branchOf(status()).setOptimumAltitude(coastStatus->getMaxAltitude());
        branchOf(status()).setTimeToOptimumAltitude(coastStatus->getTimeToApogee());
    }

    // switch to landing stepper (unless we're already on the ground)
    if (!status().isLanded())
    {
        switchStepper(*m_landingStepper);
    }

    branchOf(status()).addEvent(event);
}

void BasicEventSimulationEngine::addDeploymentSpeedWarnings(const RecoveryDevice& deployingDevice)
{
    const double                deploySpeed    = status().getRocketVelocity().length();
    const SimulationConditions& conds          = conditionsOf(status());
    const AxialStage&           deployingStage = deployingDevice.getStage();
    const MessageSources        chute{MessageSource::of(deployingDevice)};

    // Auto-detect: scan active components in the deploying stage only for a drogue
    if (stageHasDrogue(status().getConfiguration(), m_fcid, deployingStage))
    {
        // Dual-deployment: warn on both high and low speed for the main chute. (Java has the
        // low speed warning of the drogue commented out.)
        if (!deployingDevice.isDrogue())
        {
            if (deploySpeed > conds.getRecoveryDrogueMainHighSpeedWarning())
            {
                status().addWarning(Warning::HighSpeedMainDeployment(deploySpeed, chute));
            }
            if (deploySpeed < conds.getRecoveryDrogueMainLowSpeedWarning())
            {
                status().addWarning(Warning::LowSpeedMainDeployment(deploySpeed, chute));
            }
        }
    }
    else
    {
        // Single-deployment (no drogue): standard speed warning
        if (deploySpeed > conds.getRecoverySpeedWarning())
        {
            status().addWarning(Warning::RecoveryHighSpeedDeployment(deploySpeed, chute));
        }
    }
}

void BasicEventSimulationEngine::handleGroundHit(const FlightEvent& event)
{
    status().setLanded(true);

    // take one last simulation step so we can save current
    // status and compute parameters at the instant of impact
    m_currentStepper->step(status(), std::numeric_limits<double>::quiet_NaN());

    switchStepper(*m_groundStepper);

    branchOf(status()).addEvent(event);
}

void BasicEventSimulationEngine::handleTumble(const FlightEvent& event)
{
    // Inhibit if we've deployed a parachute or we're on the ground
    if (!status().getDeployedRecoveryDevices().empty() || status().isLanded())
    {
        return;
    }

    const bool tooMuchThrust =
        branchOf(status()).getLast(type(FlightDataTypeId::TYPE_THRUST_FORCE)) >
        kThrustTumbleCondition;
    if (tooMuchThrust)
    {
        status().abortSimulation(SimulationAbort::Cause::TUMBLE_UNDER_THRUST);
    }
    else
    {
        switchStepper(*m_tumbleStepper);

        status().setTumbling(true);
        branchOf(status()).addEvent(event);
    }
}

std::optional<FlightEvent> BasicEventSimulationEngine::nextEvent()
{
    EventQueue&        queue = status().getEventQueue();
    const FlightEvent* event = queue.peek();
    if (event == nullptr)
    {
        return std::nullopt;
    }
    const double eventTime = event->getTime();

    // Jump to event if no motors have been ignited
    if (!status().isMotorIgnited() && eventTime > status().getSimulationTime())
    {
        status().setSimulationTime(eventTime);
    }
    if (eventTime <= status().getSimulationTime())
    {
        return queue.poll();
    }
    return std::nullopt;
}

// we need to check geometry to make sure we can simulate the active
// stages in a simulation branch when the branch starts executing, and
// whenever a stage separation occurs
void BasicEventSimulationEngine::checkGeometry(SimulationStatus& currentStatus)
{
    const FlightConfiguration& configuration = currentStatus.getConfiguration();

    // Active stages have total length of 0.
    if (configuration.getLengthAerodynamic() < MathUtil::kEpsilon)
    {
        currentStatus.abortSimulation(SimulationAbort::Cause::ACTIVE_LENGTH_ZERO);
    }

    // Can't calculate stability.  If it's the sustainer we'll abort; if a booster
    // we'll just transition to tumbling (if it's a booster and under thrust code elsewhere
    // will abort).  One thing to note about the CP (and CG) calculations here is that this is
    // executed immediately upon stage separation; the last values recorded for them in the
    // flight data branch is before separation so we can't just look there for it.
    WarningSet       cpWarnings;
    const Coordinate cp = calculatorOf(currentStatus)
                              .getCP(configuration, FlightConditions(configuration), &cpWarnings);
    if (cp.weight < MathUtil::kEpsilon)
    {
        if (configuration.isStageActive(0))
        {
            currentStatus.abortSimulation(SimulationAbort::Cause::NO_CP);
        }
        else
        {
            currentStatus.addEvent(FlightEvent(Type::TUMBLE, currentStatus.getSimulationTime()));
        }
    }

    // see what the aero calculators have to say
    WarningSet geometryWarnings;
    calculatorOf(currentStatus)
        .checkGeometry(configuration, configuration.getRocket(), &geometryWarnings);

    // Under several circumstances to be detailed below, the open airframe warning is more
    // confusing than helpful so we'll filter it out.
    if (!geometryWarnings.empty())
    {
        // If it isn't stable, the user has bigger issues than the open from airframe and telling
        // them about it would only be noise.
        const double cpx = cp.x;
        const double cgx =
            MassCalculator::calculateLaunch(currentStatus.getConfiguration()).getCM().x;
        const bool stable = cgx < cpx;

        // If we're about to deploy a recovery device, we don't care about the open
        // airframe
        const bool recoverySoon = isRecoverySoon(currentStatus);

        // If we've already deployed a recovery device, we don't care about open airframe
        const bool recovered = !currentStatus.getDeployedRecoveryDevices().empty();

        if (!stable || recoverySoon || recovered)
        {
            geometryWarnings.filterOut(Warning::kOpenAirframeForward);
        }

        currentStatus.addWarnings(geometryWarnings);
    }
}

void BasicEventSimulationEngine::checkNaN()
{
    double d = 0;
    bool   b = false;
    d += status().getSimulationTime();
    b = b || status().getRocketPosition().isNaN();
    b = b || status().getRocketVelocity().isNaN();
    b = b || status().getRocketOrientationQuaternion().isNaN();
    b = b || status().getRocketRotationVelocity().isNaN();
    d += status().getEffectiveLaunchRodLength();

    if (std::isnan(d) || b)
    {
        throw SimulationCalculationException(kNaNResult, status().getFlightDataBranch());
    }
}

std::shared_ptr<FlightData> BasicEventSimulationEngine::computeCoastTime()
{
    const std::shared_ptr<SimulationConditions> conds =
        std::make_shared<SimulationConditions>(conditionsOf(status()).clone());
    // Clear user listeners - nested simulation should only use system listeners
    // to avoid triggering user listeners twice (once for main sim, once for nested)
    std::erase_if(conds->getSimulationListenerList(),
                  [](const std::shared_ptr<SimulationListener>& listener) {
                      return !listener->isSystemListener();
                  });
    conds->getSimulationListenerList().push_back(OptimumCoastListener::instance());
    BasicEventSimulationEngine coastEngine;

    coastEngine.simulate(conds);
    return coastEngine.getFlightData();
}

}  // namespace QtRocket
