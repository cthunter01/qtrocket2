#include "goldens/GoldenBodies.h"

#include <algorithm>
#include <cmath>
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

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// Sets a Transition's shoulders, wall and radii to the golden @p details. The shoulder setters
/// compare with MathUtil::equals, so each value is first moved away from the target.
void applyTransitionDetails(Transition& transition, const json& details)
{
    const std::optional<TransitionShape> shape =
        transitionShapeFromName(details.at("shapeType").get<std::string>());
    if (!shape)
    {
        ADD_FAILURE() << "unknown shape " << details.at("shapeType");
        return;
    }
    transition.setShapeType(*shape);
    transition.setShapeParameter(goldenValue(details.at("shapeParameter")));
    transition.setClipped(details.at("clipped").get<bool>());
    transition.setForeRadius(goldenValue(details.at("foreRadius")), false);
    transition.setAftRadius(goldenValue(details.at("aftRadius")), false);
    transition.setThickness(goldenValue(details.at("thickness")), false);
    transition.setFilled(details.at("filled").get<bool>());

    const double foreLength = goldenValue(details.at("foreShoulderLength"));
    if (foreLength != 0)
    {
        transition.setForeShoulderLength(foreLength);
    }
    transition.setForeShoulderRadius(-1, false);
    transition.setForeShoulderRadius(goldenValue(details.at("foreShoulderRadius")), false);
    transition.setForeShoulderThickness(-1);
    transition.setForeShoulderThickness(goldenValue(details.at("foreShoulderThickness")));
    transition.setForeShoulderCapped(details.at("foreShoulderCapped").get<bool>());

    const double aftLength = goldenValue(details.at("aftShoulderLength"));
    if (aftLength != 0)
    {
        transition.setAftShoulderLength(aftLength);
    }
    transition.setAftShoulderRadius(-1, false);
    transition.setAftShoulderRadius(goldenValue(details.at("aftShoulderRadius")), false);
    transition.setAftShoulderThickness(-1);
    transition.setAftShoulderThickness(goldenValue(details.at("aftShoulderThickness")));
    transition.setAftShoulderCapped(details.at("aftShoulderCapped").get<bool>());
}

}  // namespace

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

std::unique_ptr<SymmetricComponent> rebuildGoldenBody(const nlohmann::json& component)
{
    const std::string type    = component.at("type").get<std::string>();
    const json&       details = component.at("details");
    const double      length  = goldenValue(component.at("length"));

    std::unique_ptr<SymmetricComponent> result;
    if (type == "BodyTube")
    {
        const double radius = goldenValue(details.at("outerRadius"));
        if (details.at("filled").get<bool>())
        {
            result = std::make_unique<BodyTube>(length, radius, true);
        }
        else
        {
            result =
                std::make_unique<BodyTube>(length, radius, goldenValue(details.at("thickness")));
        }
    }
    else if (type == "NoseCone")
    {
        // A flipped nose cone (a tail cone) has its base at the front.
        const double fore    = goldenValue(details.at("foreRadius"));
        const double aft     = goldenValue(details.at("aftRadius"));
        const bool   flipped = fore > 0 && aft == 0;
        auto         nose =
            std::make_unique<NoseCone>(TransitionShape::CONICAL, length, flipped ? fore : aft);
        if (flipped)
        {
            nose->setFlipped(true, false);
        }
        applyTransitionDetails(*nose, details);
        result = std::move(nose);
    }
    else if (type == "Transition")
    {
        auto transition = std::make_unique<Transition>();
        transition->setLength(length);
        applyTransitionDetails(*transition, details);
        result = std::move(transition);
    }
    else
    {
        return nullptr;
    }

    const json& material = details.at("material");
    result->setMaterial(Material::newMaterial(Material::Type::BULK,
                                              material.at("name").get<std::string>(),
                                              goldenValue(material.at("density")), true));
    return result;
}

}  // namespace QtRocket::Test
