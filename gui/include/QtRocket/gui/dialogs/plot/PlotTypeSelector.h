#pragma once

#include <cstddef>
#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

class QComboBox;
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;

namespace QtRocket
{
class FlightDataType;
class Unit;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DataTypeComboBox;
class UnitSelector;

/// One Y axis row of the plot tab (OpenRocket's PlotTypeSelector, swing gui/plot/): the data
/// type of row index() of the plot configuration (typeSelector(), a DataTypeComboBox of the types
/// the panel offers, the row's type selected), "Unit:" and its unit (unitSelector(), a
/// UnitSelector over the type's unit group, the row's unit selected), "Axis:" and its axis
/// (axisSelector(): "Auto", "Left", "Right", simplotpanel.AUTO_NAME, LEFT_NAME, RIGHT_NAME; the
/// item index is the axis + 1, Util.PlotAxisSelection), and a flat button with the delete icon,
/// "Remove this plot" (removeButton()). SimulationPlotTypeSelector adds a second line, the
/// row's colour and line style.
///
/// The row only shows and picks: the panel (PlotPanel) listens to the children's signals (Java:
/// the addXListener() methods add the panel's listeners to the children) and changes the
/// configuration. setUnitGroup() gives the unit selector another group (the type changed).
///
/// Layout (Java's MigLayout "ins 0", each part in a cell of its own, all at the top): the type
/// selector, kDataTypeSelectorWidth wide whatever its name ("width 200lp!",
/// DATA_TYPE_SELECTOR_WIDTH), then a compact gap ("gapright unrel": kUnrelatedGap after the
/// layout's kRelatedGap between cells), "Unit:", the unit selector at least kUnitSelectorWidth
/// wide ("width 40lp"), the same compact gap, "Axis:", the axis box and the remove button, each
/// kRelatedGap apart; a second line (addLine()) below the first, kRelatedGap lower, indented by
/// kParagraphGap ("newline, gapleft para"). The row is as wide as its contents.
///
/// Deviations from OpenRocket: "Unit:", "Axis:" and "Remove this plot" are Java literals that
/// OpenRocket does not translate; here they go through tr() (their translator comments say so), as
/// "Unit:" and "Axis:" are also the texts of simplotpanel.lbl.Unit and simplotpanel.lbl.Axis. The
/// row is not generic over the data type (Java: PlotTypeSelector<T, G>, which Milestone 7's
/// component analysis plots also use): it offers FlightDataTypes, as DataTypeComboBox does.
/// MigLayout's paragraph gap is QtRocket's kParagraphGap (12 px, as in the other panels; 18 px
/// under OpenRocket on Linux).
class PlotTypeSelector : public QWidget
{
    Q_OBJECT

public:
    /// PlotTypeSelector.DATA_TYPE_SELECTOR_WIDTH ("200lp!"): the type selector's width, px
    /// (DataTypeComboBox::kFixedWidth).
    static constexpr int kDataTypeSelectorWidth = 200;
    /// "width 40lp": the unit selector's least width, px.
    static constexpr int kUnitSelectorWidth = 40;
    /// MigLayout's related gap ("rel"), between the cells of a line and between the lines, px.
    static constexpr int kRelatedGap = 6;
    /// MigLayout's unrelated gap ("unrel"), px.
    static constexpr int kUnrelatedGap = 12;
    /// MigLayout's paragraph gap ("para"), px.
    static constexpr int kParagraphGap = 12;

    /// The row @p plotIndex of a configuration, showing @p type of @p availableTypes (which need
    /// not hold it: DataTypeComboBox shows a type that is none of its items), @p unit (null: the
    /// unit group's default unit) and @p position (0 left, 1 right, negative automatic), with the
    /// remove button unless @p addRemoveButton is false (Java: a subclass adds it later,
    /// addRemoveButton()).
    /// @throws BugError when @p unit is not one of @p type's unit group, or a type is null
    PlotTypeSelector(std::size_t plotIndex, const FlightDataType& type, const Unit* unit,
                     int position, const std::vector<const FlightDataType*>& availableTypes,
                     bool addRemoveButton, QWidget* parent);
    /// The same with the remove button.
    PlotTypeSelector(std::size_t plotIndex, const FlightDataType& type, const Unit* unit,
                     int position, const std::vector<const FlightDataType*>& availableTypes,
                     QWidget* parent);
    ~PlotTypeSelector() override;
    PlotTypeSelector(const PlotTypeSelector&)            = delete;
    PlotTypeSelector& operator=(const PlotTypeSelector&) = delete;
    PlotTypeSelector(PlotTypeSelector&&)                 = delete;
    PlotTypeSelector& operator=(PlotTypeSelector&&)      = delete;

    /// The index of the row in the configuration (Java: getIndex()).
    [[nodiscard]] std::size_t index() const noexcept { return m_index; }

    /// The selected type (null for none, which no user choice gives).
    [[nodiscard]] const FlightDataType* selectedType() const noexcept;
    /// The selected unit.
    [[nodiscard]] const Unit& selectedUnit() const noexcept;
    /// The selected axis: -1 automatic, 0 left, 1 right (the axis box's index - 1).
    [[nodiscard]] int selectedAxis() const noexcept;
    /// Gives the unit selector @p unitGroup, its default unit selected (UnitSelector::
    /// setUnitGroup(): nothing for the group it has).
    void setUnitGroup(const UnitGroup& unitGroup);

    /// The children, whose signals the panel listens to: the type ("typeSelector",
    /// DataTypeComboBox::selectedTypeChanged()), the unit ("unitSelector", UnitSelector::
    /// unitSelected()), the axis ("axisSelector", QComboBox::currentIndexChanged()) and the remove
    /// button ("removeButton", clicked()).
    [[nodiscard]] DataTypeComboBox& typeSelector() const noexcept { return *m_typeSelector; }
    [[nodiscard]] UnitSelector&     unitSelector() const noexcept { return *m_unitSelector; }
    [[nodiscard]] QComboBox&        axisSelector() const noexcept { return *m_axisSelector; }
    [[nodiscard]] QPushButton&      removeButton() const noexcept { return *m_removeButton; }

    /// The texts of the axis box's items, in order (Util.PlotAxisSelection's names).
    [[nodiscard]] static std::vector<QString> axisNames();

protected:
    /// Puts the remove button at the end of the first line (Java: addRemoveButton(), "gapright 0").
    void addRemoveButton();

    /// Adds @p line below the first line (and any line added before), indented by
    /// kParagraphGap: the subclass's second line ("newline, gapleft para, spanx, wrap").
    void addLine(QWidget* line);

private:
    std::size_t  m_index;
    QVBoxLayout* m_lines;
    /// The first line ("firstLine") and its layout.
    QWidget*          m_firstLine;
    QHBoxLayout*      m_firstLineLayout;
    DataTypeComboBox* m_typeSelector;
    UnitSelector*     m_unitSelector;
    QComboBox*        m_axisSelector;
    QPushButton*      m_removeButton;
};

}  // namespace QtRocket::Gui
