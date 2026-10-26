// The two-thumb slider of the motor chooser's filter panel (Java: MultiSlider and MultiSliderUI).
//
// Expectations marked "probe" are OpenRocket's own, printed by tools/openrocket-probes/multi-slider
// (MultiSliderProbe -> expected.txt, OpenRocket 5f164fd0e, JDK 17): its MultiSlider made as
// MotorFilterPanel makes it, under FlatLightLaf, 300 pixels wide. Its pixels are Swing's: the tests
// put the mouse where this slider draws a value (positionForValue(), thumbRect()), and compare the
// values, the current thumb and the change events, which the probe prints as "v0,v1
// adjusting=a0,a1" after each event.

#include "QtRocket/gui/widgets/RangeSlider.h"

#include <cstddef>
#include <cstdlib>
#include <format>
#include <map>
#include <string>
#include <vector>

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QWidget>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/motor/chooser/MotorFilterSliders.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "WidgetTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using RangeSliderTest = Test::GuiTest;
using Test::showActiveWith;
using Test::turnWheel;

/// The probe's width of the sliders.
constexpr int kProbeWidth = 300;

/// The change events of a slider, as the probe prints them.
class Changes
{
public:
    explicit Changes(RangeSlider& slider)
    {
        QObject::connect(&slider, &RangeSlider::stateChanged, &m_context, [this, &slider] {
            m_events.push_back(std::format("{},{} adjusting={},{}", slider.valueAt(0),
                                           slider.valueAt(1), slider.isAdjusting(0),
                                           slider.isAdjusting(1)));
        });
    }

    /// The events since the last take(), joined by " | " as the probe does ("none" for none).
    [[nodiscard]] std::string take()
    {
        std::string joined;
        for (const std::string& event : m_events)
        {
            joined += joined.empty() ? event : " | " + event;
        }
        m_events.clear();
        return joined.empty() ? "none" : joined;
    }

private:
    QObject                  m_context;
    std::vector<std::string> m_events;
};

/// The values of the thumbs, as the probe prints them ("v0,v1").
[[nodiscard]] std::string valuesOf(const RangeSlider& slider)
{
    return std::format("{},{}", slider.valueAt(0), slider.valueAt(1));
}

/// The labels of the "Total Impulse" slider: the impulse classes' letters A to O.
[[nodiscard]] std::map<int, QString> impulseLabels()
{
    std::map<int, QString> labels;
    int                    value = 0;
    for (const ImpulseClass impulseClass : kAllImpulseClasses)
    {
        labels.emplace(value++, QtStrings::fromUtf8(toString(impulseClass)));
    }
    return labels;
}

/// The labels of the "Nominal Diameter" slider in the default motor dimension unit.
[[nodiscard]] std::map<int, QString> diameterLabels()
{
    std::map<int, QString> labels;
    int                    value = 0;
    for (const std::string& label : MotorFilterSliders::diameterLabels())
    {
        labels.emplace(value++, QtStrings::fromUtf8(label));
    }
    return labels;
}

/// Makes @p slider the "Total Impulse" slider of the probe (0..14 with a tick at every value and
/// the labels A to O), kProbeWidth wide.
void makeImpulseSlider(RangeSlider& slider)
{
    slider.setTickInterval(1);
    slider.setLabels(impulseLabels());
    slider.resize(kProbeWidth, slider.sizeHint().height());
}

/// Sets the thumbs of @p slider to @p value0 and @p value1, and forgets the events of that.
void setThumbs(RangeSlider& slider, Changes& changes, int value0, int value1)
{
    slider.setValueAt(0, value0);
    slider.setValueAt(1, value1);
    static_cast<void>(changes.take());
}

/// Sends @p slider a mouse event of @p type at @p at, with @p button (the one that changed) and
/// @p buttons (those held during and after it).
void sendMouse(RangeSlider& slider, QEvent::Type type, QPoint at, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(at), QPointF(slider.mapToGlobal(at)), button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&slider, &event);
}

void press(RangeSlider& slider, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(slider, QEvent::MouseButtonPress, at, button, button);
}

void moveTo(RangeSlider& slider, QPoint at, Qt::MouseButton held = Qt::LeftButton)
{
    sendMouse(slider, QEvent::MouseMove, at, Qt::NoButton, held);
}

void release(RangeSlider& slider, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(slider, QEvent::MouseButtonRelease, at, button, Qt::NoButton);
}

void click(RangeSlider& slider, QPoint at, Qt::MouseButton button = Qt::LeftButton)
{
    press(slider, at, button);
    release(slider, at, button);
}

/// The point on the track where @p slider draws the centre of a thumb at @p value.
[[nodiscard]] QPoint at(const RangeSlider& slider, int value)
{
    return {slider.positionForValue(value), slider.thumbRect(0).center().y()};
}

/// Sends @p slider the key @p key with @p modifiers: whether the slider took it.
bool sendKey(RangeSlider& slider, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&slider, &event);
    return event.isAccepted();
}

/// What a click of the left button at @p point does to @p slider with its thumbs at @p value0 and
/// @p value1: the values and the current thumb after it ("v0,v1 current=c"); its events stay in
/// @p changes.
[[nodiscard]] std::string clickWith(RangeSlider& slider, Changes& changes, int value0, int value1,
                                    QPoint point)
{
    setThumbs(slider, changes, value0, value1);
    click(slider, point);
    return std::format("{} current={}", valuesOf(slider), slider.currentThumb());
}

/// What a click of the left button at @p x beside the thumbs at 6 and 8 does (clickWith()).
[[nodiscard]] std::string pressBetween(RangeSlider& slider, Changes& changes, int x)
{
    return clickWith(slider, changes, 6, 8, {x, slider.thumbRect(0).center().y()});
}

/// Drags thumb 0 of @p slider, at 0 with thumb 1 at @p other, 400 pixels to the right: the values
/// after the release.
[[nodiscard]] std::string dragOnto(RangeSlider& slider, Changes& changes, int other)
{
    setThumbs(slider, changes, 0, other);
    const QPoint start = slider.thumbRect(0).center();
    press(slider, start);
    moveTo(slider, start + QPoint(400, 0));
    release(slider, start + QPoint(400, 0));
    return valuesOf(slider);
}

/// Whether the labels of @p slider lie inside it, each centred under its value, side by side
/// without overlapping.
[[nodiscard]] testing::AssertionResult labelsFit(const RangeSlider& slider)
{
    QRect previous;
    for (const auto& [value, text] : slider.labels())
    {
        const QRect label = slider.labelRect(value);
        if (!slider.rect().contains(label))
        {
            return testing::AssertionFailure()
                   << "the label of " << value << " lies outside the slider";
        }
        if (std::abs(label.center().x() - slider.positionForValue(value)) > 1)
        {
            return testing::AssertionFailure() << "the label of " << value << " is off its value";
        }
        if (label.top() != slider.sliderRect().bottom() + 1)
        {
            return testing::AssertionFailure() << "the label of " << value << " is off the row";
        }
        if (!previous.isNull() && previous.right() >= label.left())
        {
            return testing::AssertionFailure()
                   << "the label of " << value << " overlaps the one before";
        }
        previous = label;
    }
    return testing::AssertionSuccess();
}

/// Whether every value of @p slider has its thumb further right than the one before, and
/// valueForPosition() reads it back from its position.
[[nodiscard]] testing::AssertionResult positionsRoundTrip(const RangeSlider& slider)
{
    int previous = -1;
    for (int value = slider.minimum(); value <= slider.maximum(); ++value)
    {
        const int position = slider.positionForValue(value);
        if (position <= previous)
        {
            return testing::AssertionFailure()
                   << "the thumb of " << value << " is not right of " << value - 1;
        }
        if (slider.valueForPosition(position) != value)
        {
            return testing::AssertionFailure()
                   << "the position of " << value << " reads " << slider.valueForPosition(position);
        }
        previous = position;
    }
    return testing::AssertionSuccess();
}

/// Whether a press of the left button at every x from where thumb 0 ends to where thumb 1 starts
/// (the thumbs at 6 and 8) moves the thumb with the nearer edge, thumb 0 when both are as near:
/// MultiSliderUI.findClosest().
[[nodiscard]] testing::AssertionResult pressesBetweenPickTheNearerThumb(RangeSlider& slider,
                                                                        Changes&     changes)
{
    setThumbs(slider, changes, 6, 8);
    const int leftEnd    = slider.thumbRect(0).left() + slider.thumbRect(0).width();
    const int rightStart = slider.thumbRect(1).left();
    for (int x = leftEnd; x < rightStart; ++x)
    {
        const std::string expected =
            x - leftEnd <= rightStart - x ? "7,8 current=0" : "6,7 current=1";
        const std::string actual = pressBetween(slider, changes, x);
        if (actual != expected)
        {
            return testing::AssertionFailure() << "a press at x " << x << " gives " << actual;
        }
    }
    return testing::AssertionSuccess();
}

/// Waits until @p slider's thumb @p thumb holds @p value (the held press's repeats), then for
/// longer than a repeat takes: whether it got there and stayed.
[[nodiscard]] bool reachesAndStays(const RangeSlider& slider, int thumb, int value)
{
    const bool reached = QTest::qWaitFor([&] { return slider.valueAt(thumb) == value; }, 5000);
    QTest::qWait(static_cast<int>(2 * RangeSlider::kScrollInterval.count()));
    return reached && slider.valueAt(thumb) == value;
}

/// Holds the left button at @p point of @p slider with its thumbs at 0 and 1000 until thumb
/// @p thumb stops: whether it stops at @p value.
[[nodiscard]] bool holdStopsAt(RangeSlider& slider, Changes& changes, QPoint point, int thumb,
                               int value)
{
    setThumbs(slider, changes, 0, 1000);
    press(slider, point);
    const bool stops = reachesAndStays(slider, thumb, value);
    release(slider, point);
    return stops;
}

TEST_F(RangeSliderTest, StartsAsTheFilterPanelMakesIt)
{
    // Java: new MultiSlider(HORIZONTAL, 0, 14, 0, 14) with setBounded(true): thumb 0 current,
    // focusable (probe: "focusable: true"); no ticks and no labels until they are set.
    const RangeSlider slider(0, 14, 0, 14, nullptr);
    EXPECT_EQ(slider.minimum(), 0);
    EXPECT_EQ(slider.maximum(), 14);
    EXPECT_EQ(slider.valueAt(0), 0);
    EXPECT_EQ(slider.valueAt(1), 14);
    EXPECT_FALSE(slider.isAdjusting(0));
    EXPECT_FALSE(slider.isAdjusting(1));
    EXPECT_EQ(slider.currentThumb(), 0);
    EXPECT_EQ(slider.focusPolicy(), Qt::StrongFocus);
    EXPECT_EQ(slider.layoutDirection(), Qt::LeftToRight);
    EXPECT_EQ(slider.sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    EXPECT_EQ(slider.tickInterval(), 0);
    EXPECT_TRUE(slider.labels().empty());
    EXPECT_EQ(slider.blockIncrement(), 1);
    EXPECT_EQ(RangeSlider(0, 1000, 0, 1000, nullptr).blockIncrement(), 100);
}

TEST_F(RangeSliderTest, RefusesAnInvalidRangeButTakesCrossedValues)
{
    // probe: "new MultiSlider(0, 14, 20, -5): java.lang.IllegalArgumentException: invalid range
    // properties", "new MultiSlider(0, 14, 9, 3): 9,3".
    EXPECT_THROW(static_cast<void>(RangeSlider(0, 14, 20, -5, nullptr)), BugError);
    EXPECT_THROW(static_cast<void>(RangeSlider(0, 14, 3, 15, nullptr)), BugError);
    EXPECT_THROW(static_cast<void>(RangeSlider(5, 4, 5, 5, nullptr)), BugError);
    const RangeSlider crossed(0, 14, 9, 3, nullptr);
    EXPECT_EQ(valuesOf(crossed), "9,3");
}

TEST_F(RangeSliderTest, HasTwoThumbsOnly)
{
    RangeSlider slider(0, 14, 0, 14, nullptr);
    EXPECT_THROW(static_cast<void>(slider.valueAt(2)), BugError);
    EXPECT_THROW(static_cast<void>(slider.valueAt(-1)), BugError);
    EXPECT_THROW(slider.setValueAt(2, 3), BugError);
    EXPECT_THROW(static_cast<void>(slider.isAdjusting(2)), BugError);
    EXPECT_THROW(static_cast<void>(slider.thumbRect(2)), BugError);
}

TEST_F(RangeSliderTest, ASetValueIsClampedIntoTheRangeAndPassesTheOtherThumb)
{
    // probe, thumbs at 5,3: "setValueAt(0, 9): values=9,3", "setValueAt(1, 20): values=9,14",
    // "setValueAt(0, -3): values=0,14", one event each; "setValueAt(0, 0) again: none".
    RangeSlider      slider(0, 14, 5, 3, nullptr);
    Changes          changes(slider);
    const QSignalSpy values(&slider, &RangeSlider::valueChanged);
    slider.setValueAt(0, 9);
    slider.setValueAt(1, 20);
    slider.setValueAt(0, -3);
    EXPECT_EQ(changes.take(),
              "9,3 adjusting=false,false | 9,14 adjusting=false,false | "
              "0,14 adjusting=false,false");
    ASSERT_EQ(values.count(), 3);
    EXPECT_EQ(values.at(1).at(0).toInt(), 1);
    EXPECT_EQ(values.at(1).at(1).toInt(), 14);
    slider.setValueAt(0, 0);
    EXPECT_EQ(changes.take(), "none");
    EXPECT_EQ(values.count(), 3);
}

TEST_F(RangeSliderTest, TheKeysMoveTheCurrentThumb)
{
    // probe, thumbs at 0,14: +unit 1,14; +block 2,14; max 14,14; +unit at the maximum 14,14;
    // min 0,14; -unit and -block at the minimum 0,14; four events. The bindings: Right, Up and
    // their keypad keys +unit, Left, Down -unit, Page Up/Down a block, Home min, End max.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    Changes     changes(slider);
    EXPECT_TRUE(sendKey(slider, Qt::Key_Right));
    EXPECT_EQ(valuesOf(slider), "1,14");
    EXPECT_TRUE(sendKey(slider, Qt::Key_PageUp));
    EXPECT_EQ(valuesOf(slider), "2,14");
    EXPECT_TRUE(sendKey(slider, Qt::Key_End));
    EXPECT_EQ(valuesOf(slider), "14,14");
    EXPECT_TRUE(sendKey(slider, Qt::Key_Up));
    EXPECT_EQ(valuesOf(slider), "14,14");
    EXPECT_TRUE(sendKey(slider, Qt::Key_Home));
    EXPECT_EQ(valuesOf(slider), "0,14");
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left));
    EXPECT_TRUE(sendKey(slider, Qt::Key_PageDown));
    EXPECT_EQ(valuesOf(slider), "0,14");
    EXPECT_EQ(changes.take(),
              "1,14 adjusting=false,false | 2,14 adjusting=false,false | "
              "14,14 adjusting=false,false | 0,14 adjusting=false,false");

    EXPECT_TRUE(sendKey(slider, Qt::Key_Up, Qt::KeypadModifier));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Right));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Right));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Down));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left, Qt::KeypadModifier));
    EXPECT_EQ(valuesOf(slider), "1,14");
    // Swing binds the keys without modifiers only: the others go on to the widgets around.
    EXPECT_FALSE(sendKey(slider, Qt::Key_Right, Qt::ControlModifier));
    EXPECT_FALSE(sendKey(slider, Qt::Key_Home, Qt::ShiftModifier));
    EXPECT_FALSE(sendKey(slider, Qt::Key_A));
    EXPECT_EQ(valuesOf(slider), "1,14");
}

TEST_F(RangeSliderTest, AClickOnAThumbMakesItTheOneTheKeysMovePastTheOther)
{
    // probe, thumbs at 5,7: "click on thumb 1": current=1 and no event; then -unit four times:
    // 5,6 5,5 5,4 5,3 (the keys are not bounded).
    RangeSlider slider(0, 14, 5, 7, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    click(slider, slider.thumbRect(1).center());
    EXPECT_EQ(slider.currentThumb(), 1);
    EXPECT_EQ(changes.take(), "none");
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Left));
    EXPECT_EQ(changes.take(),
              "5,6 adjusting=false,false | 5,5 adjusting=false,false | "
              "5,4 adjusting=false,false | 5,3 adjusting=false,false");
}

TEST_F(RangeSliderTest, ADragMovesTheThumbUpToTheOtherOneAndNoFurther)
{
    // probe, thumbs at 2,6, a press on thumb 0: nothing; a drag by +10 px (less than a value):
    // still 2, adjusting; by +200: 6,6; by -500: 0,6; the release.
    RangeSlider slider(0, 14, 2, 6, nullptr);
    makeImpulseSlider(slider);
    Changes      changes(slider);
    const QPoint centre = slider.thumbRect(0).center();
    press(slider, centre);
    EXPECT_EQ(changes.take(), "none");
    EXPECT_FALSE(slider.isAdjusting(0));
    moveTo(slider, centre + QPoint(1, 0));
    EXPECT_EQ(valuesOf(slider), "2,6");
    EXPECT_TRUE(slider.isAdjusting(0));
    moveTo(slider, centre + QPoint(200, 0));
    EXPECT_EQ(valuesOf(slider), "6,6");
    moveTo(slider, centre - QPoint(500, 0));
    EXPECT_EQ(valuesOf(slider), "0,6");
    release(slider, centre - QPoint(500, 0));
    EXPECT_EQ(changes.take(),
              "2,6 adjusting=true,false | 6,6 adjusting=true,false | "
              "0,6 adjusting=true,false | 0,6 adjusting=false,false");
    EXPECT_EQ(slider.currentThumb(), 0);
}

TEST_F(RangeSliderTest, ThumbsOnEachOtherPartTheWayTheDragGoes)
{
    // probe, thumbs at 7,7 (the press finds both, thumb 0 current): a drag to the left moves
    // thumb 0 (6,7), back to the right only up to thumb 1 (7,7); a drag to the right moves thumb
    // 1 (7,8, current 1); no move first, then to the right: thumb 0, which stays (7,7).
    RangeSlider slider(0, 14, 7, 7, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);

    press(slider, at(slider, 7));
    EXPECT_EQ(slider.currentThumb(), 0);
    moveTo(slider, at(slider, 6));
    EXPECT_EQ(valuesOf(slider), "6,7");
    moveTo(slider, at(slider, 8));
    EXPECT_EQ(valuesOf(slider), "7,7");
    release(slider, at(slider, 8));
    EXPECT_EQ(changes.take(),
              "7,7 adjusting=true,false | 6,7 adjusting=true,false | "
              "7,7 adjusting=true,false | 7,7 adjusting=false,false");

    press(slider, at(slider, 7));
    moveTo(slider, at(slider, 8));
    EXPECT_EQ(valuesOf(slider), "7,8");
    EXPECT_EQ(slider.currentThumb(), 1);
    release(slider, at(slider, 8));
    EXPECT_EQ(changes.take(),
              "7,7 adjusting=false,true | 7,8 adjusting=false,true | 7,8 adjusting=false,false");

    setThumbs(slider, changes, 7, 7);
    press(slider, at(slider, 7));
    moveTo(slider, at(slider, 7));
    moveTo(slider, at(slider, 8));
    EXPECT_EQ(valuesOf(slider), "7,7");
    EXPECT_EQ(slider.currentThumb(), 0);
    release(slider, at(slider, 8));
    EXPECT_EQ(changes.take(), "7,7 adjusting=true,false | 7,7 adjusting=false,false");
}

TEST_F(RangeSliderTest, APressBesideTheThumbsMovesTheClosestOneABlockTowardsIt)
{
    // probe, thumbs at 3,11, a press and its release at the values 1, 5, 6, 7 move thumb 0 (2,11
    // and 4,11), at 8, 9 and 13 thumb 1 (3,10 and 3,12): the thumb set adjusting, the block, the
    // release.
    RangeSlider slider(0, 14, 3, 11, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 1)), "2,11 current=0");
    EXPECT_EQ(changes.take(),
              "3,11 adjusting=true,false | 2,11 adjusting=true,false | 2,11 adjusting=false,false");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 5)), "4,11 current=0");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 6)), "4,11 current=0");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 7)), "4,11 current=0");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 8)), "3,10 current=1");
    EXPECT_EQ(changes.take(),
              "3,11 adjusting=false,true | 3,10 adjusting=false,true | 3,10 adjusting=false,false");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 9)), "3,10 current=1");
    EXPECT_EQ(clickWith(slider, changes, 3, 11, at(slider, 13)), "3,12 current=1");
}

TEST_F(RangeSliderTest, APressBetweenTheThumbsMovesTheOneWithTheNearerEdge)
{
    // probe, thumbs at 6,8: a press just after thumb 0 ends (x 136) moves thumb 0 (7,8), just
    // before thumb 1 starts (x 165) thumb 1 (6,7); MultiSliderUI.findClosest() compares the
    // distances to the edges, thumb 0 winning a tie.
    RangeSlider slider(0, 14, 6, 8, nullptr);
    makeImpulseSlider(slider);
    Changes   changes(slider);
    const int leftEnd    = slider.thumbRect(0).left() + slider.thumbRect(0).width();
    const int rightStart = slider.thumbRect(1).left();
    ASSERT_LT(leftEnd + 1, rightStart - 1);
    EXPECT_EQ(pressBetween(slider, changes, leftEnd + 1), "7,8 current=0");
    EXPECT_EQ(pressBetween(slider, changes, rightStart - 1), "6,7 current=1");
    EXPECT_TRUE(pressesBetweenPickTheNearerThumb(slider, changes));
}

TEST_F(RangeSliderTest, APressBesideThumbsOnEachOtherMovesTheOneOnThatSide)
{
    // probe, thumbs at 7,7: a press at 12 moves thumb 1 (7,8), at 2 thumb 0 (6,7).
    RangeSlider slider(0, 14, 7, 7, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    click(slider, at(slider, 12));
    EXPECT_EQ(valuesOf(slider), "7,8");
    EXPECT_EQ(slider.currentThumb(), 1);
    setThumbs(slider, changes, 7, 7);
    click(slider, at(slider, 2));
    EXPECT_EQ(valuesOf(slider), "6,7");
    EXPECT_EQ(slider.currentThumb(), 0);
}

TEST_F(RangeSliderTest, APressAtTheEndsOrInTheLabelsMovesAThumbToo)
{
    // probe, thumbs at 3,11: a press at x 1 moves thumb 0 (2,11), at the right end thumb 1
    // (3,12); in the labels under thumb 0 thumb 1 (3,10: findClosest() passes over a thumb whose
    // x range holds the mouse), under thumb 1 thumb 0 (4,11), under 7 thumb 0 (4,11).
    RangeSlider slider(0, 14, 3, 11, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    click(slider, {1, slider.thumbRect(0).center().y()});
    EXPECT_EQ(valuesOf(slider), "2,11");
    setThumbs(slider, changes, 3, 11);
    click(slider, {slider.width() - 2, slider.thumbRect(0).center().y()});
    EXPECT_EQ(valuesOf(slider), "3,12");
    EXPECT_EQ(slider.currentThumb(), 1);

    const int labelsY = slider.labelRect(3).center().y();
    ASSERT_GT(labelsY, slider.thumbRect(0).bottom());
    setThumbs(slider, changes, 3, 11);
    click(slider, {slider.positionForValue(3), labelsY});
    EXPECT_EQ(valuesOf(slider), "3,10");
    EXPECT_EQ(slider.currentThumb(), 1);
    setThumbs(slider, changes, 3, 11);
    click(slider, {slider.positionForValue(11), labelsY});
    EXPECT_EQ(valuesOf(slider), "4,11");
    EXPECT_EQ(slider.currentThumb(), 0);
    setThumbs(slider, changes, 3, 11);
    click(slider, {slider.positionForValue(7), labelsY});
    EXPECT_EQ(valuesOf(slider), "4,11");
}

TEST_F(RangeSliderTest, AnyButtonPressesAndDrags)
{
    // probe, thumbs at 0,14: the right button at 7 moves thumb 0 (1,14), then the middle button
    // (2,14); a right-button drag of thumb 0 from 0 to 3: 3,14.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    click(slider, at(slider, 7), Qt::RightButton);
    EXPECT_EQ(valuesOf(slider), "1,14");
    click(slider, at(slider, 7), Qt::MiddleButton);
    EXPECT_EQ(valuesOf(slider), "2,14");
    setThumbs(slider, changes, 0, 14);
    press(slider, at(slider, 0), Qt::RightButton);
    moveTo(slider, at(slider, 3), Qt::RightButton);
    release(slider, at(slider, 3), Qt::RightButton);
    EXPECT_EQ(valuesOf(slider), "3,14");
    EXPECT_FALSE(slider.isAdjusting(0));
}

TEST_F(RangeSliderTest, TheWheelGoesOnToTheWidgetsAround)
{
    // probe: "wheel up at 7: values=0,14", no event.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    EXPECT_FALSE(turnWheel(slider, 120));
    EXPECT_FALSE(turnWheel(slider, -120));
    EXPECT_EQ(valuesOf(slider), "0,14");
    EXPECT_EQ(changes.take(), "none");
}

TEST_F(RangeSliderTest, ADisabledSliderTakesNoInput)
{
    // probe, thumbs at 3,11, disabled: a press at 7 and a drag of thumb 0 change nothing.
    RangeSlider slider(0, 14, 3, 11, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    slider.setEnabled(false);
    click(slider, at(slider, 7));
    press(slider, slider.thumbRect(0).center());
    moveTo(slider, slider.thumbRect(0).center() + QPoint(40, 0));
    release(slider, slider.thumbRect(0).center() + QPoint(40, 0));
    static_cast<void>(sendKey(slider, Qt::Key_Right));
    EXPECT_EQ(valuesOf(slider), "3,11");
    EXPECT_EQ(changes.take(), "none");
}

TEST_F(RangeSliderTest, DisablingEndsAPress)
{
    // (Not in Java, whose release does nothing on a disabled slider: the thumb would stay
    // adjusting.) A held press beside the thumbs stops when the slider is disabled.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    press(slider, at(slider, 10));
    EXPECT_TRUE(slider.isAdjusting(1));
    slider.setEnabled(false);
    EXPECT_FALSE(slider.isAdjusting(1));
    QTest::qWait(
        static_cast<int>((RangeSlider::kScrollDelay + RangeSlider::kScrollInterval).count()));
    EXPECT_EQ(valuesOf(slider), "0,13");
}

TEST_F(RangeSliderTest, AHeldPressRepeatsItsBlockUntilTheThumbReachesTheMouse)
{
    // probe, thumbs at 0,14, held at 10: 0,13 at once, still after 200 ms, then 0,12, 0,11, 0,10
    // ("ms between them (rounded): 0 300 100 100"), then nothing until the release.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    Changes       changes(slider);
    QElapsedTimer clock;
    clock.start();
    press(slider, at(slider, 10));
    EXPECT_EQ(valuesOf(slider), "0,13");
    ASSERT_TRUE(QTest::qWaitFor([&slider] { return slider.valueAt(1) == 12; }, 5000));
    // The first repeat waits kScrollDelay out (a precise timer: less a millisecond of rounding;
    // a busy machine only makes it later).
    EXPECT_GE(clock.elapsed(), RangeSlider::kScrollDelay.count() - 1);
    EXPECT_TRUE(reachesAndStays(slider, 1, 10));
    release(slider, at(slider, 10));
    EXPECT_EQ(changes.take(),
              "0,14 adjusting=false,true | 0,13 adjusting=false,true | "
              "0,12 adjusting=false,true | 0,11 adjusting=false,true | "
              "0,10 adjusting=false,true | 0,10 adjusting=false,false");
    EXPECT_EQ(RangeSlider::kScrollDelay.count(), 300);
    EXPECT_EQ(RangeSlider::kScrollInterval.count(), 100);
}

TEST_F(RangeSliderTest, AHeldPressFollowsTheMouse)
{
    // probe, thumbs at 0,14, held at 4 (thumb 0: 1,14), the mouse moved to 12: thumb 0 goes on
    // to 12, a block at a time.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    Changes changes(slider);
    press(slider, at(slider, 4));
    EXPECT_EQ(valuesOf(slider), "1,14");
    moveTo(slider, at(slider, 12));
    EXPECT_TRUE(reachesAndStays(slider, 0, 12));
    release(slider, at(slider, 12));
    EXPECT_EQ(changes.take(),
              "0,14 adjusting=true,false | 1,14 adjusting=true,false | 2,14 adjusting=true,false | "
              "3,14 adjusting=true,false | 4,14 adjusting=true,false | 5,14 adjusting=true,false | "
              "6,14 adjusting=true,false | 7,14 adjusting=true,false | 8,14 adjusting=true,false | "
              "9,14 adjusting=true,false | 10,14 adjusting=true,false | "
              "11,14 adjusting=true,false | 12,14 adjusting=true,false | "
              "12,14 adjusting=false,false");
}

TEST_F(RangeSliderTest, AHeldPressStepsOnceMoreWhenTheMouseMovesBehindTheThumb)
{
    // probe, thumbs at 0,14, held at 10 (0,13), the mouse moved to the right end: one more block
    // (0,12), then it stops (the step comes before the check).
    RangeSlider slider(0, 14, 0, 14, nullptr);
    makeImpulseSlider(slider);
    press(slider, at(slider, 10));
    moveTo(slider, {slider.width() - 1, slider.thumbRect(0).center().y()});
    EXPECT_TRUE(reachesAndStays(slider, 1, 12));
    QTest::qWait(static_cast<int>(RangeSlider::kScrollDelay.count()));
    EXPECT_EQ(valuesOf(slider), "0,12");
    release(slider, {slider.width() - 1, slider.thumbRect(0).center().y()});
}

TEST_F(RangeSliderTest, AHeldPressTakesTheThumbPastTheOther)
{
    // probe, thumbs at 3,8, held at 6 (thumb 1 is nearer: 3,7), the mouse moved to 1: thumb 1
    // goes on past thumb 0 to 3,1.
    RangeSlider slider(0, 14, 3, 8, nullptr);
    makeImpulseSlider(slider);
    press(slider, at(slider, 6));
    EXPECT_EQ(valuesOf(slider), "3,7");
    moveTo(slider, at(slider, 1));
    EXPECT_TRUE(reachesAndStays(slider, 1, 1));
    EXPECT_EQ(valuesOf(slider), "3,1");
    release(slider, at(slider, 1));
}

TEST_F(RangeSliderTest, AHeldPressStopsOnceTheThumbsEdgeReachesTheMouse)
{
    // probe, the length slider 0..1000, thumbs at 0,1000 (the thumb at 400 is x 116..126, at 600
    // 174..184): held at x 127 thumb 0 stops at 400, at x 128 it goes on to 500; held at x 174
    // thumb 1 stops at 600, at x 173 it goes on to 500. Up, a thumb stops once its end (its left
    // edge and width) reaches the mouse, down once its left edge does.
    RangeSlider slider(0, 1000, 0, 1000, nullptr);
    slider.resize(kProbeWidth, slider.sizeHint().height());
    Changes     changes(slider);
    const QRect at400 = slider.thumbRect(0).translated(
        slider.positionForValue(400) - slider.positionForValue(0), 0);
    const QRect at600 = slider.thumbRect(0).translated(
        slider.positionForValue(600) - slider.positionForValue(0), 0);
    const int y   = at400.center().y();
    const int end = at400.left() + at400.width();
    EXPECT_TRUE(holdStopsAt(slider, changes, {end, y}, 0, 400));
    EXPECT_TRUE(holdStopsAt(slider, changes, {end + 1, y}, 0, 500));
    EXPECT_TRUE(holdStopsAt(slider, changes, {at600.left(), y}, 1, 600));
    EXPECT_TRUE(holdStopsAt(slider, changes, {at600.left() - 1, y}, 1, 500));
}

TEST_F(RangeSliderTest, TheLengthSlidersBlockIsAHundred)
{
    // probe, the length slider at 0,1000: +block 100,1000; +unit 101,1000; a press at 500
    // 201,1000.
    RangeSlider slider(0, 1000, 0, 1000, nullptr);
    slider.resize(kProbeWidth, slider.sizeHint().height());
    Changes changes(slider);
    EXPECT_TRUE(sendKey(slider, Qt::Key_PageUp));
    EXPECT_TRUE(sendKey(slider, Qt::Key_Right));
    click(slider, at(slider, 500));
    EXPECT_EQ(changes.take(),
              "100,1000 adjusting=false,false | 101,1000 adjusting=false,false | "
              "101,1000 adjusting=true,false | 201,1000 adjusting=true,false | "
              "201,1000 adjusting=false,false");
    // MultiSliderUI's block is a tenth of the maximum, which is none for a maximum under 10.
    RangeSlider small(0, 9, 0, 9, nullptr);
    EXPECT_EQ(small.blockIncrement(), 0);
    EXPECT_TRUE(sendKey(small, Qt::Key_PageUp));
    EXPECT_EQ(valuesOf(small), "0,9");
}

TEST_F(RangeSliderTest, ADragKeepsTheValueBetweenTheNeighbours)
{
    // Deviation (RangeSlider.h): Java keeps the thumb's pixel between its neighbours' and reads
    // the value back from that pixel: probe, the length slider, thumb 0 dragged onto thumb 1 at
    // 251 reads 252,251, onto 999 1000,999, thumb 1 onto thumb 0 at 251 251,252. Here the value
    // stays between the neighbours' values.
    RangeSlider slider(0, 1000, 0, 251, nullptr);
    slider.resize(kProbeWidth, slider.sizeHint().height());
    Changes changes(slider);
    EXPECT_EQ(dragOnto(slider, changes, 251), "251,251");
    EXPECT_EQ(dragOnto(slider, changes, 503), "503,503");
    EXPECT_EQ(dragOnto(slider, changes, 507), "507,507");
    EXPECT_EQ(dragOnto(slider, changes, 999), "999,999");
    setThumbs(slider, changes, 251, 1000);
    const QPoint start = slider.thumbRect(1).center();
    press(slider, start);
    moveTo(slider, start - QPoint(400, 0));
    release(slider, start - QPoint(400, 0));
    EXPECT_EQ(valuesOf(slider), "251,251");
}

TEST_F(RangeSliderTest, ThumbsLieAtTheirValuesPositions)
{
    // Java's xPositionForValue() and valueForXPosition(): a thumb's centre at its value's place,
    // further right for a greater value; left of the track the minimum, right of it the maximum.
    RangeSlider slider(0, 14, 3, 11, nullptr);
    makeImpulseSlider(slider);
    EXPECT_TRUE(positionsRoundTrip(slider));
    EXPECT_EQ(slider.thumbRect(0).center().x(), slider.positionForValue(3));
    EXPECT_EQ(slider.thumbRect(1).center().x(), slider.positionForValue(11));
    // At the ends of the range the thumbs lie inside the band along the track (the style may put
    // a handle a pixel above it, as Fusion does for the tick marks below it).
    slider.setValueAt(0, 0);
    slider.setValueAt(1, 14);
    EXPECT_GE(slider.thumbRect(0).left(), slider.sliderRect().left());
    EXPECT_LE(slider.thumbRect(1).right(), slider.sliderRect().right());
    EXPECT_EQ(slider.valueForPosition(-100), 0);
    EXPECT_EQ(slider.valueForPosition(slider.width() + 100), 14);
    EXPECT_EQ(slider.positionForValue(-5), slider.positionForValue(0));
    EXPECT_EQ(slider.positionForValue(20), slider.positionForValue(14));

    RangeSlider diameter(0, 11, 0, 11, nullptr);
    diameter.setLabels(diameterLabels());
    diameter.resize(diameter.minimumSizeHint());
    EXPECT_TRUE(positionsRoundTrip(diameter));
}

TEST_F(RangeSliderTest, TheImpulseLabelsLieUnderTheirValues)
{
    // MotorFilterPanel's impulse slider: the labels A to O under the values 0 to 14 (Java's label
    // table), in a row under the slider, which the size hints make room for.
    RangeSlider slider(0, 14, 0, 14, nullptr);
    const int   plainHeight = slider.sizeHint().height();
    makeImpulseSlider(slider);
    EXPECT_EQ(slider.tickInterval(), 1);
    ASSERT_EQ(slider.labels().size(), std::size_t{15});
    EXPECT_EQ(slider.labels().at(0), QStringLiteral("A"));
    EXPECT_EQ(slider.labels().at(14), QStringLiteral("O"));
    EXPECT_GT(slider.sizeHint().height(), plainHeight);
    EXPECT_EQ(slider.minimumSizeHint().height(), slider.sizeHint().height());
    EXPECT_GE(slider.sizeHint().width(), RangeSlider::kPreferredLength);
    EXPECT_TRUE(labelsFit(slider));
    slider.resize(slider.sizeHint());
    EXPECT_TRUE(labelsFit(slider));
    slider.resize(slider.minimumSizeHint());
    EXPECT_TRUE(labelsFit(slider));
    EXPECT_TRUE(slider.labelRect(15).isNull());

    const RangeSlider unlabelled(0, 1000, 0, 1000, nullptr);
    EXPECT_TRUE(unlabelled.labelRect(0).isNull());
    EXPECT_EQ(unlabelled.minimumSizeHint().width(), RangeSlider::kMinimumLength);
    EXPECT_EQ(unlabelled.sizeHint().width(), RangeSlider::kPreferredLength);
}

TEST_F(RangeSliderTest, TheDiameterLabelsFitInMillimetresAndInches)
{
    // MotorFilterPanel's diameter slider over its 12 stops, labelled in the default motor
    // dimension unit: "0" .. "98", "+" in mm, "0", ".24", .., "3.9", "+" in inches.
    const QtRocket::Test::DefaultUnitsGuard units;
    RangeSlider                             slider(0, 11, 0, 11, nullptr);
    slider.setTickInterval(1);
    slider.setLabels(diameterLabels());
    EXPECT_EQ(slider.labels().at(10), QStringLiteral("98"));
    slider.resize(slider.minimumSizeHint());
    EXPECT_TRUE(labelsFit(slider));

    ASSERT_TRUE(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    slider.setLabels(diameterLabels());
    EXPECT_EQ(slider.labels().at(1), QStringLiteral(".24"));
    EXPECT_EQ(slider.labels().at(11), QStringLiteral("+"));
    slider.resize(slider.minimumSizeHint());
    EXPECT_TRUE(labelsFit(slider));
    slider.resize(kProbeWidth, slider.sizeHint().height());
    EXPECT_TRUE(labelsFit(slider));
}

TEST_F(RangeSliderTest, RefusesALabelOutsideItsRange)
{
    RangeSlider slider(0, 11, 0, 11, nullptr);
    EXPECT_THROW(slider.setLabels({{12, QStringLiteral("+")}}), BugError);
    EXPECT_THROW(slider.setLabels({{-1, QStringLiteral("0")}}), BugError);
    EXPECT_TRUE(slider.labels().empty());
}

TEST_F(RangeSliderTest, APressGivesItTheFocus)
{
    // MultiTrackListener.mousePressed(): slider.requestFocus().
    QWidget     window;
    auto* const slider = makeChild<RangeSlider>(&window, 0, 14, 0, 14);
    auto* const other  = makeChild<QWidget>(&window);
    other->setFocusPolicy(Qt::StrongFocus);
    ASSERT_TRUE(showActiveWith(window, {other, slider}));
    other->setFocus();
    ASSERT_TRUE(other->hasFocus());
    click(*slider, slider->thumbRect(1).center());
    EXPECT_TRUE(slider->hasFocus());
    EXPECT_EQ(slider->currentThumb(), 1);
}

TEST_F(RangeSliderTest, PaintsInEveryState)
{
    // No pixels are compared: the painting runs (the style's groove, ticks, handles and labels)
    // at rest, during a drag, while held, disabled and with the thumbs crossed.
    RangeSlider slider(0, 14, 2, 6, nullptr);
    makeImpulseSlider(slider);
    EXPECT_FALSE(slider.grab().isNull());
    press(slider, slider.thumbRect(1).center());
    moveTo(slider, at(slider, 9));
    EXPECT_FALSE(slider.grab().isNull());
    release(slider, at(slider, 9));
    press(slider, at(slider, 13));
    EXPECT_FALSE(slider.grab().isNull());
    release(slider, at(slider, 13));
    slider.setValueAt(0, 12);
    slider.setEnabled(false);
    EXPECT_FALSE(slider.grab().isNull());
}

}  // namespace

}  // namespace QtRocket::Gui
