#pragma once

#include <memory>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// A frequency shown for an SI value that is a period in seconds (OpenRocket's FrequencyUnit):
/// toUnit() inverts the value before scaling, so 0.5 s reads as 2 Hz. OpenRocket notes that it
/// does not use this unit and has not tested it extensively.
class FrequencyUnit : public GeneralUnit
{
public:
    /// @throws std::invalid_argument when @p multiplier is 0
    FrequencyUnit(double multiplier, std::string unit);

    /// (1 / value) / multiplier.
    [[nodiscard]] double toUnit(double value) const override;
    /// 1 / (value * multiplier).
    [[nodiscard]] double                fromUnit(double value) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;
};

}  // namespace QtRocket
