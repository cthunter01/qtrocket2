#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPresetReference.h"
#include "document/DocumentTestSupport.h"

// The reference to a preset that was not found (RocketComponent::getUnresolvedPreset(), the
// user's decision of 2026-10-08) in a document: it is kept through undo and redo, because the
// states of the undo history are copies of the rocket, and the document notices when it is
// dropped. QtRocket's own; OpenRocket has no such field (tests/core/rocket/
// unresolved_preset_tests.cpp has the component's side).
//
// A document is made as a load makes one: the components are given their references, and the
// undo history is cleared after that (OpenRocketLoader's last step), so that its one state has
// them.

namespace
{

using QtRocket::ComponentPresetReference;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::DocumentRecorder;

[[nodiscard]] ComponentPresetReference tubeReference()
{
    return {.type         = "BODY_TUBE",
            .manufacturer = "Estes",
            .partNo       = "BT-50, 30352",
            .digest       = "a59dec8e4034a2fee5955dbf4ff07f1c"};
}

[[nodiscard]] ComponentPresetReference chuteReference()
{
    return {.type         = "PARACHUTE",
            .manufacturer = "Estes",
            .partNo       = "PK-10, 2262",
            .digest       = "b595c8a31baaf325291d5c957f3940f8"};
}

/// An Alpha III document as a load leaves one whose file names two presets that were not found:
/// the body tube and the parachute have their references, the undo history has one state and
/// the document is saved.
class LoadedAlpha : public AlphaDocument
{
public:
    LoadedAlpha()
    {
        body().setUnresolvedPreset(tubeReference());
        chute().setUnresolvedPreset(chuteReference());
        document().clearUndo();
        document().setSaved(true);
    }
};

// What the loader does where OpenRocket's only warns leaves the document as it is: saved, and
// with nothing to undo.
TEST(OpenRocketDocumentUnresolvedPreset, RememberingAReferenceIsNoChangeOfTheDocument)
{
    AlphaDocument alpha;
    alpha.document().setSaved(true);
    DocumentRecorder recorder(alpha.document());

    alpha.body().setUnresolvedPreset(tubeReference());

    EXPECT_EQ(recorder.take(), "");
    EXPECT_TRUE(alpha.document().isSaved());
    EXPECT_FALSE(alpha.document().isUndoAvailable());
}

// A change of a value that a preset gives drops the reference; the undo of the change brings
// it back with the component, and the redo drops it again.
TEST(OpenRocketDocumentUnresolvedPreset, IsKeptThroughUndoAndRedo)
{
    const LoadedAlpha alpha;
    alpha.document().addUndoPosition("Change the radius");
    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);
    ASSERT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());

    alpha.document().undo();
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());

    alpha.document().redo();
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());

    alpha.document().undo();
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
}

// An undo and a redo replace every component of the rocket: a reference that no change
// touched is on the new components as it was on the old ones.
TEST(OpenRocketDocumentUnresolvedPreset, StaysOnTheComponentsAnUndoReplaces)
{
    const LoadedAlpha alpha;
    alpha.document().addUndoPosition("Rename the nose cone");
    alpha.nose().setName("Renamed");
    alpha.document().addUndoPosition("Comment on the body tube");
    alpha.body().setComment("A comment");

    alpha.document().undo();
    alpha.document().undo();
    EXPECT_EQ(alpha.nose().getName(), "Nose Cone");
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());

    alpha.document().redo();
    alpha.document().redo();
    EXPECT_EQ(alpha.body().getComment(), "A comment");
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());
}

// Forgetting the reference by itself (RocketComponent::clearPreset()) is a change of the
// document like forgetting a preset: a save would now write another design. It can be undone.
TEST(OpenRocketDocumentUnresolvedPreset, ForgettingItIsAChangeOfTheDocumentThatCanBeUndone)
{
    const LoadedAlpha alpha;
    ASSERT_TRUE(alpha.document().isSaved());
    DocumentRecorder recorder(alpha.document());
    alpha.document().addUndoPosition("Forget the preset");

    alpha.chute().clearPreset();

    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), std::nullopt);
    // The rocket's event reaches the document as every event of the rocket does.
    EXPECT_EQ(recorder.take(), "U D(Parachute) D(Parachute)");
    EXPECT_FALSE(alpha.document().isSaved());
    ASSERT_TRUE(alpha.document().isUndoAvailable());

    alpha.document().undo();
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
}

// The states of the undo history are their own: what happens to the rocket later does not
// reach a state that was taken before.
TEST(OpenRocketDocumentUnresolvedPreset, AStateOfTheHistoryKeepsWhatItHad)
{
    const LoadedAlpha alpha;
    alpha.document().addUndoPosition("First change");
    alpha.chute().setDiameter(alpha.chute().getDiameter() * 2);
    alpha.document().addUndoPosition("Second change");
    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);
    ASSERT_EQ(alpha.chute().getUnresolvedPreset(), std::nullopt);
    ASSERT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);

    // Back by one: the body tube has its reference again, the parachute's was dropped before.
    alpha.document().undo();
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), std::nullopt);

    // Back to the loaded state.
    alpha.document().undo();
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(alpha.chute().getUnresolvedPreset(), chuteReference());
}

}  // namespace
