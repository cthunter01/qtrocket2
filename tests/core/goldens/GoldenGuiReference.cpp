#include "goldens/GoldenGuiReference.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The schemas the tool writes, by version (Scenario.SCHEMA_V1 and SCHEMA_V2, GuiReference).
constexpr std::string_view kScenarioSchemaV1  = "gui-from-scratch-scenario";
constexpr std::string_view kScenarioSchemaV2  = "gui-scenario";
constexpr std::string_view kReferenceSchemaV1 = "gui-from-scratch-reference";
constexpr std::string_view kReferenceSchemaV2 = "gui-reference";

/// The versions a step kind belongs to (bits).
constexpr unsigned kV1   = 1U;
constexpr unsigned kV2   = 2U;
constexpr unsigned kBoth = kV1 | kV2;

/// A step kind, its name, the members it takes besides "step" and "note" (separated by blanks;
/// every one is required but "unit", and New's "answer"), as Scenario.membersOf() of the tool has
/// them, and the versions it belongs to (Scenario.V1_KINDS, V2_KINDS).
struct StepSpec
{
    GuiStepKind      kind{GuiStepKind::NEW};
    std::string_view name;
    std::string_view members;
    unsigned         versions{kBoth};
};

constexpr std::array kStepSpecs{
    StepSpec{.kind = GuiStepKind::NEW, .name = "new", .members = "", .versions = kV1},
    StepSpec{.kind = GuiStepKind::SELECT, .name = "select", .members = "paths"},
    StepSpec{.kind = GuiStepKind::ADD, .name = "add", .members = "component", .versions = kV1},
    StepSpec{.kind = GuiStepKind::EDIT, .name = "edit", .members = ""},
    StepSpec{.kind = GuiStepKind::TYPE, .name = "type", .members = "field text unit"},
    StepSpec{.kind = GuiStepKind::CHOOSE, .name = "choose", .members = "field item"},
    StepSpec{.kind = GuiStepKind::CHECK, .name = "check", .members = "field checked"},
    StepSpec{
        .kind = GuiStepKind::EDIT_POINT, .name = "editPoint", .members = "row column text unit"},
    StepSpec{.kind = GuiStepKind::INSERT_POINT, .name = "insertPoint", .members = "row"},
    StepSpec{.kind = GuiStepKind::BUTTON, .name = "button", .members = "button"},
    StepSpec{.kind = GuiStepKind::OK, .name = "ok", .members = ""},
    StepSpec{.kind = GuiStepKind::UNDO, .name = "undo", .members = ""},
    StepSpec{.kind = GuiStepKind::REDO, .name = "redo", .members = ""},
    StepSpec{.kind = GuiStepKind::COPY, .name = "copy", .members = "", .versions = kV1},
    StepSpec{.kind = GuiStepKind::PASTE, .name = "paste", .members = "", .versions = kV1},
    StepSpec{.kind = GuiStepKind::SAVE_AS, .name = "saveAs", .members = ""},
    StepSpec{.kind = GuiStepKind::CHOOSE_FILE, .name = "chooseFile", .members = "file"},
    StepSpec{.kind = GuiStepKind::OPEN, .name = "open", .members = "file", .versions = kV2},
    StepSpec{.kind = GuiStepKind::TAB, .name = "tab", .members = "tab", .versions = kV2},
    StepSpec{
        .kind = GuiStepKind::CONFIG_TAB, .name = "configTab", .members = "tab", .versions = kV2},
    StepSpec{.kind     = GuiStepKind::SELECT_CELLS,
             .name     = "selectCells",
             .members  = "cells",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::MOUNT_CHECK,
             .name     = "mountCheck",
             .members  = "mount checked",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::NEW_CONFIGURATION,
             .name     = "newConfiguration",
             .members  = "answer",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::RENAME_CONFIGURATION,
             .name     = "renameConfiguration",
             .members  = "answer",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::DELETE_CONFIGURATION,
             .name     = "deleteConfiguration",
             .members  = "",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::DUPLICATE_CONFIGURATION,
             .name     = "duplicateConfiguration",
             .members  = "",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::SELECT_MOTOR,
             .name     = "selectMotor",
             .members  = "answer",
             .versions = kV2},
    StepSpec{
        .kind = GuiStepKind::DELETE_MOTOR, .name = "deleteMotor", .members = "", .versions = kV2},
    StepSpec{.kind     = GuiStepKind::SELECT_IGNITION,
             .name     = "selectIgnition",
             .members  = "answer",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::RESET_IGNITION,
             .name     = "resetIgnition",
             .members  = "",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::SELECT_DEPLOYMENT,
             .name     = "selectDeployment",
             .members  = "answer",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::RESET_DEPLOYMENT,
             .name     = "resetDeployment",
             .members  = "",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::SELECT_SEPARATION,
             .name     = "selectSeparation",
             .members  = "answer",
             .versions = kV2},
    StepSpec{.kind     = GuiStepKind::RESET_SEPARATION,
             .name     = "resetSeparation",
             .members  = "",
             .versions = kV2},
};

/// The question kinds by name.
struct QuestionSpec
{
    GuiQuestionKind  kind{GuiQuestionKind::MOTOR};
    std::string_view name;
};

constexpr std::array kQuestionSpecs{
    QuestionSpec{.kind = GuiQuestionKind::MOTOR, .name = "motor"},
    QuestionSpec{.kind = GuiQuestionKind::IGNITION, .name = "ignition"},
    QuestionSpec{.kind = GuiQuestionKind::DEPLOYMENT, .name = "deployment"},
    QuestionSpec{.kind = GuiQuestionKind::SEPARATION, .name = "separation"},
    QuestionSpec{.kind = GuiQuestionKind::RENAME, .name = "rename"},
    QuestionSpec{.kind = GuiQuestionKind::MESSAGE, .name = "message"},
};

/// The members an answer may have (README.md of the tool, "Version 2").
constexpr std::array<std::string_view, 11> kAnswerMembers{
    "button", "scope", "event", "altitude",          "delay", "name", "saveAsDefault",
    "search", "motor", "curve", "nozzleExitDiameter"};

// The tables are searched through spans: an iterator of an array is a pointer with some standard
// libraries and a class with others.

[[nodiscard]] const StepSpec* stepSpecNamed(std::string_view name) noexcept
{
    const std::span<const StepSpec> specs(kStepSpecs);
    const auto                      found =
        std::ranges::find_if(specs, [name](const StepSpec& spec) { return spec.name == name; });
    return found == specs.end() ? nullptr : &*found;
}

[[nodiscard]] std::optional<GuiQuestionKind> questionKindNamed(std::string_view name) noexcept
{
    const std::span<const QuestionSpec> specs(kQuestionSpecs);
    const auto                          found = std::ranges::find(specs, name, &QuestionSpec::name);
    return found == specs.end() ? std::nullopt : std::optional<GuiQuestionKind>{found->kind};
}

[[nodiscard]] bool contains(std::span<const std::string_view> names, std::string_view name)
{
    return std::ranges::find(names, name) != names.end();
}

/// Reads members of JSON values and keeps the first failure: a read that fails gives a
/// default value, so that a parser reads on and reports what was wrong first.
class JsonReader
{
public:
    [[nodiscard]] bool        ok() const noexcept { return !m_error.has_value(); }
    [[nodiscard]] std::string error() const { return m_error.value_or(std::string()); }

    void failure(std::string_view context, std::string_view what)
    {
        if (!m_error)
        {
            m_error = std::format("{}: {}", context, what);
        }
    }

    [[nodiscard]] const json* member(const json& object, std::string_view key,
                                     std::string_view context)
    {
        if (!object.is_object())
        {
            failure(context, "not an object");
            return nullptr;
        }
        const auto found = object.find(key);
        if (found == object.end())
        {
            failure(context, std::format(R"(missing "{}")", key));
            return nullptr;
        }
        return &*found;
    }

    /// The member @p key of @p object, null when it has none (or it is null).
    [[nodiscard]] static const json* optionalMember(const json& object, std::string_view key)
    {
        // contains() and at() rather than find(): GCC's -Wnull-dereference sees a null iterator
        // in nlohmann's find() once inlined at -O3.
        if (!object.is_object() || !object.contains(key))
        {
            return nullptr;
        }
        const json& value = object.at(key);
        return value.is_null() ? nullptr : &value;
    }

    [[nodiscard]] std::string string(const json& object, std::string_view key,
                                     std::string_view context)
    {
        const json* value = member(object, key, context);
        if (value != nullptr && !value->is_string())
        {
            failure(context, std::format(R"("{}" is not a string)", key));
        }
        return value != nullptr && value->is_string() ? value->get<std::string>() : std::string();
    }

    [[nodiscard]] std::optional<std::string> optionalString(const json&      object,
                                                            std::string_view key,
                                                            std::string_view context)
    {
        if (optionalMember(object, key) == nullptr)
        {
            return std::nullopt;
        }
        return string(object, key, context);
    }

    [[nodiscard]] int integer(const json& object, std::string_view key, std::string_view context)
    {
        const json* value = member(object, key, context);
        if (value != nullptr && !value->is_number_integer())
        {
            failure(context, std::format(R"("{}" is not an integer)", key));
        }
        return value != nullptr && value->is_number_integer() ? value->get<int>() : 0;
    }

    [[nodiscard]] bool boolean(const json& object, std::string_view key, std::string_view context)
    {
        const json* value = member(object, key, context);
        if (value != nullptr && !value->is_boolean())
        {
            failure(context, std::format(R"("{}" is not a boolean)", key));
        }
        return value != nullptr && value->is_boolean() && value->get<bool>();
    }

    [[nodiscard]] const json& array(const json& object, std::string_view key,
                                    std::string_view context)
    {
        static const json kEmpty = json::array();
        const json*       value  = member(object, key, context);
        if (value != nullptr && !value->is_array())
        {
            failure(context, std::format(R"("{}" is not an array)", key));
        }
        return value != nullptr && value->is_array() ? *value : kEmpty;
    }

    [[nodiscard]] const json& object(const json& parent, std::string_view key,
                                     std::string_view context)
    {
        static const json kEmpty = json::object();
        const json*       value  = member(parent, key, context);
        if (value != nullptr && !value->is_object())
        {
            failure(context, std::format(R"("{}" is not an object)", key));
        }
        return value != nullptr && value->is_object() ? *value : kEmpty;
    }

    [[nodiscard]] std::vector<std::string> strings(const json& object, std::string_view key,
                                                   std::string_view context)
    {
        std::vector<std::string> values;
        for (const json& value : array(object, key, context))
        {
            if (!value.is_string())
            {
                failure(context, std::format(R"(an element of "{}" is not a string)", key));
                break;
            }
            values.push_back(value.get<std::string>());
        }
        return values;
    }

    [[nodiscard]] std::vector<int> integers(const json& object, std::string_view key,
                                            std::string_view context)
    {
        std::vector<int> values;
        for (const json& value : array(object, key, context))
        {
            if (!value.is_number_integer())
            {
                failure(context, std::format(R"(an element of "{}" is not an integer)", key));
                break;
            }
            values.push_back(value.get<int>());
        }
        return values;
    }

    /// An object of booleans (the buttons' enabled states), by name.
    [[nodiscard]] std::map<std::string, bool, std::less<>> booleans(const json&      parent,
                                                                    std::string_view key,
                                                                    std::string_view context)
    {
        std::map<std::string, bool, std::less<>> values;
        for (const auto& [name, value] : object(parent, key, context).items())
        {
            if (!value.is_boolean())
            {
                failure(context, std::format(R"(a member of "{}" is not a boolean)", key));
                break;
            }
            values.emplace(name, value.get<bool>());
        }
        return values;
    }

    /// A component path: an array of names and {"name", "occurrence"} objects.
    [[nodiscard]] GuiComponentPath path(const json& value, std::string_view context)
    {
        GuiComponentPath path;
        if (!value.is_array())
        {
            failure(context, "a component path is not an array");
            return path;
        }
        for (const json& element : value)
        {
            if (element.is_string())
            {
                path.push_back({.name = element.get<std::string>(), .occurrence = 1});
            }
            else
            {
                path.push_back({.name       = string(element, "name", context),
                                .occurrence = integer(element, "occurrence", context)});
            }
        }
        return path;
    }

    [[nodiscard]] std::vector<GuiComponentPath> paths(const json& object, std::string_view key,
                                                      std::string_view context)
    {
        std::vector<GuiComponentPath> paths;
        for (const json& value : array(object, key, context))
        {
            paths.push_back(path(value, context));
        }
        return paths;
    }

    /// A typed number, {"text", "unit"} and nothing else.
    [[nodiscard]] GuiTypedValue typed(const json& value, std::string_view key,
                                      std::string_view context)
    {
        if (!value.is_object() || value.size() != 2 || !value.contains("text") ||
            !value.contains("unit"))
        {
            failure(context, std::format(R"("{}" is not {{"text", "unit"}})", key));
            return {};
        }
        return {.text = string(value, "text", context), .unit = string(value, "unit", context)};
    }

    [[nodiscard]] std::optional<GuiTypedValue> optionalTyped(const json&      object,
                                                             std::string_view key,
                                                             std::string_view context)
    {
        const json* value = optionalMember(object, key);
        return value == nullptr ? std::nullopt
                                : std::optional<GuiTypedValue>{typed(*value, key, context)};
    }

    /// Fails unless @p object has the "schema" @p v1 of version 1 or @p v2 of version 2; the
    /// version (0 after a failure).
    int schemaVersion(const json& object, std::string_view v1, std::string_view v2)
    {
        const std::string schema  = string(object, "schema", "the document");
        const int         version = integer(object, "schemaVersion", "the document");
        if ((schema == v1 && version == 1) || (schema == v2 && version == 2))
        {
            return version;
        }
        failure("the document",
                std::format("neither a {} of version 1 nor a {} of version 2", v1, v2));
        return 0;
    }

private:
    std::optional<std::string> m_error;
};

// ------------------------------------------------------------------------------- the answers

/// The question the answer of a step of kind @p kind answers; nullopt for New, whose question
/// is the current sub-tab's (the chooser, the deployment or the separation dialog).
[[nodiscard]] std::optional<GuiQuestionKind> questionOfStep(GuiStepKind kind) noexcept
{
    switch (kind)
    {
        case GuiStepKind::RENAME_CONFIGURATION:
            return GuiQuestionKind::RENAME;
        case GuiStepKind::SELECT_MOTOR:
            return GuiQuestionKind::MOTOR;
        case GuiStepKind::SELECT_IGNITION:
            return GuiQuestionKind::IGNITION;
        case GuiStepKind::SELECT_DEPLOYMENT:
            return GuiQuestionKind::DEPLOYMENT;
        case GuiStepKind::SELECT_SEPARATION:
            return GuiQuestionKind::SEPARATION;
        default:
            return std::nullopt;
    }
}

/// Whether @p event is one whose altitude field the deployment or separation dialog enables
/// (DeploymentSelectionDialog and SeparationSelectionDialog's updateState()).
[[nodiscard]] bool isAltitudeEvent(std::string_view event) noexcept
{
    return event == "ALTITUDE" || event == "ALTITUDE_ASCENDING" || event == "ALTITUDE_DESCENDING";
}

/// The members of @p answer besides "button" that are set, by name.
[[nodiscard]] std::vector<std::string_view> setMembers(const GuiAnswer& answer)
{
    std::vector<std::string_view> set;
    const auto                    add = [&set](bool isSet, std::string_view name) {
        if (isSet)
        {
            set.push_back(name);
        }
    };
    add(answer.scope.has_value(), "scope");
    add(answer.event.has_value(), "event");
    add(answer.altitude.has_value(), "altitude");
    add(answer.delay.has_value(), "delay");
    add(answer.name.has_value(), "name");
    add(answer.saveAsDefault, "saveAsDefault");
    add(answer.search.has_value(), "search");
    add(answer.motor.has_value(), "motor");
    add(answer.curve.has_value(), "curve");
    add(answer.chooserDelay.has_value(), "chooserDelay");
    add(answer.nozzleExitDiameter.has_value(), "nozzleExitDiameter");
    return set;
}

/// Fails unless the members of @p answer besides its button are among @p allowed and its button
/// is one of @p buttons.
void checkMembers(JsonReader& reader, const GuiAnswer& answer,
                  std::span<const std::string_view> allowed,
                  std::span<const std::string_view> buttons, std::string_view context)
{
    for (const std::string_view member : setMembers(answer))
    {
        if (!contains(allowed, member))
        {
            reader.failure(context,
                           std::format(R"(the answer has "{}", which its question has not)",
                                       member == "chooserDelay" ? "delay" : member));
        }
    }
    if (!contains(buttons, answer.button))
    {
        reader.failure(context,
                       std::format(R"(the answer's question has no button "{}")", answer.button));
    }
}

/// The value dialogs' rules (Questions.ignition() and valueDialog() of the tool): Cancel alone,
/// or OK with the scope ("all" or "this"), the event and the delay; the altitude exactly for an
/// altitude event (never for an ignition).
void checkValueAnswer(JsonReader& reader, const GuiAnswer& answer, bool ignition,
                      std::string_view context)
{
    constexpr std::array<std::string_view, 3> kIgnitionMembers{"scope", "event", "delay"};
    constexpr std::array<std::string_view, 4> kValueMembers{"scope", "event", "altitude", "delay"};
    constexpr std::array<std::string_view, 2> kButtons{"ok", "cancel"};
    if (answer.button == "cancel")
    {
        checkMembers(reader, answer, {}, kButtons, context);
        return;
    }
    checkMembers(reader, answer,
                 ignition ? std::span<const std::string_view>(kIgnitionMembers)
                          : std::span<const std::string_view>(kValueMembers),
                 kButtons, context);
    if (!answer.scope || !answer.event || !answer.delay)
    {
        reader.failure(context,
                       R"(an OK answer of a value dialog has "scope", "event" and "delay")");
        return;
    }
    if (*answer.scope != "all" && *answer.scope != "this")
    {
        reader.failure(context, std::format(R"(no scope "{}": "all" or "this")", *answer.scope));
    }
    const bool altitude = !ignition && isAltitudeEvent(*answer.event);
    if (answer.altitude.has_value() != altitude)
    {
        reader.failure(context, std::format(R"(the event {} {} an altitude)", *answer.event,
                                            altitude ? "needs" : "has no"));
    }
}

/// Checks @p answer against the question @p question (nullopt: New's, the motor chooser's or a
/// value dialog's, told apart by the members).
void checkAnswer(JsonReader& reader, const GuiAnswer& answer,
                 std::optional<GuiQuestionKind> question, std::string_view context)
{
    constexpr std::array<std::string_view, 5> kChooserMembers{"search", "motor", "curve",
                                                              "chooserDelay", "nozzleExitDiameter"};
    constexpr std::array<std::string_view, 2> kRenameMembers{"name", "saveAsDefault"};
    constexpr std::array<std::string_view, 2> kOkCancel{"ok", "cancel"};
    constexpr std::array<std::string_view, 3> kRenameButtons{"ok", "reset", "cancel"};
    const bool valueMembers = answer.scope || answer.event || answer.altitude || answer.delay;
    if (!question)
    {
        // New: a value dialog's answer (deployment or separation) or the chooser's.
        question = valueMembers ? GuiQuestionKind::DEPLOYMENT : GuiQuestionKind::MOTOR;
    }
    switch (*question)
    {
        case GuiQuestionKind::MOTOR:
            checkMembers(reader, answer, kChooserMembers, kOkCancel, context);
            break;
        case GuiQuestionKind::RENAME:
            checkMembers(reader, answer, kRenameMembers, kRenameButtons, context);
            break;
        case GuiQuestionKind::IGNITION:
            checkValueAnswer(reader, answer, true, context);
            break;
        case GuiQuestionKind::DEPLOYMENT:
        case GuiQuestionKind::SEPARATION:
            checkValueAnswer(reader, answer, false, context);
            break;
        case GuiQuestionKind::MESSAGE:
            reader.failure(context, "a message is answered by the rename dialog's answer");
            break;
    }
}

/// The chooser's motor of an answer: {"manufacturer", "designation"} and nothing else.
[[nodiscard]] GuiMotor answeredMotor(JsonReader& reader, const json& value,
                                     std::string_view context)
{
    if (!value.is_object() || value.size() != 2)
    {
        reader.failure(context, R"(the answer's "motor" is not {"manufacturer", "designation"})");
        return {};
    }
    return {.manufacturer = reader.string(value, "manufacturer", context),
            .designation  = reader.string(value, "designation", context),
            .digest       = {}};
}

/// The answer @p object of a step of kind @p kind.
[[nodiscard]] GuiAnswer answerOf(JsonReader& reader, const json& object, GuiStepKind kind,
                                 std::string_view context)
{
    GuiAnswer answer;
    if (!object.is_object())
    {
        reader.failure(context, "the answer is not an object");
        return answer;
    }
    for (const auto& [key, value] : object.items())
    {
        if (!contains(kAnswerMembers, key))
        {
            reader.failure(context, std::format(R"(an answer has no member "{}")", key));
        }
    }
    answer.button   = reader.string(object, "button", context);
    answer.scope    = reader.optionalString(object, "scope", context);
    answer.event    = reader.optionalString(object, "event", context);
    answer.altitude = reader.optionalTyped(object, "altitude", context);
    if (const json* const delay = JsonReader::optionalMember(object, "delay"))
    {
        // The chooser's delay is the delay box's text, a value dialog's a typed number.
        if (delay->is_string())
        {
            answer.chooserDelay = delay->get<std::string>();
        }
        else
        {
            answer.delay = reader.typed(*delay, "delay", context);
        }
    }
    answer.name = reader.optionalString(object, "name", context);
    if (JsonReader::optionalMember(object, "saveAsDefault") != nullptr)
    {
        answer.saveAsDefault = reader.boolean(object, "saveAsDefault", context);
        if (!answer.saveAsDefault)
        {
            reader.failure(context, R"("saveAsDefault" is true or left out)");
        }
    }
    answer.search = reader.optionalString(object, "search", context);
    if (const json* const motor = JsonReader::optionalMember(object, "motor"))
    {
        answer.motor = answeredMotor(reader, *motor, context);
    }
    answer.curve              = reader.optionalString(object, "curve", context);
    answer.nozzleExitDiameter = reader.optionalTyped(object, "nozzleExitDiameter", context);
    checkAnswer(reader, answer, questionOfStep(kind), context);
    return answer;
}

// --------------------------------------------------------------------------------- the steps

/// The members of the step @p object that its kind does not take.
[[nodiscard]] std::vector<std::string> unknownMembers(const json& object, const StepSpec& spec)
{
    const std::vector<std::string> allowed = Strings::split(spec.members, ' ');
    std::vector<std::string>       unknown;
    for (const auto& [key, value] : object.items())
    {
        if (key != "step" && key != "note" && std::ranges::find(allowed, key) == allowed.end())
        {
            unknown.push_back(key);
        }
    }
    return unknown;
}

/// The members of a step that are strings, by name.
struct StringMember
{
    std::string_view name;
    std::string GuiScenarioStep::* member{nullptr};
};

constexpr std::array kStringMembers{
    StringMember{.name = "component", .member = &GuiScenarioStep::component},
    StringMember{.name = "field", .member = &GuiScenarioStep::field},
    StringMember{.name = "text", .member = &GuiScenarioStep::text},
    StringMember{.name = "item", .member = &GuiScenarioStep::item},
    StringMember{.name = "column", .member = &GuiScenarioStep::column},
    StringMember{.name = "button", .member = &GuiScenarioStep::button},
    StringMember{.name = "file", .member = &GuiScenarioStep::file},
};

/// A cell of selectCells: {"row", "column" (left out: column 0), "with" ("ctrl" or "shift", left
/// out: a plain click)} (Scenario.cell() of the tool).
[[nodiscard]] GuiCell cellOf(JsonReader& reader, const json& object, std::string_view context)
{
    GuiCell cell;
    if (!object.is_object())
    {
        reader.failure(context, "a cell is not an object");
        return cell;
    }
    for (const auto& [key, value] : object.items())
    {
        if (key != "row" && key != "column" && key != "with")
        {
            reader.failure(context, std::format(R"(a cell has no member "{}")", key));
        }
    }
    cell.row = reader.integer(object, "row", context);
    if (const json* const column = JsonReader::optionalMember(object, "column"))
    {
        cell.column = reader.path(*column, context);
    }
    cell.with = reader.optionalString(object, "with", context).value_or("");
    if (!cell.with.empty() && cell.with != "ctrl" && cell.with != "shift")
    {
        reader.failure(context, std::format(R"(a cell is clicked with "ctrl" or "shift", not "{}")",
                                            cell.with));
    }
    return cell;
}

/// The cells of a selectCells step (at least one).
[[nodiscard]] std::vector<GuiCell> cellsOf(JsonReader& reader, const json& object,
                                           std::string_view context)
{
    std::vector<GuiCell> cells;
    for (const json& cell : reader.array(object, "cells", context))
    {
        cells.push_back(cellOf(reader, cell, context));
    }
    if (cells.empty())
    {
        reader.failure(context, "selectCells clicks no cell");
    }
    return cells;
}

/// The tab of a tab or configTab step, one of its tabs.
[[nodiscard]] std::string tabOf(JsonReader& reader, const json& object, GuiStepKind kind,
                                std::string_view context)
{
    constexpr std::array<std::string_view, 2> kWindowTabs{"design", "flightConfiguration"};
    constexpr std::array<std::string_view, 3> kConfigTabs{"motors", "recovery", "stages"};
    std::string                               tab = reader.string(object, "tab", context);
    const std::span<const std::string_view>   tabs =
        kind == GuiStepKind::TAB ? std::span<const std::string_view>(kWindowTabs)
                                 : std::span<const std::string_view>(kConfigTabs);
    if (!contains(tabs, tab))
    {
        reader.failure(context, std::format(R"(no tab "{}")", tab));
    }
    return tab;
}

/// Reads the member @p member of the step @p object into @p step.
void readStepMember(JsonReader& reader, const json& object, std::string_view member,
                    std::string_view context, GuiScenarioStep& step)
{
    if (member == "paths")
    {
        step.paths = reader.paths(object, member, context);
    }
    else if (member == "checked")
    {
        step.checked = reader.boolean(object, member, context);
    }
    else if (member == "row")
    {
        step.row = reader.integer(object, member, context);
    }
    else if (member == "unit")
    {
        step.unit = reader.optionalString(object, member, context);
    }
    else if (member == "tab")
    {
        step.tab = tabOf(reader, object, step.kind, context);
    }
    else if (member == "cells")
    {
        step.cells = cellsOf(reader, object, context);
    }
    else if (member == "mount")
    {
        if (const json* const mount = reader.member(object, member, context))
        {
            step.mount = reader.path(*mount, context);
        }
    }
    else if (member == "answer")
    {
        // New asks nothing on a design without the sub-tab's columns: its answer may be left out.
        const json* const answer = step.kind == GuiStepKind::NEW_CONFIGURATION
                                       ? JsonReader::optionalMember(object, member)
                                       : reader.member(object, member, context);
        if (answer != nullptr)
        {
            step.answer = answerOf(reader, *answer, step.kind, context);
        }
    }
    else
    {
        const std::span<const StringMember> members(kStringMembers);
        const auto found = std::ranges::find(members, member, &StringMember::name);
        if (found == members.end())
        {
            reader.failure(context, std::format(R"(the tool knows no member "{}")", member));
            return;
        }
        step.*found->member = reader.string(object, member, context);
    }
}

/// Step @p index of a scenario of version @p version.
[[nodiscard]] GuiScenarioStep scenarioStep(JsonReader& reader, const json& object,
                                           std::size_t index, int version)
{
    const std::string context = std::format("step {}", index);
    GuiScenarioStep   step;
    step.index                 = index;
    const StepSpec* const spec = stepSpecNamed(reader.string(object, "step", context));
    if (spec == nullptr)
    {
        reader.failure(context, "an unknown step");
        return step;
    }
    step.kind = spec->kind;
    if ((spec->versions & (version == 1 ? kV1 : kV2)) == 0U)
    {
        reader.failure(context,
                       std::format(R"("{}" is no step of version {})", spec->name, version));
    }
    for (const std::string& member : unknownMembers(object, *spec))
    {
        reader.failure(
            context, std::format(R"(the member "{}" does not belong to "{}")", member, spec->name));
    }
    step.note = reader.optionalString(object, "note", context).value_or("");
    for (const std::string& member : Strings::split(spec->members, ' '))
    {
        // A kind without members has the one empty name.
        if (!member.empty())
        {
            readStepMember(reader, object, member, context, step);
        }
    }
    return step;
}

/// Fails unless the scenario starts with File > New (version 1) or File > Open (version 2, and
/// only there).
void checkFirstStep(JsonReader& reader, const GuiScenario& scenario)
{
    const GuiStepKind first = scenario.schemaVersion == 1 ? GuiStepKind::NEW : GuiStepKind::OPEN;
    if (scenario.steps.empty() || scenario.steps.front().kind != first)
    {
        reader.failure("the document",
                       std::format("the scenario does not start with {}",
                                   first == GuiStepKind::NEW ? "File > New" : "File > Open"));
    }
    for (const GuiScenarioStep& step : scenario.steps)
    {
        if (step.index > 0 && step.kind == first)
        {
            reader.failure(std::format("step {}", step.index),
                           std::format(R"(only the first step is "{}")", guiStepName(first)));
        }
    }
}

// ------------------------------------------------------------------------------- the records

[[nodiscard]] GuiActionState actionState(JsonReader& reader, const json& object,
                                         std::string_view key, std::string_view context)
{
    const json* const action = reader.member(object, key, context);
    if (action == nullptr)
    {
        return {};
    }
    return {.text    = reader.string(*action, "text", context),
            .enabled = reader.boolean(*action, "enabled", context)};
}

/// A motor of a record: {"manufacturer", "designation", "digest"}.
[[nodiscard]] GuiMotor recordedMotor(JsonReader& reader, const json& value,
                                     std::string_view context)
{
    return {.manufacturer = reader.string(value, "manufacturer", context),
            .designation  = reader.string(value, "designation", context),
            .digest       = reader.string(value, "digest", context)};
}

/// What the motor chooser showed (the tool's chooserState(); the motor and the delay text are
/// left out when there are none).
[[nodiscard]] GuiChooserState chooserState(JsonReader& reader, const json& object,
                                           std::string_view context)
{
    GuiChooserState state;
    state.search = reader.string(object, "search", context);
    state.rows   = reader.integer(object, "rows", context);
    if (const json* const motor = JsonReader::optionalMember(object, "motor"))
    {
        state.motor = recordedMotor(reader, *motor, context);
    }
    state.delayText          = reader.optionalString(object, "delayText", context);
    state.delay              = reader.string(object, "delay", context);
    state.nozzleExitDiameter = reader.string(object, "nozzleExitDiameter", context);
    return state;
}

/// The members of a value dialog's question.
void readValueQuestion(JsonReader& reader, const json& object, std::string_view context,
                       GuiQuestion& question)
{
    question.scope        = reader.string(object, "scope", context);
    question.defaultText  = reader.string(object, "default", context);
    question.overrideText = reader.string(object, "override", context);
    question.event        = reader.string(object, "event", context);
    question.delay        = reader.string(object, "delay", context);
    if (question.kind != GuiQuestionKind::IGNITION)
    {
        question.altitude        = reader.string(object, "altitude", context);
        question.altitudeEnabled = reader.boolean(object, "altitudeEnabled", context);
        question.events          = reader.strings(object, "events", context);
    }
}

[[nodiscard]] GuiQuestion questionOf(JsonReader& reader, const json& object,
                                     std::string_view context)
{
    GuiQuestion                          question;
    const std::optional<GuiQuestionKind> kind =
        questionKindNamed(reader.string(object, "question", context));
    if (!kind)
    {
        reader.failure(context, "an unknown question");
        return question;
    }
    question.kind  = *kind;
    question.title = reader.string(object, "title", context);
    switch (question.kind)
    {
        case GuiQuestionKind::MOTOR:
            question.shown = chooserState(reader, object, context);
            question.answered =
                chooserState(reader, reader.object(object, "answered", context), context);
            break;
        case GuiQuestionKind::IGNITION:
        case GuiQuestionKind::DEPLOYMENT:
        case GuiQuestionKind::SEPARATION:
            readValueQuestion(reader, object, context, question);
            break;
        case GuiQuestionKind::RENAME:
            question.name = reader.string(object, "name", context);
            break;
        case GuiQuestionKind::MESSAGE:
            question.text = reader.string(object, "text", context);
            break;
    }
    return question;
}

/// A row of a table: {"configuration", "cells"}, a cell of the Motors table {"motor",
/// "ignition", "ignitionDefault"}, of the others {"text", "shaded", "override"}.
[[nodiscard]] GuiTableRow tableRow(JsonReader& reader, const json& object, bool motors,
                                   std::string_view context)
{
    GuiTableRow row;
    row.configuration = reader.string(object, "configuration", context);
    for (const json& cell : reader.array(object, "cells", context))
    {
        if (motors)
        {
            row.motorCells.push_back(
                {.motor           = reader.string(cell, "motor", context),
                 .ignition        = reader.string(cell, "ignition", context),
                 .ignitionDefault = reader.boolean(cell, "ignitionDefault", context)});
        }
        else
        {
            row.cells.push_back({.text     = reader.string(cell, "text", context),
                                 .shaded   = reader.boolean(cell, "shaded", context),
                                 .override = reader.boolean(cell, "override", context)});
        }
    }
    return row;
}

/// One of the three sub-panels; @p motors for the Motors sub-panel, which also has its help and
/// the motor mount table.
[[nodiscard]] GuiConfigurationTable configurationTable(JsonReader& reader, const json& object,
                                                       bool motors, std::string_view context)
{
    GuiConfigurationTable table;
    table.buttons = reader.booleans(object, "buttons", context);
    table.header  = reader.strings(object, "header", context);
    for (const json& row : reader.array(object, "rows", context))
    {
        table.rows.push_back(tableRow(reader, row, motors, context));
    }
    table.selectedRows    = reader.integers(object, "selectedRows", context);
    table.selectedColumns = reader.integers(object, "selectedColumns", context);
    if (motors)
    {
        table.showsHelp = reader.boolean(object, "showsHelp", context);
        for (const json& mount : reader.array(object, "mounts", context))
        {
            const json* const component = reader.member(mount, "component", context);
            table.mounts.push_back({.component  = component == nullptr
                                                      ? GuiComponentPath{}
                                                      : reader.path(*component, context),
                                    .text       = reader.string(mount, "text", context),
                                    .motorMount = reader.boolean(mount, "motorMount", context)});
        }
    }
    return table;
}

[[nodiscard]] GuiFlightConfigurationState flightConfigurationState(JsonReader&      reader,
                                                                   const json&      object,
                                                                   std::string_view context)
{
    GuiFlightConfigurationState state;
    state.configTab = reader.string(object, "configTab", context);
    for (const json& tab : reader.array(object, "configTabs", context))
    {
        state.configTabs.push_back({.tab     = reader.string(tab, "tab", context),
                                    .title   = reader.string(tab, "title", context),
                                    .enabled = reader.boolean(tab, "enabled", context)});
    }
    state.buttons               = reader.booleans(object, "buttons", context);
    state.selectedConfiguration = reader.integer(object, "selectedConfiguration", context);
    for (const json& simulation : reader.array(object, "simulations", context))
    {
        state.simulations.push_back(
            {.name          = reader.string(simulation, "name", context),
             .configuration = reader.integer(simulation, "configuration", context)});
    }
    state.motors =
        configurationTable(reader, reader.object(object, "motors", context), true, context);
    state.recovery =
        configurationTable(reader, reader.object(object, "recovery", context), false, context);
    state.stages =
        configurationTable(reader, reader.object(object, "stages", context), false, context);
    return state;
}

/// The members of a record of version 2: the tab, the questions, what the tab showed.
void readTabRecord(JsonReader& reader, const json& object, std::string_view context,
                   GuiStepRecord& record)
{
    record.tab = reader.string(object, "tab", context);
    if (record.tab != "design" && record.tab != "flightConfiguration")
    {
        reader.failure(context, std::format(R"(no tab "{}")", record.tab));
    }
    if (JsonReader::optionalMember(object, "questions") != nullptr)
    {
        for (const json& question : reader.array(object, "questions", context))
        {
            record.questions.push_back(questionOf(reader, question, context));
        }
    }
    record.flightConfiguration = flightConfigurationState(
        reader, reader.object(object, "flightConfiguration", context), context);
}

[[nodiscard]] GuiStepRecord stepRecord(JsonReader& reader, const json& object, std::size_t at,
                                       int version)
{
    const std::string context = std::format("record {}", at);
    GuiStepRecord     record;
    const int         index    = reader.integer(object, "index", context);
    record.index               = index < 0 ? 0 : static_cast<std::size_t>(index);
    const StepSpec* const spec = stepSpecNamed(reader.string(object, "step", context));
    if (spec == nullptr)
    {
        reader.failure(context, "an unknown step");
        return record;
    }
    record.kind      = spec->kind;
    record.undo      = actionState(reader, object, "undo", context);
    record.redo      = actionState(reader, object, "redo", context);
    record.saved     = reader.boolean(object, "saved", context);
    record.selection = reader.paths(object, "selection", context);
    if (object.contains("open"))
    {
        const json& open = object.at("open");
        record.window    = reader.string(open, "window", context);
        if (open.contains("component"))
        {
            record.windowComponent = reader.path(open.at("component"), context);
        }
    }
    record.treeSha256 = reader.string(object, "treeSha256", context);
    if (object.contains("tree"))
    {
        record.tree = reader.strings(object, "tree", context);
    }
    if (version == 2)
    {
        readTabRecord(reader, object, context, record);
    }
    return record;
}

/// The members of the reference of version 2 only: the preset commit, the start-up, the input and
/// the archive's digests.
void readTabReference(JsonReader& reader, const json& document, GuiReference& reference)
{
    reference.presetDatabaseCommit =
        reader.string(reader.object(document, "openrocket", "the document"), "presetDatabaseCommit",
                      "openrocket");
    const json& startup         = reader.object(document, "startup", "the document");
    reference.startup.motorSets = reader.integer(startup, "motorSets", "startup");
    reference.startup.motors    = reader.integer(startup, "motors", "startup");
    reference.startup.presets   = reader.integer(startup, "presets", "startup");
    const json& input           = reader.object(document, "input", "the document");
    reference.inputFile         = reader.string(input, "file", "input");
    reference.inputSha256       = reader.string(input, "sha256", "input");
    const json& archive = reader.object(reader.object(document, "reference", "the document"),
                                        "archiveSha256", "reference");
    for (const auto& [entry, sha256] : archive.items())
    {
        if (!sha256.is_string())
        {
            reader.failure("reference", "a member of \"archiveSha256\" is not a string");
            break;
        }
        reference.archiveSha256.emplace(entry, sha256.get<std::string>());
    }
}

[[nodiscard]] Result<json> parsed(std::string_view text)
{
    Result<json> document = parseGoldenJson(text);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    return document;
}

}  // namespace

std::filesystem::path guiFromScratchDir()
{
    return guiScenarioDir("from-scratch");
}

std::filesystem::path guiScenarioDir(std::string_view scenario)
{
    return goldensDir() / "gui" / pathFromUtf8(scenario);
}

Result<std::filesystem::path> guiRepositoryFile(std::string_view path)
{
    if (path.empty() || path.front() == '/' || path.contains('\\') || path.contains(':'))
    {
        return fail(ErrorCode::PARSE, std::format("'{}' is no path of the repository", path));
    }
    // data/ is a directory of the repository's root.
    std::filesystem::path file = dataDir().parent_path();
    for (const std::string& name : Strings::split(path, '/'))
    {
        if (name.empty() || name == "." || name == "..")
        {
            return fail(ErrorCode::PARSE, std::format("'{}' is no path of the repository", path));
        }
        file /= pathFromUtf8(name);
    }
    return file;
}

std::string guiComponentPathText(const GuiComponentPath& path)
{
    if (path.empty())
    {
        return "(rocket)";
    }
    std::string text;
    for (const GuiPathElement& element : path)
    {
        text += (text.empty() ? "" : "/") + element.name;
        if (element.occurrence != 1)
        {
            text += std::format("#{}", element.occurrence);
        }
    }
    return text;
}

Result<const RocketComponent*> resolveGuiComponentPath(const Rocket&           rocket,
                                                       const GuiComponentPath& path)
{
    const RocketComponent* component = &rocket;
    for (const GuiPathElement& element : path)
    {
        const RocketComponent* found = nullptr;
        int                    seen  = 0;
        for (const RocketComponent* child : component->getChildren())
        {
            if (child->getName() == element.name && ++seen == element.occurrence)
            {
                found = child;
                break;
            }
        }
        if (found == nullptr)
        {
            return fail(ErrorCode::NOT_FOUND,
                        std::format("no component {} (occurrence {}) under '{}'", element.name,
                                    element.occurrence, component->getName()));
        }
        component = found;
    }
    return component;
}

GuiComponentPath guiComponentPathOf(const RocketComponent& component)
{
    GuiComponentPath path;
    for (const RocketComponent* c = &component; c->getParent() != nullptr; c = c->getParent())
    {
        int occurrence = 0;
        for (const RocketComponent* sibling : c->getParent()->getChildren())
        {
            if (sibling->getName() == c->getName())
            {
                occurrence++;
            }
            if (sibling == c)
            {
                break;
            }
        }
        path.push_back({.name = c->getName(), .occurrence = occurrence});
    }
    std::ranges::reverse(path);
    return path;
}

std::string_view guiStepName(GuiStepKind kind) noexcept
{
    const std::span<const StepSpec> specs(kStepSpecs);
    const auto                      found =
        std::ranges::find_if(specs, [kind](const StepSpec& spec) { return spec.kind == kind; });
    return found == specs.end() ? std::string_view() : found->name;
}

bool isGuiTabStep(GuiStepKind kind) noexcept
{
    return kind >= GuiStepKind::CONFIG_TAB && kind <= GuiStepKind::RESET_SEPARATION;
}

bool isGuiTabOperation(GuiStepKind kind) noexcept
{
    return isGuiTabStep(kind) && kind != GuiStepKind::CONFIG_TAB &&
           kind != GuiStepKind::SELECT_CELLS;
}

std::string_view guiQuestionName(GuiQuestionKind kind) noexcept
{
    const std::span<const QuestionSpec> specs(kQuestionSpecs);
    const auto                          found = std::ranges::find(specs, kind, &QuestionSpec::kind);
    return found == specs.end() ? std::string_view() : found->name;
}

Result<GuiScenario> parseGuiScenario(std::string_view text)
{
    const Result<json> document = parsed(text);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    JsonReader  reader;
    GuiScenario scenario;
    scenario.schemaVersion = reader.schemaVersion(*document, kScenarioSchemaV1, kScenarioSchemaV2);
    scenario.title         = reader.string(*document, "title", "the document");
    const json& steps      = reader.array(*document, "steps", "the document");
    for (std::size_t index = 0; index < steps.size(); index++)
    {
        scenario.steps.push_back(
            scenarioStep(reader, steps.at(index), index, scenario.schemaVersion));
    }
    checkFirstStep(reader, scenario);
    if (!reader.ok())
    {
        return fail(ErrorCode::PARSE, reader.error());
    }
    return scenario;
}

Result<GuiReference> parseGuiReference(std::string_view text)
{
    const Result<json> document = parsed(text);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    JsonReader   reader;
    GuiReference reference;
    reference.schemaVersion =
        reader.schemaVersion(*document, kReferenceSchemaV1, kReferenceSchemaV2);
    if (const json* const openrocket = reader.member(*document, "openrocket", "the document"))
    {
        reference.openRocketCommit  = reader.string(*openrocket, "commit", "openrocket");
        reference.openRocketVersion = reader.string(*openrocket, "version", "openrocket");
        reference.javaVersion       = reader.string(*openrocket, "javaVersion", "openrocket");
        reference.dirty             = reader.boolean(*openrocket, "dirty", "openrocket");
    }
    if (const json* const scenario = reader.member(*document, "scenario", "the document"))
    {
        reference.scenarioFile   = reader.string(*scenario, "file", "scenario");
        reference.scenarioSha256 = reader.string(*scenario, "sha256", "scenario");
        reference.scenarioTitle  = reader.string(*scenario, "title", "scenario");
        const int steps          = reader.integer(*scenario, "steps", "scenario");
        reference.scenarioSteps  = steps < 0 ? 0 : static_cast<std::size_t>(steps);
    }
    if (const json* const saved = reader.member(*document, "reference", "the document"))
    {
        reference.referenceFile  = reader.string(*saved, "file", "reference");
        reference.savedAs        = reader.string(*saved, "savedAs", "reference");
        reference.archiveEntries = reader.strings(*saved, "archiveEntries", "reference");
    }
    if (reference.schemaVersion == 2)
    {
        readTabReference(reader, *document, reference);
    }
    const json& steps = reader.array(*document, "steps", "the document");
    for (std::size_t at = 0; at < steps.size(); at++)
    {
        reference.steps.push_back(stepRecord(reader, steps.at(at), at, reference.schemaVersion));
    }
    if (const json* const loaded = reader.member(*document, "loaded", "the document"))
    {
        reference.loadedLines   = reader.strings(*loaded, "lines", "loaded");
        reference.loadedNumbers = reader.strings(*loaded, "numbers", "loaded");
    }
    if (!reader.ok())
    {
        return fail(ErrorCode::PARSE, reader.error());
    }
    return reference;
}

std::string guiTreeSha256(std::span<const std::string> lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line;
        text += '\n';
    }
    return sha256Hex(stringToBytes(text));
}

std::span<const GuiReloadChange> guiReloadChanges(std::string_view scenario) noexcept
{
    if (scenario == "from-scratch")
    {
        return kGuiSaveAndReloadChanges;
    }
    if (scenario == "flight-configurations")
    {
        return kGuiFlightConfigurationsReloadChanges;
    }
    return kGuiMultiStageReloadChanges;
}

std::vector<std::string> guiReloadedTree(std::vector<std::string>         tree,
                                         std::span<const GuiReloadChange> changes,
                                         std::string&                     problems)
{
    for (const GuiReloadChange& change : changes)
    {
        std::size_t made = 0;
        for (std::string& line : tree)
        {
            const std::size_t at = line.find(change.saved);
            if (line.starts_with(change.line) && at != std::string::npos)
            {
                line.replace(at, change.saved.size(), change.reloaded);
                made++;
            }
        }
        if (made != change.times)
        {
            problems += std::format("the saved tree has {} lines {}...{}, not {}\n", made,
                                    change.line, change.saved, change.times);
        }
    }
    return tree;
}

std::vector<std::string> guiReloadedTree(std::vector<std::string> tree, std::string& problems)
{
    return guiReloadedTree(std::move(tree), kGuiSaveAndReloadChanges, problems);
}

namespace
{

/// The text member of GuiQuestion named @p member (guiQuestionMember()); null for another name.
[[nodiscard]] std::string GuiQuestion::* questionMemberNamed(std::string_view member) noexcept
{
    struct Member
    {
        std::string_view name;
        std::string GuiQuestion::* member{nullptr};
    };
    constexpr std::array kMembers{
        Member{.name = "title", .member = &GuiQuestion::title},
        Member{.name = "scope", .member = &GuiQuestion::scope},
        Member{.name = "defaultText", .member = &GuiQuestion::defaultText},
        Member{.name = "overrideText", .member = &GuiQuestion::overrideText},
        Member{.name = "event", .member = &GuiQuestion::event},
        Member{.name = "delay", .member = &GuiQuestion::delay},
        Member{.name = "altitude", .member = &GuiQuestion::altitude},
        Member{.name = "name", .member = &GuiQuestion::name},
        Member{.name = "text", .member = &GuiQuestion::text},
    };
    const std::span<const Member> members(kMembers);
    const auto                    found = std::ranges::find(members, member, &Member::name);
    return found == members.end() ? nullptr : found->member;
}

/// @p values joined by @p separator.
[[nodiscard]] std::string joinedTexts(std::span<const std::string> values,
                                      std::string_view             separator)
{
    std::string text;
    for (const std::string& value : values)
    {
        text += (text.empty() ? "" : std::string(separator)) + value;
    }
    return text;
}

/// @p values as "[1, 2]".
[[nodiscard]] std::string integersText(std::span<const int> values)
{
    std::string text;
    for (const int value : values)
    {
        text += (text.empty() ? "" : ", ") + std::to_string(value);
    }
    return "[" + text + "]";
}

/// @p buttons as "a=true b=false" (in the map's order).
[[nodiscard]] std::string buttonsText(const std::map<std::string, bool, std::less<>>& buttons)
{
    std::string text;
    for (const auto& [name, enabled] : buttons)
    {
        text += std::format("{}{}={}", text.empty() ? "" : " ", name, enabled);
    }
    return text;
}

/// What a motor chooser showed, in one line.
[[nodiscard]] std::string chooserText(const GuiChooserState& state)
{
    return std::format(
        "search='{}' rows={} motor={} delayText={} delay={} nozzleExitDiameter={}", state.search,
        state.rows,
        state.motor.has_value() ? std::format("{} {} {}", state.motor->manufacturer,
                                              state.motor->designation, state.motor->digest)
                                : std::string("none"),
        state.delayText.has_value() ? "'" + *state.delayText + "'" : std::string("none"),
        state.delay, state.nozzleExitDiameter);
}

/// The lines of the question @p question, the step's question @p at.
void addQuestionLines(std::size_t at, const GuiQuestion& question, std::vector<std::string>& lines)
{
    const std::string prefix = std::format("question {}", at);
    lines.push_back(
        std::format("{}: {} '{}'", prefix, guiQuestionName(question.kind), question.title));
    switch (question.kind)
    {
        case GuiQuestionKind::MOTOR:
            lines.push_back(prefix + " shown: " + chooserText(question.shown));
            lines.push_back(prefix + " answered: " + chooserText(question.answered));
            return;
        case GuiQuestionKind::IGNITION:
        case GuiQuestionKind::DEPLOYMENT:
        case GuiQuestionKind::SEPARATION:
            lines.push_back(std::format("{} scope: {}", prefix, question.scope));
            lines.push_back(std::format("{} default: {}", prefix, question.defaultText));
            lines.push_back(std::format("{} override: {}", prefix, question.overrideText));
            lines.push_back(
                std::format("{} event: {} delay: {}", prefix, question.event, question.delay));
            if (question.kind != GuiQuestionKind::IGNITION)
            {
                lines.push_back(std::format("{} altitude: {} enabled: {}", prefix,
                                            question.altitude, question.altitudeEnabled));
                lines.push_back(
                    std::format("{} events: {}", prefix, joinedTexts(question.events, " ")));
            }
            return;
        case GuiQuestionKind::RENAME:
            lines.push_back(std::format("{} name: {}", prefix, question.name));
            return;
        case GuiQuestionKind::MESSAGE:
            lines.push_back(std::format("{} text: {}", prefix, question.text));
            return;
    }
}

/// The lines of the table @p table named @p name ("motors").
void addTableLines(std::string_view name, const GuiConfigurationTable& table, bool motors,
                   std::vector<std::string>& lines)
{
    lines.push_back(std::format("{} buttons: {}", name, buttonsText(table.buttons)));
    lines.push_back(std::format("{} header: {}", name, joinedTexts(table.header, " | ")));
    for (std::size_t row = 0; row < table.rows.size(); ++row)
    {
        const GuiTableRow& cells = table.rows[row];
        std::string        text  = cells.configuration;
        for (const GuiValueCell& cell : cells.cells)
        {
            text += std::format(" | {}{}{}", cell.text, cell.shaded ? " (shaded)" : "",
                                cell.override ? " [override]" : "");
        }
        for (const GuiMotorCell& cell : cells.motorCells)
        {
            text += std::format(" | {} / {}{}", cell.motor, cell.ignition,
                                cell.ignitionDefault ? " (shaded)" : "");
        }
        lines.push_back(std::format("{} row {}: {}", name, row, text));
    }
    lines.push_back(std::format("{} selection: rows {} columns {}", name,
                                integersText(table.selectedRows),
                                integersText(table.selectedColumns)));
    if (motors)
    {
        lines.push_back(std::format("{} help: {}", name, table.showsHelp));
        std::vector<std::string> mounts;
        mounts.reserve(table.mounts.size());
        for (const GuiMountRow& mount : table.mounts)
        {
            mounts.push_back(std::format("{} '{}' {}", guiComponentPathText(mount.component),
                                         mount.text, mount.motorMount ? "[x]" : "[ ]"));
        }
        lines.push_back(std::format("{} mounts: {}", name, joinedTexts(mounts, ", ")));
    }
}

/// The lines of what the tab showed.
void addTabLines(const GuiFlightConfigurationState& state, std::vector<std::string>& lines)
{
    lines.push_back("configTab: " + state.configTab);
    std::vector<std::string> tabs;
    tabs.reserve(state.configTabs.size());
    for (const GuiConfigTabState& tab : state.configTabs)
    {
        tabs.push_back(
            std::format("{} '{}' {}", tab.tab, tab.title, tab.enabled ? "enabled" : "disabled"));
    }
    lines.push_back("configTabs: " + joinedTexts(tabs, ", "));
    lines.push_back("buttons: " + buttonsText(state.buttons));
    lines.push_back(std::format("selectedConfiguration: {}", state.selectedConfiguration));
    std::vector<std::string> simulations;
    simulations.reserve(state.simulations.size());
    for (const GuiSimulationRow& simulation : state.simulations)
    {
        simulations.push_back(std::format("'{}'={}", simulation.name, simulation.configuration));
    }
    lines.push_back("simulations: " + joinedTexts(simulations, ", "));
    addTableLines("motors", state.motors, true, lines);
    addTableLines("recovery", state.recovery, false, lines);
    addTableLines("stages", state.stages, false, lines);
}

/// @p paths as text, "[Sustainer/Body Tube, (rocket)]".
[[nodiscard]] std::string pathsText(const std::vector<GuiComponentPath>& paths)
{
    std::string text = "[";
    for (const GuiComponentPath& path : paths)
    {
        text += (text.size() > 1 ? ", " : "") + guiComponentPathText(path);
    }
    return text + "]";
}

/// The open window of @p record as text: "<window> <component>", or "none".
[[nodiscard]] std::string windowText(const GuiStepRecord& record)
{
    if (!record.window.has_value())
    {
        return "none";
    }
    return *record.window + (record.windowComponent.has_value()
                                 ? " " + guiComponentPathText(*record.windowComponent)
                                 : std::string());
}

/// An action's state as text: "'Undo (Add Nose Cone)' enabled".
[[nodiscard]] std::string actionText(const GuiActionState& action)
{
    return std::format("'{}' {}", action.text, action.enabled ? "enabled" : "disabled");
}

/// The tree's difference between @p expected and @p found, whose digests differ: the first line
/// that differs where @p expected has the lines, else @p found's tree.
[[nodiscard]] std::string treeDifference(const GuiStepRecord& expected, const GuiStepRecord& found)
{
    const std::vector<std::string>& lines = found.tree.value_or(std::vector<std::string>{});
    if (expected.tree.has_value())
    {
        const std::vector<std::string_view> wanted(expected.tree->begin(), expected.tree->end());
        return "tree: " + firstDifference(wanted, lines);
    }
    std::string tree = "tree (digest differs; QtRocket's):";
    for (const std::string& line : lines)
    {
        tree += "\n  " + line;
    }
    return tree;
}

}  // namespace

std::optional<std::string> guiQuestionMember(const GuiQuestion& question, std::string_view member)
{
    std::string GuiQuestion::* const found = questionMemberNamed(member);
    return found == nullptr ? std::nullopt : std::optional<std::string>{question.*found};
}

namespace
{

/// The rule of bug 1 for the cells of @p table, the table @p name of the record of step @p step:
/// a cell is shaded exactly when its configuration has no override of its own, as QtRocket's
/// table shades it; a cell OpenRocket shaded (which its renderer never does) is a line of
/// @p problems and stays.
void shadeValueCellsAsQtRocket(std::size_t step, std::string_view name,
                               GuiConfigurationTable& table, std::string& problems)
{
    for (std::size_t row = 0; row < table.rows.size(); ++row)
    {
        for (std::size_t column = 0; column < table.rows[row].cells.size(); ++column)
        {
            GuiValueCell& cell = table.rows[row].cells[column];
            if (cell.shaded)
            {
                problems += std::format(
                    "step {}: OpenRocket shaded the {} cell of row {}, column {} (bug 1: its "
                    "renderer shades none)\n",
                    step, name, row, column + 1);
                continue;
            }
            cell.shaded = !cell.override;
        }
    }
}

}  // namespace

GuiStepRecord guiExpectedRecord(std::string_view scenario, GuiStepRecord record,
                                std::string& problems)
{
    if (record.flightConfiguration.has_value())
    {
        shadeValueCellsAsQtRocket(record.index, "recovery", record.flightConfiguration->recovery,
                                  problems);
        shadeValueCellsAsQtRocket(record.index, "stages", record.flightConfiguration->stages,
                                  problems);
    }
    for (const GuiExpectedDifference& difference : kGuiExpectedDifferences)
    {
        if (difference.scenario != scenario || difference.step != record.index)
        {
            continue;
        }
        std::string GuiQuestion::* const member = questionMemberNamed(difference.member);
        if (difference.question >= record.questions.size() || member == nullptr ||
            record.questions[difference.question].*member != difference.openRocket)
        {
            problems += std::format(
                "the expected difference of step {} (bug {}) names no question {} with {} {}\n",
                difference.step, difference.bug, difference.question, difference.member,
                difference.openRocket);
            continue;
        }
        record.questions[difference.question].*member = std::string(difference.qtRocket);
    }
    return record;
}

std::vector<std::string> guiRecordLines(const GuiStepRecord& record)
{
    std::vector<std::string> lines;
    if (!record.tab.empty())
    {
        lines.push_back("tab: " + record.tab);
    }
    for (std::size_t at = 0; at < record.questions.size(); ++at)
    {
        addQuestionLines(at, record.questions[at], lines);
    }
    if (record.flightConfiguration.has_value())
    {
        addTabLines(*record.flightConfiguration, lines);
    }
    return lines;
}

std::vector<std::string> guiRecordDifferences(const GuiStepRecord& expected,
                                              const GuiStepRecord& found)
{
    std::vector<std::string> differences;
    const auto compare = [&differences](std::string_view what, const std::string& wanted,
                                        const std::string& got) {
        if (wanted != got)
        {
            differences.push_back(std::format("{}: OpenRocket {}, QtRocket {}", what, wanted, got));
        }
    };
    compare("undo", actionText(expected.undo), actionText(found.undo));
    compare("redo", actionText(expected.redo), actionText(found.redo));
    compare("saved", expected.saved ? "true" : "false", found.saved ? "true" : "false");
    compare("selection", pathsText(expected.selection), pathsText(found.selection));
    compare("window", windowText(expected), windowText(found));
    if (expected.treeSha256 != found.treeSha256)
    {
        differences.push_back(treeDifference(expected, found));
    }
    // The members of version 2, line for line.
    const std::vector<std::string> wanted = guiRecordLines(expected);
    const std::vector<std::string> got    = guiRecordLines(found);
    std::size_t                    named  = 0;
    std::size_t                    more   = 0;
    for (std::size_t at = 0; at < std::max(wanted.size(), got.size()); ++at)
    {
        const std::string  none  = "(no line)";
        const std::string& left  = at < wanted.size() ? wanted[at] : none;
        const std::string& right = at < got.size() ? got[at] : none;
        if (left == right)
        {
            continue;
        }
        if (named == kGuiRecordDifferenceLines)
        {
            ++more;
            continue;
        }
        ++named;
        differences.push_back(std::format("OpenRocket: {}\n    QtRocket: {}", left, right));
    }
    if (more > 0)
    {
        differences.push_back(std::format("... and {} more lines that differ", more));
    }
    return differences;
}

}  // namespace QtRocket::Test
