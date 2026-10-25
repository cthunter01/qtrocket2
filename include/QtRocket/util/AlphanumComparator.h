#pragma once

#include <string_view>

namespace QtRocket
{

/// OpenRocket's util/AlphanumComparator (David Koelle's Alphanum algorithm): orders text with
/// numbers in it naturally, "file2" before "file10". OpenRocket's parts library sorts its Part
/// Number column with it.
///
/// Each text is cut into chunks, a chunk being a maximal run of ASCII digits or a maximal run of
/// other characters, and the chunks are compared pair by pair: two digit chunks by their length,
/// then digit by digit (so "2" < "10" and "2" < "002": no numeric value, leading zeros make a
/// number longer), any other two chunks by the tertiary Java collator (JavaCollator::compare).
/// The first difference decides; when one text runs out of chunks, the shorter text (in UTF-16
/// code units, javaLength()) sorts first.
///
/// Deviation: OpenRocket's collator decomposes the text canonically (NFD) before it compares;
/// this one does not. The two agree on every text that NFD leaves as it is, which includes all
/// ASCII text: every part number of OpenRocket's 31 bundled .orc files (5235 PartNumber elements)
/// is ASCII. A user's part number can sort differently when it holds a precomposed Greek,
/// Cyrillic or Hangul letter (which the rules leave out but NFD decomposes: U+03AC against U+03B1
/// gives 1 in OpenRocket, -1 here), one of the 13 Latin letters whose decomposition the rules
/// weigh differently (U+01D7, U+01D8, U+01E2, U+01E3, U+01EE, U+01EF, U+01FC to U+01FF, U+1E2E,
/// U+1E2F, U+1E9B), or two combining marks that NFD reorders. Every other accented Latin letter
/// already has its decomposition's elements.
class AlphanumComparator
{
public:
    /// AlphanumComparator.compare(s1, s2): negative when @p s1 sorts before @p s2, positive when
    /// it sorts after, 0 when they are equal. The values are Java's: the difference of the
    /// lengths of two digit chunks, or of their first differing digits, the collator's -1 or 1,
    /// or the difference of the texts' lengths in UTF-16 code units (clamped to int).
    [[nodiscard]] static int compare(std::string_view s1, std::string_view s2);

    /// compare(a, b) < 0, for the standard algorithms (std::ranges::stable_sort keeps equal texts
    /// in their order, as Java's sorts do).
    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const
    {
        return compare(a, b) < 0;
    }
};

}  // namespace QtRocket
