#pragma once

#include <string>

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

class QEvent;
class QScreen;
class QWidget;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The size and the place of the application's windows (OpenRocket: GUIUtil.rememberWindowSize()
/// and rememberWindowPosition() with SwingPreferences' window keys, WindowLocationUtil, and
/// BasicFrame's initial size and positionRelativeToExistingFrames()).
///
/// A window's geometry is stored in the preferences' node "windows" under "geometry.<name>"
/// (GuiPreferenceKeys::kNodeWindows, kWindowGeometryPrefix) as QWidget::saveGeometry() in
/// Base64: its normal position and size, whether it is maximized or full screen, and its
/// screen. Windows of one kind share a name (Java: the window's class), so the window of a kind
/// moved or closed last decides where the next one opens. A main window opens as BasicFrame's:
/// 90 % of the screen (setInitialSize()), then the remembered geometry (an object of this class),
/// then 30 pixels below and to the right of the window opened before it (cascade()).
///
/// An object of this class remembers the geometry of the window it is a child of: made with
/// makeChild<WindowGeometry>(window, preferences, name), it restores the stored geometry at once
/// (restore()) and stores the window's geometry whenever the shown window is moved or resized,
/// as GUIUtil's listeners store it on every move and resize, and also when its state changes
/// (maximized, normal again) and when it is closed.
///
/// Deviations: one key holds position, size and state, where SwingPreferences keeps
/// "position.<class>" and "size.<class>" (a frame's size or "max") apart, and the window is
/// named by its caller, not by its Java class; the initial size is 90 % of the screen for the
/// window without its frame (Java's setSize() includes the frame, whose size Qt knows only once
/// the window is shown); a stored place that no screen holds any more is replaced by the centre
/// of the window's screen (WindowLocationUtil's rule), where SwingPreferences ignores it and lets
/// the platform place the window (a Qt window that was given a place cannot go back to that).
/// WindowLocationUtil.setLocationWithinScreen() is not ported: OpenRocket never calls it.
class WindowGeometry final : public QObject
{
    Q_OBJECT

public:
    /// BasicFrame.CASCADE_OFFSET_X and CASCADE_OFFSET_Y.
    static constexpr int kCascadeOffset = 30;

    /// Remembers the geometry of @p window (the parent) under @p name in @p preferences, which
    /// must outlive the window: restores it now (restored() tells whether there was one) and
    /// stores it from now on (see the class comment).
    WindowGeometry(Preferences& preferences, QString name, QWidget* window);

    /// Whether a stored geometry was restored when this object was made.
    [[nodiscard]] bool restored() const noexcept { return m_restored; }

    /// The key of the window @p name in the node "windows": "geometry.<name>".
    [[nodiscard]] static std::string key(const QString& name);

    /// Stores the geometry of @p window under @p name.
    static void save(Preferences& preferences, const QString& name, const QWidget& window);
    /// Gives @p window the geometry stored under @p name, kept on a screen: when no screen holds
    /// the window's top left corner any more (a screen that is gone, SwingPreferences'
    /// getWindowPosition()), it is centred on its screen (WindowLocationUtil). Whether a valid
    /// geometry was stored; nothing changes when none was (or it does not decode).
    static bool restore(const Preferences& preferences, const QString& name, QWidget& window);

    /// BasicFrame's initial size: 90 % of the width and the height of the window's screen
    /// (Toolkit.getScreenSize(), the whole screen), in whole pixels as Java's int arithmetic.
    static void setInitialSize(QWidget& window);

    /// BasicFrame.positionRelativeToExistingFrames(): moves @p window (its frame) to 30 pixels
    /// right of and below the frame of @p previous, the window opened before it, kept within the
    /// available area of @p previous's screen (cascadePosition()).
    static void cascade(QWidget& window, const QWidget& previous);
    /// The place cascade() moves a frame of @p size to: @p previous + (30, 30); a coordinate past
    /// the last that keeps the frame inside @p available starts again at the area's first, and
    /// none is before the first (BasicFrame's arithmetic, with the screen's insets taken out of
    /// @p available, QScreen::availableGeometry()).
    [[nodiscard]] static QPoint cascadePosition(QPoint previous, QSize size,
                                                const QRect& available);

    /// WindowLocationUtil.moveIfOutsideOfMonitor(): centres @p window on its screen when the
    /// screen does not hold the window's top left corner. Whether it was moved. Deviation: Java
    /// does it only for a visible window (it asks for the location on the screen); Qt knows a
    /// hidden window's place too.
    static bool moveIfOutsideOfMonitor(QWidget& window);
    /// WindowLocationUtil.moveIfOutsideOfParentMonitor(): centres @p window on the screen of
    /// @p parent when it is on another screen, or its top left corner is not on that screen.
    /// Whether it was moved. Deviation as for moveIfOutsideOfMonitor(): Java tests the corner
    /// only for a visible window.
    static bool moveIfOutsideOfParentMonitor(QWidget& window, const QWidget& parent);

    /// Stores the geometry of the shown window when it is moved, resized, maximized (or made
    /// normal again) or closed.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Centres the frame of @p window on @p screen (WindowLocationUtil's arithmetic).
    static void centreOn(QWidget& window, const QScreen& screen);

    Preferences* m_preferences;
    QString      m_name;
    QWidget*     m_window;
    bool         m_restored{false};
};

}  // namespace QtRocket::Gui
