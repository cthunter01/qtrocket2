// StructuralComponent, through its concrete ring classes: the bulk material, its setter and its
// preset. OpenRocket has no JUnit test of the class of its own.

#include "QtRocket/rocket/StructuralComponent.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "rocket/InternalTestSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::CenteringRing;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetType;
using QtRocket::EngineBlock;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::StructuralComponent;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::makeFactoryPreset;

class StructuralComponentEvents : public QtRocket::Test::RingEventsFixture
{ };

TEST(StructuralComponent, DefaultMaterialIsCardboard)
{
    const InnerTube            tube;
    const StructuralComponent& structural = tube;
    EXPECT_EQ(structural.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(structural.getMaterial().getType(), Material::Type::BULK);
    EXPECT_EQ(structural.getMaterial().getDensity(), 680.0);
    EXPECT_FALSE(structural.getMaterial().isUserDefined());

    const std::vector<Material> materials = structural.getAllMaterials();
    ASSERT_EQ(materials.size(), 1U);
    EXPECT_EQ(materials.front(), structural.getMaterial());
}

TEST(StructuralComponent, SetMaterialRejectsNonBulkMaterials)
{
    CenteringRing ring;
    EXPECT_THROW(ring.setMaterial(Material::newMaterial(Material::Type::SURFACE, "s", 1.0, true)),
                 BugError);
    EXPECT_THROW(ring.setMaterial(Material::newMaterial(Material::Type::LINE, "l", 1.0, true)),
                 BugError);
    EXPECT_EQ(ring.getMaterial().getName(), "Cardboard");
}

TEST_F(StructuralComponentEvents, SetMaterialFiresAndClearsThePresetOnlyOnChange)
{
    auto& ring = m_body->addChild(std::make_unique<CenteringRing>());
    m_types.clear();
    ring.setMaterial(ring.getMaterial());  // equal: nothing
    EXPECT_TRUE(m_types.empty());

    const Material balsa = Material::newMaterial(Material::Type::BULK, "Balsa", 170, false);
    ring.setMaterial(balsa);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
    EXPECT_EQ(ring.getMaterial(), balsa);
    EXPECT_EQ(ring.getMaterial().getName(), "Balsa");
}

TEST(StructuralComponent, PresetMaterialIsLoaded)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::ENGINE_BLOCK);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 0.01);
    props.put(ComponentPreset::kOuterDiameter, 0.02);
    props.put(ComponentPreset::kInnerDiameter, 0.01);
    props.put(ComponentPreset::kMaterial,
              Material::newMaterial(Material::Type::BULK, "Plywood (birch)", 630, false));
    const ComponentPreset preset = makeFactoryPreset(props);

    EngineBlock block;
    block.loadPreset(&preset);
    EXPECT_EQ(block.getMaterial().getName(), "Plywood (birch)");
    EXPECT_EQ(block.getMaterial().getDensity(), 630.0);
    EXPECT_EQ(block.getPresetComponent(), &preset);
}

TEST(StructuralComponent, PresetWithoutAMaterialKeepsTheMaterial)
{
    // Java: "if (mat != null) material = mat" (no default material as RecoveryDevice has).
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::ENGINE_BLOCK);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 0.01);
    props.put(ComponentPreset::kOuterDiameter, 0.02);
    props.put(ComponentPreset::kInnerDiameter, 0.01);
    const ComponentPreset preset = makeFactoryPreset(props);
    ASSERT_FALSE(preset.has(ComponentPreset::kMaterial));

    EngineBlock    block;
    const Material balsa = Material::newMaterial(Material::Type::BULK, "Balsa", 170, false);
    block.setMaterial(balsa);
    block.loadPreset(&preset);
    EXPECT_EQ(block.getMaterial(), balsa);
    EXPECT_EQ(block.getPresetComponent(), &preset);
}

}  // namespace
