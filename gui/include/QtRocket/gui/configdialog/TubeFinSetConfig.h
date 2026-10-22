#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

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
class GuiServices;
class IntSpinBox;
class MaterialPanel;
class PlacementPanel;

/// The configuration of a tube fin set (OpenRocket's TubeFinSetConfig, swing
/// gui/configdialog/TubeFinSetConfig.java): the tab "General" before the common ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Number of fins:" (1 to 8; a spin box only);
/// - "Length:" (at least 0; slider 0, 2 cm, 10 cm);
/// - "Outer diameter:" (the outer radius times 2, at least 0; slider 0, 4 cm, 20 cm) and below it
///   "Automatic" (the radius at which the tubes touch, TubeFinSet::getOuterRadius());
/// - "Inner diameter:" (the inner radius times 2, at least 0; slider from 0 to the outer
///   diameter, which it follows);
/// - "Thickness:" (at least 0; slider 0 to 1 cm);
/// - to the right of them the placement (PlacementPanel) with the row "Fin rotation:" added
///   below its offset (the base rotation, unbounded, in the angle unit; slider -180 to 180
///   degrees; the label's tooltip "The angle of the first fin in the fin set."), and below it the
///   material and the finish (MaterialPanel, bulk materials).
/// "Automatic" is the outer diameter's own (Java: the check box of the DoubleModel's automatic
/// action): always enabled; a typed diameter switches it off (TubeFinSet::setOuterRadius()).
///
/// Kept from OpenRocket: the tab's title and tooltip are LaunchLugConfig's keys
/// (LaunchLugCfg.tab.General and LaunchLugCfg.tab.Generalprop; the texts are the same as a tube
/// fin set's would be).
///
/// Several tube fin sets (the user's decision U3, ComponentSelection): every field sets each
/// selected tube fin set with its own setter, as OpenRocket's setters of these properties forward
/// to the others before their early returns (TubeFinSet.setFinCount, setLength, setOuterRadius,
/// setOuterRadiusAutomatic, setInnerRadius, setThickness, setBaseRotation, setAngleOffset). The
/// inner diameter is the cascade ComponentSelection.h lists: TubeFinSet.setInnerRadius() gives
/// each tube fin set the wall thickness of its own outer radius here, of the main one's in
/// OpenRocket (its setThickness(main's outer radius - r) is forwarded too).
class TubeFinSetConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the tube fin sets of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         TubeFinSet
    TubeFinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                     std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// "Number of fins:".
    [[nodiscard]] IntSpinBox& finCountSpinBox() const noexcept { return *m_finCount; }
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
    [[nodiscard]] PlacementPanel& placementPanel() const noexcept { return *m_placement; }
    /// "Fin rotation:", in the placement panel.
    [[nodiscard]] const ConfigFields::NumberWidgets& baseRotationWidgets() const noexcept
    {
        return m_baseRotation;
    }
    [[nodiscard]] MaterialPanel& materialPanel() const noexcept { return *m_material; }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// The tube rows of the General tab, on the left.
    void addTubeRows(QWidget& page, QGridLayout& grid);
    /// The placement with the fin rotation and the material, on the right.
    [[nodiscard]] QWidget& makeRightColumn(QWidget& page);

    IntSpinBox*                 m_finCount{nullptr};
    ConfigFields::NumberWidgets m_length;
    ConfigFields::NumberWidgets m_outerDiameter;
    BoolCheckBox*               m_automatic{nullptr};
    ConfigFields::NumberWidgets m_innerDiameter;
    ConfigFields::NumberWidgets m_thickness;
    PlacementPanel*             m_placement{nullptr};
    ConfigFields::NumberWidgets m_baseRotation;
    MaterialPanel*              m_material{nullptr};
};

}  // namespace QtRocket::Gui
