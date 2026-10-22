#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BoolCheckBox;
class ComponentSelection;
class GuiServices;
class MaterialPanel;
class MotorConfig;

/// The configuration of a body tube (OpenRocket's BodyTubeConfig, swing
/// gui/configdialog/BodyTubeConfig.java): the tabs "General" and "Motor" before the common ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Length:" (at least 0; slider 0, 50 cm, 2 m);
/// - "Outer diameter:" (the outer radius times 2, at least 0; slider 0, 4 cm, 20 cm) and below it
///   "Automatic" (the diameter of the previous or next component);
/// - "Inner diameter:" (the inner radius times 2, at least 0; slider from 0 to the outer diameter,
///   which it follows);
/// - "Wall thickness:" (at least 0; slider 0 to 1 cm) and "Filled";
/// - to the right of them the material and the finish (MaterialPanel, bulk materials).
/// Motor: the motor mount tab (MotorConfig).
///
/// "Automatic", decided once, when the panel is made, by the main component's neighbours (Java:
/// updateCheckboxAutoAftRadius()): without a previous and a next symmetric component it is
/// disabled ("There is no previous/next component to take the diameter of."); when the only
/// neighbours already take their diameter from this tube (a previous one that uses its next
/// component automatically, a next one that uses its previous one, or both) it is disabled too
/// ("The previous/next component already has its auto setting turned on."); otherwise enabled
/// ("Uses the diameter of the previous/next component."). Disabled, it switches the automatic
/// diameter off on every selected body tube (Java's setOuterRadiusAutomatic(false) on the main
/// tube, which its config listeners forward), before the dialog's undo position: the dialog
/// counts no modification for it (ComponentConfigDialog, the user's decision U4).
///
/// Several body tubes (the user's decision U3, ComponentSelection): every field sets each selected
/// tube with its own setter, as OpenRocket's forwarding setters do for these properties (each
/// forwards before its own early return: BodyComponent.setLength, BodyTube.setOuterRadius,
/// setOuterRadiusAutomatic and setInnerRadius, SymmetricComponent.setThickness and setFilled).
/// The inner diameter is the cascade ComponentSelection.h lists (BodyTube.setInnerRadius: each
/// tube's thickness from its own outer radius here, from the main tube's in OpenRocket).
class BodyTubeConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the body tubes of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         BodyTube
    BodyTubeConfig(OpenRocketDocument& document, const GuiServices& services,
                   std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& lengthWidgets() const noexcept
    {
        return m_length;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& outerDiameterWidgets() const noexcept
    {
        return m_outerDiameter;
    }
    /// "Automatic" of the outer diameter.
    [[nodiscard]] BoolCheckBox& automaticCheckBox() const noexcept { return *m_automatic; }
    [[nodiscard]] const ConfigFields::NumberWidgets& innerDiameterWidgets() const noexcept
    {
        return m_innerDiameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& thicknessWidgets() const noexcept
    {
        return m_thickness;
    }
    [[nodiscard]] BoolCheckBox&  filledCheckBox() const noexcept { return *m_filled; }
    [[nodiscard]] MaterialPanel& materialPanel() const noexcept { return *m_material; }
    [[nodiscard]] MotorConfig&   motorConfig() const noexcept { return *m_motor; }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// Decides the state of "Automatic" (see the class comment).
    void updateAutomaticCheckBox();

    ConfigFields::NumberWidgets m_length;
    ConfigFields::NumberWidgets m_outerDiameter;
    BoolCheckBox*               m_automatic{nullptr};
    ConfigFields::NumberWidgets m_innerDiameter;
    ConfigFields::NumberWidgets m_thickness;
    BoolCheckBox*               m_filled{nullptr};
    MaterialPanel*              m_material{nullptr};
    MotorConfig*                m_motor{nullptr};
};

}  // namespace QtRocket::Gui
