#pragma once

// A fully specified wind field for the tests that run a simulation in wind: OpenRocket's
// DeterministicWind (core/src/test/java/info/openrocket/core/simulation/DeterministicWind.java).
// Test-only.

#include <memory>
#include <optional>

#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

/// Supplies a fully specified wind field, overriding the simulation's own wind model.
///
/// The turbulence of the bundled PinkNoiseWindModel is a random sequence (and here one from
/// another generator than Java's), which makes a turbulent run unusable as a test fixture.
/// Overriding preWindModel() sidesteps that entirely: the gust is written down rather than
/// sampled, so the scenario is identical on every run and on every machine.
class DeterministicWind final : public CloneableSimulationListener<DeterministicWind>
{
public:
    /// @param baseSpeed steady crosswind, m/s, blowing along +X
    /// @param gustSpeed additional crosswind during the gust, m/s
    /// @param gustStart time at which the gust starts, s
    /// @param gustEnd   time at which the gust ends, s
    DeterministicWind(double baseSpeed, double gustSpeed, double gustStart, double gustEnd)
      : m_baseSpeed(baseSpeed), m_gustSpeed(gustSpeed), m_gustStart(gustStart), m_gustEnd(gustEnd)
    {
    }

    /// A steady crosswind with no gust.
    [[nodiscard]] static std::shared_ptr<DeterministicWind> steady(double speed)
    {
        return std::make_shared<DeterministicWind>(speed, 0, 0, 0);
    }

    [[nodiscard]] std::optional<Coordinate> preWindModel(SimulationStatus& status) override
    {
        const double time    = status.getSimulationTime();
        const bool   gusting = time >= m_gustStart && time < m_gustEnd;
        return Coordinate(m_baseSpeed + (gusting ? m_gustSpeed : 0), 0, 0);
    }

    [[nodiscard]] bool isSystemListener() const override
    {
        // Run as a system listener so that nested simulations keep the same wind.
        return true;
    }

private:
    double m_baseSpeed;
    double m_gustSpeed;
    double m_gustStart;
    double m_gustEnd;
};

}  // namespace QtRocket::Test
