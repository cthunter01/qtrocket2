#pragma once

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class AxialStage;
class OpenRocketDocument;
class Preferences;

/// The root of every rocket component tree (OpenRocket's Rocket): it holds the change listeners,
/// the modification ids, the stage map and the flight configurations, dispatches every change
/// event of its tree, and keeps the design metadata.
///
/// Events: a new Rocket has events disabled (as in OpenRocket, so that building a rocket fires
/// nothing); enableEvents() switches them on and fires AEROMASS_CHANGE. fireComponentChangeEvent()
/// then, for each event of the tree:
/// 1. does nothing while events are disabled;
/// 2. unless the event is an undo change: draws a new modId() and copies it to the mass,
///    aerodynamic and tree ids for mass, aerodynamic and tree changes and to the functional id
///    for a functional change, which also draws new modification ids for the flight
///    configurations (those given, or all of them);
/// 3. while frozen (freeze()), queues the event and stops;
/// 4. calls componentChanged() on every component (pre-order, this rocket first, whose update()
///    renumbers the stages and updates every configuration), then updates the flight
///    configurations (those given, or all of them: their stage flags, motors and instance maps),
///    and emits the event to the listeners.
/// thaw() fires one event whose type is the OR of the queued ones and whose source is the last
/// queued event's source (found again by id; the rocket itself when it has left the tree).
///
/// The stage map numbers the stages: trackStage() gives a stage a free number, forgetStage()
/// drops it, and update() (on every event) renumbers the stages in tree order.
///
/// Flight configurations: a FlightConfigurableParameterSet of FlightConfiguration whose default
/// (the default id) is the "empty" configuration, and the selected configuration, which the GUI
/// shows and edits. getLength() and getBoundingBox() are the selected configuration's.
///
/// Deviations from OpenRocket:
/// - The listeners are a ComponentChangeSignal; a listener is removed with its connection.
///   resetListeners() disconnects them all; printListeners() is not ported.
/// - The stage map is ordered by number (Java's ConcurrentHashMap iterates small integer keys in
///   the same order).
/// - freeze() on a frozen rocket and thaw() on a thawed one are bugs (BugError); Java reports them
///   through the application's error handler and carries on.
/// - A copy of a frozen rocket is not frozen (Java's copy shares the freeze list).
/// - loadFrom() copies the source's children, so the source stays usable, and rebuilds the stage
///   map from this rocket's own stages (Java copies the source's map and repairs it on the next
///   update()).
/// - The stage map never keeps a stage that left the tree: removeChild() drops the removed stages
///   whatever its StageTracking, and update() rebuilds the map from the tree's stages. Java keeps
///   the entry of a stage removed without tracking (a stale but live object there) until its
///   number is reused; the same stages get the same numbers either way.
/// - The selected configuration is kept by id: getSelectedConfiguration() is the set's
///   configuration of that id, or the default when the set no longer holds one (Java keeps the
///   removed object selected until removeFlightConfiguration() or another selection).
/// - removeChild() drops the removed subtree from every configuration's instance maps and motors
///   at once (see FlightConfiguration), and loadFrom() updates the default configuration for the
///   loaded tree, so that no configuration refers to a destroyed component while events are
///   disabled.
/// - getId(int) is getFlightConfigurationId(int): RocketComponent::getId() is the component's
///   Uuid. createFlightConfiguration() without an argument is Java's null id (a new random id);
///   setFlightConfiguration() takes a std::optional for Java's null configuration.
/// - getTopmostStage() and getBottomCoreStage() take a reference (Java: a null configuration
///   gives null).
/// - toDebugConfigs() takes the Preferences that give the default configuration name.
class Rocket : public ComponentAssembly
{
public:
    using RocketComponent::fireComponentChangeEvent;
    using RocketComponent::getStage;
    using RocketComponent::isCompatible;

    /// The reference length when no symmetric component gives one (Java:
    /// DEFAULT_REFERENCE_LENGTH).
    static constexpr double kDefaultReferenceLength = 0.01;

    /// An empty rocket named "Rocket", positioned ABSOLUTE at the origin, with events disabled.
    Rocket();

    Rocket(const Rocket&)            = delete;
    Rocket& operator=(const Rocket&) = delete;
    Rocket(Rocket&&)                 = delete;
    Rocket& operator=(Rocket&&)      = delete;
    ~Rocket() override;

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::ROCKET; }

    /// A rocket accepts axial stages only (not booster sets: Java compares the class exactly).
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    // ------------------------------------------------------------------------- metadata

    [[nodiscard]] const std::string& getDesigner() const noexcept { return m_designer; }
    /// Sets the designer and fires NONFUNCTIONAL_CHANGE (always).
    void setDesigner(std::string_view designer);

    [[nodiscard]] DesignType getDesignType() const noexcept { return m_designType; }
    /// Sets the design type and fires NONFUNCTIONAL_CHANGE (always).
    void setDesignType(DesignType type);

    [[nodiscard]] const std::string& getKitName() const noexcept { return m_kitName; }
    /// Sets the kit name and fires NONFUNCTIONAL_CHANGE (always).
    void setKitName(std::string_view kitName);

    [[nodiscard]] const std::string& getRevision() const noexcept { return m_revision; }
    /// Sets the revision and fires NONFUNCTIONAL_CHANGE (always).
    void setRevision(std::string_view revision);

    [[nodiscard]] ReferenceType getReferenceType() const noexcept { return m_refType; }
    /// Sets the reference type; fires NONFUNCTIONAL_CHANGE when it changes.
    void setReferenceType(ReferenceType type);

    [[nodiscard]] double getCustomReferenceLength() const noexcept
    {
        return m_customReferenceLength;
    }
    /// Sets the custom reference length (at least 0.001 m); fires NONFUNCTIONAL_CHANGE when the
    /// reference type is CUSTOM. A value within MathUtil::equals() of the current one is ignored.
    void setCustomReferenceLength(double length);

    /// Whether the rocket has a perfect finish (a notable amount of laminar flow).
    [[nodiscard]] bool isPerfectFinish() const noexcept { return m_perfectFinish; }
    /// Sets the finish; fires AERODYNAMIC_CHANGE when it changes.
    void setPerfectFinish(bool perfectFinish);

    /// The document this rocket belongs to, or nullptr (non-owning; the document owns the rocket).
    [[nodiscard]] OpenRocketDocument* getDocument() const noexcept { return m_document; }
    void setDocument(OpenRocketDocument* document) noexcept { m_document = document; }

    // --------------------------------------------------------------- modification ids

    /// The id of the current state: drawn anew on every change except undo/redo (which restores
    /// the ids of the restored state). Use it to tell whether cached data is stale.
    [[nodiscard]] ModId getModId() const noexcept { return m_modId; }
    /// Changes with every mass change.
    [[nodiscard]] ModId getMassModId() const noexcept { return m_massModId; }
    /// Changes with every aerodynamic change.
    [[nodiscard]] ModId getAerodynamicModId() const noexcept { return m_aeroModId; }
    /// Changes with every tree change.
    [[nodiscard]] ModId getTreeModId() const noexcept { return m_treeModId; }
    /// Changes with every functional change (anything that is not non-functional).
    [[nodiscard]] ModId getFunctionalModId() const noexcept { return m_functionalModId; }

    // ------------------------------------------------------------------------- stages

    /// The number of tracked stages.
    [[nodiscard]] std::size_t getStageCount() const noexcept { return m_stageMap.size(); }

    /// The tracked stages, by stage number.
    [[nodiscard]] std::vector<AxialStage*> getStageList() const;

    /// The stage with number @p stageNumber, or nullptr.
    [[nodiscard]] AxialStage* getStage(int stageNumber) const noexcept;

    /// The tracked stage with the id @p stageId, or nullptr.
    [[nodiscard]] AxialStage* getStage(const Uuid& stageId) const noexcept;

    /// -1: the rocket belongs to no stage.
    [[nodiscard]] int getStageNumber() const override;

    /// Registers @p newStage: a stage already mapped under its number (the same class and id)
    /// keeps it (the entry is pointed at this object); otherwise the stage gets the smallest free
    /// number. Called by addChild(). The map holds a plain pointer: the stage must outlive its
    /// entry, which is only guaranteed for a stage of this rocket's tree (removeChild() and
    /// update() drop the others; Java's trackStage() is package-private).
    void trackStage(AxialStage& newStage);

    /// Removes the map entry under @p oldStage's number (whatever stage it holds, as in Java).
    /// Called by removeChild().
    void forgetStage(const AxialStage& oldStage);

    /// The topmost stage (booster sets included) active in @p config, in stage number order, or
    /// nullptr.
    [[nodiscard]] AxialStage* getTopmostStage(const FlightConfiguration& config) const;

    /// The last child stage of the rocket (a core stage) active in @p config, or nullptr.
    [[nodiscard]] AxialStage* getBottomCoreStage(const FlightConfiguration& config) const;

    // ------------------------------------------------------------------------- position

    /// Always ABSOLUTE (fires nothing).
    void setAxialMethod(AxialMethod newAxialMethod) override;

    /// Always 0: the rocket is the origin (fires nothing).
    void setAxialOffset(double requestedOffset) override;

    /// The selected configuration's length (FlightConfiguration::getLength(): the x extent of the
    /// bounds of its active components).
    [[nodiscard]] double getLength() const override;

    /// The selected configuration's box around its aerodynamic components
    /// (FlightConfiguration::getBoundingBoxAerodynamic()).
    [[nodiscard]] BoundingBox getBoundingBox() const;

    /// The largest getBoundingRadius() of the assemblies among the children (Java's Math.max:
    /// NaN when any of them is NaN).
    [[nodiscard]] double getBoundingRadius() const override;

    // -------------------------------------------------------------- flight configurations

    /// The selected configuration: the one the user interface shows and edits (simulations use
    /// their own copies).
    [[nodiscard]] FlightConfiguration&       getSelectedConfiguration();
    [[nodiscard]] const FlightConfiguration& getSelectedConfiguration() const;

    /// Selects the configuration of @p selectId (the default when the set has none) and fires
    /// NONFUNCTIONAL_CHANGE; nothing happens when its id is already selected.
    void setSelectedConfiguration(const FlightConfigurationId& selectId);

    /// The number of configurations, the default not counted (Java: getConfigurationCount() and
    /// getFlightConfigurationCount()).
    [[nodiscard]] int getConfigurationCount() const noexcept;
    [[nodiscard]] int getFlightConfigurationCount() const noexcept;

    /// The configuration ids, in creation order, the default left out.
    [[nodiscard]] std::vector<FlightConfigurationId> getIds() const;

    /// The id at @p configIndex in getIds() (Java: getId(int)).
    /// @throws BugError when @p configIndex is out of range (Java: IndexOutOfBoundsException).
    [[nodiscard]] FlightConfigurationId getFlightConfigurationId(int configIndex) const;

    /// Removes the configuration of @p fcid: selects the default when it was selected, resets
    /// @p fcid in every FlightConfigurableComponent, drops it from the set and fires
    /// NONFUNCTIONAL_CHANGE. The error id is ignored; the default id cannot be removed.
    void removeFlightConfiguration(const FlightConfigurationId& fcid);

    /// Whether the set has a configuration of @p id (false for the error id; true for the
    /// default id).
    [[nodiscard]] bool containsFlightConfigurationId(const FlightConfigurationId& id) const;

    /// Whether any acting motor mount of the rocket has a motor for @p fcid (false for the error
    /// id).
    [[nodiscard]] bool hasMotors(const FlightConfigurationId& fcid) const;

    /// The configuration of @p fcid, made (and TREE_CHANGE fired) when the set has none; the
    /// default for the error id.
    FlightConfiguration& createFlightConfiguration(const FlightConfigurationId& fcid);

    /// A new configuration with a new random id (Java: createFlightConfiguration(null)); fires
    /// TREE_CHANGE.
    FlightConfiguration& createFlightConfiguration();

    /// Every configuration.
    [[nodiscard]] FlightConfigurableParameterSet<FlightConfiguration>&
    getFlightConfigurations() noexcept
    {
        return m_configSet;
    }
    [[nodiscard]] const FlightConfigurableParameterSet<FlightConfiguration>&
    getFlightConfigurations() const noexcept
    {
        return m_configSet;
    }

    /// The configuration of @p fcid, or the default when the set has none.
    [[nodiscard]] FlightConfiguration& getFlightConfiguration(const FlightConfigurationId& fcid);
    [[nodiscard]] const FlightConfiguration& getFlightConfiguration(
        const FlightConfigurationId& fcid) const;

    /// The configuration at @p configIndex in getIds(); with @p allowDefault, index 0 is the
    /// default and the others shift by one.
    /// @throws BugError when the index is out of range.
    [[nodiscard]] FlightConfiguration& getFlightConfigurationByIndex(int  configIndex,
                                                                     bool allowDefault = false);

    /// Stores @p newConfig under @p fcid and fires NONFUNCTIONAL_CHANGE; nullopt removes the
    /// configuration of @p fcid (Java: null). Nothing happens for the error id, nor when the set
    /// already holds a configuration with the id @p fcid.
    void setFlightConfiguration(const FlightConfigurationId&       fcid,
                                std::optional<FlightConfiguration> newConfig);

    /// The default configuration.
    [[nodiscard]] FlightConfiguration&       getEmptyConfiguration() noexcept;
    [[nodiscard]] const FlightConfiguration& getEmptyConfiguration() const noexcept;

    /// "====== Dumping <n> Configurations from rocket: <name> ======" and one line per
    /// configuration, the default first: its short key ("=>" before the selected one's) and its
    /// raw name.
    [[nodiscard]] std::string toDebugConfigs(const Preferences& preferences) const;

    // --------------------------------------------------------------------------- events

    /// Adds a listener for every change event of this tree.
    ComponentChangeSignal::Connection addComponentChangeListener(
        ComponentChangeSignal::Slot slot) override;

    /// Disconnects every listener (Java: resetListeners()).
    void resetListeners() noexcept { m_listeners.disconnectAll(); }

    /// The number of connected listeners.
    [[nodiscard]] std::size_t getListenerCount() const noexcept { return m_listeners.size(); }

    /// Fires an event of @p type from this rocket that updates only the flight configurations
    /// @p ids (see the class comment).
    void fireComponentChangeEvent(int type, std::span<const FlightConfigurationId> ids);

    /// fireComponentChangeEvent(type, {id}).
    void fireComponentChangeEvent(int type, const FlightConfigurationId& id);

    /// Whether events are dispatched.
    [[nodiscard]] bool isEventsEnabled() const noexcept { return m_eventsEnabled; }

    /// Enables events (firing AEROMASS_CHANGE) and update()s the rocket.
    void enableEvents();

    /// Enables events, firing AEROMASS_CHANGE when they were disabled, or disables them.
    void enableEvents(bool enable);

    /// Queues the events of the tree instead of dispatching them, until thaw(). Always pair it
    /// with thaw(), also when an exception is thrown in between.
    /// @throws BugError when the rocket is already frozen.
    void freeze();

    /// Ends freeze() and fires one event combining the queued ones (nothing when none was
    /// queued).
    /// @throws BugError when the rocket is not frozen.
    void thaw();

    /// Whether the rocket is frozen.
    [[nodiscard]] bool isFrozen() const noexcept { return m_freezeList.has_value(); }

    /// Renumbers the stages in tree order, rebuilds the stage map from them, and updates every
    /// flight configuration.
    void update() override;

    // -------------------------------------------------------------------------- copying

    /// A deep copy with the original ids whose stage map points at the copied stages and whose
    /// flight configurations are rebuilt for the copy: a new default, and for every id a new
    /// configuration with the original's raw name and stage activeness; the selected id is kept
    /// (Java: copyWithOriginalID()). The listeners are not copied.
    [[nodiscard]] std::unique_ptr<RocketComponent> copyWithOriginalId() const override;

    /// copyWithOriginalId() as a Rocket.
    [[nodiscard]] std::unique_ptr<Rocket> copyRocketWithOriginalId() const;

    /// Replaces this rocket's structure with a copy of @p source's (undo/redo): the children,
    /// the fields RocketComponent::copyFrom() copies, the modification ids, the reference type
    /// and length, the stage map, the configurations (the overrides rebuilt for this rocket with
    /// the source's stage activeness and raw names; the default keeps its own, as in Java) with
    /// the source's selected id, and the finish. The designer, revision, kit name and design type
    /// are not copied, as in Java. Then fires UNDO_CHANGE | NONFUNCTIONAL_CHANGE | TREE_CHANGE,
    /// with MASS_CHANGE / AERODYNAMIC_CHANGE when the mass / aerodynamic ids differ; the previous
    /// components are destroyed after the event has been delivered.
    void loadFrom(const Rocket& source);

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    /// The dispatch described in the class comment, from any component of the tree.
    void fireComponentChangeEvent(const ComponentChangeEvent& event) override;

private:
    /// The key of the copy constructor, which only Rocket can name.
    struct CopyKey
    {
        explicit CopyKey() = default;
    };

public:
    /// The copy cloneShallow() makes (Java's clone()): every field but the listeners, the freeze
    /// state, the stage map and the flight configurations (a new default only), which
    /// copyWithOriginalId() rebuilds. Only Rocket can call it (the key type is private).
    Rocket(const Rocket& other, CopyKey key);

private:
    // removeChild() calls forgetStageEntries() and forgetComponents().
    friend class RocketComponent;

    /// An event held back by freeze(): its type and its source's id, by which thaw() finds the
    /// source again.
    struct FrozenEvent
    {
        int  type{0};
        Uuid sourceId;
    };

    void fireComponentChangeEvent(const ComponentChangeEvent&                           event,
                                  std::optional<std::span<const FlightConfigurationId>> ids);

    /// The smallest stage number not in the map.
    [[nodiscard]] int getNewStageNumber() const;

    /// Removes every map entry that holds @p stage, whatever its number (removeChild()).
    void forgetStageEntries(const AxialStage& stage) noexcept;

    /// Renumbers the stages in tree order.
    void updateStageNumbers();

    /// Tracks every stage of the tree.
    void updateStageMap();

    /// Rebuilds the stage map from @p source's map, finding each stage in this tree by id.
    void rebuildStageMap(const Rocket& source);

    /// Draws new modification ids for the configurations in @p ids (all of them for nullopt).
    void updateConfigurationsModId(std::optional<std::span<const FlightConfigurationId>> ids);

    /// Updates the configurations in @p ids (all of them for nullopt).
    void updateConfigurations(std::optional<std::span<const FlightConfigurationId>> ids);

    /// Rebuilds the overrides of the configuration set for this rocket from @p source's: for every
    /// id, a new configuration with the source's stage activeness and raw name (Rocket.java's loop
    /// in copyWithOriginalID() and loadFrom()).
    void rebuildConfigurations(const FlightConfigurableParameterSet<FlightConfiguration>& source);

    /// Called by removeChild() when @p removed (just detached) has left this rocket: its components
    /// leave every configuration.
    void forgetComponents(const RocketComponent& removed);

    ComponentChangeSignal                   m_listeners;
    std::optional<std::vector<FrozenEvent>> m_freezeList;

    ModId m_modId;
    ModId m_massModId;
    ModId m_aeroModId;
    ModId m_treeModId;
    ModId m_functionalModId;

    bool m_eventsEnabled{false};

    OpenRocketDocument* m_document{nullptr};
    ReferenceType       m_refType{ReferenceType::MAXIMUM};
    double              m_customReferenceLength{kDefaultReferenceLength};

    std::string m_designer;
    std::string m_revision;
    DesignType  m_designType{DesignType::ORIGINAL};
    std::string m_kitName;

    std::map<int, AxialStage*> m_stageMap;

    bool m_perfectFinish{false};

    /// The flight configurations; declared after the stage map, which the default configuration
    /// reads when it is made.
    FlightConfigurableParameterSet<FlightConfiguration> m_configSet;
    /// The id of the selected configuration (see the class comment).
    FlightConfigurationId m_selectedConfigurationId{FlightConfigurationId::defaultValueId()};
};

}  // namespace QtRocket
