#include "QtRocket/plot/SimulationPlotModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/plot/PlotAxisBounds.h"
#include "QtRocket/plot/PlotUtil.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/DecimalFormatter.h"
#include "QtRocket/util/LinearInterpolator.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Events within this many seconds of a group's first event join it (fillEventLists()).
constexpr double kEventGroupSpan = 0.05;
/// Half the interval width JFreeChart's XYSeriesCollection adds around the domain's values
/// (IntervalXYDelegate: a fixed width of 1, centred).
constexpr double kDomainIntervalHalfWidth = 0.5;
/// The room the event lines get at the domain's upper end, as a fraction of the upper bound
/// (plotVerticalLineMarkers()'s markerWidth).
constexpr double kEventLineMargin = 0.01;
/// The decimals of the numbers of a tool tip (formatTooltip()).
constexpr int kToolTipDecimals = 2;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] const FlightDataType& timeType()
{
    return FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
}

/// SimulationPlot.postProcessType(): "Position upwind" is plotted as the position north of
/// launch. (The types live as long as the process.)
[[nodiscard]] const FlightDataType* postProcessType(const FlightDataType* type)
{
    if (type->getName() == FlightDataType::kLegacyUpwindName)
    {
        return &FlightDataType::builtin(FlightDataTypeId::TYPE_POSITION_Y);
    }
    return type;
}

/// Whether Plot.getLabel() leaves the unit out: a unit of UNITS_NONE or UNITS_COEFFICIENT, or
/// one without a symbol.
[[nodiscard]] bool hasNoLabelUnit(const Unit& unit)
{
    return unitGroup(UnitGroupId::NONE).contains(unit) ||
           unitGroup(UnitGroupId::COEFFICIENT).contains(unit) || unit.getUnit().empty();
}

/// The colour index of each row of @p filled (computeDefaultPlotColors()).
[[nodiscard]] std::vector<int> colorIndices(const SimulationPlotConfiguration::Base& filled,
                                            std::size_t                              branchCount)
{
    const int        safeBranchCount = static_cast<int>(std::max<std::size_t>(branchCount, 1));
    std::vector<int> colors(filled.getDataCount(), 0);
    int              cumulativeSeriesCount = 0;
    for (int axis = 0; axis < SimulationPlotConfiguration::kAxesCount; ++axis)
    {
        int relativeIndex = 0;
        for (std::size_t row = 0; row < filled.getDataCount(); ++row)
        {
            int rowAxis = filled.getAxis(row);
            if (rowAxis < 0 || rowAxis >= SimulationPlotConfiguration::kAxesCount)
            {
                rowAxis = 0;
            }
            if (rowAxis == axis)
            {
                colors[row] = cumulativeSeriesCount + relativeIndex;
                ++relativeIndex;
            }
        }
        cumulativeSeriesCount += relativeIndex * safeBranchCount;
    }
    return colors;
}

/// The warning of the SIM_WARN event @p event as the plot shows it: the one of the warning set
/// (Java's event and set share the object), else the event's own.
[[nodiscard]] const Warning* eventWarning(const FlightData& data, const FlightEvent& event)
{
    const Warning* warning = data.findWarning(event);
    return warning != nullptr ? warning : event.getWarning().get();
}

/// The index of the first time of @p time nearest to @p t (Java: the stream minimum by the
/// distance, a NaN distance the largest); @p time is not empty.
[[nodiscard]] std::size_t nearestSample(const std::vector<double>& time, double t)
{
    std::size_t best         = 0;
    double      bestDistance = std::abs(time.front() - t);
    for (std::size_t i = 1; i < time.size(); ++i)
    {
        const double distance = std::abs(time[i] - t);
        if ((std::isnan(bestDistance) && !std::isnan(distance)) || distance < bestDistance)
        {
            best         = i;
            bestDistance = distance;
        }
    }
    return best;
}

[[nodiscard]] std::string toolTipNumber(double value)
{
    return DecimalFormatter::format(value, kToolTipDecimals, false);
}

}  // namespace

std::string SimulationPlotModel::ToolTip::toHtml() const
{
    std::string html = "<html>";
    html += std::format("<b><i>{}</i></b><br>", name);
    for (const std::string& warning : warnings)
    {
        html += "<b><i>Warning:  " + warning + "</b></i><br>";
    }
    if (!events.empty())
    {
        html += Strings::join(", ", events) + "<br>";
    }
    for (const std::optional<ToolTipValue>& line : {y, std::optional<ToolTipValue>(x), time})
    {
        if (line.has_value())
        {
            html += std::format("{}: {} {}<br>", line->name, line->value, line->unit);
        }
    }
    html += "</html>";
    return html;
}

SimulationPlotModel::SimulationPlotModel(std::string                       simulationName,
                                         std::shared_ptr<const FlightData> data,
                                         SimulationPlotConfiguration       configuration,
                                         Translator                        translator)
  : m_title(std::move(simulationName)),
    m_data(std::move(data)),
    m_configuration(std::move(configuration)),
    m_translator(std::move(translator))
{
    if (m_data == nullptr)
    {
        bug("A simulation plot without flight data");
    }
    m_branchCount = m_data->getBranchCount();
    if (m_branchCount == 0)
    {
        bug("A simulation plot of flight data without a branch");
    }

    // Fill the auto-selections based on first branch selected.
    m_filled = m_configuration.fillAutoAxes(m_data->getBranch(0));

    // Compute the axes based on the min and max value of all branches
    std::vector<const FlightDataBranch*> allBranches;
    allBranches.reserve(m_branchCount);
    for (const std::shared_ptr<FlightDataBranch>& branch : m_data->getBranches())
    {
        allBranches.push_back(branch.get());
    }
    m_filled->fitAxes(allBranches);

    m_subtitle = PlotUtil::formatHtmlString(
        translate(m_configuration.getNameKey(), m_configuration.getName()));
    m_timeDomain = &m_configuration.getDomainAxisType() == &timeType();
    m_separationIndices.reserve(m_branchCount);
    for (const FlightDataBranch* branch : allBranches)
    {
        m_separationIndices.push_back(branch->getDataIndexOfTime(branch->getSeparationTime()));
    }

    buildSeries();
    buildAxes();
    buildEvents();
    buildIcons();
}

std::vector<std::string> SimulationPlotModel::getSeriesLabels() const
{
    return PlotUtil::generateSeriesLabels(*m_data);
}

std::string SimulationPlotModel::translate(std::string_view key, std::string_view englishText) const
{
    if (key.empty() || !m_translator)
    {
        return std::string(englishText);
    }
    return m_translator(key, englishText);
}

std::string SimulationPlotModel::typeName(const FlightDataType& type) const
{
    return translate(type.getDisplayKey(), type.getName());
}

std::string SimulationPlotModel::eventName(FlightEvent::Type type) const
{
    return translate(displayKey(type), displayName(type));
}

std::string SimulationPlotModel::label(const FlightDataType& type, const Unit& unit) const
{
    std::string name = PlotUtil::formatHtmlString(typeName(type));
    if (!hasNoLabelUnit(unit))
    {
        name += " (" + unit.getUnit() + ")";
    }
    return name;
}

void SimulationPlotModel::buildSeries()
{
    const SimulationPlotConfiguration::Base& filled     = *m_filled;
    const FlightDataType&                    domainType = filled.getDomainAxisType();
    const Unit&                              domainUnit = filled.getDomainAxisUnit();
    const std::vector<int>                   colors     = colorIndices(filled, m_branchCount);

    for (int axis = 0; axis < SimulationPlotConfiguration::kAxesCount; ++axis)
    {
        for (std::size_t row = 0; row < filled.getDataCount(); ++row)
        {
            if (filled.getAxis(row) != axis)
            {
                continue;
            }
            const FlightDataType& type     = *postProcessType(&filled.getType(row));
            const Unit&           unit     = filled.getUnit(row);
            const std::string     rowLabel = label(type, unit);
            bool                  plotted  = false;

            // Populate data for each branch.
            for (std::size_t branchIndex = 0; branchIndex < m_branchCount; ++branchIndex)
            {
                const FlightDataBranch& branch = m_data->getBranch(branchIndex);
                // Ignore empty branches, and (bug 46) a branch without the types.
                const std::vector<double>* plotx = branch.getView(domainType);
                const std::vector<double>* ploty = branch.getView(type);
                if (branch.getLength() == 0 || plotx == nullptr || ploty == nullptr)
                {
                    continue;
                }
                Series series{
                    .row        = row,
                    .branch     = branchIndex,
                    .axis       = axis,
                    .label      = rowLabel,
                    .name       = branchIndex == 0 ? rowLabel : branch.getName() + ": " + rowLabel,
                    .unit       = unit.getUnit(),
                    .x          = {},
                    .y          = {},
                    .color      = m_configuration.getPlotDataColor(row),
                    .colorIndex = colors[row],
                    .lineStyle  = m_configuration.getPlotDataLineStyle(row)};
                series.x.reserve(plotx->size());
                series.y.reserve(plotx->size());
                for (std::size_t j = 0; j < plotx->size(); ++j)
                {
                    series.x.push_back(domainUnit.toUnit((*plotx)[j]));
                    series.y.push_back(unit.toUnit((*ploty)[j]));
                }
                m_series.push_back(std::move(series));
                plotted = true;
            }

            if (plotted)
            {
                m_legend.push_back(
                    LegendEntry{.row        = row,
                                .axis       = axis,
                                .label      = rowLabel,
                                .color      = m_configuration.getPlotDataColor(row),
                                .colorIndex = colors[row],
                                .lineStyle  = m_configuration.getPlotDataLineStyle(row)});
            }
        }
    }
}

void SimulationPlotModel::buildAxes()
{
    const SimulationPlotConfiguration::Base& filled = *m_filled;

    // The axis labels: the names of the rows' types, each axis's joined by "; ".
    std::array<bool, 2> labelled{};
    for (std::size_t row = 0; row < filled.getDataCount(); ++row)
    {
        const auto        axis = static_cast<std::size_t>(filled.getAxis(row));
        const std::string name =
            PlotUtil::formatHtmlString(typeName(*postProcessType(&filled.getType(row))));
        std::string& axisLabel = m_yAxes.at(axis).label;
        if (labelled.at(axis))
        {
            axisLabel += "; ";
        }
        axisLabel += name;
        labelled.at(axis) = true;
    }
    for (std::size_t axis = 0; axis < m_yAxes.size(); ++axis)
    {
        const PlotAxisBounds& bounds = filled.getAllAxes().at(axis);
        m_yAxes.at(axis).range = Range{.min = bounds.getMinValue(), .max = bounds.getMaxValue()};
        m_yAxes.at(axis).hasSeries = std::ranges::any_of(
            m_series, [axis](const Series& series) { return std::cmp_equal(series.axis, axis); });
    }

    m_domainLabel = label(filled.getDomainAxisType(), filled.getDomainAxisUnit());

    double lower = std::numeric_limits<double>::infinity();
    double upper = -std::numeric_limits<double>::infinity();
    for (const Series& series : m_series)
    {
        for (const double x : series.x)
        {
            if (!std::isnan(x))
            {
                lower = MathUtil::javaMin(lower, x);
                upper = MathUtil::javaMax(upper, x);
            }
        }
    }
    if (lower <= upper)
    {
        m_domainRange =
            Range{.min = lower - kDomainIntervalHalfWidth, .max = upper + kDomainIntervalHalfWidth};
    }
}

void SimulationPlotModel::buildEvents()
{
    // Create list of events to show (combine event too close to each other)
    for (std::size_t branch = 0; branch < m_branchCount; ++branch)
    {
        for (const FlightEvent& event : m_data->getBranch(branch).getEvents())
        {
            const FlightEvent::Type type = event.getType();
            if (type != FlightEvent::Type::ALTITUDE && m_configuration.isEventActive(type))
            {
                m_events.push_back(PlotEvent{.branch = branch, .event = event});
            }
        }
    }
    std::ranges::stable_sort(m_events, [](const PlotEvent& o1, const PlotEvent& o2) {
        return o1.event.getTime() < o2.event.getTime();
    });

    m_allGroups = groupEvents(std::nullopt);
    m_branchGroups.reserve(m_branchCount);
    for (std::size_t branch = 0; branch < m_branchCount; ++branch)
    {
        m_branchGroups.push_back(groupEvents(branch));
    }

    // plotVerticalLineMarkers()'s room for the lines of every branch, once (bug 43).
    m_domainRangeWithEventLines = m_domainRange;
    if (m_timeDomain)
    {
        const double markerWidth = kEventLineMargin * m_domainRangeWithEventLines.max;
        for (const EventGroup& group : m_allGroups)
        {
            const double t = group.time;
            if (t > m_domainRangeWithEventLines.max - markerWidth)
            {
                m_domainRangeWithEventLines.max = t + markerWidth;
            }
        }
    }
}

SimulationPlotModel::EventGroup SimulationPlotModel::makeGroup(std::vector<PlotEvent> events,
                                                               const FlightEvent*     look) const
{
    QTROCKET_ASSERT(!events.empty() && look != nullptr);
    EventGroup group{.time                = events.front().event.getTime(),
                     .events              = std::move(events),
                     .lookType            = look->getType(),
                     .lookWarningPriority = std::nullopt,
                     .label               = {}};
    if (group.lookType == FlightEvent::Type::SIM_WARN)
    {
        const Warning* warning = eventWarning(*m_data, *look);
        if (warning != nullptr)
        {
            group.lookWarningPriority = warning->priority();
        }
    }
    for (const PlotEvent& info : group.events)
    {
        if (!group.label.empty())
        {
            group.label += ", ";
        }
        group.label += eventName(info.event.getType());
    }
    return group;
}

std::vector<SimulationPlotModel::EventGroup> SimulationPlotModel::groupEvents(
    std::optional<std::size_t> branch) const
{
    std::vector<EventGroup> groups;
    std::vector<PlotEvent>  events;
    const FlightEvent*      look       = nullptr;
    double                  prevTime   = -100;
    int                     maxOrdinal = -1;

    for (const PlotEvent& info : m_events)
    {
        if (branch.has_value() && *branch != info.branch)
        {
            continue;
        }

        const double            t    = info.event.getTime();
        const FlightEvent::Type type = info.event.getType();

        if (std::abs(t - prevTime) <= kEventGroupSpan)
        {
            const bool typeKnown = std::ranges::any_of(
                events, [type](const PlotEvent& other) { return other.event.getType() == type; });
            if (!typeKnown)
            {
                if (ordinal(type) > maxOrdinal)
                {
                    look       = &info.event;
                    maxOrdinal = ordinal(type);
                }
                events.push_back(info);
            }
        }
        else
        {
            if (!events.empty())
            {
                groups.push_back(makeGroup(std::exchange(events, {}), look));
            }
            prevTime = t;
            look     = &info.event;
            events.push_back(info);
            maxOrdinal = ordinal(type);
        }
    }
    if (!events.empty())
    {
        groups.push_back(makeGroup(std::move(events), look));
    }
    return groups;
}

void SimulationPlotModel::buildIcons()
{
    m_branchIcons.resize(m_branchCount);
    for (std::size_t branch = 0; branch < m_branchCount; ++branch)
    {
        buildBranchIcons(branch, m_branchIcons[branch]);
    }
}

void SimulationPlotModel::buildBranchIcons(std::size_t             branchIndex,
                                           std::vector<EventIcon>& icons) const
{
    const FlightDataBranch&    dataBranch = m_data->getBranch(branchIndex);
    const std::vector<double>* time       = dataBranch.getView(timeType());
    const std::vector<double>* domain     = dataBranch.getView(m_configuration.getDomainAxisType());
    if (time == nullptr || domain == nullptr || time->empty())
    {
        return;
    }
    const LinearInterpolator domainInterpolator(*time, *domain);
    const std::string        xName = typeName(m_configuration.getDomainAxisType());
    const std::string        unitX = m_configuration.getDomainAxisUnit().getUnit();

    const std::array<PlotAxisBounds, 2>& minMaxAxes = m_filled->getAllAxes();
    const std::vector<EventGroup>&       eventSets  = m_branchGroups[branchIndex];

    for (int axisno = 0; axisno < SimulationPlotConfiguration::kAxesCount; ++axisno)
    {
        // Image annotations are drawn using the data space defined by the left axis, so the
        // position of annotations on the right axis need to be mapped to the left axis
        // dataspace.
        const double minLeft = minMaxAxes[0].getMinValue();
        const double maxLeft = minMaxAxes[0].getMaxValue();

        const double minThis = minMaxAxes.at(static_cast<std::size_t>(axisno)).getMinValue();
        const double maxThis = minMaxAxes.at(static_cast<std::size_t>(axisno)).getMaxValue();

        const double slope     = (maxLeft - minLeft) / (maxThis - minThis);
        const double intercept = ((maxThis * minLeft) - (maxLeft * minThis)) / (maxThis - minThis);

        for (std::size_t seriesIndex = 0; seriesIndex < m_series.size(); ++seriesIndex)
        {
            const Series& series = m_series[seriesIndex];
            if (series.axis != axisno || series.branch != branchIndex)
            {
                continue;
            }
            const FlightDataType&      type  = m_configuration.getType(series.row);
            const std::string          yName = typeName(type);
            const std::vector<double>* range = dataBranch.getView(type);
            if (range == nullptr)
            {
                continue;
            }
            const LinearInterpolator rangeInterpolator(*time, *range);

            for (std::size_t i = 0; i < eventSets.size(); ++i)
            {
                const EventGroup& events = eventSets[i];
                const double      t      = events.time;

                const double xcoord =
                    m_configuration.getDomainAxisUnit().toUnit(domainInterpolator.getValue(t));
                const double ycoord =
                    m_configuration.getUnit(series.row).toUnit(rangeInterpolator.getValue(t));
                if (std::isnan(xcoord) || std::isnan(ycoord))
                {
                    continue;
                }
                // The sample index of the flight event (the closest sample to its time).
                const std::size_t sampleIdx = nearestSample(*time, t);
                const double      yloc      = (slope * ycoord) + intercept;
                icons.push_back(EventIcon{
                    .series              = seriesIndex,
                    .group               = i,
                    .time                = t,
                    .x                   = xcoord,
                    .y                   = ycoord,
                    .yOnLeftAxis         = yloc,
                    .lookType            = events.lookType,
                    .lookWarningPriority = events.lookWarningPriority,
                    .toolTip             = formatToolTip(
                        getSeriesNameAt(seriesIndex, sampleIdx), t,
                        ToolTipValue{.name = xName, .value = toolTipNumber(xcoord), .unit = unitX},
                        yName, ycoord, series.unit, &events.events)});
            }
        }
    }
}

SimulationPlotModel::ToolTip SimulationPlotModel::formatToolTip(
    std::string name, double time, const ToolTipValue& x, const std::string& yName, double y,
    const std::string& yUnit, const std::vector<PlotEvent>* events) const
{
    ToolTip toolTip;
    toolTip.name = std::move(name);

    // Any events?
    if (events != nullptr)
    {
        // Pass through and collect any warnings
        for (const PlotEvent& info : *events)
        {
            if (info.event.getType() == FlightEvent::Type::SIM_WARN)
            {
                const Warning* warning = eventWarning(*m_data, info.event);
                toolTip.warnings.push_back(warning != nullptr ? warning->toString() : "null");
            }
        }
        // Now pass through and collect the other events
        for (const PlotEvent& info : *events)
        {
            if (info.event.getType() != FlightEvent::Type::SIM_WARN)
            {
                toolTip.events.push_back(eventName(info.event.getType()));
            }
        }
    }

    // Valid Y data?
    if (!std::isnan(y))
    {
        toolTip.y = ToolTipValue{.name = yName, .value = toolTipNumber(y), .unit = yUnit};
    }

    // Assuming X data is valid
    toolTip.x = x;

    // If I've got time data, and my domain isn't time, add time to tooltip
    const std::string timeName = typeName(timeType());
    if (!std::isnan(time) && x.name != timeName)
    {
        toolTip.time = ToolTipValue{.name  = timeName,
                                    .value = toolTipNumber(time),
                                    .unit  = timeType().getUnitGroup().getDefaultUnit().getUnit()};
    }
    return toolTip;
}

std::string SimulationPlotModel::getSeriesNameAt(std::size_t series, std::size_t point) const
{
    if (series >= m_series.size())
    {
        bug(std::format("No series {} (the plot has {})", series, m_series.size()));
    }
    const Series& ser = m_series[series];

    // Nothing special for single-stage rockets, or the first stage
    if (m_branchCount <= 1 || ser.branch == m_branchCount - 1)
    {
        return ser.name;
    }

    // For multi-stage rockets, add the other attached stages to the series name
    std::string newBranchName = m_data->getBranch(ser.branch).getName();
    for (std::size_t i = ser.branch + 1; i < m_branchCount; ++i)
    {
        // If the separation time is at or after the current data index, the stage is still
        // attached, so add the stage name to the series name
        const std::optional<std::size_t>& separationIdx = m_separationIndices[i];
        if (separationIdx.has_value() && *separationIdx >= point)
        {
            newBranchName += " + " + m_data->getBranch(i).getName();
        }
    }
    return newBranchName + ": " + ser.label;
}

SimulationPlotModel::ToolTip SimulationPlotModel::getToolTip(std::size_t series,
                                                             std::size_t point) const
{
    if (series >= m_series.size())
    {
        bug(std::format("No series {} (the plot has {})", series, m_series.size()));
    }
    const Series& ser = m_series[series];
    if (point >= ser.x.size())
    {
        bug(std::format("No point {} in series {} (it has {})", point, series, ser.x.size()));
    }
    const std::vector<double>* time  = m_data->getBranch(ser.branch).getView(timeType());
    const double               dataT = time != nullptr ? time->at(point) : kNaN;
    return formatToolTip(getSeriesNameAt(series, point), dataT,
                         ToolTipValue{.name  = typeName(m_configuration.getDomainAxisType()),
                                      .value = toolTipNumber(ser.x[point]),
                                      .unit  = m_filled->getDomainAxisUnit().getUnit()},
                         typeName(m_configuration.getType(ser.row)), ser.y[point], ser.unit,
                         nullptr);
}

void SimulationPlotModel::checkBranch(std::optional<std::size_t> branch) const
{
    if (branch.has_value() && *branch >= m_branchCount)
    {
        bug(std::format("No branch {} (the data has {})", *branch, m_branchCount));
    }
}

const std::vector<SimulationPlotModel::EventGroup>& SimulationPlotModel::getEventGroups(
    std::optional<std::size_t> branch) const
{
    checkBranch(branch);
    return branch.has_value() ? m_branchGroups[*branch] : m_allGroups;
}

std::vector<SimulationPlotModel::EventIcon> SimulationPlotModel::getIconMarkers(
    std::optional<std::size_t> branch) const
{
    checkBranch(branch);
    if (branch.has_value())
    {
        return m_branchIcons[*branch];
    }
    // Lower number branches get their icons put on top of higher number ones.
    std::vector<EventIcon> icons;
    for (std::size_t b = m_branchCount; b-- > 0;)
    {
        icons.insert(icons.end(), m_branchIcons[b].begin(), m_branchIcons[b].end());
    }
    return icons;
}

std::vector<SimulationPlotModel::AbortLine> SimulationPlotModel::getAbortLines(
    std::optional<std::size_t> branch) const
{
    checkBranch(branch);
    std::vector<AbortLine> lines;
    const std::size_t      first = branch.value_or(0);
    const std::size_t      last  = branch.has_value() ? *branch + 1 : m_branchCount;
    for (std::size_t b = first; b < last; ++b)
    {
        const FlightDataBranch& dataBranch = m_data->getBranch(b);
        const FlightEvent*      abortEvent = dataBranch.getFirstEvent(FlightEvent::Type::SIM_ABORT);
        if (abortEvent == nullptr)
        {
            continue;
        }
        const SimulationAbort* abort = abortEvent->getAbort();
        QTROCKET_ASSERT(abort != nullptr);
        lines.push_back(AbortLine{.branch     = b,
                                  .branchName = dataBranch.getName(),
                                  .time       = abortEvent->getTime(),
                                  .cause      = abort->cause()});
    }
    return lines;
}

std::string SimulationPlotModel::getAbortText(std::optional<std::size_t> branch) const
{
    const std::vector<AbortLine> lines = getAbortLines(branch);
    if (lines.empty())
    {
        return {};
    }
    std::string abortString = translate("simulationplot.abort.title", "Simulation Abort");
    for (const AbortLine& line : lines)
    {
        abortString += "\n" + translate("simulationplot.abort.stage", "Stage") + ": " +
                       line.branchName + "; " + translate("simulationplot.abort.time", "Time") +
                       ": " + Strings::formatFixed(line.time, 2) + " s; " +
                       translate("simulationplot.abort.cause", "Cause") + ": " +
                       translate({}, causeText(line.cause));
    }
    return abortString;
}

std::vector<int> SimulationPlotModel::computeDefaultColorIndices(
    const SimulationPlotConfiguration& configuration, const FlightData& data)
{
    if (data.getBranchCount() == 0)
    {
        std::vector<int> colors(configuration.getDataCount());
        for (std::size_t row = 0; row < colors.size(); ++row)
        {
            colors[row] = static_cast<int>(row);
        }
        return colors;
    }
    // Mirror Plot's axis-based palette ordering so default colors match the rendered series.
    const std::unique_ptr<SimulationPlotConfiguration::Base> filled =
        configuration.fillAutoAxes(data.getBranch(0));
    return colorIndices(*filled, data.getBranchCount());
}

}  // namespace QtRocket
