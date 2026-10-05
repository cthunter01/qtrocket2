#include "QtRocket/simulation/SimulationStepperMethod.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

// The texts are the English ones of OpenRocket's l10n/messages.properties, keys
// SimulationStepperMethod.<constant>.name, .shortName and .desc.

std::string_view getName(SimulationStepperMethod method) noexcept
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return "6-DOF Runge-Kutta 4";
        case SimulationStepperMethod::RK6:
            return "6-DOF Runge-Kutta 6";
    }
    return "6-DOF Runge-Kutta 4";  // not reached: the switch covers every method
}

std::string_view getShortName(SimulationStepperMethod method) noexcept
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return "RK4";
        case SimulationStepperMethod::RK6:
            return "RK6";
    }
    return "RK4";  // not reached
}

std::string_view getDescription(SimulationStepperMethod method) noexcept
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return "6-DOF Runge-Kutta 4";
        case SimulationStepperMethod::RK6:
            return "6-DOF Runge-Kutta 6: Slower than RK4, but more accurate in some cases";
    }
    return "6-DOF Runge-Kutta 4";  // not reached
}

std::string_view simulationStepperMethodName(SimulationStepperMethod method) noexcept
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return "RK4";
        case SimulationStepperMethod::RK6:
            return "RK6";
    }
    return "RK4";  // not reached
}

std::string_view toString(SimulationStepperMethod method) noexcept
{
    switch (method)
    {
        case SimulationStepperMethod::RK4:
            return "rk4";
        case SimulationStepperMethod::RK6:
            return "rk6";
    }
    return "rk4";  // not reached
}

std::optional<SimulationStepperMethod> simulationStepperMethodFromString(std::string_view text)
{
    for (const SimulationStepperMethod method : kAllSimulationStepperMethods)
    {
        if (Strings::orkEnumNameMatches(text, simulationStepperMethodName(method)))
        {
            return method;
        }
    }
    return std::nullopt;
}

std::optional<SimulationStepperMethod> simulationStepperMethodFromName(
    std::string_view name) noexcept
{
    for (const SimulationStepperMethod method : kAllSimulationStepperMethods)
    {
        if (simulationStepperMethodName(method) == name)
        {
            return method;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
