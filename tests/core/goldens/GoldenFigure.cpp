#include "goldens/GoldenFigure.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/MassComponentShapes.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Transformation.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// A PARSE failure: "@p context: @p what".
[[nodiscard]] std::unexpected<Error> parseFailure(std::string_view context, std::string_view what)
{
    return fail(ErrorCode::PARSE, std::format("{}: {}", context, what));
}

/// The member @p key of the object @p object.
[[nodiscard]] Result<const json*> member(const json& object, std::string_view key,
                                         std::string_view context)
{
    if (!object.is_object())
    {
        return parseFailure(context, "not an object");
    }
    const auto found = object.find(key);
    if (found == object.end())
    {
        return parseFailure(context, std::format(R"(missing "{}")", key));
    }
    return &*found;
}

[[nodiscard]] Result<std::string> stringMember(const json& object, std::string_view key,
                                               std::string_view context)
{
    const auto value = member(object, key, context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (!(*value)->is_string())
    {
        return parseFailure(context, std::format(R"("{}" is not a string)", key));
    }
    return (*value)->get<std::string>();
}

[[nodiscard]] Result<int> integerMember(const json& object, std::string_view key,
                                        std::string_view context)
{
    const auto value = member(object, key, context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (!(*value)->is_number_integer())
    {
        return parseFailure(context, std::format(R"("{}" is not an integer)", key));
    }
    return (*value)->get<int>();
}

[[nodiscard]] Result<bool> booleanMember(const json& object, std::string_view key,
                                         std::string_view context)
{
    const auto value = member(object, key, context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (!(*value)->is_boolean())
    {
        return parseFailure(context, std::format(R"("{}" is not a boolean)", key));
    }
    return (*value)->get<bool>();
}

[[nodiscard]] Result<double> numberMember(const json& object, std::string_view key,
                                          std::string_view context)
{
    const auto value = member(object, key, context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    const std::optional<double> number = goldenNumber(**value);
    if (!number)
    {
        return parseFailure(context, std::format(R"("{}" is not a number)", key));
    }
    return *number;
}

/// The array member @p key.
[[nodiscard]] Result<const json*> arrayMember(const json& object, std::string_view key,
                                              std::string_view context)
{
    const auto value = member(object, key, context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if (!(*value)->is_array())
    {
        return parseFailure(context, std::format(R"("{}" is not an array)", key));
    }
    return *value;
}

/// The list of numbers @p key: exactly @p count of them, or any number when @p count is 0.
[[nodiscard]] Result<std::vector<double>> numbersMember(const json& object, std::string_view key,
                                                        std::size_t count, std::string_view context)
{
    const auto list = arrayMember(object, key, context);
    if (!list)
    {
        return std::unexpected(list.error());
    }
    if (count != 0 && (*list)->size() != count)
    {
        return parseFailure(
            context, std::format(R"("{}" has {} numbers, not {})", key, (*list)->size(), count));
    }
    std::vector<double> numbers;
    numbers.reserve((*list)->size());
    for (const json& element : **list)
    {
        const std::optional<double> number = goldenNumber(element);
        if (!number)
        {
            return parseFailure(context,
                                std::format(R"("{}" holds a value that is not a number)", key));
        }
        numbers.push_back(*number);
    }
    return numbers;
}

/// A {"rotation": [9 numbers, row-major], "translation": [x, y, z]} object.
[[nodiscard]] Result<Transformation> goldenTransformation(const json&      object,
                                                          std::string_view context)
{
    const auto rotation    = numbersMember(object, "rotation", 9, context);
    const auto translation = numbersMember(object, "translation", 3, context);
    if (!rotation || !translation)
    {
        return std::unexpected(!rotation ? rotation.error() : translation.error());
    }
    const std::vector<double>&    r = *rotation;
    const Transformation::Matrix3 matrix{
        {{r[0], r[1], r[2]}, {r[3], r[4], r[5]}, {r[6], r[7], r[8]}}};
    const std::vector<double>& t = *translation;
    return Transformation{matrix, Coordinate{t[0], t[1], t[2]}};
}

/// The view a figure.json names "side", "top" or "back".
[[nodiscard]] std::optional<FigureView> goldenFigureView(std::string_view name)
{
    if (name == "side")
    {
        return FigureView::SIDE;
    }
    if (name == "top")
    {
        return FigureView::TOP;
    }
    if (name == "back")
    {
        return FigureView::BACK;
    }
    return std::nullopt;
}

// ------------------------------------------------------------------------------------- shapes

/// A path's segment types from its golden letters ("ML182Z"), or nullopt when they are malformed.
[[nodiscard]] std::optional<std::vector<PathSegment::Type>> goldenSegmentTypes(
    std::string_view letters)
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
        static constexpr std::string_view kLetters = "MLQCZ";
        const std::size_t                 position = kLetters.find(letter);
        if (position == std::string_view::npos)
        {
            return std::nullopt;
        }
        static constexpr std::array kTypes{PathSegment::Type::MOVE_TO, PathSegment::Type::LINE_TO,
                                           PathSegment::Type::QUAD_TO, PathSegment::Type::CUBIC_TO,
                                           PathSegment::Type::CLOSE};
        types.insert(types.end(), count == 0 ? 1 : count, kTypes.at(position));
    }
    return types;
}

/// Adds the segment @p type with its points from @p v at @p k to @p path.
void addGoldenSegment(FigurePath& path, PathSegment::Type type, std::span<const double> v,
                      std::size_t k)
{
    switch (type)
    {
        case PathSegment::Type::MOVE_TO:
            path.moveTo(v[k], v[k + 1]);
            break;
        case PathSegment::Type::LINE_TO:
            path.lineTo(v[k], v[k + 1]);
            break;
        case PathSegment::Type::QUAD_TO:
            path.quadTo(v[k], v[k + 1], v[k + 2], v[k + 3]);
            break;
        case PathSegment::Type::CUBIC_TO:
            path.curveTo(v[k], v[k + 1], v[k + 2], v[k + 3], v[k + 4], v[k + 5]);
            break;
        case PathSegment::Type::CLOSE:
            path.closePath();
            break;
    }
}

/// A golden path: its precision, winding rule, segment letters and coordinates, rebuilt with
/// FigurePath's own calls (a Path2D.Float's coordinates are floats already, so storing them
/// again changes nothing).
[[nodiscard]] Result<FigureShape> goldenPath(const json& shape, std::string_view context)
{
    const auto precision   = stringMember(shape, "precision", context);
    const auto winding     = stringMember(shape, "winding", context);
    const auto letters     = stringMember(shape, "segments", context);
    const auto coordinates = numbersMember(shape, "coordinates", 0, context);
    if (!precision || !winding || !letters || !coordinates)
    {
        return parseFailure(context,
                            "a path needs its precision, winding, segments and coordinates");
    }
    if ((*precision != "float" && *precision != "double") ||
        (*winding != "nonZero" && *winding != "evenOdd"))
    {
        return parseFailure(
            context, std::format("unknown precision {} or winding {}", *precision, *winding));
    }
    const auto types = goldenSegmentTypes(*letters);
    if (!types || (!types->empty() && types->front() != PathSegment::Type::MOVE_TO))
    {
        return parseFailure(context, std::format("malformed segments {}", *letters));
    }
    std::size_t needed = 0;
    for (const PathSegment::Type type : *types)
    {
        needed += 2 * static_cast<std::size_t>(PathSegment::pointCount(type));
    }
    if (needed != coordinates->size())
    {
        return parseFailure(context, std::format("segments {} need {} coordinates, not {}",
                                                 *letters, needed, coordinates->size()));
    }
    FigurePath path{
        *precision == "float" ? FigurePath::Precision::FLOAT : FigurePath::Precision::DOUBLE,
        *winding == "nonZero" ? WindingRule::NON_ZERO : WindingRule::EVEN_ODD};
    std::size_t k = 0;
    for (const PathSegment::Type type : *types)
    {
        addGoldenSegment(path, type, *coordinates, k);
        k += 2 * static_cast<std::size_t>(PathSegment::pointCount(type));
    }
    return FigureShape{std::move(path)};
}

/// The arc type a figure.json names "open", "chord" or "pie".
[[nodiscard]] std::optional<FigureArc::Type> goldenArcType(std::string_view name)
{
    if (name == "open")
    {
        return FigureArc::Type::OPEN;
    }
    if (name == "chord")
    {
        return FigureArc::Type::CHORD;
    }
    if (name == "pie")
    {
        return FigureArc::Type::PIE;
    }
    return std::nullopt;
}

/// A golden Arc2D: its frame, its start and extent in degrees and its type.
[[nodiscard]] Result<FigureShape> goldenArc(const json& shape, std::span<const double> f,
                                            std::string_view context)
{
    const auto angles = numbersMember(shape, "angles", 2, context);
    const auto name   = stringMember(shape, "arcType", context);
    if (!angles || !name)
    {
        return parseFailure(context, "an arc needs its angles and its arcType");
    }
    const std::optional<FigureArc::Type> type = goldenArcType(*name);
    if (!type)
    {
        return parseFailure(context, std::format("unknown arc type {}", *name));
    }
    return FigureShape{FigureArc{f[0], f[1], f[2], f[3], (*angles)[0], (*angles)[1], *type}};
}

/// A golden primitive with a frame (x, y, width, height): Rect, Ellipse, RoundRect or Arc.
[[nodiscard]] Result<FigureShape> goldenFramedShape(const json& shape, std::string_view type,
                                                    std::string_view context)
{
    const auto frame = numbersMember(shape, "frame", 4, context);
    if (!frame)
    {
        return std::unexpected(frame.error());
    }
    const std::vector<double>& f = *frame;
    if (type == "rect")
    {
        return FigureShape{FigureRect{f[0], f[1], f[2], f[3]}};
    }
    if (type == "ellipse")
    {
        return FigureShape{FigureEllipse{f[0], f[1], f[2], f[3]}};
    }
    if (type == "roundRect")
    {
        const auto arc = numbersMember(shape, "arc", 2, context);
        if (!arc)
        {
            return std::unexpected(arc.error());
        }
        return FigureShape{FigureRoundRect{f[0], f[1], f[2], f[3], (*arc)[0], (*arc)[1]}};
    }
    return goldenArc(shape, f, context);
}

// ------------------------------------------------------------------------------------ figures

[[nodiscard]] Result<std::optional<Color>> goldenColor(const json&      instance,
                                                       std::string_view context)
{
    const auto value = member(instance, "color", context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if ((*value)->is_null())
    {
        return std::optional<Color>{};
    }
    if (!(*value)->is_array() || (*value)->size() != 4 ||
        !std::ranges::all_of(**value, [](const json& c) { return c.is_number_integer(); }))
    {
        return parseFailure(context, R"("color" is neither null nor [r, g, b, a])");
    }
    const json& c = **value;
    return std::optional<Color>{
        Color{c[0].get<int>(), c[1].get<int>(), c[2].get<int>(), c[3].get<int>()}};
}

[[nodiscard]] Result<std::optional<LineStyle>> goldenLineStyle(const json&      instance,
                                                               std::string_view context)
{
    const auto value = member(instance, "lineStyle", context);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    if ((*value)->is_null())
    {
        return std::optional<LineStyle>{};
    }
    const std::optional<LineStyle> style =
        (*value)->is_string() ? lineStyleFromString((*value)->get<std::string>()) : std::nullopt;
    if (!style)
    {
        return parseFailure(context, R"("lineStyle" is neither null nor a line style)");
    }
    return std::optional<LineStyle>{style};
}

/// The shapes of an instance.
[[nodiscard]] Result<std::vector<GoldenFigureShape>> goldenInstanceShapes(const json&      instance,
                                                                          std::string_view context)
{
    const auto list = arrayMember(instance, "shapes", context);
    if (!list)
    {
        return std::unexpected(list.error());
    }
    std::vector<GoldenFigureShape> shapes;
    for (const json& shapeJson : **list)
    {
        auto       shape   = parseGoldenFigureShape(shapeJson);
        const auto visible = booleanMember(shapeJson, "visible", context);
        if (!shape || !visible)
        {
            return std::unexpected(!shape ? shape.error() : visible.error());
        }
        shapes.push_back(GoldenFigureShape{.shape = std::move(*shape), .visible = *visible});
    }
    return shapes;
}

[[nodiscard]] Result<GoldenFigureInstance> goldenInstance(const json&      instance,
                                                          std::string_view viewContext)
{
    const auto path = stringMember(instance, "path", viewContext);
    if (!path)
    {
        return std::unexpected(path.error());
    }
    const std::string context        = std::format("{} instance {}", viewContext, *path);
    const auto        extraRender    = booleanMember(instance, "extraRender", context);
    const auto        contextIndex   = integerMember(instance, "context", context);
    const auto        instanceNumber = integerMember(instance, "instanceNumber", context);
    const auto        displayOrder   = integerMember(instance, "displayOrder", context);
    if (!extraRender || !contextIndex || !instanceNumber || !displayOrder)
    {
        return parseFailure(context,
                            "an instance needs extraRender, context, instanceNumber and "
                            "displayOrder");
    }
    auto color = goldenColor(instance, context);
    if (!color)
    {
        return std::unexpected(color.error());
    }
    auto lineStyle = goldenLineStyle(instance, context);
    if (!lineStyle)
    {
        return std::unexpected(lineStyle.error());
    }
    auto shapes = goldenInstanceShapes(instance, context);
    if (!shapes)
    {
        return std::unexpected(shapes.error());
    }
    return GoldenFigureInstance{.path           = *path,
                                .extraRender    = *extraRender,
                                .context        = *contextIndex,
                                .instanceNumber = *instanceNumber,
                                .displayOrder   = *displayOrder,
                                .color          = *color,
                                .lineStyle      = *lineStyle,
                                .shapes         = std::move(*shapes)};
}

[[nodiscard]] Result<GoldenFigureMotor> goldenMotor(const json& motor, std::string_view context)
{
    const auto mount    = stringMember(motor, "mount", context);
    const auto location = integerMember(motor, "location", context);
    const auto shape    = member(motor, "shape", context);
    if (!mount || !location || !shape)
    {
        return parseFailure(context, "a motor needs its mount, location and shape");
    }
    auto figureShape = parseGoldenFigureShape(**shape);
    if (!figureShape)
    {
        return std::unexpected(figureShape.error());
    }
    return GoldenFigureMotor{
        .mount = *mount, .location = *location, .shape = std::move(*figureShape)};
}

/// Appends the instances and motors of the view @p view to @p result.
[[nodiscard]] Result<void> readGoldenViewContents(const json& view, std::string_view context,
                                                  GoldenFigureView& result)
{
    const auto instances = arrayMember(view, "instances", context);
    const auto motors    = arrayMember(view, "motors", context);
    if (!instances || !motors)
    {
        return std::unexpected(!instances ? instances.error() : motors.error());
    }
    for (const json& instanceJson : **instances)
    {
        auto instance = goldenInstance(instanceJson, context);
        if (!instance)
        {
            return std::unexpected(instance.error());
        }
        result.instances.push_back(std::move(*instance));
    }
    for (const json& motorJson : **motors)
    {
        auto motor = goldenMotor(motorJson, context);
        if (!motor)
        {
            return std::unexpected(motor.error());
        }
        result.motors.push_back(std::move(*motor));
    }
    return {};
}

[[nodiscard]] Result<GoldenFigureView> goldenView(const json& view, std::size_t index)
{
    const std::string context            = std::format("figure.json view {}", index);
    const auto        name               = stringMember(view, "view", context);
    const auto        rotation           = numberMember(view, "rotation", context);
    const auto        subject            = numbersMember(view, "subjectBounds", 4, context);
    const auto        figureRotationJson = member(view, "figureRotation", context);
    if (!name || !rotation || !subject || !figureRotationJson)
    {
        return parseFailure(context,
                            "a view needs its view, rotation, figureRotation and subjectBounds");
    }
    const std::optional<FigureView> figureView = goldenFigureView(*name);
    const auto figureRotation = goldenTransformation(**figureRotationJson, context);
    if (!figureView || !figureRotation)
    {
        return parseFailure(context,
                            std::format("unknown view {} or malformed figureRotation", *name));
    }
    GoldenFigureView result;
    result.view           = *figureView;
    result.rotation       = *rotation;
    result.figureRotation = *figureRotation;
    result.subjectBounds  = Rectangle2D{
        .x = (*subject)[0], .y = (*subject)[1], .width = (*subject)[2], .height = (*subject)[3]};
    if (auto contents = readGoldenViewContents(view, context, result); !contents)
    {
        return std::unexpected(contents.error());
    }
    return result;
}

/// Checks the schema, version and input of @p figure and reads its configuration into @p result.
[[nodiscard]] Result<void> readGoldenFigureHeader(const json& figure, GoldenFigure& result)
{
    constexpr std::string_view kContext      = "figure.json";
    const auto                 schema        = stringMember(figure, "schema", kContext);
    const auto                 version       = integerMember(figure, "schemaVersion", kContext);
    const auto                 input         = stringMember(figure, "input", kContext);
    const auto                 showMarkers   = booleanMember(figure, "showMarkers", kContext);
    const auto                 configuration = member(figure, "configuration", kContext);
    if (!schema || !version || !input || !showMarkers || !configuration)
    {
        return parseFailure(kContext,
                            "missing schema, schemaVersion, input, showMarkers or "
                            "configuration");
    }
    if (*schema != "figure" || *version != kGoldenSchemaVersion)
    {
        return parseFailure(kContext, std::format("schema {} version {}, not figure version {}",
                                                  *schema, *version, kGoldenSchemaVersion));
    }
    const auto index = integerMember(**configuration, "index", "figure.json configuration");
    const auto id    = stringMember(**configuration, "id", "figure.json configuration");
    if (!index || !id)
    {
        return std::unexpected(!index ? index.error() : id.error());
    }
    result.input              = *input;
    result.showMarkers        = *showMarkers;
    result.configurationIndex = *index;
    result.configurationId    = *id;
    return {};
}

}  // namespace

Result<FigureShape> parseGoldenFigureShape(const json& shape)
{
    constexpr std::string_view kContext = "figure.json shape";
    const auto                 type     = stringMember(shape, "type", kContext);
    if (!type)
    {
        return std::unexpected(type.error());
    }
    if (*type == "rect" || *type == "ellipse" || *type == "roundRect" || *type == "arc")
    {
        return goldenFramedShape(shape, *type, kContext);
    }
    if (*type == "line")
    {
        const auto points = numbersMember(shape, "points", 4, kContext);
        if (!points)
        {
            return std::unexpected(points.error());
        }
        const std::vector<double>& p = *points;
        return FigureShape{FigureLine{p[0], p[1], p[2], p[3]}};
    }
    if (*type == "path")
    {
        return goldenPath(shape, kContext);
    }
    return parseFailure(kContext, std::format("unknown type {}", *type));
}

Result<GoldenFigure> parseGoldenFigure(const json& figure)
{
    GoldenFigure result;
    if (auto header = readGoldenFigureHeader(figure, result); !header)
    {
        return std::unexpected(header.error());
    }
    const auto selection = arrayMember(figure, "selection", "figure.json");
    const auto views     = arrayMember(figure, "views", "figure.json");
    if (!selection || !views)
    {
        return std::unexpected(!selection ? selection.error() : views.error());
    }
    for (const json& path : **selection)
    {
        if (!path.is_string())
        {
            return parseFailure("figure.json", R"("selection" holds a value that is no path)");
        }
        result.selection.push_back(path.get<std::string>());
    }
    for (std::size_t i = 0; i < (*views)->size(); ++i)
    {
        auto view = goldenView((**views)[i], i);
        if (!view)
        {
            return std::unexpected(view.error());
        }
        result.views.push_back(std::move(*view));
    }
    return result;
}

Result<GoldenFigure> loadGoldenFigure(std::string_view name)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const GoldenInput* input = manifest->find(name);
    if (input == nullptr || input->figure.empty())
    {
        return fail(ErrorCode::NOT_FOUND,
                    std::format("manifest.json names no figure for the input {}", name));
    }
    const Result<json> document = loadGoldenJson(input->figure);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    return parseGoldenFigure(*document);
}

// ------------------------------------------------------------------------------- comparisons

namespace
{

/// The kind of an arc, by its constant's name.
[[nodiscard]] std::string_view arcTypeName(FigureArc::Type type) noexcept
{
    switch (type)
    {
        case FigureArc::Type::OPEN:
            return "OPEN";
        case FigureArc::Type::CHORD:
            return "CHORD";
        case FigureArc::Type::PIE:
            return "PIE";
    }
    return "?";
}

/// The segment letters of @p segments as the goldens write them ("ML182Z").
[[nodiscard]] std::string segmentLetters(const std::vector<PathSegment>& segments)
{
    std::string letters;
    std::size_t i = 0;
    while (i < segments.size())
    {
        std::size_t run = 1;
        while (i + run < segments.size() && segments[i + run].type == segments[i].type)
        {
            ++run;
        }
        letters += pathSegmentLetter(segments[i].type);
        if (run > 1)
        {
            letters += std::to_string(run);
        }
        i += run;
    }
    return letters;
}

[[nodiscard]] FigureShapeValues pathValues(const FigurePath& path)
{
    FigureShapeValues result;
    result.structure = std::format(
        "path {} {} {}", path.precision() == FigurePath::Precision::FLOAT ? "FLOAT" : "DOUBLE",
        path.windingRule() == WindingRule::NON_ZERO ? "NON_ZERO" : "EVEN_ODD",
        segmentLetters(path.segments()));
    for (const PathSegment& segment : path.segments())
    {
        const auto count = static_cast<std::size_t>(segment.pointCount());
        for (std::size_t p = 0; p < count; ++p)
        {
            result.values.push_back(segment.points.at(p).x);
            result.values.push_back(segment.points.at(p).y);
        }
    }
    return result;
}

/// Gives the values of each primitive.
struct ValuesOf
{
    [[nodiscard]] FigureShapeValues operator()(const FigureRect& r) const
    {
        return {.structure = "rect", .values = {r.x(), r.y(), r.width(), r.height()}};
    }
    [[nodiscard]] FigureShapeValues operator()(const FigureEllipse& e) const
    {
        return {.structure = "ellipse", .values = {e.x(), e.y(), e.width(), e.height()}};
    }
    [[nodiscard]] FigureShapeValues operator()(const FigureRoundRect& r) const
    {
        return {.structure = "roundRect",
                .values    = {r.x(), r.y(), r.width(), r.height(), r.arcWidth(), r.arcHeight()}};
    }
    [[nodiscard]] FigureShapeValues operator()(const FigureArc& a) const
    {
        return {.structure = std::format("arc {}", arcTypeName(a.type())),
                .values = {a.x(), a.y(), a.width(), a.height(), a.angleStart(), a.angleExtent()}};
    }
    [[nodiscard]] FigureShapeValues operator()(const FigureLine& l) const
    {
        return {.structure = "line", .values = {l.x1(), l.y1(), l.x2(), l.y2()}};
    }
    [[nodiscard]] FigureShapeValues operator()(const FigurePath& p) const { return pathValues(p); }
};

}  // namespace

FigureShapeValues figureShapeValues(const FigureShape& shape)
{
    return std::visit(ValuesOf{}, shape.geometry());
}

bool figureValueMatches(double actual, double expected, double tolerance)
{
    if (tolerance == 0)
    {
        return actual == expected;
    }
    return std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

std::optional<std::string> figureShapeDifference(const FigureShape& actual,
                                                 const FigureShape& expected, double tolerance)
{
    const FigureShapeValues a = figureShapeValues(actual);
    const FigureShapeValues e = figureShapeValues(expected);
    if (a.structure != e.structure || a.values.size() != e.values.size())
    {
        return std::format("{} with {} values, expected {} with {}", a.structure, a.values.size(),
                           e.structure, e.values.size());
    }
    for (std::size_t i = 0; i < a.values.size(); ++i)
    {
        if (!figureValueMatches(a.values[i], e.values[i], tolerance))
        {
            return std::format("{}: value {} is {}, expected {} (tolerance {})", a.structure, i,
                               a.values[i], e.values[i], tolerance);
        }
    }
    return std::nullopt;
}

namespace
{

/// Whether @p v lies in [@p from, @p to], each end widened by @p tolerance as figureValueMatches()
/// widens a value.
[[nodiscard]] bool withinRange(double v, double from, double to, double tolerance)
{
    return v >= from - (tolerance * std::max(1.0, std::abs(from))) &&
           v <= to + (tolerance * std::max(1.0, std::abs(to)));
}

/// What is wrong with ray @p ray of @p actual (its move @p move and its line @p line) against
/// Java's move @p javaMove, with the maximal ray length @p reach; nullopt when nothing is.
[[nodiscard]] std::optional<std::string> rayDifference(std::size_t ray, const PathSegment& move,
                                                       const PathSegment& line,
                                                       const PathSegment& javaMove, double reach,
                                                       double tolerance)
{
    const Point2D centre = javaMove.points[0];
    if (javaMove.type != PathSegment::Type::MOVE_TO || move.type != PathSegment::Type::MOVE_TO ||
        line.type != PathSegment::Type::LINE_TO)
    {
        return std::format("ray {} is not a move and a line", ray);
    }
    if (!figureValueMatches(move.points[0].x, centre.x, tolerance) ||
        !figureValueMatches(move.points[0].y, centre.y, tolerance))
    {
        return std::format("ray {} starts at {},{}, not at Java's centre {},{}", ray,
                           move.points[0].x, move.points[0].y, centre.x, centre.y);
    }
    const Point2D end = line.points[0];
    if (!withinRange(end.x, centre.x, centre.x + reach, tolerance) ||
        !withinRange(end.y, centre.y, centre.y + reach, tolerance))
    {
        return std::format("ray {} ends at {},{}, outside the rays' range", ray, end.x, end.y);
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> deploymentChargeRaysDifference(const FigureShape& actual,
                                                          const FigureShape& expected,
                                                          const Rectangle2D& frame,
                                                          double             tolerance)
{
    constexpr auto kSegments =
        2 * static_cast<std::size_t>(MassComponentShapes::kDeploymentChargeRays);
    const auto* a = actual.getIf<FigurePath>();
    const auto* e = expected.getIf<FigurePath>();
    if (a == nullptr || e == nullptr || a->segments().size() != kSegments ||
        e->segments().size() != kSegments)
    {
        return std::format("not {} rays as Java's", MassComponentShapes::kDeploymentChargeRays);
    }
    const double reach = std::min(frame.width, frame.height) / 4.0;
    for (std::size_t i = 0; i < kSegments; i += 2)
    {
        if (auto difference = rayDifference(i / 2, a->segments()[i], a->segments()[i + 1],
                                            e->segments()[i], reach, tolerance))
        {
            return difference;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket::Test
