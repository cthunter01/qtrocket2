#pragma once

// The types of the generated tables that pin what OpenRocket's shape providers return for
// components of the example designs and the test rockets and for components a Java probe builds
// (BodyShapePins.h, FinMassShapePins.h), and what its RocketFigure assembles for whole designs
// (FigureGeometryPins.h). Test-only.

#include <array>
#include <span>
#include <string_view>

#include "QtRocket/figure/FigureView.h"

namespace QtRocket::Test
{

/// What a shape is in Java: a Rectangle2D.Double, Ellipse2D.Double, Line2D.Double, Path2D.Double,
/// Path2D.Float, RoundRectangle2D.Double or Arc2D.Double (of the type Arc2D.OPEN, CHORD or PIE).
enum class PinShape
{
    RECT,
    ELLIPSE,
    LINE,
    PATH_DOUBLE,
    PATH_FLOAT,
    ROUND_RECT,
    ARC_OPEN,
    ARC_CHORD,
    ARC_PIE,
};

/// One shape: the four parameters of a rectangle or an ellipse (x, y, width, height) or of a line
/// (x1, y1, x2, y2); the six of a rounded rectangle (x, y, width, height, arc width, arc height)
/// or of an arc (x, y, width, height, start, extent); of a path, the points of its segments in
/// order, with one letter per segment (M, L, Q, C or Z) in @c segments, and the NON_ZERO winding
/// rule (Path2D's default, which every provider keeps). @c visible is false for a shape Java gives
/// the colour ORColor.INVISIBLE (only hit tests see it).
struct ShapePin
{
    constexpr ShapePin(PinShape pinType, std::span<const double> pinValues,
                       std::string_view pinSegments, bool pinVisible = true) noexcept
      : type(pinType), values(pinValues), segments(pinSegments), visible(pinVisible)
    {
    }

    PinShape                type;
    std::span<const double> values;
    std::string_view        segments;
    bool                    visible;
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

/// One instance in a figure of Java's RocketFigure (the probe's replica of its updateShapes): which
/// instance it is, the display order and number of its shapes, and its first shape's primitive and
/// bounds (Shape.getBounds2D(): x, y, width, height).
struct FigureInstancePin
{
    constexpr FigureInstancePin(int tree, bool extra, int context, int number, int order, int count,
                                PinShape                     firstType,
                                const std::array<double, 4>& firstBounds) noexcept
      : treeIndex(tree),
        extraRender(extra),
        contextIndex(context),
        instanceNumber(number),
        displayOrder(order),
        shapeCount(count),
        type(firstType),
        bounds(firstBounds)
    {
    }

    /// The component's index in the rocket's pre-order iteration (the rocket is 0).
    int treeIndex;
    /// Whether the instance is one of getExtraRenderInstances().
    bool extraRender;
    /// The instance's index among its component's contexts.
    int contextIndex;
    /// The context's instanceNumber.
    int instanceNumber;
    /// getDisplayOrder_side() or getDisplayOrder_back() of the component, by view.
    int displayOrder;
    /// How many shapes the provider returned.
    int                   shapeCount;
    PinShape              type;
    std::array<double, 4> bounds;
};

/// One motor of a figure (RocketFigure.drawMotors): the mount, the index of the mount's location,
/// and the shape (a RECT or an ELLIPSE: x, y, width, height).
struct FigureMotorPin
{
    constexpr FigureMotorPin(int mount, int location, PinShape motorType,
                             const std::array<double, 4>& motorValues) noexcept
      : mountTreeIndex(mount), locationIndex(location), type(motorType), values(motorValues)
    {
    }

    int                   mountTreeIndex;
    int                   locationIndex;
    PinShape              type;
    std::array<double, 4> values;
};

/// The figure of a design in a view (what RocketFigureGeometry builds): the subject bounds (x, y,
/// width, height), the instances sorted by (extra render, tree index, context index), and the
/// motors sorted by (mount, location).
struct FigurePin
{
    constexpr FigurePin(std::string_view designSource, std::string_view designName,
                        FigureView figureView, double figureRotation, bool markers, int selected,
                        const std::array<double, 4>&       subject,
                        std::span<const FigureInstancePin> figureInstances,
                        std::span<const FigureMotorPin>    figureMotors) noexcept
      : source(designSource),
        name(designName),
        view(figureView),
        rotation(figureRotation),
        showMarkers(markers),
        selectedTreeIndex(selected),
        subjectBounds(subject),
        instances(figureInstances),
        motors(figureMotors)
    {
    }

    /// "example", "testrocket", "hand-built" (a rocket the probe builds) or "new" (a new design).
    std::string_view source;
    std::string_view name;
    FigureView       view;
    /// The figure's rotation (radians).
    double rotation;
    /// The preference ShowMarkers (only the selected pod sets and booster sets show markers).
    bool showMarkers;
    /// The tree index of the one selected component, or -1 for no selection.
    int                                selectedTreeIndex;
    std::array<double, 4>              subjectBounds;
    std::span<const FigureInstancePin> instances;
    std::span<const FigureMotorPin>    motors;
};

}  // namespace QtRocket::Test
