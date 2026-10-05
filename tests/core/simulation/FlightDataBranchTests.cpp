#include "QtRocket/simulation/FlightDataBranch.h"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/DataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Uuid.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::RocketComponent;
using QtRocket::Uuid;
using QtRocket::Test::EventTestRocket;
using QtRocket::Test::expectSame;
using Id   = QtRocket::FlightDataTypeId;
using Type = QtRocket::FlightEvent::Type;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

static_assert(QtRocket::Monitorable<FlightDataBranch>);
static_assert(std::is_base_of_v<QtRocket::DataBranch<FlightDataType>, FlightDataBranch>);
static_assert(std::is_copy_constructible_v<FlightDataBranch>);

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// The names of the types of @p branch, in getTypes() order, joined by ", ".
[[nodiscard]] std::string typeNames(const FlightDataBranch& branch)
{
    std::string names;
    for (const FlightDataType* dataType : branch.getTypes())
    {
        if (!names.empty())
        {
            names += ", ";
        }
        names += dataType->getName();
    }
    return names;
}

/// The values of @p id in @p branch (empty when the branch does not have the type).
[[nodiscard]] std::vector<double> values(const FlightDataBranch& branch, Id id)
{
    return branch.get(type(id)).value_or(std::vector<double>{});
}

/// The position of each event of @p child among the events of @p parent (by id), -1 for an
/// event the parent does not have.
[[nodiscard]] std::vector<int> positionsIn(const FlightDataBranch& parent,
                                           const FlightDataBranch& child)
{
    const std::vector<FlightEvent> all = parent.getEvents();
    std::vector<int>               positions;
    for (const FlightEvent& event : child.getEvents())
    {
        int position = -1;
        for (std::size_t i = 0; i < all.size(); i++)
        {
            if (all[i].sameEvent(event))
            {
                position = static_cast<int>(i);
            }
        }
        positions.push_back(position);
    }
    return positions;
}

/// The positions, among the events of @p parent, of the events the branch of @p source takes
/// from it.
[[nodiscard]] std::vector<int> copied(const FlightDataBranch& parent, const RocketComponent* source)
{
    const FlightDataBranch child("Child", source, &parent);
    return positionsIn(parent, child);
}

/// Whether @p event is there and is the event @p expected (the same id).
[[nodiscard]] bool isEvent(const std::optional<FlightEvent>& event, const FlightEvent& expected)
{
    return event.has_value() && event->sameEvent(expected);
}

/// The parent branch of the Java probe (MiscProbe.java, parentBranch()): five rows of time,
/// altitude and velocity, an altitude that is overwritten in its row, a velocity that is never
/// set in one row, a Mach number column that starts in the third row, and fifteen events from
/// every part of the rocket.
[[nodiscard]] FlightDataBranch parentBranch(const EventTestRocket& r)
{
    FlightDataBranch parent(
        "Parent", r.sustainer,
        {type(Id::TYPE_TIME), type(Id::TYPE_VELOCITY_TOTAL), type(Id::TYPE_ALTITUDE)});
    const std::array<double, 5> time{0.0, 0.5, 1.0, 1.5, 2.0};
    const std::array<double, 5> altitude{0.0, 4.0, 9.0, 7.0, 2.0};
    const std::array<double, 5> velocity{0.0, 12.0, 6.0, -3.0, -8.0};
    for (std::size_t i = 0; i < time.size(); i++)
    {
        parent.addPoint();
        parent.setValue(type(Id::TYPE_TIME), time.at(i));
        if (i == 2)
        {
            // a value that is overwritten: the parent's maximum keeps it
            parent.setValue(type(Id::TYPE_ALTITUDE), 50.0);
        }
        parent.setValue(type(Id::TYPE_ALTITUDE), altitude.at(i));
        if (i != 3)
        {
            parent.setValue(type(Id::TYPE_VELOCITY_TOTAL), velocity.at(i));
        }
        if (i >= 2)
        {
            // a type added late: NaN in the first rows
            parent.setValue(type(Id::TYPE_MACH_NUMBER), velocity.at(i) / 340.0);
        }
    }
    parent.addEvent({Type::LAUNCH, 0.0, &r.rocket});                    // 0
    parent.addEvent({Type::IGNITION, 0.0, r.boosterBody});              // 1
    parent.addEvent({Type::IGNITION, 0.0, r.strapOnBody});              // 2
    parent.addEvent({Type::LIFTOFF, 0.1});                              // 3
    parent.addEvent({Type::BURNOUT, 1.0, r.strapOnBody});               // 4
    parent.addEvent({Type::EJECTION_CHARGE, 1.0, r.strapOns});          // 5
    parent.addEvent({Type::STAGE_SEPARATION, 1.0, r.strapOns});         // 6
    parent.addEvent({Type::BURNOUT, 2.0, r.boosterBody});               // 7
    parent.addEvent({Type::EJECTION_CHARGE, 2.0, r.booster});           // 8
    parent.addEvent({Type::STAGE_SEPARATION, 2.0, r.booster});          // 9
    parent.addEvent({Type::IGNITION, 2.0, r.sustainerMount, r.state});  // 10
    parent.addEvent({Type::RECOVERY_DEVICE_DEPLOYMENT, 5.0, r.chute});  // 11
    parent.addEvent({Type::APOGEE, 4.0, &r.rocket});                    // 12
    parent.addEvent({Type::ALTITUDE, 3.0, r.sustainer});                // 13
    parent.addEvent({Type::ALTITUDE, 3.5, r.sustainerBody});            // 14
    return parent;
}

// ============================================================================ construction

TEST(FlightDataBranch, ABranchWithTypesHasNoSourceAndNoEvents)
{
    const FlightDataBranch branch("Test", {type(Id::TYPE_TIME), type(Id::TYPE_ALTITUDE)});

    EXPECT_EQ(branch.getName(), "Test");
    EXPECT_EQ(branch.getSourceComponentId(), std::nullopt);
    EXPECT_EQ(typeNames(branch), "Time, Altitude");
    EXPECT_EQ(branch.getLength(), 0U);
    EXPECT_TRUE(branch.getEvents().empty());
    EXPECT_TRUE(branch.isMutable());
    EXPECT_EQ(branch.modId(), ModId::invalid());
    expectSame(branch.getTimeToOptimumAltitude(), kNaN);
    expectSame(branch.getOptimumAltitude(), kNaN);
    expectSame(branch.getSeparationTime(), kNaN);
    expectSame(branch.getOptimumDelay(), kNaN);
}

TEST(FlightDataBranch, TheTypesCanComeAsASpan)
{
    const std::vector<const FlightDataType*> types{&type(Id::TYPE_ALTITUDE), &type(Id::TYPE_TIME)};
    const FlightDataBranch                   branch("Span", types);
    EXPECT_EQ(typeNames(branch), "Time, Altitude") << "getTypes() sorts";
    EXPECT_EQ(branch.getSourceComponentId(), std::nullopt);

    const EventTestRocket  r;
    const FlightDataBranch sourced("Span", r.booster, types);
    EXPECT_EQ(sourced.getSourceComponentId(), r.booster->getId());
    EXPECT_EQ(typeNames(sourced), "Time, Altitude");
}

TEST(FlightDataBranch, ABranchNeedsAType)
{
    const std::vector<const FlightDataType*> none;
    EXPECT_THROW(static_cast<void>(FlightDataBranch("None", none)), BugError);
    EXPECT_THROW(static_cast<void>(FlightDataBranch("None", nullptr, none)), BugError);
    EXPECT_THROW(
        static_cast<void>(FlightDataBranch("Twice", {type(Id::TYPE_TIME), type(Id::TYPE_TIME)})),
        BugError);
}

TEST(FlightDataBranch, ABranchKeepsTheIdOfItsSourceComponent)
{
    const EventTestRocket  r;
    const FlightDataBranch branch("Sustainer", r.sustainer, {type(Id::TYPE_TIME)});
    EXPECT_EQ(branch.getSourceComponentId(), r.sustainer->getId());

    const FlightDataBranch none("None", nullptr, {type(Id::TYPE_TIME)});
    EXPECT_EQ(none.getSourceComponentId(), std::nullopt);
}

TEST(FlightDataBranch, TheEmptyBranchHasEveryTypeAndIsImmutable)
{
    // Java: new FlightDataBranch() (MiscProbe.java, "empty").
    FlightDataBranch empty;
    EXPECT_EQ(empty.getName(), "Empty branch");
    EXPECT_EQ(empty.getLength(), 0U);
    EXPECT_EQ(empty.getTypes().size(), 71U);
    EXPECT_FALSE(empty.isMutable());
    EXPECT_NE(empty.modId(), ModId::invalid());
    EXPECT_EQ(empty.getSourceComponentId(), std::nullopt);
    expectSame(empty.getMinimum(type(Id::TYPE_TIME)), kNaN);
    expectSame(empty.getMaximum(type(Id::TYPE_TIME)), kNaN);
    expectSame(empty.getLast(type(Id::TYPE_TIME)), kNaN);
    EXPECT_FALSE(empty.hasType(type(Id::TYPE_THRUST_CORRECTION))) << "not in ALL_TYPES";
    EXPECT_TRUE(empty.hasType(type(Id::TYPE_COMPUTATION_TIME)));
    expectSame(empty.getOptimumDelay(), kNaN);
    EXPECT_THROW(empty.addEvent({Type::LAUNCH, 0.0}), BugError);
    EXPECT_THROW(empty.addPoint(), BugError);
}

// ============================================================================ events

TEST(FlightDataBranch, AddEventAppendsAndDrawsAModId)
{
    const EventTestRocket r;
    FlightDataBranch      branch("Events", {type(Id::TYPE_TIME)});
    EXPECT_EQ(branch.modId(), ModId::invalid());

    const FlightEvent launch{Type::LAUNCH, 0.0};
    branch.addEvent(launch);
    const ModId afterFirst = branch.modId();
    EXPECT_NE(afterFirst, ModId::invalid());
    branch.addEvent({Type::LIFTOFF, 0.5});
    EXPECT_GT(branch.modId(), afterFirst);

    const std::vector<FlightEvent> events = branch.getEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_TRUE(events.at(0).sameEvent(launch)) << "the events keep their ids";
    EXPECT_EQ(events.at(1).getType(), Type::LIFTOFF);
    EXPECT_EQ(branch.getLength(), 0U) << "events are not rows";
}

TEST(FlightDataBranch, EventsStayInTheOrderTheyWereAddedNotInTimeOrder)
{
    FlightDataBranch branch("Events", {type(Id::TYPE_TIME)});
    branch.addEvent({Type::APOGEE, 4.0});
    branch.addEvent({Type::LAUNCH, 0.0});
    branch.addEvent({Type::GROUND_HIT, 9.0});
    branch.addEvent({Type::LIFTOFF, 0.1});

    const std::vector<FlightEvent> events = branch.getEvents();
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events.at(0).getType(), Type::APOGEE);
    EXPECT_EQ(events.at(1).getType(), Type::LAUNCH);
    EXPECT_EQ(events.at(2).getType(), Type::GROUND_HIT);
    EXPECT_EQ(events.at(3).getType(), Type::LIFTOFF);
}

TEST(FlightDataBranch, GetEventsIsACopy)
{
    FlightDataBranch branch("Events", {type(Id::TYPE_TIME)});
    branch.addEvent({Type::LAUNCH, 0.0});
    const std::vector<FlightEvent> before = branch.getEvents();
    branch.addEvent({Type::LIFTOFF, 0.5});
    EXPECT_EQ(before.size(), 1U);
    EXPECT_EQ(branch.getEvents().size(), 2U);
}

TEST(FlightDataBranch, TheSeparationTimeIsThatOfTheLastSeparationEventAdded)
{
    // Java: MiscProbe.java, "events separation time".
    const EventTestRocket r;
    FlightDataBranch      branch("Events", {type(Id::TYPE_TIME)});
    branch.addEvent({Type::LAUNCH, 0.0});
    expectSame(branch.getSeparationTime(), kNaN);
    branch.addEvent({Type::STAGE_SEPARATION, 1.5, r.booster});
    EXPECT_EQ(branch.getSeparationTime(), 1.5);
    branch.addEvent({Type::STAGE_SEPARATION, 0.5, r.strapOns});
    EXPECT_EQ(branch.getSeparationTime(), 0.5) << "the last one added, not the latest";
    branch.addEvent({Type::APOGEE, 3.0});
    EXPECT_EQ(branch.getSeparationTime(), 0.5);
}

TEST(FlightDataBranch, AddEventIsRefusedOnceTheBranchIsImmutable)
{
    FlightDataBranch branch("Events", {type(Id::TYPE_TIME)});
    branch.addEvent({Type::LAUNCH, 0.0});
    const ModId before = branch.modId();
    branch.immute();

    EXPECT_THROW(branch.addEvent({Type::LIFTOFF, 0.5}), BugError);
    EXPECT_THROW(branch.addEvent({Type::STAGE_SEPARATION, 0.5}), BugError);
    EXPECT_EQ(branch.getEvents().size(), 1U);
    expectSame(branch.getSeparationTime(), kNaN);
    EXPECT_EQ(branch.modId(), before);
}

TEST(FlightDataBranch, FirstAndLastEventOfAType)
{
    // Java: MiscProbe.java, "parent first burnout" and the lines after it.
    const EventTestRocket          r;
    const FlightDataBranch         parent = parentBranch(r);
    const std::vector<FlightEvent> events = parent.getEvents();

    EXPECT_TRUE(isEvent(parent.getFirstEvent(Type::BURNOUT), events.at(4)));
    EXPECT_TRUE(isEvent(parent.getLastEvent(Type::BURNOUT), events.at(7)));
    EXPECT_FALSE(isEvent(parent.getFirstEvent(Type::BURNOUT), events.at(7)));

    // The only event of its type is the first and the last.
    EXPECT_TRUE(isEvent(parent.getFirstEvent(Type::LIFTOFF), events.at(3)));
    EXPECT_TRUE(isEvent(parent.getLastEvent(Type::LIFTOFF), events.at(3)));

    EXPECT_EQ(parent.getFirstEvent(Type::TUMBLE), std::nullopt);
    EXPECT_EQ(parent.getLastEvent(Type::TUMBLE), std::nullopt);
}

TEST(FlightDataBranch, FindEventLooksAnEventUpByItsId)
{
    const EventTestRocket          r;
    const FlightDataBranch         parent = parentBranch(r);
    const std::vector<FlightEvent> events = parent.getEvents();

    EXPECT_TRUE(isEvent(parent.findEvent(events.at(9).getId()), events.at(9)));
    const FlightEvent found =
        parent.findEvent(events.at(9).getId()).value_or(FlightEvent{Type::LAUNCH, -1.0});
    EXPECT_EQ(found.getType(), Type::STAGE_SEPARATION);
    EXPECT_EQ(found.getSource(), r.booster);
    EXPECT_EQ(parent.findEvent(Uuid::random()), std::nullopt);
    EXPECT_EQ(parent.findEvent(Uuid::nil()), std::nullopt);
}

// ============================================================================ the optimum

TEST(FlightDataBranch, TheOptimumDelayIsTheTimeToTheOptimumAfterTheLastBurnout)
{
    // Java: MiscProbe.java, "parent optimum delay".
    const EventTestRocket r;
    FlightDataBranch      parent = parentBranch(r);
    expectSame(parent.getOptimumDelay(), kNaN);
    parent.setTimeToOptimumAltitude(6.5);
    EXPECT_EQ(parent.getOptimumDelay(), 4.5) << "6.5 - 2.0, the last BURNOUT";
    EXPECT_EQ(parent.getTimeToOptimumAltitude(), 6.5);

    FlightDataBranch noBurnout("NoBurnout", {type(Id::TYPE_TIME)});
    noBurnout.setTimeToOptimumAltitude(6.5);
    expectSame(noBurnout.getOptimumDelay(), kNaN);
    noBurnout.addEvent({Type::BURNOUT, 8.0});
    EXPECT_EQ(noBurnout.getOptimumDelay(), -1.5) << "a burnout after the optimum";
}

TEST(FlightDataBranch, TheOptimumSettersNeitherCheckMutabilityNorDrawAModId)
{
    // Java: MiscProbe.java, "setters change modID" and "setter after immute".
    FlightDataBranch branch("Events", {type(Id::TYPE_TIME)});
    branch.addEvent({Type::LAUNCH, 0.0});
    const ModId before = branch.modId();
    branch.setOptimumAltitude(1.0);
    branch.setTimeToOptimumAltitude(2.0);
    EXPECT_EQ(branch.modId(), before);
    EXPECT_EQ(branch.getOptimumAltitude(), 1.0);
    EXPECT_EQ(branch.getTimeToOptimumAltitude(), 2.0);

    branch.immute();
    branch.setOptimumAltitude(5.0);
    branch.setTimeToOptimumAltitude(kNaN);
    EXPECT_EQ(branch.getOptimumAltitude(), 5.0);
    expectSame(branch.getTimeToOptimumAltitude(), kNaN);
}

// ============================================================================ the rows

TEST(FlightDataBranch, GetDataIndexOfTimeFindsTheFirstRowAtOrAfterATime)
{
    // Java: MiscProbe.java, "index of time" (-1 is nullopt here).
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    EXPECT_EQ(parent.getDataIndexOfTime(-1.0), 0U);
    EXPECT_EQ(parent.getDataIndexOfTime(0.0), 0U);
    EXPECT_EQ(parent.getDataIndexOfTime(0.25), 1U);
    EXPECT_EQ(parent.getDataIndexOfTime(0.5), 1U);
    EXPECT_EQ(parent.getDataIndexOfTime(2.0), 4U);
    EXPECT_EQ(parent.getDataIndexOfTime(2.5), std::nullopt);
    EXPECT_EQ(parent.getDataIndexOfTime(kNaN), std::nullopt);
    EXPECT_EQ(parent.getDataIndexOfTime(-std::numeric_limits<double>::infinity()), 0U);
    EXPECT_EQ(parent.getDataIndexOfTime(std::numeric_limits<double>::infinity()), std::nullopt);
}

TEST(FlightDataBranch, GetDataIndexOfTimeNeedsATimeColumnAndSkipsNaNTimes)
{
    FlightDataBranch noTime("NoTime", {type(Id::TYPE_ALTITUDE)});
    noTime.addPoint();
    noTime.setValue(type(Id::TYPE_ALTITUDE), 3.0);
    EXPECT_EQ(noTime.getDataIndexOfTime(0.0), std::nullopt);

    const FlightDataBranch noRows("NoRows", {type(Id::TYPE_TIME)});
    EXPECT_EQ(noRows.getDataIndexOfTime(0.0), std::nullopt);

    FlightDataBranch gaps("Gaps", {type(Id::TYPE_TIME)});
    gaps.addPoint();  // the time of this row stays NaN
    gaps.addPoint();
    gaps.setValue(type(Id::TYPE_TIME), 1.0);
    EXPECT_EQ(gaps.getDataIndexOfTime(0.5), 1U) << "NaN >= 0.5 is false";
}

// =================================================================== the branch of a stage

TEST(FlightDataBranch, TheParentOfTheProbeIsAsInJava)
{
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    EXPECT_EQ(typeNames(parent), "Time, Altitude, Total velocity, Mach number");
    EXPECT_EQ(parent.getMaximum(type(Id::TYPE_ALTITUDE)), 50.0) << "the overwritten value";
    EXPECT_EQ(parent.getMinimum(type(Id::TYPE_ALTITUDE)), 0.0);
    EXPECT_EQ(parent.getMaximum(type(Id::TYPE_VELOCITY_TOTAL)), 12.0);
    EXPECT_EQ(parent.getMinimum(type(Id::TYPE_VELOCITY_TOTAL)), -8.0);
    EXPECT_EQ(parent.getSeparationTime(), 2.0);
    EXPECT_EQ(parent.getSourceComponentId(), r.sustainer->getId());
    EXPECT_EQ(parent.getEvents().size(), 15U);
}

TEST(FlightDataBranch, TheBranchOfAStageCopiesTheRowsOfItsParent)
{
    // Java: new FlightDataBranch(name, srcComponent, parent) (MiscProbe.java, "child").
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    const FlightDataBranch child("Child", r.booster, &parent);

    EXPECT_EQ(child.getName(), "Child");
    EXPECT_EQ(child.getSourceComponentId(), r.booster->getId());
    EXPECT_EQ(typeNames(child), "Time, Altitude, Total velocity, Mach number");
    EXPECT_EQ(child.getLength(), 5U);
    expectSame(values(child, Id::TYPE_TIME), {0.0, 0.5, 1.0, 1.5, 2.0});
    expectSame(values(child, Id::TYPE_ALTITUDE), {0.0, 4.0, 9.0, 7.0, 2.0});
    expectSame(values(child, Id::TYPE_VELOCITY_TOTAL), {0.0, 12.0, 6.0, kNaN, -8.0});
    expectSame(values(child, Id::TYPE_MACH_NUMBER),
               {kNaN, kNaN, 0.01764705882352941, -0.008823529411764706, -0.023529411764705882});
    EXPECT_TRUE(child.isMutable());
}

TEST(FlightDataBranch, TheBranchOfAStageHasTheMinimaAndMaximaOfTheCopiedValues)
{
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    const FlightDataBranch child("Child", r.booster, &parent);

    EXPECT_EQ(child.getMaximum(type(Id::TYPE_ALTITUDE)), 9.0)
        << "not the parent's 50, a value that was overwritten there";
    EXPECT_EQ(child.getMinimum(type(Id::TYPE_ALTITUDE)), 0.0);
    EXPECT_EQ(child.getMaximum(type(Id::TYPE_VELOCITY_TOTAL)), 12.0);
    EXPECT_EQ(child.getMinimum(type(Id::TYPE_VELOCITY_TOTAL)), -8.0);
    EXPECT_EQ(child.getMaximum(type(Id::TYPE_MACH_NUMBER)), 0.01764705882352941);
    EXPECT_EQ(child.getMinimum(type(Id::TYPE_MACH_NUMBER)), -0.023529411764705882);
}

TEST(FlightDataBranch, TheBranchOfAStageStartsWithoutSeparationTimeAndOptimum)
{
    const EventTestRocket r;
    FlightDataBranch      parent = parentBranch(r);
    parent.setOptimumAltitude(99.0);
    parent.setTimeToOptimumAltitude(3.0);
    const FlightDataBranch child("Child", r.booster, &parent);

    expectSame(child.getSeparationTime(), kNaN);
    expectSame(child.getOptimumAltitude(), kNaN);
    expectSame(child.getTimeToOptimumAltitude(), kNaN);
    EXPECT_NE(child.modId(), ModId::invalid()) << "the rows were written";
    EXPECT_NE(child.modId(), parent.modId());
}

TEST(FlightDataBranch, TheBranchOfAStageTakesTheEventsOfThatStage)
{
    // Java: MiscProbe.java, "child ... events": the positions of the copied events in the
    // parent.
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    using Positions               = std::vector<int>;

    EXPECT_EQ(copied(parent, nullptr), (Positions{})) << "no component, no events";
    EXPECT_EQ(copied(parent, &r.rocket), (Positions{0, 12})) << "the events of the rocket itself";
    EXPECT_EQ(copied(parent, r.sustainer), (Positions{10, 11, 13, 14}));
    EXPECT_EQ(copied(parent, r.booster), (Positions{1, 7, 8}))
        << "not the strap-ons' events, which are in a stage of their own, and no separation";
    EXPECT_EQ(copied(parent, r.strapOns), (Positions{2, 4, 5}));
    EXPECT_EQ(copied(parent, r.boosterBody), (Positions{1, 7}))
        << "a component that is not a stage: its own events and its descendants' of its stage";
    EXPECT_EQ(copied(parent, r.sustainerBody), (Positions{10, 11, 14}));
    EXPECT_EQ(copied(parent, r.strapOnBody), (Positions{2, 4}));
    EXPECT_EQ(copied(parent, r.sustainerMount), (Positions{10}));
    EXPECT_EQ(copied(parent, r.chute), (Positions{11}));
}

TEST(FlightDataBranch, TheCopiedEventsAreTheParentsEvents)
{
    const EventTestRocket  r;
    const FlightDataBranch parent = parentBranch(r);
    const FlightDataBranch child("Child", r.sustainer, &parent);

    const std::vector<FlightEvent> events = child.getEvents();
    ASSERT_EQ(events.size(), 4U);
    EXPECT_TRUE(events.at(0).sameEvent(parent.getEvents().at(10)));
    EXPECT_EQ(events.at(0).getMotorState(), r.state) << "with the same motor state";
    EXPECT_EQ(events.at(0).getSource(), r.sustainerMount);
}

TEST(FlightDataBranch, TheBranchOfAStageCanBeMadeFromAnImmutableParent)
{
    const EventTestRocket r;
    FlightDataBranch      parent = parentBranch(r);
    parent.immute();
    FlightDataBranch child("Child", r.booster, &parent);
    EXPECT_TRUE(child.isMutable());
    EXPECT_NO_THROW(child.addEvent({Type::STAGE_SEPARATION, 2.0, r.booster}));
    EXPECT_EQ(child.getSeparationTime(), 2.0);
    EXPECT_EQ(child.getEvents().size(), 4U);
}

TEST(FlightDataBranch, AParentWithoutRowsGivesATimeColumnOnly)
{
    // Java: MiscProbe.java, "of empty".
    const EventTestRocket r;
    FlightDataBranch emptyParent("EmptyParent", {type(Id::TYPE_ALTITUDE), type(Id::TYPE_TIME)});
    emptyParent.addEvent({Type::IGNITION, 0.0, r.boosterBody});
    const FlightDataBranch child("Child", r.booster, &emptyParent);

    EXPECT_EQ(typeNames(child), "Time") << "the parent's types come with its rows";
    EXPECT_EQ(child.getLength(), 0U);
    EXPECT_EQ(child.getEvents().size(), 1U) << "the events are copied all the same";
    EXPECT_EQ(child.modId(), ModId::invalid()) << "nothing was written";
}

TEST(FlightDataBranch, WithoutAParentThereIsATimeColumnOnly)
{
    // Java: MiscProbe.java, "of null".
    const EventTestRocket  r;
    const FlightDataBranch child("Child", r.booster, static_cast<const FlightDataBranch*>(nullptr));
    EXPECT_EQ(typeNames(child), "Time");
    EXPECT_EQ(child.getLength(), 0U);
    EXPECT_EQ(child.modId(), ModId::invalid());
    EXPECT_EQ(child.getSourceComponentId(), r.booster->getId());
    EXPECT_TRUE(child.getEvents().empty());
    EXPECT_TRUE(child.isMutable());
}

TEST(FlightDataBranch, AParentWithoutATimeColumnStillGivesOne)
{
    // Java: MiscProbe.java, "of no time".
    FlightDataBranch noTime("NoTime", {type(Id::TYPE_ALTITUDE)});
    noTime.addPoint();
    noTime.setValue(type(Id::TYPE_ALTITUDE), 3.0);
    const FlightDataBranch child("Child", nullptr, &noTime);

    EXPECT_EQ(typeNames(child), "Time, Altitude");
    expectSame(values(child, Id::TYPE_TIME), {kNaN});
    expectSame(values(child, Id::TYPE_ALTITUDE), {3.0});
    EXPECT_EQ(child.getSourceComponentId(), std::nullopt);
}

TEST(FlightDataBranch, AnEventWhoseSourceIsInNoStageCannotBeSorted)
{
    const EventTestRocket r;
    const BodyTube        detached(0.1, 0.01);
    FlightDataBranch      parent("Parent", {type(Id::TYPE_TIME)});
    parent.addEvent({Type::LAUNCH, 0.0, &detached});

    EXPECT_THROW(static_cast<void>(FlightDataBranch("Child", r.booster, &parent)), BugError)
        << "Java: the IllegalStateException of getStage()";
    // Without a component of its own the branch takes no event, and never looks.
    EXPECT_NO_THROW(static_cast<void>(FlightDataBranch("Child", nullptr, &parent)));
}

TEST(FlightDataBranch, AnEventThatKnowsItsSourceByIdOnlyIsNotCopied)
{
    const EventTestRocket r;
    FlightDataBranch      parent("Parent", {type(Id::TYPE_TIME)});
    parent.addEvent(FlightEvent{Type::BURNOUT, 1.0, r.boosterBody->getId(), FlightEvent::Data{}});
    parent.addEvent({Type::BURNOUT, 2.0, r.boosterBody});
    const FlightDataBranch child("Child", r.booster, &parent);
    EXPECT_EQ(positionsIn(parent, child), (std::vector<int>{1}));
}

// ============================================================================ clone()

TEST(FlightDataBranch, CloneIsAMutableCopyWithoutTheSeparationTime)
{
    // Java: MiscProbe.java, "clone".
    const EventTestRocket r;
    FlightDataBranch      original = parentBranch(r);
    original.setOptimumAltitude(123.5);
    original.setTimeToOptimumAltitude(4.25);
    original.immute();
    const FlightDataBranch clone = original.clone();

    EXPECT_TRUE(clone.isMutable());
    EXPECT_FALSE(original.isMutable());
    EXPECT_EQ(clone.modId(), original.modId());
    expectSame(clone.getSeparationTime(), kNaN);
    EXPECT_EQ(original.getSeparationTime(), 2.0);
    EXPECT_EQ(clone.getOptimumAltitude(), 123.5);
    EXPECT_EQ(clone.getTimeToOptimumAltitude(), 4.25);
    EXPECT_EQ(clone.getOptimumDelay(), 2.25);
    EXPECT_EQ(clone.getSourceComponentId(), r.sustainer->getId());
    EXPECT_EQ(clone.getName(), "Parent");
    EXPECT_EQ(typeNames(clone), "Time, Altitude, Total velocity, Mach number");
    EXPECT_EQ(clone.getEvents().size(), 15U);
    EXPECT_EQ(clone.getMaximum(type(Id::TYPE_ALTITUDE)), 50.0)
        << "the maxima are copied as they are";
    expectSame(values(clone, Id::TYPE_ALTITUDE), {0.0, 4.0, 9.0, 7.0, 2.0});
    expectSame(values(clone, Id::TYPE_MACH_NUMBER),
               {kNaN, kNaN, 0.01764705882352941, -0.008823529411764706, -0.023529411764705882});
}

TEST(FlightDataBranch, ACloneIsIndependentOfItsOriginal)
{
    const EventTestRocket r;
    FlightDataBranch      original = parentBranch(r);
    FlightDataBranch      clone    = original.clone();

    clone.addPoint();
    clone.setValue(type(Id::TYPE_TIME), 9.0);
    clone.addEvent({Type::GROUND_HIT, 9.0});
    EXPECT_EQ(original.getLength(), 5U);
    EXPECT_EQ(original.getEvents().size(), 15U);
    EXPECT_EQ(clone.getLength(), 6U);
    EXPECT_EQ(clone.getEvents().size(), 16U);
    EXPECT_TRUE(clone.getEvents().at(3).sameEvent(original.getEvents().at(3)));
}

TEST(FlightDataBranch, ACopyKeepsEverythingACloneDropsToo)
{
    const EventTestRocket r;
    FlightDataBranch      original = parentBranch(r);
    original.immute();
    const FlightDataBranch copy(original);

    EXPECT_FALSE(copy.isMutable()) << "a copy keeps the immutability, clone() does not";
    EXPECT_EQ(copy.getSeparationTime(), 2.0);
    EXPECT_EQ(copy.modId(), original.modId());
    EXPECT_EQ(copy.getEvents().size(), 15U);
    EXPECT_EQ(copy.getSourceComponentId(), r.sustainer->getId());
}

}  // namespace
