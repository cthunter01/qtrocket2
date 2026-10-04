// The fixtures of tests/core/rocket/TestRockets.h against OpenRocket's TestRockets.java, for what
// the golden files do not hold: the automatic radius flags the Java makers leave, the data of the
// test motors, and the modification ids drawn while a rocket is built (the events it fires).
// The expected listings and numbers are the output of Java programs run on
// TestRockets.makeEstesAlphaIII(), makeBeta(), makeFalcon9Heavy() and makeSimple2Stage() with
// OpenRocket's compiled core (JDK 17), which print the same lines as the functions below.
// TestRocketsGolden (tests/core/goldens/test_rockets_golden_tests.cpp) compares everything else.

#include <algorithm>
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
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ThrustCurveMotor;
using QtRocket::Strings::javaDoubleToString;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestSimple2Stage;

/// The golden path of @p component: "/" for the rocket, "/0/1" for child 1 of child 0.
[[nodiscard]] std::string pathOf(const RocketComponent& component)
{
    const RocketComponent* parent = component.getParent();
    if (parent == nullptr)
    {
        return "/";
    }
    const std::string above = pathOf(*parent);
    return std::format("{}/{}", above == "/" ? "" : above,
                       parent->getChildPosition(&component).value_or(0));
}

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
        lines.push_back(std::format("{} {} \"{}\"{}", pathOf(component),
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

/// The line the Java program prints for the motor @p motorConfig holds, in the mount at @p path
/// in the configuration @p id.
[[nodiscard]] std::string motorLine(const FlightConfigurationId& id, const std::string& path,
                                    const MotorConfiguration& motorConfig)
{
    const auto* motor = dynamic_cast<const ThrustCurveMotor*>(motorConfig.getMotor().get());
    if (motor == nullptr)
    {
        return std::format("MOTOR {} {} is not a thrust curve motor", id.toString(), path);
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
        id.toString(), path, motor->getManufacturer().getDisplayName(), motor->getDesignation(),
        motor->getDescription(), motor->getCaseInfo(), QtRocket::enumName(motor->getMotorType()),
        javaArray(motor->getStandardDelays()), javaDoubleToString(motor->getDiameter()),
        javaDoubleToString(motor->getLength()), javaArray(motor->getTimePoints()),
        javaArray(motor->getThrustPoints()), cg, motor->getDigest(),
        javaDoubleToString(motorConfig.getEjectionDelay()),
        QtRocket::name(motorConfig.getIgnitionEvent()),
        javaDoubleToString(motorConfig.getIgnitionDelay()));
}

/// One line per motor of @p rocket: for every configuration (the default first, then in creation
/// order), every motor mount in tree order that holds a motor in it.
[[nodiscard]] std::vector<std::string> motorLines(const Rocket& rocket)
{
    std::vector<FlightConfigurationId> ids{FlightConfigurationId::defaultValueId()};
    std::ranges::copy(rocket.getIds(), std::back_inserter(ids));

    std::vector<std::string> lines;
    for (const FlightConfigurationId& id : ids)
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
                lines.push_back(motorLine(id, pathOf(component), motorConfig));
            }
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

TEST(TestRocketsFixture, EstesAlphaIIIFlagsAreOpenRockets)
{
    const TestEstesAlphaIII alpha;
    // HOOK(fins-lugs): the fin set and the launch lug are doubles; their lines hold the class
    // their kind names and their name only, as Java's do.
    EXPECT_EQ(joined(flagLines(*alpha.rocket)),
              "/ Rocket \"Estes Alpha III / Code Verification Rocket\"\n"
              "/0 AxialStage \"Stage\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Body Tube\" outerAuto=false motorMount=false\n"
              "/0/1/0 TrapezoidFinSet \"3 Fin Set\"\n"
              "/0/1/1 LaunchLug \"Launch Lugs\"\n"
              "/0/1/2 InnerTube \"Motor Mount Tube\" outerAuto=false innerAuto=false "
              "motorMount=true\n"
              "/0/1/2/0 EngineBlock \"Engine Block\" outerAuto=false innerAuto=false\n"
              "/0/1/3 Parachute \"Parachute\" radiusAuto=false\n"
              "/0/1/4 CenteringRing \"Centering Rings\" outerAuto=true innerAuto=true\n");
}

TEST(TestRocketsFixture, BetaFlagsAreOpenRockets)
{
    const TestBeta beta;
    // HOOK(fins-lugs): the fin sets and the launch lugs are doubles (see EstesAlphaIII).
    EXPECT_EQ(joined(flagLines(*beta.rocket)),
              "/ Rocket \"Kit-bash Beta\"\n"
              "/0 AxialStage \"Sustainer Stage\"\n"
              "/0/0 NoseCone \"Nose Cone\" foreAuto=false aftAuto=false\n"
              "/0/1 BodyTube \"Sustainer Body Tube\" outerAuto=false motorMount=false\n"
              "/0/1/0 TrapezoidFinSet \"3 Fin Set\"\n"
              "/0/1/1 LaunchLug \"Launch Lugs\"\n"
              "/0/1/2 InnerTube \"Motor Mount Tube\" outerAuto=false innerAuto=false "
              "motorMount=true\n"
              "/0/1/2/0 EngineBlock \"Engine Block\" outerAuto=false innerAuto=false\n"
              "/0/1/3 Parachute \"Parachute\" radiusAuto=false\n"
              "/0/1/4 CenteringRing \"Centering Rings\" outerAuto=true innerAuto=true\n"
              "/1 AxialStage \"Booster Stage\"\n"
              "/1/0 BodyTube \"Booster Body\" outerAuto=false motorMount=false\n"
              "/1/0/0 TubeCoupler \"Coupler\" outerAuto=true innerAuto=false\n"
              "/1/0/1 TrapezoidFinSet \"Booster Fins\"\n"
              "/1/0/2 InnerTube \"Booster MMT\" outerAuto=false innerAuto=false motorMount=true\n"
              "/1/0/3 LaunchLug \"Launch Lugs\"\n"
              "/1/1 Transition \"Booster Tail Cone\" foreAuto=false aftAuto=false\n");
}

TEST(TestRocketsFixture, Falcon9HeavyFlagsAreOpenRockets)
{
    const TestFalcon9Heavy f9h;
    // HOOK(fins-lugs): the booster fins are a double (see EstesAlphaIII).
    EXPECT_EQ(joined(flagLines(*f9h.rocket)),
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
              "/1/0/0/1/1 TrapezoidFinSet \"Booster Fins\"\n");
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

// ================================================================================= motors

/// The Java program's line for the A8 of the Alpha III's motor mount (TEST_FCID_0).
constexpr std::string_view kA8Line =
    "MOTOR d010716e-ce0e-469d-ae46-190f3653ebbf /0/1/2 Estes|A8| SU Black Powder|SU 18.0x70.0|"
    "SINGLE|delays=[0.0, 3.0, 5.0]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 9.0, 0.0]|"
    "cg=(0.035,0.0,0.0,0.0164)(0.035,0.0,0.0,0.0145)(0.035,0.0,0.0,0.0131)|"
    "digest=digest A8 test|ejection=0.0|ignition=AUTOMATIC/0.0\n";

/// The line for its B4 (TEST_FCID_1).
constexpr std::string_view kB4Line =
    "MOTOR f41bee5b-ebb8-4d92-bce7-53001577a313 /0/1/2 Estes|B4| SU Black Powder|SU 18.0x70.0|"
    "SINGLE|delays=[0.0, 3.0, 5.0]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 11.4, 0.0]|"
    "cg=(0.035,0.0,0.0,0.0195)(0.035,0.0,0.0,0.0155)(0.035,0.0,0.0,0.013)|"
    "digest=digest B4 test|ejection=3.0|ignition=AUTOMATIC/0.0\n";

/// The line for the D21 of the Beta's booster motor mount (TEST_FCID_1).
constexpr std::string_view kD21Line =
    "MOTOR f41bee5b-ebb8-4d92-bce7-53001577a313 /1/0/2 AeroTech|D21|Desc|SU 18.0x70.0|SINGLE|"
    "delays=[]|d=0.018|l=0.07|time=[0.0, 1.0, 2.0]|thrust=[0.0, 32.0, 0.0]|"
    "cg=(0.035,0.0,0.0,0.025)(0.035,0.0,0.0,0.02)(0.035,0.0,0.0,0.0154)|"
    "digest=digest D21 test|ejection=0.0|ignition=AUTOMATIC/0.0\n";

/// The lines for the C6s of the Alpha III's motor mount in TEST_FCID_2, 3 and 4, which differ
/// in the ejection delay: 3, 5 and 7 s.
[[nodiscard]] std::string c6Lines()
{
    constexpr std::string_view kC6 =
        "MOTOR {} /0/1/2 Estes|C6| SU Black Powder|SU 18.0x70.0|SINGLE|delays=[0.0, 3.0, 5.0, 7.0]|"
        "d=0.018|l=0.07|time=[0.0, 0.2, 0.4, 2.0, 2.1]|thrust=[0.0, 12.0, 5.0, 5.0, 0.0]|"
        "cg=(0.035,0.0,0.0,0.0227)(0.035,0.0,0.0,0.0165)(0.035,0.0,0.0,0.0165)"
        "(0.035,0.0,0.0,0.013)(0.035,0.0,0.0,0.012)|digest=digest C6 test|ejection={}|"
        "ignition=AUTOMATIC/0.0\n";
    return std::format(kC6, "3e8d1280-53c2-4234-89a7-de215ef5cd69", "3.0") +
           std::format(kC6, "415a5485-f2da-4c2a-8803-394220ae58b8", "5.0") +
           std::format(kC6, "5abc18ec-a200-46f1-90c4-60b6995fc933", "7.0");
}

TEST(TestRocketsFixture, EstesAlphaIIIMotorsAreOpenRockets)
{
    const TestEstesAlphaIII alpha;
    EXPECT_EQ(joined(motorLines(*alpha.rocket)),
              std::string{kA8Line} + std::string{kB4Line} + c6Lines());
}

TEST(TestRocketsFixture, BetaMotorsAreOpenRockets)
{
    // The sustainer's motors, and the booster's D21 in TEST_FCID_1 (after the B4: the mounts of
    // a configuration come in tree order).
    const TestBeta beta;
    EXPECT_EQ(joined(motorLines(*beta.rocket)),
              std::string{kA8Line} + std::string{kB4Line} + std::string{kD21Line} + c6Lines());
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

TEST(TestRocketsFixture, Simple2StageHasNoMotors)
{
    const TestSimple2Stage simple;
    EXPECT_TRUE(motorLines(*simple.rocket).empty());
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

/// Builds a @p Fixture and compares the modification ids it draws with the @p java maker's.
template <class Fixture>
void expectJavaModIds(const ModIdTrace& java)
{
    const std::int64_t before = QtRocket::ModId{}.toInt();
    const Fixture      fixture;
    const std::int64_t after  = QtRocket::ModId{}.toInt();
    const Rocket&      rocket = *fixture.rocket;
    const ModIdTrace   trace{.drawn      = after - before - 1,
                             .mod        = rocket.getModId().toInt() - before,
                             .mass       = rocket.getMassModId().toInt() - before,
                             .aero       = rocket.getAerodynamicModId().toInt() - before,
                             .tree       = rocket.getTreeModId().toInt() - before,
                             .functional = rocket.getFunctionalModId().toInt() - before};
    EXPECT_EQ(trace, fromJava(java, symmetricCount(rocket)));
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

}  // namespace
