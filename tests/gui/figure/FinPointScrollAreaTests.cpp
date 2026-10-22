#include "QtRocket/gui/figure/FinPointScrollArea.h"

#include <cstddef>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRegion>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSize>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "FinPointTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointFigure.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "RecordingPaintDevice.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FinPointScrollAreaTest = Test::GuiTest;
using Test::finPointsText;
using Test::FreeformDesign;
using Test::RecordingPaintDevice;
using Test::RocketEvents;
using Test::showActive;

/// The editor's figure in its area, 600 x 400 pixels at 96 dots per inch and fitted
/// (FreeformPanelProbe's), not shown; the live update preference as asked.
struct Editor
{
    explicit Editor(const FinPointSelection& selection, bool live = false)
      : figure(makeChild<FinPointFigure>(&holder, selection, 96.0))
    {
        preferences.putBoolean(GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, live);
        area.resize(600, 400);
        area.setFitting(true);
    }

    /// The figure's pixel of the point (@p x, @p y) of the fin frame (the probe's modelPixel()).
    [[nodiscard]] QPoint pixel(double x, double y) const
    {
        const QPoint origin = figure->subjectOrigin();
        const double scale  = figure->absoluteScale();
        return {origin.x() + static_cast<int>(MathUtil::javaRound(x * scale)),
                origin.y() - static_cast<int>(MathUtil::javaRound(y * scale))};
    }

    /// The pixel of the fin point @p index.
    [[nodiscard]] QPoint pointPixel(std::size_t index) const
    {
        const Coordinate c = figure->target().main()->getFinPoints().at(index);
        return pixel(c.x, c.y);
    }

    void press(QPoint at, Qt::MouseButton button = Qt::LeftButton,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier) const
    {
        QTest::mousePress(figure, button, modifiers, at);
    }

    /// A move to @p at with the left button held and @p modifiers.
    void move(QPoint at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) const
    {
        QMouseEvent event(QEvent::MouseMove, QPointF(at), QPointF(figure->mapToGlobal(at)),
                          Qt::NoButton, Qt::LeftButton, modifiers);
        QApplication::sendEvent(figure, &event);
    }

    void release(QPoint at, Qt::MouseButton button = Qt::LeftButton) const
    {
        QTest::mouseRelease(figure, button, Qt::NoModifier, at);
    }

    QWidget             holder;  // the figure's parent until the area takes it over
    InMemoryPreferences preferences;
    FinPointFigure*     figure;
    FinPointScrollArea  area{figure, preferences, nullptr};
};

TEST_F(FinPointScrollAreaTest, APressOnAPointDragsItInAFrozenRocket)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    const RocketEvents   events(design.rocket());
    const QSignalSpy     refreshes(&editor.area, &FinPointScrollArea::refreshRequested);
    const QSignalSpy     editStates(&editor.area, &FinPointScrollArea::editStateChanged);

    editor.press(editor.pointPixel(1));
    EXPECT_TRUE(editor.area.isDragging());
    EXPECT_EQ(editor.area.dragIndex(), 1);
    EXPECT_TRUE(design.rocket().isFrozen());
    EXPECT_TRUE(editor.area.isRocketFrozenForDrag());
    EXPECT_TRUE(editor.figure->editHistory().isEditInProgress());
    EXPECT_EQ(refreshes.count(), 1);
    EXPECT_EQ(editStates.count(), 1);

    editor.move(editor.pixel(0.03, 0.06));
    EXPECT_EQ(finPointsText(design.fins()),
              "(0.0,0.0) (0.030000000000000006,0.06000000000000001) (0.075,0.05) "
              "(0.04999999999999999,0.0)");
    EXPECT_EQ(events.count(), 0U);
    EXPECT_EQ(refreshes.count(), 2);

    editor.release(editor.pixel(0.03, 0.06));
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kAeromassChange});
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
    EXPECT_EQ(editStates.count(), 2);
}

TEST_F(FinPointScrollAreaTest, TheLiveUpdateDragsWithoutAFreeze)
{
    const FreeformDesign design;
    const Editor         editor(design.selection(), true);
    const RocketEvents   events(design.rocket());
    editor.press(editor.pointPixel(1));
    EXPECT_FALSE(design.rocket().isFrozen());
    editor.move(editor.pixel(0.03, 0.06));
    editor.move(editor.pixel(0.035, 0.065));
    EXPECT_EQ(events.count(), 2U);
    editor.release(editor.pixel(0.035, 0.065));
    EXPECT_EQ(events.count(), 2U);
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
}

TEST_F(FinPointScrollAreaTest, APressOnASegmentInsertsAPointThere)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.press(editor.pixel(0.05, 0.05));
    EXPECT_EQ(editor.area.dragIndex(), 2);
    EXPECT_EQ(design.fins().getPointCount(), 5U);
    EXPECT_DOUBLE_EQ(design.fins().getFinPoints().at(2).y, 0.05);
    editor.move(editor.pixel(0.05, 0.07));
    editor.release(editor.pixel(0.05, 0.07));
    EXPECT_DOUBLE_EQ(design.fins().getFinPoints().at(2).y, 0.07);
    // One edit of the history: the point inserted and dragged.
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
}

TEST_F(FinPointScrollAreaTest, APressBesideTheOutlinePansAndDeselects)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.figure->setSelectedIndex(2);
    const QSignalSpy cleared(&editor.area, &FinPointScrollArea::pointSelectionCleared);
    editor.press(editor.pixel(0.2, 0.2));
    EXPECT_EQ(editor.figure->selectedIndex(), -1);
    EXPECT_EQ(cleared.count(), 1);
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_TRUE(editor.area.isPanning());
    editor.release(editor.pixel(0.2, 0.2));
    EXPECT_FALSE(editor.area.isPanning());
    EXPECT_EQ(editor.figure->editHistory().size(), 1U);
}

TEST_F(FinPointScrollAreaTest, ShiftLocksTheAngleToThePreviousPoint)
{
    // FreeformPanelProbe's "body tube drags": point 2 locked to point 1 (3.5, 6.5) cm.
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.press(editor.pointPixel(1));
    editor.move(editor.pixel(0.035, 0.065));
    editor.release(editor.pixel(0.035, 0.065));
    editor.press(editor.pointPixel(2));
    editor.move(editor.pixel(0.08, 0.052), Qt::ShiftModifier);
    EXPECT_EQ(design.fins().getFinPoints().at(2).y, 0.06500000000000002);
    EXPECT_EQ(editor.figure->highlightIndex(), 1);
    editor.move(editor.pixel(0.0355, 0.09), Qt::ShiftModifier);
    EXPECT_EQ(design.fins().getFinPoints().at(2).x, 0.035);
    editor.move(editor.pixel(0.06, 0.075));
    EXPECT_EQ(editor.figure->highlightIndex(), -1);
    editor.release(editor.pixel(0.06, 0.075));
}

TEST_F(FinPointScrollAreaTest, CtrlShiftLocksTheAngleToTheNextPoint)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.press(editor.pointPixel(1));
    EXPECT_EQ(editor.area.lockIndex(Qt::ShiftModifier), 0);
    EXPECT_EQ(editor.area.lockIndex(Qt::ShiftModifier | Qt::ControlModifier), 2);
    EXPECT_EQ(editor.area.highlightIndexFor(2), 1);
    EXPECT_EQ(editor.area.highlightIndexFor(0), 0);
    // Nearest the vertical through point 2 (7.5, 5) cm: x snaps to it.
    editor.move(editor.pixel(0.071, 0.065), Qt::ShiftModifier | Qt::ControlModifier);
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.075);
    EXPECT_EQ(editor.figure->highlightIndex(), 1);
    editor.release(editor.pixel(0.071, 0.065));
    // The first and the last point lock to nothing.
    editor.press(editor.pointPixel(0));
    EXPECT_EQ(editor.area.lockIndex(Qt::ShiftModifier), -1);
    editor.release(editor.pointPixel(0));
}

TEST_F(FinPointScrollAreaTest, SnapsToTheNearestOfThreeLines)
{
    const Point2D lock{0.0, 0.0};
    // Nearer the vertical through the lock point: x snaps.
    const Point2D vertical = FinPointScrollArea::snapPoint({0.01, 0.05}, lock);
    EXPECT_EQ(vertical.x, 0.0);
    EXPECT_EQ(vertical.y, 0.05);
    // Nearer the horizontal: y snaps.
    const Point2D horizontal = FinPointScrollArea::snapPoint({0.05, -0.01}, lock);
    EXPECT_EQ(horizontal.x, 0.05);
    EXPECT_EQ(horizontal.y, 0.0);
    // Nearer the diagonal: both the mean distance.
    const Point2D diagonal = FinPointScrollArea::snapPoint({0.04, -0.05}, lock);
    EXPECT_EQ(diagonal.x, 0.045);
    EXPECT_EQ(diagonal.y, -0.045);
    // On the lock point's vertical (dx 0): x stays.
    const Point2D onAxis = FinPointScrollArea::snapPoint({0.0, 0.05}, lock);
    EXPECT_EQ(onAxis.x, 0.0);
    EXPECT_EQ(onAxis.y, 0.05);
}

TEST_F(FinPointScrollAreaTest, CtrlClickDeletesAPointAfterTheFirst)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    QTest::mouseClick(editor.figure, Qt::LeftButton, Qt::ControlModifier, editor.pointPixel(2));
    EXPECT_EQ(design.fins().getPointCount(), 3U);
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
    QTest::mouseClick(editor.figure, Qt::LeftButton, Qt::ControlModifier, editor.pointPixel(0));
    EXPECT_EQ(design.fins().getPointCount(), 3U);
    // A click without Ctrl deletes nothing.
    QTest::mouseClick(editor.figure, Qt::LeftButton, Qt::NoModifier, editor.pointPixel(1));
    EXPECT_EQ(design.fins().getPointCount(), 3U);
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
}

TEST_F(FinPointScrollAreaTest, ARightClickOnAPointAsksForTheMenu)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    const QSignalSpy     menu(&editor.area, &FinPointScrollArea::pointMenuRequested);
    QTest::mouseClick(editor.figure, Qt::RightButton, Qt::NoModifier, editor.pointPixel(1));
    ASSERT_EQ(menu.count(), 1);
    EXPECT_EQ(menu.at(0).at(0).toInt(), 1);
    QTest::mouseClick(editor.figure, Qt::RightButton, Qt::NoModifier, editor.pixel(0.2, 0.2));
    EXPECT_EQ(menu.count(), 1);
    // The right button only (Java: BUTTON3), not the middle one.
    QTest::mouseClick(editor.figure, Qt::MiddleButton, Qt::NoModifier, editor.pointPixel(1));
    EXPECT_EQ(menu.count(), 1);
    // A press and a release elsewhere are no click.
    editor.press(editor.pointPixel(1), Qt::RightButton);
    editor.move(editor.pixel(0.03, 0.05));
    editor.release(editor.pixel(0.03, 0.05), Qt::RightButton);
    EXPECT_EQ(menu.count(), 1);
}

TEST_F(FinPointScrollAreaTest, ASecondPressDuringADragDoesNothing)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.press(editor.pointPixel(1));
    editor.press(editor.pointPixel(2), Qt::RightButton);
    EXPECT_EQ(editor.area.dragIndex(), 1);
    EXPECT_TRUE(design.rocket().isFrozen());
    editor.release(editor.pointPixel(2), Qt::RightButton);
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
}

TEST_F(FinPointScrollAreaTest, HidingTheAreaEndsTheDrag)
{
    const FreeformDesign design;
    Editor               editor(design.selection());
    editor.area.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&editor.area));
    editor.press(editor.pointPixel(1));
    editor.move(editor.pixel(0.03, 0.06));
    ASSERT_TRUE(design.rocket().isFrozen());
    editor.area.hide();
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
}

TEST_F(FinPointScrollAreaTest, ADeactivatedWindowEndsTheDrag)
{
    // A release that never comes (the mouse lost to another window): the rocket is not left
    // frozen while another window works on it.
    const FreeformDesign design;
    Editor               editor(design.selection());
    ASSERT_TRUE(showActive(editor.area));
    editor.press(editor.pointPixel(1));
    editor.move(editor.pixel(0.03, 0.06));
    ASSERT_TRUE(design.rocket().isFrozen());
    QWidget other;
    ASSERT_TRUE(showActive(other));
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    EXPECT_FALSE(editor.figure->editHistory().isEditInProgress());
    EXPECT_EQ(editor.figure->editHistory().size(), 2U);
    editor.release(editor.pixel(0.03, 0.06));
}

TEST_F(FinPointScrollAreaTest, AnActivationChangeAwayEndsTheDrag)
{
    // Every widget of a window hears its activation change; the area, not the active window's,
    // ends the drag (also without the focus, which a click on a point takes in an active window).
    const FreeformDesign design;
    Editor               editor(design.selection());
    editor.press(editor.pointPixel(1));
    ASSERT_FALSE(editor.area.isActiveWindow());
    QEvent change(QEvent::ActivationChange);
    QApplication::sendEvent(&editor.area, &change);
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    editor.release(editor.pointPixel(1));
}

TEST_F(FinPointScrollAreaTest, LosingTheFocusEndsTheDragButToAMenu)
{
    const FreeformDesign design;
    Editor               editor(design.selection());
    editor.press(editor.pointPixel(1));
    // A menu that opens (the right click's popup) takes the focus for a while.
    QFocusEvent toMenu(QEvent::FocusOut, Qt::PopupFocusReason);
    QApplication::sendEvent(&editor.area, &toMenu);
    EXPECT_TRUE(editor.area.isDragging());
    EXPECT_TRUE(design.rocket().isFrozen());
    QFocusEvent away(QEvent::FocusOut, Qt::TabFocusReason);
    QApplication::sendEvent(&editor.area, &away);
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    editor.release(editor.pointPixel(1));
}

TEST_F(FinPointScrollAreaTest, AMoveWithoutTheButtonEndsTheDrag)
{
    // The button that began the drag is no longer held: its release went elsewhere. (Without
    // any button held, Qt delivers no move to a widget that does not track the mouse: then the
    // next press and release end the drag.)
    const FreeformDesign design;
    const Editor         editor(design.selection());
    editor.press(editor.pointPixel(1));
    const QPoint at = editor.pixel(0.03, 0.06);
    QMouseEvent  move(QEvent::MouseMove, QPointF(at), QPointF(editor.figure->mapToGlobal(at)),
                      Qt::NoButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(editor.figure, &move);
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    EXPECT_EQ(finPointsText(design.fins()),
              "(0.0,0.0) (0.025,0.05) (0.075,0.05) (0.04999999999999999,0.0)");
    editor.release(at);
}

TEST_F(FinPointScrollAreaTest, DestroyingTheAreaThawsTheRocket)
{
    const FreeformDesign design;
    {
        const Editor editor(design.selection());
        // Sent, not QTest's: QTest would remember the left button as held after the test.
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(editor.pointPixel(1)),
                          QPointF(editor.figure->mapToGlobal(editor.pointPixel(1))), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(editor.figure, &press);
        ASSERT_TRUE(design.rocket().isFrozen());
    }
    EXPECT_FALSE(design.rocket().isFrozen());
}

/// Moves the point 1 of @p design as an undo step "Move".
void movedAsAnUndoStep(const FreeformDesign& design)
{
    design.document().addUndoPosition("Move");
    EXPECT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    design.document().addUndoPosition("After");
}

TEST_F(FinPointScrollAreaTest, AnUndoDuringAFrozenDragEndsItAtTheNextMove)
{
    // The undo replaces every component while its event waits in the drag's freeze: the drag
    // must not touch the fin set it began on (which is gone), nor the new one.
    const FreeformDesign design;
    movedAsAnUndoStep(design);
    const Editor editor(design.selection());
    editor.press(editor.pointPixel(1));
    ASSERT_TRUE(design.rocket().isFrozen());
    const RocketEvents events(design.rocket());
    design.document().undo();
    // A paint meanwhile reads the new fin set.
    RecordingPaintDevice device(QSize(100, 100));
    editor.figure->render(&device, QPoint(), QRegion(QRect(0, 0, 100, 100)), {});

    editor.move(editor.pixel(0.04, 0.07));
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_FALSE(design.rocket().isFrozen());
    ASSERT_FALSE(events.types().empty());
    EXPECT_NE(events.types().back() & ComponentChangeEvent::kUndoChange, 0);
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.025);
    editor.release(editor.pixel(0.04, 0.07));
}

TEST_F(FinPointScrollAreaTest, AnUndoDuringALiveDragEndsItAtTheNextMove)
{
    const FreeformDesign design;
    movedAsAnUndoStep(design);
    const Editor editor(design.selection(), true);
    editor.press(editor.pointPixel(1));
    design.document().undo();
    editor.move(editor.pixel(0.04, 0.07));
    EXPECT_FALSE(editor.area.isDragging());
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.025);
    editor.release(editor.pixel(0.04, 0.07));
    EXPECT_FALSE(design.rocket().isFrozen());
}

TEST_F(FinPointScrollAreaTest, TheCornerSelectorSetsTheRulersUnit)
{
    const FreeformDesign design;
    const Editor         editor(design.selection());
    const auto* selector = qobject_cast<const UnitSelector*>(editor.area.rulerCornerWidget());
    ASSERT_NE(selector, nullptr);
    const Unit* inches = unitGroup(UnitGroupId::LENGTH).getUnit("in");
    ASSERT_NE(inches, nullptr);
    DoubleBinding& unit = *selector->binding();
    unit.setCurrentUnit(*inches);
    EXPECT_EQ(&editor.area.rulerUnit(), inches);
    EXPECT_EQ(&editor.figure->gridUnit(), inches);
}

TEST_F(FinPointScrollAreaTest, ADragAtTheViewsEdgeScrollsAndEndsFitting)
{
    // handleScrolling(): within the figure, a mouse less than the border from the view's edge
    // scrolls the view by as much and ends fitting.
    const FreeformDesign design;
    Editor               editor(design.selection());
    editor.area.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&editor.area));
    editor.area.setScaling(4 * editor.area.userScale());
    const QSignalSpy scrolled(&editor.area, &FinPointScrollArea::autoScrolled);
    editor.press(editor.pointPixel(1));
    const int    before = editor.area.horizontalScrollBar()->value();
    const QPoint edge   = editor.figure->mapFrom(editor.area.viewport(), QPoint(5, 100));
    editor.move(edge);
    EXPECT_EQ(scrolled.count(), 1);
    EXPECT_FALSE(editor.area.isFitting());
    EXPECT_EQ(editor.area.horizontalScrollBar()->value(), before - 25);
    editor.release(edge);
}

}  // namespace

}  // namespace QtRocket::Gui
