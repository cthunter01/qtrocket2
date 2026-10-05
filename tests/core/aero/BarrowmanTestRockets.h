#pragma once

#include <memory>
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
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "rocket/TestRockets.h"

/// The rockets of the Barrowman calculator tests that TestRockets does not make. Each is built
/// statement for statement as the Java probe that computed the pinned values builds it
/// (Rockets.java of the probes: steps(), tubeFins(), flushPod() and shortPod(); NestedProbe.java:
/// boostersOnBoosters()), from the real components, with the rocket's events enabled at the end.
/// The definitions are in BarrowmanTestRockets.cpp.
namespace QtRocket::Test
{

/// Diameter steps in both directions and two components of length 0: a conical nose (0.06 m,
/// radius 0.02 m), "Tube A" (0.1 m, radius 0.02 m) with two rail buttons 0.05 m apart, "Disk" (a
/// body tube of length 0, radius 0.03 m), "Tube B" (0.1 m, radius 0.01 m) with a launch lug,
/// "Flat Transition" (length 0, radii 0.01 and 0.025 m) and "Tube C" (0.05 m, radius 0.025 m)
/// with three fins.
struct TestStepsRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               tubeA{nullptr};
    BodyTube*               disk{nullptr};
    BodyTube*               tubeB{nullptr};
    Transition*             flat{nullptr};
    BodyTube*               tubeC{nullptr};
    RailButton*             buttons{nullptr};
    LaunchLug*              lug{nullptr};
    TrapezoidFinSet*        fins{nullptr};

    TestStepsRocket();
};

/// The Estes Alpha III with its fin set replaced by a set of six tube fins, 0.04 m long, at the
/// BOTTOM of the body tube. The base's `fins` is null.
struct TestTubeFinsRocket : TestEstesAlphaIII
{
    TubeFinSet* tubes{nullptr};

    TestTubeFinsRocket();
};

/// A nose (0.05 m), "Tube A" and "Tube B" (0.1 m each), all of radius 0.012 m; Tube B holds a
/// coaxial pod set of one at its TOP with "Pod Tube" (0.03 m): the pod starts flush with the end
/// of Tube A.
struct TestFlushPodRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               tubeA{nullptr};
    BodyTube*               tubeB{nullptr};
    PodSet*                 pod{nullptr};
    BodyTube*               podTube{nullptr};

    TestFlushPodRocket();
};

/// A nose (0.05 m) and "Front Tube" (0.1 m), radius 0.012 m; the front tube holds a coaxial pod
/// set of one at its TOP, offset by @p podOffset, with "Short Tube" (0.02 m). With @p withTail a
/// "Tail Tube" (0.05 m) follows the front tube.
struct TestShortPodRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               front{nullptr};
    PodSet*                 pod{nullptr};
    BodyTube*               shortTube{nullptr};
    BodyTube*               tail{nullptr};

    TestShortPodRocket(double podOffset, bool withTail);
};

/// The Falcon 9 Heavy with boosters on its boosters: "Inner Boosters", a booster set of two on
/// the booster body, each with an ogive nose (0.05 m, radius 0.015 m) and a body tube (0.2 m)
/// holding three fins. The inner set is stage 3, below the booster set (2) and the core (1): the
/// one design of the tests with an active stage two inactive stages deep, once
/// FlightConfiguration::setOnlyStage(kInnerStageNumber) is called.
struct TestBoostersOnBoostersRocket : TestFalcon9Heavy
{
    static constexpr int kInnerStageNumber = 3;

    ParallelStage*   innerStage{nullptr};
    NoseCone*        innerNose{nullptr};
    BodyTube*        innerBody{nullptr};
    TrapezoidFinSet* innerFins{nullptr};

    TestBoostersOnBoostersRocket();
};

/// The rocket and every component below it, in tree order (Java: `for (RocketComponent c :
/// rocket)`). The pinned tables name a component by its index in this list.
[[nodiscard]] std::vector<RocketComponent*> allComponents(Rocket& rocket);

}  // namespace QtRocket::Test
