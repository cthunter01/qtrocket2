#include "QtRocket/gui/settings/WindowGeometry.h"

#include <optional>
#include <string>
#include <vector>

#include <QByteArray>
#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

namespace
{

using WindowGeometryTest = Test::GuiTest;
using Names              = std::vector<std::string>;

/// The geometry stored under @p name, or nullopt.
[[nodiscard]] std::optional<std::string> storedGeometry(const Preferences& preferences,
                                                        const QString&     name)
{
    const Preferences* const windows = preferences.findNode(GuiPreferenceKeys::kNodeWindows);
    if (windows == nullptr)
    {
        return std::nullopt;
    }
    return windows->get(WindowGeometry::key(name));
}

/// The offscreen platform's screen (800 x 800 pixels at 0, 0 unless configured otherwise).
[[nodiscard]] QRect screenGeometry()
{
    return QGuiApplication::primaryScreen()->geometry();
}

TEST_F(WindowGeometryTest, KeyIsInTheWindowsNode)
{
    EXPECT_EQ(WindowGeometry::key(QStringLiteral("MainWindow")), "geometry.MainWindow");

    InMemoryPreferences preferences;
    QWidget             window;
    window.setGeometry(100, 120, 300, 200);
    WindowGeometry::save(preferences, QStringLiteral("MainWindow"), window);
    EXPECT_EQ(preferences.childrenNames(), Names{"windows"});
    EXPECT_EQ(preferences.getNode("windows").keys(), Names{"geometry.MainWindow"});
    EXPECT_EQ(storedGeometry(preferences, QStringLiteral("MainWindow")),
              window.saveGeometry().toBase64().toStdString());
}

TEST_F(WindowGeometryTest, RestoresTheStoredGeometry)
{
    InMemoryPreferences preferences;
    QWidget             shown;
    shown.setGeometry(100, 120, 300, 200);
    shown.show();
    shown.move(150, 160);
    WindowGeometry::save(preferences, QStringLiteral("Test"), shown);

    QWidget other;
    EXPECT_TRUE(WindowGeometry::restore(preferences, QStringLiteral("Test"), other));
    EXPECT_EQ(other.geometry(), shown.geometry());
    EXPECT_FALSE(other.isMaximized());
}

TEST_F(WindowGeometryTest, RestoresTheMaximizedState)
{
    InMemoryPreferences preferences;
    QWidget             shown;
    shown.setGeometry(100, 120, 300, 200);
    shown.showMaximized();
    WindowGeometry::save(preferences, QStringLiteral("Test"), shown);

    QWidget other;
    EXPECT_TRUE(WindowGeometry::restore(preferences, QStringLiteral("Test"), other));
    EXPECT_TRUE(other.isMaximized());
}

TEST_F(WindowGeometryTest, NothingOrSomethingElseStoredChangesNothing)
{
    InMemoryPreferences preferences;
    QWidget             window;
    window.setGeometry(10, 20, 300, 200);
    EXPECT_FALSE(WindowGeometry::restore(preferences, QStringLiteral("Test"), window));

    Preferences& windows = preferences.getNode("windows");
    windows.put("geometry.Test", "not Base64 at all!");
    EXPECT_FALSE(WindowGeometry::restore(preferences, QStringLiteral("Test"), window));
    windows.put("geometry.Test", QByteArray("no geometry").toBase64().toStdString());
    EXPECT_FALSE(WindowGeometry::restore(preferences, QStringLiteral("Test"), window));
    windows.put("geometry.Test", "");
    EXPECT_FALSE(WindowGeometry::restore(preferences, QStringLiteral("Test"), window));
    EXPECT_EQ(window.geometry(), QRect(10, 20, 300, 200));
}

TEST_F(WindowGeometryTest, RemembersTheWindowWhileItIsShown)
{
    InMemoryPreferences preferences;
    QWidget             window;
    auto* const geometry = makeChild<WindowGeometry>(&window, preferences, QStringLiteral("Test"));
    geometry->setObjectName(QStringLiteral("geometry"));
    EXPECT_FALSE(geometry->restored());
    EXPECT_EQ(window.findChild<WindowGeometry*>(QStringLiteral("geometry")), geometry);

    // A hidden window is not remembered.
    window.setGeometry(100, 120, 300, 200);
    EXPECT_EQ(storedGeometry(preferences, QStringLiteral("Test")), std::nullopt);

    // A shown one is, on every move and resize.
    window.show();
    window.move(200, 210);
    EXPECT_EQ(storedGeometry(preferences, QStringLiteral("Test")),
              window.saveGeometry().toBase64().toStdString());
    window.resize(320, 240);
    EXPECT_EQ(storedGeometry(preferences, QStringLiteral("Test")),
              window.saveGeometry().toBase64().toStdString());
    const QRect lastShown = window.geometry();
    window.close();

    // The next window of its kind opens there.
    QWidget     next;
    auto* const nextGeometry =
        makeChild<WindowGeometry>(&next, preferences, QStringLiteral("Test"));
    nextGeometry->setObjectName(QStringLiteral("geometry"));
    EXPECT_TRUE(nextGeometry->restored());
    EXPECT_EQ(next.geometry(), lastShown);
}

TEST_F(WindowGeometryTest, RemembersTheWindowWhenItIsClosed)
{
    InMemoryPreferences preferences;
    QWidget             window;
    auto* const geometry = makeChild<WindowGeometry>(&window, preferences, QStringLiteral("Test"));
    geometry->setObjectName(QStringLiteral("geometry"));
    window.setGeometry(100, 120, 300, 200);
    window.show();
    // Another window of its kind stored its place since this one last moved.
    preferences.getNode("windows").remove(WindowGeometry::key(QStringLiteral("Test")));
    window.close();
    EXPECT_EQ(storedGeometry(preferences, QStringLiteral("Test")),
              window.saveGeometry().toBase64().toStdString());
}

TEST_F(WindowGeometryTest, AStoredPlaceOnNoScreenIsRestoredOntoTheScreen)
{
    // SwingPreferences.isPointOnScreen(): a place that is on no screen any more is not used.
    InMemoryPreferences preferences;
    QWidget             away;
    away.setGeometry(5000, 5000, 300, 200);
    WindowGeometry::save(preferences, QStringLiteral("Test"), away);

    QWidget window;
    EXPECT_TRUE(WindowGeometry::restore(preferences, QStringLiteral("Test"), window));
    EXPECT_NE(QGuiApplication::screenAt(window.frameGeometry().topLeft()), nullptr);
    EXPECT_TRUE(screenGeometry().contains(window.frameGeometry()));
    EXPECT_EQ(window.size(), QSize(300, 200));
}

TEST_F(WindowGeometryTest, InitialSizeIsNinetyPercentOfTheScreen)
{
    QWidget window;
    WindowGeometry::setInitialSize(window);
    const QSize screen = screenGeometry().size();
    EXPECT_EQ(window.size(), QSize(screen.width() * 9 / 10, screen.height() * 9 / 10));
}

TEST_F(WindowGeometryTest, CascadePositionFollowsBasicFrame)
{
    const QRect available(0, 0, 800, 800);
    EXPECT_EQ(WindowGeometry::cascadePosition({100, 100}, {300, 200}, available), QPoint(130, 130));
    // Past the last place that keeps the frame inside, a coordinate starts again at the first.
    EXPECT_EQ(WindowGeometry::cascadePosition({480, 590}, {300, 200}, available), QPoint(0, 0));
    EXPECT_EQ(WindowGeometry::cascadePosition({470, 570}, {300, 200}, available), QPoint(500, 600));
    // Never before the first place of the area (here moved by the screen's insets).
    const QRect inset(10, 20, 780, 760);
    EXPECT_EQ(WindowGeometry::cascadePosition({-100, -100}, {300, 200}, inset), QPoint(10, 20));
    // A frame larger than the area goes to its first place.
    EXPECT_EQ(WindowGeometry::cascadePosition({0, 0}, {900, 900}, available), QPoint(0, 0));
}

TEST_F(WindowGeometryTest, CascadePlacesTheNextWindowBelowAndRightOfThePreviousOne)
{
    QWidget previous;
    previous.setGeometry(100, 100, 300, 200);
    previous.show();
    QWidget next;
    next.resize(300, 200);
    WindowGeometry::cascade(next, previous);
    EXPECT_EQ(next.frameGeometry().topLeft(),
              previous.frameGeometry().topLeft() +
                  QPoint(WindowGeometry::kCascadeOffset, WindowGeometry::kCascadeOffset));
}

TEST_F(WindowGeometryTest, WindowOutsideOfItsScreenIsCentred)
{
    QWidget window;
    window.setGeometry(100, 100, 300, 200);
    EXPECT_FALSE(WindowGeometry::moveIfOutsideOfMonitor(window));
    EXPECT_EQ(window.pos(), QPoint(100, 100));

    window.move(5000, 5000);
    EXPECT_TRUE(WindowGeometry::moveIfOutsideOfMonitor(window));
    const QRect screen = screenGeometry();
    EXPECT_EQ(window.pos(), QPoint((screen.width() / 2) - 150 + screen.x(),
                                   (screen.height() / 2) - 100 + screen.y()));
}

TEST_F(WindowGeometryTest, WindowOutsideOfItsParentsScreenIsCentredThere)
{
    QWidget parent;
    parent.setGeometry(0, 0, 400, 300);
    QWidget window;
    window.setGeometry(100, 100, 300, 200);
    EXPECT_FALSE(WindowGeometry::moveIfOutsideOfParentMonitor(window, parent));

    window.move(-5000, 100);
    EXPECT_TRUE(WindowGeometry::moveIfOutsideOfParentMonitor(window, parent));
    const QRect screen = screenGeometry();
    EXPECT_EQ(window.pos(), QPoint((screen.width() / 2) - 150 + screen.x(),
                                   (screen.height() / 2) - 100 + screen.y()));
}

}  // namespace

}  // namespace QtRocket::Gui
