#pragma once

#include <memory>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class GuiServices;
class RocketComponentConfig;

/// Makes the contents of the component configuration dialog for a selection (OpenRocket:
/// ComponentConfigDialog.getDialogContents(), which finds "<Class>Config" by reflection, walking
/// up the component's superclasses). Here a switch on the main component's ComponentKind, in
/// create(), is the one place where a kind gets its panel:
///     Rocket -> RocketConfig; AxialStage, ParallelStage -> AxialStageConfig;
///     PodSet -> ComponentAssemblyConfig; TubeCoupler, EngineBlock -> ThicknessRingComponentConfig;
///     every other kind its own "<Class>Config";
/// and several components of different classes the plain RocketComponentConfig (name, Override,
/// Comment).
///
/// Milestone 2, run 2 has none of the panels of the kinds yet: each kind gets
/// createWithSharedPanels(), the plain configuration with the shared panels its Java panel shows
/// (the placement, the instances, the material, the motor mount and the shoulder tab). Run 3 adds
/// a kind's panel, a subclass of RocketComponentConfig with the constructor
/// (OpenRocketDocument&, const GuiServices&, std::shared_ptr<ComponentSelection>, QWidget*), by
/// replacing the line of its case in create() with
///     return makeChild<BodyTubeConfig>(parent, document, services, std::move(selection));
/// (and its include); the cases are kept apart by a line each so that the parts of run 3 change
/// lines that do not touch.
class ConfigPanelFactory final
{
public:
    ConfigPanelFactory() = delete;

    /// What createWithSharedPanels() shows for a kind.
    struct SharedPanels
    {
        /// The tab "General" with these panels, in this order: InstancesPanel (before the
        /// placement for a launch lug or a rail button), PlacementPanel, MaterialPanel (after the
        /// placement, or before it for a shock cord).
        bool instances = false;
        bool placement = false;
        bool material  = false;
        /// Whether the material comes before the placement (ShockCordConfig's order).
        bool materialFirst = false;
        /// The material's type, and whether its label is the shock cord's ("Shock cord
        /// material:").
        Material::Type materialType      = Material::Type::BULK;
        bool           shockCordMaterial = false;
        /// Whether "General" is the assemblies' tab (RocketCompCfg.tab.Assembly, with its
        /// tooltip "Options for locating stages parallel to other stages").
        bool assemblyTab = false;
        /// The tab "Motor" (MotorConfig) after "General".
        bool motor = false;
        /// The tab "Shoulder" (ShoulderPanel) after "General".
        bool shoulder = false;
    };

    /// The panel of the components of @p selection, a selection of @p document's rocket, as a child
    /// of @p parent (see the class comment).
    /// @throws BugError when @p selection is null or invalid
    [[nodiscard]] static RocketComponentConfig* create(
        OpenRocketDocument& document, const GuiServices& services,
        std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// The plain RocketComponentConfig with the shared panels Java's panel of the main
    /// component's kind shows (sharedPanels()), in front of Override and Comment.
    /// @throws BugError when @p selection is null or invalid
    [[nodiscard]] static RocketComponentConfig* createWithSharedPanels(
        OpenRocketDocument& document, const GuiServices& services,
        std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// The shared panels of @p kind (see SharedPanels).
    [[nodiscard]] static SharedPanels sharedPanels(ComponentKind kind) noexcept;
};

}  // namespace QtRocket::Gui
