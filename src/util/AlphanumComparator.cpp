#include "QtRocket/util/AlphanumComparator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "QtRocket/util/JavaCollator.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// isDigit(): an ASCII digit only. No byte of a multi-byte UTF-8 sequence is one, so the chunks
/// of the UTF-8 text are those of Java's UTF-16 String.
[[nodiscard]] constexpr bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// getChunk(): the chunk of @p text that starts at @p marker, a run of digits or of other
/// characters (@p marker must be below text.size()).
[[nodiscard]] std::string_view chunkAt(std::string_view text, std::size_t marker) noexcept
{
    const bool  digits = isDigit(text[marker]);
    std::size_t end    = marker + 1;
    while (end < text.size() && isDigit(text[end]) == digits)
    {
        end++;
    }
    return text.substr(marker, end - marker);
}

/// a - b, clamped to int (Java subtracts the lengths of Strings, which hold at most 2^31 - 1
/// chars).
[[nodiscard]] int clampedDifference(std::size_t a, std::size_t b) noexcept
{
    const std::int64_t difference = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
    return static_cast<int>(std::clamp<std::int64_t>(difference, std::numeric_limits<int>::min(),
                                                     std::numeric_limits<int>::max()));
}

/// Two chunks of digits: the difference of their lengths, else of their first differing digits,
/// else 0.
[[nodiscard]] int compareDigitChunks(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return clampedDifference(a.size(), b.size());
    }
    for (std::size_t i = 0; i < a.size(); i++)
    {
        if (a[i] != b[i])
        {
            return static_cast<int>(a[i]) - static_cast<int>(b[i]);
        }
    }
    return 0;
}

}  // namespace

int AlphanumComparator::compare(std::string_view s1, std::string_view s2)
{
    std::size_t thisMarker = 0;
    std::size_t thatMarker = 0;
    while (thisMarker < s1.size() && thatMarker < s2.size())
    {
        const std::string_view thisChunk = chunkAt(s1, thisMarker);
        thisMarker += thisChunk.size();
        const std::string_view thatChunk = chunkAt(s2, thatMarker);
        thatMarker += thatChunk.size();

        // Two chunks of digits compare by their numbers, any other two by the collator.
        const int result = isDigit(thisChunk.front()) && isDigit(thatChunk.front())
                               ? compareDigitChunks(thisChunk, thatChunk)
                               : JavaCollator::compare(thisChunk, thatChunk);
        if (result != 0)
        {
            return result;
        }
    }
    // s1Length - s2Length: the lengths in UTF-16 code units.
    return clampedDifference(Strings::javaLength(s1), Strings::javaLength(s2));
}

}  // namespace QtRocket
