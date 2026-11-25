#pragma once

#include <vector>

#include <QDialog>
#include <QObject>
#include <QString>
#include <Qt>

#include "QtRocket/gui/util/UserPrompts.h"

class QCheckBox;
class QEvent;
class QPushButton;
class QWidget;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DataPlot;
class GuiServices;
class StyledLabel;
class WindowGeometry;

/// OpenRocket's PlotDialog (swing gui/plot/PlotDialog.java): the window that shows a data plot (a
/// DataPlot: SimulationPlotDialog's SimulationPlot, Milestone 7's component analysis plot) with the
/// controls OpenRocket's plots share. A subclass makes its plot and its extras as children of the
/// dialog and hands them to build() from its constructor, which lays the dialog out (Java's
/// PlotDialog constructor, which asks the subclass's hooks addXAxisWarningMessage(),
/// showErrorsCheckbox() and addSeriesSelectionBox() for them; here they are given: Extras). From
/// the top (MigLayout "fill, hidemode 3", rows "[grow][]"):
/// - the chart ("chartPanel"), which takes the space ("grow"), at its preferred size when the
///   dialog opens (DataPlot::sizeHint(): ChartPanel's 1024 x 768), kChartGap below it ("wrap
///   20lp");
/// - the help text ("chartHelp", two points smaller: StyledLabel(-2)), QtRocket's own, which
///   describes the gestures of PlotInputBindings::simulationChart() (helpText()), wrapped at the
///   dialog's width;
/// - the subclass's messages (Extras::messages, addXAxisWarningMessage());
/// - one row: "Show data points" ("checkData", PlotDialog.CheckBox.Showdatapoints), checked as
///   the plot shows its points; "Show events" ("checkEvents", PlotDialog.CheckBox.ShowEvents),
///   checked as the dialog is made, which draws the plot's events or not
///   (DataPlot::setShowEvents()); the subclass's controls (Extras::controls: showErrorsCheckbox(),
///   addSeriesSelectionBox()); the zoom buttons, icons without a text: Zoom in ("zoomIn",
///   Icons ZOOM_IN), Reset zoom ("resetZoom", ZOOM_RESET) and Zoom out ("zoomOut", ZOOM_OUT);
///   "Export Image" ("exportImage", PlotDialog.btn.exportImage); and at the right end
///   ("gapbefore push, right"), Close ("close", dlg.but.close).
///
/// What the controls do, as in OpenRocket:
/// - "Show data points" marks the points or not (DataPlot::setShowPoints()) and stores the choice
///   (PlotPreferences::setShowPoints(): the preference ShowPlotPoints the next plot dialog opens
///   with); "Show events" draws or removes the events (DataPlot::setShowEvents()) and stores the
///   choice (PlotPreferences::setShowEvents(), ShowPlotEvents). A box changes only when the user
///   clicks it (Java's ActionListener).
/// - The zoom buttons zoom in and out by ChartPanel's factor 2 about the middle of the ranges,
///   of the x axis alone when Alt is held as the button is clicked (ZOOM_IN_DOMAIN,
///   ZOOM_OUT_DOMAIN), and reset the zoom to the plot's home ranges (DataPlot::zoomIn(),
///   zoomOut(), resetZoom()).
/// - "Export Image" asks for a PNG file and writes the plot there (doPNGExport(): exportImage()).
/// - Close, Escape and the window's close button close the dialog; Close is the default button,
///   which Enter presses wherever the focus is (GUIUtil.setDisposableDialogOptions(this, button));
///   the other buttons are never the default.
/// - The dialog's size and place are remembered (WindowGeometry, under the subclass's name:
///   GUIUtil.rememberWindowSize() and rememberWindowPosition() keep them by the dialog's class);
///   without a remembered size it opens at its contents' size, the chart at its preferred size
///   (pack(): not limited to two thirds of the screen, as QWidget::adjustSize() would; the help
///   text as tall as it wraps at that width).
///
/// Deviations from OpenRocket:
/// - The help text is QtRocket's (the user's decision U-M4-6): OpenRocket's PlotDialog.lbl.Chart
///   describes JFreeChart's gestures ("right click drag to zoom dynamically", which rocketplot has
///   not: its N19). It wraps (OpenRocket's is one line, which sets the dialog's least width).
/// - Zoom out goes beyond the home ranges (rocketplot's N17): OpenRocket's axes refuse a range
///   beyond those its axis fitting computed (Plot.PresetNumberAxis), so its Zoom out does nothing
///   until the plot has been zoomed in.
/// - A PNG that cannot be written shows "Error writing file" (PlotExport); OpenRocket's
///   doPNGExport() ignores the failure.
/// - Without a remembered place Qt centres the dialog over its parent's window (Java centres it
///   over its owner, then lets the platform place it, setLocationByPlatform(true)); the dialog's
///   size is without its frame; WindowGeometry's own differences from SwingPreferences' keys.
/// - The chart can be made no smaller than rocketplot's least size (160 x 120); a ChartPanel can
///   shrink to 12 x 12.
class PlotDialog : public QDialog
{
    Q_OBJECT

public:
    /// The gap below the chart ("wrap 20lp").
    static constexpr int kChartGap = 20;
    /// The help text's and the messages' size, in points relative to the dialog's font
    /// (StyledLabel(text, -2)).
    static constexpr double kSmallTextSize = -2.0;

    ~PlotDialog() override;
    PlotDialog(const PlotDialog&)            = delete;
    PlotDialog& operator=(const PlotDialog&) = delete;
    PlotDialog(PlotDialog&&)                 = delete;
    PlotDialog& operator=(PlotDialog&&)      = delete;

    /// The controls, as build() made them.
    [[nodiscard]] DataPlot&    chart() const noexcept { return *m_chart; }
    [[nodiscard]] StyledLabel& helpLabel() const noexcept { return *m_help; }
    [[nodiscard]] QCheckBox&   showPointsBox() const noexcept { return *m_checkData; }
    [[nodiscard]] QCheckBox&   showEventsBox() const noexcept { return *m_checkEvents; }
    [[nodiscard]] QPushButton& zoomInButton() const noexcept { return *m_zoomIn; }
    [[nodiscard]] QPushButton& resetZoomButton() const noexcept { return *m_resetZoom; }
    [[nodiscard]] QPushButton& zoomOutButton() const noexcept { return *m_zoomOut; }
    [[nodiscard]] QPushButton& exportButton() const noexcept { return *m_export; }
    [[nodiscard]] QPushButton& closeButton() const noexcept { return *m_close; }

    /// The name the dialog's size and place are remembered under (WindowGeometry), its
    /// objectName too.
    [[nodiscard]] const QString& geometryName() const noexcept { return m_geometryName; }

    /// The help text under the chart: what the gestures of PlotInputBindings::simulationChart()
    /// do, worded as OpenRocket's PlotDialog.lbl.Chart words JFreeChart's (QtRocket's own text).
    [[nodiscard]] static QString helpText();

    /// The question of "Export Image" (doPNGExport()'s SaveFileChooser): no title (the file
    /// dialog's own, as Swing's), FileHelper.PNG_FILTER alone, starting in @p preferences'
    /// default directory (none: the user's home directory), refusing illegal names.
    [[nodiscard]] static UserPrompts::SaveFileRequest exportImageRequest(
        const Preferences& preferences);

    /// What "Export Image" does (doPNGExport()): asks exportImageRequest() through the services'
    /// prompts, gives the file chosen the extension ".png" unless its name has it (in any case:
    /// FileHelper.forceExtension()), asks before replacing a file, and writes the plot there as a
    /// PNG image at the chart's size (DataPlot::exportImage(), PlotExport). The default directory
    /// is not stored. Whether a file was written.
    bool exportImage();

protected:
    /// Notes the modifiers of the input that clicks a zoom button (Java reads them from the
    /// button's ActionEvent).
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// A dialog asking its questions through @p services, which must outlive it, remembering its
    /// size and place under @p geometryName (also its objectName): not laid out until build().
    PlotDialog(const GuiServices& services, QString geometryName, QWidget* parent);

    /// What a subclass adds to the dialog (Java's hooks, called by PlotDialog's constructor;
    /// nothing by default): widgets it made as children of the dialog, which build() places.
    struct Extras
    {
        /// Under the help text, in order (addXAxisWarningMessage()); null ones are left out.
        std::vector<QWidget*> messages;
        /// In the row after "Show events", in order (showErrorsCheckbox(),
        /// addSeriesSelectionBox()); null ones are left out.
        std::vector<QWidget*> controls;
    };

    /// Lays the dialog out around @p plot (a child of the dialog) with @p extras, as PlotDialog's
    /// constructor: see the class comment. "Show data points" is checked as @p plot shows its
    /// points; "Show events" is checked as @p initialShowEvents says, and @p plot draws its events
    /// or not accordingly (Java: plot.setShowEvents(initialShowEvents)). Called once, from the
    /// subclass's constructor.
    void build(DataPlot& plot, bool initialShowEvents, const Extras& extras);

    [[nodiscard]] const GuiServices& services() const noexcept { return *m_services; }

private:
    /// The zoom buttons' clicks: whether Alt was held (the last modifiers noted), then forgotten.
    [[nodiscard]] bool takeDomainOnly();

    const GuiServices*    m_services;
    QString               m_geometryName;
    DataPlot*             m_chart       = nullptr;
    StyledLabel*          m_help        = nullptr;
    QCheckBox*            m_checkData   = nullptr;
    QCheckBox*            m_checkEvents = nullptr;
    QPushButton*          m_zoomIn      = nullptr;
    QPushButton*          m_resetZoom   = nullptr;
    QPushButton*          m_zoomOut     = nullptr;
    QPushButton*          m_export      = nullptr;
    QPushButton*          m_close       = nullptr;
    WindowGeometry*       m_geometry    = nullptr;
    Qt::KeyboardModifiers m_clickModifiers;
};

}  // namespace QtRocket::Gui
