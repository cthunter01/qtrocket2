#pragma once

#include <string>
#include <string_view>

namespace QtRocket
{

/// java.text.DecimalFormat for the number patterns OpenRocket's units use ("#", "0", "0.0",
/// "0.#", "0.0##", "#.###", "0.00", "0.##", "0.00E0", ...), formatting doubles digit for digit as
/// JDK 17 to 25 do. It starts from the digits of FloatingDecimal's conversion (not the shortest
/// ones, see FloatingDecimal.h) and rounds them half to even, telling from the conversion whether
/// a final 5 lies above, below or exactly at the double: with "0.#", 0.15 gives "0.1" (the double
/// is just below 0.15), 0.45 gives "0.5" (just above) and 0.25 "0.2" (exact, to even); with
/// "0.00E0", 1235000 gives "1.24E6" (an integer below 2^63 is never marked exact, so its ties
/// round up); with "#", 2^69 gives "590295810358705650000".
///
/// A negative value that rounds to zero keeps its sign ("-0", "-0.0"), as does -0.0; NaN is "NaN"
/// and the infinities "∞" (U+221E) and "-∞". The symbols are those of Locale.US (a point, no
/// grouping) whatever the process locale; OpenRocket follows the default locale.
///
/// Supported pattern syntax: integer digits '#'* '0'*, optionally a '.' and fraction digits
/// '0'* '#'*, optionally "E" and one or more '0' for the exponent. Prefixes, suffixes, grouping,
/// percent and negative subpatterns are not supported.
class DecimalFormat
{
public:
    /// @throws BugError for a pattern that is malformed (Java: IllegalArgumentException) or uses
    ///         syntax outside the supported subset
    explicit DecimalFormat(std::string_view pattern);

    /// DecimalFormat.format(double).
    [[nodiscard]] std::string format(double value) const;

    [[nodiscard]] int  getMinimumIntegerDigits() const noexcept { return m_minIntegerDigits; }
    [[nodiscard]] int  getMaximumIntegerDigits() const noexcept { return m_maxIntegerDigits; }
    [[nodiscard]] int  getMinimumFractionDigits() const noexcept { return m_minFractionDigits; }
    [[nodiscard]] int  getMaximumFractionDigits() const noexcept { return m_maxFractionDigits; }
    [[nodiscard]] bool usesExponentialNotation() const noexcept { return m_useExponentialNotation; }

private:
    int  m_minIntegerDigits{1};
    int  m_maxIntegerDigits{0};
    int  m_minFractionDigits{0};
    int  m_maxFractionDigits{0};
    bool m_useExponentialNotation{false};
    int  m_minExponentDigits{0};
    bool m_decimalSeparatorAlwaysShown{false};
};

}  // namespace QtRocket
