// QtRocket: the graphical application (OpenRocket's OpenRocket.main() and SwingStartup.main()).
// It reads the command line (--help, --version, design files to open), makes the services
// (ApplicationContext: the user's preferences, the materials, the motor database) and starts the
// application (ApplicationSession: the error hook, the windows, the first window), then runs the
// event loop until the last window is closed or the user quits.
//
// Two environment variables are test hooks of the program (the test gui.start,
// tests/gui/CMakeLists.txt); nothing else sets them:
// - QTROCKET_TEST_SETTINGS_DIRECTORY: the directory the preferences file goes into
//   (<directory>/QtRocket/QtRocket.ini) instead of the user's, so that a test of the program
//   leaves the user's preferences alone;
// - QTROCKET_TEST_QUIT_AFTER_START (any value but empty): the application quits as File > Quit
//   does as soon as its event loop runs.

#include <cstdio>
#include <exception>
#include <memory>
#include <print>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QObject>
#include <QSettings>
#include <QString>
#include <Qt>
#include <QtEnvironmentVariables>

#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/QtStrings.h"

namespace
{

using QtRocket::Gui::ApplicationSession;

/// What the command line asks for.
struct CommandLine
{
    /// The exit code when the program is done after the command line (--help, --version, an
    /// error), or -1 to start the application.
    int                         exitCode = -1;
    ApplicationSession::Options options;
};

CommandLine readCommandLine()
{
    // parse() rather than process(): process() answers --help and --version itself, in a message
    // box on Windows when the program has no console, and leaves through exit().
    QCommandLineParser       parser;
    const QCommandLineOption helpOption    = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    // QtRocket's own text.
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QCoreApplication::translate("main", "Design files to open."),
                                 QStringLiteral("[file...]"));
    CommandLine commandLine;
    if (!parser.parse(QCoreApplication::arguments()))
    {
        std::println(stderr, "QtRocket: {}", QtRocket::Gui::QtStrings::toUtf8(parser.errorText()));
        commandLine.exitCode = 1;
        return commandLine;
    }
    if (parser.isSet(versionOption))
    {
        std::println("{}",
                     QtRocket::Gui::QtStrings::toUtf8(QtRocket::Gui::BuildInfo::versionText()));
        commandLine.exitCode = 0;
        return commandLine;
    }
    // --help-all (which addHelpOption() adds too) prints the same help: QCommandLineParser has
    // no public text of Qt's own options, which QApplication has read already.
    if (parser.isSet(helpOption) || parser.isSet(QStringLiteral("help-all")))
    {
        std::print("{}", QtRocket::Gui::QtStrings::toUtf8(parser.helpText()));
        commandLine.exitCode = 0;
        return commandLine;
    }
    for (const QString& file : parser.positionalArguments())
    {
        commandLine.options.files.push_back(QtRocket::Gui::QtStrings::toPath(file));
    }
    commandLine.options.quitAfterStart =
        !qEnvironmentVariableIsEmpty("QTROCKET_TEST_QUIT_AFTER_START");
    return commandLine;
}

int run(int argc, char** argv)
{
    QtRocket::Gui::Application application(argc, argv);
    const CommandLine          commandLine = readCommandLine();
    if (commandLine.exitCode >= 0)
    {
        return commandLine.exitCode;
    }

    if (const QString directory = qEnvironmentVariable("QTROCKET_TEST_SETTINGS_DIRECTORY");
        !directory.isEmpty())
    {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
    }
    ApplicationSession session(application,
                               std::make_unique<QtRocket::Gui::ApplicationContext>(
                                   QtRocket::Gui::SettingsPreferences::openDefault(),
                                   QtRocket::Gui::DataPaths::forApplication(),
                                   std::make_unique<QtRocket::Gui::WidgetUserPrompts>()),
                               commandLine.options);
    // Queued: the slot that asked to exit returns first, and an exit asked before the event
    // loop runs ends it as soon as it does.
    QObject::connect(&session, &ApplicationSession::exitRequested, &application,
                     &QCoreApplication::exit, Qt::QueuedConnection);
    return QApplication::exec();
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        return run(argc, argv);
    }
    // What the start-up throws (a BugError before the event loop runs); in the event loop the
    // error hook and Application::notify() catch everything. Not std::print: it can throw, and
    // nothing may escape main.
    catch (const std::exception& error)
    {
        static_cast<void>(std::fputs("QtRocket: internal error: ", stderr));
        static_cast<void>(std::fputs(error.what(), stderr));
        static_cast<void>(std::fputs("\n", stderr));
        return 1;
    }
    catch (...)
    {
        static_cast<void>(std::fputs("QtRocket: internal error\n", stderr));
        return 1;
    }
}
