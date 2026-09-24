#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "QtRocket/unit/Tick.h"

namespace QtRocket
{

class Value;

/// A unit of measure (OpenRocket's Unit): the scale between the unit and SI, and the rules for
/// formatting, rounding and stepping values shown in that unit. Units are immutable; the
/// subclasses (GeneralUnit, FixedPrecisionUnit, FractionalUnit, ...) differ in how they convert,
/// round and format. A UnitGroup holds its units (a StabilityUnitGroup shares the plain ones with
/// the group it was made from, as in OpenRocket) and a Value refers to one.
///
/// toString(double) reproduces OpenRocket's DecimalFormat output digit for digit (see
/// QtRocket::DecimalFormat, which keeps java.text.DecimalFormat's digits and tie rules) with a
/// point as the decimal separator, whatever the process locale (OpenRocket follows the default
/// locale).
///
/// Not ported: Unit.toString() with no argument, which returns the unit name (use getUnit()), so
/// that no subclass overriding toString(double) hides it.
class Unit
{
public:
    virtual ~Unit() = default;

    /// Unit.NOUNIT: the dimensionless unit, a GeneralUnit with multiplier 1 whose name is a
    /// zero-width space (Chars::kZwsp) and which rounds to two significant digits.
    [[nodiscard]] static const Unit& noUnit();

    /// Converts an SI value to this unit: value / multiplier unless overridden.
    [[nodiscard]] virtual double toUnit(double value) const;
    /// Converts a value in this unit to SI: value * multiplier unless overridden.
    [[nodiscard]] virtual double fromUnit(double value) const;

    /// The unit's short name, e.g. "mm" or "ft/s".
    [[nodiscard]] const std::string& getUnit() const noexcept { return m_unit; }
    /// Whether toStringUnit() separates the value and the unit name with a space (true unless
    /// overridden; degrees and temperatures do not).
    [[nodiscard]] virtual bool hasSpace() const;
    /// One of this unit in SI units, e.g. 0.0254 for inches.
    [[nodiscard]] double getMultiplier() const noexcept { return m_multiplier; }

    /// Formats an SI @p value in this unit with a suitable number of decimals, without the unit
    /// name. Unit.java's rules: NaN is "N/A"; a magnitude above 1e6 is written as "0.00E0"
    /// ("1.23E6", the infinities as U+221E); from 100 up as an integer, half to even ("124" for
    /// 123.5); at or below 0.0005 as "0"; anything else is rounded by roundForDecimalFormat() and
    /// written as "0.0##" ("0.001", "1.12", "12.4"), or as an integer when within 0.0001 of one
    /// ("1" for 0.9995). Subclasses override this with their own formats.
    [[nodiscard]] virtual std::string toString(double value) const;

    /// toString() followed by the unit name, separated by a space when hasSpace(); "N/A" for NaN.
    [[nodiscard]] virtual std::string toStringUnit(double value) const;

    /// A Value of @p value (SI) in this unit; the Value refers to this unit, which must outlive it.
    /// A temporary unit would be gone before the Value is used, so that overload is deleted.
    [[nodiscard]] Value toValue(double value) const&;
    [[nodiscard]] Value toValue(double value) const&& = delete;

    /// Rounds a value in this unit to a precision suitable for rough valuing (about two
    /// significant digits).
    [[nodiscard]] virtual double round(double value) const = 0;
    /// The next rounded value after @p value (in this unit).
    [[nodiscard]] virtual double getNextValue(double value) const = 0;
    /// The previous rounded value before @p value (in this unit).
    [[nodiscard]] virtual double getPreviousValue(double value) const = 0;

    /// The ticks of an axis from @p start to @p end (SI units); @p minor and @p major are the
    /// smallest distances between minor and between major ticks, in SI units.
    /// @throws BugError when a distance is not positive or major is below minor
    ///         (OpenRocket: IllegalArgumentException), or when Java's int arithmetic on the step
    ///         ratios gives a zero modulus, as for a NaN major (OpenRocket: ArithmeticException)
    [[nodiscard]] virtual std::vector<Tick> getTicks(double start, double end, double minor,
                                                     double major) const = 0;

    /// Unit.equals: the same class, multiplier and unit name, nothing else (two GeneralUnits that
    /// round differently are equal).
    [[nodiscard]] bool equals(const Unit& other) const;
    [[nodiscard]] bool operator==(const Unit& other) const { return equals(other); }

    /// Unit.hashCode's shape (the class and the unit name), so that equal units hash alike.
    [[nodiscard]] std::size_t hash() const;

protected:
    /// @throws BugError when @p multiplier is 0 (OpenRocket:
    /// IllegalArgumentException).
    Unit(double multiplier, std::string unit);
    Unit(const Unit&)            = default;
    Unit& operator=(const Unit&) = default;
    Unit(Unit&&)                 = default;
    Unit& operator=(Unit&&)      = default;

    /// Rounds a value of magnitude below 100 (in this unit) for the "0.0##" pattern: to three
    /// significant digits, or to three decimals below 1 (0.0015 becomes 0.002), half to even as
    /// Math.rint. InchUnit overrides it to keep three decimals throughout.
    [[nodiscard]] virtual double roundForDecimalFormat(double val) const;

    /// GeneralUnit.getTicks, which FixedPrecisionUnit copies verbatim: ticks at the steps of a
    /// decimal scale (round tens and fives) fitting @p minor and @p major, the major and notable
    /// ones chosen by the position's remainder modulo the step ratios.
    /// @throws BugError when a distance is not positive or major is below minor
    ///         (IllegalArgumentException), or when a step ratio narrows to a zero modulus, as a
    ///         NaN major does (ArithmeticException "/ by zero")
    [[nodiscard]] std::vector<Tick> decimalTicks(double start, double end, double minor,
                                                 double major) const;

    /// The part of getTicks that GeneralUnit and FractionalUnit share once the minor step is
    /// known: the major step (a round ten or five fitting @p major), the moduli that pick the
    /// major and notable ticks, and the ticks at every multiple of @p minstep from @p start to
    /// @p end. Everything is in this unit (@p mod2 is the minor-notable modulus). The moduli use
    /// Java's int arithmetic: Math.round's long narrowed to an int, products that wrap, and a
    /// remainder where INT_MIN % -1 is 0.
    /// @throws BugError when a modulus is 0 at a tick (ArithmeticException "/ by zero")
    [[nodiscard]] std::vector<Tick> ticksAtMinorSteps(double start, double end, double major,
                                                      double minstep, int mod2) const;

private:
    double      m_multiplier;
    std::string m_unit;
};

}  // namespace QtRocket
