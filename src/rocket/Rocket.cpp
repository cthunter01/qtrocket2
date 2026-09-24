#include "QtRocket/rocket/Rocket.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

Rocket::Rocket()
  : ComponentAssembly(AxialMethod::ABSOLUTE),
    m_massModId(m_modId),
    m_aeroModId(m_modId),
    m_treeModId(m_modId),
    m_functionalModId(m_modId)
{
    // HOOK(rocket-config): Java creates the default FlightConfiguration here, the default of the
    // configuration set and the selected configuration.
}

Rocket::Rocket(const Rocket& other, CopyKey /*key*/)
  : AxialPositionable(other),
    BoxBounded(other),
    ComponentAssembly(other),
    m_modId(other.m_modId),
    m_massModId(other.m_massModId),
    m_aeroModId(other.m_aeroModId),
    m_treeModId(other.m_treeModId),
    m_functionalModId(other.m_functionalModId),
    m_eventsEnabled(other.m_eventsEnabled),
    m_document(other.m_document),
    m_refType(other.m_refType),
    m_customReferenceLength(other.m_customReferenceLength),
    m_designer(other.m_designer),
    m_revision(other.m_revision),
    m_designType(other.m_designType),
    m_kitName(other.m_kitName),
    m_perfectFinish(other.m_perfectFinish)
{
    // Not copied: the listeners, the freeze state and the stage map (rebuilt by
    // copyWithOriginalId() once the children exist).
}

Rocket::~Rocket() = default;

std::unique_ptr<RocketComponent> Rocket::cloneShallow() const
{
    return std::make_unique<Rocket>(*this, CopyKey{});
}

bool Rocket::isCompatible(ComponentKind kind) const
{
    return ComponentKind::AXIAL_STAGE == kind;
}

// =============================================================================== metadata

void Rocket::setDesigner(std::string_view designer)
{
    m_designer = designer;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void Rocket::setDesignType(DesignType type)
{
    m_designType = type;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void Rocket::setKitName(std::string_view kitName)
{
    m_kitName = kitName;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void Rocket::setRevision(std::string_view revision)
{
    m_revision = revision;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void Rocket::setReferenceType(ReferenceType type)
{
    if (m_refType == type)
    {
        return;
    }
    m_refType = type;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void Rocket::setCustomReferenceLength(double length)
{
    if (MathUtil::equals(m_customReferenceLength, length))
    {
        return;
    }
    // Java's Math.max(length, 0.001), which keeps a NaN.
    m_customReferenceLength = (length > 0.001 || std::isnan(length)) ? length : 0.001;

    if (m_refType == ReferenceType::CUSTOM)
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void Rocket::setPerfectFinish(bool perfectFinish)
{
    if (m_perfectFinish == perfectFinish)
    {
        return;
    }
    m_perfectFinish = perfectFinish;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

// ================================================================================= stages

std::vector<AxialStage*> Rocket::getStageList() const
{
    std::vector<AxialStage*> stages;
    stages.reserve(m_stageMap.size());
    for (const auto& entry : m_stageMap)
    {
        stages.push_back(entry.second);
    }
    return stages;
}

AxialStage* Rocket::getStage(int stageNumber) const noexcept
{
    const auto it = m_stageMap.find(stageNumber);
    return it != m_stageMap.end() ? it->second : nullptr;
}

AxialStage* Rocket::getStage(const Uuid& stageId) const noexcept
{
    for (const auto& entry : m_stageMap)
    {
        if (entry.second->getId() == stageId)
        {
            return entry.second;
        }
    }
    return nullptr;
}

int Rocket::getStageNumber() const
{
    return -1;  // invalid: the rocket belongs to no stage
}

int Rocket::getNewStageNumber() const
{
    int guess = 0;
    while (m_stageMap.contains(guess))
    {
        guess++;
    }
    return guess;
}

void Rocket::trackStage(AxialStage& newStage)
{
    int               stageNumber = newStage.getStageNumber();
    const AxialStage* value       = getStage(stageNumber);

    if (value != nullptr && newStage.equals(*value))
    {
        // Already tracked; point the entry at this object when it holds an equal one.
        if (&newStage != value)
        {
            m_stageMap[stageNumber] = &newStage;
        }
        return;
    }
    stageNumber = getNewStageNumber();
    newStage.setStageNumber(stageNumber);
    m_stageMap[stageNumber] = &newStage;
}

void Rocket::forgetStage(const AxialStage& oldStage)
{
    m_stageMap.erase(oldStage.getStageNumber());
}

bool Rocket::isStageActiveInSelectedConfiguration(int stageNumber) const noexcept
{
    if (-1 == stageNumber)
    {
        return true;
    }
    // HOOK(rocket-config): FlightConfiguration also requires the stage's flag in the
    // configuration to be set; stages with no children are inactive.
    const AxialStage* stage = getStage(stageNumber);
    return stage != nullptr && stage->getChildCount() > 0;
}

bool Rocket::isComponentActiveInSelectedConfiguration(const RocketComponent& component) const
{
    return isStageActiveInSelectedConfiguration(component.getStageNumber());
}

void Rocket::updateStageNumbers()
{
    int stageNr = 0;
    for (AxialStage* stage : getSubStages())
    {
        forgetStage(*stage);
        stage->setStageNumber(stageNr);
        stageNr++;
    }
}

void Rocket::updateStageMap()
{
    for (AxialStage* stage : getSubStages())
    {
        trackStage(*stage);
    }
}

void Rocket::rebuildStageMap(const Rocket& source)
{
    m_stageMap.clear();
    for (const auto& [number, sourceStage] : source.m_stageMap)
    {
        auto* stage = dynamic_cast<AxialStage*>(findComponent(sourceStage->getId()));
        if (stage != nullptr)
        {
            m_stageMap[number] = stage;
        }
    }
}

// =============================================================================== position

void Rocket::setAxialMethod(AxialMethod /*newAxialMethod*/)
{
    m_axialMethod = AxialMethod::ABSOLUTE;
}

void Rocket::setAxialOffset(double /*requestedOffset*/)
{
    m_axialOffset = 0.0;
    m_position    = Coordinate::kZero;
}

double Rocket::getLength() const
{
    // HOOK(rocket-config): getSelectedConfiguration().getLength().
    return m_length;
}

double Rocket::getBoundingRadius() const
{
    double bounding = 0;
    for (const auto& comp : m_children)
    {
        if (const auto* assembly = dynamic_cast<const ComponentAssembly*>(comp.get()))
        {
            bounding = std::max(bounding, assembly->getBoundingRadius());
        }
    }
    return bounding;
}

// ================================================================================= events

ComponentChangeSignal::Connection Rocket::addComponentChangeListener(
    ComponentChangeSignal::Slot slot)
{
    return m_listeners.connect(std::move(slot));
}

void Rocket::fireComponentChangeEvent(int type, std::span<const FlightConfigurationId> ids)
{
    fireComponentChangeEvent(ComponentChangeEvent{this, type}, ids);
}

void Rocket::fireComponentChangeEvent(const ComponentChangeEvent& event)
{
    fireComponentChangeEvent(event, std::nullopt);
}

void Rocket::fireComponentChangeEvent(const ComponentChangeEvent&                           event,
                                      std::optional<std::span<const FlightConfigurationId>> ids)
{
    if (!m_eventsEnabled)
    {
        return;
    }

    // Modification ids change only for normal (not undo/redo) events.
    if (!event.isUndoChange())
    {
        m_modId = ModId{};
        if (event.isMassChange())
        {
            m_massModId = m_modId;
        }
        if (event.isAerodynamicChange())
        {
            m_aeroModId = m_modId;
        }
        if (event.isTreeChange())
        {
            m_treeModId = m_modId;
        }
        if (event.isFunctionalChange())
        {
            m_functionalModId = m_modId;
            // HOOK(rocket-config): updateConfigurationsModID(ids): updateModID() on the
            // configurations in ids (all of them without ids).
        }
    }

    if (m_freezeList)
    {
        m_freezeList->push_back(
            FrozenEvent{.type = event.getType(), .sourceId = event.getSource()->getId()});
        return;
    }

    // Every component first, with the fail-fast iterator (componentChanged() must not change the
    // tree).
    for (RocketComponent& component : subtree(true))
    {
        component.componentChanged(event);
    }
    // HOOK(rocket-config): updateConfigurations(ids): update() on the configurations in ids (all
    // of them without ids), which rebuilds their instance maps.
    static_cast<void>(ids);

    m_listeners.emit(event);
}

void Rocket::update()
{
    updateStageNumbers();
    updateStageMap();
    // HOOK(rocket-config): updateConfigurations() (all of them).
}

void Rocket::enableEvents()
{
    enableEvents(true);
    update();
}

void Rocket::enableEvents(bool enable)
{
    if (m_eventsEnabled && enable)
    {
        return;
    }
    if (enable)
    {
        m_eventsEnabled = true;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    else
    {
        m_eventsEnabled = false;
    }
}

void Rocket::freeze()
{
    if (m_freezeList)
    {
        bug("Attempting to freeze Rocket when it is already frozen");
    }
    m_freezeList.emplace();
}

void Rocket::thaw()
{
    if (!m_freezeList)
    {
        bug("Attempting to thaw Rocket when it is not frozen");
    }
    if (m_freezeList->empty())
    {
        // Thawing a rocket with no changes made (Java logs a warning).
        m_freezeList.reset();
        return;
    }

    int  type     = 0;
    Uuid sourceId = getId();
    for (const FrozenEvent& e : *m_freezeList)
    {
        type     = type | e.type;
        sourceId = e.sourceId;
    }
    m_freezeList.reset();

    // The last source, found again by id: it may have left the tree (and been destroyed) since
    // it fired. The rocket stands in for it then (Java keeps the detached object).
    RocketComponent* source = findComponent(sourceId);
    fireComponentChangeEvent(ComponentChangeEvent{source != nullptr ? source : this, type});
}

// ================================================================================ copying

std::unique_ptr<RocketComponent> Rocket::copyWithOriginalId() const
{
    std::unique_ptr<RocketComponent> copy       = RocketComponent::copyWithOriginalId();
    auto&                            copyRocket = dynamic_cast<Rocket&>(*copy);

    // The stage map of the copy points at the copied stages.
    copyRocket.rebuildStageMap(*this);
    if (copyRocket.m_stageMap.size() != m_stageMap.size())
    {
        bug("Stage not found in copy");
    }

    // HOOK(rocket-config): the configuration set is rebuilt for the copy: a new default
    // FlightConfiguration(copy), then for every id a FlightConfiguration(copy, id) with the
    // original's raw name and stage activeness; the selected configuration is the copy's
    // configuration of the selected id.
    return copy;
}

std::unique_ptr<Rocket> Rocket::copyRocketWithOriginalId() const
{
    return componentCast<Rocket>(copyWithOriginalId());
}

void Rocket::loadFrom(const Rocket& source)
{
    // The replaced components live until the event has been delivered.
    const std::vector<std::unique_ptr<RocketComponent>> previous = copyFrom(source);

    int type = ComponentChangeEvent::kUndoChange | ComponentChangeEvent::kNonFunctionalChange;
    if (m_massModId != source.m_massModId)
    {
        type |= ComponentChangeEvent::kMassChange;
    }
    if (m_aeroModId != source.m_aeroModId)
    {
        type |= ComponentChangeEvent::kAerodynamicChange;
    }
    // Loading a rocket is always a tree change, since the component objects change.
    type |= ComponentChangeEvent::kTreeChange;

    m_modId                 = source.m_modId;
    m_massModId             = source.m_massModId;
    m_aeroModId             = source.m_aeroModId;
    m_treeModId             = source.m_treeModId;
    m_functionalModId       = source.m_functionalModId;
    m_refType               = source.m_refType;
    m_customReferenceLength = source.m_customReferenceLength;
    rebuildStageMap(source);

    // HOOK(rocket-config): the configuration set is reset to a new default FlightConfiguration
    // (this), then for every id of the source's set a FlightConfiguration(this, id) with the
    // source's stage activeness and raw name; the selected configuration is this rocket's
    // configuration of the source's selected id.

    m_perfectFinish = source.m_perfectFinish;

    checkComponentStructure();

    fireComponentChangeEvent(type);
}

}  // namespace QtRocket
