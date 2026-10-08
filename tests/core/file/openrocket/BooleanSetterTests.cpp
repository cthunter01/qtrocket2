#include "QtRocket/file/openrocket/BooleanSetter.h"

#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations are OpenRocket's (probe SetterProbe of
// tier 9b, part R1, group "boolean": its BooleanSetter on the flags of a nose cone).

namespace
{

using QtRocket::BooleanSetter;
using QtRocket::BugError;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";

TEST(BooleanSetter, SetsTrueOrFalseWhateverTheCase)
{
    SetterRecorder      recorder;
    const BooleanSetter setter(recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "true"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "false"), "flag false");
    EXPECT_EQ(recorder.apply(setter, "TRUE"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "tRuE"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "False"), "flag false");
    EXPECT_EQ(recorder.apply(setter, "FALSE"), "flag false");
    // The text is trimmed as String.trim() trims.
    EXPECT_EQ(recorder.apply(setter, " true "), "flag true");
    EXPECT_EQ(recorder.apply(setter, " false "), "flag false");
    EXPECT_EQ(recorder.apply(setter, "true\n"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "\ttrue"), "flag true");
}

TEST(BooleanSetter, WarnsOfAnyOtherText)
{
    SetterRecorder      recorder;
    const BooleanSetter setter(recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "yes"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "no"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "on"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "1"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "0"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, ""), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "  "), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "truefalse"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "tru"), kInvalid);
    // A Cyrillic letter that looks like the e, and a no-break space, which is no white space
    // to String.trim().
    EXPECT_EQ(recorder.apply(setter, "tru\xD0\xB5"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "\xC2\xA0true"), kInvalid);
}

TEST(BooleanSetter, TakesNoNoticeOfAttributes)
{
    SetterRecorder      recorder;
    const BooleanSetter setter(recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "true", "value=false"), "flag true");
}

TEST(BooleanSetter, AnEmptyFunctionIsAProgrammingError)
{
    EXPECT_THROW(static_cast<void>(BooleanSetter(BooleanSetter::SetFunction{})), BugError);
}

}  // namespace
