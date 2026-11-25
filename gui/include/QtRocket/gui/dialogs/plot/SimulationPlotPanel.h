#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QColor>
#include <QObject>

#include "QtRocket/gui/dialogs/plot/PlotPanel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QTableView;
class QWidget;

namespace QtRocket
{
class FlightDataType;
class Simulation;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DescriptionArea;
class FlightEventTableModel;
class PlotTypeSelector;
class SimulationPlotTypeSelector;

/// A simulation's "Plot data" tab (OpenRocket's SimulationPlotPanel, swing gui/simulation/), which
/// run 3's SimulationConfigDialog shows: PlotPanel over the types of the simulation's first stage
/// (its flight data's branch 0, both as X and as Y types), the 9 presets
/// (SimulationPlotConfiguration::defaultConfigurations()) and "Custom", and its own parts:
/// - after the domain unit, while the X type is not the time, "The data will be plotted in time
///   order even if the X axis type is not time." (descriptionArea(), "simPlotPanelDesc",
///   simplotpanel.Desc: a DescriptionArea of two rows, two points smaller, in the dark error
///   colour; hidden but keeping its place over the time, as MigLayout's hidemode 0);
/// - beside the rows, "Flight events:" (simplotpanel.lbl.Flightevents) over the events table
///   (eventTable(), "flightEventTable": FlightEventTableModel over the flight's event types,
///   without a header, grid or selection; kEventTableWidth x kEventTableHeight to begin with, its
///   check column as narrow as a row is high plus 2; a click on a name checks or unchecks it),
///   "All" and "None" (simplotpanel.but.All, None, "allButton", "noneButton": every
///   FlightEvent::Type marked, or none, also the types the table does not list; one width, a
///   paragraph gap at their sides), and "Marker style:" (simplotpanel.MarkerStyle.lbl.MarkerStyle,
///   its tooltip simplotpanel.MarkerStyle.lbl.MarkerStyle.ttip) with "Vertical line" and "Icon"
///   (simplotpanel.MarkerStyle.btn.VerticalMarker, btn.Icon, "radioVerticalMarker", "radioIcon"),
///   the preference PlotPreferences::isMarkerStyleIcon(), which "Icon" stores when it changes;
///   enabled only over the time, else all three with the tooltip
///   simplotpanel.MarkerStyle.OnlyInTime ("Only available for time domain, other domains only
///   support icon markers");
/// - each row is a SimulationPlotTypeSelector: its colour is the configuration's colour of the row,
///   else the plot's colour of the row (defaultPlotColor(): SimulationPlotModel::
///   computeDefaultColorIndices(), as the plot draws it); its line style the configuration's.
///
/// The configuration starts as the one last plotted (GuiServices::plotDefaults(): any
/// simulation's, in any window; the types this flight lacks dropped, which makes it "Custom";
/// without one the first preset, "Vertical motion vs. time"). doPlot() opens the plot dialog of the
/// simulation for it (GuiServices::simulationDialogs()) and keeps it, without its looks, as the one
/// last plotted.
///
/// The rows' looks (Java's appearances): the look a row of a type starts with (when the rows are
/// made and when the row's type changes) is the simulation's plot appearance of the type
/// (Simulation::getPlotAppearance()) unless it has none or the factory default, else the user's
/// default of the type (PlotPreferences::getDefaultPlotAppearance()), else none: its colour and
/// line style become the configuration's. A colour or line style the user picks becomes the
/// configuration's and the simulation's appearance of the type at once
/// (Simulation::setPlotAppearance()). "Set as default" stores the row's look (its own colour or
/// none, its line style) as the user's default of the type; "Reset to factory default" removes the
/// simulation's appearance of the type and gives the row no colour of its own (the plot's shown)
/// and a solid line. Reset is enabled unless the look is the factory default
/// (PlotAppearance::factoryDefault()); Set as default unless it is the user's default (with none
/// stored: unless it is the factory default).
///
/// OpenRocket's behaviour, kept (documented, as OpenRocket shows it; the plan's list of bugs to fix
/// has none of them):
/// - The plot's colours the rows show are computed before a row's type or axis changes (Swing
///   calls a combo box's listeners last added first: SimulationPlotPanel's before PlotPanel's),
///   so they are those of the configuration before the change until the rows are made again (a
///   preset, New Y axis plot type, a remove). OpenRocket's SimulationChartTest.
///   automaticColorsRefreshWhenLaterVariableChangesAxisAssignment, ported, depends on it.
/// - The description and the marker style follow the X type only when the user chooses it:
///   neither follows a preset's X type, nor does the description show when the panel opens over
///   another X type than the time (the marker style does).
///
/// Deviations from OpenRocket: the panel keeps the simulation (a std::shared_ptr) and reads its
/// flight data again when it computes the plot's colours (Java: the same, through the reference);
/// the events table's types are read once, when the panel is made (as Java). A simulation without
/// data is a BugError (Java's create(): IllegalArgumentException "Simulation contains no data.").
class SimulationPlotPanel final : public PlotPanel
{
    Q_OBJECT

public:
    /// The events table's width and height to begin with ("width 200lp" and the scroll pane's
    /// preferred height of 150), px.
    static constexpr int kEventTableWidth  = 200;
    static constexpr int kEventTableHeight = 150;
    /// The description's rows and its size change, in points (DescriptionArea("", 2, -2.0f)).
    static constexpr int    kDescriptionRows = 2;
    static constexpr double kDescriptionSize = -2.0;

    /// The tab of @p simulation (Java: create()), asking its questions, opening the plot dialog and
    /// keeping the configuration last plotted through @p services.
    /// @throws BugError when @p simulation is null or has no flight data, or no branch
    SimulationPlotPanel(const GuiServices& services, std::shared_ptr<Simulation> simulation,
                        QWidget* parent = nullptr);
    ~SimulationPlotPanel() override;
    SimulationPlotPanel(const SimulationPlotPanel&)            = delete;
    SimulationPlotPanel& operator=(const SimulationPlotPanel&) = delete;
    SimulationPlotPanel(SimulationPlotPanel&&)                 = delete;
    SimulationPlotPanel& operator=(SimulationPlotPanel&&)      = delete;

    /// The configuration the tab starts from with @p services (see the class comment): the one
    /// last plotted, or the first preset.
    [[nodiscard]] static SimulationPlotConfiguration startConfiguration(
        const GuiServices& services);

    /// The presets of the preset box: the 9 presets, then "Custom" (Java: PRESET_ARRAY).
    [[nodiscard]] static Configurations presetArray();

    /// The plot button of the settings dialog (Java: doPlot(parent)): without a row, the error
    /// "Please add one or more variables to plot on the Y-axis." titled "Nothing to plot"
    /// (SimulationPlotPanel.error.noPlotSelected, .title) and false; else a copy of the
    /// configuration without its looks (clearPlotAppearances()) becomes the one last plotted
    /// (PlotDefaults::setLastPlotted()), the plot dialog opens over @p parent for the configuration
    /// (SimulationDialogs::plotSimulation()), and true.
    /// @throws BugError when the services have no SimulationDialogs
    bool doPlot(QWidget* parent);

    /// Shows the configuration (PlotPanel::updatePlots()), the plot's colours computed first; then
    /// the events table shows its checks again.
    void updatePlots() override;

    /// The simulation.
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }

    /// Row @p index (PlotPanel::selector()) as the SimulationPlotTypeSelector it is.
    /// @throws BugError when there is no row @p index
    [[nodiscard]] SimulationPlotTypeSelector& plotTypeSelector(std::size_t index) const;

    /// The plot's colour of row @p index as last computed (Java: defaultPlotColors.getOrDefault(
    /// index, Util.getPlotColor(index))).
    [[nodiscard]] QColor defaultPlotColor(std::size_t index) const;

    /// The parts of the tab (see the class comment).
    [[nodiscard]] DescriptionArea&       descriptionArea() const noexcept { return *m_description; }
    [[nodiscard]] QTableView&            eventTable() const noexcept { return *m_eventTable; }
    [[nodiscard]] FlightEventTableModel& eventTableModel() const noexcept { return *m_eventModel; }
    [[nodiscard]] QPushButton&           allButton() const noexcept { return *m_allButton; }
    [[nodiscard]] QPushButton&           noneButton() const noexcept { return *m_noneButton; }
    [[nodiscard]] QLabel&       markerStyleLabel() const noexcept { return *m_styleEventMarker; }
    [[nodiscard]] QRadioButton& verticalMarkerButton() const noexcept
    {
        return *m_radioVerticalMarker;
    }
    [[nodiscard]] QRadioButton& iconMarkerButton() const noexcept { return *m_radioIcon; }

protected:
    [[nodiscard]] PlotTypeSelector* createSelector(std::size_t index, const FlightDataType& type,
                                                   const Unit& unit, int axis,
                                                   QWidget* parent) override;
    void addSelectionListeners(PlotTypeSelector& selector, std::size_t index) override;

private:
    /// The widgets of the tab's own parts.
    [[nodiscard]] DescriptionArea* makeDescription();
    [[nodiscard]] QWidget*         makeEventsPanel();

    /// The edits of the X type (the description, the marker style) and of row @p index.
    void domainTypeSelected();
    void updateStyleEventWidgets();
    void axisSelected();
    void typeSelected(SimulationPlotTypeSelector& selector, std::size_t index);
    void colorSelected(SimulationPlotTypeSelector& selector, std::size_t index);
    void lineStyleSelected(SimulationPlotTypeSelector& selector, std::size_t index);
    void setAsDefault(SimulationPlotTypeSelector& selector, std::size_t index);
    void resetToFactory(SimulationPlotTypeSelector& selector, std::size_t index);
    void setAllEvents(bool active);

    /// The enabled states of row @p index's appearance buttons (Java: syncButtonStates()).
    void syncButtonStates(SimulationPlotTypeSelector& selector, std::size_t index);
    /// The look row @p index of @p type starts with: its colour and line style become the
    /// configuration's, and the row shows them (Java: applyTypeAppearance()).
    void applyTypeAppearance(std::size_t index, const FlightDataType& type,
                             SimulationPlotTypeSelector& selector);
    /// Shows the plot's colour on every row without a colour of its own (Java:
    /// refreshDefaultPlotColors()).
    void refreshDefaultPlotColors();
    /// The look of @p type a row starts with (Java: getEffectiveAppearance()).
    [[nodiscard]] std::optional<PlotAppearance> effectiveAppearance(
        const FlightDataType& type) const;
    /// Gives row @p index the colour of @p appearance unless it has one, and its line style
    /// (Java: applyStoredAppearance()).
    void applyStoredAppearance(std::size_t index, const PlotAppearance& appearance);
    /// Changes the simulation's appearance of @p type: its colour to @p color when
    /// @p updateColor, its line style to @p lineStyle when @p updateLineStyle (Java:
    /// updateSimulationAppearance()).
    void updateSimulationAppearance(const FlightDataType* type, const std::optional<Color>& color,
                                    std::optional<LineStyle> lineStyle, bool updateColor,
                                    bool updateLineStyle);
    /// The plot's colours of the rows (Java: computeDefaultPlotColors()); none without data.
    [[nodiscard]] std::vector<QColor> computeDefaultPlotColors() const;

    std::shared_ptr<Simulation>              m_simulation;
    FlightEventTableModel*                   m_eventModel;
    DescriptionArea*                         m_description;
    QLabel*                                  m_eventsLabel;
    QTableView*                              m_eventTable          = nullptr;
    QPushButton*                             m_allButton           = nullptr;
    QPushButton*                             m_noneButton          = nullptr;
    QLabel*                                  m_styleEventMarker    = nullptr;
    QRadioButton*                            m_radioVerticalMarker = nullptr;
    QRadioButton*                            m_radioIcon           = nullptr;
    QWidget*                                 m_eventsPanel;
    std::vector<QColor>                      m_defaultPlotColors;
    std::vector<SimulationPlotTypeSelector*> m_plotTypeSelectors;
};

}  // namespace QtRocket::Gui
