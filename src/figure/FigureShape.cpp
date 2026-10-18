#include "QtRocket/figure/FigureShape.h"

#include <variant>
#include <vector>

#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

FigureShape::Type FigureShape::type() const noexcept
{
    if (std::holds_alternative<FigureRect>(m_geometry))
    {
        return Type::RECT;
    }
    if (std::holds_alternative<FigureEllipse>(m_geometry))
    {
        return Type::ELLIPSE;
    }
    if (std::holds_alternative<FigureRoundRect>(m_geometry))
    {
        return Type::ROUND_RECT;
    }
    if (std::holds_alternative<FigureArc>(m_geometry))
    {
        return Type::ARC;
    }
    if (std::holds_alternative<FigureLine>(m_geometry))
    {
        return Type::LINE;
    }
    return Type::PATH;
}

std::vector<PathSegment> FigureShape::segments() const
{
    return std::visit([](const auto& shape) { return std::vector<PathSegment>{shape.segments()}; },
                      m_geometry);
}

WindingRule FigureShape::windingRule() const noexcept
{
    if (const auto* path = getIf<FigurePath>())
    {
        return path->windingRule();
    }
    return WindingRule::NON_ZERO;
}

Rectangle2D FigureShape::bounds() const
{
    return std::visit([](const auto& shape) { return shape.bounds(); }, m_geometry);
}

bool FigureShape::contains(Point2D p) const
{
    return std::visit([p](const auto& shape) { return shape.contains(p); }, m_geometry);
}

}  // namespace QtRocket
