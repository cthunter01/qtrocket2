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

/// The configuration of a launch lug (OpenRocket's LaunchLugConfig, swing
/// gui/configdialog/LaunchLugConfig.java): the tab "General" before the common ones.
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Length:" (at least 0; slider 0, 2 cm, 10 cm);
/// - "Outer diameter:" (the outer radius times 2, at least 0; slider 0, 4 cm, 20 cm);
/// - "Inner diameter:" (the inner radius times 2, at least 0; slider from 0 to the outer
///   diameter, which it follows);
/// - "Thickness:" (at least 0; slider 0 to 1 cm);
/// - the instances below them (InstancesPanel: the count and the separation);
/// - to the right of them the placement (PlacementPanel) with the row "Rotation:" added below its
///   offset (the angle around the parent's axis, -180 to 180 degrees; slider over that range),
///   and below it the material and the finish (MaterialPanel, bulk materials).
///
/// Several launch lugs (the user's decision U3, ComponentSelection): every field sets each
/// selected launch lug with its own setter, as OpenRocket's setters of these properties forward
/// to the others before their early returns (LaunchLug.setLength, setOuterRadius,
/// setInnerRadius, setThickness, setAngleOffset, setInstanceCount, setInstanceSeparation). The
/// inner diameter is the cascade ComponentSelection.h lists: LaunchLug.setInnerRadius() keeps the
/// wall of each launch lug here and moves its outer radius, where OpenRocket gives the others the
/// main one's outer radius (its setOuterRadius(r + main's thickness) is forwarded too).
class LaunchLugConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the launch lugs of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         LaunchLug
    LaunchLugConfig(OpenRocketDocument& document, const GuiServices& services,
                    std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& lengthWidgets() const noexcept
    {
        return m_length;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& outerDiameterWidgets() const noexcept
    {
        return m_outerDiameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& innerDiameterWidgets() const noexcept
    {
        return m_innerDiameter;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& thicknessWidgets() const noexcept
    {
        return m_thickness;
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
    /// The tube rows and the instances of the General tab, on the left.
    void addTubeRows(QWidget& page, QGridLayout& grid);
    /// The placement with the rotation and the material, on the right.
    [[nodiscard]] QWidget& makeRightColumn(QWidget& page);

    ConfigFields::NumberWidgets m_length;
    ConfigFields::NumberWidgets m_outerDiameter;
    ConfigFields::NumberWidgets m_innerDiameter;
    ConfigFields::NumberWidgets m_thickness;
    InstancesPanel*             m_instances{nullptr};
    PlacementPanel*             m_placement{nullptr};
    ConfigFields::NumberWidgets m_angle;
    MaterialPanel*              m_material{nullptr};
};

}  // namespace QtRocket::Gui
