#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "rocket/preset/ExamplePresets.h"
#include "unit/DefaultUnitsGuard.h"

// The six presets of the example designs (ExamplePresets.h). The digests are the ones the example
// files store in their <preset> elements (data/examples, and the re-saves under
// tests/data/goldens/example-*/resave/rocket.ork). Every other value is what OpenRocket holds for
// the same presets: PresetDatabaseProbe.java (part F) and ExamplePresetLoadProbe.java of the tier
// 9a probes build them with OpenRocket's ComponentPresetFactory (commit 5f164fd0e, JDK 17) and
// print them with Double.toString.
//
// All doubles here are compared exactly. They are results of +, -, *, / and sqrt alone, which
// IEEE 754 fixes to the last bit on every platform (the build forbids contraction): a tube's
// volume is pi (ro^2 - ri^2) l, a canopy's area pi (d / 2)^2, and the volume of an ogive nose
// cone is SymmetricComponent's sum over its frusta, whose radii are square roots
// (TransitionShape's ogive) and whose wall height goes through MathUtil::hypot(), a plain
// sqrt(x^2 + y^2); no transcendental function is on that path. The two nose-cone densities are
// mass / volume, and their digests cover the density's bits, so a platform that computed
// another volume would fail the digest pins as well.

namespace
{

using QtRocket::BodyTube;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetDatabase;
using QtRocket::ComponentPresetType;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::TransitionShape;
using QtRocket::Test::ExamplePresetReference;
using QtRocket::Test::kExamplePresetReferences;
using QtRocket::Test::makeExamplePresetDatabase;

using PresetPtr = ComponentPresetDatabase::PresetPtr;

/// "TYPE | part number | digest".
[[nodiscard]] std::string line(const ComponentPreset& preset)
{
    return std::format("{} | {} | {}", componentPresetTypeName(preset.getType()),
                       preset.getPartNo(), preset.getDigest());
}

/// What the .ork loader does with a <preset> element (ComponentPresetSetter.java): of the
/// presets find() gives for the manufacturer and the part number, the first whose digest is the
/// file's; nullptr when there is none.
[[nodiscard]] PresetPtr resolve(const ComponentPresetDatabase& database,
                                const ExamplePresetReference&  reference)
{
    for (const PresetPtr& candidate : database.find(reference.manufacturer, reference.partNo))
    {
        if (candidate->getDigest() == reference.digest)
        {
            return candidate;
        }
    }
    return nullptr;
}

/// The reference @p index resolved in a new example database: "<candidates> | <line of the
/// preset>", or "<candidates> | unresolved".
[[nodiscard]] std::string resolved(std::size_t index)
{
    const ComponentPresetDatabase database  = makeExamplePresetDatabase();
    const ExamplePresetReference& reference = kExamplePresetReferences.at(index);
    const std::size_t candidates = database.find(reference.manufacturer, reference.partNo).size();
    const PresetPtr   preset     = resolve(database, reference);
    return std::format("{} | {}", candidates, preset != nullptr ? line(*preset) : "unresolved");
}

/// The preset with the part number @p partNo of a new example database (which the preset
/// outlives: it is co-owned).
[[nodiscard]] PresetPtr examplePreset(const std::string& partNo)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    const std::vector<PresetPtr>  found    = database.find("Estes", partNo);
    return found.size() == 1 ? found.front() : nullptr;
}

/// What the probe prints of a material, but for the density:
/// `BULK "name" group <group> userDefined <bool> documentMaterial <bool>`.
[[nodiscard]] std::string materialLine(const Material& material)
{
    return std::format(R"({} "{}" group {} userDefined {} documentMaterial {})",
                       toString(material.getType()), material.getName(),
                       databaseString(material.getGroup()), material.isUserDefined(),
                       material.isDocumentMaterial());
}

// ================================================================================ the database

TEST(ExamplePresets, TheDatabaseHoldsTheSixPresetsByPartNumber)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    const std::vector<PresetPtr>& all      = database.listAll();
    ASSERT_EQ(all.size(), 6U);
    EXPECT_EQ(line(*all[0]), "BODY_TUBE | BT-50, 30352 | a59dec8e4034a2fee5955dbf4ff07f1c");
    EXPECT_EQ(line(*all[1]), "BODY_TUBE | BT-50FE, 30359 | e9c3e97d32e96762f65e051d74cb9fd7");
    EXPECT_EQ(line(*all[2]), "PARACHUTE | PK-10, 2262 | b595c8a31baaf325291d5c957f3940f8");
    EXPECT_EQ(line(*all[3]), "PARACHUTE | PK-8, 2260 | 9a96b8806749e790db29bacfbc70eb1e");
    EXPECT_EQ(line(*all[4]), "NOSE_CONE | PNC-20, 072606 | 6f564a198c80de3af437608827fa1bc1");
    EXPECT_EQ(line(*all[5]), "NOSE_CONE | PNC-50YR, 72604 | 18363dec5642206a109389ef48efcb32");
}

TEST(ExamplePresets, EveryPresetIsEstesAndNotLegacy)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    for (const PresetPtr& preset : database.listAll())
    {
        EXPECT_EQ(preset->getManufacturer().getSimpleName(), "Estes") << preset->getPartNo();
        EXPECT_EQ(preset->getLegacy(), false) << preset->getPartNo();
    }
}

TEST(ExamplePresets, EveryReferenceOfTheExamplesResolvesToOnePreset)
{
    // The six <preset> elements, in the order of kExamplePresetReferences: one candidate each,
    // of the file's type, with the file's digest.
    EXPECT_EQ(resolved(0), "1 | BODY_TUBE | BT-50FE, 30359 | e9c3e97d32e96762f65e051d74cb9fd7");
    EXPECT_EQ(resolved(1), "1 | PARACHUTE | PK-10, 2262 | b595c8a31baaf325291d5c957f3940f8");
    EXPECT_EQ(resolved(2), "1 | PARACHUTE | PK-8, 2260 | 9a96b8806749e790db29bacfbc70eb1e");
    EXPECT_EQ(resolved(3), "1 | NOSE_CONE | PNC-50YR, 72604 | 18363dec5642206a109389ef48efcb32");
    EXPECT_EQ(resolved(4), "1 | BODY_TUBE | BT-50, 30352 | a59dec8e4034a2fee5955dbf4ff07f1c");
    EXPECT_EQ(resolved(5), "1 | NOSE_CONE | PNC-20, 072606 | 6f564a198c80de3af437608827fa1bc1");
}

TEST(ExamplePresets, TheReferencesNameTheirTypes)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    for (const ExamplePresetReference& reference : kExamplePresetReferences)
    {
        const PresetPtr preset = resolve(database, reference);
        ASSERT_NE(preset, nullptr) << reference.partNo;
        EXPECT_EQ(preset->getType(), reference.type) << reference.partNo;
        EXPECT_EQ(reference.manufacturer, "Estes") << reference.partNo;
    }
}

TEST(ExamplePresets, AFileThatSpellsTheManufacturerDifferentlyFindsThePresets)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    const std::vector<PresetPtr>  found    = database.find("ESTES", "PK-8, 2260");
    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found[0]->getDigest(), "9a96b8806749e790db29bacfbc70eb1e");
    // The part number is the whole attribute, the comma and the number included.
    EXPECT_TRUE(database.find("Estes", "PK-8").empty());
    EXPECT_TRUE(database.find("Quest", "PK-8, 2260").empty());
}

TEST(ExamplePresets, TheTypeListsAreInPartNumberOrder)
{
    const ComponentPresetDatabase database = makeExamplePresetDatabase();
    const std::vector<PresetPtr>  listed =
        database.listForTypes({ComponentPresetType::NOSE_CONE, ComponentPresetType::PARACHUTE});
    ASSERT_EQ(listed.size(), 4U);
    EXPECT_EQ(listed[0]->getPartNo(), "PK-10, 2262");
    EXPECT_EQ(listed[1]->getPartNo(), "PK-8, 2260");
    EXPECT_EQ(listed[2]->getPartNo(), "PNC-20, 072606");
    EXPECT_EQ(listed[3]->getPartNo(), "PNC-50YR, 72604");
    EXPECT_EQ(database.listForType(ComponentPresetType::BODY_TUBE).size(), 2U);
    EXPECT_TRUE(database.listForType(ComponentPresetType::TRANSITION).empty());
}

TEST(ExamplePresets, EveryCallMakesItsOwnPresets)
{
    const ComponentPresetDatabase first  = makeExamplePresetDatabase();
    const ComponentPresetDatabase second = makeExamplePresetDatabase();
    EXPECT_NE(first.listAll()[0], second.listAll()[0]);
    EXPECT_EQ(*first.listAll()[0], *second.listAll()[0]);
}

// ============================================================================== the materials

TEST(ExamplePresets, TheNoseConeDensitiesAreTheMassOverTheOgivesVolume)
{
    // The factory replaces the file's material (cast polystyrene, 1050 kg/m3) by one of the
    // same name whose density gives the part its listed mass: mass / NoseCone volume. Exact,
    // see the head of this file.
    const PresetPtr pnc50 = examplePreset("PNC-50YR, 72604");
    const PresetPtr pnc20 = examplePreset("PNC-20, 072606");
    ASSERT_NE(pnc50, nullptr);
    ASSERT_NE(pnc20, nullptr);
    EXPECT_EQ(pnc50->get(ComponentPreset::kMaterial).getDensity(), 756.2852368449444);
    EXPECT_EQ(pnc20->get(ComponentPreset::kMaterial).getDensity(), 682.4826031822124);
    EXPECT_EQ(pnc50->get(ComponentPreset::kMass), 0.007937866468);
    EXPECT_EQ(pnc20->get(ComponentPreset::kMass), 0.0034019427719999998);

    // A new material, found in no database: user-defined, a document material, in the group
    // CUSTOM (Databases.findMaterial()).
    EXPECT_EQ(
        materialLine(pnc50->get(ComponentPreset::kMaterial)),
        R"(BULK "Polystyrene, cast, bulk" group Custom userDefined true documentMaterial true)");
    EXPECT_EQ(
        materialLine(pnc20->get(ComponentPreset::kMaterial)),
        R"(BULK "Polystyrene, cast, bulk" group Custom userDefined true documentMaterial true)");
}

TEST(ExamplePresets, TheTubeAndParachuteMaterialsAreTheFilesDocumentMaterials)
{
    const PresetPtr tube  = examplePreset("BT-50FE, 30359");
    const PresetPtr chute = examplePreset("PK-10, 2262");
    ASSERT_NE(tube, nullptr);
    ASSERT_NE(chute, nullptr);

    EXPECT_EQ(materialLine(tube->get(ComponentPreset::kMaterial)),
              "BULK \"Paper, spiral kraft glassine, Estes avg, bulk\" group Other userDefined "
              "true documentMaterial true");
    EXPECT_EQ(tube->get(ComponentPreset::kMaterial).getDensity(), 894.4);

    EXPECT_EQ(materialLine(chute->get(ComponentPreset::kMaterial)),
              "SURFACE \"Polyethylene film, HDPE, 1.0 mil, bare\" group Other userDefined true "
              "documentMaterial true");
    EXPECT_EQ(chute->get(ComponentPreset::kMaterial).getDensity(), 0.0235);
    EXPECT_EQ(materialLine(chute->get(ComponentPreset::kLineMaterial)),
              R"(LINE "Carpet Thread" group Other userDefined true documentMaterial true)");
    EXPECT_EQ(chute->get(ComponentPreset::kLineMaterial).getDensity(), 3.3E-4);
}

TEST(ExamplePresets, TheFactoryCompletesTheTubesThickness)
{
    // Outer and inner diameter given: the thickness is half their difference.
    const PresetPtr shortTube = examplePreset("BT-50FE, 30359");
    const PresetPtr longTube  = examplePreset("BT-50, 30352");
    ASSERT_NE(shortTube, nullptr);
    ASSERT_NE(longTube, nullptr);
    EXPECT_EQ(shortTube->get(ComponentPreset::kThickness), 3.3019999999999924E-4);
    EXPECT_EQ(shortTube->get(ComponentPreset::kInnerDiameter), 0.02413);
    EXPECT_EQ(shortTube->get(ComponentPreset::kOuterDiameter), 0.024790399999999997);
    EXPECT_EQ(shortTube->get(ComponentPreset::kLength), 0.1651);
    EXPECT_EQ(longTube->get(ComponentPreset::kThickness), 3.3019999999999924E-4);
    EXPECT_EQ(longTube->get(ComponentPreset::kLength), 0.4572);
}

// =========================================================================== into components

TEST(ExamplePresets, ABodyTubeTakesTheTubePreset)
{
    const PresetPtr preset = examplePreset("BT-50FE, 30359");
    ASSERT_NE(preset, nullptr);
    BodyTube tube;
    tube.loadPreset(preset);

    EXPECT_EQ(tube.getPresetComponent(), preset.get());
    EXPECT_EQ(tube.getName(), "Body Tube");
    EXPECT_EQ(tube.getLength(), 0.1651);
    EXPECT_EQ(tube.getOuterRadius(), 0.012395199999999999);
    EXPECT_FALSE(tube.isOuterRadiusAutomatic());
    EXPECT_EQ(tube.getInnerRadius(), 0.012065);
    EXPECT_EQ(tube.getThickness(), 3.3019999999999924E-4);
    EXPECT_FALSE(tube.isFilled());
    EXPECT_EQ(tube.getMaterial(), preset->get(ComponentPreset::kMaterial));
    EXPECT_TRUE(tube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(tube.getComponentVolume(), 4.189228202714561E-6);
    EXPECT_EQ(tube.getComponentMass(), 0.0037468457045079034);
}

TEST(ExamplePresets, TheLongTubeDiffersInItsLength)
{
    const PresetPtr preset = examplePreset("BT-50, 30352");
    ASSERT_NE(preset, nullptr);
    BodyTube tube;
    tube.loadPreset(preset);

    EXPECT_EQ(tube.getLength(), 0.4572);
    EXPECT_EQ(tube.getOuterRadius(), 0.012395199999999999);
    EXPECT_EQ(tube.getThickness(), 3.3019999999999924E-4);
    EXPECT_EQ(tube.getComponentVolume(), 1.160093963828646E-5);
    EXPECT_EQ(tube.getComponentMass(), 0.01037588041248341);
}

TEST(ExamplePresets, AParachuteTakesTheParachutePreset)
{
    // RecoveryDevice and Parachute take a preset's materials only when their text
    // ("name (density unit)") is longer than 12 characters, which depends on the default units.
    const QtRocket::Test::DefaultUnitsGuard units;
    const PresetPtr                         preset = examplePreset("PK-10, 2262");
    ASSERT_NE(preset, nullptr);
    Parachute chute;
    chute.loadPreset(preset);

    EXPECT_EQ(chute.getPresetComponent(), preset.get());
    EXPECT_EQ(chute.getName(), "Parachute, plastic, preassembled, 10 in., PN 2262");
    EXPECT_EQ(chute.getDiameter(), 0.254);
    EXPECT_TRUE(chute.isCDAutomatic());
    EXPECT_EQ(chute.getCD(), 0.8);
    EXPECT_EQ(chute.getLineCount(), 6);
    EXPECT_EQ(chute.getLineLength(), 0.254);
    EXPECT_FALSE(chute.isLineLengthAutomatic());
    EXPECT_EQ(chute.getMaterial(), preset->get(ComponentPreset::kMaterial));
    EXPECT_EQ(chute.getMaterial().getName(), "Polyethylene film, HDPE, 1.0 mil, bare");
    EXPECT_TRUE(chute.getMaterial().isDocumentMaterial());
    EXPECT_EQ(chute.getLineMaterial(), preset->get(ComponentPreset::kLineMaterial));
    EXPECT_EQ(chute.getLineMaterial().getName(), "Carpet Thread");
    EXPECT_TRUE(chute.getLineMaterial().isDocumentMaterial());
    // No packed size and no mass in the preset: the defaults stay and nothing is overridden.
    EXPECT_EQ(chute.getLength(), 0.025);
    EXPECT_EQ(chute.getRadius(), 0.0125);
    EXPECT_FALSE(chute.isRadiusAutomatic());
    EXPECT_FALSE(chute.isMassOverridden());
    EXPECT_EQ(chute.getComponentMass(), 0.0016936825758791197);
}

TEST(ExamplePresets, TheSmallParachuteDiffersInItsSize)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const PresetPtr                         preset = examplePreset("PK-8, 2260");
    ASSERT_NE(preset, nullptr);
    Parachute chute;
    chute.loadPreset(preset);

    EXPECT_EQ(chute.getName(), "Parachute kit, plastic, 8 in., PN 2260");
    EXPECT_EQ(chute.getDiameter(), 0.2032);
    EXPECT_EQ(chute.getLineCount(), 6);
    EXPECT_EQ(chute.getLineLength(), 0.2032);
    EXPECT_EQ(chute.getMaterial().getName(), "Polyethylene film, HDPE, 1.0 mil, bare");
    EXPECT_EQ(chute.getLineMaterial().getName(), "Carpet Thread");
    EXPECT_EQ(chute.getComponentMass(), 0.0011644240485626364);
}

TEST(ExamplePresets, ANoseConeTakesTheNoseConePreset)
{
    const PresetPtr preset = examplePreset("PNC-50YR, 72604");
    ASSERT_NE(preset, nullptr);
    NoseCone nose;
    nose.loadPreset(preset);

    EXPECT_EQ(nose.getPresetComponent(), preset.get());
    EXPECT_EQ(nose.getName(), "Nose Cone");
    EXPECT_EQ(nose.getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(nose.getShapeParameter(), 1.0);
    EXPECT_EQ(nose.getLength(), 0.104775);
    EXPECT_EQ(nose.getAftRadius(), 0.012395199999999999);
    EXPECT_FALSE(nose.isAftRadiusAutomatic());
    EXPECT_EQ(nose.getForeRadius(), 0.0);
    EXPECT_EQ(nose.getThickness(), 0.0015748);
    EXPECT_FALSE(nose.isFilled());
    EXPECT_EQ(nose.getAftShoulderLength(), 0.019049999999999997);
    EXPECT_EQ(nose.getAftShoulderRadius(), 0.012065);
    EXPECT_EQ(nose.getAftShoulderThickness(), 0.0015748);
    // The one value of these components that only the preset supplies: an .ork file stores no
    // fore shoulder of a nose cone, so without the preset it stays 0.
    EXPECT_EQ(nose.getForeShoulderThickness(), 0.0015748);
    EXPECT_EQ(nose.getForeShoulderLength(), 0.0);
    EXPECT_EQ(nose.getMaterial().getDensity(), 756.2852368449444);
    EXPECT_EQ(nose.getMaterial().getGroup(), MaterialGroup::CUSTOM);

    // The component the factory measured: its volume, and so the listed mass again.
    EXPECT_EQ(nose.getComponentVolume(), 1.0495863308287005E-5);
    EXPECT_EQ(nose.getComponentMass(), 0.007937866468);
    EXPECT_EQ(0.007937866468 / nose.getComponentVolume(), 756.2852368449444);
}

TEST(ExamplePresets, TheSmallNoseConeHasItsOwnDensity)
{
    const PresetPtr preset = examplePreset("PNC-20, 072606");
    ASSERT_NE(preset, nullptr);
    NoseCone nose;
    nose.loadPreset(preset);

    EXPECT_EQ(nose.getLength(), 0.06731);
    EXPECT_EQ(nose.getAftRadius(), 0.0093472);
    EXPECT_EQ(nose.getThickness(), 0.0015748);
    EXPECT_EQ(nose.getAftShoulderLength(), 0.0127);
    EXPECT_EQ(nose.getAftShoulderRadius(), 0.009016999999999999);
    EXPECT_EQ(nose.getForeShoulderThickness(), 0.0015748);
    EXPECT_EQ(nose.getMaterial().getDensity(), 682.4826031822124);
    EXPECT_EQ(nose.getComponentVolume(), 4.9846585922304205E-6);
    EXPECT_EQ(nose.getComponentMass(), 0.0034019427719999998);
    EXPECT_EQ(0.0034019427719999998 / nose.getComponentVolume(), 682.4826031822124);
}

TEST(ExamplePresets, AComponentKeepsItsPresetWhenTheDatabaseIsGone)
{
    BodyTube tube;
    {
        const ComponentPresetDatabase database = makeExamplePresetDatabase();
        tube.loadPreset(database.find("Estes", "BT-50, 30352").at(0));
    }
    ASSERT_NE(tube.getPresetComponent(), nullptr);
    EXPECT_EQ(tube.getPresetComponent()->getDigest(), "a59dec8e4034a2fee5955dbf4ff07f1c");
    EXPECT_EQ(tube.getPresetComponent()->toOrkElement(),
              R"(<preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" )"
              R"(digest="a59dec8e4034a2fee5955dbf4ff07f1c"/>)");
}

}  // namespace
