#include "QtRocket/gui/plot/SimulationPlot.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QEvent>
#include <QIcon>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <Qt>

#include <rocketplot/Annotation.h>
#include <rocketplot/EventMarker.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/Series.h>
#include <rocketplot/TextAnnotation.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/PlotEventGraphics.h"
#include "QtRocket/gui/plot/PlotText.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p range as rocketplot's, nullopt when it has no data (NaN).
[[nodiscard]] std::optional<rocketplot::Range> rangeOf(const SimulationPlotModel::Range& range)
{
    const rocketplot::Range converted{.min = range.min, .max = range.max};
    if (!converted.isValid())
    {
        return std::nullopt;
    }
    return converted;
}

/// The model of @p simulation's flight data as @p configuration says.
[[nodiscard]] SimulationPlotModel modelOf(const Simulation&                  simulation,
                                          const SimulationPlotConfiguration& configuration)
{
    QTROCKET_ASSERT(simulation.getSimulatedData() != nullptr);
    return {simulation.getName(), simulation.getSimulatedData(), configuration,
            SimulationPlot::translator()};
}

/// The legend group of the series of row @p row: one legend entry per row (Plot.LegendItems).
[[nodiscard]] QString legendGroupOf(std::size_t row)
{
    return QStringLiteral("row %1").arg(row);
}

}  // namespace

SimulationPlot::SimulationPlot(const GuiServices& services, SimulationPlotModel model,
                               bool showPoints, QWidget* parent)
  : DataPlot(services, showPoints, parent), m_model(std::move(model))
{
    // The events' flags in a band of their own above the plot area: the axes keep their ranges.
    plotWidget().setEventFlagPlacement(rocketplot::FlagPlacement::ABOVE_PLOT_AREA);
    build();
    // ErrorAnnotationSet's constructor: setCurrent(-1), shown.
    updateAbortBox();
}

SimulationPlot::SimulationPlot(const GuiServices& services, const Simulation& simulation,
                               const SimulationPlotConfiguration& configuration, bool showPoints,
                               QWidget* parent)
  : SimulationPlot(services, modelOf(simulation, configuration), showPoints, parent)
{
}

SimulationPlotModel::Translator SimulationPlot::translator()
{
    return [](std::string_view key, std::string_view englishText) {
        return QtStrings::toUtf8(translatedEnumText(key, englishText));
    };
}

void SimulationPlot::setShowEvents(bool show)
{
    if (show)
    {
        drawEvents(std::nullopt);
    }
    else
    {
        // OpenRocket bug 44: Java clears every annotation of the chart here, the abort box with
        // them; only the events go.
        clearEvents();
    }
}

void SimulationPlot::setShowErrors(bool show)
{
    m_showErrors = show;
    if (m_abortBox != nullptr)
    {
        m_abortBox->setVisible(show);
    }
}

void SimulationPlot::setShowBranch(std::optional<std::size_t> branch)
{
    QTROCKET_ASSERT(!branch.has_value() || *branch < m_model.getBranchCount());
    m_branch                                            = branch;
    const std::vector<SimulationPlotModel::Series>& all = m_model.getSeries();
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        // Java: index % branch count == branch (OpenRocket bug 48: by the series' own branch).
        if (rocketplot::LineSeries* const line = series(i); line != nullptr)
        {
            line->setVisible(!branch.has_value() || all[i].branch == *branch);
        }
    }
    // Kept as Java: the stage's events are drawn whether or not Show events is checked.
    drawEvents(branch);
    updateAbortBox();
}

std::vector<rocketplot::EventMarker*> SimulationPlot::eventLines() const
{
    std::vector<rocketplot::EventMarker*> lines;
    for (const EventLine& line : m_eventLines)
    {
        if (line.marker != nullptr)
        {
            lines.push_back(line.marker.data());
        }
    }
    return lines;
}

std::vector<rocketplot::IconAnnotation*> SimulationPlot::eventIcons() const
{
    std::vector<rocketplot::IconAnnotation*> icons;
    for (const QPointer<rocketplot::IconAnnotation>& icon : m_eventIcons)
    {
        if (icon != nullptr)
        {
            icons.push_back(icon.data());
        }
    }
    return icons;
}

QString SimulationPlot::toolTipText(std::size_t series, std::size_t point) const
{
    return QtStrings::fromUtf8(m_model.getToolTip(series, point).toHtml());
}

void SimulationPlot::changeEvent(QEvent* event)
{
    DataPlot::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        // This widget's palette: the plot's own follows only after this event.
        recolor();
    }
}

void SimulationPlot::build()
{
    setTitles(QtStrings::fromUtf8(m_model.getTitle()), QtStrings::fromUtf8(m_model.getSubtitle()));
    const auto& axes = m_model.getYAxes();
    setAxisLabels(QtStrings::fromUtf8(m_model.getDomainLabel()),
                  axes[0].hasSeries ? QtStrings::fromUtf8(axes[0].label) : QString(),
                  axes[1].hasSeries ? QtStrings::fromUtf8(axes[1].label) : QString());
    const std::vector<SimulationPlotModel::Series>& all = m_model.getSeries();
    for (std::size_t index = 0; index < all.size(); ++index)
    {
        const SimulationPlotModel::Series& plotted = all[index];
        // OpenRocket bug 42: the colour of the series' own row (the model's colour index).
        const QColor color = plotted.color.has_value() ? ColorConversion::toQColor(*plotted.color)
                                                       : PlotColors::plotColor(plotted.colorIndex);
        std::vector<double>     x    = plotted.x;
        std::vector<double>     y    = plotted.y;
        rocketplot::LineSeries& line = addSeries(std::move(x), std::move(y),
                                                 {.name        = QtStrings::fromUtf8(plotted.name),
                                                  .legendGroup = legendGroupOf(plotted.row),
                                                  .axis        = plotted.axis,
                                                  .color       = color,
                                                  .lineStyle   = plotted.lineStyle,
                                                  .shape       = plotted.branch});
        // The legend's entry of a row stands for the row's series of every branch, and shows
        // them all (a click, a double click, its menu): a stage chosen keeps the others hidden.
        connect(&line, &rocketplot::Series::changed, this,
                [this, index] { keepToTheShownBranch(index); });
    }
    updateHomeRanges();
}

void SimulationPlot::keepToTheShownBranch(std::size_t index)
{
    if (!m_branch.has_value() || m_model.getSeries().at(index).branch == *m_branch)
    {
        return;
    }
    // Hiding it emits changed() again, which finds it hidden.
    if (rocketplot::LineSeries* const line = series(index); line != nullptr && line->isVisible())
    {
        line->setVisible(false);
    }
}

void SimulationPlot::updateHomeRanges()
{
    const auto& axes = m_model.getYAxes();
    // OpenRocket bug 43: the range widened for the event lines, once.
    const SimulationPlotModel::Range& domain =
        m_domainWidened ? m_model.getDomainRangeWithEventLines() : m_model.getDomainRange();
    setHomeRanges(rangeOf(domain), axes[0].hasSeries ? rangeOf(axes[0].range) : std::nullopt,
                  axes[1].hasSeries ? rangeOf(axes[1].range) : std::nullopt);
}

void SimulationPlot::drawEvents(std::optional<std::size_t> branch)
{
    clearEvents();
    if (m_model.hasTimeDomain() && !PlotPreferences::isMarkerStyleIcon(services().preferences()))
    {
        drawEventLines(branch);
    }
    else
    {
        drawEventIcons(branch);
    }
}

void SimulationPlot::drawEventLines(std::optional<std::size_t> branch)
{
    rocketplot::PlotWidget& plot = plotWidget();
    for (const SimulationPlotModel::EventGroup& group : m_model.getEventGroups(branch))
    {
        rocketplot::EventMarker* const marker =
            plot.addEvent(group.time, rocketplot::literalText(QtStrings::fromUtf8(group.label)));
        marker->setColor(PlotEventGraphics::markerColor(group.lookType, palette()));
        marker->setIcon(PlotEventGraphics::eventIcon(group.lookType, group.lookWarningPriority));
        m_eventLines.push_back({.marker = marker, .look = group.lookType});
    }
    if (!m_domainWidened)
    {
        // OpenRocket bug 43: widened once, by the groups of every stage, and keeping its label.
        m_domainWidened = true;
        updateHomeRanges();
    }
}

void SimulationPlot::drawEventIcons(std::optional<std::size_t> branch)
{
    rocketplot::PlotWidget& plot = plotWidget();
    for (const SimulationPlotModel::EventIcon& marker : m_model.getIconMarkers(branch))
    {
        const QIcon image =
            PlotEventGraphics::eventIcon(marker.lookType, marker.lookWarningPriority);
        if (image.isNull())
        {
            // plotIconMarkers(): a group without an image has no icon.
            continue;
        }
        rocketplot::IconAnnotation* const icon = plot.addIcon(marker.x, marker.y, image);
        if (m_model.getSeries().at(marker.series).axis == 1)
        {
            icon->setYAxis(plot.yAxis2());
        }
        icon->setToolTip(QtStrings::fromUtf8(marker.toolTip.toHtml()));
        m_eventIcons.emplace_back(icon);
    }
}

void SimulationPlot::clearEvents()
{
    rocketplot::PlotWidget& plot = plotWidget();
    for (const EventLine& line : m_eventLines)
    {
        if (line.marker != nullptr)
        {
            plot.removeAnnotation(line.marker.data());
        }
    }
    m_eventLines.clear();
    for (const QPointer<rocketplot::IconAnnotation>& icon : m_eventIcons)
    {
        if (icon != nullptr)
        {
            plot.removeAnnotation(icon.data());
        }
    }
    m_eventIcons.clear();
}

void SimulationPlot::updateAbortBox()
{
    rocketplot::PlotWidget& plot = plotWidget();
    const QString           text = QtStrings::fromUtf8(m_model.getAbortText(m_branch));
    if (text.isEmpty())
    {
        if (m_abortBox != nullptr)
        {
            plot.removeAnnotation(m_abortBox.data());
        }
        m_abortBox = nullptr;
        return;
    }
    if (m_abortBox == nullptr)
    {
        m_abortBox =
            plot.addText(kAbortBoxPosition, kAbortBoxPosition, PlotText::literalText(text));
        m_abortBox->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
        m_abortBox->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        m_abortBox->setArrowVisible(false);
        m_abortBox->setFrameVisible(true);
        m_abortBox->setColor(ThemeColors::color(ThemeColors::Key::ERROR, palette()));
    }
    else
    {
        m_abortBox->setText(PlotText::literalText(text));
    }
    m_abortBox->setVisible(m_showErrors);
}

void SimulationPlot::recolor()
{
    for (const EventLine& line : m_eventLines)
    {
        if (line.marker != nullptr)
        {
            line.marker->setColor(PlotEventGraphics::markerColor(line.look, palette()));
        }
    }
    if (m_abortBox != nullptr)
    {
        m_abortBox->setColor(ThemeColors::color(ThemeColors::Key::ERROR, palette()));
    }
}

}  // namespace QtRocket::Gui
