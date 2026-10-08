#include "QtRocket/file/openrocket/DoubleSetter.h"

#include <numbers>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "file/openrocket/SetterTestSupport.h"

// What the class does with a text, call by call. The expectations are OpenRocket's: what its
// DoubleSetter leaves a body tube with for the same texts (probes SetterProbe, group "double", and
// DoubleSetterProbe of tier 9b, part R1). The entries of the setter table are tested with
// OpenRocket's values in setter_vectors_tests.cpp.

namespace
{

using QtRocket::BugError;
using QtRocket::DoubleSetter;
using QtRocket::Test::SetterRecorder;

/// The warning for the number text @p data on the recorder's component, in brackets.
[[nodiscard]] std::string invalid(std::string_view data)
{
    return "[Invalid parameter encountered, ignoring. data: '" + std::string(data) +
           "' - Body Tube]";
}

TEST(DoubleSetter, SetsTheNumberOfTheText)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"));

    EXPECT_EQ(recorder.apply(setter, "0.35"), "value 0.35");
    EXPECT_EQ(recorder.apply(setter, "-0.5"), "value -0.5");
    EXPECT_EQ(recorder.apply(setter, "1e3"), "value 1000.0");
    // The text is trimmed as String.trim() trims.
    EXPECT_EQ(recorder.apply(setter, " 0.25\n"), "value 0.25");
    // Double.parseDouble: a sign, a missing digit on either side of the point, the type
    // suffixes, a hexadecimal number, and a text below the smallest number.
    EXPECT_EQ(recorder.apply(setter, "+0.125"), "value 0.125");
    EXPECT_EQ(recorder.apply(setter, ".5"), "value 0.5");
    EXPECT_EQ(recorder.apply(setter, "5."), "value 5.0");
    EXPECT_EQ(recorder.apply(setter, "1e-1d"), "value 0.1");
    EXPECT_EQ(recorder.apply(setter, "1.5f"), "value 1.5");
    EXPECT_EQ(recorder.apply(setter, "1.5D"), "value 1.5");
    EXPECT_EQ(recorder.apply(setter, "0x1p-3"), "value 0.125");
    EXPECT_EQ(recorder.apply(setter, "1e-400"), "value 0.0");
    EXPECT_EQ(recorder.apply(setter, "-0.0"), "value -0.0");
}

TEST(DoubleSetter, WarnsOfATextThatIsNoNumberWithTheTextAndTheComponentsName)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"));

    EXPECT_EQ(recorder.apply(setter, "abc"), invalid("abc"));
    EXPECT_EQ(recorder.apply(setter, ""), invalid(""));
    // What is told is the trimmed text.
    EXPECT_EQ(recorder.apply(setter, "  "), invalid(""));
    EXPECT_EQ(recorder.apply(setter, " x "), invalid("x"));
    EXPECT_EQ(recorder.apply(setter, "1,5"), invalid("1,5"));
    EXPECT_EQ(recorder.apply(setter, "0x10"), invalid("0x10"));
    EXPECT_EQ(recorder.apply(setter, "1_000"), invalid("1_000"));
    EXPECT_EQ(recorder.apply(setter, "0.25 0.5"), invalid("0.25 0.5"));
    // OpenRocket's own "Inf" of the flight data is no number here, nor is another case.
    EXPECT_EQ(recorder.apply(setter, "Inf"), invalid("Inf"));
    EXPECT_EQ(recorder.apply(setter, "nan"), invalid("nan"));
    EXPECT_EQ(recorder.apply(setter, "infinity"), invalid("infinity"));
    // A no-break space is no white space to String.trim() or to Double.parseDouble.
    EXPECT_EQ(recorder.apply(setter,
                             "\xC2\xA0"
                             "0.25"),
              invalid("\xC2\xA0"
                      "0.25"));

    // The name is the component's own, not the name of its class.
    recorder.component().setName("My Tube");
    EXPECT_EQ(recorder.apply(setter, "abc"),
              "[Invalid parameter encountered, ignoring. data: 'abc' - My Tube]");
}

TEST(DoubleSetter, RefusesANumberThatIsNotFinite)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"));

    EXPECT_EQ(recorder.apply(setter, "NaN"), invalid("NaN"));
    EXPECT_EQ(recorder.apply(setter, "Infinity"), invalid("Infinity"));
    EXPECT_EQ(recorder.apply(setter, "-Infinity"), invalid("-Infinity"));
    EXPECT_EQ(recorder.apply(setter, "+Infinity"), invalid("+Infinity"));
    // Double.parseDouble makes an infinity of a number beyond the largest double.
    EXPECT_EQ(recorder.apply(setter, "1e400"), invalid("1e400"));
    EXPECT_EQ(recorder.apply(setter, "-1e400"), invalid("-1e400"));
}

TEST(DoubleSetter, MultipliesTheNumber)
{
    SetterRecorder recorder;
    // The multiplier of the parameters a file holds in degrees: Math.PI / 180.0.
    const DoubleSetter degrees(recorder.number("value"), std::numbers::pi / 180.0);

    EXPECT_EQ(recorder.apply(degrees, "45"), "value 0.7853981633974483");
    EXPECT_EQ(recorder.apply(degrees, "-90"), "value -1.5707963267948966");
    EXPECT_EQ(recorder.apply(degrees, "180"), "value 3.141592653589793");
    EXPECT_EQ(recorder.apply(degrees, "1"), "value 0.017453292519943295");
    EXPECT_EQ(recorder.apply(degrees, "0"), "value 0.0");
    EXPECT_EQ(recorder.apply(degrees, "abc"), invalid("abc"));
    EXPECT_EQ(recorder.apply(degrees, "NaN"), invalid("NaN"));

    const DoubleSetter twice(recorder.number("value"), 2.0);
    EXPECT_EQ(recorder.apply(twice, "0.25"), "value 0.5");
    EXPECT_EQ(recorder.apply(twice, "-0.5"), "value -1.0");
}

// Deviation (decision L3): OpenRocket checks the number it read and sets the infinity that the
// multiplication makes (DoubleSetterProbe: a multiplier of 2 and "1e308" give a body tube of
// infinite length). Here what is not finite is never set.
TEST(DoubleSetter, RefusesAProductThatIsNotFinite)
{
    SetterRecorder     recorder;
    const DoubleSetter twice(recorder.number("value"), 2.0);

    EXPECT_EQ(recorder.apply(twice, "1e308"), invalid("1e308"));
    EXPECT_EQ(recorder.apply(twice, "-1e308"), invalid("-1e308"));
    EXPECT_EQ(recorder.apply(twice, "8e307"), "value 1.6E308");
}

TEST(DoubleSetter, SwitchesTheFlagOnForTheSpecialWord)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"), "filled", recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "filled"), "flag true");
    // Compared as String.equalsIgnoreCase() compares, after the trim.
    EXPECT_EQ(recorder.apply(setter, "FILLED"), "flag true");
    EXPECT_EQ(recorder.apply(setter, " Filled "), "flag true");
    // U+0130, the dotted capital I, equals "i" there.
    EXPECT_EQ(recorder.apply(setter, "f\xC4\xB0lled"), "flag true");

    // Any other text is the number; the flag is never switched off.
    EXPECT_EQ(recorder.apply(setter, "0.003"), "value 0.003");
    EXPECT_EQ(recorder.apply(setter, "abc"), invalid("abc"));
    EXPECT_EQ(recorder.apply(setter, "NaN"), invalid("NaN"));
    // Without a separator the word cannot stand beside a number.
    EXPECT_EQ(recorder.apply(setter, "filled 0.001"), invalid("filled 0.001"));
    EXPECT_EQ(recorder.apply(setter, "0.5 filled"), invalid("0.5 filled"));
    EXPECT_EQ(recorder.apply(setter, "fill"), invalid("fill"));
    EXPECT_EQ(recorder.apply(setter, ""), invalid(""));
}

TEST(DoubleSetter, WithASeparatorSetsTheValueAndThenTheFlag)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"), "auto", ' ', recorder.truth("flag"));

    // The order is 'value, then flag'.
    EXPECT_EQ(recorder.apply(setter, "auto 0.0125"), "value 0.0125, flag true");
    EXPECT_EQ(recorder.apply(setter, "Auto 0.5"), "value 0.5, flag true");
    EXPECT_EQ(recorder.apply(setter, "AUTO 0.04 "), "value 0.04, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto +0.03"), "value 0.03, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto -0.01"), "value -0.01, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto 1e-1d"), "value 0.1, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto 0x1p-3"), "value 0.125, flag true");
    // Two blanks: the number is " 0.03", which Double.parseDouble trims.
    EXPECT_EQ(recorder.apply(setter, "auto  0.03"), "value 0.03, flag true");

    // The word alone is the flag alone.
    EXPECT_EQ(recorder.apply(setter, "auto"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "AUTO"), "flag true");
    EXPECT_EQ(recorder.apply(setter, " auto"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "auto "), "flag true");
    EXPECT_EQ(recorder.apply(setter, "auto  "), "flag true");

    // A number alone is the value alone.
    EXPECT_EQ(recorder.apply(setter, "0.0125"), "value 0.0125");
    EXPECT_EQ(recorder.apply(setter, " 0.02 "), "value 0.02");
    EXPECT_EQ(recorder.apply(setter, "-0.01"), "value -0.01");
}

TEST(DoubleSetter, WithASeparatorWarnsAndStillSetsTheFlagUnlessTheNumberIsNotFinite)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"), "auto", ' ', recorder.truth("flag"));

    // A text behind the word that is no number: the warning, and the flag all the same.
    EXPECT_EQ(recorder.apply(setter, "auto abc"), "flag true " + invalid("abc"));
    EXPECT_EQ(recorder.apply(setter, "auto auto"), "flag true " + invalid("auto"));
    EXPECT_EQ(recorder.apply(setter, "auto 0.01 0.02"), "flag true " + invalid("0.01 0.02"));
    EXPECT_EQ(recorder.apply(setter, "auto x y"), "flag true " + invalid("x y"));
    EXPECT_EQ(recorder.apply(setter, "auto 0.05 auto"), "flag true " + invalid("0.05 auto"));

    // A number behind the word that is not finite: the warning, and nothing is set (OpenRocket
    // leaves the setter there).
    EXPECT_EQ(recorder.apply(setter, "auto NaN"), invalid("NaN"));
    EXPECT_EQ(recorder.apply(setter, "auto Infinity"), invalid("Infinity"));
    EXPECT_EQ(recorder.apply(setter, "auto -Infinity"), invalid("-Infinity"));
    EXPECT_EQ(recorder.apply(setter, "auto 1e400"), invalid("1e400"));

    // The first part is the word, whatever it is, and the rest the number.
    EXPECT_EQ(recorder.apply(setter, "0.01 auto"), invalid("auto"));
    EXPECT_EQ(recorder.apply(setter, "x auto"), invalid("auto"));
    EXPECT_EQ(recorder.apply(setter, "0.04 0.05"), "value 0.05");

    // Only the separator splits: a tab, a comma and a no-break space do not.
    EXPECT_EQ(recorder.apply(setter, "auto\t0.0125"), invalid("auto\t0.0125"));
    EXPECT_EQ(recorder.apply(setter, "auto,0.05"), invalid("auto,0.05"));
    EXPECT_EQ(recorder.apply(setter,
                             "auto\xC2\xA0"
                             "0.5"),
              invalid("auto\xC2\xA0"
                      "0.5"));
    EXPECT_EQ(recorder.apply(setter, "automatic"), invalid("automatic"));
    EXPECT_EQ(recorder.apply(setter, ""), invalid(""));
    EXPECT_EQ(recorder.apply(setter, "   "), invalid(""));
}

// A separator the setter table does not use, to show that the text is split as String.split()
// splits it: empty parts at the end are dropped, one in front is kept, and a text that is one
// part is the word and the number whole (DoubleSetterProbe, "comma").
TEST(DoubleSetter, SplitsAtAnySeparatorAsJavasStringSplitDoes)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"), "auto", ',', recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "auto,0.5"), "value 0.5, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto, 0.5"), "value 0.5, flag true");
    EXPECT_EQ(recorder.apply(setter, "Auto,0.25,"), "value 0.25, flag true");
    EXPECT_EQ(recorder.apply(setter, "auto"), "flag true");
    EXPECT_EQ(recorder.apply(setter, "0.5"), "value 0.5");
    // An empty word in front.
    EXPECT_EQ(recorder.apply(setter, ",0.5"), "value 0.5");
    // The parts behind the word are joined with the separator again.
    EXPECT_EQ(recorder.apply(setter, "auto,,0.5"), "flag true " + invalid(",0.5"));
    EXPECT_EQ(recorder.apply(setter, "auto,0.25,0.5"), "flag true " + invalid("0.25,0.5"));
    EXPECT_EQ(recorder.apply(setter, "auto,abc"), "flag true " + invalid("abc"));
    EXPECT_EQ(recorder.apply(setter, "auto,NaN"), invalid("NaN"));
    // One part after the empty ones at the end are dropped: the whole text is word and number.
    EXPECT_EQ(recorder.apply(setter, "auto,"), invalid("auto,"));
    EXPECT_EQ(recorder.apply(setter, ","), invalid(","));
    EXPECT_EQ(recorder.apply(setter, ",,"), invalid(",,"));
    EXPECT_EQ(recorder.apply(setter, ""), invalid(""));
    EXPECT_EQ(recorder.apply(setter, "0.5,auto"), invalid("auto"));
    // The parts are not trimmed: "auto " is not the word.
    EXPECT_EQ(recorder.apply(setter, " auto , 0.5 "), "value 0.5");
    // A blank does not split here.
    EXPECT_EQ(recorder.apply(setter, "auto 0.5"), invalid("auto 0.5"));
}

TEST(DoubleSetter, TakesNoNoticeOfAttributesAndNeverFails)
{
    SetterRecorder     recorder;
    const DoubleSetter setter(recorder.number("value"), "auto", ' ', recorder.truth("flag"));

    EXPECT_EQ(recorder.apply(setter, "auto 0.5", "method=top|auto=false"), "value 0.5, flag true");
    EXPECT_EQ(recorder.apply(setter, "abc", "a=b"), invalid("abc"));
}

TEST(DoubleSetter, AnEmptyFunctionIsAProgrammingError)
{
    using Set     = DoubleSetter::SetFunction;
    using Special = DoubleSetter::SpecialFunction;

    SetterRecorder recorder;
    EXPECT_THROW(static_cast<void>(DoubleSetter(Set{})), BugError);
    EXPECT_THROW(static_cast<void>(DoubleSetter(Set{}, 2.0)), BugError);
    EXPECT_THROW(static_cast<void>(DoubleSetter(Set{}, "auto", recorder.truth("flag"))), BugError);
    EXPECT_THROW(static_cast<void>(DoubleSetter(recorder.number("value"), "auto", Special{})),
                 BugError);
    EXPECT_THROW(static_cast<void>(DoubleSetter(recorder.number("value"), "auto", ' ', Special{})),
                 BugError);
    EXPECT_THROW(static_cast<void>(DoubleSetter(Set{}, "auto", ' ', recorder.truth("flag"))),
                 BugError);
}

}  // namespace
