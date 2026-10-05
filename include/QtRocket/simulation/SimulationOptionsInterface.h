#pragma once

#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class PinkNoiseWindModel;

/// The launch conditions that a simulation and the launch preferences have in common (OpenRocket's
/// simulation/SimulationOptionsInterface): the launch rod, the average wind, the launch site and
/// its atmosphere. SimulationOptions implements it.
///
/// Java's interface extends ChangeSource: addChangeListener()/removeChangeListener() are
/// changed().connect()/disconnect() here, as for the other ported change sources.
///
/// Every getter is const, also getLaunchRodDirection(), which in SimulationOptions asks the wind
/// model for its direction when launching into the wind. getAverageWindModel() has a const and a
/// non-const form, since Java hands out the model to be changed in place.
///
/// Deviation: in OpenRocket ApplicationPreferences implements this interface too, which lets
/// DefaultSimulationOptionFactory copy between the preferences and a simulation's options with
/// one routine. Preferences does not implement it here (preferences/ sits below models/ and
/// simulation/, and keeps the wind as plain values, not as a PinkNoiseWindModel), so the factory
/// spells both directions out.
class SimulationOptionsInterface
{
public:
    virtual ~SimulationOptionsInterface() = default;

    /// Emitted when a launch condition changes (ChangeSource).
    [[nodiscard]] virtual Signal<>& changed() noexcept = 0;

    /// The length of the launch rod, m.
    [[nodiscard]] virtual double getLaunchRodLength() const                 = 0;
    virtual void                 setLaunchRodLength(double launchRodLength) = 0;

    /// Whether the launch rod points into the wind (getLaunchRodDirection() then follows the
    /// wind direction).
    [[nodiscard]] virtual bool getLaunchIntoWind() const              = 0;
    virtual void               setLaunchIntoWind(bool launchIntoWind) = 0;

    /// The angle of the launch rod from vertical, rad.
    [[nodiscard]] virtual double getLaunchRodAngle() const                = 0;
    virtual void                 setLaunchRodAngle(double launchRodAngle) = 0;

    /// The direction the launch rod is tilted towards, rad clockwise from north.
    [[nodiscard]] virtual double getLaunchRodDirection() const                    = 0;
    virtual void                 setLaunchRodDirection(double launchRodDirection) = 0;

    /// The average wind model, to read and to change in place.
    [[nodiscard]] virtual PinkNoiseWindModel&       getAverageWindModel() noexcept       = 0;
    [[nodiscard]] virtual const PinkNoiseWindModel& getAverageWindModel() const noexcept = 0;

    /// The altitude of the launch site, m above mean sea level.
    [[nodiscard]] virtual double getLaunchAltitude() const          = 0;
    virtual void                 setLaunchAltitude(double altitude) = 0;

    /// The latitude of the launch site, degrees north.
    [[nodiscard]] virtual double getLaunchLatitude() const                = 0;
    virtual void                 setLaunchLatitude(double launchLatitude) = 0;

    /// The longitude of the launch site, degrees east.
    [[nodiscard]] virtual double getLaunchLongitude() const                 = 0;
    virtual void                 setLaunchLongitude(double launchLongitude) = 0;

    /// How the simulation follows the rocket's position over the Earth.
    [[nodiscard]] virtual GeodeticComputationStrategy getGeodeticComputation() const     = 0;
    virtual void setGeodeticComputation(GeodeticComputationStrategy geodeticComputation) = 0;

    /// Whether the atmosphere is the International Standard Atmosphere (Java: isISAAtmosphere()).
    [[nodiscard]] virtual bool isIsaAtmosphere() const = 0;
    /// Java: setISAAtmosphere().
    virtual void setIsaAtmosphere(bool isa) = 0;

    /// The temperature at the launch site, K.
    [[nodiscard]] virtual double getLaunchTemperature() const                   = 0;
    virtual void                 setLaunchTemperature(double launchTemperature) = 0;

    /// The pressure at the launch site, Pa.
    [[nodiscard]] virtual double getLaunchPressure() const                = 0;
    virtual void                 setLaunchPressure(double launchPressure) = 0;

    /// The relative humidity at the launch site, 0 ... 1.
    [[nodiscard]] virtual double getLaunchRelativeHumidity() const                = 0;
    virtual void                 setLaunchRelativeHumidity(double launchHumidity) = 0;

protected:
    SimulationOptionsInterface()                                             = default;
    SimulationOptionsInterface(const SimulationOptionsInterface&)            = default;
    SimulationOptionsInterface& operator=(const SimulationOptionsInterface&) = default;
    SimulationOptionsInterface(SimulationOptionsInterface&&)                 = default;
    SimulationOptionsInterface& operator=(SimulationOptionsInterface&&)      = default;
};

}  // namespace QtRocket
