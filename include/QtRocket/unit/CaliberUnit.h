#pragma once

#include <functional>
#include <memory>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// Calibers (OpenRocket's CaliberUnit): lengths as multiples of a rocket's reference length, the
/// largest body diameter. OpenRocket reads that length from a Rocket or FlightConfiguration and
/// caches it by their modification ids; the rocket model is not ported yet, so the reference
/// comes from a provider function that is evaluated on every conversion, or is a constant.
/// (CaliberUnit.calculateCaliber, which walks the rocket's SymmetricComponents and falls back to
/// kDefaultCaliber below 0.1 mm, arrives with the rocket model as such a provider.)
class CaliberUnit : public GeneralUnit
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
    [[nodiscard]] double                toUnit(double value) const override;
    [[nodiscard]] std::unique_ptr<Unit> clone() const override;

private:
    std::function<double()> m_referenceLengthProvider;
};

}  // namespace QtRocket
