#pragma once

#include <memory>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Inches (OpenRocket's InchUnit): a GeneralUnit that always keeps three decimals in
/// toString() ("25.125") and steps by a fixed precision.
class InchUnit : public GeneralUnit
{
public:
    /// Precision 1 inch.
    InchUnit(double multiplier, std::string unit);
    /// @param precision the step of getNextValue() and getPreviousValue(), in inches
    InchUnit(double multiplier, std::string unit, double precision);

    [[nodiscard]] double getPrecision() const noexcept { return m_precision; }

    [[nodiscard]] double                getNextValue(double value) const override;
    [[nodiscard]] double                getPreviousValue(double value) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;

protected:
    /// Rounds to three decimals, half to even.
    [[nodiscard]] double roundForDecimalFormat(double val) const override;

private:
    double m_precision;
};

}  // namespace QtRocket
