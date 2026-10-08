#pragma once

// What the tests of the .ork loader's setters share: a component in a document as a load makes
// it (SetterFixture), a recorder of what a setter class does with a text (SetterRecorder), one
// case of a table of setter applications (SetterCase), and the helpers that print what a
// component holds as OpenRocket's probe prints it. Test-only.
//
// The expectations of the tables come from the Java probe SetterProbe of tier 9b, part R1
// (info.openrocket.core.file.openrocket.importt.SetterProbe; its cases are made by
// scripts/make_cases.py and its answers turned into table rows by scripts/make_tests.py, all
// under the scratchpad's probes/tier9b-registry). The probe applies an entry of OpenRocket's
// DocumentConfig.setters to a component and prints the warnings and what the component's getters
// give. SetterFixture::make() builds the same tree as the probe does.
//
// The setters of part R2 (positions, fin tab, cluster, material, preset) have a second form of
// case, SetterScript: several elements applied in order to one component, as a file has them,
// with what the component and the document hold afterwards. Their expectations come from the
// probe SpecialSetterProbe of part R2 (probes/tier9b-special-setters: scripts/make_r2_cases.py
// makes the cases, scripts/make_r2_tests.py the readers and rows from OpenRocket's answers).

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "rocket/preset/ExamplePresets.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{

/// A number as Java prints a double (Double.toString()).
[[nodiscard]] inline std::string num(double value)
{
    return Strings::javaDoubleToString(value);
}

/// "true" or "false".
[[nodiscard]] inline std::string flag(bool value)
{
    return std::format("{}", value);
}

/// A whole number.
[[nodiscard]] inline std::string count(int value)
{
    return std::to_string(value);
}

/// A text as it is: a name, a comment, the name of an enum constant.
[[nodiscard]] inline std::string asText(std::string_view value)
{
    return std::string(value);
}

/// The texts, each followed by ", " but the last.
template <class... Texts>
[[nodiscard]] std::string list(const Texts&... texts)
{
    std::string joined;
    const auto  add = [&joined](std::string_view text) {
        if (!joined.empty())
        {
            joined += ", ";
        }
        joined += text;
    };
    (add(texts), ...);
    return joined;
}

/// The texts, each followed by ", " but the last, as list() joins them, for a reader whose
/// getters must be called in the order they are written: the elements of a braced list are
/// computed in that order, the arguments of a function such as list() in any (GCC computes them
/// from the last to the first). It matters where a getter changes what a later one reads, as
/// OpenRocket's and QtRocket's getRadius() of a mass object with an automatic radius sets its
/// length; the Java probe calls the getters in the order of the list.
[[nodiscard]] inline std::string listInOrder(std::initializer_list<std::string> texts)
{
    std::string joined;
    for (const std::string& text : texts)
    {
        if (!joined.empty())
        {
            joined += ", ";
        }
        joined += text;
    }
    return joined;
}

/// @p component as a C; the test is wrong when it is none.
template <class C>
[[nodiscard]] const C& as(const RocketComponent& component)
{
    const auto* const typed = dynamic_cast<const C*>(&component);
    if (typed == nullptr)
    {
        bug(std::format("the test reads a {} as another class", className(component.kind())));
    }
    return *typed;
}

/// The colour of @p component as "red, green, blue, alpha", or "null, null, null, null" when it
/// has none (the probe's getColor.getRed and so on).
[[nodiscard]] inline std::string colorOf(const RocketComponent& component)
{
    const std::optional<Color>& color = component.getColor();
    if (!color.has_value())
    {
        return "null, null, null, null";
    }
    return list(count(color->red()), count(color->green()), count(color->blue()),
                count(color->alpha()));
}

/// The name of the line style of @p component ("DASHDOT"), or "null" when it has none.
[[nodiscard]] inline std::string styleOf(const RocketComponent& component)
{
    const std::optional<LineStyle> style = component.getLineStyle();
    return style.has_value() ? std::string(lineStyleName(*style)) : "null";
}

/// The attributes "name=value|name=value" of a table row as an element's attributes. A value
/// may hold anything but '|'; a name anything but '=' and '|'.
[[nodiscard]] inline Setter::Attributes attributesOf(std::string_view text)
{
    Setter::Attributes attributes;
    while (!text.empty())
    {
        const std::size_t      bar   = text.find('|');
        const std::string_view pair  = text.substr(0, bar);
        const std::size_t      equal = pair.find('=');
        if (equal == std::string_view::npos)
        {
            bug(std::format("the attribute '{}' of a test table has no '='", pair));
        }
        attributes.insert_or_assign(std::string(pair.substr(0, equal)),
                                    std::string(pair.substr(equal + 1)));
        text = bar == std::string_view::npos ? std::string_view{} : text.substr(bar + 1);
    }
    return attributes;
}

/// A document as a load starts it (HandlerFixture: the rocket's events are on) with the tree
/// the Java probe makes: the rocket, one stage and one body tube in it.
class SetterFixture
{
public:
    SetterFixture()
      : m_stage(&m_fixture.rocket().addChild(std::make_unique<AxialStage>())),
        m_tube(&m_stage->addChild(std::make_unique<BodyTube>()))
    {
    }

    [[nodiscard]] HandlerFixture&         fixture() noexcept { return m_fixture; }
    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_fixture.context(); }
    [[nodiscard]] Rocket&                 rocket() { return m_fixture.rocket(); }
    [[nodiscard]] AxialStage&             stage() noexcept { return *m_stage; }
    [[nodiscard]] BodyTube&               tube() noexcept { return *m_tube; }

    /// The component of @p kind a setter is applied to, as the probe makes it: the rocket, the
    /// stage or the body tube themselves; a new nose cone or transition in the stage, behind
    /// the tube; a new component of any other kind in the tube. With @p inFront the new
    /// component becomes the first child of its parent (a nose cone before the tube), else the
    /// last. Call it once per fixture.
    [[nodiscard]] RocketComponent& make(ComponentKind kind, bool inFront = false)
    {
        switch (kind)
        {
            case ComponentKind::ROCKET:
                return rocket();
            case ComponentKind::AXIAL_STAGE:
                return *m_stage;
            case ComponentKind::BODY_TUBE:
                return *m_tube;
            default:
                break;
        }
        std::unique_ptr<RocketComponent> component = DocumentConfig::createComponent(xmlName(kind));
        if (component == nullptr)
        {
            bug(std::format("no component for the kind {}", componentKindName(kind)));
        }
        const bool inStage = kind == ComponentKind::NOSE_CONE || kind == ComponentKind::TRANSITION;
        RocketComponent& parent = inStage ? static_cast<RocketComponent&>(*m_stage) : *m_tube;
        if (!parent.isCompatible(*component))
        {
            bug(std::format("a {} does not fit the test's tree", componentKindName(kind)));
        }
        if (inFront)
        {
            return parent.addChild(std::move(component), std::size_t{0});
        }
        return parent.addChild(std::move(component));
    }

private:
    HandlerFixture m_fixture;
    AxialStage*    m_stage;
    BodyTube*      m_tube;
};

/// A SetterFixture with what the setters of materials and presets ask the application for, as
/// the Java probe has it: OpenRocket's built-in materials are the application's, and the
/// preset database holds the six presets of the example designs (the probe's holds all of
/// OpenRocket's; the six are the ones the cases name). The default units are OpenRocket's for
/// as long as the fixture lives: a parachute or streamer takes a preset's material by the
/// length of its text, which has the density in the default unit.
class ApplicationSetterFixture
{
public:
    ApplicationSetterFixture() : m_presets(makeExamplePresetDatabase())
    {
        addBuiltinMaterials(m_setters.fixture().materials());
        m_setters.context().setComponentPresetDatabase(&m_presets);
    }
    ~ApplicationSetterFixture() = default;

    // The context points at the members.
    ApplicationSetterFixture(const ApplicationSetterFixture&)            = delete;
    ApplicationSetterFixture& operator=(const ApplicationSetterFixture&) = delete;
    ApplicationSetterFixture(ApplicationSetterFixture&&)                 = delete;
    ApplicationSetterFixture& operator=(ApplicationSetterFixture&&)      = delete;

    [[nodiscard]] SetterFixture&          setters() noexcept { return m_setters; }
    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_setters.context(); }
    [[nodiscard]] OpenRocketDocument& document() noexcept { return m_setters.fixture().document(); }
    [[nodiscard]] Rocket&             rocket() { return m_setters.rocket(); }
    [[nodiscard]] ComponentPresetDatabase& presets() noexcept { return m_presets; }
    /// SetterFixture::make().
    [[nodiscard]] RocketComponent& make(ComponentKind kind, bool inFront = false)
    {
        return m_setters.make(kind, inFront);
    }

private:
    DefaultUnitsGuard       m_units;
    ComponentPresetDatabase m_presets;
    SetterFixture           m_setters;
};

/// Records what a setter class does with a text, for the tests of the class itself: the
/// functions it hands out stand for a component's setter methods and note each call, and apply()
/// tells the calls, in order, and the warnings.
///
///     SetterRecorder     recorder;
///     const DoubleSetter setter(recorder.number("value"), "auto", ' ', recorder.truth("flag"));
///     EXPECT_EQ(recorder.apply(setter, "auto 0.0125"), "value 0.0125, flag true");
///
/// The setter is applied to the body tube of a SetterFixture, named "Body Tube". The recorder
/// must outlive the setters made with its functions.
class SetterRecorder
{
public:
    SetterRecorder()                                 = default;
    ~SetterRecorder()                                = default;
    SetterRecorder(const SetterRecorder&)            = delete;
    SetterRecorder& operator=(const SetterRecorder&) = delete;
    SetterRecorder(SetterRecorder&&)                 = delete;
    SetterRecorder& operator=(SetterRecorder&&)      = delete;

    [[nodiscard]] SetterFixture& fixture() noexcept { return m_fixture; }
    /// The component apply() hands to a setter.
    [[nodiscard]] RocketComponent& component() noexcept { return m_fixture.tube(); }

    /// Notes the call @p call of a setter's function on @p component; a call on another
    /// component than component() is noted as that.
    void note(const RocketComponent& component, std::string call)
    {
        if (&component != &this->component())
        {
            call += " on another component";
        }
        m_calls.push_back(std::move(call));
    }

    /// A function that notes "<name> <number as Java prints a double>".
    [[nodiscard]] std::function<void(RocketComponent&, double)> number(std::string name)
    {
        return [this, name = std::move(name)](RocketComponent& component, double value) {
            note(component, name + " " + num(value));
        };
    }
    /// A function that notes "<name> true" or "<name> false".
    [[nodiscard]] std::function<void(RocketComponent&, bool)> truth(std::string name)
    {
        return [this, name = std::move(name)](RocketComponent& component, bool value) {
            note(component, name + " " + flag(value));
        };
    }
    /// A function that notes "<name> <number>".
    [[nodiscard]] std::function<void(RocketComponent&, int)> whole(std::string name)
    {
        return [this, name = std::move(name)](RocketComponent& component, int value) {
            note(component, name + " " + count(value));
        };
    }

    /// What @p setter does with the element text @p text and the attributes @p attributes
    /// (attributesOf()): the calls it makes, in order and separated by ", ", then each warning
    /// in brackets, then "FAILED <code>: <message>" when it fails. For example
    /// "value 0.0125, flag true", "flag true [Invalid parameter encountered, ignoring.]" and
    /// "[Invalid parameter encountered, ignoring.]".
    [[nodiscard]] std::string apply(const Setter& setter, std::string_view text,
                                    std::string_view attributes = {})
    {
        m_calls.clear();
        WarningSet         warnings;
        const Result<void> result =
            setter.set(component(), text, attributesOf(attributes), warnings, m_fixture.context());
        std::string told;
        for (const std::string& call : m_calls)
        {
            told += (told.empty() ? "" : ", ") + call;
        }
        for (const std::string& warning : warningTexts(warnings))
        {
            told += (told.empty() ? "[" : " [") + warning + "]";
        }
        if (!result.has_value())
        {
            told += std::format("{}FAILED {}: {}", told.empty() ? "" : " ",
                                toString(result.error().code), result.error().message);
        }
        return told;
    }

private:
    SetterFixture            m_fixture;
    std::vector<std::string> m_calls;
};

/// What a member of SetterCase holds when a row of a table leaves it out: nothing. The members
/// name it because a member a row may leave out needs an initializer (the compilers warn of a
/// designated initializer list that skips a member without one), and clang-tidy takes `{}` and
/// "" there for redundant.
inline constexpr std::string_view kNoText;

/// One application of an entry of the setter table, and what it has to give.
struct SetterCase
{
    /// The key of the entry, "Class:element": the walk for @p kind must end at this class.
    std::string_view key;
    /// The component the setter is applied to (SetterFixture::make()).
    ComponentKind kind;
    /// Whether the component is made the first child of its parent (SetterFixture::make()).
    bool inFront{false};
    /// The attributes of the element (attributesOf()).
    std::string_view attributes = kNoText;
    /// The text of the element.
    std::string_view text;
    /// What the component holds afterwards, as the probe prints its getters.
    std::string (*read)(const RocketComponent&);
    /// What @p read has to give. Not compared when a failure is expected.
    std::string_view expected;
    /// The warnings the setters have to add, each followed by " | " but the last.
    std::string_view warnings = kNoText;
    /// The failure the setter has to return, as "CODE: message"; empty when it must succeed.
    std::string_view failure = kNoText;
    /// A setter that is applied first, to the same component: its key, attributes and text.
    std::string_view beforeKey        = kNoText;
    std::string_view beforeAttributes = kNoText;
    std::string_view beforeText       = kNoText;
};

/// What applying a case gave.
struct SetterOutcome
{
    std::string values;
    std::string warnings;
    std::string failure;
    /// What went wrong before the setter could be applied; empty when nothing did.
    std::string problem;
};

/// Applies the entry @p key of the setter table to @p component as the handler of a component's
/// parameters does: the walk for the component's kind, which must end at the class of @p key
/// with a setter. Returns the failure of the setter as "CODE: message", "" when it succeeded,
/// and a text that starts with "PROBLEM" when the walk did not find that entry.
[[nodiscard]] inline std::string applyEntry(const DocumentLoadingContext& context,
                                            RocketComponent& component, std::string_view key,
                                            std::string_view attributes, std::string_view text,
                                            WarningSet& warnings)
{
    const std::size_t colon = key.find(':');
    if (colon == std::string_view::npos)
    {
        return std::format("PROBLEM: the key '{}' has no ':'", key);
    }
    const DocumentConfig::SetterLookup lookup =
        DocumentConfig::findSetter(component.kind(), key.substr(colon + 1));
    if (lookup.setter == nullptr || lookup.owner != key.substr(0, colon))
    {
        return std::format("PROBLEM: the walk for a {} ends at '{}'{}, not at the setter of '{}'",
                           className(component.kind()), lookup.owner,
                           lookup.setter == nullptr ? " without a setter" : "", key);
    }
    const Result<void> result =
        lookup.setter->set(component, text, attributesOf(attributes), warnings, context);
    if (!result.has_value())
    {
        return std::format("{}: {}", toString(result.error().code), result.error().message);
    }
    return {};
}

/// Applies @p row in a fixture of its own, an ApplicationSetterFixture: the application has
/// OpenRocket's materials and the presets of the examples.
[[nodiscard]] inline SetterOutcome runSetterCase(const SetterCase& row)
{
    ApplicationSetterFixture fixture;
    RocketComponent&         component = fixture.make(row.kind, row.inFront);
    WarningSet               warnings;
    SetterOutcome            outcome;
    if (!row.beforeKey.empty())
    {
        outcome.problem = applyEntry(fixture.context(), component, row.beforeKey,
                                     row.beforeAttributes, row.beforeText, warnings);
        if (!outcome.problem.empty())
        {
            outcome.problem = "the setter before: " + outcome.problem;
            return outcome;
        }
    }
    std::string failure =
        applyEntry(fixture.context(), component, row.key, row.attributes, row.text, warnings);
    if (failure.starts_with("PROBLEM"))
    {
        outcome.problem = std::move(failure);
        return outcome;
    }
    outcome.failure = std::move(failure);
    for (const std::string& warning : warningTexts(warnings))
    {
        if (!outcome.warnings.empty())
        {
            outcome.warnings += " | ";
        }
        outcome.warnings += warning;
    }
    outcome.values = row.read(component);
    return outcome;
}

/// What is wrong with @p row, or "" when it gives what it has to.
[[nodiscard]] inline std::string checkSetterCase(const SetterCase& row)
{
    const SetterOutcome outcome = runSetterCase(row);
    std::string         wrong;
    if (!outcome.problem.empty())
    {
        wrong = outcome.problem;
    }
    else if (outcome.failure != row.failure)
    {
        wrong = std::format("failure [{}], expected [{}]", outcome.failure, row.failure);
    }
    else if (outcome.warnings != row.warnings)
    {
        wrong = std::format("warnings [{}], expected [{}]", outcome.warnings, row.warnings);
    }
    else if (row.failure.empty() && outcome.values != row.expected)
    {
        wrong = std::format("values [{}], expected [{}]", outcome.values, row.expected);
    }
    if (wrong.empty())
    {
        return wrong;
    }
    return std::format("{} on a {} with [{}] and the text [{}]: {}", row.key, className(row.kind),
                       row.attributes, row.text, wrong);
}

/// What is wrong with each of @p rows; empty when every row gives what it has to.
[[nodiscard]] inline std::vector<std::string> checkSetterCases(std::span<const SetterCase> rows)
{
    std::vector<std::string> wrong;
    for (const SetterCase& row : rows)
    {
        if (std::string problem = checkSetterCase(row); !problem.empty())
        {
            wrong.push_back(std::move(problem));
        }
    }
    return wrong;
}

// ---------------------------------------------------------------- scripts of several elements

/// A material as the probe prints it: its storable string ("BULK|Balsa|170.0|2.3E8|Woods"),
/// whether it is user-defined and whether it is a material of the document.
[[nodiscard]] inline std::string describeMaterial(const Material& material)
{
    return list(material.toStorableString(), flag(material.isUserDefined()),
                flag(material.isDocumentMaterial()));
}

/// The materials of @p document (the probe's "@docmaterials": DocumentPreferences.
/// getAllMaterials() in its order, which is the order a save writes <docmaterials> in), each as
/// its storable string and followed by "; " but the last.
[[nodiscard]] inline std::string documentMaterialsOf(OpenRocketDocument& document)
{
    std::string all;
    for (const Material& material : document.getDocumentMaterials().allMaterials())
    {
        if (!all.empty())
        {
            all += "; ";
        }
        all += material.toStorableString();
    }
    return all;
}

/// The part number of the preset of @p component, or "null" when it has none (the probe's
/// getPresetComponent.getPartNo).
[[nodiscard]] inline std::string presetPartNoOf(const RocketComponent& component)
{
    const ComponentPreset* const preset = component.getPresetComponent();
    return preset == nullptr ? "null" : preset->getPartNo();
}

/// The digest of the preset of @p component, or "null" when it has none.
[[nodiscard]] inline std::string presetDigestOf(const RocketComponent& component)
{
    const ComponentPreset* const preset = component.getPresetComponent();
    return preset == nullptr ? "null" : preset->getDigest();
}

/// The setter of the entry @p key of the setter table, "Class:element", whatever kind's walk
/// ends there with it; null when the table has no such setter. The Java probe takes its setters
/// the same way, by key, and so can apply the entry of a superclass that the walk for the
/// component would not reach ("RocketComponent:preset" on a parachute).
[[nodiscard]] inline const Setter* setterOfKey(std::string_view key)
{
    const std::size_t colon = key.find(':');
    if (colon == std::string_view::npos)
    {
        return nullptr;
    }
    for (const ComponentKind kind : kAllComponentKinds)
    {
        const DocumentConfig::SetterLookup lookup =
            DocumentConfig::findSetter(kind, key.substr(colon + 1));
        if (lookup.setter != nullptr && lookup.owner == key.substr(0, colon))
        {
            return lookup.setter;
        }
    }
    return nullptr;
}

/// The warnings of @p warnings, each followed by " | " but the last.
[[nodiscard]] inline std::string joinedWarnings(const WarningSet& warnings)
{
    std::string joined;
    for (const std::string& warning : warningTexts(warnings))
    {
        if (!joined.empty())
        {
            joined += " | ";
        }
        joined += warning;
    }
    return joined;
}

/// One element of a script: the key of its entry of the setter table, its attributes
/// (attributesOf()) and its text.
struct SetterStep
{
    std::string_view key        = kNoText;
    std::string_view attributes = kNoText;
    std::string_view text       = kNoText;
};

/// The most elements a script has.
inline constexpr std::size_t kMaxSetterSteps = 8;

/// Several elements applied in order to one component of an ApplicationSetterFixture, and what
/// the component and the document have to hold afterwards.
struct SetterScript
{
    /// The name of the case in the probe's files, for the message of a failure.
    std::string_view id;
    /// The component the setters are applied to (SetterFixture::make()).
    ComponentKind kind;
    /// Whether the component is made the first child of its parent.
    bool inFront{false};
    /// Whether the component ignores the clearing of its preset meanwhile, as it does while the
    /// handler of a component's parameters works (RocketComponent::setIgnorePresetClearing()).
    bool bracket{false};
    /// The elements, in order; the ones behind the last have no key.
    std::array<SetterStep, kMaxSetterSteps> steps;
    /// What the component and the document hold afterwards, as the probe prints its getters.
    std::string (*read)(ApplicationSetterFixture&, const RocketComponent&);
    /// What @p read has to give.
    std::string_view expected;
    /// The warnings the setters have to add, each followed by " | " but the last.
    std::string_view warnings = kNoText;
};

/// What applying @p script gave: the values and the warnings, or a problem.
[[nodiscard]] inline SetterOutcome runSetterScript(const SetterScript& script)
{
    ApplicationSetterFixture fixture;
    RocketComponent&         component = fixture.make(script.kind, script.inFront);
    if (script.bracket)
    {
        component.setIgnorePresetClearing(true);
    }
    WarningSet    warnings;
    SetterOutcome outcome;
    for (const SetterStep& step : script.steps)
    {
        if (step.key.empty())
        {
            break;
        }
        const Setter* const setter = setterOfKey(step.key);
        if (setter == nullptr)
        {
            outcome.problem = std::format("the setter table has no setter '{}'", step.key);
            return outcome;
        }
        const Result<void> result = setter->set(component, step.text, attributesOf(step.attributes),
                                                warnings, fixture.context());
        if (!result.has_value())
        {
            outcome.problem =
                std::format("the setter '{}' failed: {}", step.key, result.error().message);
            return outcome;
        }
    }
    outcome.warnings = joinedWarnings(warnings);
    outcome.values   = script.read(fixture, component);
    return outcome;
}

/// @p told followed by each warning of @p warnings in brackets: "TOP, 0.1, 0.1 [a warning]".
[[nodiscard]] inline std::string withWarnings(std::string told, const WarningSet& warnings)
{
    for (const std::string& warning : warningTexts(warnings))
    {
        told += " [" + warning + "]";
    }
    return told;
}

/// What @p setter, taken by hand, does with one element for a new component of @p kind in a
/// fixture of its own: what @p read gives afterwards, then each warning in brackets, then
/// "FAILED <code>: <message>" when the setter fails. For the tests of a setter class that do not
/// go through the setter table.
[[nodiscard]] inline std::string applyByHand(const Setter& setter, ComponentKind kind,
                                             std::string_view attributes, std::string_view text,
                                             std::string (*read)(ApplicationSetterFixture&,
                                                                 const RocketComponent&))
{
    ApplicationSetterFixture fixture;
    RocketComponent&         component = fixture.make(kind);
    WarningSet               warnings;
    const Result<void>       result =
        setter.set(component, text, attributesOf(attributes), warnings, fixture.context());
    std::string told = withWarnings(read(fixture, component), warnings);
    if (!result.has_value())
    {
        told +=
            std::format(" FAILED {}: {}", toString(result.error().code), result.error().message);
    }
    return told;
}

/// What is wrong with @p script, or "" when it gives what it has to.
[[nodiscard]] inline std::string checkSetterScript(const SetterScript& script)
{
    const SetterOutcome outcome = runSetterScript(script);
    std::string         wrong;
    if (!outcome.problem.empty())
    {
        wrong = outcome.problem;
    }
    else if (outcome.warnings != script.warnings)
    {
        wrong = std::format("warnings [{}], expected [{}]", outcome.warnings, script.warnings);
    }
    else if (outcome.values != script.expected)
    {
        wrong = std::format("values [{}], expected [{}]", outcome.values, script.expected);
    }
    if (wrong.empty())
    {
        return wrong;
    }
    return std::format("case {} on a {}: {}", script.id, className(script.kind), wrong);
}

/// What is wrong with each of @p scripts; empty when every one gives what it has to.
[[nodiscard]] inline std::vector<std::string> checkSetterScripts(
    std::span<const SetterScript> scripts)
{
    std::vector<std::string> wrong;
    for (const SetterScript& script : scripts)
    {
        if (std::string problem = checkSetterScript(script); !problem.empty())
        {
            wrong.push_back(std::move(problem));
        }
    }
    return wrong;
}

}  // namespace QtRocket::Test
