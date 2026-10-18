#include "QtRocket/rocket/ComponentFactory.h"

#include <memory>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

/// The materials Java's constructors of a recovery device take from the preferences: the
/// canopy's, asked for RecoveryDevice.class whatever the device is (so a default stored for
/// Parachute or Streamer is not seen: the class chain without its first class), which is also
/// the default a preset without a usable material leaves; and for a parachute the lines',
/// asked for Parachute.class.
void applyRecoveryDeviceMaterials(RecoveryDevice& device, const Preferences& preferences,
                                  const MaterialStorage& materials)
{
    const ComponentClassChain chain = componentClassChain(device.kind());
    const Material canopy = getDefaultComponentMaterial(preferences, chain.subspan(1),
                                                        Material::Type::SURFACE, materials);
    device.setDefaultMaterial(canopy);
    device.setMaterial(canopy);
    if (auto* const parachute = dynamic_cast<Parachute*>(&device))
    {
        const Material line =
            getDefaultComponentMaterial(preferences, chain, Material::Type::LINE, materials);
        parachute->setDefaultLineMaterial(line);
        parachute->setLineMaterial(line);
    }
}

}  // namespace

std::unique_ptr<RocketComponent> ComponentFactory::createWithBuiltinMaterials(ComponentKind kind)
{
    switch (kind)
    {
        case ComponentKind::AXIAL_STAGE:
            return std::make_unique<AxialStage>();
        case ComponentKind::PARALLEL_STAGE:
            return std::make_unique<ParallelStage>();
        case ComponentKind::POD_SET:
            return std::make_unique<PodSet>();
        case ComponentKind::BODY_TUBE:
            return std::make_unique<BodyTube>();
        case ComponentKind::TRANSITION:
            return std::make_unique<Transition>();
        case ComponentKind::NOSE_CONE:
            return std::make_unique<NoseCone>();
        case ComponentKind::TRAPEZOID_FIN_SET:
            return std::make_unique<TrapezoidFinSet>();
        case ComponentKind::ELLIPTICAL_FIN_SET:
            return std::make_unique<EllipticalFinSet>();
        case ComponentKind::FREEFORM_FIN_SET:
            return std::make_unique<FreeformFinSet>();
        case ComponentKind::TUBE_FIN_SET:
            return std::make_unique<TubeFinSet>();
        case ComponentKind::LAUNCH_LUG:
            return std::make_unique<LaunchLug>();
        case ComponentKind::RAIL_BUTTON:
            return std::make_unique<RailButton>();
        case ComponentKind::INNER_TUBE:
            return std::make_unique<InnerTube>();
        case ComponentKind::TUBE_COUPLER:
            return std::make_unique<TubeCoupler>();
        case ComponentKind::ENGINE_BLOCK:
            return std::make_unique<EngineBlock>();
        case ComponentKind::CENTERING_RING:
            return std::make_unique<CenteringRing>();
        case ComponentKind::BULKHEAD:
            return std::make_unique<Bulkhead>();
        case ComponentKind::MASS_COMPONENT:
            return std::make_unique<MassComponent>();
        case ComponentKind::SHOCK_CORD:
            return std::make_unique<ShockCord>();
        case ComponentKind::PARACHUTE:
            return std::make_unique<Parachute>();
        case ComponentKind::STREAMER:
            return std::make_unique<Streamer>();
        case ComponentKind::ROCKET:
            bug("ComponentFactory makes no rocket: a rocket is made with its document");
    }
    QTROCKET_UNREACHABLE();
}

std::unique_ptr<RocketComponent> ComponentFactory::create(ComponentKind          kind,
                                                          const Preferences&     preferences,
                                                          const MaterialStorage& materials)
{
    std::unique_ptr<RocketComponent> component = createWithBuiltinMaterials(kind);
    applyDefaultMaterials(*component, preferences, materials);
    return component;
}

void ComponentFactory::applyDefaultMaterials(RocketComponent&       component,
                                             const Preferences&     preferences,
                                             const MaterialStorage& materials)
{
    // Java's constructor completes an object that no tree holds yet; a setter below would fire
    // into the rocket of a component that is in one.
    QTROCKET_ASSERT(component.getParent() == nullptr);
    const ComponentClassChain chain = componentClassChain(component.kind());
    if (auto* const external = dynamic_cast<ExternalComponent*>(&component))
    {
        // The fillets of a fin set too; a rail button stays Delrin.
        external->applyDefaultMaterial(preferences, materials);
    }
    else if (auto* const structural = dynamic_cast<StructuralComponent*>(&component))
    {
        structural->setMaterial(
            getDefaultComponentMaterial(preferences, chain, Material::Type::BULK, materials));
    }
    else if (auto* const cord = dynamic_cast<ShockCord*>(&component))
    {
        cord->setMaterial(
            getDefaultComponentMaterial(preferences, chain, Material::Type::LINE, materials));
    }
    else if (auto* const device = dynamic_cast<RecoveryDevice*>(&component))
    {
        applyRecoveryDeviceMaterials(*device, preferences, materials);
    }
}

}  // namespace QtRocket
