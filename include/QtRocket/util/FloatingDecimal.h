#pragma once

#include <string>

/// The binary-to-decimal conversion of Java's jdk.internal.math.FloatingDecimal
/// (BinaryToASCIIBuffer.dtoa), which java.text.DecimalFormat takes its digits from in JDK 17 to 25
/// at least, and which Double.toString printed up to JDK 18. It is not a shortest-digits algorithm
/// (JDK-4511638): an integer below 2^63 keeps its exact digits down to a double's precision
/// ("1.15292150460684698E18" for 2^60), a power of two or a value near a tie can get a digit more
/// than needed ("5.9029581035870565E20" for 2^69, whose shortest form is "5.902958103587057E20";
/// "9.999999999999999E22" for 1e23), and in E-form the general path always generates a second
/// digit ("4.9E-324" for Double.MIN_VALUE). Strings::javaDoubleToString and Strings::formatFixed
/// follow the shortest digits of JDK 19+ instead, except for those integers below 2^63.
namespace QtRocket::FloatingDecimal
{

/// The state of a BinaryToASCIIBuffer after dtoa(): the value is 0.<digits> x 10^decimalExponent.
struct BinaryToAscii
{
    bool negative{false};
    /// The generated digits, one to nineteen, trailing zeros included when dtoa generated them.
    /// Zero is "0" with decimalExponent 0.
    std::string digits;
    int         decimalExponent{0};
    /// digitsRoundedUp(): the last digit was incremented after the digit generation (the value is
    /// below the digits). Not set when the increment carried out of the leading digit, nor on the
    /// exact path for integers below 2^63, which rounds low-order digits away without saying so.
    bool digitsRoundedUp{false};
    /// decimalDigitsExact(): the digits are the value's exact decimal expansion. Only the general
    /// path sets it; an integer below 2^63 is converted exactly but never flagged.
    bool decimalDigitsExact{false};
};

/// FloatingDecimal.getBinaryToASCIIConverter(value, compatibleFormat) for a finite value, the sign
/// included. The compatible format, Double.toString's and DecimalFormat's, generates a second
/// digit only in E-form (below 10^-3 and from 10^7); java.util.Formatter converts with it off,
/// which generates a second digit always ("50" for 0.5 where the compatible format gives "5").
/// @throws BugError for NaN or an infinity (FloatingDecimal returns a fixed text for them, no
///         digits)
[[nodiscard]] BinaryToAscii binaryToAscii(double value, bool compatibleFormat = true);

/// BinaryToASCIIConverter.toJavaFormatString(), JDK 17's Double.toString(value): "NaN",
/// "Infinity", "-Infinity", "0.0", "-0.0"; from 0.001 up to but excluding 10^7 the digits with a
/// point and at least one fraction digit ("980.0", "0.001"), otherwise one digit, a point, at least
/// one more digit, "E" and the exponent ("1.0E7", "9.999999999999999E22" for 1e23).
[[nodiscard]] std::string toJavaFormatString(double value);

/// toJavaFormatString() of an existing conversion, as DigitList parses it back.
[[nodiscard]] std::string toJavaFormatString(const BinaryToAscii& converted);

}  // namespace QtRocket::FloatingDecimal
