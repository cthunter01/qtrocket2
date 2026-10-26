// The motor databases of tests/core/motor/TestMotorDatabase.h: the small ones are what their
// comments say, and smallMotorDatabase() holds the bundled sets themselves.

#include <algorithm>
#include <cstddef>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorCorrelation.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "motor/TestMotorDatabase.h"

namespace
{

using QtRocket::Motor;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Test::kSmallMotorSets;
using QtRocket::Test::SmallMotorSet;

// The motor of Estes_A8.rse, a new object every time, and the database of IntegrationTest.
TEST(TestMotorDatabase, EstesA8DatabaseHoldsTheOneMotorOfItsFile)
{
    const std::shared_ptr<const QtRocket::ThrustCurveMotor> motor = QtRocket::Test::estesA8Motor();
    const std::shared_ptr<const QtRocket::ThrustCurveMotor> again = QtRocket::Test::estesA8Motor();
    ASSERT_NE(motor, nullptr);
    ASSERT_NE(again, nullptr);
    EXPECT_NE(motor, again);
    EXPECT_EQ(motor->getDigest(), again->getDigest());
    EXPECT_EQ(motor->getDesignation(), "A8");
    EXPECT_EQ(motor->getManufacturer().getDisplayName(), "Estes");

    const ThrustCurveMotorSetDatabase database = QtRocket::Test::estesA8Database();
    ASSERT_EQ(database.getMotorSets().size(), 1U);
    const ThrustCurveMotorSet& set = database.getMotorSets().front();
    ASSERT_EQ(set.getMotorCount(), 1U);
    EXPECT_EQ(set.getMotors().front()->getDigest(), motor->getDigest());
    EXPECT_EQ(set.getDesignation(), "A8");
}

/// The index of the set of the bundled database that @p spec names, or the number of sets when
/// there is none.
[[nodiscard]] std::size_t bundledIndex(const SmallMotorSet& spec)
{
    const std::deque<ThrustCurveMotorSet>& sets =
        QtRocket::Test::bundledMotorDatabase().getMotorSets();
    for (std::size_t i = 0; i < sets.size(); ++i)
    {
        if (sets.at(i).getDesignation() == spec.designation &&
            sets.at(i).getManufacturer()->matches(spec.manufacturer))
        {
            return i;
        }
    }
    return sets.size();
}

/// The data of @p set, but for its motors and manufacturer, for a comparison.
[[nodiscard]] std::string dataOf(const ThrustCurveMotorSet& set)
{
    std::string delays;
    for (const double delay : set.getDelays())
    {
        delays += std::format("{} ", delay);
    }
    return std::format("{} | {} | {}| {} | {} {} | {} | {} | {} | {}", set.getDesignation(),
                       set.getCommonName(), delays, static_cast<int>(set.getType()),
                       set.getDiameter(), set.getLength(), set.getTotalImpulse(), set.getCaseInfo(),
                       set.isAvailable(), set.toString());
}

/// Checks that @p small is @p bundled: the same motors (the objects) in the same order, the same
/// manufacturer and the same data.
void expectSameSet(const ThrustCurveMotorSet& small, const ThrustCurveMotorSet& bundled)
{
    EXPECT_EQ(small.getMotors(), bundled.getMotors());
    EXPECT_EQ(small.getManufacturer(), bundled.getManufacturer());
    EXPECT_EQ(dataOf(small), dataOf(bundled));
}

/// Checks the set @p i of @p small against its entry of kSmallMotorSets and the bundled set the
/// entry names; returns the index of that bundled set (the number of bundled sets when there is
/// none).
std::size_t checkSmallSet(const ThrustCurveMotorSetDatabase& small, std::size_t i)
{
    const std::deque<ThrustCurveMotorSet>& bundled =
        QtRocket::Test::bundledMotorDatabase().getMotorSets();
    const SmallMotorSet& spec  = kSmallMotorSets.at(i);
    const std::size_t    index = bundledIndex(spec);
    if (index == bundled.size())
    {
        ADD_FAILURE() << "no bundled set " << spec.manufacturer << " " << spec.designation;
        return index;
    }
    expectSameSet(small.getMotorSets().at(i), bundled.at(index));
    EXPECT_EQ(small.getMotorSets().at(i).getMotorCount(), spec.curves) << spec.designation;
    return index;
}

/// The bundled indices of the sets of @p small (checkSmallSet()), in its order.
[[nodiscard]] std::vector<std::size_t> checkSmallSets(const ThrustCurveMotorSetDatabase& small)
{
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < small.getMotorSets().size() && i < kSmallMotorSets.size(); ++i)
    {
        indices.push_back(checkSmallSet(small, i));
    }
    return indices;
}

/// The number of curves of @p database.
[[nodiscard]] std::size_t curveCount(const ThrustCurveMotorSetDatabase& database)
{
    std::size_t curves = 0;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        curves += set.getMotorCount();
    }
    return curves;
}

// Each set of the small database is the bundled set its entry of kSmallMotorSets names, with the
// number of curves the entry says, and the sets keep the bundled order.
TEST(TestMotorDatabase, SmallMotorDatabaseHoldsTheBundledSets)
{
    const ThrustCurveMotorSetDatabase small = QtRocket::Test::smallMotorDatabase();
    ASSERT_EQ(small.getMotorSets().size(), kSmallMotorSets.size());
    const std::vector<std::size_t> indices = checkSmallSets(small);
    EXPECT_TRUE(std::ranges::adjacent_find(indices, std::greater_equal<>{}) == indices.end())
        << "the bundled order, strictly";
    EXPECT_EQ(curveCount(small), 21U);
}

/// The set of @p database named @p designation; the test fails when there is none (and gets the
/// first set, so @p database must have one).
[[nodiscard]] const ThrustCurveMotorSet& setNamed(const ThrustCurveMotorSetDatabase& database,
                                                  std::string_view                   designation)
{
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        if (set.getDesignation() == designation)
        {
            return set;
        }
    }
    ADD_FAILURE() << "no set " << designation;
    return database.getMotorSets().front();
}

/// The types of the sets of @p database.
[[nodiscard]] std::set<Motor::Type> typesOf(const ThrustCurveMotorSetDatabase& database)
{
    std::set<Motor::Type> types;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        types.insert(set.getType());
    }
    return types;
}

/// The display names of the manufacturers of the sets of @p database.
[[nodiscard]] std::set<std::string> manufacturersOf(const ThrustCurveMotorSetDatabase& database)
{
    std::set<std::string> names;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        names.insert(set.getManufacturer()->getDisplayName());
    }
    return names;
}

/// MotorCorrelation::similarity() of the curves @p first and @p second of @p set.
[[nodiscard]] double similarityOf(const ThrustCurveMotorSet& set, std::size_t first,
                                  std::size_t second)
{
    return QtRocket::MotorCorrelation::similarity(*set.getMotors().at(first),
                                                  *set.getMotors().at(second));
}

// The kinds the comment of kSmallMotorSets promises: the three types, five manufacturers, a
// plugged-only set, a common name of its own, and two very similar curves of E15W (the chooser's
// threshold).
TEST(TestMotorDatabase, SmallMotorDatabaseHasTheBundledKinds)
{
    const ThrustCurveMotorSetDatabase small = QtRocket::Test::smallMotorDatabase();
    ASSERT_EQ(small.getMotorSets().size(), kSmallMotorSets.size());
    EXPECT_EQ(typesOf(small), (std::set<Motor::Type>{Motor::Type::SINGLE, Motor::Type::RELOAD,
                                                     Motor::Type::HYBRID}));
    EXPECT_EQ(manufacturersOf(small).size(), 5U);
    EXPECT_EQ(setNamed(small, "G12ST").getDelays(), std::vector<double>{Motor::kPluggedDelay});
    EXPECT_EQ(setNamed(small, "B6W").getCommonName(), "B6");
    EXPECT_EQ(setNamed(small, "J145H_2JET_STD").getType(), Motor::Type::HYBRID);
    EXPECT_EQ(setNamed(small, "J145H_2JET_STD").getCommonName(), "J145");
    EXPECT_EQ(setNamed(small, "E16W").getType(), Motor::Type::RELOAD);
    EXPECT_EQ(setNamed(small, "E16W").getCaseInfo(), "RMS-29/40-120");
    EXPECT_EQ(setNamed(small, "M1850W").getDiameter(), 0.075);
    EXPECT_EQ(setNamed(small, "117G69-14A").getCaseInfo(), "Pro38-1G");
    EXPECT_TRUE(setNamed(small, "117G69-14A").getMotors().front()->isSparky());
    EXPECT_EQ(setNamed(small, "1/2A3").getDiameter(), 0.013);

    const ThrustCurveMotorSet& e15 = setNamed(small, "E15W");
    ASSERT_EQ(e15.getMotorCount(), 3U);
    const double threshold = QtRocket::ThrustCurveMotorSelection::kMotorSimilarityThreshold;
    EXPECT_GE(similarityOf(e15, 0, 1), threshold);
    EXPECT_LT(similarityOf(e15, 0, 2), threshold);
    EXPECT_LT(similarityOf(e15, 1, 2), threshold);
}

}  // namespace
