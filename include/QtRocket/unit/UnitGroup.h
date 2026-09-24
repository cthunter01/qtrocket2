#pragma once

#include <array>
#include <atomic>
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
/// one of them chosen as the default the user sees. The units keep their order of addition; the
/// group owns those given to addUnit(), and a StabilityUnitGroup shares the plain ones of the
/// group it was made from, as OpenRocket's groups share Unit objects.
///
/// The 43 groups OpenRocket defines are process-wide instances reached through unitGroup(): they
/// are built on first use (thread-safely) with the units, multipliers, names and order of
/// UnitGroup.java, and never destroyed, so a unit or Value taken from them stays valid for the
/// whole process, static destruction included. Changing a default unit (setDefaultUnit,
/// setDefaultMetricUnits, setDefaultImperialUnits, resetDefaultUnits) is meant for the GUI thread,
/// as in OpenRocket; any thread may read the defaults meanwhile (getDefaultUnit, toString, ...):
/// the index is atomic, so a reader sees the old default or the new one.
///
/// Lookups that fail return a null pointer, false or -1 where OpenRocket throws
/// IllegalArgumentException (getUnit(name), setDefaultUnit(name)) or returns null or -1
/// (findApproximate, getUnitIndex); an index out of range is a programming error and throws
/// BugError.
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
    /// The returned group must outlive every Value, Unit reference and pointer taken from it
    /// (toValue(), getDefaultUnit(), getUnit(), ...): its caliber and percentage units, the
    /// default among them, belong to it. Hold it in a member, as OpenRocket's RocketInfo does,
    /// rather than calling through a temporary (stabilityUnits(r)->toValue(x) dangles). The
    /// other units are UNITS_STABILITY's own.
    /// @throws BugError when @p reference is not positive
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> stabilityUnits(double reference);
    /// UNITS_SECONDARY_STABILITY bound to a constant reference length; the same lifetime rule as
    /// stabilityUnits().
    /// @throws BugError when @p reference is not positive
    [[nodiscard]] static std::unique_ptr<StabilityUnitGroup> secondaryStabilityUnits(
        double reference);
    /// UNITS_STABILITY with the caliber and percentage units reading the reference length from
    /// @p referenceLengthProvider on every conversion (OpenRocket's Rocket and
    /// FlightConfiguration forms, once the rocket model supplies a provider). A provider must
    /// apply OpenRocket's floor itself (CaliberUnit.calculateCaliber's kDefaultCaliber below
    /// 0.1 mm); a reference of 0 or NaN converts to an infinity or NaN, as in Java. The same
    /// lifetime rule as stabilityUnits().
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
    [[nodiscard]] int                 getDefaultUnitIndex() const noexcept
    {
        return m_defaultUnit.load(std::memory_order_relaxed);
    }
    /// @throws BugError when @p n is out of range (OpenRocket:
    /// IllegalArgumentException)
    virtual void setDefaultUnit(int n);
    /// Makes the unit named @p name (exactly) the default; false when the group has no such unit
    /// (OpenRocket throws IllegalArgumentException), so a mistyped name cannot pass unnoticed.
    [[nodiscard]] bool setDefaultUnit(std::string_view name);

    /// The first unit with multiplier 1, or UNITS_NONE's default unit (Unit::noUnit()) when there
    /// is none.
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
    /// getDefaultUnit().toValue(value). The Value refers to a unit of this group, so a temporary
    /// group, which would take the unit with it, is refused.
    [[nodiscard]] Value toValue(double value) const&;
    [[nodiscard]] Value toValue(double value) const&& = delete;

    /// UnitGroup.toString: the class name and the SI unit's name, "UnitGroup:m".
    [[nodiscard]] virtual std::string toString() const;

    /// UnitGroup.fromString: parses "<number>[ <unit>]" into an SI value. The number may be
    /// written with a point or a comma (Strings::convertToDouble, so a run of digits beyond the
    /// double range is an infinity, as in Java); the unit, matched ignoring case as
    /// String.equalsIgnoreCase does (Strings::javaEqualsIgnoreCase: "5 μm" with a Greek mu finds
    /// "µm", "5 Kg" finds "kg"), defaults to the group's default unit when absent. Fails
    /// (ErrorCode::PARSE, OpenRocket's NumberFormatException) when the text does not start with
    /// a number, holds a line break, or names a unit the group lacks. Only for units without
    /// powers, as in OpenRocket.
    /// @throws BugError when the unit found cannot convert, as the caliber and percentage
    ///         placeholders of UNITS_STABILITY and UNITS_SECONDARY_STABILITY cannot ("5 cal";
    ///         OpenRocket: BugException)
    [[nodiscard]] Result<double> fromString(std::string_view str) const;

    /// UnitGroup.equals: the same units (Unit::equals) in the same order; the default unit does
    /// not count.
    [[nodiscard]] bool equals(const UnitGroup& other) const;
    [[nodiscard]] bool operator==(const UnitGroup& other) const { return equals(other); }
    /// UnitGroup.hashCode's shape: the sum of the units' hashes.
    [[nodiscard]] std::size_t hash() const;

private:
    /// The units in order: those of m_ownedUnits, and for a StabilityUnitGroup the units it
    /// shares with its source group.
    std::vector<const Unit*> m_units;
    /// The units this group was given with addUnit().
    std::vector<std::unique_ptr<Unit>> m_ownedUnits;
    /// Written by the GUI thread, read by any (see the class comment).
    std::atomic<int> m_defaultUnit{0};
};

/// UNITS_STABILITY or UNITS_SECONDARY_STABILITY with the CaliberUnit and PercentageOfLengthUnit
/// replaced by ones that know a reference length (OpenRocket's StabilityUnitGroup). The other
/// units are the source group's own objects (units.addAll), so the group this was made from
/// must outlive it, as it must anyway for setDefaultUnit(). The default unit is copied from the
/// source, and setting the default here sets the source's default too, so the preference sticks.
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
    /// The units that replace the source group's caliber and percentage units (the percentage
    /// unit also when the source had none, as OpenRocket keeps it in a field).
    std::unique_ptr<Unit> m_caliberUnit;
    std::unique_ptr<Unit> m_percentageOfLengthUnit;
};

/// The process-wide instance of a group (OpenRocket's UnitGroup.UNITS_*), mutable so that its
/// default unit can be set. See UnitGroup for the threading rule.
[[nodiscard]] UnitGroup& unitGroup(UnitGroupId id);

/// Whether @p id is a key of OpenRocket's UnitGroup.UNITS map: every group but SHAPE_PARAMETER
/// and STABILITY_CALIBERS. A port of UNITS.get(key) or UNITS.entrySet() must skip the other two.
[[nodiscard]] constexpr bool isInUnitsMap(UnitGroupId id) noexcept
{
    return id != UnitGroupId::SHAPE_PARAMETER && id != UnitGroupId::STABILITY_CALIBERS;
}

/// The key of the group in OpenRocket's UnitGroup.UNITS map, which names the groups in the
/// preferences' "units" node: "LENGTH", "VELOCITY", "FLIGHT_TIME" for LONG_TIME, ...
/// SHAPE_PARAMETER and STABILITY_CALIBERS are not in that map (isInUnitsMap()); they get their
/// enumerator names.
[[nodiscard]] std::string_view unitGroupName(UnitGroupId id);

/// The group with unitGroupName() equal to @p name (exactly), or nullopt. This also finds the two
/// groups outside the UNITS map; UNITS.get(name) is unitGroupFromName() followed by
/// isInUnitsMap().
[[nodiscard]] std::optional<UnitGroupId> unitGroupFromName(std::string_view name) noexcept;

/// UnitGroup.SIUNITS: the group a custom expression's SI unit symbol stands for ("m" gives
/// ALL_LENGTHS, "m/s^2" ACCELERATION, "kg m^2" INERTIA, ...), or nullopt. OpenRocket stores a
/// custom expression's unit in the .ork file by this symbol.
[[nodiscard]] std::optional<UnitGroupId> unitGroupFromSiUnit(std::string_view siUnit) noexcept;

}  // namespace QtRocket
