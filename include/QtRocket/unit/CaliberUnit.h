#pragma once

#include <functional>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Calibers (OpenRocket's CaliberUnit): lengths as multiples of a rocket's reference length, the
/// largest body diameter. The reference comes from a provider function that is evaluated on
/// every conversion, or is a constant. OpenRocket's unit reads it from a Rocket or a
/// FlightConfiguration, which the unit subsystem sits below: rocket/StabilityUnits.h has that
/// form as a provider (CaliberUnit.calculateCaliber(), which walks the SymmetricComponents and
/// falls back to kDefaultCaliber below 0.1 mm, cached by the modification ids as in Java).
class CaliberUnit final : public GeneralUnit
{
public:
    /// The caliber assumed when a rocket has no symmetric component (0.01 m).
    static constexpr double kDefaultCaliber = 0.01;

    /// No reference length (OpenRocket's `new CaliberUnit((Rocket) null)`, the placeholder in
    /// UNITS_STABILITY): converting a value throws BugError until a StabilityUnitGroup replaces
    /// the unit with one that has a reference.
    CaliberUnit();
    /// A constant reference length.
    /// @throws BugError when @p reference is not positive
    explicit CaliberUnit(double reference);
    /// A reference length read from @p referenceLengthProvider on every conversion; an empty
    /// provider is the same as no reference.
    explicit CaliberUnit(std::function<double()> referenceLengthProvider);

    /// True when conversions have a reference length to use.
    [[nodiscard]] bool hasReference() const noexcept
    {
        return m_referenceLengthProvider != nullptr;
    }
    /// The current reference length. @throws BugError without a reference.
    [[nodiscard]] double getReferenceLength() const;

    /// value * reference. @throws BugError without a reference.
    [[nodiscard]] double fromUnit(double value) const override;
    /// value / reference. @throws BugError without a reference.
    [[nodiscard]] double toUnit(double value) const override;

private:
    std::function<double()> m_referenceLengthProvider;
};

}  // namespace QtRocket
