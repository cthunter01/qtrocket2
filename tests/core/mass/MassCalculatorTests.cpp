#include "QtRocket/mass/MassCalculator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
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
#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ClusterConfiguration;
using QtRocket::CMAnalysisEntry;
using QtRocket::CMAnalysisMap;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::MassCalculation;
using QtRocket::MassCalculator;
using QtRocket::MassComponent;
using QtRocket::ModId;
using QtRocket::Motor;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfiguration;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::ThrustCurveMotor;
using QtRocket::Transition;
using QtRocket::Test::addMotor;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestSimple2Stage;
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

/// Expects @p actual within 1e-12 of @p expected in x, y and z.
void expectLocation(const Coordinate& actual, const Coordinate& expected, const std::string& what)
{
    EXPECT_NEAR(actual.x, expected.x, 1e-12) << what;
    EXPECT_NEAR(actual.y, expected.y, 1e-12) << what;
    EXPECT_NEAR(actual.z, expected.z, 1e-12) << what;
}

/// Expects every component of @p rocket at the absolute locations its OpenRocket counterpart
/// has in @p geometry.
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

// ============================================================================ motor states

/// One motor state per motor of @p config, as SimulationStatus.populateMotors() makes them. (A
/// simulation keeps its states at stable, shared addresses, see MotorClusterState; a vector of
/// values is enough for these tests, which pass pointers to them through activeMotors().)
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

/// Expects @p actual to be @p expected bit for bit: the centre of mass and the inertias.
void expectIdenticalBodies(const RigidBody& actual, const RigidBody& expected,
                           const std::string& what)
{
    EXPECT_TRUE(actual.getCM().exactlyEquals(expected.getCM())) << what;
    EXPECT_EQ(actual.getIxx(), expected.getIxx()) << what;
    EXPECT_EQ(actual.getIyy(), expected.getIyy()) << what;
    EXPECT_EQ(actual.getIzz(), expected.getIzz()) << what;
}

// ======================================================================== golden comparisons
//
// The test rockets are built from the real components; only their fin sets and launch lugs are
// doubles that carry OpenRocket's mass properties (HOOK(fins-lugs), see TestRockets.h).

TEST(MassCalculatorGolden, EstesAlphaIII)
{
    TestEstesAlphaIII alpha;
    expectGoldenLocations(*alpha.rocket, loadGolden("testrocket-estes-alpha-iii", "geometry.json"));
    expectGoldenMass(*alpha.rocket, loadGolden("testrocket-estes-alpha-iii", "mass.json"));
}

TEST(MassCalculatorGolden, Beta)
{
    TestBeta beta;
    expectGoldenLocations(*beta.rocket, loadGolden("testrocket-beta", "geometry.json"));
    expectGoldenMass(*beta.rocket, loadGolden("testrocket-beta", "mass.json"));
}

TEST(MassCalculatorGolden, Falcon9Heavy)
{
    TestFalcon9Heavy f9h;
    expectGoldenLocations(*f9h.rocket, loadGolden("testrocket-falcon-9-heavy", "geometry.json"));
    expectGoldenMass(*f9h.rocket, loadGolden("testrocket-falcon-9-heavy", "mass.json"));
}

TEST(MassCalculatorGolden, Simple2Stage)
{
    TestSimple2Stage simple;
    expectGoldenLocations(*simple.rocket, loadGolden("testrocket-simple-2-stage", "geometry.json"));
    expectGoldenMass(*simple.rocket, loadGolden("testrocket-simple-2-stage", "mass.json"));
}

// ================================================================ ported from MassCalculatorTest
//
// HOOK(fins-lugs): the totals of the Estes Alpha III, the Beta and the Falcon 9 Heavy boosters
// below include their fin sets and launch lugs, which are doubles that carry OpenRocket's mass,
// CG and unit inertias (TestRockets.h). The expectations are OpenRocket's and stay as they are
// when tier 6b computes those values with TrapezoidFinSet and LaunchLug.

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

    auto tube1 = std::make_unique<BodyTube>();
    tube1->setLength(1.0);
    tube1->setMassOverridden(true);
    tube1->setOverrideMass(1.0);
    stage.addChild(std::move(tube1));

    auto tube2 = std::make_unique<BodyTube>();
    tube2->setLength(2.0);
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
    TestEstesAlphaIII    alpha;
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
    TestEstesAlphaIII    alpha;
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
    TestEstesAlphaIII          alpha;
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
    TestEstesAlphaIII          alpha;
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
    TestEstesAlphaIII          alpha;
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

/// Records calls made by the former recursive motor-mass tree walk (Java: CountingMassComponent,
/// a MassComponent).
class CountingMassComponent final : public MassComponent
{
public:
    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override
    {
        ++m_instanceLocationCalls;
        return MassComponent::getInstanceLocations();
    }

    [[nodiscard]] int getInstanceLocationCalls() const noexcept { return m_instanceLocationCalls; }
    void              resetInstanceLocationCalls() noexcept { m_instanceLocationCalls = 0; }

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override
    {
        return std::make_unique<CountingMassComponent>(*this);
    }

private:
    mutable int m_instanceLocationCalls{0};
};

TEST(MassCalculator, MotorMassSkipsNonMotorTreeTraversal)
{
    TestEstesAlphaIII          alpha;
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
    TestEstesAlphaIII          alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(0));
    const Motor&       activeMotor    = *alpha.inner->getMotorConfig(config.getId()).getMotor();
    const std::string& desig          = activeMotor.getDesignation();

    // One state per motor of the configuration, as a SimulationStatus makes them.
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
    TestEstesAlphaIII                  alpha;
    InnerTube&                         firstMount = *alpha.inner;
    FlightConfiguration&               config = alpha.rocket->getFlightConfiguration(testFcid(0));
    const FlightConfigurationId&       fcid   = config.getId();
    const std::shared_ptr<const Motor> sharedMotor = firstMount.getMotorConfig(fcid).getMotor();

    auto newMount = std::make_unique<InnerTube>();
    newMount->setLength(firstMount.getLength());
    newMount->setMotorMount(true);
    InnerTube&         secondMount = alpha.body->addChild(std::move(newMount));
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
    TestSimple2Stage           simple;
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
    TestSimple2Stage     simple;
    AxialStage&          sustainerStage = *simple.sustainerStage;
    const BodyTube&      sustainerBody  = *simple.sustainerBody;
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
    TestSimple2Stage           simple;
    const BodyTube&            sustainerBody = *simple.sustainerBody;
    AxialStage&                boosterStage  = *simple.boosterStage;
    const BodyTube&            boosterBody   = *simple.boosterBody;
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
    TestSimple2Stage           simple;
    const BodyTube&            sustainerBody = *simple.sustainerBody;
    BodyTube&                  boosterBody   = *simple.boosterBody;
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
    TestSimple2Stage           simple;
    const BodyTube&            sustainerBody = *simple.sustainerBody;
    BodyTube&                  boosterBody   = *simple.boosterBody;
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

/// The child @p index of @p parent as a @p Component (Java's cast, which asserts the class).
/// @throws std::bad_cast when the child is of another class.
template <class Component>
[[nodiscard]] const Component& childAs(const RocketComponent& parent, std::size_t index)
{
    return dynamic_cast<const Component&>(parent.getChild(index));
}

/// MassCalculatorTest.testFalcon9HComponentMasses, the payload stage.
TEST(MassCalculator, Falcon9HComponentMassesOfThePayloadStage)
{
    const TestFalcon9Heavy f9h;
    const RocketComponent& payloadStage = f9h.rocket->getChild(0);

    EXPECT_NEAR(0.02255114133733203, childAs<NoseCone>(payloadStage, 0).getComponentMass(),
                kEpsilon)
        << "P/L NoseCone mass calculated incorrectly: ";
    EXPECT_NEAR(0.02904490372, childAs<BodyTube>(payloadStage, 1).getComponentMass(), kEpsilon)
        << "P/L Body mass calculated incorrectly: ";
    EXPECT_NEAR(0.007289284477103441, childAs<Transition>(payloadStage, 2).getComponentMass(),
                kEpsilon)
        << "P/L Transition mass calculated incorrectly: ";

    const auto& upperBody = childAs<BodyTube>(payloadStage, 3);
    EXPECT_NEAR(0.029224351500753608, upperBody.getComponentMass(), kEpsilon)
        << "P/L Upper Stage Body mass calculated incorrectly: ";
    {
        const auto& chute = childAs<Parachute>(upperBody, 0);
        EXPECT_NEAR(0.0079759509252, chute.getComponentMass(), kEpsilon) << chute.getName();
        const auto& cord = childAs<ShockCord>(upperBody, 1);
        EXPECT_NEAR(0.00072, cord.getComponentMass(), kEpsilon) << cord.getName();
    }

    const auto& interstage = childAs<BodyTube>(payloadStage, 4);
    EXPECT_NEAR(0.01948290100050243, interstage.getComponentMass(), kEpsilon)
        << interstage.getName();
}

/// MassCalculatorTest.testFalcon9HComponentMasses, the core stage and the booster set.
TEST(MassCalculator, Falcon9HComponentMassesOfTheCoreAndBoosters)
{
    const TestFalcon9Heavy f9h;
    const RocketComponent& coreStage = f9h.rocket->getChild(1);

    const auto& coreBody = childAs<BodyTube>(coreStage, 0);
    EXPECT_NEAR(0.1298860066700161, coreBody.getComponentMass(), kEpsilon) << coreBody.getName();

    const auto& boosters = childAs<ParallelStage>(coreBody, 0);
    const auto& nose     = childAs<NoseCone>(boosters, 0);
    EXPECT_NEAR(0.02109368568877191, nose.getComponentMass(), kEpsilon) << nose.getName();
    const auto& body = childAs<BodyTube>(boosters, 1);
    EXPECT_NEAR(0.129886006, body.getComponentMass(), kEpsilon) << body.getName();
    const auto& mmt = childAs<InnerTube>(body, 0);
    EXPECT_NEAR(0.01890610458, mmt.getComponentMass(), kEpsilon) << mmt.getName();

    // HOOK(fins-lugs): the fins are a double that carries OpenRocket's mass.
    const RocketComponent& boosterFins = body.getChild(1);
    EXPECT_NEAR(0.13329359999999998, boosterFins.getComponentMass(), kEpsilon)
        << boosterFins.getName();
}

/// MassCalculatorTest.testFalcon9HComponentCM, the payload stage.
TEST(MassCalculator, Falcon9HComponentCMOfThePayloadStage)
{
    const TestFalcon9Heavy f9h;
    const RocketComponent& payloadStage = f9h.rocket->getChild(0);

    EXPECT_NEAR(0.08079767055284799, childAs<NoseCone>(payloadStage, 0).getComponentCG().x,
                kEpsilon)
        << "P/L NoseCone CMx calculated incorrectly: ";
    EXPECT_NEAR(0.066, childAs<BodyTube>(payloadStage, 1).getComponentCG().x, kEpsilon)
        << "P/L Body CMx calculated incorrectly: ";
    EXPECT_NEAR(0.006640909510057012, childAs<Transition>(payloadStage, 2).getComponentCG().x,
                kEpsilon)
        << "P/L Transition CMx calculated incorrectly: ";

    const auto& upperBody = childAs<BodyTube>(payloadStage, 3);
    EXPECT_NEAR(0.09, upperBody.getComponentCG().x, kEpsilon)
        << "P/L Upper Stage Body CMx calculated incorrectly: ";
    EXPECT_NEAR(0.0125, childAs<Parachute>(upperBody, 0).getComponentCG().x, kEpsilon)
        << "Parachute CMx calculated incorrectly: ";
    EXPECT_NEAR(0.0125, childAs<ShockCord>(upperBody, 1).getComponentCG().x, kEpsilon)
        << "Shock Cord CMx calculated incorrectly: ";

    EXPECT_NEAR(0.06, childAs<BodyTube>(payloadStage, 4).getComponentCG().x, kEpsilon)
        << "Interstage CMx calculated incorrectly: ";
}

/// MassCalculatorTest.testFalcon9HComponentCM, the core stage and the booster set.
TEST(MassCalculator, Falcon9HComponentCMOfTheCoreAndBoosters)
{
    const TestFalcon9Heavy f9h;
    const RocketComponent& coreStage = f9h.rocket->getChild(1);

    const auto& coreBody = childAs<BodyTube>(coreStage, 0);
    EXPECT_NEAR(0.4, coreBody.getComponentCG().x, kEpsilon)
        << "Core Body CMx calculated incorrectly: ";

    const auto& boosters = childAs<ParallelStage>(coreBody, 0);
    EXPECT_NEAR(0.05383295859557998, childAs<NoseCone>(boosters, 0).getComponentCG().x, kEpsilon)
        << "Booster Nose CMx calculated incorrectly: ";
    const auto& body = childAs<BodyTube>(boosters, 1);
    EXPECT_NEAR(0.4, body.getComponentCG().x, kEpsilon)
        << "BoosterBody CMx calculated incorrectly: ";
    EXPECT_NEAR(0.075, childAs<InnerTube>(body, 0).getComponentCG().x, kEpsilon)
        << " Motor Mount Tube CMx calculated incorrectly: ";

    // HOOK(fins-lugs): the fins are a double that carries OpenRocket's CG.
    EXPECT_NEAR(0.19393939, body.getChild(1).getComponentCG().x, kEpsilon)
        << "Core Fins CMx calculated incorrectly: ";
}

/// Expects the rotational and the longitudinal inertia of @p component.
void expectInertias(const RocketComponent& component, double rotational, double longitudinal)
{
    EXPECT_NEAR(rotational, component.getRotationalInertia(), kEpsilon)
        << component.getName() << " Rotational MOI calculated incorrectly: ";
    EXPECT_NEAR(longitudinal, component.getLongitudinalInertia(), kEpsilon)
        << component.getName() << " Longitudinal MOI calculated incorrectly: ";
}

/// MassCalculatorTest.testFalcon9HComponentMOI, the payload stage.
TEST(MassCalculator, Falcon9HComponentMOIOfThePayloadStage)
{
    const TestFalcon9Heavy f9h;
    f9h.rocket->setSelectedConfiguration(
        f9h.rocket->getEmptyConfiguration().getFlightConfigurationId());
    const RocketComponent& payloadStage = f9h.rocket->getChild(0);

    expectInertias(childAs<NoseCone>(payloadStage, 0), 3.937551444398643E-5, 4.983150394809428E-5);
    expectInertias(childAs<BodyTube>(payloadStage, 1), 7.70416e-5, 8.06940e-5);
    expectInertias(childAs<Transition>(payloadStage, 2), 1.43691e-5, 7.30265e-6);

    const RocketComponent& upperBody = payloadStage.getChild(3);
    expectInertias(upperBody, 4.22073e-5, 0.0001);
    expectInertias(upperBody.getChild(0), 6.23121e-7, 7.26975e-7);
    expectInertias(upperBody.getChild(1), 5.625e-8, 6.5625e-8);

    expectInertias(payloadStage.getChild(4), 2.81382e-5, 3.74486e-5);
}

/// MassCalculatorTest.testFalcon9HComponentMOI, the core stage and the booster set.
TEST(MassCalculator, Falcon9HComponentMOIOfTheCoreAndBoosters)
{
    const TestFalcon9Heavy f9h;
    f9h.rocket->setSelectedConfiguration(
        f9h.rocket->getEmptyConfiguration().getFlightConfigurationId());
    const RocketComponent& coreStage = f9h.rocket->getChild(1);

    const auto& coreBody = childAs<BodyTube>(coreStage, 0);
    expectInertias(coreBody, 0.000187588, 0.00702105);

    const auto& boosters = childAs<ParallelStage>(coreBody, 0);
    expectInertias(childAs<NoseCone>(boosters, 0), 1.9052671920796627E-5, 2.2559876786981176E-5);
    const auto& boosterBody = childAs<BodyTube>(boosters, 1);
    expectInertias(boosterBody, 1.875878651e-4, 0.00702104762);
    expectInertias(boosterBody.getChild(0), 4.11444e-6, 3.75062e-5);

    // HOOK(fins-lugs): the fins are a double that carries OpenRocket's unit inertias.
    expectInertias(boosterBody.getChild(1), 0.000928545614574877, 0.001246261927287438);
}

TEST(MassCalculator, Falcon9HPayloadStructureCM)
{
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
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

    auto newPrimaryBody = std::make_unique<BodyTube>(0.4, 0.02);
    newPrimaryBody->setThickness(0.001);
    newPrimaryBody->setName("Primary Body");
    BodyTube& primaryBody = stage.addChild(std::move(newPrimaryBody));

    auto newPods = std::make_unique<PodSet>();
    newPods->setName("Pods");
    newPods->setInstanceCount(2);
    PodSet& pods = primaryBody.addChild(std::move(newPods));
    pods.setAxialMethod(AxialMethod::BOTTOM);
    pods.setAxialOffset(0.0);
    pods.setAngleMethod(AngleMethod::RELATIVE);
    pods.setAngleOffset(0.0);
    pods.setRadiusMethod(RadiusMethod::FREE);
    pods.setRadiusOffset(0.04);

    auto newPodBody = std::make_unique<BodyTube>(0.0, 0.02);
    newPodBody->setThickness(0.001);
    newPodBody->setName("Primary Body");
    BodyTube& podBody = pods.addChild(std::move(newPodBody));

    // HOOK(fins-lugs): tier 6b replaces this double with the real class: TrapezoidFinSet(1 fin,
    // root 0.05, tip 0.05, sweep 0, height 0.001) with thickness 0.01, a rectangle whose CG is
    // in the middle of the root chord; its mass is overridden.
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
    TestEstesAlphaIII    reference;  // Reference rocket
    FlightConfiguration& configRef = reference.rocket->getEmptyConfiguration();
    configRef.setAllStages();

    const RigidBody  structureRef     = MassCalculator::calculateStructure(configRef);
    const double     rocketDryMassRef = structureRef.getCM().weight;
    const Coordinate rocketDryCMRef   = structureRef.getCM();

    TestEstesAlphaIII alpha;
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

    // Java's rocket.getChild(0): the empty stage added at the front, which is inactive (it has
    // no children), so its override is ignored.
    auto* const sustainer = dynamic_cast<AxialStage*>(&alpha.rocket->getChild(0));
    ASSERT_NE(sustainer, nullptr);
    ASSERT_NE(sustainer, alpha.stage);
    ASSERT_EQ(sustainer->getChildCount(), 0U);
    EXPECT_FALSE(config.isComponentActive(*sustainer));
    sustainer->setSubcomponentsOverriddenMass(true);
    sustainer->setMassOverridden(true);
    sustainer->setOverrideMass(rocketDryMass);

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

    // Addition: the same override on the real (Alpha III) stage of the rocket with the empty
    // stages also gives the reference result.
    sustainer->setMassOverridden(false);
    ASSERT_EQ(&alpha.rocket->getChild(1), static_cast<RocketComponent*>(alpha.stage));
    alpha.stage->setSubcomponentsOverriddenMass(true);
    alpha.stage->setMassOverridden(true);
    alpha.stage->setOverrideMass(rocketDryMass);
    const RigidBody stageOverride = MassCalculator::calculateStructure(config);
    EXPECT_EQ(overrideStructureRef.getCM(), stageOverride.getCM());
    EXPECT_NEAR(overrideStructureRef.getRotationalInertia(), stageOverride.getRotationalInertia(),
                kEpsilon);
    EXPECT_NEAR(overrideStructureRef.getLongitudinalInertia(),
                stageOverride.getLongitudinalInertia(), kEpsilon);
}

TEST(MassCalculator, StructureMass)
{
    // OpenRocketDocumentFactory.createNewRocket(): a rocket with one stage, every stage active;
    // the document it is made for enables the events.
    Rocket rocket;
    auto   sustainer = std::make_unique<AxialStage>();
    sustainer->setName("Sustainer");
    rocket.addChild(std::move(sustainer));
    rocket.getSelectedConfiguration().setAllStages();
    rocket.enableEvents();

    AxialStage* stage = rocket.getStage(0);
    ASSERT_NE(stage, nullptr);
    stage->addChild(std::make_unique<NoseCone>());
    BodyTube& bodyTube      = stage->addChild(std::make_unique<BodyTube>());
    auto      massComponent = std::make_unique<MassComponent>();
    massComponent->setComponentMass(0.01);
    bodyTube.addChild(std::move(massComponent));

    EXPECT_NEAR(0.041016634, bodyTube.getMass(), kEpsilon);
    EXPECT_NEAR(0.051016634, bodyTube.getSectionMass(), kEpsilon);

    bodyTube.setMassOverridden(true);
    bodyTube.setOverrideMass(0.02);

    EXPECT_NEAR(0.02, bodyTube.getMass(), kEpsilon);
    EXPECT_NEAR(0.03, bodyTube.getSectionMass(), kEpsilon);

    bodyTube.setSubcomponentsOverriddenMass(true);

    EXPECT_NEAR(0.02, bodyTube.getMass(), kEpsilon);
    EXPECT_NEAR(0.02, bodyTube.getSectionMass(), kEpsilon);
}

TEST(MassCalculator, DisabledStageMassAndCG)
{
    TestFalcon9Heavy     f9h;
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
    TestFalcon9Heavy     f9h;
    FlightConfiguration& config = f9h.rocket->getFlightConfiguration(f9h.fcid);

    const RigidBody first  = MassCalculator::calculateLaunch(config);
    const RigidBody second = MassCalculator::calculateLaunch(config);
    EXPECT_TRUE(first.getCM().exactlyEquals(second.getCM()));
    EXPECT_EQ(first.getIxx(), second.getIxx());
    EXPECT_EQ(first.getIyy(), second.getIyy());
    EXPECT_EQ(MassCalculator::modId(), ModId::zero());
    EXPECT_EQ(MassCalculator::getModId(), ModId::zero());

    // A mass change of a component (the parachute becomes 0.1 kg heavier): a new mass
    // modification id, and a new result.
    const ModId massModId = f9h.rocket->getMassModId();
    f9h.parachute->setOverrideMass(f9h.parachute->getComponentMass() + 0.1);
    f9h.parachute->setMassOverridden(true);
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

/// A rocket with one stage holding a body tube (0.5 m, radius 0.03 m) that holds an inner tube
/// motor mount (0.1 m, radius 0.01 m, starting 0.4 m from the body's front); events enabled. The
/// expectations are written with the two tubes' own mass properties: each has its CG in its
/// middle (0.25 m and 0.45 m from the nose) and both are on the axis.
class MassCalculatorRocketTest : public ::testing::Test
{
protected:
    /// The x of the body tube's CG and of the motor mount's.
    static constexpr double kBodyX  = 0.25;
    static constexpr double kMountX = 0.45;

    MassCalculatorRocketTest()
    {
        m_rocket.createFlightConfiguration(m_fcid);
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body  = &m_stage->addChild(std::make_unique<BodyTube>(0.5, 0.03));
        m_mount = &m_body->addChild(makeMount(0.4));
        m_rocket.setSelectedConfiguration(m_fcid);
        m_rocket.enableEvents();
    }

    /// An inner tube motor mount 0.1 m long of outer radius 0.01 m, @p offset from the front of
    /// its parent.
    [[nodiscard]] static std::unique_ptr<InnerTube> makeMount(double offset)
    {
        auto mount = std::make_unique<InnerTube>();
        mount->setAxialMethod(AxialMethod::TOP);
        mount->setAxialOffset(offset);
        mount->setLength(0.1);
        mount->setOuterRadius(0.01);
        mount->setMotorMount(true);
        return mount;
    }

    [[nodiscard]] FlightConfiguration& config() { return m_rocket.getFlightConfiguration(m_fcid); }

    /// The mass of the body tube alone.
    [[nodiscard]] double bodyMass() const { return m_body->getComponentMass(); }

    /// The mass of the motor mount tube alone.
    [[nodiscard]] double mountMass() const { return m_mount->getComponentMass(); }

    /// The mass of the structure: both tubes.
    [[nodiscard]] double structureMass() const { return bodyMass() + mountMass(); }

    /// The x of the structure's CG.
    [[nodiscard]] double structureCmx() const
    {
        return ((bodyMass() * kBodyX) + (mountMass() * kMountX)) / structureMass();
    }

    /// The rotational inertia of the structure: both tubes are on the axis.
    [[nodiscard]] double structureIxx() const
    {
        return (m_body->getRotationalUnitInertia() * bodyMass()) +
               (m_mount->getRotationalUnitInertia() * mountMass());
    }

    /// Puts an A8 into the mount and returns it.
    std::shared_ptr<const ThrustCurveMotor> addA8()
    {
        std::shared_ptr<const ThrustCurveMotor> motor = QtRocket::Test::motorA8();
        addMotor(*m_mount, m_fcid, motor);
        config().update();
        return motor;
    }

    /// The front of an A8 (0.07 m) in the mount: it fills the mount from 0.43 m to 0.5 m.
    static constexpr double kMotorX = 0.4 + (0.1 - 0.07);

    Rocket                m_rocket;
    FlightConfigurationId m_fcid{testFcid(0)};
    AxialStage*           m_stage{nullptr};
    BodyTube*             m_body{nullptr};
    InnerTube*            m_mount{nullptr};
};

TEST_F(MassCalculatorRocketTest, StructureAddsComponentsAndParallelAxisTerms)
{
    ASSERT_GT(bodyMass(), 0.0);
    ASSERT_GT(mountMass(), 0.0);
    EXPECT_DOUBLE_EQ(m_body->getComponentCG().x, kBodyX);
    EXPECT_DOUBLE_EQ(m_mount->getPosition().x + m_mount->getComponentCG().x, kMountX);

    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(structure.getMass(), structureMass(), 1e-15);
    const double cmx = structureCmx();
    EXPECT_NEAR(structure.getCM().x, cmx, 1e-15);
    // Ixx: each tube's rotational unit inertia times its mass; Iyy: their own plus both offsets.
    EXPECT_NEAR(structure.getIxx(), structureIxx(), 1e-15);
    const double iyy = (m_body->getLongitudinalUnitInertia() * bodyMass()) +
                       (m_mount->getLongitudinalUnitInertia() * mountMass()) +
                       (bodyMass() * (kBodyX - cmx) * (kBodyX - cmx)) +
                       (mountMass() * (kMountX - cmx) * (kMountX - cmx));
    EXPECT_NEAR(structure.getIyy(), iyy, 1e-15);
    EXPECT_EQ(structure.getIzz(), structure.getIyy());
}

TEST_F(MassCalculatorRocketTest, MassOverrideOfThisComponentOnlyScalesItsOwnInertia)
{
    m_body->setMassOverridden(true);
    m_body->setOverrideMass(0.4);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(structure.getMass(), 0.4 + mountMass(), 1e-15);
    // The body's own inertia follows the override mass (0.4), the mount keeps its mass.
    EXPECT_NEAR(structure.getIxx(),
                (m_body->getRotationalUnitInertia() * 0.4) +
                    (m_mount->getRotationalUnitInertia() * mountMass()),
                1e-15);
}

TEST_F(MassCalculatorRocketTest, SubcomponentMassOverrideScalesTheWholeSubtree)
{
    const RigidBody before = MassCalculator::calculateStructure(config());

    m_body->setMassOverridden(true);
    m_body->setSubcomponentsOverriddenMass(true);
    m_body->setOverrideMass(2 * structureMass());  // twice the geometric mass of the subtree
    const RigidBody after = MassCalculator::calculateStructure(config());

    // The whole override mass sits at the (massive) body's own CG, as in Java; the subtree's
    // bodies keep their places with twice their mass and inertia.
    EXPECT_NEAR(after.getMass(), 2 * structureMass(), 1e-15);
    EXPECT_DOUBLE_EQ(after.getCM().x, kBodyX);
    EXPECT_NEAR(after.getIxx(), 2 * before.getIxx(), 1e-15);
    // The body at the CM; the mount 0.2 m behind it.
    EXPECT_NEAR(
        after.getIyy(),
        2 * ((m_body->getLongitudinalUnitInertia() * bodyMass()) +
             (m_mount->getLongitudinalUnitInertia() * mountMass()) + (mountMass() * 0.2 * 0.2)),
        1e-15);
}

TEST_F(MassCalculatorRocketTest, SubcomponentMassOverrideOfAMasslessSubtreeDropsItsInertia)
{
    // Both tubes of a material without density.
    const QtRocket::Material massless =
        QtRocket::Material::newMaterial(QtRocket::Material::Type::BULK, "Massless", 0.0, true);
    m_body->setMaterial(massless);
    m_mount->setMaterial(massless);
    ASSERT_EQ(structureMass(), 0.0);
    m_body->setMassOverridden(true);
    m_body->setSubcomponentsOverriddenMass(true);
    m_body->setOverrideMass(0.3);
    const RigidBody structure = MassCalculator::calculateStructure(config());

    // No geometric mass to rescale: the scale is 0, so the override mass carries no inertia.
    EXPECT_DOUBLE_EQ(structure.getMass(), 0.3);
    EXPECT_DOUBLE_EQ(structure.getCM().x, kBodyX);
    EXPECT_EQ(structure.getIxx(), 0.0);
    EXPECT_EQ(structure.getIyy(), 0.0);
}

TEST_F(MassCalculatorRocketTest, MassOverrideOfANonMassiveComponentTakesItsChildrensCG)
{
    // A stage is not massive: its override mass sits at its children's CG, and they add their
    // own masses there.
    ASSERT_FALSE(m_stage->isMassive());
    m_stage->setMassOverridden(true);
    m_stage->setOverrideMass(1.0);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(structure.getMass(), 1.0 + structureMass(), 1e-15);
    EXPECT_NEAR(structure.getCM().x, structureCmx(), 1e-15);
}

TEST_F(MassCalculatorRocketTest, CGOverrideIsMeasuredFromTheComponentsFront)
{
    m_mount->setCGOverridden(true);
    m_mount->setOverrideCGX(0.0);
    const RigidBody structure = MassCalculator::calculateStructure(config());
    // The mount's CG moves to its front, 0.4 m.
    EXPECT_NEAR(structure.getCM().x,
                ((bodyMass() * kBodyX) + (mountMass() * 0.4)) / structureMass(), 1e-15);
}

TEST_F(MassCalculatorRocketTest, SubcomponentCGOverrideMovesTheChildrenToo)
{
    m_body->setCGOverridden(true);
    m_body->setOverrideCGX(0.1);
    const RigidBody own = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(own.getCM().x, ((bodyMass() * 0.1) + (mountMass() * kMountX)) / structureMass(),
                1e-15);

    m_body->setSubcomponentsOverriddenCG(true);
    const RigidBody all = MassCalculator::calculateStructure(config());
    EXPECT_DOUBLE_EQ(all.getCM().x, 0.1);
    EXPECT_NEAR(all.getMass(), structureMass(), 1e-15);
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
        const RigidBody launch = MassCalculator::calculate(MassCalculation::Type::LAUNCH, config(),
                                                           time, activeMotors(config(), states));
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

TEST_F(MassCalculatorRocketTest, BurnoutInFlightTakesTheCasingWhateverTheMotorTime)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = addA8();
    const RigidBody structure     = MassCalculator::calculateStructure(config());
    const RigidBody staticBurnout = MassCalculator::calculateBurnout(config());

    // Java's includesMotorCasing && !includesPropellant branch: the casing at
    // Motor.PSEUDO_TIME_BURNOUT, never at a state's motor time.
    const double casingMass = motor->getTotalMass(Motor::kPseudoTimeBurnout);
    const double casingX    = kMotorX + motor->getCMx(Motor::kPseudoTimeBurnout);
    EXPECT_DOUBLE_EQ(casingMass, motor->getBurnoutMass());
    EXPECT_DOUBLE_EQ(staticBurnout.getMass(), structure.getMass() + casingMass);
    EXPECT_NEAR(staticBurnout.getCM().x,
                ((structure.getMass() * structure.getCM().x) + (casingMass * casingX)) /
                    staticBurnout.getMass(),
                1e-15);

    // In flight, lit or not, at any time: bit for bit the static burnout calculation.
    const std::vector<MotorClusterState> unlit = motorStates(config());
    const std::vector<MotorClusterState> lit   = [&] {
        std::vector<MotorClusterState> states = motorStates(config());
        for (MotorClusterState& state : states)
        {
            state.ignite(0.0);
        }
        return states;
    }();
    ASSERT_EQ(lit.size(), 1U);
    for (const double time : {0.0, 0.3, 5.0})
    {
        expectIdenticalBodies(MassCalculator::calculate(MassCalculation::Type::BURNOUT, config(),
                                                        time, activeMotors(config(), unlit)),
                              staticBurnout, std::format("unlit at {}", time));
        expectIdenticalBodies(MassCalculator::calculate(MassCalculation::Type::BURNOUT, config(),
                                                        time, activeMotors(config(), lit)),
                              staticBurnout, std::format("lit at {}", time));
    }
}

TEST_F(MassCalculatorRocketTest, NullMotorStateIsABug)
{
    addA8();
    const std::vector<const MotorClusterState*> states{nullptr};
    EXPECT_THROW(static_cast<void>(MassCalculator::calculateMotor(config(), 0.5, states)),
                 QtRocket::BugError);
    EXPECT_THROW(static_cast<void>(MassCalculator::calculate(MassCalculation::Type::LAUNCH,
                                                             config(), 0.5, states)),
                 QtRocket::BugError);
    // STRUCTURE has no motor pass, so it never reads the states.
    EXPECT_NO_THROW(static_cast<void>(
        MassCalculator::calculate(MassCalculation::Type::STRUCTURE, config(), 0.5, states)));
}

TEST_F(MassCalculatorRocketTest, MotorsOfOneDesignationShareOneAnalysisRow)
{
    addA8();
    // A second mount at the body's front with another A8.
    InnerTube& front = m_body->addChild(makeMount(0.0));
    addMotor(front, m_fcid, QtRocket::Test::motorA8());
    config().update();

    const double    m      = 0.0164;
    const double    rearX  = kMotorX + 0.035;
    const double    frontX = (0.1 - 0.07) + 0.035;
    const RigidBody motors = MassCalculator::calculateMotor(config());
    ASSERT_NEAR(motors.getMass(), 2 * m, 1e-15);

    // Java keys a motor's row by the hash of its designation: both clusters share one row, with
    // the mass of one motor (the first cluster's) and the average CG of both clusters.
    const CMAnalysisMap    analysis = MassCalculator::getCMAnalysis(config());
    const CMAnalysisEntry& row =
        analysis.at(CMAnalysisEntry::keyOf(*front.getMotorConfig(m_fcid).getMotor()));
    EXPECT_EQ(row.name, "A8");
    EXPECT_DOUBLE_EQ(row.eachMass, m);
    EXPECT_DOUBLE_EQ(row.totalCM.weight, 2 * m);
    EXPECT_NEAR(row.totalCM.x, (rearX + frontX) / 2, 1e-15);
    EXPECT_NEAR(row.totalCM.x, motors.getCM().x, 1e-15);
    // Rows: the rocket, the stage, the body, the two mounts and one for both motors.
    EXPECT_EQ(analysis.size(), 6U);

    // The stage's row holds the structure and both motors.
    const RigidBody        structure = MassCalculator::calculateStructure(config());
    const CMAnalysisEntry& stageRow  = analysis.at(CMAnalysisEntry::keyOf(*m_stage));
    EXPECT_NEAR(stageRow.totalCM.weight, structure.getMass() + (2 * m), 1e-15);
}

TEST_F(MassCalculatorRocketTest, AnalysisRowOfAnAssemblyWhoseOverrideCoversItsSubcomponents)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = addA8();
    m_stage->setMassOverridden(true);
    m_stage->setSubcomponentsOverriddenMass(true);
    m_stage->setOverrideMass(1.0);

    const CMAnalysisMap analysis = MassCalculator::getCMAnalysis(config());

    // The structure pass gives the stage's row the override mass at its children's CG (a stage
    // is not massive); the motor pass then adds the motor below it, which the override does not
    // cover.
    const double           structureX = structureCmx();
    const double           m          = motor->getTotalMass(Motor::kPseudoTimeLaunch);
    const double           motorX     = kMotorX + motor->getCMx(Motor::kPseudoTimeLaunch);
    const CMAnalysisEntry& stageRow   = analysis.at(CMAnalysisEntry::keyOf(*m_stage));
    EXPECT_NEAR(stageRow.totalCM.weight, 1.0 + m, 1e-15);
    EXPECT_NEAR(stageRow.eachMass, 1.0 + m, 1e-15);
    EXPECT_NEAR(stageRow.totalCM.x, ((1.0 * structureX) + (m * motorX)) / (1.0 + m), 1e-15);

    // The physical components' rows keep their own masses: the override is the stage's.
    EXPECT_DOUBLE_EQ(analysis.at(CMAnalysisEntry::keyOf(*m_body)).eachMass, bodyMass());
    EXPECT_DOUBLE_EQ(analysis.at(CMAnalysisEntry::keyOf(*m_mount)).eachMass, mountMass());

    // The rocket's row is the whole launch calculation.
    const RigidBody        launch    = MassCalculator::calculateLaunch(config());
    const CMAnalysisEntry& rocketRow = analysis.at(CMAnalysisEntry::keyOf(m_rocket));
    EXPECT_TRUE(rocketRow.totalCM.exactlyEquals(launch.getCM()));
    EXPECT_NEAR(launch.getMass(), 1.0 + m, 1e-15);
}

TEST_F(MassCalculatorRocketTest, MotorOfAnInactiveMountAddsNothing)
{
    addMotor(*m_mount, m_fcid, QtRocket::Test::motorA8());
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
    // A cluster of four inner tubes on a ring.
    m_mount->setClusterConfiguration(ClusterConfiguration::configurations()[5]);
    ASSERT_EQ(m_mount->getClusterConfiguration().getXmlName(), "4-ring");
    addMotor(*m_mount, m_fcid, QtRocket::Test::motorA8());
    config().update();
    const std::vector<Coordinate> offsets = m_mount->getInstanceOffsets();
    ASSERT_EQ(offsets.size(), 4U);
    const double r2 = (offsets[0].y * offsets[0].y) + (offsets[0].z * offsets[0].z);
    ASSERT_GT(r2, 0.0);

    const MotorConfiguration& motorConfig = m_mount->getMotorConfig(m_fcid);
    const RigidBody           motors      = MassCalculator::calculateMotor(config());
    const double              m           = 0.0164;
    EXPECT_DOUBLE_EQ(motors.getMass(), 4 * m);
    EXPECT_DOUBLE_EQ(motors.getCM().y, 0.0);
    EXPECT_DOUBLE_EQ(motors.getCM().z, 0.0);
    // Ixx: 4 m (unit Ixx + r^2); Iyy: 4 m unit Iyy, without the radial offsets (as Java).
    EXPECT_NEAR(motors.getIxx(), 4 * m * (motorConfig.getUnitRotationalInertia() + r2), 1e-15);
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
    BodyTube& podMount = pods.addChild(std::make_unique<BodyTube>(0.1, 0.01, 0.0005));
    podMount.setMotorMount(true);
    addMotor(podMount, m_fcid, QtRocket::Test::motorA8());
    config().update();
    const double podMass = podMount.getComponentMass();
    ASSERT_GT(podMass, 0.0);

    const RigidBody structure = MassCalculator::calculateStructure(config());
    EXPECT_NEAR(structure.getMass(), structureMass() + (3 * podMass), 1e-15);
    // Three pods 120 degrees apart: the CG stays on the axis.
    EXPECT_NEAR(structure.getCM().y, 0.0, 1e-15);
    EXPECT_NEAR(structure.getCM().z, 0.0, 1e-15);
    // Each pod adds its own inertia and m d^2 about the axis.
    EXPECT_NEAR(
        structure.getIxx(),
        structureIxx() + (3 * podMass * (podMount.getRotationalUnitInertia() + (0.1 * 0.1))),
        1e-15);

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
    EXPECT_NEAR(podRow.totalCM.weight, 3 * (podMass + 0.0164), 1e-15);
    EXPECT_NEAR(podRow.eachMass, podMass + 0.0164, 1e-15);
}

TEST(MassCalculatorParallelStage, InactiveCoreStageStillCarriesActiveBoosters)
{
    TestFalcon9Heavy     f9h;
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

TEST(MassCalculatorParallelStage, OverrideOfAnInactiveCoreStageIsIgnored)
{
    TestFalcon9Heavy     f9h;
    FlightConfiguration& config = f9h.rocket->getEmptyConfiguration();
    config.setOnlyStage(kFalcon9hBoosterStageNumber);
    const RigidBody boosters = MassCalculator::calculateStructure(config);

    // Mass and CG overrides on the core stage that cover its subcomponents, the boosters
    // included.
    AxialStage& core = *f9h.coreStage;
    core.setMassOverridden(true);
    core.setSubcomponentsOverriddenMass(true);
    core.setOverrideMass(10.0);
    core.setCGOverridden(true);
    core.setSubcomponentsOverriddenCG(true);
    core.setOverrideCGX(0.0);

    // Core inactive, boosters active: Java applies the overrides of active components only
    // (MassCalculation.java's isComponentActive() branch), so the boosters keep their own mass.
    config.setAllStages();
    config.setStageActive(kFalcon9hPayloadStageNumber, false);
    config.setStageActive(kFalcon9hCoreStageNumber, false, false);
    ASSERT_TRUE(config.isStageActive(kFalcon9hBoosterStageNumber));
    const RigidBody onlyBoosters = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(onlyBoosters.getMass(), boosters.getMass(), 1e-15);
    EXPECT_NEAR(onlyBoosters.getCM().x, boosters.getCM().x, 1e-15);
    EXPECT_NEAR(onlyBoosters.getIxx(), boosters.getIxx(), 1e-15);
    EXPECT_NEAR(onlyBoosters.getIyy(), boosters.getIyy(), 1e-15);

    // Core active as well: the override replaces the mass of the core and the boosters, all at
    // the core stage's front (its position in the rocket).
    config.setStageActive(kFalcon9hCoreStageNumber, true, true);
    const RigidBody overridden = MassCalculator::calculateStructure(config);
    EXPECT_NEAR(overridden.getMass(), 10.0, 1e-12);
    EXPECT_NEAR(overridden.getCM().x, core.getPosition().x, 1e-12);
    EXPECT_NEAR(overridden.getCM().y, 0.0, 1e-12);
}

TEST(MassCalculatorParallelStage, BoosterMotorsAreOneClusterPerBooster)
{
    TestFalcon9Heavy     f9h;
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
    TestEstesAlphaIII          alpha;
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
                                               activeMotors(config, states))));
    // STRUCTURE ignores the motor states.
    EXPECT_TRUE(same(MassCalculator::calculateStructure(config),
                     MassCalculator::calculate(MassCalculation::Type::STRUCTURE, config, 0.0,
                                               activeMotors(config, states))));
}

TEST(MassCalculatorTypes, AnEmptyListOfMotorStatesMeansNoMotors)
{
    TestEstesAlphaIII          alpha;
    const FlightConfiguration& config = alpha.rocket->getFlightConfiguration(testFcid(1));

    // Unlike a static calculation, which takes the configuration's active motors. The empty
    // motor pass is still merged (x * m / m), so the CG is compared with a tolerance.
    const RigidBody structure = MassCalculator::calculateStructure(config);
    const RigidBody noMotors =
        MassCalculator::calculate(MassCalculation::Type::LAUNCH, config, 0.0, {});
    EXPECT_GT(MassCalculator::calculateLaunch(config).getMass(), structure.getMass());
    EXPECT_EQ(noMotors.getMass(), structure.getMass());
    EXPECT_NEAR(noMotors.getCM().x, structure.getCM().x, 1e-15);
    EXPECT_NEAR(noMotors.getIyy(), structure.getIyy(), 1e-15);
}

TEST(MassCalculatorTypes, LaunchIsStructurePlusMotorAndBurnoutIsBetween)
{
    TestEstesAlphaIII          alpha;
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
