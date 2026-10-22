#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>

#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Geometry2D.h"

class QPainter;
class QPaintEvent;
class QWidget;

namespace QtRocket
{
class FreeformFinSet;
class Preferences;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The figure of the freeform fin editor (OpenRocket's FinPointFigure, swing
/// gui/scalefigure/FinPointFigure.java): one fin of the main fin set of its FinPointSelection in
/// the fin frame (x aft from the fin's leading edge on the body, y outward), drawn to scale with
/// the y axis up, over a grid, with the parent body, and the fin's points as handles. The
/// FinPointScrollArea around it handles the mouse; the figure itself takes no mouse event.
///
/// - The subject (what fitting fits) is the bounding box of the fin's points (getFinPoints());
///   the content adds the parent body's box: from the body's front (minus the fin's offset from
///   it) and minus the body's radius there, to the body's end and its largest radius above that
///   (Java reads the radius at minus the fin's offset, outside the body: a transition's fore
///   radius).
/// - The origin (the fin's leading edge) sits 30 pixels from the left of the content; it is 20
///   pixels below the fin's top when the visible height holds no more than the content and the
///   border, else the body's smallest radius above the bottom border (updateCanvasOrigin()).
/// - Painted in this order: the grid (lines every 10 and 100 pixels as the ruler unit's ticks
///   give them, minor and major colours, 1 pixel), the parent body (a transition's outline in 1
///   cm steps, a tube's rectangle from mountBoundsMax(); 2 pixels), the fin with its root
///   (getFinPointsWithRoot(); 1 pixel), the snap highlight (the segment from the point
///   highlightIndex(), 3 pixels) and the points' 12 pixel boxes, the selected one with a 16 pixel
///   box of the selected colour around it (1 pixel). The colours are ThemeColors' FIN_*.
/// - Hit tests, in the figure's pixels: indexAt() is the point nearest within 12 * 1.25 pixels,
///   segmentAt() the first segment of the outline (without the root) whose line passes within
///   12 pixels and whose ends both lie closer than its length; toFinPoint() is the point of the
///   fin frame at a pixel, limited to FreeformFinSet::kSnapLargerThan.
/// - The figure keeps the editor's FinPointEditHistory (Java: FinPointFigure's editHistory).
///
/// The fin set is looked up by id at every use (FinPointSelection): once it has left the rocket
/// the figure draws its background only and every hit test misses.
///
/// Deviations from OpenRocket:
/// - The hit tests work before the figure was first painted; Java's miss until then (they need
///   the handles its paint makes).
/// - The pixel maps of the hit tests are Java's inverse projection, ((x - origin x) / scale,
///   (y - origin y) / -scale), computed from projection(), which ScaleFigure keeps up to date
///   with every scale change (Java: at the next paint or updateFigure()).
/// - Without a parent body (a fin set that is in no SymmetricComponent) there is no body to draw
///   and the content is the fin's points (Java throws).
class FinPointFigure final : public ScaleFigure
{
    Q_OBJECT

public:
    /// The width of the grid lines, the fin's outline, the body, the snap highlight and the boxes,
    /// in pixels (GRID_LINE_BASE_WIDTH_PIXELS, LINE_WIDTH_*_PIXELS).
    static constexpr int kGridLineWidth      = 1;
    static constexpr int kFinLineWidth       = 1;
    static constexpr int kBodyLineWidth      = 2;
    static constexpr int kHighlightLineWidth = 3;
    static constexpr int kBoxLineWidth       = 1;
    /// The sides of a point's box and of the selected point's (BOX_WIDTH_PIXELS,
    /// SELECTED_BOX_WIDTH_PIXELS).
    static constexpr double kBoxWidth         = 12;
    static constexpr double kSelectedBoxWidth = kBoxWidth + 4;
    /// indexAt()'s radius as a multiple of kBoxWidth (POINT_HIT_DETECTION_MULTIPLIER).
    static constexpr double kPointHitMultiplier = 1.25;
    /// The grid's minor and major spacing, in pixels (MINOR_TICKS, MAJOR_TICKS).
    static constexpr double kMinorTicks = 10.0;
    static constexpr double kMajorTicks = 100.0;
    /// The steps of a transition's outline, in m (xResolution_m, a float in Java).
    static constexpr float kBodyResolution = 0.01F;

    /// The figure of the main fin set of @p target at @p dotsPerInch (ScaleFigure::dotsPerInch()),
    /// brought up to date (updateFigure()), its history holding the current points.
    FinPointFigure(FinPointSelection target, double dotsPerInch, QWidget* parent);
    /// The same at the dots per inch of @p preferences (ScaleFigure::dotsPerInch(preferences)).
    FinPointFigure(FinPointSelection target, const Preferences& preferences, QWidget* parent);

    [[nodiscard]] const FinPointSelection& target() const noexcept { return m_target; }

    /// (max(0, origin x - 30), 0) (getAutoZoomPoint()).
    [[nodiscard]] QPoint autoZoomPoint() const override;

    /// The point whose handle is at the figure's pixel @p pixel (getIndexByPoint()), -1 when none.
    [[nodiscard]] int indexAt(QPoint pixel) const;
    /// The segment at @p pixel (getSegmentByPoint()): the index of its end point (the segment
    /// from point i - 1 to point i), -1 when none.
    [[nodiscard]] int segmentAt(QPoint pixel) const;
    /// The point of the fin frame at @p pixel, each coordinate limited to
    /// +-FreeformFinSet::kSnapLargerThan (convertPoint()).
    [[nodiscard]] Point2D toFinPoint(QPoint pixel) const;
    /// The point of the fin frame at @p pixel, unlimited: Java's inverse projection.
    [[nodiscard]] Point2D toFinFrame(QPointF pixel) const;

    /// The point whose handle is selected (setSelectedIndex(), resetSelectedIndex()), -1 for
    /// none.
    [[nodiscard]] int selectedIndex() const noexcept { return m_selectedIndex; }
    void              setSelectedIndex(int index);
    void              resetSelectedIndex() { setSelectedIndex(-1); }
    /// The first point of the segment the snapping highlights (setHighlightIndex()), -1 for none.
    [[nodiscard]] int highlightIndex() const noexcept { return m_highlightIndex; }
    void              setHighlightIndex(int index);

    /// The editor's history of the points (beginUndoableEdit(), commitUndoableEdit(), undo(),
    /// ... of Java's figure).
    [[nodiscard]] FinPointEditHistory&       editHistory() noexcept { return m_history; }
    [[nodiscard]] const FinPointEditHistory& editHistory() const noexcept { return m_history; }

    /// The fin's points' bounding box and the parent body's, with its largest and its smallest
    /// radius (finBounds_m, mountBoundsMax_m, mountBoundsMin_m), as of the last update of the
    /// subject; the body's are empty without a parent body.
    [[nodiscard]] const BoundingBox& finBounds() const noexcept { return m_finBounds; }
    [[nodiscard]] const BoundingBox& mountBoundsMax() const noexcept { return m_mountBoundsMax; }
    [[nodiscard]] const BoundingBox& mountBoundsMin() const noexcept { return m_mountBoundsMin; }

    /// The unit of the grid: the rulers' of the ScaleScrollArea showing the figure, the default
    /// length unit when there is none.
    [[nodiscard]] const Unit& gridUnit() const;

protected:
    void updateSubjectDimensions() override;
    void updateCanvasOrigin() override;
    void paintFigure(QPainter& painter) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void        paintBackgroundGrid(QPainter& painter) const;
    void        paintRocketBody(QPainter& painter, const FreeformFinSet& fins) const;
    static void paintBodyTransition(QPainter& painter, const FreeformFinSet& fins);
    void        paintBodyTube(QPainter& painter) const;
    static void paintFinShape(QPainter& painter, const FreeformFinSet& fins);
    void        paintHighlight(QPainter& painter, const FreeformFinSet& fins) const;
    void        paintFinHandles(QPainter& painter, const FreeformFinSet& fins) const;

    FinPointSelection   m_target;
    FinPointEditHistory m_history;
    BoundingBox         m_finBounds;
    BoundingBox         m_mountBoundsMax;
    BoundingBox         m_mountBoundsMin;
    bool                m_hasMount{false};
    int                 m_selectedIndex{-1};
    int                 m_highlightIndex{-1};
    /// The region of the paint event being painted, in the figure's pixels.
    QRect m_paintRect;
};

}  // namespace QtRocket::Gui
