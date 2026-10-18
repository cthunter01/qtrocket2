#pragma once

// The types of the generated tables that pin what OpenRocket's shape providers return for
// components of the example designs and the test rockets and for components a Java probe builds
// (BodyShapePins.h). Test-only.

#include <array>
#include <span>
#include <string_view>

#include "QtRocket/figure/FigureView.h"

namespace QtRocket::Test
{

/// What a shape is in Java: a Rectangle2D.Double, Ellipse2D.Double, Line2D.Double, Path2D.Double
/// or Path2D.Float.
enum class PinShape
{
    RECT,
    ELLIPSE,
    LINE,
    PATH_DOUBLE,
    PATH_FLOAT,
};

/// One shape: the four parameters of a rectangle or an ellipse (x, y, width, height) or of a line
/// (x1, y1, x2, y2); of a path, the points of its segments in order, with one letter per segment
/// (M, L, Q, C or Z) in @c segments, and the NON_ZERO winding rule (Path2D's default, which every
/// provider keeps).
struct ShapePin
{
    constexpr ShapePin(PinShape pinType, std::span<const double> pinValues,
                       std::string_view pinSegments) noexcept
      : type(pinType), values(pinValues), segments(pinSegments)
    {
    }

    PinShape                type;
    std::span<const double> values;
    std::string_view        segments;
};

/// A transformation: its matrix, rows first, then its translation x, y, z and the translation's
/// weight.
using TransformPin = std::array<double, 13>;

/// What the provider returned for one instance of a component of a design in one view: the
/// transformation RocketFigure handed it (the figure's rotation applied after the instance's) and
/// the shapes.
struct InstancePin
{
    constexpr InstancePin(int tree, int context, int number, FigureView pinView, double pinRotation,
                          const TransformPin&       pinTransform,
                          std::span<const ShapePin> pinShapes) noexcept
      : treeIndex(tree),
        contextIndex(context),
        instanceNumber(number),
        view(pinView),
        rotation(pinRotation),
        transform(pinTransform),
        shapes(pinShapes)
    {
    }

    /// The component's index in the rocket's pre-order iteration (the rocket is 0).
    int treeIndex;
    /// The instance's index in getActiveInstances().getInstanceContexts(component).
    int contextIndex;
    /// The context's instanceNumber.
    int        instanceNumber;
    FigureView view;
    /// The figure's rotation (radians).
    double                    rotation;
    TransformPin              transform;
    std::span<const ShapePin> shapes;
};

/// The pinned components of one design.
struct DesignPin
{
    constexpr DesignPin(std::string_view designSource, std::string_view designName,
                        std::span<const InstancePin> designInstances) noexcept
      : source(designSource), name(designName), instances(designInstances)
    {
    }

    /// "example" (a file of data/examples) or "testrocket" (a maker of TestRockets).
    std::string_view source;
    /// The file name ("A simple model rocket.ork") or the maker's name ("makeBeta").
    std::string_view             name;
    std::span<const InstancePin> instances;
};

/// What a provider returned for a component the probe built, under a transformation of its own.
struct HandBuiltPin
{
    constexpr HandBuiltPin(std::string_view caseName, FigureView caseView, double caseScale,
                           const TransformPin&       caseTransform,
                           std::span<const ShapePin> caseShapes) noexcept
      : name(caseName),
        view(caseView),
        scale(caseScale),
        transform(caseTransform),
        shapes(caseShapes)
    {
    }

    /// The case, as the probe names it ("ring-bulkhead/off").
    std::string_view name;
    /// SIDE for the side shapes, BACK for the back shapes.
    FigureView view;
    /// The scale factor (1 but for the printed nose cone).
    double                    scale;
    TransformPin              transform;
    std::span<const ShapePin> shapes;
};

}  // namespace QtRocket::Test
