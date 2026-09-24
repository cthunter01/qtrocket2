#pragma once

#include <string_view>

/// Non-ASCII characters used in unit names and labels (OpenRocket's Chars), as UTF-8 strings. Each
/// is written as its UTF-8 bytes so the encoding does not depend on the compiler's execution
/// character set.
namespace QtRocket::Chars
{

/// The fraction 1/2, U+00BD.
inline constexpr std::string_view kFrac12 = "\xC2\xBD";
/// The fraction 1/4, U+00BC.
inline constexpr std::string_view kFrac14 = "\xC2\xBC";
/// The fraction 3/4, U+00BE.
inline constexpr std::string_view kFrac34 = "\xC2\xBE";
/// Fraction slash, U+2044.
inline constexpr std::string_view kFraction = "\xE2\x81\x84";

/// Degree sign, U+00B0.
inline constexpr std::string_view kDegree = "\xC2\xB0";

/// Squared, superscript 2, U+00B2.
inline constexpr std::string_view kSquared = "\xC2\xB2";
/// Cubed, superscript 3, U+00B3.
inline constexpr std::string_view kCubed = "\xC2\xB3";

/// Per mille sign, U+2030.
inline constexpr std::string_view kPermille = "\xE2\x80\xB0";

/// Middle dot, multiplication, U+00B7.
inline constexpr std::string_view kDot = "\xC2\xB7";
/// Multiplication sign, cross, U+00D7.
inline constexpr std::string_view kTimes = "\xC3\x97";

/// No-break space, U+00A0.
inline constexpr std::string_view kNbsp = "\xC2\xA0";
/// Zero-width space, U+200B.
inline constexpr std::string_view kZwsp = "\xE2\x80\x8B";

/// Em dash, U+2014.
inline constexpr std::string_view kEmDash = "\xE2\x80\x94";

/// Micro sign (Greek letter mu), U+00B5.
inline constexpr std::string_view kMicro = "\xC2\xB5";

/// Greek small letter alpha, U+03B1.
inline constexpr std::string_view kAlpha = "\xCE\xB1";
/// Greek capital letter theta, U+0398.
inline constexpr std::string_view kTheta = "\xCE\x98";

/// Copyright sign, U+00A9.
inline constexpr std::string_view kCopy = "\xC2\xA9";
/// Bullet, U+2022.
inline constexpr std::string_view kBullet = "\xE2\x80\xA2";

/// Leftwards arrow, U+2190.
inline constexpr std::string_view kLeftArrow = "\xE2\x86\x90";
/// Rightwards arrow, U+2192.
inline constexpr std::string_view kRightArrow = "\xE2\x86\x92";
/// Upwards arrow, U+2191.
inline constexpr std::string_view kUpArrow = "\xE2\x86\x91";

}  // namespace QtRocket::Chars
