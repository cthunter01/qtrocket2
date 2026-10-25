#pragma once

#include <memory>
#include <span>
#include <vector>

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>

class QEvent;

namespace QtRocket
{
class ThrustCurveMotor;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;

/// The thrust-curve charts of OpenRocket's motor chooser, on a rocketplot PlotWidget that fills
/// this widget (plotWidget()): the preview of MotorInformationPanel (Style::PREVIEW,
/// MotorInformationPanel.java:205-245, 290-317, 365-392) and the chart of
/// ThrustCurveMotorPlotDialog (Style::DIALOG, ThrustCurveMotorPlotDialog.java:41-104, 127-140).
/// The areas after plot/ use it through this class alone, without naming rocketplot (CLAUDE.md,
/// section GUI): ThrustCurvePreview holds a preview, the thrust-curve dialog a DIALOG chart.
///
/// Both show the curves given to setCurves(), each the motor's time and thrust points, in the
/// colours of PlotColors::curveColor() (ThrustCurveMotorSelectionPanel.getColor(): brightened in
/// a dark palette, ThemeColors' rule) by the curve's place in the list, and re-coloured when the
/// palette changes (OpenRocket re-reads its colours on a theme change, :275-288). Both axes fit
/// the data as JFreeChart's NumberAxis does (margins 0.05, the auto range taking in zero): [0,
/// 1.05 times the largest time] and [0, 1.05 times the largest thrust], bit for bit the ranges
/// OpenRocket shows, since every curve starts at 0 s (ThrustCurveMotor::Builder admits a first
/// time within MathUtil's epsilon of it) and no thrust is negative; with no curves (clear()),
/// JFreeChart's default range with its margins, [0, 1.05] on both axes. The grid is on, the
/// theme rocketplot's light or dark one as the palette says (ThemeMode::SYSTEM).
///
/// PREVIEW: titled "Thrust curve:" (TCMotorSelPan.title.Thrustcurve), no axis labels, no
/// legend, no interaction (PlotInputBindings::setNoInteraction(): OpenRocket's ChartPanel without
/// zoom, wheel or popup; this widget prevents a context menu too). Curve i is "Thrust (i)"
/// (TCMotorSelPan.title.Thrust), 3 pixels wide for the selected curve and 1 for the others, in
/// newtons and seconds as OpenRocket plots it (its preview has no axis label to name a unit).
/// JFreeChart draws series 0 on top (SeriesRenderingOrder.REVERSE) and rocketplot the last one
/// added (upstream request N10), so the curves are added from the last: plotWidget()'s series()
/// are "Thrust (n-1)" ... "Thrust (0)".
///
/// DIALOG: titled "Motor thrust curves", the axes labelled "Time / <unit>" and "Thrust / <unit>"
/// in the default units of short time and force, the legend under the plot area (OUTSIDE_BOTTOM,
/// shown for one curve too, as JFreeChart's), JFreeChart's gestures (PlotInputBindings::
/// chartPanel()), rocketplot's context menu with QtRocket's export (PlotExport::install()), the
/// crosshair off and snapping to the data when the menu turns it on. The selected curve comes
/// first, named "<manufacturer> <designation>", with a filled 6-pixel square at every point (no
/// ring around the markers, rocketplot v0.6.0's N1); then the others in the list's order, named
/// "<manufacturer> <designation> (n)" with n from 1, without markers; all 1.5 pixels wide, each in
/// the colour of its place in the list (the selected one too). rocketplot draws them in that
/// order, so the selected curve lies under the others, where OpenRocket draws it on top (N10:
/// the legend's order is kept instead, the decision O-M3-19).
///
/// Deliberate differences: the DIALOG chart plots the thrust and the time in the units its axes
/// name; OpenRocket labels the thrust axis in the default force unit but plots newtons
/// (ThrustCurveMotorPlotDialog.java:44, 127-140; bug 12 of the plan's section 10.3, fixed). The
/// look is rocketplot's (the user's decision U-M3-6a): the dialog follows the light or dark
/// palette (OpenRocket's plot area stays white with light grey grid lines), both charts are drawn
/// on the palette's Base colour (OpenRocket: the panel's colour around the plot area and, in the
/// preview, OR.colors.background inside it) in the theme's text colours, the fonts are
/// rocketplot's (OpenRocket: the preview's tick labels 2 points smaller than its axis font, the
/// titles in the panel's plain font or Dialog bold 20), the grid lines solid hairlines in the
/// theme's colour (OpenRocket: dashed, 0.5 pixels, FIN_GRID_MAJOR or light grey) and the lines'
/// joins round (Java's BasicStroke: mitred; the caps are square as Java's).
class ThrustCurvePlot : public QWidget
{
    Q_OBJECT

public:
    /// Which of OpenRocket's two charts.
    enum class Style
    {
        /// MotorInformationPanel's preview.
        PREVIEW,
        /// ThrustCurveMotorPlotDialog's chart.
        DIALOG,
    };

    /// The preview's line width of the selected curve and of the others
    /// (MotorInformationPanel.updateData(): BasicStroke(selected ? 3 : 1)).
    static constexpr double kPreviewSelectedLineWidth = 3.0;
    static constexpr double kPreviewLineWidth         = 1.0;
    /// The dialog's line width (BasicStroke(1.5f)) and the size of the selected curve's markers
    /// (DefaultDrawingSupplier's first shape, a 6 x 6 square).
    static constexpr double kDialogLineWidth  = 1.5;
    static constexpr double kDialogMarkerSize = 6.0;
    /// JFreeChart's axis margins (ValueAxis.DEFAULT_LOWER_MARGIN, DEFAULT_UPPER_MARGIN).
    static constexpr double kAxisMargin = 0.05;
    /// The upper end of both axes without curves: JFreeChart's default auto range [0, 1] with its
    /// upper margin, as NumberAxis.autoAdjustRange() computes it.
    static constexpr double kEmptyRangeMax = 1.0 + (1.0 * kAxisMargin);

    /// The preview's chart (Style::PREVIEW), without curves.
    explicit ThrustCurvePlot(QWidget* parent = nullptr);

    /// The dialog's chart (Style::DIALOG), without curves, exporting through the prompts of
    /// @p services, which must outlive it (PlotExport::install()).
    explicit ThrustCurvePlot(const GuiServices& services, QWidget* parent = nullptr);

    [[nodiscard]] Style chartStyle() const noexcept { return m_style; }

    /// Shows the curves of @p motors (the list MotorInformationPanel.updateData() gets: the
    /// chooser's curves of a motor set, in its order), @p selected the index of the chosen one in
    /// it, or -1 for none (OpenRocket always has one). Replaces the curves shown before, turns
    /// the axes' autoscale back on and, in the DIALOG chart, reads the default units again (the
    /// axis labels and the data). A null motor, or @p selected outside -1 .. size - 1, is a
    /// BugError.
    void setCurves(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected);

    /// No curves, both axes [0, 1.05] (MotorInformationPanel.clearData()'s empty dataset), and in
    /// the DIALOG chart the axis labels in the default units.
    void clear();

    /// The number of curves setCurves() was last given (0 after clear()).
    [[nodiscard]] int curveCount() const noexcept { return static_cast<int>(m_curves.size()); }

    /// The index setCurves() was last given as the selected curve (-1 after clear()).
    [[nodiscard]] int selectedCurve() const noexcept { return m_selected; }

    /// The plot (objectName "chart", as OpenRocket's JFreeChart field), for plot/ and the tests;
    /// the areas after plot/ never name it.
    [[nodiscard]] rocketplot::PlotWidget& plotWidget() const noexcept { return *m_plot; }

protected:
    /// Re-colours the curves when the palette changes (a curve the user gave another colour
    /// through the legend's menu keeps it).
    void changeEvent(QEvent* event) override;

private:
    /// A curve shown: its series (null once removed through the legend's menu), its index in the
    /// list given (its colour's) and the colour it was given.
    struct Curve
    {
        QPointer<rocketplot::LineSeries> series;
        int                              colorIndex = 0;
        QColor                           color;
    };

    ThrustCurvePlot(Style style, QWidget* parent);

    /// The axis labels of the DIALOG chart in the default units.
    void updateAxisLabels();
    /// Adds @p motor's curve, in seconds and newtons (PREVIEW) or in the default units (DIALOG),
    /// named @p name in the colour of @p colorIndex, @p lineWidth wide.
    rocketplot::LineSeries& addCurve(const ThrustCurveMotor& motor, const QString& name,
                                     int colorIndex, double lineWidth);
    void addPreviewCurves(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
                          int                                                      selected);
    void addDialogCurves(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
                         int                                                      selected);
    void recolor();

    Style                   m_style;
    rocketplot::PlotWidget* m_plot = nullptr;
    std::vector<Curve>      m_curves;
    int                     m_selected = -1;
};

}  // namespace QtRocket::Gui
