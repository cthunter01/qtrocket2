#include "QtRocket/gui/plot/ThrustCurvePreview.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

ThrustCurvePreview::ThrustCurvePreview(QWidget* parent)
  : QFrame(parent), m_chart(makeChild<ThrustCurvePlot>(this)), m_zoomIcon(makeChild<QLabel>(this))
{
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    // A ChartPanel without a popup menu shows none, nor its parent's.
    setContextMenuPolicy(Qt::PreventContextMenu);
    m_chart->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_zoomIcon->setObjectName(QStringLiteral("zoomIcon"));
    m_zoomIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_zoomIcon->setAlignment(Qt::AlignCenter);
    m_zoomIcon->setFixedSize(kZoomIconBox, kZoomIconBox);
    // Above the chart (layer 1 of the layered pane, the chart on layer 0).
    m_zoomIcon->raise();
    updateZoomIcon();
    placeChildren();
}

QSize ThrustCurvePreview::sizeHint() const
{
    return {kWidth, kHeight};
}

QSize ThrustCurvePreview::minimumSizeHint() const
{
    return {kWidth, kHeight};
}

void ThrustCurvePreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_pressPosition = event->position().toPoint();
    }
    event->accept();
}

void ThrustCurvePreview::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton || !m_pressPosition.has_value())
    {
        return;
    }
    const QPoint moved = event->position().toPoint() - *m_pressPosition;
    m_pressPosition.reset();
    if (moved.manhattanLength() < QApplication::startDragDistance())
    {
        Q_EMIT plotRequested();
    }
}

void ThrustCurvePreview::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    placeChildren();
}

void ThrustCurvePreview::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange ||
        event->type() == QEvent::DevicePixelRatioChange)
    {
        updateZoomIcon();
    }
}

void ThrustCurvePreview::placeChildren()
{
    m_chart->setGeometry(0, 0, width(), height());
    m_zoomIcon->move(width() - kZoomIconBox, kZoomIconTop);
}

void ThrustCurvePreview::updateZoomIcon()
{
    const int   size = Icons::iconHeight(Icons::Id::ZOOM_IN, font());
    const QIcon icon = Icons::icon(Icons::Id::ZOOM_IN);
    m_zoomIcon->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatio()));
}

}  // namespace QtRocket::Gui
