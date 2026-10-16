#pragma once

#include <string>

/// The binary-to-decimal conversion of JDK 17, digit for digit: the digits its Double.toString
/// prints (as up to JDK 18) and the digits its java.text.DecimalFormat starts from (as up to JDK 25
/// at least). It is not a shortest-digits conversion (JDK-4511638). Where it departs from one, as
/// FloatingDecimal.cpp describes in detail, with the values that show it in a JDK 17 run:
/// - an integer below 2^63 is written from its exact value, rounded half up from 2^58 on to the
///   largest power of ten not above a quarter of the gap between neighbouring doubles
///   ("1.15292150460684698E18" for 2^60);
/// - a value exactly a margin below a shorter decimal keeps its longer digits when JDK 17 computes
///   in 64-bit integers ("9.999999999999999E22" for 1e23), and that 64-bit test wraps around for
///   some values between 2^84 and 2^85;
/// - below a power of two both margins are a quarter of the gap above, so 2^69 gets a digit more
///   than it needs ("5.9029581035870565E20");
/// - in E-form a second digit is always generated ("4.9E-324" for the smallest subnormal);
/// - the decimal exponent is estimated, and when the estimate is one too high for a value within
///   its margin below a power of ten, the digits come from a leading zero rounded up, which shows
///   in digitsRoundedUp.
/// Strings::javaDoubleToString and Strings::formatFixed follow the shortest digits of JDK 19+
/// instead, except for those integers below 2^63.
namespace QtRocket::FloatingDecimal
{

/// The result of a conversion: the value is 0.<digits> x 10^decimalExponent.
struct BinaryToAscii
{
    bool negative{false};
    /// One to nineteen digits, the first one non-zero, with the trailing zeros the conversion
    /// produced (a forced second digit, a carry). Zero is "0" with decimalExponent 0.
    std::string digits;
    int         decimalExponent{0};
    /// The last digit was raised by one after the digit generation, so the value lies below the
    /// digits. Not set when that carry ran out of the first digit, nor for an integer below 2^63,
    /// whose low-order digits are rounded away without it.
    bool digitsRoundedUp{false};
    /// The digits are the value's exact decimal expansion. Never set for an integer below 2^63,
    /// although it is written exactly.
    bool decimalDigitsExact{false};
};

/// Converts a finite @p value, its sign included. The compatible format, which Double.toString and
/// DecimalFormat use, forces a second digit only in E-form (below 10^-3 and from 10^8); with it
/// off, as java.util.Formatter converts, the second digit is always forced ("50" for 0.5, where the
/// compatible format gives "5").
/// @throws BugError for NaN or an infinity, which have no digits
[[nodiscard]] BinaryToAscii binaryToAscii(double value, bool compatibleFormat = true);

/// JDK 17's Double.toString(value): "NaN", "Infinity", "-Infinity", "0.0", "-0.0"; from 0.001 up to
/// but excluding 10^7 the digits with a point and at least one digit after it ("980.0", "0.001"),
/// otherwise one digit, a point, at least one more digit, "E" and the exponent ("1.0E7",
/// "9.999999999999999E22" for 1e23).
[[nodiscard]] std::string toJavaFormatString(double value);

/// toJavaFormatString() of an existing conversion.
[[nodiscard]] std::string toJavaFormatString(const BinaryToAscii& converted);

}  // namespace QtRocket::FloatingDecimal
