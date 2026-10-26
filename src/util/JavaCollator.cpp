#include "QtRocket/util/JavaCollator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>

#include "JavaCollationTables.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::JavaCollator
{

namespace
{

/// The bits of an element that count at each strength, the elements being compared masked to
/// them: the primary weight, then the secondary byte, then the tertiary byte.
constexpr std::uint32_t kPrimaryBits   = 0xFFFF0000U;
constexpr std::uint32_t kSecondaryBits = 0xFFFFFF00U;
constexpr std::uint32_t kTertiaryBits  = 0xFFFFFFFFU;

/// The first code point above the Basic Multilingual Plane.
constexpr char32_t kFirstSupplementary = 0x10000;

/// Planes 4, 8, 12 and 16 repeat the mapped chars of plane 0: the bits that select the char.
constexpr std::uint32_t kAliasBits = 0x3FFFFU;

[[nodiscard]] constexpr std::uint32_t primaryOf(std::uint32_t element) noexcept
{
    return element >> 16U;
}

[[nodiscard]] constexpr std::uint32_t secondaryOf(std::uint32_t element) noexcept
{
    return (element >> 8U) & 0xFFU;
}

[[nodiscard]] constexpr std::uint32_t tertiaryOf(std::uint32_t element) noexcept
{
    return element & 0xFFU;
}

/// -1 when @p a is below @p b, else 1 (the two differ).
[[nodiscard]] constexpr int signOfDifference(std::uint32_t a, std::uint32_t b) noexcept
{
    return a < b ? -1 : 1;
}

[[nodiscard]] constexpr std::uint32_t bitsOf(Strength strength) noexcept
{
    switch (strength)
    {
        case Strength::PRIMARY:
            return kPrimaryBits;
        case Strength::SECONDARY:
            return kSecondaryBits;
        case Strength::TERTIARY:
            break;
    }
    return kTertiaryBits;
}

/// The run of the rules that maps @p unit, or nullptr when the rules do not map it.
[[nodiscard]] const Detail::JavaCollationRun* findRun(std::uint32_t unit) noexcept
{
    // The number of runs that start at or before the unit; an index, not an iterator, since
    // std::array's iterator is a pointer with libstdc++ and libc++ but a class with MSVC.
    const auto runsBefore = static_cast<std::size_t>(
        std::distance(Detail::kJavaCollationRuns.begin(),
                      std::ranges::upper_bound(Detail::kJavaCollationRuns, unit, {},
                                               [](const Detail::JavaCollationRun& run) {
                                                   return static_cast<std::uint32_t>(run.first);
                                               })));
    if (runsBefore == 0)
    {
        return nullptr;
    }
    const Detail::JavaCollationRun& run = Detail::kJavaCollationRuns.at(runsBefore - 1);
    return unit <= static_cast<std::uint32_t>(run.last) ? &run : nullptr;
}

/// The contraction that the code points @p first and @p second make, or nullptr when they make
/// none.
[[nodiscard]] const Detail::JavaCollationContraction* findContraction(std::uint32_t first,
                                                                      std::uint32_t second) noexcept
{
    for (const Detail::JavaCollationContraction& contraction : Detail::kJavaCollationContractions)
    {
        if (static_cast<std::uint32_t>(contraction.first) == first &&
            static_cast<std::uint32_t>(contraction.second) == second)
        {
            return &contraction;
        }
    }
    return nullptr;
}

/// Whether some contraction starts with the code point @p first.
[[nodiscard]] bool startsContraction(std::uint32_t first) noexcept
{
    return std::ranges::any_of(Detail::kJavaCollationContractions,
                               [first](const Detail::JavaCollationContraction& contraction) {
                                   return static_cast<std::uint32_t>(contraction.first) == first;
                               });
}

/// The collation elements of UTF-8 text one at a time, masked to the strength, as
/// CollationElementIterator.next() returns them (an ignorable char's 0 included).
class ElementReader
{
public:
    ElementReader(std::string_view text, std::uint32_t bits) noexcept : m_text(text), m_bits(bits)
    {
    }

    /// The next element, or std::nullopt after the last.
    [[nodiscard]] std::optional<std::uint32_t> next() noexcept
    {
        if (m_pendingIndex == m_pendingCount)
        {
            if (m_position >= m_text.size())
            {
                return std::nullopt;
            }
            readCharacter();
        }
        return m_pending.at(m_pendingIndex++) & m_bits;
    }

private:
    /// Room for the elements of a char or a contraction, and for the three of an unmapped code
    /// point above U+FFFF.
    static constexpr std::size_t kCapacity =
        std::max<std::size_t>(Detail::kJavaMaxCollationElements, 3);

    /// Makes the elements of the next char (or contraction) of the text the pending ones.
    void readCharacter() noexcept
    {
        const char32_t codePoint = Strings::nextCodePoint(m_text, m_position);
        const auto     value     = static_cast<std::uint32_t>(codePoint);
        if (startsContraction(value) && m_position < m_text.size())
        {
            std::size_t after  = m_position;
            const auto  second = static_cast<std::uint32_t>(Strings::nextCodePoint(m_text, after));
            if (const Detail::JavaCollationContraction* contraction =
                    findContraction(value, second))
            {
                m_position = after;
                setPending(contraction->offset, contraction->count);
                return;
            }
        }
        // A code point above U+FFFF whose low 18 bits are a mapped char of the Basic Multilingual
        // Plane gets that char's elements (but never starts a contraction).
        const std::uint32_t unit = codePoint < kFirstSupplementary ? value : value & kAliasBits;
        if (const Detail::JavaCollationRun* run = findRun(unit))
        {
            setPending(run->offset, run->count);
            return;
        }
        m_pendingIndex = 0;
        if (codePoint < kFirstSupplementary)
        {
            m_pending      = {Detail::kJavaUnmappedOrder, value << 16U};
            m_pendingCount = 2;
            return;
        }
        // An unmapped code point above U+FFFF: the marker, then its surrogates.
        const std::uint32_t offset = value - static_cast<std::uint32_t>(kFirstSupplementary);
        m_pending      = {Detail::kJavaUnmappedOrder, (0xD800U + (offset >> 10U)) << 16U,
                          (0xDC00U + (offset & 0x3FFU)) << 16U};
        m_pendingCount = 3;
    }

    /// Makes @p count elements of the table from @p offset the pending ones.
    void setPending(std::size_t offset, std::size_t count) noexcept
    {
        for (std::size_t i = 0; i < count; i++)
        {
            m_pending.at(i) = Detail::kJavaCollationElements.at(offset + i);
        }
        m_pendingIndex = 0;
        m_pendingCount = count;
    }

    std::string_view                     m_text;
    std::size_t                          m_position{0};
    std::uint32_t                        m_bits;
    std::array<std::uint32_t, kCapacity> m_pending{};
    std::size_t                          m_pendingCount{0};
    std::size_t                          m_pendingIndex{0};
};

/// One comparison of a source string (a) with a target string (b), the elements of both read in
/// step.
class Comparison
{
public:
    Comparison(std::string_view source, std::string_view target, Strength strength) noexcept
      : m_source(source, bitsOf(strength)),
        m_target(target, bitsOf(strength)),
        m_secondaryOpen(strength != Strength::PRIMARY),
        m_tertiaryOpen(strength == Strength::TERTIARY)
    {
    }

    [[nodiscard]] int run() noexcept
    {
        std::optional<std::uint32_t> source = m_source.next();
        std::optional<std::uint32_t> target = m_target.next();
        while (source.has_value() && target.has_value())
        {
            const Step step = compareElements(*source, *target);
            if (step.decided)
            {
                return step.result;
            }
            if (step.advanceSource)
            {
                source = m_source.next();
            }
            if (step.advanceTarget)
            {
                target = m_target.next();
            }
        }
        if (source.has_value())
        {
            return finishRest(*source, m_source, 1);
        }
        if (target.has_value())
        {
            return finishRest(*target, m_target, -1);
        }
        return m_result;
    }

private:
    /// What one step decides: the result when a primary difference ends the comparison, or which
    /// of the two strings moves on to its next element.
    struct Step
    {
        bool decided{false};
        int  result{0};
        bool advanceSource{true};
        bool advanceTarget{true};
    };

    /// A difference below the primary level, which decides unless a primary one comes later:
    /// the first secondary one counts, and the first tertiary one while no secondary one has.
    void noteSecondary(int result) noexcept
    {
        if (m_secondaryOpen)
        {
            m_result        = result;
            m_secondaryOpen = false;
        }
    }

    [[nodiscard]] Step compareElements(std::uint32_t source, std::uint32_t target) noexcept
    {
        if (source == target)
        {
            return {};
        }
        const std::uint32_t sourcePrimary = primaryOf(source);
        const std::uint32_t targetPrimary = primaryOf(target);
        if (sourcePrimary == targetPrimary)
        {
            if (m_secondaryOpen)
            {
                compareBelowPrimary(source, target);
            }
            return {};
        }
        // An ignorable element is passed over; a primary-ignorable one facing a primary weight is
        // a secondary difference, its string the greater, and is passed over too.
        if (source == 0)
        {
            return {.advanceTarget = false};
        }
        if (target == 0)
        {
            return {.advanceSource = false};
        }
        if (sourcePrimary == 0)
        {
            noteSecondary(1);
            return {.advanceTarget = false};
        }
        if (targetPrimary == 0)
        {
            noteSecondary(-1);
            return {.advanceSource = false};
        }
        return {.decided = true, .result = signOfDifference(sourcePrimary, targetPrimary)};
    }

    /// Two elements of the same primary weight that differ.
    void compareBelowPrimary(std::uint32_t source, std::uint32_t target) noexcept
    {
        const std::uint32_t sourceSecondary = secondaryOf(source);
        const std::uint32_t targetSecondary = secondaryOf(target);
        if (sourceSecondary != targetSecondary)
        {
            noteSecondary(signOfDifference(sourceSecondary, targetSecondary));
            return;
        }
        const std::uint32_t sourceTertiary = tertiaryOf(source);
        const std::uint32_t targetTertiary = tertiaryOf(target);
        if (m_tertiaryOpen && sourceTertiary != targetTertiary)
        {
            m_result       = signOfDifference(sourceTertiary, targetTertiary);
            m_tertiaryOpen = false;
        }
    }

    /// The elements of the longer string from @p first on, the other string having none left:
    /// a primary weight makes it the greater one (@p greater, 1 for the source and -1 for the
    /// target) at once, a secondary weight is a secondary difference, a tertiary one counts not.
    [[nodiscard]] int finishRest(std::uint32_t first, ElementReader& rest, int greater) noexcept
    {
        for (std::optional<std::uint32_t> element = first; element.has_value();
             element                              = rest.next())
        {
            if (primaryOf(*element) != 0)
            {
                return greater;
            }
            if (secondaryOf(*element) != 0)
            {
                noteSecondary(greater);
            }
        }
        return m_result;
    }

    ElementReader m_source;
    ElementReader m_target;
    bool          m_secondaryOpen;
    bool          m_tertiaryOpen;
    int           m_result{0};
};

}  // namespace

int compare(std::string_view a, std::string_view b, Strength strength)
{
    return Comparison(a, b, strength).run();
}

}  // namespace QtRocket::JavaCollator
