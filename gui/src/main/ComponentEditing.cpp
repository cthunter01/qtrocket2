#include "QtRocket/gui/main/ComponentEditing.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QString>
#include <QWidget>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/tree/ComponentLimits.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentList = std::vector<const RocketComponent*>;

/// The position of @p component among its parent's children, 0 without a parent (the key of
/// Java's sorts: c.getParent() != null ? c.getParent().getChildPosition(c) : 0).
std::size_t childPosition(const RocketComponent* component)
{
    const RocketComponent* parent = component->getParent();
    return parent == nullptr ? 0 : parent->getChildPosition(component).value_or(0);
}

/// Sorts @p components by ascending child position, ties kept in their order (Java's
/// List.sort() is stable).
void sortAscending(ComponentList& components)
{
    std::ranges::stable_sort(components, std::ranges::less{}, childPosition);
}

/// Sorts @p components by descending child position, ties kept in their order (Java's sort by
/// the negated position).
void sortDescending(ComponentList& components)
{
    std::ranges::stable_sort(components, std::ranges::greater{}, childPosition);
}

/// Whether @p components holds @p component.
bool contains(const ComponentList& components, const RocketComponent* component)
{
    return std::ranges::find(components, component) != components.end();
}

/// The components of @p components without an ancestor among them, in their order.
ComponentList topsOf(const ComponentList& components)
{
    ComponentList tops;
    for (const RocketComponent* component : components)
    {
        if (!RocketComponent::listContainsParent(components, *component))
        {
            tops.push_back(component);
        }
    }
    return tops;
}

/// The ids of @p components, in their order.
template <class Component>
std::vector<Uuid> idsOf(const std::vector<Component*>& components)
{
    std::vector<Uuid> ids;
    ids.reserve(components.size());
    for (const Component* component : components)
    {
        ids.push_back(component->getId());
    }
    return ids;
}

/// The ids of the components of @p copy that @p placements place, in their order.
std::vector<Uuid> idsOf(const ComponentCopy&                         copy,
                        const std::vector<ComponentCopy::Placement>& placements)
{
    std::vector<Uuid> ids;
    ids.reserve(placements.size());
    for (const ComponentCopy::Placement& placement : placements)
    {
        ids.push_back(copy.getComponent(placement.component).getId());
    }
    return ids;
}

/// The name of @p component's class as OpenRocket's undo texts have it (getComponentName(),
/// translated).
QString nameOf(const RocketComponent& component)
{
    return enumText(component.kind());
}

/// selectAllUnselectedInParent(): when none of @p parent's children is in @p selections, adds
/// them all; otherwise does the same for each child that is in it.
void selectAllUnselectedInParent(ComponentList& selections, const RocketComponent& parent)
{
    const ComponentList children           = parent.getChildren();
    const bool          noChildrenSelected = std::ranges::none_of(
        children,
        [&selections](const RocketComponent* child) { return contains(selections, child); });
    if (noChildrenSelected)
    {
        selections.insert(selections.end(), children.begin(), children.end());
        return;
    }
    for (const RocketComponent* child : children)
    {
        if (contains(selections, child))
        {
            selectAllUnselectedInParent(selections, *child);
        }
    }
}

/// Removes from @p copy, a copy of @p original (one of @p listed), the children whose originals
/// are not in @p listed when some are, and does the same below for those it keeps (the rule of
/// filterClipboardComponent()).
void keepListedChildren(const RocketComponent& original, RocketComponent& copy,
                        const std::unordered_set<const RocketComponent*>& listed)
{
    const ComponentList children = original.getChildren();
    if (std::ranges::none_of(
            children, [&listed](const RocketComponent* child) { return listed.contains(child); }))
    {
        return;
    }
    for (std::size_t i = children.size(); i-- > 0;)
    {
        if (!listed.contains(children[i]))
        {
            // A detached copy: no rocket hears of it.
            static_cast<void>(copy.removeChild(i, RocketComponent::StageTracking::SKIP));
        }
    }
    std::size_t kept = 0;
    for (const RocketComponent* child : children)
    {
        if (listed.contains(child))
        {
            keepListedChildren(*child, copy.getChild(kept), listed);
            ++kept;
        }
    }
}

/// Removes each of the components @p ids from @p rocket, in this order, that is still in it and
/// deletable then (DeleteAction.deleteComponent()). The removed components are destroyed at the
/// end, after every removal has been told.
void removeEach(Rocket& rocket, const std::vector<Uuid>& ids)
{
    std::vector<std::unique_ptr<RocketComponent>> removed;
    for (const Uuid& id : ids)
    {
        RocketComponent* component = rocket.findComponent(id);
        if (component != nullptr && ComponentEditing::isDeletable(*component))
        {
            removed.push_back(component->getParent()->removeChild(component));
        }
    }
}

}  // namespace

ComponentEditing::ComponentEditing(DocumentSelectionModel& selection,
                                   OpenRocketClipboard& clipboard, UserPrompts& prompts,
                                   QWidget* dialogParent)
  : m_selection(&selection),
    m_clipboard(&clipboard),
    m_prompts(&prompts),
    m_dialogParent(dialogParent)
{
}

OpenRocketDocument& ComponentEditing::document() const noexcept
{
    return m_selection->document();
}

// ---------------------------------------------------------------------- the enable rules

bool ComponentEditing::canCut() const
{
    const ComponentList selection = selected();
    return !selection.empty() && std::ranges::all_of(selection, [](const RocketComponent* c) {
        return isDeletable(*c) && isCopyable(*c);
    });
}

bool ComponentEditing::canCopy() const
{
    const RocketComponent* first = m_selection->selectedComponent();
    return first != nullptr && isCopyable(*first);
}

bool ComponentEditing::canPaste() const
{
    return !m_clipboard->isEmpty();
}

bool ComponentEditing::canDuplicate() const
{
    return canCopy();
}

bool ComponentEditing::canDelete() const
{
    const RocketComponent* first = m_selection->selectedComponent();
    return first != nullptr && isDeletable(*first);
}

bool ComponentEditing::canMoveUp() const
{
    return canMoveUp(selected());
}

bool ComponentEditing::canMoveDown() const
{
    return canMoveDown(selected());
}

// -------------------------------------------------------------------------- the operations

bool ComponentEditing::cut()
{
    if (!canCut())
    {
        return false;
    }
    ComponentList completed = fillInMissingSelections(selected());
    sortDescending(completed);
    if (!std::ranges::all_of(
            completed, [](const RocketComponent* c) { return isDeletable(*c) && isCopyable(*c); }))
    {
        return false;
    }
    //: Hard-coded English in OpenRocket (RocketActions.CutAction): "Cut " + getComponentName()
    const QString description = completed.size() == 1 ? tr("Cut %1").arg(nameOf(*completed.front()))
                                                      //: Hard-coded English in OpenRocket
                                                      : tr("Cut components");
    std::optional<ComponentCopy> copied = copyOf(completed, description);
    if (!copied.has_value())
    {
        return false;
    }
    // The tops go (their subtrees with them), by Delete's rule and order (decision U4).
    ComponentList tops = topsOf(completed);
    sortAscending(tops);
    const std::vector<Uuid> ids = idsOf(tops);
    document().addUndoPosition(QtStrings::toUtf8(description));
    m_clipboard->setComponents(std::move(*copied));
    removeEach(document().getRocket(), ids);
    return true;
}

bool ComponentEditing::copy()
{
    if (!canCopy())
    {
        return false;
    }
    ComponentList completed = fillInMissingSelections(selected());
    sortDescending(completed);
    if (!std::ranges::all_of(completed, [](const RocketComponent* c) { return isCopyable(*c); }))
    {
        return false;
    }
    //: RocketActions.CopyAct.Copy
    std::optional<ComponentCopy> copied = copyOf(completed, tr("Copy"));
    if (!copied.has_value())
    {
        return false;
    }
    m_clipboard->setComponents(std::move(*copied));
    return true;
}

bool ComponentEditing::paste()
{
    if (!canPaste())
    {
        return false;
    }
    RocketComponent* destination = m_selection->selectedComponent();
    ComponentCopy    pasted      = m_clipboard->components().copy();
    // Every position before anything goes in (getPastePositions()).
    std::vector<ComponentCopy::Placement> placements;
    ComponentLimits::Addition             addition;
    for (std::size_t i = 0; i < pasted.size(); ++i)
    {
        const std::optional<PastePosition> position =
            pastePosition(pasted.getComponent(i), destination);
        if (position.has_value())
        {
            placements.push_back(
                {.component = i, .parent = position->parent, .index = position->index});
            addition += ComponentLimits::placing(pasted.getComponent(i), *position->parent);
        }
    }
    if (placements.empty())
    {
        // Nothing fits: the selection is what was pasted, nothing (setSelectedComponents()).
        m_selection->clearComponentSelection();
        return false;
    }
    const QString description = pasted.size() == 1
                                    //: Hard-coded English in OpenRocket
                                    //: (RocketActions.PasteAction): "Paste " + getComponentName()
                                    ? tr("Paste %1").arg(nameOf(pasted.getComponent(0)))
                                    //: Hard-coded English in OpenRocket
                                    : tr("Paste components");
    addition.decalImages      = pasted.countNewDecalImages(document(), placements);
    if (!fits(addition, description))
    {
        return false;
    }
    // The ids of what goes in, taken before: a slot of an insertion's event may change the tree.
    const std::vector<Uuid> ids = idsOf(pasted, placements);
    document().addUndoPosition(QtStrings::toUtf8(description));
    std::ignore = std::move(pasted).insertInto(document(), placements);
    select(ids);
    return true;
}

bool ComponentEditing::duplicate()
{
    if (!canDuplicate())
    {
        return false;
    }
    ComponentList sorted = selected();
    sortAscending(sorted);
    const ComponentList completed = fillInMissingSelections(sorted);
    if (!std::ranges::all_of(completed, [](const RocketComponent* c) { return isCopyable(*c); }))
    {
        return false;
    }
    const ComponentList tops = topsOf(completed);
    const QString       description =
        tops.size() == 1
            //: Hard-coded English in OpenRocket (RocketActions.DuplicateAction): "Duplicate " +
            //: getComponentName()
            ? tr("Duplicate %1").arg(nameOf(*tops.front()))
            //: Hard-coded English in OpenRocket
            : tr("Duplicate components");
    std::optional<ComponentCopy> copied = copyOf(completed, description);
    if (!copied.has_value())
    {
        return false;
    }
    ComponentCopy duplicates = copied->copy();
    // Each at the end of its original's parent, the positions taken first, the last one in
    // first (Java reverses both lists), so that the duplicates of one parent keep their order.
    Rocket&                               rocket = document().getRocket();
    std::vector<ComponentCopy::Placement> placements;
    ComponentLimits::Addition             addition;
    for (std::size_t k = tops.size(); k-- > 0;)
    {
        RocketComponent* parent = rocket.findComponent(tops[k]->getParent()->getId());
        placements.push_back({.component = k, .parent = parent, .index = parent->getChildCount()});
        // The copy, which holds what was selected of the original's children (the copy rule).
        addition += ComponentLimits::placing(duplicates.getComponent(k), *parent);
    }
    addition.decalImages = duplicates.countNewDecalImages(document(), placements);
    if (!fits(addition, description))
    {
        return false;
    }
    const std::vector<Uuid> ids = idsOf(duplicates, placements);
    document().addUndoPosition(QtStrings::toUtf8(description));
    std::ignore = std::move(duplicates).insertInto(document(), placements);
    // On the clipboard once the duplicates are in (Java: before), so that a slot of the
    // clipboard's change cannot take a parent of the placements away first.
    m_clipboard->setComponents(std::move(*copied));
    select(ids);
    return true;
}

bool ComponentEditing::deleteSelection()
{
    if (!canDelete())
    {
        return false;
    }
    ComponentList selection = selected();
    // A selected Rocket is left out (decision U4; Java's sort dies on its null parent).
    std::erase_if(selection, [](const RocketComponent* c) { return c->getParent() == nullptr; });
    sortAscending(selection);
    const QString description = selection.size() == 1
                                    //: Hard-coded English in OpenRocket
                                    //: (RocketActions.DeleteAction): "Delete " + getComponentName()
                                    ? tr("Delete %1").arg(nameOf(*selection.front()))
                                    //: Hard-coded English in OpenRocket
                                    : tr("Delete components");
    document().addUndoPosition(QtStrings::toUtf8(description));
    removeEach(document().getRocket(), idsOf(selection));
    return true;
}

bool ComponentEditing::moveUp()
{
    return move(-1);
}

bool ComponentEditing::moveDown()
{
    return move(1);
}

bool ComponentEditing::move(int step)
{
    ComponentList sorted = selected();
    if (step < 0 ? !canMoveUp(sorted) : !canMoveDown(sorted))
    {
        return false;
    }
    if (step < 0)
    {
        sortAscending(sorted);
    }
    else
    {
        sortDescending(sorted);
    }
    const QString description =
        sorted.size() == 1
            //: Hard-coded English in OpenRocket (RocketActions.MoveUpAction, MoveDownAction):
            //: "Move " + getComponentName()
            ? tr("Move %1").arg(nameOf(*sorted.front()))
            //: Hard-coded English in OpenRocket
            : tr("Move components");
    // Only the tops move; their children go with them.
    const std::vector<Uuid> tops = idsOf(topsOf(sorted));
    const std::vector<Uuid> all  = idsOf(sorted);
    document().addUndoPosition(QtStrings::toUtf8(description));
    Rocket& rocket = document().getRocket();
    for (const Uuid& id : tops)
    {
        RocketComponent* component = rocket.findComponent(id);
        if (component == nullptr || component->getParent() == nullptr)
        {
            continue;
        }
        // Each one asked again (moveUp()/moveDown(): canMove(component)).
        const ComponentList one{component};
        if (step < 0 ? canMoveUp(one) : canMoveDown(one))
        {
            RocketComponent* parent = component->getParent();
            parent->moveChild(
                component, step < 0 ? childPosition(component) - 1 : childPosition(component) + 1);
        }
    }
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    select(all);
    return true;
}

// ---------------------------------------------------------------- the rules of components

bool ComponentEditing::isDeletable(const RocketComponent& component)
{
    const RocketComponent* parent = component.getParent();
    if (parent == nullptr || component.kind() == ComponentKind::ROCKET)
    {
        return false;
    }
    // The last stage cannot go; a booster set can (an AxialStage that is no ParallelStage).
    return component.kind() != ComponentKind::AXIAL_STAGE || parent->getChildCount() != 1;
}

bool ComponentEditing::isCopyable(const RocketComponent& component)
{
    return component.kind() != ComponentKind::ROCKET;
}

std::vector<const RocketComponent*> ComponentEditing::fillInMissingSelections(
    std::span<const RocketComponent* const> selection)
{
    ComponentList       selections(selection.begin(), selection.end());
    const ComponentList initial = selections;
    for (const RocketComponent* component : initial)
    {
        selectAllUnselectedInParent(selections, *component);
        // The parents between a selected component and its selected ancestor.
        const RocketComponent* temp = component;
        if (RocketComponent::listContainsParent(selections, *temp) &&
            !contains(selections, temp->getParent()))
        {
            while (!contains(selections, temp->getParent()))
            {
                selections.push_back(temp->getParent());
                temp = temp->getParent();
            }
        }
    }
    return selections;
}

std::vector<std::unique_ptr<RocketComponent>> ComponentEditing::copyComponentsMaintainParent(
    std::span<const RocketComponent* const> components)
{
    const std::unordered_set<const RocketComponent*> listed(components.begin(), components.end());
    std::vector<std::unique_ptr<RocketComponent>>    copies;
    for (const RocketComponent* component : components)
    {
        if (RocketComponent::listContainsParent(components, *component))
        {
            continue;
        }
        std::unique_ptr<RocketComponent> tree = component->copyWithNewIds();
        keepListedChildren(*component, *tree, listed);
        copies.push_back(std::move(tree));
    }
    return copies;
}

std::optional<ComponentEditing::PastePosition> ComponentEditing::pastePosition(
    const RocketComponent& pasted, RocketComponent* destination)
{
    if (destination == nullptr)
    {
        return std::nullopt;
    }
    if (destination->isCompatible(pasted))
    {
        return PastePosition{.parent = destination, .index = destination->getChildCount()};
    }
    RocketComponent* parent = destination->getParent();
    if (parent != nullptr && parent->isCompatible(pasted))
    {
        return PastePosition{.parent = parent, .index = childPosition(destination) + 1};
    }
    return std::nullopt;
}

bool ComponentEditing::canMoveUp(std::span<const RocketComponent* const> components)
{
    return !components.empty() &&
           std::ranges::all_of(components, [components](const RocketComponent* component) {
               return RocketComponent::listContainsParent(components, *component) ||
                      (component->getParent() != nullptr && childPosition(component) > 0);
           });
}

bool ComponentEditing::canMoveDown(std::span<const RocketComponent* const> components)
{
    return !components.empty() &&
           std::ranges::all_of(components, [components](const RocketComponent* component) {
               const RocketComponent* parent = component->getParent();
               return RocketComponent::listContainsParent(components, *component) ||
                      (parent != nullptr && childPosition(component) + 1 < parent->getChildCount());
           });
}

// ------------------------------------------------------------------------------ private

std::vector<const RocketComponent*> ComponentEditing::selected() const
{
    const std::vector<RocketComponent*> selection = m_selection->selectedComponents();
    return {selection.begin(), selection.end()};
}

std::optional<ComponentCopy> ComponentEditing::copyOf(
    std::span<const RocketComponent* const> completed, const QString& title) const
{
    Result<ComponentCopy> copied =
        ComponentCopy::fromDetached(document(), copyComponentsMaintainParent(completed));
    if (!copied.has_value())
    {
        m_prompts->showMessage(m_dialogParent.data(),
                               {.severity = UserPrompts::Severity::ERROR,
                                .title    = title,
                                .lines    = {QtStrings::fromUtf8(copied.error().message)}});
        return std::nullopt;
    }
    return std::move(*copied);
}

bool ComponentEditing::fits(const ComponentLimits::Addition& addition, const QString& title) const
{
    const std::optional<UserPrompts::Message> refusal =
        ComponentLimits::refusal(document(), addition, title);
    if (refusal.has_value())
    {
        m_prompts->showMessage(m_dialogParent.data(), *refusal);
        return false;
    }
    return true;
}

void ComponentEditing::select(const std::vector<Uuid>& ids) const
{
    Rocket&                       rocket = document().getRocket();
    std::vector<RocketComponent*> components;
    components.reserve(ids.size());
    for (const Uuid& id : ids)
    {
        components.push_back(rocket.findComponent(id));
    }
    std::erase(components, nullptr);
    m_selection->setSelectedComponents(components);
}

}  // namespace QtRocket::Gui
