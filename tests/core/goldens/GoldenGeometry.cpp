#include "goldens/GoldenGeometry.h"

#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
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

Result<nlohmann::json> loadGoldenGeometry(std::string_view name)
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
    return loadGoldenJson(input->geometry);
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

}  // namespace QtRocket::Test
