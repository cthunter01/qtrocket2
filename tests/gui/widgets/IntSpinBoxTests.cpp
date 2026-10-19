// The spin box of a count field (Java: a JSpinner on IntegerModel's spinner model, with
// SpinnerEditor): the bounds and steps of IntBinding (OpenRocket's own values are pinned in
// tests/gui/binding/IntBindingTests.cpp), with the editing rules of UnitSpinBox.

#include "QtRocket/gui/widgets/IntSpinBox.h"

#include <memory>
#include <utility>

#include <QAbstractSpinBox>
#include <QDialog>
#include <QLineEdit>
#include <QString>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/SelectionKeyFilter.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using IntSpinBoxTest = Test::GuiTest;
using Test::editorOf;
using Test::IntBeanProperty;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::turnWheel;
using Test::TwoTubeRocket;
using Test::typeInto;

/// The source of a fin set's fin count.
[[nodiscard]] IntSource finCount()
{
    return IntSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFinCount(); },
        .set = [](FinSet& fins, int count) { fins.setFinCount(count); },
    });
}

/// A source whose setter throws a BugError, as a broken invariant of the core would.
[[nodiscard]] IntSource throwingSource()
{
    return IntSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFinCount(); },
        .set = [](FinSet& /*fins*/, int /*count*/) { bug("a setter's broken invariant"); },
    });
}

/// A rocket of two tubes with a trapezoidal fin set (three fins) on the first.
struct FinRocket
{
    TwoTubeRocket    design;
    TrapezoidFinSet* fins{&design.tubeA->addChild(std::make_unique<TrapezoidFinSet>())};
};

/// The binding of the fin count of @p fins, 1 to 8 (FinSetConfig's).
[[nodiscard]] std::shared_ptr<IntBinding> finCountBinding(FinSet& fins)
{
    return std::make_shared<IntBinding>(std::make_shared<ComponentSelection>(fins), finCount(), 1,
                                        8);
}

/// A shown, active dialog holding a spin box of @p binding, a line edit to take the focus and the
/// OK and Cancel buttons (OK the default).
class Dialog
{
public:
    explicit Dialog(std::shared_ptr<IntBinding> binding)
      : m_spin(makeChild<IntSpinBox>(&m_dialog, std::move(binding))),
        m_other(makeChild<QLineEdit>(&m_dialog)),
        m_active(showActiveWith(m_dialog, {m_spin, m_other}))
    {
    }

    [[nodiscard]] bool        isActive() const noexcept { return m_active; }
    [[nodiscard]] bool        isOpen() const { return m_dialog.isVisible(); }
    [[nodiscard]] int         result() const { return m_dialog.result(); }
    [[nodiscard]] IntSpinBox& spin() const noexcept { return *m_spin; }
    [[nodiscard]] QLineEdit&  other() const noexcept { return *m_other; }
    [[nodiscard]] QLineEdit&  editor() const { return *editorOf(*m_spin); }

private:
    QDialog     m_dialog;
    IntSpinBox* m_spin{nullptr};
    QLineEdit*  m_other{nullptr};
    bool        m_active{false};
};

TEST_F(IntSpinBoxTest, NeedsABinding)
{
    EXPECT_THROW(static_cast<void>(IntSpinBox(nullptr, nullptr)), BugError);
}

TEST_F(IntSpinBoxTest, FollowsTheEditingRulesOfOpenRocketsSpinners)
{
    const FinRocket  rocket;
    const IntSpinBox spin(finCountBinding(*rocket.fins), nullptr);
    EXPECT_EQ(spin.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(spin.keyboardTracking());
    EXPECT_FALSE(spin.isAccelerated());
    EXPECT_NE(spin.findChild<SelectionKeyFilter*>(), nullptr);
    EXPECT_EQ(spin.value(), 3);
    EXPECT_EQ(spin.minimum(), 1);
    EXPECT_EQ(spin.maximum(), 8);
}

TEST_F(IntSpinBoxTest, StepsWithinTheBounds)
{
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    IntSpinBox& spin = dialog.spin();
    spin.setFocus();
    QTest::keyClick(&spin, Qt::Key_Up);
    EXPECT_EQ(rocket.fins->getFinCount(), 4);
    EXPECT_EQ(spin.value(), 4);
    QTest::keyClick(&spin, Qt::Key_PageUp);  // ten steps, up to the maximum
    EXPECT_EQ(rocket.fins->getFinCount(), 8);
    QTest::keyClick(&spin, Qt::Key_Up);
    EXPECT_EQ(rocket.fins->getFinCount(), 8);
    QTest::keyClick(&spin, Qt::Key_Down);
    EXPECT_EQ(rocket.fins->getFinCount(), 7);
}

TEST_F(IntSpinBoxTest, AValueBeyondABoundIsShownAndStepsBackIn)
{
    // A count of 10 under the bounds 1 to 8 (the core's fin count clamps itself, a source need
    // not): shown as it is; no next value (Java: null at or beyond the maximum); the previous one,
    // 9, is clamped to 8 (IntegerModel.setValue()).
    TwoTubeRocket   rocket;
    IntBeanProperty bean{.value = 10};
    const Dialog    dialog(std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(), 1, 8));
    ASSERT_TRUE(dialog.isActive());
    IntSpinBox& spin = dialog.spin();
    EXPECT_EQ(spin.value(), 10);
    EXPECT_EQ(spin.text(), QStringLiteral("10"));
    EXPECT_EQ(spin.maximum(), 10);
    spin.setFocus();
    QTest::keyClick(&spin, Qt::Key_Up);
    EXPECT_EQ(bean.value, 10);
    EXPECT_EQ(bean.sets, 0);
    QTest::keyClick(&spin, Qt::Key_Down);
    EXPECT_EQ(bean.value, 8);
    EXPECT_EQ(bean.sets, 1);
    EXPECT_EQ(spin.value(), 8);
    EXPECT_EQ(spin.maximum(), 8);
}

TEST_F(IntSpinBoxTest, EnterCommitsATypedCountAndKeepsTheDialogOpen)
{
    TwoTubeRocket   rocket;
    IntBeanProperty bean{.value = 3};
    const Dialog    dialog(std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(), 1, 8));
    ASSERT_TRUE(dialog.isActive());
    typeInto(dialog.spin(), QStringLiteral("5"));
    QTest::keyClick(&dialog.spin(), Qt::Key_Return);
    EXPECT_EQ(bean.value, 5);
    EXPECT_EQ(bean.sets, 1);
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_TRUE(dialog.spin().hasFocus());
    EXPECT_EQ(dialog.editor().selectedText(), QStringLiteral("5"));
}

TEST_F(IntSpinBoxTest, AnEditIsCommittedBeforeTheArrowSteps)
{
    TwoTubeRocket   rocket;
    IntBeanProperty bean{.value = 3};
    const Dialog    dialog(std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(), 1, 8));
    ASSERT_TRUE(dialog.isActive());
    typeInto(dialog.spin(), QStringLiteral("5"));
    QTest::keyClick(&dialog.spin(), Qt::Key_Up);
    EXPECT_EQ(bean.value, 6);
    EXPECT_EQ(bean.sets, 2);
    EXPECT_EQ(dialog.spin().value(), 6);
}

TEST_F(IntSpinBoxTest, EscRevertsAnEditAndOtherwiseClosesTheDialog)
{
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    typeInto(dialog.spin(), QStringLiteral("6"));
    QTest::keyClick(&dialog.spin(), Qt::Key_Escape);
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("3"));
    EXPECT_FALSE(dialog.editor().isModified());
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_EQ(rocket.fins->getFinCount(), 3);

    QTest::keyClick(&dialog.spin(), Qt::Key_Escape);
    EXPECT_FALSE(dialog.isOpen());
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
}

TEST_F(IntSpinBoxTest, TheWheelStepsOnlyAFocusedSpinBox)
{
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    dialog.other().setFocus();
    EXPECT_FALSE(turnWheel(dialog.spin(), 120));
    EXPECT_EQ(rocket.fins->getFinCount(), 3);
    dialog.spin().setFocus();
    EXPECT_TRUE(turnWheel(dialog.spin(), 120));
    EXPECT_EQ(rocket.fins->getFinCount(), 4);
}

TEST_F(IntSpinBoxTest, TextThatCannotBeACountIsRefusedAsItIsTyped)
{
    // A QSpinBox refuses such a keystroke, where Java's formatter reverts the text on the commit
    // (the scout's decision D13).
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    typeInto(dialog.spin(), QStringLiteral("x"));
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("3"));
    typeInto(dialog.spin(), QStringLiteral("9"));  // above the maximum
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("3"));
    typeInto(dialog.spin(), QStringLiteral("2*3"));
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("2"));
    dialog.other().setFocus();
    EXPECT_EQ(rocket.fins->getFinCount(), 2);
}

TEST_F(IntSpinBoxTest, CommitEditCommitsAPendingCount)
{
    // As BindingScope does before OK, Close, a save and an undo (macOS's buttons take no focus).
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    EXPECT_FALSE(dialog.spin().commitEdit());
    typeInto(dialog.spin(), QStringLiteral("6"));
    EXPECT_TRUE(dialog.spin().commitEdit());
    EXPECT_EQ(rocket.fins->getFinCount(), 6);
    EXPECT_FALSE(dialog.spin().commitEdit());

    // An empty text is no count: it is reverted.
    typeInto(dialog.spin(), QString());
    QTest::keyClick(&dialog.spin(), Qt::Key_Delete);
    EXPECT_FALSE(dialog.spin().commitEdit());
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("6"));
}

TEST_F(IntSpinBoxTest, ACommitOfTheSpinBoxItselfReachesTheBinding)
{
    // QSpinBox commits by itself where IntSpinBox does not first (interpretText(), a hidden spin
    // box with an edit): the count goes to the binding all the same.
    const FinRocket rocket;
    const Dialog    dialog(finCountBinding(*rocket.fins));
    ASSERT_TRUE(dialog.isActive());
    typeInto(dialog.spin(), QStringLiteral("7"));
    dialog.spin().interpretText();
    EXPECT_EQ(rocket.fins->getFinCount(), 7);
    EXPECT_FALSE(dialog.editor().isModified());
}

TEST_F(IntSpinBoxTest, TheBindingAndTheFieldEnableItTogether)
{
    TwoTubeRocket   rocket;
    IntBeanProperty bean{.value = 3};
    IntSpinBox      spin(std::make_shared<IntBinding>(
                             std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source()),
                         nullptr);
    EXPECT_TRUE(spin.isEnabled());
    bean.enabled = false;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(spin.isEnabled());
    bean.enabled = true;
    spin.setFieldEnabled(false);
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(spin.isEnabled());
    EXPECT_FALSE(spin.isFieldEnabled());
    spin.setFieldEnabled(true);
    EXPECT_TRUE(spin.isEnabled());
}

TEST_F(IntSpinBoxTest, AnEditPendingWhenItsComponentsAreGoneReachesNoComponent)
{
    // An undo replaces every component and destroys the old ones once its event is delivered
    // (proved under ASan): the spin box keeps the last count, disabled, and commits nothing.
    FinRocket          rocket;
    FinSet* const      fins = rocket.fins;
    OpenRocketDocument document(std::move(rocket.design.owner));
    const auto         binding = finCountBinding(*fins);
    const Dialog       dialog(binding);
    ASSERT_TRUE(dialog.isActive());
    document.addUndoPosition("Modify Trapezoidal fin set");
    fins->setFinCount(5);
    typeInto(dialog.spin(), QStringLiteral("7"));
    document.undo();
    EXPECT_FALSE(binding->isValid());
    EXPECT_FALSE(dialog.spin().isEnabled());
    EXPECT_EQ(dialog.spin().value(), 5);
    EXPECT_FALSE(dialog.spin().commitEdit());
    dialog.spin().stepBy(1);
    EXPECT_EQ(dialog.spin().value(), 5);
}

TEST_F(IntSpinBoxTest, WhatACommitThrowsIsReported)
{
    const FinRocket          rocket;
    const RecordingGuardHook hook;
    const Dialog             dialog(std::make_shared<IntBinding>(
        std::make_shared<ComponentSelection>(*rocket.fins), throwingSource(), 1, 8));
    ASSERT_TRUE(dialog.isActive());
    dialog.spin().setObjectName(QStringLiteral("FinCount"));
    typeInto(dialog.spin(), QStringLiteral("5"));
    QTest::keyClick(&dialog.spin(), Qt::Key_Return);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::BUG);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("FinCount"));
    EXPECT_EQ(dialog.spin().text(), QStringLiteral("3"));
}

}  // namespace

}  // namespace QtRocket::Gui
