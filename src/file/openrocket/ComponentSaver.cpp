#include "QtRocket/file/openrocket/ComponentSaver.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "QtRocket/file/openrocket/AssemblySaver.h"
#include "QtRocket/file/openrocket/ExternalComponentSaver.h"
#include "QtRocket/file/openrocket/InternalComponentSaver.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

namespace
{

using Saver = ComponentSaver::Saver;

/// The saver of each kind, in the order of kAllComponentKinds: what Java's reflection finds for
/// the class (the saver of the class's own name, else of the nearest superclass with one).
constexpr std::array<Saver, 22> kSavers{{
    {.kind = ComponentKind::ROCKET, .getElements = &AssemblySaver::rocketElements},
    {.kind = ComponentKind::AXIAL_STAGE, .getElements = &AssemblySaver::axialStageElements},
    // No ParallelStageSaver: AxialStageSaver.
    {.kind = ComponentKind::PARALLEL_STAGE, .getElements = &AssemblySaver::axialStageElements},
    // No PodSetSaver: ComponentAssemblySaver.
    {.kind = ComponentKind::POD_SET, .getElements = &AssemblySaver::componentAssemblyElements},
    {.kind = ComponentKind::BODY_TUBE, .getElements = &ExternalComponentSaver::bodyTubeElements},
    {.kind = ComponentKind::TRANSITION, .getElements = &ExternalComponentSaver::transitionElements},
    {.kind = ComponentKind::NOSE_CONE, .getElements = &ExternalComponentSaver::noseConeElements},
    {.kind        = ComponentKind::TRAPEZOID_FIN_SET,
     .getElements = &ExternalComponentSaver::trapezoidFinSetElements},
    {.kind        = ComponentKind::ELLIPTICAL_FIN_SET,
     .getElements = &ExternalComponentSaver::ellipticalFinSetElements},
    {.kind        = ComponentKind::FREEFORM_FIN_SET,
     .getElements = &ExternalComponentSaver::freeformFinSetElements},
    {.kind        = ComponentKind::TUBE_FIN_SET,
     .getElements = &ExternalComponentSaver::tubeFinSetElements},
    {.kind = ComponentKind::LAUNCH_LUG, .getElements = &ExternalComponentSaver::launchLugElements},
    {.kind        = ComponentKind::RAIL_BUTTON,
     .getElements = &ExternalComponentSaver::railButtonElements},
    {.kind = ComponentKind::INNER_TUBE, .getElements = &InternalComponentSaver::innerTubeElements},
    {.kind        = ComponentKind::TUBE_COUPLER,
     .getElements = &InternalComponentSaver::tubeCouplerElements},
    {.kind        = ComponentKind::ENGINE_BLOCK,
     .getElements = &InternalComponentSaver::engineBlockElements},
    {.kind        = ComponentKind::CENTERING_RING,
     .getElements = &InternalComponentSaver::centeringRingElements},
    {.kind = ComponentKind::BULKHEAD, .getElements = &InternalComponentSaver::bulkheadElements},
    {.kind        = ComponentKind::MASS_COMPONENT,
     .getElements = &InternalComponentSaver::massComponentElements},
    {.kind = ComponentKind::SHOCK_CORD, .getElements = &InternalComponentSaver::shockCordElements},
    {.kind = ComponentKind::PARACHUTE, .getElements = &InternalComponentSaver::parachuteElements},
    {.kind = ComponentKind::STREAMER, .getElements = &InternalComponentSaver::streamerElements},
}};

/// Whether kSavers has one entry per kind, in the order of kAllComponentKinds, each with a
/// saver.
consteval bool coversEveryKindInOrder()
{
    if (kSavers.size() != kAllComponentKinds.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < kSavers.size(); ++i)
    {
        if (kSavers.at(i).kind != kAllComponentKinds.at(i) || kSavers.at(i).getElements == nullptr)
        {
            return false;
        }
    }
    return true;
}

static_assert(coversEveryKindInOrder(),
              "one saver per ComponentKind, in kAllComponentKinds' order");

}  // namespace

ComponentSaver::GetElements ComponentSaver::saverOf(ComponentKind kind)
{
    return kSavers.at(static_cast<std::size_t>(kind)).getElements;
}

std::vector<std::string> ComponentSaver::getElements(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences)
{
    return saverOf(component.kind())(component, writer, preferences);
}

}  // namespace QtRocket
