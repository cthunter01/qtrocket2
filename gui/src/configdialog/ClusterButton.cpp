#include "QtRocket/gui/configdialog/ClusterButton.h"

#include <cstddef>
#include <span>

#include <QBrush>
#include <QColor>
#include <QFrame>
#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/ClusterConfiguration.h"

namespace QtRocket::Gui
{

namespace
{

// The colours of ClusterSelectionPanel (Java's Color.RED, Color.GREEN and Color.BLACK: Qt's
// red, green and black have the same components).
constexpr Qt::GlobalColor kSelectedColor    = Qt::red;
constexpr Qt::GlobalColor kMotorFillColor   = Qt::green;
constexpr Qt::GlobalColor kMotorBorderColor = Qt::black;

}  // namespace

ClusterButton::ClusterButton(const ClusterConfiguration& configuration, QWidget* parent)
  : QFrame(parent), m_configuration(&configuration)
{
    setFixedSize(kButtonSize, kButtonSize);
    // BorderFactory.createBevelBorder(BevelBorder.LOWERED): two pixels, lowered.
    setFrameStyle(QFrame::Panel | QFrame::Sunken);
    setLineWidth(2);
    setToolTip(QtStrings::fromUtf8(configuration.getXmlName()));
    setObjectName(QtStrings::fromUtf8(configuration.getXmlName()));
    setFocusPolicy(Qt::NoFocus);
}

void ClusterButton::setSelected(bool selected)
{
    if (selected != m_selected)
    {
        m_selected = selected;
        update();
    }
}

QList<QRectF> ClusterButton::motorCircles(const ClusterConfiguration& configuration)
{
    const std::span<const double> points = configuration.getPoints();
    QList<QRectF>                 circles;
    constexpr double              kCentre = static_cast<double>(kButtonSize) / 2;
    constexpr double              kRadius = static_cast<double>(kMotorDiameter) / 2;
    for (std::size_t i = 0; i + 1 < points.size(); i += 2)
    {
        const double x  = points[i];
        const double y  = points[i + 1];
        const double px = kCentre + (x * kMotorDiameter);
        const double py = kCentre - (y * kMotorDiameter);
        circles.append(
            QRectF(QPointF(px - kRadius, py - kRadius), QPointF(px + kRadius, py + kRadius)));
    }
    return circles;
}

void ClusterButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(),
                     m_selected ? QColor(kSelectedColor) : palette().color(QPalette::Window));
    painter.setRenderHint(QPainter::Antialiasing);
    QPen border(kMotorBorderColor);
    border.setCosmetic(true);
    painter.setPen(border);
    painter.setBrush(QBrush(kMotorFillColor));
    for (const QRectF& circle : motorCircles(*m_configuration))
    {
        painter.drawEllipse(circle);
    }
    // The border over the fill, as Swing paints it after the component.
    painter.setRenderHint(QPainter::Antialiasing, false);
    drawFrame(&painter);
}

void ClusterButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_pressed = true;
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void ClusterButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressed)
    {
        m_pressed = false;
        event->accept();
        if (rect().contains(event->position().toPoint()))
        {
            Q_EMIT clicked();
        }
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

}  // namespace QtRocket::Gui
