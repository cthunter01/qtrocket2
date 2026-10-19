#pragma once

#include <QColor>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QTransform>
#include <QWidget>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/util/Geometry2D.h"

class QPainter;
class QPaintEvent;
class QResizeEvent;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The base of the figures drawn to scale (OpenRocket's AbstractScaleFigure): the design view's
/// rocket figure and the fin point editor's figure. It knows the scale, the subject it shows (in
/// metres, y up) and the pixel position of the subject's origin, and from them the size of its
/// canvas and the projection from metres to its pixels; a ScaleScrollArea shows it with rulers
/// and fits it to the visible area.
///
/// - The base scale converts metres to pixels: the screen's dots per inch times 39.3701 inches per
///   metre (dotsPerInch(): GUIUtil.getDPI()); the user scale multiplies it (1 = life size), and
///   absoluteScale() is their product.
/// - A subclass says what it shows: updateSubjectDimensions() sets the subject (what fitting
///   fits) and the content (everything drawn) with setSubjectBounds(), updateCanvasOrigin() sets
///   the origin's pixel position with setSubjectOrigin() (the design view's rules are
///   viewOrigin()), and autoZoomPoint() is where the scroll area scrolls when it fits.
/// - The canvas is at least the visible size and holds the content at the current scale with a
///   border of 30 x 20 pixels (canvasSizeFor()); it is the widget's minimum size and size hint,
///   so a QScrollArea with a resizable widget makes the figure the larger of its viewport and
///   the canvas, as Java's viewport does.
/// - projection() maps metres to the widget's pixels: translate(origin) * scale(s, -s), y up.
///
/// The figure paints its background (the theme's OR.colors.background, or the custom colour)
/// and then calls paintFigure().
///
/// Deviations from OpenRocket: scaleTo() refuses a zero scale, and a scale whose absolute value
/// overflows, as well as the non-finite and negative ones Java refuses (a zero scale makes the
/// projection singular and the rulers' tick distances infinite). Java's int arithmetic on the
/// canvas size and the origin wraps for absurd scales; here it saturates, and the canvas is at
/// most QWIDGETSIZE_MAX pixels, and viewOrigin() counts a subject at most as large as the canvas
/// can hold, so that the origin stays inside it. scaleTo() also updates the projection (Java's
/// does at the next paint). scaleTo() takes the current scale as no change also where Java's
/// tolerance underflows to 0 (scales below about 1e-161): Java's fitting scroll pane then fits
/// again and again until its stack overflows.
class ScaleFigure : public QWidget
{
    Q_OBJECT

public:
    /// AbstractScaleFigure.INCHES_PER_METER and METERS_PER_INCH.
    static constexpr double kInchesPerMeter = 39.3701;
    static constexpr double kMetersPerInch  = 0.0254;
    /// The pixels left at the edges when fitting (DEFAULT_BORDER_PIXELS_WIDTH and _HEIGHT).
    static constexpr int kBorderWidth  = 30;
    static constexpr int kBorderHeight = 20;

    /// The pixels per metre of a user scale of 1 (baseScale).
    [[nodiscard]] double baseScale() const noexcept { return m_baseScale; }
    /// The scale the user chose, 1 being life size (getUserScale()).
    [[nodiscard]] double userScale() const noexcept { return m_userScale; }
    /// The pixels per metre: baseScale() * userScale() (getAbsoluteScale()).
    [[nodiscard]] double absoluteScale() const noexcept { return m_scale; }
    /// The pixel position of the subject's origin in the widget (getSubjectOrigin()).
    [[nodiscard]] QPoint subjectOrigin() const noexcept { return m_origin; }
    /// What fitting fits, in metres (subjectBounds_m).
    [[nodiscard]] const Rectangle2D& subjectBounds() const noexcept { return m_subjectBounds; }
    /// Everything drawn, in metres (contentBounds_m).
    [[nodiscard]] const Rectangle2D& contentBounds() const noexcept { return m_contentBounds; }
    /// The visible size given to the last scaleTo() that changed the scale (visibleBounds_px).
    [[nodiscard]] QSize visibleBounds() const noexcept { return m_visibleBounds; }
    /// The size the figure asks for (Java's preferred size): canvasSizeFor() of the visible
    /// size, the content and the scale; 100 x 100 at first.
    [[nodiscard]] QSize canvasSize() const noexcept { return m_canvasSize; }
    /// Metres to the widget's pixels (projection), as of the last updateFigure() or scaleTo().
    [[nodiscard]] const QTransform& projection() const noexcept { return m_projection; }
    /// The point in metres at the widget's pixel @p pixel (the inverse of projection()).
    [[nodiscard]] QPointF toModel(const QPointF& pixel) const;

    /// Where a fitting scroll area scrolls to: the top left corner, in the widget's pixels, of
    /// the area to show (getAutoZoomPoint()).
    [[nodiscard]] virtual QPoint autoZoomPoint() const = 0;

    /// Sets the user scale to @p newScale for a visible area of @p visibleBounds pixels
    /// (scaleTo(double, Dimension)): updates the origin, the canvas and the projection and emits
    /// changed(). Nothing changes, and the result is false, when the scale is the current one or
    /// within newScale * 0.01 of it by MathUtil::equals() (a relative tolerance of newScale
    /// percent, a Java quirk kept: no change below 10 % at a scale of 10) and the visible size is
    /// the same, or when the scale is not finite, not positive, or overflows the absolute scale
    /// (see the class comment).
    bool scaleTo(double newScale, QSize visibleBounds);
    /// scaleTo() the fitScale() of @p visibleBounds (scaleTo(Dimension)); false for an empty size.
    bool scaleToFit(QSize visibleBounds);
    /// The user scale that fits the subject into @p visibleBounds (getFitScale()), after bringing
    /// the subject up to date (updateSubjectDimensions()): fitScaleFor(); the current user scale
    /// for an empty size.
    [[nodiscard]] double fitScale(QSize visibleBounds);

    /// Brings the subject, the canvas, the origin and the projection up to date and repaints
    /// (updateFigure()).
    void updateFigure();

    /// The background: the custom colour, or the theme's OR.colors.background when none is set
    /// (an invalid colour).
    [[nodiscard]] QColor backgroundColor() const;
    /// Sets a background colour instead of the theme's; an invalid colour returns to the theme's
    /// (setCustomBackgroundColor(); the design view's setting, Milestone 5).
    void setCustomBackgroundColor(const QColor& color);

    [[nodiscard]] QSize sizeHint() const override { return m_canvasSize; }
    [[nodiscard]] QSize minimumSizeHint() const override { return m_canvasSize; }

    /// The user scale that fits @p subject (metres) into @p visibleBounds pixels with the border
    /// at @p baseScale pixels per metre: Math.min of (height - 40) / (subject height *
    /// baseScale) and (width - 60) / (subject width * baseScale), NaN when either is NaN (an
    /// empty subject in an area of exactly the border's size).
    [[nodiscard]] static double fitScaleFor(QSize visibleBounds, const Rectangle2D& subject,
                                            double baseScale) noexcept;
    /// The canvas size for @p visibleBounds, @p content (metres) and @p scale (pixels per metre)
    /// (updateCanvasSize()): in each direction the larger of the visible size and the content's
    /// pixels, truncated, plus twice the border; between 0 and QWIDGETSIZE_MAX.
    [[nodiscard]] static QSize canvasSizeFor(QSize visibleBounds, const Rectangle2D& content,
                                             double scale) noexcept;
    /// The pixel position of the subject's origin in the design view (RocketFigure's
    /// updateCanvasOrigin()) for a widget of @p figureSize pixels showing @p subject (metres) at
    /// @p scale pixels per metre. With w and h the subject's pixels (truncated, and at most
    /// QWIDGETSIZE_MAX less twice the border: the largest canvas) and mid half the larger of the
    /// widget's width and w: side and top views (mid - w/2 - (int)(x * scale),
    /// max(widget height, h + 40)/2), the subject centred; back view (mid, 20 + widget
    /// height/2).
    [[nodiscard]] static QPoint viewOrigin(FigureView view, QSize figureSize,
                                           const Rectangle2D& subject, double scale) noexcept;

    /// GUIUtil.getDPI(): the preference "DPI" in tenths of a dot per inch when it is at least 10,
    /// else @p screenDotsPerInch, else 96 when that is below 1 (as Java's headless fallback).
    [[nodiscard]] static double dotsPerInch(int dpiPreference, int screenDotsPerInch) noexcept;
    /// dotsPerInch() of @p preferences' "DPI" (GuiPreferenceKeys::kDpi) and the primary screen's
    /// logical dots per inch, rounded (Toolkit.getScreenResolution()).
    [[nodiscard]] static double dotsPerInch(const Preferences& preferences);

Q_SIGNALS:
    /// The scale, the subject or what the figure shows changed (fireChangeEvent()).
    void changed();
    /// The widget was resized, after the figure was brought up to date for the new size (Java's
    /// ComponentListener.componentResized on the figure).
    void resized();

protected:
    /// A figure of @p dotsPerInch (dotsPerInch()) at user scale 1 and 100 x 100 pixels.
    ScaleFigure(double dotsPerInch, QWidget* parent);

    /// Sets the subject and the content (setSubjectBounds()) from what the figure shows.
    virtual void updateSubjectDimensions() = 0;
    /// Sets the origin (setSubjectOrigin()) for the current size, subject and scale.
    virtual void updateCanvasOrigin() = 0;
    /// Paints the figure over the background; nothing here.
    virtual void paintFigure(QPainter& painter);

    void setSubjectBounds(const Rectangle2D& subject, const Rectangle2D& content) noexcept;
    void setSubjectOrigin(QPoint origin) noexcept { m_origin = origin; }
    /// Sets the canvas size (canvasSizeFor()) as the minimum size when it changed.
    void updateCanvasSize();
    /// Sets the projection from the origin and the scale.
    void updateTransform();
    /// Emits changed().
    void fireChangeEvent();

    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    double      m_baseScale;
    double      m_userScale{1.0};
    double      m_scale;
    QPoint      m_origin;
    QSize       m_visibleBounds{0, 0};
    Rectangle2D m_contentBounds;
    Rectangle2D m_subjectBounds;
    QSize       m_canvasSize{100, 100};
    QTransform  m_projection;
    QColor      m_customBackground;
};

}  // namespace QtRocket::Gui
