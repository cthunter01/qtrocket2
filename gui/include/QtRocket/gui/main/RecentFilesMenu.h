#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/RecentFiles.h"

class QMenu;

namespace QtRocket::Gui
{

/// File > Open recent (MRUDesignFileAction): the menu it is the child of lists the recent files
/// (RecentFiles), the newest first, an item per file whose text is the file's path as the list
/// holds it, and follows every change of the list. Choosing an item opens the file
/// (DocumentFileOperations::open(), BasicFrame.open(new File(path), parent)) from the window of
/// the menu (none for the menus of no window): when it opens (or its window is activated), the
/// file goes to the top of the list again; when it cannot be opened (the error was shown), it is
/// taken out of the list. The menu is enabled, also while it is empty (a JMenu is).
///
/// The items of a list that changed are taken out of the menu and deleted later (deleteLater()):
/// the list changes while the item that was chosen is still being triggered. The recent files
/// and the file operations may go before the menu: their items then do nothing.
class RecentFilesMenu final : public QObject
{
    Q_OBJECT

public:
    /// Fills @p menu (the parent, not null; its items are replaced) with the files of
    /// @p recentFiles, opened by @p operations from @p window (null for the menus of no window).
    RecentFilesMenu(RecentFiles& recentFiles, DocumentFileOperations& operations,
                    MainWindow* window, QMenu* menu);

    /// The menu filled.
    [[nodiscard]] QMenu* menu() const noexcept { return m_menu; }

    /// What choosing the item of @p file does (MRUDesignFileAction.createAction()): opens it, and
    /// then puts it at the top of the list, or takes it out when it could not be opened. The
    /// window of the file, or null. The list is updated also when this menu went with its window
    /// during one of the open's questions; nothing of the menu is used after the open.
    MainWindow* open(const QString& file);

private:
    /// updateMenu(): an item per recent file.
    void updateMenu();

    QMenu*                           m_menu;
    QPointer<RecentFiles>            m_recentFiles;
    QPointer<DocumentFileOperations> m_operations;
    QPointer<MainWindow>             m_window;
};

}  // namespace QtRocket::Gui
