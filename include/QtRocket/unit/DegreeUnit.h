#pragma once

#include <memory>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Degrees of angle (OpenRocket's DegreeUnit): pi/180 radians, named with the degree sign, no
/// space before the name, values rounded to whole degrees and shown with at most one decimal
/// ("45", "45.2", "-0" for a tiny negative angle).
class DegreeUnit : public GeneralUnit
{
public:
    DegreeUnit();

    [[nodiscard]] bool hasSpace() const override;
    /// The closest whole number, half to even.
    [[nodiscard]] double round(double v) const override;
    /// DecimalFormat("0.#") of the value in degrees; NaN is "NaN", not "N/A".
    [[nodiscard]] std::string           toString(double value) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;
};

}  // namespace QtRocket
