#include "QtRocket/gui/main/ExampleDesignsMenu.h"

#include <filesystem>
#include <optional>

#include <QAction>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/ExampleDesigns.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
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

ExampleDesignsMenu::ExampleDesignsMenu(const std::filesystem::path& directory,
                                       DocumentFileOperations& operations, MainWindow* window,
                                       QMenu* menu)
  : QObject(nonNullMenu(menu)),
    m_menu(menu),
    m_designs(directory),
    m_operations(&operations),
    m_window(window)
{
    updateMenu();
    m_menu->menuAction()->setEnabled(true);
}

MainWindow* ExampleDesignsMenu::open(const ExampleDesigns::Design& design)
{
    // A raw pointer checked where it is used (GCC's -Wnull-dereference does not follow
    // QPointer::isNull() to operator->()).
    DocumentFileOperations* const operations = m_operations.data();
    if (operations == nullptr)
    {
        return nullptr;
    }
    return operations->openExample(design.file, m_window.data());
}

void ExampleDesignsMenu::updateMenu()
{
    // removeAll().
    const QList<QAction*> items = m_menu->actions();
    for (QAction* const item : items)
    {
        m_menu->removeAction(item);
        item->deleteLater();
    }
    for (const std::optional<ExampleDesigns::Design>& entry : m_designs.menuEntries())
    {
        if (!entry.has_value())
        {
            m_menu->addSeparator();
            continue;
        }
        auto* item = makeChild<QAction>(m_menu, MenuText::escaped(entry->name));
        // No menu role: macOS would move an item by its text otherwise (MainMenus).
        item->setMenuRole(QAction::NoRole);
        connect(item, &QAction::triggered, this,
                guarded([this, design = *entry] { static_cast<void>(open(design)); },
                        QStringLiteral("Open example")));
        m_menu->addAction(item);
    }
}

}  // namespace QtRocket::Gui
