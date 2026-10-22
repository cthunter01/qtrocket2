#include "QtRocket/gui/figure/FinPointSelection.h"

#include <array>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "FinPointTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Gui
{

namespace
{

using FinPointSelectionTest = Test::GuiTest;
using Test::finPointsText;
using Test::FreeformDesign;
using Test::RocketEvents;

/// Four points (the default outline) and five (TestRockets.makeBigBlue()'s).
[[nodiscard]] FreeformDesign::Outline four()
{
    return {};
}

[[nodiscard]] FreeformDesign::Outline five()
{
    return {0, 0, 0.115, 0.072, 0.255, 0.072, 0.255, 0.037, 0.150, 0};
}

TEST_F(FinPointSelectionTest, FindsItsFinSetsByIdAtEveryUse)
{
    const FreeformDesign    design({four(), five()});
    const FinPointSelection selection = design.selection(0, {1});
    EXPECT_EQ(selection.main(), &design.fins(0));
    ASSERT_EQ(selection.others().size(), 1U);
    EXPECT_EQ(selection.others().front(), &design.fins(1));
    EXPECT_TRUE(selection.isMultiEdit());

    // An undo replaces every component: the selection finds the new ones.
    design.document().addUndoPosition("Move");
    ASSERT_TRUE(selection.setPoint(1, 0.03, 0.06).has_value());
    design.document().undo();
    EXPECT_NE(selection.main(), nullptr);
    EXPECT_EQ(selection.main(), &design.fins(0));
    EXPECT_EQ(finPointsText(*selection.main()),
              "(0.0,0.0) (0.025,0.05) (0.075,0.05) (0.04999999999999999,0.0)");
}

TEST_F(FinPointSelectionTest, OfASelectionTakesItsFreeformFinSets)
{
    const FreeformDesign design({four(), five()});
    auto& trapezoid = design.fins(0).getParent()->addChild(std::make_unique<TrapezoidFinSet>());
    const std::array<RocketComponent*, 2> others{&trapezoid, &design.fins(1)};
    const ComponentSelection              selection(design.fins(0), others);
    const FinPointSelection               fins = FinPointSelection::of(selection);
    EXPECT_EQ(fins.mainId(), design.id(0));
    ASSERT_EQ(fins.otherIds().size(), 1U);
    EXPECT_EQ(fins.otherIds().front(), design.id(1));

    const ComponentSelection ofTrapezoid(trapezoid);
    EXPECT_THROW(static_cast<void>(FinPointSelection::of(ofTrapezoid)), BugError);
}

TEST_F(FinPointSelectionTest, ASingleFinSetIsEditedWithItsOwnEvents)
{
    const FreeformDesign    design;
    const FinPointSelection selection = design.selection();
    const RocketEvents      events(design.rocket());
    ASSERT_TRUE(selection.setPoint(1, 0.03, 0.06).has_value());
    ASSERT_TRUE(selection.addPoint(2, {0.05, 0.06}).has_value());
    ASSERT_TRUE(selection.removePoint(1).has_value());
    EXPECT_EQ(finPointsText(design.fins()),
              "(0.0,0.0) (0.05,0.06) (0.075,0.05) (0.04999999999999999,0.0)");
    // AEROMASS, NONFUNCTIONAL, AEROMASS: no freeze.
    EXPECT_EQ(events.types(), (std::vector<int>{ComponentChangeEvent::kAeromassChange,
                                                ComponentChangeEvent::kNonFunctionalChange,
                                                ComponentChangeEvent::kAeromassChange}));
}

TEST_F(FinPointSelectionTest, AnEditReachesTheOthersAtTheirConfigListenerPoint)
{
    // FreeformFinSet::getConfigListenerPointIdx(): the same index, the last for the last.
    const FreeformDesign    design({four(), five()});
    const FinPointSelection selection = design.selection(0, {1});
    ASSERT_TRUE(selection.setPoint(1, 0.03, 0.06).has_value());
    ASSERT_TRUE(selection.setPoint(3, 0.06, 0).has_value());
    EXPECT_EQ(finPointsText(design.fins(0)), "(0.0,0.0) (0.03,0.06) (0.075,0.05) (0.06,0.0)");
    EXPECT_EQ(finPointsText(design.fins(1)),
              "(0.0,0.0) (0.03,0.06) (0.255,0.072) (0.255,0.037) (0.06,0.0)");
}

TEST_F(FinPointSelectionTest, ThePointThatIsTheOthersLastOnlyLeavesItAlone)
{
    const FreeformDesign    design({five(), four()});
    const FinPointSelection selection = design.selection(0, {1});
    const std::string       other     = finPointsText(design.fins(1));
    ASSERT_TRUE(selection.setPoint(3, 0.25, 0.04).has_value());
    EXPECT_EQ(finPointsText(design.fins(1)), other);
    EXPECT_EQ(design.fins(0).getFinPoints().at(3).y, 0.04);
}

TEST_F(FinPointSelectionTest, SeveralFinSetsAreEditedInOneFreeze)
{
    const FreeformDesign    design({four(), five()});
    const FinPointSelection selection = design.selection(0, {1});
    const RocketEvents      events(design.rocket());
    ASSERT_TRUE(selection.setPoint(1, 0.03, 0.06).has_value());
    EXPECT_EQ(events.count(), 1U);
    ASSERT_TRUE(selection.addPoint(2, {0.05, 0.06}).has_value());
    EXPECT_EQ(events.count(), 2U);
    EXPECT_EQ(design.fins(0).getPointCount(), 5U);
    EXPECT_EQ(design.fins(1).getPointCount(), 6U);
    ASSERT_TRUE(selection.removePoint(1).has_value());
    EXPECT_EQ(events.count(), 3U);
    EXPECT_EQ(design.fins(0).getPointCount(), 4U);
    EXPECT_EQ(design.fins(1).getPointCount(), 5U);
    // A frozen rocket stays frozen (no second freeze).
    design.rocket().freeze();
    ASSERT_TRUE(selection.setPoint(1, 0.035, 0.065).has_value());
    EXPECT_TRUE(design.rocket().isFrozen());
    design.rocket().thaw();
    EXPECT_EQ(events.count(), 4U);
}

TEST_F(FinPointSelectionTest, TheMainFinSetFailsBeforeAnyoneChanges)
{
    // Java's checks come before its loop over the config listeners.
    const FreeformDesign    design({four(), five()});
    const FinPointSelection selection = design.selection(0, {1});
    const std::string       main      = finPointsText(design.fins(0));
    const std::string       other     = finPointsText(design.fins(1));
    const RocketEvents      events(design.rocket());

    const Result<void> beyond = selection.setPoint(4, 0.1, 0.1);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error().message, "index out of range");
    const Result<void> first = selection.addPoint(0, {0.01, 0.01});
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error().message,
              "Cannot add new point before the first or after the last point");
    EXPECT_FALSE(selection.addPoint(4, {0.01, 0.01}).has_value());
    const Result<void> removeFirst = selection.removePoint(0);
    ASSERT_FALSE(removeFirst.has_value());
    EXPECT_EQ(removeFirst.error().message, "cannot remove first or last point");
    EXPECT_FALSE(selection.removePoint(3).has_value());
    EXPECT_FALSE(selection.removePoint(7).has_value());

    EXPECT_EQ(finPointsText(design.fins(0)), main);
    EXPECT_EQ(finPointsText(design.fins(1)), other);
    EXPECT_EQ(events.count(), 0U);
}

TEST_F(FinPointSelectionTest, SetPointsGivesEveryFinSetThePoints)
{
    const FreeformDesign          design({four(), five()});
    const FinPointSelection       selection = design.selection(0, {1});
    const std::vector<Coordinate> points{Coordinate(0, 0), Coordinate(0.02, 0.03),
                                         Coordinate(0.04, 0)};
    const RocketEvents            events(design.rocket());
    selection.setPoints(points);
    EXPECT_EQ(finPointsText(design.fins(0)), "(0.0,0.0) (0.02,0.03) (0.03999999999999998,0.0)");
    EXPECT_EQ(finPointsText(design.fins(1)), "(0.0,0.0) (0.02,0.03) (0.03999999999999998,0.0)");
    EXPECT_EQ(events.count(), 1U);
    EXPECT_THROW(selection.setPoints({}), BugError);
}

TEST_F(FinPointSelectionTest, AFinSetThatLeftTheRocketIsEditedNoMore)
{
    const FreeformDesign    design({four(), five()});
    const FinPointSelection selection = design.selection(0, {1});
    RocketComponent&        body      = *design.fins(0).getParent();
    const std::string       other     = finPointsText(design.fins(1));
    const auto              removed   = body.removeChild(&design.fins(0));
    EXPECT_EQ(selection.main(), nullptr);
    EXPECT_TRUE(selection.setPoint(1, 0.03, 0.06).has_value());
    EXPECT_FALSE(selection.addPoint(1, {0.01, 0.01}).has_value());
    EXPECT_FALSE(selection.removePoint(1).has_value());
    selection.setPoints(std::vector<Coordinate>{Coordinate(0, 0), Coordinate(0.01, 0)});
    EXPECT_EQ(finPointsText(design.fins(1)), other);
}

}  // namespace

}  // namespace QtRocket::Gui
