// swing's IntegrationTest (info.openrocket.swing.IntegrationTest): a design is loaded, changed,
// simulated, and the changes are undone and redone, with the mass, the aerodynamics and a
// flight checked after every step. The test sits here, with the loader, because it needs a
// loaded design and nothing of Swing: the two UndoRedoAction objects of the Java test show the
// document's undo and redo descriptions and whether undo and redo are available, and are told
// to look again by the document (UndoRedoListener.setAllValues()); UndoRedoMirror below is that.
//
// The set-up is the Java test's: tests/data/ork/simplerocket.ork (a copy of
// swing/src/test/resources/simplerocket.ork, file format 1.2, gzip) read as a stream, no
// component presets, and a motor database that holds one motor, the one of
// tests/data/motors/Estes_A8.rse (a copy of swing/src/test/resources/Estes_A8.rse). One thing
// is not the Java test's: the preference store. The Java test runs under the application's
// preferences (swing's CoreServicesModule), from which a simulation takes what its file does
// not state; here, as for every loaded design of the tests and as for the Java probe, they
// are the preferences of OpenRocket's core tests (DesignFileEnvironment).
//
// Every expectation of the Java test is here with its tolerance (junitEquals()). Beside each
// centre of mass, mass, centre of pressure and CNa stands the value OpenRocket computes at that
// step, printed at full precision by the Java probe IntegrationProbe2 of the probes of tier 9c,
// part "recorded-files" (the JUnit literals have three digits), compared to a relative 1e-9.
//
// The flights. The design's simulation flies in a wind of 2 m/s with a turbulence of 0.2 m/s,
// and its random seed is not fixed: the Java test flies one realisation of the turbulence,
// another one in every run of the test, and accepts 1.0 m around its literals (over 20 seeds
// OpenRocket's altitudes spread by 0.33 to 0.50 m per step and stay within 0.65 m of the
// literals). QtRocket's turbulence is another sequence for the same seed (PinkNoise draws from
// another generator, an earlier decision), so its realisations are others too, and the test
// has to be the same in every run: each flight is flown with the three seeds of kSeeds, set on
// the options before the run (which changes nothing else: a seed that is not fixed is no
// change of the simulation), and each of the three altitudes has to be within Java's 1.0 m of
// Java's literal.

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Uuid.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::BarrowmanCalculator;
using QtRocket::Coordinate;
using QtRocket::EngineBlock;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::GeneralMotorLoader;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::MassCalculator;
using QtRocket::MassComponent;
using QtRocket::NoseCone;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Uuid;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::junitEquals;

/// A description of an undo step, or none (Java: null).
using Description = std::optional<std::string>;

/// The random seeds each flight is flown with (see the top of this file).
constexpr std::array<int, 3> kSeeds{0, 1, 2};

/// What swing's UndoRedoAction shows of a document for "Undo" or for "Redo": the name of the
/// menu entry ("Undo", or "Undo (<description>)" when the step has one) and whether the entry
/// is enabled. As in Java the values are set when the action is made and again whenever the
/// document tells its listeners that they may have changed, and at no other time: a check
/// reads what the last undoRedoChanged() left, not the document.
struct UndoRedoMirror
{
    std::string name;
    bool        enabled{false};

    /// UndoRedoAction.setAllValues().
    void set(std::string_view action, const Description& description, bool available)
    {
        name    = description.has_value() ? std::format("{} ({})", action, *description)
                                          : std::string(action);
        enabled = available;
    }
};

/// The launch centre of mass and mass of a flight configuration, and its worst-case centre of
/// pressure with the CNa as weight: what IntegrationTest.checkCgCp() looks at.
struct CgCp
{
    double cgx;
    double mass;
    double cpx;
    double cna;
};

// What OpenRocket computes in the six states the test goes through (IntegrationProbe2, at
// full precision).
/// As loaded.
constexpr CgCp kLoaded{.cgx  = 0.2479105122064276,
                       .mass = 0.06447338970521777,
                       .cpx  = 0.3308343828245423,
                       .cna  = 14.87897002914394};
/// With the mass component at 10 g.
constexpr CgCp kMass10{.cgx  = 0.23006380041681831,
                       .mass = 0.07447338970521777,
                       .cpx  = 0.3308343828245423,
                       .cna  = 14.87897002914394};
/// With the mass component at 10 g and without the nose cone.
constexpr CgCp kNoNoseCone{.cgx  = 0.16311470143788628,
                           .mass = 0.06131741354327119,
                           .cpx  = 0.27499999999999997,
                           .cna  = 12.87897002914394};
/// With the mass component at 15 g.
constexpr CgCp kMass15{.cgx  = 0.22282466031950948,
                       .mass = 0.07947338970521778,
                       .cpx  = 0.3308343828245423,
                       .cna  = 14.87897002914394};
/// With the mass component at 15 g and the engine block moved from the inner tube to the front
/// of the body tube (it takes the tube's inner diameter there and so gains mass).
constexpr CgCp kMoved{.cgx  = 0.2211983273423915,
                      .mass = 0.07963361093055085,
                      .cpx  = 0.3308343828245423,
                      .cna  = 14.87897002914394};
/// The same with the mass component at 20 g.
constexpr CgCp kMass20{.cgx  = 0.21492432306827772,
                       .mass = 0.08463361093055086,
                       .cpx  = 0.3308343828245423,
                       .cna  = 14.87897002914394};

/// The one motor of tests/data/motors/Estes_A8.rse (IntegrationTest.readMotor()), or null with
/// a test failure when the file cannot be read.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> readMotor()
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "Estes_A8.rse");
    if (!bytes)
    {
        ADD_FAILURE() << "Problem in unit test, cannot find Estes_A8.rse";
        return nullptr;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        GeneralMotorLoader().load(*bytes, "Estes_A8.rse");
    if (!motors || motors->empty())
    {
        ADD_FAILURE() << "Could not load motor";
        return nullptr;
    }
    Result<ThrustCurveMotor> motor = motors->front().build();
    if (!motor)
    {
        ADD_FAILURE() << motor.error().toString();
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

class IntegrationTest : public ::testing::Test
{
protected:
    /// IntegrationTest.setUp() and the start of testSimpleRocket(): the motor database with
    /// its one motor (MotorDbProvider), no component presets (EmptyComponentDbProvider), the
    /// design read as a stream (loadRocket()), the two actions and the flight configuration of
    /// the first simulation.
    void SetUp() override
    {
        const std::shared_ptr<const ThrustCurveMotor> motor = readMotor();
        ASSERT_NE(motor, nullptr);
        m_motors.addMotor(motor);
        ASSERT_EQ(m_motors.getMotorSets().size(), 1U);
        m_environment =
            std::make_unique<DesignFileEnvironment>(DesignFileEnvironment::Presets::NONE, m_motors);

        Result<std::vector<std::byte>> design =
            QtRocket::readFile(QtRocket::Test::testDataDir() / "ork" / "simplerocket.ork");
        ASSERT_TRUE(design.has_value()) << "Problem in unit test, cannot find simplerocket.ork";
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment->context()).load(std::move(*design));
        ASSERT_TRUE(loaded.has_value())
            << "RocketLoadException while loading file simplerocket.ork : "
            << loaded.error().message;
        EXPECT_TRUE(loaded->warnings.empty());
        m_document = std::move(loaded->document);

        setAllValues();
        m_listening  = m_document->undoRedoChanged().connect([this] { setAllValues(); });
        m_fcid       = m_document->getSimulation(0)->getFlightConfigurationId();
        m_conditions = FlightConditions(rocket().getFlightConfiguration(m_fcid));
    }

    [[nodiscard]] Rocket& rocket() { return m_document->getRocket(); }

    /// The mass component of the design (the first child of the body tube), found again by its
    /// id, as the Java test finds it after every undo and redo; null when the rocket has no
    /// such component.
    [[nodiscard]] MassComponent* massComponent()
    {
        const Uuid& id =
            m_massComponentId.has_value()
                ? *m_massComponentId
                : m_massComponentId.emplace(rocket().getChild(0).getChild(1).getChild(0).getId());
        return dynamic_cast<MassComponent*>(rocket().findComponent(id));
    }

    /// IntegrationTest.checkUndoState(): the names and the enabled states of the two actions.
    void checkUndoState(const Description& undo, const Description& redo) const
    {
        EXPECT_EQ(m_undoAction.name, undo.has_value() ? "Undo (" + *undo + ")" : "Undo");
        EXPECT_EQ(m_undoAction.enabled, undo.has_value());
        EXPECT_EQ(m_redoAction.name, redo.has_value() ? "Redo (" + *redo + ")" : "Redo");
        EXPECT_EQ(m_redoAction.enabled, redo.has_value());
    }

    /// IntegrationTest.checkCgCp() with the literals @p junit and Java's tolerances, and the
    /// same four values against @p java, OpenRocket's at full precision.
    void checkCgCp(const CgCp& junit, const CgCp& java)
    {
        checkCg(junit, java);
        checkCp(junit, java);
    }

    /// The first half of checkCgCp(): the launch centre of mass and mass.
    void checkCg(const CgCp& junit, const CgCp& java)
    {
        const FlightConfiguration& config = rocket().getFlightConfiguration(m_fcid);
        const RigidBody            launch = MassCalculator::calculateLaunch(config);
        const Coordinate&          cg     = launch.getCenterOfMass();
        EXPECT_TRUE(junitEquals(junit.cgx, cg.x, 0.001));
        EXPECT_TRUE(junitEquals(junit.mass, cg.weight, 0.0005));
        EXPECT_NEAR(cg.x, java.cgx, 1e-9 * java.cgx);
        EXPECT_NEAR(cg.weight, java.mass, 1e-9 * java.mass);
    }

    /// The second half of checkCgCp(): the worst-case centre of pressure and its CNa.
    void checkCp(const CgCp& junit, const CgCp& java)
    {
        const FlightConfiguration& config = rocket().getFlightConfiguration(m_fcid);
        const Coordinate           cp     = m_aeroCalc.getWorstCP(config, m_conditions, nullptr);
        EXPECT_TRUE(junitEquals(junit.cpx, cp.x, 0.001));
        EXPECT_TRUE(junitEquals(junit.cna, cp.weight, 0.1));
        EXPECT_NEAR(cp.x, java.cpx, 1e-9 * java.cpx);
        EXPECT_NEAR(cp.weight, java.cna, 1e-9 * java.cna);
    }

    /// The maximum altitude of the first simulation of the document, flown with the random
    /// seed @p seed; NaN, with a test failure, when the run fails.
    [[nodiscard]] double flownAltitude(int seed)
    {
        Simulation& simulation = *m_document->getSimulation(0);
        simulation.getOptions().setRandomSeed(seed);
        const Result<void> run = simulation.simulate();
        if (!run)
        {
            ADD_FAILURE() << run.error().toString();
            return std::numeric_limits<double>::quiet_NaN();
        }
        const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
        if (data == nullptr || data->getBranchCount() == 0)
        {
            ADD_FAILURE() << "no simulated branch";
            return std::numeric_limits<double>::quiet_NaN();
        }
        return data->getBranch(0).getMaximum(
            FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
    }

    /// IntegrationTest.checkAlt(): the first simulation is flown and its maximum altitude is
    /// within 1.0 m of @p expected, for each seed of kSeeds.
    void checkAlt(double expected)
    {
        for (const int seed : kSeeds)
        {
            EXPECT_TRUE(junitEquals(expected, flownAltitude(seed), 1.0)) << "seed " << seed;
        }
    }

    // Declared so that the preference store of the environment outlives the document.
    ThrustCurveMotorSetDatabase            m_motors;
    std::unique_ptr<DesignFileEnvironment> m_environment;
    std::unique_ptr<OpenRocketDocument>    m_document;

private:
    /// UndoRedoAction.setAllValues() of both actions.
    void setAllValues()
    {
        m_undoAction.set("Undo", m_document->getUndoDescription(), m_document->isUndoAvailable());
        m_redoAction.set("Redo", m_document->getRedoDescription(), m_document->isRedoAvailable());
    }

    UndoRedoMirror                       m_undoAction;
    UndoRedoMirror                       m_redoAction;
    QtRocket::Signal<>::ScopedConnection m_listening;
    BarrowmanCalculator                  m_aeroCalc;
    FlightConfigurationId                m_fcid;
    FlightConditions                     m_conditions;
    std::optional<Uuid>                  m_massComponentId;
};

// IntegrationTest.testSimpleRocket: "Tests loading a simple rocket design, modifying it,
// simulating it and the undo/redo mechanism in various combinations."
TEST_F(IntegrationTest, SimpleRocket)
{
    // Test undo state
    checkUndoState({}, {});

    // Compute cg+cp + altitude
    checkCgCp({.cgx = 0.248, .mass = 0.0645, .cpx = 0.331, .cna = 14.88}, kLoaded);
    checkAlt(48.8);

    // Mass modification
    m_document->addUndoPosition("Modify mass");
    checkUndoState({}, {});
    ASSERT_NE(massComponent(), nullptr);
    massComponent()->setComponentMass(0.01);
    checkUndoState("Modify mass", {});

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.230, .mass = 0.0745, .cpx = 0.331, .cna = 14.88}, kMass10);
    checkAlt(37.4);

    // Non-change
    m_document->addUndoPosition("No change");
    checkUndoState("Modify mass", {});

    // Non-functional change
    m_document->addUndoPosition("Name change");
    checkUndoState("Modify mass", {});
    massComponent()->setName("Foobar component");
    checkUndoState("Name change", {});

    // Check cg+cp
    checkCgCp({.cgx = 0.230, .mass = 0.0745, .cpx = 0.331, .cna = 14.88}, kMass10);

    // Aerodynamic modification
    m_document->addUndoPosition("Remove component");
    checkUndoState("Name change", {});
    static_cast<void>(rocket().getChild(0).removeChild(0));
    checkUndoState("Remove component", {});

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.163, .mass = 0.0613, .cpx = 0.275, .cna = 12.88}, kNoNoseCone);
    checkAlt(45.6);

    // Undo "Remove component" change
    m_document->undo();
    EXPECT_NE(dynamic_cast<const NoseCone*>(&rocket().getChild(0).getChild(0)), nullptr);
    checkUndoState("Name change", "Remove component");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.230, .mass = 0.0745, .cpx = 0.331, .cna = 14.88}, kMass10);
    checkAlt(37.4);

    // Undo "Name change" change
    m_document->undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_EQ(massComponent()->getName(), "Extra mass");
    checkUndoState("Modify mass", "Name change");

    // Check cg+cp
    checkCgCp({.cgx = 0.230, .mass = 0.0745, .cpx = 0.331, .cna = 14.88}, kMass10);

    // Undo "Modify mass" change
    m_document->undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(junitEquals(0.0, massComponent()->getComponentMass(), 0.0));
    checkUndoState({}, "Modify mass");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.248, .mass = 0.0645, .cpx = 0.331, .cna = 14.88}, kLoaded);
    checkAlt(48.87);

    // Redo "Modify mass" change
    m_document->redo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(junitEquals(0.010, massComponent()->getComponentMass(), 0.00001));
    checkUndoState("Modify mass", "Name change");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.230, .mass = 0.0745, .cpx = 0.331, .cna = 14.88}, kMass10);
    checkAlt(37.4);

    // Mass modification
    m_document->addUndoPosition("Modify mass2");
    checkUndoState("Modify mass", "Name change");
    massComponent()->setComponentMass(0.015);
    checkUndoState("Modify mass2", {});

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.223, .mass = 0.0795, .cpx = 0.331, .cna = 14.88}, kMass15);
    checkAlt(33);

    // Perform component movement
    m_document->startUndo("Move component");
    rocket().freeze();
    RocketComponent&                 bodytube    = rocket().getChild(0).getChild(1);
    RocketComponent&                 innertube   = bodytube.getChild(2);
    const RocketComponent&           engineblock = innertube.getChild(0);
    std::unique_ptr<RocketComponent> removed     = innertube.removeChild(&engineblock);
    ASSERT_NE(removed, nullptr);
    bodytube.addChild(std::move(removed), 0);
    checkUndoState("Modify mass2", {});
    rocket().thaw();
    checkUndoState("Move component", {});
    m_document->stopUndo();

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.221, .mass = 0.0797, .cpx = 0.331, .cna = 14.88}, kMoved);
    checkAlt(33);

    // Modify mass without setting undo description
    ASSERT_NE(massComponent(), nullptr);
    massComponent()->setComponentMass(0.020);
    checkUndoState("Modify mass2", {});

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.215, .mass = 0.0847, .cpx = 0.331, .cna = 14.88}, kMass20);
    checkAlt(29.0);

    // Undo "Modify mass2" change
    m_document->undo();
    ASSERT_NE(massComponent(), nullptr);
    EXPECT_TRUE(junitEquals(0.015, massComponent()->getComponentMass(), 0.0000001));
    checkUndoState("Move component", "Modify mass2");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.221, .mass = 0.0797, .cpx = 0.331, .cna = 14.88}, kMoved);
    checkAlt(33);

    // Undo "Move component" change
    m_document->undo();
    EXPECT_NE(
        dynamic_cast<const EngineBlock*>(&rocket().getChild(0).getChild(1).getChild(2).getChild(0)),
        nullptr);
    checkUndoState("Modify mass2", "Move component");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.223, .mass = 0.0795, .cpx = 0.331, .cna = 14.88}, kMass15);
    checkAlt(33);

    // Redo "Move component" change
    m_document->redo();
    EXPECT_NE(dynamic_cast<const EngineBlock*>(&rocket().getChild(0).getChild(1).getChild(0)),
              nullptr);
    checkUndoState("Move component", "Modify mass2");

    // Check cg+cp + altitude
    checkCgCp({.cgx = 0.221, .mass = 0.0797, .cpx = 0.331, .cna = 14.88}, kMoved);
    checkAlt(33);
}

/// The smallest and the largest of the altitudes of @p count flights with the seeds 0 to
/// @p count - 1, as "<min> .. <max>".
[[nodiscard]] std::string altitudeBand(const std::function<double(int)>& flownAltitude, int count)
{
    double lowest  = std::numeric_limits<double>::infinity();
    double highest = -std::numeric_limits<double>::infinity();
    for (int seed = 0; seed < count; seed++)
    {
        const double altitude = flownAltitude(seed);
        lowest                = std::min(lowest, altitude);
        highest               = std::max(highest, altitude);
    }
    return std::format("{:.4f} .. {:.4f}", lowest, highest);
}

// A measurement, not a test: the spread of the maximum altitude over 20 realisations of the
// turbulence, as IntegrationProbe2 prints OpenRocket's: 48.2260 .. 48.6655 as loaded,
// 37.0094 .. 37.5086 with the mass component at 10 g, 32.5967 .. 33.0878 at 15 g. QtRocket's
// were 48.2474 .. 48.7515, 37.0331 .. 37.5628 and 32.6182 .. 33.1288 when the test was written
// (other realisations of the same turbulence). Run with --gtest_also_run_disabled_tests.
TEST_F(IntegrationTest, DISABLED_PrintsTheSpreadOfTheAltitudes)
{
    const auto flown = [this](int seed) { return flownAltitude(seed); };
    std::cout << "as loaded: " << altitudeBand(flown, 20) << "\n";
    ASSERT_NE(massComponent(), nullptr);
    massComponent()->setComponentMass(0.01);
    std::cout << "10 g:      " << altitudeBand(flown, 20) << "\n";
    massComponent()->setComponentMass(0.015);
    std::cout << "15 g:      " << altitudeBand(flown, 20) << "\n";
}

}  // namespace
