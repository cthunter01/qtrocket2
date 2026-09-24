#pragma once

namespace QtRocket
{

class FlightConfigurationId;

/// A component with parameters that can differ per flight configuration (OpenRocket's
/// FlightConfigurableComponent: stages with their separation, recovery devices with their
/// deployment, motor mounts with their motors). A pure interface, inherited `public virtual` (see
/// AxialPositionable).
class FlightConfigurableComponent
{
public:
    virtual ~FlightConfigurableComponent() = default;

    FlightConfigurableComponent& operator=(const FlightConfigurableComponent&) = delete;
    FlightConfigurableComponent& operator=(FlightConfigurableComponent&&)      = delete;

    /// Copies the parameters of @p oldConfigId to @p newConfigId, also when @p oldConfigId has
    /// no value of its own (it then uses the default's).
    virtual void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                         const FlightConfigurationId& newConfigId) = 0;

    /// Makes @p fcid use the default parameter value again.
    virtual void reset(const FlightConfigurationId& fcid) = 0;

protected:
    FlightConfigurableComponent()                                   = default;
    FlightConfigurableComponent(const FlightConfigurableComponent&) = default;
    FlightConfigurableComponent(FlightConfigurableComponent&&)      = default;
};

}  // namespace QtRocket
