#pragma once

#include <filesystem>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

class FlightDataBranch;
class FlightDataType;
class Simulation;
class Unit;

/// Writes one branch of flight data as comma-separated values (OpenRocket's file/CSVExport, the
/// overload for flight data). The output is what OpenRocket writes, byte for byte, for the same
/// branch and options (tests/data/csv holds OpenRocket's own files of the examples' stored data;
/// tools/openrocket-goldens/README.md says how they are made):
/// - with the simulation comments, a comment line with the simulation's name and a suffix for
///   its status (getStatus(): " (Up to date)", " (Data loaded from a file)", " (Out of date)",
///   " (Imported data)", " (Not simulated yet)", none for CANT_RUN and ABORTED), then "<rows>
///   data points written for <fields> variables.", then either "No simulation data
///   available." (a simulation without simulated data; nothing more of these comments) or,
///   when the simulation's warning set is not empty, "Simulation warnings:" and a line
///   "  <warning>" (Message::toString()) for each warning, in the set's order;
/// - with both the simulation and the field comments, a line holding the comment starter alone;
/// - with the field comments, a comment line with "<name> (<unit>)" for each field, separated
///   by the field separator (a dimensionless field's unit is Unit::noUnit()'s zero-width space,
///   U+200B);
/// - a line per row of the branch with the value of each field in its unit (Unit::toUnit()),
///   written by Strings::doubleToString() with the decimal places and the exponential notation
///   of the options, separated by the field separator;
/// - with the event comments, a comment line "Event <TYPE> occurred at t=<time> seconds" for
///   every event of the branch (the type as FlightEvent's name(), the time through
///   Strings::doubleToString()), a SIM_WARN event followed by ": <warning>": before the first
///   row whose time is at or after the event's, the events after the last row at the end, and
///   every event before the first row when the branch has no TIME column. The events are taken
///   in the order of Java's FlightEvent.compareTo() (a stable sort): by time, then those without
///   a source, then by stage number, the higher first, then by the type's ordinal.
/// A comment line is the comment starter, a space and the text. Every line ends with '\n'.
///
/// Rows are written as they are made, one at a time, so that a branch of a million rows needs
/// no copy of the text in memory.
///
/// The text of the warnings, which may hold values (an angle of attack), is formatted with the
/// process's default units, as OpenRocket formats it with the user's.
///
/// Deviations from OpenRocket:
/// - Every line ends with '\n' on every platform (Java's println() ends a line with the
///   platform's separator, "\r\n" on Windows), so that a file is the same everywhere. Write to a
///   binary stream: a std::ofstream opened in text mode turns '\n' into "\r\n" on Windows. The
///   overload that takes a path opens its file in binary mode.
/// - Events are ordered by a comparator of this class that resolves the source of a detached
///   event (an event read from a file, which knows its source by id only, see FlightEvent) in
///   the simulation's rocket (Simulation::getRocket(), RocketComponent::findComponent()), so
///   that the event has the stage number its source has there, as Java's event, which points to
///   that component, has. FlightEvent::compareTo() sees only the pointer and would sort such an
///   event as one without a source (58 of the 73 branches stored in the examples would come out
///   in another order). A source id the rocket no longer holds (the component was removed after
///   the load) counts as no source; Java asks the removed component for its stage there, which
///   fails for most components.
/// - The warning of a SIM_WARN event is the one the simulation's warning set holds now
///   (FlightData::findWarning()), as Java's event holds the very object the set holds; for an
///   event whose warning the set does not have (a branch of another simulation), the event's own
///   copy.
/// - A field the branch has no column for is ErrorCode::INVALID_ARGUMENT, found before anything
///   is written. Java writes the comments and then fails with a NullPointerException at the
///   first row (and does not fail for a branch without rows).
/// - A stream that fails is ErrorCode::IO, and the export stops at the first row it could not
///   write (Java's PrintWriter swallows the IOException and only remembers it, checkError()).
/// - The caller's stream is flushed and left open (Java's writer.close() closes the
///   OutputStream it was given); the overload that takes a path closes its file.
/// - Every text (the name, the warnings, the field names and units, the separator and the
///   comment starter) is written as valid UTF-8: a malformed sequence becomes U+FFFD
///   (Strings::toValidUtf8()), as Java's UTF-8 decoder made the String in the first place.
/// - fields and units of different lengths, and a null field or unit, are a BugError (Java: the
///   IllegalArgumentException with the same message, and a NullPointerException).
/// - Not ported: the overload for component analysis data (Milestone 7, see the end of the
///   class).
class CsvExport
{
public:
    /// The options of OpenRocket's export dialog (SimulationExportPanel and CsvOptionPanel),
    /// with the dialog's defaults.
    struct Options
    {
        /// Written between two fields (the dialog offers ",", ";", a space and a tab).
        std::string fieldSeparator{","};
        /// The decimals of each value (the dialog allows 0 to 15); a negative number counts as 0.
        int decimalPlaces{Strings::kDefaultDecimalPlaces};
        /// Whether a value below 0.001 or from 10000 up is written as mantissa + "e" +
        /// exponent ("1.234e-5").
        bool exponentialNotation{true};
        /// What starts a comment line (the dialog offers "#", "%" and ";").
        std::string commentStarter{"#"};
        /// Whether to write the simulation comments (name, status, size, warnings).
        bool simulationComments{true};
        /// Whether to write the line with the field names and units.
        bool fieldComments{true};
        /// Whether to write the flight events.
        bool eventComments{true};
    };

    /// Writes @p branch to @p stream: for each row, the value of each of @p fields in the unit
    /// at the same position of @p units, in that order, as the class comment says. The branch
    /// is one of the data of @p simulation, whose name, status and warnings are written and
    /// whose rocket names the sources of the events; the status is worked out by getStatus(),
    /// which is why the simulation is not const (as in Java). Fails with
    /// ErrorCode::INVALID_ARGUMENT, before anything is written, when @p branch has no column for
    /// a field, and with ErrorCode::IO when the stream fails (it is in a failed state then, and
    /// holds what was written before), also when the stream throws on failure
    /// (std::ios::exceptions()): the exception does not leave the call.
    /// @throws BugError when @p fields and @p units differ in length or hold a null pointer
    [[nodiscard]] static Result<void> exportCsv(std::ostream& stream, Simulation& simulation,
                                                const FlightDataBranch&                branch,
                                                std::span<const FlightDataType* const> fields,
                                                std::span<const Unit* const>           units,
                                                const Options&                         options);

    /// The same into the file @p path, opened in binary mode and replaced. A field the branch
    /// does not have fails before the file is opened. Fails with ErrorCode::IO when the file
    /// cannot be opened or written; a file that could be opened then holds what was written
    /// before the failure.
    /// @throws BugError as the stream overload
    [[nodiscard]] static Result<void> exportCsv(const std::filesystem::path&           path,
                                                Simulation&                            simulation,
                                                const FlightDataBranch&                branch,
                                                std::span<const FlightDataType* const> fields,
                                                std::span<const Unit* const>           units,
                                                const Options&                         options);

    /// The default unit of each of @p fields, in order (the unit OpenRocket's export dialog
    /// starts with: the default unit of the field's unit group). The units belong to the unit
    /// groups, which live as long as the process; a change of a default unit afterwards does not
    /// change the list.
    /// @throws BugError when @p fields holds a null pointer
    [[nodiscard]] static std::vector<const Unit*> defaultUnits(
        std::span<const FlightDataType* const> fields);

    // HOOK(component-analysis): Java's exportCSV(OutputStream, CAParameters, CADataBranch,
    // CADomainDataType, CADataType[], Map<CADataType, List<RocketComponent>>, Unit[], ...), the
    // export of component analysis data, comes with the component analysis (Milestone 7).
};

}  // namespace QtRocket
