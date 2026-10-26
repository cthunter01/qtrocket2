#include "QtRocket/motor/chooser/MotorRowFilter.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::ImpulseClass;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorRowFilter;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::Test::DefaultUnitsGuard;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;
using Terms    = std::vector<std::string>;

/// MotorRowFilterTest.EPSILON: "just 'slightly' above or below limits"; above the filter's slack of
/// 0.0015 on the minimum diameter.
constexpr double kEpsilon = 0.002;

MotorPtr build(const ThrustCurveMotor::Builder& builder)
{
    QtRocket::Result<ThrustCurveMotor> motor = builder.build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// TestFullMotor's motor: "the Aerotech H123 from thrustcurve.org, motor ID 917".
MotorPtr fullMotor()
{
    ThrustCurveMotor::Builder builder;
    builder.setCaseInfo("Aerotech 38/240")
        .setCGPoints({Coordinate(0.077, 0, 0, 125.0), Coordinate(0.077, 0, 0, 109.93),
                      Coordinate(0.077, 0, 0, 82.3872), Coordinate(0.077, 0, 0, 75.6278),
                      Coordinate(0.077, 0, 0, 69.1235), Coordinate(0.077, 0, 0, 40.4295),
                      Coordinate(0.077, 0, 0, 30.4827), Coordinate(0.077, 0, 0, 17.4757),
                      Coordinate(0.077, 0, 0, 7.526), Coordinate(0.077, 0, 0, 4.84713),
                      Coordinate(0.077, 0, 0, 2.67857), Coordinate(0.077, 0, 0, 2.6)})
        .setDescription("Description of H123")
        .setDesignation("H123")
        .setDiameter(0.038)
        .setDigest("digestH123")
        .setInitialMass(293.3)
        .setLength(0.152)
        .setManufacturer(Manufacturer::getManufacturer("AeroTech"))
        .setMotorType(Motor::Type::RELOAD)
        .setPropellantInfo("White Lightning")
        .setStandardDelays({6, 10, 14})
        .setThrustPoints(
            {138.42, 116.45, 116.45, 112.18, 107.82, 86.29, 81.93, 64.72, 47.46, 43.15, 30.2, 0.0})
        .setTimePoints({0.0, 0.2, 0.6, 0.7, 0.8, 1.3, 1.5, 1.8, 2.1, 2.2, 2.3, 2.6});
    return build(builder);
}

/// TestEmptyMotor's motor: "a motor with minimum fields explicitly defined".
MotorPtr emptyMotor()
{
    ThrustCurveMotor::Builder builder;
    builder.setTimePoints({0, 1}).setThrustPoints({0, 2}).setLength(0.10).setCGPoints(
        {Coordinate(0.05, 0, 0, 0.05), Coordinate(0.03, 0, 0, 0.03)});
    return build(builder);
}

/// A set holding @p motor alone.
ThrustCurveMotorSet setOf(const MotorPtr& motor)
{
    ThrustCurveMotorSet set;
    set.addMotor(motor);
    return set;
}

/// An Estes motor of the designation @p designation (diameter 18 mm, length 70 mm), available or
/// not.
MotorPtr estes(std::string designation, bool available = true)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation(std::move(designation))
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0, 1, 2})
        .setThrustPoints({0, 5, 0})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.02), Coordinate(0.035, 0, 0, 0.015),
                      Coordinate(0.035, 0, 0, 0.01)})
        .setMotorType(Motor::Type::SINGLE)
        .setAvailability(available);
    return build(builder);
}

// ---- MotorRowFilterTest.testMotor(), section by section

void checkSearchTerms(MotorRowFilter& filter, const ThrustCurveMotorSet& set,
                      const Terms& goodSearchTerms)
{
    const InMemoryPreferences preferences;
    // Two search terms, one present and one not
    filter.setSearchTerms(Terms{"xx", "tec"});
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setSearchTerms(goodSearchTerms);
    if (!goodSearchTerms.empty())
    {
        EXPECT_TRUE(filter.include(set, preferences));
    }
}

void checkLengths(MotorRowFilter& filter, const ThrustCurveMotorSet& set, const Motor& motor)
{
    const InMemoryPreferences preferences;
    // minimum length set/get/filter
    const double minLength = motor.getLength() + kEpsilon;
    filter.setMinimumLength(minLength);
    EXPECT_NEAR(minLength, filter.getMinimumLength(), kEpsilon);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMinimumLength(motor.getLength() - kEpsilon);
    EXPECT_TRUE(filter.include(set, preferences));

    // maximum length set/get/filter
    const double maxLength = motor.getLength() - kEpsilon;
    filter.setMaximumLength(maxLength);
    EXPECT_NEAR(maxLength, filter.getMaximumLength(), kEpsilon);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMaximumLength(motor.getLength() + kEpsilon);
    EXPECT_TRUE(filter.include(set, preferences));
}

void checkDiameters(MotorRowFilter& filter, const ThrustCurveMotorSet& set, const Motor& motor)
{
    const InMemoryPreferences preferences;
    // minimum diameter set/get/filter
    const double minDiameter = motor.getDiameter() + kEpsilon;
    filter.setMinimumDiameter(minDiameter);
    EXPECT_NEAR(minDiameter, filter.getMinimumDiameter().value_or(0), kEpsilon);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMinimumDiameter(motor.getDiameter() - kEpsilon);
    EXPECT_TRUE(filter.include(set, preferences));

    // maximum diameter set/get/filter
    const double maxDiameter = motor.getDiameter() - kEpsilon;
    filter.setMaximumDiameter(maxDiameter);
    EXPECT_NEAR(maxDiameter, filter.getMaximumDiameter().value_or(0), kEpsilon);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMaximumDiameter(motor.getDiameter() + kEpsilon);
    EXPECT_TRUE(filter.include(set, preferences));
}

void checkManufacturers(MotorRowFilter& filter, const ThrustCurveMotorSet& set,
                        const ThrustCurveMotor& motor)
{
    const InMemoryPreferences preferences;
    // manufacturer set/get/filter presence/absence
    if (&motor.getManufacturer() != &Manufacturer::getManufacturer("UNKNOWN"))
    {
        const std::vector<const Manufacturer*> excluded{&Manufacturer::getManufacturer("Estes"),
                                                        &motor.getManufacturer()};
        filter.setExcludedManufacturers(excluded);
        EXPECT_FALSE(filter.include(set, preferences));
    }
    const std::vector<const Manufacturer*> excluded{&Manufacturer::getManufacturer("Estes"),
                                                    &Manufacturer::getManufacturer("Loki")};
    filter.setExcludedManufacturers(excluded);
    // Java compares the list's toString(), "[Estes, Loki Research]": the display names.
    ASSERT_EQ(filter.getExcludedManufacturers().size(), 2U);
    EXPECT_EQ(filter.getExcludedManufacturers()[0]->getDisplayName(), "Estes");
    EXPECT_EQ(filter.getExcludedManufacturers()[1]->getDisplayName(), "Loki Research");
    EXPECT_TRUE(filter.include(set, preferences));
}

void checkMinimumImpulse(MotorRowFilter& filter, const ThrustCurveMotorSet& set,
                         ImpulseClass impulse, ImpulseClass highImpulse)
{
    const InMemoryPreferences preferences;
    // minimum impulse class set/get/filter
    filter.setMinimumImpulse(highImpulse);
    EXPECT_EQ(highImpulse, filter.getMinimumImpulse());
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMinimumImpulse(impulse);
    EXPECT_EQ(impulse, filter.getMinimumImpulse());
    EXPECT_TRUE(filter.include(set, preferences));
}

void checkMaximumImpulse(MotorRowFilter& filter, const ThrustCurveMotorSet& set,
                         ImpulseClass lowImpulse, ImpulseClass impulse)
{
    const InMemoryPreferences preferences;
    // maximum impulse class set/get/filter
    if (lowImpulse != impulse)
    {
        filter.setMaximumImpulse(lowImpulse);
        EXPECT_EQ(lowImpulse, filter.getMaximumImpulse());
        EXPECT_FALSE(filter.include(set, preferences));
    }
    filter.setMaximumImpulse(impulse);
    EXPECT_TRUE(filter.include(set, preferences));
}

void checkHideUnavailable(MotorRowFilter& filter)
{
    // hide unavailable set/get
    filter.setHideUnavailable(true);
    EXPECT_TRUE(filter.isHideUnavailable());
    filter.setHideUnavailable(false);
    EXPECT_FALSE(filter.isHideUnavailable());
}

/// MotorRowFilterTest.testMotor().
void testMotor(const MotorPtr& motor, const Terms& goodSearchTerms, ImpulseClass lowImpulse,
               ImpulseClass impulse, ImpulseClass highImpulse)
{
    ASSERT_NE(motor, nullptr);
    const ThrustCurveMotorSet set = setOf(motor);
    MotorRowFilter            filter;
    checkSearchTerms(filter, set, goodSearchTerms);
    checkLengths(filter, set, *motor);
    checkDiameters(filter, set, *motor);
    checkManufacturers(filter, set, *motor);
    checkMinimumImpulse(filter, set, impulse, highImpulse);
    checkMaximumImpulse(filter, set, lowImpulse, impulse);
    checkHideUnavailable(filter);
}

/// A rocket of one stage whose body tube is a motor mount.
struct MountedRocket
{
    Rocket    rocket;
    BodyTube* mount{nullptr};

    MountedRocket()
    {
        AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
        mount             = &stage.addChild(std::make_unique<BodyTube>());
        mount->setMotorMount(true);
    }

    /// A configuration of the mount with @p motor.
    void add(MotorPtr motor)
    {
        const FlightConfigurationId fcid;
        rocket.createFlightConfiguration(fcid);
        MotorConfiguration configuration(*mount, fcid);
        configuration.setMotor(std::move(motor));
        mount->setMotorConfig(configuration, fcid);
    }
};

/// The motors of the configurations of @p mount (MotorChooserInput::fromMount()).
std::vector<MotorPtr> usedMotorsOf(const BodyTube& mount)
{
    return QtRocket::MotorChooserInput::fromMount(mount, FlightConfigurationId()).mountMotors;
}

// ---- the ports

// Port of MotorRowFilterTest.TestFullMotor (swing gui/dialogs/motor/thrustcurve).
TEST(MotorRowFilter, TestFullMotor)
{
    // two search terms, both present, one only a substring of a column
    testMotor(fullMotor(), Terms{"aerotech", "h12"}, ImpulseClass::G, ImpulseClass::H,
              ImpulseClass::I);
}

// Port of MotorRowFilterTest.TestEmptyMotor.
TEST(MotorRowFilter, TestEmptyMotor)
{
    testMotor(emptyMotor(), Terms{}, ImpulseClass::A, ImpulseClass::A, ImpulseClass::B);
}

// ---- QtRocket's own

TEST(MotorRowFilter, StartsWithoutLimits)
{
    const MotorRowFilter filter;
    EXPECT_EQ(filter.getMinimumLength(), 0.0);
    EXPECT_EQ(filter.getMaximumLength(), std::numeric_limits<double>::max());
    EXPECT_EQ(filter.getMinimumDiameter(), std::nullopt);
    EXPECT_EQ(filter.getMaximumDiameter(), std::nullopt);
    EXPECT_EQ(filter.getMinimumImpulse(), std::nullopt);
    EXPECT_EQ(filter.getMaximumImpulse(), std::nullopt);
    EXPECT_FALSE(filter.isHideUsedMotors());
    EXPECT_FALSE(filter.isHideUnavailable());
    EXPECT_TRUE(filter.getExcludedManufacturers().empty());
    EXPECT_TRUE(filter.getSearchTerms().empty());
    EXPECT_TRUE(filter.getUsedMotors().empty());
}

/// text.trim().split("\\s+"), each part trimmed and lower-cased, the empty ones left out: a control
/// character at the end of a word goes with String.trim, one inside it stays.
TEST(MotorRowFilter, SplitsTheSearchTextAsTheSearchFieldDoes)
{
    EXPECT_EQ(MotorRowFilter::searchTerms("  Estes\tC6 "), (Terms{"estes", "c6"}));
    EXPECT_EQ(MotorRowFilter::searchTerms("AeroTech  H128W\r\f\x0BX"),
              (Terms{"aerotech", "h128w", "x"}));
    EXPECT_EQ(MotorRowFilter::searchTerms(" \x01"
                                          "Estes\x02\tC6 "),
              (Terms{"estes", "c6"}));
    EXPECT_EQ(MotorRowFilter::searchTerms("a\x01"
                                          "b"),
              (Terms{"a\x01"
                     "b"}));
    EXPECT_EQ(MotorRowFilter::searchTerms("\x01 \x02"), Terms{});
    EXPECT_EQ(MotorRowFilter::searchTerms(""), Terms{});
    EXPECT_EQ(MotorRowFilter::searchTerms("\xC3\x96L"), (Terms{"\xC3\xB6l"}));
    // U+00A0 is neither \s nor trimmed: it stays part of the word.
    EXPECT_EQ(MotorRowFilter::searchTerms("\xC2\xA0"
                                          "a"),
              (Terms{"\xC2\xA0"
                     "a"}));
}

TEST(MotorRowFilter, TrimsAndFoldsTheSearchTermsItIsGiven)
{
    MotorRowFilter filter;
    filter.setSearchTerms(Terms{" Estes ", "", "\x01", "C6"});
    EXPECT_EQ(filter.getSearchTerms(), (Terms{"estes", "c6"}));
    filter.setSearchText(" H128W  aerotech");
    EXPECT_EQ(filter.getSearchTerms(), (Terms{"h128w", "aerotech"}));
}

/// The search looks at the table's texts (the dimensions as shown, in mm) and at both names: a
/// term in the common name matches while the table shows the designation, and the other way round.
TEST(MotorRowFilter, SearchesTheColumnsAndBothNames)
{
    const DefaultUnitsGuard guard;
    InMemoryPreferences     preferences;
    const MotorPtr          motor = fullMotor();
    ASSERT_NE(motor, nullptr);
    MotorRowFilter filter;
    filter.setSearchText("38 mm 152 38/240 aerotech");
    EXPECT_TRUE(filter.include(setOf(motor), preferences));
    filter.setSearchText("38 ns");
    EXPECT_FALSE(filter.include(setOf(motor), preferences));

    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer("Estes"))
        .setDesignation("C6-5")
        .setCommonName("Mighty")
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints({0, 1})
        .setThrustPoints({0, 5})
        .setCGPoints({Coordinate(0.035, 0, 0, 0.02), Coordinate(0.035, 0, 0, 0.01)});
    const MotorPtr named = build(builder);
    ASSERT_NE(named, nullptr);
    filter.setSearchText("mighty");
    EXPECT_TRUE(filter.include(setOf(named), preferences));
    preferences.setMotorNameColumn(false);
    filter.setSearchText("c6-5");
    EXPECT_TRUE(filter.include(setOf(named), preferences));
}

/// The slack: a diameter at the minimum less 1.5 mm or at the maximum plus 0.4 mm is out, just
/// inside is in; the lengths have none.
TEST(MotorRowFilter, LimitsTheSizeWithOpenRocketsSlack)
{
    const InMemoryPreferences preferences;
    const MotorPtr            motor = estes("C6");
    ASSERT_NE(motor, nullptr);
    const ThrustCurveMotorSet set = setOf(motor);
    MotorRowFilter            filter;
    filter.setMinimumDiameter(0.018 + MotorRowFilter::kMinimumDiameterSlack);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMinimumDiameter(0.0194);
    EXPECT_TRUE(filter.include(set, preferences));
    filter.setMinimumDiameter(std::nullopt);
    filter.setMaximumDiameter(0.018 - MotorRowFilter::kMaximumDiameterSlack);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMaximumDiameter(0.0177);
    EXPECT_TRUE(filter.include(set, preferences));
    filter.setMaximumLength(0.07);
    filter.setMinimumLength(0.07);
    EXPECT_TRUE(filter.include(set, preferences));
}

/// The impulse classes compare with the rounded total impulse: low end excluded, high end included.
TEST(MotorRowFilter, LimitsTheImpulseClass)
{
    const InMemoryPreferences preferences;
    const MotorPtr            motor = estes("C5");  // 5 Ns: the high end of B
    ASSERT_NE(motor, nullptr);
    const ThrustCurveMotorSet set = setOf(motor);
    ASSERT_EQ(set.getTotalImpulse(), 5);
    MotorRowFilter filter;
    filter.setMaximumImpulse(ImpulseClass::B);
    EXPECT_TRUE(filter.include(set, preferences));
    filter.setMaximumImpulse(std::nullopt);
    filter.setMinimumImpulse(ImpulseClass::C);
    EXPECT_FALSE(filter.include(set, preferences));
    filter.setMinimumImpulse(ImpulseClass::B);
    EXPECT_TRUE(filter.include(set, preferences));
}

TEST(MotorRowFilter, HidesUnavailableMotorsWhenAsked)
{
    const InMemoryPreferences preferences;
    const MotorPtr            available   = estes("C6");
    const MotorPtr            unavailable = estes("C11", false);
    ASSERT_NE(available, nullptr);
    ASSERT_NE(unavailable, nullptr);
    MotorRowFilter filter;
    EXPECT_TRUE(filter.include(setOf(unavailable), preferences));
    filter.setHideUnavailable(true);
    EXPECT_FALSE(filter.include(setOf(unavailable), preferences));
    EXPECT_TRUE(filter.include(setOf(available), preferences));
}

/// "Hide motors already used in the mount" hides the sets that match a motor of the mount, the
/// mount's own only (QtRocket fixes M3's bug 9: OpenRocket keeps the motors of every mount the
/// chooser was opened for).
TEST(MotorRowFilter, HidesTheMotorsOfTheCurrentMountOnly)
{
    const InMemoryPreferences preferences;
    const MotorPtr            c6  = estes("C6");
    const MotorPtr            d12 = estes("D12");
    ASSERT_NE(c6, nullptr);
    ASSERT_NE(d12, nullptr);
    MountedRocket first;
    first.add(c6);
    MountedRocket second;
    second.add(d12);

    MotorRowFilter filter;
    filter.setUsedMotors(usedMotorsOf(*first.mount));
    EXPECT_EQ(filter.getUsedMotors().size(), 1U);
    EXPECT_TRUE(filter.include(setOf(c6), preferences));
    filter.setHideUsedMotors(true);
    EXPECT_FALSE(filter.include(setOf(c6), preferences));
    EXPECT_TRUE(filter.include(setOf(d12), preferences));

    filter.setUsedMotors(usedMotorsOf(*second.mount));
    EXPECT_EQ(filter.getUsedMotors().size(), 1U);
    EXPECT_TRUE(filter.include(setOf(c6), preferences));
    EXPECT_FALSE(filter.include(setOf(d12), preferences));

    filter.setUsedMotors({});
    EXPECT_TRUE(filter.getUsedMotors().empty());
    EXPECT_TRUE(filter.include(setOf(d12), preferences));
}

/// The filter keeps the mount's motors, not the mount: they outlive the rocket.
TEST(MotorRowFilter, KeepsTheMotorsOfAMountThatIsGone)
{
    const InMemoryPreferences             preferences;
    std::weak_ptr<const ThrustCurveMotor> watched;
    MotorRowFilter                        filter;
    {
        MountedRocket rocket;
        MotorPtr      motor = estes("C6");
        ASSERT_NE(motor, nullptr);
        watched = motor;
        rocket.add(std::move(motor));
        filter.setUsedMotors(usedMotorsOf(*rocket.mount));
    }
    EXPECT_FALSE(watched.expired());
    filter.setHideUsedMotors(true);
    EXPECT_FALSE(filter.include(setOf(watched.lock()), preferences));
    filter.setUsedMotors({});
    EXPECT_TRUE(watched.expired());
}

TEST(MotorRowFilter, RefusesANullManufacturerOrMotorAndAnEmptySet)
{
    const InMemoryPreferences              preferences;
    MotorRowFilter                         filter;
    const std::vector<const Manufacturer*> withNull{nullptr};
    EXPECT_THROW(filter.setExcludedManufacturers(withNull), BugError);
    EXPECT_THROW(filter.setUsedMotors(std::vector<MotorPtr>{nullptr}), BugError);
    EXPECT_THROW(static_cast<void>(filter.include(ThrustCurveMotorSet{}, preferences)), BugError);
}

}  // namespace
