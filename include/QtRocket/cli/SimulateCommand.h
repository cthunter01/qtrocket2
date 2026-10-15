#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliEnvironment;
class CliOutput;
class FlightData;
class FlightDataBranch;
class FlightDataType;
class Simulation;
class SimulationOptions;
struct CliArguments;

/// QtRocket_cli simulate FILE [--sim NAME|NUMBER | --all] [--calm] [--seed N] [--time-step S]
/// [--csv OUT] [--db FILE]: runs simulations of a design as OpenRocket's run dialog does and
/// prints what its summary and its plots of events show (QtRocket's own command).
///
/// It loads FILE (the load warnings go to the standard error) and runs its first simulation, the
/// one --sim names (its exact name, the first that has it, or else its number from 1), or with
/// --all every simulation in order. With --csv the CSV file of each is checked before the first
/// simulation runs and anything is printed: its directory must exist and it must be neither a
/// directory nor the design file itself. For each simulation, before it runs (applyOptions()):
/// - --calm sets the standard deviation of the average wind model and of every level of the
///   multi-level wind model to 0: no turbulence, the average wind stays (as the golden harness's
///   makeReproducible() without its seed);
/// - --seed N sets the random seed to N (and fixes it); without it the seed is randomised unless
///   the design fixes one (SimulationOptions::randomizeSeedIfNotFixed(), as OpenRocket's
///   SimulationRunDialog does), and the seed used is printed, so that a run can be repeated with
///   --seed;
/// - --time-step S sets the time step (CliArguments has checked it against
///   SimulationInputRanges::kTimeStep).
/// It then simulates (Simulation::simulate()) and prints:
///
///     Simulation <number>: <name>
///       configuration: <name of the flight configuration>
///       random seed: <seed>
///       time step: <s> s
///       apogee: <m> m
///       max velocity: <m/s> m/s
///       max acceleration: <m/s^2> m/s^2
///       max Mach number: <M>
///       time to apogee: <s> s
///       flight time: <s> s
///       ground hit velocity: <m/s> m/s
///       launch rod velocity: <m/s> m/s
///       deployment velocity: <m/s> m/s
///       optimum delay: <s> s
///       events of branch '<branch name>':
///         <t> s: <TYPE>[ (<source component>)][: <warning or abort cause>]
///       warnings: none                    (or "warnings:" and a line "    <text>" each)
///       CSV: <file>                       (with --csv)
///
/// The summary values are FlightData's (getMaxAltitude() and the others), every value in SI
/// units in the shortest form that reads back as the same double (std::format("{}"); "nan" for
/// a value the flight did not have); the events are those of the first branch (the sustainer), in
/// the order the flight gave them; the warnings are the flight's. With --csv the first branch is
/// written through CsvExport with the export dialog's defaults, every column but 'Computation
/// time' (wall-clock time, so that two runs with the same seed give the same file), each in
/// its default unit; when several simulations run, simulation number N writes to OUT with "-N"
/// before its extension (numberedCsvPath()). See writeCsv() for a flight without data.
///
/// The exit code: ExitCode::LOAD when FILE does not load; ExitCode::USAGE when --sim names no
/// simulation, the design has none or a CSV file is the design itself; ExitCode::IO when the
/// directory of a CSV file does not exist or the file is a directory (before anything runs), or
/// when a CSV file cannot be written (the command stops there; CsvExport writes the file in
/// place, as OpenRocket does, so a file whose writing fails part way, on a full disk, is left as
/// far as it was written); else ExitCode::SIMULATION when a simulation failed (simulate()
/// returned an Error, which goes to the standard error; nothing more is printed of it), was
/// aborted (a SIM_ABORT event: its summary is printed and its CSV written, and the cause goes to
/// the standard error too) or has no flight data for its CSV; the other simulations run all the
/// same. ExitCode::OK otherwise.
///
/// Deviations from OpenRocket's run dialog and export: the simulation is run in this thread, and
/// a CSV line ends with '\n' on every platform (see CsvExport).
class SimulateCommand
{
public:
    SimulateCommand() = delete;

    /// Runs the command @p arguments (Command::SIMULATE) in @p environment, writing to
    /// @p output.
    [[nodiscard]] static ExitCode run(const CliArguments& arguments, CliEnvironment& environment,
                                      CliOutput& output);

    /// Applies --calm, --seed and --time-step of @p arguments to @p options, the options of a
    /// simulation that is about to run (see the class comment): with --calm the standard
    /// deviation of the average wind model and of every level of the multi-level wind model
    /// becomes 0, their average speeds stay; with --seed the seed is fixed to it; without
    /// --seed a seed that is not fixed is drawn anew (SimulationOptions::
    /// randomizeSeedIfNotFixed()) and a fixed one is kept; with --time-step the time step is set.
    static void applyOptions(const CliArguments& arguments, SimulationOptions& options);

    /// Writes the first branch of the flight @p data of @p simulation to the CSV file @p path
    /// (see the class comment) and prints "  CSV: <path>": ExitCode::OK. A flight without a
    /// branch, or whose first branch has no column to write (no types, which OpenRocket's
    /// export panel refuses as it refuses a flight without a branch, SimulationExportPanel.
    /// create(); or 'Computation time' alone), has nothing to write: a line on the standard
    /// error and ExitCode::SIMULATION, no file. A branch without a TIME column is written as
    /// CsvExport writes it (its events first). A file that cannot be written is a line on the
    /// standard error and ExitCode::IO.
    [[nodiscard]] static ExitCode writeCsv(const std::filesystem::path& path,
                                           Simulation& simulation, const FlightData& data,
                                           CliOutput& output);

    /// The CSV file of simulation number @p number when several simulations write one: @p csv
    /// with "-<number>" between its stem and its extension ("out.csv" -> "out-2.csv", "out" ->
    /// "out-2").
    [[nodiscard]] static std::filesystem::path numberedCsvPath(const std::filesystem::path& csv,
                                                               std::size_t                  number);

    /// The columns --csv writes of @p branch: its types (FlightDataBranch::getTypes(), in that
    /// order) without FlightDataTypeId::TYPE_COMPUTATION_TIME; an empty list for a branch
    /// without types.
    [[nodiscard]] static std::vector<const FlightDataType*> csvFields(
        const FlightDataBranch& branch);
};

}  // namespace QtRocket
