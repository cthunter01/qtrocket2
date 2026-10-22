#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/TransitionShape.h"

class QGridLayout;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BoolCheckBox;
class ComponentSelection;
class DescriptionArea;
class EnumComboBox;
class GuiServices;
class MaterialPanel;
class ShoulderPanel;

/// The configuration of a transition (OpenRocket's TransitionConfig, swing
/// gui/configdialog/TransitionConfig.java): the tabs "General" and "Shoulder" before the common
/// ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Transition shape:", the shape (makeTransitionShapeBinding(): every shape, Java's
///   TransitionShapeModel) and beside it "Clipped" (enabled for a shape that can be clipped,
///   Transition::isClippedEnabled());
/// - "Shape parameter:" (0 to 1, stepping by 0.1, no unit selector; slider over the shape's range),
///   label, spin box and slider enabled only while the shape uses the parameter;
/// - "Length:" (at least 0; slider 0, 5 cm, 30 cm);
/// - "Fore diameter:" and "Aft diameter:" (the radius times 2, at least 0; slider 0, 4 cm, 20 cm),
///   each with "Automatic" below it (the diameter of the previous or the next component);
/// - "Wall thickness:" (at least 0; slider from 0 to the largest radius) and "Filled";
/// - to the right the description of the shape (Shape.getTransitionDescription(), five rows)
///   above the material and the finish (MaterialPanel, bulk materials).
/// The description and the shape parameter's enabled state follow the shape whenever it changes
/// (Java: the shape combo box's action listener).
/// Shoulder: the fore and the aft shoulder (ShoulderPanel).
///
/// Each "Automatic" is decided once, when the panel is made, by the main transition (Java:
/// updateCheckboxAutoForeRadius() and updateCheckboxAutoAftRadius()): enabled ("Uses the diameter
/// of the previous/next component.") when the transition can take that radius from its neighbour
/// (Transition::canUsePreviousCompAutomatic(), canUseNextCompAutomatic()); otherwise disabled,
/// with "There is no previous/next component to take the diameter of." when it has no neighbour
/// on that side and "The previous/next component already has its auto setting turned on." when
/// the neighbour takes its own diameter from this transition, and that radius's automatic state
/// is switched off on every selected transition (Java's setForeRadiusAutomatic(false) or
/// setAftRadiusAutomatic(false) on the main one, forwarded to its config listeners), before the
/// dialog's undo position (no modification for the dialog: ComponentConfigDialog, the user's
/// decision U4).
///
/// Several transitions (the user's decision U3, ComponentSelection): every field sets each
/// selected transition with its own setter. OpenRocket's setters of these properties forward
/// before their own early return, except where it reaches the others differently:
/// - the fore and aft diameters: Transition.setForeRadius and setAftRadius (the main's shoulder
///   radius, the cascade ComponentSelection.h lists);
/// - the length: Java's Transition.setLength() returns before it forwards when the main
///   transition has the length already (its early return comes before BodyComponent.setLength()'s
///   loop), so that the others keep theirs; here each gets the length.
class TransitionConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the transitions of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         Transition
    TransitionConfig(OpenRocketDocument& document, const GuiServices& services,
                     std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] EnumComboBox& shapeComboBox() const noexcept { return *m_shape; }
    [[nodiscard]] BoolCheckBox& clippedCheckBox() const noexcept { return *m_clipped; }
    [[nodiscard]] const ConfigFields::NumberWidgets& shapeParameterWidgets() const noexcept
    {
        return m_shapeParameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& lengthWidgets() const noexcept
    {
        return m_length;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& foreDiameterWidgets() const noexcept
    {
        return m_foreDiameter;
    }
    /// "Automatic" of the fore diameter.
    [[nodiscard]] BoolCheckBox& foreAutomaticCheckBox() const noexcept { return *m_foreAutomatic; }
    [[nodiscard]] const ConfigFields::NumberWidgets& aftDiameterWidgets() const noexcept
    {
        return m_aftDiameter;
    }
    /// "Automatic" of the aft diameter.
    [[nodiscard]] BoolCheckBox& aftAutomaticCheckBox() const noexcept { return *m_aftAutomatic; }
    [[nodiscard]] const ConfigFields::NumberWidgets& thicknessWidgets() const noexcept
    {
        return m_thickness;
    }
    [[nodiscard]] BoolCheckBox&    filledCheckBox() const noexcept { return *m_filled; }
    [[nodiscard]] DescriptionArea& description() const noexcept { return *m_description; }
    [[nodiscard]] MaterialPanel&   materialPanel() const noexcept { return *m_material; }
    [[nodiscard]] ShoulderPanel&   shoulderPanel() const noexcept { return *m_shoulder; }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// The shape's row (with "Clipped") and the shape parameter's.
    void addShapeRows(QWidget& page, QGridLayout& grid);
    /// The fore and the aft diameter, each with "Automatic".
    void addDiameterRows(QWidget& page, QGridLayout& grid);
    /// The column to the right of the fields: the description and the material.
    [[nodiscard]] QWidget& makeDescriptionColumn(QWidget& page);
    /// The description and the shape parameter's enabled state for the shape shown.
    void updateShape();
    /// Decide the state of the fore and the aft "Automatic" (see the class comment).
    void updateForeAutomaticCheckBox();
    void updateAftAutomaticCheckBox();

    std::shared_ptr<EnumBinding<TransitionShape>> m_shapeBinding;
    EnumComboBox*                                 m_shape{nullptr};
    BoolCheckBox*                                 m_clipped{nullptr};
    ConfigFields::NumberWidgets                   m_shapeParameter;
    ConfigFields::NumberWidgets                   m_length;
    ConfigFields::NumberWidgets                   m_foreDiameter;
    BoolCheckBox*                                 m_foreAutomatic{nullptr};
    ConfigFields::NumberWidgets                   m_aftDiameter;
    BoolCheckBox*                                 m_aftAutomatic{nullptr};
    ConfigFields::NumberWidgets                   m_thickness;
    BoolCheckBox*                                 m_filled{nullptr};
    DescriptionArea*                              m_description{nullptr};
    MaterialPanel*                                m_material{nullptr};
    ShoulderPanel*                                m_shoulder{nullptr};
};

}  // namespace QtRocket::Gui
