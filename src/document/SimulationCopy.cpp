#include "QtRocket/document/SimulationCopy.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

/// What SimulationCopy keeps of @p simulation.
[[nodiscard]] SimulationCopy::Entry entryOf(const Simulation& simulation)
{
    std::vector<std::unique_ptr<const SimulationExtension>> extensions;
    extensions.reserve(simulation.getSimulationExtensions().size());
    for (const std::shared_ptr<SimulationExtension>& extension :
         simulation.getSimulationExtensions())
    {
        if (extension == nullptr)
        {
            bug("The simulation holds a null extension");
        }
        extensions.push_back(extension->clone());
    }
    return {.name                      = simulation.getName(),
            .configurationId           = simulation.getFlightConfigurationId(),
            .options                   = simulation.getOptions(),
            .extensions                = std::move(extensions),
            .landingDispersionSettings = simulation.getLandingDispersionSettings(),
            .plotAppearances           = simulation.getPlotAppearances(),
            .preferences               = simulation.getPreferences()};
}

}  // namespace

SimulationCopy::SimulationCopy(std::vector<Entry> entries) noexcept : m_entries(std::move(entries))
{
}

SimulationCopy SimulationCopy::copyOf(const std::vector<const Simulation*>& simulations)
{
    std::vector<Entry> entries;
    entries.reserve(simulations.size());
    for (const Simulation* simulation : simulations)
    {
        if (simulation == nullptr)
        {
            bug("A simulation to copy is null");
        }
        entries.push_back(entryOf(*simulation));
    }
    return SimulationCopy(std::move(entries));
}

const SimulationCopy::Entry& SimulationCopy::getEntry(std::size_t index) const
{
    if (index >= m_entries.size())
    {
        bug("The simulation copy has no entry at that index");
    }
    return m_entries[index];
}

std::shared_ptr<Simulation> SimulationCopy::makeSimulation(std::size_t         index,
                                                           OpenRocketDocument& target) const
{
    const Entry& entry = getEntry(index);

    // Java: new Simulation(this.document, newRocket), whose options are the factory's defaults;
    // the document is the target here (bug 22, see the header).
    std::shared_ptr<Simulation> simulation =
        entry.preferences != nullptr
            ? std::make_shared<Simulation>(&target, target.getRocket(), *entry.preferences)
            : std::make_shared<Simulation>(&target, target.getRocket());
    simulation->setName(entry.name);
    // Java sets the id alone and addSimulation() creates a missing configuration (see the
    // header).
    simulation->setFlightConfigurationId(entry.configurationId);
    simulation->copySimulationOptionsFrom(entry.options);
    simulation->setLandingDispersionSettings(entry.landingDispersionSettings);
    std::vector<std::shared_ptr<SimulationExtension>> extensions;
    extensions.reserve(entry.extensions.size());
    for (const std::unique_ptr<const SimulationExtension>& extension : entry.extensions)
    {
        extensions.push_back(extension->clone());
    }
    simulation->copyExtensionsFrom(extensions);
    simulation->setPlotAppearances(entry.plotAppearances);
    return simulation;
}

std::vector<std::shared_ptr<Simulation>> SimulationCopy::insertInto(OpenRocketDocument& target,
                                                                    std::size_t         index) const
{
    if (index > target.getSimulationCount())
    {
        bug("The simulations cannot be pasted beyond the end of the document's list");
    }
    std::vector<std::shared_ptr<Simulation>> inserted;
    inserted.reserve(m_entries.size());
    std::size_t newIndex = index;
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        std::shared_ptr<Simulation> simulation = makeSimulation(i, target);
        if (hasNumberedName(simulation->getName()))
        {
            simulation->setName(target.getNextSimulationName());
        }
        // A slot of an earlier insertion may have shortened the list: addSimulation() refuses
        // an index beyond its end.
        target.addSimulation(simulation, newIndex);
        // Ensure simulations are in the correct order
        newIndex++;
        inserted.push_back(std::move(simulation));
    }
    return inserted;
}

bool SimulationCopy::hasNumberedName(std::string_view name) noexcept
{
    // Java: name.matches(OpenRocketDocument.SIMULATION_NAME_PREFIX + "[0-9]+ *")
    if (!name.starts_with(OpenRocketDocument::kSimulationNamePrefix))
    {
        return false;
    }
    std::string_view rest   = name.substr(OpenRocketDocument::kSimulationNamePrefix.size());
    std::size_t      digits = 0;
    while (digits < rest.size() && rest[digits] >= '0' && rest[digits] <= '9')
    {
        ++digits;
    }
    if (digits == 0)
    {
        return false;
    }
    rest.remove_prefix(digits);
    return rest.find_first_not_of(' ') == std::string_view::npos;
}

}  // namespace QtRocket
