#include "QtRocket/file/openrocket/OverrideSetter.h"

#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations for a finite number and for a text that is
// no number are OpenRocket's (probe SetterProbe of tier 9b, part R1, group "override": its
// OverrideSetter on the override mass, centre of gravity and drag coefficient of a stage); what
// is not finite is refused here and stored there (decision L3).

namespace
{

using QtRocket::BugError;
using QtRocket::OverrideSetter;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";

TEST(OverrideSetter, SetsTheValueAndThenSwitchesTheOverrideOn)
{
    SetterRecorder       recorder;
    const OverrideSetter setter(recorder.number("value"), recorder.truth("enabled"));

    EXPECT_EQ(recorder.apply(setter, "0.25"), "value 0.25, enabled true");
    EXPECT_EQ(recorder.apply(setter, "0"), "value 0.0, enabled true");
    // Double.parseDouble: white space around the number, a hexadecimal number, a type suffix.
    EXPECT_EQ(recorder.apply(setter, " 0.5 "), "value 0.5, enabled true");
    EXPECT_EQ(recorder.apply(setter, "0x1p-1"), "value 0.5, enabled true");
    EXPECT_EQ(recorder.apply(setter, "1.5d"), "value 1.5, enabled true");
    // What a negative value means is the component's business (a mass becomes 0, a position
    // or a drag coefficient stays).
    EXPECT_EQ(recorder.apply(setter, "-1"), "value -1.0, enabled true");
    EXPECT_EQ(recorder.apply(setter, "-0.0"), "value -0.0, enabled true");
}

TEST(OverrideSetter, WarnsOfATextThatIsNoNumberAndChangesNothing)
{
    SetterRecorder       recorder;
    const OverrideSetter setter(recorder.number("value"), recorder.truth("enabled"));

    EXPECT_EQ(recorder.apply(setter, "abc"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, ""), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "nan"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "Inf"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "0.5 kg"), kInvalid);
}

// Decision L3. OpenRocket: <overridemass>NaN</overridemass> gives a stage whose mass is
// overridden with NaN, "Infinity" and "1e400" one of infinite mass, "-Infinity" one of mass 0;
// the same for the centre of gravity and the drag coefficient, each stored as it is read.
TEST(OverrideSetter, RefusesANumberThatIsNotFinite)
{
    SetterRecorder       recorder;
    const OverrideSetter setter(recorder.number("value"), recorder.truth("enabled"));

    EXPECT_EQ(recorder.apply(setter, "NaN"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "Infinity"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "-Infinity"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "+Infinity"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "1e400"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "-1e400"), kInvalid);
    // The largest and the smallest number are numbers.
    EXPECT_EQ(recorder.apply(setter, "1.7976931348623157e308"),
              "value 1.7976931348623157E308, enabled true");
    EXPECT_EQ(recorder.apply(setter, "1e-400"), "value 0.0, enabled true");
}

TEST(OverrideSetter, TakesNoNoticeOfAttributes)
{
    SetterRecorder       recorder;
    const OverrideSetter setter(recorder.number("value"), recorder.truth("enabled"));

    EXPECT_EQ(recorder.apply(setter, "0.25", "enabled=false"), "value 0.25, enabled true");
}

TEST(OverrideSetter, AnEmptyFunctionIsAProgrammingError)
{
    SetterRecorder recorder;
    EXPECT_THROW(
        static_cast<void>(OverrideSetter(OverrideSetter::SetFunction{}, recorder.truth("enabled"))),
        BugError);
    EXPECT_THROW(static_cast<void>(
                     OverrideSetter(recorder.number("value"), OverrideSetter::EnabledFunction{})),
                 BugError);
}

}  // namespace
