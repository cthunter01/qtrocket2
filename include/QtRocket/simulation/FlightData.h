#pragma once

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/Mutable.h"

namespace QtRocket
{

class Rocket;

/// The result of a simulation, or flight data imported into a document (OpenRocket's
/// simulation/FlightData):
/// - the generally interesting values of the flight (the maximum altitude, velocity,
///   acceleration and Mach number, the time to apogee, the flight time, the velocities at the
///   ground hit, off the launch rod and at deployment, the optimum ejection delay), NaN while
///   unknown;
/// - zero or more branches of flight data, the sustainer's first;
/// - the warnings raised during the simulation.
///
/// A FlightData is mutable until immute(), which also makes the warning set and every branch
/// immutable; afterwards addBranch() throws BugError.
///
/// Sharing: a branch is held by std::shared_ptr, because the simulation status that writes a
/// branch and the flight data it is part of hold the same branch (Java: the same object).
/// A FlightData is not copied (its branches would be shared between the copies): it is moved,
/// or copied on purpose with clone(), which copies the branches too.
///
/// The simulated rocket (an addition): the events of the branches, and the motor states they
/// carry, point to components of the rocket the simulation ran on, the engine's private copy
/// (see FlightEvent). setSimulatedRocket() makes the flight data a co-owner of that rocket, so
/// that the pointers stay valid as long as the data lives; clone() keeps it. Flight data read
/// from a file has none: its events know their sources by id.
///
/// Deviations from OpenRocket:
/// - NaN_DATA is nanData(), made on first use.
/// - getBranch() throws BugError for an index out of range (Java:
///   IndexOutOfBoundsException), getStageNr() gives nullopt for Java's -1, and getBranches()
///   is read-only (Java hands out the list itself, which lets a caller add a branch to
///   immutable flight data).
/// - Not ported: the debug log line at the end of calculateInterestingValues().
/// As in Java, calculateInterestingValues() does not check mutability, and clone() does not
/// copy the optimum delay (it is NaN in the copy) and gives mutable flight data.
class FlightData
{
public:
    /// Flight data with no content: no branch, no warning and every value NaN. Mutable.
    FlightData() = default;

    /// Flight data with no branch but these summary values. Mutable.
    FlightData(double maxAltitude, double maxVelocity, double maxAcceleration, double maxMachNumber,
               double timeToApogee, double flightTime, double groundHitVelocity,
               double launchRodVelocity, double deploymentVelocity, double optimumDelay) noexcept;

    /// Flight data with @p branches, in that order, and the summary values
    /// calculateInterestingValues() takes from the first of them. Mutable.
    /// @throws BugError when a branch is null
    explicit FlightData(std::span<const std::shared_ptr<FlightDataBranch>> branches);

    /// The same with the branches written in place.
    FlightData(std::initializer_list<std::shared_ptr<FlightDataBranch>> branches);

    FlightData(const FlightData&)                = delete;
    FlightData& operator=(const FlightData&)     = delete;
    FlightData(FlightData&&) noexcept            = default;
    FlightData& operator=(FlightData&&) noexcept = default;
    ~FlightData()                                = default;

    /// Immutable flight data with no content (Java: NaN_DATA).
    [[nodiscard]] static const FlightData& nanData();

    /// The warnings of the simulation. A simulation stores its warnings in this set.
    [[nodiscard]] WarningSet&       getWarningSet() noexcept { return m_warnings; }
    [[nodiscard]] const WarningSet& getWarningSet() const noexcept { return m_warnings; }

    /// The warning of the SIM_WARN event @p event as the warning set has it now: the one with
    /// the id of the event's warning, or null when the event carries no warning or the set no
    /// longer has it. An addition: in Java the event and the set hold the same object, so the
    /// event sees what the set made of the warning later (see FlightEvent). The pointer is valid
    /// as the pointers WarningSet hands out are.
    [[nodiscard]] const Warning* findWarning(const FlightEvent& event) const;

    /// Appends @p branch. A refusal is raised at @p where, the call site by default.
    /// @throws BugError when the flight data is immutable, or @p branch is null
    void addBranch(std::shared_ptr<FlightDataBranch> branch,
                   std::source_location              where = std::source_location::current());

    [[nodiscard]] std::size_t getBranchCount() const noexcept { return m_branches.size(); }

    /// The branch @p stageNr (0: the sustainer's).
    /// @throws BugError when @p stageNr is not below getBranchCount()
    [[nodiscard]] FlightDataBranch&       getBranch(std::size_t stageNr);
    [[nodiscard]] const FlightDataBranch& getBranch(std::size_t stageNr) const;

    /// The index of @p branch (the very object) among the branches, or nullopt.
    [[nodiscard]] std::optional<std::size_t> getStageNr(
        const FlightDataBranch& branch) const noexcept;

    /// The branches, in order.
    [[nodiscard]] const std::vector<std::shared_ptr<FlightDataBranch>>& getBranches() const noexcept
    {
        return m_branches;
    }

    [[nodiscard]] double getMaxAltitude() const noexcept { return m_maxAltitude; }
    [[nodiscard]] double getMaxVelocity() const noexcept { return m_maxVelocity; }
    /// The maximum acceleration before the first recovery device deployment.
    [[nodiscard]] double getMaxAcceleration() const noexcept { return m_maxAcceleration; }
    [[nodiscard]] double getMaxMachNumber() const noexcept { return m_maxMachNumber; }
    [[nodiscard]] double getTimeToApogee() const noexcept { return m_timeToApogee; }
    [[nodiscard]] double getFlightTime() const noexcept { return m_flightTime; }
    [[nodiscard]] double getGroundHitVelocity() const noexcept { return m_groundHitVelocity; }
    [[nodiscard]] double getLaunchRodVelocity() const noexcept { return m_launchRodVelocity; }
    [[nodiscard]] double getDeploymentVelocity() const noexcept { return m_deploymentVelocity; }
    [[nodiscard]] double getOptimumDelay() const noexcept { return m_optimumDelay; }

    /// Takes the summary values from the first branch (nothing happens without a branch):
    /// - the maximum altitude, total velocity and Mach number: the maxima of those columns (NaN
    ///   for a column the branch does not have); the flight time: the last TIME;
    /// - without a TIME or an ALTITUDE column, the time to apogee and the maximum acceleration
    ///   become NaN and nothing else changes;
    /// - the time to apogee: the TIME of the first row whose altitude equals the maximum
    ///   altitude (MathUtil::equals()), NaN when there is none;
    /// - the optimum delay: the branch's getOptimumDelay();
    /// - the launch rod, deployment and ground hit velocities: the total velocity interpolated
    ///   (MathUtil::interpolate()) at the time of each LAUNCHROD, RECOVERY_DEVICE_DEPLOYMENT and
    ///   GROUND_HIT event of the branch, the last event of a type winning; unchanged without
    ///   such an event, NaN without the velocity column or for a time outside the data;
    /// - the maximum acceleration: with a total acceleration column, the largest value of the
    ///   rows before the time of the earliest RECOVERY_DEVICE_DEPLOYMENT event (every row
    ///   without one), and at least 0; NaN without that column.
    void calculateInterestingValues();

    /// Makes the flight data, its warning set and every branch immutable; repeated calls do
    /// nothing. @p where, the call site by default, is what a refused change reports.
    void immute(std::source_location where = std::source_location::current()) noexcept;

    [[nodiscard]] bool isMutable() const noexcept { return m_mutable.isMutable(); }

    /// Java's clone(): mutable flight data with copies of the warnings, clones of the branches
    /// (FlightDataBranch::clone()) and the summary values, but for the optimum delay, which is
    /// NaN in the copy, as in Java. The copy shares the simulated rocket.
    [[nodiscard]] FlightData clone() const;

    /// Makes the flight data a co-owner of @p rocket, the rocket its events point into (an
    /// addition: see the class comment); null drops it. Not part of the data: allowed on
    /// immutable flight data.
    void setSimulatedRocket(std::shared_ptr<const Rocket> rocket) noexcept
    {
        m_simulatedRocket = std::move(rocket);
    }

    /// The rocket given to setSimulatedRocket(), or null.
    [[nodiscard]] const std::shared_ptr<const Rocket>& getSimulatedRocket() const noexcept
    {
        return m_simulatedRocket;
    }

private:
    /// The largest total acceleration of the first branch before the first recovery device
    /// deployment (Java: calculateMaxAcceleration()).
    [[nodiscard]] double calculateMaxAcceleration() const;

    Mutable m_mutable;
    /// Declared before the branches, so that it is destroyed after them: the events of the
    /// branches point into it.
    std::shared_ptr<const Rocket>                  m_simulatedRocket;
    std::vector<std::shared_ptr<FlightDataBranch>> m_branches;
    WarningSet                                     m_warnings;

    double m_maxAltitude{std::numeric_limits<double>::quiet_NaN()};
    double m_maxVelocity{std::numeric_limits<double>::quiet_NaN()};
    double m_maxAcceleration{std::numeric_limits<double>::quiet_NaN()};
    double m_maxMachNumber{std::numeric_limits<double>::quiet_NaN()};
    double m_timeToApogee{std::numeric_limits<double>::quiet_NaN()};
    double m_flightTime{std::numeric_limits<double>::quiet_NaN()};
    double m_groundHitVelocity{std::numeric_limits<double>::quiet_NaN()};
    double m_launchRodVelocity{std::numeric_limits<double>::quiet_NaN()};
    double m_deploymentVelocity{std::numeric_limits<double>::quiet_NaN()};
    double m_optimumDelay{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
