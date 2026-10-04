// Fin geometry golden tests: the planar fin sets (trapezoidal, elliptical, freeform) compared
// with OpenRocket's own values in tests/data/goldens/<input>/geometry.json
// (tools/openrocket-goldens).
//
// FinGeometryGolden rebuilds every fin set of every golden input on its parent body: the body is
// rebuilt from its own golden entry (GoldenBodies.h) as the only body of a one-stage rocket with
// events enabled, and the fin set from the dimensions geometry.json records, at its axial method
// and offset. It then compares the outlines (fin, root and tab points), the span, the planform
// area, the volume, the mass, the CG, the unit inertias, the component bounds, the instance
// bounding box and the instance offsets, locations and angles. A fin set's geometry depends on
// its own fields and its parent's profile only, so this covers the 48 fin sets of the 29 inputs
// without rebuilding their rockets.
//
// Two values come from elsewhere:
// - The tab's offset method is not in geometry.json. It is read from OpenRocket's re-save of the
//   design (<input>/resave/rocket.ork, the <tabposition relativeto="..."> of the fin set with
//   the same id), which holds it for every fin set with a tab; a fin set without a tab keeps the
//   method of a new fin set (MIDDLE), which the tab points confirm.
// - FinSet::getComponentBounds() adds the fin set's absolute x location to the upper x bound.
//   The rebuilt body starts at x = 0 rather than at its place in the rocket, so that bound is
//   compared with each side's own location taken off.
//
// The cross-section's name in the re-saved design (<crosssection>) is compared too: it must be
// what the saver's orkName() gives and read back through finCrossSectionFromOrkName().
//
// What this cannot rebuild, and reports instead of comparing (no golden input holds either):
// - a fin set positioned ABSOLUTE, whose offset counts from the tip of the rocket, the body being
//   rebuilt at x = 0;
// - a canted freeform fin set: its recorded finPoints (and the points of the re-saved design)
//   have their ends on the canted root, so they are not the outline the fin set was given.
// The golden inputs also hold no fin set with fillets, positioned MIDDLE or with an angle method
// other than RELATIVE. tests/core/rocket/fin_placement_tests.cpp and FinSetTests.cpp pin those
// cases with OpenRocket.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenBodies.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"

namespace
{

using nlohmann::json;
using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::FinCrossSection;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::Rocket;
using QtRocket::SymmetricComponent;
using QtRocket::TrapezoidFinSet;
using QtRocket::Test::findGoldenComponent;
using QtRocket::Test::goldenAxialMethod;
using QtRocket::Test::goldenCoordinate;
using QtRocket::Test::goldenGeometry;
using QtRocket::Test::goldenInputNames;
using QtRocket::Test::goldenMaterial;
using QtRocket::Test::GoldenMismatches;
using QtRocket::Test::goldenValue;
using QtRocket::Test::loadGoldenResave;
using QtRocket::Test::rebuildGoldenBody;
using QtRocket::Test::savedGoldenComponent;

/// How many fin sets of each class were compared.
struct FinCounts
{
    int trapezoid{0};
    int elliptical{0};
    int freeform{0};

    [[nodiscard]] int total() const noexcept { return trapezoid + elliptical + freeform; }
};

/// What comparing the fin sets of one input gave: the fin sets compared, and one report per fin
/// set that differs or could not be rebuilt.
struct FinComparison
{
    FinCounts                counts;
    std::vector<std::string> reports;
};

/// A fin set rebuilt on its parent body, in a rocket of its own.
struct RebuiltFinSet
{
    std::unique_ptr<Rocket> rocket;
    SymmetricComponent*     body{nullptr};
    FinSet*                 fins{nullptr};
};

/// Whether the golden component @p type is a planar fin set.
[[nodiscard]] bool isFinSetType(std::string_view type)
{
    return type == "TrapezoidFinSet" || type == "EllipticalFinSet" || type == "FreeformFinSet";
}

/// The angle method with the constant name @p name ("RELATIVE").
[[nodiscard]] std::optional<AngleMethod> angleMethodNamed(std::string_view name)
{
    for (const AngleMethod method : QtRocket::kAllAngleMethods)
    {
        if (QtRocket::angleMethodName(method) == name)
        {
            return method;
        }
    }
    return std::nullopt;
}

/// The cross-section with the constant name @p name ("ROUNDED").
[[nodiscard]] std::optional<FinCrossSection> crossSectionNamed(std::string_view name)
{
    for (const FinCrossSection crossSection : QtRocket::kAllFinCrossSections)
    {
        if (QtRocket::finCrossSectionName(crossSection) == name)
        {
            return crossSection;
        }
    }
    return std::nullopt;
}

/// The golden list of [x, y, z] points @p value.
[[nodiscard]] std::vector<Coordinate> goldenPoints(const json& value)
{
    std::vector<Coordinate> points;
    points.reserve(value.size());
    for (const json& point : value)
    {
        points.push_back(goldenCoordinate(point));
    }
    return points;
}

/// The tab offset method of the fin set with the id @p id in the re-saved design: the last
/// <tabposition relativeto="..."> that names an axial method (the saver writes the pre-1.1
/// front/center/end form first), or nullopt when the fin set has none (it has no tab) or is not
/// in the file.
[[nodiscard]] std::optional<AxialMethod> savedTabOffsetMethod(const pugi::xml_document& resave,
                                                              const std::string&        id)
{
    const pugi::xml_node       finSet = savedGoldenComponent(resave, id);
    std::optional<AxialMethod> method;
    for (const pugi::xml_node& position : finSet.children("tabposition"))
    {
        const std::optional<AxialMethod> named =
            QtRocket::axialMethodFromOrkName(position.attribute("relativeto").value());
        if (named)
        {
            method = named;
        }
    }
    return method;
}

/// A fin set of the class and dimensions the golden @p component records (a freeform fin set
/// still with its default outline), detached; nullptr and a note for a class or value this does
/// not know.
[[nodiscard]] std::unique_ptr<FinSet> makeFinSet(const json& component, GoldenMismatches& m)
{
    const std::string type      = component.at("type").get<std::string>();
    const json&       details   = component.at("details");
    const double      thickness = goldenValue(details.at("thickness"));

    std::unique_ptr<FinSet> fins;
    if (type == "TrapezoidFinSet")
    {
        auto trapezoid = std::make_unique<TrapezoidFinSet>();
        trapezoid->setFinShape(
            goldenValue(details.at("rootChord")), goldenValue(details.at("tipChord")),
            goldenValue(details.at("sweep")), goldenValue(details.at("height")), thickness);
        fins = std::move(trapezoid);
    }
    else if (type == "EllipticalFinSet")
    {
        auto elliptical = std::make_unique<EllipticalFinSet>();
        elliptical->setHeight(goldenValue(details.at("height")));
        elliptical->setLength(goldenValue(component.at("length")));
        fins = std::move(elliptical);
    }
    else if (type == "FreeformFinSet")
    {
        fins = std::make_unique<FreeformFinSet>();
    }
    else
    {
        m.note("not a fin set: " + type);
        return nullptr;
    }

    const std::optional<FinCrossSection> crossSection =
        crossSectionNamed(details.at("crossSection").get<std::string>());
    const std::optional<AngleMethod> angleMethod =
        angleMethodNamed(details.at("angleMethod").get<std::string>());
    if (!crossSection || !angleMethod)
    {
        m.note("unknown cross-section or angle method");
        return nullptr;
    }

    fins->setThickness(thickness);
    fins->setFinCount(details.at("finCount").get<int>());
    fins->setCrossSection(*crossSection);
    fins->setCantAngle(goldenValue(details.at("cantAngle")));
    fins->setAngleMethod(*angleMethod);
    fins->setAngleOffset(goldenValue(details.at("angleOffset")));
    fins->setMaterial(goldenMaterial(details.at("material")));
    fins->setFilletMaterial(goldenMaterial(details.at("filletMaterial")));
    fins->setFilletRadius(goldenValue(details.at("filletRadius")));
    return fins;
}

/// Rebuilds the fin set the golden @p component describes on the body its @p parent entry
/// describes; an empty result and a note when it cannot.
[[nodiscard]] RebuiltFinSet rebuildFinSet(const json& component, const json& parent,
                                          const pugi::xml_document& resave, GoldenMismatches& m)
{
    std::unique_ptr<SymmetricComponent> body = rebuildGoldenBody(parent);
    if (!body)
    {
        m.note("the parent is not a body component: " + parent.at("type").get<std::string>());
        return {};
    }
    std::unique_ptr<FinSet>          made = makeFinSet(component, m);
    const std::optional<AxialMethod> axialMethod =
        goldenAxialMethod(component.at("axialMethod").get<std::string>());
    if (!made || !axialMethod)
    {
        m.note("the fin set cannot be rebuilt");
        return {};
    }
    // What this harness cannot rebuild (see the top of the file): reported, never skipped.
    if (*axialMethod == AxialMethod::ABSOLUTE)
    {
        m.note(
            "positioned ABSOLUTE: the offset counts from the tip of the rocket, and the body "
            "is rebuilt at x = 0");
        return {};
    }
    if (made->kind() == QtRocket::ComponentKind::FREEFORM_FIN_SET && made->getCantAngle() != 0)
    {
        m.note("a canted freeform fin set: its finPoints are not the outline it was given");
        return {};
    }

    RebuiltFinSet rebuilt;
    rebuilt.rocket = std::make_unique<Rocket>();
    auto& stage    = rebuilt.rocket->addChild(std::make_unique<AxialStage>());
    rebuilt.body   = &stage.addChild(std::move(body));
    rebuilt.rocket->enableEvents();
    if (!rebuilt.body->isCompatible(made->kind()))
    {
        m.note("the parent does not accept this fin set");
        return {};
    }
    FinSet& fins = rebuilt.body->addChild(std::move(made));
    rebuilt.fins = &fins;

    fins.setAxialMethod(*axialMethod);
    fins.setAxialOffset(goldenValue(component.at("axialOffset")));

    const json& details = component.at("details");
    if (auto* freeform = dynamic_cast<FreeformFinSet*>(&fins))
    {
        freeform->setPoints(goldenPoints(details.at("finPoints")), false);
    }

    // The tab, once the fin has its length: its position follows from the offset.
    const std::optional<AxialMethod> tabMethod =
        savedTabOffsetMethod(resave, component.at("id").get<std::string>());
    fins.setTabOffsetMethod(tabMethod.value_or(AxialMethod::MIDDLE));
    fins.setTabHeight(goldenValue(details.at("tabHeight")), false);
    fins.setTabLength(goldenValue(details.at("tabLength")), false);
    fins.setTabOffset(goldenValue(details.at("tabOffset")));
    if (fins.hasTab() && !tabMethod)
    {
        m.note("the fin set has a tab, but the re-saved design has no tab position for it");
    }
    return rebuilt;
}

/// @p actual against the golden list of numbers @p expected, each within the relative tolerance.
void compareNumbers(GoldenMismatches& m, std::string_view field, const json& expected,
                    std::span<const double> actual)
{
    if (expected.size() != actual.size())
    {
        m.note(std::format("{}: {} values expected, {} computed", field, expected.size(),
                           actual.size()));
        return;
    }
    for (std::size_t i = 0; i < actual.size(); i++)
    {
        m.relative(std::format("{}[{}]", field, i), goldenValue(expected.at(i)), actual[i]);
    }
}

/// The component bounds: the upper x bound less each side's own absolute x location (see the
/// comment at the top of the file).
void compareBounds(GoldenMismatches& m, const json& expected, const FinSet& fins)
{
    const json&                   expectedBounds = expected.at("componentBounds");
    const std::vector<Coordinate> bounds         = fins.getComponentBounds();
    if (expectedBounds.size() != 2 || bounds.size() != 2)
    {
        m.note(std::format("componentBounds: {} points expected, {} computed",
                           expectedBounds.size(), bounds.size()));
        return;
    }
    const double goldenLocation = goldenCoordinate(expected.at("componentLocations").at(0)).x;
    const double actualLocation = fins.getComponentLocations().front().x;
    m.position("componentBounds[0]", goldenCoordinate(expectedBounds.at(0)), bounds[0]);
    m.position("componentBounds[1] (less the location)",
               goldenCoordinate(expectedBounds.at(1)).sub(goldenLocation, 0, 0),
               bounds[1].sub(actualLocation, 0, 0));
}

/// The dimensions and settings the fin set was rebuilt from, read back.
void compareSettings(GoldenMismatches& m, const json& expected, const FinSet& fins)
{
    const json& details = expected.at("details");
    m.text("type", expected.at("type").get<std::string>(), QtRocket::className(fins.kind()));
    m.relative("length", goldenValue(expected.at("length")), fins.getLength());
    m.text("axialMethod", expected.at("axialMethod").get<std::string>(),
           QtRocket::axialMethodName(fins.getAxialMethod()));
    m.absolute("axialOffset", goldenValue(expected.at("axialOffset")), fins.getAxialOffset());
    m.position("position", goldenCoordinate(expected.at("position")), fins.getPosition());

    m.relative("finCount", details.at("finCount").get<int>(), fins.getFinCount());
    m.relative("instanceCount", expected.at("instanceCount").get<int>(), fins.getInstanceCount());
    m.relative("thickness", goldenValue(details.at("thickness")), fins.getThickness());
    m.text("crossSection", details.at("crossSection").get<std::string>(),
           QtRocket::finCrossSectionName(fins.getCrossSection()));
    m.relative("cantAngle", goldenValue(details.at("cantAngle")), fins.getCantAngle());
    m.relative("baseRotation", goldenValue(details.at("baseRotation")), fins.getBaseRotation());
    m.relative("angleOffset", goldenValue(details.at("angleOffset")), fins.getAngleOffset());
    m.text("angleMethod", details.at("angleMethod").get<std::string>(),
           QtRocket::angleMethodName(fins.getAngleMethod()));
    m.text("radiusMethod", details.at("radiusMethod").get<std::string>(),
           QtRocket::radiusMethodName(fins.getRadiusMethod()));
    m.absolute("radiusOffset", goldenValue(details.at("radiusOffset")), fins.getRadiusOffset());
    m.relative("tabHeight", goldenValue(details.at("tabHeight")), fins.getTabHeight());
    m.relative("tabLength", goldenValue(details.at("tabLength")), fins.getTabLength());
    m.absolute("tabOffset", goldenValue(details.at("tabOffset")), fins.getTabOffset());
    m.relative("filletRadius", goldenValue(details.at("filletRadius")), fins.getFilletRadius());
    m.text("material", details.at("material").at("name").get<std::string>(),
           fins.getMaterial().getName());
    m.text("filletMaterial", details.at("filletMaterial").at("name").get<std::string>(),
           fins.getFilletMaterial().getName());

    if (const auto* trapezoid = dynamic_cast<const TrapezoidFinSet*>(&fins))
    {
        m.relative("rootChord", goldenValue(details.at("rootChord")), trapezoid->getRootChord());
        m.relative("tipChord", goldenValue(details.at("tipChord")), trapezoid->getTipChord());
        m.relative("height", goldenValue(details.at("height")), trapezoid->getHeight());
        m.relative("sweep", goldenValue(details.at("sweep")), trapezoid->getSweep());
        m.relative("sweepAngle", goldenValue(details.at("sweepAngle")), trapezoid->getSweepAngle());
    }
    else if (const auto* elliptical = dynamic_cast<const EllipticalFinSet*>(&fins))
    {
        m.relative("height", goldenValue(details.at("height")), elliptical->getHeight());
    }
}

/// The cross-section as OpenRocket's re-save of the design spells it (<crosssection>), against
/// the name the saver writes for the rebuilt fin set's and what the loader reads the text as.
void compareSavedCrossSection(GoldenMismatches& m, const pugi::xml_document& resave,
                              const json& expected, const FinSet& fins)
{
    const pugi::xml_node saved = savedGoldenComponent(resave, expected.at("id").get<std::string>());
    if (!saved)
    {
        m.note("the fin set is not in the re-saved design");
        return;
    }
    const std::string_view text = saved.child_value("crosssection");
    m.text("crosssection of the re-saved design", text, QtRocket::orkName(fins.getCrossSection()));
    if (QtRocket::finCrossSectionFromOrkName(text) != fins.getCrossSection())
    {
        m.note(
            std::format("<crosssection>{}</crosssection> is not read as the cross-section", text));
    }
}

/// The geometry and mass properties of @p fins against the golden @p expected entry.
void compareFinSet(GoldenMismatches& m, const json& expected, const FinSet& fins)
{
    compareSettings(m, expected, fins);

    const json& details = expected.at("details");
    m.positions("finPoints", details.at("finPoints"), fins.getFinPoints());
    m.positions("rootPoints", details.at("rootPoints"), fins.getRootPoints());
    m.positions("tabPoints", details.at("tabPoints"), fins.getTabPoints());
    m.relative("span", goldenValue(details.at("span")), fins.getSpan());
    m.relative("bodyRadius", goldenValue(details.at("bodyRadius")), fins.getBodyRadius());
    m.relative("planformArea", goldenValue(details.at("planformArea")), fins.getPlanformArea());
    m.relative("componentVolume", goldenValue(details.at("componentVolume")),
               fins.getComponentVolume());

    m.relative("componentMass", goldenValue(expected.at("componentMass")), fins.getComponentMass());
    m.cg("componentCG", goldenCoordinate(expected.at("componentCG")), fins.getComponentCG());
    m.relative("longitudinalUnitInertia", goldenValue(expected.at("longitudinalUnitInertia")),
               fins.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", goldenValue(expected.at("rotationalUnitInertia")),
               fins.getRotationalUnitInertia());

    compareBounds(m, expected, fins);
    const BoundingBox box = fins.getInstanceBoundingBox();
    m.position("instanceBoundingBox.min",
               goldenCoordinate(details.at("instanceBoundingBox").at("min")), box.min());
    m.position("instanceBoundingBox.max",
               goldenCoordinate(details.at("instanceBoundingBox").at("max")), box.max());

    m.positions("instanceOffsets", expected.at("instanceOffsets"), fins.getInstanceOffsets());
    m.positions("instanceLocations", expected.at("instanceLocations"), fins.getInstanceLocations());
    compareNumbers(m, "instanceAngles", expected.at("instanceAngles"), fins.getInstanceAngles());
}

/// Counts a compared fin set of the golden class @p type.
void count(FinCounts& counts, std::string_view type)
{
    if (type == "TrapezoidFinSet")
    {
        counts.trapezoid++;
    }
    else if (type == "EllipticalFinSet")
    {
        counts.elliptical++;
    }
    else if (type == "FreeformFinSet")
    {
        counts.freeform++;
    }
}

/// Rebuilds and compares the fin set @p component of the input @p name, whose geometry document
/// is @p geometry; its report is empty when everything matches. @p compared is set once the fin
/// set has been rebuilt and compared.
[[nodiscard]] std::string compareOne(const std::string& name, const json& geometry,
                                     const json& component, const pugi::xml_document& resave,
                                     bool& compared)
{
    const std::string path = component.at("path").get<std::string>();
    GoldenMismatches m(std::format("{} {} {} \"{}\"", name, component.at("type").get<std::string>(),
                                   path, component.at("name").get<std::string>()));

    // The parent: the entry whose path is the fin set's without its last element.
    const std::string parentPath = path.substr(0, path.rfind('/'));
    const json* const parent     = findGoldenComponent(geometry, parentPath);
    if (parent == nullptr)
    {
        m.note("no parent entry " + parentPath);
        return m.report();
    }

    const RebuiltFinSet rebuilt = rebuildFinSet(component, *parent, resave, m);
    if (rebuilt.fins == nullptr)
    {
        return m.report();
    }
    compareFinSet(m, component, *rebuilt.fins);
    compareSavedCrossSection(m, resave, component, *rebuilt.fins);
    compared = true;
    return m.report();
}

/// Rebuilds and compares every fin set of the golden input @p name.
[[nodiscard]] FinComparison compareFinSetsOf(const std::string& name)
{
    FinComparison                       comparison;
    const QtRocket::Result<const json*> geometry = goldenGeometry(name);
    if (!geometry)
    {
        comparison.reports.push_back(name + ": " + geometry.error().message);
        return comparison;
    }
    // The re-saved design; an empty document (and a report) when it cannot be read.
    const pugi::xml_document empty;
    const auto               loaded = loadGoldenResave(name);
    if (!loaded)
    {
        comparison.reports.push_back(name + ": " + loaded.error().message);
    }
    const pugi::xml_document& resave = loaded ? **loaded : empty;

    for (const json& component : (*geometry)->at("components"))
    {
        const std::string type = component.at("type").get<std::string>();
        if (!isFinSetType(type))
        {
            continue;
        }
        bool        compared = false;
        std::string report   = compareOne(name, **geometry, component, resave, compared);
        if (compared)
        {
            count(comparison.counts, type);
        }
        if (!report.empty())
        {
            comparison.reports.push_back(std::move(report));
        }
    }
    return comparison;
}

/// The number of planar fin sets in the golden geometry of the input @p name.
[[nodiscard]] int countFinSets(const std::string& name)
{
    const QtRocket::Result<const json*> geometry = goldenGeometry(name);
    if (!geometry)
    {
        return -1;
    }
    int found = 0;
    for (const json& component : (*geometry)->at("components"))
    {
        if (isFinSetType(component.at("type").get<std::string>()))
        {
            found++;
        }
    }
    return found;
}

// ======================================================================= per golden input

class FinGeometryGolden : public ::testing::TestWithParam<std::string>
{ };

TEST_P(FinGeometryGolden, FinSetsRebuiltOnTheirBodies)
{
    const FinComparison comparison = compareFinSetsOf(GetParam());
    for (const std::string& report : comparison.reports)
    {
        ADD_FAILURE() << report;
    }
    EXPECT_EQ(comparison.counts.total(), countFinSets(GetParam()))
        << "every fin set of the input is compared";
    RecordProperty("finSets", comparison.counts.total());
}

INSTANTIATE_TEST_SUITE_P(Inputs, FinGeometryGolden, ::testing::ValuesIn(goldenInputNames()),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             std::string name = paramInfo.param;
                             for (char& c : name)
                             {
                                 if (c == '-')
                                 {
                                     c = '_';
                                 }
                             }
                             return name;
                         });

// The fin sets compared, per class, over all inputs: nothing is skipped silently.
TEST(FinGeometryGoldenCoverage, EveryFinSetIsCompared)
{
    const std::vector<std::string> names = goldenInputNames();
    ASSERT_EQ(names.size(), 29U);
    FinCounts total;
    int       inputsWithFins = 0;
    for (const std::string& name : names)
    {
        const FinCounts counts = compareFinSetsOf(name).counts;
        total.trapezoid += counts.trapezoid;
        total.elliptical += counts.elliptical;
        total.freeform += counts.freeform;
        if (counts.total() > 0)
        {
            inputsWithFins++;
        }
    }
    EXPECT_EQ(total.trapezoid, 43);
    EXPECT_EQ(total.freeform, 4);
    EXPECT_EQ(total.elliptical, 1);
    EXPECT_EQ(inputsWithFins, 26) << "every input but the three without planar fins";
}

}  // namespace
