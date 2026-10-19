#include "QtRocket/gui/app/Application.h"

#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QMetaEnum>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

namespace
{

/// What notify() was doing: "the MouseButtonPress event of QPushButton" (the event's number for
/// a type Qt has no name for).
[[nodiscard]] QString notifyContext(QEvent::Type type, const char* receiverClass)
{
    const char* const typeName = QMetaEnum::fromType<QEvent::Type>().valueToKey(type);
    const QString     event    = typeName != nullptr ? QString::fromLatin1(typeName)
                                                     : QString::number(static_cast<int>(type));
    const QString     receiver =
        receiverClass != nullptr ? QString::fromLatin1(receiverClass) : QStringLiteral("nothing");
    return QStringLiteral("the %1 event of %2").arg(event, receiver);
}

/// Reports @p error, which an event handler threw; never throws.
void reportNotifyFailure(const std::exception_ptr& error, QEvent::Type type,
                         const char* receiverClass) noexcept
{
    try
    {
        Guard::report(Guard::failureOf(error, notifyContext(type, receiverClass)));
    }
    catch (...)
    {
        // The report could not be made (out of memory): report that instead.
        Guard::reportCurrentException();
    }
}

}  // namespace

Application::Application(int& argc, char** argv) : QApplication(argc, argv)
{
    QCoreApplication::setApplicationName(BuildInfo::applicationName());
    QCoreApplication::setOrganizationName(BuildInfo::organizationName());
    QCoreApplication::setApplicationVersion(BuildInfo::applicationVersion());
    QApplication::setWindowIcon(Icons::windowIcon());
    // The WindowManager decides what the last closed window does.
    QGuiApplication::setQuitOnLastWindowClosed(false);
}

Application::~Application() = default;

Application* Application::current() noexcept
{
    return qobject_cast<Application*>(QCoreApplication::instance());
}

bool Application::notify(QObject* receiver, QEvent* event)
{
    // Taken before the event is delivered: the handler may delete the receiver before it throws.
    const QEvent::Type type = event != nullptr ? event->type() : QEvent::None;
    const char* const  receiverClass =
        receiver != nullptr ? receiver->metaObject()->className() : nullptr;
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch (...)
    {
        reportNotifyFailure(std::current_exception(), type, receiverClass);
        return false;
    }
}

Application::QuitHook Application::setQuitHook(QuitHook hook)
{
    return std::exchange(m_quitHook, std::move(hook));
}

Application::OpenFilesHook Application::setOpenFilesHook(OpenFilesHook hook)
{
    OpenFilesHook previous = std::exchange(m_openFilesHook, std::move(hook));
    deliverPendingFiles();
    return previous;
}

bool Application::event(QEvent* event)
{
    if (event->type() == QEvent::FileOpen)
    {
        fileOpenRequested(*event);
        return true;
    }
    if (event->type() == QEvent::Quit && m_quitHook)
    {
        quitRequested(*event);
        return true;
    }
    return QApplication::event(event);
}

void Application::quitRequested(QEvent& event)
{
    // A quit that fails (and is reported) leaves the application running.
    bool quit = false;
    Guard::run([this, &quit] { quit = m_quitHook(); }, QStringLiteral("Quit"));
    event.setAccepted(quit);
}

void Application::fileOpenRequested(const QEvent& event)
{
    const auto* const open = dynamic_cast<const QFileOpenEvent*>(&event);
    if (open == nullptr || open->file().isEmpty())
    {
        // Not a local file: QtRocket opens nothing else.
        return;
    }
    m_pendingFiles.push_back(QtStrings::toPath(open->file()));
    deliverPendingFiles();
}

void Application::deliverPendingFiles()
{
    if (!m_openFilesHook || m_pendingFiles.empty())
    {
        return;
    }
    const std::vector<std::filesystem::path> files = std::exchange(m_pendingFiles, {});
    Guard::run([this, &files] { m_openFilesHook(files); }, QStringLiteral("Open"));
}

}  // namespace QtRocket::Gui
