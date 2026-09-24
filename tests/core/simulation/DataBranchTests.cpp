#include "QtRocket/simulation/DataBranch.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/DataType.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FlightDataType;
using QtRocket::ModId;
using QtRocket::UnitGroupId;
using Branch = QtRocket::DataBranch<FlightDataType>;
using Id     = QtRocket::FlightDataTypeId;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

[[nodiscard]] const FlightDataType& timeType()
{
    return type(Id::TYPE_TIME);
}

[[nodiscard]] const FlightDataType& altitudeType()
{
    return type(Id::TYPE_ALTITUDE);
}

[[nodiscard]] const FlightDataType& velocityType()
{
    return type(Id::TYPE_VELOCITY_TOTAL);
}

/// Runs @p body and returns the message of the BugError it throws, or "" when nothing is thrown.
std::string bugMessage(const std::function<void()>& body)
{
    try
    {
        body();
    }
    catch (const BugError& error)
    {
        return error.what();
    }
    return "";
}

/// Whether @p values are @p expected, NaN matching NaN.
void expectValues(const std::vector<double>& values, const std::vector<double>& expected)
{
    ASSERT_EQ(values.size(), expected.size());
    for (std::size_t i = 0; i < values.size(); i++)
    {
        if (std::isnan(expected[i]))
        {
            EXPECT_TRUE(std::isnan(values[i])) << "row " << i << ": " << values[i];
        }
        else
        {
            EXPECT_EQ(values[i], expected[i]) << "row " << i;
        }
    }
}

/// Whether @p column holds @p expected, NaN matching NaN (a column get() found).
void expectColumn(const std::optional<std::vector<double>>& column,
                  const std::vector<double>&                expected)
{
    EXPECT_TRUE(column.has_value());
    expectValues(column.value_or(std::vector<double>{}), expected);
}

/// A branch with time and altitude and three rows: t = 0, 1, 2 and h = 0, 10, 5.
Branch threeRows()
{
    Branch                      branch("three", {timeType(), altitudeType()});
    const std::array<double, 3> times{0.0, 1.0, 2.0};
    const std::array<double, 3> heights{0.0, 10.0, 5.0};
    for (std::size_t i = 0; i < times.size(); i++)
    {
        branch.addPoint();
        branch.setValue(timeType(), times.at(i));
        branch.setValue(altitudeType(), heights.at(i));
    }
    return branch;
}

static_assert(QtRocket::Monitorable<Branch>);
static_assert(std::is_same_v<decltype(std::declval<const Branch&>().getView(timeType())),
                             const std::vector<double>*>);

// ---- DataBranchViewTest.java (there on a FlightDataBranch, here on the DataBranch it extends) --

TEST(DataBranch, ValuesViewReflectsBranchMutations)
{
    Branch branch("test", {timeType()});

    branch.addPoint();
    branch.setValue(timeType(), 0.1);

    const std::vector<double>* view = branch.getView(timeType());
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->size(), 1U);
    EXPECT_EQ(view->at(0), 0.1);

    branch.addPoint();
    branch.setValue(timeType(), 0.2);

    EXPECT_EQ(view->size(), 2U) << "View should reflect newly appended samples";
    EXPECT_EQ(view->at(1), 0.2);
}

TEST(DataBranch, ValuesViewIsUnmodifiable)
{
    // Java checks that add() throws UnsupportedOperationException; here the view is a pointer to
    // const, so a change does not compile (see the static_assert above).
    Branch branch("test", {timeType()});
    branch.addPoint();
    branch.setValue(timeType(), 1.0);
    const std::vector<double>* view = branch.getView(timeType());
    ASSERT_NE(view, nullptr);
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(view)>>);
    EXPECT_EQ(*view, std::vector<double>{1.0});
}

TEST(DataBranch, ValuesViewForMissingTypeIsNull)
{
    const Branch branch("test", {timeType()});
    EXPECT_EQ(branch.getView(velocityType()), nullptr);
}

// ---- construction ----

TEST(DataBranch, ConstructorWithTypes)
{
    const Branch branch("Main", {altitudeType(), timeType()});
    EXPECT_EQ(branch.getName(), "Main");
    EXPECT_EQ(branch.getLength(), 0U);
    EXPECT_TRUE(branch.hasType(timeType()));
    EXPECT_TRUE(branch.hasType(altitudeType()));
    EXPECT_FALSE(branch.hasType(velocityType()));
    // Sorted by FlightDataType::compareTo, not in the given order.
    EXPECT_EQ(branch.getTypes(),
              (std::vector<const FlightDataType*>{&timeType(), &altitudeType()}));
    EXPECT_TRUE(branch.isMutable());
    EXPECT_EQ(branch.modId(), ModId::invalid());
    // A column without rows.
    ASSERT_TRUE(branch.get(timeType()).has_value());
    expectColumn(branch.get(timeType()), {});
    EXPECT_TRUE(std::isnan(branch.getLast(timeType())));
    EXPECT_TRUE(std::isnan(branch.getMinimum(timeType())));
    EXPECT_TRUE(std::isnan(branch.getMaximum(timeType())));
}

TEST(DataBranch, ConstructorWithASpanOfTypes)
{
    const std::array<const FlightDataType*, 3> types{&velocityType(), &timeType(), &altitudeType()};
    const Branch                               branch("span", types);
    EXPECT_EQ(branch.getTypes(),
              (std::vector<const FlightDataType*>{&timeType(), &altitudeType(), &velocityType()}));
    // getTypes() feeds another branch, as FlightDataBranch.clone() does.
    const Branch again("again", std::span<const FlightDataType* const>(branch.getTypes()));
    EXPECT_EQ(again.getTypes(), branch.getTypes());
}

TEST(DataBranch, ConstructorRejectsNoTypesAndDuplicates)
{
    // Java: IllegalArgumentException.
    EXPECT_NE(bugMessage([] {
                  const Branch b("x", std::span<const FlightDataType* const>{});
              }).find("Must specify at least one data type."),
              std::string::npos);
    EXPECT_NE(bugMessage([] {
                  const Branch b("x", {timeType(), altitudeType(), timeType()});
              }).find("Value type Time already exists."),
              std::string::npos);
    const std::array<const FlightDataType*, 1> withNull{nullptr};
    EXPECT_THROW(Branch("x", withNull), BugError);
    // A type equal to one already there (same name ignoring case) is a duplicate too.
    const FlightDataType& sameName =
        FlightDataType::getType("TIME", "qtrBranchTimeUpper", UnitGroupId::LONG_TIME);
    EXPECT_THROW(Branch("x", {timeType(), sameName}), BugError);
}

TEST(DataBranch, BranchWithoutTypes)
{
    Branch branch("Empty");
    EXPECT_EQ(branch.getLength(), 0U);
    EXPECT_FALSE(branch.hasType(timeType()));
    EXPECT_FALSE(branch.get(timeType()).has_value());
    // Java: IllegalStateException.
    EXPECT_NE(bugMessage([&branch] {
                  static_cast<void>(branch.getTypes());
              }).find("No data types have been added to branch Empty"),
              std::string::npos);
    // addPoint() without columns adds nothing but is a change.
    branch.addPoint();
    EXPECT_EQ(branch.getLength(), 0U);
    EXPECT_NE(branch.modId(), ModId::invalid());
    // setValue() adds the type.
    branch.setValue(timeType(), 3.0);
    EXPECT_EQ(branch.getTypes(), std::vector<const FlightDataType*>{&timeType()});
    EXPECT_EQ(branch.getLength(), 0U);
}

// ---- two-phase rows ----

TEST(DataBranch, AddPointOpensARowOfNaN)
{
    Branch branch("rows", {timeType(), altitudeType()});
    branch.addPoint();
    EXPECT_EQ(branch.getLength(), 1U);
    EXPECT_TRUE(std::isnan(branch.getLast(timeType())));
    EXPECT_TRUE(std::isnan(branch.getLast(altitudeType())));
    branch.setValue(timeType(), 0.5);
    EXPECT_EQ(branch.getLast(timeType()), 0.5);
    EXPECT_TRUE(std::isnan(branch.getLast(altitudeType())));  // a value not set stays NaN

    branch.addPoint();
    branch.setValue(altitudeType(), 12.0);
    EXPECT_EQ(branch.getLength(), 2U);
    expectColumn(branch.get(timeType()), {0.5, kNaN});
    expectColumn(branch.get(altitudeType()), {kNaN, 12.0});
}

TEST(DataBranch, SetValueWritesTheLastRowOnly)
{
    Branch branch = threeRows();
    branch.setValue(altitudeType(), 7.0);
    expectColumn(branch.get(altitudeType()), {0.0, 10.0, 7.0});
    expectColumn(branch.get(timeType()), {0.0, 1.0, 2.0});
    EXPECT_EQ(branch.getLast(altitudeType()), 7.0);
}

TEST(DataBranch, SetValueOfANewTypeAddsAColumnOfNaN)
{
    // Java: "New variable types can be added to the FlightDataBranch transparently".
    Branch branch = threeRows();
    branch.setValue(velocityType(), 4.0);
    EXPECT_EQ(branch.getLength(), 3U);
    expectColumn(branch.get(velocityType()), {kNaN, kNaN, 4.0});
    EXPECT_EQ(branch.getMinimum(velocityType()), 4.0);
    EXPECT_EQ(branch.getMaximum(velocityType()), 4.0);
    EXPECT_EQ(branch.getTypes(),
              (std::vector<const FlightDataType*>{&timeType(), &altitudeType(), &velocityType()}));
    branch.addPoint();
    expectColumn(branch.get(velocityType()), {kNaN, kNaN, 4.0, kNaN});
}

TEST(DataBranch, SetValueBeforeTheFirstRowOnlyUpdatesMinAndMax)
{
    Branch branch("early", {timeType()});
    branch.setValue(timeType(), 5.0);
    EXPECT_EQ(branch.getLength(), 0U);
    expectColumn(branch.get(timeType()), {});
    EXPECT_TRUE(std::isnan(branch.getLast(timeType())));
    EXPECT_EQ(branch.getMinimum(timeType()), 5.0);
    EXPECT_EQ(branch.getMaximum(timeType()), 5.0);
}

TEST(DataBranch, AddTypeAddsAColumnOfNaN)
{
    Branch      branch = threeRows();
    const ModId before = branch.modId();
    branch.addType(velocityType());
    EXPECT_GT(branch.modId(), before);
    expectColumn(branch.get(velocityType()), {kNaN, kNaN, kNaN});
    EXPECT_TRUE(std::isnan(branch.getMinimum(velocityType())));
    EXPECT_TRUE(std::isnan(branch.getMaximum(velocityType())));
    EXPECT_NE(bugMessage([&branch] {
                  branch.addType(timeType());
              }).find("Value type Time already exists."),
              std::string::npos);
}

// ---- reading ----

TEST(DataBranch, GetReturnsAnIndependentCopy)
{
    Branch branch = threeRows();
    ASSERT_TRUE(branch.get(altitudeType()).has_value());
    std::vector<double> copy = branch.get(altitudeType()).value_or(std::vector<double>{});
    expectValues(copy, {0.0, 10.0, 5.0});
    branch.addPoint();
    branch.setValue(altitudeType(), 1.0);
    expectValues(copy, {0.0, 10.0, 5.0});
    copy.at(0) = 99.0;
    EXPECT_EQ(branch.getByIndex(altitudeType(), 0), 0.0);
    EXPECT_FALSE(branch.get(velocityType()).has_value());
}

TEST(DataBranch, ViewStaysValidWhenColumnsAreAdded)
{
    Branch                     branch = threeRows();
    const std::vector<double>* view   = branch.getView(altitudeType());
    ASSERT_NE(view, nullptr);
    // New columns, through setValue() and addType(), do not move the existing ones.
    for (const FlightDataType* t : FlightDataType::allTypes())
    {
        branch.setValue(*t, 1.0);
    }
    EXPECT_EQ(branch.getView(altitudeType()), view);
    expectValues(*view, {0.0, 10.0, 1.0});
    branch.addPoint();
    EXPECT_EQ(view->size(), 4U);
}

TEST(DataBranch, GetByIndex)
{
    const Branch branch = threeRows();
    EXPECT_EQ(branch.getByIndex(timeType(), 0), 0.0);
    EXPECT_EQ(branch.getByIndex(timeType(), 2), 2.0);
    EXPECT_EQ(branch.getByIndex(altitudeType(), 1), 10.0);
    // A type the branch lacks: null in Java.
    EXPECT_EQ(branch.getByIndex(velocityType(), 1), std::nullopt);
    // Java: IllegalArgumentException("Index out of bounds"), checked before the type.
    EXPECT_NE(bugMessage([&branch] {
                  static_cast<void>(branch.getByIndex(timeType(), 3));
              }).find("out of bounds"),
              std::string::npos);
    EXPECT_THROW(static_cast<void>(branch.getByIndex(velocityType(), 3)), BugError);
    const Branch empty("empty", {timeType()});
    EXPECT_THROW(static_cast<void>(empty.getByIndex(timeType(), 0)), BugError);
}

TEST(DataBranch, GetLast)
{
    Branch branch = threeRows();
    EXPECT_EQ(branch.getLast(timeType()), 2.0);
    EXPECT_EQ(branch.getLast(altitudeType()), 5.0);
    EXPECT_TRUE(std::isnan(branch.getLast(velocityType())));
    branch.addPoint();
    EXPECT_TRUE(std::isnan(branch.getLast(timeType())));
}

// ---- minimum and maximum ----

TEST(DataBranch, MinimumAndMaximum)
{
    const Branch branch = threeRows();
    EXPECT_EQ(branch.getMinimum(altitudeType()), 0.0);
    EXPECT_EQ(branch.getMaximum(altitudeType()), 10.0);
    EXPECT_EQ(branch.getMinimum(timeType()), 0.0);
    EXPECT_EQ(branch.getMaximum(timeType()), 2.0);
    EXPECT_TRUE(std::isnan(branch.getMinimum(velocityType())));
    EXPECT_TRUE(std::isnan(branch.getMaximum(velocityType())));
}

TEST(DataBranch, MinimumAndMaximumCountEveryValueSet)
{
    // As in Java, an overwritten value still counts.
    Branch branch("overwrite", {altitudeType()});
    branch.addPoint();
    branch.setValue(altitudeType(), 5.0);
    branch.setValue(altitudeType(), -3.0);
    branch.setValue(altitudeType(), 1.0);
    EXPECT_EQ(branch.getLast(altitudeType()), 1.0);
    EXPECT_EQ(branch.getMinimum(altitudeType()), -3.0);
    EXPECT_EQ(branch.getMaximum(altitudeType()), 5.0);
}

TEST(DataBranch, MinimumAndMaximumWithNaN)
{
    Branch branch("nan", {altitudeType()});
    branch.addPoint();
    // A NaN first value makes both NaN ...
    branch.setValue(altitudeType(), kNaN);
    EXPECT_TRUE(std::isnan(branch.getMinimum(altitudeType())));
    EXPECT_TRUE(std::isnan(branch.getMaximum(altitudeType())));
    // ... which the next number replaces ...
    branch.addPoint();
    branch.setValue(altitudeType(), 2.0);
    EXPECT_EQ(branch.getMinimum(altitudeType()), 2.0);
    EXPECT_EQ(branch.getMaximum(altitudeType()), 2.0);
    // ... and a later NaN leaves them alone.
    branch.addPoint();
    branch.setValue(altitudeType(), kNaN);
    EXPECT_EQ(branch.getMinimum(altitudeType()), 2.0);
    EXPECT_EQ(branch.getMaximum(altitudeType()), 2.0);
    // Infinities are ordinary values.
    branch.setValue(altitudeType(), -std::numeric_limits<double>::infinity());
    branch.setValue(altitudeType(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(branch.getMinimum(altitudeType()), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(branch.getMaximum(altitudeType()), std::numeric_limits<double>::infinity());
}

// ---- types ----

TEST(DataBranch, GetTypesIsAStableSort)
{
    // TYPE_THRUST_FORCE and TYPE_THRUST_CORRECTION compare equal: they keep the order they were
    // added in, whichever it is.
    const FlightDataType& force      = type(Id::TYPE_THRUST_FORCE);
    const FlightDataType& correction = type(Id::TYPE_THRUST_CORRECTION);
    const FlightDataType& drag       = type(Id::TYPE_DRAG_FORCE);
    const Branch          first("a", {drag, correction, timeType(), force});
    EXPECT_EQ(first.getTypes(),
              (std::vector<const FlightDataType*>{&timeType(), &correction, &force, &drag}));
    const Branch second("b", {force, drag, correction});
    EXPECT_EQ(second.getTypes(), (std::vector<const FlightDataType*>{&force, &correction, &drag}));
    // Custom types (all CUSTOM with the default priority) keep their order after the built-ins.
    const FlightDataType& customB =
        FlightDataType::getType("QtRocket branch B", "qtrBranchB", UnitGroupId::NONE);
    const FlightDataType& customA =
        FlightDataType::getType("QtRocket branch A", "qtrBranchA", UnitGroupId::NONE);
    const Branch customs("c", {customB, timeType(), customA});
    EXPECT_EQ(customs.getTypes(),
              (std::vector<const FlightDataType*>{&timeType(), &customB, &customA}));
}

TEST(DataBranch, AnEqualTypeFindsTheSameColumn)
{
    // Java's map compares keys with equals(): a type with the same name ignoring case, such as
    // the replacement getType() makes when a custom expression's units change, finds the
    // column of the original.
    const FlightDataType& original =
        FlightDataType::getType("QtRocket branch expr", "qtrBranchExpr", UnitGroupId::LENGTH);
    const FlightDataType& replacement =
        FlightDataType::getType("QtRocket branch expr", "qtrBranchExpr", UnitGroupId::VELOCITY);
    ASSERT_NE(&original, &replacement);

    Branch branch("equal", {timeType(), original});
    branch.addPoint();
    branch.setValue(replacement, 3.0);
    EXPECT_TRUE(branch.hasType(replacement));
    EXPECT_EQ(branch.getLast(original), 3.0);
    EXPECT_EQ(branch.getView(replacement), branch.getView(original));
    EXPECT_EQ(branch.getMaximum(replacement), 3.0);
    EXPECT_EQ(branch.getByIndex(replacement, 0), 3.0);
    // The key stays the first object.
    EXPECT_EQ(branch.getTypes(), (std::vector<const FlightDataType*>{&timeType(), &original}));
    // Case does not matter either.
    const FlightDataType& shouting =
        FlightDataType::getType("QTROCKET BRANCH EXPR", "qtrBranchExprUpper", UnitGroupId::NONE);
    EXPECT_EQ(branch.getLast(shouting), 3.0);
    EXPECT_THROW(branch.addType(shouting), BugError);
}

// ---- immutability ----

TEST(DataBranch, ImmutableBranchRefusesChanges)
{
    Branch branch = threeRows();
    EXPECT_TRUE(branch.isMutable());
    const auto line = std::source_location::current().line() + 1;
    branch.immute();
    branch.immute();  // repeated calls do nothing
    EXPECT_FALSE(branch.isMutable());

    const std::string where = "DataBranchTests.cpp:" + std::to_string(line);
    EXPECT_NE(bugMessage([&branch] { branch.addPoint(); }).find(where), std::string::npos);
    EXPECT_NE(bugMessage([&branch] { branch.setValue(timeType(), 1.0); }).find(where),
              std::string::npos);
    EXPECT_NE(bugMessage([&branch] {
                  branch.setValue(velocityType(), 1.0);
              }).find("Object has been made immutable at " + where),
              std::string::npos);
    EXPECT_THROW(branch.addType(velocityType()), BugError);

    // Nothing changed, and reading still works.
    EXPECT_EQ(branch.getLength(), 3U);
    EXPECT_FALSE(branch.hasType(velocityType()));
    EXPECT_EQ(branch.getLast(timeType()), 2.0);
    EXPECT_EQ(branch.getMaximum(altitudeType()), 10.0);
    expectColumn(branch.get(altitudeType()), {0.0, 10.0, 5.0});
}

// ---- modification ids ----

TEST(DataBranch, EveryChangeDrawsAModId)
{
    Branch branch("mod", {timeType()});
    EXPECT_EQ(branch.modId(), ModId::invalid());
    branch.addPoint();
    const ModId afterPoint = branch.modId();
    EXPECT_GT(afterPoint, ModId::zero());
    branch.setValue(timeType(), 1.0);
    const ModId afterValue = branch.modId();
    EXPECT_GT(afterValue, afterPoint);
    branch.addType(altitudeType());
    EXPECT_GT(branch.modId(), afterValue);
    // Reading changes nothing.
    const ModId current = branch.modId();
    static_cast<void>(branch.get(timeType()));
    static_cast<void>(branch.getLast(timeType()));
    static_cast<void>(branch.getTypes());
    EXPECT_EQ(branch.modId(), current);
    // immute() is not a change of the data.
    branch.immute();
    EXPECT_EQ(branch.modId(), current);
}

// ---- copies ----

TEST(DataBranch, CopiesAreIndependent)
{
    Branch original = threeRows();
    Branch copy     = original;
    EXPECT_EQ(copy.getName(), "three");
    EXPECT_EQ(copy.modId(), original.modId());
    EXPECT_EQ(copy.getTypes(), original.getTypes());

    copy.addPoint();
    copy.setValue(altitudeType(), 100.0);
    copy.setValue(velocityType(), 2.0);
    EXPECT_EQ(original.getLength(), 3U);
    EXPECT_EQ(original.getMaximum(altitudeType()), 10.0);
    EXPECT_FALSE(original.hasType(velocityType()));
    EXPECT_EQ(copy.getLength(), 4U);
    EXPECT_EQ(copy.getMaximum(altitudeType()), 100.0);

    original.setValue(timeType(), -1.0);
    EXPECT_EQ(copy.getByIndex(timeType(), 2), 2.0);
    EXPECT_EQ(copy.getMinimum(timeType()), 0.0);
    // Each has its own storage.
    EXPECT_NE(copy.getView(timeType()), original.getView(timeType()));

    // Assignment replaces the whole branch.
    copy = original;
    EXPECT_EQ(copy.getLength(), 3U);
    EXPECT_FALSE(copy.hasType(velocityType()));
    EXPECT_EQ(copy.getLast(timeType()), -1.0);
}

TEST(DataBranch, CopyKeepsImmutabilityAndCloneDoesNot)
{
    Branch original = threeRows();
    original.immute();

    const Branch copy = original;
    EXPECT_FALSE(copy.isMutable());

    // clone(): FlightDataBranch.clone() gives a mutable branch with the same data and ModId.
    Branch clone = original.clone();
    EXPECT_TRUE(clone.isMutable());
    EXPECT_EQ(clone.modId(), original.modId());
    EXPECT_EQ(clone.getName(), original.getName());
    EXPECT_EQ(clone.getTypes(), original.getTypes());
    expectColumn(clone.get(altitudeType()), {0.0, 10.0, 5.0});
    EXPECT_EQ(clone.getMinimum(altitudeType()), 0.0);
    EXPECT_EQ(clone.getMaximum(altitudeType()), 10.0);
    clone.addPoint();
    clone.setValue(altitudeType(), 20.0);
    EXPECT_EQ(clone.getMaximum(altitudeType()), 20.0);
    EXPECT_EQ(original.getMaximum(altitudeType()), 10.0);
    EXPECT_EQ(original.getLength(), 3U);
    EXPECT_FALSE(original.isMutable());
}

TEST(DataBranch, MoveKeepsTheData)
{
    Branch original = threeRows();
    Branch moved    = std::move(original);
    EXPECT_EQ(moved.getLength(), 3U);
    EXPECT_EQ(moved.getLast(altitudeType()), 5.0);
    moved.addPoint();
    EXPECT_EQ(moved.getLength(), 4U);
}

// ---- another data type ----

/// A minimal data type of its own, as CADataType will be.
class TestType final : public QtRocket::DataType
{
public:
    TestType(std::string name, int priority) : m_name(std::move(name)), m_priority(priority) { }

    [[nodiscard]] const std::string&         getName() const override { return m_name; }
    [[nodiscard]] const std::string&         getSymbol() const override { return m_name; }
    [[nodiscard]] const QtRocket::UnitGroup& getUnitGroup() const override
    {
        return QtRocket::unitGroup(UnitGroupId::NONE);
    }
    [[nodiscard]] bool equals(const TestType& other) const { return m_name == other.m_name; }
    [[nodiscard]] int  hashCode() const { return static_cast<int>(m_name.size()); }
    [[nodiscard]] int  compareTo(const TestType& other) const
    {
        return m_priority - other.m_priority;
    }

private:
    std::string m_name;
    int         m_priority;
};

TEST(DataBranch, WorksWithAnyDataType)
{
    const TestType                 first("first", 2);
    const TestType                 second("second", 1);
    const TestType                 sameAsFirst("first", 5);
    QtRocket::DataBranch<TestType> branch("custom", {first, second});
    branch.addPoint();
    branch.setValue(first, 1.5);
    branch.setValue(sameAsFirst, 2.5);  // equal to first: the same column
    EXPECT_EQ(branch.getLast(first), 2.5);
    EXPECT_EQ(branch.getMinimum(first), 1.5);
    EXPECT_EQ(branch.getTypes(), (std::vector<const TestType*>{&second, &first}));
}

}  // namespace
