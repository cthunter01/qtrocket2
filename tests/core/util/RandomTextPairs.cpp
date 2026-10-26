#include "util/RandomTextPairs.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/util/Strings.h"

namespace QtRocket::Test
{

RandomTextPairs::RandomTextPairs(std::uint64_t seed, std::span<const char32_t> alphabet,
                                 std::size_t maxLength)
  : m_state(seed), m_alphabet(alphabet), m_maxLength(maxLength)
{
}

std::pair<std::string, std::string> RandomTextPairs::next()
{
    const std::u32string first  = randomText();
    const std::u32string second = below(4) == 0 ? randomText() : edited(first);
    return {Strings::fromCodePoints(first), Strings::fromCodePoints(second)};
}

std::string RandomTextPairs::describe(std::string_view text)
{
    std::string described = "[";
    for (const char32_t codePoint : Strings::toCodePoints(text))
    {
        described += std::format("{}U+{:04X}", described.size() == 1 ? "" : " ",
                                 static_cast<std::uint32_t>(codePoint));
    }
    return described + "]";
}

std::uint64_t RandomTextPairs::nextNumber()
{
    // SplitMix64, as the generator's nextLong() computes it on Java's wrapping longs.
    m_state += 0x9E3779B97F4A7C15U;
    std::uint64_t z = m_state;
    z               = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9U;
    z               = (z ^ (z >> 27U)) * 0x94D049BB133111EBU;
    return z ^ (z >> 31U);
}

std::size_t RandomTextPairs::below(std::size_t bound)
{
    return (nextNumber() >> 1U) % bound;
}

char32_t RandomTextPairs::randomCharacter()
{
    return m_alphabet[below(m_alphabet.size())];
}

std::u32string RandomTextPairs::randomText()
{
    const std::size_t length = below(m_maxLength + 1);
    std::u32string    text;
    for (std::size_t i = 0; i < length; i++)
    {
        text.push_back(randomCharacter());
    }
    return text;
}

std::u32string RandomTextPairs::edited(std::u32string text)
{
    const std::size_t edits = 1 + below(3);
    for (std::size_t i = 0; i < edits; i++)
    {
        // The draws in the generator's order: the kind, then the place, then the character.
        const std::size_t kind = below(3);
        if (kind == 0 && !text.empty())
        {
            const std::size_t at = below(text.size());
            text[at]             = randomCharacter();
        }
        else if (kind == 1)
        {
            const std::size_t at = below(text.size() + 1);
            text.insert(at, 1, randomCharacter());
        }
        else if (kind == 2 && !text.empty())
        {
            text.erase(below(text.size()), 1);
        }
    }
    return text;
}

}  // namespace QtRocket::Test
