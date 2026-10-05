#include "QtRocket/simulation/FlightData.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] const FlightDataType& type(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

/// The total velocity of @p branch at @p time, interpolated over its TIME column @p times
/// (Java: MathUtil.interpolate(time, velocity, t), which is NaN for a null list).
[[nodiscard]] double velocityAt(const FlightDataBranch& branch, const std::vector<double>& times,
                                double time)
{
    const std::vector<double>* velocity =
        branch.getView(type(FlightDataTypeId::TYPE_VELOCITY_TOTAL));
    if (velocity == nullptr)
    {
        return kNaN;
    }
    return MathUtil::interpolate(times, *velocity, time);
}

/// The TIME of the first row of @p altitude that equals @p maxAltitude, NaN when there is
/// none.
[[nodiscard]] double timeOfAltitude(const std::vector<double>& times,
                                    const std::vector<double>& altitude, double maxAltitude)
{
    std::size_t index = 0;
    for (const double alt : altitude)
    {
        if (MathUtil::equals(alt, maxAltitude))
        {
            break;
        }
        index++;
    }
    return index < times.size() ? times[index] : kNaN;
}

}  // namespace

FlightData::FlightData(double maxAltitude, double maxVelocity, double maxAcceleration,
                       double maxMachNumber, double timeToApogee, double flightTime,
                       double groundHitVelocity, double launchRodVelocity,
                       double deploymentVelocity, double optimumDelay) noexcept
  : m_maxAltitude(maxAltitude),
    m_maxVelocity(maxVelocity),
    m_maxAcceleration(maxAcceleration),
    m_maxMachNumber(maxMachNumber),
    m_timeToApogee(timeToApogee),
    m_flightTime(flightTime),
    m_groundHitVelocity(groundHitVelocity),
    m_launchRodVelocity(launchRodVelocity),
    m_deploymentVelocity(deploymentVelocity),
    m_optimumDelay(optimumDelay)
{
}

FlightData::FlightData(std::span<const std::shared_ptr<FlightDataBranch>> branches)
{
    for (const std::shared_ptr<FlightDataBranch>& branch : branches)
    {
        addBranch(branch);
    }
    calculateInterestingValues();
}

FlightData::FlightData(std::initializer_list<std::shared_ptr<FlightDataBranch>> branches)
  : FlightData(std::span<const std::shared_ptr<FlightDataBranch>>{branches})
{
}

FlightData& FlightData::operator=(FlightData&& other) noexcept
{
    // A member-wise assignment would release the old rocket before the old branches. The old
    // contents go to `incoming` instead, and leave through its destructor.
    FlightData incoming(std::move(other));
    swap(incoming);
    return *this;
}

void FlightData::swap(FlightData& other) noexcept
{
    std::swap(m_mutable, other.m_mutable);
    std::swap(m_simulatedRocket, other.m_simulatedRocket);
    std::swap(m_branches, other.m_branches);
    std::swap(m_warnings, other.m_warnings);
    std::swap(m_maxAltitude, other.m_maxAltitude);
    std::swap(m_maxVelocity, other.m_maxVelocity);
    std::swap(m_maxAcceleration, other.m_maxAcceleration);
    std::swap(m_maxMachNumber, other.m_maxMachNumber);
    std::swap(m_timeToApogee, other.m_timeToApogee);
    std::swap(m_flightTime, other.m_flightTime);
    std::swap(m_groundHitVelocity, other.m_groundHitVelocity);
    std::swap(m_launchRodVelocity, other.m_launchRodVelocity);
    std::swap(m_deploymentVelocity, other.m_deploymentVelocity);
    std::swap(m_optimumDelay, other.m_optimumDelay);
}

const FlightData& FlightData::nanData()
{
    static const FlightData kNanData = [] {
        FlightData data;
        data.immute();
        return data;
    }();
    return kNanData;
}

const Warning* FlightData::findWarning(const FlightEvent& event) const
{
    const std::shared_ptr<const Warning> warning = event.getWarning();
    return warning != nullptr ? m_warnings.findById(warning->id()) : nullptr;
}

void FlightData::addBranch(std::shared_ptr<FlightDataBranch> branch, std::source_location where)
{
    m_mutable.check(where);
    if (branch == nullptr)
    {
        bug("A flight data branch must not be null", where);
    }
    m_branches.push_back(std::move(branch));
}

FlightDataBranch& FlightData::getBranch(std::size_t stageNr)
{
    if (stageNr >= m_branches.size())
    {
        bug(std::format("Index {} out of bounds for length {}", stageNr, m_branches.size()));
    }
    return *m_branches[stageNr];
}

const FlightDataBranch& FlightData::getBranch(std::size_t stageNr) const
{
    if (stageNr >= m_branches.size())
    {
        bug(std::format("Index {} out of bounds for length {}", stageNr, m_branches.size()));
    }
    return *m_branches[stageNr];
}

std::optional<std::size_t> FlightData::getStageNr(const FlightDataBranch& branch) const noexcept
{
    for (std::size_t i = 0; i < m_branches.size(); i++)
    {
        if (m_branches[i].get() == &branch)
        {
            return i;
        }
    }
    return std::nullopt;
}

void FlightData::calculateInterestingValues()
{
    if (m_branches.empty())
    {
        return;
    }

    const FlightDataBranch& branch = *m_branches.front();
    m_maxAltitude                  = branch.getMaximum(type(FlightDataTypeId::TYPE_ALTITUDE));
    m_maxVelocity                  = branch.getMaximum(type(FlightDataTypeId::TYPE_VELOCITY_TOTAL));
    m_maxMachNumber                = branch.getMaximum(type(FlightDataTypeId::TYPE_MACH_NUMBER));
    m_flightTime                   = branch.getLast(type(FlightDataTypeId::TYPE_TIME));

    // Time to apogee
    const std::vector<double>* time     = branch.getView(type(FlightDataTypeId::TYPE_TIME));
    const std::vector<double>* altitude = branch.getView(type(FlightDataTypeId::TYPE_ALTITUDE));

    if (time == nullptr || altitude == nullptr)
    {
        m_timeToApogee    = kNaN;
        m_maxAcceleration = kNaN;
        return;
    }
    m_timeToApogee = timeOfAltitude(*time, *altitude, m_maxAltitude);

    m_optimumDelay = branch.getOptimumDelay();

    // Launch rod velocity + deployment velocity + ground hit velocity
    for (const FlightEvent& event : branch.getEvents())
    {
        if (event.getType() == FlightEvent::Type::LAUNCHROD)
        {
            m_launchRodVelocity = velocityAt(branch, *time, event.getTime());
        }
        else if (event.getType() == FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT)
        {
            m_deploymentVelocity = velocityAt(branch, *time, event.getTime());
        }
        else if (event.getType() == FlightEvent::Type::GROUND_HIT)
        {
            m_groundHitVelocity = velocityAt(branch, *time, event.getTime());
        }
    }

    // Max. acceleration (must be after apogee time)
    if (branch.getView(type(FlightDataTypeId::TYPE_ACCELERATION_TOTAL)) != nullptr)
    {
        m_maxAcceleration = calculateMaxAcceleration();
    }
    else
    {
        m_maxAcceleration = kNaN;
    }
}

void FlightData::immute(std::source_location where) noexcept
{
    m_mutable.immute(where);
    m_warnings.immute(where);
    for (const std::shared_ptr<FlightDataBranch>& branch : m_branches)
    {
        branch->immute(where);
    }
}

FlightData FlightData::clone() const
{
    FlightData copy;
    copy.m_warnings.addAll(m_warnings);
    for (const std::shared_ptr<FlightDataBranch>& branch : m_branches)
    {
        copy.m_branches.push_back(std::make_shared<FlightDataBranch>(branch->clone()));
    }
    copy.m_maxAltitude        = m_maxAltitude;
    copy.m_maxVelocity        = m_maxVelocity;
    copy.m_maxAcceleration    = m_maxAcceleration;
    copy.m_maxMachNumber      = m_maxMachNumber;
    copy.m_timeToApogee       = m_timeToApogee;
    copy.m_flightTime         = m_flightTime;
    copy.m_groundHitVelocity  = m_groundHitVelocity;
    copy.m_launchRodVelocity  = m_launchRodVelocity;
    copy.m_deploymentVelocity = m_deploymentVelocity;
    // Java does not copy the optimum delay.
    copy.m_simulatedRocket = m_simulatedRocket;
    return copy;
}

double FlightData::calculateMaxAcceleration() const
{
    // End check at first recovery device deployment
    double endTime = std::numeric_limits<double>::max();

    const FlightDataBranch& branch = getBranch(0);
    for (const FlightEvent& event : branch.getEvents())
    {
        if (event.getType() == FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT &&
            event.getTime() < endTime)
        {
            endTime = event.getTime();
        }
    }

    const std::vector<double>* time = branch.getView(type(FlightDataTypeId::TYPE_TIME));
    const std::vector<double>* acceleration =
        branch.getView(type(FlightDataTypeId::TYPE_ACCELERATION_TOTAL));

    if (time == nullptr || acceleration == nullptr)
    {
        return kNaN;
    }

    double max = 0;

    for (std::size_t i = 0; i < time->size(); i++)
    {
        if ((*time)[i] >= endTime)
        {
            break;
        }
        // (max < a) ? a : max, as Java's `if (a > max) max = a`: a NaN acceleration is skipped.
        max = std::max(max, (*acceleration)[i]);
    }

    return max;
}

}  // namespace QtRocket
