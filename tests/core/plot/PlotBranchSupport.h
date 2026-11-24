#pragma once

#include <initializer_list>
#include <string>
#include <utility>

#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"

// Small flight data branches for the tests of the plot model, the same as those of the probe
// tools/openrocket-probes/plot-configuration (PlotConfigurationProbe), so that the tests can pin
// what OpenRocket's PlotConfiguration gives for them.

namespace QtRocket::Test
{

/// Time 0, 1, ... with @p values of @p type (the probe's series()); no rows for no values.
[[nodiscard]] inline FlightDataBranch plotSeries(std::string name, const FlightDataType& type,
                                                 std::initializer_list<double> values)
{
    const FlightDataType& time = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    FlightDataBranch      branch(std::move(name), {time, type});
    double                t = 0;
    for (const double value : values)
    {
        branch.addPoint();
        branch.setValue(time, t);
        branch.setValue(type, value);
        t += 1;
    }
    return branch;
}

/// Time 0 to 10 s with an altitude from @p alt0 to @p alt10 and a vertical velocity from @p vel0
/// to @p vel10, linearly (the probe's linear()).
[[nodiscard]] inline FlightDataBranch plotLinear(double alt0, double alt10, double vel0,
                                                 double vel10)
{
    const FlightDataType& time     = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const FlightDataType& velocity = FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z);
    FlightDataBranch      branch("b", {time, altitude, velocity});
    for (int i = 0; i <= 10; ++i)
    {
        branch.addPoint();
        branch.setValue(time, i);
        branch.setValue(altitude, alt0 + ((alt10 - alt0) * i / 10));
        branch.setValue(velocity, vel0 + ((vel10 - vel0) * i / 10));
    }
    return branch;
}

/// Ten seconds of a flight with altitude, vertical velocity and vertical acceleration (the probe's
/// flightBranch).
[[nodiscard]] inline FlightDataBranch plotFlight()
{
    const FlightDataType& time     = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const FlightDataType& velocity = FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_Z);
    const FlightDataType& acceleration =
        FlightDataType::builtin(FlightDataTypeId::TYPE_ACCELERATION_Z);
    FlightDataBranch branch("flight", {time, altitude, velocity, acceleration});
    for (int i = 0; i <= 10; ++i)
    {
        const double t = i;
        branch.addPoint();
        branch.setValue(time, t);
        branch.setValue(altitude, (50 * t) - (4.9 * t * t));
        branch.setValue(velocity, 50 - (9.8 * t));
        branch.setValue(acceleration, -9.8 + (i < 2 ? 40 : 0));
    }
    return branch;
}

}  // namespace QtRocket::Test
