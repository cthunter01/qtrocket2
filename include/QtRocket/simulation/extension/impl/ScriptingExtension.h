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
/// - documentLoaded() leaves it enabled, without a warning, when OpenRocket trusts it whoever
///   the user is (isTrustedScript()): a script that is empty once its carriage returns are
///   removed and it is trimmed, which is the state of an extension the user has just added,
///   and the one script OpenRocket knows by its hash, the roll control script that its example
///   design of the file format 1.7 stored enabled (today's example holds another script, and
///   stores it disabled). Every other enabled script is disabled there, with OpenRocket's
///   warning kDisabledWarning among the load warnings. OpenRocket also leaves enabled what the
///   user has marked as trusted on the computer; QtRocket keeps no such list.
/// - initialize() does nothing for an enabled script that holds nothing but blanks, tabs and
///   line ends in a language OpenRocket has an engine for (hasScriptEngine()): OpenRocket runs
///   such a script, and it defines no function for the simulation to call, so the flight is
///   that of the simulation without the extension. For every other enabled script initialize()
///   throws a SimulationException, so Simulation::simulate() fails with
///   ErrorCode::SIMULATION_ABORTED instead of running the flight without the script. The text
///   is that of OpenRocket for a language it has no engine for, with the program's name:
///   "QtRocket does not support the scripting language '<language>'" (Java: "Your JRE does not
///   support the scripting language '<language>'", which it also has for a script that holds
///   nothing in such a language).
/// A disabled script does nothing in either. So a design that stores that roll control script
/// enabled loads with the script enabled, as in OpenRocket, and its simulation does not run
/// until the script is disabled.
///
/// Deviations from OpenRocket:
/// - No script is ever run, and of ScriptingUtil and ScriptingSimulationListener only what
///   decides whether a script is trusted is ported, as static functions of this class
///   (normalizeScript(), scriptHash(), isTrustedScript(), hasScriptEngine()): not the engines,
///   the trusted hashes kept in the preferences, nor the listener that calls the script's
///   functions. The places where they would be used are marked "HOOK(scripting)" in the source
///   file.
/// - An enabled script that holds nothing adds no listener in initialize() (Java: the listener
///   of a script without functions, which never acts).
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

    /// What documentLoaded() does: an enabled script that is not trusted (isTrustedScript() of
    /// its language and text) is disabled, and kDisabledWarning is added to @p warnings (a
    /// warning set holds the text once, however many scripts add it). Nothing happens for a
    /// disabled script and for a trusted one.
    void disableUntrustedScript(WarningSet& warnings);

    /// Nothing for a disabled script, and for an enabled one that holds nothing but blanks,
    /// tabs and line ends in a language OpenRocket has an engine for.
    /// @throws SimulationException for every other enabled script (see the class comment)
    void initialize(SimulationConditions& conditions) override;

    /// ScriptingUtil.normalize(): @p script without its carriage returns (U+000D), then without
    /// leading and trailing characters at or below U+0020 (String.trim(): a no-break space
    /// U+00A0 stays).
    [[nodiscard]] static std::string normalizeScript(std::string_view script);

    /// ScriptingUtil.hash(): "SHA-256:" and the 64 lower-case hexadecimal digits of the
    /// SHA-256 digest of the bytes of @p language, a '|' and the bytes of @p script (UTF-8),
    /// the key under which OpenRocket remembers a trusted script. OpenRocket hashes the
    /// normalised script; the language is hashed as it is written.
    [[nodiscard]] static std::string scriptHash(std::string_view language, std::string_view script);

    /// ScriptingUtil.isTrustedScript() of a computer on which the user has marked no script as
    /// trusted: whether normalizeScript(@p script) is empty, or its scriptHash() with
    /// @p language is one of DEFAULT_TRUSTED_HASHES, which holds one script, the roll control
    /// script of OpenRocket's example design of the file format 1.7 under the language
    /// "JavaScript".
    [[nodiscard]] static bool isTrustedScript(std::string_view language, std::string_view script);

    /// Whether OpenRocket has a script engine for @p language (ScriptingUtil.getEngineByName()
    /// is not null): one of the names of its JavaScript engine, "js", "JavaScript",
    /// "ECMAScript", "Graal.js", "Graal-js", "GraalJS" and "GraalJSPolyglot", compared as
    /// String.equalsIgnoreCase() compares. No other language has one ("Python", "nashorn", "",
    /// " JavaScript").
    [[nodiscard]] static bool hasScriptEngine(std::string_view language) noexcept;

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
