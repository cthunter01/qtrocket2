#include "QtRocket/file/openrocket/EnumSetter.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations are OpenRocket's (probe SetterProbe of
// tier 9b, part R1, group "enum": its EnumSetter on the line style and the finish of a nose
// cone). Every enum of the setter table is tested with every constant in
// setter_vectors_tests.cpp.

namespace
{

using QtRocket::BugError;
using QtRocket::EnumSetter;
using QtRocket::Finish;
using QtRocket::LineStyle;
using QtRocket::RocketComponent;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";

using StyleFunction  = std::function<void(RocketComponent&, LineStyle)>;
using FinishFunction = std::function<void(RocketComponent&, Finish)>;

/// A function for a setter that notes "style <NAME>" with @p recorder.
[[nodiscard]] StyleFunction noteStyle(SetterRecorder& recorder)
{
    return [&recorder](RocketComponent& component, LineStyle style) {
        recorder.note(component, "style " + std::string(lineStyleName(style)));
    };
}

/// A function for a setter that notes "finish <NAME>" with @p recorder.
[[nodiscard]] FinishFunction noteFinish(SetterRecorder& recorder)
{
    return [&recorder](RocketComponent& component, Finish finish) {
        recorder.note(component, "finish " + std::string(finishName(finish)));
    };
}

/// A lookup of its own: every text but "none" is DOTTED.
[[nodiscard]] std::optional<LineStyle> dottedUnlessNone(std::string_view text)
{
    if (text == "none")
    {
        return std::nullopt;
    }
    return LineStyle::DOTTED;
}

TEST(EnumSetter, SetsTheConstantTheTextNames)
{
    SetterRecorder   recorder;
    const EnumSetter setter(&QtRocket::lineStyleFromOrkName, noteStyle(recorder));

    EXPECT_EQ(recorder.apply(setter, "solid"), "style SOLID");
    EXPECT_EQ(recorder.apply(setter, "dashed"), "style DASHED");
    EXPECT_EQ(recorder.apply(setter, "dotted"), "style DOTTED");
    EXPECT_EQ(recorder.apply(setter, "dashdot"), "style DASHDOT");
    // DocumentConfig.findEnum() trims the text as String.trim() trims.
    EXPECT_EQ(recorder.apply(setter, " solid "), "style SOLID");
    EXPECT_EQ(recorder.apply(setter, "\tdashed\n"), "style DASHED");
}

TEST(EnumSetter, WarnsOfATextThatNamesNoConstant)
{
    SetterRecorder   recorder;
    const EnumSetter setter(&QtRocket::lineStyleFromOrkName, noteStyle(recorder));

    // Neither the constant's own name nor another case of the .ork name.
    EXPECT_EQ(recorder.apply(setter, "DASHED"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "Solid"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "dash_dot"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, ""), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "bogus"), kInvalid);
}

// The names of a constant with an underscore: "finishpolished" is FINISHPOLISHED, and what a
// saver would write for the name with an underscore does not match.
TEST(EnumSetter, MatchesTheNameWithoutItsUnderscores)
{
    SetterRecorder   recorder;
    const EnumSetter setter(&QtRocket::finishFromOrkName, noteFinish(recorder));

    EXPECT_EQ(recorder.apply(setter, " normal "), "finish NORMAL");
    EXPECT_EQ(recorder.apply(setter, "finishpolished"), "finish FINISHPOLISHED");
    EXPECT_EQ(recorder.apply(setter, "roughunfinished"), "finish ROUGHUNFINISHED");
    EXPECT_EQ(recorder.apply(setter, "finish_polished"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "ROUGH"), kInvalid);
}

TEST(EnumSetter, AsksTheFunctionItWasGivenForTheConstant)
{
    SetterRecorder   recorder;
    const EnumSetter setter(&dottedUnlessNone, noteStyle(recorder));

    EXPECT_EQ(recorder.apply(setter, "anything"), "style DOTTED");
    EXPECT_EQ(recorder.apply(setter, ""), "style DOTTED");
    EXPECT_EQ(recorder.apply(setter, "none"), kInvalid);
    // The function gets the text as it is, the white space included.
    EXPECT_EQ(recorder.apply(setter, " none"), "style DOTTED");
}

TEST(EnumSetter, TakesNoNoticeOfAttributes)
{
    SetterRecorder   recorder;
    const EnumSetter setter(&QtRocket::lineStyleFromOrkName, noteStyle(recorder));

    EXPECT_EQ(recorder.apply(setter, "dashed", "style=solid"), "style DASHED");
}

TEST(EnumSetter, AMissingFunctionIsAProgrammingError)
{
    using Find = std::optional<LineStyle> (*)(std::string_view);

    SetterRecorder recorder;
    EXPECT_THROW(static_cast<void>(EnumSetter(Find{nullptr}, noteStyle(recorder))), BugError);
    EXPECT_THROW(static_cast<void>(EnumSetter(&QtRocket::lineStyleFromOrkName, StyleFunction{})),
                 BugError);
}

}  // namespace
