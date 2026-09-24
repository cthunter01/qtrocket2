#include "QtRocket/motor/MotorDigest.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Md5.h"
#include "QtRocket/util/Strings.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::Md5;
using QtRocket::Motor;
using QtRocket::MotorDigest;
using QtRocket::ThrustCurveMotor;
using DataType = QtRocket::MotorDigest::DataType;

/// The MD5 of @p text's UTF-8 bytes in hex (MotorDigestTest.md5).
std::string md5Hex(std::string_view text)
{
    return QtRocket::Strings::hexString(QtRocket::md5(std::as_bytes(std::span(text))));
}

/// The A8-3 motor of ThrustCurveMotorTest.
ThrustCurveMotor a8Motor()
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation("A8-3")
        .setDescription("A8 Test Motor")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({0, 2, Motor::kPluggedDelay})
        .setDiameter(0.018 * 2)
        .setLength(0.10)
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
    return builder.build().value();
}

/// A motor that is not a ThrustCurveMotor: only its own digest is compatible with it.
class OtherMotor final : public Motor
{
public:
    [[nodiscard]] Type               getMotorType() const override { return Type::UNKNOWN; }
    [[nodiscard]] const std::string& getCode() const override { return m_empty; }
    [[nodiscard]] const std::string& getCommonName() const override { return m_empty; }
    [[nodiscard]] std::string        getCommonName(double /*delay*/) const override { return {}; }
    [[nodiscard]] const std::string& getDesignation() const override { return m_empty; }
    [[nodiscard]] std::string        getDesignation(double /*delay*/) const override { return {}; }
    [[nodiscard]] const std::string& getDescription() const override { return m_empty; }
    [[nodiscard]] double             getDiameter() const override { return 0; }
    [[nodiscard]] double             getLength() const override { return 0; }
    [[nodiscard]] const std::string& getDigest() const override { return m_digest; }
    [[nodiscard]] double             getLaunchCGx() const override { return 0; }
    [[nodiscard]] double             getBurnoutCGx() const override { return 0; }
    [[nodiscard]] double             getLaunchMass() const override { return 0; }
    [[nodiscard]] double             getBurnoutMass() const override { return 0; }
    [[nodiscard]] double             getBurnTimeEstimate() const override { return 0; }
    [[nodiscard]] double             getAverageThrustEstimate() const override { return 0; }
    [[nodiscard]] double             getMaxThrustEstimate() const override { return 0; }
    [[nodiscard]] double             getTotalImpulseEstimate() const override { return 0; }
    [[nodiscard]] double             getBurnTime() const override { return 0; }
    [[nodiscard]] double             getThrust(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double             getTotalMass(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getPropellantMass(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getCMx(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getUnitIxx() const override { return 0; }
    [[nodiscard]] double getUnitIyy() const override { return 0; }
    [[nodiscard]] double getUnitIzz() const override { return 0; }

private:
    std::string m_empty;
    std::string m_digest{"4375744555a4e870668294eb16fb1e23"};
};

// ---- Ported from MotorDigestTest.java ----

TEST(MotorDigest, MotorDigest)
{
    const std::array<double, 4>        timeArray{0.0, 0.123456789, 0.4115,
                                                 std::nextafter(std::nextafter(1.4445, 0.0), 0.0)};
    const std::array<double, 2>        massArray{0.54321, 0.43211};
    const std::array<double, 4>        thrustArray{0.0, 0.2345678, 9999.3335, 0.0};
    const std::array<std::int32_t, 16> intData{// Time (ms)
                                               0, 4, 0, 123, 412, 1445,
                                               // Mass specific (0.1g)
                                               1, 2, 5432, 4321,
                                               // Thrust (mN)
                                               5, 4, 0, 235, 9999334, 0};

    Md5 correct;
    for (const std::int32_t value : intData)
    {
        const auto                     bits = static_cast<std::uint32_t>(value);
        const std::array<std::byte, 4> bytes{static_cast<std::byte>((bits >> 24U) & 0xFFU),
                                             static_cast<std::byte>((bits >> 16U) & 0xFFU),
                                             static_cast<std::byte>((bits >> 8U) & 0xFFU),
                                             static_cast<std::byte>(bits & 0xFFU)};
        correct.update(bytes);
    }

    MotorDigest motor;
    motor.update(DataType::TIME_ARRAY, timeArray);
    motor.update(DataType::MASS_SPECIFIC, massArray);
    motor.update(DataType::FORCE_PER_TIME, thrustArray);

    EXPECT_EQ(QtRocket::Strings::hexString(correct.finish()), motor.getDigest());
}

TEST(MotorDigest, CommentDigest)
{
    EXPECT_EQ(md5Hex("Hello world!"), MotorDigest::digestComment("Hello  world! "));
    EXPECT_EQ(md5Hex("Hello world!"), MotorDigest::digestComment("\nHello\tworld!\n\r"));
    EXPECT_EQ(md5Hex("Hello world!"), MotorDigest::digestComment("Hello\r\r\r\nworld!"));
    EXPECT_EQ(md5Hex("Hello\u00E4 world!"), MotorDigest::digestComment("Hello\u00E4\r\r\nworld!"));
}

// ---- Digests pinned by running OpenRocket's MotorDigest on the same data ----

TEST(MotorDigest, DigestMotorMatchesOpenRocket)
{
    EXPECT_EQ(MotorDigest::digestMotor(a8Motor()), "4375744555a4e870668294eb16fb1e23");
}

TEST(MotorDigest, RoundingEdgeCasesMatchOpenRocket)
{
    // Negative values, ties (Math.round rounds half up), values whose (long) does not fit an int
    // (the (int) cast keeps the low 32 bits), NaN (0), the infinities (Long.MAX_VALUE and
    // Long.MIN_VALUE, whose low bits are -1 and 0), negative zero and a value below the epsilon.
    MotorDigest digest;
    digest.update(
        DataType::TIME_ARRAY,
        {-1.5, -0.0005, 0.0005, 0.00025, 2.5e-4, 1e7, -3e6,
         std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
         -std::numeric_limits<double>::infinity(), 0.0014995, -0.0, 4294967.296, 1.0e-12});
    EXPECT_EQ(digest.getDigest(), "3df5cc8c53a23274116c16da99270bc3");

    MotorDigest second;
    second.update(DataType::MASS_SPECIFIC, {0.12345, 0.00005, -0.00005, 214748.3647, 214748.3648});
    second.update(DataType::CG_SPECIFIC, {0.0005, 1.0});
    EXPECT_EQ(second.getDigest(), "abb0fa781ca5bcde2721862a4d3a3fb8");
}

TEST(MotorDigest, EmptyDigestsMatchOpenRocket)
{
    MotorDigest nothing;
    EXPECT_EQ(nothing.getDigest(), "d41d8cd98f00b204e9800998ecf8427e");

    MotorDigest emptyBlock;
    emptyBlock.update(DataType::FORCE_PER_TIME, std::span<const double>{});
    EXPECT_EQ(emptyBlock.getDigest(), "d58add6722cbbbeefb7ae76926855684");
}

TEST(MotorDigest, CommentDigestsMatchOpenRocket)
{
    EXPECT_EQ(MotorDigest::digestComment("Hello  world! "), "86fb269d190d2c85f6e0468ceca42a20");
    // \x0B and \f are whitespace to the regex; \x1C is not, but trim() would take it at an end.
    EXPECT_EQ(MotorDigest::digestComment("\x0B"
                                         "A\fB\x1C"
                                         "C"),
              "b15c4916130a7e3cc8b90759301cca94");
    EXPECT_EQ(MotorDigest::digestComment(""), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(MotorDigest::digestComment(" \t\n"), "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(MotorDigest, CompatibleWithEveryHistoricalLayout)
{
    const ThrustCurveMotor a8 = a8Motor();

    // The motor's own digest, then the layouts of OpenRocket's isDigestCompatible, pinned from
    // OpenRocket.
    for (const std::string_view digest :
         {"digestA8-3", "22aec01287ea1e3b8c6f66b26fe5fea6", "80494908fb300f863b12e6231395c6de",
          "2308a677de6f56458222a9a7f09f37f0", "c6f80f1392d424be308f3266c5255899",
          "c511db3f35de08159a805b04feb8160d"})
    {
        EXPECT_TRUE(MotorDigest::isDigestCompatible(a8, digest)) << digest;
    }
    // digestMotor()'s own layout (mass and CG per time together) is not among them: OpenRocket
    // only accepts it as the motor's stored digest.
    EXPECT_FALSE(MotorDigest::isDigestCompatible(a8, "4375744555a4e870668294eb16fb1e23"));
    EXPECT_FALSE(MotorDigest::isDigestCompatible(a8, "0123"));
    EXPECT_FALSE(MotorDigest::isDigestCompatible(a8, ""));
}

TEST(MotorDigest, LayoutsAreTheOnesTheLoadersBuild)
{
    const ThrustCurveMotor a8 = a8Motor();
    std::vector<double>    mass;
    std::vector<double>    cgx;
    for (const Coordinate& cg : a8.getCGPoints())
    {
        mass.push_back(cg.weight);
        cgx.push_back(cg.x);
    }

    // RASP files: endpoint masses.
    MotorDigest massSpecific;
    massSpecific.update(DataType::TIME_ARRAY, a8.getTimePoints());
    massSpecific.update(DataType::MASS_SPECIFIC, {a8.getLaunchMass(), a8.getBurnoutMass()});
    massSpecific.update(DataType::FORCE_PER_TIME, a8.getThrustPoints());
    EXPECT_EQ(massSpecific.getDigest(), "22aec01287ea1e3b8c6f66b26fe5fea6");

    MotorDigest massPerTime;
    massPerTime.update(DataType::TIME_ARRAY, a8.getTimePoints());
    massPerTime.update(DataType::MASS_PER_TIME, mass);
    massPerTime.update(DataType::FORCE_PER_TIME, a8.getThrustPoints());
    EXPECT_EQ(massPerTime.getDigest(), "c6f80f1392d424be308f3266c5255899");

    MotorDigest cgPerTime;
    cgPerTime.update(DataType::TIME_ARRAY, a8.getTimePoints());
    cgPerTime.update(DataType::CG_PER_TIME, cgx);
    cgPerTime.update(DataType::FORCE_PER_TIME, a8.getThrustPoints());
    EXPECT_EQ(cgPerTime.getDigest(), "c511db3f35de08159a805b04feb8160d");
}

TEST(MotorDigest, OnlyTheOwnDigestIsCompatibleWithOtherMotors)
{
    const OtherMotor other;
    EXPECT_TRUE(MotorDigest::isDigestCompatible(other, "4375744555a4e870668294eb16fb1e23"));
    // The thrust-only layout of the same curve does not apply: it is not a thrust curve motor.
    EXPECT_FALSE(MotorDigest::isDigestCompatible(other, "2308a677de6f56458222a9a7f09f37f0"));
}

TEST(MotorDigest, DataTypeTable)
{
    EXPECT_EQ(MotorDigest::getOrder(DataType::TIME_ARRAY), 0);
    EXPECT_EQ(MotorDigest::getOrder(DataType::MASS_SPECIFIC), 1);
    EXPECT_EQ(MotorDigest::getOrder(DataType::MASS_PER_TIME), 2);
    EXPECT_EQ(MotorDigest::getOrder(DataType::CG_SPECIFIC), 3);
    EXPECT_EQ(MotorDigest::getOrder(DataType::CG_PER_TIME), 4);
    EXPECT_EQ(MotorDigest::getOrder(DataType::FORCE_PER_TIME), 5);

    EXPECT_EQ(MotorDigest::getMultiplier(DataType::TIME_ARRAY), 1000);
    EXPECT_EQ(MotorDigest::getMultiplier(DataType::MASS_SPECIFIC), 10000);
    EXPECT_EQ(MotorDigest::getMultiplier(DataType::MASS_PER_TIME), 10000);
    EXPECT_EQ(MotorDigest::getMultiplier(DataType::CG_SPECIFIC), 1000);
    EXPECT_EQ(MotorDigest::getMultiplier(DataType::CG_PER_TIME), 1000);
    EXPECT_EQ(MotorDigest::getMultiplier(DataType::FORCE_PER_TIME), 1000);
}

TEST(MotorDigest, BlocksMustComeInIncreasingOrder)
{
    MotorDigest digest;
    digest.update(DataType::MASS_PER_TIME, {1.0});
    EXPECT_THROW(digest.update(DataType::TIME_ARRAY, {1.0}), BugError);
    EXPECT_THROW(digest.update(DataType::MASS_PER_TIME, {1.0}), BugError);
    digest.update(DataType::FORCE_PER_TIME, {1.0});
}

TEST(MotorDigest, DigestCanBeTakenOnce)
{
    MotorDigest digest;
    digest.update(DataType::TIME_ARRAY, {0.0, 1.0});
    const std::string first = digest.getDigest();
    EXPECT_EQ(first.size(), 32U);
    EXPECT_THROW(static_cast<void>(digest.getDigest()), BugError);
}

TEST(MotorDigest, RoundingAbsorbsTinyDifferences)
{
    // Values that differ below the digested precision give the same digest.
    MotorDigest a;
    a.update(DataType::TIME_ARRAY, {0.0, 0.1, 0.2});
    MotorDigest b;
    b.update(DataType::TIME_ARRAY, {0.0, 0.1 + 1e-9, 0.2 - 1e-9});
    EXPECT_EQ(a.getDigest(), b.getDigest());

    MotorDigest c;
    c.update(DataType::TIME_ARRAY, {0.0, 0.101, 0.2});
    MotorDigest d;
    d.update(DataType::TIME_ARRAY, {0.0, 0.1, 0.2});
    EXPECT_NE(c.getDigest(), d.getDigest());
}

}  // namespace
