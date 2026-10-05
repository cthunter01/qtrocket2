#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// Which stepper integrates the flight (OpenRocket's simulation/SimulationStepperMethod).
///
/// Java's enum has a translated name, short name and description per constant; here they are free
/// functions returning the English texts of OpenRocket's messages.properties, as for the other
/// ported enums. The spellings, named as for GravityModelType and WindModelType:
/// - getName(): the name the simulation options show, also what Java's toString() returns.
/// - simulationStepperMethodName(): Enum.name() ("RK4", "RK6"), what the preference store keeps
///   (Preferences::getSimulationStepperMethodName()).
/// - toString(): the .ork spelling of <simulationsteppermethod>, the lower-cased constant name
///   ("rk4", "rk6"). Deviation: Java's toString() is getName().
enum class SimulationStepperMethod
{
    /// Perform computations using an RK4 stepper.
    RK4,
    /// Perform computations using an RK6 stepper.
    RK6,
};

/// Every method, in declaration order (SimulationStepperMethod.values()).
inline constexpr std::array<SimulationStepperMethod, 2> kAllSimulationStepperMethods{
    SimulationStepperMethod::RK4, SimulationStepperMethod::RK6};

/// Java's getName() (and toString()): "6-DOF Runge-Kutta 4" or "6-DOF Runge-Kutta 6".
[[nodiscard]] std::string_view getName(SimulationStepperMethod method) noexcept;

/// Java's getShortName(), a concise identifier of the stepper: "RK4" or "RK6".
[[nodiscard]] std::string_view getShortName(SimulationStepperMethod method) noexcept;

/// Java's getDescription(): "6-DOF Runge-Kutta 4" or "6-DOF Runge-Kutta 6: Slower than RK4, but
/// more accurate in some cases".
[[nodiscard]] std::string_view getDescription(SimulationStepperMethod method) noexcept;

/// Java's Enum.name(): "RK4" or "RK6", the preference spelling.
[[nodiscard]] std::string_view simulationStepperMethodName(SimulationStepperMethod method) noexcept;

/// The .ork spelling, the text of a <simulationsteppermethod> element: "rk4" or "rk6"
/// (OpenRocketSaver: name().toLowerCase(Locale.ENGLISH)). Not Java's toString() (see getName()).
[[nodiscard]] std::string_view toString(SimulationStepperMethod method) noexcept;

/// The method a .ork file names (DocumentConfig.findEnum): @p text, trimmed, must be exactly the
/// lower-cased constant name, so "rk4" and " rk6 " match and "RK4" does not. Anything else is
/// nullopt (Java: null, which the reader reports as "Unknown Simulation Stepper").
[[nodiscard]] std::optional<SimulationStepperMethod> simulationStepperMethodFromString(
    std::string_view text);

/// Java's Enum.valueOf(): the method whose constant name is exactly @p name ("RK4", "RK6"), as
/// the preference store keeps it, or nullopt (Java: IllegalArgumentException).
[[nodiscard]] std::optional<SimulationStepperMethod> simulationStepperMethodFromName(
    std::string_view name) noexcept;

}  // namespace QtRocket
