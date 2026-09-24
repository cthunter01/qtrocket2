#pragma once

#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Radians (OpenRocket's RadianUnit): multiplier 1, values rounded to tenths and always shown
/// with one decimal ("1.6", "0.0").
class RadianUnit final : public GeneralUnit
{
public:
    RadianUnit();

    /// The closest tenth, half to even.
    [[nodiscard]] double round(double v) const override;
    /// DecimalFormat("0.0") of the value; NaN is "NaN", not "N/A".
    [[nodiscard]] std::string toString(double value) const override;
};

}  // namespace QtRocket
