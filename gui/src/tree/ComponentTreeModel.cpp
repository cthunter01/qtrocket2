#include "QtRocket/gui/tree/ComponentTreeModel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/tree/ComponentDrop.h"
#include "QtRocket/gui/tree/ComponentMimeData.h"
#include "QtRocket/gui/tree/ComponentToolTip.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

/// A component of the mirror (see the class comment): what the structural functions of the
/// model read, and the component's pointer, valid until the next reset.
struct ComponentTreeModel::Node
{
    Uuid             id;
    RocketComponent* component      = nullptr;
    ComponentKind    kind           = ComponentKind::ROCKET;
    bool             allowsChildren = false;
    /// A mass component's type (its icon); nullopt for any other kind.
    std::optional<MassComponent::MassComponentType> massType;
    Node*                                           parent = nullptr;
    /// The row within the parent (0 for the Rocket).
    int row = 0;
    /// The position in the tree's pre-order (the Rocket is 0).
    int                                order = 0;
    std::vector<std::unique_ptr<Node>> children;
};

namespace
{

using Node = ComponentTreeModel::Node;

/// The mass component type of @p component, if it is a mass component.
std::optional<MassComponent::MassComponentType> massTypeOf(const RocketComponent& component)
{
    if (const auto* mass = dynamic_cast<const MassComponent*>(&component))
    {
        return mass->getMassComponentType();
    }
    return std::nullopt;
}

/// The mirror of @p component and its subtree below @p parent at @p row; numbers the nodes in
/// pre-order from @p order on and enters each in @p nodes (the first of an id that two
/// components share, as findComponent() finds it).
std::unique_ptr<Node> mirror(RocketComponent& component, Node* parent, int row, int& order,
                             std::unordered_map<Uuid, Node*>& nodes)
{
    auto node            = std::make_unique<Node>();
    node->id             = component.getId();
    node->component      = &component;
    node->kind           = component.kind();
    node->allowsChildren = component.allowsChildren();
    node->massType       = massTypeOf(component);
    node->parent         = parent;
    node->row            = row;
    node->order          = order++;
    nodes.try_emplace(node->id, node.get());
    const std::size_t count = component.getChildCount();
    node->children.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        node->children.push_back(
            mirror(component.getChild(i), node.get(), static_cast<int>(i), order, nodes));
    }
    return node;
}

/// Whether the mirror @p node has the ids and the child counts of @p component's tree, compared
/// from the live components (a node's pointer is never read: it may point at a component that a
/// change made while the rocket's events were off destroyed); when so, each node takes its
/// component's pointer and mass type again. False at the first difference.
bool matchAndRefresh(Node& node, RocketComponent& component)
{
    const std::size_t count = component.getChildCount();
    if (node.id != component.getId() || node.children.size() != count)
    {
        return false;
    }
    node.component = &component;
    node.massType  = massTypeOf(component);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!matchAndRefresh(*node.children[i], component.getChild(i)))
        {
            return false;
        }
    }
    return true;
}

/// Whether @p type has the TREE or the UNDO bit: a change the model answers with a reset.
[[nodiscard]] bool isStructural(int type) noexcept
{
    return ComponentChangeEvent::matches(ComponentChangeEvent::Type::TREE, type) ||
           ComponentChangeEvent::matches(ComponentChangeEvent::Type::UNDO, type);
}

/// Sets a flag for its lifetime.
class Resetting
{
public:
    explicit Resetting(bool& flag) noexcept : m_flag(&flag) { *m_flag = true; }
    ~Resetting() { *m_flag = false; }
    Resetting(const Resetting&)            = delete;
    Resetting& operator=(const Resetting&) = delete;
    Resetting(Resetting&&)                 = delete;
    Resetting& operator=(Resetting&&)      = delete;

private:
    bool* m_flag;
};

/// The small icon of the component of @p node (ComponentIcons' rule, from the node's fields).
QIcon iconOf(const Node& node)
{
    if (node.massType.has_value())
    {
        return ComponentIcons::smallMassTypeIcon(*node.massType);
    }
    return ComponentIcons::smallIcon(node.kind);
}

}  // namespace

ComponentTreeModel::ComponentTreeModel(OpenRocketDocument& document, const Preferences& preferences,
                                       QObject* parent)
  : QAbstractItemModel(parent),
    m_document(&document),
    m_rocket(&document.getRocket()),
    m_preferences(&preferences)
{
    rebuild();
    m_changes = m_rocket->addComponentChangeListener(
        guarded([this](const ComponentChangeEvent& event) { componentChanged(event); },
                QStringLiteral("Component tree")));
}

ComponentTreeModel::~ComponentTreeModel() = default;

// ---------------------------------------------------------------------- QAbstractItemModel

QModelIndex ComponentTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column != 0)
    {
        return {};
    }
    if (!parent.isValid())
    {
        return row == 0 && m_root != nullptr ? createIndex(0, 0, m_root.get()) : QModelIndex();
    }
    if (!owns(parent) || parent.column() != 0)
    {
        return {};
    }
    const Node& node = nodeOf(parent);
    if (static_cast<std::size_t>(row) >= node.children.size())
    {
        return {};
    }
    return createIndex(row, 0, node.children[static_cast<std::size_t>(row)].get());
}

QModelIndex ComponentTreeModel::parent(const QModelIndex& child) const
{
    if (!owns(child))
    {
        return {};
    }
    const Node* parentNode = nodeOf(child).parent;
    return parentNode == nullptr ? QModelIndex() : indexOf(*parentNode);
}

int ComponentTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_root != nullptr ? 1 : 0;
    }
    if (!owns(parent) || parent.column() != 0)
    {
        return 0;
    }
    return static_cast<int>(nodeOf(parent).children.size());
}

int ComponentTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant ComponentTreeModel::data(const QModelIndex& index, int role) const
{
    if (!owns(index))
    {
        return {};
    }
    const Node& node = nodeOf(index);
    // The roles the mirror answers alone.
    if (role == kComponentIdRole)
    {
        return QtStrings::fromUtf8(node.id.toString());
    }
    if (role == Qt::DecorationRole)
    {
        const QIcon icon = iconOf(node);
        return icon.isNull() ? QVariant() : QVariant::fromValue(icon);
    }
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole && role != kInactiveRole &&
        role != kHiddenRole && role != kMarkersRole)
    {
        return {};
    }
    const RocketComponent* component = resolve(node);
    return component != nullptr ? componentData(*component, role) : QVariant();
}

QVariant ComponentTreeModel::componentData(const RocketComponent& component, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            // The component's name is data: shown as it is, never translated.
            return QtStrings::fromUtf8(component.toString());
        case Qt::ToolTipRole:
            return ComponentToolTip::singleComponent(component, m_preferences->getDefaultMach());
        case kInactiveRole:
            return isInInactiveStage(component);
        case kHiddenRole:
            return !component.isVisible();
        case kMarkersRole:
        {
            QList<int> values;
            for (const Icons::Id marker : markers(component))
            {
                values.append(static_cast<int>(marker));
            }
            return QVariant::fromValue(values);
        }
        default:
            return {};
    }
}

Qt::ItemFlags ComponentTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        // The blank space below the rows takes drops: the end of the Rocket (ComponentDrop).
        return Qt::ItemIsDropEnabled;
    }
    if (!owns(index))
    {
        return Qt::NoItemFlags;
    }
    // The mirror only: Qt asks during modelAboutToBeReset, when the components may be gone.
    const Node&   node  = nodeOf(index);
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (node.parent != nullptr)
    {
        flags |= Qt::ItemIsDragEnabled;
    }
    flags |= node.allowsChildren ? Qt::ItemIsDropEnabled : Qt::ItemNeverHasChildren;
    return flags;
}

Qt::DropActions ComponentTreeModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions ComponentTreeModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList ComponentTreeModel::mimeTypes() const
{
    return {ComponentMimeData::mimeType()};
}

QMimeData* ComponentTreeModel::mimeData(const QModelIndexList& indexes) const
{
    std::vector<const Node*>            nodes;
    std::vector<const RocketComponent*> dragged;
    for (const QModelIndex& index : indexes)
    {
        if (!owns(index))
        {
            continue;
        }
        const Node&            node      = nodeOf(index);
        const RocketComponent* component = resolve(node);
        if (component == nullptr || std::ranges::find(nodes, &node) != nodes.end())
        {
            continue;
        }
        if (node.parent == nullptr)
        {
            return nullptr;  // the Rocket: no drag (createTransferable() returns null)
        }
        nodes.push_back(&node);
        dragged.push_back(component);
    }
    // Those with a dragged ancestor go with it; the rest in tree order.
    std::vector<const Node*> tops;
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        if (!RocketComponent::listContainsParent(dragged, *dragged[i]))
        {
            tops.push_back(nodes[i]);
        }
    }
    if (tops.empty())
    {
        return nullptr;
    }
    std::ranges::sort(tops, {}, &Node::order);
    std::vector<Uuid> ids;
    ids.reserve(tops.size());
    for (const Node* node : tops)
    {
        ids.push_back(node->id);
    }
    // Handed to the caller, which owns it (QAbstractItemView::startDrag() gives it to its
    // QDrag).
    return std::make_unique<ComponentMimeData>(*m_document, std::move(ids)).release();
}

bool ComponentTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                         int /*column*/, const QModelIndex& parent) const
{
    const ComponentMimeData* drag = ComponentMimeData::fromMimeData(data);
    if (drag == nullptr)
    {
        return false;
    }
    const std::optional<ComponentDrop::Target> target = ComponentDrop::target(*this, row, parent);
    return target.has_value() && ComponentDrop::canDrop(*m_document, *drag, action, *target);
}

bool ComponentTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                      int /*column*/, const QModelIndex& parent)
{
    const ComponentMimeData* drag = ComponentMimeData::fromMimeData(data);
    if (drag == nullptr)
    {
        return false;
    }
    const std::optional<ComponentDrop::Target> target = ComponentDrop::target(*this, row, parent);
    if (!target.has_value())
    {
        return false;
    }
    ComponentDrop::Outcome outcome;
    // Nothing may leave a drop event into Qt's event loop.
    Guard::run([&] { outcome = ComponentDrop::drop(*m_document, *drag, action, *target); },
               QStringLiteral("Drop"));
    if (outcome.refusal.has_value() && m_prompts != nullptr)
    {
        // After the drop: a modal message inside the drop event would run while the platform's
        // drag still holds the mouse.
        QTimer::singleShot(0, this,
                           [prompts = m_prompts, parentWindow = m_dialogParent,
                            message = std::move(*outcome.refusal)] {
                               Guard::run(
                                   [&] { prompts->showMessage(parentWindow.data(), message); },
                                   QStringLiteral("Drop"));
                           });
    }
    return outcome.dropped;
}

void ComponentTreeModel::setUserPrompts(UserPrompts* prompts, QWidget* dialogParent)
{
    m_prompts      = prompts;
    m_dialogParent = dialogParent;
}

// ------------------------------------------------------------------------------ components

QModelIndex ComponentTreeModel::rocketIndex() const
{
    return index(0, 0, QModelIndex());
}

std::optional<Uuid> ComponentTreeModel::componentId(const QModelIndex& index) const
{
    if (!owns(index))
    {
        return std::nullopt;
    }
    return nodeOf(index).id;
}

RocketComponent* ComponentTreeModel::component(const QModelIndex& index) const
{
    return owns(index) ? resolve(nodeOf(index)) : nullptr;
}

std::vector<RocketComponent*> ComponentTreeModel::components(const QModelIndexList& indexes) const
{
    std::vector<RocketComponent*> found;
    found.reserve(static_cast<std::size_t>(indexes.size()));
    for (const QModelIndex& index : indexes)
    {
        found.push_back(component(index));
    }
    std::erase(found, nullptr);
    return found;
}

QModelIndex ComponentTreeModel::indexOf(const Uuid& id) const
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? QModelIndex() : indexOf(*it->second);
}

QModelIndex ComponentTreeModel::indexOf(const RocketComponent& component) const
{
    const auto it = m_nodes.find(component.getId());
    if (it == m_nodes.end() || resolve(*it->second) != &component)
    {
        return {};
    }
    return indexOf(*it->second);
}

QString ComponentTreeModel::toolTip(const QModelIndex&     hovered,
                                    const QModelIndexList& selection) const
{
    const RocketComponent* component = this->component(hovered);
    if (component == nullptr)
    {
        return {};
    }
    std::vector<const RocketComponent*> selected;
    for (const RocketComponent* c : components(selection))
    {
        if (std::ranges::find(selected, c) == selected.end())
        {
            selected.push_back(c);
        }
    }
    return ComponentToolTip::forComponent(*component, selected, m_preferences->getDefaultMach());
}

// ------------------------------------------------------------------ the renderer's rules

bool ComponentTreeModel::isInInactiveStage(const RocketComponent& component)
{
    const Rocket* rocket = component.findRocket();
    if (rocket == nullptr || rocket == &component)
    {
        return false;
    }
    // A stage without components counts as inactive in the configuration (isStageActive()) but
    // is not shown as such.
    const AxialStage* stage = component.findStage();
    return stage != nullptr && stage->getChildCount() > 0 &&
           !rocket->getSelectedConfiguration().isComponentActive(component);
}

std::vector<Icons::Id> ComponentTreeModel::markers(const RocketComponent& component)
{
    std::vector<Icons::Id> found;
    if (component.getMassOverriddenBy() != nullptr)
    {
        found.push_back(Icons::Id::MASS_OVERRIDE_SUBCOMPONENT);
    }
    else if (component.isMassOverridden())
    {
        found.push_back(Icons::Id::MASS_OVERRIDE);
    }
    if (component.getCGOverriddenBy() != nullptr)
    {
        found.push_back(Icons::Id::CG_OVERRIDE_SUBCOMPONENT);
    }
    else if (component.isCGOverridden())
    {
        found.push_back(Icons::Id::CG_OVERRIDE);
    }
    if (component.getCDOverriddenBy() != nullptr)
    {
        found.push_back(Icons::Id::CD_OVERRIDE_SUBCOMPONENT);
    }
    else if (component.isCDOverridden())
    {
        found.push_back(Icons::Id::CD_OVERRIDE);
    }
    if (!component.isVisible())
    {
        found.push_back(Icons::Id::COMPONENT_HIDDEN);
    }
    if (isStage(component.kind()) && isInInactiveStage(component))
    {
        found.push_back(Icons::Id::COMPONENT_DISABLED);
    }
    return found;
}

std::vector<Icons::Id> ComponentTreeModel::markersOf(const QModelIndex& index)
{
    std::vector<Icons::Id> found;
    for (const int value : index.data(kMarkersRole).value<QList<int>>())
    {
        found.push_back(static_cast<Icons::Id>(value));
    }
    return found;
}

// --------------------------------------------------------------------------------- private

const ComponentTreeModel::Node& ComponentTreeModel::nodeOf(const QModelIndex& index)
{
    return *static_cast<const Node*>(index.constInternalPointer());
}

bool ComponentTreeModel::owns(const QModelIndex& index) const noexcept
{
    return index.isValid() && index.model() == this && index.constInternalPointer() != nullptr;
}

QModelIndex ComponentTreeModel::indexOf(const Node& node) const
{
    return createIndex(node.row, 0, &node);
}

RocketComponent* ComponentTreeModel::resolve(const Node& node) const
{
    return mirrorIsCurrent() ? node.component : m_rocket->findComponent(node.id);
}

bool ComponentTreeModel::mirrorIsCurrent() const noexcept
{
    return !m_resetting && m_rocket->isEventsEnabled() && !m_rocket->isFrozen() &&
           m_rocket->getTreeModId() == m_treeModId && m_rocket->getModId() == m_modId;
}

void ComponentTreeModel::componentChanged(const ComponentChangeEvent& event)
{
    const Uuid source = event.getSource()->getId();
    const int  type   = event.getType();
    if (m_resetting)
    {
        // A change made while the views answer the reset: one more reset when this one ends,
        // for all such changes together (as Rocket::thaw() merges the events of a freeze): their
        // bits merged, and the source of the last that changed the tree or was an undo, which
        // the views make visible.
        if (!m_pendingReset.has_value())
        {
            m_pendingReset = std::pair{source, type};
            return;
        }
        auto& [pendingSource, pendingType] = *m_pendingReset;
        if (isStructural(type) || !isStructural(pendingType))
        {
            pendingSource = source;
        }
        pendingType |= type;
        return;
    }
    // A change event that arrives before the tree change it follows (a listener that changed
    // the rocket in its slot for that tree change, before this model's turn), and one that
    // follows a change made while the rocket's events were off (enableEvents(true) fires
    // AEROMASS: the tree may have changed without a modification id telling), find the mirror
    // different from the tree.
    if (event.isTreeChange() || event.isUndoChange() || !refreshNodes())
    {
        reset(source, type);
        return;
    }
    emitDataChangedForAll();
}

void ComponentTreeModel::reset(const Uuid& source, int changeType)
{
    std::optional<std::pair<Uuid, int>> cause = std::pair{source, changeType};
    while (cause.has_value())
    {
        // The new mirror first: the old one stays in place for what the views ask in
        // modelAboutToBeReset.
        int                             order = 0;
        std::unordered_map<Uuid, Node*> nodes;
        std::unique_ptr<Node>           root = mirror(*m_rocket, nullptr, 0, order, nodes);

        m_lastResetSource     = cause->first;
        m_lastResetChangeType = cause->second;
        m_pendingReset.reset();
        {
            // Set while the views answer the reset (and cleared if one of them throws).
            const Resetting resetting(m_resetting);
            beginResetModel();
            std::swap(m_root, root);
            std::swap(m_nodes, nodes);
            m_treeModId = m_rocket->getTreeModId();
            m_modId     = m_rocket->getModId();
            endResetModel();
        }
        // The old mirror goes after the views have let go of its indexes.
        root.reset();
        cause = std::exchange(m_pendingReset, std::nullopt);
    }
}

void ComponentTreeModel::rebuild()
{
    int order = 0;
    m_nodes.clear();
    m_root      = mirror(*m_rocket, nullptr, 0, order, m_nodes);
    m_treeModId = m_rocket->getTreeModId();
    m_modId     = m_rocket->getModId();
}

bool ComponentTreeModel::refreshNodes()
{
    if (m_root == nullptr || !m_rocket->isEventsEnabled() || m_rocket->isFrozen() ||
        m_rocket->getTreeModId() != m_treeModId || !matchAndRefresh(*m_root, *m_rocket))
    {
        return false;
    }
    m_modId = m_rocket->getModId();
    return true;
}

void ComponentTreeModel::emitDataChangedForAll()
{
    if (m_root == nullptr)
    {
        return;
    }
    const QModelIndex rocket = indexOf(*m_root);
    Q_EMIT dataChanged(rocket, rocket);
    std::vector<const Node*> parents{m_root.get()};
    while (!parents.empty())
    {
        const Node* node = parents.back();
        parents.pop_back();
        if (node->children.empty())
        {
            continue;
        }
        Q_EMIT dataChanged(indexOf(*node->children.front()), indexOf(*node->children.back()));
        for (const auto& child : node->children)
        {
            parents.push_back(child.get());
        }
    }
}

}  // namespace QtRocket::Gui
