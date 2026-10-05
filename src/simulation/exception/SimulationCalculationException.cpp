#include "QtRocket/simulation/exception/SimulationCalculationException.h"

#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "QtRocket/simulation/exception/SimulationException.h"

namespace QtRocket
{

SimulationCalculationException::SimulationCalculationException(
    const std::string& message, std::shared_ptr<FlightDataBranch> dataBranch)
  : SimulationException(message), m_flightDataBranch(std::move(dataBranch))
{
}

SimulationCalculationException::SimulationCalculationException(
    const std::exception& cause, std::shared_ptr<FlightDataBranch> dataBranch)
  : SimulationException(cause), m_flightDataBranch(std::move(dataBranch))
{
}

SimulationCalculationException::SimulationCalculationException(
    const std::string& message, const std::exception& cause,
    std::shared_ptr<FlightDataBranch> dataBranch)
  : SimulationException(message, cause), m_flightDataBranch(std::move(dataBranch))
{
}

}  // namespace QtRocket
