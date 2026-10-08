#include "QtRocket/file/openrocket/IntSetter.h"

#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/util/BugError.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations of the setter without a maximum are
// OpenRocket's (probe SetterProbe of tier 9b, part R1, group "int": its IntSetter on the fin
// count of a fin set, a parachute's line count and the instance counts); the maximum is
// QtRocket's own (decision L6).

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentConfig;
using QtRocket::IntSetter;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";

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
}

}  // namespace
