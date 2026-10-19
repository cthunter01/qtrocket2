#include "QtRocket/figure/RailButtonShapes.h"

#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/AffineTransform2D.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::RailButtonShapes
{

namespace
{

/// The dimensions of a button, as both views read them.
struct ButtonDimensions
{
    double baseHeight;
    double innerHeight;
    double flangeHeight;
    double screwHeight;
    double outerDiameter;
    double outerRadius;
    double innerDiameter;
    double innerRadius;
};

[[nodiscard]] ButtonDimensions dimensionsOf(const RailButton& btn)
{
    const double outerDiameter = btn.getOuterDiameter();
    const double innerDiameter = btn.getInnerDiameter();
    return ButtonDimensions{.baseHeight    = btn.getBaseHeight(),
                            .innerHeight   = btn.getInnerHeight(),
                            .flangeHeight  = btn.getFlangeHeight(),
                            .screwHeight   = btn.getScrewHeight(),
                            .outerDiameter = outerDiameter,
                            .outerRadius   = outerDiameter / 2,
                            .innerDiameter = innerDiameter,
                            .innerRadius   = innerDiameter / 2};
}

/// The view angle of the button: its angle offset plus PI/2 less the angle of the transformed
/// unit y vector in the (z, y) plane.
[[nodiscard]] double combinedAngle(const RailButton& btn, const Transformation& transformation)
{
    const Coordinate unitOrientation = transformation.transform(Coordinate{0, 1, 0});
    const double     viewRotationRad =
        -std::atan2(unitOrientation.y, unitOrientation.z) + (std::numbers::pi / 2);
    const double angleOffsetRad = btn.getAngleOffset();
    return angleOffsetRad + viewRotationRad;
}

/// The outline paths of the side view.
struct SidePaths
{
    FigurePath path;
    FigurePath pathInvis;
};

/// One cylinder of the side view, of @p radius (and its diameter @p drawWidth), standing on
/// @p center with its faces @p drawHeight high and its height times the cosine @p heightCos:
/// the lower face, the two sides and the upper face, and the invisible rectangle over it.
void appendCylinder(SidePaths& paths, Point2D center, double radius, double drawWidth,
                    double drawHeight, double heightCos, double sinr)
{
    const Point2D lowerLeft{center.x - radius, center.y - (radius * sinr)};
    paths.path.append(FigureShape{FigureEllipse{lowerLeft.x, lowerLeft.y, drawWidth, drawHeight}},
                      false);

    paths.path.append(
        FigureShape{FigureLine{lowerLeft.x, center.y, lowerLeft.x, (center.y + heightCos)}}, false);
    paths.path.append(FigureShape{FigureLine{(center.x + radius), center.y, (center.x + radius),
                                             (center.y + heightCos)}},
                      false);

    paths.path.append(
        FigureShape{FigureEllipse{lowerLeft.x, (lowerLeft.y + heightCos), drawWidth, drawHeight}},
        false);

    // Invisible rectangle
    double yInvis = 0;
    if (heightCos >= 0)
    {
        yInvis = center.y;
    }
    else
    {
        yInvis = center.y + heightCos;
    }
    paths.pathInvis.append(
        FigureShape{FigureRect{center.x - radius, yInvis, drawWidth, std::abs(heightCos)}}, false);
}

}  // namespace

/**
 * The rail button's shape is basically 3 cylinders stacked on top of each other. To achieve this
 * 3D shape in a 2D way, the top and bottom faces of the cylinder are drawn as 2D ellipses and the
 * side faces are 2D lines. To be able to select the side faces, extra invisible rectangles are
 * added. This is because otherwise, there would just be empty space between the 2D lines and thus
 * the rail button could not be selected.
 */
std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& btn = Detail::providerComponent<RailButton>(component, "RailButtonShapes");

    const ButtonDimensions d = dimensionsOf(btn);

    // instance absolute location
    const Coordinate loc = transformation.transform(Coordinate::kZero);

    const double angle = combinedAngle(btn, transformation);
    const double sinr  = std::abs(std::sin(angle));
    const double cosr  = std::cos(angle);

    const double baseHeightcos   = d.baseHeight * cosr;
    const double innerHeightcos  = d.innerHeight * cosr;
    const double flangeHeightcos = d.flangeHeight * cosr;
    double       screwHeightcos  = d.screwHeight * cosr;

    // Don't draw the screw if it will overlap with the ellipse of the flange
    if (std::abs(screwHeightcos) <= d.outerDiameter * std::abs(sinr) / 2)
    {
        screwHeightcos = 0;
    }

    SidePaths                paths;
    std::optional<FigureArc> screwShape;
    // base cylinder
    if (d.baseHeight > 0)
    {
        appendCylinder(paths, Point2D{loc.x, loc.y}, d.outerRadius, d.outerDiameter,
                       d.outerDiameter * sinr, baseHeightcos, sinr);
    }
    // inner cylinder
    appendCylinder(paths, Point2D{loc.x, loc.y + baseHeightcos}, d.innerRadius, d.innerDiameter,
                   d.innerDiameter * sinr, innerHeightcos, sinr);
    // flange cylinder
    if (d.flangeHeight > 0 || d.screwHeight > 0)
    {
        // Also draw when there is a screw (= the bottom of the screw)
        appendCylinder(paths, Point2D{loc.x, loc.y + baseHeightcos + innerHeightcos}, d.outerRadius,
                       d.outerDiameter, d.outerDiameter * sinr, flangeHeightcos, sinr);
    }
    // screw
    if (d.screwHeight > 0)
    {
        const double drawWidth = d.outerDiameter;
        // Times 2 because it is the full ellipse height
        const double  drawHeight = std::abs(screwHeightcos) * 2;
        const Point2D origin{loc.x - d.outerRadius, loc.y + baseHeightcos + innerHeightcos +
                                                        flangeHeightcos - std::abs(screwHeightcos)};
        screwShape = FigureArc{origin.x,
                               origin.y,
                               drawWidth,
                               drawHeight,
                               0,
                               -MathUtil::signum(screwHeightcos) * 180,
                               FigureArc::Type::OPEN};
    }

    std::vector<ComponentShape> shapes;
    shapes.emplace_back(FigureShape{std::move(paths.path)}, component);
    if (screwShape)
    {
        shapes.emplace_back(FigureShape{*screwShape}, component);
    }
    shapes.push_back(ComponentShape::invisible(FigureShape{std::move(paths.pathInvis)}, component));
    return shapes;
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& btn = Detail::providerComponent<RailButton>(component, "RailButtonShapes");

    const ButtonDimensions d = dimensionsOf(btn);

    // instance absolute location
    const Coordinate loc = transformation.transform(Coordinate::kZero);

    const double combinedAngleRad = combinedAngle(btn, transformation);

    const double sinr = std::sin(combinedAngleRad);
    const double cosr = std::cos(combinedAngleRad);

    FigurePath                path;
    std::optional<FigurePath> screwShape;

    // base
    if (d.baseHeight > 0)
    {
        path.append(FigureShape{getRotatedRectangle(loc.z, loc.y, d.outerRadius, d.baseHeight,
                                                    combinedAngleRad)},
                    false);
    }

    {  // inner
        const double deltaR = d.baseHeight;
        const double deltaY = deltaR * cosr;
        const double deltaZ = deltaR * sinr;
        path.append(FigureShape{getRotatedRectangle(loc.z + deltaZ, loc.y + deltaY, d.innerRadius,
                                                    d.innerHeight, combinedAngleRad)},
                    false);
    }
    // flange
    if (d.flangeHeight > 0 || d.screwHeight > 0)
    {
        // Also draw when there is a screw (= the bottom of the screw)
        const double deltaR = d.baseHeight + d.innerHeight;
        const double deltaY = deltaR * cosr;
        const double deltaZ = deltaR * sinr;
        path.append(FigureShape{getRotatedRectangle(loc.z + deltaZ, loc.y + deltaY, d.outerRadius,
                                                    d.flangeHeight, combinedAngleRad)},
                    false);
    }
    // screw
    if (d.screwHeight > 0)
    {
        const double drawWidth = d.outerDiameter;
        // Times 2 because it is the full ellipse height
        const double    drawHeight = 2 * d.screwHeight;
        const double    deltaY     = d.baseHeight + d.innerHeight + d.flangeHeight - d.screwHeight;
        const double    deltaZ     = -d.outerRadius;
        const Point2D   origin{loc.z + deltaZ, loc.y + deltaY};
        const FigureArc screw{
            origin.x, origin.y, drawWidth, drawHeight, 0, -180, FigureArc::Type::OPEN};
        screwShape = getRotatedShape(FigureShape{screw}, loc.z, loc.y, -combinedAngleRad);
    }

    std::vector<ComponentShape> shapes;
    shapes.emplace_back(FigureShape{std::move(path)}, component);
    if (screwShape)
    {
        shapes.emplace_back(FigureShape{std::move(*screwShape)}, component);
    }
    return shapes;
}

FigurePath getRotatedRectangle(double x, double y, double radius, double height, double angleRad)
{
    FigurePath   rect;
    const double sinr = std::sin(angleRad);
    const double cosr = std::cos(angleRad);

    rect.moveTo((x - (radius * cosr)), (y + (radius * sinr)));
    rect.lineTo((x - (radius * cosr) + (height * sinr)), (y + (radius * sinr) + (height * cosr)));
    rect.lineTo((x + (radius * cosr) + (height * sinr)), (y - (radius * sinr) + (height * cosr)));
    rect.lineTo((x + (radius * cosr)), (y - (radius * sinr)));
    rect.closePath();

    return rect;
}

FigurePath getRotatedShape(const FigureShape& base, double x, double y, double angle)
{
    const AffineTransform2D rotate = AffineTransform2D::rotation(angle, x, y);
    return rotate.createTransformedShape(base);
}

}  // namespace QtRocket::RailButtonShapes
