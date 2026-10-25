#include "QtRocket/gui/plot/PlotInputBindings.h"

#include <vector>

#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/InputBindings.h>
#include <rocketplot/Legend.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"

// OpenRocket has no JUnit test of its charts' mouse handling. The gestures expected are the table
// of PlotInputBindings.h: JFreeChart's ChartPanel as OpenRocket sets it up
// (ChartPanel.mousePressed() draws the zoom rectangle unless the pan modifier is held,
// MouseWheelHandler zooms both axes about the pointer), read from JFreeChart 1.5.6's classes, in
// rocketplot's gestures.

namespace QtRocket::Gui
{

namespace
{

using PlotInputBindingsTest = Gui::Test::GuiTest;
using rocketplot::Gesture;
using rocketplot::PlotAction;

constexpr int kPlotWidth  = 400;
constexpr int kPlotHeight = 300;
/// One notch of a mouse wheel, away from the user (QWheelEvent::angleDelta()).
constexpr int kWheelNotch = 120;

/// The action of @p gesture with the left button (or none, for the gestures without one) and
/// @p modifiers in @p bindings.
[[nodiscard]] PlotAction leftAction(const rocketplot::InputBindings& bindings, Gesture gesture,
                                    Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const bool withButton =
        gesture == Gesture::DRAG || gesture == Gesture::CLICK || gesture == Gesture::DOUBLE_CLICK;
    return bindings.action(gesture, withButton ? Qt::LeftButton : Qt::NoButton, modifiers);
}

/// Gives @p plot a line from (0, 0) to (10, 100) and its size.
void addLine(rocketplot::PlotWidget& plot)
{
    const std::vector<double> x{0.0, 2.0, 5.0, 10.0};
    const std::vector<double> y{0.0, 40.0, 70.0, 100.0};
    plot.addLine(x, y, rocketplot::literalText(QStringLiteral("Line")));
    plot.resize(kPlotWidth, kPlotHeight);
}

/// Sends @p widget a mouse event of @p type at @p position.
void sendMouse(QWidget& widget, QEvent::Type type, QPointF position, Qt::MouseButton button,
               Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type, position, widget.mapToGlobal(position), button, buttons, modifiers);
    QApplication::sendEvent(&widget, &event);
}

/// Drags the left button over @p widget from @p from to @p to, holding @p modifiers.
void dragLeft(QWidget& widget, QPointF from, QPointF to,
              Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton, modifiers);
    sendMouse(widget, QEvent::MouseMove, (from + to) / 2.0, Qt::NoButton, Qt::LeftButton,
              modifiers);
    sendMouse(widget, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton, modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton, modifiers);
}

/// Double-clicks the left button at @p position of @p widget.
void doubleClickLeft(QWidget& widget, QPointF position)
{
    sendMouse(widget, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton,
              Qt::NoModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton,
              Qt::NoModifier);
    sendMouse(widget, QEvent::MouseButtonDblClick, position, Qt::LeftButton, Qt::LeftButton,
              Qt::NoModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton,
              Qt::NoModifier);
}

/// Turns a mouse wheel (not a trackpad) one notch away from the user at @p position of @p widget.
void turnWheel(QWidget& widget, QPointF position)
{
    QWheelEvent event(position, widget.mapToGlobal(position), QPoint(), QPoint(0, kWheelNotch),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

/// The point at the fractions @p x and @p y of @p area's width and height from its top left.
[[nodiscard]] QPointF pointIn(const QRectF& area, double x, double y)
{
    return {area.left() + (x * area.width()), area.top() + (y * area.height())};
}

/// Whether @p inner lies inside @p outer and is narrower.
[[nodiscard]] bool isZoomedIn(rocketplot::Range inner, rocketplot::Range outer)
{
    return inner.min >= outer.min && inner.max <= outer.max && inner.span() < outer.span();
}

TEST_F(PlotInputBindingsTest, TheChartPanelsDragDrawsTheZoomBox)
{
    const rocketplot::InputBindings bindings = PlotInputBindings::chartPanel();
    EXPECT_EQ(leftAction(bindings, Gesture::DRAG), PlotAction::BOX_ZOOM);
    EXPECT_EQ(leftAction(bindings, Gesture::DRAG, Qt::ShiftModifier), PlotAction::PAN);
    EXPECT_EQ(leftAction(bindings, Gesture::DRAG, Qt::ControlModifier), PlotAction::NONE);
    EXPECT_EQ(leftAction(bindings, Gesture::DOUBLE_CLICK), PlotAction::RESET);
    EXPECT_EQ(leftAction(bindings, Gesture::CLICK), PlotAction::NONE);
    EXPECT_EQ(bindings.action(Gesture::CLICK, Qt::BackButton, Qt::NoModifier), PlotAction::BACK);
    EXPECT_EQ(bindings.action(Gesture::CLICK, Qt::ForwardButton, Qt::NoModifier),
              PlotAction::FORWARD);
    EXPECT_EQ(leftAction(bindings, Gesture::WHEEL), PlotAction::ZOOM);
    EXPECT_EQ(leftAction(bindings, Gesture::WHEEL, Qt::ControlModifier), PlotAction::ZOOM_X);
    EXPECT_EQ(leftAction(bindings, Gesture::WHEEL, Qt::ShiftModifier), PlotAction::ZOOM_Y);
    EXPECT_EQ(leftAction(bindings, Gesture::SCROLL), PlotAction::PAN);
    EXPECT_EQ(leftAction(bindings, Gesture::SCROLL, Qt::ControlModifier), PlotAction::ZOOM);
    EXPECT_EQ(leftAction(bindings, Gesture::PINCH), PlotAction::ZOOM);
    // The right button stays free for the context menu.
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::RightButton, Qt::NoModifier), PlotAction::NONE);
    EXPECT_EQ(bindings.action(Gesture::CLICK, Qt::RightButton, Qt::NoModifier), PlotAction::NONE);
    // rocketplot's defaults, nothing more.
    EXPECT_EQ(bindings.bindings().size(), rocketplot::InputBindings::defaults().bindings().size());
}

TEST_F(PlotInputBindingsTest, NoneBindsNothing)
{
    const rocketplot::InputBindings bindings = PlotInputBindings::none();
    EXPECT_TRUE(bindings.bindings().isEmpty());
    EXPECT_EQ(leftAction(bindings, Gesture::DRAG), PlotAction::NONE);
    EXPECT_EQ(leftAction(bindings, Gesture::DOUBLE_CLICK), PlotAction::NONE);
    EXPECT_EQ(leftAction(bindings, Gesture::WHEEL), PlotAction::NONE);
    EXPECT_EQ(leftAction(bindings, Gesture::PINCH), PlotAction::NONE);
}

TEST_F(PlotInputBindingsTest, ADragZoomsToTheBoxAndADoubleClickRestoresTheRanges)
{
    rocketplot::PlotWidget plot;
    addLine(plot);
    plot.setInputBindings(PlotInputBindings::chartPanel());
    const rocketplot::Range x    = plot.xAxis()->range();
    const rocketplot::Range y    = plot.yAxis()->range();
    const QRectF            area = plot.plotArea();

    dragLeft(plot, pointIn(area, 0.25, 0.25), pointIn(area, 0.75, 0.75));
    EXPECT_TRUE(isZoomedIn(plot.xAxis()->range(), x));
    EXPECT_TRUE(isZoomedIn(plot.yAxis()->range(), y));
    EXPECT_FALSE(plot.xAxis()->autoscale());
    EXPECT_FALSE(plot.yAxis()->autoscale());

    doubleClickLeft(plot, pointIn(area, 0.5, 0.5));
    EXPECT_TRUE(plot.xAxis()->autoscale());
    EXPECT_TRUE(plot.yAxis()->autoscale());
    EXPECT_EQ(plot.xAxis()->range().min, x.min);
    EXPECT_EQ(plot.xAxis()->range().max, x.max);
    EXPECT_EQ(plot.yAxis()->range().min, y.min);
    EXPECT_EQ(plot.yAxis()->range().max, y.max);
}

TEST_F(PlotInputBindingsTest, AShiftDragPans)
{
    rocketplot::PlotWidget plot;
    addLine(plot);
    plot.setInputBindings(PlotInputBindings::chartPanel());
    const rocketplot::Range x    = plot.xAxis()->range();
    const QRectF            area = plot.plotArea();

    // To the right: the view moves left, to smaller values, at the same scale.
    dragLeft(plot, pointIn(area, 0.25, 0.5), pointIn(area, 0.75, 0.5), Qt::ShiftModifier);
    const rocketplot::Range panned = plot.xAxis()->range();
    EXPECT_LT(panned.min, x.min);
    EXPECT_NEAR(panned.span(), x.span(), x.span() * 1e-9);
}

TEST_F(PlotInputBindingsTest, TheWheelZooms)
{
    rocketplot::PlotWidget plot;
    addLine(plot);
    plot.setInputBindings(PlotInputBindings::chartPanel());
    const rocketplot::Range x = plot.xAxis()->range();
    const rocketplot::Range y = plot.yAxis()->range();

    turnWheel(plot, pointIn(plot.plotArea(), 0.5, 0.5));
    EXPECT_TRUE(isZoomedIn(plot.xAxis()->range(), x));
    EXPECT_TRUE(isZoomedIn(plot.yAxis()->range(), y));
}

TEST_F(PlotInputBindingsTest, APlotWithoutInteractionTakesNoGestureAndHasNoMenu)
{
    rocketplot::PlotWidget plot;
    addLine(plot);
    plot.setCrosshairEnabled(true);
    PlotInputBindings::setNoInteraction(plot);
    EXPECT_EQ(plot.inputBindings(), PlotInputBindings::none());
    EXPECT_EQ(plot.contextMenuPolicy(), Qt::NoContextMenu);
    EXPECT_FALSE(plot.legend()->isInteractive());
    EXPECT_FALSE(plot.isCrosshairEnabled());

    const rocketplot::Range x    = plot.xAxis()->range();
    const QRectF            area = plot.plotArea();
    dragLeft(plot, pointIn(area, 0.25, 0.25), pointIn(area, 0.75, 0.75));
    turnWheel(plot, pointIn(area, 0.5, 0.5));
    EXPECT_EQ(plot.xAxis()->range().min, x.min);
    EXPECT_EQ(plot.xAxis()->range().max, x.max);
    EXPECT_TRUE(plot.xAxis()->autoscale());

    const QPoint      middle = area.center().toPoint();
    QContextMenuEvent event(QContextMenuEvent::Mouse, middle, plot.mapToGlobal(middle));
    QApplication::sendEvent(&plot, &event);
    EXPECT_EQ(QApplication::activePopupWidget(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
