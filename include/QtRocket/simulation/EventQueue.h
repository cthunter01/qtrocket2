#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// The queue of the flight events a simulation has still to handle, ordered by
/// FlightEvent::compareTo() (OpenRocket's simulation/EventQueue, a java.util.PriorityQueue with
/// a modification id).
///
/// The order of events that compare equal: the simulation depends on it (two motors of one
/// stage that ignite at the same time, an altitude event and a deployment at the same instant),
/// and it is neither the order of insertion nor arbitrary: it is what the binary heap of JDK 17's
/// PriorityQueue does. This class is that algorithm, operation for operation: the array (in
/// which the children of element k are 2k + 1 and 2k + 2), offer() with siftUp, poll() with
/// siftDown of the last element, removal with removeAt() (siftDown of the last element into the
/// gap and, when it stays there, siftUp), a copy that takes the array as it is and heapifies it,
/// and the iterator with its remove(). The polled order is pinned against Java in
/// EventQueueTests.
///
/// Identity: Java's remove(Object) and contains(Object) compare with Object.equals(), which
/// FlightEvent does not override, so they find the very object (see FlightEvent, "Equality").
/// Here events are values and the same event is the one with the same id
/// (FlightEvent::sameEvent()).
///
/// Monitoring (Java: Monitorable; the queue satisfies the Monitorable concept): modId() is
/// ModId::invalid() until the first change and is redrawn exactly where Java redraws it: in
/// add(), offer(), addAll() (per event), poll(), remove(event) and clear(), each whether or not
/// the queue changes (poll() of an empty queue, remove() of an event that is not there), and
/// not in Iterator::remove(), which takes an event out without a new id. A copy starts at
/// ModId::invalid() again, as Java's EventQueue(PriorityQueue) does.
///
/// Sources: compareTo() reads the stage number of the events' source components, so the
/// components must be alive, and must keep their stage numbers, while their events are queued.
///
/// Deviations from OpenRocket:
/// - add() and offer() return nothing (Java: always true), and there is no null event.
/// - peek() gives a pointer into the queue and poll() and Iterator::next() give copies, where
///   Java hands out the object.
/// - Programming errors throw BugError: an Iterator used after the queue changed behind its
///   back (Java: ConcurrentModificationException), next() at the end (NoSuchElementException),
///   remove() without a next() (IllegalStateException), a queue added to itself
///   (IllegalArgumentException).
/// - When compareTo() throws (a source that is in no stage), the queue is left as it was: every
///   comparison of an operation is made before anything moves. Java leaves the heap half
///   changed.
/// - Assigning a queue draws a new modification id for the assigned-to queue (Java has no
///   assignment; an id must not go backwards).
/// - Not ported: the capacity and comparator constructors, comparator(), toArray(), element(),
///   remove() without an argument, the bulk operations (removeAll(), retainAll(), removeIf(),
///   forEach()) and the protected clone().
class EventQueue
{
public:
    /// Iterates the events in array order, which is not the order they are polled in (Java: the
    /// order of the for-each loop). Invalidated by every change of the queue.
    using ConstIterator = std::vector<FlightEvent>::const_iterator;

    /// The iterator of Java's PriorityQueue (java.util.PriorityQueue.Itr), for the callers that
    /// remove events while they iterate, as SimulationStatus.removeUnattachedEvents() does: it
    /// visits the events in array order and, when a remove() has moved an event that was still
    /// to come into the part already visited, visits that event after all the others.
    ///
    /// The queue must outlive the iterator and must not change while it is in use, other than
    /// through remove(): every member then throws BugError.
    class Iterator
    {
    public:
        /// Whether next() has another event to give.
        [[nodiscard]] bool hasNext() const noexcept;

        /// The next event (a copy).
        /// @throws BugError at the end, or when the queue was changed behind the iterator
        FlightEvent next();

        /// Takes the event next() gave last out of the queue, without a new modification id.
        /// @throws BugError without a next() before it, or when the queue was changed behind
        ///         the iterator
        void remove();

    private:
        friend class EventQueue;
        explicit Iterator(EventQueue& queue) noexcept;

        void checkForComodification() const;

        EventQueue* m_queue;
        /// Index of the event the next next() returns.
        std::size_t m_cursor{0};
        /// Index of the event next() returned last, unless it came from m_forgetMeNot; none
        /// after a remove() (Java: -1).
        std::optional<std::size_t> m_lastRet;
        /// The events a remove() moved from the unvisited part of the heap into the visited
        /// part (a removal that needed a siftUp); they are visited at the end.
        std::deque<FlightEvent> m_forgetMeNot;
        /// The event next() returned last, when it came from m_forgetMeNot.
        std::optional<FlightEvent> m_lastRetElt;
        std::uint64_t              m_expectedModCount;
    };

    /// An empty queue.
    EventQueue() = default;

    /// A queue with the events of @p other in the same array order (Java:
    /// EventQueue(PriorityQueue), which copies the array and heapifies it: nothing moves in an
    /// array that is a heap already). The modification id starts at ModId::invalid().
    EventQueue(const EventQueue& other);
    EventQueue(EventQueue&& other) noexcept = default;

    /// Replaces the events by a copy of @p other's, as the copy constructor makes it, and draws
    /// a new modification id.
    EventQueue& operator=(const EventQueue& other);
    /// Takes the events of @p other and draws a new modification id.
    EventQueue& operator=(EventQueue&& other) noexcept;
    ~EventQueue() = default;

    /// Adds @p event (Java: add(), which is offer()).
    /// @throws BugError as FlightEvent::compareTo()
    void add(FlightEvent event);

    /// Adds @p event: it goes to the end of the array and moves up while it compares less than
    /// its parent, so it stays below every queued event it compares equal to on that path.
    /// @throws BugError as FlightEvent::compareTo()
    void offer(FlightEvent event);

    /// add() of every event of @p other, in its array order (Java: AbstractQueue.addAll(), what
    /// the copy constructor of SimulationStatus fills the queue of a new branch with). Into an
    /// empty queue that reproduces @p other's array. True when @p other has events.
    /// @throws BugError when @p other is this queue, or as FlightEvent::compareTo()
    bool addAll(const EventQueue& other);

    /// Takes the first event out: the least by compareTo() (Java: poll()). nullopt when the
    /// queue is empty.
    /// @throws BugError as FlightEvent::compareTo()
    std::optional<FlightEvent> poll();

    /// The event poll() would return, or null when the queue is empty. The pointer is valid
    /// until the queue changes.
    [[nodiscard]] const FlightEvent* peek() const noexcept;

    /// Takes @p event (the first event in array order with its id) out of the queue. True when
    /// it was there.
    /// @throws BugError as FlightEvent::compareTo()
    bool remove(const FlightEvent& event);

    /// Whether @p event (an event with its id) is queued.
    [[nodiscard]] bool contains(const FlightEvent& event) const noexcept;

    /// Removes every event.
    void clear() noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return m_events.size(); }
    /// Java: isEmpty().
    [[nodiscard]] bool empty() const noexcept { return m_events.empty(); }

    [[nodiscard]] ConstIterator begin() const noexcept { return m_events.cbegin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return m_events.cend(); }

    /// An iterator that can remove events (Java: iterator()); see Iterator.
    [[nodiscard]] Iterator iterator() noexcept { return Iterator{*this}; }

    /// The modification id (Java: getModID()); see the class comment.
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// "[<event>, <event>, ...]" with FlightEvent::toString(), in array order (Java:
    /// AbstractCollection.toString()).
    [[nodiscard]] std::string toString() const;

private:
    /// The index of the first event with the id of @p event (Java: indexOf()).
    [[nodiscard]] std::optional<std::size_t> indexOf(const FlightEvent& event) const noexcept;

    /// Where siftUp(@p k, @p key) puts @p key: the comparisons of JDK's siftUpComparable(),
    /// without moving anything. @p k may be size() (the slot offer() is about to add).
    [[nodiscard]] std::size_t siftUpTarget(std::size_t k, const FlightEvent& key) const;

    /// The children siftDown(@p k, @p key) over the first @p n events moves up, in order: the
    /// comparisons of JDK's siftDownComparable(), without moving anything. Empty when @p key
    /// stays at @p k.
    [[nodiscard]] std::vector<std::size_t> siftDownPath(std::size_t k, const FlightEvent& key,
                                                        std::size_t n) const;

    /// Moves the parents between @p k and @p target one level down and puts @p key at @p target.
    void placeUp(std::size_t k, std::size_t target, FlightEvent key) noexcept;

    /// Moves the children of @p path one level up, starting at @p k, and puts @p key at the
    /// last of them (at @p k for an empty path).
    void placeDown(std::size_t k, const std::vector<std::size_t>& path, FlightEvent key) noexcept;

    /// Removes the event at @p i (Java: removeAt()): the last event takes its place, sifted
    /// down and, when it does not move down, sifted up. Returns the last event when it ended
    /// before @p i (an iterator at @p i has then not visited it), else nullopt.
    std::optional<FlightEvent> removeAt(std::size_t i);

    /// Takes the last event out of the array and counts the change; the queue is not empty.
    [[nodiscard]] FlightEvent takeLast() noexcept;

    /// Establishes the heap order over the whole array (Java: heapify()).
    void heapify();

    /// The heap (Java: queue and size): the events in array order.
    std::vector<FlightEvent> m_events;
    ModId                    m_modId{ModId::invalid()};
    /// The number of structural changes (Java: modCount), which an Iterator watches.
    std::uint64_t m_modCount{0};
};

}  // namespace QtRocket
