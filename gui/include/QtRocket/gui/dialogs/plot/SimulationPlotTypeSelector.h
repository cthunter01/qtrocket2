#pragma once

#include <cstddef>
#include <vector>

#include <QColor>
#include <QObject>

#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/util/LineStyle.h"

class QPushButton;
class QWidget;

namespace QtRocket
{
class FlightDataType;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ColorChooserButton;
class LineStyleComboBox;
class UserPrompts;

/// A Y axis row of a simulation's plot tab (OpenRocket's SimulationPlotTypeSelector, swing
/// gui/simulation/): PlotTypeSelector's line, then a second line ("appearancePanel") with the
/// row's look: "Color:" (simplotpanel.lbl.Color) and its colour (colorButton(), a
/// ColorChooserButton, which asks UserPrompts for another), "Line style:"
/// (simplotpanel.lbl.LineStyle) and its line style (lineStyleSelector(), a LineStyleComboBox), and
/// two icon buttons: "Set as default" (setDefaultButton(), the SET_AS_DEFAULT icon, the tooltip
/// simplotpanel.but.SetAsDefault.ttip) and "Reset to factory default" (resetToFactoryButton(), the
/// RESET icon, simplotpanel.but.ResetToFactory.ttip). As the first line's children, they only show
/// and pick: SimulationPlotPanel listens to their signals (ColorChooserButton::
/// selectedColorChanged(), LineStyleComboBox::lineStyleSelected(), the buttons' clicked()) and
/// sets the buttons' states (updateButtonStates()).
///
/// Layout (Java's MigLayout "ins 0" of the second line, at the top): "Color:" and the colour
/// kRelatedGap apart, then "Line style:" a paragraph gap further ("gapleft para": kParagraphGap
/// after the layout's kRelatedGap), the line style, then the two buttons, the first a paragraph gap
/// further, the second kRelatedGap after it.
class SimulationPlotTypeSelector final : public PlotTypeSelector
{
    Q_OBJECT

public:
    /// The row of PlotTypeSelector's constructor (with the remove button), its colour @p
    /// initialColor (invalid: none) and its line style @p initialLineStyle; the colour button asks
    /// @p prompts, which must outlive the row.
    /// @throws BugError as PlotTypeSelector's constructor
    SimulationPlotTypeSelector(std::size_t plotIndex, const FlightDataType& type, const Unit* unit,
                               int                                       position,
                               const std::vector<const FlightDataType*>& availableTypes,
                               const QColor& initialColor, LineStyle initialLineStyle,
                               UserPrompts& prompts, QWidget* parent);
    ~SimulationPlotTypeSelector() override;
    SimulationPlotTypeSelector(const SimulationPlotTypeSelector&)            = delete;
    SimulationPlotTypeSelector& operator=(const SimulationPlotTypeSelector&) = delete;
    SimulationPlotTypeSelector(SimulationPlotTypeSelector&&)                 = delete;
    SimulationPlotTypeSelector& operator=(SimulationPlotTypeSelector&&)      = delete;

    /// The colour shown (Java: getSelectedColor()).
    [[nodiscard]] const QColor& selectedColor() const noexcept;
    /// Shows @p color (ColorChooserButton::setSelectedColor(): an invalid one or the one shown
    /// changes nothing and tells of nothing).
    void setSelectedColor(const QColor& color);

    /// The line style shown (Java: getSelectedLineStyle()).
    [[nodiscard]] LineStyle selectedLineStyle() const;
    /// Shows @p style (LineStyleComboBox::setSelectedLineStyle(), which tells of it always).
    void setSelectedLineStyle(LineStyle style);

    /// Enables "Reset to factory default" unless @p isAtFactoryDefault (the row's look is
    /// PlotAppearance::factoryDefault()) and "Set as default" unless @p isAtAppDefault (it is the
    /// application's default of the type already).
    void updateButtonStates(bool isAtFactoryDefault, bool isAtAppDefault);

    /// The second line's children ("colorButton", "lineStyleSelector", "setDefaultButton",
    /// "resetToFactoryButton").
    [[nodiscard]] ColorChooserButton& colorButton() const noexcept { return *m_colorButton; }
    [[nodiscard]] LineStyleComboBox&  lineStyleSelector() const noexcept
    {
        return *m_lineStyleSelector;
    }
    [[nodiscard]] QPushButton& setDefaultButton() const noexcept { return *m_setDefaultButton; }
    [[nodiscard]] QPushButton& resetToFactoryButton() const noexcept
    {
        return *m_resetToFactoryButton;
    }

private:
    /// The second line, its children made.
    [[nodiscard]] QWidget* makeAppearancePanel();

    ColorChooserButton* m_colorButton;
    LineStyleComboBox*  m_lineStyleSelector;
    QPushButton*        m_setDefaultButton;
    QPushButton*        m_resetToFactoryButton;
};

}  // namespace QtRocket::Gui
