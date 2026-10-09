#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
// make; its body is this call): an enabled script that OpenRocket does not trust on every
// computer is disabled, with OpenRocket's warning for an untrusted script.
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
    second.setScript("var a = 1;");
    second.disableUntrustedScript(warnings);
    EXPECT_FALSE(second.isEnabled());
    EXPECT_EQ(warnings.size(), 1U);

    // A script that is disabled already is left alone, without a warning.
    WarningSet none;
    extension.disableUntrustedScript(none);
    EXPECT_FALSE(extension.isEnabled());
    EXPECT_TRUE(none.empty());
}

/// What disableUntrustedScript() makes of an enabled extension with @p language and @p script:
/// "enabled" or "disabled", and " warned" when it added a warning.
[[nodiscard]] std::string afterLoading(std::string_view language, std::string_view script)
{
    ScriptingExtension extension;
    extension.setLanguage(language);
    extension.setScript(script);
    WarningSet warnings;
    extension.disableUntrustedScript(warnings);
    return std::string(extension.isEnabled() ? "enabled" : "disabled") +
           (warnings.empty() ? "" : " warned");
}

// ScriptingExtension.documentLoaded() with ScriptingUtil.isTrustedScript(): a script that is
// empty once its carriage returns are gone and it is trimmed is trusted, whatever its language,
// and stays enabled without a warning. That is the extension a user has just added. OpenRocket's
// answers: probes/tier9c-verify-top-level/out/script.java.out and
// tier9c-fix-top-level/out/engines.java.out.
TEST(ScriptingExtension, AnEnabledScriptThatHoldsNothingStaysEnabledWhenItsDocumentIsLoaded)
{
    ScriptingExtension fresh;
    WarningSet         warnings;
    fresh.disableUntrustedScript(warnings);
    EXPECT_TRUE(fresh.isEnabled());
    EXPECT_TRUE(warnings.empty());

    EXPECT_EQ(afterLoading("JavaScript", ""), "enabled");
    EXPECT_EQ(afterLoading("JavaScript", "  \r\n  "), "enabled");
    EXPECT_EQ(afterLoading("JavaScript", "\t\n\x0B\x1F \r"), "enabled");
    EXPECT_EQ(afterLoading("Python", ""), "enabled");
    EXPECT_EQ(afterLoading("", " "), "enabled");

    // Without the "script" key the script is the empty one.
    Config enabledOnly;
    enabledOnly.put("enabled", true);
    ScriptingExtension noScript = loaded(enabledOnly);
    noScript.disableUntrustedScript(warnings);
    EXPECT_TRUE(noScript.isEnabled());
    EXPECT_TRUE(warnings.empty());

    // String.trim() takes off what is at or below U+0020 only: a no-break space, an
    // ideographic space and a byte order mark are a script, which is not trusted.
    EXPECT_EQ(afterLoading("JavaScript", "\xC2\xA0"), "disabled warned");
    EXPECT_EQ(afterLoading("JavaScript", "\xE3\x80\x80"), "disabled warned");
    EXPECT_EQ(afterLoading("JavaScript", "\xEF\xBB\xBF"), "disabled warned");
    EXPECT_EQ(afterLoading("JavaScript", "var a = 1;"), "disabled warned");
    EXPECT_EQ(afterLoading("JavaScript", " ; "), "disabled warned");
}

// ScriptingUtil.normalize() and hash(), with the hashes OpenRocket computes (FixEngines.java,
// VerifyScriptProbe.java).
TEST(ScriptingExtension, NormalisesAndHashesAScriptAsOpenRocketDoes)
{
    EXPECT_EQ(ScriptingExtension::normalizeScript(""), "");
    EXPECT_EQ(ScriptingExtension::normalizeScript(" \r\n\t"), "");
    EXPECT_EQ(ScriptingExtension::normalizeScript("a\rb"), "ab");
    EXPECT_EQ(ScriptingExtension::normalizeScript(" a \r\n"), "a");
    EXPECT_EQ(ScriptingExtension::normalizeScript("\r\n  var a = 1;\r\n"), "var a = 1;");
    EXPECT_EQ(ScriptingExtension::normalizeScript("a\r\nb\n"), "a\nb");
    EXPECT_EQ(ScriptingExtension::normalizeScript("\xC2\xA0"), "\xC2\xA0");
    // A carriage return inside what is left after the trimming would be: it is removed first.
    EXPECT_EQ(ScriptingExtension::normalizeScript("\r a\r \r"), "a");

    EXPECT_EQ(ScriptingExtension::scriptHash("JavaScript", ""),
              "SHA-256:f8bcdb9b5562cad29646ebe97bd57cfa448d69692abd0b1d7f463cc5b76fea3a");
    EXPECT_EQ(ScriptingExtension::scriptHash("Python", ""),
              "SHA-256:8a0d35941e60f6971455f250b7f9453a4e4591a12aed79a89953a418430077c6");
    EXPECT_EQ(ScriptingExtension::scriptHash("JavaScript", "var a = 1;"),
              "SHA-256:f7b11f4ec67602eb5b74ee00ffe2fdea520516a1ce59b72258851dcdb6c4bfda");
    // The language is hashed as it is written.
    EXPECT_EQ(ScriptingExtension::scriptHash("js", "var a = 1;"),
              "SHA-256:eaac56c3dd8fb5c5277569e47126dda74c99f1352a4f0f2d2ed3434258903862");
    EXPECT_EQ(ScriptingExtension::scriptHash("", "x"),
              "SHA-256:ba7dc87563c00a0bc9636a69a7b7e4dbdc9d68dfa34f7f7436808eb2ebb7dbde");
    // The bytes are those of UTF-8: a no-break space, and "Caf<e acute>" with "gr<u umlaut>n".
    EXPECT_EQ(ScriptingExtension::scriptHash("JavaScript", "\xC2\xA0"),
              "SHA-256:f66ce07e80db291279b8909c4a50df1f03067d700bf86c3cc421871d8e71ef5e");
    EXPECT_EQ(ScriptingExtension::scriptHash("Caf\xC3\xA9",
                                             "gr\xC3\xBC"
                                             "n"),
              "SHA-256:541271f3d59b6405a8e51380dccd3e75888f7854a8ea6ce427442756c53c514a");

    EXPECT_TRUE(ScriptingExtension::isTrustedScript("JavaScript", ""));
    EXPECT_TRUE(ScriptingExtension::isTrustedScript("Python", " \r\n"));
    EXPECT_FALSE(ScriptingExtension::isTrustedScript("JavaScript", "var a = 1;"));
    EXPECT_FALSE(ScriptingExtension::isTrustedScript("JavaScript", "\r\n  var a = 1;\r\n"));
    EXPECT_FALSE(ScriptingExtension::isTrustedScript("JavaScript", "\xC2\xA0"));
}

/// Those of @p names for which hasScriptEngine() does not answer @p expected, each in quotes.
[[nodiscard]] std::string namesAnsweredOtherwise(std::initializer_list<std::string_view> names,
                                                 bool                                    expected)
{
    std::string wrong;
    for (const std::string_view name : names)
    {
        if (ScriptingExtension::hasScriptEngine(name) != expected)
        {
            wrong += "'" + std::string(name) + "' ";
        }
    }
    return wrong;
}

// ScriptingUtil.getEngineByName(): OpenRocket has one engine, which answers to the names of
// GraalJSScriptEngineFactory, compared as String.equalsIgnoreCase() does (FixEngines.java:
// out/engines.java.out, 45 names).
TEST(ScriptingExtension, KnowsTheLanguagesOpenRocketHasAnEngineFor)
{
    EXPECT_EQ(namesAnsweredOtherwise({"JavaScript",
                                      "javascript",
                                      "JAVASCRIPT",
                                      "js",
                                      "JS",
                                      "Js",
                                      "ECMAScript",
                                      "ecmascript",
                                      "ECMASCRIPT",
                                      "Graal.js",
                                      "graal.js",
                                      "GRAAL.JS",
                                      "Graal-js",
                                      "graal-js",
                                      "GRAAL-JS",
                                      "Graal.JS",
                                      "Graal-JS",
                                      "GraalJS",
                                      "graaljs",
                                      "GRAALJS",
                                      "GraalJSPolyglot",
                                      "graaljspolyglot"},
                                     true),
              "");
    EXPECT_EQ(
        namesAnsweredOtherwise({"nashorn", "Nashorn", "Python", "python", "", " ", " JavaScript",
                                "JavaScript ", "Java Script", "java", "application/javascript",
                                "mjs", "ecma", "ECMAScript 262 Edition 11", "JavaScript\n", "\tjs"},
                               false),
        "");
    // Beyond ASCII, as equalsIgnoreCase() folds: the long s, the dotted capital I, the dotless i.
    EXPECT_TRUE(ScriptingExtension::hasScriptEngine("j\xC5\xBF"));
    EXPECT_TRUE(
        ScriptingExtension::hasScriptEngine("Java\xC5\xBF"
                                            "cript"));
    EXPECT_TRUE(ScriptingExtension::hasScriptEngine("JAVASCR\xC4\xB0PT"));
    EXPECT_TRUE(ScriptingExtension::hasScriptEngine("javascr\xC4\xB1pt"));
    EXPECT_FALSE(ScriptingExtension::hasScriptEngine("Graal\xE2\x84\xAA"));
}

// StandIns: "script initialize disabled" and "... empty config": a disabled script adds no
// listener. An enabled one cannot run here, unless there is nothing in it to run: OpenRocket
// evaluates a script that holds nothing, which gives the simulation no function to call
// (VerifyScriptProbe.java: the flights "FLY ..." of out/script.java.out), but only in a
// language it has an engine for; in another it throws, as for any script in that language.
TEST(ScriptingExtension, InitializeRefusesAnEnabledScriptUnlessItHoldsNothing)
{
    ScriptingExtension disabled;
    disabled.setEnabled(false);
    EXPECT_EQ(initialized(disabled), "ok listeners=0");
    ScriptingExtension empty = loaded(Config{});
    EXPECT_EQ(initialized(empty), "ok listeners=0");

    ScriptingExtension enabled;
    EXPECT_EQ(initialized(enabled), "ok listeners=0");
    enabled.setScript("  \r\n\t  ");
    EXPECT_EQ(initialized(enabled), "ok listeners=0");
    enabled.setLanguage("js");
    EXPECT_EQ(initialized(enabled), "ok listeners=0");

    enabled.setLanguage("Python");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'Python'");
    enabled.setScript("");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'Python'");
    enabled.setLanguage("");
    EXPECT_EQ(initialized(enabled), "error:QtRocket does not support the scripting language ''");

    enabled.setLanguage("JavaScript");
    enabled.setScript("var a = 1;");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'JavaScript'");
    // A no-break space is white space to JavaScript, but nothing here looks into a script
    // beyond blanks, tabs and line ends; a control character is no white space to it at all.
    enabled.setScript("\xC2\xA0");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'JavaScript'");
    enabled.setScript("\x01");
    EXPECT_EQ(initialized(enabled),
              "error:QtRocket does not support the scripting language 'JavaScript'");
}

/// The maximum altitude and the flight time of @p simulation once it has flown, as text, or
/// the failure of simulate().
[[nodiscard]] std::string flightOf(QtRocket::Simulation& simulation)
{
    const QtRocket::Result<void> flown = simulation.simulate();
    if (!flown.has_value())
    {
        return std::string(toString(flown.error().code)) + ": " + flown.error().message;
    }
    const std::shared_ptr<QtRocket::FlightData>& data = simulation.getSimulatedData();
    return std::to_string(data->getMaxAltitude()) + " m, " + std::to_string(data->getFlightTime()) +
           " s, listeners affected: " +
           (data->getWarningSet().contains(QtRocket::Warning::kListenersAffected) ? "yes" : "no");
}

// A simulation flies with a disabled script as without it, and does not fly with an enabled
// one that holds a script: simulate() returns the extension's exception as an ordinary error.
TEST(ScriptingExtension, ASimulationDoesNotRunWithAnEnabledScript)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    simulation.getOptions().setRandomSeed(0);
    const std::shared_ptr<ScriptingExtension> script = std::make_shared<ScriptingExtension>();
    script->setScript("function startSimulation() {}");
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

// A simulation with an enabled script that holds nothing flies as the simulation without the
// extension does (OpenRocket, VerifyScriptProbe.java: the same maximum altitude and flight
// time to the last digit, with the script "", with "  <CR><LF>  " and without the extension),
// and in a language OpenRocket has no engine for it does not fly.
TEST(ScriptingExtension, ASimulationFliesWithAnEnabledScriptThatHoldsNothing)
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    QtRocket::Simulation              plain(*alpha.rocket);
    plain.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    plain.getOptions().setRandomSeed(0);
    const std::string without = flightOf(plain);
    ASSERT_TRUE(without.ends_with("listeners affected: no")) << without;

    QtRocket::Simulation simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
    simulation.getOptions().setRandomSeed(0);
    const std::shared_ptr<ScriptingExtension> script = std::make_shared<ScriptingExtension>();
    simulation.getSimulationExtensions().push_back(script);
    ASSERT_TRUE(script->isEnabled());

    EXPECT_EQ(flightOf(simulation), without);
    script->setScript("  \r\n  ");
    EXPECT_EQ(flightOf(simulation), without);

    script->setLanguage("Python");
    EXPECT_EQ(flightOf(simulation),
              "SIMULATION_ABORTED: QtRocket does not support the scripting language 'Python'");
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
