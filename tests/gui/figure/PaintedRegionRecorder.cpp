#include "PaintedRegionRecorder.h"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPaintEvent>
#include <QRect>
#include <QRegion>
#include <QScrollBar>
#include <QWidget>
#include <QtCore/qtestsupport_core.h>

namespace QtRocket::Gui::Test
{

PaintedRegionRecorder::PaintedRegionRecorder(QWidget& widget, QRegion& painted)
  : m_widget(&widget), m_painted(&painted)
{
    widget.installEventFilter(this);
}

PaintedRegionRecorder::~PaintedRegionRecorder()
{
    m_widget->removeEventFilter(this);
}

bool PaintedRegionRecorder::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_widget && event->type() == QEvent::Paint)
    {
        if (const auto* paint = dynamic_cast<const QPaintEvent*>(event); paint != nullptr)
        {
            *m_painted += paint->region();
        }
    }
    return QObject::eventFilter(watched, event);
}

bool coversAll(const QRegion& region, const QRect& rect)
{
    return QRegion(rect).subtracted(region).isEmpty();
}

bool paintsAllOfItAfterScrolling(QWidget& widget, QScrollBar& bar, int delta,
                                 int timeoutMilliseconds)
{
    // The paints asked for before.
    QCoreApplication::processEvents();
    QRegion                     painted;
    const PaintedRegionRecorder recorder(widget, painted);
    const int                   before = bar.value();
    bar.setValue(before + delta);
    const QRect visible = widget.visibleRegion().boundingRect();
    if (bar.value() != before + delta || visible.isEmpty())
    {
        return false;
    }
    return QTest::qWaitFor([&painted, &visible] { return coversAll(painted, visible); },
                           timeoutMilliseconds);
}

}  // namespace QtRocket::Gui::Test
