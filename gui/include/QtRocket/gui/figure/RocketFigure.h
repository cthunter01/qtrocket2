#pragma once

#include <optional>
#include <span>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Uuid.h"

class QPainter;
class QWidget;

namespace QtRocket
{
class Preferences;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class FigureElement;

/// The design view's 2D figure of the rocket (OpenRocket's RocketFigure): the rocket's selected
/// flight configuration in the side, top or back view, rotated about its axis, the selected
/// components drawn wider, with the figure elements over it (the CG and CP carets, the information
/// overlay). It is a ScaleFigure: a ScaleScrollArea shows it with rulers and fits it.
///
/// - What it draws is a RocketFigureGeometry of the core (the shapes, the motors, the subject's
///   bounds), built for the view, the rotation, the selection and the preference ShowMarkers
///   (Preferences::isShowMarkers()), and painted by RocketFigurePainter. The geometry is kept
///   until something it depends on changes and built again when it is next needed (a paint, a
///   resize, a hit test), so a resize does not rebuild it.
/// - It listens to the rocket. Every change event drops the geometry at once (a TREE or UNDO
///   event replaces every component object: the geometry's pointers must not outlive the event),
///   and the events of one pass of the event loop are answered once: updateFigure(), then
///   rocketChanged(). The rocket must have its events enabled (a document's rocket); a change
///   that fires no event is shown after refresh().
/// - The selection is kept as the components' ids, so it survives an undo or redo, which loads
///   components with the same ids.
/// - The mouse is not handled here: the design view (RocketPanel) watches the figure's mouse
///   events and asks componentsAt() what was clicked. Mouse events the figure leaves unaccepted
///   reach the scroll area, which pans with its pan buttons.
///
/// Paint order, as Java's paintComponent(): the background, the shapes and the motors
/// (RocketFigurePainter), the relative extras (when isDrawCarets()), the absolute extras, the
/// relative top extras (when isDrawCarets()). The figure elements are observers: whoever adds one
/// keeps it alive until it is removed or the figure is destroyed.
///
/// Deviations from OpenRocket:
/// - Java rebuilds the shapes at every paint, and its hit test (getComponentsByPoint()) uses the
///   shapes of the last paint, which may be older than the rocket; here the geometry is that of
///   the rocket as it is now, and the hit test builds it when it is missing. The hit test is
///   FigureShape::contains(), Java's Shape.contains() (the NON_ZERO rule for paths, open
///   polylines closed), not QPainterPath::contains().
/// - A paint brings the subject, the origin and the projection up to date as Java's does, but not
///   the canvas size (Java's updateCanvasSize() sets the preferred size from paintComponent()): a
///   widget's size is not changed while it paints. updateFigure() does it, after every change.
/// - setRotation() ignores a rotation that is not a finite number (Java takes it and draws
///   nothing, every coordinate NaN).
/// - The selection is compared by id where Java compares by identity: after an undo, Java's tree
///   keeps the detached components selected and the figure highlights nothing.
/// - Java's figure is told of rocket changes by RocketPanel (updateFigure() from its listeners);
///   here it listens itself, coalesced per pass of the event loop.
class RocketFigure final : public ScaleFigure
{
    Q_OBJECT

public:
    /// The figure of @p rocket at the dots per inch of @p preferences (ScaleFigure::dotsPerInch()),
    /// side view, rotation 0, nothing selected, carets drawn. Both must outlive the figure.
    RocketFigure(Rocket& rocket, const Preferences& preferences, QWidget* parent);
    /// The same at @p dotsPerInch (a test's fixed resolution, a preview's).
    RocketFigure(Rocket& rocket, const Preferences& preferences, double dotsPerInch,
                 QWidget* parent);

    [[nodiscard]] Rocket& rocket() const noexcept { return *m_rocket; }

    /// The view shown (getCurrentViewType()).
    [[nodiscard]] FigureView view() const noexcept { return m_view; }
    /// Shows @p view (setType()): updateFigure() and changed(); nothing when it is shown already.
    void setView(FigureView view);

    /// The rotation about the rocket's axis in radians (getRotation()); with
    /// @p applyTopViewOffset, less PI/2 in the top view (getRotation(true): the angle the carets
    /// and the rotation control use).
    [[nodiscard]] double rotation(bool applyTopViewOffset = false) const noexcept;
    /// Rotates the figure to @p rotation radians (setRotation()): updateFigure() and changed();
    /// nothing when it equals the current rotation (MathUtil::equals()) or is not finite.
    void setRotation(double rotation);

    /// The ids of the components drawn as selected (getSelection()).
    [[nodiscard]] const std::vector<Uuid>& selection() const noexcept { return m_selection; }
    /// Draws @p components as selected (setSelection(); empty: none): updateFigure() and
    /// changed(). Only their ids are kept.
    void setSelection(std::span<const RocketComponent* const> components);
    /// Whether @p component is drawn as selected.
    [[nodiscard]] bool isSelected(const RocketComponent& component) const noexcept;

    /// Whether the relative extras (the carets) are drawn (isDrawCarets(); default true).
    [[nodiscard]] bool isDrawCarets() const noexcept { return m_drawCarets; }
    /// Shows or hides the relative extras (setDrawCarets()) and repaints.
    void setDrawCarets(bool drawCarets);

    /// The elements painted in the figure's coordinates after the rocket (addRelativeExtra()...):
    /// the carets. An element added twice is painted twice, as in Java.
    void addRelativeExtra(FigureElement& element);
    void removeRelativeExtra(const FigureElement& element);
    void clearRelativeExtra() noexcept;
    /// The elements painted in the widget's pixels after the relative ones: the information
    /// overlay.
    void addAbsoluteExtra(FigureElement& element);
    void removeAbsoluteExtra(const FigureElement& element);
    void clearAbsoluteExtra() noexcept;
    /// The elements painted last, in the figure's coordinates (the calipers' highlights,
    /// Milestone 5).
    void addRelativeTopExtra(FigureElement& element);
    void removeRelativeTopExtra(const FigureElement& element);
    void clearRelativeTopExtra() noexcept;

    /// The components with a shape at the widget's pixel @p pixel (getComponentsByPoint()): each
    /// once, in the order their shapes are drawn (RocketFigureGeometry::componentsAt()), so the
    /// one of the highest display order comes first; Java's click selects the first. Invisible
    /// shapes and hidden components count, as in Java. The pointers are valid until the rocket's
    /// next change event.
    [[nodiscard]] std::vector<RocketComponent*> componentsAt(QPointF pixel);

    /// Drops the geometry and updateFigure(): for a change of the rocket that fired no event.
    void refresh();

    /// The geometry of the figure as it is now (built when it was dropped). Valid until the
    /// rocket's next change event or the next change of the view, the rotation or the selection.
    [[nodiscard]] const RocketFigureGeometry& figureGeometry();
    /// Whether a geometry is kept now (it is dropped at every change and built when needed).
    [[nodiscard]] bool hasFigureGeometry() const noexcept { return m_geometry.has_value(); }

    /// The part of the figure that can be seen, in its pixels (getVisibleRect()): the visible
    /// region's bounds, or the whole widget when nothing of it is on screen (a figure painted
    /// into an image).
    [[nodiscard]] QRect visibleRect() const;

    /// RocketFigure.getAutoZoomPoint(): (max(0, origin x - 30), max(0, -20)).
    [[nodiscard]] QPoint autoZoomPoint() const override;

Q_SIGNALS:
    /// The figure caught up with the rocket's change events of a pass of the event loop: once per
    /// pass, after updateFigure(). The design view re-fits its scroll area here
    /// (ScaleScrollArea::updateScaling(), Java's componentResized in RocketPanel's listener).
    void rocketChanged();

protected:
    /// The subject and the content: the geometry's subject bounds (updateSubjectDimensions()).
    void updateSubjectDimensions() override;
    /// The origin for the view (updateCanvasOrigin(): ScaleFigure::viewOrigin()).
    void updateCanvasOrigin() override;
    /// Paints the rocket and the extras (paintComponent()).
    void paintFigure(QPainter& painter) override;

private:
    /// The rocket's change listener: drops the geometry and asks for flushRocketChanges() once.
    void componentChanged();
    /// The answer to the change events of a pass: updateFigure() and rocketChanged().
    void flushRocketChanges();
    /// Drops the geometry, rebuilt when it is next needed.
    void invalidateGeometry() noexcept { m_geometry.reset(); }
    /// Paints @p elements with the projection as the world transform, at the absolute scale.
    void paintRelativeExtras(QPainter& painter, const std::vector<FigureElement*>& elements,
                             const QRect& visible) const;

    Rocket*                             m_rocket;
    const Preferences*                  m_preferences;
    FigureView                          m_view{kDefaultFigureView};
    double                              m_rotation{0.0};
    std::vector<Uuid>                   m_selection;
    bool                                m_drawCarets{true};
    std::vector<FigureElement*>         m_relativeExtras;
    std::vector<FigureElement*>         m_absoluteExtras;
    std::vector<FigureElement*>         m_relativeTopExtras;
    std::optional<RocketFigureGeometry> m_geometry;
    /// The ShowMarkers preference the geometry was built with.
    bool                                    m_geometryShowMarkers{false};
    bool                                    m_updatePending{false};
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
};

}  // namespace QtRocket::Gui
