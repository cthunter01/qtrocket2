// A number field in a dialog, as a user works it: the spin box, the unit selector and the slider
// of one DoubleBinding, beside another text field and the dialog's OK and Cancel buttons.
//
// The cases are the bindings scout's 20 interaction cases (handoff/m2-scout/evidence/
// m2-scout-bindings/qt/widget_main.cpp -> widget-probe.out, Qt 6.11.2 offscreen), with the
// scout's expectations from OpenRocket's JSpinner, SpinnerEditor and DoubleModel (its REPORT,
// sections 1.6, 4.4 and 4.5): the model is DoubleModel(source, 2.0, UNITS_LENGTH, 0.0, 1.0), a
// diameter of a radius between 0 and 1 m, and its slider getSliderModel(0, 0.04, 0.2). The values
// are + - * / of the units' multipliers and the unit's rounding, so they are pinned exactly. Where
// QtRocket differs from the probe's prototype, the case says so: invalid text sets nothing (the
// user's decision U3; the prototype, as Java, set the last value again).

#include <memory>

#include <QAbstractSpinBox>
#include <QAction>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Test::BeanProperty;
using Test::editorOf;
using Test::OffscreenPopupWarnings;
using Test::showActive;
using Test::turnWheel;
using Test::TwoTubeRocket;
using Test::typeInto;

/// Values after a commit or a step agree with the probe's to a few rounding errors: each one is
/// converted to the unit and back, perhaps several times.
constexpr double kTolerance = 1e-12;

[[nodiscard]] const Unit& lengthUnit(const char* name)
{
    return *unitGroup(UnitGroupId::LENGTH).getUnit(name);
}

/// The item of @p menu with the text @p text, or null.
[[nodiscard]] QAction* menuItem(const QMenu& menu, const QString& text)
{
    for (QAction* const action : menu.actions())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// The dialog of the probe: the field of a radius shown as a diameter (the multiplier 2) in the
/// length units, from 0 to 1 m, with the slider (0, 0.04, 0.2); the radius starts at 0.0125 m.
/// The dialog is shown and active, its OK button the default one.
class NumberFieldTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_bean.value = 0.0125;
        m_binding    = std::make_shared<DoubleBinding>(
            std::make_shared<ComponentSelection>(*m_rocket.tubeA), m_bean.source(),
            unitGroup(UnitGroupId::LENGTH),
            DoubleBinding::Options{.multiplier = 2, .min = 0, .max = 1.0});

        m_dialog        = std::make_unique<QDialog>();
        auto* layout    = makeChild<QVBoxLayout>(m_dialog.get());
        m_row           = makeChild<QWidget>(m_dialog.get());
        auto* rowLayout = makeChild<QHBoxLayout>(m_row);
        m_spin          = makeChild<UnitSpinBox>(m_row, m_binding);
        m_unit          = makeChild<UnitSelector>(m_row, m_binding);
        m_slider = makeChild<ValueSlider>(m_row, m_binding, SliderMapping::fromRange(0, 0.04, 0.2));
        m_spin->setObjectName(QStringLiteral("Value"));
        m_unit->setObjectName(QStringLiteral("Value"));
        m_slider->setObjectName(QStringLiteral("Value"));
        rowLayout->addWidget(m_spin);
        rowLayout->addWidget(m_unit);
        rowLayout->addWidget(m_slider);
        m_other   = makeChild<QLineEdit>(m_dialog.get());
        m_buttons = makeChild<QDialogButtonBox>(m_dialog.get(),
                                                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(m_row);
        layout->addWidget(m_other);
        layout->addWidget(m_buttons);
        QObject::connect(m_buttons, &QDialogButtonBox::accepted, m_dialog.get(), &QDialog::accept);
        QObject::connect(m_buttons, &QDialogButtonBox::rejected, m_dialog.get(), &QDialog::reject);
        ASSERT_TRUE(showActive(*m_dialog));
    }

    void TearDown() override
    {
        m_dialog.reset();
        GuiTest::TearDown();
    }

    /// Gives the radius @p radius to the component, as a change elsewhere would.
    void setRadius(double radius)
    {
        m_bean.value = radius;
        m_rocket.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }

    /// Gives @p widget the focus, and lets the deferred selection of a spin box happen.
    static void focus(QWidget& widget)
    {
        widget.setFocus();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QString      spinText() const { return m_spin->text(); }
    [[nodiscard]] QLineEdit&   editor() const { return *editorOf(*m_spin); }
    [[nodiscard]] QPushButton& okButton() const { return *m_buttons->button(QDialogButtonBox::Ok); }

    /// A button that takes no focus from a click, as macOS's; it records the radius when it is
    /// clicked, in m_valueAtClick.
    QPushButton& addButtonWithoutFocus()
    {
        auto* const button = makeChild<QPushButton>(m_dialog.get(), QStringLiteral("Apply"));
        button->setFocusPolicy(Qt::TabFocus);
        m_dialog->layout()->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, m_dialog.get(),
                         [this] { m_valueAtClick = m_bean.value; });
        return *button;
    }

    /// Records the radius when the dialog is accepted, in m_valueAtAccept.
    void recordValueAtAccept()
    {
        QObject::connect(m_dialog.get(), &QDialog::accepted, m_dialog.get(),
                         [this] { m_valueAtAccept = m_bean.value; });
    }

    TwoTubeRocket                  m_rocket;
    BeanProperty                   m_bean;
    std::shared_ptr<DoubleBinding> m_binding;
    std::unique_ptr<QDialog>       m_dialog;
    QWidget*                       m_row{nullptr};
    UnitSpinBox*                   m_spin{nullptr};
    UnitSelector*                  m_unit{nullptr};
    ValueSlider*                   m_slider{nullptr};
    QLineEdit*                     m_other{nullptr};
    QDialogButtonBox*              m_buttons{nullptr};
    double                         m_valueAtAccept{0};
    double                         m_valueAtClick{0};
};

TEST_F(NumberFieldTest, ShowsTheValueInTheDefaultUnit)
{
    // probe: initial (default unit) radius=0.0125 text=[2.5] setters=0, default button=OK.
    EXPECT_EQ(spinText(), QStringLiteral("2.5"));
    EXPECT_EQ(m_unit->text(), QStringLiteral("cm"));
    EXPECT_EQ(m_slider->value(), m_binding->sliderPosition(m_slider->mapping()));
    EXPECT_EQ(m_bean.sets, 0);
    EXPECT_TRUE(okButton().isDefault());
}

TEST_F(NumberFieldTest, WidgetsAreFoundByTheirPropertyName)
{
    // objectName = the Java property name: the three widgets of the field share it.
    EXPECT_EQ(m_dialog->findChild<UnitSpinBox*>(QStringLiteral("Value")), m_spin);
    EXPECT_EQ(m_dialog->findChild<UnitSelector*>(QStringLiteral("Value")), m_unit);
    EXPECT_EQ(m_dialog->findChild<ValueSlider*>(QStringLiteral("Value")), m_slider);
}

TEST_F(NumberFieldTest, EnterCommitsAnExpressionAndKeepsTheDialogOpen)
{
    // probe: type 2*3, Enter: radius=0.03 text=[6] setters=1 dialog=open focus=spin.
    const QSignalSpy finished(m_spin, &QAbstractSpinBox::editingFinished);
    typeInto(*m_spin, QStringLiteral("2*3"));
    EXPECT_EQ(spinText(), QStringLiteral("2*3"));  // every keystroke is taken
    QTest::keyClick(m_spin, Qt::Key_Return);
    EXPECT_NEAR(m_bean.value, 0.03, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("6"));
    EXPECT_EQ(m_bean.sets, 1);
    EXPECT_TRUE(m_dialog->isVisible());
    EXPECT_TRUE(m_spin->hasFocus());
    EXPECT_EQ(editor().selectedText(), QStringLiteral("6"));
    EXPECT_EQ(finished.count(), 1);

    typeInto(*m_spin, QStringLiteral("8"));
    QTest::keyClick(m_spin, Qt::Key_Enter);  // the keypad's
    EXPECT_NEAR(m_bean.value, 0.04, kTolerance);
    EXPECT_TRUE(m_dialog->isVisible());
}

TEST_F(NumberFieldTest, AQtSpinBoxLetsEnterPressTheDefaultButton)
{
    // probe, acceptEnter=0: type 2*3, Enter: dialog=ACCEPTED. The reason UnitSpinBox accepts
    // Enter: QAbstractSpinBox interprets the text and lets the key go on to the dialog.
    auto* plain = makeChild<QDoubleSpinBox>(m_row);
    m_row->layout()->addWidget(plain);
    focus(*plain);
    QTest::keyClick(plain, Qt::Key_Return);
    EXPECT_FALSE(m_dialog->isVisible());
    EXPECT_EQ(m_dialog->result(), QDialog::Accepted);
}

TEST_F(NumberFieldTest, AFocusInAndOutWithoutAnEditSetsNothing)
{
    // probe: focus in/out without editing: setter calls +0 (Java commits an edited field only).
    focus(*m_spin);
    focus(*m_other);
    EXPECT_EQ(m_bean.sets, 0);
    EXPECT_EQ(spinText(), QStringLiteral("2.5"));
}

TEST_F(NumberFieldTest, InvalidTextIsRevertedWhenTheFocusLeavesAndSetsNothing)
{
    // probe: type abc, focus out: radius=0.03 text=[6] setters=2. QtRocket sets nothing for
    // invalid text (U3), where the prototype and Java set the last value again.
    setRadius(0.03);
    typeInto(*m_spin, QStringLiteral("abc"));
    focus(*m_other);
    EXPECT_NEAR(m_bean.value, 0.03, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("6"));
    EXPECT_EQ(m_bean.sets, 0);
}

TEST_F(NumberFieldTest, TheUpKeyStepsByTheUnitsStep)
{
    // probe: Up key: radius=0.035 text=[7] (+1 cm), one setter call.
    setRadius(0.03);
    focus(*m_spin);
    QTest::keyClick(m_spin, Qt::Key_Up);
    EXPECT_NEAR(m_bean.value, 0.035, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("7"));
    EXPECT_EQ(m_bean.sets, 1);
    QTest::keyClick(m_spin, Qt::Key_Down);
    EXPECT_NEAR(m_bean.value, 0.03, kTolerance);
}

TEST_F(NumberFieldTest, AnEditIsCommittedBeforeTheArrowSteps)
{
    // probe: type 30, Up key (commit, then step): radius=0.155 text=[31], two setter calls
    // (Java's ArrowButtonHandler: commitEdit(), then getNextValue()).
    setRadius(0.035);
    typeInto(*m_spin, QStringLiteral("30"));
    QTest::keyClick(m_spin, Qt::Key_Up);
    EXPECT_NEAR(m_bean.value, 0.155, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("31"));
    EXPECT_EQ(m_bean.sets, 2);
}

TEST_F(NumberFieldTest, AnArrowButtonCommitsAnEditThenSteps)
{
    // The arrow buttons as the Up key (Java: the same ArrowButtonHandler).
    typeInto(*m_spin, QStringLiteral("30"));
    QStyleOptionSpinBox option;
    option.initFrom(m_spin);
    option.frame         = m_spin->hasFrame();
    option.buttonSymbols = m_spin->buttonSymbols();
    option.subControls   = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown | QStyle::SC_SpinBoxFrame |
                           QStyle::SC_SpinBoxEditField;
    option.stepEnabled   = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
    const QRect up =
        m_spin->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, m_spin);
    QTest::mouseClick(m_spin, Qt::LeftButton, Qt::NoModifier, up.center());
    EXPECT_NEAR(m_bean.value, 0.155, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("31"));
    EXPECT_EQ(m_bean.sets, 2);
}

TEST_F(NumberFieldTest, PageUpStepsTenTimes)
{
    // probe: PageUp (Qt: 10 steps; Java: no binding): radius=0.205 text=[41], ten setter calls.
    setRadius(0.155);
    focus(*m_spin);
    QTest::keyClick(m_spin, Qt::Key_PageUp);
    EXPECT_NEAR(m_bean.value, 0.205, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("41"));
    EXPECT_EQ(m_bean.sets, 10);
}

TEST_F(NumberFieldTest, TheWheelStepsOnlyAFocusedSpinBox)
{
    // probe: wheel without focus (ignored): radius=0.205; wheel with focus: radius=0.21 text=[42].
    // An ignored turn goes on to the widgets around the spin box (a scroll area scrolls); Qt
    // passes a turn on only when it comes from the platform, which a test's does not.
    setRadius(0.205);
    focus(*m_other);
    EXPECT_FALSE(turnWheel(*m_spin, 120));
    EXPECT_NEAR(m_bean.value, 0.205, kTolerance);
    EXPECT_EQ(m_bean.sets, 0);
    EXPECT_FALSE(m_spin->hasFocus());  // the wheel takes no focus

    focus(*m_spin);
    EXPECT_TRUE(turnWheel(*m_spin, 120));
    EXPECT_NEAR(m_bean.value, 0.21, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("42"));
    EXPECT_EQ(m_bean.sets, 1);
}

TEST_F(NumberFieldTest, EscRevertsAnEditAndKeepsTheDialogOpen)
{
    // probe: type 12, Esc (revert the edit): radius=0.21 text=[42] dialog=open.
    setRadius(0.21);
    typeInto(*m_spin, QStringLiteral("12"));
    QTest::keyClick(m_spin, Qt::Key_Escape);
    EXPECT_EQ(spinText(), QStringLiteral("42"));
    EXPECT_NEAR(m_bean.value, 0.21, kTolerance);
    EXPECT_EQ(m_bean.sets, 0);
    EXPECT_TRUE(m_dialog->isVisible());
    EXPECT_FALSE(editor().isModified());
}

TEST_F(NumberFieldTest, EscWithoutAnEditClosesTheDialog)
{
    // Java: JFormattedTextField's reset-field-edit is enabled only while the field is edited, so
    // an unedited spinner leaves Esc to the dialog.
    focus(*m_spin);
    QTest::keyClick(m_spin, Qt::Key_Escape);
    EXPECT_FALSE(m_dialog->isVisible());
    EXPECT_EQ(m_dialog->result(), QDialog::Rejected);
    EXPECT_EQ(m_bean.sets, 0);
}

TEST_F(NumberFieldTest, TheUnitChosenShowsTheValueInThatUnitOnly)
{
    // probe: unit -> in/64: radius=0.21 text=[16 17/32] (superscript and subscript digits).
    setRadius(0.21);
    const OffscreenPopupWarnings popup;
    QTest::mouseClick(m_unit, Qt::LeftButton);
    QMenu* const menu = m_unit->unitMenu();
    ASSERT_NE(menu, nullptr);
    ASSERT_TRUE(menu->isVisible());
    QAction* const inch64 = menuItem(*menu, QStringLiteral("in/64"));
    ASSERT_NE(inch64, nullptr);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(inch64).center());
    EXPECT_FALSE(menu->isVisible());
    EXPECT_EQ(&m_binding->getCurrentUnit(), &lengthUnit("in/64"));
    EXPECT_EQ(spinText(), QStringLiteral("16 \u00B9\u2077\u2044\u2083\u2082"));
    EXPECT_EQ(m_unit->text(), QStringLiteral("in/64"));
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    EXPECT_EQ(m_bean.sets, 0);
}

TEST_F(NumberFieldTest, AFractionIsReadInTheCurrentUnit)
{
    // probe: type 1 3/4, Enter: radius=0.022225 text=[1 3/4]; recommit the shown fraction text:
    // radius=0.022225, one more setter call; unit -> mm: text=[44.4].
    m_binding->setCurrentUnit(lengthUnit("in/64"));
    typeInto(*m_spin, QStringLiteral("1 3/4"));
    QTest::keyClick(m_spin, Qt::Key_Return);
    EXPECT_NEAR(m_bean.value, 1.75 * 0.0254 / 2, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("1 \u00B3\u2044\u2084"));
    EXPECT_EQ(m_bean.sets, 1);

    // The text the spin box shows reads back as the same value.
    editor().setModified(true);
    focus(*m_other);
    EXPECT_NEAR(m_bean.value, 1.75 * 0.0254 / 2, kTolerance);
    EXPECT_EQ(m_bean.sets, 2);

    m_binding->setCurrentUnit(lengthUnit("mm"));
    EXPECT_EQ(spinText(), QStringLiteral("44.4"));
    EXPECT_EQ(m_unit->text(), QStringLiteral("mm"));
}

TEST_F(NumberFieldTest, TheSliderSetsTheRoundedValueOfItsPosition)
{
    // probe (in mm): slider -> 500: radius=0.02 text=[40], the slider shows 500; slider -> 750:
    // radius=0.045 text=[90], it shows 750; one setter call each (no feedback loop).
    m_binding->setCurrentUnit(lengthUnit("mm"));
    m_slider->setValue(500);
    EXPECT_NEAR(m_bean.value, 0.02, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("40"));
    EXPECT_EQ(m_slider->value(), 500);
    EXPECT_EQ(m_bean.sets, 1);
    m_slider->setValue(750);
    EXPECT_NEAR(m_bean.value, 0.045, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("90"));
    EXPECT_EQ(m_slider->value(), 750);
    EXPECT_EQ(m_bean.sets, 2);
}

TEST_F(NumberFieldTest, LeftAndRightPutTheCursorAtTheEdgesOfASelection)
{
    // probe: Left with selection [1,4): cursor at 3 (Swing fix: 1); Right: 5 (Swing fix: 4).
    focus(*m_spin);
    editor().setText(QStringLiteral("12345"));
    editor().setSelection(1, 3);
    QTest::keyClick(m_spin, Qt::Key_Left);
    EXPECT_EQ(editor().cursorPosition(), 1);
    EXPECT_FALSE(editor().hasSelectedText());
    editor().setSelection(1, 3);
    QTest::keyClick(m_spin, Qt::Key_Right);
    EXPECT_EQ(editor().cursorPosition(), 4);
    EXPECT_FALSE(editor().hasSelectedText());
}

TEST_F(NumberFieldTest, TheFirstClickSelectsAllAndTheSecondPlacesTheCursor)
{
    // probe: first click into the spinner: selected=[90] of [90]; second click: selected=[].
    focus(*m_other);
    const QPoint left(3, editor().height() / 2);
    QTest::mouseClick(&editor(), Qt::LeftButton, Qt::NoModifier, left);
    QCoreApplication::processEvents();
    EXPECT_TRUE(m_spin->hasFocus());
    EXPECT_EQ(editor().selectedText(), QStringLiteral("2.5"));
    QTest::mouseClick(&editor(), Qt::LeftButton, Qt::NoModifier, left);
    QCoreApplication::processEvents();
    EXPECT_EQ(editor().selectedText(), QString());
}

TEST_F(NumberFieldTest, BacktabIntoTheSpinBoxSelectsAll)
{
    // probe: Backtab into the spinner: focus=spin selected=[90].
    focus(*m_other);
    QTest::keyClick(m_other, Qt::Key_Backtab);
    QCoreApplication::processEvents();
    EXPECT_TRUE(m_spin->hasFocus());
    EXPECT_EQ(editor().selectedText(), QStringLiteral("2.5"));
}

TEST_F(NumberFieldTest, AClickOnAButtonThatTakesTheFocusCommitsTheEdit)
{
    // probe (in mm): type 44, click OK: radius=0.022 dialog=ACCEPTED (the button took the focus:
    // Qt::StrongFocus, as on Linux and Windows).
    m_binding->setCurrentUnit(lengthUnit("mm"));
    okButton().setFocusPolicy(Qt::StrongFocus);
    recordValueAtAccept();
    typeInto(*m_spin, QStringLiteral("44"));
    QTest::mouseClick(&okButton(), Qt::LeftButton);
    EXPECT_NEAR(m_valueAtAccept, 0.022, kTolerance);
    EXPECT_EQ(m_dialog->result(), QDialog::Accepted);
}

TEST_F(NumberFieldTest, AClickOnAButtonWithoutFocusLeavesTheEditToCommitEdit)
{
    // macOS's buttons take no focus from a click (Qt::TabFocus): the edit is still pending when
    // the button acts, which is why the dialog commits it itself first (commitEdit(): BindingScope
    // before OK, Close, a save and an undo).
    m_binding->setCurrentUnit(lengthUnit("mm"));
    QPushButton& button = addButtonWithoutFocus();
    typeInto(*m_spin, QStringLiteral("44"));
    QTest::mouseClick(&button, Qt::LeftButton);
    EXPECT_EQ(m_valueAtClick, 0.0125);
    EXPECT_TRUE(m_spin->hasFocus());
    EXPECT_EQ(spinText(), QStringLiteral("44"));

    EXPECT_TRUE(m_spin->commitEdit());
    EXPECT_NEAR(m_bean.value, 0.022, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("44"));
    EXPECT_FALSE(m_spin->commitEdit());  // nothing left to commit
    EXPECT_EQ(m_bean.sets, 1);
}

TEST_F(NumberFieldTest, AtTheMaximumTheUpKeyDoesNothing)
{
    // probe (in mm): at max: next=null previous=999.
    m_binding->setCurrentUnit(lengthUnit("mm"));
    setRadius(0.5);
    EXPECT_EQ(spinText(), QStringLiteral("1000"));
    EXPECT_FALSE(m_binding->nextValue().has_value());
    EXPECT_NEAR(m_binding->previousValue().value_or(0), 999, 1e-9);
    focus(*m_spin);
    QTest::keyClick(m_spin, Qt::Key_Up);
    EXPECT_NEAR(m_bean.value, 0.5, kTolerance);
    EXPECT_EQ(m_bean.sets, 0);
    QTest::keyClick(m_spin, Qt::Key_Down);
    EXPECT_NEAR(m_bean.value, 0.4995, kTolerance);
    EXPECT_EQ(spinText(), QStringLiteral("999"));
}

}  // namespace

}  // namespace QtRocket::Gui
