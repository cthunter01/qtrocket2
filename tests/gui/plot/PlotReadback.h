#pragma once

// Reading a rocketplot plot back in a test, without pixels or fonts (CLAUDE.md, section GUI): what
// its series, axes (the secondary y axis too) and titles are, from rocketplot's getters; its
// annotations (event markers, icons, texts, reference lines: readAnnotations()), its legend's
// entries (readLegend(), from Legend::entries()) and where the crosshair goes after a pointer
// move (moveCrosshairTo()); the texts it draws, from its SVG export (QXmlStreamReader over the
// <text> elements), and from the same export the paths it draws (the size a marker comes out,
// say). rocketplot's Legend::entries() and Axis::ticks() (since 0.7.0) give the legend's names as
// they were given and the ticks as laid out; the export gives the texts as drawn, a name's
// characters as the user reads them. Never pin a tick label or a position: they follow the fonts.
// Compare a series' name with rocketplot::literalText(expected), as it was given. An annotation's
// icon is read back as its image at OpenRocket's 16 pixels (iconImage()), which a test compares
// with the image of the icon it expects: what the icon is, not how a plot draws it.

#include <cstddef>
#include <optional>
#include <ostream>
#include <vector>

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QtTypes>

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

/// A plot: its title, its x and (primary) y axes and its series, its subtitle and its secondary
/// y axis.
struct PlotReadback
{
    QString                     title;
    AxisReadback                xAxis;
    AxisReadback                yAxis;
    std::vector<SeriesReadback> series;
    QString                     subtitle;
    AxisReadback                yAxis2;
    /// Whether the secondary y axis is drawn (Axis::isShown() of whether a series is on it).
    bool yAxis2Shown = false;
};

/// What a line series draws besides SeriesReadback: its axis, its dashes and its caps
/// (LineSeries::dashPattern(), capStyle()).
struct SeriesLineReadback
{
    /// Series::isOnSecondaryYAxis().
    bool            secondaryYAxis = false;
    QList<qreal>    dashPattern;
    Qt::PenCapStyle capStyle = Qt::RoundCap;

    friend bool operator==(const SeriesLineReadback&, const SeriesLineReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const SeriesLineReadback& line, std::ostream* out);

/// An event marker of a plot (rocketplot's EventMarker getters).
struct EventMarkerReadback
{
    double x = 0.0;
    /// EventMarker::label(), as given.
    QString label;
    QColor  color;
    /// iconImage() of EventMarker::icon(); null without one.
    QImage icon;
    bool   visible = true;

    friend bool operator==(const EventMarkerReadback&, const EventMarkerReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const EventMarkerReadback& marker, std::ostream* out);

/// An icon annotation (rocketplot's IconAnnotation getters).
struct IconReadback
{
    QPointF position;
    /// Annotation::isOnSecondaryYAxis().
    bool secondaryYAxis = false;
    /// iconImage() of IconAnnotation::icon(); null without one.
    QImage  icon;
    QString toolTip;
    bool    visible             = true;
    bool    includedInAutoscale = false;

    friend bool operator==(const IconReadback&, const IconReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const IconReadback& icon, std::ostream* out);

/// A text annotation (rocketplot's TextAnnotation getters).
struct TextReadback
{
    QPointF position;
    /// TextAnnotation::text(), as given.
    QString                           text;
    QColor                            color;
    rocketplot::AnnotationCoordinates coordinates = rocketplot::AnnotationCoordinates::DATA;
    Qt::Alignment                     alignment;
    bool                              arrowVisible = true;
    bool                              frameVisible = false;
    /// TextAnnotation::frameColor(): the one set, else the theme's legend border.
    QColor frameColor;
    bool   visible = true;

    friend bool operator==(const TextReadback&, const TextReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const TextReadback& text, std::ostream* out);

/// A reference line (rocketplot's ReferenceLine getters).
struct ReferenceLineReadback
{
    Qt::Orientation orientation = Qt::Horizontal;
    double          value       = 0.0;
    QString         label;
    QColor          color;
    double          lineWidth      = 0.0;
    Qt::PenStyle    lineStyle      = Qt::DashLine;
    bool            secondaryYAxis = false;
    bool            visible        = true;

    friend bool operator==(const ReferenceLineReadback&, const ReferenceLineReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const ReferenceLineReadback& line, std::ostream* out);

/// The annotations of a plot by kind, each kind in the plot's order (PlotWidget::annotations(),
/// the order added), and how many are of another kind.
struct AnnotationsReadback
{
    std::vector<EventMarkerReadback>   events;
    std::vector<IconReadback>          icons;
    std::vector<TextReadback>          texts;
    std::vector<ReferenceLineReadback> lines;
    std::size_t                        others = 0;
};

/// An entry of a plot's legend (Legend::Entry).
struct LegendEntryReadback
{
    /// The name, as given.
    QString name;
    QString group;
    /// The series it stands for, as their indices in PlotWidget::series().
    std::vector<std::size_t> series;
    bool                     visible = true;

    friend bool operator==(const LegendEntryReadback&, const LegendEntryReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const LegendEntryReadback& entry, std::ostream* out);

/// Where a plot's crosshair is on the data (PlotWidget::crosshairSeries(), crosshairIndex()): the
/// series, as its index in PlotWidget::series(), and its point; both nullopt while it is on none.
struct CrosshairReadback
{
    std::optional<std::size_t> series;
    std::optional<std::size_t> point;

    friend bool operator==(const CrosshairReadback&, const CrosshairReadback&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const CrosshairReadback& crosshair, std::ostream* out);

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
/// What @p series draws besides readSeries() (a series that is no line: its axis only).
[[nodiscard]] SeriesLineReadback readSeriesLine(const rocketplot::Series& series);

/// @p icon as a test compares it: its image at 16 x 16 pixels (OpenRocket's icons' size), in one
/// format; a null image for a null icon.
[[nodiscard]] QImage iconImage(const QIcon& icon);
/// The annotations of @p plot as they are now.
[[nodiscard]] AnnotationsReadback readAnnotations(const rocketplot::PlotWidget& plot);
/// The entries of @p plot's legend as they are now (Legend::entries(), without where they were
/// drawn).
[[nodiscard]] std::vector<LegendEntryReadback> readLegend(const rocketplot::PlotWidget& plot);
/// Moves the pointer, with no button held, to @p position (widget coordinates) over @p plot (a
/// QMouseEvent sent to it), after laying the plot out (QWidget::grab()), and where the
/// crosshair is then. The crosshair must be on (PlotWidget::setCrosshairEnabled()).
CrosshairReadback moveCrosshairTo(rocketplot::PlotWidget& plot, QPointF position);
/// moveCrosshairTo() the widget position of point @p point of @p series of @p plot, moved by
/// @p offset pixels.
CrosshairReadback moveCrosshairToPoint(rocketplot::PlotWidget& plot, std::size_t series,
                                       std::size_t point, QPointF offset = {});

/// A text a plot draws, as its SVG export has it: the characters (of the <text> element and its
/// children, as written), and where the text starts (the element's x and y through the
/// transforms of the groups around it), in the plot's widget coordinates; rotated when those
/// transforms turn it (the y axes' labels read upward). rocketplot (from 0.8.0) draws each line
/// of a text of several lines apart: a text each.
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
/// This relies on how rocketplot (0.6.0 to 0.8.0) lays a plot out (PlotRenderer::drawLabels(): the
/// tick labels of x under the plot area, of y right-aligned left of it, the axis labels further
/// out) and holds for plots with a title, axis labels, a legend inside or outside the plot area and
/// no secondary y axis, event flags or color key. For a plot with those, read the secondary axis
/// with readAxis() and Axis::ticks(), and the flags with readAnnotations().
[[nodiscard]] PlotTexts readPlotTexts(rocketplot::PlotWidget& plot);

}  // namespace QtRocket::Gui::Test
