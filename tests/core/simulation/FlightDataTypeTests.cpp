#include "QtRocket/simulation/FlightDataType.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/DataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace
{

using QtRocket::FlightDataType;
using QtRocket::kBuiltinFlightDataTypeCount;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;
using Id    = QtRocket::FlightDataTypeId;
using Group = QtRocket::FlightDataTypeGroup;
using Units = QtRocket::UnitGroupId;

/// One newType() call of FlightDataType.java, with the English text of its translation key.
struct Expected
{
    constexpr Expected(Id typeId, std::string_view typeSaveKey, std::string_view typeDisplayKey,
                       std::string_view typeName, std::string_view typeSymbol, Units typeUnits,
                       Group typeGroup, int typePriority)
      : id(typeId),
        saveKey(typeSaveKey),
        displayKey(typeDisplayKey),
        name(typeName),
        symbol(typeSymbol),
        units(typeUnits),
        group(typeGroup),
        priority(typePriority)
    {
    }

    Id               id;
    std::string_view saveKey;
    std::string_view displayKey;
    std::string_view name;
    std::string_view symbol;
    Units            units;
    Group            group;
    int              priority;
};

// Generated from FlightDataType.java (the TYPE_* fields in declaration order) and the core
// messages.properties, the symbols' non-ASCII characters as UTF-8 bytes.
constexpr std::array<Expected, 72> kExpected{{
    {Id::TYPE_TIME, "time", "FlightDataType.TYPE_TIME", "Time", "t", Units::LONG_TIME, Group::TIME,
     0},
    {Id::TYPE_ALTITUDE, "altitude", "FlightDataType.TYPE_ALTITUDE", "Altitude", "h",
     Units::DISTANCE, Group::POSITION_AND_MOTION, 0},
    {Id::TYPE_ALTITUDE_ABOVE_SEA, "altitude_above_sea", "FlightDataType.TYPE_ALTITUDE_ABOVE_SEA",
     "Altitude above sea level", "ha", Units::DISTANCE, Group::POSITION_AND_MOTION, 1},
    {Id::TYPE_VELOCITY_Z, "velocity_z", "FlightDataType.TYPE_VELOCITY_Z", "Vertical velocity", "Vz",
     Units::VELOCITY, Group::POSITION_AND_MOTION, 2},
    {Id::TYPE_VELOCITY_TOTAL, "velocity_total", "FlightDataType.TYPE_VELOCITY_TOTAL",
     "Total velocity", "Vt", Units::VELOCITY, Group::POSITION_AND_MOTION, 3},
    {Id::TYPE_ACCELERATION_Z, "acceleration_z", "FlightDataType.TYPE_ACCELERATION_Z",
     "Vertical acceleration", "Az", Units::ACCELERATION, Group::POSITION_AND_MOTION, 4},
    {Id::TYPE_ACCELERATION_X, "acceleration_x", "FlightDataType.TYPE_ACCELERATION_X",
     "Acceleration to the East", "Ax", Units::ACCELERATION, Group::POSITION_AND_MOTION, 5},
    {Id::TYPE_ACCELERATION_Y, "acceleration_y", "FlightDataType.TYPE_ACCELERATION_Y",
     "Acceleration to the North", "Ay", Units::ACCELERATION, Group::POSITION_AND_MOTION, 6},
    {Id::TYPE_ACCELERATION_BODYX, "acceleration_bodyx", "FlightDataType.TYPE_ACCELERATION_BODYX",
     "X body acceleration", "Abx", Units::ACCELERATION, Group::POSITION_AND_MOTION, 7},
    {Id::TYPE_ACCELERATION_BODYY, "acceleration_bodyy", "FlightDataType.TYPE_ACCELERATION_BODYY",
     "Y body acceleration", "Aby", Units::ACCELERATION, Group::POSITION_AND_MOTION, 8},
    {Id::TYPE_ACCELERATION_BODYZ, "acceleration_bodyz", "FlightDataType.TYPE_ACCELERATION_BODYZ",
     "Z body acceleration", "Abz", Units::ACCELERATION, Group::POSITION_AND_MOTION, 9},
    {Id::TYPE_ACCELERATION_TOTAL, "acceleration_total", "FlightDataType.TYPE_ACCELERATION_TOTAL",
     "Total acceleration", "At", Units::ACCELERATION, Group::POSITION_AND_MOTION, 10},
    {Id::TYPE_POSITION_X, "position_x", "FlightDataType.TYPE_POSITION_X", "Position East of launch",
     "Px", Units::DISTANCE, Group::POSITION_AND_MOTION, 11},
    {Id::TYPE_POSITION_Y, "position_y", "FlightDataType.TYPE_POSITION_Y",
     "Position North of launch", "Py", Units::DISTANCE, Group::POSITION_AND_MOTION, 12},
    {Id::TYPE_POSITION_XY, "position_xy", "FlightDataType.TYPE_POSITION_XY", "Lateral distance",
     "Pl", Units::DISTANCE, Group::POSITION_AND_MOTION, 13},
    {Id::TYPE_POSITION_DIRECTION, "position_direction", "FlightDataType.TYPE_POSITION_DIRECTION",
     "Lateral direction",
     "\xCE\xB8"
     "l",
     Units::ANGLE, Group::POSITION_AND_MOTION, 14},
    {Id::TYPE_VELOCITY_XY, "velocity_xy", "FlightDataType.TYPE_VELOCITY_XY", "Lateral velocity",
     "Vl", Units::VELOCITY, Group::POSITION_AND_MOTION, 15},
    {Id::TYPE_ACCELERATION_XY, "acceleration_xy", "FlightDataType.TYPE_ACCELERATION_XY",
     "Lateral acceleration", "Al", Units::ACCELERATION, Group::POSITION_AND_MOTION, 16},
    {Id::TYPE_LATITUDE, "latitude", "FlightDataType.TYPE_LATITUDE", "Latitude", "\xCF\x86",
     Units::LATITUDE, Group::POSITION_AND_MOTION, 17},
    {Id::TYPE_LONGITUDE, "longitude", "FlightDataType.TYPE_LONGITUDE", "Longitude", "\xCE\xBB",
     Units::LONGITUDE, Group::POSITION_AND_MOTION, 18},
    {Id::TYPE_AOA, "aoa", "FlightDataType.TYPE_AOA", "Angle of attack", "\xCE\xB1", Units::ANGLE,
     Group::ORIENTATION, 0},
    {Id::TYPE_ROLL_RATE, "roll_rate", "FlightDataType.TYPE_ROLL_RATE", "Roll rate (Z)", "d\xCE\xA6",
     Units::ROLL, Group::ORIENTATION, 1},
    {Id::TYPE_PITCH_RATE, "pitch_rate", "FlightDataType.TYPE_PITCH_RATE", "Pitch rate (Y)",
     "d\xCE\xB8", Units::ROLL, Group::ORIENTATION, 2},
    {Id::TYPE_YAW_RATE, "yaw_rate", "FlightDataType.TYPE_YAW_RATE", "Yaw rate (X)", "d\xCE\xA8",
     Units::ROLL, Group::ORIENTATION, 3},
    {Id::TYPE_ORIENTATION_THETA, "orientation_theta", "FlightDataType.TYPE_ORIENTATION_THETA",
     "Vertical orientation (zenith)", "\xCE\x98", Units::ANGLE, Group::ORIENTATION, 4},
    {Id::TYPE_ORIENTATION_PHI, "orientation_phi", "FlightDataType.TYPE_ORIENTATION_PHI",
     "Lateral orientation (azimuth)", "\xCE\xA6", Units::ANGLE, Group::ORIENTATION, 5},
    {Id::TYPE_MASS, "mass", "FlightDataType.TYPE_MASS", "Mass", "m", Units::MASS,
     Group::MASS_AND_INERTIA, 0},
    {Id::TYPE_MOTOR_MASS, "motor_mass", "FlightDataType.TYPE_MOTOR_MASS", "Motor mass", "mp",
     Units::MASS, Group::MASS_AND_INERTIA, 1},
    {Id::TYPE_LONGITUDINAL_INERTIA, "longitudinal_inertia",
     "FlightDataType.TYPE_LONGITUDINAL_INERTIA", "Longitudinal moment of inertia", "Il",
     Units::INERTIA, Group::MASS_AND_INERTIA, 2},
    {Id::TYPE_ROTATIONAL_INERTIA, "rotational_inertia", "FlightDataType.TYPE_ROTATIONAL_INERTIA",
     "Rotational moment of inertia", "Ir", Units::INERTIA, Group::MASS_AND_INERTIA, 3},
    {Id::TYPE_GRAVITY, "gravity", "FlightDataType.TYPE_GRAVITY", "Gravitational acceleration", "g",
     Units::ACCELERATION, Group::MASS_AND_INERTIA, 4},
    {Id::TYPE_CP_LOCATION, "cp_location", "FlightDataType.TYPE_CP_LOCATION", "CP location", "Cp",
     Units::LENGTH, Group::STABILITY, 0},
    {Id::TYPE_CG_LOCATION, "cg_location", "FlightDataType.TYPE_CG_LOCATION", "CG location", "Cg",
     Units::LENGTH, Group::STABILITY, 1},
    {Id::TYPE_STABILITY, "stability", "FlightDataType.TYPE_STABILITY", "Stability margin calibers",
     "S", Units::COEFFICIENT, Group::STABILITY, 2},
    {Id::TYPE_DAMPING_RATIO, "damping_ratio", "FlightDataType.TYPE_DAMPING_RATIO", "Damping ratio",
     "\xCE\xB6", Units::COEFFICIENT, Group::STABILITY, 3},
    {Id::TYPE_NATURAL_FREQUENCY, "natural_frequency", "FlightDataType.TYPE_NATURAL_FREQUENCY",
     "Natural frequency",
     "\xCF\x89"
     "n",
     Units::ROLL, Group::STABILITY, 4},
    {Id::TYPE_MACH_NUMBER, "mach_number", "FlightDataType.TYPE_MACH_NUMBER", "Mach number", "M",
     Units::COEFFICIENT, Group::CHARACTERISTIC_NUMBERS, 0},
    {Id::TYPE_REYNOLDS_NUMBER, "reynolds_number", "FlightDataType.TYPE_REYNOLDS_NUMBER",
     "Reynolds number", "R", Units::COEFFICIENT, Group::CHARACTERISTIC_NUMBERS, 1},
    {Id::TYPE_THRUST_FORCE, "thrust_force", "FlightDataType.TYPE_THRUST_FORCE", "Thrust", "Ft",
     Units::FORCE, Group::THRUST_AND_DRAG, 0},
    {Id::TYPE_THRUST_CORRECTION, "thrust_correction", "FlightDataType.TYPE_THRUST_CORRECTION",
     "Thrust Pressure Correction", "Fta", Units::FORCE, Group::THRUST_AND_DRAG, 0},
    {Id::TYPE_THRUST_WEIGHT_RATIO, "thrust_weight_ratio", "FlightDataType.TYPE_THRUST_WEIGHT_RATIO",
     "Thrust-to-weight ratio", "Twr", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 1},
    {Id::TYPE_DRAG_FORCE, "drag_force", "FlightDataType.TYPE_DRAG_FORCE", "Drag force", "Fd",
     Units::FORCE, Group::THRUST_AND_DRAG, 2},
    {Id::TYPE_DRAG_COEFF, "drag_coeff", "FlightDataType.TYPE_DRAG_COEFF", "Drag coefficient (CD)",
     "Cd", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 3},
    {Id::TYPE_FRICTION_DRAG_COEFF, "friction_drag_coeff", "FlightDataType.TYPE_FRICTION_DRAG_COEFF",
     "Friction drag coefficient (CD_friction)", "Cdf", Units::COEFFICIENT, Group::THRUST_AND_DRAG,
     4},
    {Id::TYPE_PRESSURE_DRAG_COEFF, "pressure_drag_coeff", "FlightDataType.TYPE_PRESSURE_DRAG_COEFF",
     "Pressure drag coefficient (CD_pressure)", "Cdp", Units::COEFFICIENT, Group::THRUST_AND_DRAG,
     5},
    {Id::TYPE_BASE_DRAG_COEFF, "base_drag_coeff", "FlightDataType.TYPE_BASE_DRAG_COEFF",
     "Base drag coefficient (CD_base)", "Cdb", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 6},
    {Id::TYPE_AXIAL_DRAG_COEFF, "axial_drag_coeff", "FlightDataType.TYPE_AXIAL_DRAG_COEFF",
     "Axial drag coefficient (CA)", "Cda", Units::COEFFICIENT, Group::THRUST_AND_DRAG, 7},
    {Id::TYPE_NORMAL_FORCE_COEFF, "normal_force_coeff", "FlightDataType.TYPE_NORMAL_FORCE_COEFF",
     "Normal force coefficient (CN)", "Cn", Units::COEFFICIENT, Group::COEFFICIENTS, 0},
    {Id::TYPE_CNA, "cna", "FlightDataType.TYPE_CNA",
     "Normal force coefficient derivative (CN\xCE\xB1"
     ")",
     "CN\xCE\xB1", Units::COEFFICIENT, Group::COEFFICIENTS, 1},
    {Id::TYPE_PITCH_MOMENT_COEFF, "pitch_moment_coeff", "FlightDataType.TYPE_PITCH_MOMENT_COEFF",
     "Pitch moment coefficient (Cm)", "C\xCE\xB8", Units::COEFFICIENT, Group::COEFFICIENTS, 2},
    {Id::TYPE_YAW_MOMENT_COEFF, "yaw_moment_coeff", "FlightDataType.TYPE_YAW_MOMENT_COEFF",
     "Yaw moment coefficient", "C\xCF\x84\xCE\xA8", Units::COEFFICIENT, Group::COEFFICIENTS, 3},
    {Id::TYPE_SIDE_FORCE_COEFF, "side_force_coeff", "FlightDataType.TYPE_SIDE_FORCE_COEFF",
     "Side force coefficient",
     "C\xCF\x84"
     "s",
     Units::COEFFICIENT, Group::COEFFICIENTS, 4},
    {Id::TYPE_ROLL_MOMENT_COEFF, "roll_moment_coeff", "FlightDataType.TYPE_ROLL_MOMENT_COEFF",
     "Roll moment coefficient", "C\xCF\x84\xCE\xA6", Units::COEFFICIENT, Group::COEFFICIENTS, 5},
    {Id::TYPE_ROLL_FORCING_COEFF, "roll_forcing_coeff", "FlightDataType.TYPE_ROLL_FORCING_COEFF",
     "Roll forcing coefficient", "Cf\xCE\xA6", Units::COEFFICIENT, Group::COEFFICIENTS, 6},
    {Id::TYPE_ROLL_DAMPING_COEFF, "roll_damping_coeff", "FlightDataType.TYPE_ROLL_DAMPING_COEFF",
     "Roll damping coefficient", "C\xCE\xB6\xCE\xA6", Units::COEFFICIENT, Group::COEFFICIENTS, 7},
    {Id::TYPE_PITCH_DAMPING_MOMENT_COEFF, "pitch_damping_moment_coeff",
     "FlightDataType.TYPE_PITCH_DAMPING_MOMENT_COEFF", "Pitch damping coefficient",
     "C\xCE\xB6\xCE\xB8", Units::COEFFICIENT, Group::COEFFICIENTS, 8},
    {Id::TYPE_YAW_DAMPING_MOMENT_COEFF, "yaw_damping_moment_coeff",
     "FlightDataType.TYPE_YAW_DAMPING_MOMENT_COEFF", "Yaw damping coefficient", "C\xCE\xB6\xCE\xA8",
     Units::COEFFICIENT, Group::COEFFICIENTS, 9},
    {Id::TYPE_DAMPING_MOMENT_COEFF, "damping_moment_coeff",
     "FlightDataType.TYPE_DAMPING_MOMENT_COEFF", "Damping moment coefficient", "Cdm",
     Units::ANGULAR_MOMENTUM, Group::COEFFICIENTS, 10},
    {Id::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC, "damping_moment_coeff_aerodynamic",
     "FlightDataType.TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC",
     "Damping moment coefficient (aerodynamic)", "Cdm_aero", Units::ANGULAR_MOMENTUM,
     Group::COEFFICIENTS, 11},
    {Id::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE, "damping_moment_coeff_propulsive",
     "FlightDataType.TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE",
     "Damping moment coefficient (propulsive)", "Cdm_prop", Units::ANGULAR_MOMENTUM,
     Group::COEFFICIENTS, 12},
    {Id::TYPE_CORRECTIVE_MOMENT_COEFF, "corrective_moment_coeff",
     "FlightDataType.TYPE_CORRECTIVE_MOMENT_COEFF", "Corrective moment coefficient", "Ccm",
     Units::MOMENT, Group::COEFFICIENTS, 13},
    {Id::TYPE_CORIOLIS_ACCELERATION, "coriolis_acceleration",
     "FlightDataType.TYPE_CORIOLIS_ACCELERATION", "Coriolis acceleration", "Ac",
     Units::ACCELERATION, Group::CUSTOM, 99},
    {Id::TYPE_REFERENCE_LENGTH, "reference_length", "FlightDataType.TYPE_REFERENCE_LENGTH",
     "Reference length", "Lr", Units::LENGTH, Group::REFERENCE_VALUES, 0},
    {Id::TYPE_REFERENCE_AREA, "reference_area", "FlightDataType.TYPE_REFERENCE_AREA",
     "Reference area", "Ar", Units::AREA, Group::REFERENCE_VALUES, 1},
    {Id::TYPE_WIND_VELOCITY, "wind_velocity", "FlightDataType.TYPE_WIND_VELOCITY", "Wind velocity",
     "Vw", Units::VELOCITY, Group::ATMOSPHERIC_CONDITIONS, 0},
    {Id::TYPE_WIND_DIRECTION, "wind_direction", "FlightDataType.TYPE_WIND_DIRECTION",
     "Wind direction",
     "\xCE\xB8"
     "w",
     Units::ANGLE, Group::ATMOSPHERIC_CONDITIONS, 1},
    {Id::TYPE_AIR_TEMPERATURE, "air_temperature", "FlightDataType.TYPE_AIR_TEMPERATURE",
     "Air temperature", "T", Units::TEMPERATURE, Group::ATMOSPHERIC_CONDITIONS, 2},
    {Id::TYPE_AIR_PRESSURE, "air_pressure", "FlightDataType.TYPE_AIR_PRESSURE", "Air pressure", "P",
     Units::PRESSURE, Group::ATMOSPHERIC_CONDITIONS, 3},
    {Id::TYPE_AIR_DENSITY, "air_density", "FlightDataType.TYPE_AIR_DENSITY", "Air density",
     "\xCF\x81", Units::DENSITY_BULK, Group::ATMOSPHERIC_CONDITIONS, 4},
    {Id::TYPE_SPEED_OF_SOUND, "speed_of_sound", "FlightDataType.TYPE_SPEED_OF_SOUND",
     "Speed of sound", "Vs", Units::VELOCITY, Group::ATMOSPHERIC_CONDITIONS, 5},
    {Id::TYPE_TIME_STEP, "time_step", "FlightDataType.TYPE_TIME_STEP", "Simulation time step", "dt",
     Units::TIME_STEP, Group::SIMULATION_INFORMATION, 0},
    {Id::TYPE_COMPUTATION_TIME, "computation_time", "FlightDataType.TYPE_COMPUTATION_TIME",
     "Computation time", "tc", Units::SHORT_TIME, Group::SIMULATION_INFORMATION, 1},
}};

// FlightDataType.ALL_TYPES, generated from FlightDataType.java.
constexpr std::array<Id, 71> kExpectedAllTypes{
    Id::TYPE_TIME,
    Id::TYPE_ALTITUDE,
    Id::TYPE_ALTITUDE_ABOVE_SEA,
    Id::TYPE_VELOCITY_Z,
    Id::TYPE_ACCELERATION_Z,
    Id::TYPE_ACCELERATION_BODYZ,
    Id::TYPE_VELOCITY_TOTAL,
    Id::TYPE_ACCELERATION_TOTAL,
    Id::TYPE_POSITION_X,
    Id::TYPE_ACCELERATION_X,
    Id::TYPE_ACCELERATION_BODYX,
    Id::TYPE_POSITION_Y,
    Id::TYPE_ACCELERATION_Y,
    Id::TYPE_ACCELERATION_BODYY,
    Id::TYPE_POSITION_XY,
    Id::TYPE_POSITION_DIRECTION,
    Id::TYPE_VELOCITY_XY,
    Id::TYPE_ACCELERATION_XY,
    Id::TYPE_LATITUDE,
    Id::TYPE_LONGITUDE,
    Id::TYPE_GRAVITY,
    Id::TYPE_AOA,
    Id::TYPE_ROLL_RATE,
    Id::TYPE_PITCH_RATE,
    Id::TYPE_YAW_RATE,
    Id::TYPE_MASS,
    Id::TYPE_MOTOR_MASS,
    Id::TYPE_LONGITUDINAL_INERTIA,
    Id::TYPE_ROTATIONAL_INERTIA,
    Id::TYPE_CP_LOCATION,
    Id::TYPE_CG_LOCATION,
    Id::TYPE_STABILITY,
    Id::TYPE_MACH_NUMBER,
    Id::TYPE_REYNOLDS_NUMBER,
    Id::TYPE_THRUST_FORCE,
    Id::TYPE_THRUST_WEIGHT_RATIO,
    Id::TYPE_DRAG_FORCE,
    Id::TYPE_DRAG_COEFF,
    Id::TYPE_AXIAL_DRAG_COEFF,
    Id::TYPE_FRICTION_DRAG_COEFF,
    Id::TYPE_PRESSURE_DRAG_COEFF,
    Id::TYPE_BASE_DRAG_COEFF,
    Id::TYPE_NORMAL_FORCE_COEFF,
    Id::TYPE_CNA,
    Id::TYPE_PITCH_MOMENT_COEFF,
    Id::TYPE_YAW_MOMENT_COEFF,
    Id::TYPE_SIDE_FORCE_COEFF,
    Id::TYPE_ROLL_MOMENT_COEFF,
    Id::TYPE_ROLL_FORCING_COEFF,
    Id::TYPE_ROLL_DAMPING_COEFF,
    Id::TYPE_PITCH_DAMPING_MOMENT_COEFF,
    Id::TYPE_YAW_DAMPING_MOMENT_COEFF,
    Id::TYPE_DAMPING_MOMENT_COEFF,
    Id::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC,
    Id::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE,
    Id::TYPE_CORRECTIVE_MOMENT_COEFF,
    Id::TYPE_DAMPING_RATIO,
    Id::TYPE_NATURAL_FREQUENCY,
    Id::TYPE_CORIOLIS_ACCELERATION,
    Id::TYPE_REFERENCE_LENGTH,
    Id::TYPE_REFERENCE_AREA,
    Id::TYPE_ORIENTATION_THETA,
    Id::TYPE_ORIENTATION_PHI,
    Id::TYPE_WIND_VELOCITY,
    Id::TYPE_WIND_DIRECTION,
    Id::TYPE_AIR_TEMPERATURE,
    Id::TYPE_AIR_PRESSURE,
    Id::TYPE_AIR_DENSITY,
    Id::TYPE_SPEED_OF_SOUND,
    Id::TYPE_TIME_STEP,
    Id::TYPE_COMPUTATION_TIME,
};

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// The built-in type at @p index of builtinTypes() is the one @p expected describes.
void expectBuiltinIdentity(std::size_t index, const Expected& expected,
                           const FlightDataType& actual)
{
    // The enum follows the Java declaration order.
    EXPECT_EQ(static_cast<std::size_t>(expected.id), index);
    EXPECT_EQ(&type(expected.id), &actual);
    EXPECT_EQ(actual.getId(), expected.id);
    EXPECT_TRUE(actual.isBuiltin());
}

/// The names of @p actual are those @p expected gives.
void expectBuiltinNames(const Expected& expected, const FlightDataType& actual)
{
    EXPECT_EQ(actual.getSaveKey(), expected.saveKey);
    EXPECT_EQ(actual.getDisplayKey(), expected.displayKey);
    EXPECT_EQ(actual.getName(), expected.name);
    EXPECT_EQ(actual.toString(), expected.name);
    EXPECT_EQ(actual.getSymbol(), expected.symbol);
}

/// The unit group, group and priority of @p actual are those @p expected gives.
void expectBuiltinUnitsAndOrder(const Expected& expected, const FlightDataType& actual)
{
    EXPECT_EQ(actual.getUnitGroupId(), expected.units);
    EXPECT_EQ(&actual.getUnitGroup(), &unitGroup(expected.units));
    EXPECT_EQ(actual.getGroup(), expected.group);
    EXPECT_EQ(actual.getPriority(), expected.priority);
    EXPECT_EQ(actual.getGroupPriority(), QtRocket::priority(expected.group));
}

TEST(FlightDataType, SeventyTwoBuiltinsInDeclarationOrder)
{
    ASSERT_EQ(kBuiltinFlightDataTypeCount, 72U);
    const std::span<const FlightDataType* const> builtins = FlightDataType::builtinTypes();
    ASSERT_EQ(builtins.size(), kExpected.size());
    for (std::size_t i = 0; i < kExpected.size(); i++)
    {
        SCOPED_TRACE(kExpected.at(i).saveKey);
        expectBuiltinIdentity(i, kExpected.at(i), *builtins[i]);
        expectBuiltinNames(kExpected.at(i), *builtins[i]);
        expectBuiltinUnitsAndOrder(kExpected.at(i), *builtins[i]);
    }
}

TEST(FlightDataType, BuiltinsAreInterned)
{
    // The same object on every call, reachable through a DataType reference.
    const FlightDataType&     time     = type(Id::TYPE_TIME);
    const QtRocket::DataType& dataType = time;
    EXPECT_EQ(&type(Id::TYPE_TIME), &time);
    EXPECT_EQ(&dataType.getUnitGroup(), &unitGroup(UnitGroupId::LONG_TIME));
    EXPECT_EQ(dataType.getName(), "Time");
    EXPECT_EQ(dataType.getSymbol(), "t");
    EXPECT_NE(&type(Id::TYPE_TIME), &type(Id::TYPE_ALTITUDE));
}

TEST(FlightDataType, NamesSymbolsAndKeysAreUnique)
{
    std::set<std::string> names;
    std::set<std::string> lowerNames;
    std::set<std::string> symbols;
    std::set<std::string> saveKeys;
    std::set<std::string> displayKeys;
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        names.insert(t->getName());
        // equals() ignores case, so a clash ignoring case would merge two columns of a branch.
        lowerNames.insert(QtRocket::Strings::toLower(t->getName()));
        symbols.insert(t->getSymbol());
        saveKeys.insert(t->getSaveKey());
        displayKeys.insert(t->getDisplayKey());
    }
    EXPECT_EQ(names.size(), kBuiltinFlightDataTypeCount);
    EXPECT_EQ(lowerNames.size(), kBuiltinFlightDataTypeCount);
    EXPECT_EQ(symbols.size(), kBuiltinFlightDataTypeCount);
    EXPECT_EQ(saveKeys.size(), kBuiltinFlightDataTypeCount);
    EXPECT_EQ(displayKeys.size(), kBuiltinFlightDataTypeCount);
}

/// Whether @p type equals only itself among the built-in types.
void expectEqualsOnlyItself(const FlightDataType& type)
{
    for (const FlightDataType* other : FlightDataType::builtinTypes())
    {
        EXPECT_EQ(type.equals(*other), &type == other)
            << type.getName() << " / " << other->getName();
    }
}

TEST(FlightDataType, BuiltinsEqualOnlyThemselves)
{
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        expectEqualsOnlyItself(*t);
    }
}

TEST(FlightDataType, SaveKeyIsTheLowerCaseFieldName)
{
    // "FlightDataType.TYPE_DRAG_COEFF" has the save key "drag_coeff".
    constexpr std::string_view kPrefix = "FlightDataType.TYPE_";
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        const std::string& key = t->getDisplayKey();
        ASSERT_TRUE(key.starts_with(kPrefix)) << key;
        EXPECT_EQ(t->getSaveKey(), QtRocket::Strings::toLower(key.substr(kPrefix.size())));
    }
}

TEST(FlightDataType, AllTypesIsJavasArray)
{
    const std::span<const FlightDataType* const> all = FlightDataType::allTypes();
    ASSERT_EQ(all.size(), kExpectedAllTypes.size());
    for (std::size_t i = 0; i < all.size(); i++)
    {
        EXPECT_EQ(all[i], &type(kExpectedAllTypes.at(i))) << i;
    }
    // Every built-in type but TYPE_THRUST_CORRECTION, each once.
    EXPECT_EQ(std::ranges::find(all, &type(Id::TYPE_THRUST_CORRECTION)), all.end());
    const std::set<const FlightDataType*> distinct(all.begin(), all.end());
    EXPECT_EQ(distinct.size(), kBuiltinFlightDataTypeCount - 1);
}

/// @p first comes before @p second in a stable sort of @p all.
void expectSortedPair(std::span<const FlightDataType* const> all, const FlightDataType& first,
                      const FlightDataType& second)
{
    const int order = first.compareTo(second);
    EXPECT_LE(order, 0) << first.getName() << " / " << second.getName();
    if (order == 0)
    {
        // Ties keep the ALL_TYPES order.
        EXPECT_LT(std::ranges::find(all, &first), std::ranges::find(all, &second));
    }
}

TEST(FlightDataType, AllTypesSortedIsAStableSort)
{
    const std::span<const FlightDataType* const> all    = FlightDataType::allTypes();
    const std::span<const FlightDataType* const> sorted = FlightDataType::allTypesSorted();
    ASSERT_EQ(sorted.size(), all.size());
    const std::set<const FlightDataType*> sortedSet(sorted.begin(), sorted.end());
    EXPECT_EQ(sortedSet, std::set<const FlightDataType*>(all.begin(), all.end()));
    for (std::size_t i = 1; i < sorted.size(); i++)
    {
        expectSortedPair(all, *sorted[i - 1], *sorted[i]);
    }
}

TEST(FlightDataType, AllTypesSortedEnds)
{
    const std::span<const FlightDataType* const> sorted = FlightDataType::allTypesSorted();
    ASSERT_EQ(sorted.size(), 71U);
    EXPECT_EQ(sorted.front(), &type(Id::TYPE_TIME));
    EXPECT_EQ(sorted[1], &type(Id::TYPE_ALTITUDE));
    // The simulation information, then the one built-in type in the CUSTOM group.
    EXPECT_EQ(sorted[sorted.size() - 3], &type(Id::TYPE_TIME_STEP));
    EXPECT_EQ(sorted[sorted.size() - 2], &type(Id::TYPE_COMPUTATION_TIME));
    EXPECT_EQ(sorted.back(), &type(Id::TYPE_CORIOLIS_ACCELERATION));
}

TEST(FlightDataType, GetTypeBySaveKeyFindsEveryBuiltin)
{
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        EXPECT_EQ(FlightDataType::getTypeBySaveKey(t->getSaveKey()), t);
    }
}

TEST(FlightDataType, GetTypeBySaveKey)
{
    EXPECT_EQ(FlightDataType::getTypeBySaveKey("thrust_correction"),
              &type(Id::TYPE_THRUST_CORRECTION));
    EXPECT_EQ(FlightDataType::getTypeBySaveKey("drag_coeff"), &type(Id::TYPE_DRAG_COEFF));
    EXPECT_EQ(FlightDataType::getTypeBySaveKey("Time"),
              nullptr);  // keys are case-sensitive
    EXPECT_EQ(FlightDataType::getTypeBySaveKey("TYPE_TIME"), nullptr);
    EXPECT_EQ(FlightDataType::getTypeBySaveKey(""), nullptr);
    // A custom type has no save key of its own: getSaveKey() is its name, not a key.
    const FlightDataType& custom =
        FlightDataType::getType("QtRocket test save key", "qtrSaveKey", UnitGroupId::LENGTH);
    EXPECT_EQ(custom.getSaveKey(), "QtRocket test save key");
    EXPECT_EQ(FlightDataType::getTypeBySaveKey(custom.getSaveKey()), nullptr);
}

TEST(FlightDataType, FindByNameFindsAllTypes)
{
    for (const FlightDataType* t : FlightDataType::allTypes())
    {
        EXPECT_EQ(FlightDataType::findByName(t->getName()), t);
    }
}

TEST(FlightDataType, FindByNameSearchesAllTypesExactly)
{
    EXPECT_EQ(FlightDataType::findByName("Drag coefficient (CD)"), &type(Id::TYPE_DRAG_COEFF));
    EXPECT_EQ(FlightDataType::findByName("Normal force coefficient derivative (CN\xCE\xB1)"),
              &type(Id::TYPE_CNA));
    // Exact match only, and only ALL_TYPES, as OpenRocket's .ork reader looks.
    EXPECT_EQ(FlightDataType::findByName("time"), nullptr);
    EXPECT_EQ(FlightDataType::findByName("Drag coefficient"), nullptr);
    EXPECT_EQ(FlightDataType::findByName("Thrust Pressure Correction"), nullptr);
    EXPECT_EQ(FlightDataType::findByName(FlightDataType::kLegacyUpwindName), nullptr);
    EXPECT_EQ(FlightDataType::kLegacyUpwindName, "Position upwind");
}

TEST(FlightDataType, FindBySymbol)
{
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        EXPECT_EQ(FlightDataType::findBySymbol(t->getSymbol()), t);
    }
    EXPECT_EQ(FlightDataType::findBySymbol("Fta"), &type(Id::TYPE_THRUST_CORRECTION));
    EXPECT_EQ(FlightDataType::findBySymbol("\xCF\x81"), &type(Id::TYPE_AIR_DENSITY));
    EXPECT_EQ(FlightDataType::findBySymbol("H"),
              nullptr);  // symbols are case-sensitive
    EXPECT_EQ(FlightDataType::findBySymbol("qtrNoSuchSymbol"), nullptr);
}

TEST(FlightDataType, GetTypeReturnsTheBuiltinWhenNothingChanges)
{
    const FlightDataType& altitude = type(Id::TYPE_ALTITUDE);
    EXPECT_EQ(&FlightDataType::getType("Altitude", "h", UnitGroupId::DISTANCE), &altitude);
    // An empty or blank name, and no unit group, take the existing type's.
    EXPECT_EQ(&FlightDataType::getType("", "h", UnitGroupId::DISTANCE), &altitude);
    EXPECT_EQ(&FlightDataType::getType(" \t", "h"), &altitude);
    EXPECT_EQ(&FlightDataType::getType("Altitude", "h"), &altitude);
    EXPECT_EQ(&FlightDataType::getType("", "\xCE\xB1"), &type(Id::TYPE_AOA));
    EXPECT_EQ(FlightDataType::findBySymbol("h"), &altitude);
}

TEST(FlightDataType, GetTypeMakesACustomType)
{
    const FlightDataType& custom =
        FlightDataType::getType("QtRocket test custom", "qtrCustom", UnitGroupId::LENGTH);
    EXPECT_FALSE(custom.isBuiltin());
    EXPECT_EQ(custom.getId(), std::nullopt);
    EXPECT_EQ(custom.getName(), "QtRocket test custom");
    EXPECT_EQ(custom.getSymbol(), "qtrCustom");
    EXPECT_EQ(custom.getUnitGroupId(), UnitGroupId::LENGTH);
    EXPECT_EQ(&custom.getUnitGroup(), &unitGroup(UnitGroupId::LENGTH));
    EXPECT_EQ(custom.getGroup(), Group::CUSTOM);
    EXPECT_EQ(custom.getPriority(), FlightDataType::kDefaultPriority);
    EXPECT_EQ(FlightDataType::kDefaultPriority, 999);
    EXPECT_EQ(custom.getDisplayKey(), "");
    // Interned: the same arguments give the same object, and the symbol finds it.
    EXPECT_EQ(&FlightDataType::getType("QtRocket test custom", "qtrCustom", UnitGroupId::LENGTH),
              &custom);
    EXPECT_EQ(&FlightDataType::getType("", "qtrCustom"), &custom);
    EXPECT_EQ(FlightDataType::findBySymbol("qtrCustom"), &custom);
    // Not a built-in type: absent from the arrays and the name lookup.
    EXPECT_EQ(std::ranges::find(FlightDataType::builtinTypes(), &custom),
              FlightDataType::builtinTypes().end());
    EXPECT_EQ(FlightDataType::findByName("QtRocket test custom"), nullptr);
}

TEST(FlightDataType, GetTypeWithoutUnitsGivesUnitsNone)
{
    const FlightDataType& custom = FlightDataType::getType("QtRocket test no units", "qtrNoUnits");
    EXPECT_EQ(custom.getUnitGroupId(), UnitGroupId::NONE);
    EXPECT_EQ(&custom.getUnitGroup(), &unitGroup(UnitGroupId::NONE));
    EXPECT_EQ(&FlightDataType::getType("QtRocket test no units", "qtrNoUnits"), &custom);
}

TEST(FlightDataType, GetTypeReplacesATypeWhoseNameChanged)
{
    const FlightDataType& first =
        FlightDataType::getType("QtRocket test rename", "qtrRename", UnitGroupId::MASS);
    const FlightDataType& renamed =
        FlightDataType::getType("QtRocket test renamed", "qtrRename", UnitGroupId::MASS);
    EXPECT_NE(&renamed, &first);
    EXPECT_EQ(renamed.getName(), "QtRocket test renamed");
    EXPECT_EQ(renamed.getPriority(),
              first.getPriority());  // the old type's priority
    EXPECT_EQ(renamed.getUnitGroupId(), UnitGroupId::MASS);
    EXPECT_EQ(FlightDataType::findBySymbol("qtrRename"), &renamed);
    // The old type stays valid and unchanged for whoever holds it.
    EXPECT_EQ(first.getName(), "QtRocket test rename");
    EXPECT_EQ(first.getSymbol(), "qtrRename");
    // A name differing only in case is a change as well (String.equals).
    const FlightDataType& recased =
        FlightDataType::getType("QtRocket TEST renamed", "qtrRename", UnitGroupId::MASS);
    EXPECT_NE(&recased, &renamed);
    EXPECT_TRUE(recased.equals(renamed));
    // No unit group given: the replacement keeps the old one.
    const FlightDataType& again = FlightDataType::getType("QtRocket test renamed", "qtrRename");
    EXPECT_NE(&again, &recased);
    EXPECT_EQ(again.getUnitGroupId(), UnitGroupId::MASS);
}

TEST(FlightDataType, GetTypeReplacesATypeWhoseUnitsChanged)
{
    const FlightDataType& first =
        FlightDataType::getType("QtRocket test units", "qtrUnits", UnitGroupId::LENGTH);
    const FlightDataType& changed =
        FlightDataType::getType("QtRocket test units", "qtrUnits", UnitGroupId::VELOCITY);
    EXPECT_NE(&changed, &first);
    EXPECT_EQ(changed.getUnitGroupId(), UnitGroupId::VELOCITY);
    EXPECT_EQ(first.getUnitGroupId(), UnitGroupId::LENGTH);
    EXPECT_EQ(changed.getGroup(), Group::CUSTOM);
    // The two are equal (same name) but distinct objects.
    EXPECT_TRUE(changed.equals(first));
    EXPECT_EQ(changed.hashCode(), first.hashCode());
    EXPECT_EQ(FlightDataType::findBySymbol("qtrUnits"), &changed);
}

TEST(FlightDataType, GetTypeWithAFixedUnitGroup)
{
    // A custom expression whose unit is not an SI unit gets a FixedUnitGroup, which the type
    // keeps.
    const FlightDataType& custom =
        FlightDataType::getTypeWithFixedUnit("QtRocket test fixed", "qtrFixed", "furlong");
    EXPECT_EQ(custom.getUnitGroupId(), std::nullopt);
    EXPECT_EQ(custom.getUnitGroup().getDefaultUnit().getUnit(), "furlong");
    EXPECT_EQ(custom.getUnitGroup().toString(), "FixedUnitGroup:furlong");
    EXPECT_EQ(custom.getGroup(), Group::CUSTOM);
    EXPECT_EQ(custom.getPriority(), FlightDataType::kDefaultPriority);

    // All FixedUnitGroups are equal to each other (no units), so another unit string does not
    // count as a change, as in OpenRocket.
    EXPECT_EQ(&FlightDataType::getTypeWithFixedUnit("QtRocket test fixed", "qtrFixed", "parsec"),
              &custom);
    EXPECT_EQ(custom.getUnitGroup().getDefaultUnit().getUnit(), "furlong");
}

TEST(FlightDataType, GetTypeReplacesAFixedUnitGroup)
{
    const FlightDataType& fixed =
        FlightDataType::getTypeWithFixedUnit("QtRocket test fixed 3", "qtrFixed3", "furlong");
    // UNITS_NONE has units, so it is a change, and back again.
    const FlightDataType& none =
        FlightDataType::getType("QtRocket test fixed 3", "qtrFixed3", UnitGroupId::NONE);
    EXPECT_NE(&none, &fixed);
    EXPECT_EQ(none.getUnitGroupId(), UnitGroupId::NONE);
    const FlightDataType& fixedAgain =
        FlightDataType::getTypeWithFixedUnit("QtRocket test fixed 3", "qtrFixed3", "rod");
    EXPECT_NE(&fixedAgain, &none);
    EXPECT_EQ(fixedAgain.getUnitGroup().getDefaultUnit().getUnit(), "rod");
}

TEST(FlightDataType, GetTypeWithoutUnitsKeepsTheFixedUnitGroup)
{
    // A rename without a unit group keeps the existing group object.
    const FlightDataType& renamed =
        FlightDataType::getTypeWithFixedUnit("QtRocket test fixed 2", "qtrFixed2", "cubit");
    const FlightDataType& renamedAgain =
        FlightDataType::getType("QtRocket test fixed 2 renamed", "qtrFixed2");
    EXPECT_NE(&renamedAgain, &renamed);
    EXPECT_EQ(&renamedAgain.getUnitGroup(), &renamed.getUnitGroup());
    EXPECT_EQ(renamedAgain.getUnitGroupId(), std::nullopt);
    EXPECT_EQ(renamedAgain.getUnitGroup().getDefaultUnit().getUnit(), "cubit");
}

TEST(FlightDataType, GetTypeWithoutANameOrUnitsIsALookup)
{
    // Java's getType(null, symbol, null), what range and index expressions call: the existing
    // type, or IllegalArgumentException("typeName is null"), and nothing is registered.
    EXPECT_EQ(&FlightDataType::getType("", "Cd"), &type(Id::TYPE_DRAG_COEFF));
    EXPECT_THROW(static_cast<void>(FlightDataType::getType("", "qtrUnnamedLookup")),
                 QtRocket::BugError);
    EXPECT_EQ(FlightDataType::findBySymbol("qtrUnnamedLookup"), nullptr);
    // A blank name is a name, as in Java.
    const FlightDataType& blank = FlightDataType::getType(" ", "qtrBlankName");
    EXPECT_EQ(blank.getName(), " ");
    EXPECT_EQ(blank.getUnitGroupId(), UnitGroupId::NONE);
}

TEST(FlightDataType, GetTypeWithAnEmptyNameAndAnUnknownSymbol)
{
    // With a unit group an empty name is Java's "" (a custom expression without a name), which
    // makes a type of that name.
    const FlightDataType& unnamed = FlightDataType::getType("", "qtrUnnamed", UnitGroupId::MASS);
    EXPECT_EQ(unnamed.getName(), "");
    EXPECT_EQ(unnamed.getUnitGroupId(), UnitGroupId::MASS);
    EXPECT_EQ(unnamed.getGroup(), Group::CUSTOM);
    EXPECT_EQ(&FlightDataType::getType("", "qtrUnnamed", UnitGroupId::MASS), &unnamed);
    EXPECT_EQ(&FlightDataType::getType("", "qtrUnnamed"), &unnamed);
}

TEST(FlightDataType, BuiltinRejectsAnIdOutsideTheEnum)
{
    EXPECT_THROW(
        static_cast<void>(FlightDataType::builtin(static_cast<Id>(kBuiltinFlightDataTypeCount))),
        QtRocket::BugError);
    EXPECT_THROW(static_cast<void>(FlightDataType::builtin(static_cast<Id>(-1))),
                 QtRocket::BugError);
}

/// @p custom is a CUSTOM type named @p name, not built in and without a save key or translation
/// key of its own.
void expectCustomType(const FlightDataType& custom, std::string_view name)
{
    EXPECT_EQ(custom.getName(), name);
    EXPECT_EQ(custom.getGroup(), Group::CUSTOM);
    EXPECT_FALSE(custom.isBuiltin());
    EXPECT_EQ(custom.getSaveKey(), name);
    EXPECT_EQ(custom.getDisplayKey(), "");
}

/// @p replacement has replaced the built-in type @p id for its symbol: another object with the
/// built-in type's priority. The built-in type itself is still found by id and save key.
void expectReplaced(const FlightDataType& replacement, Id id)
{
    const FlightDataType& builtin = type(id);
    EXPECT_NE(&replacement, &builtin);
    EXPECT_EQ(replacement.getPriority(), builtin.getPriority());
    EXPECT_EQ(FlightDataType::findBySymbol(builtin.getSymbol()), &replacement);
    EXPECT_EQ(FlightDataType::getTypeBySaveKey(builtin.getSaveKey()), &builtin);
    EXPECT_EQ(builtin.getId(), id);
}

/// A new name for the altitude symbol "h".
void replaceAltitude()
{
    const FlightDataType& replacement =
        FlightDataType::getType("My height", "h", UnitGroupId::DISTANCE);
    expectCustomType(replacement, "My height");
    expectReplaced(replacement, Id::TYPE_ALTITUDE);
    EXPECT_EQ(replacement.getPriority(), 0);
    EXPECT_EQ(replacement.getUnitGroupId(), UnitGroupId::DISTANCE);
    EXPECT_EQ(FlightDataType::findByName("Altitude"), &type(Id::TYPE_ALTITUDE));
}

/// The built-in name again, after replaceAltitude(): one more CUSTOM type, not the built-in one.
void renameAltitudeBack()
{
    const FlightDataType* const previous = FlightDataType::findBySymbol("h");
    const FlightDataType& again = FlightDataType::getType("Altitude", "h", UnitGroupId::DISTANCE);
    expectCustomType(again, "Altitude");
    expectReplaced(again, Id::TYPE_ALTITUDE);
    EXPECT_NE(&again, previous);
    EXPECT_TRUE(again.equals(type(Id::TYPE_ALTITUDE)));
}

/// New units for the drag coefficient, the name taken from the built-in type.
void replaceDragCoefficientUnits()
{
    const FlightDataType& inMetres = FlightDataType::getType("", "Cd", UnitGroupId::LENGTH);
    expectCustomType(inMetres, "Drag coefficient (CD)");
    expectReplaced(inMetres, Id::TYPE_DRAG_COEFF);
    EXPECT_EQ(inMetres.getPriority(), 3);
    EXPECT_EQ(inMetres.getUnitGroupId(), UnitGroupId::LENGTH);
}

/// Replaces built-in types by their symbols and checks the replacements as Java makes them. The
/// replacements are for good, so this runs only in the child process of a death test, and exits
/// with 1 when a check failed (a failure in the child is not reported otherwise).
[[noreturn]] void replaceBuiltinsAndExit()
{
    replaceAltitude();
    renameAltitudeBack();
    replaceDragCoefficientUnits();
    // NOLINTNEXTLINE(concurrency-mt-unsafe): the child of a death test, with no other threads
    std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

TEST(FlightDataTypeDeathTest, GetTypeReplacesABuiltin)
{
    // Java: EXISTING_TYPES.remove(symbol), then newType(s, symbol, u, oldPriority).
    EXPECT_EXIT(replaceBuiltinsAndExit(), ::testing::ExitedWithCode(0), "");
    // The replacements were made in the child process only.
    EXPECT_EQ(FlightDataType::findBySymbol("h"), &type(Id::TYPE_ALTITUDE));
    EXPECT_EQ(FlightDataType::findBySymbol("Cd"), &type(Id::TYPE_DRAG_COEFF));
}

TEST(FlightDataType, GetTypeIsThreadSafe)
{
    constexpr std::size_t                       kThreads = 8;
    std::array<const FlightDataType*, kThreads> results{};
    std::vector<std::jthread>                   threads;
    threads.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; i++)
    {
        threads.emplace_back([&results, i] {
            const FlightDataType* found = nullptr;
            for (int n = 0; n < 100; n++)
            {
                found = &FlightDataType::getType("QtRocket test threads", "qtrThreads",
                                                 UnitGroupId::FORCE);
            }
            results.at(i) = found;
        });
    }
    threads.clear();  // joins
    for (const FlightDataType* result : results)
    {
        EXPECT_EQ(result, results.front());
    }
    EXPECT_EQ(FlightDataType::findBySymbol("qtrThreads"), results.front());
}

constexpr std::string_view kRaceName   = "QtRocket test race";
constexpr std::string_view kRaceSymbol = "qtrRace";
constexpr std::size_t      kRaceRounds = 200;

/// Replaces the race type again and again, the units alternating from @p first; counts the
/// types made wrong in @p bad.
void raceWriter(std::size_t first, std::atomic<int>& bad)
{
    for (std::size_t n = first; n < first + kRaceRounds; n++)
    {
        const UnitGroupId     units = n % 2 == 0 ? UnitGroupId::LENGTH : UnitGroupId::VELOCITY;
        const FlightDataType& made  = FlightDataType::getType(kRaceName, kRaceSymbol, units);
        if (made.getUnitGroupId() != units || made.getName() != kRaceName)
        {
            bad++;
        }
    }
}

/// Looks the race type up again and again; counts the types found missing or not whole in @p bad.
void raceReader(std::atomic<int>& bad)
{
    const QtRocket::UnitGroup& length   = unitGroup(UnitGroupId::LENGTH);
    const QtRocket::UnitGroup& velocity = unitGroup(UnitGroupId::VELOCITY);
    for (std::size_t n = 0; n < kRaceRounds; n++)
    {
        const FlightDataType* found = FlightDataType::findBySymbol(kRaceSymbol);
        if (found == nullptr || found->getName() != kRaceName ||
            found->getSymbol() != kRaceSymbol ||
            (&found->getUnitGroup() != &length && &found->getUnitGroup() != &velocity))
        {
            bad++;
        }
    }
}

TEST(FlightDataType, GetTypeReplacesSafelyWhileOthersLookUp)
{
    // Writers keep replacing the type of one symbol (the units alternate), readers keep looking
    // it up: every type found must be whole. Counted, not asserted, in the threads.
    constexpr std::size_t kThreads = 4;
    static_cast<void>(FlightDataType::getType(kRaceName, kRaceSymbol, UnitGroupId::LENGTH));

    std::atomic<int> badWrites{0};
    std::atomic<int> badReads{0};
    {
        std::vector<std::jthread> threads;
        threads.reserve(2 * kThreads);
        for (std::size_t i = 0; i < kThreads; i++)
        {
            threads.emplace_back(raceWriter, i, std::ref(badWrites));
            threads.emplace_back(raceReader, std::ref(badReads));
        }
    }  // joins
    EXPECT_EQ(badWrites.load(), 0);
    EXPECT_EQ(badReads.load(), 0);
    const FlightDataType* last = FlightDataType::findBySymbol(kRaceSymbol);
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->getName(), kRaceName);
    EXPECT_EQ(last->getPriority(), FlightDataType::kDefaultPriority);
}

TEST(FlightDataType, EqualsIgnoresCaseOfTheName)
{
    const FlightDataType& altitude = type(Id::TYPE_ALTITUDE);
    const FlightDataType& lower =
        FlightDataType::getType("altitude", "qtrLowerAltitude", UnitGroupId::DISTANCE);
    EXPECT_TRUE(altitude.equals(altitude));
    EXPECT_TRUE(altitude.equals(lower));
    EXPECT_TRUE(lower.equals(altitude));
    EXPECT_EQ(altitude.hashCode(), lower.hashCode());
    EXPECT_FALSE(altitude.equals(type(Id::TYPE_ALTITUDE_ABOVE_SEA)));
    // Java's String.compareToIgnoreCase folds non-ASCII letters too.
    const FlightDataType& greek =
        FlightDataType::getType("\xCE\x98-test", "qtrGreekUpper", UnitGroupId::NONE);
    const FlightDataType& greekLower =
        FlightDataType::getType("\xCE\xB8-TEST", "qtrGreekLower", UnitGroupId::NONE);
    EXPECT_TRUE(greek.equals(greekLower));
    EXPECT_EQ(greek.hashCode(), greekLower.hashCode());
}

TEST(FlightDataType, HashCodeIsJavasHashOfTheLowerCaseName)
{
    // "time".hashCode() in Java.
    EXPECT_EQ(type(Id::TYPE_TIME).hashCode(), 3560141);
    // name.toLowerCase(Locale.ENGLISH).hashCode() in Java, for a name with a Greek letter.
    EXPECT_EQ(type(Id::TYPE_CNA).hashCode(), 1324030731);
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        EXPECT_EQ(t->hashCode(),
                  QtRocket::Strings::javaHashCode(QtRocket::Strings::toLower(t->getName())));
    }
}

TEST(FlightDataType, HashCodeLowerCasesNonAsciiLetters)
{
    // Java: 29506 and 190199131 for both spellings.
    const FlightDataType& deltaUpper =
        FlightDataType::getType("\xCE\x94v", "qtrHashDeltaUpper", UnitGroupId::VELOCITY);
    const FlightDataType& deltaLower =
        FlightDataType::getType("\xCE\xB4v", "qtrHashDeltaLower", UnitGroupId::VELOCITY);
    EXPECT_EQ(deltaUpper.hashCode(), 29506);
    EXPECT_EQ(deltaLower.hashCode(), 29506);
    const FlightDataType& thetaUpper =
        FlightDataType::getType("\xCE\x98-PROBE", "qtrHashThetaUpper", UnitGroupId::NONE);
    const FlightDataType& thetaLower =
        FlightDataType::getType("\xCE\xB8-probe", "qtrHashThetaLower", UnitGroupId::NONE);
    EXPECT_EQ(thetaUpper.hashCode(), 190199131);
    EXPECT_EQ(thetaLower.hashCode(), 190199131);
}

TEST(FlightDataType, EqualTypesHashAlike)
{
    // Deviation: Java hashes "\u00B5m" (micro sign) as 5720 and "\u03BCm" (mu) as 29745 although
    // the two are equal ignoring case; here both hash as Java hashes "\u03BCm".
    const FlightDataType& micro =
        FlightDataType::getType("\xC2\xB5m", "qtrHashMicro", UnitGroupId::LENGTH);
    const FlightDataType& mu =
        FlightDataType::getType("\xCE\x9CM", "qtrHashMu", UnitGroupId::LENGTH);
    ASSERT_TRUE(micro.equals(mu));
    EXPECT_EQ(micro.hashCode(), mu.hashCode());
    EXPECT_EQ(mu.hashCode(), 29745);
    const FlightDataType& longS =
        FlightDataType::getType("Po\xC5\xBFition", "qtrHashLongS", UnitGroupId::LENGTH);
    const FlightDataType& plainS =
        FlightDataType::getType("POSITION", "qtrHashPlainS", UnitGroupId::LENGTH);
    ASSERT_TRUE(longS.equals(plainS));
    EXPECT_EQ(longS.hashCode(), plainS.hashCode());
}

TEST(FlightDataType, CompareToOrdersByGroupThenPriority)
{
    const FlightDataType& time     = type(Id::TYPE_TIME);
    const FlightDataType& altitude = type(Id::TYPE_ALTITUDE);
    // Group difference: POSITION_AND_MOTION (10) - TIME (0).
    EXPECT_EQ(altitude.compareTo(time), 10);
    EXPECT_EQ(time.compareTo(altitude), -10);
    // Same group: the priority difference.
    EXPECT_EQ(type(Id::TYPE_ACCELERATION_TOTAL).compareTo(altitude), 10);
    EXPECT_EQ(altitude.compareTo(type(Id::TYPE_POSITION_XY)), -13);
    // Equal group and priority compare equal although the types differ.
    EXPECT_EQ(type(Id::TYPE_THRUST_FORCE).compareTo(type(Id::TYPE_THRUST_CORRECTION)), 0);
    EXPECT_FALSE(type(Id::TYPE_THRUST_FORCE).equals(type(Id::TYPE_THRUST_CORRECTION)));
    // Custom types come after TYPE_CORIOLIS_ACCELERATION (CUSTOM, 99).
    const FlightDataType& custom =
        FlightDataType::getType("QtRocket test order", "qtrOrder", UnitGroupId::NONE);
    EXPECT_EQ(custom.compareTo(type(Id::TYPE_CORIOLIS_ACCELERATION)), 999 - 99);
    EXPECT_EQ(type(Id::TYPE_COMPUTATION_TIME).compareTo(custom), 100 - 200);
}

/// compareTo() of @p type with every built-in type is antisymmetric.
void expectAntisymmetric(const FlightDataType& type)
{
    EXPECT_EQ(type.compareTo(type), 0);
    for (const FlightDataType* other : FlightDataType::builtinTypes())
    {
        EXPECT_EQ(type.compareTo(*other), -other->compareTo(type));
    }
}

TEST(FlightDataType, CompareToIsAntisymmetric)
{
    for (const FlightDataType* t : FlightDataType::builtinTypes())
    {
        expectAntisymmetric(*t);
    }
}

}  // namespace
