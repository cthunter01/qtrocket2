#pragma once

#include <cstddef>
#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"

class QComboBox;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace QtRocket
{
class FlightDataType;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DataTypeComboBox;
class PlotTypeSelector;
class UnitSelector;

/// The plot configuration panel (OpenRocket's PlotPanel, swing gui/plot/): the part of a plot tab
/// that edits what a plot shows, its configuration (getConfiguration()). SimulationPlotPanel is
/// the simulations' "Plot data" tab, with the flight events and the rows' looks.
///
/// From top to bottom:
/// - "Preset plot configurations:" (simplotpanel.lbl.Presetplotconf) and the preset box
///   ("configurationSelector"): the presets, the last of them the custom configuration ("Custom",
///   which only stands for any configuration that is not a preset). Choosing a preset (Java's item
///   listener) makes the configuration a copy of it in the default units (setConfiguration() of
///   SimulationPlotConfiguration::resetUnits()), then shows it (updatePlots()); choosing "Custom"
///   changes nothing (not even the configuration's name). Then kPresetGap, and a separator.
/// - "X axis type:" (simplotpanel.lbl.Xaxistype), the domain type ("domainTypeSelector", a
///   DataTypeComboBox of the X types), "Unit:" (simplotpanel.lbl.Unit) and its unit
///   ("domainUnitSelector", a UnitSelector over the type's unit group, at least
///   PlotTypeSelector::kUnitSelectorWidth wide), each a compact gap after the one before
///   (PlotTypeSelector::kUnrelatedGap: Java's "gapright unrel" in a split cell), then the extra X
///   widgets (growing; the first on this line, the others on lines of their own).
/// - "Y axis types:" (simplotpanel.lbl.Yaxistypes) over a scroll area of the rows
///   ("typeSelectorPanel": a PlotTypeSelector per row of the configuration, from the top), at
///   least kYPanelMinimumWidth wide, and beside it, a paragraph gap further, the extra Y widgets,
///   one below the other; both share the panel's width and take its height.
/// - "New Y axis plot type" (simplotpanel.but.NewYaxisplottype, "newYAxisBtn"), at the left: adds a
///   row (newYAxisPlotType()).
///
/// The edits (Java's listeners, which do nothing while the panel itself sets its controls:
/// isModifying()): the domain type sets the configuration's domain and its unit selector's group,
/// and makes the configuration "Custom" (setToCustom()); the domain unit sets the domain's unit; a
/// row's type sets the row's type and its unit selector's group (the unit: the group's default
/// when the group changed), and makes it "Custom"; a row's unit and axis set them (no "Custom");
/// a row's remove button removes it and makes the configuration "Custom". Programming errors in
/// them are reported through Guard.
///
/// OpenRocket's behaviour, kept (Java's own, though they look like slips): setConfiguration() does
/// not change the preset box, so a preset whose types the data lacks shows the preset while the
/// configuration is "Custom"; and choosing "Custom" keeps the configuration's name.
///
/// Deviations from OpenRocket:
/// - Removing the last row works (Java's plot tab fails there with an internal error: the core's
///   PlotConfiguration, an OpenRocket bug fixed in Milestone 4's run 1).
/// - The panel is not generic over the data type and configuration (Java: PlotPanel<T, B, G, C,
///   S>, which Milestone 7's component analysis plots also use): it edits a
///   SimulationPlotConfiguration of FlightDataTypes. The extra widgets are given to the deferred
///   constructor's build() (Java: arrays given to the constructor, null for none), and the custom
///   configuration is the last preset (Java: a separate argument, which both of OpenRocket's
///   callers make the last preset). Java's PlotConfigurationListener is configurationChanged().
/// - The rows of an earlier updatePlots() are hidden and deleted later (deleteLater()), not at
/// once.
class PlotPanel : public QWidget
{
    Q_OBJECT

public:
    using DataTypes      = std::vector<const FlightDataType*>;
    using Configurations = std::vector<SimulationPlotConfiguration>;

    /// The most rows a configuration may get through New Y axis plot type (simplotpanel.
    /// OptionPane.lbl1, "A maximum of 15 plots is allowed.").
    static constexpr std::size_t kMaximumPlots = 15;
    /// "wrap 20lp": the gap below the preset box, px.
    static constexpr int kPresetGap = 20;
    /// "wmin 400lp": the least width of the rows' panel, px.
    static constexpr int kYPanelMinimumWidth = 400;

    /// The panel of Java's PlotPanel(typesX, typesY, customConfiguration, presets,
    /// defaultConfiguration, null, null), built at once without extra widgets: the X axis types
    /// @p typesX and the Y axis types @p typesY (in the order the DataTypeComboBoxes take them),
    /// the preset box's @p presets, the last of them the custom configuration, and the
    /// configuration to start from, @p defaultConfiguration (setConfiguration()). Its questions
    /// (the limit of the rows) go through @p services' prompts. No row is shown before
    /// updatePlots() (Java's PlotPanel calls it only from its subclass).
    /// @throws BugError when @p typesX, @p typesY or @p presets is empty, or holds a null type
    PlotPanel(const GuiServices& services, DataTypes typesX, DataTypes typesY,
              Configurations presets, const SimulationPlotConfiguration& defaultConfiguration,
              QWidget* parent = nullptr);
    ~PlotPanel() override;
    PlotPanel(const PlotPanel&)            = delete;
    PlotPanel& operator=(const PlotPanel&) = delete;
    PlotPanel(PlotPanel&&)                 = delete;
    PlotPanel& operator=(PlotPanel&&)      = delete;

    /// The configuration being edited (Java: getConfiguration()).
    [[nodiscard]] const SimulationPlotConfiguration& getConfiguration() const noexcept
    {
        return m_configuration;
    }

    /// Makes a copy of @p configuration the one being edited (Java's protected
    /// setConfiguration()): its domain type becomes the first X type unless it is one of them
    /// (FlightDataType::equals()), the rows whose type is not one of the Y types are removed, and
    /// when either happened its name becomes "Custom" (the preset box keeps its choice); then
    /// configurationChanged(). The controls show it after updatePlots().
    void setConfiguration(const SimulationPlotConfiguration& configuration);

    /// Shows the configuration (Java's protected updatePlots()): the domain type and unit, and
    /// the rows, made again (createSelector(), addSelectionListeners()).
    virtual void updatePlots();

    /// Adds a row of the first Y type that is neither the domain type nor a row's (equals()), or
    /// of the first Y type when every one is used, on an automatic axis, makes the configuration
    /// "Custom" and shows it (New Y axis plot type). With kMaximumPlots rows already, it shows
    /// the error "A maximum of 15 plots is allowed.", titled "Cannot add plot" (simplotpanel.
    /// OptionPane.lbl1, lbl2), instead.
    void newYAxisPlotType();

    /// The X and Y axis types, and the presets (the custom configuration last).
    [[nodiscard]] const DataTypes&      typesX() const noexcept { return m_typesX; }
    [[nodiscard]] const DataTypes&      typesY() const noexcept { return m_typesY; }
    [[nodiscard]] const Configurations& presets() const noexcept { return m_presets; }
    /// The index of the custom configuration in presets() and the preset box: the last.
    [[nodiscard]] std::size_t customIndex() const noexcept { return m_presets.size() - 1; }

    /// The controls (see the class comment).
    [[nodiscard]] QComboBox&        configurationSelector() const noexcept { return *m_presetBox; }
    [[nodiscard]] DataTypeComboBox& domainTypeSelector() const noexcept
    {
        return *m_domainTypeSelector;
    }
    [[nodiscard]] UnitSelector& domainUnitSelector() const noexcept
    {
        return *m_domainUnitSelector;
    }
    [[nodiscard]] QWidget&     typeSelectorPanel() const noexcept { return *m_typeSelectorPanel; }
    [[nodiscard]] QPushButton& newYAxisButton() const noexcept { return *m_newYAxisButton; }

    /// The rows shown, one per row of the configuration as updatePlots() made them.
    [[nodiscard]] std::size_t selectorCount() const noexcept { return m_selectors.size(); }
    /// Row @p index.
    /// @throws BugError when there is no row @p index
    [[nodiscard]] PlotTypeSelector& selector(std::size_t index) const;

    /// The text the preset box shows for @p configuration: its name in the user's language
    /// (translatedEnumText() of its getNameKey(), or the name as it is without a key).
    [[nodiscard]] static QString configurationText(
        const SimulationPlotConfiguration& configuration);

Q_SIGNALS:
    /// setConfiguration() made another configuration the one edited (Java:
    /// PlotConfigurationListener.onPlotConfigurationChanged()).
    void configurationChanged();

protected:
    /// The tag of the constructor that leaves the layout to build().
    struct DeferredBuild
    { };

    /// The extra widgets (Java's extraWidgetsX and extraWidgetsY): made by the subclass as its
    /// children; build() takes them into its layout.
    struct Extras
    {
        /// After the domain unit, growing: the first on the X axis line, each other on a line of
        /// its own below it, kUnrelatedGap apart (Java: "growx, wrap unrel", the last "growx,
        /// wrap"). None: the line ends there, kUnrelatedGap above the rows (Java: an empty label,
        /// "wrap unrel").
        std::vector<QWidget*> xWidgets;
        /// Beside the rows, one below the other, at the top (Java: "growx, wrap rel" in a panel
        /// that fills the rest of its height). None: the rows take the whole width.
        std::vector<QWidget*> yWidgets;
    };

    /// The panel of the public constructor, its controls made but not laid out: the subclass makes
    /// its extra widgets, then calls build().
    PlotPanel(DeferredBuild deferred, const GuiServices& services, DataTypes typesX,
              DataTypes typesY, Configurations presets,
              const SimulationPlotConfiguration& defaultConfiguration, QWidget* parent);

    /// Lays the controls and @p extras out (once).
    /// @throws BugError when called again, or for a null extra widget
    void build(const Extras& extras);

    /// The configuration being edited, to change.
    [[nodiscard]] SimulationPlotConfiguration& configuration() noexcept { return m_configuration; }

    [[nodiscard]] const GuiServices& services() const noexcept { return m_services; }

    /// Whether the panel is setting its controls itself (Java: modifying > 0): the edits'
    /// listeners do nothing then.
    [[nodiscard]] bool isModifying() const noexcept { return m_modifying > 0; }

    /// The panel sets its controls while it lives (Java: modifying++ ... modifying--).
    class Modifying
    {
    public:
        explicit Modifying(PlotPanel& panel) noexcept : m_panel(&panel) { ++m_panel->m_modifying; }
        ~Modifying() { --m_panel->m_modifying; }
        Modifying(const Modifying&)            = delete;
        Modifying& operator=(const Modifying&) = delete;
        Modifying(Modifying&&)                 = delete;
        Modifying& operator=(Modifying&&)      = delete;

    private:
        PlotPanel* m_panel;
    };

    /// Names the configuration "Custom" (simplotpanel.CUSTOM) and chooses the custom
    /// configuration in the preset box, which does nothing then (Java: setToCustom()).
    void setToCustom();

    /// Makes the row @p index of @p type, @p unit and @p axis (the configuration's), a child of
    /// @p parent (Java: createSelector()): a PlotTypeSelector of the Y types.
    [[nodiscard]] virtual PlotTypeSelector* createSelector(std::size_t           index,
                                                           const FlightDataType& type,
                                                           const Unit& unit, int axis,
                                                           QWidget* parent);

    /// Connects the edits of row @p index, @p selector (Java: addSelectionListeners(); see the
    /// class comment). A subclass that connects its own before calling this has them called first.
    virtual void addSelectionListeners(PlotTypeSelector& selector, std::size_t index);

private:
    /// The edits of the X axis and of the row @p index.
    void domainTypeSelected();
    void domainUnitSelected();
    void presetSelected(int index);
    void typeSelected(PlotTypeSelector& selector, std::size_t index);
    void unitSelected(PlotTypeSelector& selector, std::size_t index);
    void axisSelected(PlotTypeSelector& selector, std::size_t index);
    void removeSelected(std::size_t index);

    /// The preset box, made with its items and connected.
    [[nodiscard]] QComboBox* makePresetBox();

    GuiServices                    m_services;
    DataTypes                      m_typesX;
    DataTypes                      m_typesY;
    Configurations                 m_presets;
    SimulationPlotConfiguration    m_configuration;
    int                            m_modifying = 0;
    QComboBox*                     m_presetBox;
    DataTypeComboBox*              m_domainTypeSelector;
    UnitSelector*                  m_domainUnitSelector;
    QScrollArea*                   m_typeSelectorScroll;
    QWidget*                       m_typeSelectorPanel;
    QVBoxLayout*                   m_typeSelectorLayout;
    QPushButton*                   m_newYAxisButton;
    std::vector<PlotTypeSelector*> m_selectors;
    bool                           m_built = false;
};

}  // namespace QtRocket::Gui
