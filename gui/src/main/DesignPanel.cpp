#include "QtRocket/gui/main/DesignPanel.h"

#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include <QAction>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QMetaObject>
#include <QObject>
#include <QPoint>
#include <QScrollArea>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/ComponentAddButtons.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/SplitPane.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Design panel");
}

/// A font that differs from the one it is set on in its weight only: bold or not.
[[nodiscard]] QFont weightOnly(bool bold)
{
    QFont font;
    font.setBold(bold);
    return font;
}

}  // namespace

DesignPanel::DesignPanel(MainWindow& window, OpenRocketClipboard& clipboard, QWidget* parent)
  : QWidget(parent),
    m_window(&window),
    m_splitter(makeChild<SplitPane>(this, Qt::Horizontal, kResizeWeight, kDividerLocation)),
    // Made after the split pane, so that the panel deletes it after the tree (QObject deletes
    // its children in the order they were made).
    m_model(makeChild<ComponentTreeModel>(this, window.document(), window.services().preferences()))
{
    setObjectName(QStringLiteral("designPanel"));
    m_splitter->setObjectName(QStringLiteral("designSplitPane"));
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    // The add buttons' side is made first and so deleted first: its buttons read the selection,
    // which the tree's side holds.
    m_addBox                = makeChild<QGroupBox>(m_splitter, ComponentAddButtons::title());
    QWidget* const treeSide = makeTreeSide(clipboard);
    makeAddButtonsSide();
    m_splitter->insertWidget(0, treeSide);
    // Both sides stretch alike until the divider is placed (SplitPane).
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    connectRequests();

    // BasicFrame: the first stage is selected, and the figure highlights it alone.
    Rocket& rocket = window.document().getRocket();
    if (rocket.getChildCount() > 0)
    {
        m_selection->setSelectedComponent(rocket.getChild(0));
        m_startHighlight = true;
        m_tree->viewport()->installEventFilter(this);
    }
}

DesignPanel::~DesignPanel()
{
    if (m_startHighlight)
    {
        m_tree->viewport()->removeEventFilter(this);
    }
}

QWidget* DesignPanel::makeTreeSide(OpenRocketClipboard& clipboard)
{
    auto* side = makeChild<QWidget>(m_splitter);
    side->setObjectName(QStringLiteral("componentTreePanel"));
    m_tree = makeChild<ComponentTreeView>(side, *m_model);
    m_tree->setMinimumWidth(kTreeMinimumWidth);
    m_selection = makeChild<DocumentSelectionModel>(m_tree);
    m_actions   = makeChild<RocketActions>(m_selection, m_window->menus(), clipboard,
                                           m_window->services().prompts());

    const std::array<QAction*, kTreeButtonCount> actions{
        m_actions->moveUpAction(), m_actions->moveDownAction(), m_actions->editAction(),
        m_actions->duplicateAction(), m_actions->deleteAction()};
    const std::array<QString, kTreeButtonCount> names{
        QStringLiteral("moveUpButton"), QStringLiteral("moveDownButton"),
        QStringLiteral("editButton"), QStringLiteral("duplicateButton"),
        QStringLiteral("deleteButton")};
    // MigLayout "fill, flowy", "[grow][grow 0]", "[grow]": the tree spans the rows of the
    // buttons, which share the height alike.
    auto* grid = makeChild<QGridLayout>(side);
    grid->addWidget(m_tree, 0, 0, static_cast<int>(kTreeButtonCount), 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);
    for (std::size_t index = 0; index < kTreeButtonCount; ++index)
    {
        const int   row         = static_cast<int>(index);
        auto* const button      = makeChild<IconButton>(side);
        m_treeButtons.at(index) = button;
        button->setObjectName(names.at(index));
        button->setAction(actions.at(index));
        grid->setRowStretch(row, 1);
        if (index == 0)
        {
            // "aligny 65%": a cell of its own, with the free space above and below the button
            // shared 65 to 35.
            auto* cell = makeChild<QWidget>(side);
            cell->setObjectName(QStringLiteral("moveUpCell"));
            auto* cellLayout = makeChild<QVBoxLayout>(cell);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->addStretch(kMoveUpAlignment);
            cellLayout->addWidget(button);
            cellLayout->addStretch(100 - kMoveUpAlignment);
            grid->addWidget(cell, row, 1);
        }
        else if (index == 1)
        {
            // "aligny 0%".
            grid->addWidget(button, row, 1, Qt::AlignTop);
        }
        else
        {
            // In the middle of its row (a vertical alignment, so that the row may grow beyond
            // the button's fixed height).
            grid->addWidget(button, row, 1, Qt::AlignVCenter);
        }
    }
    return side;
}

void DesignPanel::makeAddButtonsSide()
{
    m_addBox->setObjectName(QStringLiteral("addNewComponentBox"));
    // The title in bold (GUIUtil.changeFontStyle(border, Font.BOLD)), the buttons as they are.
    m_addBox->setFont(weightOnly(true));
    // "[0::]": the side may be as narrow as nothing.
    m_addBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* layout = makeChild<QVBoxLayout>(m_addBox);
    m_addScroll  = makeChild<QScrollArea>(m_addBox);
    m_addScroll->setObjectName(QStringLiteral("componentAddButtonsScrollPane"));
    m_addScroll->setFont(weightOnly(false));
    m_addScroll->setFrameShape(QFrame::NoFrame);
    m_addScroll->setWidgetResizable(true);
    m_addScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_addScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_addButtons = makeChild<ComponentAddButtons>(m_addScroll, *m_selection, m_window->services());
    m_addScroll->setWidget(m_addButtons);
    layout->addWidget(m_addScroll);
}

void DesignPanel::connectRequests()
{
    // The requests for the dialog hand over components that live only during the emission:
    // direct connections.
    connect(m_tree, &ComponentTreeView::editRequested, this,
            guarded([this](RocketComponent* component,
                           bool withSelection) { requestEdit(*component, withSelection); },
                    context()));
    connect(m_actions, &RocketActions::editRequested, this,
            guarded([this](RocketComponent* component,
                           bool withSelection) { requestEdit(*component, withSelection); },
                    context()));
    connect(m_actions, &RocketActions::closeConfigDialogRequested, this,
            guarded([this] { m_window->requestConfigDialogClose(); }, context()));
    connect(m_addButtons, &ComponentAddButtons::addedComponentEditRequested, this,
            guarded(
                [this](RocketComponent* component) {
                    // ComponentButton: showDialog(..., rememberPreviousTab false, isNewComponent
                    // true).
                    m_window->requestConfigDialog(*component, {}, true, false);
                },
                context()));
    connect(m_tree, &ComponentTreeView::contextMenuRequested, this,
            guarded([this](const QPoint& position) { m_actions->showContextMenu(position); },
                    context()));
    connect(m_selection, &DocumentSelectionModel::changed, this,
            guarded([this] { selectionChanged(); }, context()));
    // A request for the dialog after a change of the selection (an add button's for its new
    // component) is the one that holds.
    connect(m_window, &MainWindow::configDialogRequested, this,
            [this] { m_reopenPending = false; });
    // A refused drop says why, over the window.
    m_model->setUserPrompts(&m_window->services().prompts(), m_window);
}

void DesignPanel::requestEdit(RocketComponent& component, bool withSelection)
{
    std::vector<RocketComponent*> others;
    if (withSelection)
    {
        others = m_selection->selectedComponents();
        std::erase(others, &component);
    }
    m_window->requestConfigDialog(component, others);
}

void DesignPanel::takeTheSpotlight()
{
    m_tree->setFocus();
}

void DesignPanel::setConfigDialogVisible(std::function<bool()> visible)
{
    m_dialogVisible = std::move(visible);
    if (m_dialogVisible)
    {
        m_tree->setEditAllowed([visible = m_dialogVisible] { return !visible(); });
    }
    else
    {
        m_tree->setEditAllowed({});
    }
}

std::vector<RocketComponent*> DesignPanel::highlightedComponents() const
{
    return m_startHighlight ? m_selection->selectedComponents()
                            : m_selection->highlightedComponents();
}

void DesignPanel::selectionChanged()
{
    if (m_startHighlight)
    {
        endStartHighlight();
    }
    else
    {
        Q_EMIT highlightChanged();
    }
    // DesignPanel's listener: the dialog follows the selection while it is visible.
    if (!m_reopenPending && m_dialogVisible && m_dialogVisible())
    {
        m_reopenPending = true;
        QMetaObject::invokeMethod(
            this, [this] { Guard::run([this] { reopenConfigDialog(); }, context()); },
            Qt::QueuedConnection);
    }
}

void DesignPanel::reopenPendingConfigDialog()
{
    // The queued call then finds nothing pending.
    reopenConfigDialog();
}

void DesignPanel::reopenConfigDialog()
{
    if (!m_reopenPending)
    {
        return;
    }
    m_reopenPending = false;
    if (!m_dialogVisible || !m_dialogVisible())
    {
        return;
    }
    if (m_selection->selectedComponent() == nullptr)
    {
        return;
    }
    // ComponentConfigDialog.disposeDialog(), then showDialog() for the first selected component
    // with the others.
    m_window->requestConfigDialogClose();
    // Closing commits the dialog's edits, which may have changed the design: read the selection
    // after it.
    const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
    if (selected.empty())
    {
        return;
    }
    m_window->requestConfigDialog(*selected.front(), std::span(selected).subspan(1));
}

void DesignPanel::endStartHighlight()
{
    m_startHighlight = false;
    m_tree->viewport()->removeEventFilter(this);
    Q_EMIT highlightChanged();
}

bool DesignPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (m_startHighlight && watched == m_tree->viewport() &&
        event->type() == QEvent::MouseButtonRelease)
    {
        endStartHighlight();
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace QtRocket::Gui
