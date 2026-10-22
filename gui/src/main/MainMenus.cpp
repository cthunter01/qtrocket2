#include "QtRocket/gui/main/MainMenus.h"

#include <QAction>
#include <QChar>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QString>
#include <Qt>

#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"

namespace QtRocket::Gui
{

namespace
{

/// Adds an action to @p menu named @p objectName with the text @p text (escaped and with its
/// mnemonic marked already): a window shortcut, no menu role (macOS would otherwise move an item
/// by its text), disabled until enabled.
QAction* addAction(QMenu& menu, const QString& objectName, const QString& text)
{
    auto* action = makeChild<QAction>(&menu, text);
    action->setObjectName(objectName);
    action->setShortcutContext(Qt::WindowShortcut);
    action->setMenuRole(QAction::NoRole);
    action->setEnabled(false);
    menu.addAction(action);
    return action;
}

/// Adds the menu @p objectName with the text @p text (as addAction()) and the accessible
/// description @p description to @p menuBar.
QMenu* addMenu(QMenuBar& menuBar, const QString& objectName, const QString& text,
               const QString& description)
{
    auto* menu = makeChild<QMenu>(&menuBar, text);
    menu->setObjectName(objectName);
    menu->setAccessibleDescription(description);
    menuBar.addMenu(menu);
    return menu;
}

/// Adds the submenu @p objectName with the text @p text (as addAction()) to @p menu.
QMenu* addSubmenu(QMenu& menu, const QString& objectName, const QString& text)
{
    auto* submenu = makeChild<QMenu>(&menu, text);
    submenu->setObjectName(objectName);
    submenu->menuAction()->setMenuRole(QAction::NoRole);
    menu.addMenu(submenu);
    return submenu;
}

/// Ctrl (Cmd on macOS) with @p key: BasicFrame's Menu+key where no standard key exists.
QKeySequence menuShortcut(Qt::Key key)
{
    return {Qt::CTRL | key};
}

}  // namespace

MainMenus::MainMenus(UndoRedoActions& undoRedo, QMenuBar* menuBar)
  : QObject(menuBar), m_menuBar(menuBar)
{
    addFileMenu();
    addCreateAndOpenItems();
    m_fileMenu->addSeparator();
    addSaveAndCloseItems();
    m_fileMenu->addSeparator();
    addQuitItem();
    addEditMenu(undoRedo);
    addHelpMenu();
}

MainMenus::MainMenus(QMenuBar* menuBar) : QObject(menuBar), m_menuBar(menuBar)
{
    // DummyFrameMenuOSX.createMenu().
    addFileMenu();
    addCreateAndOpenItems();
    m_fileMenu->addSeparator();
    addQuitItem();
    addHelpMenu();
}

void MainMenus::addFileMenu()
{
    //: main.menu.file
    m_fileMenu = addMenu(*m_menuBar, QStringLiteral("fileMenu"),
                         MenuText::withMnemonic(tr("File"), QChar(u'F')),
                         //: main.menu.file.desc
                         tr("File-handling related tasks"));
}

void MainMenus::addCreateAndOpenItems()
{
    //: main.menu.file.new
    m_new = addAction(*m_fileMenu, QStringLiteral("newAction"),
                      MenuText::withMnemonic(tr("New"), QChar(u'N')));
    m_new->setIcon(Icons::icon(Icons::Id::FILE_NEW));
    m_new->setShortcuts(QKeySequence::New);
    //: main.menu.file.new.desc
    m_new->setStatusTip(tr("Create a new rocket design"));
    m_new->setEnabled(true);

    //: main.menu.file.open
    m_open = addAction(*m_fileMenu, QStringLiteral("openAction"),
                       MenuText::withMnemonic(tr("Open\u2026"), QChar(u'O')));
    m_open->setIcon(Icons::icon(Icons::Id::FILE_OPEN));
    m_open->setShortcuts(QKeySequence::Open);
    //: main.menu.file.open.desc
    m_open->setStatusTip(tr("Open a rocket design"));

    //: main.menu.file.openRecent
    m_openRecent = addSubmenu(*m_fileMenu, QStringLiteral("openRecentMenu"),
                              MenuText::escaped(tr("Open recent")));
    m_openRecent->setIcon(Icons::icon(Icons::Id::FILE_OPEN_RECENT));
    //: main.menu.file.openRecent.desc
    m_openRecent->menuAction()->setStatusTip(tr("Open a recent rocket design"));
    m_openRecent->menuAction()->setEnabled(false);

    //: main.menu.file.openExample
    m_openExample = addSubmenu(*m_fileMenu, QStringLiteral("openExampleMenu"),
                               MenuText::escaped(tr("Open example")));
    m_openExample->setIcon(Icons::icon(Icons::Id::FILE_OPEN_EXAMPLE));
    //: main.menu.file.openExample.desc
    m_openExample->menuAction()->setStatusTip(tr("Open an example rocket design"));
    m_openExample->menuAction()->setEnabled(false);
}

void MainMenus::addSaveAndCloseItems()
{
    //: main.menu.file.save
    m_save = addAction(*m_fileMenu, QStringLiteral("saveAction"),
                       MenuText::withMnemonic(tr("Save"), QChar(u'S')));
    m_save->setIcon(Icons::icon(Icons::Id::FILE_SAVE));
    m_save->setShortcuts(QKeySequence::Save);
    //: main.menu.file.save.desc
    m_save->setStatusTip(tr("Save the current rocket design"));

    //: main.menu.file.saveAs
    m_saveAs = addAction(*m_fileMenu, QStringLiteral("saveAsAction"),
                         MenuText::withMnemonic(tr("Save as\u2026"), QChar(u'A')));
    m_saveAs->setIcon(Icons::icon(Icons::Id::FILE_SAVE_AS));
    m_saveAs->setShortcuts(QKeySequence::SaveAs);
    //: main.menu.file.saveAs.desc
    m_saveAs->setStatusTip(tr("Save the current rocket design to a new file"));

    m_fileMenu->addSeparator();

    //: main.menu.file.properties
    m_properties = addAction(*m_fileMenu, QStringLiteral("propertiesAction"),
                             MenuText::withMnemonic(tr("Properties"), QChar(u'I')));
    m_properties->setIcon(Icons::icon(Icons::Id::CONFIGURE));
    m_properties->setShortcut(menuShortcut(Qt::Key_I));
    //: main.menu.file.properties.desc
    m_properties->setStatusTip(tr("View the file properties"));

    //: main.menu.file.close
    m_close = addAction(*m_fileMenu, QStringLiteral("closeAction"),
                        MenuText::withMnemonic(tr("Close design"), QChar(u'C')));
    m_close->setIcon(Icons::icon(Icons::Id::FILE_CLOSE));
    m_close->setShortcuts(QKeySequence::Close);
    //: main.menu.file.close.desc
    m_close->setStatusTip(tr("Close the current rocket design"));
    m_close->setEnabled(true);
}

void MainMenus::addQuitItem()
{
    //: main.menu.file.quit
    m_quit = addAction(*m_fileMenu, QStringLiteral("quitAction"),
                       MenuText::withMnemonic(tr("Quit"), QChar(u'Q')));
    m_quit->setIcon(Icons::icon(Icons::Id::FILE_QUIT));
    m_quit->setShortcut(menuShortcut(Qt::Key_Q));
    //: main.menu.file.quit.desc
    m_quit->setStatusTip(tr("Quit the program"));
    m_quit->setMenuRole(QAction::QuitRole);
    m_quit->setEnabled(true);
}

void MainMenus::addEditMenu(UndoRedoActions& undoRedo)
{
    //: main.menu.edit
    m_editMenu = addMenu(*m_menuBar, QStringLiteral("editMenu"),
                         MenuText::withMnemonic(tr("Edit"), QChar(u'E')),
                         //: main.menu.edit.desc
                         tr("Rocket editing"));
    // Swing shows the tooltips of the component actions' menu items.
    m_editMenu->setToolTipsVisible(true);
    m_undo = undoRedo.undoAction();
    m_redo = undoRedo.redoAction();
    m_editMenu->addAction(m_undo);
    m_editMenu->addAction(m_redo);
    m_editMenu->addSeparator();
    addComponentItems();
    m_editMenu->addSeparator();
    addSelectMenu();
    m_editMenu->addSeparator();
    addVisibilityMenu();
    m_editMenu->addSeparator();

    //: main.menu.edit.preferences
    m_preferences = addAction(*m_editMenu, QStringLiteral("preferencesAction"),
                              MenuText::escaped(tr("Preferences")));
    m_preferences->setIcon(Icons::icon(Icons::Id::PREFERENCES));
    //: main.menu.edit.preferences.desc
    m_preferences->setStatusTip(tr("Setup the application preferences"));
    m_preferences->setMenuRole(QAction::PreferencesRole);
}

void MainMenus::addComponentItems()
{
    //: RocketActions.EditAct.Edit
    m_edit = addAction(*m_editMenu, QStringLiteral("editAction"),
                       MenuText::withMnemonic(tr("Edit"), QChar(u'E')));
    m_edit->setIcon(Icons::icon(Icons::Id::EDIT_EDIT));
    m_edit->setShortcut(menuShortcut(Qt::Key_E));
    //: RocketActions.EditAct.ttip.Edit
    m_edit->setToolTip(tr("Edit the selected component."));

    //: RocketActions.CutAction.Cut
    m_cut = addAction(*m_editMenu, QStringLiteral("cutAction"),
                      MenuText::withMnemonic(tr("Cut"), QChar(u'T')));
    m_cut->setIcon(Icons::icon(Icons::Id::EDIT_CUT));
    m_cut->setShortcuts(QKeySequence::Cut);
    m_cut->setToolTip(
        //: RocketActions.CutAction.ttip.Cut
        tr("Cut these components or simulations to the clipboard and delete from this design"));

    //: RocketActions.CopyAct.Copy
    m_copy = addAction(*m_editMenu, QStringLiteral("copyAction"),
                       MenuText::withMnemonic(tr("Copy"), QChar(u'C')));
    m_copy->setIcon(Icons::icon(Icons::Id::EDIT_COPY));
    m_copy->setShortcuts(QKeySequence::Copy);
    //: RocketActions.CopyAct.ttip.Copy
    m_copy->setToolTip(tr("Copy these components (and subcomponents) to the clipboard."));

    //: RocketActions.PasteAct.Paste
    m_paste = addAction(*m_editMenu, QStringLiteral("pasteAction"),
                        MenuText::withMnemonic(tr("Paste"), QChar(u'V')));
    m_paste->setIcon(Icons::icon(Icons::Id::EDIT_PASTE));
    m_paste->setShortcuts(QKeySequence::Paste);
    m_paste->setToolTip(
        //: RocketActions.PasteAct.ttip.Paste
        tr("Paste the components or simulations on the clipboard to the design."));

    //: RocketActions.DuplicateAct.Duplicate
    m_duplicate = addAction(*m_editMenu, QStringLiteral("duplicateAction"),
                            MenuText::withMnemonic(tr("Duplicate"), QChar(u'D')));
    m_duplicate->setIcon(Icons::icon(Icons::Id::EDIT_DUPLICATE));
    m_duplicate->setShortcut(menuShortcut(Qt::Key_D));
    //: RocketActions.DuplicateAct.ttip.Duplicate
    m_duplicate->setToolTip(tr("Duplicate this component (and subcomponents)."));

    // The mnemonic of Delete is the Delete key, which no text holds.
    //: RocketActions.DelAct.Delete
    m_delete =
        addAction(*m_editMenu, QStringLiteral("deleteAction"), MenuText::escaped(tr("Delete")));
    m_delete->setIcon(Icons::icon(Icons::Id::EDIT_DELETE));
    // RocketActions' VK_DELETE alone, not QKeySequence::Delete: the GNOME and X11 key schemes add
    // Ctrl+D to that, which Duplicate has, and an ambiguous key triggers neither.
    m_delete->setShortcut(QKeySequence(Qt::Key_Delete));
    //: RocketActions.DelAct.ttip.Delete
    m_delete->setToolTip(tr("Delete the selected components or simulations."));
}

void MainMenus::addSelectMenu()
{
    //: RocketActions.Select
    m_selectMenu =
        addSubmenu(*m_editMenu, QStringLiteral("selectMenu"), MenuText::escaped(tr("Select")));
    m_selectMenu->setToolTipsVisible(true);

    // RocketActions.Select.SelectSameColorAct ("Components of same color") comes with Milestone 5.

    //: RocketActions.Select.DeselectAllAct
    m_deselectAll = addAction(*m_selectMenu, QStringLiteral("deselectAllAction"),
                              MenuText::escaped(tr("Deselect all")));
    //: RocketActions.Select.DeselectAllAct.ttip
    m_deselectAll->setToolTip(tr("Deselect all currently selected components."));
}

void MainMenus::addVisibilityMenu()
{
    //: RocketActions.Visibility
    m_visibilityMenu = addSubmenu(*m_editMenu, QStringLiteral("visibilityMenu"),
                                  MenuText::escaped(tr("Visibility")));
    m_visibilityMenu->setToolTipsVisible(true);

    //: RocketActions.VisibilityAct.HideSelected
    m_toggleVisibility = addAction(*m_visibilityMenu, QStringLiteral("toggleVisibilityAction"),
                                   MenuText::escaped(tr("Hide selected")));
    m_toggleVisibility->setIcon(Icons::icon(Icons::Id::COMPONENT_HIDDEN));
    //: RocketActions.VisibilityAct.ttip.HideSelected
    m_toggleVisibility->setToolTip(tr("Hide selected component(s)."));

    //: RocketActions.VisibilityAct.ShowAll
    m_showAll = addAction(*m_visibilityMenu, QStringLiteral("showAllComponentsAction"),
                          MenuText::escaped(tr("Show all")));
    m_showAll->setIcon(Icons::icon(Icons::Id::COMPONENT_SHOWING));
    //: RocketActions.VisibilityAct.ttip.ShowAll
    m_showAll->setToolTip(tr("Show all components."));
}

void MainMenus::addHelpMenu()
{
    //: main.menu.help
    m_helpMenu = addMenu(*m_menuBar, QStringLiteral("helpMenu"),
                         MenuText::withMnemonic(tr("Help"), QChar(u'H')),
                         //: main.menu.help.desc
                         tr("Information about QtRocket"));
    // None of OpenRocket's Help items is in Milestone 2, and an empty menu is not shown.
    m_helpMenu->menuAction()->setVisible(!m_helpMenu->isEmpty());
}

}  // namespace QtRocket::Gui
