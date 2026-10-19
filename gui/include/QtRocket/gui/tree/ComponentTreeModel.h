#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

class QMimeData;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The component tree of a document's rocket as a Qt item model (OpenRocket's
/// ComponentTreeModel): one top-level row, the Rocket, and below every component its children,
/// in one column. A component that allows no children is a leaf (Java's isLeaf()).
///
/// MIRROR. The model does not hand out the components themselves: every QModelIndex points at a
/// node of the model's own mirror of the tree, which holds the component's id, a pointer to the
/// component, and what index(), parent(), rowCount() and flags() need (the kind, whether it
/// allows children, its parent node and row). Those four read the nodes only and never touch a
/// component, so they answer safely while the rocket's objects are being replaced: a QTreeView
/// calls flags() from modelAboutToBeReset, after Rocket::loadFrom() has replaced every
/// component and after a frozen operation may have destroyed the ones it removed. The mirror is
/// rebuilt in a model reset (beginResetModel() ... endResetModel()) at every change event of
/// the rocket with the TREE or the UNDO bit, as Java answers them with a structure change of
/// the whole tree; there is no fine-grained insertRows()/removeRows() (the core tells of a
/// change only after it, and an undo replaces every object), and the model implements neither,
/// so a drag's QAbstractItemView::clearOrRemove() cannot remove anything. Every other event
/// emits dataChanged() for every row: a row's markers, italics and tooltip depend on its
/// ancestors and the selected configuration, where Java repaints the source alone.
///
/// The pointers of the mirror are valid from one reset to the next. data() and component()
/// trust them while the rocket's events are enabled, it is not frozen and its tree has not
/// changed since the reset (Rocket::getTreeModId()); otherwise they look the component up by
/// id (Rocket::findComponent()) and answer nothing for one that is gone. So nothing reads a
/// component that a frozen operation destroyed before thaw() delivered its event.
///
/// The reset keeps what the view needs to restore its state (OpenRocket's
/// fireTreeStructureChanged(source) and expandAll() after an undo): lastResetSource() and
/// lastResetChangeType(), read in the view's modelReset slot.
///
/// The roles: Qt::DisplayRole the component's toString() (data, not a translated text);
/// Qt::DecorationRole its small icon (ComponentIcons; none for the Rocket); Qt::ToolTipRole the
/// tooltip of the component alone (ComponentToolTip; toolTip() gives the one for a selection);
/// and for the delegate kComponentIdRole, kInactiveRole, kHiddenRole and kMarkersRole.
///
/// Drag and drop: the Rocket cannot be dragged, a component that allows children takes drops;
/// mimeData() makes a ComponentMimeData. Dropping (canDropMimeData(), dropMimeData()) refuses
/// everything here; the drop rules come with the tree's drag and drop (ComponentDrop).
///
/// The document must outlive the model, and its rocket keep its events enabled (a document's
/// rocket does).
class ComponentTreeModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    /// The component's id (Uuid::toString(), a QString), read from the mirror: safe at any time.
    static constexpr int kComponentIdRole = Qt::UserRole + 1;
    /// Whether the component is in an inactive stage of the selected configuration
    /// (isInInactiveStage(), a bool): the delegate writes it in italics.
    static constexpr int kInactiveRole = Qt::UserRole + 2;
    /// Whether the component is hidden (!isVisible(), a bool).
    static constexpr int kHiddenRole = Qt::UserRole + 3;
    /// The component's markers (markers(), a QList<int> of Icons::Id values; markersOf()).
    static constexpr int kMarkersRole = Qt::UserRole + 4;

    /// A component of the mirror, what QModelIndex::internalPointer() points at; defined in the
    /// model's source only.
    struct Node;

    /// The model of @p document's rocket; the tooltips take the default Mach number (for an
    /// overrider's CD) from @p preferences. Both must outlive the model.
    ComponentTreeModel(OpenRocketDocument& document, const Preferences& preferences,
                       QObject* parent = nullptr);
    ~ComponentTreeModel() override;

    ComponentTreeModel(const ComponentTreeModel&)            = delete;
    ComponentTreeModel& operator=(const ComponentTreeModel&) = delete;
    ComponentTreeModel(ComponentTreeModel&&)                 = delete;
    ComponentTreeModel& operator=(ComponentTreeModel&&)      = delete;

    // ------------------------------------------------------------------ QAbstractItemModel

    [[nodiscard]] QModelIndex index(int row, int column,
                                    const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int         rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int         columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant    data(const QModelIndex& index,
                                   int                role = Qt::DisplayRole) const override;
    /// Selectable and enabled; drag-enabled but for the Rocket; drop-enabled when the component
    /// allows children, else never having children (Qt::ItemNeverHasChildren).
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Copy and move (Java: COPY_OR_MOVE).
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    /// Copy and move.
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    /// ComponentMimeData::mimeType().
    [[nodiscard]] QStringList mimeTypes() const override;
    /// A ComponentMimeData of the components at @p indexes (RocketComponentTransferable): those
    /// without an ancestor among them, in tree order; null when the Rocket is among them (Java:
    /// the Rocket cannot be dragged) or none is left. The caller (the drag) owns it.
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    /// False: the drop rules are not here yet.
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                       int column, const QModelIndex& parent) const override;
    /// False, nothing is dropped: the drop rules are not here yet.
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    // -------------------------------------------------------------------------- components

    /// The document whose rocket this is.
    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }

    /// The index of the Rocket (row 0 at the top).
    [[nodiscard]] QModelIndex rocketIndex() const;

    /// The id of the component at @p index, from the mirror (safe at any time); nullopt for an
    /// invalid index or one of another model.
    [[nodiscard]] std::optional<Uuid> componentId(const QModelIndex& index) const;

    /// The component at @p index (see the class comment for when it is looked up by id); null
    /// for an invalid index, one of another model, or a component that is not in the rocket any
    /// more.
    [[nodiscard]] RocketComponent* component(const QModelIndex& index) const;

    /// component() of each of @p indexes that has one, in their order.
    [[nodiscard]] std::vector<RocketComponent*> components(const QModelIndexList& indexes) const;

    /// The index of the component with the id @p id in the mirror, or an invalid index.
    [[nodiscard]] QModelIndex indexOf(const Uuid& id) const;

    /// The index of @p component, or an invalid index when it is not in the mirror.
    [[nodiscard]] QModelIndex indexOf(const RocketComponent& component) const;

    /// The tooltip of the component at @p hovered while the components at @p selection are
    /// selected (ComponentToolTip::forComponent(), the renderer's applyToolTipText()); empty
    /// when @p hovered has no component.
    [[nodiscard]] QString toolTip(const QModelIndex&     hovered,
                                  const QModelIndexList& selection) const;

    // ------------------------------------------------------------------------- the resets

    /// The id of the source of the change event that caused the last reset (OpenRocket's
    /// fireTreeStructureChanged(source)); nullopt before the first one.
    [[nodiscard]] const std::optional<Uuid>& lastResetSource() const noexcept
    {
        return m_lastResetSource;
    }

    /// The change bits of the event that caused the last reset (ComponentChangeEvent's
    /// k...Change values; TREE and UNDO together: an undo or redo, after which OpenRocket
    /// expands the whole tree); 0 before the first one.
    [[nodiscard]] int lastResetChangeType() const noexcept { return m_lastResetChangeType; }

    // ------------------------------------------------------------ the renderer's rules

    /// Whether @p component is shown as in an inactive stage (the renderer's isInactiveStage):
    /// it is not the Rocket, its tree is a rocket's, it has a stage, which holds components,
    /// and the selected configuration does not count it as active.
    [[nodiscard]] static bool isInInactiveStage(const RocketComponent& component);

    /// The markers the renderer shows after @p component's name, in its order: the mass
    /// override (MASS_OVERRIDE_SUBCOMPONENT when an ancestor overrides the mass, else
    /// MASS_OVERRIDE for an own override), the same for the CG and the CD, COMPONENT_HIDDEN when
    /// it is hidden, and COMPONENT_DISABLED for a stage (a booster set too) in an inactive stage.
    [[nodiscard]] static std::vector<Icons::Id> markers(const RocketComponent& component);

    /// The markers of the row at @p index (kMarkersRole); none when it has no component.
    [[nodiscard]] static std::vector<Icons::Id> markersOf(const QModelIndex& index);

private:
    /// The node of @p index, which must be a valid index of this model.
    [[nodiscard]] static const Node& nodeOf(const QModelIndex& index);
    /// Whether @p index is a valid index of this model.
    [[nodiscard]] bool owns(const QModelIndex& index) const noexcept;
    /// The index of @p node.
    [[nodiscard]] QModelIndex indexOf(const Node& node) const;
    /// The component of @p node: its pointer while the mirror is current, else looked up by id.
    [[nodiscard]] RocketComponent* resolve(const Node& node) const;
    /// Whether the mirror's pointers are those of the rocket now (see the class comment).
    [[nodiscard]] bool mirrorIsCurrent() const noexcept;
    /// What data() answers for the roles that need the component.
    [[nodiscard]] QVariant componentData(const RocketComponent& component, int role) const;

    /// The rocket's change event @p event.
    void componentChanged(const ComponentChangeEvent& event);
    /// Rebuilds the mirror in a model reset after the event of @p changeType from @p source.
    void reset(const Uuid& source, int changeType);
    /// Builds the mirror of the rocket as it is now.
    void rebuild();
    /// The cached fields of every node that a non-tree change may change (the mass component
    /// type), from the components; only while the mirror is current.
    void refreshNodes();
    /// dataChanged() for every row.
    void emitDataChangedForAll();

    OpenRocketDocument*                     m_document;
    Rocket*                                 m_rocket;
    const Preferences*                      m_preferences;
    std::unique_ptr<Node>                   m_root;
    std::unordered_map<Uuid, Node*>         m_nodes;
    ModId                                   m_treeModId;
    std::optional<Uuid>                     m_lastResetSource;
    int                                     m_lastResetChangeType{0};
    bool                                    m_resetting{false};
    std::optional<std::pair<Uuid, int>>     m_pendingReset;
    ComponentChangeSignal::ScopedConnection m_changes;
};

}  // namespace QtRocket::Gui
