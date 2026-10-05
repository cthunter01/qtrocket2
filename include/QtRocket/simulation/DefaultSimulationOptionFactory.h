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
/// step, gravity, stepper, thresholds, ...) are not launch conditions and are not copied.
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
/// - Preferences' wind setters emit its changed() (see Preferences), so saveDefault() emits it
///   for the wind as well as for the other values.
class DefaultSimulationOptionFactory
{
public:
    /// A factory over @p preferences, which must outlive it.
    explicit DefaultSimulationOptionFactory(Preferences& preferences) noexcept;

    /// The launch conditions a new simulation starts from: SimulationOptions(preferences) with
    /// the launch conditions of the preferences applied through the setters (see the class
    /// comment), and, when the preferences fix the random seed, their seed, fixed.
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
