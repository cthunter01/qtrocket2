#include "QtRocket/gui/main/RecentFilesMenu.h"

#include <QAction>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/RecentFiles.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p menu, which must not be null.
[[nodiscard]] QMenu* nonNullMenu(QMenu* menu)
{
    QTROCKET_ASSERT(menu != nullptr);
    return menu;
}

}  // namespace

RecentFilesMenu::RecentFilesMenu(RecentFiles& recentFiles, DocumentFileOperations& operations,
                                 MainWindow* window, QMenu* menu)
  : QObject(nonNullMenu(menu)),
    m_menu(menu),
    m_recentFiles(&recentFiles),
    m_operations(&operations),
    m_window(window)
{
    // The MRUDesignFile listener of MRU_FILE_LIST_PROPERTY.
    connect(&recentFiles, &RecentFiles::changed, this,
            guarded([this] { updateMenu(); }, QStringLiteral("Open recent")));
    updateMenu();
    m_menu->menuAction()->setEnabled(true);
}

MainWindow* RecentFilesMenu::open(const QString& file)
{
    // Raw pointers checked where they are used (GCC's -Wnull-dereference does not follow
    // QPointer::isNull() to operator->()).
    DocumentFileOperations* const operations = m_operations.data();
    if (operations == nullptr)
    {
        return nullptr;
    }
    // The open's questions run an event loop, in which this menu may go with its window: what
    // the list needs afterwards is taken now (MRUDesignFileAction updates the MRUDesignFile
    // singleton, whatever became of the menu).
    const QPointer<RecentFiles> recent = m_recentFiles;
    MainWindow* const           opened = operations->open(QtStrings::toPath(file), m_window.data());
    RecentFiles* const          recentFiles = recent.data();
    if (recentFiles == nullptr)
    {
        return opened;
    }
    if (opened != nullptr)
    {
        recentFiles->addFile(file);
    }
    else
    {
        recentFiles->removeFile(file);
    }
    return opened;
}

void RecentFilesMenu::updateMenu()
{
    // removeAll(): the items go later, as one of them may be the item being triggered.
    const QList<QAction*> items = m_menu->actions();
    for (QAction* const item : items)
    {
        m_menu->removeAction(item);
        item->deleteLater();
    }
    const RecentFiles* const recentFiles = m_recentFiles.data();
    if (recentFiles == nullptr)
    {
        return;
    }
    for (const QString& file : recentFiles->files())
    {
        auto* item = makeChild<QAction>(m_menu, MenuText::escaped(file));
        // No menu role: macOS would move an item by its text otherwise (MainMenus).
        item->setMenuRole(QAction::NoRole);
        connect(item, &QAction::triggered, this,
                guarded([this, file] { static_cast<void>(open(file)); },
                        QStringLiteral("Open recent")));
        m_menu->addAction(item);
    }
}

}  // namespace QtRocket::Gui
