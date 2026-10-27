#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include <QObject>
#include <QWidget>

class QEvent;
class QGroupBox;
class QScrollArea;

namespace QtRocket
{
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentAddButtons;
class ComponentTreeModel;
class ComponentTreeView;
class DocumentSelectionModel;
class IconButton;
class MainWindow;
class OpenRocketClipboard;
class RocketActions;
class SplitPane;

/// The "Rocket design" tab of a design window (OpenRocket's DesignPanel, with the parts of
/// BasicFrame that make and wire it): a horizontal split pane (resize weight 0.5, the divider at
/// 0.4 of the width when it is first shown, SplitPane) of
/// - on the left, the component tree (ComponentTreeView of a ComponentTreeModel, at least 140
///   pixels wide, "wmin 140px") with the buttons Move up, Move down, Edit, Duplicate and Delete
///   beside it (IconButton tied to RocketActions' actions, one width for all: "sizegroup
///   buttons"), in MigLayout's arrangement of DesignPanel ("fill, flowy", "[grow][grow 0]"): the
///   tree spans five rows of equal height, one per button; Move up sits at 65 % of the free
///   space of its row ("aligny 65%"), Move down at the top of its row ("aligny 0%"), the others in
///   the middle of theirs (OpenRocket's arrangement as MigLayout computes it: the M2 run 2 probe
///   DesignPanelLayoutProbe);
/// - on the right, under the bold title "Add new component" (BasicFrame.title.Addnewcomp), the
///   add buttons (ComponentAddButtons) in a scroll area that scrolls vertically only; this side
///   may be made as narrow as nothing ("[0::]").
///
/// It makes one DocumentSelectionModel (the window's component selection) and one RocketActions
/// (the component actions, driving the window's Edit menu: MainMenus), and wires them:
/// - the tree's context menu request shows RocketActions' popup menu
///   (BasicFrame.doComponentTreePopup());
/// - every request for the configuration dialog goes to the window (MainWindow::
///   requestConfigDialog(), decision O12): a double-click in the tree (the component alone, or
///   with Shift or Ctrl the component with the other selected ones), Edit (the first selected
///   component with the others), an add button (the new component, isNewComponent, without the
///   remembered tab: ComponentButton's showDialog(..., false, true)); and RocketActions' requests
///   to close the dialog before a change (MainWindow::requestConfigDialogClose());
/// - RocketActions' requests for the design tab after Cut, Copy, Paste, Duplicate and Delete go to
///   the window (MainWindow::selectTab(), BasicFrame.selectTab(DESIGN_TAB));
/// - while the configuration dialog is visible (setConfigDialogVisible()), the tree's
///   double-click does not edit (DesignPanel's !ComponentConfigDialog.isDialogVisible()), and a
///   change of the selection to some components opens the dialog again for them (DesignPanel's
///   second selection listener: the first selected one with the others), unless another request
///   for the dialog came after the change (an add button's, which asks for the new component's
///   own dialog); this is queued (the selection changes inside the rocket's change events, and
///   the dialog adds an undo position), and nothing happens when the dialog closed in between;
/// - the tree's drop refusals (ComponentTreeModel::setUserPrompts()) are shown through the
///   application's UserPrompts over the window.
///
/// When it is made, the first stage is selected (BasicFrame's start: rocket.getChild(0)); the
/// rocket's figure then highlights that stage alone (BasicFrame sets the figure's selection to
/// it), until the selection changes or the tree is first clicked, after which it highlights the
/// selected components with everything below a selected assembly (highlightedComponents(),
/// DesignPanel.highlightAssemblyChildren() and its start-up mouse listener).
///
/// It belongs to its window, the application's clipboard and services outliving it. The tree's
/// model lives as long as the panel and goes after the tree (the destruction order of the parts:
/// the add buttons, the tree with its selection and actions, the model).
///
/// Deviations from OpenRocket: the panel is a widget holding the split pane (Java's DesignPanel
/// is the JSplitPane); the arrangement of the buttons is MigLayout's in QGridLayout terms (the
/// Edit button's "gaptop 20%" moves nothing in MigLayout's result either); the tree takes the
/// keyboard focus when the window opens (takeTheSpotlight(), which BasicFrame, and MainWindow,
/// call when the design tab is chosen); the start-up highlight ends at the first mouse button
/// released in the tree (Java's first mouseClicked); the dialog opens again for a new selection
/// one turn of the event loop later (Java: at once), unless the code that changed the selection
/// outside the rocket's events has it done at once (reopenPendingConfigDialog(): Save as, before
/// its modal dialog).
class DesignPanel final : public QWidget
{
    Q_OBJECT

public:
    /// JSplitPane.setResizeWeight(0.5) of the split pane.
    static constexpr double kResizeWeight = 0.5;
    /// JSplitPane.setDividerLocation(0.4) of the split pane (BasicFrame).
    static constexpr double kDividerLocation = 0.4;
    /// The tree's minimum width ("wmin 140px").
    static constexpr int kTreeMinimumWidth = 140;
    /// Where Move up sits in the free space of its row, in percent ("aligny 65%").
    static constexpr int kMoveUpAlignment = 65;
    /// The number of buttons beside the tree.
    static constexpr std::size_t kTreeButtonCount = 5;

    /// The design panel of @p window, whose document, menus and services it uses, with the
    /// application's component @p clipboard (which outlives it).
    DesignPanel(MainWindow& window, OpenRocketClipboard& clipboard, QWidget* parent);
    ~DesignPanel() override;

    DesignPanel(const DesignPanel&)            = delete;
    DesignPanel& operator=(const DesignPanel&) = delete;
    DesignPanel(DesignPanel&&)                 = delete;
    DesignPanel& operator=(DesignPanel&&)      = delete;

    /// The window the panel belongs to.
    [[nodiscard]] MainWindow& mainWindow() const noexcept { return *m_window; }
    /// The split pane ("designSplitPane"): the tree's side and the add buttons' side.
    [[nodiscard]] SplitPane* splitter() const noexcept { return m_splitter; }
    /// The tree's model, made for the window's document.
    [[nodiscard]] ComponentTreeModel& treeModel() const noexcept { return *m_model; }
    /// The component tree ("componentTree").
    [[nodiscard]] ComponentTreeView& tree() const noexcept { return *m_tree; }
    /// The window's component selection: the tree's.
    [[nodiscard]] DocumentSelectionModel& selection() const noexcept { return *m_selection; }
    /// The window's component actions.
    [[nodiscard]] RocketActions& rocketActions() const noexcept { return *m_actions; }
    /// The buttons beside the tree: Move up, Move down, Edit, Duplicate and Delete.
    [[nodiscard]] const std::array<IconButton*, kTreeButtonCount>& treeButtons() const noexcept
    {
        return m_treeButtons;
    }
    /// The box titled "Add new component" ("addNewComponentBox").
    [[nodiscard]] QGroupBox* addButtonsBox() const noexcept { return m_addBox; }
    /// The scroll area of the add buttons ("componentAddButtonsScrollPane").
    [[nodiscard]] QScrollArea* addButtonsScrollArea() const noexcept { return m_addScroll; }
    /// The add buttons.
    [[nodiscard]] ComponentAddButtons& addButtons() const noexcept { return *m_addButtons; }

    /// Gives the tree the keyboard focus (takeTheSpotlight()).
    void takeTheSpotlight();

    /// Tells the panel whether the configuration dialog is visible
    /// (ComponentConfigDialog.isDialogVisible(): ConfigDialogRequests sets it in the application's
    /// windows); an empty function (the default) says it never is. See the class comment.
    void setConfigDialogVisible(std::function<bool()> visible);

    /// Opens the dialog again for the selection now, when a change of the selection asked for
    /// that and it has not been done yet (see the class comment); nothing otherwise. For a change
    /// of the selection that comes from no event of the rocket (Save as selecting the rocket), so
    /// that the dialog opens before what follows, as Java's listener opens it at once.
    void reopenPendingConfigDialog();

    /// What the rocket's figure is to highlight (see the class comment): the first stage alone
    /// at the start, then DocumentSelectionModel::highlightedComponents(). The pointers are
    /// valid until the next change of the design.
    [[nodiscard]] std::vector<RocketComponent*> highlightedComponents() const;

    /// Whether the start's highlight (the first stage alone) still holds.
    [[nodiscard]] bool isStartHighlight() const noexcept { return m_startHighlight; }

    /// Ends the start's highlight at the first mouse button released in the tree.
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// What highlightedComponents() gives may have changed.
    void highlightChanged();

private:
    /// Makes the tree's side: the tree, the selection, the actions and the buttons.
    [[nodiscard]] QWidget* makeTreeSide(OpenRocketClipboard& clipboard);
    /// Makes the add buttons' side.
    void makeAddButtonsSide();
    /// Connects the requests for the dialog and the popup menu.
    void connectRequests();
    /// Asks for the dialog of @p component, with the other selected components when
    /// @p withSelection (the tree's double-click, Edit).
    void requestEdit(RocketComponent& component, bool withSelection);
    /// The selection changed.
    void selectionChanged();
    /// Opens the dialog again for the selection (queued after a change of the selection).
    void reopenConfigDialog();
    /// Ends the start's highlight.
    void endStartHighlight();

    MainWindow*                               m_window;
    SplitPane*                                m_splitter;
    ComponentTreeModel*                       m_model;
    QGroupBox*                                m_addBox{nullptr};
    QScrollArea*                              m_addScroll{nullptr};
    ComponentAddButtons*                      m_addButtons{nullptr};
    ComponentTreeView*                        m_tree{nullptr};
    DocumentSelectionModel*                   m_selection{nullptr};
    RocketActions*                            m_actions{nullptr};
    std::array<IconButton*, kTreeButtonCount> m_treeButtons{};
    std::function<bool()>                     m_dialogVisible;
    bool                                      m_reopenPending{false};
    bool                                      m_startHighlight{false};
};

}  // namespace QtRocket::Gui
