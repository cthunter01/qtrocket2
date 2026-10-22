#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>

#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/RecentFiles.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/Guard.h"

namespace QtRocket::Gui
{

class ApplicationContext;
class MainMenus;
class MainWindow;

/// The running application (OpenRocket's SwingStartup.runInEDT() and what it leaves running):
/// made by main() once the command line is read and the services are ready
/// (ApplicationContext), and alive until the event loop ends. The tests make one the same way,
/// with test services, and do not run the event loop. Its constructor starts the application,
/// in SwingStartup's order:
/// 1. the error hook (SwingExceptionHandler): every failure the GUI reports (Guard: a guarded
///    slot, the application's notify() backstop, an error condition) shows the internal error
///    dialog (UserPrompts::showInternalError()) on the GUI thread (a report from another thread
///    is handed to it, queued); Quit there ends the application at once with
///    kInternalErrorExitCode (exitRequested()), without saving anything;
/// 2. the windows (WindowManager over the context's services, with the platform's last-window
///    behaviour); their exit (the last window closed, File > Quit) is exitRequested(0); their
///    file operations (DocumentFileOperations: File > Open, Save, Save as and Properties of every
///    window and of the menus of no window, and the Save of the "Design not saved" question); the
///    recent files (RecentFiles over the context's store, MRUDesignFile: every file opened and
///    every file saved as goes to its top) and, in the menus of every window and of no window,
///    File > Open recent (RecentFilesMenu) and File > Open example (ExampleDesignsMenu over the
///    data directory's examples);
/// 3. the platform's requests (OSXSetup): a quit (QEvent::Quit: the macOS Dock and application
///    menu) closes the windows as File > Quit does; a file to open (QFileOpenEvent) goes to
///    openFiles(); on macOS, where the application runs without a window, it opens one when it
///    is reopened (reopenIfNoWindow());
/// 4. the first window: the files of the command line (openFiles()), or when none was opened
///    and no window is open (one may have opened while a question of the files ran: macOS),
///    reopen() (the most recent design or a New design);
/// 5. the problems of the start the user is to know of: the motor database's (a message);
/// 6. for the tests of the program: with Options::quitAfterStart, a quit as File > Quit does
///    as soon as the event loop runs.
///
/// Deviations: a report from another thread does not wait for the dialog (SwingExceptionHandler
/// waits for an uncaught exception's, invokeAndWait()), so that a worker the GUI thread waits for
/// cannot block it. OpenRocket's splash screen, welcome dialog, update checks, plug-in migration
/// and look-and-feel set-up have no counterpart (the splash: Milestone 9). On macOS, Qt reports the
/// Dock's reopen as the application becoming active, which it also becomes by other means
/// (Cmd-Tab): whenever the application becomes active without a window, a New design opens,
/// where OpenRocket opens one only for the Dock's reopen.
class ApplicationSession final : public QObject
{
    Q_OBJECT

public:
    /// The exit code when the user quits from the internal error dialog.
    static constexpr int kInternalErrorExitCode = 1;

    /// How the application starts.
    struct Options
    {
        /// The design files of the command line, to open at the start.
        std::vector<std::filesystem::path> files;
        /// What happens when the last window is closed.
        WindowManager::LastWindowBehaviour lastWindowBehaviour =
            WindowManager::platformLastWindowBehaviour();
        /// Quit as File > Quit does as soon as the event loop runs (the program's test hook:
        /// the environment variable QTROCKET_TEST_QUIT_AFTER_START, gui/main.cpp).
        bool quitAfterStart = false;
    };

    /// Starts the application (see the class comment) on @p application with the services of
    /// @p context (not null), which it takes over. Not before the QApplication's event loop
    /// may run: it shows windows.
    ApplicationSession(Application& application, std::unique_ptr<ApplicationContext> context,
                       const Options& options, QObject* parent = nullptr);
    /// Puts the hooks it replaced back (the error hook, the application's quit and open-files
    /// hooks), then destroys the windows and their documents, then the services.
    ~ApplicationSession() override;
    ApplicationSession(const ApplicationSession&)            = delete;
    ApplicationSession& operator=(const ApplicationSession&) = delete;
    ApplicationSession(ApplicationSession&&)                 = delete;
    ApplicationSession& operator=(ApplicationSession&&)      = delete;

    [[nodiscard]] ApplicationContext&     context() const noexcept { return *m_context; }
    [[nodiscard]] WindowManager&          windows() const noexcept { return *m_windows; }
    [[nodiscard]] DocumentFileOperations& fileOperations() const noexcept
    {
        return *m_fileOperations;
    }
    /// The recent files (the File > Open recent menus').
    [[nodiscard]] RecentFiles& recentFiles() const noexcept { return *m_recentFiles; }

    /// SwingStartup.handleCommandLine() and OSXSetup's open-file handler: opens each of
    /// @p files in a window of its own (DocumentFileOperations::openFiles(), BasicFrame.open(file,
    /// origin): @p origin, the window the request came from or null, is replaced when it holds a
    /// New design still saved; a file that cannot be opened shows its error). Whether a window
    /// was opened (or a window of one of the files activated).
    bool openFiles(const std::vector<std::filesystem::path>& files, MainWindow* origin);

    /// BasicFrame.reopen(): opens the window the application starts with (and, on macOS, the
    /// one it opens when it is reopened without a window). When the preference "Open last design
    /// file on startup" (AutoOpenLastDesign) is set and the recent files hold one, the most
    /// recent file is opened (open(), as from no window), and goes to the top of the recent
    /// files again; when it cannot be opened (its error was shown), it is taken out of the
    /// recent files and a New design opens. Otherwise a New design opens. The window opened, or
    /// null when a reopen is under way already (BasicFrame's reopenInProgress: a question of
    /// the open runs an event loop, in which macOS may ask for a reopen again).
    MainWindow* reopen();

    /// OSXSetup's reopen handler: reopen() when no window is open.
    void reopenIfNoWindow();

Q_SIGNALS:
    /// The event loop is to end with @p returnCode: 0 when the windows were closed (the last
    /// one on Linux and Windows, or all of them by Quit), kInternalErrorExitCode when the user
    /// quit from the internal error dialog. main() connects it to QCoreApplication::exit(),
    /// queued, so that the slot that asked returns first.
    void exitRequested(int returnCode);

private:
    /// The error hook: the internal error dialog, and its Quit.
    void showInternalError(const Guard::Failure& failure);
    /// The motor database's problems, as a message over @p window.
    void showStartupProblems(MainWindow& window);
    /// Fills File > Open recent and File > Open example of @p menus, the menus of @p window
    /// (null for the menus of no window).
    void installFileMenus(MainMenus& menus, MainWindow* window);
    /// Puts back each hook this session replaced.
    void restoreHooks();

    Application*                        m_application;
    std::unique_ptr<ApplicationContext> m_context;
    /// The hooks this session replaced, each once it did.
    std::optional<Guard::Hook>                m_previousErrorHook;
    std::optional<Application::QuitHook>      m_previousQuitHook;
    std::optional<Application::OpenFilesHook> m_previousOpenFilesHook;
    /// Declared before the windows: their Open recent menus go first.
    std::unique_ptr<RecentFiles> m_recentFiles;
    /// Declared after the services: the windows go before them.
    std::unique_ptr<WindowManager> m_windows;
    /// Declared last: the file operations go before the windows, whose save hook they are.
    std::unique_ptr<DocumentFileOperations> m_fileOperations;
    /// Whether reopen() runs (BasicFrame.reopenInProgress).
    bool m_reopening{false};
};

}  // namespace QtRocket::Gui
