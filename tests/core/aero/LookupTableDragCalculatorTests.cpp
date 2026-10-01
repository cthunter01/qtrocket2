#include "QtRocket/aero/LookupTableDragCalculator.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::DragCalculator;
using QtRocket::ErrorCode;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::LookupTableDragCalculator;
using QtRocket::MachAoALookup;
using QtRocket::Rocket;
using QtRocket::WarningSet;
using QtRocket::Test::TempDir;
namespace CsvMachAoALookup = QtRocket::CsvMachAoALookup;
namespace MathUtil         = QtRocket::MathUtil;

constexpr double kEpsilon = 1e-6;  // LookupTableDragCalculatorTest.EPSILON
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();

/// The columns the drag calculator asks for (List.of("cd")).
[[nodiscard]] std::vector<std::string> cdColumns()
{
    return {"cd"};
}

/// A calculator for the CSV @p text, written to a file in @p dir (Java: Files.writeString and
/// CsvMachAoALookup.fromCsv(csv, List.of("cd"))).
[[nodiscard]] LookupTableDragCalculator calculatorFor(const TempDir& dir, std::string_view text)
{
    const std::filesystem::path csv   = dir.write("drag.csv", text);
    auto                        table = CsvMachAoALookup::fromCsv(csv, cdColumns());
    if (!table)
    {
        ADD_FAILURE() << table.error().toString();
        return LookupTableDragCalculator{*MachAoALookup::dragBuilder().addDragData(0, 0).build()};
    }
    return LookupTableDragCalculator{std::move(*table)};
}

/// The configuration the calculator ignores (Java passes null).
struct Configuration
{
    Rocket              rocket;
    FlightConfiguration config{rocket};
};

// ---- Ported from LookupTableDragCalculatorTest.java ----

TEST(LookupTableDragCalculator, UsesTableCdValue)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator =
        calculatorFor(dir, "mach,aoa,cd\n0,0,0.20\n1,0,0.40\n1,10,0.50\n");

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, nullptr, nullptr, total, warnings);

    EXPECT_NEAR(0.30, total.getCD(), kEpsilon);
    EXPECT_NEAR(0.30, total.getCDaxial(), kEpsilon);
    EXPECT_NEAR(0.30, total.getFrictionCD(), kEpsilon);
    EXPECT_NEAR(0, total.getPressureCD(), kEpsilon);
    EXPECT_NEAR(0, total.getBaseCD(), kEpsilon);
}

TEST(LookupTableDragCalculator, InterpolatesWithAoA)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator =
        calculatorFor(dir, "mach,aoa,cd\n0,0,0.20\n0,10,0.40\n1,0,0.30\n1,10,0.50\n");

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(MathUtil::javaToRadians(5));

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, nullptr, nullptr, total, warnings);

    // Should interpolate to 0.35 at mach=0.5, aoa=5
    EXPECT_NEAR(0.35, total.getCD(), kEpsilon);
}

TEST(LookupTableDragCalculator, ToAxialDragWithZeroAOA)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setAOA(0);

    // At zero AOA, axial drag should equal total drag
    const double cd    = 0.5;
    const double axial = calculator.toAxialDrag(conditions, cd);
    EXPECT_NEAR(cd, axial, kEpsilon);
}

TEST(LookupTableDragCalculator, ToAxialDragWithSmallAOA)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setAOA(MathUtil::javaToRadians(10));  // 10 degrees

    const double cd    = 0.5;
    const double axial = calculator.toAxialDrag(conditions, cd);
    // At 10 degrees AOA (< 17°), multiplier is between 1.0 and 1.3, so axial >= cd
    // The multiplier increases from 1.0 at 0° to 1.3 at 17°
    EXPECT_GE(axial, cd * 1.0);
    EXPECT_LE(axial, cd * 1.3);
    EXPECT_GT(axial, 0);
}

TEST(LookupTableDragCalculator, ToAxialDragWithLargeAOA)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setAOA(MathUtil::javaToRadians(45));  // 45 degrees

    const double cd    = 0.5;
    const double axial = calculator.toAxialDrag(conditions, cd);
    // At large AOA, axial drag should be significantly less than total drag
    EXPECT_LT(axial, cd);
    EXPECT_GT(axial, 0);
}

TEST(LookupTableDragCalculator, ToAxialDragWithNegativeAOA)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setAOA(-MathUtil::javaToRadians(10));  // -10 degrees

    const double cd    = 0.5;
    const double axial = calculator.toAxialDrag(conditions, cd);
    // Should handle negative AOA (clamped to 0)
    EXPECT_LE(std::abs(axial), cd);
}

TEST(LookupTableDragCalculator, ToAxialDragWithAOAOver90Degrees)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setAOA(MathUtil::javaToRadians(120));  // 120 degrees

    const double cd    = 0.5;
    const double axial = calculator.toAxialDrag(conditions, cd);
    // At AOA > 90, should return negative axial drag
    EXPECT_LT(axial, 0);
}

TEST(LookupTableDragCalculator, ZerosComponentForces)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    ForceMap          componentForces;
    AerodynamicForces componentForce;
    componentForce.zero();
    componentForce.setCD(1.0);
    componentForce.setFrictionCD(0.5);
    componentForces.put(nullptr, componentForce);

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, &componentForces, nullptr, total, warnings);

    // Component forces should be zeroed
    const AerodynamicForces& zeroed = *componentForces.get(nullptr);
    EXPECT_NEAR(0, zeroed.getCD(), kEpsilon);
    EXPECT_NEAR(0, zeroed.getFrictionCD(), kEpsilon);
    EXPECT_NEAR(0, zeroed.getCDaxial(), kEpsilon);
}

TEST(LookupTableDragCalculator, ZerosAssemblyForces)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    ForceMap          assemblyForces;
    AerodynamicForces assemblyForce;
    assemblyForce.zero();
    assemblyForce.setCD(1.0);
    assemblyForce.setFrictionCD(0.5);
    assemblyForces.put(nullptr, assemblyForce);

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, nullptr, &assemblyForces, total, warnings);

    // Assembly forces should be zeroed
    const AerodynamicForces& zeroed = *assemblyForces.get(nullptr);
    EXPECT_NEAR(0, zeroed.getCD(), kEpsilon);
    EXPECT_NEAR(0, zeroed.getFrictionCD(), kEpsilon);
    EXPECT_NEAR(0, zeroed.getCDaxial(), kEpsilon);
}

TEST(LookupTableDragCalculator, HandlesNullComponentForces)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, nullptr, nullptr, total, warnings);

    EXPECT_NEAR(0.50, total.getCD(), kEpsilon);
}

TEST(LookupTableDragCalculator, NewInstance)
{
    const TempDir                   dir;
    const LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    const std::unique_ptr<DragCalculator> newInstance = calculator.newInstance();
    ASSERT_NE(newInstance, nullptr);
    EXPECT_NE(newInstance.get(), &calculator);
    const auto* sameType = dynamic_cast<const LookupTableDragCalculator*>(newInstance.get());
    ASSERT_NE(sameType, nullptr);
    EXPECT_EQ(&sameType->getTable(), &calculator.getTable());  // the table is shared
}

TEST(LookupTableDragCalculator, VoidAerodynamicCache)
{
    const TempDir             dir;
    LookupTableDragCalculator calculator = calculatorFor(dir, "mach,cd\n0,0.50\n1,0.50\n");

    // Should not throw
    EXPECT_NO_THROW(calculator.voidAerodynamicCache());
}

// ---- Beyond the JUnit tests (values pinned with OpenRocket on JDK 17) ----

TEST(LookupTableDragCalculator, ToAxialDragIsJavas)
{
    // LookupTableDragCalculator.toAxialDrag(conditions, 0.5) for setAOA(Math.toRadians(deg)).
    struct Case
    {
        double aoa;
        double axial;
    };
    constexpr std::array<Case, 19>  kCases{{
        {.aoa = 0.0, .axial = 0.5},
        {.aoa = 0.017453292519943295, .axial = 0.50149603093832680},
        {.aoa = 0.087266462599716470, .axial = 0.53129452473030740},
        {.aoa = 0.17453292519943295, .axial = 0.59464685528190510},
        {.aoa = 0.29496064358704166, .axial = 0.64998449012823120},
        {.aoa = 0.29670597283903605, .axial = 0.64999999999999990},
        {.aoa = 0.34906585039886590, .axial = 0.64376874609945030},
        {.aoa = 0.52359877559829880, .axial = 0.55372443666493280},
        {.aoa = 0.78539816339744830, .axial = 0.32746064272079356},
        {.aoa = 1.0471975511965976, .axial = 0.12483519665883502},
        {.aoa = 1.5533430342749532, .axial = 6.6148463204740440e-06},
        {.aoa = 1.5707963267948966, .axial = 0.0},
        {.aoa = 1.5882496193148399, .axial = -6.6148463204740440e-06},
        {.aoa = 2.0943951023931953, .axial = -0.12483519665883436},
        {.aoa = 2.8448866807507573, .axial = -0.64999999999999990},
        {.aoa = 3.1241393610698500, .axial = -0.50149603093832680},
        {.aoa = std::numbers::pi, .axial = -0.5},
        {.aoa = -0.17453292519943295, .axial = 0.5},
        {.aoa = 3.4906585039886590, .axial = -0.5},
    }};
    const LookupTableDragCalculator calculator{
        *MachAoALookup::dragBuilder().addDragData(0, 0.5).addDragData(1, 0.5).build()};
    for (const Case& c : kCases)
    {
        FlightConditions conditions;
        conditions.setAOA(c.aoa);
        // The polynomials come from PolyInterpolator's arithmetic only.
        EXPECT_NEAR(calculator.toAxialDrag(conditions, 0.5), c.axial, 1e-15) << "aoa " << c.aoa;
    }
}

TEST(LookupTableDragCalculator, CalculateDragFillsTheTotalAndZeroesTheMaps)
{
    LookupTableDragCalculator calculator{*MachAoALookup::dragBuilder()
                                              .addDragData(0.0, 0.0, 0.3)
                                              .addDragData(0.0, 20.0, 0.7)
                                              .addDragData(2.0, 0.0, 0.5)
                                              .addDragData(2.0, 20.0, 0.9)
                                              .build()};
    FlightConditions          conditions;
    conditions.setMach(1.0);
    conditions.setAOA(MathUtil::javaToRadians(10));

    ForceMap          componentForces;
    ForceMap          assemblyForces;
    AerodynamicForces untouched;  // NaN everywhere
    componentForces.put(nullptr, untouched);
    assemblyForces.put(nullptr, untouched);

    const Configuration c;
    AerodynamicForces   total;
    total.zero();
    total.setPressureCD(1);
    total.setBaseCD(1);
    total.setOverrideCD(1);
    total.setCN(0.3);
    WarningSet warnings;
    calculator.calculateDrag(c.config, conditions, &componentForces, &assemblyForces, total,
                             warnings);

    EXPECT_NEAR(total.getCD(), 0.6, 1e-15);
    EXPECT_EQ(total.getFrictionCD(), total.getCD());
    EXPECT_EQ(total.getPressureCD(), 0);
    EXPECT_EQ(total.getBaseCD(), 0);
    EXPECT_EQ(total.getOverrideCD(), 0);
    EXPECT_EQ(total.getCDaxial(), calculator.toAxialDrag(conditions, total.getCD()));
    EXPECT_EQ(total.getCN(), 0.3);  // the drag calculator leaves the rest alone
    EXPECT_TRUE(warnings.empty());

    for (const ForceMap* map : {&componentForces, &assemblyForces})
    {
        const AerodynamicForces& forces = *map->get(nullptr);
        EXPECT_EQ(forces.getFrictionCD(), 0);
        EXPECT_EQ(forces.getPressureCD(), 0);
        EXPECT_EQ(forces.getBaseCD(), 0);
        EXPECT_EQ(forces.getOverrideCD(), 0);
        EXPECT_EQ(forces.getCD(), 0);
        EXPECT_EQ(forces.getCDaxial(), 0);
        EXPECT_TRUE(std::isnan(forces.getCN()));
    }
}

TEST(LookupTableDragCalculator, NaNAngleOfAttackGivesNaNAxialDrag)
{
    const LookupTableDragCalculator calculator{
        *MachAoALookup::dragBuilder().addDragData(0, 0.5).build()};
    FlightConditions conditions;
    conditions.setAOA(kNaN);
    EXPECT_TRUE(std::isnan(calculator.toAxialDrag(conditions, 0.5)));
}

TEST(LookupTableDragCalculator, FromCsvReadsTheTableOrReportsTheFailure)
{
    const TempDir dir;
    const auto    calculator =
        LookupTableDragCalculator::fromCsv(dir.write("drag.csv", "mach,cd\n0,0.25\n2,0.75\n"));
    ASSERT_TRUE(calculator.has_value()) << calculator.error().toString();
    EXPECT_EQ(calculator->getTable().getValueColumns(), cdColumns());
    EXPECT_EQ(calculator->getTable().interpolate(1, 0, "cd"), 0.5);

    const auto missing = LookupTableDragCalculator::fromCsv(dir.resolve("missing.csv"));
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::IO);

    const auto noCd = LookupTableDragCalculator::fromCsv(dir.write("cn.csv", "mach,cn\n0,1\n"));
    ASSERT_FALSE(noCd.has_value());
    EXPECT_EQ(noCd.error().code, ErrorCode::PARSE);
    EXPECT_EQ(noCd.error().message,
              "Lookup table header missing required column 'cd'. Make sure the column is "
              "included and you are using the correct field separator.");
}

}  // namespace
