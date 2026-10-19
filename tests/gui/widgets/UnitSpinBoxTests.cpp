// The spin box of a number field (Java: a JSpinner on DoubleModel's spinner model, with
// SpinnerEditor). The interaction cases of the bindings scout, in a dialog with the field's unit
// selector and slider, are in number_field_tests.cpp; these are the spin box's own rules.

#include "QtRocket/gui/widgets/UnitSpinBox.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include <QAbstractSpinBox>
#include <QFontMetrics>
#include <QLineEdit>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/SelectionKeyFilter.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using UnitSpinBoxTest = Test::GuiTest;
using Test::BeanProperty;
using Test::bodyTubeLength;
using Test::editorOf;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::TwoTubeRocket;
using Test::typeInto;

/// A binding of @p source on @p component in the length units, without bounds.
[[nodiscard]] std::shared_ptr<DoubleBinding> lengthBinding(RocketComponent&    component,
                                                           const DoubleSource& source)
{
    return std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(component), source,
                                           unitGroup(UnitGroupId::LENGTH),
                                           DoubleBinding::Options{});
}

/// A source whose setter throws a BugError, as a broken invariant of the core would.
[[nodiscard]] DoubleSource throwingSource()
{
    return DoubleSource::of<BodyTube>({
        .get = [](const BodyTube& tube) { return tube.getLength(); },
        .set = [](BodyTube& /*tube*/, double /*length*/) { bug("a setter's broken invariant"); },
    });
}

/// A shown, active window holding a spin box of @p binding and a line edit to take the focus.
class Window
{
public:
    explicit Window(std::shared_ptr<DoubleBinding> binding)
      : m_spin(makeChild<UnitSpinBox>(&m_window, std::move(binding))),
        m_other(makeChild<QLineEdit>(&m_window)),
        m_active(showActiveWith(m_window, {m_spin, m_other}))
    {
    }

    [[nodiscard]] bool         isActive() const noexcept { return m_active; }
    [[nodiscard]] UnitSpinBox& spin() const noexcept { return *m_spin; }
    [[nodiscard]] QLineEdit&   other() const noexcept { return *m_other; }
    [[nodiscard]] QLineEdit&   editor() const { return *editorOf(*m_spin); }

private:
    QWidget      m_window;
    UnitSpinBox* m_spin{nullptr};
    QLineEdit*   m_other{nullptr};
    bool         m_active{false};
};

TEST_F(UnitSpinBoxTest, NeedsABinding)
{
    EXPECT_THROW(static_cast<void>(UnitSpinBox(nullptr, nullptr)), BugError);
    const TwoTubeRocket rocket;
    UnitSpinBox         spin(lengthBinding(*rocket.tubeA, bodyTubeLength()), nullptr);
    const auto          binding = spin.binding();
    EXPECT_THROW(spin.setBinding(nullptr), BugError);
    EXPECT_EQ(spin.binding(), binding);
}

TEST_F(UnitSpinBoxTest, FollowsTheEditingRulesOfOpenRocketsSpinners)
{
    const TwoTubeRocket rocket;
    const UnitSpinBox   spin(lengthBinding(*rocket.tubeA, bodyTubeLength()), nullptr);
    EXPECT_EQ(spin.focusPolicy(), Qt::StrongFocus);  // never the wheel's focus
    EXPECT_FALSE(spin.keyboardTracking());
    EXPECT_FALSE(spin.isAccelerated());
    EXPECT_NE(spin.findChild<SelectionKeyFilter*>(), nullptr);
    EXPECT_EQ(spin.text(), QStringLiteral("30"));  // 0.3 m in the default cm
    EXPECT_FALSE(spin.fixedStep().has_value());
}

TEST_F(UnitSpinBoxTest, ShowsAProblemReadOnlyAndWithoutSteps)
{
    // The override CD of a stage without a force analysis shows the Error's text (M1's
    // ComponentDrag, aero/ComponentDrag.h) in place of the value.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    bean.problem = "No force analysis";
    const Window window(lengthBinding(*rocket.tubeA, bean.checkedSource()));
    ASSERT_TRUE(window.isActive());
    EXPECT_EQ(window.spin().text(), QStringLiteral("No force analysis"));
    EXPECT_TRUE(window.spin().isReadOnly());
    window.spin().setFocus();
    QTest::keyClick(&window.spin(), Qt::Key_Up);
    QTest::keyClicks(&window.spin(), QStringLiteral("7"));
    QTest::keyClick(&window.spin(), Qt::Key_Return);
    EXPECT_EQ(bean.sets, 0);
    EXPECT_EQ(window.spin().text(), QStringLiteral("No force analysis"));

    bean.problem.reset();
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(window.spin().text(), QStringLiteral("50"));
    EXPECT_FALSE(window.spin().isReadOnly());
    QTest::keyClick(&window.spin(), Qt::Key_Up);
    EXPECT_DOUBLE_EQ(bean.value, 0.51);
}

TEST_F(UnitSpinBoxTest, AReadOnlySourceIsShownReadOnly)
{
    const TwoTubeRocket rocket;
    const UnitSpinBox   spin(
        lengthBinding(*rocket.tubeA,
                      DoubleSource::of<BodyTube>({
                          .get = [](const BodyTube& tube) { return tube.getLength(); },
                      })),
        nullptr);
    EXPECT_TRUE(spin.isReadOnly());
    EXPECT_EQ(spin.text(), QStringLiteral("30"));
}

TEST_F(UnitSpinBoxTest, TheBindingAndTheFieldEnableItTogether)
{
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    const Window        window(lengthBinding(*rocket.tubeA, bean.checkedSource()));
    ASSERT_TRUE(window.isActive());
    UnitSpinBox& spin = window.spin();
    EXPECT_TRUE(spin.isEnabled());
    EXPECT_TRUE(spin.isFieldEnabled());

    bean.enabled = false;
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(spin.isEnabled());
    spin.setFieldEnabled(true);
    EXPECT_FALSE(spin.isEnabled());

    bean.enabled = true;
    spin.setFieldEnabled(false);  // a check box's follower
    rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(spin.isEnabled());
    EXPECT_FALSE(spin.isFieldEnabled());

    // The field's state leaves an edit in progress as it is (Java's setEnabled()).
    spin.setFieldEnabled(true);
    EXPECT_TRUE(spin.isEnabled());
    typeInto(spin, QStringLiteral("12"));
    spin.setFieldEnabled(true);
    EXPECT_EQ(spin.text(), QStringLiteral("12"));
    EXPECT_TRUE(window.editor().isModified());
}

TEST_F(UnitSpinBoxTest, AChangeElsewhereReplacesAnEditInProgress)
{
    // Java's editor takes the model's value too when the model tells of a change.
    const TwoTubeRocket rocket;
    const Window        window(lengthBinding(*rocket.tubeA, bodyTubeLength()));
    ASSERT_TRUE(window.isActive());
    typeInto(window.spin(), QStringLiteral("12"));
    rocket.tubeA->setLength(0.45);
    EXPECT_EQ(window.spin().text(), QStringLiteral("45"));
    EXPECT_FALSE(window.editor().isModified());
    window.other().setFocus();
    EXPECT_DOUBLE_EQ(rocket.tubeA->getLength(), 0.45);
}

TEST_F(UnitSpinBoxTest, TellsWhenAnEditingIsFinished)
{
    const TwoTubeRocket rocket;
    const Window        window(lengthBinding(*rocket.tubeA, bodyTubeLength()));
    ASSERT_TRUE(window.isActive());
    const QSignalSpy finished(&window.spin(), &QAbstractSpinBox::editingFinished);
    typeInto(window.spin(), QStringLiteral("40"));
    QTest::keyClick(&window.spin(), Qt::Key_Return);
    EXPECT_EQ(finished.count(), 1);
    window.other().setFocus();
    EXPECT_EQ(finished.count(), 2);
    EXPECT_DOUBLE_EQ(rocket.tubeA->getLength(), 0.4);
}

TEST_F(UnitSpinBoxTest, AFixedStepStepsInTheCurrentUnit)
{
    // Java's getSpinnerModel(0.1): the shape parameter of a nose cone, without a unit.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    const Window        window(std::make_shared<DoubleBinding>(
        std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(),
        unitGroup(UnitGroupId::NONE), DoubleBinding::Options{.min = 0, .max = 1}));
    ASSERT_TRUE(window.isActive());
    UnitSpinBox& spin = window.spin();
    spin.setFixedStep(0.1);
    EXPECT_EQ(spin.fixedStep(), 0.1);
    spin.setFocus();
    QTest::keyClick(&spin, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(bean.value, 0.6);
    QTest::keyClick(&spin, Qt::Key_Down);
    QTest::keyClick(&spin, Qt::Key_Down);
    EXPECT_DOUBLE_EQ(bean.value, 0.4);

    spin.setFixedStep(std::nullopt);  // the unit's own step: +1, clamped to the maximum
    QTest::keyClick(&spin, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(bean.value, 1.0);

    EXPECT_THROW(spin.setFixedStep(0.0), BugError);
    EXPECT_THROW(spin.setFixedStep(-0.1), BugError);
    EXPECT_THROW(spin.setFixedStep(std::numeric_limits<double>::quiet_NaN()), BugError);
    EXPECT_THROW(spin.setFixedStep(std::numeric_limits<double>::infinity()), BugError);
    EXPECT_FALSE(spin.fixedStep().has_value());
}

TEST_F(UnitSpinBoxTest, AnotherBindingTakesThePlaceOfTheFirst)
{
    // Java's InnerTubeConfig switches its separation field between two models.
    const TwoTubeRocket rocket;
    const Window        window(lengthBinding(*rocket.tubeA, bodyTubeLength()));
    ASSERT_TRUE(window.isActive());
    UnitSpinBox& spin = window.spin();
    typeInto(spin, QStringLiteral("12"));
    spin.setBinding(lengthBinding(*rocket.tubeB, bodyTubeLength()));
    EXPECT_EQ(spin.text(), QStringLiteral("50"));  // the edit is dropped
    rocket.tubeA->setLength(0.25);
    EXPECT_EQ(spin.text(), QStringLiteral("50"));
    typeInto(spin, QStringLiteral("60"));
    QTest::keyClick(&spin, Qt::Key_Return);
    EXPECT_DOUBLE_EQ(rocket.tubeB->getLength(), 0.6);
    EXPECT_DOUBLE_EQ(rocket.tubeA->getLength(), 0.25);
}

TEST_F(UnitSpinBoxTest, AnEditPendingWhenItsComponentsAreGoneReachesNoComponent)
{
    // An undo replaces every component (Rocket::loadFrom()) and destroys the old ones once its
    // event is delivered: the spin box shows nothing, takes no focus and commits nothing (proved
    // under ASan). The focus loss that disabling the focused spin box causes commits nothing
    // either: the text is gone first.
    TwoTubeRocket      design;
    BodyTube* const    tube = design.tubeA;
    OpenRocketDocument document(std::move(design.owner));
    const auto         binding = lengthBinding(*tube, bodyTubeLength());
    const Window       window(binding);
    ASSERT_TRUE(window.isActive());
    UnitSpinBox& spin = window.spin();
    document.addUndoPosition("Modify Body tube");
    tube->setLength(0.4);
    typeInto(spin, QStringLiteral("99"));
    ASSERT_TRUE(spin.hasFocus());
    document.undo();
    EXPECT_FALSE(binding->isValid());
    EXPECT_EQ(spin.text(), QString());
    EXPECT_FALSE(spin.isEnabled());
    EXPECT_FALSE(spin.hasFocus());
    EXPECT_FALSE(spin.commitEdit());
    window.editor().setText(QStringLiteral("77"));
    window.editor().setModified(true);
    EXPECT_FALSE(spin.commitEdit());
    spin.stepBy(1);
    EXPECT_EQ(spin.text(), QString());
}

TEST_F(UnitSpinBoxTest, ABindingOutlivesItsSpinBox)
{
    // The spin box's connection goes with it: the binding's later changes reach no widget
    // (proved under ASan).
    const TwoTubeRocket rocket;
    const auto          binding = lengthBinding(*rocket.tubeA, bodyTubeLength());
    auto                spin    = std::make_unique<UnitSpinBox>(binding, nullptr);
    spin.reset();
    rocket.tubeA->setLength(0.7);
    EXPECT_DOUBLE_EQ(binding->value(), 0.7);
}

TEST_F(UnitSpinBoxTest, WhatACommitThrowsIsReported)
{
    // A BugError of the core never leaves an event handler into Qt's event loop: Guard reports
    // it, with the field's name.
    const TwoTubeRocket      rocket;
    const RecordingGuardHook hook;
    const Window             window(lengthBinding(*rocket.tubeA, throwingSource()));
    ASSERT_TRUE(window.isActive());
    window.spin().setObjectName(QStringLiteral("Length"));
    typeInto(window.spin(), QStringLiteral("40"));
    QTest::keyClick(&window.spin(), Qt::Key_Return);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::BUG);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Length"));
    EXPECT_EQ(window.spin().text(), QStringLiteral("30"));
    QTest::keyClick(&window.spin(), Qt::Key_Up);
    EXPECT_EQ(hook.failures().size(), 2U);
}

TEST_F(UnitSpinBoxTest, IsWideEnoughForATypicalValue)
{
    // QAbstractSpinBox measures the texts of its value bounds, which this spin box has none of.
    const TwoTubeRocket rocket;
    const UnitSpinBox   spin(lengthBinding(*rocket.tubeA, bodyTubeLength()), nullptr);
    const QFontMetrics  metrics = spin.fontMetrics();
    EXPECT_GT(spin.sizeHint().width(), metrics.horizontalAdvance(QStringLiteral("-0000.000")));
    EXPECT_GT(spin.minimumSizeHint().width(), metrics.horizontalAdvance(QStringLiteral("000")));
    EXPECT_LT(spin.minimumSizeHint().width(), spin.sizeHint().width());
    EXPECT_GE(spin.sizeHint().height(), metrics.height());
}

}  // namespace

}  // namespace QtRocket::Gui
