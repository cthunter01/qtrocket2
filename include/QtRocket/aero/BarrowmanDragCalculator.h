#pragma once

namespace QtRocket
{

/// The drag half of the extended Barrowman method (OpenRocket's
/// aerodynamics/BarrowmanDragCalculator). For now it holds only the two pressure coefficients of
/// Java's static calculateStagnationCD() and calculateBaseCD(), which the component calculators
/// of aero/barrowman/ and their tests need.
///
/// Java's BarrowmanCalculator has static functions of the same names that only forward to these;
/// they are not ported: call these directly.
// HOOK(barrowman): tier 7b completes this class as the DragCalculator implementation
class BarrowmanDragCalculator
{
public:
    /// The stagnation pressure drag coefficient at Mach @p mach: 0.85 times the pressure ratio
    /// 1 + M^2 / 4 + M^4 / 40 up to Mach 1 (inclusive), and
    /// 1.84 - 0.76 / M^2 + 0.166 / M^4 + 0.035 / M^6 above. A NaN gives NaN.
    [[nodiscard]] static double calculateStagnationCD(double mach) noexcept;

    /// The base pressure drag coefficient at Mach @p mach: 0.12 + 0.13 M^2 up to Mach 1
    /// (inclusive), 0.25 / M above. A NaN gives NaN.
    [[nodiscard]] static double calculateBaseCD(double mach) noexcept;
};

}  // namespace QtRocket
