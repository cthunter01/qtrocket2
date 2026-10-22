#pragma once

// The mouse over a figure of the design view (RocketPanel's figure in its scroll area), as a
// platform delivers it to a widget: each event goes to the widget under the mouse and on to its
// parents while it is not accepted (a press the figure leaves is the scroll area's pan), and the
// modifiers held go with the moves too (QTest::mouseMove() sends none). And where a component
// can be clicked in a RocketFigure.

#include <optional>

#include <QEvent>
#include <QPoint>
#include <Qt>

class QWidget;

namespace QtRocket
{
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class RocketFigure;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// Sends @p widget a mouse event of @p type at @p position (the widget's coordinates), with
/// @p button (the button that changed; none for a move), @p buttons (those held during and after
/// it) and @p modifiers. Whether some widget accepted it.
bool sendMouse(QWidget& widget, QEvent::Type type, QPoint position, Qt::MouseButton button,
               Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = {});

/// A click at @p position: a press and a release of @p button.
void clickAt(QWidget& widget, QPoint position, Qt::MouseButton button = Qt::LeftButton,
             Qt::KeyboardModifiers modifiers = {});

/// The second click of a double-click at @p position: the double-click event and the release
/// (the first click is clickAt()'s).
void secondClickAt(QWidget& widget, QPoint position, Qt::MouseButton button = Qt::LeftButton,
                   Qt::KeyboardModifiers modifiers = {});

/// A drag of @p button from @p from to @p to, points of the widget's parent (the scroll area's
/// viewport, under which the figure moves while it pans): the press, a move every few pixels, the
/// release, with @p modifiers held throughout.
void dragFrom(QWidget& widget, QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton,
              Qt::KeyboardModifiers modifiers = {});

/// A pixel of @p figure where @p component is the topmost of the components there
/// (RocketFigure::componentsAt()), searched inside the bounds of its shapes; nullopt when there
/// is none.
[[nodiscard]] std::optional<QPoint> figurePixelOf(RocketFigure&          figure,
                                                  const RocketComponent& component);

}  // namespace QtRocket::Gui::Test
