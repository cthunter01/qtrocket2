// The model of a material combo box (Java: MaterialModel and the grouping of its
// GroupableAndSearchableComboBox).
//
// The first test ports MaterialModelTest (swing/src/test/java/info/openrocket/swing/gui/adaptors/
// MaterialModelTest.java).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (tools/openrocket-gui-pins/material-binding/probe/B2Probe.java, run on OpenRocket
// 5f164fd0e with JDK 17): MaterialModel(null, document, component, type) over a new rocket's
// document with a body tube in its stage and a parachute in the tube, after the user material
// "Aardvark resin" was added to the application's bulk materials, the document materials "Doc
// foam" (set on the tube) and "Zz custom" (added to the document) and "Doc string" (set as the
// parachute's line material). A row is the combo box's text of the material
// (MaterialComboBox.getDisplayString()) with its group and whether it is a document material; a
// group line is the combo box's menu of the group (its private itemGroupMap, read by reflection).
// The texts are generated from the probe's output (tools/openrocket-gui-pins/generate.sh
// material-binding).

#include "QtRocket/gui/binding/MaterialBinding.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using MaterialBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;

// probe: "BULK (with user and document materials)", 35 rows.
constexpr std::array<std::string_view, 35> kBulkRows{
    "ABS - 100% infill (1.05 g/cm\u00b3) [Plastics]",
    "ASA - 100% infill (1.05 g/cm\u00b3) [Plastics]",
    "(ud) Aardvark resin (1.5 g/cm\u00b3) [Plastics]",
    "Acrylic (1.19 g/cm\u00b3) [Plastics]",
    "Aluminum (2.7 g/cm\u00b3) [Metals]",
    "Balsa (0.17 g/cm\u00b3) [Woods]",
    "Basswood (0.5 g/cm\u00b3) [Woods]",
    "Birch (0.67 g/cm\u00b3) [Woods]",
    "Blue tube (1.3 g/cm\u00b3) [Composites]",
    "Brass (8.6 g/cm\u00b3) [Metals]",
    "Carbon fiber (1.78 g/cm\u00b3) [Composites]",
    "Cardboard (0.68 g/cm\u00b3) [PaperProducts]",
    "Cork (0.24 g/cm\u00b3) [Woods]",
    "Delrin (1.42 g/cm\u00b3) [Plastics]",
    "Depron (XPS) (0.04 g/cm\u00b3) [Foams]",
    "Fiberglass (1.85 g/cm\u00b3) [Composites]",
    "Kraft phenolic (0.95 g/cm\u00b3) [Composites]",
    "Maple (0.755 g/cm\u00b3) [Woods]",
    "Nylon (1.15 g/cm\u00b3) [Fibers]",
    "PETG - 100% infill (1.25 g/cm\u00b3) [Plastics]",
    "PLA - 100% infill (1.25 g/cm\u00b3) [Plastics]",
    "PVC (1.39 g/cm\u00b3) [Plastics]",
    "Paper (office) (0.82 g/cm\u00b3) [PaperProducts]",
    "Pine (0.53 g/cm\u00b3) [Woods]",
    "Plywood (birch) (0.63 g/cm\u00b3) [Woods]",
    "Polycarbonate (Lexan) (1.2 g/cm\u00b3) [Plastics]",
    "Polystyrene (1.05 g/cm\u00b3) [Plastics]",
    "Quantum tubing (1.05 g/cm\u00b3) [Plastics]",
    "Spruce (0.45 g/cm\u00b3) [Woods]",
    "Steel (7.85 g/cm\u00b3) [Metals]",
    "Styrofoam \"Blue foam\" (XPS) (0.032 g/cm\u00b3) [Foams]",
    "Styrofoam (generic EPS) (0.02 g/cm\u00b3) [Foams]",
    "Titanium (4.5 g/cm\u00b3) [Metals]",
    "(ud) Doc foam (0.03 g/cm\u00b3) [Foams, doc]",
    "(ud) Zz custom (0.9 g/cm\u00b3) [Custom, doc]",
};

// probe: "BULK groups".
constexpr std::array<std::string_view, 8> kBulkGroups{
    "Metals: Aluminum (2.7 g/cm\u00b3) | Brass (8.6 g/cm\u00b3) | Steel (7.85 g/cm\u00b3) | "
    "Titanium (4.5 g/cm\u00b3)",
    "Woods: Balsa (0.17 g/cm\u00b3) | Basswood (0.5 g/cm\u00b3) | Birch (0.67 g/cm\u00b3) | Cork "
    "(0.24 g/cm\u00b3) | Maple (0.755 g/cm\u00b3) | Pine (0.53 g/cm\u00b3) | Plywood (birch) (0.63 "
    "g/cm\u00b3) | Spruce (0.45 g/cm\u00b3)",
    "Plastics: ABS - 100% infill (1.05 g/cm\u00b3) | ASA - 100% infill (1.05 g/cm\u00b3) | (ud) "
    "Aardvark resin (1.5 g/cm\u00b3) | Acrylic (1.19 g/cm\u00b3) | Delrin (1.42 g/cm\u00b3) | PETG "
    "- 100% infill (1.25 g/cm\u00b3) | PLA - 100% infill (1.25 g/cm\u00b3) | PVC (1.39 g/cm\u00b3) "
    "| Polycarbonate (Lexan) (1.2 g/cm\u00b3) | Polystyrene (1.05 g/cm\u00b3) | Quantum tubing "
    "(1.05 g/cm\u00b3)",
    "PaperProducts: Cardboard (0.68 g/cm\u00b3) | Paper (office) (0.82 g/cm\u00b3)",
    "Foams: Depron (XPS) (0.04 g/cm\u00b3) | (ud) Doc foam (0.03 g/cm\u00b3) | Styrofoam \"Blue "
    "foam\" (XPS) (0.032 g/cm\u00b3) | Styrofoam (generic EPS) (0.02 g/cm\u00b3)",
    "Composites: Blue tube (1.3 g/cm\u00b3) | Carbon fiber (1.78 g/cm\u00b3) | Fiberglass (1.85 "
    "g/cm\u00b3) | Kraft phenolic (0.95 g/cm\u00b3)",
    "Fibers: Nylon (1.15 g/cm\u00b3)",
    "Custom: (ud) Zz custom (0.9 g/cm\u00b3)",
};

// probe: "LINE", 43 rows.
constexpr std::array<std::string_view, 43> kLineRows{
    "Braided nylon (2 mm, 1/16 in) (1 g/m) [Nylons]",
    "Braided nylon (3 mm, 1/8 in) (3.5 g/m) [Nylons]",
    "Elastic braided cord (flat 10 mm, 3/8 in) (3.81 g/m) [Elastics]",
    "Elastic braided cord (flat 13 mm, 1/2 in) (5.51 g/m) [Elastics]",
    "Elastic braided cord (flat 3.2 mm, 1/8 in) (1.06 g/m) [Elastics]",
    "Elastic braided cord (flat 4 mm, 5/32 in) (2 g/m) [Elastics]",
    "Elastic braided cord (flat 6.4 mm, 1/4 in) (2.54 g/m) [Elastics]",
    "Elastic braided cord (round 2 mm, 1/16 in) (3.5 g/m) [Elastics]",
    "Elastic braided cord (round 2.5 mm, 3/32 in) (3.8 g/m) [Elastics]",
    "Elastic cord (flat 12 mm, 1/2 in) (8 g/m) [Elastics]",
    "Elastic cord (flat 19 mm, 3/4 in) (1.2 g/m) [Elastics]",
    "Elastic cord (flat 25 mm, 1 in) (1.6 g/m) [Elastics]",
    "Elastic cord (flat 6 mm, 1/4 in) (4.3 g/m) [Elastics]",
    "Elastic cord (round 2 mm, 1/16 in) (1.8 g/m) [Elastics]",
    "Elastic rubber band (flat 3.2 mm, 1/8 in) (2.98 g/m) [Elastics]",
    "Elastic rubber band (flat 6.4 mm, 1/4 in) (6.13 g/m) [Elastics]",
    "Kevlar 12-strand (10 mm, 3/8 in) (59.5 g/m) [Kevlars]",
    "Kevlar 12-strand (11 mm, 7/16 in) (74.4 g/m) [Kevlars]",
    "Kevlar 12-strand (13 mm, 1/2 in) (116 g/m) [Kevlars]",
    "Kevlar 12-strand (14 mm, 9/16 in) (208 g/m) [Kevlars]",
    "Kevlar 12-strand (16 mm, 5/8 in) (287 g/m) [Kevlars]",
    "Kevlar 12-strand (19 mm, 3/4 in) (350 g/m) [Kevlars]",
    "Kevlar 12-strand (25 mm, 1 in) (457 g/m) [Kevlars]",
    "Kevlar 12-strand (3.2 mm, 1/8 in) (9.67 g/m) [Kevlars]",
    "Kevlar 12-strand (4.8 mm, 3/16 in) (17.9 g/m) [Kevlars]",
    "Kevlar 12-strand (6.4 mm, 1/4 in) (29.8 g/m) [Kevlars]",
    "Kevlar 12-strand (7.9 mm, 5/16 in) (44.6 g/m) [Kevlars]",
    "Kevlar thread 138  (0.4 mm, 1/64 in) (0.148 g/m) [Kevlars]",
    "Kevlar thread 207  (0.5 mm, 1/64 in) (0.236 g/m) [Kevlars]",
    "Kevlar thread 346  (0.7 mm, 1/32 in) (0.472 g/m) [Kevlars]",
    "Kevlar thread 415  (0.8 mm, 1/32 in) (0.551 g/m) [Kevlars]",
    "Kevlar thread 800 (1.1 mm, 3/64 in) (0.992 g/m) [Kevlars]",
    "Nylon flat webbing lg. (14 mm, 9/16 in) (27.2 g/m) [Nylons]",
    "Nylon flat webbing lg. (25 mm, 1 in) (39.7 g/m) [Nylons]",
    "Nylon flat webbing md. (10 mm, 3/8 in) (9.51 g/m) [Nylons]",
    "Nylon flat webbing md. (13 mm, 1/2  in) (13.3 g/m) [Nylons]",
    "Nylon flat webbing md. (16 mm, 5/8 in) (16.2 g/m) [Nylons]",
    "Paraline small IIIA (6.4 mm, 1.4 in) (3.72 g/m) [Other]",
    "Thread (heavy-duty) (0.3 g/m) [Other]",
    "Tubular nylon (11 mm, 7/16 in) (13 g/m) [Nylons]",
    "Tubular nylon (14 mm, 9/16 in) (16 g/m) [Nylons]",
    "Tubular nylon (25 mm, 1 in) (29 g/m) [Nylons]",
    "(ud) Doc string (2 g/m) [Nylons, doc]",
};

// probe: "LINE groups".
constexpr std::array<std::string_view, 4> kLineGroups{
    "Elastics: Elastic braided cord (flat 10 mm, 3/8 in) (3.81 g/m) | Elastic braided cord (flat "
    "13 mm, 1/2 in) (5.51 g/m) | Elastic braided cord (flat 3.2 mm, 1/8 in) (1.06 g/m) | Elastic "
    "braided cord (flat 4 mm, 5/32 in) (2 g/m) | Elastic braided cord (flat 6.4 mm, 1/4 in) (2.54 "
    "g/m) | Elastic braided cord (round 2 mm, 1/16 in) (3.5 g/m) | Elastic braided cord (round 2.5 "
    "mm, 3/32 in) (3.8 g/m) | Elastic cord (flat 12 mm, 1/2 in) (8 g/m) | Elastic cord (flat 19 "
    "mm, 3/4 in) (1.2 g/m) | Elastic cord (flat 25 mm, 1 in) (1.6 g/m) | Elastic cord (flat 6 mm, "
    "1/4 in) (4.3 g/m) | Elastic cord (round 2 mm, 1/16 in) (1.8 g/m) | Elastic rubber band (flat "
    "3.2 mm, 1/8 in) (2.98 g/m) | Elastic rubber band (flat 6.4 mm, 1/4 in) (6.13 g/m)",
    "Kevlars: Kevlar 12-strand (10 mm, 3/8 in) (59.5 g/m) | Kevlar 12-strand (11 mm, 7/16 in) "
    "(74.4 g/m) | Kevlar 12-strand (13 mm, 1/2 in) (116 g/m) | Kevlar 12-strand (14 mm, 9/16 in) "
    "(208 g/m) | Kevlar 12-strand (16 mm, 5/8 in) (287 g/m) | Kevlar 12-strand (19 mm, 3/4 in) "
    "(350 g/m) | Kevlar 12-strand (25 mm, 1 in) (457 g/m) | Kevlar 12-strand (3.2 mm, 1/8 in) "
    "(9.67 g/m) | Kevlar 12-strand (4.8 mm, 3/16 in) (17.9 g/m) | Kevlar 12-strand (6.4 mm, 1/4 "
    "in) (29.8 g/m) | Kevlar 12-strand (7.9 mm, 5/16 in) (44.6 g/m) | Kevlar thread 138  (0.4 mm, "
    "1/64 in) (0.148 g/m) | Kevlar thread 207  (0.5 mm, 1/64 in) (0.236 g/m) | Kevlar thread 346  "
    "(0.7 mm, 1/32 in) (0.472 g/m) | Kevlar thread 415  (0.8 mm, 1/32 in) (0.551 g/m) | Kevlar "
    "thread 800 (1.1 mm, 3/64 in) (0.992 g/m)",
    "Nylons: Braided nylon (2 mm, 1/16 in) (1 g/m) | Braided nylon (3 mm, 1/8 in) (3.5 g/m) | (ud) "
    "Doc string (2 g/m) | Nylon flat webbing lg. (14 mm, 9/16 in) (27.2 g/m) | Nylon flat webbing "
    "lg. (25 mm, 1 in) (39.7 g/m) | Nylon flat webbing md. (10 mm, 3/8 in) (9.51 g/m) | Nylon flat "
    "webbing md. (13 mm, 1/2  in) (13.3 g/m) | Nylon flat webbing md. (16 mm, 5/8 in) (16.2 g/m) | "
    "Tubular nylon (11 mm, 7/16 in) (13 g/m) | Tubular nylon (14 mm, 9/16 in) (16 g/m) | Tubular "
    "nylon (25 mm, 1 in) (29 g/m)",
    "Other: Paraline small IIIA (6.4 mm, 1.4 in) (3.72 g/m) | Thread (heavy-duty) (0.3 g/m)",
};

// probe: "SURFACE", 8 rows.
constexpr std::array<std::string_view, 8> kSurfaceRows{
    "Cellophane (18 g/m\u00b2) [Plastics]",
    "Cr\u00eape paper (25 g/m\u00b2) [PaperProducts]",
    "Mylar (21 g/m\u00b2) [Plastics]",
    "Paper (office) (80 g/m\u00b2) [PaperProducts]",
    "Polyethylene (heavy) (40 g/m\u00b2) [Plastics]",
    "Polyethylene (thin) (15 g/m\u00b2) [Plastics]",
    "Ripstop nylon (67 g/m\u00b2) [Fabrics]",
    "Silk (60 g/m\u00b2) [Fabrics]",
};

// probe: "SURFACE groups".
constexpr std::array<std::string_view, 3> kSurfaceGroups{
    "Plastics: Cellophane (18 g/m\u00b2) | Mylar (21 g/m\u00b2) | Polyethylene (heavy) (40 "
    "g/m\u00b2) | Polyethylene (thin) (15 g/m\u00b2)",
    "Fabrics: Ripstop nylon (67 g/m\u00b2) | Silk (60 g/m\u00b2)",
    "PaperProducts: Cr\u00eape paper (25 g/m\u00b2) | Paper (office) (80 g/m\u00b2)",
};

/// The source of an external component's material (Java: "Material").
[[nodiscard]] MaterialSource externalMaterial()
{
    return MaterialSource::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getMaterial(); },
        .set = [](ExternalComponent& component,
                  const Material&    material) { component.setMaterial(material); },
    });
}

/// The source of an external component's material without a setter.
[[nodiscard]] MaterialSource externalMaterialReadOnly()
{
    return MaterialSource::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getMaterial(); },
    });
}

/// The source of a parachute's line material (Java: "LineMaterial").
[[nodiscard]] MaterialSource lineMaterial()
{
    return MaterialSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getLineMaterial(); },
        .set = [](Parachute&      parachute,
                  const Material& material) { parachute.setLineMaterial(material); },
    });
}

/// The source of a recovery device's surface material (Java: "Material").
[[nodiscard]] MaterialSource surfaceMaterial()
{
    return MaterialSource::of<RecoveryDevice>({
        .get = [](const RecoveryDevice& device) { return device.getMaterial(); },
        .set = [](RecoveryDevice& device,
                  const Material& material) { device.setMaterial(material); },
    });
}

/// The probe's design: the application's materials (the built-in ones) and a new rocket's
/// document with a body tube in its stage and a parachute in the tube.
struct MaterialDesign
{
    MaterialStorage                     application;
    std::unique_ptr<OpenRocketDocument> document;
    BodyTube*                           tube{nullptr};
    Parachute*                          chute{nullptr};

    MaterialDesign();

    /// The probe's additions: a user material in the application's bulk materials and the three
    /// document materials.
    void addProbeMaterials();

    /// A binding of @p source of @p type on @p component.
    [[nodiscard]] std::shared_ptr<MaterialBinding> bind(RocketComponent& component,
                                                        MaterialSource source, Material::Type type);
};

MaterialDesign::MaterialDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    addBuiltinMaterials(application);
    RocketComponent& stage = document->getRocket().getChild(0);
    tube                   = &stage.addChild(std::make_unique<BodyTube>());
    chute                  = &tube->addChild(std::make_unique<Parachute>());
}

void MaterialDesign::addProbeMaterials()
{
    application.addMaterial(Material::newMaterial(Material::Type::BULK, "Aardvark resin", 1500,
                                                  MaterialGroup::PLASTICS, true));
    tube->setMaterial(Material::newMaterial(Material::Type::BULK, "Doc foam", 30,
                                            MaterialGroup::FOAMS, true, true));
    document->getDocumentMaterials().addMaterial(Material::newMaterial(
        Material::Type::BULK, "Zz custom", 900, MaterialGroup::CUSTOM, true, true));
    chute->setLineMaterial(Material::newMaterial(Material::Type::LINE, "Doc string", 0.002,
                                                 MaterialGroup::NYLONS, true, true));
}

std::shared_ptr<MaterialBinding> MaterialDesign::bind(RocketComponent& component,
                                                      MaterialSource source, Material::Type type)
{
    return std::make_shared<MaterialBinding>(std::make_shared<ComponentSelection>(component),
                                             std::move(source), type, application, *document);
}

/// The material named @p name of @p database (a failure and the first material when there is none).
[[nodiscard]] Material materialNamed(const MaterialDatabase& database, std::string_view name)
{
    for (const Material& material : database)
    {
        if (material.getName() == name)
        {
            return material;
        }
    }
    ADD_FAILURE() << "no material " << name;
    return database.get(0);
}

/// The probe's row of @p material: its combo box text, group and whether it is a document
/// material.
[[nodiscard]] std::string row(const Material& material)
{
    std::string text = MaterialBinding::displayString(material) + " [" +
                       std::string(databaseString(material.getGroup()));
    if (material.isDocumentMaterial())
    {
        text += ", doc";
    }
    return text + "]";
}

/// The rows of the materials @p binding offers.
[[nodiscard]] std::vector<std::string> rows(const MaterialBinding& binding)
{
    std::vector<std::string> rows;
    for (const Material& material : binding.materials())
    {
        rows.push_back(row(material));
    }
    return rows;
}

/// The probe's group lines of @p binding: "<group>: <text> | <text> ...".
[[nodiscard]] std::vector<std::string> groupLines(const MaterialBinding& binding)
{
    std::vector<std::string> lines;
    for (const MaterialBinding::GroupItems& group : binding.groups())
    {
        std::string      line      = std::string(databaseString(group.group)) + ":";
        std::string_view separator = " ";
        for (const int index : group.indices)
        {
            line += separator;
            line += MaterialBinding::displayString(*binding.materialAt(index));
            separator = " | ";
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

/// @p texts as strings.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> strings(const std::array<std::string_view, N>& texts)
{
    return {texts.begin(), texts.end()};
}

/// @p texts without the rows of user-defined materials: the probe's lists before its additions.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> builtinOnly(const std::array<std::string_view, N>& texts)
{
    std::vector<std::string> builtin;
    for (const std::string_view text : texts)
    {
        if (!text.starts_with("(ud) "))
        {
            builtin.emplace_back(text);
        }
    }
    return builtin;
}

// ------------------------------------------------------------------ MaterialModelTest's test

TEST_F(MaterialBindingTest, NegativeIndexReturnsNoMaterial)
{
    // Port of MaterialModelTest.negativeIndexReturnsNoMaterial.
    MaterialDesign design;
    const auto     model = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);

    EXPECT_EQ(model->materialAt(-1), nullptr);
}

// ------------------------------------------------------------------------------------- the rest

TEST_F(MaterialBindingTest, TheBulkMaterialsAsOpenRocket)
{
    // probe: "BULK (built-in only): size=32 selected=Cardboard (0.68 g/cm3) [PaperProducts]",
    // the 35 rows of kBulkRows without the three user-defined ones; then kBulkRows, selected
    // "(ud) Doc foam", nothing at -1 and at the size, and the groups kBulkGroups.
    MaterialDesign      design;
    const auto          bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    const ChangeCounter changes(*bulk);
    EXPECT_EQ(bulk->size(), 32);
    EXPECT_EQ(rows(*bulk), builtinOnly(kBulkRows));
    EXPECT_EQ(row(bulk->value()), "Cardboard (0.68 g/cm\u00b3) [PaperProducts]");
    EXPECT_EQ(bulk->indexOf(bulk->value()), 10);

    design.addProbeMaterials();
    EXPECT_EQ(bulk->size(), 35);
    EXPECT_EQ(rows(*bulk), strings(kBulkRows));
    EXPECT_EQ(row(bulk->value()), kBulkRows[33]);
    EXPECT_EQ(bulk->indexOf(bulk->value()), 33);
    EXPECT_EQ(bulk->materialAt(-1), nullptr);
    EXPECT_EQ(bulk->materialAt(35), nullptr);
    EXPECT_EQ(groupLines(*bulk), strings(kBulkGroups));
    // The application's database, the document's twice (the tube's material, then Zz custom),
    // and the tube's change.
    EXPECT_EQ(changes.count(), 4);
}

TEST_F(MaterialBindingTest, TheLineMaterialsAsOpenRocket)
{
    // probe: "LINE: size=43 selected=(ud) Doc string (2 g/m) [Nylons, doc]".
    MaterialDesign design;
    design.addProbeMaterials();
    const auto line = design.bind(*design.chute, lineMaterial(), Material::Type::LINE);
    EXPECT_EQ(line->getType(), Material::Type::LINE);
    EXPECT_EQ(rows(*line), strings(kLineRows));
    EXPECT_EQ(row(line->value()), kLineRows[42]);
    EXPECT_EQ(line->materialAt(43), nullptr);
    EXPECT_EQ(groupLines(*line), strings(kLineGroups));
}

TEST_F(MaterialBindingTest, TheSurfaceMaterialsAsOpenRocket)
{
    // probe: "SURFACE: size=8 selected=Ripstop nylon (67 g/m2) [Fabrics]".
    MaterialDesign design;
    design.addProbeMaterials();
    const auto surface = design.bind(*design.chute, surfaceMaterial(), Material::Type::SURFACE);
    EXPECT_EQ(rows(*surface), strings(kSurfaceRows));
    EXPECT_EQ(row(surface->value()), kSurfaceRows[6]);
    EXPECT_EQ(surface->materialAt(8), nullptr);
    EXPECT_EQ(groupLines(*surface), strings(kSurfaceGroups));
}

TEST_F(MaterialBindingTest, TheListFollowsTheDatabases)
{
    MaterialDesign design;
    design.addProbeMaterials();
    const auto          bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    const ChangeCounter changes(*bulk);
    const Material      user = *bulk->materialAt(2);
    EXPECT_EQ(row(user), kBulkRows[2]);
    EXPECT_TRUE(design.application.removeMaterial(user));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(bulk->size(), 34);
    EXPECT_EQ(row(*bulk->materialAt(2)), kBulkRows[3]);
    // Another type's database is none of the binding's business.
    EXPECT_TRUE(design.application.addMaterial(
        Material::newMaterial(Material::Type::LINE, "Kite string", 0.001, true)));
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(MaterialBindingTest, SelectSetsTheMaterialOnEveryComponent)
{
    // Java's setSelectedItem() calls the setter every time, and the main's setter forwards the
    // material to the others, also the main's own.
    MaterialDesign design;
    BodyTube&      other         = design.tube->getParent()->addChild(std::make_unique<BodyTube>());
    const Material balsaMaterial = materialNamed(design.application.bulkMaterials(), "Balsa");
    other.setMaterial(balsaMaterial);
    const std::array<RocketComponent*, 1> others{&other};
    const auto                            bulk = std::make_shared<MaterialBinding>(
        std::make_shared<ComponentSelection>(*design.tube, others), externalMaterial(),
        Material::Type::BULK, design.application, *design.document);
    const RocketEventRecorder events(design.document->getRocket());
    const ChangeCounter       changes(*bulk);
    EXPECT_TRUE(bulk->select(bulk->value()));  // Cardboard, the main's own
    EXPECT_EQ(other.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(changes.count(), 0);
    EXPECT_TRUE(bulk->select(balsaMaterial));
    EXPECT_EQ(design.tube->getMaterial().getName(), "Balsa");
    EXPECT_EQ(other.getMaterial().getName(), "Balsa");
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(bulk->indexOf(bulk->value()), 4);
}

TEST_F(MaterialBindingTest, TheTextOfAMaterial)
{
    const Material builtin =
        Material::newMaterial(Material::Type::BULK, "Oak", 750, MaterialGroup::WOODS, false);
    const Material user = Material::newMaterial(Material::Type::SURFACE, "Tyvek", 0.041,
                                                MaterialGroup::PLASTICS, true);
    EXPECT_EQ(MaterialBinding::displayString(builtin), "Oak (0.75 g/cm\u00b3)");
    EXPECT_EQ(MaterialBinding::displayString(user), "(ud) Tyvek (41 g/m\u00b2)");
}

/// What the combo box's write gave while the binding told its listeners.
struct FiringWrite
{
    std::optional<bool> selected;
};

/// Tries the combo box's write of @p binding from its change listener, into @p write.
[[nodiscard]] Signal<>::ScopedConnection tryWriteWhileFiring(MaterialBinding& binding,
                                                             FiringWrite&     write)
{
    return Signal<>::ScopedConnection(binding.addChangeListener(
        [&binding, &write] { write.selected = binding.select(*binding.materialAt(0)); }));
}

TEST_F(MaterialBindingTest, TheComboBoxWriteIsDroppedWhileFiring)
{
    MaterialDesign design;
    const auto     bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    FiringWrite    write;
    const Signal<>::ScopedConnection connection = tryWriteWhileFiring(*bulk, write);
    EXPECT_TRUE(bulk->select(*bulk->materialAt(3)));  // Aluminum
    EXPECT_EQ(write.selected, false);
    EXPECT_EQ(design.tube->getMaterial().getName(), "Aluminum");
}

TEST_F(MaterialBindingTest, AnUndoEndsTheBinding)
{
    // The undo destroys the components (Rocket::loadFrom()): the binding never touches them, nor
    // reads the databases, again (proved under ASan).
    MaterialDesign      design;
    const auto          bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    const ChangeCounter changes(*bulk);
    design.document->addUndoPosition("Modify body tube");
    EXPECT_TRUE(bulk->select(*bulk->materialAt(3)));  // Aluminum
    design.document->undo();
    EXPECT_FALSE(bulk->isValid());
    EXPECT_EQ(changes.count(), 2);
    EXPECT_FALSE(bulk->isEnabled());
    EXPECT_EQ(bulk->value().getName(), "Aluminum");  // the last material read
    EXPECT_FALSE(bulk->select(*bulk->materialAt(5)));
    design.application.addMaterial(
        Material::newMaterial(Material::Type::BULK, "Late resin", 1200, true));
    EXPECT_EQ(changes.count(), 2);
    EXPECT_EQ(bulk->size(), 32);
}

TEST_F(MaterialBindingTest, ADocumentClosedFirstIsReadNoMore)
{
    // The document, its rocket and its materials may go before the binding (a window closing):
    // the selection is then invalid, and a change of the application's materials reads neither
    // database (proved under ASan).
    MaterialDesign      design;
    const auto          bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    const ChangeCounter changes(*bulk);
    design.document.reset();
    EXPECT_FALSE(bulk->isValid());
    EXPECT_TRUE(design.application.addMaterial(
        Material::newMaterial(Material::Type::BULK, "Late resin", 1200, true)));
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(bulk->size(), 32);
    EXPECT_FALSE(bulk->select(*bulk->materialAt(0)));
}

TEST_F(MaterialBindingTest, RefusesMaterialsOfAnotherType)
{
    MaterialDesign design;
    const auto     bulk = design.bind(*design.tube, externalMaterial(), Material::Type::BULK);
    EXPECT_THROW(static_cast<void>(bulk->select(design.chute->getLineMaterial())), BugError);
    // The main component's material must be of the binding's type.
    EXPECT_THROW(
        static_cast<void>(design.bind(*design.chute, surfaceMaterial(), Material::Type::LINE)),
        BugError);
    EXPECT_THROW(
        static_cast<void>(design.bind(*design.tube, externalMaterial(), Material::Type::CUSTOM)),
        BugError);
}

TEST_F(MaterialBindingTest, RefusesAMissingSelectionSetterOrDocument)
{
    MaterialDesign design;
    EXPECT_THROW(
        static_cast<void>(MaterialBinding(nullptr, externalMaterial(), Material::Type::BULK,
                                          design.application, *design.document)),
        BugError);
    EXPECT_THROW(static_cast<void>(
                     design.bind(*design.tube, externalMaterialReadOnly(), Material::Type::BULK)),
                 BugError);
    MaterialDesign other;
    EXPECT_THROW(static_cast<void>(MaterialBinding(
                     std::make_shared<ComponentSelection>(*design.tube), externalMaterial(),
                     Material::Type::BULK, design.application, *other.document)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
