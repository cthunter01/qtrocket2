#include "QtRocket/simulation/SimulationStatus.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/TumbleDetector.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

namespace
{

/// The simple name of the Java class of an event's data, as toEventDebug() prints it.
struct DataClassName
{
    [[nodiscard]] std::string operator()(std::monostate /*none*/) const { return {}; }
    [[nodiscard]] std::string operator()(const std::shared_ptr<MotorClusterState>& /*state*/) const
    {
        return "MotorClusterState";
    }
    [[nodiscard]] std::string operator()(const std::shared_ptr<const Warning>& warning) const
    {
        return std::string{warning->typeName()};
    }
    [[nodiscard]] std::string operator()(const SimulationAbort& /*abort*/) const
    {
        return "SimulationAbort";
    }
    [[nodiscard]] std::string operator()(const FlightEvent::AltitudeChange& /*change*/) const
    {
        return "Pair";
    }
    [[nodiscard]] std::string operator()(const std::string& /*text*/) const { return "String"; }
};

/// The builtin flight data type @p id.
[[nodiscard]] const FlightDataType& type(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

/// The conditions of a new status, which must not be null (Java: NullPointerException).
[[nodiscard]] std::shared_ptr<SimulationConditions> requireConditions(
    std::shared_ptr<SimulationConditions> simulationConditions)
{
    if (simulationConditions == nullptr)
    {
        bug("A simulation status needs simulation conditions");
    }
    return simulationConditions;
}

/// The flight configuration of a new status, which must not be null (Java:
/// NullPointerException).
[[nodiscard]] std::shared_ptr<FlightConfiguration> requireConfiguration(
    std::shared_ptr<FlightConfiguration> configuration)
{
    if (configuration == nullptr)
    {
        bug("A simulation status needs a flight configuration");
    }
    return configuration;
}

/// The orientation of a rocket of @p configuration on the launch rod of @p conditions.
[[nodiscard]] Quaternion initialOrientation(const FlightConfiguration&  configuration,
                                            const SimulationConditions& conditions)
{
    // Initialize to roll angle with least stability w.r.t. the wind
    const FlightConditions cond(configuration);
    const double           angle =
        -cond.getTheta() - ((std::numbers::pi / 2.0) - conditions.getLaunchRodDirection());
    Quaternion o = Quaternion::rotation(Coordinate(0, 0, angle));

    // Launch rod angle and direction
    o = o.multiplyLeft(Quaternion::rotation(Coordinate(0, conditions.getLaunchRodAngle(), 0)));
    o = o.multiplyLeft(Quaternion::rotation(
        Coordinate(0, 0, (std::numbers::pi / 2.0) - conditions.getLaunchRodDirection())));

    return o;
}

}  // namespace

SimulationStatus::SimulationStatus(std::shared_ptr<FlightConfiguration>  configuration,
                                   std::shared_ptr<SimulationConditions> simulationConditions)
  : m_simulationConditions(requireConditions(std::move(simulationConditions))),
    m_configuration(requireConfiguration(std::move(configuration))),
    m_position(m_simulationConditions->getLaunchPosition()),
    m_worldPosition(m_simulationConditions->getLaunchSite()),
    m_velocity(m_simulationConditions->getLaunchVelocity()),
    m_orientation(initialOrientation(*m_configuration, *m_simulationConditions)),
    m_rotationVelocity(Coordinate::kNul),
    m_effectiveLaunchRodLength(calculateEffectiveLaunchRodLength()),
    m_simulationStartWallTime(WallClock::now()),
    m_warnings(std::make_shared<WarningSet>())
{
    populateMotors();
}

SimulationStatus::SimulationStatus(const SimulationStatus& orig)
  :  // FlightDataBranch is not cloned.
    m_flightDataBranch(orig.m_flightDataBranch),
    m_time(orig.m_time),
    m_position(orig.m_position),
    m_worldPosition(orig.m_worldPosition),
    m_velocity(orig.m_velocity),
    m_orientation(orig.m_orientation),
    m_rotationVelocity(orig.m_rotationVelocity),
    m_maxZVelocity(orig.m_maxZVelocity),
    m_startWarningsTime(orig.m_startWarningsTime),
    m_tumbleDetector(std::make_shared<TumbleDetector>(*orig.m_tumbleDetector)),
    m_separatedStage(orig.m_separatedStage),
    m_effectiveLaunchRodLength(orig.m_effectiveLaunchRodLength),
    m_simulationStartWallTime(orig.m_simulationStartWallTime),
    m_motorIgnited(orig.m_motorIgnited),
    m_liftoff(orig.m_liftoff),
    m_launchRodCleared(orig.m_launchRodCleared),
    m_apogeeReached(orig.m_apogeeReached),
    m_tumbling(orig.m_tumbling),
    m_landed(orig.m_landed)
{
    if (orig.m_simulationConditions == nullptr)
    {
        bug("The simulation status to copy has no simulation conditions");
    }
    m_simulationConditions =
        std::make_shared<SimulationConditions>(orig.m_simulationConditions->clone());
    m_configuration = std::make_shared<FlightConfiguration>(orig.m_configuration->clone());

    m_configuration->copyStages(*orig.m_configuration);

    m_deployedRecoveryDevices->clear();
    m_deployedRecoveryDevices->addAll(*orig.m_deployedRecoveryDevices);

    m_motorStateList->clear();
    *m_motorStateList = *orig.m_motorStateList;

    m_eventQueue->clear();
    m_eventQueue->addAll(*orig.m_eventQueue);

    // WarningSet is not cloned.
    m_warnings = std::make_shared<WarningSet>();

    m_extraData->clear();
    *m_extraData = *orig.m_extraData;

    // The maximum altitude and its time are not copied, as in Java: they keep the values of a
    // new status.

    m_modId    = orig.m_modId;
    m_modIdAdd = orig.m_modIdAdd;
}

SimulationStatus::SimulationStatus(ShallowKey /*key*/, const SimulationStatus& orig)
  : m_simulationConditions(orig.m_simulationConditions),
    m_configuration(orig.m_configuration),
    m_flightDataBranch(orig.m_flightDataBranch),
    m_time(orig.m_time),
    m_position(orig.m_position),
    m_worldPosition(orig.m_worldPosition),
    m_velocity(orig.m_velocity),
    m_orientation(orig.m_orientation),
    m_rotationVelocity(orig.m_rotationVelocity),
    m_maxZVelocity(orig.m_maxZVelocity),
    m_startWarningsTime(orig.m_startWarningsTime),
    m_tumbleDetector(orig.m_tumbleDetector),
    m_separatedStage(orig.m_separatedStage),
    m_effectiveLaunchRodLength(orig.m_effectiveLaunchRodLength),
    m_motorStateList(orig.m_motorStateList),
    m_simulationStartWallTime(orig.m_simulationStartWallTime),
    m_motorIgnited(orig.m_motorIgnited),
    m_liftoff(orig.m_liftoff),
    m_launchRodCleared(orig.m_launchRodCleared),
    m_apogeeReached(orig.m_apogeeReached),
    m_tumbling(orig.m_tumbling),
    m_landed(orig.m_landed),
    m_deployedRecoveryDevices(orig.m_deployedRecoveryDevices),
    m_eventQueue(orig.m_eventQueue),
    m_warnings(orig.m_warnings),
    m_extraData(orig.m_extraData),
    m_maxAlt(orig.m_maxAlt),
    m_maxAltTime(orig.m_maxAltTime),
    m_modId(orig.m_modId),
    m_modIdAdd(orig.m_modIdAdd)
{
}

SimulationStatus SimulationStatus::clone() const
{
    return {ShallowKey{}, *this};
}

void SimulationStatus::copyProperties(const SimulationStatus& orig) noexcept
{
    m_position         = orig.m_position;
    m_worldPosition    = orig.m_worldPosition;
    m_velocity         = orig.m_velocity;
    m_orientation      = orig.m_orientation;
    m_rotationVelocity = orig.m_rotationVelocity;
    // these are booleans, so no cloning (primitives).
    m_motorIgnited     = orig.m_motorIgnited;
    m_liftoff          = orig.m_liftoff;
    m_launchRodCleared = orig.m_launchRodCleared;
    m_apogeeReached    = orig.m_apogeeReached;
}

void SimulationStatus::setSimulationTime(double time) noexcept
{
    m_time  = time;
    m_modId = ModId{};
}

void SimulationStatus::setConfiguration(std::shared_ptr<FlightConfiguration> configuration)
{
    if (configuration == nullptr)
    {
        bug("A simulation status needs a flight configuration");
    }
    if (m_configuration != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_configuration = std::move(configuration);
}

std::vector<std::shared_ptr<MotorClusterState>> SimulationStatus::getActiveMotors() const
{
    std::vector<std::shared_ptr<MotorClusterState>> activeList;
    for (const std::shared_ptr<MotorClusterState>& state : *m_motorStateList)
    {
        const MotorClusterState& motorState = *state;
        if (m_configuration->isComponentActive(motorState.getMount()))
        {
            activeList.push_back(state);
        }
    }

    return activeList;
}

std::vector<const MotorClusterState*> SimulationStatus::getActiveMotorStates() const
{
    std::vector<const MotorClusterState*> activeList;
    for (const std::shared_ptr<MotorClusterState>& state : *m_motorStateList)
    {
        const MotorClusterState& motorState = *state;
        if (m_configuration->isComponentActive(motorState.getMount()))
        {
            activeList.push_back(state.get());
        }
    }

    return activeList;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
bool SimulationStatus::moveBurntOutMotor(const MotorConfigurationId& /*motor*/) const noexcept
{
    // get motor from normal list
    // remove motor from 'normal' list
    // add to spent list
    return false;
}

void SimulationStatus::setFlightDataBranch(std::shared_ptr<FlightDataBranch> flightDataBranch)
{
    if (m_flightDataBranch != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_flightDataBranch = std::move(flightDataBranch);
}

void SimulationStatus::setRocketPosition(const Coordinate& position) noexcept
{
    m_position = position;
    m_modId    = ModId{};
}

void SimulationStatus::setRocketWorldPosition(const WorldCoordinate& wc) noexcept
{
    m_worldPosition = wc;
    m_modId         = ModId{};
}

void SimulationStatus::setRocketVelocity(const Coordinate& velocity) noexcept
{
    m_velocity = velocity;
    m_modId    = ModId{};
}

void SimulationStatus::setRocketOrientationQuaternion(const Quaternion& orientation) noexcept
{
    m_orientation = orientation;
    m_modId       = ModId{};
}

void SimulationStatus::setRocketRotationVelocity(const Coordinate& rotation) noexcept
{
    m_rotationVelocity = rotation;
}

void SimulationStatus::setEffectiveLaunchRodLength(double effectiveLaunchRodLength) noexcept
{
    m_effectiveLaunchRodLength = effectiveLaunchRodLength;
    m_modId                    = ModId{};
}

void SimulationStatus::setSimulationStartWallTime(
    WallClock::time_point simulationStartWallTime) noexcept
{
    m_simulationStartWallTime = simulationStartWallTime;
    m_modId                   = ModId{};
}

void SimulationStatus::setMotorIgnited(bool motorIgnited) noexcept
{
    m_motorIgnited = motorIgnited;
    m_modId        = ModId{};
}

void SimulationStatus::setLiftoff(bool liftoff) noexcept
{
    m_liftoff = liftoff;
    m_modId   = ModId{};
}

void SimulationStatus::setLaunchRodCleared(bool launchRod) noexcept
{
    m_launchRodCleared = launchRod;
    if (launchRod)
    {
        m_startWarningsTime = getSimulationTime() + kWarningsWait;
    }
    m_modId = ModId{};
}

void SimulationStatus::setApogeeReached(bool apogeeReached) noexcept
{
    m_apogeeReached = apogeeReached;
    m_modId         = ModId{};
}

void SimulationStatus::setTumbling(bool tumbling) noexcept
{
    m_tumbling = tumbling;
    m_modId    = ModId{};
}

void SimulationStatus::setLanded(bool landed) noexcept
{
    m_landed = landed;
    m_modId  = ModId{};
}

void SimulationStatus::setMaxAlt(double maxAlt) noexcept
{
    m_maxAlt = maxAlt;
    m_modId  = ModId{};
}

void SimulationStatus::setMaxAltTime(double maxAltTime) noexcept
{
    m_maxAltTime = maxAltTime;
    m_modId      = ModId{};
}

void SimulationStatus::setWarnings(std::shared_ptr<WarningSet> warnings)
{
    if (m_warnings != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_warnings = std::move(warnings);
}

void SimulationStatus::addWarning(const Warning& warning)
{
    if (m_warnings == nullptr)
    {
        setWarnings(std::make_shared<WarningSet>());
    }

    // Only add a new SIM_WARN event if warning wasn't already present
    if (m_warnings->add(warning))
    {
        // For a variety of reasons, the Warning actually added to
        // the set may not be the one passed in. So we add the Warning
        // to the set, then read it again.
        const WarningSet& warnings = *m_warnings;
        const Warning*    stored   = warnings.find(warning);
        QTROCKET_ASSERT(stored != nullptr);

        if (m_flightDataBranch == nullptr)
        {
            bug("A warning was added to a simulation status without a flight data branch");
        }
        m_flightDataBranch->addEvent(FlightEvent(FlightEvent::Type::SIM_WARN, getSimulationTime(),
                                                 nullptr, FlightEvent::warningData(*stored)));
    }
}

void SimulationStatus::addWarnings(const WarningSet& warnings)
{
    for (const Warning& warning : warnings)
    {
        addWarning(warning);
    }
}

bool SimulationStatus::recordWarnings() const noexcept
{
    if (!m_launchRodCleared)
    {
        return false;
    }

    if (getSimulationTime() < m_startWarningsTime)
    {
        return false;
    }

    // Java writes the literal 0.2 here, not WARNINGS_VEL.
    return !(getRocketVelocity().z < getMaxZVelocity() * 0.2);
}

void SimulationStatus::addEvent(const FlightEvent& event)
{
    if (SimulationListenerHelper::fireAddFlightEvent(*this, event))
    {
        getEventQueue().add(event);
    }
}

void SimulationStatus::abortSimulation(SimulationAbort::Cause cause)
{
    const FlightEvent abortEvent(FlightEvent::Type::SIM_ABORT, getSimulationTime(), nullptr,
                                 SimulationAbort(cause));
    addEvent(abortEvent);
}

void SimulationStatus::removeUnattachedEvents()
{
    EventQueue::Iterator i = getEventQueue().iterator();
    while (i.hasNext())
    {
        if (!isAttached(i.next()))
        {
            i.remove();
        }
    }
}

bool SimulationStatus::isAttached(const FlightEvent& event) const
{
    const RocketComponent* source = event.getSource();
    return source == nullptr || source->getParent() == nullptr ||
           getConfiguration().isComponentActive(*source);
}

void SimulationStatus::setSimulationConditions(
    std::shared_ptr<SimulationConditions> simulationConditions)
{
    if (m_simulationConditions != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_simulationConditions = std::move(simulationConditions);
}

void SimulationStatus::putExtraData(const std::string& key, std::any value)
{
    m_extraData->insert_or_assign(key, std::move(value));
}

std::any* SimulationStatus::getExtraData(const std::string& key)
{
    const auto it = m_extraData->find(key);
    return it == m_extraData->end() ? nullptr : &it->second;
}

const std::any* SimulationStatus::getExtraData(const std::string& key) const
{
    const std::map<std::string, std::any>& extraData = *m_extraData;
    const auto                             it        = extraData.find(key);
    return it == extraData.end() ? nullptr : &it->second;
}

std::string SimulationStatus::toEventDebug() const
{
    std::string buf;
    for (const FlightEvent& event : getEventQueue())
    {
        buf += "      [t:";
        buf += displayName(event.getType());
        buf += " @";
        buf += Strings::javaDoubleToString(event.getTime());
        if (event.getSource() != nullptr)
        {
            buf += "  src:";
            buf += event.getSource()->getName();
        }
        if (event.hasData())
        {
            buf += "  data:";
            buf += std::visit(DataClassName{}, event.getData());
        }
        buf += "]\n";
    }
    return buf;
}

std::string SimulationStatus::toMotorsDebug() const
{
    std::string buf = "MotorState list:\n";
    for (const std::shared_ptr<MotorClusterState>& state : *m_motorStateList)
    {
        buf += "          [";
        buf += state->toDescription();
        buf += "]\n";
    }
    return buf;
}

void SimulationStatus::populateMotors()
{
    m_motorStateList->clear();
    for (const MotorConfiguration& motorConfig : m_configuration->getAllMotors())
    {
        m_motorStateList->push_back(std::make_shared<MotorClusterState>(motorConfig));
    }
}

double SimulationStatus::calculateEffectiveLaunchRodLength() const
{
    /*
     * Calculate the effective launch rod length taking into account launch lugs.
     * If no lugs are found, assume a tower launcher of full length.
     */
    double length      = m_simulationConditions->getLaunchRodLength();
    double lugPosition = std::numeric_limits<double>::quiet_NaN();
    for (const RocketComponent* c : m_configuration->getActiveComponents())
    {
        if (dynamic_cast<const LaunchLug*>(c) != nullptr)
        {
            const std::vector<Coordinate> absolute = c->toAbsolute(Coordinate(c->getLength()));
            QTROCKET_ASSERT(!absolute.empty());
            const double pos = absolute[0].x;
            if (std::isnan(lugPosition) || pos > lugPosition)
            {
                lugPosition = pos;
            }
        }
    }
    if (!std::isnan(lugPosition))
    {
        double maxX = 0;
        for (const Coordinate& c : m_configuration->getBounds())
        {
            // Java: if (c.getX() > maxX) maxX = c.getX();
            maxX = std::max(maxX, c.x);
        }
        if (maxX >= lugPosition)
        {
            length = MathUtil::javaMax(0, length - (maxX - lugPosition));
        }
    }
    return length;
}

void SimulationStatus::storeData()
{
    if (m_flightDataBranch == nullptr)
    {
        bug("A simulation status without a flight data branch cannot store its data");
    }
    FlightDataBranch& branch = *m_flightDataBranch;
    branch.addPoint();
    branch.setValue(type(FlightDataTypeId::TYPE_TIME), getSimulationTime());
    branch.setValue(type(FlightDataTypeId::TYPE_ALTITUDE), getRocketPosition().z);
    branch.setValue(type(FlightDataTypeId::TYPE_ALTITUDE_ABOVE_SEA),
                    getRocketWorldPosition().getAltitude());
    branch.setValue(type(FlightDataTypeId::TYPE_POSITION_X), getRocketPosition().x);
    branch.setValue(type(FlightDataTypeId::TYPE_POSITION_Y), getRocketPosition().y);

    branch.setValue(type(FlightDataTypeId::TYPE_LATITUDE),
                    getRocketWorldPosition().getLatitudeDeg());
    branch.setValue(type(FlightDataTypeId::TYPE_LONGITUDE),
                    getRocketWorldPosition().getLongitudeDeg());

    branch.setValue(type(FlightDataTypeId::TYPE_POSITION_XY),
                    MathUtil::hypot(getRocketPosition().x, getRocketPosition().y));
    // (x, y) instead of (y, x) because 0 is north
    branch.setValue(type(FlightDataTypeId::TYPE_POSITION_DIRECTION),
                    std::fmod(std::atan2(getRocketPosition().x, getRocketPosition().y) +
                                  (2.0 * std::numbers::pi),
                              2.0 * std::numbers::pi));

    branch.setValue(type(FlightDataTypeId::TYPE_VELOCITY_XY),
                    MathUtil::hypot(getRocketVelocity().x, getRocketVelocity().y));
    branch.setValue(type(FlightDataTypeId::TYPE_VELOCITY_Z), getRocketVelocity().z);
    setMaxZVelocity(MathUtil::javaMax(getRocketVelocity().z, getMaxZVelocity()));

    branch.setValue(type(FlightDataTypeId::TYPE_VELOCITY_TOTAL), getRocketVelocity().length());

    const Coordinate c     = getRocketOrientationQuaternion().rotateZ();
    const double     theta = std::atan2(c.z, MathUtil::hypot(c.x, c.y));
    //(x, y) instead of (y, x) because 0 is north
    const double phi =
        std::fmod(std::atan2(c.x, c.y) + (2.0 * std::numbers::pi), 2.0 * std::numbers::pi);

    branch.setValue(type(FlightDataTypeId::TYPE_ORIENTATION_THETA), theta);
    branch.setValue(type(FlightDataTypeId::TYPE_ORIENTATION_PHI), phi);
    branch.setValue(type(FlightDataTypeId::TYPE_COMPUTATION_TIME),
                    static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            WallClock::now() - getSimulationStartWallTime())
                                            .count()) /
                        1000000000.0);
}

void SimulationStatus::setMaxZVelocity(double zVel) noexcept
{
    if (zVel > m_maxZVelocity)
    {
        m_maxZVelocity = zVel;
        m_modId        = ModId{};
    }
}

}  // namespace QtRocket
