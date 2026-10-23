#pragma once

#include <memory>

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
/// Comment). Each panel is a subclass of RocketComponentConfig with the constructor
/// (OpenRocketDocument&, const GuiServices&, std::shared_ptr<ComponentSelection>, QWidget*).
class ConfigPanelFactory final
{
public:
    ConfigPanelFactory() = delete;

    /// The panel of the components of @p selection, a selection of @p document's rocket, as a child
    /// of @p parent (see the class comment).
    /// @throws BugError when @p selection is null or invalid
    [[nodiscard]] static RocketComponentConfig* create(
        OpenRocketDocument& document, const GuiServices& services,
        std::shared_ptr<ComponentSelection> selection, QWidget* parent);
};

}  // namespace QtRocket::Gui
