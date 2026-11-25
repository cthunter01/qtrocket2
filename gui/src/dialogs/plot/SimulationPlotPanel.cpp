#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/plot/FlightEventTableModel.h"
#include "QtRocket/gui/dialogs/plot/PlotPanel.h"
#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/PlotDefaults.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/ColorChooserButton.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/LineStyleComboBox.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui
{

namespace
{

constexpr int kRelatedGap   = PlotTypeSelector::kRelatedGap;
constexpr int kParagraphGap = PlotTypeSelector::kParagraphGap;

/// The types of @p simulation's first branch (Java's create(): branch 0's getTypes()), checked: a
/// simulation with flight data.
[[nodiscard]] PlotPanel::DataTypes plottedTypes(const std::shared_ptr<Simulation>& simulation)
{
    // Check the simulation data ("Simulation contains no data.").
    QTROCKET_ASSERT(simulation != nullptr);
    const std::shared_ptr<FlightData>& data = simulation->getSimulatedData();
    QTROCKET_ASSERT(data != nullptr && data->getBranchCount() > 0);
    return data->getBranch(0).getTypes();
}

/// The events table: its size to begin with is SimulationPlotPanel's (the scroll pane's
/// preferred size of 150 and "width 200lp").
class EventTable final : public QTableView
{
public:
    explicit EventTable(QWidget* parent) : QTableView(parent) { }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {SimulationPlotPanel::kEventTableWidth, SimulationPlotPanel::kEventTableHeight};
    }
};

/// A button of the events panel, named @p name; never a dialog's default button.
[[nodiscard]] QPushButton* eventsButton(QWidget* parent, const QString& text, const QString& name)
{
    auto* button = makeChild<QPushButton>(parent, text);
    button->setObjectName(name);
    button->setAutoDefault(false);
    return button;
}

}  // namespace

SimulationPlotPanel::SimulationPlotPanel(const GuiServices&          services,
                                         std::shared_ptr<Simulation> simulation, QWidget* parent)
  : PlotPanel(DeferredBuild{}, services, plottedTypes(simulation), plottedTypes(simulation),
              presetArray(), startConfiguration(services), parent),
    m_simulation(std::move(simulation)),
    m_eventModel(makeChild<FlightEventTableModel>(
        this, FlightEventTableModel::eventTypesOf(*m_simulation->getSimulatedData()),
        configuration())),
    m_description(makeDescription()),
    //: simplotpanel.lbl.Flightevents
    m_eventsLabel(makeChild<QLabel>(this, tr("Flight events:"))),
    m_eventsPanel(makeEventsPanel())
{
    m_eventModel->setObjectName(QStringLiteral("eventTableModel"));
    build({.xWidgets = {m_description}, .yWidgets = {m_eventsLabel, m_eventsPanel}});

    //// X axis listeners
    connect(&domainTypeSelector(), &DataTypeComboBox::selectedTypeChanged, this,
            guarded([this] { domainTypeSelected(); }, QStringLiteral("X axis type")));
    connect(&domainTypeSelector(), &DataTypeComboBox::selectedTypeChanged, this,
            guarded([this] { updateStyleEventWidgets(); }, QStringLiteral("X axis type")));
    updateStyleEventWidgets();

    updatePlots();
}

SimulationPlotPanel::~SimulationPlotPanel() = default;

SimulationPlotConfiguration SimulationPlotPanel::startConfiguration(const GuiServices& services)
{
    // DEFAULT_CONFIGURATION: the configuration last plotted, or the first preset in the default
    // units (DEFAULT_CONFIGURATIONS[0].resetUnits()).
    if (const PlotDefaults* defaults = services.plotDefaults(); defaults != nullptr)
    {
        if (const std::shared_ptr<const SimulationPlotConfiguration> last = defaults->lastPlotted();
            last != nullptr)
        {
            return *last;
        }
    }
    return SimulationPlotConfiguration::defaultConfigurations().front();
}

PlotPanel::Configurations SimulationPlotPanel::presetArray()
{
    Configurations presets = SimulationPlotConfiguration::defaultConfigurations();
    presets.push_back(SimulationPlotConfiguration::customConfiguration());
    return presets;
}

DescriptionArea* SimulationPlotPanel::makeDescription()
{
    //// The data will be plotted in time order even if the X axis type is not time.
    auto* description =
        makeChild<DescriptionArea>(this, QString(), kDescriptionRows, kDescriptionSize, false);
    description->setObjectName(QStringLiteral("simPlotPanelDesc"));
    description->setForegroundColor(
        ThemeColors::color(ThemeColors::Key::DARK_ERROR, description->palette()));
    // MigLayout's hidemode 0: hidden, it keeps its place.
    QSizePolicy policy = description->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    description->setSizePolicy(policy);
    description->setVisible(false);
    return description;
}

QWidget* SimulationPlotPanel::makeEventsPanel()
{
    auto* panel = makeChild<QWidget>(this);
    panel->setObjectName(QStringLiteral("eventsPanel"));
    auto* layout = makeChild<QVBoxLayout>(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRelatedGap);

    //// Flight events
    m_eventTable = makeChild<EventTable>(panel);
    m_eventTable->setObjectName(QStringLiteral("flightEventTable"));
    m_eventTable->setModel(m_eventModel);
    m_eventTable->horizontalHeader()->hide();
    m_eventTable->verticalHeader()->hide();
    m_eventTable->setShowGrid(false);
    m_eventTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // The check column: as wide as a row is high, plus 2.
    const int checkWidth = m_eventTable->verticalHeader()->defaultSectionSize() + 2;
    m_eventTable->horizontalHeader()->setMinimumSectionSize(checkWidth);
    m_eventTable->horizontalHeader()->setSectionResizeMode(FlightEventTableModel::kSelectedColumn,
                                                           QHeaderView::Fixed);
    m_eventTable->setColumnWidth(FlightEventTableModel::kSelectedColumn, checkWidth);
    m_eventTable->horizontalHeader()->setStretchLastSection(true);
    // GUIUtil.BooleanTableClickListener: a click on a name checks or unchecks it.
    connect(m_eventTable, &QTableView::clicked, this,
            guarded(
                [this](const QModelIndex& index) {
                    if (index.column() == FlightEventTableModel::kNameColumn)
                    {
                        m_eventModel->toggle(index.row());
                    }
                },
                QStringLiteral("Flight events")));
    layout->addWidget(m_eventTable, 1);

    ////  All + None buttons ("gapleft para, gapright para, growx, sizegroup buttons")
    auto* buttons = makeChild<QWidget>(panel);
    buttons->setObjectName(QStringLiteral("eventButtons"));
    auto* buttonsLayout = makeChild<QHBoxLayout>(buttons);
    buttonsLayout->setContentsMargins(kParagraphGap, 0, kParagraphGap, 0);
    buttonsLayout->setSpacing(kParagraphGap);
    //: simplotpanel.but.All
    m_allButton = eventsButton(buttons, tr("All"), QStringLiteral("allButton"));
    //: simplotpanel.but.None
    m_noneButton = eventsButton(buttons, tr("None"), QStringLiteral("noneButton"));
    connect(m_allButton, &QPushButton::clicked, this,
            guarded([this] { setAllEvents(true); }, QStringLiteral("All")));
    connect(m_noneButton, &QPushButton::clicked, this,
            guarded([this] { setAllEvents(false); }, QStringLiteral("None")));
    // "sizegroup buttons": both as wide as the wider one would be.
    const int buttonWidth =
        std::max(m_allButton->sizeHint().width(), m_noneButton->sizeHint().width());
    m_allButton->setMinimumWidth(buttonWidth);
    m_noneButton->setMinimumWidth(buttonWidth);
    buttonsLayout->addWidget(m_allButton, 1);
    buttonsLayout->addWidget(m_noneButton, 1);
    layout->addWidget(buttons);

    //// Style event marker
    auto* style = makeChild<QWidget>(panel);
    style->setObjectName(QStringLiteral("markerStyle"));
    auto* styleLayout = makeChild<QHBoxLayout>(style);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(kRelatedGap);
    //: simplotpanel.MarkerStyle.lbl.MarkerStyle
    m_styleEventMarker = makeChild<QLabel>(style, tr("Marker style:"));
    m_styleEventMarker->setObjectName(QStringLiteral("styleEventMarker"));
    //: simplotpanel.MarkerStyle.btn.VerticalMarker
    m_radioVerticalMarker = makeChild<QRadioButton>(style, tr("Vertical line"));
    m_radioVerticalMarker->setObjectName(QStringLiteral("radioVerticalMarker"));
    //: simplotpanel.MarkerStyle.btn.Icon
    m_radioIcon = makeChild<QRadioButton>(style, tr("Icon"));
    m_radioIcon->setObjectName(QStringLiteral("radioIcon"));
    auto* group = makeChild<QButtonGroup>(style);
    group->addButton(m_radioVerticalMarker);
    group->addButton(m_radioIcon);
    if (PlotPreferences::isMarkerStyleIcon(services().preferences()))
    {
        m_radioIcon->setChecked(true);
    }
    else
    {
        m_radioVerticalMarker->setChecked(true);
    }
    // radioIcon's item listener: both its selection and its deselection store it.
    connect(m_radioIcon, &QRadioButton::toggled, this,
            guarded(
                [this](bool icon) {
                    if (!isModifying())
                    {
                        PlotPreferences::setMarkerStyleIcon(services().preferences(), icon);
                    }
                },
                QStringLiteral("Marker style")));
    // "split 3, growx": the label takes what the radio buttons leave.
    styleLayout->addWidget(m_styleEventMarker, 1);
    styleLayout->addWidget(m_radioVerticalMarker);
    styleLayout->addWidget(m_radioIcon);
    layout->addWidget(style);
    return panel;
}

bool SimulationPlotPanel::doPlot(QWidget* parent)
{
    if (getConfiguration().getDataCount() == 0)
    {
        services().prompts().showMessage(
            this,
            {.severity = UserPrompts::Severity::ERROR,
             //: SimulationPlotPanel.error.noPlotSelected.title
             .title = tr("Nothing to plot"),
             //: SimulationPlotPanel.error.noPlotSelected
             .lines = QStringList{tr("Please add one or more variables to plot on the Y-axis.")}});
        return false;
    }
    SimulationDialogs* const dialogs = services().simulationDialogs();
    QTROCKET_ASSERT(dialogs != nullptr);
    // setDefaultConfiguration(configuration.clone()): strip per-simulation appearance overrides so
    // they don't leak into other simulations that will start from this default.
    if (PlotDefaults* const defaults = services().plotDefaults(); defaults != nullptr)
    {
        auto plotted = std::make_shared<SimulationPlotConfiguration>(getConfiguration());
        plotted->clearPlotAppearances();
        defaults->setLastPlotted(std::move(plotted));
    }
    dialogs->plotSimulation(parent, m_simulation, getConfiguration());
    return true;
}

void SimulationPlotPanel::updatePlots()
{
    m_defaultPlotColors = computeDefaultPlotColors();
    m_plotTypeSelectors.clear();
    PlotPanel::updatePlots();
    m_eventModel->refresh();
}

SimulationPlotTypeSelector& SimulationPlotPanel::plotTypeSelector(std::size_t index) const
{
    QTROCKET_ASSERT(index < m_plotTypeSelectors.size());
    return *m_plotTypeSelectors[index];
}

QColor SimulationPlotPanel::defaultPlotColor(std::size_t index) const
{
    return index < m_defaultPlotColors.size() ? m_defaultPlotColors[index]
                                              : PlotColors::plotColor(static_cast<int>(index));
}

PlotTypeSelector* SimulationPlotPanel::createSelector(std::size_t index, const FlightDataType& type,
                                                      const Unit& unit, int axis, QWidget* parent)
{
    if (const std::optional<PlotAppearance> stored = effectiveAppearance(type); stored.has_value())
    {
        applyStoredAppearance(index, *stored);
    }
    const std::optional<Color>& color    = getConfiguration().getPlotDataColor(index);
    auto*                       selector = makeChild<SimulationPlotTypeSelector>(
        parent, index, type, &unit, axis, typesY(),
        color.has_value() ? ColorConversion::toQColor(*color) : defaultPlotColor(index),
        getConfiguration().getPlotDataLineStyle(index), services().prompts());
    QTROCKET_ASSERT(m_plotTypeSelectors.size() == index);
    m_plotTypeSelectors.push_back(selector);
    return selector;
}

void SimulationPlotPanel::addSelectionListeners(PlotTypeSelector& selector, std::size_t index)
{
    SimulationPlotTypeSelector& row = plotTypeSelector(index);
    QTROCKET_ASSERT(&selector == &row);
    // Swing calls a combo box's item listeners last added first, so the axis and type listeners
    // Java adds here after super.addSelectionListeners() run before PlotPanel's: connected first,
    // they are called first (the plot's colours are computed before the axis or the type changes).
    connect(&row.axisSelector(), &QComboBox::currentIndexChanged, &row,
            guarded([this] { axisSelected(); }, QStringLiteral("Y axis")));
    connect(
        &row.typeSelector(), &DataTypeComboBox::selectedTypeChanged, &row,
        guarded([this, &row, index] { typeSelected(row, index); }, QStringLiteral("Y axis type")));
    PlotPanel::addSelectionListeners(selector, index);

    connect(&row.colorButton(), &ColorChooserButton::selectedColorChanged, &row,
            guarded([this, &row, index] { colorSelected(row, index); }, QStringLiteral("Color")));
    connect(&row.lineStyleSelector(), &LineStyleComboBox::lineStyleSelected, &row,
            guarded([this, &row, index] { lineStyleSelected(row, index); },
                    QStringLiteral("Line style")));
    connect(&row.setDefaultButton(), &QPushButton::clicked, &row,
            guarded([this, &row, index] { setAsDefault(row, index); },
                    QStringLiteral("Set as default")));
    connect(&row.resetToFactoryButton(), &QPushButton::clicked, &row,
            guarded([this, &row, index] { resetToFactory(row, index); },
                    QStringLiteral("Reset to factory default")));

    // Set initial button states now that all configuration for this selector is in place.
    syncButtonStates(row, index);
}

void SimulationPlotPanel::domainTypeSelected()
{
    if (isModifying())
    {
        return;
    }
    const FlightDataType* type = domainTypeSelector().selectedType();
    if (type == nullptr)
    {
        return;
    }
    if (type == &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME))
    {
        m_description->setVisible(false);
        m_description->setDescription(QString());
    }
    else
    {
        m_description->setVisible(true);
        //: simplotpanel.Desc
        m_description->setDescription(
            tr("The data will be plotted in time order even if the X axis type is not time."));
    }
}

void SimulationPlotPanel::updateStyleEventWidgets()
{
    if (isModifying())
    {
        return;
    }
    const bool isTime = domainTypeSelector().selectedType() ==
                        &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    m_styleEventMarker->setEnabled(isTime);
    m_radioVerticalMarker->setEnabled(isTime);
    m_radioIcon->setEnabled(isTime);
    //: simplotpanel.MarkerStyle.OnlyInTime
    const QString onlyInTime =
        tr("Only available for time domain, other domains only support icon markers");
    m_styleEventMarker->setToolTip(
        isTime
            //: simplotpanel.MarkerStyle.lbl.MarkerStyle.ttip
            ? tr("Style of the flight event marker (how it's drawn in the simulation plot)")
            : onlyInTime);
    m_radioVerticalMarker->setToolTip(isTime ? QString() : onlyInTime);
    m_radioIcon->setToolTip(isTime ? QString() : onlyInTime);
}

void SimulationPlotPanel::axisSelected()
{
    if (isModifying())
    {
        return;
    }
    m_defaultPlotColors = computeDefaultPlotColors();
    refreshDefaultPlotColors();
}

void SimulationPlotPanel::typeSelected(SimulationPlotTypeSelector& selector, std::size_t index)
{
    const FlightDataType* type = selector.selectedType();
    if (isModifying() || type == nullptr)
    {
        return;
    }
    {
        const Modifying modifying(*this);
        m_defaultPlotColors = computeDefaultPlotColors();
        applyTypeAppearance(index, *type, selector);
        refreshDefaultPlotColors();
    }
    syncButtonStates(selector, index);
}

void SimulationPlotPanel::colorSelected(SimulationPlotTypeSelector& selector, std::size_t index)
{
    if (isModifying())
    {
        return;
    }
    const Color color = ColorConversion::fromQColor(selector.selectedColor());
    configuration().setPlotDataColor(index, color);
    updateSimulationAppearance(selector.selectedType(), color, std::nullopt, true, false);
    syncButtonStates(selector, index);
}

void SimulationPlotPanel::lineStyleSelected(SimulationPlotTypeSelector& selector, std::size_t index)
{
    if (isModifying())
    {
        return;
    }
    const LineStyle lineStyle = selector.selectedLineStyle();
    configuration().setPlotDataLineStyle(index, lineStyle);
    updateSimulationAppearance(selector.selectedType(), std::nullopt, lineStyle, false, true);
    syncButtonStates(selector, index);
}

void SimulationPlotPanel::setAsDefault(SimulationPlotTypeSelector& selector, std::size_t index)
{
    const FlightDataType* type = selector.selectedType();
    if (isModifying() || type == nullptr)
    {
        return;
    }
    const PlotAppearance appearance(getConfiguration().getPlotDataColor(index),
                                    getConfiguration().getPlotDataLineStyle(index));
    PlotPreferences::setDefaultPlotAppearance(services().preferences(), *type, appearance);
    syncButtonStates(selector, index);
}

void SimulationPlotPanel::resetToFactory(SimulationPlotTypeSelector& selector, std::size_t index)
{
    const FlightDataType* type = selector.selectedType();
    if (isModifying() || type == nullptr)
    {
        return;
    }
    // Clear the per-simulation override so this type returns to factory defaults.
    m_simulation->setPlotAppearance(*type, std::nullopt);
    configuration().setPlotDataColor(index, std::nullopt);
    configuration().setPlotDataLineStyle(index, LineStyle::SOLID);
    // Update the UI to reflect the reset without triggering change listeners.
    {
        const Modifying modifying(*this);
        selector.setSelectedColor(defaultPlotColor(index));
        selector.setSelectedLineStyle(LineStyle::SOLID);
    }
    syncButtonStates(selector, index);
}

void SimulationPlotPanel::setAllEvents(bool active)
{
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        configuration().setEvent(type, active);
    }
    m_eventModel->refresh();
}

void SimulationPlotPanel::syncButtonStates(SimulationPlotTypeSelector& selector, std::size_t index)
{
    const PlotAppearance  current(getConfiguration().getPlotDataColor(index),
                                  getConfiguration().getPlotDataLineStyle(index));
    const bool            isAtFactory = current == PlotAppearance::factoryDefault();
    const FlightDataType* type        = selector.selectedType();
    QTROCKET_ASSERT(type != nullptr);
    const std::optional<PlotAppearance> appDefault =
        PlotPreferences::getDefaultPlotAppearance(services().preferences(), *type);
    // No user default stored: the effective default is the factory default.
    const bool isAtAppDefault = appDefault.has_value() ? current == *appDefault : isAtFactory;
    selector.updateButtonStates(isAtFactory, isAtAppDefault);
}

void SimulationPlotPanel::applyTypeAppearance(std::size_t index, const FlightDataType& type,
                                              SimulationPlotTypeSelector& selector)
{
    const std::optional<PlotAppearance> stored = effectiveAppearance(type);
    std::optional<Color>                color;
    LineStyle                           lineStyle = LineStyle::SOLID;
    if (stored.has_value())
    {
        color = stored->getColor();
        if (stored->getLineStyle().has_value())
        {
            lineStyle = *stored->getLineStyle();
        }
    }
    configuration().setPlotDataColor(index, color);
    configuration().setPlotDataLineStyle(index, lineStyle);
    selector.setSelectedColor(color.has_value() ? ColorConversion::toQColor(*color)
                                                : defaultPlotColor(index));
    selector.setSelectedLineStyle(lineStyle);
}

void SimulationPlotPanel::refreshDefaultPlotColors()
{
    const Modifying modifying(*this);
    for (std::size_t index = 0; index < m_plotTypeSelectors.size(); ++index)
    {
        if (!getConfiguration().getPlotDataColor(index).has_value())
        {
            m_plotTypeSelectors[index]->setSelectedColor(defaultPlotColor(index));
        }
    }
}

std::optional<PlotAppearance> SimulationPlotPanel::effectiveAppearance(
    const FlightDataType& type) const
{
    std::optional<PlotAppearance> stored = m_simulation->getPlotAppearance(type);
    if (!stored.has_value() || *stored == PlotAppearance::factoryDefault())
    {
        return PlotPreferences::getDefaultPlotAppearance(services().preferences(), type);
    }
    return stored;
}

void SimulationPlotPanel::applyStoredAppearance(std::size_t index, const PlotAppearance& appearance)
{
    if (appearance.getColor().has_value() &&
        !getConfiguration().getPlotDataColor(index).has_value())
    {
        configuration().setPlotDataColor(index, appearance.getColor());
    }
    if (appearance.getLineStyle().has_value())
    {
        configuration().setPlotDataLineStyle(index, *appearance.getLineStyle());
    }
}

void SimulationPlotPanel::updateSimulationAppearance(const FlightDataType*       type,
                                                     const std::optional<Color>& color,
                                                     std::optional<LineStyle>    lineStyle,
                                                     bool updateColor, bool updateLineStyle)
{
    if (type == nullptr)
    {
        return;
    }
    const std::optional<PlotAppearance> appearance = m_simulation->getPlotAppearance(*type);
    std::optional<Color>                storedColor =
        appearance.has_value() ? appearance->getColor() : std::nullopt;
    std::optional<LineStyle> storedLineStyle =
        appearance.has_value() ? appearance->getLineStyle() : std::nullopt;
    if (updateColor)
    {
        storedColor = color;
    }
    if (updateLineStyle)
    {
        storedLineStyle = lineStyle;
    }
    m_simulation->setPlotAppearance(*type, PlotAppearance(storedColor, storedLineStyle));
}

std::vector<QColor> SimulationPlotPanel::computeDefaultPlotColors() const
{
    // (Not "data": QWidget has a member of that name.)
    const std::shared_ptr<FlightData>& flight = m_simulation->getSimulatedData();
    if (flight == nullptr)
    {
        return {};
    }
    // Mirror Plot's axis-based palette ordering so default colors match the rendered series.
    std::vector<QColor> colors;
    for (const int index :
         SimulationPlotModel::computeDefaultColorIndices(getConfiguration(), *flight))
    {
        colors.push_back(PlotColors::plotColor(index));
    }
    return colors;
}

}  // namespace QtRocket::Gui
