#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace QtRocket::Test
{

/// The random pairs of texts of the corpora that tools/openrocket-collation pins
/// (JavaCollatorPins.java, AlphanumComparatorPins.java), made again from their seed exactly as the
/// generator makes them: SplitMix64 numbers, n drawn below a bound as (number >>> 1) % bound, a
/// text of 0 to maxLength characters of the alphabet, and for three pairs in four the second text
/// a copy of the first with one to three edits (replace, insert or remove a character).
class RandomTextPairs
{
public:
    /// The pairs of the corpus with @p seed over @p alphabet (which must outlive this object),
    /// texts of at most @p maxLength characters.
    RandomTextPairs(std::uint64_t seed, std::span<const char32_t> alphabet, std::size_t maxLength);

    /// The next pair of texts, as UTF-8.
    [[nodiscard]] std::pair<std::string, std::string> next();

    /// The code points of UTF-8 @p text for a failure message: "[U+0061 U+0301]".
    [[nodiscard]] static std::string describe(std::string_view text);

private:
    [[nodiscard]] std::uint64_t  nextNumber();
    [[nodiscard]] std::size_t    below(std::size_t bound);
    [[nodiscard]] char32_t       randomCharacter();
    [[nodiscard]] std::u32string randomText();
    [[nodiscard]] std::u32string edited(std::u32string text);

    std::uint64_t             m_state;
    std::span<const char32_t> m_alphabet;
    std::size_t               m_maxLength;
};

}  // namespace QtRocket::Test
