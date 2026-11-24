#include "QtRocket/simulation/LaunchPreferences.h"

#include <utility>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

LaunchPreferences::LaunchPreferences(Preferences& preferences)
  : m_preferences(&preferences),
    m_windConnection(m_averageWindModel.changed().connect([this] { storeWind(); }))
{
    // Java's first getAverageWindModel(): a new model that stores every change of its own, then
    // loadWindModelState(), whose setters store as they go. Here the model is stored once it is
    // loaded, which leaves the keys as Java's last store leaves them, without the half-loaded
    // states in between (the intensity of the calm model after its first setter, ...).
    loadWind();
    storeWind();
    m_preferencesConnection = preferences.changed().connect([this] { preferencesChanged(); });
}

LaunchPreferences::~LaunchPreferences() = default;

LaunchPreferences::WindKeys LaunchPreferences::readWindKeys(const Preferences& preferences)
{
    return {.average             = preferences.getWindAverage(),
            .turbulenceIntensity = preferences.getWindTurbulenceIntensity(),
            .direction           = preferences.getWindDirection()};
}

bool LaunchPreferences::sameWindKeys(const WindKeys& a, const WindKeys& b) noexcept
{
    return MathUtil::javaDoubleCompare(a.average, b.average) == 0 &&
           MathUtil::javaDoubleCompare(a.turbulenceIntensity, b.turbulenceIntensity) == 0 &&
           MathUtil::javaDoubleCompare(a.direction, b.direction) == 0;
}

void LaunchPreferences::loadWind()
{
    m_loadingWind = true;
    m_averageWindModel.loadFrom(*m_preferences);
    m_loadingWind = false;
}

void LaunchPreferences::storeWind()
{
    if (m_loadingWind)
    {
        // The model takes the keys' values: there is nothing to store.
        return;
    }
    // A slot of the preferences' change may change the model again: that store nests.
    const bool wasStoring = std::exchange(m_storingWind, true);
    m_averageWindModel.storeTo(*m_preferences);
    m_storingWind = wasStoring;
    m_syncedWind  = readWindKeys(*m_preferences);
}

void LaunchPreferences::preferencesChanged()
{
    if (!m_storingWind && !m_loadingWind)
    {
        const WindKeys keys = readWindKeys(*m_preferences);
        if (!sameWindKeys(keys, m_syncedWind))
        {
            m_syncedWind = keys;
            loadWind();
        }
    }
    m_changed.emit();
}

double LaunchPreferences::getLaunchRodLength() const
{
    return m_preferences->getLaunchRodLength();
}

void LaunchPreferences::setLaunchRodLength(double launchRodLength)
{
    m_preferences->setLaunchRodLength(launchRodLength);
}

bool LaunchPreferences::getLaunchIntoWind() const
{
    return m_preferences->getLaunchIntoWind();
}

void LaunchPreferences::setLaunchIntoWind(bool launchIntoWind)
{
    const bool changed = getLaunchIntoWind() != launchIntoWind;
    // Java: putBoolean(), which writes the key also when it holds the value.
    m_preferences->setLaunchIntoWind(launchIntoWind);
    if (changed)
    {
        // Not in Java (see the class comment).
        m_changed.emit();
    }
}

double LaunchPreferences::getLaunchRodAngle() const
{
    return m_preferences->getLaunchRodAngle();
}

void LaunchPreferences::setLaunchRodAngle(double launchRodAngle)
{
    m_preferences->setLaunchRodAngle(launchRodAngle);
}

double LaunchPreferences::getLaunchRodDirection() const
{
    // Java's getter stores the wind direction as the rod direction when launching into the wind.
    return m_preferences->getLaunchRodDirection();
}

void LaunchPreferences::setLaunchRodDirection(double launchRodDirection)
{
    m_preferences->setLaunchRodDirection(launchRodDirection);
}

double LaunchPreferences::getLaunchAltitude() const
{
    return m_preferences->getLaunchAltitude();
}

void LaunchPreferences::setLaunchAltitude(double altitude)
{
    m_preferences->setLaunchAltitude(altitude);
}

double LaunchPreferences::getLaunchLatitude() const
{
    return m_preferences->getLaunchLatitude();
}

void LaunchPreferences::setLaunchLatitude(double launchLatitude)
{
    m_preferences->setLaunchLatitude(launchLatitude);
}

double LaunchPreferences::getLaunchLongitude() const
{
    return m_preferences->getLaunchLongitude();
}

void LaunchPreferences::setLaunchLongitude(double launchLongitude)
{
    m_preferences->setLaunchLongitude(launchLongitude);
}

GeodeticComputationStrategy LaunchPreferences::getGeodeticComputation() const
{
    // Java: getEnum(GEODETIC_COMPUTATION, SPHERICAL); the store gives one of the constant names.
    return geodeticComputationStrategyFromString(m_preferences->getGeodeticComputationName())
        .value_or(GeodeticComputationStrategy::SPHERICAL);
}

void LaunchPreferences::setGeodeticComputation(GeodeticComputationStrategy geodeticComputation)
{
    const bool changed = getGeodeticComputation() != geodeticComputation;
    // Java: putEnum(), which writes the key also when it holds the value.
    m_preferences->setGeodeticComputationName(name(geodeticComputation));
    if (changed)
    {
        // Not in Java (see the class comment).
        m_changed.emit();
    }
}

bool LaunchPreferences::isIsaAtmosphere() const
{
    return m_preferences->isIsaAtmosphere();
}

void LaunchPreferences::setIsaAtmosphere(bool isa)
{
    m_preferences->setIsaAtmosphere(isa);
}

double LaunchPreferences::getLaunchTemperature() const
{
    return m_preferences->getLaunchTemperature();
}

void LaunchPreferences::setLaunchTemperature(double launchTemperature)
{
    m_preferences->setLaunchTemperature(launchTemperature);
}

double LaunchPreferences::getLaunchPressure() const
{
    return m_preferences->getLaunchPressure();
}

void LaunchPreferences::setLaunchPressure(double launchPressure)
{
    m_preferences->setLaunchPressure(launchPressure);
}

double LaunchPreferences::getLaunchRelativeHumidity() const
{
    return m_preferences->getLaunchRelativeHumidity();
}

void LaunchPreferences::setLaunchRelativeHumidity(double launchHumidity)
{
    m_preferences->setLaunchRelativeHumidity(launchHumidity);
}

}  // namespace QtRocket
