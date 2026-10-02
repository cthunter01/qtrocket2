// ThicknessRingComponent, through EngineBlock and TubeCoupler: the outer radius and the wall
// thickness that keep each other in range, and the preset's dimensions. OpenRocket has no JUnit
// test of the class of its own.

#include "QtRocket/rocket/ThicknessRingComponent.h"

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "rocket/InternalTestSupport.h"

namespace
{

using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetType;
using QtRocket::EngineBlock;
using QtRocket::Manufacturer;
using QtRocket::ThicknessRingComponent;
using QtRocket::TubeCoupler;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::makeFactoryPreset;

constexpr double kEpsilon = 1e-12;

TEST(ThicknessRingComponent, OuterRadiusCutsTheThickness)
{
    EngineBlock             block;
    ThicknessRingComponent& ring = block;
    ring.setOuterRadius(0.02);
    ring.setThickness(0.015);
    EXPECT_EQ(ring.getThickness(), 0.015);
    ring.setOuterRadius(0.01);  // the thickness is cut to the radius
    EXPECT_EQ(ring.getThickness(), 0.01);
    EXPECT_EQ(ring.getInnerRadius(), 0.0);
    ring.setOuterRadius(-1.0);
    EXPECT_EQ(ring.getOuterRadius(), 0.0);
    EXPECT_FALSE(ring.isOuterRadiusAutomatic());
}

TEST(ThicknessRingComponent, ThicknessIsClampedToTheOuterRadius)
{
    EngineBlock block;
    block.setOuterRadius(0.02);
    block.setThickness(0.05);
    EXPECT_EQ(block.getThickness(), 0.02);
    EXPECT_EQ(block.getInnerRadius(), 0.0);
    block.setThickness(-0.05);
    EXPECT_EQ(block.getThickness(), 0.0);
    EXPECT_EQ(block.getInnerRadius(), 0.02);

    block.setInnerRadius(0.015);
    EXPECT_NEAR(block.getThickness(), 0.005, kEpsilon);
    block.setInnerRadius(-1.0);  // becomes 0: the full thickness
    EXPECT_EQ(block.getThickness(), 0.02);
}

TEST(ThicknessRingComponent, ANewBlockOrCouplerHasNoThickness)
{
    // The constructors set the thickness while the automatic outer radius is still 0.
    const EngineBlock block;
    EXPECT_EQ(block.getThickness(), 0.0);
    const TubeCoupler coupler;
    EXPECT_EQ(coupler.getThickness(), 0.0);
}

TEST(ThicknessRingComponent, SetOuterRadiusAgainOnlyClearsAnAutomaticRadius)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::ENGINE_BLOCK);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 0.01);
    props.put(ComponentPreset::kOuterDiameter, 0.02);
    props.put(ComponentPreset::kInnerDiameter, 0.01);
    const ComponentPreset preset = makeFactoryPreset(props);

    EngineBlock block;
    block.loadPreset(&preset);
    EXPECT_FALSE(block.isOuterRadiusAutomatic());
    EXPECT_FALSE(block.isInnerRadiusAutomatic());
    block.setOuterRadius(0.01);  // equal and manual: nothing
    block.setThickness(0.005);   // equal: nothing
    EXPECT_EQ(block.getPresetComponent(), &preset);
    block.setInnerRadius(0.004);
    EXPECT_EQ(block.getPresetComponent(), nullptr);
}

}  // namespace
