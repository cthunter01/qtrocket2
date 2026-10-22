#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"

#include <memory>
#include <utility>

#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/BodyTubeConfig.h"
#include "QtRocket/gui/configdialog/InstancesPanel.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/NoseConeConfig.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/ShoulderPanel.h"
#include "QtRocket/gui/configdialog/TransitionConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The main component of @p selection, which must be valid.
[[nodiscard]] RocketComponent& validMain(const std::shared_ptr<ComponentSelection>& selection)
{
    if (selection == nullptr || !selection->isValid())
    {
        bug("A configuration panel needs a valid selection");
    }
    return selection->getMain();
}

/// The tab "General" of @p panel with the shared panels @p panels.
void addGeneralTab(RocketComponentConfig& panel, const ConfigPanelFactory::SharedPanels& panels)
{
    auto*         page        = makeChild<QWidget>(&panel);
    auto*         layout      = makeChild<QVBoxLayout>(page);
    BindingScope& scope       = panel.scope();
    const auto    addMaterial = [&] {
        if (panels.shockCordMaterial)
        {
            layout->addWidget(makeChild<MaterialPanel>(
                page, scope, panel.document(), panel.services().materials(), panels.materialType,
                //: ShockCordCfg.lbl.Shockcordmaterial
                RocketComponentConfig::tr("Shock cord material:"),
                //: MaterialPanel.lbl.ComponentFinish
                RocketComponentConfig::tr("Component finish:")));
        }
        else
        {
            layout->addWidget(makeChild<MaterialPanel>(
                page, scope, panel.document(), panel.services().materials(), panels.materialType));
        }
    };
    if (panels.instances)
    {
        layout->addWidget(makeChild<InstancesPanel>(page, scope));
    }
    if (panels.material && panels.materialFirst)
    {
        addMaterial();
    }
    if (panels.placement)
    {
        layout->addWidget(makeChild<PlacementPanel>(page, scope));
    }
    if (panels.material && !panels.materialFirst)
    {
        addMaterial();
    }
    layout->addStretch(1);
    if (panels.assemblyTab)
    {
        panel.insertTab(
            0, *page,
            //: RocketCompCfg.tab.Assembly
            RocketComponentConfig::tr("General"),
            //: RocketCompCfg.tab.AssemblyComment
            RocketComponentConfig::tr("Options for locating stages parallel to other stages"));
    }
    else
    {
        // Each kind's own key in Java (BodyTubecfg.tab.General, NoseConeCfg.tab.General, ...), all
        // with these texts.
        panel.insertTab(0, *page,
                        //: BodyTubecfg.tab.General
                        RocketComponentConfig::tr("General"),
                        //: BodyTubecfg.tab.Generalproperties
                        RocketComponentConfig::tr("General properties"));
    }
}

}  // namespace

ConfigPanelFactory::SharedPanels ConfigPanelFactory::sharedPanels(ComponentKind kind) noexcept
{
    switch (kind)
    {
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
            return {};
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
            return {.placement = true, .assemblyTab = true};
        case ComponentKind::BODY_TUBE:
            return {.material = true, .motor = true};
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
            return {.material = true, .shoulder = true};
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
        case ComponentKind::TUBE_FIN_SET:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
            return {.placement = true, .material = true};
        case ComponentKind::LAUNCH_LUG:
        case ComponentKind::RAIL_BUTTON:
            return {.instances = true, .placement = true, .material = true};
        case ComponentKind::INNER_TUBE:
            return {.placement = true, .material = true, .motor = true};
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::PARACHUTE:
        case ComponentKind::STREAMER:
            return {.placement = true};
        case ComponentKind::SHOCK_CORD:
            return {.placement         = true,
                    .material          = true,
                    .materialFirst     = true,
                    .materialType      = Material::Type::LINE,
                    .shockCordMaterial = true};
    }
    return {};
}

RocketComponentConfig* ConfigPanelFactory::createWithSharedPanels(
    OpenRocketDocument& document, const GuiServices& services,
    std::shared_ptr<ComponentSelection> selection, QWidget* parent)
{
    const ComponentKind kind   = validMain(selection).kind();
    const SharedPanels  panels = sharedPanels(kind);
    auto*               panel =
        makeChild<RocketComponentConfig>(parent, document, services, std::move(selection));
    if (panels.instances || panels.placement || panels.material)
    {
        addGeneralTab(*panel, panels);
    }
    if (panels.motor)
    {
        panel->insertTab(1, *makeChild<MotorConfig>(panel, panel->scope()),
                         //: BodyTubecfg.tab.Motor
                         RocketComponentConfig::tr("Motor"),
                         //: BodyTubecfg.tab.Motormountconf
                         RocketComponentConfig::tr("Motor mount configuration"));
    }
    if (panels.shoulder)
    {
        panel->insertTab(1, *makeChild<ShoulderPanel>(panel, panel->scope()),
                         //: NoseConeCfg.tab.Shoulder
                         RocketComponentConfig::tr("Shoulder"),
                         //: NoseConeCfg.tab.ttip.Shoulder
                         RocketComponentConfig::tr("Shoulder properties"));
    }
    panel->setSelectedTabIndex(0);
    return panel;
}

RocketComponentConfig* ConfigPanelFactory::create(OpenRocketDocument&                 document,
                                                  const GuiServices&                  services,
                                                  std::shared_ptr<ComponentSelection> selection,
                                                  QWidget*                            parent)
{
    const ComponentKind kind = validMain(selection).kind();
    // Several classes: the plain configuration (Java: getDialogContents()).
    if (!selection->isAllOfOneKind())
    {
        return makeChild<RocketComponentConfig>(parent, document, services, std::move(selection));
    }
    // One case per kind, each with its own line: run 3 replaces the line of a kind with its panel
    // (see the class comment).
    // NOLINTBEGIN(bugprone-branch-clone): a case per kind until run 3 gives each its own panel
    switch (kind)
    {
        case ComponentKind::ROCKET:
            // RocketConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::AXIAL_STAGE:
            // AxialStageConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::PARALLEL_STAGE:
            // AxialStageConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::POD_SET:
            // ComponentAssemblyConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::BODY_TUBE:
            // BodyTubeConfig
            return makeChild<BodyTubeConfig>(parent, document, services, std::move(selection));

        case ComponentKind::TRANSITION:
            // TransitionConfig
            return makeChild<TransitionConfig>(parent, document, services, std::move(selection));

        case ComponentKind::NOSE_CONE:
            // NoseConeConfig
            return makeChild<NoseConeConfig>(parent, document, services, std::move(selection));

        case ComponentKind::TRAPEZOID_FIN_SET:
            // TrapezoidFinSetConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::ELLIPTICAL_FIN_SET:
            // EllipticalFinSetConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::FREEFORM_FIN_SET:
            // FreeformFinSetConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::TUBE_FIN_SET:
            // TubeFinSetConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::LAUNCH_LUG:
            // LaunchLugConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::RAIL_BUTTON:
            // RailButtonConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::INNER_TUBE:
            // InnerTubeConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::TUBE_COUPLER:
            // ThicknessRingComponentConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::ENGINE_BLOCK:
            // ThicknessRingComponentConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::CENTERING_RING:
            // CenteringRingConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::BULKHEAD:
            // BulkheadConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::MASS_COMPONENT:
            // MassComponentConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::SHOCK_CORD:
            // ShockCordConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::PARACHUTE:
            // ParachuteConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);

        case ComponentKind::STREAMER:
            // StreamerConfig
            return createWithSharedPanels(document, services, std::move(selection), parent);
    }
    // NOLINTEND(bugprone-branch-clone)
    QTROCKET_UNREACHABLE();
}

}  // namespace QtRocket::Gui
