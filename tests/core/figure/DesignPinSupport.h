#pragma once

// Loading the designs of the generated pin tables (BodyShapePins.h, FinMassShapePins.h,
// FigureGeometryPins.h) as the Java probes loaded or built them, and comparing a pinned instance's
// transformation and shapes with QtRocket's. Test-only.

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketComponentShapeProvider.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Test
{

/// A design of a pin, loaded and settled: an example's document or a test rocket.
struct PinnedDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    std::unique_ptr<Rocket>             testRocket;

    /// Whether the design was loaded.
    [[nodiscard]] bool isLoaded() const noexcept
    {
        return testRocket != nullptr || document != nullptr;
    }

    /// The rocket of a loaded design.
    [[nodiscard]] Rocket& rocket() const
    {
        if (testRocket != nullptr)
        {
            return *testRocket;
        }
        QTROCKET_ASSERT(document != nullptr);
        return document->getRocket();
    }
};

/// The rocket FinMassShapesProbe builds as "hand-built empty booster sets": a stage with an ogive
/// nose cone (0.1 m, radius 0.02 m) and a body tube (0.3 m, radius 0.02 m) that holds a booster
/// set of three without children (an extra render instance: inactive, since it has no children,
/// while its stage is flagged active), a booster set of two with a body tube (0.15 m, radius
/// 0.01 m) and a pod set of two without children; events enabled, every stage active.
[[nodiscard]] inline std::unique_ptr<Rocket> makeEmptyBoosterSetsRocket()
{
    auto  rocket = std::make_unique<Rocket>();
    auto& stage  = rocket->addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.02));
    auto body = std::make_unique<BodyTube>();
    body->setLength(0.3);
    body->setOuterRadius(0.02);
    BodyTube& mount = stage.addChild(std::move(body));
    auto      empty = std::make_unique<ParallelStage>();
    empty->setInstanceCount(3);
    mount.addChild(std::move(empty));
    auto boosters = std::make_unique<ParallelStage>();
    boosters->setInstanceCount(2);
    ParallelStage& boosterSet  = mount.addChild(std::move(boosters));
    auto           boosterBody = std::make_unique<BodyTube>();
    boosterBody->setLength(0.15);
    boosterBody->setOuterRadius(0.01);
    boosterSet.addChild(std::move(boosterBody));
    auto emptyPods = std::make_unique<PodSet>();
    emptyPods->setInstanceCount(2);
    mount.addChild(std::move(emptyPods));
    rocket->enableEvents();
    rocket->getSelectedConfiguration().setAllStages();
    return rocket;
}

/// Loads the design @p name of @p source as the probes did: an example ("example", a file of
/// data/examples) without presets, a test rocket ("testrocket", a maker of TestRockets), both
/// settled; "hand-built" "empty booster sets" (makeEmptyBoosterSetsRocket()) and "new" "design"
/// (OpenRocketDocumentFactory::createNewRocket()) as they are. Fails the test (and returns
/// nothing) when it cannot.
[[nodiscard]] inline PinnedDesign loadPinnedDesign(std::string_view source, std::string_view name)
{
    PinnedDesign design;
    if (source == "hand-built" && name == "empty booster sets")
    {
        design.testRocket = makeEmptyBoosterSetsRocket();
        return design;
    }
    if (source == "new" && name == "design")
    {
        design.document = OpenRocketDocumentFactory::createNewRocket();
        return design;
    }
    if (source == "example")
    {
        const GoldenExample* example = findGoldenExample(name);
        if (example == nullptr)
        {
            ADD_FAILURE() << "no example " << name;
            return design;
        }
        Result<LoadedDocument> loaded = loadSettledGoldenExample(
            *example, ExampleSource::ORIGINAL, DesignFileEnvironment::Presets::NONE);
        if (!loaded)
        {
            ADD_FAILURE() << loaded.error().message;
            return design;
        }
        design.document = std::move(loaded->document);
        return design;
    }
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        if (source == "testrocket" && maker.method == name)
        {
            design.testRocket         = maker.make();
            const Result<int> settled = settleAutomaticDimensions(*design.testRocket);
            if (!settled)
            {
                ADD_FAILURE() << settled.error().message;
                design.testRocket.reset();
            }
            return design;
        }
    }
    ADD_FAILURE() << "no design " << source << " " << name;
    return design;
}

/// The components of @p rocket in pre-order, the rocket first (the probes' tree indices).
[[nodiscard]] inline std::vector<const RocketComponent*> treeOrder(const Rocket& rocket)
{
    std::vector<const RocketComponent*> order;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        order.push_back(&component);
    }
    return order;
}

/// Whether @p a and @p b differ by no more than sin and cos can make them.
[[nodiscard]] inline bool transformationsNear(const Transformation& a, const TransformPin& b)
{
    const Transformation expected = pinnedTransformation(b);
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            if (!figureValueNear(a.matrix().at(row).at(column),
                                 expected.matrix().at(row).at(column), kTranscendentalTolerance))
            {
                return false;
            }
        }
    }
    const Coordinate& t = a.translationVector();
    const Coordinate& e = expected.translationVector();
    return figureValueNear(t.x, e.x, kTranscendentalTolerance) &&
           figureValueNear(t.y, e.y, kTranscendentalTolerance) &&
           figureValueNear(t.z, e.z, kTranscendentalTolerance);
}

/// The tolerance of the shapes of a component (0 for exact).
using ShapeToleranceFunction = double (*)(const RocketComponent&);

/// Success when the instance of @p pin is QtRocket's too (the context at the pinned index of the
/// pinned component, of the pinned number, whose transformation in the pinned view is Java's
/// within the sin/cos tolerance) and the provider gives the pinned shapes for Java's exact
/// transformation, within @p tolerance of the component.
[[nodiscard]] inline ::testing::AssertionResult instanceAsJava(
    const InstancePin& pin, const FlightConfiguration& configuration,
    const std::vector<const RocketComponent*>& order, ShapeToleranceFunction tolerance)
{
    const auto index = static_cast<std::size_t>(pin.treeIndex);
    if (index >= order.size())
    {
        return ::testing::AssertionFailure() << "no component " << pin.treeIndex;
    }
    const RocketComponent&                 component = *order[index];
    const std::span<const InstanceContext> contexts =
        configuration.getActiveInstances().getInstanceContexts(component);
    const auto contextIndex = static_cast<std::size_t>(pin.contextIndex);
    if (contextIndex >= contexts.size() ||
        contexts[contextIndex].instanceNumber != pin.instanceNumber)
    {
        return ::testing::AssertionFailure()
               << "no instance " << pin.contextIndex << " (number " << pin.instanceNumber << ")";
    }
    const Transformation ours = figureRotation(pin.view, pin.rotation)
                                    .applyTransformation(contexts[contextIndex].transform);
    if (!transformationsNear(ours, pin.transform))
    {
        return ::testing::AssertionFailure()
               << "the instance's transformation is " << ours.toString() << ", Java's "
               << pinnedTransformation(pin.transform).toString();
    }
    const std::vector<ComponentShape> shapes = RocketComponentShapeProvider::getShapes(
        pin.view, component, pinnedTransformation(pin.transform));
    return shapesMatchPins(shapes, pin.shapes, component, tolerance(component));
}

/// Success when every instance of @p pin is as Java's (instanceAsJava()) in the design @p rocket.
[[nodiscard]] inline ::testing::AssertionResult designInstancesAsJava(
    const DesignPin& pin, const Rocket& rocket, ShapeToleranceFunction tolerance)
{
    const std::vector<const RocketComponent*> order = treeOrder(rocket);
    for (const InstancePin& instance : pin.instances)
    {
        const ::testing::AssertionResult result =
            instanceAsJava(instance, rocket.getSelectedConfiguration(), order, tolerance);
        if (!result)
        {
            return ::testing::AssertionFailure()
                   << "component " << instance.treeIndex << " instance " << instance.contextIndex
                   << " in the " << figureViewName(instance.view) << " view rotated by "
                   << instance.rotation << ": " << result.message();
        }
    }
    return ::testing::AssertionSuccess();
}

/// A test name for the design @p name of @p source: "<source>_<name>" with every run of other
/// characters than letters and digits made one '_' and the name's extension dropped.
[[nodiscard]] inline std::string pinnedDesignTestName(std::string_view source,
                                                      std::string_view name)
{
    std::string testName;
    for (const char c : source)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        {
            testName += c;
        }
    }
    testName += '_';
    for (const char c : name)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        {
            testName += c;
        }
        else if (c == '.')
        {
            break;
        }
        else if (testName.back() != '_')
        {
            testName += '_';
        }
    }
    return testName;
}

}  // namespace QtRocket::Test
