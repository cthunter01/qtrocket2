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

class ComponentSelection;
class GuiServices;
class InstancesPanel;
class MaterialPanel;
class PlacementPanel;

/// The configuration of a rail button (OpenRocket's RailButtonConfig, swing
/// gui/configdialog/RailButtonConfig.java): the tab "General" before the common ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider), each value
/// at least 0:
/// - "Outer Diameter:" and "Inner Diameter:" (sliders 0 to 2 cm);
/// - "Base Height:" (slider from 0 to the total height less the flange height) and "Flange
///   Height:" (slider from 0 to the total height less the base height), each slider following
///   its bound (RailButton::getMaxBaseHeight(), getMaxFlangeHeight());
/// - "Total Height (excl. screw):" (slider from the base plus the flange height, which it follows
///   (getMinTotalHeight()), to 2 cm);
/// - "Screw Height:" (slider 0 to 2 cm);
/// - the instances below them (InstancesPanel: the count and the separation);
/// - to the right of them the placement (PlacementPanel) with the row "Rotation:" added below its
///   offset (the angle around the parent's axis; slider -180 to 180 degrees), and below it the
///   material and the finish (MaterialPanel, bulk materials).
/// The setters keep the heights consistent (RailButton): a value beyond its bound is clamped
/// there, and the inner diameter is at most the outer one.
///
/// Kept from OpenRocket (a slip of RailButtonConfig.java): the rotation's bounds are -180 and 180
/// in radians, the SI unit of the angle (Java: DoubleModel(component, "AngleOffset", UNITS_ANGLE,
/// -180, +180)), not -pi and pi. They limit nothing that matters: the setter clamps the angle to
/// -pi ... pi (RailButton::setAngleOffset()), so that a typed 200 degrees is 180 degrees and
/// -20000 degrees is -180 degrees, as in OpenRocket.
///
/// Several rail buttons (the user's decision U3, ComponentSelection): every field sets each
/// selected rail button with its own setter, as OpenRocket's setters of these properties forward
/// to the others first (RailButton.setOuterDiameter, setInnerDiameter, setBaseHeight,
/// setFlangeHeight, setTotalHeight, setScrewHeight, setAngleOffset, setInstanceCount,
/// setInstanceSeparation). The outer diameter is the cascade ComponentSelection.h lists:
/// RailButton.setOuterDiameter() keeps each rail button's own inner diameter here (at most the
/// new outer one), where OpenRocket gives the others the main one's inner diameter (its
/// setInnerDiameter(main's inner diameter) is forwarded too).
class RailButtonConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the rail buttons of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         RailButton
    RailButtonConfig(OpenRocketDocument& document, const GuiServices& services,
                     std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& outerDiameterWidgets() const noexcept
    {
        return m_outerDiameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& innerDiameterWidgets() const noexcept
    {
        return m_innerDiameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& baseHeightWidgets() const noexcept
    {
        return m_baseHeight;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& flangeHeightWidgets() const noexcept
    {
        return m_flangeHeight;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& totalHeightWidgets() const noexcept
    {
        return m_totalHeight;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& screwHeightWidgets() const noexcept
    {
        return m_screwHeight;
    }
    [[nodiscard]] InstancesPanel& instancesPanel() const noexcept { return *m_instances; }
    [[nodiscard]] PlacementPanel& placementPanel() const noexcept { return *m_placement; }
    /// "Rotation:", in the placement panel.
    [[nodiscard]] const ConfigFields::NumberWidgets& angleWidgets() const noexcept
    {
        return m_angle;
    }
    [[nodiscard]] MaterialPanel& materialPanel() const noexcept { return *m_material; }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// The diameter and height rows of the General tab, on the left.
    void addDimensionRows(QWidget& page, QGridLayout& grid);
    /// The placement with the rotation and the material, on the right.
    [[nodiscard]] QWidget& makeRightColumn(QWidget& page);

    ConfigFields::NumberWidgets m_outerDiameter;
    ConfigFields::NumberWidgets m_innerDiameter;
    ConfigFields::NumberWidgets m_baseHeight;
    ConfigFields::NumberWidgets m_flangeHeight;
    ConfigFields::NumberWidgets m_totalHeight;
    ConfigFields::NumberWidgets m_screwHeight;
    InstancesPanel*             m_instances{nullptr};
    PlacementPanel*             m_placement{nullptr};
    ConfigFields::NumberWidgets m_angle;
    MaterialPanel*              m_material{nullptr};
};

}  // namespace QtRocket::Gui
