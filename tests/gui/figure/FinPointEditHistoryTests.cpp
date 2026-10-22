#include "QtRocket/gui/figure/FinPointEditHistory.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FinPointTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Gui
{

namespace
{

using FinPointEditHistoryTest = Test::GuiTest;
using Test::finPointsText;
using Test::FreeformDesign;
using Test::pointsText;

/// The default outline as the fin set gives it (its last point a hair short of 5 cm).
constexpr const char* kDefaultPoints =
    "(0.0,0.0) (0.025,0.05) (0.075,0.05) (0.04999999999999999,0.0)";

/// Moves the point 1 of @p design's fin set to (@p x, 0.05) as one edit of @p history.
void moveSecondPoint(const FreeformDesign& design, FinPointEditHistory& history, double x)
{
    history.beginEdit();
    EXPECT_TRUE(design.fins().setPoint(1, x, 0.05).has_value());
    history.commitEdit();
}

TEST_F(FinPointEditHistoryTest, StartsWithTheCurrentPoints)
{
    const FreeformDesign      design;
    const FinPointEditHistory history(design.selection());
    EXPECT_EQ(history.maxSize(), OpenRocketDocument::kUndoLevels);
    EXPECT_EQ(history.size(), 1U);
    EXPECT_EQ(history.position(), 0U);
    EXPECT_FALSE(history.canUndo());
    EXPECT_FALSE(history.canRedo());
    EXPECT_FALSE(history.isEditInProgress());
    EXPECT_EQ(pointsText(history.snapshot(0)), kDefaultPoints);
}

TEST_F(FinPointEditHistoryTest, AnEditIsKeptWhenItChangedThePoints)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    history.beginEdit();
    EXPECT_TRUE(history.isEditInProgress());
    history.commitEdit();
    EXPECT_FALSE(history.isEditInProgress());
    EXPECT_EQ(history.size(), 1U);

    moveSecondPoint(design, history, 0.03);
    EXPECT_EQ(history.size(), 2U);
    EXPECT_EQ(history.position(), 1U);
    EXPECT_TRUE(history.canUndo());
    EXPECT_EQ(pointsText(history.snapshot(1)), finPointsText(design.fins()));
}

TEST_F(FinPointEditHistoryTest, NestedEditsAreOne)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    history.beginEdit();
    history.beginEdit();
    EXPECT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    history.commitEdit();
    EXPECT_TRUE(design.fins().setPoint(1, 0.035, 0.05).has_value());
    history.commitEdit();
    EXPECT_EQ(history.size(), 2U);
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.035);
    EXPECT_EQ(history.snapshot(1)[1].x, 0.03);
}

TEST_F(FinPointEditHistoryTest, UndoAndRedoSetTheSnapshots)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    moveSecondPoint(design, history, 0.03);
    moveSecondPoint(design, history, 0.035);
    history.undo();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.03);
    EXPECT_EQ(history.position(), 1U);
    EXPECT_TRUE(history.canRedo());
    history.undo();
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    EXPECT_FALSE(history.canUndo());
    history.undo();  // nothing to undo
    EXPECT_EQ(history.position(), 0U);
    history.redo();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.03);

    // A new edit discards the redo states.
    moveSecondPoint(design, history, 0.04);
    EXPECT_EQ(history.size(), 3U);
    EXPECT_FALSE(history.canRedo());
    history.redo();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.04);
}

TEST_F(FinPointEditHistoryTest, AnEditToTheCurrentSnapshotDiscardsTheRedoStatesOnly)
{
    // push(): the redo states go first, then a snapshot equal to the current one is not added
    // ("Don't add duplicates"). The points changed outside the history meanwhile.
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    moveSecondPoint(design, history, 0.03);
    moveSecondPoint(design, history, 0.035);
    history.undo();
    ASSERT_TRUE(history.canRedo());
    EXPECT_TRUE(design.fins().setPoint(1, 0.05, 0.05).has_value());
    moveSecondPoint(design, history, 0.03);
    EXPECT_EQ(history.size(), 2U);
    EXPECT_EQ(history.position(), 1U);
    EXPECT_FALSE(history.canRedo());
}

/// Moves the point 1 @p count times, each an edit of @p history.
void editTimes(const FreeformDesign& design, FinPointEditHistory& history, int count)
{
    for (int i = 1; i <= count; ++i)
    {
        moveSecondPoint(design, history, 0.025 + (0.0001 * i));
    }
}

/// Undoes as long as @p history can: how many times.
[[nodiscard]] int undoAll(FinPointEditHistory& history)
{
    int undone = 0;
    while (history.canUndo())
    {
        history.undo();
        ++undone;
    }
    return undone;
}

TEST_F(FinPointEditHistoryTest, KeepsFiftySnapshots)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    editTimes(design, history, 55);
    EXPECT_EQ(history.size(), 50U);
    EXPECT_EQ(history.position(), 49U);
    EXPECT_EQ(undoAll(history), 49);
    // The oldest left is the sixth edit's.
    EXPECT_DOUBLE_EQ(design.fins().getFinPoints().at(1).x, 0.025 + (0.0001 * 6));
}

TEST_F(FinPointEditHistoryTest, NothingIsUndoneDuringAnEdit)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    moveSecondPoint(design, history, 0.03);
    history.beginEdit();
    history.undo();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.03);
    history.commitEdit();
    history.undo();
    history.beginEdit();
    history.redo();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.025);
    history.commitEdit();
}

TEST_F(FinPointEditHistoryTest, UndoReachesEverySelectedFinSet)
{
    // Java's setPoints() forwards the snapshot to the config listeners.
    const FreeformDesign design(
        {FreeformDesign::Outline{}, FreeformDesign::Outline{0, 0, 0.02, 0.03, 0.04, 0}});
    FinPointEditHistory history(design.selection(0, {1}));
    moveSecondPoint(design, history, 0.03);
    history.undo();
    EXPECT_EQ(finPointsText(design.fins(0)), kDefaultPoints);
    EXPECT_EQ(finPointsText(design.fins(1)), kDefaultPoints);
}

TEST_F(FinPointEditHistoryTest, SnapshotsAreEqualWithinMathUtilsEpsilon)
{
    const std::vector<Coordinate> a{Coordinate(0, 0), Coordinate(0.025, 0.05)};
    const std::vector<Coordinate> nearly{Coordinate(0, 0), Coordinate(0.025 + 1e-12, 0.05)};
    const std::vector<Coordinate> moved{Coordinate(0, 0), Coordinate(0.026, 0.05)};
    const std::vector<Coordinate> weighted{Coordinate(0, 0), Coordinate(0.025, 0.05, 0, 1)};
    const std::vector<Coordinate> shorter{Coordinate(0, 0)};
    EXPECT_TRUE(FinPointEditHistory::snapshotsEqual(a, nearly));
    EXPECT_FALSE(FinPointEditHistory::snapshotsEqual(a, moved));
    EXPECT_FALSE(FinPointEditHistory::snapshotsEqual(a, weighted));
    EXPECT_FALSE(FinPointEditHistory::snapshotsEqual(a, shorter));
}

TEST_F(FinPointEditHistoryTest, AFinSetThatLeftTheRocketEndsTheHistory)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    moveSecondPoint(design, history, 0.03);
    history.beginEdit();
    RocketComponent& body    = *design.fins().getParent();
    const auto       removed = body.removeChild(&design.fins());
    history.commitEdit();
    EXPECT_EQ(history.size(), 2U);
    // The undo moves to the snapshot but sets it on no fin set.
    history.undo();
    EXPECT_EQ(history.position(), 0U);
    EXPECT_FALSE(history.isEditInProgress());
}

TEST_F(FinPointEditHistoryTest, ClearForgetsEverything)
{
    const FreeformDesign design;
    FinPointEditHistory  history(design.selection());
    moveSecondPoint(design, history, 0.03);
    history.beginEdit();
    history.clear();
    EXPECT_EQ(history.size(), 0U);
    EXPECT_FALSE(history.canUndo());
    EXPECT_FALSE(history.canRedo());
    EXPECT_FALSE(history.isEditInProgress());
    history.reset();
    EXPECT_EQ(history.size(), 1U);
    EXPECT_EQ(pointsText(history.snapshot(0)), finPointsText(design.fins()));
}

}  // namespace

}  // namespace QtRocket::Gui
