// What GuiTestMain.cpp sets up for every GUI test: the platform, the environment, the locale and
// where QSettings writes.

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QtEnvironmentVariables>

#include <gtest/gtest.h>

namespace
{

TEST(GuiTestEnvironment, RunsOnThePlatformOfTheEnvironment)
{
    // main() sets QT_QPA_PLATFORM to offscreen when it is unset (ctest sets it too).
    EXPECT_FALSE(QGuiApplication::platformName().isEmpty());
    EXPECT_EQ(QGuiApplication::platformName(), qEnvironmentVariable("QT_QPA_PLATFORM"));
}

TEST(GuiTestEnvironment, LeavesNoScaleThemeOrStyleVariable)
{
    for (const char* name :
         {"QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS", "QT_ENABLE_HIGHDPI_SCALING", "QT_FONT_DPI",
          "QT_QPA_PLATFORMTHEME", "QT_STYLE_OVERRIDE"})
    {
        EXPECT_FALSE(qEnvironmentVariableIsSet(name)) << name;
    }
}

TEST(GuiTestEnvironment, QtPaintsOnTheGuiThreadOnly)
{
    // Qt's thread pool of the raster engine is off: its threads' locking is invisible to
    // ThreadSanitizer in an uninstrumented Qt.
    EXPECT_TRUE(qEnvironmentVariableIsSet("QT_NO_GUI_THREADPOOL"));
}

TEST(GuiTestEnvironment, ThePlatformHasFonts)
{
    // Without the system's fonts (the offscreen platform on Windows without QT_QPA_FONTDIR) Qt
    // warns in whichever test first measures a text, and every text is drawn in boxes.
    EXPECT_FALSE(QFontDatabase::families().isEmpty());
#ifdef _WIN32
    EXPECT_FALSE(qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"));
#endif
}

TEST(GuiTestEnvironment, TheDefaultLocaleIsAmericanEnglish)
{
    EXPECT_EQ(QLocale().name(), QStringLiteral("en_US"));
    EXPECT_EQ(QLocale().toString(12.5), QStringLiteral("12.5"));
}

TEST(GuiTestEnvironment, StandardPathsAreInTestMode)
{
    EXPECT_TRUE(QStandardPaths::isTestModeEnabled());
}

TEST(GuiTestEnvironment, SettingsWithoutAFileAreIniFilesInATemporaryDirectory)
{
    const QSettings settings;
    EXPECT_EQ(settings.format(), QSettings::IniFormat);
    EXPECT_TRUE(settings.fileName().startsWith(QDir::tempPath()))
        << settings.fileName().toStdString();
    const QSettings system(QSettings::IniFormat, QSettings::SystemScope, QStringLiteral("QtRocket"),
                           QStringLiteral("QtRocket"));
    EXPECT_TRUE(system.fileName().startsWith(QDir::tempPath())) << system.fileName().toStdString();
}

}  // namespace
