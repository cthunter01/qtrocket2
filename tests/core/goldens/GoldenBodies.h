#pragma once

// What the geometry golden tests (body_geometry_golden_tests.cpp, fin_geometry_golden_tests.cpp)
// share: reading the values of tests/data/goldens/<input>/geometry.json, collecting the
// differences between them and the computed values, and rebuilding a body component from its
// golden entry. Test-only.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// Geometry and mass values: relative tolerance.
inline constexpr double kGoldenRelative = 1e-9;
/// Positions and CGs: absolute tolerance, in m.
inline constexpr double kGoldenAbsolute = 1e-9;

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

/// Collects the differences between golden and computed values, one line each.
class GoldenMismatches
{
public:
    /// Mismatches of the thing @p context names (it heads the report).
    explicit GoldenMismatches(std::string context);

    /// @p actual within kGoldenRelative of @p expected, relative to the larger magnitude (exact
    /// for 0); two NaNs match.
    void relative(std::string_view field, double expected, double actual);

    /// @p actual within kGoldenAbsolute of @p expected.
    void absolute(std::string_view field, double expected, double actual);

    /// A position: each of x, y and z within kGoldenAbsolute.
    void position(std::string_view field, const Coordinate& expected, const Coordinate& actual);

    /// A CG: the position within kGoldenAbsolute, the mass (weight) within kGoldenRelative.
    void cg(std::string_view field, const Coordinate& expected, const Coordinate& actual);

    /// A list of positions of the same length as the golden list @p expected.
    void positions(std::string_view field, const nlohmann::json& expected,
                   const std::vector<Coordinate>& actual);

    /// Two texts that must be the same.
    void text(std::string_view field, std::string_view expected, std::string_view actual);

    /// A problem that is not a comparison (a value that could not be read or rebuilt).
    void note(std::string_view message);

    /// The report, empty when everything matched.
    [[nodiscard]] std::string report() const;

private:
    void add(std::string_view field, double expected, double actual);

    std::string m_context;
    std::string m_text;
};

/// The body component the golden @p component describes, built on its own (not in a rocket),
/// with its material and with fixed radii (automatic radii as OpenRocket settled them); nullptr
/// for a component that is not a body component (a BodyTube, a NoseCone or a Transition).
[[nodiscard]] std::unique_ptr<SymmetricComponent> rebuildGoldenBody(
    const nlohmann::json& component);

}  // namespace QtRocket::Test
