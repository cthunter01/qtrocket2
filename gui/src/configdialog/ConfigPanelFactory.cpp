#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"

#include <memory>
#include <utility>

#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/AxialStageConfig.h"
#include "QtRocket/gui/configdialog/BodyTubeConfig.h"
#include "QtRocket/gui/configdialog/BulkheadConfig.h"
#include "QtRocket/gui/configdialog/CenteringRingConfig.h"
#include "QtRocket/gui/configdialog/ComponentAssemblyConfig.h"
#include "QtRocket/gui/configdialog/EllipticalFinSetConfig.h"
#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"
#include "QtRocket/gui/configdialog/InnerTubeConfig.h"
#include "QtRocket/gui/configdialog/LaunchLugConfig.h"
#include "QtRocket/gui/configdialog/MassComponentConfig.h"
#include "QtRocket/gui/configdialog/NoseConeConfig.h"
#include "QtRocket/gui/configdialog/ParachuteConfig.h"
#include "QtRocket/gui/configdialog/RailButtonConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/RocketConfig.h"
#include "QtRocket/gui/configdialog/ShockCordConfig.h"
#include "QtRocket/gui/configdialog/StreamerConfig.h"
#include "QtRocket/gui/configdialog/ThicknessRingComponentConfig.h"
#include "QtRocket/gui/configdialog/TransitionConfig.h"
#include "QtRocket/gui/configdialog/TrapezoidFinSetConfig.h"
#include "QtRocket/gui/configdialog/TubeFinSetConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
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

}  // namespace

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
    // Java's "<Class>Config" of the class or its nearest superclass that has one.
    switch (kind)
    {
        case ComponentKind::ROCKET:
            return makeChild<RocketConfig>(parent, document, services, std::move(selection));

        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
            return makeChild<AxialStageConfig>(parent, document, services, std::move(selection));

        case ComponentKind::POD_SET:
            return makeChild<ComponentAssemblyConfig>(parent, document, services,
                                                      std::move(selection));

        case ComponentKind::BODY_TUBE:
            return makeChild<BodyTubeConfig>(parent, document, services, std::move(selection));

        case ComponentKind::TRANSITION:
            return makeChild<TransitionConfig>(parent, document, services, std::move(selection));

        case ComponentKind::NOSE_CONE:
            return makeChild<NoseConeConfig>(parent, document, services, std::move(selection));

        case ComponentKind::TRAPEZOID_FIN_SET:
            return makeChild<TrapezoidFinSetConfig>(parent, document, services,
                                                    std::move(selection));

        case ComponentKind::ELLIPTICAL_FIN_SET:
            return makeChild<EllipticalFinSetConfig>(parent, document, services,
                                                     std::move(selection));

        case ComponentKind::FREEFORM_FIN_SET:
            return makeChild<FreeformFinSetConfig>(parent, document, services,
                                                   std::move(selection));

        case ComponentKind::TUBE_FIN_SET:
            return makeChild<TubeFinSetConfig>(parent, document, services, std::move(selection));

        case ComponentKind::LAUNCH_LUG:
            return makeChild<LaunchLugConfig>(parent, document, services, std::move(selection));

        case ComponentKind::RAIL_BUTTON:
            return makeChild<RailButtonConfig>(parent, document, services, std::move(selection));

        case ComponentKind::INNER_TUBE:
            return makeChild<InnerTubeConfig>(parent, document, services, std::move(selection));

        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
            return makeChild<ThicknessRingComponentConfig>(parent, document, services,
                                                           std::move(selection));

        case ComponentKind::CENTERING_RING:
            return makeChild<CenteringRingConfig>(parent, document, services, std::move(selection));

        case ComponentKind::BULKHEAD:
            return makeChild<BulkheadConfig>(parent, document, services, std::move(selection));

        case ComponentKind::MASS_COMPONENT:
            return makeChild<MassComponentConfig>(parent, document, services, std::move(selection));

        case ComponentKind::SHOCK_CORD:
            return makeChild<ShockCordConfig>(parent, document, services, std::move(selection));

        case ComponentKind::PARACHUTE:
            return makeChild<ParachuteConfig>(parent, document, services, std::move(selection));

        case ComponentKind::STREAMER:
            return makeChild<StreamerConfig>(parent, document, services, std::move(selection));
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace QtRocket::Gui
