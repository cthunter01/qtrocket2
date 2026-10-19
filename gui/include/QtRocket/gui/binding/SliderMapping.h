#pragma once

namespace QtRocket
{
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// How a slider's integer position 0 .. kMaximum maps to a value and back (Java: DoubleModel's
/// ValueSliderModel, DoubleModel.java:207-523). The value is the binding's (the property times
/// the multiplier, in SI units); the unit is the binding's current one, which rounds the value a
/// position gives and inverts the slider when its multiplier is negative.
///
/// Linear: from min at 0 to max at kMaximum. Quadratic: linear from min at 0 to mid at the
/// position p * kMaximum, then a parabola to max at kMaximum, with the value and the slope
/// continuous at p: with d = (mid - min) / p,
///     q2 = (max - mid - d + d p) / (p - 1)^2
///     q1 = (d + 2 (mid - max) p - d p^2) / (p - 1)^2
///     q0 = (mid - (2 mid + d) p + (max + d) p^2) / (p - 1)^2
/// The arithmetic is Java's, operation for operation (+ - * / and sqrt, the unit's round()), so
/// the results are bit for bit OpenRocket's.
///
/// A dynamic bound (Java: a DoubleModel given as min or max) is the owner's to follow: when it
/// changes, the owner calls setBounds(), which does what Java's slider model does when it hears
/// of the change.
class SliderMapping
{
public:
    /// The number of steps of the slider (Java: ValueSliderModel.MAX).
    static constexpr int kMaximum = 1000;

    /// Linear from @p min to @p max (Java: getSliderModel(min, max), in its four forms of constant
    /// and dynamic bounds).
    [[nodiscard]] static SliderMapping linear(double min, double max) noexcept;

    /// Linear from @p min to @p mid up to @p linearPosition, quadratic beyond (Java:
    /// getSliderModel(min, pos, mid, max), and with @p linearPosition 0.5 getSliderModel(min, mid,
    /// DoubleModel max)).
    /// @throws BugError unless min < mid <= max and 0 < linearPosition < 1 (Java:
    ///         IllegalArgumentException)
    [[nodiscard]] static SliderMapping quadratic(double min, double linearPosition, double mid,
                                                 double max);

    /// Java's getSliderModel(min, mid, max): linear from @p min to @p max when @p mid is their
    /// centre (MathUtil::equals()), otherwise quadratic with the linear part up to the middle
    /// position.
    /// @throws BugError when it is quadratic and min < mid <= max does not hold
    [[nodiscard]] static SliderMapping fromRange(double min, double mid, double max);

    [[nodiscard]] bool   isLinear() const noexcept { return m_linear; }
    [[nodiscard]] double getMin() const noexcept { return m_min; }
    [[nodiscard]] double getMid() const noexcept { return m_mid; }
    [[nodiscard]] double getMax() const noexcept { return m_max; }
    /// Where the linear part ends, 0 .. 1 (1 for a linear mapping).
    [[nodiscard]] double getLinearPosition() const noexcept { return m_linearPosition; }

    /// The slider position of @p value (Java: ValueSliderModel.getValue()): 0 at or below min,
    /// kMaximum at or above max (the other way round when @p unit inverts), otherwise truncated
    /// towards zero; a NaN position becomes 0 (Java's (int) of NaN, MathUtil::javaIntCast()).
    [[nodiscard]] int positionOf(double value, const Unit& unit) const;

    /// The value at the slider position @p position (Java: ValueSliderModel.setValue(int), up to
    /// the call of DoubleModel.setValue()): the mapped value rounded in @p unit,
    /// unit.fromUnit(unit.round(unit.toUnit(v))). The binding clamps it to its bounds.
    [[nodiscard]] double valueAt(int position, const Unit& unit) const;

    /// A dynamic bound changed to @p min and @p max (Java: ValueSliderModel.stateChanged()). A
    /// linear mapping takes them; a quadratic one takes them too and puts mid at (max - min) / 3,
    /// as Java does (no dialog of OpenRocket gives a quadratic slider a bound that changes).
    void setBounds(double min, double max);

private:
    SliderMapping(bool linear, double min, double linearPosition, double mid, double max) noexcept;
    void updateQuadraticParameters() noexcept;

    bool   m_linear;
    double m_min;
    double m_linearPosition;
    double m_mid;
    double m_max;
    double m_quad2{0};
    double m_quad1{0};
    double m_quad0{0};
};

}  // namespace QtRocket::Gui
