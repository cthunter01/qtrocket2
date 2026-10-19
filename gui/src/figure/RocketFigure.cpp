#include "QtRocket/gui/figure/RocketFigure.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include <QMetaObject>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRegion>
#include <QString>
#include <QTransform>
#include <QWidget>
#include <Qt>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/gui/figure/FigureElement.h"
#include "QtRocket/gui/figure/RocketFigurePainter.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// Removes every occurrence of @p element from @p elements (Java's ArrayList.remove() removes the
/// first; an element is normally added once).
void removeElement(std::vector<FigureElement*>& elements, const FigureElement& element)
{
    std::erase(elements, &element);
}

}  // namespace

RocketFigure::RocketFigure(Rocket& rocket, const Preferences& preferences, QWidget* parent)
  : RocketFigure(rocket, preferences, dotsPerInch(preferences), parent)
{
}

RocketFigure::RocketFigure(Rocket& rocket, const Preferences& preferences, double dotsPerInch,
                           QWidget* parent)
  : ScaleFigure(dotsPerInch, parent), m_rocket(&rocket), m_preferences(&preferences)
{
    m_rocketConnection = rocket.addComponentChangeListener(
        guarded([this] { componentChanged(); }, QStringLiteral("Rocket figure")));
    updateFigure();
}

void RocketFigure::setView(FigureView view)
{
    if (m_view == view)
    {
        return;
    }
    m_view = view;
    invalidateGeometry();
    updateFigure();
    fireChangeEvent();
}

double RocketFigure::rotation(bool applyTopViewOffset) const noexcept
{
    return applyTopViewOffset ? rotationWithTopViewOffset(m_view, m_rotation) : m_rotation;
}

void RocketFigure::setRotation(double rotation)
{
    // Deviation: a rotation that is not finite is ignored (class comment).
    if (!std::isfinite(rotation) || MathUtil::equals(m_rotation, rotation))
    {
        return;
    }
    m_rotation = rotation;
    invalidateGeometry();
    updateFigure();
    fireChangeEvent();
}

void RocketFigure::setSelection(std::span<const RocketComponent* const> components)
{
    std::vector<Uuid> selection;
    selection.reserve(components.size());
    for (const RocketComponent* component : components)
    {
        if (component != nullptr)
        {
            selection.push_back(component->getId());
        }
    }
    m_selection = std::move(selection);
    // The markers of pod sets and booster sets may depend on the selection (ShowMarkers).
    invalidateGeometry();
    updateFigure();
    fireChangeEvent();
}

bool RocketFigure::isSelected(const RocketComponent& component) const noexcept
{
    return std::ranges::find(m_selection, component.getId()) != m_selection.end();
}

void RocketFigure::setDrawCarets(bool drawCarets)
{
    m_drawCarets = drawCarets;
    update();
}

void RocketFigure::addRelativeExtra(FigureElement& element)
{
    m_relativeExtras.push_back(&element);
}

void RocketFigure::removeRelativeExtra(const FigureElement& element)
{
    removeElement(m_relativeExtras, element);
}

void RocketFigure::clearRelativeExtra() noexcept
{
    m_relativeExtras.clear();
}

void RocketFigure::addAbsoluteExtra(FigureElement& element)
{
    m_absoluteExtras.push_back(&element);
}

void RocketFigure::removeAbsoluteExtra(const FigureElement& element)
{
    removeElement(m_absoluteExtras, element);
}

void RocketFigure::clearAbsoluteExtra() noexcept
{
    m_absoluteExtras.clear();
}

void RocketFigure::addRelativeTopExtra(FigureElement& element)
{
    m_relativeTopExtras.push_back(&element);
}

void RocketFigure::removeRelativeTopExtra(const FigureElement& element)
{
    removeElement(m_relativeTopExtras, element);
}

void RocketFigure::clearRelativeTopExtra() noexcept
{
    m_relativeTopExtras.clear();
}

std::vector<RocketComponent*> RocketFigure::componentsAt(QPointF pixel)
{
    // Calculate point in shapes' coordinates
    bool             invertible = false;
    const QTransform inverse    = projection().inverted(&invertible);
    if (!invertible)
    {
        return {};
    }
    const QPointF point = inverse.map(pixel);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
    {
        return {};
    }

    // The rocket's own, mutable objects of the components the geometry observes.
    std::vector<RocketComponent*> components;
    for (const RocketComponent* component :
         figureGeometry().componentsAt(Point2D{point.x(), point.y()}))
    {
        components.push_back(m_rocket->findComponent(component->getId()));
    }
    std::erase(components, nullptr);
    return components;
}

void RocketFigure::refresh()
{
    invalidateGeometry();
    updateFigure();
}

const RocketFigureGeometry& RocketFigure::figureGeometry()
{
    const bool showMarkers = m_preferences->isShowMarkers();
    if (m_geometry.has_value() && m_geometryShowMarkers == showMarkers)
    {
        return *m_geometry;
    }

    // The selected components, for the markers of the selected pod sets and booster sets.
    std::vector<const RocketComponent*> selected;
    for (const Uuid& id : m_selection)
    {
        if (const RocketComponent* component = std::as_const(*m_rocket).findComponent(id);
            component != nullptr)
        {
            selected.push_back(component);
        }
    }
    m_geometry = RocketFigureGeometry::build(std::as_const(*m_rocket).getSelectedConfiguration(),
                                             m_view, m_rotation, selected, showMarkers);
    m_geometryShowMarkers = showMarkers;
    return *m_geometry;
}

QRect RocketFigure::visibleRect() const
{
    const QRect visible = visibleRegion().boundingRect();
    return visible.isEmpty() ? rect() : visible;
}

QPoint RocketFigure::autoZoomPoint() const
{
    return {std::max(0, subjectOrigin().x() - kBorderWidth), std::max(0, -kBorderHeight)};
}

void RocketFigure::updateSubjectDimensions()
{
    // for a rocket, these are the same
    const Rectangle2D& subject = figureGeometry().subjectBounds();
    setSubjectBounds(subject, subject);
}

void RocketFigure::updateCanvasOrigin()
{
    setSubjectOrigin(viewOrigin(m_view, size(), subjectBounds(), absoluteScale()));
}

void RocketFigure::paintFigure(QPainter& painter)
{
    // Java's paintComponent() brings everything up to date first; the canvas size is left to
    // updateFigure() (class comment).
    updateSubjectDimensions();
    updateCanvasOrigin();
    updateTransform();

    const RocketFigurePainter rocketPainter(*m_preferences, palette());
    rocketPainter.paint(painter, figureGeometry(), projection(), m_selection);

    // Draw relative extras
    const QRect visible = visibleRect();
    if (m_drawCarets)
    {
        paintRelativeExtras(painter, m_relativeExtras, visible);
    }

    // Draw absolute extras
    for (FigureElement* element : m_absoluteExtras)
    {
        painter.save();
        element->paint(painter, 1.0, visible);
        painter.restore();
    }

    // Draw relative top extras (on top of everything, using the rocket transform)
    if (m_drawCarets)
    {
        paintRelativeExtras(painter, m_relativeTopExtras, visible);
    }
}

void RocketFigure::componentChanged()
{
    // The components the geometry observes may be gone once this event is delivered.
    invalidateGeometry();
    if (m_updatePending)
    {
        return;
    }
    m_updatePending = true;
    QMetaObject::invokeMethod(
        this,
        [this] { Guard::run([this] { flushRocketChanges(); }, QStringLiteral("Rocket figure")); },
        Qt::QueuedConnection);
}

void RocketFigure::flushRocketChanges()
{
    m_updatePending = false;
    updateFigure();
    Q_EMIT rocketChanged();
}

void RocketFigure::paintRelativeExtras(QPainter&                          painter,
                                       const std::vector<FigureElement*>& elements,
                                       const QRect&                       visible) const
{
    for (FigureElement* element : elements)
    {
        painter.save();
        painter.setWorldTransform(projection(), true);
        element->paint(painter, absoluteScale(), visible);
        painter.restore();
    }
}

}  // namespace QtRocket::Gui
