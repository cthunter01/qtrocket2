#pragma once

#include <QObject>

class QAction;
class QMenu;
class QMenuBar;

namespace QtRocket::Gui
{

class UndoRedoActions;

/// The menu bar of a design window (BasicFrame.createMenu()) or of the application while no
/// window is open (macOS: DummyFrameMenuOSX), with OpenRocket's items of Milestone 2 in its
/// order. An object of this class fills the menu bar it is a child of and gives access to every
/// menu and item; it does nothing itself. What an item does is connected by whoever owns that
/// behaviour, which also enables the item: New, Close design and Quit by the WindowManager (now);
/// Open, Open recent, Open example, Save, Save as, Properties and Preferences by the file
/// operations and the dialogs (run 3); Edit, Cut, Copy, Paste, Duplicate, Delete and the items
/// of Select and Visibility by RocketActions (run 2), which also sets their enabled states (and
/// the toggling text and icon of Hide/Show selected). Until then those items are disabled; Undo
/// and Redo are UndoRedoActions'.
///
/// A design window's menus (the same in every window), in OpenRocket's order:
/// - File: New | Open... | Open recent > | Open example > | --- | Save | Save as... | --- |
///   Properties | Close design | --- | Quit
/// - Edit: Undo | Redo | --- | Edit | Cut | Copy | Paste | Duplicate | Delete | --- |
///   Select > (Components of same color, Deselect all) | --- |
///   Visibility > (Hide selected, Show all) | --- | Preferences
/// - Help: none of its items is in Milestone 2 (the About and License dialogs come in Milestone
///   5), so it is hidden while it is empty.
/// The application's menus without a window: File: New | Open... | Open recent > | Open example
/// > | --- | Quit, and Help.
///
/// Every item has OpenRocket's text (its mnemonic marked: MenuText), icon, keys and description
/// (the accessible description of the Java item, here the status tip), and OpenRocket's tooltip
/// where its action has one (the Edit menu shows them, as Swing does); a stable object name
/// (newAction, openRecentMenu, ...) names it for the code that connects it and for the tests.
/// The keys are window shortcuts: the platform's standard keys where Qt has one (New, Open,
/// Save, Save as, Close, Undo, Redo, Cut, Copy, Paste, Delete) and Ctrl (on macOS Cmd) with Q
/// (Quit), E (Edit), D (Duplicate) and I (Properties), which no platform binds for every
/// desktop. On macOS, Preferences and Quit go to the application menu (their menu roles); every
/// other item keeps its place.
///
/// Deviations: the items of later milestones are left out where OpenRocket has them (File:
/// Import > (RockSim M8), Export as > (RockSim, SVG M8), Save decal images... (M5), Print design
/// info... (M8), Export simulations table as CSV file (M4); Edit: Scale (M5); Tools (M7); Help:
/// License, About (M5), Bug report (M9)), and OpenRocket's items that QtRocket does not have
/// (RASAero, Wavefront OBJ, Photo Studio, Guided tours, online Documentation, Debug log, Check for
/// updates, the debug menu and the banana); a separator that would then stand between nothing is
/// left out too. Standard keys follow the platform where BasicFrame binds the same keys on every
/// platform (Close is Ctrl+W or Ctrl+F4, Redo see UndoRedoActions); an item whose mnemonic letter
/// is not in its text (Paste: V, Delete: the Delete key) has no mnemonic.
class MainMenus final : public QObject
{
    Q_OBJECT

public:
    /// Fills @p menuBar (the parent) with the menus of a design window; @p undoRedo is the
    /// window's, whose actions go into the Edit menu.
    MainMenus(UndoRedoActions& undoRedo, QMenuBar* menuBar);

    /// Fills @p menuBar (the parent) with the menus of the application without a window: File
    /// with New, Open..., Open recent, Open example and Quit, and Help (DummyFrameMenuOSX). The
    /// design window's other items are null.
    explicit MainMenus(QMenuBar* menuBar);

    /// Whether these are a design window's menus (not the application's without a window).
    [[nodiscard]] bool isDesignWindowMenus() const noexcept { return m_editMenu != nullptr; }

    /// The menu bar the menus are in (the parent).
    [[nodiscard]] QMenuBar* menuBar() const noexcept { return m_menuBar; }

    // ----------------------------------------------------------------------------- File

    [[nodiscard]] QMenu*   fileMenu() const noexcept { return m_fileMenu; }
    [[nodiscard]] QAction* newAction() const noexcept { return m_new; }
    [[nodiscard]] QAction* openAction() const noexcept { return m_open; }
    /// The submenu of the recent files (MRUDesignFileAction, run 3), empty and disabled now.
    [[nodiscard]] QMenu* openRecentMenu() const noexcept { return m_openRecent; }
    /// The submenu of the example designs (ExampleDesignFileAction, run 3), empty and disabled
    /// now.
    [[nodiscard]] QMenu*   openExampleMenu() const noexcept { return m_openExample; }
    [[nodiscard]] QAction* saveAction() const noexcept { return m_save; }
    [[nodiscard]] QAction* saveAsAction() const noexcept { return m_saveAs; }
    /// File > Properties: the rocket's configuration dialog.
    [[nodiscard]] QAction* propertiesAction() const noexcept { return m_properties; }
    [[nodiscard]] QAction* closeAction() const noexcept { return m_close; }
    [[nodiscard]] QAction* quitAction() const noexcept { return m_quit; }

    // ----------------------------------------------------------------------------- Edit

    [[nodiscard]] QMenu*   editMenu() const noexcept { return m_editMenu; }
    [[nodiscard]] QAction* undoAction() const noexcept { return m_undo; }
    [[nodiscard]] QAction* redoAction() const noexcept { return m_redo; }
    /// Edit > Edit: the configuration dialog of the selected components.
    [[nodiscard]] QAction* editAction() const noexcept { return m_edit; }
    [[nodiscard]] QAction* cutAction() const noexcept { return m_cut; }
    [[nodiscard]] QAction* copyAction() const noexcept { return m_copy; }
    [[nodiscard]] QAction* pasteAction() const noexcept { return m_paste; }
    [[nodiscard]] QAction* duplicateAction() const noexcept { return m_duplicate; }
    [[nodiscard]] QAction* deleteAction() const noexcept { return m_delete; }
    [[nodiscard]] QMenu*   selectMenu() const noexcept { return m_selectMenu; }
    [[nodiscard]] QAction* selectSameColorAction() const noexcept { return m_selectSameColor; }
    [[nodiscard]] QAction* deselectAllAction() const noexcept { return m_deselectAll; }
    [[nodiscard]] QMenu*   visibilityMenu() const noexcept { return m_visibilityMenu; }
    /// Hide selected / Show selected / Hide all / Show all (RocketActions'
    /// ToggleVisibilityAction sets the text as the selection asks).
    [[nodiscard]] QAction* toggleVisibilityAction() const noexcept { return m_toggleVisibility; }
    [[nodiscard]] QAction* showAllComponentsAction() const noexcept { return m_showAll; }
    [[nodiscard]] QAction* preferencesAction() const noexcept { return m_preferences; }

    // ----------------------------------------------------------------------------- Help

    /// The Help menu: empty and hidden in Milestone 2.
    [[nodiscard]] QMenu* helpMenu() const noexcept { return m_helpMenu; }

private:
    /// The File menu, empty.
    void addFileMenu();
    /// File's first items (BasicFrame.addFileCreateAndOpenMenuItems()).
    void addCreateAndOpenItems();
    /// File's Quit.
    void addQuitItem();
    /// The rest of a design window's File menu, after the create and open items.
    void addSaveAndCloseItems();
    /// The Edit menu.
    void addEditMenu(UndoRedoActions& undoRedo);
    /// The Edit menu's items of the component actions (RocketActions').
    void addComponentItems();
    /// Edit's Select submenu.
    void addSelectMenu();
    /// Edit's Visibility submenu.
    void addVisibilityMenu();
    /// The Help menu.
    void addHelpMenu();

    QMenuBar* m_menuBar;
    QMenu*    m_fileMenu{nullptr};
    QAction*  m_new{nullptr};
    QAction*  m_open{nullptr};
    QMenu*    m_openRecent{nullptr};
    QMenu*    m_openExample{nullptr};
    QAction*  m_save{nullptr};
    QAction*  m_saveAs{nullptr};
    QAction*  m_properties{nullptr};
    QAction*  m_close{nullptr};
    QAction*  m_quit{nullptr};
    QMenu*    m_editMenu{nullptr};
    QAction*  m_undo{nullptr};
    QAction*  m_redo{nullptr};
    QAction*  m_edit{nullptr};
    QAction*  m_cut{nullptr};
    QAction*  m_copy{nullptr};
    QAction*  m_paste{nullptr};
    QAction*  m_duplicate{nullptr};
    QAction*  m_delete{nullptr};
    QMenu*    m_selectMenu{nullptr};
    QAction*  m_selectSameColor{nullptr};
    QAction*  m_deselectAll{nullptr};
    QMenu*    m_visibilityMenu{nullptr};
    QAction*  m_toggleVisibility{nullptr};
    QAction*  m_showAll{nullptr};
    QAction*  m_preferences{nullptr};
    QMenu*    m_helpMenu{nullptr};
};

}  // namespace QtRocket::Gui
