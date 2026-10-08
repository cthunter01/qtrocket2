#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

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
        // HOOK(scripting): Java asks util.isTrustedScript(getLanguage(), getScript()) and leaves
        // a trusted script enabled. No script is trusted here.
        setEnabled(false);
        warnings.add(Warning::fromString(std::string(kDisabledWarning)));
    }
}

void ScriptingExtension::initialize(SimulationConditions& /*conditions*/)
{
    if (isEnabled())
    {
        // HOOK(scripting): Java adds getListener() to the conditions, the listener that runs the
        // script, and throws when the JRE has no engine for the language. There is no engine
        // for any language here.
        throw SimulationException(
            std::format("QtRocket does not support the scripting language '{}'", getLanguage()));
    }
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
