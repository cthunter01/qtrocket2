#pragma once

// Reading a rocketplot plot back in a test, without pixels or fonts (CLAUDE.md, section GUI): what
// its series, axes and title are, from rocketplot's getters; and, since rocketplot v0.6.0 tells
// neither its legend's entries nor its axes' ticks (the upstream request N11), the texts it
// draws, from its SVG export (QXmlStreamReader over the <text> elements), and from the same export
// the paths it draws (the size a marker comes out, say). Never pin a tick label or a position:
// they follow the fonts. Compare a series' name with rocketplot::literalText(expected), as it was
// given.

#include <optional>
#include <ostream>
#include <vector>

#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <rocketplot/Axis.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/enums.h>

namespace QtRocket::Gui::Test
{

/// A series of a plot as the plot shows it (rocketplot's Series and LineSeries getters).
struct SeriesReadback
{
    /// Series::name(): as given (rich text for a name given through rocketplot::literalText() that
    /// has markup characters); empty for a series the legend leaves out.
    QString            name;
    QString            legendGroup;
    bool               visible = true;
    QColor             color;
    rocketplot::Marker marker          = rocketplot::Marker::NONE;
    double             markerSize      = 0.0;
    double             markerRingWidth = 0.0;
    /// LineSeries::lineWidth(); 0 for a series that is no line.
    double lineWidth = 0.0;
    /// LineSeries::areMarkersAlwaysVisible(); false for a series that is no line.
    bool                markersAlwaysVisible = false;
    std::vector<double> x;
    std::vector<double> y;

    friend bool operator==(const SeriesReadback&, const SeriesReadback&) = default;
};

/// What GoogleTest prints for a series: its name, style and size.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const SeriesReadback& series, std::ostream* out);

/// An axis of a plot (rocketplot's Axis getters).
struct AxisReadback
{
    /// Axis::label(), as given.
    QString label;
    double  min                   = 0.0;
    double  max                   = 0.0;
    bool    autoscale             = false;
    double  autoscaleMargin       = 0.0;
    double  autoscaleIncludesZero = 0.0;
    bool    gridVisible           = false;
    bool    tickLabelsVisible     = false;

    friend bool operator==(const AxisReadback&, const AxisReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const AxisReadback& axis, std::ostream* out);

/// A plot: its title, its x and (primary) y axes and its series.
struct PlotReadback
{
    QString                     title;
    AxisReadback                xAxis;
    AxisReadback                yAxis;
    std::vector<SeriesReadback> series;
};

/// @p series as it is now.
[[nodiscard]] SeriesReadback readSeries(const rocketplot::Series& series);
/// Every series of @p plot as it is now, in the plot's order (PlotWidget::series(): the order
/// added, the later drawn on top).
[[nodiscard]] std::vector<SeriesReadback> readAllSeries(const rocketplot::PlotWidget& plot);
/// The names of @p plot's series in its order, an empty one for an unnamed series.
[[nodiscard]] QStringList seriesNames(const rocketplot::PlotWidget& plot);
/// @p axis as it is now.
[[nodiscard]] AxisReadback readAxis(const rocketplot::Axis& axis);
/// @p plot as it is now.
[[nodiscard]] PlotReadback readPlot(const rocketplot::PlotWidget& plot);

/// A text a plot draws, as its SVG export has it: the characters (of the <text> element and its
/// children, as written), and where the text starts (the element's x and y through the
/// transforms of the groups around it), in the plot's widget coordinates; rotated when those
/// transforms turn it (the y axes' labels read upward).
struct SvgText
{
    QString text;
    QPointF position;
    bool    rotated = false;
};

/// Every text of @p plot's SVG export (PlotWidget::exportSvg() at the plot's size, into a
/// temporary directory that goes again), in the order the export draws them; empty, with a test
/// failure, when the export cannot be written or read.
[[nodiscard]] std::vector<SvgText> svgTexts(const rocketplot::PlotWidget& plot);

/// A path a plot draws, as its SVG export has it: its outline (the <path> element's "d", as
/// written), its fill (the "fill" of the element or of the innermost group around it that has
/// one, as written: "#rrggbb" or "none"; empty when none has one) and where the path's origin is
/// (through the transforms of the groups around it), in the plot's widget coordinates. rocketplot
/// draws a marker of the export as a path around its origin, placed by its group's transform.
struct SvgPath
{
    QString data;
    QString fill;
    QPointF origin;
};

/// Every path of @p plot's SVG export (as svgTexts()), in the order the export draws them.
[[nodiscard]] std::vector<SvgPath> svgPaths(const rocketplot::PlotWidget& plot);

/// The side of @p path if it is a square around its origin with its sides along the axes, as
/// rocketplot draws a SQUARE marker ("M-h,-h L h,-h L h,h L -h,h", closed); nullopt for any other
/// path.
[[nodiscard]] std::optional<double> centredSquareSide(const SvgPath& path);

/// The texts of a plot sorted by what they are (readPlotTexts()).
struct PlotTexts
{
    /// Every text, in the order the SVG export draws them.
    QStringList all;
    /// The legend's entries in the order the legend draws them (row by row): the names of the
    /// entries' series, groups and annotations.
    QStringList legendEntries;
    /// The x axis' tick labels, left to right, and what goes with them on their row (an offset or
    /// a multiplier); empty while the axis shows none.
    QStringList xTickLabels;
    /// The (left) y axis' tick labels, bottom to top, and what goes with them; empty while the
    /// axis shows none.
    QStringList yTickLabels;
};

/// The texts of @p plot (svgTexts()), sorted by where they are: after painting @p plot (QWidget::
/// grab(), so that PlotWidget::legendArea() is where the legend is at the plot's size)
/// - the legend's entries: the texts that start inside legendArea();
/// - the x tick labels: of the unrotated texts below plotArea(), outside the legend, those on the
///   row nearest to the plot area (the row of the topmost of them that starts right of the plot
///   area's left edge: the first tick label is centred on that edge);
/// - the y tick labels: the unrotated texts that start left of plotArea() and below its top,
///   outside the legend, above the x tick labels' row (the title is above the plot area, and the
///   y axis' label is rotated).
/// This relies on how rocketplot v0.6.0 lays a plot out (PlotRenderer::drawLabels(): the tick
/// labels of x under the plot area, of y right-aligned left of it, the axis labels further out)
/// and holds for plots with a title, axis labels, a legend inside or outside the plot area and
/// no secondary y axis, event flags or color key.
[[nodiscard]] PlotTexts readPlotTexts(rocketplot::PlotWidget& plot);

}  // namespace QtRocket::Gui::Test
