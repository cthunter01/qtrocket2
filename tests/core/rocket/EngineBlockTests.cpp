// EngineBlockComponentTests.java (core/src/test/.../preset), ported, and the engine block's own
// behaviour.

#include "QtRocket/rocket/EngineBlock.h"

#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::EngineBlock;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::Rocket;
using QtRocket::TypedPropertyMap;

/// EngineBlockComponentTests.createPreset(): an engine block preset 2 m long, 2 m outer and 1 m
/// inner diameter, of mass 100 kg.
[[nodiscard]] ComponentPreset engineBlockPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::ENGINE_BLOCK);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class EngineBlockComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{engineBlockPreset()};
};

// Java: testComponentType
TEST_F(EngineBlockComponentTest, ComponentType)
{
    const EngineBlock eb;
    EXPECT_EQ(eb.getPresetType(), ComponentPresetType::ENGINE_BLOCK);
}

// Java: testLoadFromPresetIsSane
TEST_F(EngineBlockComponentTest, LoadFromPresetIsSane)
{
    EngineBlock eb;
    eb.loadPreset(&m_preset);

    EXPECT_EQ(eb.getLength(), 2.0);
    EXPECT_EQ(eb.getOuterRadius(), 1.0);
    EXPECT_EQ(eb.getInnerRadius(), 0.5);

    EXPECT_FALSE(eb.isOuterRadiusAutomatic());

    EXPECT_EQ(eb.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(eb.getMass(), 100.0, 0.05);
}

// Java: changeODClearsPreset
TEST_F(EngineBlockComponentTest, ChangeODClearsPreset)
{
    EngineBlock eb;
    eb.loadPreset(&m_preset);
    eb.setOuterRadius(2.0);
    EXPECT_EQ(eb.getPresetComponent(), nullptr);
}

// Java: changeIDClearsPreset
TEST_F(EngineBlockComponentTest, ChangeIDClearsPreset)
{
    EngineBlock eb;
    eb.loadPreset(&m_preset);
    eb.setInnerRadius(0.75);
    EXPECT_EQ(eb.getPresetComponent(), nullptr);
}

// Java: changeThicknessClearsPreset
TEST_F(EngineBlockComponentTest, ChangeThicknessClearsPreset)
{
    EngineBlock eb;
    eb.loadPreset(&m_preset);
    eb.setThickness(0.1);
    EXPECT_EQ(eb.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(EngineBlockComponentTest, ChangeMaterialClearsPreset)
{
    EngineBlock eb;
    eb.loadPreset(&m_preset);
    eb.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(eb.getPresetComponent(), nullptr);
}

TEST_F(EngineBlockComponentTest, SettingAnAutomaticRadiusKeepsThePreset)
{
    // setOuterRadiusAutomatic() does not clear the preset (only Bulkhead's does).
    EngineBlock eb;
    eb.loadPreset(&m_preset);
    eb.setOuterRadiusAutomatic(true);
    EXPECT_TRUE(eb.isOuterRadiusAutomatic());
    EXPECT_EQ(eb.getPresetComponent(), &m_preset);
}

TEST(EngineBlock, Defaults)
{
    const EngineBlock eb;
    EXPECT_EQ(eb.kind(), ComponentKind::ENGINE_BLOCK);
    EXPECT_EQ(eb.getName(), "Engine Block");
    EXPECT_TRUE(eb.isOuterRadiusAutomatic());
    EXPECT_EQ(eb.getLength(), 0.005);
    EXPECT_EQ(eb.getThickness(), 0.0);  // set while the outer radius was 0
    EXPECT_EQ(eb.getDisplayOrderSide(), 9);
    EXPECT_EQ(eb.getDisplayOrderBack(), 15);
    EXPECT_EQ(eb.getAxialMethod(), AxialMethod::BOTTOM);
}

TEST(EngineBlock, HoldsNoChildren)
{
    EngineBlock eb;
    EXPECT_FALSE(eb.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(eb.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(eb.addChild(std::make_unique<EngineBlock>()), BugError);
}

TEST(EngineBlock, FitsTheMotorMount)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    auto&  inner = body.addChild(std::make_unique<InnerTube>());
    auto&  eb    = inner.addChild(std::make_unique<EngineBlock>());
    rocket.enableEvents();

    // The automatic outer radius is the inner tube's inner radius.
    EXPECT_EQ(eb.getOuterRadius(), inner.getInnerRadius());
    eb.setThickness(0.002);
    EXPECT_EQ(eb.getThickness(), 0.002);
    EXPECT_NEAR(eb.getInnerRadius(), inner.getInnerRadius() - 0.002, 1e-12);
    // At the aft end of the mount (BOTTOM, offset 0).
    EXPECT_NEAR(eb.getPosition().x, 0.07 - 0.005, 1e-12);
}

}  // namespace
