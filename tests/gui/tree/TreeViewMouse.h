#pragma once

// The mouse as a platform delivers it to an item view's viewport, for the tests of the component
// tree (QTest's helpers alone send less than a platform does), and a drag dropped there.

#include <QPoint>
#include <Qt>

class QMimeData;
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

/// What a drop on an item view's viewport did (dropAt()).
struct ViewDrop
{
    /// Whether the view accepted the drag over the point (the cursor allows a drop).
    bool moveAccepted = false;
    /// The action it proposed there.
    Qt::DropAction moveAction = Qt::IgnoreAction;
    /// Whether the drop was accepted (QAbstractItemView: the model dropped).
    bool dropAccepted = false;
    /// The action the drop reported to the drag (QDropEvent::dropAction() after the drop).
    Qt::DropAction dropAction = Qt::IgnoreAction;
};

/// A drag of @p data entering @p viewport, moving to @p position and dropped there, the keys
/// @p modifiers held (Shift: a move, Ctrl: a copy; QPlatformDrag's rule, offscreen too); as a
/// platform delivers the events to a widget. The drop is sent also where the view refused the
/// drag: the model must refuse it then too.
ViewDrop dropAt(QWidget& viewport, const QMimeData& data, QPoint position,
                Qt::KeyboardModifiers modifiers);

}  // namespace QtRocket::Gui::Test
