// main() of QtRocket_gui_tests: GoogleTest with one QApplication for the whole process, set up so
// that no test can reach the user's desktop or depend on it.
// - The platform is offscreen unless QT_QPA_PLATFORM names another: no window ever appears. ctest
//   sets the variable for the tests, but not for gtest_discover_tests' listing run
//   (--gtest_list_tests), which builds the QApplication too and has no display in CI.
// - The variables that scale the screen or the fonts, pick a platform theme or a style are unset,
//   and the default locale is en_US (a German one would read "12,5" in a spin box).
// - QStandardPaths' test mode, and every QSettings made without a file name is an INI file in a
//   temporary directory of this process (the code under test takes its own INI file anyway).
// - A Qt warning, critical or fatal message during a test fails it (ExpectedQtMessage).

#include <cstdio>
#include <print>

#include <QApplication>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEnvironmentVariables>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"

namespace
{

/// Before the QApplication: the platform plugin, the scale and the theme are read by its
/// constructor.
void prepareEnvironment()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    for (const char* name :
         {"QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS", "QT_ENABLE_HIGHDPI_SCALING", "QT_FONT_DPI",
          "QT_QPA_PLATFORMTHEME", "QT_STYLE_OVERRIDE"})
    {
        qunsetenv(name);
    }
}

int runTests(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    QtRocket::Gui::Test::ExpectedQtMessage::installHandler();
    const QApplication application(argc, argv);
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QStandardPaths::setTestModeEnabled(true);

    const QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid())
    {
        std::println(stderr, "QtRocket_gui_tests: cannot make a temporary directory: {}",
                     settingsDirectory.errorString().toStdString());
        return 1;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    for (const QSettings::Format format : {QSettings::NativeFormat, QSettings::IniFormat})
    {
        for (const QSettings::Scope scope : {QSettings::UserScope, QSettings::SystemScope})
        {
            QSettings::setPath(format, scope, settingsDirectory.path());
        }
    }
    return RUN_ALL_TESTS();
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        prepareEnvironment();
        return runTests(argc, argv);
    }
    catch (...)
    {
        // GoogleTest catches what a test throws; this is for the set-up around the tests. Not
        // std::print: it can throw, and nothing may escape main.
        static_cast<void>(std::fputs("QtRocket_gui_tests: internal error\n", stderr));
        return 1;
    }
}
