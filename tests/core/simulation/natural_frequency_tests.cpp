// OpenRocket's NaturalFrequencyTest
// (core/src/test/java/info/openrocket/core/simulation/NaturalFrequencyTest.java).
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
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::StabilityDataRun;

// NaturalFrequencyTest.testNaturalFrequencyTypeIsDefined
TEST(NaturalFrequencyTest, NaturalFrequencyTypeIsDefined)
{
    const FlightDataType& type = FlightDataType::builtin(FlightDataTypeId::TYPE_NATURAL_FREQUENCY);
    EXPECT_EQ(&unitGroup(UnitGroupId::ROLL), &type.getUnitGroup());
    // Java: "ωn" (omega, n), here the UTF-8 bytes of the omega.
    EXPECT_EQ("\xCF\x89n", type.getSymbol());

    const std::span<const FlightDataType* const> allTypes = FlightDataType::allTypes();
    EXPECT_NE(std::ranges::find(allTypes, &type), allTypes.end());

    // Ensure we have a non-empty translated name for UI display (Java: !getName().isBlank()).
    EXPECT_FALSE(QtRocket::Strings::isEmpty(type.getName()));
}

class NaturalFrequencyRunTest : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// NaturalFrequencyTest.testNaturalFrequencyIsStoredInSimulationData
// (@EnumSource(SimulationStepperMethod))
TEST_P(NaturalFrequencyRunTest, NaturalFrequencyIsStoredInSimulationData)
{
    StabilityDataRun run(GetParam());

    simulateOrFail(run.simulation);

    const FlightData& data = simulatedData(run.simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<double>* omegaN =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_NATURAL_FREQUENCY));
    const std::vector<double>* time =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));

    ASSERT_NE(omegaN, nullptr);
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(time->size(), omegaN->size());
    ASSERT_FALSE(omegaN->empty());

    // Java's loop: the first value that is not NaN must not be infinite, and there must be one.
    const std::optional<double> firstValue = firstNotNaN(*omegaN);
    ASSERT_TRUE(firstValue.has_value())
        << "Expected at least one finite \xCF\x89n value during flight";
    EXPECT_FALSE(std::isinf(firstValue.value_or(0.0))) << "\xCF\x89n should never be infinite";
}

INSTANTIATE_TEST_SUITE_P(NaturalFrequencyTest, NaturalFrequencyRunTest,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         QtRocket::Test::stepperMethodTestName);

}  // namespace
