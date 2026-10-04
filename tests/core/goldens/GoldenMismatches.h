#pragma once

// GoldenMismatches: collects the differences between golden and computed values, for the golden
// tests that compare many values of one thing and report them together. Test-only.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

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

    /// @p actual within kGoldenAbsolute of @p expected (a length or a position in m, an angle
    /// in rad).
    void absolute(std::string_view field, double expected, double actual);

    /// @p actual exactly @p expected: a value a component was given must come back unchanged.
    void exact(std::string_view field, double expected, double actual);

    /// A position: each of x, y and z within kGoldenAbsolute.
    void position(std::string_view field, const Coordinate& expected, const Coordinate& actual);

    /// A CG: the position within kGoldenAbsolute, the mass (weight) within kGoldenRelative.
    void cg(std::string_view field, const Coordinate& expected, const Coordinate& actual);

    /// A list of positions of the same length as the golden list @p expected (a JSON array of
    /// [x, y, z], see goldenCoordinate() in GoldenGeometry.h).
    void positions(std::string_view field, const nlohmann::json& expected,
                   std::span<const Coordinate> actual);

    /// A list of angles of the same length as the golden list @p expected, each within
    /// kGoldenAbsolute (rad).
    void angles(std::string_view field, const nlohmann::json& expected,
                std::span<const double> actual);

    /// A transformation against the golden {"rotation": 9 numbers (rows first), "translation":
    /// [x, y, z]}: each element of its matrix and each of x, y and z of its translation within
    /// kGoldenAbsolute.
    void transformation(std::string_view field, const nlohmann::json& expected,
                        const Transformation& actual);

    /// Two texts that must be the same (a name, the name of an enum constant).
    void text(std::string_view field, std::string_view expected, std::string_view actual);

    /// Two integers that must be the same.
    void integer(std::string_view field, std::int64_t expected, std::int64_t actual);

    /// Two flags that must be the same.
    void boolean(std::string_view field, bool expected, bool actual);

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
