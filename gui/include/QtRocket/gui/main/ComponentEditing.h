#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DocumentSelectionModel;
class OpenRocketClipboard;
class UserPrompts;

/// What the component actions of a design window do with the selected components (the component
/// half of OpenRocket's RocketActions: Cut, Copy, Paste, Duplicate, Delete, Move up, Move down,
/// with the helpers of lines 328-563): the rules that enable each (canCut(), ...), the
/// operations, and the rules as functions of components, for the tests and for whoever needs
/// them. The QActions, their texts, icons and keys, Edit and Deselect all are RocketActions';
/// it closes the configuration dialog before an operation, as Java's actions do.
///
/// The selection is the window's DocumentSelectionModel, in the order of selection: its first
/// component decides what can be copied, duplicated or deleted and where a paste goes.
///
/// - Copy: the selected components without a selected ancestor ("tops"), each copied with its
///   subtree, except that the selection decides within it: a copied component of which some
///   children are part of the completed selection keeps only those (fillInMissingSelections()
///   and copyComponentsMaintainParent(), which together give what Java's clipboard ends up with
///   after its filter). The tops are in descending child position within their parent, ties in
///   the order of selection (Java's sort). Not available with the Rocket among the selection.
/// - Paste: a further copy of the clipboard (new ids, the decal images along), each component
///   placed by pastePosition() from the first selected component, all positions taken before
///   anything goes in; one undo step, "Paste <component name>" for one component on the
///   clipboard, else "Paste components"; the pasted components become the selection (none
///   when nothing fitted).
/// - Duplicate: the copy of Copy (ascending child position this time), which also becomes the
///   clipboard (Java: a later Paste pastes the duplicates' originals), and a further copy of it
///   inserted at the end of each top's parent; "Duplicate <name>" / "Duplicate components"; the
///   duplicates become the selection, in Java's (reversed) order.
/// - Delete: the selected components in ascending child position, each removed when it is
///   still deletable then (isDeletable(): so of all the stages the last one stays); "Delete
///   <name>" / "Delete components" by the number selected; the tree then selects the nearest
///   remaining ancestor (ComponentTreeView).
/// - Cut: Copy, then Delete of the tops; "Cut <name>" only when the completed selection is one
///   component, else "Cut components".
/// - Move up / Move down: each top one place up or down within its parent (in ascending or
///   descending child position), one undo step "Move <name>" / "Move components", then one more
///   TREE_CHANGE event and the selection set again, in that order (Java's).
/// Each operation first asks its rule: an operation whose action would be disabled does nothing
/// and returns false (Java's actions cannot be triggered then). The undo texts are hard-coded
/// English in Java; here through tr(), the component name through enumText() (Java's
/// getComponentName() is translated).
///
/// Bounds (decision O27): a paste or a duplicate that would give the rocket more components or
/// the document more decal images than QtRocket loads (ComponentLimits) is refused with a
/// message through UserPrompts, before anything changes (the clipboard too).
///
/// No component pointer is kept across a change of the rocket: the operations work through ids
/// between the changes, so that a listener that changes the tree cannot leave them a freed
/// component.
///
/// Deviations from OpenRocket:
/// - Decision U4: no exception for a Cut or Delete of all the stages, of a stage holding a
///   booster set, or with the Rocket selected. Java's Cut deletes the whole completed selection
///   one by one and throws when a detached booster set is removed ("cut [Booster]",
///   IllegalStateException) or a stage becomes the last ("cut [Sustainer, Booster]",
///   IllegalArgumentException, half done); its Delete sorts with the Rocket's null parent
///   (NullPointerException). Here Cut deletes the tops by Delete's rule (so cutting all the
///   stages keeps the last, as Delete does, and the clipboard has them all), and Delete leaves
///   a selected Rocket out.
/// - Operations whose rule says no do nothing (see above); Java's do what they can when called
///   directly (the scouting's ActionsProbe calls a disabled Move up, which moves the others).
/// - A paste that fits nowhere adds no undo position; Java adds one (its description is then
///   given to the next change). It clears the selection, as Java's.
/// - Duplicate puts its copy on the clipboard once the duplicates are in (Java: before); the
///   result is the same, and a slot of the clipboard's change cannot take a parent away first.
/// - The decal images go along with copies into another document (ComponentCopy).
/// - The bounds above are QtRocket's.
class ComponentEditing
{
    Q_DECLARE_TR_FUNCTIONS(ComponentEditing)

public:
    /// Where a pasted component goes: the new parent and its position there.
    struct PastePosition
    {
        RocketComponent* parent = nullptr;
        std::size_t      index  = 0;
    };

    /// The editing of the components @p selection selects, through @p clipboard, telling the
    /// user through @p prompts with @p dialogParent as the messages' parent. All three must
    /// outlive this object.
    ComponentEditing(DocumentSelectionModel& selection, OpenRocketClipboard& clipboard,
                     UserPrompts& prompts, QWidget* dialogParent = nullptr);

    [[nodiscard]] DocumentSelectionModel& selection() const noexcept { return *m_selection; }
    [[nodiscard]] OpenRocketClipboard&    clipboard() const noexcept { return *m_clipboard; }
    /// The document of the selection.
    [[nodiscard]] OpenRocketDocument& document() const noexcept;

    // ------------------------------------------------------------------- the enable rules

    /// Every selected component deletable and copyable (CutAction).
    [[nodiscard]] bool canCut() const;
    /// The first selected component copyable (CopyAction).
    [[nodiscard]] bool canCopy() const;
    /// Something on the clipboard (PasteAction: getPastePositions() has an entry for each).
    [[nodiscard]] bool canPaste() const;
    /// The first selected component copyable (DuplicateAction).
    [[nodiscard]] bool canDuplicate() const;
    /// The first selected component deletable (DeleteAction).
    [[nodiscard]] bool canDelete() const;
    /// Some component selected, and every top one can move up (MoveUpAction.canMove()).
    [[nodiscard]] bool canMoveUp() const;
    /// Some component selected, and every top one can move down.
    [[nodiscard]] bool canMoveDown() const;

    // ----------------------------------------------------------------------- the operations

    /// Cut (cutComponents()); true when something was cut.
    bool cut();
    /// Copy (copyComponents()); true when the clipboard was set.
    bool copy();
    /// Paste (pasteComponents()); true when something was pasted.
    bool paste();
    /// Duplicate (duplicateComponents()); true when something was duplicated.
    bool duplicate();
    /// Delete (deleteComponents()); true when something was deleted.
    bool deleteSelection();
    /// Move up; true when the undo step was made.
    bool moveUp();
    /// Move down; true when the undo step was made.
    bool moveDown();

    // ------------------------------------------------------------- the rules of components

    /// Whether @p component may be deleted (isDeletable()): it has a parent, is not the Rocket,
    /// and is not a stage (a booster set may) that is its parent's only child.
    [[nodiscard]] static bool isDeletable(const RocketComponent& component);

    /// Whether @p component may be copied (isCopyable()): it is not the Rocket.
    [[nodiscard]] static bool isCopyable(const RocketComponent& component);

    /// @p selection completed as Java's Cut, Copy and Duplicate complete it
    /// (fillInMissingSelections()): for each selected component, when none of its children is
    /// selected, all its children are added (one level), and otherwise the same is done for each
    /// selected child, recursively (selectAllUnselectedInParent()); and the parents between a
    /// selected component and a selected ancestor of it are added. The selection first, then
    /// what was added, each once.
    [[nodiscard]] static std::vector<const RocketComponent*> fillInMissingSelections(
        std::span<const RocketComponent* const> selection);

    /// The copies of @p components (Java's copyComponentsMaintainParent() with the clipboard's
    /// filter, filterClipboardComponents()): for each component without an ancestor among
    /// them, in their order, a copy of its subtree with new ids
    /// (RocketComponent::copyWithNewIds()) in which every component that is one of
    /// @p components and has children among them keeps only those children; the others keep
    /// all theirs.
    [[nodiscard]] static std::vector<std::unique_ptr<RocketComponent>> copyComponentsMaintainParent(
        std::span<const RocketComponent* const> components);

    /// Where @p pasted goes when @p destination is selected (getPastePosition()): the end of
    /// @p destination when it takes it, else right after @p destination in its parent when that
    /// takes it, else nowhere (also for no destination).
    [[nodiscard]] static std::optional<PastePosition> pastePosition(const RocketComponent& pasted,
                                                                    RocketComponent* destination);

    /// Whether every component of @p components without an ancestor among them can move one
    /// place up in its parent (MoveUpAction.canMove(List)); false for none.
    [[nodiscard]] static bool canMoveUp(std::span<const RocketComponent* const> components);

    /// The same for down (MoveDownAction.canMove(List)).
    [[nodiscard]] static bool canMoveDown(std::span<const RocketComponent* const> components);

private:
    /// The selected components, in the order of selection.
    [[nodiscard]] std::vector<const RocketComponent*> selected() const;
    /// The clipboard's copy of @p completed, a completed selection in its order
    /// (copyComponentsMaintainParent()), with the decal images it shows; nullopt after telling
    /// the user why there is none, under @p title.
    [[nodiscard]] std::optional<ComponentCopy> copyOf(
        std::span<const RocketComponent* const> completed, const QString& title) const;
    /// Whether the rocket can take @p newComponents more components and the document
    /// @p newDecalImages more images; tells the user why not under @p title.
    [[nodiscard]] bool fits(std::size_t newComponents, std::size_t newDecalImages,
                            const QString& title) const;
    /// Selects the components @p ids that are in the rocket, in their order.
    void select(const std::vector<Uuid>& ids) const;
    /// Moves every top selected component @p step places (-1 up, +1 down) within its parent.
    bool move(int step);

    DocumentSelectionModel* m_selection;
    OpenRocketClipboard*    m_clipboard;
    UserPrompts*            m_prompts;
    QPointer<QWidget>       m_dialogParent;
};

}  // namespace QtRocket::Gui
