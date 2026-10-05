#pragma once

#include "QtRocket/simulation/exception/SimulationException.h"

namespace QtRocket
{

/// Thrown when a simulation listener fails or misbehaves (OpenRocket's simulation/exception/
/// SimulationListenerException), for example a scripted listener that returns a value of the
/// wrong type. It has SimulationException's four constructors: none, a message, a cause, a
/// message and a cause.
class SimulationListenerException : public SimulationException
{
public:
    using SimulationException::SimulationException;

    /// An exception without a message.
    SimulationListenerException() = default;
};

}  // namespace QtRocket
