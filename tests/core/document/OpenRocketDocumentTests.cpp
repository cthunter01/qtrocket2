#include "QtRocket/document/OpenRocketDocument.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Signal.h"
#include "TestTempDir.h"
#include "document/DocumentTestSupport.h"
#include "document/TestAttachments.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

// OpenRocket's own tests of the document are OpenRocketDocumentSimulationUndoRedoTest and
// OpenRocketDocumentPreferencesTest (ported in open_rocket_document_simulation_undo_redo_tests.cpp
// and open_rocket_document_preferences_tests.cpp). The tests here pin the rest against what
// OpenRocket itself does: Java probes against its compiled core (commit 5f164fd0e, JDK 17) ran
// the same steps and printed, after each, what the document's listeners heard and the state of
// its undo history (read by reflection). The comments name the probe and the section:
// - "DocumentProbe" and "DocumentProbe2": the tier 9 scout's (tier9-scout-document/logs/*.out);
// - "DocumentProbe3" and "DocumentProbe4": this part's (probes/tier9a-document-d3/logs/*.txt);
// - "Reenter", "DecalUndo", "DecalUndoFile" and "Files": the review fixes'
//   (probes/tier9a-fix-document/out/*.txt).
// The expected strings are the lines of those logs. Where this port deviates from OpenRocket by
// decision, the test says what Java printed.
//
// Notation (see DocumentRecorder): U = undoRedoChanged(), D(x) = documentChanged() of kind
// DOCUMENT from x, S(x) = of kind SIMULATION, ERR(text) = undoErrorOccurred(), and ",saved"
// where the document was saved while the event was emitted. The probes' "C[...]" entries, a
// listener of the rocket, are left out: its place among the others is arbitrary in Java.

namespace
{

using QtRocket::absolutePath;
using QtRocket::AbstractSimulationExtension;
using QtRocket::Appearance;
using QtRocket::AppearanceBuilder;
using QtRocket::Attachment;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentChangeSignal;
using QtRocket::CustomExpression;
using QtRocket::Decal;
using QtRocket::DecalImage;
using QtRocket::DocumentChangeEvent;
using QtRocket::DocumentPreferences;
using QtRocket::FileSystemAttachment;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightDataType;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationExtension;
using QtRocket::StorageOptions;
using QtRocket::UnitGroupId;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::DocumentRecorder;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::RocketEventRecorder;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::state;
using QtRocket::Test::stateWithConfigs;
using QtRocket::Test::TempDir;
using QtRocket::Test::testFcid;
using QtRocket::Test::undoState;

using Status = Simulation::Status;

// A document stays where it is: its rocket, its simulations and its listeners point at it.
static_assert(!std::is_copy_constructible_v<OpenRocketDocument>);
static_assert(!std::is_move_constructible_v<OpenRocketDocument>);
static_assert(!std::is_copy_assignable_v<OpenRocketDocument>);
static_assert(!std::is_move_assignable_v<OpenRocketDocument>);

// OpenRocketDocument.UNDO_LEVELS, UNDO_MARGIN and SIMULATION_NAME_PREFIX.
static_assert(OpenRocketDocument::kUndoLevels == 50);
static_assert(OpenRocketDocument::kUndoMargin == 10);
static_assert(OpenRocketDocument::kSimulationNamePrefix == "Simulation ");

/// The state every probe starts from: nothing happened to a document of the Alpha III.
constexpr std::string_view kUntouched =
    "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null "
    "redoAvail=false redoDesc=null saved=true sims=0";

// ================================================================== the rocket of a document

// Java: a NullPointerException in the constructor.
TEST(OpenRocketDocument, ANullRocketIsABug)
{
    EXPECT_THROW(const OpenRocketDocument document(std::unique_ptr<Rocket>{}), BugError);
}

TEST(OpenRocketDocument, HandsOutItsRocketAndItsSelectedConfiguration)
{
    const AlphaDocument       alpha;
    OpenRocketDocument&       d = alpha.document();
    const OpenRocketDocument& c = d;

    EXPECT_EQ(&c.getRocket(), &d.getRocket());
    EXPECT_EQ(d.getRocket().getDocument(), &d);
    EXPECT_EQ(&d.getSelectedConfiguration(), &d.getRocket().getSelectedConfiguration());
    EXPECT_EQ(&c.getSelectedConfiguration(), &d.getRocket().getSelectedConfiguration());
    EXPECT_TRUE(d.getSelectedConfiguration().getId().isDefaultId());

    d.getRocket().setSelectedConfiguration(testFcid(1));
    EXPECT_EQ(d.getSelectedConfiguration().getId(), testFcid(1));
    EXPECT_EQ(c.getSelectedConfiguration().getId(), testFcid(1));
}

// ===================================================== rocket edits, undo and redo: the events

// DocumentProbe, section 1 ("rocket edits, undo, redo: events, descriptions, saved flag"), up to
// the third change.
TEST(OpenRocketDocumentUndo, ARocketEditIsLabelledWithTheDescriptionOfItsUndoPosition)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    DocumentRecorder    events(d);

    // "created"
    EXPECT_EQ(state(d), kUntouched);
    // "setSaved(true)"
    d.setSaved(true);
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d), kUntouched);

    // "addUndoPosition(Edit A)": in the clean state only the description is noted.
    d.addUndoPosition("Edit A");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[null] next=Edit A stored=null undoAvail=false "
              "undoDesc=null redoAvail=false redoDesc=null saved=true sims=0");

    // "change 1 (name)"
    alpha.nose().setName("nose A");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[Edit A] next=Edit A stored=null undoAvail=true "
              "undoDesc=Edit A redoAvail=false redoDesc=null saved=false sims=0");

    // "change 2 (name)": a second change of the same step.
    alpha.nose().setName("nose A2");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[Edit A] next=Edit A stored=null undoAvail=true "
              "undoDesc=Edit A redoAvail=false redoDesc=null saved=false sims=0");

    // "addUndoPosition(Edit B)": in the dirty state the current state is appended.
    d.addUndoPosition("Edit B");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Edit A, null] next=Edit B stored=null undoAvail=true "
              "undoDesc=Edit A redoAvail=false redoDesc=null saved=false sims=0");

    // "change 3 (length)"
    alpha.nose().setLength(0.1);
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Edit A, Edit B] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");
}

/// The fixture of the tests that go on from the third change of DocumentProbe's section 1.
class OpenRocketDocumentUndoWalk : public ::testing::Test
{
protected:
    OpenRocketDocumentUndoWalk()
    {
        document().setSaved(true);
        document().addUndoPosition("Edit A");
        alpha().nose().setName("nose A");
        alpha().nose().setName("nose A2");
        document().addUndoPosition("Edit B");
        alpha().nose().setLength(0.1);
    }

    [[nodiscard]] const AlphaDocument& alpha() const noexcept { return m_alpha; }
    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return m_alpha.document(); }

private:
    AlphaDocument m_alpha;
};

// DocumentProbe, section 1: "undo", "undo", "undo (none available)".
TEST_F(OpenRocketDocumentUndoWalk, UndoGoesBackStateByState)
{
    OpenRocketDocument& d = document();
    DocumentRecorder    events(d);

    // The first undo, in the dirty state, appends the current state and loads the one at the
    // position, which stays.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=1 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit A redoAvail=true redoDesc=Edit B saved=false "
              "sims=0");
    // "name=nose A2 len=0.07"
    EXPECT_EQ(alpha().nose().getName(), "nose A2");
    EXPECT_EQ(alpha().nose().getLength(), 0.07);

    // The second, in the clean state, moves back.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=0 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Edit A saved=false "
              "sims=0");
    // "name=Nose Cone"
    EXPECT_EQ(alpha().nose().getName(), "Nose Cone");

    // "undo (none available) | events: ERR(Undo/Redo error: Undo not available) U"
    d.undo();
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: Undo not available) U");
    EXPECT_EQ(state(d),
              "pos=0 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Edit A saved=false "
              "sims=0");
}

// DocumentProbe, section 1: "redo", "redo", "redo (none available)", "undo".
TEST_F(OpenRocketDocumentUndoWalk, RedoGoesForwardStateByState)
{
    OpenRocketDocument& d = document();
    d.undo();
    d.undo();
    DocumentRecorder events(d);

    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=1 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit A redoAvail=true redoDesc=Edit B saved=false "
              "sims=0");
    EXPECT_EQ(alpha().nose().getName(), "nose A2");
    EXPECT_EQ(alpha().nose().getLength(), 0.07);

    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");
    EXPECT_EQ(alpha().nose().getLength(), 0.1);

    // "redo (none available) | events: U". Java tells its error handler of the first undo error
    // of a run only, and this was the probe's second; the error is the one DocumentProbe3,
    // section D, shows for a redo that comes first: "ERR(Undo/Redo error: Redo not available) U".
    d.redo();
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: Redo not available) U");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "undo", now in the clean state at the end of the history.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=1 hist=3 desc=[Edit A, Edit B, null] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit A redoAvail=true redoDesc=Edit B saved=false "
              "sims=0");
}

// DocumentProbe, section 1: "change while in history (no undo pos)", "addUndoPosition(null)",
// "addUndoPosition X then Y, clean", "change".
TEST_F(OpenRocketDocumentUndoWalk, AChangeInsideTheHistoryDropsTheRedoStates)
{
    OpenRocketDocument& d = document();
    d.undo();
    d.undo();
    d.redo();
    DocumentRecorder events(d);

    alpha().nose().setName("branch");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Edit A, Edit B] next=Edit B stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // A position without a description.
    d.addUndoPosition(std::nullopt);
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Edit A, Edit B, null] next=null stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // Two positions without a change in between: the last description counts.
    d.addUndoPosition("X");
    d.addUndoPosition("Y");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Edit A, Edit B, null] next=Y stored=null "
              "undoAvail=true undoDesc=Edit B redoAvail=false redoDesc=null saved=false "
              "sims=0");

    alpha().nose().setName("y-change");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Edit A, Edit B, Y] next=Y stored=null undoAvail=true "
              "undoDesc=Y redoAvail=false redoDesc=null saved=false sims=0");
}

// DocumentProbe, section 2 ("startUndo/stopUndo").
TEST(OpenRocketDocumentUndo, StartAndStopUndoBracketATimeLimitedStep)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    DocumentRecorder    events(d);

    // "addUndoPosition(Outer) + change"
    d.addUndoPosition("Outer");
    alpha.nose().setName("o1");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[Outer] next=Outer stored=null undoAvail=true "
              "undoDesc=Outer redoAvail=false redoDesc=null saved=false sims=0");

    // "startUndo(Timed)"
    d.startUndo("Timed");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Outer, null] next=Timed stored=Outer undoAvail=true "
              "undoDesc=Outer redoAvail=false redoDesc=null saved=false sims=0");

    // "change"
    alpha.nose().setName("t1");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Outer, Timed] next=Timed stored=Outer undoAvail=true "
              "undoDesc=Timed redoAvail=false redoDesc=null saved=false sims=0");

    // "stopUndo": the description from before startUndo() is in force again.
    d.stopUndo();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Outer, Timed, null] next=Outer stored=null "
              "undoAvail=true undoDesc=Timed redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "change"
    alpha.nose().setName("o2");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Outer, Timed, Outer] next=Outer stored=null "
              "undoAvail=true undoDesc=Outer redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // Three undos: "name=t1", "name=o1", "name=Nose Cone".
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=2 hist=4 desc=[Outer, Timed, Outer, null] next=Outer stored=null "
              "undoAvail=true undoDesc=Timed redoAvail=true redoDesc=Outer saved=false "
              "sims=0");
    EXPECT_EQ(alpha.nose().getName(), "t1");

    d.undo();
    EXPECT_EQ(state(d),
              "pos=1 hist=4 desc=[Outer, Timed, Outer, null] next=Outer stored=null "
              "undoAvail=true undoDesc=Outer redoAvail=true redoDesc=Timed saved=false "
              "sims=0");
    EXPECT_EQ(alpha.nose().getName(), "o1");

    d.undo();
    EXPECT_EQ(state(d),
              "pos=0 hist=4 desc=[Outer, Timed, Outer, null] next=Outer stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Outer saved=false "
              "sims=0");
    EXPECT_EQ(alpha.nose().getName(), "Nose Cone");
}

// ================================================================================ trimming

/// Makes @p count edits of the nose cone's name, each with an undo position of its own ("E0",
/// "n0", ...), and returns the probe's line: after each edit its number, the position and the
/// size of the history, as "0:0/1 1:1/2 ".
[[nodiscard]] std::string editAndTrace(const AlphaDocument& alpha, int count)
{
    std::string sizes;
    for (int i = 0; i < count; i++)
    {
        alpha.document().addUndoPosition(std::format("E{}", i));
        alpha.nose().setName(std::format("n{}", i));
        const OpenRocketDocument::UndoDetail detail = alpha.document().getUndoDetail();
        sizes += std::format("{}:{}/{} ", i, detail.position, detail.descriptions.size());
    }
    return sizes;
}

/// Undoes for as long as undo is available; the number of undos.
[[nodiscard]] int undoAll(OpenRocketDocument& document)
{
    int undos = 0;
    while (document.isUndoAvailable())
    {
        document.undo();
        undos++;
    }
    return undos;
}

// DocumentProbe, section 3 ("undo level trimming (UNDO_LEVELS=50, UNDO_MARGIN=10)"): the
// history grows to 60 states and loses its 10 oldest with the next position, twice in 75 edits.
TEST(OpenRocketDocumentUndo, TheHistoryIsTrimmedToFiftyLevelsWhenItExceedsSixty)
{
    const AlphaDocument alpha;

    EXPECT_EQ(editAndTrace(alpha, 75),
              "0:0/1 1:1/2 2:2/3 3:3/4 4:4/5 5:5/6 6:6/7 7:7/8 8:8/9 9:9/10 10:10/11 11:11/12 "
              "12:12/13 13:13/14 14:14/15 15:15/16 16:16/17 17:17/18 18:18/19 19:19/20 20:20/21 "
              "21:21/22 22:22/23 23:23/24 24:24/25 25:25/26 26:26/27 27:27/28 28:28/29 29:29/30 "
              "30:30/31 31:31/32 32:32/33 33:33/34 34:34/35 35:35/36 36:36/37 37:37/38 38:38/39 "
              "39:39/40 40:40/41 41:41/42 42:42/43 43:43/44 44:44/45 45:45/46 46:46/47 47:47/48 "
              "48:48/49 49:49/50 50:50/51 51:51/52 52:52/53 53:53/54 54:54/55 55:55/56 56:56/57 "
              "57:57/58 58:58/59 59:59/60 60:50/51 61:51/52 62:52/53 63:53/54 64:54/55 65:55/56 "
              "66:56/57 67:57/58 68:58/59 69:59/60 70:50/51 71:51/52 72:52/53 73:53/54 74:54/55 ");

    // "undos possible=55 name=n19": the 20 oldest states are gone.
    EXPECT_EQ(undoAll(alpha.document()), 55);
    EXPECT_EQ(alpha.nose().getName(), "n19");
    EXPECT_FALSE(alpha.document().isUndoAvailable());
    EXPECT_TRUE(alpha.document().isRedoAvailable());
}

// The descriptions go with their states.
TEST(OpenRocketDocumentUndo, TrimmingKeepsTheDescriptionsOfTheStatesThatStay)
{
    const AlphaDocument alpha;
    static_cast<void>(editAndTrace(alpha, 61));

    const OpenRocketDocument::UndoDetail detail = alpha.document().getUndoDetail();
    ASSERT_EQ(detail.descriptions.size(), 51U);
    EXPECT_EQ(detail.position, 50U);
    EXPECT_EQ(detail.descriptions.at(0), "E10");
    EXPECT_EQ(detail.descriptions.at(50), "E60");
    EXPECT_EQ(alpha.document().getUndoDescription(), "E60");
    EXPECT_FALSE(detail.clean);
}

// ====================================================== simulations: add, change, remove, undo

// DocumentProbe, section 4 ("simulations: add, change, remove, undo").
TEST(OpenRocketDocumentSimulations, AreAddedChangedRunAndUndone)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    DocumentRecorder    events(d);
    d.setSaved(true);

    // "new Simulation(d, rocket)" and "sim.setFlightConfigurationId (not yet added)". Deviation:
    // Java's simulation tells the document of its changes from its construction on ("events:
    // D(Simulation)", saved=false); here the document hears a simulation once it is in the list.
    const std::shared_ptr<Simulation> s = alpha.newSimulation();
    s->setFlightConfigurationId(testFcid(0));
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d), kUntouched);

    // "addUndoPosition + addSimulation"
    d.setSaved(true);
    d.addUndoPosition("Add sim");
    d.addSimulation(s);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    ASSERT_TRUE(events.lastEvent().has_value());
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getSimulation(), s.get());
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[Add sim] next=Add sim stored=null undoAvail=true "
              "undoDesc=Add sim redoAvail=false redoDesc=null saved=false sims=1");

    // "sim.setName"
    s->setName("Renamed");
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getSimulation(), s.get());
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[Add sim] next=Add sim stored=null undoAvail=true "
              "undoDesc=Add sim redoAvail=false redoDesc=null saved=false sims=1");

    // "addUndoPosition(Edit sim)"
    d.addUndoPosition("Edit sim");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Add sim, null] next=Edit sim stored=null "
              "undoAvail=true undoDesc=Add sim redoAvail=false redoDesc=null saved=false "
              "sims=1");

    // "options.setLaunchRodLength"
    s->getOptions().setLaunchRodLength(2.5);
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Add sim, Edit sim] next=Edit sim stored=null "
              "undoAvail=true undoDesc=Edit sim redoAvail=false redoDesc=null saved=false "
              "sims=1");

    // "simulate", "status=Up To Date"
    simulateOrFail(*s);
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_EQ(s->getStatus(), Status::UPTODATE);

    // "rocket change (no undo pos)", "status=Out of Date"
    alpha.nose().setLength(0.12);
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Add sim, Edit sim] next=Edit sim stored=null "
              "undoAvail=true undoDesc=Edit sim redoAvail=false redoDesc=null saved=false "
              "sims=1");
    EXPECT_EQ(s->getStatus(), Status::OUTDATED);

    // "undo": the rocket, then the simulation, whose restored options are a change of its own.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) D(Simulation) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=1 hist=3 desc=[Add sim, Edit sim, null] next=Edit sim stored=null "
              "undoAvail=true undoDesc=Add sim redoAvail=true redoDesc=Edit sim "
              "saved=false sims=1");
    // "status=Not Simulated Yet rod=0.0 name=Renamed len=0.07 data=false"
    EXPECT_EQ(s->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(s->getOptions().getLaunchRodLength(), 0.0);
    EXPECT_EQ(s->getName(), "Renamed");
    EXPECT_EQ(alpha.nose().getLength(), 0.07);
    EXPECT_EQ(s->getSimulatedData(), nullptr);
    EXPECT_EQ(d.getSimulation(0), s);

    // "undo": back to the state without the simulation.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getDocument(), &d);
    EXPECT_EQ(state(d),
              "pos=0 hist=3 desc=[Add sim, Edit sim, null] next=Edit sim stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Add sim saved=false "
              "sims=0");
    // "name=Renamed": the simulation itself stays as it was, with whoever holds it.
    EXPECT_EQ(s->getName(), "Renamed");

    // "undo | events: U" (Java had told its handler of an undo error before; see above).
    d.undo();
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: Undo not available) U");

    // "redo", "same object=false", "nextName=Simulation 1"
    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=1 hist=3 desc=[Add sim, Edit sim, null] next=Edit sim stored=null "
              "undoAvail=true undoDesc=Add sim redoAvail=true redoDesc=Edit sim "
              "saved=false sims=1");
    ASSERT_EQ(d.getSimulationCount(), 1U);
    EXPECT_NE(d.getSimulation(0), s);
    EXPECT_EQ(d.getSimulation(0)->getName(), "Renamed");
    EXPECT_EQ(d.getNextSimulationName(), "Simulation 1");
}

// DocumentProbe3, section A ("a simulation in the list: clean and dirty changes, the snapshot
// refresh").
TEST(OpenRocketDocumentSimulations, ARunMakesNoUndoStepAndItsResultsComeBackWithAnUndo)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    d.clearUndo();
    d.setSaved(true);
    DocumentRecorder events(d, true);

    // "start (sim A in the list, undo cleared, saved)"
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null "
              "redoAvail=false redoDesc=null saved=true sims=1 configs=5");

    // "simulate in the clean state": a change of the document, not an undo step.
    simulateOrFail(*a);
    EXPECT_EQ(events.take(), "D(Simulation)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=1 configs=5");
    EXPECT_EQ(alpha.simulationStates(), "A(UPTODATE,true),");

    // "syncModID in the clean state"
    a->syncModId();
    EXPECT_EQ(events.take(), "D(Simulation)");
    EXPECT_FALSE(d.isUndoAvailable());

    // "setName in the clean state": an undo step without a description.
    d.setSaved(true);
    a->setName("A2");
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=1 configs=5");

    // "setName again (dirty)"
    a->setName("A3");
    EXPECT_EQ(events.take(), "U D(Simulation)");

    // "undo": the state of the first position, with the results the run refreshed it with.
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=2 desc=[null, null] next=null stored=null undoAvail=false undoDesc=null "
              "redoAvail=true redoDesc=null saved=false sims=1 configs=5");
    EXPECT_EQ(alpha.simulationStates(), "A(UPTODATE,true),");

    // "redo"
    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=1 hist=2 desc=[null, null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=1 configs=5");
    EXPECT_EQ(alpha.simulationStates(), "A3(UPTODATE,true),");
}

// DocumentProbe3, section B ("removeSimulation(int), undo restores other objects; what the
// restored simulation fires").
TEST(OpenRocketDocumentSimulations, AnUndoThatChangesTheirNumberReplacesTheObjects)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    const std::shared_ptr<Simulation> b = alpha.newSimulation("B", testFcid(1));
    d.addSimulation(a);
    d.addSimulation(b);
    simulateOrFail(*a);
    d.clearUndo();
    d.setSaved(true);
    DocumentRecorder events(d, true);

    // "removeSimulation(0) -> same object=true"
    d.addUndoPosition("Remove A");
    const std::shared_ptr<Simulation> removed = d.removeSimulation(0);
    EXPECT_EQ(removed, a);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getSimulation(), a.get());
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[Remove A] next=Remove A stored=null undoAvail=true "
              "undoDesc=Remove A redoAvail=false redoDesc=null saved=false sims=1 configs=5");

    // "undo": "A(UPTODATE,true),B(NOT_SIMULATED,false), A same object=false B same object=false"
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=2 desc=[Remove A, null] next=Remove A stored=null undoAvail=false "
              "undoDesc=null redoAvail=true redoDesc=Remove A saved=false sims=2 configs=5");
    EXPECT_EQ(alpha.simulationStates(), "A(UPTODATE,true),B(NOT_SIMULATED,false),");
    ASSERT_EQ(d.getSimulationCount(), 2U);
    const std::shared_ptr<Simulation> restored = d.getSimulation(0);
    EXPECT_NE(restored, a);
    EXPECT_NE(d.getSimulation(1), b);
    // The results are shared with the simulation that was removed.
    EXPECT_EQ(restored->getSimulatedData(), a->getSimulatedData());

    // "restored.getOptions().setLaunchRodLength | events:" (none), "saved=true",
    // "undoAvail=true", "restored status=Out of Date". As in Java, the options of a simulation
    // that an undo made do not tell the simulation of their changes, so the document learns of
    // this one with the next event only.
    d.setSaved(true);
    restored->getOptions().setLaunchRodLength(3.25);
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=2 desc=[Remove A, null] next=Remove A stored=null undoAvail=true "
              "undoDesc=Remove A redoAvail=true redoDesc=Remove A saved=true sims=2 configs=5");
    EXPECT_EQ(restored->getStatus(), Status::OUTDATED);

    // "restored.setName": the restored simulation is heard.
    restored->setName("A restored");
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getSimulation(), restored.get());
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[Remove A] next=Remove A stored=null undoAvail=true "
              "undoDesc=Remove A redoAvail=false redoDesc=null saved=false sims=2 configs=5");

    // "change of the old object A | events: U D(Simulation)". Deviation: a simulation that left
    // the list is no longer heard.
    d.setSaved(true);
    a->setName("old A");
    b->setName("old B");
    EXPECT_EQ(events.take(), "");
    EXPECT_TRUE(d.isSaved());
}

// DocumentProbe3, section B: "removeSimulation(7): IndexOutOfBoundsException",
// "getSimulation(7): IndexOutOfBoundsException", "addSimulation(a, 9): IndexOutOfBoundsException".
TEST(OpenRocketDocumentSimulations, AnIndexOutOfRangeIsABug)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    DocumentRecorder events(d);

    EXPECT_THROW(static_cast<void>(d.removeSimulation(7)), BugError);
    EXPECT_THROW(static_cast<void>(d.removeSimulation(1)), BugError);
    EXPECT_THROW(static_cast<void>(d.getSimulation(7)), BugError);
    EXPECT_THROW(static_cast<void>(d.getSimulation(1)), BugError);
    EXPECT_THROW(d.addSimulation(alpha.newSimulation(), 9), BugError);
    EXPECT_THROW(d.addSimulation(alpha.newSimulation(), 2), BugError);
    // Java: a NullPointerException, after the null was put into the list.
    EXPECT_THROW(d.addSimulation(nullptr), BugError);
    EXPECT_THROW(d.addSimulation(nullptr, 0), BugError);

    // Nothing happened.
    EXPECT_EQ(events.take(), "");
    ASSERT_EQ(d.getSimulationCount(), 1U);
    EXPECT_EQ(d.getSimulation(0), a);
}

// DocumentProbe2, section A ("simulation list operations"), up to clearUndo.
TEST(OpenRocketDocumentSimulations, TheListIsChangedThroughTheDocument)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d      = alpha.document();
    Rocket&                           rocket = alpha.rocket();
    const std::shared_ptr<Simulation> a      = alpha.newSimulation("A", testFcid(0));
    const std::shared_ptr<Simulation> b      = alpha.newSimulation("B", testFcid(1));
    const std::shared_ptr<Simulation> c      = alpha.newSimulation("C", testFcid(0));
    DocumentRecorder                  events(d);

    // "add A, B"
    d.addSimulation(a);
    d.addSimulation(b);
    EXPECT_EQ(events.take(), "U S(Simulation) U S(Simulation)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=2 configs=5");

    // "addSimulation(C, 0)", "order=CAB indexOf(B)=2"
    d.addSimulation(c, 0);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    EXPECT_EQ(alpha.simulationNames(), "C,A,B,");
    EXPECT_EQ(d.getSimulationIndex(*b), 2U);
    EXPECT_EQ(d.getSimulations(), (std::vector<std::shared_ptr<Simulation>>{c, a, b}));

    // "removeSimulation(not in the list)": the events come all the same.
    Simulation stranger(rocket);
    d.removeSimulation(stranger);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getSimulation(), &stranger);
    EXPECT_EQ(d.getSimulationCount(), 3U);

    // "sim.setFlightConfigurationId(new id), not added | events: C[[tree]] U D(Rocket) D(Rocket)
    // U D(Simulation)": the rocket's event for the new configuration; the simulation's own
    // change is not heard here (the deviation of AreAddedChangedRunAndUndone).
    const FlightConfigurationId       fresh;
    const std::shared_ptr<Simulation> e = alpha.newSimulation();
    events.clear();
    e->setFlightConfigurationId(fresh);
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket)");
    EXPECT_EQ(rocket.getFlightConfigurationCount(), 6);

    // "addSimulation(sim with that id)"
    d.addSimulation(e);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    EXPECT_EQ(d.getSimulationCount(), 4U);
    EXPECT_EQ(rocket.getFlightConfigurationCount(), 6);

    // "addSimulation(sim made without a document)"
    const std::shared_ptr<Simulation> f = std::make_shared<Simulation>(rocket);
    f->setName("no document");
    d.addSimulation(f);
    EXPECT_EQ(events.take(), "U S(Simulation)");
    EXPECT_EQ(d.getSimulationCount(), 5U);

    // "change of the sim made without a document | events:" (none), "saved=true". Deviation:
    // Java never hears a simulation that was made without a document; here every simulation of
    // the list is heard.
    d.setSaved(true);
    f->setName("changed");
    EXPECT_EQ(events.take(), "U D(Simulation)");
    EXPECT_FALSE(d.isSaved());

    // "clearUndo"
    d.clearUndo();
    EXPECT_EQ(events.take(), "U");
    EXPECT_FALSE(d.isUndoAvailable());
    EXPECT_FALSE(d.isRedoAvailable());
}

// DocumentProbe2, section A, from "removeFlightConfigurationAndSimulations(FCID_0)" on.
TEST(OpenRocketDocumentSimulations, AFlightConfigurationIsRemovedWithItsSimulations)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d      = alpha.document();
    Rocket&                           rocket = alpha.rocket();
    const std::shared_ptr<Simulation> a      = alpha.newSimulation("A", testFcid(0));
    const std::shared_ptr<Simulation> b      = alpha.newSimulation("B", testFcid(1));
    const std::shared_ptr<Simulation> c      = alpha.newSimulation("C", testFcid(0));
    const std::shared_ptr<Simulation> e      = alpha.newSimulation();
    e->setFlightConfigurationId(FlightConfigurationId{});
    const std::shared_ptr<Simulation> f = std::make_shared<Simulation>(rocket);
    f->setName("changed");
    d.addSimulation(a);
    d.addSimulation(b);
    d.addSimulation(c, 0);
    d.addSimulation(e);
    d.addSimulation(f);
    d.clearUndo();
    ASSERT_EQ(alpha.simulationNames(), "C,A,B,,changed,");
    ASSERT_EQ(rocket.getFlightConfigurationCount(), 6);
    DocumentRecorder events(d);

    // Two simulations go, each with its events, then the configuration with the rocket's.
    d.addUndoPosition("Remove config");
    d.removeFlightConfigurationAndSimulations(testFcid(0));
    EXPECT_EQ(events.take(), "U S(Simulation) U S(Simulation) U D(Rocket) D(Rocket)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[Remove config] next=Remove config stored=null undoAvail=true "
              "undoDesc=Remove config redoAvail=false redoDesc=null saved=false sims=3 configs=5");
    // "left=B,,changed, contains FCID_0=false"
    EXPECT_EQ(alpha.simulationNames(), "B,,changed,");
    EXPECT_FALSE(rocket.containsFlightConfigurationId(testFcid(0)));

    // "undo": "now=C(NOT_SIMULATED),A(NOT_SIMULATED),B(NOT_SIMULATED),(CANT_RUN),
    // changed(CANT_RUN), contains FCID_0=true A same object=false"
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=2 desc=[Remove config, null] next=Remove config stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Remove config saved=false "
              "sims=5 configs=6");
    EXPECT_EQ(alpha.simulationStates(),
              "C(NOT_SIMULATED,false),A(NOT_SIMULATED,false),B(NOT_SIMULATED,false),"
              "(CANT_RUN,false),changed(CANT_RUN,false),");
    EXPECT_TRUE(rocket.containsFlightConfigurationId(testFcid(0)));
    EXPECT_EQ(d.getSimulationIndex(*a), std::nullopt);
    EXPECT_EQ(d.getSimulationIndex(*b), std::nullopt);

    // "redo"
    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=1 hist=2 desc=[Remove config, null] next=Remove config stored=null "
              "undoAvail=true undoDesc=Remove config redoAvail=false redoDesc=null saved=false "
              "sims=3 configs=5");
    EXPECT_EQ(alpha.simulationNames(), "B,,changed,");

    // "change of a simulation no longer in the list | events: D(Simulation)". Deviation: not
    // heard.
    a->setName("A changed after it left the document");
    EXPECT_EQ(events.take(), "");
}

// DocumentProbe3, section C ("addSimulation of a simulation whose configuration the rocket
// lacks").
TEST(OpenRocketDocumentSimulations, AddingOneCreatesItsFlightConfiguration)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d      = alpha.document();
    Rocket&                           rocket = alpha.rocket();
    const FlightConfigurationId       fresh;
    const std::shared_ptr<Simulation> a = std::make_shared<Simulation>(rocket);
    a->setFlightConfigurationId(fresh);
    rocket.removeFlightConfiguration(fresh);
    d.setSaved(true);
    DocumentRecorder events(d, true);

    // "start"
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=true sims=0 configs=5");

    // "addSimulation": the rocket's event of the new configuration, then the simulation's.
    d.addSimulation(a);
    EXPECT_EQ(events.take(), "U D(Rocket,saved) D(Rocket) U S(Simulation)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=1 configs=6");
    // "contains=true"
    EXPECT_TRUE(rocket.containsFlightConfigurationId(fresh));
}

// A simulation that is in the list twice (Java's list allows it) is heard once, and for as
// long as one of its two places is left.
TEST(OpenRocketDocumentSimulations, ASimulationThatIsInTheListTwiceIsHeardOnce)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    d.addSimulation(a);
    d.clearUndo();
    DocumentRecorder events(d);

    a->setName("A2");
    EXPECT_EQ(events.take(), "U D(Simulation)");

    // Removing it takes the first of the two out.
    d.removeSimulation(*a);
    EXPECT_EQ(d.getSimulationCount(), 1U);
    events.clear();
    a->setName("A3");
    EXPECT_EQ(events.take(), "U D(Simulation)");

    static_cast<void>(d.removeSimulation(0));
    EXPECT_EQ(d.getSimulationCount(), 0U);
    events.clear();
    a->setName("A4");
    EXPECT_EQ(events.take(), "");
}

// ================================================================== names, details, the file

/// Adds a simulation named @p name to @p alpha's document and returns the next simulation name.
[[nodiscard]] std::string nextNameAfterAdding(const AlphaDocument& alpha, std::string_view name)
{
    const std::shared_ptr<Simulation> s = alpha.newSimulation();
    s->setName(name);
    alpha.document().addSimulation(s);
    return alpha.document().getNextSimulationName();
}

// DocumentProbe, section 5 ("next simulation name"): what Integer.parseInt reads counts.
TEST(OpenRocketDocumentNames, TheNextSimulationNameIsOneAboveTheLargestNumber)
{
    const AlphaDocument alpha;
    // DocumentProbe3, section G: "empty: next=Simulation 1"
    EXPECT_EQ(alpha.document().getNextSimulationName(), "Simulation 1");

    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 3"), "Simulation 4");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation x"), "Simulation 4");
    // A space at the end is no number.
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 12 "), "Simulation 4");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation -4"), "Simulation 4");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation +7"), "Simulation 8");
    // The prefix is compared exactly.
    EXPECT_EQ(nextNameAfterAdding(alpha, "simulation 99"), "Simulation 8");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 007"), "Simulation 8");
}

// DocumentProbe3, section G: Java's int addition wraps around.
TEST(OpenRocketDocumentNames, TheNextSimulationNumberWrapsAroundAsAJavaInt)
{
    const AlphaDocument alpha;

    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 2147483646"), "Simulation 2147483647");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 2147483647"), "Simulation -2147483648");
    // Too large for an int: no number.
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 2147483648"), "Simulation -2147483648");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation "), "Simulation -2147483648");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation  5"), "Simulation -2147483648");
    EXPECT_EQ(nextNameAfterAdding(alpha, "Simulation 0"), "Simulation -2147483648");
}

// DocumentProbe3, section G: "detail of none: [>> Dumping simulation list:\n]" and
// "detail: [>> Dumping simulation list:\n    [0] First (d010716e) \n    [1]  (DefaultKey) \n]".
TEST(OpenRocketDocumentNames, TheSimulationDetailListsNamesAndConfigurationKeys)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    EXPECT_EQ(d.toSimulationDetail(), ">> Dumping simulation list:\n");

    const std::shared_ptr<Simulation> first   = alpha.newSimulation("First", testFcid(0));
    const std::shared_ptr<Simulation> unnamed = alpha.newSimulation();
    d.addSimulation(first);
    d.addSimulation(unnamed);

    EXPECT_EQ(d.toSimulationDetail(),
              ">> Dumping simulation list:\n"
              "    [0] First (d010716e) \n"
              "    [1]  (DefaultKey) \n");
}

/// getFileNoExtension() of a document whose file is @p file.
[[nodiscard]] std::optional<std::filesystem::path> withoutExtension(
    OpenRocketDocument& document, const std::filesystem::path& file)
{
    document.setFile(file);
    return document.getFileNoExtension();
}

// DocumentProbe3, section G: "'/a/b/rocket.ork' -> '/a/b/rocket' same object=false" and the
// lines after it. The probe's directory /a/b is "a/b" under the current directory here, so that
// the paths are absolute on every platform.
TEST(OpenRocketDocumentFile, TheExtensionIsCutFromTheAbsolutePath)
{
    const AlphaDocument         alpha;
    OpenRocketDocument&         d    = alpha.document();
    const std::filesystem::path base = std::filesystem::current_path() / "a" / "b";

    // "getFile of a new document: null noext=null"
    EXPECT_EQ(d.getFile(), std::nullopt);
    EXPECT_EQ(d.getFileNoExtension(), std::nullopt);

    EXPECT_EQ(withoutExtension(d, base / "rocket.ork"), base / "rocket");
    EXPECT_EQ(d.getFile(), base / "rocket.ork");
    EXPECT_EQ(withoutExtension(d, base / "x.rkt"), base / "x");
    // "'/a/b/two.dots.ork' -> '/a/b/two.dots'": the last dot counts.
    EXPECT_EQ(withoutExtension(d, base / "two.dots.ork"), base / "two.dots");
    // "'/a/b/.ork' -> '/a/b'": Java's File drops the separator at the end.
    EXPECT_EQ(withoutExtension(d, base / ".ork"), base);

    // A relative file is made absolute when its extension is cut: "'rocket.ork' ->
    // '<cwd>/rocket'", "'sub/dir/y.rkt' -> '<cwd>/sub/dir/y'".
    EXPECT_EQ(withoutExtension(d, "rocket.ork"), std::filesystem::current_path() / "rocket");
    EXPECT_EQ(withoutExtension(d, std::filesystem::path("sub") / "dir" / "y.rkt"),
              std::filesystem::current_path() / "sub" / "dir" / "y");
    // "'./z.ork' -> '<cwd>/./z'" and "'../z.ork' -> '<cwd>/../z'": the absolute path as
    // absolutePath() makes it (POSIX keeps the "." and "..", as Java does; Windows resolves them).
    EXPECT_EQ(withoutExtension(d, std::filesystem::path(".") / "z.ork"),
              absolutePath(std::filesystem::path(".") / "z.ork").replace_extension());
    EXPECT_EQ(withoutExtension(d, std::filesystem::path("..") / "z.ork"),
              absolutePath(std::filesystem::path("..") / "z.ork").replace_extension());

    d.setFile(std::nullopt);
    EXPECT_EQ(d.getFile(), std::nullopt);
    EXPECT_EQ(d.getFileNoExtension(), std::nullopt);
}

// DocumentProbe3, section G: the lines with "same object=true". Java's quirk: "ork.gz" and
// "rkt.gz" are in its list of extensions, but what follows the last dot of such a name is "gz".
TEST(OpenRocketDocumentFile, AnyOtherFileComesBackAsItWasSet)
{
    const AlphaDocument         alpha;
    OpenRocketDocument&         d    = alpha.document();
    const std::filesystem::path base = std::filesystem::current_path() / "a" / "b";

    EXPECT_EQ(withoutExtension(d, base / "x.ork.gz"), base / "x.ork.gz");
    EXPECT_EQ(withoutExtension(d, base / "x.rkt.gz"), base / "x.rkt.gz");
    EXPECT_EQ(withoutExtension(d, base / "x.txt"), base / "x.txt");
    EXPECT_EQ(withoutExtension(d, base / "noext"), base / "noext");
    // "'/a/b.c/noext'": a dot in a directory's name.
    EXPECT_EQ(withoutExtension(d, base / "b.c" / "noext"), base / "b.c" / "noext");
    // "'/a/b.ork/x'"
    EXPECT_EQ(withoutExtension(d, base / "b.ork" / "x"), base / "b.ork" / "x");
    // "'/a/b/x.ORK'": the extension is compared exactly.
    EXPECT_EQ(withoutExtension(d, base / "x.ORK"), base / "x.ORK");
    // "'/a/b/trailing.'"
    EXPECT_EQ(withoutExtension(d, base / "trailing."), base / "trailing.");
    // The file itself comes back, so a relative one stays relative.
    EXPECT_EQ(withoutExtension(d, "notes.txt"), std::filesystem::path("notes.txt"));
    EXPECT_EQ(withoutExtension(d, "x.ork.gz"), std::filesystem::path("x.ork.gz"));
}

/// @p file as text, "none" without one.
[[nodiscard]] std::string textOf(const std::optional<std::filesystem::path>& file)
{
    return file.has_value() ? QtRocket::pathToUtf8(*file) : "none";
}

/// @p base followed by @p separators separators and @p name: a path with a run of separators in
/// it, which operator/ would not make.
[[nodiscard]] std::filesystem::path withSeparators(std::filesystem::path base, int separators,
                                                   const std::filesystem::path& name)
{
    for (int i = 0; i < separators; i++)
    {
        base += std::filesystem::path::preferred_separator;
    }
    base += name;
    return base;
}

/// getFile() and getFileNoExtension() of a document whose file is @p file, as the probe prints
/// them: "file='<file>' noext='<file without extension>'".
[[nodiscard]] std::string fileAndNoExtension(OpenRocketDocument&          document,
                                             const std::filesystem::path& file)
{
    document.setFile(file);
    return std::format("file='{}' noext='{}'", textOf(document.getFile()),
                       textOf(document.getFileNoExtension()));
}

/// The same for the file @p file and the cut file @p noExtension, as they are expected.
[[nodiscard]] std::string expectedFiles(const std::filesystem::path& file,
                                        const std::filesystem::path& noExtension)
{
    return std::format("file='{}' noext='{}'", QtRocket::pathToUtf8(file),
                       QtRocket::pathToUtf8(noExtension));
}

// Files: "'/a/b.ork/' -> file='/a/b.ork' noext='/a/b'", "'/a//b.ork' -> file='/a/b.ork'
// noext='/a/b'" and the lines after them. Java's File drops a separator at the end and takes a
// run of separators as one when it is made; std::filesystem::path keeps both, so setFile() does
// what File does. The probe's "/a" is "a" under the current directory here.
TEST(OpenRocketDocumentFile, TheFileIsKeptAsJavasFileKeepsIt)
{
    const AlphaDocument         alpha;
    OpenRocketDocument&         d    = alpha.document();
    const std::filesystem::path cwd  = std::filesystem::current_path();
    const std::filesystem::path base = cwd / "a";

    EXPECT_EQ(fileAndNoExtension(d, base / "b.ork" / ""),
              expectedFiles(base / "b.ork", base / "b"));
    EXPECT_EQ(fileAndNoExtension(d, withSeparators(base, 2, "b.ork")),
              expectedFiles(base / "b.ork", base / "b"));
    // "'/a///b//x.ork//' -> file='/a/b/x.ork' noext='/a/b/x'"
    EXPECT_EQ(fileAndNoExtension(d, withSeparators(withSeparators(base, 3, "b"), 2, "x.ork") / ""),
              expectedFiles(base / "b" / "x.ork", base / "b" / "x"));
    // "'/a/b/' -> file='/a/b' noext='/a/b'"
    EXPECT_EQ(fileAndNoExtension(d, base / "b" / ""), expectedFiles(base / "b", base / "b"));
    // "'a//b.ork' -> file='a/b.ork' noext='<cwd>/a/b'": a relative file stays relative.
    EXPECT_EQ(fileAndNoExtension(d, withSeparators("a", 2, "b.ork")),
              expectedFiles(std::filesystem::path("a") / "b.ork", cwd / "a" / "b"));
    // "'sub/x.txt/' -> file='sub/x.txt' noext='sub/x.txt'"
    EXPECT_EQ(fileAndNoExtension(d, std::filesystem::path("sub") / "x.txt" / ""),
              expectedFiles(std::filesystem::path("sub") / "x.txt",
                            std::filesystem::path("sub") / "x.txt"));
    // "'/a/./b.ork' -> file='/a/./b.ork'": nothing else is changed.
    d.setFile(base / "." / "b.ork");
    EXPECT_EQ(textOf(d.getFile()), QtRocket::pathToUtf8(base / "." / "b.ork"));
    // "'' -> file='' noext=''" and "'/' -> file='/' noext='/'"
    EXPECT_EQ(fileAndNoExtension(d, std::filesystem::path()), "file='' noext=''");
    EXPECT_EQ(fileAndNoExtension(d, cwd.root_path()),
              expectedFiles(cwd.root_path(), cwd.root_path()));
}

// ============================================================================== saved or not

// DocumentProbe3, section K ("clearUndo in a dirty state; setSaved; undo to the saved state"),
// and DocumentProbe2, section B.
TEST(OpenRocketDocumentSaved, FollowsTheDocumentsModificationId)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    DocumentRecorder    events(d, true);

    // "created from a rocket": saved, with nothing to compare (both ids are invalid).
    EXPECT_EQ(state(d), kUntouched);
    // DocumentProbe2: "fresh document after setSaved(false): saved=true"
    d.setSaved(false);
    EXPECT_TRUE(d.isSaved());

    // "change": the document is still saved in the first of the two events of a rocket change.
    d.addUndoPosition("x");
    alpha.nose().setName("x1");
    EXPECT_EQ(events.take(), "U D(NoseCone,saved) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[x] next=x stored=null undoAvail=true undoDesc=x "
              "redoAvail=false redoDesc=null saved=false sims=0");

    // "clearUndo": the description for the next change stays, and so does "unsaved".
    d.clearUndo();
    EXPECT_EQ(events.take(), "U");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[null] next=x stored=null undoAvail=false "
              "undoDesc=null redoAvail=false redoDesc=null saved=false sims=0");

    // "setSaved(true)", "setSaved(false)"
    d.setSaved(true);
    EXPECT_TRUE(d.isSaved());
    d.setSaved(false);
    EXPECT_FALSE(d.isSaved());
    EXPECT_EQ(events.take(), "");

    // "saved, change, undo": back at the saved state of the rocket, the document is not saved.
    d.setSaved(true);
    d.addUndoPosition("y");
    alpha.nose().setName("y1");
    d.undo();
    EXPECT_EQ(state(d),
              "pos=0 hist=2 desc=[y, null] next=y stored=null undoAvail=false "
              "undoDesc=null redoAvail=true redoDesc=y saved=false sims=0");
    EXPECT_EQ(alpha.nose().getName(), "x1");

    // "redo"
    d.redo();
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[y, null] next=y stored=null undoAvail=true "
              "undoDesc=y redoAvail=false redoDesc=null saved=false sims=0");
}

// DocumentProbe4, section C ("rocket events that are not component edits").
TEST(OpenRocketDocumentSaved, EveryEventOfTheRocketCounts)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d      = alpha.document();
    Rocket&             rocket = alpha.rocket();
    d.setSaved(true);
    DocumentRecorder events(d, true);

    // "setSelectedConfiguration"
    rocket.setSelectedConfiguration(testFcid(1));
    EXPECT_EQ(events.take(), "U D(Rocket,saved) D(Rocket)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=0 configs=5");

    // "setSelectedConfiguration (the same)"
    d.setSaved(true);
    rocket.setSelectedConfiguration(testFcid(1));
    EXPECT_EQ(events.take(), "");
    EXPECT_TRUE(d.isSaved());

    // "two changes while frozen": the rocket holds its events back, so the document is saved
    // until the rocket thaws.
    rocket.freeze();
    alpha.nose().setName("f1");
    alpha.nose().setLength(0.09);
    EXPECT_EQ(events.take(), "");
    EXPECT_TRUE(d.isSaved());
    EXPECT_TRUE(d.isUndoAvailable());

    // "thaw": one event for the two changes.
    rocket.thaw();
    EXPECT_EQ(events.take(), "U D(NoseCone,saved) D(NoseCone)");
    EXPECT_FALSE(d.isSaved());
}

// What a loader leaves (the scout's report, "traps"): the rocket was filled with its events
// enabled, so the document is not saved; OpenRocket's loader ends with clearUndo(), and its
// caller sets the file and the saved state.
TEST(OpenRocketDocumentSaved, ADocumentThatWasFilledIsNotSavedUntilItsCallerSaysSo)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createEmptyRocket();
    DocumentRecorder                          events(*d, true);

    d->getRocket().addChild(std::make_unique<QtRocket::AxialStage>());
    EXPECT_EQ(events.take(), "U D(Rocket,saved) D(Rocket)");
    d->clearUndo();
    EXPECT_EQ(events.take(), "U");

    EXPECT_FALSE(d->isSaved());
    EXPECT_EQ(d->getFile(), std::nullopt);
    EXPECT_EQ(undoState(*d), "pos=0 hist=1 desc=[null] next=null stored=null");
    EXPECT_FALSE(d->isUndoAvailable());

    d->setFile(std::filesystem::path("loaded.ork"));
    d->setSaved(true);
    EXPECT_TRUE(d->isSaved());
}

// ============================================================================== undo errors

// DocumentProbe3, section D ("the undo error sites").
TEST(OpenRocketDocumentUndoErrors, AreReportedAndTheCallCarriesOn)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    DocumentRecorder    events(d, true);

    // "redo (none available)" and "undo (none available)"
    d.redo();
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: Redo not available) U");
    d.undo();
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: Undo not available) U");
    EXPECT_EQ(state(d), kUntouched);

    // "startUndo(First)": the stored description is the one that was in force, none.
    d.startUndo("First");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=0 hist=1 desc=[null] next=First stored=null undoAvail=false "
              "undoDesc=null redoAvail=false redoDesc=null saved=true sims=0");

    // "startUndo(Second) while First is open": no error either, as nothing was stored.
    alpha.nose().setName("n1");
    events.clear();
    d.startUndo("Second");
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[First, null] next=Second stored=First undoAvail=true "
              "undoDesc=First redoAvail=false redoDesc=null saved=false sims=0");

    // "addUndoPosition(Third) with a stored description"
    alpha.nose().setName("n2");
    events.clear();
    d.addUndoPosition("Third");
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: addUndoPosition called while "
              "storedDescription=First description=Third)");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[First, Second, null] next=Third stored=First "
              "undoAvail=true undoDesc=Second redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "undo with a stored description"
    alpha.nose().setName("n3");
    events.clear();
    d.undo();
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: undo() called with storedDescription=First) "
              "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=2 hist=4 desc=[First, Second, Third, null] next=Third stored=First "
              "undoAvail=true undoDesc=Second redoAvail=true redoDesc=Third saved=false "
              "sims=0");
    EXPECT_EQ(alpha.nose().getName(), "n2");

    // "redo with a stored description", "name=n3"
    d.redo();
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: redo() called with storedDescription=First) "
              "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=3 hist=4 desc=[First, Second, Third, null] next=Third stored=First "
              "undoAvail=true undoDesc=Third redoAvail=false redoDesc=null saved=false "
              "sims=0");
    EXPECT_EQ(alpha.nose().getName(), "n3");

    // "stopUndo" and "stopUndo again"
    d.stopUndo();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(undoState(d),
              "pos=3 hist=4 desc=[First, Second, Third, null] next=First stored=null");
    d.stopUndo();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(undoState(d), "pos=3 hist=4 desc=[First, Second, Third, null] next=null stored=null");
}

// DocumentProbe4, section B ("startUndo while a description is stored; null descriptions").
// Deviation: Java's log has both errors of a startUndo() (its own and that of the
// addUndoPosition() it calls), its error handler the first one of a run; here every error is
// emitted.
TEST(OpenRocketDocumentUndoErrors, StartUndoWhileADescriptionIsStoredReportsTwoErrors)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    d.addUndoPosition("Outer");
    alpha.nose().setName("o1");
    d.startUndo("Timed");
    DocumentRecorder events(d);

    // "addUndoPosition(Outer), change, startUndo(Timed)"
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Outer, null] next=Timed stored=Outer undoAvail=true "
              "undoDesc=Outer redoAvail=false redoDesc=null saved=false sims=0");

    // "startUndo(Again)"
    d.startUndo("Again");
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: startUndo called while storedDescription=Outer "
              "description=Again) ERR(Undo/Redo error: addUndoPosition called while "
              "storedDescription=Outer description=Again)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Outer, null] next=Again stored=Timed undoAvail=true "
              "undoDesc=Outer redoAvail=false redoDesc=null saved=false sims=0");

    // "startUndo(null)": a description that is none is printed as Java prints null.
    d.startUndo(std::nullopt);
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: startUndo called while storedDescription=Timed "
              "description=null) ERR(Undo/Redo error: addUndoPosition called while "
              "storedDescription=Timed description=null)");
    EXPECT_EQ(state(d),
              "pos=1 hist=2 desc=[Outer, null] next=null stored=Again undoAvail=true "
              "undoDesc=Outer redoAvail=false redoDesc=null saved=false sims=0");

    // "change, addUndoPosition(null)"
    alpha.nose().setName("o2");
    events.clear();
    d.addUndoPosition(std::nullopt);
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: addUndoPosition called while "
              "storedDescription=Again description=null)");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Outer, null, null] next=null stored=Again "
              "undoAvail=true undoDesc=null redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "stopUndo"
    d.stopUndo();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Outer, null, null] next=Again stored=null "
              "undoAvail=true undoDesc=null redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "change"
    alpha.nose().setName("o3");
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    EXPECT_EQ(state(d),
              "pos=2 hist=3 desc=[Outer, null, Again] next=Again stored=null "
              "undoAvail=true undoDesc=Again redoAvail=false redoDesc=null saved=false "
              "sims=0");

    // "undo"
    d.undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(state(d),
              "pos=2 hist=4 desc=[Outer, null, Again, null] next=Again stored=null "
              "undoAvail=true undoDesc=null redoAvail=true redoDesc=Again saved=false "
              "sims=0");
}

/// An extension that names a flight data type, as OpenRocket's RollControl names its "Control
/// fin cant". The symbol is this file's own (see OpenRocketDocumentTypes).
class TypedExtension final : public AbstractSimulationExtension
{
public:
    TypedExtension() : AbstractSimulationExtension("test.TypedExtension") { }

    void initialize(SimulationConditions& /*conditions*/) override { }

    [[nodiscard]] std::vector<const FlightDataType*> getFlightDataTypes() const override
    {
        return {
            &FlightDataType::getType("Control fin cant qtrDoc", "qtrDocFc", UnitGroupId::ANGLE)};
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<TypedExtension>(*this);
    }
};

/// The fixture of DocumentProbe3's section E: a simulation "A" that was renamed twice, each
/// with an undo position, and two undos; then an extension is added to the simulation's list,
/// which changes without an event. The document is now dirty although it has redo states.
class OpenRocketDocumentInconsistency : public ::testing::Test
{
protected:
    OpenRocketDocumentInconsistency() : m_simulation(m_alpha.newSimulation("A", testFcid(0)))
    {
        OpenRocketDocument& d = m_alpha.document();
        d.addSimulation(m_simulation);
        d.clearUndo();
        d.addUndoPosition("Rename");
        m_simulation->setName("A2");
        d.addUndoPosition("Rename again");
        m_simulation->setName("A3");
        d.undo();
        d.undo();
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_alpha.document(); }
    [[nodiscard]] Simulation&         simulation() const noexcept { return *m_simulation; }

    void addExtension() const
    {
        m_simulation->getSimulationExtensions().push_back(std::make_shared<TypedExtension>());
    }

private:
    AlphaDocument               m_alpha;
    std::shared_ptr<Simulation> m_simulation;
};

// DocumentProbe3, section E ("undo position inconsistency through the extension list"), the
// first document.
TEST_F(OpenRocketDocumentInconsistency, IsReportedAndRepairedByAnUndoPosition)
{
    OpenRocketDocument& d = document();
    DocumentRecorder    events(d);

    // "two edits, two undos"
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=3 desc=[Rename, Rename again, null] next=Rename again stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Rename saved=false sims=1 "
              "configs=5");

    // "extension added to the list (fires nothing)"
    addExtension();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=3 desc=[Rename, Rename again, null] next=Rename again stored=null "
              "undoAvail=true undoDesc=Rename redoAvail=true redoDesc=Rename saved=false sims=1 "
              "configs=5");

    // "addUndoPosition(After)": the redo states go.
    d.addUndoPosition("After");
    EXPECT_EQ(events.take(), "ERR(Undo/Redo error: undo position inconsistency)");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=1 hist=2 desc=[Rename, null] next=After stored=null undoAvail=true "
              "undoDesc=Rename redoAvail=false redoDesc=null saved=false sims=1 configs=5");
}

// DocumentProbe3, section E, the second document ("d2").
TEST_F(OpenRocketDocumentInconsistency, IsReportedByAnUndoWhichKeepsTheRedoStates)
{
    OpenRocketDocument& d = document();
    addExtension();
    DocumentRecorder events(d);

    // "d2: undo": the current state is appended behind the redo states, and the state at the
    // position is loaded. "name=A extensions=0"
    d.undo();
    EXPECT_EQ(events.take(),
              "ERR(Undo/Redo error: undo position inconsistency) U D(Rocket) "
              "D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=0 hist=4 desc=[Rename, Rename again, null, null] next=Rename again stored=null "
              "undoAvail=false undoDesc=null redoAvail=true redoDesc=Rename saved=false sims=1 "
              "configs=5");
    EXPECT_EQ(simulation().getName(), "A");
    EXPECT_EQ(simulation().getSimulationExtensions().size(), 0U);

    // "d2: redo", three times: "name=A2 extensions=0", "name=A3 extensions=0", "name=A
    // extensions=1".
    d.redo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(d),
              "pos=1 hist=4 desc=[Rename, Rename again, null, null] next=Rename again stored=null "
              "undoAvail=true undoDesc=Rename redoAvail=true redoDesc=Rename again saved=false "
              "sims=1 configs=5");
    EXPECT_EQ(simulation().getName(), "A2");
    EXPECT_EQ(simulation().getSimulationExtensions().size(), 0U);

    d.redo();
    EXPECT_EQ(stateWithConfigs(d),
              "pos=2 hist=4 desc=[Rename, Rename again, null, null] next=Rename again stored=null "
              "undoAvail=true undoDesc=Rename again redoAvail=true redoDesc=null saved=false "
              "sims=1 configs=5");
    EXPECT_EQ(simulation().getName(), "A3");
    EXPECT_EQ(simulation().getSimulationExtensions().size(), 0U);

    d.redo();
    EXPECT_EQ(stateWithConfigs(d),
              "pos=3 hist=4 desc=[Rename, Rename again, null, null] next=Rename again stored=null "
              "undoAvail=true undoDesc=null redoAvail=false redoDesc=null saved=false sims=1 "
              "configs=5");
    EXPECT_EQ(simulation().getName(), "A");
    EXPECT_EQ(simulation().getSimulationExtensions().size(), 1U);
}

// ================================================================ slots that change the document

/// Clears the undo history of a document when it tells of the first change of a simulation, as
/// a slot may: the state that is being loaded then has to outlive the history.
class ClearUndoOnSimulationChange
{
public:
    explicit ClearUndoOnSimulationChange(OpenRocketDocument& document)
      : m_connection(
            document.documentChanged().connect([this, &document](const DocumentChangeEvent& event) {
                if (event.getSimulation() != nullptr && !m_done)
                {
                    m_done = true;
                    document.clearUndo();
                }
            }))
    {
    }

    [[nodiscard]] bool done() const noexcept { return m_done; }

private:
    bool                                                           m_done{false};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// Java keeps the list it loads from alive by reference; here the state is shared.
TEST(OpenRocketDocumentSlots, ASlotMayClearTheHistoryWhileAStateIsLoaded)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    const std::shared_ptr<Simulation> b = alpha.newSimulation("B", testFcid(1));
    d.addSimulation(a);
    d.addSimulation(b);
    d.clearUndo();
    d.addUndoPosition("Rename and edit");
    a->getOptions().setLaunchRodLength(1.5);
    b->setName("B2");
    const ClearUndoOnSimulationChange clearing(d);

    d.undo();
    ASSERT_TRUE(clearing.done());

    // Both simulations were loaded from the state, although the first one's event cleared the
    // history.
    EXPECT_EQ(a->getOptions().getLaunchRodLength(), 0.0);
    EXPECT_EQ(b->getName(), "B");
    EXPECT_EQ(undoState(d), "pos=0 hist=1 desc=[null] next=Rename and edit stored=null");
    EXPECT_FALSE(d.isRedoAvailable());
}

/// A slot of a document that throws a BugError when the document first passes on an event of
/// the rocket.
class ThrowOnRocketEvent
{
public:
    explicit ThrowOnRocketEvent(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            if (event.getComponent() != nullptr && m_armed)
            {
                m_armed = false;
                QtRocket::bug("a slot failed");
            }
        }))
    {
    }

private:
    bool                                                           m_armed{true};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// Java's finally block: when loading a state fails, the document is out of its undo mode again
// and undoRedoChanged() is emitted on the way out.
TEST(OpenRocketDocumentSlots, AFailureWhileAStateIsLoadedLeavesTheUndoMode)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    d.clearUndo();
    d.addUndoPosition("Edit");
    alpha.nose().setName("edited");
    DocumentRecorder         events(d);
    const ThrowOnRocketEvent failing(d);

    EXPECT_THROW(d.undo(), BugError);

    // The recorder heard the rocket's event before the slot that failed (slots run in the order
    // they were connected), and then the U of the way out. The simulations were not reached.
    EXPECT_EQ(events.take(), "U D(Rocket) U");

    // Not in the undo mode: a change of the simulation makes an undo step again (in the undo
    // mode it would be "D(Simulation)" alone).
    a->setName("A2");
    EXPECT_EQ(events.take(), "U D(Simulation)");
}

/// Runs an action once, when a document first passes on an event of its rocket itself (the
/// event of an undo or redo): the listener of the probe Reenter.
class OnFirstRocketEvent
{
public:
    OnFirstRocketEvent(OpenRocketDocument& document, std::function<void()> action)
      : m_action(std::move(action)),
        m_connection(
            document.documentChanged().connect([this, &document](const DocumentChangeEvent& event) {
                if (event.getComponent() == &document.getRocket() && !m_done)
                {
                    m_done = true;
                    m_action();
                }
            }))
    {
    }

    [[nodiscard]] bool done() const noexcept { return m_done; }

private:
    std::function<void()>                                          m_action;
    bool                                                           m_done{false};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// The undo errors a document reports, in order.
class UndoErrors
{
public:
    explicit UndoErrors(OpenRocketDocument& document)
      : m_connection(document.undoErrorOccurred().connect(
            [this](const std::string& text) { m_texts.push_back(text); }))
    {
    }

    [[nodiscard]] const std::vector<std::string>& texts() const noexcept { return m_texts; }

private:
    std::vector<std::string>                               m_texts;
    QtRocket::Signal<const std::string&>::ScopedConnection m_connection;
};

/// The fixture of the probe Reenter: a simulation "A" in the list, an undo position, then the
/// nose cone and the simulation renamed. An undo would put both names back.
class OpenRocketDocumentReenter : public ::testing::Test
{
protected:
    OpenRocketDocumentReenter() : m_simulation(m_alpha.newSimulation("A", testFcid(0)))
    {
        OpenRocketDocument& d = m_alpha.document();
        d.addSimulation(m_simulation);
        d.clearUndo();
        d.addUndoPosition("Edit");
        m_alpha.nose().setName("edited");
        m_simulation->setName("A2");
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_alpha.document(); }

    /// What the probe prints after the undo: "nose=<name> sim=<name> undoAvail=<bool>
    /// redoAvail=<bool>".
    [[nodiscard]] std::string outcome() const
    {
        return std::format("nose={} sim={} undoAvail={} redoAvail={}", m_alpha.nose().getName(),
                           m_simulation->getName(), document().isUndoAvailable(),
                           document().isRedoAvailable());
    }

private:
    AlphaDocument               m_alpha;
    std::shared_ptr<Simulation> m_simulation;
};

// Reenter: "clearUndo in the rocket's undo event: nose=Nose Cone sim=A2 undoAvail=false
// redoAvail=false". Java asks its history for the simulations after the rocket is loaded, so
// they are loaded from the state the slot made, which holds them as they are.
TEST_F(OpenRocketDocumentReenter, AfterASlotClearedTheHistoryTheSimulationsStayAsTheyAre)
{
    OpenRocketDocument&      d = document();
    const OnFirstRocketEvent clearing(d, [&d] { d.clearUndo(); });

    d.undo();

    ASSERT_TRUE(clearing.done());
    EXPECT_EQ(outcome(), "nose=Nose Cone sim=A2 undoAvail=false redoAvail=false");
    EXPECT_EQ(undoState(d), "pos=0 hist=1 desc=[null] next=Edit stored=null");
}

// Reenter: "ERR(Undo/Redo error: undo position inconsistency)" and "addUndoPosition in the
// rocket's undo event: nose=Nose Cone sim=A2 undoAvail=true redoAvail=false".
TEST_F(OpenRocketDocumentReenter, AfterASlotAddedAnUndoPositionTheSimulationsStayAsTheyAre)
{
    OpenRocketDocument&      d = document();
    const UndoErrors         errors(d);
    const OnFirstRocketEvent adding(d, [&d] { d.addUndoPosition("Inside"); });

    d.undo();

    ASSERT_TRUE(adding.done());
    EXPECT_EQ(errors.texts(),
              std::vector<std::string>{"Undo/Redo error: undo position inconsistency"});
    EXPECT_EQ(outcome(), "nose=Nose Cone sim=A2 undoAvail=true redoAvail=false");
    EXPECT_EQ(undoState(d), "pos=1 hist=2 desc=[Edit, null] next=Inside stored=null");
}

// ---- undo() and redo() from a slot ---------------------------------------------------------
//
// Not OpenRocket's, where a listener may undo while the rocket's event is delivered: the
// replaced components are kept by the collector there. Here an undo frees them, so the call
// is refused with a BugError before it changes anything, and whoever fired the event, the
// later slots and the document go on with components that are still there. These tests are
// the cases that read freed memory before (they run under the asan preset too).

/// Calls undo() or redo() of a document once, when the document first passes on an event of a
/// component, and keeps the text of the BugError the call ends in.
class UndoInSlot
{
public:
    enum class Call
    {
        UNDO,
        REDO,
    };

    explicit UndoInSlot(OpenRocketDocument& document, Call call = Call::UNDO)
      : m_call(call),
        m_connection(
            document.documentChanged().connect([this, &document](const DocumentChangeEvent& event) {
                if (event.getComponent() != nullptr && !m_done)
                {
                    m_done = true;
                    attempt(document);
                }
            }))
    {
    }

    [[nodiscard]] bool               done() const noexcept { return m_done; }
    [[nodiscard]] const std::string& refusal() const noexcept { return m_refusal; }

private:
    void attempt(OpenRocketDocument& document)
    {
        try
        {
            if (m_call == Call::UNDO)
            {
                document.undo();
            }
            else
            {
                document.redo();
            }
            m_refusal = "not refused";
        }
        catch (const BugError& error)
        {
            m_refusal = error.what();
        }
    }

    Call                                                           m_call;
    bool                                                           m_done{false};
    std::string                                                    m_refusal;
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// Appends the name of the component of every event a document passes on.
class ComponentNames
{
public:
    explicit ComponentNames(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            if (const QtRocket::RocketComponent* const component = event.getComponent())
            {
                m_names += component->getName() + ";";
            }
        }))
    {
    }

    [[nodiscard]] const std::string& names() const noexcept { return m_names; }

private:
    std::string                                                    m_names;
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

constexpr std::string_view kUndoRefused =
    "OpenRocketDocument::undo() was called from a slot while the document delivers an event of "
    "the rocket or loads a state";
constexpr std::string_view kRedoRefused =
    "OpenRocketDocument::redo() was called from a slot while the document delivers an event of "
    "the rocket or loads a state";

TEST(OpenRocketDocumentReentrantUndo, IsRefusedInASlotOfARocketEvent)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    d.addUndoPosition("a");
    alpha.body().setLength(0.5);
    d.addUndoPosition("b");
    const UndoInSlot     undoing(d);
    const ComponentNames reading(d);
    const std::string    before = undoState(d);

    alpha.body().setLength(0.7);

    ASSERT_TRUE(undoing.done());
    EXPECT_TRUE(undoing.refusal().contains(kUndoRefused)) << undoing.refusal();
    // The second slot and the second emission read the component the event names.
    EXPECT_EQ(reading.names(), "Body Tube;Body Tube;");
    // The edit went through as if nobody had tried: nothing was undone.
    EXPECT_EQ(alpha.body().getLength(), 0.7);
    EXPECT_EQ(before, "pos=1 hist=2 desc=[a, null] next=b stored=null");
    EXPECT_EQ(undoState(d), "pos=1 hist=2 desc=[a, b] next=b stored=null");
    // Outside the event the undo is the caller's again.
    d.undo();
    EXPECT_EQ(alpha.body().getLength(), 0.5);
}

TEST(OpenRocketDocumentReentrantUndo, RedoIsRefusedToo)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    d.addUndoPosition("a");
    alpha.body().setLength(0.5);
    d.undo();
    ASSERT_TRUE(d.isRedoAvailable());
    const UndoInSlot redoing(d, UndoInSlot::Call::REDO);

    alpha.nose().setName("renamed");

    ASSERT_TRUE(redoing.done());
    EXPECT_TRUE(redoing.refusal().contains(kRedoRefused)) << redoing.refusal();
    EXPECT_EQ(alpha.nose().getName(), "renamed");
}

/// Calls undo() of a document whenever the document passes on an event, and catches nothing.
class UndoOnEveryEvent
{
public:
    explicit UndoOnEveryEvent(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect(
            [&document](const DocumentChangeEvent& /*event*/) { document.undo(); }))
    {
    }

private:
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// Notes whether a document passed on an event of its rocket itself: the event of an undo or
/// redo.
class RocketEventSeen
{
public:
    explicit RocketEventSeen(OpenRocketDocument& document)
      : m_connection(
            document.documentChanged().connect([this, &document](const DocumentChangeEvent& event) {
                m_seen = m_seen || event.getComponent() == &document.getRocket();
            }))
    {
    }

    [[nodiscard]] bool seen() const noexcept { return m_seen; }

private:
    bool                                                           m_seen{false};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// Without a slot that catches it, the BugError leaves the call that fired the event.
TEST(OpenRocketDocumentReentrantUndo, TheBugErrorLeavesTheSetterWhenNoSlotCatchesIt)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    d.addUndoPosition("a");
    alpha.body().setLength(0.5);
    d.addUndoPosition("b");
    const RocketEventSeen  undone(d);
    const UndoOnEveryEvent undoing(d);

    EXPECT_THROW(alpha.body().setLength(0.7), BugError);
    // No state was loaded.
    EXPECT_FALSE(undone.seen());
}

/// In the rocket's event of an undo: tries to undo once more and then changes an option of a
/// simulation, as a careless slot might. The nested undo used to end the undo mode of the
/// outer one, and the change then replaced the state the outer one was still loading.
class NestedUndoAndEdit
{
public:
    NestedUndoAndEdit(OpenRocketDocument& document, std::shared_ptr<Simulation> simulation)
      : m_simulation(std::move(simulation)),
        m_connection(
            document.documentChanged().connect([this, &document](const DocumentChangeEvent& event) {
                if (event.getComponent() != nullptr && !m_done)
                {
                    m_done = true;
                    act(document);
                }
            }))
    {
    }

    [[nodiscard]] bool done() const noexcept { return m_done; }
    /// Whether the nested undo() ended in a BugError.
    [[nodiscard]] bool refused() const noexcept { return m_refused; }

private:
    void act(OpenRocketDocument& document)
    {
        try
        {
            document.undo();
        }
        catch (const BugError&)
        {
            m_refused = true;
        }
        m_simulation->getOptions().setLaunchRodLength(5.0);
    }

    std::shared_ptr<Simulation>                                    m_simulation;
    bool                                                           m_done{false};
    bool                                                           m_refused{false};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

TEST(OpenRocketDocumentReentrantUndo, IsRefusedWhileAStateIsLoadedAndTheUndoModeStays)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    d.clearUndo();
    d.addUndoPosition("one");
    a->getOptions().setLaunchRodLength(1.0);
    const NestedUndoAndEdit nesting(d, a);

    d.undo();

    ASSERT_TRUE(nesting.done());
    EXPECT_TRUE(nesting.refused());
    // The state was loaded to its end: the simulation has the rod length of the state, whatever
    // the slot set meanwhile, and the history is the one of a single undo.
    EXPECT_EQ(a->getOptions().getLaunchRodLength(), 0.0);
    EXPECT_EQ(undoState(d), "pos=0 hist=2 desc=[one, null] next=one stored=null");
    EXPECT_TRUE(d.isRedoAvailable());
}

// ---- a slot of the undo error -----------------------------------------------------------------

/// Clears the undo history of a document when it reports an undo error: the obvious way for an
/// application to recover from one.
class ClearUndoOnError
{
public:
    explicit ClearUndoOnError(OpenRocketDocument& document)
      : m_connection(document.undoErrorOccurred().connect(
            [&document](const std::string& /*text*/) { document.clearUndo(); }))
    {
    }

private:
    QtRocket::Signal<const std::string&>::ScopedConnection m_connection;
};

// Not Java's, whose error handler is no listener of the document. undo() used to carry on with
// the position it had tested before the error went out: it moved back from position 0 of the
// one state the slot had left and read beyond the history.
TEST(OpenRocketDocumentUndoErrors, ASlotOfTheErrorMayClearTheHistoryBeforeAnUndo)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    d.addUndoPosition("a");
    alpha.body().setLength(0.5);
    d.addUndoPosition("b");
    d.startUndo("c");
    ASSERT_EQ(undoState(d), "pos=1 hist=2 desc=[a, null] next=c stored=b");
    const ClearUndoOnError clearing(d);
    DocumentRecorder       events(d);

    d.undo();

    // The U of the slot's clearUndo(), the error, and the U of an undo that found nothing left
    // to undo.
    EXPECT_EQ(events.take(), "U ERR(Undo/Redo error: undo() called with storedDescription=b) U");
    EXPECT_EQ(undoState(d), "pos=0 hist=1 desc=[null] next=c stored=b");
    EXPECT_FALSE(d.isUndoAvailable());
    EXPECT_EQ(alpha.body().getLength(), 0.5);
}

TEST(OpenRocketDocumentUndoErrors, ASlotOfTheErrorMayClearTheHistoryBeforeARedo)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d        = alpha.document();
    const double        original = alpha.body().getLength();
    d.addUndoPosition("a");
    alpha.body().setLength(0.5);
    d.undo();
    d.startUndo("c");
    ASSERT_EQ(undoState(d), "pos=0 hist=2 desc=[a, null] next=c stored=a");
    const ClearUndoOnError clearing(d);
    DocumentRecorder       events(d);

    d.redo();

    EXPECT_EQ(events.take(), "U ERR(Undo/Redo error: redo() called with storedDescription=a) U");
    EXPECT_EQ(undoState(d), "pos=0 hist=1 desc=[null] next=c stored=a");
    EXPECT_FALSE(d.isRedoAvailable());
    EXPECT_EQ(alpha.body().getLength(), original);
}

// ---- other slots that change the document ------------------------------------------------------

/// Removes a simulation from a document's list when the document tells of its change.
class RemoveChangedSimulation
{
public:
    explicit RemoveChangedSimulation(OpenRocketDocument& document)
      : m_connection(
            document.documentChanged().connect([&document](const DocumentChangeEvent& event) {
                Simulation* const simulation = event.getSimulation();
                if (simulation != nullptr && document.getSimulationIndex(*simulation).has_value())
                {
                    document.removeSimulation(*simulation);
                }
            }))
    {
    }

private:
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// A slot may take the simulation it is told of out of the list when somebody else holds it (the
// header's rule): the simulation goes on telling its own listeners and can still be run.
TEST(OpenRocketDocumentSlots, ASlotMayRemoveTheSimulationItIsToldOf)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<Simulation> a = alpha.newSimulation("A", testFcid(0));
    d.addSimulation(a);
    d.clearUndo();
    const RemoveChangedSimulation removing(d);
    DocumentRecorder              events(d);

    a->setName("gone");

    EXPECT_EQ(d.getSimulationCount(), 0U);
    // The slot's removal: U S(Simulation); then the recorder hears the change that started it.
    EXPECT_EQ(events.take(), "U U S(Simulation) D(Simulation)");
    // Out of the list, it is no longer heard.
    a->setName("still mine");
    EXPECT_EQ(events.take(), "");
    simulateOrFail(*a);
    EXPECT_EQ(a->getName(), "still mine");
}

// ============================================================= preferences, saving, fire

// DocumentProbe3, section F ("document preferences, saving event, fire functions").
TEST(OpenRocketDocumentEvents, APreferenceChangeAndTheFireFunctions)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createNewRocket();
    DocumentRecorder                          events(*d, true);
    const std::string_view                    unsaved =
        "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null "
        "redoAvail=false redoDesc=null saved=false sims=0 configs=0";

    // "putBoolean": the document is unsaved when the event is emitted, and no undo step is made.
    d->getDocumentPreferences().putBoolean(DocumentPreferences::kPref3DShadowsEnabled, true);
    EXPECT_EQ(events.take(), "D(DocumentPreferences)");
    EXPECT_EQ(stateWithConfigs(*d), unsaved);

    // "putBoolean (same value)"
    d->getDocumentPreferences().putBoolean(DocumentPreferences::kPref3DShadowsEnabled, true);
    EXPECT_EQ(events.take(), "");

    // "removePreference"
    d->getDocumentPreferences().removePreference(DocumentPreferences::kPref3DShadowsEnabled);
    EXPECT_EQ(events.take(), "D(DocumentPreferences)");

    // "fireDocumentSavingEvent"
    d->fireDocumentSavingEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::DOCUMENT, .source = d.get()});
    EXPECT_EQ(events.take(), "SAVING(OpenRocketDocument)");

    // "fireDocumentChangeEvent(SimulationChangeEvent)"
    d->fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::SIMULATION, .source = d.get()});
    EXPECT_EQ(events.take(), "S(OpenRocketDocument)");
    EXPECT_EQ(stateWithConfigs(*d), unsaved);
}

// The fire functions pass an event on and change nothing: a window fires events with itself
// as the source (here: no source).
TEST(OpenRocketDocumentEvents, FiringAnEventDoesNotChangeTheDocument)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createNewRocket();
    DocumentRecorder                          events(*d, true);

    d->fireDocumentChangeEvent(DocumentChangeEvent{});
    d->fireDocumentSavingEvent(DocumentChangeEvent{});

    EXPECT_EQ(events.take(), "D(null,saved) SAVING(null,saved)");
    EXPECT_TRUE(d->isSaved());
    EXPECT_FALSE(d->isUndoAvailable());
}

// ============================================================ flight data types, expressions

// The custom expressions and the extension of these tests register flight data types in the
// process-wide registry of FlightDataType. Their symbols are this file's own (prefix "qtrDoc"),
// so that no other test finds another type under a symbol it looks up.

// DocumentProbe3, section H ("flight data types of a document").
TEST(OpenRocketDocumentTypes, AreTheBuiltInOnesTheExpressionsAndTheExtensionsWithoutRepeats)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    // "plain: 71 ALL_TYPES=71"
    EXPECT_EQ(d.getFlightDataTypes().size(), 71U);

    const CustomExpression e1("Kinetic energy qtrDoc", "qtrDocKE", "J", "0.5*m*Vt^2");
    const CustomExpression e2("Fixed qtrDoc", "qtrDocPf", "furlongs", "1");
    // Named as a built-in type, with another symbol: equal to the built-in type (the names are
    // compared ignoring case), so it is not listed again.
    const CustomExpression e3("altitude", "qtrDocALT", "m", "h");
    d.addCustomExpression(e1);
    d.addCustomExpression(e2);
    d.addCustomExpression(e3);
    // "expressions=4": an expression the list holds already is added all the same.
    d.addCustomExpression(e1);
    EXPECT_EQ(d.getCustomExpressions().size(), 4U);

    const std::shared_ptr<Simulation> s  = alpha.newSimulation();
    const std::shared_ptr<Simulation> s2 = alpha.newSimulation();
    s->getSimulationExtensions().push_back(std::make_shared<TypedExtension>());
    s2->getSimulationExtensions().push_back(std::make_shared<TypedExtension>());
    d.addSimulation(s);
    d.addSimulation(s2);

    // "with 4 expressions (1 repeated, 1 named as a built-in) and 2 roll controls: 74"
    const std::vector<const FlightDataType*> types = d.getFlightDataTypes();
    ASSERT_EQ(types.size(), 74U);
    // "the first 71 are ALL_TYPES in order: true"
    const std::vector<const FlightDataType*> builtIn(FlightDataType::allTypes().begin(),
                                                     FlightDataType::allTypes().end());
    EXPECT_EQ(std::vector<const FlightDataType*>(types.begin(), types.begin() + 71), builtIn);
    // "[71] Kinetic energy probe / KEp / UnitGroup", "[72] Probe fixed / Pf / FixedUnitGroup",
    // "[73] Control fin cant / (alpha)fc / UnitGroup"
    EXPECT_EQ(types[71], &e1.getType());
    EXPECT_EQ(types[72], &e2.getType());
    EXPECT_EQ(types[73]->getName(), "Control fin cant qtrDoc");
    EXPECT_EQ(types[73]->getSymbol(), "qtrDocFc");
}

// DocumentProbe3, section H: "after removeCustomExpression(e1): 3 first=Probe fixed", "after
// removing one that is not there: 3".
TEST(OpenRocketDocumentTypes, TheFirstEqualCustomExpressionIsRemoved)
{
    const AlphaDocument    alpha;
    OpenRocketDocument&    d = alpha.document();
    const CustomExpression e1("Kinetic energy qtrDoc", "qtrDocKE", "J", "0.5*m*Vt^2");
    const CustomExpression e2("Fixed qtrDoc", "qtrDocPf", "furlongs", "1");
    d.addCustomExpression(e1);
    d.addCustomExpression(e2);
    d.addCustomExpression(e1);

    d.removeCustomExpression(e1);
    EXPECT_EQ(d.getCustomExpressions(), (std::vector<CustomExpression>{e2, e1}));

    d.removeCustomExpression(CustomExpression("nobody", "qtrDocNb", "", "1"));
    EXPECT_EQ(d.getCustomExpressions().size(), 2U);

    // The list itself is handed out, as in Java.
    d.getCustomExpressions().clear();
    const OpenRocketDocument& c = d;
    EXPECT_TRUE(c.getCustomExpressions().empty());
}

// ================================================= photo settings, storage options, the others

TEST(OpenRocketDocumentSettings, ThePhotoSettingsAreAMapOfTexts)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    EXPECT_TRUE(d.getPhotoSettings().empty());

    d.setPhotoSettings({{"roll", "1.5"}, {"sky", "Miramar"}});
    EXPECT_EQ(d.getPhotoSettings().size(), 2U);
    EXPECT_EQ(d.getPhotoSettings().at("sky"), "Miramar");

    // The map itself is handed out, as in Java.
    d.getPhotoSettings().insert_or_assign("roll", "2.5");
    const OpenRocketDocument& c = d;
    EXPECT_EQ(c.getPhotoSettings(),
              (std::map<std::string, std::string>{{"roll", "2.5"}, {"sky", "Miramar"}}));

    // Neither an event nor a change of the saved state, as in Java.
    EXPECT_TRUE(d.isSaved());
}

TEST(OpenRocketDocumentSettings, TheDefaultStorageOptionsAreTheDocumentsOwn)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();

    d.getDefaultStorageOptions().setSaveSimulationData(true);
    d.getDefaultStorageOptions().setExplicitlySet(true);

    const OpenRocketDocument& c = d;
    EXPECT_TRUE(c.getDefaultStorageOptions().getSaveSimulationData());
    EXPECT_TRUE(c.getDefaultStorageOptions().isExplicitlySet());
    EXPECT_EQ(c.getDefaultStorageOptions().getFileType(), StorageOptions::FileType::OPENROCKET);
    EXPECT_TRUE(d.isSaved());
}

// ==================================================================================== decals

/// An attachment that is not a file (as an entry of the archive a design was loaded from).
[[nodiscard]] std::shared_ptr<const Attachment> entry(const std::string& name)
{
    return std::make_shared<MemoryAttachment>(name, "image");
}

/// An appearance with the default paint and shine and a decal of @p image (Java:
/// `AppearanceBuilder ab = new AppearanceBuilder(); ab.setImage(img); ab.getAppearance()`).
[[nodiscard]] Appearance appearanceWith(const DecalImage& image)
{
    AppearanceBuilder builder;
    builder.setImage(image.getName());
    return builder.getAppearance();
}

/// The names of the images of @p document's decal list, as Java prints the list:
/// "[decals/a (1).png, decals/a.png]".
[[nodiscard]] std::string decalList(const OpenRocketDocument& document)
{
    std::vector<std::string> names;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        names.push_back(image->getName());
    }
    return std::format("[{}]", QtRocket::Test::joined(names, ", "));
}

// DocumentProbe2, section C ("decals"), up to the second makeUniqueDecal.
TEST(OpenRocketDocumentDecals, UsageIsCountedOutsideAndInside)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    DocumentRecorder                  events(d);

    // "nose.setAppearance(decal)"
    alpha.nose().setAppearance(appearanceWith(*img));
    EXPECT_EQ(events.take(), "U D(NoseCone) D(NoseCone)");
    alpha.body().setAppearance(appearanceWith(*img));
    alpha.body().getInsideColorComponentHandler().setInsideAppearance(appearanceWith(*img));

    // "usage=3": the nose cone, and the body tube outside and inside.
    EXPECT_EQ(d.countDecalUsage(*img), 3);

    // "makeUniqueDecal -> 'decals/a (1).png' same=false list=[decals/a (1).png, decals/a.png]"
    const std::shared_ptr<DecalImage> unique = d.makeUniqueDecal(img);
    ASSERT_NE(unique, nullptr);
    EXPECT_EQ(unique->getName(), "decals/a (1).png");
    EXPECT_NE(unique, img);
    EXPECT_EQ(decalList(d), "[decals/a (1).png, decals/a.png]");

    // "again -> 'decals/a (2).png' list=[decals/a (1).png, decals/a (2).png, decals/a.png]"
    const std::shared_ptr<DecalImage> unique2 = d.makeUniqueDecal(img);
    ASSERT_NE(unique2, nullptr);
    EXPECT_EQ(unique2->getName(), "decals/a (2).png");
    EXPECT_EQ(decalList(d), "[decals/a (1).png, decals/a (2).png, decals/a.png]");
    EXPECT_EQ(d.findDecalImage("decals/a (2).png"), unique2);
    EXPECT_EQ(d.findDecalImage("decals/a (3).png"), nullptr);
}

// DocumentProbe3, section J ("decals: usage, unique, change events, removal"), the counts.
TEST(OpenRocketDocumentDecals, AnImageUsedAtMostOnceIsUniqueAlready)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d     = alpha.document();
    const std::shared_ptr<DecalImage> img   = d.getDecalImage(entry("decals/a.png"));
    const std::shared_ptr<DecalImage> other = d.getDecalImage(entry("decals/b.png"));

    // "same image for the same name: true"
    EXPECT_EQ(d.getDecalImage(entry("decals/a.png")), img);
    EXPECT_EQ(d.findDecalImage("decals/a.png"), img);

    // "usage unused=0", "makeUniqueDecal unused -> same=true list=[decals/a.png, decals/b.png]"
    EXPECT_EQ(d.countDecalUsage(*img), 0);
    EXPECT_EQ(d.makeUniqueDecal(img), img);
    EXPECT_EQ(decalList(d), "[decals/a.png, decals/b.png]");

    // "usage one=1", "makeUniqueDecal used once -> same=true"
    alpha.nose().setAppearance(appearanceWith(*img));
    EXPECT_EQ(d.countDecalUsage(*img), 1);
    EXPECT_EQ(d.makeUniqueDecal(img), img);
    EXPECT_EQ(decalList(d), "[decals/a.png, decals/b.png]");

    // "usage two (nose outside, body inside)=2 other=0"
    alpha.body().getInsideColorComponentHandler().setInsideAppearance(appearanceWith(*img));
    EXPECT_EQ(d.countDecalUsage(*img), 2);
    EXPECT_EQ(d.countDecalUsage(*other), 0);

    // Java: null comes back unless a component has a decal, and then a NullPointerException.
    EXPECT_THROW(static_cast<void>(d.makeUniqueDecal(nullptr)), BugError);
    // Java: a NullPointerException in the registry.
    EXPECT_THROW(static_cast<void>(d.getDecalImage(nullptr)), BugError);
}

// The copy makeUniqueDecal() registers is an image of the document like any other: it is
// counted and heard by its own name.
TEST(OpenRocketDocumentDecals, AUniqueCopyIsCountedAndHeardByItsOwnName)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    alpha.nose().setAppearance(appearanceWith(*img));
    alpha.body().setAppearance(appearanceWith(*img));

    const std::shared_ptr<DecalImage> unique = d.makeUniqueDecal(img);
    ASSERT_NE(unique, nullptr);
    ASSERT_NE(unique, img);
    alpha.body().setAppearance(appearanceWith(*unique));
    EXPECT_EQ(d.countDecalUsage(*img), 1);
    EXPECT_EQ(d.countDecalUsage(*unique), 1);

    RocketEventRecorder rocketEvents(alpha.rocket());
    unique->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "C[BodyTube,texture,nonfunctional]");
    img->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "C[NoseCone,texture,nonfunctional]");
}

/// The fixture of the decal tests that change and remove images: DocumentProbe3's section J,
/// an image used by the nose cone outside and by the body tube inside, and one nobody uses.
class OpenRocketDocumentDecalEvents : public ::testing::Test
{
protected:
    OpenRocketDocumentDecalEvents()
      : m_img(document().getDecalImage(entry("decals/a.png"))),
        m_other(document().getDecalImage(entry("decals/b.png")))
    {
        m_alpha.nose().setAppearance(appearanceWith(*m_img));
        m_alpha.body().getInsideColorComponentHandler().setInsideAppearance(appearanceWith(*m_img));
        document().setSaved(true);
    }

    [[nodiscard]] const AlphaDocument& alpha() const noexcept { return m_alpha; }
    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return m_alpha.document(); }
    [[nodiscard]] const std::shared_ptr<DecalImage>& img() const noexcept { return m_img; }
    [[nodiscard]] const std::shared_ptr<DecalImage>& other() const noexcept { return m_other; }

private:
    AlphaDocument               m_alpha;
    std::shared_ptr<DecalImage> m_img;
    std::shared_ptr<DecalImage> m_other;
};

// DocumentProbe3, section J: "an unused image fires a change", "the used image fires a change".
TEST_F(OpenRocketDocumentDecalEvents, AChangedImageFiresATextureChangeOnTheComponentsThatUseIt)
{
    OpenRocketDocument& d = document();
    DocumentRecorder    events(d, true);
    RocketEventRecorder rocketEvents(alpha().rocket());

    other()->fireChangeEvent();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(rocketEvents.take(), "");
    EXPECT_TRUE(d.isSaved());

    img()->fireChangeEvent();
    EXPECT_EQ(events.take(), "U D(NoseCone,saved) D(NoseCone) U D(BodyTube) D(BodyTube)");
    EXPECT_EQ(rocketEvents.take(),
              "C[NoseCone,texture,nonfunctional] C[BodyTube,texture,nonfunctional]");
    EXPECT_FALSE(d.isSaved());
    EXPECT_TRUE(d.isUndoAvailable());
}

// DocumentProbe2, section C: "decal image fires a change | events: ... U D(NoseCone)
// D(NoseCone) ... U D(BodyTube) D(BodyTube) ... U D(BodyTube) D(BodyTube)": a component that
// uses the image outside and inside fires twice.
TEST_F(OpenRocketDocumentDecalEvents, AComponentThatUsesTheImageTwiceFiresTwice)
{
    alpha().body().setAppearance(appearanceWith(*img()));
    DocumentRecorder events(document());

    img()->fireChangeEvent();
    EXPECT_EQ(events.take(),
              "U D(NoseCone) D(NoseCone) U D(BodyTube) D(BodyTube) "
              "U D(BodyTube) D(BodyTube)");
}

// Deviation: in Java the components listen to the image, and the components an undo makes do
// not; here the document asks who uses the image when it changes.
TEST_F(OpenRocketDocumentDecalEvents, TheComponentsAnUndoMadeFireToo)
{
    OpenRocketDocument& d = document();
    d.clearUndo();
    d.addUndoPosition("Rename");
    alpha().nose().setName("renamed");
    d.undo();
    RocketEventRecorder rocketEvents(alpha().rocket());

    img()->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(),
              "C[NoseCone,texture,nonfunctional] C[BodyTube,texture,nonfunctional]");
}

// DocumentProbe3, section J: "removeDecal(unused image) -> true", "removeDecal(used image) ->
// true", "removeDecal(null)=false", "the removed image fires a change".
TEST_F(OpenRocketDocumentDecalEvents, RemovingAnImageTakesItOffTheComponents)
{
    OpenRocketDocument& d = document();
    DocumentRecorder    events(d, true);
    RocketEventRecorder rocketEvents(alpha().rocket());

    // An image nobody uses: one event, and the document stays saved.
    EXPECT_TRUE(d.removeDecal(other().get()));
    EXPECT_EQ(events.take(), "D(OpenRocketDocument,saved)");
    EXPECT_EQ(events.lastEvent().value_or(DocumentChangeEvent{}).getDocument(), &d);
    EXPECT_TRUE(d.isSaved());
    EXPECT_EQ(decalList(d), "[decals/a.png]");
    // Out of the list, the image is still what its name means (Java: a Decal that an undo
    // brought back would still hold the object).
    EXPECT_EQ(d.findDecalImage("decals/b.png"), other());

    // The image in use: every component that loses it changes, then the document's event.
    EXPECT_TRUE(d.removeDecal(img().get()));
    EXPECT_EQ(events.take(),
              "U D(NoseCone,saved) D(NoseCone) U D(BodyTube) D(BodyTube) "
              "D(OpenRocketDocument)");
    EXPECT_EQ(rocketEvents.take(), "C[NoseCone,nonfunctional] C[BodyTube,nonfunctional]");
    EXPECT_FALSE(d.isSaved());
    // "list=[] nose texture=null body inside texture=null nose paint=ORColor [r=187, g=187,
    // b=187, a=255] shine=0.3": the appearances stay, without the image.
    EXPECT_EQ(decalList(d), "[]");
    const std::optional<Appearance> expected = Appearance(Color{187, 187, 187}, 0.3);
    EXPECT_EQ(alpha().nose().getAppearance(), expected);
    EXPECT_EQ(alpha().body().getInsideColorComponentHandler().getInsideAppearance(), expected);
    EXPECT_EQ(d.countDecalUsage(*img()), 0);

    // "removeDecal again -> false" (DocumentProbe2) and "removeDecal(null)=false"
    EXPECT_FALSE(d.removeDecal(img().get()));
    EXPECT_FALSE(d.removeDecal(nullptr));
    EXPECT_EQ(events.take(), "");

    // "the removed image fires a change | events: U D(NoseCone) D(NoseCone) ...". Deviation:
    // Java's components go on listening to an image they no longer use; here nothing fires.
    img()->fireChangeEvent();
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(rocketEvents.take(), "");
}

// Deviation: a decal holds the name of its image, so an image of another document that has the
// name of one of this document's counts as that image. Java compares the objects: it counts no
// use of the other document's image and, told to remove it, takes this document's image of that
// name out of the registry while the components go on using it.
TEST_F(OpenRocketDocumentDecalEvents, TheNameOfAnImageDecidesNotTheObject)
{
    const AlphaDocument               elsewhere;
    const std::shared_ptr<DecalImage> foreign =
        elsewhere.document().getDecalImage(entry("decals/a.png"));
    ASSERT_NE(foreign, img());
    OpenRocketDocument& d = document();

    EXPECT_EQ(d.countDecalUsage(*foreign), 2);
    EXPECT_TRUE(d.removeDecal(foreign.get()));
    EXPECT_EQ(decalList(d), "[decals/b.png]");
    EXPECT_EQ(d.countDecalUsage(*img()), 0);
    // The other document keeps its image.
    EXPECT_EQ(decalList(elsewhere.document()), "[decals/a.png]");
}

// A removed image that is registered again is heard again. Deviation: in Java an attachment of
// the name of a removed image gives a new image object under that name; here the name still
// means the removed image, which comes back (see DecalRegistry).
TEST_F(OpenRocketDocumentDecalEvents, AnImageRegisteredAgainIsHeardAgain)
{
    OpenRocketDocument& d = document();
    ASSERT_TRUE(d.removeDecal(img().get()));
    alpha().nose().setAppearance(appearanceWith(*img()));
    RocketEventRecorder rocketEvents(alpha().rocket());

    // Removed, it is silent although a component uses it.
    img()->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "");

    const std::shared_ptr<DecalImage> again = d.getDecalImage(entry("decals/a.png"));
    ASSERT_EQ(again, img());
    EXPECT_EQ(decalList(d), "[decals/a.png, decals/b.png]");
    again->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "C[NoseCone,texture,nonfunctional]");
    // Once per change, however often the image was registered.
    static_cast<void>(d.getDecalImage(entry("decals/a.png")));
    again->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "C[NoseCone,texture,nonfunctional]");
}

// ---- a removal and its undo -------------------------------------------------------------------
//
// Swing's "delete decal" calls removeDecal() without an undo position of its own, so the
// cleared appearances are an ordinary undo step and Ctrl+Z after it brings the decals back. In
// Java the Decal holds the image object, so the image and its bytes come back with it; here a
// Decal holds the name, and the registry keeps a removed image under its name.

/// The decal of the nose cone as the probes DecalUndo and DecalUndoFile print it: "<image name>
/// bytes='<the image's bytes>'", "none" without a decal, and "<image name> without an image"
/// when the document knows no image of the name.
[[nodiscard]] std::string noseTexture(const AlphaDocument& alpha)
{
    const std::optional<Appearance>& appearance = alpha.nose().getAppearance();
    if (!appearance.has_value())
    {
        return "none";
    }
    const std::optional<Decal>& texture = appearance->getTexture();
    if (!texture.has_value())
    {
        return "none";
    }
    const std::string                 name  = texture->getImageName();
    const std::shared_ptr<DecalImage> image = alpha.document().findDecalImage(name);
    if (image == nullptr)
    {
        return name + " without an image";
    }
    const auto bytes = image->getBytes();
    return std::format("{} bytes='{}'", name,
                       bytes.has_value() ? QtRocket::bytesToString(*bytes) : "unreadable");
}

// DecalUndo.
TEST(OpenRocketDocumentDecals, AnUndoOfARemovalBringsTheImageBackWithItsBytes)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d = alpha.document();
    const std::shared_ptr<DecalImage> image =
        d.getDecalImage(std::make_shared<MemoryAttachment>("decals/a.png", "first"));
    alpha.nose().setAppearance(appearanceWith(*image));
    d.clearUndo();
    // "start: nose texture=decals/a.png bytes='first' list=[decals/a.png] usage=1"
    EXPECT_EQ(noseTexture(alpha), "decals/a.png bytes='first'");
    EXPECT_EQ(decalList(d), "[decals/a.png]");
    EXPECT_EQ(d.countDecalUsage(*image), 1);

    // "removeDecal=true", "removed: nose texture=none list=[] usage=0"
    d.addUndoPosition("Remove decal");
    EXPECT_TRUE(d.removeDecal(image.get()));
    EXPECT_EQ(noseTexture(alpha), "none");
    EXPECT_EQ(decalList(d), "[]");
    EXPECT_EQ(d.countDecalUsage(*image), 0);

    // "undone: nose texture=decals/a.png bytes='first' list=[] usage=1"
    d.undo();
    EXPECT_EQ(noseTexture(alpha), "decals/a.png bytes='first'");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), image);
    EXPECT_EQ(decalList(d), "[]");
    EXPECT_EQ(d.countDecalUsage(*image), 1);

    // Deviation. Java: "another attachment of that name: same image=false nose
    // texture=decals/a.png bytes='first' list=[decals/a.png] usage(old)=1 usage(new)=0", two
    // image objects of one name. Here the name means the image the nose cone uses.
    const std::shared_ptr<DecalImage> second =
        d.getDecalImage(std::make_shared<MemoryAttachment>("decals/a.png", "second"));
    EXPECT_EQ(second, image);
    EXPECT_EQ(noseTexture(alpha), "decals/a.png bytes='first'");
    EXPECT_EQ(decalList(d), "[decals/a.png]");
    EXPECT_EQ(d.countDecalUsage(*second), 1);

    // "redone: nose texture=none list=[decals/a.png]"
    d.redo();
    EXPECT_EQ(noseTexture(alpha), "none");
    EXPECT_EQ(decalList(d), "[decals/a.png]");
}

/// The fixture of DecalUndoFile: the files x/a.png and y/a.png, and a document whose nose cone
/// has the image of x/a.png, which was removed and whose removal was undone.
class OpenRocketDocumentDecalUndo : public ::testing::Test
{
protected:
    OpenRocketDocumentDecalUndo()
      : m_x(m_dir.write("x/a.png", "file-x")),
        m_y(m_dir.write("y/a.png", "file-y")),
        m_image(document().getDecalImage(std::make_shared<FileSystemAttachment>("n", m_x)))
    {
        m_alpha.nose().setAppearance(appearanceWith(*m_image));
        document().clearUndo();
        document().addUndoPosition("Remove decal");
        EXPECT_TRUE(document().removeDecal(m_image.get()));
        document().undo();
    }

    [[nodiscard]] const AlphaDocument& alpha() const noexcept { return m_alpha; }
    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return m_alpha.document(); }
    [[nodiscard]] const std::shared_ptr<DecalImage>& image() const noexcept { return m_image; }

    /// The image the document gives for the file x/a.png or y/a.png.
    [[nodiscard]] std::shared_ptr<DecalImage> imageOfX() const
    {
        return document().getDecalImage(std::make_shared<FileSystemAttachment>("n", m_x));
    }
    [[nodiscard]] std::shared_ptr<DecalImage> imageOfY() const
    {
        return document().getDecalImage(std::make_shared<FileSystemAttachment>("n", m_y));
    }

private:
    TempDir                     m_dir;
    std::filesystem::path       m_x;
    std::filesystem::path       m_y;
    AlphaDocument               m_alpha;
    std::shared_ptr<DecalImage> m_image;
};

// DecalUndoFile.
TEST_F(OpenRocketDocumentDecalUndo, AnotherFileOfTheSameNameIsAnotherImageAndTheSameFileTheSame)
{
    OpenRocketDocument& d = document();
    // "undone: nose texture=decals/a.png bytes='file-x' list=[] usage=1"
    EXPECT_EQ(noseTexture(alpha()), "decals/a.png bytes='file-x'");
    EXPECT_EQ(decalList(d), "[]");
    EXPECT_EQ(d.countDecalUsage(*image()), 1);

    // Deviation. Java: "another file of that file name: name=decals/a.png same image=false
    // bytes='file-y' nose texture=decals/a.png bytes='file-x' list=[decals/a.png] usage(old)=1
    // usage(new)=0": two images of one name, told apart by the objects only (a save would write
    // one of them). Here the other file's image gets a name of its own.
    const std::shared_ptr<DecalImage> other = imageOfY();
    ASSERT_NE(other, nullptr);
    EXPECT_NE(other, image());
    EXPECT_EQ(other->getName(), "decals/a (1).png");
    EXPECT_EQ(QtRocket::bytesToString(other->getBytes().value_or(std::vector<std::byte>{})),
              "file-y");
    EXPECT_EQ(noseTexture(alpha()), "decals/a.png bytes='file-x'");
    EXPECT_EQ(decalList(d), "[decals/a (1).png]");
    EXPECT_EQ(d.countDecalUsage(*image()), 1);
    EXPECT_EQ(d.countDecalUsage(*other), 0);

    // Deviation. Java: "the same file again: name=decals/a (1).png same image=false same as
    // other=false bytes='file-x' list=[decals/a (1).png, decals/a.png]", a third image. Here
    // the file leads back to the image the nose cone uses.
    EXPECT_EQ(imageOfX(), image());
    EXPECT_EQ(decalList(d), "[decals/a (1).png, decals/a.png]");
}

TEST_F(OpenRocketDocumentDecalUndo, TheRemovedImageIsHeardOnceItIsRegisteredAgain)
{
    RocketEventRecorder rocketEvents(alpha().rocket());
    // Not registered: nothing fires (in Java neither, for the components an undo made).
    image()->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "");

    EXPECT_EQ(imageOfX(), image());
    image()->fireChangeEvent();
    EXPECT_EQ(rocketEvents.take(), "C[NoseCone,texture,nonfunctional]");
}

// DecalUndoFile: "makeUniqueDecal of a removed image two components use again: usage=2
// name=decals/a.png same image=true list=[]". Deviation: Java's registry has forgotten the name,
// finds nothing to rename and hands the image itself back, so the two components go on sharing
// it; here the name is still the image's and the copy is another image.
TEST_F(OpenRocketDocumentDecalUndo, AUniqueCopyOfTheRemovedImageGetsANewName)
{
    OpenRocketDocument& d = document();
    alpha().body().setAppearance(appearanceWith(*image()));
    EXPECT_EQ(d.countDecalUsage(*image()), 2);

    const std::shared_ptr<DecalImage> unique = d.makeUniqueDecal(image());
    ASSERT_NE(unique, nullptr);
    EXPECT_NE(unique, image());
    EXPECT_EQ(unique->getName(), "decals/a (1).png");
    EXPECT_EQ(decalList(d), "[decals/a (1).png]");
    EXPECT_EQ(QtRocket::bytesToString(unique->getBytes().value_or(std::vector<std::byte>{})),
              "file-x");
}

// An image may outlive the document that registered it; it then has nobody to tell.
TEST(OpenRocketDocumentDecals, AnImageMayOutliveItsDocument)
{
    std::shared_ptr<DecalImage> img;
    {
        const AlphaDocument alpha;
        img = alpha.document().getDecalImage(entry("decals/a.png"));
        alpha.nose().setAppearance(appearanceWith(*img));
    }
    img->fireChangeEvent();
    EXPECT_EQ(img->getName(), "decals/a.png");
}

// The two cases of a refused undo() (see OpenRocketDocumentReentrantUndo above) in which the
// document itself walks the components while their events go out.

TEST(OpenRocketDocumentReentrantUndo, IsRefusedWhileAChangedDecalImageTellsTheComponents)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    d.addUndoPosition("decal");
    alpha.nose().setAppearance(appearanceWith(*img));
    alpha.body().setAppearance(appearanceWith(*img));
    d.addUndoPosition("more");
    alpha.body().setLength(0.3);
    const UndoInSlot    undoing(d);
    RocketEventRecorder rocketEvents(alpha.rocket());

    img->fireChangeEvent();

    ASSERT_TRUE(undoing.done());
    EXPECT_TRUE(undoing.refusal().contains(kUndoRefused)) << undoing.refusal();
    // Every component that uses the image was told, the one after the refused undo too.
    EXPECT_EQ(rocketEvents.take(),
              "C[NoseCone,texture,nonfunctional] C[BodyTube,texture,nonfunctional]");
    EXPECT_EQ(alpha.body().getLength(), 0.3);
}

TEST(OpenRocketDocumentReentrantUndo, IsRefusedWhileRemoveDecalClearsTheComponents)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    d.addUndoPosition("decal");
    alpha.nose().setAppearance(appearanceWith(*img));
    alpha.body().setAppearance(appearanceWith(*img));
    d.addUndoPosition("remove");
    const UndoInSlot undoing(d);

    EXPECT_TRUE(d.removeDecal(img.get()));

    ASSERT_TRUE(undoing.done());
    EXPECT_TRUE(undoing.refusal().contains(kUndoRefused)) << undoing.refusal();
    // The removal went on to the component after the one whose event the slot heard.
    EXPECT_EQ(d.countDecalUsage(*img), 0);
    EXPECT_EQ(decalList(d), "[]");
    // And the removal is one undo step.
    d.undo();
    EXPECT_EQ(d.countDecalUsage(*img), 2);
}

/// Takes the component of the first event a document passes on out of the tree, as a slot must
/// not while the document walks the components, and keeps it: the event that names it is
/// still being delivered.
class TakeTheComponentOfTheEvent
{
public:
    explicit TakeTheComponentOfTheEvent(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            QtRocket::RocketComponent* const component = event.getComponent();
            if (!m_done && component != nullptr && component->getParent() != nullptr)
            {
                // Before the removal, whose own event comes here too.
                m_done  = true;
                m_taken = component->getParent()->removeChild(component);
            }
        }))
    {
    }

    [[nodiscard]] bool done() const noexcept { return m_taken != nullptr; }

private:
    bool                                                           m_done{false};
    std::unique_ptr<QtRocket::RocketComponent>                     m_taken;
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// Java: a ConcurrentModificationException from the iterator of the walk. Here the walk ends
// with a BugError before it touches a component again.
TEST(OpenRocketDocumentSlots, ASlotThatChangesTheTreeEndsTheWalkOfRemoveDecal)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    alpha.nose().setAppearance(appearanceWith(*img));
    alpha.body().setAppearance(appearanceWith(*img));
    const TakeTheComponentOfTheEvent removing(d);

    EXPECT_THROW(static_cast<void>(d.removeDecal(img.get())), BugError);

    ASSERT_TRUE(removing.done());
    // The nose cone is out of the tree; the body tube, which the walk did not reach, has its
    // decal still.
    EXPECT_EQ(alpha.stage().getChildCount(), 1U);
    EXPECT_EQ(d.countDecalUsage(*img), 1);
    // The document is whole: an undo is not refused afterwards.
    d.undo();
    EXPECT_EQ(alpha.stage().getChildCount(), 2U);
}

TEST(OpenRocketDocumentSlots, ASlotThatChangesTheTreeEndsTheWalkOfAChangedImage)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               d   = alpha.document();
    const std::shared_ptr<DecalImage> img = d.getDecalImage(entry("decals/a.png"));
    alpha.nose().setAppearance(appearanceWith(*img));
    alpha.body().setAppearance(appearanceWith(*img));
    const TakeTheComponentOfTheEvent removing(d);
    RocketEventRecorder              rocketEvents(alpha.rocket());

    EXPECT_THROW(img->fireChangeEvent(), BugError);

    ASSERT_TRUE(removing.done());
    EXPECT_EQ(alpha.stage().getChildCount(), 1U);
    // The nose cone's texture change, the removal the slot made, and no event of the body tube.
    EXPECT_FALSE(rocketEvents.take().contains("BodyTube,texture"));
}

}  // namespace
