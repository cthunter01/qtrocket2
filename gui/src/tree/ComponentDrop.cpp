#include "QtRocket/gui/tree/ComponentDrop.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <Qt>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/tree/ComponentLimits.h"
#include "QtRocket/gui/tree/ComponentMimeData.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// @p components in the pre-order of the tree of @p root, those with an ancestor among them
/// left out (they go with it).
std::vector<const RocketComponent*> topsInTreeOrder(
    const RocketComponent& root, const std::vector<const RocketComponent*>& components)
{
    const std::unordered_set<const RocketComponent*> wanted(components.begin(), components.end());
    std::vector<const RocketComponent*>              ordered;
    for (const RocketComponent* component : root.getAllChildren())
    {
        if (wanted.contains(component) &&
            !RocketComponent::listContainsParent(components, *component))
        {
            ordered.push_back(component);
        }
    }
    return ordered;
}

/// The position of @p component among its parent's children (it has a parent).
std::size_t positionOf(const RocketComponent& component)
{
    const std::optional<std::size_t> position = component.getParent()->getChildPosition(&component);
    QTROCKET_ASSERT(position.has_value());
    return *position;
}

/// Takes the components @p ids out of @p rocket and puts them into the component @p parentId at
/// @p index and after it, in their order, within one freeze (Java's importDataTarget() for each,
/// which freezes for each). The rocket is thawed also when this throws.
void moveFrozen(Rocket& rocket, const std::vector<Uuid>& ids, const Uuid& parentId,
                std::size_t index)
{
    // Declared before the freeze: should an insertion fail, what is left of them is destroyed
    // only after thaw() has told everybody.
    std::vector<std::unique_ptr<RocketComponent>> moving;
    moving.reserve(ids.size());
    rocket.freeze();
    try
    {
        for (const Uuid& id : ids)
        {
            RocketComponent* component = rocket.findComponent(id);
            QTROCKET_ASSERT(component != nullptr && component->getParent() != nullptr);
            moving.push_back(component->getParent()->removeChild(component));
        }
        RocketComponent* parent = rocket.findComponent(parentId);
        QTROCKET_ASSERT(parent != nullptr);
        for (std::size_t i = 0; i < moving.size(); ++i)
        {
            parent->addChild(std::move(moving[i]), index + i);
        }
    }
    catch (...)
    {
        rocket.thaw();
        throw;
    }
    rocket.thaw();
}

}  // namespace

std::optional<ComponentDrop::Target> ComponentDrop::target(const ComponentTreeModel& model, int row,
                                                           const QModelIndex& parent)
{
    if (!parent.isValid())
    {
        // The blank space below the rows (Qt's OnViewport, row -1): the end of the Rocket, as
        // JTree's drop location below the last row. Above or below the Rocket's own row (a row
        // of the invalid root): no place.
        RocketComponent* rocket = model.component(model.rocketIndex());
        if (row != -1 || rocket == nullptr)
        {
            return std::nullopt;
        }
        return Target{.parent = rocket, .index = rocket->getChildCount()};
    }
    RocketComponent* component = model.component(parent);
    if (component == nullptr || row < -1)
    {
        return std::nullopt;
    }
    // On the middle of a row (Qt's row -1): its first child (Java's getSourceAndTarget(): index
    // -1 becomes 0).
    const std::size_t index = row == -1 ? 0 : static_cast<std::size_t>(row);
    if (index > component->getChildCount())
    {
        return std::nullopt;
    }
    return Target{.parent = component, .index = index};
}

std::optional<ComponentDrop::Operation> ComponentDrop::operation(const OpenRocketDocument& document,
                                                                 const ComponentMimeData&  data,
                                                                 Qt::DropAction            action)
{
    if (action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return std::nullopt;
    }
    if (!data.isFrom(document))
    {
        // Into another rocket always a copy (canImportTarget(), importData()).
        return Operation::COPY;
    }
    return action == Qt::MoveAction ? Operation::MOVE : Operation::COPY;
}

std::vector<const RocketComponent*> ComponentDrop::draggedComponents(
    const OpenRocketDocument& document, const ComponentMimeData& data)
{
    const Rocket& rocket =
        data.isFrom(document) ? document.getRocket() : data.sourceDocument().getRocket();
    std::vector<const RocketComponent*> found;
    found.reserve(data.componentIds().size());
    for (const Uuid& id : data.componentIds())
    {
        const RocketComponent* component = rocket.findComponent(id);
        if (component == nullptr || component->getParent() == nullptr)
        {
            // Gone since the drag began, or the Rocket (getSourceAndTarget(): no source parent).
            return {};
        }
        found.push_back(component);
    }
    // In tree order, as the drag made them: the tree may have changed since.
    return topsInTreeOrder(rocket, found);
}

bool ComponentDrop::isAllowed(std::span<const RocketComponent* const> components,
                              const Target&                           target)
{
    if (components.empty() || target.parent == nullptr)
    {
        return false;
    }
    return std::ranges::all_of(components, [&target](const RocketComponent* component) {
        // Compatible, and no cycle: the new parent is not the component nor inside it.
        return target.parent->isCompatible(*component) && component != target.parent &&
               !component->isAncestor(*target.parent);
    });
}

bool ComponentDrop::isNoOpMove(std::span<const RocketComponent* const> components,
                               const Target&                           target)
{
    if (components.empty())
    {
        return false;
    }
    std::size_t first = 0;
    for (std::size_t k = 0; k < components.size(); ++k)
    {
        if (components[k]->getParent() != target.parent)
        {
            return false;
        }
        const std::size_t position = positionOf(*components[k]);
        if (k == 0)
        {
            first = position;
        }
        else if (position != first + k)
        {
            return false;
        }
    }
    return target.index >= first && target.index <= first + components.size();
}

bool ComponentDrop::canDrop(const OpenRocketDocument& document, const ComponentMimeData& data,
                            Qt::DropAction action, const Target& target)
{
    return operation(document, data, action).has_value() &&
           isAllowed(draggedComponents(document, data), target);
}

ComponentDrop::Outcome ComponentDrop::drop(OpenRocketDocument&      document,
                                           const ComponentMimeData& data, Qt::DropAction action,
                                           const Target& target)
{
    QTROCKET_ASSERT(target.parent != nullptr &&
                    &target.parent->getRoot() == &document.getRocket() &&
                    target.index <= target.parent->getChildCount());
    const std::optional<Operation> dropOperation = operation(document, data, action);
    if (!dropOperation.has_value())
    {
        return {};
    }
    const std::vector<const RocketComponent*> dragged = draggedComponents(document, data);
    if (!isAllowed(dragged, target))
    {
        return {};
    }
    if (*dropOperation == Operation::MOVE)
    {
        if (isNoOpMove(dragged, target))
        {
            // checkImportAction(): dropped at the same place.
            return {};
        }
        move(document, dragged, target);
        return {.dropped = true, .refusal = std::nullopt};
    }
    return copy(document, data.isFrom(document) ? document : data.sourceDocument(), dragged,
                target);
}

void ComponentDrop::move(OpenRocketDocument&                     document,
                         std::span<const RocketComponent* const> components, const Target& target)
{
    // The drop index once the dragged siblings before it are out (Java's destIndex--).
    const auto before = static_cast<std::size_t>(
        std::ranges::count_if(components, [&target](const RocketComponent* component) {
            return component->getParent() == target.parent && positionOf(*component) < target.index;
        }));
    std::vector<Uuid> ids;
    ids.reserve(components.size());
    for (const RocketComponent* component : components)
    {
        ids.push_back(component->getId());
    }
    //: Hard-coded English in OpenRocket (ComponentTreeTransferHandler.importData())
    const QString description =
        components.size() == 1 ? tr("Move component") : tr("Move components");
    document.startUndo(QtStrings::toUtf8(description));
    try
    {
        moveFrozen(document.getRocket(), ids, target.parent->getId(), target.index - before);
    }
    catch (...)
    {
        document.stopUndo();
        throw;
    }
    document.stopUndo();
}

ComponentDrop::Outcome ComponentDrop::copy(OpenRocketDocument&                     document,
                                           const OpenRocketDocument&               source,
                                           std::span<const RocketComponent* const> components,
                                           const Target&                           target)
{
    //: Hard-coded English in OpenRocket (ComponentTreeTransferHandler.importData())
    const QString description =
        components.size() == 1 ? tr("Copy component") : tr("Copy components");
    Result<ComponentCopy> copied = ComponentCopy::copyOf(
        source, std::vector<const RocketComponent*>(components.begin(), components.end()));
    if (!copied.has_value())
    {
        return {.dropped = false,
                .refusal =
                    UserPrompts::Message{.severity = UserPrompts::Severity::ERROR,
                                         .title    = description,
                                         .lines = {QtStrings::fromUtf8(copied.error().message)}}};
    }
    std::vector<ComponentCopy::Placement> placements;
    std::size_t                           newComponents = 0;
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        placements.push_back({.component = i, .parent = target.parent, .index = target.index + i});
        newComponents += ComponentLimits::treeSize(*components[i]);
    }
    std::optional<UserPrompts::Message> refusal = ComponentLimits::refusal(
        document, newComponents, copied->countNewDecalImages(document, placements), description);
    if (refusal.has_value())
    {
        return {.dropped = false, .refusal = std::move(refusal)};
    }
    document.startUndo(QtStrings::toUtf8(description));
    try
    {
        std::ignore = std::move(*copied).insertInto(document, placements);
    }
    catch (...)
    {
        document.stopUndo();
        throw;
    }
    document.stopUndo();
    return {.dropped = true, .refusal = std::nullopt};
}

}  // namespace QtRocket::Gui
