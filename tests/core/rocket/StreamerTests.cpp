// StreamerComponentTests.java (core/src/test/.../preset), ported, and the streamer's own
// behaviour (the drag coefficient estimate, the aspect ratio and the area).

#include "QtRocket/rocket/Streamer.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::Streamer;
using QtRocket::TypedPropertyMap;

/// OpenRocket's estimate, written out: 0.034 * ((density + 0.025) / 0.105) * (L + 1) / L, at
/// most 0.4.
[[nodiscard]] double expectedCd(double density, double stripLength)
{
    const double cd = 0.034 * ((density + 0.025) / 0.105) * (stripLength + 1) / stripLength;
    return cd < 0.4 ? cd : 0.4;
}

/// StreamerComponentTests.createPreset(): a strip 20 m by 2 m of "testMaterial" (2 kg/m^2).
[[nodiscard]] ComponentPreset streamerPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::STREAMER);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 20.0);
    presetspec.put(ComponentPreset::kWidth, 2.0);
    presetspec.put(ComponentPreset::kMaterial,
                   Material::newMaterial(Material::Type::SURFACE, "testMaterial", 2.0, true));
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class StreamerComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{streamerPreset()};
};

// Java: testComponentType
TEST_F(StreamerComponentTest, ComponentType)
{
    const Streamer cr;
    EXPECT_EQ(cr.getPresetType(), ComponentPresetType::STREAMER);
}

// Java: testLoadFromPresetIsSane
TEST_F(StreamerComponentTest, LoadFromPresetIsSane)
{
    Streamer cr;
    cr.loadPreset(&m_preset);

    EXPECT_EQ(cr.getStripLength(), 20.0);
    EXPECT_EQ(cr.getStripWidth(), 2.0);
    EXPECT_EQ(cr.getLength(), 2.0);

    EXPECT_EQ(cr.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(cr.getMass(), 80.0, 0.05);
}

// Java: changeWidthClearsPreset
TEST_F(StreamerComponentTest, ChangeWidthClearsPreset)
{
    Streamer cr;
    cr.loadPreset(&m_preset);
    cr.setStripWidth(1.0);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(StreamerComponentTest, ChangeMaterialClearsPreset)
{
    Streamer cr;
    cr.loadPreset(&m_preset);
    cr.setMaterial(Material::newMaterial(Material::Type::SURFACE, "new", 1.0, true));
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

TEST_F(StreamerComponentTest, ChangeLengthKeepsThePreset)
{
    // Java's changeLengthClearsPreset is commented out ("test fails"): setStripLength() does not
    // clear the preset (its clearPreset() is commented out too).
    Streamer cr;
    cr.loadPreset(&m_preset);
    cr.setStripLength(1.0);
    EXPECT_EQ(cr.getPresetComponent(), &m_preset);
}

TEST_F(StreamerComponentTest, PresetMakesTheCdAutomaticWithTheEstimate)
{
    Streamer cr;
    cr.setCD(0.9);
    cr.loadPreset(&m_preset);
    EXPECT_TRUE(cr.isCDAutomatic());
    EXPECT_NEAR(cr.getCD(), expectedCd(2.0, 20.0), 1e-15);
    EXPECT_EQ(cr.getCD(), 0.4);  // capped
}

TEST(Streamer, Defaults)
{
    const Streamer streamer;
    EXPECT_EQ(streamer.kind(), ComponentKind::STREAMER);
    EXPECT_EQ(streamer.getName(), "Streamer");
    EXPECT_EQ(streamer.getStripLength(), 0.5);
    EXPECT_EQ(streamer.getStripWidth(), 0.05);
    EXPECT_EQ(streamer.getAspectRatio(), 10.0);
    EXPECT_NEAR(streamer.getArea(), 0.025, 1e-15);
    EXPECT_EQ(streamer.getDisplayOrderSide(), 10);
    EXPECT_EQ(streamer.getDisplayOrderBack(), 8);
    EXPECT_EQ(Streamer::kDefaultCd, 0.6);
    EXPECT_EQ(Streamer::kMaxComputedCd, 0.4);
    EXPECT_FALSE(streamer.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(streamer.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
}

TEST(Streamer, CdEstimate)
{
    Streamer streamer;
    EXPECT_NEAR(streamer.getComponentCD(0.0), expectedCd(0.067, 0.5), 1e-15);
    EXPECT_EQ(streamer.getComponentCD(0.0), streamer.getComponentCD(2.0));  // no Mach effect
    streamer.setStripLength(2.0);
    EXPECT_NEAR(streamer.getComponentCD(0.0), expectedCd(0.067, 2.0), 1e-15);
    streamer.setMaterial(Material::newMaterial(Material::Type::SURFACE, "heavy", 1.0, true));
    EXPECT_EQ(streamer.getComponentCD(0.0), 0.4);
}

TEST(Streamer, AspectRatioKeepsTheArea)
{
    Streamer streamer;
    streamer.setAspectRatio(40);
    EXPECT_NEAR(streamer.getArea(), 0.025, 1e-15);
    EXPECT_NEAR(streamer.getStripWidth(), 0.025, 1e-15);
    EXPECT_NEAR(streamer.getStripLength(), 1.0, 1e-15);
    EXPECT_NEAR(streamer.getAspectRatio(), 40, 1e-12);

    streamer.setAspectRatio(0.001);  // at least 0.01
    EXPECT_NEAR(streamer.getAspectRatio(), 0.01, 1e-12);
    EXPECT_NEAR(streamer.getArea(), 0.025, 1e-15);
}

TEST(Streamer, AreaKeepsTheAspectRatio)
{
    Streamer streamer;
    streamer.setArea(0.1);
    EXPECT_NEAR(streamer.getArea(), 0.1, 1e-15);
    EXPECT_NEAR(streamer.getAspectRatio(), 10, 1e-12);
    EXPECT_NEAR(streamer.getStripWidth(), 0.1, 1e-15);
    EXPECT_NEAR(streamer.getStripLength(), 1.0, 1e-15);
}

TEST(Streamer, NarrowStripHasAFixedAspectRatio)
{
    Streamer streamer;
    streamer.setStripWidth(0.0001);
    EXPECT_EQ(streamer.getAspectRatio(), 1000.0);
    streamer.setStripWidth(0.0);
    EXPECT_EQ(streamer.getAspectRatio(), 1000.0);
    // setArea() then uses that ratio.
    streamer.setArea(0.1);
    EXPECT_NEAR(streamer.getStripLength() / streamer.getStripWidth(), 1000, 1e-9);
}

TEST(Streamer, SettersFire)
{
    Rocket rocket;
    auto&  stage    = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body     = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    auto&  streamer = body.addChild(std::make_unique<Streamer>());
    rocket.enableEvents();
    std::vector<int>                              types;
    const ComponentChangeSignal::ScopedConnection connection{rocket.addComponentChangeListener(
        [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};

    streamer.setStripLength(0.5);
    streamer.setStripWidth(0.05);
    streamer.setAspectRatio(10);
    streamer.setArea(0.025);
    EXPECT_TRUE(types.empty());
    streamer.setStripLength(0.6);
    streamer.setStripWidth(0.06);
    streamer.setAspectRatio(5);
    streamer.setArea(0.05);
    EXPECT_EQ(types, std::vector<int>(4, ComponentChangeEvent::kAeromassChange));
}

TEST(Streamer, NewStreamerStartsFromTheParachuteCd)
{
    // As in Java, the stored CD of a new streamer is Parachute.DEFAULT_CD until computed.
    Streamer streamer;
    streamer.setCDAutomatic(false);
    EXPECT_EQ(streamer.getCD(), Parachute::kDefaultCd);
}

TEST(Streamer, PresetWithoutAMaterialGetsTheDefault)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::STREAMER);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 1.0);
    props.put(ComponentPreset::kWidth, 0.1);
    const QtRocket::MaterialStorage materials;
    const ComponentPreset preset = ComponentPresetFactory::create(props, materials).value();

    Streamer streamer;
    streamer.setMaterial(Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true));
    streamer.loadPreset(&preset);
    EXPECT_EQ(streamer.getMaterial().getName(), "Ripstop nylon");
    EXPECT_EQ(streamer.getLength(), 0.1);
    EXPECT_NEAR(streamer.getCD(), expectedCd(0.067, 1.0), 1e-15);
    EXPECT_THROW(streamer.setMaterial(Material::newMaterial(Material::Type::LINE, "l", 1, true)),
                 BugError);
}

}  // namespace
