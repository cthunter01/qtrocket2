#pragma once

// Comparisons of what QtRocket's shape providers return with the shapes OpenRocket's providers
// returned, as the generated pin tables hold them (ShapePins.h): the pinned primitives as
// FigureShapes, the pinned transformations, and assertions that name the first difference.
// Test-only.

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePins.h"

namespace QtRocket::Test
{

/// The pinned transformation, exactly.
[[nodiscard]] inline Transformation pinnedTransformation(const TransformPin& pin)
{
    const Transformation::Matrix3 matrix{
        {{pin[0], pin[1], pin[2]}, {pin[3], pin[4], pin[5]}, {pin[6], pin[7], pin[8]}}};
    return Transformation{matrix, Coordinate{pin[9], pin[10], pin[11], pin[12]}};
}

/// The segment types of a pinned path, from its letters: a letter (M, L, Q, C or Z) followed by
/// how many times it repeats when more than once ("ML3Z" is a move, three lines and a close).
[[nodiscard]] inline std::vector<PathSegment::Type> pinnedSegmentTypes(std::string_view letters)
{
    std::vector<PathSegment::Type> types;
    std::size_t                    i = 0;
    while (i < letters.size())
    {
        const char  letter = letters[i++];
        std::size_t count  = 0;
        while (i < letters.size() && letters[i] >= '0' && letters[i] <= '9')
        {
            count = (count * 10) + static_cast<std::size_t>(letters[i++] - '0');
        }
        PathSegment::Type type = PathSegment::Type::MOVE_TO;
        switch (letter)
        {
            case 'M':
                type = PathSegment::Type::MOVE_TO;
                break;
            case 'L':
                type = PathSegment::Type::LINE_TO;
                break;
            case 'Q':
                type = PathSegment::Type::QUAD_TO;
                break;
            case 'C':
                type = PathSegment::Type::CUBIC_TO;
                break;
            case 'Z':
                type = PathSegment::Type::CLOSE;
                break;
            default:
                bug(std::format("no segment type {}", letter));
        }
        types.insert(types.end(), count == 0 ? 1 : count, type);
    }
    return types;
}

/// The pinned path, rebuilt with FigurePath's own calls (a Path2D.Float's pinned coordinates are
/// floats already, so storing them again changes nothing).
[[nodiscard]] inline FigurePath pinnedPath(const ShapePin& pin)
{
    FigurePath path{pin.type == PinShape::PATH_FLOAT ? FigurePath::Precision::FLOAT
                                                     : FigurePath::Precision::DOUBLE};
    const std::span<const double> v = pin.values;
    std::size_t                   k = 0;
    for (const PathSegment::Type type : pinnedSegmentTypes(pin.segments))
    {
        switch (type)
        {
            case PathSegment::Type::MOVE_TO:
                path.moveTo(v[k], v[k + 1]);
                k += 2;
                break;
            case PathSegment::Type::LINE_TO:
                path.lineTo(v[k], v[k + 1]);
                k += 2;
                break;
            case PathSegment::Type::QUAD_TO:
                path.quadTo(v[k], v[k + 1], v[k + 2], v[k + 3]);
                k += 4;
                break;
            case PathSegment::Type::CUBIC_TO:
                path.curveTo(v[k], v[k + 1], v[k + 2], v[k + 3], v[k + 4], v[k + 5]);
                k += 6;
                break;
            case PathSegment::Type::CLOSE:
                path.closePath();
                break;
        }
    }
    QTROCKET_ASSERT(k == v.size());
    return path;
}

/// The pinned shape as a FigureShape.
[[nodiscard]] inline FigureShape pinnedShape(const ShapePin& pin)
{
    const std::span<const double> v = pin.values;
    switch (pin.type)
    {
        case PinShape::RECT:
            return FigureShape{FigureRect{v[0], v[1], v[2], v[3]}};
        case PinShape::ELLIPSE:
            return FigureShape{FigureEllipse{v[0], v[1], v[2], v[3]}};
        case PinShape::LINE:
            return FigureShape{FigureLine{v[0], v[1], v[2], v[3]}};
        case PinShape::PATH_DOUBLE:
        case PinShape::PATH_FLOAT:
            return FigureShape{pinnedPath(pin)};
        case PinShape::ROUND_RECT:
            return FigureShape{FigureRoundRect{v[0], v[1], v[2], v[3], v[4], v[5]}};
        case PinShape::ARC_OPEN:
            return FigureShape{
                FigureArc{v[0], v[1], v[2], v[3], v[4], v[5], FigureArc::Type::OPEN}};
        case PinShape::ARC_CHORD:
            return FigureShape{
                FigureArc{v[0], v[1], v[2], v[3], v[4], v[5], FigureArc::Type::CHORD}};
        case PinShape::ARC_PIE:
            return FigureShape{FigureArc{v[0], v[1], v[2], v[3], v[4], v[5], FigureArc::Type::PIE}};
    }
    QTROCKET_UNREACHABLE();
}

namespace Detail
{

/// The parameters of a rectangle, an ellipse or a line (four), of a rounded rectangle or an arc
/// (six, the arc's type aside), or nothing.
[[nodiscard]] inline std::vector<double> primitiveParameters(const FigureShape& shape)
{
    if (const auto* r = shape.getIf<FigureRoundRect>())
    {
        return {r->x(), r->y(), r->width(), r->height(), r->arcWidth(), r->arcHeight()};
    }
    if (const auto* a = shape.getIf<FigureArc>())
    {
        return {a->x(), a->y(), a->width(), a->height(), a->angleStart(), a->angleExtent()};
    }
    if (const auto* r = shape.getIf<FigureRect>())
    {
        return {r->x(), r->y(), r->width(), r->height()};
    }
    if (const auto* e = shape.getIf<FigureEllipse>())
    {
        return {e->x(), e->y(), e->width(), e->height()};
    }
    if (const auto* l = shape.getIf<FigureLine>())
    {
        return {l->x1(), l->y1(), l->x2(), l->y2()};
    }
    return {};
}

/// "(a, b, ...)" of @p values.
[[nodiscard]] inline std::string joinedParameters(const std::vector<double>& values)
{
    std::string text = "(";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        text += std::format("{}{}", i == 0 ? "" : ", ", values[i]);
    }
    return text + ")";
}

}  // namespace Detail

/// Success when @p actual is the pinned shape: the same primitive with every parameter (of a path:
/// the same precision, rule and segment types, and every coordinate) within @p tolerance of the
/// pinned one (figureValueNear(); a tolerance of 0 asks for equal values).
[[nodiscard]] inline ::testing::AssertionResult shapeMatchesPin(const FigureShape& actual,
                                                                const ShapePin&    pin,
                                                                double             tolerance = 0)
{
    const FigureShape expected = pinnedShape(pin);
    if (actual.type() != expected.type())
    {
        return ::testing::AssertionFailure()
               << "another primitive: type " << static_cast<int>(actual.type()) << ", expected "
               << static_cast<int>(expected.type());
    }
    if (const auto* path = actual.getIf<FigurePath>())
    {
        const auto* expectedPath = expected.getIf<FigurePath>();
        if (expectedPath == nullptr)
        {
            bug("a pinned path that is no path");
        }
        if (path->precision() != expectedPath->precision() ||
            path->windingRule() != expectedPath->windingRule())
        {
            return ::testing::AssertionFailure() << "another path precision or winding rule";
        }
        return segmentsNear(path->segments(), expectedPath->segments(), tolerance);
    }
    if (const auto* arc = actual.getIf<FigureArc>())
    {
        const auto* expectedArc = expected.getIf<FigureArc>();
        if (expectedArc == nullptr || arc->type() != expectedArc->type())
        {
            return ::testing::AssertionFailure() << "another arc type";
        }
    }
    const std::vector<double> a = Detail::primitiveParameters(actual);
    const std::vector<double> e = Detail::primitiveParameters(expected);
    for (std::size_t i = 0; i < e.size(); ++i)
    {
        if (!figureValueNear(a[i], e[i], tolerance))
        {
            return ::testing::AssertionFailure()
                   << Detail::joinedParameters(a) << ", expected " << Detail::joinedParameters(e);
        }
    }
    return ::testing::AssertionSuccess();
}

/// Whether @p shape is a shape of @p component as @p pin says: visible with the component's colour,
/// or invisible with Color::invisible(), and the component's line style either way.
[[nodiscard]] inline bool shapeOfComponentAsPinned(const ComponentShape&  shape,
                                                   const RocketComponent& component,
                                                   const ShapePin&        pin)
{
    if (&shape.component() != &component || shape.isVisible() != pin.visible ||
        shape.lineStyle() != component.getLineStyle())
    {
        return false;
    }
    return pin.visible ? shape.color() == component.getColor()
                       : shape.color() == Color::invisible();
}

/// Success when @p actual are the pinned shapes, in order (shapeMatchesPin()), each a shape of
/// @p component with its line style, visible with its colour or, where the pin says so,
/// invisible.
[[nodiscard]] inline ::testing::AssertionResult shapesMatchPins(
    std::span<const ComponentShape> actual, std::span<const ShapePin> pins,
    const RocketComponent& component, double tolerance = 0)
{
    if (actual.size() != pins.size())
    {
        return ::testing::AssertionFailure()
               << actual.size() << " shapes, expected " << pins.size();
    }
    for (std::size_t i = 0; i < pins.size(); ++i)
    {
        const ComponentShape& shape = actual[i];
        if (!shapeOfComponentAsPinned(shape, component, pins[i]))
        {
            return ::testing::AssertionFailure()
                   << "shape " << i << " is not a " << (pins[i].visible ? "visible" : "invisible")
                   << " shape of " << component.toString();
        }
        const ::testing::AssertionResult result =
            shapeMatchesPin(shape.shape(), pins[i], tolerance);
        if (!result)
        {
            return ::testing::AssertionFailure() << "shape " << i << ": " << result.message();
        }
    }
    return ::testing::AssertionSuccess();
}

/// Success when @p actual are the pinned shapes, in order, as EmptyShapes returns them.
[[nodiscard]] inline ::testing::AssertionResult figureShapesMatchPins(
    std::span<const FigureShape> actual, std::span<const ShapePin> pins)
{
    if (actual.size() != pins.size())
    {
        return ::testing::AssertionFailure()
               << actual.size() << " shapes, expected " << pins.size();
    }
    for (std::size_t i = 0; i < pins.size(); ++i)
    {
        const ::testing::AssertionResult result = shapeMatchesPin(actual[i], pins[i]);
        if (!result)
        {
            return ::testing::AssertionFailure() << "shape " << i << ": " << result.message();
        }
    }
    return ::testing::AssertionSuccess();
}

/// The path of the only shape of @p shapes, which must outlive it; a BugError when the list holds
/// anything else.
[[nodiscard]] inline const FigurePath& onlyPathOf(const std::vector<ComponentShape>& shapes)
{
    const FigurePath* path = shapes.size() == 1 ? shapes[0].shape().getIf<FigurePath>() : nullptr;
    if (path == nullptr)
    {
        bug(std::format("{} shapes, not one path", shapes.size()));
    }
    return *path;
}

/// The pin of the hand-built case @p name in @p view; a BugError when there is none (a test
/// asks only for the cases the generator wrote).
[[nodiscard]] inline const HandBuiltPin& handBuiltPin(std::span<const HandBuiltPin> pins,
                                                      std::string_view name, FigureView view)
{
    for (const HandBuiltPin& pin : pins)
    {
        if (pin.name == name && pin.view == view)
        {
            return pin;
        }
    }
    bug(std::format("no hand-built pin {} in the {} view", name, figureViewName(view)));
}

/// A provider's getShapesSide or getShapesBack.
using ShapeFunction = std::vector<ComponentShape> (*)(const RocketComponent&,
                                                      const Transformation&);

/// Success when @p side (for a SIDE pin) or @p back (for a BACK pin) gives for @p component, under
/// the transformation of the hand-built case @p name in @p view, the case's pinned shapes
/// (shapesMatchPins() with @p tolerance).
[[nodiscard]] inline ::testing::AssertionResult handBuiltAsJava(
    ShapeFunction side, ShapeFunction back, const RocketComponent& component,
    std::span<const HandBuiltPin> pins, std::string_view name, FigureView view,
    double tolerance = 0)
{
    const HandBuiltPin&               pin = handBuiltPin(pins, name, view);
    const Transformation              t   = pinnedTransformation(pin.transform);
    const std::vector<ComponentShape> shapes =
        usesSideShapes(view) ? side(component, t) : back(component, t);
    const ::testing::AssertionResult result =
        shapesMatchPins(shapes, pin.shapes, component, tolerance);
    if (!result)
    {
        return ::testing::AssertionFailure()
               << name << " " << figureViewName(view) << ": " << result.message();
    }
    return ::testing::AssertionSuccess();
}

/// handBuiltAsJava() with @p tolerance for every pin of the case @p name ("<name>") or of its
/// variants ("<name>/<variant>": "/axial", "/off", ...), in whichever views the generator pinned;
/// a failure when there is none.
[[nodiscard]] inline ::testing::AssertionResult handBuiltVariantsAsJava(
    ShapeFunction side, ShapeFunction back, const RocketComponent& component,
    std::span<const HandBuiltPin> pins, std::string_view name, double tolerance = 0)
{
    int checked = 0;
    for (const HandBuiltPin& pin : pins)
    {
        const bool variant = pin.name.size() > name.size() && pin.name.starts_with(name) &&
                             pin.name[name.size()] == '/';
        if (pin.name != name && !variant)
        {
            continue;
        }
        const ::testing::AssertionResult result =
            handBuiltAsJava(side, back, component, pins, pin.name, pin.view, tolerance);
        if (!result)
        {
            return result;
        }
        ++checked;
    }
    if (checked == 0)
    {
        return ::testing::AssertionFailure() << "no pins of " << name;
    }
    return ::testing::AssertionSuccess();
}

/// handBuiltAsJava() for the side and back views of the cases "<name>/axial" (translated along
/// the axis) and "<name>/off" (rotated and off the axis).
[[nodiscard]] inline ::testing::AssertionResult handBuiltCasesAsJava(
    ShapeFunction side, ShapeFunction back, const RocketComponent& component,
    std::span<const HandBuiltPin> pins, std::string_view name)
{
    for (const std::string_view suffix : {std::string_view{"/axial"}, std::string_view{"/off"}})
    {
        const std::string caseName = std::string{name} + std::string{suffix};
        for (const FigureView view : {FigureView::SIDE, FigureView::BACK})
        {
            const ::testing::AssertionResult result =
                handBuiltAsJava(side, back, component, pins, caseName, view);
            if (!result)
            {
                return result;
            }
        }
    }
    return ::testing::AssertionSuccess();
}

}  // namespace QtRocket::Test
