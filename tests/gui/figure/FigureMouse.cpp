#include "figure/FigureMouse.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QTransform>
#include <QWidget>
#include <Qt>

#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The pixels between two moves of a drag.
constexpr int kDragStep = 5;
/// The points tried across each of a shape's bounds (figurePixelOf()).
constexpr int kSearchSteps = 15;

}  // namespace

bool sendMouse(QWidget& widget, QEvent::Type type, QPoint position, Qt::MouseButton button,
               Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    const QPointF local(position);
    QMouseEvent   event(type, local, widget.mapToGlobal(local), button, buttons, modifiers);
    QApplication::sendEvent(&widget, &event);
    return event.isAccepted();
}

void clickAt(QWidget& widget, QPoint position, Qt::MouseButton button,
             Qt::KeyboardModifiers modifiers)
{
    sendMouse(widget, QEvent::MouseButtonPress, position, button, button, modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, position, button, Qt::NoButton, modifiers);
}

void secondClickAt(QWidget& widget, QPoint position, Qt::MouseButton button,
                   Qt::KeyboardModifiers modifiers)
{
    sendMouse(widget, QEvent::MouseButtonDblClick, position, button, button, modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, position, button, Qt::NoButton, modifiers);
}

void dragFrom(QWidget& widget, QPoint from, QPoint to, Qt::MouseButton button,
              Qt::KeyboardModifiers modifiers)
{
    // The mouse moves over the widget's parent (the scroll area's viewport), under which the
    // widget moves while the view pans: each position is the widget's when it is sent.
    sendMouse(widget, QEvent::MouseButtonPress, widget.mapFromParent(from), button, button,
              modifiers);
    const QPoint delta = to - from;
    const int steps = std::max(1, std::max(std::abs(delta.x()), std::abs(delta.y())) / kDragStep);
    for (int step = 1; step <= steps; ++step)
    {
        const QPoint position = from + ((delta * step) / steps);
        sendMouse(widget, QEvent::MouseMove, widget.mapFromParent(position), Qt::NoButton, button,
                  modifiers);
    }
    sendMouse(widget, QEvent::MouseButtonRelease, widget.mapFromParent(to), button, Qt::NoButton,
              modifiers);
}

std::optional<QPoint> figurePixelOf(RocketFigure& figure, const RocketComponent& component)
{
    // The bounds first: a hit test may build the geometry again.
    std::vector<Rectangle2D> bounds;
    for (const RocketFigureGeometry::InstanceShape& shape : figure.figureGeometry().shapes())
    {
        if (&shape.shape.component() == &component)
        {
            bounds.push_back(shape.shape.shape().bounds());
        }
    }
    for (const Rectangle2D& box : bounds)
    {
        for (int i = 0; i < kSearchSteps; ++i)
        {
            for (int j = 0; j < kSearchSteps; ++j)
            {
                const QPointF model(box.x + (box.width * (i + 0.5) / kSearchSteps),
                                    box.y + (box.height * (j + 0.5) / kSearchSteps));
                const QPoint  pixel = figure.projection().map(model).toPoint();
                const std::vector<RocketComponent*> found = figure.componentsAt(QPointF(pixel));
                if (!found.empty() && found.front() == &component)
                {
                    return pixel;
                }
            }
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket::Gui::Test
