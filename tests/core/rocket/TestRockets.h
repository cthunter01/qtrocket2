#pragma once

#include <array>
#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"

/// The test motors and rockets of OpenRocket's TestRockets (core/src/main/java/.../util/
/// TestRockets.java), built call for call as the Java makers build them, from the real
/// components and the real MotorMount API. Each rocket is a struct whose constructor is the Java
/// maker; it owns the rocket and names the components the tests reach for. The definitions are in
/// TestRockets.cpp.
///
/// Where Java asks the application preferences for a material (the default material of a new
/// component, and makeEstesAlphaIII()'s getDefaultComponentMaterial(null, BULK)), the rockets
/// have the built-in bulk default, "Cardboard": what OpenRocket's test preferences give, and so
/// what the values OpenRocket's tests pin are computed with.
///
/// Every maker of TestRockets.java that returns a Rocket is here, but for makeTestRocket(): it
/// draws the name, the dimensions, the colors and the finishes of its components from a
/// java.util.Random seeded with the hash of a key, a stream that is not reproduced here (and
/// OpenRocket does not promise the same rocket from one version to the next). Not ported either:
/// the makers that return an OpenRocketDocument (makeTestRocket_v100() ... and
/// makeTestRocket_for_estimateFileSize(), with getTestMotor(), which only they use) and
/// dumpRocket(), which wait for the document and file tiers; and splitRocketFins(), which only
/// BarrowmanCalculatorTest calls (the aero tier).
namespace QtRocket::Test
{

// ============================================================================ test motors

/// A single-use motor whose CG stays at half its @p length, built as TestRockets'
/// generateMotor_*() do: the thrust curve @p thrust over @p time, with the masses @p masses.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> makeTestMotor(
    const std::string& manufacturer, const std::string& designation, const std::string& description,
    const std::string& caseInfo, std::vector<double> delays, double diameter, double length,
    std::vector<double> time, std::vector<double> thrust, const std::vector<double>& masses);

/// TestRockets.generateMotor_M1350_75mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorM1350();

/// TestRockets.generateMotor_G77_29mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorG77();

/// TestRockets.generateMotor_A8_18mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorA8();

/// TestRockets.generateMotor_A10_13mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorA10();

/// TestRockets.generateMotor_B4_18mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorB4();

/// TestRockets.generateMotor_C6_18mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorC6();

/// TestRockets.generateMotor_D21_18mm().
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorD21();

/// Gives @p mount a motor configuration for @p fcid with @p motor and @p ejectionDelay, as
/// TestRockets does (new MotorConfiguration(mount, fcid), setMotor(), setEjectionDelay(),
/// mount.setMotorConfig()), which also makes the mount act as one. Returns the stored
/// configuration.
MotorConfiguration& addMotor(MotorMount& mount, const FlightConfigurationId& fcid,
                             std::shared_ptr<const Motor> motor, double ejectionDelay = 0.0);

// ============================================================================ test rockets

/// TestRockets' TEST_FCID_0 ... TEST_FCID_4.
[[nodiscard]] FlightConfigurationId testFcid(int n);

/// TestRockets.makeEstesAlphaIII(): a stage with an ogive nose cone (0.07 m, base radius
/// 0.012 m, an aft shoulder) and a body tube (0.2 m, wall 0.3 mm) holding three trapezoidal fins
/// (root chord 0.05 m, tip chord 0.03 m, sweep 0.02 m, height 0.05 m, 3.2 mm thick, at the
/// BOTTOM), a launch lug (0.05 m, radii 3 and 2 mm, TOP 0.111 m), a motor mount inner tube (with an
/// engine block, and a motor in each of the five test configurations), a parachute whose mass is
/// overridden and two centering rings. The default configuration stays selected; events are
/// enabled.
struct TestEstesAlphaIII
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               body{nullptr};
    TrapezoidFinSet*        fins{nullptr};
    LaunchLug*              lug{nullptr};
    InnerTube*              inner{nullptr};
    EngineBlock*            block{nullptr};
    Parachute*              chute{nullptr};
    CenteringRing*          rings{nullptr};

    TestEstesAlphaIII();

private:
    /// The fin set, the launch lug and the motor mount with its engine block and motors.
    void addFinsLugAndMotorMount();

    /// A C6 with @p ejectionDelay in the motor mount for @p fcid (the delay is set first, as in
    /// Java).
    void addC6(const FlightConfigurationId& fcid, double ejectionDelay) const;

    /// The parachute and the centering rings.
    void addRecoveryAndRings();
};

/// TestRockets.makeBeta(): the Estes Alpha III as the sustainer, plus a booster stage with a
/// body tube (0.06 m, the sustainer's radius and wall) holding a coupler, the booster fins, a
/// motor mount (a D21 in TEST_FCID_1) and a launch lug, and a tail cone (0.005 m, radii 0.012
/// and 0.01 m). TEST_FCID_1 is selected, with every stage active.
struct TestBeta : TestEstesAlphaIII
{
    AxialStage*      boosterStage{nullptr};
    BodyTube*        boosterBody{nullptr};
    TubeCoupler*     coupler{nullptr};
    TrapezoidFinSet* boosterFins{nullptr};
    InnerTube*       boosterMmt{nullptr};
    LaunchLug*       boosterLug{nullptr};
    Transition*      boosterTail{nullptr};

    TestBeta();

private:
    /// The coupler, the fins, the motor mount and the launch lug of the booster body.
    void addBoosterInternals(double sustainerThickness);
};

/// TestRockets.makeSimple2Stage(): two stages, each holding a body tube 0.1 m long with radius
/// 0.01 m and a 1 mm wall; TEST_FCID_0 selected with every stage active.
struct TestSimple2Stage
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             sustainerStage{nullptr};
    BodyTube*               sustainerBody{nullptr};
    AxialStage*             boosterStage{nullptr};
    BodyTube*               boosterBody{nullptr};

    TestSimple2Stage();
};

/// TestRockets.makeBigBlue(): one stage with an ellipsoid nose cone (0.105 m, base radius
/// 0.033 m, wall 1 mm) and a body tube (0.69 m) holding four freeform fins (five points, 3 mm
/// thick) and a mass component (0.2 m, radius 0.03 m, 0.105 kg, at the TOP). No flight
/// configuration but the default; events are enabled.
struct TestBigBlue
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               body{nullptr};
    FreeformFinSet*         fins{nullptr};
    MassComponent*          mass{nullptr};

    TestBigBlue();
};

/// TestRockets.makeIsoHaisu(): one stage with an ogive nose cone (0.53 m, radius 0.07 m) and
/// three body tubes (0.505, 0.605 and 1.065 m; wall 5 mm), every one with its mass overridden.
/// The first tube holds a launch lug, a coupler, three mass components and the two "CONTROL"
/// fins; the second a coupler, a mass component, a bulkhead and another mass component; the
/// third an inner tube, three centering rings and the three main fins. The stage is built
/// detached and added to the rocket last. No flight configuration but the default (with every
/// stage active); events are enabled.
struct TestIsoHaisu
{
    std::unique_ptr<Rocket>       rocket = std::make_unique<Rocket>();
    AxialStage*                   stage{nullptr};
    NoseCone*                     nose{nullptr};
    BodyTube*                     tube1{nullptr};
    BodyTube*                     tube2{nullptr};
    BodyTube*                     tube3{nullptr};
    LaunchLug*                    lug{nullptr};
    TubeCoupler*                  tube1Coupler{nullptr};
    std::array<MassComponent*, 3> tube1Masses{};  ///< "Parachute", "Cord" and "Payload"
    TrapezoidFinSet*              controlFins{nullptr};
    TubeCoupler*                  tube2Coupler{nullptr};
    std::array<MassComponent*, 2> tube2Masses{};  ///< "Parachute" and "Chord"
    Bulkhead*                     bulk{nullptr};
    InnerTube*                    inner{nullptr};
    std::array<CenteringRing*, 3> centers{};
    TrapezoidFinSet*              fins{nullptr};

    TestIsoHaisu();

private:
    /// What the first, the second and the third body tube hold.
    void fillTube1();
    void fillTube2();
    void fillTube3();
};

/// TestRockets.makeFalcon9Heavy(): a payload stage (a power-series nose 0.118 m, the fairing body
/// 0.132 m, a transition 0.014 m with automatic radii, the upper stage body 0.18 m with a
/// parachute and a shock cord, the interstage 0.12 m) and a core stage whose body (0.8 m, radius
/// 0.0385 m, an M1350 motor) holds a two-booster set on its surface; each booster has a nose
/// (0.08 m) and a body (0.8 m, automatic radius) holding a 4-ring cluster of inner tubes (G77
/// motors, overhang 0.01234 m) and three fins. Its configuration is selected, with every stage
/// active.
struct TestFalcon9Heavy
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    /// TestRockets.FALCON_9H_FCID_1.
    FlightConfigurationId fcid{FlightConfigurationId::fromString("test_config #1: [ M1350, G77]")};
    AxialStage*           payloadStage{nullptr};
    NoseCone*             payloadNose{nullptr};
    BodyTube*             payloadBody{nullptr};
    Transition*           payloadTransition{nullptr};
    BodyTube*             upperStageBody{nullptr};
    Parachute*            parachute{nullptr};
    ShockCord*            shockCord{nullptr};
    BodyTube*             interstage{nullptr};
    AxialStage*           coreStage{nullptr};
    BodyTube*             coreBody{nullptr};
    ParallelStage*        boosterStage{nullptr};
    NoseCone*             boosterNose{nullptr};
    BodyTube*             boosterBody{nullptr};
    InnerTube*            boosterMotorTubes{nullptr};
    TrapezoidFinSet*      boosterFins{nullptr};

    TestFalcon9Heavy();

private:
    void addPayloadStage();
    void addCoreStage();
    void addBoosterStage();
};

/// TestRockets.addCoreFins(): adds four fins (chords 0.05 m, height 0.025 m, no sweep, at the
/// BOTTOM), named "Body Tube FinSet", to the first child of the second stage of @p rocket (the
/// core body of the Falcon 9 Heavy, which the simulations need them on). Returns the fin set.
TrapezoidFinSet& addCoreFins(Rocket& rocket);

/// TestRockets.makeMultiStageEventTestRocket(): a sustainer (an ogive nose 0.1 m, a body 0.2 m of
/// radius 0.01 m with a C6 ignited at the BURNOUT of the stage below and a parachute) on a center
/// booster (a body 0.09 m with a C6 ignited at LAUNCH after 0.01 s and four fins), whose body
/// holds two side boosters turned by pi / 4 (a nose 0.05 m and a body 0.09 m of radius 0.007 m,
/// with a parachute and an A10 ignited at LAUNCH) that separate at their BURNOUT. Its one
/// configuration, whose id is random as in Java, is selected; events are enabled.
struct TestMultiStageEventTestRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    /// The id of the configuration the maker creates (Java: selFCID, a new random id).
    FlightConfigurationId fcid;
    AxialStage*           sustainer{nullptr};
    NoseCone*             sustainerNose{nullptr};
    BodyTube*             sustainerBody{nullptr};
    Parachute*            sustainerChute{nullptr};
    AxialStage*           centerBooster{nullptr};
    BodyTube*             centerBody{nullptr};
    TrapezoidFinSet*      centerFins{nullptr};
    ParallelStage*        sideBoosters{nullptr};
    NoseCone*             sideNose{nullptr};
    BodyTube*             sideBody{nullptr};
    Parachute*            sideChute{nullptr};

    TestMultiStageEventTestRocket();

private:
    void addSustainer();
    void addCenterBooster();
    void addSideBoosters();
};

/// TestRockets.makeEndPlateRocket(): "End Plate Test", a nose cone (0.05 m) and a body tube
/// (0.254 m, radius 0.01 m) with four square fins (0.05 by 0.025 m) and a set of four pods at
/// the fin tips, each a phantom body tube (0.05 m, radius 0) carrying two end plates turned by
/// pi / 2. The events are enabled from the start, so every statement of the maker fires. No
/// flight configuration but the default.
struct TestEndPlateRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             sustainer{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               body{nullptr};
    TrapezoidFinSet*        bodyFins{nullptr};
    PodSet*                 podSet{nullptr};
    BodyTube*               phantom{nullptr};
    TrapezoidFinSet*        endPlates{nullptr};

    TestEndPlateRocket();
};

/// TestRockets.makeEstesAlphaIIIWithPods(): the Alpha III with its fins moved onto "phantom"
/// pods: a pod set of three on the body tube, each a body tube ("Pod Body", as long as the fins'
/// root chord, radius 0) carrying the fin set with a single fin. The base's `fins` is that fin
/// set, in its new place.
struct TestEstesAlphaIIIWithPods : TestEstesAlphaIII
{
    PodSet*   podSet{nullptr};
    BodyTube* podBody{nullptr};

    TestEstesAlphaIIIWithPods();
};

/// TestRockets.makeEstesAlphaIIIWithMotorPods(): "Alpha III with motor pods", the Alpha III with
/// two pods at the BOTTOM of its body tube, each a body tube (0.06 m, radius 7.5 mm) with a
/// motor mount (0.045 m) that holds an A10 without ejection delay in each of the five test
/// configurations.
struct TestEstesAlphaIIIWithMotorPods : TestEstesAlphaIII
{
    PodSet*    podSet{nullptr};
    BodyTube*  podBody{nullptr};
    InnerTube* podMount{nullptr};

    TestEstesAlphaIIIWithMotorPods();
};

/// TestRockets.makeEstesAlphaIIIWithSecondMotor(): "Alpha III with a second motor", the Alpha
/// III with a second motor mount (0.045 m, TOP 0.06 m) in its body tube that holds an A10 with
/// an ejection delay of 5 s in each of the five test configurations.
struct TestEstesAlphaIIIWithSecondMotor : TestEstesAlphaIII
{
    InnerTube* secondMount{nullptr};

    TestEstesAlphaIIIWithSecondMotor();
};

/// TestRockets.makeEstesAlphaIIIwithInlinePod(): the Alpha III with its body tube cut in three:
/// a front tube (half the length) on the stage, which holds a coaxial pod set of one at its end,
/// and in the pod a middle tube (a quarter) followed by the original body tube ("Aft body
/// tube", shortened to a quarter), whose children are positioned from the BOTTOM. The base's
/// `body` is that aft tube.
struct TestEstesAlphaIIIWithInlinePod : TestEstesAlphaIII
{
    BodyTube* frontTube{nullptr};
    PodSet*   pod{nullptr};
    BodyTube* middleTube{nullptr};

    TestEstesAlphaIIIWithInlinePod();
};

/// TestRockets.makeClusterPods(): a sustainer whose default body tube holds a two-motor cluster
/// of inner tubes (C6, ignited at LAUNCH) and three side boosters, each a default body tube with
/// a four-motor cluster (C6, ignited at LAUNCH): fourteen motors. Its one configuration, whose
/// id is random as in Java, is selected; events are enabled.
struct TestClusterPods
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    /// The id of the configuration the maker creates (Java: selFCID, a new random id).
    FlightConfigurationId fcid;
    AxialStage*           sustainer{nullptr};
    BodyTube*             sustainerBody{nullptr};
    InnerTube*            sustainerMount{nullptr};
    ParallelStage*        sideBoosters{nullptr};
    BodyTube*             sideBody{nullptr};
    InnerTube*            sideMounts{nullptr};

    TestClusterPods();
};

// ============================================================================ the makers

/// One maker of TestRockets.java and the fixture that ports it.
struct TestRocketMaker
{
    /// The golden data of the rocket: tests/data/goldens/<input> ("testrocket-beta").
    std::string_view input;
    /// The Java method ("makeBeta").
    std::string_view method;
    /// Builds the fixture and hands its rocket over.
    std::unique_ptr<Rocket> (*make)();
    /// Whether the maker gives its flight configuration a new random id (the golden data then
    /// holds the id OpenRocket drew, which no other run draws).
    bool randomConfigurationId;
};

/// "makeBeta (testrocket-beta)", for the messages of the tests parameterised by a maker.
std::ostream& operator<<(std::ostream& out, const TestRocketMaker& maker);

/// The thirteen makers, in the order of the golden data (tools/openrocket-goldens:
/// GoldenDumper.TEST_ROCKETS).
[[nodiscard]] std::span<const TestRocketMaker> testRocketMakers();

}  // namespace QtRocket::Test
