#include "QtRocket/util/JavaCollator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Strings.h"
#include "util/JavaCollatorPins.h"
#include "util/RandomTextPairs.h"

namespace
{

namespace JavaCollator = QtRocket::JavaCollator;
using JavaCollator::Strength;
using QtRocket::Test::RandomTextPairs;

constexpr std::array<Strength, 3> kStrengths{Strength::PRIMARY, Strength::SECONDARY,
                                             Strength::TERTIARY};

/// Java's answers at the three strengths, decoded from one character of the corpus.
[[nodiscard]] std::array<int, 3> decodeResults(char code)
{
    const std::size_t index = QtRocket::Test::kJavaCollatorResultCodes.find(code);
    EXPECT_NE(index, std::string_view::npos) << code;
    const auto value = static_cast<int>(index);
    return {(value / 9) - 1, ((value / 3) % 3) - 1, (value % 3) - 1};
}

/// Checks the answers of the collator at the three strengths, and of
/// Strings::javaPrimaryCollatorCompare(), for one pinned case.
void expectJavasAnswers(const QtRocket::Test::JavaCollatorCase& pin)
{
    const std::string described =
        RandomTextPairs::describe(pin.first) + " vs " + RandomTextPairs::describe(pin.second);
    EXPECT_EQ(JavaCollator::compare(pin.first, pin.second, Strength::PRIMARY), pin.primary)
        << described;
    EXPECT_EQ(JavaCollator::compare(pin.first, pin.second, Strength::SECONDARY), pin.secondary)
        << described;
    EXPECT_EQ(JavaCollator::compare(pin.first, pin.second, Strength::TERTIARY), pin.tertiary)
        << described;
    EXPECT_EQ(JavaCollator::compare(pin.first, pin.second), pin.tertiary) << described;
    EXPECT_EQ(QtRocket::Strings::javaPrimaryCollatorCompare(pin.first, pin.second), pin.primary)
        << described;
}

/// The number of answers for the pair (@p first, @p second), the corpus's pair @p index, that
/// differ from Java's (@p code): the collator's at the three strengths and
/// Strings::javaPrimaryCollatorCompare()'s. Reports the first few of the whole corpus, of which
/// @p reported have been.
[[nodiscard]] std::size_t countMismatches(const std::string& first, const std::string& second,
                                          char code, std::size_t index, std::size_t reported)
{
    const std::array<int, 3> pinned = decodeResults(code);
    const std::array<int, 4> answers{
        JavaCollator::compare(first, second, Strength::PRIMARY),
        JavaCollator::compare(first, second, Strength::SECONDARY),
        JavaCollator::compare(first, second, Strength::TERTIARY),
        QtRocket::Strings::javaPrimaryCollatorCompare(first, second),
    };
    const std::array<int, 4> java{pinned.at(0), pinned.at(1), pinned.at(2), pinned.at(0)};
    std::size_t              mismatches = 0;
    for (std::size_t i = 0; i < answers.size(); i++)
    {
        if (answers.at(i) != java.at(i) && reported + mismatches++ < 10)
        {
            ADD_FAILURE() << "pair " << index << ", check " << i << ": "
                          << RandomTextPairs::describe(first) << " vs "
                          << RandomTextPairs::describe(second) << " gives " << answers.at(i)
                          << ", Java " << java.at(i);
        }
    }
    return mismatches;
}

TEST(JavaCollator, GivesJavasAnswersForTheCases)
{
    // Pinned from JDK 17 (tests/core/util/JavaCollatorPins.h): the scouts' pairs of the motor
    // database and the parts library, and the corners of the rules.
    for (const QtRocket::Test::JavaCollatorCase& pin : QtRocket::Test::kJavaCollatorCases)
    {
        expectJavasAnswers(pin);
    }
}

TEST(JavaCollator, GivesJavasAnswersForTheRandomCorpus)
{
    // The corpus of tools/openrocket-collation/JavaCollatorPins.java: random texts of ignorable
    // characters, spaces, accents, hyphens, punctuation, digits, letters, expansions, unmapped
    // characters and code points above U+FFFF, most pairs a text and an edited copy.
    RandomTextPairs pairs(QtRocket::Test::kJavaCollatorCorpusSeed,
                          QtRocket::Test::kJavaCollatorCorpusAlphabet,
                          QtRocket::Test::kJavaCollatorCorpusMaxLength);
    std::size_t     compared   = 0;
    std::size_t     mismatches = 0;
    for (const std::string_view line : QtRocket::Test::kJavaCollatorCorpusResults)
    {
        for (const char code : line)
        {
            const auto [first, second] = pairs.next();
            mismatches += countMismatches(first, second, code, compared, mismatches);
            compared++;
        }
    }
    EXPECT_EQ(compared, 30000U);
    EXPECT_EQ(mismatches, 0U);
}

TEST(JavaCollator, ReadsAMalformedByteAsTheReplacementCharacter)
{
    // Java's UTF-8 decoder reads each byte that starts no well-formed sequence as U+FFFD.
    for (const Strength strength : kStrengths)
    {
        EXPECT_EQ(JavaCollator::compare("\xFF", "\uFFFD", strength), 0);
        EXPECT_EQ(JavaCollator::compare("a\xC3", "a\uFFFD", strength), 0);
        EXPECT_EQ(JavaCollator::compare("\xE2\x82", "\uFFFD\uFFFD", strength), 0);
        EXPECT_EQ(JavaCollator::compare("\xFF", "\uFFFD\uFFFD", strength), -1);
    }
}

TEST(JavaCollator, SortsAsJava)
{
    // Java's sort of these texts with Collator.getInstance() (pinned from JDK 17): a lower-case
    // letter before its capital, an accent after both, a space or a hyphen after that, and a
    // primary difference over everything.
    const std::vector<std::string> expected{
        "",    "a",   "A", "\u00E1", "\u00C1", "ab",        "aB",         "Ab",           "AB",
        "a b", "a-b", "b", "B",      "cote",   "c\u00F4te", "Pro98-6GXL", "Pro-98 6g XL",
    };
    std::vector<std::string> sorted{
        "Pro-98 6g XL", "AB",   "c\u00F4te", "\u00C1", "a-b", "b",
        "Pro98-6GXL",   "Ab",   "a",         "B",      "",    "a b",
        "aB",           "cote", "ab",        "\u00E1", "A",
    };
    std::ranges::stable_sort(sorted, [](const std::string& a, const std::string& b) {
        return JavaCollator::compare(a, b) < 0;
    });
    EXPECT_EQ(sorted, expected);
}

}  // namespace
