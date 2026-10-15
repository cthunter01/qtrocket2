#include "QtRocket/file/CsvExport.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

using Columns = std::vector<const std::vector<double>*>;

/// Java's IllegalArgumentException for fields and units of different lengths, and its
/// NullPointerException for a null field or unit.
void checkFieldsAndUnits(std::span<const FlightDataType* const> fields,
                         std::span<const Unit* const>           units)
{
    if (fields.size() != units.size())
    {
        bug(std::format("fields and units lengths must be equal ({} vs {})", fields.size(),
                        units.size()));
    }
    QTROCKET_ASSERT(
        std::ranges::none_of(fields, [](const FlightDataType* f) { return f == nullptr; }));
    QTROCKET_ASSERT(std::ranges::none_of(units, [](const Unit* u) { return u == nullptr; }));
}

/// The column of each field, in order, or the failure of the first field the branch has no
/// column for.
[[nodiscard]] Result<Columns> columnsOf(const FlightDataBranch&                branch,
                                        std::span<const FlightDataType* const> fields)
{
    Columns columns;
    columns.reserve(fields.size());
    for (const FlightDataType* field : fields)
    {
        const std::vector<double>* column = branch.getView(*field);
        if (column == nullptr)
        {
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("The flight data branch '{}' has no data of type '{}'.",
                                    branch.getName(), field->getName()));
        }
        columns.push_back(column);
    }
    return columns;
}

// ------------------------------------------------------------------------- the event order

/// An event and what Java's FlightEvent.compareTo() reads of it: its time, whether it has a
/// source, the stage number of the source and the ordinal of its type.
struct EventOrder
{
    const FlightEvent* event{nullptr};
    double             time{0.0};
    bool               hasSource{false};
    int                stageNumber{0};
    int                typeOrdinal{0};
};

/// The source of @p event as Java's event has it: the component it points to, else the one its
/// source id names in @p rocket (a detached event, read from a file), else none.
[[nodiscard]] const RocketComponent* sourceOf(const FlightEvent& event, const Rocket& rocket)
{
    if (event.getSource() != nullptr)
    {
        return event.getSource();
    }
    const std::optional<Uuid>& sourceId = event.getSourceId();
    return sourceId.has_value() ? rocket.findComponent(*sourceId) : nullptr;
}

[[nodiscard]] EventOrder orderOf(const FlightEvent& event, const Rocket& rocket)
{
    const RocketComponent* source = sourceOf(event, rocket);
    return EventOrder{.event       = &event,
                      .time        = event.getTime(),
                      .hasSource   = source != nullptr,
                      .stageNumber = source != nullptr ? source->getStageNumber() : 0,
                      .typeOrdinal = ordinal(event.getType())};
}

/// Java's FlightEvent.compareTo() over what orderOf() read: negative when @p a comes first.
[[nodiscard]] int compareEvents(const EventOrder& a, const EventOrder& b) noexcept
{
    // first, sort on time
    if (a.time < b.time)
    {
        return -1;
    }
    if (a.time > b.time)
    {
        return 1;
    }

    // second, sort on stage presence.  Events with no source go first
    if (!a.hasSource && b.hasSource)
    {
        return -1;
    }
    if (a.hasSource && !b.hasSource)
    {
        return 1;
    }

    // third, sort on stage order.  Bigger stage number goes first
    if (a.hasSource && b.hasSource)
    {
        if (a.stageNumber > b.stageNumber)
        {
            return -1;
        }
        if (a.stageNumber < b.stageNumber)
        {
            return 1;
        }
    }

    // finally, sort on event type
    return a.typeOrdinal - b.typeOrdinal;
}

/// The events of @p branch in occurrence order (Java: Collections.sort() of a copy of the list,
/// a stable sort): pointers into the branch's list.
[[nodiscard]] std::vector<const FlightEvent*> sortedEvents(const FlightDataBranch& branch,
                                                           const Rocket&           rocket)
{
    const std::vector<FlightEvent>& events = branch.getEvents();
    std::vector<EventOrder>         orders;
    orders.reserve(events.size());
    for (const FlightEvent& event : events)
    {
        orders.push_back(orderOf(event, rocket));
    }
    std::ranges::stable_sort(
        orders, [](const EventOrder& a, const EventOrder& b) { return compareEvents(a, b) < 0; });
    std::vector<const FlightEvent*> sorted;
    sorted.reserve(orders.size());
    for (const EventOrder& order : orders)
    {
        sorted.push_back(order.event);
    }
    return sorted;
}

// ------------------------------------------------------------------------------ the lines

/// Appends commentStarter + " " + @p comment (Java: prependComment()) and the line's end.
void appendCommentLine(std::string& out, std::string_view commentStarter, std::string_view comment)
{
    out += commentStarter;
    out += ' ';
    out += comment;
    out += '\n';
}

/// How the events are written (Java: printEvent()'s arguments): the comment starter and the
/// simulation's data, whose warning set holds the warnings of SIM_WARN events.
struct EventFormat
{
    std::string_view  commentStarter;
    const FlightData* data{nullptr};
};

/// The warning of the SIM_WARN event @p event as text: the warning as the simulation's set has
/// it now, else the event's own copy (see the class comment).
[[nodiscard]] std::string warningText(const FlightEvent& event, const FlightData* data)
{
    if (const Warning* current = data != nullptr ? data->findWarning(event) : nullptr;
        current != nullptr)
    {
        return Strings::toValidUtf8(current->toString());
    }
    if (const std::shared_ptr<const Warning> own = event.getWarning(); own != nullptr)
    {
        return Strings::toValidUtf8(own->toString());
    }
    // Java's string concatenation of a null warning; validate() lets no SIM_WARN event without
    // a warning be made.
    return "null";
}

/// Appends the line of @p event (Java: printEvent()).
void appendEvent(std::string& out, const FlightEvent& event, const EventFormat& format)
{
    std::string text = std::format("Event {} occurred at t={} seconds", name(event.getType()),
                                   Strings::doubleToString(event.getTime()));
    if (event.getType() == FlightEvent::Type::SIM_WARN)
    {
        text += ": ";
        text += warningText(event, format.data);
    }
    appendCommentLine(out, format.commentStarter, text);
}

/// The suffix of the simulation's line for @p status.
[[nodiscard]] std::string_view statusSuffix(Simulation::Status status)
{
    switch (status)
    {
        case Simulation::Status::UPTODATE:
            return " (Up to date)";
        case Simulation::Status::LOADED:
            return " (Data loaded from a file)";
        case Simulation::Status::OUTDATED:
            return " (Out of date)";
        case Simulation::Status::EXTERNAL:
            return " (Imported data)";
        case Simulation::Status::NOT_SIMULATED:
            return " (Not simulated yet)";
        case Simulation::Status::CANT_RUN:
        case Simulation::Status::ABORTED:
            return "";
    }
    QTROCKET_UNREACHABLE();
}

/// Appends the simulation comments (Java: writeSimulationComments()).
void appendSimulationComments(std::string& out, Simulation& simulation,
                              const FlightDataBranch& branch, std::size_t fieldCount,
                              std::string_view commentStarter)
{
    std::string line = Strings::toValidUtf8(simulation.getName());

    const FlightData* data = simulation.getSimulatedData().get();

    line += statusSuffix(simulation.getStatus());

    appendCommentLine(out, commentStarter, line);

    appendCommentLine(
        out, commentStarter,
        std::format("{} data points written for {} variables.", branch.getLength(), fieldCount));

    if (data == nullptr)
    {
        appendCommentLine(out, commentStarter, "No simulation data available.");
        return;
    }
    const WarningSet& warnings = data->getWarningSet();

    if (!warnings.empty())
    {
        appendCommentLine(out, commentStarter, "Simulation warnings:");
        for (const Warning& warning : warnings)
        {
            appendCommentLine(out, commentStarter, "  " + Strings::toValidUtf8(warning.toString()));
        }
    }
}

/// Appends the line with the field names and units.
void appendFieldComments(std::string& out, std::span<const FlightDataType* const> fields,
                         std::span<const Unit* const> units, std::string_view fieldSeparator,
                         std::string_view commentStarter)
{
    out += commentStarter;
    out += ' ';
    for (std::size_t i = 0; i < fields.size(); i++)
    {
        out += Strings::toValidUtf8(fields[i]->getName());
        out += " (";
        out += Strings::toValidUtf8(units[i]->getUnit());
        out += ')';
        if (i < fields.size() - 1)
        {
            out += fieldSeparator;
        }
    }
    out += '\n';
}

/// How the rows are written (Java: writeData()'s arguments that concern the values).
struct RowFormat
{
    std::span<const std::vector<double>* const> columns;
    std::span<const Unit* const>                units;
    std::string_view                            fieldSeparator;
    int                                         decimalPlaces{0};
    bool                                        exponentialNotation{false};
};

/// Appends the values of row @p pos and the line's end.
void appendRow(std::string& out, const RowFormat& format, std::size_t pos)
{
    for (std::size_t i = 0; i < format.columns.size(); i++)
    {
        const double value = (*format.columns[i])[pos];
        out += Strings::doubleToString(format.units[i]->toUnit(value), format.decimalPlaces,
                                       format.exponentialNotation);

        if (i < format.columns.size() - 1)
        {
            out += format.fieldSeparator;
        }
    }
    out += '\n';
}

// ----------------------------------------------------------------------------- the stream

/// Writes @p text to @p stream; false when the stream fails.
[[nodiscard]] bool writeText(std::ostream& stream, std::string_view text)
{
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return !stream.fail();
}

/// Writes the rows of @p branch and, with @p events (null: no event comments), its events
/// (Java: writeData()); false when the stream fails. Each row is written as soon as it is made.
[[nodiscard]] bool writeData(std::ostream& stream, const FlightDataBranch& branch,
                             const Rocket& rocket, const RowFormat& rows, const EventFormat* events)
{
    // Time variable
    const std::vector<double>* time =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));

    // Number of data points
    const std::size_t n = time != nullptr ? time->size() : branch.getLength();

    // Flight events in occurrence order
    const std::vector<const FlightEvent*> sorted =
        events != nullptr ? sortedEvents(branch, rocket) : std::vector<const FlightEvent*>{};
    std::size_t eventPosition = 0;

    std::string out;

    // If time information is not available, print events at beginning of file
    if (events != nullptr && time == nullptr)
    {
        for (const FlightEvent* event : sorted)
        {
            appendEvent(out, *event, *events);
        }
        eventPosition = sorted.size();
    }

    // Loop over all data points
    for (std::size_t pos = 0; pos < n; pos++)
    {
        // Check for events to store
        if (events != nullptr && time != nullptr)
        {
            const double t = (*time)[pos];

            while (eventPosition < sorted.size() && sorted[eventPosition]->getTime() <= t)
            {
                appendEvent(out, *sorted[eventPosition], *events);
                eventPosition++;
            }
        }

        // Store CSV line
        appendRow(out, rows, pos);
        if (!writeText(stream, out))
        {
            return false;
        }
        out.clear();
    }

    // Store any remaining events
    if (events != nullptr && time != nullptr)
    {
        while (eventPosition < sorted.size())
        {
            appendEvent(out, *sorted[eventPosition], *events);
            eventPosition++;
        }
    }
    return writeText(stream, out);
}

/// The failure of a stream that could not be written.
[[nodiscard]] std::unexpected<Error> streamFailure(
    std::source_location where = std::source_location::current())
{
    return fail(ErrorCode::IO, "error while writing the CSV data", where);
}

}  // namespace

Result<void> CsvExport::exportCsv(std::ostream& stream, Simulation& simulation,
                                  const FlightDataBranch&                branch,
                                  std::span<const FlightDataType* const> fields,
                                  std::span<const Unit* const> units, const Options& options)
{
    checkFieldsAndUnits(fields, units);
    Result<Columns> columns = columnsOf(branch, fields);
    if (!columns)
    {
        return std::unexpected(std::move(columns.error()));
    }

    const std::string fieldSeparator = Strings::toValidUtf8(options.fieldSeparator);
    const std::string commentStarter = Strings::toValidUtf8(options.commentStarter);

    try
    {
        std::string header;

        // Write the initial comments
        if (options.simulationComments)
        {
            appendSimulationComments(header, simulation, branch, fields.size(), commentStarter);
        }

        if (options.simulationComments && options.fieldComments)
        {
            header += commentStarter;
            header += '\n';
        }

        if (options.fieldComments)
        {
            appendFieldComments(header, fields, units, fieldSeparator, commentStarter);
        }
        if (!writeText(stream, header))
        {
            return streamFailure();
        }

        const RowFormat   rows{.columns             = *columns,
                               .units               = units,
                               .fieldSeparator      = fieldSeparator,
                               .decimalPlaces       = options.decimalPlaces,
                               .exponentialNotation = options.exponentialNotation};
        const EventFormat events{.commentStarter = commentStarter,
                                 .data           = simulation.getSimulatedData().get()};
        if (!writeData(stream, branch, simulation.getRocket(), rows,
                       options.eventComments ? &events : nullptr))
        {
            return streamFailure();
        }
        if (stream.flush().fail())
        {
            return streamFailure();
        }
    }
    catch (const std::ios_base::failure&)
    {
        // A stream whose caller asked for exceptions (std::ios::exceptions()).
        return streamFailure();
    }
    return {};
}

Result<void> CsvExport::exportCsv(const std::filesystem::path& path, Simulation& simulation,
                                  const FlightDataBranch&                branch,
                                  std::span<const FlightDataType* const> fields,
                                  std::span<const Unit* const> units, const Options& options)
{
    checkFieldsAndUnits(fields, units);
    if (Result<Columns> columns = columnsOf(branch, fields); !columns)
    {
        return std::unexpected(std::move(columns.error()));
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return fail(ErrorCode::IO, std::format("cannot open '{}' for writing", pathToUtf8(path)));
    }
    if (Result<void> written = exportCsv(file, simulation, branch, fields, units, options);
        !written)
    {
        if (written.error().code == ErrorCode::IO)
        {
            return fail(ErrorCode::IO, std::format("error while writing '{}'", pathToUtf8(path)));
        }
        return written;
    }
    file.close();
    if (file.fail())
    {
        return fail(ErrorCode::IO, std::format("error while writing '{}'", pathToUtf8(path)));
    }
    return {};
}

std::vector<const Unit*> CsvExport::defaultUnits(std::span<const FlightDataType* const> fields)
{
    std::vector<const Unit*> units;
    units.reserve(fields.size());
    for (const FlightDataType* field : fields)
    {
        QTROCKET_ASSERT(field != nullptr);
        units.push_back(&field->getUnitGroup().getDefaultUnit());
    }
    return units;
}

}  // namespace QtRocket
