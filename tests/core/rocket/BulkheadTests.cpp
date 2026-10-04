// BulkHeadComponentTests.java (core/src/test/.../preset), ported, and the bulkhead's own
// behaviour.

#include "QtRocket/rocket/Bulkhead.h"

#include <memory>
#include <numbers>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
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
using QtRocket::Bulkhead;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::Rocket;
using QtRocket::TypedPropertyMap;

/// BulkHeadComponentTests.createPreset(): a bulkhead preset 2 m long, 2 m in diameter, of mass
/// 100 kg.
[[nodiscard]] ComponentPreset bulkheadPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::BULK_HEAD);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class BulkHeadComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{bulkheadPreset()};
};

// Java: testComponentType
TEST_F(BulkHeadComponentTest, ComponentType)
{
    const Bulkhead bt;
    EXPECT_EQ(bt.getPresetType(), ComponentPresetType::BULK_HEAD);
}

// Java: testLoadFromPresetIsSane
TEST_F(BulkHeadComponentTest, LoadFromPresetIsSane)
{
    Bulkhead bt;
    bt.loadPreset(&m_preset);

    EXPECT_EQ(bt.getLength(), 2.0);
    EXPECT_EQ(bt.getOuterRadius(), 1.0);

    EXPECT_FALSE(bt.isOuterRadiusAutomatic());

    EXPECT_EQ(bt.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(bt.getMass(), 100.0, 0.05);
}

// Java: changeODClearsPreset
TEST_F(BulkHeadComponentTest, ChangeODClearsPreset)
{
    Bulkhead bt;
    bt.loadPreset(&m_preset);
    bt.setOuterRadius(2.0);
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeODAutomaticClearsPreset
TEST_F(BulkHeadComponentTest, ChangeODAutomaticClearsPreset)
{
    Bulkhead bt;
    bt.loadPreset(&m_preset);
    bt.setOuterRadiusAutomatic(true);
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(BulkHeadComponentTest, ChangeMaterialClearsPreset)
{
    Bulkhead bt;
    bt.loadPreset(&m_preset);
    bt.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

TEST_F(BulkHeadComponentTest, SetOuterRadiusAutomaticClearsThePresetEvenUnchanged)
{
    Bulkhead bt;
    bt.loadPreset(&m_preset);
    bt.setOuterRadiusAutomatic(false);  // already manual
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

TEST(Bulkhead, Defaults)
{
    const Bulkhead bt;
    EXPECT_EQ(bt.kind(), ComponentKind::BULKHEAD);
    EXPECT_EQ(bt.getName(), "Bulkhead");
    EXPECT_TRUE(bt.isOuterRadiusAutomatic());
    EXPECT_FALSE(bt.isInnerRadiusAutomatic());
    EXPECT_EQ(bt.getLength(), 0.002);
    EXPECT_EQ(bt.getDisplayOrderSide(), 8);
    EXPECT_EQ(bt.getDisplayOrderBack(), 6);
    EXPECT_EQ(bt.getPatternName(), "1-Line");
}

TEST(Bulkhead, IsSolid)
{
    Bulkhead bt;
    bt.setOuterRadius(0.02);
    bt.setInnerRadius(0.01);  // ignored
    EXPECT_EQ(bt.getInnerRadius(), 0.0);
    EXPECT_EQ(bt.getThickness(), 0.02);
    bt.setThickness(0.005);  // through setInnerRadius(): ignored too
    EXPECT_EQ(bt.getThickness(), 0.02);
    bt.setLength(0.01);
    const double mass = std::numbers::pi * 0.02 * 0.02 * 0.01 * bt.getMaterial().getDensity();
    EXPECT_NEAR(bt.getComponentMass(), mass, mass * 1e-12);
    EXPECT_NEAR(bt.getRotationalUnitInertia(), (0.02 * 0.02) / 2, 1e-15);
}

TEST(Bulkhead, HoldsNoChildren)
{
    Bulkhead bt;
    EXPECT_FALSE(bt.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(bt.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(bt.addChild(std::make_unique<Bulkhead>()), BugError);
}

TEST(Bulkhead, FillsTheBodyTube)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.001));
    ASSERT_EQ(body.getInnerRadius(), 0.024);
    const auto& bt = body.addChild(std::make_unique<Bulkhead>());
    rocket.enableEvents();
    EXPECT_EQ(bt.getOuterRadius(), 0.024);
    EXPECT_EQ(bt.getThickness(), 0.024);
}

}  // namespace
