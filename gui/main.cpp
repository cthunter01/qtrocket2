// QtRocket: the graphical application. It reads the command line (--help, --version) and shows an
// empty main window; the application's start-up (preferences, materials, the motor database, the
// design windows) replaces the window later in Milestone 2.

#include <cstdio>
#include <print>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMainWindow>

#include "QtRocket/gui/util/BuildInfo.h"

namespace
{

int run(int argc, char** argv)
{
    const QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QtRocket::Gui::BuildInfo::applicationName());
    QCoreApplication::setOrganizationName(QtRocket::Gui::BuildInfo::organizationName());
    QCoreApplication::setApplicationVersion(QtRocket::Gui::BuildInfo::applicationVersion());

    // parse() rather than process(): process() answers --help and --version itself, in a message
    // box on Windows when the program has no console, and leaves through exit().
    QCommandLineParser       parser;
    const QCommandLineOption helpOption    = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    if (!parser.parse(QCoreApplication::arguments()))
    {
        std::println(stderr, "QtRocket: {}", parser.errorText().toStdString());
        return 1;
    }
    if (parser.isSet(versionOption))
    {
        std::println("{}", QtRocket::Gui::BuildInfo::versionText().toStdString());
        return 0;
    }
    if (parser.isSet(helpOption))
    {
        std::print("{}", parser.helpText().toStdString());
        return 0;
    }

    QMainWindow window;
    window.setWindowTitle(QtRocket::Gui::BuildInfo::applicationName());
    window.show();
    return QApplication::exec();
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        return run(argc, argv);
    }
    catch (...)
    {
        // Not std::print: it can throw, and nothing may escape main.
        static_cast<void>(std::fputs("QtRocket: internal error\n", stderr));
        return 1;
    }
}
