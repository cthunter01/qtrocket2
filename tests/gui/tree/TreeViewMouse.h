#pragma once

// The mouse as a platform delivers it to an item view's viewport, for the tests of the component
// tree (QTest's helpers alone send less than a platform does).

#include <QPoint>
#include <Qt>

class QWidget;

namespace QtRocket::Gui::Test
{

/// A double-click at @p position of @p viewport as a platform delivers it to a widget: press,
/// release, double-click, release (Qt drops the second press; QTest::mouseDClick() alone sends
/// only the double-click).
void doubleClickAt(QWidget& viewport, const QPoint& position, Qt::KeyboardModifiers modifiers = {},
                   Qt::MouseButton button = Qt::LeftButton);

/// A click of the right button at @p position of @p viewport, then the context menu event of the
/// mouse there (QTest sends no context menu events).
void rightClickAt(QWidget& viewport, const QPoint& position, Qt::KeyboardModifiers modifiers = {});

}  // namespace QtRocket::Gui::Test
