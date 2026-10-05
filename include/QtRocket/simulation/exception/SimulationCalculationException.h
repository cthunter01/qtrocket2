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
