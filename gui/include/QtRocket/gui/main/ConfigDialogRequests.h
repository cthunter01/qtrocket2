#pragma once

#include <QObject>

namespace QtRocket::Gui
{

class MainWindow;
class WindowManager;

/// Answers the design windows' requests for the component configuration dialog
/// (ComponentConfigDialog, the application's one dialog), for every window of one WindowManager:
/// those open when it is made and every one opened later (WindowManager::windowOpened()). In
/// OpenRocket the parts call ComponentConfigDialog's statics themselves (BasicFrame, DesignPanel,
/// RocketPanel, ComponentButton, RocketActions); here they ask their window by signal (decision
/// O12), and this object connects each window to the dialog:
/// - MainWindow::configDialogRequested() (a double-click in the tree or in the figure, with Shift
///   or Ctrl for the other selected components too; Edit; an add button's new component; File >
///   Properties and an example's rocket; the dialog following the tree's selection) shows
///   ComponentConfigDialog::showDialog() for that window, its services and its document, with
///   the request's other components, new component and remembered tab;
/// - MainWindow::closeConfigDialogRequested() (RocketActions before a change of the design, the
///   window closing: BasicFrame.closeAction()) closes the dialog, keeping its edits
///   (ComponentConfigDialog::disposeDialog());
/// - Undo and Redo first commit the dialog's edits still being typed
///   (UndoRedoActions::aboutToUndoOrRedo(), ComponentConfigDialog::commitPendingEdits(): the
///   window's menu or key may leave them pending); the undo or redo then closes the dialog (an
///   UNDO change);
/// - the window's design panel learns whether the dialog is visible
///   (DesignPanel::setConfigDialogVisible() with ComponentConfigDialog::isDialogVisible()): while
///   it is, a double-click in the tree does not edit, and the dialog follows the tree's selection
///   (closed and shown again for the first selected component with the others, the remembered tab
///   kept, one turn of the event loop later, or at once for Save as: DesignPanel).
/// The dialog is the application's, so that a request of one window closes the dialog another
/// window showed, as in OpenRocket.
///
/// The application makes one (app/ApplicationSession). Without it the requests go unanswered:
/// the windows of the tests of the parts record them instead. Destroyed, it answers no more
/// requests (its connections go with it), and the design panels keep asking the dialog whether it
/// is visible, which it then never is again.
class ConfigDialogRequests final : public QObject
{
    Q_OBJECT

public:
    /// Answers the requests of the windows of @p windows, those open now and those to come (the
    /// manager may go first: its windows go with it).
    explicit ConfigDialogRequests(WindowManager& windows, QObject* parent = nullptr);

private:
    /// Connects @p window (see the class comment).
    void connectWindow(MainWindow& window);
};

}  // namespace QtRocket::Gui
