#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace QtRocket
{

/// An immutable decimal number of any size: an integer of any number of digits (the unscaled
/// value) and a 32-bit scale, the value being unscaledValue * 10^-scale. It ports the part of
/// java.math.BigDecimal that a Config needs: OpenRocket keeps a number that is not one of
/// Java's primitive wrappers as a BigDecimal (Config.clone(Object): new
/// BigDecimal(value.toString())), the .ork reader makes one of every number that is not exactly an
/// int, a long or a double ("0.10", a 30-digit integer), and the .ork writer prints it back with
/// toString(). The scale is part of the value: "1.0" and "1.00" are different BigDecimals, as in
/// Java.
///
/// Not ported: the arithmetic, rounding and MathContext, compareTo() (which ignores the scale),
/// hashCode(), and the constructor from a double (the exact binary expansion; valueOfDouble() is
/// BigDecimal.valueOf(double), the one OpenRocket calls).
///
/// Deviation: parse() takes ASCII digits only, where Java also takes the decimal digits of other
/// scripts (Character.digit, so the Arabic-Indic digit U+0661 is 1).
class BigDecimal
{
public:
    /// Zero with scale 0 (BigDecimal.ZERO).
    BigDecimal() = default;

    /// Java's new BigDecimal(String): an optional sign, decimal digits with an optional point (at
    /// least one digit: "5.", ".5" and "007" are fine), and an optional exponent, 'e' or 'E', an
    /// optional sign and at least one digit ("1E3", "1.5e-7"). The unscaled value is the digits
    /// without the point, and the scale the number of fraction digits minus the exponent, so
    /// "1E3" is 1 with scale -3 and "0.10" is 10 with scale 2. No whitespace, no "NaN" or
    /// "Infinity", no type suffix.
    ///
    /// nullopt where Java throws NumberFormatException: any other text, an exponent that does not
    /// fit an int or has more than ten digits after its leading zeros (which are skipped only
    /// while more than ten characters remain, as in Java), and a scale that does not fit an int.
    [[nodiscard]] static std::optional<BigDecimal> parse(std::string_view text);

    /// BigDecimal.valueOf(long): @p value with scale 0.
    [[nodiscard]] static BigDecimal valueOf(std::int64_t value);

    /// BigDecimal.valueOf(double): parse() of Double.toString(@p value) as JDK 17 prints it
    /// (FloatingDecimal::toJavaFormatString), so 100.0 is 1000 with scale 1 and 1.0E-4 is 10 with
    /// scale 5. nullopt for NaN and the infinities (Java: NumberFormatException).
    [[nodiscard]] static std::optional<BigDecimal> valueOfDouble(double value);

    /// -1, 0 or 1 (signum()).
    [[nodiscard]] int signum() const noexcept;

    /// The scale: the number of digits after the decimal point, or, negated, the power of ten the
    /// unscaled value is multiplied by.
    [[nodiscard]] std::int32_t scale() const noexcept { return m_scale; }

    /// The unscaled value in decimal digits (unscaledValue().toString()): "-123" for -1.23, "0"
    /// for every zero.
    [[nodiscard]] std::string unscaledValue() const;

    /// Java's toString(), the canonical text that parse() reads back as an equal value (unless
    /// its exponent no longer fits an int, which parse() refuses, as in Java). Without an
    /// exponent when the scale is not negative and the adjusted exponent (digits - 1 - scale) is
    /// at least -6: the digits with the point scale digits from the right ("123.45", "0.00010",
    /// "0.00" for a zero of scale 2). Otherwise scientific: the first digit, a point and the
    /// other digits when there are any, "E", the sign and the adjusted exponent ("1E+3",
    /// "1.23E-7", "0E-10").
    [[nodiscard]] std::string toString() const;

    /// Java's intValue(): the low 32 bits of the integer part (the fraction is dropped), so a
    /// value beyond the int range wraps instead of saturating (2147483648 gives -2147483648).
    [[nodiscard]] std::int32_t intValue() const noexcept;

    /// Java's longValue(): the low 64 bits of the integer part.
    [[nodiscard]] std::int64_t longValue() const noexcept;

    /// Java's doubleValue(): the nearest double (ties to even), an infinity beyond the double
    /// range and a zero of the value's sign below it. A zero is always 0.0.
    [[nodiscard]] double doubleValue() const;

    /// Java's equals(): the same unscaled value and the same scale, so 1.0 does not equal 1.00.
    [[nodiscard]] bool operator==(const BigDecimal& other) const noexcept = default;

private:
    BigDecimal(bool negative, std::string digits, std::int32_t scale) noexcept;

    /// digits - 1 - scale: the exponent of the first digit.
    [[nodiscard]] std::int64_t adjustedExponent() const noexcept;

    /// The magnitude of the unscaled value in decimal digits, without leading zeros ("0" for
    /// zero).
    std::string m_digits{"0"};
    /// Whether the unscaled value is negative; never set for zero.
    bool         m_negative{false};
    std::int32_t m_scale{0};
};

}  // namespace QtRocket
