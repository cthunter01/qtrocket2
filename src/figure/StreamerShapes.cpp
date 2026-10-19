#include "QtRocket/figure/StreamerShapes.h"

#include <utility>
#include <vector>

#include "MassObjectShapeSupport.h"
#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::StreamerShapes
{

namespace
{

/// The four pairs of cubic curves along the edge at @p edge, from @p left over the flutter width
/// @p flutterWidth, bulging @p flutterHeight up and then down.
void addFlutter(FigurePath& streamer, double left, double edge, double flutterWidth,
                double flutterHeight)
{
    for (int i = 0; i < 4; i++)
    {
        streamer.curveTo(left + (((4 * i) + 1) * flutterWidth / 4), edge + flutterHeight,
                         left + (((4 * i) + 1) * flutterWidth / 4), edge + flutterHeight,
                         left + (((4 * i) + 2) * flutterWidth / 4), edge);
        streamer.curveTo(left + (((4 * i) + 3) * flutterWidth / 4), edge - flutterHeight,
                         left + (((4 * i) + 3) * flutterWidth / 4), edge - flutterHeight,
                         left + (((4 * i) + 4) * flutterWidth / 4), edge);
    }
}

void addSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double left           = bounds.x + (bounds.width / 4);
    const double streamerWidth  = bounds.width / 2;
    const double streamerHeight = bounds.height / 2;
    const double top            = Detail::centerY(bounds) + (streamerHeight / 2);
    const double bottom         = Detail::centerY(bounds) - (streamerHeight / 2);
    const double flutterHeight  = bounds.height / 16;
    const double flutterWidth   = streamerWidth / 4;

    FigurePath streamer;
    streamer.moveTo(left, bottom);  // bottom left
    streamer.lineTo(left, top);     // upper left
    addFlutter(streamer, left, top, flutterWidth, flutterHeight);
    streamer.lineTo(left + streamerWidth, bottom);
    streamer.moveTo(left, bottom);  // bottom left
    addFlutter(streamer, left, bottom, flutterWidth, flutterHeight);

    shapes.emplace_back(std::move(streamer));
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<MassObject>(component, "StreamerShapes");

    const FigureRoundRect    base = Detail::massObjectSide(massObj, transformation);
    std::vector<FigureShape> s{FigureShape{base}};
    addSymbol(s, base.bounds());
    return ComponentShape::fromShapes(std::move(s), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<MassObject>(component, "StreamerShapes");

    return ComponentShape::fromShapes(
        {FigureShape{Detail::massObjectBack(massObj, transformation)}}, component);
}

}  // namespace QtRocket::StreamerShapes
