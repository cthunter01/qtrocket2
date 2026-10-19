#include "QtRocket/util/ExpressionParser.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "util/ExpressionParserParityCases.h"

// OpenRocket's ExpressionParserTest (ported first), then the parity with OpenRocket's own parser
// (util/ExpressionParserParityCases.h, written by tools/openrocket-expression-parser from
// OpenRocket's ExpressionParser: every value expected below that does not come from
// ExpressionParserTest is a value of that table), the quirks one by one, and hostile input.
// Non-ASCII text is written as its UTF-8 bytes; a string literal is split where a hexadecimal
// escape would otherwise swallow the digit after it.

namespace
{

using QtRocket::ErrorCode;
using QtRocket::ExpressionParser;
using QtRocket::Result;
using QtRocket::Test::ExpressionParityCase;
using QtRocket::Test::ParityComparison;

/// ExpressionParserTest.EPS.
constexpr double kEps = 1.0e-10;

/// The relative tolerance of a parity case whose value involves Math.pow or a transcendental
/// function (CLAUDE.md, Portability): the libraries differ in the last bits only.
constexpr double kRelativeTolerance = 1.0e-14;

/// The value of @p text, which must parse; NaN where it does not.
[[nodiscard]] double valueOf(std::string_view text)
{
    const Result<double> result = ExpressionParser::parse(text);
    EXPECT_TRUE(result.has_value()) << "'" << text << "': " << result.error().toString();
    return result.value_or(std::numeric_limits<double>::quiet_NaN());
}

/// Whether @p text fails as OpenRocket's InvalidExpressionException does: with ErrorCode::PARSE.
[[nodiscard]] bool isInvalid(std::string_view text)
{
    const Result<double> result = ExpressionParser::parse(text);
    return !result.has_value() && result.error().code == ErrorCode::PARSE;
}

/// ExpressionParserTest.expectInvalid().
void expectInvalid(std::string_view text)
{
    const Result<double> result = ExpressionParser::parse(text);
    EXPECT_FALSE(result.has_value()) << "Expression '" << text << "' evaluated to "
                                     << result.value_or(0) << ", expected failure";
    if (!result.has_value())
    {
        EXPECT_EQ(result.error().code, ErrorCode::PARSE) << text;
    }
}

[[nodiscard]] std::uint64_t bitsOf(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

/// Bit for bit, a NaN as any NaN (its bits depend on the processor).
void expectSameDouble(double actual, double expected)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << actual;
        return;
    }
    EXPECT_EQ(bitsOf(actual), bitsOf(expected)) << actual << " != " << expected;
}

/// Within kRelativeTolerance, with an absolute floor for a value that is zero; NaN and the
/// infinities exactly.
void expectCloseDouble(double actual, double expected)
{
    if (!std::isfinite(expected))
    {
        expectSameDouble(actual, expected);
        return;
    }
    EXPECT_NEAR(actual, expected, (std::abs(expected) * kRelativeTolerance) + 1e-300);
}

void expectParity(const ExpressionParityCase& parityCase)
{
    const Result<double> result = ExpressionParser::parse(parityCase.input);
    ASSERT_EQ(result.has_value(), parityCase.valid)
        << (result.has_value() ? std::to_string(*result) : result.error().toString());
    if (!result.has_value())
    {
        EXPECT_EQ(result.error().code, ErrorCode::PARSE);
        return;
    }
    const auto expected = std::bit_cast<double>(parityCase.bits);
    switch (parityCase.comparison)
    {
        case ParityComparison::EXACT:
            expectSameDouble(*result, expected);
            break;
        case ParityComparison::TOLERANCE:
            expectCloseDouble(*result, expected);
            break;
        case ParityComparison::RANDOM:
            break;
    }
}

/// The value OpenRocket gives @p c alone (kSingleCharacterDigits), or -1 where it is invalid.
[[nodiscard]] int openRocketDigitOf(char32_t c)
{
    for (const QtRocket::Test::SingleCharacterDigits& run : QtRocket::Test::kSingleCharacterDigits)
    {
        const auto first = static_cast<char32_t>(run.first);
        if (c >= first && c <= static_cast<char32_t>(run.last))
        {
            return run.firstValue + static_cast<int>(c - first);
        }
    }
    return -1;
}

/// Parses @p c alone and compares with OpenRocket; true when it parsed.
bool expectSingleCharacter(char32_t c)
{
    const std::string    text   = QtRocket::Strings::fromCodePoints(std::u32string(1, c));
    const Result<double> result = ExpressionParser::parse(text);
    const int            digit  = openRocketDigitOf(c);
    if (digit < 0)
    {
        EXPECT_FALSE(result.has_value())
            << std::format("U+{:04X} alone gives {}", static_cast<unsigned>(c), result.value_or(0));
        return result.has_value();
    }
    EXPECT_TRUE(result.has_value()) << std::format("U+{:04X} alone", static_cast<unsigned>(c));
    EXPECT_EQ(result.value_or(-1), static_cast<double>(digit))
        << std::format("U+{:04X}", static_cast<unsigned>(c));
    return result.has_value();
}

/// @p count copies of @p text.
[[nodiscard]] std::string repeated(std::string_view text, std::size_t count)
{
    std::string out;
    out.reserve(text.size() * count);
    for (std::size_t i = 0; i < count; ++i)
    {
        out += text;
    }
    return out;
}

/// @p depth brackets around "1".
[[nodiscard]] std::string nested(std::size_t depth)
{
    return repeated("(", depth) + "1" + repeated(")", depth);
}

/// The values of @p count parses of @p text (a random() call); every one must parse.
[[nodiscard]] std::vector<double> drawRandom(std::string_view text, std::size_t count)
{
    std::vector<double> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        values.push_back(valueOf(text));
    }
    return values;
}

/// Every one of @p values, a result of random(@p scale), is @p scale times a number from [0, 1).
void expectRandomRange(const std::vector<double>& values, double scale)
{
    for (const double value : values)
    {
        EXPECT_TRUE(value / scale >= 0 && value / scale < 1)
            << value << " of random(" << scale << ")";
    }
}

/// Parses "1 1/2*2" and "random(1)" @p count times each; the sum of the first and of the failures
/// of the second (3 per round).
[[nodiscard]] double parseRepeatedly(int count)
{
    double sum = 0;
    for (int i = 0; i < count; ++i)
    {
        const Result<double> fixed  = ExpressionParser::parse("1 1/2*2");
        const Result<double> random = ExpressionParser::parse("random(1)");
        sum += fixed.value_or(0) + (random.has_value() ? 0 : 1);
    }
    return sum;
}

// ----------------------------------------------------------------------------------------------
// Port of OpenRocket's ExpressionParserTest (core/src/test/java/info/openrocket/core/util)

// Port of ExpressionParserTest.testPlainNumber.
TEST(ExpressionParser, PlainNumber)
{
    EXPECT_NEAR(valueOf("1"), 1.0, kEps);
    EXPECT_NEAR(valueOf("\t 1 "), 1.0, kEps);
    EXPECT_NEAR(valueOf(".9"), 0.9, kEps);
    EXPECT_NEAR(valueOf("1."), 1.0, kEps);
    EXPECT_NEAR(valueOf("1.2"), 1.2, kEps);
    EXPECT_NEAR(valueOf("01.200"), 1.2, kEps);
}

// Port of ExpressionParserTest.testNegativeNumber.
TEST(ExpressionParser, NegativeNumber)
{
    EXPECT_NEAR(valueOf("-1"), -1.0, kEps);
    EXPECT_NEAR(valueOf("-15"), -15.0, kEps);
    EXPECT_NEAR(valueOf("-.9"), -0.9, kEps);
    EXPECT_NEAR(valueOf("-1."), -1.0, kEps);
    EXPECT_NEAR(valueOf("-1.2"), -1.2, kEps);
    EXPECT_NEAR(valueOf("-01.200"), -1.2, kEps);
}

// Port of ExpressionParserTest.testDecimalComma.
TEST(ExpressionParser, DecimalComma)
{
    EXPECT_NEAR(valueOf("1,"), 1.0, kEps);
    EXPECT_NEAR(valueOf("1,2"), 1.2, kEps);
    EXPECT_NEAR(valueOf("01,200"), 1.2, kEps);
    EXPECT_NEAR(valueOf(",9"), 0.9, kEps);
}

// Port of ExpressionParserTest.testSimpleExpression.
TEST(ExpressionParser, SimpleExpression)
{
    EXPECT_NEAR(valueOf("1+2"), 3.0, kEps);
    EXPECT_NEAR(valueOf("1+2.5*2"), 6.0, kEps);
    EXPECT_NEAR(valueOf("(1+2.5) * 2"), 7.0, kEps);
    EXPECT_NEAR(valueOf("1+2/3"), 1.0 + (2.0 / 3.0), kEps);
}

// Port of ExpressionParserTest.testFraction (which checks two of its inputs twice).
TEST(ExpressionParser, Fraction)
{
    EXPECT_NEAR(valueOf("1 1/2"), 1.5, kEps);
    EXPECT_NEAR(valueOf("11 11/22"), 11 + (11.0 / 22.0), kEps);
    EXPECT_NEAR(valueOf("-11 11/22"), -11 - (11.0 / 22.0), kEps);
    EXPECT_NEAR(valueOf("  1    1 / 2"), 1.5, kEps);
    EXPECT_NEAR(valueOf("  11    11 / 22"), 11 + (11.0 / 22.0), kEps);
    EXPECT_NEAR(valueOf("1 + 1 3/7"), 2.0 + (3.0 / 7.0), kEps);
    EXPECT_NEAR(valueOf("1 + 1 3/7"), 2.0 + (3.0 / 7.0), kEps);
    EXPECT_NEAR(valueOf("1 1/2* 2"), 3.0, kEps);
    EXPECT_NEAR(valueOf("1 1/2* 2"), 3.0, kEps);
}

// Port of ExpressionParserTest.testCharConversion: "1 U+2081 U+2044 U+2089" (subscript one,
// fraction slash, subscript nine).
TEST(ExpressionParser, CharConversion)
{
    EXPECT_NEAR(valueOf("1 \xE2\x82\x81 \xE2\x81\x84 \xE2\x82\x89"), 1 + (1.0 / 9.0), kEps);
}

// Port of ExpressionParserTest.testInvalidExpression.
TEST(ExpressionParser, InvalidExpression)
{
    expectInvalid("1+");
    expectInvalid("1+2/");
    expectInvalid("1 2");
    expectInvalid("12 2.5");
    expectInvalid("1 2.5/4");
    expectInvalid("11 22.55/44");
    expectInvalid("1 2/4.1");
    expectInvalid("11 22/44.11");
    expectInvalid("1.2 3/4");
    expectInvalid("12.23 34/45");

    expectInvalid("1. 2");
    expectInvalid("1 .2");
}

// ----------------------------------------------------------------------------------------------
// Parity with OpenRocket's parser

// The inputs of parity-inputs.txt: the M2 bindings scout's 83 (82 of them deterministic), the rest
// of ExpressionParserTest's, and the quirks below, each with OpenRocket's result.
TEST(ExpressionParser, MatchesOpenRocketOnEveryParityInput)
{
    for (const ExpressionParityCase& parityCase : QtRocket::Test::kExpressionParityCases)
    {
        SCOPED_TRACE(std::string(parityCase.escaped));
        expectParity(parityCase);
    }
}

// The digit table end to end: every char of the Basic Multilingual Plane alone parses exactly
// where OpenRocket's parser takes it, to the same digit (550 chars of 73 scripts and forms).
TEST(ExpressionParser, TakesAloneExactlyTheCharsOpenRocketTakes)
{
    std::size_t parsed = 0;
    for (char32_t c = 0; c <= 0xFFFF; ++c)
    {
        if (c < 0xD800 || c > 0xDFFF)
        {
            parsed += expectSingleCharacter(c) ? 1U : 0U;
        }
    }
    EXPECT_EQ(parsed, 550U);
}

// Digits of different scripts make one number: U+0661 ARABIC-INDIC DIGIT ONE, '2', U+0E53 THAI
// DIGIT THREE.
TEST(ExpressionParser, MixesTheDigitsOfScripts)
{
    EXPECT_EQ(valueOf("\xD9\xA1"
                      "2\xE0\xB9\x93"),
              123);
}

// ----------------------------------------------------------------------------------------------
// The quirks, one by one (the values are OpenRocket's, from the parity table)

// Digits of any script (Character.getNumericValue 0 to 9): fullwidth three U+FF13, Arabic-Indic
// three U+0663, circled one U+2460, Roman numeral four U+2163, and superscript one, fraction slash,
// subscript four (FractionalUnit's "1/4"). The vulgar fraction U+00BD, the Roman numeral ten
// U+2169 and the supplementary U+1D7CE MATHEMATICAL BOLD DIGIT ZERO are no digits.
TEST(ExpressionParser, TakesTheDigitsOfAnyScript)
{
    EXPECT_EQ(valueOf("\xEF\xBC\x93"), 3);
    EXPECT_EQ(valueOf("\xD9\xA3"), 3);
    EXPECT_EQ(valueOf("\xE2\x91\xA0"), 1);
    EXPECT_EQ(valueOf("\xE2\x85\xA3"), 4);
    EXPECT_EQ(valueOf("\xC2\xB9\xE2\x81\x84\xE2\x82\x84"), 0.25);
    EXPECT_EQ(valueOf("7 \xE2\x81\xB7\xE2\x81\x84\xE2\x82\x88"), 7.875);
    EXPECT_TRUE(isInvalid("\xC2\xBD"));
    EXPECT_TRUE(isInvalid("\xE2\x85\xA9"));
    EXPECT_TRUE(isInvalid("\xF0\x9D\x9F\x8E"));
}

// U+2044 FRACTION SLASH is '/'; U+2215 DIVISION SLASH and U+2212 MINUS SIGN are nothing (en_US).
TEST(ExpressionParser, TakesTheFractionSlashForSlash)
{
    EXPECT_EQ(valueOf("1\xE2\x81\x84"
                      "2"),
              0.5);
    EXPECT_TRUE(
        isInvalid("1\xE2\x88\x95"
                  "2"));
    EXPECT_TRUE(
        isInvalid("\xE2\x88\x92"
                  "3"));
}

// ',' and '.' are both decimal points, so neither separates thousands.
TEST(ExpressionParser, TakesCommaAndPointAsDecimalPoints)
{
    EXPECT_EQ(valueOf("1,5"), 1.5);
    EXPECT_EQ(valueOf("3,5*2"), 7);
    EXPECT_TRUE(isInvalid("1,000.5"));
    EXPECT_TRUE(isInvalid("1.000,5"));
}

// The mixed fraction "a b/c" is (a+b/c). The look-behind (no digit or point before a) and the
// look-ahead (none after c) are written out: "1.5 1/2" and "1 1/2.5" are no mixed fractions, and
// their space is excess. A second fraction after a replaced one stays a quotient.
TEST(ExpressionParser, ReadsMixedFractions)
{
    EXPECT_EQ(valueOf("1 3/4"), 1.75);
    EXPECT_EQ(valueOf("-1 1/2"), -1.5);
    EXPECT_EQ(valueOf("1  2 / 3"), 1 + (2.0 / 3.0));
    EXPECT_EQ(valueOf("12 3/4"), 12.75);
    EXPECT_EQ(valueOf("2*1 1/2"), 3);
    EXPECT_TRUE(isInvalid("1.5 1/2"));
    EXPECT_TRUE(isInvalid("1 1/2.5"));
    EXPECT_TRUE(isInvalid("1 1/2."));
    EXPECT_EQ(valueOf("1 1/2 3/4"), 0.75);
}

// Whitespace between two numbers makes the text invalid, unless a line terminator outside it
// stops the regular expression's '.': then whitespace is removed and the digits join.
TEST(ExpressionParser, RefusesExcessSpaceAsOpenRocketsRegexDoes)
{
    EXPECT_TRUE(isInvalid("1 2"));
    EXPECT_TRUE(isInvalid("1\t2"));
    EXPECT_TRUE(isInvalid("1\n2"));
    EXPECT_EQ(valueOf("1 2\n"), 12);
    EXPECT_EQ(valueOf("\r1 2"), 12);
    // U+2028 LINE SEPARATOR is a line terminator and whitespace; U+3000 IDEOGRAPHIC SPACE is
    // whitespace but not \s; U+00A0 NO-BREAK SPACE is neither.
    EXPECT_EQ(valueOf("1 2\xE2\x80\xA8"), 12);
    EXPECT_EQ(valueOf("1\xE3\x80\x80"
                      "2"),
              12);
    EXPECT_TRUE(
        isInvalid("1\xC2\xA0"
                  "2"));
    EXPECT_EQ(valueOf("2 * 3"), 6);
}

// + - * / % ^, % truncating as Java's remainder, ^ right-associative, a unary minus binding
// tighter than ^.
TEST(ExpressionParser, AppliesTheOperatorsAsExp4jDoes)
{
    EXPECT_EQ(valueOf("1+2*3"), 7);
    EXPECT_EQ(valueOf("8/2*4"), 16);
    EXPECT_EQ(valueOf("2-3-4"), -5);
    EXPECT_EQ(valueOf("7%4"), 3);
    EXPECT_EQ(valueOf("-7%4"), -3);
    EXPECT_EQ(valueOf("7%-4"), 3);
    EXPECT_NEAR(valueOf("2^3^2"), 512, 1e-12);
    EXPECT_NEAR(valueOf("-2^2"), 4, 1e-12);
    EXPECT_NEAR(valueOf("2^-1"), 0.5, 1e-12);
    EXPECT_EQ(valueOf("1--2"), 3);
    EXPECT_EQ(valueOf("1*-2"), -2);
    EXPECT_EQ(valueOf("+5"), 5);
    EXPECT_TRUE(isInvalid("3-"));
}

// A sign is unary only at the start, after '(' or after an operator: after leading whitespace,
// '[' or '{' it is binary and lacks its left operand.
TEST(ExpressionParser, TakesASignAsUnaryOnlyWhereExp4jDoes)
{
    EXPECT_EQ(valueOf("-5"), -5);
    EXPECT_EQ(valueOf("(-2)"), -2);
    EXPECT_TRUE(isInvalid(" -5"));
    EXPECT_TRUE(isInvalid(" +5"));
    EXPECT_TRUE(isInvalid("[-2]"));
    EXPECT_TRUE(isInvalid("{-2}"));
    EXPECT_TRUE(isInvalid("+-3"));
    EXPECT_EQ(valueOf("-+3"), -3);
}

// (), [] and {} are interchangeable, also with each other.
TEST(ExpressionParser, TakesAnyBracketForAnyOther)
{
    EXPECT_EQ(valueOf("[2]"), 2);
    EXPECT_EQ(valueOf("{3}"), 3);
    EXPECT_EQ(valueOf("(2]"), 2);
    EXPECT_EQ(valueOf("(1+[2*{3}])"), 7);
    EXPECT_TRUE(isInvalid("(2"));
    EXPECT_TRUE(isInvalid("2)"));
    EXPECT_TRUE(isInvalid("()"));
}

// The 20 functions of exp4j, in any case: each name in lower and upper case, on 0.5.
TEST(ExpressionParser, KnowsExp4jsTwentyFunctions)
{
    struct Function
    {
        std::string_view name;
        double           value;
    };
    const std::array<Function, 19> functions{{
        {.name = "abs", .value = 0.5},
        {.name = "acos", .value = std::acos(0.5)},
        {.name = "asin", .value = std::asin(0.5)},
        {.name = "atan", .value = std::atan(0.5)},
        {.name = "cbrt", .value = std::cbrt(0.5)},
        {.name = "ceil", .value = 1},
        {.name = "cos", .value = std::cos(0.5)},
        {.name = "cosh", .value = std::cosh(0.5)},
        {.name = "exp", .value = std::exp(0.5)},
        {.name = "expm1", .value = std::expm1(0.5)},
        {.name = "floor", .value = 0},
        {.name = "round", .value = 1},
        {.name = "log", .value = std::log(0.5)},
        {.name = "sin", .value = std::sin(0.5)},
        {.name = "sinh", .value = std::sinh(0.5)},
        {.name = "sqrt", .value = std::sqrt(0.5)},
        {.name = "tan", .value = std::tan(0.5)},
        {.name = "tanh", .value = std::tanh(0.5)},
        {.name = "log10", .value = std::log10(0.5)},
    }};
    for (const Function& function : functions)
    {
        // The expectations are computed here, perhaps at compile time: within a few ulp.
        EXPECT_DOUBLE_EQ(valueOf(std::string(function.name) + "(0.5)"), function.value)
            << function.name;
        EXPECT_DOUBLE_EQ(valueOf(QtRocket::Strings::toUpper(function.name) + "(0.5)"),
                         function.value)
            << function.name;
    }
    const double random = valueOf("Random(0.5)");
    EXPECT_GE(random, 0);
    EXPECT_LT(random, 0.5);
}

// The names compare as String.toUpperCase() spells them: U+017F LATIN SMALL LETTER LONG S is
// "S", U+FB02 LATIN SMALL LIGATURE FL is "FL", U+0131 LATIN SMALL LETTER DOTLESS I is "I";
// U+0163 is no ASCII letter in upper case, and U+212A KELVIN SIGN stays itself.
TEST(ExpressionParser, SpellsFunctionNamesAsJavasUpperCase)
{
    EXPECT_EQ(valueOf("\xC5\xBFqrt(16)"), 4);
    EXPECT_EQ(valueOf("\xEF\xAC\x82oor(2.5)"), 2);
    EXPECT_EQ(valueOf("s\xC4\xB1n(0)"), 0);
    EXPECT_TRUE(isInvalid("sqr\xC5\xA3(4)"));
    EXPECT_TRUE(isInvalid("\xE2\x84\xAAos(0)"));
}

// round is Math.round: ties up, NaN gives 0, the long range saturates.
TEST(ExpressionParser, RoundsAsJavasMathRound)
{
    EXPECT_EQ(valueOf("round(2.5)"), 3);
    EXPECT_EQ(valueOf("round(-2.5)"), -2);
    EXPECT_EQ(bitsOf(valueOf("round(-0.4)")), bitsOf(0.0));
    EXPECT_EQ(valueOf("round(0.49999999999999994)"), 0);
    EXPECT_EQ(valueOf("round(0/0)"), 0);
    EXPECT_EQ(valueOf("round(1/0)"), 9223372036854775807.0);
    EXPECT_EQ(valueOf("round(-1/0)"), -9223372036854775808.0);
}

// No constants, no units, no exponent notation, no Java number syntax beyond digits and a point.
TEST(ExpressionParser, KnowsNoConstantsUnitsOrExponents)
{
    EXPECT_TRUE(isInvalid("pi"));
    EXPECT_TRUE(isInvalid("e"));
    EXPECT_TRUE(isInvalid("5 cm"));
    EXPECT_TRUE(isInvalid("5cm"));
    EXPECT_TRUE(isInvalid("1e3"));
    EXPECT_TRUE(isInvalid("1E3"));
    EXPECT_TRUE(isInvalid("NaN"));
    EXPECT_TRUE(isInvalid("Infinity"));
    EXPECT_TRUE(isInvalid("0x10"));
    EXPECT_TRUE(isInvalid("1d"));
    EXPECT_TRUE(isInvalid("1.5.2"));
    EXPECT_TRUE(isInvalid("."));
    EXPECT_TRUE(isInvalid("max(1,2)"));
}

// The evaluation's result is the top of its stack; the values below it are dropped.
TEST(ExpressionParser, DropsWhatIsLeftBelowTheResult)
{
    EXPECT_EQ(valueOf("2(3)"), 3);
    EXPECT_EQ(valueOf("(2)(3)"), 3);
    EXPECT_EQ(valueOf("(2)3"), 3);
    EXPECT_EQ(valueOf("sqrt(16)(4)"), 2);
    EXPECT_EQ(valueOf("2sqrt(9)"), 3);
}

// "f(x)=" is dropped up to the first '=' after the first char; the name between its first '('
// and ')' is a variable without a value, invalid where it is used.
TEST(ExpressionParser, DropsAFunctionDefinitionAndItsVariable)
{
    EXPECT_EQ(valueOf("x=5"), 5);
    EXPECT_EQ(valueOf("f(x)=2*3"), 6);
    EXPECT_EQ(valueOf("1+2=5"), 5);
    EXPECT_TRUE(isInvalid("=5"));
    EXPECT_TRUE(isInvalid("a=b=5"));
    EXPECT_TRUE(isInvalid("f)(=5"));
    EXPECT_EQ(valueOf("f(x)=(x)(2)"), 2);
    EXPECT_TRUE(isInvalid("f(x)=(2)(x)"));
    EXPECT_TRUE(isInvalid("f(x)=x+1"));
    // A variable named like one of the Tokenizer's 17 functions is refused, round is not one.
    EXPECT_TRUE(isInvalid("f(sin)=5"));
    EXPECT_EQ(valueOf("f(round)=5"), 5);
    // A letter beyond ASCII names a variable too (U+00E9), U+00D7 MULTIPLICATION SIGN does not.
    EXPECT_EQ(valueOf("f(\xC3\xA9)=(\xC3\xA9)(3)"), 3);
    EXPECT_TRUE(isInvalid("f(\xC3\x97)=(\xC3\x97)(3)"));
}

// '#' is exp4j's own unary minus.
TEST(ExpressionParser, TakesHashForUnaryMinus)
{
    EXPECT_EQ(valueOf("#3"), -3);
    EXPECT_EQ(valueOf("2#3"), -3);
    EXPECT_EQ(valueOf("##3"), 3);
    EXPECT_TRUE(isInvalid("#"));
}

// Division by zero is no error: the caller clamps the value or stores it.
TEST(ExpressionParser, DividesByZeroAsJavaDoes)
{
    EXPECT_EQ(valueOf("1/0"), std::numeric_limits<double>::infinity());
    EXPECT_EQ(valueOf("-1/0"), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(valueOf("0/0")));
    EXPECT_TRUE(std::isnan(valueOf("1%0")));
    EXPECT_EQ(bitsOf(valueOf("-0")), bitsOf(-0.0));
}

// Numbers are read correctly rounded, whatever their length.
TEST(ExpressionParser, ReadsNumbersAsDoubleParseDouble)
{
    EXPECT_EQ(valueOf("9007199254740993"), 9007199254740992.0);
    EXPECT_EQ(valueOf("0.30000000000000004"), 0.30000000000000004);
    EXPECT_EQ(valueOf("3.141592653589793238462643383279"), std::numbers::pi);
    EXPECT_EQ(valueOf("1" + repeated("0", 308)), 1e308);
    EXPECT_EQ(valueOf("1" + repeated("0", 309)), std::numeric_limits<double>::infinity());
    EXPECT_EQ(valueOf("0." + repeated("0", 323) + "5"), std::numeric_limits<double>::denorm_min());
}

// random(x) is a random number from [0, 1) times x (not pinned: QtRocket's own generator).
TEST(ExpressionParser, DrawsRandomNumbersFromZeroToTheArgument)
{
    const std::vector<double> draws = drawRandom("random(1)", 64);
    expectRandomRange(draws, 1);
    // 64 equal draws of 53 random bits do not happen.
    EXPECT_GT(std::set<double>(draws.begin(), draws.end()).size(), 1U);
    expectRandomRange(drawRandom("random(-2)", 16), -2);
    EXPECT_EQ(bitsOf(valueOf("random(0)")), bitsOf(0.0));
    EXPECT_EQ(bitsOf(valueOf("random(-0)")), bitsOf(-0.0));
}

// The parser keeps no state but its generator, which is guarded: threads may parse at once.
TEST(ExpressionParser, ParsesOnSeveralThreadsAtOnce)
{
    std::array<double, 4>    sums{};
    std::vector<std::thread> threads;
    threads.reserve(sums.size());
    for (double& sum : sums)
    {
        threads.emplace_back([&sum] { sum = parseRepeatedly(200); });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    for (const double sum : sums)
    {
        EXPECT_EQ(sum, 600);
    }
}

// ----------------------------------------------------------------------------------------------
// Hostile input

// kMaxDepth brackets open at once are taken, one more is an error, where OpenRocket takes any
// depth (a deviation). The parity table holds 100 brackets of each kind, and texts of kMaxLength
// bytes of brackets.
TEST(ExpressionParser, LimitsTheDepthOfBrackets)
{
    EXPECT_EQ(ExpressionParser::kMaxDepth, 100U);
    EXPECT_EQ(valueOf(nested(ExpressionParser::kMaxDepth)), 1);
    const Result<double> deeper = ExpressionParser::parse(nested(ExpressionParser::kMaxDepth + 1));
    ASSERT_FALSE(deeper.has_value());
    EXPECT_EQ(deeper.error().code, ErrorCode::PARSE);
    EXPECT_NE(deeper.error().message.find("more than 100 brackets open at once"), std::string::npos)
        << deeper.error().message;
    EXPECT_TRUE(isInvalid(repeated("[", ExpressionParser::kMaxDepth + 1) + "1"));
    // Brackets that close are no deeper: 333 of them in a row.
    EXPECT_EQ(valueOf(repeated("(1)", 333)), 1);
}

// A text of kMaxLength bytes is read (the parity table holds several, with OpenRocket's values),
// a longer one is an error before anything is read, where OpenRocket takes any length (a
// deviation).
TEST(ExpressionParser, LimitsTheLengthOfTheText)
{
    EXPECT_EQ(ExpressionParser::kMaxLength, 1000U);
    const std::string sum = repeated("1+", 499) + "1 ";
    ASSERT_EQ(sum.size(), ExpressionParser::kMaxLength);
    EXPECT_EQ(valueOf(sum), 500);
    const Result<double> longer = ExpressionParser::parse(sum + " ");
    ASSERT_FALSE(longer.has_value());
    EXPECT_EQ(longer.error().code, ErrorCode::PARSE);
    EXPECT_EQ(longer.error().message, "Invalid expression: longer than 1000 bytes");
    EXPECT_TRUE(isInvalid(repeated("1", 1000000)));
}

// Text that is not UTF-8 is an error: a stray continuation byte, a byte that starts no sequence,
// an overlong encoding, an encoded surrogate, a code point beyond U+10FFFF, a truncated sequence.
TEST(ExpressionParser, RefusesInvalidUtf8)
{
    EXPECT_TRUE(isInvalid("1\x80"));
    EXPECT_TRUE(isInvalid("\xFF"));
    EXPECT_TRUE(isInvalid("\xC0\x80"));
    EXPECT_TRUE(isInvalid("\xED\xA0\x80"));
    EXPECT_TRUE(isInvalid("\xF4\x90\x80\x80"));
    EXPECT_TRUE(isInvalid("1\xE2\x81"));
    EXPECT_TRUE(isInvalid("x\xFE=5"));
}

// A NUL char is a char like any other, and no symbol of the language.
TEST(ExpressionParser, RefusesAnEmbeddedNul)
{
    EXPECT_TRUE(
        isInvalid(std::string_view("1\0"
                                   "2",
                                   3)));
    EXPECT_EQ(valueOf(std::string_view("\0=2", 3)), 2);
}

// Every failure says what was wrong and names the text.
TEST(ExpressionParser, SaysWhyATextIsInvalid)
{
    const Result<double> result = ExpressionParser::parse("1 2");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::PARSE);
    EXPECT_EQ(result.error().message,
              "Invalid expression: 1 2 (the expression contains excess space)");
    EXPECT_FALSE(ExpressionParser::parse("").has_value());
    EXPECT_FALSE(ExpressionParser::parse(" ").has_value());
}

}  // namespace
