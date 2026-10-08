#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/impl/ScriptingProvider.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::Config;
using QtRocket::ScriptingExtension;
using QtRocket::ScriptingProvider;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;
using QtRocket::WarningSet;

// OpenRocket's tests of the scripting extension (TestScriptingUtil) are about its trusted-script
// bookkeeping, which is not ported: QtRocket never runs a script. The expectations here are the
// Java source's and the output of a Java probe on OpenRocket's compiled core
// (probes/tier9a-extensions/java: StandIns.java, out/standins-java.txt).

static_assert(std::is_final_v<ScriptingExtension>);

/// A ScriptingExtension with the configuration @p config, as the .ork reader makes one.
[[nodiscard]] ScriptingExtension loaded(const Config& config)
{
    ScriptingExtension extension;
    extension.setConfig(config);
    return {extension};
}

/// What initialize() of @p extension does: "ok" and the number of listeners it added, or
/// "error:" and the message of its SimulationException.
[[nodiscard]] std::string initialized(ScriptingExtension& extension)
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

// StandIns: "script new".
TEST(ScriptingExtension, ANewOneIsAnEnabledEmptyJavaScript)
{
    const ScriptingExtension extension;
    EXPECT_EQ(ScriptingExtension::kId,
              "info.openrocket.core.simulation.extension.impl.ScriptingExtension");
    EXPECT_EQ(extension.getId(), ScriptingExtension::kId);
    EXPECT_EQ(extension.getName(), "JavaScript script");
    EXPECT_EQ(extension.getDescription(), "Extend OpenRocket simulations by custom scripts.");
    EXPECT_FALSE(extension.isMonteCarloSafe());
    EXPECT_TRUE(extension.getFlightDataTypes().empty());
    EXPECT_TRUE(extension.getInputNumbers().empty());
    EXPECT_EQ(extension.getLanguage(), "JavaScript");
    EXPECT_EQ(extension.getScript(), "");
    EXPECT_TRUE(extension.isEnabled());

    // The constructor's three settings, in its order, with Java's types.
    const Config config = extension.getConfig();
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"language", "script", "enabled"}));
    EXPECT_TRUE(config.get("language") == Config::Value{"JavaScript"});
    EXPECT_TRUE(config.get("script") == Config::Value{""});
    EXPECT_TRUE(config.get("enabled") == Config::Value{true});
}

// StandIns: "script empty config", "python disabled", "enabled only", "enabled as text",
// "language number".
TEST(ScriptingExtension, ReadsItsSettingsFromAConfiguration)
{
    // Without the keys: the default language, no script, and not enabled.
    const ScriptingExtension empty = loaded(Config{});
    EXPECT_EQ(empty.getName(), "JavaScript script");
    EXPECT_EQ(empty.getLanguage(), "JavaScript");
    EXPECT_EQ(empty.getScript(), "");
    EXPECT_FALSE(empty.isEnabled());
    EXPECT_TRUE(empty.isMonteCarloSafe());
    EXPECT_TRUE(empty.getConfig().keySet().empty());

    // As the scripts of OpenRocket's example design are stored.
    Config stored;
    stored.put("language", "Python");
    stored.put("script", "x = 1");
    stored.put("enabled", false);
    const ScriptingExtension python = loaded(stored);
    EXPECT_EQ(python.getName(), "Python script");
    EXPECT_EQ(python.getLanguage(), "Python");
    EXPECT_EQ(python.getScript(), "x = 1");
    EXPECT_FALSE(python.isEnabled());
    EXPECT_TRUE(python.isMonteCarloSafe());
    EXPECT_TRUE(python.getConfig().sameEntries(stored));

    Config enabledOnly;
    enabledOnly.put("enabled", true);
    EXPECT_TRUE(loaded(enabledOnly).isEnabled());
    EXPECT_FALSE(loaded(enabledOnly).isMonteCarloSafe());

    // A setting of another type is the default.
    Config asText;
    asText.put("enabled", "true");
    EXPECT_FALSE(loaded(asText).isEnabled());
    Config number;
    number.put("language", 5);
    EXPECT_EQ(loaded(number).getLanguage(), "JavaScript");
    EXPECT_EQ(loaded(number).getName(), "JavaScript script");
}

// StandIns: "script language [...]". The language goes into the name as it is; OpenRocket
// treats it as a replacement pattern, throws for "a$b" and prints "ab script" for the last.
TEST(ScriptingExtension, NamesItselfByItsLanguage)
{
    ScriptingExtension extension;
    extension.setLanguage("");
    EXPECT_EQ(extension.getName(), " script");
    extension.setLanguage(" ");
    EXPECT_EQ(extension.getName(), "  script");
    extension.setLanguage("a$b");
    EXPECT_EQ(extension.getName(), "a$b script");
    extension.setLanguage("a\\b");
    EXPECT_EQ(extension.getName(), "a\\b script");
}

TEST(ScriptingExtension, TheSettersStoreWithoutAnnouncing)
{
    ScriptingExtension                         extension;
    int                                        events = 0;
    const QtRocket::Signal<>::ScopedConnection connection{
        extension.changed().connect([&events] { ++events; })};

    extension.setLanguage("Python");
    extension.setScript("print(1)");
    extension.setEnabled(false);

    EXPECT_EQ(events, 0);  // as in Java: no fireChangeEvent() in these setters
    EXPECT_EQ(extension.getLanguage(), "Python");
    EXPECT_EQ(extension.getScript(), "print(1)");
    EXPECT_FALSE(extension.isEnabled());
    // The keys keep their places.
    EXPECT_EQ(extension.getConfig().keySet(),
              (std::vector<std::string>{"language", "script", "enabled"}));
}

TEST(ScriptingExtension, CloneCopiesTheConfiguration)
{
    ScriptingExtension extension;
    extension.setScript("function startSimulation() {}");
    extension.setEnabled(false);

    const std::unique_ptr<SimulationExtension> copy = extension.clone();
    auto* clone                                     = dynamic_cast<ScriptingExtension*>(copy.get());
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getId(), ScriptingExtension::kId);
    EXPECT_TRUE(clone->getConfig().sameEntries(extension.getConfig()));

    clone->setEnabled(true);
    EXPECT_FALSE(extension.isEnabled());
}

// What documentLoaded() does (it takes an OpenRocketDocument, which only the document tier can
// make; its body is this call): QtRocket trusts no script, so an enabled one is disabled, with
// OpenRocket's warning for an untrusted script.
TEST(ScriptingExtension, AnEnabledScriptIsDisabledWhenItsDocumentIsLoaded)
{
    ScriptingExtension extension;
    extension.setScript("function startSimulation() {}");
    ASSERT_TRUE(extension.isEnabled());
    WarningSet warnings;

    extension.disableUntrustedScript(warnings);

    EXPECT_FALSE(extension.isEnabled());
    EXPECT_EQ(extension.getScript(), "function startSimulation() {}");
    ASSERT_EQ(warnings.size(), 1U);
    // The text of SimulationExtension.scripting.warning.disabled, with its two spaces.
    EXPECT_EQ(ScriptingExtension::kDisabledWarning,
              "Untrusted scripts have been disabled.  You need to manually enable them in the "
              "Simulation options.");
    EXPECT_EQ((*warnings.begin()).toString(), ScriptingExtension::kDisabledWarning);
    EXPECT_EQ((*warnings.begin()).priority(), QtRocket::MessagePriority::NORMAL);
    EXPECT_TRUE(warnings.contains(
        QtRocket::Warning::fromString(std::string(ScriptingExtension::kDisabledWarning))));

    // A second script of the document adds the same warning: the set holds it once.
    ScriptingExtension second;
    second.disableUntrustedScript(warnings);
    EXPECT_FALSE(second.isEnabled());
    EXPECT_EQ(warnings.size(), 1U);

    // A script that is disabled already is left alone, without a warning.
    WarningSet none;
    extension.disableUntrustedScript(none);
    EXPECT_FALSE(extension.isEnabled());
    EXPECT_TRUE(none.empty());
}

// StandIns: "script initialize disabled" and "... empty config": a disabled script adds no
// listener. An enabled one cannot run here.
TEST(ScriptingExtension, InitializeDoesNothingForADisabledScriptAndRefusesAnEnabledOne)
{
    ScriptingExtension disabled;
    disabled.setEnabled(false);
    EXPECT_EQ(initialized(disabled), "ok listeners=0");
    ScriptingExtension empty = loaded(Config{});
    EXPECT_EQ(initialized(empty), "ok listeners=0");

    ScriptingExtension enabled;
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'JavaScript'");
    enabled.setLanguage("Python");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'Python'");
}

// A simulation flies with a disabled script as without it, and does not fly with an enabled
// one: simulate() returns the extension's exception as an ordinary error.
TEST(ScriptingExtension, ASimulationRunsWithADisabledScriptOnly)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    simulation.getOptions().setRandomSeed(0);
    const std::shared_ptr<ScriptingExtension> script = std::make_shared<ScriptingExtension>();
    simulation.getSimulationExtensions().push_back(script);

    const QtRocket::Result<void> refused = simulation.simulate();
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, QtRocket::ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(refused.error().message,
              "QtRocket does not support the scripting language 'JavaScript'");
    EXPECT_EQ(simulation.getSimulatedData(), nullptr);

    WarningSet warnings;
    script->disableUntrustedScript(warnings);
    const QtRocket::Result<void> flown = simulation.simulate();
    ASSERT_TRUE(flown.has_value()) << flown.error().message;
    const std::shared_ptr<QtRocket::FlightData>& data = simulation.getSimulatedData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getBranchCount(), 1U);
    // No listener of the script took part: nothing says that a listener affected the flight.
    EXPECT_FALSE(data->getWarningSet().contains(QtRocket::Warning::kListenersAffected));
}

TEST(ScriptingProvider, MakesScriptingExtensionsUnderOpenRocketsMenuName)
{
    const ScriptingProvider provider;
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{std::string(ScriptingExtension::kId)}));
    EXPECT_EQ(provider.getName(ScriptingExtension::kId),
              (std::vector<std::string>{"Scripts", "JavaScript"}));
    EXPECT_EQ(provider.getName("something.Else"), std::nullopt);

    const std::unique_ptr<SimulationExtension> made = provider.getInstance(ScriptingExtension::kId);
    ASSERT_NE(made, nullptr);
    auto* script = dynamic_cast<ScriptingExtension*>(made.get());
    ASSERT_NE(script, nullptr);
    // A new one, enabled as in Java: the reader's setConfig() replaces that with the file's.
    EXPECT_TRUE(script->isEnabled());
}

}  // namespace
