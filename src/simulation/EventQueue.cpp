#include "QtRocket/simulation/EventQueue.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

// ================================================================================== Iterator

EventQueue::Iterator::Iterator(EventQueue& queue) noexcept
  : m_queue(&queue), m_expectedModCount(queue.m_modCount)
{
}

void EventQueue::Iterator::checkForComodification() const
{
    if (m_expectedModCount != m_queue->m_modCount)
    {
        bug("The event queue was modified while it was being iterated");
    }
}

bool EventQueue::Iterator::hasNext() const noexcept
{
    return m_cursor < m_queue->m_events.size() || m_forgetMeNotNext < m_forgetMeNot.size();
}

FlightEvent EventQueue::Iterator::next()
{
    checkForComodification();
    if (m_cursor < m_queue->m_events.size())
    {
        m_lastRet = m_cursor++;
        return m_queue->m_events[*m_lastRet];
    }
    // Java: `if (forgetMeNot != null)`, which forgets the last returned event even when the
    // deque has run empty.
    if (!m_forgetMeNot.empty())
    {
        m_lastRet.reset();
        m_lastRetElt.reset();
        if (m_forgetMeNotNext < m_forgetMeNot.size())
        {
            m_lastRetElt = std::move(m_forgetMeNot[m_forgetMeNotNext]);
            m_forgetMeNotNext++;
            return *m_lastRetElt;
        }
    }
    bug("The event queue has no more events");
}

void EventQueue::Iterator::remove()
{
    checkForComodification();
    if (m_lastRet.has_value())
    {
        std::optional<FlightEvent> moved = m_queue->removeAt(*m_lastRet);
        m_lastRet.reset();
        if (!moved.has_value())
        {
            m_cursor--;
        }
        else
        {
            m_forgetMeNot.push_back(std::move(*moved));
        }
    }
    else if (m_lastRetElt.has_value())
    {
        // Java: removeEq(), the first slot that holds this very event.
        if (const std::optional<std::size_t> index = m_queue->indexOf(*m_lastRetElt))
        {
            m_queue->removeAt(*index);
        }
        m_lastRetElt.reset();
    }
    else
    {
        bug("Iterator::remove() needs a next() before it");
    }
    m_expectedModCount = m_queue->m_modCount;
}

// ================================================================================ EventQueue

EventQueue::EventQueue(const EventQueue& other) : m_events(other.m_events)
{
    heapify();
}

EventQueue& EventQueue::operator=(const EventQueue& other)
{
    if (this != &other)
    {
        EventQueue copy(other);
        m_events = std::move(copy.m_events);
        ++m_modCount;
        m_modId = ModId{};
    }
    return *this;
}

EventQueue& EventQueue::operator=(EventQueue&& other) noexcept
{
    if (this != &other)
    {
        m_events = std::move(other.m_events);
        ++m_modCount;
        m_modId = ModId{};
    }
    return *this;
}

void EventQueue::add(FlightEvent event)
{
    // Java draws one id in add() and another in the offer() it calls.
    m_modId = ModId{};
    offer(std::move(event));
}

void EventQueue::offer(FlightEvent event)
{
    m_modId                  = ModId{};
    const std::size_t i      = m_events.size();
    const std::size_t target = siftUpTarget(i, event);
    // Nothing throws once the array has grown: FlightEvent moves are noexcept.
    m_events.push_back(std::move(event));
    ++m_modCount;
    if (target != i)
    {
        FlightEvent key = std::move(m_events[i]);
        placeUp(i, target, std::move(key));
    }
}

bool EventQueue::addAll(const EventQueue& other)
{
    if (this == &other)
    {
        bug("An event queue cannot be added to itself");
    }
    bool modified = false;
    for (const FlightEvent& event : other.m_events)
    {
        add(event);
        modified = true;
    }
    return modified;
}

std::optional<FlightEvent> EventQueue::poll()
{
    m_modId = ModId{};
    if (m_events.empty())
    {
        return std::nullopt;
    }
    const std::size_t        n = m_events.size() - 1;
    std::vector<std::size_t> path;
    if (n > 0)
    {
        path = siftDownPath(0, m_events.back(), n);
    }
    // Nothing throws from here on.
    FlightEvent last = takeLast();
    if (n == 0)
    {
        return last;  // it was the only event
    }
    FlightEvent result = std::move(m_events.front());
    placeDown(0, path, std::move(last));
    return result;
}

const FlightEvent* EventQueue::peek() const noexcept
{
    return m_events.empty() ? nullptr : &m_events.front();
}

bool EventQueue::remove(const FlightEvent& event)
{
    m_modId                                = ModId{};
    const std::optional<std::size_t> index = indexOf(event);
    if (!index.has_value())
    {
        return false;
    }
    removeAt(*index);
    return true;
}

bool EventQueue::contains(const FlightEvent& event) const noexcept
{
    return indexOf(event).has_value();
}

void EventQueue::clear() noexcept
{
    m_modId = ModId{};
    ++m_modCount;
    m_events.clear();
}

std::string EventQueue::toString() const
{
    std::string text  = "[";
    bool        first = true;
    for (const FlightEvent& event : m_events)
    {
        if (!first)
        {
            text += ", ";
        }
        first = false;
        text += event.toString();
    }
    text += ']';
    return text;
}

std::optional<std::size_t> EventQueue::indexOf(const FlightEvent& event) const noexcept
{
    for (std::size_t i = 0; i < m_events.size(); i++)
    {
        if (event.sameEvent(m_events[i]))
        {
            return i;
        }
    }
    return std::nullopt;
}

std::size_t EventQueue::siftUpTarget(std::size_t k, const FlightEvent& key) const
{
    while (k > 0)
    {
        const std::size_t parent = (k - 1) >> 1U;
        if (key.compareTo(m_events[parent]) >= 0)
        {
            break;
        }
        k = parent;
    }
    return k;
}

std::vector<std::size_t> EventQueue::siftDownPath(std::size_t k, const FlightEvent& key,
                                                  std::size_t n) const
{
    std::vector<std::size_t> path;
    const std::size_t        half = n >> 1U;  // loop while a non-leaf
    while (k < half)
    {
        std::size_t       child = (k << 1U) + 1;  // assume left child is least
        const std::size_t right = child + 1;
        if (right < n && m_events[child].compareTo(m_events[right]) > 0)
        {
            child = right;
        }
        if (key.compareTo(m_events[child]) <= 0)
        {
            break;
        }
        path.push_back(child);
        k = child;
    }
    return path;
}

void EventQueue::placeUp(std::size_t k, std::size_t target, FlightEvent key) noexcept
{
    while (k > target)
    {
        const std::size_t parent = (k - 1) >> 1U;
        m_events[k]              = std::move(m_events[parent]);
        k                        = parent;
    }
    m_events[k] = std::move(key);
}

void EventQueue::placeDown(std::size_t k, const std::vector<std::size_t>& path,
                           FlightEvent key) noexcept
{
    for (const std::size_t child : path)
    {
        m_events[k] = std::move(m_events[child]);
        k           = child;
    }
    m_events[k] = std::move(key);
}

std::optional<FlightEvent> EventQueue::removeAt(std::size_t i)
{
    const std::size_t s = m_events.size() - 1;
    if (s == i)  // removed last element
    {
        static_cast<void>(takeLast());
        return std::nullopt;
    }

    // Java: siftDown(i, moved), and when moved stays at i, siftUp(i, moved). Every comparison
    // is made before anything moves; none of them looks at slot i or at the last slot.
    const FlightEvent&             last = m_events.back();
    const std::vector<std::size_t> path = siftDownPath(i, last, s);
    if (!path.empty())
    {
        placeDown(i, path, takeLast());
        return std::nullopt;
    }
    const std::size_t upTarget = siftUpTarget(i, last);
    if (upTarget == i)
    {
        placeUp(i, i, takeLast());
        return std::nullopt;
    }
    // The last event goes above i: an iterator at i has not visited it. The copy is made first,
    // since it may throw.
    std::optional<FlightEvent> movedBefore{last};
    placeUp(i, upTarget, takeLast());
    return movedBefore;
}

FlightEvent EventQueue::takeLast() noexcept
{
    ++m_modCount;
    FlightEvent last = std::move(m_events.back());
    m_events.pop_back();
    return last;
}

void EventQueue::heapify()
{
    const std::size_t n = m_events.size();
    std::size_t       i = n >> 1U;
    while (i > 0)
    {
        i--;
        const std::vector<std::size_t> path = siftDownPath(i, m_events[i], n);
        if (!path.empty())
        {
            FlightEvent key = std::move(m_events[i]);
            placeDown(i, path, std::move(key));
        }
    }
}

}  // namespace QtRocket
