#include "QtRocket/logging/Message.h"

#include <cstdint>
#include <format>
#include <random>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace QtRocket
{

namespace
{

/// A random version-4 UUID in canonical text form (Java: UUID.randomUUID().toString()).
/// TODO(util): use QtRocket::Uuid once util/Uuid.h exists.
std::string randomUuid()
{
    thread_local std::mt19937_64                 s_engine{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> anyBits;
    std::uint64_t                                high = anyBits(s_engine);
    std::uint64_t                                low  = anyBits(s_engine);
    // Version 4 in the top nibble of the third group, variant 10xx in the top bits of the fourth.
    high = (high & ~std::uint64_t{0xF000U}) | std::uint64_t{0x4000U};
    low  = (low & ~(std::uint64_t{0xC000U} << 48U)) | (std::uint64_t{0x8000U} << 48U);
    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}", high >> 32U, (high >> 16U) & 0xFFFFU,
                       high & 0xFFFFU, low >> 48U, low & 0xFFFFFFFFFFFFU);
}

}  // namespace

Message::Message() : m_id(randomUuid()) { }

Message::Message(std::string id) : m_id(std::move(id)) { }

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
    throw std::logic_error("class doesn't implement replaceContents");
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
