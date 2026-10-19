#include "QtRocket/figure/MassComponentShapes.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::FigurePath;
using QtRocket::FigureRoundRect;
using QtRocket::FigureView;
using QtRocket::MassComponent;
using QtRocket::Parachute;
using QtRocket::PathSegment;
using QtRocket::Point2D;
using QtRocket::Rectangle2D;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltAsJava;
using QtRocket::Test::handBuiltPin;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::shapesMatchPins;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace MassComponentShapes = QtRocket::MassComponentShapes;
using Type                    = MassComponent::MassComponentType;

/// The probe's mass(type, length, radius).
[[nodiscard]] MassComponent mass(Type type, double length, double radius)
{
    MassComponent m;
    m.setMassComponentType(type);
    m.setLength(length);
    m.setRadius(radius);
    return m;
}

/// The type's name as the probe writes it (MassComponentType.name()).
[[nodiscard]] std::string_view typeName(Type type)
{
    switch (type)
    {
        case Type::MASSCOMPONENT:
            return "MASSCOMPONENT";
        case Type::ALTIMETER:
            return "ALTIMETER";
        case Type::FLIGHTCOMPUTER:
            return "FLIGHTCOMPUTER";
        case Type::DEPLOYMENTCHARGE:
            return "DEPLOYMENTCHARGE";
        case Type::TRACKER:
            return "TRACKER";
        case Type::PAYLOAD:
            return "PAYLOAD";
        case Type::RECOVERYHARDWARE:
            return "RECOVERYHARDWARE";
        case Type::BATTERY:
            return "BATTERY";
    }
    return "?";
}

/// Success when the hand-built case @p name of @p massComponent is as Java's (the tracker's arcs
/// and an off-axis position go through sin and cos).
[[nodiscard]] ::testing::AssertionResult massAsJava(const MassComponent& massComponent,
                                                    const std::string&   name)
{
    return handBuiltVariantsAsJava(&MassComponentShapes::getShapesSide,
                                   &MassComponentShapes::getShapesBack, massComponent, kHandBuilt,
                                   name, kTranscendentalTolerance);
}

TEST(MassComponentShapes, EveryTypeAsJava)
{
    for (const Type type : MassComponent::kAllMassComponentTypes)
    {
        if (type == Type::DEPLOYMENTCHARGE)
        {
            continue;  // its rays are random in Java: DeploymentChargeAsJava
        }
        EXPECT_TRUE(massAsJava(mass(type, 0.06, 0.012), "mass-" + std::string(typeName(type))));
    }
}

TEST(MassComponentShapes, ShortLongAndOffAxisAsJava)
{
    // Short and wide: the arcs are 0.7 of the length, and the symbols take their other branches.
    for (const Type type : {Type::ALTIMETER, Type::FLIGHTCOMPUTER, Type::TRACKER,
                            Type::RECOVERYHARDWARE, Type::BATTERY, Type::PAYLOAD})
    {
        EXPECT_TRUE(
            massAsJava(mass(type, 0.01, 0.02), "mass-short-" + std::string(typeName(type))));
    }
    EXPECT_TRUE(massAsJava(mass(Type::TRACKER, 0.1, 0.005), "mass-long-TRACKER"));
    EXPECT_TRUE(massAsJava(mass(Type::BATTERY, 0.1, 0.005), "mass-long-BATTERY"));

    MassComponent offAxis = mass(Type::PAYLOAD, 0.03, 0.008);
    offAxis.setRadialPosition(0.01);
    offAxis.setRadialDirection(1.0);
    EXPECT_TRUE(massAsJava(offAxis, "mass-off-axis"));
}

/// Whether @p v lies in [@p from, @p from + @p length).
[[nodiscard]] bool inRay(double v, double from, double length) noexcept
{
    return v >= from && v < from + length;
}

/// Success when @p rays is the deployment charge's explosion: kDeploymentChargeRays rays from the
/// centre of @p bounds, each ending up to a quarter of its smaller side right of and above it.
[[nodiscard]] ::testing::AssertionResult raysOfDeploymentCharge(const FigurePath&  rays,
                                                                const Rectangle2D& bounds)
{
    const double xCenter      = bounds.x + (bounds.width / 2.0);
    const double yCenter      = bounds.y + (bounds.height / 2.0);
    const double maxRayLength = QtRocket::MathUtil::javaMin(bounds.width, bounds.height) / 4.0;
    const std::vector<PathSegment>& segments = rays.segments();
    if (segments.size() != 2 * static_cast<std::size_t>(MassComponentShapes::kDeploymentChargeRays))
    {
        return ::testing::AssertionFailure() << segments.size() << " segments";
    }
    for (std::size_t i = 0; i < segments.size(); i += 2)
    {
        const Point2D from    = segments[i].points[0];
        const Point2D to      = segments[i + 1].points[0];
        const bool    shaped  = segments[i].type == PathSegment::Type::MOVE_TO &&
                                segments[i + 1].type == PathSegment::Type::LINE_TO;
        const bool    centred = from.x == xCenter && from.y == yCenter;
        if (!shaped || !centred || !inRay(to.x, xCenter, maxRayLength) ||
            !inRay(to.y, yCenter, maxRayLength))
        {
            return ::testing::AssertionFailure() << "ray " << i / 2 << " is not one";
        }
    }
    return ::testing::AssertionSuccess();
}

/// Success when the deployment charge's side view for the hand-built case @p name is Java's
/// rounded rectangle and arc followed by the rays.
[[nodiscard]] ::testing::AssertionResult deploymentChargeSideAsJava(const MassComponent& charge,
                                                                    const char*          name)
{
    const auto& pin = handBuiltPin(kHandBuilt, name, FigureView::SIDE);
    const auto  shapes =
        MassComponentShapes::getShapesSide(charge, pinnedTransformation(pin.transform));
    if (shapes.size() != 3)
    {
        return ::testing::AssertionFailure() << shapes.size() << " shapes";
    }
    const ::testing::AssertionResult pinned =
        shapesMatchPins(std::span{shapes}.first(2), pin.shapes, charge);
    if (!pinned)
    {
        return pinned;
    }
    const auto* rays = shapes[2].shape().getIf<FigurePath>();
    const auto* base = shapes[0].shape().getIf<FigureRoundRect>();
    if (rays == nullptr || base == nullptr)
    {
        return ::testing::AssertionFailure() << "no rays";
    }
    return raysOfDeploymentCharge(*rays, base->bounds());
}

TEST(MassComponentShapes, DeploymentChargeAsJava)
{
    // Java draws the rays from an unseeded java.util.Random: the probe's pins hold the rounded
    // rectangle and the arc only; the rays are checked for their number and their range.
    const MassComponent charge = mass(Type::DEPLOYMENTCHARGE, 0.06, 0.012);
    EXPECT_TRUE(deploymentChargeSideAsJava(charge, "mass-DEPLOYMENTCHARGE/axial"));
    EXPECT_TRUE(deploymentChargeSideAsJava(charge, "mass-DEPLOYMENTCHARGE/off"));
    EXPECT_TRUE(handBuiltAsJava(&MassComponentShapes::getShapesSide,
                                &MassComponentShapes::getShapesBack, charge, kHandBuilt,
                                "mass-DEPLOYMENTCHARGE/axial", FigureView::BACK));
    EXPECT_TRUE(handBuiltAsJava(&MassComponentShapes::getShapesSide,
                                &MassComponentShapes::getShapesBack, charge, kHandBuilt,
                                "mass-DEPLOYMENTCHARGE/off", FigureView::BACK));
}

TEST(MassComponentShapes, TheDeploymentChargesRaysFollowTheComponentsId)
{
    // Deviation (U4): seeded by the component's id, the rays are the same on every call (Java
    // draws new ones on every paint) and differ between components.
    MassComponent charge = mass(Type::DEPLOYMENTCHARGE, 0.06, 0.012);
    MassComponent other  = mass(Type::DEPLOYMENTCHARGE, 0.06, 0.012);
    const auto    t      = Transformation::translation(0.1, 0, 0);
    const auto    first  = MassComponentShapes::getShapesSide(charge, t);
    const auto    again  = MassComponentShapes::getShapesSide(charge, t);
    const auto    others = MassComponentShapes::getShapesSide(other, t);
    ASSERT_EQ(first.size(), 3U);
    ASSERT_EQ(again.size(), 3U);
    ASSERT_EQ(others.size(), 3U);
    EXPECT_EQ(first[2].shape(), again[2].shape());
    EXPECT_NE(first[2].shape(), others[2].shape());

    // The same id gives the same rays, whatever the component.
    other.setId(charge.getId());
    EXPECT_EQ(MassComponentShapes::getShapesSide(other, t)[2].shape(), first[2].shape());
}

TEST(MassComponentShapes, TheBackViewTakesAnyMassObject)
{
    // Java's getShapesBack casts to MassObject, its getShapesSide to MassComponent.
    Parachute chute;
    chute.setRadius(0.01);
    EXPECT_EQ(MassComponentShapes::getShapesBack(chute, Transformation::kIdentity).size(), 1U);
    EXPECT_THROW((void)MassComponentShapes::getShapesSide(chute, Transformation::kIdentity),
                 BugError);
}

TEST(MassComponentShapes, AnotherComponentIsABug)
{
    const BodyTube tube;
    EXPECT_THROW((void)MassComponentShapes::getShapesSide(tube, Transformation::kIdentity),
                 BugError);
    EXPECT_THROW((void)MassComponentShapes::getShapesBack(tube, Transformation::kIdentity),
                 BugError);
}

}  // namespace
