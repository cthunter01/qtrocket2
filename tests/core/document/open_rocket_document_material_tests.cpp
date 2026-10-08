#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "document/DocumentTestSupport.h"
#include "rocket/TestRockets.h"

// The document materials of an OpenRocketDocument: the materials its rocket's components were
// given that belong to the document (Java: the material databases of
// document.getDocumentPreferences(); here OpenRocketDocument::getDocumentMaterials()).
//
// First the two cases of MaterialTest.java that need a document, in full
// (tests/core/material/MaterialTests.cpp has their material side, and
// tests/core/rocket/document_material_tests.cpp the rocket's signal the document listens to).
// Then tests of this port's own, whose expectations are what OpenRocket printed for the same
// steps: DocumentProbe.out section 6 of the tier 9 scout, and DocumentProbe3.txt section I and
// DocumentProbe5.txt of this part (probes/tier9a-document-d3/logs).

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Rocket;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::TestEstesAlphaIII;

/// MaterialTest.EPSILON
constexpr double kEpsilon = 1e-6;

/// A user-defined document material in the group Custom (Java: Material.newMaterial(type, name,
/// density, true, true)).
[[nodiscard]] Material doc(Material::Type type, std::string name, double density)
{
    return Material::newMaterial(type, std::move(name), density, true, true);
}

/// A user-defined document material in @p group.
[[nodiscard]] Material doc(Material::Type type, std::string name, double density,
                           MaterialGroup group)
{
    return Material::newMaterial(type, std::move(name), density, group, true, true);
}

/// document.getDocumentPreferences().getAllMaterials().size()
[[nodiscard]] std::size_t allMaterialsSize(const OpenRocketDocument& document)
{
    return document.getDocumentMaterials().allMaterials().size();
}

/// The document materials as the probes print them: getAllMaterials() as storable strings, each
/// followed by ", ", in brackets, and the sizes of the three databases.
[[nodiscard]] std::string all(const OpenRocketDocument& document)
{
    const MaterialStorage& materials = document.getDocumentMaterials();
    std::string            text      = "[";
    for (const Material& material : materials.allMaterials())
    {
        text += material.toStorableString();
        text += ", ";
    }
    text += "] bulk=" + std::to_string(materials.bulkMaterials().size());
    text += " surface=" + std::to_string(materials.surfaceMaterials().size());
    text += " line=" + std::to_string(materials.lineMaterials().size());
    return text;
}

// ---------------------------------------------------------------------- MaterialTest.java

// MaterialTest.testDocumentDatabase
TEST(OpenRocketDocumentMaterials, DocumentDatabase)
{
    // Create a document and rocket
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    Rocket&     rocket   = document->getRocket();
    AxialStage* stage    = rocket.getStage(0);
    BodyTube&   bodyTube = stage->addChild(std::make_unique<BodyTube>());

    // Check current document material database
    EXPECT_EQ(0U, allMaterialsSize(*document));

    // Create a material
    std::string          name    = "Tube Material";
    const Material::Type type    = Material::Type::BULK;
    double               density = 314;
    const Material       m       = Material::newMaterial(type, name, density, true, true);

    // Check document material database
    EXPECT_EQ(0U, allMaterialsSize(*document));

    // Assign material to body tube
    bodyTube.setMaterial(m);
    EXPECT_EQ(1U, allMaterialsSize(*document));
    EXPECT_EQ(name, bodyTube.getMaterial().getName());
    EXPECT_EQ(type, bodyTube.getMaterial().getType());
    EXPECT_NEAR(density, bodyTube.getMaterial().getDensity(), kEpsilon);
    EXPECT_TRUE(bodyTube.getMaterial().isUserDefined());
    EXPECT_TRUE(bodyTube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, bodyTube.getMaterial().getGroup());

    // Assign a new material
    name              = "New Material";
    density           = 271;
    const Material m2 = Material::newMaterial(type, name, density, true, true);
    bodyTube.setMaterial(m2);
    EXPECT_EQ(2U, allMaterialsSize(*document));
    EXPECT_EQ(name, bodyTube.getMaterial().getName());
    EXPECT_EQ(type, bodyTube.getMaterial().getType());
    EXPECT_NEAR(density, bodyTube.getMaterial().getDensity(), kEpsilon);
    EXPECT_TRUE(bodyTube.getMaterial().isUserDefined());
    EXPECT_TRUE(bodyTube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, bodyTube.getMaterial().getGroup());

    // Remove a material
    document->getDocumentMaterials().removeMaterial(m);
    EXPECT_EQ(1U, allMaterialsSize(*document));
}

// MaterialTest.testLoadFromPreset (with createPreset())
TEST(OpenRocketDocumentMaterials, LoadFromPreset)
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMaterial,
                   Material::newMaterial(Material::Type::BULK, "Preset Material", 980, true, true));
    const MaterialStorage                        applicationMaterials;
    const std::shared_ptr<const ComponentPreset> preset = std::make_shared<const ComponentPreset>(
        ComponentPresetFactory::create(presetspec, applicationMaterials).value());

    // Create a document and rocket
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    Rocket&     rocket   = document->getRocket();
    AxialStage* stage    = rocket.getStage(0);
    BodyTube&   bodyTube = stage->addChild(std::make_unique<BodyTube>());

    // Check document material database
    EXPECT_EQ(0U, allMaterialsSize(*document));

    // Load from preset (Java's assertSame on the material: materials are values here)
    bodyTube.loadPreset(preset);
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial), bodyTube.getMaterial());
    EXPECT_EQ("Preset Material", bodyTube.getMaterial().getName());
    EXPECT_EQ(Material::Type::BULK, bodyTube.getMaterial().getType());
    EXPECT_NEAR(980, bodyTube.getMaterial().getDensity(), kEpsilon);
    EXPECT_TRUE(bodyTube.getMaterial().isUserDefined());
    EXPECT_TRUE(bodyTube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, bodyTube.getMaterial().getGroup());

    // Check document material database
    EXPECT_EQ(1U, allMaterialsSize(*document));
}

// ------------------------------------------------------------------ QtRocket's own tests

// DocumentProbe.out, section 6 ("document materials through setMaterial").
TEST(OpenRocketDocumentMaterials, AreRegisteredWhenAComponentIsGivenOne)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d         = alpha.document();
    MaterialStorage&    materials = d.getDocumentMaterials();

    // "count after create=0", "count after reload=0": the Alpha III has built-in materials only.
    EXPECT_EQ(materials.totalMaterialCount(), 0U);
    d.reloadDocumentMaterials();
    EXPECT_EQ(materials.totalMaterialCount(), 0U);

    const Material docStuff = doc(Material::Type::BULK, "DocStuff", 123.0);
    const Material userStuff =
        Material::newMaterial(Material::Type::BULK, "UserStuff", 124.0, true);
    d.setSaved(true);

    // "after setMaterial(doc material): count=1 saved=false"
    alpha.body().setMaterial(docStuff);
    EXPECT_EQ(materials.totalMaterialCount(), 1U);
    EXPECT_FALSE(d.isSaved());

    // "after setMaterial(user material): count=1": the tube no longer has the document
    // material, which stays registered.
    alpha.body().setMaterial(userStuff);
    EXPECT_EQ(materials.totalMaterialCount(), 1U);

    // "after removeMaterial: count=0"
    EXPECT_TRUE(materials.removeMaterial(docStuff));
    EXPECT_EQ(materials.totalMaterialCount(), 0U);

    // "set doc again: count=1"
    d.addUndoPosition("mat");
    alpha.body().setMaterial(docStuff);
    EXPECT_EQ(materials.totalMaterialCount(), 1U);

    // "after remove, undo, redo: count=0 tube material=BULK|DocStuff|123.0|0.0|Custom": undo
    // and redo load the tube with its material and register nothing.
    EXPECT_TRUE(materials.removeMaterial(docStuff));
    d.undo();
    EXPECT_EQ(alpha.body().getMaterial().getName(), "UserStuff");
    d.redo();
    EXPECT_EQ(materials.totalMaterialCount(), 0U);
    EXPECT_EQ(alpha.body().getMaterial().toStorableString(), "BULK|DocStuff|123.0|0.0|Custom");

    // "detached component: ok, count=0"
    BodyTube loose;
    loose.setMaterial(doc(Material::Type::BULK, "Loose", 5.0));
    EXPECT_EQ(materials.totalMaterialCount(), 0U);

    // "added InnerTube: count=0": a new component comes with a material that is not the
    // document's.
    alpha.body().addChild(std::make_unique<InnerTube>());
    EXPECT_EQ(materials.totalMaterialCount(), 0U);
}

// DocumentProbe5.txt: what getAllMaterials() lists after each step. Java keeps a database
// sorted by name, then density; materials that compare equal (the same name and density in
// different groups) stand in the order its binary search gives them.
TEST(OpenRocketDocumentMaterials, AreListedInTheOrderOfOpenRocketsDatabases)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    EXPECT_EQ(all(d), "[] bulk=0 surface=0 line=0");

    alpha.body().setMaterial(doc(Material::Type::BULK, "Zeta", 100.0));
    EXPECT_EQ(all(d), "[BULK|Zeta|100.0|0.0|Custom, ] bulk=1 surface=0 line=0");

    alpha.nose().setMaterial(doc(Material::Type::BULK, "Alpha", 200.0));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|200.0|0.0|Custom, BULK|Zeta|100.0|0.0|Custom, ] "
              "bulk=2 surface=0 line=0");

    // The same material on a second component is not registered twice.
    alpha.body().setMaterial(doc(Material::Type::BULK, "Alpha", 200.0));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|200.0|0.0|Custom, BULK|Zeta|100.0|0.0|Custom, ] "
              "bulk=2 surface=0 line=0");

    alpha.body().setMaterial(doc(Material::Type::BULK, "Alpha", 150.0));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Zeta|100.0|0.0|Custom, ] bulk=3 surface=0 line=0");

    alpha.body().setMaterial(doc(Material::Type::BULK, "Alpha", 200.0, MaterialGroup::OTHER));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Other, BULK|Zeta|100.0|0.0|Custom, ] "
              "bulk=4 surface=0 line=0");

    alpha.body().setMaterial(doc(Material::Type::BULK, "Alpha", 200.0, MaterialGroup::WOODS));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Other, BULK|Alpha|200.0|0.0|Woods, "
              "BULK|Zeta|100.0|0.0|Custom, ] bulk=5 surface=0 line=0");

    alpha.chute().setMaterial(doc(Material::Type::SURFACE, "Mylar probe", 0.021));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Other, BULK|Alpha|200.0|0.0|Woods, "
              "SURFACE|Mylar probe|0.021|0.0|Custom, BULK|Zeta|100.0|0.0|Custom, ] "
              "bulk=5 surface=1 line=0");

    alpha.chute().setLineMaterial(doc(Material::Type::LINE, "Braid probe", 0.002));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Other, BULK|Alpha|200.0|0.0|Woods, "
              "LINE|Braid probe|0.002|0.0|Custom, SURFACE|Mylar probe|0.021|0.0|Custom, "
              "BULK|Zeta|100.0|0.0|Custom, ] bulk=5 surface=1 line=1");
    EXPECT_FALSE(d.isSaved());

    // Adding and removing by hand, as a loader and a material dialog do, changes neither the
    // rocket nor the saved state: "saved=true" twice.
    d.setSaved(true);
    EXPECT_TRUE(d.getDocumentMaterials().removeMaterial(doc(Material::Type::BULK, "Zeta", 100.0)));
    EXPECT_EQ(all(d),
              "[BULK|Alpha|150.0|0.0|Custom, BULK|Alpha|200.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Other, BULK|Alpha|200.0|0.0|Woods, "
              "LINE|Braid probe|0.002|0.0|Custom, SURFACE|Mylar probe|0.021|0.0|Custom, ] "
              "bulk=4 surface=1 line=1");
    EXPECT_TRUE(d.isSaved());
    EXPECT_TRUE(
        d.getDocumentMaterials().addMaterial(doc(Material::Type::BULK, "Added by hand", 1)));
    EXPECT_EQ(all(d),
              "[BULK|Added by hand|1.0|0.0|Custom, BULK|Alpha|150.0|0.0|Custom, "
              "BULK|Alpha|200.0|0.0|Custom, BULK|Alpha|200.0|0.0|Other, "
              "BULK|Alpha|200.0|0.0|Woods, LINE|Braid probe|0.002|0.0|Custom, "
              "SURFACE|Mylar probe|0.021|0.0|Custom, ] bulk=5 surface=1 line=1");
    EXPECT_TRUE(d.isSaved());
}

/// The rocket of an Estes Alpha III whose tube, nose cone and parachute got document materials
/// before any document existed (DocumentProbe3.java, section I).
[[nodiscard]] std::unique_ptr<Rocket> rocketWithDocumentMaterials()
{
    TestEstesAlphaIII alpha;
    alpha.body->setMaterial(doc(Material::Type::BULK, "Zeta", 100.0));
    alpha.nose->setMaterial(doc(Material::Type::BULK, "Alpha", 200.0));
    alpha.chute->setMaterial(doc(Material::Type::SURFACE, "Canopy", 0.05));
    alpha.chute->setLineMaterial(doc(Material::Type::LINE, "Line", 0.002));
    return std::move(alpha.rocket);
}

// DocumentProbe3.txt, section I.
TEST(OpenRocketDocumentMaterials, ReloadCollectsTheMaterialsOfARocketMadeBeforeTheDocument)
{
    // "made before the document: count=0"
    const std::unique_ptr<OpenRocketDocument> d =
        OpenRocketDocumentFactory::createDocumentFromRocket(rocketWithDocumentMaterials());
    EXPECT_EQ(d->getDocumentMaterials().totalMaterialCount(), 0U);

    // "after reload: count=4 [Bulk Alpha, Surface Canopy, Line Line, Bulk Zeta, ] saved=true"
    d->setSaved(true);
    d->reloadDocumentMaterials();
    EXPECT_EQ(all(*d),
              "[BULK|Alpha|200.0|0.0|Custom, SURFACE|Canopy|0.05|0.0|Custom, "
              "LINE|Line|0.002|0.0|Custom, BULK|Zeta|100.0|0.0|Custom, ] "
              "bulk=2 surface=1 line=1");
    EXPECT_TRUE(d->isSaved());

    // A second reload adds nothing.
    d->reloadDocumentMaterials();
    EXPECT_EQ(d->getDocumentMaterials().totalMaterialCount(), 4U);
}

// DocumentProbe3.txt, section I: a document material that is not user-defined ("odd: user=false
// doc=true") is registered when a component is given it ("count=1"), and not by a reload
// ("removed, then reload: count=0"), which asks for both flags.
TEST(OpenRocketDocumentMaterials, ReloadSkipsADocumentMaterialThatIsNotUserDefined)
{
    const AlphaDocument alpha;
    OpenRocketDocument& d = alpha.document();
    const Material odd    = Material::newMaterial(Material::Type::BULK, "Odd", 300.0, false, true);
    ASSERT_FALSE(odd.isUserDefined());
    ASSERT_TRUE(odd.isDocumentMaterial());

    alpha.body().setMaterial(odd);
    EXPECT_EQ(d.getDocumentMaterials().totalMaterialCount(), 1U);

    EXPECT_TRUE(d.getDocumentMaterials().removeMaterial(odd));
    d.reloadDocumentMaterials();
    EXPECT_EQ(d.getDocumentMaterials().totalMaterialCount(), 0U);
}

// The document hears its rocket for as long as it lives, whatever happens to the rocket's other
// listeners: Java's rocket keeps its document pointer through resetListeners().
TEST(OpenRocketDocumentMaterials, AreStillRegisteredAfterTheRocketsListenersWereReset)
{
    const AlphaDocument alpha;
    alpha.rocket().resetListeners();

    alpha.body().setMaterial(doc(Material::Type::BULK, "After the reset", 77.0));

    EXPECT_EQ(all(alpha.document()),
              "[BULK|After the reset|77.0|0.0|Custom, ] bulk=1 surface=0 line=0");
}

}  // namespace
