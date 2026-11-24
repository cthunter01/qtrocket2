// The validation of a field (OpenRocket's FlatLafOutlines.Validator): the most severe condition
// that holds outlines the field, its messages replace the tooltip, and a transient popup shows
// them when the field becomes invalid or its message changes; none of it stays behind.

#include "QtRocket/gui/widgets/FieldValidator.h"

#include <memory>
#include <vector>

#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPointer>
#include <QRegion>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"
#include "WidgetTestSupport.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using FieldValidatorTest = Test::GuiTest;
using Outline            = FieldValidator::Outline;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Test::showActive;
using Test::showActiveWith;

/// What a validator's conditions read: switched by the test.
struct Inputs
{
    bool warning{false};
    bool error{false};
    bool secondError{false};
};

/// A condition that holds while the flag it reads is set.
struct Holds
{
    const bool* flag;

    [[nodiscard]] bool operator()() const { return *flag; }
};

/// A condition that always holds.
struct Always
{
    [[nodiscard]] bool operator()() const { return true; }
};

/// A message: the text it reads.
struct TextOf
{
    const QString* text;

    [[nodiscard]] QString operator()() const { return *text; }
};

/// A condition that holds while a binding's value is at least the limit.
struct AtLeast
{
    const DoubleBinding* binding;
    double               limit;

    [[nodiscard]] bool operator()() const { return binding->value() >= limit; }
};

/// Whether a validator's popup has gone.
struct PopupGone
{
    const FieldValidator* validator;

    [[nodiscard]] bool operator()() const { return validator->messagePopup() == nullptr; }
};

/// The average wind speed's validator (SimulationConditionsPanel.java:557): an error from 20
/// m/s, a warning from 10.
void validateWindSpeed(FieldValidator& validator, const DoubleBinding& speed)
{
    validator
        .errorIf(AtLeast{.binding = &speed, .limit = 20},
                 QStringLiteral("Extremely high wind speed"))
        .warnIf(AtLeast{.binding = &speed, .limit = 10}, QStringLiteral("High wind speed"));
}

/// A light and a dark palette (ThemeColors::theme() tells them apart by the window's text).
[[nodiscard]] QPalette themePalette(bool dark)
{
    QPalette palette;
    palette.setColor(QPalette::Window, dark ? QColor(0x3c, 0x3f, 0x41) : QColor(Qt::white));
    palette.setColor(QPalette::WindowText, dark ? QColor(0xbb, 0xbb, 0xbb) : QColor(Qt::black));
    return palette;
}

/// The child that draws @p field's outline; null when it has none.
[[nodiscard]] QWidget* outlineFrameOf(const QWidget& field)
{
    return field.findChild<QWidget*>(QStringLiteral("fieldOutline"), Qt::FindDirectChildrenOnly);
}

/// What the outline frame of @p field paints (without its children and background).
[[nodiscard]] std::vector<RecordedPaint> paintedOutline(const QWidget& field)
{
    QWidget* frame = outlineFrameOf(field);
    if (frame == nullptr)
    {
        ADD_FAILURE() << "the field has no outline frame";
        return {};
    }
    RecordingPaintDevice device(frame->size());
    frame->render(&device, QPoint(), QRegion(), {});
    return device.records();
}

/// Deletes the popups and outline frames given to deleteLater().
void deleteLaterNow()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

}  // namespace

TEST_F(FieldValidatorTest, TheMostSevereConditionOutlinesTheField)
{
    QLineEdit field;
    field.setToolTip(QStringLiteral("The field's own tip"));
    Inputs          inputs;
    FieldValidator& validator = FieldValidator::of(field);
    EXPECT_EQ(validator.parent(), &field);
    validator.warnIf(Holds{&inputs.warning}, QStringLiteral("A warning"))
        .errorIf(Holds{&inputs.error}, QStringLiteral("An error"))
        .errorIf(Holds{&inputs.secondError}, QStringLiteral("Another error"));
    validator.update();
    EXPECT_EQ(validator.outline(), Outline::NONE);
    EXPECT_EQ(FieldValidator::outlineOf(field), Outline::NONE);
    EXPECT_TRUE(validator.message().isEmpty());
    EXPECT_EQ(field.toolTip(), QStringLiteral("The field's own tip"));

    inputs.warning = true;
    validator.update();
    EXPECT_EQ(validator.outline(), Outline::WARNING);
    EXPECT_EQ(FieldValidator::outlineOf(field), Outline::WARNING);
    EXPECT_EQ(validator.message(), QStringLiteral("A warning"));
    EXPECT_EQ(field.toolTip(), QStringLiteral("<html>A warning</html>"));

    // An error outranks the warning, whose message goes; two errors give both messages.
    inputs.error       = true;
    inputs.secondError = true;
    validator.update();
    EXPECT_EQ(validator.outline(), Outline::ERROR);
    EXPECT_EQ(validator.message(), QStringLiteral("An error\nAnother error"));
    EXPECT_EQ(field.toolTip(), QStringLiteral("<html>An error<br>Another error</html>"));

    inputs = Inputs{};
    validator.update();
    EXPECT_EQ(FieldValidator::outlineOf(field), Outline::NONE);
    EXPECT_EQ(field.toolTip(), QStringLiteral("The field's own tip"));
}

TEST_F(FieldValidatorTest, MessagesAreTrimmedEscapedOrTakenAsHtml)
{
    QLineEdit       field;
    QString         message   = QStringLiteral("  1 < 2 & \"it's\"\t ");
    FieldValidator& validator = FieldValidator::of(field).warnIf(Always{}, TextOf{&message});
    validator.update();
    EXPECT_EQ(validator.message(), QStringLiteral("1 < 2 & \"it's\""));
    // StringUtils.escapeHtml's escapes.
    EXPECT_EQ(field.toolTip(), QStringLiteral("<html>1 &lt; 2 &amp; &quot;it&#39;s&quot;</html>"));

    message = QStringLiteral("<HTML><b>bold</b></HTML>");
    validator.update();
    EXPECT_EQ(field.toolTip(), message);

    // A blank message shows nothing: the outline stays, the tooltip is the field's (none).
    message = QStringLiteral(" \t ");
    validator.update();
    EXPECT_EQ(validator.outline(), Outline::WARNING);
    EXPECT_TRUE(validator.message().isEmpty());
    EXPECT_TRUE(field.toolTip().isEmpty());

    // A condition without a message outlines the field without a tooltip.
    QLineEdit plain;
    FieldValidator::of(plain).errorIf(Always{}).update();
    EXPECT_EQ(FieldValidator::outlineOf(plain), Outline::ERROR);
    EXPECT_TRUE(plain.toolTip().isEmpty());
}

TEST_F(FieldValidatorTest, AConditionNeedsAPredicate)
{
    QLineEdit       field;
    FieldValidator& validator = FieldValidator::of(field);
    EXPECT_THROW(validator.warnIf(nullptr), BugError);
    EXPECT_THROW(validator.errorIf(nullptr, QStringLiteral("x")), BugError);
    EXPECT_THROW(static_cast<void>(FieldValidator(nullptr)), BugError);
}

TEST_F(FieldValidatorTest, ItUpdatesOnTheChangesOfWhatItListensTo)
{
    // SimulationConditionsPanel's average wind speed: listenTo(the speed's model).
    QLineEdit       field;
    const auto      speed     = std::make_shared<DoubleBinding>(5.0);
    FieldValidator& validator = FieldValidator::of(field);
    validateWindSpeed(validator, *speed);
    validator.listenTo(*speed);  // updates at once
    EXPECT_EQ(validator.outline(), Outline::NONE);
    speed->setValue(12);
    EXPECT_EQ(validator.outline(), Outline::WARNING);
    EXPECT_EQ(validator.message(), QStringLiteral("High wind speed"));
    speed->setValue(25);
    EXPECT_EQ(validator.outline(), Outline::ERROR);
    EXPECT_EQ(validator.message(), QStringLiteral("Extremely high wind speed"));
    validator.listenTo(*speed);  // a second time: skipped

    // A core object's signal.
    Signal<>        changed;
    bool            invalid = false;
    QLineEdit       other;
    FieldValidator& signalled =
        FieldValidator::of(other).errorIf(Holds{&invalid}).listenTo(changed);
    signalled.listenTo(changed);
    EXPECT_EQ(changed.size(), 1U);
    invalid = true;
    changed.emit();
    EXPECT_EQ(signalled.outline(), Outline::ERROR);
}

TEST_F(FieldValidatorTest, ThePopupShowsWhenTheFieldBecomesInvalidOrItsMessageChanges)
{
    QWidget         window;
    auto*           field     = makeChild<QLineEdit>(&window);
    QString         message   = QStringLiteral("Enter a signed 32-bit integer.");
    bool            invalid   = false;
    FieldValidator& validator = FieldValidator::of(*field)
                                    .errorIf(Holds{&invalid}, TextOf{&message})
                                    .showMessagePopup(FieldValidator::kMessagePopupDuration);
    EXPECT_EQ(validator.messagePopupDuration(), 2500);
    ASSERT_TRUE(showActiveWith(window, {field}));
    window.resize(400, 300);
    validator.update();
    EXPECT_EQ(validator.messagePopup(), nullptr);

    invalid = true;
    validator.update();
    const QLabel* popup = validator.messagePopup();
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(popup->isVisible());
    // A child of the field's window, on top, letting the mouse through: no window of its own.
    EXPECT_FALSE(popup->isWindow());
    EXPECT_EQ(popup->parent(), &window);
    EXPECT_TRUE(popup->testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_EQ(window.children().last(), popup);
    EXPECT_EQ(popup->text(), QStringLiteral("<html>Enter a signed 32-bit integer.</html>"));
    EXPECT_EQ(popup->pos(), field->mapTo(&window, QPoint(0, field->height() + 2)));
    const auto* timer = validator.findChild<QTimer*>();
    ASSERT_NE(timer, nullptr);
    EXPECT_TRUE(timer->isActive());
    EXPECT_EQ(timer->interval(), 2500);

    // The same message again: the popup is not shown again.
    validator.update();
    EXPECT_EQ(validator.messagePopup(), popup);

    // Another message: a new popup.
    message = QStringLiteral("Another message");
    validator.update();
    deleteLaterNow();
    ASSERT_NE(validator.messagePopup(), nullptr);
    EXPECT_EQ(validator.messagePopup()->text(), QStringLiteral("<html>Another message</html>"));

    // Valid again: it goes at once, and is deleted.
    invalid = false;
    validator.update();
    EXPECT_EQ(validator.messagePopup(), nullptr);
    EXPECT_FALSE(timer->isActive());
    deleteLaterNow();
    EXPECT_EQ(window.findChild<QLabel*>(), nullptr);
}

TEST_F(FieldValidatorTest, ThePopupStaysInsideTheWindow)
{
    // A field at the bottom right of its window: the popup goes above it, moved left.
    QWidget window;
    window.resize(300, 200);
    auto*           field = makeChild<QLineEdit>(&window);
    FieldValidator& validator =
        FieldValidator::of(*field)
            .warnIf(Always{}, QStringLiteral("A message wider than the field is"))
            .showMessagePopup(FieldValidator::kMessagePopupDuration);
    field->setGeometry(250, 170, 50, 24);
    ASSERT_TRUE(showActive(window));
    validator.update();
    const QLabel* popup = validator.messagePopup();
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(window.rect().contains(popup->geometry()));
    EXPECT_EQ(popup->geometry().bottom(), field->y() - 2 - 1);
    EXPECT_EQ(popup->x(), window.width() - popup->width());
    validator.hideMessagePopup();
    deleteLaterNow();
}

TEST_F(FieldValidatorTest, ThePopupGoesWhenItsTimeIsUp)
{
    QWidget         window;
    auto*           field     = makeChild<QSpinBox>(&window);
    FieldValidator& validator = FieldValidator::of(*field)
                                    .warnIf(Always{}, QStringLiteral("Soon gone"))
                                    .showMessagePopup(1);
    ASSERT_TRUE(showActiveWith(window, {field}));
    validator.update();
    ASSERT_NE(validator.messagePopup(), nullptr);
    EXPECT_TRUE(QTest::qWaitFor(PopupGone{&validator}));
    deleteLaterNow();
    EXPECT_EQ(window.findChild<QLabel*>(QStringLiteral("messagePopup")), nullptr);
    // The outline and the tooltip stay while the condition holds.
    EXPECT_EQ(FieldValidator::outlineOf(*field), Outline::WARNING);
    EXPECT_EQ(field->toolTip(), QStringLiteral("<html>Soon gone</html>"));
}

TEST_F(FieldValidatorTest, NoPopupWithoutADurationOrWhileTheFieldIsHidden)
{
    QWidget         window;
    auto*           field  = makeChild<QLineEdit>(&window);
    auto*           silent = makeChild<QLineEdit>(&window);
    FieldValidator& shown  = FieldValidator::of(*field)
                                 .errorIf(Always{}, QStringLiteral("Hidden field"))
                                 .showMessagePopup(FieldValidator::kMessagePopupDuration);
    FieldValidator& quiet =
        FieldValidator::of(*silent).errorIf(Always{}, QStringLiteral("No popup"));
    shown.update();  // the window is not shown yet (Java: isShowing())
    EXPECT_EQ(shown.messagePopup(), nullptr);
    EXPECT_EQ(shown.outline(), Outline::ERROR);
    ASSERT_TRUE(showActiveWith(window, {field, silent}));
    shown.update();  // the state is the same: no popup now either
    EXPECT_EQ(shown.messagePopup(), nullptr);
    quiet.update();
    EXPECT_EQ(quiet.messagePopup(), nullptr);
    EXPECT_EQ(quiet.messagePopupDuration(), 0);
    EXPECT_EQ(quiet.showMessagePopup(-5).messagePopupDuration(), 0);
}

TEST_F(FieldValidatorTest, HidingTheFieldOrHideMessagePopupEndsThePopup)
{
    QWidget         window;
    auto*           field     = makeChild<QLineEdit>(&window);
    QString         message   = QStringLiteral("First");
    FieldValidator& validator = FieldValidator::of(*field)
                                    .warnIf(Always{}, TextOf{&message})
                                    .showMessagePopup(FieldValidator::kMessagePopupDuration);
    ASSERT_TRUE(showActiveWith(window, {field}));
    validator.update();
    ASSERT_NE(validator.messagePopup(), nullptr);
    field->hide();
    EXPECT_EQ(validator.messagePopup(), nullptr);

    field->show();
    message = QStringLiteral("Second");
    validator.update();
    ASSERT_NE(validator.messagePopup(), nullptr);
    validator.hideMessagePopup();
    EXPECT_EQ(validator.messagePopup(), nullptr);
    validator.hideMessagePopup();  // again: nothing
    deleteLaterNow();
}

TEST_F(FieldValidatorTest, ClosingGivesTheToolTipBackAndStopsListening)
{
    QWidget         window;
    auto*           field = makeChild<QLineEdit>(&window);
    Signal<>        changed;
    bool            invalid   = true;
    FieldValidator& validator = FieldValidator::of(*field)
                                    .errorIf(Holds{&invalid}, QStringLiteral("Wrong"))
                                    .showMessagePopup(FieldValidator::kMessagePopupDuration);
    field->setToolTip(QStringLiteral("Own"));
    ASSERT_TRUE(showActiveWith(window, {field}));
    validator.listenTo(changed);
    ASSERT_NE(validator.messagePopup(), nullptr);
    EXPECT_EQ(field->toolTip(), QStringLiteral("<html>Wrong</html>"));

    validator.close();
    EXPECT_TRUE(validator.isClosed());
    EXPECT_EQ(validator.messagePopup(), nullptr);
    EXPECT_EQ(field->toolTip(), QStringLiteral("Own"));
    EXPECT_TRUE(changed.empty());
    // The outline stays, as in Java; updates do nothing.
    EXPECT_EQ(FieldValidator::outlineOf(*field), Outline::ERROR);
    invalid = false;
    validator.update();
    EXPECT_EQ(validator.outline(), Outline::ERROR);
    validator.close();  // again: nothing
    deleteLaterNow();
}

TEST_F(FieldValidatorTest, AValidatorAndItsPopupGoWithTheField)
{
    auto            window = std::make_unique<QWidget>();
    auto*           field  = makeChild<QLineEdit>(window.get());
    Signal<>        changed;
    const auto      speed     = std::make_shared<DoubleBinding>(25.0);
    FieldValidator& validator = FieldValidator::of(*field);
    validateWindSpeed(validator, *speed);
    validator.showMessagePopup(FieldValidator::kMessagePopupDuration);
    ASSERT_TRUE(showActiveWith(*window, {field}));
    validator.listenTo(changed).listenTo(*speed);
    ASSERT_NE(validator.messagePopup(), nullptr);

    // The field alone: its validator goes, and takes the popup, the window's child, along.
    const QPointer<QLabel> popup = validator.messagePopup();
    field->deleteLater();
    deleteLaterNow();
    EXPECT_TRUE(changed.empty());
    speed->setValue(30);  // reaches no destroyed validator (ASan)
    deleteLaterNow();
    EXPECT_TRUE(popup.isNull());
    window.reset();
}

TEST_F(FieldValidatorTest, TheOutlineColoursAreOpenRocketsThemes)
{
    // FlatLaf 3.7.2's Component.warning/error borderColor and focusedBorderColor under
    // FlatLightLaf and FlatDarculaLaf.
    const QPalette light = themePalette(false);
    const QPalette dark  = themePalette(true);
    EXPECT_EQ(FieldValidator::outlineColor(Outline::WARNING, false, light),
              QColor(0xfe, 0xd2, 0x84));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::WARNING, true, light),
              QColor(0xe2, 0xa5, 0x3a));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::ERROR, false, light), QColor(0xeb, 0xb8, 0xbc));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::ERROR, true, light), QColor(0xe5, 0x3e, 0x4d));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::WARNING, false, dark),
              QColor(0x72, 0x56, 0x27));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::WARNING, true, dark), QColor(0xac, 0x79, 0x20));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::ERROR, false, dark), QColor(0x72, 0x55, 0x55));
    EXPECT_EQ(FieldValidator::outlineColor(Outline::ERROR, true, dark), QColor(0x8b, 0x3c, 0x3c));
    EXPECT_FALSE(FieldValidator::outlineColor(Outline::NONE, false, light).isValid());
}

TEST_F(FieldValidatorTest, AnOutlineIsAFrameOverTheFieldsEdge)
{
    // Without a validator, as the CSV import dialog sets them (Java: FlatLafOutlines.setOutline()).
    QSpinBox spin;
    spin.resize(120, 30);
    EXPECT_EQ(outlineFrameOf(spin), nullptr);
    FieldValidator::setOutline(spin, Outline::ERROR);
    EXPECT_EQ(FieldValidator::outlineOf(spin), Outline::ERROR);
    const QWidget* frame = outlineFrameOf(spin);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->geometry(), spin.rect());
    EXPECT_TRUE(frame->testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_EQ(frame->focusPolicy(), Qt::NoFocus);
    EXPECT_TRUE(frame->isVisibleTo(&spin));

    // A spin box's frame has rounded corners (a path); unfocused, 1 pixel in the border colour.
    const std::vector<RecordedPaint> spinPaint = paintedOutline(spin);
    ASSERT_EQ(spinPaint.size(), 1U);
    EXPECT_EQ(spinPaint.front().kind, RecordedPaint::Kind::PATH);
    EXPECT_EQ(spinPaint.front().pen.color(),
              FieldValidator::outlineColor(Outline::ERROR, false, spin.palette()));
    EXPECT_EQ(spinPaint.front().pen.widthF(), 1.0);

    // A line edit's is a rectangle.
    QLineEdit line;
    line.resize(100, 24);
    FieldValidator::setOutline(line, Outline::WARNING);
    const std::vector<RecordedPaint> linePaint = paintedOutline(line);
    ASSERT_EQ(linePaint.size(), 1U);
    EXPECT_EQ(linePaint.front().kind, RecordedPaint::Kind::RECT);
    EXPECT_EQ(linePaint.front().pen.color(),
              FieldValidator::outlineColor(Outline::WARNING, false, line.palette()));

    FieldValidator::setOutline(spin, Outline::NONE);
    EXPECT_EQ(FieldValidator::outlineOf(spin), Outline::NONE);
    EXPECT_FALSE(frame->isVisibleTo(&spin));
    FieldValidator::setOutline(spin, Outline::WARNING);
    EXPECT_EQ(outlineFrameOf(spin), frame);  // the same frame again
    EXPECT_TRUE(frame->isVisibleTo(&spin));
}

TEST_F(FieldValidatorTest, AFocusedFieldsOutlineIsWiderInTheFocusedColour)
{
    QWidget window;
    auto*   field = makeChild<QSpinBox>(&window);
    auto*   other = makeChild<QLineEdit>(&window);
    ASSERT_TRUE(showActiveWith(window, {field, other}));
    FieldValidator::setOutline(*field, Outline::ERROR);
    field->setFocus();
    ASSERT_TRUE(field->hasFocus());
    const std::vector<RecordedPaint> focused = paintedOutline(*field);
    ASSERT_EQ(focused.size(), 1U);
    EXPECT_EQ(focused.front().pen.color(),
              FieldValidator::outlineColor(Outline::ERROR, true, field->palette()));
    EXPECT_EQ(focused.front().pen.widthF(), 2.0);

    other->setFocus();
    const std::vector<RecordedPaint> unfocused = paintedOutline(*field);
    ASSERT_EQ(unfocused.size(), 1U);
    EXPECT_EQ(unfocused.front().pen.widthF(), 1.0);

    // The frame follows the field's size.
    field->resize(field->width() + 10, field->height() + 2);
    const QWidget* frame = outlineFrameOf(*field);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->geometry(), field->rect());
}

}  // namespace QtRocket::Gui
