#include "plot/SimulationPlotGoldens.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Sha256.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using Json = nlohmann::json;

/// A member of a table that is missing or of the wrong type: thrown by the readers below and
/// turned into the Error of the parse, so that the readers read straight on.
class PlotGoldenFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// The member @p key of the object @p object.
[[nodiscard]] const Json& member(const Json& object, std::string_view key)
{
    if (!object.is_object() || !object.contains(key))
    {
        throw PlotGoldenFormatError(std::format(R"(missing "{}")", key));
    }
    return object.at(key);
}

[[nodiscard]] double numberOf(const Json& value, std::string_view what)
{
    const std::optional<double> number = goldenNumber(value);
    if (!number.has_value())
    {
        throw PlotGoldenFormatError(std::format("{} is not a number", what));
    }
    return *number;
}

[[nodiscard]] double number(const Json& object, std::string_view key)
{
    return numberOf(member(object, key), key);
}

[[nodiscard]] std::string stringOf(const Json& value, std::string_view what)
{
    if (!value.is_string())
    {
        throw PlotGoldenFormatError(std::format("{} is not a string", what));
    }
    return value.get<std::string>();
}

[[nodiscard]] std::string string(const Json& object, std::string_view key)
{
    return stringOf(member(object, key), key);
}

[[nodiscard]] long long integerOf(const Json& value, std::string_view what)
{
    if (!value.is_number_integer())
    {
        throw PlotGoldenFormatError(std::format("{} is not an integer", what));
    }
    return value.get<long long>();
}

[[nodiscard]] int integer(const Json& object, std::string_view key)
{
    return static_cast<int>(integerOf(member(object, key), key));
}

[[nodiscard]] std::size_t count(const Json& object, std::string_view key)
{
    const long long value = integerOf(member(object, key), key);
    if (value < 0)
    {
        throw PlotGoldenFormatError(std::format("{} is negative", key));
    }
    return static_cast<std::size_t>(value);
}

/// Refuses @p value unless it is an array.
void requireArray(const Json& value, std::string_view what)
{
    if (!value.is_array())
    {
        throw PlotGoldenFormatError(std::format("{} is not an array", what));
    }
}

[[nodiscard]] const Json& array(const Json& object, std::string_view key)
{
    const Json& value = member(object, key);
    requireArray(value, key);
    return value;
}

[[nodiscard]] std::vector<std::string> strings(const Json& object, std::string_view key)
{
    std::vector<std::string> values;
    for (const Json& value : array(object, key))
    {
        values.push_back(stringOf(value, key));
    }
    return values;
}

[[nodiscard]] std::vector<int> integers(const Json& value, std::string_view what)
{
    std::vector<int> values;
    requireArray(value, what);
    for (const Json& element : value)
    {
        values.push_back(static_cast<int>(integerOf(element, what)));
    }
    return values;
}

/// [a, b] of two numbers.
[[nodiscard]] GoldenPlotPair pairOf(const Json& value, std::string_view what)
{
    requireArray(value, what);
    const Json& elements = value;
    if (elements.size() != 2)
    {
        throw PlotGoldenFormatError(std::format("{} is not a pair", what));
    }
    return {numberOf(elements.at(0), what), numberOf(elements.at(1), what)};
}

[[nodiscard]] GoldenPlotPair pair(const Json& object, std::string_view key)
{
    return pairOf(member(object, key), key);
}

[[nodiscard]] std::array<std::string, 2> stringPair(const Json& object, std::string_view key)
{
    const Json& elements = array(object, key);
    if (elements.size() != 2)
    {
        throw PlotGoldenFormatError(std::format("{} is not a pair", key));
    }
    return {stringOf(elements.at(0), key), stringOf(elements.at(1), key)};
}

/// {"axes": [...], "ranges": [[left], [right]]}.
[[nodiscard]] GoldenPlotAxes axesOf(const Json& value)
{
    GoldenPlotAxes axes;
    axes.axes          = integers(member(value, "axes"), "axes");
    const Json& ranges = array(value, "ranges");
    if (ranges.size() != 2)
    {
        throw PlotGoldenFormatError("ranges is not two ranges");
    }
    axes.ranges = {pairOf(ranges.at(0), "ranges"), pairOf(ranges.at(1), "ranges")};
    return axes;
}

[[nodiscard]] std::vector<GoldenPlotCandidate> candidatesOf(const Json& value)
{
    std::vector<GoldenPlotCandidate> candidates;
    requireArray(value, "candidates");
    for (const Json& candidate : value)
    {
        candidates.push_back(GoldenPlotCandidate{.fit      = axesOf(candidate),
                                                 .goodness = number(candidate, "goodness")});
    }
    return candidates;
}

/// [type, unit, axis].
[[nodiscard]] std::vector<GoldenPlotRow> rowsOf(const Json& value)
{
    std::vector<GoldenPlotRow> rows;
    requireArray(value, "rows");
    for (const Json& row : value)
    {
        if (!row.is_array() || row.size() != 3)
        {
            throw PlotGoldenFormatError("a row is not [type, unit, axis]");
        }
        rows.push_back(
            GoldenPlotRow{.type = stringOf(row.at(0), "a row's type"),
                          .unit = stringOf(row.at(1), "a row's unit"),
                          .axis = static_cast<int>(integerOf(row.at(2), "a row's axis"))});
    }
    return rows;
}

/// [type, time, branch].
[[nodiscard]] std::vector<GoldenPlotEvent> eventsOf(const Json& value, std::string_view what)
{
    std::vector<GoldenPlotEvent> events;
    requireArray(value, what);
    for (const Json& event : value)
    {
        if (!event.is_array() || event.size() != 3)
        {
            throw PlotGoldenFormatError(
                std::format("an event of {} is not [type, time, branch]", what));
        }
        const long long branch = integerOf(event.at(2), "an event's branch");
        if (branch < 0)
        {
            throw PlotGoldenFormatError("an event's branch is negative");
        }
        events.push_back(GoldenPlotEvent{.type   = stringOf(event.at(0), "an event's type"),
                                         .time   = numberOf(event.at(1), "an event's time"),
                                         .branch = static_cast<std::size_t>(branch)});
    }
    return events;
}

[[nodiscard]] std::vector<GoldenPlotGroup> groupsOf(const Json& value)
{
    std::vector<GoldenPlotGroup> groups;
    requireArray(value, "groups");
    for (const Json& group : value)
    {
        groups.push_back(GoldenPlotGroup{.time   = number(group, "time"),
                                         .look   = string(group, "look"),
                                         .events = eventsOf(member(group, "events"), "events"),
                                         .label  = string(group, "label")});
    }
    return groups;
}

[[nodiscard]] GoldenPlotYAxis yAxisOf(const Json& value)
{
    GoldenPlotYAxis axis;
    axis.series       = count(value, "series");
    const Json& label = member(value, "label");
    if (!label.is_null())
    {
        axis.label = stringOf(label, "label");
    }
    axis.range = pair(value, "range");
    return axis;
}

[[nodiscard]] GoldenPlotSeries seriesOf(const Json& value)
{
    return GoldenPlotSeries{.name   = string(value, "name"),
                            .axis   = integer(value, "axis"),
                            .branch = count(value, "branch"),
                            .row    = count(value, "row"),
                            .unit   = string(value, "unit"),
                            .points = count(value, "points"),
                            .digest = string(value, "digest")};
}

[[nodiscard]] GoldenPlotOpenRocket openRocketOf(const Json& value)
{
    return GoldenPlotOpenRocket{.bug        = integer(value, "bug"),
                                .automatic  = axesOf(member(value, "auto")),
                                .filled     = axesOf(member(value, "filled")),
                                .candidates = candidatesOf(member(value, "candidates"))};
}

/// The members of a plot up to the titles.
void readPlotConfiguration(const Json& value, GoldenPlot& plot)
{
    plot.preset     = count(value, "preset");
    plot.name       = string(value, "name");
    plot.domain     = stringPair(value, "domain");
    plot.rows       = rowsOf(member(value, "rows"));
    plot.automatic  = axesOf(member(value, "auto"));
    plot.candidates = candidatesOf(member(value, "candidates"));
    plot.filled     = axesOf(member(value, "filled"));
    if (value.contains("openRocket"))
    {
        plot.openRocket = openRocketOf(member(value, "openRocket"));
    }
}

/// The members of a plot from the titles on.
void readPlotChart(const Json& value, GoldenPlot& plot)
{
    plot.title       = string(value, "title");
    plot.subtitle    = string(value, "subtitle");
    plot.domainLabel = string(value, "domainLabel");
    plot.domainRange = pair(value, "domainRange");
    if (const Json& widened = member(value, "domainRangeWithEventLines"); !widened.is_null())
    {
        plot.domainRangeWithEventLines = pairOf(widened, "domainRangeWithEventLines");
    }
    const Json& yAxes = array(value, "yAxes");
    if (yAxes.size() != 2)
    {
        throw PlotGoldenFormatError("yAxes is not two axes");
    }
    plot.yAxes       = {yAxisOf(yAxes.at(0)), yAxisOf(yAxes.at(1))};
    plot.panelColors = integers(member(value, "panelColors"), "panelColors");
    for (const Json& series : array(value, "series"))
    {
        plot.series.push_back(seriesOf(series));
    }
    plot.legend       = strings(value, "legend");
    plot.groups       = groupsOf(member(value, "groups"));
    const Json& icons = member(value, "icons");
    plot.icons        = GoldenPlotIcons{.count         = count(icons, "count"),
                                        .digest        = string(icons, "digest"),
                                        .toolTipDigest = string(icons, "toolTipDigest")};
}

[[nodiscard]] GoldenPlot plotOf(const Json& value)
{
    GoldenPlot plot;
    readPlotConfiguration(value, plot);
    readPlotChart(value, plot);
    return plot;
}

[[nodiscard]] GoldenPlotBranch branchOf(const Json& value)
{
    return GoldenPlotBranch{.name            = string(value, "name"),
                            .points          = count(value, "points"),
                            .separationTime  = number(value, "separationTime"),
                            .separationIndex = integer(value, "separationIndex")};
}

[[nodiscard]] GoldenPlotPreset presetOf(const Json& value)
{
    return GoldenPlotPreset{.name   = string(value, "name"),
                            .domain = stringPair(value, "domain"),
                            .rows   = rowsOf(member(value, "rows")),
                            .events = strings(value, "events")};
}

/// The head of a table, which must be of @p schema.
[[nodiscard]] GoldenPlotProvenance provenanceOf(const Json& document, std::string_view schema)
{
    if (string(document, "schema") != schema)
    {
        throw PlotGoldenFormatError(std::format(R"(the schema is not "{}")", schema));
    }
    const Json&          openrocket = member(document, "openrocket");
    GoldenPlotProvenance provenance{.schemaVersion     = integer(document, "schemaVersion"),
                                    .openrocketCommit  = string(openrocket, "commit"),
                                    .openrocketVersion = string(openrocket, "version"),
                                    .dirty             = false};
    const Json&          dirty = member(openrocket, "dirty");
    if (!dirty.is_boolean())
    {
        throw PlotGoldenFormatError("dirty is not a boolean");
    }
    provenance.dirty = dirty.get<bool>();
    return provenance;
}

[[nodiscard]] GoldenPlotSimulation simulationOf(const Json& value)
{
    GoldenPlotSimulation simulation{.file         = string(value, "file"),
                                    .simulation   = count(value, "simulation"),
                                    .name         = string(value, "name"),
                                    .branches     = {},
                                    .seriesLabels = strings(value, "seriesLabels"),
                                    .plots        = {}};
    for (const Json& branch : array(value, "branches"))
    {
        simulation.branches.push_back(branchOf(branch));
    }
    for (const Json& plot : array(value, "plots"))
    {
        simulation.plots.push_back(plotOf(plot));
    }
    return simulation;
}

[[nodiscard]] GoldenPlotSeriesPoints seriesPointsOf(const Json& value)
{
    return GoldenPlotSeriesPoints{.min    = pair(value, "min"),
                                  .max    = pair(value, "max"),
                                  .first  = pair(value, "first"),
                                  .middle = pair(value, "middle"),
                                  .last   = pair(value, "last")};
}

[[nodiscard]] GoldenPlotIcon iconOf(const Json& value)
{
    return GoldenPlotIcon{.series      = count(value, "series"),
                          .group       = count(value, "group"),
                          .time        = number(value, "time"),
                          .x           = number(value, "x"),
                          .y           = number(value, "y"),
                          .yOnLeftAxis = number(value, "yOnLeftAxis"),
                          .look        = string(value, "look"),
                          .toolTip     = string(value, "toolTip")};
}

[[nodiscard]] GoldenPlotDetail detailOf(const Json& value)
{
    GoldenPlotDetail detail;
    detail.preset = count(value, "preset");
    for (const Json& series : array(value, "series"))
    {
        detail.series.push_back(seriesPointsOf(series));
    }
    for (const Json& toolTip : array(value, "toolTips"))
    {
        detail.toolTips.push_back(GoldenPlotToolTip{.series = count(toolTip, "series"),
                                                    .point  = count(toolTip, "point"),
                                                    .name   = string(toolTip, "name"),
                                                    .html   = string(toolTip, "html")});
    }
    detail.eventList = eventsOf(member(value, "eventList"), "eventList");
    for (const Json& groups : array(value, "branchGroups"))
    {
        detail.branchGroups.push_back(groupsOf(groups));
    }
    for (const Json& icon : array(value, "icons"))
    {
        detail.icons.push_back(iconOf(icon));
    }
    return detail;
}

[[nodiscard]] GoldenPlotDetailSimulation detailSimulationOf(const Json& value)
{
    GoldenPlotDetailSimulation simulation{.file       = string(value, "file"),
                                          .simulation = count(value, "simulation"),
                                          .name       = string(value, "name"),
                                          .plots      = {}};
    for (const Json& plot : array(value, "plots"))
    {
        simulation.plots.push_back(detailOf(plot));
    }
    return simulation;
}

/// @p read applied to @p document, or the Error of what it found wrong, located in @p file.
template <class T, class Read>
[[nodiscard]] Result<T> parseTable(const Json& document, std::string_view file, Read read)
{
    try
    {
        return read(document);
    }
    catch (const PlotGoldenFormatError& error)
    {
        return fail(ErrorCode::PARSE, std::format("{}: {}", file, error.what()));
    }
    catch (const Json::exception& error)
    {
        return fail(ErrorCode::PARSE, std::format("{}: {}", file, error.what()));
    }
}

/// tests/data/goldens/plots/@p file, read and interpreted by @p parse.
template <class T>
[[nodiscard]] Result<T> loadTable(std::string_view file, Result<T> (*parse)(const Json&))
{
    const Result<Json> document = loadGoldenJson(std::filesystem::path("plots") / file);
    if (!document.has_value())
    {
        return std::unexpected(document.error());
    }
    return parse(*document);
}

}  // namespace

const GoldenPlotSimulation* GoldenPlotTable::find(std::string_view file,
                                                  std::size_t      simulation) const
{
    const auto found = std::ranges::find_if(simulations, [&](const GoldenPlotSimulation& each) {
        return each.file == file && each.simulation == simulation;
    });
    return found == simulations.end() ? nullptr : &*found;
}

const GoldenPlotDetailSimulation* GoldenPlotDetails::find(std::string_view file,
                                                          std::size_t      simulation) const
{
    const auto found =
        std::ranges::find_if(simulations, [&](const GoldenPlotDetailSimulation& each) {
            return each.file == file && each.simulation == simulation;
        });
    return found == simulations.end() ? nullptr : &*found;
}

Result<GoldenPlotTable> parseGoldenPlotTable(const nlohmann::json& document)
{
    return parseTable<GoldenPlotTable>(document, "simulation-plots.json", [](const Json& table) {
        GoldenPlotTable result;
        result.provenance = provenanceOf(table, "simulation-plots");
        for (const Json& preset : array(table, "presets"))
        {
            result.presets.push_back(presetOf(preset));
        }
        for (const Json& simulation : array(table, "simulations"))
        {
            result.simulations.push_back(simulationOf(simulation));
        }
        return result;
    });
}

Result<GoldenPlotTable> parseGoldenPlotTableText(std::string_view text)
{
    const Result<Json> document = parseGoldenJson(text);
    if (!document.has_value())
    {
        return std::unexpected(document.error());
    }
    return parseGoldenPlotTable(*document);
}

Result<GoldenPlotDetails> parseGoldenPlotDetails(const nlohmann::json& document)
{
    return parseTable<GoldenPlotDetails>(
        document, "simulation-plot-details.json", [](const Json& table) {
            GoldenPlotDetails result;
            result.provenance = provenanceOf(table, "simulation-plot-details");
            for (const Json& simulation : array(table, "simulations"))
            {
                result.simulations.push_back(detailSimulationOf(simulation));
            }
            return result;
        });
}

const Result<GoldenPlotTable>& goldenPlotTable()
{
    static const Result<GoldenPlotTable> kTable =
        loadTable<GoldenPlotTable>("simulation-plots.json", &parseGoldenPlotTable);
    return kTable;
}

const Result<GoldenPlotDetails>& goldenPlotDetails()
{
    static const Result<GoldenPlotDetails> kDetails =
        loadTable<GoldenPlotDetails>("simulation-plot-details.json", &parseGoldenPlotDetails);
    return kDetails;
}

std::uint64_t javaDoubleBits(double value) noexcept
{
    if (std::isnan(value))
    {
        return std::uint64_t{0x7ff8000000000000};
    }
    return std::bit_cast<std::uint64_t>(value);
}

bool sameDoubleBits(double a, double b) noexcept
{
    return javaDoubleBits(a) == javaDoubleBits(b);
}

std::string shortDigest(std::string_view text)
{
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    return sha256Hex(bytes).substr(0, 16);
}

std::string seriesDigest(const std::vector<double>& x, const std::vector<double>& y)
{
    std::vector<std::byte> bytes;
    bytes.reserve(8 * (x.size() + y.size()));
    for (const std::vector<double>* values : {&x, &y})
    {
        for (const double value : *values)
        {
            const std::uint64_t bits = javaDoubleBits(value);
            for (int shift = 0; shift < 64; shift += 8)
            {
                bytes.push_back(static_cast<std::byte>((bits >> shift) & 0xffU));
            }
        }
    }
    return sha256Hex(bytes).substr(0, 16);
}

}  // namespace QtRocket::Test
