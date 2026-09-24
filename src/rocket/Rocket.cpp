#include "QtRocket/rocket/Rocket.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/util/BoundingBox.h"
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
    m_functionalModId(m_modId),
    // The default configuration, the default of the set and the selected one; it reads the
    // (empty) stage map and tree, which are initialised before it.
    m_configSet(FlightConfiguration{*this, FlightConfigurationId::defaultValueId()})
{
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
    m_perfectFinish(other.m_perfectFinish),
    m_configSet(FlightConfiguration{*this, FlightConfigurationId::defaultValueId()}),
    m_selectedConfigurationId(other.m_selectedConfigurationId)
{
    // Not copied: the listeners, the freeze state, the stage map and the configurations (rebuilt
    // by copyWithOriginalId() once the children exist).
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

void Rocket::forgetStageEntries(const AxialStage& stage) noexcept
{
    std::erase_if(m_stageMap, [&stage](const auto& entry) { return entry.second == &stage; });
}

AxialStage* Rocket::getTopmostStage(const FlightConfiguration& config) const
{
    for (AxialStage* stage : getStageList())
    {
        if (config.isStageActive(stage->getStageNumber()))
        {
            return stage;
        }
    }
    return nullptr;
}

AxialStage* Rocket::getBottomCoreStage(const FlightConfiguration& config) const
{
    for (const auto& child : std::views::reverse(m_children))
    {
        auto* stage = dynamic_cast<AxialStage*>(child.get());
        if (stage != nullptr && config.isStageActive(stage->getStageNumber()))
        {
            return stage;
        }
    }
    return nullptr;
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
    return getSelectedConfiguration().getLength();
}

BoundingBox Rocket::getBoundingBox() const
{
    return getSelectedConfiguration().getBoundingBoxAerodynamic();
}

double Rocket::getBoundingRadius() const
{
    double bounding = 0;
    for (const auto& comp : m_children)
    {
        if (const auto* assembly = dynamic_cast<const ComponentAssembly*>(comp.get()))
        {
            bounding = MathUtil::javaMax(bounding, assembly->getBoundingRadius());
        }
    }
    return bounding;
}

// ================================================================== flight configurations

FlightConfiguration& Rocket::getSelectedConfiguration()
{
    return m_selectionOrphaned ? m_configSet.getDefault()
                               : m_configSet.get(m_selectedConfigurationId);
}

const FlightConfiguration& Rocket::getSelectedConfiguration() const
{
    return m_selectionOrphaned ? m_configSet.getDefault()
                               : m_configSet.get(m_selectedConfigurationId);
}

void Rocket::setSelectedConfiguration(const FlightConfigurationId& selectId)
{
    // Java compares with the selected object's id, also when that object has left the set.
    if (selectId == m_selectedConfigurationId)
    {
        // The configuration is already selected: no event.
        return;
    }
    m_selectedConfigurationId = m_configSet.get(selectId).getId();
    m_selectionOrphaned       = false;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

int Rocket::getConfigurationCount() const noexcept
{
    return static_cast<int>(m_configSet.size());
}

int Rocket::getFlightConfigurationCount() const noexcept
{
    return static_cast<int>(m_configSet.size());
}

std::vector<FlightConfigurationId> Rocket::getIds() const
{
    return m_configSet.getIds();
}

FlightConfigurationId Rocket::getFlightConfigurationId(int configIndex) const
{
    const std::vector<FlightConfigurationId> idList = m_configSet.getIds();
    if (configIndex < 0 || std::cmp_greater_equal(configIndex, idList.size()))
    {
        bug(std::format("flight configuration index {} out of range ({} configurations)",
                        configIndex, idList.size()));
    }
    return idList[static_cast<std::size_t>(configIndex)];
}

void Rocket::removeFlightConfiguration(const FlightConfigurationId& fcid)
{
    if (fcid.hasError())
    {
        return;
    }
    if (m_selectedConfigurationId == fcid)
    {
        m_selectedConfigurationId = FlightConfigurationId::defaultValueId();
        m_selectionOrphaned       = false;
    }

    // Every component configuration tied to this id goes too.
    forEach([&fcid](RocketComponent& comp) {
        if (auto* configurable = dynamic_cast<FlightConfigurableComponent*>(&comp))
        {
            configurable->reset(fcid);
        }
    });

    m_configSet.reset(fcid);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

bool Rocket::containsFlightConfigurationId(const FlightConfigurationId& id) const
{
    if (id.hasError())
    {
        return false;
    }
    return m_configSet.containsId(id);
}

bool Rocket::hasMotors(const FlightConfigurationId& fcid) const
{
    if (fcid.hasError())
    {
        return false;
    }
    bool found = false;
    forEach([&fcid, &found](const RocketComponent& c) {
        const auto* mount = dynamic_cast<const MotorMount*>(&c);
        if (mount != nullptr && mount->isMotorMount() &&
            mount->getMotorConfig(fcid).getMotor() != nullptr)
        {
            found = true;
        }
    });
    return found;
}

FlightConfiguration& Rocket::createFlightConfiguration(const FlightConfigurationId& fcid)
{
    if (fcid.hasError())
    {
        return m_configSet.getDefault();
    }
    if (m_configSet.containsId(fcid))
    {
        return m_configSet.get(fcid);
    }
    m_configSet.set(fcid, FlightConfiguration{*this, fcid});
    fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    return m_configSet.get(fcid);
}

FlightConfiguration& Rocket::createFlightConfiguration()
{
    // Java's null id: a configuration with a fresh random id.
    const FlightConfigurationId fcid;
    m_configSet.set(fcid, FlightConfiguration{*this, fcid});
    fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    return m_configSet.get(fcid);
}

FlightConfiguration& Rocket::getFlightConfiguration(const FlightConfigurationId& fcid)
{
    return m_configSet.get(fcid);
}

const FlightConfiguration& Rocket::getFlightConfiguration(const FlightConfigurationId& fcid) const
{
    return m_configSet.get(fcid);
}

FlightConfiguration& Rocket::getFlightConfigurationByIndex(int configIndex, bool allowDefault)
{
    if (allowDefault)
    {
        if (0 == configIndex)
        {
            return m_configSet.getDefault();
        }
        --configIndex;
    }
    return m_configSet.get(getFlightConfigurationId(configIndex));
}

void Rocket::setFlightConfiguration(const FlightConfigurationId&       fcid,
                                    std::optional<FlightConfiguration> newConfig)
{
    if (fcid.hasError())
    {
        // Java logs "attempt to set a 'fcid = config' with a error fcid.  Ignored."
        return;
    }
    if (!newConfig)
    {
        m_configSet.reset(fcid);
        if (fcid == m_selectedConfigurationId && !m_configSet.containsId(fcid))
        {
            // Java keeps the removed configuration selected (see the class comment).
            m_selectionOrphaned = true;
        }
    }
    else if (fcid == m_configSet.get(fcid).getFlightConfigurationId())
    {
        // This mapping already exists: no event.
        return;
    }
    else
    {
        // OpenRocket's only caller (FlightConfigurationPanel) passes a configuration of this
        // rocket stored under its own id; anything else would be selected and found by the wrong
        // key, or refer to another rocket's components.
        QTROCKET_ASSERT(newConfig->getId() == fcid);
        QTROCKET_ASSERT(&newConfig->getRocket() == this);
        m_configSet.set(fcid, std::move(*newConfig));
    }
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

FlightConfiguration& Rocket::getEmptyConfiguration() noexcept
{
    return m_configSet.getDefault();
}

const FlightConfiguration& Rocket::getEmptyConfiguration() const noexcept
{
    return m_configSet.getDefault();
}

std::string Rocket::toDebugConfigs(const Preferences& preferences) const
{
    std::string buffer;
    std::format_to(std::back_inserter(buffer),
                   "====== Dumping {} Configurations from rocket: {} ======\n",
                   getConfigurationCount(), getName());
    for (const FlightConfiguration& config : m_configSet.values())
    {
        std::string shortKey = config.getId().toShortKey();
        // Java's equals(): the same id, also for a selection that has left the set.
        if (config.getId() == m_selectedConfigurationId)
        {
            shortKey.insert(0, "=>");
        }
        std::format_to(std::back_inserter(buffer), "    [{:>12}]: {}\n", shortKey,
                       config.getNameRaw(preferences));
    }
    return buffer;
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

void Rocket::fireComponentChangeEvent(int type, const FlightConfigurationId& id)
{
    fireComponentChangeEvent(type, std::span<const FlightConfigurationId>{&id, 1});
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
            updateConfigurationsModId(ids);
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
    updateConfigurations(ids);

    m_listeners.emit(event);
}

void Rocket::update()
{
    // Deviation: the map is rebuilt from the tree, so that it cannot keep a stage that left the
    // tree (see the class comment); Java forgets only the tree stages' previous numbers here.
    m_stageMap.clear();
    updateStageNumbers();
    updateStageMap();
    updateConfigurations(std::nullopt);
}

void Rocket::updateConfigurationsModId(std::optional<std::span<const FlightConfigurationId>> ids)
{
    for (FlightConfiguration& config : m_configSet.values())
    {
        if (!ids || std::ranges::find(*ids, config.getId()) != ids->end())
        {
            config.updateModId();
        }
    }
}

void Rocket::updateConfigurations(std::optional<std::span<const FlightConfigurationId>> ids)
{
    for (FlightConfiguration& config : m_configSet.values())
    {
        if (!ids || std::ranges::find(*ids, config.getId()) != ids->end())
        {
            config.update();
        }
    }
}

void Rocket::forgetComponents(const RocketComponent& removed)
{
    for (FlightConfiguration& config : m_configSet.values())
    {
        config.forgetComponents(removed);
    }
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

    // The flight configurations refer to the copy: a new default (it has different semantics),
    // then a new configuration for every id.
    copyRocket.m_configSet = FlightConfigurableParameterSet<FlightConfiguration>{
        FlightConfiguration{copyRocket, FlightConfigurationId::defaultValueId()}};
    copyRocket.rebuildConfigurations(m_configSet);
    // Java looks up the selected object's id, also when that object has left the set.
    copyRocket.m_selectedConfigurationId =
        copyRocket.m_configSet.get(m_selectedConfigurationId).getId();
    copyRocket.m_selectionOrphaned = false;
    return copy;
}

void Rocket::rebuildConfigurations(
    const FlightConfigurableParameterSet<FlightConfiguration>& source)
{
    for (const FlightConfigurationId& configId : source.getIds())
    {
        const FlightConfiguration& sourceConfig = source.get(configId);
        FlightConfiguration        newConfig{*this, configId};
        newConfig.setNameRaw(sourceConfig.getStoredName());
        newConfig.copyStageActiveness(sourceConfig);
        m_configSet.set(configId, std::move(newConfig));
    }
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

    // The configurations refer to this rocket. Java's setDefault(new FlightConfiguration(this))
    // keeps the default (it equals the new one), and the source's default id is skipped by set():
    // the default keeps its own activeness and name. Deviation: the default is updated for the
    // loaded tree here, so that it refers to no replaced component even with events disabled.
    m_configSet.reset();
    m_configSet.getDefault().update();
    rebuildConfigurations(source.m_configSet);
    m_selectedConfigurationId = m_configSet.get(source.m_selectedConfigurationId).getId();
    m_selectionOrphaned       = false;

    m_perfectFinish = source.m_perfectFinish;

    checkComponentStructure();

    fireComponentChangeEvent(type);
}

}  // namespace QtRocket
