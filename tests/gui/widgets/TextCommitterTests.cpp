// The text fields of a dialog: the component name field and the comment area (Java: the private
// TextFieldListener of RocketComponentConfig and RocketConfig, and gui/TextFieldListener).
//
// The first five tests port the behaviour of TextFieldListenerTest
// (swing/src/test/java/info/openrocket/swing/gui/TextFieldListenerTest.java): its listener is a
// committer here, its RecordingListener's setText() a property that records the text it is given
// (StringBeanProperty), listenTo(field) is addField() and listenTo(another field or null) is
// removeField(); Java calls the listener's actionPerformed() and focusLost() directly, these
// tests press Enter and move the focus.

#include "QtRocket/gui/widgets/TextCommitter.h"

#include <array>
#include <memory>
#include <tuple>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QFocusEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/SelectionKeyFilter.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using TextCommitterTest = Test::GuiTest;
using Test::OffscreenPopupWarnings;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::StringBeanProperty;
using Test::TwoTubeRocket;

/// A binding of @p source on @p component alone.
[[nodiscard]] std::shared_ptr<StringBinding> bindOn(RocketComponent&    component,
                                                    const StringSource& source)
{
    return std::make_shared<StringBinding>(std::make_shared<ComponentSelection>(component), source);
}

/// Sends @p edit a focus event of @p type, as the focus moving does.
void sendFocus(QWidget& edit, QEvent::Type type)
{
    QFocusEvent event(type, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(&edit, &event);
}

/// Opens the context menu of a field as a right click does, sent to @p receiver (a line edit
/// itself, a text area's viewport): the popup shown, or null.
[[nodiscard]] QWidget* openContextMenu(QWidget& receiver)
{
    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(5, 5),
                            receiver.mapToGlobal(QPoint(5, 5)));
    QCoreApplication::sendEvent(&receiver, &event);
    return QApplication::activePopupWidget();
}

/// A window with a name field, a comment area and a line edit to take the focus, shown active,
/// and a committer of the name and the comment of tube A.
class NameAndComment
{
public:
    explicit NameAndComment(QWidget& window, const TwoTubeRocket& design)
      : m_selection(std::make_shared<ComponentSelection>(*design.tubeA)),
        name(makeChild<QLineEdit>(&window)),
        comment(makeChild<QPlainTextEdit>(&window)),
        other(makeChild<QLineEdit>(&window)),
        committer(makeChild<TextCommitter>(&window)),
        active(showActiveWith(window, {name, comment, other}))
    {
        committer->addField(*name,
                            std::make_shared<StringBinding>(m_selection, componentNameSource()));
        committer->addField(*comment,
                            std::make_shared<StringBinding>(m_selection, componentCommentSource()));
    }

private:
    std::shared_ptr<ComponentSelection> m_selection;

public:
    QLineEdit*      name{nullptr};
    QPlainTextEdit* comment{nullptr};
    QLineEdit*      other{nullptr};
    TextCommitter*  committer{nullptr};
    bool            active{false};
};

// ------------------------------------------------------- TextFieldListenerTest's behaviour

TEST_F(TextCommitterTest, EnterPassesTheCurrentText)
{
    // Port of TextFieldListenerTest.actionPerformedPassesCurrentText (behaviour).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "initial"};
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, bean.source()));
    EXPECT_EQ(field.text(), QStringLiteral("initial"));
    field.setText(QStringLiteral("trigger"));
    QTest::keyClick(&field, Qt::Key_Return);
    EXPECT_EQ(bean.value, "trigger");
    EXPECT_TRUE(committer.hasField(field));  // Java: the action and focus listeners are on it
}

TEST_F(TextCommitterTest, AFocusLossAlsoPassesTheText)
{
    // Port of TextFieldListenerTest.focusLostAlsoPropagatesText (behaviour).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "focus"};
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, bean.source()));
    field.setText(QStringLiteral("blurred"));
    sendFocus(field, QEvent::FocusOut);
    EXPECT_EQ(bean.value, "blurred");
}

TEST_F(TextCommitterTest, SwitchingFieldsLeavesThePreviousFieldAlone)
{
    // Port of TextFieldListenerTest.switchingFieldsRemovesListenersFromPreviousField
    // (behaviour): listenTo(first), then listenTo(second).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "first"};
    QLineEdit           first;
    QLineEdit           second;
    TextCommitter       committer(nullptr);
    committer.addField(first, bindOn(*design.tubeA, bean.source()));
    committer.removeField(first);
    committer.addField(second, bindOn(*design.tubeA, bean.source()));
    EXPECT_FALSE(committer.hasField(first));
    EXPECT_TRUE(committer.hasField(second));

    first.setText(QStringLiteral("from first"));
    QTest::keyClick(&first, Qt::Key_Return);
    sendFocus(first, QEvent::FocusOut);
    EXPECT_EQ(bean.sets, 0);
    second.setText(QStringLiteral("from second"));
    QTest::keyClick(&second, Qt::Key_Return);
    EXPECT_EQ(bean.value, "from second");
}

TEST_F(TextCommitterTest, AFocusGainChangesNothing)
{
    // Port of TextFieldListenerTest.focusGainedDoesNotMutateState (behaviour).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "value"};
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, bean.source()));
    bean.value = "existing";
    sendFocus(field, QEvent::FocusIn);
    EXPECT_EQ(bean.value, "existing");
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(TextCommitterTest, ARemovedFieldIsLeftAlone)
{
    // Port of TextFieldListenerTest.listenToNullDetachesListeners (behaviour).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "value"};
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, bean.source()));
    committer.removeField(field);
    EXPECT_FALSE(committer.hasField(field));
    field.setText(QStringLiteral("edited"));
    QTest::keyClick(&field, Qt::Key_Return);
    sendFocus(field, QEvent::FocusOut);
    EXPECT_TRUE(committer.commit() == false);
    EXPECT_EQ(bean.sets, 0);
    EXPECT_EQ(bean.value, "value");
}

// ------------------------------------------------------------------------------ the rest

TEST_F(TextCommitterTest, TheNameFieldAlsoCommitsTheComment)
{
    // Java: RocketComponentConfig's setName() sets the name and the comment, when they differ.
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    ASSERT_TRUE(fields.active);
    EXPECT_EQ(fields.name->text(), QStringLiteral("A"));
    fields.comment->setFocus();
    QTest::keyClicks(fields.comment, QStringLiteral("glued"));
    fields.name->setFocus();  // the comment's focus loss commits both: only the comment changed
    EXPECT_EQ(design.tubeA->getComment(), "glued");
    EXPECT_EQ(design.tubeA->getName(), "A");

    fields.name->selectAll();
    QTest::keyClicks(fields.name, QStringLiteral("Main tube"));
    fields.comment->setPlainText(QStringLiteral("line 1\nline 2"));
    QTest::keyClick(fields.name, Qt::Key_Return);
    EXPECT_EQ(design.tubeA->getName(), "Main tube");
    EXPECT_EQ(design.tubeA->getComment(), "line 1\nline 2");
    EXPECT_TRUE(fields.name->hasFocus());  // Enter keeps the focus
}

TEST_F(TextCommitterTest, EnterInTheNameFieldLeavesTheDialogOpen)
{
    // Java's text field takes Enter while it has an action listener: the dialog's default button
    // is not pressed.
    const TwoTubeRocket  design;
    QDialog              dialog;
    NameAndComment const fields(dialog, design);
    ASSERT_TRUE(fields.active);
    fields.name->setFocus();
    fields.name->selectAll();
    QTest::keyClicks(fields.name, QStringLiteral("Nose"));
    QTest::keyClick(fields.name, Qt::Key_Return);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.result(), QDialog::Rejected);  // neither accepted nor rejected yet
    EXPECT_EQ(design.tubeA->getName(), "Nose");

    // Enter in the comment area is a new line.
    fields.comment->setFocus();
    QTest::keyClicks(fields.comment, QStringLiteral("a"));
    QTest::keyClick(fields.comment, Qt::Key_Return);
    QTest::keyClicks(fields.comment, QStringLiteral("b"));
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(design.tubeA->getComment(), "");
    fields.other->setFocus();
    EXPECT_EQ(design.tubeA->getComment(), "a\nb");
}

TEST_F(TextCommitterTest, EscIsLeftToTheDialog)
{
    // Java: neither the name field nor the comment area handles Esc: the component dialog closes
    // (keeping the changes), and the focus loss commits.
    const TwoTubeRocket  design;
    QDialog              dialog;
    NameAndComment const fields(dialog, design);
    ASSERT_TRUE(fields.active);
    fields.name->setFocus();
    fields.name->selectAll();
    QTest::keyClicks(fields.name, QStringLiteral("Booster"));
    QTest::keyClick(fields.name, Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(fields.name->text(), QStringLiteral("Booster"));  // not reverted
    EXPECT_EQ(design.tubeA->getName(), "Booster");
}

TEST_F(TextCommitterTest, AnUneditedFieldCommitsNothing)
{
    // Tabbing through the fields sets nothing (Java compares the texts: the same).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "same"};
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, bean.source()));
    sendFocus(field, QEvent::FocusOut);
    QTest::keyClick(&field, Qt::Key_Return);
    EXPECT_FALSE(committer.commit());
    EXPECT_EQ(bean.sets, 0);

    // Typed back to what it was: the binding skips the same text (Java's equals()).
    field.setText(QStringLiteral("other"));
    field.setText(QStringLiteral("same"));
    EXPECT_FALSE(committer.commit());
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(TextCommitterTest, ABlankNameShowsTheDefaultName)
{
    const TwoTubeRocket design;
    QLineEdit           field;
    TextCommitter       committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, componentNameSource()));
    field.setText(QStringLiteral("   "));
    EXPECT_TRUE(committer.commit());
    EXPECT_EQ(design.tubeA->getName(), "Body Tube");
    EXPECT_EQ(field.text(), QStringLiteral("Body Tube"));

    // A blank name for a component whose name is the default already changes nothing it shows:
    // the field shows the name again all the same.
    field.setText(QStringLiteral(" "));
    static_cast<void>(committer.commit());
    EXPECT_EQ(field.text(), QStringLiteral("Body Tube"));
}

TEST_F(TextCommitterTest, AMultiEditRenamesEverySelectedComponent)
{
    // Java's setName() forwards the name to every config listener.
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(
        *design.tubeA, std::array<RocketComponent*, 1>{design.tubeB});
    QLineEdit     field;
    TextCommitter committer(nullptr);
    committer.addField(field, std::make_shared<StringBinding>(selection, componentNameSource()));
    field.setText(QStringLiteral("Tube"));
    EXPECT_TRUE(committer.commit());
    EXPECT_EQ(design.tubeA->getName(), "Tube");
    EXPECT_EQ(design.tubeB->getName(), "Tube");
}

TEST_F(TextCommitterTest, AChangeElsewhereIsShownUnlessTheFieldHoldsAnEdit)
{
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    ASSERT_TRUE(fields.active);
    design.tubeA->setName("Renamed");
    design.tubeA->setComment("noted");
    EXPECT_EQ(fields.name->text(), QStringLiteral("Renamed"));
    EXPECT_EQ(fields.comment->toPlainText(), QStringLiteral("noted"));

    fields.name->setText(QStringLiteral("typing"));
    design.tubeA->setName("Elsewhere");
    EXPECT_EQ(fields.name->text(), QStringLiteral("typing"));
    EXPECT_TRUE(fields.committer->commit());
    EXPECT_EQ(design.tubeA->getName(), "typing");
}

TEST_F(TextCommitterTest, TheFieldsGetTheSelectionKeyFilterAndTabLeavesTheCommentArea)
{
    // Java: TextComponentSelectionKeyListener on both, GUIUtil.setTabToFocusing() on the area.
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    EXPECT_NE(fields.name->findChild<SelectionKeyFilter*>(), nullptr);
    EXPECT_NE(fields.comment->findChild<SelectionKeyFilter*>(), nullptr);
    EXPECT_TRUE(fields.comment->tabChangesFocus());

    // A field added again keeps one filter.
    fields.committer->addField(*fields.name, bindOn(*design.tubeA, componentNameSource()));
    EXPECT_EQ(fields.name->findChildren<SelectionKeyFilter*>().size(), 1);
}

TEST_F(TextCommitterTest, TheFieldsOwnContextMenuLeavesTheEditPending)
{
    // Qt's text fields have a context menu, which takes the focus (Qt::PopupFocusReason); Java's
    // have none. The edit stays pending while it is open (Qt's rule for editingFinished()), so
    // that a stage's new name (a TREE change) does not close the dialog under the menu, and the
    // next focus loss or commit() commits it.
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    ASSERT_TRUE(fields.active);
    const OffscreenPopupWarnings warnings;
    fields.name->setFocus();
    fields.name->selectAll();
    QTest::keyClicks(fields.name, QStringLiteral("Nose"));
    QWidget* const nameMenu = openContextMenu(*fields.name);
    ASSERT_NE(nameMenu, nullptr);
    EXPECT_EQ(design.tubeA->getName(), "A");
    nameMenu->close();
    EXPECT_EQ(design.tubeA->getName(), "A");
    fields.other->setFocus();
    EXPECT_EQ(design.tubeA->getName(), "Nose");

    // A text area's menu belongs to its viewport.
    fields.comment->setFocus();
    QTest::keyClicks(fields.comment, QStringLiteral("glued"));
    QWidget* const commentMenu = openContextMenu(*fields.comment->viewport());
    ASSERT_NE(commentMenu, nullptr);
    EXPECT_EQ(design.tubeA->getComment(), "");
    commentMenu->close();
    EXPECT_EQ(design.tubeA->getComment(), "");
    EXPECT_TRUE(fields.committer->commit());
    EXPECT_EQ(design.tubeA->getComment(), "glued");
}

TEST_F(TextCommitterTest, APopupElsewhereCommits)
{
    // As in OpenRocket, whose focusable popups (a unit selector's menu) take the focus from the
    // field: a temporary focus loss, which RocketComponentConfig's listener commits.
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    ASSERT_TRUE(fields.active);
    fields.name->setFocus();
    fields.name->selectAll();
    QTest::keyClicks(fields.name, QStringLiteral("Nose"));
    const OffscreenPopupWarnings warnings;
    QMenu                        menu(&window);
    menu.addAction(QStringLiteral("mm"));
    menu.popup(fields.other->mapToGlobal(QPoint(0, 0)));
    ASSERT_EQ(QApplication::activePopupWidget(), &menu);
    EXPECT_EQ(design.tubeA->getName(), "Nose");
    menu.close();
}

TEST_F(TextCommitterTest, AnInvalidSelectionDisablesTheFieldsAndTakesNothing)
{
    const TwoTubeRocket  design;
    QWidget              window;
    NameAndComment const fields(window, design);
    ASSERT_TRUE(fields.active);
    fields.name->setFocus();
    fields.name->setText(QStringLiteral("late"));
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(fields.name->isEnabled());
    EXPECT_FALSE(fields.comment->isEnabled());
    EXPECT_FALSE(fields.committer->commit());
    EXPECT_EQ(design.tubeA->getName(), "A");
}

TEST_F(TextCommitterTest, AFieldDestroyedFirstIsLeftOut)
{
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "value"};
    TextCommitter       committer(nullptr);
    {
        QLineEdit field;
        committer.addField(field, bindOn(*design.tubeA, bean.source()));
        field.setText(QStringLiteral("edited"));
    }
    EXPECT_FALSE(committer.commit());
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(TextCommitterTest, AFocusedFieldDestroyedCommitsNothing)
{
    // QWidget::~QWidget clears the focus after the line edit's own part is gone: the committer
    // must not read the half-destroyed field (UBSan's vptr check, ASan).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "value"};
    QWidget             window;
    TextCommitter       committer(nullptr);
    auto*               field = makeChild<QLineEdit>(&window);
    auto*               other = makeChild<QLineEdit>(&window);
    ASSERT_TRUE(showActiveWith(window, {field, other}));
    committer.addField(*field, bindOn(*design.tubeA, bean.source()));
    field->setFocus();
    QTest::keyClicks(field, QStringLiteral("x"));
    const QPointer<QLineEdit> guard(field);
    field->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(bean.sets, 0);
    EXPECT_FALSE(committer.commit());
}

TEST_F(TextCommitterTest, NeedsABinding)
{
    QLineEdit     field;
    TextCommitter committer(nullptr);
    EXPECT_THROW(committer.addField(field, nullptr), BugError);
    EXPECT_FALSE(committer.hasField(field));
}

TEST_F(TextCommitterTest, ACoreErrorOfACommitIsReported)
{
    const TwoTubeRocket      design;
    const RecordingGuardHook hook;
    const auto               throwing = StringSource::of<RocketComponent>({
        .get = [](const RocketComponent& c) { return c.getName(); },
        .set = [](RocketComponent&, const std::string&) { bug("a broken setter"); },
    });
    QLineEdit                field;
    field.setObjectName(QStringLiteral("Name"));
    TextCommitter committer(nullptr);
    committer.addField(field, bindOn(*design.tubeA, throwing));
    field.setText(QStringLiteral("x"));
    QTest::keyClick(&field, Qt::Key_Return);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Name"));
}

}  // namespace

}  // namespace QtRocket::Gui
