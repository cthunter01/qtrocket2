#pragma once

#include <functional>
#include <memory>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Lengths as a percentage of a rocket's aerodynamic length (OpenRocket's
/// PercentageOfLengthUnit), the secondary stability unit. As CaliberUnit, the reference length
/// comes from a provider evaluated on every conversion, or is a constant, until the rocket model
/// exists (OpenRocket reads FlightConfiguration.getLengthAerodynamic()).
class PercentageOfLengthUnit : public GeneralUnit
{
public:
    /// No reference length (OpenRocket's `new PercentageOfLengthUnit((Rocket) null)`, the
    /// placeholder in UNITS_STABILITY): converting throws BugError until a StabilityUnitGroup
    /// replaces the unit.
    PercentageOfLengthUnit();
    /// A constant reference length.
    /// @throws std::invalid_argument when @p reference is not positive
    explicit PercentageOfLengthUnit(double reference);
    /// A reference length read from @p referenceLengthProvider on every conversion; an empty
    /// provider is the same as no reference.
    explicit PercentageOfLengthUnit(std::function<double()> referenceLengthProvider);

    [[nodiscard]] bool hasReference() const noexcept
    {
        return m_referenceLengthProvider != nullptr;
    }
    /// The current reference length. @throws BugError without a reference.
    [[nodiscard]] double getReferenceLength() const;

    /// value * reference * multiplier. @throws BugError without a reference.
    [[nodiscard]] double fromUnit(double value) const override;
    /// value / reference / multiplier. @throws BugError without a reference.
    [[nodiscard]] double                toUnit(double value) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;

private:
    std::function<double()> m_referenceLengthProvider;
};

}  // namespace QtRocket
