#pragma once

#include <cstddef>
#include <optional>

#include <QObject>

#include "QtRocket/gui/dialogs/plot/PlotDialog.h"
#include "QtRocket/plot/SimulationPlotModel.h"

class QCheckBox;
class QComboBox;
class QWidget;

namespace QtRocket
{
class Simulation;
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;
class SimulationPlot;
class StyledLabel;

/// OpenRocket's SimulationPlotDialog (swing gui/plot/SimulationPlotDialog.java): the plot dialog
/// (PlotDialog) of a simulation's flight data, which the settings dialog's "Plot data" tab opens
/// (doPlot(): SimulationPlotDialog.getPlot(), through util/SimulationDialogs::plotSimulation()),
/// titled with the simulation's name, its chart a SimulationPlot of the plot configuration given.
/// What it adds to PlotDialog's controls:
/// - Over any x axis but the time, under the help text, the warning "The data is plotted in time
///   order even though the X axis type is not time. " ("timeSeriesWarning",
///   PlotDialog.lbl.timeSeriesWarning, its trailing space as Java's), two points smaller, in the
///   dark error colour (ThemeColors DARK_ERROR, read when the dialog is made, as Java's).
/// - After "Show events", "Show errors" ("checkErrors", PlotDialog.CheckBox.ShowErrors), checked at
///   first, shown only when the flight has an abort (Simulation.hasErrors()): it shows or hides the
///   plot's abort box (SimulationPlot::setShowErrors()). The box shows at once (OpenRocket bug 44,
///   fixed in SimulationPlot: Java's first setShowEvents() hides it until the box is clicked twice
///   or a stage is chosen).
/// - Then, for a flight of two or more stages, the stage box ("stageSelection"): "All Stages"
///   (PlotDialog.StageDropDown.allStages), then each stage by its label
///   (SimulationPlotModel::getSeriesLabels(): "Booster stage(1)", "Booster stage(2)" for two of
///   one name). Choosing a stage shows that stage's series alone with its events and its abort
///   (SimulationPlot::setShowBranch(), whatever "Show events" says, as Java), and enables "Show
///   errors" only for a stage with an abort (hasErrors(stage)); "All Stages" shows every stage,
///   "Show errors" enabled when any has an abort. A flight of one stage has the box, hidden (Java
///   makes it and does not add it).
/// The preferences "Show data points" and "Show events" (PlotPreferences: ShowPlotPoints, false
/// by default; ShowPlotEvents, true) decide how the dialog opens (getPlot()). The size and place
/// are remembered under kGeometryName (Java: the class).
///
/// Deviations from OpenRocket:
/// - showPlot() makes the dialog a child of the settings dialog, window-modal to it, and returns at
///   once (the decision O-M4-17); Java's doPlot() gives it the main window as its owner,
///   DOCUMENT_MODAL, which blocks the settings dialog too: the same windows wait, but here the plot
///   goes with the settings dialog (which cannot be closed while the plot is open).
/// - The dialog reads the aborts from the data it plots (SimulationPlotModel::getAbortLines()),
///   which the plot shares, where Java asks the simulation (hasErrors()) each time a stage is
///   chosen; nothing here refers to the simulation once the dialog is made, so a re-run of the
///   simulation while the dialog is open (which the settings dialog it is opened from prevents)
///   leaves the dialog with the flight it shows.
class SimulationPlotDialog final : public PlotDialog
{
    Q_OBJECT

public:
    /// The dialog's objectName and the name its geometry is remembered under (WindowGeometry).
    static constexpr const char* kGeometryName = "SimulationPlotDialog";

    /// The dialog of @p model's plot (its title the model's), asking its questions through
    /// @p services, which must outlive it. Not shown yet.
    SimulationPlotDialog(const GuiServices& services, SimulationPlotModel model,
                         QWidget* parent = nullptr);

    /// The dialog of @p simulation's flight data plotted as @p configuration says (getPlot()'s
    /// SimulationPlot.create()), as the constructor above.
    /// @throws BugError when @p simulation has no flight data, or no branch
    SimulationPlotDialog(const GuiServices& services, const Simulation& simulation,
                         const SimulationPlotConfiguration& configuration,
                         QWidget*                           parent = nullptr);

    /// What the plot tab's doPlot() does with the dialog (getPlot(parent, simulation,
    /// configuration).setVisible(true)): makes it as a child of @p parent (the settings dialog:
    /// the dialog is a window of its own, placed over @p parent's window, and goes with
    /// @p parent), opens it modal to @p parent's window (QDialog::open(): DOCUMENT_MODAL) and
    /// returns at once; the dialog is deleted once it is closed, however it is closed
    /// (DISPOSE_ON_CLOSE).
    /// @throws BugError as the constructor (no dialog is left behind)
    static SimulationPlotDialog& showPlot(const GuiServices& services, const Simulation& simulation,
                                          const SimulationPlotConfiguration& configuration,
                                          QWidget&                           parent);

    /// The chart (PlotDialog::chart(), "chartPanel").
    [[nodiscard]] SimulationPlot& plot() const noexcept { return *m_plot; }
    /// "Show errors".
    [[nodiscard]] QCheckBox& showErrorsBox() const noexcept { return *m_checkErrors; }
    /// The stage box, hidden for a flight of one stage.
    [[nodiscard]] QComboBox& stageBox() const noexcept { return *m_stageSelection; }
    /// The warning over another x axis than the time; null over the time.
    [[nodiscard]] StyledLabel* timeSeriesWarning() const noexcept { return m_timeSeriesWarning; }

    /// Whether the flight shown has an abort in stage @p branch, or in any stage
    /// (Simulation.hasErrors()).
    /// @throws BugError when there is no branch @p branch
    [[nodiscard]] bool hasErrors(std::optional<std::size_t> branch = std::nullopt) const;

private:
    /// addXAxisWarningMessage(): the warning, or null over the time.
    [[nodiscard]] StyledLabel* makeTimeSeriesWarning();
    /// showErrorsCheckbox(): "Show errors".
    [[nodiscard]] QCheckBox* makeShowErrorsBox();
    /// addSeriesSelectionBox(): the stage box.
    [[nodiscard]] QComboBox* makeStageBox();
    /// The stage box's item @p index chosen (its item listener).
    void chooseStage(int index);

    SimulationPlot* m_plot;
    QCheckBox*      m_checkErrors       = nullptr;
    QComboBox*      m_stageSelection    = nullptr;
    StyledLabel*    m_timeSeriesWarning = nullptr;
};

}  // namespace QtRocket::Gui
