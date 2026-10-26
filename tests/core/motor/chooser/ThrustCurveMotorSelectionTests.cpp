#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

// OpenRocket has no JUnit test of ThrustCurveMotorSelectionPanel; its behaviour over the bundled
// database is pinned in motor_chooser_golden_tests.cpp. These tests cover the rules one by one on a
// small database of their own.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorHolder;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSelection;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::ThrustCurveMotorSetDatabase;
namespace MotorPreferences = QtRocket::MotorPreferences;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;
using Texts    = std::vector<std::string>;

/// What a test motor is made of.
struct MotorSpec
{
    std::string         manufacturer;
    std::string         designation;
    double              diameter;
    std::vector<double> time;
    std::vector<double> thrust;
    std::vector<double> delays;
    std::string         digest;
    std::string         description;
};

MotorPtr makeMotor(MotorSpec spec)
{
    std::vector<Coordinate> cg;
    cg.reserve(spec.time.size());
    for (std::size_t i = 0; i < spec.time.size(); i++)
    {
        cg.emplace_back(0.035, 0, 0, 0.02 - (0.001 * static_cast<double>(i)));
    }
    QtRocket::Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer(spec.manufacturer))
            .setDesignation(std::move(spec.designation))
            .setDiameter(spec.diameter)
            .setLength(0.07)
            .setTimePoints(std::move(spec.time))
            .setThrustPoints(std::move(spec.thrust))
            .setCGPoints(std::move(cg))
            .setStandardDelays(std::move(spec.delays))
            .setDigest(std::move(spec.digest))
            .setDescription(std::move(spec.description))
            .setMotorType(Motor::Type::SINGLE)
            .build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// A small database: Estes C6 with three curves (the best, one just like it, and another), Estes
/// D12, AeroTech H128W and an Estes Z0 without standard delays, in that order.
struct SmallDatabase
{
    MotorPtr                    c6Best    = makeMotor({.manufacturer = "Estes",
                                                       .designation  = "C6",
                                                       .diameter     = 0.018,
                                                       .time         = {0, 0.5, 1, 1.5, 2},
                                                       .thrust       = {0, 10, 5, 5, 0},
                                                       .delays       = {0, 3, 5, 7},
                                                       .digest       = "c6-best",
                                                       .description  = "best curve"});
    MotorPtr                    c6Similar = makeMotor({.manufacturer = "Estes",
                                                       .designation  = "C6",
                                                       .diameter     = 0.018,
                                                       .time         = {0, 0.5, 1, 1.5, 2},
                                                       .thrust       = {0, 10, 5, 5, 0},
                                                       .delays       = {0, 3, 5, 7},
                                                       .digest       = "c6-similar",
                                                       .description  = ""});
    MotorPtr                    c6Other   = makeMotor({.manufacturer = "Estes",
                                                       .designation  = "C6",
                                                       .diameter     = 0.018,
                                                       .time         = {0, 0.2, 0.4},
                                                       .thrust       = {0, 20, 0},
                                                       .delays       = {0, 3, 5, 7},
                                                       .digest       = "c6-other",
                                                       .description  = ""});
    MotorPtr                    d12       = makeMotor({.manufacturer = "Estes",
                                                       .designation  = "D12",
                                                       .diameter     = 0.024,
                                                       .time         = {0, 1, 2},
                                                       .thrust       = {0, 12, 0},
                                                       .delays       = {0, 3, 5, 7},
                                                       .digest       = "d12",
                                                       .description  = ""});
    MotorPtr                    h128      = makeMotor({.manufacturer = "AeroTech",
                                                       .designation  = "H128W",
                                                       .diameter     = 0.029,
                                                       .time         = {0, 1, 2},
                                                       .thrust       = {0, 128, 0},
                                                       .delays       = {4, 6, 8, 10, 14},
                                                       .digest       = "h128",
                                                       .description  = ""});
    MotorPtr                    z0        = makeMotor({.manufacturer = "Estes",
                                                       .designation  = "Z0",
                                                       .diameter     = 0.018,
                                                       .time         = {0, 1, 2},
                                                       .thrust       = {0, 1, 0},
                                                       .delays       = {},
                                                       .digest       = "z0",
                                                       .description  = ""});
    ThrustCurveMotorSetDatabase database;

    SmallDatabase()
    {
        for (const MotorPtr& motor : {c6Best, c6Similar, c6Other, d12, h128, z0})
        {
            database.addMotor(motor);
        }
    }

    [[nodiscard]] const ThrustCurveMotorSet& set(std::size_t index) const
    {
        return database.getMotorSets().at(index);
    }
};

constexpr std::size_t kC6Row   = 0;
constexpr std::size_t kD12Row  = 1;
constexpr std::size_t kH128Row = 2;
constexpr std::size_t kZ0Row   = 3;

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

    /// A configuration of the mount with @p motor, @p delay and @p nozzle.
    FlightConfigurationId add(MotorPtr motor, double delay, double nozzle = 0)
    {
        const FlightConfigurationId fcid;
        rocket.createFlightConfiguration(fcid);
        MotorConfiguration configuration(*mount, fcid);
        configuration.setMotor(std::move(motor));
        configuration.setEjectionDelay(delay);
        EXPECT_TRUE(configuration.setNozzleExitDiameter(nozzle).has_value());
        mount->setMotorConfig(configuration, fcid);
        return fcid;
    }
};

std::vector<MotorPtr> motorsOf(const std::vector<MotorHolder>& items)
{
    std::vector<MotorPtr> motors;
    motors.reserve(items.size());
    for (const MotorHolder& holder : items)
    {
        motors.push_back(holder.getMotor());
    }
    return motors;
}

TEST(ThrustCurveMotorSelection, StartsWithNothingSelected)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    EXPECT_EQ(selection.getSelectedMotor(), nullptr);
    EXPECT_EQ(selection.getSelectedMotorSet(), nullptr);
    EXPECT_EQ(selection.getSelectedDelay(), 0.0);
    EXPECT_TRUE(selection.getDelayItems().empty());
    EXPECT_EQ(selection.getDelayText(), "");
    EXPECT_EQ(selection.getNozzleExitDiameter(), 0.0);
    EXPECT_EQ(selection.getMaximumNozzleExitDiameter(), 0.0);
    EXPECT_TRUE(selection.getFilteredCurves().empty());
    EXPECT_TRUE(selection.isHideSimilar());
    EXPECT_TRUE(selection.isHideUnavailable());
    EXPECT_EQ(selection.getModel().getRowCount(), 4U);
    // By diameter, then total impulse: Z0 (18 mm, 1 Ns), C6 (18 mm, 10 Ns), D12, H128W.
    EXPECT_EQ(selection.getViewRows(),
              (std::vector<std::size_t>{kZ0Row, kC6Row, kD12Row, kH128Row}));
    // The constructor stores nothing.
    EXPECT_TRUE(preferences.keys().empty());
}

TEST(ThrustCurveMotorSelection, TakesItsFiltersFromThePreferences)
{
    const SmallDatabase data;
    InMemoryPreferences preferences;
    MotorPreferences::setHideSimilar(preferences, false);
    MotorPreferences::setHideUnavailable(preferences, false);
    const std::vector<const Manufacturer*> excluded{&Manufacturer::getManufacturer("Estes")};
    MotorPreferences::setExcludedMotorManufacturers(preferences, excluded);
    const ThrustCurveMotorSelection selection(data.database, preferences);
    EXPECT_FALSE(selection.isHideSimilar());
    EXPECT_FALSE(selection.isHideUnavailable());
    EXPECT_EQ(selection.getRowFilter().getExcludedManufacturers(), excluded);
    EXPECT_EQ(selection.getViewRows(), (std::vector<std::size_t>{kH128Row}));
}

/// The filter panel's list: every manufacturer once, by simple name as String.compareTo orders it
/// (capitals first).
TEST(ThrustCurveMotorSelection, ListsTheManufacturersBySimpleName)
{
    const SmallDatabase             data;
    InMemoryPreferences             preferences;
    const ThrustCurveMotorSelection selection(data.database, preferences);
    EXPECT_EQ(selection.getManufacturers(),
              (std::vector<const Manufacturer*>{&Manufacturer::getManufacturer("AeroTech"),
                                                &Manufacturer::getManufacturer("Estes")}));
}

/// selectMotor(): the only curve; else the preferred one; else the best by goodness.
TEST(ThrustCurveMotorSelection, ChoosesTheDefaultCurveOfASet)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    EXPECT_EQ(selection.selectMotor(data.set(kD12Row)), data.d12);
    EXPECT_EQ(selection.selectMotor(data.set(kC6Row)), data.c6Best);
    MotorPreferences::storePreferredThrustCurve(preferences, data.set(kC6Row), *data.c6Other);
    EXPECT_EQ(selection.selectMotor(data.set(kC6Row)), data.c6Other);
    EXPECT_THROW(static_cast<void>(selection.selectMotor(ThrustCurveMotorSet{})), BugError);
}

/// A set selected through the table: its default curve, the delay closest to the one before (a
/// tie goes to the later delay), the nozzle reset with the motor's diameter as its maximum.
TEST(ThrustCurveMotorSelection, SelectsASetThroughTheTable)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    EXPECT_EQ(selection.getSelectedMotor(), data.c6Best);
    EXPECT_EQ(selection.getSelectedMotorSet(), &data.set(kC6Row));
    EXPECT_EQ(selection.getSelectedDelay(), 0.0);
    EXPECT_EQ(selection.getDelayItems(), (Texts{"0", "3", "5", "7", "Plugged"}));
    EXPECT_EQ(selection.getDelayText(), "0");
    EXPECT_EQ(selection.getMaximumNozzleExitDiameter(), 0.018);

    selection.setDelayText("4");
    selection.selectMotorFromTable(kD12Row);
    EXPECT_EQ(selection.getSelectedMotor(), data.d12);
    EXPECT_EQ(selection.getSelectedDelay(), 5.0);
    selection.setDelayText("Plugged");
    selection.selectMotorFromTable(kH128Row);
    EXPECT_EQ(selection.getSelectedDelay(), 14.0);
    EXPECT_EQ(selection.getDelayItems(), (Texts{"4", "6", "8", "10", "14", "Plugged"}));
}

/// The same set again changes nothing: the typed delay stays.
TEST(ThrustCurveMotorSelection, KeepsTheDelayWithinASet)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    selection.setDelayText("4.5");
    selection.selectMotorFromTable(kC6Row);
    EXPECT_EQ(selection.getSelectedDelay(), 4.5);
    selection.select(data.c6Other);
    EXPECT_EQ(selection.getSelectedMotor(), data.c6Other);
    EXPECT_EQ(selection.getSelectedDelay(), 4.5);
}

/// A set without standard delays offers "Plugged" alone and selects it (OpenRocket's combo box
/// reads its "Plugged" item back).
TEST(ThrustCurveMotorSelection, PlugsASetWithoutDelays)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.setDelayText("5");
    selection.selectMotorFromTable(kZ0Row);
    EXPECT_EQ(selection.getDelayItems(), (Texts{"Plugged"}));
    EXPECT_EQ(selection.getSelectedDelay(), Motor::kPluggedDelay);
    EXPECT_EQ(selection.getDelayText(), "Plugged");
}

/// The delay box: "Plugged" and "None" in any case, Java's number syntax, the shown text not read
/// back; text that is no number, and NaN (M3's bug 11), keep the delay.
TEST(ThrustCurveMotorSelection, ReadsTheTypedDelay)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    selection.setDelayText("5.04");
    EXPECT_EQ(selection.getSelectedDelay(), 5.04);
    EXPECT_EQ(selection.getDelayText(), "5");
    selection.setDelayText("abc");
    EXPECT_EQ(selection.getSelectedDelay(), 5.04);
    selection.setDelayText("NaN");
    EXPECT_EQ(selection.getSelectedDelay(), 5.04);
    selection.setDelayText("pLuGgEd");
    EXPECT_EQ(selection.getSelectedDelay(), Motor::kPluggedDelay);
    selection.setDelayText(" 0x1p3 ");
    EXPECT_EQ(selection.getSelectedDelay(), 8.0);
    selection.setDelayText("NONE");
    EXPECT_EQ(selection.getSelectedDelay(), Motor::kPluggedDelay);
}

/// The GUI's translations of "Plugged" and "None".
TEST(ThrustCurveMotorSelection, UsesTheGivenPluggedTexts)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.setDelayTexts("Zu", "Keine");
    selection.selectMotorFromTable(kC6Row);
    EXPECT_EQ(selection.getDelayItems(), (Texts{"0", "3", "5", "7", "Zu"}));
    selection.setDelayText("keine");
    EXPECT_EQ(selection.getSelectedDelay(), Motor::kPluggedDelay);
    EXPECT_EQ(selection.getDelayText(), "Zu");
    selection.setDelayText("Plugged");
    EXPECT_EQ(selection.getSelectedDelay(), Motor::kPluggedDelay);
    selection.setDelayText("3");
    selection.setDelayText("Plugged");
    EXPECT_EQ(selection.getSelectedDelay(), 3.0);
}

/// Opening for a configuration selects its motor with its nozzle and keeps its delay, also one
/// that is no standard delay (QtRocket fixes M3's bug 8: OpenRocket snaps 6 s to 7 s).
TEST(ThrustCurveMotorSelection, KeepsTheDelayOfTheConfiguration)
{
    const SmallDatabase         data;
    InMemoryPreferences         preferences;
    ThrustCurveMotorSelection   selection(data.database, preferences);
    MountedRocket               rocket;
    const FlightConfigurationId fcid = rocket.add(data.c6Other, 6, 0.01);
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(*rocket.mount, fcid));
    EXPECT_EQ(selection.getSelectedMotor(), data.c6Other);
    EXPECT_EQ(selection.getSelectedDelay(), 6.0);
    EXPECT_EQ(selection.getDelayText(), "6");
    EXPECT_EQ(selection.getNozzleExitDiameter(), 0.01);
    EXPECT_EQ(selection.getRowFilter().getUsedMotors().size(), 1U);
}

/// A NaN delay has no closest standard delay but the last: OpenRocket's answer stays.
TEST(ThrustCurveMotorSelection, ReplacesANanDelayOfTheConfiguration)
{
    const SmallDatabase         data;
    InMemoryPreferences         preferences;
    ThrustCurveMotorSelection   selection(data.database, preferences);
    MountedRocket               rocket;
    const FlightConfigurationId fcid =
        rocket.add(data.d12, std::numeric_limits<double>::quiet_NaN());
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(*rocket.mount, fcid));
    EXPECT_EQ(selection.getSelectedDelay(), 7.0);
}

/// A mount without a motor in the configuration selects nothing, and forgets the selection before.
TEST(ThrustCurveMotorSelection, SelectsNothingForAnEmptyConfiguration)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    MountedRocket rocket;
    rocket.add(data.d12, 5);
    const FlightConfigurationId empty;
    rocket.rocket.createFlightConfiguration(empty);
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(*rocket.mount, empty));
    EXPECT_EQ(selection.getSelectedMotor(), nullptr);
    EXPECT_EQ(selection.getSelectedDelay(), 0.0);
    EXPECT_EQ(selection.getRowFilter().getUsedMotors().size(), 1U);
}

/// A motor in no set gets a set of its own; the list is then sorted (the model order of the table),
/// and the same motor later finds that set.
TEST(ThrustCurveMotorSelection, AddsASetForAMotorOfNoSet)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    const MotorPtr            custom = makeMotor({.manufacturer = "AeroTech",
                                                  .designation  = "A1",
                                                  .diameter     = 0.018,
                                                  .time         = {0, 1},
                                                  .thrust       = {0, 1},
                                                  .delays       = {3},
                                                  .digest       = "custom",
                                                  .description  = ""});
    ASSERT_NE(custom, nullptr);
    EXPECT_EQ(selection.findMotorSet(*custom), nullptr);
    MountedRocket               rocket;
    const FlightConfigurationId fcid = rocket.add(custom, 3);
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(*rocket.mount, fcid));
    EXPECT_EQ(selection.getModel().getRowCount(), 5U);
    ASSERT_NE(selection.findMotorSet(*custom), nullptr);
    EXPECT_EQ(selection.getModel().getIndex(*selection.findMotorSet(*custom)), 0U);
    EXPECT_EQ(selection.getModel().getIndex(data.set(kH128Row)), 1U);
    EXPECT_EQ(selection.getModel().getIndex(data.set(kC6Row)), 2U);
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(*rocket.mount, fcid));
    EXPECT_EQ(selection.getModel().getRowCount(), 5U);
}

/// "Hide very similar thrust curves" hides a curve just like the selected one, compared with the
/// selected one; off, every curve is listed, the best first.
TEST(ThrustCurveMotorSelection, HidesVerySimilarCurves)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    EXPECT_EQ(motorsOf(selection.getCurveItems()),
              (std::vector<MotorPtr>{data.c6Best, data.c6Other}));
    selection.select(data.c6Other);
    EXPECT_EQ(motorsOf(selection.getCurveItems()),
              (std::vector<MotorPtr>{data.c6Best, data.c6Similar, data.c6Other}));
    selection.select(data.c6Best);
    selection.setHideSimilar(false);
    EXPECT_FALSE(MotorPreferences::isHideSimilar(preferences));
    EXPECT_EQ(selection.getFilteredCurves(),
              (std::vector<MotorPtr>{data.c6Best, data.c6Similar, data.c6Other}));
    EXPECT_EQ(selection.getCurveItems().at(2).getIndex(), 2U);
}

/// A curve change resets the nozzle exit diameter; the diameter stays within [0, the motor's].
TEST(ThrustCurveMotorSelection, LimitsTheNozzleExitDiameter)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.selectMotorFromTable(kC6Row);
    selection.setNozzleExitDiameter(1.0);
    EXPECT_EQ(selection.getNozzleExitDiameter(), 0.018);
    selection.setNozzleExitDiameter(-1.0);
    EXPECT_EQ(selection.getNozzleExitDiameter(), 0.0);
    selection.setNozzleExitDiameter(0.01);
    selection.select(data.c6Other);
    EXPECT_EQ(selection.getNozzleExitDiameter(), 0.0);
}

TEST(ThrustCurveMotorSelection, RefusesAMotorOfNoSetOfTheList)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    const MotorPtr            stranger = makeMotor({.manufacturer = "Estes",
                                                    .designation  = "Q1",
                                                    .diameter     = 0.018,
                                                    .time         = {0, 1},
                                                    .thrust       = {0, 1},
                                                    .delays       = {},
                                                    .digest       = "q1",
                                                    .description  = ""});
    ASSERT_NE(stranger, nullptr);
    EXPECT_THROW(selection.select(stranger), BugError);
    selection.select(nullptr);
    EXPECT_EQ(selection.getSelectedMotor(), nullptr);
}

/// OK stores the chosen curve as its set's preferred one; a motor of no set stores nothing.
TEST(ThrustCurveMotorSelection, StoresTheChosenCurve)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.storeSelectedMotor(*data.c6Other);
    EXPECT_TRUE(MotorPreferences::isPreferredThrustCurve(preferences, "c6-other"));
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(preferences, "c6-best"));
    const ThrustCurveMotorSelection later(data.database, preferences);
    EXPECT_EQ(later.selectMotor(data.set(kC6Row)), data.c6Other);
}

/// The two check boxes are stored when changed.
TEST(ThrustCurveMotorSelection, StoresTheCheckBoxes)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    selection.setHideUnavailable(false);
    EXPECT_FALSE(selection.getRowFilter().isHideUnavailable());
    EXPECT_FALSE(MotorPreferences::isHideUnavailable(preferences));
    selection.setHideSimilar(false);
    EXPECT_FALSE(MotorPreferences::isHideSimilar(preferences));
}

/// The selection keeps no part of the rocket: it works on after the rocket is gone.
TEST(ThrustCurveMotorSelection, OutlivesTheRocketItWasOpenedFor)
{
    const SmallDatabase       data;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(data.database, preferences);
    {
        MountedRocket               rocket;
        const FlightConfigurationId fcid = rocket.add(data.d12, 3);
        selection.setMotorMountAndConfig(
            QtRocket::MotorChooserInput::fromMount(*rocket.mount, fcid));
    }
    selection.getRowFilter().setHideUsedMotors(true);
    EXPECT_EQ(selection.getViewRows(), (std::vector<std::size_t>{kZ0Row, kC6Row, kH128Row}));
    EXPECT_EQ(selection.getSelectedMotor(), data.d12);
}

}  // namespace
