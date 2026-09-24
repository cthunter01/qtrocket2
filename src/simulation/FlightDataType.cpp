#include "QtRocket/simulation/FlightDataType.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/unit/FixedUnitGroup.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Group = FlightDataTypeGroup;
using Units = UnitGroupId;
using enum FlightDataTypeId;

/// The arguments of one built-in type's newType() call, with its English message text.
struct BuiltinInfo
{
    FlightDataTypeId id;
    std::string_view saveKey;
    std::string_view displayKey;
    std::string_view name;
    std::string_view symbol;
    UnitGroupId      units;
    Group            group;
    int              priority;
};

[[nodiscard]] constexpr BuiltinInfo info(FlightDataTypeId id, std::string_view saveKey,
                                         std::string_view displayKey, std::string_view name,
                                         std::string_view symbol, UnitGroupId units, Group group,
                                         int priority) noexcept
{
    return {.id         = id,
            .saveKey    = saveKey,
            .displayKey = displayKey,
            .name       = name,
            .symbol     = symbol,
            .units      = units,
            .group      = group,
            .priority   = priority};
}

// The Greek letters of the symbols, as UTF-8 bytes (see util/Chars.h). A literal is split after
// each escape so that a following letter cannot extend the hex escape.
constexpr std::string_view kSymPositionDirection =
    "\xCE\xB8"  // theta
    "l";
constexpr std::string_view kSymLatitude  = "\xCF\x86";  // phi
constexpr std::string_view kSymLongitude = "\xCE\xBB";  // lambda
constexpr std::string_view kSymAoa       = "\xCE\xB1";  // alpha
constexpr std::string_view kSymRollRate =
    "d"
    "\xCE\xA6";  // capital phi
constexpr std::string_view kSymPitchRate =
    "d"
    "\xCE\xB8";  // theta
constexpr std::string_view kSymYawRate =
    "d"
    "\xCE\xA8";                                                // capital psi
constexpr std::string_view kSymOrientationTheta = "\xCE\x98";  // capital theta
constexpr std::string_view kSymOrientationPhi   = "\xCE\xA6";  // capital phi
constexpr std::string_view kSymDampingRatio     = "\xCE\xB6";  // zeta
constexpr std::string_view kSymNaturalFrequency =
    "\xCF\x89"  // omega
    "n";
constexpr std::string_view kSymCna =
    "CN"
    "\xCE\xB1";  // "CN" + Chars.ALPHA
constexpr std::string_view kSymPitchMoment =
    "C"
    "\xCE\xB8";
constexpr std::string_view kSymYawMoment =
    "C"
    "\xCF\x84"  // tau
    "\xCE\xA8";
constexpr std::string_view kSymSideForce =
    "C"
    "\xCF\x84"
    "s";
constexpr std::string_view kSymRollMoment =
    "C"
    "\xCF\x84"
    "\xCE\xA6";
constexpr std::string_view kSymRollForcing =
    "Cf"
    "\xCE\xA6";
constexpr std::string_view kSymRollDamping =
    "C"
    "\xCE\xB6"
    "\xCE\xA6";
constexpr std::string_view kSymPitchDamping =
    "C"
    "\xCE\xB6"
    "\xCE\xB8";
constexpr std::string_view kSymYawDamping =
    "C"
    "\xCE\xB6"
    "\xCE\xA8";
constexpr std::string_view kSymWindDirection =
    "\xCE\xB8"
    "w";
constexpr std::string_view kSymAirDensity = "\xCF\x81";  // rho

constexpr std::string_view kNameCna =
    "Normal force coefficient derivative (CN"
    "\xCE\xB1"
    ")";

/// FlightDataType.java's TYPE_* fields in declaration order: save key, translation key, English
/// name (core messages.properties), symbol, unit group, group and priority.
constexpr std::array<BuiltinInfo, kBuiltinFlightDataTypeCount> kBuiltins{
    // Time
    info(TYPE_TIME, "time", "FlightDataType.TYPE_TIME", "Time", "t", Units::LONG_TIME, Group::TIME,
         0),
    // Position and motion
    info(TYPE_ALTITUDE, "altitude", "FlightDataType.TYPE_ALTITUDE", "Altitude", "h",
         Units::DISTANCE, Group::POSITION_AND_MOTION, 0),
    info(TYPE_ALTITUDE_ABOVE_SEA, "altitude_above_sea", "FlightDataType.TYPE_ALTITUDE_ABOVE_SEA",
         "Altitude above sea level", "ha", Units::DISTANCE, Group::POSITION_AND_MOTION, 1),
    info(TYPE_VELOCITY_Z, "velocity_z", "FlightDataType.TYPE_VELOCITY_Z", "Vertical velocity", "Vz",
         Units::VELOCITY, Group::POSITION_AND_MOTION, 2),
    info(TYPE_VELOCITY_TOTAL, "velocity_total", "FlightDataType.TYPE_VELOCITY_TOTAL",
         "Total velocity", "Vt", Units::VELOCITY, Group::POSITION_AND_MOTION, 3),
    info(TYPE_ACCELERATION_Z, "acceleration_z", "FlightDataType.TYPE_ACCELERATION_Z",
         "Vertical acceleration", "Az", Units::ACCELERATION, Group::POSITION_AND_MOTION, 4),
    info(TYPE_ACCELERATION_X, "acceleration_x", "FlightDataType.TYPE_ACCELERATION_X",
         "Acceleration to the East", "Ax", Units::ACCELERATION, Group::POSITION_AND_MOTION, 5),
    info(TYPE_ACCELERATION_Y, "acceleration_y", "FlightDataType.TYPE_ACCELERATION_Y",
         "Acceleration to the North", "Ay", Units::ACCELERATION, Group::POSITION_AND_MOTION, 6),
    info(TYPE_ACCELERATION_BODYX, "acceleration_bodyx", "FlightDataType.TYPE_ACCELERATION_BODYX",
         "X body acceleration", "Abx", Units::ACCELERATION, Group::POSITION_AND_MOTION, 7),
    info(TYPE_ACCELERATION_BODYY, "acceleration_bodyy", "FlightDataType.TYPE_ACCELERATION_BODYY",
         "Y body acceleration", "Aby", Units::ACCELERATION, Group::POSITION_AND_MOTION, 8),
    info(TYPE_ACCELERATION_BODYZ, "acceleration_bodyz", "FlightDataType.TYPE_ACCELERATION_BODYZ",
         "Z body acceleration", "Abz", Units::ACCELERATION, Group::POSITION_AND_MOTION, 9),
    info(TYPE_ACCELERATION_TOTAL, "acceleration_total", "FlightDataType.TYPE_ACCELERATION_TOTAL",
         "Total acceleration", "At", Units::ACCELERATION, Group::POSITION_AND_MOTION, 10),
    // Lateral position and motion
    info(TYPE_POSITION_X, "position_x", "FlightDataType.TYPE_POSITION_X", "Position East of launch",
         "Px", Units::DISTANCE, Group::POSITION_AND_MOTION, 11),
    info(TYPE_POSITION_Y, "position_y", "FlightDataType.TYPE_POSITION_Y",
         "Position North of launch", "Py", Units::DISTANCE, Group::POSITION_AND_MOTION, 12),
    info(TYPE_POSITION_XY, "position_xy", "FlightDataType.TYPE_POSITION_XY", "Lateral distance",
         "Pl", Units::DISTANCE, Group::POSITION_AND_MOTION, 13),
    info(TYPE_POSITION_DIRECTION, "position_direction", "FlightDataType.TYPE_POSITION_DIRECTION",
         "Lateral direction", kSymPositionDirection, Units::ANGLE, Group::POSITION_AND_MOTION, 14),
    info(TYPE_VELOCITY_XY, "velocity_xy", "FlightDataType.TYPE_VELOCITY_XY", "Lateral velocity",
         "Vl", Units::VELOCITY, Group::POSITION_AND_MOTION, 15),
    info(TYPE_ACCELERATION_XY, "acceleration_xy", "FlightDataType.TYPE_ACCELERATION_XY",
         "Lateral acceleration", "Al", Units::ACCELERATION, Group::POSITION_AND_MOTION, 16),
    info(TYPE_LATITUDE, "latitude", "FlightDataType.TYPE_LATITUDE", "Latitude", kSymLatitude,
         Units::LATITUDE, Group::POSITION_AND_MOTION, 17),
    info(TYPE_LONGITUDE, "longitude", "FlightDataType.TYPE_LONGITUDE", "Longitude", kSymLongitude,
         Units::LONGITUDE, Group::POSITION_AND_MOTION, 18),
    // Orientation
    info(TYPE_AOA, "aoa", "FlightDataType.TYPE_AOA", "Angle of attack", kSymAoa, Units::ANGLE,
         Group::ORIENTATION, 0),
    info(TYPE_ROLL_RATE, "roll_rate", "FlightDataType.TYPE_ROLL_RATE", "Roll rate (Z)",
         kSymRollRate, Units::ROLL, Group::ORIENTATION, 1),
    info(TYPE_PITCH_RATE, "pitch_rate", "FlightDataType.TYPE_PITCH_RATE", "Pitch rate (Y)",
         kSymPitchRate, Units::ROLL, Group::ORIENTATION, 2),
    info(TYPE_YAW_RATE, "yaw_rate", "FlightDataType.TYPE_YAW_RATE", "Yaw rate (X)", kSymYawRate,
         Units::ROLL, Group::ORIENTATION, 3),
    info(TYPE_ORIENTATION_THETA, "orientation_theta", "FlightDataType.TYPE_ORIENTATION_THETA",
         "Vertical orientation (zenith)", kSymOrientationTheta, Units::ANGLE, Group::ORIENTATION,
         4),
    info(TYPE_ORIENTATION_PHI, "orientation_phi", "FlightDataType.TYPE_ORIENTATION_PHI",
         "Lateral orientation (azimuth)", kSymOrientationPhi, Units::ANGLE, Group::ORIENTATION, 5),
    // Mass and inertia
    info(TYPE_MASS, "mass", "FlightDataType.TYPE_MASS", "Mass", "m", Units::MASS,
         Group::MASS_AND_INERTIA, 0),
    info(TYPE_MOTOR_MASS, "motor_mass", "FlightDataType.TYPE_MOTOR_MASS", "Motor mass", "mp",
         Units::MASS, Group::MASS_AND_INERTIA, 1),
    info(TYPE_LONGITUDINAL_INERTIA, "longitudinal_inertia",
         "FlightDataType.TYPE_LONGITUDINAL_INERTIA", "Longitudinal moment of inertia", "Il",
         Units::INERTIA, Group::MASS_AND_INERTIA, 2),
    info(TYPE_ROTATIONAL_INERTIA, "rotational_inertia", "FlightDataType.TYPE_ROTATIONAL_INERTIA",
         "Rotational moment of inertia", "Ir", Units::INERTIA, Group::MASS_AND_INERTIA, 3),
    info(TYPE_GRAVITY, "gravity", "FlightDataType.TYPE_GRAVITY", "Gravitational acceleration", "g",
         Units::ACCELERATION, Group::MASS_AND_INERTIA, 4),
    // Stability
    info(TYPE_CP_LOCATION, "cp_location", "FlightDataType.TYPE_CP_LOCATION", "CP location", "Cp",
         Units::LENGTH, Group::STABILITY, 0),
    info(TYPE_CG_LOCATION, "cg_location", "FlightDataType.TYPE_CG_LOCATION", "CG location", "Cg",
         Units::LENGTH, Group::STABILITY, 1),
    info(TYPE_STABILITY, "stability", "FlightDataType.TYPE_STABILITY", "Stability margin calibers",
         "S", Units::COEFFICIENT, Group::STABILITY, 2),
    info(TYPE_DAMPING_RATIO, "damping_ratio", "FlightDataType.TYPE_DAMPING_RATIO", "Damping ratio",
         kSymDampingRatio, Units::COEFFICIENT, Group::STABILITY, 3),
    info(TYPE_NATURAL_FREQUENCY, "natural_frequency", "FlightDataType.TYPE_NATURAL_FREQUENCY",
         "Natural frequency", kSymNaturalFrequency, Units::ROLL, Group::STABILITY, 4),
    // Characteristic numbers
    info(TYPE_MACH_NUMBER, "mach_number", "FlightDataType.TYPE_MACH_NUMBER", "Mach number", "M",
         Units::COEFFICIENT, Group::CHARACTERISTIC_NUMBERS, 0),
    info(TYPE_REYNOLDS_NUMBER, "reynolds_number", "FlightDataType.TYPE_REYNOLDS_NUMBER",
         "Reynolds number", "R", Units::COEFFICIENT, Group::CHARACTERISTIC_NUMBERS, 1),
    // Thrust and drag
    info(TYPE_THRUST_FORCE, "thrust_force", "FlightDataType.TYPE_THRUST_FORCE", "Thrust", "Ft",
         Units::FORCE, Group::THRUST_AND_DRAG, 0),
    info(TYPE_THRUST_CORRECTION, "thrust_correction", "FlightDataType.TYPE_THRUST_CORRECTION",
         "Thrust Pressure Correction", "Fta", Units::FORCE, Group::THRUST_AND_DRAG, 0),
    info(TYPE_THRUST_WEIGHT_RATIO, "thrust_weight_ratio", "FlightDataType.TYPE_THRUST_WEIGHT_RATIO",
         "Thrust-to-weight ratio", "Twr", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 1),
    info(TYPE_DRAG_FORCE, "drag_force", "FlightDataType.TYPE_DRAG_FORCE", "Drag force", "Fd",
         Units::FORCE, Group::THRUST_AND_DRAG, 2),
    info(TYPE_DRAG_COEFF, "drag_coeff", "FlightDataType.TYPE_DRAG_COEFF", "Drag coefficient (CD)",
         "Cd", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 3),
    info(TYPE_FRICTION_DRAG_COEFF, "friction_drag_coeff", "FlightDataType.TYPE_FRICTION_DRAG_COEFF",
         "Friction drag coefficient (CD_friction)", "Cdf", Units::COEFFICIENT,
         Group::THRUST_AND_DRAG, 4),
    info(TYPE_PRESSURE_DRAG_COEFF, "pressure_drag_coeff", "FlightDataType.TYPE_PRESSURE_DRAG_COEFF",
         "Pressure drag coefficient (CD_pressure)", "Cdp", Units::COEFFICIENT,
         Group::THRUST_AND_DRAG, 5),
    info(TYPE_BASE_DRAG_COEFF, "base_drag_coeff", "FlightDataType.TYPE_BASE_DRAG_COEFF",
         "Base drag coefficient (CD_base)", "Cdb", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 6),
    info(TYPE_AXIAL_DRAG_COEFF, "axial_drag_coeff", "FlightDataType.TYPE_AXIAL_DRAG_COEFF",
         "Axial drag coefficient (CA)", "Cda", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 7),
    // Coefficients
    info(TYPE_NORMAL_FORCE_COEFF, "normal_force_coeff", "FlightDataType.TYPE_NORMAL_FORCE_COEFF",
         "Normal force coefficient (CN)", "Cn", Units::COEFFICIENT, Group::COEFFICIENTS, 0),
    info(TYPE_CNA, "cna", "FlightDataType.TYPE_CNA", kNameCna, kSymCna, Units::COEFFICIENT,
         Group::COEFFICIENTS, 1),
    info(TYPE_PITCH_MOMENT_COEFF, "pitch_moment_coeff", "FlightDataType.TYPE_PITCH_MOMENT_COEFF",
         "Pitch moment coefficient (Cm)", kSymPitchMoment, Units::COEFFICIENT, Group::COEFFICIENTS,
         2),
    info(TYPE_YAW_MOMENT_COEFF, "yaw_moment_coeff", "FlightDataType.TYPE_YAW_MOMENT_COEFF",
         "Yaw moment coefficient", kSymYawMoment, Units::COEFFICIENT, Group::COEFFICIENTS, 3),
    info(TYPE_SIDE_FORCE_COEFF, "side_force_coeff", "FlightDataType.TYPE_SIDE_FORCE_COEFF",
         "Side force coefficient", kSymSideForce, Units::COEFFICIENT, Group::COEFFICIENTS, 4),
    info(TYPE_ROLL_MOMENT_COEFF, "roll_moment_coeff", "FlightDataType.TYPE_ROLL_MOMENT_COEFF",
         "Roll moment coefficient", kSymRollMoment, Units::COEFFICIENT, Group::COEFFICIENTS, 5),
    info(TYPE_ROLL_FORCING_COEFF, "roll_forcing_coeff", "FlightDataType.TYPE_ROLL_FORCING_COEFF",
         "Roll forcing coefficient", kSymRollForcing, Units::COEFFICIENT, Group::COEFFICIENTS, 6),
    info(TYPE_ROLL_DAMPING_COEFF, "roll_damping_coeff", "FlightDataType.TYPE_ROLL_DAMPING_COEFF",
         "Roll damping coefficient", kSymRollDamping, Units::COEFFICIENT, Group::COEFFICIENTS, 7),
    info(TYPE_PITCH_DAMPING_MOMENT_COEFF, "pitch_damping_moment_coeff",
         "FlightDataType.TYPE_PITCH_DAMPING_MOMENT_COEFF", "Pitch damping coefficient",
         kSymPitchDamping, Units::COEFFICIENT, Group::COEFFICIENTS, 8),
    info(TYPE_YAW_DAMPING_MOMENT_COEFF, "yaw_damping_moment_coeff",
         "FlightDataType.TYPE_YAW_DAMPING_MOMENT_COEFF", "Yaw damping coefficient", kSymYawDamping,
         Units::COEFFICIENT, Group::COEFFICIENTS, 9),
    info(TYPE_DAMPING_MOMENT_COEFF, "damping_moment_coeff",
         "FlightDataType.TYPE_DAMPING_MOMENT_COEFF", "Damping moment coefficient", "Cdm",
         Units::ANGULAR_MOMENTUM, Group::COEFFICIENTS, 10),
    info(TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC, "damping_moment_coeff_aerodynamic",
         "FlightDataType.TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC",
         "Damping moment coefficient (aerodynamic)", "Cdm_aero", Units::ANGULAR_MOMENTUM,
         Group::COEFFICIENTS, 11),
    info(TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE, "damping_moment_coeff_propulsive",
         "FlightDataType.TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE",
         "Damping moment coefficient (propulsive)", "Cdm_prop", Units::ANGULAR_MOMENTUM,
         Group::COEFFICIENTS, 12),
    info(TYPE_CORRECTIVE_MOMENT_COEFF, "corrective_moment_coeff",
         "FlightDataType.TYPE_CORRECTIVE_MOMENT_COEFF", "Corrective moment coefficient", "Ccm",
         Units::MOMENT, Group::COEFFICIENTS, 13),
    // Coriolis acceleration: made with the newType() overload without a group, so CUSTOM
    info(TYPE_CORIOLIS_ACCELERATION, "coriolis_acceleration",
         "FlightDataType.TYPE_CORIOLIS_ACCELERATION", "Coriolis acceleration", "Ac",
         Units::ACCELERATION, Group::CUSTOM, 99),
    // Reference values
    info(TYPE_REFERENCE_LENGTH, "reference_length", "FlightDataType.TYPE_REFERENCE_LENGTH",
         "Reference length", "Lr", Units::LENGTH, Group::REFERENCE_VALUES, 0),
    info(TYPE_REFERENCE_AREA, "reference_area", "FlightDataType.TYPE_REFERENCE_AREA",
         "Reference area", "Ar", Units::AREA, Group::REFERENCE_VALUES, 1),
    // Atmospheric conditions
    info(TYPE_WIND_VELOCITY, "wind_velocity", "FlightDataType.TYPE_WIND_VELOCITY", "Wind velocity",
         "Vw", Units::VELOCITY, Group::ATMOSPHERIC_CONDITIONS, 0),
    info(TYPE_WIND_DIRECTION, "wind_direction", "FlightDataType.TYPE_WIND_DIRECTION",
         "Wind direction", kSymWindDirection, Units::ANGLE, Group::ATMOSPHERIC_CONDITIONS, 1),
    info(TYPE_AIR_TEMPERATURE, "air_temperature", "FlightDataType.TYPE_AIR_TEMPERATURE",
         "Air temperature", "T", Units::TEMPERATURE, Group::ATMOSPHERIC_CONDITIONS, 2),
    info(TYPE_AIR_PRESSURE, "air_pressure", "FlightDataType.TYPE_AIR_PRESSURE", "Air pressure", "P",
         Units::PRESSURE, Group::ATMOSPHERIC_CONDITIONS, 3),
    info(TYPE_AIR_DENSITY, "air_density", "FlightDataType.TYPE_AIR_DENSITY", "Air density",
         kSymAirDensity, Units::DENSITY_BULK, Group::ATMOSPHERIC_CONDITIONS, 4),
    info(TYPE_SPEED_OF_SOUND, "speed_of_sound", "FlightDataType.TYPE_SPEED_OF_SOUND",
         "Speed of sound", "Vs", Units::VELOCITY, Group::ATMOSPHERIC_CONDITIONS, 5),
    // Simulation information
    info(TYPE_TIME_STEP, "time_step", "FlightDataType.TYPE_TIME_STEP", "Simulation time step", "dt",
         Units::TIME_STEP, Group::SIMULATION_INFORMATION, 0),
    info(TYPE_COMPUTATION_TIME, "computation_time", "FlightDataType.TYPE_COMPUTATION_TIME",
         "Computation time", "tc", Units::SHORT_TIME, Group::SIMULATION_INFORMATION, 1),
};

constexpr std::size_t kAllTypesCount = 71;

/// FlightDataType.ALL_TYPES, in that array's order (no TYPE_THRUST_CORRECTION).
constexpr std::array<FlightDataTypeId, kAllTypesCount> kAllTypes{
    TYPE_TIME,
    TYPE_ALTITUDE,
    TYPE_ALTITUDE_ABOVE_SEA,
    TYPE_VELOCITY_Z,
    TYPE_ACCELERATION_Z,
    TYPE_ACCELERATION_BODYZ,
    TYPE_VELOCITY_TOTAL,
    TYPE_ACCELERATION_TOTAL,
    TYPE_POSITION_X,
    TYPE_ACCELERATION_X,
    TYPE_ACCELERATION_BODYX,
    TYPE_POSITION_Y,
    TYPE_ACCELERATION_Y,
    TYPE_ACCELERATION_BODYY,
    TYPE_POSITION_XY,
    TYPE_POSITION_DIRECTION,
    TYPE_VELOCITY_XY,
    TYPE_ACCELERATION_XY,
    TYPE_LATITUDE,
    TYPE_LONGITUDE,
    TYPE_GRAVITY,
    TYPE_AOA,
    TYPE_ROLL_RATE,
    TYPE_PITCH_RATE,
    TYPE_YAW_RATE,
    TYPE_MASS,
    TYPE_MOTOR_MASS,
    TYPE_LONGITUDINAL_INERTIA,
    TYPE_ROTATIONAL_INERTIA,
    TYPE_CP_LOCATION,
    TYPE_CG_LOCATION,
    TYPE_STABILITY,
    TYPE_MACH_NUMBER,
    TYPE_REYNOLDS_NUMBER,
    TYPE_THRUST_FORCE,
    TYPE_THRUST_WEIGHT_RATIO,
    TYPE_DRAG_FORCE,
    TYPE_DRAG_COEFF,
    TYPE_AXIAL_DRAG_COEFF,
    TYPE_FRICTION_DRAG_COEFF,
    TYPE_PRESSURE_DRAG_COEFF,
    TYPE_BASE_DRAG_COEFF,
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
    TYPE_DAMPING_RATIO,
    TYPE_NATURAL_FREQUENCY,
    TYPE_CORIOLIS_ACCELERATION,
    TYPE_REFERENCE_LENGTH,
    TYPE_REFERENCE_AREA,
    TYPE_ORIENTATION_THETA,
    TYPE_ORIENTATION_PHI,
    TYPE_WIND_VELOCITY,
    TYPE_WIND_DIRECTION,
    TYPE_AIR_TEMPERATURE,
    TYPE_AIR_PRESSURE,
    TYPE_AIR_DENSITY,
    TYPE_SPEED_OF_SOUND,
    TYPE_TIME_STEP,
    TYPE_COMPUTATION_TIME,
};

[[nodiscard]] constexpr std::size_t indexOf(FlightDataTypeId id) noexcept
{
    return static_cast<std::size_t>(id);
}

}  // namespace

/// Every FlightDataType ever made, and the lookup tables of FlightDataType.java's statics.
struct FlightDataType::Registry
{
    Registry();

    /// Guards types and bySymbol, which getType() changes (Java: synchronized).
    std::mutex guard;
    /// Owns every type; a type is never removed, so references to it stay valid.
    std::vector<std::unique_ptr<FlightDataType>> types;
    /// EXISTING_TYPES: the current type of each symbol.
    std::map<std::string, const FlightDataType*, std::less<>> bySymbol;

    // Written by the constructor only.
    std::array<const FlightDataType*, kBuiltinFlightDataTypeCount> builtins{};
    std::array<const FlightDataType*, kAllTypesCount>              all{};
    std::array<const FlightDataType*, kAllTypesCount>              allSorted{};
    /// SAVE_KEY_TYPES.
    std::map<std::string, const FlightDataType*, std::less<>> bySaveKey;
};

FlightDataType::Registry::Registry()
{
    types.reserve(kBuiltins.size());
    for (std::size_t i = 0; i < kBuiltins.size(); i++)
    {
        const BuiltinInfo& entry = kBuiltins.at(i);
        QTROCKET_ASSERT(indexOf(entry.id) == i);
        auto type = std::make_unique<FlightDataType>(
            Passkey{}, entry.id, std::string(entry.name), std::string(entry.saveKey),
            std::string(entry.displayKey), std::string(entry.symbol), entry.units, nullptr,
            entry.group, entry.priority);
        builtins.at(i) = type.get();
        // newType(): EXISTING_TYPES.put(symbol, type) and SAVE_KEY_TYPES.put(saveKey, type).
        bySymbol.insert_or_assign(std::string(entry.symbol), type.get());
        bySaveKey.insert_or_assign(std::string(entry.saveKey), type.get());
        types.push_back(std::move(type));
    }

    for (std::size_t i = 0; i < kAllTypes.size(); i++)
    {
        all.at(i) = builtins.at(indexOf(kAllTypes.at(i)));
    }
    allSorted = all;
    std::ranges::stable_sort(allSorted, [](const FlightDataType* a, const FlightDataType* b) {
        return a->compareTo(*b) < 0;
    });
}

FlightDataType::Registry& FlightDataType::registry()
{
    // Never destroyed, as OpenRocket's static types are not: a type stays valid during static
    // destruction and on threads still running at exit. (A static pointer keeps it reachable, so
    // LeakSanitizer does not report it.)
    static Registry* const kRegistry = std::make_unique<Registry>().release();
    return *kRegistry;
}

FlightDataType::FlightDataType(Passkey /*passkey*/, std::optional<FlightDataTypeId> id,
                               std::string name, std::string saveKey, std::string typeDisplayKey,
                               std::string symbol, std::optional<UnitGroupId> unitGroupId,
                               std::shared_ptr<const UnitGroup> ownedUnits,
                               FlightDataTypeGroup group, int typePriority)
  : m_id(id),
    m_name(std::move(name)),
    m_saveKey(std::move(saveKey)),
    m_displayKey(std::move(typeDisplayKey)),
    m_symbol(std::move(symbol)),
    m_unitGroupId(unitGroupId),
    m_ownedUnits(std::move(ownedUnits)),
    m_units(m_unitGroupId.has_value() ? &unitGroup(*m_unitGroupId) : m_ownedUnits.get()),
    m_group(group),
    m_priority(typePriority),
    m_hashCode(Strings::javaHashCode(Strings::toLower(m_name)))
{
    QTROCKET_ASSERT(m_units != nullptr);
}

const FlightDataType& FlightDataType::builtin(FlightDataTypeId id)
{
    return *registry().builtins.at(indexOf(id));
}

std::span<const FlightDataType* const> FlightDataType::builtinTypes()
{
    return registry().builtins;
}

std::span<const FlightDataType* const> FlightDataType::allTypes()
{
    return registry().all;
}

std::span<const FlightDataType* const> FlightDataType::allTypesSorted()
{
    return registry().allSorted;
}

const FlightDataType& FlightDataType::getType(std::string_view name, std::string_view symbol,
                                              UnitGroupId units)
{
    return internType(name, symbol, units, nullptr);
}

const FlightDataType& FlightDataType::getTypeWithFixedUnit(std::string_view name,
                                                           std::string_view symbol,
                                                           std::string_view unit)
{
    return internType(name, symbol, std::nullopt,
                      std::make_shared<const FixedUnitGroup>(std::string(unit)));
}

const FlightDataType& FlightDataType::getType(std::string_view name, std::string_view symbol)
{
    return internType(name, symbol, std::nullopt, nullptr);
}

const FlightDataType& FlightDataType::internType(std::string_view name, std::string_view symbol,
                                                 std::optional<UnitGroupId>       unitGroupId,
                                                 std::shared_ptr<const UnitGroup> ownedUnits)
{
    Registry&              reg = registry();
    const std::scoped_lock lock{reg.guard};

    int         oldPriority = kDefaultPriority;
    std::string typeName(name);

    const auto found = reg.bySymbol.find(symbol);
    if (found != reg.bySymbol.end())
    {
        const FlightDataType& type = *found->second;

        // No name given (empty or blank): take the existing type's.
        if (Strings::isEmpty(typeName))
        {
            typeName = type.getName();
        }
        // No unit group given: take the existing type's.
        if (!unitGroupId.has_value() && ownedUnits == nullptr)
        {
            unitGroupId = type.m_unitGroupId;
            ownedUnits  = type.m_ownedUnits;
        }
        const UnitGroup& units = unitGroupId.has_value() ? unitGroup(*unitGroupId) : *ownedUnits;

        // When something has changed the old type is replaced; otherwise it is the answer.
        if (!units.equals(type.getUnitGroup()) || typeName != type.getName())
        {
            oldPriority = type.m_priority;
            reg.bySymbol.erase(found);
        }
        else
        {
            return type;
        }
    }

    if (!unitGroupId.has_value() && ownedUnits == nullptr)
    {
        unitGroupId = UnitGroupId::NONE;
    }

    // A custom type: no id, no save key, no translation key.
    auto type = std::make_unique<FlightDataType>(Passkey{}, std::nullopt, std::move(typeName),
                                                 std::string{}, std::string{}, std::string(symbol),
                                                 unitGroupId, std::move(ownedUnits),
                                                 FlightDataTypeGroup::CUSTOM, oldPriority);
    const FlightDataType& result = *type;
    reg.bySymbol.insert_or_assign(std::string(symbol), &result);
    reg.types.push_back(std::move(type));
    return result;
}

const FlightDataType* FlightDataType::getTypeBySaveKey(std::string_view saveKey)
{
    const Registry& reg   = registry();
    const auto      found = reg.bySaveKey.find(saveKey);
    return found == reg.bySaveKey.end() ? nullptr : found->second;
}

const FlightDataType* FlightDataType::findBySymbol(std::string_view symbol)
{
    Registry&              reg = registry();
    const std::scoped_lock lock{reg.guard};
    const auto             found = reg.bySymbol.find(symbol);
    return found == reg.bySymbol.end() ? nullptr : found->second;
}

const FlightDataType* FlightDataType::findByName(std::string_view name)
{
    for (const FlightDataType* type : allTypes())
    {
        if (type->getName() == name)
        {
            return type;
        }
    }
    return nullptr;
}

bool FlightDataType::equals(const FlightDataType& other) const noexcept
{
    return Strings::javaEqualsIgnoreCase(m_name, other.m_name);
}

int FlightDataType::compareTo(const FlightDataType& other) const noexcept
{
    const int groupCompare = QtRocket::compareTo(m_group, other.m_group);
    if (groupCompare != 0)
    {
        return groupCompare;
    }
    return m_priority - other.m_priority;
}

}  // namespace QtRocket
