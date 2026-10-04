// Where the planar fin sets stand, in the cases the golden inputs do not hold (their 48 fin sets
// are positioned TOP or BOTTOM with the RELATIVE angle method, no freeform one is canted, and the
// golden test rebuilds each on a body of its own): a canted freeform fin set, fin sets positioned
// ABSOLUTE and MIDDLE, the angle and radius methods, and fin sets on the bodies of a pod set and
// of a booster set. Every number is pinned with OpenRocket (ProbeFix, sections S3 to S6).

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::NoseCone;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::MathUtil::javaToRadians;

/// The tolerance of a pinned position or angle: most pass through sin and cos, whose last bit
/// differs between math libraries.
constexpr double kPinned = 1e-14;

/// The relative tolerance of a pinned area, volume, mass or inertia.
constexpr double kPinnedRelative = 1e-12;

/// @p actual within kPinnedRelative of @p expected.
void expectRelative(double expected, double actual, std::string_view what)
{
    EXPECT_NEAR(expected, actual, kPinnedRelative * expected) << what;
}

/// @p actual within kPinned of @p expected in x, y and z.
void expectNear(const Coordinate& expected, const Coordinate& actual, std::string_view what)
{
    EXPECT_NEAR(expected.x, actual.x, kPinned) << what << " x";
    EXPECT_NEAR(expected.y, actual.y, kPinned) << what << " y";
    EXPECT_NEAR(expected.z, actual.z, kPinned) << what << " z";
}

/// @p actual has the points @p expected, each within kPinned.
void expectPointsNear(const std::vector<Coordinate>& expected,
                      const std::vector<Coordinate>& actual, std::string_view what)
{
    ASSERT_EQ(expected.size(), actual.size()) << what;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        expectNear(expected[i], actual[i], std::string{what} + " " + std::to_string(i));
    }
}

/// @p actual has the values @p expected, each within kPinned.
void expectValuesNear(const std::vector<double>& expected, const std::vector<double>& actual,
                      std::string_view what)
{
    ASSERT_EQ(expected.size(), actual.size()) << what;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(expected[i], actual[i], kPinned) << what << " " << i;
    }
}

/// What OpenRocket gives for a fin set (ProbeFix.state()).
struct PinnedFinSet
{
    AxialMethod             axialMethod{AxialMethod::BOTTOM};
    double                  axialOffset{0};
    double                  positionX{0};
    double                  length{0};
    double                  span{0};
    Coordinate              finFront;
    std::vector<Coordinate> finPoints;
    /// The number of root points, and some of them by index.
    std::size_t                                     rootPointCount{0};
    std::vector<std::pair<std::size_t, Coordinate>> rootPoints;
    double                                          planformArea{0};
    double                                          volume{0};
    double                                          mass{0};
    double                                          cgX{0};
    double                                          longitudinalUnitInertia{0};
    double                                          rotationalUnitInertia{0};
    std::vector<Coordinate>                         bounds;
    Coordinate                                      boxMin;
    Coordinate                                      boxMax;
    std::vector<Coordinate>                         instanceOffsets;
    std::vector<Coordinate>                         instanceLocations;
    std::vector<double>                             instanceAngles;
    std::vector<Coordinate>                         componentLocations;
    /// The x of getComponentAngles() (y and z are 0).
    std::vector<double> componentAngles;
};

/// The position and the outline of @p fins.
void expectOutline(const PinnedFinSet& expected, const FinSet& fins)
{
    EXPECT_EQ(expected.axialMethod, fins.getAxialMethod());
    EXPECT_NEAR(expected.axialOffset, fins.getAxialOffset(), kPinned);
    expectNear(Coordinate{expected.positionX, 0, 0}, fins.getPosition(), "position");
    EXPECT_NEAR(expected.length, fins.getLength(), kPinned);
    EXPECT_NEAR(expected.span, fins.getSpan(), kPinned);
    expectNear(expected.finFront, fins.getFinFront(), "fin front");
    EXPECT_NEAR(expected.finFront.y, fins.getBodyRadius(), kPinned);
    expectPointsNear(expected.finPoints, fins.getFinPoints(), "fin point");

    const std::vector<Coordinate> rootPoints = fins.getRootPoints();
    ASSERT_EQ(expected.rootPointCount, rootPoints.size());
    for (const auto& [index, point] : expected.rootPoints)
    {
        expectNear(point, rootPoints.at(index), "root point " + std::to_string(index));
    }
}

/// The area, mass and inertia of @p fins.
void expectMass(const PinnedFinSet& expected, const FinSet& fins)
{
    expectRelative(expected.planformArea, fins.getPlanformArea(), "planform area");
    expectRelative(expected.volume, fins.getComponentVolume(), "volume");
    expectRelative(expected.mass, fins.getComponentMass(), "mass");
    const Coordinate cg = fins.getComponentCG();
    expectNear(Coordinate{expected.cgX, 0, 0}, cg, "CG");
    expectRelative(expected.mass, cg.weight, "CG weight");
    expectRelative(expected.longitudinalUnitInertia, fins.getLongitudinalUnitInertia(),
                   "longitudinal unit inertia");
    expectRelative(expected.rotationalUnitInertia, fins.getRotationalUnitInertia(),
                   "rotational unit inertia");
}

/// The bounds and the places and angles of the fins of @p fins.
void expectPlacement(const PinnedFinSet& expected, const FinSet& fins)
{
    expectPointsNear(expected.bounds, fins.getComponentBounds(), "bound");
    const BoundingBox box = fins.getInstanceBoundingBox();
    expectNear(expected.boxMin, box.min(), "box minimum");
    expectNear(expected.boxMax, box.max(), "box maximum");
    expectPointsNear(expected.instanceOffsets, fins.getInstanceOffsets(), "instance offset");
    expectPointsNear(expected.instanceLocations, fins.getInstanceLocations(), "instance location");
    expectValuesNear(expected.instanceAngles, fins.getInstanceAngles(), "instance angle");
    expectPointsNear(expected.componentLocations, fins.getComponentLocations(),
                     "component location");

    const std::vector<Coordinate> angles = fins.getComponentAngles();
    ASSERT_EQ(expected.componentAngles.size(), angles.size());
    for (std::size_t i = 0; i < angles.size(); ++i)
    {
        expectNear(Coordinate{expected.componentAngles[i], 0, 0}, angles[i],
                   "component angle " + std::to_string(i));
    }
}

/// Everything ProbeFix.state() printed.
void expectMatches(const PinnedFinSet& expected, const FinSet& fins)
{
    expectOutline(expected, fins);
    expectMass(expected, fins);
    expectPlacement(expected, fins);
}

/// A stage with an ogive nose cone (0.07 m long, radius 0.012 m) and a body tube (0.20 m long,
/// radius 0.012 m, wall 0.3 mm): the Estes Alpha III without its nose shoulder.
class FinPlacement : public ::testing::Test
{
protected:
    FinPlacement()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_nose = &stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body = &stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
    }

    Rocket    m_rocket;
    NoseCone* m_nose{nullptr};
    BodyTube* m_body{nullptr};
};

// ============================================================== a canted freeform fin set

/// Four airfoil fins canted by 5 degrees, 0.02 m ahead of the middle of the tube, from 0.3 rad.
[[nodiscard]] PinnedFinSet cantedFreeform()
{
    return PinnedFinSet{
        .axialMethod             = AxialMethod::MIDDLE,
        .axialOffset             = -0.02,
        .positionX               = 0.05500000000000001,
        .length                  = 0.05,
        .span                    = 0.05,
        .finFront                = Coordinate{0.05500000000000001, 0.012},
        .finPoints               = {Coordinate{0.0, -1.9947362121862725E-4}, Coordinate{0.02, 0.05},
                                    Coordinate{0.06, 0.04},
                                    Coordinate{0.050000000000000044, -1.9947362121862898E-4}},
        .rootPointCount          = 21,
        .rootPoints              = {{0, Coordinate{0.0, -1.9947362121862725E-4}},
                                    {1, Coordinate{0.0025000000000000022, -1.6131500202766838E-4}},
                                    {2, Coordinate{0.0050000000000000044, -1.2727703505040038E-4}},
                                    {10, Coordinate{0.025000000000000022, 0.0}},
                                    {19, Coordinate{0.04750000000000004, -1.6131500202766838E-4}},
                                    {20, Coordinate{0.050000000000000044, -1.9947362121862898E-4}}},
        .planformArea            = 0.00210233282322025,
        .volume                  = 1.42958631978977E-5,
        .mass                    = 0.009721186974570436,
        .cgX                     = 0.03145227471933683,
        .longitudinalUnitInertia = 8.72690424241673E-4,
        .rotationalUnitInertia   = 0.0013949920446133044,
        .bounds = {Coordinate{0.0, -0.062, -0.062}, Coordinate{0.18509513254770638, 0.062, 0.062}},
        .boxMin = Coordinate{0.0, -1.9947362121862898E-4, -0.001},
        .boxMax = Coordinate{0.06, 0.05, 0.001},
        .instanceOffsets =
            {Coordinate{9.513254770636068E-5, 0.01082013079179451, 0.005627819012028131},
             Coordinate{9.513254770636068E-5, -0.005627819012028131, 0.01082013079179451},
             Coordinate{9.513254770636068E-5, -0.010820130791794513, -0.005627819012028128},
             Coordinate{9.513254770636068E-5, 0.005627819012028128, -0.010820130791794513}},
        .instanceLocations =
            {Coordinate{0.05509513254770637, 0.01082013079179451, 0.005627819012028131},
             Coordinate{0.05509513254770637, -0.005627819012028131, 0.01082013079179451},
             Coordinate{0.05509513254770637, -0.010820130791794513, -0.005627819012028128},
             Coordinate{0.05509513254770637, 0.005627819012028128, -0.010820130791794513}},
        .instanceAngles = {0.3, 1.8707963267948966, 3.441592653589793, 5.0123889803846895},
        .componentLocations =
            {Coordinate{0.12509513254770638, 0.01082013079179451, 0.005627819012028131},
             Coordinate{0.12509513254770638, -0.005627819012028131, 0.01082013079179451},
             Coordinate{0.12509513254770638, -0.010820130791794513, -0.005627819012028128},
             Coordinate{0.12509513254770638, 0.005627819012028128, -0.010820130791794513}},
        .componentAngles = {0.3, 1.8707963267948966, 3.441592653589793, 5.0123889803846895}};
}

/// cantedFreeform() once its own fin points have been set as its outline.
[[nodiscard]] PinnedFinSet cantedFreeformFedBack()
{
    PinnedFinSet pinned   = cantedFreeform();
    pinned.positionX      = 0.05499999999999998;
    pinned.length         = 0.050000000000000044;
    pinned.span           = 0.05019947362121863;
    pinned.finFront       = Coordinate{0.05499999999999998, 0.012};
    pinned.finPoints      = {Coordinate{0.0, -1.9947362121862898E-4},
                             Coordinate{0.02, 0.05019947362121863},
                             Coordinate{0.06, 0.04019947362121863},
                             Coordinate{0.050000000000000114, -1.9947362121862898E-4}};
    pinned.rootPointCount = 22;
    pinned.rootPoints     = {{0, Coordinate{0.0, -1.9947362121862898E-4}},
                             {1, Coordinate{0.0023809523809523864, -1.6303834137803598E-4}},
                             {2, Coordinate{0.004761904761904773, -1.3034221428769553E-4}},
                             {11, Coordinate{0.02619047619047625, -4.4857009822048266E-7}},
                             {20, Coordinate{0.04761904761904773, -1.6303834137803772E-4}},
                             {21, Coordinate{0.050000000000000114, -1.9947362121862898E-4}}};
    pinned.planformArea   = 0.002111307577735103;
    pinned.volume         = 1.43568915285987E-5;
    pinned.mass           = 0.009762686239447116;
    pinned.cgX            = 0.03145555294620471;
    pinned.longitudinalUnitInertia = 8.767747921570798E-4;
    pinned.rotationalUnitInertia   = 0.0014030632436163087;
    pinned.bounds = {Coordinate{0.0, -0.06219947362121862, -0.06219947362121862},
                     Coordinate{0.18509513254770635, 0.06219947362121862, 0.06219947362121862}};
    pinned.boxMax = Coordinate{0.06, 0.05019947362121863, 0.001};
    return pinned;
}

TEST_F(FinPlacement, CantedFreeformFinSetMatchesOpenRocket)
{
    FreeformFinSet& fins = m_body->addChild(std::make_unique<FreeformFinSet>());
    m_rocket.enableEvents();
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02, 0.05},
                                         Coordinate{0.06, 0.04}, Coordinate{0.05, 0}};
    fins.setPoints(points);
    fins.setFinCount(4);
    fins.setThickness(0.002);
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    fins.setCantAngle(javaToRadians(5));
    fins.setAxialMethod(AxialMethod::MIDDLE);
    fins.setAxialOffset(-0.02);
    fins.setBaseRotation(0.3);

    // The ends of the outline lie on the canted root, which dips below the fin's own ends.
    expectMatches(cantedFreeform(), fins);
    EXPECT_TRUE(fins.isRootStraight());
}

// Kept from OpenRocket: the fin points of a canted freeform fin set are not the outline it was
// given (the ends are on the root), and they are what a .ork file stores. Set as the outline
// again, as loading the file does, they give a fin that is higher by the dip of the root.
TEST_F(FinPlacement, CantedFreeformFinPointsSetAgainGiveAnotherOutline)
{
    FreeformFinSet& fins = m_body->addChild(std::make_unique<FreeformFinSet>());
    m_rocket.enableEvents();
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02, 0.05},
                                         Coordinate{0.06, 0.04}, Coordinate{0.05, 0}};
    fins.setPoints(points);
    fins.setFinCount(4);
    fins.setThickness(0.002);
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    fins.setCantAngle(javaToRadians(5));
    fins.setAxialMethod(AxialMethod::MIDDLE);
    fins.setAxialOffset(-0.02);
    fins.setBaseRotation(0.3);

    fins.setPoints(fins.getFinPoints());
    expectMatches(cantedFreeformFedBack(), fins);

    EXPECT_TRUE(fins.setPoint(1, 0.025, 0.055).has_value());
    expectPointsNear({Coordinate{0.0, -1.9947362121862898E-4}, Coordinate{0.025, 0.055},
                      Coordinate{0.06, 0.04019947362121863},
                      Coordinate{0.050000000000000114, -1.9947362121862898E-4}},
                     fins.getFinPoints(), "fin point after setPoint(1)");
    expectRelative(0.002154326000992451, fins.getPlanformArea(), "area after setPoint(1)");

    // Without the cant the root is flat again, and the ends are back on it.
    fins.setCantAngle(0);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.025, 0.055},
                      Coordinate{0.06, 0.04019947362121863}, Coordinate{0.050000000000000044, 0.0}},
                     fins.getFinPoints(), "fin point without the cant");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.050000000000000044, 0.0}},
                     fins.getRootPoints(), "root point without the cant");
    expectRelative(0.0021524934202652338, fins.getPlanformArea(), "area without the cant");
}

// ======================================================================= ABSOLUTE and MIDDLE

/// The three fins of a fin set 0.2 m from the tip of the rocket on the tube of 0.012 m radius.
void expectFinsAtAbsolute(const FinSet& fins)
{
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_EQ(fins.getAxialOffset(), 0.2);
    EXPECT_NEAR(fins.getAxialOffset(AxialMethod::ABSOLUTE), 0.2, kPinned);
    expectPointsNear(
        {Coordinate{0.2, 0.012, 0.0}, Coordinate{0.2, -0.0059999999999999975, 0.010392304845413265},
         Coordinate{0.2, -0.006000000000000005, -0.01039230484541326}},
        fins.getComponentLocations(), "component location");
    // The upper x bound is the outline's largest x plus the absolute location.
    expectPointsNear({Coordinate{0.0, -0.062, -0.062}, Coordinate{0.25, 0.062, 0.062}},
                     fins.getComponentBounds(), "bound");
}

TEST_F(FinPlacement, AbsolutePositionIsFromTheTipOfTheRocket)
{
    TrapezoidFinSet& fins =
        m_body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    m_rocket.enableEvents();

    // Changing the method describes the same place differently: at the end of the tube.
    fins.setAxialMethod(AxialMethod::ABSOLUTE);
    EXPECT_NEAR(fins.getAxialOffset(), 0.22000000000000003, kPinned);
    EXPECT_NEAR(fins.getPosition().x, 0.15000000000000002, kPinned);

    fins.setAxialOffset(0.2);
    EXPECT_NEAR(fins.getPosition().x, 0.13, kPinned);
    EXPECT_NEAR(fins.getAxialOffset(AxialMethod::TOP), 0.13, kPinned);
    EXPECT_NEAR(fins.getAxialOffset(AxialMethod::MIDDLE), 0.05499999999999999, kPinned);
    EXPECT_NEAR(fins.getAxialOffset(AxialMethod::BOTTOM), -0.020000000000000018, kPinned);
    expectNear(Coordinate{0.13, 0.012}, fins.getFinFront(), "fin front");
    expectFinsAtAbsolute(fins);
    expectRelative(0.012240000000000001, fins.getComponentMass(), "mass");
    expectNear(Coordinate{0.029583333333333333, 0.0, 0.0}, fins.getComponentCG(), "CG");

    // The nose cone grows by 0.03 m: the fin set stays 0.2 m from the tip, so it moves forward
    // on the tube.
    m_nose->setLength(0.1);
    EXPECT_NEAR(fins.getPosition().x, 0.1, kPinned);
    expectFinsAtAbsolute(fins);
}

TEST_F(FinPlacement, MiddlePositionIsFromTheMiddleOfTheParent)
{
    TrapezoidFinSet& fins =
        m_body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    m_rocket.enableEvents();
    m_nose->setLength(0.1);
    fins.setAxialMethod(AxialMethod::ABSOLUTE);
    fins.setAxialOffset(0.2);

    fins.setAxialMethod(AxialMethod::MIDDLE);
    EXPECT_NEAR(fins.getAxialOffset(), 0.024999999999999994, kPinned);
    EXPECT_NEAR(fins.getPosition().x, 0.1, kPinned);

    fins.setAxialOffset(0.01);
    EXPECT_NEAR(fins.getPosition().x, 0.085, kPinned);
    EXPECT_NEAR(fins.getAxialOffset(AxialMethod::ABSOLUTE), 0.185, kPinned);
    expectPointsNear({Coordinate{0.0, -0.062, -0.062}, Coordinate{0.235, 0.062, 0.062}},
                     fins.getComponentBounds(), "bound");
}

TEST_F(FinPlacement, AbsoluteFinSetThatOverhangsTheTube)
{
    TrapezoidFinSet& fins =
        m_body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    m_rocket.enableEvents();
    m_nose->setLength(0.1);

    // 0.18 m behind the front of the 0.2 m tube: the root gets a point where the tube ends.
    fins.setAxialMethod(AxialMethod::ABSOLUTE);
    fins.setAxialOffset(0.28);
    EXPECT_NEAR(fins.getPosition().x, 0.18000000000000002, kPinned);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
                      Coordinate{0.05000000000000002, 0.0}},
                     fins.getFinPoints(), "fin point");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.01999999999999999, 0.0},
                      Coordinate{0.05000000000000002, 0.0}},
                     fins.getRootPoints(), "root point");
    expectRelative(0.0019999999999999996, fins.getPlanformArea(), "area");
    const Coordinate cg = fins.getComponentCG();
    expectNear(Coordinate{0.029583333333333333, 0.0, 0.0}, cg, "CG");
    expectRelative(0.012239999999999997, cg.weight, "mass");
}

// Kept from OpenRocket: moving the ends of a freeform fin set adjusts the offset of one
// positioned TOP, MIDDLE or BOTTOM so that the other points stay where they are on the parent;
// positioned ABSOLUTE the offset and the position stay, and the outline moves instead.
TEST_F(FinPlacement, FreeformPositionedAbsoluteKeepsItsOffsetWhenItsEndsMove)
{
    FreeformFinSet& fins = m_body->addChild(std::make_unique<FreeformFinSet>());
    m_rocket.enableEvents();
    fins.setAxialMethod(AxialMethod::ABSOLUTE);
    fins.setAxialOffset(0.15);
    EXPECT_NEAR(fins.getPosition().x, 0.07999999999999999, kPinned);

    EXPECT_TRUE(fins.setPoint(0, 0.01, 0.0).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.015000000000000001, 0.05},
                      Coordinate{0.065, 0.05}, Coordinate{0.04000000000000001, 0.0}},
                     fins.getFinPoints(), "fin point after setPoint(0)");
    EXPECT_EQ(fins.getAxialOffset(), 0.15);
    EXPECT_NEAR(fins.getPosition().x, 0.07999999999999999, kPinned);
    EXPECT_NEAR(fins.getLength(), 0.04, kPinned);

    EXPECT_TRUE(fins.setPoint(3, 0.06, 0.0).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.015000000000000001, 0.05},
                      Coordinate{0.065, 0.05}, Coordinate{0.06, 0.0}},
                     fins.getFinPoints(), "fin point after setPoint(3)");
    EXPECT_EQ(fins.getAxialOffset(), 0.15);
    EXPECT_NEAR(fins.getPosition().x, 0.07999999999999999, kPinned);
    EXPECT_NEAR(fins.getLength(), 0.06, kPinned);
}

// ================================================================ angle and radius methods

/// Three fins from 0.4 rad on the tube of 0.012 m radius, whatever the angle method is called.
void expectThreeFinsFromPointFour(const FinSet& fins, const std::string& method)
{
    EXPECT_EQ(fins.getAngleOffset(), 0.4) << method;
    EXPECT_EQ(fins.getBaseRotation(), 0.4) << method;
    expectValuesNear({0.4, 2.494395102393195, 4.588790204786391}, fins.getInstanceAngles(),
                     method + " instance angle");
    expectPointsNear({Coordinate{0.4, 0.0, 0.0}, Coordinate{2.494395102393195, 0.0, 0.0},
                      Coordinate{4.588790204786391, 0.0, 0.0}},
                     fins.getComponentAngles(), method + " component angle");
    expectPointsNear({Coordinate{0.0, 0.011052731928034621, 0.004673020107703807},
                      Coordinate{0.0, -0.009573320089684298, 0.0072354365770454405},
                      Coordinate{0.0, -0.0014794118383503222, -0.011908456684749243}},
                     fins.getInstanceOffsets(), method + " instance offset");
}

TEST_F(FinPlacement, AngleMethodsDoNotTurnTheFins)
{
    TrapezoidFinSet& fins =
        m_body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    m_rocket.enableEvents();

    // The angle method is stored; the fins stand at the same angles under each.
    for (const AngleMethod method : QtRocket::kAllAngleMethods)
    {
        fins.setAngleMethod(method);
        fins.setAngleOffset(0.4);
        EXPECT_EQ(fins.getAngleMethod(), method);
        expectThreeFinsFromPointFour(fins, std::string{QtRocket::angleMethodName(method)});
    }
}

TEST_F(FinPlacement, RadiusMethodsDoNotMoveTheFins)
{
    TrapezoidFinSet& fins =
        m_body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    m_rocket.enableEvents();
    const std::vector<Coordinate> offsets = fins.getInstanceOffsets();

    // The fins stand on the parent's surface, whatever is asked for.
    for (const RadiusMethod method : QtRocket::kAllRadiusMethods)
    {
        fins.setRadiusMethod(method);
        fins.setRadius(method, 0.3);
        fins.setRadiusOffset(0.2);
        EXPECT_EQ(fins.getRadiusMethod(), RadiusMethod::SURFACE);
        EXPECT_EQ(fins.getRadiusOffset(), 0.0);
    }
    expectPointsNear(offsets, fins.getInstanceOffsets(), "instance offset");
    EXPECT_NEAR(fins.getBodyRadius(), 0.012, kPinned);
}

// ============================================================ pods and boosters

/// A core tube (0.30 m long, radius 0.02 m) behind an ogive nose cone, carrying
/// - three pods 0.05 m behind its front, turned by 0.2 rad, each a tube (0.10 m, radius 0.008 m)
///   with three trapezoidal fins canted by 0.05 rad from 0.5 rad, and
/// - two boosters ending 0.02 m behind its end, turned by -0.4 rad, each a conical nose (0.05 m)
///   and a tube (0.15 m, radius 0.01 m) with two default freeform fins from 1 rad, 0.03 m behind
///   the front of the tube.
class FinsOnPodsAndBoosters : public ::testing::Test
{
protected:
    FinsOnPodsAndBoosters()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.02));
        BodyTube& core = stage.addChild(std::make_unique<BodyTube>(0.30, 0.02, 0.001));

        PodSet& pods = core.addChild(std::make_unique<PodSet>());
        pods.setInstanceCount(3);
        pods.setAxialMethod(AxialMethod::TOP);
        pods.setAxialOffset(0.05);
        pods.setAngleOffset(0.2);
        m_podBody = &pods.addChild(std::make_unique<BodyTube>(0.10, 0.008, 0.0005));
        m_podFins =
            &m_podBody->addChild(std::make_unique<TrapezoidFinSet>(3, 0.04, 0.02, 0.01, 0.03));

        ParallelStage& boosters = core.addChild(std::make_unique<ParallelStage>(2));
        boosters.setAxialMethod(AxialMethod::BOTTOM);
        boosters.setAxialOffset(0.02);
        boosters.setRadius(RadiusMethod::SURFACE, 0.005);
        boosters.setAngleOffset(-0.4);
        boosters.addChild(std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.05, 0.01));
        m_boosterBody = &boosters.addChild(std::make_unique<BodyTube>(0.15, 0.01, 0.0005));
        m_boosterFins = &m_boosterBody->addChild(std::make_unique<FreeformFinSet>());

        m_rocket.enableEvents();
        m_podFins->setBaseRotation(0.5);
        m_podFins->setCantAngle(0.05);
        m_boosterFins->setFinCount(2);
        m_boosterFins->setAngleOffset(1.0);
        m_boosterFins->setAxialMethod(AxialMethod::TOP);
        m_boosterFins->setAxialOffset(0.03);
    }

    Rocket           m_rocket;
    BodyTube*        m_podBody{nullptr};
    TrapezoidFinSet* m_podFins{nullptr};
    BodyTube*        m_boosterBody{nullptr};
    FreeformFinSet*  m_boosterFins{nullptr};
};

/// The three canted fins on each of the three pods.
[[nodiscard]] PinnedFinSet finsOnPods()
{
    return PinnedFinSet{
        .axialMethod    = AxialMethod::BOTTOM,
        .axialOffset    = 0.0,
        .positionX      = 0.060000000000000005,
        .length         = 0.04,
        .span           = 0.03,
        .finFront       = Coordinate{0.060000000000000005, 0.008},
        .finPoints      = {Coordinate{0.0, -6.269358940924098E-5}, Coordinate{0.01, 0.03},
                           Coordinate{0.03, 0.03}, Coordinate{0.04, -6.269358940924272E-5}},
        .rootPointCount = 17,
        .rootPoints     = {{0, Coordinate{0.0, -6.269358940924098E-5}},
                           {1, Coordinate{0.0024999999999999953, -4.7955432205017134E-5}},
                           {2, Coordinate{0.0049999999999999975, -3.520442234843796E-5}},
                           {8, Coordinate{0.02000000000000001, 0.0}},
                           {15, Coordinate{0.037500000000000026, -4.7955432205017134E-5}},
                           {16, Coordinate{0.04, -6.269358940924272E-5}}},
        .planformArea   = 9.002142203445757E-4,
        .volume         = 8.101927983101182E-6,
        .mass           = 0.005509311028508803,
        .cgX            = 0.02,
        .longitudinalUnitInertia = 3.484859955391943E-4,
        .rotationalUnitInertia   = 4.969243865573718E-4,
        // The upper x bound adds the location of the first fin of the first pod.
        .bounds = {Coordinate{0.0, -0.038, -0.038}, Coordinate{0.2200249947921007, 0.038, 0.038}},
        .boxMin = Coordinate{0.0, -6.269358940924272E-5, -0.0015},
        .boxMax = Coordinate{0.04, 0.03, 0.0015},
        .instanceOffsets =
            {Coordinate{2.4994792100673652E-5, 0.00654143469219127, 0.004712621257027914},
             Coordinate{2.4994792100673652E-5, -0.0073519670730963615, 0.0033087379921205237},
             Coordinate{2.4994792100673652E-5, 8.105323809050891E-4, -0.008021359249148437}},
        .instanceLocations =
            {Coordinate{0.060024994792100675, 0.00654143469219127, 0.004712621257027914},
             Coordinate{0.060024994792100675, -0.0073519670730963615, 0.0033087379921205237},
             Coordinate{0.060024994792100675, 8.105323809050891E-4, -0.008021359249148437}},
        .instanceAngles = {0.5, 2.5943951023931953, 4.6887902047863905},
        // Pod by pod for each fin: the first fin on the three pods, then the second, the third.
        .componentLocations =
            {Coordinate{0.18002499479210068, 0.032916652381078326, 0.011481006303036188},
             Coordinate{0.18002499479210068, -0.02640116930997776, 0.022766154018037266},
             Coordinate{0.18002499479210068, -0.006515483071100572, -0.03424716032107345},
             Coordinate{0.18002499479210068, 0.01957910220715295, 0.007344914404733198},
             Coordinate{0.18002499479210068, -0.016150433566697677, 0.013283542692319825},
             Coordinate{0.18002499479210068, -0.0034286686404552723, -0.02062845709705302},
             Coordinate{0.18002499479210068, 0.029829837950433025, -0.0021376969209842434},
             Coordinate{0.18002499479210068, -0.01306361913605238, 0.026902245916340252},
             Coordinate{0.18002499479210068, -0.016766218814380653, -0.024764548995356007}},
        // The pod's angle plus the fin's, not reduced to a turn.
        .componentAngles = {0.7, 2.7943951023931954, 4.888790204786391, 2.7943951023931954,
                            4.888790204786391, 6.9831853071795855, 4.888790204786391,
                            6.9831853071795855, 9.077580409572782}};
}

/// The two default freeform fins on each of the two boosters.
[[nodiscard]] PinnedFinSet finsOnBoosters()
{
    return PinnedFinSet{
        .axialMethod    = AxialMethod::TOP,
        .axialOffset    = 0.03,
        .positionX      = 0.03,
        .length         = 0.05,
        .span           = 0.05,
        .finFront       = Coordinate{0.03, 0.01},
        .finPoints      = {Coordinate{0.0, 0.0}, Coordinate{0.025, 0.05}, Coordinate{0.075, 0.05},
                           Coordinate{0.05, 0.0}},
        .rootPointCount = 2,
        .rootPoints     = {{0, Coordinate{0.0, 0.0}}, {1, Coordinate{0.05, 0.0}}},
        .planformArea   = 0.0025,
        .volume         = 1.5E-5,
        .mass           = 0.0102,
        .cgX            = 0.03749999999999999,
        .longitudinalUnitInertia = 9.25E-4,
        .rotationalUnitInertia   = 0.0014333333333333336,
        .bounds = {Coordinate{0.0, -0.060000000000000005, -0.060000000000000005},
                   Coordinate{0.34500000000000003, 0.060000000000000005, 0.060000000000000005}},
        .boxMin = Coordinate{0.0, 0.0, -0.0015},
        .boxMax = Coordinate{0.075, 0.05, 0.0015},
        .instanceOffsets    = {Coordinate{0.0, 0.005403023058681398, 0.008414709848078966},
                               Coordinate{0.0, -0.005403023058681398, -0.008414709848078964}},
        .instanceLocations  = {Coordinate{0.03, 0.005403023058681398, 0.008414709848078966},
                               Coordinate{0.03, -0.005403023058681398, -0.008414709848078964}},
        .instanceAngles     = {1.0, 4.141592653589793},
        .componentLocations = {Coordinate{0.27, 0.03588518596918334, -0.006036125535309161},
                               Coordinate{0.27, -0.03588518596918334, 0.006036125535309161},
                               Coordinate{0.27, 0.019378473670989767, -0.017328975003209866},
                               Coordinate{0.27, -0.019378473670989767, 0.017328975003209866}},
        .componentAngles    = {0.6, 3.741592653589793, 3.741592653589793, 6.883185307179586}};
}

TEST_F(FinsOnPodsAndBoosters, FinsOnPodsMatchOpenRocket)
{
    expectPointsNear(
        {Coordinate{0.12000000000000001, 0.027441864179554767, 0.0055627412622617145},
         Coordinate{0.12000000000000001, -0.01853840733757594, 0.020983980875565782},
         Coordinate{0.12000000000000001, -0.008903456841978833, -0.026546722137827494}},
        m_podBody->getComponentLocations(), "pod body location");
    expectMatches(finsOnPods(), *m_podFins);
}

TEST_F(FinsOnPodsAndBoosters, FinsOnBoostersMatchOpenRocket)
{
    expectPointsNear({Coordinate{0.24, 0.02763182982008655, -0.011682550269259516},
                      Coordinate{0.24, -0.02763182982008655, 0.011682550269259516}},
                     m_boosterBody->getComponentLocations(), "booster body location");
    expectMatches(finsOnBoosters(), *m_boosterFins);
}

}  // namespace
