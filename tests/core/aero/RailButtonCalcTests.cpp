#include "QtRocket/aero/barrowman/RailButtonCalc.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::LaunchLug;
using QtRocket::ModId;
using QtRocket::NoseCone;
using QtRocket::RailButton;
using QtRocket::RailButtonCalc;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketComponentCalc;
using QtRocket::Transformation;
using QtRocket::TransitionShape;
using QtRocket::WarningSet;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::TestEstesAlphaIII;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ================================================================== RailButtonCalcTest.java

// RailButtonCalcTest.testRailButtons. Java makes a BarrowmanCalculator only to call its static
// calculateStagnationCD(), which forwards to BarrowmanDragCalculator's: that one is called here.
TEST(RailButtonCalc, RailButtons)
{
    /// RailButtonCalcTest.EPSILON.
    const double epsilon = 0.0001;

    const TestEstesAlphaIII    alpha;
    Rocket&                    rocket = *alpha.rocket;
    const FlightConfiguration& config = rocket.getSelectedConfiguration();

    // Get the body tube...
    BodyTube& tube = *alpha.body;
    ASSERT_EQ(&rocket.getChild(0).getChild(1), &tube);

    // Replace the launch lug with a (single) railbutton
    const LaunchLug* lug = alpha.lug;
    ASSERT_EQ(&tube.getChild(1), lug);
    // As in Java, this asks the rocket, whose child the lug is not: nothing is removed.
    const std::unique_ptr<RocketComponent> removed = rocket.removeChild(lug);
    EXPECT_EQ(removed, nullptr);

    RailButton& button = tube.addChild(std::make_unique<RailButton>());

    // Button parameters from Binder Design standard 1010
    button.setOuterDiameter(0.011);
    button.setInnerDiameter(0.006);

    button.setBaseHeight(0.002);
    button.setFlangeHeight(0.002);
    button.setTotalHeight(0.008);

    button.setAxialMethod(AxialMethod::ABSOLUTE);
    button.setAxialOffset(1.0);

    // Set up flight conditions
    FlightConditions conditions{config};
    conditions.setMach(1.0);

    RailButtonCalc calcObj{button};

    // Calculate effective CD for rail button
    // Boundary layer height
    const double rex =
        RocketComponentCalc::calculateReynoldsNumber(1.0, conditions);  // of button location
    const double del = 0.37 * 1.0 / MathUtil::javaPow(rex, 0.2);        // Boundary layer height

    // Interpolate velocity at midpoint of railbutton
    const double mach = MathUtil::map(0.008 / 2.0, 0, del, 0, 1.0);

    // Interpolate to get CD
    const double cd = MathUtil::map(mach, 0.2, 0.3, 1.22, 1.25);

    // Reference area of rail button
    const double outerArea = button.getTotalHeight() * button.getOuterDiameter();
    const double notchArea =
        (button.getOuterDiameter() - button.getInnerDiameter()) * button.getInnerHeight();
    const double refArea = outerArea - notchArea;

    // Get "effective" CD
    const double calccd = cd * MathUtil::pow2(mach) *
                          BarrowmanDragCalculator::calculateStagnationCD(conditions.getMach()) *
                          refArea / conditions.getRefArea();

    // Now compare with value from RailButtonCalc
    WarningSet   warnings;
    const double testcd = calcObj.calculatePressureCD(
        conditions, BarrowmanDragCalculator::calculateStagnationCD(conditions.getMach()), 0,
        warnings);

    EXPECT_NEAR(testcd, calccd, epsilon) << "Calculated rail button CD incorrect";
}

// ================================================================== pins against OpenRocket

// Every value below was printed by OpenRocket's own RailButtonCalc (JDK 17, the probe
// ButtonProbe.java of the port's scratch directory) for a button built the same way: on a body
// tube 1 m long of radius 0.05 m behind an ogive nose cone 0.1 m long, in a stage of a rocket
// (or on its own), in fresh flight conditions with the reference length kRefLength.

constexpr double kRefLength = 0.1;

/// The Mach numbers: at rest, below, at and just above MathUtil::kEpsilon, the usual ones, and
/// two for the end of the drag table: at Mach 30 the mean Mach number over a button is in the
/// table's last segment (2.8 to 100), at Mach 150 it is beyond the table for every button that
/// is not deep inside the boundary layer, and the drag is NaN (MathUtil::interpolate() outside
/// its domain).
constexpr std::array<double, 16> kMachs{0.0, 1.0E-9, 1.0E-8, 2.0E-8, 0.05, 0.3, 0.6,  0.9,
                                        1.0, 1.1,    1.5,    2.0,    3.0,  5.0, 30.0, 150.0};

struct ButtonCase
{
    std::string_view name;
    bool             onBody;  ///< on the body tube; else without a parent
    bool             custom;  ///< RailButton(od, id, ht, flange, base); else RailButton()
    double           outerDiameter;
    double           innerDiameter;
    double           totalHeight;
    double           flangeHeight;
    double           baseHeight;
    AxialMethod      axialMethod;
    double           axialOffset;
    int              instanceCount;
    double           instanceSeparation;
    /// calculatePressureCD() at each of kMachs, with the stagnation drag coefficient of that
    /// Mach number.
    std::array<double, 16> pressureCD;
};

// The cases cover buttons taller than the boundary layer at their position ("near the front",
// "tall button"), within it ("low button", "without a notch"), one that leaves it as the Mach
// number rises ("in the middle"), and instances on both sides of it ("three buttons").
// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<ButtonCase, 8> kButtonCases{{
    // PINS-BEGIN (ButtonProbe.java)
    {.name               = "default button in the middle",
     .onBody             = true,
     .custom             = false,
     .outerDiameter      = 0.0097,
     .innerDiameter      = 0.008,
     .totalHeight        = 0.0097,
     .flangeHeight       = 0.002,
     .baseHeight         = 0.002,
     .axialMethod        = AxialMethod::MIDDLE,
     .axialOffset        = 0.0,
     .instanceCount      = 1,
     .instanceSeparation = 0.0582,
     .pressureCD         = {8.025686993999977E-6, 8.025686993999977E-6, 8.025686993999977E-6,
                            3.0992234659111807E-6, 0.0011253281160152446, 0.0023789413213828403,
                            0.003459225077057728, 0.004940942231035788, 0.0055858201765529225,
                            0.006333007341488097, 0.009979837417865206, 0.010999416918803807,
                            0.010035328491181926, 0.01044014026650084, 0.013688423586448336, kNaN}},
    {.name               = "default button near the front",
     .onBody             = true,
     .custom             = false,
     .outerDiameter      = 0.0097,
     .innerDiameter      = 0.008,
     .totalHeight        = 0.0097,
     .flangeHeight       = 0.002,
     .baseHeight         = 0.002,
     .axialMethod        = AxialMethod::TOP,
     .axialOffset        = 0.01,
     .instanceCount      = 1,
     .instanceSeparation = 0.0582,
     .pressureCD = {8.025686993999977E-6, 8.025686993999977E-6, 8.025686993999977E-6,
                    4.678068707448143E-5, 0.007024840112244648, 0.008539405176584776,
                    0.010974716967463641, 0.015461671106346686, 0.017658045382884253,
                    0.020201039905191225, 0.019865309431933265, 0.018270262146112066,
                    0.017867158926911444, 0.018608813020573947, 0.019913774801734323, kNaN}},
    {.name               = "low button at the aft end",
     .onBody             = true,
     .custom             = true,
     .outerDiameter      = 0.008,
     .innerDiameter      = 0.005,
     .totalHeight        = 0.003,
     .flangeHeight       = 0.001,
     .baseHeight         = 0.001,
     .axialMethod        = AxialMethod::BOTTOM,
     .axialOffset        = 0.0,
     .instanceCount      = 1,
     .instanceSeparation = 0.016,
     .pressureCD         = {1.9969126406871983E-6, 1.9969126406871983E-6, 1.9969126406871983E-6,
                            2.7966781221880848E-8, 1.0143788890910284E-5, 2.127033754798944E-5,
                            3.008686575046275E-5, 3.957506472945748E-5, 4.322537452939135E-5,
                            4.74244224356616E-5, 6.167830599417966E-5, 7.585797415632396E-5,
                            1.0038690519803623E-4, 1.6486187003160268E-4, 2.66911874519674E-4,
                            5.083317494441877E-4}},
    {.name               = "three buttons along the body",
     .onBody             = true,
     .custom             = false,
     .outerDiameter      = 0.0097,
     .innerDiameter      = 0.008,
     .totalHeight        = 0.0097,
     .flangeHeight       = 0.002,
     .baseHeight         = 0.002,
     .axialMethod        = AxialMethod::TOP,
     .axialOffset        = 0.01,
     .instanceCount      = 3,
     .instanceSeparation = 0.45,
     .pressureCD = {8.025686993999977E-6, 8.025686993999977E-6, 8.025686993999977E-6,
                    1.7196222510705478E-5, 0.0029234969793249213, 0.004075897978246506,
                    0.005423491661798222, 0.007635252419439804, 0.00867318584210657,
                    0.009885243964595163, 0.011523452546181716, 0.011786054961191735,
                    0.011514032398767832, 0.011936352753742966, 0.014685253897392682, kNaN}},
    {.name               = "two low buttons",
     .onBody             = true,
     .custom             = true,
     .outerDiameter      = 0.008,
     .innerDiameter      = 0.005,
     .totalHeight        = 0.003,
     .flangeHeight       = 0.001,
     .baseHeight         = 0.001,
     .axialMethod        = AxialMethod::TOP,
     .axialOffset        = 0.2,
     .instanceCount      = 2,
     .instanceSeparation = 0.6,
     .pressureCD = {1.9969126406871983E-6, 1.9969126406871983E-6, 1.9969126406871983E-6,
                    1.310788719774938E-7, 4.7562547935376796E-5, 1.0003670300309664E-4,
                    1.4212942899242013E-4, 1.8986403810462748E-4, 2.0917843767559655E-4,
                    2.3225957896208591E-4, 3.282284948875599E-4, 4.5381996733341053E-4,
                    6.612573270093761E-4, 6.779348950609793E-4, 0.0011729987942405446, kNaN}},
    {.name               = "button without a notch",
     .onBody             = true,
     .custom             = true,
     .outerDiameter      = 0.01,
     .innerDiameter      = 0.01,
     .totalHeight        = 0.006,
     .flangeHeight       = 0.002,
     .baseHeight         = 0.002,
     .axialMethod        = AxialMethod::MIDDLE,
     .axialOffset        = 0.0,
     .instanceCount      = 1,
     .instanceSeparation = 0.02,
     .pressureCD = {5.705464687677709E-6, 5.705464687677709E-6, 5.705464687677709E-6,
                    8.429867095959586E-7, 3.059329788416978E-4, 6.442760075408607E-4,
                    9.170546772409807E-4, 0.0012412543997836949, 0.0013772982420835731,
                    0.0015376136849762443, 0.0022686931041619052, 0.0033537188061006605,
                    0.004034144485355062, 0.004003048605218375, 0.006684627592646061, kNaN}},
    {.name               = "tall button",
     .onBody             = true,
     .custom             = true,
     .outerDiameter      = 0.012,
     .innerDiameter      = 0.006,
     .totalHeight        = 0.03,
     .flangeHeight       = 0.004,
     .baseHeight         = 0.004,
     .axialMethod        = AxialMethod::BOTTOM,
     .axialOffset        = -0.05,
     .instanceCount      = 1,
     .instanceSeparation = 0.024,
     .pressureCD         = {2.1680765813175294E-5, 2.1680765813175294E-5, 2.1680765813175294E-5,
                            3.2710219245503104E-5, 0.01086783854784873, 0.016200620102187455,
                            0.02110377398702752, 0.029169391680163973, 0.033525826200545776,
                            0.03857603505993386, 0.04614051754684337, 0.04036550251219355,
                            0.040766085710500374, 0.04218002599180091, 0.04781737567804386, kNaN}},
    {.name               = "button without a parent",
     .onBody             = false,
     .custom             = false,
     .outerDiameter      = 0.0097,
     .innerDiameter      = 0.008,
     .totalHeight        = 0.0097,
     .flangeHeight       = 0.002,
     .baseHeight         = 0.002,
     .axialMethod        = AxialMethod::MIDDLE,
     .axialOffset        = 0.0,
     .instanceCount      = 1,
     .instanceSeparation = 0.0582,
     .pressureCD = {8.025686993999977E-6, 8.025686993999977E-6, 8.025686993999977E-6, kNaN, kNaN,
                    kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN}},
    // PINS-END
}};
// NOLINTEND(modernize-use-std-numbers)

/// A rail button built as the probe built it, with what owns it.
struct Button
{
    std::unique_ptr<Rocket>     rocket;  ///< with a stage, a nose cone and a body tube
    std::unique_ptr<RailButton> orphan;  ///< the button when it has no parent
    RailButton*                 button{nullptr};
};

[[nodiscard]] std::unique_ptr<RailButton> newButton(const ButtonCase& c)
{
    if (c.custom)
    {
        return std::make_unique<RailButton>(c.outerDiameter, c.innerDiameter, c.totalHeight,
                                            c.flangeHeight, c.baseHeight);
    }
    return std::make_unique<RailButton>();
}

[[nodiscard]] Button makeButton(const ButtonCase& c)
{
    Button made;
    if (c.onBody)
    {
        made.rocket       = std::make_unique<Rocket>();
        AxialStage& stage = made.rocket->addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.05));
        BodyTube& body = stage.addChild(std::make_unique<BodyTube>(1.0, 0.05));
        made.button    = &body.addChild(newButton(c));
    }
    else
    {
        made.orphan = newButton(c);
        made.button = made.orphan.get();
    }
    made.button->setAxialMethod(c.axialMethod);
    made.button->setAxialOffset(c.axialOffset);
    made.button->setInstanceCount(c.instanceCount);
    made.button->setInstanceSeparation(c.instanceSeparation);
    if (made.rocket != nullptr)
    {
        made.rocket->enableEvents();
    }
    return made;
}

/// Fresh conditions with the reference length kRefLength.
[[nodiscard]] FlightConditions conditionsAt(double mach, double aoaDeg = 0)
{
    FlightConditions conditions;
    conditions.setRefLength(kRefLength);
    conditions.setMach(mach);
    conditions.setAOA(MathUtil::deg2rad(aoaDeg));
    return conditions;
}

/// What differs between Java's values and the calculator's for a case; empty when nothing does.
[[nodiscard]] std::string differencesOf(const ButtonCase& c)
{
    const Button         made = makeButton(c);
    RailButtonCalc       calc{*made.button};
    JavaValueDifferences differences;
    differences.pinned("outer diameter", c.outerDiameter, made.button->getOuterDiameter());
    differences.pinned("inner diameter", c.innerDiameter, made.button->getInnerDiameter());
    differences.pinned("total height", c.totalHeight, made.button->getTotalHeight());
    differences.pinned("flange height", c.flangeHeight, made.button->getFlangeHeight());
    differences.pinned("base height", c.baseHeight, made.button->getBaseHeight());

    WarningSet warnings;
    for (std::size_t i = 0; i < kMachs.size(); i++)
    {
        const double mach = kMachs.at(i);
        const double stag = BarrowmanDragCalculator::calculateStagnationCD(mach);
        differences.pinned(
            std::format("pressure CD at Mach {}", mach), c.pressureCD.at(i),
            calc.calculatePressureCD(conditionsAt(mach), stag,
                                     BarrowmanDragCalculator::calculateBaseCD(mach), warnings));
        // Neither the base drag coefficient nor the angle of attack matter.
        differences.pinned(std::format("pressure CD at Mach {} and 10 degrees", mach),
                           c.pressureCD.at(i),
                           calc.calculatePressureCD(conditionsAt(mach, 10), stag, 0.7, warnings));
    }
    if (!warnings.empty())
    {
        differences.problem("warnings: " + warnings.toString());
    }
    return differences.text();
}

TEST(RailButtonCalc, MatchesOpenRocketForEveryButton)
{
    for (const ButtonCase& c : kButtonCases)
    {
        EXPECT_EQ(differencesOf(c), "") << c.name;
    }
}

// ================================================================== behaviour

/// A default rail button on a body tube 1 m long of radius 0.05 m that is the first component
/// of its stage, at @p offset from the top of the tube.
[[nodiscard]] Button buttonOnTube(double offset)
{
    Button made;
    made.rocket       = std::make_unique<Rocket>();
    AxialStage& stage = made.rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>(1.0, 0.05));
    made.button       = &body.addChild(std::make_unique<RailButton>());
    made.button->setAxialMethod(AxialMethod::TOP);
    made.button->setAxialOffset(offset);
    made.rocket->enableEvents();
    return made;
}

/// The pressure drag of @p calc at Mach 0.3 with the stagnation drag coefficient of that Mach
/// number.
[[nodiscard]] double pressureAtMach03(RailButtonCalc& calc)
{
    WarningSet warnings;
    return calc.calculatePressureCD(
        conditionsAt(0.3), BarrowmanDragCalculator::calculateStagnationCD(0.3), 0, warnings);
}

TEST(RailButtonCalc, AddsNoForcesAndNoFrictionDrag)
{
    const RailButton     button;
    RailButtonCalc       calc{button};
    RocketComponentCalc& base = calc;

    FlightConditions conditions;
    conditions.setMach(1.2);
    conditions.setAOA(0.1);
    WarningSet        warnings;
    AerodynamicForces forces;
    forces.setCN(0.5);
    forces.setCP(Coordinate{0.3, 0, 0, 2});
    const ModId before = forces.modId();

    base.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_EQ(forces.modId(), before);  // untouched
    EXPECT_EQ(forces.getCN(), 0.5);
    EXPECT_TRUE(std::isnan(forces.getCm()));

    EXPECT_EQ(base.calculateFrictionCD(conditions, 0.004, warnings), 0.0);
    EXPECT_EQ(base.calculateComponentBaseCD(conditions, 0.12, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

TEST(RailButtonCalc, AtRestTheDragIsOpenRocketsConstant)
{
    // At a Mach number up to MathUtil::kEpsilon the mean is the constant 8.786395072609939E-4,
    // on the button's side area (the outline less the notch), wherever the button is.
    const RailButton button;  // 9.7 mm wide and high, a notch 0.85 mm deep and 5.7 mm high
    RailButtonCalc   calc{button};
    WarningSet       warnings;
    const double     refArea = (0.0097 * 0.0097) - ((0.0097 - 0.008) * (0.0097 - 0.002 - 0.002));

    const FlightConditions rest = conditionsAt(0.0);
    EXPECT_EQ(calc.calculatePressureCD(rest, 0.85, 0.12, warnings),
              8.786395072609939E-4 * 0.85 * refArea / rest.getRefArea());
    const FlightConditions limit = conditionsAt(MathUtil::kEpsilon);
    EXPECT_EQ(limit.getMach(), MathUtil::kEpsilon);
    EXPECT_EQ(calc.calculatePressureCD(limit, 0.85, 0.12, warnings),
              8.786395072609939E-4 * 0.85 * refArea / limit.getRefArea());
    // The drag is proportional to the stagnation drag coefficient.
    EXPECT_EQ(calc.calculatePressureCD(rest, 0.0, 0.12, warnings), 0.0);
}

TEST(RailButtonCalc, ReadsTheButtonAtEveryCall)
{
    // OpenRocket's values (the probe EdgeProbe.java): the button is moved, made taller and
    // repeated after the calculator was made.
    const Button   made = buttonOnTube(0);
    RailButtonCalc calc{*made.button};

    // At the very front there is no boundary layer to speak of: 0.37 * 0 / 0^0.2 is NaN.
    EXPECT_TRUE(std::isnan(pressureAtMach03(calc)));
    // Ahead of the rocket the Reynolds number is negative and its power NaN.
    made.button->setAxialOffset(-0.1);
    EXPECT_TRUE(std::isnan(pressureAtMach03(calc)));

    made.button->setAxialOffset(0.5);
    EXPECT_NEAR(pressureAtMach03(calc), 0.0031698758140496926, 0.0031698758140496926 * 1e-12);
    made.button->setTotalHeight(0.02);
    EXPECT_NEAR(pressureAtMach03(calc), 0.013499802752650529, 0.013499802752650529 * 1e-12);
    made.button->setInstanceCount(2);
    EXPECT_NEAR(pressureAtMach03(calc), 0.013126225529766855, 0.013126225529766855 * 1e-12);
}

TEST(RailButtonCalc, DragIsTheMeanOverTheInstances)
{
    // Two instances 0.3 m apart have the mean drag of single buttons at the two positions.
    const Button front = buttonOnTube(0.2);
    const Button rear  = buttonOnTube(0.5);
    const Button both  = buttonOnTube(0.2);
    both.button->setInstanceCount(2);
    both.button->setInstanceSeparation(0.3);
    RailButtonCalc frontCalc{*front.button};
    RailButtonCalc rearCalc{*rear.button};
    RailButtonCalc bothCalc{*both.button};

    const double frontCD = pressureAtMach03(frontCalc);
    const double rearCD  = pressureAtMach03(rearCalc);
    EXPECT_GT(frontCD, rearCD);  // the boundary layer is thinner at the front
    EXPECT_GT(rearCD, 0.0);
    EXPECT_NEAR(pressureAtMach03(bothCalc), (frontCD + rearCD) / 2, frontCD * 1e-12);
}

/// A rail button of the given inner diameter and total height (10 mm wide, without a flange or
/// a base) at the top of a body tube 1 m long of radius 0.05 m, @p offset from its front end. The
/// tube is behind a conical nose cone @p noseLength long, or the first component of its stage
/// when that is NaN.
[[nodiscard]] Button flatButton(double noseLength, double innerDiameter, double totalHeight,
                                double offset)
{
    Button made;
    made.rocket       = std::make_unique<Rocket>();
    AxialStage& stage = made.rocket->addChild(std::make_unique<AxialStage>());
    if (!std::isnan(noseLength))
    {
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::CONICAL, noseLength, 0.05));
    }
    BodyTube& body = stage.addChild(std::make_unique<BodyTube>(1.0, 0.05));
    made.button =
        &body.addChild(std::make_unique<RailButton>(0.01, innerDiameter, totalHeight, 0, 0));
    made.button->setAxialMethod(AxialMethod::TOP);
    made.button->setAxialOffset(offset);
    made.rocket->enableEvents();
    return made;
}

/// The pressure drag of @p made's button at @p mach for the stagnation drag coefficient 1.5,
/// from a calculator of its own.
[[nodiscard]] double pressureAt(const Button& made, double mach)
{
    RailButtonCalc calc{*made.button};
    WarningSet     warnings;
    return calc.calculatePressureCD(conditionsAt(mach), 1.5, 0.1, warnings);
}

TEST(RailButtonCalc, ButtonInABoundaryLayerOfNoThicknessIsABug)
{
    // A button that does not reach beyond the boundary layer has the Mach number at half its
    // height, MathUtil::map() over the layer's thickness, which throws when the thickness is
    // below MathUtil::kEpsilon / 2 = 5e-9 m (Java: IllegalArgumentException). OpenRocket's
    // results (the probe FixProbe.java).

    // At an ordinary Mach number the layer is that thin only within some 1e-8 m of the front of
    // the rocket, where the length of a component can put a button (an axial offset that small
    // is taken for 0): one nanometre behind the tip the layer is 1.005e-9 m thick at Mach 0.3.
    const double   stag03 = BarrowmanDragCalculator::calculateStagnationCD(0.3);
    WarningSet     warnings;
    const Button   flat = flatButton(1e-9, 0.008, 0, 0);
    RailButtonCalc flatCalc{*flat.button};
    EXPECT_THROW(
        static_cast<void>(flatCalc.calculatePressureCD(conditionsAt(0.3), stag03, 0, warnings)),
        BugError);
    const Button   low = flatButton(1e-9, 0.008, 1e-10, 0);
    RailButtonCalc lowCalc{*low.button};
    EXPECT_THROW(
        static_cast<void>(lowCalc.calculatePressureCD(conditionsAt(0.3), stag03, 0, warnings)),
        BugError);
    // A button that reaches beyond the layer is not mapped.
    const Button   tall = flatButton(1e-9, 0.008, 0.001, 0);
    RailButtonCalc tallCalc{*tall.button};
    EXPECT_NEAR(tallCalc.calculatePressureCD(conditionsAt(0.3), stag03, 0, warnings),
                0.0011068223229105843, 0.0011068223229105843 * 1e-12);
    // Ten nanometres behind the tip the layer is 6.34e-9 m thick: no exception.
    const Button   behind = flatButton(1e-8, 0.008, 1e-10, 0);
    RailButtonCalc behindCalc{*behind.button};
    EXPECT_NEAR(behindCalc.calculatePressureCD(conditionsAt(0.3), stag03, 0, warnings),
                6.603441894956084E-15, 6.603441894956084E-15 * 1e-12);
    EXPECT_TRUE(warnings.empty());
}

TEST(RailButtonCalc, FurtherAftTheBoundaryLayerOnlyVanishesAtAbsurdMachNumbers)
{
    // 2 micrometres from the front the layer is 8.7e-9 m thick at Mach 1e8 and 3.5e-9 m at Mach
    // 1e10; 1 mm from the front it takes Mach 1e20. OpenRocket's results (the probe
    // FixProbe.java).
    const Button atTwoMicrometres = flatButton(kNaN, 0.005, 0, 2e-6);
    EXPECT_EQ(pressureAt(atTwoMicrometres, 1e8), 0.0);
    EXPECT_THROW(static_cast<void>(pressureAt(atTwoMicrometres, 1e10)), BugError);
    EXPECT_THROW(static_cast<void>(pressureAt(atTwoMicrometres, 1e30)), BugError);

    const Button atOneMillimetre = flatButton(kNaN, 0.005, 0, 0.001);
    EXPECT_EQ(pressureAt(atOneMillimetre, 1e14), 0.0);
    EXPECT_THROW(static_cast<void>(pressureAt(atOneMillimetre, 1e20)), BugError);

    // A button 1e-12 m high is within the layer (NaN: its mean Mach number is beyond the drag
    // table), in a layer of no thickness (the exception), or beyond the layer (NaN again).
    const Button thin = flatButton(kNaN, 0.005, 1e-12, 2e-6);
    EXPECT_TRUE(std::isnan(pressureAt(thin, 1e8)));
    EXPECT_THROW(static_cast<void>(pressureAt(thin, 1e10)), BugError);
    EXPECT_TRUE(std::isnan(pressureAt(thin, 1e30)));
}

TEST(RailButtonCalc, DragTableEndsAtMach100)
{
    // A button 30 mm high half-way along the body reaches far beyond the boundary layer, so the
    // mean Mach number over it is a little below the rocket's: 94.3 at Mach 99 and 98.1 at Mach
    // 103, where the drag coefficient is still the table's last value 1.33, and 114.5 at Mach
    // 120, beyond the table. OpenRocket's results (the probe FixProbe.java).
    Button made;
    made.rocket       = std::make_unique<Rocket>();
    AxialStage& stage = made.rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>(1.0, 0.05));
    made.button = &body.addChild(std::make_unique<RailButton>(0.012, 0.006, 0.03, 0.004, 0.004));
    made.button->setAxialMethod(AxialMethod::TOP);
    made.button->setAxialOffset(0.5);
    made.rocket->enableEvents();

    EXPECT_NEAR(pressureAt(made, 2.8), 0.04861760380024849, 0.04861760380024849 * 1e-12);
    EXPECT_NEAR(pressureAt(made, 30), 0.05109502436213796, 0.05109502436213796 * 1e-12);
    EXPECT_NEAR(pressureAt(made, 99), 0.05250788060731237, 0.05250788060731237 * 1e-12);
    EXPECT_NEAR(pressureAt(made, 103), 0.052549505479435855, 0.052549505479435855 * 1e-12);
    EXPECT_TRUE(std::isnan(pressureAt(made, 120)));
    EXPECT_TRUE(std::isnan(pressureAt(made, 150)));
}

TEST(RailButtonCalc, ButtonOfNoHeightHasNoDrag)
{
    // A button without height in the boundary layer: the mean Mach number over it is 0 and so
    // is its side area (OpenRocket gives 0 too: the probe EdgeProbe.java).
    const Button made = buttonOnTube(0.5);
    made.button->setFlangeHeight(0);
    made.button->setBaseHeight(0);
    made.button->setTotalHeight(0);
    ASSERT_EQ(made.button->getTotalHeight(), 0.0);
    RailButtonCalc calc{*made.button};
    EXPECT_EQ(pressureAtMach03(calc), 0.0);
}

}  // namespace
