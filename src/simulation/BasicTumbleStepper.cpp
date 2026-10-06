#include "QtRocket/simulation/BasicTumbleStepper.h"

#include <cstddef>
#include <utility>

#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): the override of a virtual
double BasicTumbleStepper::computeCD(const SimulationStatus& status)
{
    // Computed based on Sampo's experimentation as documented in techdoc.pdf.

    // compute the fin and body tube projected areas
    double             aFins = 0.0;
    double             aBt   = 0.0;
    const InstanceMap& imap  = status.getConfiguration().getActiveInstances();
    for (const InstanceMap::Entry& entry : imap)
    {
        const RocketComponent* component = entry.first;

        if (!component->isAerodynamic())
        {
            continue;
        }

        // iterate across component instances
        const auto* finComponent       = dynamic_cast<const FinSet*>(component);
        const auto* symmetricComponent = dynamic_cast<const SymmetricComponent*>(component);
        for ([[maybe_unused]] const InstanceContext& context : entry.second)
        {
            if (finComponent != nullptr)
            {
                const double finArea  = finComponent->getPlanformArea();
                int          finCount = finComponent->getFinCount();

                // check bounds on finCount.
                if (std::cmp_greater_equal(finCount, kFinEfficiency.size()))
                {
                    finCount = static_cast<int>(kFinEfficiency.size()) - 1;
                }

                aFins += finArea * kFinEfficiency.at(static_cast<std::size_t>(finCount)) /
                         finComponent->getFinCount();
            }
            else if (symmetricComponent != nullptr)
            {
                aBt += symmetricComponent->getComponentPlanformArea();
            }
        }
    }

    return ((kCdFin * aFins) + (kCdBt * aBt)) / status.getConfiguration().getReferenceArea();
}

}  // namespace QtRocket
