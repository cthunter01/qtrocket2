#include "QtRocket/figure/ComponentShape.h"

#include <utility>
#include <vector>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"

namespace QtRocket
{

ComponentShape::ComponentShape(FigureShape shape, const RocketComponent& component)
  : m_shape(std::move(shape)),
    m_component(&component),
    m_color(component.getColor()),
    m_lineStyle(component.getLineStyle())
{
}

ComponentShape ComponentShape::invisible(FigureShape shape, const RocketComponent& component)
{
    ComponentShape result{std::move(shape), component};
    result.m_color   = Color::invisible();
    result.m_visible = false;
    return result;
}

std::vector<ComponentShape> ComponentShape::fromShapes(std::vector<FigureShape> shapes,
                                                       const RocketComponent&   component)
{
    std::vector<ComponentShape> result;
    result.reserve(shapes.size());
    for (FigureShape& shape : shapes)
    {
        result.emplace_back(std::move(shape), component);
    }
    return result;
}

}  // namespace QtRocket
