#pragma once

#include <memory>
#include <numbers>
#include <optional>
#include <string_view>

#include "QtRocket/models/WindModel.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/PinkNoise.h"

namespace QtRocket
{

class Preferences;

/// Wind as pink noise around an average speed, from one direction at every altitude (OpenRocket's
/// models/wind/PinkNoiseWindModel).
///
/// The speed is average + noise * standardDeviation / kStdDev, where the noise is a PinkNoise
/// source (alpha 5/3, two poles) sampled every kDeltaT seconds and interpolated linearly between
/// samples, and kStdDev is the standard deviation of that source. The wind blows from
/// getDirection() (radians clockwise from north, pi/2 being a wind from the east), and the
/// velocity is speed * (sin(direction), cos(direction), 0): it points towards where the wind
/// comes from, the negated motion of the air (see WindModel::getWindVelocity()).
///
/// The random source is seeded from the model's seed (XOR kSeedRandomization) when the first
/// velocity is asked for, and started again whenever an earlier time is asked for, so the wind
/// is a function of the time for a given seed. Deviation: the Gaussian input comes from
/// std::mt19937 (see PinkNoise), so a seeded run is reproducible within QtRocket (for one standard
/// library) but never bit-identical to OpenRocket's java.util.Random sequence.
///
/// Each setter of the average, direction or standard deviation emits changed() unless its
/// argument equals the stored value, as in Java. The comparison comes before the value is
/// clamped or reduced, so a call that leaves the stored value as it was may still emit:
/// setStandardDeviation(-1) with a deviation of 0 (stored as 0 again), setDirection(pi / 2 +
/// 2 pi) with a direction of pi / 2 (reduced to pi / 2 again). The turbulence intensity is not
/// stored: it is standardDeviation / average.
///
/// Copying (the copy constructor, clone()) is Java's clone(): the average, direction, standard
/// deviation and seed, with a fresh random state and no connections to changed(). There is no
/// assignment; loadFrom() copies the configuration without the seed, as in Java.
///
/// Deviations from OpenRocket:
/// - getIntensityDescription(), which looks a translated text up, is getIntensityDescriptionKey()
///   and returns the translation key.
/// - The default constructor seeds from std::random_device where Java draws new Random().nextInt().
class PinkNoiseWindModel final : public WindModel
{
public:
    /// Time between the random samples, s (DELTA_T).
    static constexpr double kDeltaT = 0.05;
    /// The value XORed into every seed (SEED_RANDOMIZATION).
    static constexpr int kSeedRandomization = 0x7343AA03;
    /// The pink noise exponent (ALPHA).
    static constexpr double kAlpha = 5.0 / 3.0;
    /// The number of poles of the pink noise filter (POLES).
    static constexpr int kPoles = 2;
    /// The standard deviation of the pink noise with kPoles poles (STDDEV).
    static constexpr double kStdDev = 2.252;

    /// A model with a nondeterministic seed: no wind (average and deviation 0) from the east.
    PinkNoiseWindModel();
    /// A model with the given seed: no wind (average and deviation 0) from the east.
    explicit PinkNoiseWindModel(int seed) noexcept;
    ~PinkNoiseWindModel() override = default;

    /// Java's clone() (see the class comment).
    PinkNoiseWindModel(const PinkNoiseWindModel& other);
    /// Takes the state, the random state and the connections to changed() along.
    PinkNoiseWindModel(PinkNoiseWindModel&& other) noexcept  = default;
    PinkNoiseWindModel& operator=(const PinkNoiseWindModel&) = delete;
    PinkNoiseWindModel& operator=(PinkNoiseWindModel&&)      = delete;

    /// Seeds the random source with @p seed XOR kSeedRandomization and discards the random state.
    void setSeed(int seed) override;

    /// The average wind speed, m/s.
    [[nodiscard]] double getAverage() const noexcept { return m_average; }

    /// Sets the average wind speed, keeping the turbulence intensity: the standard deviation is
    /// scaled along. A negative speed is its magnitude blowing the other way (the direction is
    /// turned by pi). Emits changed() when the speed changes (and when the direction or deviation
    /// do, each on its own).
    void setAverage(double average);

    /// Sets the average wind speed without touching the standard deviation; a negative speed
    /// turns the direction as setAverage() does.
    void setAveragePreservingStandardDeviation(double average);

    /// The direction the wind blows from, radians clockwise from north, 0 ... 2 pi.
    [[nodiscard]] double getDirection() const noexcept { return m_direction; }

    /// Sets the direction, reduced to 0 ... 2 pi (MathUtil::reduce2Pi). Nothing happens when
    /// @p direction equals the stored direction before reduction, as in Java.
    void setDirection(double direction);

    /// The standard deviation of the wind speed, m/s.
    [[nodiscard]] double getStandardDeviation() const noexcept { return m_standardDeviation; }

    /// Sets the standard deviation; a negative value is stored as 0 (Java's Math.max, so NaN
    /// stays NaN and -0.0 becomes 0.0).
    void setStandardDeviation(double standardDeviation);

    /// The turbulence intensity, standard deviation / average. With an average of 0 (within
    /// MathUtil::kEpsilon) it is 0 when the deviation is 0 too and 1 otherwise.
    [[nodiscard]] double getTurbulenceIntensity() const noexcept;

    /// Sets the standard deviation to @p intensity * average.
    void setTurbulenceIntensity(double intensity);

    /// The translation key describing the turbulence intensity: "simedtdlg.IntensityDesc." +
    /// None (below 0.001), Verylow (0.05), Low (0.10), Medium (0.15), High (0.20), Veryhigh
    /// (0.25) or Extreme.
    [[nodiscard]] std::string_view getIntensityDescriptionKey() const noexcept;

    /// The velocity at @p time; the altitudes do not matter.
    [[nodiscard]] Coordinate getWindVelocity(double time, double altitudeMsl,
                                             double altitudeAgl) override;

    /// The velocity at @p time (see the class comment); the altitude does not matter.
    /// @throws BugError when @p time is negative (Java: IllegalArgumentException "Requesting wind
    ///         speed at t=..."): the simulation never asks for a time before its start.
    [[nodiscard]] Coordinate getWindVelocity(double time, double altitude) override;

    /// Copies the average, direction and standard deviation of @p source, not the seed, and
    /// emits nothing (Java's loadFrom()).
    void loadFrom(const PinkNoiseWindModel& source) noexcept;

    /// ApplicationPreferences.loadWindModelState(): setAverage(), setTurbulenceIntensity() and
    /// setDirection() from the wind preferences (Preferences::getWindAverage() and friends).
    void loadFrom(const Preferences& preferences);

    /// ApplicationPreferences.storeWindModelState(): the average, turbulence intensity and
    /// direction into the wind preferences (Preferences::setWindAverage() and friends).
    void storeTo(Preferences& preferences) const;

    /// Always ModId::zero(), as in Java (changes are announced through changed()).
    [[nodiscard]] ModId modId() const noexcept override { return ModId::zero(); }

    /// A copy (see the class comment).
    [[nodiscard]] std::unique_ptr<WindModel> clone() const override;

    /// Java's equals(): the average, standard deviation and direction compare equal under
    /// Double.compare (NaN equals NaN, 0.0 differs from -0.0) and the seeds are equal.
    [[nodiscard]] bool operator==(const PinkNoiseWindModel& other) const noexcept;

    /// Java's hashCode(): 17, then 31 * h + Double.hashCode of the average, the standard
    /// deviation and the direction, then 31 * h + seed, in Java's wrapping int arithmetic.
    [[nodiscard]] int hashCode() const noexcept;

private:
    /// Drops the random source; the next velocity starts it again.
    void reset() noexcept { m_randomSource.reset(); }

    double m_average{0};
    double m_direction{std::numbers::pi / 2};  // a wind from the east
    double m_standardDeviation{0};

    int m_seed;

    std::optional<PinkNoise> m_randomSource;
    double                   m_time1{0};
    double                   m_value1{0};
    double                   m_value2{0};
};

}  // namespace QtRocket
