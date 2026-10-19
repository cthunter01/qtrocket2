// The editing rules of OpenRocket's spinners that UnitSpinBox and IntSpinBox share
// (SpinBoxEditing); the spin boxes' own tests and number_field_tests.cpp drive them with keys,
// clicks and the wheel.

#include "QtRocket/gui/widgets/SpinBoxEditing.h"

#include <cstddef>
#include <memory>
#include <vector>

#include <QAbstractSpinBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/SelectionKeyFilter.h"
#include "WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SpinBoxEditingTest = Test::GuiTest;
using SpinBoxEditing::KeyAction;
using Test::editorOf;
using Test::showActive;

/// What a press of @p key does in a spin box edited (@p edited) or not.
[[nodiscard]] KeyAction actionOf(Qt::Key key, bool edited,
                                 Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QKeyEvent event(QEvent::KeyPress, key, modifiers);
    return SpinBoxEditing::keyAction(event, edited);
}

/// The directions repeatSteps() asked for, a step allowed while the list is shorter than a limit.
struct StepRecorder
{
    std::vector<bool> steps;
    std::size_t       limit{1000};

    [[nodiscard]] bool step(bool up)
    {
        steps.push_back(up);
        return steps.size() < limit;
    }
};

/// repeatSteps() of @p steps on a spin box that lives, recorded.
[[nodiscard]] std::vector<bool> stepsOf(int steps, std::size_t limit = 1000)
{
    QDoubleSpinBox                   spin;
    const QPointer<QAbstractSpinBox> alive(&spin);
    StepRecorder                     recorder;
    recorder.limit = limit;
    SpinBoxEditing::repeatSteps(alive, steps, [&recorder](bool up) { return recorder.step(up); });
    return recorder.steps;
}

/// repeatSteps() of three steps up whose first deletes the spin box: the steps made.
[[nodiscard]] int stepsWhileTheFirstDeletesTheSpinBox()
{
    auto                             spin = std::make_unique<QDoubleSpinBox>();
    const QPointer<QAbstractSpinBox> alive(spin.get());
    int                              made = 0;
    SpinBoxEditing::repeatSteps(alive, 3, [&spin, &made](bool /*up*/) {
        ++made;
        spin.reset();
        return true;
    });
    return made;
}

TEST_F(SpinBoxEditingTest, EnterCommitsAndEscRevertsOnlyAnEdit)
{
    EXPECT_EQ(actionOf(Qt::Key_Return, true), KeyAction::COMMIT);
    EXPECT_EQ(actionOf(Qt::Key_Return, false), KeyAction::COMMIT);
    EXPECT_EQ(actionOf(Qt::Key_Enter, false, Qt::KeypadModifier), KeyAction::COMMIT);
    EXPECT_EQ(actionOf(Qt::Key_Escape, true), KeyAction::REVERT);
    EXPECT_EQ(actionOf(Qt::Key_Escape, false), KeyAction::PASS_ON);
    EXPECT_EQ(actionOf(Qt::Key_Up, true), KeyAction::EDIT);
    EXPECT_EQ(actionOf(Qt::Key_PageDown, false), KeyAction::EDIT);
    EXPECT_EQ(actionOf(Qt::Key_A, true), KeyAction::EDIT);
    EXPECT_EQ(actionOf(Qt::Key_Left, false), KeyAction::EDIT);
}

TEST_F(SpinBoxEditingTest, SetsASpinBoxUp)
{
    QDoubleSpinBox spin;
    SpinBoxEditing::setUp(spin);
    EXPECT_EQ(spin.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(spin.keyboardTracking());
    EXPECT_FALSE(spin.isAccelerated());
    EXPECT_NE(spin.findChild<SelectionKeyFilter*>(), nullptr);
}

TEST_F(SpinBoxEditingTest, RepeatsStepsUntilABound)
{
    EXPECT_EQ(stepsOf(3), (std::vector<bool>{true, true, true}));
    EXPECT_EQ(stepsOf(-2), (std::vector<bool>{false, false}));
    EXPECT_EQ(stepsOf(0), std::vector<bool>{});
    EXPECT_EQ(stepsOf(10, 2), (std::vector<bool>{true, true}));
    EXPECT_EQ(stepsWhileTheFirstDeletesTheSpinBox(), 1);
}

TEST_F(SpinBoxEditingTest, TheWheelNeedsTheFocus)
{
    QWidget     window;
    auto* const layout = makeChild<QVBoxLayout>(&window);
    auto* const spin   = makeChild<QDoubleSpinBox>(&window);
    auto* const other  = makeChild<QLineEdit>(&window);
    layout->addWidget(spin);
    layout->addWidget(other);
    ASSERT_TRUE(showActive(window));
    other->setFocus();
    EXPECT_FALSE(SpinBoxEditing::takesWheel(*spin));
    spin->setFocus();
    EXPECT_TRUE(SpinBoxEditing::takesWheel(*spin));
}

TEST_F(SpinBoxEditingTest, SelectsAllOnceTheEventsInHandAreDone)
{
    QDoubleSpinBox spin;
    spin.setValue(12.5);
    QLineEdit& edit = *editorOf(spin);
    edit.deselect();
    SpinBoxEditing::selectAllOnFocus(spin, QFocusEvent(QEvent::FocusIn, Qt::MouseFocusReason));
    EXPECT_FALSE(edit.hasSelectedText());  // the click puts its cursor first
    QCoreApplication::processEvents();
    EXPECT_EQ(edit.selectedText(), spin.text());

    // The focus back from a popup menu: Swing's popups take no focus.
    edit.deselect();
    SpinBoxEditing::selectAllOnFocus(spin, QFocusEvent(QEvent::FocusIn, Qt::PopupFocusReason));
    QCoreApplication::processEvents();
    EXPECT_FALSE(edit.hasSelectedText());
}

TEST_F(SpinBoxEditingTest, ADeletedSpinBoxIsNotSelected)
{
    // The spin box is the deferred call's context (proved under ASan).
    auto spin = std::make_unique<QDoubleSpinBox>();
    SpinBoxEditing::selectAllOnFocus(*spin, QFocusEvent(QEvent::FocusIn, Qt::TabFocusReason));
    spin.reset();
    QCoreApplication::processEvents();
    EXPECT_EQ(spin, nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
