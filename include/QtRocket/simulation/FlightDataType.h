#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/simulation/DataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

/// The built-in flight data types, OpenRocket's FlightDataType.TYPE_* fields in their declaration
/// order. FlightDataType::builtin() gives the process-wide instance of each.
enum class FlightDataTypeId
{
    TYPE_TIME,
    TYPE_ALTITUDE,
    TYPE_ALTITUDE_ABOVE_SEA,
    TYPE_VELOCITY_Z,
    TYPE_VELOCITY_TOTAL,
    TYPE_ACCELERATION_Z,
    TYPE_ACCELERATION_X,
    TYPE_ACCELERATION_Y,
    TYPE_ACCELERATION_BODYX,
    TYPE_ACCELERATION_BODYY,
    TYPE_ACCELERATION_BODYZ,
    TYPE_ACCELERATION_TOTAL,
    TYPE_POSITION_X,
    TYPE_POSITION_Y,
    TYPE_POSITION_XY,
    TYPE_POSITION_DIRECTION,
    TYPE_VELOCITY_XY,
    TYPE_ACCELERATION_XY,
    TYPE_LATITUDE,
    TYPE_LONGITUDE,
    TYPE_AOA,
    TYPE_ROLL_RATE,
    TYPE_PITCH_RATE,
    TYPE_YAW_RATE,
    TYPE_ORIENTATION_THETA,
    TYPE_ORIENTATION_PHI,
    TYPE_MASS,
    TYPE_MOTOR_MASS,
    TYPE_LONGITUDINAL_INERTIA,
    TYPE_ROTATIONAL_INERTIA,
    TYPE_GRAVITY,
    TYPE_CP_LOCATION,
    TYPE_CG_LOCATION,
    TYPE_STABILITY,
    TYPE_DAMPING_RATIO,
    TYPE_NATURAL_FREQUENCY,
    TYPE_MACH_NUMBER,
    TYPE_REYNOLDS_NUMBER,
    TYPE_THRUST_FORCE,
    TYPE_THRUST_CORRECTION,
    TYPE_THRUST_WEIGHT_RATIO,
    TYPE_DRAG_FORCE,
    TYPE_DRAG_COEFF,
    TYPE_FRICTION_DRAG_COEFF,
    TYPE_PRESSURE_DRAG_COEFF,
    TYPE_BASE_DRAG_COEFF,
    TYPE_AXIAL_DRAG_COEFF,
    TYPE_NORMAL_FORCE_COEFF,
    TYPE_CNA,
    TYPE_PITCH_MOMENT_COEFF,
    TYPE_YAW_MOMENT_COEFF,
    TYPE_SIDE_FORCE_COEFF,
    TYPE_ROLL_MOMENT_COEFF,
    TYPE_ROLL_FORCING_COEFF,
    TYPE_ROLL_DAMPING_COEFF,
    TYPE_PITCH_DAMPING_MOMENT_COEFF,
    TYPE_YAW_DAMPING_MOMENT_COEFF,
    TYPE_DAMPING_MOMENT_COEFF,
    TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC,
    TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE,
    TYPE_CORRECTIVE_MOMENT_COEFF,
    TYPE_CORIOLIS_ACCELERATION,
    TYPE_REFERENCE_LENGTH,
    TYPE_REFERENCE_AREA,
    TYPE_WIND_VELOCITY,
    TYPE_WIND_DIRECTION,
    TYPE_AIR_TEMPERATURE,
    TYPE_AIR_PRESSURE,
    TYPE_AIR_DENSITY,
    TYPE_SPEED_OF_SOUND,
    TYPE_TIME_STEP,
    TYPE_COMPUTATION_TIME,
};

inline constexpr std::size_t kBuiltinFlightDataTypeCount = 72;

/// A kind of value the simulation stores per time step (OpenRocket's FlightDataType): a name, a
/// symbol, the unit group its values are shown in, and the group and priority that order it
/// among the others.
///
/// Identity: every FlightDataType is a process-wide object made by this class and never
/// destroyed, so a reference or pointer to one stays valid for the whole process, static
/// destruction included; types are neither copied nor moved. The 72 built-in types are made on
/// first use (thread-safely, no static initialisation order issue) and reached through
/// builtin(); the others come from getType() and getTypeWithFixedUnit() (custom expressions,
/// simulation extensions and unknown types in .ork files). Java's DataBranch keys its columns by
/// equals() and hashCode(), which compare the names ignoring case; DataBranch does the same (see
/// there).
///
/// Names: getName() is the English name OpenRocket shows (its messages.properties text, e.g.
/// "Drag coefficient (CD)"), and getDisplayKey() the translation key it comes from
/// ("FlightDataType.TYPE_DRAG_COEFF"). A .ork file names a built-in type by getSaveKey()
/// ("drag_coeff"); files written before save keys existed use the name, which is why the English
/// names are kept exactly (findByName()).
///
/// Not ported: the slf4j logging of getType(). Java's getType() takes any UnitGroup object; here
/// it takes a process-wide group's id, or a unit string for a FixedUnitGroup
/// (getTypeWithFixedUnit()), the only other kind OpenRocket passes.
class FlightDataType final : public DataType
{
    /// Only this class makes FlightDataTypes, so that each one is interned.
    struct Passkey
    {
        explicit Passkey() = default;
    };
    struct Registry;

public:
    /// DEFAULT_PRIORITY: the priority of a type made by getType().
    static constexpr int kDefaultPriority = 999;

    /// The English text of "FlightDataType.TYPE_UPWIND", the name of a type that no longer exists:
    /// OpenRocket's .ork reader reads it as TYPE_POSITION_Y.
    static constexpr std::string_view kLegacyUpwindName = "Position upwind";

    /// The built-in type @p id (FlightDataType.TYPE_*).
    /// @throws BugError when @p id is not one of the enumerators
    [[nodiscard]] static const FlightDataType& builtin(FlightDataTypeId id);

    /// The 72 built-in types, in declaration order (the order of FlightDataTypeId).
    [[nodiscard]] static std::span<const FlightDataType* const> builtinTypes();

    /// FlightDataType.ALL_TYPES: the 71 built-in types in that array's order, which is not the
    /// declaration order and, as in OpenRocket, leaves out TYPE_THRUST_CORRECTION.
    [[nodiscard]] static std::span<const FlightDataType* const> allTypes();

    /// allTypes() sorted as OpenRocket sorts types (Arrays.sort by compareTo(), a stable sort):
    /// by group, then priority, types that compare equal keeping their ALL_TYPES order.
    [[nodiscard]] static std::span<const FlightDataType* const> allTypesSorted();

    /// FlightDataType.getType: the type with symbol @p symbol, made when needed. When a type with
    /// that symbol exists and has the same name and an equal unit group (UnitGroup::equals), it
    /// is returned. When the name or the unit group differs, a new type with @p name, @p symbol,
    /// @p units and the old type's priority replaces it for later lookups by symbol (the old type
    /// stays valid, as Java's does for whoever holds it); the replacement is a CUSTOM type without
    /// a save key even when the old one was built in. An empty (or blank) @p name stands for the
    /// existing type's name. Without an existing type a new one is made in the CUSTOM group with
    /// kDefaultPriority and no save key; an empty @p name then makes a type named "", as Java
    /// does for "" (a custom expression without a name). Thread-safe (Java: synchronized).
    [[nodiscard]] static const FlightDataType& getType(std::string_view name,
                                                       std::string_view symbol, UnitGroupId units);
    /// getType() with a null unit group (Java: getType(name, symbol, null)): the existing type's
    /// unit group, or UNITS_NONE for a new type. An empty @p name stands for Java's null, which
    /// is how OpenRocket's range and index expressions look a type up: the existing type, or
    /// BugError where Java throws IllegalArgumentException("typeName is null"). findBySymbol() is
    /// the lookup that gives null instead.
    /// @throws BugError when @p name is empty and no type has @p symbol
    [[nodiscard]] static const FlightDataType& getType(std::string_view name,
                                                       std::string_view symbol);
    /// getType() with a new FixedUnitGroup(@p unit) (Java: getType(name, symbol, new
    /// FixedUnitGroup(unit)), what a custom expression whose unit is not an SI unit gets); the
    /// type keeps the group. All FixedUnitGroups are equal (UnitGroup::equals), so an existing
    /// type with a fixed unit group of another unit is returned as it is, as in OpenRocket.
    /// (No overload takes a UnitGroup object: a temporary one passed to a function returning a
    /// reference trips GCC's -Wdangling-reference.)
    [[nodiscard]] static const FlightDataType& getTypeWithFixedUnit(std::string_view name,
                                                                    std::string_view symbol,
                                                                    std::string_view unit);

    /// FlightDataType.getTypeBySaveKey: the built-in type with that save key ("time",
    /// "drag_coeff", ...), or null. Every built-in type has one, TYPE_THRUST_CORRECTION included.
    [[nodiscard]] static const FlightDataType* getTypeBySaveKey(std::string_view saveKey);

    /// The type getType() finds for @p symbol (built-in or not), or null (Java:
    /// EXISTING_TYPES.get(symbol)). Thread-safe.
    [[nodiscard]] static const FlightDataType* findBySymbol(std::string_view symbol);

    /// The first type of allTypes() whose name is exactly @p name, or null: how OpenRocket's .ork
    /// reader resolves a type written before save keys existed.
    [[nodiscard]] static const FlightDataType* findByName(std::string_view name);

    /// Only FlightDataType can call this (see Passkey); use builtin() or getType().
    /// A type has either @p unitGroupId (a process-wide group) or @p ownedUnits.
    FlightDataType(Passkey passkey, std::optional<FlightDataTypeId> id, std::string name,
                   std::string saveKey, std::string typeDisplayKey, std::string symbol,
                   std::optional<UnitGroupId>       unitGroupId,
                   std::shared_ptr<const UnitGroup> ownedUnits, FlightDataTypeGroup group,
                   int typePriority);

    ~FlightDataType() override                       = default;
    FlightDataType(const FlightDataType&)            = delete;
    FlightDataType& operator=(const FlightDataType&) = delete;
    FlightDataType(FlightDataType&&)                 = delete;
    FlightDataType& operator=(FlightDataType&&)      = delete;

    /// The name shown to the user: the English name of a built-in type, the given name of others.
    [[nodiscard]] const std::string& getName() const override { return m_name; }
    [[nodiscard]] const std::string& getSymbol() const override { return m_symbol; }
    [[nodiscard]] const UnitGroup&   getUnitGroup() const override { return *m_units; }
    /// The id of the process-wide group getUnitGroup() is, or nullopt for the FixedUnitGroup of
    /// getTypeWithFixedUnit().
    [[nodiscard]] std::optional<UnitGroupId> getUnitGroupId() const noexcept
    {
        return m_unitGroupId;
    }

    /// The id of a built-in type, nullopt for the others.
    [[nodiscard]] std::optional<FlightDataTypeId> getId() const noexcept { return m_id; }
    [[nodiscard]] bool isBuiltin() const noexcept { return m_id.has_value(); }

    /// FlightDataType.getSaveKey: the stable key a .ork file names the type by, "time" for
    /// TYPE_TIME; getName() for a type that is not built in.
    [[nodiscard]] const std::string& getSaveKey() const noexcept
    {
        return m_saveKey.empty() ? m_name : m_saveKey;
    }

    /// The translation key of a built-in type's name ("FlightDataType.TYPE_TIME"), empty for the
    /// others.
    [[nodiscard]] const std::string& getDisplayKey() const noexcept { return m_displayKey; }

    [[nodiscard]] FlightDataTypeGroup getGroup() const noexcept { return m_group; }
    /// The group's priority (FlightDataType.getGroupPriority).
    [[nodiscard]] int getGroupPriority() const noexcept { return priority(m_group); }
    /// The priority within the group.
    [[nodiscard]] int getPriority() const noexcept { return m_priority; }

    /// FlightDataType.toString: the name.
    [[nodiscard]] const std::string& toString() const noexcept { return m_name; }

    /// FlightDataType.equals: the names are equal ignoring case (String.compareToIgnoreCase, here
    /// Strings::javaEqualsIgnoreCase). Two distinct types can be equal.
    [[nodiscard]] bool equals(const FlightDataType& other) const noexcept;

    /// FlightDataType.hashCode: the String.hashCode of the name lower-cased. Deviation: the name
    /// is case-folded as equals() folds it (Strings::javaCaseFold), so that equal types always
    /// hash alike. That is Java's toLowerCase(Locale.ENGLISH) for every character but 25: the 23
    /// lower-case letters that fold to another one (the micro sign, the dotless i, the long s,
    /// final sigma, Greek symbol forms such as U+03D1) and String.toLowerCase's special cases
    /// U+0130 and a word-final capital sigma. Java's hash of a name holding one of them can
    /// differ from that of an equal name, which the fold avoids.
    [[nodiscard]] int hashCode() const noexcept { return m_hashCode; }

    /// FlightDataType.compareTo: by group (compareTo(FlightDataTypeGroup, FlightDataTypeGroup)),
    /// then by the difference of the priorities. Types in the same group with the same priority
    /// compare equal (TYPE_THRUST_FORCE and TYPE_THRUST_CORRECTION, and all custom types).
    [[nodiscard]] int compareTo(const FlightDataType& other) const noexcept;

private:
    [[nodiscard]] static Registry& registry();
    /// getType() with the unit group as a process-wide id or an owned group; neither stands for
    /// Java's null.
    [[nodiscard]] static const FlightDataType& internType(
        std::string_view name, std::string_view symbol, std::optional<UnitGroupId> unitGroupId,
        std::shared_ptr<const UnitGroup> ownedUnits);

    std::optional<FlightDataTypeId>  m_id;
    std::string                      m_name;
    std::string                      m_saveKey;
    std::string                      m_displayKey;
    std::string                      m_symbol;
    std::optional<UnitGroupId>       m_unitGroupId;
    std::shared_ptr<const UnitGroup> m_ownedUnits;
    const UnitGroup*                 m_units;
    FlightDataTypeGroup              m_group;
    int                              m_priority;
    int                              m_hashCode;
};

}  // namespace QtRocket
