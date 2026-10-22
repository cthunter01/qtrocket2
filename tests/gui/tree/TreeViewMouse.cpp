#include "tree/TreeViewMouse.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPoint>
#include <QPointF>
#include <QTest>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui::Test
{

void doubleClickAt(QWidget& viewport, const QPoint& position, Qt::KeyboardModifiers modifiers,
                   Qt::MouseButton button)
{
    QTest::mousePress(&viewport, button, modifiers, position);
    QTest::mouseRelease(&viewport, button, modifiers, position);
    QTest::mouseDClick(&viewport, button, modifiers, position);
    QTest::mouseRelease(&viewport, button, modifiers, position);
}

void rightClickAt(QWidget& viewport, const QPoint& position, Qt::KeyboardModifiers modifiers)
{
    QTest::mouseClick(&viewport, Qt::RightButton, modifiers, position);
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, viewport.mapToGlobal(position),
                            modifiers);
    QApplication::sendEvent(&viewport, &event);
}

ViewDrop dropAt(QWidget& viewport, const QMimeData& data, QPoint position,
                Qt::KeyboardModifiers modifiers)
{
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent       enter(position, actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(&viewport, &enter);
    QDragMoveEvent move(position, actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(&viewport, &move);
    ViewDrop   result{.moveAccepted = move.isAccepted(), .moveAction = move.dropAction()};
    QDropEvent drop(QPointF(position), actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(&viewport, &drop);
    result.dropAccepted = drop.isAccepted();
    result.dropAction   = drop.dropAction();
    return result;
}

}  // namespace QtRocket::Gui::Test
