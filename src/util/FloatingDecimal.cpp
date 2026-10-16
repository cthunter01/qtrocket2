#include "QtRocket/util/FloatingDecimal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

#include "QtRocket/util/BugError.h"

// Binary-to-decimal conversion with the output of JDK 17 (Double.toString and the digits
// java.text.DecimalFormat starts from), written from the published algorithms:
//
// - Guy L. Steele Jr. and Jon L White, "How to Print Floating-Point Numbers Accurately" (PLDI
//   1990), and Robert G. Burger and R. Kent Dybvig, "Printing Floating-Point Numbers Quickly and
//   Accurately" (PLDI 1996): free-format digit generation with exact integers. The value is the
//   fraction r/s, every double within m/s of it reads back as the same double, and digits are
//   produced while neither end of that interval is reached; the last digit is then rounded.
// - David M. Gay, "Correctly Rounded Binary-Decimal and Decimal-Binary Conversions" (AT&T Bell
//   Laboratories Numerical Analysis Manuscript 90-10, 1990): the first decimal exponent estimated
//   from the binary exponent and the leading bits, which can come out one too high.
//
// JDK 17 does not print the shortest digits, and FloatingDecimal.h lists how its output departs
// from them. Each departure is reproduced below where it arises; the comments there say what JDK
// 17 does and how that is known (its output for the values named, or, for the two flags, its
// internal state read through jdk.internal.math in a JDK 17 run).

namespace QtRocket::FloatingDecimal
{

namespace
{

constexpr int           kFractionBits = 52;  // stored below the implicit leading bit
constexpr std::uint64_t kImplicitBit  = std::uint64_t{1} << kFractionBits;
constexpr std::uint64_t kFractionMask = kImplicitBit - 1;
constexpr std::uint64_t kSignMask     = std::uint64_t{1} << 63U;
constexpr int           kExponentMask = 0x7FF;
/// The power of two of a normal double's last significand bit is its stored exponent minus this.
constexpr int kUnitExponentOffset = 1075;
/// The power of two of every subnormal's last significand bit.
constexpr int kSubnormalUnitExponent = 1 - kUnitExponentOffset;

/// A non-negative integer of at most kLimbCount 32-bit limbs, least significant first. The
/// conversion's numbers stay below 2^800 (a subnormal scaled by 10^324, its margin grown by the
/// digit loop); fixed storage keeps the conversion free of allocations, and outgrowing it is a bug.
class Natural
{
public:
    static constexpr std::size_t kLimbCount = 28;

    constexpr Natural() = default;

    constexpr explicit Natural(std::uint64_t value)
      : m_limbs{static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32U)},
        m_length{static_cast<std::size_t>((std::bit_width(value) + 31) / 32)}
    {
    }

    [[nodiscard]] bool isZero() const noexcept { return m_length == 0; }

    /// The sign of this minus @p other.
    [[nodiscard]] int compare(const Natural& other) const
    {
        if (m_length != other.m_length)
        {
            return m_length < other.m_length ? -1 : 1;
        }
        for (std::size_t i = m_length; i > 0; --i)
        {
            const std::uint32_t mine   = m_limbs.at(i - 1);
            const std::uint32_t theirs = other.m_limbs.at(i - 1);
            if (mine != theirs)
            {
                return mine < theirs ? -1 : 1;
            }
        }
        return 0;
    }

    void multiply(std::uint32_t factor)
    {
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < m_length; ++i)
        {
            const std::uint64_t product = (std::uint64_t{m_limbs.at(i)} * factor) + carry;
            m_limbs.at(i)               = static_cast<std::uint32_t>(product);
            carry                       = product >> 32U;
        }
        if (carry != 0)
        {
            appendLimb(static_cast<std::uint32_t>(carry));
        }
        trim();
    }

    void multiplyByPowerOfFive(int exponent)
    {
        constexpr std::uint32_t kFiveToThe13 = 1220703125;  // the largest power of five in 32 bits
        constexpr int           kChunk       = 13;
        for (; exponent >= kChunk; exponent -= kChunk)
        {
            multiply(kFiveToThe13);
        }
        std::uint32_t rest = 1;
        for (int i = 0; i < exponent; ++i)
        {
            rest *= 5;
        }
        multiply(rest);
    }

    void shiftLeft(int bits)
    {
        if (isZero() || bits <= 0)
        {
            return;
        }
        const auto limbShift = static_cast<std::size_t>(bits) / 32U;
        const auto bitShift  = static_cast<unsigned>(bits) % 32U;
        QTROCKET_ASSERT(m_length + limbShift < kLimbCount);
        // From the top down, so that every limb is read before its place is written.
        for (std::size_t i = m_length; i > 0; --i)
        {
            const std::uint64_t wide = std::uint64_t{m_limbs.at(i - 1)} << bitShift;
            m_limbs.at(i + limbShift) |= static_cast<std::uint32_t>(wide >> 32U);
            m_limbs.at(i - 1 + limbShift) = static_cast<std::uint32_t>(wide);
        }
        for (std::size_t i = 0; i < limbShift; ++i)
        {
            m_limbs.at(i) = 0;
        }
        m_length += limbShift + 1;
        trim();
    }

    void add(const Natural& other)
    {
        const std::size_t length = std::max(m_length, other.m_length);
        std::uint64_t     carry  = 0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const std::uint64_t sum = std::uint64_t{m_limbs.at(i)} + other.m_limbs.at(i) + carry;
            m_limbs.at(i)           = static_cast<std::uint32_t>(sum);
            carry                   = sum >> 32U;
        }
        m_length = length;
        if (carry != 0)
        {
            appendLimb(static_cast<std::uint32_t>(carry));
        }
    }

    /// Subtracts @p factor times @p other, which must not exceed this number.
    void subtract(const Natural& other, std::uint32_t factor = 1)
    {
        QTROCKET_ASSERT(other.m_length <= m_length);
        std::uint64_t borrow = 0;  // what the next limb owes, the product's high part included
        for (std::size_t i = 0; i < m_length; ++i)
        {
            const std::uint64_t owed = (std::uint64_t{other.m_limbs.at(i)} * factor) + borrow;
            const auto          low  = static_cast<std::uint32_t>(owed);
            borrow                   = (owed >> 32U) + (m_limbs.at(i) < low ? 1 : 0);
            m_limbs.at(i) -= low;  // modulo 2^32
        }
        QTROCKET_ASSERT(borrow == 0);
        trim();
    }

    /// Replaces this number by its remainder modulo @p divisor and returns the quotient, which the
    /// caller guarantees to be a single decimal digit. The quotient is estimated in floating point
    /// from the divisor's three leading limbs and the same places of this number (rounding errors
    /// below 2^-50), made a little low so that the subtraction cannot go below zero, and corrected.
    [[nodiscard]] int takeDigit(const Natural& divisor)
    {
        const std::size_t from = divisor.m_length > 3 ? divisor.m_length - 3 : 0;
        const double estimate  = leadingPart(from) / divisor.leadingPart(from) * (1.0 - 0x1p-40);
        auto         digit     = static_cast<std::uint32_t>(std::min(estimate, 9.0));
        if (digit > 0)
        {
            subtract(divisor, digit);
        }
        while (compare(divisor) >= 0)
        {
            subtract(divisor);
            ++digit;
        }
        QTROCKET_ASSERT(digit <= 9);
        return static_cast<int>(digit);
    }

private:
    /// floor(this / 2^(32 * @p from)) as a double, rounded.
    [[nodiscard]] double leadingPart(std::size_t from) const
    {
        double value = 0.0;
        for (std::size_t i = m_length; i > from; --i)
        {
            value = (value * 0x1p32) + m_limbs.at(i - 1);
        }
        return value;
    }

    void appendLimb(std::uint32_t limb)
    {
        QTROCKET_ASSERT(m_length < kLimbCount);
        m_limbs.at(m_length) = limb;
        ++m_length;
    }

    void trim()
    {
        while (m_length > 0 && m_limbs.at(m_length - 1) == 0)
        {
            --m_length;
        }
    }

    // The limbs from m_length up are zero, so that add() and subtract() can read them.
    std::array<std::uint32_t, kLimbCount> m_limbs{};
    std::size_t                           m_length{0};
};

/// A positive finite double as significand * 2^unitExponent, the significand holding the implicit
/// bit of a normal number.
struct BinaryValue
{
    std::uint64_t significand{0};
    int           unitExponent{0};

    /// floor(log2(value)).
    [[nodiscard]] int topBitExponent() const
    {
        return unitExponent + std::bit_width(significand) - 1;
    }

    /// Whether the significand is a single bit: a power of two, subnormal ones included.
    [[nodiscard]] bool isPowerOfTwo() const { return std::has_single_bit(significand); }
};

[[nodiscard]] BinaryValue decompose(std::uint64_t bits)
{
    const auto          stored   = static_cast<int>((bits >> kFractionBits) & kExponentMask);
    const std::uint64_t fraction = bits & kFractionMask;
    if (stored == 0)
    {
        return BinaryValue{.significand = fraction, .unitExponent = kSubnormalUnitExponent};
    }
    return BinaryValue{.significand  = fraction | kImplicitBit,
                       .unitExponent = stored - kUnitExponentOffset};
}

/// Gay's estimate of floor(log10(value)): log10 of the significand scaled into [1, 2), taken from
/// its tangent at 1.5, plus the binary exponent times log10(2). The tangent lies above the
/// logarithm, so the estimate is exact or one too high (it exceeds log10 by less than 0.032), and
/// never too low: within a binade it rises with the value, and for the smallest double at or above
/// each power of ten it is at least that power's exponent (checked for every power). The constants,
/// shorter than Gay's, and the order of evaluation without fused multiply-adds are those of the
/// conversion this file replaced, which took them from JDK 17: the estimate decides which values
/// take the leading-zero branch of generateDigits(), which shows in digitsRoundedUp, and the size
/// of the scaled numbers, which decides JDK 17's arithmetic. Whether the shorter constants matter
/// is not known: Gay's longer ones gave the same output for every value of the differential test.
[[nodiscard]] int estimateDecimalExponent(const BinaryValue& value)
{
    const int    topBit     = value.topBitExponent();
    const double normalized = std::ldexp(static_cast<double>(value.significand),
                                         -(std::bit_width(value.significand) - 1));
    const double estimate   = ((normalized - 1.5) * 0.289529654) + 0.176091259 +
                              (static_cast<double>(topBit) * 0.301029995663981);
    return static_cast<int>(std::floor(estimate));
}

/// JDK 17 prints an integer below 2^63 from its exact value, not from the digit loop: "1234" for
/// 1234.0 whatever the format. From 2^58 on, it rounds the value half up to the largest power of
/// ten that is not above a quarter of the gap between neighbouring doubles (10 from 2^58, 100
/// from 2^61): 2^60 = 1152921504606846976 prints as "1.15292150460684698E18" (JDK 17's
/// Double.toString). No double there lies halfway between two candidates (a multiple of 64 never
/// ends in 5, nor a multiple of 512 in 50), so half up cannot be told from half even. It flags
/// neither digitsRoundedUp nor decimalDigitsExact here (read in a JDK 17 run). Returns false for
/// any other @p magnitude.
[[nodiscard]] bool writeSmallInteger(BinaryToAscii& out, double magnitude, const BinaryValue& value)
{
    constexpr double kTwoTo63 = 0x1p63;
    if (magnitude < 1.0 || magnitude >= kTwoTo63 || magnitude != std::trunc(magnitude))
    {
        return false;
    }
    auto      integer = static_cast<std::uint64_t>(magnitude);
    const int topBit  = value.topBitExponent();

    // A quarter of the gap is 2^(topBit - 54); below 2^58 it is under ten and nothing is rounded.
    int           dropped = 0;
    std::uint64_t unit    = 1;
    while (topBit >= kFractionBits + 2 &&
           unit * 10 <= (std::uint64_t{1} << static_cast<unsigned>(topBit - kFractionBits - 2)))
    {
        unit *= 10;
        ++dropped;
    }
    if (dropped > 0)
    {
        const std::uint64_t below = integer % unit;
        integer /= unit;
        if (below >= unit / 2)
        {
            ++integer;
        }
    }
    std::string text    = std::format("{}", integer);
    out.decimalExponent = static_cast<int>(text.size()) + dropped;
    text.erase(text.find_last_not_of('0') + 1);
    out.digits = std::move(text);
    return true;
}

/// The decimal fraction being written, as integers over one denominator (Steele and White's R, S
/// and M): the part of the value not yet written is remainder / unit units of the digit being
/// generated, and margin / unit is half the gap to a neighbouring double in the same units.
struct ScaledFraction
{
    Natural remainder;
    Natural unit;
    Natural margin;
    /// JDK 17 runs its digit loop in 64-bit integers for this value; see reachesUpperEnd().
    bool jdkWordArithmetic{false};
};

/// The smallest n with 10 * n >= 2^63: ten times a number below it fits a signed 64-bit integer.
constexpr Natural kWordLimit{922337203685477581};

/// Scales @p value for the first digit at 10^@p decimalExponent: remainder / unit = value /
/// 10^decimalExponent. The three numbers are the smallest integers with their ratios (no common
/// power of two), as JDK 17 scales them, because its choice of arithmetic depends on their size.
///
/// The margin is half the gap to the lower neighbour, on both sides: half the gap normally, and a
/// quarter of the gap above when the significand is a power of two. JDK 17 takes that quarter for
/// every single-bit significand, also for the subnormal powers of two and for 2^-1022, whose
/// neighbours are equally far on both sides. The narrower upper side gives 2^69 its extra digit:
/// "5.9029581035870565E20" in JDK 17, where "5.902958103587057E20" reads back as 2^69 too.
[[nodiscard]] ScaledFraction scaleFraction(const BinaryValue& value, int decimalExponent)
{
    const int trailingZeros = std::countr_zero(value.significand);
    const int oddExponent   = value.unitExponent + trailingZeros;
    const int marginExponent =
        value.unitExponent - 1 - (value.isPowerOfTwo() ? 1 : 0);  // in powers of two
    const int fivesBelow      = std::max(0, -decimalExponent);    // 10^-k = 5^-k * 2^-k for k < 0
    const int fivesAbove      = std::max(0, decimalExponent);
    const int twosOfRemainder = oddExponent + fivesBelow;
    const int twosOfUnit      = fivesAbove;
    const int twosOfMargin    = marginExponent + fivesBelow;
    const int common          = std::min({twosOfRemainder, twosOfUnit, twosOfMargin});

    ScaledFraction scaled;
    scaled.remainder = Natural(value.significand >> static_cast<unsigned>(trailingZeros));
    scaled.remainder.multiplyByPowerOfFive(fivesBelow);
    scaled.remainder.shiftLeft(twosOfRemainder - common);
    scaled.unit = Natural(1);
    scaled.unit.multiplyByPowerOfFive(fivesAbove);
    scaled.unit.shiftLeft(twosOfUnit - common);
    scaled.margin = Natural(1);
    scaled.margin.multiplyByPowerOfFive(fivesBelow);
    scaled.margin.shiftLeft(twosOfMargin - common);
    // JDK 17 generates the digits in 64-bit integers when ten times the unit fits a signed one
    // (its estimate of the remainder's size never decides on its own: checked over every
    // exponent class), and in big integers otherwise.
    scaled.jdkWordArithmetic = scaled.unit.compare(kWordLimit) < 0;
    return scaled;
}

/// Whether the remainder lies below the margin: dropping the rest keeps the digits inside the
/// rounding interval.
[[nodiscard]] bool reachesLowerEnd(const ScaledFraction& f)
{
    return f.remainder.compare(f.margin) < 0;
}

/// Whether rounding the last digit up keeps the digits inside the rounding interval: remainder +
/// margin against one unit.
///
/// Burger and Dybvig count the end of the interval in when the significand is even (a reader that
/// rounds half to even reads it back as this double) and out when it is odd. JDK 17 counts it out
/// when it computes in 64-bit integers and in when it computes in big integers, whatever the
/// significand, so for a value exactly a margin below a shorter decimal the digits depend on the
/// size of its scaled numbers. 1e23 = 0x1.52d02c7e14af6p76 = 99999999999999991611392 lies exactly
/// half a gap below 10^23, on the 64-bit side, and JDK 17 prints "9.999999999999999E22"; the same
/// significand at 2^87, 204799999999999982820130816, lies exactly half a gap below 2.048 * 10^26,
/// on the big-integer side, and JDK 17 prints "2.048E26".
///
/// In 64-bit integers JDK 17 moreover forms ten times the sum in a signed 64-bit integer, which
/// wraps to a negative number, failing the test, when it reaches 2^63. (When ten times the margin
/// alone reaches 2^63, JDK 17 counts both ends as reached, as exact arithmetic does.) Enumerating
/// every exponent class shows that the wrap happens only between 2^84 and 2^85, at the seventeenth
/// digit, where it keeps a digit that the exact test rounds up: JDK 17 prints
/// 0x1.7606c1e42a3b5p84 = 28260635462337084975480832 as "2.8260635462337084E25", where the exact
/// test gives "2.8260635462337085E25".
[[nodiscard]] bool reachesUpperEnd(const ScaledFraction& f)
{
    Natural sum = f.remainder;
    sum.add(f.margin);
    const int order = sum.compare(f.unit);
    if (!f.jdkWordArithmetic)
    {
        return order >= 0;
    }
    if (order <= 0)
    {
        return false;
    }
    return sum.compare(kWordLimit) < 0 || f.margin.compare(kWordLimit) >= 0;
}

/// The sign of 2 * remainder - unit: whether the rest is below, at or above half a unit.
[[nodiscard]] int compareWithHalfUnit(const ScaledFraction& f)
{
    Natural doubled = f.remainder;
    doubled.add(f.remainder);
    return doubled.compare(f.unit);
}

/// Adds one to the last digit, the carry running through trailing nines, which become zeros and
/// stay in the digits. A carry out of the first digit leaves "10...0", as many digits as before,
/// with the decimal exponent one higher and digitsRoundedUp unset, as in JDK 17 (its state after
/// converting 1e-321: "99" became "10", not flagged).
void incrementLastDigit(BinaryToAscii& out)
{
    for (std::size_t i = out.digits.size(); i > 0; --i)
    {
        char& digit = out.digits[i - 1];
        if (digit != '9')
        {
            ++digit;
            out.digitsRoundedUp = true;
            return;
        }
        digit = '0';
    }
    out.digits.front() = '1';
    ++out.decimalExponent;
}

/// Generates the digits of @p value outside writeSmallInteger()'s range.
void generateDigits(BinaryToAscii& out, const BinaryValue& value, bool compatibleFormat)
{
    int            exponent = estimateDecimalExponent(value);
    ScaledFraction f        = scaleFraction(value, exponent);

    // With the estimate one too high the value lies below one unit and its first digit would be
    // a zero. JDK 17 drops that zero and moves down one place, unless rounding it up stays inside
    // the interval, that is unless the value lies within its margin below 10^exponent: then the
    // zero is the first digit and the end test rounds it up to "1" (or, with a second digit
    // forced, "09" to "10"), setting digitsRoundedUp. Burger and Dybvig choose the exponent from
    // the upper end of the interval in the same way. With a correct estimate such a value gives
    // "9" (or "99") instead, carried to "10" with digitsRoundedUp unset. JDK 17 reports 1e-7 and
    // 1e-322 as "10" rounded up, and 1e-321, the only power of ten below which the nearest double
    // gets a correct estimate, as "10" not rounded up.
    //
    // JDK 17 always writes a second digit when the decimal exponent is below -3 or from 8 on, and
    // with the compatible format off for every exponent: the end tests after the first digit are
    // ignored. Below -3 that matches Double.toString's E-form ("4.9E-324" for the smallest
    // subnormal, where "5" would do). Two details come from the conversion this file replaced
    // (which took them from JDK 17) and show in no output, so no JDK 17 run can confirm them
    // (variants without them differed on no value of the differential test): the threshold is 8
    // although E-form starts at 7, but a non-integer of [10^7, 10^8) needs more than one digit
    // anyway; and after a dropped zero the check has been spent on the zero, so the first real
    // digit is tested, but that digit is a 9 (the value lies above 0.93 * 10^exponent) and ends
    // nothing.
    bool testFirstDigit = true;
    if (f.remainder.compare(f.unit) < 0 && !reachesUpperEnd(f))
    {
        --exponent;
        f.remainder.multiply(10);
        f.margin.multiply(10);
    }
    else
    {
        testFirstDigit = compatibleFormat && exponent >= -3 && exponent < 8;
    }

    bool atLowerEnd = false;
    bool atUpperEnd = false;
    for (bool first = true;; first = false)
    {
        const int digit = f.remainder.takeDigit(f.unit);
        out.digits.push_back(static_cast<char>('0' + digit));
        if (first && !testFirstDigit)
        {
            atLowerEnd = false;
            atUpperEnd = false;
        }
        else
        {
            atLowerEnd = reachesLowerEnd(f);
            atUpperEnd = reachesUpperEnd(f);
        }
        if (atLowerEnd || atUpperEnd)
        {
            break;
        }
        f.remainder.multiply(10);
        f.margin.multiply(10);
    }
    out.decimalExponent    = exponent + 1;
    out.decimalDigitsExact = f.remainder.isZero();

    // Round the last digit: up when only the upper end is reached; when both are, to the nearer
    // of the two candidates, and on an exact tie to an even digit.
    if (!atUpperEnd)
    {
        return;
    }
    if (atLowerEnd)
    {
        const int half = compareWithHalfUnit(f);
        if (half < 0 || (half == 0 && (out.digits.back() - '0') % 2 == 0))
        {
            return;
        }
    }
    incrementLastDigit(out);
}

}  // namespace

BinaryToAscii binaryToAscii(double value, bool compatibleFormat)
{
    QTROCKET_ASSERT(std::isfinite(value));
    const auto    bits = std::bit_cast<std::uint64_t>(value);
    BinaryToAscii out;
    out.negative = (bits & kSignMask) != 0;
    if ((bits & ~kSignMask) == 0)
    {
        out.digits = "0";
        return out;
    }
    const BinaryValue binary = decompose(bits);
    if (!writeSmallInteger(out, std::abs(value), binary))
    {
        generateDigits(out, binary, compatibleFormat);
    }
    return out;
}

std::string toJavaFormatString(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    return toJavaFormatString(binaryToAscii(value));
}

std::string toJavaFormatString(const BinaryToAscii& converted)
{
    // Double.toString's layout: plain from 0.001 up to 10^7 with at least one digit after the
    // point, otherwise one digit, the point, at least one more digit and the power of ten.
    const std::string& digits   = converted.digits;
    const auto         count    = static_cast<int>(digits.size());
    const int          exponent = converted.decimalExponent;  // value = 0.digits * 10^exponent
    std::string        text     = converted.negative ? "-" : "";
    if (exponent >= 1 && exponent <= 7)
    {
        const auto whole = static_cast<std::size_t>(std::min(count, exponent));
        text += digits.substr(0, whole);
        text.append(static_cast<std::size_t>(std::max(0, exponent - count)), '0');
        text += '.';
        text += count > exponent ? digits.substr(whole) : std::string("0");
    }
    else if (exponent >= -2 && exponent <= 0)
    {
        text += "0.";
        text.append(static_cast<std::size_t>(-exponent), '0');
        text += digits;
    }
    else
    {
        text += digits.front();
        text += '.';
        text += count > 1 ? digits.substr(1) : std::string("0");
        text += std::format("E{}", exponent - 1);
    }
    return text;
}

}  // namespace QtRocket::FloatingDecimal
