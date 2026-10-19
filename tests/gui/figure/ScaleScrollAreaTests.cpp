#include "QtRocket/gui/figure/ScaleScrollArea.h"

#include <algorithm>
#include <string>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/Ruler.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Geometry2D.h"
#include "TestScaleFigure.h"

namespace QtRocket::Gui
{

namespace
{

using ScaleScrollAreaTest = Test::GuiTest;
using Test::TestScaleFigure;

/// The subjects of "A simple model rocket.ork" and "Two stage high power rocket.ork" in the side
/// view (ScaleProbe), and OpenRocket's fit scales for them in 760 x 560 and 1000 x 700 pixels:
/// the fitted view of a scroll pane of 800 x 600 and 1040 x 740.
constexpr Rectangle2D kSimpleSide{.x      = 0.0,
                                  .y      = -0.05589616641898764,
                                  .width  = 0.42539999999999994,
                                  .height = 0.11179233283797528};
constexpr Rectangle2D kTwoStageSide{
    .x = 0.0, .y = -0.2851184274524962, .width = 2.05105, .height = 0.5702368549049924};
constexpr double kSimpleFit760x560   = 0.4353743143415312;
constexpr double kSimpleFit1000x700  = 0.5846455078300562;
constexpr double kTwoStageFit760x560 = 0.09029922884419557;

/// A TestScaleFigure in a ScaleScrollArea of 800 x 600 pixels, shown (offscreen) unless asked
/// not to be.
struct AreaWithFigure
{
    explicit AreaWithFigure(const Rectangle2D& subject = kSimpleSide, bool show = true)
      : figure(makeChild<TestScaleFigure>(&holder, subject, FigureView::SIDE))
    {
        area.resize(800, 600);
        if (show)
        {
            area.show();
        }
    }

    [[nodiscard]] QScrollBar& horizontal() const { return *area.horizontalScrollBar(); }
    [[nodiscard]] QScrollBar& vertical() const { return *area.verticalScrollBar(); }
    /// The figure's point at the viewport's point @p point.
    [[nodiscard]] QPoint figurePoint(QPoint point) const
    {
        return figure->mapFrom(area.viewport(), point);
    }

    QWidget          holder;  // the figure's parent until the area takes it over
    TestScaleFigure* figure;
    ScaleScrollArea  area{figure, nullptr};
};

[[nodiscard]] int clampedTo(const QScrollBar& bar, int value)
{
    return std::clamp(value, bar.minimum(), bar.maximum());
}

TEST_F(ScaleScrollAreaTest, PutsTheRulersAroundTheViewport)
{
    const AreaWithFigure   shown;
    const ScaleScrollArea& area = shown.area;
    EXPECT_EQ(area.widget(), shown.figure);
    EXPECT_EQ(area.figure(), shown.figure);
    EXPECT_TRUE(area.widgetResizable());
    // Viewport margins 20, 20, 0, 0: the viewport starts 20 pixels into the frame's contents.
    EXPECT_EQ(area.viewport()->geometry().topLeft() - area.contentsRect().topLeft(),
              QPoint(20, 20));
    EXPECT_EQ(area.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOn);
    EXPECT_EQ(area.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOn);
    EXPECT_EQ(shown.horizontal().singleStep(), 50);
    EXPECT_EQ(shown.vertical().singleStep(), 50);
    EXPECT_FALSE(area.isFitting());

    const QRect view = area.viewport()->geometry();
    EXPECT_EQ(area.horizontalRuler()->geometry(),
              QRect(view.left(), view.top() - 20, view.width(), 20));
    EXPECT_EQ(area.verticalRuler()->geometry(),
              QRect(view.left() - 20, view.top(), 20, view.height()));
    EXPECT_EQ(area.rulerCornerWidget()->geometry(),
              QRect(view.left() - 20, view.top() - 20, 20, 20));

    // Until run 3 puts the unit selector there: the unit's symbol.
    const auto* label = qobject_cast<const QLabel*>(area.rulerCornerWidget());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(&area.rulerUnit(), &unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
    EXPECT_EQ(label->text(), QtStrings::unitSymbol(area.rulerUnit()));
}

TEST_F(ScaleScrollAreaTest, FitsTheFigureIntoTheAreaLessTheMargin)
{
    AreaWithFigure   shown;
    ScaleScrollArea& area = shown.area;
    EXPECT_EQ(area.fitViewSize(), QSize(760, 560));
    area.setFitting(true);
    EXPECT_TRUE(area.isFitting());
    EXPECT_EQ(area.userScale(), kSimpleFit760x560);
    EXPECT_EQ(area.fitScale(), kSimpleFit760x560);
    EXPECT_EQ(area.relativeScale(), 1.0);
    // The figure is the larger of the viewport and its canvas, scrolled to its auto-zoom point.
    EXPECT_EQ(shown.figure->size(), area.viewport()->size().expandedTo(shown.figure->canvasSize()));
    EXPECT_EQ(shown.horizontal().value(),
              clampedTo(shown.horizontal(), shown.figure->autoZoomPoint().x()));
    EXPECT_EQ(shown.vertical().value(), 0);
}

TEST_F(ScaleScrollAreaTest, FitsAgainWhenResized)
{
    AreaWithFigure shown;
    shown.area.setFitting(true);
    shown.area.resize(1040, 740);
    EXPECT_EQ(shown.area.userScale(), kSimpleFit1000x700);
    // Not fitting: a resize keeps the scale.
    shown.area.setScaling(2.0);
    shown.area.resize(800, 600);
    EXPECT_EQ(shown.area.userScale(), 2.0);
}

TEST_F(ScaleScrollAreaTest, FitsAgainWhenTheFigureChanges)
{
    AreaWithFigure shown;
    shown.area.setFitting(true);
    shown.figure->setSubject(kTwoStageSide);
    EXPECT_EQ(shown.area.userScale(), kTwoStageFit760x560);
}

TEST_F(ScaleScrollAreaTest, UpdateScalingFitsAChangedRocket)
{
    AreaWithFigure shown;
    shown.area.setFitting(true);
    shown.figure->setSubjectQuietly(kTwoStageSide);
    EXPECT_EQ(shown.area.userScale(), kSimpleFit760x560);
    shown.area.updateScaling();
    EXPECT_EQ(shown.area.userScale(), kTwoStageFit760x560);
}

TEST_F(ScaleScrollAreaTest, AnExplicitZoomStopsFittingAndKeepsTheCentre)
{
    AreaWithFigure   shown;
    ScaleScrollArea& area = shown.area;
    area.setFitting(true);
    const QSize view = area.viewport()->size();
    ASSERT_EQ(shown.figure->size(), view);

    // From the fitted view the centre is at half the figure.
    area.setScaling(2.0);
    EXPECT_FALSE(area.isFitting());
    EXPECT_EQ(area.userScale(), 2.0);
    EXPECT_EQ(shown.figure->size(), view.expandedTo(shown.figure->canvasSize()));
    EXPECT_EQ(shown.horizontal().value(),
              clampedTo(shown.horizontal(),
                        static_cast<int>(0.5 * shown.figure->width()) - (view.width() / 2)));
    EXPECT_EQ(shown.vertical().value(),
              clampedTo(shown.vertical(),
                        static_cast<int>(0.5 * shown.figure->height()) - (view.height() / 2)));
    EXPECT_GT(shown.horizontal().value(), 0);

    // From elsewhere: the same fraction of the larger figure.
    shown.horizontal().setValue(300);
    shown.vertical().setValue(100);
    const QPointF fraction((300 + (view.width() / 2.0)) / shown.figure->width(),
                           (100 + (view.height() / 2.0)) / shown.figure->height());
    area.setScaling(3.0);
    EXPECT_EQ(area.userScale(), 3.0);
    EXPECT_EQ(shown.horizontal().value(),
              clampedTo(shown.horizontal(), static_cast<int>(fraction.x() * shown.figure->width()) -
                                                (view.width() / 2)));
    EXPECT_EQ(shown.vertical().value(),
              clampedTo(shown.vertical(), static_cast<int>(fraction.y() * shown.figure->height()) -
                                              (view.height() / 2)));
}

TEST_F(ScaleScrollAreaTest, AZoomWithinOnePercentDoesNothing)
{
    AreaWithFigure shown;
    shown.area.setFitting(true);
    shown.area.setScaling(kSimpleFit760x560 * 1.005);
    EXPECT_TRUE(shown.area.isFitting());
    EXPECT_EQ(shown.area.userScale(), kSimpleFit760x560);
    shown.area.setScaling(2.0);
    shown.area.setScaling(2.01);
    EXPECT_EQ(shown.area.userScale(), 2.0);
    // A scale the figure refuses still ends fitting, as in Java.
    shown.area.setFitting(true);
    shown.area.setScaling(0.0);
    EXPECT_FALSE(shown.area.isFitting());
    EXPECT_EQ(shown.area.userScale(), kSimpleFit760x560);
}

TEST_F(ScaleScrollAreaTest, RelativeScalingIsToTheFit)
{
    AreaWithFigure   shown;
    ScaleScrollArea& area = shown.area;
    area.setFitting(true);
    EXPECT_EQ(area.relativeScale(), 1.0);
    area.setRelativeScaling(1.5);
    EXPECT_FALSE(area.isFitting());
    EXPECT_EQ(area.userScale(), 1.5 * kSimpleFit760x560);
    EXPECT_DOUBLE_EQ(area.relativeScale(), 1.5);

    // Too small to fit anything: the relative scale is 1, and a relative zoom does nothing.
    area.resize(50, 50);
    EXPECT_LT(area.fitScale(), 0);
    EXPECT_EQ(area.relativeScale(), 1.0);
    area.setRelativeScaling(2.0);
    EXPECT_EQ(area.userScale(), 1.5 * kSimpleFit760x560);
}

TEST_F(ScaleScrollAreaTest, AHiddenAreaFitsOnceShown)
{
    AreaWithFigure shown(kSimpleSide, false);
    shown.area.setFitting(true);
    EXPECT_EQ(shown.area.userScale(), kSimpleFit760x560);
    shown.area.resize(1040, 740);
    shown.area.show();
    EXPECT_EQ(shown.area.userScale(), kSimpleFit1000x700);
}

/// Drags in @p shown with @p button from the viewport's point (300, 200) to (250, 170).
void dragInViewport(const AreaWithFigure& shown, Qt::MouseButton button)
{
    QTest::mousePress(shown.figure, button, Qt::NoModifier, shown.figurePoint({300, 200}));
    EXPECT_EQ(shown.area.isPanning(), (shown.area.panButtons() & button) != Qt::NoButton);
    QTest::mouseMove(shown.figure, shown.figurePoint({250, 170}));
    QTest::mouseRelease(shown.figure, button, Qt::NoModifier, shown.figurePoint({250, 170}));
    EXPECT_FALSE(shown.area.isPanning());
}

TEST_F(ScaleScrollAreaTest, TheMiddleAndRightButtonsPan)
{
    AreaWithFigure shown;
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&shown.area));
    shown.area.setScaling(4.0);
    EXPECT_EQ(shown.area.panButtons(), Qt::MiddleButton | Qt::RightButton);

    shown.horizontal().setValue(200);
    shown.vertical().setValue(100);
    dragInViewport(shown, Qt::MiddleButton);
    // The view follows the mouse: the figure moves left and up with it.
    EXPECT_EQ(shown.horizontal().value(), 250);
    EXPECT_EQ(shown.vertical().value(), 130);

    dragInViewport(shown, Qt::RightButton);
    EXPECT_EQ(shown.horizontal().value(), 300);
    EXPECT_EQ(shown.vertical().value(), 160);

    // The left button does not pan, unless told to (and nothing takes its events).
    {
        const Gui::Test::ExpectedQtMessage unaccepted(
            QStringLiteral("not accepted by receiving widget"));
        dragInViewport(shown, Qt::LeftButton);
    }
    EXPECT_EQ(shown.horizontal().value(), 300);
    EXPECT_EQ(shown.vertical().value(), 160);
    shown.area.setPanButtons(Qt::LeftButton);
    dragInViewport(shown, Qt::LeftButton);
    EXPECT_EQ(shown.horizontal().value(), 350);
    EXPECT_EQ(shown.vertical().value(), 190);
}

TEST_F(ScaleScrollAreaTest, PanningStopsAtTheEdges)
{
    AreaWithFigure shown;
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&shown.area));
    shown.area.setScaling(4.0);
    shown.horizontal().setValue(shown.horizontal().maximum() - 10);
    shown.vertical().setValue(0);
    // Dragging up and left moves the view right and down: 10 pixels are left to the right.
    dragInViewport(shown, Qt::MiddleButton);
    EXPECT_EQ(shown.horizontal().value(), shown.horizontal().maximum());
    EXPECT_EQ(shown.vertical().value(), 30);
}

TEST_F(ScaleScrollAreaTest, TheWheelScrolls)
{
    AreaWithFigure shown;
    shown.area.setScaling(4.0);
    shown.vertical().setValue(0);
    // The figure leaves the wheel to its parent, the viewport (Qt passes a spontaneous event on),
    // whose scroll area scrolls by the wheel's lines of 50 pixels.
    const QPoint onFigure = shown.figurePoint({300, 200});
    QWheelEvent  figureWheel(QPointF(onFigure), QPointF(shown.figure->mapToGlobal(onFigure)),
                             QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                             Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(shown.figure, &figureWheel);
    EXPECT_FALSE(figureWheel.isAccepted());

    const QPoint onViewport(300, 200);
    QWheelEvent  viewportWheel(
        QPointF(onViewport), QPointF(shown.area.viewport()->mapToGlobal(onViewport)), QPoint(),
        QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(shown.area.viewport(), &viewportWheel);
    EXPECT_EQ(shown.vertical().value(),
              std::min(QApplication::wheelScrollLines() * ScaleScrollArea::kScrollStep,
                       shown.vertical().pageStep()));
}

TEST_F(ScaleScrollAreaTest, TheRulerUnitCanBeChanged)
{
    AreaWithFigure   shown;
    ScaleScrollArea& area = shown.area;
    const QSignalSpy changes(&area, &ScaleScrollArea::rulerUnitChanged);
    const Unit*      millimetre = unitGroup(UnitGroupId::LENGTH).getUnit("mm");
    ASSERT_NE(millimetre, nullptr);
    area.setRulerUnit(*millimetre);
    area.setRulerUnit(*millimetre);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(&area.rulerUnit(), millimetre);
    EXPECT_EQ(&area.horizontalRuler()->unit(), millimetre);
    EXPECT_EQ(&area.verticalRuler()->unit(), millimetre);
    const auto* label = qobject_cast<const QLabel*>(area.rulerCornerWidget());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), QStringLiteral("mm"));

    // updateRulerUnit(): the default length unit (after the preferences changed it).
    UnitGroup&        lengths  = unitGroup(UnitGroupId::LENGTH);
    const std::string previous = lengths.getDefaultUnit().getUnit();
    ASSERT_TRUE(lengths.setDefaultUnit("in"));
    area.updateRulerUnit();
    EXPECT_EQ(area.rulerUnit().getUnit(), "in");
    EXPECT_EQ(label->text(), QStringLiteral("in"));
    EXPECT_EQ(changes.count(), 2);
    EXPECT_TRUE(lengths.setDefaultUnit(previous));
}

TEST_F(ScaleScrollAreaTest, TheCornerTakesAnotherWidget)
{
    AreaWithFigure          shown;
    ScaleScrollArea&        area     = shown.area;
    const QPointer<QWidget> previous = area.rulerCornerWidget();
    auto*                   selector = makeChild<QLabel>(&shown.holder, QStringLiteral("unit"));
    area.setRulerCornerWidget(selector);
    EXPECT_EQ(area.rulerCornerWidget(), selector);
    EXPECT_EQ(selector->parentWidget(), &area);
    EXPECT_TRUE(selector->isVisible());
    const QRect view = area.viewport()->geometry();
    EXPECT_EQ(selector->geometry(), QRect(view.left() - 20, view.top() - 20, 20, 20));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(previous.isNull());

    // A unit change no longer writes into the corner.
    const Unit* metre = unitGroup(UnitGroupId::LENGTH).getUnit("m");
    ASSERT_NE(metre, nullptr);
    area.setRulerUnit(*metre);
    EXPECT_EQ(selector->text(), QStringLiteral("unit"));
}

}  // namespace

}  // namespace QtRocket::Gui
