#include "QtRocket/logging/Message.h"

#include <string>
#include <typeinfo>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

Message::Message() : m_id(Uuid::random()) { }

Message::Message(Uuid id) noexcept : m_id(id) { }

std::string Message::toString() const
{
    return addSourcesToMessageText(messageDescription(), m_sources);
}

std::string Message::addSourcesToMessageText(std::string text, const MessageSources& sources)
{
    if (sources.empty())
    {
        return text;
    }
    text += ":  ";
    bool first = true;
    for (const MessageSource& source : sources)
    {
        if (!first)
        {
            text += ", ";
        }
        first = false;
        text += '"';
        text += source.name;
        text += '"';
    }
    return text;
}

bool Message::sourcesEqual(const MessageSources& lhs, const MessageSources& rhs) noexcept
{
    return lhs == rhs;  // element-wise MessageSource::operator==, i.e. by id
}

void Message::replaceContents(const Message& /*other*/)
{
    bug("class doesn't implement replaceContents");  // Java: UnsupportedOperationException
}

bool Message::equals(const Message& other) const
{
    return sameType(other) && sourcesEqual(m_sources, other.m_sources) &&
           m_priority == other.m_priority;
}

bool Message::sameType(const Message& other) const noexcept
{
    return typeid(*this) == typeid(other);
}

}  // namespace QtRocket
