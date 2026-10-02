// TubeCouplerComponentTests.java (core/src/test/.../preset), ported, and the coupler's own
// behaviour.

#include "QtRocket/rocket/TubeCoupler.h"

#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "rocket/TestBodyComponent.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::RadialParent;
using QtRocket::Rocket;
using QtRocket::TubeCoupler;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::TestBodyComponent;

/// TubeCouplerComponentTests.createPreset(): a coupler preset 2 m long, 2 m outer and 1 m inner
/// diameter, of mass 100 kg.
[[nodiscard]] ComponentPreset couplerPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::TUBE_COUPLER);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class TubeCouplerComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{couplerPreset()};
};

// Java: testComponentType
TEST_F(TubeCouplerComponentTest, ComponentType)
{
    const TubeCoupler tc;
    EXPECT_EQ(tc.getPresetType(), ComponentPresetType::TUBE_COUPLER);
}

// Java: testLoadFromPresetIsSane
TEST_F(TubeCouplerComponentTest, LoadFromPresetIsSane)
{
    TubeCoupler tc;
    tc.loadPreset(&m_preset);

    EXPECT_EQ(tc.getLength(), 2.0);
    EXPECT_EQ(tc.getOuterRadius(), 1.0);
    EXPECT_EQ(tc.getInnerRadius(), 0.5);

    EXPECT_FALSE(tc.isInnerRadiusAutomatic());
    EXPECT_FALSE(tc.isOuterRadiusAutomatic());

    EXPECT_EQ(tc.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(tc.getMass(), 100.0, 0.05);
}

// Java: changeODClearsPreset
TEST_F(TubeCouplerComponentTest, ChangeODClearsPreset)
{
    TubeCoupler tc;
    tc.loadPreset(&m_preset);
    tc.setOuterRadius(2.0);
    EXPECT_EQ(tc.getPresetComponent(), nullptr);
}

// Java: changeIDClearsPreset
TEST_F(TubeCouplerComponentTest, ChangeIDClearsPreset)
{
    TubeCoupler tc;
    tc.loadPreset(&m_preset);
    tc.setInnerRadius(0.75);
    EXPECT_EQ(tc.getPresetComponent(), nullptr);
}

// Java: changeThicknessClearsPreset
TEST_F(TubeCouplerComponentTest, ChangeThicknessClearsPreset)
{
    TubeCoupler tc;
    tc.loadPreset(&m_preset);
    tc.setThickness(0.1);
    EXPECT_EQ(tc.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(TubeCouplerComponentTest, ChangeMaterialClearsPreset)
{
    TubeCoupler tc;
    tc.loadPreset(&m_preset);
    tc.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(tc.getPresetComponent(), nullptr);
}

// Java: changeLengthLeavesPreset (commented out in Java)
TEST_F(TubeCouplerComponentTest, ChangeLengthClearsPreset)
{
    // Java's changeLengthLeavesPreset is commented out ("test fails"): RingComponent's
    // setLength() clears the preset.
    TubeCoupler tc;
    tc.loadPreset(&m_preset);
    tc.setLength(1.0);
    EXPECT_EQ(tc.getPresetComponent(), nullptr);
}

TEST(TubeCoupler, Defaults)
{
    const TubeCoupler tc;
    EXPECT_EQ(tc.kind(), ComponentKind::TUBE_COUPLER);
    EXPECT_EQ(tc.getName(), "Tube Coupler");
    EXPECT_TRUE(tc.isOuterRadiusAutomatic());
    EXPECT_FALSE(tc.isInnerRadiusAutomatic());
    EXPECT_EQ(tc.getLength(), 0.06);
    EXPECT_EQ(tc.getOuterRadius(), 0.0);
    EXPECT_EQ(tc.getThickness(), 0.0);  // set while the outer radius was 0
    EXPECT_EQ(tc.getDisplayOrderSide(), 6);
    EXPECT_EQ(tc.getDisplayOrderBack(), 13);
    EXPECT_EQ(tc.getComponentMass(), 0.0);
}

TEST(TubeCoupler, AcceptsInternalComponents)
{
    TubeCoupler tc;
    EXPECT_TRUE(tc.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_EQ(tc.isCompatible(kind), QtRocket::isInternal(kind))
            << QtRocket::componentKindName(kind);
    }
    tc.addChild(std::make_unique<QtRocket::MassComponent>());
    EXPECT_EQ(tc.getChildCount(), 1U);
}

TEST(TubeCoupler, TakesTheBodyTubesInnerRadius)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.3, 0.025));
    body.setInnerRadius(0.0245);
    auto& tc = body.addChild(std::make_unique<TubeCoupler>());
    rocket.enableEvents();

    EXPECT_EQ(tc.getOuterRadius(), 0.0245);
    tc.setThickness(0.001);
    EXPECT_EQ(tc.getThickness(), 0.001);
    EXPECT_NEAR(tc.getInnerRadius(), 0.0235, 1e-12);

    // A coupler is a RadialParent of constant radius for its own children.
    const RadialParent& radial = tc;
    EXPECT_EQ(radial.getOuterRadius(0.01), tc.getOuterRadius());
    EXPECT_EQ(radial.getInnerRadius(0.01), tc.getInnerRadius());
    EXPECT_EQ(radial.getLength(), 0.06);

    // A manual radius stops following the body.
    tc.setOuterRadius(0.02);
    EXPECT_FALSE(tc.isOuterRadiusAutomatic());
    body.setInnerRadius(0.0240);
    EXPECT_EQ(tc.getOuterRadius(), 0.02);
    tc.setOuterRadiusAutomatic(true);
    EXPECT_EQ(tc.getOuterRadius(), 0.024);
}

}  // namespace
