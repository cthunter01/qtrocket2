#include "QtRocket/aero/BarrowmanCalculator.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/BarrowmanStabilityCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/LookupTableDragCalculator.h"
#include "QtRocket/aero/LookupTableStabilityCalculator.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "aero/BarrowmanTestRockets.h"
#include "aero/ForcePins.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AerodynamicCalculator;
using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::BarrowmanCalculator;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::BarrowmanStabilityCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentAssembly;
using QtRocket::Coordinate;
using QtRocket::FinSet;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::ForceMap;
using QtRocket::LookupTableDragCalculator;
using QtRocket::LookupTableStabilityCalculator;
using QtRocket::MachAoALookup;
using QtRocket::ModId;
using QtRocket::NoseCone;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RailButton;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::compareDrag;
using QtRocket::Test::compareNonAxial;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::DragPin;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::kComponentTolerance;
using QtRocket::Test::kRocketTolerance;
using QtRocket::Test::NonAxialPin;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEndPlateRocket;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestEstesAlphaIIIWithInlinePod;
using QtRocket::Test::TestEstesAlphaIIIWithPods;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestMultiStageEventTestRocket;
using QtRocket::Test::TestStepsRocket;

constexpr double kPi = std::numbers::pi;

/// BarrowmanCalculatorTest.EPSILON
constexpr double kEpsilon = 0.00001;

using Areas = std::vector<FlightConditions::NozzleExitArea>;

// ============================================== ported from BarrowmanCalculatorTest.java
//
// The Java tests reach the components through rocket.getChild(...) chains; the same chains are
// used here, cast with dynamic_cast (a reference cast throws std::bad_cast where Java throws a
// ClassCastException). JUnit's assertEquals(expected, actual, delta) is EXPECT_NEAR in the same
// argument order.

/// The rocket and every component below it, in tree order (Java: `for (RocketComponent comp :
/// rocket)`).
[[nodiscard]] std::vector<const RocketComponent*> allComponents(const Rocket& rocket)
{
    std::vector<const RocketComponent*> components = rocket.getAllChildren();
    components.insert(components.begin(), &rocket);
    return components;
}

/// Java's `comp.isAerodynamic() || comp instanceof ComponentAssembly`.
[[nodiscard]] bool isAerodynamicOrAssembly(const RocketComponent& comp)
{
    return comp.isAerodynamic() || QtRocket::isAssembly(comp.kind());
}

/// testForceMapContent: the force map holds every aerodynamic component and assembly (the two
/// assertions of the loop, with their messages).
void expectHoldsEveryAerodynamicComponent(const Rocket& rocket, const ForceMap& forceMap,
                                          const std::string& missingMessage,
                                          const std::string& nullMessage)
{
    for (const RocketComponent* const comp : allComponents(rocket))
    {
        if (isAerodynamicOrAssembly(*comp))
        {
            EXPECT_TRUE(forceMap.containsKey(comp)) << missingMessage << comp->getName();
            EXPECT_NE(forceMap.get(comp), nullptr) << nullMessage << comp->getName();
        }
    }
}

/// testForceMapContent, the first branch of the loop over the components: the force map holds
/// no component of an inactive stage.
void expectHoldsNoInactiveComponent(const Rocket& rocket, const FlightConfiguration& config,
                                    const ForceMap& forceMap)
{
    for (const RocketComponent* const comp : allComponents(rocket))
    {
        if (!config.isComponentActive(*comp))
        {
            EXPECT_FALSE(forceMap.containsKey(comp))
                << "Force map contains inactive component: " << comp->getName();
        }
    }
}

/// testForceMapContent, the second branch of that loop: the force map holds every active
/// aerodynamic component and assembly.
void expectHoldsEveryActiveAerodynamicComponent(const Rocket&              rocket,
                                                const FlightConfiguration& config,
                                                const ForceMap&            forceMap)
{
    for (const RocketComponent* const comp : allComponents(rocket))
    {
        if (config.isComponentActive(*comp) && isAerodynamicOrAssembly(*comp))
        {
            EXPECT_TRUE(forceMap.containsKey(comp))
                << "Force map missing active component: " << comp->getName();
            EXPECT_NE(forceMap.get(comp), nullptr)
                << "Force entry is null for active component: " << comp->getName();
        }
    }
}

/// testForceMapContent: the force map holds the components of the active stages only.
void expectHoldsActiveComponentsOnly(const Rocket& rocket, const FlightConfiguration& config,
                                     const ForceMap& forceMap)
{
    expectHoldsNoInactiveComponent(rocket, config, forceMap);
    expectHoldsEveryActiveAerodynamicComponent(rocket, config, forceMap);
}

/// The sums testStageDragAggregatesAerodynamicDescendants expects of a stage.
struct ExpectedStageDrag
{
    double pressureCD{0};
    double baseCD{0};
    double frictionCD{0};
    double totalCD{0};
};

/// testStageDragAggregatesAerodynamicDescendants: the drag of the aerodynamic descendants of
/// @p stage in @p forceMap, each times its number of active instances.
[[nodiscard]] ExpectedStageDrag expectedStageDrag(const AxialStage& stage, const ForceMap& forceMap,
                                                  const FlightConfiguration& config)
{
    ExpectedStageDrag expected;
    for (const auto& [component, forces] : forceMap)
    {
        if (component->isAerodynamic() && stage.isAncestor(*component))
        {
            const int instanceCount = config.getActiveInstances().count(*component);
            expected.pressureCD += forces.getPressureCD() * instanceCount;
            expected.baseCD += forces.getBaseCD() * instanceCount;
            expected.frictionCD += forces.getFrictionCD() * instanceCount;
            expected.totalCD += forces.getCD() * instanceCount;
        }
    }
    return expected;
}

/// testStageDragAggregatesAerodynamicDescendants: the assertions on one stage.
void expectStageDragOfDescendants(const AxialStage& stage, const ForceMap& forceMap,
                                  const FlightConfiguration& config)
{
    const AerodynamicForces* const stageForces = forceMap.get(&stage);
    const ExpectedStageDrag        expected    = expectedStageDrag(stage, forceMap, config);

    ASSERT_NE(stageForces, nullptr) << "Force map missing stage: " << stage.getName();
    EXPECT_TRUE(expected.totalCD > 0) << "Stage has no aerodynamic drag: " << stage.getName();
    const int stageInstanceCount = config.getActiveInstances().count(stage);
    EXPECT_NEAR(expected.pressureCD / stageInstanceCount, stageForces->getPressureCD(), kEpsilon);
    EXPECT_NEAR(expected.baseCD / stageInstanceCount, stageForces->getBaseCD(), kEpsilon);
    EXPECT_NEAR(expected.frictionCD / stageInstanceCount, stageForces->getFrictionCD(), kEpsilon);
    EXPECT_NEAR(expected.totalCD, stageForces->getCDTotal(), kEpsilon)
        << "Stage drag does not equal its aerodynamic descendants: " << stage.getName();
}

/// testStageDragRespectsAssemblyOverrides: the summed getCDTotal() of the aerodynamic
/// descendants of @p stage in @p forceMap.
[[nodiscard]] double descendantCDTotal(const AxialStage& stage, const ForceMap& forceMap)
{
    double descendantCD = 0;
    for (const auto& [component, forces] : forceMap)
    {
        if (component->isAerodynamic() && stage.isAncestor(*component))
        {
            descendantCD += forces.getCDTotal();
        }
    }
    return descendantCD;
}

/// The forces of @p component in @p forceMap (Java: forceMap.get(component), whose use throws a
/// NullPointerException when the component is missing; a test failure here).
[[nodiscard]] const AerodynamicForces& forcesOf(const ForceMap&        forceMap,
                                                const RocketComponent& component)
{
    const AerodynamicForces* const forces = forceMap.get(&component);
    if (forces == nullptr)
    {
        ADD_FAILURE() << "the force map has no entry for " << component.getName();
        static const AerodynamicForces kNone;
        return kNone;
    }
    return *forces;
}

// Java: testEmptyRocket
/// Test a completely empty rocket.
TEST(BarrowmanCalculatorTest, EmptyRocket)
{
    // First test completely empty rocket
    Rocket                     rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calc;
    const FlightConditions     conditions{config};
    WarningSet                 warnings;

    const Coordinate cpCalc = calc.getCP(config, conditions, &warnings);

    EXPECT_EQ(0.0, cpCalc.weight) << " Empty rocket CNa value is incorrect:";
    EXPECT_EQ(0.0, cpCalc.x) << " Empty rocket cp x value is incorrect:";
    EXPECT_EQ(0.0, cpCalc.y) << " Empty rocket cp y value is incorrect:";
    EXPECT_EQ(0.0, cpCalc.z) << " Empty rocket cp z value is incorrect:";
}

// Java: testCPSimpleDry
TEST(BarrowmanCalculatorTest, CPSimpleDry)
{
    const TestEstesAlphaIII    alpha;
    const Rocket&              rocket = *alpha.rocket;
    const auto&                stage  = dynamic_cast<const AxialStage&>(rocket.getChild(0));
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calc;
    const FlightConditions     conditions{config};
    WarningSet                 warnings;

    // By Hand: i.e. Manually calculate the Barrowman numbers
    const auto& nose = dynamic_cast<const NoseCone&>(stage.getChild(0));
    EXPECT_NEAR(0.07, nose.getLength(), kEpsilon)
        << " Estes Alpha III nose cone has incorrect length:";
    EXPECT_NEAR(0.012, nose.getAftRadius(), kEpsilon)
        << " Estes Alpha III nosecone has wrong (base) radius:";
    EXPECT_EQ(TransitionShape::OGIVE, nose.getShapeType())
        << " Estes Alpha III nosecone has wrong type:";
    const double cnaNose = 2;
    const double cpxNose = 0.03235;

    const double cnaBody = 0;  // equal-to-zero, see [Barrowman66] p15.
    const double cpxBody = 0;

    const double cna3fin = 28.82053382;
    double       cpx3fin = 0.018524171835048092;
    const double finX    = 0.22;
    cpx3fin += finX;

    const double cnaLugs = 0;  // n/a
    const double cpxLugs = 0;  // n/a

    // N.B. CP @ AoA = zero
    const double expCna = cnaNose + cnaBody + cna3fin + cnaLugs;
    const double expCpx =
        ((cnaNose * cpxNose) + (cnaBody * cpxBody) + (cna3fin * cpx3fin) + (cnaLugs * cpxLugs)) /
        expCna;

    const Coordinate cpCalc = calc.getCP(config, conditions, &warnings);

    EXPECT_NEAR(expCna, cpCalc.weight, kEpsilon) << " Estes Alpha III CNa value is incorrect:";
    EXPECT_NEAR(expCpx, cpCalc.x, kEpsilon) << " Estes Alpha III cp x value is incorrect:";
    EXPECT_NEAR(0.0, cpCalc.y, kEpsilon) << " Estes Alpha III cp y value is incorrect:";
}

// Java: testCPSimpleWithMotor
TEST(BarrowmanCalculatorTest, CPSimpleWithMotor)
{
    const TestEstesAlphaIII                      alpha;
    const FlightConfiguration&                   config = alpha.rocket->getSelectedConfiguration();
    const std::unique_ptr<AerodynamicCalculator> calc   = std::make_unique<BarrowmanCalculator>();
    const FlightConditions                       conditions{config};
    WarningSet                                   warnings;

    // calculated from OpenRocket 15.03:
    // double expCPx = 0.225;
    // verified from the equations:
    const double     expCPx = 0.22514554750367705;
    const double     expCna = 30.82053382;
    const Coordinate calcCP = calc->getCP(config, conditions, &warnings);

    EXPECT_NEAR(expCPx, calcCP.x, kEpsilon) << " Estes Alpha III cp x value is incorrect:";
    EXPECT_NEAR(expCna, calcCP.weight, kEpsilon) << " Estes Alpha III CNa value is incorrect:";
}

// Java: testCPParallelBoosters
// Component CP calculations resulting in expected test values are in comments in
// TestRockets.makeFalcon9Heavy()
TEST(BarrowmanCalculatorTest, CPParallelBoosters)
{
    const TestFalcon9Heavy falcon;
    const Rocket&          rocket = *falcon.rocket;
    auto&                  boosterStage =
        dynamic_cast<ParallelStage&>(falcon.rocket->getChild(1).getChild(0).getChild(0));
    auto& boosterFins = dynamic_cast<TrapezoidFinSet&>(boosterStage.getChild(1).getChild(1));
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calc;
    const FlightConditions     conditions{config};
    WarningSet                 warnings;

    {
        boosterFins.setFinCount(3);
        const Coordinate cp3fin = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(20.19347236, cp3fin.weight, kEpsilon)
            << " Falcon 9 Heavy CNa value is incorrect:";
        EXPECT_NEAR(1.0310997664583446, cp3fin.x, kEpsilon)
            << " Falcon 9 Heavy CP x value is incorrect:";
        EXPECT_NEAR(0.0, cp3fin.y, kEpsilon) << " Falcon 9 Heavy CP y value is incorrect:";
        EXPECT_NEAR(0.0, cp3fin.z, kEpsilon) << " Falcon 9 Heavy CP z value is incorrect:";
    }
    {
        boosterFins.setFinCount(2);
        boosterFins.setAngleOffset(kPi / 4);
        const Coordinate cp2fin = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(14.55865366, cp2fin.weight, kEpsilon)
            << " Falcon 9 Heavy CNa value is incorrect:";
        EXPECT_NEAR(0.9771511413520412, cp2fin.x, kEpsilon)
            << " Falcon 9 Heavy CP x value is incorrect:";
        EXPECT_NEAR(0.0, cp2fin.y, kEpsilon) << " Falcon 9 Heavy CP y value is incorrect:";
        EXPECT_NEAR(0.0, cp2fin.z, kEpsilon) << " Falcon 9 Heavy CP z value is incorrect:";
    }
    {
        boosterFins.setFinCount(1);
        const Coordinate cp1fin = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(8.92383497, cp1fin.weight, kEpsilon)
            << " Falcon 9 Heavy CNa value is incorrect:";
        EXPECT_NEAR(0.8550724527785717, cp1fin.x, kEpsilon)
            << " Falcon 9 Heavy CP x value is incorrect:";
        EXPECT_NEAR(0.0, cp1fin.y, kEpsilon) << " Falcon 9 Heavy CP y value is incorrect:";
        EXPECT_NEAR(0.0, cp1fin.z, kEpsilon) << " Falcon 9 Heavy CP z value is incorrect:";
    }
}

// Java: testFinCountEffect
TEST(BarrowmanCalculatorTest, FinCountEffect)
{
    BarrowmanCalculator calc;
    WarningSet          warnings;

    const TestEstesAlphaIII    alpha;
    Rocket&                    rocket = *alpha.rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    const FlightConditions     conditions{config};
    auto& fins = dynamic_cast<FinSet&>(rocket.getChild(0).getChild(1).getChild(0));
    {
        fins.setFinCount(4);
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(40.42737843, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.2283247440074719, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
    {
        fins.setFinCount(3);
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(30.82053382, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.22514554750367705, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
    {
        fins.setFinCount(2);
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(2.0, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.032356, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
    {
        fins.setFinCount(1);
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(2.0, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.032356, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
}

// Java: testCpSplitTripleFin
TEST(BarrowmanCalculatorTest, CpSplitTripleFin)
{
    BarrowmanCalculator calc;
    WarningSet          warnings;

    const TestEstesAlphaIII    alpha;
    Rocket&                    rocket = *alpha.rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    const FlightConditions     conditions{config};

    {
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(30.82053382, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.22514554750367705, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
    {
        auto& body = dynamic_cast<BodyTube&>(rocket.getChild(0).getChild(1));
        auto& fins = dynamic_cast<TrapezoidFinSet&>(body.getChild(0));
        fins.setAngleOffset(0);
        // Java's caller keeps its reference to the fin set that splitRocketFins() takes out.
        const std::unique_ptr<TrapezoidFinSet> removed =
            QtRocket::Test::splitRocketFins(body, fins, 3);

        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(30.82053382, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.22514554750367705, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
}

// Java: testCpSplitQuadrupleFin
TEST(BarrowmanCalculatorTest, CpSplitQuadrupleFin)
{
    BarrowmanCalculator calc;
    WarningSet          warnings;

    const TestEstesAlphaIII    alpha;
    Rocket&                    rocket = *alpha.rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    const FlightConditions     conditions{config};

    {
        dynamic_cast<FinSet&>(rocket.getChild(0).getChild(1).getChild(0)).setFinCount(4);
        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(40.42737843, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.2283247440074719, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
    {
        auto& body = dynamic_cast<BodyTube&>(rocket.getChild(0).getChild(1));
        auto& fins = dynamic_cast<TrapezoidFinSet&>(body.getChild(0));
        const std::unique_ptr<TrapezoidFinSet> removed =
            QtRocket::Test::splitRocketFins(body, fins, 4);

        const Coordinate wholeRocketCP = calc.getCP(config, conditions, &warnings);
        EXPECT_NEAR(40.42737843, wholeRocketCP.weight, kEpsilon)
            << "Split-Fin Rocket CNa value is incorrect:";
        EXPECT_NEAR(0.2283247440074719, wholeRocketCP.x, kEpsilon)
            << "Split-Fin Rocket CP x value is incorrect:";
    }
}

// Java: testEndPlateCP
// test rocket with endplates on fins. Comments tracing calculation of CP are in
// TestRockets.makeEndPlateRocket().
TEST(BarrowmanCalculatorTest, EndPlateCP)
{
    const TestEndPlateRocket endPlate;
    // Java: new FlightConfiguration(rocket, null), a configuration with a new random id
    const FlightConfiguration                    config{*endPlate.rocket, FlightConfigurationId{}};
    const std::unique_ptr<AerodynamicCalculator> calc = std::make_unique<BarrowmanCalculator>();
    const FlightConditions                       conditions{config};
    WarningSet                                   warnings;

    const Coordinate cp = calc->getCP(config, conditions, &warnings);
    EXPECT_NEAR(0.25527062477601287, cp.x, kEpsilon) << " Endplate rocket cp x value is incorrect:";
    EXPECT_NEAR(0.0, cp.y, kEpsilon) << " Endplate rocket cp y value is incorrect:";
    EXPECT_NEAR(0.0, cp.z, kEpsilon) << " Endplate rocket cp z value is incorrect:";
    EXPECT_NEAR(45.32530223, cp.weight, kEpsilon) << " Endplate rocket CNa value is incorrect:";
}

// Java: testContinuousRocket
TEST(BarrowmanCalculatorTest, ContinuousRocket)
{
    const DefaultUnitsGuard                      guard;  // checkGeometry() reads the default unit
    const TestEstesAlphaIII                      alpha;
    const Rocket&                                rocket = *alpha.rocket;
    const std::unique_ptr<AerodynamicCalculator> calc   = std::make_unique<BarrowmanCalculator>();
    const FlightConfiguration&                   configuration = rocket.getSelectedConfiguration();
    WarningSet                                   warnings;

    calc->checkGeometry(configuration, rocket, &warnings);
    EXPECT_TRUE(warnings.empty()) << "Estes Alpha III should be continuous: ";
}

// Java: testContinuousRocketWithStrapOns
TEST(BarrowmanCalculatorTest, ContinuousRocketWithStrapOns)
{
    const DefaultUnitsGuard                      guard;
    const TestFalcon9Heavy                       falcon;
    const Rocket&                                rocket = *falcon.rocket;
    const std::unique_ptr<AerodynamicCalculator> calc   = std::make_unique<BarrowmanCalculator>();
    const FlightConfiguration&                   configuration = rocket.getSelectedConfiguration();
    WarningSet                                   warnings;

    calc->checkGeometry(configuration, rocket, &warnings);
    EXPECT_TRUE(warnings.empty()) << "F9H should be continuous: ";
}

// Java: testRadialDiscontinuousRocket
TEST(BarrowmanCalculatorTest, RadialDiscontinuousRocket)
{
    const DefaultUnitsGuard                      guard;
    const TestEstesAlphaIII                      alpha;
    Rocket&                                      rocket = *alpha.rocket;
    const std::unique_ptr<AerodynamicCalculator> calc   = std::make_unique<BarrowmanCalculator>();
    const FlightConfiguration&                   configuration = rocket.getSelectedConfiguration();
    WarningSet                                   warnings;

    auto& nose = dynamic_cast<NoseCone&>(rocket.getChild(0).getChild(0));
    auto& body = dynamic_cast<BodyTube&>(rocket.getChild(0).getChild(1));

    nose.setAftRadius(0.015);
    body.setOuterRadius(0.012);
    body.setName(body.getName() + "  << discontinuous");

    calc->checkGeometry(configuration, rocket, &warnings);
    EXPECT_FALSE(warnings.empty()) << " Estes Alpha III has an undetected discontinuity:";
}

// Java: testRadialDiscontinuityWithStrapOns
TEST(BarrowmanCalculatorTest, RadialDiscontinuityWithStrapOns)
{
    const DefaultUnitsGuard                      guard;
    const TestFalcon9Heavy                       falcon;
    Rocket&                                      rocket = *falcon.rocket;
    const std::unique_ptr<AerodynamicCalculator> calc   = std::make_unique<BarrowmanCalculator>();
    const FlightConfiguration&                   configuration = rocket.getSelectedConfiguration();
    WarningSet                                   warnings;

    auto& coreStage = dynamic_cast<AxialStage&>(rocket.getChild(1));
    auto& booster   = dynamic_cast<ParallelStage&>(coreStage.getChild(0).getChild(0));

    auto& nose = dynamic_cast<NoseCone&>(booster.getChild(0));
    auto& body = dynamic_cast<BodyTube&>(booster.getChild(1));

    nose.setAftRadius(0.015);
    body.setOuterRadius(0.012);
    body.setName(body.getName() + "  << discontinuous");

    calc->checkGeometry(configuration, rocket, &warnings);
    EXPECT_FALSE(warnings.empty()) << " Missed discontinuity in Falcon 9 Heavy:";
}

// Java: testPhantomTubes
TEST(BarrowmanCalculatorTest, PhantomTubes)
{
    const TestEstesAlphaIII    rocketNoPods;
    const FlightConfiguration& configNoPods = rocketNoPods.rocket->getSelectedConfiguration();
    const FlightConditions     conditionsNoPods{configNoPods};
    WarningSet                 warningsNoPods;

    const TestEstesAlphaIIIWithPods rocketWithPods;
    const FlightConfiguration&      configPods = rocketWithPods.rocket->getSelectedConfiguration();
    const FlightConditions          conditionsPods{configPods};
    WarningSet                      warningsPods;
    const std::unique_ptr<AerodynamicCalculator> calcPods = std::make_unique<BarrowmanCalculator>();
    const std::unique_ptr<AerodynamicCalculator> calcNoPods =
        std::make_unique<BarrowmanCalculator>();

    // As in Java, the calculator of the pods is used for both rockets here.
    const AerodynamicForces forcesNoPods =
        calcPods->getAerodynamicForces(configNoPods, conditionsNoPods, &warningsNoPods);
    const AerodynamicForces forcesPods =
        calcPods->getAerodynamicForces(configPods, conditionsPods, &warningsPods);
    EXPECT_NEAR(forcesPods.getCD(), forcesNoPods.getCD(), kEpsilon)
        << " Estes Alpha III With Pods rocket CD value is incorrect:";

    // The "with pods" version has no way of seeing the fins are on the actual body tube rather
    // than the phantom tubes, so its fin normal force won't include body interference. The
    // deltas below come from comparing the same design with the interference calculation in
    // FinSetCalc disabled and enabled.

    const Coordinate cpNoPods = calcNoPods->getCP(configNoPods, conditionsNoPods, &warningsNoPods);
    const Coordinate cpPods   = calcPods->getCP(configPods, conditionsPods, &warningsPods);
    EXPECT_NEAR(cpNoPods.x - 0.0044189530999860505, cpPods.x, kEpsilon)
        << " Alpha III With Pods rocket cp x value is incorrect:";
    EXPECT_NEAR(cpNoPods.y, cpPods.y, kEpsilon)
        << " Alpha III With Pods rocket cp y value is incorrect:";
    EXPECT_NEAR(cpNoPods.z, cpPods.z, kEpsilon)
        << " Alpha III With Pods rocket cp z value is incorrect:";
    EXPECT_NEAR(cpPods.weight, cpNoPods.weight - 8.589319064, kEpsilon)
        << " Alpha III With Pods rocket CNa value is incorrect:";
}

// Java: testEmptyStages
/// Tests whether adding extra empty stages has an effect.
TEST(BarrowmanCalculatorTest, EmptyStages)
{
    // Reference rocket
    const TestEstesAlphaIII    rocketRef;
    const FlightConfiguration& configRef = rocketRef.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calcRef;
    const FlightConditions     conditionsRef{configRef};
    WarningSet                 warnings;

    const Coordinate cpCalcRef = calcRef.getCP(configRef, conditionsRef, &warnings);

    // First test with adding an empty stage in the front of the design
    const TestEstesAlphaIII rocketFront;
    // To be placed in front of the design
    rocketFront.rocket->addChild(std::make_unique<AxialStage>(), 0);
    const FlightConfiguration& configFront = rocketFront.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calcFront;
    const FlightConditions     conditionsFront{configFront};
    warnings = WarningSet{};

    const Coordinate cpCalcFront = calcFront.getCP(configFront, conditionsFront, &warnings);

    EXPECT_NEAR(cpCalcRef.weight, cpCalcFront.weight, kEpsilon)
        << " Estes Alpha III with front empty stage CNa value is incorrect:";
    EXPECT_NEAR(cpCalcRef.x, cpCalcFront.x, kEpsilon)
        << " Estes Alpha III with front empty stage cp x value is incorrect:";
    EXPECT_NEAR(cpCalcRef.y, cpCalcFront.y, kEpsilon)
        << " Estes Alpha III with front empty stage cp y value is incorrect:";
    EXPECT_NEAR(cpCalcRef.z, cpCalcFront.z, kEpsilon)
        << " Estes Alpha III with front empty stage cp z value is incorrect:";

    // Now test with adding an empty stage in the rear of the design
    const TestEstesAlphaIII rocketRear;
    // To be placed in the rear of the design
    rocketRear.rocket->addChild(std::make_unique<AxialStage>());
    const FlightConfiguration& configRear = rocketRear.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calcRear;
    const FlightConditions     conditionsRear{configRear};
    warnings = WarningSet{};

    const Coordinate cpCalcRear = calcRear.getCP(configRear, conditionsRear, &warnings);

    EXPECT_NEAR(cpCalcRef.weight, cpCalcRear.weight, kEpsilon)
        << " Estes Alpha III with rear empty stage CNa value is incorrect:";
    EXPECT_NEAR(cpCalcRef.x, cpCalcRear.x, kEpsilon)
        << " Estes Alpha III with rear empty stage cp x value is incorrect:";
    EXPECT_NEAR(cpCalcRef.y, cpCalcRear.y, kEpsilon)
        << " Estes Alpha III with rear empty stage cp y value is incorrect:";
    EXPECT_NEAR(cpCalcRef.z, cpCalcRear.z, kEpsilon)
        << " Estes Alpha III with rear empty stage cp z value is incorrect:";

    // Test with multiple empty stages
    Rocket& rocketMulti = *rocketFront.rocket;
    // To be placed in the rear of the design
    rocketMulti.addChild(std::make_unique<AxialStage>());
    const FlightConfiguration& configMulti = rocketMulti.getSelectedConfiguration();
    BarrowmanCalculator        calcMulti;
    const FlightConditions     conditionsMulti{configMulti};
    warnings = WarningSet{};

    const Coordinate cpCalcMulti = calcMulti.getCP(configMulti, conditionsMulti, &warnings);

    EXPECT_NEAR(cpCalcRef.weight, cpCalcMulti.weight, kEpsilon)
        << " Estes Alpha III with multiple empty stages CNa value is incorrect:";
    EXPECT_NEAR(cpCalcRef.x, cpCalcMulti.x, kEpsilon)
        << " Estes Alpha III with multiple empty stages cp x value is incorrect:";
    EXPECT_NEAR(cpCalcRef.y, cpCalcMulti.y, kEpsilon)
        << " Estes Alpha III with multiple empty stages cp y value is incorrect:";
    EXPECT_NEAR(cpCalcRef.z, cpCalcMulti.z, kEpsilon)
        << " Estes Alpha III with multiple empty stages cp z value is incorrect:";
}

// Java: testInlinePods
/// Tests in-line pod aerodynamics and warnings
TEST(BarrowmanCalculatorTest, InlinePods)
{
    const DefaultUnitsGuard guard;
    WarningSet              warnings;

    // reference rocket and results
    const TestEstesAlphaIII    refRocket;
    const FlightConfiguration& refConfig = refRocket.rocket->getSelectedConfiguration();
    const FlightConditions     refConditions{refConfig};

    BarrowmanCalculator     refCalc;
    const double            refCP = refCalc.getCP(refConfig, refConditions, &warnings).x;
    const AerodynamicForces refForces =
        refCalc.getAerodynamicForces(refConfig, refConditions, &warnings);
    EXPECT_TRUE(warnings.empty()) << "reference rocket should have no warnings";
    const double refCD = refForces.getCD();

    // test rocket
    const TestEstesAlphaIIIWithInlinePod test;
    Rocket&                              testRocket = *test.rocket;
    auto& pod = dynamic_cast<PodSet&>(testRocket.getChild(0).getChild(1).getChild(0));
    const FlightConfiguration& testConfig = testRocket.getSelectedConfiguration();
    const FlightConditions     testConditions{testConfig};

    BarrowmanCalculator     testCalc;
    const double            testCP = testCalc.getCP(testConfig, testConditions, &warnings).x;
    const AerodynamicForces testForces =
        testCalc.getAerodynamicForces(testConfig, testConditions, &warnings);
    EXPECT_TRUE(warnings.empty()) << "test rocket should have no warnings";

    EXPECT_NEAR(refCP, testCP, kEpsilon) << "ref and test rocket CP should match";

    const double testCD = testForces.getCD();
    EXPECT_NEAR(refCD, testCD, kEpsilon) << "ref and test rocket CD should match";

    // move the pod back.
    pod.setAxialOffset(pod.getAxialOffset() + 0.1);
    testCalc.checkGeometry(testConfig, testRocket, &warnings);
    EXPECT_EQ(1U, warnings.size()) << "should be warning from gap in airframe";

    // move the pod forward.
    warnings.clear();
    pod.setAxialOffset(pod.getAxialOffset() - 0.3);
    testCalc.checkGeometry(testConfig, testRocket, &warnings);
    EXPECT_EQ(1U, warnings.size()) << "should be warning from airframe overlap";

    // move the pod back.
    warnings.clear();
    pod.setAxialOffset(pod.getAxialOffset() + 0.1);
    testCalc.checkGeometry(testConfig, testRocket, &warnings);
    EXPECT_EQ(1U, warnings.size()) << "should be warning from podset airframe overlap";
}

// Java: testBaseDragWithOverride
TEST(BarrowmanCalculatorTest, BaseDragWithOverride)
{
    WarningSet          warnings;
    BarrowmanCalculator calc;

    // get base drag of minimal rocket consisting of just a tube.
    Rocket      tubeRocket;
    AxialStage& tubeStage = tubeRocket.addChild(std::make_unique<AxialStage>());

    const BodyTube& tubeBodyTube = tubeStage.addChild(std::make_unique<BodyTube>());

    const FlightConfiguration tubeConfig{tubeRocket};
    const FlightConditions    tubeConditions{tubeConfig};
    const AerodynamicForces   tubeForces =
        calc.getAerodynamicForces(tubeConfig, tubeConditions, &warnings);
    const double tubeBaseCD = tubeForces.getBaseCD();

    // get base CD of minimal rocket consisting of just a cone
    Rocket      coneRocket;
    AxialStage& coneStage = coneRocket.addChild(std::make_unique<AxialStage>());

    auto coneConeOwned = std::make_unique<NoseCone>();
    coneConeOwned->setAftRadius(tubeBodyTube.getOuterRadius());
    const NoseCone& coneCone = coneStage.addChild(std::move(coneConeOwned));

    const FlightConfiguration coneConfig{coneRocket};
    const FlightConditions    coneConditions{coneConfig};
    const AerodynamicForces   coneForces =
        calc.getAerodynamicForces(coneConfig, coneConditions, &warnings);
    const double coneBaseCD = coneForces.getBaseCD();

    // now our test rocket, with a tube and a cone
    Rocket      testRocket;
    AxialStage& testStage = testRocket.addChild(std::make_unique<AxialStage>());

    auto testTubeOwned = std::make_unique<BodyTube>();
    testTubeOwned->setOuterRadius(tubeBodyTube.getOuterRadius());
    BodyTube& testTube = testStage.addChild(std::move(testTubeOwned));

    auto testConeOwned = std::make_unique<NoseCone>();
    testConeOwned->setAftRadius(coneCone.getAftRadius());
    NoseCone& testCone = testStage.addChild(std::move(testConeOwned));

    const FlightConfiguration testConfig{testRocket};
    const FlightConditions    testConditions{testConfig};

    // no overrides
    AerodynamicForces testForces = calc.getAerodynamicForces(testConfig, testConditions, &warnings);
    EXPECT_NEAR(tubeBaseCD + coneBaseCD, testForces.getBaseCD(), kEpsilon)
        << "base CD should be base CD of tube plus base CD of cone";

    // override tube CD
    testTube.setCDOverridden(true);
    testTube.setOverrideCD(0);
    testForces = calc.getAerodynamicForces(testConfig, testConditions, &warnings);
    EXPECT_NEAR(coneBaseCD, testForces.getBaseCD(), kEpsilon)
        << "base CD should be base CD of cone";

    // override cone CD
    testCone.setCDOverridden(true);
    testCone.setOverrideCD(0);
    testForces = calc.getAerodynamicForces(testConfig, testConditions, &warnings);
    EXPECT_NEAR(0.0, testForces.getBaseCD(), kEpsilon) << "base CD should be 0";

    // and turn off tube override
    testTube.setCDOverridden(false);
    testForces = calc.getAerodynamicForces(testConfig, testConditions, &warnings);
    EXPECT_NEAR(tubeBaseCD, testForces.getBaseCD(), kEpsilon)
        << "base CD should be base CD of tube";
}

// Java: testPoweredBaseDragSubtractsThrustingNozzleExitArea
/// Verify that motor exhaust excludes only its nozzle exit area from body-base drag. Fin
/// trailing-edge base drag is a separate contribution and must remain unchanged.
TEST(BarrowmanCalculatorTest, PoweredBaseDragSubtractsThrustingNozzleExitArea)
{
    const TestEstesAlphaIII alpha;
    const Rocket&           rocket = *alpha.rocket;
    const auto& bodyTube           = dynamic_cast<const BodyTube&>(rocket.getChild(0).getChild(1));
    const auto& finSet             = dynamic_cast<const FinSet&>(bodyTube.getChild(0));
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    FlightConditions           conditions{configuration};
    conditions.setMach(0.5);

    BarrowmanCalculator calculator;
    WarningSet          warnings;
    const ForceMap      coastingForces =
        calculator.getForceAnalysis(configuration, conditions, &warnings);

    const double bodyBaseArea        = kPi * QtRocket::MathUtil::pow2(bodyTube.getAftRadius());
    const double nozzleExitArea      = kPi * QtRocket::MathUtil::pow2(0.010 / 2);
    const double exposedAreaFraction = (bodyBaseArea - nozzleExitArea) / bodyBaseArea;
    const double coastingBodyBaseCD  = forcesOf(coastingForces, bodyTube).getBaseCD();
    const double coastingFinBaseCD   = forcesOf(coastingForces, finSet).getBaseCD();

    conditions.setThrustingNozzleExitAreas(Areas{{&bodyTube.getAssembly(), nozzleExitArea}});
    const ForceMap poweredForces =
        calculator.getForceAnalysis(configuration, conditions, &warnings);

    EXPECT_NEAR(coastingBodyBaseCD * exposedAreaFraction,
                forcesOf(poweredForces, bodyTube).getBaseCD(), kEpsilon)
        << "Powered body-base CD should use only the base area outside the thrusting motor";
    EXPECT_NEAR(coastingFinBaseCD, forcesOf(poweredForces, finSet).getBaseCD(), kEpsilon)
        << "Motor exhaust must not alter fin trailing-edge base drag";
    EXPECT_NEAR(forcesOf(coastingForces, rocket).getBaseCD() -
                    (coastingBodyBaseCD * (1 - exposedAreaFraction)),
                forcesOf(poweredForces, rocket).getBaseCD(), kEpsilon)
        << "Rocket base CD should contain the documented powered-area correction";
}

// Java: testPoweredBaseDragClampsExposedBodyAreaToZero
/// An invalid or unusually large nozzle exit area must never create negative body-base drag.
TEST(BarrowmanCalculatorTest, PoweredBaseDragClampsExposedBodyAreaToZero)
{
    const TestEstesAlphaIII alpha;
    const Rocket&           rocket = *alpha.rocket;
    const auto& bodyTube           = dynamic_cast<const BodyTube&>(rocket.getChild(0).getChild(1));
    const auto& finSet             = dynamic_cast<const FinSet&>(bodyTube.getChild(0));
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    FlightConditions           conditions{configuration};
    conditions.setThrustingNozzleExitAreas(Areas{
        {&bodyTube.getAssembly(), 2 * kPi * QtRocket::MathUtil::pow2(bodyTube.getAftRadius())}});

    BarrowmanCalculator calculator;
    WarningSet          warnings;
    const ForceMap forceMap = calculator.getForceAnalysis(configuration, conditions, &warnings);

    EXPECT_NEAR(0, forcesOf(forceMap, bodyTube).getBaseCD(), kEpsilon)
        << "Exposed body-base area should be clamped to zero";
    EXPECT_TRUE(forcesOf(forceMap, finSet).getBaseCD() > 0)
        << "Clamping body-base drag must not remove fin trailing-edge base drag";
}

// Java: testPoweredBaseDragLeavesInternalBodyStepUnchanged
/// Motor exhaust acts at the terminal base; it must not suppress drag from an upstream diameter
/// discontinuity.
TEST(BarrowmanCalculatorTest, PoweredBaseDragLeavesInternalBodyStepUnchanged)
{
    Rocket          rocket;
    AxialStage&     stage       = rocket.addChild(std::make_unique<AxialStage>());
    const BodyTube& forwardTube = stage.addChild(std::make_unique<BodyTube>(0.1, 0.02, 0.001));
    const BodyTube& aftTube     = stage.addChild(std::make_unique<BodyTube>(0.1, 0.01, 0.001));

    const FlightConfiguration configuration{rocket};
    FlightConditions          conditions{configuration};
    BarrowmanCalculator       calculator;
    WarningSet                coastingWarnings;
    const ForceMap            coastingForces =
        calculator.getForceAnalysis(configuration, conditions, &coastingWarnings);

    conditions.setThrustingNozzleExitAreas(
        Areas{{&aftTube.getAssembly(), kPi * QtRocket::MathUtil::pow2(aftTube.getAftRadius())}});
    WarningSet     poweredWarnings;
    const ForceMap poweredForces =
        calculator.getForceAnalysis(configuration, conditions, &poweredWarnings);

    EXPECT_TRUE(forcesOf(coastingForces, forwardTube).getBaseCD() > 0)
        << "The diameter step should create base drag";
    EXPECT_NEAR(forcesOf(coastingForces, forwardTube).getBaseCD(),
                forcesOf(poweredForces, forwardTube).getBaseCD(), kEpsilon)
        << "Powered correction must not alter an internal diameter step";
    EXPECT_NEAR(0, forcesOf(poweredForces, aftTube).getBaseCD(), kEpsilon)
        << "A motor filling the terminal base should remove its body-base drag";
}

// Java: testPoweredBaseDragUsesIndependentWakeAreas
/// Each core or booster assembly has an independent terminal wake.  Motors in one wake must not
/// scale the base drag of another, including when different motor sizes burn at the same time.
TEST(BarrowmanCalculatorTest, PoweredBaseDragUsesIndependentWakeAreas)
{
    const TestMultiStageEventTestRocket multi;
    Rocket&                             rocket        = *multi.rocket;
    FlightConfiguration&                configuration = rocket.getSelectedConfiguration();
    configuration.setAllStages();

    const auto& coreBody     = dynamic_cast<const BodyTube&>(rocket.getChild(1).getChild(0));
    const auto& sideBoosters = dynamic_cast<const ParallelStage&>(coreBody.getChild(1));
    const auto& sideBody     = dynamic_cast<const BodyTube&>(sideBoosters.getChild(1));
    const ComponentAssembly& coreAssembly  = coreBody.getAssembly();
    const ComponentAssembly& sideAssembly  = sideBody.getAssembly();
    const int                sideBodyCount = configuration.getActiveInstances().count(sideBody);

    FlightConditions conditions{configuration};
    conditions.setMach(0.5);
    BarrowmanCalculator calculator;
    WarningSet          warnings;
    const ForceMap      coastingForces =
        calculator.getForceAnalysis(configuration, conditions, &warnings);

    const double coreBaseArea = kPi * QtRocket::MathUtil::pow2(coreBody.getAftRadius());
    const double sideBaseArea =
        sideBodyCount * kPi * QtRocket::MathUtil::pow2(sideBody.getAftRadius());
    const double coreNozzleExitArea = kPi * QtRocket::MathUtil::pow2(0.010 / 2);
    const double sideNozzleExitArea = sideBodyCount * kPi * QtRocket::MathUtil::pow2(0.008 / 2);
    const double coreAreaScale      = (coreBaseArea - coreNozzleExitArea) / coreBaseArea;
    const double sideAreaScale      = (sideBaseArea - sideNozzleExitArea) / sideBaseArea;

    conditions.setThrustingNozzleExitAreas(
        Areas{{&coreAssembly, coreNozzleExitArea}, {&sideAssembly, sideNozzleExitArea}});
    const ForceMap poweredForces =
        calculator.getForceAnalysis(configuration, conditions, &warnings);

    const double coastingCoreBaseCD = forcesOf(coastingForces, coreBody).getBaseCD();
    const double coastingSideBaseCD = forcesOf(coastingForces, sideBody).getBaseCD();
    EXPECT_NEAR(coastingCoreBaseCD * coreAreaScale, forcesOf(poweredForces, coreBody).getBaseCD(),
                kEpsilon)
        << "The core motor should scale only the core wake";
    EXPECT_NEAR(coastingSideBaseCD * sideAreaScale, forcesOf(poweredForces, sideBody).getBaseCD(),
                kEpsilon)
        << "The side motors should scale only the repeated booster wakes";
    EXPECT_NEAR(forcesOf(coastingForces, rocket).getBaseCD() -
                    (coastingCoreBaseCD * (1 - coreAreaScale)) -
                    (sideBodyCount * coastingSideBaseCD * (1 - sideAreaScale)),
                forcesOf(poweredForces, rocket).getBaseCD(), kEpsilon)
        << "Total base CD should contain each wake's independent correction";
}

// Java: testPoweredBaseDragClampingDoesNotSpillAcrossWakes
/// Motor area is clamped within its own wake instead of spilling into another body's base-drag
/// contribution.
TEST(BarrowmanCalculatorTest, PoweredBaseDragClampingDoesNotSpillAcrossWakes)
{
    const TestMultiStageEventTestRocket multi;
    Rocket&                             rocket        = *multi.rocket;
    FlightConfiguration&                configuration = rocket.getSelectedConfiguration();
    configuration.setAllStages();

    const auto& coreBody      = dynamic_cast<const BodyTube&>(rocket.getChild(1).getChild(0));
    const auto& sideBoosters  = dynamic_cast<const ParallelStage&>(coreBody.getChild(1));
    const auto& sideBody      = dynamic_cast<const BodyTube&>(sideBoosters.getChild(1));
    const int   sideBodyCount = configuration.getActiveInstances().count(sideBody);

    FlightConditions    conditions{configuration};
    BarrowmanCalculator calculator;
    WarningSet          coastingWarnings;
    const ForceMap      coastingForces =
        calculator.getForceAnalysis(configuration, conditions, &coastingWarnings);

    const double oversizedSideNozzleExitArea =
        2 * sideBodyCount * kPi * QtRocket::MathUtil::pow2(sideBody.getAftRadius());
    conditions.setThrustingNozzleExitAreas(
        Areas{{&sideBody.getAssembly(), oversizedSideNozzleExitArea}});
    WarningSet     poweredWarnings;
    const ForceMap poweredForces =
        calculator.getForceAnalysis(configuration, conditions, &poweredWarnings);

    EXPECT_NEAR(forcesOf(coastingForces, coreBody).getBaseCD(),
                forcesOf(poweredForces, coreBody).getBaseCD(), kEpsilon)
        << "Excess side-nozzle area must not reduce the core base drag";
    EXPECT_NEAR(0, forcesOf(poweredForces, sideBody).getBaseCD(), kEpsilon)
        << "The side wake should clamp at zero exposed area";
    EXPECT_NEAR(forcesOf(coastingForces, rocket).getBaseCD() -
                    (sideBodyCount * forcesOf(coastingForces, sideBody).getBaseCD()),
                forcesOf(poweredForces, rocket).getBaseCD(), kEpsilon)
        << "Only the side-body base contribution should be removed";
}

// Java: testRailButtonDrag
/// Tests railbutton drag. Really is testing instancing more than actual drag calculations, and
/// making sure we don't divide by 0 when not moving
//
// As written the Java test compares zeros: the rocket is new, so its events are disabled, and the
// selected configuration, taken before the buttons are added, never learns of the stage (its
// active instances hold the rocket alone); and the pair of buttons is never added to the tube.
// Every CD is 0.0 in OpenRocket (the probe Explore.java). The port keeps the statements;
// RailButtonsCountPerInstance below is the test with the events enabled and the pair in place.
TEST(BarrowmanCalculatorTest, RailButtonDrag)
{
    // minimal rocket with nothing on it but two railbuttons
    Rocket rocket;

    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());

    // phantom tubes have no drag to confuse things
    auto phantomOwned = std::make_unique<BodyTube>();
    phantomOwned->setOuterRadius(0);
    BodyTube& phantom = stage.addChild(std::move(phantomOwned));

    // set up test environment
    WarningSet                 warnings;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    FlightConditions           conditions{config};
    BarrowmanCalculator        calc;

    // part 1: instancing

    // Put two individual railbuttons and get their CD
    auto button1Owned = std::make_unique<RailButton>();
    button1Owned->setInstanceCount(1);
    button1Owned->setAxialOffset(1.0);
    const RailButton& button1 = phantom.addChild(std::move(button1Owned));

    auto button2Owned = std::make_unique<RailButton>();
    button2Owned->setInstanceCount(1);
    button2Owned->setAxialOffset(2.0);
    const RailButton& button2 = phantom.addChild(std::move(button2Owned));

    const AerodynamicForces individualForces =
        calc.getAerodynamicForces(config, conditions, &warnings);
    const double individualCD = individualForces.getCD();

    // get rid of individual buttons and put in a railbutton set with two instances at same
    // locations as original railbuttons
    const std::unique_ptr<RocketComponent> removed1 = phantom.removeChild(&button1);
    const std::unique_ptr<RocketComponent> removed2 = phantom.removeChild(&button2);

    RailButton buttons;
    buttons.setInstanceCount(2);
    buttons.setAxialOffset(1.0);
    buttons.setInstanceSeparation(1.0);

    const AerodynamicForces pairForces = calc.getAerodynamicForces(config, conditions, &warnings);
    const double            pairCD     = pairForces.getCD();

    EXPECT_NEAR(individualCD, pairCD, kEpsilon)
        << "two individual railbuttons should have same CD as a pair";

    // part 2: test at Mach 0
    conditions.setMach(QtRocket::MathUtil::kEpsilon);
    const AerodynamicForces epsForces = calc.getAerodynamicForces(config, conditions, &warnings);
    const double            epsCD     = epsForces.getCD();

    conditions.setMach(0);
    const AerodynamicForces zeroForces = calc.getAerodynamicForces(config, conditions, &warnings);
    const double            zeroCD     = zeroForces.getCD();
    EXPECT_NEAR(epsCD, zeroCD, kEpsilon)
        << "drag at mach 0 should equal drag at mach MathUtil.EPSILON";

    // What OpenRocket computes here (see the comment above the test).
    EXPECT_EQ(individualCD, 0.0);
    EXPECT_EQ(pairCD, 0.0);
    EXPECT_EQ(epsCD, 0.0);
    EXPECT_EQ(zeroCD, 0.0);
}

// Java: testForceMapContent
/// Tests that the force map contains all necessary components when stages are enabled/disabled.
TEST(BarrowmanCalculatorTest, ForceMapContent)
{
    const TestFalcon9Heavy      falcon;
    Rocket&                     rocket = *falcon.rocket;
    const FlightConfigurationId fcid = FlightConfigurationId::fromString(TestFalcon9Heavy::kFcid1);

    const std::unique_ptr<AerodynamicCalculator> calculator =
        std::make_unique<BarrowmanCalculator>();

    // Java: new Simulation(rocket), setFlightConfigurationId(fcid) and getActiveConfiguration(),
    // which is rocket.getFlightConfiguration(fcid)
    FlightConfiguration&   config = rocket.getFlightConfiguration(fcid);
    const FlightConditions conditions{config};
    WarningSet             warnings;

    // Get force analysis with all stages active
    const ForceMap forceMap = calculator->getForceAnalysis(config, conditions, &warnings);

    // First verify all stages active case
    EXPECT_TRUE(config.isStageActive(TestFalcon9Heavy::kPayloadStageNumber));
    EXPECT_TRUE(config.isStageActive(TestFalcon9Heavy::kCoreStageNumber));
    EXPECT_TRUE(config.isStageActive(TestFalcon9Heavy::kBoosterStageNumber));

    // Check that force map contains all aerodynamic components
    expectHoldsEveryAerodynamicComponent(
        rocket, forceMap, "Force map missing component: ", "Force entry is null for: ");
    EXPECT_EQ(13U, forceMap.size()) << "Force map should contain 13 components";

    // Now disable core stage and check map is updated correctly
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, false);

    warnings.clear();
    const ForceMap disabledForceMap = calculator->getForceAnalysis(config, conditions, &warnings);

    // Map should only contain components from active stages
    expectHoldsActiveComponentsOnly(rocket, config, disabledForceMap);
    EXPECT_EQ(7U, disabledForceMap.size()) << "Force map should contain 7 components";

    // Re-enable the stages and verify map is complete again
    config.setAllStages();

    warnings.clear();
    const ForceMap reenabledForceMap = calculator->getForceAnalysis(config, conditions, &warnings);

    expectHoldsEveryAerodynamicComponent(rocket, reenabledForceMap,
                                         "Force map missing component after re-enable: ",
                                         "Force entry is null after re-enable for: ");

    // Now disable the core stage, but enable its child stage, the booster
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);

    warnings.clear();
    const ForceMap boosterOnlyForceMap =
        calculator->getForceAnalysis(config, conditions, &warnings);

    // Map should only contain components from active stages
    expectHoldsActiveComponentsOnly(rocket, config, boosterOnlyForceMap);
    EXPECT_EQ(11U, boosterOnlyForceMap.size()) << "Force map should contain 10 components";
}

// Java: testDisabledStageAerodynamics
TEST(BarrowmanCalculatorTest, DisabledStageAerodynamics)
{
    const TestFalcon9Heavy      falcon;
    Rocket&                     rocket = *falcon.rocket;
    const FlightConfigurationId fcid = FlightConfigurationId::fromString(TestFalcon9Heavy::kFcid1);

    // Java: simulation.getActiveConfiguration() (see ForceMapContent)
    FlightConfiguration& config = rocket.getFlightConfiguration(fcid);

    BarrowmanCalculator    calc;
    const FlightConditions conditions{config};
    WarningSet             warnings;

    // Baseline: all stages active
    const Coordinate cpAll  = calc.getCP(config, conditions, &warnings);
    const double     cnAll  = cpAll.weight;
    const double     cpxAll = cpAll.x;

    // Disable core + booster stages
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, false);

    warnings.clear();
    const Coordinate cpPayloadOnly = calc.getCP(config, conditions, &warnings);

    // CNa must decrease: booster fins are gone
    EXPECT_TRUE(cpPayloadOnly.weight < cnAll) << "CNa should decrease when booster/core disabled";

    // CP should shift forward: rear fins removed
    EXPECT_TRUE(cpPayloadOnly.x < cpxAll) << "CP should move forward when booster/core disabled";

    // Re-enable all stages and verify full restoration
    config.setAllStages();
    warnings.clear();
    const Coordinate cpRestored = calc.getCP(config, conditions, &warnings);

    EXPECT_NEAR(cnAll, cpRestored.weight, kEpsilon)
        << "CNa should be restored after re-enabling all stages";
    EXPECT_NEAR(cpxAll, cpRestored.x, kEpsilon)
        << "CP should be restored after re-enabling all stages";
}

// Java: testStageDragAggregatesAerodynamicDescendants
TEST(BarrowmanCalculatorTest, StageDragAggregatesAerodynamicDescendants)
{
    const TestFalcon9Heavy     falcon;
    const Rocket&              rocket = *falcon.rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    const FlightConditions     conditions{config};

    WarningSet     warnings;
    const ForceMap forceMap = calculator.getForceAnalysis(config, conditions, &warnings);

    const int stageCount = static_cast<int>(rocket.getStageCount());
    EXPECT_EQ(stageCount, 3);
    for (int stageNumber = 0; stageNumber < stageCount; stageNumber++)
    {
        const AxialStage* const stage = rocket.getStage(stageNumber);
        ASSERT_NE(stage, nullptr);
        expectStageDragOfDescendants(*stage, forceMap, config);
    }
}

// Java: testStageDragRespectsAssemblyOverrides
TEST(BarrowmanCalculatorTest, StageDragRespectsAssemblyOverrides)
{
    const TestEstesAlphaIII alpha;
    const Rocket&           rocket = *alpha.rocket;
    AxialStage* const       stage  = rocket.getStage(0);
    ASSERT_NE(stage, nullptr);
    stage->setCDOverridden(true);
    stage->setOverrideCD(0.5);
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    const FlightConditions     conditions{config};

    WarningSet   warnings;
    ForceMap     forceMap     = calculator.getForceAnalysis(config, conditions, &warnings);
    const double descendantCD = descendantCDTotal(*stage, forceMap);
    EXPECT_NEAR(0.5 + descendantCD, forcesOf(forceMap, *stage).getCDTotal(), kEpsilon);

    stage->setSubcomponentsOverriddenCD(true);
    WarningSet overriddenWarnings;
    forceMap = calculator.getForceAnalysis(config, conditions, &overriddenWarnings);
    EXPECT_NEAR(0.5, forcesOf(forceMap, *stage).getCDTotal(), kEpsilon);
    EXPECT_NEAR(0, forcesOf(forceMap, *stage).getPressureCD(), kEpsilon);
    EXPECT_NEAR(0, forcesOf(forceMap, *stage).getBaseCD(), kEpsilon);
    EXPECT_NEAR(0, forcesOf(forceMap, *stage).getFrictionCD(), kEpsilon);
}

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants

// ================================================= beyond BarrowmanCalculatorTest.java
//
// The pinned values were printed by the Java probes CalcProbe.java and ButtonsProbe.java, which
// call OpenRocket's BarrowmanCalculator. Values that sum over the components of a rocket are
// compared within a relative 1e-9 (see BarrowmanCalculator, "Summation order").

// Java: testGetWorstCP, whose body is commented out ("NYI"). What its comments set out to
// compare, the CP and the worst CP of the Falcon 9 Heavy, with OpenRocket's values (at an angle
// of attack of 4 degrees).
TEST(BarrowmanCalculatorTest, GetWorstCP)
{
    const TestFalcon9Heavy     falcon;
    const FlightConfiguration& config = falcon.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calc;
    FlightConditions           conditions{config};
    conditions.setAOA(QtRocket::MathUtil::javaToRadians(4));
    WarningSet warnings;

    const Coordinate calcBestCP  = calc.getCP(config, conditions, &warnings);
    const Coordinate calcWorstCP = calc.getWorstCP(config, conditions, &warnings);

    EXPECT_NEAR(1.0105027069253338, calcBestCP.x, 1e-9)
        << " Falcon Heavy best CP x value is incorrect:";
    EXPECT_NEAR(22.350461647195615, calcBestCP.weight, 22.35e-9);
    EXPECT_NEAR(1.0105027069253336, calcWorstCP.x, 1e-9)
        << " Falcon Heavy Worst CP x value is incorrect:";
    EXPECT_NEAR(22.35046164719562, calcWorstCP.weight, 22.35e-9);

    // Two boosters with three fins each: the CP does not depend on the wind direction but for
    // the last bits, and those decide which of the 360 directions is left in the conditions.
    EXPECT_LE(calcWorstCP.x, calcBestCP.x);
    const double division = conditions.getTheta() / (2 * kPi / 360);
    EXPECT_NEAR(division, std::round(division), 1e-9);
    EXPECT_GE(conditions.getTheta(), 0);
    EXPECT_LT(conditions.getTheta(), 2 * kPi);
}

TEST(BarrowmanCalculator, WorstCpOfARocketWhoseCpDependsOnTheWindDirection)
{
    // Two fins in one plane, turned by 60 degrees: they give no lift when the lateral airflow
    // lies in their plane, and the CP is then that of the nose cone and the body lift, far
    // forward.
    const TestEstesAlphaIII alpha;
    alpha.fins->setFinCount(2);
    alpha.fins->setAngleOffset(kPi / 3);
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calc;
    FlightConditions           conditions{config};
    conditions.setAOA(QtRocket::MathUtil::javaToRadians(4));
    conditions.setTheta(0.4);
    WarningSet warnings;

    const Coordinate cp = calc.getCP(config, conditions, &warnings);
    EXPECT_NEAR(cp.x, 0.20877412781161375, 1e-9);
    EXPECT_NEAR(cp.weight, 16.97315529191544, 17e-9);

    const Coordinate worst = calc.getWorstCP(config, conditions, &warnings);
    EXPECT_NEAR(worst.x, 0.07036311164392002, 1e-9);
    EXPECT_NEAR(worst.weight, 3.002790476277246, 3e-9);
    // The 60th of the 360 directions: the first of the two (60 and 240 degrees) in the fins'
    // plane, where what is left of the fins' lift is far below the last bit of the total.
    EXPECT_EQ(conditions.getTheta(), 2 * kPi * 60 / 360);
    // The conditions now give that CP.
    EXPECT_TRUE(calc.getCP(config, conditions, &warnings).exactlyEquals(worst));

    // One fin at 2 rad (114.6 degrees): the nearest directions are 115 and 295 degrees, which
    // differ in the last bits only.
    alpha.fins->setFinCount(1);
    alpha.fins->setAngleOffset(2.0);
    conditions.setTheta(0.4);
    const Coordinate worstOfOne = calc.getWorstCP(config, conditions, &warnings);
    EXPECT_NEAR(worstOfOne.x, 0.07041777244856517, 1e-9);
    EXPECT_NEAR(worstOfOne.weight, 3.003766851491675, 3e-9);
    EXPECT_TRUE(conditions.getTheta() == 2 * kPi * 115 / 360 ||
                conditions.getTheta() == 2 * kPi * 295 / 360)
        << conditions.getTheta();
}

TEST(BarrowmanCalculator, WorstCpOfARocketWithoutLift)
{
    // No direction gives a CP with a weight: the "worst" stays at Double.MAX_VALUE and theta
    // becomes 0.
    Rocket rocket;
    rocket.enableEvents();
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    BarrowmanCalculator        calc;
    FlightConditions           conditions{config};
    conditions.setTheta(0.4);

    const Coordinate worst = calc.getWorstCP(config, conditions, nullptr);
    EXPECT_TRUE(worst.exactlyEquals(Coordinate{std::numeric_limits<double>::max(), 0, 0, 0}));
    EXPECT_EQ(conditions.getTheta(), 0.0);
}

// ---------------------------------------------------------------- the total forces

/// What getAerodynamicForces() gives: the non-axial forces (Cm and Cyaw less the damping
/// moments), the drag, and the pitch and yaw damping moments.
struct TotalPin
{
    NonAxialPin           forces;
    DragPin               drag;
    std::array<double, 2> damping;
};

struct TotalCase
{
    /// The Mach number, the angle of attack in degrees, theta, the roll, pitch and yaw rates
    /// and the x of the pitch centre.
    std::array<double, 7> conditions;
    TotalPin              expected;
};

/// FlightConditions made for @p configuration with the values of @p c (CalcProbe.conditions()).
[[nodiscard]] FlightConditions conditionsOf(const FlightConfiguration&   configuration,
                                            const std::array<double, 7>& c)
{
    FlightConditions conditions{configuration};
    conditions.setMach(c[0]);
    conditions.setAOA(c[1] * kPi / 180);
    conditions.setTheta(c[2]);
    conditions.setRollRate(c[3]);
    conditions.setPitchRate(c[4]);
    conditions.setYawRate(c[5]);
    conditions.setPitchCenter(Coordinate{c[6], 0, 0});
    return conditions;
}

/// Adds the differences between @p forces and @p expected to @p diff.
void compareTotal(JavaValueDifferences& diff, const std::string& what, const TotalPin& expected,
                  const AerodynamicForces& forces)
{
    compareNonAxial(diff, what, expected.forces, forces, kRocketTolerance);
    compareDrag(diff, what, expected.drag, forces, kRocketTolerance);
    diff.pinned(what + ": pitch damping", expected.damping[0], forces.getPitchDampingMoment(),
                kRocketTolerance);
    diff.pinned(what + ": yaw damping", expected.damping[1], forces.getYawDampingMoment(),
                kRocketTolerance);
    if (forces.getComponent() != nullptr)
    {
        diff.problem(what + ": the total forces have a component");
    }
}

/// The differences between getAerodynamicForces() of @p configuration and OpenRocket's, each
/// case by a new calculator: at rest, with pitch and yaw rates (damping moments below the total
/// moments, and capped by them), subsonic to supersonic, and beyond 90 degrees.
[[nodiscard]] std::string totalDifferences(const FlightConfiguration& configuration,
                                           std::span<const TotalCase> cases)
{
    JavaValueDifferences diff;
    for (const TotalCase& c : cases)
    {
        const FlightConditions conditions = conditionsOf(configuration, c.conditions);
        BarrowmanCalculator    calculator;
        WarningSet             warnings;
        compareTotal(diff, std::format("Mach {} aoa {}", c.conditions[0], c.conditions[1]),
                     c.expected,
                     calculator.getAerodynamicForces(configuration, conditions, &warnings));
    }
    return diff.text();
}

TEST(BarrowmanCalculator, AerodynamicForcesOfTheAlphaAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.22514554750367705, 0.0, 0.0,
                                    30.820533821498472},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, 1.541755469408005},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {1.0933771293819694, 10.199210351278602, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.22387726032632682, 0.0, 0.0, 31.322947464858103},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, 1.5594544330706572},
                        .damping = {5.1158283860786216E-5, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected   = {.forces  = {2.950182908075077, 27.42477505997564, 0.0, 0.0,
                                    -0.6133015126770895, 0.6133015126770895, 0.0, 0.2231008122146244,
                                    0.0, 0.0, 33.80660588486672},
                        .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                                    1.6590460045239195, 1.7628841169585023},
                        .damping = {-2.666027568008677E-4, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected   = {.forces  = {1.9909639540287445, 18.945550551788756, 0.0, 0.0,
                                    0.502165840469925, -0.502165840469925, 0.0, 0.22887664297617863,
                                    0.0, 0.0, 38.02461057617518},
                        .drag    = {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                                    1.8974033669616457, 1.9443267353894575},
                        .damping = {0.041330535072824234, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {0.6804718090022704, 6.345609593931472, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.22380747791684003, 0.0, 0.0, 19.494081366731177},
                        .drag    = {0.31235524957723215, 1.247016026859001, 0.34350549232432814, 0.0,
                                    1.9028767687605612, 1.9247213137957933},
                        .damping = {2.046331354431449E-6, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {3.5019711719348967, 29.92280568595406, 0.0, 0.0,
                                    -0.00676404318747777, 0.00676404318747777, 0.0,
                                    0.20507124304628335, 0.0, 0.0, 16.720680677362697},
                        .drag    = {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                                    1.8567818000221035, 2.2976021321095814},
                        .damping = {2.602032517314372E-4, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {26.005925059203086, 187.9955484523873, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.21349691020207312, 0.0, 0.0, 37.95672453945721},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, -0.018491353491944747},
                        .damping = {2.046331354431449E-6, 0.0}}},
    }};
    const TestEstesAlphaIII               alpha;
    EXPECT_EQ(totalDifferences(alpha.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanCalculator, AerodynamicForcesOfTheFalconAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0310997664583448, 0.0, 0.0,
                                    20.193472360813374},
                        .drag    = {0.530348823850528, 0.06151063350596056, 0.2305348542194396, 0.0,
                                    0.822394311575928, 0.822394311575928},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {0.7426009956617993, 7.248159358689257, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    1.0202443758117772, 0.0, 0.0, 21.273951456816924},
                        .drag    = {0.530348823850528, 0.06151063350596056, 0.2305348542194396, 0.0,
                                    0.822394311575928, 0.831835190707392},
                        .damping = {0.03678765377398091, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected   = {.forces  = {2.025775711955578, 19.778915915558702, 0.0, 0.0,
                                    -0.8495278526554195, 0.8495278526554195, 0.0, 1.0085913861645357,
                                    0.0, 0.0, 23.213679707032846},
                        .drag    = {0.5158993604358315, 0.08558740925800616, 0.29197580625514447, 0.0,
                                    0.893462575948982, 0.9493835493062611},
                        .damping = {-0.13295501767672335, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected   = {.forces = {1.1987477732824747, 0.0, 0.0, 0.0, 0.6955812534458622,
                                   -0.6955812534458622, 0.0, 1.0304274209678657, 0.0, 0.0,
                                   22.89439603659702},
                        .drag   = {0.47872582779183914, 0.37781147336656357, 0.41542660803058856, 0.0,
                                   1.2719639091889914, 1.3034199675986113},
                        .damping = {11.877140157831075, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {0.9653245587051095, 10.141231116230918, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    1.0927320383754493, 0.0, 0.0, 27.65451153706572},
                        .drag    = {0.3809005776906953, 0.76833142359795, 0.29174241232528425, 0.0,
                                    1.4409744136139295, 1.4575164361923594},
                        .damping = {0.0014715061509592367, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {5.39201113407096, 52.21176854514787, 0.0, 0.0,
                                    -0.009371694690155674, 0.009371694690155674, 0.0,
                                    1.0074704618177432, 0.0, 0.0, 25.744956755817896},
                        .drag    = {0.3111426230554807, 0.7502833414730651, 0.2188068092439632, 0.0,
                                    1.280232773772509, 1.584174053505514},
                        .damping = {0.021807871860643896, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {39.19066246081298, 312.7667506568437, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.9610768072573409, 0.0, 0.0, 35.97816042416296},
                        .drag    = {0.530348823850528, 0.06151063350596056, 0.2305348542194396, 0.0,
                                    0.822394311575928, -0.009863551144692359},
                        .damping = {0.0014715061509592367, 0.0}}},
    }};
    const TestFalcon9Heavy                falcon;
    EXPECT_EQ(totalDifferences(falcon.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanCalculator, AerodynamicForcesOfTheStepsRocketAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.22116070670159432, 0.0, 0.0,
                                    3.5534708252375147},
                        .drag    = {0.08131515656090255, 1.1545886503072256, 0.22110142360312154, 0.0,
                                    1.4570052304712495, 1.4570052304712495},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {0.1286295519469514, 0.469998791362653, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.21923646579595024, 0.0, 0.0, 3.6849652236095483},
                        .drag    = {0.08131515656090255, 1.1545886503072256, 0.22110142360312154, 0.0,
                                    1.4570052304712495, 1.4737312827811575},
                        .damping = {6.014733450701047E-6, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected = {.forces = {0.3490050523983986, 1.2712942666370817, 0.0, 0.0,
                                 -0.01907210359998241, 0.01907210359998241, 0.0, 0.2185528124105703,
                                 0.0, 0.0, 3.9993033062340775},
                      .drag   = {0.07909970829940469, 1.2476830147370608, 0.28002822670463695, 0.0,
                                 1.6068109497411025, 1.7073797197483056},
                      .damping = {-2.7004184274096688E-5, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected   = {.forces  = {0.2157748820871804, 0.8054692715538493, 0.0, 0.0,
                                    0.015616076092541366, -0.015616076092541366, 0.0,
                                    0.22516849783576418, 0.0, 0.0, 4.120996689509475},
                        .drag    = {0.07340011684010511, 1.5098593571482124, 0.39842745145490377, 0.0,
                                    1.9816869254432212, 2.030694652176653},
                        .damping = {0.004292496283809889, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {0.09081963570429617, 0.3005646824322166, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.198568241784306, 0.0, 0.0, 2.6017909113859066},
                        .drag    = {0.05840116677205349, 1.8777692120130092, 0.27980438319807843, 0.0,
                                    2.215974761983141, 2.2414135929572607},
                        .damping = {2.405893380280419E-7, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {0.5685339190426727, 1.644021736811944, 0.0, 0.0,
                                    -2.1034098752269794E-4, 2.1034098752269794E-4, 0.0,
                                    0.17350365883457516, 0.0, 0.0, 2.714549505931464},
                        .drag    = {0.04770560425275299, 1.97424681028892, 0.20985328739855885, 0.0,
                                    2.2318057019402318, 2.761660815057082},
                        .damping = {2.3515279120167413E-5, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {5.465447567561023, 15.149018951841633, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.2132498642588523, 0.0, 0.0, 5.263136336791786},
                        .drag    = {0.08131515656090255, 1.1545886503072256, 0.22110142360312154, 0.0,
                                    1.4570052304712495, -0.017474884500718747},
                        .damping = {2.405893380280419E-7, 0.0}}},
    }};
    const TestStepsRocket                 steps;
    EXPECT_EQ(totalDifferences(steps.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanCalculator, AerodynamicForcesOfFinsOnPodsAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.220726594403691, 0.0, 0.0,
                                    22.23121475709279},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, 1.541755469408005},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {0.7935533330255734, 7.243660580696469, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.2190767763614928, 0.0, 0.0, 22.73362840045242},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, 1.5594544330706572},
                        .damping = {5.21722268973166E-5, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected   = {.forces  = {2.1555196402000987, 19.594051456247758, 0.0, 0.0,
                                    -0.31562715446940026, 0.31562715446940026, 0.0,
                                    0.21816149750877273, 0.0, 0.0, 24.700435608204682},
                        .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                                    1.6590460045239195, 1.7628841169585023},
                        .damping = {-2.434305967343177E-4, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected = {.forces  = {1.4521528495085345, 13.613101561545381, 0.0, 0.0,
                                  0.2584329985237151, -0.2584329985237151, 0.0, 0.22562165699204675,
                                  0.0, 0.0, 27.734076494911733},
                      .drag    = {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                                  1.8974033669616457, 1.9443267353894575},
                      .damping = {0.03844560978119327, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {0.5121252128559437, 4.589014719654224, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.21505756911058316, 0.0, 0.0, 14.671306639442252},
                        .drag    = {0.31235524957723215, 1.247016026859001, 0.34350549232432814, 0.0,
                                    1.9028767687605612, 1.9247213137957933},
                        .damping = {2.0868890758926645E-6, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {2.9231480948122823, 23.873201338826604, 0.0, 0.0,
                                    -0.003480879728405261, 0.003480879728405261, 0.0,
                                    0.1960085676202045, 0.0, 0.0, 13.95700406037093},
                        .drag    = {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                                    1.8567818000221035, 2.2976021321095814},
                        .damping = {2.1828641860750444E-4, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {23.007687095639128, 158.09457040529827, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.2059359177642648, 0.0, 0.0, 29.367405475051527},
                        .drag    = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                                    1.541755469408005, -0.018491353491944747},
                        .damping = {2.0868890758926645E-6, 0.0}}},
    }};
    const TestEstesAlphaIIIWithPods       pods;
    EXPECT_EQ(totalDifferences(pods.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanCalculator, AerodynamicForcesOfTheMultiStageRocketAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3213666218514193, 0.0, 0.0,
                                    27.484318301185283},
                        .drag    = {0.7456190658747087, 0.7969129024857299, 0.4116830832374588, 0.0,
                                    1.9542150515978973, 1.9542150515978973},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {1.004530583897598, 15.976508160451278, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.3181153316506456, 0.0, 0.0, 28.77768142457231},
                        .drag    = {0.7456190658747087, 0.7969129024857299, 0.4116830832374588, 0.0,
                                    1.9542150515978973, 1.97664894716275},
                        .damping = {0.0013208320387739555, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected   = {.forces  = {2.7727310057250674, 43.67881370550049, 0.0, 0.0,
                                    -0.27492302224252974, 0.27492302224252974, 0.0,
                                    0.31503706189781155, 0.0, 0.0, 31.773156870622095},
                        .drag    = {0.7253045201848831, 0.8518773167304247, 0.5214027204556426, 0.0,
                                    2.09858455737095, 2.2299329700295227},
                        .damping = {-0.0031622316710032097, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected = {.forces = {1.7367497867811945, 27.18864956391656, 0.0, 0.0,
                                 0.22510499323775304, -0.22510499323775304, 0.0, 0.3221390651187139,
                                 0.0, 0.0, 33.16947761760269},
                      .drag   = {0.6730421346778526, 0.9709017427879301, 0.7418579174588453, 0.0,
                                 2.385801794924628, 2.444803406584173},
                      .damping = {0.7850980690244217, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {0.7950114561066158, 12.628093187570897, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.31768463017365467, 0.0, 0.0, 22.775400549729593},
                        .drag    = {0.5355093103947691, 1.1985106322515935, 0.5209859317102744, 0.0,
                                    2.255005874356637, 2.2808927726496786},
                        .damping = {5.2833281550958234E-5, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {4.913836133754363, 71.12672983130801, 0.0, 0.0,
                                    -0.0030318692954956505, 0.0030318692954956505, 0.0,
                                    0.2894976071320231, 0.0, 0.0, 23.46183930691724},
                        .drag    = {0.43743638436316895, 1.2942278641229918, 0.39073944878270583, 0.0,
                                    2.122403697268867, 2.626285576465774},
                        .damping = {4.60296729989312E-4, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {48.08319090654644, 622.202656945019, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.29558108068046407, 0.0, 0.0, 46.36909368561753},
                        .drag    = {0.7456190658747087, 0.7969129024857299, 0.4116830832374588, 0.0,
                                    1.9542150515978973, -0.02343827022857983},
                        .damping = {5.2833281550958234E-5, 0.0}}},
    }};
    const TestMultiStageEventTestRocket   multi;
    FlightConfiguration&                  config = multi.rocket->getSelectedConfiguration();
    config.setAllStages();
    EXPECT_EQ(totalDifferences(config, kCases), "");
}

TEST(BarrowmanCalculator, AerodynamicForcesOfTheBetaAreOpenRockets)
{
    static const std::array<TotalCase, 7> kCases{{
        {.conditions = {0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .expected   = {.forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2598610155085625, 0.0, 0.0,
                                    59.02995653188583},
                        .drag    = {0.6511060085003184, 1.7060275300297463, 0.3709344134027015, 0.0,
                                    2.728067951932766, 2.728067951932766},
                        .damping = {0.0, 0.0}}},
        {.conditions = {0.3, 2.0, 0.0, 0.0, 0.5, -0.3, 0.15},
         .expected   = {.forces  = {2.0826664719590773, 22.47495698310272, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.25899654239651587, 0.0, 0.0, 59.66399948832817},
                        .drag    = {0.6511060085003184, 1.7060275300297463, 0.3709344134027015, 0.0,
                                    2.728067951932766, 2.7593854834794804},
                        .damping = {1.8531700358744209E-4, 0.0}}},
        {.conditions = {0.6, 5.0, 1.9, 20.0, -2.0, 4.0, 0.2},
         .expected = {.forces = {5.591956820368838, 60.23752525695652, 0.0, 0.0, -1.226603025354179,
                                 1.226603025354179, 0.0, 0.25853024471174635, 0.0, 0.0,
                                 64.07910500530596},
                      .drag   = {0.6333664906097952, 1.829006854679993, 0.46979392676970866, 0.0,
                                 2.932167272059497, 3.1156888344772247},
                      .damping = {-4.4337414507889216E-4, 0.0}}},
        {.conditions = {0.95, 3.0, 0.4, -15.0, 30.0, 40.0, 0.1},
         .expected   = {.forces  = {3.8161247381312453, 41.7459236144055, 0.0, 0.0, 1.00433168093985,
                                    -1.00433168093985, 0.0, 0.2634922864868939, 0.0, 0.0,
                                    72.88261386346228},
                        .drag    = {0.5877287718609221, 2.0948363917404067, 0.6684283193682321, 0.0,
                                    3.350993482969561, 3.4338645817240496},
                        .damping = {0.15071941765287508, 0.0}}},
        {.conditions = {1.5, 2.0, 0.0, 0.0, 0.5, 0.3, 0.15},
         .expected   = {.forces  = {1.2568558311996796, 13.735571847877644, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.26228457876408107, 0.0, 0.0, 36.00626729207433},
                        .drag    = {0.4676293104131623, 2.538592238903187, 0.4694183920560638, 0.0,
                                    3.475639941372413, 3.5155394106769933},
                        .damping = {7.412680143497684E-6, 0.0}}},
        {.conditions = {2.0, 12.0, 2.5, 5.0, 3.0, -3.0, 0.3},
         .expected   = {.forces  = {6.0010501655129085, 61.94651339496858, 0.0, 0.0,
                                    -0.01352808637495554, 0.01352808637495554, 0.0,
                                    0.24774374983910694, 0.0, 0.0, 28.652903927514483},
                        .drag    = {0.3819878960061005, 2.7214241915442647, 0.3520637940420478, 0.0,
                                    3.455475881592413, 4.275843695207188},
                        .damping = {2.645623964663039E-4, 0.0}}},
        {.conditions = {0.3, 100.0, 0.3, 0.0, 0.1, 0.1, 0.15},
         .expected   = {.forces  = {39.119594156755994, 351.180654938277, 0.0, 0.0, 0.0, 0.0, 0.0,
                                    0.25171935520755484, 0.0, 0.0, 68.52673052886512},
                        .drag    = {0.6511060085003184, 1.7060275300297463, 0.3709344134027015, 0.0,
                                    2.728067951932766, -0.03271963022035159},
                        .damping = {7.412680143497684E-6, 0.0}}},
    }};
    const TestBeta                        beta;
    EXPECT_EQ(totalDifferences(beta.rocket->getSelectedConfiguration(), kCases), "");
}

// ---------------------------------------------------------------- the force analysis

struct AnalysisPin
{
    /// The component's index in allComponents(rocket), and its name.
    int              index;
    std::string_view name;
    NonAxialPin      forces;
    DragPin          drag;
    /// getCDTotal(): CD times the component's instance count.
    double cdTotal;
};

/// The conditions of every force analysis below (CalcProbe.analyses()).
constexpr std::array<double, 7> kAnalysisConditions{0.6, 5, 1.9, 20, -2.0, 4.0, 0.2};

/// The differences between getForceAnalysis() of @p configuration and OpenRocket's. Java's map
/// is in the order of its InstanceMap, which follows the components' hash codes; here the
/// entries are in tree order, so the pins, which are, name them in order.
[[nodiscard]] std::string analysisDifferences(Rocket&                      rocket,
                                              const FlightConfiguration&   configuration,
                                              std::span<const AnalysisPin> pins)
{
    const FlightConditions conditions = conditionsOf(configuration, kAnalysisConditions);
    BarrowmanCalculator    calculator;
    WarningSet             warnings;
    const ForceMap forceMap = calculator.getForceAnalysis(configuration, conditions, &warnings);

    JavaValueDifferences diff;
    if (forceMap.size() != pins.size())
    {
        diff.problem(std::format("{} entries, OpenRocket has {}", forceMap.size(), pins.size()));
        return diff.text();
    }
    const std::vector<const RocketComponent*> components = allComponents(rocket);
    auto                                      entry      = forceMap.begin();
    for (const AnalysisPin& pin : pins)
    {
        const RocketComponent& component = *components.at(static_cast<std::size_t>(pin.index));
        const std::string      what{pin.name};
        diff.name(std::format("component {}", pin.index), pin.name, component.getName());
        if (entry->first != &component || entry->second.getComponent() != &component)
        {
            diff.problem(what + ": the entry is out of order or belongs to another component");
        }
        const AerodynamicForces& forces = entry->second;
        // An assembly's forces sum over its subtree.
        const double tolerance =
            QtRocket::isAssembly(component.kind()) ? kRocketTolerance : kComponentTolerance;
        compareNonAxial(diff, what, pin.forces, forces, tolerance);
        compareDrag(diff, what, pin.drag, forces, tolerance);
        diff.pinned(what + ": CDTotal", pin.cdTotal, forces.getCDTotal(), tolerance);
        ++entry;
    }

    // The rocket's entry is the total: getAerodynamicForces() but for the damping, and for a
    // drag coefficient summed in another order.
    const AerodynamicForces total =
        calculator.getAerodynamicForces(configuration, conditions, &warnings);
    const AerodynamicForces* const rocketEntry = forceMap.get(&rocket);
    if (rocketEntry == nullptr)
    {
        diff.problem("no entry for the rocket");
        return diff.text();
    }
    diff.pinned("rocket entry CN against the total", total.getCN(), rocketEntry->getCN(),
                kRocketTolerance);
    diff.pinned("rocket entry CD against the total", total.getCD(), rocketEntry->getCD(),
                kRocketTolerance);
    diff.pinned("rocket entry CDaxial against the total", total.getCDaxial(),
                rocketEntry->getCDaxial(), kRocketTolerance);
    return diff.text();
}

TEST(BarrowmanCalculator, ForceAnalysisOfTheAlphaIsOpenRockets)
{
    // One entry per active assembly and aerodynamic component; the internal components have
    // none. The stage's drag is that of its descendants, here all of the rocket's.
    static const std::array<AnalysisPin, 6> kPins{{
        {.index   = 0,
         .name    = "Estes Alpha III / Code Verification Rocket",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                     1.6590460045239195, 1.7628841169585023},
         .cdTotal = 1.6590460045239195},
        {.index   = 1,
         .name    = "Stage",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                     1.6590460045239195, 1.7628841169585023},
         .cdTotal = 1.6590460045239195},
        {.index   = 2,
         .name    = "Nose Cone",
         .forces  = {0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .drag    = {0.04740792268823329, 1.4210854715202004E-14, 0.0, 0.0, 0.0474079226882475,
                     0.05037513950620722},
         .cdTotal = 0.0474079226882475},
        {.index   = 3,
         .name    = "Body Tube",
         .forces  = {0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .drag    = {0.1996833052554658, 0.0, 0.1668, 0.0, 0.3664833052554658, 0.3894211469745897},
         .cdTotal = 0.3664833052554658},
        {.index   = 4,
         .name    = "3 Fin Set",
         .forces  = {2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849},
         .drag    = {0.058656323062898565, 0.28332445878911144, 0.058993432239395883, 0.0,
                     0.40097421409140593, 0.42607080900960403},
         .cdTotal = 1.2029226422742179},
        {.index   = 5,
         .name    = "Launch Lugs",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.04223213430598841, 0.0, 0.0, 0.04223213430598841, 0.04487540344889324},
         .cdTotal = 0.04223213430598841},
    }};
    const TestEstesAlphaIII                 alpha;
    EXPECT_EQ(analysisDifferences(*alpha.rocket, alpha.rocket->getSelectedConfiguration(), kPins),
              "");
}

TEST(BarrowmanCalculator, ForceAnalysisOfTheFalconIsOpenRockets)
{
    // The booster stage has two instances: its drag is that of one booster (each component's
    // drag times its instances over the stage's two), and getCDTotal() that of both. The core
    // stage's drag includes both boosters.
    static const std::array<AnalysisPin, 13> kPins{{
        {.index   = 0,
         .name    = "Falcon9H Scale Rocket",
         .forces  = {2.025775711955579, 19.645960897881977, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.0085913861645355, 0.0, 0.0, 23.213679707032846},
         .drag    = {0.5158993604358315, 0.08558740925800616, 0.29197580625514447, 0.0,
                     0.8934625759489822, 0.9493835493062612},
         .cdTotal = 0.8934625759489822},
        {.index   = 1,
         .name    = "Payload Fairing Stage",
         .forces  = {0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.020217577928807764, 0.0, 0.0, 1.6165308217107732},
         .drag    = {0.0753098413483292, 0.07536534763313608, 0.0, 0.0, 0.15067518898146529,
                     0.16010580583711367},
         .cdTotal = 0.15067518898146529},
        {.index   = 2,
         .name    = "PL Fairing Nose",
         .forces  = {0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .drag    = {0.01385036688869129, 0.0, 0.0, 0.0, 0.01385036688869129, 0.014717248186935249},
         .cdTotal = 0.01385036688869129},
        {.index  = 3,
         .name   = "PL Fairing Body",
         .forces = {0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .drag = {0.021864657833474462, 0.0, 0.0, 0.0, 0.021864657833474462, 0.023233145984053214},
         .cdTotal = 0.021864657833474462},
        {.index   = 4,
         .name    = "PL Fairing Transition",
         .forces  = {-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .drag    = {0.002803325079451613, 0.07536534763313608, 0.0, 0.0, 0.07816867271258769,
                     0.08306117563526645},
         .cdTotal = 0.07816867271258769},
        {.index   = 5,
         .name    = "Upper Stage Body",
         .forces  = {0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                     0.0, 0.15622264990921683},
         .drag    = {0.0220748949280271, 0.0, 0.0, 0.0, 0.0220748949280271, 0.02345654161851526},
         .cdTotal = 0.0220748949280271},
        {.index  = 8,
         .name   = "Interstage",
         .forces = {0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                    0.0, 0.10414843327281123},
         .drag = {0.014716596618684734, 0.0, 0.0, 0.0, 0.014716596618684734, 0.015637694412343507},
         .cdTotal = 0.014716596618684734},
        {.index   = 9,
         .name    = "Core Stage",
         .forces  = {1.8847067854614665, 19.618537128509594, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.082570443903521, 0.0, 0.0, 21.59714888532207},
         .drag    = {0.4405895190875022, 0.010222061624870073, 0.29197580625514447, 0.0,
                     0.7427873869675168, 0.7892777434691475},
         .cdTotal = 0.7427873869675168},
        {.index   = 10,
         .name    = "Core Stage Body",
         .forces  = {0.060591102380138984, 0.5616329105235963, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.9640000000000006, 0.0, 0.0, 0.6943228884854081},
         .drag    = {0.09811064412456491, 0.0, 0.0914346523668639, 0.0, 0.18954529649142882,
                     0.20140875642855774},
         .cdTotal = 0.18954529649142882},
        {.index   = 11,
         .name    = "Booster Stage",
         .forces  = {1.8241156830813274, 19.056904217986, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.086508962700575, 0.0, 0.0, 20.90282599683666},
         .drag    = {0.17123943748146864, 0.005111030812435037, 0.10027057694414028, 0.0,
                     0.276621045238044, 0.2939344935202949},
         .cdTotal = 0.553242090476088},
        {.index   = 12,
         .name    = "Booster Nose",
         .forces  = {0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.5243265366155166, 0.0, 0.0, 2.2824523458633212},
         .drag    = {0.0070252977181285755, 0.0, 0.0, 0.0, 0.0070252977181285755,
                     0.0074650044244840685},
         .cdTotal = 0.0070252977181285755},
        {.index   = 13,
         .name    = "Booster Body",
         .forces  = {0.12118220476027797, 1.1232658210471926, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.9640000000000006, 0.0, 0.0, 1.3886457769708163},
         .drag    = {0.09811064412456491, 0.0, 0.0914346523668639, 0.0, 0.18954529649142882,
                     0.20140875642855774},
         .cdTotal = 0.18954529649142882},
        {.index   = 15,
         .name    = "Booster Fins",
         .forces  = {1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523},
         .drag    = {0.022034498546258385, 0.0017036769374783455, 0.00294530819242546, 0.0,
                     0.02668348367616219, 0.02835357755575101},
         .cdTotal = 0.08005045102848657},
    }};
    const TestFalcon9Heavy                   falcon;
    EXPECT_EQ(analysisDifferences(*falcon.rocket, falcon.rocket->getSelectedConfiguration(), kPins),
              "");
}

TEST(BarrowmanCalculator, ForceAnalysisOfBoostersWithoutTheirCoreIsOpenRockets)
{
    static const std::array<AnalysisPin, 11> kPins{{
        {.index   = 0,
         .name    = "Falcon9H Scale Rocket",
         .forces  = {1.9651846095754395, 19.08432798735838, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.0099662398201172, 0.0, 0.0, 22.519356818547436},
         .drag    = {0.4177887163112665, 0.08558740925800616, 0.29197580625514447, 0.0,
                     0.7953519318244171, 0.845132253223971},
         .cdTotal = 0.7953519318244171},
        {.index   = 1,
         .name    = "Payload Fairing Stage",
         .forces  = {0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.020217577928807764, 0.0, 0.0, 1.6165308217107732},
         .drag    = {0.0753098413483292, 0.07536534763313608, 0.0914346523668639, 0.0,
                     0.2421098413483292, 0.25726326618338136},
         .cdTotal = 0.2421098413483292},
        {.index   = 2,
         .name    = "PL Fairing Nose",
         .forces  = {0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .drag    = {0.01385036688869129, 0.0, 0.0, 0.0, 0.01385036688869129, 0.014717248186935249},
         .cdTotal = 0.01385036688869129},
        {.index  = 3,
         .name   = "PL Fairing Body",
         .forces = {0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .drag = {0.021864657833474462, 0.0, 0.0, 0.0, 0.021864657833474462, 0.023233145984053214},
         .cdTotal = 0.021864657833474462},
        {.index   = 4,
         .name    = "PL Fairing Transition",
         .forces  = {-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .drag    = {0.002803325079451613, 0.07536534763313608, 0.0, 0.0, 0.07816867271258769,
                     0.08306117563526645},
         .cdTotal = 0.07816867271258769},
        {.index   = 5,
         .name    = "Upper Stage Body",
         .forces  = {0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                     0.0, 0.15622264990921683},
         .drag    = {0.0220748949280271, 0.0, 0.0, 0.0, 0.0220748949280271, 0.02345654161851526},
         .cdTotal = 0.0220748949280271},
        {.index   = 8,
         .name    = "Interstage",
         .forces  = {0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                     0.0, 0.10414843327281123},
         .drag    = {0.014716596618684734, 0.0, 0.0914346523668639, 0.0, 0.10615124898554865,
                     0.11279515475861118},
         .cdTotal = 0.10615124898554865},
        {.index   = 11,
         .name    = "Booster Stage",
         .forces  = {1.8241156830813274, 19.056904217986, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.086508962700575, 0.0, 0.0, 20.90282599683666},
         .drag    = {0.17123943748146864, 0.005111030812435037, 0.10027057694414028, 0.0,
                     0.276621045238044, 0.2939344935202949},
         .cdTotal = 0.553242090476088},
        {.index   = 12,
         .name    = "Booster Nose",
         .forces  = {0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.5243265366155166, 0.0, 0.0, 2.2824523458633212},
         .drag    = {0.0070252977181285755, 0.0, 0.0, 0.0, 0.0070252977181285755,
                     0.0074650044244840685},
         .cdTotal = 0.0070252977181285755},
        {.index   = 13,
         .name    = "Booster Body",
         .forces  = {0.12118220476027797, 1.1232658210471926, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.9640000000000006, 0.0, 0.0, 1.3886457769708163},
         .drag    = {0.09811064412456491, 0.0, 0.0914346523668639, 0.0, 0.18954529649142882,
                     0.20140875642855774},
         .cdTotal = 0.18954529649142882},
        {.index   = 15,
         .name    = "Booster Fins",
         .forces  = {1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523},
         .drag    = {0.022034498546258385, 0.0017036769374783455, 0.00294530819242546, 0.0,
                     0.02668348367616219, 0.02835357755575101},
         .cdTotal = 0.08005045102848657},
    }};
    const TestFalcon9Heavy                   falcon;
    FlightConfiguration&                     config = falcon.rocket->getSelectedConfiguration();
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);
    EXPECT_EQ(analysisDifferences(*falcon.rocket, config, kPins), "");
}

TEST(BarrowmanCalculator, ForceAnalysisWithOverridesInABoosterSetIsOpenRockets)
{
    // The booster set (0.2) and its fin set (0.05) are overridden: the fin set's CD is its
    // override (three fins on each of the two boosters make 0.3 of the override total), the
    // booster stage's override CD holds its own 0.2 and the fins' 0.15 per booster, and the
    // core stage's the 0.4 and 0.3 of both.
    static const std::array<AnalysisPin, 13> kPins{{
        {.index  = 0,
         .name   = "Falcon9H Scale Rocket",
         .forces = {2.025775711955579, 19.645960897881977, 0.0, 0.0, -0.8495278526554195,
                    0.8495278526554195, 0.0, 1.0085913861645355, 0.0, 0.0, 23.213679707032846},
         .drag = {0.3836923691582811, 0.07536534763313608, 0.27430395710059174, 0.7000000000000001,
                  1.433361673892009, 1.5230744185941854},
         .cdTotal = 1.433361673892009},
        {.index   = 1,
         .name    = "Payload Fairing Stage",
         .forces  = {0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.020217577928807764, 0.0, 0.0, 1.6165308217107732},
         .drag    = {0.0753098413483292, 0.07536534763313608, 0.0, 0.0, 0.15067518898146529,
                     0.16010580583711367},
         .cdTotal = 0.15067518898146529},
        {.index   = 2,
         .name    = "PL Fairing Nose",
         .forces  = {0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .drag    = {0.01385036688869129, 0.0, 0.0, 0.0, 0.01385036688869129, 0.014717248186935249},
         .cdTotal = 0.01385036688869129},
        {.index  = 3,
         .name   = "PL Fairing Body",
         .forces = {0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .drag = {0.021864657833474462, 0.0, 0.0, 0.0, 0.021864657833474462, 0.023233145984053214},
         .cdTotal = 0.021864657833474462},
        {.index   = 4,
         .name    = "PL Fairing Transition",
         .forces  = {-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .drag    = {0.002803325079451613, 0.07536534763313608, 0.0, 0.0, 0.07816867271258769,
                     0.08306117563526645},
         .cdTotal = 0.07816867271258769},
        {.index   = 5,
         .name    = "Upper Stage Body",
         .forces  = {0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                     0.0, 0.15622264990921683},
         .drag    = {0.0220748949280271, 0.0, 0.0, 0.0, 0.0220748949280271, 0.02345654161851526},
         .cdTotal = 0.0220748949280271},
        {.index  = 8,
         .name   = "Interstage",
         .forces = {0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                    0.0, 0.10414843327281123},
         .drag = {0.014716596618684734, 0.0, 0.0, 0.0, 0.014716596618684734, 0.015637694412343507},
         .cdTotal = 0.014716596618684734},
        {.index   = 9,
         .name    = "Core Stage",
         .forces  = {1.8847067854614665, 19.618537128509594, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.082570443903521, 0.0, 0.0, 21.59714888532207},
         .drag    = {0.30838252780995185, 0.0, 0.27430395710059174, 0.7000000000000001,
                     1.2826864849105437, 1.3629686127570717},
         .cdTotal = 1.2826864849105437},
        {.index   = 10,
         .name    = "Core Stage Body",
         .forces  = {0.060591102380138984, 0.5616329105235963, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.9640000000000006, 0.0, 0.0, 0.6943228884854081},
         .drag    = {0.09811064412456491, 0.0, 0.0914346523668639, 0.0, 0.18954529649142882,
                     0.20140875642855774},
         .cdTotal = 0.18954529649142882},
        {.index   = 11,
         .name    = "Booster Stage",
         .forces  = {1.8241156830813274, 19.056904217986, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.086508962700575, 0.0, 0.0, 20.90282599683666},
         .drag    = {0.10513594184269348, 0.0, 0.0914346523668639, 0.35000000000000003,
                     0.5465705942095574, 0.580779928164257},
         .cdTotal = 1.093141188419115},
        {.index   = 12,
         .name    = "Booster Nose",
         .forces  = {0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.5243265366155166, 0.0, 0.0, 2.2824523458633212},
         .drag    = {0.0070252977181285755, 0.0, 0.0, 0.0, 0.0070252977181285755,
                     0.0074650044244840685},
         .cdTotal = 0.0070252977181285755},
        {.index   = 13,
         .name    = "Booster Body",
         .forces  = {0.12118220476027797, 1.1232658210471926, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.9640000000000006, 0.0, 0.0, 1.3886457769708163},
         .drag    = {0.09811064412456491, 0.0, 0.0914346523668639, 0.0, 0.18954529649142882,
                     0.20140875642855774},
         .cdTotal = 0.18954529649142882},
        {.index   = 15,
         .name    = "Booster Fins",
         .forces  = {1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                     0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523},
         .drag    = {0.0, 0.0, 0.0, 0.30000000000000004, 0.05, 0.3187767148381845},
         .cdTotal = 0.15000000000000002},
    }};
    const TestFalcon9Heavy                   falcon;
    falcon.boosterStage->setCDOverridden(true);
    falcon.boosterStage->setOverrideCD(0.2);
    falcon.boosterFins->setCDOverridden(true);
    falcon.boosterFins->setOverrideCD(0.05);
    EXPECT_EQ(analysisDifferences(*falcon.rocket, falcon.rocket->getSelectedConfiguration(), kPins),
              "");
}

TEST(BarrowmanCalculator, ForceAnalysisWithAnOverriddenStageIsOpenRockets)
{
    static const std::array<AnalysisPin, 6> kStagePins{{
        {.index   = 0,
         .name    = "Estes Alpha III / Code Verification Rocket",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.2470912279436991, 0.042232134306002624, 0.1668, 0.71, 1.1661233622497016,
                     1.2391099150467264},
         .cdTotal = 1.1661233622497016},
        {.index   = 1,
         .name    = "Stage",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.2470912279436991, 0.042232134306002624, 0.1668, 0.71, 1.1661233622497016,
                     1.2391099150467264},
         .cdTotal = 1.1661233622497016},
        {.index   = 2,
         .name    = "Nose Cone",
         .forces  = {0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .drag    = {0.04740792268823329, 1.4210854715202004E-14, 0.0, 0.0, 0.0474079226882475,
                     0.05037513950620722},
         .cdTotal = 0.0474079226882475},
        {.index   = 3,
         .name    = "Body Tube",
         .forces  = {0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .drag    = {0.1996833052554658, 0.0, 0.1668, 0.0, 0.3664833052554658, 0.3894211469745897},
         .cdTotal = 0.3664833052554658},
        {.index   = 4,
         .name    = "3 Fin Set",
         .forces  = {2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849},
         .drag    = {0.0, 0.0, 0.0, 0.21000000000000002, 0.07, 0.22314370038672912},
         .cdTotal = 0.21000000000000002},
        {.index   = 5,
         .name    = "Launch Lugs",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.04223213430598841, 0.0, 0.0, 0.04223213430598841, 0.04487540344889324},
         .cdTotal = 0.04223213430598841},
    }};
    static const std::array<AnalysisPin, 6> kStageAndChildrenPins{{
        {.index   = 0,
         .name    = "Estes Alpha III / Code Verification Rocket",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.0, 0.0, 0.0, 0.5, 0.5, 0.5312945247303074},
         .cdTotal = 0.5},
        {.index   = 1,
         .name    = "Stage",
         .forces  = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag    = {0.0, 0.0, 0.0, 0.5, 0.5, 0.5312945247303074},
         .cdTotal = 0.5},
        {.index   = 2,
         .name    = "Nose Cone",
         .forces  = {0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .drag    = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .cdTotal = 0.0},
        {.index   = 3,
         .name    = "Body Tube",
         .forces  = {0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .drag    = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .cdTotal = 0.0},
        {.index   = 4,
         .name    = "3 Fin Set",
         .forces  = {2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                     0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849},
         .drag    = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .cdTotal = 0.0},
        {.index   = 5,
         .name    = "Launch Lugs",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .cdTotal = 0.0},
    }};
    const TestEstesAlphaIII                 alpha;
    alpha.stage->setCDOverridden(true);
    alpha.stage->setOverrideCD(0.5);
    alpha.fins->setCDOverridden(true);
    alpha.fins->setOverrideCD(0.07);
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    EXPECT_EQ(analysisDifferences(*alpha.rocket, config, kStagePins), "");

    alpha.stage->setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(analysisDifferences(*alpha.rocket, config, kStageAndChildrenPins), "");
}

TEST(BarrowmanCalculator, ForceAnalysisOfTheStepsRocketIsOpenRockets)
{
    static const std::array<AnalysisPin, 11> kPins{{
        {.index   = 0,
         .name    = "Steps",
         .forces  = {0.3490050523983985, 1.2712672624528079, 0.0, 0.0, -0.01907210359998241,
                     0.01907210359998241, 0.0, 0.2185528124105703, 0.0, 0.0, 3.9993033062340775},
         .drag    = {0.07909970829940467, 1.247683014737061, 0.28002822670463695, 0.0,
                     1.6068109497411027, 1.7073797197483058},
         .cdTotal = 1.6068109497411027},
        {.index   = 1,
         .name    = "Stage",
         .forces  = {0.3490050523983985, 1.2712672624528079, 0.0, 0.0, -0.01907210359998241,
                     0.01907210359998241, 0.0, 0.2185528124105703, 0.0, 0.0, 3.9993033062340775},
         .drag    = {0.07909970829940469, 1.247683014737061, 0.28002822670463695, 0.0,
                     1.6068109497411027, 1.7073797197483058},
         .cdTotal = 1.6068109497411027},
        {.index  = 2,
         .name   = "Nose Cone",
         .forces = {0.0810180557649291, 0.0540120371766194, 0.0, 0.0, 0.0, 0.0, 0.0, 0.04, 0.0, 0.0,
                    0.9283985319371972},
         .drag   = {0.008600174654944587, 0.04189393736182943, 0.0, 0.0, 0.05049411201677401,
                    0.053654490491261704},
         .cdTotal = 0.05049411201677401},
        {.index   = 3,
         .name    = "Tube A",
         .forces  = {0.011820948112257996, 0.021671738205806312, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.10999999999999993, 0.0, 0.0, 0.13545808733510417},
         .drag    = {0.02719614018487758, 0.0, 0.0, 0.0, 0.02719614018487758, 0.028898320748046692},
         .cdTotal = 0.02719614018487758},
        {.index   = 4,
         .name    = "Rail Button",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.02878881207390774, 0.0, 0.0, 0.02878881207390774, 0.030590676456713897},
         .cdTotal = 0.05757762414781548},
        {.index   = 5,
         .name    = "Disk",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.5162522222222222, 0.14826666666666669, 0.0, 0.6645188888888889,
                     0.7061104944930683},
         .cdTotal = 0.6645188888888889},
        {.index   = 6,
         .name    = "Tube B",
         .forces  = {0.005910474056128998, 0.020686659196451487, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.20999999999999994, 0.0, 0.0, 0.06772904366755209},
         .drag    = {0.01359807009243879, 0.0, 0.0, 0.0, 0.01359807009243879, 0.014449160374023346},
         .cdTotal = 0.01359807009243879},
        {.index   = 7,
         .name    = "Launch Lug",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.01003089922838556, 0.0, 0.0, 0.01003089922838556, 0.010658723676325426},
         .cdTotal = 0.01003089922838556},
        {.index   = 8,
         .name    = "Flat Transition",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.5420648333333334, 0.0, 0.0, 0.5420648333333334, 0.5759921559976933},
         .cdTotal = 0.5420648333333334},
        {.index   = 9,
         .name    = "Tube C",
         .forces  = {0.007388092570161248, 0.03509343970826592, 0.0, 0.0, 0.0, 0.0, 0.0, 0.285, 0.0,
                     0.0, 0.08466130458444011},
         .drag    = {0.016997587615548485, 0.0, 0.11583333333333336, 0.0, 0.13283092094888185,
                     0.14114468203005043},
         .cdTotal = 0.13283092094888185},
        {.index   = 10,
         .name    = "Trapezoidal Fin Set",
         .forces  = {0.24286748189492122, 1.1398033881656648, 0.0, 0.0, -0.01907210359998241,
                     0.01907210359998241, 0.0, 0.2815864962915399, 0.0, 0.0, 2.783056338709784},
         .drag    = {0.004235911917198415, 0.02662116614782488, 0.005309408901545629, 0.0,
                     0.03616648696656893, 0.03843011300813619},
         .cdTotal = 0.10849946089970677},
    }};
    const TestStepsRocket                    steps;
    EXPECT_EQ(analysisDifferences(*steps.rocket, steps.rocket->getSelectedConfiguration(), kPins),
              "");
}

TEST(BarrowmanCalculator, ForceAnalysisOfFinsOnPodsIsOpenRockets)
{
    static const std::array<AnalysisPin, 8> kPins{{
        {.index   = 0,
         .name    = "Estes Alpha III / Code Verification Rocket",
         .forces  = {2.1555196402000987, 19.593808025651025, 0.0, 0.0, -0.31562715446940026,
                     0.31562715446940026, 0.0, 0.21816149750877273, 0.0, 0.0, 24.700435608204682},
         .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                     1.6590460045239195, 1.7628841169585023},
         .cdTotal = 1.6590460045239195},
        {.index   = 1,
         .name    = "Stage",
         .forces  = {2.1555196402000987, 19.593808025651025, 0.0, 0.0, -0.31562715446940026,
                     0.31562715446940026, 0.0, 0.21816149750877273, 0.0, 0.0, 24.700435608204682},
         .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                     1.6590460045239195, 1.7628841169585023},
         .cdTotal = 1.6590460045239195},
        {.index   = 2,
         .name    = "Nose Cone",
         .forces  = {0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .drag    = {0.04740792268823329, 1.4210854715202004E-14, 0.0, 0.0, 0.0474079226882475,
                     0.05037513950620722},
         .cdTotal = 0.0474079226882475},
        {.index   = 3,
         .name    = "Body Tube",
         .forces  = {0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .drag    = {0.1996833052554658, 0.0, 0.1668, 0.0, 0.3664833052554658, 0.3894211469745897},
         .cdTotal = 0.3664833052554658},
        {.index   = 4,
         .name    = "Launch Lugs",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.04223213430598841, 0.0, 0.0, 0.04223213430598841, 0.04487540344889324},
         .cdTotal = 0.04223213430598841},
        {.index   = 9,
         .name    = "Pod Set",
         .forces  = {1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                     0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448},
         .drag    = {0.058656323062898565, 0.28332445878911144, 0.058993432239395883, 0.0,
                     0.40097421409140593, 0.42607080900960403},
         .cdTotal = 1.2029226422742179},
        {.index   = 10,
         .name    = "Pod Body",
         .forces  = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag    = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .cdTotal = 0.0},
        {.index   = 11,
         .name    = "3 Fin Set",
         .forces  = {1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                     0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448},
         .drag    = {0.058656323062898565, 0.28332445878911144, 0.058993432239395883, 0.0,
                     0.40097421409140593, 0.42607080900960403},
         .cdTotal = 0.40097421409140593},
    }};
    const TestEstesAlphaIIIWithPods         pods;
    EXPECT_EQ(analysisDifferences(*pods.rocket, pods.rocket->getSelectedConfiguration(), kPins),
              "");
}

// ------------------------------------------------------------------ rail buttons

// testRailButtonDrag as it was meant (ButtonsProbe.java): the rocket's events enabled, and the
// pair of buttons added to the tube.
TEST(BarrowmanCalculator, RailButtonsCountPerInstance)
{
    Rocket rocket;
    rocket.enableEvents();
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());

    // phantom tubes have no drag to confuse things
    auto phantomOwned = std::make_unique<BodyTube>();
    phantomOwned->setOuterRadius(0);
    BodyTube& phantom = stage.addChild(std::move(phantomOwned));

    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    FlightConditions           conditions{config};
    BarrowmanCalculator        calc;
    WarningSet                 warnings;
    JavaValueDifferences       diff;

    // two individual railbuttons
    auto button1Owned = std::make_unique<RailButton>();
    button1Owned->setInstanceCount(1);
    button1Owned->setAxialOffset(1.0);
    const RailButton& button1 = phantom.addChild(std::move(button1Owned));

    auto button2Owned = std::make_unique<RailButton>();
    button2Owned->setInstanceCount(1);
    button2Owned->setAxialOffset(2.0);
    const RailButton& button2 = phantom.addChild(std::move(button2Owned));

    const AerodynamicForces individualForces =
        calc.getAerodynamicForces(config, conditions, &warnings);
    compareDrag(
        diff, "individual",
        DragPin{0.0, 0.12162948062320347, 0.0, 0.0, 0.12162948062320347, 0.12162948062320347},
        individualForces, kRocketTolerance);

    // without them: no drag (the buttons are destroyed; the calculator must not go back to them)
    static_cast<void>(phantom.removeChild(&button1));
    static_cast<void>(phantom.removeChild(&button2));
    compareDrag(diff, "none", DragPin{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                calc.getAerodynamicForces(config, conditions, &warnings), kRocketTolerance);

    // a railbutton set with two instances at the same locations
    auto buttonsOwned = std::make_unique<RailButton>();
    buttonsOwned->setInstanceCount(2);
    buttonsOwned->setAxialOffset(1.0);
    buttonsOwned->setInstanceSeparation(1.0);
    phantom.addChild(std::move(buttonsOwned));

    const AerodynamicForces pairForces = calc.getAerodynamicForces(config, conditions, &warnings);
    compareDrag(
        diff, "pair",
        DragPin{0.0, 0.12162948062320347, 0.0, 0.0, 0.12162948062320347, 0.12162948062320347},
        pairForces, kRocketTolerance);
    EXPECT_NEAR(individualForces.getCD(), pairForces.getCD(), kEpsilon)
        << "two individual railbuttons should have same CD as a pair";

    // at Mach 0 the drag is that of Mach MathUtil.EPSILON
    conditions.setMach(QtRocket::MathUtil::kEpsilon);
    const AerodynamicForces epsForces = calc.getAerodynamicForces(config, conditions, &warnings);
    compareDrag(
        diff, "Mach epsilon",
        DragPin{0.0, 0.0016051373987999954, 0.0, 0.0, 0.0016051373987999954, 0.0016051373987999954},
        epsForces, kRocketTolerance);
    conditions.setMach(0);
    const AerodynamicForces zeroForces = calc.getAerodynamicForces(config, conditions, &warnings);
    compareDrag(
        diff, "Mach 0",
        DragPin{0.0, 0.0016051373987999954, 0.0, 0.0, 0.0016051373987999954, 0.0016051373987999954},
        zeroForces, kRocketTolerance);
    EXPECT_EQ(epsForces.getCD(), zeroForces.getCD())
        << "drag at mach 0 should equal drag at mach MathUtil.EPSILON";

    EXPECT_EQ(diff.text(), "");
    EXPECT_TRUE(warnings.empty());
}

// ------------------------------------------------------ lookup tables in place of a half

/// The stability table of the mixed calculators (CalcProbe.mixed()).
[[nodiscard]] MachAoALookup stabilityTable()
{
    return MachAoALookup::stabilityBuilder()
        .addStabilityData(0, 0, 0.10, 0.01, 0.50)
        .addStabilityData(0, 10, 0.20, 0.02, 0.55)
        .addStabilityData(1, 0, 0.30, 0.03, 0.60)
        .addStabilityData(1, 10, 0.40, 0.04, 0.65)
        .build()
        .value();
}

/// The drag table of the mixed calculators.
[[nodiscard]] MachAoALookup dragTable()
{
    return MachAoALookup::dragBuilder()
        .addDragData(0, 0, 0.40)
        .addDragData(0, 10, 0.50)
        .addDragData(1, 0, 0.60)
        .addDragData(1, 10, 0.80)
        .build()
        .value();
}

struct MixedPin
{
    int              index;
    std::string_view name;
    NonAxialPin      forces;
    DragPin          drag;
};

/// The differences between a calculator with a lookup table for a half (or both) and
/// OpenRocket's on the Estes Alpha III: the stall angle, the total forces, the CP and the force
/// analysis; and the same again for a newInstance() of it.
[[nodiscard]] std::string mixedDifferences(BarrowmanCalculator& calculator, double stallAngle,
                                           const TotalPin& total, const Coordinate& cp,
                                           std::span<const MixedPin> pins)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config     = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions = conditionsOf(config, kAnalysisConditions);
    WarningSet                 warnings;
    JavaValueDifferences       diff;

    const std::unique_ptr<AerodynamicCalculator> instance = calculator.newInstance();
    const std::array<AerodynamicCalculator*, 2>  calculators{&calculator, instance.get()};
    for (AerodynamicCalculator* const calc : calculators)
    {
        diff.pinned("stall angle", stallAngle, calc->getStallAngle(), 0.0);
        compareTotal(diff, "total", total,
                     calc->getAerodynamicForces(config, conditions, &warnings));
        const Coordinate actualCp = calc->getCP(config, conditions, &warnings);
        diff.pinned("CP.x", cp.x, actualCp.x, kRocketTolerance);
        diff.pinned("CNa", cp.weight, actualCp.weight, kRocketTolerance);

        const ForceMap forceMap = calc->getForceAnalysis(config, conditions, &warnings);
        if (forceMap.size() != pins.size())
        {
            diff.problem(
                std::format("{} entries, OpenRocket has {}", forceMap.size(), pins.size()));
            continue;
        }
        const std::vector<const RocketComponent*> components = allComponents(*alpha.rocket);
        for (const MixedPin& pin : pins)
        {
            const AerodynamicForces* const forces =
                forceMap.get(components.at(static_cast<std::size_t>(pin.index)));
            if (forces == nullptr)
            {
                diff.problem(std::format("no entry for {}", pin.name));
                continue;
            }
            compareNonAxial(diff, pin.name, pin.forces, *forces, kRocketTolerance);
            compareDrag(diff, pin.name, pin.drag, *forces, kRocketTolerance);
        }
    }
    return diff.text();
}

TEST(BarrowmanCalculator, WithATableForTheStabilityTheDragIsBarrowmans)
{
    // The table's CN, Cm and CP for the rocket (no damping), zero forces for every component,
    // and the Barrowman drag of each; the stall angle is the table's largest angle of attack.
    static const std::array<MixedPin, 6> kPins{{
        {.index  = 0,
         .name   = "Estes Alpha III / Code Verification Rocket",
         .forces = {0.27, 0.027000000000000003, 0.0, 0.0, 0.0, 0.0, 0.0, 0.585, 0.0, 0.0, 1.0},
         .drag   = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                    1.6590460045239195, 1.7628841169585023}},
        {.index  = 1,
         .name   = "Stage",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                    1.6590460045239195, 1.7628841169585023}},
        {.index  = 2,
         .name   = "Nose Cone",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.04740792268823329, 1.4210854715202004E-14, 0.0, 0.0, 0.0474079226882475,
                    0.05037513950620722}},
        {.index  = 3,
         .name   = "Body Tube",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.1996833052554658, 0.0, 0.1668, 0.0, 0.3664833052554658, 0.3894211469745897}},
        {.index  = 4,
         .name   = "3 Fin Set",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.058656323062898565, 0.28332445878911144, 0.058993432239395883, 0.0,
                    0.40097421409140593, 0.42607080900960403}},
        {.index  = 5,
         .name   = "Launch Lugs",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.04223213430598841, 0.0, 0.0, 0.04223213430598841, 0.04487540344889324}},
    }};
    BarrowmanCalculator                  calculator{
        std::make_unique<LookupTableStabilityCalculator>(stabilityTable()),
        std::make_unique<BarrowmanDragCalculator>()};
    EXPECT_EQ(
        mixedDifferences(
            calculator, 0.17453292519943295,
            {.forces  = {0.27, 0.027000000000000003, 0.0, 0.0, 0.0, 0.0, 0.0, 0.585, 0.0, 0.0, 1.0},
             .drag    = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                         1.6590460045239195, 1.7628841169585023},
             .damping = {0.0, 0.0}},
            Coordinate{0.585, 0.0, 0.0, 1.0}, kPins),
        "");
}

TEST(BarrowmanCalculator, WithATableForTheDragTheStabilityIsBarrowmans)
{
    // The Barrowman forces of each component and the damping moments, and the table's CD for
    // the rocket alone (as its friction drag), every component's drag zero.
    static const std::array<MixedPin, 6> kPins{{
        {.index  = 0,
         .name   = "Estes Alpha III / Code Verification Rocket",
         .forces = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                    0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag   = {0.6, 0.0, 0.0, 0.0, 0.6, 0.6375534296763689}},
        {.index  = 1,
         .name   = "Stage",
         .forces = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                    0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 2,
         .name   = "Nose Cone",
         .forces = {0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 3,
         .name   = "Body Tube",
         .forces = {0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 4,
         .name   = "3 Fin Set",
         .forces = {2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                    0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 5,
         .name   = "Launch Lugs",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    BarrowmanCalculator calculator{std::make_unique<BarrowmanStabilityCalculator>(),
                                   std::make_unique<LookupTableDragCalculator>(dragTable())};
    EXPECT_EQ(mixedDifferences(calculator, 0.30543261909900765,
                               {.forces  = {2.950182908075077, 27.42477505997564, 0.0, 0.0,
                                            -0.6133015126770895, 0.6133015126770895, 0.0,
                                            0.22310081221462436, 0.0, 0.0, 33.80660588486673},
                                .drag    = {0.6, 0.0, 0.0, 0.0, 0.6, 0.6375534296763689},
                                .damping = {-2.666027568008677E-4, 0.0}},
                               Coordinate{0.22310081221462436, 0.0, 0.0, 33.80660588486673}, kPins),
              "");
}

TEST(BarrowmanCalculator, WithTablesForBothNothingIsBarrowmans)
{
    static const std::array<MixedPin, 6> kPins{{
        {.index  = 0,
         .name   = "Estes Alpha III / Code Verification Rocket",
         .forces = {0.27, 0.027000000000000003, 0.0, 0.0, 0.0, 0.0, 0.0, 0.585, 0.0, 0.0, 1.0},
         .drag   = {0.6, 0.0, 0.0, 0.0, 0.6, 0.6375534296763689}},
        {.index  = 1,
         .name   = "Stage",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 2,
         .name   = "Nose Cone",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 3,
         .name   = "Body Tube",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 4,
         .name   = "3 Fin Set",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index  = 5,
         .name   = "Launch Lugs",
         .forces = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .drag   = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    BarrowmanCalculator                  calculator{
        std::make_unique<LookupTableStabilityCalculator>(stabilityTable()),
        std::make_unique<LookupTableDragCalculator>(dragTable())};
    EXPECT_EQ(mixedDifferences(calculator, 0.17453292519943295,
                               {.forces  = {0.27, 0.027000000000000003, 0.0, 0.0, 0.0, 0.0, 0.0,
                                            0.585, 0.0, 0.0, 1.0},
                                .drag    = {0.6, 0.0, 0.0, 0.0, 0.6, 0.6375534296763689},
                                .damping = {0.0, 0.0}},
                               Coordinate{0.585, 0.0, 0.0, 1.0}, kPins),
              "");
}

// NOLINTEND(modernize-use-std-numbers)

// ------------------------------------------------------------------- the calculator itself

/// What a calculator gives for a configuration, to compare two calculators exactly.
struct Snapshot
{
    AerodynamicForces forces;
    Coordinate        cp;
    std::size_t       analysisSize{0};
    double            analysisCD{0};
};

/// The forces, the CP and the force analysis of @p configuration by @p calculator, at Mach 0.6
/// and an angle of attack of 5 degrees.
[[nodiscard]] Snapshot snapshotOf(AerodynamicCalculator&     calculator,
                                  const FlightConfiguration& configuration)
{
    const FlightConditions conditions = conditionsOf(configuration, kAnalysisConditions);
    Snapshot               snapshot;
    snapshot.forces         = calculator.getAerodynamicForces(configuration, conditions, nullptr);
    snapshot.cp             = calculator.getCP(configuration, conditions, nullptr);
    const ForceMap forceMap = calculator.getForceAnalysis(configuration, conditions, nullptr);
    snapshot.analysisSize   = forceMap.size();
    if (const AerodynamicForces* const rocketEntry = forceMap.get(&configuration.getRocket()))
    {
        snapshot.analysisCD = rocketEntry->getCD();
    }
    return snapshot;
}

/// Whether two snapshots are the same, bit for bit ("" when they are).
[[nodiscard]] std::string differences(const Snapshot& expected, const Snapshot& actual)
{
    JavaValueDifferences diff;
    diff.pinned("CN", expected.forces.getCN(), actual.forces.getCN(), 0.0);
    diff.pinned("Cm", expected.forces.getCm(), actual.forces.getCm(), 0.0);
    diff.pinned("Croll", expected.forces.getCroll(), actual.forces.getCroll(), 0.0);
    diff.pinned("CD", expected.forces.getCD(), actual.forces.getCD(), 0.0);
    diff.pinned("CDaxial", expected.forces.getCDaxial(), actual.forces.getCDaxial(), 0.0);
    diff.pinned("CP.x", expected.cp.x, actual.cp.x, 0.0);
    diff.pinned("CNa", expected.cp.weight, actual.cp.weight, 0.0);
    diff.pinned("analysis CD", expected.analysisCD, actual.analysisCD, 0.0);
    if (expected.analysisSize != actual.analysisSize)
    {
        diff.problem(std::format("analysis size {} instead of {}", actual.analysisSize,
                                 expected.analysisSize));
    }
    return diff.text();
}

/// The differences between what @p calculator gives now and what a new calculator gives.
[[nodiscard]] std::string differencesFromANewCalculator(AerodynamicCalculator&     calculator,
                                                        const FlightConfiguration& configuration)
{
    BarrowmanCalculator fresh;
    return differences(snapshotOf(fresh, configuration), snapshotOf(calculator, configuration));
}

TEST(BarrowmanCalculator, DefaultCalculatorIsBarrowmansBoth)
{
    const BarrowmanCalculator calculator;
    EXPECT_EQ(calculator.getStallAngle(), BarrowmanStabilityCalculator::kStallAngle);
    // Java: getModID() is ModID.ZERO, always.
    EXPECT_EQ(calculator.modId(), ModId::zero());
    EXPECT_EQ(BarrowmanCalculator::calculateStagnationCD(1.5),
              BarrowmanDragCalculator::calculateStagnationCD(1.5));
    EXPECT_EQ(BarrowmanCalculator::calculateBaseCD(1.5),
              BarrowmanDragCalculator::calculateBaseCD(1.5));
    EXPECT_EQ(BarrowmanCalculator::calculateStagnationCD(0.3),
              BarrowmanDragCalculator::calculateStagnationCD(0.3));
    EXPECT_EQ(BarrowmanCalculator::calculateBaseCD(0.3),
              BarrowmanDragCalculator::calculateBaseCD(0.3));
}

TEST(BarrowmanCalculator, ANullCalculatorIsABug)
{
    // Java: IllegalArgumentException("Calculators must not be null")
    EXPECT_THROW((BarrowmanCalculator{nullptr, std::make_unique<BarrowmanDragCalculator>()}),
                 BugError);
    EXPECT_THROW((BarrowmanCalculator{std::make_unique<BarrowmanStabilityCalculator>(), nullptr}),
                 BugError);
    EXPECT_THROW((BarrowmanCalculator{nullptr, nullptr}), BugError);
}

TEST(BarrowmanCalculator, TheCacheIsVoidedWhenTheRocketChanges)
{
    // a calculation, a change to the rocket, the same calculation again
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    const Snapshot             before = snapshotOf(calculator, config);

    // an aerodynamic change
    alpha.fins->setFinCount(4);
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");
    EXPECT_NE(snapshotOf(calculator, config).cp.weight, before.cp.weight);

    // a tree change
    alpha.body->addChild(std::make_unique<RailButton>());
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");
    EXPECT_EQ(snapshotOf(calculator, config).analysisSize, before.analysisSize + 1);

    // and back
    alpha.fins->setFinCount(3);
    static_cast<void>(alpha.body->removeChild(alpha.body->getChildCount() - 1));
    EXPECT_EQ(differences(before, snapshotOf(calculator, config)), "");
}

TEST(BarrowmanCalculator, WithoutChangeEventsTheCacheStays)
{
    // As in OpenRocket: with the rocket's events disabled its modification ids do not change,
    // so the calculator goes on with the calculations it made (which hold the old geometry),
    // until the events are enabled again, which fires one. (The damping moments read the fin
    // set itself, so the pitching moment does follow the change.)
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    const Snapshot             before = snapshotOf(calculator, config);

    alpha.rocket->enableEvents(false);
    alpha.fins->setHeight(2 * alpha.fins->getHeight());
    const Snapshot stale = snapshotOf(calculator, config);
    EXPECT_TRUE(stale.cp.exactlyEquals(before.cp));
    EXPECT_EQ(stale.forces.getCN(), before.forces.getCN());
    EXPECT_EQ(stale.forces.getCD(), before.forces.getCD());
    EXPECT_EQ(stale.analysisCD, before.analysisCD);

    alpha.rocket->enableEvents();
    EXPECT_GT(snapshotOf(calculator, config).cp.weight, before.cp.weight);
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");
}

TEST(BarrowmanCalculator, ComponentsRemovedAndAddedBetweenCalculations)
{
    // Every removed component is destroyed at once: the calculator must drop what it kept of
    // it (AddressSanitizer is the judge), and the results must be those of a new calculator.
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");

    // the fins go, rail buttons come (their calculation keeps a pointer to them)
    static_cast<void>(alpha.body->removeChild(alpha.fins));
    auto buttons = std::make_unique<RailButton>();
    buttons->setInstanceCount(2);
    const RailButton& addedButtons = alpha.body->addChild(std::move(buttons));
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");

    // the buttons go and others come, quite possibly where the first ones were
    static_cast<void>(alpha.body->removeChild(&addedButtons));
    alpha.body->addChild(std::make_unique<RailButton>());
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");

    // the launch lug goes, new fins come
    static_cast<void>(alpha.body->removeChild(alpha.lug));
    alpha.body->addChild(std::make_unique<TrapezoidFinSet>(4, 0.04, 0.02, 0.01, 0.03));
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");

    // the whole body tube goes with all it holds, and a new one comes
    static_cast<void>(alpha.stage->removeChild(alpha.body));
    BodyTube& newBody = alpha.stage->addChild(std::make_unique<BodyTube>(0.15, 0.012, 0.0003));
    newBody.addChild(std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05));
    newBody.addChild(std::make_unique<RailButton>());
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");

    // and a stage
    AxialStage& booster = alpha.rocket->addChild(std::make_unique<AxialStage>());
    booster.addChild(std::make_unique<BodyTube>(0.1, 0.012, 0.0003));
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");
    static_cast<void>(alpha.rocket->removeChild(&booster));
    EXPECT_EQ(differencesFromANewCalculator(calculator, config), "");
}

TEST(BarrowmanCalculator, ComponentObjectsReplacedByAnUndoGetNewCalculations)
{
    // Rocket::loadFrom() (an undo) replaces every component object and restores the
    // modification ids of the loaded state. After a change of mass alone those are the
    // aerodynamic and tree ids the calculator saw last, so its cache is not voided: it must
    // still not go back to the destroyed components (a rail button's calculation reads its
    // button), and give the same forces.
    const TestEstesAlphaIII alpha;
    auto                    buttons = std::make_unique<RailButton>();
    buttons->setInstanceCount(2);
    alpha.body->addChild(std::move(buttons));
    Rocket&             rocket = *alpha.rocket;
    BarrowmanCalculator calculator;
    const Snapshot      before = snapshotOf(calculator, rocket.getSelectedConfiguration());

    const std::unique_ptr<Rocket> saved  = rocket.copyRocketWithOriginalId();
    const QtRocket::ModId         aeroId = rocket.getAerodynamicModId();
    const QtRocket::ModId         treeId = rocket.getTreeModId();
    alpha.chute->setOverrideMass(0.01);
    ASSERT_EQ(rocket.getAerodynamicModId(), aeroId);
    ASSERT_EQ(rocket.getTreeModId(), treeId);

    const RocketComponent* const oldStage = &rocket.getChild(0);
    rocket.loadFrom(*saved);
    ASSERT_NE(&rocket.getChild(0), oldStage);
    ASSERT_EQ(rocket.getAerodynamicModId(), aeroId);
    ASSERT_EQ(rocket.getTreeModId(), treeId);

    // (the fixture's pointers dangle now)
    EXPECT_EQ(differences(before, snapshotOf(calculator, rocket.getSelectedConfiguration())), "");
}

TEST(BarrowmanCalculator, ACopyOfTheRocketWithItsIdsGetsCalculationsOfItsOwn)
{
    // A copy with the original ids has the original's modification ids too: the calculator's
    // cache is not voided when it is given the copy, and the copy may outlive the original.
    TestEstesAlphaIII alpha;
    alpha.body->addChild(std::make_unique<RailButton>());
    BarrowmanCalculator calculator;
    const Snapshot      before = snapshotOf(calculator, alpha.rocket->getSelectedConfiguration());

    const std::unique_ptr<Rocket> copy = alpha.rocket->copyRocketWithOriginalId();
    EXPECT_EQ(differences(before, snapshotOf(calculator, copy->getSelectedConfiguration())), "");
    // ... and back to the original
    EXPECT_EQ(differences(before, snapshotOf(calculator, alpha.rocket->getSelectedConfiguration())),
              "");

    alpha.rocket.reset();
    EXPECT_EQ(differences(before, snapshotOf(calculator, copy->getSelectedConfiguration())), "");
}

TEST(BarrowmanCalculator, OneCalculatorForSeveralRockets)
{
    // Each rocket has modification ids of its own: going from one to another voids the cache.
    const TestEstesAlphaIII alpha;
    const TestFalcon9Heavy  falcon;
    const TestStepsRocket   steps;
    BarrowmanCalculator     calculator;
    for (int round = 0; round < 2; round++)
    {
        EXPECT_EQ(
            differencesFromANewCalculator(calculator, alpha.rocket->getSelectedConfiguration()),
            "");
        EXPECT_EQ(
            differencesFromANewCalculator(calculator, falcon.rocket->getSelectedConfiguration()),
            "");
        EXPECT_EQ(
            differencesFromANewCalculator(calculator, steps.rocket->getSelectedConfiguration()),
            "");
    }
}

TEST(BarrowmanCalculator, NewInstanceIsIndependent)
{
    const TestEstesAlphaIII    alpha;
    const TestFalcon9Heavy     falcon;
    const FlightConfiguration& alphaConfig  = alpha.rocket->getSelectedConfiguration();
    const FlightConfiguration& falconConfig = falcon.rocket->getSelectedConfiguration();
    BarrowmanCalculator        calculator;
    const Snapshot             alphaBefore = snapshotOf(calculator, alphaConfig);

    const std::unique_ptr<AerodynamicCalculator> instance = calculator.newInstance();
    ASSERT_NE(dynamic_cast<BarrowmanCalculator*>(instance.get()), nullptr);
    EXPECT_EQ(instance->getStallAngle(), calculator.getStallAngle());

    // The instance works on another rocket, and through a change of it, while the first
    // calculator's cache stays what it was.
    const Snapshot falconBefore = snapshotOf(*instance, falconConfig);
    EXPECT_EQ(differences(alphaBefore, snapshotOf(calculator, alphaConfig)), "");
    falcon.boosterFins->setFinCount(4);
    EXPECT_NE(snapshotOf(*instance, falconConfig).cp.weight, falconBefore.cp.weight);
    EXPECT_EQ(differencesFromANewCalculator(*instance, falconConfig), "");
    EXPECT_EQ(differences(alphaBefore, snapshotOf(calculator, alphaConfig)), "");

    // The original may go first.
    const std::unique_ptr<AerodynamicCalculator> second = instance->newInstance();
    EXPECT_EQ(differencesFromANewCalculator(*second, alphaConfig), "");
}

TEST(BarrowmanCalculator, WarningsAreOptional)
{
    // Body lift at supersonic speed warns; a null WarningSet drops the warning.
    const DefaultUnitsGuard    guard;
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    conditions.setMach(2.0);
    conditions.setAOA(0.05);
    BarrowmanCalculator calculator;

    WarningSet              warnings;
    const AerodynamicForces withWarnings =
        calculator.getAerodynamicForces(config, conditions, &warnings);
    EXPECT_TRUE(warnings.contains(Warning::kSupersonic));

    const AerodynamicForces withoutWarnings =
        calculator.getAerodynamicForces(config, conditions, nullptr);
    EXPECT_EQ(withoutWarnings.getCN(), withWarnings.getCN());
    EXPECT_EQ(withoutWarnings.getCD(), withWarnings.getCD());
    EXPECT_TRUE(calculator.getCP(config, conditions, nullptr)
                    .exactlyEquals(calculator.getCP(config, conditions, &warnings)));
    EXPECT_EQ(calculator.getForceAnalysis(config, conditions, nullptr).size(), 6U);

    // checkGeometry() too.
    alpha.nose->setAftRadius(0.015);
    EXPECT_NO_THROW(calculator.checkGeometry(config, *alpha.rocket, nullptr));
    WarningSet geometryWarnings;
    calculator.checkGeometry(config, *alpha.rocket, &geometryWarnings);
    EXPECT_EQ(geometryWarnings.size(), 1U);
}

}  // namespace
