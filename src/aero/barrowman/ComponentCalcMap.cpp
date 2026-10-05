#include "QtRocket/aero/barrowman/ComponentCalcMap.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/aero/barrowman/CalcFactory.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

namespace
{

/// Whether the calculators keep a calculation for @p component (Java:
/// `comp.isAerodynamic() || comp instanceof ComponentAssembly`).
[[nodiscard]] bool hasCalculation(const RocketComponent& component)
{
    return component.isAerodynamic() || isAssembly(component.kind());
}

}  // namespace

ComponentCalcMap::ComponentCalcMap(ComponentCalcMap&& other) noexcept
  : m_calcs(std::move(other.m_calcs)), m_built(other.m_built), m_namesModId(other.m_namesModId)
{
    other.clear();
}

ComponentCalcMap& ComponentCalcMap::operator=(ComponentCalcMap&& other) noexcept
{
    if (this != &other)
    {
        m_calcs      = std::move(other.m_calcs);
        m_built      = other.m_built;
        m_namesModId = other.m_namesModId;
        other.clear();
    }
    return *this;
}

bool ComponentCalcMap::isFor(const Entry& entry, const RocketComponent& component) noexcept
{
    return entry.kind == component.kind() && entry.id == component.getId();
}

void ComponentCalcMap::ensureBuilt(const FlightConfiguration& configuration)
{
    if (!m_built)
    {
        build(configuration);
        return;
    }

    // A rename renews the rocket's modification id but neither its aerodynamic nor its tree
    // one, so the calculators do not void their cache for it: the names are compared here, and
    // only when the rocket has changed at all since they were last compared.
    const ModId modId = configuration.getRocket().getModId();
    if (modId != m_namesModId)
    {
        refreshRenamed(configuration);
        m_namesModId = modId;
    }
}

void ComponentCalcMap::build(const FlightConfiguration& configuration)
{
    const std::vector<RocketComponent*>       all = configuration.getAllComponents();
    const std::vector<const RocketComponent*> components(all.begin(), all.end());
    rebuild(components);
    m_namesModId = configuration.getRocket().getModId();
}

void ComponentCalcMap::refreshRenamed(const FlightConfiguration& configuration)
{
    for (const RocketComponent* const component : configuration.getAllComponents())
    {
        const auto found = m_calcs.find(component);
        if (found == m_calcs.end() || !isFor(found->second, *component))
        {
            continue;
        }
        std::string name = component->getName();
        if (found->second.name == name)
        {
            continue;
        }
        // The calculation first: when it cannot be made (BugError), the entry stays as it was.
        std::unique_ptr<RocketComponentCalc> calc = CalcFactory::create(*component);
        found->second.calc                        = std::move(calc);
        found->second.name                        = std::move(name);
    }
}

void ComponentCalcMap::rebuild(std::span<const RocketComponent* const> components)
{
    Calcs calcs;
    calcs.reserve(components.size());
    for (const RocketComponent* const component : components)
    {
        if (!hasCalculation(*component))
        {
            continue;
        }
        calcs.insert_or_assign(component, Entry{.id   = component->getId(),
                                                .kind = component->kind(),
                                                .name = component->getName(),
                                                .calc = CalcFactory::create(*component)});
    }
    m_calcs = std::move(calcs);
    m_built = true;
}

void ComponentCalcMap::clear() noexcept
{
    m_calcs.clear();
    m_built      = false;
    m_namesModId = ModId::invalid();
}

RocketComponentCalc* ComponentCalcMap::get(const RocketComponent& component)
{
    if (!hasCalculation(component))
    {
        return nullptr;
    }

    const auto found = m_calcs.find(&component);
    if (found != m_calcs.end() && isFor(found->second, component))
    {
        return found->second.calc.get();
    }

    // Nothing at this address for this component. Java's map compares by class and id: it has
    // no calculation for an id it does not know, and the calculation of the equal component
    // for one it does.
    const bool known = std::ranges::any_of(m_calcs, [&component](const Calcs::value_type& item) {
        return isFor(item.second, component);
    });
    if (!known)
    {
        return nullptr;
    }

    // The component's object replaced the one the map was built with (Rocket::loadFrom(), a
    // copy with the original ids): the other calculations belong to replaced objects as well.
    const RocketComponent&              root       = component.getRoot();
    std::vector<const RocketComponent*> components = root.getAllChildren();
    components.insert(components.begin(), &root);
    rebuild(components);

    const auto rebuilt = m_calcs.find(&component);
    return rebuilt == m_calcs.end() ? nullptr : rebuilt->second.calc.get();
}

}  // namespace QtRocket
