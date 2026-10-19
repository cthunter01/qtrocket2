// The figures RocketFigureGeometry builds for the test rockets, a few examples, a rocket with
// booster sets without children and a new design, compared with what OpenRocket's RocketFigure
// assembles for them (FigureGeometryPins.h): the subject bounds, every instance that has shapes
// (which instance, how many shapes, its first shape's primitive and bounds) and the motors, and
// that the shapes are in the stable drawing order: the highest display order first, equal ones
// in the order they were added (extra render instances last, each map in tree order, a
// component's instances in order).
//
// QtRocket computes the instances' transformations itself here, so values that went through the
// figure's rotation are compared within kTranscendentalTolerance, and the bounds of float paths
// (fins, conical transitions) within kFloatTolerance: a double that differs from Java's in its
// last bit can round to the neighbouring float.

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/DesignPinSupport.h"
#include "figure/FigureGeometryPins.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::FigureArc;
using QtRocket::FigurePath;
using QtRocket::FigureShape;
using QtRocket::Rectangle2D;
using QtRocket::RocketComponent;
using QtRocket::RocketFigureGeometry;
using QtRocket::Test::FigureInstancePin;
using QtRocket::Test::FigureMotorPin;
using QtRocket::Test::FigurePin;
using QtRocket::Test::kFloatTolerance;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::PinnedDesign;
using QtRocket::Test::PinShape;
using QtRocket::Test::rectanglesNear;
using QtRocket::Test::FigureGeometryPins::kFigures;

/// What a pin calls the arc type @p type.
[[nodiscard]] PinShape pinShapeOfArc(FigureArc::Type type)
{
    switch (type)
    {
        case FigureArc::Type::OPEN:
            return PinShape::ARC_OPEN;
        case FigureArc::Type::CHORD:
            return PinShape::ARC_CHORD;
        case FigureArc::Type::PIE:
            return PinShape::ARC_PIE;
    }
    return PinShape::ARC_OPEN;
}

/// What a pin calls @p shape's primitive.
[[nodiscard]] PinShape pinShapeOf(const FigureShape& shape)
{
    if (const auto* arc = shape.getIf<FigureArc>(); arc != nullptr)
    {
        return pinShapeOfArc(arc->type());
    }
    if (const auto* path = shape.getIf<FigurePath>(); path != nullptr)
    {
        return path->precision() == FigurePath::Precision::FLOAT ? PinShape::PATH_FLOAT
                                                                 : PinShape::PATH_DOUBLE;
    }
    switch (shape.type())
    {
        case FigureShape::Type::ELLIPSE:
            return PinShape::ELLIPSE;
        case FigureShape::Type::ROUND_RECT:
            return PinShape::ROUND_RECT;
        case FigureShape::Type::LINE:
            return PinShape::LINE;
        case FigureShape::Type::RECT:
        case FigureShape::Type::ARC:
        case FigureShape::Type::PATH:
            break;
    }
    return PinShape::RECT;
}

/// One instance of a built figure: the run of consecutive shapes of one component's context.
struct BuiltInstance
{
    int                treeIndex;
    bool               extraRender;
    int                contextIndex;
    int                instanceNumber;
    int                displayOrder;
    int                shapeCount;
    const FigureShape* firstShape;

    /// The order of the instance maps: the extra render instances last, then tree and context.
    [[nodiscard]] std::tuple<bool, int, int> key() const noexcept
    {
        return {extraRender, treeIndex, contextIndex};
    }
};

/// The index of @p component in @p order.
[[nodiscard]] int treeIndexOf(const std::vector<const RocketComponent*>& order,
                              const RocketComponent&                     component)
{
    const auto found = std::ranges::find(order, &component);
    return found == order.end() ? -1 : static_cast<int>(std::distance(order.begin(), found));
}

/// The instances of @p geometry, in drawing order.
[[nodiscard]] std::vector<BuiltInstance> builtInstances(
    const RocketFigureGeometry& geometry, const std::vector<const RocketComponent*>& order)
{
    std::vector<BuiltInstance> instances;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        const int tree = treeIndexOf(order, shape.shape.component());
        if (!instances.empty() && instances.back().treeIndex == tree &&
            instances.back().extraRender == shape.extraRender &&
            instances.back().contextIndex == shape.contextIndex)
        {
            ++instances.back().shapeCount;
            continue;
        }
        instances.push_back(BuiltInstance{
            .treeIndex      = tree,
            .extraRender    = shape.extraRender,
            .contextIndex   = shape.contextIndex,
            .instanceNumber = shape.instanceNumber,
            .displayOrder   = QtRocket::displayOrder(geometry.view(), shape.shape.component()),
            .shapeCount     = 1,
            .firstShape     = &shape.shape.shape()});
    }
    return instances;
}

/// Success when @p instances are in the stable drawing order: display order descending, then
/// the order of the instance maps.
[[nodiscard]] ::testing::AssertionResult inDrawingOrder(const std::vector<BuiltInstance>& instances)
{
    for (std::size_t i = 1; i < instances.size(); ++i)
    {
        const BuiltInstance& a = instances[i - 1];
        const BuiltInstance& b = instances[i];
        if (a.displayOrder < b.displayOrder ||
            (a.displayOrder == b.displayOrder && !(a.key() < b.key())))
        {
            return ::testing::AssertionFailure()
                   << "instance " << i << " (component " << b.treeIndex << ") is out of order";
        }
    }
    return ::testing::AssertionSuccess();
}

/// Success when @p actual is the instance @p pin.
[[nodiscard]] ::testing::AssertionResult instanceMatchesPin(const BuiltInstance&     actual,
                                                            const FigureInstancePin& pin)
{
    if (actual.treeIndex != pin.treeIndex || actual.extraRender != pin.extraRender ||
        actual.contextIndex != pin.contextIndex || actual.instanceNumber != pin.instanceNumber ||
        actual.displayOrder != pin.displayOrder || actual.shapeCount != pin.shapeCount ||
        pinShapeOf(*actual.firstShape) != pin.type)
    {
        return ::testing::AssertionFailure() << std::format(
                   "instance {} {} {} (number {}, order {}, {} shapes), expected {} {} "
                   "{} (number {}, order {}, {} shapes)",
                   actual.treeIndex, actual.extraRender, actual.contextIndex, actual.instanceNumber,
                   actual.displayOrder, actual.shapeCount, pin.treeIndex, pin.extraRender,
                   pin.contextIndex, pin.instanceNumber, pin.displayOrder, pin.shapeCount);
    }
    const double tolerance =
        pin.type == PinShape::PATH_FLOAT ? kFloatTolerance : kTranscendentalTolerance;
    const Rectangle2D expected{
        .x = pin.bounds[0], .y = pin.bounds[1], .width = pin.bounds[2], .height = pin.bounds[3]};
    const ::testing::AssertionResult bounds =
        rectanglesNear(actual.firstShape->bounds(), expected, tolerance);
    if (!bounds)
    {
        return ::testing::AssertionFailure()
               << "instance of component " << pin.treeIndex << " context " << pin.contextIndex
               << ": " << bounds.message();
    }
    return ::testing::AssertionSuccess();
}

/// Success when @p geometry's motors, sorted by mount and location, are @p pins.
[[nodiscard]] ::testing::AssertionResult motorsMatchPins(
    const RocketFigureGeometry& geometry, const std::vector<const RocketComponent*>& order,
    std::span<const FigureMotorPin> pins)
{
    std::vector<const RocketFigureGeometry::MotorShape*> motors;
    for (const RocketFigureGeometry::MotorShape& motor : geometry.motors())
    {
        motors.push_back(&motor);
    }
    std::ranges::stable_sort(motors, {}, [&order](const RocketFigureGeometry::MotorShape* m) {
        return std::pair{treeIndexOf(order, *m->mount), m->locationIndex};
    });
    if (motors.size() != pins.size())
    {
        return ::testing::AssertionFailure()
               << motors.size() << " motors, expected " << pins.size();
    }
    for (std::size_t i = 0; i < pins.size(); ++i)
    {
        const RocketFigureGeometry::MotorShape& motor = *motors[i];
        const FigureMotorPin&                   pin   = pins[i];
        const Rectangle2D                       expected{.x      = pin.values[0],
                                                         .y      = pin.values[1],
                                                         .width  = pin.values[2],
                                                         .height = pin.values[3]};
        // A motor's Rect or Ellipse has its parameters as its bounds.
        if (treeIndexOf(order, *motor.mount) != pin.mountTreeIndex ||
            motor.locationIndex != pin.locationIndex || pinShapeOf(motor.shape) != pin.type ||
            !rectanglesNear(motor.shape.bounds(), expected, kTranscendentalTolerance))
        {
            return ::testing::AssertionFailure() << "motor " << i << " differs";
        }
    }
    return ::testing::AssertionSuccess();
}

class FigureGeometryOfDesigns : public ::testing::TestWithParam<FigurePin>
{ };

TEST_P(FigureGeometryOfDesigns, AsJava)
{
    const FigurePin&   pin    = GetParam();
    const PinnedDesign design = QtRocket::Test::loadPinnedDesign(pin.source, pin.name);
    ASSERT_TRUE(design.isLoaded());
    const std::vector<const RocketComponent*> order = QtRocket::Test::treeOrder(design.rocket());
    std::vector<const RocketComponent*>       selection;
    if (pin.selectedTreeIndex >= 0)
    {
        selection.push_back(order.at(static_cast<std::size_t>(pin.selectedTreeIndex)));
    }

    const RocketFigureGeometry geometry =
        RocketFigureGeometry::build(design.rocket().getSelectedConfiguration(), pin.view,
                                    pin.rotation, selection, pin.showMarkers);

    const Rectangle2D subject{.x      = pin.subjectBounds[0],
                              .y      = pin.subjectBounds[1],
                              .width  = pin.subjectBounds[2],
                              .height = pin.subjectBounds[3]};
    EXPECT_TRUE(rectanglesNear(geometry.subjectBounds(), subject, kTranscendentalTolerance));

    std::vector<BuiltInstance> instances = builtInstances(geometry, order);
    EXPECT_TRUE(inDrawingOrder(instances));
    std::ranges::sort(instances, {}, &BuiltInstance::key);
    ASSERT_EQ(instances.size(), pin.instances.size());
    for (std::size_t i = 0; i < instances.size(); ++i)
    {
        EXPECT_TRUE(instanceMatchesPin(instances[i], pin.instances[i])) << i;
    }

    EXPECT_TRUE(motorsMatchPins(geometry, order, pin.motors));
}

[[nodiscard]] std::string figureTestName(const ::testing::TestParamInfo<FigurePin>& info)
{
    const FigurePin& pin = info.param;
    return std::format("{}_{}{}_{}{}", QtRocket::Test::pinnedDesignTestName(pin.source, pin.name),
                       QtRocket::figureViewName(pin.view), pin.rotation == 0 ? "0" : "30",
                       pin.showMarkers ? "markers" : "all",
                       pin.selectedTreeIndex >= 0
                           ? std::format("_selected{}", pin.selectedTreeIndex)
                           : std::string{});
}

INSTANTIATE_TEST_SUITE_P(Pins, FigureGeometryOfDesigns, ::testing::ValuesIn(kFigures),
                         figureTestName);

}  // namespace
