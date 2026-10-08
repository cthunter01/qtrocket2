#include "QtRocket/file/openrocket/IntSetter.h"

#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations of the setter without a maximum are
// OpenRocket's (probe SetterProbe of tier 9b, part R1, group "int": its IntSetter on the fin
// count of a fin set, a parachute's line count and the instance counts); the maximum is
// QtRocket's own (decision L6).

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::DocumentConfig;
using QtRocket::IntSetter;
using QtRocket::LaunchLug;
using QtRocket::PodSet;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::TrapezoidFinSet;
using QtRocket::WarningSet;
using QtRocket::Test::joinedWarnings;
using QtRocket::Test::SetterFixture;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";
constexpr std::string_view kWarning = "Invalid parameter encountered, ignoring.";

TEST(IntSetter, SetsTheNumberOfTheText)
{
    SetterRecorder  recorder;
    const IntSetter setter(recorder.whole("count"));

    EXPECT_EQ(recorder.apply(setter, "4"), "count 4");
    EXPECT_EQ(recorder.apply(setter, "0"), "count 0");
    // Integer.parseInt: one sign, leading zeros, the whole range of an int.
    EXPECT_EQ(recorder.apply(setter, "+5"), "count 5");
    EXPECT_EQ(recorder.apply(setter, "-3"), "count -3");
    EXPECT_EQ(recorder.apply(setter, "007"), "count 7");
    EXPECT_EQ(recorder.apply(setter, "2147483647"), "count 2147483647");
    EXPECT_EQ(recorder.apply(setter, "-2147483648"), "count -2147483648");
}

TEST(IntSetter, WarnsOfATextThatIsNoInt)
{
    SetterRecorder  recorder;
    const IntSetter setter(recorder.whole("count"));

    // Nothing is trimmed.
    EXPECT_EQ(recorder.apply(setter, " 4"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "4 "), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "4\n"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, ""), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "abc"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "3.0"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "1e3"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "0x10"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "+"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "-"), kInvalid);
    // Beyond an int.
    EXPECT_EQ(recorder.apply(setter, "2147483648"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "-2147483649"), kInvalid);
}

// Deviation: Integer.parseInt reads the digits of every script, so OpenRocket gives a fin set
// of <fincount> U+0664 </fincount> (the Arabic-Indic digit four) four fins. Here only ASCII
// digits are digits.
TEST(IntSetter, ReadsAsciiDigitsOnly)
{
    SetterRecorder  recorder;
    const IntSetter setter(recorder.whole("count"));

    EXPECT_EQ(recorder.apply(setter, "\xD9\xA4"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "\xEF\xBC\x94"), kInvalid);  // U+FF14, the fullwidth four
}

// Decision L6: OpenRocket has no maximum and keeps <instancecount>2147483647</instancecount>.
TEST(IntSetter, WithAMaximumRefusesALargerNumber)
{
    SetterRecorder  recorder;
    const IntSetter setter(recorder.whole("count"), DocumentConfig::kMaxCount);

    EXPECT_EQ(DocumentConfig::kMaxCount, 10000);
    EXPECT_EQ(recorder.apply(setter, "3"), "count 3");
    EXPECT_EQ(recorder.apply(setter, "9999"), "count 9999");
    EXPECT_EQ(recorder.apply(setter, "10000"), "count 10000");
    EXPECT_EQ(recorder.apply(setter, "+10000"), "count 10000");
    EXPECT_EQ(recorder.apply(setter, "10001"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "20000"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "2147483647"), kInvalid);
    // The maximum is no minimum: what a count below one means is the component's business.
    EXPECT_EQ(recorder.apply(setter, "0"), "count 0");
    EXPECT_EQ(recorder.apply(setter, "-1"), "count -1");
    EXPECT_EQ(recorder.apply(setter, "-2147483648"), "count -2147483648");
    // A text that is no number is the same warning.
    EXPECT_EQ(recorder.apply(setter, "x"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "2147483648"), kInvalid);

    const IntSetter small(recorder.whole("count"), 2);
    EXPECT_EQ(recorder.apply(small, "2"), "count 2");
    EXPECT_EQ(recorder.apply(small, "3"), kInvalid);
}

/// A setter's warnings for @p text on @p component, joined; "FAILED" when it fails.
[[nodiscard]] std::string warned(SetterFixture& fixture, const IntSetter& setter,
                                 RocketComponent& component, std::string_view text)
{
    WarningSet         warnings;
    const Result<void> result = setter.set(component, text, {}, warnings, fixture.context());
    return result.has_value() ? joinedWarnings(warnings) : "FAILED";
}

/// A body tube in each of the 10000 pods of a pod set in the fixture's tube, for a component
/// of which every instance counts 10000 times.
[[nodiscard]] BodyTube& tubeInTenThousandPods(SetterFixture& fixture)
{
    PodSet& pods = fixture.tube().addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(DocumentConfig::kMaxCount);
    return pods.addChild(std::make_unique<BodyTube>());
}

/// What the table's setter of a launch lug's instance count calls.
void setLugCount(RocketComponent& component, int count)
{
    dynamic_cast<LaunchLug&>(component).setInstanceCount(count);
}

/// What the table's setter of a fin set's fin count calls.
void setFinCount(RocketComponent& component, int count)
{
    dynamic_cast<TrapezoidFinSet&>(component).setFinCount(count);
}

// The instance budget of a rocket (DocumentConfig::kMaxInstances), QtRocket's own: the counts
// of nested components multiply, so the maximum of a count by itself bounds nothing. OpenRocket
// has no bound and runs out of memory.
TEST(IntSetter, OfAnInstanceCountRefusesWhatTheRocketCannotHold)
{
    SetterFixture   fixture;
    LaunchLug&      lug = tubeInTenThousandPods(fixture).addChild(std::make_unique<LaunchLug>());
    const IntSetter setter(&setLugCount, IntSetter::InstanceCount{.kept = DocumentConfig::kMaxCount,
                                                                  .refuseAbove = true});
    // The rocket, the stage, the tube, 10000 pods, their tubes and a lug in each.
    ASSERT_EQ(DocumentConfig::instanceLoad(lug), 30003U);

    // 10000 lugs in each of the 10000 pods.
    EXPECT_EQ(warned(fixture, setter, lug, "10000"), kWarning);
    EXPECT_EQ(lug.getInstanceCount(), 1) << "nothing is set";
    // 8 in each are 80000: 100003 instances in all, three too many.
    EXPECT_EQ(warned(fixture, setter, lug, "8"), kWarning);
    EXPECT_EQ(lug.getInstanceCount(), 1);
    EXPECT_EQ(warned(fixture, setter, lug, "7"), "");
    EXPECT_EQ(lug.getInstanceCount(), 7);
    EXPECT_EQ(DocumentConfig::instanceLoad(lug), 90003U);
    // The count it has, and a smaller one, always fit; so does a count the lug does not take.
    EXPECT_EQ(warned(fixture, setter, lug, "7"), "");
    EXPECT_EQ(warned(fixture, setter, lug, "0"), "");
    EXPECT_EQ(lug.getInstanceCount(), 7);
    EXPECT_EQ(warned(fixture, setter, lug, "2"), "");
    EXPECT_EQ(lug.getInstanceCount(), 2);
    // The maximum of the count by itself stays (decision L6), and a text that is no number.
    EXPECT_EQ(warned(fixture, setter, lug, "10001"), kWarning);
    EXPECT_EQ(warned(fixture, setter, lug, "x"), kWarning);
    EXPECT_EQ(lug.getInstanceCount(), 2);
}

// A fin set keeps 8 fins of a larger number (FinSet::setFinCount()), as OpenRocket's does: the
// 8 are what the budget is asked about, and a number above them is no reason for a warning.
TEST(IntSetter, OfAFinCountAsksTheBudgetAboutTheFinsTheSetKeeps)
{
    SetterFixture    fixture;
    TrapezoidFinSet& fins =
        tubeInTenThousandPods(fixture).addChild(std::make_unique<TrapezoidFinSet>());
    const IntSetter setter(&setFinCount, IntSetter::InstanceCount{.kept = 8, .refuseAbove = false});
    // Three fins in each of the 10000 pods.
    ASSERT_EQ(DocumentConfig::instanceLoad(fins), 50003U);

    EXPECT_EQ(warned(fixture, setter, fins, "8"), kWarning);
    EXPECT_EQ(warned(fixture, setter, fins, "2147483647"), kWarning);
    EXPECT_EQ(fins.getFinCount(), 3);
    EXPECT_EQ(warned(fixture, setter, fins, "7"), "");
    EXPECT_EQ(fins.getFinCount(), 7);
    EXPECT_EQ(warned(fixture, setter, fins, "-1"), "");
    EXPECT_EQ(fins.getFinCount(), 1) << "the fin set's own bound";

    // In a rocket with room for them, a number above 8 is 8 fins without a warning.
    SetterFixture    small;
    TrapezoidFinSet& few = small.tube().addChild(std::make_unique<TrapezoidFinSet>());
    EXPECT_EQ(warned(small, setter, few, "2147483647"), "");
    EXPECT_EQ(few.getFinCount(), 8);
}

// The entries of the table that count instances are made that way: a launch lug's, which has
// no bound of its own, and a fin set's, which has.
TEST(IntSetter, TheTablesInstanceCountsKnowTheBudget)
{
    SetterFixture                      fixture;
    BodyTube&                          tube = tubeInTenThousandPods(fixture);
    LaunchLug&                         lug  = tube.addChild(std::make_unique<LaunchLug>());
    const DocumentConfig::SetterLookup found =
        DocumentConfig::findSetter(lug.kind(), "instancecount");
    ASSERT_NE(found.setter, nullptr);
    WarningSet warnings;
    ASSERT_TRUE(found.setter->set(lug, "9", {}, warnings, fixture.context()).has_value());
    EXPECT_EQ(joinedWarnings(warnings), kWarning);
    EXPECT_EQ(lug.getInstanceCount(), 1);

    TrapezoidFinSet&                   fins  = tube.addChild(std::make_unique<TrapezoidFinSet>());
    const DocumentConfig::SetterLookup count = DocumentConfig::findSetter(fins.kind(), "fincount");
    ASSERT_NE(count.setter, nullptr);
    WarningSet finWarnings;
    // 3 + 10000 + 10000 + 10000 + 30000 instances so far; 7 fins are 40000 more, 6 are 30000.
    ASSERT_TRUE(count.setter->set(fins, "7", {}, finWarnings, fixture.context()).has_value());
    EXPECT_EQ(joinedWarnings(finWarnings), kWarning);
    EXPECT_EQ(fins.getFinCount(), 3);
    ASSERT_TRUE(count.setter->set(fins, "6", {}, finWarnings, fixture.context()).has_value());
    EXPECT_EQ(fins.getFinCount(), 6);
    EXPECT_EQ(DocumentConfig::instanceLoad(fins), 90003U);
}

TEST(IntSetter, TakesNoNoticeOfAttributes)
{
    SetterRecorder  recorder;
    const IntSetter setter(recorder.whole("count"));

    EXPECT_EQ(recorder.apply(setter, "4", "count=7"), "count 4");
}

TEST(IntSetter, AnEmptyFunctionIsAProgrammingError)
{
    EXPECT_THROW(static_cast<void>(IntSetter(IntSetter::SetFunction{})), BugError);
    EXPECT_THROW(static_cast<void>(IntSetter(IntSetter::SetFunction{}, 10)), BugError);
    EXPECT_THROW(
        static_cast<void>(IntSetter(IntSetter::SetFunction{},
                                    IntSetter::InstanceCount{.kept = 8, .refuseAbove = false})),
        BugError);
}

}  // namespace
