#include "QtRocket/gui/tree/DocumentSelectionModel.h"

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

#include <QItemSelection>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QtContainerFwd>

#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

DocumentSelectionModel::DocumentSelectionModel(ComponentTreeView* view)
  : QObject(view), m_view(view)
{
    QTROCKET_ASSERT(view != nullptr);
    const QString context = QStringLiteral("Component selection");
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { onSelectionChanged(); }, context));
    connect(view, &ComponentTreeView::stateRestored, this,
            guarded([this] { Q_EMIT changed(); }, context));
}

DocumentSelectionModel::~DocumentSelectionModel() = default;

ComponentTreeModel& DocumentSelectionModel::model() const noexcept
{
    return m_view->componentModel();
}

OpenRocketDocument& DocumentSelectionModel::document() const noexcept
{
    return model().document();
}

RocketComponent* DocumentSelectionModel::selectedComponent() const
{
    const std::vector<RocketComponent*> selected = selectedComponents();
    return selected.empty() ? nullptr : selected.front();
}

std::vector<RocketComponent*> DocumentSelectionModel::selectedComponents() const
{
    return model().components(m_view->selectionModel()->selectedIndexes());
}

std::vector<Uuid> DocumentSelectionModel::selectedComponentIds() const
{
    std::vector<Uuid> ids;
    for (const QModelIndex& index : m_view->selectionModel()->selectedIndexes())
    {
        const std::optional<Uuid> id = model().componentId(index);
        if (id.has_value())
        {
            ids.push_back(*id);
        }
    }
    return ids;
}

bool DocumentSelectionModel::isSelected(const RocketComponent& component) const
{
    const QModelIndex index = model().indexOf(component);
    return index.isValid() && m_view->selectionModel()->isSelected(index);
}

void DocumentSelectionModel::setSelectedComponent(const RocketComponent& component)
{
    const QModelIndex index = model().indexOf(component);
    if (!index.isValid())
    {
        clearComponentSelection();
        return;
    }
    m_view->selectionModel()->setCurrentIndex(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void DocumentSelectionModel::setSelectedComponents(std::span<RocketComponent* const> components)
{
    const std::vector<Uuid> before = selectedComponentIds();
    QItemSelection          selection;
    QModelIndex             first;
    for (const RocketComponent* component : components)
    {
        const QModelIndex index =
            component != nullptr ? model().indexOf(*component) : QModelIndex();
        if (index.isValid() && !selection.contains(index))
        {
            selection.select(index, index);
            if (!first.isValid())
            {
                first = index;
            }
        }
    }
    QItemSelectionModel& selectionModel = *m_view->selectionModel();
    if (first.isValid())
    {
        selectionModel.setCurrentIndex(first, QItemSelectionModel::NoUpdate);
    }
    selectionModel.select(selection,
                          QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    // Qt tells nothing when the same components are selected in another order, but the order
    // decides which is the first (RocketActions' and the add buttons' rules).
    const std::vector<Uuid> after = selectedComponentIds();
    if (after != before && std::ranges::is_permutation(after, before))
    {
        Q_EMIT changed();
    }
}

void DocumentSelectionModel::clearComponentSelection()
{
    m_view->selectionModel()->clearSelection();
}

std::vector<RocketComponent*> DocumentSelectionModel::highlightedComponents() const
{
    const std::vector<RocketComponent*> selected = selectedComponents();
    return withAssemblyDescendants(selected);
}

std::vector<RocketComponent*> DocumentSelectionModel::withAssemblyDescendants(
    std::span<RocketComponent* const> selection)
{
    std::vector<RocketComponent*> components;
    const auto                    add = [&components](RocketComponent* component) {
        if (std::ranges::find(components, component) == components.end())
        {
            components.push_back(component);
        }
    };
    for (RocketComponent* component : selection)
    {
        add(component);
    }
    for (RocketComponent* component : selection)
    {
        if (component != nullptr && isAssembly(component->kind()))
        {
            for (RocketComponent* descendant : component->getAllChildren())
            {
                add(descendant);
            }
        }
    }
    std::erase(components, nullptr);
    return components;
}

void DocumentSelectionModel::onSelectionChanged()
{
    // During the view's restore after a reset, stateRestored() tells once.
    if (!m_view->isRestoring())
    {
        Q_EMIT changed();
    }
}

}  // namespace QtRocket::Gui
