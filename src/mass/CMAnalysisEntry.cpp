#include "QtRocket/mass/CMAnalysisEntry.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The designation of @p motor.
/// @throws BugError when @p motor is null (Java: NullPointerException).
const std::string& designationOf(const std::shared_ptr<const Motor>& motor)
{
    if (motor == nullptr)
    {
        bug("CMAnalysisEntry of a null motor");
    }
    return motor->getDesignation();
}

}  // namespace

CMAnalysisEntry::CMAnalysisEntry(const RocketComponent& component)
  : name(component.getName()),
    source(&component),
    eachMass(std::numeric_limits<double>::quiet_NaN()),
    totalCM(Coordinate::kNaN)
{
}

CMAnalysisEntry::CMAnalysisEntry(std::shared_ptr<const Motor> motor)
  : name(designationOf(motor)),
    source(std::move(motor)),
    eachMass(std::numeric_limits<double>::quiet_NaN()),
    totalCM(Coordinate::kNaN)
{
}

std::int32_t CMAnalysisEntry::keyOf(const RocketComponent& component) noexcept
{
    return component.hashCode();
}

std::int32_t CMAnalysisEntry::keyOf(const Motor& motor) noexcept
{
    return Strings::javaHashCode(motor.getDesignation());
}

const RocketComponent* CMAnalysisEntry::getComponent() const noexcept
{
    if (const auto* component = std::get_if<const RocketComponent*>(&source))
    {
        return *component;
    }
    return nullptr;
}

const Motor* CMAnalysisEntry::getMotor() const noexcept
{
    if (const auto* motor = std::get_if<std::shared_ptr<const Motor>>(&source))
    {
        return motor->get();
    }
    return nullptr;
}

void CMAnalysisEntry::updateEachMass(double newMass) noexcept
{
    if (std::isnan(eachMass))
    {
        eachMass = newMass;
    }
}

void CMAnalysisEntry::updateAverageCM(const Coordinate& newCM) noexcept
{
    if (totalCM.isNaN())
    {
        totalCM = newCM;
    }
    else
    {
        totalCM = totalCM.average(newCM);
    }
}

void CMAnalysisEntry::updateAssemblyMass(const Coordinate& aggregateCM, int instanceCount) noexcept
{
    updateAverageCM(aggregateCM);
    eachMass = totalCM.weight / instanceCount;
}

}  // namespace QtRocket
