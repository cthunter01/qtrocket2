#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
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

/// One shape of the 2D figure, in the figure's coordinates (metres, y up): one of the
/// java.awt.geom primitives OpenRocket's shape providers create (java.awt.Shape). It answers
/// what RocketFigure asks of a Shape: its outline (the PathIterator, which is also what a painter
/// draws), its bounds and whether it contains a point (the hit test). The primitive itself, with
/// its parameters, is geometry() or getIf<T>().
class FigureShape
{
public:
    /// The primitive, in the order of Type.
    using Geometry =
        std::variant<FigureRect, FigureEllipse, FigureRoundRect, FigureArc, FigureLine, FigurePath>;

    /// Which primitive the shape is (Geometry's alternative).
    enum class Type
    {
        RECT,
        ELLIPSE,
        ROUND_RECT,
        ARC,
        LINE,
        PATH,
    };

    /// The shape of @p geometry (any of Geometry's alternatives).
    template <typename T>
        requires std::constructible_from<Geometry, T>
    explicit FigureShape(T geometry) noexcept(std::is_nothrow_move_constructible_v<T>)
      : m_geometry(std::move(geometry))
    {
    }

    /// The alternative geometry() holds.
    [[nodiscard]] Type type() const noexcept;

    [[nodiscard]] const Geometry& geometry() const noexcept { return m_geometry; }

    /// The primitive when it is a @p T, else nullptr.
    template <typename T>
    [[nodiscard]] const T* getIf() const noexcept
    {
        return std::get_if<T>(&m_geometry);
    }

    /// The outline, as the shape's PathIterator gives it without a transformation
    /// (Shape.getPathIterator(null) and currentSegment(double[])).
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// The rule by which the outline encloses points: NON_ZERO for every primitive but a path,
    /// which has its own (PathIterator.getWindingRule).
    [[nodiscard]] WindingRule windingRule() const noexcept;

    /// The primitive's Shape.getBounds2D.
    [[nodiscard]] Rectangle2D bounds() const;

    /// The primitive's Shape.contains(Point2D), RocketFigure's hit test. Always false for a line.
    [[nodiscard]] bool contains(Point2D p) const;

    /// Equal primitives with equal parameters.
    [[nodiscard]] bool operator==(const FigureShape&) const = default;

private:
    Geometry m_geometry;
};

}  // namespace QtRocket
