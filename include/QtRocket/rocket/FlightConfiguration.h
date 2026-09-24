#pragma once

#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Transformation.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class AxialStage;
class MotorMount;
class Preferences;
class Rocket;
class RocketComponent;

/// One flight configuration of a rocket (OpenRocket's FlightConfiguration): which stages fly
/// (per-stage flags), and, derived from them and the rocket's motor mounts, which motors fly and
/// where every active component instance sits. It is the element type of the Rocket's
/// FlightConfigurableParameterSet; the default configuration (the default id) is the rocket's
/// "empty" configuration.
///
/// State: the rocket (non-owning; the rocket owns its configurations, a clone() handed out must
/// not outlive the rocket it refers to), the id, the name, the stage flags (active or not, by
/// stage number, each remembering its stage's id), and the derived data update() rebuilds: the
/// motors of the active mounts, the active instances (an InstanceMap filled by the recursive walk
/// updateActiveInstances()), the extra render instances, and the cached bounds and reference
/// length, which are recomputed when the rocket's modification id changes.
///
/// Stage flags: update() rebuilds them from the rocket's stage map, keeping the activeness of
/// each stage by its id (a stage moved or renumbered keeps its flag; a new stage is active). A
/// stage is active when it is flagged active, is in the rocket's stage map and has children
/// (isStageActive()). The flag setters (setStageActive(), toggleStage(), ...) act on stage
/// numbers and, except for the "all stages" ones, refresh everything as Java's fireChangeEvent()
/// does. OpenRocket's Rocket updates every configuration on every change event of its tree (see
/// Rocket), so the derived data are only stale while the rocket's events are disabled.
///
/// Instance walk: for each instance i of a component (getInstanceCount(), getInstanceOffsets(),
/// getInstanceAngles()) the transformation is
/// parent * translation(position) * translation(offset[i]) * axialRotation(angle[i]); an active
/// component's instances go into getActiveInstances(), an inactive booster set whose own stage
/// is flagged active (a booster set without children) into getExtraRenderInstances(), and the
/// walk descends into every child, active or not, once per instance. See InstanceMap for the
/// order.
///
/// Name: the raw name is either the default (Preferences::getDefaultFlightConfigName(), OpenRocket
/// "[{motors}]") or one the user set; getName() substitutes the {motors}, {manufacturers} and
/// {cases} tags (OpenRocket's MotorConfigurationSubstitutor, through RocketDescriptor): a tag such
/// as "{motors}" or "{manufacturers | motors}" becomes, for every stage of the rocket in stage
/// number order, the data of the motors of that stage's own mounts ("" for an inactive stage,
/// "None" for an active stage without motors), joined by "; ", or "No motors" when no stage has
/// any. Several motors of one stage give "M1350-0" for one, "4× G77-0" for four equal ones and
/// "A8-3, 2× C6-5" for different ones (sorted); with several keys in one tag, the first key's
/// text is followed by the separator written between the keys and the later keys' first motor
/// only. A tag without a key is replaced too (by the same combination of nothing: "None" per
/// active stage, so "No motors"), but only while some tag contains a key.
///
/// Deviations from OpenRocket:
/// - The name is stored only when set: the default name is looked up in the Preferences passed to
///   getName(), getNameRaw() and isNameOverridden() (Java stores the preference's value in the
///   constructor, which differs only when the preference changes while the configuration lives).
///   getName() also takes the Preferences that choose between designation and common name
///   (Motor::getMotorName()), and so do toString() and the debug dumps that print it.
/// - getName()'s substitution stops after 100 rounds, or when a round changes nothing, where Java
///   loops for ever (only possible with braces in the motor data).
/// - Java's regular expressions are hand-written here: '.' excludes the line terminators \n, \r,
///   U+0085, U+2028 and U+2029, '\s' is [ \t\n\x0B\f\r], and '\b' is JDK 17's
///   (Strings::javaRegexWordBoundary(): '_', the Unicode letters and decimal digits, and a
///   non-spacing mark after one of them are word characters, so "×", "—", "·" or a no-break space
///   next to a key separate it while "é" or "µ" do not). The name is read as Java's decoder would
///   have read it, a byte of malformed UTF-8 as one U+FFFD.
/// - The motors keep the order in which updateMotors() found their mounts (Java: a HashMap in
///   hash order of the random motor configuration ids). The motor lists hold copies of the mounts'
///   configurations (Java shares the objects), refreshed by every update: see getAllMotors() for
///   how long a reference into them lives.
/// - A flag setter given a stage without a flag, or a sub-stage without one, ignores it where
///   Java logs an error or throws a NullPointerException; the instance walk counts a booster set
///   without a flag as inactive (Java: NullPointerException); getAllStages() leaves out the
///   flags whose stage has left the rocket (Java: null elements); toStageListDetail() prints
///   "null" for them (Java: NullPointerException).
/// - When a subtree leaves the rocket, the Rocket removes its components from every
///   configuration's instance maps and motors at once (forgetComponents()), so that no
///   configuration refers to a component its caller may destroy; Java keeps them until the next
///   update.
/// - The Java constructor with a null id takes a fresh random id: pass FlightConfigurationId{}.
/// - Java's configurationInstanceId is a public field; getConfigurationInstanceId() reads it.
///
/// Threads: the const geometry getters (getLength(), getLengthAerodynamic(), getReferenceLength(),
/// getReferenceArea(), getBounds(), getBoundingBox*()) refresh mutable caches, so they must not
/// run concurrently on one configuration, nor while it is cloned or copied; a simulation works on
/// its own clone(). Moving a configuration is allowed (a holder such as a simulation status may
/// reassign one); the Rocket's own configurations are never moved or assigned in place, since
/// the set keeps each one in its own cell (FlightConfigurableParameterSet).
class FlightConfiguration
{
public:
    /// The class name FlightConfigurableParameterSet::toDebug() prints.
    static constexpr std::string_view kTypeName = "FlightConfiguration";

    /// The default name (Java: DEFAULT_CONFIG_NAME).
    static constexpr std::string_view kDefaultConfigName = "[{motors}]";

    /// The text of an active stage without motors in a {motors} substitution
    /// (Rocket.motorCount.noStageMotors).
    static constexpr std::string_view kNoStageMotors = "None";

    /// The substitution when no stage has motors (Rocket.motorCount.Nomotor).
    static constexpr std::string_view kNoMotors = "No motors";

    /// The default configuration of @p rocket (the default id).
    explicit FlightConfiguration(Rocket& rocket);

    /// A configuration of @p rocket with the id @p fcid; its stages are all active, and its
    /// motors and instances are computed from the rocket as it is.
    FlightConfiguration(Rocket& rocket, const FlightConfigurationId& fcid);

    FlightConfiguration(const FlightConfiguration&)                = delete;
    FlightConfiguration& operator=(const FlightConfiguration&)     = delete;
    FlightConfiguration(FlightConfiguration&&) noexcept            = default;
    FlightConfiguration& operator=(FlightConfiguration&&) noexcept = default;
    ~FlightConfiguration()                                         = default;

    /// The rocket (non-owning).
    [[nodiscard]] Rocket&       getRocket() noexcept { return *m_rocket; }
    [[nodiscard]] const Rocket& getRocket() const noexcept { return *m_rocket; }

    // ---------------------------------------------------------------------- stage flags

    /// Flags every stage inactive, then refreshes the motors and instances.
    void clearAllStages();

    /// Flags every stage active, then refreshes the motors and instances.
    void setAllStages();

    /// Adds (or replaces) a flag for every flag of @p other, by stage number, then refreshes the
    /// motors and instances.
    void copyStages(const FlightConfiguration& other);

    /// Copies the activeness of every stage of @p other with the same number as one of this
    /// configuration's flags, then refreshes the motors and instances.
    void copyStageActiveness(const FlightConfiguration& other);

    /// Flags stage @p stageNumber inactive (its sub-stages too).
    void clearStage(int stageNumber);

    /// Flags stage @p stageNumber and every stage with a higher number inactive (without their
    /// sub-stages, so booster sets keep their own flags).
    void clearStagesBelow(int stageNumber);

    /// Flags every stage with a lower number than @p stageNumber inactive.
    void clearStagesAbove(int stageNumber);

    /// Flags every stage inactive, then stages 0 to @p stage's number (and their sub-stages)
    /// active: the stages of the rocket from the top through @p stage.
    void activateStagesThrough(const AxialStage& stage);

    /// Flags stage @p stageNumber active (not its sub-stages) and every other stage inactive.
    void setOnlyStage(int stageNumber);

    /// Flags stage @p stageNumber @p active, and its sub-stages too when @p activateSubStages,
    /// then refreshes everything (Java: _setStageActive()). A negative or unknown stage number is
    /// ignored (Java logs an error).
    void setStageActive(int stageNumber, bool active, bool activateSubStages);

    /// setStageActive(stageNumber, active, true).
    void setStageActive(int stageNumber, bool active);

    /// Inverts the flag of stage @p stageNumber, sets its sub-stages to the new value, then
    /// refreshes everything. A negative or unknown stage number is ignored.
    void toggleStage(int stageNumber);

    /// Whether stage @p stageNumber flies: true for -1 (the rocket itself); otherwise the stage
    /// must be in the rocket's stage map, have children, and be flagged active.
    [[nodiscard]] bool isStageActive(int stageNumber) const;

    /// Remembers the activeness of stage @p stageNumber for applyPreloadedStageActiveness() (the
    /// .ork loader reads it before the stages exist).
    void preloadStageActiveness(int stageNumber, bool isActive);

    /// Applies the preloaded activeness (without sub-stages), in stage number order, and forgets
    /// it; nothing when none was preloaded.
    void applyPreloadedStageActiveness();

    /// The preloaded activeness by stage number, or nullopt when there is none.
    [[nodiscard]] const std::optional<std::map<int, bool>>& getPreloadedStageActiveness()
        const noexcept
    {
        return m_preloadStageActiveness;
    }

    // ------------------------------------------------------------------------ components

    /// Every component of the rocket in pre-order, the rocket first.
    [[nodiscard]] std::vector<RocketComponent*> getAllComponents() const;

    /// Every component in an active stage (and the rocket) in pre-order; the subtree of an
    /// inactive component is skipped.
    [[nodiscard]] std::vector<RocketComponent*> getAllActiveComponents() const;

    /// The components on the centreline core stages, breadth first, the rocket left out: the
    /// active axial stages (exactly AxialStage, not booster sets) and their descendants, without
    /// entering booster sets and pod sets.
    [[nodiscard]] std::vector<RocketComponent*> getCoreComponents() const;

    /// The active stages and their components, breadth first from all the active stages at once,
    /// without entering nested stages (Java: the deprecated getActiveComponents(), which ignores
    /// the instancing of parent components; used for the motors and the reference length).
    [[nodiscard]] std::vector<RocketComponent*> getActiveComponents() const;

    /// Every instance of every active component (see the class comment). References, iterators
    /// and spans into the map live until the next update of this configuration: a flag setter,
    /// update(), or a change event of the rocket (and removeChild() drops the removed
    /// components' entries at once).
    [[nodiscard]] const InstanceMap& getActiveInstances() const noexcept
    {
        return m_activeInstances;
    }

    /// The instances drawn although not active: the booster sets without children whose stage is
    /// flagged active (OpenRocket issue #1980). Valid as long as getActiveInstances().
    [[nodiscard]] const InstanceMap& getExtraRenderInstances() const noexcept
    {
        return m_extraRenderInstances;
    }

    // ---------------------------------------------------------------------------- stages

    /// The stage of every flag, in stage number order (see the class comment for flags whose
    /// stage has left the rocket).
    [[nodiscard]] std::vector<AxialStage*> getAllStages() const;

    /// The active stages, in stage number order.
    [[nodiscard]] std::vector<AxialStage*> getActiveStages() const;

    /// getActiveStages().size().
    [[nodiscard]] int getActiveStageCount() const;

    /// The active stage with the highest number (the bottom one), or nullptr.
    [[nodiscard]] AxialStage* getBottomStage() const;

    /// The number of stage flags.
    [[nodiscard]] int getStageCount() const noexcept { return static_cast<int>(m_stages.size()); }

    /// Whether @p component is in an active stage (isStageActive(its stage number)).
    [[nodiscard]] bool isComponentActive(const RocketComponent& component) const;

    /// isComponentActive(asComponent(mount)) for a mount known by its interface (Java's
    /// isComponentActive(MotorMount), which a simulation calls with MotorClusterState::getMount()).
    /// A concrete mount class, which is both a RocketComponent and a MotorMount, takes the
    /// RocketComponent overload: the constraint keeps this one from making such calls ambiguous.
    template <std::same_as<MotorMount> Mount>
    [[nodiscard]] bool isComponentActive(const Mount& mount) const
    {
        return isMountActive(mount);
    }

    /// Whether an active stage has a recovery device of its own; false for the error id.
    [[nodiscard]] bool hasRecoveryDevice() const;

    // ------------------------------------------------------------------------- geometry

    /// The reference length of the rocket's reference type (see ReferenceType), cached until the
    /// rocket's modification id changes.
    [[nodiscard]] double getReferenceLength() const;

    /// pi (getReferenceLength() / 2)^2.
    [[nodiscard]] double getReferenceArea() const;

    /// The bounds of the aerodynamic components as corner points (Java: the deprecated
    /// getBounds(), getBoundingBoxAerodynamic().toCollection(): the maximum corner first).
    [[nodiscard]] std::vector<Coordinate> getBounds() const;

    /// The box around every active instance of the aerodynamic components; (0,0,0)-(1,0,0) when
    /// there is none. Recomputed on every call, as in Java.
    [[nodiscard]] BoundingBox getBoundingBoxAerodynamic() const;

    /// The box around every active instance of every component; (0,0,0)-(1,0,0) when there is
    /// none. Recomputed on every call, as in Java.
    [[nodiscard]] BoundingBox getBoundingBox() const;

    /// The x extent of getBoundingBoxAerodynamic()'s box (0 without components), cached until the
    /// rocket's modification id changes.
    [[nodiscard]] double getLengthAerodynamic() const;

    /// The x extent of getBoundingBox()'s box (0 without components), cached until the rocket's
    /// modification id changes.
    [[nodiscard]] double getLength() const;

    // ------------------------------------------------------------------------------ ids

    /// The id (Java: getFlightConfigurationID() and getId()).
    [[nodiscard]] const FlightConfigurationId& getFlightConfigurationId() const noexcept
    {
        return m_fcid;
    }
    [[nodiscard]] const FlightConfigurationId& getId() const noexcept { return m_fcid; }

    /// The serial number of this configuration object: every construction takes the next one
    /// (Java: the public field configurationInstanceId).
    [[nodiscard]] int getConfigurationInstanceId() const noexcept
    {
        return m_configurationInstanceId;
    }

    /// The modification id: drawn anew by the flag setters, addMotor() and updateModId(); zero for
    /// a new configuration.
    [[nodiscard]] ModId getModId() const noexcept { return m_modId; }
    /// getModId() (the Monitorable concept).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Draws a new modification id (the Rocket calls it on a functional change).
    void updateModId() { m_modId = ModId{}; }

    // ----------------------------------------------------------------------------- name

    /// Whether the raw name differs from the default name of @p preferences.
    [[nodiscard]] bool isNameOverridden(const Preferences& preferences) const;

    /// The raw name with its {motors}, {manufacturers} and {cases} tags substituted (see the
    /// class comment).
    [[nodiscard]] std::string getName(const Preferences& preferences) const;

    /// The name as set, or the default name of @p preferences.
    [[nodiscard]] std::string getNameRaw(const Preferences& preferences) const;

    /// The name as set, or nullopt while it is the default.
    [[nodiscard]] const std::optional<std::string>& getStoredName() const noexcept
    {
        return m_configurationName;
    }

    /// Sets the name: an empty one restores the default; the name of an invalid id (the error
    /// id) cannot be set.
    void setName(std::string_view newName);

    /// getName() (Java: toString()); FlightConfigurableParameterSet::toDebug(preferences) prints
    /// it.
    [[nodiscard]] std::string toString(const Preferences& preferences) const;

    // --------------------------------------------------------------------------- motors

    /// Adds @p motorConfig to the motors (replacing one with the same id) and draws a new
    /// modification id; the active motors are not touched. An empty configuration is added too
    /// (Java logs an error).
    void addMotor(const MotorConfiguration& motorConfig);

    /// Whether any mount has a motor in this configuration (as of the last update).
    [[nodiscard]] bool hasMotors() const noexcept { return !m_motors.empty(); }

    /// The motor configurations of the active acting mounts that have a motor, as of the last
    /// update, plus those added since with addMotor().
    ///
    /// These are copies, rebuilt by every update: references and iterators into the list become
    /// invalid on any flag setter, update(), addMotor(), clearAllMotors(), change event of the
    /// rocket or removal of a mount. Java hands out the mounts' own objects, which a simulation
    /// keeps across stage separations (MotorClusterState); a holder like that must keep its own
    /// copy of the MotorConfiguration, not a reference into this list.
    [[nodiscard]] const std::vector<MotorConfiguration>& getAllMotors() const noexcept
    {
        return m_motors;
    }

    /// The motors whose mount is active, as of the last update. Valid as long as getAllMotors().
    [[nodiscard]] const std::vector<MotorConfiguration>& getActiveMotors() const noexcept
    {
        return m_activeMotors;
    }

    /// The instances of the visible motor mounts with a motor whose nozzle (instance x + mount
    /// length + overhang) is the furthest aft, from the active and the extra render instances; a
    /// nozzle more than MathUtil::kEpsilon further aft replaces the ones found so far, one equal
    /// within MathUtil::equals() joins them.
    [[nodiscard]] InstanceMap getLowestMotorInstances() const;

    /// Resets every active acting mount's motor for this configuration (to the default), then
    /// refreshes the motors.
    void clearAllMotors();

    /// Rebuilds the stage flags, the motors and the instances.
    void update();

    // ------------------------------------------------------------------------ copying

    /// A configuration of @p rocket with this id, name, stage activeness (by stage number),
    /// preloaded activeness and modification id; the bounds and reference length caches start
    /// empty (Java copies the cached bounds, which nothing reads before they are recomputed).
    [[nodiscard]] FlightConfiguration clone(Rocket& rocket) const;

    /// clone(getRocket()).
    [[nodiscard]] FlightConfiguration clone() const;

    /// A configuration of the same rocket with the id @p newId: the stage flags, preloaded
    /// activeness, modification id and name are copied, and every motor of this configuration is
    /// copied to @p newId in its mount. Although const (FlightConfigurableParameter requires a
    /// const copy()), it therefore changes the rocket: each of those mounts gets a motor
    /// configuration for @p newId, as in Java.
    [[nodiscard]] FlightConfiguration copy(const FlightConfigurationId& newId) const;

    /// Java's equals(): the same id.
    [[nodiscard]] bool operator==(const FlightConfiguration& other) const noexcept
    {
        return m_fcid == other.m_fcid;
    }

    /// Java's hashCode(): the id's.
    [[nodiscard]] std::int32_t hashCode() const noexcept { return m_fcid.hashCode(); }

    // --------------------------------------------------------------------------- debug

    /// "<short key> (#<instance id>) <name>".
    [[nodiscard]] std::string toDebug(const Preferences& preferences) const;

    /// A table of the stage flags: stage id, on/off and the stage's name.
    [[nodiscard]] std::string toStageListDetail(const Preferences& preferences) const;

    /// A table of the motors: active or not, and MotorConfiguration::toDebugDetail().
    [[nodiscard]] std::string toMotorDetail(const Preferences& preferences) const;

private:
    friend class Rocket;

    /// A stage's flag (Java: StageFlags).
    struct StageFlags
    {
        bool active{true};
        int  stageNumber{-1};
        Uuid stageId;
    };

    /// Java's fireChangeEvent(): a new modification id, invalid caches, then update().
    void fireChangeEvent();

    /// isComponentActive() of @p mount's component.
    [[nodiscard]] bool isMountActive(const MotorMount& mount) const;

    /// Rebuilds the flags from the rocket's stages, keeping each stage's activeness by id.
    void updateStages();

    /// Rebuilds the motors from the active acting mounts, and the active motors.
    void updateMotors();

    /// Rebuilds the active and extra render instances.
    void updateActiveInstances();

    /// Adds the instances of @p component and its subtree under @p parentTransform.
    void addActiveContexts(RocketComponent& component, const Transformation& parentTransform);

    /// Sets every flag to @p active, then refreshes the motors and instances.
    void setAllStagesActive(bool active);

    /// Recomputes the cached bounds and lengths.
    void calculateBounds() const;

    /// The name as a setter would store it: nullopt restores the default, anything else goes
    /// through setName().
    void setNameRaw(const std::optional<std::string>& name);

    /// Called by the Rocket when the subtree of @p removedRoot (just detached) has left it: drops
    /// its components from the instance maps and the motors of its mounts.
    void forgetComponents(const RocketComponent& removedRoot);

    /// The default name of @p preferences (Java: getDefaultName()).
    [[nodiscard]] static std::string getDefaultName(const Preferences& preferences);

    Rocket*                            m_rocket;
    FlightConfigurationId              m_fcid;
    std::optional<std::string>         m_configurationName;
    std::map<int, StageFlags>          m_stages;
    std::vector<MotorConfiguration>    m_motors;
    std::vector<MotorConfiguration>    m_activeMotors;
    std::optional<std::map<int, bool>> m_preloadStageActiveness;
    InstanceMap                        m_activeInstances;
    InstanceMap                        m_extraRenderInstances;

    mutable ModId       m_boundsModId{ModId::invalid()};
    mutable BoundingBox m_cachedBoundsAerodynamic;
    mutable BoundingBox m_cachedBounds;
    mutable double      m_cachedLengthAerodynamic{-1};
    mutable double      m_cachedLength{-1};
    mutable ModId       m_refLengthModId{ModId::invalid()};
    mutable double      m_cachedRefLength{-1};

    ModId m_modId{ModId::zero()};
    int   m_configurationInstanceId;
};

}  // namespace QtRocket
