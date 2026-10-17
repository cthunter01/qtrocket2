#include "QtRocket/util/FloatingDecimal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

#include "QtRocket/util/BugError.h"

// Binary-to-decimal conversion with the output of JDK 17 (Double.toString and the digits
// java.text.DecimalFormat starts from), written from the published algorithms:
//
// - Guy L. Steele Jr. and Jon L White, "How to Print Floating-Point Numbers Accurately" (PLDI
//   1990), and Robert G. Burger and R. Kent Dybvig, "Printing Floating-Point Numbers Quickly and
//   Accurately" (PLDI 1996): free-format digit generation with exact integers. The value is the
//   fraction r/s, every number within m/s of it reads back as this double, and digits are
//   produced while neither end of that interval is reached; the last digit is then rounded.
// - David M. Gay, "Correctly Rounded Binary-Decimal and Decimal-Binary Conversions" (AT&T Bell
//   Laboratories Numerical Analysis Manuscript 90-10, 1990): the first decimal exponent estimated
//   from the binary exponent and the leading bits, which can come out one too high.
//
// JDK 17 does not print the shortest digits, and FloatingDecimal.h lists how its output departs
// from them. Each departure is reproduced below where it arises; the comments there say what JDK
// 17 does and how that is known (its output for the values named, or, for the two flags, its
// internal state read through jdk.internal.math in a JDK 17 run), and which details are only
// those of QtRocket's earlier conversion (commit 219df0d), whose output this one keeps.

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

/// The smallest n with 10 * n >= 2^63: ten times a number below it fits a signed 64-bit integer.
constexpr std::uint64_t kJdkWordLimit = 922337203685477581;

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

    /// The value, which must fit 64 bits.
    [[nodiscard]] std::uint64_t toWord() const
    {
        QTROCKET_ASSERT(m_length <= 2);
        return std::uint64_t{m_limbs.at(0)} | (std::uint64_t{m_limbs.at(1)} << 32U);
    }

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

constexpr Natural kJdkWordLimitNatural{kJdkWordLimit};

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
/// a straight line through log10(1.5) with the slope of the logarithm there, plus the binary
/// exponent times log10(2). It exceeds log10(value) by less than 0.032, and its constants, a
/// little short of the true ones (0.176091259 is 5.6e-11 below log10(1.5)), can put it below
/// log10(value) by less than 1e-10 near a significand of 1.5. So the estimate is exact or one too
/// high wherever it is not too low at the smallest double at or above a power of ten: within a
/// binade it rises with the value, and floor(log10) steps only at powers of ten. The test
/// TheDecimalExponentEstimateIsNeverTooLow converts those doubles; an estimate too low there
/// would make the first digit ten or more and trip an assertion of the digit generation.
///
/// The constants and the order of evaluation, without fused multiply-adds, are those of QtRocket's
/// earlier conversion, which took them from JDK 17: the estimate decides which values take the
/// leading-zero branch of writeDigits(), which shows in digitsRoundedUp, and the size of the scaled
/// numbers, which decides JDK 17's arithmetic. Gay's longer constants would floor differently for
/// about 1.4e9 doubles, none of them next to a power of ten. A check during the review of this file
/// found no output that changes: in full for the one window of them (1.2e6 doubles in the binade
/// of 2^86, decimal exponent 26) where the estimate also changes JDK 17's arithmetic, and on a
/// sample of the others.
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
/// 1234.0 whatever the format. From 2^58 on, it rounds the value to the largest power of ten that
/// is not above a quarter of the gap between neighbouring doubles (10 from 2^58, 100 from 2^61):
/// 2^60 = 1152921504606846976 prints as "1.15292150460684698E18" (JDK 17's Double.toString). The
/// rounding here is half up, as QtRocket's earlier conversion rounded; no JDK 17 output can confirm
/// it, because no double there lies halfway between two candidates (a multiple of 64 never ends in
/// 5, nor a multiple of 512 in 50), so half up cannot be told from half even. JDK 17 flags neither
/// digitsRoundedUp nor decimalDigitsExact here (read in a JDK 17 run). Returns false for any other
/// @p magnitude.
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
    std::array<char, 20> buffer{};  // 2^64 has twenty digits
    const auto [end, error] =
        std::to_chars(buffer.data(), std::next(buffer.data(), buffer.size()), integer);
    QTROCKET_ASSERT(error == std::errc{});
    const std::string_view text(buffer.data(), end);
    out.decimalExponent = static_cast<int>(text.size()) + dropped;
    out.digits.assign(text.substr(0, text.find_last_not_of('0') + 1));
    return true;
}

/// The value scaled for its first digit, as integers over one denominator (Steele and White's R,
/// S and M): the part of the value not yet written is remainder / unit units of the digit being
/// generated, and margin / unit is half the gap to a neighbouring double in the same units. The
/// member functions are the steps of writeDigits() in exact big-integer arithmetic, with the
/// upper-end test JDK 17 uses when it computes in big integers.
class NaturalFraction
{
public:
    /// Scales @p value for the first digit at 10^@p decimalExponent: remainder / unit = value /
    /// 10^decimalExponent. The three numbers are the smallest integers with their ratios (no
    /// common power of two), as JDK 17 scales them, because its choice of arithmetic depends on
    /// their size, see fitsJdkWords().
    ///
    /// The margin is the same on both sides: half the gap normally, and a quarter of the gap above
    /// when the significand is a power of two. JDK 17 takes that quarter for every single-bit
    /// significand, also for the subnormal powers of two and for 2^-1022, whose neighbours are
    /// equally far on both sides. The narrower upper side gives 2^69 its extra digit:
    /// "5.9029581035870565E20" in JDK 17, where "5.902958103587057E20" reads back as 2^69 too.
    NaturalFraction(const BinaryValue& value, int decimalExponent)
    {
        const int trailingZeros = std::countr_zero(value.significand);
        const int oddExponent   = value.unitExponent + trailingZeros;
        const int marginExponent =
            value.unitExponent - 1 - (value.isPowerOfTwo() ? 1 : 0);  // in powers of two
        const int fivesBelow      = std::max(0, -decimalExponent);    // 10^-k = 5^-k * 2^-k
        const int fivesAbove      = std::max(0, decimalExponent);
        const int twosOfRemainder = oddExponent + fivesBelow;
        const int twosOfUnit      = fivesAbove;
        const int twosOfMargin    = marginExponent + fivesBelow;
        const int common          = std::min({twosOfRemainder, twosOfUnit, twosOfMargin});

        m_remainder = Natural(value.significand >> static_cast<unsigned>(trailingZeros));
        m_remainder.multiplyByPowerOfFive(fivesBelow);
        m_remainder.shiftLeft(twosOfRemainder - common);
        m_unit = Natural(1);
        m_unit.multiplyByPowerOfFive(fivesAbove);
        m_unit.shiftLeft(twosOfUnit - common);
        m_margin = Natural(1);
        m_margin.multiplyByPowerOfFive(fivesBelow);
        m_margin.shiftLeft(twosOfMargin - common);
    }

    /// Whether JDK 17 generates this value's digits in 64-bit integers: when ten times the unit
    /// fits a signed one. It also looks at the size of the remainder, but an enumeration of every
    /// class of values (binary exponent, significant bits, decimal exponent) made when this file
    /// was written found that the unit alone decides. In big integers otherwise.
    [[nodiscard]] bool fitsJdkWords() const { return m_unit.compare(kJdkWordLimitNatural) < 0; }

    [[nodiscard]] const Natural& remainder() const noexcept { return m_remainder; }
    [[nodiscard]] const Natural& unit() const noexcept { return m_unit; }
    [[nodiscard]] const Natural& margin() const noexcept { return m_margin; }

    [[nodiscard]] bool belowOneUnit() const { return m_remainder.compare(m_unit) < 0; }

    /// Whether the remainder lies below the margin: dropping the rest keeps the digits inside the
    /// rounding interval. Never when it is equal (strict on both of JDK 17's routes).
    [[nodiscard]] bool reachesLowerEnd() const { return m_remainder.compare(m_margin) < 0; }

    /// Whether rounding the last digit up keeps the digits inside the rounding interval: remainder
    /// + margin against one unit. Burger and Dybvig count the end of the interval in when the
    /// significand is even (a reader that rounds half to even reads it back as this double) and
    /// out when it is odd. JDK 17 in big integers counts it in whatever the significand, and in
    /// 64-bit integers out, see WordFraction::reachesUpperEnd(). So for a value exactly a margin
    /// below a shorter decimal the digits depend on the size of its scaled numbers: the
    /// significand of 1e23 at 2^87, 204799999999999982820130816, lies exactly half a gap below
    /// 2.048 * 10^26, on the big-integer side, and JDK 17 prints "2.048E26".
    [[nodiscard]] bool reachesUpperEnd() const
    {
        Natural sum = m_remainder;
        sum.add(m_margin);
        return sum.compare(m_unit) >= 0;
    }

    [[nodiscard]] int takeDigit() { return m_remainder.takeDigit(m_unit); }

    void nextPlace()
    {
        m_remainder.multiply(10);
        m_margin.multiply(10);
    }

    [[nodiscard]] bool isExhausted() const { return m_remainder.isZero(); }

    /// The sign of 2 * remainder - unit: whether the rest is below, at or above half a unit.
    [[nodiscard]] int compareWithHalfUnit() const
    {
        Natural doubled = m_remainder;
        doubled.add(m_remainder);
        return doubled.compare(m_unit);
    }

private:
    Natural m_remainder;
    Natural m_unit;
    Natural m_margin;
};

/// The steps of writeDigits() in exact 64-bit arithmetic, for the values JDK 17 converts with
/// 64-bit integers (NaturalFraction::fitsJdkWords()). This is QtRocket's own fast path: it
/// converts the usual values (from about 10^-2 up to 10^26) two to three times as fast as the big
/// integers do.
///
/// No number overflows: the remainder is below ten units at the start (the estimate is never too
/// low) and after nextPlace(), and below one after takeDigit(); the margin starts below one unit
/// (half a gap is at most 2^-53 of a normal value; subnormals never come here) and, since the loop
/// goes on only while it is not above the remainder, stays below ten units; so their sum stays
/// below eleven units, under 2^64.
class WordFraction
{
public:
    explicit WordFraction(const NaturalFraction& scaled)
      : m_remainder{scaled.remainder().toWord()},
        m_unit{scaled.unit().toWord()},
        m_margin{scaled.margin().toWord()}
    {
        QTROCKET_ASSERT(m_unit < kJdkWordLimit && m_remainder < 10 * m_unit && m_margin < m_unit);
    }

    [[nodiscard]] bool belowOneUnit() const noexcept { return m_remainder < m_unit; }

    [[nodiscard]] bool reachesLowerEnd() const noexcept { return m_remainder < m_margin; }

    /// JDK 17's upper-end test in 64-bit integers. It is strict: 1e23 = 0x1.52d02c7e14af6p76 =
    /// 99999999999999991611392 lies exactly half a gap below 10^23 and JDK 17 prints
    /// "9.999999999999999E22". JDK 17 moreover forms ten times the sum in a signed 64-bit
    /// integer, which wraps to a negative number, failing the test, when the sum reaches
    /// kJdkWordLimit. An enumeration of every class of values made when this file was written
    /// found that this happens only between 2^84 and 2^85, at the seventeenth digit, where it
    /// keeps a digit that the exact test rounds up: JDK 17 prints 0x1.7606c1e42a3b5p84 =
    /// 28260635462337084975480832 as "2.8260635462337084E25", where the exact test gives
    /// "...085E25". When ten times the margin alone reaches 2^63, JDK 17 counts both ends as
    /// reached and rounds to the nearer digit: it prints 0x1.bebd5bef7b22dp85 =
    /// 67509392340440294547783680 as "6.7509392340440295E25", rounded up. The test here states
    /// those outcomes as exact conditions; nothing wraps.
    [[nodiscard]] bool reachesUpperEnd() const noexcept
    {
        const std::uint64_t sum = m_remainder + m_margin;
        return sum > m_unit && (sum < kJdkWordLimit || m_margin >= kJdkWordLimit);
    }

    [[nodiscard]] int takeDigit() noexcept
    {
        const std::uint64_t digit = m_remainder / m_unit;
        m_remainder %= m_unit;
        return static_cast<int>(digit);
    }

    void nextPlace() noexcept
    {
        m_remainder *= 10;
        m_margin *= 10;
    }

    [[nodiscard]] bool isExhausted() const noexcept { return m_remainder == 0; }

    [[nodiscard]] int compareWithHalfUnit() const noexcept
    {
        const std::uint64_t doubled = 2 * m_remainder;
        if (doubled == m_unit)
        {
            return 0;
        }
        return doubled < m_unit ? -1 : 1;
    }

private:
    std::uint64_t m_remainder;
    std::uint64_t m_unit;
    std::uint64_t m_margin;
};

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

/// Generates the digits of the value that @p f holds, scaled for a first digit at 10^@p exponent,
/// in the arithmetic of @p f (NaturalFraction or WordFraction, which differ in the upper-end test).
template <typename Fraction>
void writeDigits(BinaryToAscii& out, Fraction& f, int exponent, bool compatibleFormat)
{
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
    // skipped. Below -3 that matches Double.toString's E-form ("4.9E-324" for the smallest
    // subnormal, where "5" would do; 1e-4 is "10" but 1e-3 "1" in a JDK 17 run). Two details are
    // those of QtRocket's earlier conversion and show in no output, so no JDK 17 run can confirm
    // them (variants without them gave the same output on every value compared when this file was
    // written): the threshold is 8 although E-form starts at 7, but a non-integer of [10^7, 10^8)
    // needs more than one digit anyway; and after a dropped zero the first real digit is always
    // tested, but that digit is a 9 (the value lies above 0.93 * 10^exponent) and ends nothing.
    bool testFirstDigit = true;
    if (f.belowOneUnit() && !f.reachesUpperEnd())
    {
        --exponent;
        f.nextPlace();
    }
    else
    {
        testFirstDigit = compatibleFormat && exponent >= -3 && exponent < 8;
    }

    out.digits.push_back(static_cast<char>('0' + f.takeDigit()));
    bool atLowerEnd = testFirstDigit && f.reachesLowerEnd();
    bool atUpperEnd = testFirstDigit && f.reachesUpperEnd();
    while (!atLowerEnd && !atUpperEnd)
    {
        f.nextPlace();
        out.digits.push_back(static_cast<char>('0' + f.takeDigit()));
        atLowerEnd = f.reachesLowerEnd();
        atUpperEnd = f.reachesUpperEnd();
    }
    out.decimalExponent = exponent + 1;
    // Whether nothing is left after the last digit: the digits are the value's whole decimal
    // expansion. The rounding below never raises such digits, since a zero rest lies below the
    // margin and below half a unit. JDK 17 sets the flag so for 0.25, for 0.5 in both formats and
    // for 1e19 ("10" with the forced second digit), as its state in a JDK 17 run shows.
    out.decimalDigitsExact = f.isExhausted();

    // Round the last digit: up when only the upper end is reached; when both are, to the nearer
    // of the two candidates. On an exact tie JDK 17 keeps the even digit (2^-25, 2^50 + 0.25 and
    // 2^50 + 0.75 in a JDK 17 run), a choice that Burger and Dybvig leave open.
    if (!atUpperEnd)
    {
        return;
    }
    if (atLowerEnd)
    {
        const int  half = f.compareWithHalfUnit();
        const bool odd  = (out.digits.back() - '0') % 2 != 0;
        if (half < 0 || (half == 0 && !odd))
        {
            return;
        }
    }
    incrementLastDigit(out);
}

/// Generates the digits of @p value outside writeSmallInteger()'s range.
void generateDigits(BinaryToAscii& out, const BinaryValue& value, bool compatibleFormat)
{
    const int       exponent = estimateDecimalExponent(value);
    NaturalFraction scaled(value, exponent);
    if (scaled.fitsJdkWords())
    {
        WordFraction words(scaled);
        writeDigits(out, words, exponent, compatibleFormat);
    }
    else
    {
        writeDigits(out, scaled, exponent, compatibleFormat);
    }
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
    const std::string& digits   = converted.digits;
    const auto         count    = static_cast<int>(digits.size());
    const int          exponent = converted.decimalExponent;  // value = 0.digits * 10^exponent
    std::string        text     = converted.negative ? "-" : "";

    // Double.toString's Javadoc: from 10^-3 up to but excluding 10^7, the integer part and the
    // fraction, each at least one digit, around a point; otherwise the "computerized scientific
    // notation", one digit, a point, at least one more digit, "E" and the power of ten.
    if (exponent < -2 || exponent > 7)
    {
        std::string mantissa = digits;
        mantissa.resize(std::max<std::size_t>(mantissa.size(), 2), '0');
        mantissa.insert(1, 1, '.');
        text += mantissa;
        text += std::format("E{}", exponent - 1);
        return text;
    }
    // The digit of 10^power: the digits fill the powers from exponent - 1 down.
    const auto digitOfPower = [&](int power) {
        const int index = exponent - 1 - power;
        return index >= 0 && index < count ? digits[static_cast<std::size_t>(index)] : '0';
    };
    for (int power = std::max(exponent, 1) - 1; power >= 0; --power)
    {
        text += digitOfPower(power);
    }
    text += '.';
    const int lowestPower = std::min(-1, exponent - count);
    for (int power = -1; power >= lowestPower; --power)
    {
        text += digitOfPower(power);
    }
    return text;
}

}  // namespace QtRocket::FloatingDecimal
