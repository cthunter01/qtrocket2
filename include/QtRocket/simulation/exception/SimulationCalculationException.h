#pragma once

#include <exception>
#include <memory>
#include <string>

#include "QtRocket/simulation/exception/SimulationException.h"

namespace QtRocket
{

class FlightDataBranch;

/// Thrown when a computation of the simulation has gone wrong, for example when values have
/// exceeded reasonable bounds or become NaN (OpenRocket's simulation/exception/
/// SimulationCalculationException). It carries the flight data branch that was being written,
/// so that what was computed up to the failure can still be shown.
///
/// The branch is shared with the simulation status and the flight data that hold it (Java: the
/// same object); it may be null.
///
/// Lifetime: the exception co-owns the branch, not the rocket the simulation ran on. The name
/// and the columns of the branch are safe for as long as the exception (or a copy of it) lives.
/// The events of the branch are not: their sources, and the mounts of the motor states they
/// carry, point into the simulated rocket (see FlightEvent), and are valid only while that
/// rocket lives. Whoever catches the exception outside the simulation reads the name and the
/// columns only (as OpenRocket's one reader of the branch does), unless it holds the rocket:
/// the FlightData the engine fills is the holder (FlightData::setSimulatedRocket()). In Java
/// the exception keeps everything reachable.
class SimulationCalculationException : public SimulationException
{
public:
    /// An exception without a message and without a branch.
    SimulationCalculationException() = default;

    /// An exception with the message @p message for the branch @p dataBranch.
    SimulationCalculationException(const std::string&                message,
                                   std::shared_ptr<FlightDataBranch> dataBranch);

    /// An exception caused by @p cause (whose message it takes) for the branch @p dataBranch.
    SimulationCalculationException(const std::exception&             cause,
                                   std::shared_ptr<FlightDataBranch> dataBranch);

    /// An exception with the message @p message, caused by @p cause, for the branch
    /// @p dataBranch.
    SimulationCalculationException(const std::string& message, const std::exception& cause,
                                   std::shared_ptr<FlightDataBranch> dataBranch);

    /// The branch that was being written when the computation failed, or null.
    [[nodiscard]] const std::shared_ptr<FlightDataBranch>& getFlightDataBranch() const noexcept
    {
        return m_flightDataBranch;
    }

private:
    std::shared_ptr<FlightDataBranch> m_flightDataBranch;
};

}  // namespace QtRocket
