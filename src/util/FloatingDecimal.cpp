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
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket::FloatingDecimal
{

namespace
{

// DoubleConsts and FloatingDecimal's constants.
constexpr int           kExpShift       = 52;  // EXP_SHIFT: SIGNIFICAND_WIDTH - 1
constexpr std::uint64_t kFractHob       = std::uint64_t{1} << kExpShift;  // the implicit bit
constexpr std::uint64_t kSignifBitMask  = kFractHob - 1;
constexpr std::uint64_t kExpBitMask     = 0x7FF0000000000000ULL;
constexpr std::uint64_t kSignBitMask    = 0x8000000000000000ULL;
constexpr int           kExpBias        = 1023;
constexpr std::uint64_t kExpOne         = std::uint64_t{kExpBias} << kExpShift;  // 1.0's exponent
constexpr int           kMaxSmallBinExp = 62;
constexpr int           kMinSmallBinExp = -(63 / 3);

/// FDBigInteger.LONG_5_POW: 5^0 ... 5^26, the powers of five that fit a long.
constexpr std::array<std::int64_t, 27> kLong5Pow = [] {
    std::array<std::int64_t, 27> powers{};
    std::int64_t                 p = 1;
    for (std::int64_t& power : powers)
    {
        power = p;
        p *= 5;
    }
    return powers;
}();

/// FloatingDecimal.N_5_BITS: approximately ceil(log2(5^i)).
constexpr std::array<int, 27> kN5Bits{0,  3,  5,  7,  10, 12, 14, 17, 19, 21, 24, 26, 28, 31,
                                      33, 35, 38, 40, 42, 45, 47, 49, 52, 54, 56, 59, 61};

/// FloatingDecimal.insignificantDigitsNumber: the number of decimal digits of 2^i below its
/// leading one.
constexpr std::array<int, 64> kInsignificantDigitsNumber{
    0,  0,  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  5,  5,  5,  6,  6,
    6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 11, 11, 11, 12, 12, 12, 12,
    13, 13, 13, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19};

/// FloatingDecimal.insignificantDigitsForPow2.
[[nodiscard]] int insignificantDigitsForPow2(int p2)
{
    if (p2 > 1 && std::cmp_less(p2, kInsignificantDigitsNumber.size()))
    {
        return kInsignificantDigitsNumber.at(static_cast<std::size_t>(p2));
    }
    return 0;
}

/// 5^n from kLong5Pow; dtoa's bit estimates keep n in range.
[[nodiscard]] std::int64_t long5Pow(int n)
{
    QTROCKET_ASSERT(n >= 0 && std::cmp_less(n, kLong5Pow.size()));
    return kLong5Pow.at(static_cast<std::size_t>(n));
}

/// N_5_BITS[n] where the table has it, 3n beyond (dtoa's "binary digits needed", approx.).
[[nodiscard]] int n5Bits(int n)
{
    return std::cmp_less(n, kN5Bits.size()) ? kN5Bits.at(static_cast<std::size_t>(n)) : n * 3;
}

// Java's long arithmetic wraps around on overflow, where C++'s signed arithmetic is undefined;
// dtoa's long path relies on the wrap-around (its "m might overflow" hack).
[[nodiscard]] std::int64_t wrapAdd(std::int64_t a, std::int64_t b)
{
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b));
}

[[nodiscard]] std::int64_t wrapSub(std::int64_t a, std::int64_t b)
{
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b));
}

[[nodiscard]] std::int64_t wrapMul(std::int64_t a, std::int64_t b)
{
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
}

[[nodiscard]] std::int64_t wrapShl(std::int64_t a, int shift)
{
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << static_cast<unsigned>(shift));
}

/// The non-negative big integers of dtoa's general path (FDBigInteger's role). Only exact results
/// matter there, so this keeps plain little-endian 32-bit words: no normalisation bias, and
/// quoRemIteration divides by repeated subtraction, the quotient being a single digit.
class BigInt
{
public:
    /// 5^p5 * 2^p2 (FDBigInteger.valueOfPow52).
    [[nodiscard]] static BigInt pow52(int p5, int p2) { return mulPow52(1, p5, p2); }

    /// value * 5^p5 * 2^p2 (FDBigInteger.valueOfMulPow52).
    [[nodiscard]] static BigInt mulPow52(std::uint64_t value, int p5, int p2)
    {
        BigInt result;
        result.m_words = {static_cast<std::uint32_t>(value),
                          static_cast<std::uint32_t>(value >> 32U)};
        result.trim();
        constexpr std::uint32_t kFivePow13 = 1220703125;  // the largest power of five below 2^32
        for (; p5 >= 13; p5 -= 13)
        {
            result.multiply(kFivePow13);
        }
        std::uint32_t rest = 1;
        for (; p5 > 0; --p5)
        {
            rest *= 5;
        }
        result.multiply(rest);
        result.shiftLeft(p2);
        return result;
    }

    [[nodiscard]] bool isZero() const noexcept { return m_words.empty(); }

    /// -1, 0 or 1 as this is below, equal to or above @p other.
    [[nodiscard]] int compare(const BigInt& other) const noexcept
    {
        if (m_words.size() != other.m_words.size())
        {
            return m_words.size() < other.m_words.size() ? -1 : 1;
        }
        for (std::size_t i = m_words.size(); i-- > 0;)
        {
            if (m_words[i] != other.m_words[i])
            {
                return m_words[i] < other.m_words[i] ? -1 : 1;
            }
        }
        return 0;
    }

    /// FDBigInteger.addAndCmp: compares this with @p x + @p y.
    [[nodiscard]] int addAndCompare(const BigInt& x, const BigInt& y) const
    {
        BigInt sum = x;
        sum.add(y);
        return compare(sum);
    }

    /// FDBigInteger.quoRemIteration: returns q = this / s, which is below 10, and leaves
    /// (this - q * s) * 10 here.
    [[nodiscard]] int quoRemIteration(const BigInt& s)
    {
        int q = 0;
        while (compare(s) >= 0)
        {
            subtract(s);
            ++q;
        }
        multiply(10);
        return q;
    }

    void multiply(std::uint32_t factor)
    {
        std::uint64_t carry = 0;
        for (std::uint32_t& word : m_words)
        {
            const std::uint64_t product = (static_cast<std::uint64_t>(word) * factor) + carry;
            word                        = static_cast<std::uint32_t>(product);
            carry                       = product >> 32U;
        }
        if (carry != 0)
        {
            m_words.push_back(static_cast<std::uint32_t>(carry));
        }
        trim();
    }

    void shiftLeft(int bits)
    {
        if (isZero() || bits <= 0)
        {
            return;
        }
        const auto                 wordShift = static_cast<std::size_t>(bits / 32);
        const auto                 bitShift  = static_cast<unsigned>(bits % 32);
        std::vector<std::uint32_t> shifted(m_words.size() + wordShift + 1, 0);
        for (std::size_t i = 0; i < m_words.size(); ++i)
        {
            const std::uint64_t word = static_cast<std::uint64_t>(m_words[i]) << bitShift;
            shifted[i + wordShift] |= static_cast<std::uint32_t>(word);
            shifted[i + wordShift + 1] |= static_cast<std::uint32_t>(word >> 32U);
        }
        m_words = std::move(shifted);
        trim();
    }

private:
    void add(const BigInt& other)
    {
        m_words.resize(std::max(m_words.size(), other.m_words.size()) + 1, 0);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < m_words.size(); ++i)
        {
            const std::uint64_t otherWord = i < other.m_words.size() ? other.m_words[i] : 0;
            const std::uint64_t sum       = m_words[i] + otherWord + carry;
            m_words[i]                    = static_cast<std::uint32_t>(sum);
            carry                         = sum >> 32U;
        }
        trim();
    }

    /// this -= other, which must not exceed this.
    void subtract(const BigInt& other)
    {
        std::uint64_t borrow = 0;
        for (std::size_t i = 0; i < m_words.size(); ++i)
        {
            const std::uint64_t otherWord = i < other.m_words.size() ? other.m_words[i] : 0;
            const std::uint64_t diff      = m_words[i] - otherWord - borrow;  // modulo 2^64
            m_words[i]                    = static_cast<std::uint32_t>(diff);
            borrow                        = (diff >> 32U) != 0 ? 1 : 0;
        }
        trim();
    }

    void trim()
    {
        while (!m_words.empty() && m_words.back() == 0)
        {
            m_words.pop_back();
        }
    }

    std::vector<std::uint32_t> m_words;
};

/// BinaryToASCIIBuffer.estimateDecExp: floor(log10(d)) or one more, from a linear approximation
/// of log10 of the significand (d2 in [1, 2)). The estimate decides whether dtoa discards a
/// leading zero, so it is computed as Java does, operation for operation (the build forbids
/// contracting it into fused multiply-adds).
[[nodiscard]] int estimateDecExp(std::uint64_t fractBits, int binExp)
{
    const auto   d2 = std::bit_cast<double>(kExpOne | (fractBits & kSignifBitMask));
    const double d  = ((d2 - 1.5) * 0.289529654) + 0.176091259 +
                      (static_cast<double>(binExp) * 0.301029995663981);
    // Java extracts floor(d) from its bits; the results are the same.
    return static_cast<int>(std::floor(d));
}

/// BinaryToASCIIBuffer.developLongDigits: the digits of @p lvalue, an integer, with
/// @p insignificantDigits low-order digits rounded away half-up and trailing zeros dropped.
void developLongDigits(BinaryToAscii& out, int decExponent, std::int64_t lvalue,
                       int insignificantDigits)
{
    if (insignificantDigits != 0)
    {
        // Discard non-significant low-order bits, while rounding, up to insignificant value.
        const std::int64_t pow10   = wrapShl(long5Pow(insignificantDigits), insignificantDigits);
        const std::int64_t residue = lvalue % pow10;
        lvalue /= pow10;
        decExponent += insignificantDigits;
        if (residue >= (pow10 / 2))
        {
            // round up based on the low-order bits we're discarding
            lvalue++;
        }
    }
    std::string reversed;  // the digits from the lowest, the trailing zeros skipped
    auto        c = static_cast<int>(lvalue % 10);
    lvalue /= 10;
    while (c == 0)
    {
        decExponent++;
        c = static_cast<int>(lvalue % 10);
        lvalue /= 10;
    }
    while (lvalue != 0)
    {
        reversed.push_back(static_cast<char>('0' + c));
        decExponent++;
        c = static_cast<int>(lvalue % 10);
        lvalue /= 10;
    }
    reversed.push_back(static_cast<char>('0' + c));
    out.digits.assign(reversed.rbegin(), reversed.rend());
    out.decimalExponent = decExponent + 1;
}

/// BinaryToASCIIBuffer.roundup: adds one to the last digit. A carry out of the leading digit makes
/// the digits "10...0" with a larger exponent and, as in Java, leaves digitsRoundedUp unset.
void roundup(BinaryToAscii& out)
{
    std::size_t i = out.digits.size() - 1;
    char        q = out.digits[i];
    if (q == '9')
    {
        while (q == '9' && i > 0)
        {
            out.digits[i] = '0';
            q             = out.digits[--i];
        }
        if (q == '9')
        {
            // carryout! High-order 1, rest 0s, larger exp.
            out.decimalExponent += 1;
            out.digits[0] = '1';
            return;
        }
    }
    out.digits[i]       = static_cast<char>(q + 1);
    out.digitsRoundedUp = true;
}

/// dtoa's scale factors: B = fractBits * 5^b5 * 2^b2, S = 5^s5 * 2^s2 and M = 5^m5 * 2^m2.
struct Scale
{
    int b5{0};
    int b2{0};
    int s5{0};
    int s2{0};
    int m5{0};
    int m2{0};
};

/// The digit generation's outcome before the last digit is rounded.
struct Generated
{
    bool         low{false};
    bool         high{false};
    std::int64_t lowDigitDifference{0};
};

/// dtoa's long path, taken when B and 10 * S fit a long (the int path before it cannot be reached
/// by a double: B / M >= 2^53 there, so B never fits 32 bits). The arithmetic wraps as Java's does.
[[nodiscard]] Generated generateWithLongs(BinaryToAscii& out, int& decExp, std::uint64_t fractBits,
                                          const Scale& scale, bool isCompatibleFormat)
{
    std::int64_t b =
        wrapShl(wrapMul(static_cast<std::int64_t>(fractBits), long5Pow(scale.b5)), scale.b2);
    const std::int64_t s    = wrapShl(long5Pow(scale.s5), scale.s2);
    std::int64_t       m    = wrapShl(long5Pow(scale.m5), scale.m2);
    const std::int64_t tens = wrapMul(s, 10);

    // Unroll the first iteration. If our decExp estimate was too high, our first quotient will be
    // zero. In this case, we discard it and decrement decExp.
    Generated g;
    auto      q = static_cast<int>(b / s);
    b           = wrapMul(10, b % s);
    m           = wrapMul(m, 10);
    g.low       = b < m;
    g.high      = wrapAdd(b, m) > tens;
    if (q == 0 && !g.high)
    {
        // oops. Usually ignore leading zero.
        decExp--;
    }
    else
    {
        out.digits.push_back(static_cast<char>('0' + q));
    }
    // HACK! Java spec sez that we always have at least one digit after the . in either F- or
    // E-form output. Thus we will need more than one digit if we're using E-form.
    if (!isCompatibleFormat || decExp < -3 || decExp >= 8)
    {
        g.high = false;
        g.low  = false;
    }
    while (!g.low && !g.high)
    {
        q = static_cast<int>(b / s);
        b = wrapMul(10, b % s);
        m = wrapMul(m, 10);
        if (m > 0)
        {
            g.low  = b < m;
            g.high = wrapAdd(b, m) > tens;
        }
        else
        {
            // hack -- m might overflow! In this case, it is certainly > b, which won't, and b+m >
            // tens, too, since that has overflowed either!
            g.low  = true;
            g.high = true;
        }
        out.digits.push_back(static_cast<char>('0' + q));
    }
    g.lowDigitDifference   = wrapSub(wrapShl(b, 1), tens);
    out.decimalDigitsExact = b == 0;
    return g;
}

/// dtoa's FDBigInteger path. Unlike the long path, it stops on B + M >= 10 * S (not >).
[[nodiscard]] Generated generateWithBigInts(BinaryToAscii& out, int& decExp,
                                            std::uint64_t fractBits, const Scale& scale,
                                            bool isCompatibleFormat)
{
    const BigInt sVal    = BigInt::pow52(scale.s5, scale.s2);
    BigInt       bVal    = BigInt::mulPow52(fractBits, scale.b5, scale.b2);
    BigInt       mVal    = BigInt::pow52(scale.m5 + 1, scale.m2 + 1);  // 10 * M
    const BigInt tenSVal = BigInt::pow52(scale.s5 + 1, scale.s2 + 1);  // 10 * S

    Generated g;
    int       q = bVal.quoRemIteration(sVal);
    g.low       = bVal.compare(mVal) < 0;
    g.high      = tenSVal.addAndCompare(bVal, mVal) <= 0;
    if (q == 0 && !g.high)
    {
        // oops. Usually ignore leading zero.
        decExp--;
    }
    else
    {
        out.digits.push_back(static_cast<char>('0' + q));
    }
    if (!isCompatibleFormat || decExp < -3 || decExp >= 8)
    {
        g.high = false;
        g.low  = false;
    }
    while (!g.low && !g.high)
    {
        q = bVal.quoRemIteration(sVal);
        mVal.multiply(10);
        g.low  = bVal.compare(mVal) < 0;
        g.high = tenSVal.addAndCompare(bVal, mVal) <= 0;
        out.digits.push_back(static_cast<char>('0' + q));
    }
    if (g.high && g.low)
    {
        bVal.shiftLeft(1);
        g.lowDigitDifference = bVal.compare(tenSVal);
    }
    out.decimalDigitsExact = bVal.isZero();
    return g;
}

/// BinaryToASCIIBuffer.dtoa for the positive value fractBits * 2^(binExp - 52), fractBits holding
/// its high-order bit at bit 52.
void dtoa(BinaryToAscii& out, int binExp, std::uint64_t fractBits, int nSignificantBits,
          bool isCompatibleFormat)
{
    const int tailZeros = std::countr_zero(fractBits);
    // number of significant bits of fractBits
    const int nFractBits = kExpShift + 1 - tailZeros;
    // number of significant bits to the right of the point
    const int nTinyBits = std::max(0, nFractBits - binExp - 1);
    // Java looks more closely at a small number to decide if, with scaling by 10^nTinyBits, the
    // result fits a long, but only uses that for an integer (nTinyBits == 0), which always fits.
    if (binExp <= kMaxSmallBinExp && binExp >= kMinSmallBinExp && nTinyBits == 0)
    {
        // Shift the binary point to the extreme right.
        const int insignificant = binExp > nSignificantBits
                                      ? insignificantDigitsForPow2(binExp - nSignificantBits - 1)
                                      : 0;
        if (binExp >= kExpShift)
        {
            fractBits <<= static_cast<unsigned>(binExp - kExpShift);
        }
        else
        {
            fractBits >>= static_cast<unsigned>(kExpShift - binExp);
        }
        developLongDigits(out, 0, static_cast<std::int64_t>(fractBits), insignificant);
        return;
    }

    // The hard case: large positive integers B and S and an integer decExp with
    // d = (B / S) * 10^decExp and 1 <= B / S < 10, and M = half an ULP of d scaled like B; the
    // digits are the quotients of B / S until the remainder is within M of either end.
    int   decExp = estimateDecExp(fractBits, binExp);
    Scale scale;
    scale.b5 = std::max(0, -decExp);
    scale.b2 = scale.b5 + nTinyBits + binExp;
    scale.s5 = std::max(0, decExp);
    scale.s2 = scale.s5 + nTinyBits;
    scale.m5 = scale.b5;
    scale.m2 = scale.b2 - nSignificantBits;

    fractBits >>= static_cast<unsigned>(tailZeros);
    scale.b2 -= nFractBits - 1;
    const int common2factor = std::min(scale.b2, scale.s2);
    scale.b2 -= common2factor;
    scale.s2 -= common2factor;
    scale.m2 -= common2factor;
    // HACK!! For exact powers of two, the next smallest number is only half as far away as we
    // think (because the meaning of ULP changes at power-of-two bounds) for this reason, we hack
    // M2. Hope this works.
    if (nFractBits == 1)
    {
        scale.m2 -= 1;
    }
    if (scale.m2 < 0)
    {
        // oops. since we cannot scale M down far enough, we must scale the other values up.
        scale.b2 -= scale.m2;
        scale.s2 -= scale.m2;
        scale.m2 = 0;
    }

    // binary digits needed to represent B, and 10*S, approx.
    const int       bBits    = nFractBits + scale.b2 + n5Bits(scale.b5);
    const int       tenSBits = scale.s2 + 1 + n5Bits(scale.s5 + 1);
    const Generated g =
        bBits < 64 && tenSBits < 64
            ? generateWithLongs(out, decExp, fractBits, scale, isCompatibleFormat)
            : generateWithBigInts(out, decExp, fractBits, scale, isCompatibleFormat);
    out.decimalExponent = decExp + 1;

    // Last digit gets rounded based on stopping condition.
    if (g.high)
    {
        if (g.low)
        {
            if (g.lowDigitDifference == 0)
            {
                // it's a tie! choose based on which digits we like.
                if (((out.digits.back() - '0') % 2) != 0)
                {
                    roundup(out);
                }
            }
            else if (g.lowDigitDifference > 0)
            {
                roundup(out);
            }
        }
        else
        {
            roundup(out);
        }
    }
}

}  // namespace

BinaryToAscii binaryToAscii(double value, bool compatibleFormat)
{
    QTROCKET_ASSERT(std::isfinite(value));
    const auto    dBits = std::bit_cast<std::uint64_t>(value);
    BinaryToAscii out;
    out.negative                   = (dBits & kSignBitMask) != 0;
    std::uint64_t fractBits        = dBits & kSignifBitMask;
    auto          binExp           = static_cast<int>((dBits & kExpBitMask) >> kExpShift);
    int           nSignificantBits = 0;
    if (binExp == 0)
    {
        if (fractBits == 0)
        {
            // not a denorm, just a 0!
            out.digits = "0";
            return out;
        }
        // Normalize the subnormal: its high-order bit moves to bit 52.
        const int leadingZeros = std::countl_zero(fractBits);
        const int shift        = leadingZeros - (63 - kExpShift);
        fractBits <<= static_cast<unsigned>(shift);
        binExp           = 1 - shift;
        nSignificantBits = 64 - leadingZeros;
    }
    else
    {
        fractBits |= kFractHob;
        nSignificantBits = kExpShift + 1;
    }
    binExp -= kExpBias;
    dtoa(out, binExp, fractBits, nSignificantBits, compatibleFormat);
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
    // BinaryToASCIIBuffer.getChars
    const std::string& digits      = converted.digits;
    const auto         nDigits     = static_cast<int>(digits.size());
    const int          decExponent = converted.decimalExponent;
    std::string        result      = converted.negative ? "-" : "";
    if (decExponent > 0 && decExponent < 8)
    {
        // print digits.digits.
        const int charLength = std::min(nDigits, decExponent);
        result += digits.substr(0, static_cast<std::size_t>(charLength));
        if (charLength < decExponent)
        {
            result.append(static_cast<std::size_t>(decExponent - charLength), '0');
            result += ".0";
        }
        else
        {
            result += '.';
            result += charLength < nDigits ? digits.substr(static_cast<std::size_t>(charLength))
                                           : std::string("0");
        }
    }
    else if (decExponent <= 0 && decExponent > -3)
    {
        result += "0.";
        result.append(static_cast<std::size_t>(-decExponent), '0');
        result += digits;
    }
    else
    {
        result += digits.front();
        result += '.';
        result += nDigits > 1 ? digits.substr(1) : std::string("0");
        // Java writes the sign and the magnitude of decExponent - 1 separately; same text.
        result += std::format("E{}", decExponent - 1);
    }
    return result;
}

}  // namespace QtRocket::FloatingDecimal
