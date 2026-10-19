#include "QtRocket/gui/settings/WindowGeometry.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include <QByteArray>
#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The screen @p window is on, or the primary screen; null only without any screen.
[[nodiscard]] QScreen* screenOf(const QWidget& window)
{
    QScreen* const screen = window.screen();
    return screen != nullptr ? screen : QGuiApplication::primaryScreen();
}

/// The top left corner of @p window's frame (Java: a window's location).
[[nodiscard]] QPoint frameTopLeft(const QWidget& window)
{
    return window.frameGeometry().topLeft();
}

}  // namespace

WindowGeometry::WindowGeometry(Preferences& preferences, QString name, QWidget* window)
  : QObject(window), m_preferences(&preferences), m_name(std::move(name)), m_window(window)
{
    QTROCKET_ASSERT(window != nullptr);
    m_restored = restore(preferences, m_name, *window);
    window->installEventFilter(this);
}

std::string WindowGeometry::key(const QString& name)
{
    return std::format("{}{}", GuiPreferenceKeys::kWindowGeometryPrefix, QtStrings::toUtf8(name));
}

void WindowGeometry::save(Preferences& preferences, const QString& name, const QWidget& window)
{
    preferences.getNode(GuiPreferenceKeys::kNodeWindows)
        .put(key(name), window.saveGeometry().toBase64().toStdString());
}

bool WindowGeometry::restore(const Preferences& preferences, const QString& name, QWidget& window)
{
    const Preferences* const node = preferences.findNode(GuiPreferenceKeys::kNodeWindows);
    if (node == nullptr)
    {
        return false;
    }
    const std::optional<std::string> text = node->get(key(name));
    if (!text.has_value())
    {
        return false;
    }
    const QByteArray::FromBase64Result geometry = QByteArray::fromBase64Encoding(
        QByteArray::fromStdString(*text), QByteArray::AbortOnBase64DecodingErrors);
    if (!geometry || !window.restoreGeometry(*geometry))
    {
        return false;
    }
    // Qt already keeps a restored window on the screen it was on, or on the primary one when that
    // screen is gone; this is SwingPreferences' rule on top (a place on no screen is not used).
    const QScreen* const screen = screenOf(window);
    if (screen != nullptr && QGuiApplication::screenAt(frameTopLeft(window)) == nullptr)
    {
        centreOn(window, *screen);
    }
    return true;
}

void WindowGeometry::setInitialSize(QWidget& window)
{
    const QScreen* const screen = screenOf(window);
    if (screen == nullptr)
    {
        return;
    }
    const QSize size = screen->size();
    window.resize(size.width() * 9 / 10, size.height() * 9 / 10);
}

void WindowGeometry::cascade(QWidget& window, const QWidget& previous)
{
    // Java: the previous frame's screen, else this frame's, else the default screen.
    const QScreen* const screen = screenOf(previous);
    if (screen == nullptr)
    {
        return;
    }
    window.move(cascadePosition(frameTopLeft(previous), window.frameGeometry().size(),
                                screen->availableGeometry()));
}

QPoint WindowGeometry::cascadePosition(QPoint previous, QSize size, const QRect& available)
{
    const int minX = available.x();
    const int minY = available.y();
    const int maxX = std::max(available.x() + available.width() - size.width(), minX);
    const int maxY = std::max(available.y() + available.height() - size.height(), minY);

    int x = previous.x() + kCascadeOffset;
    int y = previous.y() + kCascadeOffset;
    if (x > maxX)
    {
        x = minX;
    }
    if (y > maxY)
    {
        y = minY;
    }
    return {std::max(x, minX), std::max(y, minY)};
}

bool WindowGeometry::moveIfOutsideOfMonitor(QWidget& window)
{
    const QScreen* const screen = screenOf(window);
    if (screen == nullptr || screen->geometry().contains(frameTopLeft(window)))
    {
        return false;
    }
    centreOn(window, *screen);
    return true;
}

bool WindowGeometry::moveIfOutsideOfParentMonitor(QWidget& window, const QWidget& parent)
{
    const QScreen* const parentScreen = screenOf(parent);
    if (parentScreen == nullptr)
    {
        return false;
    }
    if (window.screen() == parentScreen && parentScreen->geometry().contains(frameTopLeft(window)))
    {
        return false;
    }
    centreOn(window, *parentScreen);
    return true;
}

void WindowGeometry::centreOn(QWidget& window, const QScreen& screen)
{
    const QRect bounds = screen.geometry();
    const QSize size   = window.frameGeometry().size();
    window.move((bounds.width() / 2) - (size.width() / 2) + bounds.x(),
                (bounds.height() / 2) - (size.height() / 2) + bounds.y());
}

bool WindowGeometry::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window && m_window->isVisible())
    {
        switch (event->type())
        {
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::WindowStateChange:
            case QEvent::Close:
                save(*m_preferences, m_name, *m_window);
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace QtRocket::Gui
