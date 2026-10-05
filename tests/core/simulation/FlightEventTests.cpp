#include "QtRocket/simulation/FlightEvent.h"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::FlightEvent;
using QtRocket::MotorClusterState;
using QtRocket::Result;
using QtRocket::SimulationAbort;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::Test::bugText;
using QtRocket::Test::EventTestRocket;
using Type = QtRocket::FlightEvent::Type;
using Data = QtRocket::FlightEvent::Data;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// 123e4567-e89b-12d3-a456-426614174000, the id the Java probe gives an event.
constexpr Uuid kGivenId{0x123e4567e89b12d3ULL, 0xa456426614174000ULL};
/// 0f8fad5b-d9cb-469f-a165-70867728950e, the id of a component that is not at hand.
constexpr Uuid kSourceId{0x0f8fad5bd9cb469fULL, 0xa16570867728950eULL};

/// A copy of @p event, as a container or another holder of the event makes one.
[[nodiscard]] FlightEvent copyOf(const FlightEvent& event)
{
    return event;
}

static_assert(std::is_copy_constructible_v<FlightEvent>);
static_assert(std::is_nothrow_move_constructible_v<FlightEvent>);
static_assert(std::is_nothrow_move_assignable_v<FlightEvent>);

/// A warning as the data of an event.
[[nodiscard]] std::shared_ptr<const Warning> warning(const std::string& text)
{
    return std::make_shared<const Warning::Other>(Warning::fromString(text));
}

// ============================================================================ Type

/// A type as Java has it: FlightEvent.Type.values(), name() and toString() (MiscProbe.java).
struct ExpectedType
{
    Type             type;
    std::string_view name;
    std::string_view displayName;
};

/// The sixteen types in OpenRocket's order.
[[nodiscard]] const std::array<ExpectedType, 16>& expectedTypes()
{
    static const std::array<ExpectedType, 16> kExpected{{
        {.type = Type::LAUNCH, .name = "LAUNCH", .displayName = "Launch"},
        {.type = Type::IGNITION, .name = "IGNITION", .displayName = "Motor ignition"},
        {.type = Type::LIFTOFF, .name = "LIFTOFF", .displayName = "Lift-off"},
        {.type = Type::LAUNCHROD, .name = "LAUNCHROD", .displayName = "Launch rod clearance"},
        {.type = Type::BURNOUT, .name = "BURNOUT", .displayName = "Motor burnout"},
        {.type        = Type::EJECTION_CHARGE,
         .name        = "EJECTION_CHARGE",
         .displayName = "Ejection charge"},
        {.type        = Type::STAGE_SEPARATION,
         .name        = "STAGE_SEPARATION",
         .displayName = "Stage separation"},
        {.type = Type::APOGEE, .name = "APOGEE", .displayName = "Apogee"},
        {.type        = Type::RECOVERY_DEVICE_DEPLOYMENT,
         .name        = "RECOVERY_DEVICE_DEPLOYMENT",
         .displayName = "Recovery device deployment"},
        {.type = Type::GROUND_HIT, .name = "GROUND_HIT", .displayName = "Ground hit"},
        {.type = Type::SIMULATION_END, .name = "SIMULATION_END", .displayName = "Simulation end"},
        {.type = Type::ALTITUDE, .name = "ALTITUDE", .displayName = "Altitude change"},
        {.type = Type::TUMBLE, .name = "TUMBLE", .displayName = "Tumbling"},
        {.type = Type::SIM_WARN, .name = "SIM_WARN", .displayName = "Warning"},
        {.type = Type::SIM_ABORT, .name = "SIM_ABORT", .displayName = "Simulation abort"},
        {.type = Type::EXCEPTION, .name = "EXCEPTION", .displayName = "Exception"},
    }};
    return kExpected;
}

/// Expects the type at position @p i of kAllTypes to be the one Java has there.
void expectTypeAt(std::size_t i)
{
    const ExpectedType& expected = expectedTypes().at(i);
    SCOPED_TRACE(expected.name);
    const Type type = FlightEvent::kAllTypes.at(i);
    EXPECT_EQ(type, expected.type);
    EXPECT_EQ(ordinal(type), static_cast<int>(i));
    EXPECT_EQ(name(type), expected.name);
    EXPECT_EQ(displayName(type), expected.displayName);
    EXPECT_EQ(displayKey(type), "FlightEvent.Type." + std::string{expected.name});
    EXPECT_EQ(QtRocket::flightEventTypeFromName(expected.name), type);
}

TEST(FlightEventType, HasOpenRocketsOrderNamesAndDisplayNames)
{
    ASSERT_EQ(FlightEvent::kAllTypes.size(), expectedTypes().size());
    for (std::size_t i = 0; i < expectedTypes().size(); i++)
    {
        expectTypeAt(i);
    }
}

TEST(FlightEventType, FromNameIsExact)
{
    EXPECT_EQ(QtRocket::flightEventTypeFromName("SIM_WARN"), Type::SIM_WARN);
    EXPECT_EQ(QtRocket::flightEventTypeFromName("sim_warn"), std::nullopt);
    EXPECT_EQ(QtRocket::flightEventTypeFromName(" LAUNCH"), std::nullopt);
    EXPECT_EQ(QtRocket::flightEventTypeFromName("Launch"), std::nullopt);
    EXPECT_EQ(QtRocket::flightEventTypeFromName(""), std::nullopt);
}

TEST(FlightEventType, OrkNamesAreLowerCaseWithoutUnderscores)
{
    EXPECT_EQ(orkName(Type::LAUNCH), "launch");
    EXPECT_EQ(orkName(Type::EJECTION_CHARGE), "ejectioncharge");
    EXPECT_EQ(orkName(Type::RECOVERY_DEVICE_DEPLOYMENT), "recoverydevicedeployment");
    EXPECT_EQ(orkName(Type::SIM_WARN), "simwarn");
    EXPECT_EQ(orkName(Type::EXCEPTION), "exception");
}

TEST(FlightEventType, EveryOrkNameIsReadBack)
{
    for (const Type type : FlightEvent::kAllTypes)
    {
        EXPECT_EQ(QtRocket::flightEventTypeFromOrkName(orkName(type)), type) << name(type);
    }
}

TEST(FlightEventType, FromOrkNameMatchesAsTheLoaderDoes)
{
    // DocumentConfig.findEnum() trims and compares exactly.
    EXPECT_EQ(QtRocket::flightEventTypeFromOrkName("  groundhit "), Type::GROUND_HIT);
    EXPECT_EQ(QtRocket::flightEventTypeFromOrkName("GROUNDHIT"), std::nullopt);
    EXPECT_EQ(QtRocket::flightEventTypeFromOrkName("ground_hit"), std::nullopt);
    EXPECT_EQ(QtRocket::flightEventTypeFromOrkName(""), std::nullopt);
}

// ============================================================================ construction

TEST(FlightEvent, TheShortConstructorsLeaveTheRestEmpty)
{
    const EventTestRocket r;

    const FlightEvent bare{Type::LIFTOFF, 0.1275};
    EXPECT_EQ(bare.getType(), Type::LIFTOFF);
    EXPECT_EQ(bare.getTime(), 0.1275);
    EXPECT_EQ(bare.getSource(), nullptr);
    EXPECT_EQ(bare.getSourceId(), std::nullopt);
    EXPECT_FALSE(bare.hasSource());
    EXPECT_FALSE(bare.hasData());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(bare.getData()));
    EXPECT_FALSE(bare.getId().isNil());
    EXPECT_EQ(bare.getId().version(), 4) << "a random UUID";

    const FlightEvent sourced{Type::LAUNCH, 0.0, &r.rocket};
    EXPECT_EQ(sourced.getSource(), &r.rocket);
    EXPECT_EQ(sourced.getSourceId(), r.rocket.getId());
    EXPECT_TRUE(sourced.hasSource());
    EXPECT_FALSE(sourced.hasData());

    const FlightEvent noSource{Type::LAUNCHROD, 0.13, nullptr};
    EXPECT_EQ(noSource.getSource(), nullptr);
    EXPECT_FALSE(noSource.hasSource());
}

TEST(FlightEvent, EveryEventGetsItsOwnIdUnlessOneIsGiven)
{
    const FlightEvent first{Type::APOGEE, 1.0};
    const FlightEvent second{Type::APOGEE, 1.0};
    EXPECT_NE(first.getId(), second.getId());
    EXPECT_FALSE(first.sameEvent(second));

    // Java: new FlightEvent(type, time, source, data, id) (MiscProbe.java, "given id").
    const Uuid        id = kGivenId;
    const FlightEvent given{Type::APOGEE, 1.0, nullptr, Data{}, id};
    EXPECT_EQ(given.getId(), id);
    EXPECT_EQ(given.getId().toString(), "123e4567-e89b-12d3-a456-426614174000");

    const FlightEvent drawn{Type::APOGEE, 1.0, nullptr, Data{}, std::nullopt};
    EXPECT_NE(drawn.getId(), id);
    EXPECT_FALSE(drawn.getId().isNil());
}

TEST(FlightEvent, ACopyIsTheSameEvent)
{
    const EventTestRocket r;
    const FlightEvent     original{Type::IGNITION, 0.01, r.sustainerMount, r.state};
    const FlightEvent     copy = copyOf(original);

    EXPECT_TRUE(copy.sameEvent(original));
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), Type::IGNITION);
    EXPECT_EQ(copy.getTime(), 0.01);
    EXPECT_EQ(copy.getSource(), r.sustainerMount);
    EXPECT_EQ(copy.getSourceId(), r.sustainerMount->getId());
    EXPECT_EQ(copy.getMotorState(), r.state) << "the state is shared, not copied";
}

TEST(FlightEvent, TheReSourcingConstructorMakesANewEventOfTheSameTypeAndTime)
{
    // Java: new FlightEvent(sourceEvent, source, data) (MiscProbe.java, "resourced").
    const EventTestRocket r;
    const FlightEvent     base{Type::BURNOUT, 7.25, r.sustainerBody, r.state};
    const FlightEvent     resourced{base, r.boosterBody, Data{}};

    EXPECT_EQ(resourced.getType(), Type::BURNOUT);
    EXPECT_EQ(resourced.getTime(), 7.25);
    EXPECT_EQ(resourced.getSource(), r.boosterBody);
    EXPECT_FALSE(resourced.hasData());
    EXPECT_NE(resourced.getId(), base.getId());
    EXPECT_EQ(resourced.toString(),
              "FlightEvent[type=BURNOUT,time=7.25,source=Booster Body,data=null]");

    EXPECT_THROW((FlightEvent{base, r.chute, Data{}}), BugError) << "it is validated";
}

TEST(FlightEvent, TheSourceIdIsCopiedWhenTheEventIsMade)
{
    Uuid                       id;
    std::optional<FlightEvent> event;
    {
        const BodyTube detached(0.1, 0.01);
        id = detached.getId();
        event.emplace(Type::LAUNCH, 0.0, &detached);
    }
    // The component is gone; the id is still there (the pointer must not be used any more).
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->getSourceId(), id);
}

// ============================================================================ the data

TEST(FlightEventData, AMotorStateIsShared)
{
    const EventTestRocket r;
    const FlightEvent     event{Type::IGNITION, 0.0, r.sustainerMount, r.state};

    EXPECT_TRUE(event.hasData());
    EXPECT_EQ(event.getMotorState(), r.state);
    EXPECT_EQ(event.getWarning(), nullptr);
    EXPECT_EQ(event.getAbort(), nullptr);
    EXPECT_EQ(event.getAltitudeChange(), std::nullopt);
    EXPECT_EQ(event.getMessage(), nullptr);

    // Igniting through the event ignites the simulation's state.
    event.getMotorState()->ignite(1.5);
    EXPECT_EQ(r.state->getIgnitionTime(), 1.5);

    // The event keeps the state alive.
    std::weak_ptr<MotorClusterState> weak;
    std::optional<FlightEvent>       keeper;
    {
        auto own = std::make_shared<MotorClusterState>(r.sustainerMount->getMotorConfig(r.fcid));
        weak     = own;
        keeper.emplace(Type::BURNOUT, 2.0, r.sustainerMount, own);
    }
    EXPECT_FALSE(weak.expired());
    keeper.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(FlightEventData, AWarningKeepsItsTypeAndIdThroughCopies)
{
    const Warning::LargeAOA aoa(0.5);
    const FlightEvent       event{Type::SIM_WARN, 2.5, nullptr, FlightEvent::warningData(aoa)};

    const std::shared_ptr<const Warning> carried = event.getWarning();
    ASSERT_NE(carried, nullptr);
    EXPECT_NE(carried.get(), &aoa) << "the event holds a copy";
    EXPECT_EQ(carried->id(), aoa.id());
    EXPECT_EQ(carried->typeName(), "LargeAOA");
    const auto* typed = dynamic_cast<const Warning::LargeAOA*>(carried.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->aoa(), 0.5);

    const FlightEvent copy = copyOf(event);
    EXPECT_EQ(copy.getWarning(), carried) << "the copies of an event share the warning";
    EXPECT_EQ(event.getMotorState(), nullptr);
    EXPECT_EQ(event.getAbort(), nullptr);
}

TEST(FlightEventData, AnAbortIsAValue)
{
    const FlightEvent event{Type::SIM_ABORT, 0.0, nullptr,
                            SimulationAbort{SimulationAbort::Cause::NO_LIFTOFF}};
    ASSERT_NE(event.getAbort(), nullptr);
    EXPECT_EQ(event.getAbort()->cause(), SimulationAbort::Cause::NO_LIFTOFF);
    EXPECT_EQ(event.getWarning(), nullptr);

    const FlightEvent copy = copyOf(event);
    ASSERT_NE(copy.getAbort(), nullptr);
    EXPECT_NE(copy.getAbort(), event.getAbort());
    EXPECT_EQ(copy.getAbort()->cause(), SimulationAbort::Cause::NO_LIFTOFF);
    EXPECT_EQ(copy.getAbort()->id(), event.getAbort()->id());
}

TEST(FlightEventData, AltitudesAndMessages)
{
    const EventTestRocket r;
    const FlightEvent     altitude{Type::ALTITUDE, 1.0, &r.rocket,
                                   FlightEvent::AltitudeChange{.previous = 1.5, .current = 2.25}};
    EXPECT_EQ(altitude.getAltitudeChange(),
              (FlightEvent::AltitudeChange{.previous = 1.5, .current = 2.25}));
    EXPECT_EQ(altitude.getMessage(), nullptr);

    const FlightEvent exception{Type::EXCEPTION, 3.0, &r.rocket, std::string{"boom"}};
    ASSERT_NE(exception.getMessage(), nullptr);
    EXPECT_EQ(*exception.getMessage(), "boom");
    EXPECT_EQ(exception.getAltitudeChange(), std::nullopt);
}

TEST(FlightEventData, ANullPointerIsNoData)
{
    const FlightEvent state{Type::IGNITION, 0.0, nullptr, std::shared_ptr<MotorClusterState>{}};
    EXPECT_FALSE(state.hasData());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(state.getData()));

    const FlightEvent warn{Type::LAUNCH, 0.0, nullptr, std::shared_ptr<const Warning>{}};
    EXPECT_FALSE(warn.hasData());

    // So a SIM_WARN event with a null warning has none.
    EXPECT_THROW((FlightEvent{Type::SIM_WARN, 0.0, nullptr, std::shared_ptr<const Warning>{}}),
                 BugError);
}

TEST(FlightEventData, AltitudeChangesCompareAsJavasPairs)
{
    using Change = FlightEvent::AltitudeChange;
    EXPECT_EQ((Change{.previous = 1.0, .current = 2.0}), (Change{.previous = 1.0, .current = 2.0}));
    EXPECT_NE((Change{.previous = 1.0, .current = 2.0}), (Change{.previous = 2.0, .current = 1.0}));
    // Java (FixProbe.java, "pair"): each of the two values counts.
    EXPECT_NE((Change{.previous = 1.0, .current = 2.0}), (Change{.previous = 1.0, .current = 3.0}));
    EXPECT_NE((Change{.previous = 1.0, .current = 2.0}), (Change{.previous = 3.0, .current = 2.0}));
    EXPECT_EQ((Change{.previous = kNaN, .current = 2.0}),
              (Change{.previous = kNaN, .current = 2.0}))
        << "Double.equals(): NaN equals NaN";
    EXPECT_NE((Change{.previous = 0.0, .current = 2.0}), (Change{.previous = -0.0, .current = 2.0}))
        << "Double.equals(): 0.0 differs from -0.0";
}

// ============================================================================ toString()

TEST(FlightEvent, ToStringIsJavas)
{
    // Java: MiscProbe.java, "toString".
    const EventTestRocket r;
    EXPECT_EQ((FlightEvent{Type::LAUNCH, 0.0, &r.rocket}).toString(),
              "FlightEvent[type=LAUNCH,time=0.0,source=Probe Rocket,data=null]");
    EXPECT_EQ((FlightEvent{Type::IGNITION, 0.01, r.sustainerMount, r.state}).toString(),
              "FlightEvent[type=IGNITION,time=0.01,source=Sustainer Mount,data=A8]");
    EXPECT_EQ((FlightEvent{Type::LIFTOFF, 0.1275}).toString(),
              "FlightEvent[type=LIFTOFF,time=0.1275,source=null,data=null]");
    EXPECT_EQ((FlightEvent{Type::ALTITUDE, 1200, &r.rocket,
                           FlightEvent::AltitudeChange{.previous = 1.5, .current = 2.25}})
                  .toString(),
              "FlightEvent[type=ALTITUDE,time=1200.0,source=Probe Rocket,data=[1.5;2.25]]");
    EXPECT_EQ((FlightEvent{Type::ALTITUDE, 1.0E-4, r.boosterBody,
                           FlightEvent::AltitudeChange{.previous = -0.0, .current = 1.0E10}})
                  .toString(),
              "FlightEvent[type=ALTITUDE,time=1.0E-4,source=Booster Body,data=[-0.0;1.0E10]]");
    EXPECT_EQ((FlightEvent{Type::SIM_WARN, 2.5, nullptr, warning("custom text")}).toString(),
              "FlightEvent[type=SIM_WARN,time=2.5,source=null,data=custom text]");
    EXPECT_EQ((FlightEvent{Type::SIM_ABORT, 0.0, nullptr,
                           SimulationAbort{SimulationAbort::Cause::NO_MOTORS_DEFINED}})
                  .toString(),
              "FlightEvent[type=SIM_ABORT,time=0.0,source=null,"
              "data=No motors defined in the simulation]");
    EXPECT_EQ((FlightEvent{Type::EXCEPTION, 3.0, &r.rocket, std::string{"boom"}}).toString(),
              "FlightEvent[type=EXCEPTION,time=3.0,source=Probe Rocket,data=boom]");
    EXPECT_EQ((FlightEvent{Type::EJECTION_CHARGE, 4.5, r.strapOns, r.state}).toString(),
              "FlightEvent[type=EJECTION_CHARGE,time=4.5,source=Strap-ons,data=A8]");
    EXPECT_EQ((FlightEvent{Type::RECOVERY_DEVICE_DEPLOYMENT, 4.501, r.chute}).toString(),
              "FlightEvent[type=RECOVERY_DEVICE_DEPLOYMENT,time=4.501,source=Chute,data=null]");
}

TEST(FlightEvent, ToStringPrintsTheSourcesOfAWarning)
{
    const EventTestRocket r;
    Warning::Other        sourced = Warning::fromString("sourced text");
    sourced.setSources(
        {QtRocket::MessageSource::of(*r.sustainerBody), QtRocket::MessageSource::of(*r.chute)});
    const FlightEvent event{Type::SIM_WARN, 2.5, nullptr, FlightEvent::warningData(sourced)};
    EXPECT_EQ(event.toString(),
              "FlightEvent[type=SIM_WARN,time=2.5,source=null,"
              "data=sourced text:  \"Sustainer Body\", \"Chute\"]");
}

// ============================================================================ validate()

TEST(FlightEventValidation, ATimeMustNotBeNaN)
{
    EXPECT_EQ(bugText([] { static_cast<void>(FlightEvent{Type::LAUNCH, kNaN}); }),
              "LAUNCH event has a NaN time!");
    EXPECT_EQ(bugText([] { static_cast<void>(FlightEvent{Type::SIM_WARN, kNaN}); }),
              "SIM_WARN event has a NaN time!")
        << "the time is checked first";
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::LAUNCH, kInf}));
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::LAUNCH, -1.0}));
}

TEST(FlightEventValidation, IgnitionAndBurnoutComeFromAMotorMount)
{
    const EventTestRocket r;
    EXPECT_EQ(bugText([&r] { static_cast<void>(FlightEvent{Type::BURNOUT, 1.0, r.chute}); }),
              "BURNOUT events should have MotorMount type data payloads, instead of Parachute");
    EXPECT_EQ(bugText([&r] { static_cast<void>(FlightEvent{Type::BURNOUT, 1.0, r.sustainer}); }),
              "BURNOUT events should have MotorMount type data payloads, instead of AxialStage");
    EXPECT_EQ(bugText([&r] { static_cast<void>(FlightEvent{Type::IGNITION, 1.0, r.booster}); }),
              "IGNITION events should have MotorMount type data payloads, instead of AxialStage");
    EXPECT_EQ(
        bugText([&r] {
            static_cast<void>(FlightEvent{Type::BURNOUT, 1.0, &r.rocket, std::string{"text"}});
        }),
        "BURNOUT events should have MotorMount type data payloads, instead of Rocket")
        << "the source is checked before the data";

    // A body tube and an inner tube are mounts, whether they hold a motor or not.
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::IGNITION, 1.0, r.boosterBody}));
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::BURNOUT, 1.0, r.sustainerMount, r.state}));
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::IGNITION, 1.0, nullptr, Data{}}));
}

TEST(FlightEventValidation, TheMotorEventsCarryAMotorStateOrNothing)
{
    const EventTestRocket r;
    EXPECT_EQ(bugText([&r] {
                  static_cast<void>(
                      FlightEvent{Type::BURNOUT, 1.0, r.sustainerBody, std::string{"text"}});
              }),
              "BURNOUT events should have MotorClusterState type data payloads");
    EXPECT_EQ(bugText([&r] {
                  static_cast<void>(
                      FlightEvent{Type::IGNITION, 1.0, r.sustainerMount, std::string{"text"}});
              }),
              "IGNITIONevents should have MotorClusterState type data payloads")
        << "Java's text has no space there";
    EXPECT_EQ(bugText([&r] {
                  static_cast<void>(
                      FlightEvent{Type::EJECTION_CHARGE, 1.0, r.sustainer,
                                  FlightEvent::AltitudeChange{.previous = 1.0, .current = 2.0}});
              }),
              "EJECTION_CHARGE events should have MotorClusterState type data payloads");
}

TEST(FlightEventValidation, AnEjectionChargeComesFromAStage)
{
    const EventTestRocket r;
    EXPECT_EQ(
        bugText(
            [&r] { static_cast<void>(FlightEvent{Type::EJECTION_CHARGE, 1.0, r.sustainerBody}); }),
        "EJECTION_CHARGE events should have AxialStage type data payloads, instead of BodyTube");
    EXPECT_EQ(
        bugText([&r] { static_cast<void>(FlightEvent{Type::EJECTION_CHARGE, 1.0, &r.rocket}); }),
        "EJECTION_CHARGE events should have AxialStage type data payloads, instead of Rocket");
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::EJECTION_CHARGE, 1.0, r.sustainer}));
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::EJECTION_CHARGE, 1.0, r.strapOns, r.state}))
        << "a parallel stage is a stage";
}

TEST(FlightEventValidation, AWarningEventHasAWarningAndNoSource)
{
    const EventTestRocket r;
    EXPECT_EQ(bugText([&r] {
                  static_cast<void>(
                      FlightEvent{Type::SIM_WARN, 1.0, r.sustainer, warning("custom text")});
              }),
              "SIM_WARN event requires null source component; was Sustainer");
    EXPECT_EQ(bugText([] { static_cast<void>(FlightEvent{Type::SIM_WARN, 1.0}); }),
              "SIM_WARN events require Warning objects");
    EXPECT_EQ(bugText([] {
                  static_cast<void>(FlightEvent{Type::SIM_WARN, 1.0, nullptr, std::string{"text"}});
              }),
              "SIM_WARN events require Warning objects");
    EXPECT_EQ(bugText([] {
                  static_cast<void>(FlightEvent{Type::SIM_WARN, 1.0, nullptr,
                                                SimulationAbort{SimulationAbort::Cause::NO_CP}});
              }),
              "SIM_WARN events require Warning objects");
    EXPECT_NO_THROW(
        static_cast<void>(FlightEvent{Type::SIM_WARN, 1.0, nullptr, warning("custom text")}));
}

TEST(FlightEventValidation, AnAbortEventHasAnAbort)
{
    const EventTestRocket r;
    EXPECT_EQ(bugText([] { static_cast<void>(FlightEvent{Type::SIM_ABORT, 1.0}); }),
              "SIM_ABORT events require SimulationAbort objects");
    EXPECT_EQ(
        bugText([] {
            static_cast<void>(FlightEvent{Type::SIM_ABORT, 1.0, nullptr, warning("custom text")});
        }),
        "SIM_ABORT events require SimulationAbort objects");
    EXPECT_NO_THROW(static_cast<void>(
        FlightEvent{Type::SIM_ABORT, 1.0, r.chute, SimulationAbort{SimulationAbort::Cause::NO_CP}}))
        << "any source will do";
}

TEST(FlightEventValidation, TheOtherTypesTakeAnySourceAndAnyData)
{
    const EventTestRocket r;
    EXPECT_NO_THROW(
        static_cast<void>(FlightEvent{Type::LAUNCH, 1.0, r.chute, std::string{"text"}}));
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::ALTITUDE, 1.0, r.chute, r.state}));
    for (const Type type : {Type::LAUNCH, Type::LIFTOFF, Type::LAUNCHROD, Type::STAGE_SEPARATION,
                            Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT, Type::GROUND_HIT,
                            Type::SIMULATION_END, Type::ALTITUDE, Type::TUMBLE, Type::EXCEPTION})
    {
        SCOPED_TRACE(name(type));
        EXPECT_NO_THROW(static_cast<void>(FlightEvent{type, 1.0, r.chute, warning("any")}));
        EXPECT_NO_THROW(static_cast<void>(FlightEvent{type, 1.0, &r.rocket}));
        EXPECT_NO_THROW(static_cast<void>(FlightEvent{type, 1.0}));
    }
}

TEST(FlightEventValidation, ValidateCanBeCalledAgain)
{
    const FlightEvent event{Type::LAUNCH, 1.0};
    EXPECT_NO_THROW(event.validate());
}

TEST(FlightEventValidation, CreateReportsTheSameFailureAsAResult)
{
    const EventTestRocket r;

    const Result<FlightEvent> good =
        FlightEvent::create(Type::IGNITION, 0.5, r.sustainerMount, r.state);
    ASSERT_TRUE(good.has_value());
    EXPECT_EQ(good->getType(), Type::IGNITION);
    EXPECT_EQ(good->getTime(), 0.5);
    EXPECT_EQ(good->getSource(), r.sustainerMount);
    EXPECT_TRUE(good->hasSource());
    EXPECT_EQ(good->getSourceId(), r.sustainerMount->getId())
        << "what the saver writes as the source of the event";
    EXPECT_EQ(good->getMotorState(), r.state);

    const Result<FlightEvent> bare = FlightEvent::create(Type::LIFTOFF, 0.5, nullptr);
    ASSERT_TRUE(bare.has_value());
    EXPECT_FALSE(bare->hasData());
    EXPECT_FALSE(bare->hasSource());

    const Uuid                id     = kGivenId;
    const Result<FlightEvent> withId = FlightEvent::create(Type::APOGEE, 0.5, nullptr, Data{}, id);
    ASSERT_TRUE(withId.has_value());
    EXPECT_EQ(withId->getId(), id);
    EXPECT_EQ(withId->getSourceId(), std::nullopt);

    const Result<FlightEvent> sourcedWithId =
        FlightEvent::create(Type::BURNOUT, 0.5, r.boosterBody, Data{}, id);
    ASSERT_TRUE(sourcedWithId.has_value());
    EXPECT_EQ(sourcedWithId->getId(), id);
    EXPECT_EQ(sourcedWithId->getSourceId(), r.boosterBody->getId());

    // The .ork loader's case: a SIM_WARN event whose warning was not found.
    const Result<FlightEvent> noWarning = FlightEvent::create(Type::SIM_WARN, 0.5, nullptr);
    ASSERT_FALSE(noWarning.has_value());
    EXPECT_EQ(noWarning.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(noWarning.error().message, "SIM_WARN events require Warning objects");

    const Result<FlightEvent> nan = FlightEvent::create(Type::LAUNCH, kNaN, nullptr);
    ASSERT_FALSE(nan.has_value());
    EXPECT_EQ(nan.error().message, "LAUNCH event has a NaN time!");

    const Result<FlightEvent> wrongSource = FlightEvent::create(Type::BURNOUT, 0.5, r.chute);
    ASSERT_FALSE(wrongSource.has_value());
    EXPECT_EQ(wrongSource.error().message,
              "BURNOUT events should have MotorMount type data payloads, instead of Parachute");
}

// ==================================================================== a source known by id

TEST(FlightEventSourceId, AnEventCanKnowItsSourceByIdOnly)
{
    const Uuid        sourceId = kSourceId;
    const Uuid        id       = kGivenId;
    const FlightEvent event{Type::BURNOUT, 2.0, sourceId, Data{}, id};

    EXPECT_EQ(event.getSource(), nullptr);
    EXPECT_EQ(event.getSourceId(), sourceId);
    EXPECT_TRUE(event.hasSource());
    EXPECT_EQ(event.getId(), id);
    EXPECT_EQ(event.toString(),
              "FlightEvent[type=BURNOUT,time=2.0,"
              "source=0f8fad5b-d9cb-469f-a165-70867728950e,data=null]");

    const FlightEvent drawn{Type::BURNOUT, 2.0, sourceId, Data{}};
    EXPECT_NE(drawn.getId(), id);
    EXPECT_FALSE(drawn.getId().isNil());
}

TEST(FlightEventSourceId, ItsClassCannotBeCheckedButAWarningEventStillRefusesIt)
{
    const Uuid sourceId = kSourceId;
    // Whatever component the id names: an EJECTION_CHARGE from it is taken.
    EXPECT_NO_THROW(static_cast<void>(FlightEvent{Type::EJECTION_CHARGE, 2.0, sourceId, Data{}}));
    EXPECT_EQ(
        bugText([&sourceId] {
            static_cast<void>(FlightEvent{Type::SIM_WARN, 2.0, sourceId, warning("custom text")});
        }),
        "SIM_WARN event requires null source component; was "
        "0f8fad5b-d9cb-469f-a165-70867728950e");
    EXPECT_EQ(bugText([&sourceId] {
                  static_cast<void>(FlightEvent{Type::LAUNCH, kNaN, sourceId, Data{}});
              }),
              "LAUNCH event has a NaN time!");

    const Result<FlightEvent> created = FlightEvent::create(Type::STAGE_SEPARATION, 2.0, sourceId);
    ASSERT_TRUE(created.has_value());
    EXPECT_EQ(created->getSourceId(), sourceId);
    EXPECT_NE(created->getId(), kGivenId) << "an id of its own";

    // The .ork loader's case: the source and the id of the event are those of the file.
    const Result<FlightEvent> loaded =
        FlightEvent::create(Type::STAGE_SEPARATION, 2.0, sourceId, Data{}, kGivenId);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), kGivenId);
    EXPECT_EQ(loaded->getSource(), nullptr);
    EXPECT_EQ(loaded->getSourceId(), sourceId);
    EXPECT_TRUE(loaded->hasSource());
    EXPECT_EQ(loaded->getType(), Type::STAGE_SEPARATION);
    EXPECT_EQ(loaded->getTime(), 2.0);
    const Result<FlightEvent> refused =
        FlightEvent::create(Type::SIM_ABORT, 2.0, sourceId, std::string{"text"});
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().message, "SIM_ABORT events require SimulationAbort objects");
}

TEST(FlightEventSourceId, ADetachedEventIsCheckedAgainstItsSourceAndKeepsTheIdOnly)
{
    const EventTestRocket r;

    // A mount is what a BURNOUT needs: accepted, and the pointer is gone.
    const Result<FlightEvent> burnout =
        FlightEvent::createDetached(Type::BURNOUT, 2.0, r.sustainerMount, Data{}, kGivenId);
    ASSERT_TRUE(burnout.has_value());
    EXPECT_EQ(burnout->getSource(), nullptr);
    EXPECT_TRUE(burnout->hasSource());
    EXPECT_EQ(burnout->getSourceId(), r.sustainerMount->getId());
    EXPECT_EQ(burnout->getId(), kGivenId);
    EXPECT_EQ(burnout->getType(), Type::BURNOUT);
    EXPECT_EQ(burnout->getTime(), 2.0);
    EXPECT_FALSE(burnout->hasData());
    EXPECT_NO_THROW(burnout->validate());
    EXPECT_EQ(burnout->toString(), "FlightEvent[type=BURNOUT,time=2.0,source=" +
                                       r.sustainerMount->getId().toString() + ",data=null]");

    // The class of the source is checked while the component is at hand: Java's messages.
    const Result<FlightEvent> wrongSource =
        FlightEvent::createDetached(Type::BURNOUT, 2.0, r.chute);
    ASSERT_FALSE(wrongSource.has_value());
    EXPECT_EQ(wrongSource.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(wrongSource.error().message,
              "BURNOUT events should have MotorMount type data payloads, instead of Parachute");
    const Result<FlightEvent> wrongStage =
        FlightEvent::createDetached(Type::EJECTION_CHARGE, 2.0, r.sustainerBody);
    ASSERT_FALSE(wrongStage.has_value());
    EXPECT_EQ(
        wrongStage.error().message,
        "EJECTION_CHARGE events should have AxialStage type data payloads, instead of BodyTube");
    // The same event by its id alone is taken: nothing can be checked.
    EXPECT_TRUE(
        FlightEvent::create(Type::EJECTION_CHARGE, 2.0, r.sustainerBody->getId()).has_value());

    const Result<FlightEvent> warned =
        FlightEvent::createDetached(Type::SIM_WARN, 2.0, r.sustainer, warning("custom text"));
    ASSERT_FALSE(warned.has_value());
    EXPECT_EQ(warned.error().message,
              "SIM_WARN event requires null source component; was Sustainer");
    const Result<FlightEvent> nan = FlightEvent::createDetached(Type::LAUNCH, kNaN, &r.rocket);
    ASSERT_FALSE(nan.has_value());
    EXPECT_EQ(nan.error().message, "LAUNCH event has a NaN time!");
}

TEST(FlightEventSourceId, ADetachedEventWithoutASourceHasNone)
{
    const Result<FlightEvent> bare = FlightEvent::createDetached(Type::APOGEE, 4.0, nullptr);
    ASSERT_TRUE(bare.has_value());
    EXPECT_EQ(bare->getSource(), nullptr);
    EXPECT_FALSE(bare->hasSource());
    EXPECT_EQ(bare->getSourceId(), std::nullopt);
    EXPECT_FALSE(bare->getId().isNil()) << "a drawn id";

    const Result<FlightEvent> abort = FlightEvent::createDetached(
        Type::SIM_ABORT, 4.0, nullptr, SimulationAbort{SimulationAbort::Cause::NO_CP});
    ASSERT_TRUE(abort.has_value());
    ASSERT_NE(abort->getAbort(), nullptr);
}

TEST(FlightEventSourceId, ADetachedEventOutlivesTheRocketItWasCheckedAgainst)
{
    std::optional<FlightEvent> event;
    Uuid                       stageId;
    {
        const EventTestRocket     r;
        const Result<FlightEvent> separation =
            FlightEvent::createDetached(Type::STAGE_SEPARATION, 2.0, r.booster);
        ASSERT_TRUE(separation.has_value());
        event   = *separation;
        stageId = r.booster->getId();
    }
    // The rocket is gone; nothing of the event points into it.
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->getSourceId(), stageId);
    EXPECT_EQ(event->toString(), "FlightEvent[type=STAGE_SEPARATION,time=2.0,source=" +
                                     stageId.toString() + ",data=null]");
    EXPECT_EQ(event->compareTo(FlightEvent{Type::STAGE_SEPARATION, 2.0}), 0);
}

TEST(FlightEventSourceId, ItIsOrderedAsAnEventWithoutASource)
{
    const EventTestRocket r;
    const Uuid            sourceId = kSourceId;
    const FlightEvent     byId{Type::BURNOUT, 2.0, sourceId, Data{}};
    const FlightEvent     noSource{Type::BURNOUT, 2.0};
    const FlightEvent     byPointer{Type::BURNOUT, 2.0, r.boosterBody};

    EXPECT_EQ(byId.compareTo(noSource), 0);
    EXPECT_EQ(noSource.compareTo(byId), 0);
    EXPECT_EQ(byId.compareTo(byPointer), -1);
    EXPECT_EQ(byPointer.compareTo(byId), 1);
}

// ==================================================================== compareTo(), equals()

/// The events of the Java probe's comparison matrix (MiscProbe.java, "compare"), in its order.
[[nodiscard]] std::vector<FlightEvent> comparisonEvents(const EventTestRocket& r)
{
    const std::shared_ptr<const Warning> aoaSmall = std::make_shared<Warning::LargeAOA>(0.3);
    const std::shared_ptr<const Warning> aoaLarge = std::make_shared<Warning::LargeAOA>(0.6);
    return {
        FlightEvent{Type::LAUNCH, 0.0, &r.rocket},                          // 0
        FlightEvent{Type::IGNITION, 0.0, r.sustainerMount, r.state},        // 1
        FlightEvent{Type::IGNITION, 0.0, r.boosterBody},                    // 2
        FlightEvent{Type::IGNITION, 0.0, r.strapOnBody},                    // 3
        FlightEvent{Type::LIFTOFF, 0.0},                                    // 4
        FlightEvent{Type::EXCEPTION, 0.0},                                  // 5
        FlightEvent{Type::ALTITUDE, 0.0, r.sustainer},                      // 6
        FlightEvent{Type::ALTITUDE, 0.5, r.booster},                        // 7
        FlightEvent{Type::BURNOUT, 0.5, r.strapOnBody},                     // 8
        FlightEvent{Type::STAGE_SEPARATION, 0.5, r.strapOns},               // 9
        FlightEvent{Type::SIM_WARN, 0.5, nullptr, aoaSmall},                // 10
        FlightEvent{Type::SIM_WARN, 0.75, nullptr, aoaLarge},               // 11
        FlightEvent{Type::SIM_WARN, 0.5, nullptr, warning("custom text")},  // 12
        FlightEvent{Type::IGNITION, 0.0, r.boosterBody},               // 13: as 2, another event
        FlightEvent{Type::APOGEE, 0.5, &r.rocket},                     // 14
        FlightEvent{Type::RECOVERY_DEVICE_DEPLOYMENT, -1.0, r.chute},  // 15
    };
}

/// compareTo() of event @p i of @p events with each of them.
[[nodiscard]] std::vector<int> compareRow(const std::vector<FlightEvent>& events, std::size_t i)
{
    std::vector<int> row;
    row.reserve(events.size());
    for (const FlightEvent& other : events)
    {
        row.push_back(events.at(i).compareTo(other));
    }
    return row;
}

/// equals() of event @p i of @p events with each of them, as a string of T and F.
[[nodiscard]] std::string equalsRow(const std::vector<FlightEvent>& events, std::size_t i)
{
    std::string row;
    for (const FlightEvent& other : events)
    {
        row += events.at(i).equals(other) ? 'T' : 'F';
    }
    return row;
}

TEST(FlightEventOrder, CompareToIsJavas)
{
    const EventTestRocket          r;
    const std::vector<FlightEvent> events = comparisonEvents(r);
    using Row                             = std::vector<int>;
    // The time first; then no source first; then the higher stage number first (the rocket
    // itself is stage -1); then the difference of the type ordinals (-10, 13, -2).
    EXPECT_EQ(compareRow(events, 0), (Row{0, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 1),
              (Row{-1, 0, 1, 1, 1, 1, -10, -1, -1, -1, -1, -1, -1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 2),
              (Row{-1, -1, 0, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, 0, -1, 1}));
    EXPECT_EQ(compareRow(events, 3),
              (Row{-1, -1, -1, 0, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1}));
    EXPECT_EQ(compareRow(events, 4),
              (Row{-1, -1, -1, -1, 0, -13, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1}));
    EXPECT_EQ(compareRow(events, 5),
              (Row{-1, -1, -1, -1, 13, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1}));
    EXPECT_EQ(compareRow(events, 6),
              (Row{-1, 10, 1, 1, 1, 1, 0, -1, -1, -1, -1, -1, -1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 7), (Row{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, -1, 1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 8), (Row{1, 1, 1, 1, 1, 1, 1, -1, 0, -2, 1, -1, 1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 9), (Row{1, 1, 1, 1, 1, 1, 1, -1, 2, 0, 1, -1, 1, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 10), (Row{1, 1, 1, 1, 1, 1, 1, -1, -1, -1, 0, -1, 0, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 11), (Row{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1}));
    EXPECT_EQ(compareRow(events, 12), (Row{1, 1, 1, 1, 1, 1, 1, -1, -1, -1, 0, -1, 0, 1, -1, 1}));
    EXPECT_EQ(compareRow(events, 13),
              (Row{-1, -1, 0, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, 0, -1, 1}));
    EXPECT_EQ(compareRow(events, 14), (Row{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, 1, 0, 1}));
    EXPECT_EQ(compareRow(events, 15),
              (Row{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0}));
}

TEST(FlightEventOrder, EqualsIsJavasOverload)
{
    const EventTestRocket          r;
    const std::vector<FlightEvent> events = comparisonEvents(r);
    // compareTo() == 0, but for two warning events: those are equal when their warnings are,
    // whatever their times (10 and 11: two LargeAOA warnings; 12: another kind of warning at the
    // time of 10).
    EXPECT_EQ(equalsRow(events, 0), "TFFFFFFFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 1), "FTFFFFFFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 2), "FFTFFFFFFFFFFTFF");
    EXPECT_EQ(equalsRow(events, 3), "FFFTFFFFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 4), "FFFFTFFFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 5), "FFFFFTFFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 6), "FFFFFFTFFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 7), "FFFFFFFTFFFFFFFF");
    EXPECT_EQ(equalsRow(events, 8), "FFFFFFFFTFFFFFFF");
    EXPECT_EQ(equalsRow(events, 9), "FFFFFFFFFTFFFFFF");
    EXPECT_EQ(equalsRow(events, 10), "FFFFFFFFFFTTFFFF");
    EXPECT_EQ(equalsRow(events, 11), "FFFFFFFFFFTTFFFF");
    EXPECT_EQ(equalsRow(events, 12), "FFFFFFFFFFFFTFFF");
    EXPECT_EQ(equalsRow(events, 13), "FFTFFFFFFFFFFTFF");
    EXPECT_EQ(equalsRow(events, 14), "FFFFFFFFFFFFFFTF");
    EXPECT_EQ(equalsRow(events, 15), "FFFFFFFFFFFFFFFT");
}

TEST(FlightEventOrder, EqualEventsAreNotTheSameEvent)
{
    const EventTestRocket          r;
    const std::vector<FlightEvent> events = comparisonEvents(r);
    EXPECT_TRUE(events.at(2).equals(events.at(13)));
    EXPECT_FALSE(events.at(2).sameEvent(events.at(13)));
    EXPECT_TRUE(events.at(2).sameEvent(events.at(2)));
}

TEST(FlightEventOrder, ASourceOutsideAStageCannotBeCompared)
{
    // Java: MiscProbe.java, "compare detached".
    const EventTestRocket r;
    const BodyTube        detached(0.1, 0.01);
    const FlightEvent     fromDetached{Type::LAUNCH, 0.0, &detached};

    EXPECT_EQ(
        bugText([&] { static_cast<void>(fromDetached.compareTo({Type::LAUNCH, 0.0, &r.rocket})); }),
        "getStage() called on hierarchy without an AxialStage.");
    EXPECT_THROW(static_cast<void>(fromDetached.equals({Type::LAUNCH, 0.0, &r.rocket})), BugError);
    // The stage is asked for only when the times are equal and both events have a source.
    EXPECT_EQ(fromDetached.compareTo({Type::ALTITUDE, 0.5, r.booster}), -1);
    EXPECT_EQ(fromDetached.compareTo({Type::LIFTOFF, 0.0}), 1);
}

}  // namespace
