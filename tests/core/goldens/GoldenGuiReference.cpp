#include "goldens/GoldenGuiReference.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The schemas and the version the tool writes (Scenario.SCHEMA, GuiReference).
constexpr std::string_view kScenarioSchema  = "gui-from-scratch-scenario";
constexpr std::string_view kReferenceSchema = "gui-from-scratch-reference";
constexpr int              kSchemaVersion   = 1;

/// A step kind, its name and the members it takes besides "step" and "note" (separated by
/// blanks; every one is required but "unit"), as Scenario.membersOf() of the tool has them.
struct StepSpec
{
    GuiStepKind      kind{GuiStepKind::NEW};
    std::string_view name;
    std::string_view members;
};

constexpr std::array kStepSpecs{
    StepSpec{.kind = GuiStepKind::NEW, .name = "new", .members = ""},
    StepSpec{.kind = GuiStepKind::SELECT, .name = "select", .members = "paths"},
    StepSpec{.kind = GuiStepKind::ADD, .name = "add", .members = "component"},
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
    StepSpec{.kind = GuiStepKind::COPY, .name = "copy", .members = ""},
    StepSpec{.kind = GuiStepKind::PASTE, .name = "paste", .members = ""},
    StepSpec{.kind = GuiStepKind::SAVE_AS, .name = "saveAs", .members = ""},
    StepSpec{.kind = GuiStepKind::CHOOSE_FILE, .name = "chooseFile", .members = "file"},
};

// The tables are searched through spans: an iterator of an array is a pointer with some standard
// libraries and a class with others.

[[nodiscard]] const StepSpec* stepSpecNamed(std::string_view name) noexcept
{
    const std::span<const StepSpec> specs(kStepSpecs);
    const auto                      found =
        std::ranges::find_if(specs, [name](const StepSpec& spec) { return spec.name == name; });
    return found == specs.end() ? nullptr : &*found;
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
        if (!object.is_object() || !object.contains(key) || object.at(key).is_null())
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

    /// Fails unless @p object has the "schema" @p schema of kSchemaVersion.
    void schema(const json& object, std::string_view schema)
    {
        if (string(object, "schema", "the document") != schema ||
            integer(object, "schemaVersion", "the document") != kSchemaVersion)
        {
            failure("the document", std::format("not a {} of version {}", schema, kSchemaVersion));
        }
    }

private:
    std::optional<std::string> m_error;
};

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
    else
    {
        const std::span<const StringMember> members(kStringMembers);
        const auto found = std::ranges::find(members, member, &StringMember::name);
        if (found == members.end())
        {
            reader.failure(context, std::format(R"(the tool knows no member "{}")", member));
            return;
        }
        step.*(found->member) = reader.string(object, member, context);
    }
}

/// Step @p index of the scenario.
[[nodiscard]] GuiScenarioStep scenarioStep(JsonReader& reader, const json& object,
                                           std::size_t index)
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

[[nodiscard]] GuiStepRecord stepRecord(JsonReader& reader, const json& object, std::size_t at)
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
    return record;
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
    return goldensDir() / "gui" / "from-scratch";
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

Result<GuiScenario> parseGuiScenario(std::string_view text)
{
    const Result<json> document = parsed(text);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    JsonReader reader;
    reader.schema(*document, kScenarioSchema);
    GuiScenario scenario;
    scenario.title    = reader.string(*document, "title", "the document");
    const json& steps = reader.array(*document, "steps", "the document");
    for (std::size_t index = 0; index < steps.size(); index++)
    {
        scenario.steps.push_back(scenarioStep(reader, steps.at(index), index));
    }
    if (scenario.steps.empty() || scenario.steps.front().kind != GuiStepKind::NEW)
    {
        reader.failure("the document", "the scenario does not start with File > New");
    }
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
    JsonReader reader;
    reader.schema(*document, kReferenceSchema);
    GuiReference reference;
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
    const json& steps = reader.array(*document, "steps", "the document");
    for (std::size_t at = 0; at < steps.size(); at++)
    {
        reference.steps.push_back(stepRecord(reader, steps.at(at), at));
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

}  // namespace QtRocket::Test
