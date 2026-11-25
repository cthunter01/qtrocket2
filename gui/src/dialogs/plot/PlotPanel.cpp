#include "QtRocket/gui/dialogs/plot/PlotPanel.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

constexpr int kRelatedGap   = PlotTypeSelector::kRelatedGap;
constexpr int kUnrelatedGap = PlotTypeSelector::kUnrelatedGap;
constexpr int kParagraphGap = PlotTypeSelector::kParagraphGap;
constexpr int kUnitMinWidth = PlotTypeSelector::kUnitSelectorWidth;
/// MigLayout's insets of a panel ("ins n": 6 px under OpenRocket on Linux).
constexpr int kPanelInset = 6;

/// @p types, checked: not empty and no null type.
[[nodiscard]] PlotPanel::DataTypes checkedTypes(PlotPanel::DataTypes types)
{
    QTROCKET_ASSERT(!types.empty());
    for (const FlightDataType* type : types)
    {
        QTROCKET_ASSERT(type != nullptr);
    }
    return types;
}

/// @p presets, checked: not empty (the last is the custom configuration).
[[nodiscard]] PlotPanel::Configurations checkedPresets(PlotPanel::Configurations presets)
{
    QTROCKET_ASSERT(!presets.empty());
    return presets;
}

/// Utils.contains(): whether @p types holds a type equal to @p type (FlightDataType.equals()).
[[nodiscard]] bool containsType(const PlotPanel::DataTypes& types, const FlightDataType& type)
{
    return std::ranges::any_of(
        types, [&type](const FlightDataType* candidate) { return candidate->equals(type); });
}

/// setConfiguration()'s copy of @p configuration: the domain the first of @p typesX unless it is
/// one of them, the rows of no type of @p typesY removed, "Custom" when either happened.
[[nodiscard]] SimulationPlotConfiguration adjustedConfiguration(
    const SimulationPlotConfiguration& configuration, const PlotPanel::DataTypes& typesX,
    const PlotPanel::DataTypes& typesY)
{
    bool                        modified = false;
    SimulationPlotConfiguration adjusted = configuration;
    if (!containsType(typesX, adjusted.getDomainAxisType()))
    {
        adjusted.setDomainAxisType(*typesX.front());
        modified = true;
    }
    for (std::size_t i = 0; i < adjusted.getDataCount();)
    {
        if (!containsType(typesY, adjusted.getType(i)))
        {
            adjusted.removePlotDataType(i);
            modified = true;
        }
        else
        {
            ++i;
        }
    }
    if (modified)
    {
        adjusted.setName(std::string(SimulationPlotConfiguration::kCustomName),
                         std::string(SimulationPlotConfiguration::kCustomNameKey));
    }
    return adjusted;
}

/// A line of the panel: a widget whose layout lays its children out side by side, kRelatedGap
/// apart, without margins.
[[nodiscard]] QHBoxLayout* lineLayout(QWidget* line)
{
    auto* layout = makeChild<QHBoxLayout>(line);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRelatedGap);
    return layout;
}

/// A column of the panel: a widget whose layout lays its children out one below the other,
/// kRelatedGap apart, without margins.
[[nodiscard]] QVBoxLayout* columnLayout(QWidget* column)
{
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRelatedGap);
    return layout;
}

}  // namespace

PlotPanel::PlotPanel(const GuiServices& services, DataTypes typesX, DataTypes typesY,
                     Configurations                     presets,
                     const SimulationPlotConfiguration& defaultConfiguration, QWidget* parent)
  : PlotPanel(DeferredBuild{}, services, std::move(typesX), std::move(typesY), std::move(presets),
              defaultConfiguration, parent)
{
    build({});
}

PlotPanel::PlotPanel(DeferredBuild /*deferred*/, const GuiServices& services, DataTypes typesX,
                     DataTypes typesY, Configurations presets,
                     const SimulationPlotConfiguration& defaultConfiguration, QWidget* parent)
  : QWidget(parent),
    m_services(services),
    m_typesX(checkedTypes(std::move(typesX))),
    m_typesY(checkedTypes(std::move(typesY))),
    m_presets(checkedPresets(std::move(presets))),
    // setConfiguration(defaultConfiguration), whose listeners none has yet.
    m_configuration(adjustedConfiguration(defaultConfiguration, m_typesX, m_typesY)),
    m_presetBox(makePresetBox()),
    m_domainTypeSelector(makeChild<DataTypeComboBox>(this, m_typesX)),
    m_domainUnitSelector(
        makeChild<UnitSelector>(this, m_configuration.getDomainAxisType().getUnitGroup())),
    m_typeSelectorScroll(makeChild<QScrollArea>(this)),
    m_typeSelectorPanel(makeChild<QWidget>(this)),
    m_typeSelectorLayout(makeChild<QVBoxLayout>(m_typeSelectorPanel)),
    //: simplotpanel.but.NewYaxisplottype
    m_newYAxisButton(makeChild<QPushButton>(this, tr("New Y axis plot type")))
{
    //// X axis
    m_domainTypeSelector->setObjectName(QStringLiteral("domainTypeSelector"));
    m_domainTypeSelector->setSelectedType(&m_configuration.getDomainAxisType());
    connect(m_domainTypeSelector, &DataTypeComboBox::selectedTypeChanged, this,
            guarded([this] { domainTypeSelected(); }, QStringLiteral("X axis type")));
    m_domainUnitSelector->setObjectName(QStringLiteral("domainUnitSelector"));
    m_domainUnitSelector->setSelectedUnit(m_configuration.getDomainAxisUnit());
    m_domainUnitSelector->setMinimumWidth(kUnitMinWidth);
    connect(m_domainUnitSelector, &UnitSelector::unitSelected, this,
            guarded([this] { domainUnitSelected(); }, QStringLiteral("X axis unit")));

    //// Y axis selector panel: the rows from the top, MigLayout("gapy rel") with a panel's insets.
    m_typeSelectorPanel->setObjectName(QStringLiteral("typeSelectorPanel"));
    m_typeSelectorLayout->setContentsMargins(kPanelInset, kPanelInset, kPanelInset, kPanelInset);
    m_typeSelectorLayout->setSpacing(kRelatedGap);
    m_typeSelectorLayout->addStretch(1);
    m_typeSelectorScroll->setObjectName(QStringLiteral("typeSelectorScroll"));
    m_typeSelectorScroll->setWidgetResizable(true);
    m_typeSelectorScroll->setWidget(m_typeSelectorPanel);

    //// New Y axis plot type
    m_newYAxisButton->setObjectName(QStringLiteral("newYAxisBtn"));
    m_newYAxisButton->setAutoDefault(false);
    connect(m_newYAxisButton, &QPushButton::clicked, this,
            guarded([this] { newYAxisPlotType(); }, QStringLiteral("New Y axis plot type")));
}

PlotPanel::~PlotPanel() = default;

QComboBox* PlotPanel::makePresetBox()
{
    auto* box = makeChild<QComboBox>(this);
    box->setObjectName(QStringLiteral("configurationSelector"));
    for (const SimulationPlotConfiguration& preset : m_presets)
    {
        box->addItem(configurationText(preset));
    }
    // The preset of the configuration's name, the last one of that name (Java's loop); the first
    // when none has it (DefaultComboBoxModel selects its first item).
    for (std::size_t i = 0; i < m_presets.size(); ++i)
    {
        if (m_presets[i].getName() == m_configuration.getName())
        {
            box->setCurrentIndex(static_cast<int>(i));
        }
    }
    connect(box, &QComboBox::currentIndexChanged, this,
            guarded([this](int index) { presetSelected(index); },
                    QStringLiteral("Preset plot configurations")));
    return box;
}

void PlotPanel::build(const Extras& extras)
{
    QTROCKET_ASSERT(!m_built);
    m_built = true;
    for (const QWidget* widget : extras.xWidgets)
    {
        QTROCKET_ASSERT(widget != nullptr);
    }
    for (const QWidget* widget : extras.yWidgets)
    {
        QTROCKET_ASSERT(widget != nullptr);
    }

    // MigLayout("fill, ins n n 0 n"): a panel's insets but at the bottom.
    auto* layout = columnLayout(this);
    layout->setContentsMargins(kPanelInset, kPanelInset, kPanelInset, 0);

    //// Preset plot configurations:  ("spanx, split"; the box "growx, wrap 20lp")
    auto* presetLine = makeChild<QWidget>(this);
    presetLine->setObjectName(QStringLiteral("presetLine"));
    QHBoxLayout* presetLayout = lineLayout(presetLine);
    //: simplotpanel.lbl.Presetplotconf
    presetLayout->addWidget(makeChild<QLabel>(presetLine, tr("Preset plot configurations:")));
    presetLayout->addWidget(m_presetBox, 1);
    layout->addWidget(presetLine);
    layout->addSpacing(kPresetGap - kRelatedGap);

    auto* separator = makeChild<QFrame>(this);
    separator->setObjectName(QStringLiteral("separator"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    //// X axis type:  (one split cell: the components' own gaps, "gapright unrel" between them)
    auto* xLine = makeChild<QWidget>(this);
    xLine->setObjectName(QStringLiteral("xAxisLine"));
    QHBoxLayout* xLayout = lineLayout(xLine);
    //: simplotpanel.lbl.Xaxistype
    xLayout->addWidget(makeChild<QLabel>(xLine, tr("X axis type:")));
    // Keep long localized data type names from determining the dialog width ("width 200lp!").
    xLayout->addWidget(m_domainTypeSelector);
    xLayout->addSpacing(kUnrelatedGap - kRelatedGap);
    //: simplotpanel.lbl.Unit
    xLayout->addWidget(makeChild<QLabel>(xLine, tr("Unit:")));
    xLayout->addWidget(m_domainUnitSelector);
    xLayout->addSpacing(kUnrelatedGap - kRelatedGap);
    if (extras.xWidgets.empty())
    {
        xLayout->addStretch(1);
        layout->addWidget(xLine);
        layout->addSpacing(kUnrelatedGap - kRelatedGap);
    }
    else
    {
        xLayout->addWidget(extras.xWidgets.front(), 1);
        layout->addWidget(xLine);
        for (std::size_t i = 1; i < extras.xWidgets.size(); ++i)
        {
            layout->addSpacing(kUnrelatedGap - kRelatedGap);
            layout->addWidget(extras.xWidgets[i]);
        }
    }

    //// Y axis types:  ("pushy, wmin 400lp, grow 100", beside the extras "gapright para")
    auto* yArea = makeChild<QWidget>(this);
    yArea->setObjectName(QStringLiteral("yAxisArea"));
    QHBoxLayout* yAreaLayout = lineLayout(yArea);
    auto*        yPanel      = makeChild<QWidget>(yArea);
    yPanel->setObjectName(QStringLiteral("yPanel"));
    yPanel->setMinimumWidth(kYPanelMinimumWidth);
    QVBoxLayout* yLayout = columnLayout(yPanel);
    //: simplotpanel.lbl.Yaxistypes
    yLayout->addWidget(makeChild<QLabel>(yPanel, tr("Y axis types:")));
    yLayout->addWidget(m_typeSelectorScroll, 1);
    yAreaLayout->addWidget(yPanel, 1);
    if (!extras.yWidgets.empty())
    {
        yAreaLayout->addSpacing(kParagraphGap);
        auto* extraYPanel = makeChild<QWidget>(yArea);
        extraYPanel->setObjectName(QStringLiteral("extraYPanel"));
        QVBoxLayout* extraLayout = columnLayout(extraYPanel);
        for (QWidget* widget : extras.yWidgets)
        {
            extraLayout->addWidget(widget);
        }
        // Fill up the rest of the vertical space.
        extraLayout->addStretch(1);
        yAreaLayout->addWidget(extraYPanel, 1);
    }
    layout->addWidget(yArea, 1);

    //// New Y axis plot type  ("spanx, pushx, left")
    layout->addWidget(m_newYAxisButton, 0, Qt::AlignLeft);
}

void PlotPanel::setConfiguration(const SimulationPlotConfiguration& configuration)
{
    m_configuration = adjustedConfiguration(configuration, m_typesX, m_typesY);
    Q_EMIT configurationChanged();
}

void PlotPanel::setToCustom()
{
    const Modifying modifying(*this);
    m_configuration.setName(std::string(SimulationPlotConfiguration::kCustomName),
                            std::string(SimulationPlotConfiguration::kCustomNameKey));
    m_presetBox->setCurrentIndex(static_cast<int>(customIndex()));
}

void PlotPanel::updatePlots()
{
    m_domainTypeSelector->setSelectedType(&m_configuration.getDomainAxisType());
    m_domainUnitSelector->setUnitGroup(m_configuration.getDomainAxisType().getUnitGroup());
    m_domainUnitSelector->setSelectedUnit(m_configuration.getDomainAxisUnit());

    // typeSelectorPanel.removeAll(): the old rows go (a row's own button may be what called this).
    for (PlotTypeSelector* old : m_selectors)
    {
        old->hide();
        m_typeSelectorLayout->removeWidget(old);
        old->deleteLater();
    }
    m_selectors.clear();
    for (std::size_t i = 0; i < m_configuration.getDataCount(); ++i)
    {
        PlotTypeSelector* selector =
            createSelector(i, m_configuration.getType(i), m_configuration.getUnit(i),
                           m_configuration.getAxis(i), m_typeSelectorPanel);
        QTROCKET_ASSERT(selector != nullptr);
        m_selectors.push_back(selector);
        addSelectionListeners(*selector, i);
        // "wrap": one below the other, as wide as it is, before the stretch below the rows.
        m_typeSelectorLayout->insertWidget(static_cast<int>(i), selector, 0,
                                           Qt::AlignLeft | Qt::AlignTop);
    }
}

void PlotPanel::newYAxisPlotType()
{
    if (m_configuration.getDataCount() >= kMaximumPlots)
    {
        m_services.prompts().showMessage(
            this, {.severity = UserPrompts::Severity::ERROR,
                   //: simplotpanel.OptionPane.lbl2
                   .title = tr("Cannot add plot"),
                   //: simplotpanel.OptionPane.lbl1
                   .lines = QStringList{tr("A maximum of 15 plots is allowed.")}});
        return;
    }

    // Select new type smartly
    const FlightDataType* type = nullptr;
    for (const FlightDataType* candidate : m_typesY)
    {
        bool used = m_configuration.getDomainAxisType().equals(*candidate);
        for (std::size_t i = 0; !used && i < m_configuration.getDataCount(); ++i)
        {
            used = m_configuration.getType(i).equals(*candidate);
        }
        if (!used)
        {
            type = candidate;
            break;
        }
    }
    if (type == nullptr)
    {
        type = m_typesY.front();
    }

    // Add new type
    m_configuration.addPlotDataType(*type);
    setToCustom();
    updatePlots();
}

PlotTypeSelector& PlotPanel::selector(std::size_t index) const
{
    QTROCKET_ASSERT(index < m_selectors.size());
    return *m_selectors[index];
}

QString PlotPanel::configurationText(const SimulationPlotConfiguration& configuration)
{
    return configuration.getNameKey().empty()
               ? QtStrings::fromUtf8(configuration.getName())
               : translatedEnumText(configuration.getNameKey(), configuration.getName());
}

PlotTypeSelector* PlotPanel::createSelector(std::size_t index, const FlightDataType& type,
                                            const Unit& unit, int axis, QWidget* parent)
{
    return makeChild<PlotTypeSelector>(parent, index, type, &unit, axis, m_typesY);
}

void PlotPanel::addSelectionListeners(PlotTypeSelector& selector, std::size_t index)
{
    // Type
    connect(&selector.typeSelector(), &DataTypeComboBox::selectedTypeChanged, &selector,
            guarded([this, &selector, index] { typeSelected(selector, index); },
                    QStringLiteral("Y axis type")));
    // Unit
    connect(&selector.unitSelector(), &UnitSelector::unitSelected, &selector,
            guarded([this, &selector, index] { unitSelected(selector, index); },
                    QStringLiteral("Y axis unit")));
    // Axis
    connect(&selector.axisSelector(), &QComboBox::currentIndexChanged, &selector,
            guarded([this, &selector, index] { axisSelected(selector, index); },
                    QStringLiteral("Y axis")));
    // Remove button
    connect(&selector.removeButton(), &QPushButton::clicked, &selector,
            guarded([this, index] { removeSelected(index); }, QStringLiteral("Remove this plot")));
}

void PlotPanel::presetSelected(int index)
{
    if (isModifying() || index < 0 || static_cast<std::size_t>(index) == customIndex())
    {
        return;
    }
    const Modifying             modifying(*this);
    SimulationPlotConfiguration chosen = m_presets.at(static_cast<std::size_t>(index));
    chosen.resetUnits();
    setConfiguration(chosen);
    updatePlots();
}

void PlotPanel::domainTypeSelected()
{
    if (isModifying())
    {
        return;
    }
    const FlightDataType* type = m_domainTypeSelector->selectedType();
    if (type == nullptr)
    {
        return;
    }
    m_configuration.setDomainAxisType(*type);
    m_domainUnitSelector->setUnitGroup(type->getUnitGroup());
    m_domainUnitSelector->setSelectedUnit(m_configuration.getDomainAxisUnit());
    setToCustom();
}

void PlotPanel::domainUnitSelected()
{
    if (isModifying())
    {
        return;
    }
    m_configuration.setDomainAxisUnit(m_domainUnitSelector->getSelectedUnit());
}

void PlotPanel::typeSelected(PlotTypeSelector& selector, std::size_t index)
{
    const FlightDataType* type = selector.selectedType();
    if (isModifying() || type == nullptr)
    {
        return;
    }
    m_configuration.setPlotDataType(index, *type);
    selector.setUnitGroup(type->getUnitGroup());
    m_configuration.setPlotDataUnit(index, selector.selectedUnit());
    setToCustom();
}

void PlotPanel::unitSelected(PlotTypeSelector& selector, std::size_t index)
{
    if (isModifying())
    {
        return;
    }
    m_configuration.setPlotDataUnit(index, selector.selectedUnit());
}

void PlotPanel::axisSelected(PlotTypeSelector& selector, std::size_t index)
{
    if (isModifying())
    {
        return;
    }
    m_configuration.setPlotDataAxis(index, selector.selectedAxis());
}

void PlotPanel::removeSelected(std::size_t index)
{
    m_configuration.removePlotDataType(index);
    setToCustom();
    updatePlots();
}

}  // namespace QtRocket::Gui
