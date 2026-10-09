#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// ScriptingUtil.DEFAULT_TRUSTED_HASHES: the scripts OpenRocket trusts on every computer.
constexpr std::array<std::string_view, 1> kDefaultTrustedHashes{
    // Roll control script in roll control example file:
    "SHA-256:9bf364ce4d4a75f09b29178bf9d6872b232084f73dae20dc7b5b073e54e95a42"};

/// The names OpenRocket's one script engine answers to (GraalJSScriptEngineFactory.NAMES,
/// without those that differ in the case of their letters only).
constexpr std::array<std::string_view, 7> kEngineNames{
    "js", "JavaScript", "ECMAScript", "Graal.js", "Graal-js", "GraalJS", "GraalJSPolyglot"};

/// Whether @p script holds nothing an engine would run: no character but blanks, tabs and line
/// ends (U+0009 to U+000D and U+0020, all of them white space of JavaScript).
[[nodiscard]] bool holdsNothing(std::string_view script) noexcept
{
    return std::ranges::all_of(script, [](char c) { return c == ' ' || (c >= '\t' && c <= '\r'); });
}

}  // namespace

ScriptingExtension::ScriptingExtension() : AbstractSimulationExtension(std::string(kId))
{
    setLanguage(kDefaultLanguage);
    setScript("");
    setEnabled(true);
}

bool ScriptingExtension::isMonteCarloSafe() const
{
    return !isEnabled();
}

std::string ScriptingExtension::getName() const
{
    // The text of SimulationExtension.scripting.name, "{language} script".
    return std::format("{} script", getLanguage());
}

std::optional<std::string> ScriptingExtension::getDescription() const
{
    return "Extend OpenRocket simulations by custom scripts.";
}

void ScriptingExtension::documentLoaded(OpenRocketDocument& /*document*/,
                                        Simulation& /*simulation*/, WarningSet& warnings)
{
    disableUntrustedScript(warnings);
}

void ScriptingExtension::disableUntrustedScript(WarningSet& warnings)
{
    /*
     * Scripts that the user has not explicitly indicated as trusted are disabled
     * when loading from a file. This is to prevent trojans.
     */
    if (isEnabled())
    {
        if (!isTrustedScript(getLanguage(), getScript()))
        {
            setEnabled(false);
            warnings.add(Warning::fromString(std::string(kDisabledWarning)));
        }
    }
}

void ScriptingExtension::initialize(SimulationConditions& /*conditions*/)
{
    if (isEnabled())
    {
        // HOOK(scripting): Java adds getListener() to the conditions, the listener that runs the
        // script, and throws when the JRE has no engine for the language. There is no engine
        // for any language here. A script that holds nothing has no function for a listener
        // to call, so where Java has an engine such a script is left to do nothing.
        if (hasScriptEngine(getLanguage()) && holdsNothing(getScript()))
        {
            return;
        }
        throw SimulationException(
            std::format("QtRocket does not support the scripting language '{}'", getLanguage()));
    }
}

std::string ScriptingExtension::normalizeScript(std::string_view script)
{
    // script.replaceAll("\r", "").trim()
    std::string text;
    text.reserve(script.size());
    std::ranges::copy_if(script, std::back_inserter(text), [](char c) { return c != '\r'; });
    const std::string_view trimmed = Strings::trim(text);
    return std::string(trimmed);
}

std::string ScriptingExtension::scriptHash(std::string_view language, std::string_view script)
{
    /*
     * NOTE: Hash length must be max 80 chars, the max length of a key in a
     * Properties object.
     */
    std::vector<std::byte> message = stringToBytes(language);
    message.push_back(static_cast<std::byte>('|'));
    const std::vector<std::byte> text = stringToBytes(script);
    message.insert(message.end(), text.begin(), text.end());
    return "SHA-256:" + sha256Hex(message);
}

bool ScriptingExtension::isTrustedScript(std::string_view language, std::string_view script)
{
    const std::string normal = normalizeScript(script);
    if (normal.empty())
    {
        return true;
    }
    const std::string hash = scriptHash(language, normal);
    // HOOK(scripting): Java goes on to ask the preferences whether the user has marked the
    // script with this hash as trusted (ScriptingUtil.setTrustedScript()). QtRocket keeps no
    // such list: it runs no script.
    return std::ranges::any_of(kDefaultTrustedHashes,
                               [&hash](std::string_view trusted) { return trusted == hash; });
}

bool ScriptingExtension::hasScriptEngine(std::string_view language) noexcept
{
    return std::ranges::any_of(kEngineNames, [language](std::string_view name) {
        return Strings::javaEqualsIgnoreCase(name, language);
    });
}

std::unique_ptr<SimulationExtension> ScriptingExtension::clone() const
{
    return std::make_unique<ScriptingExtension>(*this);
}

std::string ScriptingExtension::getScript() const
{
    return m_config.getString("script", "");
}

void ScriptingExtension::setScript(std::string_view script)
{
    m_config.put("script", script);
}

std::string ScriptingExtension::getLanguage() const
{
    return m_config.getString("language", kDefaultLanguage);
}

void ScriptingExtension::setLanguage(std::string_view language)
{
    m_config.put("language", language);
}

bool ScriptingExtension::isEnabled() const
{
    return m_config.getBoolean("enabled", false);
}

void ScriptingExtension::setEnabled(bool enabled)
{
    m_config.put("enabled", enabled);
}

}  // namespace QtRocket
