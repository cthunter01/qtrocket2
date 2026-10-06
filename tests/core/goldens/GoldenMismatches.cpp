#include "goldens/GoldenMismatches.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

// The complete nlohmann::json, whose members positions(), angles() and transformation() call; the
// include-cleaner check counts only the name, which json_fwd.hpp declares.
// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "goldens/GoldenGeometry.h"

namespace QtRocket::Test
{

GoldenMismatches::GoldenMismatches(std::string context) : m_context(std::move(context)) { }

void GoldenMismatches::relative(std::string_view field, double expected, double actual)
{
    if (std::isnan(expected) && std::isnan(actual))
    {
        return;
    }
    const double scale = std::max(std::abs(expected), std::abs(actual));
    if (!(std::abs(actual - expected) <= kGoldenRelative * scale))
    {
        add(field, expected, actual);
    }
}

void GoldenMismatches::absolute(std::string_view field, double expected, double actual)
{
    if (!(std::abs(actual - expected) <= kGoldenAbsolute))
    {
        add(field, expected, actual);
    }
}

void GoldenMismatches::exact(std::string_view field, double expected, double actual)
{
    if (expected != actual)
    {
        add(field, expected, actual);
    }
}

void GoldenMismatches::within(std::string_view field, double expected, double actual,
                              double relativeTolerance, double absoluteTolerance)
{
    if ((std::isnan(expected) && std::isnan(actual)) || expected == actual)
    {
        return;
    }
    // An infinity on one side only, or a NaN, gives a difference that is not finite: a mismatch
    // whatever the tolerances (an infinite scale would otherwise admit it).
    const double difference = std::abs(actual - expected);
    const double scale      = std::max(std::abs(expected), std::abs(actual));
    if (!std::isfinite(difference) ||
        (!(difference <= relativeTolerance * scale) && !(difference <= absoluteTolerance)))
    {
        add(field, expected, actual);
    }
}

void GoldenMismatches::position(std::string_view field, const Coordinate& expected,
                                const Coordinate& actual)
{
    absolute(std::format("{}.x", field), expected.x, actual.x);
    absolute(std::format("{}.y", field), expected.y, actual.y);
    absolute(std::format("{}.z", field), expected.z, actual.z);
}

void GoldenMismatches::cg(std::string_view field, const Coordinate& expected,
                          const Coordinate& actual)
{
    position(field, expected, actual);
    relative(std::format("{}.weight", field), expected.weight, actual.weight);
}

void GoldenMismatches::positions(std::string_view field, const nlohmann::json& expected,
                                 std::span<const Coordinate> actual)
{
    if (expected.size() != actual.size())
    {
        m_text += std::format("  {}: {} points expected, {} computed\n", field, expected.size(),
                              actual.size());
        return;
    }
    for (std::size_t i = 0; i < actual.size(); i++)
    {
        position(std::format("{}[{}]", field, i), goldenCoordinate(expected.at(i)), actual[i]);
    }
}

void GoldenMismatches::angles(std::string_view field, const nlohmann::json& expected,
                              std::span<const double> actual)
{
    if (expected.size() != actual.size())
    {
        m_text += std::format("  {}: {} angles expected, {} computed\n", field, expected.size(),
                              actual.size());
        return;
    }
    for (std::size_t i = 0; i < actual.size(); i++)
    {
        absolute(std::format("{}[{}]", field, i), goldenValue(expected.at(i)), actual[i]);
    }
}

void GoldenMismatches::transformation(std::string_view field, const nlohmann::json& expected,
                                      const Transformation& actual)
{
    const nlohmann::json& rotation = expected.at("rotation");
    for (std::size_t row = 0; row < 3; row++)
    {
        for (std::size_t column = 0; column < 3; column++)
        {
            absolute(std::format("{}.rotation[{}][{}]", field, row, column),
                     goldenValue(rotation.at((row * 3) + column)),
                     actual.matrix().at(row).at(column));
        }
    }
    position(std::format("{}.translation", field), goldenCoordinate(expected.at("translation")),
             actual.translationVector());
}

void GoldenMismatches::text(std::string_view field, std::string_view expected,
                            std::string_view actual)
{
    if (expected != actual)
    {
        m_text += std::format("  {}: expected \"{}\", got \"{}\"\n", field, expected, actual);
    }
}

void GoldenMismatches::integer(std::string_view field, std::int64_t expected, std::int64_t actual)
{
    if (expected != actual)
    {
        m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
    }
}

void GoldenMismatches::boolean(std::string_view field, bool expected, bool actual)
{
    if (expected != actual)
    {
        m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
    }
}

void GoldenMismatches::note(std::string_view message)
{
    m_text += std::format("  {}\n", message);
}

std::string GoldenMismatches::report() const
{
    return m_text.empty() ? std::string{} : m_context + ":\n" + m_text;
}

void GoldenMismatches::add(std::string_view field, double expected, double actual)
{
    m_text += std::format("  {}: expected {}, got {} (difference {})\n", field, expected, actual,
                          actual - expected);
}

}  // namespace QtRocket::Test
