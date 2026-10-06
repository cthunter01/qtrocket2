// OpenRocket's DampingMomentCoefficientTest
// (core/src/test/java/info/openrocket/core/simulation/DampingMomentCoefficientTest.java).
// The simulation runs under the preferences of OpenRocket's test set-up (SimulationRunSupport.h).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Strings.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::SimulationStepperMethod;
using QtRocket::unitGroup;
using QtRocket::UnitGroupId;
using QtRocket::Test::junitEquals;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::StabilityDataRun;

/// DampingMomentCoefficientTest.SUM_TOLERANCE
constexpr double kSumTolerance = 1.0e-12;
/// DampingMomentCoefficientTest.NON_ZERO_THRESHOLD
constexpr double kNonZeroThreshold = 1.0e-9;

// DampingMomentCoefficientTest.testDampingMomentCoefficientTypesAreDefined
TEST(DampingMomentCoefficientTest, DampingMomentCoefficientTypesAreDefined)
{
    const FlightDataType& total =
        FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF);
    const FlightDataType& aerodynamic =
        FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC);
    const FlightDataType& propulsive =
        FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE);

    EXPECT_EQ(&unitGroup(UnitGroupId::ANGULAR_MOMENTUM), &total.getUnitGroup());
    EXPECT_EQ(&unitGroup(UnitGroupId::ANGULAR_MOMENTUM), &aerodynamic.getUnitGroup());
    EXPECT_EQ(&unitGroup(UnitGroupId::ANGULAR_MOMENTUM), &propulsive.getUnitGroup());

    EXPECT_EQ("Cdm", total.getSymbol());
    EXPECT_EQ("Cdm_aero", aerodynamic.getSymbol());
    EXPECT_EQ("Cdm_prop", propulsive.getSymbol());

    const std::span<const FlightDataType* const> allTypes = FlightDataType::allTypes();
    EXPECT_NE(std::ranges::find(allTypes, &total), allTypes.end());
    EXPECT_NE(std::ranges::find(allTypes, &aerodynamic), allTypes.end());
    EXPECT_NE(std::ranges::find(allTypes, &propulsive), allTypes.end());

    // (Java: !getName().isBlank())
    EXPECT_FALSE(QtRocket::Strings::isEmpty(aerodynamic.getName()));
    EXPECT_FALSE(QtRocket::Strings::isEmpty(propulsive.getName()));
}

/// What the loop of testDampingMomentCoefficientStoredAndConsistent finds.
struct Consistency
{
    std::vector<std::string> failures;  ///< the assertions of the loop that do not hold
    bool                     foundNonZeroAerodynamic{false};
    bool                     foundNonZeroPropulsive{false};
};

/// The loop of testDampingMomentCoefficientStoredAndConsistent over the three columns, which
/// have the same length: where the total is NaN the two contributions are NaN; elsewhere they
/// add up to the total within SUM_TOLERANCE and the propulsive one is not negative.
[[nodiscard]] Consistency checkConsistency(const std::vector<double>& total,
                                           const std::vector<double>& aerodynamic,
                                           const std::vector<double>& propulsive)
{
    Consistency found;
    for (std::size_t i = 0; i < total.size(); i++)
    {
        const double t = total[i];
        const double a = aerodynamic[i];
        const double p = propulsive[i];

        if (std::isnan(t))
        {
            if (!std::isnan(a) || !std::isnan(p))
            {
                found.failures.push_back(std::format(
                    "row {}: the total is NaN, the contributions are {} and {}", i, a, p));
            }
            continue;
        }

        if (!junitEquals(t, a + p, kSumTolerance))
        {
            found.failures.push_back(std::format("row {}: {} is not {} + {}", i, t, a, p));
        }
        if (!(p >= 0.0))
        {
            found.failures.push_back(std::format(
                "row {}: Propulsive damping moment coefficient should not be negative: {}", i, p));
        }
        found.foundNonZeroAerodynamic |= std::abs(a) > kNonZeroThreshold;
        found.foundNonZeroPropulsive |= std::abs(p) > kNonZeroThreshold;
    }
    return found;
}

class DampingMomentCoefficientRunTest : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// DampingMomentCoefficientTest.testDampingMomentCoefficientStoredAndConsistent
// (@EnumSource(SimulationStepperMethod))
TEST_P(DampingMomentCoefficientRunTest, DampingMomentCoefficientStoredAndConsistent)
{
    StabilityDataRun run(GetParam());

    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<double>* total =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF));
    const std::vector<double>* aerodynamic = branch.getView(
        FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC));
    const std::vector<double>* propulsive = branch.getView(
        FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_PROPULSIVE));

    ASSERT_NE(total, nullptr);
    ASSERT_NE(aerodynamic, nullptr);
    ASSERT_NE(propulsive, nullptr);
    ASSERT_EQ(total->size(), aerodynamic->size());
    ASSERT_EQ(total->size(), propulsive->size());
    ASSERT_FALSE(total->empty());

    const Consistency found = checkConsistency(*total, *aerodynamic, *propulsive);
    EXPECT_EQ(found.failures, std::vector<std::string>{});
    EXPECT_TRUE(found.foundNonZeroAerodynamic)
        << "Expected at least one non-zero aerodynamic contribution";
    EXPECT_TRUE(found.foundNonZeroPropulsive)
        << "Expected at least one non-zero propulsive contribution";
}

INSTANTIATE_TEST_SUITE_P(DampingMomentCoefficientTest, DampingMomentCoefficientRunTest,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         QtRocket::Test::stepperMethodTestName);

}  // namespace
