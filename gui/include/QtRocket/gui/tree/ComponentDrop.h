#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <QCoreApplication>
#include <QModelIndex>
#include <QString>
#include <Qt>

#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentMimeData;
class ComponentTreeModel;

/// The drop rules of the component tree (OpenRocket's ComponentTreeTransferHandler: canImport()
/// and importData()): whether the components a drag carries (ComponentMimeData) may go where
/// they are dropped, and moving or copying them there. ComponentTreeModel's canDropMimeData()
/// and dropMimeData() answer with these; the drag itself is ComponentTreeView's.
///
/// - What is dragged: the selected components without a selected ancestor, in tree order; the
///   Rocket is never dragged (ComponentTreeModel::mimeData()).
/// - Where it goes (target()): Qt's drop position as QAbstractItemView measures it, which
///   differs from JTree's (measured by the scouting's drop_position_probe, Qt 6.11): in the top
///   margin of a row, before it in its parent (row, parent); on the middle of a row that takes
///   children, its first child (Qt's row -1, OnItem; Java's index -1, getSourceAndTarget()); in
///   the bottom margin of a row, or on the middle of one that takes none, after it (row + 1,
///   parent), also for an expanded row with children (JTree inserts as the first child there);
///   on the blank space below the rows, the end of the Rocket (Java: the root's end below the
///   last row). Above or below the Rocket's own row is no place.
/// - The operation (operation()): a move for Qt::MoveAction and a copy for Qt::CopyAction
///   (Ctrl held), as Java's; a drag from another document (another window) is always copied,
///   and the view shows the copy cursor for it (Java: canImportTarget() sets COPY).
/// - The rules (isAllowed(), Java's canImportTarget()): the new parent is compatible with every
///   dragged component (RocketComponent::isCompatible()) and is none of them nor inside one.
/// - A move that would leave every component where it is is refused, with nothing changed and
///   no undo step (Java's checkImportAction(): the same parent at the component's own index or
///   the next one).
/// - A move (one undo step, "Move component" or "Move components", hard-coded English in Java,
///   through tr() here): within one freeze of the rocket all dragged components are taken out
///   and then put in, in tree order, at the drop index less the dragged siblings before it
///   (Java's destIndex--), so that they are next to each other in their old order; they keep
///   their identity (and ids), so they stay selected.
/// - A copy ("Copy component(s)"): copies with new ids (ComponentCopy::copyOf(), Java's
///   copy()), with the decal images they show when they come from another document, inserted
///   in tree order at the drop index and after it. A copy that would give the design more
///   components or decal images than QtRocket loads is refused with a message
///   (ComponentLimits, decision O27).
///
/// Deviations from OpenRocket:
/// - A drop of several components is checked whole before anything moves (decision U4): Java's
///   importData() moves them one by one with the source positions taken before the first move,
///   and stops at the first one that would stay where it is, returning false with the earlier
///   ones moved (DndProbe: [NC, BT2] to (Sustainer, 1) moves BT2 and reports a failure;
///   [IT, Chute] to (BT2, 0) moves IT and not Chute). Here every rule is checked first; a move
///   is refused only when no component would change its place; and the components go in
///   together in tree order. Java takes them in descending child position, so that two
///   components of different parents can land in the other order ([Chute, IT] to (BT1, 0):
///   Java BT1[Chute, IT, ...], here BT1[IT, Chute, ...]). A drag of one component gives Java's
///   result in every case of DndProbe.
/// - A move is one freeze of the rocket and so one change event; Java freezes for each
///   component. A copy fires an event for each component, as Java's.
/// - The images of a copy from another document go along (ComponentCopy; Java's copy keeps
///   the source document's image objects, which the target never registers).
/// - The cycle check compares objects; Java compares with equals() (class and id), which would
///   refuse a drop into another document whose component has the dragged one's id.
/// - The bounds of a copy (ComponentLimits) are QtRocket's.
/// - Qt's drop positions (see above): the bottom margin of an expanded row with children is the
///   place after the row, where JTree inserts as the row's first child (the scouting's decision
///   D3: the drop goes where Qt's indicator shows it).
class ComponentDrop
{
    Q_DECLARE_TR_FUNCTIONS(ComponentDrop)

public:
    ComponentDrop() = delete;

    /// Where a drop puts the dragged components: the new parent (a component of the target
    /// document's rocket) and the position among its children before the drop, 0 to its child
    /// count.
    struct Target
    {
        RocketComponent* parent = nullptr;
        std::size_t      index  = 0;
    };

    /// What a drop does.
    enum class Operation
    {
        MOVE,
        COPY,
    };

    /// What drop() did.
    struct Outcome
    {
        /// Whether the components were moved or copied.
        bool dropped = false;
        /// Why a drop the rules allow was refused all the same, for the user (a copy beyond a
        /// bound of ComponentLimits); nullopt otherwise.
        std::optional<UserPrompts::Message> refusal;
    };

    /// The target of a drop on @p model at Qt's position (@p row, @p parent), the arguments of
    /// dropMimeData() (see the class comment); nullopt where nothing can be dropped.
    [[nodiscard]] static std::optional<Target> target(const ComponentTreeModel& model, int row,
                                                      const QModelIndex& parent);

    /// The operation of a drop of @p data into @p document with @p action: always COPY for
    /// another document's components; nullopt for an action other than move and copy.
    [[nodiscard]] static std::optional<Operation> operation(const OpenRocketDocument& document,
                                                            const ComponentMimeData&  data,
                                                            Qt::DropAction            action);

    /// The components @p data carries, in its order: in @p document's rocket for a drag of its
    /// own, else in the source document's. Empty when one of them is not there any more or one
    /// is a rocket (no drag carries one).
    [[nodiscard]] static std::vector<const RocketComponent*> draggedComponents(
        const OpenRocketDocument& document, const ComponentMimeData& data);

    /// Whether the rules let @p components go to @p target (canImportTarget() of each): the
    /// parent is compatible with every one and is none of them nor inside one. False for none.
    [[nodiscard]] static bool isAllowed(std::span<const RocketComponent* const> components,
                                        const Target&                           target);

    /// Whether moving @p components (in tree order) to @p target would leave every one at its
    /// place: all are children of the target's parent, next to each other, and the target's
    /// index is at the first of them or right after one of them.
    [[nodiscard]] static bool isNoOpMove(std::span<const RocketComponent* const> components,
                                         const Target&                           target);

    /// Whether @p data may be dropped into @p document at @p target with @p action
    /// (canImport()): a known operation and the rules (isAllowed()). A move to the same place
    /// passes (drop() refuses it), as Java's canImport() lets it through.
    [[nodiscard]] static bool canDrop(const OpenRocketDocument& document,
                                      const ComponentMimeData& data, Qt::DropAction action,
                                      const Target& target);

    /// Drops @p data into @p document at @p target with @p action (importData()): moves or
    /// copies the components (see the class comment), or refuses.
    /// @throws BugError when @p target's parent is not a component of @p document's rocket
    static Outcome drop(OpenRocketDocument& document, const ComponentMimeData& data,
                        Qt::DropAction action, const Target& target);

private:
    /// Moves @p components of @p document (in tree order) to @p target.
    static void move(OpenRocketDocument&                     document,
                     std::span<const RocketComponent* const> components, const Target& target);
    /// Copies @p components of @p source (in tree order) to @p target in @p document.
    static Outcome copy(OpenRocketDocument& document, const OpenRocketDocument& source,
                        std::span<const RocketComponent* const> components, const Target& target);
};

}  // namespace QtRocket::Gui
