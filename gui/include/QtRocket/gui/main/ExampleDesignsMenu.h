#pragma once

#include <filesystem>

#include <QObject>
#include <QPointer>

#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/ExampleDesigns.h"
#include "QtRocket/gui/main/MainWindow.h"

class QMenu;

namespace QtRocket::Gui
{

/// File > Open example (ExampleDesignFileAction): the menu it is the child of lists the example
/// designs of a directory (ExampleDesigns: the data directory's examples/), an item per design
/// named after it, in OpenRocket's order with its separators (ExampleDesigns::menuEntries()),
/// made once, when the menu is (as Java's updateMenu() runs in the constructor). Choosing an item
/// opens the example (DocumentFileOperations::openExample(), BasicFrame.open(URL, parent)) from
/// the window of the menu (none for the menus of no window): without a file, saved, not a recent
/// file, and the rocket's configuration dialog is asked for. The menu is enabled, also while it
/// is empty (a JMenu is).
///
/// The separators are the menu's own: a QMenu does not show a separator that would stand at its
/// start or end or next to another one (QMenu::separatorsCollapsible()), where Swing shows each.
class ExampleDesignsMenu final : public QObject
{
    Q_OBJECT

public:
    /// Fills @p menu (the parent, not null; its items are replaced) with the example designs of
    /// @p directory, opened by @p operations from @p window (null for the menus of no window).
    ExampleDesignsMenu(const std::filesystem::path& directory, DocumentFileOperations& operations,
                       MainWindow* window, QMenu* menu);

    /// The menu filled.
    [[nodiscard]] QMenu* menu() const noexcept { return m_menu; }

    /// The designs the menu offers.
    [[nodiscard]] const ExampleDesigns& designs() const noexcept { return m_designs; }

    /// What choosing the item of @p design does: opens it as an example. The window of the
    /// design, or null when it could not be opened (the error was shown).
    MainWindow* open(const ExampleDesigns::Design& design);

private:
    /// updateMenu(): the items in the order of the menu.
    void updateMenu();

    QMenu*                           m_menu;
    ExampleDesigns                   m_designs;
    QPointer<DocumentFileOperations> m_operations;
    QPointer<MainWindow>             m_window;
};

}  // namespace QtRocket::Gui
