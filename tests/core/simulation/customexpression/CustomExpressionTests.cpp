#include "QtRocket/simulation/customexpression/CustomExpression.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"

// OpenRocket's tests of custom expressions (CustomExpressionSimulationListenerTest,
// RangeExpressionTest, TestExpressions) are about evaluating them, which is not ported. These
// tests pin the part that is carried: the four strings, equals(), toString() and getType(), with
// the values of the probe CustomExpressionProbe.java, sections A to D (probes/tier9a-document-d2).
//
// getType() makes its types in the process-wide registry of FlightDataType. Every test here
// uses symbols of its own (prefix "qtrCx"), and the two cases that would change what a symbol
// other tests look up finds run in the child process of a death test.

namespace
{

using QtRocket::CustomExpression;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeGroup;
using QtRocket::FlightDataTypeId;
using QtRocket::UnitGroupId;

// A value type: Java's clone() is the copy.
static_assert(std::is_default_constructible_v<CustomExpression>);
static_assert(std::is_copy_constructible_v<CustomExpression>);
static_assert(std::is_copy_assignable_v<CustomExpression>);
static_assert(std::is_nothrow_move_constructible_v<CustomExpression>);

/// What getType() of an expression named @p name with the symbol @p symbol is expected to be: a
/// CUSTOM type of that name and symbol, not built in, saved under its name.
void expectCustomType(const FlightDataType& type, std::string_view name, std::string_view symbol)
{
    EXPECT_EQ(type.getName(), name);
    EXPECT_EQ(type.getSymbol(), symbol);
    EXPECT_EQ(type.getGroup(), FlightDataTypeGroup::CUSTOM);
    EXPECT_FALSE(type.isBuiltin());
    EXPECT_EQ(type.getSaveKey(), name);
}

/// getType() of an expression with an SI unit: the unit's group.
void expectSiType(const std::string& name, const std::string& symbol, const std::string& unit,
                  UnitGroupId units)
{
    const CustomExpression expression(name, symbol, unit, "1");
    const FlightDataType&  type = expression.getType();
    expectCustomType(type, name, symbol);
    EXPECT_EQ(type.getUnitGroupId(), units) << unit;
    EXPECT_EQ(&type.getUnitGroup(), &QtRocket::unitGroup(units)) << unit;
    // Java: "again same=true".
    EXPECT_EQ(&expression.getType(), &type);
    EXPECT_EQ(FlightDataType::findBySymbol(symbol), &type);
}

/// getType() of an expression whose unit is no SI unit: a fixed unit group of that text.
void expectFixedUnitType(const std::string& name, const std::string& symbol,
                         const std::string& unit)
{
    const CustomExpression expression(name, symbol, unit, "1");
    const FlightDataType&  type = expression.getType();
    expectCustomType(type, name, symbol);
    EXPECT_EQ(type.getUnitGroupId(), std::nullopt) << unit;
    EXPECT_EQ(type.getUnitGroup().getDefaultUnit().getUnit(), unit);
    EXPECT_EQ(type.getUnitGroup().toString(), "FixedUnitGroup:" + unit);
    EXPECT_EQ(&expression.getType(), &type);
}

TEST(CustomExpression, ANewExpressionIsFourEmptyStrings)
{
    // Java: new CustomExpression(doc): name='' symbol='' unit='' expression=''
    // toString=[Expression name= expression= unit=]
    const CustomExpression expression;
    EXPECT_EQ(expression.getName(), "");
    EXPECT_EQ(expression.getSymbol(), "");
    EXPECT_EQ(expression.getUnit(), "");
    EXPECT_EQ(expression.getExpressionString(), "");
    EXPECT_EQ(expression.toString(), "[Expression name= expression= unit=]");
}

TEST(CustomExpression, TheFourStringsOfTheConstructor)
{
    // Java: name='Kinetic energy' symbol='qtrKE' unit='J' expression='0.5*m*Vt^2'
    // toString=[Expression name=Kinetic energy expression=0.5*m*Vt^2 unit=J]
    const CustomExpression expression("Kinetic energy", "qtrKE", "J", "0.5*m*Vt^2");
    EXPECT_EQ(expression.getName(), "Kinetic energy");
    EXPECT_EQ(expression.getSymbol(), "qtrKE");
    EXPECT_EQ(expression.getUnit(), "J");
    EXPECT_EQ(expression.getExpressionString(), "0.5*m*Vt^2");
    // The symbol is not part of the text.
    EXPECT_EQ(expression.toString(),
              "[Expression name=Kinetic energy expression=0.5*m*Vt^2 unit=J]");
}

TEST(CustomExpression, TheSettersStoreWhatTheyAreGiven)
{
    // Java: after the setters: n2 s2 u2 x2 toString=[Expression name=n2 expression=x2 unit=u2]
    CustomExpression expression("Kinetic energy", "qtrKE", "J", "0.5*m*Vt^2");
    expression.setName("n2");
    EXPECT_EQ(expression.getName(), "n2");
    EXPECT_EQ(expression.getSymbol(), "qtrKE");
    expression.setSymbol("s2");
    EXPECT_EQ(expression.getSymbol(), "s2");
    EXPECT_EQ(expression.getUnit(), "J");
    expression.setUnit("u2");
    EXPECT_EQ(expression.getUnit(), "u2");
    EXPECT_EQ(expression.getExpressionString(), "0.5*m*Vt^2");
    expression.setExpression("x2");
    EXPECT_EQ(expression.getExpressionString(), "x2");
    EXPECT_EQ(expression.getName(), "n2");
    EXPECT_EQ(expression.toString(), "[Expression name=n2 expression=x2 unit=u2]");
}

TEST(CustomExpression, TheTextIsKeptAsItIs)
{
    // Nothing is parsed, trimmed or checked: what a file holds is what a save writes back,
    // whether or not OpenRocket could evaluate it.
    CustomExpression expression(" spaced name ", "s y m", " m ", " h[1:2] + (unbalanced ");
    EXPECT_EQ(expression.getName(), " spaced name ");
    EXPECT_EQ(expression.getSymbol(), "s y m");
    EXPECT_EQ(expression.getUnit(), " m ");
    EXPECT_EQ(expression.getExpressionString(), " h[1:2] + (unbalanced ");
    expression.setExpression("");
    EXPECT_EQ(expression.getExpressionString(), "");
    // A symbol that is no regular expression is just a symbol (in Java it breaks the making of
    // every later expression of the document).
    expression.setSymbol("(");
    EXPECT_EQ(expression.getSymbol(), "(");
    EXPECT_EQ(expression.toString(), "[Expression name= spaced name  expression= unit= m ]");
}

TEST(CustomExpression, EqualsComparesTheFourStringsExactly)
{
    // Java: [N S U E] equals [N S U E] = true, and false when the name, the symbol, the unit or
    // the expression differs, in case or by a space.
    const CustomExpression expression("N", "S", "U", "E");
    EXPECT_TRUE(expression == CustomExpression("N", "S", "U", "E"));
    EXPECT_FALSE(expression == CustomExpression("n", "S", "U", "E"));
    EXPECT_FALSE(expression == CustomExpression("N", "s", "U", "E"));
    EXPECT_FALSE(expression == CustomExpression("N", "S", "u", "E"));
    EXPECT_FALSE(expression == CustomExpression("N", "S", "U", "e"));
    EXPECT_FALSE(expression == CustomExpression("N", "S", "U", "E "));
    EXPECT_TRUE(expression != CustomExpression("N", "S", "U", "E "));
    EXPECT_TRUE(CustomExpression() == CustomExpression("", "", "", ""));
    EXPECT_FALSE(CustomExpression() == expression);
}

TEST(CustomExpression, ACopyIsJavasClone)
{
    // Java: clone equals=true same=false
    const CustomExpression expression("N", "S", "U", "E");
    CustomExpression       clone = expression;
    EXPECT_TRUE(clone == expression);
    clone.setExpression("E2");
    EXPECT_FALSE(clone == expression);
    EXPECT_EQ(expression.getExpressionString(), "E");

    CustomExpression assigned;
    assigned = expression;
    EXPECT_TRUE(assigned == expression);
    CustomExpression       source("N", "S", "U", "E");
    const CustomExpression moved = std::move(source);
    EXPECT_TRUE(moved == expression);
}

TEST(CustomExpression, TheTypeOfAnSiUnitHasTheUnitsGroup)
{
    // Java: [QtRocket probe energy | qtrPE | J] -> units=ENERGY group=Custom priority=200
    // saveKey='QtRocket probe energy', and so on for m, m/s^2 and kg m^2.
    expectSiType("QtRocket test energy", "qtrCxEnergy", "J", UnitGroupId::ENERGY);
    const FlightDataType* const energy = FlightDataType::findBySymbol("qtrCxEnergy");
    ASSERT_NE(energy, nullptr);
    EXPECT_EQ(energy->getGroupPriority(), 200);
    EXPECT_EQ(energy->getPriority(), FlightDataType::kDefaultPriority);
    EXPECT_EQ(energy->toString(), "QtRocket test energy");
    expectSiType("QtRocket test length", "qtrCxLength", "m", UnitGroupId::ALL_LENGTHS);
    expectSiType("QtRocket test accel", "qtrCxAccel", "m/s^2", UnitGroupId::ACCELERATION);
    expectSiType("QtRocket test inertia", "qtrCxInertia", "kg m^2", UnitGroupId::INERTIA);
}

TEST(CustomExpression, EverySiUnitOfOpenRocket)
{
    // Java: UnitGroup.SIUNITS, 20 keys ('s' is UNITS_FLIGHT_TIME, the group LONG_TIME here).
    expectSiType("QtRocket test A", "qtrCxSiA", "A", UnitGroupId::CURRENT);
    expectSiType("QtRocket test Hz", "qtrCxSiHz", "Hz", UnitGroupId::FREQUENCY);
    expectSiType("QtRocket test K", "qtrCxSiK", "K", UnitGroupId::TEMPERATURE);
    expectSiType("QtRocket test N", "qtrCxSiN", "N", UnitGroupId::FORCE);
    expectSiType("QtRocket test N m", "qtrCxSiNm", "N m", UnitGroupId::MOMENT);
    expectSiType("QtRocket test Ns", "qtrCxSiNs", "Ns", UnitGroupId::IMPULSE);
    expectSiType("QtRocket test Pa", "qtrCxSiPa", "Pa", UnitGroupId::PRESSURE);
    expectSiType("QtRocket test V", "qtrCxSiV", "V", UnitGroupId::VOLTAGE);
    expectSiType("QtRocket test W", "qtrCxSiW", "W", UnitGroupId::POWER);
    expectSiType("QtRocket test kg", "qtrCxSiKg", "kg", UnitGroupId::MASS);
    expectSiType("QtRocket test kg m/s", "qtrCxSiKgMS", "kg m/s", UnitGroupId::MOMENTUM);
    expectSiType("QtRocket test kg m^2/s", "qtrCxSiKgM2S", "kg m^2/s",
                 UnitGroupId::ANGULAR_MOMENTUM);
    expectSiType("QtRocket test kg/m^3", "qtrCxSiKgM3", "kg/m^3", UnitGroupId::DENSITY_BULK);
    expectSiType("QtRocket test m/s", "qtrCxSiMS", "m/s", UnitGroupId::VELOCITY);
    expectSiType("QtRocket test m^2", "qtrCxSiM2", "m^2", UnitGroupId::AREA);
    expectSiType("QtRocket test s", "qtrCxSiS", "s", UnitGroupId::LONG_TIME);
}

TEST(CustomExpression, TheTypeOfAnyOtherUnitHasAFixedUnit)
{
    // Java: [QtRocket probe furlongs | qtrPF | furlong] -> units=FixedUnitGroup:furlong.
    expectFixedUnitType("QtRocket test furlongs", "qtrCxFurlong", "furlong");
    // Java: no unit at all is a fixed unit "" (FixedUnitGroup:).
    expectFixedUnitType("QtRocket test no unit", "qtrCxNoUnit", "");
    // The SI units are looked up exactly: "M" and " m" are not "m".
    expectFixedUnitType("QtRocket test metres upper", "qtrCxUpper", "M");
    expectFixedUnitType("QtRocket test spaced", "qtrCxSpaced", " m");
}

TEST(CustomExpression, AnExpressionWithoutANameHasATypeWithoutAName)
{
    // Java: [ | qtrPEmptyName | m] -> name='' symbol='qtrPEmptyName' units=ALL_LENGTHS
    // saveKey=''.
    const CustomExpression expression("", "qtrCxNoName", "m", "1");
    const FlightDataType&  type = expression.getType();
    expectCustomType(type, "", "qtrCxNoName");
    EXPECT_EQ(type.getUnitGroupId(), UnitGroupId::ALL_LENGTHS);
}

TEST(CustomExpression, GetTypeChangesNothingOfTheExpression)
{
    const CustomExpression expression("QtRocket test const", "qtrCxConst", "m/s", "Vz * 2");
    const CustomExpression same("QtRocket test const", "qtrCxConst", "m/s", "Vz * 2");
    const FlightDataType&  type = expression.getType();
    EXPECT_TRUE(expression == same);
    EXPECT_EQ(expression.toString(),
              "[Expression name=QtRocket test const expression=Vz * 2 "
              "unit=m/s]");
    // Two equal expressions have one type.
    EXPECT_EQ(&same.getType(), &type);
    // The expression text plays no part in the type.
    EXPECT_EQ(&CustomExpression("QtRocket test const", "qtrCxConst", "m/s", "other").getType(),
              &type);
}

TEST(CustomExpression, AChangedExpressionGetsANewTypeForItsSymbol)
{
    // Java, section D: "unit m -> s: same=false ... units=FLIGHT_TIME", "then renamed:
    // same=false name='QtRocket probe changed'": FlightDataType.getType() replaces the type of
    // the symbol when the unit group or the name differs.
    CustomExpression      expression("QtRocket test changing", "qtrCxChanging", "m", "1");
    const FlightDataType& inMetres = expression.getType();
    EXPECT_EQ(inMetres.getUnitGroupId(), UnitGroupId::ALL_LENGTHS);

    expression.setUnit("s");
    const FlightDataType& inSeconds = expression.getType();
    EXPECT_NE(&inSeconds, &inMetres);
    expectCustomType(inSeconds, "QtRocket test changing", "qtrCxChanging");
    EXPECT_EQ(inSeconds.getUnitGroupId(), UnitGroupId::LONG_TIME);
    EXPECT_EQ(FlightDataType::findBySymbol("qtrCxChanging"), &inSeconds);

    expression.setName("QtRocket test changed");
    const FlightDataType& renamed = expression.getType();
    EXPECT_NE(&renamed, &inSeconds);
    expectCustomType(renamed, "QtRocket test changed", "qtrCxChanging");
    EXPECT_EQ(renamed.getUnitGroupId(), UnitGroupId::LONG_TIME);
    // The types handed out before stay what they were.
    EXPECT_EQ(inMetres.getName(), "QtRocket test changing");
    EXPECT_EQ(inMetres.getUnitGroupId(), UnitGroupId::ALL_LENGTHS);

    // The symbol and the expression text are no reason for a new type of that symbol.
    expression.setExpression("2");
    EXPECT_EQ(&expression.getType(), &renamed);
}

TEST(CustomExpression, AnotherFixedUnitIsNoChangeOfTheType)
{
    // Java, section D: "unit furlong: ... units=FixedUnitGroup:furlong", "unit fortnight: same
    // as furlong's=true ... units=FixedUnitGroup:furlong": all fixed unit groups are equal, so
    // the type of the symbol stays, with the unit it was made with.
    CustomExpression      expression("QtRocket test fixed", "qtrCxFixed", "furlong", "1");
    const FlightDataType& furlongs = expression.getType();
    EXPECT_EQ(furlongs.getUnitGroup().getDefaultUnit().getUnit(), "furlong");

    expression.setUnit("fortnight");
    const FlightDataType& fortnights = expression.getType();
    EXPECT_EQ(&fortnights, &furlongs);
    EXPECT_EQ(fortnights.getUnitGroup().getDefaultUnit().getUnit(), "furlong");

    // From a fixed unit to an SI unit is a change.
    expression.setUnit("m");
    const FlightDataType& metres = expression.getType();
    EXPECT_NE(&metres, &furlongs);
    EXPECT_EQ(metres.getUnitGroupId(), UnitGroupId::ALL_LENGTHS);
}

// The two cases of getType() that change what other tests find in the registry of
// FlightDataType. They run in the child process of a death test.

/// Java, section D: "[Altitude | h | m] is the built-in: false name='Altitude' symbol='h'
/// units=ALL_LENGTHS group=Custom": the built-in altitude is a DISTANCE, the SI unit "m" is
/// ALL_LENGTHS, so the expression gets a type of its own, which then is what "h" finds.
void checkAnExpressionNamedAsABuiltinType()
{
    const FlightDataType&  altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const CustomExpression expression("Altitude", "h", "m", "1");
    const FlightDataType&  type = expression.getType();
    expectCustomType(type, "Altitude", "h");
    EXPECT_NE(&type, &altitude);
    EXPECT_EQ(type.getUnitGroupId(), UnitGroupId::ALL_LENGTHS);
    EXPECT_EQ(altitude.getUnitGroupId(), UnitGroupId::DISTANCE);
    EXPECT_EQ(FlightDataType::findBySymbol("h"), &type);
}

/// Java: an expression as new CustomExpression(doc) leaves it has a type too, with neither name
/// nor symbol, in a fixed unit "".
void checkTheEmptyExpression()
{
    // (A named expression: GCC's -Wdangling-reference takes the reference a member function of
    // a temporary returns for one into the temporary.)
    const CustomExpression expression;
    const FlightDataType&  empty = expression.getType();
    expectCustomType(empty, "", "");
    EXPECT_EQ(empty.getUnitGroupId(), std::nullopt);
    EXPECT_EQ(empty.getUnitGroup().getDefaultUnit().getUnit(), "");
    EXPECT_EQ(FlightDataType::findBySymbol(""), &empty);
}

/// Runs the two checks and exits with 1 when one failed (a failure in the child of a death test
/// is not reported otherwise).
[[noreturn]] void checkTheTypesThatReplaceOthersAndExit()
{
    checkAnExpressionNamedAsABuiltinType();
    checkTheEmptyExpression();
    // NOLINTNEXTLINE(concurrency-mt-unsafe): the child of a death test, with no other threads
    std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

TEST(CustomExpressionDeathTest, ATypeCanReplaceABuiltinTypesSymbol)
{
    EXPECT_EXIT(checkTheTypesThatReplaceOthersAndExit(), ::testing::ExitedWithCode(0), "");
    // The replacement was made in the child process only.
    EXPECT_EQ(FlightDataType::findBySymbol("h"),
              &FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
}

}  // namespace
