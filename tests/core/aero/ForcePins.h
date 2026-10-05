#pragma once

// The aerodynamic forces OpenRocket's own calculators computed, as the Java probes printed them,
// and their comparison with an AerodynamicForces (the tests of the Barrowman calculators).
// Test-only.

#include <array>
#include <format>
#include <string_view>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/JavaValueDifferences.h"

namespace QtRocket::Test
{

/// The non-axial forces of a pin: CN, Cm, Cside, Cyaw, Croll, CrollDamp, CrollForce, and the CP's
/// x, y, z and weight (CNa).
using NonAxialPin = std::array<double, 11>;

/// The drag of a pin, as the getters of AerodynamicForces give it (with the component's override
/// applied): the friction, pressure, base and override CD, CD and CDaxial.
using DragPin = std::array<double, 6>;

/// The relative tolerance of a value that sums over the components of a rocket: Java sums them
/// in the order of the components' hash codes, QtRocket in tree order (the golden tolerance of
/// the plan, section 6.4).
inline constexpr double kRocketTolerance = 1e-9;

/// The relative tolerance of a value of one component, which no summation order touches.
inline constexpr double kComponentTolerance = 1e-12;

/// Adds the differences between @p actual and the non-axial pin @p expected to @p diff, each
/// value within @p relativeTolerance (see JavaValueDifferences::pinned()).
inline void compareNonAxial(JavaValueDifferences& diff, std::string_view what,
                            const NonAxialPin& expected, const AerodynamicForces& actual,
                            double relativeTolerance)
{
    const Coordinate cp = actual.getCP();
    diff.pinned(std::format("{}: CN", what), expected[0], actual.getCN(), relativeTolerance);
    diff.pinned(std::format("{}: Cm", what), expected[1], actual.getCm(), relativeTolerance);
    diff.pinned(std::format("{}: Cside", what), expected[2], actual.getCside(), relativeTolerance);
    diff.pinned(std::format("{}: Cyaw", what), expected[3], actual.getCyaw(), relativeTolerance);
    diff.pinned(std::format("{}: Croll", what), expected[4], actual.getCroll(), relativeTolerance);
    diff.pinned(std::format("{}: CrollDamp", what), expected[5], actual.getCrollDamp(),
                relativeTolerance);
    diff.pinned(std::format("{}: CrollForce", what), expected[6], actual.getCrollForce(),
                relativeTolerance);
    diff.pinned(std::format("{}: CP.x", what), expected[7], cp.x, relativeTolerance);
    diff.pinned(std::format("{}: CP.y", what), expected[8], cp.y, relativeTolerance);
    diff.pinned(std::format("{}: CP.z", what), expected[9], cp.z, relativeTolerance);
    diff.pinned(std::format("{}: CNa", what), expected[10], cp.weight, relativeTolerance);
}

/// Adds the differences between the drag of @p actual and the pin @p expected to @p diff.
inline void compareDrag(JavaValueDifferences& diff, std::string_view what, const DragPin& expected,
                        const AerodynamicForces& actual, double relativeTolerance)
{
    diff.pinned(std::format("{}: frictionCD", what), expected[0], actual.getFrictionCD(),
                relativeTolerance);
    diff.pinned(std::format("{}: pressureCD", what), expected[1], actual.getPressureCD(),
                relativeTolerance);
    diff.pinned(std::format("{}: baseCD", what), expected[2], actual.getBaseCD(),
                relativeTolerance);
    diff.pinned(std::format("{}: overrideCD", what), expected[3], actual.getOverrideCD(),
                relativeTolerance);
    diff.pinned(std::format("{}: CD", what), expected[4], actual.getCD(), relativeTolerance);
    diff.pinned(std::format("{}: CDaxial", what), expected[5], actual.getCDaxial(),
                relativeTolerance);
}

}  // namespace QtRocket::Test
