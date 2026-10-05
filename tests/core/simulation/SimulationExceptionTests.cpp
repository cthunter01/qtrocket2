#include "QtRocket/simulation/exception/SimulationException.h"

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationListenerException.h"

namespace
{

using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::SimulationCalculationException;
using QtRocket::SimulationCancelledException;
using QtRocket::SimulationException;
using QtRocket::SimulationListenerException;

// Java's hierarchy: the three specialised exceptions are SimulationExceptions.
static_assert(std::is_base_of_v<std::runtime_error, SimulationException>);
static_assert(std::is_base_of_v<SimulationException, SimulationCalculationException>);
static_assert(std::is_base_of_v<SimulationException, SimulationCancelledException>);
static_assert(std::is_base_of_v<SimulationException, SimulationListenerException>);
static_assert(!std::is_base_of_v<SimulationCancelledException, SimulationListenerException>);
// A thrown object must be copied without throwing.
static_assert(std::is_nothrow_copy_constructible_v<SimulationException>);
static_assert(std::is_nothrow_copy_constructible_v<SimulationCalculationException>);
static_assert(std::is_nothrow_copy_constructible_v<SimulationCancelledException>);
static_assert(std::is_nothrow_copy_constructible_v<SimulationListenerException>);
// The constructors from a single argument do not convert.
static_assert(!std::is_convertible_v<std::string, SimulationException>);
static_assert(!std::is_convertible_v<const std::exception&, SimulationCancelledException>);

[[nodiscard]] std::shared_ptr<FlightDataBranch> someBranch()
{
    const std::vector<const FlightDataType*> types{
        &FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME)};
    return std::make_shared<FlightDataBranch>("B", types);
}

/// A copy of @p exception as a plain SimulationException, as a handler that catches by value or
/// a container of exceptions would make one.
[[nodiscard]] SimulationException copyOf(const SimulationException& exception)
{
    return exception;
}

// The messages are those of Java's getMessage() (probes/events-data-impl/MiscProbe.java,
// "exceptions"), but for the one documented deviation.

TEST(SimulationException, WithoutAMessage)
{
    const SimulationException exception;
    EXPECT_FALSE(exception.hasMessage());
    EXPECT_EQ(exception.getMessage(), std::nullopt) << "Java: null";
    EXPECT_STREQ(exception.what(), "");
    EXPECT_FALSE(exception.hasCause());
    EXPECT_EQ(exception.getCauseMessage(), std::nullopt);
}

TEST(SimulationException, WithAMessage)
{
    const SimulationException exception("msg");
    EXPECT_TRUE(exception.hasMessage());
    EXPECT_EQ(exception.getMessage(), "msg");
    EXPECT_STREQ(exception.what(), "msg");
    EXPECT_FALSE(exception.hasCause());

    const SimulationException empty("");
    EXPECT_TRUE(empty.hasMessage()) << "an empty message is a message";
    EXPECT_EQ(empty.getMessage(), "");
}

TEST(SimulationException, WithACauseTakesTheCausesMessage)
{
    const std::logic_error    cause("inner");
    const SimulationException exception(cause);
    EXPECT_TRUE(exception.hasMessage());
    // Deviation: Java's message is the cause's toString(),
    // "java.lang.IllegalStateException: inner".
    EXPECT_EQ(exception.getMessage(), "inner");
    EXPECT_STREQ(exception.what(), "inner");
    EXPECT_TRUE(exception.hasCause());
    EXPECT_EQ(exception.getCauseMessage(), "inner");
}

TEST(SimulationException, WithAMessageAndACause)
{
    const std::logic_error    cause("inner");
    const SimulationException exception("msg", cause);
    EXPECT_EQ(exception.getMessage(), "msg");
    EXPECT_STREQ(exception.what(), "msg");
    EXPECT_TRUE(exception.hasCause());
    EXPECT_EQ(exception.getCauseMessage(), "inner");
}

TEST(SimulationException, ACopyKeepsEverything)
{
    const std::logic_error    cause("inner");
    const SimulationException original("msg", cause);
    const SimulationException copy = copyOf(original);
    EXPECT_EQ(copy.getMessage(), "msg");
    EXPECT_EQ(copy.getCauseMessage(), "inner");

    const SimulationException none;
    const SimulationException noneCopy = copyOf(none);
    EXPECT_FALSE(noneCopy.hasMessage());
    EXPECT_FALSE(noneCopy.hasCause());
}

TEST(SimulationException, CopyingFromADerivedExceptionIsNotAWrapping)
{
    const SimulationCancelledException cancelled("stop");
    const SimulationException          copy = copyOf(cancelled);
    EXPECT_EQ(copy.getMessage(), "stop");
    EXPECT_FALSE(copy.hasCause()) << "a copy, as documented";

    const SimulationException wrapped("wrapped", cancelled);
    EXPECT_EQ(wrapped.getMessage(), "wrapped");
    EXPECT_EQ(wrapped.getCauseMessage(), "stop");

    // Another class of the family wraps.
    const SimulationListenerException listener(cancelled);
    EXPECT_EQ(listener.getMessage(), "stop");
    EXPECT_EQ(listener.getCauseMessage(), "stop");
}

TEST(SimulationCalculationException, CarriesTheBranch)
{
    const SimulationCalculationException none;
    EXPECT_EQ(none.getMessage(), std::nullopt);
    EXPECT_EQ(none.getFlightDataBranch(), nullptr);

    const std::shared_ptr<FlightDataBranch> branch = someBranch();
    const SimulationCalculationException    message("calc", branch);
    EXPECT_EQ(message.getMessage(), "calc");
    EXPECT_EQ(message.getFlightDataBranch(), branch) << "the branch itself, not a copy";
    EXPECT_FALSE(message.hasCause());

    const std::logic_error               cause("inner");
    const SimulationCalculationException fromCause(cause, branch);
    EXPECT_EQ(fromCause.getMessage(), "inner");
    EXPECT_EQ(fromCause.getCauseMessage(), "inner");
    EXPECT_EQ(fromCause.getFlightDataBranch(), branch);

    const SimulationCalculationException both("calc", cause, branch);
    EXPECT_EQ(both.getMessage(), "calc");
    EXPECT_EQ(both.getCauseMessage(), "inner");
    EXPECT_EQ(both.getFlightDataBranch(), branch);

    const SimulationCalculationException noBranch("calc", nullptr);
    EXPECT_EQ(noBranch.getFlightDataBranch(), nullptr);
}

TEST(SimulationCalculationException, KeepsTheBranchAlive)
{
    std::weak_ptr<FlightDataBranch>               weak;
    std::optional<SimulationCalculationException> exception;
    {
        std::shared_ptr<FlightDataBranch> branch = someBranch();
        weak                                     = branch;
        exception.emplace("calc", std::move(branch));
    }
    EXPECT_FALSE(weak.expired());
    const SimulationCalculationException copy = *exception;
    exception.reset();
    EXPECT_FALSE(weak.expired()) << "a copy shares the branch";
    EXPECT_EQ(copy.getFlightDataBranch(), weak.lock());
}

TEST(SimulationCancelledException, HasTheFourConstructors)
{
    const SimulationCancelledException none;
    EXPECT_EQ(none.getMessage(), std::nullopt);

    const SimulationCancelledException message("The simulation was interrupted.");
    EXPECT_EQ(message.getMessage(), "The simulation was interrupted.");
    EXPECT_STREQ(message.what(), "The simulation was interrupted.");

    const std::logic_error             cause("inner");
    const SimulationCancelledException fromCause(cause);
    EXPECT_EQ(fromCause.getMessage(), "inner");
    EXPECT_TRUE(fromCause.hasCause());

    const SimulationCancelledException both("stop", cause);
    EXPECT_EQ(both.getMessage(), "stop");
    EXPECT_EQ(both.getCauseMessage(), "inner");
}

TEST(SimulationListenerException, HasTheFourConstructors)
{
    const SimulationListenerException none;
    EXPECT_EQ(none.getMessage(), std::nullopt);

    const SimulationListenerException message("listener");
    EXPECT_EQ(message.getMessage(), "listener");

    const std::logic_error            cause("inner");
    const SimulationListenerException fromCause(cause);
    EXPECT_EQ(fromCause.getMessage(), "inner");

    const SimulationListenerException both("listener", cause);
    EXPECT_EQ(both.getMessage(), "listener");
    EXPECT_EQ(both.getCauseMessage(), "inner");
}

/// Throws @p exception and reports what a handler of SimulationException sees of it: the
/// message, and whether it still is of the class @p Thrown.
template <class Thrown>
[[nodiscard]] std::string caughtAsSimulationException(const Thrown& exception)
{
    try
    {
        throw exception;
    }
    catch (const SimulationException& caught)
    {
        const bool kept = dynamic_cast<const Thrown*>(&caught) != nullptr;
        return std::string{caught.what()} + (kept ? " (kept its class)" : " (sliced)");
    }
}

TEST(SimulationException, TheFamilyIsCaughtByItsBase)
{
    EXPECT_EQ(caughtAsSimulationException(SimulationException("plain")), "plain (kept its class)");
    EXPECT_EQ(caughtAsSimulationException(SimulationCalculationException("calc", someBranch())),
              "calc (kept its class)");
    EXPECT_EQ(caughtAsSimulationException(SimulationCancelledException("cancelled")),
              "cancelled (kept its class)");
    EXPECT_EQ(caughtAsSimulationException(SimulationListenerException("listener")),
              "listener (kept its class)");
}

TEST(SimulationException, IsAStandardException)
{
    EXPECT_THROW(throw SimulationCancelledException("cancelled"), std::runtime_error);
    EXPECT_THROW(throw SimulationCalculationException("calc", nullptr), std::exception);
    EXPECT_THROW(throw SimulationListenerException("listener"), SimulationException);
}

}  // namespace
