#include "QtRocket/figure/ComponentAssemblyShapes.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/EmptyShapes.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentShape;
using QtRocket::FigureShape;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace ComponentAssemblyShapes = QtRocket::ComponentAssemblyShapes;

/// The probe's bodyOfNewRocket(): a rocket, a stage and a body tube 0.3 long of radius 0.02.
struct RocketWithBody
{
    std::unique_ptr<Rocket> rocket;
    BodyTube*               body;
};

[[nodiscard]] RocketWithBody bodyOfNewRocket()
{
    auto  rocket = std::make_unique<Rocket>();
    auto& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto  body   = std::make_unique<BodyTube>();
    body->setLength(0.3);
    body->setOuterRadius(0.02);
    BodyTube& added = stage.addChild(std::move(body));
    return RocketWithBody{.rocket = std::move(rocket), .body = &added};
}

/// A body tube of @p length and @p radius added to @p parent.
void addBody(RocketComponent& parent, double length, double radius)
{
    auto body = std::make_unique<BodyTube>();
    body->setLength(length);
    body->setOuterRadius(radius);
    parent.addChild(std::move(body));
}

/// The name the probe gives @p method (AxialMethod.name()).
[[nodiscard]] std::string axialName(AxialMethod method)
{
    switch (method)
    {
        case AxialMethod::TOP:
            return "TOP";
        case AxialMethod::BOTTOM:
            return "BOTTOM";
        case AxialMethod::MIDDLE:
            return "MIDDLE";
        case AxialMethod::ABSOLUTE:
            return "ABSOLUTE";
        case AxialMethod::AFTER:
            return "AFTER";
    }
    return "?";
}

/// Success when the hand-built case @p name of @p assembly is as Java's.
[[nodiscard]] ::testing::AssertionResult assemblyAsJava(const RocketComponent& assembly,
                                                        const std::string&     name)
{
    return handBuiltVariantsAsJava(&ComponentAssemblyShapes::getShapesSide,
                                   &ComponentAssemblyShapes::getShapesBack, assembly, kHandBuilt,
                                   name);
}

constexpr std::array<AxialMethod, 4> kAxialMethods{AxialMethod::TOP, AxialMethod::BOTTOM,
                                                   AxialMethod::MIDDLE, AxialMethod::ABSOLUTE};

TEST(ComponentAssemblyShapes, PodSetsAsJava)
{
    // The marker moves aft by the length for BOTTOM, by half of it for MIDDLE (side view only),
    // and down by the bounding radius for RELATIVE.
    for (const AxialMethod axial : kAxialMethods)
    {
        const RocketWithBody built = bodyOfNewRocket();
        auto&                pods  = built.body->addChild(std::make_unique<PodSet>());
        pods.setInstanceCount(2);
        addBody(pods, 0.1, 0.01);
        pods.setAxialMethod(axial);
        pods.setRadiusMethod(RadiusMethod::RELATIVE);
        pods.setRadiusOffset(0.005);
        built.rocket->enableEvents();
        EXPECT_TRUE(assemblyAsJava(pods, "pods-" + axialName(axial)));
    }

    const RocketWithBody built = bodyOfNewRocket();
    auto&                pods  = built.body->addChild(std::make_unique<PodSet>());
    addBody(pods, 0.08, 0.012);
    pods.setRadiusMethod(RadiusMethod::FREE);
    pods.setRadiusOffset(0.05);
    built.rocket->enableEvents();
    EXPECT_TRUE(assemblyAsJava(pods, "pods-FREE"));

    auto& empty = built.body->addChild(std::make_unique<PodSet>());
    built.rocket->enableEvents();
    EXPECT_TRUE(assemblyAsJava(empty, "pods-empty"));
}

TEST(ComponentAssemblyShapes, BoosterSetsAsJava)
{
    for (const AxialMethod axial : kAxialMethods)
    {
        const RocketWithBody built    = bodyOfNewRocket();
        auto&                boosters = built.body->addChild(std::make_unique<ParallelStage>());
        boosters.setInstanceCount(3);
        addBody(boosters, 0.15, 0.015);
        boosters.setAxialMethod(axial);
        boosters.setRadiusMethod(RadiusMethod::SURFACE);
        built.rocket->enableEvents();
        EXPECT_TRUE(assemblyAsJava(boosters, "boosters-" + axialName(axial)));
    }
}

TEST(ComponentAssemblyShapes, TheRocketHasAMarkerAndAPlainStageNone)
{
    // RocketFigure never asks for the rocket's shapes, but ComponentAssemblyShapes would draw a
    // marker for it; it returns null for a plain AxialStage.
    const RocketWithBody built    = bodyOfNewRocket();
    auto&                boosters = built.body->addChild(std::make_unique<ParallelStage>());
    addBody(boosters, 0.15, 0.015);
    boosters.setRadiusMethod(RadiusMethod::RELATIVE);
    boosters.setRadiusOffset(0.002);
    built.rocket->enableEvents();
    EXPECT_TRUE(assemblyAsJava(boosters, "boosters-RELATIVE"));
    EXPECT_TRUE(assemblyAsJava(*built.rocket, "rocket"));
    EXPECT_TRUE(assemblyAsJava(built.rocket->getChild(0), "stage"));
    EXPECT_TRUE(
        ComponentAssemblyShapes::getShapesSide(built.rocket->getChild(0), Transformation::kIdentity)
            .empty());
}

/// Success when @p shapes are @p expected, all visible but the last, which has Color::invisible().
[[nodiscard]] ::testing::AssertionResult markerAsEmptyShapes(
    const std::vector<ComponentShape>& shapes, const std::vector<FigureShape>& expected)
{
    if (shapes.size() != expected.size())
    {
        return ::testing::AssertionFailure() << shapes.size() << " shapes";
    }
    for (std::size_t i = 0; i < shapes.size(); ++i)
    {
        const bool last = i + 1 == shapes.size();
        if (shapes[i].shape() != expected[i] || shapes[i].isVisible() == last)
        {
            return ::testing::AssertionFailure() << "shape " << i << " differs";
        }
    }
    if (shapes.back().color() != Color::invisible())
    {
        return ::testing::AssertionFailure() << "the selection square has a colour";
    }
    return ::testing::AssertionSuccess();
}

TEST(ComponentAssemblyShapes, TheMarkerIsEmptyShapesWithAnInvisibleSelectionSquare)
{
    const RocketWithBody built = bodyOfNewRocket();
    auto&                pods  = built.body->addChild(std::make_unique<PodSet>());
    addBody(pods, 0.1, 0.01);
    // TOP and FREE: the marker sits at the transformed origin.
    pods.setAxialMethod(AxialMethod::TOP);
    pods.setRadiusMethod(RadiusMethod::FREE);
    built.rocket->enableEvents();

    // The rocket's bounding radius is its stage's: the body tube's 0.02.
    const double         radius = built.rocket->getBoundingRadius() * 0.03;
    const Transformation t      = Transformation::translation(0.25, 0.01, 0);
    EXPECT_TRUE(
        markerAsEmptyShapes(ComponentAssemblyShapes::getShapesSide(pods, t),
                            QtRocket::EmptyShapes::getShapesSideWithSelectionSquare(t, radius)));
    EXPECT_TRUE(
        markerAsEmptyShapes(ComponentAssemblyShapes::getShapesBack(pods, t),
                            QtRocket::EmptyShapes::getShapesBackWithSelectionSquare(t, radius)));
}

TEST(ComponentAssemblyShapes, AnAssemblyOutsideARocketIsABug)
{
    // getRocket() fails without a Rocket at the root (Java: IllegalStateException).
    const PodSet pods;
    EXPECT_THROW((void)ComponentAssemblyShapes::getShapesSide(pods, Transformation::kIdentity),
                 BugError);
    const BodyTube tube;
    EXPECT_THROW((void)ComponentAssemblyShapes::getShapesBack(tube, Transformation::kIdentity),
                 BugError);
}

}  // namespace
