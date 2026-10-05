#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace QtRocket
{

/// Thrown when a simulation cannot go on (OpenRocket's simulation/exception/
/// SimulationException): the one exception family that crosses the simulation loop. A stepper,
/// the engine or a simulation listener throws it; the engine tells the listeners
/// (endSimulationBranch(), endSimulation()), records an EXCEPTION flight event with the message
/// and lets it out, and whoever started the simulation catches it at the boundary.
/// SimulationCalculationException, SimulationCancelledException and
/// SimulationListenerException derive from it, as in Java.
///
/// The message: what() is Java's getMessage(), and "" where Java's is null (an exception made
/// without a message); getMessage() tells the two apart, which matters where OpenRocket stores
/// the message (the data of the EXCEPTION event: no data for a null message).
///
/// The cause: C++ has no exception chaining, so a cause is kept as its message
/// (getCauseMessage()). As in Java, an exception made from a cause alone takes its message from
/// the cause.
///
/// Deviations from OpenRocket:
/// - An exception made from a cause alone has the cause's what() as its message; Java's is the
///   cause's toString(), the class name followed by ": " and the message.
/// - Copying an exception from one of its own class or of a derived class is a copy, not a
///   wrapping: SimulationException(aCancelledException) has the same message and no cause. Give
///   a message to wrap one: SimulationException(message, cause).
/// - The copy operations do not throw (the texts are shared), as a C++ exception's must not.
class SimulationException : public std::runtime_error
{
public:
    /// An exception without a message (Java: SimulationException()).
    SimulationException();

    /// An exception with the message @p message.
    explicit SimulationException(const std::string& message);

    /// An exception caused by @p cause, whose message it takes (Java:
    /// SimulationException(Throwable)).
    explicit SimulationException(const std::exception& cause);

    /// An exception with the message @p message, caused by @p cause.
    SimulationException(const std::string& message, const std::exception& cause);

    /// Whether the exception has a message (Java: getMessage() != null).
    [[nodiscard]] bool hasMessage() const noexcept { return m_hasMessage; }

    /// The message (Java: getMessage() and getLocalizedMessage()); nullopt for Java's null.
    [[nodiscard]] std::optional<std::string> getMessage() const;

    /// Whether the exception was made with a cause (Java: getCause() != null).
    [[nodiscard]] bool hasCause() const noexcept { return m_causeMessage != nullptr; }

    /// The what() of the cause; nullopt without a cause.
    [[nodiscard]] std::optional<std::string> getCauseMessage() const;

private:
    bool m_hasMessage;
    /// Shared, so that copying the exception does not allocate.
    std::shared_ptr<const std::string> m_causeMessage;
};

}  // namespace QtRocket
