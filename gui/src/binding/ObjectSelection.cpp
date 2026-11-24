#include "QtRocket/gui/binding/ObjectSelection.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

ObjectSelection::ObjectSelection(Token /*token*/, std::unique_ptr<Holder> object, Signal<>& changed)
  : m_object(std::move(object))
{
    QTROCKET_ASSERT(m_object != nullptr);
    m_connection = changed.connect([this] { objectChanged(); });
}

ObjectSelection::~ObjectSelection() = default;

bool ObjectSelection::isValid() const noexcept
{
    return !m_ended && m_connection.connected();
}

void ObjectSelection::checkValid() const
{
    if (!isValid())
    {
        bug("The object selection is used after its object has gone or it was ended");
    }
}

void ObjectSelection::wrongClass(std::string_view wanted) const
{
    bug("The property is defined for the class " + std::string(wanted) +
        ", the object selection holds an object of the class " +
        std::string(m_object->className()));
}

void ObjectSelection::applyCall(const std::function<void()>& call)
{
    // A listener of the object's change may drop the last owner of the selection.
    const std::shared_ptr<ObjectSelection> keepAlive = weak_from_this().lock();
    const std::uint64_t                    changes   = m_changeCount;
    call();
    if (m_changeCount == changes)
    {
        refresh();
    }
}

void ObjectSelection::refresh()
{
    if (!isValid())
    {
        return;
    }
    const std::shared_ptr<ObjectSelection> keepAlive = weak_from_this().lock();
    m_listeners.emit(Phase::READ);
    // A slot of READ only reads; one of NOTIFY may change the object, or end the selection.
    if (isValid())
    {
        m_listeners.emit(Phase::NOTIFY);
    }
}

ObjectSelection::PhaseSignal::Connection ObjectSelection::addListener(PhaseSignal::Slot slot)
{
    return m_listeners.connect(std::move(slot));
}

void ObjectSelection::objectChanged()
{
    // A listener may drop the last owner of the selection (see the class comment).
    const std::shared_ptr<ObjectSelection> keepAlive = weak_from_this().lock();
    ++m_changeCount;
    refresh();
}

void ObjectSelection::dropObject() noexcept
{
    m_ended = true;
    m_connection.disconnect();
}

void ObjectSelection::invalidate()
{
    // Also when the object has gone already: the bindings never read it on INVALIDATED.
    if (m_ended)
    {
        return;
    }
    const std::shared_ptr<ObjectSelection> keepAlive = weak_from_this().lock();
    dropObject();
    m_listeners.emit(Phase::INVALIDATED);
}

void ObjectSelection::release() noexcept
{
    if (!m_ended)
    {
        dropObject();
    }
}

}  // namespace QtRocket::Gui
