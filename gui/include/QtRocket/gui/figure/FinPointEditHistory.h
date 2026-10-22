#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Gui
{

/// The fin point editor's own undo and redo (OpenRocket's FinPointFigure.FinPointEditHistory,
/// swing gui/scalefigure/FinPointFigure.java): snapshots of the main fin set's points
/// (getFinPoints()), at most maxSize() of them (OpenRocketDocument::kUndoLevels, 50, as Java's
/// UNDO_LEVELS), the current one at position(). It starts with the points the fin set has when it
/// is made (reset()).
///
/// Every edit of the points brackets itself with beginEdit() (the snapshot before) and
/// commitEdit() (the snapshot after, pushed when it differs from the one before: MathUtil::equals
/// on x, y, z and the weight of every point), which discards the redo states and drops the oldest
/// beyond maxSize(). An edit in progress or the history's own undo() and redo() make nested
/// beginEdit() and commitEdit() calls do nothing (isEditInProgress()). undo() and redo() set the
/// snapshot they move to on every selected fin set (FinPointSelection::setPoints(): Java's
/// setPoints() reaches the config listeners), unless an edit is in progress.
///
/// These edits are also part of the configuration dialog's session, the one undo step of the
/// document's history (ComponentConfigDialog).
///
/// The fin sets are found by id at every use: once the main one is gone, beginEdit() and
/// commitEdit() do nothing and undo() and redo() set nothing (a deviation: Java keeps editing the
/// fin set object it has, detached).
class FinPointEditHistory
{
public:
    /// A history of the points of the main fin set of @p target with at most @p maxSize snapshots
    /// (at least 1), holding the current points.
    explicit FinPointEditHistory(FinPointSelection target,
                                 std::size_t       maxSize = OpenRocketDocument::kUndoLevels);

    /// Forgets everything but the current points (reset()).
    void reset();
    /// Forgets everything, the current points too (clear(), when the figure goes).
    void clear();

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    /// Whether an edit has begun and not been committed, or undo() or redo() is applying a
    /// snapshot.
    [[nodiscard]] bool isEditInProgress() const noexcept { return m_editInProgress || m_applying; }

    /// Takes the snapshot before an edit, unless an edit is in progress.
    void beginEdit();
    /// Ends the edit begun: pushes the snapshot after it when it differs from the one before.
    void commitEdit();
    void undo();
    void redo();

    [[nodiscard]] std::size_t maxSize() const noexcept { return m_maxSize; }
    /// The number of snapshots.
    [[nodiscard]] std::size_t size() const noexcept { return m_history.size(); }
    /// The index of the current snapshot; 0 when there is none (after clear()).
    [[nodiscard]] std::size_t position() const noexcept { return m_position; }
    /// The snapshot @p index.
    [[nodiscard]] std::span<const Coordinate> snapshot(std::size_t index) const
    {
        return m_history.at(index);
    }

    /// Whether @p a and @p b hold the same points (snapshotsEqual()).
    [[nodiscard]] static bool snapshotsEqual(std::span<const Coordinate> a,
                                             std::span<const Coordinate> b) noexcept;

private:
    /// The main fin set's points now; nullopt when it is gone.
    [[nodiscard]] std::optional<std::vector<Coordinate>> snapshotCurrent() const;
    void                                                 push(std::vector<Coordinate> snapshot);
    void                                                 applyFromHistory();

    FinPointSelection                      m_target;
    std::size_t                            m_maxSize;
    std::vector<std::vector<Coordinate>>   m_history;
    std::size_t                            m_position{0};
    std::optional<std::vector<Coordinate>> m_pendingBefore;
    bool                                   m_editInProgress{false};
    bool                                   m_applying{false};
};

}  // namespace QtRocket::Gui
