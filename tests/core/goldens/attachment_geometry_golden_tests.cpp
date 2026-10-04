// Attachment geometry golden tests: the launch lugs, rail buttons and tube fin sets of the golden
// inputs compared with OpenRocket's own values in tests/data/goldens/<input>/geometry.json
// (tools/openrocket-goldens).
//
// AttachmentGeometryGolden rebuilds each of them on a rebuilt copy of its parent body tube, in a
// rocket of one stage so that the change events reach it, from what geometry.json records: the
// parent's length, settled outer radius and thickness, and the component's dimensions, material,
// instances, angle, overrides, axial method and offset. What geometry.json does not record comes
// from the design's re-saved file <input>/resave/rocket.ork: a rail button's diameters and
// heights, and whether a tube fin set's radius is automatic. Compared: the volume, the mass, the
// CG, both unit inertias, the component bounds, the instance bounding box, the instance offsets
// and angles, the position in the parent with the instance locations, the angle and radius
// methods with the radius offset (which the rebuild never sets: the classes' own answers), the
// instance separation of the lugs and buttons, and the mass, CG and inertias with the overrides
// applied.
//
// The 18 launch lugs, 5 rail buttons and 1 tube fin set of the 29 inputs all sit on body tubes
// (no other component accepts them); AttachmentGeometryGoldenCoverage counts the compared ones.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"

namespace
{

using nlohmann::json;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::ExternalComponent;
using QtRocket::LaunchLug;
using QtRocket::Material;
using QtRocket::RailButton;
using QtRocket::Rocket;
using QtRocket::TubeFinSet;

/// Geometry and mass values: relative tolerance.
constexpr double kRelative = 1e-9;
/// Positions and CGs: absolute tolerance, in m (and in rad for angles).
constexpr double kAbsolute = 1e-9;

/// A golden number (also "NaN", "Infinity", "-Infinity").
[[nodiscard]] double number(const json& value)
{
    const std::optional<double> parsed = QtRocket::Test::goldenNumber(value);
    return parsed.value_or(std::numeric_limits<double>::quiet_NaN());
}

/// A golden [x, y, z] or [x, y, z, w].
[[nodiscard]] Coordinate coordinate(const json& value)
{
    return Coordinate{number(value.at(0)), number(value.at(1)), number(value.at(2)),
                      value.size() > 3 ? number(value.at(3)) : 0.0};
}

/// Collects the differences between golden and computed values, one line each.
class Mismatches
{
public:
    explicit Mismatches(std::string context) : m_context(std::move(context)) { }

    /// @p actual within kRelative of @p expected, relative to the larger magnitude (exact for 0).
    void relative(std::string_view field, double expected, double actual)
    {
        if (std::isnan(expected) && std::isnan(actual))
        {
            return;
        }
        const double scale = std::max(std::abs(expected), std::abs(actual));
        if (!(std::abs(actual - expected) <= kRelative * scale))
        {
            add(field, expected, actual);
        }
    }

    /// @p actual within kAbsolute of @p expected.
    void absolute(std::string_view field, double expected, double actual)
    {
        if (!(std::abs(actual - expected) <= kAbsolute))
        {
            add(field, expected, actual);
        }
    }

    /// @p actual exactly @p expected: a value the component was given must come back unchanged.
    void exact(std::string_view field, double expected, double actual)
    {
        if (expected != actual)
        {
            add(field, expected, actual);
        }
    }

    /// @p actual the same text as @p expected (the name of an enum constant).
    void same(std::string_view field, std::string_view expected, std::string_view actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    /// A position: each of x, y and z within kAbsolute.
    void position(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        absolute(std::format("{}.x", field), expected.x, actual.x);
        absolute(std::format("{}.y", field), expected.y, actual.y);
        absolute(std::format("{}.z", field), expected.z, actual.z);
    }

    /// A CG: the position within kAbsolute, the mass (weight) within kRelative.
    void cg(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        position(field, expected, actual);
        relative(std::format("{}.weight", field), expected.weight, actual.weight);
    }

    /// A list of positions of the same length.
    void positions(std::string_view field, const json& expected, std::span<const Coordinate> actual)
    {
        if (expected.size() != actual.size())
        {
            problem(std::format("{}: {} points expected, {} computed", field, expected.size(),
                                actual.size()));
            return;
        }
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            position(std::format("{}[{}]", field, i), coordinate(expected.at(i)), actual[i]);
        }
    }

    /// A list of angles of the same length, each within kAbsolute.
    void angles(std::string_view field, const json& expected, std::span<const double> actual)
    {
        if (expected.size() != actual.size())
        {
            problem(std::format("{}: {} angles expected, {} computed", field, expected.size(),
                                actual.size()));
            return;
        }
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            absolute(std::format("{}[{}]", field, i), number(expected.at(i)), actual[i]);
        }
    }

    /// Something that kept a component from being rebuilt or compared.
    void problem(std::string_view what) { m_text += std::format("  {}\n", what); }

    /// The report, empty when everything matched.
    [[nodiscard]] std::string report() const
    {
        return m_text.empty() ? std::string{} : m_context + ":\n" + m_text;
    }

private:
    void add(std::string_view field, double expected, double actual)
    {
        m_text += std::format("  {}: expected {}, got {} (difference {})\n", field, expected,
                              actual, actual - expected);
    }

    std::string m_context;
    std::string m_text;
};

/// How many components of each class were rebuilt and compared.
struct Counts
{
    int launchLugs{0};
    int railButtons{0};
    int tubeFinSets{0};
};

/// What comparing the attachments of one input gave.
struct Outcome
{
    Counts      compared;
    std::string report;  ///< empty when everything matched
};

// ============================================================================ golden input

/// One golden input: its geometry.json and its re-saved design.
struct Input
{
    json               geometry;
    pugi::xml_document resave;
};

/// Loads geometry.json and resave/rocket.ork of the golden input @p name.
[[nodiscard]] QtRocket::Result<std::unique_ptr<Input>> loadInput(std::string_view name)
{
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const QtRocket::Test::GoldenInput* input = manifest->find(name);
    if (input == nullptr)
    {
        return std::unexpected(QtRocket::Error{.code    = QtRocket::ErrorCode::NOT_FOUND,
                                               .message = std::format("no input {}", name),
                                               .where   = std::source_location::current()});
    }
    QtRocket::Result<json> geometry = QtRocket::Test::loadGoldenJson(input->geometry);
    if (!geometry)
    {
        return std::unexpected(geometry.error());
    }
    const QtRocket::Result<std::string> text =
        QtRocket::readTextFile(QtRocket::Test::goldensDir() / input->resave);
    if (!text)
    {
        return std::unexpected(text.error());
    }
    auto loaded                         = std::make_unique<Input>();
    loaded->geometry                    = std::move(*geometry);
    const pugi::xml_parse_result parsed = loaded->resave.load_buffer(text->data(), text->size());
    if (!parsed)
    {
        return std::unexpected(
            QtRocket::Error{.code    = QtRocket::ErrorCode::PARSE,
                            .message = std::format("{}: {}", input->resave, parsed.description()),
                            .where   = std::source_location::current()});
    }
    return loaded;
}

/// The names of every golden input, from manifest.json.
[[nodiscard]] std::vector<std::string> inputNames()
{
    std::vector<std::string>                               names;
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (manifest)
    {
        for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
        {
            names.push_back(input.name);
        }
    }
    return names;
}

/// The golden entry of the component at @p path, or null.
[[nodiscard]] const json* componentAt(const json& geometry, std::string_view path)
{
    for (const json& component : geometry.at("components"))
    {
        if (component.at("path").get<std::string>() == path)
        {
            return &component;
        }
    }
    return nullptr;
}

/// The path of the parent of the component at @p path: the path without its last element ("/"
/// for a child of the rocket).
[[nodiscard]] std::string parentPath(std::string_view path)
{
    const std::size_t slash = path.rfind('/');
    return slash == 0 || slash == std::string_view::npos ? std::string{"/"}
                                                         : std::string{path.substr(0, slash)};
}

/// The element named @p element of the re-saved design whose <id> is @p id (an empty node when
/// there is none).
[[nodiscard]] pugi::xml_node resaveElement(const pugi::xml_document& resave,
                                           std::string_view element, std::string_view id)
{
    return resave.find_node([element, id](const pugi::xml_node& node) {
        return std::string_view{node.name()} == element &&
               std::string_view{node.child_value("id")} == id;
    });
}

/// The number in the child element @p child of @p node, as Java wrote it; nullopt when it is
/// missing or not a number ("auto").
[[nodiscard]] std::optional<double> resaveNumber(const pugi::xml_node& node, const char* child)
{
    return QtRocket::Strings::javaParseDouble(node.child_value(child));
}

/// The golden axial method ("TOP", "MIDDLE", ...).
[[nodiscard]] std::optional<AxialMethod> axialMethod(std::string_view name)
{
    for (const AxialMethod method : QtRocket::kAllAxialMethods)
    {
        if (QtRocket::axialMethodName(method) == name)
        {
            return method;
        }
    }
    return std::nullopt;
}

/// The golden material of a component's details.
[[nodiscard]] Material material(const json& details)
{
    const json& golden = details.at("material");
    return Material::newMaterial(Material::Type::BULK, golden.at("name").get<std::string>(),
                                 number(golden.at("density")), true);
}

// ================================================================================ rebuilding

/// The body tube the golden @p parent describes: its length, settled outer radius and wall.
[[nodiscard]] std::unique_ptr<BodyTube> makeBodyTube(const json& parent)
{
    const json&  details = parent.at("details");
    const double length  = number(parent.at("length"));
    const double radius  = number(details.at("outerRadius"));
    if (details.at("filled").get<bool>())
    {
        return std::make_unique<BodyTube>(length, radius, true);
    }
    return std::make_unique<BodyTube>(length, radius, number(details.at("thickness")));
}

/// Calls @p set with a value clearly different from @p value and then with @p value: the setters
/// that compare with MathUtil::equals() ignore a value within 1e-8 of the current one, which
/// would leave the component up to that far from the golden value.
template <class Setter>
void setExactly(const Setter& set, double value)
{
    set(value + 1.0);
    set(value);
}

/// Sets the material, the overrides and the axial position the golden @p expected records on
/// @p component, which is in its parent already.
void applyCommon(ExternalComponent& component, const json& expected, Mismatches& m)
{
    component.setMaterial(material(expected.at("details")));

    const json& overrides = expected.at("overrides");
    if (overrides.at("massOverridden").get<bool>())
    {
        component.setMassOverridden(true);
        component.setOverrideMass(number(overrides.at("overrideMass")));
    }
    if (overrides.at("cgOverridden").get<bool>())
    {
        component.setCGOverridden(true);
        component.setOverrideCGX(number(overrides.at("overrideCGX")));
    }

    const std::optional<AxialMethod> method =
        axialMethod(expected.at("axialMethod").get<std::string>());
    if (!method)
    {
        m.problem(std::format("unknown axial method {}", expected.at("axialMethod").dump()));
        return;
    }
    component.setAxialMethod(*method);
    component.setAxialOffset(number(expected.at("axialOffset")));
}

/// A launch lug as the golden @p expected describes it, on @p body.
[[nodiscard]] LaunchLug& addLaunchLug(BodyTube& body, const json& expected, Mismatches& m)
{
    const json& details = expected.at("details");
    LaunchLug&  lug     = body.addChild(std::make_unique<LaunchLug>());
    setExactly([&lug](double v) { lug.setLength(v); }, number(expected.at("length")));
    const double radius    = number(details.at("outerRadius"));
    const double thickness = number(details.at("thickness"));
    setExactly([&lug](double v) { lug.setOuterRadius(v); }, radius);
    // The wall is clamped to 0 ... the radius: come from the far end of that range.
    lug.setThickness(thickness > radius / 2 ? 0.0 : radius);
    lug.setThickness(thickness);
    const double angle = number(details.at("angleOffset"));
    lug.setAngleOffset(angle > 0 ? angle - 1.0 : angle + 1.0);
    lug.setAngleOffset(angle);
    lug.setInstanceCount(expected.at("instanceCount").get<int>());
    setExactly([&lug](double v) { lug.setInstanceSeparation(v); },
               number(details.at("instanceSeparation")));
    applyCommon(lug, expected, m);

    m.exact("set outerRadius", radius, lug.getOuterRadius());
    m.exact("set thickness", thickness, lug.getThickness());
    m.exact("set angleOffset", angle, lug.getAngleOffset());
    return lug;
}

/// The value of the child element @p child of the re-saved rail button or tube fin set @p node;
/// 0, with a problem noted, when there is none.
[[nodiscard]] double requiredNumber(const pugi::xml_node& node, const char* child, Mismatches& m)
{
    const std::optional<double> value = resaveNumber(node, child);
    if (!value)
    {
        m.problem(std::format("the re-saved design has no <{}> for this component", child));
    }
    return value.value_or(0.0);
}

/// A rail button as the golden @p expected and its element of the re-saved design describe it,
/// on @p body.
[[nodiscard]] RailButton& addRailButton(BodyTube& body, const json& expected,
                                        const pugi::xml_document& resave, Mismatches& m)
{
    const json&          details = expected.at("details");
    const pugi::xml_node saved =
        resaveElement(resave, "railbutton", expected.at("id").get<std::string>());
    const double outerDiameter = requiredNumber(saved, "outerdiameter", m);
    const double innerDiameter = requiredNumber(saved, "innerdiameter", m);
    const double totalHeight   = requiredNumber(saved, "height", m);
    const double baseHeight    = requiredNumber(saved, "baseheight", m);
    const double flangeHeight  = requiredNumber(saved, "flangeheight", m);
    const double screwHeight   = requiredNumber(saved, "screwheight", m);

    RailButton& button = body.addChild(std::make_unique<RailButton>());
    button.setOuterDiameter(outerDiameter);
    button.setInnerDiameter(innerDiameter);
    // The heights limit each other: make room first.
    button.setFlangeHeight(0);
    button.setBaseHeight(0);
    button.setTotalHeight(totalHeight);
    button.setFlangeHeight(flangeHeight);
    button.setBaseHeight(baseHeight);
    button.setScrewHeight(screwHeight);
    const double angle = number(details.at("angleOffset"));
    button.setAngleOffset(angle > 0 ? angle - 1.0 : angle + 1.0);
    button.setAngleOffset(angle);
    button.setInstanceCount(expected.at("instanceCount").get<int>());
    setExactly([&button](double v) { button.setInstanceSeparation(v); },
               number(details.at("instanceSeparation")));
    applyCommon(button, expected, m);

    m.exact("set outerDiameter", outerDiameter, button.getOuterDiameter());
    m.exact("set innerDiameter", innerDiameter, button.getInnerDiameter());
    m.exact("set totalHeight", totalHeight, button.getTotalHeight());
    m.exact("set baseHeight", baseHeight, button.getBaseHeight());
    m.exact("set flangeHeight", flangeHeight, button.getFlangeHeight());
    m.exact("set screwHeight", screwHeight, button.getScrewHeight());
    m.exact("set angleOffset", angle, button.getAngleOffset());
    return button;
}

/// A tube fin set as the golden @p expected and its element of the re-saved design describe it,
/// on @p body.
[[nodiscard]] TubeFinSet& addTubeFinSet(BodyTube& body, const json& expected,
                                        const pugi::xml_document& resave, Mismatches& m)
{
    const json&          details = expected.at("details");
    const pugi::xml_node saved =
        resaveElement(resave, "tubefinset", expected.at("id").get<std::string>());
    const std::string_view savedRadius = saved.child_value("radius");
    if (savedRadius.empty())
    {
        m.problem("the re-saved design has no <radius> for this component");
    }

    TubeFinSet& fins = body.addChild(std::make_unique<TubeFinSet>());
    fins.setFinCount(details.at("finCount").get<int>());
    setExactly([&fins](double v) { fins.setLength(v); }, number(expected.at("length")));
    const double rotation = number(details.at("baseRotation"));
    fins.setBaseRotation(rotation > 0 ? rotation - 1.0 : rotation + 1.0);
    fins.setBaseRotation(rotation);
    if (savedRadius != "auto")
    {
        fins.setOuterRadius(number(details.at("outerRadius")));
    }
    applyCommon(fins, expected, m);
    // Last: the thickness is clamped to the outer radius, which an automatic one takes from the
    // body at the fin set's position.
    fins.setThickness(number(details.at("thickness")));

    m.exact("set baseRotation", rotation, fins.getBaseRotation());
    m.exact("automatic radius", savedRadius == "auto" ? 1.0 : 0.0,
            fins.isOuterRadiusAutomatic() ? 1.0 : 0.0);
    return fins;
}

// ================================================================================= comparing

/// Compares what every attachment has with the golden @p expected entry: the volume, mass, CG,
/// unit inertias, bounds, instances, position and angle and radius methods of @p actual.
template <class Component>
void compareAttachment(Mismatches& m, const json& expected, const Component& actual)
{
    const json& details = expected.at("details");
    m.relative("length", number(expected.at("length")), actual.getLength());
    m.position("position", coordinate(expected.at("position")), actual.getPosition());

    m.relative("componentVolume", number(details.at("componentVolume")),
               actual.getComponentVolume());
    m.relative("componentMass", number(expected.at("componentMass")), actual.getComponentMass());
    m.cg("componentCG", coordinate(expected.at("componentCG")), actual.getComponentCG());
    m.relative("longitudinalUnitInertia", number(expected.at("longitudinalUnitInertia")),
               actual.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", number(expected.at("rotationalUnitInertia")),
               actual.getRotationalUnitInertia());

    m.positions("componentBounds", expected.at("componentBounds"), actual.getComponentBounds());
    const BoundingBox box = actual.getInstanceBoundingBox();
    m.position("instanceBoundingBox.min", coordinate(details.at("instanceBoundingBox").at("min")),
               box.min());
    m.position("instanceBoundingBox.max", coordinate(details.at("instanceBoundingBox").at("max")),
               box.max());

    if (expected.at("instanceCount").get<int>() != actual.getInstanceCount())
    {
        m.problem(std::format("instanceCount: expected {}, got {}",
                              expected.at("instanceCount").get<int>(), actual.getInstanceCount()));
    }
    m.positions("instanceOffsets", expected.at("instanceOffsets"), actual.getInstanceOffsets());
    m.angles("instanceAngles", expected.at("instanceAngles"), actual.getInstanceAngles());
    m.positions("instanceLocations", expected.at("instanceLocations"),
                actual.getInstanceLocations());

    // How the component is placed around the axis. The rebuild sets none of these: a launch lug's
    // and a rail button's angle method is always RELATIVE, a tube fin set's is FIXED unless it
    // is set, and none of the three has a radius method or offset of its own.
    m.same("angleMethod", details.at("angleMethod").get<std::string>(),
           QtRocket::angleMethodName(actual.getAngleMethod()));
    m.same("radiusMethod", details.at("radiusMethod").get<std::string>(),
           QtRocket::radiusMethodName(actual.getRadiusMethod()));
    m.exact("radiusOffset", number(details.at("radiusOffset")), actual.getRadiusOffset());

    // With the overrides applied.
    m.relative("mass", number(expected.at("mass")), actual.getMass());
    m.relative("sectionMass", number(expected.at("sectionMass")), actual.getSectionMass());
    m.cg("cg", coordinate(expected.at("cg")), actual.getCG());
    m.relative("longitudinalInertia", number(expected.at("longitudinalInertia")),
               actual.getLongitudinalInertia());
    m.relative("rotationalInertia", number(expected.at("rotationalInertia")),
               actual.getRotationalInertia());
}

/// Rebuilds the golden attachment @p expected of @p input on its parent body tube and compares
/// it, counting it in @p compared.
void compareOne(const Input& input, const json& expected, Mismatches& m, Counts& compared)
{
    const std::string type   = expected.at("type").get<std::string>();
    const std::string path   = expected.at("path").get<std::string>();
    const json*       parent = componentAt(input.geometry, parentPath(path));
    if (parent == nullptr || parent->at("type").get<std::string>() != "BodyTube")
    {
        // Only a body tube accepts these components, in OpenRocket as here.
        m.problem(std::format("the parent {} is not a body tube", parentPath(path)));
        return;
    }

    Rocket      rocket;
    auto&       stage   = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body    = stage.addChild(makeBodyTube(*parent));
    const json& details = expected.at("details");
    if (type == "LaunchLug")
    {
        const LaunchLug& lug = addLaunchLug(body, expected, m);
        rocket.enableEvents();
        compareAttachment(m, expected, lug);
        m.absolute("innerRadius", number(details.at("innerRadius")), lug.getInnerRadius());
        m.exact("instanceSeparation", number(details.at("instanceSeparation")),
                lug.getInstanceSeparation());
        compared.launchLugs++;
    }
    else if (type == "RailButton")
    {
        const RailButton& button = addRailButton(body, expected, input.resave, m);
        rocket.enableEvents();
        compareAttachment(m, expected, button);
        m.exact("instanceSeparation", number(details.at("instanceSeparation")),
                button.getInstanceSeparation());
        compared.railButtons++;
    }
    else
    {
        const TubeFinSet& fins = addTubeFinSet(body, expected, input.resave, m);
        rocket.enableEvents();
        compareAttachment(m, expected, fins);
        m.absolute("outerRadius", number(details.at("outerRadius")), fins.getOuterRadius());
        m.absolute("innerRadius", number(details.at("innerRadius")), fins.getInnerRadius());
        m.absolute("thickness", number(details.at("thickness")), fins.getThickness());
        m.absolute("bodyRadius", number(details.at("bodyRadius")), fins.getBodyRadius());
        compared.tubeFinSets++;
    }
}

/// Whether the golden component type @p type is one this file rebuilds.
[[nodiscard]] bool isAttachment(std::string_view type)
{
    return type == "LaunchLug" || type == "RailButton" || type == "TubeFinSet";
}

/// Rebuilds and compares every launch lug, rail button and tube fin set of the golden input
/// @p name.
[[nodiscard]] Outcome compareInput(const std::string& name)
{
    Outcome                                        outcome;
    const QtRocket::Result<std::unique_ptr<Input>> input = loadInput(name);
    if (!input)
    {
        outcome.report = std::format("{}: {}\n", name, input.error().message);
        return outcome;
    }
    for (const json& component : (*input)->geometry.at("components"))
    {
        const std::string type = component.at("type").get<std::string>();
        if (!isAttachment(type))
        {
            continue;
        }
        Mismatches m(std::format("{} {} {} \"{}\"", name, type,
                                 component.at("path").get<std::string>(),
                                 component.at("name").get<std::string>()));
        compareOne(**input, component, m, outcome.compared);
        outcome.report += m.report();
    }
    return outcome;
}

// ===================================================================================== tests

class AttachmentGeometryGolden : public ::testing::TestWithParam<std::string>
{ };

TEST_P(AttachmentGeometryGolden, RebuiltOnTheirBodyTube)
{
    const Outcome outcome = compareInput(GetParam());
    EXPECT_EQ(outcome.report, "");
    RecordProperty("launchLugs", outcome.compared.launchLugs);
    RecordProperty("railButtons", outcome.compared.railButtons);
    RecordProperty("tubeFinSets", outcome.compared.tubeFinSets);
}

INSTANTIATE_TEST_SUITE_P(Inputs, AttachmentGeometryGolden, ::testing::ValuesIn(inputNames()),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             std::string name = paramInfo.param;
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });

TEST(AttachmentGeometryGoldenCoverage, EveryAttachmentIsCompared)
{
    const std::vector<std::string> names = inputNames();
    ASSERT_EQ(names.size(), 29U);
    Counts total;
    int    inputsWithAttachments = 0;
    for (const std::string& name : names)
    {
        const Outcome outcome = compareInput(name);
        total.launchLugs += outcome.compared.launchLugs;
        total.railButtons += outcome.compared.railButtons;
        total.tubeFinSets += outcome.compared.tubeFinSets;
        if (outcome.compared.launchLugs + outcome.compared.railButtons +
                outcome.compared.tubeFinSets >
            0)
        {
            inputsWithAttachments++;
        }
    }
    EXPECT_EQ(total.launchLugs, 18) << "the launch lugs the per-input tests rebuild";
    EXPECT_EQ(total.railButtons, 5) << "the rail buttons";
    EXPECT_EQ(total.tubeFinSets, 1) << "the tube fin set";
    EXPECT_EQ(inputsWithAttachments, 19);
}

TEST(AttachmentGeometryGoldenCoverage, AnUnknownInputIsReported)
{
    const Outcome outcome = compareInput("no-such-input");
    EXPECT_EQ(outcome.report, "no-such-input: no input no-such-input\n");
    EXPECT_EQ(
        outcome.compared.launchLugs + outcome.compared.railButtons + outcome.compared.tubeFinSets,
        0);
}

}  // namespace
