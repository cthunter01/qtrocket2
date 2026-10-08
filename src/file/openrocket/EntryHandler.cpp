#include "QtRocket/file/openrocket/EntryHandler.h"

#include <memory>
#include <utility>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

const Config::List* EntryHandler::getNestedList() const noexcept
{
    if (m_listHandler != nullptr)
    {
        return &m_listHandler->m_list;
    }
    return nullptr;
}

EntryHandler* EntryHandler::openList(std::unique_ptr<EntryHandler> handler)
{
    if (handler == nullptr)
    {
        bug("The handler of a list entry must not be null");
    }
    m_listHandler = std::move(handler);
    return m_listHandler.get();
}

void EntryHandler::closeList() noexcept
{
    m_listHandler.reset();
}

void EntryHandler::addToList(Config::Value value)
{
    m_list.push_back(std::move(value));
}

}  // namespace QtRocket
