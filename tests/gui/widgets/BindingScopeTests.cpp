// The bindings of one dialog and their lifetime (Java: a panel's Invalidatables and
// ModelInvalidator): what the scope keeps, the commit of every pending edit before OK, Close, a
// save and an undo, and the release before the components can go. The lifetime tests are proved
// under the asan preset: a pending edit of a closing dialog must reach no destroyed component.

#include "QtRocket/gui/widgets/BindingScope.h"

#include <memory>
#include <tuple>
#include <utility>

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QLineEdit>
#include <QPointer>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using BindingScopeTest = Test::GuiTest;
using Test::bodyTubeLength;
using Test::IntBeanProperty;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::TwoTubeRocket;
using Test::typeInto;

/// A configuration dialog of tube A as the dialog shell builds one: its scope, a length field,
/// a count field, a name field and a line edit that takes the focus, shown active.
class LengthDialog
{
public:
    explicit LengthDialog(RocketComponent& tube)
      : m_dialog(std::make_unique<QDialog>()),
        m_scope(
            std::make_unique<BindingScope>(*m_dialog, std::make_shared<ComponentSelection>(tube))),
        length(makeChild<UnitSpinBox>(
            m_dialog.get(), m_scope->make<DoubleBinding>(m_scope->selection(), bodyTubeLength(),
                                                         unitGroup(UnitGroupId::LENGTH),
                                                         DoubleBinding::Options{}))),
        count(makeChild<IntSpinBox>(
            m_dialog.get(), m_scope->make<IntBinding>(m_scope->selection(), m_count.source()))),
        name(makeChild<QLineEdit>(m_dialog.get())),
        other(makeChild<QLineEdit>(m_dialog.get())),
        committer(makeChild<TextCommitter>(m_dialog.get())),
        active(showActiveWith(*m_dialog, {length, count, name, other}))
    {
        committer->addField(
            *name, m_scope->make<StringBinding>(m_scope->selection(), componentNameSource()));
    }

    [[nodiscard]] QDialog&      dialog() const noexcept { return *m_dialog; }
    [[nodiscard]] BindingScope& scope() const noexcept { return *m_scope; }
    [[nodiscard]] int           countValue() const noexcept { return m_count.value; }

    /// Closes and destroys the dialog, the scope first as a panel's member goes before its
    /// widgets: the focus loss of its fields comes while the dialog goes.
    void destroy()
    {
        m_scope.reset();
        m_dialog.reset();
    }

private:
    IntBeanProperty               m_count{.value = 3};
    std::unique_ptr<QDialog>      m_dialog;
    std::unique_ptr<BindingScope> m_scope;

public:
    UnitSpinBox*   length{nullptr};
    IntSpinBox*    count{nullptr};
    QLineEdit*     name{nullptr};
    QLineEdit*     other{nullptr};
    TextCommitter* committer{nullptr};
    bool           active{false};
};

TEST_F(BindingScopeTest, KeepsTheBindingsItMakes)
{
    const TwoTubeRocket design;
    QWidget             root;
    BindingScope        scope(root, std::make_shared<ComponentSelection>(*design.tubeA));
    const auto          length =
        scope.make<DoubleBinding>(scope.selection(), bodyTubeLength(),
                                  unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    const std::weak_ptr<DoubleBinding> watch = scope.make<DoubleBinding>(0.5);
    EXPECT_EQ(scope.bindingCount(), 2U);
    EXPECT_FALSE(watch.expired());  // only the scope holds it
    EXPECT_DOUBLE_EQ(length->value(), 0.3);
    EXPECT_THROW(std::ignore = scope.keep(std::shared_ptr<DoubleBinding>()), BugError);
    EXPECT_FALSE(scope.isReleased());
}

TEST_F(BindingScopeTest, ReleaseLetsGoOfTheBindingsAndEndsTheSelection)
{
    const TwoTubeRocket design;
    QWidget             root;
    BindingScope        scope(root, std::make_shared<ComponentSelection>(*design.tubeA));
    const auto          length =
        scope.make<DoubleBinding>(scope.selection(), bodyTubeLength(),
                                  unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    const std::weak_ptr<DoubleBinding> watch = scope.make<DoubleBinding>(0.5);
    scope.release();
    EXPECT_TRUE(scope.isReleased());
    EXPECT_TRUE(watch.expired());
    EXPECT_EQ(scope.bindingCount(), 0U);
    EXPECT_FALSE(length->isValid());  // its selection is released
    EXPECT_NE(scope.selection(), nullptr);
    EXPECT_FALSE(scope.selection()->isValid());
}

TEST_F(BindingScopeTest, AReleasedScopeKeepsNothingMore)
{
    const TwoTubeRocket design;
    QWidget             root;
    BindingScope        scope(root, std::make_shared<ComponentSelection>(*design.tubeA));
    scope.release();
    EXPECT_THROW(std::ignore = scope.make<DoubleBinding>(0.5), BugError);
    scope.release();  // again: nothing
    EXPECT_TRUE(scope.isReleased());
}

/// What the listeners kept by keepCounters() heard.
struct Counts
{
    int changes{0};
    int phases{0};
};

/// Keeps in @p scope two listeners of @p binding (a Connection and a ScopedConnection) and one
/// of the scope's selection, which count in @p counts.
void keepCounters(BindingScope& scope, Binding& binding, Counts& counts)
{
    scope.keepConnection(binding.addChangeListener([&counts] { ++counts.changes; }));
    scope.keepConnection(
        Signal<>::ScopedConnection(binding.addChangeListener([&counts] { ++counts.changes; })));
    scope.keepConnection(scope.selection()->addListener(
        [&counts](ComponentSelection::Phase /*phase*/) { ++counts.phases; }));
}

TEST_F(BindingScopeTest, ReleaseDisconnectsTheConnectionsItKeeps)
{
    const TwoTubeRocket design;
    QWidget             root;
    BindingScope        scope(root, std::make_shared<ComponentSelection>(*design.tubeA));
    const auto          length =
        scope.make<DoubleBinding>(scope.selection(), bodyTubeLength(),
                                  unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    Counts counts;
    keepCounters(scope, *length, counts);
    design.tubeA->setLength(0.4);
    EXPECT_EQ(counts.changes, 2);
    EXPECT_EQ(counts.phases, 2);  // READ, NOTIFY

    scope.release();
    design.tubeA->setLength(0.5);
    EXPECT_EQ(counts.changes, 2);
    EXPECT_EQ(counts.phases, 2);
    EXPECT_THROW(keepCounters(scope, *length, counts), BugError);
}

TEST_F(BindingScopeTest, CommitAllCommitsEveryPendingEdit)
{
    // Before OK, Close, a save or an undo: a button that takes no focus (macOS) leaves the edit
    // of the focused field pending.
    const TwoTubeRocket design;
    LengthDialog        dialog(*design.tubeA);
    ASSERT_TRUE(dialog.active);
    typeInto(*dialog.length, QStringLiteral("45"));
    EXPECT_TRUE(dialog.scope().commitAll());
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.45);
    EXPECT_TRUE(dialog.length->hasFocus());
    EXPECT_FALSE(dialog.scope().commitAll());  // nothing pending any more

    typeInto(*dialog.count, QStringLiteral("5"));
    EXPECT_TRUE(dialog.scope().commitAll());
    EXPECT_EQ(dialog.countValue(), 5);

    dialog.name->setFocus();
    dialog.name->selectAll();
    QTest::keyClicks(dialog.name, QStringLiteral("Booster tube"));
    EXPECT_TRUE(dialog.scope().commitAll());
    EXPECT_EQ(design.tubeA->getName(), "Booster tube");
}

TEST_F(BindingScopeTest, ACommitThatThrowsIsReportedAndTheOthersCommit)
{
    const TwoTubeRocket      design;
    const RecordingGuardHook hook;
    QWidget                  root;
    BindingScope             scope(root, std::make_shared<ComponentSelection>(*design.tubeA));
    const auto               throwing = DoubleSource::of<BodyTube>({
        .get = [](const BodyTube& tube) { return tube.getLength(); },
        .set = [](BodyTube&, double) { bug("a broken setter"); },
    });
    auto*                    broken   = makeChild<UnitSpinBox>(
        &root, scope.make<DoubleBinding>(scope.selection(), throwing,
                                         unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{}));
    broken->setObjectName(QStringLiteral("Length"));
    auto* name      = makeChild<QLineEdit>(&root);
    auto* committer = makeChild<TextCommitter>(&root);
    committer->addField(*name, scope.make<StringBinding>(scope.selection(), componentNameSource()));
    ASSERT_TRUE(showActiveWith(root, {broken, name}));
    typeInto(*broken, QStringLiteral("12"));
    name->setText(QStringLiteral("Renamed"));
    EXPECT_TRUE(scope.commitAll());
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Length"));
    EXPECT_EQ(design.tubeA->getName(), "Renamed");
}

TEST_F(BindingScopeTest, AReleasedScopeCommitsNothing)
{
    const TwoTubeRocket design;
    LengthDialog        dialog(*design.tubeA);
    ASSERT_TRUE(dialog.active);
    typeInto(*dialog.length, QStringLiteral("45"));
    dialog.scope().release();
    EXPECT_FALSE(dialog.scope().commitAll());
    dialog.other->setFocus();  // the focus loss: the binding drops the edit
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.3);
}

TEST_F(BindingScopeTest, AClosingDialogsFocusLossReachesNoDestroyedComponent)
{
    // The components go without an event of the rocket (events off), after the dialog released
    // its scope: the focus loss of the edited field while the dialog goes reaches nothing. Under
    // ASan a commit to the destroyed tube would be a heap-use-after-free.
    TwoTubeRocket design;
    LengthDialog  dialog(*design.tubeA);
    ASSERT_TRUE(dialog.active);
    typeInto(*dialog.length, QStringLiteral("45"));
    dialog.name->setText(QStringLiteral("Lost"));
    dialog.scope().release();
    design.rocket->enableEvents(false);
    static_cast<void>(design.stage->removeChild(design.tubeA));  // destroyed
    design.tubeA = nullptr;
    dialog.destroy();
    EXPECT_DOUBLE_EQ(design.tubeB->getLength(), 0.5);
}

TEST_F(BindingScopeTest, AnUndoEndsTheScopeAndNoFocusLossReachesTheOldComponents)
{
    // Rocket::loadFrom() replaces every component and destroys the old ones once its UNDO|TREE
    // event is delivered (ComponentSelection): the dialog commits its pending edit before the
    // undo (commitAll()), and whatever is typed afterwards reaches no old component.
    TwoTubeRocket      design;
    BodyTube*          tube = design.tubeA;
    OpenRocketDocument document(std::move(design.owner));
    LengthDialog       dialog(*tube);
    ASSERT_TRUE(dialog.active);
    document.addUndoPosition("Modify Body tube");
    typeInto(*dialog.length, QStringLiteral("45"));
    EXPECT_TRUE(dialog.scope().commitAll());
    EXPECT_DOUBLE_EQ(tube->getLength(), 0.45);

    typeInto(*dialog.length, QStringLiteral("60"));
    document.undo();
    EXPECT_TRUE(dialog.scope().isReleased());
    EXPECT_FALSE(dialog.scope().commitAll());
    QTest::keyClicks(dialog.name, QStringLiteral("x"));
    dialog.destroy();  // the focus loss of the length field: dropped
    const auto& restored =
        dynamic_cast<const BodyTube&>(document.getRocket().getChild(0).getChild(1));
    EXPECT_EQ(restored.getName(), "A");
    EXPECT_DOUBLE_EQ(restored.getLength(), 0.3);
}

TEST_F(BindingScopeTest, ADialogClosedWithAPendingEditCommitsIt)
{
    // Without a release, a dialog that closes (hides) while a field holds an edit commits it with
    // the focus loss, as Java's focusLost() does.
    const TwoTubeRocket design;
    LengthDialog        dialog(*design.tubeA);
    ASSERT_TRUE(dialog.active);
    typeInto(*dialog.length, QStringLiteral("45"));
    dialog.dialog().hide();
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.45);
    dialog.destroy();
}

TEST_F(BindingScopeTest, AScopeWithoutSelectionCommitsItsOwnValues)
{
    // A dialog that edits no component (its bindings hold values of their own).
    QWidget      root;
    BindingScope scope(root, nullptr);
    const auto   value = scope.make<DoubleBinding>(0.5, unitGroup(UnitGroupId::LENGTH));
    auto*        spin  = makeChild<UnitSpinBox>(&root, value);
    auto*        other = makeChild<QLineEdit>(&root);
    ASSERT_TRUE(showActiveWith(root, {spin, other}));
    typeInto(*spin, QStringLiteral("20"));
    EXPECT_TRUE(scope.commitAll());
    EXPECT_DOUBLE_EQ(value->value(), 0.2);
    EXPECT_FALSE(scope.isReleased());
    scope.release();
    EXPECT_TRUE(scope.isReleased());
}

TEST_F(BindingScopeTest, AScopeWhoseRootWentCommitsNothing)
{
    const TwoTubeRocket design;
    auto                root = std::make_unique<QWidget>();
    BindingScope        scope(*root, std::make_shared<ComponentSelection>(*design.tubeA));
    root.reset();
    EXPECT_FALSE(scope.commitAll());
}

}  // namespace

}  // namespace QtRocket::Gui
