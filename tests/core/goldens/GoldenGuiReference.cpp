#include "goldens/GoldenGuiReference.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The schemas the tool writes, by version (Scenario.SCHEMA_V1 and SCHEMA_V2, GuiReference): the
/// schema names of version 2 are those of version 3 too.
constexpr std::string_view kScenarioSchemaV1  = "gui-from-scratch-scenario";
constexpr std::string_view kScenarioSchemaV2  = "gui-scenario";
constexpr std::string_view kReferenceSchemaV1 = "gui-from-scratch-reference";
constexpr std::string_view kReferenceSchemaV2 = "gui-reference";

/// The versions a step kind belongs to (bits).
constexpr unsigned kV1   = 1U;
constexpr unsigned kV2   = 2U;
constexpr unsigned kV3   = 4U;
constexpr unsigned kV2V3 = kV2 | kV3;
constexpr unsigned kAll  = kV1 | kV2 | kV3;

/// The bit of version @p version.
[[nodiscard]] constexpr unsigned versionBit(int version) noexcept
{
    if (version == 1)
    {
        return kV1;
    }
    return version == 2 ? kV2 : kV3;
}

/// A step kind, its name, the members it takes besides "step" and "note" (separated by blanks;
/// every one is required but "unit", New Configuration's "answer" and the answer of Delete and
/// Cut of simulations), as Scenario.membersOf() of the tool has them, and the versions it
/// belongs to (Scenario.V1_KINDS, V2_KINDS, V3_KINDS).
struct StepSpec
{
    GuiStepKind      kind{GuiStepKind::NEW};
    std::string_view name;
    std::string_view members;
    unsigned         versions{kAll};
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
    StepSpec{.kind = GuiStepKind::OPEN, .name = "open", .members = "file", .versions = kV2V3},
    StepSpec{.kind = GuiStepKind::TAB, .name = "tab", .members = "tab", .versions = kV2V3},
    StepSpec{
        .kind = GuiStepKind::CONFIG_TAB, .name = "configTab", .members = "tab", .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_CELLS,
             .name     = "selectCells",
             .members  = "cells",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::MOUNT_CHECK,
             .name     = "mountCheck",
             .members  = "mount checked",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::NEW_CONFIGURATION,
             .name     = "newConfiguration",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::RENAME_CONFIGURATION,
             .name     = "renameConfiguration",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::DELETE_CONFIGURATION,
             .name     = "deleteConfiguration",
             .members  = "",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::DUPLICATE_CONFIGURATION,
             .name     = "duplicateConfiguration",
             .members  = "",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_MOTOR,
             .name     = "selectMotor",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{
        .kind = GuiStepKind::DELETE_MOTOR, .name = "deleteMotor", .members = "", .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_IGNITION,
             .name     = "selectIgnition",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::RESET_IGNITION,
             .name     = "resetIgnition",
             .members  = "",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_DEPLOYMENT,
             .name     = "selectDeployment",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::RESET_DEPLOYMENT,
             .name     = "resetDeployment",
             .members  = "",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_SEPARATION,
             .name     = "selectSeparation",
             .members  = "answer",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::RESET_SEPARATION,
             .name     = "resetSeparation",
             .members  = "",
             .versions = kV2V3},
    StepSpec{.kind     = GuiStepKind::SELECT_SIMULATIONS,
             .name     = "selectSimulations",
             .members  = "rows",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::NEW_SIMULATION,
             .name     = "newSimulation",
             .members  = "answer",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::EDIT_SIMULATION,
             .name     = "editSimulation",
             .members  = "answer",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::DUPLICATE_SIMULATIONS,
             .name     = "duplicateSimulations",
             .members  = "",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::DELETE_SIMULATIONS,
             .name     = "deleteSimulations",
             .members  = "answer",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::COPY_SIMULATIONS,
             .name     = "copySimulations",
             .members  = "",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::CUT_SIMULATIONS,
             .name     = "cutSimulations",
             .members  = "answer",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::PASTE_SIMULATIONS,
             .name     = "pasteSimulations",
             .members  = "",
             .versions = kV3},
    StepSpec{.kind     = GuiStepKind::COLUMN_VISIBILITY,
             .name     = "columnVisibility",
             .members  = "column visible",
             .versions = kV3},
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
    QuestionSpec{.kind = GuiQuestionKind::SIMULATION, .name = "simulation"},
    QuestionSpec{.kind = GuiQuestionKind::RUN, .name = "run"},
    QuestionSpec{.kind = GuiQuestionKind::CONFIRM, .name = "confirm"},
    QuestionSpec{.kind = GuiQuestionKind::WIND_LEVELS, .name = "windLevels"},
    QuestionSpec{.kind = GuiQuestionKind::CSV_IMPORT_SETTINGS, .name = "csvImportSettings"},
    QuestionSpec{.kind = GuiQuestionKind::OPEN_FILE, .name = "openFile"},
    QuestionSpec{.kind = GuiQuestionKind::LOOKUP, .name = "lookup"},
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

    /// Fails unless @p object has the "schema" @p v1 of version 1 or @p v2 of version 2 or 3;
    /// the version (0 after a failure).
    int schemaVersion(const json& object, std::string_view v1, std::string_view v2)
    {
        const std::string schema  = string(object, "schema", "the document");
        const int         version = integer(object, "schemaVersion", "the document");
        if ((schema == v1 && version == 1) || (schema == v2 && (version == 2 || version == 3)))
        {
            return version;
        }
        failure("the document",
                std::format("neither a {} of version 1 nor a {} of version 2 or 3", v1, v2));
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
        case GuiQuestionKind::SIMULATION:
        case GuiQuestionKind::RUN:
        case GuiQuestionKind::CONFIRM:
        case GuiQuestionKind::WIND_LEVELS:
        case GuiQuestionKind::CSV_IMPORT_SETTINGS:
        case GuiQuestionKind::OPEN_FILE:
        case GuiQuestionKind::LOOKUP:
            reader.failure(context, "a question of the simulations tab has no answer of version 2");
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

// --------------------------------------------------------------- the answers of version 3
//
// The answers of the dialogs of the simulations tab, as the tool's SimulationQuestions takes them
// (its Questions.members() checks: the required members, and no member its dialog does not take).

/// The names of @p object's members that are not in @p allowed, and those of @p required it does
/// not have, as failures of @p reader ("the <what> has no member ..."); false when @p object is
/// no object.
bool checkObjectMembers(JsonReader& reader, const json& object,
                        std::span<const std::string_view> allowed,
                        std::span<const std::string_view> required, std::string_view what,
                        std::string_view context)
{
    if (!object.is_object())
    {
        reader.failure(context, std::format("the {} is not an object", what));
        return false;
    }
    for (const auto& [key, value] : object.items())
    {
        if (!contains(allowed, key))
        {
            reader.failure(context, std::format(R"(the {} has no member "{}")", what, key));
        }
    }
    for (const std::string_view key : required)
    {
        if (!object.contains(key))
        {
            reader.failure(context, std::format(R"(the {} needs "{}")", what, key));
        }
    }
    return true;
}

/// Fails unless @p value is one of @p values (the member @p member of the @p what).
void checkOneOf(JsonReader& reader, std::string_view value,
                std::span<const std::string_view> values, std::string_view member,
                std::string_view what, std::string_view context)
{
    if (!contains(values, value))
    {
        std::string names;
        for (const std::string_view name : values)
        {
            names += std::format("{}\"{}\"", names.empty() ? "" : ", ", name);
        }
        reader.failure(context, std::format(R"(the {}'s "{}" is one of {}, not "{}")", what, member,
                                            names, value));
    }
}

/// A member that must be true when present ("doNotAskAgain", "addLevel", "refresh", "clear").
[[nodiscard]] bool trueMember(JsonReader& reader, const json& object, std::string_view key,
                              std::string_view context)
{
    const bool value = reader.boolean(object, key, context);
    if (!value)
    {
        reader.failure(context, std::format(R"("{}" is true or left out)", key));
    }
    return value;
}

/// A JOptionPane question's answer: {"button": one of @p buttons, "doNotAskAgain": true (left
/// out: the box is left alone)} (SimulationQuestions.confirm()).
[[nodiscard]] GuiConfirmAnswer confirmAnswer(JsonReader& reader, const json& object,
                                             std::span<const std::string_view> buttons,
                                             std::string_view                  context)
{
    constexpr std::array<std::string_view, 2> kMembers{"button", "doNotAskAgain"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    GuiConfirmAnswer                          answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "confirmation's answer", context))
    {
        return answer;
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, buttons, "button", "confirmation's answer", context);
    if (JsonReader::optionalMember(object, "doNotAskAgain") != nullptr)
    {
        answer.doNotAskAgain = trueMember(reader, object, "doNotAskAgain", context);
    }
    return answer;
}

/// A file chooser's answer: {"file": a path of the repository, "button": "open"} or {"button":
/// "cancel"} (SimulationQuestions.openFile()).
[[nodiscard]] GuiFileAnswer fileAnswer(JsonReader& reader, const json& object,
                                       std::string_view context)
{
    constexpr std::array<std::string_view, 2> kMembers{"button", "file"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    constexpr std::array<std::string_view, 2> kButtons{"open", "cancel"};
    GuiFileAnswer                             answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "file chooser's answer", context))
    {
        return answer;
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, kButtons, "button", "file chooser's answer", context);
    const bool hasFile = object.contains("file");
    if (hasFile != (answer.button == "open"))
    {
        reader.failure(context, R"(a file chooser's "open" has a "file", its "cancel" none)");
    }
    if (hasFile)
    {
        answer.file = reader.string(object, "file", context);
        if (const Result<std::filesystem::path> file = guiRepositoryFile(answer.file); !file)
        {
            reader.failure(context, file.error().message);
        }
    }
    return answer;
}

/// A row of the CSV import settings: {"name", "index", "unit"}, each optional
/// (SimulationQuestions.csvColumn()).
[[nodiscard]] GuiCsvColumnAnswer csvColumnAnswer(JsonReader& reader, const json& object,
                                                 std::string_view row, std::string_view context)
{
    constexpr std::array<std::string_view, 3> kMembers{"name", "index", "unit"};
    GuiCsvColumnAnswer                        answer;
    answer.row = std::string(row);
    if (!checkObjectMembers(reader, object, kMembers, {}, "CSV import's row", context))
    {
        return answer;
    }
    answer.name = reader.optionalString(object, "name", context);
    if (JsonReader::optionalMember(object, "index") != nullptr)
    {
        answer.index = reader.integer(object, "index", context);
    }
    answer.unit = reader.optionalString(object, "unit", context);
    return answer;
}

/// The CSV import settings' answer: {"hasHeader", "columns": {<row>: ...}, "separator",
/// "button": "ok" | "cancel"} (SimulationQuestions.csvSettings()); the rows in the order of
/// kGuiCsvImportRows.
[[nodiscard]] GuiCsvImportAnswer csvImportAnswer(JsonReader& reader, const json& object,
                                                 std::string_view context)
{
    constexpr std::array<std::string_view, 4> kMembers{"hasHeader", "columns", "separator",
                                                       "button"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    constexpr std::array<std::string_view, 2> kButtons{"ok", "cancel"};
    GuiCsvImportAnswer                        answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "CSV import's answer", context))
    {
        return answer;
    }
    if (JsonReader::optionalMember(object, "hasHeader") != nullptr)
    {
        answer.hasHeader = reader.boolean(object, "hasHeader", context);
    }
    if (const json* const columns = JsonReader::optionalMember(object, "columns");
        columns != nullptr && checkObjectMembers(reader, *columns, kGuiCsvImportRows, {},
                                                 "CSV import's columns", context))
    {
        for (const std::string_view row : kGuiCsvImportRows)
        {
            if (columns->contains(row))
            {
                answer.columns.push_back(csvColumnAnswer(reader, columns->at(row), row, context));
            }
        }
    }
    answer.separator = reader.optionalString(object, "separator", context);
    if (answer.separator && answer.separator->empty())
    {
        reader.failure(context, "the CSV import's separator is not empty");
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, kButtons, "button", "CSV import's answer", context);
    return answer;
}

/// Whether the file name of @p path (a path of the repository) has the extension csv, in any
/// case (MultiLevelWindEditDialog.importLevels: OpenRocket appends ".csv" to another, bug 37).
[[nodiscard]] bool hasCsvExtension(std::string_view path)
{
    const std::size_t      slash = path.rfind('/');
    const std::string_view name  = slash == std::string_view::npos ? path : path.substr(slash + 1);
    const std::size_t      dot   = name.rfind('.');
    return dot != std::string_view::npos && dot > 0 &&
           Strings::toLower(name.substr(dot + 1)) == "csv";
}

/// "Import levels": {"settings", "file", "overwrite"} (SimulationQuestions.importLevels()); a
/// chosen file without the extension .csv is refused (bug 37).
[[nodiscard]] GuiWindImportAnswer windImportAnswer(JsonReader& reader, const json& object,
                                                   std::string_view context)
{
    constexpr std::array<std::string_view, 3> kMembers{"settings", "file", "overwrite"};
    constexpr std::array<std::string_view, 1> kRequired{"settings"};
    constexpr std::array<std::string_view, 2> kYesNo{"yes", "no"};
    GuiWindImportAnswer                       answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "wind import", context))
    {
        return answer;
    }
    answer.settings = csvImportAnswer(reader, reader.object(object, "settings", context), context);
    if (const json* const file = JsonReader::optionalMember(object, "file"))
    {
        answer.file = fileAnswer(reader, *file, context);
        if (answer.file->button == "open" && !hasCsvExtension(answer.file->file))
        {
            reader.failure(context, std::format("bug 37: OpenRocket appends .csv to the chosen "
                                                "file {}",
                                                answer.file->file));
        }
    }
    if (const json* const overwrite = JsonReader::optionalMember(object, "overwrite"))
    {
        answer.overwrite = confirmAnswer(reader, *overwrite, kYesNo, context);
    }
    return answer;
}

/// The names of an edit of the wind editor by kind (SimulationQuestions.windEdit()).
struct WindEditSpec
{
    GuiWindEditKind  kind{GuiWindEditKind::ADD_LEVEL};
    std::string_view name;
};

constexpr std::array kWindEditSpecs{
    WindEditSpec{.kind = GuiWindEditKind::ALTITUDE_REFERENCE, .name = "altitudeReference"},
    WindEditSpec{.kind = GuiWindEditKind::ADD_LEVEL, .name = "addLevel"},
    WindEditSpec{.kind = GuiWindEditKind::DELETE_LEVEL, .name = "deleteLevel"},
    WindEditSpec{.kind = GuiWindEditKind::DELETE_ROW, .name = "deleteRow"},
    WindEditSpec{.kind = GuiWindEditKind::CELL, .name = "cell"},
    WindEditSpec{.kind = GuiWindEditKind::COLUMN_UNIT, .name = "columnUnit"},
    WindEditSpec{.kind = GuiWindEditKind::RESET_LEVELS, .name = "resetLevels"},
    WindEditSpec{.kind = GuiWindEditKind::IMPORT_LEVELS, .name = "importLevels"},
};

/// The members of a wind edit's "cell" or "columnUnit": {"row", "column", "text"} or {"column",
/// "unit"}, the column one of kGuiWindColumns.
void readWindCell(JsonReader& reader, const json& object, GuiWindEdit& edit,
                  std::string_view context)
{
    constexpr std::array<std::string_view, 3> kCell{"row", "column", "text"};
    constexpr std::array<std::string_view, 2> kUnit{"column", "unit"};
    const bool                                cell = edit.kind == GuiWindEditKind::CELL;
    const std::span<const std::string_view>   members =
        cell ? std::span<const std::string_view>(kCell) : std::span<const std::string_view>(kUnit);
    if (!checkObjectMembers(reader, object, members, members,
                            cell ? "wind table's cell" : "wind table's column unit", context))
    {
        return;
    }
    edit.column = reader.string(object, "column", context);
    checkOneOf(reader, edit.column, kGuiWindColumns, "column", "wind table's edit", context);
    if (cell)
    {
        edit.row  = reader.integer(object, "row", context);
        edit.text = reader.string(object, "text", context);
    }
    else
    {
        edit.unit = reader.string(object, "unit", context);
    }
}

/// An edit of the wind editor: an object with exactly one member, of kWindEditSpecs.
[[nodiscard]] GuiWindEdit windEdit(JsonReader& reader, const json& object, std::string_view context)
{
    constexpr std::array<std::string_view, 2> kReferences{"MSL", "AGL"};
    constexpr std::array<std::string_view, 2> kYesNo{"yes", "no"};
    GuiWindEdit                               edit;
    if (!object.is_object() || object.size() != 1)
    {
        reader.failure(context, "an edit of the wind editor has exactly one member");
        return edit;
    }
    const std::string                   key = object.begin().key();
    const std::span<const WindEditSpec> specs(kWindEditSpecs);
    const auto found = std::ranges::find(specs, std::string_view(key), &WindEditSpec::name);
    if (found == specs.end())
    {
        reader.failure(context, std::format(R"(the wind editor has no edit "{}")", key));
        return edit;
    }
    edit.kind = found->kind;
    switch (edit.kind)
    {
        case GuiWindEditKind::ALTITUDE_REFERENCE:
            edit.altitudeReference = reader.string(object, key, context);
            checkOneOf(reader, edit.altitudeReference, kReferences, key, "wind edit", context);
            break;
        case GuiWindEditKind::ADD_LEVEL:
            static_cast<void>(trueMember(reader, object, key, context));
            break;
        case GuiWindEditKind::DELETE_LEVEL:
        case GuiWindEditKind::DELETE_ROW:
            edit.row = reader.integer(object, key, context);
            break;
        case GuiWindEditKind::CELL:
        case GuiWindEditKind::COLUMN_UNIT:
            readWindCell(reader, object.at(key), edit, context);
            break;
        case GuiWindEditKind::RESET_LEVELS:
            edit.resetQuestion = confirmAnswer(reader, object.at(key), kYesNo, context);
            break;
        case GuiWindEditKind::IMPORT_LEVELS:
            edit.importLevels = windImportAnswer(reader, object.at(key), context);
            break;
    }
    return edit;
}

/// The wind editor's answer: {"edits": [...], "button": "close"}
/// (SimulationQuestions.windEditor()).
[[nodiscard]] GuiWindEditorAnswer windEditorAnswer(JsonReader& reader, const json& object,
                                                   std::string_view context)
{
    constexpr std::array<std::string_view, 2> kMembers{"edits", "button"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    constexpr std::array<std::string_view, 1> kButtons{"close"};
    GuiWindEditorAnswer                       answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "wind editor's answer", context))
    {
        return answer;
    }
    if (JsonReader::optionalMember(object, "edits") != nullptr)
    {
        for (const json& edit : reader.array(object, "edits", context))
        {
            answer.edits.push_back(windEdit(reader, edit, context));
        }
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, kButtons, "button", "wind editor's answer", context);
    return answer;
}

/// The names of a lookup action by kind (SimulationQuestions.lookupAction()).
struct LookupActionSpec
{
    GuiLookupActionKind kind{GuiLookupActionKind::CLEAR};
    std::string_view    name;
};

constexpr std::array kLookupActionSpecs{
    LookupActionSpec{.kind = GuiLookupActionKind::SEPARATOR, .name = "separator"},
    LookupActionSpec{.kind = GuiLookupActionKind::LOAD, .name = "load"},
    LookupActionSpec{.kind = GuiLookupActionKind::TEXT, .name = "text"},
    LookupActionSpec{.kind = GuiLookupActionKind::REFRESH, .name = "refresh"},
    LookupActionSpec{.kind = GuiLookupActionKind::CLEAR, .name = "clear"},
};

/// An action of a section of the lookup dialog: an object with exactly one member, of
/// kLookupActionSpecs.
[[nodiscard]] GuiLookupAction lookupAction(JsonReader& reader, const json& object,
                                           std::string_view context)
{
    GuiLookupAction action;
    if (!object.is_object() || object.size() != 1)
    {
        reader.failure(context, "a lookup action has exactly one member");
        return action;
    }
    const std::string                       key = object.begin().key();
    const std::span<const LookupActionSpec> specs(kLookupActionSpecs);
    const auto found = std::ranges::find(specs, std::string_view(key), &LookupActionSpec::name);
    if (found == specs.end())
    {
        reader.failure(context, std::format(R"(the lookup dialog has no action "{}")", key));
        return action;
    }
    action.kind = found->kind;
    switch (action.kind)
    {
        case GuiLookupActionKind::SEPARATOR:
        case GuiLookupActionKind::TEXT:
            action.text = reader.string(object, key, context);
            break;
        case GuiLookupActionKind::LOAD:
            action.file = fileAnswer(reader, object.at(key), context);
            break;
        case GuiLookupActionKind::REFRESH:
        case GuiLookupActionKind::CLEAR:
            static_cast<void>(trueMember(reader, object, key, context));
            break;
    }
    return action;
}

/// The lookup dialog's answer: {"drag": [...], "stability": [...], "button": "ok" | "cancel"}
/// (SimulationQuestions.lookup()).
[[nodiscard]] GuiLookupAnswer lookupAnswer(JsonReader& reader, const json& object,
                                           std::string_view context)
{
    constexpr std::array<std::string_view, 3> kMembers{"drag", "stability", "button"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    constexpr std::array<std::string_view, 2> kButtons{"ok", "cancel"};
    GuiLookupAnswer                           answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "lookup dialog's answer", context))
    {
        return answer;
    }
    for (const auto& [name, section] :
         {std::pair{std::string_view("drag"), &answer.drag},
          std::pair{std::string_view("stability"), &answer.stability}})
    {
        if (JsonReader::optionalMember(object, name) == nullptr)
        {
            continue;
        }
        std::vector<GuiLookupAction> actions;
        for (const json& action : reader.array(object, name, context))
        {
            actions.push_back(lookupAction(reader, action, context));
        }
        *section = std::move(actions);
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, kButtons, "button", "lookup dialog's answer", context);
    return answer;
}

/// A field edit: {"field": a property of kGuiSettingsFields, and the members of its kind}
/// (SimulationQuestions.fieldEdit()).
void readFieldEdit(JsonReader& reader, const json& object, GuiSettingsEdit& edit,
                   std::string_view context)
{
    edit.field                         = reader.string(object, "field", context);
    const GuiSettingsField* const spec = guiSettingsField(edit.field);
    if (spec == nullptr)
    {
        reader.failure(context, std::format("the settings dialog has no field {}", edit.field));
        return;
    }
    constexpr std::array<std::string_view, 3> kSpinner{"field", "text", "unit"};
    constexpr std::array<std::string_view, 2> kSpinnerRequired{"field", "text"};
    constexpr std::array<std::string_view, 2> kCheckBox{"field", "checked"};
    constexpr std::array<std::string_view, 2> kComboBox{"field", "item"};
    switch (spec->kind)
    {
        case GuiSettingsFieldKind::SPINNER:
            checkObjectMembers(reader, object, kSpinner, kSpinnerRequired, "spinner's edit",
                               context);
            edit.text = reader.string(object, "text", context);
            edit.unit = reader.optionalString(object, "unit", context);
            return;
        case GuiSettingsFieldKind::CHECK_BOX:
            checkObjectMembers(reader, object, kCheckBox, kCheckBox, "check box's edit", context);
            edit.checked = reader.boolean(object, "checked", context);
            return;
        case GuiSettingsFieldKind::COMBO_BOX:
        {
            checkObjectMembers(reader, object, kComboBox, kComboBox, "combo box's edit", context);
            edit.item                            = reader.string(object, "item", context);
            const std::vector<std::string> items = Strings::split(spec->items, ' ');
            if (std::ranges::find(items, *edit.item) == items.end())
            {
                reader.failure(context, std::format("the combo box {} has no item {}", edit.field,
                                                    *edit.item));
            }
            return;
        }
    }
}

/// The edit "randomSeed": {"fixed": true, "text": <an integer>} or {"fixed": false}
/// (SimulationQuestions.randomSeedEdit()).
void readRandomSeed(JsonReader& reader, const json& object, GuiSettingsEdit& edit,
                    std::string_view context)
{
    constexpr std::array<std::string_view, 2> kMembers{"fixed", "text"};
    constexpr std::array<std::string_view, 1> kRequired{"fixed"};
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "random seed", context))
    {
        return;
    }
    edit.seedFixed = reader.boolean(object, "fixed", context);
    edit.seedText  = reader.optionalString(object, "text", context);
    if (edit.seedFixed != edit.seedText.has_value())
    {
        reader.failure(context,
                       "a fixed seed has its text (the box draws a random one), one that "
                       "is not fixed none");
    }
    // Integer.parseInt(text.trim()).
    if (edit.seedText && !Strings::parseInt(Strings::trim(*edit.seedText)).has_value())
    {
        reader.failure(context, std::format("the seed '{}' is no integer", *edit.seedText));
    }
}

/// The kinds of the settings dialog's edits by the member that names them, in the order the tool
/// tells them apart (SimulationQuestions.settingsEdit()).
struct SettingsEditSpec
{
    GuiSettingsEditKind kind{GuiSettingsEditKind::NAME};
    std::string_view    name;
};

constexpr std::array kSettingsEditSpecs{
    SettingsEditSpec{.kind = GuiSettingsEditKind::FIELD, .name = "field"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::NAME, .name = "name"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::CONFIGURATION, .name = "configuration"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::TAB, .name = "tab"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::WIND_MODEL, .name = "windModel"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::RANDOM_SEED, .name = "randomSeed"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::DEFAULTS, .name = "defaults"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::WIND_LEVELS, .name = "windLevels"},
    SettingsEditSpec{.kind = GuiSettingsEditKind::LOOKUP, .name = "lookup"},
};

/// The members of an edit that is not a field's, of kind @p edit.kind.
void readSettingsEditMembers(JsonReader& reader, const json& object, GuiSettingsEdit& edit,
                             std::string_view key, std::string_view context)
{
    constexpr std::array<std::string_view, 3> kPages{"launchConditions", "simulationOptions",
                                                     "warnings"};
    constexpr std::array<std::string_view, 2> kWindModels{"AVERAGE", "MULTI_LEVEL"};
    constexpr std::array<std::string_view, 2> kDefaults{"reset", "save"};
    constexpr std::array<std::string_view, 2> kDefaultsMembers{"defaults", "panel"};
    const std::array<std::string_view, 1>     single{key};
    if (edit.kind == GuiSettingsEditKind::DEFAULTS)
    {
        checkObjectMembers(reader, object, kDefaultsMembers, kDefaultsMembers, "defaults edit",
                           context);
    }
    else
    {
        checkObjectMembers(reader, object, single, single, "settings edit", context);
    }
    switch (edit.kind)
    {
        case GuiSettingsEditKind::NAME:
            edit.name = reader.string(object, key, context);
            if (edit.name.empty())
            {
                reader.failure(context, "an empty name leaves the simulation's name as it is");
            }
            break;
        case GuiSettingsEditKind::CONFIGURATION:
            edit.configuration = reader.integer(object, key, context);
            if (edit.configuration < 0)
            {
                reader.failure(context, "the configuration is a row, from 0");
            }
            break;
        case GuiSettingsEditKind::TAB:
            edit.tab = reader.string(object, key, context);
            checkOneOf(reader, edit.tab, kPages, key, "settings edit", context);
            break;
        case GuiSettingsEditKind::WIND_MODEL:
            edit.windModel = reader.string(object, key, context);
            checkOneOf(reader, edit.windModel, kWindModels, key, "settings edit", context);
            break;
        case GuiSettingsEditKind::RANDOM_SEED:
            readRandomSeed(reader, reader.object(object, key, context), edit, context);
            break;
        case GuiSettingsEditKind::DEFAULTS:
            edit.defaults = reader.string(object, "defaults", context);
            checkOneOf(reader, edit.defaults, kDefaults, "defaults", "settings edit", context);
            edit.tab = reader.string(object, "panel", context);
            checkOneOf(reader, edit.tab, kPages, "panel", "settings edit", context);
            break;
        case GuiSettingsEditKind::WIND_LEVELS:
            edit.windLevels =
                windEditorAnswer(reader, reader.object(object, key, context), context);
            break;
        case GuiSettingsEditKind::LOOKUP:
            edit.lookup = lookupAnswer(reader, reader.object(object, key, context), context);
            break;
        case GuiSettingsEditKind::FIELD:
            break;
    }
}

/// An edit of the settings dialog: exactly one of the members of kSettingsEditSpecs ("field"
/// with the members of its field, "defaults" with "panel").
[[nodiscard]] GuiSettingsEdit settingsEdit(JsonReader& reader, const json& object,
                                           std::string_view context)
{
    GuiSettingsEdit edit;
    if (!object.is_object())
    {
        reader.failure(context, "an edit of the settings dialog is not an object");
        return edit;
    }
    const std::span<const SettingsEditSpec> specs(kSettingsEditSpecs);
    const auto                              found = std::ranges::find_if(
        specs, [&object](const SettingsEditSpec& spec) { return object.contains(spec.name); });
    if (found == specs.end())
    {
        reader.failure(context, "an edit of the settings dialog the tool does not know");
        return edit;
    }
    edit.kind = found->kind;
    if (edit.kind == GuiSettingsEditKind::FIELD)
    {
        readFieldEdit(reader, object, edit, context);
    }
    else
    {
        readSettingsEditMembers(reader, object, edit, found->name, context);
    }
    return edit;
}

/// The settings dialog's answer: {"edits": [...], "button": "ok" | "cancel", "run": {"button":
/// "cancel"} (OK only), "discard": {"button": "yes", "doNotAskAgain"} (Cancel only)}
/// (SimulationQuestions.settings(), ok(), cancel(), run()).
[[nodiscard]] GuiSettingsAnswer settingsAnswer(JsonReader& reader, const json& object,
                                               std::string_view context)
{
    constexpr std::array<std::string_view, 4> kMembers{"edits", "button", "run", "discard"};
    constexpr std::array<std::string_view, 1> kRequired{"button"};
    constexpr std::array<std::string_view, 2> kButtons{"ok", "cancel"};
    constexpr std::array<std::string_view, 1> kRunMembers{"button"};
    constexpr std::array<std::string_view, 1> kRunButtons{"cancel"};
    constexpr std::array<std::string_view, 1> kYes{"yes"};
    GuiSettingsAnswer                         answer;
    if (!checkObjectMembers(reader, object, kMembers, kRequired, "settings dialog's answer",
                            context))
    {
        return answer;
    }
    if (JsonReader::optionalMember(object, "edits") != nullptr)
    {
        for (const json& edit : reader.array(object, "edits", context))
        {
            answer.edits.push_back(settingsEdit(reader, edit, context));
        }
    }
    answer.button = reader.string(object, "button", context);
    checkOneOf(reader, answer.button, kButtons, "button", "settings dialog's answer", context);
    if (const json* const run = JsonReader::optionalMember(object, "run");
        run != nullptr &&
        checkObjectMembers(reader, *run, kRunMembers, kRunMembers, "run dialog's answer", context))
    {
        answer.run = reader.string(*run, "button", context);
        checkOneOf(reader, *answer.run, kRunButtons, "button", "run dialog's answer", context);
    }
    if (const json* const discard = JsonReader::optionalMember(object, "discard"))
    {
        answer.discard = confirmAnswer(reader, *discard, kYes, context);
    }
    if ((answer.button == "ok" && answer.discard) || (answer.button == "cancel" && answer.run))
    {
        reader.failure(context, R"(OK asks no discard question, and Cancel runs nothing)");
    }
    return answer;
}

/// The answer of the step @p object of kind @p kind of the simulations tab into @p step: the
/// settings dialog's (New, Edit; required) or the delete confirmation's (Delete, Cut; left out
/// once its "Do not ask me again" was ticked).
void readSimulationAnswer(JsonReader& reader, const json& object, std::string_view context,
                          GuiScenarioStep& step)
{
    constexpr std::array<std::string_view, 2> kOkCancel{"ok", "cancel"};
    if (step.kind == GuiStepKind::NEW_SIMULATION || step.kind == GuiStepKind::EDIT_SIMULATION)
    {
        if (const json* const answer = reader.member(object, "answer", context))
        {
            step.settings = settingsAnswer(reader, *answer, context);
        }
        return;
    }
    if (const json* const answer = JsonReader::optionalMember(object, "answer"))
    {
        step.confirm = confirmAnswer(reader, *answer, kOkCancel, context);
    }
}

/// The rows of a selectSimulations step: [{"row", "with"}] (Scenario.simulationRow()).
[[nodiscard]] std::vector<GuiRowClick> rowClicks(JsonReader& reader, const json& object,
                                                 std::string_view context)
{
    constexpr std::array<std::string_view, 2> kMembers{"row", "with"};
    constexpr std::array<std::string_view, 1> kRequired{"row"};
    constexpr std::array<std::string_view, 2> kKeys{"ctrl", "shift"};
    std::vector<GuiRowClick>                  rows;
    for (const json& row : reader.array(object, "rows", context))
    {
        if (!checkObjectMembers(reader, row, kMembers, kRequired, "row", context))
        {
            continue;
        }
        GuiRowClick click{.row  = reader.integer(row, "row", context),
                          .with = reader.optionalString(row, "with", context).value_or("")};
        if (!click.with.empty())
        {
            checkOneOf(reader, click.with, kKeys, "with", "row", context);
        }
        rows.push_back(std::move(click));
    }
    return rows;
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

/// The window's tabs of a scenario of version @p version (2 or 3).
[[nodiscard]] std::span<const std::string_view> windowTabs(int version) noexcept
{
    static constexpr std::array<std::string_view, 3> kWindowTabs{"design", "flightConfiguration",
                                                                 "simulations"};
    return std::span<const std::string_view>(kWindowTabs).first(version == 3 ? 3 : 2);
}

/// The tab of a tab or configTab step of version @p version, one of its tabs.
[[nodiscard]] std::string tabOf(JsonReader& reader, const json& object, GuiStepKind kind,
                                int version, std::string_view context)
{
    constexpr std::array<std::string_view, 3> kConfigTabs{"motors", "recovery", "stages"};
    std::string                               tab = reader.string(object, "tab", context);
    const std::span<const std::string_view>   tabs =
        kind == GuiStepKind::TAB ? windowTabs(version)
                                 : std::span<const std::string_view>(kConfigTabs);
    if (!contains(tabs, tab))
    {
        reader.failure(context, std::format(R"(no tab "{}")", tab));
    }
    return tab;
}

/// Reads the member @p member of the step @p object of a scenario of version @p version into
/// @p step.
void readStepMember(JsonReader& reader, const json& object, std::string_view member, int version,
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
        step.tab = tabOf(reader, object, step.kind, version, context);
    }
    else if (member == "rows")
    {
        step.rows = rowClicks(reader, object, context);
    }
    else if (member == "visible")
    {
        step.visible = reader.boolean(object, member, context);
    }
    else if (member == "answer" && isGuiSimulationStep(step.kind))
    {
        readSimulationAnswer(reader, object, context, step);
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
        if (step.kind == GuiStepKind::COLUMN_VISIBILITY && member == "column")
        {
            checkOneOf(reader, step.column, kGuiSimulationColumns, member, "column visibility",
                       context);
        }
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
    if ((spec->versions & versionBit(version)) == 0U)
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
            readStepMember(reader, object, member, version, context, step);
        }
    }
    return step;
}

/// Fails unless the scenario starts with File > New (version 1) or File > Open (versions 2 and 3,
/// and only there).
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

/// A tab of a record ({"tab", "title", "enabled"}): a sub-tab of the Motors & Configuration tab,
/// or a tab of the settings dialog.
[[nodiscard]] GuiConfigTabState tabState(JsonReader& reader, const json& object,
                                         std::string_view context)
{
    return {.tab     = reader.string(object, "tab", context),
            .title   = reader.string(object, "title", context),
            .enabled = reader.boolean(object, "enabled", context)};
}

/// What the settings dialog showed (SimulationQuestions.settingsState()): "multiSimEdit" left out
/// (null) in a single edit.
[[nodiscard]] GuiSettingsDialogState settingsDialogState(JsonReader& reader, const json& object,
                                                         std::string_view context)
{
    GuiSettingsDialogState state;
    state.title         = reader.string(object, "title", context);
    state.name          = reader.string(object, "name", context);
    state.configuration = reader.string(object, "configuration", context);
    state.status        = reader.string(object, "status", context);
    state.tab           = reader.string(object, "tab", context);
    for (const json& tab : reader.array(object, "tabs", context))
    {
        state.tabs.push_back(tabState(reader, tab, context));
    }
    state.multiSimEdit = reader.optionalString(object, "multiSimEdit", context);
    return state;
}

/// What the wind editor showed (SimulationQuestions.windState()).
[[nodiscard]] GuiWindEditorState windEditorState(JsonReader& reader, const json& object,
                                                 std::string_view context)
{
    return {.title             = reader.string(object, "title", context),
            .altitudeReference = reader.string(object, "altitudeReference", context),
            .lines             = reader.strings(object, "lines", context)};
}

/// What the CSV import settings showed (SimulationQuestions.csvState()).
[[nodiscard]] GuiCsvImportState csvImportState(JsonReader& reader, const json& object,
                                               std::string_view context)
{
    GuiCsvImportState state;
    state.title         = reader.string(object, "title", context);
    state.hasHeader     = reader.boolean(object, "hasHeader", context);
    const json& columns = reader.object(object, "columns", context);
    for (std::size_t at = 0; at < kGuiCsvImportRows.size(); ++at)
    {
        const json& column   = reader.object(columns, kGuiCsvImportRows.at(at), context);
        state.columns.at(at) = {.name  = reader.string(column, "name", context),
                                .index = reader.integer(column, "index", context),
                                .unit  = reader.string(column, "unit", context)};
    }
    state.separator = reader.string(object, "separator", context);
    return state;
}

/// A section of the lookup dialog (SimulationQuestions.lookupState()): "summaryToolTip" left out
/// (null) without a tool tip.
[[nodiscard]] GuiLookupSectionState lookupSectionState(JsonReader& reader, const json& object,
                                                       std::string_view context)
{
    return {.summary        = reader.string(object, "summary", context),
            .summaryToolTip = reader.optionalString(object, "summaryToolTip", context),
            .border         = reader.string(object, "border", context),
            .text           = reader.string(object, "text", context),
            .editable       = reader.boolean(object, "editable", context),
            .separator      = reader.string(object, "separator", context),
            .clearEnabled   = reader.boolean(object, "clearEnabled", context),
            .refreshEnabled = reader.boolean(object, "refreshEnabled", context)};
}

/// What the lookup dialog showed.
[[nodiscard]] GuiLookupState lookupState(JsonReader& reader, const json& object,
                                         std::string_view context)
{
    return {.title = reader.string(object, "title", context),
            .tab   = reader.string(object, "tab", context),
            .drag  = lookupSectionState(reader, reader.object(object, "drag", context), context),
            .stability =
                lookupSectionState(reader, reader.object(object, "stability", context), context)};
}

/// The members of a question of the simulations tab (version 3), of kind @p question.kind.
void readSimulationQuestion(JsonReader& reader, const json& object, std::string_view context,
                            GuiQuestion& question)
{
    switch (question.kind)
    {
        case GuiQuestionKind::SIMULATION:
            question.settingsShown = settingsDialogState(reader, object, context);
            question.settingsAnswered =
                settingsDialogState(reader, reader.object(object, "answered", context), context);
            break;
        case GuiQuestionKind::RUN:
            question.simulations = reader.strings(object, "simulations", context);
            break;
        case GuiQuestionKind::CONFIRM:
            question.message    = reader.strings(object, "message", context);
            question.checkBoxes = reader.strings(object, "checkBoxes", context);
            question.buttons    = reader.strings(object, "buttons", context);
            break;
        case GuiQuestionKind::WIND_LEVELS:
            question.windShown = windEditorState(reader, object, context);
            question.windAnswered =
                windEditorState(reader, reader.object(object, "answered", context), context);
            break;
        case GuiQuestionKind::CSV_IMPORT_SETTINGS:
            question.csvShown = csvImportState(reader, object, context);
            question.csvAnswered =
                csvImportState(reader, reader.object(object, "answered", context), context);
            break;
        case GuiQuestionKind::OPEN_FILE:
            question.file = reader.optionalString(object, "file", context);
            break;
        case GuiQuestionKind::LOOKUP:
            question.lookupShown = lookupState(reader, object, context);
            question.lookupAnswered =
                lookupState(reader, reader.object(object, "answered", context), context);
            break;
        default:
            break;
    }
}

/// Whether @p kind is a question of the simulations tab (version 3).
[[nodiscard]] bool isSimulationQuestion(GuiQuestionKind kind) noexcept
{
    return kind >= GuiQuestionKind::SIMULATION;
}

/// A row of the simulations table (SimulationsTab.row()): its cells' nulls are no value.
[[nodiscard]] GuiSimulationTableRow simulationTableRow(JsonReader& reader, const json& object,
                                                       std::string_view context)
{
    GuiSimulationTableRow row;
    row.status                    = reader.string(object, "status", context);
    row.statusSortText            = reader.string(object, "statusSortText", context);
    row.description               = reader.string(object, "description", context);
    row.warnings                  = reader.string(object, "warnings", context);
    const std::vector<int> counts = reader.integers(object, "warningCounts", context);
    if (counts.size() != row.warningCounts.size())
    {
        reader.failure(context, R"("warningCounts" has three counts)");
    }
    for (std::size_t at = 0; at < counts.size() && at < row.warningCounts.size(); ++at)
    {
        row.warningCounts.at(at) = counts[at];
    }
    row.warningsToolTip = reader.string(object, "warningsToolTip", context);
    for (const json& cell : reader.array(object, "cells", context))
    {
        if (cell.is_null())
        {
            row.cells.emplace_back(std::nullopt);
        }
        else if (cell.is_string())
        {
            row.cells.emplace_back(cell.get<std::string>());
        }
        else
        {
            reader.failure(context, "a cell is a string or null");
        }
    }
    return row;
}

/// What the simulations tab showed (SimulationsTab.record()).
[[nodiscard]] GuiSimulationsTabState simulationsTabState(JsonReader& reader, const json& object,
                                                         std::string_view context)
{
    GuiSimulationsTabState state;
    state.showsHelp = reader.boolean(object, "showsHelp", context);
    for (const json& column : reader.array(object, "columns", context))
    {
        state.columns.push_back({.id     = reader.string(column, "id", context),
                                 .header = reader.string(column, "header", context)});
    }
    for (const json& row : reader.array(object, "rows", context))
    {
        state.rows.push_back(simulationTableRow(reader, row, context));
    }
    state.selectedRows         = reader.integers(object, "selectedRows", context);
    state.actions              = reader.booleans(object, "actions", context);
    state.clipboardSimulations = reader.strings(object, "clipboardSimulations", context);
    state.hiddenColumnsPreference =
        reader.optionalString(object, "hiddenColumnsPreference", context);
    return state;
}

/// The members of a record of version 3 only: the simulations tab, the simulations, the tool's
/// fixes and the clipboard's text (the last two left out when there are none).
void readSimulationsRecord(JsonReader& reader, const json& object, std::string_view context,
                           GuiStepRecord& record)
{
    record.simulationsTab =
        simulationsTabState(reader, reader.object(object, "simulationsTab", context), context);
    for (const json& simulation : reader.array(object, "simulations", context))
    {
        record.simulations.push_back(
            {.name          = reader.string(simulation, "name", context),
             .configuration = reader.integer(simulation, "configuration", context),
             .status        = reader.string(simulation, "status", context),
             .options       = reader.strings(simulation, "options", context),
             .extensions    = reader.strings(simulation, "extensions", context)});
    }
    if (JsonReader::optionalMember(object, "toolFixes") != nullptr)
    {
        for (const json& fix : reader.array(object, "toolFixes", context))
        {
            GuiRecordedToolFix recorded;
            if (JsonReader::optionalMember(fix, "bug") != nullptr)
            {
                recorded.bug = reader.integer(fix, "bug", context);
            }
            recorded.fix = reader.string(fix, "fix", context);
            record.toolFixes.push_back(std::move(recorded));
        }
    }
    record.clipboardText = reader.optionalString(object, "clipboardText", context);
}

/// A question of a record of version @p version.
[[nodiscard]] GuiQuestion questionOf(JsonReader& reader, const json& object, int version,
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
    if (isSimulationQuestion(question.kind))
    {
        if (version != 3)
        {
            reader.failure(context, "a question of the simulations tab before version 3");
        }
        readSimulationQuestion(reader, object, context, question);
        return question;
    }
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
        default:
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

/// The members of a record of version 2 or 3 (@p version): the tab, the questions, what the
/// Motors & Configuration tab showed.
void readTabRecord(JsonReader& reader, const json& object, int version, std::string_view context,
                   GuiStepRecord& record)
{
    record.tab = reader.string(object, "tab", context);
    if (!contains(windowTabs(version), record.tab))
    {
        reader.failure(context, std::format(R"(no tab "{}")", record.tab));
    }
    if (JsonReader::optionalMember(object, "questions") != nullptr)
    {
        for (const json& question : reader.array(object, "questions", context))
        {
            record.questions.push_back(questionOf(reader, question, version, context));
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
    if (version >= 2)
    {
        readTabRecord(reader, object, version, context, record);
    }
    if (version == 3)
    {
        readSimulationsRecord(reader, object, context, record);
    }
    return record;
}

/// The members of the references of versions 2 and 3 only: the preset commit, the start-up, the
/// input and the archive's digests.
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

bool isGuiSimulationStep(GuiStepKind kind) noexcept
{
    return kind >= GuiStepKind::SELECT_SIMULATIONS && kind <= GuiStepKind::COLUMN_VISIBILITY;
}

const GuiSettingsField* guiSettingsField(std::string_view property) noexcept
{
    const std::span<const GuiSettingsField> fields(kGuiSettingsFields);
    const auto found = std::ranges::find(fields, property, &GuiSettingsField::property);
    return found == fields.end() ? nullptr : &*found;
}

namespace
{

/// The file of @p answer when it opens one.
void addAnsweredFile(const GuiFileAnswer& answer, std::vector<std::string>& files)
{
    if (answer.button == "open")
    {
        files.push_back(answer.file);
    }
}

/// The files the edits of @p answer name.
void addSettingsFiles(const GuiSettingsAnswer& answer, std::vector<std::string>& files)
{
    for (const GuiSettingsEdit& edit : answer.edits)
    {
        for (const GuiWindEdit& wind : edit.windLevels.edits)
        {
            if (wind.kind == GuiWindEditKind::IMPORT_LEVELS && wind.importLevels.file)
            {
                addAnsweredFile(*wind.importLevels.file, files);
            }
        }
        for (const std::optional<std::vector<GuiLookupAction>>* const section :
             {&edit.lookup.drag, &edit.lookup.stability})
        {
            for (const GuiLookupAction& action : section->value_or(std::vector<GuiLookupAction>{}))
            {
                if (action.kind == GuiLookupActionKind::LOAD)
                {
                    addAnsweredFile(action.file, files);
                }
            }
        }
    }
}

/// The questions the wind editor's answer @p answer asks, after the editor's own.
void addWindQuestions(const GuiWindEditorAnswer& answer, std::vector<GuiQuestionKind>& questions)
{
    for (const GuiWindEdit& edit : answer.edits)
    {
        if (edit.kind == GuiWindEditKind::RESET_LEVELS)
        {
            questions.push_back(GuiQuestionKind::CONFIRM);
        }
        else if (edit.kind == GuiWindEditKind::IMPORT_LEVELS)
        {
            questions.push_back(GuiQuestionKind::CSV_IMPORT_SETTINGS);
            if (edit.importLevels.file)
            {
                questions.push_back(GuiQuestionKind::OPEN_FILE);
            }
            if (edit.importLevels.overwrite)
            {
                questions.push_back(GuiQuestionKind::CONFIRM);
            }
        }
    }
}

/// The questions the lookup dialog's answer @p answer asks, after the dialog's own: a file
/// chooser per "Load from file...".
void addLookupQuestions(const GuiLookupAnswer& answer, std::vector<GuiQuestionKind>& questions)
{
    for (const std::optional<std::vector<GuiLookupAction>>* const section :
         {&answer.drag, &answer.stability})
    {
        for (const GuiLookupAction& action : section->value_or(std::vector<GuiLookupAction>{}))
        {
            if (action.kind == GuiLookupActionKind::LOAD)
            {
                questions.push_back(GuiQuestionKind::OPEN_FILE);
            }
        }
    }
}

/// Java's @p text.split(", "): the parts between the separators ", ", without the empty parts at
/// the end ("" gives one empty part).
[[nodiscard]] std::vector<std::string> javaSplitAtCommaSpace(std::string_view text)
{
    constexpr std::string_view kSeparator = ", ";
    std::vector<std::string>   parts;
    std::size_t                from = 0;
    for (std::size_t at = text.find(kSeparator); at != std::string_view::npos;
         at             = text.find(kSeparator, from))
    {
        parts.emplace_back(text.substr(from, at - from));
        from = at + kSeparator.size();
    }
    if (parts.empty())
    {
        return {std::string(text)};
    }
    parts.emplace_back(text.substr(from));
    while (!parts.empty() && parts.back().empty())
    {
        parts.pop_back();
    }
    return parts;
}

}  // namespace

std::vector<std::string> guiScenarioAnswerFiles(const GuiScenario& scenario)
{
    std::vector<std::string> files;
    for (const GuiScenarioStep& step : scenario.steps)
    {
        if (step.settings)
        {
            addSettingsFiles(*step.settings, files);
        }
    }
    return files;
}

std::vector<GuiQuestionKind> guiSimulationStepQuestions(const GuiScenarioStep& step)
{
    std::vector<GuiQuestionKind> questions;
    if (step.confirm)
    {
        questions.push_back(GuiQuestionKind::CONFIRM);
    }
    if (!step.settings)
    {
        return questions;
    }
    questions.push_back(GuiQuestionKind::SIMULATION);
    for (const GuiSettingsEdit& edit : step.settings->edits)
    {
        if (edit.kind == GuiSettingsEditKind::WIND_LEVELS)
        {
            questions.push_back(GuiQuestionKind::WIND_LEVELS);
            addWindQuestions(edit.windLevels, questions);
        }
        else if (edit.kind == GuiSettingsEditKind::LOOKUP)
        {
            questions.push_back(GuiQuestionKind::LOOKUP);
            addLookupQuestions(edit.lookup, questions);
        }
    }
    if (step.settings->run)
    {
        questions.push_back(GuiQuestionKind::RUN);
    }
    if (step.settings->discard)
    {
        questions.push_back(GuiQuestionKind::CONFIRM);
    }
    return questions;
}

std::vector<std::string> guiRelativeLookupPaths(std::vector<std::string> lines)
{
    // data/ is a directory of the repository's root (guiRepositoryFile()).
    const std::u8string root   = dataDir().parent_path().generic_u8string();
    const std::string   prefix = "path:" + std::string(root.begin(), root.end()) + "/";
    for (std::string& line : lines)
    {
        for (std::size_t at = line.find(prefix); at != std::string::npos;
             at             = line.find(prefix, at + 1))
        {
            line.replace(at, prefix.size(), "path:");
        }
    }
    return lines;
}

std::vector<GuiSimulationState> guiSimulationStates(OpenRocketDocument& document)
{
    std::vector<GuiSimulationState>          states;
    const std::vector<FlightConfigurationId> ids = document.getRocket().getIds();
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        const auto         id = std::ranges::find(ids, simulation->getFlightConfigurationId());
        GuiSimulationState state;
        state.name          = simulation->getName();
        state.configuration = id == ids.end() ? -1 : static_cast<int>(id - ids.begin());
        state.status        = std::string(name(simulation->getStatus()));
        state.options       = guiRelativeLookupPaths(describeConditions(simulation->getOptions()));
        for (const std::shared_ptr<SimulationExtension>& extension :
             simulation->getSimulationExtensions())
        {
            state.extensions.push_back(extension->getName());
        }
        states.push_back(std::move(state));
    }
    return states;
}

std::string guiSortedLookupColumns(std::string_view text)
{
    // SimulationQuestions.SUMMARY_COLUMNS: "columns: ([^<(]*?)(?= <i>\(modified\)</i>|
    // \(modified\)|</html>|$)", its first match; a list ends where one of the endings starts, and
    // a '<' or '(' before an ending makes that start fail (the search goes on after it).
    static constexpr std::string_view                kStart = "columns: ";
    static constexpr std::array<std::string_view, 3> kEndings{" <i>(modified)</i>", " (modified)",
                                                              "</html>"};
    for (std::size_t start = text.find(kStart); start != std::string_view::npos;
         start             = text.find(kStart, start + 1))
    {
        const std::size_t first    = start + kStart.size();
        std::size_t       end      = first;
        const auto        endsHere = [&text](std::size_t at) {
            return at == text.size() ||
                   std::ranges::any_of(kEndings, [&text, at](std::string_view ending) {
                       return text.substr(at).starts_with(ending);
                   });
        };
        while (!endsHere(end) && text[end] != '<' && text[end] != '(')
        {
            ++end;
        }
        if (!endsHere(end))
        {
            continue;
        }
        std::vector<std::string> columns = javaSplitAtCommaSpace(text.substr(first, end - first));
        std::ranges::sort(columns, [](const std::string& a, const std::string& b) {
            return Strings::javaCompareTo(a, b) < 0;
        });
        return std::string(text.substr(0, first)) + Strings::join(", ", columns) +
               std::string(text.substr(end));
    }
    return std::string(text);
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
    if (reference.schemaVersion >= 2)
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
    if (scenario == kGuiSimulationsScenario)
    {
        return kGuiSimulationsReloadChanges;
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

/// @p text with its backslashes, line feeds and tabs written as \\, \n and \t, so that a text is
/// one line.
[[nodiscard]] std::string escapedText(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

/// @p text quoted and escaped, or "(none)".
[[nodiscard]] std::string optionalText(const std::optional<std::string>& text)
{
    return text.has_value() ? "'" + escapedText(*text) + "'" : std::string("(none)");
}

/// What the settings dialog showed, in one line.
[[nodiscard]] std::string settingsDialogText(const GuiSettingsDialogState& state)
{
    std::vector<std::string> tabs;
    tabs.reserve(state.tabs.size());
    for (const GuiConfigTabState& tab : state.tabs)
    {
        tabs.push_back(
            std::format("{} '{}' {}", tab.tab, tab.title, tab.enabled ? "enabled" : "disabled"));
    }
    return std::format(
        "title='{}' name='{}' configuration='{}' status='{}' tab={} tabs=[{}] "
        "multiSimEdit={}",
        state.title, state.name, state.configuration, state.status, state.tab,
        joinedTexts(tabs, ", "), optionalText(state.multiSimEdit));
}

/// The lines of what the wind editor showed: its title and reference, then its probe lines.
void addWindEditorLines(std::string_view prefix, const GuiWindEditorState& state,
                        std::vector<std::string>& lines)
{
    lines.push_back(
        std::format("{}: title='{}' reference={}", prefix, state.title, state.altitudeReference));
    for (const std::string& line : state.lines)
    {
        lines.push_back(std::format("{} line: {}", prefix, escapedText(line)));
    }
}

/// What the CSV import settings showed, in one line.
[[nodiscard]] std::string csvImportText(const GuiCsvImportState& state)
{
    std::string columns;
    for (std::size_t at = 0; at < state.columns.size(); ++at)
    {
        const GuiCsvColumnState& column = state.columns.at(at);
        columns += std::format("{}{}='{}'/{}/{}", columns.empty() ? "" : " ",
                               kGuiCsvImportRows.at(at), column.name, column.index, column.unit);
    }
    return std::format("title='{}' hasHeader={} {} separator='{}'", state.title, state.hasHeader,
                       columns, escapedText(state.separator));
}

/// The lines of what the lookup dialog showed: its title and section, then per section its
/// summary and states, and its text.
void addLookupLines(std::string_view prefix, const GuiLookupState& state,
                    std::vector<std::string>& lines)
{
    lines.push_back(std::format("{}: title='{}' tab={}", prefix, state.title, state.tab));
    for (const auto& [name, section] : {std::pair{std::string_view("drag"), &state.drag},
                                        std::pair{std::string_view("stability"), &state.stability}})
    {
        lines.push_back(std::format(
            "{} {}: summary='{}' toolTip={} border='{}' editable={} separator='{}' clear={} "
            "refresh={}",
            prefix, name, escapedText(section->summary), optionalText(section->summaryToolTip),
            section->border, section->editable, escapedText(section->separator),
            section->clearEnabled, section->refreshEnabled));
        lines.push_back(std::format("{} {} text: {}", prefix, name, escapedText(section->text)));
    }
}

/// The lines of a question of the simulations tab (version 3), after its first line.
void addSimulationQuestionLines(const std::string& prefix, const GuiQuestion& question,
                                std::vector<std::string>& lines)
{
    switch (question.kind)
    {
        case GuiQuestionKind::SIMULATION:
            lines.push_back(prefix + " shown: " + settingsDialogText(question.settingsShown));
            lines.push_back(prefix + " answered: " + settingsDialogText(question.settingsAnswered));
            return;
        case GuiQuestionKind::RUN:
            lines.push_back(std::format("{} simulations: {}", prefix,
                                        joinedTexts(question.simulations, " | ")));
            return;
        case GuiQuestionKind::CONFIRM:
        {
            std::vector<std::string> message;
            message.reserve(question.message.size());
            for (const std::string& part : question.message)
            {
                message.push_back(escapedText(part));
            }
            lines.push_back(std::format("{} message: {}", prefix, joinedTexts(message, " | ")));
            lines.push_back(
                std::format("{} checkBoxes: {}", prefix, joinedTexts(question.checkBoxes, " | ")));
            lines.push_back(
                std::format("{} buttons: {}", prefix, joinedTexts(question.buttons, " | ")));
            return;
        }
        case GuiQuestionKind::WIND_LEVELS:
            addWindEditorLines(prefix + " shown", question.windShown, lines);
            addWindEditorLines(prefix + " answered", question.windAnswered, lines);
            return;
        case GuiQuestionKind::CSV_IMPORT_SETTINGS:
            lines.push_back(prefix + " shown: " + csvImportText(question.csvShown));
            lines.push_back(prefix + " answered: " + csvImportText(question.csvAnswered));
            return;
        case GuiQuestionKind::OPEN_FILE:
            lines.push_back(std::format("{} file: {}", prefix, optionalText(question.file)));
            return;
        case GuiQuestionKind::LOOKUP:
            addLookupLines(prefix + " shown", question.lookupShown, lines);
            addLookupLines(prefix + " answered", question.lookupAnswered, lines);
            return;
        default:
            return;
    }
}

/// The lines of what the simulations tab showed (version 3).
void addSimulationsTabLines(const GuiSimulationsTabState& state, std::vector<std::string>& lines)
{
    lines.push_back(std::format("simulationsTab help: {}", state.showsHelp));
    std::vector<std::string> columns;
    columns.reserve(state.columns.size());
    for (const GuiSimulationTableColumn& column : state.columns)
    {
        columns.push_back(std::format("{} '{}'", column.id, column.header));
    }
    lines.push_back("simulationsTab columns: " + joinedTexts(columns, ", "));
    for (std::size_t at = 0; at < state.rows.size(); ++at)
    {
        const GuiSimulationTableRow& row = state.rows[at];
        std::vector<std::string>     cells;
        cells.reserve(row.cells.size());
        for (const std::optional<std::string>& cell : row.cells)
        {
            cells.push_back(optionalText(cell));
        }
        lines.push_back(std::format(
            "simulationsTab row {}: status={} sortText='{}' description='{}' warnings='{}' "
            "counts={} toolTip='{}' cells=[{}]",
            at, row.status, row.statusSortText, row.description, row.warnings,
            integersText(row.warningCounts), escapedText(row.warningsToolTip),
            joinedTexts(cells, " | ")));
    }
    lines.push_back("simulationsTab selection: " + integersText(state.selectedRows));
    lines.push_back("simulationsTab actions: " + buttonsText(state.actions));
    lines.push_back("simulationsTab clipboard: " + joinedTexts(state.clipboardSimulations, " | "));
    lines.push_back("simulationsTab hiddenColumns: " + optionalText(state.hiddenColumnsPreference));
}

/// The lines of the document's simulations (version 3): each one's name, configuration, status
/// and extensions, then its options.
void addSimulationLines(std::span<const GuiSimulationState> simulations,
                        std::vector<std::string>&           lines)
{
    for (std::size_t at = 0; at < simulations.size(); ++at)
    {
        const GuiSimulationState& simulation = simulations[at];
        lines.push_back(
            std::format("simulation {}: '{}' configuration={} status={} "
                        "extensions=[{}]",
                        at, simulation.name, simulation.configuration, simulation.status,
                        joinedTexts(simulation.extensions, ", ")));
        for (const std::string& option : simulation.options)
        {
            lines.push_back(std::format("simulation {} option: {}", at, escapedText(option)));
        }
    }
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
        case GuiQuestionKind::SIMULATION:
        case GuiQuestionKind::RUN:
        case GuiQuestionKind::CONFIRM:
        case GuiQuestionKind::WIND_LEVELS:
        case GuiQuestionKind::CSV_IMPORT_SETTINGS:
        case GuiQuestionKind::OPEN_FILE:
        case GuiQuestionKind::LOOKUP:
            addSimulationQuestionLines(prefix, question, lines);
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

/// The rules of the simulations tab for the record of step @p step (guiExpectedRecord()): no
/// Monte Carlo action (Milestone 7), and QtRocket's Paste (bug 27).
void applySimulationsTabRules(std::size_t step, GuiSimulationsTabState& state,
                              std::string& problems)
{
    // The Monte Carlo action comes with Milestone 7 (U-M4-3).
    state.actions.erase("landingDispersion");
    const auto paste = state.actions.find("pasteSimulation");
    if (paste == state.actions.end())
    {
        problems += std::format("step {}: the simulations tab has no Paste\n", step);
        return;
    }
    const bool clipboard = !state.clipboardSimulations.empty();
    const bool selected  = !state.selectedRows.empty();
    // SimulationPanel.PasteSimulationAction.updateEnabledState(): simulations on the clipboard and
    // a selected row.
    if (paste->second != (clipboard && selected))
    {
        problems += std::format(
            "step {}: OpenRocket's Paste is {} with {} simulations on the clipboard and {} "
            "selected rows (bug 27: it needs both)\n",
            step, paste->second ? "enabled" : "disabled", state.clipboardSimulations.size(),
            state.selectedRows.size());
        return;
    }
    // OpenRocket bug 27, fixed in QtRocket: no selected row needed while the table is shown.
    paste->second = clipboard && (selected || !state.showsHelp);
}

}  // namespace

GuiStepRecord guiExpectedRecord(std::string_view scenario, GuiStepRecord record,
                                std::string& problems)
{
    if (record.simulationsTab.has_value())
    {
        applySimulationsTabRules(record.index, *record.simulationsTab, problems);
    }
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
    if (record.simulationsTab.has_value())
    {
        addSimulationsTabLines(*record.simulationsTab, lines);
        addSimulationLines(record.simulations, lines);
    }
    if (record.clipboardText.has_value())
    {
        lines.push_back("clipboard text: " + escapedText(*record.clipboardText));
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
    // The members of versions 2 and 3, line for line.
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
