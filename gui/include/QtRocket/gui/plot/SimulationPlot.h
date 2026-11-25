#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>

#include <rocketplot/EventMarker.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/TextAnnotation.h>

#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightEvent.h"

class QEvent;
class QWidget;

namespace QtRocket
{
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;

/// The chart of OpenRocket's simulation plot (swing gui/plot/SimulationPlot.java with its base
/// Plot.java; EventGraphics.java): the core's SimulationPlotModel of a simulation's flight data
/// and a plot configuration, drawn on a DataPlot. The plot dialog (dialogs/plot/, Milestone 4's
/// G4) shows it and switches its points, events, errors and stage (setShowPoints(),
/// setShowEvents(), setShowErrors(), setShowBranch()), as SimulationPlotDialog switches the
/// chart's.
///
/// What it draws, as the model has it (see SimulationPlotModel for the computing, and DataPlot for
/// the chart):
/// - Titled with the simulation's name, the configuration's name below; the x axis labelled
///   "<type> (<unit>)" (getDomainLabel()), each y axis with the names of its rows' types joined by
///   "; " (YAxis::label); the right one only when a series is on it.
/// - The series (getSeries(), axis by axis, row by row, branch by branch), named as OpenRocket's
///   ("Altitude (m)", "Booster stage: Altitude (m)"), a legend group per row (the legend's entry of
///   the row, axis by axis, row by row), in the row's colour (its own, else PlotColors::plotColor()
///   of the colour index the plot panel shows), line style and the point shape of its branch.
/// - The home ranges: the domain's (getDomainRange(); with the event lines drawn,
///   getDomainRangeWithEventLines(), see setShowEvents()) and the y axes' fitted ranges
///   (YAxis::range of an axis with series).
/// - The events, once setShowEvents(true) or setShowBranch() draws them: over time, unless the
///   user chose icons (PlotPreferences::isMarkerStyleIcon(), read at each drawing as Java reads
///   it), a line and a flag at each group of events (getEventGroups() of the stage shown, or of
///   all stages), the flag reading the group's label beside its icon, in the group's look
///   (PlotEventGraphics::markerColor() of EventGroup::lookType); over any other x, and over time
///   with icons, the group's icon on each series of its branch (getIconMarkers(): on the series'
///   axis, with OpenRocket's tool tip).
/// - The abort box (OpenRocket's ErrorAnnotationSet): the model's getAbortText() of the stage
///   shown, or of all stages, at the bottom left of the plot area, framed, in the error colour
///   (ThemeColors ERROR), shown while setShowErrors() says so (shown at first).
/// - The readout (DataPlot): the model's tool tip of the point under the crosshair
///   (SimulationPlotModel::getToolTip()).
///
/// OpenRocket's bugs fixed here (plan section 11.4), each where it is made:
/// - Bug 43: the x axis keeps its label when the event lines widen it (Java replaces the axis by
///   an unlabelled one whenever the last event is within 1 % of its upper bound), and it is
///   widened once, the first time event lines are drawn, never again (Java widens it again at
///   every redraw: each stage chosen, each Show events).
/// - Bug 44: the abort box stays while the events are hidden or drawn again (Java's
///   setShowEvents() clears every annotation of the chart, the abort box with them, so the box
///   is hidden at the dialog's open, which calls it, until Show errors is unchecked and checked
///   or a stage is chosen).
/// The model's fixes of bugs 41, 42, 45, 46 and 48 show here as they are.
///
/// While a stage is chosen, the series of the other stages stay hidden, whatever shows them:
/// rocketplot's legend (U-M4-6: its entry of a row stands for the row's series of every stage,
/// and a click, a double click or its menu shows them all) would show the other stages' curves
/// under the stage chosen; its entry shows and hides the stage's own series (OpenRocket's legend
/// only highlights them). Choosing a stage shows each of its series again, as Java's does.
///
/// Kept as Java: choosing a stage (setShowBranch()) draws that stage's events whether or not
/// Show events is checked, and Show events (setShowEvents(true)) draws every stage's events
/// whatever stage is chosen (SimulationPlotDialog calls the two without regard to each other).
///
/// Deliberate differences (the user's decisions U-M3-6a and U-M4-6, besides DataPlot's): the
/// events' labels are rocketplot's flags, horizontal boxes above the plot area each with its
/// group's icon (FlagPlacement::ABOVE_PLOT_AREA, so that the axes keep their ranges; OpenRocket:
/// the label in the event's colour, turned upright beside the line, in the plot area); their
/// colours follow PlotEventGraphics' dark rule; an icon is drawn against the axis of its series
/// (Java draws it on the left axis's scale, its right-axis height mapped to it, the same place
/// until a zoom moves the axes apart); the abort box has rocketplot's font and background (Java:
/// bold 14 point on white, framed in the error colour; the frame here is rocketplot's, N22).
///
/// The plot holds the model, which shares the flight data: a re-run of the simulation leaves this
/// plot's data alive, never dangling (a new plot shows the new data).
class SimulationPlot : public DataPlot
{
    Q_OBJECT

public:
    /// The position of the abort box in the plot area: its bottom-left corner at 1 % of the plot
    /// area's width and height from its bottom-left corner (XYTitleAnnotation(0.01, 0.01, ...,
    /// RectangleAnchor.BOTTOM_LEFT)).
    static constexpr double kAbortBoxPosition = 0.01;

    /// The plot of @p model, asking its questions through @p services (which must outlive it), its
    /// points marked as @p showPoints says (the preference "Show data points"); no events until
    /// setShowEvents() or setShowBranch() (OpenRocket's plot dialog calls setShowEvents() at
    /// once, with the preference "Show events").
    SimulationPlot(const GuiServices& services, SimulationPlotModel model, bool showPoints,
                   QWidget* parent = nullptr);

    /// The plot of @p simulation's flight data as @p configuration says (SimulationPlot.create():
    /// the model of the simulation's name and data, through translator()), as the constructor
    /// above. A simulation without flight data, or whose data has no branch, is a BugError.
    SimulationPlot(const GuiServices& services, const Simulation& simulation,
                   const SimulationPlotConfiguration& configuration, bool showPoints,
                   QWidget* parent = nullptr);

    /// The GUI's translator of the model's texts: translatedEnumText() of the key and the English
    /// text, as UTF-8 (the names of the types and events as the GUI shows them elsewhere).
    [[nodiscard]] static SimulationPlotModel::Translator translator();

    /// What the plot shows.
    [[nodiscard]] const SimulationPlotModel& model() const noexcept { return m_model; }

    /// Draws the events of every stage, whatever stage is shown, or removes the events
    /// (SimulationPlot.setShowEvents(): drawDomainMarkers(-1), or clearing the markers and the
    /// icons). The first drawing of event lines widens the domain's home range (bug 43).
    void setShowEvents(bool show) override;
    /// Shows or hides the abort box (setShowErrors(), ErrorAnnotationSet.setVisible()).
    void setShowErrors(bool show);
    /// Shows the series of branch @p branch only and draws its events, its abort box in place of
    /// the one shown, or with nullopt every branch's ("All Stages") (setShowBranch(): the series
    /// whose branch it is visible, drawDomainMarkers(branch), ErrorAnnotationSet.setCurrent()).
    /// A branch the data does not have is a BugError.
    void setShowBranch(std::optional<std::size_t> branch);

    /// Whether the abort box is to be shown (setShowErrors(); true at first).
    [[nodiscard]] bool showsErrors() const noexcept { return m_showErrors; }
    /// The branch whose series are shown, nullopt for every branch (setShowBranch()).
    [[nodiscard]] std::optional<std::size_t> shownBranch() const noexcept { return m_branch; }

    /// The event lines drawn (one per group), for the tests.
    [[nodiscard]] std::vector<rocketplot::EventMarker*> eventLines() const;
    /// The event icons drawn, for the tests.
    [[nodiscard]] std::vector<rocketplot::IconAnnotation*> eventIcons() const;
    /// The abort box, null while there is no abort to show (the stage shown has none), for the
    /// tests; hidden while setShowErrors() says so.
    [[nodiscard]] rocketplot::TextAnnotation* abortBox() const noexcept
    {
        return m_abortBox.data();
    }

protected:
    /// The model's tool tip of point @p point of series @p series (Java's tool tip generator).
    [[nodiscard]] QString toolTipText(std::size_t series, std::size_t point) const override;
    /// Re-colours the events and the abort box when the palette changes (their dark rule).
    void changeEvent(QEvent* event) override;

private:
    /// An event line and the type it looks like (for its colour in another palette).
    struct EventLine
    {
        QPointer<rocketplot::EventMarker> marker;
        FlightEvent::Type                 look = FlightEvent::Type::LAUNCH;
    };

    /// Adds the model's series, axis labels and home ranges.
    void build();
    /// Hides the series @p index again when it was shown while another stage is chosen (the
    /// legend shows the series of every branch of its row).
    void keepToTheShownBranch(std::size_t index);
    /// The home ranges, the domain's widened once event lines have been drawn (bug 43).
    void updateHomeRanges();
    /// drawDomainMarkers(@p branch): the event lines or icons of the branch, or of all.
    void drawEvents(std::optional<std::size_t> branch);
    void drawEventLines(std::optional<std::size_t> branch);
    void drawEventIcons(std::optional<std::size_t> branch);
    /// Removes the event lines and icons.
    void clearEvents();
    /// The abort box of the branch shown (ErrorAnnotationSet.setCurrent()).
    void updateAbortBox();
    void recolor();

    SimulationPlotModel                               m_model;
    std::vector<EventLine>                            m_eventLines;
    std::vector<QPointer<rocketplot::IconAnnotation>> m_eventIcons;
    QPointer<rocketplot::TextAnnotation>              m_abortBox;
    std::optional<std::size_t>                        m_branch;
    bool                                              m_showErrors    = true;
    bool                                              m_domainWidened = false;
};

}  // namespace QtRocket::Gui
