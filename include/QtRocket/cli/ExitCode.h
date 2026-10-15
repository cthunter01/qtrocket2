#pragma once

#include <cstdint>

namespace QtRocket
{

/// What the command-line tool QtRocket_cli returns to the shell (QtRocket's own; OpenRocket has
/// no command line). The plan's codes 0 to 4, and 5 for a defect of the program, so that a crash
/// is never taken for a usage error.
enum class ExitCode : std::uint8_t
{
    /// Everything asked for was done.
    OK = 0,
    /// The command line is wrong: no or an unknown command, an unknown option, an option without
    /// its value or with a value out of its range, a simulation or a flight configuration the
    /// design does not have, a design without simulations for 'simulate'.
    USAGE = 1,
    /// A design file or a motor file that was named cannot be read or loaded.
    LOAD = 2,
    /// A simulation failed (an error before or during the flight) or was aborted (a SIM_ABORT
    /// event).
    SIMULATION = 3,
    /// An output file (the CSV of --csv) cannot be written, or the motor database cannot be read.
    IO = 4,
    /// A defect of the program: a BugError or any other exception.
    INTERNAL = 5,
};

/// The number the process returns for @p code.
[[nodiscard]] constexpr int exitStatus(ExitCode code) noexcept
{
    return static_cast<int>(code);
}

}  // namespace QtRocket
