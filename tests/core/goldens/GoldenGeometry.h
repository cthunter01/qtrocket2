#pragma once

// Reading the geometry golden data (tests/data/goldens/<input>/geometry.json, written by
// tools/openrocket-goldens): the documents, the component entries and their values. Test-only;
// the geometry golden tests (body_geometry_golden_tests.cpp, fin_geometry_golden_tests.cpp) share
// it.

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// A golden number (also "NaN", "Infinity", "-Infinity"); NaN for anything else.
[[nodiscard]] double goldenValue(const nlohmann::json& value);

/// A golden [x, y, z] or [x, y, z, w].
[[nodiscard]] Coordinate goldenCoordinate(const nlohmann::json& value);

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

}  // namespace QtRocket::Test
