#pragma once

// What the tests of the .ork loader's component handlers share: the content of a <rocket>
// element is read by a ComponentParameterHandler for the rocket of a document as a load starts
// it, through the test root of HandlerTestSupport.h, and what came of it is printed as the Java
// probe of tier 9b, part R3 prints it for OpenRocket (HandlerProbe.java under the scratchpad's
// probes/tier9b-component-handlers; its outputs are the expectations of the case tables).
// Test-only.
//
// What a case gives (runRocketCase()), one line each:
//
//     RESULT ok                                   or  RESULT FAILED <code>: <message>
//     W <warning>                                 every warning, in order
//     ROOT rocket {name=value, ...} [<text>]      what the element's parent is told at its end
//     EVENTS <n> {<kinds>=<n>, ...}               the rocket's change events, by kind
//     | <component> ...                           the rocket, read through getters
//     | selected=...                              the selected flight configuration
//     | config ...                                every flight configuration, the default first
//
// A component's line is its Java class, its name and "key=value" for what it holds; see
// describeComponent() in the source for the keys. An id that the case's text does not spell out
// is random and printed as "random".

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/MotorFinder.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{

/// The motor finder of the probe: a <motor> that names a designation gets the motor of
/// makeEmbeddedTestMotor("F12X", 12.0, "d"), whatever the designation, and one that names none
/// gets no motor. A designation that starts with "W" also adds the warning "finder:
/// <designation>", as a database search adds its warnings.
class ByDesignationMotorFinder final : public MotorFinder
{
public:
    ByDesignationMotorFinder();

    [[nodiscard]] std::shared_ptr<const Motor> findMotor(
        std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
        std::optional<std::string_view> designation, double diameter, double length,
        std::optional<std::string_view> digest, WarningSet& warnings) const override;

private:
    std::shared_ptr<const Motor> m_motor;
};

/// The UUIDs @p xml spells out: every run of hexadecimal digits and dashes that
/// java.util.UUID.fromString() reads ("1-2-3-4-5" too).
[[nodiscard]] std::set<Uuid> knownIds(std::string_view xml);

/// The state of @p rocket, one line per component in tree order (two blanks deeper per level),
/// then the selected configuration and every flight configuration. Ids outside @p known are
/// "random".
[[nodiscard]] std::vector<std::string> describeRocket(const Rocket&         rocket,
                                                      const Preferences&    preferences,
                                                      const std::set<Uuid>& known);

/// The environment of a load, as the probe has it: a HandlerFixture whose application
/// materials are OpenRocket's built-in ones and whose motors come from a
/// ByDesignationMotorFinder; with @p withPresets the preset database holds the six presets of
/// the example designs (the probe's holds all of OpenRocket's), without it there is none. The
/// default units are OpenRocket's while the fixture lives.
class RocketLoadFixture
{
public:
    explicit RocketLoadFixture(bool withPresets = false);
    ~RocketLoadFixture() = default;

    // The context points at the members.
    RocketLoadFixture(const RocketLoadFixture&)            = delete;
    RocketLoadFixture& operator=(const RocketLoadFixture&) = delete;
    RocketLoadFixture(RocketLoadFixture&&)                 = delete;
    RocketLoadFixture& operator=(RocketLoadFixture&&)      = delete;

    [[nodiscard]] HandlerFixture&         fixture() noexcept { return m_fixture; }
    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_fixture.context(); }
    [[nodiscard]] OpenRocketDocument&     document() noexcept { return m_fixture.document(); }
    [[nodiscard]] Rocket&                 rocket() { return m_fixture.rocket(); }

    /// Reads @p xml as the content of the rocket element: the children of <rocket>, or the
    /// whole element when the text starts with "<rocket". A ComponentParameterHandler for the
    /// fixture's rocket reads it, as OpenRocketContentHandler has it read.
    [[nodiscard]] HandlerRun load(std::string_view xml);

    /// load(), and what came of it in the probe's notation (see the top of this file).
    [[nodiscard]] std::string loadAndDescribe(std::string_view xml);

    /// loadAndDescribe() with two lines in the place of the state's: "COMPONENTS <n>", the
    /// number of components, and "STATE <digest>", the SHA-256 of the state's lines, each
    /// followed by a line feed. With @p state the lines themselves are stored there.
    [[nodiscard]] std::string loadAndSummarize(std::string_view          xml,
                                               std::vector<std::string>* state = nullptr);

private:
    /// load(), and what came of it but the state, whose lines go to @p state.
    [[nodiscard]] std::string describeLoad(std::string_view xml, std::vector<std::string>& state);

    DefaultUnitsGuard        m_units;
    ComponentPresetDatabase  m_presets;
    ByDesignationMotorFinder m_motorFinder;
    HandlerFixture           m_fixture;
};

/// RocketLoadFixture(withPresets).loadAndDescribe(xml) in a fixture of its own.
[[nodiscard]] std::string runRocketCase(std::string_view xml, bool withPresets = false);

/// One case of a table: the content of a rocket element and what reading it gives.
struct RocketCase
{
    /// The name of the case in the probe's case file.
    std::string_view name;
    /// The content of the rocket element (RocketLoadFixture::load()).
    std::string_view xml;
    /// What runRocketCase() has to give.
    std::string_view expected;
};

/// What reading each case of @p cases gives that is not what the case expects: per case its
/// name, its text, and the lines expected and found. Empty when every case agrees.
[[nodiscard]] std::vector<std::string> failedRocketCases(std::span<const RocketCase> cases,
                                                         bool withPresets = false);

/// The cases of @p cases as the probe's case file has them, each followed by what QtRocket
/// makes of it: the text a DISABLED_ test prints for the script that makes the tables.
[[nodiscard]] std::string printedRocketCases(std::span<const RocketCase> cases,
                                             bool                        withPresets = false);

/// What the top-level loader does to a rocket when a file has been read, and a first look at
/// the result: the preloaded stage activeness of every flight configuration is applied, the
/// default one included (OpenRocketLoader), the document's undo history is cleared, which
/// copies the rocket, the rocket is updated (GeneralRocketLoader's enableEvents()), and its
/// active components, its length and everything describeRocket() reads are asked for.
/// Returns what that throws, or "" when nothing does.
[[nodiscard]] std::string whatUsingTheRocketThrows(RocketLoadFixture& fixture);

/// What reading @p xml (RocketLoadFixture::load()) and then using the rocket
/// (whatUsingTheRocketThrows()) throws, or "": the failure policy of the loader is that
/// nothing a file can hold makes it throw. A load that fails is as good as one that succeeds.
[[nodiscard]] std::string whatReadingThrows(std::string_view xml, bool withPresets = false);

/// The cases of @p cases that make the loader or the use of the rocket throw when their text
/// is cut off behind any of its '>' (a document that ends too early is one a file can hold:
/// the handlers have then read part of it), each with the text and what was thrown. Empty when
/// none does.
[[nodiscard]] std::vector<std::string> casesThatThrowWhenCutOff(std::span<const RocketCase> cases,
                                                                bool withPresets = false);

/// The rocket element of the design file @p path, without its appearance elements: the file
/// may be the XML document itself, a gzip of it or a zip archive, whose first entry with a name
/// that ends in ".ork" is the document. (The appearance elements are taken out because their
/// handlers are part R4's: HOOK(R4).)
/// @throws BugError when the file cannot be read or holds no rocket element
[[nodiscard]] std::string rocketElementOfDesignFile(const std::filesystem::path& path);

/// A design file and what reading its rocket element gives.
struct DesignFileCase
{
    /// The name of the file in its directory.
    std::string_view file;
    /// What RocketLoadFixture::loadAndSummarize() has to give for rocketElementOfDesignFile().
    std::string_view expected;
};

/// What reading each file of @p cases in @p directory gives that is not what the case expects;
/// empty when every file agrees.
[[nodiscard]] std::vector<std::string> failedDesignFiles(const std::filesystem::path&    directory,
                                                         std::span<const DesignFileCase> cases,
                                                         bool withPresets);

/// The files of @p cases, each followed by what QtRocket makes of its rocket element, as the
/// probe prints it; with @p withState the lines of the state follow each file, as the probe
/// writes them to its dump files.
[[nodiscard]] std::string printedDesignFiles(const std::filesystem::path&    directory,
                                             std::span<const DesignFileCase> cases,
                                             bool withPresets, bool withState);

}  // namespace QtRocket::Test
