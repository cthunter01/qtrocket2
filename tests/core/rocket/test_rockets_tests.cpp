// The fixtures of tests/core/rocket/TestRockets.h against OpenRocket's TestRockets.java, for what
// the golden files do not hold: the automatic radius flags the Java makers leave, the data of the
// test motors, the separation settings of the stages, and the modification ids drawn while a
// rocket is built (the events it fires). The expected listings and numbers are the output of a
// Java program run on the thirteen makers (and on makeFalcon9Heavy() followed by addCoreFins())
// with OpenRocket's compiled core (JDK 17), which prints the same lines as the functions below.
// TestRocketsGolden (tests/core/goldens/test_rockets_golden_tests.cpp) compares everything else.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenGeometry.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StageSeparationConfiguration;
using QtRocket::ThrustCurveMotor;
using QtRocket::Strings::javaDoubleToString;
using QtRocket::Test::goldenPathOf;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestBigBlue;
using QtRocket::Test::TestClusterPods;
using QtRocket::Test::TestEndPlateRocket;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestEstesAlphaIIIWithInlinePod;
using QtRocket::Test::TestEstesAlphaIIIWithMotorPods;
using QtRocket::Test::TestEstesAlphaIIIWithPods;
using QtRocket::Test::TestEstesAlphaIIIWithSecondMotor;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestIsoHaisu;
using QtRocket::Test::TestMultiStageEventTestRocket;
using QtRocket::Test::TestSimple2Stage;

/// The automatic flags of @p component, as the Java program prints them: the fore and aft radius
/// of a Transition, the outer radius of a BodyTube, both radii of a RingComponent, the radius of
/// a MassObject, and whether a MotorMount is one.
[[nodiscard]] std::string flagsOf(const RocketComponent& component)
{
    std::string flags;
    if (const auto* transition = dynamic_cast<const QtRocket::Transition*>(&component))
    {
        flags += std::format(" foreAuto={} aftAuto={}", transition->isForeRadiusAutomatic(),
                             transition->isAftRadiusAutomatic());
    }
    if (const auto* tube = dynamic_cast<const QtRocket::BodyTube*>(&component))
    {
        flags += std::format(" outerAuto={}", tube->isOuterRadiusAutomatic());
    }
    if (const auto* ring = dynamic_cast<const QtRocket::RingComponent*>(&component))
    {
        flags += std::format(" outerAuto={} innerAuto={}", ring->isOuterRadiusAutomatic(),
                             ring->isInnerRadiusAutomatic());
    }
    if (const auto* massObject = dynamic_cast<const QtRocket::MassObject*>(&component))
    {
        flags += std::format(" radiusAuto={}", massObject->isRadiusAutomatic());
    }
    if (const auto* mount = dynamic_cast<const MotorMount*>(&component))
    {
        flags += std::format(" motorMount={}", mount->isMotorMount());
    }
    return flags;
}

/// One line per component of @p rocket, in tree order: its path, class, name and flags.
[[nodiscard]] std::vector<std::string> flagLines(const Rocket& rocket)
{
    std::vector<std::string> lines;
    for (const RocketComponent& component : rocket.subtree())
    {
        lines.push_back(std::format("{} {} \"{}\"{}", goldenPathOf(component),
                                    QtRocket::className(component.kind()), component.getName(),
                                    flagsOf(component)));
    }
    return lines;
}

/// Java's Arrays.toString(double[]).
[[nodiscard]] std::string javaArray(std::span<const double> values)
{
    std::string text = "[";
    for (const double value : values)
    {
        if (text.size() > 1)
        {
            text += ", ";
        }
        text += javaDoubleToString(value);
    }
    return text + ']';
}

/// Whether @p id is one of the constant ids of TestRockets.java (the default id, TEST_FCID_0 ...
/// TEST_FCID_4, FALCON_9H_FCID_1) rather than one a maker draws at random.
[[nodiscard]] bool isConstantId(const FlightConfigurationId& id)
{
    if (id.isDefaultId() ||
        id == FlightConfigurationId::fromString("test_config #1: [ M1350, G77]"))
    {
        return true;
    }
    for (int n = 0; n < 5; n++)
    {
        if (id == testFcid(n))
        {
            return true;
        }
    }
    return false;
}

/// The name the Java program prints for the configuration id @p id of @p rocket: its key, or,
/// for an id a maker drew at random, "<config n>" with its index among the rocket's ids.
[[nodiscard]] std::string idName(const Rocket& rocket, const FlightConfigurationId& id)
{
    if (isConstantId(id))
    {
        return id.toString();
    }
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    for (std::size_t i = 0; i < ids.size(); i++)
    {
        if (ids[i] == id)
        {
            return std::format("<config {}>", i);
        }
    }
    return "<unknown>";
}

/// The line the Java program prints for the motor @p motorConfig holds, in the mount at @p path
/// in the configuration named @p id.
[[nodiscard]] std::string motorLine(const std::string& id, const std::string& path,
                                    const MotorConfiguration& motorConfig)
{
    const auto* motor = dynamic_cast<const ThrustCurveMotor*>(motorConfig.getMotor().get());
    if (motor == nullptr)
    {
        return std::format("MOTOR {} {} is not a thrust curve motor", id, path);
    }
    std::string cg;
    for (const Coordinate& point : motor->getCGPoints())
    {
        cg += std::format("({},{},{},{})", javaDoubleToString(point.x), javaDoubleToString(point.y),
                          javaDoubleToString(point.z), javaDoubleToString(point.weight));
    }
    return std::format(
        "MOTOR {} {} {}|{}|{}|{}|{}|delays={}|d={}|l={}|time={}|thrust={}|cg={}|digest={}|"
        "ejection={}|ignition={}/{}",
        id, path, motor->getManufacturer().getDisplayName(), motor->getDesignation(),
        motor->getDescription(), motor->getCaseInfo(), QtRocket::enumName(motor->getMotorType()),
        javaArray(motor->getStandardDelays()), javaDoubleToString(motor->getDiameter()),
        javaDoubleToString(motor->getLength()), javaArray(motor->getTimePoints()),
        javaArray(motor->getThrustPoints()), cg, motor->getDigest(),
        javaDoubleToString(motorConfig.getEjectionDelay()),
        QtRocket::name(motorConfig.getIgnitionEvent()),
        javaDoubleToString(motorConfig.getIgnitionDelay()));
}

/// The ids of the configurations of @p rocket: the default first, then in creation order.
[[nodiscard]] std::vector<FlightConfigurationId> configurationIds(const Rocket& rocket)
{
    std::vector<FlightConfigurationId> ids{FlightConfigurationId::defaultValueId()};
    std::ranges::copy(rocket.getIds(), std::back_inserter(ids));
    return ids;
}

/// One line per motor of @p rocket: for every configuration (the default first, then in creation
/// order), every motor mount in tree order that holds a motor in it.
[[nodiscard]] std::vector<std::string> motorLines(const Rocket& rocket)
{
    std::vector<std::string> lines;
    for (const FlightConfigurationId& id : configurationIds(rocket))
    {
        for (const RocketComponent& component : rocket.subtree())
        {
            const auto* mount = dynamic_cast<const MotorMount*>(&component);
            if (mount == nullptr || !mount->isMotorMount())
            {
                continue;
            }
            const MotorConfiguration& motorConfig = mount->getMotorConfig(id);
            if (!motorConfig.isEmpty())
            {
                lines.push_back(
                    motorLine(idName(rocket, id), goldenPathOf(component), motorConfig));
            }
        }
    }
    return lines;
}

/// One line per stage of @p rocket and configuration (the default first, then in creation
/// order; the stages in tree order): the separation event and delay of the stage in it, and
/// whether its separation set holds the default object under that id (Java: isDefault(fcid)).
[[nodiscard]] std::vector<std::string> separationLines(const Rocket& rocket)
{
    std::vector<std::string> lines;
    for (const FlightConfigurationId& id : configurationIds(rocket))
    {
        for (const RocketComponent& component : rocket.subtree())
        {
            const auto* stage = dynamic_cast<const AxialStage*>(&component);
            if (stage == nullptr)
            {
                continue;
            }
            const StageSeparationConfiguration& separation =
                stage->getSeparationConfigurations().get(id);
            lines.push_back(std::format(
                "SEPARATION {} {} {}/{} default={}", idName(rocket, id), goldenPathOf(component),
                QtRocket::separationEventName(separation.getSeparationEvent()),
                javaDoubleToString(separation.getSeparationDelay()),
                stage->getSeparationConfigurations().isDefault(id)));
        }
    }
    return lines;
}

/// @p lines as one text, a line each.
[[nodiscard]] std::string joined(std::span<const std::string> lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line;
        text += '\n';
    }
    return text;
}

// ============================================================================ automatic flags

/// The Java program's lines for the Estes Alpha III's stage and nose cone, which every Alpha III
/// variant keeps.
constexpr std::string_view kAlphaStageAndNose =
    "/0 AxialStage \"Stage\"\n"
    "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n";

/// The Java program's lines for what the Alpha III's body tube holds but its fins, under @p body
/// with the first of them at child @p first: the launch lug, the motor mount with its engine
/// block, the parachute and the centering rings. (A fin set and a launch lug have no automatic
/// flag: their lines hold the class and the name only.)
[[nodiscard]] std::string alphaInternals(std::string_view body, int first)
{
    return std::format(
        "{0}/{1} LaunchLug \"Launch Lugs\"\n"
        "{0}/{2} InnerTube \"Motor Mount Tube\" outerAuto=false innerAuto=false "
        "motorMount=true\n"
        "{0}/{2}/0 EngineBlock \"Engine Block\" outerAuto=false innerAuto=false\n"
        "{0}/{3} Parachute \"Parachute\" radiusAuto=false\n"
        "{0}/{4} CenteringRing \"Centering Rings\" outerAuto=true innerAuto=true\n",
        body, first, first + 1, first + 2, first + 3);
}

/// The lines for the Alpha III's body tube named @p name with its fins and what else it holds.
[[nodiscard]] std::string alphaBody(std::string_view name)
{
    return std::format(
               "/0/1 BodyTube \"{}\" outerAuto=false motorMount=false\n"
               "/0/1/0 TrapezoidFinSet \"3 Fin Set\"\n",
               name) +
           alphaInternals("/0/1", 1);
}

TEST(TestRocketsFixture, EstesAlphaIIIFlagsAreOpenRockets)
{
    const TestEstesAlphaIII alpha;
    EXPECT_EQ(joined(flagLines(*alpha.rocket)),
              "/ Rocket \"Estes Alpha III / Code Verification Rocket\"\n" +
                  std::string{kAlphaStageAndNose} + alphaBody("Body Tube"));
}

TEST(TestRocketsFixture, BetaFlagsAreOpenRockets)
{
    const TestBeta beta;
    EXPECT_EQ(joined(flagLines(*beta.rocket)),
              "/ Rocket \"Kit-bash Beta\"\n"
              "/0 AxialStage \"Sustainer Stage\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n" +
                  alphaBody("Sustainer Body Tube") +
                  "/1 AxialStage \"Booster Stage\"\n"
                  "/1/0 BodyTube \"Booster Body\" outerAuto=false motorMount=false\n"
                  "/1/0/0 TubeCoupler \"Coupler\" outerAuto=true innerAuto=false\n"
                  "/1/0/1 TrapezoidFinSet \"Booster Fins\"\n"
                  "/1/0/2 InnerTube \"Booster MMT\" outerAuto=false innerAuto=false "
                  "motorMount=true\n"
                  "/1/0/3 LaunchLug \"Launch Lugs\"\n"
                  "/1/1 Transition \"Booster Tail Cone\" foreAuto=false aftAuto=false\n");
}

/// The Java program's lines for the Falcon 9 Heavy.
constexpr std::string_view kFalcon9HeavyFlags =
    "/ Rocket \"Falcon9H Scale Rocket\"\n"
    "/0 AxialStage \"Payload Fairing Stage\"\n"
    "/0/0 NoseCone \"PL Fairing Nose\" foreAuto=false aftAuto=false\n"
    "/0/1 BodyTube \"PL Fairing Body\" outerAuto=false motorMount=false\n"
    "/0/2 Transition \"PL Fairing Transition\" foreAuto=true aftAuto=true\n"
    "/0/3 BodyTube \"Upper Stage Body\" outerAuto=false motorMount=false\n"
    "/0/3/0 Parachute \"Parachute\" radiusAuto=false\n"
    "/0/3/1 ShockCord \"Shock Cord\" radiusAuto=false\n"
    "/0/4 BodyTube \"Interstage\" outerAuto=false motorMount=false\n"
    "/1 AxialStage \"Core Stage\"\n"
    "/1/0 BodyTube \"Core Stage Body\" outerAuto=false motorMount=true\n"
    "/1/0/0 ParallelStage \"Booster Stage\"\n"
    "/1/0/0/0 NoseCone \"Booster Nose\" foreAuto=false aftAuto=false\n"
    "/1/0/0/1 BodyTube \"Booster Body\" outerAuto=true motorMount=false\n"
    "/1/0/0/1/0 InnerTube \"Booster Motor Tubes\" outerAuto=false innerAuto=false "
    "motorMount=true\n"
    "/1/0/0/1/1 TrapezoidFinSet \"Booster Fins\"\n";

TEST(TestRocketsFixture, Falcon9HeavyFlagsAreOpenRockets)
{
    const TestFalcon9Heavy f9h;
    EXPECT_EQ(joined(flagLines(*f9h.rocket)), kFalcon9HeavyFlags);
}

/// TestRockets.addCoreFins() puts its fins on the core stage's body, after the booster set.
TEST(TestRocketsFixture, CoreFinsGoOnTheCoreBodyOfTheFalcon9Heavy)
{
    const TestFalcon9Heavy           f9h;
    const QtRocket::TrapezoidFinSet& fins = QtRocket::Test::addCoreFins(*f9h.rocket);
    EXPECT_EQ(joined(flagLines(*f9h.rocket)),
              std::string{kFalcon9HeavyFlags} + "/1/0/1 TrapezoidFinSet \"Body Tube FinSet\"\n");

    // The arguments of the Java maker: four square fins of 0.05 by 0.025 m at the BOTTOM.
    EXPECT_EQ(fins.getParent(), f9h.coreBody);
    EXPECT_EQ(fins.getFinCount(), 4);
    EXPECT_EQ(fins.getRootChord(), 0.05);
    EXPECT_EQ(fins.getTipChord(), 0.05);
    EXPECT_EQ(fins.getSweep(), 0.0);
    EXPECT_EQ(fins.getHeight(), 0.025);
    EXPECT_EQ(fins.getAxialMethod(), QtRocket::AxialMethod::BOTTOM);
    EXPECT_EQ(fins.getAxialOffset(), 0.0);
}

TEST(TestRocketsFixture, Simple2StageFlagsAreOpenRockets)
{
    const TestSimple2Stage simple;
    EXPECT_EQ(joined(flagLines(*simple.rocket)),
              "/ Rocket \"Simple 2-Stage Rocket\"\n"
              "/0 AxialStage \"Sustainer Stage\"\n"
              "/0/0 BodyTube \"Sustainer Body Tube\" outerAuto=false motorMount=false\n"
              "/1 AxialStage \"Booster Stage\"\n"
              "/1/0 BodyTube \"Booster Body Tube\" outerAuto=false motorMount=false\n");
}

TEST(TestRocketsFixture, BigBlueFlagsAreOpenRockets)
{
    const TestBigBlue blue;
    EXPECT_EQ(joined(flagLines(*blue.rocket)),
              "/ Rocket \"Rocket\"\n"
              "/0 AxialStage \"Stage1\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n"
              "/0/1/0 FreeformFinSet \"Freeform Fin Set\"\n"
              "/0/1/1 MassComponent \"Mass Component\" radiusAuto=false\n");
}

TEST(TestRocketsFixture, IsoHaisuFlagsAreOpenRockets)
{
    const TestIsoHaisu iso;
    EXPECT_EQ(joined(flagLines(*iso.rocket)),
              "/ Rocket \"Rocket\"\n"
              "/0 AxialStage \"Stage1\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n"
              "/0/1/0 LaunchLug \"Launch Lug\"\n"
              "/0/1/1 TubeCoupler \"Tube Coupler\" outerAuto=true innerAuto=false\n"
              "/0/1/2 MassComponent \"Mass Component\" radiusAuto=false\n"
              "/0/1/3 MassComponent \"Mass Component\" radiusAuto=false\n"
              "/0/1/4 MassComponent \"Mass Component\" radiusAuto=false\n"
              "/0/1/5 TrapezoidFinSet \"CONTROL\"\n"
              "/0/2 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n"
              "/0/2/0 TubeCoupler \"Tube Coupler\" outerAuto=true innerAuto=false\n"
              "/0/2/1 MassComponent \"Mass Component\" radiusAuto=false\n"
              "/0/2/2 Bulkhead \"Bulkhead\" outerAuto=true innerAuto=false\n"
              "/0/2/3 MassComponent \"Mass Component\" radiusAuto=false\n"
              "/0/3 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n"
              "/0/3/0 InnerTube \"Inner Tube\" outerAuto=false innerAuto=false motorMount=false\n"
              "/0/3/1 CenteringRing \"Centering Ring\" outerAuto=true innerAuto=true\n"
              "/0/3/2 CenteringRing \"Centering Ring\" outerAuto=true innerAuto=true\n"
              "/0/3/3 CenteringRing \"Centering Ring\" outerAuto=true innerAuto=true\n"
              "/0/3/4 TrapezoidFinSet \"Trapezoidal Fin Set\"\n");
}

TEST(TestRocketsFixture, MultiStageEventTestRocketFlagsAreOpenRockets)
{
    const TestMultiStageEventTestRocket events;
    EXPECT_EQ(joined(flagLines(*events.rocket)),
              "/ Rocket \"Rocket\"\n"
              "/0 AxialStage \"Sustainer\"\n"
              "/0/0 NoseCone \"Sustainer Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Sustainer Body Tube\" outerAuto=false motorMount=true\n"
              "/0/1/0 Parachute \"Sustainer Parachute\" radiusAuto=false\n"
              "/1 AxialStage \"Center Booster\"\n"
              "/1/0 BodyTube \"Center Booster Body Tube\" outerAuto=false motorMount=true\n"
              "/1/0/0 TrapezoidFinSet \"Center Booster Fin Set\"\n"
              "/1/0/1 ParallelStage \"Side boosters\"\n"
              "/1/0/1/0 NoseCone \"Side Booster Nose Cones\" foreAuto=false aftAuto=false\n"
              "/1/0/1/1 BodyTube \"Side Booster Body Tubes\" outerAuto=false motorMount=true\n"
              "/1/0/1/1/0 Parachute \"Side Chutes\" radiusAuto=false\n");
}

TEST(TestRocketsFixture, EndPlateRocketFlagsAreOpenRockets)
{
    const TestEndPlateRocket plates;
    EXPECT_EQ(joined(flagLines(*plates.rocket)),
              "/ Rocket \"End Plate Test\"\n"
              "/0 AxialStage \"Sustainer\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Body tube\" outerAuto=false motorMount=false\n"
              "/0/1/0 TrapezoidFinSet \"Body Tube FinSet\"\n"
              "/0/1/1 PodSet \"Pod Set\"\n"
              "/0/1/1/0 BodyTube \"Phantom\" outerAuto=false motorMount=false\n"
              "/0/1/1/0/0 TrapezoidFinSet \"End plates\"\n");
}

TEST(TestRocketsFixture, EstesAlphaIIIWithPodsFlagsAreOpenRockets)
{
    // The fin set has left the body tube, so the other children moved up by one; the pod set
    // went to the end.
    const TestEstesAlphaIIIWithPods pods;
    EXPECT_EQ(joined(flagLines(*pods.rocket)),
              "/ Rocket \"Estes Alpha III / Code Verification Rocket\"\n" +
                  std::string{kAlphaStageAndNose} +
                  "/0/1 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n" +
                  alphaInternals("/0/1", 0) +
                  "/0/1/4 PodSet \"Pod Set\"\n"
                  "/0/1/4/0 BodyTube \"Pod Body\" outerAuto=false motorMount=false\n"
                  "/0/1/4/0/0 TrapezoidFinSet \"3 Fin Set\"\n");
}

TEST(TestRocketsFixture, EstesAlphaIIIWithMotorPodsFlagsAreOpenRockets)
{
    const TestEstesAlphaIIIWithMotorPods pods;
    EXPECT_EQ(joined(flagLines(*pods.rocket)),
              "/ Rocket \"Alpha III with motor pods\"\n" + std::string{kAlphaStageAndNose} +
                  alphaBody("Body Tube") +
                  "/0/1/5 PodSet \"Motor Pods\"\n"
                  "/0/1/5/0 BodyTube \"Pod Body Tube\" outerAuto=false motorMount=false\n"
                  "/0/1/5/0/0 InnerTube \"Pod Motor Mount\" outerAuto=false innerAuto=false "
                  "motorMount=true\n");
}

TEST(TestRocketsFixture, EstesAlphaIIIWithSecondMotorFlagsAreOpenRockets)
{
    const TestEstesAlphaIIIWithSecondMotor second;
    EXPECT_EQ(joined(flagLines(*second.rocket)),
              "/ Rocket \"Alpha III with a second motor\"\n" + std::string{kAlphaStageAndNose} +
                  alphaBody("Body Tube") +
                  "/0/1/5 InnerTube \"Second Motor Mount Tube\" outerAuto=false innerAuto=false "
                  "motorMount=true\n");
}

TEST(TestRocketsFixture, EstesAlphaIIIWithInlinePodFlagsAreOpenRockets)
{
    const TestEstesAlphaIIIWithInlinePod inlinePod;
    EXPECT_EQ(joined(flagLines(*inlinePod.rocket)),
              "/ Rocket \"Estes Alpha III / Code Verification Rocket\"\n" +
                  std::string{kAlphaStageAndNose} +
                  "/0/1 BodyTube \"Front Body Tube\" outerAuto=false motorMount=false\n"
                  "/0/1/0 PodSet \"Pod Set\"\n"
                  "/0/1/0/0 BodyTube \"Middle Body Tube\" outerAuto=false motorMount=false\n"
                  "/0/1/0/1 BodyTube \"Aft body tube\" outerAuto=false motorMount=false\n"
                  "/0/1/0/1/0 TrapezoidFinSet \"3 Fin Set\"\n" +
                  alphaInternals("/0/1/0/1", 1));
}

TEST(TestRocketsFixture, ClusterPodsFlagsAreOpenRockets)
{
    // The body tubes are default ones: new BodyTube() has an automatic outer radius.
    const TestClusterPods cluster;
    EXPECT_EQ(joined(flagLines(*cluster.rocket)),
              "/ Rocket \"Rocket\"\n"
              "/0 AxialStage \"Sustainer\"\n"
              "/0/0 BodyTube \"Sustainer Body Tube\" outerAuto=true motorMount=false\n"
              "/0/0/0 InnerTube \"Inner Tube\" outerAuto=false innerAuto=false motorMount=true\n"
              "/0/0/1 ParallelStage \"Booster Set\"\n"
              "/0/0/1/0 BodyTube \"Side Booster Body Tubes\" outerAuto=true motorMount=false\n"
              "/0/0/1/0/0 InnerTube \"Inner Tube\" outerAuto=false innerAuto=false "
              "motorMount=true\n");
}

// ================================================================================= motors

// The Java program's lines for the test motors: the configuration, the path of the mount, then
// the ejection delay and the ignition ("AUTOMATIC/0.0": the event and the delay).

constexpr std::string_view kA8 =
    "MOTOR {} {} Estes|A8| SU Black Powder|SU 18.0x70.0|"
    "SINGLE|delays=[0.0, 3.0, 5.0]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 9.0, 0.0]|"
    "cg=(0.035,0.0,0.0,0.0164)(0.035,0.0,0.0,0.0145)(0.035,0.0,0.0,0.0131)|"
    "digest=digest A8 test|ejection={}|ignition={}\n";

constexpr std::string_view kA10 =
    "MOTOR {} {} Estes|A10| SU Black Powder|SU 13.0x45.0|"
    "SINGLE|delays=[0.0, 3.0]|d=0.013|l=0.045|time=[0.0, 0.2, 0.3, 1.04, 1.05]|"
    "thrust=[0.0, 10.0, 1.0, 1.0, 0.0]|"
    "cg=(0.0225,0.0,0.0,0.011)(0.0225,0.0,0.0,0.009)(0.0225,0.0,0.0,0.008)"
    "(0.0225,0.0,0.0,0.003)(0.0225,0.0,0.0,0.003)|"
    "digest=digest A10 test|ejection={}|ignition={}\n";

constexpr std::string_view kB4 =
    "MOTOR {} {} Estes|B4| SU Black Powder|SU 18.0x70.0|"
    "SINGLE|delays=[0.0, 3.0, 5.0]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 11.4, 0.0]|"
    "cg=(0.035,0.0,0.0,0.0195)(0.035,0.0,0.0,0.0155)(0.035,0.0,0.0,0.013)|"
    "digest=digest B4 test|ejection={}|ignition={}\n";

constexpr std::string_view kC6 =
    "MOTOR {} {} Estes|C6| SU Black Powder|SU 18.0x70.0|SINGLE|delays=[0.0, 3.0, 5.0, 7.0]|"
    "d=0.018|l=0.07|time=[0.0, 0.2, 0.4, 2.0, 2.1]|thrust=[0.0, 12.0, 5.0, 5.0, 0.0]|"
    "cg=(0.035,0.0,0.0,0.0227)(0.035,0.0,0.0,0.0165)(0.035,0.0,0.0,0.0165)"
    "(0.035,0.0,0.0,0.013)(0.035,0.0,0.0,0.012)|digest=digest C6 test|ejection={}|"
    "ignition={}\n";

constexpr std::string_view kD21 =
    "MOTOR {} {} AeroTech|D21|Desc|SU 18.0x70.0|SINGLE|"
    "delays=[]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 32.0, 0.0]|"
    "cg=(0.035,0.0,0.0,0.025)(0.035,0.0,0.0,0.02)(0.035,0.0,0.0,0.0154)|"
    "digest=digest D21 test|ejection={}|ignition={}\n";

/// The ignition of a motor configuration nobody touched.
constexpr std::string_view kAutomatic = "AUTOMATIC/0.0";

/// The keys of TestRockets' TEST_FCID_0 ... TEST_FCID_4.
constexpr std::array<std::string_view, 5> kFcidKeys{
    "d010716e-ce0e-469d-ae46-190f3653ebbf", "f41bee5b-ebb8-4d92-bce7-53001577a313",
    "3e8d1280-53c2-4234-89a7-de215ef5cd69", "415a5485-f2da-4c2a-8803-394220ae58b8",
    "5abc18ec-a200-46f1-90c4-60b6995fc933"};

/// The Java program's line for the motor the Estes Alpha III's mount, at @p path, holds in
/// TEST_FCID_@p n: an A8, a B4, then a C6 with an ejection delay of 3, 5 and 7 s.
[[nodiscard]] std::string alphaMotorLine(std::size_t n, std::string_view path)
{
    switch (n)
    {
        case 0:
            return std::format(kA8, kFcidKeys[0], path, "0.0", kAutomatic);
        case 1:
            return std::format(kB4, kFcidKeys[1], path, "3.0", kAutomatic);
        case 2:
            return std::format(kC6, kFcidKeys[2], path, "3.0", kAutomatic);
        case 3:
            return std::format(kC6, kFcidKeys[3], path, "5.0", kAutomatic);
        default:
            return std::format(kC6, kFcidKeys[4], path, "7.0", kAutomatic);
    }
}

/// The lines for the Alpha III's five motors with its mount at @p path, each followed by the
/// line of an A10 with @p a10Ejection in the mount at @p a10Path of the same configuration (no
/// A10 for an empty @p a10Path).
[[nodiscard]] std::string alphaMotorLines(std::string_view path, std::string_view a10Path = {},
                                          std::string_view a10Ejection = {})
{
    std::string lines;
    for (std::size_t n = 0; n < kFcidKeys.size(); n++)
    {
        lines += alphaMotorLine(n, path);
        if (!a10Path.empty())
        {
            lines += std::format(kA10, kFcidKeys.at(n), a10Path, a10Ejection, kAutomatic);
        }
    }
    return lines;
}

TEST(TestRocketsFixture, EstesAlphaIIIMotorsAreOpenRockets)
{
    const TestEstesAlphaIII alpha;
    EXPECT_EQ(joined(motorLines(*alpha.rocket)), alphaMotorLines("/0/1/2"));
}

TEST(TestRocketsFixture, BetaMotorsAreOpenRockets)
{
    // The sustainer's motors, and the booster's D21 in TEST_FCID_1 (after the B4: the mounts of
    // a configuration come in tree order).
    const TestBeta beta;
    EXPECT_EQ(joined(motorLines(*beta.rocket)),
              alphaMotorLine(0, "/0/1/2") + alphaMotorLine(1, "/0/1/2") +
                  std::format(kD21, kFcidKeys[1], "/1/0/2", "0.0", kAutomatic) +
                  alphaMotorLine(2, "/0/1/2") + alphaMotorLine(3, "/0/1/2") +
                  alphaMotorLine(4, "/0/1/2"));
}

TEST(TestRocketsFixture, Falcon9HeavyMotorsAreOpenRockets)
{
    const TestFalcon9Heavy f9h;
    EXPECT_EQ(joined(motorLines(*f9h.rocket)),
              "MOTOR 00000000-0000-0000-0000-00007cc3bbaa /1/0 AeroTech|M1350|Desc|SU 75/512|"
              "SINGLE|delays=[]|d=0.075|l=0.622|time=[0.0, 1.0, 2.0]|thrust=[0.0, 1357.0, 0.0]|"
              "cg=(0.311,0.0,0.0,4.808)(0.311,0.0,0.0,3.389)(0.311,0.0,0.0,1.97)|"
              "digest=digest M1350 test|ejection=0.0|ignition=AUTOMATIC/0.0\n"
              "MOTOR 00000000-0000-0000-0000-00007cc3bbaa /1/0/0/1/0 AeroTech|G77|Desc|SU 29/180|"
              "SINGLE|delays=[4.0, 7.0, 10.0]|d=0.029|l=0.124|time=[0.0, 1.0, 2.0]|"
              "thrust=[0.0, 20.0, 0.0]|cg=(0.062,0.0,0.0,0.123)(0.062,0.0,0.0,0.0935)"
              "(0.062,0.0,0.0,0.064)|digest=digest G77 test|ejection=0.0|"
              "ignition=AUTOMATIC/0.0\n");
}

TEST(TestRocketsFixture, RocketsWithoutMotors)
{
    EXPECT_TRUE(motorLines(*TestSimple2Stage{}.rocket).empty());
    EXPECT_TRUE(motorLines(*TestBigBlue{}.rocket).empty());
    EXPECT_TRUE(motorLines(*TestIsoHaisu{}.rocket).empty());
    EXPECT_TRUE(motorLines(*TestEndPlateRocket{}.rocket).empty());
}

TEST(TestRocketsFixture, MultiStageEventTestRocketMotorsAreOpenRockets)
{
    // The sustainer's C6 waits for the burnout of the stage below, the center booster's is lit
    // 0.01 s after launch and the side boosters' A10s at launch. The id of the configuration is
    // random: the listing names it by its index.
    const TestMultiStageEventTestRocket events;
    EXPECT_EQ(joined(motorLines(*events.rocket)),
              std::format(kC6, "<config 0>", "/0/1", "5.0", "BURNOUT/0.0") +
                  std::format(kC6, "<config 0>", "/1/0", "0.0", "LAUNCH/0.01") +
                  std::format(kA10, "<config 0>", "/1/0/1/1", "0.0", "LAUNCH/0.0"));
}

TEST(TestRocketsFixture, EstesAlphaIIIVariantsMotorsAreOpenRockets)
{
    // The fin set left the body tube of the Alpha III with pods, so its mount is child 1.
    EXPECT_EQ(joined(motorLines(*TestEstesAlphaIIIWithPods{}.rocket)), alphaMotorLines("/0/1/1"));
    // An A10 without ejection delay in the pods' mount, in every test configuration.
    EXPECT_EQ(joined(motorLines(*TestEstesAlphaIIIWithMotorPods{}.rocket)),
              alphaMotorLines("/0/1/2", "/0/1/5/0/0", "0.0"));
    // An A10 with an ejection delay of 5 s in the second mount.
    EXPECT_EQ(joined(motorLines(*TestEstesAlphaIIIWithSecondMotor{}.rocket)),
              alphaMotorLines("/0/1/2", "/0/1/5", "5.0"));
    EXPECT_EQ(joined(motorLines(*TestEstesAlphaIIIWithInlinePod{}.rocket)),
              alphaMotorLines("/0/1/0/1/2"));
}

TEST(TestRocketsFixture, ClusterPodsMotorsAreOpenRockets)
{
    // One motor configuration per mount, whatever its cluster: two lines for fourteen motors.
    const TestClusterPods cluster;
    EXPECT_EQ(joined(motorLines(*cluster.rocket)),
              std::format(kC6, "<config 0>", "/0/0/0", "0.0", "LAUNCH/0.0") +
                  std::format(kC6, "<config 0>", "/0/0/1/0/0", "0.0", "LAUNCH/0.0"));
}

// ================================================================================ separation

/// makeMultiStageEventTestRocket() is the one maker that sets a separation: its side boosters
/// leave at their BURNOUT in its configuration. Every other stage keeps the default (EJECTION);
/// a separation set answers isDefault(fcid) for the default id only, as in Java.
TEST(TestRocketsFixture, MultiStageEventTestRocketSeparationsAreOpenRockets)
{
    const TestMultiStageEventTestRocket events;
    EXPECT_EQ(joined(separationLines(*events.rocket)),
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /0 EJECTION/0.0 default=true\n"
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /1 EJECTION/0.0 default=true\n"
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /1/0/1 EJECTION/0.0 default=true\n"
              "SEPARATION <config 0> /0 EJECTION/0.0 default=false\n"
              "SEPARATION <config 0> /1 EJECTION/0.0 default=false\n"
              "SEPARATION <config 0> /1/0/1 BURNOUT/0.0 default=false\n");
}

TEST(TestRocketsFixture, Falcon9HeavySeparationsAreOpenRockets)
{
    const TestFalcon9Heavy f9h;
    EXPECT_EQ(joined(separationLines(*f9h.rocket)),
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /0 EJECTION/0.0 default=true\n"
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /1 EJECTION/0.0 default=true\n"
              "SEPARATION ffffffff-f4f2-f1f0-0000-00000000162c /1/0/0 EJECTION/0.0 default=true\n"
              "SEPARATION 00000000-0000-0000-0000-00007cc3bbaa /0 EJECTION/0.0 default=false\n"
              "SEPARATION 00000000-0000-0000-0000-00007cc3bbaa /1 EJECTION/0.0 default=false\n"
              "SEPARATION 00000000-0000-0000-0000-00007cc3bbaa /1/0/0 EJECTION/0.0 "
              "default=false\n");
}

// ======================================================================= modification ids

/// The modification ids drawn while a rocket is built, counted from an id drawn just before:
/// how many, and which of them the rocket's own ids are at the end.
struct ModIdTrace
{
    std::int64_t drawn;
    std::int64_t mod;
    std::int64_t mass;
    std::int64_t aero;
    std::int64_t tree;
    std::int64_t functional;

    [[nodiscard]] bool operator==(const ModIdTrace&) const = default;
};

/// For gtest's failure messages.
std::ostream& operator<<(std::ostream& out, const ModIdTrace& trace)
{
    return out << std::format("drawn={} mod={} mass={} aero={} tree={} functional={}", trace.drawn,
                              trace.mod, trace.mass, trace.aero, trace.tree, trace.functional);
}

/// The number of symmetric components (nose cones, body tubes, transitions) of @p rocket.
[[nodiscard]] std::int64_t symmetricCount(const Rocket& rocket)
{
    std::int64_t count = 0;
    for (const RocketComponent& component : rocket.subtree())
    {
        if (dynamic_cast<const QtRocket::SymmetricComponent*>(&component) != nullptr)
        {
            count++;
        }
    }
    return count;
}

/// What the Java program measured for a maker, as the fixture must show it. OpenRocket draws an
/// id for a new Rocket, for a new MotorConfiguration, for every event the rocket fires and for
/// every flight configuration such an event updates; QtRocket draws one more for every new
/// SymmetricComponent (its identity, SymmetricComponent.h). A fixture creates all its
/// @p symmetric components before the events that give the rocket its final ids, so every id
/// but the rocket's first one (1: the tree id of a rocket whose events were enabled last) comes
/// that many draws later.
[[nodiscard]] ModIdTrace fromJava(const ModIdTrace& java, std::int64_t symmetric)
{
    const auto shifted = [symmetric](std::int64_t id) { return id == 1 ? id : id + symmetric; };
    return {.drawn      = java.drawn + symmetric,
            .mod        = shifted(java.mod),
            .mass       = shifted(java.mass),
            .aero       = shifted(java.aero),
            .tree       = shifted(java.tree),
            .functional = shifted(java.functional)};
}

/// The modification ids a new @p Fixture draws, and where @p after (the fixture, then what the
/// test does to its rocket) leaves the rocket's ids among them.
template <class Fixture, class After>
[[nodiscard]] ModIdTrace drawnBy(std::int64_t& symmetric, const After& after)
{
    const std::int64_t before = QtRocket::ModId{}.toInt();
    const Fixture      fixture;
    after(*fixture.rocket);
    const std::int64_t end    = QtRocket::ModId{}.toInt();
    const Rocket&      rocket = *fixture.rocket;
    symmetric                 = symmetricCount(rocket);
    return {.drawn      = end - before - 1,
            .mod        = rocket.getModId().toInt() - before,
            .mass       = rocket.getMassModId().toInt() - before,
            .aero       = rocket.getAerodynamicModId().toInt() - before,
            .tree       = rocket.getTreeModId().toInt() - before,
            .functional = rocket.getFunctionalModId().toInt() - before};
}

/// Builds a @p Fixture and compares the modification ids it draws with the @p java maker's.
template <class Fixture>
void expectJavaModIds(const ModIdTrace& java)
{
    std::int64_t     symmetric = 0;
    const ModIdTrace trace     = drawnBy<Fixture>(symmetric, [](Rocket&) { });
    EXPECT_EQ(trace, fromJava(java, symmetric));
}

/// The fixtures fire the events OpenRocket's makers fire while they build their rockets: they
/// draw as many modification ids, and the rocket's ids are the same draws. makeBeta() goes on
/// building after makeEstesAlphaIII() has enabled the events, so most of its statements fire.
TEST(TestRocketsFixture, TheMakersDrawOpenRocketsModificationIds)
{
    expectJavaModIds<TestEstesAlphaIII>(
        {.drawn = 15, .mod = 9, .mass = 9, .aero = 9, .tree = 1, .functional = 9});
    expectJavaModIds<TestBeta>(
        {.drawn = 77, .mod = 77, .mass = 70, .aero = 70, .tree = 70, .functional = 70});
    expectJavaModIds<TestFalcon9Heavy>(
        {.drawn = 12, .mod = 10, .mass = 10, .aero = 10, .tree = 1, .functional = 10});
    expectJavaModIds<TestSimple2Stage>(
        {.drawn = 6, .mod = 4, .mass = 4, .aero = 4, .tree = 1, .functional = 4});
}

/// The makers that enable the events at the end: the ids come from the new rocket, its motor
/// configurations, and the events of enableEvents() and of the selection that follows it.
TEST(TestRocketsFixture, TheQuietMakersDrawOpenRocketsModificationIds)
{
    expectJavaModIds<TestBigBlue>(
        {.drawn = 4, .mod = 3, .mass = 3, .aero = 3, .tree = 1, .functional = 3});
    expectJavaModIds<TestIsoHaisu>(
        {.drawn = 7, .mod = 6, .mass = 6, .aero = 6, .tree = 1, .functional = 6});
    expectJavaModIds<TestMultiStageEventTestRocket>(
        {.drawn = 11, .mod = 11, .mass = 8, .aero = 8, .tree = 1, .functional = 8});
    expectJavaModIds<TestClusterPods>(
        {.drawn = 11, .mod = 11, .mass = 8, .aero = 8, .tree = 1, .functional = 8});
}

/// The makers whose every statement fires: makeEndPlateRocket() enables the events first, and
/// the Alpha III variants go on from the finished Alpha III.
TEST(TestRocketsFixture, TheFiringMakersDrawOpenRocketsModificationIds)
{
    expectJavaModIds<TestEndPlateRocket>(
        {.drawn = 40, .mod = 39, .mass = 39, .aero = 39, .tree = 34, .functional = 39});
    expectJavaModIds<TestEstesAlphaIIIWithPods>(
        {.drawn = 44, .mod = 38, .mass = 38, .aero = 38, .tree = 38, .functional = 38});
    expectJavaModIds<TestEstesAlphaIIIWithMotorPods>(
        {.drawn = 60, .mod = 49, .mass = 49, .aero = 41, .tree = 49, .functional = 49});
    expectJavaModIds<TestEstesAlphaIIIWithSecondMotor>(
        {.drawn = 65, .mod = 54, .mass = 47, .aero = 26, .tree = 18, .functional = 54});
    expectJavaModIds<TestEstesAlphaIIIWithInlinePod>(
        {.drawn = 66, .mod = 60, .mass = 60, .aero = 60, .tree = 60, .functional = 60});
}

/// TestRockets.addCoreFins() on the Falcon 9 Heavy fires one tree change.
TEST(TestRocketsFixture, AddCoreFinsDrawsOpenRocketsModificationIds)
{
    std::int64_t     symmetric = 0;
    const ModIdTrace trace     = drawnBy<TestFalcon9Heavy>(
        symmetric, [](Rocket& rocket) { static_cast<void>(QtRocket::Test::addCoreFins(rocket)); });
    EXPECT_EQ(
        trace,
        fromJava({.drawn = 15, .mod = 13, .mass = 13, .aero = 13, .tree = 13, .functional = 13},
                 symmetric));
}

}  // namespace
