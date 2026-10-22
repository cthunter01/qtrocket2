#include "QtRocket/gui/main/ConfigDialogRequests.h"

#include <vector>

#include <QObject>
#include <QString>

#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Component configuration");
}

}  // namespace

ConfigDialogRequests::ConfigDialogRequests(WindowManager& windows, QObject* parent)
  : QObject(parent)
{
    for (MainWindow* const window : windows.windows())
    {
        connectWindow(*window);
    }
    connect(&windows, &WindowManager::windowOpened, this,
            guarded([this](MainWindow* window) { connectWindow(*window); }, context()));
}

void ConfigDialogRequests::connectWindow(MainWindow& window)
{
    // The requests hand over components that live during the emission only: direct
    // connections (the window and this object share the GUI thread).
    connect(&window, &MainWindow::configDialogRequested, this,
            guarded(
                [&window](RocketComponent* component, const std::vector<RocketComponent*>& others,
                          bool isNewComponent, bool rememberPreviousTab) {
                    // ComponentConfigDialog.showDialog(frame, document, component, ...).
                    ComponentConfigDialog::showDialog(window, window.services(), window.document(),
                                                      *component, others, isNewComponent,
                                                      rememberPreviousTab);
                },
                context()));
    connect(&window, &MainWindow::closeConfigDialogRequested, this,
            guarded([] { ComponentConfigDialog::disposeDialog(); }, context()));
    // The edits still being typed go into the design before the undo or redo replaces it.
    connect(
        &window.undoRedoActions(), &UndoRedoActions::aboutToUndoOrRedo, this,
        guarded([] { static_cast<void>(ComponentConfigDialog::commitPendingEdits()); }, context()));
    if (DesignPanel* const panel = window.componentDesignPanel(); panel != nullptr)
    {
        panel->setConfigDialogVisible([] { return ComponentConfigDialog::isDialogVisible(); });
    }
}

}  // namespace QtRocket::Gui
