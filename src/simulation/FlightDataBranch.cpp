#include "QtRocket/simulation/FlightDataBranch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/DataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

[[nodiscard]] const FlightDataType& timeType()
{
    return FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
}

[[nodiscard]] std::optional<Uuid> idOf(const RocketComponent* component)
{
    return component != nullptr ? std::optional<Uuid>{component->getId()} : std::nullopt;
}

/// The stage of @p component for the comparison of copyValuesFromBranch() (Java:
/// getStageForComponent()): the component itself when it is a stage, null for the Rocket, else
/// the stage it is in.
[[nodiscard]] const AxialStage* getStageForComponent(const RocketComponent& component)
{
    if (const auto* stage = dynamic_cast<const AxialStage*>(&component))
    {
        return stage;
    }
    if (dynamic_cast<const Rocket*>(&component) != nullptr)
    {
        return nullptr;
    }
    return &component.getStage();
}

/// Java's parent.containsChild(component): whether a descendant of @p parent equals
/// @p component (List.contains(), so RocketComponent.equals(): the same class and id).
[[nodiscard]] bool containsChild(const RocketComponent& parent, const RocketComponent& component)
{
    const std::vector<const RocketComponent*> all = parent.getAllChildren();
    return std::ranges::any_of(
        all, [&component](const RocketComponent* child) { return component.equals(*child); });
}

/// Whether the branch of the separated stage @p srcComponent takes @p event of its parent.
[[nodiscard]] bool belongsToStage(const FlightEvent& event, const RocketComponent* srcComponent)
{
    // Stage separation is already added elsewhere, so don't copy it over (otherwise you have a
    // duplicate)
    if (event.getType() == FlightEvent::Type::STAGE_SEPARATION)
    {
        return false;
    }
    const RocketComponent* srcEventComponent = event.getSource();
    // Ignore null events
    if (srcComponent == nullptr || srcEventComponent == nullptr)
    {
        return false;
    }
    // Ignore events from other stages. Important for when the current stage has a booster
    // stage; we don't want to copy over the booster events.
    if (getStageForComponent(*srcComponent) != getStageForComponent(*srcEventComponent))
    {
        return false;
    }
    return srcComponent == srcEventComponent || containsChild(*srcComponent, *srcEventComponent);
}

}  // namespace

FlightDataBranch::FlightDataBranch(std::string name, std::span<const FlightDataType* const> types)
  : DataBranch<FlightDataType>(std::move(name), types)
{
}

FlightDataBranch::FlightDataBranch(
    std::string name, std::initializer_list<std::reference_wrapper<const FlightDataType>> types)
  : DataBranch<FlightDataType>(std::move(name), types)
{
}

FlightDataBranch::FlightDataBranch(std::string name, const RocketComponent* srcComponent,
                                   std::span<const FlightDataType* const> types)
  : DataBranch<FlightDataType>(std::move(name), types), m_sourceComponentId(idOf(srcComponent))
{
}

FlightDataBranch::FlightDataBranch(
    std::string name, const RocketComponent* srcComponent,
    std::initializer_list<std::reference_wrapper<const FlightDataType>> types)
  : DataBranch<FlightDataType>(std::move(name), types), m_sourceComponentId(idOf(srcComponent))
{
}

// Java: super(name), then copyValuesFromBranch() clears the columns and puts the TIME column
// straight into the maps (no modification id): a branch constructed with that one type.
FlightDataBranch::FlightDataBranch(std::string name, const RocketComponent* srcComponent,
                                   const FlightDataBranch* parent)
  : DataBranch<FlightDataType>(std::move(name), {timeType()}),
    m_sourceComponentId(idOf(srcComponent))
{
    // Copy all the values from the parent
    copyValuesFromBranch(parent, srcComponent);
}

FlightDataBranch::FlightDataBranch() : DataBranch<FlightDataType>("Empty branch")
{
    for (const FlightDataType* type : FlightDataType::allTypes())
    {
        setValue(*type, std::numeric_limits<double>::quiet_NaN());
    }
    immute();
}

FlightDataBranch::FlightDataBranch(DataBranch<FlightDataType>&& columns,
                                   std::optional<Uuid>          sourceComponentId)
  : DataBranch<FlightDataType>(std::move(columns)), m_sourceComponentId(sourceComponentId)
{
}

void FlightDataBranch::copyValuesFromBranch(const FlightDataBranch* srcBranch,
                                            const RocketComponent*  srcComponent)
{
    if (srcBranch == nullptr)
    {
        return;
    }

    // Copy flight data
    const std::size_t length = srcBranch->getLength();
    if (length > 0)
    {
        const std::vector<const FlightDataType*> types = srcBranch->getTypes();
        for (std::size_t i = 0; i < length; i++)
        {
            addPoint();
            for (const FlightDataType* type : types)
            {
                const std::optional<double> value = srcBranch->getByIndex(*type, i);
                QTROCKET_ASSERT(value.has_value());
                setValue(*type, *value);
            }
        }
    }

    // Copy flight events belonging to this branch
    for (const FlightEvent& event : srcBranch->m_events)
    {
        if (belongsToStage(event, srcComponent))
        {
            m_events.push_back(event);
        }
    }
}

double FlightDataBranch::getOptimumDelay() const
{
    if (std::isnan(m_timeToOptimumAltitude))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    // TODO - we really want the first burnout of this stage.  which
    // could be computed as the first burnout after the last stage separation event.
    // however, that's not quite so concise
    if (const std::optional<FlightEvent> event = getLastEvent(FlightEvent::Type::BURNOUT))
    {
        return m_timeToOptimumAltitude - event->getTime();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

void FlightDataBranch::addEvent(FlightEvent event, std::source_location where)
{
    checkMutable(where);
    const bool   isSeparation = event.getType() == FlightEvent::Type::STAGE_SEPARATION;
    const double time         = event.getTime();
    m_events.push_back(std::move(event));
    if (isSeparation)
    {
        m_separationTime = time;
    }
    markModified();
}

std::optional<FlightEvent> FlightDataBranch::getFirstEvent(FlightEvent::Type type) const
{
    for (const FlightEvent& event : m_events)
    {
        if (event.getType() == type)
        {
            return event;
        }
    }
    return std::nullopt;
}

std::optional<FlightEvent> FlightDataBranch::getLastEvent(FlightEvent::Type type) const
{
    const FlightEvent* last = nullptr;
    for (const FlightEvent& event : m_events)
    {
        if (event.getType() == type)
        {
            last = &event;
        }
    }
    return last != nullptr ? std::optional<FlightEvent>{*last} : std::nullopt;
}

std::optional<FlightEvent> FlightDataBranch::findEvent(const Uuid& id) const
{
    for (const FlightEvent& event : m_events)
    {
        if (id == event.getId())
        {
            return event;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FlightDataBranch::getDataIndexOfTime(double time) const
{
    if (std::isnan(time))
    {
        return std::nullopt;
    }
    const std::vector<double>* times = getView(timeType());
    if (times == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < times->size(); i++)
    {
        if ((*times)[i] >= time)
        {
            return i;
        }
    }
    return std::nullopt;
}

FlightDataBranch FlightDataBranch::clone() const
{
    FlightDataBranch copy(DataBranch<FlightDataType>::clone(), m_sourceComponentId);
    copy.m_events                = m_events;
    copy.m_timeToOptimumAltitude = m_timeToOptimumAltitude;
    copy.m_optimumAltitude       = m_optimumAltitude;
    return copy;
}

}  // namespace QtRocket
