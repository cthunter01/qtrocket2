#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationCellText.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using Scope = UserPrompts::ConfigurationScope;

/// What a component's kind is called in the BugError of an id that is not one.
template <typename T>
[[nodiscard]] constexpr const char* kindName() noexcept
{
    if constexpr (std::is_same_v<T, MotorMount>)
    {
        return "a motor mount";
    }
    else if constexpr (std::is_same_v<T, RecoveryDevice>)
    {
        return "a recovery device";
    }
    else
    {
        return "a stage";
    }
}

/// The ignition values of one entry of a mount's motor configurations, to tell a change.
struct IgnitionEntry
{
    FlightConfigurationId id;
    IgnitionEvent         event      = IgnitionEvent::AUTOMATIC;
    double                delay      = 0.0;
    bool                  overridden = false;

    [[nodiscard]] bool operator==(const IgnitionEntry& other) const noexcept = default;
};

/// Every entry of @p mount's motor configurations (the default first) with its ignition.
[[nodiscard]] std::vector<IgnitionEntry> ignitionEntries(const MotorMount& mount)
{
    const MotorConfigurationSet& set = mount.getMotorConfigurationSet();
    std::vector<IgnitionEntry>   entries;
    const auto add = [&entries](const FlightConfigurationId& id, const MotorConfiguration& c) {
        entries.push_back(IgnitionEntry{.id         = id,
                                        .event      = c.getIgnitionEvent(),
                                        .delay      = c.getIgnitionDelay(),
                                        .overridden = c.hasIgnitionOverride()});
    };
    add(FlightConfigurationId::defaultValueId(), set.getDefault());
    for (const FlightConfigurationId& id : set.getIds())
    {
        add(id, set.get(id));
    }
    return entries;
}

/// The values of one entry of a deployment or separation set (every field: Java's equals()
/// leaves the separation's altitude out), to tell a change.
template <typename E>
struct ValueEntry
{
    FlightConfigurationId id;
    E                     value;
};

[[nodiscard]] bool sameDeployment(const DeploymentConfiguration& a,
                                  const DeploymentConfiguration& b) noexcept
{
    return a.getDeployEvent() == b.getDeployEvent() &&
           a.getDeployAltitude() == b.getDeployAltitude() &&
           a.getDeployDelay() == b.getDeployDelay();
}

[[nodiscard]] bool sameSeparation(const StageSeparationConfiguration& a,
                                  const StageSeparationConfiguration& b) noexcept
{
    return a.getSeparationEvent() == b.getSeparationEvent() &&
           a.getSeparationAltitude() == b.getSeparationAltitude() &&
           a.getSeparationDelay() == b.getSeparationDelay();
}

/// Every entry of @p set (the default first) with its value.
template <typename Set>
[[nodiscard]] auto valueEntries(const Set& set)
{
    using E = std::remove_cvref_t<decltype(set.getDefault())>;
    std::vector<ValueEntry<E>> entries;
    entries.push_back(ValueEntry<E>{.id    = FlightConfigurationId::defaultValueId(),
                                    .value = set.getDefault().clone()});
    for (const FlightConfigurationId& id : set.getIds())
    {
        entries.push_back(ValueEntry<E>{.id = id, .value = set.get(id).clone()});
    }
    return entries;
}

/// Whether two snapshots of valueEntries() differ, by @p same for the values.
template <typename E, typename Same>
[[nodiscard]] bool differ(const std::vector<ValueEntry<E>>& before,
                          const std::vector<ValueEntry<E>>& after, Same same)
{
    if (before.size() != after.size())
    {
        return true;
    }
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        if (before[i].id != after[i].id || !same(before[i].value, after[i].value))
        {
            return true;
        }
    }
    return false;
}

/// The snapshots (valueEntries()) of the sets @p setOf gives for @p components.
template <typename C, typename SetOf>
[[nodiscard]] auto snapshots(std::span<C* const> components, SetOf setOf)
{
    std::vector<decltype(valueEntries(setOf(*components.front())))> all;
    all.reserve(components.size());
    for (const C* component : components)
    {
        all.push_back(valueEntries(setOf(*component)));
    }
    return all;
}

/// Whether a set of @p components differs from its snapshot in @p before, by @p same.
template <typename C, typename Snapshots, typename SetOf, typename Same>
[[nodiscard]] bool anyChanged(const Snapshots& before, std::span<C* const> components, SetOf setOf,
                              Same same)
{
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        if (differ(before[i], valueEntries(setOf(*components[i])), same))
        {
            return true;
        }
    }
    return false;
}

/// RecoveryConfigurationPanel.selectDeployment()'s loop over the cells of @p devices x @p ids but
/// the first (@p first, ids.front()): each that differs from @p modified takes its delay, altitude
/// and event; for this configuration only (@p thisConfiguration), a cell without an override
/// first gets one made from the device's default. Whether a cell changed.
[[nodiscard]] bool copyDeployment(std::span<RecoveryDevice* const>       devices,
                                  const RecoveryDevice&                  first,
                                  std::span<const FlightConfigurationId> ids,
                                  bool thisConfiguration, const DeploymentConfiguration& modified)
{
    bool update = false;
    for (RecoveryDevice* device : devices)
    {
        for (const FlightConfigurationId& id : ids)
        {
            if (device == &first && id == ids.front())
            {
                continue;
            }
            auto& set = device->getDeploymentConfigurations();
            if (thisConfiguration && !set.containsId(id))
            {
                set.set(id, set.getDefault().clone());
            }
            DeploymentConfiguration& current = set.get(id);
            if (current == modified)
            {
                continue;
            }
            update = true;
            current.setDeployDelay(modified.getDeployDelay());
            current.setDeployAltitude(modified.getDeployAltitude());
            current.setDeployEvent(modified.getDeployEvent());
        }
    }
    return update;
}

/// SeparationConfigurationPanel.selectSeparation()'s loop, as copyDeployment(): the delay and the
/// event (not the altitude, as in Java), and the value stored again (Java's set(fcid,
/// currentConfig): for a cell without an override, the default it has just changed, which so
/// becomes the cell's override; Java stores the default object itself).
[[nodiscard]] bool copySeparation(std::span<AxialStage* const> stages, const AxialStage& first,
                                  std::span<const FlightConfigurationId> ids,
                                  bool                                   thisConfiguration,
                                  const StageSeparationConfiguration&    modified)
{
    bool update = false;
    for (AxialStage* stage : stages)
    {
        for (const FlightConfigurationId& id : ids)
        {
            if (stage == &first && id == ids.front())
            {
                continue;
            }
            auto& set = stage->getSeparationConfigurations();
            if (thisConfiguration && !set.containsId(id))
            {
                set.set(id, set.getDefault().clone());
            }
            StageSeparationConfiguration& current = set.get(id);
            if (current == modified)
            {
                continue;
            }
            update = true;
            current.setSeparationDelay(modified.getSeparationDelay());
            current.setSeparationEvent(modified.getSeparationEvent());
            set.set(id, current.clone());
        }
    }
    return update;
}

[[nodiscard]] const auto& deploymentsOf(const RecoveryDevice& device)
{
    return device.getDeploymentConfigurations();
}

[[nodiscard]] const auto& separationsOf(const AxialStage& stage)
{
    return stage.getSeparationConfigurations();
}

}  // namespace

FlightConfigurationEditing::FlightConfigurationEditing(OpenRocketDocument& document,
                                                       const GuiServices&  services,
                                                       QWidget* dialogParent, QObject* parent)
  : QObject(parent), m_document(&document), m_services(services), m_dialogParent(dialogParent)
{
}

FlightConfigurationEditing::~FlightConfigurationEditing() = default;

Rocket& FlightConfigurationEditing::rocket() const noexcept
{
    return m_document->getRocket();
}

void FlightConfigurationEditing::setOpenDocuments(OpenDocuments documents)
{
    m_openDocuments = std::move(documents);
}

std::vector<OpenRocketDocument*> FlightConfigurationEditing::openDocuments() const
{
    std::vector<OpenRocketDocument*> documents;
    if (m_openDocuments)
    {
        documents = m_openDocuments();
    }
    if (std::ranges::find(documents, m_document) == documents.end())
    {
        documents.push_back(m_document);
    }
    return documents;
}

// ====================================================================== the configurations

std::optional<FlightConfigurationId> FlightConfigurationEditing::newConfiguration(
    const NewConfigurationFollowUp& followUp)
{
    //: Not in OpenRocket: the undo step of New Configuration (OpenRocket adds none: its change
    //: joins the step before; bug 5 of the plan)
    startStep(tr("New Configuration"));

    Rocket& r = rocket();
    // Java: new FlightConfiguration(rocket, null), a random id and the default name.
    FlightConfiguration         configuration{r, FlightConfigurationId{}};
    const FlightConfigurationId id = configuration.getId();
    r.setFlightConfiguration(id, std::move(configuration));
    r.setSelectedConfiguration(id);
    auto simulation = std::make_shared<Simulation>(m_document, r, m_services.preferences());
    simulation->setName(m_document->getNextSimulationName());
    m_document->addSimulation(std::move(simulation));
    r.setSelectedConfiguration(id);

    fire(ComponentChangeEvent::kMotorChange, std::span<const FlightConfigurationId>{&id, 1});
    if (followUp)
    {
        const QPointer<FlightConfigurationEditing> alive(this);
        followUp(id);
        if (alive.isNull())
        {
            return id;  // the tab went during the follow-up's question
        }
    }
    // Java's "trigger select" (configurationChanged(NONFUNCTIONAL_CHANGE)).
    fire(ComponentChangeEvent::kNonFunctionalChange, std::nullopt);
    return id;
}

std::vector<FlightConfigurationId> FlightConfigurationEditing::duplicateConfigurations(
    std::span<const FlightConfigurationId> ids)
{
    if (ids.empty())
    {
        return {};
    }
    //: Not in OpenRocket: the undo step of Duplicate Configuration (OpenRocket adds none: its
    //: change joins the step before; bug 5 of the plan)
    startStep(tr("Duplicate Configuration"));

    Rocket&                                                            r = rocket();
    std::vector<std::pair<FlightConfigurationId, FlightConfiguration>> copies;
    copies.reserve(ids.size());
    for (const FlightConfigurationId& oldId : ids)
    {
        // Java: oldConfig.copy(null), which copies the motors into their mounts, then
        // copyFlightConfiguration() on every FlightConfigurableComponent.
        FlightConfiguration copy = r.getFlightConfiguration(oldId).copy(FlightConfigurationId{});
        const FlightConfigurationId               newId = copy.getId();
        std::vector<FlightConfigurableComponent*> configurables;
        for (RocketComponent& component : r.subtree(true))
        {
            if (auto* configurable = dynamic_cast<FlightConfigurableComponent*>(&component))
            {
                configurables.push_back(configurable);
            }
        }
        for (FlightConfigurableComponent* configurable : configurables)
        {
            configurable->copyFlightConfiguration(oldId, newId);
        }
        copies.emplace_back(newId, std::move(copy));
    }

    std::vector<FlightConfigurationId> newIds;
    newIds.reserve(copies.size());
    for (auto& [id, configuration] : copies)
    {
        newIds.push_back(id);
        r.setFlightConfiguration(id, std::move(configuration));
        r.setSelectedConfiguration(id);
        auto simulation = std::make_shared<Simulation>(m_document, r, m_services.preferences());
        simulation->setName(m_document->getNextSimulationName());
        m_document->addSimulation(std::move(simulation));
    }
    r.setSelectedConfiguration(newIds.front());

    fire(ComponentChangeEvent::kMotorChange, newIds);
    fire(ComponentChangeEvent::kNonFunctionalChange, std::nullopt);
    return newIds;
}

bool FlightConfigurationEditing::renameConfigurations(std::span<const FlightConfigurationId> ids)
{
    if (ids.empty())
    {
        return false;
    }
    const Preferences&          preferences = m_services.preferences();
    const FlightConfigurationId first       = ids.front();
    const std::string initName = rocket().getFlightConfiguration(first).getNameRaw(preferences);

    // As Java's, before the question, also when it is cancelled.
    //: Not translated in OpenRocket (FlightConfigurationPanel.java): the undo step of Rename
    //: Configuration
    startStep(tr("Rename configuration(s)"));

    const UserPrompts::RenameConfigurationRequest request{
        .name          = QtStrings::fromUtf8(initName),
        .saveAsDefault = [this](QWidget* dialog, const QString& name) {
            saveAsDefaultName(dialog, name);
        }};
    const QPointer<FlightConfigurationEditing>   alive(this);
    const UserPrompts::RenameConfigurationAnswer answer =
        m_services.prompts().askRenameConfiguration(m_dialogParent, request);
    if (alive.isNull())
    {
        return false;  // the tab (and its document) went during the question
    }

    Rocket& r = rocket();
    if (!r.containsFlightConfigurationId(first))
    {
        return false;  // gone during the question
    }
    switch (answer.action)
    {
        case UserPrompts::RenameAction::RENAME:
            r.getFlightConfiguration(first).setName(QtStrings::toUtf8(answer.name));
            break;
        case UserPrompts::RenameAction::RESET:
            r.getFlightConfiguration(first).setName({});  // Java: setName(null), the default name
            break;
        case UserPrompts::RenameAction::CANCEL:
            break;
    }

    const std::string newName = r.getFlightConfiguration(first).getNameRaw(preferences);
    bool              update  = newName != initName;
    for (const FlightConfigurationId& id : ids.subspan(1))
    {
        if (!r.containsFlightConfigurationId(id))
        {
            continue;
        }
        FlightConfiguration& configuration = r.getFlightConfiguration(id);
        if (configuration.getNameRaw(preferences) != newName)
        {
            update = true;
            configuration.setName(newName);
        }
    }
    if (update)
    {
        fire(ComponentChangeEvent::kNonFunctionalChange, std::nullopt);
    }
    return update;
}

bool FlightConfigurationEditing::removeConfigurations(std::span<const FlightConfigurationId> ids)
{
    if (ids.empty())
    {
        return false;
    }
    //: Not translated in OpenRocket (FlightConfigurationPanel.java): the undo step of Delete
    //: Configuration
    startStep(tr("Remove configuration(s)"));
    for (const FlightConfigurationId& id : ids)
    {
        m_document->removeFlightConfigurationAndSimulations(id);
    }
    fire(ComponentChangeEvent::kNonFunctionalChange, std::nullopt);
    return true;
}

// ================================================================================ the motors

bool FlightConfigurationEditing::selectMotor(std::span<const FlightConfigurationId> ids,
                                             std::span<const Uuid>                  mounts)
{
    if (ids.empty() || mounts.empty())
    {
        return false;
    }
    if (std::ranges::any_of(ids, [](const FlightConfigurationId& id) { return id.isDefaultId(); }))
    {
        bug("Attempting to set a motor on the default FCID.");
    }
    static_cast<void>(find<MotorMount>(mounts, true));

    //: Not translated in OpenRocket (MotorConfigurationPanel.java): the undo step of Select
    //: motor
    startStep(tr("Select motor"));

    const std::vector<MotorMount*> before = find<MotorMount>(mounts.first(1), false);
    if (before.empty())
    {
        return false;
    }
    const QPointer<FlightConfigurationEditing>    alive(this);
    const std::optional<UserPrompts::MotorChoice> choice =
        m_services.prompts().chooseMotor(m_dialogParent, m_services, *before.front(), ids.front());
    if (alive.isNull() || !choice || choice->motor == nullptr)
    {
        return false;
    }
    const std::vector<FlightConfigurationId> kept = stillThere(ids);
    if (kept.empty())
    {
        return false;  // the first cell's configuration went during the question
    }

    bool update = false;
    for (MotorMount* mount : find<MotorMount>(mounts, false))
    {
        for (const FlightConfigurationId& id : kept)
        {
            const MotorConfiguration& templateConfig = mount->getMotorConfig(id);
            if (choice->motor != templateConfig.getMotor() ||
                choice->delay != templateConfig.getEjectionDelay() ||
                choice->nozzleExitDiameter != templateConfig.getNozzleExitDiameter())
            {
                update = true;
                MotorConfiguration configuration{*mount, id, templateConfig};
                configuration.setMotor(choice->motor);
                configuration.setEjectionDelay(choice->delay);
                // The chooser answers a diameter the motor takes (UserPrompts::MotorChoice).
                if (Result<void> set =
                        configuration.setNozzleExitDiameter(choice->nozzleExitDiameter);
                    !set)
                {
                    bug("the motor chooser answered a nozzle exit diameter the motor refuses: " +
                        set.error().message);
                }
                mount->setMotorConfig(std::move(configuration), id);
            }
        }
    }
    if (update)
    {
        fire(ComponentChangeEvent::kMotorChange, kept);
    }
    return update;
}

bool FlightConfigurationEditing::deleteMotors(std::span<const FlightConfigurationId> ids,
                                              std::span<const Uuid>                  mounts)
{
    if (ids.empty() || mounts.empty())
    {
        return false;
    }
    static_cast<void>(find<MotorMount>(mounts, true));
    //: Not translated in OpenRocket (MotorConfigurationPanel.java): the undo step of Delete
    //: motor
    startStep(tr("Delete motor(s)"));
    for (MotorMount* mount : find<MotorMount>(mounts, false))
    {
        for (const FlightConfigurationId& id : ids)
        {
            mount->setMotorConfig(std::nullopt, id);
        }
    }
    fire(ComponentChangeEvent::kMotorChange, ids);
    return true;
}

bool FlightConfigurationEditing::selectIgnition(std::span<const FlightConfigurationId> ids,
                                                std::span<const Uuid>                  mounts)
{
    if (ids.empty() || mounts.empty())
    {
        return false;
    }
    const std::vector<MotorMount*>     validated = find<MotorMount>(mounts, true);
    const UserPrompts::IgnitionRequest request =
        ignitionRequest(rocket(), *validated.front(), ids.front(), m_services.preferences());

    //: Not translated in OpenRocket (MotorConfigurationPanel.java): the undo step of Select
    //: ignition
    startStep(tr("Select ignition"));

    const QPointer<FlightConfigurationEditing>       alive(this);
    const std::optional<UserPrompts::IgnitionChoice> choice =
        m_services.prompts().askIgnition(m_dialogParent, request);
    if (alive.isNull() || !choice)
    {
        return false;  // bug 3: Cancel and the close button change nothing
    }
    const std::vector<MotorMount*> found = find<MotorMount>(mounts, false);
    if (found.empty() || &asComponent(*found.front()) != rocket().findComponent(mounts.front()))
    {
        return false;  // the first cell's mount went during the question
    }
    const std::vector<FlightConfigurationId> kept = stillThere(ids);
    if (kept.empty())
    {
        return false;  // the first cell's configuration went during the question
    }

    std::vector<std::vector<IgnitionEntry>> before;
    before.reserve(found.size());
    for (const MotorMount* mount : found)
    {
        before.push_back(ignitionEntries(*mount));
    }

    MotorMount&                  first   = *found.front();
    const FlightConfigurationId& firstId = kept.front();
    applyIgnition(first, firstId, *choice);
    const bool thisConfiguration = choice->scope == Scope::THIS_CONFIGURATION;

    // Java's loop over the other cells (which runs after a Cancel too there: bug 3). Java's
    // "update" for the first cell (its values changed) is part of the comparison below.
    bool update = false;
    for (MotorMount* mount : found)
    {
        for (const FlightConfigurationId& id : kept)
        {
            if (mount == &first && id == firstId)
            {
                continue;
            }
            MotorConfigurationSet& set = mount->getMotorConfigurationSet();
            // Bug 4: the configuration is read after the override is made (Java reads it before,
            // and so writes the values into the mount's default).
            if (thisConfiguration && !set.containsId(id))
            {
                set.set(id, MotorConfiguration{*mount, id, set.getDefault()});
            }
            MotorConfiguration& current = mount->getMotorConfig(id);
            update                      = true;
            current.setIgnitionEvent(choice->event);
            current.setIgnitionDelay(choice->delay);
        }
    }
    for (std::size_t i = 0; i < found.size(); ++i)
    {
        update = update || ignitionEntries(*found[i]) != before[i];
    }
    if (update)
    {
        fire(ComponentChangeEvent::kMotorChange,
             thisConfiguration ? std::optional<std::span<const FlightConfigurationId>>{kept}
                               : std::nullopt);
    }
    return update;
}

bool FlightConfigurationEditing::resetIgnition(std::span<const FlightConfigurationId> ids,
                                               std::span<const Uuid>                  mounts)
{
    if (ids.empty() || mounts.empty())
    {
        return false;
    }
    static_cast<void>(find<MotorMount>(mounts, true));
    //: Not translated in OpenRocket (MotorConfigurationPanel.java): the undo step of Reset
    //: ignition
    startStep(tr("Reset ignition"));

    bool update = false;
    for (MotorMount* mount : find<MotorMount>(mounts, false))
    {
        for (const FlightConfigurationId& id : ids)
        {
            // Bug 4: a cell without a configuration of its own uses the mount's default already,
            // which Java's useDefaultIgnition() on getMotorConfig(id) would reset.
            if (!mount->getMotorConfigurationSet().containsId(id))
            {
                continue;
            }
            MotorConfiguration& configuration = mount->getMotorConfig(id);
            const IgnitionEvent initialEvent  = configuration.getIgnitionEvent();
            const double        initialDelay  = configuration.getIgnitionDelay();
            const bool          initialFlag   = configuration.hasIgnitionOverride();
            configuration.useDefaultIgnition();
            if (initialEvent != configuration.getIgnitionEvent() ||
                initialDelay != configuration.getIgnitionDelay() ||
                initialFlag != configuration.hasIgnitionOverride())
            {
                update = true;
            }
        }
    }
    if (update)
    {
        fire(ComponentChangeEvent::kMotorChange, ids);
    }
    return update;
}

// ===================================================================== recovery and stages

bool FlightConfigurationEditing::selectDeployment(std::span<const FlightConfigurationId> ids,
                                                  std::span<const Uuid>                  devices)
{
    if (ids.empty() || devices.empty())
    {
        return false;
    }
    const std::vector<RecoveryDevice*>   validated = find<RecoveryDevice>(devices, true);
    const UserPrompts::DeploymentRequest request =
        deploymentRequest(rocket(), *validated.front(), ids.front(), m_services.preferences());
    const DeploymentConfiguration initialConfig = request.initial.deployment;

    //: Not translated in OpenRocket (RecoveryConfigurationPanel.java): the undo step of Select
    //: deployment
    startStep(tr("Select deployment"));

    const QPointer<FlightConfigurationEditing>         alive(this);
    const std::optional<UserPrompts::DeploymentChoice> choice =
        m_services.prompts().askDeployment(m_dialogParent, request);
    if (alive.isNull() || !choice)
    {
        return false;  // bug 3 (as the user's U-M3-5 has it for the other two questions)
    }
    const std::vector<RecoveryDevice*> found = find<RecoveryDevice>(devices, false);
    if (found.empty() ||
        &static_cast<RocketComponent&>(*found.front()) != rocket().findComponent(devices.front()))
    {
        return false;
    }
    const std::vector<FlightConfigurationId> kept = stillThere(ids);
    if (kept.empty())
    {
        return false;  // the first cell's configuration went during the question
    }

    const auto before = snapshots(std::span<RecoveryDevice* const>{found}, deploymentsOf);

    RecoveryDevice&              first             = *found.front();
    const FlightConfigurationId& firstId           = kept.front();
    const bool                   thisConfiguration = choice->scope == Scope::THIS_CONFIGURATION;
    // The dialog's OK action.
    if (thisConfiguration)
    {
        first.getDeploymentConfigurations().set(firstId, choice->deployment.clone());
    }
    else
    {
        first.getDeploymentConfigurations().setDefault(choice->deployment.clone());
    }

    // The panel's loop over the other cells; a change Java's "update" misses counts too.
    const DeploymentConfiguration modified =
        first.getDeploymentConfigurations().get(firstId).clone();
    bool update = !(initialConfig == modified);
    update      = copyDeployment(found, first, kept, thisConfiguration, modified) || update;
    update = update || anyChanged(before, std::span<RecoveryDevice* const>{found}, deploymentsOf,
                                  sameDeployment);
    if (update)
    {
        fire(ComponentChangeEvent::kAerodynamicChange,
             thisConfiguration ? std::optional<std::span<const FlightConfigurationId>>{kept}
                               : std::nullopt);
    }
    return update;
}

bool FlightConfigurationEditing::resetDeployment(std::span<const FlightConfigurationId> ids,
                                                 std::span<const Uuid>                  devices)
{
    if (ids.empty() || devices.empty())
    {
        return false;
    }
    static_cast<void>(find<RecoveryDevice>(devices, true));
    //: Not translated in OpenRocket (RecoveryConfigurationPanel.java): the undo step of Reset
    //: deployment
    startStep(tr("Reset deployment"));

    bool update = false;
    for (RecoveryDevice* device : find<RecoveryDevice>(devices, false))
    {
        auto& set = device->getDeploymentConfigurations();
        for (const FlightConfigurationId& id : ids)
        {
            const DeploymentConfiguration initialConfig = set.get(id).clone();
            set.reset(id);
            update = update || !sameDeployment(initialConfig, set.get(id));
        }
    }
    if (update)
    {
        fire(ComponentChangeEvent::kAerodynamicChange, ids);
    }
    return update;
}

bool FlightConfigurationEditing::selectSeparation(std::span<const FlightConfigurationId> ids,
                                                  std::span<const Uuid>                  stages)
{
    if (ids.empty() || stages.empty())
    {
        return false;
    }
    const std::vector<AxialStage*>       validated = find<AxialStage>(stages, true);
    const UserPrompts::SeparationRequest request =
        separationRequest(rocket(), *validated.front(), ids.front(), m_services.preferences());
    const StageSeparationConfiguration initialConfig = request.initial.separation;

    //: Not translated in OpenRocket (SeparationConfigurationPanel.java): the undo step of Select
    //: separation
    startStep(tr("Select separation"));

    const QPointer<FlightConfigurationEditing>         alive(this);
    const std::optional<UserPrompts::SeparationChoice> choice =
        m_services.prompts().askSeparation(m_dialogParent, request);
    if (alive.isNull() || !choice)
    {
        return false;  // bug 3: Cancel and the close button change nothing
    }
    const std::vector<AxialStage*> found = find<AxialStage>(stages, false);
    if (found.empty() ||
        &static_cast<RocketComponent&>(*found.front()) != rocket().findComponent(stages.front()))
    {
        return false;
    }
    const std::vector<FlightConfigurationId> kept = stillThere(ids);
    if (kept.empty())
    {
        return false;  // the first cell's configuration went during the question
    }

    const auto before = snapshots(std::span<AxialStage* const>{found}, separationsOf);

    AxialStage&                  first             = *found.front();
    const FlightConfigurationId& firstId           = kept.front();
    const bool                   thisConfiguration = choice->scope == Scope::THIS_CONFIGURATION;
    // The dialog's OK action (the dialog has set NEVER's delay to 0).
    if (thisConfiguration)
    {
        first.getSeparationConfigurations().set(firstId, choice->separation.clone());
    }
    else
    {
        first.getSeparationConfigurations().reset();
        first.getSeparationConfigurations().setDefault(choice->separation.clone());
    }

    // The panel's loop over the other cells; a change Java's "update" misses counts too (its
    // equals() leaves the altitude out).
    const StageSeparationConfiguration modified =
        first.getSeparationConfigurations().get(firstId).clone();
    bool update = !(initialConfig == modified);
    update      = copySeparation(found, first, kept, thisConfiguration, modified) || update;
    update = update ||
             anyChanged(before, std::span<AxialStage* const>{found}, separationsOf, sameSeparation);
    if (update)
    {
        fire(ComponentChangeEvent::kAeromassChange,
             thisConfiguration ? std::optional<std::span<const FlightConfigurationId>>{kept}
                               : std::nullopt);
    }
    return update;
}

bool FlightConfigurationEditing::resetSeparation(std::span<const FlightConfigurationId> ids,
                                                 std::span<const Uuid>                  stages)
{
    if (ids.empty() || stages.empty())
    {
        return false;
    }
    static_cast<void>(find<AxialStage>(stages, true));
    //: Not translated in OpenRocket (SeparationConfigurationPanel.java): the undo step of Reset
    //: separation
    startStep(tr("Reset separation"));

    bool update = false;
    for (AxialStage* stage : find<AxialStage>(stages, false))
    {
        auto& set = stage->getSeparationConfigurations();
        for (const FlightConfigurationId& id : ids)
        {
            const StageSeparationConfiguration initialConfig = set.get(id).clone();
            set.reset(id);
            update = update || !sameSeparation(initialConfig, set.get(id));
        }
    }
    if (update)
    {
        fire(ComponentChangeEvent::kAeromassChange, ids);
    }
    return update;
}

// ========================================================================== the motor mounts

bool FlightConfigurationEditing::setMotorMount(const Uuid& mount, bool motorMount)
{
    const std::span<const Uuid> ids{&mount, 1};
    if (find<MotorMount>(ids, true).front()->isMotorMount() == motorMount)
    {
        return false;
    }
    //: Not in OpenRocket: the undo step of the motor mount check box of the Motors tab
    //: (OpenRocket adds none; bug 5 of the plan)
    startStep(tr("Modify motor mount"));
    const std::vector<MotorMount*> found = find<MotorMount>(ids, false);
    if (found.empty())
    {
        return false;
    }
    found.front()->setMotorMount(motorMount);
    return true;
}

// ============================================================================ the questions

UserPrompts::IgnitionRequest FlightConfigurationEditing::ignitionRequest(
    const Rocket& rocket, const MotorMount& mount, const FlightConfigurationId& id,
    const Preferences& preferences)
{
    const MotorConfigurationSet& set = mount.getMotorConfigurationSet();
    // Bug 2: "this configuration" for a cell whose own configuration overrides the ignition
    // (Java: whenever the cell has a motor).
    const bool overridden = set.containsId(id) && set.get(id).hasIgnitionOverride();
    // The values the cell shows: its own configuration's when it overrides the ignition, else the
    // mount's default's ("Default (...)"), which "all configurations" edits; Java shows the cell's
    // own configuration's (and starts on "this configuration" for every cell with a motor, so
    // that an unchanged OK sets nothing there).
    const MotorConfiguration& shown = overridden ? set.get(id) : set.getDefault();
    return UserPrompts::IgnitionRequest{
        .configurationName =
            FlightConfigurationCellText::configurationName(rocket, id, preferences),
        .initial = UserPrompts::IgnitionChoice{
            .scope = overridden ? Scope::THIS_CONFIGURATION : Scope::ALL_CONFIGURATIONS,
            .event = shown.getIgnitionEvent(),
            .delay = shown.getIgnitionDelay()}};
}

UserPrompts::DeploymentRequest FlightConfigurationEditing::deploymentRequest(
    const Rocket& rocket, const RecoveryDevice& device, const FlightConfigurationId& id,
    const Preferences& preferences)
{
    const auto& set = device.getDeploymentConfigurations();
    // Bug 1: "this configuration" for a cell with an override (Java: always).
    return UserPrompts::DeploymentRequest{
        .configurationName =
            FlightConfigurationCellText::configurationName(rocket, id, preferences),
        .initial =
            UserPrompts::DeploymentChoice{
                .scope = set.containsId(id) ? Scope::THIS_CONFIGURATION : Scope::ALL_CONFIGURATIONS,
                .deployment = set.get(id).clone()},
        .bottomStage = RecoveryDeviceConfig::isInBottomStage(device)};
}

UserPrompts::SeparationRequest FlightConfigurationEditing::separationRequest(
    const Rocket& rocket, const AxialStage& stage, const FlightConfigurationId& id,
    const Preferences& preferences)
{
    const auto& set = stage.getSeparationConfigurations();
    // Bug 1, as for the deployment.
    return UserPrompts::SeparationRequest{
        .configurationName =
            FlightConfigurationCellText::configurationName(rocket, id, preferences),
        .initial = UserPrompts::SeparationChoice{
            .scope = set.containsId(id) ? Scope::THIS_CONFIGURATION : Scope::ALL_CONFIGURATIONS,
            .separation = set.get(id).clone()}};
}

// ============================================================================== the helpers

void FlightConfigurationEditing::startStep(const QString& description)
{
    Q_EMIT closeConfigDialogRequested();
    m_document->addUndoPosition(QtStrings::toUtf8(description));
}

void FlightConfigurationEditing::saveAsDefaultName(QWidget* dialog, const QString& name) const
{
    // The services' objects outlive the tab, which may go during the message (its event loop).
    Preferences& preferences = m_services.preferences();
    UserPrompts& prompts     = m_services.prompts();
    // O-M3-7: QtRocket's configurations read the default name when asked; Java's store the one
    // they were made with. The names are pinned first, so that only new configurations take the
    // new default, as Java's message says.
    for (OpenRocketDocument* document : openDocuments())
    {
        document->pinFlightConfigurationNames(preferences);
    }
    prompts.showMessage(
        dialog, UserPrompts::Message{
                    .severity = UserPrompts::Severity::INFORMATION,
                    //: RenameConfigDialog.dlg.saveDefault.title
                    .title = tr("Save as default"),
                    //: RenameConfigDialog.dlg.saveDefault.msg
                    .lines = {tr("This change will only affect new flight configurations.")}});
    preferences.setDefaultFlightConfigName(QtStrings::toUtf8(name));
}

std::vector<FlightConfigurationId> FlightConfigurationEditing::stillThere(
    std::span<const FlightConfigurationId> ids) const
{
    std::vector<FlightConfigurationId> kept;
    if (ids.empty() || !rocket().containsFlightConfigurationId(ids.front()))
    {
        return kept;
    }
    kept.reserve(ids.size());
    for (const FlightConfigurationId& id : ids)
    {
        if (rocket().containsFlightConfigurationId(id))
        {
            kept.push_back(id);
        }
    }
    return kept;
}

template <typename T>
std::vector<T*> FlightConfigurationEditing::find(std::span<const Uuid> ids, bool strict) const
{
    std::vector<T*> found;
    found.reserve(ids.size());
    for (const Uuid& id : ids)
    {
        RocketComponent* component = rocket().findComponent(id);
        auto*            typed     = dynamic_cast<T*>(component);
        if (typed == nullptr)
        {
            if (strict)
            {
                bug(std::string{"FlightConfigurationEditing: the id "} + id.toString() +
                    " is not " + kindName<T>() + " of the rocket");
            }
            continue;
        }
        found.push_back(typed);
    }
    return found;
}

void FlightConfigurationEditing::applyIgnition(MotorMount& mount, const FlightConfigurationId& id,
                                               const UserPrompts::IgnitionChoice& choice)
{
    MotorConfigurationSet& set = mount.getMotorConfigurationSet();
    if (choice.scope == Scope::ALL_CONFIGURATIONS)
    {
        // Java's OK with "Change all configurations using the default ignition event": the
        // mount's default and every configuration of the mount (the overridden ones too).
        MotorConfiguration& defaultConfiguration = mount.getDefaultMotorConfig();
        defaultConfiguration.setIgnitionDelay(choice.delay);
        defaultConfiguration.setIgnitionEvent(choice.event);
        for (MotorConfiguration& configuration : set.values())
        {
            configuration.setIgnitionDelay(choice.delay);
            configuration.setIgnitionEvent(choice.event);
        }
        return;
    }
    // Java's live edits of the combo box and the spinner: a value set only when it differs from
    // the cell's configuration's (its own, or the mount's default for a cell without one), on the
    // cell's own configuration, made from the mount's default when the cell has none (bug 4: Java
    // edits the default itself then). The cell's stored values, not the question's: a question
    // that started on "all configurations" showed the mount's default's values (bug 2), which the
    // cell's own configuration takes with "this configuration".
    const MotorConfiguration& stored       = mount.getMotorConfig(id);
    const bool                eventChanged = choice.event != stored.getIgnitionEvent();
    const bool                delayChanged = choice.delay != stored.getIgnitionDelay();
    if (!eventChanged && !delayChanged)
    {
        return;
    }
    if (!set.containsId(id))
    {
        set.set(id, MotorConfiguration{mount, id, set.getDefault()});
    }
    MotorConfiguration& configuration = set.get(id);
    if (eventChanged)
    {
        configuration.setIgnitionEvent(choice.event);
    }
    if (delayChanged)
    {
        configuration.setIgnitionDelay(choice.delay);
    }
}

void FlightConfigurationEditing::fire(int                                                   type,
                                      std::optional<std::span<const FlightConfigurationId>> ids)
{
    Q_EMIT changeAboutToFire();
    if (ids)
    {
        rocket().fireComponentChangeEvent(type, *ids);
    }
    else
    {
        rocket().fireComponentChangeEvent(type);
    }
}

}  // namespace QtRocket::Gui
