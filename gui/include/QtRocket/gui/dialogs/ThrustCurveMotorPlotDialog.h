#pragma once

#include <memory>
#include <span>

#include <QDialog>

class QPushButton;
class QWidget;

namespace QtRocket
{
class ThrustCurveMotor;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;
class ThrustCurvePlot;

/// OpenRocket's ThrustCurveMotorPlotDialog (swing gui/dialogs/motor/thrustcurve/
/// ThrustCurveMotorPlotDialog.java): the window "Motor thrust curves" that the motor chooser's
/// thrust-curve preview opens when it is clicked (MotorInformationPanel's listener, run 2), with
/// the curves of the chosen motor set in a ThrustCurvePlot of Style::DIALOG (see there for the
/// chart: the selected curve first, as filled squares, the legend below, the axes in the default
/// units, JFreeChart's gestures, QtRocket's export) and a Close button.
///
/// - Modal for the whole application (Dialog.ModalityType.APPLICATION_MODAL), titled "Motor
///   thrust curves" (a text OpenRocket does not translate).
/// - The layout (MigLayout "fill"): the chart takes the space, at least kChartWidth x
///   kChartHeight and that large when the dialog opens ("width 600:600:, height 400:400:, grow":
///   the size the dialog is made with is the contents', as Java's pack()); below it, a paragraph
///   gap ("wrap para") apart, the Close button at the right ("right, tag close").
/// - Close, Esc and the window's close button close the dialog (Java: setVisible(false), and
///   GUIUtil.setDisposableDialogOptions()'s Esc and DISPOSE_ON_CLOSE). The dialog has no default
///   button (setDisposableDialogOptions(this, null)): Enter clicks nothing.
/// - The objects' names are Java's: the dialog "plotDialog" (MotorInformationPanel's variable), the
///   chart "chartPanel", the button "close".
///
/// Deviations from OpenRocket:
/// - showPlot() returns as soon as the dialog is shown (Java's setVisible() of the modal dialog
///   returns once it is closed; nothing after it depends on that): no event loop of its own runs
///   under the click.
/// - A dialog showPlot() made is deleted however it closes, Close included (Java's Close only hides
///   it, setVisible(false) without dispose(); nothing shows the difference).
/// - The chart's look and gestures are rocketplot's (ThrustCurvePlot, the user's decision U-M3-6).
class ThrustCurveMotorPlotDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The chart's smallest size, which is also its size when the dialog opens.
    static constexpr int kChartWidth  = 600;
    static constexpr int kChartHeight = 400;
    /// MigLayout's paragraph gap ("wrap para"), between the chart and the button.
    static constexpr int kParagraphGap = 12;

    /// The dialog showing @p motors (the list the chooser's preview shows: ThrustCurvePlot::
    /// setCurves()) with the curve @p selected chosen, exporting through the prompts of
    /// @p services, which must outlive it. Not shown yet. The motors are not kept: the chart
    /// copies their points. A null motor, or @p selected outside -1 .. size - 1, is a BugError.
    ThrustCurveMotorPlotDialog(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
                               int selected, const GuiServices& services,
                               QWidget* parent = nullptr);

    /// What MotorInformationPanel's click does (new ThrustCurveMotorPlotDialog(selectedMotorSet,
    /// selectedMotorSet.indexOf(selectedMotor), window).setVisible(true)): makes the dialog as a
    /// child of @p parent (the widget clicked: the dialog is a window of its own, placed over
    /// @p parent's window, and goes with @p parent), shows it modal for the application and
    /// returns at once; the dialog is deleted once it is closed, however it is closed
    /// (DISPOSE_ON_CLOSE). The caller checks that a motor is selected (Java's listener returns
    /// otherwise). A null motor, or @p selected outside -1 .. size - 1, is a BugError, and no
    /// dialog is left behind.
    static ThrustCurveMotorPlotDialog& showPlot(
        std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected,
        const GuiServices& services, QWidget& parent);

    /// The chart (objectName "chartPanel").
    [[nodiscard]] ThrustCurvePlot& chart() const noexcept { return *m_chart; }
    /// The Close button (objectName "close").
    [[nodiscard]] QPushButton& closeButton() const noexcept { return *m_close; }

private:
    ThrustCurvePlot* m_chart;
    QPushButton*     m_close = nullptr;
};

}  // namespace QtRocket::Gui
