#include "QtRocket/gui/figure/ScaleScrollArea.h"

#include <cmath>

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/figure/Ruler.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// How deep updateScaling() may nest: it rescales the figure and lays it out, which resizes it,
/// which calls updateScaling() again, two or three levels deep when nothing is wrong. The limit
/// only keeps a figure that never settles from overflowing the stack.
constexpr int kMaxResizeDepth = 8;

[[nodiscard]] bool isPositiveFinite(double value) noexcept
{
    return std::isfinite(value) && value > 0;
}

}  // namespace

ScaleScrollArea::ScaleScrollArea(ScaleFigure* figure, QWidget* parent)
  : QScrollArea(parent),
    m_figure(figure),
    m_rulerUnit(&unitGroup(UnitGroupId::LENGTH).getDefaultUnit())
{
    QTROCKET_ASSERT(figure != nullptr);
    setWidget(figure);
    // The figure is the larger of the viewport and its canvas (its minimum size), as Java's
    // viewport lays out a view smaller than itself.
    setWidgetResizable(true);
    setViewportMargins(Ruler::kSize, Ruler::kSize, 0, 0);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    horizontalScrollBar()->setSingleStep(kScrollStep);
    verticalScrollBar()->setSingleStep(kScrollStep);

    m_horizontalRuler = makeChild<Ruler>(this, Qt::Horizontal, figure);
    m_horizontalRuler->setObjectName(QStringLiteral("horizontalRuler"));
    m_verticalRuler = makeChild<Ruler>(this, Qt::Vertical, figure);
    m_verticalRuler->setObjectName(QStringLiteral("verticalRuler"));

    m_unitLabel = makeChild<QLabel>(this);
    m_unitLabel->setObjectName(QStringLiteral("rulerUnit"));
    m_unitLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_unitLabel->font();
    font.setPixelSize(kCornerFontPixelSize);
    m_unitLabel->setFont(font);
    m_corner = m_unitLabel;
    updateUnitLabel();

    connect(figure, &ScaleFigure::changed, this, [this] { figureChanged(); });
    connect(figure, &ScaleFigure::resized, this, [this] { updateScaling(); });
    layoutRulers();
}

void ScaleScrollArea::setFitting(bool shouldFit)
{
    m_fit = shouldFit;
    if (shouldFit)
    {
        m_figureRescaled = m_figure->scaleToFit(fitViewSize());
        revalidate();
    }
}

double ScaleScrollArea::userScale() const
{
    return m_figure->userScale();
}

double ScaleScrollArea::fitScale()
{
    return m_figure->fitScale(fitViewSize());
}

double ScaleScrollArea::relativeScale()
{
    const double fit = fitScale();
    if (!isPositiveFinite(fit))
    {
        return 1.0;
    }
    return m_figure->userScale() / fit;
}

void ScaleScrollArea::setScaling(double newScale)
{
    // Match if closer than 1 %.
    if (MathUtil::equals(newScale, m_figure->userScale(), 0.01))
    {
        return;
    }

    const QRect view = viewRect();
    m_viewCentre     = QPointF((view.x() + (view.width() / 2.0)) / m_figure->width(),
                               (view.y() + (view.height() / 2.0)) / m_figure->height());

    // An explicit zoom level turns fitting off.
    m_fit            = false;
    m_figureRescaled = m_figure->scaleTo(newScale, viewport()->size());
    revalidate();
    // The figure has its new size (Java centres the view once its viewport has laid it out,
    // in componentResized(); the resize here came while the flag above was still unset).
    updateScaling();
}

void ScaleScrollArea::setRelativeScaling(double relativeScale)
{
    const double fit = fitScale();
    if (!isPositiveFinite(fit))
    {
        return;
    }
    setScaling(relativeScale * fit);
}

QSize ScaleScrollArea::fitViewSize() const
{
    return {width() - kViewportMargin, height() - kViewportMargin};
}

QRect ScaleScrollArea::viewRect() const
{
    return {horizontalScrollBar()->value(), verticalScrollBar()->value(), viewport()->width(),
            viewport()->height()};
}

void ScaleScrollArea::updateScaling()
{
    if (m_resizeDepth >= kMaxResizeDepth || (!m_fit && !m_figureRescaled))
    {
        return;
    }
    const QScopedValueRollback<int> depth(m_resizeDepth, m_resizeDepth + 1);
    if (m_fit)
    {
        m_figureRescaled = m_figure->scaleToFit(fitViewSize());
        scrollFigureTo(m_figure->autoZoomPoint());
    }
    else
    {
        // Always centre the view again after a zoom.
        const QSize view = viewport()->size();
        scrollFigureTo(
            {MathUtil::javaIntCast(m_viewCentre.x() * m_figure->width()) - (view.width() / 2),
             MathUtil::javaIntCast(m_viewCentre.y() * m_figure->height()) - (view.height() / 2)});
    }
    revalidate();
    m_figureRescaled = false;
    m_figure->update();
}

void ScaleScrollArea::setRulerUnit(const Unit& unit)
{
    if (&unit == m_rulerUnit)
    {
        return;
    }
    m_rulerUnit = &unit;
    m_horizontalRuler->setUnit(unit);
    m_verticalRuler->setUnit(unit);
    updateUnitLabel();
    m_figure->update();
    Q_EMIT rulerUnitChanged();
}

void ScaleScrollArea::updateRulerUnit()
{
    setRulerUnit(unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
}

void ScaleScrollArea::setRulerCornerWidget(QWidget* widget)
{
    QTROCKET_ASSERT(widget != nullptr);
    if (widget == m_corner)
    {
        return;
    }
    if (m_corner != nullptr)
    {
        m_corner->hide();
        m_corner->deleteLater();
    }
    m_unitLabel = nullptr;
    widget->setParent(this);
    m_corner = widget;
    layoutRulers();
    widget->show();
}

void ScaleScrollArea::resizeEvent(QResizeEvent* event)
{
    // The viewport's resize (QAbstractScrollArea passes it here): QScrollArea resizes the figure.
    QScrollArea::resizeEvent(event);
    layoutRulers();
    updateScaling();
}

void ScaleScrollArea::showEvent(QShowEvent* event)
{
    QScrollArea::showEvent(event);
    layoutRulers();
}

void ScaleScrollArea::scrollContentsBy(int dx, int dy)
{
    QScrollArea::scrollContentsBy(dx, dy);
    m_horizontalRuler->update();
    m_verticalRuler->update();
}

void ScaleScrollArea::mousePressEvent(QMouseEvent* event)
{
    if (!isPanning() && (m_panButtons & event->button()) != Qt::NoButton)
    {
        m_panButton   = event->button();
        m_panPosition = event->position().toPoint();
        event->accept();
        return;
    }
    QScrollArea::mousePressEvent(event);
}

void ScaleScrollArea::mouseMoveEvent(QMouseEvent* event)
{
    if (!isPanning())
    {
        QScrollArea::mouseMoveEvent(event);
        return;
    }
    // The viewport's coordinates do not move with the figure: the view moves with the mouse.
    const QPoint position = event->position().toPoint();
    const QPoint delta    = m_panPosition - position;
    m_panPosition         = position;
    scrollFigureTo(viewRect().topLeft() + delta);
    event->accept();
}

void ScaleScrollArea::mouseReleaseEvent(QMouseEvent* event)
{
    if (isPanning() && event->button() == m_panButton)
    {
        m_panButton = Qt::NoButton;
        event->accept();
        return;
    }
    QScrollArea::mouseReleaseEvent(event);
}

void ScaleScrollArea::revalidate()
{
    m_figure->updateFigure();
    // Lay the figure out now: QScrollArea resizes its widget on a layout request.
    QEvent request(QEvent::LayoutRequest);
    QCoreApplication::sendEvent(this, &request);
    m_horizontalRuler->update();
    m_verticalRuler->update();
}

void ScaleScrollArea::figureChanged()
{
    m_horizontalRuler->update();
    m_verticalRuler->update();
    if (m_fit)
    {
        m_figureRescaled = m_figure->scaleToFit(fitViewSize());
    }
}

void ScaleScrollArea::scrollFigureTo(QPoint topLeft)
{
    horizontalScrollBar()->setValue(topLeft.x());
    verticalScrollBar()->setValue(topLeft.y());
}

void ScaleScrollArea::layoutRulers()
{
    const QRect view = viewport()->geometry();
    m_horizontalRuler->setGeometry(view.left(), view.top() - Ruler::kSize, view.width(),
                                   Ruler::kSize);
    m_verticalRuler->setGeometry(view.left() - Ruler::kSize, view.top(), Ruler::kSize,
                                 view.height());
    if (m_corner != nullptr)
    {
        m_corner->setGeometry(view.left() - Ruler::kSize, view.top() - Ruler::kSize, Ruler::kSize,
                              Ruler::kSize);
    }
}

void ScaleScrollArea::updateUnitLabel()
{
    if (m_unitLabel != nullptr)
    {
        m_unitLabel->setText(QtStrings::unitSymbol(*m_rulerUnit));
    }
}

}  // namespace QtRocket::Gui
