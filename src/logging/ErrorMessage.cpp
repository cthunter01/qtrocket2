#include "QtRocket/logging/ErrorMessage.h"

#include <memory>
#include <string>
#include <utility>

#include "QtRocket/logging/Message.h"

namespace QtRocket
{

ErrorMessage::Other ErrorMessage::fromString(std::string text)
{
    return Other{std::move(text)};
}

ErrorMessage::Other::Other(std::string description) : m_description(std::move(description)) { }

std::unique_ptr<Message> ErrorMessage::Other::clone() const
{
    return std::make_unique<Other>(*this);
}

bool ErrorMessage::Other::equals(const Message& other) const
{
    const auto* o = dynamic_cast<const Other*>(&other);
    return o != nullptr && m_description == o->m_description;
}

}  // namespace QtRocket
