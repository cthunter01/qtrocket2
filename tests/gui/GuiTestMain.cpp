// main() of QtRocket_gui_tests: GoogleTest with one QApplication for the whole process, set up so
// that no test can reach the user's desktop or depend on it. The QApplication is the
// application's own (QtRocket::Gui::Application), so that the tests run under its backstop for
// exceptions (notify()) and its event handling.
// - The platform is offscreen unless QT_QPA_PLATFORM names another: no window ever appears. ctest
//   sets the variable for the tests, but not for gtest_discover_tests' listing run
//   (--gtest_list_tests), which builds the QApplication too and has no display in CI.
// - The variables that scale the screen or the fonts, pick a platform theme or a style are unset,
//   and the default locale is en_US (a German one would read "12,5" in a spin box). So the tests
//   see Qt's default key bindings only (offscreen: the Windows scheme), never those of the GNOME
//   or KDE themes, which differ (GNOME's standard Delete keys hold Ctrl+D).
// - Qt paints on the GUI thread only (QT_NO_GUI_THREADPOOL): Qt's raster engine shares a large
//   fill out to the threads of its own pool, whose locking ThreadSanitizer cannot see in an
//   uninstrumented Qt, so every painting test would report races under the tsan preset.
// - The offscreen platform has fonts on every system: on Windows it reads them from
//   QT_QPA_FONTDIR only, which is the system's font directory unless set. The font database is
//   read, and the application's font looked up, before the first test, so that the platform's
//   one-time messages about fonts are printed then instead of failing whichever test measures a
//   text first (on macOS the offscreen platform's font family "Sans Serif" does not exist, which
//   Qt warns of once).
// - QStandardPaths' test mode, and every QSettings made without a file name is an INI file in a
//   temporary directory of this process (the code under test takes its own INI file anyway). In
//   that mode the application's data (DataPaths::forApplication()) has no user's data directory
//   and no bundled component presets: no test reads the user's files or makes directories in the
//   user's home.
// - A Qt warning, critical or fatal message during a test fails it (ExpectedQtMessage).
// - So does a report of the error hook (Guard: a guarded slot or the notify() backstop caught an
//   exception, or the code reported an error condition) that the test does not expect: the
//   hook of the process fails the test, unless the test records the reports itself
//   (RecordingGuardHook) or starts the application (ApplicationSession, whose hook asks the
//   test's UserPrompts).

#include <cstdio>
#include <print>
#include <string>

#include <QFontMetrics>
#include <QGuiApplication>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QtEnvironmentVariables>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"

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
    qputenv("QT_NO_GUI_THREADPOOL", "1");
#ifdef _WIN32
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
    {
        const QString windows = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
        qputenv("QT_QPA_FONTDIR", (windows + QStringLiteral("/Fonts")).toLocal8Bit());
    }
#endif
}

/// After the QApplication, before the tests: reads the font database and resolves the
/// application's font, as the first text a test measures would.
void resolveTheApplicationFont()
{
    const QFontMetrics metrics(QGuiApplication::font());
    static_cast<void>(metrics.height());
    static_cast<void>(metrics.horizontalAdvance(QStringLiteral("QtRocket")));
}

/// The error hook of the test process: a report fails the test that runs (outside a test it is
/// only printed, as GoogleTest has no test to fail).
void failOnReport(const QtRocket::Gui::Guard::Failure& failure)
{
    const std::string text =
        QtRocket::Gui::QtStrings::toUtf8(QtRocket::Gui::Guard::describe(failure));
    if (::testing::UnitTest::GetInstance()->current_test_info() != nullptr)
    {
        ADD_FAILURE() << "unexpected report of the error hook: " << text;
    }
    else
    {
        std::println(stderr, "QtRocket_gui_tests: report of the error hook: {}", text);
    }
}

int runTests(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    QtRocket::Gui::Test::ExpectedQtMessage::installHandler();
    const QtRocket::Gui::Application application(argc, argv);
    static_cast<void>(QtRocket::Gui::Guard::setHook(&failOnReport));
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QStandardPaths::setTestModeEnabled(true);
    resolveTheApplicationFont();

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
