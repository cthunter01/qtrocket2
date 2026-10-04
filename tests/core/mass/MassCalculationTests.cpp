#include "QtRocket/mass/MassCalculation.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::CMAnalysisEntry;
using QtRocket::CMAnalysisMap;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::MassCalculation;
using QtRocket::Motor;
using QtRocket::MotorClusterState;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::Transformation;
using QtRocket::Test::TestComponent;
using Type = QtRocket::MassCalculation::Type;

/// A rocket with a stage holding a body tube (0.5 m, radius 0.03 m) that holds an inner tube
/// motor mount (0.1 m, 0.4 m from the body's front) with an A8 in one flight configuration, and
/// a child of the body with two instances at +-0.02 m in y (a TestComponent of a prescribed
/// mass); events enabled.
class MassCalculationTest : public ::testing::Test
{
protected:
    /// The mass of the two-instance child, both instances together.
    static constexpr double kPairMass = 0.01;

    MassCalculationTest()
    {
        m_rocket.createFlightConfiguration(m_fcid);
        m_stage    = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body     = &m_stage->addChild(std::make_unique<BodyTube>(0.5, 0.03));
        auto mount = std::make_unique<InnerTube>();
        mount->setAxialMethod(AxialMethod::TOP);
        mount->setAxialOffset(0.4);
        mount->setLength(0.1);
        mount->setOuterRadius(0.01);
        mount->setMotorMount(true);
        m_mount = &m_body->addChild(std::move(mount));
        m_motor = QtRocket::Test::motorA8();
        QtRocket::Test::addMotor(*m_mount, m_fcid, m_motor);
        m_pair = &m_body->addChild(
            TestComponent::make(0.05, ComponentKind::LAUNCH_LUG, AxialMethod::TOP));
        m_pair->setAxialOffset(AxialMethod::TOP, 0.1);
        m_pair->setInstances({Coordinate{0, 0.02, 0}, Coordinate{0, -0.02, 0}}, {0, 0});
        m_pair->setMass(kPairMass);
        m_pair->setCG(Coordinate{0.025});
        m_rocket.setSelectedConfiguration(m_fcid);
        m_rocket.enableEvents();
    }

    /// The mass of the body tube alone (its CG is its middle, 0.25 m).
    [[nodiscard]] double bodyMass() const { return m_body->getComponentMass(); }

    /// The mass of the motor mount tube alone (its CG is its middle, 0.45 m from the nose).
    [[nodiscard]] double mountMass() const { return m_mount->getComponentMass(); }

    /// The mass of the structure: the body, the mount and the pair.
    [[nodiscard]] double structureMass() const { return bodyMass() + mountMass() + kPairMass; }

    /// The x of the structure's CG (the pair's own CG, 0.1 + 0.025, holds both instances).
    [[nodiscard]] double structureCmx() const
    {
        return ((bodyMass() * 0.25) + (mountMass() * 0.45) + (kPairMass * 0.125)) / structureMass();
    }

    [[nodiscard]] FlightConfiguration& config() { return m_rocket.getFlightConfiguration(m_fcid); }

    [[nodiscard]] MassCalculation calculation(Type type, CMAnalysisMap* map = nullptr)
    {
        return MassCalculation{type,         config(), Motor::kPseudoTimeLaunch,
                               std::nullopt, m_rocket, Transformation::kIdentity,
                               map};
    }

    Rocket                                  m_rocket;
    FlightConfigurationId                   m_fcid{QtRocket::Test::testFcid(0)};
    AxialStage*                             m_stage{nullptr};
    BodyTube*                               m_body{nullptr};
    InnerTube*                              m_mount{nullptr};
    TestComponent*                          m_pair{nullptr};
    std::shared_ptr<const ThrustCurveMotor> m_motor;
};

// =================================================================================== types

TEST(MassCalculationType, FlagsAreJavas)
{
    EXPECT_TRUE(MassCalculation::includesStructure(Type::STRUCTURE));
    EXPECT_FALSE(MassCalculation::includesMotorCasing(Type::STRUCTURE));
    EXPECT_FALSE(MassCalculation::includesPropellant(Type::STRUCTURE));

    EXPECT_FALSE(MassCalculation::includesStructure(Type::MOTOR));
    EXPECT_TRUE(MassCalculation::includesMotorCasing(Type::MOTOR));
    EXPECT_TRUE(MassCalculation::includesPropellant(Type::MOTOR));

    EXPECT_TRUE(MassCalculation::includesStructure(Type::BURNOUT));
    EXPECT_TRUE(MassCalculation::includesMotorCasing(Type::BURNOUT));
    EXPECT_FALSE(MassCalculation::includesPropellant(Type::BURNOUT));

    EXPECT_TRUE(MassCalculation::includesStructure(Type::LAUNCH));
    EXPECT_TRUE(MassCalculation::includesMotorCasing(Type::LAUNCH));
    EXPECT_TRUE(MassCalculation::includesPropellant(Type::LAUNCH));
}

TEST(MassCalculationType, NamesInDeclarationOrder)
{
    ASSERT_EQ(MassCalculation::kAllTypes.size(), 4U);
    EXPECT_EQ(name(MassCalculation::kAllTypes[0]), "STRUCTURE");
    EXPECT_EQ(name(MassCalculation::kAllTypes[1]), "MOTOR");
    EXPECT_EQ(name(MassCalculation::kAllTypes[2]), "BURNOUT");
    EXPECT_EQ(name(MassCalculation::kAllTypes[3]), "LAUNCH");
    EXPECT_EQ(MassCalculation::kMinMass, 1e-8);
}

// ============================================================================ accumulation

TEST_F(MassCalculationTest, StartsEmpty)
{
    const MassCalculation calc = calculation(Type::LAUNCH);
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(calc.getMass(), 0.0);
    EXPECT_EQ(calc.size(), 0U);
    EXPECT_EQ(calc.getType(), Type::LAUNCH);
    EXPECT_EQ(&calc.getConfig(), &config());
    EXPECT_EQ(&calc.getRoot(), static_cast<const QtRocket::RocketComponent*>(&m_rocket));
    EXPECT_TRUE(calc.getTransform().isIdentity());
    EXPECT_FALSE(calc.getActiveMotors().has_value());
    EXPECT_EQ(calc.getAnalysisMap(), nullptr);
    EXPECT_EQ(calc.getSimulationTime(), Motor::kPseudoTimeLaunch);
}

TEST_F(MassCalculationTest, FirstPointMassReplacesTheEmptyCenter)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.addMass(Coordinate{1.0, 2.0, 3.0, 0.5});
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate{1.0, 2.0, 3.0, 0.5}));

    calc.addMass(Coordinate{3.0, 0.0, 0.0, 1.5});
    EXPECT_DOUBLE_EQ(calc.getMass(), 2.0);
    EXPECT_DOUBLE_EQ(calc.getCM().x, ((1.0 * 0.5) + (3.0 * 1.5)) / 2.0);
    EXPECT_DOUBLE_EQ(calc.getCM().y, 0.5);
}

TEST_F(MassCalculationTest, MassBelowTheMinimumCountsAsEmpty)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.addMass(Coordinate{5.0, 0.0, 0.0, 0.5e-8});
    // Still "empty": the next point mass replaces the centre (Java: MIN_MASS > weight).
    calc.addMass(Coordinate{1.0, 0.0, 0.0, 0.0});
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate{1.0, 0.0, 0.0, 0.0}));
}

TEST_F(MassCalculationTest, AddMassAndSetters)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.setCM(Coordinate{0.5, 0.0, 0.0, 1.0});
    calc.addMass(0.25);
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate{0.5, 0.0, 0.0, 1.25}));
    calc.setMass(2.0);
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate{0.5, 0.0, 0.0, 2.0}));

    calc.addInertia(RigidBody{Coordinate{0.5, 0, 0, 2.0}, 1.0, 2.0});
    EXPECT_EQ(calc.size(), 1U);
    calc.reset();
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(calc.size(), 0U);
}

TEST_F(MassCalculationTest, InertiaGettersReadJavasNeverSetField)
{
    MassCalculation calc = calculation(Type::LAUNCH);
    calc.calculateAssembly();
    ASSERT_GT(calc.getMass(), 0.0);
    EXPECT_EQ(calc.getLongitudinalInertia(), 0.0);
    EXPECT_EQ(calc.getRotationalInertia(), 0.0);
    EXPECT_GT(calc.calculateMomentOfInertia().getLongitudinalInertia(), 0.0);
}

TEST_F(MassCalculationTest, MergeAddsTheCenterAndTheBodies)
{
    MassCalculation first = calculation(Type::STRUCTURE);
    first.addMass(Coordinate{0.0, 0.0, 0.0, 1.0});
    first.addInertia(RigidBody{Coordinate{0.0, 0.0, 0.0, 1.0}, 0.1, 0.2});

    MassCalculation second = calculation(Type::STRUCTURE);
    second.addMass(Coordinate{2.0, 0.0, 0.0, 1.0});
    second.addInertia(RigidBody{Coordinate{2.0, 0.0, 0.0, 1.0}, 0.3, 0.4});

    first.merge(second);
    EXPECT_DOUBLE_EQ(first.getMass(), 2.0);
    EXPECT_DOUBLE_EQ(first.getCM().x, 1.0);
    ASSERT_EQ(first.size(), 2U);
    EXPECT_EQ(first.getBodies()[1].getIxx(), 0.3);

    // Each body moved to the common centre: I + m d^2 with d = 1 for both.
    const RigidBody total = first.calculateMomentOfInertia();
    EXPECT_DOUBLE_EQ(total.getIxx(), 0.1 + 0.3);
    EXPECT_DOUBLE_EQ(total.getIyy(), 0.2 + 1.0 + 0.4 + 1.0);
    EXPECT_EQ(total.getIzz(), total.getIyy());
    EXPECT_TRUE(total.getCM().exactlyEquals(first.getCM()));
}

TEST_F(MassCalculationTest, ScaleInertiaScalesEveryBody)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.addInertia(RigidBody{Coordinate{1.0, 0.0, 0.0, 2.0}, 0.5, 0.25});
    calc.addInertia(RigidBody{Coordinate{3.0, 0.0, 0.0, 4.0}, 1.0, 2.0});
    calc.scaleInertia(0.5);
    EXPECT_EQ(calc.getBodies()[0].getMass(), 1.0);
    EXPECT_EQ(calc.getBodies()[0].getIxx(), 0.25);
    EXPECT_EQ(calc.getBodies()[1].getMass(), 2.0);
    EXPECT_EQ(calc.getBodies()[1].getIyy(), 1.0);
    EXPECT_EQ(calc.getBodies()[1].getCM().x, 3.0);
}

TEST_F(MassCalculationTest, CopyKeepsTheParametersAndStartsEmpty)
{
    CMAnalysisMap   map;
    MassCalculation calc = calculation(Type::BURNOUT, &map);
    calc.addMass(Coordinate{1.0, 0.0, 0.0, 1.0});
    const Transformation  moved = Transformation::translation(1.0, 2.0, 3.0);
    const MassCalculation copy  = calc.copy(*m_body, moved);

    EXPECT_EQ(copy.getType(), Type::BURNOUT);
    EXPECT_EQ(&copy.getConfig(), &calc.getConfig());
    EXPECT_EQ(copy.getSimulationTime(), calc.getSimulationTime());
    EXPECT_EQ(copy.getAnalysisMap(), &map);
    EXPECT_EQ(&copy.getRoot(), static_cast<const QtRocket::RocketComponent*>(m_body));
    EXPECT_EQ(copy.getTransform(), moved);
    EXPECT_EQ(copy.getMass(), 0.0);
    EXPECT_EQ(copy.size(), 0U);
}

TEST_F(MassCalculationTest, EqualityAndDebugString)
{
    MassCalculation a = calculation(Type::LAUNCH);
    MassCalculation b = calculation(Type::LAUNCH);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a == a);

    a.addMass(Coordinate{0.123456789, -0.5, 0.0, 0.0164});
    EXPECT_FALSE(a == b);
    b.addMass(Coordinate{0.123456789, -0.5, 0.0, 0.0164});
    EXPECT_TRUE(a == b);

    const MassCalculation other = calculation(Type::STRUCTURE);
    EXPECT_FALSE(calculation(Type::LAUNCH) == other);

    EXPECT_EQ(a.toCMDebug(), "cm= 0.016400g@[0.123457,-0.500000,0.000000]");
    EXPECT_EQ(a.toString(), a.toCMDebug());
}

TEST_F(MassCalculationTest, EqualityComparesTheTimeAndTheConfigurationToo)
{
    const MassCalculation launch = calculation(Type::LAUNCH);

    // Another time.
    const MassCalculation later{
        Type::LAUNCH, config(), 1.0, std::nullopt, m_rocket, Transformation::kIdentity, nullptr};
    EXPECT_FALSE(launch == later);
    EXPECT_FALSE(later == launch);

    // Another configuration: Java's FlightConfiguration.equals compares the ids.
    const FlightConfigurationId otherId = QtRocket::Test::testFcid(1);
    m_rocket.createFlightConfiguration(otherId);
    const MassCalculation other{Type::LAUNCH,
                                m_rocket.getFlightConfiguration(otherId),
                                Motor::kPseudoTimeLaunch,
                                std::nullopt,
                                m_rocket,
                                Transformation::kIdentity,
                                nullptr};
    EXPECT_FALSE(launch == other);

    // The root, the transformation and the bodies do not count.
    MassCalculation moved = launch.copy(*m_body, Transformation::translation(1.0, 0.0, 0.0));
    moved.addInertia(RigidBody{Coordinate{0.0, 0.0, 0.0, 1.0}, 1.0, 1.0});
    EXPECT_TRUE(launch == moved);
}

TEST_F(MassCalculationTest, HashCodeIsTheCentersOfMass)
{
    MassCalculation calc = calculation(Type::LAUNCH);
    EXPECT_EQ(calc.hashCode(), 0);

    // Java: (int)((x + y + z) * 100000) of the centre of mass; the weight does not count.
    calc.setCM(Coordinate{0.25, -0.5, 0.125, 3.0});
    EXPECT_EQ(calc.hashCode(), -12500);
    EXPECT_EQ(calc.hashCode(), static_cast<std::int32_t>(std::hash<Coordinate>{}(calc.getCM())));
    calc.setMass(7.0);
    EXPECT_EQ(calc.hashCode(), -12500);
    EXPECT_EQ(std::hash<MassCalculation>{}(calc), static_cast<std::size_t>(calc.hashCode()));
}

TEST_F(MassCalculationTest, EqualCalculationsHashAlike)
{
    MassCalculation calc = calculation(Type::LAUNCH);
    calc.setCM(Coordinate{0.25, -0.5, 0.125, 7.0});
    const MassCalculation same = [&] {
        MassCalculation result = calculation(Type::LAUNCH);
        result.setCM(Coordinate{0.25, -0.5, 0.125, 7.0});
        return result;
    }();
    ASSERT_TRUE(calc == same);
    EXPECT_EQ(calc.hashCode(), same.hashCode());

    // A NaN centre gives Java's (int) NaN, 0.
    calc.setCM(Coordinate::kNaN);
    EXPECT_EQ(calc.hashCode(), 0);
}

TEST_F(MassCalculationTest, MergingACalculationIntoItselfDoublesIt)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.addMass(Coordinate{0.5, 0.0, 0.0, 1.0});
    calc.addInertia(RigidBody{Coordinate{0.5, 0.0, 0.0, 1.0}, 0.1, 0.2});
    calc.addInertia(RigidBody{Coordinate{0.5, 0.0, 0.0, 0.5}, 0.3, 0.4});

    // Java: addMass(this.centerOfMass) averages the centre with itself, and
    // bodies.addAll(bodies) appends a copy of the list.
    calc.merge(calc);
    EXPECT_TRUE(calc.getCM().exactlyEquals(Coordinate{0.5, 0.0, 0.0, 2.0}));
    ASSERT_EQ(calc.size(), 4U);
    EXPECT_EQ(calc.getBodies()[2].getIxx(), 0.1);
    EXPECT_EQ(calc.getBodies()[3].getIyy(), 0.4);
}

/// The calculation keeps references to its configuration, root and motor states, so the
/// constructor rejects temporaries of them (and copy() a temporary root); spans and lvalues pass.
using Motors     = std::optional<std::span<const MotorClusterState* const>>;
using PointerVec = std::vector<const MotorClusterState*>;
static_assert(std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                      Motors, const QtRocket::RocketComponent&,
                                      const Transformation&, CMAnalysisMap*>);
static_assert(std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                      std::nullopt_t, const QtRocket::RocketComponent&,
                                      const Transformation&, CMAnalysisMap*>);
static_assert(std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                      PointerVec&, const QtRocket::RocketComponent&,
                                      const Transformation&, CMAnalysisMap*>);
static_assert(std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                      const PointerVec&, const QtRocket::RocketComponent&,
                                      const Transformation&, CMAnalysisMap*>);
static_assert(std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                      std::span<const MotorClusterState* const>,
                                      const QtRocket::RocketComponent&, const Transformation&,
                                      CMAnalysisMap*>);
static_assert(!std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                       PointerVec, const QtRocket::RocketComponent&,
                                       const Transformation&, CMAnalysisMap*>);
static_assert(!std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                                       const PointerVec, const QtRocket::RocketComponent&,
                                       const Transformation&, CMAnalysisMap*>);
static_assert(
    !std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double,
                             std::vector<MotorClusterState*>, const QtRocket::RocketComponent&,
                             const Transformation&, CMAnalysisMap*>);
static_assert(!std::is_constructible_v<MassCalculation, Type, FlightConfiguration, double, Motors,
                                       const QtRocket::RocketComponent&, const Transformation&,
                                       CMAnalysisMap*>);
static_assert(
    !std::is_constructible_v<MassCalculation, Type, const FlightConfiguration&, double, Motors,
                             TestComponent, const Transformation&, CMAnalysisMap*>);

/// Whether copy() takes a root of type @p Root.
template <typename Root>
concept CopyTakes = requires(const MassCalculation& calc, Root&& root) {
    calc.copy(std::forward<Root>(root), Transformation::kIdentity);
};
static_assert(CopyTakes<const QtRocket::RocketComponent&>);
static_assert(CopyTakes<TestComponent&>);
static_assert(!CopyTakes<TestComponent>);

// ============================================================================== calculation

TEST_F(MassCalculationTest, StructurePlacesChildInstancesThroughTheTransforms)
{
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.calculateStructure();

    // The body tube at 0.25, the mount at 0.4 + 0.05, the pair at 0.1 + 0.025 (its own CG holds
    // both instances). Bodies: rocket, stage, body, mount, pair.
    ASSERT_GT(bodyMass(), 0.0);
    ASSERT_GT(mountMass(), 0.0);
    EXPECT_NEAR(calc.getMass(), structureMass(), 1e-15);
    EXPECT_NEAR(calc.getCM().x, structureCmx(), 1e-15);
    EXPECT_NEAR(calc.getCM().y, 0.0, 1e-15);
    EXPECT_NEAR(calc.getCM().z, 0.0, 1e-15);
    EXPECT_EQ(calc.size(), 5U);
}

TEST_F(MassCalculationTest, ChildrenFollowTheirParentsInstancesAndAngles)
{
    // A child of the two-instance pair is placed at each instance, rotated by its angle.
    m_pair->setInstances({Coordinate{0, 0.02, 0}, Coordinate{0, 0.02, 0}},
                         {0, std::numbers::pi / 2});
    auto& child = m_pair->addChild(TestComponent::make(0.01, ComponentKind::MASS_COMPONENT));
    child.setMass(0.5);
    child.setCG(Coordinate{0.0, 0.01, 0.0});
    config().update();

    MassCalculation structure = calculation(Type::STRUCTURE);
    structure.calculateStructure();
    // The child's CG (0.01 in y) sits at y = 0.02 + 0.01 for instance 0 and, rotated by 90
    // degrees about x, at y = 0.02, z = 0.01 for instance 1: 0.5 kg each.
    const double childX     = 0.1 + 0.0;  // the pair's front plus the child's position (AFTER)
    const double totalMass  = structureMass() + (2 * 0.5);
    const double expectedY  = ((0.5 * 0.03) + (0.5 * 0.02)) / totalMass;
    const double expectedZ  = (0.5 * 0.01) / totalMass;
    const double expectedCx = ((structureMass() * structureCmx()) + (1.0 * childX)) / totalMass;
    EXPECT_NEAR(structure.getMass(), totalMass, 1e-15);
    EXPECT_NEAR(structure.getCM().x, expectedCx, 1e-15);
    EXPECT_NEAR(structure.getCM().y, expectedY, 1e-15);
    EXPECT_NEAR(structure.getCM().z, expectedZ, 1e-15);
}

TEST_F(MassCalculationTest, InactiveComponentsAddNothing)
{
    config().clearAllStages();
    MassCalculation calc = calculation(Type::LAUNCH);
    calc.calculateAssembly();
    // Only the rocket itself (no mass, at the origin) remains.
    EXPECT_EQ(calc.getMass(), 0.0);
    EXPECT_EQ(calc.size(), 1U);
}

TEST_F(MassCalculationTest, AssemblyRunsTheStructureAndMotorPasses)
{
    MassCalculation structure = calculation(Type::STRUCTURE);
    structure.calculateAssembly();
    MassCalculation motor = calculation(Type::MOTOR);
    motor.calculateAssembly();
    MassCalculation launch = calculation(Type::LAUNCH);
    launch.calculateAssembly();

    EXPECT_DOUBLE_EQ(motor.getMass(), 0.0164);
    EXPECT_EQ(motor.size(), 1U);
    EXPECT_DOUBLE_EQ(launch.getMass(), structure.getMass() + motor.getMass());
    EXPECT_EQ(launch.size(), structure.size() + motor.size());
}

TEST_F(MassCalculationTest, MotorPassWithoutCasingIsThePropellantAlone)
{
    // Java's third branch: a type without the casings, reachable through calculateMotors().
    MassCalculation calc = calculation(Type::STRUCTURE);
    calc.calculateMotors();
    const double propellant = m_motor->getTotalMass(0.0) - m_motor->getBurnoutMass();
    EXPECT_DOUBLE_EQ(calc.getMass(), propellant);
    const double cmx = ((m_motor->getCMx(0.0) * m_motor->getTotalMass(0.0)) -
                        (m_motor->getBurnoutCGx() * m_motor->getBurnoutMass())) /
                       propellant;
    EXPECT_DOUBLE_EQ(calc.getCM().x, 0.4 + (0.1 - 0.07) + cmx);
}

TEST_F(MassCalculationTest, MotorPassUsesEachStatesMotorTime)
{
    std::vector<MotorClusterState> states;
    states.emplace_back(m_mount->getMotorConfig(m_fcid));
    states.front().ignite(1.0);
    const std::vector<const MotorClusterState*> pointers{&states.front()};

    MassCalculation calc{Type::MOTOR, config(),
                         1.5,         std::span<const MotorClusterState* const>{pointers},
                         m_rocket,    Transformation::kIdentity,
                         nullptr};
    calc.calculateMotors();
    EXPECT_EQ(calc.getMass(), m_motor->getTotalMass(0.5));

    // An empty (but present) list means no motors, unlike a static calculation.
    MassCalculation none{Type::MOTOR, config(),
                         1.5,         std::span<const MotorClusterState* const>{},
                         m_rocket,    Transformation::kIdentity,
                         nullptr};
    none.calculateMotors();
    EXPECT_EQ(none.getMass(), 0.0);
}

TEST_F(MassCalculationTest, MountWithoutMotorAddsNothing)
{
    m_mount->setMotorConfig(std::nullopt, m_fcid);
    config().update();
    MassCalculation calc = calculation(Type::MOTOR);
    calc.calculateMotors();
    EXPECT_EQ(calc.getMass(), 0.0);
    EXPECT_EQ(calc.size(), 0U);
}

TEST_F(MassCalculationTest, AnalysisRowsForComponentsAssembliesAndMotors)
{
    CMAnalysisMap   map;
    MassCalculation calc = calculation(Type::LAUNCH, &map);
    calc.calculateAssembly();

    // Rows for the rocket, the stage, the body, the mount, the pair and the motor.
    EXPECT_EQ(map.size(), 6U);

    const CMAnalysisEntry& pair = map.at(CMAnalysisEntry::keyOf(*m_pair));
    EXPECT_DOUBLE_EQ(pair.eachMass, kPairMass);
    EXPECT_DOUBLE_EQ(pair.totalCM.x, 0.125);

    const CMAnalysisEntry& body = map.at(CMAnalysisEntry::keyOf(*m_body));
    EXPECT_DOUBLE_EQ(body.eachMass, bodyMass());  // the body alone, not its children
    EXPECT_DOUBLE_EQ(body.totalCM.x, 0.25);

    const CMAnalysisEntry& mount = map.at(CMAnalysisEntry::keyOf(*m_mount));
    EXPECT_DOUBLE_EQ(mount.eachMass, mountMass());  // the tube alone, not its motor
    EXPECT_DOUBLE_EQ(mount.totalCM.x, 0.45);

    const CMAnalysisEntry& motor = map.at(CMAnalysisEntry::keyOf(*m_motor));
    EXPECT_EQ(motor.getMotor(), m_motor.get());
    EXPECT_DOUBLE_EQ(motor.eachMass, 0.0164);

    // The stage holds its structure and the motor below it.
    const CMAnalysisEntry& stage = map.at(CMAnalysisEntry::keyOf(*m_stage));
    EXPECT_NEAR(stage.totalCM.weight, structureMass() + 0.0164, 1e-15);
    EXPECT_NEAR(stage.eachMass, structureMass() + 0.0164, 1e-15);
    EXPECT_NEAR(stage.totalCM.x, calc.getCM().x, 1e-15);

    // Without a map nothing is recorded, and STRUCTURE records no motors.
    CMAnalysisMap   structureMap;
    MassCalculation structure = calculation(Type::STRUCTURE, &structureMap);
    structure.calculateAssembly();
    EXPECT_EQ(structureMap.size(), 5U);
    EXPECT_FALSE(structureMap.contains(CMAnalysisEntry::keyOf(*m_motor)));
}

}  // namespace
