#include "QtRocket/file/openrocket/StringSetter.h"

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text. The expectations are OpenRocket's (probe SetterProbe of
// tier 9b, part R1, group "string": its StringSetter on a component's name, comment and id).

namespace
{

using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::StringSetter;
using QtRocket::Test::SetterRecorder;

/// A function for a setter that notes "text [<the text>]" with @p recorder and succeeds.
[[nodiscard]] StringSetter::SetFunction noteText(SetterRecorder& recorder)
{
    return [&recorder](RocketComponent& component, std::string_view text) -> Result<void> {
        recorder.note(component, "text [" + std::string(text) + "]");
        return {};
    };
}

/// A function for a setter that fails as RocketComponent.setID(String) fails in Java.
[[nodiscard]] Result<void> refuse(RocketComponent& /*component*/, std::string_view text)
{
    return QtRocket::fail(ErrorCode::INVALID_ARGUMENT, "Invalid UUID string: " + std::string(text));
}

TEST(StringSetter, SetsTheTextAsItIs)
{
    SetterRecorder     recorder;
    const StringSetter setter(noteText(recorder));

    EXPECT_EQ(recorder.apply(setter, "x"), "text [x]");
    // Nothing is trimmed: a name keeps its blanks and a comment its line breaks.
    EXPECT_EQ(recorder.apply(setter, " padded name "), "text [ padded name ]");
    EXPECT_EQ(recorder.apply(setter, "two\nlines"), "text [two\nlines]");
    EXPECT_EQ(recorder.apply(setter, "\t\n"), "text [\t\n]");
    EXPECT_EQ(recorder.apply(setter, ""), "text []");
    EXPECT_EQ(recorder.apply(setter, "x", "name=y"), "text [x]");
}

// OpenRocket: an IllegalArgumentException of the setter method leaves the loader, which fails
// the load with "Exception loading stream: " and the message.
TEST(StringSetter, PassesTheFailureOfItsFunctionOn)
{
    SetterRecorder     recorder;
    const StringSetter setter(&refuse);

    EXPECT_EQ(recorder.apply(setter, "not-a-uuid"),
              "FAILED INVALID_ARGUMENT: Invalid UUID string: not-a-uuid");
    EXPECT_EQ(recorder.apply(setter, ""), "FAILED INVALID_ARGUMENT: Invalid UUID string: ");
}

TEST(StringSetter, AnEmptyFunctionIsAProgrammingError)
{
    EXPECT_THROW(static_cast<void>(StringSetter(StringSetter::SetFunction{})), BugError);
}

}  // namespace
