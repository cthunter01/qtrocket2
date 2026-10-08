#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

class OpenRocketDocument;
class Simulation;
class SimulationConditions;
class WarningSet;

/// A script attached to a simulation (OpenRocket's simulation/extension/impl/ScriptingExtension),
/// as a class that never executes anything: QtRocket has no script engine and none is planned.
/// The class exists so that a design whose simulations carry scripts loads, shows them by name,
/// keeps their text and settings for a later save, and simulates as OpenRocket simulates a
/// simulation whose scripts are disabled, which is how OpenRocket's own example designs store
/// theirs.
///
/// The configuration (the keys of the Config, which a .ork file stores): "language", "script"
/// and "enabled". A new extension has the language "JavaScript", an empty script and is enabled,
/// as in Java, and holds the three keys in that order. The getters of an extension whose
/// configuration lacks a key (after setConfig() with what a file holds) return "JavaScript", ""
/// and false: a script is disabled unless its configuration says otherwise.
///
/// What an enabled script does here:
/// - documentLoaded() disables it and adds OpenRocket's warning kDisabledWarning to the load
///   warnings. OpenRocket does that with every script the user has not marked as trusted on the
///   computer; QtRocket trusts no script.
/// - initialize() throws a SimulationException, so Simulation::simulate() fails with
///   ErrorCode::SIMULATION_ABORTED instead of running the flight without the script. The text
///   is that of OpenRocket for a language it has no engine for, with the program's name:
///   "QtRocket does not support the scripting language '<language>'" (Java: "Your JRE does not
///   support the scripting language '<language>'").
/// A disabled script does nothing in either.
///
/// Deviations from OpenRocket:
/// - No script is ever run, and nothing of ScriptingUtil and ScriptingSimulationListener is
///   ported (the engines, the trusted hashes kept in the preferences, the listener that calls
///   the script's functions). The two places where they would be used are marked
///   "HOOK(scripting)" in the source file.
/// - getName() puts the language into the text as it is. Java's L10N.replace() gives it to
///   Matcher.replaceAll() as a replacement pattern, where a '$' or a backslash is special: a
///   language such as "a$b" throws there, and "a\\b" loses its backslash.
/// - disableUntrustedScript() is an addition: documentLoaded() without the document and the
///   simulation, which Java's method does not use.
class ScriptingExtension final : public AbstractSimulationExtension
{
public:
    /// OpenRocket's class name: the id a .ork file names the extension by.
    static constexpr std::string_view kId =
        "info.openrocket.core.simulation.extension.impl.ScriptingExtension";

    /// The language of a new extension, and of one whose configuration names none (Java:
    /// DEFAULT_LANGUAGE).
    static constexpr std::string_view kDefaultLanguage = "JavaScript";

    /// The English text of SimulationExtension.scripting.warning.disabled, two spaces after the
    /// first full stop included.
    static constexpr std::string_view kDisabledWarning =
        "Untrusted scripts have been disabled.  You need to manually enable them in the "
        "Simulation options.";

    /// A new, enabled extension with an empty JavaScript script (see the class comment).
    ScriptingExtension();

    /// Whether the script is disabled: a disabled script does nothing.
    [[nodiscard]] bool isMonteCarloSafe() const override;

    /// "<language> script" (the text of SimulationExtension.scripting.name), for example
    /// "JavaScript script".
    [[nodiscard]] std::string getName() const override;

    /// "Extend OpenRocket simulations by custom scripts." (SimulationExtension.scripting.desc).
    [[nodiscard]] std::optional<std::string> getDescription() const override;

    /// disableUntrustedScript(@p warnings).
    void documentLoaded(OpenRocketDocument& document, Simulation& simulation,
                        WarningSet& warnings) override;

    /// What documentLoaded() does: an enabled script is disabled, and kDisabledWarning is added
    /// to @p warnings (a warning set holds the text once, however many scripts add it). Nothing
    /// happens for a disabled script.
    void disableUntrustedScript(WarningSet& warnings);

    /// Nothing for a disabled script.
    /// @throws SimulationException for an enabled script (see the class comment)
    void initialize(SimulationConditions& conditions) override;

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override;

    /// The text of the script.
    [[nodiscard]] std::string getScript() const;
    void                      setScript(std::string_view script);

    /// The scripting language, for example "JavaScript".
    [[nodiscard]] std::string getLanguage() const;
    void                      setLanguage(std::string_view language);

    /// Whether the script is to run. As in Java, the three setters store their value in the
    /// configuration without emitting changed().
    [[nodiscard]] bool isEnabled() const;
    void               setEnabled(bool enabled);
};

}  // namespace QtRocket
