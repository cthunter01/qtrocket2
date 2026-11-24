#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"

namespace QtRocket
{

DefaultSimulationOptionFactory::DefaultSimulationOptionFactory(Preferences& preferences) noexcept
  : m_prefs(&preferences)
{
}

SimulationOptions DefaultSimulationOptionFactory::getDefault() const
{
    Preferences& prefs = *m_prefs;

    SimulationOptions defaults(prefs);

    // copyLaunchConditions(prefs, defaults)

    // The wind is copied first, because the launch rod direction follows the wind direction
    // while launching into wind.
    PinkNoiseWindModel sourceWind;  // prefs.getAverageWindModel()
    sourceWind.loadFrom(prefs);
    PinkNoiseWindModel& destinationWind = defaults.getAverageWindModel();
    // Setting the average rescales the standard deviation, so copy the deviation after it.
    destinationWind.setAverage(sourceWind.getAverage());
    destinationWind.setStandardDeviation(sourceWind.getStandardDeviation());
    destinationWind.setDirection(sourceWind.getDirection());

    defaults.setLaunchLatitude(prefs.getLaunchLatitude());
    defaults.setLaunchLongitude(prefs.getLaunchLongitude());

    // While the ISA model is in use, the atmospheric conditions are derived from the altitude,
    // so the explicit values are applied after the ISA flag and the altitude.
    defaults.setIsaAtmosphere(prefs.isIsaAtmosphere());
    defaults.setLaunchAltitude(prefs.getLaunchAltitude());
    defaults.setLaunchTemperature(prefs.getLaunchTemperature());
    defaults.setLaunchPressure(prefs.getLaunchPressure());
    defaults.setLaunchRelativeHumidity(prefs.getLaunchRelativeHumidity());

    defaults.setLaunchIntoWind(prefs.getLaunchIntoWind());
    defaults.setLaunchRodLength(prefs.getLaunchRodLength());
    defaults.setLaunchRodAngle(prefs.getLaunchRodAngle());
    defaults.setLaunchRodDirection(prefs.getLaunchRodDirection());

    if (prefs.isRandomSeedFixed())
    {
        defaults.setRandomSeed(prefs.getRandomSeed());
        defaults.setRandomSeedFixed(true);
    }

    // Bug 38 (not in Java, see the header): the Simulation page's geodetic computation and the
    // Warnings page's three thresholds, read as the simulation dialog's "Reset to default"
    // buttons of the options and warnings tabs read them (SimulationOptionsPanel.java:407-409,
    // SimulationWarningsPanel.java:172-175).
    defaults.setGeodeticComputation(
        geodeticComputationStrategyFromString(prefs.getGeodeticComputationName())
            .value_or(GeodeticComputationStrategy::SPHERICAL));
    defaults.setRecoverySpeedWarning(prefs.getRecoverySpeedWarning());
    defaults.setRecoveryDrogueMainHighSpeedWarning(prefs.getRecoveryDrogueMainHighSpeedWarning());
    defaults.setRecoveryDrogueMainLowSpeedWarning(prefs.getRecoveryDrogueMainLowSpeedWarning());
    return defaults;
}

void DefaultSimulationOptionFactory::saveDefault(const SimulationOptions& newDefaults) const
{
    Preferences& prefs = *m_prefs;

    // copyLaunchConditions(newDefaults, prefs)

    // The wind is copied first, because the launch rod direction follows the wind direction
    // while launching into wind.
    const PinkNoiseWindModel& sourceWind = newDefaults.getAverageWindModel();
    PinkNoiseWindModel        destinationWind;  // prefs.getAverageWindModel()
    destinationWind.loadFrom(prefs);
    // ApplicationPreferences.stateChanged(): every change of the model is stored.
    destinationWind.changed().connect(
        [&destinationWind, &prefs] { destinationWind.storeTo(prefs); });
    // Setting the average rescales the standard deviation, so copy the deviation after it.
    destinationWind.setAverage(sourceWind.getAverage());
    destinationWind.setStandardDeviation(sourceWind.getStandardDeviation());
    destinationWind.setDirection(sourceWind.getDirection());
    // Java's keys always describe the model its preferences keep, which stored itself when it
    // was first loaded. The model here was loaded just now, without storing: a key that does not
    // describe it (an intensity left over from a wind whose average is now zero, or one a
    // rounding step away from deviation / average) would be read again by the next call.
    destinationWind.storeTo(prefs);

    prefs.setLaunchLatitude(newDefaults.getLaunchLatitude());
    prefs.setLaunchLongitude(newDefaults.getLaunchLongitude());

    // While the ISA model is in use, the atmospheric conditions are derived from the altitude,
    // so the explicit values are applied after the ISA flag and the altitude.
    prefs.setIsaAtmosphere(newDefaults.isIsaAtmosphere());
    prefs.setLaunchAltitude(newDefaults.getLaunchAltitude());
    prefs.setLaunchTemperature(newDefaults.getLaunchTemperature());
    prefs.setLaunchPressure(newDefaults.getLaunchPressure());
    prefs.setLaunchRelativeHumidity(newDefaults.getLaunchRelativeHumidity());

    prefs.setLaunchIntoWind(newDefaults.getLaunchIntoWind());
    prefs.setLaunchRodLength(newDefaults.getLaunchRodLength());
    prefs.setLaunchRodAngle(newDefaults.getLaunchRodAngle());
    prefs.setLaunchRodDirection(newDefaults.getLaunchRodDirection());
}

}  // namespace QtRocket
