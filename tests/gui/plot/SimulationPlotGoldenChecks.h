#pragma once

// A drawn simulation plot (plot/SimulationPlot over rocketplot, read back without pixels through
// plot/PlotReadback.h) against OpenRocket's plot of the same stored simulation with the same
// preset: the core's plot goldens (tests/data/goldens/plots, read through
// tests/core/plot/SimulationPlotGoldens.h). What the chart's golden test (plot/
// simulation_plot_chart_golden_tests.cpp: a SimulationPlot of a few simulations) and the exit
// suite of every preset (dialogs/plot/simulation_plot_presets_golden_tests.cpp: every stored
// simulation with every preset, through SimulationPlotDialog) compare:
// - the titles, the axes' labels and ranges, to the last bit (the axes' home ranges,
//   rocketplot's Axis::setHomeRange());
// - the series: their names, axes and colours (those the plot tab shows: bug 42) and a digest of
//   their points;
// - the legend's entries;
// - the event lines over time (marker style "Line"): their places, labels, colours and icons;
// - the event icons (any other domain, or the marker style "Icon"): their number, the digests of
//   their places and tool tips, the image and the axis of each, and, for the simulations of the
//   details table, each icon's place and tool tip.
// Where QtRocket fixes OpenRocket's bugs the goldens hold QtRocket's answers (bugs 41 and 45), and
// the x axis' range over time is the one widened once for the event lines (bug 43). Each
// comparison adds a line for every mismatch it finds (SimulationPlotMismatches), so that a test
// lists them all at once (QtRocket::Test::noGoldenMismatches()).

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include <rocketplot/Axis.h>

#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui
{
class SimulationPlot;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The mismatches of one plot with its golden: lines "preset <k>: <what>", added to a list.
class SimulationPlotMismatches
{
public:
    /// Adds its lines to @p lines (which must outlive it), each prefixed with @p preset.
    SimulationPlotMismatches(std::vector<std::string>& lines, std::size_t preset);

    /// Adds @p what.
    void fail(std::string_view what) const;

    /// A text the plot was given (@p actual) against the golden's, given as rocketplot's literal
    /// text (rocketplot::literalText()).
    void text(std::string_view what, const QString& actual, std::string_view golden) const;

    /// A double against the golden's, to the last bit (QtRocket::Test::sameDoubleBits()).
    void number(std::string_view what, double actual, double golden) const;

    /// The range of @p axis against @p golden ([min, max]), to the last bit.
    void range(std::string_view what, const rocketplot::Axis& axis,
               const QtRocket::Test::GoldenPlotPair& golden) const;

    /// Whether the counts agree (a mismatch otherwise).
    [[nodiscard]] bool sameCount(std::string_view what, std::size_t actual,
                                 std::size_t golden) const;

private:
    std::vector<std::string>* m_lines;
    std::size_t               m_preset;
};

/// The x range of OpenRocket's plot @p golden: over time with its event lines drawn
/// (@p eventLinesDrawn), the range widened for them once (GoldenPlot::domainRangeWithEventLines,
/// OpenRocket bug 43); otherwise GoldenPlot::domainRange (also over time with the marker style
/// "Icon", which draws no line).
[[nodiscard]] const QtRocket::Test::GoldenPlotPair& goldenDomainRange(
    const QtRocket::Test::GoldenPlot& golden, bool eventLinesDrawn);

/// The title, the subtitle, the axes' labels and the axes' ranges of @p plot against @p golden,
/// the x axis' range against @p domainRange (goldenDomainRange()); the right axis shown only when
/// OpenRocket's has series.
void compareSimulationPlotAxes(const SimulationPlot& plot, const QtRocket::Test::GoldenPlot& golden,
                               const QtRocket::Test::GoldenPlotPair& domainRange,
                               const SimulationPlotMismatches&       mismatches);

/// The series of @p plot against @p golden: their number, and each one's name, axis, digest of
/// its points (QtRocket::Test::seriesDigest()) and colour (the plot tab's: GoldenPlot::
/// panelColors).
void compareSimulationPlotSeries(const SimulationPlot&             plot,
                                 const QtRocket::Test::GoldenPlot& golden,
                                 const SimulationPlotMismatches&   mismatches);

/// The entries of @p plot's legend against @p golden's.
void compareSimulationPlotLegend(const SimulationPlot&             plot,
                                 const QtRocket::Test::GoldenPlot& golden,
                                 const SimulationPlotMismatches&   mismatches);

/// The event lines of @p plot (over time, the marker style "Line") against @p golden's groups:
/// one line for each group, at its time, with its label, in the colour of its look
/// (PlotEventGraphics::markerColor()) and with its look's icon (PlotEventGraphics::eventIcon());
/// no icon besides.
void compareSimulationPlotEventLines(const SimulationPlot&             plot,
                                     const QtRocket::Test::GoldenPlot& golden,
                                     const SimulationPlotMismatches&   mismatches);

/// The event icons of @p plot (another domain than the time, or the marker style "Icon") against
/// @p golden: no event line; as many icons as OpenRocket's, the digests of their places and
/// tool tips OpenRocket's (GoldenPlotIcons: the places drawn, with the series, group, time and
/// look of the plot's model icon each stands for, SimulationPlotModel::getIconMarkers()), each on
/// its series' axis and with its look's image; and, with @p detail (the plot in the details
/// table, null for a simulation it does not hold), each icon's place and tool tip.
void compareSimulationPlotEventIcons(const SimulationPlot&                   plot,
                                     const QtRocket::Test::GoldenPlot&       golden,
                                     const QtRocket::Test::GoldenPlotDetail* detail,
                                     const SimulationPlotMismatches&         mismatches);

/// The plot of preset @p preset of @p detail (a simulation of the details table, or null), or
/// null.
[[nodiscard]] const QtRocket::Test::GoldenPlotDetail* goldenPlotDetailOf(
    const QtRocket::Test::GoldenPlotDetailSimulation* detail, std::size_t preset);

/// QtRocket::Test::goldenPlotTable(), or null with a test failure (the error) when it cannot be
/// read.
[[nodiscard]] const QtRocket::Test::GoldenPlotTable* goldenPlotTableOrFail();

/// QtRocket::Test::goldenPlotDetails(), or null with a test failure when it cannot be read.
[[nodiscard]] const QtRocket::Test::GoldenPlotDetails* goldenPlotDetailsOrFail();

}  // namespace QtRocket::Gui::Test
