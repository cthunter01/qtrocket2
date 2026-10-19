#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include <QApplication>

class QEvent;
class QObject;

namespace QtRocket::Gui
{

/// QtRocket's QApplication (OpenRocket's start-up in OpenRocket.main(), SwingStartup and
/// OSXSetup, as far as they belong to the application object rather than to the start-up
/// sequence, which is ApplicationSession's):
/// - the names Qt is told (application and organisation "QtRocket", the version: BuildInfo),
///   which QSettings and QCommandLineParser use, and the window icon of every window and dialog
///   (Icons::windowIcon(): OpenRocket's window icons, the Dock's icon on macOS);
/// - the last window closed does not end the application: the WindowManager decides that
///   (exit on Linux and Windows, keep running on macOS), so quitOnLastWindowClosed is off;
/// - the backstop for exceptions (SwingExceptionHandler as the default uncaught exception
///   handler): notify() catches what an event handler throws and reports it to the error hook
///   (Guard), so that nothing leaves through Qt's event loop, which cannot carry an exception.
///   The connections into the core are guarded (guarded()) and report before it; this catches
///   what they miss. Not on macOS: there an exception thrown in an event handler cannot unwind
///   through Qt's own frames, and the program terminates before notify() sees it (CI, Qt 6.8.3),
///   so only the guarded connections protect;
/// - the platform's requests: a QFileOpenEvent (macOS opens a design file with the
///   application: OSXSetup's open-file handler) goes to the open-files hook, or waits for one
///   when none is set (pendingFiles()); a QEvent::Quit (the macOS Dock's or the session's Quit,
///   QCoreApplication::quit(): OSXSetup's quit handler) asks the quit hook, which closes the
///   windows and tells whether the application may quit, instead of Qt's closing of every
///   window.
///
/// The application and the GUI tests make one each (tests/gui/GuiTestMain.cpp), so that the tests
/// run under the same backstop. The hooks belong to whoever set them: ApplicationSession sets
/// both and puts the previous ones back when it ends.
///
/// Deviations: OpenRocket opens only the first file of an open-file event (OSXSetup); Qt sends
/// one event per file, and each is opened. OpenRocket's JRE work-arounds (the ignored JRE bugs,
/// the memory reserve for an OutOfMemoryError) have no counterpart.
class Application final : public QApplication
{
    Q_OBJECT

public:
    /// Asked for a QEvent::Quit: closes the windows and returns whether the application may
    /// quit (BasicFrame.quitAction(); false when a window stayed open).
    using QuitHook = std::function<bool()>;
    /// Opens design files the platform asked to open (one QFileOpenEvent each).
    using OpenFilesHook = std::function<void(const std::vector<std::filesystem::path>& files)>;

    /// @p argc and @p argv as main() got them; both must outlive the application (QApplication).
    Application(int& argc, char** argv);
    ~Application() override;
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    /// The running Application, or null when there is no QCoreApplication or it is not an
    /// Application.
    [[nodiscard]] static Application* current() noexcept;

    /// Delivers @p event to @p receiver (QApplication::notify()); what that throws is reported
    /// to the error hook (Guard), naming the event and the receiver's class, and the event
    /// counts as not handled (false).
    bool notify(QObject* receiver, QEvent* event) override;

    /// Sets the hook a QEvent::Quit asks and returns the one it replaces (empty for none). With
    /// no hook, Qt quits as it does by default (it closes every window, then exits).
    QuitHook setQuitHook(QuitHook hook);

    /// Sets the hook the files of QFileOpenEvents go to and returns the one it replaces (empty
    /// for none). The files that came while there was no hook are handed to @p hook at once.
    OpenFilesHook setOpenFilesHook(OpenFilesHook hook);

    /// The files of QFileOpenEvents that came while there was no open-files hook, in order.
    [[nodiscard]] const std::vector<std::filesystem::path>& pendingFiles() const noexcept
    {
        return m_pendingFiles;
    }

protected:
    /// The application's events: QFileOpenEvent and QEvent::Quit as the class comment says,
    /// the others as QApplication's.
    bool event(QEvent* event) override;

private:
    /// A QEvent::Quit while a quit hook is set: accepted when the hook lets the application
    /// quit.
    void quitRequested(QEvent& event);
    /// A QFileOpenEvent: its file (a local file; nothing for another URL) goes to the hook, or
    /// waits for one.
    void fileOpenRequested(const QEvent& event);
    /// Hands the waiting files to the hook.
    void deliverPendingFiles();

    QuitHook                           m_quitHook;
    OpenFilesHook                      m_openFilesHook;
    std::vector<std::filesystem::path> m_pendingFiles;
};

}  // namespace QtRocket::Gui
