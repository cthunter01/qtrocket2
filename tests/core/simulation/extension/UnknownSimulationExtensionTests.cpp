#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"

#include <cstdint>
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
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::Config;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;
using QtRocket::UnknownSimulationExtension;

// The class has no counterpart in OpenRocket, whose .ork reader drops an extension whose id no
// provider knows (with the warning whose text notFoundText() is). The tests pin what the reader
// and the writer of the file tier build on: the id and the configuration are kept as they are,
// and a simulation with such an extension does not run.

static_assert(std::is_final_v<UnknownSimulationExtension>);
static_assert(!std::is_default_constructible_v<UnknownSimulationExtension>);
// A std::string is not silently an extension.
static_assert(!std::is_convertible_v<std::string, UnknownSimulationExtension>);

/// A configuration with an entry of every kind a .ork file can hold.
[[nodiscard]] Config fileConfig()
{
    Config config;
    config.put("text", "some text");
    config.put("flag", true);
    config.put("count", 250);
    config.put("big", std::int64_t{4294967296});
    config.put("ratio", 2.5);
    config.put("list", Config::List{Config::Value{1}, Config::Value{"two"},
                                    Config::Value{Config::List{Config::Value{3.0}}}});
    return config;
}

TEST(UnknownSimulationExtension, KeepsTheIdAndTheConfiguration)
{
    const Config                     config = fileConfig();
    const UnknownSimulationExtension extension("com.example.plugin.Wobble", config);

    EXPECT_EQ(extension.getId(), "com.example.plugin.Wobble");
    // Named as an OpenRocket extension that does not name itself: the last part of the id.
    EXPECT_EQ(extension.getName(), "Wobble");
    EXPECT_EQ(extension.getDescription(), std::nullopt);
    EXPECT_FALSE(extension.isMonteCarloSafe());
    EXPECT_TRUE(extension.getFlightDataTypes().empty());
    // It reads none of its numbers: there is nothing for Simulation::validateInputs() to check.
    EXPECT_TRUE(extension.getInputNumbers().empty());

    // Entry for entry, in the file's order, with the types of the file.
    const Config kept = extension.getConfig();
    EXPECT_EQ(kept.keySet(),
              (std::vector<std::string>{"text", "flag", "count", "big", "ratio", "list"}));
    EXPECT_TRUE(kept.sameEntries(config));
    EXPECT_TRUE(kept.get("count") == Config::Value{250});
    EXPECT_FALSE(kept.get("count") == Config::Value{250.0});
    EXPECT_TRUE(kept.get("big") == Config::Value{std::int64_t{4294967296}});
    ASSERT_TRUE(kept.getList("list").has_value());
    EXPECT_EQ(kept.getList("list").value_or(Config::List{}).size(), 3U);
}

TEST(UnknownSimulationExtension, WithoutAConfigurationItHasNone)
{
    const UnknownSimulationExtension extension("NoPackage");
    EXPECT_EQ(extension.getId(), "NoPackage");
    EXPECT_EQ(extension.getName(), "NoPackage");
    EXPECT_TRUE(extension.getConfig().keySet().empty());
    // An id is whatever the file holds, an empty one included.
    EXPECT_EQ(UnknownSimulationExtension("").getId(), "");
    EXPECT_EQ(UnknownSimulationExtension("").getName(), "");
}

TEST(UnknownSimulationExtension, TheConfigurationCanBeSetAndAnnouncesIt)
{
    UnknownSimulationExtension                 extension("a.B");
    int                                        events = 0;
    const QtRocket::Signal<>::ScopedConnection connection{
        extension.changed().connect([&events] { ++events; })};

    extension.setConfig(fileConfig());
    EXPECT_EQ(events, 1);
    EXPECT_TRUE(extension.getConfig().sameEntries(fileConfig()));
}

TEST(UnknownSimulationExtension, CloneKeepsTheIdAndTheConfiguration)
{
    UnknownSimulationExtension extension("com.example.plugin.Wobble", fileConfig());

    const std::unique_ptr<SimulationExtension> copy = extension.clone();
    auto* clone = dynamic_cast<UnknownSimulationExtension*>(copy.get());
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getId(), "com.example.plugin.Wobble");
    EXPECT_EQ(clone->getName(), "Wobble");
    EXPECT_EQ(clone->getConfig().keySet(), extension.getConfig().keySet());
    EXPECT_TRUE(clone->getConfig().sameEntries(extension.getConfig()));

    // A configuration of its own.
    extension.setConfig(Config{});
    EXPECT_TRUE(clone->getConfig().sameEntries(fileConfig()));
    // And a clone of the clone.
    const std::unique_ptr<SimulationExtension> again = clone->clone();
    EXPECT_EQ(again->getId(), "com.example.plugin.Wobble");
    EXPECT_TRUE(again->getConfig().sameEntries(fileConfig()));
}

TEST(UnknownSimulationExtension, TheNotFoundTextIsOpenRocketsWarning)
{
    // importt/SingleSimulationHandler: "Simulation extension with id '" + id + "' not found."
    EXPECT_EQ(UnknownSimulationExtension::notFoundText("com.example.plugin.Wobble"),
              "Simulation extension with id 'com.example.plugin.Wobble' not found.");
    EXPECT_EQ(UnknownSimulationExtension::notFoundText(""),
              "Simulation extension with id '' not found.");
}

TEST(UnknownSimulationExtension, InitializeThrowsNamingTheId)
{
    UnknownSimulationExtension extension("com.example.plugin.Wobble", fileConfig());
    SimulationConditions       conditions;
    try
    {
        extension.initialize(conditions);
        FAIL() << "initialize() returned";
    }
    catch (const SimulationException& e)
    {
        EXPECT_STREQ(e.what(),
                     "Simulation extension with id 'com.example.plugin.Wobble' not found.");
    }
    // Nothing was added to the conditions.
    EXPECT_TRUE(conditions.getSimulationListenerList().empty());
}

// A simulation that says it has an extension that cannot act does not fly without it:
// simulate() fails with the extension's exception, and runs once the extension is taken out.
TEST(UnknownSimulationExtension, ASimulationWithOneDoesNotRun)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    simulation.getOptions().setRandomSeed(0);
    simulation.getSimulationExtensions().push_back(
        std::make_shared<UnknownSimulationExtension>("com.example.plugin.Wobble"));

    const QtRocket::Result<void> refused = simulation.simulate();
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, QtRocket::ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(refused.error().message,
              "Simulation extension with id 'com.example.plugin.Wobble' not found.");
    EXPECT_EQ(simulation.getSimulatedData(), nullptr);

    simulation.getSimulationExtensions().clear();
    const QtRocket::Result<void> flown = simulation.simulate();
    EXPECT_TRUE(flown.has_value()) << (flown.has_value() ? "" : flown.error().message);
}

// Two simulations whose unknown extensions hold the same entries are equal as simulations, and
// another id or another entry makes them differ (Simulation::operator== compares extensions
// by their id and their entries).
TEST(UnknownSimulationExtension, CountsInTheEqualityOfSimulationsByIdAndEntries)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              simulation(*alpha.rocket);
    simulation.getSimulationExtensions().push_back(
        std::make_shared<UnknownSimulationExtension>("a.B", fileConfig()));
    const std::unique_ptr<QtRocket::Simulation> copy = simulation.copy();
    EXPECT_TRUE(simulation == *copy);

    copy->getSimulationExtensions()[0] =
        std::make_shared<UnknownSimulationExtension>("a.C", fileConfig());
    EXPECT_FALSE(simulation == *copy);
    copy->getSimulationExtensions()[0] = std::make_shared<UnknownSimulationExtension>("a.B");
    EXPECT_FALSE(simulation == *copy);
}

}  // namespace
