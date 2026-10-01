#include "QtRocket/mass/MassCalculator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MassCalculation.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestComponent.h"
#include "rocket/TestMotorMount.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::CMAnalysisEntry;
using QtRocket::CMAnalysisMap;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::MassCalculation;
using QtRocket::MassCalculator;
using QtRocket::ModId;
using QtRocket::Motor;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfiguration;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ThrustCurveMotor;
using QtRocket::Test::TestBodyComponent;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMotorMount;
using Json = nlohmann::json;

// tolerance for compared double test results (MassCalculatorTest.EPSILON, MathUtil's precision)
constexpr double kEpsilon = 0.00000001;

/// TestRockets.FALCON_9H_*_STAGE_NUMBER.
constexpr int kFalcon9hPayloadStageNumber = 0;
constexpr int kFalcon9hCoreStageNumber    = 1;
constexpr int kFalcon9hBoosterStageNumber = 2;

// ================================================================================ golden data

/// Reads tests/data/goldens/<input>/<file>.
/// @throws std::runtime_error when it cannot be read.
Json loadGolden(const std::string& input, const std::string& file)
{
    auto loaded = QtRocket::Test::loadGoldenJson(std::filesystem::path{input} / file);
    if (!loaded)
    {
        throw std::runtime_error(loaded.error().toString());
    }
    return std::move(*loaded);
}

/// A number of a golden file.
/// @throws std::runtime_error when @p value is not one.
double number(const Json& value)
{
    const std::optional<double> parsed = QtRocket::Test::goldenNumber(value);
    if (!parsed)
    {
        throw std::runtime_error("not a golden number: " + value.dump());
    }
    return *parsed;
}

/// A golden [x, y, z] or [x, y, z, w].
Coordinate coordinate(const Json& value)
{
    const double weight = value.size() > 3 ? number(value.at(3)) : 0.0;
    return Coordinate{number(value.at(0)), number(value.at(1)), number(value.at(2)), weight};
}

/// The component at the golden @p path ("/", "/0", "/0/1/2": child indices from the root).
RocketComponent& componentAt(RocketComponent& root, std::string_view path)
{
    RocketComponent* component = &root;
    std::size_t      position  = 1;
    while (position < path.size())
    {
        std::size_t next = path.find('/', position);
        if (next == std::string_view::npos)
        {
            next = path.size();
        }
        const std::optional<int> index =
            QtRocket::Strings::parseInt(path.substr(position, next - position));
        if (!index || *index < 0)
        {
            throw std::runtime_error("bad golden path " + std::string{path});
        }
        component = &component->getChild(static_cast<std::size_t>(*index));
        position  = next + 1;
    }
    return *component;
}

/// The golden vector of coordinates @p values.
std::vector<Coordinate> coordinates(const Json& values)
{
    std::vector<Coordinate> result;
    for (const Json& value : values)
    {
        result.push_back(coordinate(value));
    }
    return result;
}

/// The golden vector of numbers @p values.
std::vector<double> numbers(const Json& values)
{
    std::vector<double> result;
    for (const Json& value : values)
    {
        result.push_back(number(value));
    }
    return result;
}

/// Gives @p testDouble the mass properties (component mass, CG, unit inertias, massiveness) and
/// the instances of the golden component @p golden.
void applyGoldenDouble(TestComponent& testDouble, const Json& golden)
{
    testDouble.setMass(number(golden.at("componentMass")));
    testDouble.setCG(coordinate(golden.at("componentCG")).setWeight(0));
    testDouble.setUnitInertias(number(golden.at("longitudinalUnitInertia")),
                               number(golden.at("rotationalUnitInertia")));
    testDouble.setMassive(golden.at("isMassive").get<bool>());
    testDouble.setInstances(coordinates(golden.at("instanceOffsets")),
                            numbers(golden.at("instanceAngles")));
}

/// Gives @p component the mass and CG overrides of the golden @p overrides.
void applyGoldenOverrides(RocketComponent& component, const Json& overrides)
{
    if (overrides.at("massOverridden").get<bool>())
    {
        component.setMassOverridden(true);
        component.setOverrideMass(number(overrides.at("overrideMass")));
    }
    if (overrides.at("cgOverridden").get<bool>())
    {
        component.setCGOverridden(true);
        component.setOverrideCGX(number(overrides.at("overrideCGX")));
    }
    component.setSubcomponentsOverriddenMass(
        overrides.at("subcomponentsOverriddenMass").get<bool>());
    component.setSubcomponentsOverriddenCG(overrides.at("subcomponentsOverriddenCG").get<bool>());
}

/// Gives every test double of @p rocket the mass properties, the instances and the overrides
/// of the OpenRocket component at the same path in @p geometry, so that the doubles stand in for
/// OpenRocket's concrete components. The assemblies are the real classes (massless in
/// OpenRocket too); they only take the overrides.
void applyGoldenMassProperties(Rocket& rocket, const Json& geometry)
{
    for (const Json& golden : geometry.at("components"))
    {
        const auto       path      = golden.at("path").get<std::string>();
        RocketComponent& component = componentAt(rocket, path);
        if (auto* const testDouble = dynamic_cast<TestComponent*>(&component))
        {
            applyGoldenDouble(*testDouble, golden);
        }
        else
        {
            EXPECT_TRUE(QtRocket::isAssembly(component.kind()) &&
                        number(golden.at("componentMass")) == 0.0)
                << path;
        }
        applyGoldenOverrides(component, golden.at("overrides"));
    }
}

/// Expects @p actual within 1e-12 of @p expected in x, y and z.
void expectLocation(const Coordinate& actual, const Coordinate& expected, const std::string& what)
{
    EXPECT_NEAR(actual.x, expected.x, 1e-12) << what;
    EXPECT_NEAR(actual.y, expected.y, 1e-12) << what;
    EXPECT_NEAR(actual.z, expected.z, 1e-12) << what;
}

/// Expects every component of @p rocket at the absolute locations its OpenRocket counterpart
/// has in @p geometry, so that the doubles reproduce OpenRocket's geometry.
void expectGoldenLocations(Rocket& rocket, const Json& geometry)
{
    for (const Json& golden : geometry.at("components"))
    {
        const auto                    path     = golden.at("path").get<std::string>();
        const std::vector<Coordinate> actual   = componentAt(rocket, path).getComponentLocations();
        const std::vector<Coordinate> expected = coordinates(golden.at("componentLocations"));
        ASSERT_EQ(actual.size(), expected.size()) << path;
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            expectLocation(actual[i], expected[i], std::format("{} #{}", path, i));
        }
    }
}

/// Expects @p actual within relative 1e-9 of @p expected (and 1e-15 absolute, for zeros).
void expectClose(double actual, double expected, const std::string& what)
{
    EXPECT_NEAR(actual, expected, (1e-9 * std::abs(expected)) + 1e-15) << what;
}

/// Expects @p actual within relative 1e-9 of @p expected, weight included.
void expectCloseCoordinate(const Coordinate& actual, const Coordinate& expected,
                           const std::string& what)
{
    expectClose(actual.x, expected.x, what + " x");
    expectClose(actual.y, expected.y, what + " y");
    expectClose(actual.z, expected.z, what + " z");
    expectClose(actual.weight, expected.weight, what + " weight");
}

/// Expects @p actual to be the golden rigid body @p expected (mass, CM, the three inertias).
void expectRigidBody(const RigidBody& actual, const Json& expected, const std::string& what)
{
    expectClose(actual.getMass(), number(expected.at("mass")), what + " mass");
    expectCloseCoordinate(actual.getCM(), coordinate(expected.at("cm")), what + " cm");
    expectClose(actual.getIxx(), number(expected.at("ixx")), what + " ixx");
    expectClose(actual.getIyy(), number(expected.at("iyy")), what + " iyy");
    expectClose(actual.getIzz(), number(expected.at("izz")), what + " izz");
    expectClose(actual.getRotationalInertia(), number(expected.at("rotationalInertia")),
                what + " rotationalInertia");
    expectClose(actual.getLongitudinalInertia(), number(expected.at("longitudinalInertia")),
                what + " longitudinalInertia");
}

/// The key of the golden CM analysis row @p row: a motor's designation hash, else the key of
/// the component at its path.
std::int32_t analysisKey(Rocket& rocket, const Json& row)
{
    if (row.at("kind").get<std::string>() == "motor")
    {
        return QtRocket::Strings::javaHashCode(row.at("name").get<std::string>());
    }
    return CMAnalysisEntry::keyOf(componentAt(rocket, row.at("path").get<std::string>()));
}

/// Expects @p analysis to hold exactly the golden CM analysis @p rows.
void expectGoldenAnalysis(Rocket& rocket, const CMAnalysisMap& analysis, const Json& rows,
                          const std::string& configName)
{
    std::set<std::int32_t> keys;
    for (const Json& row : rows)
    {
        const auto         rowName = row.at("name").get<std::string>();
        const std::string  what    = std::format("{} analysis {}", configName, rowName);
        const std::int32_t key     = analysisKey(rocket, row);
        keys.insert(key);
        const auto entry = analysis.find(key);
        ASSERT_TRUE(entry != analysis.end()) << what;
        EXPECT_EQ(entry->second.name, rowName) << what;
        expectClose(entry->second.eachMass, number(row.at("eachMass")), what + " eachMass");
        expectCloseCoordinate(entry->second.totalCM, coordinate(row.at("totalCM")),
                              what + " totalCM");
    }
    EXPECT_EQ(analysis.size(), keys.size()) << configName;
}

/// Expects the mass calculations of every flight configuration of @p rocket to give the golden
/// values of @p mass: the four rigid bodies and the CM analysis rows. Each configuration is
/// selected while it is calculated, as the golden harness does.
void expectGoldenMass(Rocket& rocket, const Json& mass)
{
    const FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
    for (const Json& golden : mass.at("configurations"))
    {
        const auto           name = golden.at("name").get<std::string>();
        FlightConfiguration& config =
            golden.at("isDefault").get<bool>()
                ? rocket.getEmptyConfiguration()
                : rocket.getFlightConfiguration(
                      FlightConfigurationId::fromString(golden.at("id").get<std::string>()));
        rocket.setSelectedConfiguration(config.getId());

        expectRigidBody(MassCalculator::calculateStructure(config), golden.at("structure"),
                        name + " structure");
        expectRigidBody(MassCalculator::calculateLaunch(config), golden.at("launch"),
                        name + " launch");
        expectRigidBody(MassCalculator::calculateBurnout(config), golden.at("burnout"),
                        name + " burnout");
        expectRigidBody(MassCalculator::calculateMotor(config), golden.at("motor"),
                        name + " motor");
        expectGoldenAnalysis(rocket, MassCalculator::getCMAnalysis(config), golden.at("cmAnalysis"),
                             name);
    }
    rocket.setSelectedConfiguration(selected);
}

// ============================================================================ test rockets

/// TestRockets.generateMotor_C6_18mm() exactly (TestRockets.h's motorC6() is a triangular
/// approximation with another burnout mass): the golden comparisons need Java's burnout data.
std::shared_ptr<const ThrustCurveMotor> javaMotorC6()
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(QtRocket::Manufacturer::getManufacturer("Estes"))
        .setDesignation("C6")
        .setDescription("Desc")
        .setCaseInfo("SU 18.0x70.0")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({0, 3, 5, 7})
        .setDiameter(0.018)
        .setLength(0.070)
        .setTimePoints({0, 0.2, 0.4, 2.0, 2.1})
        .setThrustPoints({0, 12, 5, 5, 0})
        .setCGPoints({Coordinate{0.035, 0, 0, 0.0227}, Coordinate{0.035, 0, 0, 0.0165},
                      Coordinate{0.035, 0, 0, 0.0165}, Coordinate{0.035, 0, 0, 0.013},
                      Coordinate{0.035, 0, 0, 0.012}})
        .setDigest("digest C6 test");
    auto built = builder.build();
    if (!built)
    {
        throw std::runtime_error(built.error().toString());
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*built));
}

/// Puts Java's C6 into the three C6 configurations of an Alpha III motor mount.
void useJavaC6(TestMotorMount& mount)
{
    const std::shared_ptr<const ThrustCurveMotor> c6 = javaMotorC6();
    mount.addMotor(testFcid(2), c6, 3.0);
    mount.addMotor(testFcid(3), c6, 5.0);
    mount.addMotor(testFcid(4), c6, 7.0);
}

/// The Estes Alpha III with OpenRocket's mass properties.
struct GoldenAlphaIII : TestEstesAlphaIII
{
    GoldenAlphaIII()
    {
        applyGoldenMassProperties(*rocket,
                                  loadGolden("testrocket-estes-alpha-iii", "geometry.json"));
        useJavaC6(*inner);
    }
};

/// The Beta with OpenRocket's mass properties.
struct GoldenBeta : QtRocket::Test::TestBeta
{
    GoldenBeta()
    {
        applyGoldenMassProperties(*rocket, loadGolden("testrocket-beta", "geometry.json"));
        useJavaC6(*inner);
    }
};

/// The Falcon 9 Heavy with OpenRocket's mass properties.
struct GoldenFalcon9Heavy : TestFalcon9Heavy
{
    GoldenFalcon9Heavy()
    {
        applyGoldenMassProperties(*rocket,
                                  loadGolden("testrocket-falcon-9-heavy", "geometry.json"));
    }
};

/// TestRockets.makeSimple2Stage() from test doubles: two stages, each with a body tube (0.1 m,
/// radius 0.01 m, wall 0.001 m), with OpenRocket's mass properties; TEST_FCID_0 selected with
/// every stage active.
struct GoldenSimple2Stage
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             sustainerStage{nullptr};
    TestBodyComponent*      sustainerBody{nullptr};
    AxialStage*             boosterStage{nullptr};
    TestBodyComponent*      boosterBody{nullptr};

    GoldenSimple2Stage()
    {
        rocket->createFlightConfiguration(testFcid(0));
        rocket->setName("Simple 2-Stage Rocket");

        sustainerStage = &rocket->addChild(std::make_unique<AxialStage>());
        sustainerStage->setName("Sustainer Stage");
        sustainerBody = &sustainerStage->addChild(TestBodyComponent::make(0.10, 0.01));
        sustainerBody->setInnerRadius(0.009);
        sustainerBody->setName("Sustainer Body Tube");

        boosterStage = &rocket->addChild(std::make_unique<AxialStage>());
        boosterStage->setName("Booster Stage");
        boosterBody = &boosterStage->addChild(TestBodyComponent::make(0.10, 0.01));
        boosterBody->setInnerRadius(0.009);
        boosterBody->setName("Booster Body Tube");

        rocket->setSelectedConfiguration(testFcid(0));
        rocket->getSelectedConfiguration().setAllStages();
        rocket->enableEvents();

        applyGoldenMassProperties(*rocket,
                                  loadGolden("testrocket-simple-2-stage", "geometry.json"));
    }
};

/// A body tube double of @p length with BodyTube's CG (the middle) and the mass of a cardboard
/// tube (680 kg/m^3) of outer radius @p radius and wall @p thickness.
std::unique_ptr<TestBodyComponent> makeTube(double length, double radius, double thickness)
{
    auto         tube    = TestBodyComponent::make(length, radius);
    const double inner   = radius - thickness;
    const double density = 680;
    tube->setInnerRadius(inner);
    tube->setMass(std::numbers::pi * ((radius * radius) - (inner * inner)) * length * density);
    tube->setCG(Coordinate{length / 2});
    return tube;
}

/// The motor states SimulationStatus makes: one per motor of @p config (populateMotors()).
std::vector<MotorClusterState> motorStates(const FlightConfiguration& config)
{
    std::vector<MotorClusterState> states;
    for (const MotorConfiguration& motorConfig : config.getAllMotors())
    {
        states.emplace_back(motorConfig);
    }
    return states;
}

/// SimulationStatus.getActiveMotors(): the states whose mount is active in @p config.
std::vector<const MotorClusterState*> activeMotors(const FlightConfiguration&            config,
                                                   const std::vector<MotorClusterState>& states)
{
    std::vector<const MotorClusterState*> active;
    for (const MotorClusterState& state : states)
    {
        if (config.isComponentActive(QtRocket::asComponent(state.getMount())))
        {
            active.push_back(&state);
        }
    }
    return active;
}

// ======================================================= golden comparisons through test doubles

TEST(MassCalculatorGolden, EstesAlphaIII)
{
    GoldenAlphaIII alpha;
    expectGoldenLocations(*alpha.rocket, loadGolden("testrocket-estes-alpha-iii", "geometry.json"));
    expectGoldenMass(*alpha.rocket, loadGolden("testrocket-estes-alpha-iii", "mass.json"));
}

TEST(MassCalculatorGolden, Beta)
{
    GoldenBeta beta;
    expectGoldenLocations(*beta.rocket, loadGolden("testrocket-beta", "geometry.json"));
    expectGoldenMass(*beta.rocket, loadGolden("testrocket-beta", "mass.json"));
}

TEST(MassCalculatorGolden, Falcon9Heavy)
{
    GoldenFalcon9Heavy f9h;
    expectGoldenLocations(*f9h.rocket, loadGolden("testrocket-falcon-9-heavy", "geometry.json"));
    expectGoldenMass(*f9h.rocket, loadGolden("testrocket-falcon-9-heavy", "mass.json"));
}

TEST(MassCalculatorGolden, Simple2Stage)
{
    GoldenSimple2Stage simple;
    expectGoldenLocations(*simple.rocket, loadGolden("testrocket-simple-2-stage", "geometry.json"));
    expectGoldenMass(*simple.rocket, loadGolden("testrocket-simple-2-stage", "mass.json"));
}

// ================================================================ ported from MassCalculatorTest

TEST(MassCalculator, EmptyRocket)
{
    Rocket                     rocket;
    const FlightConfiguration& config = rocket.getEmptyConfiguration();

    const RigidBody  actualStructure     = MassCalculator::calculateStructure(config);
    const double     actualRocketDryMass = actualStructure.getCM().weight;
    const Coordinate actualRocketDryCM   = actualStructure.getCM();

    EXPECT_EQ(0, actualRocketDryMass) << " Empty Rocket Empty Mass is incorrect: ";

    const Coordinate expCM{0, 0, 0, 0};
    EXPECT_EQ(expCM.x, actualRocketDryCM.x) << "Empty Rocket CM.getX() is incorrect: ";
    EXPECT_EQ(expCM.y, actualRocketDryCM.y) << "Empty Rocket CM.getY() is incorrect: ";
    EXPECT_EQ(expCM.z, actualRocketDryCM.z) << "Empty Rocket CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, actualRocketDryCM) << "Empty Rocket CM is incorrect: ";

    EXPECT_EQ(0, actualStructure.getRotationalInertia())
        << "Empty Rocket Rotational MOI calculated incorrectly: ";
    EXPECT_EQ(0, actualStructure.getLongitudinalInertia())
        << "Empty Rocket Longitudinal MOI calculated incorrectly: ";
}

TEST(MassCalculator, StageOverride)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());

    FlightConfiguration& config = rocket.getEmptyConfiguration();
    config.setAllStages();
    rocket.enableEvents();

    // BodyTubes whose mass is overridden: only their CG (the middle) matters.
    auto tube1 = TestBodyComponent::make(1.0, 0.01);
    tube1->setCG(Coordinate{0.5});
    tube1->setMassOverridden(true);
    tube1->setOverrideMass(1.0);
    stage.addChild(std::move(tube1));

    auto tube2 = TestBodyComponent::make(2.0, 0.01);
    tube2->setCG(Coordinate{1.0});
    tube2->setMassOverridden(true);
    tube2->setOverrideMass(2.0);
    stage.addChild(std::move(tube2));

    RigidBody structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(3.0, structure.getCM().weight, kEpsilon) << "No overrides -- mass incorrect";
    EXPECT_NEAR(1.5, structure.getCM().x, kEpsilon) << "No overrides -- CG incorrect";

    stage.setMassOverridden(true);
    stage.setOverrideMass(1.0);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(4.0, structure.getCM().weight, kEpsilon) << "Overrides: mass -- mass incorrect";
    EXPECT_NEAR(1.5, structure.getCM().x, kEpsilon) << "Overrides: mass -- CG incorrect";

    stage.setSubcomponentsOverriddenMass(true);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(1.0, structure.getCM().weight, kEpsilon)
        << "Overrides: mass, children mass --  mass incorrect";
    EXPECT_NEAR(1.5, structure.getCM().x, kEpsilon)
        << "Overrides: mass, children mass -- CG incorrect";

    stage.setCGOverridden(true);
    stage.setOverrideCGX(1.0);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(1.0, structure.getCM().weight, kEpsilon)
        << "Overrides: mass, children mass, CG -- mass incorrect";
    EXPECT_NEAR(1.0, structure.getCM().x, kEpsilon)
        << "Overrides: mass, children mass, CG -- CG incorrect";

    stage.setSubcomponentsOverriddenCG(true);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(1.0, structure.getCM().weight, kEpsilon)
        << "Overrides: mass, children mass, CG, children CG -- mass incorrect";
    EXPECT_NEAR(1.0, structure.getCM().x, kEpsilon)
        << "Overrides: mass, children mass, CG, children CG -- CG incorrect";

    stage.setSubcomponentsOverriddenMass(false);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(4.0, structure.getCM().weight, kEpsilon)
        << "Overrides: mass, CG, children CG -- mass incorrect";
    EXPECT_NEAR(1.0, structure.getCM().x, kEpsilon)
        << "Overrides: mass, CG, children CG -- CG incorrect";

    stage.setSubcomponentsOverriddenCG(false);
    structure = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(4.0, structure.getCM().weight, kEpsilon) << "Overrides: mass, CG -- mass incorrect";
    EXPECT_NEAR(1.375, structure.getCM().x, kEpsilon) << "Overrides: mass, CG -- CG incorrect";
}

TEST(MassCalculator, AlphaIIIStructure)
{
    GoldenAlphaIII       alpha;
    FlightConfiguration& config = alpha.rocket->getEmptyConfiguration();
    config.setAllStages();

    const RigidBody  actualStructure     = MassCalculator::calculateStructure(config);
    const double     actualRocketDryMass = actualStructure.getCM().weight;
    const Coordinate actualRocketDryCM   = actualStructure.getCM();

    const double expRocketDryMass = 0.025268291846128787;
    EXPECT_NEAR(expRocketDryMass, actualRocketDryMass, kEpsilon)
        << " Alpha III Empty Mass is incorrect: ";

    const double     expCMx = 0.19176843580003;
    const double     expCMy = -0.00031704007993248;  // Slight offset due to launch lug
    const Coordinate expCM{expCMx, expCMy, 0, expRocketDryMass};
    EXPECT_NEAR(expCM.x, actualRocketDryCM.x, kEpsilon) << "Simple Rocket CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, actualRocketDryCM.y, kEpsilon) << "Simple Rocket CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, actualRocketDryCM.z, kEpsilon) << "Simple Rocket CM.getZ() is incorrect: ";
    EXPECT_NEAR(expCM.weight, actualRocketDryCM.weight, kEpsilon)
        << "Simple Rocket CM.getWeight() is incorrect: ";
    EXPECT_EQ(expCM, actualRocketDryCM) << "Simple Rocket CM is incorrect: ";

    const double expMOIrot  = 1.888136072268211E-5;
    const double expMOIlong = 1.7808603404853048E-4;

    const double actualMOIrot  = actualStructure.getRotationalInertia();
    const double actualMOIlong = actualStructure.getLongitudinalInertia();
    EXPECT_NEAR(expMOIrot, actualMOIrot, kEpsilon)
        << "Alpha III Rotational MOI calculated incorrectly: ";
    EXPECT_NEAR(expMOIlong, actualMOIlong, kEpsilon)
        << "Alpha III Longitudinal MOI calculated incorrectly: ";

    // if we use a mass override, setting to same mass, we should get same result
    AxialStage& sustainer = *alpha.stage;
    sustainer.setSubcomponentsOverriddenMass(true);
    sustainer.setMassOverridden(true);
    sustainer.setOverrideMass(actualRocketDryMass);

    const RigidBody  overrideStructure   = MassCalculator::calculateStructure(config);
    const Coordinate overrideRocketDryCM = overrideStructure.getCM();

    EXPECT_EQ(actualRocketDryCM, overrideRocketDryCM) << "Simple Rocket Override CM is incorrect: ";

    EXPECT_NEAR(actualMOIrot, overrideStructure.getRotationalInertia(), kEpsilon)
        << "Alpha III Rotational MOI calculated incorrectly: ";
    EXPECT_NEAR(actualMOIlong, overrideStructure.getLongitudinalInertia(), kEpsilon)
        << "Alpha III Longitudinal MOI calculated incorrectly: ";
}

TEST(MassCalculator, SubcomponentMassOverrideScalesInertia)
{
    GoldenAlphaIII       alpha;
    FlightConfiguration& config = alpha.rocket->getEmptyConfiguration();
    config.setAllStages();

    // Baseline: geometric mass and inertia of the un-overridden rocket.
    const RigidBody baseline      = MassCalculator::calculateStructure(config);
    const double    geometricMass = baseline.getMass();
    const double    baselineRot   = baseline.getRotationalInertia();
    const double    baselineLong  = baseline.getLongitudinalInertia();

    // Pin the whole (single) stage's mass, covering its subcomponents.
    AxialStage& sustainer = *alpha.rocket->getStage(0);
    sustainer.setSubcomponentsOverriddenMass(true);
    sustainer.setMassOverridden(true);

    // Keeping the geometric distribution, mass X carries X/m times the inertia.
    for (const double factor : {1.0, 1.8, 3.6, 9.1})
    {
        sustainer.setOverrideMass(geometricMass * factor);
        const RigidBody pinned  = MassCalculator::calculateStructure(config);
        const double    expMass = geometricMass * factor;
        const double    expRot  = baselineRot * factor;
        const double    expLong = baselineLong * factor;
        EXPECT_NEAR(expMass, pinned.getMass(), std::abs(expMass) * 1e-6)
            << "overridden total mass wrong at factor " << factor;
        EXPECT_NEAR(expRot, pinned.getRotationalInertia(), std::abs(expRot) * 1e-6)
            << "rotational (roll) inertia must scale with the overridden mass, factor " << factor;
        EXPECT_NEAR(expLong, pinned.getLongitudinalInertia(), std::abs(expLong) * 1e-6)
            << "longitudinal inertia must scale with the overridden mass, factor " << factor;
    }
}

TEST(MassCalculator, AlphaIIILaunchMass)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));

    const Motor&       activeMotor = *alpha.inner->getMotorConfig(config.getId()).getMotor();
    const std::string& desig       = activeMotor.getDesignation();

    const RigidBody  actualLaunchRigidBody  = MassCalculator::calculateLaunch(config);
    const double     actualRocketLaunchMass = actualLaunchRigidBody.getMass();
    const Coordinate actualRocketLaunchCM   = actualLaunchRigidBody.getCM();

    const double expRocketLaunchMass = 0.04166829184612879;
    EXPECT_NEAR(expRocketLaunchMass, actualRocketLaunchMass, kEpsilon)
        << " Alpha III Total Mass (with motor: " << desig << ") is incorrect: ";

    const double     expCMx = 0.20996446974544236;
    const double     expCMy = -0.00019225797151073;  // Slight offset due to launch lug
    const Coordinate expCM{expCMx, expCMy, 0, expRocketLaunchMass};
    EXPECT_NEAR(expCM.x, actualRocketLaunchCM.x, kEpsilon)
        << "Simple Rocket CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, actualRocketLaunchCM.y, kEpsilon)
        << "Simple Rocket CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, actualRocketLaunchCM.z, kEpsilon)
        << "Simple Rocket CM.getZ() is incorrect: ";
    EXPECT_NEAR(expCM.weight, actualRocketLaunchCM.weight, kEpsilon)
        << "Simple Rocket CM.getWeight() is incorrect: ";
    EXPECT_EQ(expCM, actualRocketLaunchCM) << "Simple Rocket CM is incorrect: ";
}

TEST(MassCalculator, AlphaIIIStageAnalysisIncludesMotorMass)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));

    const CMAnalysisMap    analysis    = MassCalculator::getCMAnalysis(config);
    const CMAnalysisEntry& rocketEntry = analysis.at(CMAnalysisEntry::keyOf(*alpha.rocket));
    const CMAnalysisEntry& stageEntry  = analysis.at(CMAnalysisEntry::keyOf(*alpha.stage));

    // A one-stage rocket provides an exact aggregate check: both rows represent the same launch
    // mass.
    EXPECT_NEAR(rocketEntry.totalCM.weight, stageEntry.totalCM.weight, kEpsilon)
        << "Stage aggregate mass should include its configured motor and propellant";
    EXPECT_NEAR(rocketEntry.totalCM.x, stageEntry.totalCM.x, kEpsilon)
        << "Stage aggregate CG should include its configured motor and propellant";
    EXPECT_NEAR(stageEntry.totalCM.weight, stageEntry.eachMass, kEpsilon)
        << "A single stage instance should have the same instance and aggregate mass";
}

TEST(MassCalculator, AlphaIIIMotorMass)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));
    const Motor&       activeMotor    = *alpha.inner->getMotorConfig(config.getId()).getMotor();
    const std::string& desig          = activeMotor.getDesignation();

    const double expMotorLaunchMass = activeMotor.getLaunchMass();  // 0.0164 kg

    const RigidBody actualMotorData = MassCalculator::calculateMotor(config);

    EXPECT_NEAR(expMotorLaunchMass, actualMotorData.getMass(), kEpsilon)
        << " Motor Mass " << desig << " is incorrect: ";

    const double     expCMx = 0.238;
    const Coordinate expCM{expCMx, 0, 0, expMotorLaunchMass};
    EXPECT_NEAR(expCM.x, actualMotorData.getCM().x, kEpsilon)
        << "Simple Rocket CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, actualMotorData.getCM().y, kEpsilon)
        << "Simple Rocket CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, actualMotorData.getCM().z, kEpsilon)
        << "Simple Rocket CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, actualMotorData.getCM()) << "Simple Rocket CM is incorrect: ";
}

/// Records calls made by the former recursive motor-mass tree walk (Java: CountingMassComponent).
class CountingMassComponent final : public TestComponent
{
public:
    CountingMassComponent() : TestComponent(ComponentKind::MASS_COMPONENT) { }

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override
    {
        ++m_instanceLocationCalls;
        return TestComponent::getInstanceLocations();
    }

    [[nodiscard]] int getInstanceLocationCalls() const noexcept { return m_instanceLocationCalls; }
    void              resetInstanceLocationCalls() noexcept { m_instanceLocationCalls = 0; }

private:
    mutable int m_instanceLocationCalls{0};
};

TEST(MassCalculator, MotorMassSkipsNonMotorTreeTraversal)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));
    auto& nonMotorComponent = alpha.body->addChild(std::make_unique<CountingMassComponent>());

    // Component events may update the instance map while the test rocket is assembled.
    nonMotorComponent.resetInstanceLocationCalls();
    const RigidBody motorData = MassCalculator::calculateMotor(config);

    EXPECT_EQ(0, nonMotorComponent.getInstanceLocationCalls())
        << "Motor mass calculation should not traverse non-motor components";
    EXPECT_NEAR(0.0164, motorData.getMass(), kEpsilon)
        << "Skipping unrelated components must preserve the active motor mass";

    // The structure pass does visit it.
    static_cast<void>(MassCalculator::calculateStructure(config));
    EXPECT_GT(nonMotorComponent.getInstanceLocationCalls(), 0);
}

TEST(MassCalculator, AlphaIIIMotorSimulationMass)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));
    const Motor&       activeMotor    = *alpha.inner->getMotorConfig(config.getId()).getMotor();
    const std::string& desig          = activeMotor.getDesignation();

    // What a SimulationStatus of the configuration holds: one state per motor.
    std::vector<MotorClusterState> states = motorStates(config);
    ASSERT_EQ(states.size(), 1U);

    // Ignite motor at 1.0 seconds
    MotorClusterState& currentMotorState = states.front();
    const double       ignitionTime      = 1.0;
    currentMotorState.ignite(ignitionTime);

    for (const double simTime :
         {1.03 /* almost launch */, 2.03 /* middle */, 3.03 /* after burnout */})
    {
        const RigidBody actualMotorData =
            MassCalculator::calculateMotor(config, simTime, activeMotors(config, states));
        const double expMass = activeMotor.getTotalMass(simTime - ignitionTime);
        EXPECT_NEAR(expMass, actualMotorData.getMass(), kEpsilon)
            << " Motor Mass " << desig << " is incorrect: ";
    }
}

TEST(MassCalculator, SimulationMotorMassUsesEachMountIgnitionTime)
{
    GoldenAlphaIII                     alpha;
    TestMotorMount&                    firstMount = *alpha.inner;
    FlightConfiguration&               config = alpha.rocket->getFlightConfiguration(testFcid(0));
    const FlightConfigurationId&       fcid   = config.getId();
    const std::shared_ptr<const Motor> sharedMotor = firstMount.getMotorConfig(fcid).getMotor();

    auto& secondMount = alpha.body->addChild(std::make_unique<TestMotorMount>(
        ComponentKind::INNER_TUBE, AxialMethod::TOP, firstMount.getLength()));
    secondMount.setMotorMount(true);
    MotorConfiguration secondConfig{secondMount, fcid};
    secondConfig.setMotor(sharedMotor);
    secondMount.setMotorConfig(std::move(secondConfig), fcid);
    config.update();

    const std::vector<MotorClusterState> states = motorStates(config);
    std::vector<MotorClusterState>       ignited;
    for (const MotorClusterState& state : states)
    {
        MotorClusterState copy = state;
        copy.ignite(
            &state.getMount() == static_cast<const QtRocket::MotorMount*>(&firstMount) ? 0.0 : 1.0);
        ignited.push_back(copy);
    }
    ASSERT_EQ(ignited.size(), 2U);

    const double    expectedMass = sharedMotor->getTotalMass(1.5) + sharedMotor->getTotalMass(0.5);
    const RigidBody motorData =
        MassCalculator::calculateMotor(config, 1.5, activeMotors(config, ignited));
    EXPECT_NEAR(expectedMass, motorData.getMass(), kEpsilon)
        << "Each motor mount must use its own ignition time";
}

TEST(MassCalculator, StageCMxOverride)
{
    GoldenSimple2Stage         simple;
    AxialStage&                boosterStage = *simple.boosterStage;
    const FlightConfiguration& config       = simple.rocket->getSelectedConfiguration();

    {  // [0] verify / document structure
        EXPECT_NEAR(0.0, simple.sustainerBody->getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, simple.sustainerBody->getLength(), kEpsilon);

        EXPECT_NEAR(0.10, simple.boosterBody->getComponentLocations()[0].x, kEpsilon);
        EXPECT_NEAR(0.10, simple.boosterBody->getLength(), kEpsilon);
    }

    {  // [1] test Rocket CM, before:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.0081178754, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.10, actualStructure.getCM().x, kEpsilon);
    }

    boosterStage.setSubcomponentsOverriddenCG(true);
    boosterStage.setCGOverridden(true);
    boosterStage.setOverrideCGX(0.0);

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.0081178754, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.075, actualStructure.getCM().x, kEpsilon);
    }
}

TEST(MassCalculator, SingleStageMassOverride)
{
    GoldenSimple2Stage   simple;
    AxialStage&          sustainerStage = *simple.sustainerStage;
    TestBodyComponent&   sustainerBody  = *simple.sustainerBody;
    FlightConfiguration& config         = simple.rocket->getSelectedConfiguration();
    config.setOnlyStage(0);

    const double expSingleBodyMass = 0.0040589377;
    {  // [0] verify / document structure
        EXPECT_NEAR(0.0, sustainerBody.getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, sustainerBody.getLength(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, sustainerBody.getMass(), kEpsilon);
    }

    {  // [1] test Rocket CM, before:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(expSingleBodyMass, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.05, actualStructure.getCM().x, kEpsilon);
    }

    sustainerStage.setSubcomponentsOverriddenMass(true);
    sustainerStage.setMassOverridden(true);
    sustainerStage.setOverrideMass(0.001);  // something small, but not zero

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.001, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.05, actualStructure.getCM().x, kEpsilon);
    }
}

TEST(MassCalculator, DoubleStageMassOverride)
{
    GoldenSimple2Stage         simple;
    const TestBodyComponent&   sustainerBody = *simple.sustainerBody;
    AxialStage&                boosterStage  = *simple.boosterStage;
    const TestBodyComponent&   boosterBody   = *simple.boosterBody;
    const FlightConfiguration& config        = simple.rocket->getSelectedConfiguration();

    const double expSingleBodyMass = 0.0040589377;
    {  // [0] verify / document structure
        EXPECT_NEAR(0.0, sustainerBody.getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, sustainerBody.getLength(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, sustainerBody.getMass(), kEpsilon);
        EXPECT_NEAR(0.0, boosterBody.getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, boosterBody.getLength(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, boosterBody.getMass(), kEpsilon);
    }

    {  // [1] test Rocket CM, before:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(2 * expSingleBodyMass, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.10, actualStructure.getCM().x, kEpsilon);
    }

    boosterStage.setSubcomponentsOverriddenMass(true);
    boosterStage.setMassOverridden(true);
    boosterStage.setOverrideMass(0.001);  // something small, but not zero

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(expSingleBodyMass + 0.001, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.06976699, actualStructure.getCM().x, kEpsilon);
    }
}

TEST(MassCalculator, ComponentCMxOverride)
{
    GoldenSimple2Stage         simple;
    const TestBodyComponent&   sustainerBody = *simple.sustainerBody;
    TestBodyComponent&         boosterBody   = *simple.boosterBody;
    const FlightConfiguration& config        = simple.rocket->getSelectedConfiguration();

    {  // [0] verify / document structure
        EXPECT_NEAR(0.0, sustainerBody.getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, sustainerBody.getLength(), kEpsilon);

        EXPECT_NEAR(0.10, boosterBody.getComponentLocations()[0].x, kEpsilon);
        EXPECT_NEAR(0.10, boosterBody.getLength(), kEpsilon);
    }

    {  // [1] test Rocket CM, before:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.0081178754, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.10, actualStructure.getCM().x, kEpsilon);
    }

    boosterBody.setSubcomponentsOverriddenCG(false);
    boosterBody.setCGOverridden(true);
    boosterBody.setOverrideCGX(0.0);

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.0081178754, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.075, actualStructure.getCM().x, kEpsilon);
    }
}

TEST(MassCalculator, ComponentMassOverride)
{
    GoldenSimple2Stage         simple;
    const TestBodyComponent&   sustainerBody = *simple.sustainerBody;
    TestBodyComponent&         boosterBody   = *simple.boosterBody;
    const FlightConfiguration& config        = simple.rocket->getSelectedConfiguration();

    const double expSingleBodyMass = 0.0040589377;
    {  // [0] verify / document structure
        EXPECT_NEAR(0.0, sustainerBody.getPosition().x, kEpsilon);
        EXPECT_NEAR(0.1, sustainerBody.getLength(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, sustainerBody.getMass(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, sustainerBody.getSectionMass(), kEpsilon);

        EXPECT_NEAR(0.10, boosterBody.getComponentLocations()[0].x, kEpsilon);
        EXPECT_NEAR(0.10, boosterBody.getLength(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, boosterBody.getMass(), kEpsilon);
        EXPECT_NEAR(expSingleBodyMass, boosterBody.getSectionMass(), kEpsilon);
    }

    {  // [1] test Rocket CM, before:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(0.0081178754, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(0.10, actualStructure.getCM().x, kEpsilon);
    }

    boosterBody.setSubcomponentsOverriddenMass(false);
    boosterBody.setMassOverridden(true);
    const double newMass = 0.001;
    boosterBody.setOverrideMass(newMass);

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(expSingleBodyMass + newMass, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(newMass, boosterBody.getMass(), kEpsilon);
        EXPECT_NEAR(newMass, boosterBody.getSectionMass(), kEpsilon);
        EXPECT_NEAR(0.06976699, actualStructure.getCM().x, kEpsilon);
    }

    boosterBody.setSubcomponentsOverriddenMass(
        true);                             // change. Also, this body lacks subcomponents.
    boosterBody.setMassOverridden(true);   // repeat
    boosterBody.setOverrideMass(newMass);  // repeat

    {  // [1] test Rocket CM, after:
        const RigidBody actualStructure = MassCalculator::calculateStructure(config);
        EXPECT_NEAR(expSingleBodyMass + newMass, actualStructure.getCM().weight, kEpsilon);
        EXPECT_NEAR(newMass, boosterBody.getMass(), kEpsilon);
        EXPECT_NEAR(newMass, boosterBody.getSectionMass(), kEpsilon);
        EXPECT_NEAR(0.06976699, actualStructure.getCM().x, kEpsilon);
    }
}

TEST(MassCalculator, Falcon9HPayloadStructureCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();

    // validate payload stage
    config.setOnlyStage(f9h.payloadStage->getStageNumber());

    const RigidBody  actualStructureData = MassCalculator::calculateStructure(config);
    const Coordinate actualCM            = actualStructureData.getCM();

    EXPECT_NEAR(0.11628853296935873, actualCM.weight, kEpsilon)
        << "Upper Stage Mass is incorrect: ";
    EXPECT_NEAR(0.2780673116227175, actualCM.x, kEpsilon) << "Upper Stage CM.getX() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.y, kEpsilon) << "Upper Stage CM.getY() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.z, kEpsilon) << "Upper Stage CM.getZ() is incorrect: ";
}

TEST(MassCalculator, Falcon9HCoreStructureCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    config.setOnlyStage(f9h.coreStage->getStageNumber());

    const Coordinate actualCM = MassCalculator::calculateStructure(config).getCM();

    EXPECT_NEAR(0.12988600, actualCM.weight, kEpsilon) << "Upper Stage Mass is incorrect: ";
    EXPECT_NEAR(0.964, actualCM.x, kEpsilon) << "Upper Stage CM.getX() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.y, kEpsilon) << "Upper Stage CM.getY() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.z, kEpsilon) << "Upper Stage CM.getZ() is incorrect: ";
}

TEST(MassCalculator, Falcon9HCoreMotorLaunchCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(f9h.coreStage->getStageNumber());

    const Motor& motor = *f9h.coreBody->getMotorConfig(config.getId()).getMotor();

    const RigidBody  actMotorData = MassCalculator::calculateMotor(config);
    const Coordinate actCM        = actMotorData.getCM();

    const double     expMotorMass = motor.getLaunchMass();
    const Coordinate expCM{1.053, 0, 0, expMotorMass};

    EXPECT_NEAR(expMotorMass, actMotorData.getMass(), kEpsilon) << " propellant mass is incorrect";
    EXPECT_NEAR(expCM.x, actCM.x, kEpsilon) << " propellant CoM x is incorrect";
    EXPECT_NEAR(expCM.y, actCM.y, kEpsilon) << " propellant CoM y is incorrect";
    EXPECT_NEAR(expCM.z, actCM.z, kEpsilon) << " propellant CoM z is incorrect";
}

TEST(MassCalculator, Falcon9HCoreMotorLaunchMOIs)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(1);

    const RigidBody corePropInertia = MassCalculator::calculateMotor(config);

    // validated against a specific motor/radius/length
    EXPECT_NEAR(0.003380625, corePropInertia.getIxx(), kEpsilon)
        << "Core Stage motor axial MOI is incorrect: ";
    EXPECT_NEAR(0.156701835, corePropInertia.getIyy(), kEpsilon)
        << "Core Stage motor longitudinal MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HBoosterStructureCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const Coordinate actualCM = MassCalculator::calculateStructure(config).getCM();

    EXPECT_NEAR(0.6063587938961827, actualCM.weight, kEpsilon)
        << "Heavy Booster Mass is incorrect: ";
    EXPECT_NEAR(1.0750544407309763, actualCM.x, kEpsilon)
        << "Heavy Booster CM.getX() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.y, kEpsilon) << "Heavy Booster CM.getY() is incorrect: ";
    EXPECT_NEAR(0.0, actualCM.z, kEpsilon) << "Heavy Booster CM.getZ() is incorrect: ";
}

TEST(MassCalculator, Falcon9HBoosterLaunchCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody actualBoosterLaunchData = MassCalculator::calculateLaunch(config);

    const double expectedMass = 1.5903587938961827;
    EXPECT_NEAR(expectedMass, actualBoosterLaunchData.getMass(), kEpsilon)
        << " Booster Launch Mass is incorrect: ";

    const Coordinate actualCM = actualBoosterLaunchData.getCM();
    const Coordinate expCM{1.223107189094683, 0, 0, expectedMass};
    EXPECT_NEAR(expCM.x, actualCM.x, kEpsilon) << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, actualCM.y, kEpsilon) << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, actualCM.z, kEpsilon) << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, actualCM) << " Booster Launch CM is incorrect: ";
}

TEST(MassCalculator, Falcon9HBoosterSpentCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    // Validate Booster Launch Mass
    const Coordinate spentCM = MassCalculator::calculateBurnout(config).getCM();

    const Coordinate expLaunchCM{1.1846026528203366, 0, 0, 1.1183587938961828};
    EXPECT_NEAR(expLaunchCM.weight, spentCM.weight, kEpsilon)
        << " Booster Launch Mass is incorrect: ";
    EXPECT_NEAR(expLaunchCM.x, spentCM.x, kEpsilon) << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expLaunchCM.y, spentCM.y, kEpsilon) << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expLaunchCM.z, spentCM.z, kEpsilon) << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expLaunchCM, spentCM) << " Booster Launch CM is incorrect: ";
}

TEST(MassCalculator, Falcon9HBoosterMotorCM)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody  actualPropellant = MassCalculator::calculateMotor(config);
    const Coordinate actCM            = actualPropellant.getCM();

    const Motor& boosterMotor = *f9h.boosterMotorTubes->getMotorConfig(config.getId()).getMotor();

    const double expBoosterPropMassEach = boosterMotor.getLaunchMass();
    const double boosterSetMotorCount   = 8.0;
    const double expBoosterPropMass     = expBoosterPropMassEach * boosterSetMotorCount;

    const Coordinate expCM{1.31434, 0, 0, expBoosterPropMass};

    EXPECT_NEAR(expBoosterPropMass, actualPropellant.getMass(), kEpsilon)
        << " G77 propellant mass is incorrect";
    EXPECT_NEAR(expCM.x, actCM.x, kEpsilon) << " G77 propellant CoM x is incorrect";
    EXPECT_NEAR(expCM.y, actCM.y, kEpsilon) << " G77 propellant CoM y is incorrect";
    EXPECT_NEAR(expCM.z, actCM.z, kEpsilon) << " G77 propellant CoM z is incorrect";
}

TEST(MassCalculator, Falcon9HeavyBoosterMotorLaunchMOIs)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody actualInertia = MassCalculator::calculateMotor(config);

    EXPECT_NEAR(0.006380379, actualInertia.getIxx(), kEpsilon)
        << "Booster stage propellant axial MOI is incorrect: ";
    EXPECT_NEAR(0.001312553, actualInertia.getIyy(), kEpsilon)
        << "Booster stage propellant longitudinal MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HeavyBoosterSpentMOIs)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody spent = MassCalculator::calculateBurnout(config);

    EXPECT_NEAR(0.009193574474290651, spent.getRotationalInertia(), kEpsilon)
        << " Booster x-axis MOI is incorrect: ";
    EXPECT_NEAR(0.05741546005688325, spent.getLongitudinalInertia(), kEpsilon)
        << " Booster transverse MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HeavyBoosterLaunchMOIs)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody launchData = MassCalculator::calculateLaunch(config);

    EXPECT_NEAR(0.012254081474290652, launchData.getRotationalInertia(), kEpsilon)
        << " Booster x-axis MOI is incorrect: ";
    EXPECT_NEAR(0.06363179384136365, launchData.getLongitudinalInertia(), kEpsilon)
        << " Booster transverse MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HeavyBoosterStageMassOverride)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    f9h.rocket->setSelectedConfiguration(config.getId());
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    ParallelStage& boosters     = *f9h.boosterStage;
    const double   overrideMass = 0.5;
    boosters.setSubcomponentsOverriddenMass(true);
    boosters.setMassOverridden(true);
    boosters.setOverrideMass(overrideMass);
    boosters.setSubcomponentsOverriddenCG(true);
    boosters.setCGOverridden(true);
    boosters.setOverrideCGX(6.0);

    const RigidBody  burnout      = MassCalculator::calculateStructure(config);
    const Coordinate boosterSetCM = burnout.getCM();

    EXPECT_NEAR(overrideMass, burnout.getMass(), kEpsilon) << " Booster Launch Mass is incorrect: ";

    // With only the booster stage active in the selected configuration, the core stage sits at
    // the front of the rocket, so the boosters' front is at -0.08.
    const Coordinate expCM{5.92, 0, 0, overrideMass};
    EXPECT_NEAR(expCM.x, boosterSetCM.x, kEpsilon) << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, boosterSetCM.y, kEpsilon) << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, boosterSetCM.z, kEpsilon) << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, boosterSetCM) << " Booster Launch CM is incorrect: ";

    // Because the 0.5 kg mass override covers the booster's subcomponents, the geometric inertia
    // is rescaled to the overridden mass (scale = overrideMass / geometricMass).
    EXPECT_NEAR(0.004843421529808234, burnout.getRotationalInertia(), kEpsilon)
        << " Booster x-axis MOI is incorrect: ";
    EXPECT_NEAR(14.662020679052493, burnout.getLongitudinalInertia(), kEpsilon)
        << " Booster transverse MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HeavyComponentMassOverride)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    f9h.rocket->setSelectedConfiguration(config.getId());
    config.setOnlyStage(f9h.boosterStage->getStageNumber());

    f9h.boosterNose->setMassOverridden(true);
    f9h.boosterNose->setOverrideMass(0.71);

    f9h.boosterBody->setMassOverridden(true);
    f9h.boosterBody->setOverrideMass(0.622);

    f9h.boosterMotorTubes->setMassOverridden(true);
    f9h.boosterMotorTubes->setOverrideMass(0.213);

    // Fin mass is not overridden

    const RigidBody  boosterData = MassCalculator::calculateStructure(config);
    const Coordinate boosterCM   = boosterData.getCM();

    const double expTotalMass = 3.3565872;
    EXPECT_NEAR(expTotalMass, boosterData.getMass(), kEpsilon)
        << " Booster Launch Mass is incorrect: ";

    const Coordinate expCM{0.2827146624421746, 0, 0, expTotalMass};
    EXPECT_NEAR(expCM.x, boosterCM.x, kEpsilon) << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, boosterCM.y, kEpsilon) << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, boosterCM.z, kEpsilon) << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, boosterCM) << " Booster Launch CM is incorrect: ";

    EXPECT_NEAR(0.02493025354590946, boosterData.getRotationalInertia(), kEpsilon)
        << " Booster x-axis MOI is incorrect: ";
    EXPECT_NEAR(0.3488283595364345, boosterData.getLongitudinalInertia(), kEpsilon)
        << " Booster transverse MOI is incorrect: ";
}

TEST(MassCalculator, Falcon9HeavyComponentCMxOverride)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    f9h.rocket->setSelectedConfiguration(config.getId());
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    f9h.boosterNose->setCGOverridden(true);
    f9h.boosterNose->setOverrideCGX(0.22);

    f9h.boosterBody->setCGOverridden(true);
    f9h.boosterBody->setOverrideCGX(0.433);

    f9h.boosterMotorTubes->setCGOverridden(true);
    f9h.boosterMotorTubes->setOverrideCGX(0.395);

    const RigidBody structure = MassCalculator::calculateStructure(config);
    const double    expMass   = 0.6063587938961827;
    EXPECT_NEAR(expMass, structure.getMass(), kEpsilon) << " Booster Launch Mass is incorrect: ";

    const Coordinate expCM{0.5567081598531745, 0, 0, expMass};
    EXPECT_NEAR(expCM.x, structure.getCM().x, kEpsilon)
        << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, structure.getCM().y, kEpsilon)
        << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, structure.getCM().z, kEpsilon)
        << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, structure.getCM()) << " Booster Launch CM is incorrect: ";

    EXPECT_NEAR(0.005873702474290652, structure.getRotationalInertia(), kEpsilon)
        << " Booster x-axis MOI is incorrect: ";
    EXPECT_NEAR(0.04063515705066886, structure.getLongitudinalInertia(), kEpsilon)
        << " Booster transverse MOI is incorrect: ";
}

TEST(MassCalculator, SimplePhantomPodRocket)
{
    Rocket rocket;
    rocket.setName("Test Phantom Pods");

    rocket.createFlightConfiguration(testFcid(0));
    rocket.setSelectedConfiguration(testFcid(0));
    FlightConfiguration& config = rocket.getSelectedConfiguration();

    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.setName("Primary Stage");

    TestBodyComponent& primaryBody = stage.addChild(makeTube(0.4, 0.02, 0.001));
    primaryBody.setName("Primary Body");

    PodSet& pods = primaryBody.addChild(std::make_unique<PodSet>());
    pods.setName("Pods");
    pods.setInstanceCount(2);
    pods.setAxialMethod(AxialMethod::BOTTOM);
    pods.setAxialOffset(0.0);
    pods.setAngleMethod(AngleMethod::RELATIVE);
    pods.setAngleOffset(0.0);
    pods.setRadiusMethod(RadiusMethod::FREE);
    pods.setRadiusOffset(0.04);

    TestBodyComponent& podBody = pods.addChild(makeTube(0.0, 0.02, 0.001));
    podBody.setName("Primary Body");

    // TrapezoidFinSet(1 fin, root 0.05, tip 0.05, sweep 0, height 0.001): a rectangle whose CG
    // is in the middle of the root chord; its mass is overridden.
    auto fins = TestComponent::make(0.05, ComponentKind::TRAPEZOID_FIN_SET, AxialMethod::BOTTOM);
    fins->setName("podFins");
    fins->setCG(Coordinate{0.025});
    fins->setMassOverridden(true);
    fins->setOverrideMass(0.02835);
    fins->setSubcomponentsOverriddenMass(false);
    fins->setAxialOffset(AxialMethod::BOTTOM, -0.01);
    podBody.addChild(std::move(fins));

    rocket.enableEvents();
    config.setAllStages();

    const RigidBody structure = MassCalculator::calculateStructure(config);
    const double    expMass   = 0.0900260149;
    EXPECT_NEAR(expMass, structure.getMass(), kEpsilon) << " Booster Launch Mass is incorrect: ";

    const Coordinate expCM{0.3039199615, 0.0, 0.0, expMass};
    EXPECT_NEAR(expCM.x, structure.getCM().x, kEpsilon)
        << " Booster Launch CM.getX() is incorrect: ";
    EXPECT_NEAR(expCM.y, structure.getCM().y, kEpsilon)
        << " Booster Launch CM.getY() is incorrect: ";
    EXPECT_NEAR(expCM.z, structure.getCM().z, kEpsilon)
        << " Booster Launch CM.getZ() is incorrect: ";
    EXPECT_EQ(expCM, structure.getCM()) << " Booster Launch CM is incorrect: ";
}

TEST(MassCalculator, EmptyStages)
{
    GoldenAlphaIII       reference;  // Reference rocket
    FlightConfiguration& configRef = reference.rocket->getEmptyConfiguration();
    configRef.setAllStages();

    const RigidBody  structureRef     = MassCalculator::calculateStructure(configRef);
    const double     rocketDryMassRef = structureRef.getCM().weight;
    const Coordinate rocketDryCMRef   = structureRef.getCM();

    GoldenAlphaIII alpha;
    alpha.rocket->addChild(std::make_unique<AxialStage>(), 0);  // the front of the rocket
    alpha.rocket->addChild(std::make_unique<AxialStage>());     // the rear of the rocket
    FlightConfiguration& config = alpha.rocket->getEmptyConfiguration();
    config.setAllStages();

    const RigidBody  structure     = MassCalculator::calculateStructure(config);
    const double     rocketDryMass = structure.getCM().weight;
    const Coordinate rocketDryCM   = structure.getCM();

    EXPECT_NEAR(rocketDryMassRef, rocketDryMass, kEpsilon)
        << " Empty Stages Rocket Empty Mass is incorrect: ";
    EXPECT_NEAR(rocketDryCMRef.x, rocketDryCM.x, kEpsilon)
        << "Empty Stages Rocket CM.getX() is incorrect: ";
    EXPECT_NEAR(rocketDryCMRef.y, rocketDryCM.y, kEpsilon)
        << "Empty Stages Rocket CM.getY() is incorrect: ";
    EXPECT_NEAR(rocketDryCMRef.z, rocketDryCM.z, kEpsilon)
        << "Empty Stages Rocket CM.getZ() is incorrect: ";
    EXPECT_EQ(rocketDryCMRef, rocketDryCM) << "Empty Stages Rocket CM is incorrect: ";

    EXPECT_NEAR(structureRef.getRotationalInertia(), structure.getRotationalInertia(), kEpsilon)
        << "Empty Stages Rocket Rotational MOI calculated incorrectly: ";
    EXPECT_NEAR(structureRef.getLongitudinalInertia(), structure.getLongitudinalInertia(), kEpsilon)
        << "Empty Stages Rocket Longitudinal MOI calculated incorrectly: ";

    // if we use a mass override, setting to same mass, we should get same result
    AxialStage& sustainerRef = *reference.stage;
    sustainerRef.setSubcomponentsOverriddenMass(true);
    sustainerRef.setMassOverridden(true);
    sustainerRef.setOverrideMass(rocketDryMassRef);

    ASSERT_EQ(&alpha.rocket->getChild(1), static_cast<RocketComponent*>(alpha.stage));
    AxialStage& sustainer = *alpha.stage;
    sustainer.setSubcomponentsOverriddenMass(true);
    sustainer.setMassOverridden(true);
    sustainer.setOverrideMass(rocketDryMass);

    const RigidBody overrideStructureRef = MassCalculator::calculateStructure(configRef);
    const RigidBody overrideStructure    = MassCalculator::calculateStructure(config);

    EXPECT_EQ(overrideStructureRef.getCM(), overrideStructure.getCM())
        << "Empty Stages Rocket Override CM is incorrect: ";
    EXPECT_NEAR(overrideStructureRef.getRotationalInertia(),
                overrideStructure.getRotationalInertia(), kEpsilon)
        << "Empty Stages Rocket Rotational MOI calculated incorrectly: ";
    EXPECT_NEAR(overrideStructureRef.getLongitudinalInertia(),
                overrideStructure.getLongitudinalInertia(), kEpsilon)
        << "Empty Stages Rocket Longitudinal MOI calculated incorrectly: ";
}

TEST(MassCalculator, DisabledStageMassAndCG)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    config.setAllStages();

    // Baseline: all stages active
    const RigidBody allActive = MassCalculator::calculateStructure(config);
    const double    massAll   = allActive.getMass();
    const double    cmxAll    = allActive.getCM().x;

    // Disable booster (also disable core since it is a child stage)
    config.setStageActive(kFalcon9hCoreStageNumber, false);
    config.setStageActive(kFalcon9hBoosterStageNumber, false);

    // All hardcoded values are pulled from Falcon9HPayloadStructureCM
    const RigidBody noBooster = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(0.11628853296935873, noBooster.getMass(), kEpsilon)
        << "Mass with core+booster disabled should equal payload mass only";
    EXPECT_NEAR(0.2780673116227175, noBooster.getCM().x, kEpsilon)
        << "CG with core+booster disabled should match standalone payload CG";

    // Disable core stage too (only payload remains)
    config.setStageActive(kFalcon9hCoreStageNumber, false);

    const RigidBody payloadOnly = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(0.11628853296935873, payloadOnly.getMass(), kEpsilon)
        << "Mass with core+booster disabled should equal payload mass only";
    EXPECT_NEAR(0.2780673116227175, payloadOnly.getCM().x, kEpsilon)
        << "CG with core+booster disabled should match standalone payload CG";

    // Re-enable all stages and verify full restoration
    config.setAllStages();

    const RigidBody restored = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(massAll, restored.getMass(), kEpsilon)
        << "Mass should be fully restored after re-enabling all stages";
    EXPECT_NEAR(cmxAll, restored.getCM().x, kEpsilon)
        << "CG should be fully restored after re-enabling all stages";
}

// ===================================================================== ported from MassCacheTest

/// MassCacheTest.testCMCache only builds the Falcon 9 Heavy (its cache checks are commented out
/// in OpenRocket, whose MassCalculator keeps no cache). Here: repeated calculations agree, and a
/// change of the rocket (a new mass modification id) or of the configuration shows at once.
TEST(MassCache, CMCache)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);

    const RigidBody first  = MassCalculator::calculateLaunch(config);
    const RigidBody second = MassCalculator::calculateLaunch(config);
    EXPECT_TRUE(first.getCM().exactlyEquals(second.getCM()));
    EXPECT_EQ(first.getIxx(), second.getIxx());
    EXPECT_EQ(first.getIyy(), second.getIyy());
    EXPECT_EQ(MassCalculator::modId(), ModId::zero());
    EXPECT_EQ(MassCalculator::getModId(), ModId::zero());

    // A mass change of a component: a new mass modification id, and a new result.
    const ModId massModId = f9h.rocket->getMassModId();
    f9h.parachute->setMass(f9h.parachute->getComponentMass() + 0.1);
    EXPECT_NE(f9h.rocket->getMassModId(), massModId);
    const RigidBody heavier = MassCalculator::calculateLaunch(config);
    EXPECT_NEAR(heavier.getMass(), first.getMass() + 0.1, 1e-12);

    // A stage flag change of the configuration: a new configuration modification id.
    const ModId configModId = config.getModId();
    config.setOnlyStage(kFalcon9hPayloadStageNumber);
    EXPECT_NE(config.getModId(), configModId);
    const RigidBody payload = MassCalculator::calculateLaunch(config);
    EXPECT_NEAR(payload.getMass(), 0.11628853296935873 + 0.1, kEpsilon);

    // Back to the full rocket.
    config.setAllStages();
    EXPECT_NEAR(MassCalculator::calculateLaunch(config).getMass(), heavier.getMass(), 1e-12);
}

// ============================================================================ further cases

/// A rocket with one stage holding a body (a double of 0.5 m, 0.2 kg, CG at its middle, unit
/// inertias 0.01 longitudinal, 0.002 rotational) that holds a motor mount (0.1 m, starting
/// 0.4 m from the body's front, 0.05 kg, CG at its middle); events enabled.
class MassCalculatorRocketTest : public ::testing::Test
{
protected:
    MassCalculatorRocketTest()
    {
        m_rocket.createFlightConfiguration(m_fcid);
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body  = &m_stage->addChild(TestBodyComponent::make(0.5, 0.03));
        m_body->setMass(0.2);
        m_body->setCG(Coordinate{0.25});
        m_body->setUnitInertias(0.01, 0.002);
        m_mount = &m_body->addChild(
            TestMotorMount::make(0.1, 0.01, ComponentKind::INNER_TUBE, AxialMethod::TOP));
        m_mount->setAxialOffset(AxialMethod::TOP, 0.4);
        m_mount->setMass(0.05);
        m_mount->setCG(Coordinate{0.05});
        m_mount->setMotorMount(true);
        m_rocket.setSelectedConfiguration(m_fcid);
        m_rocket.enableEvents();
    }

    [[nodiscard]] FlightConfiguration& config() { return m_rocket.getFlightConfiguration(m_fcid); }

    /// Puts an A8 into the mount and returns it.
    std::shared_ptr<const ThrustCurveMotor> addA8()
    {
        std::shared_ptr<const ThrustCurveMotor> motor = QtRocket::Test::motorA8();
        m_mount->addMotor(m_fcid, motor);
        config().update();
        return motor;
    }

    /// The front of an A8 (0.07 m) in the mount: it fills the mount from 0.43 m to 0.5 m.
    static constexpr double kMotorX = 0.4 + (0.1 - 0.07);

    Rocket                m_rocket;
    FlightConfigurationId m_fcid{testFcid(0)};
    AxialStage*           m_stage{nullptr};
    TestBodyComponent*    m_body{nullptr};
    TestMotorMount*       m_mount{nullptr};
};

TEST_F(MassCalculatorRocketTest, StructureAddsComponentsAndParallelAxisTerms)
{
    const RigidBody structure = MassCalculator::calculateStructure(config());
    // Body: 0.2 kg at 0.25; mount: 0.05 kg at 0.45.
    EXPECT_DOUBLE_EQ(structure.getMass(), 0.25);
    const double cmx = ((0.2 * 0.25) + (0.05 * 0.45)) / 0.25;
    EXPECT_DOUBLE_EQ(structure.getCM().x, cmx);
    // Ixx: only the body has a rotational unit inertia; Iyy: its own plus both offsets.
    EXPECT_DOUBLE_EQ(structure.getIxx(), 0.002 * 0.2);
    const double iyy =
        (0.01 * 0.2) + (0.2 * (0.25 - cmx) * (0.25 - cmx)) + (0.05 * (0.45 - cmx) * (0.45 - cmx));
    EXPECT_NEAR(structure.getIyy(), iyy, 1e-15);
    EXPECT_EQ(structure.getIzz(), structure.getIyy());
}

TEST_F(MassCalculatorRocketTest, MassOverrideOfThisComponentOnlyScalesItsOwnInertia)
{
    m_body->setMassOverridden(true);
    m_body->setOverrideMass(0.4);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_DOUBLE_EQ(structure.getMass(), 0.45);
    // The body's own inertia follows the override mass (0.4), the mount keeps its mass.
    EXPECT_DOUBLE_EQ(structure.getIxx(), 0.002 * 0.4);
}

TEST_F(MassCalculatorRocketTest, SubcomponentMassOverrideScalesTheWholeSubtree)
{
    m_mount->setUnitInertias(0.003, 0.001);
    const RigidBody before = MassCalculator::calculateStructure(config());

    m_body->setMassOverridden(true);
    m_body->setSubcomponentsOverriddenMass(true);
    m_body->setOverrideMass(0.5);  // twice the geometric 0.25 kg of the subtree
    const RigidBody after = MassCalculator::calculateStructure(config());

    // The whole override mass sits at the (massive) body's own CG, as in Java; the subtree's
    // bodies keep their places with twice their mass and inertia.
    EXPECT_DOUBLE_EQ(after.getMass(), 0.5);
    EXPECT_DOUBLE_EQ(after.getCM().x, 0.25);
    EXPECT_NEAR(after.getIxx(), 2 * before.getIxx(), 1e-15);
    // Body: 0.4 kg (0.01 unit) at the CM; mount: 0.1 kg (0.003 unit) 0.2 m behind it.
    EXPECT_NEAR(after.getIyy(), (0.01 * 0.4) + (0.003 * 0.1) + (0.1 * 0.2 * 0.2), 1e-15);
}

TEST_F(MassCalculatorRocketTest, SubcomponentMassOverrideOfAMasslessSubtreeDropsItsInertia)
{
    m_body->setMass(0.0);
    m_mount->setMass(0.0);
    m_body->setMassOverridden(true);
    m_body->setSubcomponentsOverriddenMass(true);
    m_body->setOverrideMass(0.3);
    const RigidBody structure = MassCalculator::calculateStructure(config());

    // No geometric mass to rescale: the scale is 0, so the override mass carries no inertia.
    EXPECT_DOUBLE_EQ(structure.getMass(), 0.3);
    EXPECT_DOUBLE_EQ(structure.getCM().x, 0.25);
    EXPECT_EQ(structure.getIxx(), 0.0);
    EXPECT_EQ(structure.getIyy(), 0.0);
}

TEST_F(MassCalculatorRocketTest, MassOverrideOfANonMassiveComponentTakesItsChildrensCG)
{
    m_body->setMassive(false);
    m_body->setMassOverridden(true);
    m_body->setOverrideMass(1.0);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    // The body's 1 kg sits at its child's CG (0.45), and the mount adds its 0.05 kg there too.
    EXPECT_DOUBLE_EQ(structure.getMass(), 1.05);
    EXPECT_DOUBLE_EQ(structure.getCM().x, 0.45);
}

TEST_F(MassCalculatorRocketTest, CGOverrideIsMeasuredFromTheComponentsFront)
{
    m_mount->setCGOverridden(true);
    m_mount->setOverrideCGX(0.0);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    // The mount's CG moves to its front, 0.4 m.
    EXPECT_DOUBLE_EQ(structure.getCM().x, ((0.2 * 0.25) + (0.05 * 0.4)) / 0.25);
}

TEST_F(MassCalculatorRocketTest, SubcomponentCGOverrideMovesTheChildrenToo)
{
    m_body->setCGOverridden(true);
    m_body->setOverrideCGX(0.1);
    const RigidBody own = MassCalculator::calculateStructure(config());
    EXPECT_DOUBLE_EQ(own.getCM().x, ((0.2 * 0.1) + (0.05 * 0.45)) / 0.25);

    m_body->setSubcomponentsOverriddenCG(true);
    const RigidBody all = MassCalculator::calculateStructure(config());
    EXPECT_DOUBLE_EQ(all.getCM().x, 0.1);
    EXPECT_DOUBLE_EQ(all.getMass(), 0.25);
}

TEST_F(MassCalculatorRocketTest, MotorMassAtLaunch)
{
    addA8();

    const RigidBody launch = MassCalculator::calculateMotor(config());
    EXPECT_DOUBLE_EQ(launch.getMass(), 0.0164);
    EXPECT_DOUBLE_EQ(launch.getCM().x, kMotorX + 0.035);
    // A single motor on the axis: unit inertias of a solid cylinder times the mass.
    const MotorConfiguration& motorConfig = m_mount->getMotorConfig(m_fcid);
    EXPECT_DOUBLE_EQ(launch.getIxx(), motorConfig.getUnitRotationalInertia() * 0.0164);
    EXPECT_DOUBLE_EQ(launch.getIyy(), motorConfig.getUnitLongitudinalInertia() * 0.0164);
}

TEST_F(MassCalculatorRocketTest, BurnoutAndLaunchAddTheMotorToTheStructure)
{
    addA8();

    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_DOUBLE_EQ(MassCalculator::calculateBurnout(config()).getMass(),
                     structure.getMass() + 0.0131);
    EXPECT_DOUBLE_EQ(MassCalculator::calculateLaunch(config()).getMass(),
                     structure.getMass() + 0.0164);
}

TEST_F(MassCalculatorRocketTest, MotorMassInFlightFollowsTheTimeSinceIgnition)
{
    const std::shared_ptr<const ThrustCurveMotor> motor  = addA8();
    std::vector<MotorClusterState>                states = motorStates(config());
    ASSERT_EQ(states.size(), 1U);
    states.front().ignite(0.5);

    for (const double time : {0.0, 0.5, 1.0, 1.5, 2.5, 4.0})
    {
        const double    motorTime = QtRocket::MathUtil::javaMax(time - 0.5, 0.0);
        const RigidBody inFlight =
            MassCalculator::calculateMotor(config(), time, activeMotors(config(), states));
        EXPECT_DOUBLE_EQ(inFlight.getMass(), motor->getTotalMass(motorTime)) << time;
        EXPECT_DOUBLE_EQ(inFlight.getCM().x, kMotorX + motor->getCMx(motorTime)) << time;
    }
}

TEST_F(MassCalculatorRocketTest, LaunchInFlightAddsTheBurningMotorToTheStructure)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = addA8();
    const double structure                = MassCalculator::calculateStructure(config()).getMass();
    std::vector<MotorClusterState> states = motorStates(config());
    ASSERT_EQ(states.size(), 1U);
    states.front().ignite(0.5);

    for (const double time : {0.0, 1.0, 2.5})
    {
        const double    motorTime = QtRocket::MathUtil::javaMax(time - 0.5, 0.0);
        const RigidBody launch =
            MassCalculator::calculate(MassCalculation::Type::LAUNCH, config(), time,
                                      std::span<const MotorClusterState>{states});
        EXPECT_DOUBLE_EQ(launch.getMass(), structure + motor->getTotalMass(motorTime)) << time;
    }
}

TEST_F(MassCalculatorRocketTest, UnlitOrAbsentMotorStates)
{
    addA8();

    // A motor state that is not lit yet stands at its launch mass.
    const std::vector<MotorClusterState> unlit = motorStates(config());
    EXPECT_DOUBLE_EQ(
        MassCalculator::calculateMotor(config(), 10.0, activeMotors(config(), unlit)).getMass(),
        0.0164);

    // No motor states at all: no motors.
    EXPECT_EQ(MassCalculator::calculateMotor(config(), 1.0, {}).getMass(), 0.0);
}

TEST_F(MassCalculatorRocketTest, MotorOfAnInactiveMountAddsNothing)
{
    m_mount->addMotor(m_fcid, QtRocket::Test::motorA8());
    config().update();
    const std::vector<MotorClusterState> states = motorStates(config());
    ASSERT_EQ(states.size(), 1U);

    // Every stage off: the mount has no active instance, so even an explicitly passed state
    // contributes nothing.
    config().clearAllStages();
    const std::vector<const MotorClusterState*> all{&states.front()};
    EXPECT_EQ(MassCalculator::calculateMotor(config(), 0.0, all).getMass(), 0.0);
    EXPECT_EQ(MassCalculator::calculateMotor(config()).getMass(), 0.0);
}

TEST_F(MassCalculatorRocketTest, ClusterAddsTheOffsetsToTheAxialInertiaOnly)
{
    const double r = 0.02;
    m_mount->setInstances(
        {Coordinate{0, r, 0}, Coordinate{0, 0, r}, Coordinate{0, -r, 0}, Coordinate{0, 0, -r}},
        {0, 0, 0, 0});
    m_mount->setMotorCount(4);
    m_mount->addMotor(m_fcid, QtRocket::Test::motorA8());
    config().update();

    const MotorConfiguration& motorConfig = m_mount->getMotorConfig(m_fcid);
    const RigidBody           motors      = MassCalculator::calculateMotor(config());
    const double              m           = 0.0164;
    EXPECT_DOUBLE_EQ(motors.getMass(), 4 * m);
    EXPECT_DOUBLE_EQ(motors.getCM().y, 0.0);
    EXPECT_DOUBLE_EQ(motors.getCM().z, 0.0);
    // Ixx: 4 m (unit Ixx + r^2); Iyy: 4 m unit Iyy, without the radial offsets (as Java).
    EXPECT_NEAR(motors.getIxx(), 4 * m * (motorConfig.getUnitRotationalInertia() + (r * r)), 1e-15);
    EXPECT_NEAR(motors.getIyy(), 4 * m * motorConfig.getUnitLongitudinalInertia(), 1e-15);

    // The CM analysis gives the motor's row the mass of one motor and the cluster's CG.
    const CMAnalysisMap    analysis = MassCalculator::getCMAnalysis(config());
    const CMAnalysisEntry& row      = analysis.at(CMAnalysisEntry::keyOf(*motorConfig.getMotor()));
    EXPECT_EQ(row.name, "A8");
    EXPECT_DOUBLE_EQ(row.eachMass, m);
    EXPECT_DOUBLE_EQ(row.totalCM.weight, 4 * m);
    EXPECT_DOUBLE_EQ(row.totalCM.x, motors.getCM().x);
}

TEST_F(MassCalculatorRocketTest, PodSetInstancesPlaceTheirSubtreeAndMotors)
{
    PodSet& pods = m_body->addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(3);
    pods.setRadius(RadiusMethod::FREE, 0.1);
    TestMotorMount& podMount = pods.addChild(TestMotorMount::make(0.1, 0.01));
    podMount.setMass(0.03);
    podMount.setCG(Coordinate{0.05});
    podMount.setMotorMount(true);
    podMount.addMotor(m_fcid, QtRocket::Test::motorA8());
    config().update();

    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(structure.getMass(), 0.25 + (3 * 0.03), 1e-15);
    // Three pods 120 degrees apart: the CG stays on the axis.
    EXPECT_NEAR(structure.getCM().y, 0.0, 1e-15);
    EXPECT_NEAR(structure.getCM().z, 0.0, 1e-15);
    // Each pod adds m d^2 about the axis.
    EXPECT_NEAR(structure.getIxx(), (0.002 * 0.2) + (3 * 0.03 * 0.1 * 0.1), 1e-15);

    // One cluster per pod, each on its pod's axis.
    const RigidBody motors = MassCalculator::calculateMotor(config());
    EXPECT_NEAR(motors.getMass(), 3 * 0.0164, 1e-15);
    EXPECT_NEAR(motors.getCM().y, 0.0, 1e-15);
    const MotorConfiguration& motorConfig = podMount.getMotorConfig(m_fcid);
    EXPECT_NEAR(motors.getIxx(),
                3 * 0.0164 * (motorConfig.getUnitRotationalInertia() + (0.1 * 0.1)), 1e-15);

    // The pod set's CM analysis row holds its pods and their motors, per pod.
    const CMAnalysisMap    analysis = MassCalculator::getCMAnalysis(config());
    const CMAnalysisEntry& podRow   = analysis.at(CMAnalysisEntry::keyOf(pods));
    EXPECT_NEAR(podRow.totalCM.weight, 3 * (0.03 + 0.0164), 1e-15);
    EXPECT_NEAR(podRow.eachMass, 0.03 + 0.0164, 1e-15);
}

TEST(MassCalculatorParallelStage, InactiveCoreStageStillCarriesActiveBoosters)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();

    const RigidBody boosters = [&] {
        config.setOnlyStage(kFalcon9hBoosterStageNumber);
        return MassCalculator::calculateStructure(config);
    }();
    const RigidBody core = [&] {
        config.setOnlyStage(kFalcon9hCoreStageNumber);
        return MassCalculator::calculateStructure(config);
    }();

    // The walk descends into the inactive core body to reach the active boosters.
    config.setAllStages();
    config.setStageActive(kFalcon9hPayloadStageNumber, false);
    config.setStageActive(kFalcon9hCoreStageNumber, false, false);
    ASSERT_TRUE(config.isStageActive(kFalcon9hBoosterStageNumber));
    const RigidBody onlyBoosters = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(onlyBoosters.getMass(), boosters.getMass(), 1e-15);

    // Core and boosters together.
    config.setStageActive(kFalcon9hCoreStageNumber, true, true);
    const RigidBody both = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(both.getMass(), boosters.getMass() + core.getMass(), 1e-15);
    EXPECT_NEAR(both.getCM().x,
                ((boosters.getMass() * boosters.getCM().x) + (core.getMass() * core.getCM().x)) /
                    both.getMass(),
                1e-12);
    // Two boosters at +-0.077 m: the CG stays on the axis.
    EXPECT_NEAR(both.getCM().y, 0.0, 1e-15);
}

TEST(MassCalculatorParallelStage, BoosterMotorsAreOneClusterPerBooster)
{
    GoldenFalcon9Heavy   f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);
    config.setOnlyStage(kFalcon9hBoosterStageNumber);

    const RigidBody motors  = MassCalculator::calculateMotor(config);
    const Motor&    g77     = *f9h.boosterMotorTubes->getMotorConfig(f9h.fcid).getMotor();
    const double    perTube = g77.getLaunchMass();
    EXPECT_NEAR(motors.getMass(), 8 * perTube, 1e-14);

    // The analysis: the motor row has one motor's mass and all eight motors' weight; each
    // booster row its share of the boosters' structure and motors.
    const CMAnalysisMap    analysis = MassCalculator::getCMAnalysis(config);
    const CMAnalysisEntry& g77Row   = analysis.at(CMAnalysisEntry::keyOf(g77));
    EXPECT_DOUBLE_EQ(g77Row.eachMass, perTube);
    EXPECT_NEAR(g77Row.totalCM.weight, 8 * perTube, 1e-14);
    const CMAnalysisEntry& boosterRow = analysis.at(CMAnalysisEntry::keyOf(*f9h.boosterStage));
    const RigidBody        structure  = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(boosterRow.totalCM.weight, structure.getMass() + motors.getMass(), 1e-12);
    EXPECT_NEAR(boosterRow.eachMass, (structure.getMass() + motors.getMass()) / 2, 1e-12);
    // Inactive components have no row.
    EXPECT_FALSE(analysis.contains(CMAnalysisEntry::keyOf(*f9h.payloadNose)));
}

TEST(MassCalculatorTypes, StaticCalculateMatchesTheNamedEntryPoints)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(1));

    const auto same = [](const RigidBody& a, const RigidBody& b) {
        return a.getCM().exactlyEquals(b.getCM()) && a.getIxx() == b.getIxx() &&
               a.getIyy() == b.getIyy() && a.getIzz() == b.getIzz();
    };
    EXPECT_TRUE(same(MassCalculator::calculateStructure(config),
                     MassCalculator::calculate(MassCalculation::Type::STRUCTURE, config,
                                               Motor::kPseudoTimeEmpty)));
    EXPECT_TRUE(same(MassCalculator::calculateLaunch(config),
                     MassCalculator::calculate(MassCalculation::Type::LAUNCH, config,
                                               Motor::kPseudoTimeLaunch)));
    EXPECT_TRUE(same(MassCalculator::calculateBurnout(config),
                     MassCalculator::calculate(MassCalculation::Type::BURNOUT, config,
                                               Motor::kPseudoTimeBurnout)));
    EXPECT_TRUE(same(
        MassCalculator::calculateMotor(config),
        MassCalculator::calculate(MassCalculation::Type::MOTOR, config, Motor::kPseudoTimeLaunch)));

    // Unlit motor states in flight give the static launch result.
    const std::vector<MotorClusterState> states = motorStates(config);
    EXPECT_TRUE(same(MassCalculator::calculateLaunch(config),
                     MassCalculator::calculate(MassCalculation::Type::LAUNCH, config, 0.0,
                                               std::span<const MotorClusterState>{states})));
    // STRUCTURE ignores the motor states.
    EXPECT_TRUE(same(MassCalculator::calculateStructure(config),
                     MassCalculator::calculate(MassCalculation::Type::STRUCTURE, config, 0.0,
                                               std::span<const MotorClusterState>{states})));
}

TEST(MassCalculatorTypes, LaunchIsStructurePlusMotorAndBurnoutIsBetween)
{
    GoldenAlphaIII             alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(2));

    const RigidBody structure = MassCalculator::calculateStructure(config);
    const RigidBody motor     = MassCalculator::calculateMotor(config);
    const RigidBody launch    = MassCalculator::calculateLaunch(config);
    const RigidBody burnout   = MassCalculator::calculateBurnout(config);

    EXPECT_NEAR(launch.getMass(), structure.getMass() + motor.getMass(), 1e-15);
    const RigidBody sum = structure.add(motor);
    EXPECT_NEAR(launch.getCM().x, sum.getCM().x, 1e-15);
    EXPECT_NEAR(launch.getIyy(), sum.getIyy(), 1e-15);
    EXPECT_NEAR(launch.getIxx(), sum.getIxx(), 1e-15);

    EXPECT_GT(burnout.getMass(), structure.getMass());
    EXPECT_LT(burnout.getMass(), launch.getMass());
}

}  // namespace
