#pragma once

#include <string>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/DecimalFormat.h"

namespace QtRocket
{

/// A unit shown with a fixed precision (OpenRocket's FixedPrecisionUnit): values are rounded to
/// multiples of @p precision and formatted with as many decimals as the precision has.
class FixedPrecisionUnit : public Unit
{
public:
    /// Multiplier 1, trailing zeros shown.
    FixedPrecisionUnit(std::string unit, double precision);
    FixedPrecisionUnit(std::string unit, double precision, double multiplier);
    /// @param precision            the step of round(); its decimals set the display decimals
    ///                             (0.01 gives two, 1 gives none, 10e-6 gives five)
    /// @param displayTrailingZeros show every decimal ("1.50") or drop trailing zeros ("1.5")
    /// @throws BugError when @p multiplier is 0
    FixedPrecisionUnit(std::string unit, double precision, double multiplier,
                       bool displayTrailingZeros);
    ~FixedPrecisionUnit() override = default;

    [[nodiscard]] double getPrecision() const noexcept { return m_precision; }
    /// The number of decimals shown, derived from the precision.
    [[nodiscard]] int  getDecimals() const noexcept { return m_decimals; }
    [[nodiscard]] bool displaysTrailingZeros() const noexcept { return m_displayTrailingZeros; }

    /// round(value + precision).
    [[nodiscard]] double getNextValue(double value) const override;
    /// round(value - precision).
    [[nodiscard]] double getPreviousValue(double value) const override;
    /// The closest multiple of the precision, ties toward positive infinity as Java's Math.round
    /// (1.05 rounds to 1.1 for precision 0.1, -0.05 to 0); NaN gives 0.
    [[nodiscard]] double round(double value) const override;

    /// The value in this unit with the fixed number of decimals: Java's "%.Nf" (half-up on the
    /// decimal digits, "NaN", "Infinity") with trailing zeros, DecimalFormat("0.##") with as many
    /// '#' as decimals (half to even, "NaN", U+221E) without. Unlike Unit::toString, NaN is not
    /// "N/A" here, though toStringUnit() still returns "N/A" for it.
    [[nodiscard]] std::string toString(double value) const override;

    [[nodiscard]] std::vector<Tick> getTicks(double start, double end, double minor,
                                             double major) const override;

protected:
    /// Copying is for subclasses only, so that a subclass (a CaliberUnit, say) cannot be sliced
    /// into a plain FixedPrecisionUnit by accident.
    FixedPrecisionUnit(const FixedPrecisionUnit&)            = default;
    FixedPrecisionUnit& operator=(const FixedPrecisionUnit&) = default;
    FixedPrecisionUnit(FixedPrecisionUnit&&)                 = default;
    FixedPrecisionUnit& operator=(FixedPrecisionUnit&&)      = default;

private:
    double        m_precision;
    int           m_decimals;
    bool          m_displayTrailingZeros;
    DecimalFormat m_formatter;  ///< "0.##" without trailing zeros (OpenRocket builds it always)
};

}  // namespace QtRocket
