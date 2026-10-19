#include "tree/TreeViewMouse.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QPoint>
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

}  // namespace QtRocket::Gui::Test
