#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <QColor>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QWidget>

#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/enums.h>

#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui
{

class GuiServices;

/// The chart of OpenRocket's data plots (swing gui/plot/Plot.java: what SimulationPlot and
/// Milestone 7's component analysis plot, CAPlot, share; with SimulationChart, the ChartPanel that
/// shows them) on a rocketplot PlotWidget that fills this widget (plotWidget(); objectName "chart",
/// OpenRocket's field; this widget is "chartPanel"). The areas after plot/ use it through this
/// class and its subclasses alone, without naming rocketplot (CLAUDE.md, section GUI): the plot
/// dialog (dialogs/plot/PlotDialog) shows it at its preferred size (sizeHint(), ChartPanel's),
/// zooms it (zoomIn(), zoomOut(), resetZoom()), switches its points and events (setShowPoints(),
/// setShowEvents()) and exports it (exportImage()).
///
/// What Plot's constructor makes, a subclass gives this class (setTitles(), setAxisLabels(),
/// addSeries(), setHomeRanges()):
/// - The title (Plot's plotName) and the subtitle (the configuration's name), the legend under the
///   plot area (OUTSIDE_BOTTOM, shown for one entry too, as JFreeChart's LegendTitle), without the
///   values rocketplot's legend shows at the crosshair (OpenRocket's has none).
/// - The series: each named (its legend name, and its header in rocketplot's CSV export), in a
///   legend group (Plot.LegendItems: one entry for each data type, which stands for its series on
///   every branch), on the left or the right y axis (yAxis2()), in its colour, kLineWidth wide
///   with flat caps and the dashes of its line style (Plot.createPlotStroke(): BasicStroke(1.5f,
///   CAP_BUTT, JOIN_BEVEL, 0, the style's dashes; none for SOLID), the pattern going on along the
///   whole line, as setDrawSeriesLineAsPath(true) draws it). The first series is drawn on top,
///   each under those added before it (rocketplot's SeriesDrawOrder::REVERSE: JFreeChart's
///   XYPlot renders its datasets and their series in reverse order).
/// - The points (setShowPoints(): ModifiedXYItemRenderer.setDefaultShapesVisible(), its shapes
///   filled): a marker at every point, in the series' colour, of the shape of the series' branch
///   (pointShape(): ModifiedXYItemRenderer.lookupSeriesShape(), JFreeChart's
///   DEFAULT_SHAPE_SEQUENCE by the branch, all ten of its shapes).
/// - The axes' labels, the grid of x and of the left y axis (Plot: both grids; JFreeChart draws
///   the gridlines of the first range axis), and the ranges the axis fitting computed
///   (Plot.PresetNumberAxis) as the axes' home ranges (setHomeRanges(), rocketplot's
///   Axis::setHomeRange(): shown at first and after every reset, exactly as given).
/// - A line across the plot at 0 on the left axis (Plot: addRangeMarker(new ValueMarker(0)):
///   JFreeChart's grey, 0.5 pixels, opacity 0.8, over the series). Plot's domain marker at 0 never
///   shows: SimulationPlot's first drawDomainMarkers() clears it, and OpenRocket's plot dialog
///   always calls it (setShowEvents()). Milestone 7's CAPlot has no events: it adds its own.
///
/// SimulationChart's part: the gestures of PlotInputBindings::simulationChart(), rocketplot's
/// context menu with QtRocket's export (PlotExport::install()) and the readout: OpenRocket's chart
/// shows the tool tip of the data point under the pointer (its renderers' tool tip generators);
/// here the crosshair snaps to the data point nearest the pointer (CrosshairMode::SNAP, on from
/// the start) and a tool tip shows that point's text (toolTipText(), the subclass's), until the
/// crosshair leaves the data (the readout hook: QToolTip, or a test's recorder, which keeps the
/// tests' QToolTip windows away, the critic's G7).
///
/// Deliberate differences (the user's decisions U-M3-6a and U-M4-6): the look is rocketplot's
/// (fonts, the light or dark palette, the plot area's background, the legend's swatches); a
/// click on a legend entry hides or shows its series and pointing at it brings them forward
/// (OpenRocket: hovering or clicking an entry draws its series three times as wide); the four
/// triangles among the point shapes are rocketplot's, equilateral within the circle of their size
/// (pointSize(); JFreeChart's fill the 6 x 6 pixel square, their base on one side of it and their
/// tip in the middle of the other); the view can be panned and zoomed beyond the home ranges
/// (rocketplot's N17); the lines' joins are round (Java's bevelled).
class DataPlot : public QWidget
{
    Q_OBJECT

public:
    /// How a series is drawn (addSeries()).
    struct SeriesLook
    {
        /// Its name, as written (rocketplot::literalText() makes it plain): its legend entry's,
        /// when it is the first of its legend group.
        QString name;
        /// The series of one legend entry share it (Series::setLegendGroup()); empty for an entry
        /// of the series' own.
        QString legendGroup;
        /// The y axis: 0 the left one, 1 the right one (yAxis2()).
        int axis = 0;
        /// Its colour, opaque.
        QColor color;
        /// Its line style: its dashes (dashes()), none for SOLID.
        LineStyle lineStyle = LineStyle::SOLID;
        /// Which point shape it gets (pointShape()): the branch of its data.
        std::size_t shape = 0;
    };

    /// Shows @p text at @p globalPosition as the readout's tool tip, or hides the tool tip when
    /// @p text is empty.
    using ReadoutHook = std::function<void(const QPoint& globalPosition, const QString& text)>;

    /// The width of every series' line (Plot.PLOT_STROKE_WIDTH).
    static constexpr double kLineWidth = 1.5;
    /// The size of JFreeChart's shapes of DEFAULT_SHAPE_SEQUENCE (6 x 6 pixels).
    static constexpr double kPointSide = 6.0;
    /// rocketplot draws a SQUARE marker of size s as a square of side s times this, and a
    /// RECTANGLE_WIDE or RECTANGLE_TALL as a rectangle that long and half as wide
    /// (Series::markerSize()).
    static constexpr double kRocketplotSquareScale = 0.85;
    /// The zero line (JFreeChart's ValueMarker: Color.GRAY, a 0.5 pixel stroke, alpha 0.8).
    static constexpr int    kZeroLineGrey  = 128;
    static constexpr double kZeroLineWidth = 0.5;
    static constexpr double kZeroLineAlpha = 0.8;
    /// ChartPanel's preferred size, DEFAULT_WIDTH x DEFAULT_HEIGHT (sizeHint()).
    static constexpr int kPreferredWidth  = 1024;
    static constexpr int kPreferredHeight = 768;
    /// How far the zoom buttons zoom: ChartPanel's zoomInFactor 0.5 halves the ranges about the
    /// middle of the data area, its zoomOutFactor 2.0 doubles them (PlotWidget::zoom(2.0) and
    /// zoom(0.5)).
    static constexpr double kZoomFactor = 2.0;
    /// The point shapes, by branch (pointShape()): JFreeChart's DEFAULT_SHAPE_SEQUENCE in its
    /// order (DefaultDrawingSupplier.createStandardSeriesShapes(), each 6 pixels across about the
    /// point): a square, a circle, a triangle pointing up, a diamond, a rectangle lying (6 x 3), a
    /// triangle pointing down, an ellipse lying (6 x 3), a triangle pointing right, a rectangle
    /// standing (3 x 6) and a triangle pointing left.
    static constexpr std::array kPointShapes{
        rocketplot::Marker::SQUARE,         rocketplot::Marker::CIRCLE,
        rocketplot::Marker::TRIANGLE,       rocketplot::Marker::DIAMOND,
        rocketplot::Marker::RECTANGLE_WIDE, rocketplot::Marker::TRIANGLE_DOWN,
        rocketplot::Marker::ELLIPSE_WIDE,   rocketplot::Marker::TRIANGLE_RIGHT,
        rocketplot::Marker::RECTANGLE_TALL, rocketplot::Marker::TRIANGLE_LEFT,
    };

    ~DataPlot() override;
    DataPlot(const DataPlot&)            = delete;
    DataPlot& operator=(const DataPlot&) = delete;
    DataPlot(DataPlot&&)                 = delete;
    DataPlot& operator=(DataPlot&&)      = delete;

    /// The plot, for plot/ and the tests; the areas after plot/ never name it.
    [[nodiscard]] rocketplot::PlotWidget& plotWidget() const noexcept { return *m_plot; }

    /// The number of series added.
    [[nodiscard]] std::size_t seriesCount() const noexcept { return m_series.size(); }
    /// The series added as @p index (from 0, in the order added); null once removed through its
    /// legend entry's menu (rocketplot's Remove). An index of no series added is a BugError.
    [[nodiscard]] rocketplot::LineSeries* series(std::size_t index) const;

    /// Whether the points are marked (the plot dialog's "Show data points").
    [[nodiscard]] bool showsPoints() const noexcept { return m_showPoints; }
    /// Marks the points of every series, or no point (Plot.setShowPoints()).
    void setShowPoints(bool show);

    /// Draws the plot's flight events, or removes them (the plot dialog's "Show events"):
    /// Plot.setShowEvents(), which does nothing; SimulationPlot draws its events (Milestone 7's
    /// component analysis plot has none).
    virtual void setShowEvents(bool show);

    /// ChartPanel's preferred size (ChartPanel.DEFAULT_WIDTH x DEFAULT_HEIGHT, which
    /// SimulationChart keeps): the size the plot dialog's chart opens with (pack()).
    [[nodiscard]] QSize sizeHint() const override;

    /// The plot dialog's zoom buttons (ChartPanel's ZOOM_IN_BOTH, ZOOM_OUT_BOTH and with Alt held
    /// ZOOM_IN_DOMAIN, ZOOM_OUT_DOMAIN): the ranges halved or doubled about their middles, of x
    /// alone with @p domainOnly; one step of rocketplot's view history.
    void zoomIn(bool domainOnly);
    void zoomOut(bool domainOnly);
    /// The plot dialog's Reset zoom (ChartPanel's ZOOM_RESET_BOTH): back to the home ranges.
    void resetZoom();

    /// Exports the plot as PlotExport::exportPlot() does, asking @p request (the plot dialog's
    /// Export Image asks for a PNG file); whether a file was written.
    bool exportImage(const UserPrompts::SaveFileRequest& request);

    /// The readout's tool tip from now on: @p hook, or, for an empty one, QToolTip (the
    /// default). Tests give a recorder, so that no tool tip window is left behind.
    void setReadoutHook(ReadoutHook hook);
    /// The readout's text now: the tool tip of the data point the crosshair is on, empty while it
    /// is on none.
    [[nodiscard]] const QString& readout() const noexcept { return m_readout; }

    /// The point shape of a series of the branch @p shape (JFreeChart's DEFAULT_SHAPE_SEQUENCE[
    /// branch % 10]): kPointShapes[@p shape % 10], so that an eleventh branch has the first shape
    /// again, as in OpenRocket.
    [[nodiscard]] static rocketplot::Marker pointShape(std::size_t shape) noexcept;
    /// The marker size of @p shape whose shape is kPointSide across, as JFreeChart's are: the
    /// size of a SQUARE, a RECTANGLE_WIDE and a RECTANGLE_TALL is their long side over
    /// kRocketplotSquareScale (a rectangle's short side is half its long one, as JFreeChart's),
    /// that of every other shape the diameter of the circle it is drawn in: a CIRCLE's, an
    /// ELLIPSE_WIDE's width, and the corners of a DIAMOND and of the triangles lie on it.
    [[nodiscard]] static double pointSize(rocketplot::Marker shape) noexcept;

protected:
    /// A plot without series, asking its questions through @p services (which must outlive it),
    /// its points marked as @p showPoints says (the preference "Show data points").
    DataPlot(const GuiServices& services, bool showPoints, QWidget* parent);

    /// The title and the subtitle, as written.
    void setTitles(const QString& title, const QString& subtitle);
    /// The labels of the x axis, the left and the right y axis, as written.
    void setAxisLabels(const QString& domain, const QString& left, const QString& right);
    /// Adds a series through the points (@p x[i], @p y[i]) drawn as @p look says; the plot's
    /// series() then lists it after those added before, and series() here as the next index. A
    /// @p look.axis other than 0 and 1 is a BugError.
    rocketplot::LineSeries& addSeries(std::vector<double>&& x, std::vector<double>&& y,
                                      const SeriesLook& look);
    /// The home ranges (rocketplot's Axis::setHomeRange()): @p x of the x axis, @p y of the y
    /// axis and @p y2 of the secondary one, which each axis shows at first and after every reset
    /// (resetZoom(), a double click, the menu's Reset view), exactly as given, whatever series
    /// are hidden; nullopt for an axis without data. A range rocketplot cannot show (not finite,
    /// its minimum above its maximum, or too narrow for its distance from zero to tell its ends
    /// apart: that of a flat series of values beyond about 1e13, which the axis fitting widens by
    /// 1 on either side) counts as none, which rocketplot would ignore with a warning: such an
    /// axis fits its data, without a margin (the constructor). Replaces the ranges given before;
    /// while an axis autoscales (no zoom or pan since the last reset) it shows its new range at
    /// once.
    void setHomeRanges(std::optional<rocketplot::Range> x, std::optional<rocketplot::Range> y,
                       std::optional<rocketplot::Range> y2);

    /// The readout's text of point @p point of the series added as @p series (both exist): the
    /// tool tip OpenRocket's renderer shows there.
    [[nodiscard]] virtual QString toolTipText(std::size_t series, std::size_t point) const = 0;

    [[nodiscard]] const GuiServices& services() const noexcept { return *m_services; }

private:
    /// Shows or hides the readout for where the crosshair is now.
    void updateReadout();
    /// Marks the points of @p series of the branch @p shape, or none.
    void applyPoints(rocketplot::LineSeries& series, std::size_t shape) const;

    const GuiServices*                            m_services;
    rocketplot::PlotWidget*                       m_plot;
    std::vector<QPointer<rocketplot::LineSeries>> m_series;
    std::vector<std::size_t>                      m_shapes;
    bool                                          m_showPoints;
    ReadoutHook                                   m_readoutHook;
    QString                                       m_readout;
};

}  // namespace QtRocket::Gui
