#include "QtRocket/gui/util/WaitCursor.h"

#include <QCursor>
#include <QGuiApplication>
#include <Qt>

namespace QtRocket::Gui
{

WaitCursor::WaitCursor()
{
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

WaitCursor::~WaitCursor()
{
    QGuiApplication::restoreOverrideCursor();
}

}  // namespace QtRocket::Gui
