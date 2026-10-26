#include "QtRocket/motor/MotorPreferences.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

// OpenRocket has no JUnit test of these preferences. The stored keys and values below are what
// OpenRocket's own SwingPreferences writes and reads for the same calls: PrefsProbe.java of the
// M3 run 1 probes of part C3 (sections "excluded manufacturers" and "flags"), run against the
// compiled core and swing of commit 5f164fd0e (JDK 17, java.util.prefs' FileSystemPreferences,
// whose keys() are sorted as QtRocket's stores sort them).

namespace
{

namespace Keys = QtRocket::PreferenceKeys;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::Preferences;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSet;
namespace MotorPreferences = QtRocket::MotorPreferences;

using MotorPtr      = std::shared_ptr<const ThrustCurveMotor>;
using Manufacturers = std::vector<const Manufacturer*>;
using TextList      = std::vector<std::string>;

const Manufacturer& manufacturer(std::string_view name)
{
    return Manufacturer::getManufacturer(name);
}

/// An Estes C6 of @p points data points with the digest @p digest. A set keeps the curves with
/// more points first, so the point counts give the set's order.
MotorPtr estesC6(std::size_t points, std::string digest)
{
    std::vector<double>     time;
    std::vector<double>     thrust;
    std::vector<Coordinate> cg;
    for (std::size_t i = 0; i < points; i++)
    {
        time.push_back(0.5 * static_cast<double>(i));
        thrust.push_back(i == 0 || i + 1 == points ? 0.0 : 10.0);
        cg.push_back(Coordinate::kNul);
    }
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(manufacturer("Estes"))
        .setDesignation("C6")
        .setDescription("Desc")
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays({3, 5, 7})
        .setDiameter(0.018)
        .setLength(0.07)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg))
        .setDigest(std::move(digest));
    return std::make_shared<const ThrustCurveMotor>(builder.build().value());
}

constexpr std::string_view kDigest5 = "2967cd7a0e62f9cb6d1a6a3b3e6f1c95";
constexpr std::string_view kDigest4 = "9b06acb7d4f1e0d1a3c5b7e9f1a3c5b7";
constexpr std::string_view kDigest3 = "0123456789abcdef0123456789abcdef";

/// One set of three C6 curves, in this order: 5, 4 and 3 points.
struct C6Set
{
    MotorPtr            five  = estesC6(5, std::string(kDigest5));
    MotorPtr            four  = estesC6(4, std::string(kDigest4));
    MotorPtr            three = estesC6(3, std::string(kDigest3));
    ThrustCurveMotorSet set;

    C6Set()
    {
        set.addMotor(three);
        set.addMotor(five);
        set.addMotor(four);
    }
};

/// The keys of the node @p name, each "key=value", or {"absent"}.
TextList nodeContents(const Preferences& preferences, std::string_view name)
{
    const Preferences* const node = preferences.findNode(name);
    if (node == nullptr)
    {
        return {"absent"};
    }
    TextList contents;
    for (const std::string& key : node->keys())
    {
        contents.push_back(key + "=" + node->get(key).value_or("<none>"));
    }
    return contents;
}

TEST(MotorPreferences, KeysMatchOpenRocket)
{
    EXPECT_EQ(Keys::kMotorHideSimilar, "MotorHideSimilar");
    EXPECT_EQ(Keys::kMotorHideUnavailable, "MotorHideUnavailable");
    EXPECT_EQ(Keys::kMotorFilterLimitLength, "motorFilterLimitLength");
    EXPECT_EQ(Keys::kMotorFilterLimitDiameter, "motorFilterLimitDiameter");
    EXPECT_EQ(Keys::kExcludedMotorManufacturersNode, "excludedMotorManufacturers");
    EXPECT_EQ(Keys::kPreferredThrustCurveMotorNode, "PreferredThrustCurveMotors");
}

// The probe's "== flags": the defaults of an empty store.
TEST(MotorPreferences, FlagsDefaultAsOpenRocketsAndReadingStoresNothing)
{
    const InMemoryPreferences prefs;
    EXPECT_TRUE(MotorPreferences::isHideSimilar(prefs));
    EXPECT_TRUE(MotorPreferences::isHideUnavailable(prefs));
    EXPECT_FALSE(MotorPreferences::isLimitDiameter(prefs));
    EXPECT_FALSE(MotorPreferences::isLimitLength(prefs));
    EXPECT_TRUE(prefs.empty());
}

TEST(MotorPreferences, FlagsAreBooleansUnderJavasKeys)
{
    InMemoryPreferences prefs;
    MotorPreferences::setHideSimilar(prefs, false);
    MotorPreferences::setHideUnavailable(prefs, false);
    MotorPreferences::setLimitDiameter(prefs, true);
    MotorPreferences::setLimitLength(prefs, true);
    EXPECT_EQ(prefs.get("MotorHideSimilar"), "false");
    EXPECT_EQ(prefs.get("MotorHideUnavailable"), "false");
    EXPECT_EQ(prefs.get("motorFilterLimitDiameter"), "true");
    EXPECT_EQ(prefs.get("motorFilterLimitLength"), "true");
    EXPECT_FALSE(MotorPreferences::isHideSimilar(prefs));
    EXPECT_FALSE(MotorPreferences::isHideUnavailable(prefs));
    EXPECT_TRUE(MotorPreferences::isLimitDiameter(prefs));
    EXPECT_TRUE(MotorPreferences::isLimitLength(prefs));
    EXPECT_EQ(prefs.keys(), (TextList{"MotorHideSimilar", "MotorHideUnavailable",
                                      "motorFilterLimitDiameter", "motorFilterLimitLength"}));

    // AbstractPreferences.getBoolean(): the case does not matter, anything else is the default.
    prefs.put("MotorHideSimilar", "FALSE");
    prefs.put("motorFilterLimitLength", "yes");
    EXPECT_FALSE(MotorPreferences::isHideSimilar(prefs));
    EXPECT_FALSE(MotorPreferences::isLimitLength(prefs));
}

// The probe: setExcludedMotorManufacturers([Estes, AeroTech, LOC, Cesaroni Technology Inc.]).
TEST(MotorPreferences, ExcludedManufacturersAreStoredBySimpleName)
{
    InMemoryPreferences prefs;
    const Manufacturers four{&manufacturer("Estes"), &manufacturer("AeroTech"),
                             &manufacturer("LOC"), &manufacturer("Cesaroni Technology Inc.")};
    MotorPreferences::setExcludedMotorManufacturers(prefs, four);
    const TextList stored{"AeroTech=true", "Cesaroni Technology=true", "Estes=true",
                          "LOC/Precision=true"};
    EXPECT_EQ(nodeContents(prefs, "excludedMotorManufacturers"), stored);
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs),
              (Manufacturers{&manufacturer("AeroTech"), &manufacturer("Cesaroni Technology"),
                             &manufacturer("Estes"), &manufacturer("LOC/Precision")}));
    EXPECT_EQ(prefs.keys(), TextList{});
}

// The probe: setExcludedMotorManufacturers([Quest]) after the four.
TEST(MotorPreferences, SettingTheExcludedManufacturersReplacesThemAll)
{
    InMemoryPreferences prefs;
    const Manufacturers two{&manufacturer("Estes"), &manufacturer("AeroTech")};
    MotorPreferences::setExcludedMotorManufacturers(prefs, two);
    const Manufacturers quest{&manufacturer("Quest")};
    MotorPreferences::setExcludedMotorManufacturers(prefs, quest);
    EXPECT_EQ(nodeContents(prefs, "excludedMotorManufacturers"), TextList{"Quest=true"});
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs), quest);

    MotorPreferences::setExcludedMotorManufacturers(prefs, {});
    EXPECT_EQ(nodeContents(prefs, "excludedMotorManufacturers"), TextList{});
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs), Manufacturers{});
}

// The probe: keys written by hand ("aerotech" = "false", "AT" = "true") beside "Quest". Java
// reads every key, whatever its value, as any name of a manufacturer, in the keys' order.
TEST(MotorPreferences, ExcludedManufacturersAreEveryKeyByAnyName)
{
    InMemoryPreferences prefs;
    const Manufacturers quest{&manufacturer("Quest")};
    MotorPreferences::setExcludedMotorManufacturers(prefs, quest);
    Preferences& node = prefs.getNode("excludedMotorManufacturers");
    node.put("aerotech", "false");
    node.put("AT", "true");
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs),
              (Manufacturers{&manufacturer("AeroTech"), &manufacturer("Quest"),
                             &manufacturer("AeroTech")}));
}

// A key with '/' ("LOC/Precision", or the database's "AMW/ProX") is a key like any other.
TEST(MotorPreferences, ExcludedManufacturerNamesMayHoldASlash)
{
    InMemoryPreferences prefs;
    const Manufacturers loc{&manufacturer("LOC/Precision")};
    MotorPreferences::setExcludedMotorManufacturers(prefs, loc);
    EXPECT_EQ(prefs.getNode("excludedMotorManufacturers").keys(), TextList{"LOC/Precision"});
    EXPECT_EQ(prefs.findNode("excludedMotorManufacturers/LOC"), nullptr);
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs), loc);
}

TEST(MotorPreferences, ReadingNoExcludedManufacturersCreatesNothing)
{
    const InMemoryPreferences prefs;
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(prefs), Manufacturers{});
    EXPECT_TRUE(prefs.empty());
}

TEST(MotorPreferences, ANullManufacturerIsABug)
{
    InMemoryPreferences prefs;
    const Manufacturers withNull{&manufacturer("Estes"), nullptr};
    EXPECT_THROW(MotorPreferences::setExcludedMotorManufacturers(prefs, withNull), BugError);
    // Java's loop stored the keys before the null one.
    EXPECT_EQ(nodeContents(prefs, "excludedMotorManufacturers"), TextList{"Estes=true"});
}

// selectedMotor(): every curve of the set gets a boolean, true for the chosen one.
TEST(MotorPreferences, TheChosenThrustCurveIsStoredForEveryCurveOfItsSet)
{
    const C6Set c6;
    ASSERT_EQ(c6.set.getMotorCount(), 3U);
    ASSERT_EQ(c6.set.getMotors()[0], c6.five);
    ASSERT_EQ(c6.set.getMotors()[1], c6.four);
    ASSERT_EQ(c6.set.getMotors()[2], c6.three);

    InMemoryPreferences prefs;
    MotorPreferences::storePreferredThrustCurve(prefs, c6.set, *c6.four);
    EXPECT_EQ(nodeContents(prefs, "PreferredThrustCurveMotors"),
              (TextList{std::string(kDigest3) + "=false", std::string(kDigest5) + "=false",
                        std::string(kDigest4) + "=true"}));
    EXPECT_TRUE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest4));
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest5));
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest3));
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), c6.four);

    MotorPreferences::storePreferredThrustCurve(prefs, c6.set, *c6.three);
    EXPECT_EQ(nodeContents(prefs, "PreferredThrustCurveMotors"),
              (TextList{std::string(kDigest3) + "=true", std::string(kDigest5) + "=false",
                        std::string(kDigest4) + "=false"}));
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), c6.three);
}

TEST(MotorPreferences, AMotorOfAnotherSetMakesEveryCurveFalse)
{
    const C6Set         c6;
    const MotorPtr      other = estesC6(6, "ffffffffffffffffffffffffffffffff");
    InMemoryPreferences prefs;
    MotorPreferences::storePreferredThrustCurve(prefs, c6.set, *c6.five);
    MotorPreferences::storePreferredThrustCurve(prefs, c6.set, *other);
    EXPECT_EQ(nodeContents(prefs, "PreferredThrustCurveMotors"),
              (TextList{std::string(kDigest3) + "=false", std::string(kDigest5) + "=false",
                        std::string(kDigest4) + "=false"}));
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), nullptr);
}

// selectMotor()'s loop: the first curve of the set, in its order, stored as true. The probe:
// getBoolean() of a value "TRUE" is true.
TEST(MotorPreferences, ThePreferredThrustCurveIsTheFirstOfTheSetStoredAsTrue)
{
    const C6Set         c6;
    InMemoryPreferences prefs;
    Preferences&        node = prefs.getNode("PreferredThrustCurveMotors");
    node.put(kDigest3, "TRUE");
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), c6.three);
    node.put(kDigest4, "true");
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), c6.four);
    node.put(kDigest5, "yes");
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), c6.four);
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest5));
    EXPECT_TRUE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest3));
}

TEST(MotorPreferences, ReadingNoPreferredThrustCurveCreatesNothing)
{
    const C6Set               c6;
    const InMemoryPreferences prefs;
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, c6.set), nullptr);
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(prefs, kDigest5));
    EXPECT_TRUE(prefs.empty());

    // An empty set has no preferred curve either.
    const ThrustCurveMotorSet empty;
    EXPECT_EQ(MotorPreferences::findPreferredThrustCurve(prefs, empty), nullptr);
}

}  // namespace
