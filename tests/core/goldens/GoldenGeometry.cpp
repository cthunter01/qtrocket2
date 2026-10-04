#include "goldens/GoldenGeometry.h"

#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

double goldenValue(const nlohmann::json& value)
{
    const std::optional<double> parsed = goldenNumber(value);
    return parsed.value_or(std::numeric_limits<double>::quiet_NaN());
}

Coordinate goldenCoordinate(const nlohmann::json& value)
{
    return Coordinate{goldenValue(value.at(0)), goldenValue(value.at(1)), goldenValue(value.at(2)),
                      value.size() > 3 ? goldenValue(value.at(3)) : 0.0};
}

Material goldenMaterial(const nlohmann::json& material)
{
    return Material::newMaterial(Material::Type::BULK, material.at("name").get<std::string>(),
                                 goldenValue(material.at("density")), true);
}

std::optional<AxialMethod> goldenAxialMethod(std::string_view name)
{
    for (const AxialMethod method : kAllAxialMethods)
    {
        if (axialMethodName(method) == name)
        {
            return method;
        }
    }
    return std::nullopt;
}

namespace
{

/// The manifest entry of the golden input @p name (a copy: the manifest is read for the call).
[[nodiscard]] Result<GoldenInput> goldenInput(std::string_view name)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const GoldenInput* input = manifest->find(name);
    if (input == nullptr)
    {
        return std::unexpected(Error{.code    = ErrorCode::NOT_FOUND,
                                     .message = std::format("no input {}", name),
                                     .where   = std::source_location::current()});
    }
    return *input;
}

}  // namespace

Result<nlohmann::json> loadGoldenGeometry(std::string_view name)
{
    const Result<GoldenInput> input = goldenInput(name);
    if (!input)
    {
        return std::unexpected(input.error());
    }
    return loadGoldenJson(input->geometry);
}

Result<std::unique_ptr<pugi::xml_document>> loadGoldenResave(std::string_view name)
{
    const Result<GoldenInput> input = goldenInput(name);
    if (!input)
    {
        return std::unexpected(input.error());
    }
    const Result<std::string> text = readTextFile(goldensDir() / input->resave);
    if (!text)
    {
        return std::unexpected(text.error());
    }
    auto                         document = std::make_unique<pugi::xml_document>();
    const pugi::xml_parse_result parsed   = document->load_buffer(text->data(), text->size());
    if (!parsed)
    {
        return std::unexpected(
            Error{.code    = ErrorCode::PARSE,
                  .message = std::format("{}: {}", input->resave, parsed.description()),
                  .where   = std::source_location::current()});
    }
    return document;
}

pugi::xml_node savedGoldenComponent(const pugi::xml_document& resave, std::string_view id)
{
    return resave.find_node([id](const pugi::xml_node& node) {
        return std::string_view{node.child_value("id")} == id;
    });
}

Result<const nlohmann::json*> goldenGeometry(std::string_view name)
{
    // The parsed geometry files by input name (std::map keeps the entries where they are, so
    // the pointers handed out stay valid).
    static std::mutex                                         s_mutex;
    static std::map<std::string, nlohmann::json, std::less<>> s_geometries;
    const std::scoped_lock                                    lock{s_mutex};
    auto                                                      cached = s_geometries.find(name);
    if (cached == s_geometries.end())
    {
        Result<nlohmann::json> geometry = loadGoldenGeometry(name);
        if (!geometry)
        {
            return std::unexpected(geometry.error());
        }
        cached = s_geometries.emplace(std::string{name}, std::move(*geometry)).first;
    }
    return &cached->second;
}

const nlohmann::json* findGoldenComponent(const nlohmann::json& geometry, std::string_view path)
{
    const auto components = geometry.find("components");
    if (components == geometry.end() || !components->is_array())
    {
        return nullptr;
    }
    for (const nlohmann::json& component : *components)
    {
        const auto componentPath = component.find("path");
        if (componentPath != component.end() && componentPath->is_string() &&
            componentPath->get_ref<const std::string&>() == path)
        {
            return &component;
        }
    }
    return nullptr;
}

std::vector<std::string> goldenInputNames()
{
    std::vector<std::string>     names;
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    if (manifest)
    {
        for (const GoldenInput& input : manifest->inputs)
        {
            names.push_back(input.name);
        }
    }
    return names;
}

namespace
{

/// componentAtGoldenPath() for a const or a mutable tree.
template <class Component>
[[nodiscard]] Component* descendGoldenPath(Component& root, std::string_view path)
{
    Component*  component = &root;
    std::size_t start     = 1;
    while (start < path.size())
    {
        std::size_t end = path.find('/', start);
        if (end == std::string_view::npos)
        {
            end = path.size();
        }
        const std::optional<int> index = Strings::parseInt(path.substr(start, end - start));
        if (!index || *index < 0 || std::cmp_greater_equal(*index, component->getChildCount()))
        {
            return nullptr;
        }
        component = &component->getChild(static_cast<std::size_t>(*index));
        start     = end + 1;
    }
    return component;
}

}  // namespace

RocketComponent* componentAtGoldenPath(RocketComponent& root, std::string_view path)
{
    return descendGoldenPath(root, path);
}

const RocketComponent* componentAtGoldenPath(const RocketComponent& root, std::string_view path)
{
    return descendGoldenPath(root, path);
}

std::string goldenPathOf(const RocketComponent& component)
{
    const RocketComponent* parent = component.getParent();
    if (parent == nullptr)
    {
        return "/";
    }
    const std::string above = goldenPathOf(*parent);
    return std::format("{}/{}", above == "/" ? "" : above,
                       parent->getChildPosition(&component).value_or(0));
}

}  // namespace QtRocket::Test
