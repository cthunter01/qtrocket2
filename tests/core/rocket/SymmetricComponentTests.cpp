#include "QtRocket/rocket/SymmetricComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::MathUtil::pow2;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestFalcon9Heavy;

/// SymmetricComponentVolumeTest's tolerance (MathUtil.EPSILON * 1000).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon * 1000;

/// Java's protected setAxialOffset(method, offset), which the JUnit tests (in the same package)
/// call: here the public setAxialMethod() and setAxialOffset(), which end in the same place.
void setAxialOffset(RocketComponent& component, AxialMethod method, double offset)
{
    component.setAxialMethod(method);
    component.setAxialOffset(offset);
}

/// A test bulk material of @p density.
[[nodiscard]] Material testMaterial(double density)
{
    return Material::newMaterial(Material::Type::BULK, "test", density, true);
}

// ================================================================ SymmetricComponentVolumeTest
// The helper functions of SymmetricComponentVolumeTest: closed-form volumes, CGs and moments of
// inertia of shoulders, cones and conical frustums.

/// The height of a frustum wall: the thickness projected onto the yz plane.
[[nodiscard]] double getHeight(double length, double foreRadius, double aftRadius, double thickness)
{
    const double angle = std::atan2(aftRadius - foreRadius, length);
    return thickness / std::cos(angle);
}

/// The CG and volume of a (possibly hollow, if thickness < outerR) shoulder.
[[nodiscard]] Coordinate calculateShoulderCG(double x1, double length, double outerR,
                                             double thickness)
{
    const double cg = x1 + (length / 2.0);

    const double innerR = std::max(0.0, outerR - thickness);
    const double volume = std::numbers::pi * length * (pow2(outerR) - pow2(innerR));

    return Coordinate{cg, 0, 0, volume};
}

/// The CG and volume of a frustum (a cone when the fore radius is 0).
[[nodiscard]] Coordinate calculateFrustumCG(double length, double foreRadius, double aftRadius)
{
    const double moment =
        std::numbers::pi * pow2(length) *
        (pow2(foreRadius) + (2.0 * foreRadius * aftRadius) + (3.0 * pow2(aftRadius))) / 12.0;
    const double volume = std::numbers::pi * length *
                          (pow2(foreRadius) + (foreRadius * aftRadius) + pow2(aftRadius)) / 3.0;

    return Coordinate{moment / volume, 0, 0, volume};
}

/// The CG and volume of a conical transition.
[[nodiscard]] Coordinate calculateConicalTransitionCG(double length, double foreRadius,
                                                      double aftRadius, double thickness)
{
    // get moment and volume of outer frustum
    const Coordinate fullCG = calculateFrustumCG(length, foreRadius, aftRadius);

    const double height = getHeight(length, foreRadius, aftRadius, thickness);

    // if aftRadius <= height the transition is filled and we don't need to mess with the inner
    // frustum
    if (aftRadius <= height)
    {
        return fullCG;
    }

    double       innerLen     = length;
    double       innerForeRad = foreRadius - height;
    const double innerAftRad  = aftRadius - height;

    // if inner forward radius <= 0 the transition is a cone; we need to determine its length (if
    // it's exactly equal the inner length is the same as the outer)
    if (innerForeRad < 0)
    {
        innerLen     = length * (aftRadius - height) / (aftRadius - foreRadius);
        innerForeRad = 0;
    }

    const Coordinate innerCG = calculateFrustumCG(innerLen, innerForeRad, innerAftRad);

    // subtract inner from outer
    const double offset = length - innerLen;
    const double moment = (fullCG.x * fullCG.weight) - ((innerCG.x + offset) * innerCG.weight);
    const double volume = fullCG.weight - innerCG.weight;

    return Coordinate{moment / volume, 0, 0, volume};
}

/// Combines five CGs (fore cap, fore shoulder, transition, aft shoulder, aft cap); pass
/// Coordinate::kZero for the missing ones.
[[nodiscard]] Coordinate combineCG(const Coordinate& cg1, const Coordinate& cg2,
                                   const Coordinate& cg3, const Coordinate& cg4,
                                   const Coordinate& cg5)
{
    const double moment1 = cg1.x * cg1.weight;
    const double moment2 = cg2.x * cg2.weight;
    const double moment3 = cg3.x * cg3.weight;
    const double moment4 = cg4.x * cg4.weight;
    const double moment5 = cg5.x * cg5.weight;

    const double volume = cg1.weight + cg2.weight + cg3.weight + cg4.weight + cg5.weight;
    return Coordinate{(moment1 + moment2 + moment3 + moment4 + moment5) / volume, 0, 0, volume};
}

/// checkCG(): the CG, volume and mass of @p nc.
void checkCG(const Coordinate& expectedCG, const Transition& nc)
{
    const Coordinate cg = nc.getCG();
    EXPECT_NEAR(expectedCG.x, cg.x, kEpsilon) << "CG is incorrect";
    EXPECT_NEAR(expectedCG.weight, nc.getComponentVolume(), kEpsilon) << "Volume is incorrect";

    const double mass = expectedCG.weight * nc.getMaterial().getDensity();
    EXPECT_NEAR(mass, nc.getMass(), kEpsilon) << "Mass is incorrect";
    EXPECT_NEAR(mass, cg.weight, kEpsilon) << "Mass (stored in cg.getWeight()) is incorrect";
}

/// The longitudinal moment of inertia of a shoulder about an axis @p cgShift from its CG.
[[nodiscard]] double calculateShoulderLongitudinalMOI(double length, double outerRad,
                                                      double thickness, double cgShift)
{
    const double innerRad = std::max(0.0, outerRad - thickness);
    const double volume   = length * std::numbers::pi * (pow2(outerRad) - pow2(innerRad));
    const double moi = volume * ((3 * (pow2(outerRad) + pow2(innerRad))) + pow2(length)) / 12.0;

    // parallel axis theorem
    return moi + (volume * pow2(cgShift));
}

/// The longitudinal moment of inertia of a cone about its point.
[[nodiscard]] double calculateLongMOICone(double h, double r)
{
    const double m       = std::numbers::pi * pow2(r) * h / 3.0;
    const double unitIyy = (3.0 * pow2(r) / 20.0) + (3.0 * pow2(h) / 5.0);
    return m * unitIyy;
}

/// The longitudinal moment of inertia of a solid conical frustum about its aft end (two cones
/// subtracted).
[[nodiscard]] double calculateFrustumLongMOI(double length, double forwardRad, double aftRad)
{
    // Find the heights of the two cones.
    const double h2 = length * aftRad / (aftRad - forwardRad);
    const double h1 = h2 * forwardRad / aftRad;

    const double moi1 = calculateLongMOICone(h1, forwardRad);
    const double moi2 = calculateLongMOICone(h2, aftRad);

    // compute MOI relative to tip of cones (they share the same tip, of course)
    double moi = moi2 - moi1;

    // use parallel axis theorem to move MOI to be relative to CG of frustum
    const Coordinate cg = calculateFrustumCG(length, forwardRad, aftRad);
    moi                 = moi - (pow2(cg.x + h1) * cg.weight);

    // move MOI to be relative to back surface of frustum
    moi = moi + (pow2(length - cg.x) * cg.weight);

    return moi;
}

/// The longitudinal moment of inertia of a (possibly hollow) conical frustum with CG @p cg about
/// an axis @p cgShift from its CG.
[[nodiscard]] double calculateConicalTransitionLongitudinalMOI(double length, double foreRadius,
                                                               double aftRadius, double thickness,
                                                               const Coordinate& cg, double cgShift)
{
    // get MOI of outer frustum, axis through base (aft end) of frustum
    const double fullMOI = calculateFrustumLongMOI(length, foreRadius, aftRadius);

    const double height = getHeight(length, foreRadius, aftRadius, thickness);

    // if frustum is solid, MOI of inner part is 0. If not, we need to calculate it
    double moi = 0;
    if (height >= aftRadius)
    {
        moi = fullMOI;
    }
    else
    {
        double       innerLen     = length;
        double       innerForeRad = foreRadius - height;
        const double innerAftRad  = aftRadius - height;

        // if inner forward radius <= 0 the transition is a cone; we need to determine its length
        if (innerForeRad < 0)
        {
            innerLen     = length * (aftRadius - height) / (aftRadius - foreRadius);
            innerForeRad = 0;
        }

        const double innerMOI = calculateFrustumLongMOI(innerLen, innerForeRad, innerAftRad);

        moi = fullMOI - innerMOI;
    }

    // use parallel axis theorem to move moi relative to CG
    moi = moi - (pow2(length - cg.x) * cg.weight);

    // now axis through component CG
    return moi + (cg.weight * pow2(cgShift));
}

/// The rotational moment of inertia of a shoulder.
[[nodiscard]] double calculateShoulderRotationalMOI(double length, double outerRad,
                                                    double thickness)
{
    const double innerRad = std::max(0.0, outerRad - thickness);

    return length * std::numbers::pi * (std::pow(outerRad, 4) - std::pow(innerRad, 4)) / 2.0;
}

/// The rotational moment of inertia of a solid frustum (a cone when the fore radius is 0).
[[nodiscard]] double calculateFrustumRotationalMOI(double length, double foreRadius,
                                                   double aftRadius)
{
    const double unitMOI = 3.0 * (std::pow(aftRadius, 5) - std::pow(foreRadius, 5)) /
                           (10.0 * (std::pow(aftRadius, 3) - std::pow(foreRadius, 3)));
    const double volume  = std::numbers::pi * length *
                           (pow2(foreRadius) + (foreRadius * aftRadius) + pow2(aftRadius)) / 3.0;

    return unitMOI * volume;
}

/// The rotational moment of inertia of a conical transition.
[[nodiscard]] double calculateConicalTransitionRotationalMOI(double length, double foreRadius,
                                                             double aftRadius, double thickness)
{
    // get MOI of outer frustum
    const double fullMOI = calculateFrustumRotationalMOI(length, foreRadius, aftRadius);
    const double height  = getHeight(length, foreRadius, aftRadius, thickness);

    // if aftRadius <= height the transition is filled and we don't need to mess with the inner
    // frustum
    if (aftRadius <= height)
    {
        return fullMOI;
    }

    double       innerLen     = length;
    double       innerForeRad = foreRadius - height;
    const double innerAftRad  = aftRadius - height;

    // if forward radius <= height the transition is a cone; we need to determine its length
    if (foreRadius < height)
    {
        innerLen     = length * (aftRadius - height) / (aftRadius - foreRadius);
        innerForeRad = 0;
    }

    const double innerMOI = calculateFrustumRotationalMOI(innerLen, innerForeRad, innerAftRad);

    return fullMOI - innerMOI;
}

/// checkConeSurface(): the planform area and centroid and the wetted area of a conical
/// transition.
void checkConeSurface(double length, double foreRadius, double aftRadius, const Transition& trans)
{
    const double planformArea = length * (foreRadius + aftRadius);
    const double planformCentroid =
        2.0 * pow2(length) * ((foreRadius / 6.0) + (aftRadius / 3.0)) / planformArea;
    const double surfaceArea = std::numbers::pi * (foreRadius + aftRadius) *
                               std::sqrt(pow2(foreRadius - aftRadius) + pow2(length));

    EXPECT_NEAR(planformArea, trans.getComponentPlanformArea(), kEpsilon)
        << "Planform Area is incorrect";
    EXPECT_NEAR(planformCentroid, trans.getComponentPlanformCenter(), kEpsilon)
        << "Planform Centroid is incorrect";
    EXPECT_NEAR(surfaceArea, trans.getComponentWetArea(), kEpsilon) << "Surface Area is incorrect";
}

TEST(SymmetricComponentVolume, SimpleConeFilled)
{
    const double length    = 1.0;
    const double aftRadius = 1.0;
    const double density   = 2.0;

    NoseCone nc;
    nc.setLength(length);
    nc.setFilled(true);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setAftRadius(aftRadius);
    nc.setMaterial(testMaterial(density));

    const Coordinate expectedCG = calculateConicalTransitionCG(length, 0, aftRadius, aftRadius);

    checkCG(expectedCG, nc);

    const double moi =
        calculateConicalTransitionLongitudinalMOI(length, 0, aftRadius, aftRadius, expectedCG, 0);
    const double expectedLongUnitMOI = moi / expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double expectedRotUnitMOI =
        calculateConicalTransitionRotationalMOI(length, 0, aftRadius, aftRadius) /
        expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, 0, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleConeWithShoulderFilled)
{
    const double length    = 1.0;
    const double aftRadius = 1.0;
    const double density   = 2.0;

    NoseCone nc;
    nc.setLength(length);
    nc.setFilled(true);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setAftRadius(aftRadius);
    nc.setAftShoulderRadius(aftRadius);
    nc.setAftShoulderLength(length);
    nc.setAftShoulderThickness(aftRadius);
    nc.setMaterial(testMaterial(density));

    const Coordinate coneCG     = calculateConicalTransitionCG(length, 0, aftRadius, aftRadius);
    const Coordinate shoulderCG = calculateShoulderCG(length, length, aftRadius, aftRadius);
    const Coordinate expectedCG = coneCG.average(shoulderCG);

    checkCG(expectedCG, nc);

    const double transitionLongMOI = calculateConicalTransitionLongitudinalMOI(
        length, 0, aftRadius, aftRadius, coneCG, expectedCG.x - coneCG.x);
    const double shoulderLongMOI =
        calculateShoulderLongitudinalMOI(length, aftRadius, aftRadius, expectedCG.x - shoulderCG.x);

    const double moi = (shoulderLongMOI + transitionLongMOI) / expectedCG.weight;
    EXPECT_NEAR(moi, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double coneRotMOI =
        calculateConicalTransitionRotationalMOI(length, 0, aftRadius, aftRadius);
    const double shoulderRotMOI     = calculateShoulderRotationalMOI(length, aftRadius, aftRadius);
    const double expectedRotUnitMOI = (coneRotMOI + shoulderRotMOI) / expectedCG.weight;

    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, 0, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleConeHollow)
{
    const double length    = 1.0;
    const double aftRadius = 1.0;
    const double thickness = 0.5;
    const double density   = 2.0;

    NoseCone nc;
    nc.setLength(length);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setMaterial(testMaterial(density));

    const Coordinate expectedCG = calculateConicalTransitionCG(length, 0.0, aftRadius, thickness);

    checkCG(expectedCG, nc);

    const double expectedLongUnitMOI = calculateConicalTransitionLongitudinalMOI(
                                           length, 0, aftRadius, thickness, expectedCG, 0.0) /
                                       expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double expectedRotUnitMOI =
        calculateConicalTransitionRotationalMOI(length, 0, aftRadius, thickness) /
        expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, 0, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleConeWithShoulderHollow)
{
    const double aftRadius = 1.0;
    const double length    = 1.0;
    const double thickness = 0.5;
    const double density   = 2.0;

    NoseCone nc;
    nc.setLength(length);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setAftShoulderRadius(aftRadius);
    nc.setAftShoulderLength(length);
    nc.setAftShoulderThickness(thickness);
    nc.setAftShoulderCapped(false);

    nc.setMaterial(testMaterial(density));

    const Coordinate coneCG     = calculateConicalTransitionCG(length, 0, aftRadius, thickness);
    const Coordinate shoulderCG = calculateShoulderCG(length, length, aftRadius, thickness);
    const Coordinate expectedCG = coneCG.average(shoulderCG);

    checkCG(expectedCG, nc);

    const double coneLongMOI = calculateConicalTransitionLongitudinalMOI(
        length, 0.0, aftRadius, thickness, coneCG, coneCG.x - expectedCG.x);
    const double shoulderLongMOI =
        calculateShoulderLongitudinalMOI(length, aftRadius, thickness, shoulderCG.x - expectedCG.x);

    const double longUnitMOI = (coneLongMOI + shoulderLongMOI) / expectedCG.weight;
    EXPECT_NEAR(longUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double coneRotMOI =
        calculateConicalTransitionRotationalMOI(length, 0, aftRadius, thickness);
    const double shoulderRotMOI     = calculateShoulderRotationalMOI(length, aftRadius, thickness);
    const double expectedRotUnitMOI = (coneRotMOI + shoulderRotMOI) / expectedCG.weight;

    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, 0, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionFilled)
{
    const double length     = 4.0;
    const double foreRadius = 1.0;
    const double aftRadius  = 2.0;
    const double density    = 2.0;

    Transition nc;
    nc.setLength(length);
    nc.setFilled(true);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setMaterial(testMaterial(density));

    const Coordinate expectedCG =
        calculateConicalTransitionCG(length, foreRadius, aftRadius, aftRadius);

    checkCG(expectedCG, nc);

    const double expectedLongUnitMOI =
        calculateConicalTransitionLongitudinalMOI(length, foreRadius, aftRadius, aftRadius,
                                                  expectedCG, 0.0) /
        expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double expectedRotUnitMOI =
        calculateConicalTransitionRotationalMOI(length, foreRadius, aftRadius, aftRadius) /
        expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionWithShouldersFilled)
{
    const double transLength        = 4.0;
    const double foreRadius         = 1.0;
    const double foreShoulderLength = 1.0;
    const double aftRadius          = 2.0;
    const double aftShoulderLength  = 1.0;
    const double density            = 2.0;

    Transition nc;
    nc.setLength(transLength);
    nc.setFilled(true);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setAftShoulderLength(aftShoulderLength);
    nc.setAftShoulderRadius(aftRadius);
    nc.setAftShoulderThickness(aftRadius);
    nc.setForeShoulderLength(foreShoulderLength);
    nc.setForeShoulderRadius(foreRadius);
    nc.setForeShoulderThickness(foreRadius);
    nc.setMaterial(testMaterial(density));

    const Coordinate foreShoulderCG =
        calculateShoulderCG(-foreShoulderLength, foreShoulderLength, foreRadius, foreRadius);
    const Coordinate transCG =
        calculateConicalTransitionCG(transLength, foreRadius, aftRadius, aftRadius);
    const Coordinate aftShoulderCG =
        calculateShoulderCG(transLength, aftShoulderLength, aftRadius, aftRadius);
    const Coordinate expectedCG =
        combineCG(Coordinate::kZero, foreShoulderCG, transCG, aftShoulderCG, Coordinate::kZero);

    checkCG(expectedCG, nc);

    const double foreShoulderLongMOI = calculateShoulderLongitudinalMOI(
        foreShoulderLength, foreRadius, foreRadius, expectedCG.x - foreShoulderCG.x);
    const double transLongMOI = calculateConicalTransitionLongitudinalMOI(
        transLength, foreRadius, aftRadius, aftRadius, transCG, expectedCG.x - transCG.x);
    const double aftShoulderLongMOI = calculateShoulderLongitudinalMOI(
        aftShoulderLength, aftRadius, aftRadius, expectedCG.x - aftShoulderCG.x);

    const double expectedLongUnitMOI =
        (foreShoulderLongMOI + transLongMOI + aftShoulderLongMOI) / expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double foreShoulderRotMOI =
        calculateShoulderRotationalMOI(foreShoulderLength, foreRadius, foreRadius);
    const double coneRotMOI =
        calculateConicalTransitionRotationalMOI(transLength, foreRadius, aftRadius, aftRadius);
    const double aftShoulderRotMOI =
        calculateShoulderRotationalMOI(aftShoulderLength, aftRadius, aftRadius);

    const double expectedRotUnitMOI =
        (foreShoulderRotMOI + coneRotMOI + aftShoulderRotMOI) / expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(transLength, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionHollow1)
{
    const double length     = 1.0;
    const double foreRadius = 0.5;
    const double aftRadius  = 1.0;
    const double thickness  = 0.5;
    const double density    = 2.0;

    Transition nc;
    nc.setLength(length);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setMaterial(testMaterial(density));

    const Coordinate expectedCG =
        calculateConicalTransitionCG(length, foreRadius, aftRadius, thickness);

    checkCG(expectedCG, nc);

    const double expectedLongUnitMOI =
        calculateConicalTransitionLongitudinalMOI(length, foreRadius, aftRadius, thickness,
                                                  expectedCG, 0.0) /
        expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double expectedRotUnitMOI =
        calculateConicalTransitionRotationalMOI(length, foreRadius, aftRadius, thickness) /
        expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionWithShouldersHollow1)
{
    const double length     = 1.0;  // length of transition itself and each shoulder
    const double foreRadius = 0.5;
    const double aftRadius  = 1.0;
    const double thickness  = 0.5;
    const double density    = 2.0;

    Transition nc;
    nc.setLength(length);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setAftShoulderLength(length);
    nc.setAftShoulderRadius(aftRadius);
    nc.setAftShoulderThickness(thickness);
    nc.setAftShoulderCapped(false);
    nc.setForeShoulderLength(length);
    nc.setForeShoulderRadius(foreRadius);
    nc.setForeShoulderThickness(thickness);  // note this means fore shoulder is filled.
    nc.setForeShoulderCapped(false);
    nc.setMaterial(testMaterial(density));

    const Coordinate foreShoulderCG = calculateShoulderCG(-length, length, foreRadius, thickness);
    const Coordinate coneCG =
        calculateConicalTransitionCG(length, foreRadius, aftRadius, thickness);
    const Coordinate aftShoulderCG = calculateShoulderCG(length, length, aftRadius, thickness);

    const Coordinate expectedCG =
        combineCG(Coordinate::kZero, foreShoulderCG, coneCG, aftShoulderCG, Coordinate::kZero);
    checkCG(expectedCG, nc);

    const double foreShoulderLongMOI = calculateShoulderLongitudinalMOI(
        length, foreRadius, thickness, foreShoulderCG.x - expectedCG.x);
    const double coneLongMOI = calculateConicalTransitionLongitudinalMOI(
        length, foreRadius, aftRadius, thickness, coneCG, coneCG.x - expectedCG.x);
    const double aftShoulderLongMOI = calculateShoulderLongitudinalMOI(
        length, aftRadius, thickness, aftShoulderCG.x - expectedCG.x);

    const double longUnitMOI =
        (foreShoulderLongMOI + coneLongMOI + aftShoulderLongMOI) / expectedCG.weight;
    EXPECT_NEAR(longUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double foreShoulderRotMOI = calculateShoulderRotationalMOI(length, foreRadius, thickness);
    const double coneRotMOI =
        calculateConicalTransitionRotationalMOI(length, foreRadius, aftRadius, thickness);
    const double aftShoulderRotMOI = calculateShoulderRotationalMOI(length, aftRadius, thickness);

    const double expectedRotUnitMOI =
        (foreShoulderRotMOI + coneRotMOI + aftShoulderRotMOI) / expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionHollow2)
{
    const double length     = 1.0;
    const double foreRadius = 0.5;
    const double aftRadius  = 1.0;
    const double thickness  = 0.25;
    const double density    = 2.0;

    Transition nc;
    nc.setLength(length);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setMaterial(testMaterial(density));

    const Coordinate expectedCG =
        calculateConicalTransitionCG(length, foreRadius, aftRadius, thickness);
    checkCG(expectedCG, nc);

    const double expectedLongUnitMOI =
        calculateConicalTransitionLongitudinalMOI(length, foreRadius, aftRadius, thickness,
                                                  expectedCG, 0.0) /
        expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit MOI is incorrect";

    const double expectedRotUnitMOI =
        calculateConicalTransitionRotationalMOI(length, foreRadius, aftRadius, thickness) /
        expectedCG.weight;
    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, SimpleTransitionWithShouldersHollow2)
{
    const double length     = 1.0;
    const double foreRadius = 0.5;
    const double aftRadius  = 1.0;
    const double thickness  = 0.25;
    const double density    = 2.0;

    Transition nc;
    nc.setLength(length);
    nc.setShapeType(TransitionShape::CONICAL);
    nc.setForeRadius(foreRadius);
    nc.setAftRadius(aftRadius);
    nc.setThickness(thickness);
    nc.setAftShoulderLength(length);
    nc.setAftShoulderRadius(aftRadius);
    nc.setAftShoulderThickness(thickness);
    nc.setAftShoulderCapped(true);
    nc.setForeShoulderLength(length);
    nc.setForeShoulderRadius(foreRadius);
    nc.setForeShoulderThickness(thickness);
    nc.setForeShoulderCapped(true);
    nc.setMaterial(testMaterial(density));

    // A cap is a solid disc as thick as the shoulder wall, inside the shoulder: a "shoulder" of
    // that length whose wall is as thick as the shoulder is wide.
    const double     capLength     = thickness;
    const double     foreCapRadius = foreRadius - thickness;
    const double     foreCapSolid  = foreRadius;
    const double     aftCapRadius  = aftRadius - thickness;
    const double     aftCapSolid   = aftRadius;
    const Coordinate foreCapCG =
        calculateShoulderCG(-length, capLength, foreCapRadius, foreCapSolid);
    const Coordinate foreShoulderCG = calculateShoulderCG(-length, length, foreRadius, thickness);
    const Coordinate coneCG =
        calculateConicalTransitionCG(length, foreRadius, aftRadius, thickness);
    const Coordinate aftShoulderCG = calculateShoulderCG(length, length, aftRadius, thickness);
    const Coordinate aftCapCG =
        calculateShoulderCG((2.0 * length) - capLength, capLength, aftCapRadius, aftCapSolid);

    const Coordinate expectedCG =
        combineCG(foreCapCG, foreShoulderCG, coneCG, aftShoulderCG, aftCapCG);
    checkCG(expectedCG, nc);

    const double foreCapLongMOI = calculateShoulderLongitudinalMOI(
        capLength, foreCapRadius, foreCapSolid, foreCapCG.x - expectedCG.x);
    const double foreShoulderLongMOI = calculateShoulderLongitudinalMOI(
        length, foreRadius, thickness, foreShoulderCG.x - expectedCG.x);
    const double coneLongMOI = calculateConicalTransitionLongitudinalMOI(
        length, foreRadius, aftRadius, thickness, coneCG, coneCG.x - expectedCG.x);
    const double aftShoulderLongMOI = calculateShoulderLongitudinalMOI(
        length, aftRadius, thickness, aftShoulderCG.x - expectedCG.x);
    const double aftCapLongMOI = calculateShoulderLongitudinalMOI(
        capLength, aftCapRadius, aftCapSolid, aftCapCG.x - expectedCG.x);

    const double expectedLongUnitMOI =
        (foreCapLongMOI + foreShoulderLongMOI + coneLongMOI + aftShoulderLongMOI + aftCapLongMOI) /
        expectedCG.weight;
    EXPECT_NEAR(expectedLongUnitMOI, nc.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal Unit MOI is incorrect";

    const double foreCapRotMOI =
        calculateShoulderRotationalMOI(capLength, foreCapRadius, foreCapSolid);
    const double foreShoulderRotMOI = calculateShoulderRotationalMOI(length, foreRadius, thickness);
    const double coneRotMOI =
        calculateConicalTransitionRotationalMOI(length, foreRadius, aftRadius, thickness);
    const double aftShoulderRotMOI = calculateShoulderRotationalMOI(length, aftRadius, thickness);
    const double aftCapRotMOI =
        calculateShoulderRotationalMOI(capLength, aftCapRadius, aftCapSolid);

    const double expectedRotUnitMOI =
        (foreCapRotMOI + foreShoulderRotMOI + coneRotMOI + aftShoulderRotMOI + aftCapRotMOI) /
        expectedCG.weight;

    EXPECT_NEAR(expectedRotUnitMOI, nc.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit MOI is incorrect";

    checkConeSurface(length, foreRadius, aftRadius, nc);
}

TEST(SymmetricComponentVolume, TransitionVsTubeFilled)
{
    // BodyTubes use closed form solutions for mass properties, while Transitions use numerical
    // integration from SymmetricComponent. Properties should agree.
    const double radius = 1.0;
    const double length = 10.0;

    const BodyTube bt1(length, radius, true);

    Transition trans1;
    trans1.setFilled(true);
    trans1.setLength(length);
    trans1.setForeRadius(radius, true);
    trans1.setAftRadius(radius, true);
    trans1.setShapeType(TransitionShape::CONICAL);

    EXPECT_NEAR(bt1.getLength(), trans1.getLength(), kEpsilon) << "Length is incorrect";
    EXPECT_NEAR(bt1.getRadius(0), trans1.getRadius(0), kEpsilon) << "Forward radius is incorrect";
    EXPECT_NEAR(bt1.getRadius(bt1.getLength()), trans1.getRadius(trans1.getLength()), kEpsilon)
        << "Aft radius is incorrect";

    EXPECT_NEAR(bt1.getComponentVolume(), trans1.getComponentVolume(), kEpsilon)
        << "Volume is incorrect";
    EXPECT_NEAR(bt1.getComponentCG().x, trans1.getComponentCG().x, kEpsilon) << "CG is incorrect";

    EXPECT_NEAR(bt1.getLongitudinalUnitInertia(), trans1.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal moment of inertia is incorrect";
    EXPECT_NEAR(bt1.getRotationalUnitInertia(), trans1.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit moment of inertia is incorrect";

    EXPECT_NEAR(bt1.getComponentWetArea(), trans1.getComponentWetArea(), kEpsilon)
        << "Wetted area is incorrect";
    EXPECT_NEAR(bt1.getComponentPlanformArea(), trans1.getComponentPlanformArea(), kEpsilon)
        << "Planform area is incorrect";
    EXPECT_NEAR(bt1.getComponentPlanformCenter(), trans1.getComponentPlanformCenter(), kEpsilon)
        << "Planform centroid is incorrect";
}

TEST(SymmetricComponentVolume, TransitionVsTubeHollow)
{
    const double radius      = 1.0;
    const double innerRadius = 0.1;
    const double length      = 10.0;

    BodyTube bt2(length, radius, false);
    bt2.setInnerRadius(innerRadius);

    Transition trans2;
    trans2.setFilled(false);
    trans2.setLength(length);
    trans2.setForeRadius(radius, true);
    trans2.setAftRadius(radius, true);
    trans2.setShapeType(TransitionShape::CONICAL);
    trans2.setThickness(radius - innerRadius, true);

    EXPECT_NEAR(bt2.getComponentVolume(), trans2.getComponentVolume(), kEpsilon)
        << "Volume is incorrect";
    EXPECT_NEAR(bt2.getComponentCG().x, trans2.getComponentCG().x, kEpsilon) << "CG is incorrect";
    EXPECT_NEAR(bt2.getLongitudinalUnitInertia(), trans2.getLongitudinalUnitInertia(), kEpsilon)
        << "Longitudinal unit moment of inertia is incorrect";
    EXPECT_NEAR(bt2.getRotationalUnitInertia(), trans2.getRotationalUnitInertia(), kEpsilon)
        << "Rotational unit moment of inertia is incorrect";
    EXPECT_NEAR(bt2.getComponentWetArea(), trans2.getComponentWetArea(), kEpsilon)
        << "Wetted area is incorrect";

    EXPECT_NEAR(bt2.getComponentPlanformArea(), trans2.getComponentPlanformArea(), kEpsilon)
        << "Planform area is incorrect";
    EXPECT_NEAR(bt2.getComponentPlanformCenter(), trans2.getComponentPlanformCenter(), kEpsilon)
        << "Planform centroid is incorrect";
}

// ====================================================================== SymmetricComponentTest

// SymmetricComponentTest runs on TestRockets.makeFalcon9Heavy(): the shared TestFalcon9Heavy
// (TestRockets.h).

TEST(SymmetricComponent, PreviousSymmetricComponent)
{
    const TestFalcon9Heavy f9h;

    EXPECT_EQ(f9h.payloadNose->getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(f9h.payloadBody->getPreviousSymmetricComponent(), f9h.payloadNose);
    EXPECT_EQ(f9h.payloadTransition->getPreviousSymmetricComponent(), f9h.payloadBody);
    EXPECT_EQ(f9h.upperStageBody->getPreviousSymmetricComponent(), f9h.payloadTransition);
    EXPECT_EQ(f9h.interstage->getPreviousSymmetricComponent(), f9h.upperStageBody);

    EXPECT_EQ(f9h.coreBody->getPreviousSymmetricComponent(), f9h.interstage);

    EXPECT_EQ(f9h.boosterNose->getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(f9h.boosterBody->getPreviousSymmetricComponent(), f9h.boosterNose);
}

TEST(SymmetricComponent, NextSymmetricComponent)
{
    const TestFalcon9Heavy f9h;

    EXPECT_EQ(f9h.payloadNose->getNextSymmetricComponent(), f9h.payloadBody);
    EXPECT_EQ(f9h.payloadBody->getNextSymmetricComponent(), f9h.payloadTransition);
    EXPECT_EQ(f9h.payloadTransition->getNextSymmetricComponent(), f9h.upperStageBody);
    EXPECT_EQ(f9h.upperStageBody->getNextSymmetricComponent(), f9h.interstage);

    EXPECT_EQ(f9h.interstage->getNextSymmetricComponent(), f9h.coreBody);
    EXPECT_EQ(f9h.coreBody->getNextSymmetricComponent(), nullptr);

    EXPECT_EQ(f9h.boosterNose->getNextSymmetricComponent(), f9h.boosterBody);
    EXPECT_EQ(f9h.boosterBody->getNextSymmetricComponent(), nullptr);
}

TEST(SymmetricComponent, NeighboursThroughConstAndMutableAccessAgree)
{
    const TestFalcon9Heavy    f9h;
    const SymmetricComponent& constTail = *f9h.payloadTransition;
    EXPECT_EQ(f9h.payloadTransition->getPreviousSymmetricComponent(),
              constTail.getPreviousSymmetricComponent());
    EXPECT_EQ(f9h.payloadTransition->getNextSymmetricComponent(),
              constTail.getNextSymmetricComponent());
}

/// The rocket of the two InlineComponentAssembly tests: the Falcon 9 Heavy with the booster set
/// reduced to one booster off the axis, a last stage (a body tube 0.2 m, radius 0.05 m), and a
/// pod set of one pod on the core body's axis (FREE radius 0) holding a nose cone (0.1 m, base
/// radius 0.05 m) and a body (0.2 m, radius 0.05 m, with fins), in the order @p coneFirst gives.
struct InlineAssemblyRocket : TestFalcon9Heavy
{
    PodSet*   podSet{nullptr};
    NoseCone* podSetCone{nullptr};
    BodyTube* podSetBody{nullptr};
    BodyTube* lastStageBody{nullptr};

    explicit InlineAssemblyRocket(bool coneFirst)
    {
        boosterStage->setInstanceCount(1);
        boosterStage->setRadius(RadiusMethod::RELATIVE, 0);

        // Add inline pod set
        podSet = &coreBody->addChild(std::make_unique<PodSet>());
        podSet->setName("Inline Pod Set");
        podSet->setInstanceCount(1);
        podSet->setRadius(RadiusMethod::FREE, 0);
        setAxialOffset(*podSet, coneFirst ? AxialMethod::BOTTOM : AxialMethod::TOP, 0);

        auto cone = std::make_unique<NoseCone>();
        cone->setLength(0.1);
        cone->setBaseRadius(0.05);
        auto body = std::make_unique<BodyTube>(0.2, 0.05, 0.001);
        body->setName("Pod Set Body");
        // HOOK(fins-lugs): tier 6b replaces this double with the real class (TrapezoidFinSet()).
        body->addChild(
            TestComponent::make(0.05, ComponentKind::TRAPEZOID_FIN_SET, AxialMethod::BOTTOM));
        if (coneFirst)
        {
            podSetCone = &podSet->addChild(std::move(cone));
            podSetBody = &podSet->addChild(std::move(body));
        }
        else
        {
            podSetBody = &podSet->addChild(std::move(body));
            cone->setFlipped(true);
            podSetCone = &podSet->addChild(std::move(cone));
        }

        // Add last stage
        auto lastStage = std::make_unique<AxialStage>();
        auto lastBody  = std::make_unique<BodyTube>(0.2, 0.05, 0.001);
        lastBody->setName("Last Stage Body");
        lastStageBody = &lastStage->addChild(std::move(lastBody));
        rocket->addChild(std::move(lastStage));
    }
};

TEST(SymmetricComponent, PreviousSymmetricComponentInlineComponentAssembly)
{
    const InlineAssemblyRocket r(true);
    BodyTube* const            coreBody      = r.coreBody;
    BodyTube* const            interstage    = r.interstage;
    NoseCone* const            podSetCone    = r.podSetCone;
    BodyTube* const            podSetBody    = r.podSetBody;
    BodyTube* const            lastStageBody = r.lastStageBody;
    PodSet* const              podSet        = r.podSet;

    EXPECT_EQ(r.payloadNose->getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(r.payloadBody->getPreviousSymmetricComponent(), r.payloadNose);
    EXPECT_EQ(r.payloadTransition->getPreviousSymmetricComponent(), r.payloadBody);
    EXPECT_EQ(r.upperStageBody->getPreviousSymmetricComponent(), r.payloadTransition);
    EXPECT_EQ(interstage->getPreviousSymmetricComponent(), r.upperStageBody);

    EXPECT_EQ(r.boosterNose->getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(r.boosterBody->getPreviousSymmetricComponent(), r.boosterNose);

    // case 1: pod set is larger, and at the back of the core stage
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 2: pod set is smaller, and at the back of the core stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 3: pod set is equal, and at the back of the core stage
    podSetBody->setOuterRadius(0.0385);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 4: pod set is larger, and at the front of the core stage
    podSetBody->setOuterRadius(0.05);
    setAxialOffset(*podSet, AxialMethod::TOP, 0);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 5: pod set is smaller, and at the front of the core stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 6: pod set is equal, and at the front of the core stage
    podSetBody->setOuterRadius(0.0385);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 7: pod set is same length as core stage, and larger, and at the front of the core
    // stage
    podSetBody->setOuterRadius(0.05);
    podSetBody->setLength(0.7);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 8: pod set is same length as core stage, and smaller, and at the front of the core
    // stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 9: pod set is in larger, and in the middle of the core stage
    podSetBody->setLength(0.2);
    podSetBody->setOuterRadius(0.05);
    setAxialOffset(*podSet, AxialMethod::MIDDLE, 0);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // case 10: pod set is in larger, and behind the back of the core stage
    setAxialOffset(*podSet, AxialMethod::BOTTOM, 1);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);

    // Add a booster inside the pod set
    auto& insideBooster = podSetBody->addChild(std::make_unique<ParallelStage>());
    insideBooster.setName("Inside Booster");
    insideBooster.setInstanceCount(1);
    insideBooster.setRadius(RadiusMethod::FREE, 0);
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, 0);
    auto& insideBoosterBody = insideBooster.addChild(std::make_unique<BodyTube>(0.2, 0.06, 0.001));
    insideBoosterBody.setName("Inside Booster Body");

    // Case 1: inside booster is larger than pod set and flush to its end (both are at the back
    // of the core stage)
    setAxialOffset(*podSet, AxialMethod::BOTTOM, 0);
    insideBoosterBody.setOuterRadius(0.06);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 2: inside booster is smaller than pod set and flush to its end
    insideBoosterBody.setOuterRadius(0.04);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 3: inside booster is equal the pod set and flush to its end
    insideBoosterBody.setOuterRadius(0.05);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 4: inside booster is larger than pod set and before the back (pod set at the back of
    // the core stage)
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, -1);
    insideBoosterBody.setOuterRadius(0.06);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 5: inside booster is larger than pod set and after the back (pod set at the back of
    // the core stage)
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, 1);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetBody);

    // Case 6: inside booster is larger than pod set, pod set is before the back of the core
    // stage, inside booster is an equal amount after the back of the pod set
    setAxialOffset(*podSet, AxialMethod::BOTTOM, -1.5);
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, 1.5);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetBody);

    // Case 7: inside booster is larger than pod set, pod set is after the back of the core
    // stage, inside booster is an equal amount before the back of the pod set
    setAxialOffset(*podSet, AxialMethod::BOTTOM, 1.5);
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, -1.5);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 8: inside booster is larger than pod set, pod set is before the back of the core
    // stage, inside booster is flush with the back of the pod set
    setAxialOffset(*podSet, AxialMethod::BOTTOM, -1.5);
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, 0);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);

    // Case 9: inside booster is larger than pod set, pod set is after the back of the core
    // stage, inside booster is flush with the back of the pod set
    setAxialOffset(*podSet, AxialMethod::BOTTOM, 1.5);
    setAxialOffset(insideBooster, AxialMethod::BOTTOM, 0);
    EXPECT_EQ(lastStageBody->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetCone->getPreviousSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getPreviousSymmetricComponent(), podSetCone);
    EXPECT_EQ(coreBody->getPreviousSymmetricComponent(), interstage);
    EXPECT_EQ(insideBoosterBody.getPreviousSymmetricComponent(), podSetCone);
}

TEST(SymmetricComponent, NextSymmetricComponentInlineComponentAssembly)
{
    const InlineAssemblyRocket r(false);
    BodyTube* const            coreBody      = r.coreBody;
    BodyTube* const            interstage    = r.interstage;
    NoseCone* const            podSetCone    = r.podSetCone;
    BodyTube* const            podSetBody    = r.podSetBody;
    BodyTube* const            lastStageBody = r.lastStageBody;
    PodSet* const              podSet        = r.podSet;

    EXPECT_EQ(r.payloadNose->getNextSymmetricComponent(), r.payloadBody);
    EXPECT_EQ(r.payloadBody->getNextSymmetricComponent(), r.payloadTransition);
    EXPECT_EQ(r.payloadTransition->getNextSymmetricComponent(), r.upperStageBody);
    EXPECT_EQ(r.upperStageBody->getNextSymmetricComponent(), interstage);

    EXPECT_EQ(lastStageBody->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(r.boosterNose->getNextSymmetricComponent(), r.boosterBody);

    // case 1: pod set is larger, and at the front of the core stage
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 2: pod set is smaller, and at the front of the core stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 3: pod set is equal, and at the front of the core stage
    podSetBody->setOuterRadius(0.0385);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 4: pod set is larger, and at the back of the core stage
    podSetBody->setOuterRadius(0.05);
    setAxialOffset(*podSet, AxialMethod::BOTTOM, 0);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 5: pod set is smaller, and at the back of the core stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 6: pod set is equal, and at the back of the core stage
    podSetBody->setOuterRadius(0.0385);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 7: pod set is same length as core stage, and larger, and at the back of the core
    // stage
    podSetBody->setOuterRadius(0.05);
    podSetBody->setLength(0.7);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 8: pod set is same length as core stage, and smaller, and at the back of the core
    // stage
    podSetBody->setOuterRadius(0.02);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 9: pod set is in larger, and in the middle of the core stage
    podSetBody->setLength(0.2);
    podSetBody->setOuterRadius(0.05);
    setAxialOffset(*podSet, AxialMethod::MIDDLE, 0);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // case 10: pod set is in larger, and behind the front of the core stage
    setAxialOffset(*podSet, AxialMethod::TOP, 1);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);

    // Add a booster inside the pod set
    auto& insideBooster = podSetBody->addChild(std::make_unique<ParallelStage>());
    insideBooster.setName("Inside Booster");
    insideBooster.setInstanceCount(1);
    insideBooster.setRadius(RadiusMethod::FREE, 0);
    setAxialOffset(insideBooster, AxialMethod::TOP, 0);
    auto& insideBoosterBody = insideBooster.addChild(std::make_unique<BodyTube>(0.2, 0.06, 0.001));
    insideBoosterBody.setName("Inside Booster Body");

    // Case 1: inside booster is larger than pod set and flush to its front (both are at the
    // front of the core stage)
    setAxialOffset(*podSet, AxialMethod::TOP, 0);
    insideBoosterBody.setOuterRadius(0.06);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 2: inside booster is smaller than pod set and flush to its front
    insideBoosterBody.setOuterRadius(0.04);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 3: inside booster is equal the pod set and flush to its front
    insideBoosterBody.setOuterRadius(0.05);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 4: inside booster is larger than pod set and before the front (pod set at the front
    // of the core stage)
    setAxialOffset(insideBooster, AxialMethod::TOP, -1);
    insideBoosterBody.setOuterRadius(0.06);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 5: inside booster is larger than pod set and after the front (pod set at the front of
    // the core stage)
    setAxialOffset(insideBooster, AxialMethod::TOP, 1);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), podSetBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 6: inside booster is larger than pod set, pod set is before the front of the core
    // stage, inside booster is an equal amount after the front of the pod set
    setAxialOffset(*podSet, AxialMethod::TOP, -1.5);
    setAxialOffset(insideBooster, AxialMethod::TOP, 1.5);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 7: inside booster is larger than pod set, pod set is after the front of the core
    // stage, inside booster is an equal amount before the front of the pod set
    setAxialOffset(*podSet, AxialMethod::TOP, 1.5);
    setAxialOffset(insideBooster, AxialMethod::TOP, -1.5);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), &insideBoosterBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 8: inside booster is larger than pod set, pod set is before the front of the core
    // stage, inside booster is flush with the front of the pod set
    setAxialOffset(*podSet, AxialMethod::TOP, -1.5);
    setAxialOffset(insideBooster, AxialMethod::TOP, 0);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);

    // Case 9: inside booster is larger than pod set, pod set is after the front of the core
    // stage, inside booster is flush with the front of the pod set
    setAxialOffset(*podSet, AxialMethod::TOP, 1.5);
    setAxialOffset(insideBooster, AxialMethod::TOP, 0);
    EXPECT_EQ(interstage->getNextSymmetricComponent(), coreBody);
    EXPECT_EQ(podSetBody->getNextSymmetricComponent(), podSetCone);
    EXPECT_EQ(podSetCone->getNextSymmetricComponent(), nullptr);
    EXPECT_EQ(coreBody->getNextSymmetricComponent(), lastStageBody);
    EXPECT_EQ(insideBoosterBody.getNextSymmetricComponent(), nullptr);
}

// ================================================================ behaviour beyond the JUnit

TEST(SymmetricComponent, NoNeighboursWithoutAGrandparent)
{
    const Transition detached;
    EXPECT_EQ(detached.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(detached.getNextSymmetricComponent(), nullptr);

    AxialStage  stage;
    const auto& tube = stage.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    EXPECT_EQ(tube.getNextSymmetricComponent(), nullptr) << "the stage has no parent";
}

TEST(SymmetricComponent, BodyComponentDefaults)
{
    const Transition transition;
    EXPECT_EQ(transition.getAxialMethod(), AxialMethod::AFTER);
    EXPECT_TRUE(transition.isAfter());
    EXPECT_TRUE(transition.allowsChildren());
    EXPECT_TRUE(transition.isAerodynamic());
    EXPECT_TRUE(transition.isMassive());
    EXPECT_FALSE(transition.isFilled());
    EXPECT_EQ(transition.getThickness(), SymmetricComponent::kDefaultThickness);
    // BodyComponent.getInnerRadius() reads a field OpenRocket never sets.
    EXPECT_EQ(transition.getInnerRadius(), 0.0);
    EXPECT_EQ(transition.getRadius(0.01, 1.0), transition.getRadius(0.01));
    EXPECT_EQ(transition.getInnerRadius(0.01, 1.0), transition.getInnerRadius(0.01));
    EXPECT_EQ(transition.getOuterRadius(0.01), transition.getRadius(0.01));
}

TEST(SymmetricComponent, SetLengthClampsNegativeLengthsToZero)
{
    BodyTube tube(0.3, 0.02);
    tube.setLength(-1.0);
    EXPECT_EQ(tube.getLength(), 0.0);
    tube.setLength(0.25);
    EXPECT_EQ(tube.getLength(), 0.25);
    tube.setLength(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(tube.getLength())) << "Java's Math.max keeps a NaN";
}

TEST(SymmetricComponent, ThicknessClampingAndFilling)
{
    Transition transition;
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.03);

    transition.setThickness(0.5);
    EXPECT_EQ(transition.getThickness(), 0.03) << "clamped to the larger radius";
    transition.setThickness(-0.1);
    EXPECT_EQ(transition.getThickness(), 0.0) << "clamped to zero";
    transition.setThickness(0.5, false);
    EXPECT_EQ(transition.getThickness(), 0.5) << "not clamped";

    transition.setFilled(true);
    EXPECT_TRUE(transition.isFilled());
    EXPECT_EQ(transition.getThickness(), 0.03) << "a filled component is as thick as it is wide";

    // Setting the same thickness on a filled component makes it hollow again.
    transition.setThickness(0.5, false);
    EXPECT_FALSE(transition.isFilled());
    EXPECT_EQ(transition.getThickness(), 0.5);
}

TEST(SymmetricComponent, MaxRadiusAndInstanceBoundingBox)
{
    Transition transition;
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.03);
    EXPECT_EQ(transition.getMaxRadius(), 0.03);

    const BoundingBox box = transition.getInstanceBoundingBox();
    EXPECT_EQ(box.min(), Coordinate(0, -0.03, -0.03));
    EXPECT_EQ(box.max(), Coordinate(transition.getLength(), 0.03, 0.03));
}

TEST(SymmetricComponent, ComponentBoundsSampleTheProfileAtSixStations)
{
    Transition transition;
    transition.setForeRadius(0.0);
    transition.setAftRadius(0.05);
    transition.setLength(0.5);

    std::vector<Coordinate> expected;
    for (int n = 0; n <= 5; n++)
    {
        const double x = n * 0.5 / 5;
        const double r = transition.getRadius(x);
        expected.emplace_back(x, -r, -r);
        expected.emplace_back(x, r, -r);
        expected.emplace_back(x, r, r);
        expected.emplace_back(x, -r, r);
    }
    EXPECT_EQ(transition.getComponentBounds(), expected);
}

TEST(SymmetricComponent, AZeroLengthComponentHasNoProperties)
{
    Transition transition;
    transition.setLength(0);
    EXPECT_EQ(transition.getComponentVolume(), 0.0);
    EXPECT_EQ(transition.getFullVolume(), 0.0);
    EXPECT_EQ(transition.getComponentWetArea(), 0.0);
    EXPECT_EQ(transition.getComponentPlanformArea(), 0.0);
    EXPECT_EQ(transition.getComponentPlanformCenter(), 0.0);
    EXPECT_EQ(transition.getComponentCG(), Coordinate());
    EXPECT_EQ(transition.getLongitudinalUnitInertia(), 0.0);
    EXPECT_EQ(transition.getRotationalUnitInertia(), 0.0);
}

TEST(SymmetricComponent, AComponentWithoutVolumeHasItsCGAtHalfItsLength)
{
    Transition transition;
    transition.setForeRadius(0);
    transition.setAftRadius(0);
    transition.setLength(0.4);
    EXPECT_EQ(transition.getComponentVolume(), 0.0);
    EXPECT_EQ(transition.getComponentCG(), Coordinate(0.2, 0, 0, 0));
    EXPECT_EQ(transition.getLongitudinalUnitInertia(), 0.0);
    EXPECT_EQ(transition.getRotationalUnitInertia(), 0.0);
}

TEST(SymmetricComponent, FullVolumeIsTheFilledVolume)
{
    Transition transition;
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.04);
    transition.setLength(0.1);
    transition.setThickness(0.002);

    Transition filled;
    filled.setForeRadius(0.02);
    filled.setAftRadius(0.04);
    filled.setLength(0.1);
    filled.setFilled(true);

    EXPECT_NEAR(transition.getFullVolume(), filled.getComponentVolume(), 1e-15);
    EXPECT_LT(transition.getComponentVolume(), transition.getFullVolume());
    // The conical frustum is integrated exactly.
    const double expected =
        std::numbers::pi * 0.1 * (pow2(0.02) + (0.02 * 0.04) + pow2(0.04)) / 3.0;
    EXPECT_NEAR(transition.getFullVolume(), expected, expected * 1e-12);
}

TEST(SymmetricComponent, CachedValuesAreClearedByMassAndAerodynamicChanges)
{
    Rocket rocket;
    auto&  stage      = rocket.addChild(std::make_unique<AxialStage>());
    auto&  transition = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.04);
    transition.setLength(0.1);

    Transition longer;
    longer.setForeRadius(0.02);
    longer.setAftRadius(0.04);
    longer.setLength(0.2);

    const double volume = transition.getComponentVolume();
    transition.setLength(0.2);  // AEROMASS_CHANGE
    EXPECT_NE(transition.getComponentVolume(), volume);
    EXPECT_EQ(transition.getComponentVolume(), longer.getComponentVolume());
    EXPECT_EQ(transition.getComponentWetArea(), longer.getComponentWetArea());
    EXPECT_EQ(transition.getComponentPlanformArea(), longer.getComponentPlanformArea());
    EXPECT_EQ(transition.getComponentPlanformCenter(), longer.getComponentPlanformCenter());
    EXPECT_EQ(transition.getFullVolume(), longer.getFullVolume());
    EXPECT_EQ(transition.getComponentCG(), longer.getComponentCG());
    EXPECT_EQ(transition.getLongitudinalUnitInertia(), longer.getLongitudinalUnitInertia());
    EXPECT_EQ(transition.getRotationalUnitInertia(), longer.getRotationalUnitInertia());

    const double massBefore = transition.getComponentCG().weight;
    transition.setMaterial(testMaterial(2 * transition.getMaterial().getDensity()));  // MASS_CHANGE
    EXPECT_EQ(transition.getComponentCG().weight, 2 * massBefore);
}

TEST(SymmetricComponent, ADetachedComponentKeepsItsCacheAsInOpenRocket)
{
    // Without a Rocket at the root no change event is delivered, so the integrated values are
    // not recomputed after they have been read once (OpenRocket behaves the same).
    Transition transition;
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.04);
    transition.setLength(0.1);
    const double volume = transition.getComponentVolume();
    transition.setLength(0.2);
    EXPECT_EQ(transition.getComponentVolume(), volume);
}

TEST(SymmetricComponent, AnOffAxisPodIsNotInLine)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.05));
    auto&  pods  = body.addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(1);
    pods.setRadius(RadiusMethod::FREE, 0.1);
    setAxialOffset(pods, AxialMethod::BOTTOM, 0);
    auto& podBody = pods.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    auto& tail    = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();

    EXPECT_EQ(podBody.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(tail.getPreviousSymmetricComponent(), &body);

    // On the axis, the pod is in line: its symmetric grandparent starts ahead of it.
    pods.setRadius(RadiusMethod::FREE, 0);
    EXPECT_EQ(podBody.getPreviousSymmetricComponent(), &body);
}

}  // namespace
