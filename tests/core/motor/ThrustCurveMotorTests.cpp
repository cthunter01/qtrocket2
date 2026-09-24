#include "QtRocket/motor/ThrustCurveMotor.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/CaseInfo.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::CaseInfo;
using QtRocket::Coordinate;
using QtRocket::ErrorCode;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::Result;
using QtRocket::ThrustCurveMotor;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The builder of ThrustCurveMotorTest.motorX6.
ThrustCurveMotor::Builder x6Builder()
{
    constexpr double          kRadius = 0.025;
    constexpr double          kLength = 0.10;
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("foo"))
        .setDesignation("X6")
        .setDescription("Description of X6")
        .setMotorType(Motor::Type::RELOAD)
        .setStandardDelays({0, 2, Motor::kPluggedDelay})
        .setDiameter(kRadius * 2)
        .setLength(kLength)
        .setTimePoints({0, 1, 3, 4})
        .setThrustPoints({0, 2, 3, 0})
        .setCGPoints({Coordinate(0.02, 0, 0, 0.05), Coordinate(0.02, 0, 0, 0.05),
                      Coordinate(0.02, 0, 0, 0.05), Coordinate(0.03, 0, 0, 0.03)})
        .setDigest("digestA");
    return builder;
}

/// The builder of ThrustCurveMotorTest.motorEstesA8_3.
ThrustCurveMotor::Builder a8Builder()
{
    constexpr double          kRadiusA8 = 0.018;
    constexpr double          kLengthA8 = 0.10;
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation("A8-3")
        .setDescription("A8 Test Motor")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({0, 2, Motor::kPluggedDelay})
        .setDiameter(kRadiusA8 * 2)
        .setLength(kLengthA8)
        .setTimePoints({0,     0.041, 0.084, 0.127, 0.166, 0.192, 0.206, 0.226,
                        0.236, 0.247, 0.261, 0.277, 0.306, 0.351, 0.405, 0.467,
                        0.532, 0.589, 0.632, 0.652, 0.668, 0.684, 0.703, 0.73})
        .setThrustPoints({0,     0.512, 2.115, 4.358, 6.794, 8.588, 9.294, 9.73,
                          8.845, 7.179, 5.063, 3.717, 3.205, 2.884, 2.499, 2.371,
                          2.307, 2.371, 2.371, 2.243, 1.794, 1.153, 0.448, 0})
        .setCGPoints({Coordinate(0.0350, 0, 0, 0.016350), Coordinate(0.0352, 0, 0, 0.016335),
                      Coordinate(0.0354, 0, 0, 0.016255), Coordinate(0.0356, 0, 0, 0.016057),
                      Coordinate(0.0358, 0, 0, 0.015748), Coordinate(0.0360, 0, 0, 0.015463),
                      Coordinate(0.0362, 0, 0, 0.015285), Coordinate(0.0364, 0, 0, 0.015014),
                      Coordinate(0.0366, 0, 0, 0.014882), Coordinate(0.0368, 0, 0, 0.014757),
                      Coordinate(0.0370, 0, 0, 0.014635), Coordinate(0.0372, 0, 0, 0.014535),
                      Coordinate(0.0374, 0, 0, 0.014393), Coordinate(0.0376, 0, 0, 0.014198),
                      Coordinate(0.0378, 0, 0, 0.013991), Coordinate(0.0380, 0, 0, 0.013776),
                      Coordinate(0.0382, 0, 0, 0.013560), Coordinate(0.0384, 0, 0, 0.013370),
                      Coordinate(0.0386, 0, 0, 0.013225), Coordinate(0.0388, 0, 0, 0.013160),
                      Coordinate(0.0390, 0, 0, 0.013114), Coordinate(0.0392, 0, 0, 0.013080),
                      Coordinate(0.0394, 0, 0, 0.013059), Coordinate(0.0396, 0, 0, 0.013050)})
        .setDigest("digestA8-3");
    return builder;
}

/// The motor @p builder builds; a build failure fails the test (and throws out of it).
ThrustCurveMotor build(const ThrustCurveMotor::Builder& builder)
{
    Result<ThrustCurveMotor> built = builder.build();
    EXPECT_TRUE(built.has_value()) << built.error().toString();
    return std::move(built).value();
}

/// A two-point motor with the given designation and common name
/// (ThrustCurveMotorTest.buildSimpleMotor).
ThrustCurveMotor buildSimpleMotor(std::string designation, std::string commonName)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("TestCo"))
        .setDesignation(std::move(designation))
        .setCommonName(std::move(commonName))
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({})
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0.0, 1.0})
        .setThrustPoints({0.0, 0.0})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.05), Coordinate(0.035, 0, 0, 0.04)});
    return build(builder);
}

/// A motor of manufacturer "A" with the given curve, CG at half the length, and the mass falling
/// by 0.01 over the points (the harness that pinned OpenRocket's values builds the same).
ThrustCurveMotor::Builder simpleBuilder(std::vector<double> time, std::vector<double> thrust)
{
    constexpr double        kLength = 0.07;
    std::vector<Coordinate> cg;
    cg.reserve(time.size());
    for (std::size_t i = 0; i < time.size(); i++)
    {
        cg.emplace_back(kLength / 2, 0, 0,
                        0.1 - (0.01 * static_cast<double>(i) / static_cast<double>(time.size())));
    }
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("A"))
        .setDesignation("F1")
        .setMotorType(Motor::Type::SINGLE)
        .setDiameter(0.024)
        .setLength(kLength)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg));
    return builder;
}

/// The error message of a build that must fail with INVALID_ARGUMENT.
std::string buildError(const ThrustCurveMotor::Builder& builder)
{
    const Result<ThrustCurveMotor> built = builder.build();
    EXPECT_FALSE(built.has_value());
    if (built.has_value())
    {
        return {};
    }
    EXPECT_EQ(built.error().code, ErrorCode::INVALID_ARGUMENT);
    return built.error().message;
}

class ThrustCurveMotorTest : public ::testing::Test
{
protected:
    ThrustCurveMotor m_motorX6{build(x6Builder())};
    ThrustCurveMotor m_motorEstesA8{build(a8Builder())};
};

// ---- Ported from ThrustCurveMotorTest.java ----

TEST_F(ThrustCurveMotorTest, VerifyMotorA8_3Times)
{
    const ThrustCurveMotor& mtr = m_motorEstesA8;

    EXPECT_NEAR(0.041, mtr.getTime(0.041), 0.001);

    EXPECT_NEAR(0.206, mtr.getTime(0.206), 0.001);
}

TEST_F(ThrustCurveMotorTest, VerifyMotorA8_3Thrusts)
{
    const ThrustCurveMotor& mtr = m_motorEstesA8;

    EXPECT_NEAR(0.512, mtr.getThrust(0.041), 0.001);

    EXPECT_NEAR(9.294, mtr.getThrust(0.206), 0.001);
}

TEST_F(ThrustCurveMotorTest, VerifyMotorA8_3CG)
{
    const ThrustCurveMotor& mtr = m_motorEstesA8;

    const double actCGx0p041 = mtr.getCMx(0.041);
    EXPECT_NEAR(0.0352, actCGx0p041, 0.001);
    const double actMass0p041 = mtr.getTotalMass(0.041);
    EXPECT_NEAR(0.016335, actMass0p041, 0.001);

    const double actCGx0p206 = mtr.getCMx(0.206);
    EXPECT_NEAR(0.0362, actCGx0p206, 0.001);
    const double actMass0p206 = mtr.getTotalMass(0.206);
    EXPECT_NEAR(0.015285, actMass0p206, 0.001);
}

TEST_F(ThrustCurveMotorTest, ThrustInterpolation)
{
    const ThrustCurveMotor& mtr = m_motorEstesA8;

    // (expected thrust, motor time)
    const std::vector<std::pair<double, double>> testPairs{
        {0.512, 0.041}, {2.115, 0.084}, {1.220, 0.060},
        {1.593, 0.070}, {1.965, 0.080}, {2.428, 0.090},
    };

    for (const auto& [expThrust, motorTime] : testPairs)
    {
        const double actThrust = mtr.getThrust(motorTime);

        EXPECT_NEAR(expThrust, actThrust, 0.001) << "Error in interpolating thrust: ";
    }
}

TEST_F(ThrustCurveMotorTest, MotorData)
{
    EXPECT_EQ(m_motorX6.getDesignation(), "X6");
    EXPECT_EQ(m_motorX6.getDesignation(5.0), "X6-5");
    EXPECT_EQ(m_motorX6.getDescription(), "Description of X6");
    EXPECT_EQ(Motor::Type::RELOAD, m_motorX6.getMotorType());
}

TEST_F(ThrustCurveMotorTest, TimeIndexingNegative)
{
    const ThrustCurveMotor& mtr = m_motorX6;
    // attempt to retrieve for a time before the motor ignites
    EXPECT_TRUE(std::isnan(mtr.getTime(-1))) << "Fault in negative time indexing: ";
}

TEST_F(ThrustCurveMotorTest, TimeIndexingPastBurnout)
{
    const ThrustCurveMotor& mtr = m_motorX6;

    // attempt to retrieve for a time after the motor finishes
    // should retrieve the last time value. In this case: 4.0
    EXPECT_NEAR(4.0, mtr.getTime(std::numeric_limits<double>::max()), 0.00000001);
    EXPECT_NEAR(4.0, mtr.getTime(20.0), 0.00000001);
}

TEST_F(ThrustCurveMotorTest, TimeIndexingAtBurnout)
{
    // attempt to retrieve for a time after motor cutoff
    EXPECT_NEAR(4.0, m_motorX6.getTime(4.0), 0.00001);
}

TEST_F(ThrustCurveMotorTest, TimeRetrieval)
{
    const ThrustCurveMotor& mtr = m_motorX6;

    for (const double searchTime : {0.2, 0.441, 0.512, 1.0, 2.0, 3.0})
    {
        EXPECT_NEAR(searchTime, mtr.getTime(searchTime), 0.00001);
    }
}

TEST_F(ThrustCurveMotorTest, ThrustRetrieval)
{
    // attempt to retrieve an integer index:
    EXPECT_NEAR(2.0, m_motorX6.getThrust(1), 0.001);
    EXPECT_NEAR(2.5, m_motorX6.getThrust(2), 0.001);
    EXPECT_NEAR(3.0, m_motorX6.getThrust(3), 0.001);
}

TEST(ThrustCurveMotor, SimplifyDesignation)
{
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("J115"), "J115");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation(" J115  "), "J115");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("241H115-KS"), "H115");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("384  J115"), "J115");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("384-J115"), "J115");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("A2T"), "A2");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("1/2A2T"), "1/2A2T");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("Micro Maxx II"), "MicroMaxxII");
}

TEST(ThrustCurveMotor, CommonNameDelayStripping)
{
    // Delay suffix stripped
    EXPECT_EQ("B6", buildSimpleMotor("B6", "B6-0").getCommonName());
    EXPECT_EQ("C6", buildSimpleMotor("C6", "C6-3").getCommonName());
    EXPECT_EQ("B6", buildSimpleMotor("B6", "B6-P").getCommonName());
    // Propellant-code suffix stripped
    EXPECT_EQ("B6", buildSimpleMotor("B6W", "B6W").getCommonName());
    EXPECT_EQ("H128", buildSimpleMotor("H128W", "H128W").getCommonName());
    // Already-simplified names remain unchanged
    EXPECT_EQ("B6", buildSimpleMotor("B6", "B6").getCommonName());
    EXPECT_EQ("C6", buildSimpleMotor("C6", "C6").getCommonName());
    // Non-standard names that don't match the pattern are left unchanged
    EXPECT_EQ("RCS 18/20", buildSimpleMotor("RCS 18/20", "RCS 18/20").getCommonName());
}

// ---- Values pinned by running OpenRocket's ThrustCurveMotor on the same data ----

TEST_F(ThrustCurveMotorTest, EstimatesMatchOpenRocket)
{
    const ThrustCurveMotor& a8 = m_motorEstesA8;
    EXPECT_EQ(a8.getBurnTimeEstimate(), 0.6630044035350177);
    EXPECT_EQ(a8.getAverageThrustEstimate(), 3.4753685481065397);
    EXPECT_EQ(a8.getMaxThrustEstimate(), 9.73);
    EXPECT_EQ(a8.getTotalImpulseEstimate(), 2.3201939999999994);
    EXPECT_EQ(a8.getUnitIxx(), 1.6199999999999998E-4);
    EXPECT_EQ(a8.getUnitIyy(), 9.143333333333335E-4);
    EXPECT_EQ(a8.getUnitIzz(), 9.143333333333335E-4);
    EXPECT_EQ(a8.getUnitRotationalInertia(), a8.getUnitIxx());
    EXPECT_EQ(a8.getUnitLongitudinalInertia(), a8.getUnitIyy());
    EXPECT_EQ(a8.getPropellantMass(), 0.003299999999999999);
    EXPECT_EQ(a8.getCommonName(), "A8");
    EXPECT_EQ(a8.getDesignation(3), "A8-3-3");
    EXPECT_EQ(a8.getCommonName(Motor::kPluggedDelay), "A8-P");
}

/// Values of the A8-3 motor at one time, as OpenRocket computes them.
struct Pin
{
    Pin(double timeValue, double thrustValue, double cmxValue, double massValue,
        double propellantValue, double timeBackValue)
      : time(timeValue),
        thrust(thrustValue),
        cmx(cmxValue),
        mass(massValue),
        propellant(propellantValue),
        timeBack(timeBackValue)
    {
    }

    double time;
    double thrust;
    double cmx;
    double mass;
    double propellant;
    double timeBack;
};

void expectPin(const ThrustCurveMotor& motor, const Pin& pin)
{
    SCOPED_TRACE(pin.time);
    EXPECT_EQ(motor.getThrust(pin.time), pin.thrust);
    EXPECT_EQ(motor.getCMx(pin.time), pin.cmx);
    EXPECT_EQ(motor.getTotalMass(pin.time), pin.mass);
    EXPECT_EQ(motor.getPropellantMass(pin.time), pin.propellant);
    EXPECT_EQ(motor.getTime(pin.time), pin.timeBack);
}

TEST_F(ThrustCurveMotorTest, InterpolationMatchesOpenRocket)
{
    const std::vector<Pin> pins{
        {0.0, 0.0, 0.035, 0.01635, 0.003299999999999999, 0.0},
        {0.0205, 0.256, 0.035100000000000006, 0.0163425, 0.0032924999999999986, 0.0205},
        {0.041, 0.512, 0.0352, 0.016335, 0.003284999999999998, 0.041},
        {0.05, 0.8475116279069768, 0.03524186046511628, 0.016318255813953488, 0.0032682558139534874,
         0.05},
        {0.1, 2.949604651162791, 0.03547441860465116, 0.01618132558139535, 0.0031313255813953476,
         0.1},
        {0.2, 8.991428571428573, 0.03611428571428572, 0.015361285714285713, 0.002311285714285712,
         0.2},
        {0.2261, 9.721150000000002, 0.036402, 0.01501268, 0.0019626799999999996, 0.2261},
        {0.5, 2.3385076923076924, 0.03810153846153846, 0.01366633846153846, 6.163384615384595E-4,
         0.5},
        {0.72, 0.16592592592592587, 0.03952592592592593, 0.013053333333333333,
         3.3333333333326193E-6, 0.72},
        {0.73, 0.0, 0.0396, 0.01305, 0.0, 0.73},
        {1.0, 0.0, 0.0396, 0.01305, 0.0, 0.73},
    };
    for (const Pin& pin : pins)
    {
        expectPin(m_motorEstesA8, pin);
    }
}

TEST(ThrustCurveMotor, EdgeCurveEstimatesMatchOpenRocket)
{
    // All thrust zero: the limit is zero, so the burn spans the whole curve.
    const ThrustCurveMotor flat = build(simpleBuilder({0, 1, 2}, {0, 0, 0}));
    EXPECT_EQ(flat.getBurnTimeEstimate(), 2.0);
    EXPECT_EQ(flat.getAverageThrustEstimate(), 0.0);
    EXPECT_EQ(flat.getTotalImpulseEstimate(), 0.0);
    EXPECT_EQ(flat.getMaxThrustEstimate(), 0.0);

    // Starting above the limit, and a flat top.
    const ThrustCurveMotor step = build(simpleBuilder({0, 0.1, 0.2, 1.0, 1.5}, {5, 20, 20, 1, 0}));
    EXPECT_EQ(step.getBurnTimeEstimate(), 1.0);
    EXPECT_EQ(step.getAverageThrustEstimate(), 11.65);
    EXPECT_EQ(step.getTotalImpulseEstimate(), 11.9);
    EXPECT_EQ(step.getMaxThrustEstimate(), 20.0);

    // Neighbours equal at the limit crossing take the midpoint ("for safety").
    const ThrustCurveMotor plateau =
        build(simpleBuilder({0, 0.1, 0.2, 0.3, 0.4}, {0.5, 0.5, 10, 0.5, 0.5}));
    EXPECT_EQ(plateau.getBurnTimeEstimate(), 0.4);
    EXPECT_EQ(plateau.getAverageThrustEstimate(), 2.875);
    EXPECT_EQ(plateau.getTotalImpulseEstimate(), 1.1500000000000001);
}

TEST(ThrustCurveMotor, DelayStringsMatchOpenRocket)
{
    const std::vector<std::pair<double, std::string_view>> pins{
        {0, "0"},
        {2, "2"},
        {5.0, "5"},
        {2.5, "2.5"},
        {2.44, "2.4"},
        {2.45, "2.4"},
        {2.55, "2.6"},
        {-1.25, "-1.2"},
        {0.05, "0"},
        {0.15, "0.2"},
        {10.96, "11"},
        {kNaN, "NaN"},
        {-std::numeric_limits<double>::infinity(), "-Infinity"},
        {Motor::kPluggedDelay, "P"},
        {3.0e9, "2147483647"},
        {-0.04, "0"},
        {1e-9, "0"},
        {7.999999999, "8"},
    };
    for (const auto& [delay, expected] : pins)
    {
        EXPECT_EQ(ThrustCurveMotor::getDelayString(delay), expected) << delay;
    }
    EXPECT_EQ(ThrustCurveMotor::getDelayString(Motor::kPluggedDelay, "none"), "none");
    EXPECT_EQ(ThrustCurveMotor::getDelayString(4, "none"), "4");
}

TEST(ThrustCurveMotor, SimplifyDesignationEdgeCasesMatchOpenRocket)
{
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("h128w"), "h128w");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("H128\nW"), "H128W");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("H128W\n"), "H128");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("\tH128"), "H128");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation(" - H1"), "H1");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("12"), "12");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation(""), "");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("  "), "");
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("K\t1 0"), "K10");
    // A Unicode line separator after the number also defeats the pattern.
    EXPECT_EQ(ThrustCurveMotor::Builder::simplifyDesignation("H128\xE2\x80\xA8W"),
              "H128\xE2\x80\xA8W");
}

TEST(ThrustCurveMotor, CommonNameComesFromTheDesignationOrCode)
{
    // No common name: simplifyDesignation of the designation.
    EXPECT_EQ(buildSimpleMotor("241H115-KS", "").getCommonName(), "H115");
    EXPECT_EQ(buildSimpleMotor("Micro Maxx II", "").getCommonName(), "MicroMaxxII");
    // The common name is matched untrimmed: a leading space is part of the [ -]* prefix, a tab
    // is not.
    EXPECT_EQ(buildSimpleMotor("X1", " B6-0").getCommonName(), "B6");
    EXPECT_EQ(buildSimpleMotor("X1", "\tB6-0").getCommonName(), "\tB6-0");

    // No designation: the code stands in for it.
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {1, 1});
    builder.setDesignation("").setCode("G80T-7");
    const ThrustCurveMotor fromCode = build(builder);
    EXPECT_EQ(fromCode.getDesignation(), "G80T-7");
    EXPECT_EQ(fromCode.getCode(), "G80T-7");
    EXPECT_EQ(fromCode.getCommonName(), "G80");
}

TEST(ThrustCurveMotor, ValidationMessagesMatchOpenRocket)
{
    EXPECT_EQ(buildError(simpleBuilder({0, 1}, {0, 1, 0})),
              "Array lengths do not match, time:2 thrust:3 cg:2");
    EXPECT_EQ(buildError(simpleBuilder({0}, {0})), "Too short thrust-curve, length=1");
    EXPECT_EQ(buildError(simpleBuilder({0, 1, 1}, {0, 1.5, 2})),
              "Two thrust values for single time point, time[1]=1.0, thrust=1.5; time[2]=1.0, "
              "thrust=2.0");
    EXPECT_EQ(buildError(simpleBuilder({0.5, 1}, {0, 1})), "Curve starts at time 0.5");
    EXPECT_EQ(buildError(simpleBuilder({0, 1}, {0, -1})), "Negative thrust.");
    EXPECT_EQ(buildError(simpleBuilder({0, 1}, {0, 1.0e7 + 1})), "Invalid thrust 1.0000001E7");
    EXPECT_EQ(buildError(simpleBuilder({0, 1}, {0, kNaN})), "Invalid thrust NaN");

    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {0, 1});
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(kNaN, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder), "Invalid CG (NaN,0.00000,0.00000,w=0.10000)");
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(-0.001, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder),
              "Invalid CG position -0.001000: CG is below the start of the motor.");
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(0.0712345, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder),
              "Invalid CG position: 0.071235: CG is above the end of the motor.");
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(0.01, 0, 0, -0.1)});
    EXPECT_EQ(buildError(builder), "Negative mass -0.1at time=1.0");
}

TEST(ThrustCurveMotor, ValidationEdgeCases)
{
    // An empty curve fails on its length; mismatched CG arrays on the lengths.
    EXPECT_EQ(buildError(simpleBuilder({}, {})), "Too short thrust-curve, length=0");
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {0, 1});
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder), "Array lengths do not match, time:2 thrust:2 cg:1");

    // The start may be off zero by less than MathUtil's tolerance.
    EXPECT_TRUE(simpleBuilder({4e-9, 1}, {0, 1}).build().has_value());
    EXPECT_EQ(buildError(simpleBuilder({6e-9, 1}, {0, 1})), "Curve starts at time 6.0E-9");

    // Decreasing time is reported at the first offending pair.
    EXPECT_EQ(buildError(simpleBuilder({0, 2, 1}, {0, 1, 2})),
              "Two thrust values for single time point, time[1]=2.0, thrust=1.0; time[2]=1.0, "
              "thrust=2.0");

    // A NaN time after the first passes the monotonic check (every comparison with NaN is
    // false), as in OpenRocket; a NaN first time fails the start check.
    EXPECT_EQ(buildError(simpleBuilder({kNaN, 1}, {0, 1})), "Curve starts at time NaN");

    // Exactly the maximum thrust is allowed, a CG exactly at either end too.
    EXPECT_TRUE(simpleBuilder({0, 1}, {0, ThrustCurveMotor::kMaxThrust}).build().has_value());
    builder.setCGPoints({Coordinate(0, 0, 0, 0.1), Coordinate(0.07, 0, 0, 0)});
    EXPECT_TRUE(builder.build().has_value());

    // An infinite thrust is invalid.
    EXPECT_EQ(buildError(simpleBuilder({0, 1}, {0, std::numeric_limits<double>::infinity()})),
              "Invalid thrust Infinity");

    // A curve that neither starts nor ends at zero thrust is accepted.
    EXPECT_TRUE(simpleBuilder({0, 1}, {3, 3}).build().has_value());
}

TEST(ThrustCurveMotor, NegativeMassReportsTheFirstEqualPoint)
{
    // Arrays.asList(cg).indexOf(c) finds the first point equal to the offending one within
    // Coordinate's tolerance: a mass of -1e-10 equals the zero mass of the point before, so the
    // message names that point's time.
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1, 2}, {0, 1, 0});
    builder.setCGPoints(
        {Coordinate(0.01, 0, 0, 0), Coordinate(0.01, 0, 0, -1e-10), Coordinate(0.01, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder), "Negative mass -1.0E-10at time=0.0");
    builder.setCGPoints(
        {Coordinate(0.01, 0, 0, 0.1), Coordinate(0.01, 0, 0, -0.1), Coordinate(0.01, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder), "Negative mass -0.1at time=1.0");
}

TEST(ThrustCurveMotor, NegativeMassOfAPointEqualToNone)
{
    // A point with an infinite mass or coordinate equals no point, itself included (MathUtil's
    // equals is false for two equal infinities), so OpenRocket's indexOf finds none and indexing
    // the time array throws; here the message names the point's own time.
    constexpr double          kInf    = std::numeric_limits<double>::infinity();
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {0, 1});
    builder.setCGPoints({Coordinate(0.01, 0, 0, 0.1), Coordinate(0.01, 0, 0, -kInf)});
    EXPECT_EQ(buildError(builder), "Negative mass -Infinityat time=1.0");
    builder.setCGPoints({Coordinate(0.01, kInf, 0, 0.1), Coordinate(0.01, kInf, 0, -0.1)});
    EXPECT_EQ(buildError(builder), "Negative mass -0.1at time=1.0");
    builder.setCGPoints({Coordinate(0.01, 0, -kInf, -0.1), Coordinate(0.01, 0, 0, 0.1)});
    EXPECT_EQ(buildError(builder), "Negative mass -0.1at time=0.0");
}

TEST(ThrustCurveMotor, BuilderCanBuildAgain)
{
    ThrustCurveMotor::Builder builder = a8Builder();
    const ThrustCurveMotor    first   = build(builder);
    builder.setDigest("other");
    const ThrustCurveMotor second = build(builder);
    EXPECT_EQ(first.getDigest(), "digestA8-3");
    EXPECT_EQ(second.getDigest(), "other");
    EXPECT_EQ(second.getCommonName(), first.getCommonName());
    EXPECT_EQ(second.getAverageThrustEstimate(), first.getAverageThrustEstimate());
}

TEST(ThrustCurveMotor, DefaultsOfAnEmptyBuilder)
{
    ThrustCurveMotor::Builder builder;
    builder.setTimePoints({0, 1}).setThrustPoints({1, 0}).setCGPoints(
        {Coordinate(0, 0, 0, 1), Coordinate(0, 0, 0, 0.5)});
    const ThrustCurveMotor motor = build(builder);
    EXPECT_EQ(&motor.getManufacturer(), &Manufacturer::getManufacturer("Unknown"));
    EXPECT_EQ(motor.getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_EQ(motor.getDigest(), "");
    EXPECT_EQ(motor.getCode(), "");
    EXPECT_EQ(motor.getDesignation(), "");
    EXPECT_EQ(motor.getCommonName(), "");
    EXPECT_EQ(motor.getDescription(), "");
    EXPECT_EQ(motor.getCaseInfo(), "");
    EXPECT_EQ(motor.getPropellantInfo(), "");
    EXPECT_EQ(motor.getTcMotorId(), "");
    EXPECT_EQ(motor.getInfoUrl(), "");
    EXPECT_EQ(motor.getDataFiles(), std::nullopt);
    EXPECT_EQ(motor.getUpdatedOn(), "");
    EXPECT_EQ(motor.getDataSource(), "");
    EXPECT_FALSE(motor.isSparky());
    EXPECT_TRUE(motor.isAvailable());
    EXPECT_TRUE(motor.getStandardDelays().empty());
    EXPECT_EQ(motor.getDiameter(), 0.0);
    EXPECT_EQ(motor.getLength(), 0.0);
    EXPECT_EQ(motor.getInitialMass(), 0.0);
    EXPECT_EQ(motor.getUnitIxx(), 0.0);
    EXPECT_EQ(motor.getUnitIyy(), 0.0);
    EXPECT_EQ(motor.getCaseInfoEnum(), std::nullopt);
    EXPECT_TRUE(motor.getCompatibleCases().empty());
}

TEST(ThrustCurveMotor, MetadataIsKept)
{
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {1, 0});
    builder.setCaseInfo("RMS-29/180")
        .setPropellantInfo("Blue Thunder")
        .setTcMotorId("5f4294d20002e900000005a0")
        .setInfoUrl("https://www.thrustcurve.org/motors/AeroTech/G80T/")
        .setDataFiles(3)
        .setUpdatedOn("2024-01-02")
        .setDataSource("cert")
        .setSparky(true)
        .setAvailability(false)
        .setInitialMass(0.123)
        .setCode("G80T")
        .setStandardDelays({4, 7, Motor::kPluggedDelay});
    const ThrustCurveMotor motor = build(builder);
    EXPECT_EQ(motor.getCaseInfo(), "RMS-29/180");
    EXPECT_EQ(motor.getCaseInfoEnum(), CaseInfo::RMS29_180);
    const std::vector<CaseInfo> compatible(motor.getCompatibleCases().begin(),
                                           motor.getCompatibleCases().end());
    EXPECT_EQ(compatible, (std::vector<CaseInfo>{CaseInfo::RMS29_180, CaseInfo::RMS29_240,
                                                 CaseInfo::RMS29_360}));
    EXPECT_EQ(motor.getPropellantInfo(), "Blue Thunder");
    EXPECT_EQ(motor.getTcMotorId(), "5f4294d20002e900000005a0");
    EXPECT_EQ(motor.getInfoUrl(), "https://www.thrustcurve.org/motors/AeroTech/G80T/");
    EXPECT_EQ(motor.getDataFiles(), 3);
    EXPECT_EQ(motor.getUpdatedOn(), "2024-01-02");
    EXPECT_EQ(motor.getDataSource(), "cert");
    EXPECT_TRUE(motor.isSparky());
    EXPECT_FALSE(motor.isAvailable());
    EXPECT_EQ(motor.getInitialMass(), 0.123);
    EXPECT_EQ(motor.getCode(), "G80T");
    EXPECT_EQ(motor.getDesignation(), "F1");
    EXPECT_EQ(motor.getStandardDelays(), (std::vector<double>{4, 7, Motor::kPluggedDelay}));
    EXPECT_EQ(motor.getSampleSize(), 2U);
    EXPECT_EQ(motor.getDataSize(), 2U);
    EXPECT_EQ(motor.getCutOffTime(), 1.0);
    EXPECT_EQ(motor.getBurnTime(), 1.0);
    EXPECT_EQ(motor.getTimePoints(), (std::vector<double>{0, 1}));
    EXPECT_EQ(motor.getThrustPoints(), (std::vector<double>{1, 0}));
    EXPECT_EQ(motor.getCGPoints().size(), 2U);
}

TEST_F(ThrustCurveMotorTest, NaNTimeGivesNaN)
{
    EXPECT_TRUE(std::isnan(m_motorX6.getThrust(kNaN)));
    EXPECT_TRUE(std::isnan(m_motorX6.getCMx(kNaN)));
    EXPECT_TRUE(std::isnan(m_motorX6.getTotalMass(kNaN)));
    EXPECT_TRUE(std::isnan(m_motorX6.getPropellantMass(-1)));
    EXPECT_TRUE(std::isnan(m_motorX6.getThrust(-0.001)));
    // Time zero is ignition, not before it.
    EXPECT_EQ(m_motorX6.getThrust(0.0), 0.0);
    EXPECT_EQ(m_motorX6.getThrust(-0.0), 0.0);
}

TEST_F(ThrustCurveMotorTest, SnapsWithinATenThousandthOfAnInterval)
{
    // X6 has 1 s between t=0 and t=1: within 1e-4 of a sample, the sample's value is returned.
    EXPECT_EQ(m_motorX6.getThrust(0.99995), 2.0);
    EXPECT_EQ(m_motorX6.getThrust(1.00005), 2.0);
    EXPECT_EQ(m_motorX6.getThrust(0.00005), 0.0);
    EXPECT_NEAR(m_motorX6.getThrust(0.5), 1.0, 1e-15);
    // Between t=1 and t=3 (2 s), the snap distance is 2e-4 s.
    EXPECT_EQ(m_motorX6.getThrust(2.9999), 3.0);
    EXPECT_NE(m_motorX6.getThrust(2.999), 3.0);
}

/// A constant-thrust motor with the given identity and dimensions.
ThrustCurveMotor sized(std::string_view manufacturer, std::string designation, double diameter,
                       double length)
{
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {1, 1});
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setDesignation(std::move(designation))
        .setDiameter(diameter)
        .setLength(length);
    return build(builder);
}

TEST(ThrustCurveMotor, CompareToMatchesOpenRocket)
{
    const ThrustCurveMotor m1   = sized("A", "F12", 0.024, 0.07);
    const ThrustCurveMotor e1   = sized("Estes", "C6-5", 0.018, 0.07);
    const ThrustCurveMotor wide = sized("A", "F12", 0.0240123456, 0.0712);

    EXPECT_EQ(m1.compareTo(m1), 0);
    EXPECT_EQ(m1.compareTo(e1), -1);  // "AeroTech" before "Estes"
    EXPECT_EQ(e1.compareTo(m1), 1);
    EXPECT_EQ(wide.compareTo(m1), 12);  // (int) (1.23456e-5 * 1e6)
    EXPECT_EQ(m1.compareTo(wide), -12);

    // Below a micrometre, diameters count as equal and the length decides.
    const ThrustCurveMotor close = sized("A", "F12", 0.0240000009, 0.0700021);
    EXPECT_EQ(close.compareTo(m1), 2);
    // The designation comes before the dimensions.
    const ThrustCurveMotor g80 = sized("A", "G80", 0.018, 0.07);
    EXPECT_GT(g80.compareTo(m1), 0);
}

TEST(ThrustCurveMotor, ToStringNamesTheMotor)
{
    ThrustCurveMotor::Builder builder = simpleBuilder({0, 1}, {1, 1});
    builder.setDigest("abc");
    EXPECT_EQ(build(builder).toString(), "ThrustCurveMotor[AeroTech F1, digest=abc]");
}

}  // namespace
