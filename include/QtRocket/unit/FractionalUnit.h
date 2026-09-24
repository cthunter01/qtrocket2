#pragma once

#include <memory>
#include <string>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

/// A unit shown as whole numbers and fractions (OpenRocket's FractionalUnit), such as inches in
/// 64ths: "7 ⁷⁄₈" for 7 7/8, with superscript numerator, the fraction slash
/// (Chars::kFraction) and subscript denominator. A value farther than @p epsilon from a
/// representable fraction is shown as a decimal instead.
class FractionalUnit : public Unit
{
public:
    /// Epsilon 0.1 / fractionBase.
    FractionalUnit(double multiplier, std::string unit, std::string unitLabel, int fractionBase,
                   double incrementValue);
    /// @param unit           the unit's name, e.g. "in/64"
    /// @param unitLabel      what toStringUnit() appends instead of the name, e.g. "in"
    /// @param fractionBase   the denominator of the finest fraction, e.g. 64
    /// @param incrementValue the step of getNextValue() and getPreviousValue()
    /// @param epsilon        the largest distance from a fraction still shown as that fraction
    /// @throws BugError when @p multiplier is 0
    FractionalUnit(double multiplier, std::string unit, std::string unitLabel, int fractionBase,
                   double incrementValue, double epsilon);

    [[nodiscard]] const std::string& getUnitLabel() const noexcept { return m_unitLabel; }
    [[nodiscard]] int                getFractionBase() const noexcept { return m_fractionBase; }
    [[nodiscard]] double             getIncrementValue() const noexcept { return m_incrementValue; }
    [[nodiscard]] double             getEpsilon() const noexcept { return m_epsilon; }

    /// The closest multiple of 1/fractionBase, ties to even (Math.IEEEremainder).
    [[nodiscard]] double round(double value) const override;
    /// The next multiple of the increment above value + epsilon.
    [[nodiscard]] double getNextValue(double value) const override;
    /// The previous multiple of the increment below value - epsilon.
    [[nodiscard]] double getPreviousValue(double value) const override;
    /// Ticks at the halvings of one unit fitting @p minor, major ticks at decimal steps.
    [[nodiscard]] std::vector<Tick> getTicks(double start, double end, double minor,
                                             double major) const override;

    /// The value as a reduced fraction ("¹⁄₄", "-1 ¹⁄₂", "3"), or
    /// as "#.###" when it is more than epsilon away from one. NaN and the infinities fall
    /// through the fraction arithmetic as in OpenRocket ("NaN 0⁄₄").
    [[nodiscard]] std::string toString(double value) const override;
    /// toString() and the unit label, always separated by a space; "N/A" for NaN.
    [[nodiscard]] std::string toStringUnit(double value) const override;

    [[nodiscard]] std::unique_ptr<Unit> clone() const override;

private:
    [[nodiscard]] static double roundTo(double value, double fraction) noexcept;

    int         m_fractionBase;
    double      m_fractionValue;  ///< 1 / fractionBase
    double      m_incrementValue;
    double      m_epsilon;
    std::string m_unitLabel;
};

}  // namespace QtRocket
