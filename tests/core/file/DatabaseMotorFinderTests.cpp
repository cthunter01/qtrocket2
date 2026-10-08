#include "QtRocket/file/DatabaseMotorFinder.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/MotorFinder.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "motor/TestMotorDatabase.h"

// The expectations against the bundled database are OpenRocket's own answers: for the motors
// of the example designs the scouts' tables (expected-motors.tsv and motor-queries.tsv of the
// tier 9 scout reports), for the rest the probe FinderProbe of part D4, both over the same
// database filled in the same order.

namespace
{

using QtRocket::DatabaseMotorFinder;
using QtRocket::MessagePriority;
using QtRocket::Motor;
using QtRocket::MotorDatabase;
using QtRocket::MotorDigest;
using QtRocket::MotorFinder;
using QtRocket::ThrustCurveMotor;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::bundledMotorDatabase;
using QtRocket::Test::makeEmbeddedTestMotor;
using QtRocket::Test::raspStyleDigest;
using QtRocket::Test::warningTexts;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// A database that answers every search with the motors it was made with, in their order, and
/// records what it was asked.
class FixedDatabase final : public MotorDatabase
{
public:
    FixedDatabase() = default;
    explicit FixedDatabase(std::vector<std::shared_ptr<const Motor>> motors)
      : m_motors(std::move(motors))
    {
    }

    [[nodiscard]] std::vector<std::shared_ptr<const Motor>> findMotors(
        std::optional<std::string_view> digest, std::optional<Motor::Type> type,
        std::optional<std::string_view> manufacturer, std::optional<std::string_view> designation,
        double diameter, double length) const override
    {
        m_queries.push_back(std::string(digest.value_or("null")) + "|" +
                            std::string(type.has_value() ? enumName(*type) : "null") + "|" +
                            std::string(manufacturer.value_or("null")) + "|" +
                            std::string(designation.value_or("null")) + "|" +
                            QtRocket::Strings::javaDoubleToString(diameter) + "|" +
                            QtRocket::Strings::javaDoubleToString(length));
        return m_motors;
    }

    [[nodiscard]] const std::vector<std::string>& queries() const noexcept { return m_queries; }

private:
    std::vector<std::shared_ptr<const Motor>> m_motors;
    mutable std::vector<std::string>          m_queries;
};

/// What a search gave: the motor and the warnings.
struct Found
{
    std::shared_ptr<const Motor> motor;
    WarningSet                   warnings;

    [[nodiscard]] std::vector<std::string> texts() const { return warningTexts(warnings); }
    /// "<manufacturer>/<designation>/<digest>" of the motor, or "null".
    [[nodiscard]] std::string describe() const
    {
        const auto* curve = dynamic_cast<const ThrustCurveMotor*>(motor.get());
        if (curve == nullptr)
        {
            return "null";
        }
        return curve->getManufacturer().getDisplayName() + "/" + curve->getDesignation() + "/" +
               curve->getDigest();
    }
};

[[nodiscard]] Found find(const MotorFinder& finder, std::optional<Motor::Type> type,
                         std::optional<std::string_view> manufacturer,
                         std::optional<std::string_view> designation,
                         std::optional<std::string_view> digest)
{
    Found found;
    found.motor =
        finder.findMotor(type, manufacturer, designation, kNaN, kNaN, digest, found.warnings);
    return found;
}

/// find() in the bundled database.
[[nodiscard]] Found findBundled(std::optional<Motor::Type>      type,
                                std::optional<std::string_view> manufacturer,
                                std::optional<std::string_view> designation,
                                std::optional<std::string_view> digest)
{
    const DatabaseMotorFinder finder(bundledMotorDatabase());
    return find(finder, type, manufacturer, designation, digest);
}

/// The number of motors the bundled database offers for the query.
[[nodiscard]] std::size_t candidates(std::optional<Motor::Type>      type,
                                     std::optional<std::string_view> manufacturer,
                                     std::string_view                designation,
                                     std::optional<std::string_view> digest)
{
    return bundledMotorDatabase()
        .findMotors(digest, type, manufacturer, designation, kNaN, kNaN)
        .size();
}

// ---- step by step, over a database that answers what the test says -------------------------

TEST(DatabaseMotorFinder, WithoutADesignationThereIsNoMotorAndNoSearch)
{
    const FixedDatabase       database({makeEmbeddedTestMotor("F12X", 12.0, "a")});
    const DatabaseMotorFinder finder(database);
    const Found               found = find(finder, Motor::Type::SINGLE, "Estes", std::nullopt, "a");
    EXPECT_EQ(found.motor, nullptr);
    EXPECT_EQ(found.texts(), std::vector<std::string>{"No motor specified, ignoring."});
    EXPECT_EQ(found.warnings.begin()->priority(), MessagePriority::NORMAL);
    EXPECT_TRUE(database.queries().empty());
}

TEST(DatabaseMotorFinder, AsksTheDatabaseWithEveryCriterion)
{
    const FixedDatabase       database;
    const DatabaseMotorFinder finder(database);
    WarningSet                warnings;
    EXPECT_EQ(
        finder.findMotor(Motor::Type::RELOAD, "AeroTech", "H148R", 0.038, 0.152, "abc", warnings),
        nullptr);
    EXPECT_EQ(
        finder.findMotor(std::nullopt, std::nullopt, "C6", kNaN, kNaN, std::nullopt, warnings),
        nullptr);
    EXPECT_EQ(database.queries(), (std::vector<std::string>{"abc|RELOAD|AeroTech|H148R|0.038|0.152",
                                                            "null|null|null|C6|NaN|NaN"}));
}

TEST(DatabaseMotorFinder, AMissingMotorIsAWarningWithTheQuerysFields)
{
    const FixedDatabase       database;
    const DatabaseMotorFinder finder(database);
    WarningSet                warnings;
    EXPECT_EQ(finder.findMotor(Motor::Type::HYBRID, "Estes", "C6", 0.018, 0.07, "abc", warnings),
              nullptr);
    ASSERT_EQ(warnings.size(), 1U);
    const auto* missing = dynamic_cast<const Warning::MissingMotor*>(&*warnings.begin());
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->priority(), MessagePriority::HIGH);
    EXPECT_EQ(missing->toString(),
              "No motor with designation 'C6' for manufacturer 'Estes' found.");
    EXPECT_EQ(missing->type(), std::optional<std::string>("HYBRID"));
    EXPECT_EQ(missing->manufacturer(), std::optional<std::string>("Estes"));
    EXPECT_EQ(missing->designation(), std::optional<std::string>("C6"));
    EXPECT_EQ(missing->digest(), std::optional<std::string>("abc"));
    EXPECT_EQ(missing->diameter(), 0.018);
    EXPECT_EQ(missing->length(), 0.07);
    EXPECT_TRUE(std::isnan(missing->delay()));

    // Without a manufacturer, a type or a digest.
    const Found plain = find(finder, std::nullopt, std::nullopt, "ZZZ999", std::nullopt);
    EXPECT_EQ(plain.texts(), std::vector<std::string>{"No motor with designation 'ZZZ999' found."});
    const auto* bare = dynamic_cast<const Warning::MissingMotor*>(&*plain.warnings.begin());
    ASSERT_NE(bare, nullptr);
    EXPECT_FALSE(bare->type().has_value());
    EXPECT_FALSE(bare->manufacturer().has_value());
    EXPECT_FALSE(bare->digest().has_value());
    EXPECT_TRUE(std::isnan(bare->diameter()));
    EXPECT_TRUE(std::isnan(bare->length()));
}

TEST(DatabaseMotorFinder, ASingleCandidateIsTakenWhateverItsDigest)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = makeEmbeddedTestMotor("F12X", 12.0, "a");
    const FixedDatabase                           database({motor});
    const DatabaseMotorFinder                     finder(database);
    for (const std::optional<std::string_view> digest :
         {std::optional<std::string_view>{}, std::optional<std::string_view>{"a"},
          std::optional<std::string_view>{"another"}, std::optional<std::string_view>{""}})
    {
        const Found found = find(finder, std::nullopt, std::nullopt, "G1", digest);
        EXPECT_EQ(found.motor, motor) << digest.value_or("(none)");
        EXPECT_TRUE(found.warnings.empty()) << digest.value_or("(none)");
    }
}

TEST(DatabaseMotorFinder, OfSeveralWithADigestTheFirstCompatibleOneIsTaken)
{
    const std::shared_ptr<const ThrustCurveMotor> other = makeEmbeddedTestMotor("F12X", 20.0, "x");
    const std::shared_ptr<const ThrustCurveMotor> exact = makeEmbeddedTestMotor("F9", 12.0, "y");
    const std::shared_ptr<const ThrustCurveMotor> another =
        makeEmbeddedTestMotor("F12X", 12.0, "z");
    const FixedDatabase       database({other, exact, another});
    const DatabaseMotorFinder finder(database);

    // The motor's own digest.
    const Found own = find(finder, std::nullopt, std::nullopt, "F12X", "y");
    EXPECT_EQ(own.motor, exact);
    EXPECT_TRUE(own.warnings.empty());
    // A digest of an older format of the same curve: "exact" and "another" share the curve,
    // and the first of them in the database's order is taken.
    const Found historical =
        find(finder, std::nullopt, std::nullopt, "F12X", raspStyleDigest(*another));
    EXPECT_EQ(historical.motor, exact);
    EXPECT_TRUE(historical.warnings.empty());
}

TEST(DatabaseMotorFinder, ElseTheFirstWithTheDesignationElseTheFirst)
{
    const std::shared_ptr<const ThrustCurveMotor> b60 = makeEmbeddedTestMotor("B6-0", 6.0, "x");
    const std::shared_ptr<const ThrustCurveMotor> b6  = makeEmbeddedTestMotor("B6", 7.0, "y");
    const std::shared_ptr<const ThrustCurveMotor> b6b = makeEmbeddedTestMotor("B6", 8.0, "z");
    const FixedDatabase                           database({b60, b6, b6b});
    const DatabaseMotorFinder                     finder(database);

    // No digest fits: the designation decides, ignoring case, and silently.
    const Found byName = find(finder, std::nullopt, std::nullopt, "B6", "no such digest");
    EXPECT_EQ(byName.motor, b6);
    EXPECT_TRUE(byName.warnings.empty());
    EXPECT_EQ(find(finder, std::nullopt, std::nullopt, "b6", "no such digest").motor, b6);
    EXPECT_EQ(find(finder, std::nullopt, std::nullopt, "b6-0", "no such digest").motor, b60);
    // An empty digest is a digest: no warning, and the same two fallbacks.
    const Found empty = find(finder, std::nullopt, std::nullopt, "B6", "");
    EXPECT_EQ(empty.motor, b6);
    EXPECT_TRUE(empty.warnings.empty());
    // No designation fits either: the first, still silently.
    const Found first = find(finder, std::nullopt, std::nullopt, "B", "no such digest");
    EXPECT_EQ(first.motor, b60);
    EXPECT_TRUE(first.warnings.empty());
}

TEST(DatabaseMotorFinder, OfSeveralWithoutADigestTheFirstIsTakenWithAWarning)
{
    const std::shared_ptr<const ThrustCurveMotor> b60 = makeEmbeddedTestMotor("B6-0", 6.0, "x");
    const std::shared_ptr<const ThrustCurveMotor> b6  = makeEmbeddedTestMotor("B6", 7.0, "y");
    const FixedDatabase                           database({b60, b6});
    const DatabaseMotorFinder                     finder(database);

    // Not the one with the designation: the first.
    const Found named = find(finder, Motor::Type::SINGLE, "Estes", "B6", std::nullopt);
    EXPECT_EQ(named.motor, b60);
    EXPECT_EQ(named.texts(),
              std::vector<std::string>{"Multiple motors with designation 'B6' for manufacturer "
                                       "'Estes' found, one chosen arbitrarily."});
    EXPECT_EQ(named.warnings.begin()->priority(), MessagePriority::NORMAL);
    const Found anonymous = find(finder, std::nullopt, std::nullopt, "B6", std::nullopt);
    EXPECT_EQ(anonymous.motor, b60);
    EXPECT_EQ(anonymous.texts(),
              std::vector<std::string>{
                  "Multiple motors with designation 'B6' found, one chosen arbitrarily."});
    // An empty manufacturer is one.
    EXPECT_EQ(find(finder, std::nullopt, "", "B6", std::nullopt).texts(),
              std::vector<std::string>{"Multiple motors with designation 'B6' for manufacturer '' "
                                       "found, one chosen arbitrarily."});
}

/// A finder that puts a stand-in where a motor is missing, as the comment of
/// handleMissingMotor() invites.
class StandInFinder final : public DatabaseMotorFinder
{
public:
    StandInFinder(const MotorDatabase& database, std::shared_ptr<const Motor> standIn)
      : DatabaseMotorFinder(database), m_standIn(std::move(standIn))
    {
    }

protected:
    [[nodiscard]] std::shared_ptr<const Motor> handleMissingMotor(
        std::optional<Motor::Type> /*type*/, std::optional<std::string_view> /*manufacturer*/,
        std::string_view designation, double /*diameter*/, double /*length*/,
        std::optional<std::string_view> /*digest*/, WarningSet& warnings) const override
    {
        warnings.add("stand-in for " + std::string(designation));
        return m_standIn;
    }

private:
    std::shared_ptr<const Motor> m_standIn;
};

TEST(DatabaseMotorFinder, ASubclassDecidesWhatAMissingMotorBecomes)
{
    const std::shared_ptr<const ThrustCurveMotor> standIn = makeEmbeddedTestMotor("X1", 1.0, "s");
    const FixedDatabase                           database;
    const StandInFinder                           finder(database, standIn);
    const Found found = find(finder, std::nullopt, std::nullopt, "C6", std::nullopt);
    EXPECT_EQ(found.motor, standIn);
    EXPECT_EQ(found.texts(), std::vector<std::string>{"stand-in for C6"});
}

// ---- the bundled database --------------------------------------------------------------------

TEST(DatabaseMotorFinderBundled, TheDatabaseIsTheOneOpenRocketsAnswersWereMadeWith)
{
    // "database: 1591 curves read, 1458 sets, 1588 motors"
    std::size_t motors = 0;
    for (const QtRocket::ThrustCurveMotorSet& set : bundledMotorDatabase().getMotorSets())
    {
        motors += set.getMotors().size();
    }
    EXPECT_EQ(bundledMotorDatabase().getMotorSets().size(), 1458U);
    EXPECT_EQ(motors, 1588U);
}

/// How the motor found relates to the digest of the file.
enum class Match
{
    EXACT,        ///< the motor's digest is the file's
    COMPATIBLE,   ///< the file's digest is one of an older format of the motor's curve
    DESCRIPTION,  ///< the digest does not fit: the motor is taken by its description, silently
};

/// One <motor> reference of an example design, and what OpenRocket makes of it.
struct ExampleMotor
{
    std::string_view file;
    Motor::Type      type;
    std::string_view manufacturer;
    std::string_view designation;
    std::string_view digest;  ///< the file's
    std::string_view found;   ///< the digest of the motor OpenRocket loads
    std::size_t      candidates;
    Match            match;
};

/// Every distinct motor reference of the 16 example designs, by file (41; the files hold 65
/// <motor> elements): expected-motors.tsv, with the kind of match of motor-queries.tsv.
// clang-format off
constexpr auto kExampleMotors = std::to_array<ExampleMotor>({
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "524787762ca4da7db6dbfb7e43b4cd09", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = Match::DESCRIPTION},
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "ARC payload rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "F50T", .digest = "019164167fce99d0709c22dc0e410ad7", .found = "88ed07b96422ec99767fb35bf6a51966", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Airstart timing.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "K550W", .digest = "822e1d4ed7924f9dea7a62b5f5ff2823", .found = "9e8c531f225566c6c75eedaeacfcfef1", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Airstart timing.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I211W", .digest = "6f055ac96725164ac5814d96f109b8d5", .found = "8a203416fee4400cdaf09edf369235af", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Chute release.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "G40W", .digest = "96f84e3d549cb6ae6e52df6732fe1bfc", .found = "7c6080928783078289d9a473efecc134", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Chute release.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "G80T", .digest = "bc77ef06b0e4f9471985fe24e1d7fbd6", .found = "6438359c302fd7c031af855f17d8d27e", .candidates = 8, .match = Match::DESCRIPTION},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H669N", .digest = "dfeac53db6e18bc3583856fb9f06272e", .found = "26a5e7834018943090396d419ca64662", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H242T", .digest = "e49249080ef9c1671ec2a86418848af9", .found = "cff44a78b2c4bc9e2ef19f6a2ded4da0", .candidates = 2, .match = Match::DESCRIPTION},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "J570W", .digest = "14373c65485521035497b1db20e110d3", .found = "6eb7fe0c078fe0365e2052d8c877f6a4", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H999N", .digest = "e8c8cdd09c63c4fc3d6790252de6049a", .found = "871f7fe309f61ec7e45e4b29833349d9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I1299N", .digest = "6b4aebccc355196a4e289ce84d517b0e", .found = "e26c7159754a0c58db92d8c5b3568902", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "G64W", .digest = "d72e5f01400ce95e40c3f4f4990a67f8", .found = "0fa1ede88d6407eadc5bbc413810100d", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Parallel booster staging.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I115W", .digest = "18b10160746afa88aa571c950221cb06", .found = "0039ed088e61360d934d9bd8503fad92", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Parallel booster staging.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "E12", .digest = "27575268e95ac6983801956efb389764", .found = "7baff5a049a60829dbb8316d1b183e86", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "f3a785e1523935caf239c7366cf81fee", .found = "bd060845629e4cfceec7e9b19297ab9f", .candidates = 2, .match = Match::DESCRIPTION},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "524787762ca4da7db6dbfb7e43b4cd09", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = Match::DESCRIPTION},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Quest", .designation = "C12", .digest = "14b080b33be165c3309254a604bcecba", .found = "a47e144089f502e95b8a16130db9b416", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Quest", .designation = "D16", .digest = "99efdce542a60845c9b82c846fb59d4c", .found = "597863e31e02f245b23d2bfd550f615e", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Pods--powered with recovery deployment.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A10", .digest = "e5b53def203dd437ebf0d67846f6cd3b", .found = "e5b53def203dd437ebf0d67846f6cd3b", .candidates = 1, .match = Match::EXACT},
    {.file = "Pods--powered with recovery deployment.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "bd060845629e4cfceec7e9b19297ab9f", .found = "bd060845629e4cfceec7e9b19297ab9f", .candidates = 1, .match = Match::EXACT},
    {.file = "Simulation extensions.ork", .type = Motor::Type::HYBRID, .manufacturer = "HyperTEK", .designation = "2800CC172L-L540", .digest = "08bbc968cab39e437cf3721ae4fac3f1", .found = "5498ead583ab6cd6900a533b1cb69df8", .candidates = 1, .match = Match::DESCRIPTION},
    {.file = "Simulation scripting.ork", .type = Motor::Type::HYBRID, .manufacturer = "HyperTEK", .designation = "2800CC172L-L540", .digest = "08bbc968cab39e437cf3721ae4fac3f1", .found = "5498ead583ab6cd6900a533b1cb69df8", .candidates = 1, .match = Match::DESCRIPTION},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = Match::COMPATIBLE},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "ebc2af7f8c6f8665d1bec720ddc2b5e1", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Tube fin rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "D12", .digest = "ec683e131a2b32950561abfe011b0afc", .found = "c26987c1c7e95810bbb6f2e284861494", .candidates = 1, .match = Match::COMPATIBLE},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H148R", .digest = "a06234b6049c1079e394cb9ecd4607a9", .found = "a06234b6049c1079e394cb9ecd4607a9", .candidates = 1, .match = Match::EXACT},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I59WN", .digest = "a0006978c9a542518b425c0caa67042b", .found = "a0006978c9a542518b425c0caa67042b", .candidates = 1, .match = Match::EXACT},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I357T", .digest = "1a3327383625336706131b2a9d198139", .found = "1a3327383625336706131b2a9d198139", .candidates = 1, .match = Match::EXACT},
});
// clang-format on

[[nodiscard]] std::string_view nameOf(Match match) noexcept
{
    switch (match)
    {
        case Match::EXACT:
            return "exact";
        case Match::COMPATIBLE:
            return "compatible";
        case Match::DESCRIPTION:
            return "description";
    }
    return "?";
}

/// What the table says of @p example: "<digest> <designation> <candidates> <match> warnings: ".
[[nodiscard]] std::string expected(const ExampleMotor& example)
{
    return std::format("{} {} {} {} warnings: ", example.found, example.designation,
                       example.candidates, nameOf(example.match));
}

/// The same of what the finder makes of @p example in the bundled database, with every warning
/// it adds at the end.
[[nodiscard]] std::string resolved(const ExampleMotor& example)
{
    const Found found =
        findBundled(example.type, example.manufacturer, example.designation, example.digest);
    if (found.motor == nullptr)
    {
        return "no motor";
    }
    Match match = Match::DESCRIPTION;
    if (found.motor->getDigest() == example.digest)
    {
        match = Match::EXACT;
    }
    else if (MotorDigest::isDigestCompatible(*found.motor, example.digest))
    {
        match = Match::COMPATIBLE;
    }
    std::string text = std::format(
        "{} {} {} {} warnings: ", found.motor->getDigest(), found.motor->getDesignation(),
        candidates(example.type, example.manufacturer, example.designation, example.digest),
        nameOf(match));
    for (const std::string& warning : found.texts())
    {
        text += warning + "; ";
    }
    return text;
}

TEST(DatabaseMotorFinderBundled, EveryMotorOfTheExamplesResolvesAsOpenRockets)
{
    // No example motor gives a warning, the seven that are another curve included.
    for (const ExampleMotor& example : kExampleMotors)
    {
        EXPECT_EQ(resolved(example), expected(example))
            << example.file << ": " << example.manufacturer << " " << example.designation;
    }
}

TEST(DatabaseMotorFinderBundled, TheExampleTableIsComplete)
{
    // examples.md: 5 exact, 29 by a digest of an older format, 7 by the description.
    std::size_t exact       = 0;
    std::size_t compatible  = 0;
    std::size_t description = 0;
    for (const ExampleMotor& example : kExampleMotors)
    {
        exact += example.match == Match::EXACT ? 1U : 0U;
        compatible += example.match == Match::COMPATIBLE ? 1U : 0U;
        description += example.match == Match::DESCRIPTION ? 1U : 0U;
    }
    EXPECT_EQ(kExampleMotors.size(), 41U);
    EXPECT_EQ(exact, 5U);
    EXPECT_EQ(compatible, 29U);
    EXPECT_EQ(description, 7U);
}

TEST(DatabaseMotorFinderBundled, AFileOlderThanTheDigestTakesTheFirstWithAWarning)
{
    // tests/data/ork/simplerocket.ork (format 1.2): its digest is not read.
    const Found found = findBundled(std::nullopt, "Estes", "A8", std::nullopt);
    EXPECT_EQ(found.describe(), "Estes/A8/bd060845629e4cfceec7e9b19297ab9f");
    EXPECT_EQ(found.texts(),
              std::vector<std::string>{"Multiple motors with designation 'A8' for manufacturer "
                                       "'Estes' found, one chosen arbitrarily."});
    EXPECT_EQ(candidates(std::nullopt, "Estes", "A8", std::nullopt), 2U);
}

TEST(DatabaseMotorFinderBundled, NoDesignation)
{
    EXPECT_EQ(findBundled(std::nullopt, std::nullopt, std::nullopt, std::nullopt).texts(),
              std::vector<std::string>{"No motor specified, ignoring."});
    const Found found =
        findBundled(Motor::Type::SINGLE, "Estes", std::nullopt, "2967cd7a160b396ef96f09695429d8e9");
    EXPECT_EQ(found.describe(), "null");
    EXPECT_EQ(found.texts(), std::vector<std::string>{"No motor specified, ignoring."});
}

TEST(DatabaseMotorFinderBundled, MissingMotors)
{
    EXPECT_EQ(findBundled(std::nullopt, std::nullopt, "ZZZ999", std::nullopt).texts(),
              std::vector<std::string>{"No motor with designation 'ZZZ999' found."});
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "Estes", "ZZZ999", std::nullopt).texts(),
              std::vector<std::string>{
                  "No motor with designation 'ZZZ999' for manufacturer 'Estes' found."});
    // The type is a criterion: no hybrid C6.
    const Found hybrid = findBundled(Motor::Type::HYBRID, "Estes", "C6", "abc");
    EXPECT_EQ(hybrid.describe(), "null");
    EXPECT_EQ(
        hybrid.texts(),
        std::vector<std::string>{"No motor with designation 'C6' for manufacturer 'Estes' found."});
    const auto* missing = dynamic_cast<const Warning::MissingMotor*>(&*hybrid.warnings.begin());
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->type(), std::optional<std::string>("HYBRID"));
    EXPECT_EQ(missing->digest(), std::optional<std::string>("abc"));
    EXPECT_EQ(findBundled(std::nullopt, "Nobody", "C6", std::nullopt).texts(),
              std::vector<std::string>{
                  "No motor with designation 'C6' for manufacturer 'Nobody' found."});
    EXPECT_EQ(
        findBundled(Motor::Type::SINGLE, "", "C6", std::nullopt).texts(),
        std::vector<std::string>{"No motor with designation 'C6' for manufacturer '' found."});
}

TEST(DatabaseMotorFinderBundled, OneCandidate)
{
    const Found plain = findBundled(Motor::Type::SINGLE, "Estes", "E12", std::nullopt);
    EXPECT_EQ(plain.describe(), "Estes/E12/7baff5a049a60829dbb8316d1b183e86");
    EXPECT_TRUE(plain.warnings.empty());
    const Found wrongDigest = findBundled(Motor::Type::SINGLE, "Estes", "E12", "no-such-digest");
    EXPECT_EQ(wrongDigest.describe(), "Estes/E12/7baff5a049a60829dbb8316d1b183e86");
    EXPECT_TRUE(wrongDigest.warnings.empty());
    // The digest alone finds a motor whose description does not fit.
    const Found byDigest =
        findBundled(Motor::Type::SINGLE, "Estes", "ZZZ999", "2967cd7a160b396ef96f09695429d8e9");
    EXPECT_EQ(byDigest.describe(), "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    EXPECT_TRUE(byDigest.warnings.empty());
    EXPECT_EQ(findBundled(std::nullopt, std::nullopt, "ZZZ999", "2967cd7a160b396ef96f09695429d8e9")
                  .describe(),
              "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
}

TEST(DatabaseMotorFinderBundled, SeveralCandidatesWithoutADigest)
{
    const Found estes = findBundled(Motor::Type::SINGLE, "Estes", "C6", std::nullopt);
    EXPECT_EQ(estes.describe(), "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    EXPECT_EQ(estes.texts(),
              std::vector<std::string>{"Multiple motors with designation 'C6' for manufacturer "
                                       "'Estes' found, one chosen arbitrarily."});
    EXPECT_EQ(candidates(Motor::Type::SINGLE, "Estes", "C6", std::nullopt), 2U);

    // Without a manufacturer the first of seven is another maker's.
    const Found any = findBundled(std::nullopt, std::nullopt, "C6", std::nullopt);
    EXPECT_EQ(any.describe(), "Klima/C6/1e84224430ae4ea4b4c190ad7049baa1");
    EXPECT_EQ(any.texts(),
              std::vector<std::string>{
                  "Multiple motors with designation 'C6' found, one chosen arbitrarily."});
    EXPECT_EQ(candidates(std::nullopt, std::nullopt, "C6", std::nullopt), 7U);

    // An empty designation is contained in every one: the first motor of the database.
    const Found all = findBundled(std::nullopt, std::nullopt, "", std::nullopt);
    EXPECT_EQ(all.describe(), "Klima/A6/418fa5ae09fdb53a19cd751fe289ba5b");
    EXPECT_EQ(all.texts(),
              std::vector<std::string>{
                  "Multiple motors with designation '' found, one chosen arbitrarily."});
    EXPECT_EQ(candidates(std::nullopt, std::nullopt, "", std::nullopt), 1588U);
}

TEST(DatabaseMotorFinderBundled, SeveralCandidatesWithADigest)
{
    // A digest of an older format picks its curve of the two Estes C6.
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "Estes", "C6", "c8743ef3fa99e14a89885cbe0ead47b2")
                  .describe(),
              "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    // The second of the two Estes A8 by such a digest, the first when the digest fits neither.
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "Estes", "A8", "22aec01287ea1e3b8c6f66b26fe5fea6")
                  .describe(),
              "Estes/A8/c049499ca03fd69115352e5d4be72de7");
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "Estes", "A8", "f3a785e1523935caf239c7366cf81fee")
                  .describe(),
              "Estes/A8/bd060845629e4cfceec7e9b19297ab9f");
    // No digest fits: part of a digest is no digest, nor is an empty one.
    const Found partial = findBundled(Motor::Type::SINGLE, "Estes", "C6", "9b06acb7");
    EXPECT_EQ(partial.describe(), "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    EXPECT_TRUE(partial.warnings.empty());
    const Found empty = findBundled(Motor::Type::SINGLE, "Estes", "C6", "");
    EXPECT_EQ(empty.describe(), "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    EXPECT_TRUE(empty.warnings.empty());
    const Found nothing = findBundled(Motor::Type::SINGLE, "Estes", "C6", "nothing");
    EXPECT_EQ(nothing.describe(), "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
    EXPECT_TRUE(nothing.warnings.empty());
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "Estes", "c6", "nothing").describe(),
              "Estes/C6/2967cd7a160b396ef96f09695429d8e9");
}

TEST(DatabaseMotorFinderBundled, TheDesignationDecidesBetweenCandidatesWhoseDigestsDoNotFit)
{
    // Eight AeroTech motors hold "G80" or are held by it: G80T (four curves), G8ST (two), G80
    // and G80NBT.
    EXPECT_EQ(candidates(Motor::Type::SINGLE, "AeroTech", "G80", "nothing"), 8U);
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "AeroTech", "G80T", "nothing").describe(),
              "AeroTech/G80T/6438359c302fd7c031af855f17d8d27e");
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "AeroTech", "g80t", "nothing").describe(),
              "AeroTech/G80T/6438359c302fd7c031af855f17d8d27e");
    // The seventh candidate is the one named exactly "G80".
    EXPECT_EQ(findBundled(Motor::Type::SINGLE, "AeroTech", "G80", "nothing").describe(),
              "AeroTech/G80/3d8d52cbd5a4774fdc9380846aeace3d");
    // None is named "G8": the first.
    const Found first = findBundled(Motor::Type::SINGLE, "AeroTech", "G8", "nothing");
    EXPECT_EQ(first.describe(), "AeroTech/G80T/6438359c302fd7c031af855f17d8d27e");
    EXPECT_TRUE(first.warnings.empty());

    EXPECT_EQ(findBundled(std::nullopt, std::nullopt, "C6", "nothing").describe(),
              "Klima/C6/1e84224430ae4ea4b4c190ad7049baa1");
    EXPECT_EQ(candidates(std::nullopt, std::nullopt, "6", "nothing"), 383U);
    EXPECT_EQ(findBundled(std::nullopt, std::nullopt, "6", "nothing").describe(),
              "Klima/A6/418fa5ae09fdb53a19cd751fe289ba5b");
}

}  // namespace
