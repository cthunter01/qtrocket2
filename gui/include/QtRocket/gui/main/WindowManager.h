#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include <QObject>

#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/util/GuiServices.h"

class QMenuBar;

namespace QtRocket
{
class OpenRocketDocument;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class OpenRocketClipboard;

/// The open design windows (BasicFrame's static part: its list of frames, newAction(),
/// closeAction(), quitAction(), closeIfReplaceable(), findFrame()). It owns each window and its
/// document, the document outside its window: a closed window is destroyed (later, from the
/// event loop) before its document. It connects every window's New, Close design and Quit, and
/// the close button.
///
/// - newDesign() opens a window on a new design (OpenRocketDocumentFactory::createNewRocket():
///   "Rocket" with the stage "Sustainer", saved, no file); addDocument() opens one on a document
///   that was loaded (run 3's file operations). A window opens as BasicFrame's: with its design
///   panel (a DesignPanel: the component tree, its selection and actions, the add buttons; the
///   first stage selected, the tree with the keyboard focus), its size and the remembered
///   geometry (MainWindow), then 30 pixels right of and below the window opened before it
///   (WindowGeometry::cascade()), then shown. Every window's design panel shares the component
///   clipboard (the application's, OpenRocketClipboard::instance(), unless the manager was made
///   with another one).
/// - A window of a new design is "replaceable": a New or an Open from it while its design is
///   still saved closes it, and the new window takes its place (closeIfReplaceable()). As in
///   Java a replaceable window stays so for good: it is not replaced once the design has
///   changed, unless it has been saved since.
/// - close() closes one window: when its design is not saved, it asks first ("Design not
///   saved", UserPrompts::askSaveChanges()): Save saves it through the save hook (the file
///   operations of run 3 set it; without one, a message says that saving is not available, and
///   the window stays), and a save that fails or is cancelled keeps the window; Discard closes
///   it; Cancel keeps it. When the last window is closed, the application exits (exitRequested())
///   on Linux and Windows; on macOS it keeps running with the menu bar of no window (File: New,
///   Open, Open recent, Open example, Quit; Help: DummyFrameMenuOSX), unless it is quitting.
///   The window asks for the configuration dialog to close (MainWindow::
///   requestConfigDialogClose()) before the question and after it closed, as
///   BasicFrame.closeAction() disposes the dialog.
/// - quit() closes the windows from the last opened to the first; a window that stays open (its
///   question cancelled, its save failed) stops the quit, and the windows closed so far stay
///   closed. When all are closed, the application exits.
///
/// The application connects exitRequested() to the end of its event loop. menusCreated() and
/// windowOpened() let the code that owns the other items connect them (MainMenus).
///
/// Deviations: the application's exit is a signal, where BasicFrame calls System.exit(0); the
/// menu bar of no window is made once and kept (Java makes a new dummy frame every time the last
/// window closes) and lives as long as this object; quit() returns whether it succeeded, where
/// OpenRocket's macOS quit handler learns it from the exit not happening.
class WindowManager final : public QObject
{
    Q_OBJECT

public:
    /// What the application does when its last window is closed (and it is not quitting).
    enum class LastWindowBehaviour
    {
        /// Exit (Linux, Windows: BasicFrame's System.exit(0)).
        EXIT,
        /// Keep running, with the menu bar of no window (macOS: DummyFrameMenuOSX).
        KEEP_RUNNING,
    };

    /// Saves the design of the window and tells whether it was saved (BasicFrame.saveAction()).
    using SaveHook = std::function<bool(MainWindow& window)>;

    /// The platform's: KEEP_RUNNING on macOS, EXIT on the others.
    [[nodiscard]] static LastWindowBehaviour platformLastWindowBehaviour() noexcept;

    /// No window yet; the objects of @p services must outlive this object. The windows share the
    /// application's component clipboard (OpenRocketClipboard::instance()).
    explicit WindowManager(const GuiServices&  services,
                           LastWindowBehaviour behaviour = platformLastWindowBehaviour(),
                           QObject*            parent    = nullptr);
    /// As above, the windows sharing @p clipboard, which must outlive this object (a test's own).
    WindowManager(const GuiServices& services, OpenRocketClipboard& clipboard,
                  LastWindowBehaviour behaviour = platformLastWindowBehaviour(),
                  QObject*            parent    = nullptr);
    ~WindowManager() override;
    WindowManager(const WindowManager&)            = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&)                 = delete;
    WindowManager& operator=(WindowManager&&)      = delete;

    /// BasicFrame.newAction(): opens a window on a new design, replaceable, and closes
    /// @p origin (the window the New came from; null for none) when it is replaceable and saved
    /// (closeIfReplaceable()). The new window.
    MainWindow& newDesign(MainWindow* origin = nullptr);

    /// Opens a window on @p document (not null), which it takes over, and closes @p origin when
    /// it is replaceable and saved, as BasicFrame.open() does after a file is loaded. The new
    /// window.
    MainWindow& addDocument(std::unique_ptr<OpenRocketDocument> document,
                            MainWindow*                         origin = nullptr);

    /// BasicFrame.closeAction(): closes @p window, asking first whether to save its design when
    /// it is not saved. Whether it was closed (false also for a window that is not open here or
    /// already being closed).
    bool close(MainWindow& window);

    /// BasicFrame.quitAction(): closes every window, the last opened first. Whether all were
    /// closed (then exitRequested() was emitted); false when one stayed open, or when a quit is
    /// already under way.
    bool quit();

    /// Sets what Save in the "Design not saved" question does; an empty hook (the default)
    /// makes it say that saving is not available.
    void setSaveHook(SaveHook hook);

    /// The open windows, in the order they were opened.
    [[nodiscard]] std::vector<MainWindow*> windows() const;
    /// The number of open windows.
    [[nodiscard]] std::size_t windowCount() const noexcept { return m_entries.size(); }
    /// The window of @p rocket (BasicFrame.findFrame()), or null.
    [[nodiscard]] MainWindow* findWindow(const Rocket& rocket) const noexcept;
    /// Whether @p window was opened by newDesign() and is still to be replaced by a New or an
    /// Open while its design is saved.
    [[nodiscard]] bool isReplaceable(const MainWindow& window) const noexcept;
    /// What happens when the last window is closed.
    [[nodiscard]] LastWindowBehaviour lastWindowBehaviour() const noexcept { return m_behaviour; }
    /// The component clipboard the windows share.
    [[nodiscard]] OpenRocketClipboard& clipboard() const noexcept { return *m_clipboard; }

    /// The menu bar of no window (parentless: macOS shows it while no window is active), made
    /// when the last window is closed with KEEP_RUNNING; null before.
    [[nodiscard]] QMenuBar* noWindowMenuBar() const noexcept { return m_noWindowMenuBar.get(); }
    /// The menus of noWindowMenuBar(), or null.
    [[nodiscard]] MainMenus* noWindowMenus() const noexcept { return m_noWindowMenus; }

Q_SIGNALS:
    /// The menus of a new window, or of no window (@p window null), were made: the other items
    /// can be connected.
    void menusCreated(QtRocket::Gui::MainMenus* menus, QtRocket::Gui::MainWindow* window);
    /// A new window was opened and shown.
    void windowOpened(QtRocket::Gui::MainWindow* window);
    /// The application is to exit: its last window was closed (EXIT), or quit() closed them all.
    void exitRequested();

private:
    /// A window and its document; the window is declared last, so that it is destroyed first.
    struct Entry
    {
        std::unique_ptr<OpenRocketDocument> document;
        std::unique_ptr<MainWindow>         window;
        bool                                replaceable = false;
        bool                                closing     = false;
    };

    /// Makes the window of @p document, places, connects and shows it.
    MainWindow& open(std::unique_ptr<OpenRocketDocument> document, bool replaceable);
    /// Connects New, Close design and Quit of @p menus; @p window is theirs, or null.
    void connectMenus(MainMenus& menus, MainWindow* window);
    /// BasicFrame.closeIfReplaceable(): @p origin is closed and @p replacement takes its place
    /// when @p origin is replaceable and saved.
    void closeIfReplaceable(MainWindow* origin, MainWindow& replacement);
    /// Asks whether to save the design of @p window and saves it; false to keep the window.
    [[nodiscard]] bool confirmClose(MainWindow& window);
    /// The entry of @p window, or null.
    [[nodiscard]] Entry* find(const MainWindow& window) const noexcept;
    /// Takes @p window out of the list, hides it and destroys it with its document later.
    void destroyLater(const MainWindow& window);
    /// After the last window was closed: exits, or shows the menu bar of no window.
    void lastWindowClosed();

    GuiServices          m_services;
    OpenRocketClipboard* m_clipboard;
    LastWindowBehaviour  m_behaviour;
    SaveHook             m_saveHook;
    /// The open windows; each Entry by pointer, so that one stays where it is while a question
    /// (a nested event loop) runs.
    std::vector<std::unique_ptr<Entry>> m_entries;
    std::unique_ptr<QMenuBar>           m_noWindowMenuBar;
    MainMenus*                          m_noWindowMenus{nullptr};
    bool                                m_quitting{false};
};

}  // namespace QtRocket::Gui
