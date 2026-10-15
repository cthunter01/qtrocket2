#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliOutput;
class ThrustCurveMotorSet;
class ThrustCurveMotorSetDatabase;

/// QtRocket_cli motors [--search TEXT] [--db FILE] and QtRocket_cli motors --load FILE: lists
/// motors as a table (QtRocket's own command).
///
/// The table has a header line and a line per motor, the fields separated by a tab:
///
///     Manufacturer  Designation  Common name  Type  Diameter (m)  Length (m)
///     Total impulse (Ns)  Average thrust (N)  Burn time (s)  Delays (s)  Digest
///
/// the type as Motor's name() ("Single-use"), the numbers the estimates of ThrustCurveMotor in
/// the shortest form that reads back as the same double (std::format("{}")), the delays the
/// standard delays as ThrustCurveMotor::getDelayString() writes them ("P" for plugged),
/// separated by commas. A tab or a line break inside a text becomes a space. The motors come
/// motor set by motor set in the order of the database, each set's motors in the set's order.
///
/// --search keeps the motor sets that match every word of TEXT (matches()), as the search field
/// of OpenRocket's motor chooser filters its table (swing MotorRowFilter.filterByString()); a
/// documented subset of it, see matches().
///
/// --load lists the motors of a motor file instead of the database: a RASP (.eng), RockSim
/// (.rse) or zip file, or a motor database (.db), read as OpenRocket reads the user's motor files
/// (MotorDatabaseLoader::loadUserDefinedMotors(), so a directory is searched for such files).
class MotorsCommand
{
public:
    MotorsCommand() = delete;

    /// Lists the motors of @p database, the sets that match @p search only when it is given.
    /// ExitCode::OK.
    [[nodiscard]] static ExitCode list(const ThrustCurveMotorSetDatabase& database,
                                       const std::optional<std::string>& search, CliOutput& output);

    /// Lists the motors of the motor file @p file (UTF-8). What cannot be read goes to the
    /// standard error ("cannot load '<file>': <reason>", a line each) and makes the exit code
    /// ExitCode::LOAD; the motors that were read are listed all the same.
    [[nodiscard]] static ExitCode load(std::string_view file, CliOutput& output);

    /// The words of @p text (MotorRowFilter.setSearchTerms() of text.trim().split("\\s+")): split
    /// at runs of Java regex whitespace (space, tab, line feed, vertical tab, form feed,
    /// carriage return), empty words left out, each case-folded (Strings::javaCaseFold()).
    [[nodiscard]] static std::vector<std::string> searchTerms(std::string_view text);

    /// Whether every one of @p terms (from searchTerms()) is contained in one of the texts of
    /// @p set, each case-folded (Strings::javaCaseFold()): the manufacturer's display name, the
    /// common name, the designation and the case info. MotorRowFilter.filterByString() also looks
    /// at the total impulse, the diameter and the length as the motor chooser's table shows them
    /// (numbers with the user's units), and folds the case with the default locale's rules: this
    /// subset leaves the numbers out.
    [[nodiscard]] static bool matches(const ThrustCurveMotorSet&   set,
                                      std::span<const std::string> terms);
};

}  // namespace QtRocket
