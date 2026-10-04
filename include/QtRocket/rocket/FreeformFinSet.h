#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// A set of fins of any outline (OpenRocket's FreeformFinSet): a list of points in the fin frame
/// (see FinSet), the first at the origin and the last on the parent's surface, the length being
/// the last point's x. It is the only fin set a transition or a nose cone accepts.
///
/// The outline is kept valid as OpenRocket keeps it, silently:
/// - update() (which every change event of the rocket runs, and setPoints() and setPoint() call)
///   recomputes the length and the position and, on a parent, clamps the points: the first point
///   goes back to the origin, the fin's offset taking up its x (for TOP and MIDDLE) and every
///   other point moving the other way; an interior point that lies inside the body (within the
///   parent's length) is lifted onto its surface; the last point is put on the surface.
/// - An outline whose segments intersect is rejected: setPoints() restores the previous points
///   and length, setPoint() the previous point, removePoint() the previous points. What update()
///   changed meanwhile (the position, the tab length, the other points) stays, as in OpenRocket,
///   and setPoints() and removePoint() still fire their event.
/// - Coordinates above kSnapLargerThan are limited to it. In setPoints() a point with both x and
///   y above the limit has only its y limited (OpenRocket overwrites the limited x).
/// - setPoint() does nothing without a parent.
///
/// Errors: OpenRocket's checked IllegalFinPointException becomes a Result<void> failure with the
/// same message (ErrorCode::INVALID_ARGUMENT): an index out of range in addPoint(), removePoint()
/// and setPoint(). setPoints() never fails; it reverts as described.
///
/// Deviations from OpenRocket:
/// - Point indices are std::size_t, so removePoint()'s "index out of range" covers indices beyond
///   the last point only (Java's int can be negative).
/// - The setPoints() overloads for an array and for an ArrayList are one function taking a span;
///   the points are copied (Java keeps and modifies the caller's ArrayList). An empty list throws
///   BugError (Java: IndexOutOfBoundsException).
/// - convertFinSet() returns a ConversionResult that says who owns the new and the replaced fin
///   set (Java returns the new one and leaves the old one to the garbage collector).
/// - copyWithOriginalID(), which copies the point list, is the implicit copy constructor.
/// - The multi-edit config listeners (and getConfigListenerPointIdx()) are not ported, by
///   decision, nor is the logging of a detected intersection.
class FreeformFinSet : public FinSet
{
public:
    /// A fin point coordinate above this is limited to it, in m (SNAP_LARGER_THAN).
    static constexpr double kSnapLargerThan = 2.5;

    /// The result of convertFinSet().
    struct [[nodiscard]] ConversionResult
    {
        /// The new freeform fin set: in the tree in the converted fin set's place, or, when
        /// that one was detached, owned by @c detached.
        FreeformFinSet* freeform{nullptr};
        /// The new fin set when the converted one had no parent; null when the parent owns it.
        std::unique_ptr<FreeformFinSet> detached;
        /// The converted fin set when it was taken out of its parent (dropping it destroys it);
        /// null when it had no parent (its owner keeps it).
        std::unique_ptr<RocketComponent> original;
    };

    /// Three fins with the outline (0, 0), (0.025, 0.05), (0.075, 0.05), (0.05, 0).
    FreeformFinSet();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::FREEFORM_FIN_SET;
    }

    /// Converts @p finset into a freeform fin set with the same outline: takes it out of its
    /// parent (if any) and puts a new FreeformFinSet in its place in the child list. The new fin
    /// set gets @p finset's fin points (taken once it is detached), its axial method and offset,
    /// the properties FinSet::copyFrom() copies (among them the id, the name, the comment, the
    /// colour and the overrides) and its appearance; a name that starts with @p finset's
    /// component name gets the freeform one's in its place ("Trapezoidal Fin Set 2" becomes
    /// "Freeform Fin Set 2"). With @p freezeRocket the rocket (when @p finset is in one) is
    /// frozen meanwhile, so that one combined event fires at the end. @p finset must not be used
    /// afterwards, except through the result.
    /// @throws BugError when the rocket is already frozen (see Rocket::freeze()).
    static ConversionResult convertFinSet(FinSet& finset, bool freezeRocket = true);

    /// Inserts a point at @p location before the point @p index and fires NONFUNCTIONAL_CHANGE;
    /// the point is not checked.
    /// Fails ("Cannot add new point before the first or after the last point") unless
    /// 1 <= @p index <= getPointCount() - 1.
    [[nodiscard]] Result<void> addPoint(std::size_t index, Point2D location);

    /// Removes the point @p index, unless the outline would then intersect itself, and fires
    /// AEROMASS_CHANGE (in both cases).
    /// Fails for the first or the last point ("cannot remove first or last point") and beyond
    /// the last one ("index out of range").
    [[nodiscard]] Result<void> removePoint(std::size_t index);

    [[nodiscard]] std::size_t getPointCount() const noexcept { return m_points.size(); }

    /// Replaces the outline: the points are moved so that the first is at the origin, limited to
    /// kSnapLargerThan and then clamped by update(@p validateFinTab). An outline that intersects
    /// itself is given up for the previous points and length. Fires AEROMASS_CHANGE (in both
    /// cases).
    /// @throws BugError when @p newPoints is empty.
    void setPoints(std::span<const Coordinate> newPoints, bool validateFinTab = true);

    /// Moves the point @p index to (@p xRequest, @p yRequest), limited to kSnapLargerThan (the
    /// first point so that the fin is no longer than that), then clamps the outline (update())
    /// and fires AEROMASS_CHANGE. Moving the first point moves the fin: the other points keep
    /// their place on the parent. Nothing happens without a parent, or when the point moves by
    /// less than 1e-12; a move that makes the outline intersect itself is taken back (the point
    /// only, see the class comment) without an event.
    /// Fails ("index out of range") for an index beyond the last point.
    [[nodiscard]] Result<void> setPoint(std::size_t index, double xRequest, double yRequest);

    /// The points; the first and the last lie on the ends of the root when it has more than one
    /// point.
    [[nodiscard]] std::vector<Coordinate> getFinPoints() const override;

    /// The largest y of the outline (at least 0) less the last point's y when that is negative.
    [[nodiscard]] double getSpan() const override;

    /// update(true).
    void update() override;

    /// Recomputes the length (the last point's x less the first's) and the position, and on a
    /// parent clamps the points (see the class comment); when the length changed and
    /// @p validateFinTab is set, shortens a tab that would end behind the fin.
    void update(bool validateFinTab);

    /// Whether any two segments of the outline intersect, segments next to each other excepted
    /// (they share a point), and the first and the last segment when the outline is closed (the
    /// first and the last point coincide).
    [[nodiscard]] bool intersects() const;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// Differences below this are no change (IGNORE_SMALLER_THAN).
    static constexpr double kIgnoreSmallerThan = 1.0e-12;

    /// Puts the first point at the origin and moves every other point by (@p deltaX, @p deltaY).
    void movePoints(double deltaX, double deltaY);

    /// Moves a first point that is not at the origin back to it (see the class comment), and
    /// sets the length to the last point's x.
    void clampFirstPoint();

    /// Lifts the interior point @p index onto the parent's surface when it lies inside it.
    void clampInteriorPoint(std::size_t index);

    /// Puts the last point on the parent's surface; when it was moved by @p xDelta in x, sets the
    /// length and adjusts the offset of a fin positioned MIDDLE or BOTTOM.
    void clampLastPoint(double xDelta = 0);

    /// Whether the segment from point @p targetIndex to the next intersects a later segment.
    [[nodiscard]] bool intersects(std::size_t targetIndex) const;

    std::vector<Coordinate> m_points;
};

}  // namespace QtRocket
