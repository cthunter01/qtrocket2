#include "QtRocket/gui/main/RocketActions.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/ComponentEditing.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentList = std::vector<const RocketComponent*>;

/// @p selection, not null.
DocumentSelectionModel* checked(DocumentSelectionModel* selection)
{
    QTROCKET_ASSERT(selection != nullptr);
    return selection;
}

/// @p menus, a design window's.
MainMenus& checked(MainMenus& menus)
{
    QTROCKET_ASSERT(menus.isDesignWindowMenus());
    return menus;
}

/// @p components as a list of const pointers.
[[nodiscard]] ComponentList constList(const std::vector<RocketComponent*>& components)
{
    return {components.begin(), components.end()};
}

/// Every component below @p component, breadth first (RocketActions.getDescendants()).
[[nodiscard]] std::vector<RocketComponent*> descendantsOf(RocketComponent& component)
{
    std::vector<RocketComponent*>       result;
    const std::vector<RocketComponent*> children = component.getChildren();
    std::deque<RocketComponent*>        queue(children.begin(), children.end());
    while (!queue.empty())
    {
        RocketComponent* const node = queue.front();
        queue.pop_front();
        result.push_back(node);
        const std::vector<RocketComponent*> below = node->getChildren();
        queue.insert(queue.end(), below.begin(), below.end());
    }
    return result;
}

/// The number of components below @p component.
[[nodiscard]] std::size_t descendantCount(const RocketComponent& component)
{
    return component.getAllChildren().size();
}

/// Whether one of @p components is visible.
template <class Components>
[[nodiscard]] bool anyVisible(const Components& components)
{
    return std::ranges::any_of(components, [](const RocketComponent* c) { return c->isVisible(); });
}

/// Whether @p component is a stage (an AxialStage: a booster set is one).
[[nodiscard]] bool isStageComponent(const RocketComponent& component)
{
    return isStage(component.kind());
}

/// The stages among @p components, in their order.
template <class Component>
[[nodiscard]] std::vector<Component*> stagesOf(const std::vector<Component*>& components)
{
    std::vector<Component*> stages;
    std::ranges::copy_if(components, std::back_inserter(stages),
                         [](const RocketComponent* c) { return isStageComponent(*c); });
    return stages;
}

/// Whether stage @p stage is active in @p configuration.
[[nodiscard]] bool isActive(const FlightConfiguration& configuration, const RocketComponent& stage)
{
    return configuration.isStageActive(stage.getStageNumber());
}

/// Calls @p change with @p rocket frozen (unless it is frozen already), so that the rocket fires
/// one event for all it changes; thaws it also when @p change throws.
template <class Change>
void changeFrozen(Rocket& rocket, Change&& change)
{
    const bool freezing = !rocket.isFrozen();
    if (freezing)
    {
        rocket.freeze();
    }
    try
    {
        std::forward<Change>(change)();
    }
    catch (...)
    {
        if (freezing)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freezing)
    {
        rocket.thaw();
    }
}

/// Sets @p action's text (shown as it is), tooltip and icon.
void setLook(QAction& action, const QString& text, const QString& toolTip, Icons::Id icon)
{
    action.setText(MenuText::escaped(text));
    action.setToolTip(toolTip);
    action.setIcon(Icons::icon(icon));
}

/// Makes an action of @p parent named @p objectName with the text @p text (shown as it is), the
/// tooltip @p toolTip and the icon @p icon, disabled until the rules say otherwise.
QAction* makeAction(QObject* parent, const QString& objectName, const QString& text,
                    const QString& toolTip, Icons::Id icon)
{
    auto* action = makeChild<QAction>(parent);
    action->setObjectName(objectName);
    action->setMenuRole(QAction::NoRole);
    setLook(*action, text, toolTip, icon);
    action->setEnabled(false);
    return action;
}

/// Sets @p action enabled when it exists (a menu's action that is gone is left alone).
void setEnabled(const QPointer<QAction>& action, bool enabled)
{
    if (!action.isNull())
    {
        action->setEnabled(enabled);
    }
}

}  // namespace

RocketActions::RocketActions(MainMenus& menus, OpenRocketClipboard& clipboard, UserPrompts& prompts,
                             DocumentSelectionModel* selection)
  : QObject(checked(selection)),
    m_selection(selection),
    m_editing(*selection, clipboard, prompts, &selection->view()),
    m_edit(checked(menus).editAction()),
    m_cut(menus.cutAction()),
    m_copy(menus.copyAction()),
    m_paste(menus.pasteAction()),
    m_duplicate(menus.duplicateAction()),
    m_delete(menus.deleteAction()),
    m_deselectAll(menus.deselectAllAction()),
    m_toggleVisibility(menus.toggleVisibilityAction()),
    m_showAll(menus.showAllComponentsAction())
{
    createOwnActions();
    connectActions();
    createContextMenu();

    connect(m_selection, &DocumentSelectionModel::changed, this,
            guarded([this] { updateActions(); }, QStringLiteral("Rocket actions")));
    connect(&clipboard, &OpenRocketClipboard::changed, this,
            guarded([this] { updateActions(); }, QStringLiteral("Rocket actions")));
    m_rocketChanges = m_selection->document().getRocket().addComponentChangeListener(
        guarded([this](const ComponentChangeEvent& event) { rocketChanged(event); },
                QStringLiteral("Rocket actions")));
    updateActions();
    prepareContextMenu();
}

RocketActions::~RocketActions() = default;

void RocketActions::createOwnActions()
{
    //: RocketActions.MoveUpAct.Moveup
    m_moveUp = makeAction(this, QStringLiteral("moveUpAction"), tr("Move up"),
                          //: RocketActions.MoveUpAct.ttip.Moveup
                          tr("Move this component upwards."), Icons::Id::UP);
    //: RocketActions.MoveDownAct.Movedown
    m_moveDown = makeAction(this, QStringLiteral("moveDownAction"), tr("Move down"),
                            //: RocketActions.MoveDownAct.ttip.Movedown
                            tr("Move this component downwards."), Icons::Id::DOWN);
    // ContextMenuToggleVisibilityAction: made by ToggleVisibilityAction's constructor, which
    // gives it the menu action's first text.
    m_contextToggleVisibility =
        //: RocketActions.VisibilityAct.HideSelected
        makeAction(this, QStringLiteral("toggleVisibilityContextMenuAction"), tr("Hide selected"),
                   //: RocketActions.VisibilityAct.ttip.HideSelected
                   tr("Hide selected component(s)."), Icons::Id::COMPONENT_HIDDEN);
    m_toggleActive =
        //: RocketActions.StageActiveAct.DisableSelected
        makeAction(this, QStringLiteral("toggleActiveAction"), tr("Disable"),
                   //: RocketActions.StageActiveAct.ttip.DisableSelected
                   tr("Disable selected component(s) in the current flight configuration."),
                   Icons::Id::COMPONENT_DISABLED);
}

void RocketActions::connectActions()
{
    connect(m_edit.data(), &QAction::triggered, this,
            guarded([this] { edit(); }, QStringLiteral("Edit")));
    connect(m_cut.data(), &QAction::triggered, this,
            guarded([this] { runClosingDialog(m_editing.canCut(), &ComponentEditing::cut); },
                    QStringLiteral("Cut")));
    connect(m_copy.data(), &QAction::triggered, this,
            guarded([this] { static_cast<void>(m_editing.copy()); }, QStringLiteral("Copy")));
    connect(m_paste.data(), &QAction::triggered, this,
            guarded([this] { runClosingDialog(m_editing.canPaste(), &ComponentEditing::paste); },
                    QStringLiteral("Paste")));
    connect(
        m_duplicate.data(), &QAction::triggered, this,
        guarded(
            [this] { runClosingDialog(m_editing.canDuplicate(), &ComponentEditing::duplicate); },
            QStringLiteral("Duplicate")));
    connect(
        m_delete.data(), &QAction::triggered, this,
        guarded(
            [this] { runClosingDialog(m_editing.canDelete(), &ComponentEditing::deleteSelection); },
            QStringLiteral("Delete")));
    connect(m_moveUp, &QAction::triggered, this,
            guarded([this] { runClosingDialog(m_editing.canMoveUp(), &ComponentEditing::moveUp); },
                    QStringLiteral("Move up")));
    connect(
        m_moveDown, &QAction::triggered, this,
        guarded([this] { runClosingDialog(m_editing.canMoveDown(), &ComponentEditing::moveDown); },
                QStringLiteral("Move down")));
    connect(m_deselectAll.data(), &QAction::triggered, this,
            guarded([this] { deselectAll(); }, QStringLiteral("Deselect all")));
    connect(m_toggleVisibility.data(), &QAction::triggered, this,
            guarded([this] { toggleVisibility(); }, QStringLiteral("Hide/Show")));
    connect(m_contextToggleVisibility, &QAction::triggered, this,
            guarded([this] { toggleVisibility(); }, QStringLiteral("Hide/Show")));
    connect(m_showAll.data(), &QAction::triggered, this,
            guarded([this] { showAll(); }, QStringLiteral("Show all")));
    connect(m_toggleActive, &QAction::triggered, this,
            guarded([this] { toggleActive(); }, QStringLiteral("Disable/Enable")));
}

void RocketActions::createContextMenu()
{
    // BasicFrame's popupMenu, with the items of later milestones left out (see the class
    // comment).
    m_contextMenu = makeChild<QMenu>(&m_selection->view());
    m_contextMenu->setObjectName(QStringLiteral("componentTreePopupMenu"));
    // Swing shows the actions' tooltips.
    m_contextMenu->setToolTipsVisible(true);
    m_contextMenu->addAction(m_edit.data());
    m_contextMenu->addAction(m_cut.data());
    m_contextMenu->addAction(m_copy.data());
    m_contextMenu->addAction(m_paste.data());
    m_contextMenu->addAction(m_duplicate.data());
    m_contextMenu->addAction(m_delete.data());
    m_contextMenu->addSeparator();
    //: RocketActions.Select
    auto* select = makeChild<QMenu>(m_contextMenu.data(), MenuText::escaped(tr("Select")));
    select->setObjectName(QStringLiteral("componentTreePopupSelectMenu"));
    select->setToolTipsVisible(true);
    select->menuAction()->setMenuRole(QAction::NoRole);
    select->addAction(m_deselectAll.data());
    m_contextMenu->addMenu(select);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_contextToggleVisibility);
    m_contextMenu->addAction(m_toggleActive);
    connect(m_contextMenu.data(), &QMenu::aboutToShow, this,
            guarded([this] { prepareContextMenu(); }, QStringLiteral("Component tree popup")));
}

void RocketActions::showContextMenu(const QPoint& globalPosition)
{
    if (m_contextMenu.isNull())
    {
        return;
    }
    prepareContextMenu();
    m_contextMenu->popup(globalPosition);
}

void RocketActions::prepareContextMenu()
{
    const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
    const bool                          onlyStages =
        !selected.empty() && std::ranges::all_of(selected, [](const RocketComponent* c) {
            return isStageComponent(*c);
        });
    if (m_contextMenu.isNull())
    {
        return;
    }
    // The item, not the action, is shown or not (JMenuItem.setVisible()): a hidden QAction would
    // be disabled too.
    const bool inMenu = m_contextMenu->actions().contains(m_toggleActive);
    if (onlyStages && !inMenu)
    {
        m_contextMenu->addAction(m_toggleActive);
    }
    else if (!onlyStages && inMenu)
    {
        m_contextMenu->removeAction(m_toggleActive);
    }
}

// ----------------------------------------------------------------------------- the rules

void RocketActions::updateActions()
{
    const std::vector<RocketComponent*> selected    = m_selection->selectedComponents();
    const bool                          anySelected = !selected.empty();
    setEnabled(m_edit, anySelected);
    setEnabled(m_cut, m_editing.canCut());
    setEnabled(m_copy, m_editing.canCopy());
    setEnabled(m_paste, m_editing.canPaste());
    setEnabled(m_delete, m_editing.canDelete());
    setEnabled(m_duplicate, m_editing.canDuplicate());
    setEnabled(m_deselectAll, anySelected);
    m_moveUp->setEnabled(m_editing.canMoveUp());
    m_moveDown->setEnabled(m_editing.canMoveDown());
    updateVisibilityActions();
    // ShowAllComponentsAction: some component below the Rocket is hidden.
    const std::vector<RocketComponent*> all = descendantsOf(m_selection->document().getRocket());
    setEnabled(m_showAll,
               !std::ranges::all_of(all, [](const RocketComponent* c) { return c->isVisible(); }));
    updateToggleActiveAction();
}

void RocketActions::updateVisibilityActions()
{
    const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
    setEnabled(m_toggleVisibility, !selected.empty());
    m_contextToggleVisibility->setEnabled(!selected.empty());
    if (selected.empty())
    {
        // The texts stay as they were (Java returns before it sets them).
        return;
    }
    const Rocket& rocket = m_selection->document().getRocket();
    QString       text;
    QString       toolTip;
    Icons::Id     icon = Icons::Id::COMPONENT_HIDDEN;
    if (isRocketSelected(rocket, constList(selected)))
    {
        const bool visible = rocket.isVisible();
        //: RocketActions.VisibilityAct.HideAll
        text = visible ? tr("Hide all")
                       //: RocketActions.VisibilityAct.ShowAll
                       : tr("Show all");
        //: RocketActions.VisibilityAct.ttip.HideAll
        toolTip = visible ? tr("Hide all components.")
                          //: RocketActions.VisibilityAct.ttip.ShowAll
                          : tr("Show all components.");
        icon    = visible ? Icons::Id::COMPONENT_HIDDEN : Icons::Id::COMPONENT_SHOWING;
    }
    else
    {
        const bool visible = anyVisible(selected);
        //: RocketActions.VisibilityAct.HideSelected
        text = visible ? tr("Hide selected")
                       //: RocketActions.VisibilityAct.ShowSelected
                       : tr("Show selected");
        //: RocketActions.VisibilityAct.ttip.HideSelected
        toolTip = visible ? tr("Hide selected component(s).")
                          //: RocketActions.VisibilityAct.ttip.ShowSelected
                          : tr("Show selected component(s).");
        icon    = visible ? Icons::Id::COMPONENT_HIDDEN : Icons::Id::COMPONENT_SHOWING;
    }
    if (!m_toggleVisibility.isNull())
    {
        setLook(*m_toggleVisibility, text, toolTip, icon);
    }
    // ContextMenuToggleVisibilityAction: the same, then its own short text.
    //: RocketActions.VisibilityAct.Hide
    setLook(*m_contextToggleVisibility,
            anyVisible(selected) ? tr("Hide")
                                 //: RocketActions.VisibilityAct.Show
                                 : tr("Show"),
            toolTip, icon);
}

void RocketActions::updateToggleActiveAction()
{
    const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
    const std::vector<RocketComponent*> stages   = stagesOf(selected);
    m_toggleActive->setEnabled(!stages.empty());
    if (stages.empty())
    {
        return;
    }
    const Rocket&              rocket        = m_selection->document().getRocket();
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    // isStageActive() is false for a stage without children, so those are asked apart.
    const bool anyWithoutChildren = std::ranges::any_of(
        stages, [](const RocketComponent* stage) { return stage->getChildCount() == 0; });
    const bool anyActive = std::ranges::any_of(
        stages, [&configuration](const RocketComponent* s) { return isActive(configuration, *s); });
    if (anyWithoutChildren || anyActive)
    {
        const std::optional<QString> reason =
            cannotDisableReason(rocket, configuration, constList(stages));
        setLook(*m_toggleActive,
                //: RocketActions.StageActiveAct.DisableSelected
                tr("Disable"),
                reason.has_value()
                    ? *reason
                    //: RocketActions.StageActiveAct.ttip.DisableSelected
                    : tr("Disable selected component(s) in the current flight configuration."),
                Icons::Id::COMPONENT_DISABLED);
        m_toggleActive->setEnabled(!reason.has_value());
    }
    else
    {
        setLook(*m_toggleActive,
                //: RocketActions.StageActiveAct.EnableSelected
                tr("Enable"),
                //: RocketActions.StageActiveAct.ttip.EnableSelected
                tr("Enable selected stage(s) in the current flight configuration."),
                Icons::Id::COMPONENT_ENABLED);
        m_toggleActive->setEnabled(true);
    }
}

bool RocketActions::isRocketSelected(const Rocket&                           rocket,
                                     std::span<const RocketComponent* const> selection)
{
    const bool rocketSelected = std::ranges::any_of(
        selection, [](const RocketComponent* c) { return c->kind() == ComponentKind::ROCKET; });
    return rocketSelected || descendantCount(rocket) == selection.size();
}

std::optional<QString> RocketActions::cannotDisableReason(
    const Rocket& rocket, const FlightConfiguration& configuration,
    std::span<const RocketComponent* const> selection)
{
    ComponentList stages;
    std::ranges::copy_if(selection, std::back_inserter(stages),
                         [](const RocketComponent* c) { return isStageComponent(*c); });
    // Stages without children can never be toggled (StageSelector's StageAction).
    if (std::ranges::any_of(
            stages, [](const RocketComponent* stage) { return stage->getChildCount() == 0; }))
    {
        //: RocketActions.StageActiveAct.ttip.CannotDisableNoChildren
        return tr("Cannot disable a stage with no components.");
    }
    if (!stages.empty())
    {
        const std::vector<const ComponentAssembly*> assemblies = rocket.getAllChildAssemblies();
        const auto                                  totalActive =
            std::ranges::count_if(assemblies, [&configuration](const ComponentAssembly* assembly) {
                return isStageComponent(*assembly) && isActive(configuration, *assembly);
            });
        const auto selectedActive =
            std::ranges::count_if(stages, [&configuration](const RocketComponent* stage) {
                return isActive(configuration, *stage);
            });
        if (selectedActive > 0 && totalActive - selectedActive <= 0)
        {
            //: RocketActions.StageActiveAct.ttip.CannotDisableLastStage
            return tr("Cannot disable the only active stage.");
        }
    }
    return std::nullopt;
}

void RocketActions::rocketChanged(const ComponentChangeEvent& /*event*/)
{
    updateActions();
}

// ----------------------------------------------------------------------------- the actions

void RocketActions::edit()
{
    if (m_selection->selectedComponent() == nullptr)
    {
        return;
    }
    Q_EMIT closeConfigDialogRequested();
    // Read again: closing the dialog may have changed the design.
    RocketComponent* first = m_selection->selectedComponent();
    if (first != nullptr)
    {
        Q_EMIT editRequested(first, true);
    }
}

void RocketActions::deselectAll()
{
    m_selection->clearComponentSelection();
}

void RocketActions::runClosingDialog(bool allowed, bool (ComponentEditing::*operation)())
{
    if (!allowed)
    {
        return;
    }
    Q_EMIT closeConfigDialogRequested();
    static_cast<void>((m_editing.*operation)());
}

void RocketActions::toggleVisibility()
{
    const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
    if (selected.empty())
    {
        return;
    }
    Rocket& rocket = m_selection->document().getRocket();
    // Frozen: no listener runs before thaw(), so the pointers stay valid throughout.
    changeFrozen(rocket, [&rocket, &selected] {
        if (isRocketSelected(rocket, constList(selected)))
        {
            const bool visible = !rocket.isVisible();
            rocket.setVisible(visible);
            for (RocketComponent* descendant : descendantsOf(rocket))
            {
                descendant->setVisible(visible);
            }
            return;
        }
        const bool visible = !anyVisible(selected);
        // The components that are not stages, each with its stage and the rocket after it.
        for (RocketComponent* component : selected)
        {
            if (isStageComponent(*component) || component->kind() == ComponentKind::ROCKET)
            {
                continue;
            }
            component->setVisible(visible);
            RocketComponent& stage = component->getStage();
            stage.setVisible(anyVisible(descendantsOf(stage)));
            rocket.setVisible(anyVisible(descendantsOf(rocket)));
        }
        // The stages with everything below them.
        for (RocketComponent* stage : stagesOf(selected))
        {
            stage->setVisible(visible);
            for (RocketComponent* descendant : descendantsOf(*stage))
            {
                descendant->setVisible(visible);
            }
        }
    });
}

void RocketActions::showAll()
{
    Rocket& rocket = m_selection->document().getRocket();
    changeFrozen(rocket, [&rocket] {
        rocket.setVisible(true);
        for (RocketComponent* descendant : descendantsOf(rocket))
        {
            descendant->setVisible(true);
        }
    });
}

void RocketActions::toggleActive()
{
    const std::vector<RocketComponent*> stages = stagesOf(m_selection->selectedComponents());
    if (stages.empty())
    {
        return;
    }
    Rocket&              rocket        = m_selection->document().getRocket();
    FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    const bool           activate      = std::ranges::none_of(
        stages, [&configuration](const RocketComponent* s) { return isActive(configuration, *s); });
    const FlightConfigurationId configurationId = configuration.getId();
    // toggleStage() fires no event: the pointers stay valid.
    for (const RocketComponent* stage : stages)
    {
        if (isActive(configuration, *stage) != activate)
        {
            configuration.toggleStage(stage->getStageNumber());
        }
    }
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange |
                                        ComponentChangeEvent::kMotorChange |
                                        ComponentChangeEvent::kTreeChangeChildren,
                                    configurationId);
}

}  // namespace QtRocket::Gui
