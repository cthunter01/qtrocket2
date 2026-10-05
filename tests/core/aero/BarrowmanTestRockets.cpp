#include "aero/BarrowmanTestRockets.h"

#include <memory>
#include <utility>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"

namespace QtRocket::Test
{

namespace
{

/// A body tube named @p name (Java: new BodyTube(length, radius, thickness), then setName()).
[[nodiscard]] std::unique_ptr<BodyTube> namedTube(const char* name, double length, double radius,
                                                  double thickness)
{
    auto tube = std::make_unique<BodyTube>(length, radius, thickness);
    tube->setName(name);
    return tube;
}

}  // namespace

TestStepsRocket::TestStepsRocket()
{
    rocket->setName("Steps");
    stage = &rocket->addChild(std::make_unique<AxialStage>());

    nose = &stage->addChild(std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.06, 0.02));

    tubeA = &stage->addChild(namedTube("Tube A", 0.1, 0.02, 0.001));

    disk = &stage->addChild(namedTube("Disk", 0.0, 0.03, 0.001));

    tubeB = &stage->addChild(namedTube("Tube B", 0.1, 0.01, 0.001));

    auto flatTransition = std::make_unique<Transition>();
    flatTransition->setName("Flat Transition");
    flatTransition->setLength(0);
    flatTransition->setForeRadius(0.01);
    flatTransition->setAftRadius(0.025);
    flat = &stage->addChild(std::move(flatTransition));

    tubeC = &stage->addChild(namedTube("Tube C", 0.05, 0.025, 0.001));

    auto railButtons = std::make_unique<RailButton>();
    railButtons->setInstanceCount(2);
    railButtons->setInstanceSeparation(0.05);
    buttons = &tubeA->addChild(std::move(railButtons));

    lug = &tubeB->addChild(std::make_unique<LaunchLug>());

    fins = &tubeC->addChild(std::make_unique<TrapezoidFinSet>(3, 0.04, 0.02, 0.01, 0.03));

    rocket->enableEvents();
}

TestTubeFinsRocket::TestTubeFinsRocket()
{
    // Java: body.removeChild(body.getChild(0)), the fin set
    static_cast<void>(body->removeChild(0));
    fins = nullptr;

    auto tubeFins = std::make_unique<TubeFinSet>();
    tubeFins->setFinCount(6);
    tubeFins->setLength(0.04);
    tubeFins->setAxialMethod(AxialMethod::BOTTOM);
    tubes = &body->addChild(std::move(tubeFins), 0);
}

TestFlushPodRocket::TestFlushPodRocket()
{
    rocket->setName("Flush Pod");
    stage = &rocket->addChild(std::make_unique<AxialStage>());

    nose = &stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.05, 0.012));

    tubeA = &stage->addChild(namedTube("Tube A", 0.1, 0.012, 0.001));

    tubeB = &stage->addChild(namedTube("Tube B", 0.1, 0.012, 0.001));

    auto podSet = std::make_unique<PodSet>();
    podSet->setInstanceCount(1);
    podSet->setRadiusMethod(RadiusMethod::COAXIAL);
    pod = &tubeB->addChild(std::move(podSet));
    pod->setAxialMethod(AxialMethod::TOP);
    pod->setAxialOffset(0);

    podTube = &pod->addChild(namedTube("Pod Tube", 0.03, 0.012, 0.001));

    rocket->enableEvents();
}

TestShortPodRocket::TestShortPodRocket(double podOffset, bool withTail)
{
    rocket->setName("Short Pod");
    stage = &rocket->addChild(std::make_unique<AxialStage>());

    nose = &stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.05, 0.012));

    front = &stage->addChild(namedTube("Front Tube", 0.1, 0.012, 0.001));

    auto podSet = std::make_unique<PodSet>();
    podSet->setInstanceCount(1);
    podSet->setRadiusMethod(RadiusMethod::COAXIAL);
    pod = &front->addChild(std::move(podSet));
    pod->setAxialMethod(AxialMethod::TOP);
    pod->setAxialOffset(podOffset);

    shortTube = &pod->addChild(namedTube("Short Tube", 0.02, 0.012, 0.001));

    if (withTail)
    {
        tail = &stage->addChild(namedTube("Tail Tube", 0.05, 0.012, 0.001));
    }

    rocket->enableEvents();
}

TestBoostersOnBoostersRocket::TestBoostersOnBoostersRocket()
{
    auto inner = std::make_unique<ParallelStage>();
    inner->setName("Inner Boosters");
    inner->setInstanceCount(2);
    innerStage = &boosterBody->addChild(std::move(inner));

    innerNose =
        &innerStage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.05, 0.015));

    innerBody = &innerStage->addChild(std::make_unique<BodyTube>(0.2, 0.015, 0.001));

    innerFins = &innerBody->addChild(std::make_unique<TrapezoidFinSet>(3, 0.04, 0.02, 0.01, 0.03));
}

std::vector<RocketComponent*> allComponents(Rocket& rocket)
{
    std::vector<RocketComponent*> components = rocket.getAllChildren();
    components.insert(components.begin(), &rocket);
    return components;
}

}  // namespace QtRocket::Test
