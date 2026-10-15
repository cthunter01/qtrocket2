#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliEnvironment;
class CliOutput;
class FlightDataBranch;
class FlightDataType;
struct CliArguments;

/// QtRocket_cli simulate FILE [--sim NAME|NUMBER | --all] [--calm] [--seed N] [--time-step S]
/// [--csv OUT] [--db FILE]: runs simulations of a design as OpenRocket's run dialog does and
/// prints what its summary and its plots of events show (QtRocket's own command).
///
/// It loads FILE (the load warnings go to the standard error) and runs its first simulation, the
/// one --sim names (its exact name, the first that has it, or else its number from 1), or with
/// --all every simulation in order. For each, before it runs:
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
/// before its extension (numberedCsvPath()).
///
/// The exit code: ExitCode::LOAD when FILE does not load; ExitCode::USAGE when --sim names no
/// simulation or the design has none; ExitCode::IO when a CSV file cannot be written (the command
/// stops there); else ExitCode::SIMULATION when a simulation failed (simulate() returned an
/// Error, which goes to the standard error; nothing more is printed of it) or was aborted (a
/// SIM_ABORT event: its summary is printed and its CSV written, and the cause goes to the
/// standard error too); the other simulations run all the same. ExitCode::OK otherwise.
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

    /// The CSV file of simulation number @p number when several simulations write one: @p csv
    /// with "-<number>" between its stem and its extension ("out.csv" -> "out-2.csv", "out" ->
    /// "out-2").
    [[nodiscard]] static std::filesystem::path numberedCsvPath(const std::filesystem::path& csv,
                                                               std::size_t                  number);

    /// The columns --csv writes of @p branch: its types (FlightDataBranch::getTypes(), in that
    /// order) without FlightDataTypeId::TYPE_COMPUTATION_TIME.
    /// @throws BugError when @p branch has no types
    [[nodiscard]] static std::vector<const FlightDataType*> csvFields(
        const FlightDataBranch& branch);
};

}  // namespace QtRocket
