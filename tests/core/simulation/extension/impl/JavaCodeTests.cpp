#include "QtRocket/simulation/extension/impl/JavaCode.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/impl/JavaCodeProvider.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::Config;
using QtRocket::JavaCode;
using QtRocket::JavaCodeProvider;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;

// OpenRocket has no test of JavaCode. The expectations here are the Java source's and the
// output of a Java probe on OpenRocket's compiled core (probes/tier9a-extensions/java:
// StandIns.java, out/standins-java.txt).

static_assert(std::is_final_v<JavaCode>);

/// A JavaCode whose class name is @p className.
[[nodiscard]] JavaCode named(const std::string& className)
{
    JavaCode extension;
    extension.setClassName(className);
    return {extension};
}

/// What initialize() of @p extension does: "ok" and the number of listeners it added, or
/// "error:" and the message of its SimulationException.
[[nodiscard]] std::string initialized(JavaCode& extension)
{
    SimulationConditions conditions;
    try
    {
        extension.initialize(conditions);
    }
    catch (const SimulationException& e)
    {
        return std::string("error:") + e.what();
    }
    return "ok listeners=" + std::to_string(conditions.getSimulationListenerList().size());
}

// StandIns: "java new".
TEST(JavaCode, ANewOneHasNoClass)
{
    const JavaCode extension;
    EXPECT_EQ(JavaCode::kId, "info.openrocket.core.simulation.extension.impl.JavaCode");
    EXPECT_EQ(extension.getId(), JavaCode::kId);
    EXPECT_EQ(extension.getName(), "Java code: none");
    EXPECT_EQ(extension.getDescription(), std::nullopt);
    EXPECT_TRUE(extension.isMonteCarloSafe());
    EXPECT_TRUE(extension.getFlightDataTypes().empty());
    EXPECT_EQ(extension.getClassName(), "");
    EXPECT_TRUE(extension.getConfig().keySet().empty());
}

// StandIns: "java named", "java blank", "java number".
TEST(JavaCode, NamesItselfByItsClass)
{
    const JavaCode listener = named("com.example.MyListener");
    EXPECT_EQ(listener.getName(), "Java code: com.example.MyListener");
    EXPECT_EQ(listener.getClassName(), "com.example.MyListener");
    EXPECT_FALSE(listener.isMonteCarloSafe());
    EXPECT_TRUE(listener.getConfig().get("className") == Config::Value{"com.example.MyListener"});

    // A blank name is no name (StringUtils.isEmpty() trims), but it is kept as it is.
    const JavaCode blank = named("   ");
    EXPECT_EQ(blank.getName(), "Java code: none");
    EXPECT_EQ(blank.getClassName(), "   ");
    EXPECT_TRUE(blank.isMonteCarloSafe());

    // An entry that is no String is no name either.
    Config number;
    number.put("className", 7);
    JavaCode extension;
    extension.setConfig(number);
    EXPECT_EQ(extension.getClassName(), "");
    EXPECT_EQ(extension.getName(), "Java code: none");
}

TEST(JavaCode, TheSetterStoresAndAnnounces)
{
    JavaCode                                   extension;
    int                                        events = 0;
    const QtRocket::Signal<>::ScopedConnection connection{
        extension.changed().connect([&events] { ++events; })};

    extension.setClassName("a.B");
    EXPECT_EQ(events, 1);
    extension.setClassName("a.B");
    EXPECT_EQ(events, 2);  // as in Java: always
    EXPECT_EQ(extension.getConfig().keySet(), (std::vector<std::string>{"className"}));
}

TEST(JavaCode, CloneCopiesTheConfiguration)
{
    JavaCode extension;
    extension.setClassName("a.B");

    const std::unique_ptr<SimulationExtension> copy  = extension.clone();
    auto*                                      clone = dynamic_cast<JavaCode*>(copy.get());
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getId(), JavaCode::kId);
    EXPECT_EQ(clone->getClassName(), "a.B");

    clone->setClassName("c.D");
    EXPECT_EQ(extension.getClassName(), "a.B");
}

// StandIns: "java initialize ...". Without a class nothing happens; any class is one that
// cannot be found, with OpenRocket's message for that (the name as it is, not trimmed).
// OpenRocket finds java.lang.String and says that it is no SimulationListener; here no class
// is ever found.
TEST(JavaCode, InitializeCannotFindAnyClass)
{
    JavaCode none;
    EXPECT_EQ(initialized(none), "ok listeners=0");
    JavaCode blank = named("   ");
    EXPECT_EQ(initialized(blank), "ok listeners=0");

    JavaCode missing = named("com.example.MyListener");
    EXPECT_EQ(initialized(missing), "error:Could not find class com.example.MyListener");
    JavaCode padded = named(" com.example.MyListener ");
    EXPECT_EQ(initialized(padded), "error:Could not find class  com.example.MyListener ");
    JavaCode javaClass = named("java.lang.String");
    EXPECT_EQ(initialized(javaClass), "error:Could not find class java.lang.String");
}

// A simulation with a JavaCode that names a class does not run, as in an OpenRocket that lacks
// the class: simulate() returns the extension's exception as an ordinary error.
TEST(JavaCode, ASimulationWithAListenerClassDoesNotRun)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    simulation.getOptions().setRandomSeed(0);
    const std::shared_ptr<JavaCode> code = std::make_shared<JavaCode>();
    simulation.getSimulationExtensions().push_back(code);

    // Without a class name the extension does nothing.
    const QtRocket::Result<void> flown = simulation.simulate();
    ASSERT_TRUE(flown.has_value()) << flown.error().message;

    code->setClassName("net.sf.openrocket.simulation.listeners.example.AirStart");
    const QtRocket::Result<void> refused = simulation.simulate();
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, QtRocket::ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(refused.error().message,
              "Could not find class net.sf.openrocket.simulation.listeners.example.AirStart");
    EXPECT_EQ(simulation.getSimulatedData(), nullptr);
}

TEST(JavaCodeProvider, MakesJavaCodesUnderOpenRocketsMenuName)
{
    const JavaCodeProvider provider;
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{std::string(JavaCode::kId)}));
    EXPECT_EQ(provider.getName(JavaCode::kId),
              (std::vector<std::string>{"Scripts", "Java listeners"}));
    EXPECT_EQ(provider.getName("something.Else"), std::nullopt);

    const std::unique_ptr<SimulationExtension> made = provider.getInstance(JavaCode::kId);
    ASSERT_NE(made, nullptr);
    EXPECT_NE(dynamic_cast<JavaCode*>(made.get()), nullptr);
    EXPECT_TRUE(made->getConfig().keySet().empty());
}

}  // namespace
