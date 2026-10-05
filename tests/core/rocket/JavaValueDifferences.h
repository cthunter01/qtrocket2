#pragma once

// Comparing what a component answers with the values OpenRocket's own classes answer, as a Java
// program printed them (the "Pins" of TubeFinSetTests.cpp, LaunchLugTests.cpp and
// RailButtonTests.cpp, and of the calculator tests in tests/core/aero). Test-only.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

/// Whether @p actual is the Java value @p expected: both NaN, equal (the infinities, the zeros),
/// or within 1e-12 relative, plus 1e-15 absolute for values near zero such as sin(pi) * r (values
/// that went through sin() or cos() differ in the last bits between math libraries). An infinite
/// Java value (two tube fins: r / (1 - sin(pi / 2))) also matches a value beyond 1e12 of its
/// sign, which is what a sin(pi / 2) one bit below 1 would give.
[[nodiscard]] inline bool matchesJavaValue(double expected, double actual)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (expected == actual)
    {
        return true;
    }
    if (std::isinf(expected))
    {
        return expected > 0 ? actual > 1e12 : actual < -1e12;
    }
    if (std::isinf(actual))
    {
        // Never a finite Java value (without this, inf <= 1e-12 * inf + 1e-15 would accept it).
        return false;
    }
    return std::abs(actual - expected) <=
           (1e-12 * std::max(std::abs(expected), std::abs(actual))) + 1e-15;
}

/// Whether @p actual is the pinned Java value @p expected, held more strictly than
/// matchesJavaValue(): NaN with NaN, an infinity with the same infinity only (the result of an
/// exact division by zero), and a finite value with a finite one within @p relativeTolerance of
/// the value itself, without an absolute allowance: a small value is held as strictly as a large
/// one and a zero must be a zero (of either sign).
[[nodiscard]] inline bool matchesPinnedValue(double expected, double actual,
                                             double relativeTolerance = 1e-12)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (std::isinf(expected) || std::isinf(actual))
    {
        return expected == actual;
    }
    return std::abs(actual - expected) <=
           relativeTolerance * std::max(std::abs(expected), std::abs(actual));
}

/// Collects the differences between Java's values and the computed ones, one line each.
class JavaValueDifferences
{
public:
    /// A number: see matchesJavaValue().
    void number(std::string_view field, double expected, double actual)
    {
        if (!matchesJavaValue(expected, actual))
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    /// A pinned number: see matchesPinnedValue().
    void pinned(std::string_view field, double expected, double actual,
                double relativeTolerance = 1e-12)
    {
        if (!matchesPinnedValue(expected, actual, relativeTolerance))
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    void integer(std::string_view field, int expected, int actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    void name(std::string_view field, std::string_view expected, std::string_view actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected \"{}\", got \"{}\"\n", field, expected, actual);
        }
    }

    /// Something that kept a value from being compared.
    void problem(std::string_view what) { m_text += std::format("  {}\n", what); }

    /// A coordinate: x, y, z and the weight, each as number().
    void coordinate(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        number(std::format("{}.x", field), expected.x, actual.x);
        number(std::format("{}.y", field), expected.y, actual.y);
        number(std::format("{}.z", field), expected.z, actual.z);
        number(std::format("{}.weight", field), expected.weight, actual.weight);
    }

    /// Two lists of numbers of the same length, each element as number().
    void numbers(std::string_view field, const std::vector<double>& expected,
                 const std::vector<double>& actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: expected {} values, got {}\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < expected.size(); i++)
        {
            number(std::format("{}[{}]", field, i), expected[i], actual[i]);
        }
    }

    /// Two lists of coordinates of the same length, each element as coordinate().
    void coordinates(std::string_view field, const std::vector<Coordinate>& expected,
                     const std::vector<Coordinate>& actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: expected {} points, got {}\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < expected.size(); i++)
        {
            coordinate(std::format("{}[{}]", field, i), expected[i], actual[i]);
        }
    }

    /// Empty when everything matched.
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

private:
    std::string m_text;
};

}  // namespace QtRocket::Test
