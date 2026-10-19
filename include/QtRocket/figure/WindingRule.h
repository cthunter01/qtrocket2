#pragma once

namespace QtRocket
{

/// Which points a closed outline encloses (java.awt.geom.PathIterator's WIND_EVEN_ODD and
/// WIND_NON_ZERO). Every shape primitive of the figure uses NON_ZERO, as Java's do, and so does a
/// FigurePath unless it is given another rule; OpenRocket's figure never uses EVEN_ODD. A painter
/// must fill with the same rule (QPainterPath's default is the even-odd rule).
enum class WindingRule
{
    /// A point is inside when a ray from it crosses the outline an odd number of times.
    EVEN_ODD,
    /// A point is inside when the outline winds around it a non-zero number of times.
    NON_ZERO,
};

}  // namespace QtRocket
