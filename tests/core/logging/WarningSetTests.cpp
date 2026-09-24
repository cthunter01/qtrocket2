#include "QtRocket/logging/WarningSet.h"

#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"

namespace
{

using QtRocket::MessageSources;
using QtRocket::Warning;
using QtRocket::WarningSet;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The LargeAOA warning of @p set (every LargeAOA is equal to every other), or nullptr.
const Warning::LargeAOA* findLargeAoa(const WarningSet& set)
{
    return dynamic_cast<const Warning::LargeAOA*>(set.find(Warning::LargeAOA{0.0}));
}

TEST(WarningSet, LargeAOAKeepsTheLargestAngle)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::LargeAOA{0.1}));
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.3}));  // replaced in place, not added
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.2}));  // smaller: ignored
    ASSERT_EQ(warnings.size(), 1U);
    const Warning::LargeAOA* stored = findLargeAoa(warnings);
    ASSERT_NE(stored, nullptr);
    EXPECT_DOUBLE_EQ(stored->aoa(), 0.3);
    EXPECT_EQ(warnings.toString(), "Messages[Large angle of attack encountered (17.2°)]");
}

TEST(WarningSet, UnknownLargeAOAGivesWayToAnyAngle)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::LargeAOA{kNaN}));
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.05}));
    const Warning::LargeAOA* stored = findLargeAoa(warnings);
    ASSERT_NE(stored, nullptr);
    EXPECT_DOUBLE_EQ(stored->aoa(), 0.05);
    // ... and a NaN never replaces a known angle.
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{kNaN}));
    const Warning::LargeAOA* still = findLargeAoa(warnings);
    ASSERT_NE(still, nullptr);
    EXPECT_DOUBLE_EQ(still->aoa(), 0.05);
}

TEST(WarningSet, SpeedWarningsKeepTheFirst)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::RecoveryHighSpeedDeployment{50.0}));
    EXPECT_FALSE(warnings.add(Warning::RecoveryHighSpeedDeployment{80.0}));
    EXPECT_TRUE(warnings.add(Warning::HighSpeedMainDeployment{50.0}));  // another kind
    ASSERT_EQ(warnings.size(), 2U);
    const auto* stored =
        dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(&*warnings.begin());
    ASSERT_NE(stored, nullptr);
    EXPECT_DOUBLE_EQ(stored->speed(), 50.0);
}

TEST(WarningSet, EventAfterLandingAccumulates)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::EventAfterLanding{"Apogee"}));
    EXPECT_TRUE(warnings.add(Warning::EventAfterLanding{"Apogee"}));
    const Warning::EventAfterLanding same{"Ejection charge"};
    EXPECT_TRUE(warnings.add(same));
    EXPECT_FALSE(warnings.add(same));  // the very same warning (same id)
    EXPECT_EQ(warnings.size(), 3U);
    EXPECT_EQ(warnings.countCritical(), 3U);
}

TEST(WarningSet, GroupsByPriority)
{
    WarningSet warnings;
    warnings.add(Warning::kThickFin);                    // LOW
    warnings.add(Warning::kZeroAreaFin);                 // LOW
    warnings.add(Warning::kSupersonic);                  // NORMAL
    warnings.add(Warning::kRecoveryLaunchRod);           // HIGH
    warnings.add(Warning::RecoveryDrogueWithoutMain{});  // HIGH
    EXPECT_EQ(warnings.countInformational(), 2U);
    EXPECT_EQ(warnings.countNormal(), 1U);
    EXPECT_EQ(warnings.countCritical(), 2U);
    const std::vector<const Warning*> critical = warnings.criticalWarnings();
    ASSERT_EQ(critical.size(), 2U);
    EXPECT_EQ(critical[0]->messageDescription(),
              "Recovery device deployed while on the launch guide.");
    EXPECT_EQ(critical[1]->typeName(), "RecoveryDrogueWithoutMain");
    const std::vector<const Warning*> normal = warnings.normalWarnings();
    ASSERT_EQ(normal.size(), 1U);
    EXPECT_EQ(normal[0]->messageDescription(),
              "Body calculations may not be entirely accurate at supersonic speeds.");
    EXPECT_EQ(warnings.informationalWarnings().size(), 2U);
}

TEST(WarningSet, EmptySetHasNoWarningsOfAnyPriority)
{
    const WarningSet warnings;
    EXPECT_EQ(warnings.countCritical(), 0U);
    EXPECT_EQ(warnings.countNormal(), 0U);
    EXPECT_EQ(warnings.countInformational(), 0U);
    EXPECT_TRUE(warnings.criticalWarnings().empty());
    EXPECT_TRUE(warnings.normalWarnings().empty());
    EXPECT_TRUE(warnings.informationalWarnings().empty());
    EXPECT_EQ(warnings.toString(), "Messages[]");
}

TEST(WarningSet, AddByTextUsesTheDefaultPriority)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add("Something odd"));
    EXPECT_FALSE(warnings.add("Something odd"));
    EXPECT_FALSE(warnings.add(Warning::fromString("Something odd")));
    // The text of a LOW constant makes another kind of warning: the priority differs (as in
    // Java, where the .ork loader therefore sets the priority after fromString()).
    EXPECT_TRUE(warnings.add(Warning::kThickFin.messageDescription()));
    EXPECT_TRUE(warnings.add(Warning::kThickFin));
    EXPECT_EQ(warnings.size(), 3U);
    EXPECT_EQ(warnings.countNormal(), 2U);
    EXPECT_EQ(warnings.countInformational(), 1U);
}

TEST(WarningSet, SameTextWithDifferentSourcesAreDistinct)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{{"fs-1", "Fin set 1"}}));
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{{"fs-2", "Fin set 2"}}));
    EXPECT_FALSE(warnings.add(Warning::kThickFin, MessageSources{{"fs-1", "Fin set 1"}}));
    EXPECT_TRUE(warnings.add(Warning::kThickFin,
                             MessageSources{{"fs-1", "Fin set 1"}, {"fs-2", "Fin set 2"}}));
    EXPECT_EQ(warnings.size(), 3U);
    EXPECT_EQ(warnings.toString(),
              "Messages[Thick fins may not simulate accurately:  \"Fin set 1\","
              "Thick fins may not simulate accurately:  \"Fin set 2\","
              "Thick fins may not simulate accurately:  \"Fin set 1\", \"Fin set 2\"]");
}

TEST(WarningSet, FilterOutRemovesTheWholeType)
{
    WarningSet warnings;
    warnings.add(Warning::kOpenAirframeForward, MessageSources{{"nc-1", "Nose cone"}});
    warnings.add(Warning::kOpenAirframeForward, MessageSources{{"tr-1", "Transition"}});
    warnings.add(Warning::LargeAOA{0.2});
    ASSERT_EQ(warnings.size(), 3U);
    warnings.filterOut(Warning::kOpenAirframeForward);
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->typeName(), "LargeAOA");
    warnings.filterOut(Warning::LargeAOA{kNaN});
    EXPECT_TRUE(warnings.empty());
}

TEST(WarningSet, CopyKeepsWarningsAndHelpers)
{
    WarningSet warnings;
    warnings.add(Warning::kEmptyBranch);
    const WarningSet copy = warnings;
    EXPECT_EQ(copy.countCritical(), 1U);
    EXPECT_TRUE(copy == warnings);
    warnings.clear();
    EXPECT_EQ(copy.size(), 1U);
    EXPECT_FALSE(copy == warnings);
}

}  // namespace
