#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

namespace
{

/// Class.getSimpleName() of the class whose canonical name is @p id: what follows the last '.'.
[[nodiscard]] std::string simpleName(const std::string& id)
{
    const std::size_t lastDot = id.rfind('.');
    return lastDot == std::string::npos ? id : id.substr(lastDot + 1);
}

}  // namespace

AbstractSimulationExtension::AbstractSimulationExtension(std::string id)
  : m_id(std::move(id)), m_name(simpleName(m_id))
{
}

AbstractSimulationExtension::AbstractSimulationExtension(std::string id, std::string name)
  : m_id(std::move(id)), m_name(std::move(name))
{
}

AbstractSimulationExtension::AbstractSimulationExtension(const AbstractSimulationExtension& other)
  : SimulationExtension(other), m_config(other.m_config), m_id(other.m_id), m_name(other.m_name)
{
    // m_changed starts empty: the connections stay with the original.
}

std::string AbstractSimulationExtension::getId() const
{
    return m_id;
}

std::string AbstractSimulationExtension::getName() const
{
    return m_name;
}

std::optional<std::string> AbstractSimulationExtension::getDescription() const
{
    return std::nullopt;
}

std::vector<const FlightDataType*> AbstractSimulationExtension::getFlightDataTypes() const
{
    return {};
}

void AbstractSimulationExtension::documentLoaded(OpenRocketDocument& /*document*/,
                                                 Simulation& /*simulation*/,
                                                 WarningSet& /*warnings*/)
{
}

Config AbstractSimulationExtension::getConfig() const
{
    return m_config;
}

void AbstractSimulationExtension::setConfig(const Config& config)
{
    m_config = config;
    fireChangeEvent();
}

}  // namespace QtRocket
