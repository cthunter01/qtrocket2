#include "QtRocket/aero/barrowman/ComponentCalcMap.h"

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/aero/barrowman/CalcFactory.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"

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

bool ComponentCalcMap::isFor(const Entry& entry, const RocketComponent& component) noexcept
{
    return entry.kind == component.kind() && entry.id == component.getId();
}

void ComponentCalcMap::ensureBuilt(const FlightConfiguration& configuration)
{
    if (!m_built)
    {
        build(configuration);
    }
}

void ComponentCalcMap::build(const FlightConfiguration& configuration)
{
    const std::vector<RocketComponent*>       all = configuration.getAllComponents();
    const std::vector<const RocketComponent*> components(all.begin(), all.end());
    rebuild(components);
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
                                                .calc = CalcFactory::create(*component)});
    }
    m_calcs = std::move(calcs);
    m_built = true;
}

void ComponentCalcMap::clear() noexcept
{
    m_calcs.clear();
    m_built = false;
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
