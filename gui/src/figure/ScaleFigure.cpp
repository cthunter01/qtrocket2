#include "QtRocket/gui/figure/ScaleFigure.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QColor>
#include <QGuiApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QResizeEvent>
#include <QScreen>
#include <QSize>
#include <QTransform>
#include <QWidget>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// Java's (int) narrowing of a double, widened for the sums Java computes in int (which wrap
/// there; here they saturate in clampToInt()/clampToWidgetSize()).
[[nodiscard]] long long javaInt(double value) noexcept
{
    return MathUtil::javaIntCast(value);
}

[[nodiscard]] int clampToInt(long long value) noexcept
{
    return static_cast<int>(std::clamp<long long>(value, std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max()));
}

[[nodiscard]] int clampToWidgetSize(long long value) noexcept
{
    return static_cast<int>(std::clamp<long long>(value, 0, QWIDGETSIZE_MAX));
}

}  // namespace

ScaleFigure::ScaleFigure(double dotsPerInch, QWidget* parent)
  : QWidget(parent), m_baseScale(dotsPerInch * kInchesPerMeter), m_scale(m_baseScale * m_userScale)
{
    // Java: preferred size and size 100 x 100.
    resize(m_canvasSize);
}

QPointF ScaleFigure::toModel(const QPointF& pixel) const
{
    return m_projection.inverted().map(pixel);
}

bool ScaleFigure::scaleTo(double newScale, QSize visibleBounds)
{
    // Deviation: the very scale it has is no change either. Java's test alone,
    // |user - new| < new * 0.01 * new, fails for an identical scale below about 1e-161, where the
    // product underflows to 0: a scroll area that fits a design longer than about 1e161 m (an .ork
    // can hold one) would then be told of a change and fit again without end, until the stack
    // overflows (Java catches its StackOverflowError on the event thread; here the process dies).
    if ((newScale == m_userScale || MathUtil::equals(m_userScale, newScale, newScale * 0.01)) &&
        m_visibleBounds == visibleBounds)
    {
        return false;
    }
    // Java refuses the infinite, NaN and negative scales; zero and an absolute scale that is not
    // a positive finite number are refused too (class comment).
    const double absolute = m_baseScale * newScale;
    if (!std::isfinite(newScale) || newScale <= 0 || !std::isfinite(absolute) || absolute <= 0)
    {
        return false;
    }

    m_userScale = newScale;
    m_scale     = absolute;
    updateCanvasOrigin();

    m_visibleBounds = visibleBounds;
    updateCanvasSize();
    updateTransform();

    fireChangeEvent();
    return true;
}

bool ScaleFigure::scaleToFit(QSize visibleBounds)
{
    if (visibleBounds.width() <= 0 || visibleBounds.height() <= 0)
    {
        return false;
    }
    return scaleTo(fitScale(visibleBounds), visibleBounds);
}

double ScaleFigure::fitScale(QSize visibleBounds)
{
    if (visibleBounds.width() <= 0 || visibleBounds.height() <= 0)
    {
        return m_userScale;
    }
    updateSubjectDimensions();
    return fitScaleFor(visibleBounds, m_subjectBounds, m_baseScale);
}

void ScaleFigure::updateFigure()
{
    updateSubjectDimensions();
    updateCanvasSize();
    updateCanvasOrigin();
    updateTransform();

    updateGeometry();
    update();
}

QColor ScaleFigure::backgroundColor() const
{
    return m_customBackground.isValid() ? m_customBackground
                                        : ThemeColors::color(ThemeColors::Key::BACKGROUND);
}

void ScaleFigure::setCustomBackgroundColor(const QColor& color)
{
    m_customBackground = color;
    update();
}

double ScaleFigure::fitScaleFor(QSize visibleBounds, const Rectangle2D& subject,
                                double baseScale) noexcept
{
    const double widthScale =
        (visibleBounds.width() - (2 * kBorderWidth)) / (subject.width * baseScale);
    const double heightScale =
        (visibleBounds.height() - (2 * kBorderHeight)) / (subject.height * baseScale);
    return MathUtil::javaMin(heightScale, widthScale);
}

QSize ScaleFigure::canvasSizeFor(QSize visibleBounds, const Rectangle2D& content,
                                 double scale) noexcept
{
    const long long width = std::max<long long>(
        visibleBounds.width(), javaInt(content.width * scale) + (2LL * kBorderWidth));
    const long long height = std::max<long long>(
        visibleBounds.height(), javaInt(content.height * scale) + (2LL * kBorderHeight));
    return {clampToWidgetSize(width), clampToWidgetSize(height)};
}

QPoint ScaleFigure::viewOrigin(FigureView view, QSize figureSize, const Rectangle2D& subject,
                               double scale) noexcept
{
    // Deviation: the subject counts at most as large as the largest canvas (canvasSizeFor()), so
    // that the origin of an absurdly large design stays inside the canvas (Java's int arithmetic
    // wraps instead).
    const long long subjectWidth =
        std::min<long long>(javaInt(subject.width * scale), QWIDGETSIZE_MAX - (2LL * kBorderWidth));
    const long long subjectHeight = std::min<long long>(javaInt(subject.height * scale),
                                                        QWIDGETSIZE_MAX - (2LL * kBorderHeight));
    const long long midX          = std::max<long long>(figureSize.width(), subjectWidth) / 2;

    if (view == FigureView::BACK)
    {
        return {clampToInt(midX), clampToInt(kBorderHeight + (figureSize.height() / 2))};
    }
    const long long originX = midX - (subjectWidth / 2) - javaInt(subject.x * scale);
    const long long originY =
        std::max<long long>(figureSize.height(), subjectHeight + (2LL * kBorderHeight)) / 2;
    return {clampToInt(originX), clampToInt(originY)};
}

double ScaleFigure::dotsPerInch(int dpiPreference, int screenDotsPerInch) noexcept
{
    long long dpi = dpiPreference;  // tenths of a dot per inch
    if (dpi < 10)
    {
        dpi = static_cast<long long>(screenDotsPerInch) * 10;
    }
    if (dpi < 10)
    {
        dpi = 960;
    }
    return static_cast<double>(dpi) / 10.0;
}

double ScaleFigure::dotsPerInch(const Preferences& preferences)
{
    const int preference =
        preferences.getInt(GuiPreferenceKeys::kDpi, GuiPreferenceKeys::kDpiDefault);
    const QScreen* screen = QGuiApplication::primaryScreen();
    const int      screenDpi =
        screen == nullptr ? 0 : clampToInt(std::llround(screen->logicalDotsPerInchX()));
    return dotsPerInch(preference, screenDpi);
}

void ScaleFigure::paintFigure(QPainter& /*painter*/) { }

void ScaleFigure::setSubjectBounds(const Rectangle2D& subject, const Rectangle2D& content) noexcept
{
    m_subjectBounds = subject;
    m_contentBounds = content;
}

void ScaleFigure::updateCanvasSize()
{
    const QSize canvas = canvasSizeFor(m_visibleBounds, m_contentBounds, m_scale);
    if (canvas != m_canvasSize || minimumSize() != canvas)
    {
        m_canvasSize = canvas;
        setMinimumSize(canvas);
        updateGeometry();
    }
}

void ScaleFigure::updateTransform()
{
    QTransform projection;
    projection.translate(m_origin.x(), m_origin.y());
    // Mirror the y axis upwards.
    projection.scale(m_scale, -m_scale);
    m_projection = projection;
}

void ScaleFigure::fireChangeEvent()
{
    Q_EMIT changed();
}

void ScaleFigure::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), backgroundColor());
    paintFigure(painter);
}

void ScaleFigure::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateFigure();
    Q_EMIT resized();
}

}  // namespace QtRocket::Gui
