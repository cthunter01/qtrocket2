// Tests of goldens/AutomaticDimensions.h, the port of the golden harness's
// AutomaticDimensions.java (tools/openrocket-goldens), on rockets built here whose automatic
// radii depend on their neighbours.
//
// The harness class has no JUnit test. The expectations are OpenRocket's: the probe
// tier9a-g2-rules/java/src/info/qtrocket/goldens/SettleProbe.java builds the same rockets,
// statement for statement, with OpenRocket's compiled core (commit 5f164fd0e), runs the harness's
// AutomaticDimensions.settle() on them and prints what the tests pin: the number of passes it
// reports and the values of the passes, as Double.toString() writes them. Every value is the
// result of additions, subtractions, multiplications, divisions, minima and maxima (a radius less
// a wall thickness, a volume over the square of a radius), so the pins are exact on every platform.

#include "goldens/AutomaticDimensions.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenGeometry.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ErrorCode;
using QtRocket::InnerTube;
using QtRocket::MassComponent;
using QtRocket::MassObject;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::ShockCord;
using QtRocket::Streamer;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TubeCoupler;
using QtRocket::Test::automaticDimensions;
using QtRocket::Test::componentAtGoldenPath;
using QtRocket::Test::kMaxSettlingPasses;
using QtRocket::Test::samePass;
using QtRocket::Test::settleAutomaticDimensions;
using QtRocket::Test::settleBy;

// ================================================================================= the rockets

/// A rocket named @p name with one stage, "Sustainer", which is returned through @p stage.
[[nodiscard]] std::unique_ptr<Rocket> makeRocket(std::string_view name, AxialStage*& stage)
{
    auto rocket = std::make_unique<Rocket>();
    rocket->enableEvents();
    rocket->setName(name);
    auto sustainer = std::make_unique<AxialStage>();
    sustainer->setName("Sustainer");
    stage = &rocket->addChild(std::move(sustainer));
    return rocket;
}

/// A body tube named @p name with an automatic outer radius.
[[nodiscard]] std::unique_ptr<BodyTube> automaticTube(std::string_view name, double length,
                                                      double thickness)
{
    auto tube = std::make_unique<BodyTube>(length, 0.02, thickness);
    tube->setName(name);
    tube->setOuterRadiusAutomatic(true);
    return tube;
}

/// @p object named @p name with the packed radius @p radius and length @p length, and then an
/// automatic radius. It is set up before it is added to its parent, so that no setter asks the
/// parent for its radius.
template <class Object>
[[nodiscard]] std::unique_ptr<Object> automaticObject(std::unique_ptr<Object> object,
                                                      std::string_view name, double radius,
                                                      double length)
{
    object->setName(name);
    object->setRadius(radius);
    object->setLength(length);
    object->setRadiusAutomatic(true);
    return object;
}

/// @p component, named @p name.
template <class Component>
[[nodiscard]] std::unique_ptr<Component> named(std::unique_ptr<Component> component,
                                               std::string_view           name)
{
    component->setName(name);
    return component;
}

/// The internal components of the rocket of makeTubes(): a mass component in the nose cone, a
/// parachute in the first tube, a motor mount, a centering ring and a shock cord in the second,
/// a tube coupler and a mass component in the third, a bulkhead and a streamer in the fourth.
/// The radii of the mass objects, the outer radii of the ring, the coupler and the bulkhead and
/// the inner radius of the ring are automatic.
void addInternalComponents(NoseCone& nose, BodyTube& a, BodyTube& b, BodyTube& c, BodyTube& n)
{
    auto ballast = std::make_unique<MassComponent>(0.02, 0.005, 0.01);
    ballast->setName("Ballast");
    ballast->setRadiusAutomatic(true);
    nose.addChild(std::move(ballast));

    a.addChild(automaticObject(std::make_unique<Parachute>(), "Chute", 0.012, 0.05));

    b.addChild(named(std::make_unique<InnerTube>(), "Mount"));
    b.addChild(named(std::make_unique<CenteringRing>(), "Ring"));
    b.addChild(automaticObject(std::make_unique<ShockCord>(), "Cord", 0.008, 0.03));

    c.addChild(named(std::make_unique<TubeCoupler>(), "Coupler"));
    auto payload = std::make_unique<MassComponent>(0.04, 0.01, 0.05);
    payload->setName("Payload");
    payload->setRadiusAutomatic(true);
    c.addChild(std::move(payload));

    n.addChild(named(std::make_unique<Bulkhead>(), "Bulkhead"));
    n.addChild(automaticObject(std::make_unique<Streamer>(), "Streamer", 0.01, 0.06));
}

/// SettleProbe.tubes(): a sustainer of a nose cone, three body tubes with automatic radii (A, B
/// and C), a body tube of 31 mm radius (N) and a transition, with internal components whose
/// radii are automatic too (addInternalComponents()).
///
/// @p staged: the second and the third tube are asked for their radius as soon as they are
/// added, before the tube after them is there, so that each remembers the tube before it as the
/// one its radius came from (BodyTube's refComp), as a design remembers what its getters were
/// asked while it was put together. A tube does not take its radius from a neighbour that took
/// its own from the tube, so the radius of N then reaches B only when C has let go of B, and A
/// when B has let go of A: a pass later each.
[[nodiscard]] std::unique_ptr<Rocket> makeTubes(std::string_view name, bool staged)
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = makeRocket(name, stage);

    auto noseCone = std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.10, 0.02);
    noseCone->setName("Nose");
    noseCone->setAftRadiusAutomatic(true);
    NoseCone& nose = stage->addChild(std::move(noseCone));

    BodyTube& a = stage->addChild(automaticTube("A", 0.20, 0.002));
    BodyTube& b = stage->addChild(automaticTube("B", 0.15, 0.001));
    if (staged)
    {
        (void)b.getOuterRadius();
    }
    BodyTube& c = stage->addChild(automaticTube("C", 0.10, 0.0015));
    if (staged)
    {
        (void)c.getOuterRadius();
    }
    BodyTube& n = stage->addChild(named(std::make_unique<BodyTube>(0.30, 0.031, 0.002), "N"));

    auto tail = std::make_unique<Transition>();
    tail->setName("Tail");
    tail->setLength(0.05);
    tail->setAftRadiusAutomatic(false);
    tail->setAftRadius(0.02);
    tail->setForeRadiusAutomatic(true);
    stage->addChild(std::move(tail));

    addInternalComponents(nose, a, b, c, n);
    return rocket;
}

/// SettleProbe.fixed(): a sustainer without an automatic dimension: a nose cone, a body tube, a
/// parachute and a bulkhead.
[[nodiscard]] std::unique_ptr<Rocket> makeFixed()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = makeRocket("Fixed", stage);
    stage->addChild(
        named(std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.08, 0.015), "Nose"));
    BodyTube& tube = stage->addChild(named(std::make_unique<BodyTube>(0.25, 0.015, 0.001), "Tube"));
    auto      chute = std::make_unique<Parachute>();
    chute->setName("Chute");
    chute->setRadius(0.012);
    chute->setLength(0.05);
    tube.addChild(std::move(chute));
    auto bulkhead = std::make_unique<Bulkhead>();
    bulkhead->setName("Bulkhead");
    bulkhead->setOuterRadiusAutomatic(false);
    bulkhead->setOuterRadius(0.012);
    tube.addChild(std::move(bulkhead));
    return rocket;
}

/// SettleProbe.turned(): a parachute with an automatic radius in a body tube whose radius is
/// then changed: the stored radius and length of the parachute are stale until its getters are
/// asked.
[[nodiscard]] std::unique_ptr<Rocket> makeTurned()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = makeRocket("Turned", stage);
    BodyTube& tube = stage->addChild(named(std::make_unique<BodyTube>(0.25, 0.02, 0.001), "Tube"));
    auto      parachute = std::make_unique<Parachute>();
    parachute->setName("Chute");
    parachute->setRadius(0.012);
    parachute->setLength(0.05);
    Parachute& chute = tube.addChild(std::move(parachute));
    chute.setRadiusAutomatic(true);
    (void)chute.getRadius();
    tube.setOuterRadius(0.03);
    return rocket;
}

// ==================================================================== what SettleProbe prints

// The values of a pass over the rocket of makeTubes(), in the order of
// AutomaticDimensions.snapshot(); a boolean is 1.0 or 0.0.
// clang-format off

/// The settled state: what a pass returns once the dimensions have settled, for the staged
/// rocket and for the one built without asking. (The inner radius of the coupler is its outer
/// radius: TubeCoupler() sets its wall thickness while its automatic outer radius, without a
/// parent, is still 0, and ThicknessRingComponent.setThickness() clamps the thickness to that.)
constexpr std::array<double, 50> kSettledTubes{
    0.0, 0.031, 0.0, 1.0,                                   // Nose: fore, aft, uses previous, next
    0.031, 5.202913631633716E-4,                            // Ballast: radius, length
    0.031, 0.028999999999999998, 0.031, 0.031, 0.0, 1.0,    // A: outer, inner, fore, aft, uses ...
    0.028999999999999998, 0.00856123662306778,              // Chute: radius, length
    0.031, 0.03, 0.031, 0.031, 0.0, 1.0,                    // B
    0.0095, 0.009,                                          // Mount: outer, inner
    0.03, 0.0095,                                           // Ring: outer, inner
    0.03, 0.002133333333333333,                             // Cord
    0.031, 0.0295, 0.031, 0.031, 0.0, 1.0,                  // C
    0.0295, 0.0295,                                         // Coupler
    0.0295, 0.004596380350474003,                           // Payload
    0.031, 0.028999999999999998, 0.031, 0.031, 0.0, 0.0,    // N
    0.028999999999999998, 0.0,                              // Bulkhead
    0.028999999999999998, 0.0071343638525564815,            // Streamer
    0.031, 0.02, 1.0, 0.0};                                 // Tail

/// The first pass over the staged rocket: A and B still have the default radius of 25 mm, B
/// taken from A (it uses the previous component), and what they hold is as wide as they are.
constexpr std::array<double, 50> kFirstPassOfTheChain{
    0.0, 0.031, 0.0, 1.0,                                   // Nose
    0.031, 5.202913631633716E-4,                            // Ballast
    0.025, 0.023, 0.025, 0.025, 0.0, 0.0,                   // A
    0.023, 0.013610586011342157,                            // Chute
    0.025, 0.024, 0.025, 0.025, 1.0, 0.0,                   // B
    0.0095, 0.009,                                          // Mount
    0.024, 0.0095,                                          // Ring
    0.024, 0.003333333333333333,                            // Cord
    0.031, 0.0295, 0.031, 0.031, 0.0, 1.0,                  // C
    0.0295, 0.0295,                                         // Coupler
    0.0295, 0.004596380350474003,                           // Payload
    0.031, 0.028999999999999998, 0.031, 0.031, 0.0, 0.0,    // N
    0.028999999999999998, 0.0,                              // Bulkhead
    0.028999999999999998, 0.0071343638525564815,            // Streamer
    0.031, 0.02, 1.0, 0.0};                                 // Tail

/// The places of the values that the second pass over the staged rocket changes: those of B
/// (which now takes the radius of N through C), the outer radius of the ring and the shock cord.
constexpr std::array<std::size_t, 9> kChangedByTheSecondPass{14, 15, 16, 17, 18, 19, 22, 24, 25};

/// SettleProbe.fixed(): the nose cone, the body tube (as a body tube and as a symmetric
/// component), the parachute and the bulkhead.
constexpr std::array<double, 14> kFixed{
    0.0, 0.015, 0.0, 0.0,
    0.015, 0.013999999999999999, 0.015, 0.015, 0.0, 0.0,
    0.012, 0.05,
    0.012, 0.0};

/// SettleProbe.turned(): the body tube and the parachute.
constexpr std::array<double, 8> kTurned{
    0.03, 0.028999999999999998, 0.03, 0.03, 0.0, 0.0,
    0.028999999999999998, 0.00856123662306778};
// clang-format on

/// The values @p values as a pass.
[[nodiscard]] std::vector<double> passOf(std::span<const double> values)
{
    return {values.begin(), values.end()};
}

/// The pass @p base with the values at @p places taken from @p from.
[[nodiscard]] std::vector<double> withValuesOf(std::span<const double>      base,
                                               std::span<const double>      from,
                                               std::span<const std::size_t> places)
{
    std::vector<double> pass = passOf(base);
    for (const std::size_t place : places)
    {
        pass.at(place) = from[place];
    }
    return pass;
}

/// The number of passes settleAutomaticDimensions() reports for @p rocket; -1 (with a test
/// failure) when the dimensions do not settle.
[[nodiscard]] int settledAfter(const Rocket& rocket)
{
    const Result<int> passes = settleAutomaticDimensions(rocket);
    if (!passes)
    {
        ADD_FAILURE() << passes.error().message;
        return -1;
    }
    return *passes;
}

// ======================================================================================= tests

// A chain that needs more than one pass. Java: settle() = 2, the values of every pass.
TEST(GoldenAutomaticDimensions, AChainOfAutomaticTubesSettlesInThePassesOfTheHarness)
{
    const std::unique_ptr<Rocket> rocket = makeTubes("Chain", true);
    EXPECT_EQ(settledAfter(*rocket), 2);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kSettledTubes));
    // The settled state stays: no later pass changes it.
    EXPECT_EQ(settledAfter(*rocket), 0);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kSettledTubes));
}

// How the chain gets there, pass by pass (the passes the probe prints for a second instance of
// the design): the first pass leaves A and B at the default radius, the second gives B the
// radius of N, the third A; the fourth repeats the third.
TEST(GoldenAutomaticDimensions, ThePassesOverTheChainAreTheHarnesss)
{
    const std::unique_ptr<Rocket> rocket = makeTubes("Chain", true);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kFirstPassOfTheChain));
    EXPECT_EQ(automaticDimensions(*rocket),
              withValuesOf(kFirstPassOfTheChain, kSettledTubes, kChangedByTheSecondPass));
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kSettledTubes));
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kSettledTubes));
}

// A design that is already settled: the same rocket, built without asking a tube for its radius
// on the way. Java: settle() = 0, and the same settled values as the chain's.
TEST(GoldenAutomaticDimensions, ADesignBuiltWithoutAskingIsSettledByItsFirstPass)
{
    const std::unique_ptr<Rocket> rocket = makeTubes("Settled", false);
    EXPECT_EQ(settledAfter(*rocket), 0);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kSettledTubes));
}

// Java: settle() = 0 and the 14 values. A body tube gives six values: Java asks it as a
// BodyTube and as a SymmetricComponent; the stage and the rocket give none.
TEST(GoldenAutomaticDimensions, ADesignWithoutAutomaticDimensionsIsLeftAsItIs)
{
    const std::unique_ptr<Rocket> rocket = makeFixed();
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kFixed));
    EXPECT_EQ(settledAfter(*rocket), 0);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kFixed));
}

// What the settling is for. Java: before any pass the parachute's getLength() is
// 0.01994459833795014, the volume over the square of the radius it stored in the narrower tube;
// settle() = 0 (its first pass refreshes the radius, and no later one changes anything), and
// then the 8 values.
TEST(GoldenAutomaticDimensions, TheFirstPassRefreshesAStaleMassObject)
{
    const std::unique_ptr<Rocket> rocket = makeTurned();
    const auto* chute = dynamic_cast<const MassObject*>(componentAtGoldenPath(*rocket, "/0/0/0"));
    ASSERT_NE(chute, nullptr);
    EXPECT_EQ(chute->getLength(), 0.01994459833795014) << "stale: of the radius of 19 mm";
    EXPECT_EQ(settledAfter(*rocket), 0);
    EXPECT_EQ(chute->getLength(), 0.00856123662306778);
    EXPECT_EQ(chute->getRadius(), 0.028999999999999998);
    EXPECT_EQ(automaticDimensions(*rocket), passOf(kTurned));
}

// Java compares two passes with List.equals(), that is Double.equals() value by value.
TEST(GoldenAutomaticDimensions, TwoPassesAreComparedAsJavaComparesItsLists)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(samePass({}, {}));
    EXPECT_TRUE(samePass({0.025, 1.0, 0.0}, {0.025, 1.0, 0.0}));
    EXPECT_FALSE(samePass({0.025, 1.0, 0.0}, {0.025, 1.0}));
    EXPECT_FALSE(samePass({0.025}, {0.025, 1.0}));
    EXPECT_FALSE(samePass({0.025, 1.0}, {0.025, 0.0}));
    EXPECT_FALSE(samePass({0.025}, {0.025000000000000005})) << "the next number";
    EXPECT_TRUE(samePass({nan, 1.0}, {nan, 1.0})) << "Double.equals(): NaN equals NaN";
    EXPECT_TRUE(samePass({nan}, {-nan}));
    EXPECT_FALSE(samePass({nan}, {1.0}));
    EXPECT_FALSE(samePass({1.0}, {nan}));
    EXPECT_FALSE(samePass({0.0}, {-0.0})) << "Double.equals(): 0.0 differs from -0.0";
    EXPECT_TRUE(samePass({-0.0}, {-0.0}));
}

/// What settleBy() makes of a design whose passes change @p changes times and then repeat.
struct Settling
{
    Result<int> result;
    int         passes{0};  ///< the passes that were made
};

/// settleBy() over passes that return another value each, until @p changes of them have changed.
[[nodiscard]] Settling settlingOf(int changes)
{
    int               made   = 0;
    const Result<int> result = settleBy(
        [&made, changes] {
            return std::vector<double>{static_cast<double>(std::min(made++, changes))};
        },
        "Endless");
    return {.result = result, .passes = made};
}

// AutomaticDimensions.settle(): a first pass, then passes until one repeats the pass before it;
// the result is the number of passes that changed something, and after MAX_PASSES = 20 passes
// that all did, Java throws IllegalStateException.
TEST(GoldenAutomaticDimensions, TheLoopCountsTheChangedPassesAndGivesUpAfterTwenty)
{
    EXPECT_EQ(kMaxSettlingPasses, 20);
    const Settling settled = settlingOf(0);
    EXPECT_EQ(settled.result.value_or(-1), 0);
    EXPECT_EQ(settled.passes, 2) << "the first pass, and one that repeats it";
    const Settling once = settlingOf(1);
    EXPECT_EQ(once.result.value_or(-1), 1);
    EXPECT_EQ(once.passes, 3);
    const Settling last = settlingOf(19);
    EXPECT_EQ(last.result.value_or(-1), 19);
    EXPECT_EQ(last.passes, 21) << "the first pass, and the twenty the loop allows";

    const Settling never = settlingOf(20);
    ASSERT_FALSE(never.result.has_value());
    EXPECT_EQ(never.result.error().code, ErrorCode::UNKNOWN);
    EXPECT_EQ(never.result.error().message,
              "The automatic dimensions of Endless did not settle in 20 passes");
    EXPECT_EQ(never.passes, 21);
    EXPECT_FALSE(settlingOf(1000).result.has_value());
}

}  // namespace
