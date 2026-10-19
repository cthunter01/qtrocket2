#include "QtRocket/figure/ShockCordShapes.h"

#include <utility>
#include <vector>

#include "MassObjectShapeSupport.h"
#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::ShockCordShapes
{

namespace
{

void addSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double left          = bounds.x + (bounds.width / 4);
    const double cordWidth     = bounds.width / 2;
    const double top           = Detail::centerY(bounds);
    const double flutterHeight = bounds.height / 4;
    const double flutterWidth  = cordWidth / 4;

    FigurePath streamer;
    streamer.moveTo(left, Detail::centerY(bounds));

    for (int i = 0; i < 4; i++)
    {
        streamer.curveTo(left + (((4 * i) + 1) * flutterWidth / 4), top + flutterHeight,
                         left + (((4 * i) + 1) * flutterWidth / 4), top + flutterHeight,
                         left + (((4 * i) + 2) * flutterWidth / 4), top);
        streamer.curveTo(left + (((4 * i) + 3) * flutterWidth / 4), top - flutterHeight,
                         left + (((4 * i) + 3) * flutterWidth / 4), top - flutterHeight,
                         left + (((4 * i) + 4) * flutterWidth / 4), top);
    }

    shapes.emplace_back(std::move(streamer));
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<ShockCord>(component, "ShockCordShapes");

    const FigureRoundRect    base = Detail::massObjectSide(massObj, transformation);
    std::vector<FigureShape> s{FigureShape{base}};
    addSymbol(s, base.bounds());
    return ComponentShape::fromShapes(std::move(s), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<ShockCord>(component, "ShockCordShapes");

    return ComponentShape::fromShapes(
        {FigureShape{Detail::massObjectBack(massObj, transformation)}}, component);
}

}  // namespace QtRocket::ShockCordShapes
