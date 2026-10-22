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
class BoolBinding;
class ComponentSelection;
class DescriptionArea;
class EnumComboBox;
class GuiServices;
class MaterialPanel;
class ShoulderPanel;

/// The configuration of a nose cone (OpenRocket's NoseConeConfig, swing
/// gui/configdialog/NoseConeConfig.java): the tabs "General" and "Shoulder" before the common
/// ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Nose cone shape:" and the shape (makeTransitionShapeBinding(): every shape, Java's
///   TransitionShapeModel);
/// - "Shape parameter:" (0 to 1, stepping by 0.1, no unit selector; slider over the shape's range),
///   label, spin box and slider enabled only while the shape uses the parameter;
/// - "Length:" (at least 0; slider 0, 10 cm, 70 cm);
/// - "Base diameter:" (the base radius times 2, at least 0; slider 0, 4 cm, 20 cm) and below it
///   "Automatic" (the diameter of the next component);
/// - "Wall thickness:" (at least 0; slider from 0 to the largest radius) and "Filled";
/// - "Flip to tail cone";
/// - to the right the description of the shape (Shape.getNoseConeDescription(), five rows) above
///   the material and the finish (MaterialPanel, bulk materials).
/// The description and the shape parameter's enabled state follow the shape whenever it changes
/// (Java: the shape combo box's action listener).
/// Shoulder: the nose cone's shoulder (ShoulderPanel).
///
/// "Automatic" is decided by the main nose cone's neighbour on its base side, the next symmetric
/// component (the previous one once flipped) (Java: updateCheckboxAutoBaseRadius()): without one
/// it is disabled ("There is no next component to take the diameter of."); when that one already
/// takes its diameter from this nose cone it is disabled too ("The next component already has its
/// auto setting turned on."); otherwise enabled ("Uses the diameter of the next component.").
/// Disabled, it switches the automatic base diameter off on every selected nose cone (Java's
/// setBaseRadiusAutomatic(false) on the main one, forwarded to its config listeners). It is
/// decided when the panel is made, before the dialog's undo position (no modification for the
/// dialog: ComponentConfigDialog, the user's decision U4), and again whenever the user toggles
/// "Flip to tail cone", for the new orientation and, as in Java (its item listener runs before
/// the check box's action sets the component), before the nose cones are flipped.
///
/// Several nose cones (the user's decision U3, ComponentSelection): every field sets each
/// selected nose cone with its own setter. OpenRocket's setters of these properties forward
/// before their own early return, but three of them reach the others through the main one's
/// orientation or values, so that here each nose cone differs where it is not oriented like the
/// main one or has other values:
/// - the base diameter and its automatic state (also the switch-off above): Java's
///   NoseCone.setBaseRadius() and setBaseRadiusAutomatic() call setAftRadius() and
///   setAftRadiusAutomatic() of the main nose cone (setForeRadius...() once flipped), which forward
///   those to the others whatever their orientation; here each sets its own base (the Shoulder
///   tab's NoseCone.setShoulder...() likewise);
/// - "Flip to tail cone": NoseCone.setFlipped (the main's radii and shoulders, the cascade
///   ComponentSelection.h lists);
/// - the length: Java's Transition.setLength() returns before it forwards when the main nose
///   cone has the length already (its early return comes before BodyComponent.setLength()'s
///   loop), so that the others keep theirs; here each gets the length.
/// The flip check box toggled while its binding refreshes (a change of the flip from elsewhere,
/// which no other part of the program makes while the dialog is open) leaves "Automatic" as it
/// is; Java's item listener would decide it again inside that change.
class NoseConeConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the nose cones of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         NoseCone
    NoseConeConfig(OpenRocketDocument& document, const GuiServices& services,
                   std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] EnumComboBox& shapeComboBox() const noexcept { return *m_shape; }
    [[nodiscard]] const ConfigFields::NumberWidgets& shapeParameterWidgets() const noexcept
    {
        return m_shapeParameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& lengthWidgets() const noexcept
    {
        return m_length;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& baseDiameterWidgets() const noexcept
    {
        return m_baseDiameter;
    }
    /// "Automatic" of the base diameter.
    [[nodiscard]] BoolCheckBox& automaticCheckBox() const noexcept { return *m_automatic; }
    [[nodiscard]] const ConfigFields::NumberWidgets& thicknessWidgets() const noexcept
    {
        return m_thickness;
    }
    [[nodiscard]] BoolCheckBox&    filledCheckBox() const noexcept { return *m_filled; }
    [[nodiscard]] BoolCheckBox&    flipCheckBox() const noexcept { return *m_flip; }
    [[nodiscard]] DescriptionArea& description() const noexcept { return *m_description; }
    [[nodiscard]] MaterialPanel&   materialPanel() const noexcept { return *m_material; }
    [[nodiscard]] ShoulderPanel&   shoulderPanel() const noexcept { return *m_shoulder; }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// The shape's row and the shape parameter's.
    void addShapeRows(QWidget& page, QGridLayout& grid);
    /// The column to the right of the fields: the description and the material.
    [[nodiscard]] QWidget& makeDescriptionColumn(QWidget& page);
    /// The description and the shape parameter's enabled state for the shape shown.
    void updateShape();
    /// Decides the state of "Automatic" for the orientation @p flipped (see the class comment).
    void updateAutomaticCheckBox(bool flipped);
    /// "Flip to tail cone" was toggled to @p flipped.
    void flipToggled(bool flipped);

    std::shared_ptr<EnumBinding<TransitionShape>> m_shapeBinding;
    std::shared_ptr<BoolBinding>                  m_flipBinding;
    EnumComboBox*                                 m_shape{nullptr};
    ConfigFields::NumberWidgets                   m_shapeParameter;
    ConfigFields::NumberWidgets                   m_length;
    ConfigFields::NumberWidgets                   m_baseDiameter;
    BoolCheckBox*                                 m_automatic{nullptr};
    ConfigFields::NumberWidgets                   m_thickness;
    BoolCheckBox*                                 m_filled{nullptr};
    BoolCheckBox*                                 m_flip{nullptr};
    DescriptionArea*                              m_description{nullptr};
    MaterialPanel*                                m_material{nullptr};
    ShoulderPanel*                                m_shoulder{nullptr};
};

}  // namespace QtRocket::Gui
