#include "QtRocket/util/PinkNoise.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <random>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

constexpr int kDefaultPoles = 5;

}  // namespace

PinkNoise::PinkNoise() : PinkNoise(1.0, kDefaultPoles) { }

PinkNoise::PinkNoise(double alpha) : PinkNoise(alpha, kDefaultPoles) { }

PinkNoise::PinkNoise(double alpha, int poles) : PinkNoise(alpha, poles, std::random_device{}()) { }

PinkNoise::PinkNoise(double alpha, int poles, std::uint32_t seed) : m_generator(seed)
{
    if (poles < 0)
    {
        bug(std::format("PinkNoise: negative number of poles {}", poles));
    }
    const auto count = static_cast<std::size_t>(poles);
    m_multipliers.resize(count);
    m_values.assign(count, 0.0);

    double a = 1;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto index = static_cast<double>(i);
        a                = (index - (alpha / 2)) * a / (index + 1);
        m_multipliers[i] = a;
    }

    // Fill the history with random values
    for (std::size_t i = 0; i < 5 * count; ++i)
    {
        nextValue();
    }
}

double PinkNoise::nextValue()
{
    double x = m_gaussian(m_generator);

    for (std::size_t i = 0; i < m_values.size(); ++i)
    {
        x -= m_multipliers[i] * m_values[i];
    }
    if (!m_values.empty())
    {
        std::ranges::shift_right(m_values, 1);
        m_values[0] = x;
    }

    return x;
}

}  // namespace QtRocket
