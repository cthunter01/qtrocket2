#pragma once

#include "QtRocket/simulation/SimulationOptions.h"

namespace QtRocket
{

class Preferences;

/// Provides the launch conditions that new simulations start from, and stores the conditions of
/// an existing simulation as the new defaults (OpenRocket's
/// simulation/DefaultSimulationOptionFactory).
///
/// Both directions copy the same launch conditions, in this order: the average wind (average
/// speed, then standard deviation, since setting the average rescales the deviation, then
/// direction; the wind comes first because the rod direction follows it while launching into the
/// wind), the launch latitude and longitude, the ISA flag and the launch altitude (while the ISA
/// is in use these decide the atmosphere), then the launch temperature, pressure and humidity,
/// the launch-into-wind flag and the rod length, angle and direction. The other options (time
/// step, gravity, stepper, thresholds, ...) are not launch conditions and are not copied by
/// saveDefault(); getDefault() applies four of them besides (bug 38, below).
///
/// The factory holds a reference to the preferences, which must outlive it. Java injects the
/// application preferences; here they are a constructor argument.
///
/// Deviations from OpenRocket:
/// - Java copies between two SimulationOptionsInterface objects, the preferences being one. Here
///   Preferences does not implement the interface, so the two directions are written out.
/// - Java's preferences keep one PinkNoiseWindModel for the life of the program, loaded from the
///   wind keys on first use and stored to them on every change. Here the wind is a model loaded
///   from the wind keys for each call (PinkNoiseWindModel::loadFrom(const Preferences&), as on
///   Java's first use) and stored after each change (PinkNoiseWindModel::storeTo()). The keys
///   hold the average, the turbulence intensity and the direction, so a standard deviation
///   saved with a zero average comes back as 0 (in Java only after a restart), and a deviation
///   may come back one rounding step away from the one saved.
/// - saveDefault() stores the wind once more after copying it, so that the wind keys describe
///   the wind just saved also when no setter changed the freshly loaded model: saving a calm
///   wind (0 m/s, no deviation) over a stored intensity of a wind without an average writes the
///   intensity 0, as Java's long-lived model does when its deviation goes to 0, and an intensity
///   that is a rounding step away from deviation / average is rewritten, as Java rewrites it
///   when it first loads the model. Nothing is written or announced for a key that already
///   holds the value.
/// - Preferences' wind setters emit its changed() (see Preferences), so saveDefault() emits it
///   for the wind as well as for the other values.
/// - OpenRocket bug 38, fixed: getDefault() also takes the geodetic computation
///   (Preferences::getGeodeticComputationName(), the Simulation preference page's "Geodetic
///   calculations") and the three recovery speed warning thresholds of the Warnings preference
///   page (getRecoverySpeedWarning(), getRecoveryDrogueMainHighSpeedWarning(),
///   getRecoveryDrogueMainLowSpeedWarning()) from the preferences. In Java a new simulation
///   starts with SimulationOptions' constants for them (SPHERICAL, 20, 30.48 and 15.24 m/s),
///   whatever the pages say, though the pages say that their settings apply to new simulations
///   (only the dialog tabs' own "Reset to default" buttons read them). The drogue's low speed
///   threshold, which no page edits, keeps its constant, and saveDefault() stores none of the
///   four (the dialog tabs' "Save as default" buttons do). What else reads getDefault() reads
///   them too: the launch conditions tab's "Reset to default" now resets the geodetic
///   computation and the three thresholds to the preferences' values, where Java resets them to
///   the constants.
class DefaultSimulationOptionFactory
{
public:
    /// A factory over @p preferences, which must outlive it.
    explicit DefaultSimulationOptionFactory(Preferences& preferences) noexcept;

    /// The launch conditions a new simulation starts from: SimulationOptions(preferences) with
    /// the launch conditions of the preferences applied through the setters (see the class
    /// comment), and, when the preferences fix the random seed, their seed, fixed; then the
    /// geodetic computation and the three warning thresholds of the preferences (bug 38).
    ///
    /// Reading the rod direction of preferences that launch into the wind stores their wind
    /// direction as their rod direction (Preferences::getLaunchRodDirection()), as in Java.
    [[nodiscard]] SimulationOptions getDefault() const;

    /// Stores the launch conditions of @p newDefaults as the launch preferences, the ones
    /// getDefault() reads. The random seed is not stored.
    void saveDefault(const SimulationOptions& newDefaults) const;

private:
    Preferences* m_prefs;
};

}  // namespace QtRocket
