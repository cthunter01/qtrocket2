#include "QtRocket/aero/barrowman/SymmetricComponentCalc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::SymmetricComponentCalc;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::TestEstesAlphaIII;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ============================================================ SymmetricComponentCalcTest.java

/// SymmetricComponentCalcTest.EPSILON.
constexpr double kEpsilon = MathUtil::kEpsilon * 1000;

/// The nose cone of the Estes Alpha III: Java's (NoseCone) rocket.getChild(0).getChild(0).
[[nodiscard]] NoseCone& noseOf(const TestEstesAlphaIII& alpha)
{
    EXPECT_EQ(&alpha.rocket->getChild(0).getChild(0), alpha.nose);
    return *alpha.nose;
}

// SymmetricComponentCalcTest.testConicalNoseParams
TEST(SymmetricComponentCalc, ConicalNoseParams)
{
    const TestEstesAlphaIII alpha;
    NoseCone&               nose = noseOf(alpha);
    nose.setShapeType(TransitionShape::CONICAL);

    // to illustrate the NoseCone properties to the reader:
    EXPECT_NEAR(nose.getLength(), 0.07, kEpsilon);
    EXPECT_NEAR(nose.getAftRadius(), 0.012, kEpsilon);
    EXPECT_EQ(nose.getShapeType(), TransitionShape::CONICAL);

    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    const Transformation&      transform = Transformation::kIdentity;
    WarningSet                 warnings;
    AerodynamicForces          forces;
    SymmetricComponentCalc     calcObj{nose};

    conditions.setAOA(0.0);
    calcObj.calculateNonaxialForces(conditions, transform, forces, warnings);

    const double cnaNose = 2;
    const double cpxNose = 2.0 / 3.0 * nose.getLength();

    EXPECT_NEAR(forces.getCP().weight, cnaNose, kEpsilon);
    EXPECT_NEAR(forces.getCP().x, cpxNose, kEpsilon);
    EXPECT_NEAR(forces.getCN(), 0.0, kEpsilon);
    EXPECT_NEAR(forces.getCm(), 0.0, kEpsilon);
}

// SymmetricComponentCalcTest.testOgiveNoseParams
TEST(SymmetricComponentCalc, OgiveNoseParams)
{
    const TestEstesAlphaIII alpha;
    const NoseCone&         nose = noseOf(alpha);

    // to illustrate the NoseCone properties to the reader:
    EXPECT_NEAR(nose.getLength(), 0.07, kEpsilon);
    EXPECT_NEAR(nose.getAftRadius(), 0.012, kEpsilon);
    EXPECT_EQ(nose.getShapeType(), TransitionShape::OGIVE);

    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    const Transformation&      transform = Transformation::kIdentity;
    WarningSet                 warnings;
    AerodynamicForces          forces;
    SymmetricComponentCalc     calcObj{nose};

    conditions.setAOA(0.0);
    calcObj.calculateNonaxialForces(conditions, transform, forces, warnings);

    const double lNose   = nose.getLength();
    const double cnaNose = 2;
    const double cpxNose = 0.46216 * lNose;
    EXPECT_NEAR(forces.getCP().weight, cnaNose, kEpsilon);
    EXPECT_NEAR(forces.getCP().x, cpxNose, kEpsilon);
    EXPECT_NEAR(forces.getCN(), 0.0, kEpsilon);
    EXPECT_NEAR(forces.getCm(), 0.0, kEpsilon);
}

// these values from a reimplementation of the pressure cd calculation in python
// values at M = 0, 0.05, ... , 1.15
constexpr std::array<double, 24> kEllipseCd{
    8.000392024269301e-07, 2.422001414621988e-06, 2.0098855921838474e-05, 8.295843903984836e-05,
    0.000230425812213129,  0.0005104254351619708, 0.0009783566607446353,  0.0016963974152150677,
    0.0027329880483111142, 0.004162427611715722,  0.006064546184601524,   0.008524431611715306,
    0.011632196831399358,  0.01548277847481293,   0.020175760185344116,   0.02581521589534269,
    0.032509569500239276,  0.04037146820686186,   0.04951766743137877,    0.06006892556095394,
    0.07214990722137526,   0.08588909394291767,   0.10141870131021756,    0.11887460183385967};

/// The loop of testEllipseNoseconeDrag.
void expectEllipsePressureDrag(SymmetricComponentCalc& calcObj, FlightConditions& conditions,
                               double frontalArea)
{
    WarningSet warnings;
    for (std::size_t i = 0; i < kEllipseCd.size(); i++)
    {
        const double m = static_cast<double>(i) / 20.0;
        conditions.setMach(m);
        const double testcd = calcObj.calculatePressureCD(conditions, 0.0, 0.0, warnings) *
                              conditions.getRefArea() / frontalArea;
        const auto   index  = static_cast<std::size_t>(MathUtil::javaRound(m * 20));
        EXPECT_NEAR(testcd, kEllipseCd.at(index), kEpsilon)
            << "SymmetricComponentCalc produces bad Cd at index " << i << "(m=" << m << ")";
    }
}

// SymmetricComponentCalcTest.testEllipseNoseconeDrag
TEST(SymmetricComponentCalc, EllipseNoseconeDrag)
{
    const TestEstesAlphaIII alpha;
    NoseCone&               nose = noseOf(alpha);
    // use an ellipsoidal nose cone with fineness ratio 5
    nose.setShapeType(TransitionShape::ELLIPSOID);
    nose.setLength(nose.getAftRadius() * 5.0);
    SymmetricComponentCalc calcObj{nose};

    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    conditions.setAOA(0.0);

    const double frontalArea = std::numbers::pi * nose.getAftRadius() * nose.getAftRadius();
    expectEllipsePressureDrag(calcObj, conditions, frontalArea);
}

// ============================================================ pins against OpenRocket

// Every value below was printed by OpenRocket's own SymmetricComponentCalc (JDK 17, the probe
// BodyProbe.java of the port's scratch directory) for a component built the same way, in fresh
// flight conditions with the reference length kRefLength.

enum class Body
{
    TUBE,        ///< BodyTube(length, aftRadius)
    NOSE,        ///< NoseCone(shape, length, aftRadius), then the shape parameter
    TRANSITION,  ///< Transition(): shape, parameter, clipping, length, fore and aft radius
};

constexpr double kRefLength = 0.05;
constexpr double kCf        = 0.005;

/// The Mach numbers of the pressure drag: the usual ones, and both sides of every limit of the
/// calculation (0.05, 1, 1.1, 1.3, 4).
constexpr std::array<double, 24> kMachs{0.0,   0.02, 0.05,  0.3,  0.6, 0.85,      0.9,  0.95,
                                        0.999, 1.0,  1.001, 1.05, 1.1, 1.1000001, 1.25, 1.3,
                                        1.31,  1.5,  2.0,   2.5,  3.0, 3.99,      4.0,  5.0};

/// The angles of attack of the forces, in degrees.
constexpr std::array<double, 3> kAoas{0, 2, 10};

/// {Mach, angle of attack in degrees} around the limits of the low-speed body lift multiplier
/// (below Mach 0.05, beyond 45 degrees).
constexpr std::array<std::array<double, 2>, 4> kSlow{
    {{0.02, 50}, {0.04, 90}, {0.02, 40}, {0.05, 50}}};

struct BodyCase
{
    std::string_view name;
    Body             body;
    TransitionShape  shape;
    double           param;
    bool             clipped;
    double           length;
    double           foreRadius;
    double           aftRadius;
    /// calculateFrictionCD() for the coefficient of friction kCf.
    double frictionCD;
    /// {CP x, CNa, CN, Cm} at each of kAoas, the same at every Mach number of kMachs.
    std::array<std::array<double, 4>, 3> forces;
    /// {CP x, CNa, CN, Cm} at each of kSlow.
    std::array<std::array<double, 4>, 4> slowForces;
    /// calculatePressureCD() at each of kMachs, with the stagnation and base drag coefficients
    /// of that Mach number.
    std::array<double, 24> pressureCD;
};

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<BodyCase, 67> kBodyCases{{
    // PINS-BEGIN (BodyProbe.java)
    {.name       = "conical nose",
     .body       = Body::NOSE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0304138126514911,
     .forces     = {{{0.09999999999999999, 2.0, 0.0, 0.0},
                     {0.1, 2.072897421541491, 0.07235777012399969, 0.14471554024799937},
                     {0.09999999999999999, 2.3528201937243383, 0.4106445908790053,
                      0.8212891817580105}}},
     .slowForces =
         {{{0.09999999999999998, 1.9816789755723923, 1.7293411420643263, 3.4586822841286518},
           {0.09999999999999999, 2.129200904145632, 3.3445409592403315, 6.689081918480662},
           {0.09999999999999999, 3.084795987362331, 2.153593869271283, 4.307187738542566},
           {0.09999999999999998, 3.168359760192403, 2.76491548515277, 5.529830970305539}}},
     .pressureCD = {0.02162162162162165,  0.021621621621657892, 0.02162162162171226,
                    0.021623410307552576, 0.022809492388699726, 0.05273472237331517,
                    0.07479214050502724,  0.10989145892259161,  0.16330883673769891,
                    0.16439898730535418,  0.16573756252790964,  0.21741038767421017,
                    0.2342743423783169,   0.23427433496254324,  0.17362975279970894,
                    0.15570850721585577,  0.15393282814823317,  0.13027821896613764,
                    0.10421465654438182,  0.09263155692346563,  0.08581866644271167,
                    0.07809439455909943,  0.07809439455909943,  0.07809439455909943}},
    {.name       = "conical nose, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.007071067811865474,
     .forces =
         {{{0.01666666666666667, 2.0, 0.0, 0.0},
           {0.016666666666666666, 2.0118111270769474, 0.07022545619150156, 0.02340848539716718},
           {0.01666666666666667, 2.050354649029554, 0.35785439459138474, 0.11928479819712827}}},
     .slowForces =
         {{{0.01666666666666667, 1.7933169462676286, 1.5649642622090179, 0.5216547540696728},
           {0.01666666666666667, 1.4158997713035744, 2.2240901598733887, 0.741363386624463},
           {0.01666666666666667, 2.0486750461579373, 1.4302449943561124, 0.47674833145203754},
           {0.01666666666666667, 1.9910970770376304, 1.7375599860570916, 0.5791866620190306}}},
     .pressureCD = {0.4000000000000003, 0.4000000672838841,  0.4000001682097098,
                    0.400935066308348,  0.42627863636021784, 0.54047706590991,
                    0.5849580939534694, 0.6399276459665852,  0.7057631984820912,
                    0.7071067811864893, 0.7085847872142177,  0.8083145563137002,
                    0.9720533231126609, 0.9720536991400125,  1.452079216872718,
                    1.4756075715960861, 1.46797008714136,    1.3662277660168387,
                    1.2541241452319323, 1.2043033499620925,  1.1750000000000007,
                    1.1417766504030484, 1.1417766504030484,  1.1417766504030484}},
    {.name       = "conical nose, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.080156097709407,
     .forces = {{{0.2666666666666667, 2.0, 0.0, 0.0},
                 {0.26666666666666666, 2.195070010470578, 0.07662239798899598, 0.40865278927464516},
                 {0.2666666666666667, 2.957751283113907, 0.5162249834542464, 2.7531999117559813}}},
     .slowForces =
         {{{0.2666666666666667, 2.358403034181919, 2.058094901774943, 10.976506142799698},
           {0.26666666666666666, 3.5558031698297476, 5.5854425579742175, 29.78902697586249},
           {0.2666666666666667, 5.157037869771119, 3.6002916191016228, 19.201555301875324},
           {0.2666666666666667, 5.522885126501947, 4.819626483344126, 25.704674577835345}}},
     .pressureCD = {0.0031128404669260763, 0.0031128404669260763, 0.0031128404669260763,
                    0.0031128404669522537, 0.0031131799619299604, 0.0043865543217487075,
                    0.008028905256389747,  0.020750552894862426,  0.06154573148996694,
                    0.062378286155173164,  0.06377864374229,      0.11686823217208264,
                    0.13139445341417755,   0.13139443796787512,   0.06089110698135524,
                    0.04571669666245985,   0.045042946666712845,  0.0360676238582645,
                    0.02617826637732146,   0.021783264227001477,  0.019198233510461734,
                    0.016267395250482507,  0.016267395250482507,  0.016267395250482507}},
    {.name       = "ogive nose, parameter 0",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0304138126514911,
     .forces     = {{{0.09999999999999999, 2.0, 0.0, 0.0},
                     {0.1, 2.072897421541491, 0.07235777012399969, 0.14471554024799937},
                     {0.09999999999999999, 2.3528201937243383, 0.4106445908790053,
                      0.8212891817580105}}},
     .slowForces =
         {{{0.09999999999999998, 1.9816789755723923, 1.7293411420643263, 3.4586822841286518},
           {0.09999999999999999, 2.129200904145632, 3.3445409592403315, 6.689081918480662},
           {0.09999999999999999, 3.084795987362331, 2.153593869271283, 4.307187738542566},
           {0.09999999999999998, 3.168359760192403, 2.76491548515277, 5.529830970305539}}},
     .pressureCD = {0.02162162162162165,  0.021621621621657892, 0.02162162162171226,
                    0.021623410307552576, 0.022809492388699726, 0.05273472237331517,
                    0.07479214050502724,  0.10989145892259161,  0.16330883673769891,
                    0.16439898730535418,  0.16573756252790964,  0.21741038767421017,
                    0.2342743423783169,   0.23427433496254324,  0.17362975279970894,
                    0.15570850721585577,  0.15393282814823317,  0.13027821896613764,
                    0.10421465654438182,  0.09263155692346563,  0.08581866644271167,
                    0.07809439455909943,  0.07809439455909943,  0.07809439455909943}},
    {.name       = "ogive nose, parameter 0.5",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03380749353944451,
     .forces = {{{0.09087080903889684, 2.0, 0.0, 0.0},
                 {0.09112799688178609, 2.0812628348513393, 0.07264977813509371, 0.1324085751071454},
                 {0.091977793723416, 2.3942410937075405, 0.4178739017174667, 0.7687023906913631}}},
     .slowForces =
         {{{0.09169300814781862, 2.007474061701421, 1.7518516012536798, 3.212650862950455},
           {0.0936775932757267, 2.2268833522776004, 3.4979801899583602, 6.553647310429369},
           {0.09368457020547113, 3.226686733693243, 2.2526522973347416, 4.220775245963448},
           {0.0939690674085597, 3.329579048498834, 2.9056058550862307, 5.460741449186075}}},
     .pressureCD = {0.009676084429113815, 0.009676084429113815, 0.009676084429113815,
                    0.00967608859510644,  0.009741315140895716, 0.01803802718843773,
                    0.028220535657312844, 0.049068602752901064, 0.08935949210605874,
                    0.09018175515408237,  0.09130356975234151,  0.13395750656598074,
                    0.14594724504319073,  0.1459472333811343,   0.08894241916142229,
                    0.07510817016091625,  0.07413411394844294,  0.06115827870461978,
                    0.046861002040767555, 0.04050704408116933,  0.036769804385855934,
                    0.03253262259313846,  0.03253262259313846,  0.03253262259313846}},
    {.name       = "ogive nose, tangent",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.04066168651958334,
     .forces =
         {{{0.0693700470236148, 2.0, 0.0, 0.0},
           {0.07050240819009397, 2.0978712596205464, 0.07322952152667861, 0.10325715236478337},
           {0.07411969759094317, 2.476476827408107, 0.43222674487614815, 0.6407303124187569}}},
     .slowForces =
         {{{0.07292818757382415, 2.0586868029636887, 1.7965431489536061, 2.620372715027145},
           {0.08082862221764552, 2.4208189692018482, 3.802613544657671, 6.147200272816736},
           {0.08085481819580165, 3.508392095831087, 2.4493197409278755, 3.960786047122227},
           {0.08191419435573628, 3.649658681388006, 3.18492802821077, 5.217816270237778}}},
     .pressureCD = {1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    0.001451851851866176,
                    0.057037037037046545,
                    0.07407407407409394,
                    0.07407406370372359,
                    0.011851851851865639,
                    2.84446945164607E-14,
                    1.421085471520216E-14,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0}},
    {.name       = "ogive nose, tangent, shorter than its radius",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.02,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.008696532068003094,
     .forces =
         {{{0.006667636275116946, 2.0, 0.0, 0.0},
           {0.006704557532286541, 2.0149425610106726, 0.07033476385640591, 0.00943126941590123},
           {0.006845944114936175, 2.06585977794173, 0.3605605500960211, 0.04936754752016011}}},
     .slowForces =
         {{{0.006794869209544522, 1.802972847899256, 1.5733906259950472, 0.21381967038319452},
           {0.007265720715649669, 1.4524653445131692, 2.28152722795817, 0.3315387928698888},
           {0.007268003082659453, 2.1017891579758317, 1.4673256395759, 0.21329054543405787},
           {0.00736652584631221, 2.051446462235301, 1.7902247597197758, 0.2637547392636759}}},
     .pressureCD = {1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    1.4210854715202004E-14,
                    0.001451851851866176,
                    0.057037037037046545,
                    0.07407407407409394,
                    0.07407406370372359,
                    0.011851851851865639,
                    2.84446945164607E-14,
                    1.421085471520216E-14,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0}},
    {.name       = "ogive nose, parameter 0.5, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.08906719723691954,
     .forces = {{{0.24287409485900577, 2.0, 0.0, 0.0},
                 {0.24454989756914233, 2.2168712006811395, 0.07738340308905169,
                  0.37848206597958484},
                 {0.2488742987105768, 3.0656987192750513, 0.53506536525523, 2.6632803508442797}}},
     .slowForces =
         {{{0.24759677741229597, 2.4256278751272826, 2.1167596424562474, 10.482057320571414},
           {0.25425887978556083, 3.8103744089401035, 5.985322125276389, 30.436425974570138},
           {0.2542754017457272, 5.526820812382812, 3.8584488137531148, 19.622172444647955},
           {0.2549212434032746, 5.9430403824104685, 5.186281112602279, 26.441864597269824}}},
     .pressureCD = {0.0013967479186891083, 0.0013967479186891083, 0.0013967479186891083,
                    0.0013967479186891083, 0.0013967483951629676, 0.0015021549879047666,
                    0.0021910325742308025, 0.006762711061657187,  0.033713186666252046,
                    0.03426319637246827,   0.03542448612566835,   0.07952144363134744,
                    0.09184155889709791,   0.09184154691817197,   0.03528610881921375,
                    0.023629517791586094,  0.023259439839238262,  0.01832946713803012,
                    0.012897432719448551,  0.010483342320119434,  0.009063434520002633,
                    0.007453581267738089,  0.007453581267738089,  0.007453581267738089}},
    {.name       = "ellipsoid nose",
     .body       = Body::NOSE,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.047690533190298534,
     .forces = {{{0.05000727206337708, 2.0, 0.0, 0.0},
                 {0.05198611375007158, 2.114709064383792, 0.07381727179053184, 0.07674946176045076},
                 {0.058101577198371335, 2.5598483239740926, 0.4467778160500641,
                  0.519169915395051}}},
     .slowForces =
         {{{0.0561209691101176, 2.110606846601026, 1.8418519344161397, 2.0673303103355702},
           {0.06867602160263514, 2.6174330430702124, 4.111454209686278, 5.6471663624532},
           {0.0687154883547891, 3.793988108357857, 2.64870337089424, 3.6401389127594754},
           {0.07030026202970925, 3.9741589541213633, 3.468107937351603, 4.876177934862643}}},
     .pressureCD = {5.555829492279099E-7,
                    7.226755188870925E-7,
                    9.733143733758666E-7,
                    4.758084715268784E-4,
                    0.007230877408507208,
                    0.028394782239757044,
                    0.03554072719298108,
                    0.04394830642139487,
                    0.053559860224881545,
                    0.05375601438413638,
                    0.05398307958443782,
                    0.06510927439921024,
                    0.07816041282680349,
                    0.07816044264324698,
                    0.128,
                    0.14,
                    0.1408,
                    0.15,
                    0.159,
                    0.162,
                    0.162,
                    0.162,
                    0.162,
                    0.162}},
    {.name       = "ellipsoid nose, hemisphere",
     .body       = Body::NOSE,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.00999963623816414,
     .forces =
         {{{0.008334545343896192, 2.0, 0.0, 0.0},
           {0.008392125155613708, 2.018779734217331, 0.0704687064692569, 0.011827644084884186},
           {0.008610612375558026, 2.084859337404513, 0.3638765987865612, 0.06266400689375053}}},
     .slowForces =
         {{{0.008532050667850153, 1.8148049247724007, 1.58371606093432, 0.27024691350759356},
           {0.009241086792923172, 1.497271794457671, 2.3519090349477128, 0.4346839104202398},
           {0.009244442362012873, 2.166873732990525, 1.5127632446266053, 0.2796930524464446},
           {0.009388565028938981, 2.125396942692457, 1.8547587280902302, 0.34827045863334555}}},
     .pressureCD = {2.0000500024970192E-5, 0.0025831574065706144, 0.006427892766389081,
                    0.0823993570196644,    0.2212673513595071,    0.36350016559321396,
                    0.3943518947921158,    0.42594157395738624,   0.4576005081895443,
                    0.4582466088881597,    0.4589066008293683,    0.4912462059485921,
                    0.5249211486523854,    0.5249212173173099,    0.629515602395976,
                    0.6541701703516669,    0.6566717857517012,    0.6931389378144239,
                    0.7453965750652944,    0.7661109449693221,    0.7661109449693221,
                    0.7661109449693221,    0.7661109449693221,    0.7661109449693221}},
    {.name       = "ellipsoid nose, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.12586828253901217,
     .forces = {{{0.13335272550233906, 2.0, 0.0, 0.0},
                 {0.14625400937618235, 2.3065677247167136, 0.08051440243308168, 0.2355110833673129},
                 {0.17533303600114494, 3.5098262971132517, 0.6125802505770699, 2.148111102560396}}},
     .slowForces =
         {{{0.16730974106126154, 2.7022106902582754, 2.3581236813797783, 7.89074125044159},
           {0.2048834491676571, 4.857755540295294, 7.630544559163406, 31.26744576617796},
           {0.20496484937015488, 7.04821685909252, 4.920583623429508, 20.170933623789598},
           {0.20810735457780805, 7.6716829769791755, 6.694806355874348, 27.864768802634114}}},
     .pressureCD = {7.812889886387838E-8, 7.816103217327309E-8, 7.820923213736515E-8,
                    5.260210601665546E-6, 3.760149967286758E-4, 0.003236554274026706,
                    0.004607971119337047, 0.006436354591324024, 0.008789384839784451,
                    0.008837405865263236, 0.008899615393725212, 0.011947882288362584,
                    0.015928086811414197, 0.015928096884258208, 0.034296222652370256,
                    0.03913333356637611,  0.03941912467306934,  0.042317655607759685,
                    0.044323663471861716, 0.044835604743956495, 0.044835604743956495,
                    0.044835604743956495, 0.044835604743956495, 0.044835604743956495}},
    {.name       = "power nose, parameter 0",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.064536740149888,
     .forces =
         {{{7.812500000001722E-4, 2.0, 0.0, 0.0},
           {0.0058525943481320475, 2.145628290888338, 0.0748965563998803, 0.00876678325360985},
           {0.02064078194956403, 2.712943232876567, 0.47349791833395366, 0.19546734571807226}}},
     .slowForces =
         {{{0.015991347256644892, 2.2059475167133766, 1.9250523646420532, 0.6156836170043292},
           {0.04344045435216758, 2.9784750654357093, 4.678577692236601, 4.064790813453472},
           {0.04351968610144912, 4.318427482983815, 3.014831123467159, 2.624090082843399},
           {0.046668006340075674, 4.570038142323558, 3.9881106262635653, 3.7223434398278243}}},
     .pressureCD = {0.85,
                    0.850212553125,
                    0.8505313828125001,
                    0.869297125,
                    0.9292539999999999,
                    1.0146238828125,
                    1.0360671250000002,
                    1.0590895078125,
                    1.0882547901562503,
                    1.08885,
                    1.0893998436831835,
                    1.1163421841591736,
                    1.14328200045045,
                    1.143282052021089,
                    1.2161533439999999,
                    1.2373180036748916,
                    1.241241827313331,
                    1.307372290809328,
                    1.41178359375,
                    1.464374016,
                    1.4940050068587105,
                    1.4940050068587105,
                    1.4940050068587105,
                    1.4940050068587105}},
    {.name       = "power nose, parameter 0.1",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.1,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.056492452725636,
     .forces = {{{0.02523002007458503, 2.0, 0.0, 0.0},
                 {0.0285670889099438, 2.1325789612407524, 0.07444104886482333, 0.04253128122941758},
                 {0.03853549207563783, 2.648330169760588, 0.46222081142222626,
                  0.35623812831512175}}},
     .slowForces =
         {{{0.03536259178056434, 2.1657093874401605, 1.8899379726090164, 1.3366621003192007},
           {0.05463440354231855, 2.8260987967154794, 4.439225609040152, 4.85068886679389},
           {0.054691967241248134, 4.097090048211261, 2.860308443679189, 3.1287179140313524},
           {0.05698866375157149, 4.318549834365954, 3.768645676057083, 4.295401624632618}}},
     .pressureCD = {0.5660000000000001, 0.5661275318750001, 0.5663188296875,
                    0.577578275,        0.6135524,          0.6647743296875,
                    0.677640275,        0.6914537046875,    0.7089528740937502,
                    0.7093100000000001, 0.7096399062099102, 0.7258053104955042,
                    0.7419692002702701, 0.7419692312126535, 0.7888920063999999,
                    0.8047908022049348, 0.8076650963879987, 0.8566233744855968,
                    0.93367015625,      0.9736244096,       0.9960030041152262,
                    0.9972030041152263, 0.9972030041152263, 0.9972030041152263}},
    {.name       = "power nose, parameter 0.25",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.25,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0487841798011042,
     .forces =
         {{{0.05004036449803102, 2.0, 0.0, 0.0},
           {0.05188758228507033, 2.1167706199278076, 0.07388923365444347, 0.07667867382451446},
           {0.05757359820749237, 2.5700560068267686, 0.44855939279784973, 0.5165035650628028}}},
     .slowForces =
         {{{0.05573581473564613, 2.1169637354776407, 1.8473993664701562, 2.059326176646613},
           {0.06732536831286545, 2.641505708106667, 4.149267463501705, 5.5870192041768245},
           {0.06736157604295125, 3.8289553770280897, 2.6731151296432407, 3.601304961540533},
           {0.06881433296024712, 4.013889509600206, 3.5027793876892073, 4.820828541414713}}},
     .pressureCD = {0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.14,
                    0.148,
                    0.15599999999999997,
                    0.1573,
                    0.18049999999999997,
                    0.21649999999999997,
                    0.2375,
                    0.249,
                    0.252,
                    0.252,
                    0.252}},
    {.name       = "power nose, parameter 0.4",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.4,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.04331483267815459,
     .forces = {{{0.06667502342515526, 2.0, 0.0, 0.0},
                 {0.06771262412853435, 2.104272879491228, 0.07345298021508759, 0.09947388080849791},
                 {0.07098531612535439, 2.508174108411763, 0.4377589640505847, 0.621489168996768}}},
     .slowForces =
         {{{0.06991429405518154, 2.078426453571882, 1.8137692437689201, 2.5361679251420903},
           {0.07689127636309616, 2.495570314750103, 3.920032683667846, 6.028326328645477},
           {0.07691393106757859, 3.6169737746476587, 2.5251240530355616, 3.884344347045237},
           {0.07782750414895857, 3.7730314976892165, 3.2925911208064824, 5.125082982307814}}},
     .pressureCD = {0.0035611129705446273,
                    0.003561113618155744,
                    0.0035611145895724193,
                    0.003627377079599687,
                    0.0075924844355416835,
                    0.03532993328785406,
                    0.048139871326022265,
                    0.0644,
                    0.08556799999999999,
                    0.086,
                    0.08611999999999997,
                    0.092,
                    0.0914,
                    0.09140001319999998,
                    0.10869999999999999,
                    0.11279999999999998,
                    0.11333499999999999,
                    0.1229,
                    0.1334,
                    0.14179999999999998,
                    0.14639999999999997,
                    0.14759999999999998,
                    0.14759999999999998,
                    0.14759999999999998}},
    {.name       = "power nose, parameter 0.5",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.04038838613356745,
     .forces = {{{0.07500337320387503, 2.0, 0.0, 0.0},
                 {0.07570295127487288, 2.0973112663515763, 0.07320997407401353,
                  0.11084422200319505},
                 {0.0779402346977145, 2.473704050460198, 0.43174280400450404, 0.673002709463208}}},
     .slowForces =
         {{{0.07720286730498356, 2.0569600413477676, 1.795036265173862, 2.771638931757019},
           {0.08209981394129261, 2.4142799601386584, 3.792342093240334, 6.227011605135271},
           {0.0821160814839519, 3.498893717217694, 2.4426886217116865, 4.01168035800798},
           {0.08277411646799294, 3.6388664212884994, 3.1755100045873697, 5.257000699299835}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.014000000000000002,
                    0.049280000000000004,
                    0.05,
                    0.05019999999999998,
                    0.06,
                    0.059,
                    0.05900002199999996,
                    0.0825,
                    0.084,
                    0.084025,
                    0.0845,
                    0.078,
                    0.078,
                    0.078,
                    0.078,
                    0.078,
                    0.078}},
    {.name       = "power nose, parameter 0.6",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.6,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.037866375250193055,
     .forces = {{{0.0818197389746363, 2.0, 0.0, 0.0},
                 {0.08227966960699404, 2.091212876694991, 0.07299710011685957, 0.12012354559767743},
                 {0.0837687258179135, 2.443508197766745, 0.42647263350502457, 0.7145013820985182}}},
     .slowForces =
         {{{0.08327487255287849, 2.0381554132184183, 1.7786261314003478, 2.9622972882316656},
           {0.08661303837680244, 2.3430694162894206, 3.6804848325328843, 6.3755594809082},
           {0.08662436104193327, 3.3954553062582558, 2.370474987940759, 4.106817623925054},
           {0.08708369830695152, 3.5213374954800676, 3.072946668502906, 5.352071211865176}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0029999999999999957,
                    0.005999999999999998,
                    0.026999999999999986,
                    0.060515999999999986,
                    0.0612,
                    0.06160666666666662,
                    0.08153333333333332,
                    0.08254285714285714,
                    0.08254286719999998,
                    0.0923,
                    0.092,
                    0.091775,
                    0.0883,
                    0.0804,
                    0.0789,
                    0.07746666666666667,
                    0.0764,
                    0.0764,
                    0.0764}},
    {.name       = "power nose, parameter 0.75",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.75,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03465700326231658,
     .forces = {{{0.09000057572260776, 2.0, 0.0, 0.0},
                 {0.09022002462297336, 2.0833658946173106, 0.07272318876965855, 0.1312217576291947},
                 {0.09094198492787776, 2.4046542825122623, 0.4196913460202088,
                  0.7633512812826112}}},
     .slowForces =
         {{{0.09070057554307948, 2.013958930504479, 1.7575107168623412, 3.1881446708508907},
           {0.09237177244946619, 2.2514406597170367, 3.5365547182802, 6.533556553841293},
           {0.09237760217484114, 3.262357980136122, 2.277555525283484, 4.20790236491498},
           {0.09261505104376733, 3.3701094785179446, 2.9409753276403623, 5.447571601757449}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.007499999999999991,
                    0.015,
                    0.046499999999999965,
                    0.07737,
                    0.078,
                    0.07871666666666659,
                    0.11383333333333334,
                    0.11785714285714285,
                    0.11785713500000002,
                    0.107,
                    0.104,
                    0.10339999999999999,
                    0.094,
                    0.084,
                    0.08025,
                    0.07666666666666666,
                    0.074,
                    0.074,
                    0.074}},
    {.name       = "power nose, parameter 0.9",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.9,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03197318770737957,
     .forces = {{{0.09642874409967588, 2.0, 0.0, 0.0},
                 {0.09649740532880588, 2.0767547034245513, 0.07249241466207355,
                  0.13990659841819963},
                 {0.0967264099352694, 2.3719193202309943, 0.413978017296966, 0.8008521481051275}}},
     .slowForces =
         {{{0.09664929772447146, 1.9935730583525395, 1.7397206873652429, 3.3628556534117098},
           {0.09719454874360618, 2.1742421619432397, 3.4152916015430357, 6.638954520796068},
           {0.09719649853886468, 3.1502216375871432, 2.1992695897385284, 4.275226069311808},
           {0.09727620544682195, 3.242697777568324, 2.829787643283499, 5.505420083178475}}},
     .pressureCD = {0.017621195935168966, 0.02389094619206862, 0.03329557157741811,
                    0.06293320825879394,  0.08594333105245633, 0.10163939238320932,
                    0.10463939238320932,  0.11723939238320931, 0.12958739238320932,
                    0.12983939238320932,  0.13092920418340928, 0.17597956593785413,
                    0.1877074625698494,   0.18770745497752808, 0.14697785167982216,
                    0.13502510432950493,  0.1337196968889356,  0.11576693137968251,
                    0.09612879392662904,  0.0876789341540793,  0.08215786653229362,
                    0.0764566367354596,   0.0764566367354596,  0.0764566367354596}},
    {.name       = "power nose, parameter 1",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0304138126514911,
     .forces     = {{{0.09999999999999999, 2.0, 0.0, 0.0},
                     {0.1, 2.072897421541491, 0.07235777012399969, 0.14471554024799937},
                     {0.09999999999999999, 2.3528201937243383, 0.4106445908790053,
                      0.8212891817580105}}},
     .slowForces =
         {{{0.09999999999999998, 1.9816789755723923, 1.7293411420643263, 3.4586822841286518},
           {0.09999999999999999, 2.129200904145632, 3.3445409592403315, 6.689081918480662},
           {0.09999999999999999, 3.084795987362331, 2.153593869271283, 4.307187738542566},
           {0.09999999999999998, 3.168359760192403, 2.76491548515277, 5.529830970305539}}},
     .pressureCD = {0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.1657375625279044,  0.2174103876742013,
                    0.23427434237832045, 0.2342743349625468,  0.1736297527997036,
                    0.1557085072158416,  0.153932828148226,   0.13027821896613753,
                    0.10421465654438171, 0.09263155692346552, 0.08581866644271158,
                    0.07809439455909935, 0.07809439455909935, 0.07809439455909935}},
    {.name       = "power nose, parameter 0.5, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.008483330337458234,
     .forces =
         {{{0.012500562200645843, 2.0, 0.0, 0.0},
           {0.012520779906730787, 2.0158801012119616, 0.07036749018317053, 0.017621117143450347},
           {0.012598028181770445, 2.0705019584855306, 0.36137076344563457, 0.0910511812391201}}},
     .slowForces =
         {{{0.01257015449678446, 1.8058637905635244, 1.5759134493939404, 0.396189510648847},
           {0.012825768400080768, 1.463412947302412, 2.2987236822067225, 0.5896579512752858},
           {0.012827000109552076, 2.1176913344671644, 1.4784274530961798, 0.3792757820565899},
           {0.01288010226369147, 2.0695148538869796, 1.8059924059628583, 0.4652273375250363}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.2988254197739337,
                    0.43934246119990594,
                    0.44221015592288493,
                    0.4428606180439495,
                    0.47473326197611593,
                    0.48044298476409975,
                    0.48044304767075113,
                    0.5533665769219405,
                    0.5633835175955726,
                    0.5646449912840376,
                    0.5860168035032332,
                    0.6052324495293304,
                    0.6206550855095987,
                    0.6299640851250217,
                    0.6299640851250217,
                    0.6299640851250217,
                    0.6299640851250217}},
    {.name       = "power nose, parameter 0.9, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.9,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.08435842021658117,
     .forces = {{{0.2571433175991357, 2.0, 0.0, 0.0},
                 {0.25760310335906245, 2.205356095492072, 0.07698145009052627, 0.3966132088880069},
                 {0.2588120628377611, 3.0086822871316565, 0.5251141205688082, 2.718117375393001}}},
     .slowForces =
         {{{0.25845148706297516, 2.390120588262312, 2.085773689244054, 10.78142623323907},
           {0.2603643724962824, 3.675913190623368, 5.7741109374480954, 30.067455419051885},
           {0.2603692182387218, 5.331506270370618, 3.7220935403476116, 19.38237170623408},
           {0.26055904176696926, 5.7211198395044045, 4.99261890502607, 26.017439956024962}}},
     .pressureCD = {0.005494146543490156, 0.005494146543490156, 0.005494146543490156,
                    0.005494146543490156, 0.005494146543490156, 0.006142298791217845,
                    0.006790451038945535, 0.010206572782987703, 0.013554372092149033,
                    0.013622694527029878, 0.014003636191061614, 0.03074494467320514,
                    0.03467301995047489,  0.034673013687730174, 0.01608334074979279,
                    0.01275911737538818,  0.012548556276500129, 0.009695235031630247,
                    0.00692946670349284,  0.005903193068680101, 0.005235372147022012,
                    0.004690652215991112, 0.004690652215991112, 0.004690652215991112}},
    {.name       = "power nose, parameter 0.25, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.25,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0100907403323495,
     .forces = {{{0.008340060749671845, 2.0, 0.0, 0.0},
                 {0.00839385384734803, 2.019123326808, 0.07048070011324219, 0.011832093916186413},
                 {0.008597805911466997, 2.086560617879959, 0.3641735282445255,
                  0.06262186627881149}}},
     .slowForces =
         {{{0.008524500940750745, 1.8158644062518368, 1.5846406329433227, 0.2701654113255442},
           {0.009184864248279323, 1.5012839052970803, 2.358211243916951, 0.4331970028828622},
           {0.009187982906209655, 2.1727016111022306, 1.5168318710847724, 0.2787325060624179},
           {0.009321871960327143, 2.132018701938931, 1.8605373031464978, 0.34687381034688036}}},
     .pressureCD = {0.5015578439420733, 0.501646573405084,  0.5017796675996002,
                    0.5095876002276211, 0.5342111871077266, 0.5684855408493209,
                    0.5769599712526147, 0.5860015821991759, 0.5973727026051481,
                    0.5976047662869026, 0.5978174988098724, 0.6082413924353952,
                    0.618590230894626,  0.6185902505720909, 0.6568223947574372,
                    0.6752060891063271, 0.678357730299639,  0.7326358967564064,
                    0.8158262150203875, 0.8601916432349567, 0.8846204534841657,
                    0.8877245453833486, 0.8877245453833486, 0.8877245453833486}},
    {.name       = "power nose, parameter 0.75, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::POWER,
     .param      = 0.75,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.091565329610251,
     .forces = {{{0.24000153526028745, 2.0, 0.0, 0.0},
                 {0.24146404816978728, 2.222985938672763, 0.07759684771075293, 0.3747369794690578},
                 {0.24520114841401405, 3.095975519881705, 0.5403496638307892, 2.649887162328719}}},
     .slowForces =
         {{{0.24410262239863154, 2.444482914000817, 2.1332137679029826, 10.414461497639675},
           {0.24978145075824848, 3.881775851353493, 6.097479248747199, 30.460744254407807},
           {0.2497953822443929, 5.630536517167896, 3.9308560351341604, 19.638193716880327},
           {0.2503393803848887, 6.06088437537006, 5.289119396644373, 26.48149745075297}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    6.297116158941357E-4,
                    0.0012594232317882728,
                    0.007470991210525115,
                    0.01650299231680454,
                    0.016687318869993913,
                    0.016952387697276552,
                    0.029940760234127332,
                    0.031197256240337243,
                    0.031197252315698767,
                    0.02585167803867979,
                    0.024430738320750697,
                    0.024172375372344915,
                    0.02021513168080362,
                    0.016121827385736864,
                    0.014747624787371455,
                    0.013534761611676875,
                    0.012758078378326206,
                    0.012758078378326206,
                    0.012758078378326206}},
    {.name       = "parabolic nose, parameter 0",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0304138126514911,
     .forces     = {{{0.09999999999999999, 2.0, 0.0, 0.0},
                     {0.1, 2.072897421541491, 0.07235777012399969, 0.14471554024799937},
                     {0.09999999999999999, 2.3528201937243383, 0.4106445908790053,
                      0.8212891817580105}}},
     .slowForces =
         {{{0.09999999999999998, 1.9816789755723923, 1.7293411420643263, 3.4586822841286518},
           {0.09999999999999999, 2.129200904145632, 3.3445409592403315, 6.689081918480662},
           {0.09999999999999999, 3.084795987362331, 2.153593869271283, 4.307187738542566},
           {0.09999999999999998, 3.168359760192403, 2.76491548515277, 5.529830970305539}}},
     .pressureCD = {0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.16439898730534885, 0.16439898730534885,
                    0.16439898730534885, 0.1657375625279044,  0.2174103876742013,
                    0.23427434237832045, 0.2342743349625468,  0.1736297527997036,
                    0.15570850721584156, 0.153932828148226,   0.13027821896613753,
                    0.10421465654438171, 0.09263155692346552, 0.08581866644271158,
                    0.07809439455909935, 0.07809439455909935, 0.07809439455909935}},
    {.name       = "parabolic nose, parameter 0.3",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.3,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03215871033224633,
     .forces = {{{0.09543281106349194, 2.0, 0.0, 0.0},
                 {0.095551571763686, 2.0772091320864097, 0.07250827721480328, 0.13856559707503027},
                 {0.09594729686928619, 2.374169399629962, 0.4143707302363988, 0.7951550293586931}}},
     .slowForces =
         {{{0.09581410969368759, 1.9949743072852537, 1.740943507741039, 3.3361390444242627},
           {0.0967544583472789, 2.179548499185347, 3.4236267765916724, 6.625023087047353},
           {0.09675781514572743, 3.1579294721670967, 2.2046506734032985, 4.266343646361197},
           {0.09689500420886249, 3.251455583397788, 2.8374302706322236, 5.4986563603052625}}},
     .pressureCD = {0.014854738551920117, 0.015165313984672852, 0.015631177133801955,
                    0.026445220694067705, 0.04783579245642354,  0.07055959492213955,
                    0.07535959492213955,  0.09095959492213955,  0.12506359492213956,
                    0.12575959492213956,  0.12660702501116175,  0.16256415506968053,
                    0.16870973695132818,  0.16870972648501872,  0.13320190111988145,
                    0.12228340288633663,  0.1212731312592904,   0.10611128758645502,
                    0.09448586261775267,  0.08985262276938619,  0.08712746657708462,
                    0.08403775782363973,  0.08403775782363973,  0.08403775782363973}},
    {.name       = "parabolic nose, parameter 0.5",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0337174627945467,
     .forces = {{{0.09111167625006703, 2.0, 0.0, 0.0},
                 {0.09136170809445683, 2.081041763681893, 0.07264206129551758, 0.13273405598921434},
                 {0.09218823805659866, 2.393146471546072, 0.4176828541096375, 0.7701089277363766}}},
     .slowForces =
         {{{0.09191118506526537, 2.0067923799189087, 1.7512567216758947, 3.2192016128548615},
           {0.09384324110792332, 2.2243019169984275, 3.493925280903977, 6.557625450978812},
           {0.09385003896177048, 3.2229370142157787, 2.250034499298425, 4.223316508489698},
           {0.09412726742575614, 3.325318537358131, 2.9018878577250726, 5.46293548847285}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.007999999999999991,
                    0.016,
                    0.042,
                    0.09884000000000001,
                    0.1,
                    0.10051999999999994,
                    0.126,
                    0.125,
                    0.12499998750000003,
                    0.10625000000000001,
                    0.1,
                    0.0995,
                    0.09,
                    0.088,
                    0.088,
                    0.088,
                    0.088,
                    0.088,
                    0.088}},
    {.name       = "parabolic nose, parameter 0.6",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.6,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03466769022643676,
     .forces = {{{0.08836809430575492, 2.0, 0.0, 0.0},
                 {0.08871017428758836, 2.083368718579151, 0.07272328734452269, 0.1290259099019795},
                 {0.08983556945475903, 2.404668265209425, 0.4196937864612468, 0.7540686060674027}}},
     .slowForces =
         {{{0.08945926047114211, 2.013967638303628, 1.7575183158506285, 3.144525776009688},
           {0.09206429944219283, 2.251473634956369, 3.5366065156650186, 6.5119040253478895},
           {0.09207338664455926, 3.26240587903105, 2.277588965020466, 4.194106587874218},
           {0.09244351431653373, 3.3701639022626253, 2.9410228213171594, 5.437569705753705}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.004799999999999995,
                    0.009600000000000001,
                    0.0344,
                    0.088104,
                    0.0892,
                    0.08971199999999994,
                    0.11480000000000003,
                    0.11779999999999999,
                    0.1177999921,
                    0.10445,
                    0.099,
                    0.09836,
                    0.08866666666666667,
                    0.08560000000000001,
                    0.08560000000000001,
                    0.08560000000000001,
                    0.08560000000000001,
                    0.08560000000000001,
                    0.08560000000000001}},
    {.name       = "parabolic nose, parameter 0.75",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.75,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.03638595612729109,
     .forces = {{{0.08320109862834213, 2.0, 0.0, 0.0},
                 {0.08373339185003574, 2.087557237394215, 0.07286949423273188, 0.12203219829006515},
                 {0.08546958795697072, 2.4254074938034593, 0.42331346469414327,
                  0.7236085480809219}}},
     .slowForces =
         {{{0.08489157928137811, 2.0268831033961217, 1.7687891853651492, 3.003106147229395},
           {0.08884160848172433, 2.300382727280663, 3.613432738234892, 6.4204635321061865},
           {0.08885518025396547, 3.3334498356985365, 2.3271870033201387, 4.135652413293933},
           {0.08940676419878973, 3.4508855590907137, 3.0114657557829143, 5.3849081744002625}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.023,
                    0.072,
                    0.073,
                    0.07349999999999994,
                    0.098,
                    0.10699999999999998,
                    0.10699999899999998,
                    0.10175000000000001,
                    0.09749999999999999,
                    0.09664999999999999,
                    0.08666666666666667,
                    0.082,
                    0.082,
                    0.082,
                    0.082,
                    0.082,
                    0.082}},
    {.name       = "parabolic nose, parameter 0.9",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.9,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0385885836442153,
     .forces = {{{0.07619993603087899, 2.0, 0.0, 0.0},
                 {0.07702207351883844, 2.092888079522478, 0.0730555757268163, 0.11253783849183827},
                 {0.07967479380707475, 2.451802875650412, 0.42792032789964796,
                  0.6818892778252053}}},
     .slowForces =
         {{{0.07879652641061126, 2.043320968059296, 1.78313392838363, 2.810095193630754},
           {0.0847039458389769, 2.3626306629661284, 3.711211566960186, 6.287085271295603},
           {0.08472386718523564, 3.4238694169117005, 2.3903117793379036, 4.050329154478576},
           {0.08553137879899123, 3.5536222132355517, 3.1011203996484196, 5.304862072072161}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.009199999999999998,
                    0.05279999999999999,
                    0.053799999999999994,
                    0.054611999999999904,
                    0.0944,
                    0.1082,
                    0.1082000056,
                    0.11119999999999998,
                    0.10859999999999999,
                    0.10807999999999998,
                    0.10146666666666666,
                    0.09760000000000002,
                    0.09760000000000002,
                    0.09760000000000002,
                    0.09760000000000002,
                    0.09760000000000002,
                    0.09760000000000002}},
    {.name       = "parabolic nose, parameter 1",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.040438699365261815,
     .forces = {{{0.07000203449279072, 2.0, 0.0, 0.0},
                 {0.07110871412485216, 2.0973304479626975, 0.07321064363855334,
                  0.10411829458780632},
                 {0.07464779151779298, 2.47379902718954, 0.43175938057090196, 0.6445976845341624}}},
     .slowForces =
         {{{0.07348139969037534, 2.0570191886119416, 1.7950878808990314, 2.6381114011138123},
           {0.08122712864217661, 2.414503942704017, 3.792693924231265, 6.161392745678691},
           {0.08125285812072082, 3.499219067922673, 2.4429157593527093, 3.969877751911173},
           {0.08229362673100087, 3.639236091689587, 3.175832602869677, 5.227015655614002}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.04,
                    0.041,
                    0.04201999999999989,
                    0.092,
                    0.109,
                    0.10900000999999998,
                    0.1175,
                    0.11599999999999999,
                    0.1157,
                    0.11133333333333334,
                    0.108,
                    0.108,
                    0.108,
                    0.108,
                    0.108,
                    0.108}},
    {.name       = "parabolic nose, parameter 0.3, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.3,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0072890454022652715,
     .forces =
         {{{0.015905468510582, 2.0, 0.0, 0.0},
           {0.01590887344039356, 2.0125297455011006, 0.07025054070663549, 0.022352139224421596},
           {0.01592198815092296, 2.0539128500138246, 0.358475417817617, 0.11415282709778508}}},
     .slowForces =
         {{{0.015917236620054447, 1.7955328348864388, 1.5668979898218032, 0.49881372126962603},
           {0.01596164839779739, 1.424291037143527, 2.237271129431946, 0.7142107027707154},
           {0.015961867044867913, 2.060863960292065, 1.4387544617114485, 0.45930414856097285},
           {0.015971336939669286, 2.004946380905195, 1.749645783637001, 0.558883646710765}}},
     .pressureCD = {0.3241240496631735, 0.3521578839354144, 0.3942086353437758, 0.5275220933819298,
                    0.6312757692271669, 0.7021270342162306, 0.7156722365445286, 0.7380208245562208,
                    0.775797202584274,  0.7765681490746424, 0.7774369228119065, 0.8262014412423169,
                    0.8781292372008426, 0.8781293431512622, 1.0099666674871521, 1.0250862896089417,
                    1.0258245702445072, 1.0412907193935206, 1.074108697222049,  1.0905279053054409,
                    1.0989445623099585, 1.090699640943845,  1.090699640943845,  1.090699640943845}},
    {.name       = "parabolic nose, parameter 1, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.10683142790499327,
     .forces = {{{0.18667209198077517, 2.0, 0.0, 0.0},
                 {0.19397464329759312, 2.260224747593795, 0.07889672736113901, 0.3060792909444879},
                 {0.2115857021322203, 3.2803615056877775, 0.5725310892993041, 2.4227878504383633}}},
     .slowForces =
         {{{0.20655834145614613, 2.559310268954051, 2.23341953866749, 9.226628713658155},
           {0.23132116381605577, 4.316611272652106, 6.780517131283372, 31.369542281663467},
           {0.23137835116680008, 6.262166084598695, 4.3718166593187595, 20.230874604734453},
           {0.23359864925566123, 6.77855534382777, 5.915405463922543, 27.63661452343728}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.005835080111635825,
                    0.006021289472401933,
                    0.006328144022382848,
                    0.02136401697144943,
                    0.027563829608932725,
                    0.027563832938102276,
                    0.030014658148033856,
                    0.0291363171309339,
                    0.028960648927513908,
                    0.02643846527612353,
                    0.024556125634902615,
                    0.024556125634902615,
                    0.024556125634902615,
                    0.024556125634902615,
                    0.024556125634902615,
                    0.024556125634902615}},
    {.name       = "parabolic nose, parameter 0.5, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.007502509936323894,
     .forces =
         {{{0.01518527937501117, 2.0, 0.0, 0.0},
           {0.015192458863270043, 2.013168517433681, 0.0702728380534212, 0.021352344026636785},
           {0.015220069458798387, 2.057075695333176, 0.35902743846315677, 0.1092884510204742}}},
     .slowForces =
         {{{0.015210073780115943, 1.7975025136587146, 1.5686168588109457, 0.4771755631049639},
           {0.015303158140389139, 1.4317499401123734, 2.2489875468173293, 0.6883322416942281},
           {0.015303614514648144, 2.0716985506335117, 1.4463184326939693, 0.4426779951875716},
           {0.015323362946852893, 2.0172568732319185, 1.7603887148191422, 0.5395015040943525}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.15296638192153106,
                    0.30593276384306245,
                    0.41206578888546147,
                    0.5390040439106609,
                    0.5415946205438282,
                    0.542588594140868,
                    0.5897836972475485,
                    0.5984222163739055,
                    0.598422217706749,
                    0.5960730628619019,
                    0.5928584662803874,
                    0.5933134509893802,
                    0.5977095733051347,
                    0.626967072330418,
                    0.6434026985387218,
                    0.6525868506080563,
                    0.6525868506080563,
                    0.6525868506080563,
                    0.6525868506080563}},
    {.name       = "parabolic nose, parameter 0.75, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.75,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.09614486263762961,
     .forces = {{{0.2218695963422457, 2.0, 0.0, 0.0},
                 {0.22540640826383315, 2.2341628527445083, 0.0779869956122818, 0.3515753714450351},
                 {0.23428516942020217, 3.1513174166582294, 0.550008646961281, 2.5771773807179974}}},
     .slowForces =
         {{{0.23169859021422146, 2.4789473750451982, 2.163289683910471, 10.024623399740497},
           {0.24486620646764554, 4.012288031523164, 6.3024873019597125, 30.865323138827623},
           {0.2448978980844417, 5.8201147986676665, 4.063206643231906, 19.901415328204674},
           {0.2461331035330341, 6.276287256897443, 5.477093871691178, 26.961882259622225}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.002448022356638643,
                    0.014772441689349837,
                    0.015023960451241903,
                    0.015195763362897819,
                    0.02361410603403866,
                    0.026766534527419217,
                    0.026766533478448103,
                    0.02388641003609386,
                    0.02205525666192709,
                    0.021725094117822814,
                    0.01771928469493153,
                    0.015732876849470848,
                    0.015732876849470848,
                    0.015732876849470848,
                    0.015732876849470848,
                    0.015732876849470848,
                    0.015732876849470848}},
    {.name       = "Haack nose, parameter 0 (Von Karman)",
     .body       = Body::NOSE,
     .shape      = TransitionShape::HAACK,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0394641578689723,
     .forces =
         {{{0.07500206721041283, 2.0, 0.0, 0.0},
           {0.07582021307761991, 2.0950619587349393, 0.07313145842641253, 0.11089685521135398},
           {0.07844851523609897, 2.4625667231854664, 0.42979897369634174, 0.6743418267295432}}},
     .slowForces =
         {{{0.07758027669165608, 2.0500242114547573, 1.7889836117742983, 2.775796871965766},
           {0.08338352780930494, 2.3880149249925386, 3.75108507250967, 6.255574129173574},
           {0.08340295288073128, 3.4607418740712594, 2.4160536105672983, 4.030120108789301},
           {0.08418953445538996, 3.5955174844571847, 3.1376809208400958, 5.283197919901738}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.01,
                    0.026660000000000003,
                    0.027,
                    0.02755999999999994,
                    0.055,
                    0.07,
                    0.07000001099999999,
                    0.0845,
                    0.08800000000000001,
                    0.08870000000000001,
                    0.096,
                    0.091,
                    0.087,
                    0.083,
                    0.083,
                    0.083,
                    0.083}},
    {.name       = "Haack nose, parameter 0.2",
     .body       = Body::NOSE,
     .shape      = TransitionShape::HAACK,
     .param      = 0.2,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.04128844511797004,
     .forces = {{{0.06937732746035086, 2.0, 0.0, 0.0},
                 {0.07043793175622115, 2.0994687905693206, 0.07328528587659584,
                  0.10324127930621631},
                 {0.07381520893679677, 2.484386917107002, 0.4336073159698862, 0.640136292496816}}},
     .slowForces =
         {{{0.07270456770154415, 2.0636128534636557, 1.800841938970803, 2.6185886934336557},
           {0.08003582226508596, 2.439473245836226, 3.8319156138739676, 6.133810340136493},
           {0.08006000239002367, 3.5354887675928275, 2.4682367864708574, 3.9521408604800237},
           {0.0810371487175353, 3.6804464970128015, 3.2117954658182515, 5.205494936276383}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.01,
                    0.024896,
                    0.0252,
                    0.02592799999999992,
                    0.0616,
                    0.0784,
                    0.07840001399999998,
                    0.09590000000000001,
                    0.09940000000000002,
                    0.10010000000000002,
                    0.10770000000000002,
                    0.10419999999999999,
                    0.1026,
                    0.101,
                    0.101,
                    0.101,
                    0.101}},
    {.name       = "Haack nose, parameter 1/3 (LV-Haack)",
     .body       = Body::NOSE,
     .shape      = TransitionShape::HAACK,
     .param      = 0.3333333333333333,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.04244570135834023,
     .forces = {{{0.06562751347954289, 2.0, 0.0, 0.0},
                 {0.06686351990526875, 2.102248315135568, 0.07338230958723801, 0.09813199035561765},
                 {0.07077764630356452, 2.498149585464806, 0.4360093547369234, 0.6171943178923072}}},
     .slowForces =
         {{{0.06949408531126877, 2.0721836285023145, 1.80832135116442, 2.5133527649601803},
           {0.07790184296826676, 2.471929593683903, 3.882897925854276, 6.04969808963417},
           {0.07792932238221671, 3.582633935505295, 2.5011503227299827, 3.8982589965282037},
           {0.07903842863226926, 3.7340138410044186, 3.2585417920283577, 5.151000457490002}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.01,
                    0.023719999999999998,
                    0.024,
                    0.024839999999999907,
                    0.066,
                    0.084,
                    0.08400001599999998,
                    0.10350000000000001,
                    0.10700000000000001,
                    0.10770000000000002,
                    0.11549999999999999,
                    0.113,
                    0.11299999999999999,
                    0.11299999999999999,
                    0.11299999999999999,
                    0.11299999999999999,
                    0.11299999999999999}},
    {.name       = "Haack nose, parameter 0, blunt",
     .body       = Body::NOSE,
     .shape      = TransitionShape::HAACK,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.025,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.008421476600802983,
     .forces =
         {{{0.012500344535068814, 2.0, 0.0, 0.0},
           {0.012523967868769696, 2.015505216609189, 0.0703544042419037, 0.017622325963040725},
           {0.012614309435823843, 2.068645737273075, 0.36104679172760745, 0.09108711903326969}}},
     .slowForces =
         {{{0.012581696451973098, 1.8047078189146895, 1.5749046738273464, 0.39629945093778746},
           {0.01288140018234502, 1.4590354414447255, 2.2918475120849453, 0.5904440992015599},
           {0.01288284776661074, 2.111332693942759, 1.473988284572115, 0.3797833335982054},
           {0.012945288136320688, 2.0622900310817607, 1.799687558671646, 0.46594948004712006}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.27081818780811956,
                    0.36731336165167827,
                    0.3692826509137917,
                    0.37115308028273075,
                    0.4628041193607555,
                    0.5050768085207545,
                    0.5050768467935822,
                    0.5566594442035954,
                    0.5699692521588823,
                    0.5726312137499396,
                    0.6083152055731355,
                    0.6331445682425965,
                    0.6373306259724969,
                    0.6415166837023973,
                    0.6415166837023973,
                    0.6415166837023973,
                    0.6415166837023973}},
    {.name       = "Haack nose, parameter 1/3, slender",
     .body       = Body::NOSE,
     .shape      = TransitionShape::HAACK,
     .param      = 0.3333333333333333,
     .clipped    = false,
     .length     = 0.4,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.1121970551625867,
     .forces = {{{0.17500670261211437, 2.0, 0.0, 0.0},
                 {0.1831345958842074, 2.2733390600547834, 0.07935450322429814, 0.2906510975914775},
                 {0.20235559486789056, 3.345296327755154, 0.583864353742028, 2.3629643724724914}}},
     .slowForces =
         {{{0.19692265777186233, 2.5997487753283792, 2.268708792708527, 8.935203303411123},
           {0.22327794325537287, 4.4697463419318035, 7.0210611356114025, 31.352961796590932},
           {0.22333764912243376, 6.484605731485688, 4.527108828324823, 20.22147686078963},
           {0.22565168462936397, 7.031296008667322, 6.135963301679026, 27.691809116956527}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    6.538683619946563E-4,
                    0.002538282762464223,
                    0.0025767401992084996,
                    0.0027776067899974617,
                    0.012620069738657713,
                    0.01823918669176234,
                    0.018239191901394503,
                    0.024527779293947997,
                    0.025606739724339838,
                    0.025822531810418207,
                    0.02796501263411713,
                    0.025796221748439006,
                    0.025376123859467664,
                    0.024956025970496325,
                    0.024956025970496325,
                    0.024956025970496325,
                    0.024956025970496325}},
    {.name       = "conical transition, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.015461646096066223,
     .forces =
         {{{0.027777777777777773, 1.5, 0.0, 0.0},
           {0.027777777777777773, 1.5363471778167552, 0.05362863341325033, 0.02979368522958351},
           {0.02777777777777777, 1.6738754818848187, 0.29214638427296785, 0.1623035468183154}}},
     .slowForces =
         {{{0.027777777777777773, 1.429750622887865, 1.2476927925916521, 0.6931626625509176},
           {0.02777777777777777, 1.3829103382566068, 2.172270479620166, 1.206816933122314},
           {0.027777777777777773, 2.00276070816043, 1.3981907394789108, 0.7767726330438391},
           {0.027777777777777773, 2.0230910151978705, 1.765479964135874, 0.9808222022977077}}},
     .pressureCD = {0.03529411764705889, 0.035294117758923785, 0.035294117926721116,
                    0.03534001791742991, 0.04007296126336075,  0.08462524749131783,
                    0.10765254098597223, 0.13925255768821942,  0.18104873555547904,
                    0.18190171877725983, 0.18288428513313681,  0.22250253932213873,
                    0.2405498305117111,  0.24054983984357836,  0.21546089505355073,
                    0.20213370335556222, 0.20016897668849656,  0.17399598050552573,
                    0.14515756197458002, 0.1323412681254019,   0.12480304353775003,
                    0.11625640736339164, 0.11625640736339164,  0.11625640736339164}},
    {.name       = "ogive transition, tangent, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.017324529907587788,
     .forces =
         {{{0.018457021447628907, 1.5, 0.0, 0.0},
           {0.018696061512689578, 1.5406211720884173, 0.053777823957754094, 0.020108670094655237},
           {0.019532791042193087, 1.6950379376653213, 0.2958399295847426, 0.11557159053031786}}},
     .slowForces =
         {{{0.019244014648672293, 1.4429296549654202, 1.2591936676906164, 0.4846388277310722},
           {0.02145828966084581, 1.432817522551548, 2.2506645013913356, 0.9659082160047677},
           {0.02146698941218478, 2.0752544648414326, 1.448800929127836, 0.6220278841190147},
           {0.02182794706053148, 2.1054599656825905, 1.8373604335044011, 0.8021161254729847}}},
     .pressureCD = {1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    1.0658141036401504E-14,
                    0.0010888888888996322,
                    0.042777777777784916,
                    0.055555555555570464,
                    0.0555555477777927,
                    0.00888888888889923,
                    2.1333520887345528E-14,
                    1.0658141036401623E-14,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0}},
    {.name       = "ogive transition, parameter 0.5, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::OGIVE,
     .param      = 0.5,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.016051693429396403,
     .forces =
         {{{0.024765269951364254, 1.5, 0.0, 0.0},
           {0.02483719281853255, 1.5377874077559461, 0.053678906922099694, 0.02666466723024503},
           {0.02509105111844824, 1.6810067039840073, 0.29339101732618605, 0.14722978026849734}}},
     .slowForces =
         {{{0.0250030665773076, 1.4341916294407984, 1.2515683019142185, 0.6258609115761804},
           {0.025687945114541904, 1.3997278200859253, 2.1986873183035995, 1.1295951831324438},
           {0.02569068307966755, 2.0271893040218356, 1.4152451166557343, 0.7271722754409918},
           {0.025804614547540076, 2.0508473061537043, 1.7897018974019152, 0.9236513523491507}}},
     .pressureCD = {0.015734274179634654, 0.015734274179640184, 0.01573427417964847,
                    0.0157348851302638,   0.016288606945056527, 0.03271859498984304,
                    0.045516134948362125, 0.06639423909138892,  0.09892768375590995,
                    0.09959163160620631,  0.10041557045891493,  0.13219143425972965,
                    0.14247017480773355,  0.14247016998112313,  0.10481718442864509,
                    0.0938122282354398,   0.09273653562655117,  0.07840675682398433,
                    0.0626176528301035,   0.05560070090593108,  0.05147350468620962,
                    0.04679420068033967,  0.04679420068033967,  0.04679420068033967}},
    {.name       = "ellipsoid transition, clipped, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = true,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.017725522893556344,
     .forces =
         {{{0.016650912855229582, 1.5, 0.0, 0.0},
           {0.016937972046496396, 1.5414670853392285, 0.0538073519005799, 0.01822774844776034},
           {0.01794030964772856, 1.6992264322272173, 0.2965709597928122, 0.10641149702414215}}},
     .slowForces =
         {{{0.017594812364979118, 1.4455380622675043, 1.2614699324733378, 0.4439065353186251},
           {0.02023237411221777, 1.442695202731345, 2.2661803251350157, 0.9170041628775788},
           {0.020242684380489245, 2.0896025021760005, 1.4588177488353273, 0.5906077451665872},
           {0.02067010285946734, 2.1217625113206156, 1.85158708839641, 0.7654499114083088}}},
     .pressureCD = {2.105265445496261E-6,  4.393569753980815E-6, 7.826026216707646E-6,
                    0.0016747230051552123, 0.015045769664496092, 0.04536672998707579,
                    0.054375027233921645,  0.0645368668351202,   0.07569969066669315,
                    0.07592750339794972,   0.0761814106167231,   0.0886228643366193,
                    0.10269952175156329,   0.10269955282185327,  0.1531717158916827,
                    0.16503473185390197,   0.16588570830630153,  0.1762457926187764,
                    0.18761309724740616,   0.19164240327383703,  0.19164240327383703,
                    0.19164240327383703,   0.19164240327383703,  0.19164240327383703}},
    {.name       = "ellipsoid transition, not clipped, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.018775556613277965,
     .forces =
         {{{0.012716523625958993, 1.5, 0.0, 0.0},
           {0.013110776159361777, 1.5433157849571388, 0.05387188369100569, 0.014126044167118958},
           {0.014480029444724707, 1.7083801702597778, 0.2981685884681443, 0.08634979881021466}}},
     .slowForces =
         {{{0.014009349539397597, 1.4512386013926373, 1.2664445913169542, 0.35484129904077494},
           {0.017568666633133712, 1.4642823614107034, 2.30008935469449, 0.8081900619809427},
           {0.017582428413616034, 2.120959394993018, 1.480708989749404, 0.5206891962733322},
           {0.018151905338408682, 2.157390880852697, 1.8826787061690127, 0.6834841131403531}}},
     .pressureCD = {9.375454129456688E-7,  3.2260683731588545E-6, 6.658852813478634E-6,
                    0.0016736331376608882, 0.015045016963900197,  0.045366381622743174,
                    0.05437475657446479,   0.06453666840022278,   0.07569955551128107,
                    0.07592736953395572,   0.07618127784503134,   0.08862278508773817,
                    0.10269948473555195,   0.10269951580589655,   0.1531717158916827,
                    0.16503473185390197,   0.16588570830630153,   0.1762457926187764,
                    0.18761309724740616,   0.19164240327383703,   0.19164240327383703,
                    0.19164240327383703,   0.19164240327383703,   0.19164240327383703}},
    {.name       = "power transition, parameter 0.5, clipped, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::POWER,
     .param      = 0.5,
     .clipped    = true,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.016015265412017546,
     .forces =
         {{{0.025006281418123635, 1.5, 0.0, 0.0},
           {0.025070887167984553, 1.537701242490643, 0.05367589918693891, 0.026914048243117182},
           {0.025298976926607798, 1.680580061246351, 0.2933165541211678, 0.14841217469807064}}},
     .slowForces =
         {{{0.025219913265602835, 1.4339259354030391, 1.2513364401261278, 0.6311719297213831},
           {0.02583563736849748, 1.3987216732588676, 2.197106866563441, 1.1352731252873767},
           {0.02583810019185516, 2.025727803752956, 1.414224796987305, 0.7308176399672801},
           {0.02594059185691637, 2.049186718417708, 1.7882527612263466, 0.9277667003195276}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.025767749311790055,
                    0.07016469829346851,
                    0.07107075847676808,
                    0.07130023232178806,
                    0.08254445072776843,
                    0.08185636264875072,
                    0.08185638695962089,
                    0.10813070849395359,
                    0.11009418412845803,
                    0.1101892188920967,
                    0.11183920897093425,
                    0.10669564767600535,
                    0.1074854210870239,
                    0.10795638386118796,
                    0.10795638386118796,
                    0.10795638386118796,
                    0.10795638386118796}},
    {.name       = "parabolic transition, parameter 0.6, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::PARABOLIC,
     .param      = 0.6,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.0161816150532216,
     .forces =
         {{{0.024104536752598788, 1.5, 0.0, 0.0},
           {0.024193083830274898, 1.5380923939896987, 0.05368955295000417, 0.025978317106588673},
           {0.024505338280814238, 1.6825168271323332, 0.2936545835366748, 0.14392209814555676}}},
     .slowForces =
         {{{0.02439716373450444, 1.4351320666764042, 1.2523889882227024, 0.6110947840991924},
           {0.025237820419157765, 1.403289126725063, 2.204281405690947, 1.1126251654023351},
           {0.02524117478846099, 2.032362356771883, 1.4188565887704414, 0.7162721431382846},
           {0.02538071112377587, 2.0567250388762406, 1.7948311868299387, 0.9110818373774894}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.009510696611356373,
                    0.019021393222712766,
                    0.05253941873740118,
                    0.1112409274241201,
                    0.11243891739731844,
                    0.11295091825092111,
                    0.13803896007745503,
                    0.14158890907681246,
                    0.14158890269650068,
                    0.13030647300384451,
                    0.125404348711888,
                    0.1248319105554088,
                    0.11623478787993187,
                    0.1142942993663156,
                    0.1142942993663156,
                    0.1142942993663156,
                    0.1142942993663156,
                    0.1142942993663156,
                    0.1142942993663156}},
    {.name       = "Haack transition, parameter 0.2, clipped, widening",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::HAACK,
     .param      = 0.2,
     .clipped    = true,
     .length     = 0.05,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.01687085547192863,
     .forces =
         {{{0.02067024574197642, 1.5, 0.0, 0.0},
           {0.02084846757275953, 1.539649724253565, 0.053743914031295005, 0.022409564978292596},
           {0.02147408590198183, 1.6902278652736853, 0.29500041357980933, 0.12669728444665984}}},
     .slowForces =
         {{{0.021257856549311494, 1.4399341567584902, 1.2565796023681908, 0.5342437785986776},
           {0.022924345054469917, 1.421473982283289, 2.232846110005104, 1.0237306935857577},
           {0.02293093128750134, 2.058777160827476, 1.437297600852011, 0.6591714504965598},
           {0.023204466709847743, 2.086738101889278, 1.8210225252392402, 0.8451171312959364}}},
     .pressureCD = {0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    0.01973661903529797,
                    0.040861527022158034,
                    0.041292647593318445,
                    0.04215247610746989,
                    0.08428407330089527,
                    0.10254048991497877,
                    0.10254050521919708,
                    0.12178552352265719,
                    0.12572633879308923,
                    0.12651450184717564,
                    0.1355498852843179,
                    0.1342207126875031,
                    0.13335578964414674,
                    0.1324908666007903,
                    0.1324908666007903,
                    0.1324908666007903,
                    0.1324908666007903}},
    {.name       = "boat tail, fineness 0.5",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.015625,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.008286407592029853,
     .forces =
         {{{0.006944444444444443, -2.34375, -0.0, -0.0},
           {0.006944444444444446, -2.328956964013043, -0.08129593431855738, -0.011291101988688525},
           {0.006944444444444444, -2.2609786302558286, -0.3946152141519569, -0.05480766863221623}}},
     .slowForces =
         {{{0.006944444444444444, -2.013248595170769, -1.7568908323440164, -0.2440126156033356},
           {0.006944444444444444, -1.3248976384766613, -2.0811443438983726, -0.2890478255414406},
           {0.006944444444444443, -1.9151093785267728, -1.336998567644583, -0.18569424550619204},
           {0.006944444444444443, -1.781475004424673, -1.5546302184595546, -0.2159208636749381}}},
     .pressureCD = {0.140625,
                    0.14068593749999997,
                    0.141005859375,
                    0.1543359375,
                    0.19546875,
                    0.250693359375,
                    0.2640234375,
                    0.278115234375,
                    0.29266421484375,
                    0.29296875,
                    0.292676073926074,
                    0.2790178571428571,
                    0.26633522727272724,
                    0.26633520306043607,
                    0.23437499999999997,
                    0.22536057692307687,
                    0.22364026717557248,
                    0.19531249999999997,
                    0.146484375,
                    0.11718749999999999,
                    0.09765624999999999,
                    0.07342575187969924,
                    0.0732421875,
                    0.05859374999999999}},
    {.name       = "boat tail, fineness 1",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.03125,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.01310196080566283,
     .forces     = {{{0.013888888888888886, -2.34375, -0.0, -0.0},
                     {0.01388888888888889, -2.3146398637479155, -0.08079617324062813,
                      -0.022443381455730035},
                     {0.013888888888888886, -2.1900882682179885, -0.38224251189704583,
                      -0.10617847552695715}}},
     .slowForces =
         {{{0.013888888888888888, -1.969101244552465, -1.7183650011279286, -0.47732361142442453},
           {0.013888888888888886, -1.157717685466804, -1.8185386877967453, -0.5051496354990959},
           {0.013888888888888886, -1.6722685329319928, -1.1674636750863399, -0.324295465301761},
           {0.013888888888888885, -1.5055540630602733, -1.3138437733590052, -0.36495660371083466}}},
     .pressureCD = {0.140625,
                    0.14068593749999997,
                    0.141005859375,
                    0.1543359375,
                    0.19546875,
                    0.250693359375,
                    0.2640234375,
                    0.278115234375,
                    0.29266421484375,
                    0.29296875,
                    0.292676073926074,
                    0.2790178571428571,
                    0.26633522727272724,
                    0.26633520306043607,
                    0.23437499999999997,
                    0.22536057692307687,
                    0.22364026717557248,
                    0.19531249999999997,
                    0.146484375,
                    0.11718749999999999,
                    0.09765624999999999,
                    0.07342575187969924,
                    0.0732421875,
                    0.05859374999999999}},
    {.name       = "boat tail, fineness just above 1",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.032,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.013354110804940367,
     .forces =
         {{{0.014222222222222216, -2.34375, -0.0, -0.0},
           {0.014222222222222216, -2.3139526429351895, -0.08077218470888753, -0.02297519920608355},
           {0.014222222222222218, -2.186685530840172, -0.38164862218881007, -0.10855783031148371}}},
     .slowForces =
         {{{0.014222222222222216, -1.9669821717227864, -1.7165157612295563, -0.48825337208307357},
           {0.01422222222222221, -1.149693047722331, -1.8059336163038673, -0.5136877841930996},
           {0.014222222222222209, -1.6606121723434435, -1.1593260002435444, -0.32976384006927456},
           {0.014222222222222214, -1.492309857874782, -1.3022860239941787, -0.3704280246027884}}},
     .pressureCD = {0.1389375,           0.13899770624999996, 0.1393137890625,
                    0.15248390625,       0.193123125,         0.24768503906249997,
                    0.26085515625,       0.2747778515625,     0.28915224426562497,
                    0.289453125,         0.2891639610389611,  0.2756696428571428,
                    0.26313920454545453, 0.26313918062371083, 0.23156249999999998,
                    0.22265624999999994, 0.2209565839694656,  0.19296874999999997,
                    0.1447265625,        0.11578124999999999, 0.09648437499999998,
                    0.07254464285714285, 0.07236328125,       0.057890624999999994}},
    {.name       = "boat tail, fineness 2",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.0625,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.027069577980605924,
     .forces =
         {{{0.039428723190463824, -2.34375, -0.0, -0.0},
           {0.03974428893983655, -2.279327547168189, -0.0795635408589825, -0.06324392713951782},
           {0.0411959882545475, -2.015241206985273, -0.35172594283757563, -0.2897939561991282}}},
     .slowForces =
         {{{0.04062101675167027, -1.860214305694677, -1.6233432213536207, -1.318837043766316},
           {0.050696786426463615, -0.7453778039862433, -1.1708367165760374, -1.1871531792103456},
           {0.05079546329302907, -1.0733153469283658, -0.7493154686434076, -0.7612365276475017},
           {0.05623094127116877, -0.8250106951990976, -0.7199576497695814, -0.8096779264384405}}},
     .pressureCD = {0.0703125,           0.07034296874999998, 0.0705029296875,
                    0.07716796875,       0.097734375,         0.1253466796875,
                    0.13201171875,       0.1390576171875,     0.146332107421875,
                    0.146484375,         0.146338036963037,   0.13950892857142855,
                    0.13316761363636362, 0.13316760153021803, 0.11718749999999999,
                    0.11268028846153844, 0.11182013358778624, 0.09765624999999999,
                    0.0732421875,        0.05859374999999999, 0.04882812499999999,
                    0.03671287593984962, 0.03662109375,       0.029296874999999997}},
    {.name       = "boat tail, fineness just below 3",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.0937,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.03562269194825996,
     .forces =
         {{{0.04164444444444447, -2.34375, -0.0, -0.0},
           {0.04164444444444447, -2.2574172774082544, -0.0787987281643605, -0.06563058514667185},
           {0.04164444444444447, -1.9067536692251492, -0.3327912955246173, -0.2771781723613925}}},
     .slowForces =
         {{{0.04164444444444448, -1.7926531136012276, -1.5643849589234682, -1.3029588502322582},
           {0.041644444444444505, -0.48953284927700635, -0.7689564014897613, -0.6404552428408065},
           {0.04164444444444451, -0.7016822412587769, -0.48986661650955426, -0.40800446192840273},
           {0.04164444444444452, -0.4027532446150399, -0.3514685095811284, -0.29273421642446035}}},
     .pressureCD = {1.1249999999998761E-4, 1.1254874999998758E-4, 1.1280468749998756E-4,
                    1.2346874999998638E-4, 1.5637499999998278E-4, 2.005546874999779E-4,
                    2.1121874999997675E-4, 2.224921874999755E-4,  2.341313718749742E-4,
                    2.343749999999742E-4,  2.341408591408334E-4,  2.232142857142611E-4,
                    2.1306818181815833E-4, 2.130681624483254E-4,  1.8749999999997932E-4,
                    1.8028846153844165E-4, 1.7891221374043828E-4, 1.5624999999998276E-4,
                    1.171874999999871E-4,  9.374999999998966E-5,  7.812499999999138E-5,
                    5.8740601503752924E-5, 5.859374999999355E-5,  4.687499999999483E-5}},
    {.name       = "boat tail, fineness 3",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.09375,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.035641186700966124,
     .forces =
         {{{0.041666666666666664, -2.34375, -0.0, -0.0},
           {0.04166666666666666, -2.257371462687406, -0.07879712892891112, -0.06566427410742592},
           {0.04166666666666666, -1.906526820066628, -0.3327517028774016, -0.2772930857311679}}},
     .slowForces =
         {{{0.041666666666666664, -1.7925118420792492, -1.5642616762635768, -1.3035513968863137},
           {0.04166666666666668, -0.488997873427375, -0.7681160633902364, -0.6400967194918639},
           {0.041666666666666664, -0.7009051505528736, -0.4893241048533679, -0.40777008737780657},
           {0.04166666666666668, -0.40187029760267423, -0.350697992956807, -0.29224832746400586}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "boat tail, fineness 4",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::ELLIPSOID,
     .param      = 0.0,
     .clipped    = true,
     .length     = 0.125,
     .foreRadius = 0.03125,
     .aftRadius  = 0.015625,
     .frictionCD = 0.053921029043550145,
     .forces =
         {{{0.08332001975388321, -2.34375, -0.0, -0.0},
           {0.08487941600084578, -2.2127717541988243, -0.07724030541180028, -0.13112224030161151},
           {0.09345555200712888, -1.6856937307546298, -0.29420905781895085, -0.5499093980793469}}},
     .slowForces =
         {{{0.08974908944067837, -1.6549868592504682, -1.4442484885580322, -2.5923997354831876},
           {-1.184118690267747, 0.03179034704319972, 0.049936160362993115, -1.1826068161205516},
           {-0.9697678388282717, 0.055577008689138285, 0.03880007160139843, -0.752541231665407},
           {-0.061984439368249745, 0.45766084507720706, 0.39938443020284725, -0.4951123999706271}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "tail cone to a point",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.03,
     .foreRadius = 0.025,
     .aftRadius  = 0.0,
     .frictionCD = 0.007810249675906656,
     .forces =
         {{{0.009999999999999998, -2.0, -0.0, -0.0},
           {0.009999999999999998, -1.9849331575125484, -0.06928723806120238, -0.013857447612240472},
           {0.009999999999999998, -1.9172698093638487, -0.3346267082248317, -0.06692534164496633}}},
     .slowForces =
         {{{0.009999999999999998, -1.7104376533735326, -1.492638435072682, -0.29852768701453636},
           {0.009999999999999998, -1.1020472728530688, -1.7310918081519335, -0.3462183616303866},
           {0.01, -1.5927818320280038, -1.1119714893934374, -0.22239429787868747},
           {0.01, -1.4731014964495304, -1.285523566454993, -0.2571047132909986}}},
     .pressureCD = {0.12,
                    0.12005199999999999,
                    0.120325,
                    0.13169999999999998,
                    0.1668,
                    0.21392499999999998,
                    0.2253,
                    0.237325,
                    0.24974013,
                    0.25,
                    0.24975024975024981,
                    0.23809523809523808,
                    0.22727272727272727,
                    0.22727270661157215,
                    0.2,
                    0.1923076923076923,
                    0.19083969465648853,
                    0.16666666666666666,
                    0.125,
                    0.1,
                    0.08333333333333333,
                    0.06265664160401002,
                    0.0625,
                    0.05}},
    {.name       = "step up, 0.5 mm long",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 5.0E-4,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.003752998800959042,
     .forces     = {{{2.7777777777777783E-4, 1.5, 0.0, 0.0},
                     {2.7777777777777783E-4, 1.5000619189048165, 0.05236203893734644,
                      2.9090021631859137E-4},
                     {2.7777777777777783E-4, 1.4942109483361168, 0.26078900767812124,
                      0.0014488278204340072}}},
     .slowForces =
         {{{2.7777777777777783E-4, 1.3178635774808354, 1.1500529259575987, 0.00638918292198666},
           {2.7777777777777783E-4, 0.9592094653484243, 1.5067227047962015, 0.008370681693312232},
           {2.777777777777779E-4, 1.38730486908502, 0.9685215077792997, 0.005380675043218333},
           {2.7777777777777783E-4, 1.3237969814039354, 1.155230797673041, 0.00641794887596134}}},
     .pressureCD = {0.6375000000000001, 0.6375637525500001, 0.6378985371093752,
                    0.6519728437500001, 0.6969405,          0.7609679121093752,
                    0.7770503437500003, 0.794317130859375,  0.812430254936266,
                    0.8128125000000002, 0.8170493968885845, 0.8372566381193804,
                    0.8574615003378379, 0.8574615399867362, 0.9121150080000001,
                    0.9279885027561691, 0.9310224867677299, 0.9805292181069962,
                    1.0588376953125003, 1.0982805120000005, 1.1205037551440329,
                    1.1429898425575686, 1.1431375762939453, 1.1537907480000003}},
    {.name       = "step down, 0.5 mm long",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 5.0E-4,
     .foreRadius = 0.025,
     .aftRadius  = 0.0125,
     .frictionCD = 0.003752998800959041,
     .forces     = {{{2.2222222222222231E-4, -1.5, -0.0, -0.0},
                     {2.2222222222222231E-4, -1.499328883371242, -0.05233645117015646,
                      -2.326064496451399E-4},
                     {2.2222222222222234E-4, -1.4905813617997794, -0.2601555253226698,
                      -0.0011562467792118661}}},
     .slowForces =
         {{{2.222222222222223E-4, -1.3156032331291783, -1.148080403399335, -0.005102579570663713},
           {2.2222222222222231E-4, -0.9506498517543197, -1.4932772952037983, -0.00663678797868355},
           {2.2222222222222231E-4, -1.3748714177905674, -0.9598413212803177, -0.004265961427912525},
           {2.222222222222223E-4, -1.3096698292060782, -1.142902531683893, -0.00507956680748397}}},
     .pressureCD = {0.09000000000000001,
                    0.090039,
                    0.09024375000000001,
                    0.098775,
                    0.12510000000000002,
                    0.16044375,
                    0.16897500000000001,
                    0.17799375000000003,
                    0.18730509750000005,
                    0.18750000000000003,
                    0.18731268731268738,
                    0.17857142857142858,
                    0.17045454545454547,
                    0.17045452995867913,
                    0.15000000000000005,
                    0.14423076923076925,
                    0.14312977099236643,
                    0.125,
                    0.09375000000000001,
                    0.07500000000000002,
                    0.0625,
                    0.04699248120300752,
                    0.04687500000000001,
                    0.03750000000000001}},
    {.name       = "step up, 1 mm long",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.001,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.0037619808611953362,
     .forces =
         {{{5.555555555555557E-4, 1.5, 0.0, 0.0},
           {5.555555555555557E-4, 1.5004284366716039, 0.052374832820941436, 5.819425868993493E-4},
           {5.555555555555557E-4, 1.4960257416042855, 0.261105748855847, 0.002901174987287189}}},
     .slowForces =
         {{{5.555555555555557E-4, 1.318993749656664, 1.1510391872367307, 0.012789324302630344},
           {5.555555555555557E-4, 0.9634892721454767, 1.5134454095924033, 0.01681606010658226},
           {5.555555555555557E-4, 1.3935215947322466, 0.9728616010287908, 0.010809573344764343},
           {5.555555555555557E-4, 1.330860557502864, 1.161394930667615, 0.012904388118529057}}},
     .pressureCD = {0.5961844197138317, 0.5961844197138818, 0.596184419713957,
                    0.5961865296732826, 0.5975024137174165, 0.6296596171624289,
                    0.6531034647670965, 0.6902269910514696, 0.7464637695450661,
                    0.7476114589020783, 0.7490177486132711, 0.8717926084290647,
                    1.1228939809073142, 1.1228945925709315, 1.9517510290686704,
                    2.0149714388619917, 2.0068964620020613, 1.8993261103213825,
                    1.7808009402719944, 1.7281262970266753, 1.6971443848194177,
                    1.6620179265211215, 1.6620179265211215, 1.6620179265211215}},
    {.name       = "step down, 1 mm long",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.001,
     .foreRadius = 0.025,
     .aftRadius  = 0.0125,
     .frictionCD = 0.003761980861195335,
     .forces     = {{{4.4444444444444463E-4, -1.5, -0.0, -0.0},
                     {4.4444444444444463E-4, -1.4989623656044546, -0.05232365728656147,
                      -4.6509917588054663E-4},
                     {4.4444444444444463E-4, -1.4887665685316107, -0.2598387841449441,
                      -0.0023096780812883926}}},
     .slowForces =
         {{{4.4444444444444463E-4, -1.3144730609533497, -1.1470941421202032, -0.01019639237440181},
           {4.4444444444444463E-4, -0.9463700449572673, -1.4865545904075965, -0.013213818581400862},
           {4.444444444444447E-4, -1.368654692143341, -0.9555012280308266, -0.008493344249162907},
           {4.4444444444444463E-4, -1.3026062531071496, -1.136738398689319,
            -0.010104341321682838}}},
     .pressureCD = {0.09000000000000001,
                    0.090039,
                    0.09024375000000001,
                    0.098775,
                    0.12510000000000002,
                    0.16044375,
                    0.16897500000000001,
                    0.17799375000000003,
                    0.18730509750000005,
                    0.18750000000000003,
                    0.18731268731268738,
                    0.17857142857142858,
                    0.17045454545454547,
                    0.17045452995867913,
                    0.15000000000000005,
                    0.14423076923076925,
                    0.14312977099236643,
                    0.125,
                    0.09375000000000001,
                    0.07500000000000002,
                    0.0625,
                    0.04699248120300752,
                    0.04687500000000001,
                    0.03750000000000001}},
    {.name       = "transition of no length",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.0,
     .foreRadius = 0.0125,
     .aftRadius  = 0.025,
     .frictionCD = 0.0,
     .forces     = {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .slowForces =
         {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "transition with equal radii",
     .body       = Body::TRANSITION,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.05,
     .foreRadius = 0.025,
     .aftRadius  = 0.025,
     .frictionCD = 0.020000000000000035,
     .forces =
         {{{0.0, 0.0, 0.0, 0.0},
           {0.024999999999999956, 0.04886903557163486, 0.0017058511459985151, 8.52925572999256E-4},
           {0.024999999999999956, 0.24197243575582802, 0.04223215703009653, 0.021116078515048223}}},
     .slowForces =
         {{{0.024999999999999956, 0.15068962344381115, 0.13150150388424706, 0.0657507519421234},
           {0.024999999999999956, 0.5706409062736474, 0.8963606394935562, 0.44818031974677724},
           {0.024999999999999956, 0.8288967529635167, 0.5786790999321373, 0.2893395499660681},
           {0.024999999999999956, 0.9418101465238198, 0.8218843992765441, 0.4109421996382713}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "nose cone of no length",
     .body       = Body::NOSE,
     .shape      = TransitionShape::OGIVE,
     .param      = 1.0,
     .clipped    = false,
     .length     = 0.0,
     .foreRadius = 0.0,
     .aftRadius  = 0.025,
     .frictionCD = 0.0,
     .forces     = {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .slowForces =
         {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "nose cone of no radius",
     .body       = Body::NOSE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.15,
     .foreRadius = 0.0,
     .aftRadius  = 0.0,
     .frictionCD = 0.0,
     .forces     = {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .slowForces =
         {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "body tube",
     .body       = Body::TUBE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.2,
     .foreRadius = 0.025,
     .aftRadius  = 0.025,
     .frictionCD = 0.08,
     .forces = {{{0.0, 0.0, 0.0, 0.0},
                 {0.09999999999999983, 0.19547614228653912, 0.006823404583994049,
                  0.013646809167988074},
                 {0.09999999999999983, 0.9678897430233104, 0.1689286281203858, 0.337857256240771}}},
     .slowForces =
         {{{0.09999999999999983, 0.6027584937752435, 0.5260060155369872, 1.0520120310739727},
           {0.09999999999999983, 2.2825636250945855, 3.5854425579742184, 7.170885115948424},
           {0.09999999999999983, 3.315587011854061, 2.314716399728545, 4.629432799457082},
           {0.09999999999999983, 3.767240586095273, 3.287537597106171, 6.575075194212331}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "body tube, short and wide",
     .body       = Body::TUBE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.01,
     .foreRadius = 0.05,
     .aftRadius  = 0.05,
     .frictionCD = 0.008,
     .forces     = {{{0.0, 0.0, 0.0, 0.0},
                     {0.005, 0.01954761422865391, 6.823404583994048E-4, 6.823404583994048E-5},
                     {0.005, 0.09678897430233102, 0.016892862812038577, 0.0016892862812038576}}},
     .slowForces = {{{0.005, 0.060275849377524345, 0.052600601553698716, 0.005260060155369871},
                     {0.005, 0.22825636250945852, 0.3585442557974218, 0.03585442557974218},
                     {0.005, 0.3315587011854061, 0.2314716399728545, 0.02314716399728545},
                     {0.005, 0.37672405860952723, 0.32875375971061704, 0.0328753759710617}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {.name       = "body tube of no length",
     .body       = Body::TUBE,
     .shape      = TransitionShape::CONICAL,
     .param      = 0.0,
     .clipped    = false,
     .length     = 0.0,
     .foreRadius = 0.025,
     .aftRadius  = 0.025,
     .frictionCD = 0.0,
     .forces     = {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .slowForces =
         {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}},
     .pressureCD = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    // PINS-END
}};
// NOLINTEND(modernize-use-std-numbers)

[[nodiscard]] std::unique_ptr<SymmetricComponent> makeTransition(const BodyCase& c)
{
    auto transition = std::make_unique<Transition>();
    transition->setShapeType(c.shape);
    transition->setShapeParameter(c.param);
    transition->setClipped(c.clipped);
    transition->setLength(c.length);
    transition->setForeRadius(c.foreRadius);
    transition->setAftRadius(c.aftRadius);
    return transition;
}

/// The component of a case, built as the probe built it.
[[nodiscard]] std::unique_ptr<SymmetricComponent> makeBody(const BodyCase& c)
{
    switch (c.body)
    {
        case Body::TUBE:
            return std::make_unique<BodyTube>(c.length, c.aftRadius);
        case Body::NOSE:
        {
            auto nose = std::make_unique<NoseCone>(c.shape, c.length, c.aftRadius);
            nose->setShapeParameter(c.param);
            return nose;
        }
        case Body::TRANSITION:
            return makeTransition(c);
    }
    return nullptr;
}

/// Fresh conditions with the reference length kRefLength.
[[nodiscard]] FlightConditions conditionsAt(double mach, double aoaDeg)
{
    FlightConditions conditions;
    conditions.setRefLength(kRefLength);
    conditions.setMach(mach);
    conditions.setAOA(MathUtil::deg2rad(aoaDeg));
    return conditions;
}

/// Compares a value with the one OpenRocket printed: within 1e-12 of the value itself, so a small
/// value is held as strictly as a large one and a zero must be a zero.
/// JavaValueDifferences::number(), which also allows an absolute 1e-15, only decides for NaN (NaN
/// with NaN) and the infinities.
void pin(JavaValueDifferences& differences, std::string_view field, double expected, double actual)
{
    const bool finite = std::isfinite(expected) && std::isfinite(actual);
    const bool within =
        std::abs(actual - expected) <= 1e-12 * std::max(std::abs(expected), std::abs(actual));
    if (finite && !within)
    {
        differences.problem(std::format("{}: expected {}, got {}", field, expected, actual));
    }
    else
    {
        differences.number(field, expected, actual);
    }
}

/// Compares the forces at @p mach and @p aoaDeg with Java's @p expected {CP x, CNa, CN, Cm}; the
/// other coefficients must be 0 and the supersonic warning the only one, above Mach 1.1.
void compareForces(JavaValueDifferences& differences, SymmetricComponentCalc& calc, double mach,
                   double aoaDeg, const std::array<double, 4>& expected)
{
    const FlightConditions conditions = conditionsAt(mach, aoaDeg);
    WarningSet             warnings;
    AerodynamicForces      forces;
    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);

    const std::string at = std::format("Mach {} AoA {}", mach, aoaDeg);
    pin(differences, at + " CP x", expected[0], forces.getCP().x);
    pin(differences, at + " CNa", expected[1], forces.getCP().weight);
    pin(differences, at + " CN", expected[2], forces.getCN());
    pin(differences, at + " Cm", expected[3], forces.getCm());
    if (forces.getCroll() != 0 || forces.getCrollDamp() != 0 || forces.getCrollForce() != 0 ||
        forces.getCside() != 0 || forces.getCyaw() != 0)
    {
        differences.problem(at + ": a roll, side force or yaw coefficient is not 0");
    }
    const bool supersonic = mach > 1.1;
    if (warnings.contains(Warning::kSupersonic) != supersonic ||
        warnings.size() != (supersonic ? 1U : 0U))
    {
        differences.problem(at + ": warnings " + warnings.toString());
    }
}

/// The forces of a case over the whole grid: every Mach number at each angle of attack, then
/// the low-speed points.
void compareAllForces(JavaValueDifferences& differences, SymmetricComponentCalc& calc,
                      const BodyCase& c)
{
    for (std::size_t a = 0; a < kAoas.size(); a++)
    {
        for (const double mach : kMachs)
        {
            compareForces(differences, calc, mach, kAoas.at(a), c.forces.at(a));
        }
    }
    for (std::size_t i = 0; i < kSlow.size(); i++)
    {
        compareForces(differences, calc, kSlow.at(i)[0], kSlow.at(i)[1], c.slowForces.at(i));
    }
}

/// The friction and pressure drag of a case.
void compareDrag(JavaValueDifferences& differences, SymmetricComponentCalc& calc, const BodyCase& c)
{
    WarningSet warnings;
    pin(differences, "friction CD", c.frictionCD,
        calc.calculateFrictionCD(conditionsAt(0.3, 0), kCf, warnings));
    for (std::size_t i = 0; i < kMachs.size(); i++)
    {
        const double           mach       = kMachs.at(i);
        const FlightConditions conditions = conditionsAt(mach, 0);
        pin(differences, std::format("pressure CD at Mach {}", mach), c.pressureCD.at(i),
            calc.calculatePressureCD(conditions,
                                     BarrowmanDragCalculator::calculateStagnationCD(mach),
                                     BarrowmanDragCalculator::calculateBaseCD(mach), warnings));
    }
    if (!warnings.empty())
    {
        differences.problem("the drag added warnings: " + warnings.toString());
    }
}

/// What differs between Java's values and the calculators' for a case; empty when nothing does.
[[nodiscard]] std::string differencesOf(const BodyCase& c)
{
    const std::unique_ptr<SymmetricComponent> body = makeBody(c);
    // As the probe: one calculator for the forces and one for the drag, each used for the whole
    // grid (so the values kept between calls are exercised).
    SymmetricComponentCalc forceCalc{*body};
    SymmetricComponentCalc dragCalc{*body};
    JavaValueDifferences   differences;
    compareAllForces(differences, forceCalc, c);
    compareDrag(differences, dragCalc, c);
    return differences.text();
}

TEST(SymmetricComponentCalc, MatchesOpenRocketForEveryShape)
{
    for (const BodyCase& c : kBodyCases)
    {
        EXPECT_EQ(differencesOf(c), "") << c.name;
    }
}

TEST(SymmetricComponentCalc, CoversEveryShapeAndBody)
{
    std::array<int, QtRocket::kAllTransitionShapes.size()> noses{};
    std::array<int, 3>                                     bodies{};
    for (const BodyCase& c : kBodyCases)
    {
        bodies.at(static_cast<std::size_t>(c.body))++;
        if (c.body == Body::NOSE)
        {
            noses.at(static_cast<std::size_t>(c.shape))++;
        }
    }
    for (const int count : noses)
    {
        EXPECT_GE(count, 3);
    }
    for (const int count : bodies)
    {
        EXPECT_GE(count, 3);
    }
}

// ============================================================ behaviour

/// The forces of @p calc at a Mach number and an angle of attack in radians.
[[nodiscard]] AerodynamicForces forcesAt(SymmetricComponentCalc& calc, double mach, double aoa,
                                         WarningSet& warnings)
{
    FlightConditions conditions;
    conditions.setRefLength(kRefLength);
    conditions.setMach(mach);
    conditions.setAOA(aoa);
    AerodynamicForces forces;
    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    return forces;
}

TEST(SymmetricComponentCalc, BodyLiftConstantIsOpenRockets)
{
    EXPECT_EQ(SymmetricComponentCalc::kBodyLiftK, 1.1);
}

TEST(SymmetricComponentCalc, BodyLiftIsReducedOnlyBelowMach005BeyondAnEighthTurn)
{
    // A body tube has the body lift alone. OpenRocket's values (the probe EdgeProbe.java).
    const BodyTube         tube{0.2, 0.025};
    SymmetricComponentCalc calc{tube};
    WarningSet             warnings;
    const double           quarterPi = std::numbers::pi / 4;
    const double           beyond    = std::nextafter(quarterPi, 1.0);

    // At pi / 4 exactly the lift is whole; one step beyond it is times (0.04 / 0.05)^2.
    const AerodynamicForces at = forcesAt(calc, 0.04, quarterPi, warnings);
    EXPECT_NEAR(at.getCP().weight, 3.56650566421029, 1e-12);
    EXPECT_NEAR(at.getCN(), 2.801126998417358, 1e-12);
    EXPECT_NEAR(at.getCm(), 5.602253996834706, 1e-11);
    const AerodynamicForces past = forcesAt(calc, 0.04, beyond, warnings);
    EXPECT_NEAR(past.getCP().weight, 2.2825636250945855, 1e-12);
    EXPECT_NEAR(past.getCN(), 1.7927212789871094, 1e-12);
    EXPECT_NEAR(past.getCm(), 3.5854425579742126, 1e-11);
    EXPECT_NEAR(past.getCP().weight / at.getCP().weight, 0.64, 1e-12);

    // At Mach 0.05 exactly the lift is whole; one step below it is reduced (by almost nothing).
    const double halfPi = std::numbers::pi / 2;
    EXPECT_NEAR(forcesAt(calc, 0.05, halfPi, warnings).getCP().weight, 3.5665056642102906, 1e-12);
    EXPECT_NEAR(forcesAt(calc, std::nextafter(0.05, 0.0), halfPi, warnings).getCP().weight,
                3.56650566421029, 1e-12);

    // At rest there is no lift at all, and the CP of no lift is the origin.
    const AerodynamicForces rest = forcesAt(calc, 0.0, halfPi, warnings);
    EXPECT_EQ(rest.getCP().weight, 0.0);
    EXPECT_EQ(rest.getCP().x, 0.0);
    EXPECT_EQ(rest.getCN(), 0.0);
    EXPECT_EQ(rest.getCm(), 0.0);
    EXPECT_TRUE(warnings.empty());
}

TEST(SymmetricComponentCalc, WarnsAboveMach11)
{
    const NoseCone         nose{TransitionShape::OGIVE, 0.15, 0.025};
    SymmetricComponentCalc calc{nose};
    WarningSet             warnings;
    static_cast<void>(forcesAt(calc, 1.1, 0.0, warnings));
    EXPECT_TRUE(warnings.empty());
    static_cast<void>(forcesAt(calc, std::nextafter(1.1, 2.0), 0.0, warnings));
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.contains(Warning::kSupersonic));
    // A set holds one warning of a kind.
    static_cast<void>(forcesAt(calc, 2.0, 0.0, warnings));
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.begin()->sources().empty());
}

TEST(SymmetricComponentCalc, CopiesTheGeometryWhenItIsMade)
{
    // OpenRocket's calculators are thrown away when the rocket changes; one that is kept goes on
    // with the geometry it copied, also for what it computes on first use (the slender-body
    // values and the pressure drag interpolator). The nose cone is in a rocket, whose change
    // events clear the areas and the volume the component itself caches (a component without a
    // rocket keeps them through its own setters, as in OpenRocket).
    const std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage&                   stage  = rocket->addChild(std::make_unique<AxialStage>());
    NoseCone&                     nose =
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::POWER, 0.15, 0.025));
    rocket->enableEvents();
    SymmetricComponentCalc kept{nose};
    SymmetricComponentCalc reference{nose};
    WarningSet             warnings;

    const AerodynamicForces before   = forcesAt(reference, 0.3, 0.1, warnings);
    const FlightConditions  mach2    = conditionsAt(2.0, 0);
    const double            pressure = reference.calculatePressureCD(mach2, 1.4, 0.125, warnings);
    const double            friction = reference.calculateFrictionCD(mach2, kCf, warnings);

    nose.setLength(0.3);
    nose.setAftRadius(0.04);
    nose.setShapeType(TransitionShape::CONICAL);

    const AerodynamicForces after = forcesAt(kept, 0.3, 0.1, warnings);
    EXPECT_EQ(after.getCP().x, before.getCP().x);
    EXPECT_EQ(after.getCP().weight, before.getCP().weight);
    EXPECT_EQ(after.getCN(), before.getCN());
    EXPECT_EQ(after.getCm(), before.getCm());
    EXPECT_EQ(kept.calculatePressureCD(mach2, 1.4, 0.125, warnings), pressure);
    EXPECT_EQ(kept.calculateFrictionCD(mach2, kCf, warnings), friction);

    // A new calculator sees the changed nose cone.
    SymmetricComponentCalc renewed{nose};
    EXPECT_NE(forcesAt(renewed, 0.3, 0.1, warnings).getCP().x, before.getCP().x);
    EXPECT_NE(renewed.calculatePressureCD(mach2, 1.4, 0.125, warnings), pressure);
    EXPECT_NE(renewed.calculateFrictionCD(mach2, kCf, warnings), friction);
}

TEST(SymmetricComponentCalc, PressureDragOfAStepFollowsTheGivenCoefficients)
{
    // Shorter than 1 mm: the stagnation drag of a step up and the base drag of a step down, on
    // the frontal area pi (r2^2 - r1^2).
    const FlightConditions conditions = conditionsAt(0.3, 0);
    const double frontalArea = std::abs(std::numbers::pi * ((0.01 * 0.01) - (0.02 * 0.02)));
    WarningSet   warnings;

    Transition up;
    up.setLength(0.0005);
    up.setForeRadius(0.01);
    up.setAftRadius(0.02);
    SymmetricComponentCalc upCalc{up};
    EXPECT_EQ(upCalc.calculatePressureCD(conditions, 0.9, 0.2, warnings),
              0.9 * frontalArea / conditions.getRefArea());

    Transition down;
    down.setLength(0.0005);
    down.setForeRadius(0.02);
    down.setAftRadius(0.01);
    SymmetricComponentCalc downCalc{down};
    EXPECT_EQ(downCalc.calculatePressureCD(conditions, 0.9, 0.2, warnings),
              0.2 * std::abs(std::numbers::pi * ((0.02 * 0.02) - (0.01 * 0.01))) /
                  conditions.getRefArea());
    EXPECT_TRUE(warnings.empty());
}

TEST(SymmetricComponentCalc, BoatTailDragFallsLinearlyBetweenFineness1And3)
{
    // The radii differ by 1/64, so the fineness ratio is 32 * length.
    const FlightConditions conditions = conditionsAt(0.3, 0);
    WarningSet             warnings;
    const auto             drag = [&conditions, &warnings](double length) {
        Transition tail;
        tail.setLength(length);
        tail.setForeRadius(0.03125);
        tail.setAftRadius(0.015625);
        SymmetricComponentCalc calc{tail};
        return calc.calculatePressureCD(conditions, 0.9, 0.2, warnings);
    };
    const double full = drag(0.03125);  // fineness 1
    EXPECT_GT(full, 0.0);
    EXPECT_EQ(drag(0.015625), full);            // fineness 0.5
    EXPECT_EQ(drag(0.0625), full * 1 / 2);      // fineness 2: (3 - 2) / 2
    EXPECT_EQ(drag(0.078125), full * 0.5 / 2);  // fineness 2.5
    EXPECT_EQ(drag(0.09375), 0.0);              // fineness 3
    EXPECT_EQ(drag(0.5), 0.0);
}

/// A symmetric component that is neither a body tube nor a transition, which OpenRocket's
/// calculator rejects.
class OtherBody final : public SymmetricComponent
{
public:
    using RocketComponent::isCompatible;
    using SymmetricComponent::getInnerRadius;
    using SymmetricComponent::getRadius;

    OtherBody() { m_length = 0.1; }

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::BODY_TUBE; }
    [[nodiscard]] double        getRadius(double /*x*/) const override { return 0.02; }
    [[nodiscard]] double        getInnerRadius(double /*x*/) const override { return 0.018; }
    [[nodiscard]] double        getForeRadius() const override { return 0.02; }
    [[nodiscard]] bool          isForeRadiusAutomatic() const override { return false; }
    [[nodiscard]] double        getAftRadius() const override { return 0.02; }
    [[nodiscard]] bool          isAftRadiusAutomatic() const override { return false; }
    [[nodiscard]] double        getFrontAutoRadius() const override { return 0.02; }
    [[nodiscard]] double        getRearAutoRadius() const override { return 0.02; }
    [[nodiscard]] bool          usesPreviousCompAutomatic() const override { return false; }
    [[nodiscard]] bool          usesNextCompAutomatic() const override { return false; }
    [[nodiscard]] bool isCompatible(ComponentKind /*kind*/) const override { return false; }

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override
    {
        return std::make_unique<OtherBody>();
    }
};

TEST(SymmetricComponentCalc, AnotherKindOfBodyIsABug)
{
    // Java: UnsupportedOperationException("Unknown component type ..."). The class decides, as
    // Java's instanceof, not the kind the component reports.
    const OtherBody other;
    EXPECT_THROW(SymmetricComponentCalc{other}, BugError);
}

TEST(SymmetricComponentCalc, BodyTubeWithANaNRadius)
{
    // NaN radii never compare equal, so the tube is taken for neither a cylinder nor a boat
    // tail: its forces are NaN and its pressure drag ends at the shape a body tube does not
    // have (Java: NullPointerException, then IllegalStateException on the next call).
    const BodyTube         tube{0.2, kNaN};
    SymmetricComponentCalc calc{tube};
    WarningSet             warnings;
    const FlightConditions conditions = conditionsAt(0.3, 2);

    AerodynamicForces forces;
    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_TRUE(std::isnan(forces.getCP().weight));
    EXPECT_TRUE(std::isnan(forces.getCN()));
    EXPECT_TRUE(std::isnan(forces.getCm()));

    EXPECT_THROW(static_cast<void>(calc.calculatePressureCD(conditions, 0.85, 0.12, warnings)),
                 BugError);
    EXPECT_THROW(static_cast<void>(calc.calculatePressureCD(conditions, 0.85, 0.12, warnings)),
                 BugError);

    // Shorter than 1 mm it is a step down of no frontal area.
    const BodyTube         disk{0.0005, kNaN};
    SymmetricComponentCalc diskCalc{disk};
    EXPECT_EQ(diskCalc.calculatePressureCD(conditions, 0.85, 0.12, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
