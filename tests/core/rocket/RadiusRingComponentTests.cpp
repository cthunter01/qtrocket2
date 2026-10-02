// RadiusRingComponent, through CenteringRing and Bulkhead: the two radii that push each other,
// the line instances and the preset's radii. OpenRocket has no JUnit test of the class of its
// own.

#include "QtRocket/rocket/RadiusRingComponent.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/InternalTestSupport.h"

namespace
{

using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::RadiusRingComponent;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::makeFactoryPreset;

constexpr double kEpsilon = 1e-12;

class RadiusRingComponentEvents : public QtRocket::Test::RingEventsFixture
{ };

TEST(RadiusRingComponent, RadiiPushEachOther)
{
    CenteringRing        centeringRing;
    RadiusRingComponent& ring = centeringRing;
    ring.setOuterRadius(0.02);
    ring.setInnerRadius(0.01);
    EXPECT_FALSE(ring.isOuterRadiusAutomatic());
    EXPECT_FALSE(ring.isInnerRadiusAutomatic());
    EXPECT_EQ(ring.getThickness(), 0.01);

    ring.setOuterRadius(0.005);  // below the inner radius: it follows
    EXPECT_EQ(ring.getInnerRadius(), 0.005);
    EXPECT_EQ(ring.getThickness(), 0.0);

    ring.setInnerRadius(0.03);  // beyond the outer radius: it follows
    EXPECT_EQ(ring.getOuterRadius(), 0.03);
    ring.setInnerRadius(-1.0);
    EXPECT_EQ(ring.getInnerRadius(), 0.0);

    ring.setThickness(0.01);
    EXPECT_NEAR(ring.getInnerRadius(), 0.02, kEpsilon);
    ring.setThickness(1.0);  // clamped to the outer radius
    EXPECT_EQ(ring.getInnerRadius(), 0.0);
}

TEST(RadiusRingComponent, LineInstances)
{
    CenteringRing ring;
    EXPECT_EQ(ring.getInstanceCount(), 1);
    EXPECT_EQ(ring.getPatternName(), "1-Line");
    ring.setInstanceCount(0);   // ignored
    ring.setInstanceCount(-2);  // ignored
    EXPECT_EQ(ring.getInstanceCount(), 1);
    ring.setInstanceCount(3);
    ring.setInstanceSeparation(0.04);
    EXPECT_EQ(ring.getPatternName(), "3-Line");
    const std::vector<Coordinate> offsets = ring.getInstanceOffsets();
    ASSERT_EQ(offsets.size(), 3U);
    EXPECT_TRUE(offsets[0].exactlyEquals(Coordinate{0, 0, 0}));
    EXPECT_TRUE(offsets[1].exactlyEquals(Coordinate{0.04, 0, 0}));
    EXPECT_TRUE(offsets[2].exactlyEquals(Coordinate{0.08, 0, 0}));
    EXPECT_EQ(ring.getInstanceLocations().size(), 3U);
}

TEST_F(RadiusRingComponentEvents, LineInstanceSettersFireAeromassOnChange)
{
    auto& ring = m_body->addChild(std::make_unique<Bulkhead>());
    m_types.clear();
    ring.setInstanceCount(1);         // unchanged
    ring.setInstanceSeparation(0.0);  // unchanged
    EXPECT_TRUE(m_types.empty());
    ring.setInstanceCount(2);
    ring.setInstanceSeparation(0.01);
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kAeromassChange));
}

TEST(RadiusRingComponent, PresetMakesBothRadiiManual)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 0.003);
    props.put(ComponentPreset::kOuterDiameter, 0.05);
    const ComponentPreset preset = makeFactoryPreset(props);

    CenteringRing ring;
    ASSERT_TRUE(ring.isInnerRadiusAutomatic());
    ring.loadPreset(&preset);
    EXPECT_FALSE(ring.isOuterRadiusAutomatic());
    EXPECT_FALSE(ring.isInnerRadiusAutomatic());  // even without an inner diameter
    EXPECT_EQ(ring.getOuterRadius(), 0.025);
    EXPECT_EQ(ring.getLength(), 0.003);
}

}  // namespace
