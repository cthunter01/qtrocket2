#pragma once

#include <span>
#include <vector>

#include <QObject>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentTreeModel;
class ComponentTreeView;

/// The components selected in a design window (the component half of OpenRocket's
/// DocumentSelectionModel; the simulations come with Milestone 4): the selection of the window's
/// component tree, which it reads and sets.
///
/// The components are in the order of their selection, as Qt's selection model keeps it (a
/// deselected one leaves its place, a selected one is appended): the first is OpenRocket's
/// getSelectedComponent(), the component whose configuration dialog opens for the selection.
/// (Java's DefaultTreeSelectionModel keeps that order too until a path is deselected, after
/// which the order of its hash table decides.)
///
/// changed() tells of every change of the selection, and once after every change of the tree
/// (a model reset): the view puts back the selection by id then (ComponentTreeView), and the
/// components may be new objects (after an undo or redo every one is). So nobody keeps a
/// component pointer from here beyond the slot that read it (decision O14); and a reaction that
/// changes the document (an undo position, an undo or redo) is queued, since changed() may come
/// while the rocket delivers its change event.
///
/// It belongs to the view, its parent (makeChild<DocumentSelectionModel>(view)).
class DocumentSelectionModel final : public QObject
{
    Q_OBJECT

public:
    /// The selection of @p view (not null), which is its parent and owner.
    explicit DocumentSelectionModel(ComponentTreeView* view);
    ~DocumentSelectionModel() override;

    DocumentSelectionModel(const DocumentSelectionModel&)            = delete;
    DocumentSelectionModel& operator=(const DocumentSelectionModel&) = delete;
    DocumentSelectionModel(DocumentSelectionModel&&)                 = delete;
    DocumentSelectionModel& operator=(DocumentSelectionModel&&)      = delete;

    [[nodiscard]] ComponentTreeView&  view() const noexcept { return *m_view; }
    [[nodiscard]] ComponentTreeModel& model() const noexcept;
    [[nodiscard]] OpenRocketDocument& document() const noexcept;

    /// The first selected component (getSelectedComponent()); null when none is selected.
    [[nodiscard]] RocketComponent* selectedComponent() const;

    /// The selected components in the order of their selection (getSelectedComponents()).
    [[nodiscard]] std::vector<RocketComponent*> selectedComponents() const;

    /// The ids of the selected components in the order of their selection, from the tree's
    /// mirror (safe at any time, also for a reaction queued past a change of the tree).
    [[nodiscard]] std::vector<Uuid> selectedComponentIds() const;

    /// Whether @p component is selected.
    [[nodiscard]] bool isSelected(const RocketComponent& component) const;

    /// Selects @p component alone (setSelectedComponent()). A component that is not in the tree
    /// (another rocket's, or one added while the rocket is frozen, before thaw() tells the tree)
    /// clears the selection.
    void setSelectedComponent(const RocketComponent& component);

    /// Selects @p components in their order (setSelectedComponents()); those that are not in the
    /// tree are left out.
    void setSelectedComponents(std::span<RocketComponent* const> components);

    /// Selects nothing (clearComponentSelection()).
    void clearComponentSelection();

    /// What the rocket's figure highlights for the selection (DesignPanel's
    /// highlightAssemblyChildren()): withAssemblyDescendants() of the selected components.
    [[nodiscard]] std::vector<RocketComponent*> highlightedComponents() const;

    /// @p selection, then the descendants of each component in it that is an assembly (the
    /// Rocket, a stage, a booster set, a pod set) in tree order, each component once.
    [[nodiscard]] static std::vector<RocketComponent*> withAssemblyDescendants(
        std::span<RocketComponent* const> selection);

Q_SIGNALS:
    /// The selection changed, or the tree did (OpenRocket's COMPONENT_SELECTION_CHANGE).
    void changed();

private:
    /// The selection model's selectionChanged.
    void onSelectionChanged();

    ComponentTreeView* m_view;
};

}  // namespace QtRocket::Gui
