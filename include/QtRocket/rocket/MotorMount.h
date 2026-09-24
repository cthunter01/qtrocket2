#pragma once

#include <optional>
#include <string>

#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightConfigurationId;
class MotorConfiguration;
class MotorConfigurationSet;
class Preferences;

/// A component that can hold motors (OpenRocket's MotorMount: body tubes and inner tubes). It
/// keeps a MotorConfigurationSet, the motor of each flight configuration, whose default is always
/// "no motor", and whether it acts as a motor mount at all (a tube is a structural component until
/// it does). A pure interface, inherited `public virtual` (see AxialPositionable).
///
/// isMotorMount(), getInstanceCount() and getLength() also exist in RocketComponent, so an
/// implementing class declares them with `override`, which overrides both at once. Java's
/// MotorMount re-declares further RocketComponent methods (getID(), getDebugName(), getStage(),
/// getAssembly(), getComponentLocations()); those are non-virtual here and reached through
/// asComponent() instead.
///
/// Deviations from OpenRocket:
/// - getMotorIterator() is getMotorConfigurationSet().values().
/// - setMotorConfig() takes a std::optional: nullopt is Java's null (forget the configuration's
///   motor).
/// - getMotorPosition() throws BugError where Java throws IllegalArgumentException (asking for
///   the position of a motor that does not exist is a programming error).
/// - toMotorDebug() takes the Preferences that choose the motor name (see Motor::getMotorName()).
///
/// Deferred to the rocket components group: getClusterConfiguration(), which needs
/// ClusterConfiguration (a body tube answers SINGLE, an inner tube its cluster). The callers in
/// this group only need the motor count, which getMotorCount() gives.
class MotorMount : public virtual FlightConfigurableComponent
{
public:
    ~MotorMount() override = default;

    MotorMount& operator=(const MotorMount&) = delete;
    MotorMount& operator=(MotorMount&&)      = delete;

    /// Whether this mount holds a motor in at least one flight configuration (an override in its
    /// motor configuration set).
    [[nodiscard]] virtual bool hasMotor() const = 0;

    /// Makes the component act as a motor mount (true) or as a structural component only.
    virtual void setMotorMount(bool acting) = 0;

    /// Whether the component acts as a motor mount (RocketComponent declares it too).
    [[nodiscard]] virtual bool isMotorMount() const = 0;

    /// The default motor configuration: no motor, and the ignition settings new configurations
    /// start from.
    [[nodiscard]] virtual MotorConfiguration&       getDefaultMotorConfig()       = 0;
    [[nodiscard]] virtual const MotorConfiguration& getDefaultMotorConfig() const = 0;

    /// How many times the component is instanced (RocketComponent declares it too).
    [[nodiscard]] virtual int getInstanceCount() const = 0;

    /// The length of the mount (RocketComponent declares it too).
    [[nodiscard]] virtual double getLength() const = 0;

    /// The motor of every flight configuration.
    [[nodiscard]] virtual MotorConfigurationSet&       getMotorConfigurationSet()       = 0;
    [[nodiscard]] virtual const MotorConfigurationSet& getMotorConfigurationSet() const = 0;

    /// The motor configuration of @p fcid: its own, or the default.
    [[nodiscard]] virtual MotorConfiguration& getMotorConfig(const FlightConfigurationId& fcid) = 0;
    [[nodiscard]] virtual const MotorConfiguration& getMotorConfig(
        const FlightConfigurationId& fcid) const = 0;

    /// Stores @p newMotorConfig as the motor configuration of @p fcid, or forgets the one of
    /// @p fcid for nullopt, and makes the component act as a motor mount (without an event, as in
    /// Java).
    /// @throws BugError when @p newMotorConfig belongs to another mount.
    virtual void setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
                                const FlightConfigurationId&      fcid) = 0;

    /// The number of motors the mount holds in a configuration (the size of its cluster).
    [[nodiscard]] virtual int getMotorCount() const = 0;

    /// getMotorCount() times the instance count of every assembly above the mount (one motor in a
    /// two-instance booster set gives 2).
    [[nodiscard]] virtual int getMotorCountIncludingAssemblyCopies() const = 0;

    /// How far the motors stick out of the aft end of the mount, in m.
    [[nodiscard]] virtual double getMotorOverhang() const   = 0;
    virtual void                 setMotorOverhang(double v) = 0;

    /// The inner diameter of the mount, in m.
    [[nodiscard]] virtual double getMotorMountDiameter() const = 0;

    /// The position of the front of the motor of @p fcid, relative to this component.
    /// @throws BugError when @p fcid has no motor.
    [[nodiscard]] virtual Coordinate getMotorPosition(const FlightConfigurationId& fcid) const = 0;

    /// A table of every motor configuration of this mount (MotorConfigurationSet::toDebug()).
    [[nodiscard]] virtual std::string toMotorDebug(const Preferences& preferences) const = 0;

protected:
    MotorMount()                  = default;
    MotorMount(const MotorMount&) = default;
    MotorMount(MotorMount&&)      = default;
};

/// @p mount as the RocketComponent it is (a free function, so that MotorMount stays a pure
/// interface).
/// @throws BugError when the implementing class is not a RocketComponent.
[[nodiscard]] inline RocketComponent& asComponent(MotorMount& mount)
{
    auto* component = dynamic_cast<RocketComponent*>(&mount);
    if (component == nullptr)
    {
        bug("a MotorMount that is not a RocketComponent");
    }
    return *component;
}

/// asComponent() of a const mount.
[[nodiscard]] inline const RocketComponent& asComponent(const MotorMount& mount)
{
    const auto* component = dynamic_cast<const RocketComponent*>(&mount);
    if (component == nullptr)
    {
        bug("a MotorMount that is not a RocketComponent");
    }
    return *component;
}

}  // namespace QtRocket
