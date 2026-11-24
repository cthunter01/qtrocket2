#pragma once

// The core's plot goldens: OpenRocket's plots of the stored simulations of its example designs,
// which tools/openrocket-plots writes into tests/data/goldens/plots (see its README.md for how
// they are made and what they hold). Test-only, Qt-free: QtRocket_test_support, so that the GUI's
// tests of the plot dialog can compare with the same tables.
//
// Two tables:
// - simulation-plots.json (GoldenPlotTable): every stored simulation of the sixteen examples (54)
//   with each of the 9 presets of SimulationPlotConfiguration (486 plots), compact: the
//   configuration the plot panel takes (a copy in the default units), the automatic axes and every
//   candidate fillAutoAxes() scores, the filled ranges, the titles, the axes' labels and ranges,
//   the colours the panel shows, the series (a digest of their points), the legend, the event
//   groups of all stages, and the icons (a digest of their positions and one of their tool tips).
// - simulation-plot-details.json (GoldenPlotDetails): ten of those simulations in detail: the
//   series' extremes and first, middle and last points, readout tool tips, the events, the groups
//   of each stage and every icon.
// Every double is OpenRocket's to the last bit (only + - * / and sqrt make them, and the readers
// keep the bits: -0.0, NaN and the infinities included); the plot model is compared exactly.
//
// Where QtRocket fixes one of OpenRocket's bugs the tables hold QtRocket's answer, which the
// probe derives with OpenRocket's own code: the event groups have their events in time order and
// are drawn at their first event (M4's bug 41), and the common zero leaves out an infinite bound
// (bug 45: GoldenPlot::openRocket holds OpenRocket's own choice for the one plot it changes).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// The schema version of the plot goldens (gen_simulation_plots.py's SCHEMA_VERSION).
inline constexpr int kGoldenPlotSchemaVersion = 1;

/// Two doubles: a range [min, max] or a point [x, y].
using GoldenPlotPair = std::array<double, 2>;

/// A row of a plot configuration: its type's and unit's names and its axis (-1 automatic).
struct GoldenPlotRow
{
    std::string type;
    std::string unit;
    int         axis{-1};
};

/// The axes of the rows and the two ranges (left, right) a configuration was fitted to.
struct GoldenPlotAxes
{
    std::vector<int>              axes;
    std::array<GoldenPlotPair, 2> ranges{};
};

/// A candidate of fillAutoAxes(): the configuration with the automatic rows on the axes, its
/// goodness on the first branch and the ranges it was scored on.
struct GoldenPlotCandidate
{
    GoldenPlotAxes fit;
    double         goodness{};
};

/// OpenRocket's own choice where its bug 45 makes it differ from QtRocket's.
struct GoldenPlotOpenRocket
{
    int                              bug{};
    GoldenPlotAxes                   automatic;
    GoldenPlotAxes                   filled;
    std::vector<GoldenPlotCandidate> candidates;
};

/// A y axis of the chart: its number of series, its label (none without series) and its range.
struct GoldenPlotYAxis
{
    std::size_t                series{};
    std::optional<std::string> label;
    GoldenPlotPair             range{};
};

/// A series: its name, axis, branch, row, unit, number of points and the digest of its points
/// (seriesDigest()).
struct GoldenPlotSeries
{
    std::string name;
    int         axis{};
    std::size_t branch{};
    std::size_t row{};
    std::string unit;
    std::size_t points{};
    std::string digest;
};

/// An event of a branch: its type's name, time and branch.
struct GoldenPlotEvent
{
    std::string type;
    double      time{};
    std::size_t branch{};
};

/// An event group: its time, its look (the name of its look's event type, for a warning followed
/// by "/" and the warning's priority: "APOGEE", "SIM_WARN/NORMAL"), its events and its label.
struct GoldenPlotGroup
{
    double                       time{};
    std::string                  look;
    std::vector<GoldenPlotEvent> events;
    std::string                  label;
};

/// The icons of a plot: how many, and two digests (shortDigest()): of their positions, a line
/// "<series> <group> <time> <x> <y> <y on the left axis> <look>\n" for each icon with the doubles
/// as the 16 hexadecimal digits of javaDoubleBits(), and of their tool tips, each followed by
/// "\n".
struct GoldenPlotIcons
{
    std::size_t count{};
    std::string digest;
    std::string toolTipDigest;
};

/// One plot: a stored simulation with a preset.
struct GoldenPlot
{
    std::size_t                         preset{};
    std::string                         name;
    std::array<std::string, 2>          domain;
    std::vector<GoldenPlotRow>          rows;
    GoldenPlotAxes                      automatic;
    std::vector<GoldenPlotCandidate>    candidates;
    GoldenPlotAxes                      filled;
    std::optional<GoldenPlotOpenRocket> openRocket;
    std::string                         title;
    std::string                         subtitle;
    std::string                         domainLabel;
    GoldenPlotPair                      domainRange{};
    /// The time domain's range widened for the event lines; nullopt for another domain.
    std::optional<GoldenPlotPair>  domainRangeWithEventLines;
    std::array<GoldenPlotYAxis, 2> yAxes{};
    /// The colour index the panel shows for each row.
    std::vector<int>              panelColors;
    std::vector<GoldenPlotSeries> series;
    std::vector<std::string>      legend;
    /// The groups of every branch.
    std::vector<GoldenPlotGroup> groups;
    GoldenPlotIcons              icons;
};

/// A branch of a stored simulation: its name, number of points, separation time and the index of
/// that time in its data (-1: none).
struct GoldenPlotBranch
{
    std::string name;
    std::size_t points{};
    double      separationTime{};
    int         separationIndex{-1};
};

/// A stored simulation of an example and its 9 plots.
struct GoldenPlotSimulation
{
    std::string                   file;
    std::size_t                   simulation{};
    std::string                   name;
    std::vector<GoldenPlotBranch> branches;
    std::vector<std::string>      seriesLabels;
    std::vector<GoldenPlot>       plots;
};

/// A preset as OpenRocket defines it.
struct GoldenPlotPreset
{
    std::string                name;
    std::array<std::string, 2> domain;
    std::vector<GoldenPlotRow> rows;
    std::vector<std::string>   events;
};

/// Where a table came from.
struct GoldenPlotProvenance
{
    int         schemaVersion{};
    std::string openrocketCommit;
    std::string openrocketVersion;
    bool        dirty{};
};

/// simulation-plots.json.
struct GoldenPlotTable
{
    GoldenPlotProvenance              provenance;
    std::vector<GoldenPlotPreset>     presets;
    std::vector<GoldenPlotSimulation> simulations;

    /// The stored simulation @p simulation of the example file @p file, or null.
    [[nodiscard]] const GoldenPlotSimulation* find(std::string_view file,
                                                   std::size_t      simulation) const;
};

/// A series in detail: its extremes ([x, y], NaN ignored) and its first, middle (points / 2) and
/// last point.
struct GoldenPlotSeriesPoints
{
    GoldenPlotPair min{};
    GoldenPlotPair max{};
    GoldenPlotPair first{};
    GoldenPlotPair middle{};
    GoldenPlotPair last{};
};

/// A readout tool tip: of point @p point of series @p series, with the series' name there.
struct GoldenPlotToolTip
{
    std::size_t series{};
    std::size_t point{};
    std::string name;
    std::string html;
};

/// An icon: its series, group (of the series' branch), time, position and look, and tool tip.
struct GoldenPlotIcon
{
    std::size_t series{};
    std::size_t group{};
    double      time{};
    double      x{};
    double      y{};
    double      yOnLeftAxis{};
    std::string look;
    std::string toolTip;
};

/// A plot in detail.
struct GoldenPlotDetail
{
    std::size_t                         preset{};
    std::vector<GoldenPlotSeriesPoints> series;
    std::vector<GoldenPlotToolTip>      toolTips;
    std::vector<GoldenPlotEvent>        eventList;
    /// The groups of each branch.
    std::vector<std::vector<GoldenPlotGroup>> branchGroups;
    std::vector<GoldenPlotIcon>               icons;
};

/// A simulation in detail.
struct GoldenPlotDetailSimulation
{
    std::string                   file;
    std::size_t                   simulation{};
    std::string                   name;
    std::vector<GoldenPlotDetail> plots;
};

/// simulation-plot-details.json.
struct GoldenPlotDetails
{
    GoldenPlotProvenance                    provenance;
    std::vector<GoldenPlotDetailSimulation> simulations;

    /// The simulation @p simulation of the example file @p file in detail, or null.
    [[nodiscard]] const GoldenPlotDetailSimulation* find(std::string_view file,
                                                         std::size_t      simulation) const;
};

/// Interprets parsed simulation-plots.json (ErrorCode::PARSE, with the place, when a member is
/// missing or of the wrong type or the schema is another).
[[nodiscard]] Result<GoldenPlotTable> parseGoldenPlotTable(const nlohmann::json& document);

/// parseGoldenPlotTable() of the JSON text @p text (ErrorCode::PARSE also when it is not JSON).
[[nodiscard]] Result<GoldenPlotTable> parseGoldenPlotTableText(std::string_view text);

/// Interprets parsed simulation-plot-details.json (as parseGoldenPlotTable()).
[[nodiscard]] Result<GoldenPlotDetails> parseGoldenPlotDetails(const nlohmann::json& document);

/// tests/data/goldens/plots/simulation-plots.json, read and interpreted once per test process.
[[nodiscard]] const Result<GoldenPlotTable>& goldenPlotTable();

/// tests/data/goldens/plots/simulation-plot-details.json, read and interpreted once per test
/// process.
[[nodiscard]] const Result<GoldenPlotDetails>& goldenPlotDetails();

/// The digest of a series' points as the tables store it: the first 16 hexadecimal digits of the
/// SHA-256 of the x values, then the y values, each as the 8 little-endian bytes of its bits
/// (every NaN as 0x7ff8000000000000, Java's Double.doubleToLongBits()).
[[nodiscard]] std::string seriesDigest(const std::vector<double>& x, const std::vector<double>& y);

/// The bits of @p value as Java's Double.doubleToLongBits() gives them (every NaN as
/// 0x7ff8000000000000).
[[nodiscard]] std::uint64_t javaDoubleBits(double value) noexcept;

/// Whether @p a and @p b are the same double: the same bits (so 0.0 is not -0.0), or both NaN.
[[nodiscard]] bool sameDoubleBits(double a, double b) noexcept;

/// The first 16 hexadecimal digits of the SHA-256 of @p text (the icons' digests).
[[nodiscard]] std::string shortDigest(std::string_view text);

}  // namespace QtRocket::Test
