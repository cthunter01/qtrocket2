#include "QtRocket/simulation/extension/example/RollControl.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/RollControlProvider.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Signal.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"

namespace
{

using QtRocket::BigDecimal;
using QtRocket::Config;
using QtRocket::FlightConditions;
using QtRocket::FlightDataType;
using QtRocket::RollControl;
using QtRocket::RollControlProvider;
using QtRocket::SimulationComputationListener;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestStatus;

// OpenRocket has no test of RollControl. The expectations here are the Java source's, and for
// the listener the output of Java probes on OpenRocket's compiled core
// (probes/tier9a-extensions/java: RollHooks.java, out/rollhooks-java.txt; for the search of the
// fin set on the two-stage Beta probes/tier9a-fix-extensions/java: FixHooks.java, the "ROLL"
// lines of out/fixhooks-java.txt). The flights are in extension_flight_tests.cpp.

static_assert(std::is_final_v<RollControl>);
static_assert(!std::is_copy_assignable_v<RollControl>);

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The BigDecimal @p text.
[[nodiscard]] BigDecimal big(std::string_view text)
{
    const std::optional<BigDecimal> value = BigDecimal::parse(text);
    if (!value.has_value())
    {
        ADD_FAILURE() << "not a BigDecimal: " << text;
        return BigDecimal::valueOf(0);
    }
    return *value;
}

/// Counts the emissions of a signal while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    mutable int                          m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// ------------------------------------------------------------------------- the extension

TEST(RollControl, HasOpenRocketsIdNameAndDescription)
{
    const RollControl roll;
    EXPECT_EQ(RollControl::kId, "info.openrocket.core.simulation.extension.example.RollControl");
    EXPECT_EQ(roll.getId(), RollControl::kId);
    EXPECT_EQ(roll.getName(), "Roll Control");
    // The probe's line (ExtPins2, NAMES); the alpha (U+03B1) of the symbol as its UTF-8 bytes.
    EXPECT_EQ(roll.getDescription(),
              "Use a PID control to control a rocket's roll.  The current cant angle of the "
              "control finset is published to flight data as \xCE\xB1"
              "fc. Since this extension "
              "modifies design parameters during the simulation, it causes the simulation to run "
              "<b>much</b> more slowly.");
    EXPECT_FALSE(roll.isMonteCarloSafe());
}

TEST(RollControl, HasJavasDefaults)
{
    const RollControl roll;
    // A new extension holds no setting: the getters give the defaults.
    EXPECT_TRUE(roll.getConfig().keySet().empty());
    EXPECT_EQ(roll.getControlFinName(), "CONTROL");
    EXPECT_EQ(roll.getStartTime(), 0.5);
    EXPECT_EQ(roll.getSetPoint(), 0.0);
    EXPECT_EQ(roll.getFinRate(), 10 * std::numbers::pi / 180);
    EXPECT_EQ(roll.getFinRate(), 0.17453292519943295);  // Java: 10 * Math.PI / 180
    EXPECT_EQ(roll.getMaxFinAngle(), 15 * std::numbers::pi / 180);
    EXPECT_EQ(roll.getMaxFinAngle(), 0.2617993877991494);  // Java: 15 * Math.PI / 180
    EXPECT_EQ(roll.getKP(), 0.007);
    EXPECT_EQ(roll.getKI(), 0.2);
}

TEST(RollControl, EverySetterStoresItsKeyAndAnnouncesTheChange)
{
    RollControl         roll;
    const ChangeCounter events(roll.changed());

    roll.setControlFinName("Canards");
    roll.setStartTime(1.25);
    roll.setSetPoint(-2.0);
    roll.setFinRate(0.5);
    roll.setMaxFinAngle(0.1);
    roll.setKP(0.01);
    roll.setKI(0.3);
    EXPECT_EQ(events.count(), 7);

    EXPECT_EQ(roll.getControlFinName(), "Canards");
    EXPECT_EQ(roll.getStartTime(), 1.25);
    EXPECT_EQ(roll.getSetPoint(), -2.0);
    EXPECT_EQ(roll.getFinRate(), 0.5);
    EXPECT_EQ(roll.getMaxFinAngle(), 0.1);
    EXPECT_EQ(roll.getKP(), 0.01);
    EXPECT_EQ(roll.getKI(), 0.3);

    // The keys a .ork file holds, in the order they were put, with Java's types: a String and
    // six Doubles.
    const Config config = roll.getConfig();
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"controlFinName", "startTime", "setPoint",
                                                         "finRate", "maxFinAngle", "KP", "KI"}));
    EXPECT_TRUE(config.get("controlFinName") == Config::Value{"Canards"});
    EXPECT_TRUE(config.get("startTime") == Config::Value{1.25});
    EXPECT_TRUE(config.get("setPoint") == Config::Value{-2.0});
    EXPECT_TRUE(config.get("finRate") == Config::Value{0.5});
    EXPECT_TRUE(config.get("maxFinAngle") == Config::Value{0.1});
    EXPECT_TRUE(config.get("KP") == Config::Value{0.01});
    EXPECT_TRUE(config.get("KI") == Config::Value{0.3});

    // And back: another extension that is given this configuration has the seven settings.
    RollControl other;
    other.setConfig(config);
    EXPECT_EQ(other.getControlFinName(), "Canards");
    EXPECT_EQ(other.getStartTime(), 1.25);
    EXPECT_EQ(other.getSetPoint(), -2.0);
    EXPECT_EQ(other.getFinRate(), 0.5);
    EXPECT_EQ(other.getMaxFinAngle(), 0.1);
    EXPECT_EQ(other.getKP(), 0.01);
    EXPECT_EQ(other.getKI(), 0.3);
    EXPECT_EQ(other.getConfig().keySet(), config.keySet());
    EXPECT_TRUE(other.getConfig().sameEntries(config));

    // A setter announces also a value that is the stored one, as in Java.
    roll.setKI(0.3);
    EXPECT_EQ(events.count(), 8);
}

TEST(RollControl, ReadsItsSettingsFromAConfiguration)
{
    // What the .ork reader does: setConfig() with the entries of the file.
    Config config;
    config.put("controlFinName", "Fins");
    config.put("startTime", 2);               // an Integer
    config.put("setPoint", std::int64_t{3});  // a Long
    config.put("finRate", 1.5F);              // a Float
    config.put("KP", big("0.25"));            // a BigDecimal
    config.put("KI", true);                   // no number: the default
    config.put("maxFinAngle", "wide");        // no number: the default
    config.put("somethingElse", 4.0);         // kept, and ignored

    RollControl         roll;
    const ChangeCounter events(roll.changed());
    roll.setConfig(config);
    EXPECT_EQ(events.count(), 1);

    EXPECT_EQ(roll.getControlFinName(), "Fins");
    EXPECT_EQ(roll.getStartTime(), 2.0);
    EXPECT_EQ(roll.getSetPoint(), 3.0);
    EXPECT_EQ(roll.getFinRate(), 1.5);
    EXPECT_EQ(roll.getKP(), 0.25);
    EXPECT_EQ(roll.getKI(), 0.2);
    EXPECT_EQ(roll.getMaxFinAngle(), 15 * std::numbers::pi / 180);
    EXPECT_TRUE(roll.getConfig().sameEntries(config));

    // A name that is no String gives the default name.
    Config numbered;
    numbered.put("controlFinName", 7);
    roll.setConfig(numbered);
    EXPECT_EQ(roll.getControlFinName(), "CONTROL");
}

TEST(RollControl, CloneCopiesTheConfigurationAndNothingElse)
{
    RollControl roll;
    roll.setControlFinName("Canards");
    roll.setKP(0.5);
    const ChangeCounter events(roll.changed());

    const std::unique_ptr<SimulationExtension> copy  = roll.clone();
    auto*                                      clone = dynamic_cast<RollControl*>(copy.get());
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getId(), RollControl::kId);
    EXPECT_EQ(clone->getName(), "Roll Control");
    EXPECT_TRUE(clone->getConfig().sameEntries(roll.getConfig()));
    EXPECT_EQ(clone->getFlightDataTypes(), roll.getFlightDataTypes());

    // The two have configurations of their own, and the clone has no listeners.
    clone->setKP(0.75);
    EXPECT_EQ(roll.getKP(), 0.5);
    EXPECT_EQ(events.count(), 0);
    roll.setControlFinName("Other");
    EXPECT_EQ(clone->getControlFinName(), "Canards");
}

TEST(RollControl, PublishesTheFinCantTypeOnce)
{
    const RollControl first;
    const RollControl second;
    const RollControl third;

    const FlightDataType& type = RollControl::finCantType();
    EXPECT_EQ(type.getName(), "Control fin cant");
    // A small alpha (U+03B1) and "fc", as the UTF-8 bytes of the symbol.
    EXPECT_EQ(type.getSymbol(),
              "\xCE\xB1"
              "fc");
    EXPECT_EQ(type.getSymbol(), RollControl::kFinCantTypeSymbol);
    EXPECT_EQ(type.getUnitGroupId(), QtRocket::UnitGroupId::ANGLE);
    EXPECT_EQ(type.getGroup(), QtRocket::FlightDataTypeGroup::CUSTOM);
    EXPECT_EQ(type.getPriority(), FlightDataType::kDefaultPriority);
    EXPECT_FALSE(type.isBuiltin());
    EXPECT_EQ(type.getSaveKey(), "Control fin cant");
    EXPECT_EQ(&RollControl::finCantType(), &type);
    EXPECT_EQ(FlightDataType::findBySymbol(RollControl::kFinCantTypeSymbol), &type);

    // Once, however many extensions were made (Java's static list holds it once per
    // extension ever constructed: [Control fin cant, Control fin cant, ...] in the probe).
    EXPECT_EQ(third.getFlightDataTypes(), (std::vector<const FlightDataType*>{&type}));
    EXPECT_EQ(first.getFlightDataTypes(), (std::vector<const FlightDataType*>{&type}));
}

/// What the numbers @p numbers are called, in order.
[[nodiscard]] std::vector<std::string> namesOf(
    const std::vector<SimulationExtension::InputNumber>& numbers)
{
    std::vector<std::string> names;
    names.reserve(numbers.size());
    for (const SimulationExtension::InputNumber& number : numbers)
    {
        names.push_back(number.what);
    }
    return names;
}

/// The values of the numbers @p numbers, in order.
[[nodiscard]] std::vector<double> valuesOf(
    const std::vector<SimulationExtension::InputNumber>& numbers)
{
    std::vector<double> values;
    values.reserve(numbers.size());
    for (const SimulationExtension::InputNumber& number : numbers)
    {
        values.push_back(number.value);
    }
    return values;
}

// QtRocket's own, for Simulation::validateInputs(): the six numbers the listener takes, each
// called by its key in the configuration, with the value the getter gives (the default while
// the key is absent or no number).
TEST(RollControl, ListsTheNumbersItsListenerReads)
{
    RollControl roll;
    EXPECT_EQ(namesOf(roll.getInputNumbers()),
              (std::vector<std::string>{
                  "the 'startTime' of the simulation extension 'Roll Control'",
                  "the 'setPoint' of the simulation extension 'Roll Control'",
                  "the 'finRate' of the simulation extension 'Roll Control'",
                  "the 'maxFinAngle' of the simulation extension 'Roll Control'",
                  "the 'KP' of the simulation extension 'Roll Control'",
                  "the 'KI' of the simulation extension 'Roll Control'",
              }));
    EXPECT_EQ(valuesOf(roll.getInputNumbers()),
              (std::vector<double>{0.5, 0.0, 0.17453292519943295, 0.2617993877991494, 0.007, 0.2}));

    Config config;
    config.put("controlFinName", "Fins");
    config.put("startTime", 2);
    config.put("setPoint", std::numeric_limits<double>::infinity());
    config.put("finRate", 1.5F);
    config.put("maxFinAngle", "wide");
    config.put("KP", big("0.25"));
    config.put("KI", -std::numeric_limits<double>::infinity());
    roll.setConfig(config);
    EXPECT_EQ(
        valuesOf(roll.getInputNumbers()),
        (std::vector<double>{2.0, std::numeric_limits<double>::infinity(), 1.5, 0.2617993877991494,
                             0.25, -std::numeric_limits<double>::infinity()}));

    roll.setKP(kNaN);
    EXPECT_TRUE(std::isnan(valuesOf(roll.getInputNumbers()).at(4)));
    // The clone lists its own.
    const std::unique_ptr<SimulationExtension> copy = roll.clone();
    roll.setKP(1.0);
    EXPECT_TRUE(std::isnan(valuesOf(copy->getInputNumbers()).at(4)));
    EXPECT_EQ(valuesOf(roll.getInputNumbers()).at(4), 1.0);
}

TEST(RollControlProvider, MakesRollControlsUnderOpenRocketsMenuName)
{
    const RollControlProvider provider;
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{std::string(RollControl::kId)}));
    EXPECT_EQ(provider.getName(RollControl::kId),
              (std::vector<std::string>{"Control Enhancements", "Roll Control"}));
    EXPECT_EQ(provider.getName("something.Else"), std::nullopt);

    const std::unique_ptr<SimulationExtension> made = provider.getInstance(RollControl::kId);
    ASSERT_NE(made, nullptr);
    EXPECT_NE(dynamic_cast<RollControl*>(made.get()), nullptr);
    EXPECT_TRUE(made->getConfig().keySet().empty());
    // A new instance every time.
    EXPECT_NE(provider.getInstance(RollControl::kId), made);
}

// --------------------------------------------------------------------------- the listener

/// The listener of a RollControl on a hand-made status of the Estes Alpha III, as RollHooks
/// drives it: the fins ("3 Fin Set") get the cant @p presetCant before the listener starts.
struct Hooked
{
    TestStatus                          s;
    std::shared_ptr<SimulationListener> listener;

    Hooked(double presetCant, RollControl& extension)
      : listener(listenerOf(s, presetCant, extension))
    {
    }

    /// Gives the fins of @p status the cant @p presetCant, initialises @p extension with the
    /// conditions of @p status and returns the listener that added.
    [[nodiscard]] static std::shared_ptr<SimulationListener> listenerOf(TestStatus&  status,
                                                                        double       presetCant,
                                                                        RollControl& extension)
    {
        status.alpha.fins->setCantAngle(presetCant);
        extension.initialize(*status.conditions);
        return status.conditions->getSimulationListenerList().back();
    }

    /// One step as the engine takes it: the flight conditions with the roll rate
    /// @p rollRate, a new row, the simulation time @p time, and the hook after the step.
    void step(double time, double rollRate)
    {
        FlightConditions conditions;
        conditions.setRollRate(rollRate);
        auto* computation = dynamic_cast<SimulationComputationListener*>(listener.get());
        ASSERT_NE(computation, nullptr);
        EXPECT_EQ(computation->postFlightConditions(s.status, conditions), std::nullopt);
        s.status.setSimulationTime(time);
        s.branch->addPoint();
        listener->postStep(s.status);
    }

    /// The cant of the fin set.
    [[nodiscard]] double cant() const { return s.alpha.fins->getCantAngle(); }

    /// The last value of the fin cant column; nullopt while the branch has no such column.
    [[nodiscard]] std::optional<double> column() const
    {
        if (!s.branch->hasType(RollControl::finCantType()))
        {
            return std::nullopt;
        }
        return s.branch->getLast(RollControl::finCantType());
    }
};

/// Expects the cant of the fin set and the last value of the column to be exactly @p cant and
/// @p column (NaN matching NaN).
void expectStep(const Hooked& hooked, double cant, double column)
{
    const std::optional<double> stored = hooked.column();
    if (!stored.has_value())
    {
        ADD_FAILURE() << "the branch has no fin cant column";
        return;
    }
    if (std::isnan(cant))
    {
        EXPECT_TRUE(std::isnan(hooked.cant()));
        EXPECT_TRUE(std::isnan(*stored));
        return;
    }
    EXPECT_EQ(hooked.cant(), cant);
    EXPECT_EQ(*stored, column);
}

/// The message of the SimulationException that startSimulation() of the listener of
/// @p extension throws on the Alpha III; "<none>" when it throws nothing.
[[nodiscard]] std::string startError(RollControl& extension)
{
    Hooked hooked(0.0, extension);
    try
    {
        hooked.listener->startSimulation(hooked.s.status);
    }
    catch (const SimulationException& e)
    {
        return e.what();
    }
    return "<none>";
}

TEST(RollControlListener, IsAUserListenerThatInitializeAdds)
{
    RollControl          roll;
    SimulationConditions conditions;
    roll.initialize(conditions);
    ASSERT_EQ(conditions.getSimulationListenerList().size(), 1U);
    EXPECT_FALSE(conditions.getSimulationListenerList()[0]->isSystemListener());

    // Every initialize() adds a listener of its own.
    roll.initialize(conditions);
    ASSERT_EQ(conditions.getSimulationListenerList().size(), 2U);
    EXPECT_NE(conditions.getSimulationListenerList()[0], conditions.getSimulationListenerList()[1]);
}

// RollHooks: SEQUENCE missing and not-a-fin-set.
TEST(RollControlListener, StopsTheSimulationWithoutAFinSetOfTheName)
{
    RollControl roll;
    EXPECT_EQ(startError(roll), "A fin set with name 'CONTROL' was not found");
    // A component of that name that is no fin set does not count.
    roll.setControlFinName("Body Tube");
    EXPECT_EQ(startError(roll), "A fin set with name 'Body Tube' was not found");
    roll.setControlFinName("3 Fin Set");
    EXPECT_EQ(startError(roll), "<none>");
}

// RollHooks: SEQUENCE defaults. The controller is idle before the start time; it starts from
// a fin angle of 0 (the cant of 0.1 is gone after the first controlled step); the turn rate
// is limited both ways, the angle to +-15 degrees; a roll rate that is no number makes the
// cant no number, and it stays that; the end puts the cant of the start back.
TEST(RollControlListener, ControlsAsInJavaWithTheDefaultSettings)
{
    RollControl roll;
    roll.setControlFinName("3 Fin Set");
    Hooked hooked(0.1, roll);
    hooked.listener->startSimulation(hooked.s.status);
    EXPECT_EQ(hooked.cant(), 0.1);

    hooked.step(0.1, 5.0);
    EXPECT_EQ(hooked.cant(), 0.1);
    EXPECT_EQ(hooked.column(), std::nullopt);
    hooked.step(0.4, 5.0);
    EXPECT_EQ(hooked.cant(), 0.1);
    EXPECT_EQ(hooked.column(), std::nullopt);
    EXPECT_EQ(hooked.s.branch->getTypes().size(), 1U);

    hooked.step(0.6, 2.0);
    expectStep(hooked, -0.034906585039886584, -0.034906585039886584);
    EXPECT_EQ(hooked.s.branch->getTypes().size(), 2U);
    hooked.step(0.7, -1.0);
    expectStep(hooked, -0.052359877559829876, -0.052359877559829876);
    hooked.step(0.8, -30.0);
    expectStep(hooked, -0.03490658503988657, -0.03490658503988657);
    hooked.step(0.9, -30.0);
    expectStep(hooked, -0.017453292519943278, -0.017453292519943278);
    hooked.step(1.9, -30.0);
    expectStep(hooked, 0.15707963267948966, 0.15707963267948966);
    hooked.step(11.9, -30.0);
    expectStep(hooked, 0.2617993877991494, 0.2617993877991494);
    hooked.step(12.0, 300.0);
    expectStep(hooked, 0.2617993877991494, 0.2617993877991494);
    hooked.step(30.0, 300.0);
    expectStep(hooked, -0.2617993877991494, -0.2617993877991494);
    hooked.step(30.5, kNaN);
    expectStep(hooked, kNaN, kNaN);
    hooked.step(31.0, 0.0);
    expectStep(hooked, kNaN, kNaN);

    hooked.listener->endSimulation(hooked.s.status, nullptr);
    EXPECT_EQ(hooked.cant(), 0.1);
}

// RollHooks: SEQUENCE custom. Every setting changed: the controller acts from time 0 (with a
// first time difference of 0), follows the wanted angle at once (a turn rate of 100 rad/s),
// and its own limit of 0.5 rad is wider than the fin set's (15 degrees), so the column holds
// the controller's angle and the fin set what it accepts of it.
TEST(RollControlListener, ControlsAsInJavaWithEverySettingChanged)
{
    RollControl roll;
    roll.setControlFinName("3 Fin Set");
    roll.setStartTime(0.0);
    roll.setSetPoint(1.5);
    roll.setFinRate(100.0);
    roll.setMaxFinAngle(0.5);
    roll.setKP(0.02);
    roll.setKI(0.5);
    Hooked hooked(-0.05, roll);
    hooked.listener->startSimulation(hooked.s.status);
    EXPECT_EQ(hooked.cant(), -0.05);

    hooked.step(0.0, 0.0);
    expectStep(hooked, 0.0, 0.0);
    hooked.step(0.01, 0.5);
    expectStep(hooked, 0.025, 0.025);
    hooked.step(0.02, 3.0);
    expectStep(hooked, -0.0325, -0.0325);
    hooked.step(0.03, 3.0);
    expectStep(hooked, -0.039999999999999994, -0.039999999999999994);
    hooked.step(0.5, -4.0);
    expectStep(hooked, 0.2617993877991494, 0.5);
    hooked.step(0.6, 1.5);
    expectStep(hooked, 0.2617993877991494, 0.5);
    hooked.step(0.7, 1.5);
    expectStep(hooked, 0.2617993877991494, 0.5);

    hooked.listener->endSimulation(hooked.s.status, nullptr);
    EXPECT_EQ(hooked.cant(), -0.05);
}

// RollHooks: SEQUENCE typed. Settings a file gives as other kinds of numbers count, and one
// that is no number is the default.
TEST(RollControlListener, ControlsAsInJavaWithSettingsOfOtherTypes)
{
    Config config;
    config.put("controlFinName", "3 Fin Set");
    config.put("startTime", 2);
    config.put("setPoint", std::int64_t{3});
    config.put("finRate", 1.5F);
    config.put("maxFinAngle", "wide");
    config.put("KP", big("0.25"));
    config.put("KI", true);
    RollControl roll;
    roll.setConfig(config);
    Hooked hooked(0.0, roll);
    hooked.listener->startSimulation(hooked.s.status);

    hooked.step(1.0, 1.0);
    EXPECT_EQ(hooked.cant(), 0.0);
    EXPECT_EQ(hooked.column(), std::nullopt);
    hooked.step(2.0, 1.0);
    expectStep(hooked, 0.2617993877991494, 0.2617993877991494);
    hooked.step(3.0, 1.0);
    expectStep(hooked, 0.2617993877991494, 0.2617993877991494);

    hooked.listener->endSimulation(hooked.s.status, nullptr);
    EXPECT_EQ(hooked.cant(), 0.0);
}

// The listener has the settings of the extension as they were when initialize() made it (a
// stated deviation: Java's reads them at every step).
TEST(RollControlListener, KeepsTheSettingsItWasMadeWith)
{
    RollControl roll;
    roll.setControlFinName("3 Fin Set");
    Hooked hooked(0.1, roll);
    roll.setControlFinName("No such fins");
    roll.setStartTime(100.0);

    hooked.listener->startSimulation(hooked.s.status);
    hooked.step(0.4, 5.0);
    hooked.step(0.6, 2.0);
    expectStep(hooked, -0.034906585039886584, -0.034906585039886584);
}

// A clone of the listener goes on where the original is (the engine clones the listeners with
// the status): the same fin set, the state of the controller as it was, and from then on a
// state of its own, as the primitive fields of Java's listener.
TEST(RollControlListener, ACloneGoesOnFromTheStateOfTheOriginal)
{
    RollControl roll;
    roll.setControlFinName("3 Fin Set");
    Hooked hooked(0.1, roll);
    hooked.listener->startSimulation(hooked.s.status);
    hooked.step(0.4, 5.0);
    hooked.step(0.6, 2.0);
    expectStep(hooked, -0.034906585039886584, -0.034906585039886584);

    // What SimulationConditions::clone() does with its listeners.
    const SimulationConditions                 cloned = hooked.s.conditions->clone();
    const std::shared_ptr<SimulationListener>& clone  = cloned.getSimulationListenerList().back();
    ASSERT_NE(clone, nullptr);
    ASSERT_NE(clone, hooked.listener);
    EXPECT_FALSE(clone->isSystemListener());

    // The clone takes the next two steps of the sequence...
    const std::shared_ptr<SimulationListener> original = std::exchange(hooked.listener, clone);
    hooked.step(0.7, -1.0);
    expectStep(hooked, -0.052359877559829876, -0.052359877559829876);
    hooked.step(0.8, -30.0);
    expectStep(hooked, -0.03490658503988657, -0.03490658503988657);
    // ... which the original has not seen: its next step is the third one of the sequence
    // again, from its own state, on the same fin set.
    hooked.listener = original;
    hooked.step(0.7, -1.0);
    expectStep(hooked, -0.052359877559829876, -0.052359877559829876);

    // Either puts the cant of the start back.
    clone->endSimulation(hooked.s.status, nullptr);
    EXPECT_EQ(hooked.cant(), 0.1);
}

// ------------------------------------------------------ the search of the fin set (FixHooks)

/// The listener of a RollControl on a hand-made status of the two-stage Beta (TEST_FCID_1), as
/// FixHooks.java drives it: the sustainer's fins ("3 Fin Set") canted by 0.01 rad and the
/// booster's by 0.03 rad, the booster's fins renamed, and one stage active or both.
struct HookedBeta
{
    TestBeta                              beta;
    std::shared_ptr<SimulationConditions> conditions   = std::make_shared<SimulationConditions>();
    std::shared_ptr<QtRocket::FlightDataBranch> branch = QtRocket::Test::newBranch();
    SimulationStatus                            status;
    std::shared_ptr<SimulationListener>         listener;

    /// A status with the booster's fins called @p boosterFinsName and only the stage
    /// @p onlyStage active (-1: both), and the listener of a RollControl on the fin set called
    /// @p controlled.
    HookedBeta(std::string_view boosterFinsName, int onlyStage, std::string_view controlled)
      : status(configurationOf(beta, boosterFinsName, onlyStage), conditions),
        listener(listenerOf(*conditions, controlled))
    {
        status.setFlightDataBranch(branch);
    }

    /// The listener that a RollControl on the fin set called @p controlled adds to
    /// @p conditions.
    [[nodiscard]] static std::shared_ptr<SimulationListener> listenerOf(
        SimulationConditions& conditions, std::string_view controlled)
    {
        RollControl roll;
        roll.setControlFinName(controlled);
        roll.initialize(conditions);
        return conditions.getSimulationListenerList().back();
    }

    /// The configuration of the status: a clone of the Beta's, as the engine makes one.
    [[nodiscard]] static std::shared_ptr<QtRocket::FlightConfiguration> configurationOf(
        const TestBeta& beta, std::string_view boosterFinsName, int onlyStage)
    {
        beta.boosterFins->setName(boosterFinsName);
        beta.fins->setCantAngle(0.01);
        beta.boosterFins->setCantAngle(0.03);
        std::shared_ptr<QtRocket::FlightConfiguration> config =
            QtRocket::Test::statusConfiguration(*beta.rocket, QtRocket::Test::testFcid(1));
        if (onlyStage >= 0)
        {
            config->setOnlyStage(onlyStage);
        }
        return config;
    }

    /// The message of the SimulationException startSimulation() throws; "<none>" without one.
    [[nodiscard]] std::string start()
    {
        try
        {
            listener->startSimulation(status);
        }
        catch (const SimulationException& e)
        {
            return e.what();
        }
        return "<none>";
    }

    /// The probe's three steps: idle at 0.4 s, then two controlled ones.
    void threeSteps()
    {
        step(0.4, 5.0);
        step(0.6, 2.0);
        step(0.7, -1.0);
    }

    void step(double time, double rollRate)
    {
        FlightConditions flightConditions;
        flightConditions.setRollRate(rollRate);
        auto* computation = dynamic_cast<SimulationComputationListener*>(listener.get());
        ASSERT_NE(computation, nullptr);
        EXPECT_EQ(computation->postFlightConditions(status, flightConditions), std::nullopt);
        status.setSimulationTime(time);
        branch->addPoint();
        listener->postStep(status);
    }

    [[nodiscard]] double sustainerCant() const { return beta.fins->getCantAngle(); }
    [[nodiscard]] double boosterCant() const { return beta.boosterFins->getCantAngle(); }
};

// FixHooks: ROLL booster-inactive and booster-inactive-sustainer-controlled. The fin set is
// searched among the ACTIVE components of the configuration: with only the sustainer active,
// the fins of the booster are not found, though the rocket has them.
TEST(RollControlListener, LooksForTheFinSetAmongTheActiveComponents)
{
    HookedBeta boosterWanted("Booster Fins", 0, "Booster Fins");
    EXPECT_EQ(boosterWanted.start(), "A fin set with name 'Booster Fins' was not found");

    HookedBeta sustainerWanted("Booster Fins", 0, "3 Fin Set");
    ASSERT_EQ(sustainerWanted.start(), "<none>");
    sustainerWanted.threeSteps();
    EXPECT_EQ(sustainerWanted.sustainerCant(), -0.052359877559829876);
    EXPECT_EQ(sustainerWanted.boosterCant(), 0.03);

    // FixHooks: ROLL all-active. With both stages active the booster's fins are found.
    HookedBeta bothActive("Booster Fins", -1, "Booster Fins");
    ASSERT_EQ(bothActive.start(), "<none>");
    bothActive.threeSteps();
    EXPECT_EQ(bothActive.sustainerCant(), 0.01);
    EXPECT_EQ(bothActive.boosterCant(), -0.052359877559829876);
    bothActive.listener->endSimulation(bothActive.status, nullptr);
    EXPECT_EQ(bothActive.boosterCant(), 0.03);
}

// FixHooks: ROLL same-name and same-name-booster-only. Of two fin sets of the name, the first
// among the active components is the one that is turned, the sustainer's here, and the other
// keeps its cant; with the sustainer not active, the first active one is the booster's.
TEST(RollControlListener, TurnsTheFirstActiveFinSetOfTheName)
{
    HookedBeta sameName("3 Fin Set", -1, "3 Fin Set");
    ASSERT_EQ(sameName.start(), "<none>");
    sameName.step(0.4, 5.0);
    EXPECT_EQ(sameName.sustainerCant(), 0.01);
    EXPECT_EQ(sameName.boosterCant(), 0.03);
    sameName.step(0.6, 2.0);
    EXPECT_EQ(sameName.sustainerCant(), -0.034906585039886584);
    EXPECT_EQ(sameName.boosterCant(), 0.03);
    sameName.step(0.7, -1.0);
    EXPECT_EQ(sameName.sustainerCant(), -0.052359877559829876);
    EXPECT_EQ(sameName.boosterCant(), 0.03);
    sameName.listener->endSimulation(sameName.status, nullptr);
    EXPECT_EQ(sameName.sustainerCant(), 0.01);
    EXPECT_EQ(sameName.boosterCant(), 0.03);

    HookedBeta boosterOnly("3 Fin Set", 1, "3 Fin Set");
    ASSERT_EQ(boosterOnly.start(), "<none>");
    boosterOnly.threeSteps();
    EXPECT_EQ(boosterOnly.sustainerCant(), 0.01);
    EXPECT_EQ(boosterOnly.boosterCant(), -0.052359877559829876);
    boosterOnly.listener->endSimulation(boosterOnly.status, nullptr);
    EXPECT_EQ(boosterOnly.boosterCant(), 0.03);
}

}  // namespace
