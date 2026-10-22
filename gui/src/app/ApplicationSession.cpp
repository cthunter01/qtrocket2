#include "QtRocket/gui/app/ApplicationSession.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p context, which must not be null.
[[nodiscard]] std::unique_ptr<ApplicationContext> nonNullContext(
    std::unique_ptr<ApplicationContext> context)
{
    QTROCKET_ASSERT(context != nullptr);
    return context;
}

}  // namespace

ApplicationSession::ApplicationSession(Application&                        application,
                                       std::unique_ptr<ApplicationContext> context,
                                       const Options& options, QObject* parent)
  : QObject(parent), m_application(&application), m_context(nonNullContext(std::move(context)))
{
    try
    {
        // 1. SwingExceptionHandler.registerExceptionHandler().
        m_previousErrorHook =
            Guard::setHook([this](const Guard::Failure& failure) { showInternalError(failure); });

        // 2. The windows.
        m_windows =
            std::make_unique<WindowManager>(m_context->services(), options.lastWindowBehaviour);
        connect(m_windows.get(), &WindowManager::exitRequested, this,
                [this] { Q_EMIT exitRequested(0); });
        m_fileOperations =
            std::make_unique<DocumentFileOperations>(*m_windows, m_context->services());

        // 3. OSXSetup's handlers.
        m_previousQuitHook = application.setQuitHook([this] { return m_windows->quit(); });
        m_previousOpenFilesHook =
            application.setOpenFilesHook([this](const std::vector<std::filesystem::path>& files) {
                const std::vector<MainWindow*> open = m_windows->windows();
                static_cast<void>(openFiles(files, open.empty() ? nullptr : open.back()));
            });
        if (options.lastWindowBehaviour == WindowManager::LastWindowBehaviour::KEEP_RUNNING)
        {
            connect(&application, &QGuiApplication::applicationStateChanged, this,
                    guarded(
                        [this](Qt::ApplicationState state) {
                            if (state == Qt::ApplicationActive)
                            {
                                reopenIfNoWindow();
                            }
                        },
                        QStringLiteral("Reopen")));
        }

        // 4. SwingStartup.handleCommandLine(), else BasicFrame.reopen().
        if (!openFiles(options.files, nullptr))
        {
            static_cast<void>(reopen());
        }

        // 5. What the user is to know of the start.
        if (const std::vector<MainWindow*> open = m_windows->windows(); !open.empty())
        {
            showStartupProblems(*open.back());
        }

        // 6. The program's test hook.
        if (options.quitAfterStart)
        {
            QTimer::singleShot(std::chrono::milliseconds(0), this,
                               guarded([this] { static_cast<void>(m_windows->quit()); },
                                       QStringLiteral("Quit after start")));
        }
    }
    catch (...)
    {
        // The destructor does not run for a constructor that throws.
        restoreHooks();
        throw;
    }
}

ApplicationSession::~ApplicationSession()
{
    restoreHooks();
}

bool ApplicationSession::openFiles(const std::vector<std::filesystem::path>& files,
                                   MainWindow*                               origin)
{
    return m_fileOperations->openFiles(files, origin);
}

MainWindow& ApplicationSession::reopen()
{
    return m_windows->newDesign();
}

void ApplicationSession::reopenIfNoWindow()
{
    if (m_windows->windowCount() == 0)
    {
        static_cast<void>(reopen());
    }
}

void ApplicationSession::showInternalError(const Guard::Failure& failure)
{
    if (QThread::currentThread() != thread())
    {
        // SwingExceptionHandler shows its dialog on the event dispatch thread: a report from
        // another thread is reported again on the GUI thread, from its event loop.
        static_cast<void>(QMetaObject::invokeMethod(
            this, [failure] { Guard::report(failure); }, Qt::QueuedConnection));
        return;
    }
    QWidget* parent = QApplication::activeModalWidget();
    if (parent == nullptr)
    {
        parent = QApplication::activeWindow();
    }
    if (m_context->prompts().showInternalError(parent, failure) ==
        UserPrompts::InternalErrorAnswer::QUIT)
    {
        Q_EMIT exitRequested(kInternalErrorExitCode);
    }
}

void ApplicationSession::restoreHooks()
{
    if (m_previousOpenFilesHook.has_value())
    {
        static_cast<void>(m_application->setOpenFilesHook(std::move(*m_previousOpenFilesHook)));
        m_previousOpenFilesHook.reset();
    }
    if (m_previousQuitHook.has_value())
    {
        static_cast<void>(m_application->setQuitHook(std::move(*m_previousQuitHook)));
        m_previousQuitHook.reset();
    }
    if (m_previousErrorHook.has_value())
    {
        static_cast<void>(Guard::setHook(std::move(*m_previousErrorHook)));
        m_previousErrorHook.reset();
    }
}

void ApplicationSession::showStartupProblems(MainWindow& window)
{
    const std::vector<MotorDatabaseLoader::Problem>& problems = m_context->motorDatabaseProblems();
    if (problems.empty())
    {
        return;
    }
    // QtRocket's own texts (OpenRocket reports a broken bundled database as a bug).
    QStringList lines{
        tr("Not every motor of the motor database could be loaded: a design that "
           "uses a missing motor opens without it.")};
    for (const MotorDatabaseLoader::Problem& problem : problems)
    {
        lines.append(QtStrings::fromPath(problem.file) + QStringLiteral(": ") +
                     QtStrings::fromUtf8(problem.error.message));
    }
    m_context->prompts().showMessage(&window, {.severity = UserPrompts::Severity::WARNING,
                                               .title    = tr("Motor database"),
                                               .lines    = lines});
}

}  // namespace QtRocket::Gui
