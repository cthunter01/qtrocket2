#pragma once

#include "QtRocket/simulation/exception/SimulationException.h"

namespace QtRocket
{

/// Thrown when a simulation was cancelled (OpenRocket's simulation/exception/
/// SimulationCancelledException): by the user, through a listener that watches for the
/// request. It has SimulationException's four constructors: none, a message, a cause, a message
/// and a cause.
class SimulationCancelledException : public SimulationException
{
public:
    using SimulationException::SimulationException;

    /// An exception without a message.
    SimulationCancelledException() = default;
};

}  // namespace QtRocket
