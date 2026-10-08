#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Signal.h"
#include "unit/DefaultUnitsGuard.h"

// Rocket::documentMaterialSet(): the signal that stands for OpenRocket's
// rocket.getDocument().getDocumentPreferences().addMaterial(material) in the material setters and
// preset loaders of ExternalComponent, StructuralComponent, RecoveryDevice and Parachute (eight
// places). OpenRocket's only tests of it are MaterialTest.testDocumentDatabase and
// testLoadFromPreset, which need a document and are ported in full in
// tests/core/document/open_rocket_document_material_tests.cpp. The expectations here are what
// OpenRocket itself does for the same steps: DocumentMaterialProbe.java of the tier 9a probes
// (commit 5f164fd0e, JDK 17) watches the three material databases of a document's preferences
// and prints what each step registers; the comments name its parts (A to K).

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::CenteringRing;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::MaterialDatabase;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Streamer;
using QtRocket::TransitionShape;
using QtRocket::TubeCoupler;
using QtRocket::TypedPropertyMap;

using MaterialSignal = QtRocket::Signal<const Material&>;
using PresetPtr      = std::shared_ptr<const ComponentPreset>;

// ---- materials

/// A user-defined document material without a group (the probe's doc()).
[[nodiscard]] Material doc(Material::Type type, std::string name, double density)
{
    return Material::newMaterial(type, std::move(name), density, true, true);
}

/// A user-defined material that is not a document material.
[[nodiscard]] Material user(Material::Type type, std::string name, double density)
{
    return Material::newMaterial(type, std::move(name), density, true);
}

/// A document material as an .orc preset file gives one: user-defined, in the group OTHER.
[[nodiscard]] Material presetMaterial(Material::Type type, std::string name, double density)
{
    return Material::newMaterial(type, std::move(name), density, MaterialGroup::OTHER, true, true);
}

// ---- presets (the probe's tubePreset(), streamerPreset(), chutePreset() and nosePreset())

[[nodiscard]] PresetPtr make(const TypedPropertyMap& props)
{
    const MaterialStorage materials;
    return std::make_shared<const ComponentPreset>(
        ComponentPresetFactory::create(props, materials).value());
}

[[nodiscard]] TypedPropertyMap base(const std::string& partNo, ComponentPresetType type)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kType, type);
    return props;
}

/// A tube-like preset (BODY_TUBE, CENTERING_RING, TUBE_COUPLER) with @p material when given.
[[nodiscard]] PresetPtr tubePreset(const std::string& partNo, ComponentPresetType type,
                                   const std::optional<Material>& material)
{
    TypedPropertyMap props = base(partNo, type);
    props.put(ComponentPreset::kLength, 0.3);
    props.put(ComponentPreset::kOuterDiameter, 0.0247);
    props.put(ComponentPreset::kInnerDiameter, 0.0237);
    if (material.has_value())
    {
        props.put(ComponentPreset::kMaterial, *material);
    }
    return make(props);
}

[[nodiscard]] PresetPtr streamerPreset(const std::string&             partNo,
                                       const std::optional<Material>& material)
{
    TypedPropertyMap props = base(partNo, ComponentPresetType::STREAMER);
    props.put(ComponentPreset::kLength, 0.5);
    props.put(ComponentPreset::kWidth, 0.05);
    if (material.has_value())
    {
        props.put(ComponentPreset::kMaterial, *material);
    }
    return make(props);
}

[[nodiscard]] PresetPtr chutePreset(const std::string&             partNo,
                                    const std::optional<Material>& canopy,
                                    const std::optional<Material>& line)
{
    TypedPropertyMap props = base(partNo, ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, 0.3);
    props.put(ComponentPreset::kLineCount, 6);
    props.put(ComponentPreset::kLineLength, 0.3);
    if (canopy.has_value())
    {
        props.put(ComponentPreset::kMaterial, *canopy);
    }
    if (line.has_value())
    {
        props.put(ComponentPreset::kLineMaterial, *line);
    }
    return make(props);
}

/// A nose cone preset with a mass, which the factory turns into a new material (a document
/// material) of the density that gives the mass.
[[nodiscard]] PresetPtr nosePreset(const std::string& partNo)
{
    TypedPropertyMap props = base(partNo, ComponentPresetType::NOSE_CONE);
    props.put(ComponentPreset::kMaterial,
              presetMaterial(Material::Type::BULK, "Polystyrene, cast, bulk", 1050.0));
    props.put(ComponentPreset::kMass, 0.007937866468);
    props.put(ComponentPreset::kShape, TransitionShape::OGIVE);
    props.put(ComponentPreset::kAftOuterDiameter, 0.024790399999999997);
    props.put(ComponentPreset::kLength, 0.104775);
    props.put(ComponentPreset::kThickness, 0.0015748);
    return make(props);
}

/// "TYPE name" of a material, as the probe prints a registration.
[[nodiscard]] std::string describe(const Material& material)
{
    return std::format("{} {}", toString(material.getType()), material.getName());
}

/// The materials of @p database as "name density group", in order.
[[nodiscard]] std::vector<std::string> contents(const MaterialDatabase& database)
{
    std::vector<std::string> result;
    for (const Material& material : database)
    {
        result.push_back(std::format("{} {} {}", material.getName(), material.getDensity(),
                                     databaseString(material.getGroup())));
    }
    return result;
}

/// Records what a rocket announces on documentMaterialSet(), as the probe's database listeners
/// record what the document registers.
class Recorder
{
public:
    explicit Recorder(Rocket& rocket)
      : m_connection(rocket.documentMaterialSet().connect(
            [this](const Material& material) { m_announced.push_back(describe(material)); }))
    {
    }

    /// What was announced since the last call, joined with ", "; empty when nothing was.
    [[nodiscard]] std::string taken()
    {
        std::string joined;
        for (const std::string& entry : m_announced)
        {
            joined += (joined.empty() ? "" : ", ");
            joined += entry;
        }
        m_announced.clear();
        return joined;
    }

private:
    std::vector<std::string>         m_announced;
    MaterialSignal::ScopedConnection m_connection;
};

/// The probe's rocket: a stage with a nose cone and a body tube that holds a centering ring, a
/// tube coupler, a parachute and a streamer; events enabled, as a document's constructor
/// leaves them, and a recorder in the document's place.
class DocumentMaterialTest : public ::testing::Test
{
protected:
    DocumentMaterialTest()
      : m_stage(&m_rocket.addChild(std::make_unique<AxialStage>())),
        m_nose(&m_stage->addChild(std::make_unique<NoseCone>())),
        m_tube(&m_stage->addChild(std::make_unique<BodyTube>(0.3, 0.02, 0.001))),
        m_ring(&m_tube->addChild(std::make_unique<CenteringRing>())),
        m_coupler(&m_tube->addChild(std::make_unique<TubeCoupler>())),
        m_chute(&m_tube->addChild(std::make_unique<Parachute>())),
        m_streamer(&m_tube->addChild(std::make_unique<Streamer>())),
        m_recorder(m_rocket)
    {
        m_rocket.enableEvents();
    }

    /// What the rocket announced since the last call (the probe's "registered [...]").
    [[nodiscard]] std::string taken() { return m_recorder.taken(); }

    // RecoveryDevice and Parachute take a preset's material only when its text is longer than 12
    // characters, which depends on the default units.
    QtRocket::Test::DefaultUnitsGuard m_units;
    Rocket                            m_rocket;
    AxialStage*                       m_stage;
    NoseCone*                         m_nose;
    BodyTube*                         m_tube;
    CenteringRing*                    m_ring;
    TubeCoupler*                      m_coupler;
    Parachute*                        m_chute;
    Streamer*                         m_streamer;
    Recorder                          m_recorder;
};

// =========================================================== A: ExternalComponent::setMaterial()

TEST_F(DocumentMaterialTest, SettingADocumentMaterialAnnouncesItOnce)
{
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk", 123.0));
    EXPECT_EQ(m_tube->getMaterial().getName(), "DocBulk");
    EXPECT_EQ(taken(), "BULK DocBulk");
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, AnEqualMaterialIsNotAnnouncedAgain)
{
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk", 123.0));
    EXPECT_EQ(taken(), "BULK DocBulk");

    // The setter returns before it gets that far when the material equals the current one, also
    // when the document no longer has it (the probe removes it between the two calls).
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk", 123.0));
    EXPECT_EQ(taken(), "");

    // Equal but for the document flag, which Material's equality does not look at: nothing
    // happens, and the tube keeps the material it had.
    m_tube->setMaterial(user(Material::Type::BULK, "DocBulk", 123.0));
    EXPECT_EQ(taken(), "");
    EXPECT_TRUE(m_tube->getMaterial().isDocumentMaterial());
}

TEST_F(DocumentMaterialTest, AMaterialThatIsNoDocumentMaterialIsNotAnnounced)
{
    m_tube->setMaterial(user(Material::Type::BULK, "UserBulk", 456.0));
    EXPECT_EQ(m_tube->getMaterial().getName(), "UserBulk");
    EXPECT_EQ(taken(), "");

    // The probe's built-in look-alike: the same name and density as a document material, but
    // not user-defined (so in another group) and no document material.
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk", 123.0));
    EXPECT_EQ(taken(), "BULK DocBulk");
    m_tube->setMaterial(
        Material::newMaterial(Material::Type::BULK, "DocBulk", 123.0, false, false));
    EXPECT_FALSE(m_tube->getMaterial().isDocumentMaterial());
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, TheDocumentFlagAloneDecides)
{
    // A document material that is not user-defined is announced like any other.
    m_tube->setMaterial(
        Material::newMaterial(Material::Type::BULK, "BuiltinDoc", 5.0, false, true));
    EXPECT_EQ(taken(), "BULK BuiltinDoc");
}

TEST_F(DocumentMaterialTest, EverySetIsAnnouncedInOrder)
{
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk", 123.0));
    m_tube->setMaterial(doc(Material::Type::BULK, "DocBulk2", 124.0));
    EXPECT_EQ(taken(), "BULK DocBulk, BULK DocBulk2");
}

TEST_F(DocumentMaterialTest, TheSameMaterialOnTwoComponentsIsAnnouncedTwiceAndStoredOnce)
{
    // The document adds what it hears to its materials (a MaterialStorage), which refuses a
    // material it has: OpenRocket's count is 1 after the two calls.
    MaterialStorage                        documentMaterials;
    const MaterialSignal::ScopedConnection connection{
        m_rocket.documentMaterialSet().connect([&documentMaterials](const Material& material) {
            documentMaterials.addMaterial(material);
        })};

    m_tube->setMaterial(doc(Material::Type::BULK, "Twice", 7.0));
    m_nose->setMaterial(doc(Material::Type::BULK, "Twice", 7.0));
    EXPECT_EQ(taken(), "BULK Twice, BULK Twice");
    EXPECT_EQ(documentMaterials.totalMaterialCount(), 1U);
}

// ================================================= B: events disabled, frozen, bypassing events

TEST_F(DocumentMaterialTest, AnnouncedWhileEventsAreDisabled)
{
    m_rocket.enableEvents(false);
    m_tube->setMaterial(doc(Material::Type::BULK, "WhileDisabled", 1.0));
    EXPECT_EQ(taken(), "BULK WhileDisabled");
}

TEST_F(DocumentMaterialTest, AnnouncedAtOnceWhileTheRocketIsFrozen)
{
    m_rocket.freeze();
    m_tube->setMaterial(doc(Material::Type::BULK, "WhileFrozen", 2.0));
    // Before the thaw, which adds nothing.
    EXPECT_EQ(taken(), "BULK WhileFrozen");
    m_rocket.thaw();
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, AnnouncedWhileTheComponentBypassesItsChangeEvents)
{
    m_tube->setBypassChangeEvent(true);
    m_tube->setMaterial(doc(Material::Type::BULK, "WhileBypassing", 3.0));
    m_tube->setBypassChangeEvent(false);
    EXPECT_EQ(taken(), "BULK WhileBypassing");
}

TEST_F(DocumentMaterialTest, ResettingTheChangeListenersKeepsTheDocumentsConnection)
{
    // Java's resetListeners() empties the listener list; the rocket still knows its document.
    int                                           events = 0;
    const ComponentChangeSignal::ScopedConnection listener{m_rocket.addComponentChangeListener(
        [&events](const ComponentChangeEvent& /*event*/) { ++events; })};
    m_rocket.resetListeners();

    m_tube->setMaterial(doc(Material::Type::BULK, "AfterReset", 3.5));
    EXPECT_EQ(events, 0);
    EXPECT_EQ(taken(), "BULK AfterReset");
}

// ============================================================ C: components outside the rocket

TEST_F(DocumentMaterialTest, AComponentOutsideTheRocketAnnouncesNothing)
{
    // A detached component.
    auto loose = std::make_unique<BodyTube>();
    loose->setMaterial(doc(Material::Type::BULK, "Loose", 4.0));
    EXPECT_EQ(taken(), "");

    // A component whose root is not a Rocket.
    AxialStage looseStage;
    auto&      inLooseStage = looseStage.addChild(std::make_unique<BodyTube>());
    inLooseStage.setMaterial(doc(Material::Type::BULK, "InLooseStage", 5.0));
    EXPECT_EQ(taken(), "");

    // Adding a component that already has a document material announces nothing either.
    m_stage->addChild(std::move(loose));
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, EachRocketAnnouncesItsOwn)
{
    Rocket other;
    auto&  otherStage = other.addChild(std::make_unique<AxialStage>());
    auto&  otherTube  = otherStage.addChild(std::make_unique<BodyTube>());

    // A rocket nobody listens to (one without a document): nothing happens.
    EXPECT_TRUE(other.documentMaterialSet().empty());
    otherTube.setMaterial(doc(Material::Type::BULK, "OtherRocket", 6.0));
    EXPECT_EQ(taken(), "");

    Recorder otherRecorder{other};
    otherTube.setMaterial(doc(Material::Type::BULK, "OtherRocket2", 6.5));
    EXPECT_EQ(otherRecorder.taken(), "BULK OtherRocket2");
    EXPECT_EQ(taken(), "");
}

// ========================================================= D: StructuralComponent::setMaterial()

TEST_F(DocumentMaterialTest, AStructuralComponentAnnouncesItsDocumentMaterial)
{
    m_ring->setMaterial(doc(Material::Type::BULK, "RingDoc", 10.0));
    EXPECT_EQ(taken(), "BULK RingDoc");
    m_ring->setMaterial(doc(Material::Type::BULK, "RingDoc", 10.0));
    EXPECT_EQ(taken(), "");
    m_ring->setMaterial(user(Material::Type::BULK, "RingUser", 11.0));
    EXPECT_EQ(taken(), "");

    m_coupler->setMaterial(doc(Material::Type::BULK, "CouplerDoc", 12.0));
    EXPECT_EQ(taken(), "BULK CouplerDoc");
}

// ============================================================ E: RecoveryDevice::setMaterial()

TEST_F(DocumentMaterialTest, ARecoveryDeviceAnnouncesItsDocumentMaterial)
{
    m_chute->setMaterial(doc(Material::Type::SURFACE, "CanopyDoc", 0.05));
    EXPECT_EQ(taken(), "SURFACE CanopyDoc");
    m_chute->setMaterial(doc(Material::Type::SURFACE, "CanopyDoc", 0.05));
    EXPECT_EQ(taken(), "");
    m_chute->setMaterial(user(Material::Type::SURFACE, "CanopyUser", 0.06));
    EXPECT_EQ(taken(), "");

    m_streamer->setMaterial(doc(Material::Type::SURFACE, "StreamerDoc", 0.07));
    EXPECT_EQ(taken(), "SURFACE StreamerDoc");
}

// ========================================================== F: Parachute::setLineMaterial()

TEST_F(DocumentMaterialTest, AParachuteAnnouncesItsLineMaterialWithLinesOrWithout)
{
    ASSERT_EQ(m_chute->getLineCount(), 6);
    m_chute->setLineMaterial(doc(Material::Type::LINE, "LineDoc", 0.002));
    EXPECT_EQ(taken(), "LINE LineDoc");
    m_chute->setLineMaterial(doc(Material::Type::LINE, "LineDoc", 0.002));
    EXPECT_EQ(taken(), "");
    m_chute->setLineMaterial(user(Material::Type::LINE, "LineUser", 0.003));
    EXPECT_EQ(taken(), "");

    // Without lines the setter fires another event and keeps the preset, but the material is
    // announced all the same.
    m_chute->setLineCount(0);
    m_chute->setLineMaterial(doc(Material::Type::LINE, "LineDocNoLines", 0.004));
    EXPECT_EQ(taken(), "LINE LineDocNoLines");
}

// ======================================================================== G: loadFromPreset()

TEST_F(DocumentMaterialTest, AnExternalComponentAnnouncesItsPresetsDocumentMaterial)
{
    const Material  paper   = presetMaterial(Material::Type::BULK, "PresetPaper", 894.4);
    const PresetPtr tubeDoc = tubePreset("T-DOC", ComponentPresetType::BODY_TUBE, paper);
    m_tube->loadPreset(tubeDoc);
    EXPECT_EQ(taken(), "BULK PresetPaper");

    // The same preset again: nothing is loaded.
    m_tube->loadPreset(tubeDoc);
    EXPECT_EQ(taken(), "");

    // Another preset with the material the tube already has: a preset's material is announced
    // whatever the component had (loadFromPreset() has no "same material" return).
    m_tube->loadPreset(tubePreset("T-DOC2", ComponentPresetType::BODY_TUBE, paper));
    EXPECT_EQ(taken(), "BULK PresetPaper");

    // A preset whose material is no document material, and one without a material.
    m_tube->loadPreset(tubePreset("T-USER", ComponentPresetType::BODY_TUBE,
                                  user(Material::Type::BULK, "PresetUser", 700.0)));
    EXPECT_EQ(taken(), "");
    m_tube->loadPreset(tubePreset("T-NONE", ComponentPresetType::BODY_TUBE, std::nullopt));
    EXPECT_EQ(taken(), "");
    EXPECT_EQ(m_tube->getMaterial().getName(), "PresetUser");
}

TEST_F(DocumentMaterialTest, ANoseConePresetsComputedMaterialIsAnnounced)
{
    // The factory turned the preset's mass into a material of its own (mass / volume), a
    // document material: it reaches the document with the preset.
    m_nose->loadPreset(nosePreset("NC-MASS"));
    EXPECT_EQ(taken(), "BULK Polystyrene, cast, bulk");
    EXPECT_EQ(m_nose->getMaterial().getName(), "Polystyrene, cast, bulk");
    // + - * / and sqrt only (see example_presets_tests.cpp), so the pin is exact.
    EXPECT_EQ(m_nose->getMaterial().getDensity(), 1014.3383946535503);
    EXPECT_EQ(m_nose->getMaterial().getGroup(), MaterialGroup::CUSTOM);
    EXPECT_TRUE(m_nose->getMaterial().isDocumentMaterial());
}

TEST_F(DocumentMaterialTest, AStructuralComponentAnnouncesItsPresetsDocumentMaterial)
{
    m_ring->loadPreset(tubePreset("CR-DOC", ComponentPresetType::CENTERING_RING,
                                  presetMaterial(Material::Type::BULK, "PresetPlywood", 630.0)));
    EXPECT_EQ(taken(), "BULK PresetPlywood");
    EXPECT_EQ(m_ring->getMaterial().getName(), "PresetPlywood");

    m_coupler->loadPreset(tubePreset("TC-DOC", ComponentPresetType::TUBE_COUPLER,
                                     presetMaterial(Material::Type::BULK, "PresetCoupler", 800.0)));
    EXPECT_EQ(taken(), "BULK PresetCoupler");

    m_ring->loadPreset(tubePreset("CR-USER", ComponentPresetType::CENTERING_RING,
                                  user(Material::Type::BULK, "PresetUser", 700.0)));
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, ARecoveryDeviceAnnouncesItsPresetsDocumentMaterial)
{
    m_streamer->loadPreset(
        streamerPreset("S-DOC", presetMaterial(Material::Type::SURFACE, "PresetMylar", 0.021)));
    EXPECT_EQ(taken(), "SURFACE PresetMylar");

    // Without a material the streamer falls back to its default material, which is not
    // announced.
    m_streamer->loadPreset(streamerPreset("S-NONE", std::nullopt));
    EXPECT_EQ(taken(), "");
    EXPECT_EQ(m_streamer->getMaterial().getName(), "Ripstop nylon");
}

TEST_F(DocumentMaterialTest, AParachutePresetAnnouncesTheCanopyMaterialBeforeTheLineMaterial)
{
    const Material canopy = presetMaterial(Material::Type::SURFACE, "PresetCanopy", 0.0235);
    const Material line   = presetMaterial(Material::Type::LINE, "PresetThread", 3.3E-4);

    m_chute->loadPreset(chutePreset("P-DOC", canopy, line));
    EXPECT_EQ(taken(), "SURFACE PresetCanopy, LINE PresetThread");

    m_chute->loadPreset(chutePreset("P-CANOPY", canopy, std::nullopt));
    EXPECT_EQ(taken(), "SURFACE PresetCanopy");
    m_chute->loadPreset(chutePreset("P-LINE", std::nullopt, line));
    EXPECT_EQ(taken(), "LINE PresetThread");
    m_chute->loadPreset(chutePreset("P-NONE", std::nullopt, std::nullopt));
    EXPECT_EQ(taken(), "");
}

TEST_F(DocumentMaterialTest, PresetMaterialsThatPrintShortAreNeitherTakenNorAnnounced)
{
    // "NEED a better way to set preset if field is empty" (OpenRocket): a material whose text
    // has 12 characters or fewer counts as missing. Without a name the text is the density in
    // parentheses alone, 11 characters for the canopy (500 g per square metre) and 10 for the
    // line (" (500 g/m)"); the parachute gets its default materials.
    m_chute->loadPreset(chutePreset("P-TINY", presetMaterial(Material::Type::SURFACE, "", 0.5),
                                    presetMaterial(Material::Type::LINE, "", 0.5)));
    EXPECT_EQ(taken(), "");
    EXPECT_EQ(m_chute->getMaterial().getName(), "Ripstop nylon");
    EXPECT_EQ(m_chute->getLineMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");
}

// ============================================== H: the order while a file is loaded

TEST_F(DocumentMaterialTest, WhileAFileLoadsThePresetsMaterialComesBeforeTheFiles)
{
    // What the .ork loader does with <preset> followed by <material>: the preset is loaded with
    // preset clearing ignored, then the file's material is set, here the same name and density
    // in another group (the case of "3D printable nose cone and fins.ork").
    MaterialStorage                        documentMaterials;
    const MaterialSignal::ScopedConnection connection{
        m_rocket.documentMaterialSet().connect([&documentMaterials](const Material& material) {
            documentMaterials.addMaterial(material);
        })};
    auto& loaded = m_stage->addChild(std::make_unique<BodyTube>());

    loaded.setIgnorePresetClearing(true);
    loaded.loadPreset(tubePreset("T-FILE", ComponentPresetType::BODY_TUBE,
                                 presetMaterial(Material::Type::BULK, "PresetPaper", 894.4)));
    EXPECT_EQ(taken(), "BULK PresetPaper");
    loaded.setMaterial(Material::newMaterial(Material::Type::BULK, "PresetPaper", 894.4,
                                             MaterialGroup::CUSTOM, true, true));
    loaded.setIgnorePresetClearing(false);
    EXPECT_EQ(taken(), "BULK PresetPaper");
    EXPECT_NE(loaded.getPresetComponent(), nullptr);
    EXPECT_EQ(loaded.getMaterial().getGroup(), MaterialGroup::CUSTOM);

    // Both are kept, as they differ in the group, and the later one stands first: the two
    // compare equal (name and density), so Database.add() puts the new one at the index of the
    // one it found. This is the order a save writes <docmaterials> in.
    const std::vector<std::string> expected{"PresetPaper 894.4 Custom", "PresetPaper 894.4 Other"};
    EXPECT_EQ(contents(documentMaterials.bulkMaterials()), expected);
}

// =============================================== K: the moment of the announcement

/// A body tube with a parachute in it and the probe's centering ring, each based on a preset
/// without materials, and one log of what the rocket announces (with the state of the
/// components at that moment) and of its change events.
class DocumentMaterialMomentTest : public DocumentMaterialTest
{
protected:
    DocumentMaterialMomentTest()
      : m_k(&m_stage->addChild(std::make_unique<BodyTube>())),
        m_kc(&m_k->addChild(std::make_unique<Parachute>()))
    {
        m_k->loadPreset(tubePreset("T-K", ComponentPresetType::BODY_TUBE, std::nullopt));
        m_kc->loadPreset(chutePreset("P-K", std::nullopt, std::nullopt));
        m_ring->loadPreset(tubePreset("CR-K", ComponentPresetType::CENTERING_RING, std::nullopt));
        m_materialConnection = m_rocket.documentMaterialSet().connect(
            [this](const Material& material) { m_log.push_back(registration(material)); });
        m_eventConnection =
            m_rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
                m_log.push_back(std::format("event type {} from {}", event.getType(),
                                            event.getSource()->getComponentName()));
            });
    }

    /// The probe's line for a registration.
    [[nodiscard]] std::string registration(const Material& material) const
    {
        return std::format(
            "registered {}: tube has {}, tube preset {}, lines have {}, canopy has "
            "{}, chute preset {}",
            material.getName(), m_k->getMaterial().getName(), m_k->getPresetComponent() != nullptr,
            m_kc->getLineMaterial().getName(), m_kc->getMaterial().getName(),
            m_kc->getPresetComponent() != nullptr);
    }

    /// The log since the last call.
    [[nodiscard]] std::vector<std::string> log() { return std::exchange(m_log, {}); }

    BodyTube*                               m_k;
    Parachute*                              m_kc;
    std::vector<std::string>                m_log;
    MaterialSignal::ScopedConnection        m_materialConnection;
    ComponentChangeSignal::ScopedConnection m_eventConnection;
};

TEST_F(DocumentMaterialMomentTest, ASetterAnnouncesAfterTheAssignmentAndBeforeThePresetIsCleared)
{
    // The component holds the new material, still has its preset, and no event has fired; then
    // NONFUNCTIONAL_CHANGE (1) of clearPreset() and the setter's MASS_CHANGE (2).
    m_k->setMaterial(doc(Material::Type::BULK, "MomentBulk", 31.0));
    const std::vector<std::string> external{
        "registered MomentBulk: tube has MomentBulk, tube preset true, lines have Elastic cord "
        "(round 2 mm, 1/16 in), canopy has Ripstop nylon, chute preset true",
        "event type 1 from Body Tube", "event type 2 from Body Tube"};
    EXPECT_EQ(log(), external);

    m_kc->setMaterial(doc(Material::Type::SURFACE, "MomentCanopy", 0.031));
    const std::vector<std::string> recovery{
        "registered MomentCanopy: tube has MomentBulk, tube preset false, lines have Elastic "
        "cord (round 2 mm, 1/16 in), canopy has MomentCanopy, chute preset true",
        "event type 1 from Parachute", "event type 2 from Parachute"};
    EXPECT_EQ(log(), recovery);

    m_ring->setMaterial(doc(Material::Type::BULK, "MomentRing", 32.0));
    const std::vector<std::string> structural{
        "registered MomentRing: tube has MomentBulk, tube preset false, lines have Elastic cord "
        "(round 2 mm, 1/16 in), canopy has MomentCanopy, chute preset false",
        "event type 1 from Centering Ring", "event type 2 from Centering Ring"};
    EXPECT_EQ(log(), structural);
}

TEST_F(DocumentMaterialMomentTest, TheLineMaterialIsAnnouncedBeforeThePresetIsCleared)
{
    m_kc->setLineMaterial(doc(Material::Type::LINE, "MomentLine", 0.0031));
    const std::vector<std::string> expected{
        "registered MomentLine: tube has Cardboard, tube preset true, lines have MomentLine, "
        "canopy has Ripstop nylon, chute preset true",
        "event type 1 from Parachute", "event type 2 from Parachute"};
    EXPECT_EQ(log(), expected);
}

TEST_F(DocumentMaterialMomentTest, APresetAnnouncesWhileItIsLoadedBeforeTheEvents)
{
    // During loadFromPreset(): the component has the preset's material but not yet the preset
    // (the old one went with the setter before). The rocket is frozen meanwhile, so the events
    // follow: the combined change of the load (6) and loadPreset()'s NONFUNCTIONAL_CHANGE (1).
    m_k->setMaterial(doc(Material::Type::BULK, "MomentBulk", 31.0));
    static_cast<void>(log());
    m_k->loadPreset(tubePreset("T-K2", ComponentPresetType::BODY_TUBE,
                               presetMaterial(Material::Type::BULK, "MomentPreset", 33.0)));
    const std::vector<std::string> tube{
        "registered MomentPreset: tube has MomentPreset, tube preset false, lines have Elastic "
        "cord (round 2 mm, 1/16 in), canopy has Ripstop nylon, chute preset true",
        "event type 6 from Body Tube", "event type 1 from Body Tube"};
    EXPECT_EQ(log(), tube);

    m_kc->setLineMaterial(doc(Material::Type::LINE, "MomentLine", 0.0031));
    static_cast<void>(log());
    m_kc->loadPreset(
        chutePreset("P-K3", presetMaterial(Material::Type::SURFACE, "MomentPresetCanopy", 0.033),
                    presetMaterial(Material::Type::LINE, "MomentPresetLine", 0.0033)));
    const std::vector<std::string> chute{
        "registered MomentPresetCanopy: tube has MomentPreset, tube preset true, lines have "
        "MomentLine, canopy has MomentPresetCanopy, chute preset false",
        "registered MomentPresetLine: tube has MomentPreset, tube preset true, lines have "
        "MomentPresetLine, canopy has MomentPresetCanopy, chute preset false",
        "event type 6 from Parachute", "event type 1 from Parachute"};
    EXPECT_EQ(log(), chute);
}

// ============================================= MaterialTest, with a stand-in for the document

/// What MaterialTest's two document tests need of OpenRocketDocumentFactory.createNewRocket():
/// a rocket with one stage, events enabled, whose document adds the document materials it
/// hears of to its material databases. Here a MaterialStorage connected to the rocket's signal
/// takes the document's place, so that the signal is tested without the subsystem above it
/// (tests/core/document/open_rocket_document_material_tests.cpp has the two tests with the
/// document itself, tests/core/material/MaterialTests.cpp the material side alone).
class StandInDocumentTest : public ::testing::Test
{
protected:
    StandInDocumentTest()
      : m_stage(&m_rocket.addChild(std::make_unique<AxialStage>())),
        m_connection(m_rocket.documentMaterialSet().connect(
            [this](const Material& material) { m_documentMaterials.addMaterial(material); }))
    {
        m_rocket.enableEvents();
    }

    /// document.getDocumentPreferences().getAllMaterials().size()
    [[nodiscard]] std::size_t allMaterialsSize() const
    {
        return m_documentMaterials.allMaterials().size();
    }

    static constexpr double kEpsilon = 1e-6;

    Rocket                           m_rocket;
    AxialStage*                      m_stage;
    MaterialStorage                  m_documentMaterials;
    MaterialSignal::ScopedConnection m_connection;
};

// Java: MaterialTest.testDocumentDatabase
TEST_F(StandInDocumentTest, DocumentDatabase)
{
    auto& bodyTube = m_stage->addChild(std::make_unique<BodyTube>());

    // Check current document material database
    EXPECT_EQ(0U, allMaterialsSize());

    // Create a material
    std::string          name    = "Tube Material";
    const Material::Type type    = Material::Type::BULK;
    double               density = 314;
    const Material       m       = Material::newMaterial(type, name, density, true, true);

    // Check document material database
    EXPECT_EQ(0U, allMaterialsSize());

    // Assign material to body tube
    bodyTube.setMaterial(m);
    EXPECT_EQ(1U, allMaterialsSize());
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
    EXPECT_EQ(2U, allMaterialsSize());
    EXPECT_EQ(name, bodyTube.getMaterial().getName());
    EXPECT_EQ(type, bodyTube.getMaterial().getType());
    EXPECT_NEAR(density, bodyTube.getMaterial().getDensity(), kEpsilon);
    EXPECT_TRUE(bodyTube.getMaterial().isUserDefined());
    EXPECT_TRUE(bodyTube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, bodyTube.getMaterial().getGroup());

    // Remove a material
    m_documentMaterials.removeMaterial(m);
    EXPECT_EQ(1U, allMaterialsSize());
}

// Java: MaterialTest.testLoadFromPreset (with createPreset())
TEST_F(StandInDocumentTest, LoadFromPreset)
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
    const PresetPtr preset = make(presetspec);

    auto& bodyTube = m_stage->addChild(std::make_unique<BodyTube>());

    // Check document material database
    EXPECT_EQ(0U, allMaterialsSize());

    // Load from preset (Java's assertSame: materials are values here)
    bodyTube.loadPreset(preset);
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial), bodyTube.getMaterial());
    EXPECT_EQ("Preset Material", bodyTube.getMaterial().getName());
    EXPECT_EQ(Material::Type::BULK, bodyTube.getMaterial().getType());
    EXPECT_NEAR(980, bodyTube.getMaterial().getDensity(), kEpsilon);
    EXPECT_TRUE(bodyTube.getMaterial().isUserDefined());
    EXPECT_TRUE(bodyTube.getMaterial().isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, bodyTube.getMaterial().getGroup());

    // Check document material database
    EXPECT_EQ(1U, allMaterialsSize());
}

// ===================================================================== the slot's argument

TEST_F(DocumentMaterialTest, ASlotMayChangeTheComponentItHearsAbout)
{
    // The slot gets a copy of the material: a slot that sets another material on the component
    // does not change what the slots after it are told.
    const MaterialSignal::ScopedConnection changer{
        m_rocket.documentMaterialSet().connect([this](const Material& material) {
            if (material.getName() == "First")
            {
                m_tube->setMaterial(user(Material::Type::BULK, "FromTheSlot", 9.0));
            }
        })};
    std::vector<std::string>               later;
    const MaterialSignal::ScopedConnection after{m_rocket.documentMaterialSet().connect(
        [&later](const Material& material) { later.push_back(describe(material)); })};

    m_tube->setMaterial(doc(Material::Type::BULK, "First", 8.0));
    EXPECT_EQ(later, std::vector<std::string>{"BULK First"});
    EXPECT_EQ(taken(), "BULK First");
    EXPECT_EQ(m_tube->getMaterial().getName(), "FromTheSlot");
}

// ======================================================================== I: copies of the rocket

TEST_F(DocumentMaterialTest, ACopyOfTheRocketHasNoListeners)
{
    // Deviation from OpenRocket, whose copy keeps the document reference and registers
    // "SetOnCopy" with the original's document: a copy here tells nobody.
    const std::unique_ptr<Rocket> copy = m_rocket.copyRocketWithOriginalId();
    ASSERT_NE(copy, nullptr);
    EXPECT_TRUE(copy->documentMaterialSet().empty());
    EXPECT_FALSE(m_rocket.documentMaterialSet().empty());

    auto* copyTube = dynamic_cast<BodyTube*>(copy->findComponent(m_tube->getId()));
    ASSERT_NE(copyTube, nullptr);
    copyTube->setMaterial(doc(Material::Type::BULK, "SetOnCopy", 9.0));
    EXPECT_EQ(copyTube->getMaterial().getName(), "SetOnCopy");
    EXPECT_EQ(taken(), "");

    // The original still does.
    m_tube->setMaterial(doc(Material::Type::BULK, "SetOnOriginal", 9.5));
    EXPECT_EQ(taken(), "BULK SetOnOriginal");
}

TEST_F(DocumentMaterialTest, ACopyWithNewIdsHasNoListenersEither)
{
    std::unique_ptr<RocketComponent> copied = m_rocket.copyWithNewIds();
    const std::unique_ptr<Rocket>    copy   = QtRocket::componentCast<Rocket>(copied);
    ASSERT_NE(copy, nullptr);
    EXPECT_TRUE(copy->documentMaterialSet().empty());
}

// ========================================================================= J: Rocket::loadFrom()

TEST_F(DocumentMaterialTest, LoadFromKeepsEachRocketsListeners)
{
    Rocket      other;
    auto&       otherStage = other.addChild(std::make_unique<AxialStage>());
    const auto& otherTube  = otherStage.addChild(std::make_unique<BodyTube>());
    Recorder    otherRecorder{other};

    // Undo and redo load a snapshot into the document's rocket: the rocket stays the
    // document's, and its new components announce to the same listeners.
    m_rocket.loadFrom(other);
    auto* loadedTube = dynamic_cast<BodyTube*>(m_rocket.findComponent(otherTube.getId()));
    ASSERT_NE(loadedTube, nullptr);
    ASSERT_NE(loadedTube, &otherTube);
    // Loading announces nothing by itself.
    EXPECT_EQ(taken(), "");
    loadedTube->setMaterial(doc(Material::Type::BULK, "AfterLoadFrom", 20.0));
    EXPECT_EQ(taken(), "BULK AfterLoadFrom");
    EXPECT_EQ(otherRecorder.taken(), "");

    // The source keeps its own.
    otherStage.addChild(std::make_unique<BodyTube>())
        .setMaterial(doc(Material::Type::BULK, "OnTheSource", 21.0));
    EXPECT_EQ(otherRecorder.taken(), "BULK OnTheSource");
    EXPECT_EQ(taken(), "");
}

}  // namespace
