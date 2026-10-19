#include "QtRocket/rocket/StabilityUnits.h"

#include <memory>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/unit/CaliberUnit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

namespace
{

/// The largest diameter so far, @p caliber, grown by @p component's when it is a
/// SymmetricComponent (the loop of CaliberUnit.calculateCaliber(Iterator)).
[[nodiscard]] double widerCaliber(double caliber, const RocketComponent& component)
{
    if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(&component))
    {
        return MathUtil::max(caliber, symmetric->getForeRadius() * 2,
                             symmetric->getAftRadius() * 2);
    }
    return caliber;
}

/// CaliberUnit.calculateCaliber(Iterator)'s floor: kDefaultCaliber below 0.1 mm.
[[nodiscard]] double floorCaliber(double caliber)
{
    if (caliber < 0.0001)
    {
        return CaliberUnit::kDefaultCaliber;
    }
    return caliber;
}

/// Which reference a bound unit reads.
enum class Reference
{
    CALIBER,
    LENGTH,
};

/// The reference provider of a unit bound to a configuration or a rocket (CaliberUnit and
/// PercentageOfLengthUnit with a FlightConfiguration or a Rocket): it keeps the last reference
/// and computes it again when the configuration's or the rocket's modification id has changed
/// since the previous conversion (checkCaliber(), checkLength()). Each unit has its own copy.
class BoundReference
{
public:
    /// Bound to @p configuration and its rocket (Java: the FlightConfiguration constructor).
    BoundReference(Reference reference, const FlightConfiguration& configuration)
      : m_reference(reference),
        m_configuration(&configuration),
        m_rocket(&configuration.getRocket())
    {
    }

    /// Bound to @p rocket alone (Java: the Rocket constructor).
    BoundReference(Reference reference, const Rocket& rocket)
      : m_reference(reference), m_rocket(&rocket)
    {
    }

    [[nodiscard]] double operator()()
    {
        if (m_configuration != nullptr && m_configuration->getModId() != m_configurationModId)
        {
            m_value              = -1;
            m_configurationModId = m_configuration->getModId();
        }
        if (m_rocket->getModId() != m_rocketModId)
        {
            m_value       = -1;
            m_rocketModId = m_rocket->getModId();
        }
        // Java's sentinel test: a NaN reference stays until the next change, as there.
        if (m_value < 0)
        {
            m_value = compute();
        }
        return m_value;
    }

private:
    [[nodiscard]] double compute() const
    {
        if (m_configuration != nullptr)
        {
            return m_reference == Reference::CALIBER
                       ? StabilityUnits::calculateCaliber(*m_configuration)
                       : StabilityUnits::getPercentageReferenceLength(*m_configuration);
        }
        return m_reference == Reference::CALIBER
                   ? StabilityUnits::calculateCaliber(*m_rocket)
                   : StabilityUnits::getPercentageReferenceLength(*m_rocket);
    }

    Reference                  m_reference;
    const FlightConfiguration* m_configuration{nullptr};
    const Rocket*              m_rocket;
    ModId                      m_configurationModId{ModId::invalid()};
    ModId                      m_rocketModId{ModId::invalid()};
    /// The last reference; -1 until it is computed (Java: caliber = -1, referenceLength = -1).
    double m_value{-1};
};

}  // namespace

double StabilityUnits::calculateCaliber(const FlightConfiguration& configuration)
{
    double caliber = 0;
    for (const RocketComponent* component : configuration.getActiveComponents())
    {
        caliber = widerCaliber(caliber, *component);
    }
    return floorCaliber(caliber);
}

double StabilityUnits::calculateCaliber(const Rocket& rocket)
{
    double caliber = 0;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        caliber = widerCaliber(caliber, component);
    }
    return floorCaliber(caliber);
}

double StabilityUnits::getPercentageReferenceLength(const FlightConfiguration& configuration)
{
    return configuration.getLengthAerodynamic();
}

double StabilityUnits::getPercentageReferenceLength(const Rocket& rocket)
{
    return getPercentageReferenceLength(rocket.getSelectedConfiguration());
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> StabilityUnits::stabilityUnits(
    const FlightConfiguration& configuration)
{
    return UnitGroup::stabilityUnits(BoundReference(Reference::CALIBER, configuration),
                                     BoundReference(Reference::LENGTH, configuration));
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> StabilityUnits::secondaryStabilityUnits(
    const FlightConfiguration& configuration)
{
    return UnitGroup::secondaryStabilityUnits(BoundReference(Reference::CALIBER, configuration),
                                              BoundReference(Reference::LENGTH, configuration));
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> StabilityUnits::stabilityUnits(const Rocket& rocket)
{
    return UnitGroup::stabilityUnits(BoundReference(Reference::CALIBER, rocket),
                                     BoundReference(Reference::LENGTH, rocket));
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> StabilityUnits::secondaryStabilityUnits(
    const Rocket& rocket)
{
    return UnitGroup::secondaryStabilityUnits(BoundReference(Reference::CALIBER, rocket),
                                              BoundReference(Reference::LENGTH, rocket));
}

}  // namespace QtRocket
