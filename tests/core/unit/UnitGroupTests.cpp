#include "QtRocket/unit/UnitGroup.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/CaliberUnit.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/FractionalUnit.h"
#include "QtRocket/unit/FrequencyUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/InchUnit.h"
#include "QtRocket/unit/PercentageOfLengthUnit.h"
#include "QtRocket/unit/TemperatureUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::BugError;
using QtRocket::CaliberUnit;
using QtRocket::DegreeUnit;
using QtRocket::ErrorCode;
using QtRocket::FixedPrecisionUnit;
using QtRocket::FixedUnitGroup;
using QtRocket::FractionalUnit;
using QtRocket::FrequencyUnit;
using QtRocket::GeneralUnit;
using QtRocket::InchUnit;
using QtRocket::kAllUnitGroupIds;
using QtRocket::kUnitGroupCount;
using QtRocket::PercentageOfLengthUnit;
using QtRocket::TemperatureUnit;
using QtRocket::Unit;
using QtRocket::unitGroup;
using QtRocket::UnitGroup;
using QtRocket::unitGroupFromName;
using QtRocket::unitGroupFromSiUnit;
using QtRocket::UnitGroupId;
using QtRocket::unitGroupName;
using QtRocket::Value;
using QtRocket::Chars::kMicro;
using QtRocket::Chars::kPermille;
using QtRocket::Chars::kZwsp;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kPi = std::numbers::pi;

/// Every test starts and ends with UnitGroup.java's initial defaults, so the order of the tests
/// (and of the other test files that read the defaults) does not matter.
class UnitGroupTest : public ::testing::Test
{
protected:
    void SetUp() override { UnitGroup::resetDefaultUnits(); }
    void TearDown() override { UnitGroup::resetDefaultUnits(); }
};

/// A unit of UnitGroup.java: its name and multiplier. A constructor, not an aggregate, so that
/// the table below stays one pair per unit.
struct ExpectedUnit
{
    ExpectedUnit(std::string unitName, double unitMultiplier)
      : name(std::move(unitName)), multiplier(unitMultiplier)
    {
    }

    std::string name;
    double      multiplier;
};

struct ExpectedGroup
{
    ExpectedGroup(UnitGroupId groupId, std::string_view groupJavaName, std::string groupDefaultUnit,
                  std::vector<ExpectedUnit> groupUnits)
      : id(groupId),
        javaName(groupJavaName),
        defaultUnit(std::move(groupDefaultUnit)),
        units(std::move(groupUnits))
    {
    }

    UnitGroupId               id;
    std::string_view          javaName;  ///< the UNITS map key, empty when not in the map
    std::string               defaultUnit;
    std::vector<ExpectedUnit> units;
};

/// The static initialiser and resetDefaultUnits() of UnitGroup.java, group for group.
std::vector<ExpectedGroup> expectedGroups()
{
    const std::string zwsp(kZwsp);
    const std::string micro(kMicro);
    const std::string permille(kPermille);
    return {
        {UnitGroupId::NONE, "NONE", zwsp, {{zwsp, 1}}},
        {UnitGroupId::MOTOR_DIMENSIONS,
         "MOTOR_DIMENSIONS",
         "mm",
         {{"mm", 0.001}, {"cm", 0.01}, {"m", 1}, {"in", 0.0254}}},
        {UnitGroupId::LENGTH,
         "LENGTH",
         "cm",
         {{"mm", 0.001},
          {"cm", 0.01},
          {"m", 1},
          {"in", 0.0254},
          {"in/64", 0.0254},
          {"ft", 0.3048}}},
        {UnitGroupId::ALL_LENGTHS,
         "ALL_LENGTHS",
         "m",
         {{"mm", 0.001},
          {"cm", 0.01},
          {"m", 1},
          {"km", 1000},
          {"in", 0.0254},
          {"in/64", 0.0254},
          {"ft", 0.3048},
          {"yd", 0.9144},
          {"mi", 1609.344},
          {"nmi", 1852}}},
        {UnitGroupId::DISTANCE,
         "DISTANCE",
         "m",
         {{"m", 1}, {"km", 1000}, {"ft", 0.3048}, {"yd", 0.9144}, {"mi", 1609.344}, {"nmi", 1852}}},
        {UnitGroupId::SHAPE_PARAMETER, "", zwsp, {{zwsp, 1}}},
        {UnitGroupId::AREA,
         "AREA",
         "cm²",
         {{"mm²", MathUtil::pow2(0.001)},
          {"cm²", MathUtil::pow2(0.01)},
          {"m²", 1},
          {"in²", MathUtil::pow2(0.0254)},
          {"ft²", MathUtil::pow2(0.3048)}}},
        {UnitGroupId::STABILITY,
         "STABILITY",
         "cal",
         {{"mm", 0.001}, {"cm", 0.01}, {"m", 1}, {"in", 0.0254}, {"cal", 1}, {"%", 0.01}}},
        {UnitGroupId::SECONDARY_STABILITY,
         "SECONDARY_STABILITY",
         "%",
         {{"mm", 0.001}, {"cm", 0.01}, {"m", 1}, {"in", 0.0254}, {"cal", 1}, {"%", 0.01}}},
        {UnitGroupId::STABILITY_CALIBERS, "", "cal", {{"cal", 1}}},
        {UnitGroupId::VELOCITY,
         "VELOCITY",
         "m/s",
         {{"m/s", 1}, {"km/h", 1 / 3.6}, {"ft/s", 0.3048}, {"mph", 0.44704}, {"kt", 0.51444445}}},
        {UnitGroupId::WINDSPEED,
         "WINDSPEED",
         "m/s",
         {{"m/s", 1}, {"km/h", 1 / 3.6}, {"ft/s", 0.3048}, {"mph", 0.44704}, {"kt", 0.51444445}}},
        {UnitGroupId::LATITUDE, "LATITUDE", "° N", {{"° N", 1}, {"° S", -1}}},
        {UnitGroupId::LONGITUDE, "LONGITUDE", "° E", {{"° E", 1}, {"° W", -1}}},
        {UnitGroupId::ACCELERATION,
         "ACCELERATION",
         "m/s²",
         {{"m/s²", 1}, {"ft/s²", 0.3048}, {"G", 9.80665}}},
        {UnitGroupId::MASS,
         "MASS",
         "g",
         {{"g", 0.001}, {"kg", 1}, {"oz", 0.0283495231}, {"lb", 0.45359237}}},
        {UnitGroupId::INERTIA,
         "INERTIA",
         "kg·m²",
         {{"kg·cm²", 0.0001},
          {"kg·m²", 1},
          {"oz·in²", 1.82899783e-5},
          {"lb·in²", 0.000292639653},
          {"lb·ft²", 0.0421401101},
          {"lbf·ft·s²", 1.35581795}}},
        {UnitGroupId::ANGLE,
         "ANGLE",
         "°",
         {{"°", kPi / 180.0}, {"rad", 1}, {"arcmin", 1.0 / 3437.74677078}}},
        {UnitGroupId::DENSITY_BULK,
         "DENSITY_BULK",
         "g/cm³",
         {{"g/cm³", 1000},
          {"kg/cm³", 1000999},
          {"kg/dm³", 1000},
          {"kg/m³", 1},
          {"oz/in³", 1729.99404},
          {"lb/ft³", 16.0184634}}},
        {UnitGroupId::DENSITY_SURFACE,
         "DENSITY_SURFACE",
         "g/m²",
         {{"g/cm²", 10},
          {"g/m²", 0.001},
          {"kg/cm²", 10000},
          {"kg/dm²", 100},
          {"kg/m²", 1},
          {"oz/in²", 43.9418487},
          {"oz/ft²", 0.305151727},
          {"lb/ft²", 4.88242764}}},
        {UnitGroupId::DENSITY_LINE,
         "DENSITY_LINE",
         "g/m",
         {{"g/cm", 0.1},
          {"g/m", 0.001},
          {"kg/cm", 100},
          {"kg/dm", 10},
          {"kg/m", 1},
          {"oz/ft", 0.0930102465}}},
        {UnitGroupId::FORCE, "FORCE", "N", {{"N", 1}, {"lbf", 4.44822162}, {"kgf", 9.80665}}},
        {UnitGroupId::IMPULSE, "IMPULSE", "Ns", {{"Ns", 1}, {"lbf·s", 4.44822162}}},
        {UnitGroupId::TIME_STEP, "TIME_STEP", "s", {{"ms", 0.001}, {"s", 1}}},
        {UnitGroupId::SHORT_TIME, "SHORT_TIME", "s", {{"s", 1}}},
        {UnitGroupId::LONG_TIME, "FLIGHT_TIME", "s", {{"s", 1}, {"min", 60}}},
        {UnitGroupId::ROLL,
         "ROLL",
         "°/s",
         {{"rad/s", 1},
          {"°/s", kPi / 180},
          {"r/s", 2 * kPi},
          {"Hz", 2 * kPi},
          {"rpm", 2 * kPi / 60}}},
        {UnitGroupId::TEMPERATURE, "TEMPERATURE", "°C", {{"K", 1}, {"°C", 1}, {"°F", 5.0 / 9.0}}},
        {UnitGroupId::PRESSURE,
         "PRESSURE",
         "mbar",
         {{"mbar", 1.0e2},
          {"bar", 1.0e5},
          {"atm", 1.01325e5},
          {"mmHg", 101325.0 / 760.0},
          {"inHg", 3386.389},
          {"psi", 6894.75729},
          {"Pa", 1}}},
        {UnitGroupId::SHEAR_MODULUS,
         "SHEAR_MODULUS",
         "GPa",
         {{"Pa", 1}, {"GPa", 1.0e9}, {"ksi", 6.89475729e6}}},
        {UnitGroupId::RELATIVE, "RELATIVE", "%", {{zwsp, 1.0}, {"%", 0.01}, {permille, 0.001}}},
        {UnitGroupId::ROUGHNESS,
         "ROUGHNESS",
         micro + "m",
         {{micro + "m", 0.000001}, {"mil", 0.0000254}, {"in", 0.0254}, {"m", 1}}},
        {UnitGroupId::COEFFICIENT, "COEFFICIENT", zwsp, {{zwsp, 1}}},
        {UnitGroupId::FREQUENCY, "FREQUENCY", "Hz", {{"mHz", 0.001}, {"Hz", 1}, {"kHz", 1000}}},
        {UnitGroupId::ENERGY,
         "ENERGY",
         "J",
         {{"J", 1}, {"erg", 1.0e-7}, {"BTU", 1.055}, {"cal", 4.184}, {"ft·lbf", 1.3558179483314}}},
        {UnitGroupId::POWER,
         "POWER",
         "W",
         {{"mW", 1.0e-3}, {"W", 1}, {"kW", 1.0e3}, {"ergs", 1.0e-7}, {"hp", 745.699872}}},
        {UnitGroupId::MOMENT,
         "MOMENT",
         "N·m",
         {{"N·cm", 0.01}, {"N·m", 1}, {"lbf·in", 0.112984829}, {"lbf·ft", 1.35581795}}},
        {UnitGroupId::MOMENTUM, "MOMENTUM", "kg·m/s", {{"kg·m/s", 1}}},
        {UnitGroupId::ANGULAR_MOMENTUM,
         "ANGULAR_MOMENTUM",
         "kg·m²/s",
         {{"kg·cm²/s", 0.0001},
          {"kg·m²/s", 1},
          {"N·m·s", 1},
          {"oz·in²/s", 1.82899783e-5},
          {"lb·in²/s", 0.000292639653},
          {"lb·ft²/s", 0.0421401101},
          {"lbf·ft·s", 1.35581795}}},
        {UnitGroupId::VOLTAGE, "VOLTAGE", "V", {{"mV", 1.0e-3}, {"V", 1}}},
        {UnitGroupId::CURRENT, "CURRENT", "A", {{"mA", 1.0e-3}, {"A", 1}}},
        {UnitGroupId::SCALING, "SCALING", zwsp, {{zwsp, 1}}},
        {UnitGroupId::STROKE_WIDTH,
         "STROKE_WIDTH",
         "mm",
         {{"mm", 1}, {micro + "m", 0.1}, {"mil", 0.0254}}},
    };
}

TEST_F(UnitGroupTest, ThereAreFortyThreeDistinctGroups)
{
    EXPECT_EQ(kUnitGroupCount, 43U);
    EXPECT_EQ(kAllUnitGroupIds.size(), 43U);
    EXPECT_EQ(expectedGroups().size(), 43U);

    std::set<const UnitGroup*> instances;
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        instances.insert(&unitGroup(id));
        EXPECT_EQ(&unitGroup(id), &unitGroup(id));  // the same process-wide instance every time
    }
    EXPECT_EQ(instances.size(), 43U);
}

void expectUnitMatches(const UnitGroup& group, const ExpectedUnit& expected, std::size_t i,
                       const std::string& where)
{
    const Unit& unit = group.getUnit(static_cast<int>(i));
    EXPECT_EQ(unit.getUnit(), expected.name) << where << " unit " << i;
    EXPECT_EQ(unit.getMultiplier(), expected.multiplier) << where << " unit " << i;
    EXPECT_EQ(group.getUnits()[i], &unit) << where;
    EXPECT_EQ(group.getUnitIndex(unit), static_cast<int>(i)) << where;
}

void expectGroupMatches(const ExpectedGroup& expected)
{
    const UnitGroup& group = unitGroup(expected.id);
    const auto       where = std::string(unitGroupName(expected.id));
    ASSERT_EQ(group.getUnitCount(), static_cast<int>(expected.units.size())) << where;
    ASSERT_EQ(group.getUnits().size(), expected.units.size()) << where;
    for (std::size_t i = 0; i < expected.units.size(); i++)
    {
        expectUnitMatches(group, expected.units[i], i, where);
    }
    EXPECT_EQ(group.getDefaultUnit().getUnit(), expected.defaultUnit) << where;
}

/// Every group has UnitGroup.java's initial default unit.
void expectInitialDefaults()
{
    for (const ExpectedGroup& expected : expectedGroups())
    {
        const UnitGroup& group = unitGroup(expected.id);
        EXPECT_EQ(group.getDefaultUnit().getUnit(), expected.defaultUnit)
            << unitGroupName(expected.id);
        EXPECT_EQ(group.getDefaultUnitIndex(), group.getUnitIndex(group.getDefaultUnit()));
    }
}

/// Units @p first to @p last (excluded) of @p group are of class T.
template <class T>
void expectUnitsOfClass(const UnitGroup& group, int first, int last)
{
    for (int i = first; i < last; i++)
    {
        EXPECT_NE(dynamic_cast<const T*>(&group.getUnit(i)), nullptr) << i;
    }
}

TEST_F(UnitGroupTest, UnitsMultipliersOrderAndDefaultsMatchUnitGroupJava)
{
    for (const ExpectedGroup& expected : expectedGroups())
    {
        expectGroupMatches(expected);
    }
}

TEST_F(UnitGroupTest, UnitsHaveTheirJavaClasses)
{
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    EXPECT_NE(dynamic_cast<const GeneralUnit*>(&length.getUnit(0)), nullptr);
    EXPECT_NE(dynamic_cast<const InchUnit*>(&length.getUnit(3)), nullptr);
    EXPECT_EQ(dynamic_cast<const InchUnit&>(length.getUnit(3)).getPrecision(), 1.0);
    const auto* in64 = dynamic_cast<const FractionalUnit*>(&length.getUnit(4));
    ASSERT_NE(in64, nullptr);
    EXPECT_EQ(in64->getUnitLabel(), "in");
    EXPECT_EQ(in64->getFractionBase(), 64);
    EXPECT_EQ(in64->getIncrementValue(), 1.0 / 16.0);
    EXPECT_EQ(in64->getEpsilon(), 0.5 / 64.0);
    // MOTOR_DIMENSIONS has a plain "in", ALL_LENGTHS too.
    EXPECT_EQ(dynamic_cast<const InchUnit*>(&unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getUnit(3)),
              nullptr);
    EXPECT_EQ(dynamic_cast<const InchUnit*>(&unitGroup(UnitGroupId::ALL_LENGTHS).getUnit(4)),
              nullptr);

    const UnitGroup& angle = unitGroup(UnitGroupId::ANGLE);
    EXPECT_NE(dynamic_cast<const DegreeUnit*>(&angle.getUnit(0)), nullptr);
    const auto* rad = dynamic_cast<const FixedPrecisionUnit*>(&angle.getUnit(1));
    ASSERT_NE(rad, nullptr);
    EXPECT_EQ(rad->getPrecision(), 0.01);
    EXPECT_TRUE(rad->displaysTrailingZeros());

    const UnitGroup& temperature = unitGroup(UnitGroupId::TEMPERATURE);
    EXPECT_NE(dynamic_cast<const FixedPrecisionUnit*>(&temperature.getUnit(0)), nullptr);
    EXPECT_EQ(dynamic_cast<const TemperatureUnit*>(&temperature.getUnit(0)), nullptr);
    const auto* fahrenheit = dynamic_cast<const TemperatureUnit*>(&temperature.getUnit(2));
    ASSERT_NE(fahrenheit, nullptr);
    EXPECT_EQ(fahrenheit->getAddition(), 459.67);
    EXPECT_EQ(fahrenheit->getPrecision(), 0.01);

    const UnitGroup& latitude = unitGroup(UnitGroupId::LATITUDE);
    const auto*      south    = dynamic_cast<const FixedPrecisionUnit*>(&latitude.getUnit(1));
    ASSERT_NE(south, nullptr);
    EXPECT_EQ(south->getPrecision(), 10E-6);
    EXPECT_FALSE(south->displaysTrailingZeros());
    EXPECT_EQ(south->toString(12.345678), "-12.34568");

    const UnitGroup& timeStep = unitGroup(UnitGroupId::TIME_STEP);
    const auto*      ms       = dynamic_cast<const FixedPrecisionUnit*>(&timeStep.getUnit(0));
    ASSERT_NE(ms, nullptr);
    EXPECT_EQ(ms->getPrecision(), 1.0);
    EXPECT_EQ(ms->getDecimals(), 0);

    expectUnitsOfClass<FrequencyUnit>(unitGroup(UnitGroupId::FREQUENCY), 0, 3);
    expectUnitsOfClass<FixedPrecisionUnit>(unitGroup(UnitGroupId::PRESSURE), 0, 6);
    EXPECT_NE(dynamic_cast<const GeneralUnit*>(&unitGroup(UnitGroupId::PRESSURE).getUnit(6)),
              nullptr);

    const auto* shape =
        dynamic_cast<const GeneralUnit*>(&unitGroup(UnitGroupId::SHAPE_PARAMETER).getUnit(0));
    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->getSignificantNumbers(), 1);
    EXPECT_EQ(shape->getDecimalRounding(), 10);
    EXPECT_EQ(shape->getStepValue(), 0.1);

    const auto* relative =
        dynamic_cast<const FixedPrecisionUnit*>(&unitGroup(UnitGroupId::RELATIVE).getUnit(2));
    ASSERT_NE(relative, nullptr);
    EXPECT_EQ(relative->getPrecision(), 1.0);
    EXPECT_EQ(relative->getMultiplier(), 0.001);
}

void expectCaliberPlaceholder(const Unit& unit)
{
    const auto* caliber = dynamic_cast<const CaliberUnit*>(&unit);
    ASSERT_NE(caliber, nullptr);
    EXPECT_FALSE(caliber->hasReference());
}

void expectPercentagePlaceholder(const Unit& unit)
{
    const auto* percent = dynamic_cast<const PercentageOfLengthUnit*>(&unit);
    ASSERT_NE(percent, nullptr);
    EXPECT_FALSE(percent->hasReference());
}

TEST_F(UnitGroupTest, StabilityPlaceholdersHaveNoReference)
{
    expectCaliberPlaceholder(unitGroup(UnitGroupId::STABILITY).getUnit(4));
    expectPercentagePlaceholder(unitGroup(UnitGroupId::STABILITY).getUnit(5));
    expectCaliberPlaceholder(unitGroup(UnitGroupId::SECONDARY_STABILITY).getUnit(4));
    expectPercentagePlaceholder(unitGroup(UnitGroupId::SECONDARY_STABILITY).getUnit(5));
    // Converting without a reference is a bug.
    const UnitGroup& stability = unitGroup(UnitGroupId::STABILITY);
    const UnitGroup& secondary = unitGroup(UnitGroupId::SECONDARY_STABILITY);
    EXPECT_THROW(static_cast<void>(stability.getUnit(4).toUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(stability.getUnit(5).fromUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(secondary.getUnit(4).toUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(secondary.getUnit(5).fromUnit(1.0)), BugError);
    // UNITS_STABILITY_CALIBERS holds a plain unit that never scales.
    const UnitGroup& calibers = unitGroup(UnitGroupId::STABILITY_CALIBERS);
    EXPECT_EQ(dynamic_cast<const CaliberUnit*>(&calibers.getUnit(0)), nullptr);
    EXPECT_EQ(calibers.getUnit(0).toUnit(2.5), 2.5);
    EXPECT_EQ(calibers.toStringUnit(2.5), "2.5 cal");
}

/// Every group's name maps back to it, and the names are distinct.
void expectNamesRoundTrip()
{
    std::set<std::string_view> names;
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        const std::string_view name = unitGroupName(id);
        EXPECT_FALSE(name.empty());
        EXPECT_EQ(unitGroupFromName(name), id) << name;
        names.insert(name);
    }
    EXPECT_EQ(names.size(), 43U);
}

/// The names of the groups in UnitGroup.UNITS are its keys.
void expectJavaMapKeys()
{
    for (const ExpectedGroup& expected : expectedGroups())
    {
        if (!expected.javaName.empty())
        {
            EXPECT_EQ(unitGroupName(expected.id), expected.javaName);
        }
    }
}

TEST_F(UnitGroupTest, NamesRoundTripAndUseTheJavaMapKeys)
{
    expectNamesRoundTrip();
    expectJavaMapKeys();
    EXPECT_EQ(unitGroupName(UnitGroupId::LONG_TIME), "FLIGHT_TIME");
    EXPECT_EQ(unitGroupFromName("FLIGHT_TIME"), UnitGroupId::LONG_TIME);
    EXPECT_EQ(unitGroupFromName("LENGTH"), UnitGroupId::LENGTH);
    EXPECT_EQ(unitGroupFromName("NONE"), UnitGroupId::NONE);
    EXPECT_EQ(unitGroupFromName("LONG_TIME"), std::nullopt);  // not a key of the Java map
    EXPECT_EQ(unitGroupFromName("length"), std::nullopt);     // case-sensitive
    EXPECT_EQ(unitGroupFromName(""), std::nullopt);
    EXPECT_EQ(unitGroupFromName(" LENGTH"), std::nullopt);
}

TEST_F(UnitGroupTest, SiUnitSymbolsMapAsSIUNITS)
{
    EXPECT_EQ(unitGroupFromSiUnit("m"), UnitGroupId::ALL_LENGTHS);
    EXPECT_EQ(unitGroupFromSiUnit("m^2"), UnitGroupId::AREA);
    EXPECT_EQ(unitGroupFromSiUnit("m/s"), UnitGroupId::VELOCITY);
    EXPECT_EQ(unitGroupFromSiUnit("m/s^2"), UnitGroupId::ACCELERATION);
    EXPECT_EQ(unitGroupFromSiUnit("kg"), UnitGroupId::MASS);
    EXPECT_EQ(unitGroupFromSiUnit("kg m^2"), UnitGroupId::INERTIA);
    EXPECT_EQ(unitGroupFromSiUnit("kg/m^3"), UnitGroupId::DENSITY_BULK);
    EXPECT_EQ(unitGroupFromSiUnit("N"), UnitGroupId::FORCE);
    EXPECT_EQ(unitGroupFromSiUnit("N m"), UnitGroupId::MOMENT);
    EXPECT_EQ(unitGroupFromSiUnit("Ns"), UnitGroupId::IMPULSE);
    EXPECT_EQ(unitGroupFromSiUnit("s"), UnitGroupId::LONG_TIME);
    EXPECT_EQ(unitGroupFromSiUnit("Pa"), UnitGroupId::PRESSURE);
    EXPECT_EQ(unitGroupFromSiUnit("V"), UnitGroupId::VOLTAGE);
    EXPECT_EQ(unitGroupFromSiUnit("A"), UnitGroupId::CURRENT);
    EXPECT_EQ(unitGroupFromSiUnit("J"), UnitGroupId::ENERGY);
    EXPECT_EQ(unitGroupFromSiUnit("W"), UnitGroupId::POWER);
    EXPECT_EQ(unitGroupFromSiUnit("kg m/s"), UnitGroupId::MOMENTUM);
    EXPECT_EQ(unitGroupFromSiUnit("kg m^2/s"), UnitGroupId::ANGULAR_MOMENTUM);
    EXPECT_EQ(unitGroupFromSiUnit("Hz"), UnitGroupId::FREQUENCY);
    EXPECT_EQ(unitGroupFromSiUnit("K"), UnitGroupId::TEMPERATURE);
    EXPECT_EQ(unitGroupFromSiUnit("mm"), std::nullopt);
    EXPECT_EQ(unitGroupFromSiUnit("cal"), std::nullopt);
    EXPECT_EQ(unitGroupFromSiUnit(""), std::nullopt);
}

TEST_F(UnitGroupTest, MetricDefaults)
{
    UnitGroup::setDefaultMetricUnits();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    EXPECT_EQ(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getDefaultUnit().getUnit(), "mm");
    EXPECT_EQ(unitGroup(UnitGroupId::DISTANCE).getDefaultUnit().getUnit(), "m");
    EXPECT_EQ(unitGroup(UnitGroupId::AREA).getDefaultUnit().getUnit(), "cm²");
    EXPECT_EQ(unitGroup(UnitGroupId::STABILITY).getDefaultUnit().getUnit(), "cal");
    EXPECT_EQ(unitGroup(UnitGroupId::SECONDARY_STABILITY).getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(unitGroup(UnitGroupId::VELOCITY).getDefaultUnit().getUnit(), "m/s");
    EXPECT_EQ(unitGroup(UnitGroupId::ACCELERATION).getDefaultUnit().getUnit(), "m/s²");
    EXPECT_EQ(unitGroup(UnitGroupId::MASS).getDefaultUnit().getUnit(), "g");
    EXPECT_EQ(unitGroup(UnitGroupId::INERTIA).getDefaultUnit().getUnit(), "kg·m²");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGULAR_MOMENTUM).getDefaultUnit().getUnit(), "kg·m²/s");
    EXPECT_EQ(unitGroup(UnitGroupId::MOMENT).getDefaultUnit().getUnit(), "N·m");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).getDefaultUnit().getUnit(), "°");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_BULK).getDefaultUnit().getUnit(), "g/cm³");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_SURFACE).getDefaultUnit().getUnit(), "g/m²");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_LINE).getDefaultUnit().getUnit(), "g/m");
    EXPECT_EQ(unitGroup(UnitGroupId::FORCE).getDefaultUnit().getUnit(), "N");
    EXPECT_EQ(unitGroup(UnitGroupId::IMPULSE).getDefaultUnit().getUnit(), "Ns");
    EXPECT_EQ(unitGroup(UnitGroupId::TIME_STEP).getDefaultUnit().getUnit(), "s");
    EXPECT_EQ(unitGroup(UnitGroupId::LONG_TIME).getDefaultUnit().getUnit(), "s");
    EXPECT_EQ(unitGroup(UnitGroupId::ROLL).getDefaultUnit().getUnit(), "r/s");
    EXPECT_EQ(unitGroup(UnitGroupId::TEMPERATURE).getDefaultUnit().getUnit(), "°C");
    EXPECT_EQ(unitGroup(UnitGroupId::WINDSPEED).getDefaultUnit().getUnit(), "m/s");
    EXPECT_EQ(unitGroup(UnitGroupId::LATITUDE).getDefaultUnit().getUnit(), "° N");
    EXPECT_EQ(unitGroup(UnitGroupId::LONGITUDE).getDefaultUnit().getUnit(), "° E");
    EXPECT_EQ(unitGroup(UnitGroupId::PRESSURE).getDefaultUnit().getUnit(), "mbar");
    EXPECT_EQ(unitGroup(UnitGroupId::SHEAR_MODULUS).getDefaultUnit().getUnit(), "GPa");
    EXPECT_EQ(unitGroup(UnitGroupId::RELATIVE).getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(unitGroup(UnitGroupId::ROUGHNESS).getDefaultUnit().getUnit(),
              std::string(kMicro) + "m");
    EXPECT_EQ(unitGroup(UnitGroupId::STROKE_WIDTH).getDefaultUnit().getUnit(), "mm");
    // Groups the method does not touch keep their defaults.
    EXPECT_EQ(unitGroup(UnitGroupId::POWER).getDefaultUnit().getUnit(), "W");
    EXPECT_EQ(unitGroup(UnitGroupId::FREQUENCY).getDefaultUnit().getUnit(), "Hz");
}

TEST_F(UnitGroupTest, ImperialDefaults)
{
    UnitGroup::setDefaultImperialUnits();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_NE(dynamic_cast<const InchUnit*>(&unitGroup(UnitGroupId::LENGTH).getDefaultUnit()),
              nullptr);
    EXPECT_EQ(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getDefaultUnit().getUnit(), "in");
    EXPECT_EQ(unitGroup(UnitGroupId::DISTANCE).getDefaultUnit().getUnit(), "ft");
    EXPECT_EQ(unitGroup(UnitGroupId::AREA).getDefaultUnit().getUnit(), "in²");
    EXPECT_EQ(unitGroup(UnitGroupId::STABILITY).getDefaultUnit().getUnit(), "cal");
    EXPECT_EQ(unitGroup(UnitGroupId::SECONDARY_STABILITY).getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(unitGroup(UnitGroupId::VELOCITY).getDefaultUnit().getUnit(), "ft/s");
    EXPECT_EQ(unitGroup(UnitGroupId::ACCELERATION).getDefaultUnit().getUnit(), "ft/s²");
    EXPECT_EQ(unitGroup(UnitGroupId::MASS).getDefaultUnit().getUnit(), "oz");
    EXPECT_EQ(unitGroup(UnitGroupId::INERTIA).getDefaultUnit().getUnit(), "lb·ft²");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGULAR_MOMENTUM).getDefaultUnit().getUnit(), "lb·ft²/s");
    EXPECT_EQ(unitGroup(UnitGroupId::MOMENT).getDefaultUnit().getUnit(), "lbf·in");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).getDefaultUnit().getUnit(), "°");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_BULK).getDefaultUnit().getUnit(), "oz/in³");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_SURFACE).getDefaultUnit().getUnit(), "oz/ft²");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_LINE).getDefaultUnit().getUnit(), "oz/ft");
    EXPECT_EQ(unitGroup(UnitGroupId::FORCE).getDefaultUnit().getUnit(), "N");
    EXPECT_EQ(unitGroup(UnitGroupId::IMPULSE).getDefaultUnit().getUnit(), "Ns");
    EXPECT_EQ(unitGroup(UnitGroupId::TIME_STEP).getDefaultUnit().getUnit(), "s");
    EXPECT_EQ(unitGroup(UnitGroupId::LONG_TIME).getDefaultUnit().getUnit(), "s");
    EXPECT_EQ(unitGroup(UnitGroupId::ROLL).getDefaultUnit().getUnit(), "r/s");
    EXPECT_EQ(unitGroup(UnitGroupId::TEMPERATURE).getDefaultUnit().getUnit(), "°F");
    EXPECT_EQ(unitGroup(UnitGroupId::WINDSPEED).getDefaultUnit().getUnit(), "mph");
    EXPECT_EQ(unitGroup(UnitGroupId::LATITUDE).getDefaultUnit().getUnit(), "° N");
    EXPECT_EQ(unitGroup(UnitGroupId::LONGITUDE).getDefaultUnit().getUnit(), "° E");
    EXPECT_EQ(unitGroup(UnitGroupId::PRESSURE).getDefaultUnit().getUnit(), "mbar");
    EXPECT_EQ(unitGroup(UnitGroupId::SHEAR_MODULUS).getDefaultUnit().getUnit(), "ksi");
    EXPECT_EQ(unitGroup(UnitGroupId::RELATIVE).getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(unitGroup(UnitGroupId::ROUGHNESS).getDefaultUnit().getUnit(), "mil");
    EXPECT_EQ(unitGroup(UnitGroupId::STROKE_WIDTH).getDefaultUnit().getUnit(), "mil");

    // resetDefaultUnits puts everything back.
    UnitGroup::resetDefaultUnits();
    expectInitialDefaults();
}

TEST_F(UnitGroupTest, SetDefaultUnitByIndexAndName)
{
    UnitGroup& mass = unitGroup(UnitGroupId::MASS);
    mass.setDefaultUnit(3);
    EXPECT_EQ(mass.getDefaultUnitIndex(), 3);
    EXPECT_EQ(mass.getDefaultUnit().getUnit(), "lb");
    EXPECT_TRUE(mass.setDefaultUnit("oz"));
    EXPECT_EQ(mass.getDefaultUnitIndex(), 2);
    EXPECT_FALSE(mass.setDefaultUnit("OZ"));  // exact name only, as UnitGroup.setDefaultUnit
    EXPECT_FALSE(mass.setDefaultUnit("stone"));
    EXPECT_EQ(mass.getDefaultUnitIndex(), 2);  // a failed lookup changes nothing
    EXPECT_THROW(mass.setDefaultUnit(4), BugError);
    EXPECT_THROW(mass.setDefaultUnit(-1), BugError);
    EXPECT_EQ(mass.getDefaultUnitIndex(), 2);
    try
    {
        mass.setDefaultUnit(4);
        FAIL();
    }
    catch (const BugError& e)
    {
        EXPECT_TRUE(std::string_view(e.what()).starts_with("BUG: index out of range: 4 ("));
    }
}

TEST_F(UnitGroupTest, LookupsByNameIndexAndApproximation)
{
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    ASSERT_NE(length.getUnit("in"), nullptr);
    EXPECT_EQ(length.getUnit("in"), &length.getUnit(3));
    EXPECT_EQ(length.getUnit("in/64"), &length.getUnit(4));
    EXPECT_EQ(length.getUnit("IN"), nullptr);  // exact
    EXPECT_EQ(length.getUnit("furlong"), nullptr);
    EXPECT_EQ(length.getUnit(""), nullptr);
    EXPECT_THROW(static_cast<void>(length.getUnit(6)), BugError);
    EXPECT_THROW(static_cast<void>(length.getUnit(-1)), BugError);

    // findApproximate keeps only ASCII letters, digits and underscores, ignoring case.
    EXPECT_EQ(length.findApproximate("in"), &length.getUnit(3));  // "in" before "in/64"
    EXPECT_EQ(length.findApproximate("IN"), &length.getUnit(3));
    EXPECT_EQ(length.findApproximate(" in "), &length.getUnit(3));
    EXPECT_EQ(length.findApproximate("in/64"), &length.getUnit(4));
    EXPECT_EQ(length.findApproximate("in64"), &length.getUnit(4));
    EXPECT_EQ(length.findApproximate("Ft"), &length.getUnit(5));
    EXPECT_EQ(length.findApproximate("furlong"), nullptr);
    const UnitGroup& temperature = unitGroup(UnitGroupId::TEMPERATURE);
    EXPECT_EQ(temperature.findApproximate("F"), &temperature.getUnit(2));
    EXPECT_EQ(temperature.findApproximate("°C"), &temperature.getUnit(1));
    EXPECT_EQ(temperature.findApproximate("c"), &temperature.getUnit(1));
    EXPECT_EQ(temperature.findApproximate("K"), &temperature.getUnit(0));
    const UnitGroup& angle = unitGroup(UnitGroupId::ANGLE);
    EXPECT_EQ(angle.findApproximate("°"), &angle.getUnit(0));  // both reduce to ""
    EXPECT_EQ(angle.findApproximate(""), &angle.getUnit(0));
    EXPECT_EQ(angle.findApproximate("rad"), &angle.getUnit(1));
    const UnitGroup& inertia = unitGroup(UnitGroupId::INERTIA);
    EXPECT_EQ(inertia.findApproximate("kg m"), &inertia.getUnit(1));  // "kg·m²" is "kgm"
    EXPECT_EQ(inertia.findApproximate("lbf ft s"), &inertia.getUnit(5));
    EXPECT_EQ(inertia.findApproximate("kg m2"), nullptr);
    const UnitGroup& latitude = unitGroup(UnitGroupId::LATITUDE);
    EXPECT_EQ(latitude.findApproximate("N"), &latitude.getUnit(0));
    EXPECT_EQ(latitude.findApproximate("s"), &latitude.getUnit(1));

    // contains and getUnitIndex use Unit::equals, so an equal unit from elsewhere counts.
    const GeneralUnit cm(0.01, "cm");
    EXPECT_TRUE(length.contains(cm));
    EXPECT_EQ(length.getUnitIndex(cm), 1);
    EXPECT_FALSE(length.contains(GeneralUnit(0.0254, "in")));  // LENGTH's "in" is an InchUnit
    EXPECT_TRUE(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).contains(GeneralUnit(0.0254, "in")));
    EXPECT_EQ(length.getUnitIndex(GeneralUnit(1, "furlong")), -1);
    EXPECT_FALSE(length.contains(GeneralUnit(1, "furlong")));
}

void expectEverySiUnitHasMultiplierOne()
{
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        EXPECT_EQ(unitGroup(id).getSIUnit().getMultiplier(), 1.0) << unitGroupName(id);
        EXPECT_TRUE(unitGroup(id).contains(unitGroup(id).getSIUnit())) << unitGroupName(id);
    }
}

TEST_F(UnitGroupTest, SiUnitIsTheFirstWithMultiplierOne)
{
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getSIUnit().getUnit(), "m");
    EXPECT_EQ(unitGroup(UnitGroupId::PRESSURE).getSIUnit().getUnit(), "Pa");
    EXPECT_EQ(unitGroup(UnitGroupId::TEMPERATURE).getSIUnit().getUnit(), "K");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).getSIUnit().getUnit(), "rad");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGULAR_MOMENTUM).getSIUnit().getUnit(), "kg·m²/s");
    EXPECT_EQ(unitGroup(UnitGroupId::LATITUDE).getSIUnit().getUnit(), "° N");
    EXPECT_EQ(unitGroup(UnitGroupId::STROKE_WIDTH).getSIUnit().getUnit(), "mm");
    EXPECT_EQ(unitGroup(UnitGroupId::RELATIVE).getSIUnit().getUnit(), kZwsp);
    EXPECT_EQ(unitGroup(UnitGroupId::FREQUENCY).getSIUnit().getUnit(), "Hz");
    EXPECT_EQ(unitGroup(UnitGroupId::NONE).getSIUnit().getUnit(), kZwsp);
    expectEverySiUnitHasMultiplierOne();

    // Without a unit of multiplier 1, UNITS_NONE's default unit stands in.
    UnitGroup custom;
    custom.addUnit(std::make_unique<GeneralUnit>(2, "double"));
    custom.addUnit(std::make_unique<GeneralUnit>(0.5, "half"));
    EXPECT_EQ(&custom.getSIUnit(), &unitGroup(UnitGroupId::NONE).getDefaultUnit());
    EXPECT_EQ(custom.toString(), "UnitGroup:" + std::string(kZwsp));
    EXPECT_EQ(custom.getUnitCount(), 2);
    EXPECT_EQ(custom.getDefaultUnitIndex(), 0);
    EXPECT_EQ(custom.getDefaultUnit().getUnit(), "double");
    EXPECT_THROW(static_cast<void>(UnitGroup().getDefaultUnit()), BugError);
}

TEST_F(UnitGroupTest, DelegatesToTheDefaultUnit)
{
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);  // default cm
    EXPECT_DOUBLE_EQ(length.fromUnit(25.0), 0.25);
    EXPECT_EQ(length.toString(0.25), "25");
    EXPECT_EQ(length.toStringUnit(0.25), "25 cm");
    EXPECT_EQ(length.toStringUnit(0.12345), "12.3 cm");
    const Value value = length.toValue(0.25);
    EXPECT_EQ(value.getValue(), 0.25);
    EXPECT_EQ(&value.getUnit(), &length.getDefaultUnit());
    EXPECT_EQ(value.toString(), "25 cm");
    EXPECT_EQ(Value(0.25, length).toString(), "25 cm");
    EXPECT_EQ(length.toString(), "UnitGroup:m");
    EXPECT_EQ(unitGroup(UnitGroupId::TEMPERATURE).toStringUnit(300.0), "26.85°C");
    EXPECT_EQ(unitGroup(UnitGroupId::TEMPERATURE).toString(), "UnitGroup:K");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).toStringUnit(kPi / 2), "90°");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_BULK).toStringUnit(2700), "2.7 g/cm³");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_SURFACE).toStringUnit(0.067), "67 g/m²");
    EXPECT_EQ(unitGroup(UnitGroupId::DENSITY_LINE).toStringUnit(0.0018), "1.8 g/m");
    EXPECT_EQ(unitGroup(UnitGroupId::MASS).toStringUnit(0.0283495231), "28.3 g");
    EXPECT_EQ(unitGroup(UnitGroupId::TIME_STEP).toStringUnit(0.05), "0.05 s");
    EXPECT_EQ(unitGroup(UnitGroupId::LATITUDE).toStringUnit(45.5), "45.5 ° N");
    EXPECT_EQ(unitGroup(UnitGroupId::ROLL).toStringUnit(kPi), "180 °/s");
}

void expectParseFailures(const UnitGroup& group, std::initializer_list<std::string_view> inputs)
{
    for (const std::string_view bad : inputs)
    {
        const auto result = group.fromString(bad);
        ASSERT_FALSE(result.has_value()) << bad;
        EXPECT_EQ(result.error().code, ErrorCode::PARSE) << bad;
    }
}

TEST_F(UnitGroupTest, FromStringParsesValueAndOptionalUnit)
{
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);  // default cm
    EXPECT_DOUBLE_EQ(length.fromString("12.5 mm").value(), 0.0125);
    EXPECT_DOUBLE_EQ(length.fromString("12.5mm").value(), 0.0125);
    EXPECT_DOUBLE_EQ(length.fromString("12,5 mm").value(), 0.0125);        // a comma is accepted
    EXPECT_DOUBLE_EQ(length.fromString("  12.5   MM  ").value(), 0.0125);  // any ASCII case
    EXPECT_DOUBLE_EQ(length.fromString("3").value(), 0.03);                // the default unit
    EXPECT_DOUBLE_EQ(length.fromString("-5 cm").value(), -0.05);
    EXPECT_DOUBLE_EQ(length.fromString("2 in").value(), 0.0508);
    EXPECT_DOUBLE_EQ(length.fromString("2 in/64").value(), 0.0508);
    EXPECT_DOUBLE_EQ(length.fromString("\t1.5\tft").value(), 0.4572);
    EXPECT_DOUBLE_EQ(unitGroup(UnitGroupId::TEMPERATURE).fromString("10 °C").value(), 283.15);
    EXPECT_DOUBLE_EQ(unitGroup(UnitGroupId::TEMPERATURE).fromString("50 °f").value(), 283.15);

    expectParseFailures(length, {"", "   ", "abc", "mm", "12 furlong", "1e3", "1.5 mm\n",
                                 "12.5 mm km", "- 5 cm", "--5"});
    EXPECT_EQ(length.fromString("abc").error().message, "string did not match required pattern");
    EXPECT_EQ(length.fromString("12 furlong").error().message, "unknown unit furlong");
}

TEST_F(UnitGroupTest, EqualityComparesTheUnitLists)
{
    const UnitGroup& velocity  = unitGroup(UnitGroupId::VELOCITY);
    const UnitGroup& windspeed = unitGroup(UnitGroupId::WINDSPEED);
    EXPECT_TRUE(velocity.equals(velocity));
    EXPECT_TRUE(velocity == windspeed);  // the same units, whatever the defaults
    EXPECT_EQ(velocity.hash(), windspeed.hash());
    unitGroup(UnitGroupId::WINDSPEED).setDefaultUnit(3);
    EXPECT_TRUE(velocity.equals(windspeed));
    EXPECT_FALSE(unitGroup(UnitGroupId::LENGTH) == unitGroup(UnitGroupId::MOTOR_DIMENSIONS));
    EXPECT_FALSE(unitGroup(UnitGroupId::LENGTH) == unitGroup(UnitGroupId::ALL_LENGTHS));
    EXPECT_TRUE(unitGroup(UnitGroupId::STABILITY) == unitGroup(UnitGroupId::SECONDARY_STABILITY));

    UnitGroup mine;
    mine.addUnit(std::make_unique<GeneralUnit>(1, "m/s"));
    mine.addUnit(std::make_unique<GeneralUnit>(1 / 3.6, "km/h"));
    mine.addUnit(std::make_unique<GeneralUnit>(0.3048, "ft/s"));
    mine.addUnit(std::make_unique<GeneralUnit>(0.44704, "mph"));
    EXPECT_FALSE(mine == velocity);                                    // one unit short
    mine.addUnit(std::make_unique<GeneralUnit>(0.51444445, "kt", 5));  // rounding does not count
    EXPECT_TRUE(mine == velocity);
    EXPECT_EQ(mine.hash(), velocity.hash());
}

TEST_F(UnitGroupTest, StabilityUnitGroupBindsAReferenceLength)
{
    UnitGroup& stability                                       = unitGroup(UnitGroupId::STABILITY);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> group = UnitGroup::stabilityUnits(0.05);
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->getUnitCount(), 6);
    EXPECT_EQ(group->getDefaultUnitIndex(), stability.getDefaultUnitIndex());
    EXPECT_EQ(group->getDefaultUnit().getUnit(), "cal");
    EXPECT_TRUE(group->equals(stability));
    EXPECT_EQ(group->toString(), "StabilityUnitGroup:m");
    EXPECT_EQ(group->getSIUnit().getUnit(), "m");

    const auto* caliber = dynamic_cast<const CaliberUnit*>(&group->getUnit(4));
    ASSERT_NE(caliber, nullptr);
    EXPECT_TRUE(caliber->hasReference());
    EXPECT_DOUBLE_EQ(caliber->getReferenceLength(), 0.05);
    EXPECT_DOUBLE_EQ(group->getUnit(4).toUnit(0.1), 2.0);
    EXPECT_EQ(group->toStringUnit(0.125), "2.5 cal");
    EXPECT_EQ(&group->getPercentageOfLengthUnit(), &group->getUnit(5));
    EXPECT_DOUBLE_EQ(group->getPercentageOfLengthUnit().toUnit(0.025), 50.0);
    EXPECT_EQ(group->getPercentageOfLengthUnit().toStringUnit(0.025), "50 %");
    // The other units are clones of UNITS_STABILITY's, not the same objects.
    EXPECT_NE(&group->getUnit(0), &stability.getUnit(0));
    EXPECT_TRUE(group->getUnit(0).equals(stability.getUnit(0)));
    EXPECT_EQ(group->getUnit(3).toString(0.0254), "1");
    // The placeholders of UNITS_STABILITY are untouched.
    EXPECT_FALSE(dynamic_cast<const CaliberUnit&>(stability.getUnit(4)).hasReference());

    // Setting the default sets it in UNITS_STABILITY too.
    group->setDefaultUnit(2);
    EXPECT_EQ(group->getDefaultUnitIndex(), 2);
    EXPECT_EQ(stability.getDefaultUnitIndex(), 2);
    EXPECT_TRUE(group->setDefaultUnit("%"));
    EXPECT_EQ(stability.getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(group->toStringUnit(0.025), "50 %");
    EXPECT_THROW(group->setDefaultUnit(6), BugError);

    EXPECT_THROW(static_cast<void>(UnitGroup::stabilityUnits(0.0)), BugError);
    EXPECT_THROW(static_cast<void>(UnitGroup::stabilityUnits(-1.0)), BugError);
    EXPECT_THROW(static_cast<void>(UnitGroup::secondaryStabilityUnits(0.0)), BugError);
}

TEST_F(UnitGroupTest, SecondaryStabilityUnitGroupAndProviders)
{
    UnitGroup& secondary = unitGroup(UnitGroupId::SECONDARY_STABILITY);
    EXPECT_EQ(secondary.getDefaultUnit().getUnit(), "%");
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> constant =
        UnitGroup::secondaryStabilityUnits(0.5);
    EXPECT_EQ(constant->getDefaultUnit().getUnit(), "%");
    EXPECT_EQ(constant->toStringUnit(0.1), "20 %");
    EXPECT_DOUBLE_EQ(constant->getUnit(4).toUnit(0.1), 0.2);

    // A provider-less group keeps the placeholders' behaviour.
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> empty =
        UnitGroup::stabilityUnits(std::function<double()>{});
    EXPECT_THROW(static_cast<void>(empty->getUnit(4).toUnit(1.0)), BugError);
    EXPECT_THROW(static_cast<void>(empty->getPercentageOfLengthUnit().toUnit(1.0)), BugError);

    // Built from a group without the placeholders, the percentage unit still exists.
    UnitGroup plain;
    plain.addUnit(std::make_unique<GeneralUnit>(1, "m"));
    const UnitGroup::StabilityUnitGroup fromPlain(plain, 2.0);
    EXPECT_EQ(fromPlain.getUnitCount(), 1);
    EXPECT_DOUBLE_EQ(fromPlain.getPercentageOfLengthUnit().toUnit(1.0), 50.0);
    EXPECT_FALSE(fromPlain.contains(fromPlain.getPercentageOfLengthUnit()));
}

TEST_F(UnitGroupTest, StabilityProvidersAreReadOnEveryConversion)
{
    double                        reference = 0.1;
    const std::function<double()> provider  = [&reference] { return reference; };
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> dynamic =
        UnitGroup::stabilityUnits(provider);
    EXPECT_EQ(dynamic->getDefaultUnit().getUnit(), "cal");
    EXPECT_DOUBLE_EQ(dynamic->fromUnit(2.0), 0.2);
    reference = 0.2;
    EXPECT_DOUBLE_EQ(dynamic->fromUnit(2.0), 0.4);
    EXPECT_DOUBLE_EQ(dynamic->getPercentageOfLengthUnit().toUnit(0.1), 50.0);
}

TEST_F(UnitGroupTest, SecondaryStabilityProvidersAreReadOnEveryConversion)
{
    double                        reference = 0.2;
    const std::function<double()> provider  = [&reference] { return reference; };
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> dynamicSecondary =
        UnitGroup::secondaryStabilityUnits(provider);
    EXPECT_EQ(dynamicSecondary->toStringUnit(0.1), "50 %");
    reference = 0.4;
    EXPECT_EQ(dynamicSecondary->toStringUnit(0.1), "25 %");
}

TEST_F(UnitGroupTest, FixedUnitGroupIsOneArbitraryUnit)
{
    const FixedUnitGroup furlongs("furlong");
    EXPECT_EQ(furlongs.getUnitString(), "furlong");
    EXPECT_EQ(furlongs.getUnitCount(), 1);
    EXPECT_EQ(furlongs.getDefaultUnit().getUnit(), "furlong");
    EXPECT_EQ(furlongs.getDefaultUnit().getMultiplier(), 1.0);
    EXPECT_EQ(&furlongs.getSIUnit(), &furlongs.getDefaultUnit());
    EXPECT_EQ(&furlongs.getUnit(0), &furlongs.getDefaultUnit());
    EXPECT_NE(dynamic_cast<const GeneralUnit*>(&furlongs.getDefaultUnit()), nullptr);
    EXPECT_TRUE(furlongs.contains(GeneralUnit(1, "furlong")));
    EXPECT_TRUE(furlongs.contains(GeneralUnit(3, "anything")));  // every unit
    EXPECT_TRUE(furlongs.contains(DegreeUnit()));
    EXPECT_EQ(furlongs.toString(), "FixedUnitGroup:furlong");
    EXPECT_EQ(furlongs.toString(2.5), "2.5");
    EXPECT_EQ(furlongs.toStringUnit(2.5), "2.5 furlong");
    EXPECT_DOUBLE_EQ(furlongs.fromString("7 furlong").value(), 7.0);
    EXPECT_FALSE(furlongs.equals(unitGroup(UnitGroupId::SHORT_TIME)));
    EXPECT_TRUE(furlongs.equals(FixedUnitGroup("furlong")));
    EXPECT_THROW(static_cast<void>(furlongs.getUnit(1)), BugError);
}

}  // namespace
