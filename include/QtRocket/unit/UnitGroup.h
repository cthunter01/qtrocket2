#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class CaliberUnit;
class PercentageOfLengthUnit;

/// The unit groups OpenRocket keeps as UnitGroup's static UNITS_* fields, in their declaration
/// order. unitGroup() gives the process-wide instance of each.
enum class UnitGroupId
{
    NONE,
    MOTOR_DIMENSIONS,
    LENGTH,
    ALL_LENGTHS,
    DISTANCE,
    SHAPE_PARAMETER,
    AREA,
    STABILITY,
    SECONDARY_STABILITY,
    /// Only the caliber unit, which never scales the value: for values already in calibers.
    STABILITY_CALIBERS,
    VELOCITY,
    WINDSPEED,
    LATITUDE,
    LONGITUDE,
    ACCELERATION,
    MASS,
    INERTIA,
    ANGLE,
    DENSITY_BULK,
    DENSITY_SURFACE,
    DENSITY_LINE,
    FORCE,
    IMPULSE,
    /// Time in the order of less than a second (time step etc.).
    TIME_STEP,
    /// Time in the order of seconds (motor delay etc.).
    SHORT_TIME,
    /// Time in the order of minutes (flight time etc.).
    LONG_TIME,
    ROLL,
    TEMPERATURE,
    PRESSURE,
    SHEAR_MODULUS,
    RELATIVE,
    ROUGHNESS,
    COEFFICIENT,
    FREQUENCY,
    ENERGY,
    POWER,
    MOMENT,
    MOMENTUM,
    ANGULAR_MOMENTUM,
    VOLTAGE,
    CURRENT,
    SCALING,
    STROKE_WIDTH,
};

inline constexpr std::size_t kUnitGroupCount = 43;

/// Every group id, in declaration order.
inline constexpr std::array<UnitGroupId, kUnitGroupCount> kAllUnitGroupIds{
    UnitGroupId::NONE,
    UnitGroupId::MOTOR_DIMENSIONS,
    UnitGroupId::LENGTH,
    UnitGroupId::ALL_LENGTHS,
    UnitGroupId::DISTANCE,
    UnitGroupId::SHAPE_PARAMETER,
    UnitGroupId::AREA,
    UnitGroupId::STABILITY,
    UnitGroupId::SECONDARY_STABILITY,
    UnitGroupId::STABILITY_CALIBERS,
    UnitGroupId::VELOCITY,
    UnitGroupId::WINDSPEED,
    UnitGroupId::LATITUDE,
    UnitGroupId::LONGITUDE,
    UnitGroupId::ACCELERATION,
    UnitGroupId::MASS,
    UnitGroupId::INERTIA,
    UnitGroupId::ANGLE,
    UnitGroupId::DENSITY_BULK,
    UnitGroupId::DENSITY_SURFACE,
    UnitGroupId::DENSITY_LINE,
    UnitGroupId::FORCE,
    UnitGroupId::IMPULSE,
    UnitGroupId::TIME_STEP,
    UnitGroupId::SHORT_TIME,
    UnitGroupId::LONG_TIME,
    UnitGroupId::ROLL,
    UnitGroupId::TEMPERATURE,
    UnitGroupId::PRESSURE,
    UnitGroupId::SHEAR_MODULUS,
    UnitGroupId::RELATIVE,
    UnitGroupId::ROUGHNESS,
    UnitGroupId::COEFFICIENT,
    UnitGroupId::FREQUENCY,
    UnitGroupId::ENERGY,
    UnitGroupId::POWER,
    UnitGroupId::MOMENT,
    UnitGroupId::MOMENTUM,
    UnitGroupId::ANGULAR_MOMENTUM,
    UnitGroupId::VOLTAGE,
    UnitGroupId::CURRENT,
    UnitGroupId::SCALING,
    UnitGroupId::STROKE_WIDTH,
};

/// A group of units of the same quantity (OpenRocket's UnitGroup), such as the length units, with
/// one of them chosen as the default the user sees. The group owns its units, which keep their
/// order of addition; the units OpenRocket shares between groups are cloned here.
///
/// The 43 groups OpenRocket defines are process-wide instances reached through unitGroup(): they
/// are built on first use (thread-safely) with the units, multipliers, names and order of
/// UnitGroup.java. Changing a default unit (setDefaultUnit, setDefaultMetricUnits,
/// setDefaultImperialUnits, resetDefaultUnits) is not synchronised with readers: it is a
/// GUI-thread-only operation, as in OpenRocket, and a simulation thread only reads.
///
/// Lookups that fail return a null pointer or false where OpenRocket throws
/// IllegalArgumentException (getUnit(name), findApproximate, setDefaultUnit(name)); an index out
/// of range is a programming error and throws BugError.
class UnitGroup
{
public:
    class StabilityUnitGroup;

    UnitGroup()                            = default;
    virtual ~UnitGroup()                   = default;
    UnitGroup(const UnitGroup&)            = delete;
    UnitGroup& operator=(const UnitGroup&) = delete;
    UnitGroup(UnitGroup&&)                 = delete;
    UnitGroup& operator=(UnitGroup&&)      = delete;

    /// The metric defaults of OpenRocket's preferences ("cm", "g", "m/s", ...), applied to the
    /// process-wide groups. GUI thread only.
    static void setDefaultMetricUnits();
    /// The imperial defaults ("in", "oz", "ft/s", ...). GUI thread only.
    static void setDefaultImperialUnits();
    /// The defaults UnitGroup.java starts with (mostly the SI unit). GUI thread only.
    static void resetDefaultUnits();

    /// UNITS_STABILITY with the caliber and percentage units bound to a constant reference length.
    /// @throws BugError when @p reference is not positive
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> stabilityUnits(double reference);
    /// UNITS_SECONDARY_STABILITY bound to a constant reference length.
    /// @throws BugError when @p reference is not positive
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> secondaryStabilityUnits(
        double reference);
    /// UNITS_STABILITY with the caliber and percentage units reading the reference length from
    /// @p referenceLengthProvider on every conversion (OpenRocket's Rocket and
    /// FlightConfiguration forms, once the rocket model supplies a provider).
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> stabilityUnits(
        const std::function<double()>& referenceLengthProvider);
    /// UNITS_SECONDARY_STABILITY with a reference length provider.
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> secondaryStabilityUnits(
        const std::function<double()>& referenceLengthProvider);

    /// Appends a unit; the group takes ownership.
    void addUnit(std::unique_ptr<Unit> unit);

    [[nodiscard]] virtual int getUnitCount() const;
    /// @throws BugError for a group without units (OpenRocket: IndexOutOfBoundsException)
    [[nodiscard]] virtual const Unit& getDefaultUnit() const;
    [[nodiscard]] int                 getDefaultUnitIndex() const noexcept { return m_defaultUnit; }
    /// @throws BugError when @p n is out of range (OpenRocket:
    /// IllegalArgumentException)
    virtual void setDefaultUnit(int n);
    /// Makes the unit named @p name (exactly) the default; false when the group has no such unit
    /// (OpenRocket throws IllegalArgumentException).
    bool setDefaultUnit(std::string_view name);

    /// The first unit with multiplier 1, or UNITS_NONE's default unit when there is none.
    [[nodiscard]] virtual const Unit& getSIUnit() const;

    /// The first unit whose name matches @p str once both are reduced to their ASCII letters,
    /// digits and underscores, ignoring ASCII case ("F" finds "°F", "in" finds "in" before
    /// "in/64"); null when none matches. Mainly for tests, as in OpenRocket.
    [[nodiscard]] const Unit* findApproximate(std::string_view str) const;
    /// The unit named @p name exactly, or null (OpenRocket throws IllegalArgumentException).
    [[nodiscard]] const Unit* getUnit(std::string_view name) const;
    /// @throws BugError when @p n is out of range (OpenRocket: IndexOutOfBoundsException)
    [[nodiscard]] const Unit& getUnit(int n) const;
    /// The index of the first unit equal to @p u (Unit::equals), or -1.
    [[nodiscard]] int getUnitIndex(const Unit& u) const;
    /// Whether a unit equal to @p u (Unit::equals) is in the group.
    [[nodiscard]] virtual bool contains(const Unit& u) const;
    /// The units in order; the pointers stay valid while the group lives and is not added to.
    [[nodiscard]] std::vector<const Unit*> getUnits() const;

    /// getDefaultUnit().fromUnit(value).
    [[nodiscard]] double fromUnit(double value) const;
    /// getDefaultUnit().toString(value).
    [[nodiscard]] std::string toString(double value) const;
    /// getDefaultUnit().toStringUnit(value).
    [[nodiscard]] std::string toStringUnit(double value) const;
    /// getDefaultUnit().toValue(value).
    [[nodiscard]] Value toValue(double value) const;

    /// UnitGroup.toString: the class name and the SI unit's name, "UnitGroup:m".
    [[nodiscard]] virtual std::string toString() const;

    /// UnitGroup.fromString: parses "<number>[ <unit>]" into an SI value. The number may be
    /// written with a point or a comma (Strings::convertToDouble); the unit, matched ignoring
    /// ASCII case, defaults to the group's default unit when absent. Fails (ErrorCode::PARSE,
    /// OpenRocket's NumberFormatException) when the text does not start with a number, holds a
    /// line break, or names a unit the group lacks. Only for units without powers, as in
    /// OpenRocket.
    [[nodiscard]] Result<double> fromString(std::string_view str) const;

    /// UnitGroup.equals: the same units (Unit::equals) in the same order; the default unit does
    /// not count.
    [[nodiscard]] bool equals(const UnitGroup& other) const;
    [[nodiscard]] bool operator==(const UnitGroup& other) const { return equals(other); }
    /// UnitGroup.hashCode's shape: the sum of the units' hashes.
    [[nodiscard]] std::size_t hash() const;

protected:
    std::vector<std::unique_ptr<Unit>> m_units;
    int                                m_defaultUnit{0};
};

/// UNITS_STABILITY or UNITS_SECONDARY_STABILITY with the CaliberUnit and PercentageOfLengthUnit
/// replaced by ones that know a reference length (OpenRocket's StabilityUnitGroup). The other
/// units are clones of the group's; the default unit is copied from it, and setting the default
/// here sets the original group's default too, so the preference sticks.
class UnitGroup::StabilityUnitGroup : public UnitGroup
{
public:
    /// @throws BugError when @p reference is not positive
    StabilityUnitGroup(UnitGroup& stabilityUnit, double reference);
    StabilityUnitGroup(UnitGroup&                     stabilityUnit,
                       const std::function<double()>& referenceLengthProvider);

    /// Sets the default here and in the group this was made from.
    void setDefaultUnit(int n) override;
    using UnitGroup::setDefaultUnit;
    using UnitGroup::toString;

    /// The percentage-of-length unit (stability in %).
    [[nodiscard]] const Unit& getPercentageOfLengthUnit() const noexcept
    {
        return *m_percentageOfLengthUnit;
    }

    /// "StabilityUnitGroup:m".
    [[nodiscard]] std::string toString() const override;

private:
    StabilityUnitGroup(UnitGroup& stabilityUnit, std::unique_ptr<CaliberUnit> caliberUnit,
                       std::unique_ptr<PercentageOfLengthUnit> percentageOfLengthUnit);

    UnitGroup* m_stabilityUnit;
    /// The percentage unit when the source group had none to replace (never the case for
    /// OpenRocket's stability groups); otherwise it lives in m_units.
    std::unique_ptr<Unit> m_detachedPercentageOfLengthUnit;
    const Unit*           m_percentageOfLengthUnit{nullptr};
};

/// A "dumb" group of one arbitrary unit with multiplier 1 (OpenRocket's FixedUnitGroup), for
/// custom expressions: contains() accepts every unit. Where OpenRocket creates a new GeneralUnit
/// on each getDefaultUnit() and getSIUnit() call and keeps its unit list empty, this group owns
/// that one unit, so getUnit(0) works as well.
class FixedUnitGroup : public UnitGroup
{
public:
    explicit FixedUnitGroup(std::string unitString);

    [[nodiscard]] const std::string& getUnitString() const noexcept { return m_unitString; }

    [[nodiscard]] bool contains(const Unit& u) const override;
    /// "FixedUnitGroup:<unit>".
    [[nodiscard]] std::string toString() const override;
    using UnitGroup::toString;

private:
    std::string m_unitString;
};

/// The process-wide instance of a group (OpenRocket's UnitGroup.UNITS_*), mutable so that its
/// default unit can be set. See UnitGroup for the threading rule.
[[nodiscard]] UnitGroup& unitGroup(UnitGroupId id);

/// The key of the group in OpenRocket's UnitGroup.UNITS map, which the .ork <datatypes> and the
/// preferences use: "LENGTH", "VELOCITY", "FLIGHT_TIME" for LONG_TIME, ... SHAPE_PARAMETER and
/// STABILITY_CALIBERS are not in that map; they get their enumerator names.
[[nodiscard]] std::string_view unitGroupName(UnitGroupId id);

/// The group with unitGroupName() equal to @p name (exactly), or nullopt.
[[nodiscard]] std::optional<UnitGroupId> unitGroupFromName(std::string_view name) noexcept;

/// UnitGroup.SIUNITS: the group a custom expression's SI unit symbol stands for ("m" gives
/// ALL_LENGTHS, "m/s^2" ACCELERATION, "kg m^2" INERTIA, ...), or nullopt.
[[nodiscard]] std::optional<UnitGroupId> unitGroupFromSiUnit(std::string_view siUnit) noexcept;

}  // namespace QtRocket
