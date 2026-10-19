#include "QtRocket/figure/ParachuteShapes.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "MassObjectShapeSupport.h"
#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::ParachuteShapes
{

namespace
{

using Detail::centerX;
using Detail::centerY;

void addSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    // Java: if (chuteDiameter > 0.75 * width) chuteDiameter = 0.75 * width; std::min(a, b) is
    // (b < a) ? b : a, the same for every value, NaN and signed zeros included.
    const double chuteDiameter = std::min(bounds.height / 2, 0.75 * bounds.width);

    shapes.emplace_back(FigureArc{centerX(bounds) - (chuteDiameter / 2),
                                  centerY(bounds) - (chuteDiameter / 4), chuteDiameter,
                                  chuteDiameter, 180.0, 180.0, FigureArc::Type::PIE});
    FigurePath shrouds;
    shrouds.moveTo(centerX(bounds) - (chuteDiameter / 2), centerY(bounds) + (chuteDiameter / 4));
    shrouds.lineTo(centerX(bounds), centerY(bounds) - (3 * chuteDiameter / 4));
    shrouds.lineTo(centerX(bounds) + (chuteDiameter / 2), centerY(bounds) + (chuteDiameter / 4));

    shrouds.moveTo(centerX(bounds) - (chuteDiameter / 4), centerY(bounds) + (chuteDiameter / 4));
    shrouds.lineTo(centerX(bounds), centerY(bounds) - (3 * chuteDiameter / 4));
    shrouds.lineTo(centerX(bounds) + (chuteDiameter / 4), centerY(bounds) + (chuteDiameter / 4));

    shrouds.moveTo(centerX(bounds), centerY(bounds) + (chuteDiameter / 4));
    shrouds.lineTo(centerX(bounds), centerY(bounds) - (3 * chuteDiameter / 4));

    shapes.emplace_back(std::move(shrouds));
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<MassObject>(component, "ParachuteShapes");

    const FigureRoundRect    base = Detail::massObjectSide(massObj, transformation);
    std::vector<FigureShape> s{FigureShape{base}};
    addSymbol(s, base.bounds());
    return ComponentShape::fromShapes(std::move(s), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<MassObject>(component, "ParachuteShapes");

    return ComponentShape::fromShapes(
        {FigureShape{Detail::massObjectBack(massObj, transformation)}}, component);
}

}  // namespace QtRocket::ParachuteShapes
