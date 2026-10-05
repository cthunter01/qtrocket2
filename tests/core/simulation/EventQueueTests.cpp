#include "QtRocket/simulation/EventQueue.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::EventQueue;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Uuid;
using QtRocket::Test::EventTestRocket;
using Type = QtRocket::FlightEvent::Type;

static_assert(QtRocket::Monitorable<EventQueue>);
static_assert(std::is_nothrow_move_constructible_v<EventQueue>);
static_assert(std::is_nothrow_move_assignable_v<EventQueue>);

/// An event with the id (0, @p label), so that a test can tell which event came out.
[[nodiscard]] FlightEvent labelled(Type type, double time, const RocketComponent* source,
                                   std::uint64_t label)
{
    return FlightEvent{type, time, source, FlightEvent::Data{}, Uuid{0, label}};
}

[[nodiscard]] std::int64_t labelOf(const FlightEvent& event)
{
    return static_cast<std::int64_t>(event.getId().leastSignificantBits());
}

/// The labels of the queued events in array order.
[[nodiscard]] std::vector<std::int64_t> arrayOrder(const EventQueue& queue)
{
    std::vector<std::int64_t> labels;
    for (const FlightEvent& event : queue)
    {
        labels.push_back(labelOf(event));
    }
    return labels;
}

/// The label of the event poll() takes out of @p queue, or -1 when it is empty.
[[nodiscard]] std::int64_t pollLabel(EventQueue& queue)
{
    const std::optional<FlightEvent> event = queue.poll();
    return event.has_value() ? labelOf(*event) : -1;
}

/// The label of the event at the head of @p queue, or -1 when it is empty.
[[nodiscard]] std::int64_t peekLabel(const EventQueue& queue)
{
    const FlightEvent* event = queue.peek();
    return event != nullptr ? labelOf(*event) : -1;
}

/// Polls @p queue empty and returns the labels in the order they came out.
[[nodiscard]] std::vector<std::int64_t> drain(EventQueue& queue)
{
    std::vector<std::int64_t> labels;
    while (const std::optional<FlightEvent> event = queue.poll())
    {
        labels.push_back(labelOf(*event));
    }
    return labels;
}

// ============================================================================ the order

TEST(EventQueue, ANewQueueIsEmpty)
{
    EventQueue queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0U);
    EXPECT_EQ(queue.peek(), nullptr);
    EXPECT_EQ(queue.poll(), std::nullopt);
    EXPECT_EQ(queue.begin(), queue.end());
    EXPECT_EQ(queue.toString(), "[]");
    EXPECT_FALSE(queue.iterator().hasNext());
}

TEST(EventQueue, PollsByTimeThenSourceThenStageThenType)
{
    const EventTestRocket r;
    // The events and the expected orders are those of probes/events-data-impl/MiscProbe.java
    // ("mixed array" and "mixed polled").
    const std::array<FlightEvent, 12> events{
        labelled(Type::ALTITUDE, 0.5, &r.rocket, 0),
        labelled(Type::APOGEE, 0.5, &r.rocket, 1),
        labelled(Type::BURNOUT, 0.5, r.boosterBody, 2),
        labelled(Type::BURNOUT, 0.5, r.strapOnBody, 3),
        labelled(Type::IGNITION, 0.5, r.sustainerMount, 4),
        labelled(Type::LIFTOFF, 0.5, nullptr, 5),
        labelled(Type::TUMBLE, 0.5, nullptr, 6),
        labelled(Type::LAUNCH, 0.25, &r.rocket, 7),
        labelled(Type::EJECTION_CHARGE, 0.5, r.booster, 8),
        labelled(Type::STAGE_SEPARATION, 0.5, r.strapOns, 9),
        labelled(Type::SIMULATION_END, 0.75, nullptr, 10),
        labelled(Type::RECOVERY_DEVICE_DEPLOYMENT, 0.5, r.chute, 11)};
    EventQueue queue;
    for (const FlightEvent& event : events)
    {
        queue.add(event);
    }

    EXPECT_EQ(queue.size(), 12U);
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{7, 5, 6, 2, 9, 11, 3, 0, 8, 4, 10, 1}));
    EXPECT_EQ(peekLabel(queue), 7);
    // The earliest first; at 0.5 s the events without a source (by type), then the strap-ons
    // (stage 2), the booster (stage 1), the sustainer (stage 0) and the rocket (stage -1).
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{7, 5, 6, 3, 9, 2, 8, 4, 11, 1, 0, 10}));
    EXPECT_TRUE(queue.empty());
}

/// Adds @p count events that compare equal, labelled 0 to @p count - 1 in that order, and
/// returns the order they are polled in.
[[nodiscard]] std::vector<std::int64_t> pollOrderOfTies(std::uint64_t count)
{
    const EventTestRocket r;
    EventQueue            queue;
    for (std::uint64_t label = 0; label < count; label++)
    {
        queue.add(labelled(Type::IGNITION, 1.0, r.boosterBody, label));
    }
    return drain(queue);
}

TEST(EventQueue, EventsThatCompareEqualComeOutInHeapOrderNotInInsertionOrder)
{
    // Java (MiscProbe.java, "ties"): the motors of one stage that ignite at the same time.
    EXPECT_EQ(pollOrderOfTies(1), (std::vector<std::int64_t>{0}));
    EXPECT_EQ(pollOrderOfTies(2), (std::vector<std::int64_t>{0, 1}));
    EXPECT_EQ(pollOrderOfTies(3), (std::vector<std::int64_t>{0, 2, 1}));
    EXPECT_EQ(pollOrderOfTies(4), (std::vector<std::int64_t>{0, 3, 2, 1}));
    EXPECT_EQ(pollOrderOfTies(5), (std::vector<std::int64_t>{0, 4, 3, 2, 1}));
    EXPECT_EQ(pollOrderOfTies(6), (std::vector<std::int64_t>{0, 5, 4, 3, 2, 1}));
    EXPECT_EQ(pollOrderOfTies(7), (std::vector<std::int64_t>{0, 6, 5, 4, 3, 2, 1}));
}

TEST(EventQueue, OfferIsAdd)
{
    EventQueue added;
    EventQueue offered;
    for (std::uint64_t label = 0; label < 9; label++)
    {
        const auto time = static_cast<double>((label * 5) % 4);
        added.add(labelled(Type::ALTITUDE, time, nullptr, label));
        offered.offer(labelled(Type::ALTITUDE, time, nullptr, label));
    }
    EXPECT_EQ(arrayOrder(added), arrayOrder(offered));
    EXPECT_EQ(drain(added), drain(offered));
}

// ================================================================== the script against Java

/// A 64-bit linear congruential generator, the same as in the Java probe.
class Lcg
{
public:
    explicit Lcg(std::uint64_t seed) : m_state(seed) { }

    /// The next number in [0, bound).
    [[nodiscard]] std::uint64_t next(std::uint64_t bound)
    {
        m_state = (m_state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return (m_state >> 33U) % bound;
    }

private:
    std::uint64_t m_state;
};

/// One run of the Java probe (probes/events-data-impl/QueueProbe.java): its parameters and the
/// trace it printed, the numbers separated by spaces.
struct JavaTrace
{
    std::uint64_t    seed;
    int              operations;
    std::size_t      typeCount;
    std::size_t      timeCount;
    std::string_view trace;
};

/// The numbers of @p text, which are separated by single spaces.
[[nodiscard]] std::vector<std::int64_t> parseTrace(std::string_view text)
{
    std::vector<std::int64_t> values;
    while (!text.empty())
    {
        const std::size_t        end   = std::min(text.find(' '), text.size());
        const std::string_view   token = text.substr(0, end);
        const std::optional<int> value = QtRocket::Strings::parseInt(token);
        if (!value.has_value())
        {
            QtRocket::bug("not a number in a trace: " + std::string{token});
        }
        values.push_back(*value);
        text.remove_prefix(std::min(end + 1, text.size()));
    }
    return values;
}

constexpr std::array<Type, 14> kScriptTypes{
    Type::ALTITUDE,        Type::IGNITION,         Type::BURNOUT,
    Type::EJECTION_CHARGE, Type::STAGE_SEPARATION, Type::RECOVERY_DEVICE_DEPLOYMENT,
    Type::APOGEE,          Type::LAUNCH,           Type::LIFTOFF,
    Type::LAUNCHROD,       Type::GROUND_HIT,       Type::SIMULATION_END,
    Type::TUMBLE,          Type::EXCEPTION};

constexpr std::array<double, 6> kScriptTimes{0.0, 0.5, 1.0, 1.5, 2.0, 2.5};

/// Whether FlightEvent::validate() accepts the source number @p source for @p type.
[[nodiscard]] bool validSource(Type type, std::uint64_t source)
{
    switch (type)
    {
        case Type::IGNITION:
        case Type::BURNOUT:
            return source == 0 || source == 3 || source == 5 || source == 7;
        case Type::EJECTION_CHARGE:
            return source == 0 || source == 2 || source == 4 || source == 6;
        default:
            return true;
    }
}

/// The order-sensitive checksum of the labels of @p events that the probe prints.
template <class Events>
[[nodiscard]] std::int64_t orderHash(const Events& events)
{
    std::int64_t hash = 17;
    for (const FlightEvent& event : events)
    {
        hash = ((hash * 31) + labelOf(event) + 1) % 1000000007LL;
    }
    return hash;
}

/// The script of the Java probe: scripted sequences of add, offer, poll, remove, removal
/// through the iterator, copy, addAll, peek and clear over events with many ties in time, stage
/// and type, recording what comes out.
class QueueScript
{
public:
    QueueScript(std::uint64_t seed, std::size_t typeCount, std::size_t timeCount)
      : m_rng(seed), m_typeCount(typeCount), m_timeCount(timeCount)
    {
        AxialStage& s0 = m_rocket.addChild(std::make_unique<AxialStage>());
        AxialStage& s1 = m_rocket.addChild(std::make_unique<AxialStage>());
        AxialStage& s2 = m_rocket.addChild(std::make_unique<AxialStage>());
        BodyTube&   b0 = s0.addChild(std::make_unique<BodyTube>(0.1, 0.01));
        BodyTube&   b1 = s1.addChild(std::make_unique<BodyTube>(0.1, 0.01));
        BodyTube&   b2 = s2.addChild(std::make_unique<BodyTube>(0.1, 0.01));
        m_sources      = {nullptr, &m_rocket, &s0, &b0, &s1, &b1, &s2, &b2};
    }

    /// Runs @p operations steps, drains the queue and returns the trace.
    [[nodiscard]] std::vector<std::int64_t> run(int operations)
    {
        for (int step = 0; step < operations; step++)
        {
            runStep();
            if (step % 20 == 19)
            {
                m_trace.push_back(orderHash(m_queue));
            }
        }
        m_trace.push_back(static_cast<std::int64_t>(m_queue.size()));
        for (const std::int64_t label : drain(m_queue))
        {
            m_trace.push_back(label);
        }
        return m_trace;
    }

    [[nodiscard]] std::uint64_t eventCount() const noexcept { return m_counter; }

private:
    void runStep()
    {
        const std::uint64_t op = m_rng.next(100);
        if (op < 48 || m_queue.size() < 6)
        {
            addEvent();
        }
        else if (op < 70)
        {
            pollEvent();
        }
        else if (op < 88)
        {
            removeEvent();
        }
        else if (op < 93)
        {
            removeThroughIterator();
        }
        else
        {
            runRareStep(op);
        }
    }

    void runRareStep(std::uint64_t op)
    {
        if (op < 96)
        {
            EventQueue copy(m_queue);
            m_queue = std::move(copy);
            m_trace.push_back(orderHash(m_queue));
        }
        else if (op < 98)
        {
            EventQueue other;
            other.addAll(m_queue);
            m_queue = std::move(other);
            m_trace.push_back(orderHash(m_queue));
        }
        else if (op < 99)
        {
            const FlightEvent* event = m_queue.peek();
            m_trace.push_back(event == nullptr ? -1 : labelOf(*event));
        }
        else
        {
            m_trace.push_back(orderHash(m_queue));
            m_queue.clear();
            m_trace.push_back(static_cast<std::int64_t>(m_queue.size()));
        }
    }

    void addEvent()
    {
        const Type    type   = kScriptTypes.at(m_rng.next(m_typeCount));
        const double  time   = kScriptTimes.at(m_rng.next(m_timeCount));
        std::uint64_t source = m_rng.next(m_sources.size());
        if (!validSource(type, source))
        {
            source = 0;
        }
        FlightEvent event = labelled(type, time, m_sources.at(source), m_counter);
        m_counter++;
        m_live.push_back(event);
        if ((m_counter & 1U) == 0)
        {
            m_queue.add(std::move(event));
        }
        else
        {
            m_queue.offer(std::move(event));
        }
    }

    void pollEvent()
    {
        const std::optional<FlightEvent> event = m_queue.poll();
        m_trace.push_back(event.has_value() ? labelOf(*event) : -1);
        if (event.has_value())
        {
            std::erase_if(m_live,
                          [&event](const FlightEvent& live) { return live.sameEvent(*event); });
        }
    }

    /// Removes an event that was created; it may be gone already.
    void removeEvent()
    {
        const FlightEvent& event   = m_live.at(m_rng.next(m_live.size()));
        const bool         removed = m_queue.remove(event);
        m_trace.push_back(removed ? 1 : 0);
        m_trace.push_back(static_cast<std::int64_t>(m_queue.size()));
    }

    /// Iterates the queue and removes, through the iterator, the events whose label has a
    /// drawn remainder.
    void removeThroughIterator()
    {
        const auto modulus = static_cast<std::int64_t>(2 + m_rng.next(3));
        const auto rest =
            static_cast<std::int64_t>(m_rng.next(static_cast<std::uint64_t>(modulus)));
        std::int64_t         visited  = 17;
        std::int64_t         removed  = 0;
        EventQueue::Iterator iterator = m_queue.iterator();
        while (iterator.hasNext())
        {
            const FlightEvent event = iterator.next();
            visited                 = ((visited * 31) + labelOf(event) + 1) % 1000000007LL;
            if (labelOf(event) % modulus == rest)
            {
                iterator.remove();
                removed++;
            }
        }
        m_trace.push_back(visited);
        m_trace.push_back(removed);
    }

    Lcg                                   m_rng;
    std::size_t                           m_typeCount;
    std::size_t                           m_timeCount;
    Rocket                                m_rocket;
    std::array<const RocketComponent*, 8> m_sources{};
    EventQueue                            m_queue;
    /// Every event created and not polled.
    std::vector<FlightEvent>  m_live;
    std::vector<std::int64_t> m_trace;
    std::uint64_t             m_counter{0};
};

/// The traces Java printed (probes/events-data-impl/queue.out), one per seed.
[[nodiscard]] const std::vector<JavaTrace>& javaTraces()
{
    static const std::vector<JavaTrace> kTraces{
        // seed 1, 400 operations, 14 types, 6 times: 304 values, 214 events
        JavaTrace{.seed       = 1,
                  .operations = 400,
                  .typeCount  = 14,
                  .timeCount  = 6,
                  .trace = "0 1 323182535 1 5 1 5 224633178 13 998700062 3 477007597 6 "
                           "352040567 0 885241259 0 6 26 742155090 1 5 742304045 0 29 29 "
                           "976113707 2 0 6 29 36 561752219 1 179405685 39 0 6 36 0 8 68205999 "
                           "3 41 44 43 0 6 436583698 2 523222971 48 50 0 8 0 8 46 0 7 37 53 0 6 "
                           "33 435448352 435448352 0 9 1 8 0 12 63 55 0 14 0 14 64 56 934912379 "
                           "60 67 0 14 1 13 825550067 4 634998742 0 0 6 0 6 73 101255972 81 0 8 "
                           "0 12 0 13 1 12 0 13 87 88 521631842 0 19218 113206964 0 7 0 7 91 89 "
                           "896114964 2 100 97 98 98 581913587 96698633 101 125327784 0 6 0 6 0 "
                           "6 102 0 9 596258626 3 166894878 103 464075652 464075652 535115578 0 "
                           "10 370461273 0 15 571958071 7 120 470568199 0 13 122 457226848 123 "
                           "0 14 125 0 13 569937296 4 109 0 8 0 9 0 9 314248083 1 955145533 129 "
                           "0 10 99 1 11 0 18 132 138 136 133 810541375 1 18 0 18 144 0 18 147 "
                           "1 18 146 148 86249175 148 86249175 0 130144234 0 9 660866832 "
                           "703994845 5 279095639 279095639 0 9 0 9 0 9 167 158 825595459 "
                           "380985005 0 11 171 598629876 730111270 0 17 175 176 171 178 179 "
                           "234495367 168 0 16 0 17 686359617 9 534421442 1 9 184 173 181 1 5 0 "
                           "6 792019353 4 0 6 188 0 8 972363703 191 193 1 8 195 196 197 198 0 9 "
                           "200 0 9 0 9 199 197 661380054 192 202 707871890 6 0 6 207 204 "
                           "308116714 0 9 992675790 10 208 206 211 210 194 186 213 209 212 190"},
        // seed 2, 400 operations, 3 types, 2 times: 313 values, 215 events
        JavaTrace{.seed       = 2,
                  .operations = 400,
                  .typeCount  = 3,
                  .timeCount  = 2,
                  .trace = "1 6 1 1 260506112 5 9 1 7 63445107 11 800063415 4 16 14 15 12 1 5 "
                           "21 1 6 290885052 548606982 4 304865318 0 57629968 0 12 57629968 0 "
                           "12 31 508454784 0 13 28 787579654 787399984 3 42 43 0 13 1 12 0 12 "
                           "515487427 45 0 11 37 35 32 40 1 14 0 16 0 17 387752435 58 0 18 "
                           "286858120 4 0 15 0 15 57 1 17 54 529017129 1 1 17 87692802 0 17 1 "
                           "16 1 15 433625286 6 29 973718047 973718047 0 65 0 8 0 12 0 12 72 0 "
                           "11 71 533303874 1 10 0 10 0 10 498574109 2 268126300 0 0 13 75 "
                           "764914784 10 78 434024100 459364696 84 92 0 15 0 18 1 18 658511014 "
                           "95 0 18 91 0 18 101 937290353 2 924496682 11 103 104 97 0 6 106 "
                           "208251277 0 6 0 6 110 105 0 10 111 113 116 84254968 84254968 0 12 "
                           "119 120 114 0 12 285445387 0 1 6 325570144 967620652 967620652 0 11 "
                           "321262184 125 127 128 788323721 0 11 0 12 139 241606051 141 "
                           "802887772 0 360156402 0 6835879 683801246 0 10 153 0 11 162 "
                           "402676589 1 12 167 0 13 293618367 165 0 15 166 155 169 163 170 0 13 "
                           "172 0 15 172 543677595 5 164 449459054 0 8 150538838 176 0 9 "
                           "317837471 6 0 7 0 9 179 180 0 8 0 8 874154046 178 187 0 9 0 9 0 13 "
                           "190 0 13 0 16 0 16 730172137 193 897076617 3 766245805 0 15 0 15 0 "
                           "16 197 198 0 14 188 199 44928131 1 14 905449728 200 203 516980703 6 "
                           "460408294 0 0 10 208 817442078 211 289225596 16 212 213 195 204 205 "
                           "214 189 209 201 207 185 210 192 183 152 173"},
        // seed 3, 400 operations, 2 types, 1 times: 298 values, 221 events
        JavaTrace{.seed       = 3,
                  .operations = 400,
                  .typeCount  = 2,
                  .timeCount  = 1,
                  .trace = "1 5 120097194 2 0 6 4 194987353 194987353 0 0 6 0 7 0 8 0 9 16 "
                           "650397458 544567519 22 23 948470083 862223612 10 0 11 473129428 5 "
                           "269780702 0 9 0 10 24 42 0 9 20 43 0 8 39 683145275 45 108875690 3 "
                           "932739374 201211942 47 0 9 48 37 28 0 6 52 0 8 140777159 0 "
                           "541062187 56 457553101 4 0 10 62 283372215 4 68 66 234183879 72 "
                           "477821461 4 0 11 478394455 5 533884811 0 12 0 13 467184918 4 0 11 0 "
                           "12 0 14 75 916160637 6 95 141074067 141074067 373707989 6 987084306 "
                           "94 0 6 39490523 99 103 23986455 2 92 264571980 0 7 264571980 "
                           "264571980 106 107 100 0 6 307806590 0 458258126 113 94429120 "
                           "889533067 114 0 8 0 8 115 0 8 787228147 0 0 9 0 9 723994045 5 "
                           "603184397 0 8 121 123 1 9 136 590768139 6 995486867 0 10 141 150 0 "
                           "14 140242764 151 0 16 153 140 557853669 149 0 14 154 706699117 4 "
                           "539281704 5 0 7 214018866 0 13 0 14 0 15 668943784 0 20 0 21 1 20 1 "
                           "19 162 168 169 0 22 811256850 175 0 22 170 901712412 3 0 19 173 0 "
                           "19 161 0 18 1 17 0 17 152 630054035 0 17 796287532 796287532 "
                           "796287532 180 181 181 630054035 807460075 7 183 185 155 1 9 0 12 "
                           "69660616 0 16 190 0 16 606466067 0 17 194 186 145 196 143 193 0 14 "
                           "197 560495684 177 198 371678922 2 200 195 202 163 171 914790113 3 "
                           "165 0 6 873880532 0 17 211 0 6 0 7 440916773 440916773 490408648 14 "
                           "214 207 206 209 218 208 212 215 220 219 216 217 210 213"},
        // seed 20261005, 500 operations, 6 types, 3 times: 365 values, 279 events
        JavaTrace{.seed       = 20261005,
                  .operations = 500,
                  .typeCount  = 6,
                  .timeCount  = 3,
                  .trace = "4 1 7 3 1 6 384200208 2 407529626 1 6 0 6 13 414771819 3 17 0 6 21 "
                           "1 9 23 903339573 24 22 0 9 26 20 29 28 252607772 1 9 31 1 8 "
                           "867541186 0 10 1 9 32 16 539129671 4 12 37 38 40 39 41 327887876 "
                           "351238256 3 45 46 171005819 1 8 1 7 52 1 7 0 8 466572995 49 55 60 0 "
                           "12 878773443 6 57 61 62 63 998125590 1 5 66 0 7 68 70 65 72 0 12 "
                           "5930905 183858133 1 12 6109651 1 12 76 78 369271698 0 12 81 85 "
                           "617105224 1 15 1 16 87 0 17 0 17 0 23 1 24 0 24 548781847 91 1 25 "
                           "233169477 0 26 0 26 0 26 102 1 24 100 1 24 103 1 23 116377838 103 "
                           "310477710 0 23 910548236 772043240 5 107 110 107 1 20 111 115 "
                           "844329280 0 24 174207615 1 25 117 117 120 154775332 0 39 128 128 1 "
                           "39 0 42 486044512 138 144 112 84 0 45 1 45 0 45 147 60969299 23 150 "
                           "239697918 24 659336 155 157 156 1 6 153 0 9 0 10 811830195 "
                           "375700819 6 402666856 0 161 996631363 0 8 0 9 0 9 165 171 463136066 "
                           "218450746 0 18 0 21 186 366684080 793051606 192 351739335 175 185 "
                           "375249561 189 179 0 27 0 27 176 174 609103781 13 301426276 0 15 "
                           "550164409 6 0 15 184716762 1 15 201 210 814682165 942607572 212 "
                           "341658488 757083585 2 0 19 216 211 163 205 0 19 221 839323435 223 "
                           "224 283829074 363191445 0 29 0 29 363191445 0 739772 238 0 7 0 9 "
                           "242 0 10 0 10 988695823 7 0 6 41710366 0 7 234 0 8 260264306 0 253 "
                           "668140978 261 0 10 0 12 0 14 264 0 15 0 15 147818771 260 0 15 0 15 "
                           "0 15 265 920782433 0 15 0 15 271 0 15 0 15 326318436 272 273 0 17 0 "
                           "20 254 489211834 19 263 266 270 257 277 256 252 267 269 278 268 258 "
                           "259 255 251 262 276 275 274"},
        // seed 77, 400 operations, 1 types, 1 times: 284 values, 230 events
        JavaTrace{.seed       = 77,
                  .operations = 400,
                  .typeCount  = 1,
                  .timeCount  = 1,
                  .trace = "1 13 1 12 1 11 916777056 12 1 12 16 560859686 7 15 0 575624591 5 1 "
                           "5 419348556 658275620 406544106 22 23 26 5 1 8 24 27 30 28 1 8 0 9 "
                           "472668477 680531163 1 35 0 9 0 11 33 39 1 14 40 197768761 112437355 "
                           "0 17 40 45 1 18 44 1 17 50 0 16 1 15 49 1 13 0 14 316156145 53 54 "
                           "46 0 16 0 16 58 62 63 1 19 788271994 1 19 0 21 1 21 1 21 72 1 25 76 "
                           "397284057 397284057 77 212872377 12 1 16 69 83 67 85 82 0 17 "
                           "735066162 65 0 20 91 595240931 0 95 664388279 97 1 12 590017338 1 "
                           "14 107 0 13 845887908 98 0 13 365601171 102 0 21 115 116 0 20 122 1 "
                           "18 96 424284705 0 22 128 111 238815373 124 0 27 461979794 0 20754 "
                           "137 138 143 145 0 6 141 125572712 148 518787964 0 8 0 8 149 142 147 "
                           "0 7 0 7 153 589688445 0 19 408504911 0 114037084 169 0 6 172 "
                           "172071008 1 174 646099319 166 0 10 181 180 0 14 180 654146843 0 16 "
                           "8738747 8 0 9 0 10 175 191 192 0 9 0 9 178 0 9 0 9 0 12 387772259 "
                           "193 0 12 0 16 201 0 17 194 233650639 203 326540017 7 266791134 "
                           "266791134 214 215 205 566831863 7 0 14 0 15 701732112 212 222 "
                           "921486355 216 0 18 0 18 343758988 219 0 19 220 874901867 20 229 228 "
                           "189 177 213 226 204 223 200 209 208 217 197 218 221 227 184 196 224 "
                           "225"},
        // seed 123456789, 600 operations, 14 types, 2 times: 444 values, 335 events
        JavaTrace{.seed       = 123456789,
                  .operations = 600,
                  .typeCount  = 14,
                  .timeCount  = 2,
                  .trace = "4 1 6 5 1 7 10 0 7 11 791041485 13 1 7 9 0 16 17 6 18 560086242 2 0 "
                           "6 1 5 502089115 714621446 0 7 22 0 8 27 0 10 1 10 1 9 603506334 2 "
                           "359367507 1 9 31 32 21 0 6 33 1 5 0 7 574502474 0 7 780949858 "
                           "780949858 0 8 484339969 264132349 1 1 10 1 9 40 41 26138222 43 1 9 "
                           "0 12 44 185262567 1 14 49 55 1 16 0 16 809360388 50 37 46 47 1 12 "
                           "882993512 882993512 0 952153320 3 66670075 5 550555342 550555342 "
                           "764344998 0 12 0 14 1 14 0 15 66 542581119 83 76 527556655 75 1 18 "
                           "89 43116076 0 20 971338556 6 723738901 8 77 86 190593474 96 0 6 71 "
                           "0 6 0 6 98 447168216 4 72775691 2 0 7 0 7 0 7 258075959 100 "
                           "417149475 3 922108255 155347928 4 384428101 3 0 8 870595365 7 "
                           "624548 0 7 0 7 907775282 118 695845479 0 128 129 1 5 605150567 132 "
                           "0 6 0 6 123 0 6 277025589 136 0 11 85056861 5 707310602 139 127 142 "
                           "143 0 14 0 15 0 15 147 141 651448275 0 13 651448275 0 0 6 0 7 0 7 0 "
                           "7 153 381459209 0 21616 0 13 175 753267083 6 174 170 461077238 "
                           "461077238 0 7 164446310 2 690801262 0 183 0 6 756832588 3 182 "
                           "668789911 190 191 195 0 12 200 546558131 4 194 198 360549385 0 10 "
                           "203 0 10 0 12 0 13 70089973 6 210 208 206 0 6 253616900 0 7 213 212 "
                           "214 0 6 215 216 217 218 566754947 5 696674477 912140647 3 564847211 "
                           "3 0 6 230 219 29933746 1 225 0 6 833687120 2 22724193 0 6 236 0 7 "
                           "241 240 0 11 240 0 10 197717327 8 23064150 247 0 8 747638288 0 0 6 "
                           "692297022 2 750659542 0 6 0 6 0 6 1 5 0 6 258 0 11 0 12 263 267 "
                           "599426430 270 259 0 11 0 13 272 1 13 433098063 301879436 301879436 "
                           "0 21 285 282 283 0 26 280 0 26 197720977 274 271 868822328 0 34 0 "
                           "36 304 155666203 10 441711817 743682514 11 970457903 4 501092570 6 "
                           "0 10 308 0 9 0 9 311 310 309 0 10 313 315 686949781 0 12 881735177 "
                           "3 316 305 67470518 0 9 314 0 11 821879651 5 0 7 720771341 720771341 "
                           "0 9 0 9 0 11 325 328 315610764 577024937 6 330 0 9 0 9 735233676 9 "
                           "324 322 332 320 260 284 326 296 334"},
    };
    return kTraces;
}

class EventQueueAgainstJava : public ::testing::TestWithParam<std::size_t>
{ };

TEST_P(EventQueueAgainstJava, TheScriptedSequenceGivesJavasTrace)
{
    const JavaTrace&                java = javaTraces().at(GetParam());
    QueueScript                     script(java.seed, java.typeCount, java.timeCount);
    const std::vector<std::int64_t> expected = parseTrace(java.trace);
    EXPECT_GT(expected.size(), 250U);
    EXPECT_EQ(script.run(java.operations), expected) << "seed " << java.seed;
    EXPECT_GT(script.eventCount(), 200U);
}

INSTANTIATE_TEST_SUITE_P(Seeds, EventQueueAgainstJava, ::testing::Range<std::size_t>(0, 6));

TEST(EventQueue, TheJavaTracesCoverSixSeeds)
{
    EXPECT_EQ(javaTraces().size(), 6U);
}

// ============================================================================ remove()

TEST(EventQueue, RemoveTakesTheEventWithThatIdOut)
{
    EventQueue        queue;
    const FlightEvent first  = labelled(Type::ALTITUDE, 1.0, nullptr, 1);
    const FlightEvent second = labelled(Type::ALTITUDE, 1.0, nullptr, 2);
    const FlightEvent third  = labelled(Type::ALTITUDE, 1.0, nullptr, 3);
    queue.add(first);
    queue.add(second);
    queue.add(third);

    EXPECT_TRUE(queue.contains(second));
    EXPECT_TRUE(queue.remove(second));
    EXPECT_FALSE(queue.contains(second));
    EXPECT_FALSE(queue.remove(second)) << "it is gone";
    EXPECT_EQ(queue.size(), 2U);
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{1, 3}));
}

TEST(EventQueue, RemoveComparesTheIdNotTheContents)
{
    EventQueue queue;
    queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, 1));
    // The same type, time and source, and so equal by compareTo() and by equals(): another event.
    const FlightEvent twin = labelled(Type::ALTITUDE, 1.0, nullptr, 2);
    EXPECT_FALSE(queue.contains(twin));
    EXPECT_FALSE(queue.remove(twin));
    EXPECT_EQ(queue.size(), 1U);

    // A copy of a queued event is that event.
    const FlightEvent copy = labelled(Type::ALTITUDE, 1.0, nullptr, 1);
    EXPECT_EQ(peekLabel(queue), 1);
    EXPECT_TRUE(queue.contains(copy));
    EXPECT_TRUE(queue.remove(copy));
    EXPECT_TRUE(queue.empty());
}

TEST(EventQueue, RemoveOfTheLastArraySlotMovesNothing)
{
    EventQueue queue;
    for (std::uint64_t label = 0; label < 5; label++)
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(label), nullptr, label));
    }
    ASSERT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 1, 2, 3, 4}));
    EXPECT_TRUE(queue.remove(labelled(Type::ALTITUDE, 4.0, nullptr, 4)));
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 1, 2, 3}));
}

TEST(EventQueue, RemoveSiftsTheLastEventUpWhenItBelongsAboveTheGap)
{
    // The heap 0; 10, 1; 11, 12, 2, 3: removing 11 (slot 3) puts 3 (the last) there, which is
    // less than its new parent 10 and moves up.
    EventQueue queue;
    for (const std::uint64_t time : {0U, 10U, 1U, 11U, 12U, 2U, 3U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    ASSERT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 10, 1, 11, 12, 2, 3}));
    EXPECT_TRUE(queue.remove(labelled(Type::ALTITUDE, 11.0, nullptr, 11)));
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 3, 1, 10, 12, 2}));
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{0, 1, 2, 3, 10, 12}));
}

TEST(EventQueue, RemoveSiftsTheLastEventDownWhenItBelongsBelowTheGap)
{
    EventQueue queue;
    for (const std::uint64_t time : {0U, 1U, 2U, 3U, 4U, 5U, 6U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    ASSERT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 1, 2, 3, 4, 5, 6}));
    EXPECT_TRUE(queue.remove(labelled(Type::ALTITUDE, 0.0, nullptr, 0)));
    // 6 goes to the root and sinks below 1 and 3.
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{1, 3, 2, 6, 4, 5}));
}

// ============================================================================ Iterator

TEST(EventQueueIterator, VisitsTheEventsInArrayOrder)
{
    EventQueue queue;
    for (const std::uint64_t time : {5U, 3U, 4U, 1U, 2U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    std::vector<std::int64_t> visited;
    EventQueue::Iterator      iterator = queue.iterator();
    while (iterator.hasNext())
    {
        visited.push_back(labelOf(iterator.next()));
    }
    EXPECT_EQ(visited, arrayOrder(queue));
    EXPECT_EQ(visited, (std::vector<std::int64_t>{1, 2, 4, 5, 3}));
}

TEST(EventQueueIterator, RemoveTakesTheLastReturnedEventOut)
{
    EventQueue queue;
    for (const std::uint64_t time : {0U, 1U, 2U, 3U, 4U, 5U, 6U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    EventQueue::Iterator iterator = queue.iterator();
    EXPECT_EQ(labelOf(iterator.next()), 0);
    EXPECT_EQ(labelOf(iterator.next()), 1);
    iterator.remove();
    // 6, the last, went to slot 1 and sank below 3: the iterator goes on at slot 1.
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 3, 2, 6, 4, 5}));
    EXPECT_EQ(labelOf(iterator.next()), 3);
    EXPECT_EQ(queue.size(), 6U);
}

/// Visits every event of @p queue and removes, through the iterator, those whose label is in
/// @p remove; returns the labels in the order they were visited.
[[nodiscard]] std::vector<std::int64_t> visitRemoving(EventQueue&                   queue,
                                                      std::span<const std::int64_t> remove)
{
    std::vector<std::int64_t> visited;
    EventQueue::Iterator      iterator = queue.iterator();
    while (iterator.hasNext())
    {
        const std::int64_t label = labelOf(iterator.next());
        visited.push_back(label);
        for (const std::int64_t candidate : remove)
        {
            if (candidate == label)
            {
                iterator.remove();
            }
        }
    }
    return visited;
}

TEST(EventQueueIterator, AnEventMovedIntoTheVisitedPartIsVisitedAtTheEnd)
{
    // The heap 0; 10, 1; 11, 12, 2, 3: removing 11 at slot 3 moves 3, which the iterator has
    // not seen, up to slot 1, which it has passed. It is visited after the others.
    EventQueue queue;
    for (const std::uint64_t time : {0U, 10U, 1U, 11U, 12U, 2U, 3U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    const std::array<std::int64_t, 1> remove{11};
    EXPECT_EQ(visitRemoving(queue, remove), (std::vector<std::int64_t>{0, 10, 1, 11, 12, 2, 3}));
    EXPECT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 3, 1, 10, 12, 2}));
}

TEST(EventQueueIterator, AnEventVisitedAtTheEndCanBeRemovedToo)
{
    EventQueue queue;
    for (const std::uint64_t time : {0U, 10U, 1U, 11U, 12U, 2U, 3U})
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>(time), nullptr, time));
    }
    const std::array<std::int64_t, 2> remove{11, 3};
    EXPECT_EQ(visitRemoving(queue, remove), (std::vector<std::int64_t>{0, 10, 1, 11, 12, 2, 3}));
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{0, 1, 2, 10, 12}));
}

TEST(EventQueueIterator, RemovingEveryEventEmptiesTheQueue)
{
    EventQueue                queue;
    std::vector<std::int64_t> all;
    for (std::uint64_t label = 0; label < 40; label++)
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>((label * 7) % 5), nullptr, label));
        all.push_back(static_cast<std::int64_t>(label));
    }
    EXPECT_EQ(visitRemoving(queue, all).size(), 40U);
    EXPECT_TRUE(queue.empty());
}

TEST(EventQueueIterator, MisuseIsABug)
{
    EventQueue queue;
    queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, 1));
    queue.add(labelled(Type::ALTITUDE, 2.0, nullptr, 2));

    EventQueue::Iterator fresh = queue.iterator();
    EXPECT_THROW(fresh.remove(), BugError) << "no next() yet";
    EXPECT_EQ(labelOf(fresh.next()), 1);
    fresh.remove();
    EXPECT_THROW(fresh.remove(), BugError) << "the event is removed already";
    EXPECT_EQ(labelOf(fresh.next()), 2);
    EXPECT_FALSE(fresh.hasNext());
    EXPECT_THROW(static_cast<void>(fresh.next()), BugError) << "at the end";
}

TEST(EventQueueIterator, AChangeBehindTheIteratorIsABug)
{
    EventQueue queue;
    queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, 1));
    queue.add(labelled(Type::ALTITUDE, 2.0, nullptr, 2));

    EventQueue::Iterator added = queue.iterator();
    queue.add(labelled(Type::ALTITUDE, 3.0, nullptr, 3));
    EXPECT_THROW(static_cast<void>(added.next()), BugError);

    EventQueue::Iterator polled = queue.iterator();
    static_cast<void>(polled.next());
    static_cast<void>(queue.poll());
    EXPECT_THROW(polled.remove(), BugError);

    EventQueue::Iterator cleared = queue.iterator();
    queue.clear();
    EXPECT_THROW(static_cast<void>(cleared.next()), BugError);
}

// ============================================================================ copies

/// A queue of twenty events, labelled 0 to 19, at four different times.
[[nodiscard]] EventQueue queueOfTwenty()
{
    EventQueue queue;
    for (std::uint64_t label = 0; label < 20; label++)
    {
        queue.add(labelled(Type::ALTITUDE, static_cast<double>((label * 3) % 4), nullptr, label));
    }
    return queue;
}

TEST(EventQueue, ACopyHasTheSameArrayAndAnInvalidModId)
{
    const EventQueue queue  = queueOfTwenty();
    const ModId      before = queue.modId();
    EXPECT_NE(before, ModId::invalid());

    EventQueue copy(queue);
    EXPECT_EQ(arrayOrder(copy), arrayOrder(queue));
    EXPECT_EQ(copy.modId(), ModId::invalid()) << "Java: EventQueue(PriorityQueue)";
    EXPECT_EQ(queue.modId(), before);

    // The copy is its own queue.
    static_cast<void>(copy.poll());
    EXPECT_EQ(copy.size(), 19U);
    EXPECT_EQ(queue.size(), 20U);
    EXPECT_EQ(drain(copy).size(), 19U);
}

TEST(EventQueue, AddAllIntoAnEmptyQueueReproducesTheArray)
{
    const EventQueue queue = queueOfTwenty();
    EventQueue       other;
    EXPECT_TRUE(other.addAll(queue));
    EXPECT_EQ(arrayOrder(other), arrayOrder(queue));
    EXPECT_NE(other.modId(), ModId::invalid());

    EventQueue empty;
    EXPECT_FALSE(other.addAll(empty)) << "nothing to add";
    EXPECT_THROW(other.addAll(other), BugError) << "Java: IllegalArgumentException";
}

TEST(EventQueue, AssignmentTakesTheEventsAndDrawsANewModId)
{
    EventQueue source;
    source.add(labelled(Type::ALTITUDE, 1.0, nullptr, 1));
    source.add(labelled(Type::ALTITUDE, 0.5, nullptr, 2));

    EventQueue target;
    target.add(labelled(Type::ALTITUDE, 9.0, nullptr, 9));
    const ModId before = target.modId();
    target             = source;
    EXPECT_EQ(arrayOrder(target), arrayOrder(source));
    EXPECT_GT(target.modId(), before);

    EventQueue  moved;
    const ModId beforeMove = moved.modId();
    moved                  = std::move(target);
    EXPECT_EQ(arrayOrder(moved), (std::vector<std::int64_t>{2, 1}));
    EXPECT_GT(moved.modId(), beforeMove);

    EventQueue& self = moved;
    moved            = self;
    EXPECT_EQ(arrayOrder(moved), (std::vector<std::int64_t>{2, 1})) << "self-assignment";
}

// ============================================================================ modId()

TEST(EventQueue, TheModIdChangesWhereJavasDoes)
{
    // Java: MiscProbe.java, "EventQueue".
    EventQueue queue;
    EXPECT_EQ(queue.modId(), ModId::invalid());
    const FlightEvent a = labelled(Type::LAUNCH, 1.0, nullptr, 1);
    const FlightEvent b = labelled(Type::LAUNCH, 2.0, nullptr, 2);
    const FlightEvent c = labelled(Type::LAUNCH, 3.0, nullptr, 3);

    ModId id = queue.modId();
    queue.add(a);
    EXPECT_GT(queue.modId(), id) << "add()";
    id = queue.modId();
    queue.offer(b);
    EXPECT_GT(queue.modId(), id) << "offer()";
    id = queue.modId();
    static_cast<void>(queue.peek());
    static_cast<void>(queue.contains(a));
    static_cast<void>(queue.size());
    static_cast<void>(queue.toString());
    EXPECT_EQ(queue.modId(), id) << "the queries";
    EXPECT_FALSE(queue.remove(c));
    EXPECT_GT(queue.modId(), id) << "remove() of an event that is not there";
    id = queue.modId();

    EventQueue::Iterator iterator = queue.iterator();
    static_cast<void>(iterator.next());
    iterator.remove();
    EXPECT_EQ(queue.size(), 1U);
    EXPECT_EQ(queue.modId(), id) << "Iterator::remove() draws no id";

    ASSERT_TRUE(queue.poll().has_value());
    EXPECT_GT(queue.modId(), id) << "poll()";
    id = queue.modId();
    EXPECT_FALSE(queue.poll().has_value());
    EXPECT_GT(queue.modId(), id) << "poll() of an empty queue";
    id = queue.modId();
    queue.clear();
    EXPECT_GT(queue.modId(), id) << "clear() of an empty queue";
}

// ============================================================================ the rest

TEST(EventQueue, ToStringListsTheEventsInArrayOrder)
{
    EventQueue queue;
    queue.add(FlightEvent{Type::LAUNCH, 3.0});
    queue.add(FlightEvent{Type::LAUNCH, 1.0});
    EXPECT_EQ(queue.toString(),
              "[FlightEvent[type=LAUNCH,time=1.0,source=null,data=null], "
              "FlightEvent[type=LAUNCH,time=3.0,source=null,data=null]]");
}

TEST(EventQueue, AFailingComparisonLeavesTheQueueAsItWas)
{
    const EventTestRocket r;
    const BodyTube        detached(0.1, 0.01);
    EventQueue            queue;
    // A source that is in no stage cannot be compared with a source that is, at the same time.
    // The events are placed so that no comparison of the two is needed while they are added
    // (Java: QueueProbe2.java, "stray").
    queue.add(labelled(Type::ALTITUDE, 0.0, nullptr, 0));
    queue.add(labelled(Type::ALTITUDE, 0.5, nullptr, 1));
    queue.add(labelled(Type::LAUNCH, 1.0, &detached, 2));
    queue.add(labelled(Type::ALTITUDE, 0.75, nullptr, 3));
    queue.add(labelled(Type::LAUNCH, 1.0, r.boosterBody, 4));
    ASSERT_EQ(arrayOrder(queue), (std::vector<std::int64_t>{0, 1, 2, 3, 4}));
    ASSERT_EQ(pollLabel(queue), 0);
    ASSERT_EQ(pollLabel(queue), 1);
    const std::vector<std::int64_t> before{3, 4, 2};
    ASSERT_EQ(arrayOrder(queue), before);

    // poll(): the last event has to be compared with the other one at 1.0 s on its way down.
    EXPECT_THROW(queue.poll(), BugError);
    EXPECT_EQ(arrayOrder(queue), before) << "Java loses the last event here";

    // remove() of the first event: the same comparison.
    EXPECT_THROW(queue.remove(labelled(Type::ALTITUDE, 0.75, nullptr, 3)), BugError);
    EXPECT_EQ(arrayOrder(queue), before);

    // Iterator::remove(): the same.
    EventQueue::Iterator iterator = queue.iterator();
    EXPECT_EQ(labelOf(iterator.next()), 3);
    EXPECT_THROW(iterator.remove(), BugError);
    EXPECT_EQ(arrayOrder(queue), before);

    // add(): a new event at 1.0 s whose parent in the heap is the stray one.
    queue.add(labelled(Type::ALTITUDE, 2.0, nullptr, 5));
    queue.add(labelled(Type::ALTITUDE, 3.0, nullptr, 6));
    const std::vector<std::int64_t> grown{3, 4, 2, 5, 6};
    ASSERT_EQ(arrayOrder(queue), grown);
    EXPECT_THROW(queue.add(labelled(Type::LAUNCH, 1.0, r.boosterBody, 7)), BugError);
    EXPECT_EQ(arrayOrder(queue), grown);
    EXPECT_THROW(queue.offer(labelled(Type::LAUNCH, 1.0, r.boosterBody, 7)), BugError);
    EXPECT_EQ(arrayOrder(queue), grown);

    // Without the stray event the queue works on.
    EXPECT_TRUE(queue.remove(labelled(Type::LAUNCH, 1.0, &detached, 2)));
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{3, 4, 5, 6}));
}

TEST(EventQueue, TiesAcrossAddsAndPollsFollowTheHeap)
{
    // Java (QueueProbe2.java, "interleaved ties"): three equal events added, one polled, four
    // times over.
    EventQueue                queue;
    std::vector<std::int64_t> polled;
    std::uint64_t             label = 0;
    for (int round = 0; round < 4; round++)
    {
        queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, label));
        queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, label + 1));
        queue.add(labelled(Type::ALTITUDE, 1.0, nullptr, label + 2));
        label += 3;
        polled.push_back(pollLabel(queue));
    }
    EXPECT_EQ(polled, (std::vector<std::int64_t>{0, 2, 5, 8}));
    EXPECT_EQ(drain(queue), (std::vector<std::int64_t>{11, 10, 9, 7, 6, 4, 3, 1}));
}

}  // namespace
