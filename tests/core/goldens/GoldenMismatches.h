#pragma once

// GoldenMismatches: collects the differences between golden and computed values, for the golden
// tests that compare many values of one thing and report them together. Test-only.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

/// Geometry and mass values: relative tolerance.
inline constexpr double kGoldenRelative = 1e-9;
/// Positions and CGs: absolute tolerance, in m.
inline constexpr double kGoldenAbsolute = 1e-9;

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

    /// A list of positions of the same length as the golden list @p expected (a JSON array of
    /// [x, y, z], see goldenCoordinate() in GoldenGeometry.h).
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

}  // namespace QtRocket::Test
