#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class FreeformFinSet;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;

/// The freeform fin sets whose points the fin point editor edits (OpenRocket's FreeformFinSet and
/// its config listeners): the main one, whose points the editor shows, and the others selected
/// with it, kept by id and found again in the rocket at every use, so that no component pointer
/// outlives an undo (Rocket::loadFrom() replaces every component object). That also holds while
/// the editor keeps the rocket frozen during a drag, when the undo's event waits in the freeze
/// and no ComponentSelection has heard of it yet.
///
/// The edits are FreeformFinSet's, applied as OpenRocket's forwarding loops apply them to the
/// config listeners (the user's decision U3: each selected fin set with its own call): the main
/// fin set checks the index first and fails without touching anyone, then every other fin set is
/// edited at the point FreeformFinSet::getConfigListenerPointIdx() gives (left alone when it
/// gives none; its failure ignored, as Java ignores the listener's IllegalFinPointException),
/// then the main one. With other fin sets the edit runs inside one freeze of the rocket (taken
/// only when the rocket is not frozen already), so that it fires one merged event; Java fires
/// each fin set's own. A single fin set is edited without a freeze, its own events as Java's.
///
/// The rocket must outlive the selection (the document owns it; a design window and its dialogs
/// go before their document).
class FinPointSelection
{
public:
    /// The fin sets with the ids @p main and @p others in @p rocket.
    FinPointSelection(Rocket& rocket, const Uuid& main, std::vector<Uuid> others);

    /// The main component of @p selection and those of its others that are freeform fin sets.
    /// @throws BugError when @p selection is invalid or its main component is no FreeformFinSet
    [[nodiscard]] static FinPointSelection of(const ComponentSelection& selection);

    [[nodiscard]] Rocket&                  rocket() const noexcept { return *m_rocket; }
    [[nodiscard]] const Uuid&              mainId() const noexcept { return m_main; }
    [[nodiscard]] const std::vector<Uuid>& otherIds() const noexcept { return m_others; }
    /// Whether other fin sets are selected with the main one.
    [[nodiscard]] bool isMultiEdit() const noexcept { return !m_others.empty(); }

    /// The main fin set as the rocket has it now; null when it is no longer in the rocket.
    [[nodiscard]] FreeformFinSet* main() const;
    /// The other fin sets still in the rocket, in the order of their ids.
    [[nodiscard]] std::vector<FreeformFinSet*> others() const;

    /// FreeformFinSet::setPoint() on every fin set (see the class comment): nothing without the
    /// main fin set or without a parent of it (Java returns before its loop), the main one's
    /// failure for an index beyond its last point.
    [[nodiscard]] Result<void> setPoint(std::size_t index, double x, double y) const;
    /// FreeformFinSet::addPoint() on every fin set: the main one's failure, before anything
    /// changes, for an index outside 1 ... its last point.
    [[nodiscard]] Result<void> addPoint(std::size_t index, Point2D location) const;
    /// FreeformFinSet::removePoint() on every fin set: the main one's failure, before anything
    /// changes, for its first or last point or an index beyond them.
    [[nodiscard]] Result<void> removePoint(std::size_t index) const;
    /// FreeformFinSet::setPoints() with @p points on every fin set, the others first (Java's
    /// setPoints() hands the same points to each config listener).
    /// @throws BugError when @p points is empty
    void setPoints(std::span<const Coordinate> points) const;

private:
    Rocket*           m_rocket;
    Uuid              m_main;
    std::vector<Uuid> m_others;
};

}  // namespace QtRocket::Gui
