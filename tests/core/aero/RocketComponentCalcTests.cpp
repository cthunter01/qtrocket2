#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AtmosphericConditions;
using QtRocket::FlightConditions;
using QtRocket::RocketComponent;
using QtRocket::RocketComponentCalc;
using QtRocket::Transformation;
using QtRocket::WarningSet;
using QtRocket::Test::TestComponent;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The smallest concrete RocketComponentCalc, exposing the protected CP helpers.
class TestCalc final : public RocketComponentCalc
{
public:
    explicit TestCalc(const RocketComponent& component) : RocketComponentCalc(component) { }

    using RocketComponentCalc::kSubsonicCpPos;
    using RocketComponentCalc::supersonicCPPos;
    using RocketComponentCalc::transonicCPPos;

    void calculateNonaxialForces(const FlightConditions& /*conditions*/,
                                 const Transformation& /*transform*/, AerodynamicForces& forces,
                                 WarningSet& /*warnings*/) override
    {
        forces.setCN(1);
    }
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& /*conditions*/,
                                             double componentCf, WarningSet& /*warnings*/) override
    {
        return componentCf;
    }
    [[nodiscard]] double calculatePressureCD(const FlightConditions& /*conditions*/,
                                             double stagnationCD, double /*baseCD*/,
                                             WarningSet& /*warnings*/) override
    {
        return stagnationCD;
    }
};

TEST(RocketComponentCalc, BaseDragDefaultsToZero)
{
    const TestComponent    component;
    TestCalc               calc{component};
    const FlightConditions conditions;
    WarningSet             warnings;
    EXPECT_EQ(calc.calculateComponentBaseCD(conditions, 0.12, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

TEST(RocketComponentCalc, ReynoldsNumberIsVelocityTimesLengthOverViscosity)
{
    FlightConditions conditions;
    conditions.setAtmosphericConditions(AtmosphericConditions{280, 90000});
    conditions.setMach(0.5);
    const double expected = conditions.getVelocity() * 0.3 /
                            conditions.getAtmosphericConditions().getKinematicViscosity();
    EXPECT_EQ(RocketComponentCalc::calculateReynoldsNumber(0.3, conditions), expected);
    EXPECT_EQ(RocketComponentCalc::calculateReynoldsNumber(0.0, conditions), 0.0);
}

TEST(RocketComponentCalc, SupersonicCpPositionIsJavas)
{
    // RocketComponentCalc.supersonicCPPos on JDK 17; arithmetic only, so the same doubles.
    // arBeta, position
    constexpr std::array<std::array<double, 2>, 13> kCases{{
        {-1.0, 0.25},
        {0.0, 0.25},
        {0.5, 0.25},
        {0.84, 0.25},
        {0.85, 0.25069921874999995},
        {0.9, 0.27053124999999995},
        {0.95, 0.30341015624999995},
        {0.999, 0.32965490078125},
        {1.0, 0.32999999999999996},
        {1.5, 0.415},
        {3.0, 0.466},
        {10.0, 0.49105263157894735},
        {kNaN, kNaN},
    }};
    for (const std::array<double, 2>& row : kCases)
    {
        const double position = TestCalc::supersonicCPPos(row[0]);
        if (std::isnan(row[1]))
        {
            EXPECT_TRUE(std::isnan(position));
        }
        else
        {
            EXPECT_EQ(position, row[1]) << "arBeta " << row[0];
        }
    }
    EXPECT_EQ(TestCalc::kSubsonicCpPos, 0.25);
}

TEST(RocketComponentCalc, SupersonicCpPositionIsContinuousAtTheBridgeEnds)
{
    // The bridge meets the quarter chord at 0.84 and the source formula at arBeta = 1.
    const double start = (0.67 - 0.25) / (1 - (2 * 0.25));
    EXPECT_NEAR(TestCalc::supersonicCPPos(start + 1e-9), 0.25, 1e-9);
    EXPECT_NEAR(TestCalc::supersonicCPPos(1.0 - 1e-9), TestCalc::supersonicCPPos(1.0), 1e-9);
    // The source formula is monotone and tends to one half.
    EXPECT_LT(TestCalc::supersonicCPPos(3.0), TestCalc::supersonicCPPos(10.0));
    EXPECT_LT(TestCalc::supersonicCPPos(1e9), 0.5);
}

TEST(RocketComponentCalc, TransonicCpPositionIsJavas)
{
    // RocketComponentCalc.transonicCPPos on JDK 17 (sqrt and, at low aspect ratios, pow).
    // Mach, aspect ratio, position
    constexpr std::array<std::array<double, 3>, 56> kCases{{
        {0.5, 0.1, 0.25},
        {0.6, 0.1, 0.25},
        {0.9, 0.1, 0.25},
        {1.0, 0.1, 0.25},
        {1.25, 0.1, 0.25},
        {1.7, 0.1, 0.25},
        {2.0, 0.1, 0.25},
        {0.5, 0.3, 0.25},
        {0.6, 0.3, 0.25},
        {0.9, 0.3, 0.25},
        {1.0, 0.3, 0.25},
        {1.25, 0.3, 0.25},
        {1.7, 0.3, 0.25},
        {2.0, 0.3, 0.25},
        {0.5, 0.45, 0.25},
        {0.6, 0.45, 0.25},
        {0.9, 0.45, 0.25},
        {1.0, 0.45, 0.25},
        {1.25, 0.45, 0.25},
        {1.7, 0.45, 0.25},
        {2.0, 0.45, 0.25},
        {0.5, 0.5, 0.25},
        {0.6, 0.5, 0.25},
        {0.9, 0.5, 0.25},
        {1.0, 0.5, 0.25},
        {1.25, 0.5, 0.25},
        {1.7, 0.5, 0.25000000108088066},
        {2.0, 0.5, 0.2544561211363595},
        {0.5, 0.6, 0.25},
        {0.6, 0.6, 0.2500000948013319},
        {0.9, 0.6, 0.25022791466429195},
        {1.0, 0.6, 0.25073629097130473},
        {1.25, 0.6, 0.25550744725306707},
        {1.7, 0.6, 0.29178659343439955},
        {2.0, 0.6, 0.3423679624264906},
        {0.5, 1.0, 0.25},
        {0.6, 1.0, 0.25482015174869055},
        {0.9, 1.0, 0.3021860268860546},
        {1.0, 1.0, 0.321238484704136},
        {1.25, 1.0, 0.3637301315103315},
        {1.7, 1.0, 0.4109227832032711},
        {2.0, 1.0, 0.4310093386751437},
        {0.5, 2.0, 0.25},
        {0.6, 2.0, 0.25782763542378917},
        {0.9, 2.0, 0.3319100797863368},
        {1.0, 2.0, 0.36021181844746997},
        {1.25, 2.0, 0.4183049994193134},
        {1.7, 2.0, 0.4633157857645463},
        {2.0, 2.0, 0.47132352023091884},
        {0.5, 5.0, 0.25},
        {0.6, 5.0, 0.25904684225126756},
        {0.9, 5.0, 0.3440348306839625},
        {1.0, 5.0, 0.3761557899080676},
        {1.25, 5.0, 0.44086171035445915},
        {1.7, 5.0, 0.4858228765876417},
        {2.0, 5.0, 0.48958365761582867},
    }};
    for (const std::array<double, 3>& row : kCases)
    {
        EXPECT_NEAR(TestCalc::transonicCPPos(row[0], row[1]), row[2], 1e-12 * row[2])
            << "Mach " << row[0] << " aspect ratio " << row[1];
    }
}

/// Checks that the transonic CP position of @p aspectRatio starts at the quarter chord, ends at
/// the Mach-2 supersonic position and does not move forward in between.
void expectTransonicJoin(double aspectRatio)
{
    SCOPED_TRACE(::testing::Message() << "aspect ratio " << aspectRatio);
    EXPECT_EQ(TestCalc::transonicCPPos(0.5, aspectRatio), 0.25);
    EXPECT_NEAR(TestCalc::transonicCPPos(2.0, aspectRatio),
                TestCalc::supersonicCPPos(aspectRatio * std::numbers::sqrt3), 1e-12);
    double previous = 0.25;
    for (int i = 1; i <= 30; i++)
    {
        const double position = TestCalc::transonicCPPos(0.5 + (0.05 * i), aspectRatio);
        EXPECT_GE(position, previous - 1e-15) << i;
        previous = position;
    }
}

TEST(RocketComponentCalc, TransonicCpPositionJoinsTheSubsonicAndSupersonicPositions)
{
    for (const double aspectRatio : {0.55, 0.6, 1.0, 3.0})
    {
        expectTransonicJoin(aspectRatio);
    }
    EXPECT_TRUE(std::isnan(TestCalc::transonicCPPos(kNaN, 1.0)));
}

TEST(RocketComponentCalc, SubclassCalculatesThroughTheInterface)
{
    const TestComponent    component;
    TestCalc               calc{component};
    RocketComponentCalc&   base = calc;
    const FlightConditions conditions;
    WarningSet             warnings;
    AerodynamicForces      forces;
    base.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_EQ(forces.getCN(), 1);
    EXPECT_EQ(base.calculateFrictionCD(conditions, 0.01, warnings), 0.01);
    EXPECT_EQ(base.calculatePressureCD(conditions, 0.85, 0.12, warnings), 0.85);
}

}  // namespace
