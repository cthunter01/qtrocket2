// OpenRocket's CorrectiveMomentCoefficientTest
// (core/src/test/java/info/openrocket/core/simulation/CorrectiveMomentCoefficientTest.java).
// The simulation runs under the preferences of OpenRocket's test set-up (SimulationRunSupport.h).

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string_view>
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

// CorrectiveMomentCoefficientTest.testCorrectiveMomentCoefficientTypeIsDefined
TEST(CorrectiveMomentCoefficientTest, CorrectiveMomentCoefficientTypeIsDefined)
{
    const FlightDataType& type =
        FlightDataType::builtin(FlightDataTypeId::TYPE_CORRECTIVE_MOMENT_COEFF);
    EXPECT_EQ(&unitGroup(UnitGroupId::MOMENT), &type.getUnitGroup());
    EXPECT_EQ("Ccm", type.getSymbol());

    const std::span<const FlightDataType* const> allTypes = FlightDataType::allTypes();
    EXPECT_NE(std::ranges::find(allTypes, &type), allTypes.end());

    // Ensure we have a non-empty translated name for UI display (Java: !getName().isBlank()).
    EXPECT_FALSE(QtRocket::Strings::isEmpty(type.getName()));
}

class CorrectiveMomentCoefficientRunTest : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// CorrectiveMomentCoefficientTest.testCorrectiveMomentCoefficientIsStoredInSimulationData
// (@EnumSource(SimulationStepperMethod))
TEST_P(CorrectiveMomentCoefficientRunTest, CorrectiveMomentCoefficientIsStoredInSimulationData)
{
    StabilityDataRun run(GetParam());

    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<double>* time =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
    const std::vector<double>* ccm =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_CORRECTIVE_MOMENT_COEFF));
    ASSERT_NE(time, nullptr);
    ASSERT_NE(ccm, nullptr);
    ASSERT_FALSE(ccm->empty());
    EXPECT_EQ(time->size(), ccm->size());

    // At t=0 the rocket is still on the launch rod, so Ccm is intentionally 0.
    EXPECT_TRUE(junitEquals(0.0, ccm->front())) << "Expected NaN at t=0 (still on launch rod)";

    // Java's loop: the first value that is not NaN must not be infinite, and there must be one.
    const std::optional<double> firstValue = firstNotNaN(*ccm);
    ASSERT_TRUE(firstValue.has_value()) << "Expected at least one finite Ccm value during flight";
    EXPECT_FALSE(std::isinf(firstValue.value_or(0.0))) << "Ccm should never be infinite";
}

INSTANTIATE_TEST_SUITE_P(CorrectiveMomentCoefficientTest, CorrectiveMomentCoefficientRunTest,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         QtRocket::Test::stepperMethodTestName);

}  // namespace
