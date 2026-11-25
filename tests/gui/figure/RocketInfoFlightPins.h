#pragma once

// The flight block of OpenRocket's design view overlay (RocketInfo.drawFlightInformation()) as
// OpenRocket shows it for each design of the exit suite OpenDesignsExitGolden and a few states of
// the probe's own: tests/data/goldens/gui/rocket-info/flight-blocks.json, which
// tools/openrocket-gui-pins writes (the rocket-info family's RocketInfoFlightProbe; its README
// says how). Read once per test process. rocket_info_flight_golden_tests.cpp compares QtRocket's
// RocketInfo and BackgroundSimulation with it, and the exit suite of the flight block
// (exit/open_designs_flight_block_exit_tests.cpp, plan 11.6 E5) a design's window, through
// RocketInfoFlightPinChecks.h.
//
// Every scenario holds:
// - the design (its file, relative to the repository, and its kind: example, example-resave,
//   testrocket, legacy, coverage, gui, special) and how the probe set it up: its overlay as
//   RocketPanel.updateExtras() leaves it for the design just opened with the estimate on and
//   AutoRunSimulations off ("estimated"), or set by the probe; the default units; the
//   simulation whose configuration the probe selected first;
// - the selected configuration (hasMotors(), getName() with motors named by their common
//   name) and every simulation (its getStatus(), its data's maxima);
// - which simulations updateSims(false) and updateSims(true) would run (by index);
// - the overlay's state (no flight data, FlightData.NaN_DATA or the shown simulation's data;
//   calculating; the shown simulation) and the shown simulation's status with its text
//   (removeHTMLTags(getStatusDescription()));
// - the texts of the flight block in the order RocketInfo draws them: font, theme colour, row
//   above the bottom one, column (the left margin, or the values' column 10 pixels right of the
//   widest label's visual width). Nothing of it depends on the font;
// - for the four designs whose estimate runs when they are opened (the simulation of the selected
//   configuration is not up to date: the re-saves of the test rockets beta, Falcon 9 Heavy, the
//   multi-stage event rocket and the cluster pods), the estimate's run as OpenRocket's worker does
//   it (schema 2): the document's saved state and undo history before (the design just opened,
//   saved) and after, the status of each simulation run, and the overlay afterwards with its texts.
// No value of a fresh flight: the texts of a run leave out the flight's apogee, maximum velocity
// and maximum acceleration (FlightPinText::fresh), and nothing else of it is a number. Before the
// run, the four designs show "Calculating..." and their labels, dimmed, with "N/A".

#include <array>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket::Gui::Test
{

/// The schema version of flight-blocks.json (gen_rocket_info_flight_pins.py's SCHEMA_VERSION).
inline constexpr int kRocketInfoFlightSchemaVersion = 2;

/// A simulation of a scenario's design.
struct FlightPinSimulation
{
    std::string name;
    /// getStatus() as updateSims() finds it: the constant's name ("LOADED", "ABORTED", ...).
    std::string status;
    /// hasSummaryData().
    bool hasData{false};
    /// Whether its configuration is the selected one.
    bool selectedConfiguration{false};
    /// Whether its configuration has motors.
    bool configurationHasMotors{false};
    /// The maxima of its data (altitude, velocity, Mach number, acceleration; NaN where the data
    /// has none), when it has data.
    std::optional<std::array<double, 4>> maxima;
};

/// What the overlay's flight block shows of the flight.
enum class FlightPinShown
{
    NONE,      ///< no flight data: no block
    NAN_DATA,  ///< FlightData.NaN_DATA: "N/A"
    DATA,      ///< the shown simulation's data
};

/// A text of the flight block.
struct FlightPinText
{
    /// The text; empty for a value of a fresh flight (fresh).
    std::string text;
    /// Whether it is a value of a fresh flight (the apogee, the maximum velocity or the maximum
    /// acceleration after the estimate's run), which the pins leave out: JSON's null.
    bool fresh{false};
    /// RocketInfo's font: "REGULAR" or "BOLD".
    std::string font;
    /// The theme colour: "FLIGHTDATA_TEXT_ACTIVE", "FLIGHTDATA_TEXT_INACTIVE", "ERROR" or "TEXT",
    /// with its components as Java drew it (FlatLaf Light).
    std::string        colour;
    std::array<int, 4> rgba{};
    /// The row: (y2 - y) / line.
    int rowFromBottom{0};
    /// Whether the text starts at the left margin (false: in the values' column).
    bool leftColumn{true};
};

/// What the overlay shows of the flight, and the texts of its block.
struct FlightPinOverlay
{
    FlightPinShown shown{FlightPinShown::NONE};
    bool           calculating{false};
    /// The overlay's simulation (RocketInfo's), by index, if any.
    std::optional<std::size_t> shownSimulation;
    /// The shown simulation's status (the constant's name) and its text, when there is one.
    std::optional<std::string> status;
    std::optional<std::string> statusText;
    std::vector<FlightPinText> texts;
};

/// isSaved(), isUndoAvailable() and isRedoAvailable() of the document.
struct FlightPinDocumentState
{
    bool saved{false};
    bool undoAvailable{false};
    bool redoAvailable{false};
};

/// A simulation the estimate ran: its getStatus() (the constant's name) and hasSummaryData() after
/// its run.
struct FlightPinRanSimulation
{
    std::size_t index{0};
    std::string status;
    bool        hasData{false};
};

/// The estimate's runs of a design just opened, as OpenRocket's BackgroundSimulationWorker runs
/// the simulations updateSims(false) finds (the probe's runEstimate()).
struct FlightPinRun
{
    /// The document marked saved, as OpenFileWorker marks a design it opened, before the runs, and
    /// after them.
    FlightPinDocumentState before;
    FlightPinDocumentState after;
    /// The simulations that ran to their end (simulationDone()), in order.
    std::vector<FlightPinRanSimulation> ran;
    /// Whether no run ended in simulationInterrupted() (which stops the chain); failed is that
    /// simulation's index and what was thrown.
    bool                       succeeded{true};
    std::optional<std::size_t> failed;
    std::string                failure;
    /// The overlay after the runs (its texts without the fresh flight's values).
    FlightPinOverlay overlay;
};

/// One scenario.
struct RocketInfoFlightPin
{
    /// The design's name in the suite ("example_a_simple_model_rocket") or the probe's own
    /// ("special:simple-calculating").
    std::string name;
    /// The name as a test name: letters, digits and '_'.
    std::string testName;
    std::string kind;
    /// The design file, relative to the repository ("data/examples/A simple model rocket.ork").
    std::string file;
    /// Whether the overlay is as updateExtras() leaves it for the design just opened (else the
    /// probe set it).
    bool estimated{true};
    /// Whether the default units are imperial (UnitGroup.setDefaultImperialUnits()).
    bool imperial{false};
    /// The simulation whose configuration the probe selected before, if any.
    std::optional<std::size_t>       selectConfigurationOf;
    bool                             hasMotors{false};
    std::string                      configurationName;
    std::vector<FlightPinSimulation> simulations;
    std::vector<std::size_t>         runsCurrent;
    std::vector<std::size_t>         runsAll;
    /// The overlay as updateExtras() left it (estimated), or as the probe set it.
    FlightPinOverlay overlay;
    /// The estimate's runs, for a design of the suite whose estimate runs (none otherwise: the
    /// probe's own states run nothing).
    std::optional<FlightPinRun> run;
};

/// What GoogleTest prints for the scenario of a test that failed: its name.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
inline void PrintTo(const RocketInfoFlightPin& pin, std::ostream* out)
{
    *out << pin.name;
}

/// The document.
struct RocketInfoFlightPins
{
    int                              schema{0};
    std::string                      openRocketCommit;
    std::vector<RocketInfoFlightPin> scenarios;
    /// What went wrong reading it (empty when nothing did).
    std::string problem;
};

/// flight-blocks.json, read on the first call.
[[nodiscard]] const RocketInfoFlightPins& rocketInfoFlightPins();

/// The scenario named @p name of rocketInfoFlightPins() (a design's name, or "special:..."), or
/// null.
[[nodiscard]] const RocketInfoFlightPin* findRocketInfoFlightPin(std::string_view name);

}  // namespace QtRocket::Gui::Test
