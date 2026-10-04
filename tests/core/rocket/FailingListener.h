#pragma once

// A change listener that throws, for the tests of what an exception from a listener leaves in the
// tree (RocketComponent::removeChild() and splitInstances()). Test-only.

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Test
{

/// A change listener of a rocket that records how many children a watched component has at every
/// event it hears and throws std::runtime_error at its first event, or at every event. An action,
/// when it has one, runs at the first event before the exception is thrown: the tree may be
/// changed there, and the events that fires are neither recorded nor answered with an exception.
/// The slot holds a pointer to the listener, so it stays where it was made.
struct FailingListener
{
    /// What the slot records, also through a listener that is a const object.
    mutable std::vector<std::size_t> childCounts;

    /// Listens to @p rocket and watches @p watched; throws at every event when @p failAlways,
    /// else at the first one only, after @p action (if any).
    FailingListener(Rocket& rocket, const RocketComponent& watched, bool failAlways,
                    std::function<void()> action = {})
      : m_always(failAlways),
        m_action(std::move(action)),
        m_connection(rocket.addComponentChangeListener(
            [this, &watched](const ComponentChangeEvent&) { heard(watched); }))
    {
    }

    FailingListener(const FailingListener&)            = delete;
    FailingListener& operator=(const FailingListener&) = delete;
    FailingListener(FailingListener&&)                 = delete;
    FailingListener& operator=(FailingListener&&)      = delete;
    ~FailingListener()                                 = default;

private:
    void heard(const RocketComponent& watched) const
    {
        if (m_inAction)
        {
            return;
        }
        childCounts.push_back(watched.getChildCount());
        const bool first = childCounts.size() == 1;
        if (first && m_action)
        {
            m_inAction = true;
            m_action();
            m_inAction = false;
        }
        if (m_always || first)
        {
            throw std::runtime_error("the listener failed");
        }
    }

    bool                                    m_always;
    std::function<void()>                   m_action;
    mutable bool                            m_inAction{false};
    ComponentChangeSignal::ScopedConnection m_connection;
};

}  // namespace QtRocket::Test
