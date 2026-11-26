#pragma once

// The inputs of the interop check (tools/openrocket-goldens/InteropCheck.java, README.md there,
// "Interop check") whose saves the GUI tests write, which no manifest lists: InteropCheck's names
// for them, which the GUI's DISABLED_ writers (tests/gui/exit/*InteropSave*) give their files
// (<input>.<form>.ork) and the core's writer (OrkInteropSaves of ork_interop_saves_tests.cpp)
// removes as the saves of an earlier run before it writes its own.

#include <array>
#include <cstddef>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "goldens/GoldenGuiReference.h"

namespace QtRocket::Test
{

/// The input of the GUI's from-scratch design (FromScratchInteropSave of
/// tests/gui/exit/from_scratch_exit_tests.cpp; InteropCheck.GUI_INPUT).
inline constexpr std::string_view kGuiFromScratchInput = "gui-from-scratch";

/// A flight QtRocket's GUI runs for the interop check's mode of flight data QtRocket ran (plan
/// section 11.6 E7; FlightDataInteropSave of tests/gui/exit/flight_data_interop_tests.cpp,
/// InteropCheck.GUI_RUNS): an example opened with File > Open, one of its simulations run with
/// the Run button of the Flight simulations tab on the application's executor, and the design
/// saved with Save as, its simulation data included.
struct GuiFlightRun
{
    /// InteropCheck's name of the save's input: "gui-run-" and the example's input.
    std::string_view input;
    /// The example's input of the goldens' manifest ("example-a-simple-model-rocket"),
    /// findGoldenExample() of GoldenExamples.h.
    std::string_view example;
    /// The index of the simulation that runs, from 0 (Java's getSimulation(index)).
    std::size_t simulation{0};
};

/// What GoogleTest prints for a flight that is the parameter of a test (and ctest's test name):
/// its input.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
inline void PrintTo(const GuiFlightRun& run, std::ostream* out)
{
    *out << run.input;
}

/// The flights (plan section 11.6: fresh runs in tests fly only "A simple model rocket" #1,
/// "Clustered motors" #1 or "3D printable" #1): the first simulation of "A simple model rocket"
/// and of "Clustered motors". InteropCheck.GUI_RUNS has the same list.
inline constexpr std::array<GuiFlightRun, 2> kGuiFlightRuns{
    GuiFlightRun{.input      = "gui-run-example-a-simple-model-rocket",
                 .example    = "example-a-simple-model-rocket",
                 .simulation = 0},
    GuiFlightRun{.input      = "gui-run-example-clustered-motors",
                 .example    = "example-clustered-motors",
                 .simulation = 0}};

/// Every input of the GUI's saves, in InteropCheck's order: kGuiFromScratchInput; "gui-<scenario>"
/// of the scenarios of the tabs (kGuiTabScenarios, the Motors & Configuration tab's, then
/// kGuiSimulationsScenario, the Flight simulations tab's: FlightConfigurationsInteropSave of
/// flight_configurations_exit_tests.cpp and SimulationsInteropSave of simulations_exit_tests.cpp,
/// InteropCheck.GUI_TAB_SCENARIOS); then the inputs of kGuiFlightRuns.
[[nodiscard]] inline std::vector<std::string> guiInteropInputs()
{
    std::vector<std::string> inputs{std::string(kGuiFromScratchInput)};
    for (const std::string_view scenario : kGuiTabScenarios)
    {
        inputs.push_back(std::format("gui-{}", scenario));
    }
    inputs.push_back(std::format("gui-{}", kGuiSimulationsScenario));
    for (const GuiFlightRun& run : kGuiFlightRuns)
    {
        inputs.emplace_back(run.input);
    }
    return inputs;
}

}  // namespace QtRocket::Test
