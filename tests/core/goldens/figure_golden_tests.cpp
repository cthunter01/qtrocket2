// Figure golden tests: QtRocket's 2D figure geometry (RocketFigureGeometry of figure/) for every
// design of the goldens against tests/data/goldens/<input>/figure.json, what OpenRocket's
// RocketFigure draws of the design (tools/openrocket-goldens, FigureDumper; README.md has the
// format). The sixteen examples are loaded and settled as the golden harness loads them
// (GoldenExamples.h), the thirteen test rockets built by their makers (TestRockets.h) and
// settled; the figure is that of the rocket's selected flight configuration, in the five views of
// the goldens (side, top and back at rotation 0, the side view at 30 and at 225 degrees), with
// RocketFigure's "only show pod set/booster markers when selected" off and nothing selected.
//
// Compared per view:
// - the subject bounds;
// - the instances, which QtRocket draws in display order, the highest first, and equal display
//   orders in the order they were added (the active instances before the extra render ones, in
//   tree order, a component's in context order: the user's decision U4). Java draws equal
//   display orders in the order of its PriorityQueue, fed from hash maps, which the golden does
//   not keep: it lists the instances sorted by (extra render, tree order, context), each with its
//   display order. QtRocket's instances, checked to be in that drawing order and then sorted the
//   same way, have to be the golden's one for one, so that each display order has the same set
//   of instances in Java and in QtRocket. Of each: the component, the context, the instance
//   number, the display order, the component's colour and line style, and every shape (its
//   visibility, the colour and line style it carries, its primitive and its parameters);
// - the motors, sorted by the tree order of their mounts and by location, as the golden is
//   (Java's getActiveMotors() iterates a hash map).
//
// Exactness: a value is compared exactly (==, a zero matching a zero of either sign) where the
// arithmetic allows it: QtRocket's figure rotation of the view is Java's bit for bit (the golden
// records Java's; the sine and cosine of a rotation can differ in the last bit between Java and a
// C library), so is the instance's transformation (geometry.json records Java's for the active
// instances of the configuration), and the component's provider and its dimensions use nothing
// but +, -, *, / and sqrt. The providers that use more, as part F2's and F3's pin tests
// established: the outlines sampled from a power or Haack profile (pow, acos, sin), canted and
// elliptical fin sets and fin sets on a power or Haack transition (sin and cos before the float
// rounding of their outlines), rail buttons (the view angle), trackers (their arcs), mass
// objects off the axis (cos and sin of their radial direction) and tube fin sets of automatic
// radius with three fins or more (TubeFinSet::getTouchingRadius(), the sine of pi over the fin
// count). Everything else is compared
// within kTranscendentalTolerance (1e-14 relative to max(1, |value|)), a Path2D.Float's
// coordinates within kFloatTolerance (2^-23: a double that differs from Java's in its last bit can
// round to the neighbouring float). The subject bounds are always compared within
// kTranscendentalTolerance (Java's Math.hypot is fdlibm's), the motors exactly when the figure
// rotation and the mount's component locations are Java's bit for bit. Each test records how many
// shapes and motors were compared exactly (the properties "exactShapes" and "exactMotors").
//
// The rays of a deployment charge come from an unseeded java.util.Random in OpenRocket and from a
// generator seeded with the component's id in QtRocket (decision U4): they are compared by their
// number and their range only (FigureGoldenComparison tests that comparison; no golden design has
// a deployment charge).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/MassComponentShapes.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenFigure.h"
#include "goldens/GoldenGeometry.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::ComponentKind;
using QtRocket::ComponentShape;
using QtRocket::FigurePath;
using QtRocket::FigureShape;
using QtRocket::FigureView;
using QtRocket::FlightConfiguration;
using QtRocket::Rectangle2D;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketFigureGeometry;
using QtRocket::Transformation;
using QtRocket::Test::figureShapeDifference;
using QtRocket::Test::figureValueMatches;
using QtRocket::Test::GoldenFigure;
using QtRocket::Test::GoldenFigureInstance;
using QtRocket::Test::GoldenFigureMotor;
using QtRocket::Test::GoldenFigureShape;
using QtRocket::Test::GoldenFigureView;
using QtRocket::Test::kFloatTolerance;
using QtRocket::Test::kTranscendentalTolerance;

// ================================================================================ tolerances

/// Whether @p component is a transition or nose cone whose outline is sampled from a power or
/// Haack profile.
[[nodiscard]] bool sampledWithTranscendentals(const RocketComponent* component)
{
    const auto* transition = dynamic_cast<const QtRocket::Transition*>(component);
    return transition != nullptr &&
           (transition->getShapeType() == QtRocket::TransitionShape::POWER ||
            transition->getShapeType() == QtRocket::TransitionShape::HAACK);
}

/// Whether @p component's shapes go through sin, cos, atan2, acos or pow (see the file comment).
[[nodiscard]] bool usesTranscendentals(const RocketComponent& component)
{
    if (component.kind() == ComponentKind::RAIL_BUTTON || sampledWithTranscendentals(&component))
    {
        return true;
    }
    if (const auto* tubeFins = dynamic_cast<const QtRocket::TubeFinSet*>(&component))
    {
        // getOuterRadius() is getTouchingRadius() then: r * sin(pi / n) / (1 - sin(pi / n)).
        return tubeFins->isOuterRadiusAutomatic() && tubeFins->getFinCount() >= 3;
    }
    if (const auto* fins = dynamic_cast<const QtRocket::FinSet*>(&component))
    {
        return !QtRocket::MathUtil::equals(fins->getCantAngle(), 0) ||
               component.kind() == ComponentKind::ELLIPTICAL_FIN_SET ||
               sampledWithTranscendentals(component.getParent());
    }
    if (const auto* massObject = dynamic_cast<const QtRocket::MassObject*>(&component))
    {
        const auto* massComponent = dynamic_cast<const QtRocket::MassComponent*>(&component);
        return massObject->getRadialPosition() != 0 ||
               (massComponent != nullptr &&
                massComponent->getMassComponentType() ==
                    QtRocket::MassComponent::MassComponentType::TRACKER);
    }
    return false;
}

/// The tolerance of @p shape when it cannot be compared exactly.
[[nodiscard]] double inexactTolerance(const FigureShape& shape)
{
    const auto* path = shape.getIf<FigurePath>();
    return path != nullptr && path->precision() == FigurePath::Precision::FLOAT
               ? kFloatTolerance
               : kTranscendentalTolerance;
}

/// Whether @p a and @p b are the same transformation bit for bit (but for the sign of zeros).
[[nodiscard]] bool sameTransformation(const Transformation& a, const Transformation& b)
{
    const QtRocket::Coordinate& ta = a.translationVector();
    const QtRocket::Coordinate& tb = b.translationVector();
    return a.matrix() == b.matrix() && ta.x == tb.x && ta.y == tb.y && ta.z == tb.z;
}

/// Whether @p a is @p b within kTranscendentalTolerance.
[[nodiscard]] bool nearTransformation(const Transformation& a, const Transformation& b)
{
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            if (!figureValueMatches(a.matrix().at(row).at(column), b.matrix().at(row).at(column),
                                    kTranscendentalTolerance))
            {
                return false;
            }
        }
    }
    const QtRocket::Coordinate& ta = a.translationVector();
    const QtRocket::Coordinate& tb = b.translationVector();
    return figureValueMatches(ta.x, tb.x, kTranscendentalTolerance) &&
           figureValueMatches(ta.y, tb.y, kTranscendentalTolerance) &&
           figureValueMatches(ta.z, tb.z, kTranscendentalTolerance);
}

// ===================================================================== what geometry.json says

/// What geometry.json records of the configuration the figure shows: Java's InstanceContext
/// transformations of its active instances, and Java's component locations, by path.
struct GoldenPlacements
{
    std::map<std::string, std::vector<Transformation>, std::less<>>       contexts;
    std::map<std::string, std::vector<QtRocket::Coordinate>, std::less<>> locations;
};

/// A golden {"rotation", "translation"} transformation of geometry.json (a value that is no number
/// reads as NaN, which no transformation equals, and a missing entry throws, failing the test).
[[nodiscard]] Transformation goldenTransformationOf(const json& entry)
{
    const json&                   r = entry.at("rotation");
    const json&                   t = entry.at("translation");
    const Transformation::Matrix3 matrix{
        {{QtRocket::Test::goldenValue(r.at(0)), QtRocket::Test::goldenValue(r.at(1)),
          QtRocket::Test::goldenValue(r.at(2))},
         {QtRocket::Test::goldenValue(r.at(3)), QtRocket::Test::goldenValue(r.at(4)),
          QtRocket::Test::goldenValue(r.at(5))},
         {QtRocket::Test::goldenValue(r.at(6)), QtRocket::Test::goldenValue(r.at(7)),
          QtRocket::Test::goldenValue(r.at(8))}}};
    return Transformation{matrix, QtRocket::Test::goldenCoordinate(t)};
}

/// The placements geometry.json of @p input records for configuration @p configurationIndex.
[[nodiscard]] GoldenPlacements goldenPlacements(std::string_view input, int configurationIndex)
{
    GoldenPlacements                    placements;
    const QtRocket::Result<const json*> geometry = QtRocket::Test::goldenGeometry(input);
    if (!geometry)
    {
        ADD_FAILURE() << geometry.error().message;
        return placements;
    }
    for (const json& component : (*geometry)->at("components"))
    {
        std::vector<QtRocket::Coordinate>& list =
            placements.locations[component.at("path").get<std::string>()];
        for (const json& location : component.at("componentLocations"))
        {
            list.push_back(QtRocket::Test::goldenCoordinate(location));
        }
    }
    const json& configuration =
        (*geometry)->at("configurations").at(static_cast<std::size_t>(configurationIndex));
    for (const json& entry : configuration.at("instances"))
    {
        std::vector<Transformation>& list =
            placements.contexts[entry.at("path").get<std::string>()];
        for (const json& context : entry.at("instances"))
        {
            list.push_back(goldenTransformationOf(context.at("transform")));
        }
    }
    return placements;
}

// ============================================================================ QtRocket's side

/// One instance of QtRocket's figure: the run of consecutive shapes of one component's context.
struct BuiltInstance
{
    const RocketComponent*             component{nullptr};
    int                                treeIndex{0};
    bool                               extraRender{false};
    int                                contextIndex{0};
    int                                instanceNumber{0};
    int                                displayOrder{0};
    std::vector<const ComponentShape*> shapes;

    /// The order of the golden: the extra render instances last, then tree order and context.
    [[nodiscard]] std::tuple<bool, int, int> key() const noexcept
    {
        return {extraRender, treeIndex, contextIndex};
    }
};

/// The tree index (pre-order, the rocket first) of every component of @p rocket.
[[nodiscard]] std::map<const RocketComponent*, int> treeIndices(const Rocket& rocket)
{
    std::map<const RocketComponent*, int> indices;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        indices.emplace(&component, static_cast<int>(indices.size()));
    }
    return indices;
}

/// The instances of @p geometry, in drawing order.
[[nodiscard]] std::vector<BuiltInstance> builtInstances(
    const RocketFigureGeometry& geometry, const std::map<const RocketComponent*, int>& indices)
{
    std::vector<BuiltInstance> instances;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        const RocketComponent& component = shape.shape.component();
        if (!instances.empty() && instances.back().component == &component &&
            instances.back().extraRender == shape.extraRender &&
            instances.back().contextIndex == shape.contextIndex)
        {
            instances.back().shapes.push_back(&shape.shape);
            continue;
        }
        const auto index = indices.find(&component);
        instances.push_back(
            BuiltInstance{.component      = &component,
                          .treeIndex      = index == indices.end() ? -1 : index->second,
                          .extraRender    = shape.extraRender,
                          .contextIndex   = shape.contextIndex,
                          .instanceNumber = shape.instanceNumber,
                          .displayOrder   = QtRocket::displayOrder(geometry.view(), component),
                          .shapes         = {&shape.shape}});
    }
    return instances;
}

/// What a comparison of a figure found.
struct FigureComparison
{
    std::string report;  ///< the mismatches, one per line; empty when everything matched
    int         instances{0};
    int         shapes{0};
    int         exactShapes{0};
    int         motors{0};
    int         exactMotors{0};

    void fail(std::string_view context, std::string_view what)
    {
        report += std::format("{}: {}\n", context, what);
    }
};

/// Checks that @p instances are in QtRocket's drawing order: display order descending, equal
/// ones in the golden's key order.
void checkDrawingOrder(const std::vector<BuiltInstance>& instances, std::string_view context,
                       FigureComparison& comparison)
{
    for (std::size_t i = 1; i < instances.size(); ++i)
    {
        const BuiltInstance& a = instances[i - 1];
        const BuiltInstance& b = instances[i];
        if (a.displayOrder < b.displayOrder ||
            (a.displayOrder == b.displayOrder && !(a.key() < b.key())))
        {
            comparison.fail(context, std::format("instance {} (component {}) is drawn out of order",
                                                 i, b.treeIndex));
        }
    }
}

// =============================================================================== comparisons

/// What a view's comparison needs to know of the design.
struct ViewContext
{
    const GoldenFigureView*    golden{nullptr};
    const FlightConfiguration* configuration{nullptr};
    const GoldenPlacements*    placements{nullptr};
    /// Whether QtRocket's figure rotation of the view is Java's bit for bit.
    bool        exactRotation{false};
    std::string name;  ///< "SIDE 0.5235987755982988", for the messages
};

/// Whether the instance transformation of @p instance is Java's bit for bit.
[[nodiscard]] bool exactContext(const BuiltInstance& instance, std::string_view path,
                                const ViewContext& view)
{
    if (instance.extraRender)
    {
        return false;
    }
    const auto golden = view.placements->contexts.find(path);
    const auto ours =
        view.configuration->getActiveInstances().getInstanceContexts(*instance.component);
    const auto index = static_cast<std::size_t>(instance.contextIndex);
    return golden != view.placements->contexts.end() && index < golden->second.size() &&
           index < ours.size() && sameTransformation(ours[index].transform, golden->second[index]);
}

/// Whether @p component is a deployment charge, whose side view's third shape holds the rays.
[[nodiscard]] bool isDeploymentCharge(const RocketComponent& component)
{
    const auto* mass = dynamic_cast<const QtRocket::MassComponent*>(&component);
    return mass != nullptr && mass->getMassComponentType() ==
                                  QtRocket::MassComponent::MassComponentType::DEPLOYMENTCHARGE;
}

/// Compares the shape @p ours, number @p index of its instance, with the golden instance's
/// shape; exactly when @p exact (but for a deployment charge's rays).
void compareShape(const ComponentShape& ours, std::size_t index,
                  const GoldenFigureInstance& goldenInstance, bool exact, std::string_view context,
                  FigureComparison& comparison)
{
    const GoldenFigureShape& golden       = goldenInstance.shapes.at(index);
    const std::string        shapeContext = std::format("{} shape {}", context, index);
    ++comparison.shapes;
    if (ours.isVisible() != golden.visible)
    {
        comparison.fail(shapeContext, golden.visible ? "is invisible" : "is visible");
    }
    const std::optional<QtRocket::Color> expectedColor =
        golden.visible ? goldenInstance.color : std::optional{QtRocket::Color::invisible()};
    if (ours.color() != expectedColor || ours.lineStyle() != goldenInstance.lineStyle)
    {
        comparison.fail(shapeContext, "carries another colour or line style");
    }
    // The side view of a deployment charge: a RoundRect, an Arc and the rays.
    const bool rays =
        isDeploymentCharge(ours.component()) && index == 2 && goldenInstance.shapes.size() == 3;
    const double tolerance = exact && !rays ? 0 : inexactTolerance(golden.shape);
    const std::optional<std::string> difference =
        rays ? QtRocket::Test::deploymentChargeRaysDifference(
                   ours.shape(), golden.shape, goldenInstance.shapes[0].shape.bounds(),
                   kTranscendentalTolerance)
             : figureShapeDifference(ours.shape(), golden.shape, tolerance);
    if (difference)
    {
        comparison.fail(shapeContext, *difference);
    }
    else if (tolerance == 0)
    {
        ++comparison.exactShapes;
    }
}

/// Compares QtRocket's instance @p ours with the golden @p golden.
void compareInstance(const BuiltInstance& ours, const GoldenFigureInstance& golden,
                     const ViewContext& view, FigureComparison& comparison)
{
    const std::string path = QtRocket::Test::goldenPathOf(*ours.component);
    const std::string context =
        std::format("{} instance {} context {}", view.name, golden.path, golden.context);
    ++comparison.instances;
    if (path != golden.path || ours.extraRender != golden.extraRender ||
        ours.contextIndex != golden.context)
    {
        comparison.fail(context, std::format("QtRocket has the instance {} context {} (extra "
                                             "render {}) there",
                                             path, ours.contextIndex, ours.extraRender));
        return;
    }
    if (ours.instanceNumber != golden.instanceNumber || ours.displayOrder != golden.displayOrder)
    {
        comparison.fail(context, std::format("instance number {} and display order {}, expected "
                                             "{} and {}",
                                             ours.instanceNumber, ours.displayOrder,
                                             golden.instanceNumber, golden.displayOrder));
    }
    if (ours.component->getColor() != golden.color ||
        ours.component->getLineStyle() != golden.lineStyle)
    {
        comparison.fail(context, "the component has another colour or line style");
    }
    if (ours.shapes.size() != golden.shapes.size())
    {
        comparison.fail(context, std::format("{} shapes, expected {}", ours.shapes.size(),
                                             golden.shapes.size()));
        return;
    }
    const bool exact = view.exactRotation && !usesTranscendentals(*ours.component) &&
                       exactContext(ours, path, view);
    for (std::size_t i = 0; i < golden.shapes.size(); ++i)
    {
        compareShape(*ours.shapes[i], i, golden, exact, context, comparison);
    }
}

/// Compares the instances of @p geometry with the golden view's.
void compareInstances(const RocketFigureGeometry&                  geometry,
                      const std::map<const RocketComponent*, int>& indices, const ViewContext& view,
                      FigureComparison& comparison)
{
    std::vector<BuiltInstance> ours = builtInstances(geometry, indices);
    checkDrawingOrder(ours, view.name, comparison);
    std::ranges::stable_sort(ours, {}, &BuiltInstance::key);
    // An instance without shapes leaves nothing in QtRocket's figure (none in the goldens has).
    std::vector<const GoldenFigureInstance*> golden;
    for (const GoldenFigureInstance& instance : view.golden->instances)
    {
        if (!instance.shapes.empty())
        {
            golden.push_back(&instance);
        }
    }
    if (ours.size() != golden.size())
    {
        comparison.fail(view.name, std::format("{} instances with shapes, expected {}", ours.size(),
                                               golden.size()));
    }
    for (std::size_t i = 0; i < std::min(ours.size(), golden.size()); ++i)
    {
        compareInstance(ours[i], *golden[i], view, comparison);
    }
}

/// Whether the component locations of @p mount are Java's bit for bit.
[[nodiscard]] bool exactLocations(const RocketComponent& mount, std::string_view path,
                                  const ViewContext& view)
{
    const auto golden = view.placements->locations.find(path);
    if (golden == view.placements->locations.end())
    {
        return false;
    }
    const std::vector<QtRocket::Coordinate> ours = mount.getComponentLocations();
    return std::ranges::equal(ours, golden->second,
                              [](const QtRocket::Coordinate& a, const QtRocket::Coordinate& b) {
                                  return a.x == b.x && a.y == b.y && a.z == b.z;
                              });
}

/// Compares the motors of @p geometry, sorted by mount and location, with the golden view's.
void compareMotors(const RocketFigureGeometry&                  geometry,
                   const std::map<const RocketComponent*, int>& indices, const ViewContext& view,
                   FigureComparison& comparison)
{
    std::vector<const RocketFigureGeometry::MotorShape*> ours;
    for (const RocketFigureGeometry::MotorShape& motor : geometry.motors())
    {
        ours.push_back(&motor);
    }
    std::ranges::stable_sort(ours, {}, [&indices](const RocketFigureGeometry::MotorShape* m) {
        return std::pair{indices.at(m->mount), m->locationIndex};
    });
    const std::vector<GoldenFigureMotor>& golden = view.golden->motors;
    if (ours.size() != golden.size())
    {
        comparison.fail(view.name,
                        std::format("{} motors, expected {}", ours.size(), golden.size()));
        return;
    }
    for (std::size_t i = 0; i < ours.size(); ++i)
    {
        const std::string path    = QtRocket::Test::goldenPathOf(*ours[i]->mount);
        const std::string context = std::format("{} motor {}", view.name, i);
        ++comparison.motors;
        if (path != golden[i].mount || ours[i]->locationIndex != golden[i].location)
        {
            comparison.fail(
                context, std::format("in {} at location {}, expected {} at {}", path,
                                     ours[i]->locationIndex, golden[i].mount, golden[i].location));
            continue;
        }
        const bool   exact     = view.exactRotation && exactLocations(*ours[i]->mount, path, view);
        const double tolerance = exact ? 0 : kTranscendentalTolerance;
        if (const auto difference =
                figureShapeDifference(ours[i]->shape, golden[i].shape, tolerance))
        {
            comparison.fail(context, *difference);
        }
        else if (exact)
        {
            ++comparison.exactMotors;
        }
    }
}

/// Compares QtRocket's figure of @p configuration in the golden view @p golden.
void compareView(const GoldenFigureView& golden, const FlightConfiguration& configuration,
                 const GoldenPlacements&                      placements,
                 const std::map<const RocketComponent*, int>& indices, FigureComparison& comparison)
{
    const Transformation rotation = QtRocket::figureRotation(golden.view, golden.rotation);
    const ViewContext    view{
        .golden        = &golden,
        .configuration = &configuration,
        .placements    = &placements,
        .exactRotation = sameTransformation(rotation, golden.figureRotation),
        .name = std::format("{} {}", QtRocket::figureViewName(golden.view), golden.rotation)};
    if (!nearTransformation(rotation, golden.figureRotation))
    {
        comparison.fail(view.name,
                        std::format("the figure rotation is {}, Java's {}", rotation.toString(),
                                    golden.figureRotation.toString()));
    }
    const RocketFigureGeometry geometry =
        RocketFigureGeometry::build(configuration, golden.view, golden.rotation,
                                    std::span<const RocketComponent* const>{}, false);
    if (const auto difference = figureShapeDifference(
            FigureShape{QtRocket::FigureRect{geometry.subjectBounds().x, geometry.subjectBounds().y,
                                             geometry.subjectBounds().width,
                                             geometry.subjectBounds().height}},
            FigureShape{QtRocket::FigureRect{golden.subjectBounds.x, golden.subjectBounds.y,
                                             golden.subjectBounds.width,
                                             golden.subjectBounds.height}},
            kTranscendentalTolerance))
    {
        comparison.fail(view.name, std::format("subject bounds: {}", *difference));
    }
    compareInstances(geometry, indices, view, comparison);
    compareMotors(geometry, indices, view, comparison);
}

/// Compares QtRocket's figure of @p rocket with the golden figure of @p input.
[[nodiscard]] FigureComparison compareFigure(const Rocket& rocket, std::string_view input)
{
    FigureComparison                                     comparison;
    const QtRocket::Result<QtRocket::Test::GoldenFigure> golden =
        QtRocket::Test::loadGoldenFigure(input);
    if (!golden)
    {
        comparison.fail(input, golden.error().message);
        return comparison;
    }
    if (QtRocket::Test::selectedIndex(rocket) != golden->configurationIndex)
    {
        comparison.fail(
            input, std::format("the selected configuration is number {}, Java's {}",
                               QtRocket::Test::selectedIndex(rocket), golden->configurationIndex));
        return comparison;
    }
    const GoldenPlacements placements = goldenPlacements(input, golden->configurationIndex);
    const std::map<const RocketComponent*, int> indices = treeIndices(rocket);
    for (const GoldenFigureView& view : golden->views)
    {
        compareView(view, rocket.getSelectedConfiguration(), placements, indices, comparison);
    }
    return comparison;
}

/// Records the counts of @p comparison as properties of the running test.
void recordCounts(const FigureComparison& comparison)
{
    ::testing::Test::RecordProperty("instances", comparison.instances);
    ::testing::Test::RecordProperty("shapes", comparison.shapes);
    ::testing::Test::RecordProperty("exactShapes", comparison.exactShapes);
    ::testing::Test::RecordProperty("motors", comparison.motors);
    ::testing::Test::RecordProperty("exactMotors", comparison.exactMotors);
}

// ===================================================================================== tests

class FigureGoldenExamples : public ::testing::TestWithParam<QtRocket::Test::GoldenExample>
{ };

TEST_P(FigureGoldenExamples, AsOpenRocket)
{
    const QtRocket::Test::GoldenExample&             example = GetParam();
    const QtRocket::Result<QtRocket::LoadedDocument> loaded =
        QtRocket::Test::loadSettledGoldenExample(example);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    const FigureComparison comparison = compareFigure(loaded->document->getRocket(), example.name);
    recordCounts(comparison);
    EXPECT_EQ(comparison.report, "");
    EXPECT_GT(comparison.shapes, 0);
}

INSTANTIATE_TEST_SUITE_P(Examples, FigureGoldenExamples,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

class FigureGoldenTestRockets : public ::testing::TestWithParam<QtRocket::Test::TestRocketMaker>
{ };

TEST_P(FigureGoldenTestRockets, AsOpenRocket)
{
    const QtRocket::Test::TestRocketMaker& maker   = GetParam();
    const std::unique_ptr<Rocket>          rocket  = maker.make();
    const QtRocket::Result<int>            settled = QtRocket::settleAutomaticDimensions(*rocket);
    ASSERT_TRUE(settled.has_value()) << settled.error().message;
    const FigureComparison comparison = compareFigure(*rocket, maker.input);
    recordCounts(comparison);
    EXPECT_EQ(comparison.report, "");
    EXPECT_GT(comparison.shapes, 0);
}

/// The test name of @p maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(
    const ::testing::TestParamInfo<QtRocket::Test::TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, FigureGoldenTestRockets,
                         ::testing::ValuesIn(QtRocket::Test::testRocketMakers()), makerTestName);

// ================================================================ the files and the comparisons

/// The views of every figure.json (FigureDumper.VIEWS): the view and the rotation.
constexpr std::array<std::pair<FigureView, double>, 5> kGoldenViews{{
    {FigureView::SIDE, 0.0},
    {FigureView::TOP, 0.0},
    {FigureView::BACK, 0.0},
    {FigureView::SIDE, std::numbers::pi / 6},
    {FigureView::SIDE, 5 * std::numbers::pi / 4},
}};

/// What is wrong with the golden figure of @p input: the documented settings and views, each
/// with a figure rotation that is QtRocket's within kTranscendentalTolerance; empty when nothing.
[[nodiscard]] std::string goldenFigureProblems(const std::string& input)
{
    const QtRocket::Result<GoldenFigure> figure = QtRocket::Test::loadGoldenFigure(input);
    if (!figure)
    {
        return figure.error().message;
    }
    std::string problems;
    if (figure->input != input || figure->showMarkers || !figure->selection.empty() ||
        figure->views.size() != kGoldenViews.size())
    {
        problems += std::format("{}: another input, markers, selection or views\n", input);
        return problems;
    }
    for (std::size_t i = 0; i < kGoldenViews.size(); ++i)
    {
        const GoldenFigureView& view = figure->views[i];
        if (view.view != kGoldenViews.at(i).first || view.rotation != kGoldenViews.at(i).second ||
            !nearTransformation(QtRocket::figureRotation(view.view, view.rotation),
                                view.figureRotation))
        {
            problems += std::format("{}: view {} is not the documented one\n", input, i);
        }
    }
    return problems;
}

TEST(FigureGoldenSchema, EveryDesignHasItsFigureInTheFiveViews)
{
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().message;
    int         figures = 0;
    std::string problems;
    for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
    {
        // The inputs of both kinds have a design to draw.
        if (input.figure != input.name + "/figure.json")
        {
            problems +=
                std::format("{}: the manifest names the figure {}\n", input.name, input.figure);
            continue;
        }
        problems += goldenFigureProblems(input.name);
        ++figures;
    }
    EXPECT_EQ(problems, "");
    EXPECT_EQ(figures, 29);  // the 16 examples and the 13 test rockets
}

TEST(FigureGoldenComparison, ParsesEveryPrimitive)
{
    const json                            shapes = json::parse(R"([
        {"type": "rect", "frame": [1.0, 2.0, 3.0, 4.0], "visible": true},
        {"type": "ellipse", "frame": [1.0, 2.0, 3.0, 4.0], "visible": true},
        {"type": "roundRect", "frame": [1.0, 2.0, 3.0, 4.0], "arc": [0.5, 0.25], "visible": true},
        {"type": "arc", "frame": [1.0, 2.0, 3.0, 4.0], "angles": [55.0, 180.0], "arcType": "chord",
         "visible": true},
        {"type": "line", "points": [1.0, 2.0, 3.0, 4.0], "visible": true},
        {"type": "path", "precision": "float", "winding": "evenOdd", "segments": "ML2Z",
         "coordinates": [0.0, 0.0, 1.0, 0.0, 0.10000000149011612, 1.0], "visible": true}
    ])");
    const std::array<std::string_view, 6> structures{
        "rect", "ellipse", "roundRect", "arc CHORD", "line", "path FLOAT EVEN_ODD ML2Z"};
    for (std::size_t i = 0; i < structures.size(); ++i)
    {
        const QtRocket::Result<FigureShape> shape =
            QtRocket::Test::parseGoldenFigureShape(shapes.at(i));
        ASSERT_TRUE(shape.has_value()) << shape.error().message;
        EXPECT_EQ(QtRocket::Test::figureShapeValues(*shape).structure, structures.at(i));
    }
    const QtRocket::Result<FigureShape> path = QtRocket::Test::parseGoldenFigureShape(shapes.at(5));
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(QtRocket::Test::figureShapeValues(*path).values,
              (std::vector<double>{0.0, 0.0, 1.0, 0.0, 0.10000000149011612, 1.0}));
}

TEST(FigureGoldenComparison, RefusesMalformedShapes)
{
    for (const char* text :
         {R"({"type": "square", "frame": [1.0, 2.0, 3.0, 4.0]})",
          R"({"type": "rect", "frame": [1.0]})",
          R"({"type": "arc", "frame": [1.0, 2.0, 3.0, 4.0], "angles": [1.0, 2.0], "arcType": "x"})",
          R"({"type": "path", "precision": "float", "winding": "nonZero", "segments": "LZ",
              "coordinates": [1.0, 2.0]})",
          R"({"type": "path", "precision": "double", "winding": "nonZero", "segments": "ML",
              "coordinates": [1.0, 2.0, 3.0]})",
          R"({"type": "path", "precision": "half", "winding": "nonZero", "segments": "M",
              "coordinates": [1.0, 2.0]})"})
    {
        EXPECT_FALSE(QtRocket::Test::parseGoldenFigureShape(json::parse(text)).has_value()) << text;
    }
}

TEST(FigureGoldenComparison, ExactUnlessGivenATolerance)
{
    const FigureShape a{QtRocket::FigureRect{0.1, 0.0, 0.3, 0.025}};
    const FigureShape b{QtRocket::FigureRect{0.1, -0.0, 0.3, 0.025}};
    const FigureShape c{QtRocket::FigureRect{0.1, 0.0, std::nextafter(0.3, 1.0), 0.025}};
    const FigureShape e{QtRocket::FigureEllipse{0.1, 0.0, 0.3, 0.025}};
    EXPECT_EQ(figureShapeDifference(a, b, 0), std::nullopt);  // zeros of either sign
    EXPECT_NE(figureShapeDifference(a, c, 0), std::nullopt);
    EXPECT_EQ(figureShapeDifference(a, c, kTranscendentalTolerance), std::nullopt);
    EXPECT_NE(figureShapeDifference(a, e, kTranscendentalTolerance), std::nullopt);
}

/// A deployment charge of length 0.06 m and radius 0.012 m.
[[nodiscard]] QtRocket::MassComponent deploymentCharge()
{
    QtRocket::MassComponent charge;
    charge.setMassComponentType(QtRocket::MassComponent::MassComponentType::DEPLOYMENTCHARGE);
    charge.setLength(0.06);
    charge.setRadius(0.012);
    return charge;
}

/// @p rays as a deployment charge's ray path: from @p centre to centre + each offset.
[[nodiscard]] FigureShape raysFrom(QtRocket::Point2D                  centre,
                                   std::span<const QtRocket::Point2D> rays)
{
    FigurePath path{FigurePath::Precision::DOUBLE};
    for (const QtRocket::Point2D& ray : rays)
    {
        path.moveTo(centre.x, centre.y);
        path.lineTo(centre.x + ray.x, centre.y + ray.y);
    }
    return FigureShape{std::move(path)};
}

/// @p count ray offsets within the range of rays of length @p reach (the first of length 0, the
/// last nearly at the end), but for ray @p outlier, which ends beyond it.
[[nodiscard]] std::vector<QtRocket::Point2D> raysWithin(double reach, std::size_t count,
                                                        std::size_t outlier)
{
    std::vector<QtRocket::Point2D> rays;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i == outlier)
        {
            rays.emplace_back(1.01 * reach, 0);
        }
        else if (i == 0)
        {
            rays.emplace_back(0, 0);
        }
        else if (i + 1 == count)
        {
            rays.emplace_back(0.999 * reach, 0.5 * reach);
        }
        else
        {
            rays.emplace_back(0.25 * reach, 0.75 * reach);
        }
    }
    return rays;
}

TEST(FigureGoldenComparison, DeploymentChargeRaysByCountAndRange)
{
    const QtRocket::MassComponent     charge = deploymentCharge();
    const std::vector<ComponentShape> shapes =
        QtRocket::MassComponentShapes::getShapesSide(charge, Transformation{});
    ASSERT_EQ(shapes.size(), 3U);
    const Rectangle2D frame = shapes[0].shape().bounds();
    // The symbol's centre and maximal ray length, as Java computes them.
    const QtRocket::Point2D centre{frame.x + (frame.width / 2.0), frame.y + (frame.height / 2.0)};
    const double            reach = std::min(frame.width, frame.height) / 4.0;

    // Other random rays, as another paint in Java draws them: as good as any.
    constexpr auto kRays =
        static_cast<std::size_t>(QtRocket::MassComponentShapes::kDeploymentChargeRays);
    const FigureShape java = raysFrom(centre, raysWithin(reach, kRays, kRays));
    EXPECT_EQ(QtRocket::Test::deploymentChargeRaysDifference(shapes[2].shape(), java, frame,
                                                             kTranscendentalTolerance),
              std::nullopt);

    // A ray beyond the range, and one ray too few.
    EXPECT_NE(
        QtRocket::Test::deploymentChargeRaysDifference(
            raysFrom(centre, raysWithin(reach, kRays, 3)), java, frame, kTranscendentalTolerance),
        std::nullopt);
    EXPECT_NE(QtRocket::Test::deploymentChargeRaysDifference(
                  raysFrom(centre, raysWithin(reach, kRays - 1, kRays)), java, frame,
                  kTranscendentalTolerance),
              std::nullopt);
}

}  // namespace
