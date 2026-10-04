#include "goldens/GoldenMismatches.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The complete nlohmann::json, whose members positions() calls; the include-cleaner check counts
// only the name, which json_fwd.hpp declares.
// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
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
                                 const std::vector<Coordinate>& actual)
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

void GoldenMismatches::text(std::string_view field, std::string_view expected,
                            std::string_view actual)
{
    if (expected != actual)
    {
        m_text += std::format("  {}: expected \"{}\", got \"{}\"\n", field, expected, actual);
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
