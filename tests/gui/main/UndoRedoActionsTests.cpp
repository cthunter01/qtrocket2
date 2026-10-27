#include "QtRocket/gui/main/UndoRedoActions.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Uuid.h"
#include "RecordingGuardHook.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Gui
{

namespace
{

using UndoRedoActionsTest = Test::GuiTest;
/// A description of an undo step, or none (Java: null).
using Description = std::optional<std::string>;

/// The text of @p action as UTF-8.
[[nodiscard]] std::string actionText(const QAction& action)
{
    return QtStrings::toUtf8(action.text());
}

/// The keys of @p actions, as text.
[[nodiscard]] std::string keysText(const QList<QKeySequence>& keys)
{
    return QtStrings::toUtf8(QKeySequence::listToString(keys));
}

/// Forgets the process's undo error report before and after a test that makes one.
class UndoErrorReportReset
{
public:
    UndoErrorReportReset() { UndoRedoActions::resetUndoErrorReported(); }
    ~UndoErrorReportReset() { UndoRedoActions::resetUndoErrorReported(); }
    UndoErrorReportReset(const UndoErrorReportReset&)            = delete;
    UndoErrorReportReset& operator=(const UndoErrorReportReset&) = delete;
    UndoErrorReportReset(UndoErrorReportReset&&)                 = delete;
    UndoErrorReportReset& operator=(UndoErrorReportReset&&)      = delete;
};

/// Records the rocket's name whenever the actions are about to undo or redo.
struct NameRecorder
{
    const OpenRocketDocument* document;
    std::vector<std::string>* names;

    void operator()() const { names->push_back(document->getRocket().getName()); }
};

TEST_F(UndoRedoActionsTest, NewDocumentHasNothingToUndoOrRedo)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const UndoRedoActions actions(*document, nullptr);
    const QAction&        undo = *actions.undoAction();
    const QAction&        redo = *actions.redoAction();

    EXPECT_EQ(actionText(undo), "&Undo");
    EXPECT_FALSE(undo.isEnabled());
    EXPECT_EQ(actionText(redo), "&Redo");
    EXPECT_FALSE(redo.isEnabled());

    EXPECT_EQ(undo.objectName(), QStringLiteral("undoAction"));
    EXPECT_EQ(redo.objectName(), QStringLiteral("redoAction"));
    EXPECT_EQ(keysText(undo.shortcuts()), keysText(QKeySequence::keyBindings(QKeySequence::Undo)));
    EXPECT_EQ(keysText(redo.shortcuts()), keysText(QKeySequence::keyBindings(QKeySequence::Redo)));
    EXPECT_EQ(undo.shortcutContext(), Qt::WindowShortcut);
    EXPECT_EQ(redo.shortcutContext(), Qt::WindowShortcut);
    EXPECT_EQ(undo.menuRole(), QAction::NoRole);
    EXPECT_FALSE(undo.icon().isNull());
    EXPECT_FALSE(redo.icon().isNull());
    EXPECT_EQ(undo.statusTip(), QStringLiteral("Undo the previous operation"));
    EXPECT_EQ(redo.statusTip(), QStringLiteral("Redo the previously undone operation"));
}

TEST_F(UndoRedoActionsTest, TriggeringUndoesAndRedoes)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const UndoRedoActions actions(*document, nullptr);
    document->addUndoPosition("Rename");
    document->getRocket().setName("Mars");
    EXPECT_EQ(actionText(*actions.undoAction()), "&Undo (Rename)");
    EXPECT_TRUE(actions.undoAction()->isEnabled());

    actions.undoAction()->trigger();
    EXPECT_EQ(document->getRocket().getName(), "Rocket");
    EXPECT_EQ(actionText(*actions.undoAction()), "&Undo");
    EXPECT_FALSE(actions.undoAction()->isEnabled());
    EXPECT_EQ(actionText(*actions.redoAction()), "&Redo (Rename)");
    EXPECT_TRUE(actions.redoAction()->isEnabled());

    actions.redoAction()->trigger();
    EXPECT_EQ(document->getRocket().getName(), "Mars");
    EXPECT_EQ(actionText(*actions.undoAction()), "&Undo (Rename)");
    EXPECT_EQ(actionText(*actions.redoAction()), "&Redo");
    EXPECT_FALSE(actions.redoAction()->isEnabled());
}

TEST_F(UndoRedoActionsTest, ShowsAnAmpersandOfTheDescriptionAsItIs)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const UndoRedoActions actions(*document, nullptr);
    document->addUndoPosition("Modify A&B");
    document->getRocket().setName("Mars");
    EXPECT_EQ(actionText(*actions.undoAction()), "&Undo (Modify A&&B)");
    EXPECT_EQ(UndoRedoActions::undoText("x"), QStringLiteral("&Undo (x)"));
    EXPECT_EQ(UndoRedoActions::redoText(std::nullopt), QStringLiteral("&Redo"));
}

TEST_F(UndoRedoActionsTest, SaysSoBeforeItUndoesOrRedoes)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    UndoRedoActions          actions(*document, nullptr);
    std::vector<std::string> names;
    QObject::connect(&actions, &UndoRedoActions::aboutToUndoOrRedo, &actions,
                     NameRecorder{.document = document.get(), .names = &names});
    document->addUndoPosition("Rename");
    document->getRocket().setName("Mars");

    actions.undoAction()->trigger();
    actions.redoAction()->trigger();
    // Each time before the rocket changed.
    EXPECT_EQ(names, (std::vector<std::string>{"Mars", "Rocket"}));
    EXPECT_EQ(document->getRocket().getName(), "Mars");
}

TEST_F(UndoRedoActionsTest, AChangeBeforeTheRedoLeavesNothingToRedo)
{
    // An edit committed before the redo (aboutToUndoOrRedo()) is a change, which removes the redo
    // history: the redo is not done, and no undo error is reported.
    const UndoErrorReportReset                reset;
    const Gui::Test::RecordingGuardHook       hook;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    UndoRedoActions actions(*document, nullptr);
    document->addUndoPosition("Rename");
    document->getRocket().setName("Mars");
    actions.undoAction()->trigger();
    ASSERT_TRUE(actions.redoAction()->isEnabled());
    QObject::connect(&actions, &UndoRedoActions::aboutToUndoOrRedo, &actions,
                     [&document] { document->getRocket().setName("Venus"); });
    actions.redoAction()->trigger();
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(document->getRocket().getName(), "Venus");
    EXPECT_FALSE(actions.redoAction()->isEnabled());
    EXPECT_TRUE(hook.failures().empty());
    EXPECT_FALSE(UndoRedoActions::undoErrorReported());
}

TEST_F(UndoRedoActionsTest, ReportsTheProcesssFirstUndoErrorOnly)
{
    const UndoErrorReportReset                reset;
    const Gui::Test::RecordingGuardHook       hook;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const UndoRedoActions actions(*document, nullptr);

    // Nothing to undo: an undo error, reported from the event loop.
    document->undo();
    EXPECT_TRUE(hook.failures().empty());
    EXPECT_FALSE(UndoRedoActions::undoErrorReported());
    QCoreApplication::sendPostedEvents();
    ASSERT_EQ(hook.failures().size(), 1U);
    const Guard::Failure& failure = hook.failures().front();
    EXPECT_EQ(failure.kind, Guard::Failure::Kind::ERROR_CONDITION);
    EXPECT_TRUE(failure.message.startsWith(QStringLiteral("Undo/Redo error: ")))
        << QtStrings::toUtf8(failure.message);
    EXPECT_TRUE(UndoRedoActions::undoErrorReported());

    // Neither this document's next error nor another document's is reported.
    document->redo();
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    const UndoRedoActions                     otherActions(*other, nullptr);
    other->undo();
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(hook.failures().size(), 1U);
}

TEST_F(UndoRedoActionsTest, AReportNotDeliveredLeavesTheNextOneToBeReported)
{
    const UndoErrorReportReset                reset;
    const Gui::Test::RecordingGuardHook       hook;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    {
        const UndoRedoActions gone(*document, nullptr);
        document->undo();
    }
    QCoreApplication::sendPostedEvents();
    EXPECT_TRUE(hook.failures().empty());
    EXPECT_FALSE(UndoRedoActions::undoErrorReported());

    const UndoRedoActions actions(*document, nullptr);
    document->undo();
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(hook.failures().size(), 1U);
}

/// The GUI half of swing's IntegrationTest (its core half is
/// tests/core/file/openrocket/integration_tests.cpp, with the same set-up): the design
/// simplerocket.ork, loaded as the core half loads it (the motor database of its one motor, no
/// component presets), with the actions made right after.
class UndoRedoActionsIntegrationTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ASSERT_EQ(m_motors.getMotorSets().size(), 1U);
        Result<std::vector<std::byte>> design =
            readFile(QtRocket::Test::testDataDir() / "ork" / "simplerocket.ork");
        ASSERT_TRUE(design.has_value()) << "Problem in unit test, cannot find simplerocket.ork";
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context()).load(std::move(*design));
        ASSERT_TRUE(loaded.has_value())
            << "RocketLoadException while loading file simplerocket.ork : "
            << loaded.error().message;
        EXPECT_TRUE(loaded->warnings.empty()) << loaded->warnings.toString();
        m_document = std::move(loaded->document);
        // UndoRedoAction.newUndoAction(document), newRedoAction(document).
        m_actions = std::make_unique<UndoRedoActions>(*m_document, nullptr);
    }

    void TearDown() override
    {
        m_actions.reset();
        m_document.reset();
        GuiTest::TearDown();
    }

    [[nodiscard]] Rocket& rocket() { return m_document->getRocket(); }

    /// The mass component of the design (the first child of the body tube), found again by its
    /// id after every undo and redo; null when the rocket has no such component.
    [[nodiscard]] MassComponent* massComponent()
    {
        const Uuid& id =
            m_massComponentId.has_value()
                ? *m_massComponentId
                : m_massComponentId.emplace(rocket().getChild(0).getChild(1).getChild(0).getId());
        return dynamic_cast<MassComponent*>(rocket().findComponent(id));
    }

    /// undoAction.actionPerformed(...).
    void undo() { m_actions->undoAction()->trigger(); }
    /// redoAction.actionPerformed(...).
    void redo() { m_actions->redoAction()->trigger(); }

    /// IntegrationTest.checkUndoState(): the texts and the enabled states of the two actions.
    /// The texts are the English ones (the Java test's translator writes the keys:
    /// "[UndoRedoAction.OpenRocketDocument.Undo] (<description>)"), with the mnemonic marks of
    /// BasicFrame's menu items.
    void checkUndoState(const Description& undoDescription,
                        const Description& redoDescription) const
    {
        EXPECT_EQ(actionText(*m_actions->undoAction()),
                  undoDescription.has_value() ? "&Undo (" + *undoDescription + ")" : "&Undo");
        EXPECT_EQ(m_actions->undoAction()->isEnabled(), undoDescription.has_value());
        EXPECT_EQ(actionText(*m_actions->redoAction()),
                  redoDescription.has_value() ? "&Redo (" + *redoDescription + ")" : "&Redo");
        EXPECT_EQ(m_actions->redoAction()->isEnabled(), redoDescription.has_value());
    }

    // The motors and the preference store of the environment outlive the document:
    // IntegrationTest's motor database (MotorDbProvider), the one motor of Estes_A8.rse.
    ThrustCurveMotorSetDatabase           m_motors = QtRocket::Test::estesA8Database();
    QtRocket::Test::DesignFileEnvironment m_environment{
        QtRocket::Test::DesignFileEnvironment::Presets::NONE, m_motors};
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<UndoRedoActions>    m_actions;
    std::optional<Uuid>                 m_massComponentId;
};

// Port of IntegrationTest.testSimpleRocket (swing IntegrationTest.java), its GUI half: the undo
// and redo actions through the design's changes, undos and redos. The mass, aerodynamics and
// flight checks between the steps (checkCgCp(), checkAlt()) are the core half's.
TEST_F(UndoRedoActionsIntegrationTest, SimpleRocket)
{
    // Test undo state
    checkUndoState({}, {});

    // Mass modification
    m_document->addUndoPosition("Modify mass");
    checkUndoState({}, {});
    ASSERT_NE(massComponent(), nullptr);
    massComponent()->setComponentMass(0.01);
    checkUndoState("Modify mass", {});

    // Non-change
    m_document->addUndoPosition("No change");
    checkUndoState("Modify mass", {});

    // Non-functional change
    m_document->addUndoPosition("Name change");
    checkUndoState("Modify mass", {});
    massComponent()->setName("Foobar component");
    checkUndoState("Name change", {});

    // Aerodynamic modification
    m_document->addUndoPosition("Remove component");
    checkUndoState("Name change", {});
    static_cast<void>(rocket().getChild(0).removeChild(0));
    checkUndoState("Remove component", {});

    // Undo "Remove component" change
    undo();
    EXPECT_NE(dynamic_cast<const NoseCone*>(&rocket().getChild(0).getChild(0)), nullptr);
    checkUndoState("Name change", "Remove component");

    // Undo "Name change" change
    undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_EQ(massComponent()->getName(), "Extra mass");
    checkUndoState("Modify mass", "Name change");

    // Undo "Modify mass" change
    undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(QtRocket::Test::junitEquals(0.0, massComponent()->getComponentMass(), 0.0));
    checkUndoState({}, "Modify mass");

    // Redo "Modify mass" change
    redo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(QtRocket::Test::junitEquals(0.010, massComponent()->getComponentMass(), 0.00001));
    checkUndoState("Modify mass", "Name change");

    // Mass modification
    m_document->addUndoPosition("Modify mass2");
    checkUndoState("Modify mass", "Name change");
    massComponent()->setComponentMass(0.015);
    checkUndoState("Modify mass2", {});

    // Perform component movement
    m_document->startUndo("Move component");
    rocket().freeze();
    RocketComponent&                 bodytube    = rocket().getChild(0).getChild(1);
    RocketComponent&                 innertube   = bodytube.getChild(2);
    const RocketComponent&           engineblock = innertube.getChild(0);
    std::unique_ptr<RocketComponent> removed     = innertube.removeChild(&engineblock);
    ASSERT_NE(removed, nullptr);
    bodytube.addChild(std::move(removed), 0);
    checkUndoState("Modify mass2", {});
    rocket().thaw();
    checkUndoState("Move component", {});
    m_document->stopUndo();

    // Modify mass without setting undo description
    ASSERT_NE(massComponent(), nullptr);
    massComponent()->setComponentMass(0.020);
    checkUndoState("Modify mass2", {});

    // Undo "Modify mass2" change
    undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(QtRocket::Test::junitEquals(0.015, massComponent()->getComponentMass(), 0.0000001));
    checkUndoState("Move component", "Modify mass2");

    // Undo "Move component" change
    undo();
    EXPECT_NE(
        dynamic_cast<const EngineBlock*>(&rocket().getChild(0).getChild(1).getChild(2).getChild(0)),
        nullptr);
    checkUndoState("Modify mass2", "Move component");

    // Redo "Move component" change
    redo();
    EXPECT_NE(dynamic_cast<const EngineBlock*>(&rocket().getChild(0).getChild(1).getChild(0)),
              nullptr);
    checkUndoState("Move component", "Modify mass2");
}

}  // namespace

}  // namespace QtRocket::Gui
