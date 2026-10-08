#pragma once

// The scenarios of the flights with OpenRocket's example extensions RollControl and AirStart,
// built and run as the Java probes that pinned them build and run them
// (probes/tier9a-extensions/java: ExtPins2.java; for the last three scenarios, which the review
// asked for, probes/tier9a-fix-extensions/java: ExtPins3.java, the same program with those
// scenarios), and printed as the probes print them, so that a run here and a run of OpenRocket
// can be compared line by line (ExtensionFlightPins.h holds the probes' lines). Test-only; used
// by extension_flight_tests.cpp.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"

namespace QtRocket::Test
{

/// One scenario of the probe: a rocket with the flight configuration that is simulated, the
/// extensions of the simulation, and how it is run.
struct ExtensionFlightScenario
{
    std::string             id;
    std::unique_ptr<Rocket> rocket;
    FlightConfigurationId   fcid;
    double                  rodLength{1.0};
    /// The fin set whose cant the probe watches, in the caller's rocket and in the simulated
    /// one: the one a RollControl of the scenario turns.
    std::string                                       watchedFinSet;
    std::vector<std::shared_ptr<SimulationExtension>> extensions;
    /// Whether the run is the golden harness's (the engine on conditions with the jitter
    /// removal listeners around the extensions' listeners) and not Simulation::simulate().
    bool jitterRemoved{false};
    /// The start time of the scenario's RollControl.
    double startTime{0.5};
    /// The launch rod: its angle from the vertical and its direction, rad.
    double rodAngle{0.0};
    double rodDirection{0.0};
    /// Whether the flight is the probe's "windy" one: a steady wind of 3 m/s from the direction
    /// 0.7 rad (without turbulence, which is not OpenRocket's random sequence here), a launch
    /// site at 350 m, 28.61 degrees north and 80.6 degrees west with 293.15 K and 97000 Pa in
    /// place of the ISA atmosphere, the WGS84 geodetic computation and the RK6 stepper; else
    /// calm air, the ISA atmosphere at sea level, flat-earth coordinates and RK4.
    bool windy{false};
};

/// The ids of the probe's scenarios, in its order.
[[nodiscard]] const std::vector<std::string>& extensionFlightScenarioIds();

/// The scenario @p id, newly built.
/// @throws BugError for an id the probe does not have
[[nodiscard]] ExtensionFlightScenario makeExtensionFlightScenario(std::string_view id);

/// Runs @p scenario with the time step @p timeStep and returns the probe's lines for it, the
/// "SCENARIO" line first. @p lastListener, when not null, is one more listener of the run,
/// after the extensions' listeners and the probe's own: a perturbation, for the measurement of
/// how far a difference in the last bits of a flight carries.
[[nodiscard]] std::vector<std::string> runExtensionFlightScenario(
    ExtensionFlightScenario& scenario, double timeStep,
    const std::shared_ptr<SimulationListener>& lastListener = nullptr);

/// The tokens of a probe line: its parts between spaces.
[[nodiscard]] std::vector<std::string_view> probeTokens(std::string_view line);

/// What kind of line the tokens @p tokens are: "summary", "branch", "spy", "caller", "result",
/// "column finCant", "event APOGEE", ... (the first token, without what follows a '=' in it,
/// and for an event or a column the second token as well).
[[nodiscard]] std::string probeLineKind(const std::vector<std::string_view>& tokens);

}  // namespace QtRocket::Test
