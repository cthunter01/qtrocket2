#include "QtRocket/gui/binding/ComponentSelection.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

ComponentSelection::ComponentSelection(RocketComponent&                  main,
                                       std::span<RocketComponent* const> others)
  : m_main(&main), m_rocket(&main.getRocket())
{
    for (RocketComponent* other : others)
    {
        QTROCKET_ASSERT(other != nullptr);
        if (&other->getRoot() != m_rocket)
        {
            bug("A selected component is not in the main component's rocket");
        }
        if (other != m_main && std::ranges::find(m_others, other) == m_others.end())
        {
            m_others.push_back(other);
        }
    }
    m_rocketConnection = m_rocket->addComponentChangeListener(
        [this](const ComponentChangeEvent& event) { componentChanged(event); });
}

ComponentSelection::~ComponentSelection() = default;

bool ComponentSelection::isValid() const noexcept
{
    return !m_invalidated && m_rocketConnection.connected();
}

void ComponentSelection::checkValid() const
{
    if (!isValid())
    {
        bug("The component selection is used after a tree or undo change made it invalid");
    }
}

RocketComponent& ComponentSelection::getMain() const
{
    checkValid();
    return *m_main;
}

std::span<RocketComponent* const> ComponentSelection::getOthers() const
{
    checkValid();
    return m_others;
}

Rocket& ComponentSelection::getRocket() const
{
    checkValid();
    return *m_rocket;
}

bool ComponentSelection::isMultiEdit() const
{
    checkValid();
    return !m_others.empty();
}

bool ComponentSelection::isAllOfOneKind() const
{
    checkValid();
    const std::vector<const RocketComponent*> others(m_others.begin(), m_others.end());
    return m_main->checkAllClassesEqual(others);
}

void ComponentSelection::applyToEach(const std::function<void(RocketComponent&)>& f)
{
    checkValid();
    // A listener of the thaw's event may drop the last owner of the selection.
    const std::shared_ptr<ComponentSelection> keepAlive = weak_from_this().lock();
    // The targets are copied: f's events are queued until the thaw, but a TREE or UNDO change
    // delivered then clears the lists.
    std::vector<RocketComponent*> targets = m_others;
    targets.push_back(m_main);
    Rocket&             rocket = *m_rocket;
    const bool          freeze = !rocket.isFrozen();
    const std::uint64_t events = m_eventCount;
    if (freeze)
    {
        rocket.freeze();
    }
    try
    {
        for (RocketComponent* target : targets)
        {
            f(*target);
        }
    }
    catch (...)
    {
        if (freeze)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freeze)
    {
        rocket.thaw();
    }
    if (m_eventCount == events)
    {
        refresh();
    }
}

void ComponentSelection::applyToEachUnfrozen(const std::function<void(RocketComponent&)>& f)
{
    checkValid();
    // A listener of a call's event may drop the last owner of the selection.
    const std::shared_ptr<ComponentSelection> keepAlive = weak_from_this().lock();
    // The targets are copied: an event that ends the selection clears the lists.
    std::vector<RocketComponent*> targets = m_others;
    targets.push_back(m_main);
    const std::uint64_t events = m_eventCount;
    for (RocketComponent* target : targets)
    {
        // Each call's events are delivered at once; one that ended the selection may have
        // destroyed the components not reached yet.
        if (!isValid())
        {
            return;
        }
        f(*target);
    }
    if (m_eventCount == events)
    {
        refresh();
    }
}

void ComponentSelection::refresh()
{
    if (!isValid())
    {
        return;
    }
    const std::shared_ptr<ComponentSelection> keepAlive = weak_from_this().lock();
    m_listeners.emit(Phase::READ);
    // A slot of READ only reads; one of NOTIFY may change the rocket, and so end the selection.
    if (isValid())
    {
        m_listeners.emit(Phase::NOTIFY);
    }
}

ComponentSelection::PhaseSignal::Connection ComponentSelection::addListener(PhaseSignal::Slot slot)
{
    return m_listeners.connect(std::move(slot));
}

void ComponentSelection::componentChanged(const ComponentChangeEvent& event)
{
    // A listener may drop the last owner of the selection (see the class comment).
    const std::shared_ptr<ComponentSelection> keepAlive = weak_from_this().lock();
    ++m_eventCount;
    if (event.isTreeChange() || event.isUndoChange())
    {
        invalidate();
        return;
    }
    refresh();
}

void ComponentSelection::dropComponents() noexcept
{
    m_invalidated = true;
    m_main        = nullptr;
    m_others.clear();
    m_rocket = nullptr;
    m_rocketConnection.disconnect();
}

void ComponentSelection::invalidate()
{
    if (m_invalidated)
    {
        return;
    }
    // The components of the event live until it is delivered; this selection lets go of them now.
    dropComponents();
    m_listeners.emit(Phase::INVALIDATED);
}

void ComponentSelection::release() noexcept
{
    if (!m_invalidated)
    {
        dropComponents();
    }
}

}  // namespace QtRocket::Gui
