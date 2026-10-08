#include "QtRocket/file/openrocket/ColorSetter.h"

#include <format>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with the attributes and the text of an element. The expectations are
// OpenRocket's (probe SetterProbe of tier 9b, part R1, group "color": its ColorSetter on the
// colour of a nose cone).

namespace
{

using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ColorSetter;
using QtRocket::RocketComponent;
using QtRocket::Test::SetterRecorder;

constexpr std::string_view kInvalid = "[Invalid parameter encountered, ignoring.]";

/// A function for a setter that notes "color <red> <green> <blue> <alpha>" with @p recorder.
[[nodiscard]] ColorSetter::SetFunction noteColor(SetterRecorder& recorder)
{
    return [&recorder](RocketComponent& component, const Color& color) {
        recorder.note(component, std::format("color {} {} {} {}", color.red(), color.green(),
                                             color.blue(), color.alpha()));
    };
}

TEST(ColorSetter, SetsTheColourOfTheAttributes)
{
    SetterRecorder    recorder;
    const ColorSetter setter(noteColor(recorder));

    // Without alpha the colour is opaque.
    EXPECT_EQ(recorder.apply(setter, "", "red=1|green=2|blue=3"), "color 1 2 3 255");
    EXPECT_EQ(recorder.apply(setter, "", "red=1|green=2|blue=3|alpha=4"), "color 1 2 3 4");
    EXPECT_EQ(recorder.apply(setter, "", "red=0|green=0|blue=0|alpha=0"), "color 0 0 0 0");
    // Integer.parseInt takes a sign.
    EXPECT_EQ(recorder.apply(setter, "", "red=+1|green=2|blue=3"), "color 1 2 3 255");
    // Other attributes are not looked at.
    EXPECT_EQ(recorder.apply(setter, "", "red=255|green=255|blue=255|alpha=255|extra=1"),
              "color 255 255 255 255");
}

TEST(ColorSetter, WarnsOfAttributesThatMakeNoColourAndSetsNothing)
{
    SetterRecorder    recorder;
    const ColorSetter setter(noteColor(recorder));

    // A channel is missing.
    EXPECT_EQ(recorder.apply(setter, ""), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "", "red=1|green=2"), kInvalid);
    // Attribute names are compared exactly.
    EXPECT_EQ(recorder.apply(setter, "", "Red=1|green=2|blue=3"), kInvalid);
    // A channel is no int: nothing is trimmed.
    EXPECT_EQ(recorder.apply(setter, "", "red= 1|green=2|blue=3|alpha=4"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "", "red=1.0|green=2|blue=3"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "", "red=1|green=2|blue=3|alpha="), kInvalid);
    // A channel is out of range.
    EXPECT_EQ(recorder.apply(setter, "", "red=1|green=2|blue=300"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "", "red=-1|green=2|blue=3"), kInvalid);
    EXPECT_EQ(recorder.apply(setter, "", "red=255|green=0|blue=0|alpha=256"), kInvalid);
    // The text is not looked at then: one warning, not two of them.
    EXPECT_EQ(recorder.apply(setter, "text", "red=1|green=2|blue=3|alpha=x"), kInvalid);
}

TEST(ColorSetter, WarnsOfATextInTheElementAfterItHasSetTheColour)
{
    SetterRecorder    recorder;
    const ColorSetter setter(noteColor(recorder));

    EXPECT_EQ(recorder.apply(setter, "x", "red=1|green=2|blue=3"),
              "color 1 2 3 255 [Invalid parameter encountered, ignoring.]");
    // A text that String.trim() empties is none.
    EXPECT_EQ(recorder.apply(setter, " \n\t", "red=1|green=2|blue=3"), "color 1 2 3 255");
}

TEST(ColorSetter, AnEmptyFunctionIsAProgrammingError)
{
    EXPECT_THROW(static_cast<void>(ColorSetter(ColorSetter::SetFunction{})), BugError);
}

}  // namespace
