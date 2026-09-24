#pragma once

#include <memory>
#include <string>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// The everyday unit (OpenRocket's GeneralUnit): a plain multiplier, Unit's formatting, and
/// rounding to a number of significant digits (two by default), or to a fraction of the unit
/// (a tenth by default) for values below 10^(significantNumbers - 1).
///
/// getNextValue() and getPreviousValue() are OpenRocket's stubs: they step by one. The
/// significant-number count and the step value are stored but, as in OpenRocket, only the count
/// takes part in round(); GeneralUnit.getTicks(start, end, scale), an unfinished method that
/// returns an empty list, is not ported.
class GeneralUnit : public Unit
{
public:
    /// Two significant digits, decimal rounding to tenths, step 1.
    GeneralUnit(double multiplier, std::string unit);
    GeneralUnit(double multiplier, std::string unit, int significantNumbers);
    GeneralUnit(double multiplier, std::string unit, int significantNumbers, int decimalRounding);
    /// @param significantNumbers  digits kept by round() for large values; must be positive
    /// @param decimalRounding     round() keeps 1/decimalRounding steps below the significant
    ///                            limit; must be positive
    /// @param stepValue           the step of a spinner in this unit (unused by OpenRocket too)
    /// @throws BugError when @p multiplier is 0, or when the count or the rounding is not
    ///         positive (OpenRocket asserts them, which its runtime does not check)
    GeneralUnit(double multiplier, std::string unit, int significantNumbers, int decimalRounding,
                double stepValue);

    [[nodiscard]] int    getSignificantNumbers() const noexcept { return m_significantNumbers; }
    [[nodiscard]] int    getDecimalRounding() const noexcept { return m_decimalRounding; }
    [[nodiscard]] double getStepValue() const noexcept { return m_stepValue; }

    /// Below 10^(significantNumbers - 1): to the closest 1/decimalRounding (half to even).
    /// Otherwise to significantNumbers significant digits. Deviation: OpenRocket loops forever
    /// on an infinite value; it is returned unchanged here.
    [[nodiscard]] double                round(double value) const override;
    [[nodiscard]] double                getNextValue(double value) const override;
    [[nodiscard]] double                getPreviousValue(double value) const override;
    [[nodiscard]] std::vector<Tick>     getTicks(double start, double end, double minor,
                                                 double major) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;

private:
    int    m_significantNumbers;
    int    m_decimalRounding;
    double m_stepValue;
    /// 10^(significantNumbers - 1): values below it are rounded decimally.
    double m_decimalLimit;
    /// 10^significantNumbers.
    double m_significantNumbersLimit;
};

}  // namespace QtRocket
