#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

UnknownSimulationExtension::UnknownSimulationExtension(std::string id, const Config& config)
  : AbstractSimulationExtension(std::move(id))
{
    // Not setConfig(): nobody can listen yet, and a constructor announces nothing.
    m_config = config;
}

std::string UnknownSimulationExtension::notFoundText(std::string_view id)
{
    return std::format("Simulation extension with id '{}' not found.", id);
}

void UnknownSimulationExtension::initialize(SimulationConditions& /*conditions*/)
{
    throw SimulationException(notFoundText(getId()));
}

std::unique_ptr<SimulationExtension> UnknownSimulationExtension::clone() const
{
    return std::make_unique<UnknownSimulationExtension>(*this);
}

}  // namespace QtRocket
