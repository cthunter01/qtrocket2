#pragma once

// Reading the figure golden data (tests/data/goldens/<input>/figure.json, written by the
// FigureDumper of tools/openrocket-goldens; the format is in that tool's README.md): what
// OpenRocket's 2D rocket figure draws of a design's selected flight configuration in five views,
// every shape turned into the QtRocket FigureShape of the same primitive and parameters (a path
// with Java's segments, precision and winding rule), and comparing two shapes value by value.
// Test-only: figure_golden_tests.cpp compares QtRocket's RocketFigureGeometry with these files.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::Test
{

/// One shape of an instance: the primitive, and whether it is drawn (false for the shapes
/// OpenRocket gives the colour ORColor.INVISIBLE: the selection areas of rail buttons and of
/// the markers of pod sets and booster sets).
struct GoldenFigureShape
{
    FigureShape shape;
    bool        visible{true};
};

/// One instance of a component in a view: the shapes the provider gave for it, in its order.
struct GoldenFigureInstance
{
    std::string path;  ///< the component's golden path ("/0/1")
    /// Whether it is one of FlightConfiguration.getExtraRenderInstances().
    bool extraRender{false};
    int  context{0};         ///< its index among the component's InstanceContexts
    int  instanceNumber{0};  ///< InstanceContext.instanceNumber
    /// The order RocketFigure draws it in: the component's side display order in the side and
    /// top views, its back display order in the back view (the highest first).
    int displayOrder{0};
    /// The component's colour and line style (getColor(), getLineStyle(); nullopt for the
    /// defaults), which every visible shape of it carries.
    std::optional<Color>           color;
    std::optional<LineStyle>       lineStyle;
    std::vector<GoldenFigureShape> shapes;
};

/// One motor of a view (RocketFigure.drawMotors): a Rect in the side and top views, an Ellipse
/// in the back view.
struct GoldenFigureMotor
{
    std::string mount;        ///< the golden path of the motor mount
    int         location{0};  ///< the index of the mount's component location it is drawn at
    FigureShape shape;
};

/// One view of the figure.
struct GoldenFigureView
{
    FigureView view{FigureView::SIDE};
    double     rotation{0};  ///< about the rocket's axis, in radians
    /// Java's RocketFigure.getFigureRotation() of the view (its sines and cosines).
    Transformation figureRotation;
    Rectangle2D    subjectBounds;
    /// The instances in the order of the dump: the active instances, then the extra render
    /// instances, each in the tree order of their components, a component's in context order.
    std::vector<GoldenFigureInstance> instances;
    /// The motors, in the tree order of their mounts, a mount's in location order.
    std::vector<GoldenFigureMotor> motors;
};

/// One figure.json document.
struct GoldenFigure
{
    std::string input;
    /// The flight configuration the figure shows (the rocket's selected one): its index among
    /// the rocket's configurations, the default first, and its id.
    int         configurationIndex{0};
    std::string configurationId;
    /// RocketFigure's preference "only show pod set/booster markers when selected".
    bool showMarkers{false};
    /// The golden paths of the selected components (RocketFigure's selection).
    std::vector<std::string>      selection;
    std::vector<GoldenFigureView> views;
};

/// The FigureShape a golden shape object describes ({"type": "rect", "frame": [...]}, ...).
/// Fails with ErrorCode::PARSE on an unknown type or a missing or malformed parameter, and on a
/// path whose segments do not start with a move.
[[nodiscard]] Result<FigureShape> parseGoldenFigureShape(const nlohmann::json& shape);

/// Interprets a parsed figure.json document (ErrorCode::PARSE when a required field is missing
/// or has the wrong type, or the schema is not "figure" of kGoldenSchemaVersion).
[[nodiscard]] Result<GoldenFigure> parseGoldenFigure(const nlohmann::json& figure);

/// The figure.json of the golden input @p name, found through manifest.json ("figure" of the
/// input). Fails as loadGoldenJson() does, with ErrorCode::NOT_FOUND when the manifest names no
/// figure for the input, and as parseGoldenFigure() does.
[[nodiscard]] Result<GoldenFigure> loadGoldenFigure(std::string_view name);

/// The parameters of @p shape that a comparison holds against another shape's.
struct FigureShapeValues
{
    /// What has to be equal: the primitive and its kind ("rect", "arc CHORD", "path FLOAT
    /// NON_ZERO ML182Z").
    std::string structure;
    /// The numbers, in a fixed order: x, y, width and height of a Rect, Ellipse, RoundRect or
    /// Arc, then a RoundRect's arc width and height or an Arc's start and extent in degrees; a
    /// Line's x1, y1, x2, y2; a path's coordinates, point by point.
    std::vector<double> values;
};

/// The structure and values of @p shape.
[[nodiscard]] FigureShapeValues figureShapeValues(const FigureShape& shape);

/// Whether @p actual is @p expected: the same structure and, value by value, equal when
/// @p tolerance is 0 (==, so that a zero matches a zero of either sign), else within
/// @p tolerance relative to max(1, |expected|). nullopt when it is, else what differs (the first
/// value that does, with its index, or the two structures).
[[nodiscard]] std::optional<std::string> figureShapeDifference(const FigureShape& actual,
                                                               const FigureShape& expected,
                                                               double             tolerance);

/// The difference of the value @p actual from @p expected that figureShapeDifference() allows
/// with @p tolerance, as there: true when it is within it.
[[nodiscard]] bool figureValueMatches(double actual, double expected, double tolerance);

/// The rays of a deployment charge (the third shape of MassComponentShapes' side view, a path)
/// against Java's @p expected, by their number and range only: Java draws them from an unseeded
/// java.util.Random on every paint, QtRocket from a generator seeded with the component's id
/// (decision U4). Both have to hold MassComponentShapes::kDeploymentChargeRays rays, each a move
/// and a line; every move of @p actual has to be at Java's centre, and every line has to end
/// within the square that reaches from it by Java's maximal ray length (a quarter of the smaller
/// side of @p frame, the bounds of the symbol's first shape), both within @p tolerance (as
/// figureValueMatches()). nullopt when they do, else what differs.
[[nodiscard]] std::optional<std::string> deploymentChargeRaysDifference(const FigureShape& actual,
                                                                        const FigureShape& expected,
                                                                        const Rectangle2D& frame,
                                                                        double tolerance);

}  // namespace QtRocket::Test
