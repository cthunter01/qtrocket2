#include "QtRocket/models/PinkNoiseWindModel.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <random>
#include <string_view>

#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PinkNoise.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// seed ^ SEED_RANDOMIZATION on Java ints.
[[nodiscard]] int randomizeSeed(int seed) noexcept
{
    return seed ^ PinkNoiseWindModel::kSeedRandomization;
}

}  // namespace

PinkNoiseWindModel::PinkNoiseWindModel()
  // Java: new Random().nextInt(); the int conversion of the 32 random bits is modular.
  : PinkNoiseWindModel(static_cast<int>(std::random_device{}()))
{
}

PinkNoiseWindModel::PinkNoiseWindModel(int seed) noexcept : m_seed(randomizeSeed(seed)) { }

PinkNoiseWindModel::PinkNoiseWindModel(const PinkNoiseWindModel& other)
  : WindModel(other),
    m_average(other.m_average),
    m_direction(other.m_direction),
    m_standardDeviation(other.m_standardDeviation),
    m_seed(other.m_seed)
{
    // The random state is not copied: the clone starts its source afresh from the seed.
}

void PinkNoiseWindModel::setSeed(int seed)
{
    m_seed = randomizeSeed(seed);
    reset();
}

void PinkNoiseWindModel::setAverage(double average)
{
    if (average < 0)
    {
        average = -average;
        setDirection(std::numbers::pi + getDirection());
    }
    if (average == m_average)
    {
        return;
    }
    const double intensity = getTurbulenceIntensity();
    m_average              = average;
    setTurbulenceIntensity(intensity);
    fireChangeEvent();
}

void PinkNoiseWindModel::setAveragePreservingStandardDeviation(double average)
{
    if (average < 0)
    {
        average = -average;
        setDirection(std::numbers::pi + getDirection());
    }
    if (average == m_average)
    {
        return;
    }
    m_average = average;
    fireChangeEvent();
}

void PinkNoiseWindModel::setDirection(double direction)
{
    if (direction == m_direction)
    {
        return;
    }
    m_direction = MathUtil::reduce2Pi(direction);
    fireChangeEvent();
}

void PinkNoiseWindModel::setStandardDeviation(double standardDeviation)
{
    if (standardDeviation == m_standardDeviation)
    {
        return;
    }
    m_standardDeviation = MathUtil::javaMax(standardDeviation, 0);
    fireChangeEvent();
}

double PinkNoiseWindModel::getTurbulenceIntensity() const noexcept
{
    if (MathUtil::equals(m_average, 0))
    {
        return MathUtil::equals(m_standardDeviation, 0) ? 0 : 1;
    }
    return m_standardDeviation / m_average;
}

void PinkNoiseWindModel::setTurbulenceIntensity(double intensity)
{
    setStandardDeviation(intensity * m_average);
}

std::string_view PinkNoiseWindModel::getIntensityDescriptionKey() const noexcept
{
    const double i = getTurbulenceIntensity();
    if (i < 0.001)
    {
        return "simedtdlg.IntensityDesc.None";
    }
    if (i < 0.05)
    {
        return "simedtdlg.IntensityDesc.Verylow";
    }
    if (i < 0.10)
    {
        return "simedtdlg.IntensityDesc.Low";
    }
    if (i < 0.15)
    {
        return "simedtdlg.IntensityDesc.Medium";
    }
    if (i < 0.20)
    {
        return "simedtdlg.IntensityDesc.High";
    }
    if (i < 0.25)
    {
        return "simedtdlg.IntensityDesc.Veryhigh";
    }
    return "simedtdlg.IntensityDesc.Extreme";
}

Coordinate PinkNoiseWindModel::getWindVelocity(double time, double altitudeMsl,
                                               double /*altitudeAgl*/)
{
    return getWindVelocity(time, altitudeMsl);
}

Coordinate PinkNoiseWindModel::getWindVelocity(double time, double /*altitude*/)
{
    if (time < 0)
    {
        bug("Requesting wind speed at t=" + Strings::javaDoubleToString(time));
    }

    // Java resets and recurses when an earlier time is asked for; starting the source again here
    // is the same, since the new source starts at time 0 <= time.
    if (!m_randomSource.has_value() || time < m_time1)
    {
        // The seed keeps its bit pattern, as java.util.Random's long seed keeps the int's value.
        m_randomSource.emplace(kAlpha, kPoles, static_cast<std::uint32_t>(m_seed));
        m_time1  = 0;
        m_value1 = m_randomSource->nextValue();
        m_value2 = m_randomSource->nextValue();
    }

    while (m_time1 + kDeltaT < time)
    {
        m_value1 = m_value2;
        m_value2 = m_randomSource->nextValue();
        m_time1 += kDeltaT;
    }

    const double a = (time - m_time1) / kDeltaT;

    const double speed =
        m_average + (((m_value1 * (1 - a)) + (m_value2 * a)) * m_standardDeviation / kStdDev);
    return Coordinate{speed * std::sin(m_direction), speed * std::cos(m_direction), 0};
}

void PinkNoiseWindModel::loadFrom(const PinkNoiseWindModel& source) noexcept
{
    m_average           = source.m_average;
    m_direction         = source.m_direction;
    m_standardDeviation = source.m_standardDeviation;
}

void PinkNoiseWindModel::loadFrom(const Preferences& preferences)
{
    const double average             = preferences.getWindAverage();
    const double turbulenceIntensity = preferences.getWindTurbulenceIntensity();
    const double direction           = preferences.getWindDirection();

    setAverage(average);
    setTurbulenceIntensity(turbulenceIntensity);
    setDirection(direction);
}

void PinkNoiseWindModel::storeTo(Preferences& preferences) const
{
    // Read everything first: each setter emits the preferences' changed(), and a listener may
    // come back to this model.
    const double average             = getAverage();
    const double turbulenceIntensity = getTurbulenceIntensity();
    const double direction           = getDirection();

    preferences.setWindAverage(average);
    preferences.setWindTurbulenceIntensity(turbulenceIntensity);
    preferences.setWindDirection(direction);
}

std::unique_ptr<WindModel> PinkNoiseWindModel::clone() const
{
    return std::make_unique<PinkNoiseWindModel>(*this);
}

bool PinkNoiseWindModel::operator==(const PinkNoiseWindModel& other) const noexcept
{
    return MathUtil::javaDoubleCompare(other.m_average, m_average) == 0 &&
           MathUtil::javaDoubleCompare(other.m_standardDeviation, m_standardDeviation) == 0 &&
           MathUtil::javaDoubleCompare(other.m_direction, m_direction) == 0 &&
           m_seed == other.m_seed;
}

int PinkNoiseWindModel::hashCode() const noexcept
{
    int result = 17;
    result     = MathUtil::javaHashCombine(result, MathUtil::javaDoubleHashCode(m_average));
    result = MathUtil::javaHashCombine(result, MathUtil::javaDoubleHashCode(m_standardDeviation));
    result = MathUtil::javaHashCombine(result, MathUtil::javaDoubleHashCode(m_direction));
    result = MathUtil::javaHashCombine(result, m_seed);
    return result;
}

}  // namespace QtRocket
