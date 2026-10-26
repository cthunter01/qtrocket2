#include "QtRocket/util/AlphanumComparator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "goldens/GoldenData.h"
#include "util/AlphanumComparatorPins.h"
#include "util/RandomTextPairs.h"

namespace
{

using QtRocket::AlphanumComparator;
using QtRocket::Test::RandomTextPairs;

// Port of AlphanumComparatorTest.sortsNumericSuffixesInNaturalOrder (core
// util/AlphanumComparatorTest.java).
TEST(AlphanumComparator, SortsNumericSuffixesInNaturalOrder)
{
    std::vector<std::string> values{"file2", "file10", "file1"};
    std::ranges::stable_sort(values, AlphanumComparator());

    EXPECT_EQ(values, (std::vector<std::string>{"file1", "file2", "file10"}));
}

// Port of AlphanumComparatorTest.compareHandlesInterleavedNumbersAndLetters.
TEST(AlphanumComparator, CompareHandlesInterleavedNumbersAndLetters)
{
    EXPECT_LT(AlphanumComparator::compare("img2.png", "img12.png"), 0);
    EXPECT_LT(AlphanumComparator::compare("section9b", "section10a"), 0);
    EXPECT_LT(AlphanumComparator::compare("chapter10", "chapter10a"), 0);
}

// Port of AlphanumComparatorTest.alphabeticalChunksFollowCollatorOrdering.
TEST(AlphanumComparator, AlphabeticalChunksFollowCollatorOrdering)
{
    EXPECT_LT(AlphanumComparator::compare("alpha", "beta"), 0);
    EXPECT_EQ(AlphanumComparator::compare("gamma", "gamma"), 0);
}

// Port of AlphanumComparatorTest.leadingZerosOrderByChunkLengthFirst.
TEST(AlphanumComparator, LeadingZerosOrderByChunkLengthFirst)
{
    EXPECT_LT(AlphanumComparator::compare("item2", "item002"), 0);
    EXPECT_GT(AlphanumComparator::compare("item002", "item2"), 0);
    EXPECT_LT(AlphanumComparator::compare("item01", "item002"), 0);
}

TEST(AlphanumComparator, GivesJavasAnswersForTheCases)
{
    // OpenRocket's answers, the ints as they are (tests/core/util/AlphanumComparatorPins.h): the
    // cases of AlphanumComparatorTest, part numbers, numbers, empty texts and UTF-16 lengths.
    for (const QtRocket::Test::AlphanumCase& pin : QtRocket::Test::kAlphanumCases)
    {
        EXPECT_EQ(AlphanumComparator::compare(pin.first, pin.second), pin.result)
            << RandomTextPairs::describe(pin.first) << " vs "
            << RandomTextPairs::describe(pin.second);
    }
}

TEST(AlphanumComparator, GivesJavasAnswersForTheRandomCorpus)
{
    // The corpus of tools/openrocket-collation/AlphanumComparatorPins.java: texts of digits,
    // part-number letters, punctuation and a few characters beyond ASCII that NFD leaves alone.
    RandomTextPairs pairs(QtRocket::Test::kAlphanumCorpusSeed,
                          QtRocket::Test::kAlphanumCorpusAlphabet,
                          QtRocket::Test::kAlphanumCorpusMaxLength);
    std::size_t     compared   = 0;
    std::size_t     mismatches = 0;
    for (const std::string_view line : QtRocket::Test::kAlphanumCorpusResults)
    {
        for (const char code : line)
        {
            const auto [first, second] = pairs.next();
            const std::size_t index    = QtRocket::Test::kAlphanumResultCodes.find(code);
            ASSERT_NE(index, std::string_view::npos) << code;
            const int pinned = static_cast<int>(index) - QtRocket::Test::kAlphanumResultOffset;
            const int answer = AlphanumComparator::compare(first, second);
            if (answer != pinned && ++mismatches <= 10)
            {
                ADD_FAILURE() << "pair " << compared << ": " << RandomTextPairs::describe(first)
                              << " vs " << RandomTextPairs::describe(second) << " gives " << answer
                              << ", Java " << pinned;
            }
            compared++;
        }
    }
    EXPECT_EQ(compared, 20000U);
    EXPECT_EQ(mismatches, 0U);
}

TEST(AlphanumComparator, DeviatesFromJavaWhereNfdChangesTheText)
{
    // The documented deviation: OpenRocket's collator decomposes (NFD), QtRocket's does not.
    // Greek alpha with tonos is unmapped and sorts before alpha here; NFD makes it alpha and an
    // accent there, after alpha. U+01D7 is U, diaeresis, acute here and U with the contraction
    // of U+0308 U+0301 there: the collator decides here, the lengths there.
    const std::array<int, 2> qtRocket{-1, -1};
    ASSERT_EQ(QtRocket::Test::kAlphanumDeviationCases.size(), qtRocket.size());
    for (std::size_t i = 0; i < qtRocket.size(); i++)
    {
        const QtRocket::Test::AlphanumCase& pin = QtRocket::Test::kAlphanumDeviationCases.at(i);
        EXPECT_EQ(AlphanumComparator::compare(pin.first, pin.second), qtRocket.at(i))
            << RandomTextPairs::describe(pin.first) << " vs "
            << RandomTextPairs::describe(pin.second) << " (Java " << pin.result << ")";
    }
    EXPECT_EQ(QtRocket::Test::kAlphanumDeviationCases.at(0).result, 1);
    EXPECT_EQ(QtRocket::Test::kAlphanumDeviationCases.at(1).result, -2);
}

TEST(AlphanumComparatorPinsGolden, ComeFromTheOpenRocketOfTheGoldens)
{
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(QtRocket::Test::kAlphanumPinsCommit, manifest->openrocketCommit);
}

}  // namespace
