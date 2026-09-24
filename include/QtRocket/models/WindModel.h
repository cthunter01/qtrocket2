#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// A model of the wind (OpenRocket's models/wind/WindModel): the wind velocity at a time and
/// altitude. The simulation runs a clone() of the configured model, seeded with the run's seed.
///
/// Java's interface extends Monitorable (modId() here, so every model satisfies the Monitorable
/// concept), Cloneable (clone(), which returns a unique_ptr since C++ has no covariant smart
/// pointers; the concrete classes also have a copy constructor with the same meaning) and
/// ChangeSource (addChangeListener()/removeChangeListener() are changed().connect()/
/// disconnect(), and fireChangeEvent() emits it).
///
/// A copy never carries the original's connections to changed(), as Java's clone() starts a new
/// listener list. Moving takes the connections along. Models are polymorphic and held through
/// pointers; the copy and move constructors are protected here so that a model cannot be sliced.
///
/// getWindVelocity() is not const: a model draws its turbulence from a random source as time
/// advances. A model belongs to one thread at a time.
class WindModel
{
public:
    /// The altitude the wind-altitude relation of a model refers to.
    enum class AltitudeReference
    {
        MSL,  ///< mean sea level
        AGL,  ///< above ground level (the launch site)
    };

    virtual ~WindModel() = default;

    WindModel& operator=(const WindModel&) = delete;
    WindModel& operator=(WindModel&&)      = delete;

    /// The wind velocity in m/s (x east, y north, z up) @p time seconds after the start of the
    /// simulation at @p altitudeMsl metres above mean sea level, which is @p altitudeAgl metres
    /// above the ground; the model decides which altitude it uses.
    ///
    /// Sign convention, as in OpenRocket: the vector points into the wind, towards where it
    /// comes from (a wind from the east gives +x), so it is the negated motion of the air. The
    /// simulation adds it to the rocket's velocity to get the airspeed (Java's
    /// AbstractSimulationStepper: airSpeed = rocketVelocity + windVelocity); subtracting it
    /// would turn every wind effect around.
    [[nodiscard]] virtual Coordinate getWindVelocity(double time, double altitudeMsl,
                                                     double altitudeAgl) = 0;

    /// The wind velocity in m/s @p time seconds after the start of the simulation at @p altitude
    /// metres (with the sign convention above).
    [[nodiscard]] virtual Coordinate getWindVelocity(double time, double altitude) = 0;

    /// Seeds the model's random source, so that a simulation run with a given seed reproduces
    /// the same wind. This discards any random state already generated, so it must only be
    /// called while configuring a simulation, never during one: reseeding mid-flight would change
    /// the wind partway through.
    virtual void setSeed(int seed) = 0;

    /// A copy of the model (Java's clone()): the configuration and the seed, a fresh random
    /// state, and no connections to changed().
    [[nodiscard]] virtual std::unique_ptr<WindModel> clone() const = 0;

    /// The id of the model's current state (Monitorable).
    [[nodiscard]] virtual ModId modId() const = 0;

    /// Emitted when the model's configuration changes (ChangeSource).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

    /// Emits changed() (Java's public fireChangeEvent()).
    void fireChangeEvent() const { m_changed.emit(); }

protected:
    WindModel() = default;
    /// Copies nothing: the connections to changed() stay with @p other.
    WindModel(const WindModel& /*other*/) noexcept { }
    WindModel(WindModel&&) noexcept = default;

private:
    Signal<> m_changed;
};

/// The .ork spelling of a reference (OpenRocketSaver.enumToXMLName, the lower-cased constant
/// name): "msl" or "agl", the altituderef attribute of <wind model="multilevel">.
[[nodiscard]] std::string_view toString(WindModel::AltitudeReference reference) noexcept;

/// The reference whose .ork spelling is @p text once trimmed (DocumentConfig.findEnum: an exact
/// match against the lower-cased constant names), or nullopt (Java: null).
[[nodiscard]] std::optional<WindModel::AltitudeReference> altitudeReferenceFromString(
    std::string_view text) noexcept;

}  // namespace QtRocket
