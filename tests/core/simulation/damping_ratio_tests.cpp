// OpenRocket's DampingRatioTest
// (core/src/test/java/info/openrocket/core/simulation/DampingRatioTest.java).
// The simulation runs under the preferences of OpenRocket's test set-up (SimulationRunSupport.h).

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
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
using QtRocket::Test::firstNotNaN;
using QtRocket::Test::junitEquals;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::StabilityDataRun;

// DampingRatioTest.testDampingRatioTypeIsDefined
TEST(DampingRatioTest, DampingRatioTypeIsDefined)
{
    const FlightDataType& type = FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_RATIO);
    EXPECT_EQ(&unitGroup(UnitGroupId::COEFFICIENT), &type.getUnitGroup());
    // Java: "ζ" (zeta), here its UTF-8 bytes.
    EXPECT_EQ("\xCE\xB6", type.getSymbol());

    const std::span<const FlightDataType* const> allTypes = FlightDataType::allTypes();
    EXPECT_NE(std::ranges::find(allTypes, &type), allTypes.end());

    // Ensure we have a non-empty translated name for UI display (Java: !getName().isBlank()).
    EXPECT_FALSE(QtRocket::Strings::isEmpty(type.getName()));
}

class DampingRatioRunTest : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// DampingRatioTest.testDampingRatioIsStoredInSimulationData
// (@EnumSource(SimulationStepperMethod))
TEST_P(DampingRatioRunTest, DampingRatioIsStoredInSimulationData)
{
    StabilityDataRun run(GetParam());

    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<double>* zeta =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_DAMPING_RATIO));
    const std::vector<double>* time =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));

    ASSERT_NE(zeta, nullptr);
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(time->size(), zeta->size());
    ASSERT_FALSE(zeta->empty());

    // At t=0 the rocket is still on the launch rod, so zeta is intentionally 0.
    EXPECT_TRUE(junitEquals(0.0, zeta->front())) << "Expected NaN at t=0 (still on launch rod)";

    // Java's loop: the first value that is not NaN must not be infinite, and there must be one.
    const std::optional<double> firstValue = firstNotNaN(*zeta);
    ASSERT_TRUE(firstValue.has_value())
        << "Expected at least one finite \xCE\xB6 value during flight";
    EXPECT_FALSE(std::isinf(firstValue.value_or(0.0))) << "\xCE\xB6 should never be infinite";
}

INSTANTIATE_TEST_SUITE_P(DampingRatioTest, DampingRatioRunTest,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         QtRocket::Test::stepperMethodTestName);

}  // namespace
