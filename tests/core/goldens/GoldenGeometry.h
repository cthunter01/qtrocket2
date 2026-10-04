#pragma once

// Reading the geometry golden data (tests/data/goldens/<input>/geometry.json, written by
// tools/openrocket-goldens): the documents, the component entries and their values, the golden
// paths of the components of a rebuilt rocket, and OpenRocket's re-save of an input (for what
// geometry.json does not record). Test-only; the golden tests share it.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Test
{

/// A golden number (also "NaN", "Infinity", "-Infinity"); NaN for anything else.
[[nodiscard]] double goldenValue(const nlohmann::json& value);

/// A golden [x, y, z] or [x, y, z, w].
[[nodiscard]] Coordinate goldenCoordinate(const nlohmann::json& value);

/// The bulk material a golden {name, type, density} entry describes (a component's
/// "details.material").
[[nodiscard]] Material goldenMaterial(const nlohmann::json& material);

/// The axial method geometry.json names @p name ("BOTTOM"), or nullopt.
[[nodiscard]] std::optional<AxialMethod> goldenAxialMethod(std::string_view name);

/// The geometry.json of the golden input @p name, found through manifest.json.
[[nodiscard]] Result<nlohmann::json> loadGoldenGeometry(std::string_view name);

/// loadGoldenGeometry(), read and parsed once per test run and kept: the document lives as long
/// as the process.
[[nodiscard]] Result<const nlohmann::json*> goldenGeometry(std::string_view name);

/// The entry of the component at @p path ("/0/1/2") in the geometry document @p geometry, or
/// null when it has none.
[[nodiscard]] const nlohmann::json* findGoldenComponent(const nlohmann::json& geometry,
                                                        std::string_view      path);

/// The names of every golden input, from manifest.json; none when it cannot be read.
[[nodiscard]] std::vector<std::string> goldenInputNames();

/// OpenRocket's re-save of the golden input @p name (<input>/resave/rocket.ork, found through
/// manifest.json), parsed. Fails as loadGoldenGeometry() does, and with ErrorCode::PARSE when the
/// file is not XML.
[[nodiscard]] Result<std::unique_ptr<pugi::xml_document>> loadGoldenResave(std::string_view name);

/// The element of the component with the id @p id (the "id" of its golden entry) in the re-saved
/// design @p resave; an empty node when it is not in the file.
[[nodiscard]] pugi::xml_node savedGoldenComponent(const pugi::xml_document& resave,
                                                  std::string_view          id);

/// The component at the golden @p path ("/" is @p root, "/0" its first child, "/0/1" that
/// child's second child) under @p root, or nullptr when @p root has no such descendant.
[[nodiscard]] RocketComponent* componentAtGoldenPath(RocketComponent& root, std::string_view path);
[[nodiscard]] const RocketComponent* componentAtGoldenPath(const RocketComponent& root,
                                                           std::string_view       path);

/// The golden path of @p component: the child indices from the root of its tree ("/" for the
/// root itself).
[[nodiscard]] std::string goldenPathOf(const RocketComponent& component);

}  // namespace QtRocket::Test
