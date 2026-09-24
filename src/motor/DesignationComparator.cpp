#include "QtRocket/motor/DesignationComparator.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The capture groups of OpenRocket's designation pattern that the comparison uses.
struct Designation
{
    std::string_view divisor;       ///< group 2: "2" for "1/2A", empty without a fraction
    std::string_view impulseClass;  ///< group 3: the class letter
    std::string      thrust;        ///< group 4 without its commas and leading zeros
    std::string_view extra;         ///< group 5: whatever follows the thrust
};

[[nodiscard]] bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

[[nodiscard]] bool isAsciiLetter(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/// The line terminators of java.util.regex without UNIX_LINES, as UTF-8: '.' matches none of
/// them, and '$' matches before one of them that ends the input.
constexpr std::array<std::string_view, 6> kLineTerminators{
    "\r\n", "\n", "\r", "\xC2\x85", "\xE2\x80\xA8", "\xE2\x80\xA9"};

/// `(.*?)$` applied to @p rest: the part before a single line terminator that ends the input
/// (or all of @p rest), or nullopt when a line terminator occurs anywhere else.
[[nodiscard]] std::optional<std::string_view> matchRest(std::string_view rest) noexcept
{
    for (std::size_t i = 0; i < rest.size(); i++)
    {
        const std::string_view tail = rest.substr(i);
        for (const std::string_view terminator : kLineTerminators)
        {
            if (tail.starts_with(terminator))
            {
                if (tail == terminator)
                {
                    return rest.substr(0, i);
                }
                return std::nullopt;
            }
        }
    }
    return rest;
}

/// Groups 3 to 5 of the pattern from @p position on: `([a-zA-Z])([0-9,]+)(.*?)$`.
[[nodiscard]] std::optional<Designation> matchClass(std::string_view text, std::size_t position,
                                                    std::string_view divisor)
{
    if (position >= text.size() || !isAsciiLetter(text[position]))
    {
        return std::nullopt;
    }
    const std::string_view impulseClass = text.substr(position, 1);
    position++;
    const std::size_t thrustStart = position;
    while (position < text.size() && (isDigit(text[position]) || text[position] == ','))
    {
        position++;
    }
    if (position == thrustStart)
    {
        return std::nullopt;
    }
    const std::optional<std::string_view> extra = matchRest(text.substr(position));
    if (!extra.has_value())
    {
        return std::nullopt;
    }
    std::string digits;
    for (const char c : text.substr(thrustStart, position - thrustStart))
    {
        if (c != ',' && (c != '0' || !digits.empty()))
        {
            digits.push_back(c);
        }
    }
    return Designation{.divisor      = divisor,
                       .impulseClass = impulseClass,
                       .thrust       = std::move(digits),
                       .extra        = *extra};
}

/// OpenRocket's pattern `^([0-9]+-?|1/([1-8]))?([a-zA-Z])([0-9,]+)(.*?)$` applied to @p text.
/// Each way of reading the optional prefix leaves a different kind of character next (a letter or
/// '-', a letter, or the class letter itself), so at most one of them can lead to a match and the
/// regex's backtracking reduces to trying them in turn.
[[nodiscard]] std::optional<Designation> parse(std::string_view text)
{
    if (text.empty() || !isDigit(text.front()))
    {
        return matchClass(text, 0, {});
    }
    // [0-9]+-?
    std::size_t position = 0;
    while (position < text.size() && isDigit(text[position]))
    {
        position++;
    }
    if (position < text.size() && text[position] == '-')
    {
        position++;
    }
    if (std::optional<Designation> match = matchClass(text, position, {}))
    {
        return match;
    }
    // 1/([1-8])
    if (text.size() >= 3 && text.starts_with("1/") && text[2] >= '1' && text[2] <= '8')
    {
        return matchClass(text, 3, text.substr(2, 1));
    }
    return std::nullopt;
}

/// The divisor of a fractional A class, "1" for a whole one.
[[nodiscard]] std::string_view divisorOrOne(const Designation& designation) noexcept
{
    return designation.divisor.empty() ? "1" : designation.divisor;
}

/// Compares two thrusts given as digits without leading zeros (empty for zero): their difference
/// when both fit an int, as Integer.parseInt gives them in OpenRocket, else -1, 0 or 1 by value.
/// Deviation: Integer.parseInt throws in OpenRocket on a thrust with no digits ("A,") or beyond
/// the int range, aborting the sort; here the no-digit thrust counts as zero and the large one by
/// its value.
[[nodiscard]] int compareThrust(std::string_view t1, std::string_view t2) noexcept
{
    const std::optional<int> a1 = t1.empty() ? std::optional<int>{0} : Strings::parseInt(t1);
    const std::optional<int> a2 = t2.empty() ? std::optional<int>{0} : Strings::parseInt(t2);
    if (a1.has_value() && a2.has_value())
    {
        // Both are non-negative, so the difference cannot overflow.
        return *a1 - *a2;
    }
    // Without leading zeros the longer number is the larger, and equally long ones compare
    // digit by digit.
    if (t1.size() != t2.size())
    {
        return t1.size() < t2.size() ? -1 : 1;
    }
    const int value = t1.compare(t2);
    if (value == 0)
    {
        return 0;
    }
    return value < 0 ? -1 : 1;
}

/// The comparison of two designations that both have the pattern's form.
[[nodiscard]] int compareMatched(const Designation& m1, const Designation& m2)
{
    // 1. Motor class; 1/2A and 1/4A comparison within the A class
    if (Strings::equalsIgnoreAsciiCase(m1.impulseClass, "A") &&
        Strings::equalsIgnoreAsciiCase(m2.impulseClass, "A") &&
        (!m1.divisor.empty() || !m2.divisor.empty()))
    {
        const int value = -Strings::javaPrimaryCollatorCompare(divisorOrOne(m1), divisorOrOne(m2));
        if (value != 0)
        {
            return value;
        }
    }
    const int value = Strings::javaPrimaryCollatorCompare(m1.impulseClass, m2.impulseClass);
    if (value != 0)
    {
        return value;
    }

    // 2. Average thrust
    const int thrustValue = compareThrust(m1.thrust, m2.thrust);
    if (thrustValue != 0)
    {
        return thrustValue;
    }

    // 3. Extra modifier
    return Strings::javaPrimaryCollatorCompare(m1.extra, m2.extra);
}

}  // namespace

int DesignationComparator::compare(std::string_view a, std::string_view b)
{
    const std::optional<Designation> m1 = parse(a);
    const std::optional<Designation> m2 = parse(b);

    if (m1.has_value() && m2.has_value())
    {
        return compareMatched(*m1, *m2);
    }
    if (!m1.has_value() && !m2.has_value())
    {
        // Neither matches the designation pattern, simply compare strings
        return Strings::javaPrimaryCollatorCompare(a, b);
    }
    // One matches and one doesn't: non-matching designations sort after matching ones, which
    // keeps the comparison transitive.
    return m1.has_value() ? -1 : 1;
}

}  // namespace QtRocket
