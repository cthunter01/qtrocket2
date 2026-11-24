#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket
{

class FlightData;
class FlightDataType;
class Unit;

/// What OpenRocket's simulation plot shows, without the chart (swing gui/plot/Plot and
/// SimulationPlot, their data parts): for the flight data of a simulation and a plot
/// configuration, the axes with their labels and ranges, the series of every row and branch in
/// the units of the configuration, the legend, the flight events in their groups, the icons that
/// mark them on the curves, the aborts and the tool tips. The GUI's plot (gui/plot/) draws this
/// with rocketplot; the tests compare it with OpenRocket's own plots without Qt.
///
/// What is computed, at construction, as Plot's and SimulationPlot's constructors do:
/// - The filled configuration (getFilledConfiguration()): the configuration's fillAutoAxes() on
///   the first branch, then fitAxes() over every branch (PlotConfiguration).
/// - The series (getSeries()), axis by axis (the left one first), row by row on it, branch by
///   branch: a series for every row and every branch with points, x the domain type in the domain
///   unit and y the row's type in the row's unit, named "<label>" on the first branch and
///   "<branch name>: <label>" on the others, the label being "<type name> (<unit>)" (the unit
///   left out for a dimensionless one, getLabel()). A row of the type "Position upwind" (a type
///   that no longer exists) plots the position north of launch, as Java's postProcessType().
/// - The axes: the domain labelled as a series is and ranging over the series' x values, 0.5
///   added on both sides (JFreeChart's interval width of 1 for an XYSeriesCollection), and with
///   the event lines widened once (getDomainRangeWithEventLines()); each y axis labelled with the
///   names of its rows' types joined by "; " and ranging over the filled configuration's range.
/// - The legend (getLegend()): an entry per row that has a series, axis by axis, row by row.
/// - The events: every event of every branch of a type the configuration marks (ALTITUDE never),
///   sorted by time (stably: at the same time the first branch's first), and their groups
///   (getEventGroups()): an event within 0.05 s of a group's first event joins it unless the group
///   has its type; a group looks like its event of the highest ordinal (its colour and icon).
/// - The icons (getIconMarkers()): every group of a branch on every series of that branch, at the
///   group's time, interpolated linearly (LinearInterpolator) over the branch's time.
/// - The aborts (getAbortLines(), getAbortText()): the first SIM_ABORT of each branch.
/// - The tool tips (getToolTip(), EventIcon::toolTip): Java's HTML, its numbers formatted by
///   DecimalFormatter::format(value, 2, false), the series named with the stages still attached
///   to it at the point (getSeriesNameAt()).
/// Everything is + - * / on the stored data, as in Java (the ranges, the icons' positions, the
/// event line widening), so the values are Java's to the last bit; the tool tips' digits count
/// on log10() and pow() (DecimalFormatter).
///
/// Texts: the names OpenRocket translates (the types', the events', the configuration's, the
/// abort annotation's words) are English unless a Translator is given, which the model asks for
/// each with its key of messages.properties: a type's getDisplayKey() (a type without one, a
/// custom type, keeps its name), an event type's displayKey(), the configuration's getNameKey(),
/// "simulationplot.abort.title", ".stage", ".time" and ".cause". The cause of an abort is its
/// English text (SimulationAbort::messageDescription()), asked with an empty key, which a
/// Translator is not asked for. The branch names and the warnings are the core's texts.
///
/// Lifetime: the model shares the flight data (a re-run of the simulation replaces the
/// simulation's data and leaves this one alive) and copies the configuration; it changes neither.
/// It is a value object: built on the thread that owns the data's simulation, read anywhere.
///
/// Deviations from OpenRocket (bugs 41, 42, 43, 46 and 48 of the plan's section 11.4 are fixed):
/// - Bug 41: Java keeps a group's events in a HashSet (identity hashes), so the time its line or
///   icon is drawn at (the first event of the set) and the order of its label's names change from
///   run to run. Here a group has its events in the order of the sorted events, and is drawn at
///   its first event's time.
/// - Bug 42: Java's renderer looks the paint of the series at index series / branch count up, so
///   with several branches every row of an axis is drawn in the colour of its first row, while
///   the plot panel shows each row's own colour. Here each series has its row's colour, the one
///   the panel shows (computeDefaultColorIndices()).
/// - Bug 43: Java replaces the domain axis by an unlabelled one when it widens it for the event
///   lines (whenever the last event is within 1 % of the upper bound, every flight longer than
///   49.5 s), and widens it again at every redraw. Here the domain keeps its label, and
///   getDomainRangeWithEventLines() is the range widened once.
/// - Bug 46: a branch with points that has no column of a row's type, or of the domain's type,
///   makes Java's plot fail (NullPointerException); here that branch has no series for the row
///   (or no series at all), its icons are left out with them, and the ranges are those of the
///   branches that have the type (PlotConfiguration's bug 46).
/// - Bug 48: Java steps through each axis's series by the branch count to find the rows' legend
///   entries and colours and the branches' point shapes and stages, which a branch without points
///   (it has no series) misaligns. Here a series knows its row and branch, the colours and the
///   legend go by row, and a legend entry is named by its row's label.
/// - The domain ranges over every series (Java: over the last axis's series, which are the same
///   points but where bug 46 leaves some out).
/// - The sample whose stages name an icon's tool tip is the first one nearest to the group's time
///   (Java: the first sample of the value of the nearest one, which is the same sample).
/// - A branch without a time column has no icons (Java's plotIconMarkers() fails there: its
///   LinearInterpolator gets no times, NullPointerException).
/// Kept as Java: the readout tool tips of a branch without a time column leave the time line out
/// (getToolTip()); a tool tip's time is the stored value in seconds, shown with the name of the
/// time's default unit (a time unit other than s, which the user can choose, shows the seconds
/// under its name); the multi-stage names compare the point's index with the index of a later
/// branch's separation in that later branch's data.
class SimulationPlotModel
{
public:
    /// Translates the text of OpenRocket's key @p key whose English text is @p englishText (see
    /// the class comment); never asked for an empty key.
    using Translator =
        std::function<std::string(std::string_view key, std::string_view englishText)>;

    /// The range an axis shows: NaN for an axis without data.
    struct Range
    {
        double min{std::numeric_limits<double>::quiet_NaN()};
        double max{std::numeric_limits<double>::quiet_NaN()};
    };

    /// A y axis (0 the left one, 1 the right one).
    struct YAxis
    {
        /// The names of the types of its rows joined by "; ", without units (Java's axisLabel).
        std::string label;
        /// The filled configuration's range of the axis, over every branch.
        Range range;
        /// Whether a series is on the axis; Java shows an axis only then.
        bool hasSeries{false};
    };

    /// One curve: a row of the configuration on one branch (Java: a MetadataXYSeries).
    struct Series
    {
        /// The row of the configuration (Java: getDataIdx()).
        std::size_t row{0};
        /// The branch of the flight data (Java: getBranchIdx()); the plot's point shape and the
        /// stage it is shown for follow it.
        std::size_t branch{0};
        /// The y axis, 0 or 1.
        int axis{0};
        /// "<type name> (<unit>)", or the type's name alone for a dimensionless unit (Java:
        /// getBaseName(), getLabel()); the legend entry of the row.
        std::string label;
        /// The label on the first branch, "<branch name>: <label>" on the others (Java:
        /// getDescription()).
        std::string name;
        /// The symbol of the row's unit (Java: getUnit()).
        std::string unit;
        /// The domain's values in the domain unit and the row's values in the row's unit, point by
        /// point (a NaN is kept).
        std::vector<double> x;
        std::vector<double> y;
        /// The row's colour: color when set, else the plot colour of colorIndex (the GUI's
        /// PlotColors::plotColor(), Java's Util.getPlotColor()).
        std::optional<Color> color;
        int                  colorIndex{0};
        /// The row's line style.
        LineStyle lineStyle{LineStyle::SOLID};
    };

    /// One entry of the legend: a row, which stands for its series on every branch.
    struct LegendEntry
    {
        std::size_t row{0};
        int         axis{0};
        /// The row's label (Series::label).
        std::string          label;
        std::optional<Color> color;
        int                  colorIndex{0};
        LineStyle            lineStyle{LineStyle::SOLID};
    };

    /// An event of a branch (Java: SimulationPlot.EventDisplayInfo).
    struct PlotEvent
    {
        /// The branch whose events have it.
        std::size_t branch{0};
        FlightEvent event;
    };

    /// Events close together, marked by one line or icon (Java: fillEventLists()).
    struct EventGroup
    {
        /// The time of the first event, in s: where the group is drawn (bug 41).
        double time{0};
        /// The events, in time order (bug 41), of different types.
        std::vector<PlotEvent> events;
        /// The type of the event the group looks like: its type of the highest ordinal (Java:
        /// EventGraphics.getEventColor() and getEventImage() of that event).
        FlightEvent::Type lookType{FlightEvent::Type::LAUNCH};
        /// For a SIM_WARN look, its warning's priority (the icon: warning_low, _normal or _high);
        /// nullopt otherwise.
        std::optional<MessagePriority> lookWarningPriority;
        /// The names of the events' types joined by ", " (Java: constructEventLabels()).
        std::string label;
    };

    /// A line of a tool tip: "<name>: <value> <unit>".
    struct ToolTipValue
    {
        std::string name;
        /// DecimalFormatter::format(value, 2, false).
        std::string value;
        std::string unit;
    };

    /// A tool tip (Java: Plot.formatTooltip()).
    struct ToolTip
    {
        /// The series' name at the point (getSeriesNameAt()).
        std::string name;
        /// Message::toString() of the warning of each SIM_WARN event of an icon's group.
        std::vector<std::string> warnings;
        /// The names of the types of the other events of an icon's group.
        std::vector<std::string> events;
        /// The y value; nullopt when it is NaN.
        std::optional<ToolTipValue> y;
        /// The x value.
        ToolTipValue x;
        /// The time, when the domain is not the time and the branch has times (see the class
        /// comment for its unit).
        std::optional<ToolTipValue> time;

        /// Java's HTML: "<html><b><i>name</i></b><br>", then "<b><i>Warning:  text</b></i><br>"
        /// for each warning, the events' names joined by ", " and "<br>", the lines
        /// "name: value unit<br>" of y, x and the time, and "</html>".
        [[nodiscard]] std::string toHtml() const;
    };

    /// An icon marking a group of events on a series (Java: plotIconMarkers()'s
    /// XYImageAnnotation).
    struct EventIcon
    {
        /// The series in getSeries(), and so the branch, the row and the axis.
        std::size_t series{0};
        /// The group in getEventGroups() of the series' branch.
        std::size_t group{0};
        /// The group's time, in s.
        double time{0};
        /// The domain's and the row's values at the time, in their units.
        double x{0};
        double y{0};
        /// y mapped from the series' axis onto the left axis's range (Java draws every icon in
        /// the left axis's space): y itself on the left axis.
        double yOnLeftAxis{0};
        /// The group's look (EventGroup::lookType, lookWarningPriority).
        FlightEvent::Type              lookType{FlightEvent::Type::LAUNCH};
        std::optional<MessagePriority> lookWarningPriority;
        ToolTip                        toolTip;
    };

    /// The abort of a branch (Java: ErrorAnnotationSet's line of it).
    struct AbortLine
    {
        std::size_t branch{0};
        /// The branch's name, as stored.
        std::string branchName;
        /// The time of its first SIM_ABORT, in s.
        double                 time{0};
        SimulationAbort::Cause cause{SimulationAbort::Cause::NO_ACTIVE_STAGES};
    };

    /// The plot of @p data, the flight data of the simulation named @p simulationName, as
    /// @p configuration describes it (see the class comment).
    /// @throws BugError when @p data is null or has no branch (Java: IndexOutOfBoundsException),
    ///         or as PlotConfiguration::fitAxes()
    SimulationPlotModel(std::string simulationName, std::shared_ptr<const FlightData> data,
                        SimulationPlotConfiguration configuration, Translator translator = {});

    SimulationPlotModel(const SimulationPlotModel&)                = delete;
    SimulationPlotModel& operator=(const SimulationPlotModel&)     = delete;
    SimulationPlotModel(SimulationPlotModel&&) noexcept            = default;
    SimulationPlotModel& operator=(SimulationPlotModel&&) noexcept = default;
    ~SimulationPlotModel()                                         = default;

    //// What is plotted

    [[nodiscard]] const FlightData& getData() const noexcept { return *m_data; }
    /// The configuration as given.
    [[nodiscard]] const SimulationPlotConfiguration& getConfiguration() const noexcept
    {
        return m_configuration;
    }
    /// The configuration with its automatic axes chosen and its ranges fitted to every branch
    /// (Java: Plot.filledConfig).
    [[nodiscard]] const SimulationPlotConfiguration::Base& getFilledConfiguration() const noexcept
    {
        return *m_filled;
    }
    /// The number of branches of the data, those without points included.
    [[nodiscard]] std::size_t getBranchCount() const noexcept { return m_branchCount; }
    /// The labels of the branches (PlotUtil::generateSeriesLabels()), the plot dialog's stages
    /// after "All Stages".
    [[nodiscard]] std::vector<std::string> getSeriesLabels() const;

    //// Titles and axes

    /// The simulation's name (the chart's title).
    [[nodiscard]] const std::string& getTitle() const noexcept { return m_title; }
    /// The configuration's name without HTML tags (the chart's subtitle).
    [[nodiscard]] const std::string& getSubtitle() const noexcept { return m_subtitle; }

    /// "<domain type name> (<unit>)", without the unit for a dimensionless one.
    [[nodiscard]] const std::string& getDomainLabel() const noexcept { return m_domainLabel; }
    /// The series' x values from the smallest to the largest, 0.5 added on both sides; NaN
    /// without series.
    [[nodiscard]] const Range& getDomainRange() const noexcept { return m_domainRange; }
    /// Whether the domain is the time: Java marks the events of a plot over time with lines
    /// (getEventGroups()), unless the user chose icons (PlotPreferences::isMarkerStyleIcon()), and
    /// those of any other plot with icons (getIconMarkers()).
    [[nodiscard]] bool hasTimeDomain() const noexcept { return m_timeDomain; }
    /// The domain range with room for the event lines of every branch (Java:
    /// plotVerticalLineMarkers()): with 1 % of the upper bound as w, each group's time t, in time
    /// order, that is above the upper bound minus w moves the upper bound to t + w. The domain
    /// range itself when the domain is not the time (no line is drawn).
    [[nodiscard]] const Range& getDomainRangeWithEventLines() const noexcept
    {
        return m_domainRangeWithEventLines;
    }

    /// The left (0) and the right (1) y axis.
    [[nodiscard]] const std::array<YAxis, 2>& getYAxes() const noexcept { return m_yAxes; }

    //// Series and legend

    /// The series, axis by axis, row by row, branch by branch (see the class comment).
    [[nodiscard]] const std::vector<Series>& getSeries() const noexcept { return m_series; }
    /// The legend's entries, axis by axis, row by row.
    [[nodiscard]] const std::vector<LegendEntry>& getLegend() const noexcept { return m_legend; }

    /// The name of series @p series at its point @p point (Java: getNameBasedOnIdxAndSeries()):
    /// its name on the last branch and with a single branch; otherwise "<its branch's name>",
    /// " + <name>" for each later branch whose separation is at or after index @p point of its own
    /// data (the stages still attached), ": " and the series' label.
    /// @throws BugError when there is no series @p series
    [[nodiscard]] std::string getSeriesNameAt(std::size_t series, std::size_t point) const;

    /// The tool tip of point @p point of series @p series (Java: the tool tip generator of the
    /// series' axis): no events, the series' y and x, and the time of the branch at the point.
    /// @throws BugError when there is no such series or point
    [[nodiscard]] ToolTip getToolTip(std::size_t series, std::size_t point) const;

    //// Events

    /// The events the configuration marks, in time order (Java: eventList).
    [[nodiscard]] const std::vector<PlotEvent>& getEvents() const noexcept { return m_events; }

    /// The event groups of branch @p branch, or of every branch (Java: fillEventLists()): the
    /// plot's event lines for that stage, or for "All Stages".
    /// @throws BugError when there is no branch @p branch
    [[nodiscard]] const std::vector<EventGroup>& getEventGroups(
        std::optional<std::size_t> branch = std::nullopt) const;

    /// The icons of branch @p branch (the groups of that branch on its series, axis by axis,
    /// series by series, group by group), or of every branch, the last branch first (so that
    /// the earlier branches' icons are drawn above), as Java's drawDomainMarkers() places them. A
    /// group whose x or y is NaN on a series has no icon there.
    /// @throws BugError when there is no branch @p branch
    [[nodiscard]] std::vector<EventIcon> getIconMarkers(
        std::optional<std::size_t> branch = std::nullopt) const;

    //// Aborts

    /// The aborts of branch @p branch, or of every branch in order.
    /// @throws BugError when there is no branch @p branch
    [[nodiscard]] std::vector<AbortLine> getAbortLines(
        std::optional<std::size_t> branch = std::nullopt) const;

    /// The abort annotation of branch @p branch or of every branch (Java: ErrorAnnotationSet's
    /// text): "Simulation Abort", then for each abort a line "Stage: <name>; Time: <time with two
    /// decimals> s; Cause: <cause>", the lines joined by '\n'; empty without an abort.
    /// @throws BugError when there is no branch @p branch
    [[nodiscard]] std::string getAbortText(std::optional<std::size_t> branch = std::nullopt) const;

    //// The plot panel's colours

    /// The colour index the plot panel shows for each row of @p configuration without a colour of
    /// its own over @p data (Java: SimulationPlotPanel.computeDefaultPlotColors()): with the axes
    /// of fillAutoAxes() on the first branch, the rows of the left axis get 0, 1, ... in order and
    /// those of the right axis follow the left rows' series, (number of left rows) x (number of
    /// branches) + 0, 1, ... Without a branch, row i gets i (the panel's fallback). The model's
    /// series and legend take these (bugs 42 and 48). The indices are not wrapped to the palette:
    /// the colour of index i is the palette's i modulo its 10 colours (the GUI's
    /// PlotColors::plotColor(), Java's Util.getPlotColor()).
    [[nodiscard]] static std::vector<int> computeDefaultColorIndices(
        const SimulationPlotConfiguration& configuration, const FlightData& data);

private:
    [[nodiscard]] std::string translate(std::string_view key, std::string_view englishText) const;
    [[nodiscard]] std::string typeName(const FlightDataType& type) const;
    [[nodiscard]] std::string eventName(FlightEvent::Type type) const;
    /// Plot.getLabel().
    [[nodiscard]] std::string label(const FlightDataType& type, const Unit& unit) const;
    /// Plot.formatTooltip().
    [[nodiscard]] ToolTip formatToolTip(std::string name, double time, const ToolTipValue& x,
                                        const std::string& yName, double y,
                                        const std::string&            yUnit,
                                        const std::vector<PlotEvent>* events) const;

    void                                  buildSeries();
    void                                  buildAxes();
    void                                  buildEvents();
    [[nodiscard]] std::vector<EventGroup> groupEvents(std::optional<std::size_t> branch) const;
    /// The group of @p events (not empty) looking like @p look (not null, one of them).
    [[nodiscard]] EventGroup makeGroup(std::vector<PlotEvent> events,
                                       const FlightEvent*     look) const;
    void                     buildIcons();
    void buildBranchIcons(std::size_t branch, std::vector<EventIcon>& icons) const;
    void checkBranch(std::optional<std::size_t> branch) const;

    std::string                                        m_title;
    std::shared_ptr<const FlightData>                  m_data;
    SimulationPlotConfiguration                        m_configuration;
    Translator                                         m_translator;
    std::unique_ptr<SimulationPlotConfiguration::Base> m_filled;
    std::size_t                                        m_branchCount{0};
    std::string                                        m_subtitle;
    std::string                                        m_domainLabel;
    Range                                              m_domainRange;
    Range                                              m_domainRangeWithEventLines;
    bool                                               m_timeDomain{false};
    std::array<YAxis, 2>                               m_yAxes{};
    std::vector<Series>                                m_series;
    std::vector<LegendEntry>                           m_legend;
    std::vector<PlotEvent>                             m_events;
    std::vector<EventGroup>                            m_allGroups;
    std::vector<std::vector<EventGroup>>               m_branchGroups;
    std::vector<std::vector<EventIcon>>                m_branchIcons;
    /// The index of each branch's separation in its own data (Java: getDataIndexOfTime() of
    /// getSeparationTime()).
    std::vector<std::optional<std::size_t>> m_separationIndices;
};

}  // namespace QtRocket
