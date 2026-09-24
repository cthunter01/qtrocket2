#include "QtRocket/file/motor/RockSimMotorLoader.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Motor;
using QtRocket::MotorDigest;
using QtRocket::Result;
using QtRocket::RockSimMotorLoader;
using QtRocket::ThrustCurveMotor;

constexpr double kInf = std::numeric_limits<double>::infinity();

// TestMotorLoader.java's digests of test2.rse and test3.rse.
constexpr std::string_view kDigest2 = "6a41f0f10b7283793eb0e6b389753729";
constexpr std::string_view kDigest3 = "e3164a735f9a50500f2725f0a33d246b";

/// OpenRocket's digest of kHead + kData + kTail (pinned by running OpenRocket 5f164fd0e); the same
/// as the RASP A8 of RaspMotorLoaderTests, since mass and CG are computed.
constexpr std::string_view kDigestA8 = "8e6fb7d51ee11a4a56ab77a3467bcffe";

constexpr std::string_view kHead = R"(<engine-database><engine-list><engine mfg="Estes" )"
                                   R"(code="A8-3" dia="18" len="70" initWt="16" propWt="3" )"
                                   R"(delays="3,5">)";
constexpr std::string_view kData = R"(<data><eng-data t="0" f="0" m="3" cg="35"/>)"
                                   R"(<eng-data t="0.5" f="5" m="1" cg="35"/>)"
                                   R"(<eng-data t="1" f="0" m="0" cg="35"/></data>)";
constexpr std::string_view kTail = "</engine></engine-list></engine-database>";

[[nodiscard]] std::string document(std::string_view body)
{
    return std::string(kHead) + std::string(body) + std::string(kTail);
}

/// @p head with the attribute text @p from replaced by @p to.
[[nodiscard]] std::string replaced(std::string_view head, std::string_view from,
                                   std::string_view to)
{
    std::string       text(head);
    const std::size_t at = text.find(from);
    EXPECT_NE(at, std::string::npos) << from;
    if (at != std::string::npos)
    {
        text.replace(at, from.size(), to);
    }
    return text;
}

[[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(std::string_view text)
{
    return RockSimMotorLoader().load(QtRocket::stringToBytes(text), "x.rse");
}

/// The motors of @p text, built; fails the test when loading or building fails.
[[nodiscard]] std::vector<ThrustCurveMotor> loadMotors(std::string_view text)
{
    std::vector<ThrustCurveMotor>                        motors;
    const Result<std::vector<ThrustCurveMotor::Builder>> builders = load(text);
    EXPECT_TRUE(builders) << builders.error().toString();
    if (!builders)
    {
        return motors;
    }
    for (const ThrustCurveMotor::Builder& builder : *builders)
    {
        Result<ThrustCurveMotor> motor = builder.build();
        EXPECT_TRUE(motor) << motor.error().toString();
        if (motor)
        {
            motors.push_back(std::move(*motor));
        }
    }
    return motors;
}

/// The one motor of @p text.
[[nodiscard]] ThrustCurveMotor loadMotor(std::string_view text)
{
    std::vector<ThrustCurveMotor> motors = loadMotors(text);
    EXPECT_EQ(motors.size(), 1U);
    if (motors.empty())
    {
        return loadMotors(document(kData)).front();
    }
    return std::move(motors.front());
}

/// The failure message of loading @p text.
[[nodiscard]] std::string loadError(std::string_view text)
{
    const Result<std::vector<ThrustCurveMotor::Builder>> builders = load(text);
    EXPECT_FALSE(builders) << text;
    if (builders)
    {
        return {};
    }
    EXPECT_EQ(builders.error().code, ErrorCode::PARSE);
    return builders.error().message;
}

/// The standard delays of kHead's engine with its delays attribute replaced by @p delays.
[[nodiscard]] std::vector<double> delaysOf(std::string_view delays)
{
    return loadMotor(replaced(kHead, R"(delays="3,5")", delays) + std::string(kData) +
                     std::string(kTail))
        .getStandardDelays();
}

/// The failure of kHead's engine with the attribute text @p from replaced by @p to.
[[nodiscard]] std::string headError(std::string_view from, std::string_view to)
{
    return loadError(replaced(kHead, from, to) + std::string(kData) + std::string(kTail));
}

[[nodiscard]] std::vector<ThrustCurveMotor> loadFile(std::string_view name)
{
    std::vector<ThrustCurveMotor>        motors;
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / name);
    EXPECT_TRUE(bytes);
    if (!bytes)
    {
        return motors;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> builders =
        RockSimMotorLoader().load(*bytes, name);
    EXPECT_TRUE(builders) << builders.error().toString();
    if (!builders)
    {
        return motors;
    }
    for (const ThrustCurveMotor::Builder& builder : *builders)
    {
        Result<ThrustCurveMotor> motor = builder.build();
        EXPECT_TRUE(motor);
        if (motor)
        {
            motors.push_back(std::move(*motor));
        }
    }
    return motors;
}

// TestMotorLoader.testRockSimMotorLoader
TEST(RockSimMotorLoader, LoadsTest2)
{
    const std::vector<ThrustCurveMotor> motors = loadFile("test2.rse");
    ASSERT_EQ(motors.size(), 1U);
    const ThrustCurveMotor& motor = motors.front();
    EXPECT_EQ(motor.getDigest(), kDigest2);
    EXPECT_EQ(motor.getDesignation(), "G115-13A");  // "13A" is no delay
    EXPECT_EQ(motor.getCommonName(), "G115");
    EXPECT_EQ(motor.getManufacturer().getSimpleName(), "Cesaroni Technology");
    EXPECT_EQ(motor.getMotorType(), Motor::Type::RELOAD);
    EXPECT_EQ(motor.getStandardDelays(), std::vector<double>{13});
    EXPECT_DOUBLE_EQ(motor.getDiameter(), 0.038);
    EXPECT_DOUBLE_EQ(motor.getLength(), 0.127);
    EXPECT_EQ(motor.getTimePoints().size(), 39U);
    // auto-calc-mass and auto-calc-cg: the file's masses and CGs are replaced.
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.195);
    EXPECT_DOUBLE_EQ(motor.getLaunchCGx(), 0.0635);
}

// TestMotorLoader.testRockSimMotorLoader3
TEST(RockSimMotorLoader, LoadsTest3)
{
    const std::vector<ThrustCurveMotor> motors = loadFile("test3.rse");
    ASSERT_EQ(motors.size(), 1U);
    const ThrustCurveMotor& motor = motors.front();
    EXPECT_EQ(motor.getDigest(), kDigest3);
    EXPECT_EQ(motor.getDescription(), "Water Rocket, 90 psi, 800g");
    EXPECT_EQ(motor.getStandardDelays(), std::vector<double>{kInf});  // 1000 is plugged
    // auto-calc-cg="0": the file's CGs are kept.
    EXPECT_DOUBLE_EQ(motor.getLaunchCGx(), 0.0816327);
    EXPECT_DOUBLE_EQ(motor.getBurnoutCGx(), 0.192857);
}

TEST(RockSimMotorLoader, ReadsAnEngine)
{
    const ThrustCurveMotor motor = loadMotor(document(kData));
    EXPECT_EQ(motor.getDigest(), kDigestA8);
    EXPECT_EQ(motor.getDesignation(), "A8");
    EXPECT_EQ(motor.getManufacturer().getSimpleName(), "Estes");
    EXPECT_EQ(motor.getMotorType(), Motor::Type::SINGLE);  // from the manufacturer
    EXPECT_EQ(motor.getStandardDelays(), (std::vector<double>{3, 5}));
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.016);
    EXPECT_DOUBLE_EQ(motor.getLaunchCGx(), 0.035);
    EXPECT_EQ(motor.getDescription(), "");
}

TEST(RockSimMotorLoader, ExplicitMassAndCg)
{
    const ThrustCurveMotor motor =
        loadMotor(replaced(kHead, R"(delays="3,5")",
                           R"(delays="3,5" auto-calc-mass="0" auto-calc-cg="FALSE")") +
                  R"(<data><eng-data t="0" f="0" m="3" cg="35"/>)"
                  R"(<eng-data t="0.5" f="5" m="1.5" cg="36"/>)"
                  R"(<eng-data t="1" f="0" m="0" cg="37"/></data>)" +
                  std::string(kTail));
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.003);
    EXPECT_DOUBLE_EQ(motor.getBurnoutMass(), 0.0);
    EXPECT_DOUBLE_EQ(motor.getLaunchCGx(), 0.035);
    EXPECT_DOUBLE_EQ(motor.getBurnoutCGx(), 0.037);
    // The digest then covers the masses and CGs point by point.
    EXPECT_EQ(motor.getDigest(), MotorDigest::digestMotor(motor));
}

TEST(RockSimMotorLoader, AMissingOrInvalidValueSwitchesTheCalculationOn)
{
    const std::string head =
        replaced(kHead, R"(delays="3,5")", R"(delays="3,5" auto-calc-mass="0" auto-calc-cg="0")");
    const ThrustCurveMotor motor = loadMotor(head +
                                             R"(<data><eng-data t="0" f="0" cg="35"/>)"
                                             R"(<eng-data t="0.5" f="5" m="1" cg="x"/>)"
                                             R"(<eng-data t="1" f="0" m="0" cg="35"/></data>)" +
                                             std::string(kTail));
    EXPECT_EQ(motor.getDigest(), kDigestA8);
    EXPECT_DOUBLE_EQ(motor.getLaunchMass(), 0.016);
    EXPECT_DOUBLE_EQ(motor.getBurnoutCGx(), 0.035);
}

TEST(RockSimMotorLoader, MotorType)
{
    const auto typeOf = [](std::string_view typeAttribute) {
        return loadMotor(replaced(kHead, "delays=", std::string(typeAttribute) + " delays=") +
                         std::string(kData) + std::string(kTail))
            .getMotorType();
    };
    EXPECT_EQ(typeOf(R"(Type="single-use")"), Motor::Type::SINGLE);
    EXPECT_EQ(typeOf(R"(Type="Reloadable")"), Motor::Type::RELOAD);
    EXPECT_EQ(typeOf(R"(Type="HYBRID")"), Motor::Type::HYBRID);
    // An explicit unknown type stays; any other value takes the manufacturer's.
    EXPECT_EQ(typeOf(R"(Type="unknown")"), Motor::Type::UNKNOWN);
    EXPECT_EQ(typeOf(R"(Type="bogus")"), Motor::Type::SINGLE);
    EXPECT_EQ(typeOf(R"(type="unknown")"), Motor::Type::SINGLE);  // the attribute is "Type"
}

TEST(RockSimMotorLoader, Delays)
{
    EXPECT_EQ(delaysOf(R"(delays="5,3")"), (std::vector<double>{5, 3}));  // not sorted
    EXPECT_EQ(delaysOf(R"(delays=" 4 ,89,90,1000")"), (std::vector<double>{4, 89, kInf, kInf}));
    EXPECT_TRUE(delaysOf(R"(delays="")").empty());
    EXPECT_TRUE(delaysOf("").empty());
    const std::vector<double> nan = delaysOf(R"(delays="NaN,100")");
    ASSERT_EQ(nan.size(), 2U);
    EXPECT_TRUE(std::isnan(nan[0]));
    EXPECT_EQ(nan[1], kInf);
}

TEST(RockSimMotorLoader, PluggedDelays)
{
    EXPECT_EQ(delaysOf(R"(delays="P")"), std::vector<double>{kInf});
    EXPECT_EQ(delaysOf(R"(delays="plugged")"), std::vector<double>{kInf});
    // OpenRocket compares the whole attribute with "P", so a P among numbers is dropped.
    EXPECT_EQ(delaysOf(R"(delays="3,P")"), std::vector<double>{3});
}

TEST(RockSimMotorLoader, Comments)
{
    EXPECT_EQ(
        loadMotor(document("<comments>  hello  </comments>" + std::string(kData))).getDescription(),
        "hello");
    EXPECT_EQ(
        loadMotor(document("<comments>one</comments><comments>two</comments>" + std::string(kData)))
            .getDescription(),
        "one\n\ntwo");
    EXPECT_EQ(
        loadMotor(document("<comments>a &amp; b &lt;c&gt; &#233;</comments>" + std::string(kData)))
            .getDescription(),
        "a & b <c> \u00E9");
    EXPECT_EQ(loadMotor(document("<comments><![CDATA[x <y> z]]></comments>" + std::string(kData)))
                  .getDescription(),
              "x <y> z");
    EXPECT_EQ(loadMotor(document("<comments>line1\r\nline2\rline3</comments>" + std::string(kData)))
                  .getDescription(),
              "line1\nline2\nline3");
    EXPECT_EQ(loadMotor(document("<comments>a<!-- note -->b</comments>" + std::string(kData)))
                  .getDescription(),
              "ab");
}

TEST(RockSimMotorLoader, KeepsSimpleSaxsStackQuirk)
{
    // An ignored element leaves its text buffer on DelegatorHandler's stack, so
    // <comments> is closed with the text after its last child element (pinned
    // against OpenRocket).
    EXPECT_EQ(loadMotor(document("<comments>abc<b>bold</b>def</comments>" + std::string(kData)))
                  .getDescription(),
              "def");
    EXPECT_EQ(loadMotor(document("<comments>A<x/>B<y/>C</comments>" + std::string(kData)))
                  .getDescription(),
              "C");
    // ... and its attributes, so an <eng-data> with a child element loses its
    // own.
    EXPECT_EQ(loadError(document(R"(<data><eng-data t="0" f="0" m="3" cg="35"><foo/></eng-data>)"
                                 R"(<eng-data t="1" f="0" m="0" cg="35"/></data>)")),
              "Illegal motor data point encountered");
    // Where the parent does not use them, nothing changes.
    EXPECT_EQ(loadMotor(document(R"(<data><foo t="9" f="9"/><eng-data t="0" f="0" m="3" cg="35"/>)"
                                 R"(<eng-data t="0.5" f="5" m="1" cg="35"/>)"
                                 R"(<eng-data t="1" f="0" m="0" cg="35"/></data>)"))
                  .getDigest(),
              kDigestA8);
    EXPECT_EQ(loadMotor(document("<foo/><comments>hello</comments>" + std::string(kData)))
                  .getDescription(),
              "hello");
}

TEST(RockSimMotorLoader, SkipsUnknownElements)
{
    EXPECT_EQ(loadMotor(R"(<engine-database><version>1</version><engine-list><foo mfg="x"/>)" +
                        std::string(kHead.substr(kHead.find("<engine "))) + std::string(kData) +
                        std::string(kTail))
                  .getDigest(),
              kDigestA8);
    // Namespace prefixes are dropped, as a namespace-aware SAX parser reports
    // local names.
    const ThrustCurveMotor prefixed = loadMotor(
        R"(<r:engine-database xmlns:r="urn:x"><r:engine-list><r:engine mfg="Estes" code="A8-3" )"
        R"(dia="18" len="70" initWt="16" propWt="3"><r:data><r:eng-data t="0" f="0" m="3" )"
        R"(cg="35"/><r:eng-data t="1" f="2" m="0" cg="35"/></r:data></r:engine></r:engine-list>)"
        R"(</r:engine-database>)");
    EXPECT_EQ(prefixed.getDigest(), "aa6ea8324e89e438508868ceefb8c53e");
    EXPECT_EQ(prefixed.getTimePoints().size(), 2U);
    // A document of another kind holds no motors.
    const Result<std::vector<ThrustCurveMotor::Builder>> other = load("<a>\xE9</a>");
    ASSERT_TRUE(other);
    EXPECT_TRUE(other->empty());
}

TEST(RockSimMotorLoader, ReadsSeveralEngines)
{
    const std::string second =
        R"(<engine mfg="Estes" code="B4-2" dia="18" len="70" initWt="20" propWt="4">)"
        R"(<data><eng-data t="0.2" f="3"/><eng-data t="0.8" f="0"/></data></engine>)";
    const std::vector<ThrustCurveMotor> motors =
        loadMotors(std::string(kHead) + std::string(kData) + "</engine>" + second +
                   "</engine-list></engine-database>");
    ASSERT_EQ(motors.size(), 2U);
    EXPECT_EQ(motors[0].getDesignation(), "A8");
    EXPECT_EQ(motors[1].getDesignation(), "B4");
    EXPECT_EQ(motors[1].getTimePoints(), (std::vector<double>{0, 0.2, 0.8}));
}

TEST(RockSimMotorLoader, IgnoresTheDeclaredEncodingAndReadsUtf8)
{
    EXPECT_EQ(loadMotor(R"(<?xml version="1.0" encoding="ISO-8859-1"?>)" +
                        document("<comments>\u00E9</comments>" + std::string(kData)))
                  .getDescription(),
              "\u00E9");
    // A malformed byte becomes U+FFFD.
    EXPECT_EQ(
        loadMotor(document("<comments>caf\xE9</comments>" + std::string(kData))).getDescription(),
        "caf\uFFFD");
}

TEST(RockSimMotorLoader, AttributeWhitespaceIsNormalised)
{
    EXPECT_EQ(loadMotor(replaced(kHead, R"(dia="18")", "dia=\"\t18\n\"") + std::string(kData) +
                        std::string(kTail))
                  .getDigest(),
              kDigestA8);
}

TEST(RockSimMotorLoader, RejectsMalformedXml)
{
    EXPECT_EQ(loadError(""), "Premature end of file.");
    EXPECT_EQ(loadError("   \n"), "Premature end of file.");
    EXPECT_EQ(loadError(document(kData) + "<x/>"),
              "The markup in the document following the root element must be "
              "well-formed.");
    EXPECT_EQ(loadError(document(kData) + "junk"), "Content is not allowed in trailing section.");
    EXPECT_EQ(loadError("junk" + document(kData)), "Content is not allowed in prolog.");
    // Java's InputStreamReader keeps a byte-order mark, which the parser rejects.
    EXPECT_EQ(loadError("\xEF\xBB\xBF" + document(kData)), "Content is not allowed in prolog.");
    EXPECT_EQ(loadError(replaced(kHead, R"(dia="18")", R"(dia="18" dia="19")") +
                        std::string(kData) + std::string(kTail)),
              R"(Attribute "dia" was already specified for element "engine".)");
    EXPECT_FALSE(load(document(kData).substr(0, 60)));                       // truncated
    EXPECT_FALSE(load("<engine-database><engine-list></engine-database>"));  // mismatched
}

TEST(RockSimMotorLoader, RejectsMissingAttributes)
{
    EXPECT_EQ(headError(R"(mfg="Estes" )", ""), "Manufacturer missing");
    EXPECT_EQ(headError(R"(code="A8-3" )", ""), "Designation missing");
    EXPECT_EQ(headError(R"(dia="18" )", ""), "Diameter missing");
    EXPECT_EQ(headError(R"(len="70" )", ""), "Length missing");
    EXPECT_EQ(headError(R"(initWt="16" )", ""), "Initial mass missing");
    EXPECT_EQ(headError(R"(propWt="3" )", ""), "Propellant mass missing");
    // The attributes are checked in OpenRocket's order.
    EXPECT_EQ(headError(R"(mfg="Estes" code="A8-3")", ""), "Manufacturer missing");
}

TEST(RockSimMotorLoader, RejectsInvalidAttributes)
{
    EXPECT_EQ(headError(R"(dia="18")", R"(dia="18mm")"), "Invalid diameter 18mm");
    EXPECT_EQ(headError(R"(len="70")", R"(len="")"), "Invalid length ");
    EXPECT_EQ(headError(R"(initWt="16")", R"(initWt="Inf")"), "Invalid initial mass Inf");
    EXPECT_EQ(headError(R"(propWt="3")", R"(propWt="3,5")"), "Invalid propellant mass 3,5");
    EXPECT_EQ(headError(R"(propWt="3")", R"(propWt="17")"),
              "Propellant weight exceeds total weight in RockSim engine format");
}

TEST(RockSimMotorLoader, RejectsBadData)
{
    EXPECT_EQ(loadError(document(std::string(kData) + std::string(kData))),
              "Multiple data elements encountered in motor definition");
    EXPECT_EQ(loadError(document("")), "Illegal motor data");
    EXPECT_EQ(loadError(document("<data></data>")), "Illegal motor data");
    EXPECT_EQ(loadError(document(R"(<data><eng-data f="0"/><eng-data t="1" f="0"/></data>)")),
              "Illegal motor data point encountered");
    EXPECT_EQ(loadError(document(R"(<data><eng-data t="0" f="x"/><eng-data t="1" f="0"/></data>)")),
              "Illegal motor data point encountered");
    EXPECT_EQ(loadError(document(R"(<data><eng-data t="NaN" f="0"/></data>)")),
              "Illegal motor data point encountered");
    // A data point in the wrong place is an unknown element, skipped.
    EXPECT_EQ(loadError(document(R"(<eng-data t="0" f="0"/>)")), "Illegal motor data");
}

TEST(RockSimMotorLoader, OnePointCurves)
{
    // OpenRocket throws IndexOutOfBoundsException for a single point at time 0.
    EXPECT_EQ(loadError(document(R"(<data><eng-data t="0" f="0" m="3" cg="35"/></data>)")),
              "Index 1 out of bounds for length 1");
    // Two points at time 0: the first goes from the times and thrusts but not
    // from the masses and CGs, which OpenRocket then digests misaligned and one
    // longer (pinned against OpenRocket).
    const ThrustCurveMotor motor =
        loadMotor(document(R"(<data><eng-data t="0" f="0" m="3" cg="35"/>)"
                           R"(<eng-data t="0" f="5" m="2" cg="35"/>)"
                           R"(<eng-data t="1" f="0" m="0" cg="35"/></data>)"));
    EXPECT_EQ(motor.getDigest(), "015d41ee8ca9dba40ba6182ccfb074c6");
    EXPECT_EQ(motor.getTimePoints(), (std::vector<double>{0, 1}));
}

TEST(RockSimMotorLoader, SortsTheDataPoints)
{
    const ThrustCurveMotor motor =
        loadMotor(document(R"(<data><eng-data t="1" f="0" m="0" cg="35"/>)"
                           R"(<eng-data t="0" f="0" m="3" cg="35"/>)"
                           R"(<eng-data t="0.5" f="5" m="1" cg="35"/>)"
                           R"(</data>)"));
    EXPECT_EQ(motor.getDigest(), kDigestA8);
    EXPECT_EQ(motor.getTimePoints(), (std::vector<double>{0, 0.5, 1}));
}

TEST(RockSimMotorLoader, SurvivesDeepNesting)
{
    std::string   deep;
    constexpr int kDepth = 100000;
    for (int i = 0; i < kDepth; i++)
    {
        deep += "<x>";
    }
    for (int i = 0; i < kDepth; i++)
    {
        deep += "</x>";
    }
    EXPECT_EQ(loadMotor(document(deep + std::string(kData))).getDigest(), kDigestA8);
}

}  // namespace
