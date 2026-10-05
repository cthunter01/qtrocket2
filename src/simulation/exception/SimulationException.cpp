#include "QtRocket/simulation/exception/SimulationException.h"

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace QtRocket
{

SimulationException::SimulationException() : std::runtime_error(""), m_hasMessage(false) { }

SimulationException::SimulationException(const std::string& message)
  : std::runtime_error(message), m_hasMessage(true)
{
}

SimulationException::SimulationException(const std::exception& cause)
  : std::runtime_error(cause.what()),
    m_hasMessage(true),
    m_causeMessage(std::make_shared<const std::string>(cause.what()))
{
}

SimulationException::SimulationException(const std::string& message, const std::exception& cause)
  : std::runtime_error(message),
    m_hasMessage(true),
    m_causeMessage(std::make_shared<const std::string>(cause.what()))
{
}

std::optional<std::string> SimulationException::getMessage() const
{
    return m_hasMessage ? std::optional<std::string>{what()} : std::nullopt;
}

std::optional<std::string> SimulationException::getCauseMessage() const
{
    return m_causeMessage != nullptr ? std::optional<std::string>{*m_causeMessage} : std::nullopt;
}

}  // namespace QtRocket
