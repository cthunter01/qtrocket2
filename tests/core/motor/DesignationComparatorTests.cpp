#include "QtRocket/motor/DesignationComparator.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::DesignationComparator;

int compare(std::string_view a, std::string_view b)
{
    return DesignationComparator::compare(a, b);
}

// ---- Orders and values pinned by running OpenRocket's DesignationComparator ----

TEST(DesignationComparator, SortMatchesOpenRocket)
{
    std::vector<std::string> designations{
        "F12",       "1/2A3",       "1/4A3",  "A8",     "a8",     "A10",   "B6",
        "132G100",   "132-G80",     "G80",    "G80-10", "G80T",   "G80W",  "Micro Maxx II",
        "RCS 18/20", "1,000K1,000", "K1,000", "K999",   "1/2A3T", "1/8A1", "1/9A1",
        "H128W",     "h128",        "A",      "",       "-",      "A8\n",  "A8-3\nx",
        "A8-3\n",    "A8\r\n",      "A8 ",    "2/2A3"};
    // OpenRocket sorts with List.sort, a stable merge sort.
    std::ranges::stable_sort(designations, DesignationComparator{});

    const std::vector<std::string> expected{"1/8A1", "1/4A3",       "1/2A3",         "1/2A3T",
                                            "A8",    "a8",          "A8\n",          "A8\r\n",
                                            "A8 ",   "A8-3\n",      "A10",           "B6",
                                            "F12",   "132-G80",     "G80",           "G80-10",
                                            "G80T",  "G80W",        "132G100",       "h128",
                                            "H128W", "K999",        "K1,000",        "",
                                            "-",     "1,000K1,000", "1/9A1",         "2/2A3",
                                            "A",     "A8-3\nx",     "Micro Maxx II", "RCS 18/20"};
    EXPECT_EQ(designations, expected);
}

TEST(DesignationComparator, ComparisonsMatchOpenRocket)
{
    // The thrust difference itself comes back for designations of one class.
    EXPECT_EQ(compare("G80", "G12"), 68);
    EXPECT_EQ(compare("G12", "G80"), -68);
    // 1/2A before A, 1/4A before 1/2A.
    EXPECT_EQ(compare("1/2A3", "A3"), -1);
    EXPECT_EQ(compare("1/4A3", "1/2A3"), -1);
    EXPECT_EQ(compare("A3", "1/2A3"), 1);
    // The extras compare by the collator, which ignores the hyphen: "10" before "4".
    EXPECT_EQ(compare("G80-10", "G80-4"), -1);
    // Case is ignored.
    EXPECT_EQ(compare("G80T", "g80t"), 0);
    // A designation of the usual form sorts before one that is not.
    EXPECT_EQ(compare("F12", "Micro Maxx II"), -1);
    EXPECT_EQ(compare("Micro", "Maxx"), 1);
    // Commas in the thrust are ignored; a number before the class is a prefix.
    EXPECT_EQ(compare("K1,000", "K999"), 1);
    EXPECT_EQ(compare("1,000K1,000", "K1000"), 1);
    // A single line break at the end is allowed; one inside defeats the form.
    EXPECT_EQ(compare("A8\n", "A8"), 0);
    EXPECT_EQ(compare("A8-3\nx", "A8"), 1);
    EXPECT_EQ(compare("A8-3\n", "A8-3"), 0);
}

TEST(DesignationComparator, UnparsableThrustCountsAsNotMatching)
{
    // OpenRocket's Integer.parseInt throws for these; here they sort as non-designations.
    EXPECT_EQ(compare("A,", "A1"), 1);
    EXPECT_EQ(compare("A1", "A,"), -1);
    EXPECT_EQ(compare("K99999999999", "K1"), 1);
    // The largest int still parses.
    EXPECT_GT(compare("K2147483647", "K1"), 0);
    EXPECT_LT(compare("K2147483647", "A"), 0);
}

TEST(DesignationComparator, FractionsOfTheAClass)
{
    // 1/8A < 1/4A < 1/2A < A, whatever the thrust.
    EXPECT_LT(compare("1/8A9", "1/4A1"), 0);
    EXPECT_LT(compare("1/4A9", "1/2A1"), 0);
    EXPECT_LT(compare("1/2A9", "A1"), 0);
    // The fraction only matters within the A class.
    EXPECT_LT(compare("1/2A3", "B1"), 0);
    // "1/9" is no fraction the pattern accepts, and "2/2" neither.
    EXPECT_GT(compare("1/9A1", "Z99"), 0);
    EXPECT_GT(compare("2/2A3", "Z99"), 0);
    // A lower-case a is the A class too.
    EXPECT_LT(compare("1/2a3", "a3"), 0);
}

TEST(DesignationComparator, OtherLineTerminators)
{
    // \r, \r\n, U+0085, U+2028 and U+2029 end a line as \n does.
    EXPECT_EQ(compare("A8\r", "A8"), 0);
    EXPECT_EQ(compare("A8\r\n", "A8"), 0);
    EXPECT_EQ(compare("A8\xC2\x85", "A8"), 0);
    EXPECT_EQ(compare("A8\xE2\x80\xA8", "A8"), 0);
    EXPECT_EQ(compare("A8\xE2\x80\xA9", "A8"), 0);
    // Two of them, or one that is not last, defeat the form.
    EXPECT_EQ(compare("A8\n\n", "A8"), 1);
    EXPECT_EQ(compare("A8\n\r", "A8"), 1);
    EXPECT_EQ(compare("A8\rX", "A8"), 1);
}

TEST(DesignationComparator, PrefixForms)
{
    // Total impulse prefixes, with or without a hyphen, do not affect the order.
    EXPECT_EQ(compare("132G100", "G100"), 0);
    EXPECT_EQ(compare("132-G100", "G100"), 0);
    EXPECT_EQ(compare("132-G100", "999G100"), 0);
    // A prefix without a class letter after it does not match.
    EXPECT_GT(compare("132", "G100"), 0);
    EXPECT_GT(compare("132-", "G100"), 0);
    EXPECT_GT(compare("G", "G100"), 0);
}

TEST(DesignationComparator, StrictWeakOrderingOperator)
{
    const DesignationComparator less;
    EXPECT_TRUE(less("A8", "B6"));
    EXPECT_FALSE(less("B6", "A8"));
    EXPECT_FALSE(less("G80T", "g80t"));
    EXPECT_FALSE(less("g80t", "G80T"));
}

}  // namespace
