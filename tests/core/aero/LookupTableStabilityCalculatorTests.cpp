#include "QtRocket/aero/LookupTableStabilityCalculator.h"

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::ErrorCode;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::InnerTube;
using QtRocket::LookupTableStabilityCalculator;
using QtRocket::MachAoALookup;
using QtRocket::NoseCone;
using QtRocket::PodSet;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StabilityCalculator;
using QtRocket::StabilityForceBreakdown;
using QtRocket::TransitionShape;
using QtRocket::WarningSet;
using QtRocket::Test::TempDir;
namespace CsvMachAoALookup = QtRocket::CsvMachAoALookup;
namespace MathUtil         = QtRocket::MathUtil;

constexpr double kEpsilon = 1e-6;  // LookupTableStabilityCalculatorTest.EPSILON

/// The columns the stability calculator asks for (List.of("cn", "cm", "cp")).
[[nodiscard]] std::vector<std::string> stabilityColumns()
{
    return {"cn", "cm", "cp"};
}

/// The four-row table most of the Java tests write.
constexpr std::string_view kFourRows =
    "mach,aoa,cn,cm,cp\n0,0,0.10,0.01,0.50\n0,10,0.20,0.02,0.55\n1,0,0.30,0.03,0.60\n"
    "1,10,0.40,0.04,0.65\n";
/// The two-row table of the others.
constexpr std::string_view kTwoRows = "mach,aoa,cn,cm,cp\n0,0,0.10,0.01,0.50\n1,0,0.30,0.03,0.60\n";

/// The table in the CSV @p text, written to a file in @p dir.
[[nodiscard]] MachAoALookup tableFor(const TempDir& dir, std::string_view text)
{
    auto table = CsvMachAoALookup::fromCsv(dir.write("stability.csv", text), stabilityColumns());
    if (!table)
    {
        ADD_FAILURE() << table.error().toString();
        return *MachAoALookup::stabilityBuilder().addStabilityData(0, 0, 0, 0).build();
    }
    return std::move(*table);
}

/// A configuration of a new rocket (Java: new FlightConfiguration(new Rocket())).
struct Configuration
{
    Rocket              rocket;
    FlightConfiguration config{rocket};
};

// ---- Ported from LookupTableStabilityCalculatorTest.java ----

TEST(LookupTableStabilityCalculator, InterpolatesNonAxialCoefficients)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kFourRows)};

    const double mach = 0.5;
    const double aoa  = 5;

    FlightConditions conditions;
    conditions.setMach(mach);
    conditions.setAOA(MathUtil::javaToRadians(aoa));

    const Configuration     c;
    WarningSet              warnings;
    const AerodynamicForces forces =
        calculator.calculateNonAxialForces(c.config, conditions, warnings);

    EXPECT_NEAR(0.25, forces.getCN(), kEpsilon);
    EXPECT_NEAR(0.025, forces.getCm(), kEpsilon);
    EXPECT_NEAR(0.575, forces.getCP().x, kEpsilon);

    // max aoa in table is 10, so this should be stall angle
    EXPECT_NEAR(MathUtil::javaToRadians(10), calculator.getStallAngle(), kEpsilon);
}

TEST(LookupTableStabilityCalculator, GetCP)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kFourRows)};

    const Configuration c;
    FlightConditions    conditions{c.config};
    conditions.setMach(0.5);
    conditions.setAOA(MathUtil::javaToRadians(5));

    WarningSet       warnings;
    const Coordinate cp = calculator.getCP(c.config, conditions, warnings);
    EXPECT_NEAR(0.575, cp.x, kEpsilon);
    EXPECT_NEAR(0, cp.y, kEpsilon);
    EXPECT_NEAR(0, cp.z, kEpsilon);
}

TEST(LookupTableStabilityCalculator, GetStallAngleWithAoA)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kFourRows)};

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(MathUtil::javaToRadians(3));

    const Configuration c;
    WarningSet          warnings;
    (void)calculator.calculateNonAxialForces(c.config, conditions, warnings);

    // max aoa in table is 10, so this should be stall angle
    EXPECT_NEAR(MathUtil::javaToRadians(10), calculator.getStallAngle(), kEpsilon);
}

TEST(LookupTableStabilityCalculator, GetStallAngleWithoutAoA)
{
    const TempDir       dir;
    const MachAoALookup table =
        tableFor(dir, "mach,cn,cm,cp\n0,0.10,0.01,0.50\n1,0.30,0.03,0.60\n");
    EXPECT_FALSE(table.hasAoA());
    LookupTableStabilityCalculator calculator{table};

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(MathUtil::javaToRadians(5));

    const Configuration c;
    WarningSet          warnings;
    (void)calculator.calculateNonAxialForces(c.config, conditions, warnings);
    const double stall = calculator.getStallAngle();
    // Without AoA data in table, stall angle should be infinity
    EXPECT_EQ(std::numeric_limits<double>::infinity(), stall);
}

TEST(LookupTableStabilityCalculator, GetForceAnalysis)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kFourRows)};

    const Configuration c;
    FlightConditions    conditions{c.config};
    conditions.setMach(0.5);
    conditions.setAOA(MathUtil::javaToRadians(5));

    WarningSet                    warnings;
    const StabilityForceBreakdown breakdown =
        calculator.getForceAnalysis(c.config, conditions, warnings);

    // Total forces should be in assembly map for the rocket
    const AerodynamicForces* total = breakdown.getAssemblyForces().get(&c.rocket);
    ASSERT_NE(total, nullptr);
    EXPECT_NEAR(0.25, total->getCN(), kEpsilon);
    EXPECT_NEAR(0.025, total->getCm(), kEpsilon);
    EXPECT_NEAR(0.575, total->getCP().x, kEpsilon);
}

TEST(LookupTableStabilityCalculator, CalculateDampingMoments)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kTwoRows)};

    const Configuration c;  // Java passes null
    FlightConditions    conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    AerodynamicForces total;
    total.zero();
    total.setPitchDampingMoment(1.0);
    total.setYawDampingMoment(2.0);

    calculator.calculateDampingMoments(c.config, conditions, total);

    // Damping moments should be zeroed
    EXPECT_NEAR(0, total.getPitchDampingMoment(), kEpsilon);
    EXPECT_NEAR(0, total.getYawDampingMoment(), kEpsilon);
}

TEST(LookupTableStabilityCalculator, CheckGeometry)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kTwoRows)};

    const Configuration c;
    WarningSet          warnings;

    // Should not throw or add warnings
    calculator.checkGeometry(c.config, c.rocket, warnings);
    EXPECT_TRUE(warnings.empty());
}

TEST(LookupTableStabilityCalculator, NewInstance)
{
    const TempDir                        dir;
    const LookupTableStabilityCalculator calculator{tableFor(dir, kTwoRows)};

    const std::unique_ptr<StabilityCalculator> newInstance = calculator.newInstance();
    ASSERT_NE(newInstance, nullptr);
    EXPECT_NE(newInstance.get(), &calculator);
    const auto* sameType = dynamic_cast<const LookupTableStabilityCalculator*>(newInstance.get());
    ASSERT_NE(sameType, nullptr);
    EXPECT_EQ(&sameType->getTable(), &calculator.getTable());  // the table is shared
    EXPECT_EQ(sameType->getStallAngle(), calculator.getStallAngle());
}

TEST(LookupTableStabilityCalculator, SharesTheCallersTable)
{
    // Java's SimulationOptions passes its one MachAoALookup object to every calculator.
    const std::shared_ptr<const MachAoALookup> table =
        std::make_shared<const MachAoALookup>(*MachAoALookup::stabilityBuilder()
                                                   .addStabilityData(0, 0, 1, 2, 3)
                                                   .addStabilityData(0, 20, 2, 3, 4)
                                                   .build());
    const LookupTableStabilityCalculator first{table};
    const LookupTableStabilityCalculator second{table};
    EXPECT_EQ(&first.getTable(), table.get());
    EXPECT_EQ(&second.getTable(), table.get());
    EXPECT_EQ(first.getTableShared(), table);
    EXPECT_EQ(first.getStallAngle(), MathUtil::javaToRadians(20));

    const std::unique_ptr<StabilityCalculator> newInstance = first.newInstance();
    const auto* sameType = dynamic_cast<const LookupTableStabilityCalculator*>(newInstance.get());
    ASSERT_NE(sameType, nullptr);
    EXPECT_EQ(&sameType->getTable(), table.get());

    // The by-value constructor gives the calculator a table of its own.
    const LookupTableStabilityCalculator own{*table};
    EXPECT_NE(&own.getTable(), table.get());
    EXPECT_EQ(own.getStallAngle(), first.getStallAngle());
}

TEST(LookupTableStabilityCalculator, ANullTableIsABug)
{
    const std::shared_ptr<const MachAoALookup> none;
    EXPECT_THROW(LookupTableStabilityCalculator{none}, BugError);
}

TEST(LookupTableStabilityCalculator, VoidAerodynamicCache)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kTwoRows)};

    // Should not throw
    EXPECT_NO_THROW(calculator.voidAerodynamicCache());
}

TEST(LookupTableStabilityCalculator, SetsAllForceCoefficientsToZero)
{
    const TempDir                  dir;
    LookupTableStabilityCalculator calculator{tableFor(dir, kTwoRows)};

    FlightConditions conditions;
    conditions.setMach(0.5);
    conditions.setAOA(0);

    const Configuration     c;
    WarningSet              warnings;
    const AerodynamicForces forces =
        calculator.calculateNonAxialForces(c.config, conditions, warnings);

    // Side forces should be zero
    EXPECT_NEAR(0, forces.getCside(), kEpsilon);
    EXPECT_NEAR(0, forces.getCyaw(), kEpsilon);
    EXPECT_NEAR(0, forces.getCroll(), kEpsilon);
    EXPECT_NEAR(0, forces.getCrollDamp(), kEpsilon);
    EXPECT_NEAR(0, forces.getCrollForce(), kEpsilon);
}

// ---- Beyond the JUnit tests (values pinned with OpenRocket on JDK 17) ----

TEST(LookupTableStabilityCalculator, CoefficientsAndStallAngleAreJavas)
{
    LookupTableStabilityCalculator calculator{*MachAoALookup::stabilityBuilder()
                                                   .addStabilityData(0.0, 0.0, 0.10, 0.01, 0.50)
                                                   .addStabilityData(0.0, 12.5, 0.20, 0.02, 0.55)
                                                   .addStabilityData(1.0, 0.0, 0.30, 0.03, 0.60)
                                                   .addStabilityData(1.0, 12.5, 0.40, 0.04, 0.65)
                                                   .build()};
    EXPECT_EQ(calculator.getStallAngle(), 0.21816615649929120);  // Math.toRadians(12.5)

    FlightConditions conditions;
    conditions.setMach(0.37);
    conditions.setAOA(MathUtil::javaToRadians(7.3));
    const Configuration     c;
    WarningSet              warnings;
    const AerodynamicForces forces =
        calculator.calculateNonAxialForces(c.config, conditions, warnings);
    // Linear interpolation only (and Java's Math.toDegrees): the same doubles as Java.
    EXPECT_EQ(forces.getCN(), 0.23240000000000000);
    EXPECT_EQ(forces.getCm(), 0.023240000000000000);
    EXPECT_EQ(forces.getCP().x, 0.56620000000000000);
    EXPECT_EQ(forces.getCP().weight, 1.0);
    EXPECT_EQ(forces.toString(),
              "AerodynamicForces[cp:(0.56620,0.00000,0.00000,w=1.00000),CN:0.2324,Cm:0.02324,"
              "Cside:0.0,Cyaw:0.0,Croll:0.0,CDaxial:0.0,CD:0.0]");
    EXPECT_EQ(forces.getPitchDampingMoment(), 0);
    EXPECT_EQ(forces.getComponent(), nullptr);
    EXPECT_TRUE(std::isnan(forces.getPressureCD()));  // zero() leaves the drag parts
    EXPECT_EQ(calculator.getCP(c.config, conditions, warnings).x, 0.56620000000000000);
    EXPECT_TRUE(warnings.empty());
}

/// A rocket with a nose, a body holding a non-aerodynamic inner tube and a pod set with a body,
/// and the force analysis of a lookup calculator at Mach 1.
struct PodRocket
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    NoseCone&  nose = stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.02));
    BodyTube&  body = stage.addChild(std::make_unique<BodyTube>(0.5, 0.02));
    InnerTube& inner   = body.addChild(std::make_unique<InnerTube>());
    PodSet&    pods    = body.addChild(std::make_unique<PodSet>());
    BodyTube&  podBody = pods.addChild(std::make_unique<BodyTube>(0.2, 0.01));

    PodRocket()
    {
        // The inner tube is active but not aerodynamic: the analysis leaves it out.
        EXPECT_FALSE(inner.isAerodynamic());
        rocket.enableEvents();
    }

    /// The force analysis of a table with CN 0.2 ... 0.6, Cm 0.1 ... 0.3 and CP 0.4 ... 0.8 over
    /// Mach 0 ... 2, at Mach 1.
    [[nodiscard]] StabilityForceBreakdown analysis()
    {
        LookupTableStabilityCalculator calculator{*MachAoALookup::stabilityBuilder()
                                                       .addStabilityData(0.0, 0.2, 0.1, 0.4)
                                                       .addStabilityData(2.0, 0.6, 0.3, 0.8)
                                                       .build()};
        const FlightConfiguration      config{rocket};
        FlightConditions               conditions{config};
        conditions.setMach(1.0);
        WarningSet warnings;
        return calculator.getForceAnalysis(config, conditions, warnings);
    }
};

/// Expects @p forces to be the zeroed forces of @p component.
void expectZeroForces(const RocketComponent* component, const AerodynamicForces& forces)
{
    EXPECT_EQ(forces.getComponent(), component);
    EXPECT_EQ(forces.getCN(), 0);
    EXPECT_EQ(forces.getCm(), 0);
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(forces.getCrollForce(), 0);
}

TEST(LookupTableStabilityCalculator, ForceAnalysisZeroesTheActiveAerodynamicComponents)
{
    PodRocket                     r;
    const StabilityForceBreakdown breakdown = r.analysis();

    // The aerodynamic components, in the order of the active instances.
    const std::vector<const RocketComponent*> expected{&r.nose, &r.body, &r.podBody};
    EXPECT_EQ(breakdown.getComponentForces().keys(), expected);
    for (const auto& [component, forces] : breakdown.getComponentForces())
    {
        expectZeroForces(component, forces);
    }
}

TEST(LookupTableStabilityCalculator, ForceAnalysisPutsTheTotalOnTheRocket)
{
    PodRocket                     r;
    const StabilityForceBreakdown breakdown = r.analysis();

    // The assemblies, the rocket keeping its first place with the total.
    const std::vector<const RocketComponent*> expected{&r.rocket, &r.stage, &r.pods};
    EXPECT_EQ(breakdown.getAssemblyForces().keys(), expected);
    const AerodynamicForces& total = *breakdown.getAssemblyForces().get(&r.rocket);
    EXPECT_EQ(total.getComponent(), &r.rocket);
    EXPECT_NEAR(total.getCN(), 0.4, 1e-15);
    EXPECT_NEAR(total.getCm(), 0.2, 1e-15);
    EXPECT_EQ(total.getCP(), (Coordinate{0.6, 0, 0, 1}));
    expectZeroForces(&r.stage, *breakdown.getAssemblyForces().get(&r.stage));
    expectZeroForces(&r.pods, *breakdown.getAssemblyForces().get(&r.pods));
}

TEST(LookupTableStabilityCalculator, FromCsvReadsTheTableOrReportsTheFailure)
{
    const TempDir dir;
    const auto    calculator = LookupTableStabilityCalculator::fromCsv(
        dir.write("table.csv", "Mach,Angle of Attack,CN,CM,CP\n0,0,1,2,3\n0,20,2,3,4\n"));
    ASSERT_TRUE(calculator.has_value()) << calculator.error().toString();
    EXPECT_EQ(calculator->getStallAngle(), MathUtil::javaToRadians(20));
    EXPECT_EQ(calculator->getTable().getValueColumns(), stabilityColumns());

    const auto directory = LookupTableStabilityCalculator::fromCsv(dir.path());
    ASSERT_FALSE(directory.has_value());
    EXPECT_EQ(directory.error().code, ErrorCode::IO);
    EXPECT_EQ(directory.error().message,
              "Failed to read lookup table from " + QtRocket::pathToUtf8(dir.path()));

    const auto broken =
        LookupTableStabilityCalculator::fromCsv(dir.write("broken.csv", "mach,cn,cm\n0,1,2\n"));
    ASSERT_FALSE(broken.has_value());
    EXPECT_EQ(broken.error().code, ErrorCode::PARSE);
    EXPECT_EQ(broken.error().message,
              "Lookup table header missing required column 'cp'. Make sure the column is "
              "included and you are using the correct field separator.");
}

}  // namespace
