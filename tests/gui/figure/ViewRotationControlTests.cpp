// The design view's rotation control (ViewRotationControl): its fields and the figure's rotation
// following each other, the clamp to 0 .. 2 pi, the slider's Shift snap and the rotation lock
// stored as the preference LockClickDragRotation. The snapping itself is pinned against
// OpenRocket in rocket_panel_golden_tests.cpp.

#include "QtRocket/gui/figure/ViewRotationControl.h"

#include <memory>
#include <numbers>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QSize>
#include <QSlider>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

using ViewRotationControlTest = Gui::Test::GuiTest;

constexpr double kDotsPerInch = 96.0;
constexpr double kTwoPi       = 2 * std::numbers::pi;

/// A rocket with a body tube, its figure and the rotation control of the figure, in a widget of
/// their own.
struct RotationControl
{
    explicit RotationControl(bool locked = false)
      : figure(makeChild<RocketFigure>(&holder, withBodyTube(rocket), preferences, kDotsPerInch)),
        control(makeChild<ViewRotationControl>(&holder, *figure, withLock(preferences, locked)))
    {
    }

    /// @p rocket with a stage holding a body tube, its events enabled.
    static Rocket& withBodyTube(Rocket& rocket)
    {
        rocket.addChild(std::make_unique<AxialStage>())
            .addChild(std::make_unique<BodyTube>(0.3, 0.02));
        rocket.enableEvents();
        return rocket;
    }

    /// @p preferences with the rotation lock @p locked.
    static Preferences& withLock(InMemoryPreferences& preferences, bool locked)
    {
        preferences.putBoolean(GuiPreferenceKeys::kLockClickDragRotation, locked);
        return preferences;
    }

    [[nodiscard]] UnitSpinBox& spinner() const { return *control->spinner(); }
    [[nodiscard]] ValueSlider& slider() const { return *control->rotationSlider(); }

    /// Types @p text into the spin box and commits it with Enter.
    void type(const QString& text) const
    {
        auto* edit = spinner().findChild<QLineEdit*>();
        edit->selectAll();
        QTest::keyClicks(edit, text);
        QTest::keyClick(&spinner(), Qt::Key_Return);
    }

    /// A mouse event of @p type over the slider with @p modifiers held, which the control reads
    /// Shift from (the slider's own press is not there, so it does not move).
    void mouseOverSlider(QEvent::Type type, Qt::KeyboardModifiers modifiers) const
    {
        const QPointF point(slider().width() / 2.0, slider().height() / 2.0);
        // A move with the button held: Qt delivers no move without a button to a widget that
        // does not track the mouse.
        const bool  move = type == QEvent::MouseMove;
        QMouseEvent event(type, point, slider().mapToGlobal(point),
                          move ? Qt::NoButton : Qt::LeftButton,
                          move ? Qt::LeftButton : Qt::NoButton, modifiers);
        QApplication::sendEvent(&slider(), &event);
    }

    Rocket               rocket;
    InMemoryPreferences  preferences;
    QWidget              holder;
    RocketFigure*        figure  = nullptr;
    ViewRotationControl* control = nullptr;
};

TEST_F(ViewRotationControlTest, IsMadeAsOpenRocketsControl)
{
    const RotationControl      shown;
    const ViewRotationControl& control = *shown.control;
    EXPECT_EQ(control.objectName(), QStringLiteral("rotationControl"));

    // The spin box without buttons, 50 pixels wide, and the unit selector of the angle.
    const UnitSpinBox& spinner = shown.spinner();
    EXPECT_EQ(spinner.objectName(), QStringLiteral("Rotation"));
    EXPECT_EQ(spinner.buttonSymbols(), QAbstractSpinBox::NoButtons);
    EXPECT_EQ(spinner.minimumWidth(), ViewRotationControl::kSpinnerWidth);
    EXPECT_EQ(spinner.maximumWidth(), ViewRotationControl::kSpinnerWidth);
    EXPECT_EQ(control.unitSelector()->objectName(), QStringLiteral("Rotation"));
    EXPECT_EQ(&control.binding()->getUnitGroup(), &unitGroup(UnitGroupId::ANGLE));
    EXPECT_EQ(spinner.binding(), control.binding());
    EXPECT_EQ(control.unitSelector()->binding(), control.binding());

    // The vertical slider, inverted (0 at the top), over the binding from 0 to 2 pi.
    const ValueSlider& slider = shown.slider();
    EXPECT_EQ(slider.objectName(), QStringLiteral("Rotation"));
    EXPECT_EQ(slider.orientation(), Qt::Vertical);
    EXPECT_TRUE(slider.invertedAppearance());
    EXPECT_EQ(slider.binding(), control.binding());
    EXPECT_TRUE(slider.mapping().isLinear());
    EXPECT_EQ(slider.mapping().getMin(), 0.0);
    EXPECT_EQ(slider.mapping().getMax(), kTwoPi);

    // The lock: 24 x 24 pixels, checkable.
    const QToolButton& lock = *control.lockButton();
    EXPECT_TRUE(lock.isCheckable());
    EXPECT_EQ(lock.minimumSize(), QSize(24, 24));
    EXPECT_EQ(lock.maximumSize(), QSize(24, 24));
    EXPECT_EQ(lock.toolTip(), QStringLiteral("Lock/unlock click-drag rotation"));

    // RocketPanel.ttip.Rotation on the fields.
    const QString tip = QStringLiteral(
        u"<html>Adjust the view's roll rotation.<br>You can also drag empty space in the rocket "
        u"view (view lock must be disabled).<br>Hold Shift for 30\u00B0 or 45\u00B0 "
        u"increments.</html>");
    EXPECT_EQ(spinner.toolTip(), tip);
    EXPECT_EQ(control.unitSelector()->toolTip(), tip);
    EXPECT_EQ(slider.toolTip(), tip);
}

TEST_F(ViewRotationControlTest, ShowsTheFiguresRotation)
{
    const RotationControl shown;
    EXPECT_EQ(shown.control->binding()->value(), 0.0);
    EXPECT_EQ(shown.slider().value(), 0);

    shown.figure->setRotation(std::numbers::pi);
    EXPECT_EQ(shown.control->binding()->value(), std::numbers::pi);
    EXPECT_EQ(shown.slider().value(), SliderMapping::kMaximum / 2);
    EXPECT_EQ(shown.spinner().text(), QStringLiteral("180"));

    // A rotation outside 0 .. 2 pi (a long drag in the design view) is shown as it is, and the
    // figure keeps it.
    shown.figure->setRotation(-std::numbers::pi / 2);
    EXPECT_EQ(shown.control->binding()->value(), -std::numbers::pi / 2);
    EXPECT_EQ(shown.figure->rotation(), -std::numbers::pi / 2);
    EXPECT_EQ(shown.slider().value(), 0);
}

TEST_F(ViewRotationControlTest, AValueOfTheFieldsRotatesTheFigureWithinATurn)
{
    const RotationControl shown;
    shown.type(QStringLiteral("45"));
    EXPECT_DOUBLE_EQ(shown.figure->rotation(), std::numbers::pi / 4);

    // DoubleModel.setValue() clamps to 0 .. 2 pi, and the fields show the figure's rotation.
    shown.type(QStringLiteral("400"));
    EXPECT_EQ(shown.figure->rotation(), kTwoPi);
    EXPECT_EQ(shown.control->binding()->value(), kTwoPi);
    shown.type(QStringLiteral("-30"));
    EXPECT_EQ(shown.figure->rotation(), 0.0);
    EXPECT_EQ(shown.control->binding()->value(), 0.0);

    // The slider rotates it too.
    shown.slider().setValue(SliderMapping::kMaximum / 4);
    EXPECT_DOUBLE_EQ(shown.figure->rotation(), std::numbers::pi / 2);
}

TEST_F(ViewRotationControlTest, ShiftSnapsTheDraggedSlider)
{
    const RotationControl shown;
    ValueSlider&          slider = shown.slider();

    // Dragged without Shift: no snap (1/9 of a turn is 40 degrees).
    slider.setSliderDown(true);
    slider.setSliderPosition(SliderMapping::kMaximum / 9);
    EXPECT_NEAR(MathUtil::rad2deg(shown.figure->rotation()), 40.0, 0.5);

    // With Shift: the nearest multiple of 30 or 45 degrees, here 45 for 43 degrees (a position
    // whose value, rounded to the degree, is another one: the binding tells of no unchanged one).
    shown.mouseOverSlider(QEvent::MouseMove, Qt::ShiftModifier);
    slider.setSliderPosition(120);
    EXPECT_EQ(shown.figure->rotation(), MathUtil::javaToRadians(45.0));
    EXPECT_EQ(shown.control->binding()->value(), MathUtil::javaToRadians(45.0));
    // 140 degrees: 135.
    slider.setSliderPosition(389);
    EXPECT_EQ(shown.figure->rotation(), MathUtil::javaToRadians(135.0));

    // Released: Shift is forgotten (Java's mouseReleased()), and a slider that is not dragged
    // never snaps.
    slider.setSliderDown(false);
    shown.mouseOverSlider(QEvent::MouseButtonRelease, Qt::ShiftModifier);
    slider.setSliderDown(true);
    slider.setSliderPosition(SliderMapping::kMaximum / 9);
    EXPECT_NEAR(MathUtil::rad2deg(shown.figure->rotation()), 40.0, 0.5);
    slider.setSliderDown(false);
    shown.mouseOverSlider(QEvent::MouseMove, Qt::ShiftModifier);
    slider.setValue(SliderMapping::kMaximum / 5);
    EXPECT_NEAR(MathUtil::rad2deg(shown.figure->rotation()), 72.0, 0.5);
}

TEST_F(ViewRotationControlTest, TheLockIsThePreference)
{
    {
        const RotationControl unlocked;
        EXPECT_FALSE(unlocked.control->isDragRotationLocked());
        EXPECT_FALSE(unlocked.control->lockButton()->isChecked());
    }
    const RotationControl locked(true);
    ViewRotationControl&  control = *locked.control;
    EXPECT_TRUE(control.isDragRotationLocked());
    EXPECT_TRUE(control.lockButton()->isChecked());

    // The user's toggle stores it.
    const QSignalSpy changes(&control, &ViewRotationControl::dragRotationLockChanged);
    control.lockButton()->click();
    EXPECT_FALSE(control.isDragRotationLocked());
    EXPECT_FALSE(locked.preferences.getBoolean(GuiPreferenceKeys::kLockClickDragRotation, true));
    ASSERT_EQ(changes.count(), 1);
    EXPECT_FALSE(changes.at(0).at(0).toBool());

    // setDragRotationLocked() does not (Java: only the button's action listener stores it).
    control.setDragRotationLocked(true);
    EXPECT_TRUE(control.isDragRotationLocked());
    EXPECT_TRUE(control.lockButton()->isChecked());
    EXPECT_FALSE(locked.preferences.getBoolean(GuiPreferenceKeys::kLockClickDragRotation, true));
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(ViewRotationControlTest, RotatesNothingOnceTheFigureIsGone)
{
    const RotationControl shown;
    shown.figure->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // The fields still take values, and nothing reaches the figure that is gone (ASan).
    shown.type(QStringLiteral("90"));
    EXPECT_DOUBLE_EQ(shown.control->binding()->value(), std::numbers::pi / 2);
    shown.slider().setValue(SliderMapping::kMaximum / 2);
    EXPECT_EQ(shown.control->binding()->value(), std::numbers::pi);
}

}  // namespace

}  // namespace QtRocket::Gui
