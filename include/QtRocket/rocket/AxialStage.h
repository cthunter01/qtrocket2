#pragma once

#include <memory>
#include <string>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

namespace QtRocket
{

class FlightConfiguration;

/// A stage on the rocket's centerline (OpenRocket's AxialStage), and the base of the booster
/// sets (ParallelStage). A stage holds body components, is always positioned AFTER the previous
/// stage, has a stage number kept by the Rocket (0 for the topmost) and a separation
/// configuration per flight configuration.
///
/// Deviation: isLaunchStage() is false when no core stage is active in the configuration (Java:
/// NullPointerException).
///
/// Not ported: getRelativeToStage(), unused in OpenRocket, whose Java version decrements the
/// stored stage number as a side effect; the multi-edit config listener overrides
/// (addConfigListener() and friends), by decision.
class AxialStage : public ComponentAssembly, public virtual FlightConfigurableComponent
{
public:
    using RocketComponent::isCompatible;

    /// A stage with the default separation (ejection charge, no delay), positioned AFTER, stage
    /// number 0.
    AxialStage();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::AXIAL_STAGE;
    }

    /// Always true.
    [[nodiscard]] bool allowsChildren() const override;

    /// A stage accepts body components only (Java: BodyComponent.class.isAssignableFrom()).
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    /// The separation of every flight configuration.
    [[nodiscard]] FlightConfigurableParameterSet<StageSeparationConfiguration>&
    getSeparationConfigurations() noexcept
    {
        return m_separations;
    }
    [[nodiscard]] const FlightConfigurableParameterSet<StageSeparationConfiguration>&
    getSeparationConfigurations() const noexcept
    {
        return m_separations;
    }

    /// Makes @p fcid use the default separation.
    void reset(const FlightConfigurationId& fcid) override;

    /// Copies the separation of @p oldConfigId to @p newConfigId.
    void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                 const FlightConfigurationId& newConfigId) override;

    /// The separation of the rocket's selected configuration, for editing: when the selected
    /// configuration (other than the default one) uses the default separation, a copy of it is
    /// stored for the configuration first, so that editing it leaves the default alone.
    /// @throws BugError when the stage is not in a Rocket.
    [[nodiscard]] StageSeparationConfiguration& getSeparationConfiguration();

    /// Whether this stage is active in the rocket's selected configuration.
    /// @throws BugError when the stage is not in a Rocket.
    [[nodiscard]] bool isStageActive() const;

    /// Whether this stage is active in @p config.
    [[nodiscard]] bool isStageActive(const FlightConfiguration& config) const;

    /// Whether this stage is the launch stage of @p config: the bottom active core stage
    /// (Rocket::getBottomCoreStage()); a booster set overrides it.
    /// @throws BugError when the stage is not in a Rocket.
    [[nodiscard]] virtual bool isLaunchStage(const FlightConfiguration& config) const;

    /// The stage number (Java: the field the Rocket assigns).
    [[nodiscard]] int getStageNumber() const override;

    /// Sets the stage number; the Rocket numbers its stages.
    void setStageNumber(int newStageNumber) noexcept { m_stageNumber = newStageNumber; }

    /// Always true: a stage follows the previous one.
    [[nodiscard]] bool isAfter() const override;

    /// Whether a recovery device (a parachute or a streamer) belongs to this stage itself (not to
    /// a booster set inside it).
    [[nodiscard]] bool hasRecoveryDevice() const;

    /// The dump of the separation set (FlightConfigurableParameterSet::toDebug()).
    [[nodiscard]] std::string toDebugSeparation() const;

    /// The stage above this one: the previous child of the Rocket for a stage on the rocket, or
    /// the stage the parent belongs to (for a booster set); nullptr for the first stage or a
    /// detached one.
    [[nodiscard]] AxialStage* getUpperStage();

    /// The stage's line of toDebugTree(): name, stage number, length, position, absolute location,
    /// or one line per instance.
    void toDebugTreeNode(std::string& buffer, const std::string& indent) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    /// The separation configurations (Java: separations, cloned by copyWithOriginalID()).
    FlightConfigurableParameterSet<StageSeparationConfiguration> m_separations;
    /// The stage number, assigned by the Rocket (Java: stageNumber).
    int m_stageNumber{0};
};

}  // namespace QtRocket
