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

#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/HandlerTestSupport.h"

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
[[nodiscard]] inline std::string applyEntry(SetterFixture& fixture, RocketComponent& component,
                                            std::string_view key, std::string_view attributes,
                                            std::string_view text, WarningSet& warnings)
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
        lookup.setter->set(component, text, attributesOf(attributes), warnings, fixture.context());
    if (!result.has_value())
    {
        return std::format("{}: {}", toString(result.error().code), result.error().message);
    }
    return {};
}

/// Applies @p row in a fixture of its own.
[[nodiscard]] inline SetterOutcome runSetterCase(const SetterCase& row)
{
    SetterFixture    fixture;
    RocketComponent& component = fixture.make(row.kind, row.inFront);
    WarningSet       warnings;
    SetterOutcome    outcome;
    if (!row.beforeKey.empty())
    {
        outcome.problem = applyEntry(fixture, component, row.beforeKey, row.beforeAttributes,
                                     row.beforeText, warnings);
        if (!outcome.problem.empty())
        {
            outcome.problem = "the setter before: " + outcome.problem;
            return outcome;
        }
    }
    std::string failure =
        applyEntry(fixture, component, row.key, row.attributes, row.text, warnings);
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

}  // namespace QtRocket::Test
