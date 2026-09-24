#include "QtRocket/logging/Warning.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "logging/TestSources.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Message;
using QtRocket::MessagePriority;
using QtRocket::MessageSources;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::Test::source;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

constexpr std::array<const Warning::Other*, 24> kAllConstants{&Warning::kDiameterDiscontinuity,
                                                              &Warning::kOpenAirframeForward,
                                                              &Warning::kAirframeGap,
                                                              &Warning::kAirframeOverlap,
                                                              &Warning::kPodsetForward,
                                                              &Warning::kPodsetOverlap,
                                                              &Warning::kThickFin,
                                                              &Warning::kJaggedEdgedFin,
                                                              &Warning::kZeroAreaFin,
                                                              &Warning::kListenersAffected,
                                                              &Warning::kNoRecoveryDevice,
                                                              &Warning::kFileInvalidParameter,
                                                              &Warning::kParallelFins,
                                                              &Warning::kSupersonic,
                                                              &Warning::kRecoveryLaunchRod,
                                                              &Warning::kTumbleUnderThrust,
                                                              &Warning::kZeroVolumeBody,
                                                              &Warning::kTubeIsolated,
                                                              &Warning::kTubeSeparation,
                                                              &Warning::kTubeOverlap,
                                                              &Warning::kObjZeroThickness,
                                                              &Warning::kSeparationOrder,
                                                              &Warning::kEarlySeparation,
                                                              &Warning::kEmptyBranch};

/// Checks a fixed-text constant against OpenRocket's text and priority, and that the .ork loader
/// gets it back from its text and priority (Java: fromString(text) then setPriority()).
void expectConstant(const Warning::Other& warning, std::string_view text, MessagePriority priority)
{
    EXPECT_EQ(warning.messageDescription(), text);
    EXPECT_EQ(warning.toString(), text);  // no sources
    EXPECT_EQ(warning.priority(), priority);
    EXPECT_EQ(warning.typeName(), "Other");
    const Warning::Other rebuilt = Warning::fromString(std::string{text}, priority);
    EXPECT_TRUE(rebuilt == warning && warning == rebuilt);
    // Without the priority, fromString() only matches the NORMAL constants.
    EXPECT_EQ(Warning::fromString(std::string{text}) == warning,
              priority == MessagePriority::NORMAL);
}

TEST(Warning, ConstantsHaveOpenRocketTextAndRoundTripThroughFromString)
{
    expectConstant(Warning::kDiameterDiscontinuity, "Discontinuity in rocket body diameter",
                   MessagePriority::LOW);
    expectConstant(Warning::kOpenAirframeForward, "Open forward airframe (diameter > 0)",
                   MessagePriority::LOW);
    expectConstant(Warning::kAirframeGap, "Gap in rocket airframe", MessagePriority::LOW);
    expectConstant(Warning::kAirframeOverlap, "Overlap in airframe components",
                   MessagePriority::LOW);
    expectConstant(Warning::kPodsetForward, "In-line podset forward of parent airframe component",
                   MessagePriority::LOW);
    expectConstant(Warning::kPodsetOverlap, "In-line podset overlaps parent airframe component",
                   MessagePriority::LOW);
    expectConstant(Warning::kThickFin, "Thick fins may not simulate accurately",
                   MessagePriority::LOW);
    expectConstant(Warning::kJaggedEdgedFin, "Jagged-edged fin predictions may be inaccurate",
                   MessagePriority::LOW);
    expectConstant(Warning::kZeroAreaFin, "Fins with zero area will not affect aerodynamics",
                   MessagePriority::LOW);
    expectConstant(Warning::kListenersAffected, "Listeners modified the flight simulation",
                   MessagePriority::LOW);
    expectConstant(Warning::kNoRecoveryDevice, "No recovery device defined in the simulation.",
                   MessagePriority::HIGH);
    expectConstant(Warning::kFileInvalidParameter, "Invalid parameter encountered, ignoring.",
                   MessagePriority::NORMAL);
    expectConstant(Warning::kParallelFins, "Too many parallel fins", MessagePriority::LOW);
    expectConstant(Warning::kSupersonic,
                   "Body calculations may not be entirely accurate at supersonic speeds.",
                   MessagePriority::NORMAL);
    expectConstant(Warning::kRecoveryLaunchRod,
                   "Recovery device deployed while on the launch guide.", MessagePriority::HIGH);
    expectConstant(Warning::kTumbleUnderThrust, "Stage began to tumble under thrust.",
                   MessagePriority::HIGH);
    expectConstant(Warning::kZeroVolumeBody, "Zero-volume bodies may not simulate accurately",
                   MessagePriority::LOW);
    expectConstant(Warning::kTubeIsolated, "Isolated tube fins may not simulate accurately",
                   MessagePriority::LOW);
    expectConstant(Warning::kTubeSeparation, "Space between tube fins may not simulate accurately",
                   MessagePriority::LOW);
    expectConstant(Warning::kTubeOverlap, "Overlapping tube fins may not simulate accurately",
                   MessagePriority::LOW);
    expectConstant(Warning::kObjZeroThickness,
                   "Zero-thickness component can cause issues for 3D printing",
                   MessagePriority::LOW);
    expectConstant(Warning::kSeparationOrder, "Stages separated in an unreasonable order",
                   MessagePriority::NORMAL);
    expectConstant(Warning::kEarlySeparation, "Stages separated before clearing launch rod/rail",
                   MessagePriority::HIGH);
    expectConstant(Warning::kEmptyBranch, "Simulation branch contains no data",
                   MessagePriority::HIGH);
}

TEST(Warning, ConstantsAreDistinctKinds)
{
    for (const Warning::Other* lhs : kAllConstants)
    {
        for (const Warning::Other* rhs : kAllConstants)
        {
            EXPECT_EQ(*lhs == *rhs, lhs == rhs)
                << lhs->messageDescription() << " vs " << rhs->messageDescription();
        }
    }
}

TEST(Warning, OtherComparesTextPriorityAndSources)
{
    const Warning::Other a{"text"};
    EXPECT_EQ(a.priority(), MessagePriority::NORMAL);
    EXPECT_EQ(a.description(), "text");
    EXPECT_TRUE(a == Warning::Other{"text"});
    EXPECT_FALSE(a == Warning::Other{"other text"});
    EXPECT_FALSE((a == Warning::Other{"text", MessagePriority::HIGH}));
    Warning::Other withSources{"text"};
    withSources.setSources(MessageSources{source("fs-1", "Fin set")});
    EXPECT_FALSE(a == withSources);
    EXPECT_TRUE(withSources == Warning::Other{withSources});
    EXPECT_FALSE(a.replaceBy(withSources));
    EXPECT_THROW(Warning::Other{"x"}.replaceContents(a), BugError);
}

TEST(Warning, LargeAOAFormatsDegrees)
{
    const Warning::LargeAOA unknown{kNaN};
    EXPECT_EQ(unknown.messageDescription(), "Large angle of attack encountered.");
    EXPECT_EQ(unknown.priority(), MessagePriority::LOW);
    EXPECT_EQ(unknown.typeName(), "LargeAOA");
    EXPECT_TRUE(std::isnan(unknown.aoa()));
    const Warning::LargeAOA fifteen{std::numbers::pi / 12.0};
    EXPECT_EQ(fifteen.messageDescription(), "Large angle of attack encountered (15°)");
    const Warning::LargeAOA finer{0.3};  // 17.19 degrees
    EXPECT_EQ(finer.messageDescription(), "Large angle of attack encountered (17.2°)");
    EXPECT_DOUBLE_EQ(finer.aoa(), 0.3);
    const Warning::LargeAOA negative{-0.3};
    EXPECT_EQ(negative.messageDescription(), "Large angle of attack encountered (-17.2°)");
    EXPECT_EQ(Warning::LargeAOA{0.0}.messageDescription(),
              "Large angle of attack encountered (0°)");
    EXPECT_EQ(Warning::LargeAOA{kInf}.messageDescription(),
              "Large angle of attack encountered (∞°)");
    EXPECT_EQ(Warning::LargeAOA{-kInf}.messageDescription(),
              "Large angle of attack encountered (-∞°)");
}

TEST(Warning, LargeAOAIsReplacedByALargerAngle)
{
    const Warning::LargeAOA unknown{kNaN};
    const Warning::LargeAOA small{0.1};
    const Warning::LargeAOA large{0.3};
    EXPECT_TRUE(small == large);  // equal whatever the angle
    EXPECT_TRUE(small.replaceBy(large));
    EXPECT_FALSE(large.replaceBy(small));
    EXPECT_FALSE(large.replaceBy(Warning::LargeAOA{0.3}));
    EXPECT_TRUE(unknown.replaceBy(small));   // an unknown angle gives way to any angle
    EXPECT_FALSE(small.replaceBy(unknown));  // NaN > x is false
    EXPECT_FALSE(small.replaceBy(Warning::kSupersonic));
    Warning::LargeAOA target{0.1};
    target.replaceContents(large);
    EXPECT_DOUBLE_EQ(target.aoa(), 0.3);
    EXPECT_THROW(target.replaceContents(Warning::kSupersonic), BugError);
}

TEST(Warning, SpeedWarningsFormatMetresPerSecond)
{
    const Warning::RecoveryHighSpeedDeployment high{38.27,
                                                    MessageSources{source("main-1", "Main")}};
    EXPECT_EQ(high.messageDescription(), "Recovery device deployment at high speed (38.3 m/s)");
    EXPECT_EQ(high.toString(), "Recovery device deployment at high speed (38.3 m/s):  \"Main\"");
    EXPECT_DOUBLE_EQ(high.speed(), 38.27);
    EXPECT_EQ(high.priority(), MessagePriority::NORMAL);
    EXPECT_EQ(high.typeName(), "RecoveryHighSpeedDeployment");
    EXPECT_EQ(Warning::RecoveryHighSpeedDeployment{kNaN}.messageDescription(),
              "Recovery device deployment at high speed");
    EXPECT_EQ(Warning::HighSpeedMainDeployment{123.4}.messageDescription(),
              "Main parachute deployment at high speed (123 m/s)");
    EXPECT_EQ(Warning::LowSpeedMainDeployment{2.0}.messageDescription(),
              "Main parachute deployment at low speed (2 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{0.456}.messageDescription(),
              "Drogue deployment at low speed at apogee (0.456 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{0.0004}.messageDescription(),
              "Drogue deployment at low speed at apogee (0 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{2.5e6}.messageDescription(),
              "Drogue deployment at low speed at apogee (2.50E6 m/s)");
}

TEST(Warning, SpeedWarningsFormatSmallAndNegativeSpeeds)
{
    // Unit.toString(): three significant digits and at most three decimals below 100 ...
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{0.0123}.messageDescription(),
              "Drogue deployment at low speed at apogee (0.012 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{0.00051}.messageDescription(),
              "Drogue deployment at low speed at apogee (0.001 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{99.96}.messageDescription(),
              "Drogue deployment at low speed at apogee (100 m/s)");
    // ... and the sign in front on every branch.
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{-38.27}.messageDescription(),
              "Drogue deployment at low speed at apogee (-38.3 m/s)");
    EXPECT_EQ(Warning::HighSpeedMainDeployment{-123.4}.messageDescription(),
              "Main parachute deployment at high speed (-123 m/s)");
    EXPECT_EQ(Warning::LowSpeedMainDeployment{-0.0004}.messageDescription(),
              "Main parachute deployment at low speed (0 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{-2.5e6}.messageDescription(),
              "Drogue deployment at low speed at apogee (-2.50E6 m/s)");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{-1.0e-7}.messageDescription(),
              "Drogue deployment at low speed at apogee (0 m/s)");
}

TEST(Warning, InfinitiesPrintTheInfinitySymbol)
{
    // DecimalFormat prints the infinity symbol, sign in front, whatever the pattern.
    EXPECT_EQ(Warning::RecoveryHighSpeedDeployment{kInf}.messageDescription(),
              "Recovery device deployment at high speed (∞ m/s)");
    EXPECT_EQ(Warning::LowSpeedMainDeployment{-kInf}.messageDescription(),
              "Main parachute deployment at low speed (-∞ m/s)");
    EXPECT_EQ(Warning::HighSpeedMainDeployment{kInf}.toString(),
              "Main parachute deployment at high speed (∞ m/s)");
}

TEST(Warning, SpeedWarningsAreEqualWhateverTheSpeed)
{
    const Warning::HighSpeedMainDeployment one{1.0};
    const Warning::HighSpeedMainDeployment two{2.0};
    EXPECT_TRUE(one == two);
    EXPECT_FALSE(one.replaceBy(two));  // the first one stays
    EXPECT_FALSE(one == Warning::LowSpeedMainDeployment{1.0});
    EXPECT_FALSE(one == Warning::RecoveryHighSpeedDeployment{1.0});
    EXPECT_FALSE(one == Warning::LowSpeedDrogueDeployment{1.0});
    EXPECT_EQ(Warning::LowSpeedMainDeployment{1.0}.typeName(), "LowSpeedMainDeployment");
    EXPECT_EQ(Warning::LowSpeedDrogueDeployment{1.0}.typeName(), "LowSpeedDrogueDeployment");
    EXPECT_EQ(Warning::HighSpeedMainDeployment{1.0}.priority(), MessagePriority::NORMAL);
    const Warning::HighSpeedMainDeployment withChute{1.0, MessageSources{source("main-1", "Main")}};
    EXPECT_FALSE(one == withChute);  // sources differ
    EXPECT_EQ(withChute.sources(), (MessageSources{source("main-1", "Main")}));
}

TEST(Warning, RecoveryDrogueWithoutMainIsCritical)
{
    const Warning::RecoveryDrogueWithoutMain warning;
    EXPECT_EQ(warning.messageDescription(), "Drogue configured but no main parachute present");
    EXPECT_EQ(warning.priority(), MessagePriority::HIGH);
    EXPECT_EQ(warning.typeName(), "RecoveryDrogueWithoutMain");
    EXPECT_TRUE(warning == Warning::RecoveryDrogueWithoutMain{});
    EXPECT_FALSE(warning.replaceBy(Warning::RecoveryDrogueWithoutMain{}));
}

TEST(Warning, EventAfterLandingIsEqualOnlyToItself)
{
    const Warning::EventAfterLanding apogee{"Apogee"};
    const Warning::EventAfterLanding another{"Apogee"};
    EXPECT_EQ(apogee.messageDescription(), "Flight Event occurred after landing: Apogee");
    EXPECT_EQ(Warning::EventAfterLanding{}.messageDescription(),
              "Flight Event occurred after landing: ");
    EXPECT_EQ(apogee.priority(), MessagePriority::HIGH);
    EXPECT_EQ(apogee.typeName(), "EventAfterLanding");
    EXPECT_FALSE(apogee == another);
    EXPECT_NE(apogee.id(), another.id());
    EXPECT_TRUE(apogee == Warning::EventAfterLanding{apogee});
    EXPECT_FALSE(apogee == Warning::kSupersonic);
    EXPECT_FALSE(apogee.replaceBy(another));
}

TEST(Warning, EventAfterLandingCanBePatchedWithTheEvent)
{
    const Warning::EventAfterLanding apogee{"Apogee"};
    Warning::EventAfterLanding       loaded;  // the .ork loader knows the event only later
    EXPECT_FALSE(loaded.eventType().has_value());
    loaded.setId(apogee.id());
    // Equal ids are equal here; Java compares the UUID references (see the class comment).
    EXPECT_TRUE(loaded == apogee);
    loaded.setEventType("Ejection charge");
    EXPECT_EQ(loaded.eventType().value_or(""), "Ejection charge");
    EXPECT_EQ(loaded.messageDescription(), "Flight Event occurred after landing: Ejection charge");
    EXPECT_TRUE(loaded == apogee);  // still the same warning
}

TEST(Warning, MissingMotorDescribesTheMotor)
{
    Warning::MissingMotor missing;
    EXPECT_EQ(missing.priority(), MessagePriority::HIGH);
    EXPECT_EQ(missing.typeName(), "MissingMotor");
    EXPECT_TRUE(std::isnan(missing.diameter()));
    EXPECT_TRUE(std::isnan(missing.length()));
    EXPECT_TRUE(std::isnan(missing.delay()));
    EXPECT_FALSE(missing.type().has_value());
    EXPECT_FALSE(missing.digest().has_value());
    EXPECT_EQ(missing.messageDescription(), "No motor with designation 'null' found.");
    missing.setDesignation("D12");
    EXPECT_EQ(missing.messageDescription(), "No motor with designation 'D12' found.");
    missing.setManufacturer("Estes");
    EXPECT_EQ(missing.messageDescription(),
              "No motor with designation 'D12' for manufacturer 'Estes' found.");
    EXPECT_FALSE(missing.replaceBy(Warning::MissingMotor{}));
}

TEST(Warning, MissingMotorComparesEveryField)
{
    Warning::MissingMotor a;
    Warning::MissingMotor b;
    EXPECT_TRUE(a == b);  // NaN fields compare equal, as Double.doubleToLongBits() does
    b.setDesignation("D12");
    EXPECT_FALSE(a == b);
    a.setDesignation("D12");
    b.setDelay(3.0);
    EXPECT_FALSE(a == b);
    a.setDelay(3.0);
    b.setType("SINGLE");
    EXPECT_FALSE(a == b);
    a.setType("SINGLE");
    b.setDigest("abc");
    EXPECT_FALSE(a == b);
    a.setDigest("abc");
    b.setDiameter(0.018);
    EXPECT_FALSE(a == b);
    a.setDiameter(0.018);
    b.setLength(0.07);
    EXPECT_FALSE(a == b);
    a.setLength(0.07);
    b.setManufacturer("Estes");
    EXPECT_FALSE(a == b);
    a.setManufacturer("Estes");
    EXPECT_TRUE(a == b);
    b.setSources(MessageSources{source("mm-1", "Motor mount")});
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(a == Warning::kSupersonic);
}

TEST(Warning, MissingMotorDistinguishesSignedZero)
{
    Warning::MissingMotor a;
    Warning::MissingMotor b;
    a.setDelay(0.0);
    b.setDelay(-0.0);
    EXPECT_FALSE(a == b);  // Double.doubleToLongBits(0.0) != Double.doubleToLongBits(-0.0)
    b.setDelay(0.0);
    EXPECT_TRUE(a == b);
}

TEST(Warning, CloneKeepsDynamicTypeAndState)
{
    Warning::LargeAOA original{0.25};
    original.setSources(MessageSources{source("fs-1", "Fin set")});
    const Uuid id = Uuid::random();
    original.setId(id);
    const std::unique_ptr<Message> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->typeName(), "LargeAOA");
    EXPECT_EQ(copy->id(), id);
    EXPECT_EQ(copy->sources(), (MessageSources{source("fs-1", "Fin set")}));
    EXPECT_EQ(copy->priority(), MessagePriority::LOW);
    const auto* typed = dynamic_cast<const Warning::LargeAOA*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_DOUBLE_EQ(typed->aoa(), 0.25);
    EXPECT_TRUE(*copy == original);
    EXPECT_EQ(Warning::kSupersonic.clone()->toString(),
              "Body calculations may not be entirely accurate at supersonic speeds.");
    EXPECT_EQ(Warning::MissingMotor{}.clone()->typeName(), "MissingMotor");
    EXPECT_EQ(Warning::EventAfterLanding{}.clone()->typeName(), "EventAfterLanding");
}

}  // namespace
