// The shapes of body tubes, transitions, nose cones, ring components, launch lugs and tube fin
// sets of example designs and test rockets, compared with what OpenRocket's providers returned
// for the same instances in the side, top and back views (BodyShapePins.h).
//
// Each pin holds the transformation OpenRocket's RocketFigure handed the provider for an
// instance in a view. The test checks that QtRocket's own instance and figure rotation give the
// same transformation (within the sin/cos tolerance), then hands the provider Java's exact
// transformation and compares the shapes exactly: they come from +, -, * and / of the
// component's dimensions and the transformation, the outlines of ogive, ellipsoid, parabolic and
// conical shapes from sqrt as well. A power or Haack outline goes through pow, or acos and sin,
// and is compared within kTranscendentalTolerance.

#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketComponentShapeProvider.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::ComponentShape;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignPin;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::figureValueNear;
using QtRocket::Test::InstancePin;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::shapesMatchPins;
using QtRocket::Test::TransformPin;
using QtRocket::Test::BodyShapePins::kDesigns;

/// A design of a pin, loaded and settled: an example's document or a test rocket.
struct PinnedDesign
{
    std::unique_ptr<QtRocket::OpenRocketDocument> document;
    std::unique_ptr<Rocket>                       testRocket;

    /// Whether the design was loaded.
    [[nodiscard]] bool isLoaded() const noexcept
    {
        return testRocket != nullptr || document != nullptr;
    }

    /// The rocket of a loaded design.
    [[nodiscard]] const Rocket& rocket() const
    {
        if (testRocket != nullptr)
        {
            return *testRocket;
        }
        QTROCKET_ASSERT(document != nullptr);
        return document->getRocket();
    }
};

/// Loads the design of @p pin as the probe did: an example without presets, a test rocket from
/// its maker, both settled. Fails the test (and returns nothing) when it cannot.
[[nodiscard]] PinnedDesign loadPinnedDesign(const DesignPin& pin)
{
    PinnedDesign design;
    if (pin.source == "example")
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(pin.name);
        if (example == nullptr)
        {
            ADD_FAILURE() << "no example " << pin.name;
            return design;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, ExampleSource::ORIGINAL, DesignFileEnvironment::Presets::NONE);
        if (!loaded)
        {
            ADD_FAILURE() << loaded.error().message;
            return design;
        }
        design.document = std::move(loaded->document);
        return design;
    }
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        if (maker.method == pin.name)
        {
            design.testRocket         = maker.make();
            const Result<int> settled = QtRocket::settleAutomaticDimensions(*design.testRocket);
            if (!settled)
            {
                ADD_FAILURE() << settled.error().message;
                design.testRocket.reset();
            }
            return design;
        }
    }
    ADD_FAILURE() << "no test rocket maker " << pin.name;
    return design;
}

/// The components of @p rocket in pre-order, the rocket first (the probe's tree indices).
[[nodiscard]] std::vector<const RocketComponent*> treeOrder(const Rocket& rocket)
{
    std::vector<const RocketComponent*> order;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        order.push_back(&component);
    }
    return order;
}

/// Whether @p a and @p b differ by no more than sin and cos can make them.
[[nodiscard]] bool transformationsNear(const Transformation& a, const TransformPin& b)
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
    const QtRocket::Coordinate& t = a.translationVector();
    const QtRocket::Coordinate& e = expected.translationVector();
    return figureValueNear(t.x, e.x, kTranscendentalTolerance) &&
           figureValueNear(t.y, e.y, kTranscendentalTolerance) &&
           figureValueNear(t.z, e.z, kTranscendentalTolerance);
}

/// The tolerance of @p component's shapes: an outline sampled from a power or Haack profile
/// went through pow, acos and sin.
[[nodiscard]] double shapeTolerance(const RocketComponent& component)
{
    const auto* transition = dynamic_cast<const Transition*>(&component);
    if (transition != nullptr && (transition->getShapeType() == TransitionShape::POWER ||
                                  transition->getShapeType() == TransitionShape::HAACK))
    {
        return kTranscendentalTolerance;
    }
    return 0;
}

/// Success when the instance of @p pin is QtRocket's too and the provider gives its shapes.
[[nodiscard]] ::testing::AssertionResult instanceAsJava(
    const InstancePin& pin, const QtRocket::FlightConfiguration& configuration,
    const std::vector<const RocketComponent*>& order)
{
    const auto index = static_cast<std::size_t>(pin.treeIndex);
    if (index >= order.size())
    {
        return ::testing::AssertionFailure() << "no component " << pin.treeIndex;
    }
    const RocketComponent&                           component = *order[index];
    const std::span<const QtRocket::InstanceContext> contexts =
        configuration.getActiveInstances().getInstanceContexts(component);
    const auto contextIndex = static_cast<std::size_t>(pin.contextIndex);
    if (contextIndex >= contexts.size() ||
        contexts[contextIndex].instanceNumber != pin.instanceNumber)
    {
        return ::testing::AssertionFailure()
               << "no instance " << pin.contextIndex << " (number " << pin.instanceNumber << ")";
    }
    const Transformation ours = QtRocket::figureRotation(pin.view, pin.rotation)
                                    .applyTransformation(contexts[contextIndex].transform);
    if (!transformationsNear(ours, pin.transform))
    {
        return ::testing::AssertionFailure()
               << "the instance's transformation is " << ours.toString() << ", Java's "
               << pinnedTransformation(pin.transform).toString();
    }
    const std::vector<ComponentShape> shapes = QtRocket::RocketComponentShapeProvider::getShapes(
        pin.view, component, pinnedTransformation(pin.transform));
    return shapesMatchPins(shapes, pin.shapes, component, shapeTolerance(component));
}

class BodyShapesOfDesigns : public ::testing::TestWithParam<DesignPin>
{ };

TEST_P(BodyShapesOfDesigns, AsJava)
{
    const DesignPin&   pin    = GetParam();
    const PinnedDesign design = loadPinnedDesign(pin);
    ASSERT_TRUE(design.isLoaded());
    const Rocket&                             rocket = design.rocket();
    const std::vector<const RocketComponent*> order  = treeOrder(rocket);
    for (const InstancePin& instance : pin.instances)
    {
        EXPECT_TRUE(instanceAsJava(instance, rocket.getSelectedConfiguration(), order))
            << "component " << instance.treeIndex << " instance " << instance.contextIndex
            << " in the " << QtRocket::figureViewName(instance.view) << " view rotated by "
            << instance.rotation;
    }
}

[[nodiscard]] std::string designTestName(const ::testing::TestParamInfo<DesignPin>& info)
{
    std::string name = std::format("{}_", info.param.source);
    for (const char c : info.param.name)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        {
            name += c;
        }
        else if (c == '.')
        {
            break;
        }
        else if (name.back() != '_')
        {
            name += '_';
        }
    }
    return name;
}

INSTANTIATE_TEST_SUITE_P(Pins, BodyShapesOfDesigns, ::testing::ValuesIn(kDesigns), designTestName);

}  // namespace
