// CenteringRingComponentTests.java (core/src/test/.../preset), ported, the centering ring part
// of RocketTest.testEstesAlphaIII, and the ring's automatic radii.

#include "QtRocket/rocket/CenteringRing.h"

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::CenteringRing;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::TestComponent;

constexpr double kEpsilon = 1e-12;

/// CenteringRingComponentTests.createPreset(): a ring preset 2 m long, 2 m outer and 1 m inner
/// diameter, of mass 100 kg.
[[nodiscard]] ComponentPreset centeringRingPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::CENTERING_RING);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class CenteringRingComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{centeringRingPreset()};
};

// Java: testComponentType
TEST_F(CenteringRingComponentTest, ComponentType)
{
    const CenteringRing cr;
    EXPECT_EQ(cr.getPresetType(), ComponentPresetType::CENTERING_RING);
}

// Java: testLoadFromPresetIsSane
TEST_F(CenteringRingComponentTest, LoadFromPresetIsSane)
{
    CenteringRing cr;
    cr.loadPreset(&m_preset);

    EXPECT_EQ(cr.getLength(), 2.0);
    EXPECT_EQ(cr.getOuterRadius(), 1.0);
    EXPECT_EQ(cr.getInnerRadius(), 0.5);

    EXPECT_FALSE(cr.isOuterRadiusAutomatic());

    EXPECT_EQ(cr.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(cr.getMass(), 100.0, 0.05);
}

// Java: changeODClearsPreset
TEST_F(CenteringRingComponentTest, ChangeODClearsPreset)
{
    CenteringRing cr;
    cr.loadPreset(&m_preset);
    cr.setOuterRadius(2.0);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeIDClearsPreset
TEST_F(CenteringRingComponentTest, ChangeIDClearsPreset)
{
    CenteringRing cr;
    cr.loadPreset(&m_preset);
    cr.setInnerRadius(0.75);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeThicknessClearsPreset
TEST_F(CenteringRingComponentTest, ChangeThicknessClearsPreset)
{
    CenteringRing cr;
    cr.loadPreset(&m_preset);
    cr.setThickness(0.1);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(CenteringRingComponentTest, ChangeMaterialClearsPreset)
{
    CenteringRing cr;
    cr.loadPreset(&m_preset);
    cr.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

TEST(CenteringRing, Defaults)
{
    const CenteringRing cr;
    EXPECT_EQ(cr.kind(), ComponentKind::CENTERING_RING);
    EXPECT_EQ(cr.getName(), "Centering Ring");
    EXPECT_TRUE(cr.isOuterRadiusAutomatic());
    EXPECT_TRUE(cr.isInnerRadiusAutomatic());
    EXPECT_EQ(cr.getLength(), 0.002);
    EXPECT_EQ(cr.getOuterRadius(), 0.0);
    EXPECT_EQ(cr.getInnerRadius(), 0.0);  // no parent
    EXPECT_EQ(cr.getDisplayOrderSide(), 7);
    EXPECT_EQ(cr.getDisplayOrderBack(), 5);
    EXPECT_EQ(cr.getInstanceCount(), 1);
    EXPECT_EQ(cr.getInstanceSeparation(), 0.0);
}

TEST(CenteringRing, HoldsNoChildren)
{
    CenteringRing cr;
    EXPECT_FALSE(cr.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(cr.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(cr.addChild(std::make_unique<CenteringRing>()), BugError);
}

/// A rocket with a stage holding a body tube (0.5 m long, radius 0.03 m, inner radius 0.029 m)
/// with an inner tube of outer radius 0.012 m from 0.2 to 0.4 m; events enabled.
class CenteringRingTest : public ::testing::Test
{
protected:
    CenteringRingTest()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_body      = &stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
        m_body->setInnerRadius(0.029);
        m_tube = &m_body->addChild(std::make_unique<InnerTube>());
        m_tube->setLength(0.2);
        m_tube->setOuterRadius(0.012);
        m_tube->setAxialMethod(AxialMethod::TOP);
        m_tube->setAxialOffset(0.2);
        m_ring = &m_body->addChild(std::make_unique<CenteringRing>());
        m_ring->setAxialMethod(AxialMethod::TOP);
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    BodyTube*      m_body{nullptr};
    InnerTube*     m_tube{nullptr};
    CenteringRing* m_ring{nullptr};
};

TEST_F(CenteringRingTest, InnerRadiusFollowsTheOverlappingInnerTube)
{
    m_ring->setAxialOffset(0.3);
    EXPECT_EQ(m_ring->getOuterRadius(), 0.029);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);
    EXPECT_NEAR(m_ring->getThickness(), 0.017, kEpsilon);

    // Overlapping either end of the tube still counts.
    m_ring->setAxialOffset(0.399);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);
    m_ring->setAxialOffset(0.199);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);

    // Beyond the tube: no inner radius.
    m_ring->setAxialOffset(0.41);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.0);
    m_ring->setAxialOffset(0.1);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.0);
}

TEST_F(CenteringRingTest, InnerRadiusTakesTheWidestTubeAndAtMostTheOuterRadius)
{
    m_ring->setAxialOffset(0.3);
    auto& wide = m_body->addChild(std::make_unique<InnerTube>());
    wide.setLength(0.2);
    wide.setOuterRadius(0.015);
    wide.setAxialMethod(AxialMethod::TOP);
    wide.setAxialOffset(0.25);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.015);

    wide.setOuterRadius(0.04);  // wider than the body
    EXPECT_EQ(m_ring->getInnerRadius(), 0.029);
}

TEST_F(CenteringRingTest, OnlyRealInnerTubesCount)
{
    // A component of the INNER_TUBE kind that is not an InnerTube (Java: instanceof) is ignored.
    m_ring->setAxialOffset(0.3);
    static_cast<void>(m_body->removeChild(m_tube));
    auto& fake =
        m_body->addChild(TestComponent::make(0.2, ComponentKind::INNER_TUBE, AxialMethod::TOP));
    fake.setOuterRadius(0.012);
    fake.setAxialOffset(AxialMethod::TOP, 0.2);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.0);
}

TEST_F(CenteringRingTest, AutomaticInnerRadiusIsStored)
{
    m_ring->setAxialOffset(0.3);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);
    m_ring->setInnerRadiusAutomatic(false);
    EXPECT_FALSE(m_ring->isInnerRadiusAutomatic());
    m_tube->setOuterRadius(0.01);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);  // the stored value

    // A manual inner radius equal to the stored one changes nothing.
    m_ring->setInnerRadius(0.012);
    EXPECT_FALSE(m_ring->isInnerRadiusAutomatic());
}

TEST_F(CenteringRingTest, SetInnerRadiusMakesItManual)
{
    m_ring->setAxialOffset(0.3);
    m_ring->setInnerRadius(0.005);
    EXPECT_FALSE(m_ring->isInnerRadiusAutomatic());
    EXPECT_EQ(m_ring->getInnerRadius(), 0.005);
    m_ring->setInnerRadiusAutomatic(true);
    EXPECT_EQ(m_ring->getInnerRadius(), 0.012);
}

/// RocketTest.testEstesAlphaIII, the centering rings: two rings 6 mm long at TOP 0.14 m, 0.035 m
/// apart, in the Alpha III's body tube (0.2 m after a 0.07 m nose cone).
TEST(CenteringRing, EstesAlphaIIICenteringRingLocations)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
    auto& body = stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
    auto  ring = std::make_unique<CenteringRing>();
    ring->setName("Centering Rings");
    ring->setAxialMethod(AxialMethod::TOP);
    ring->setAxialOffset(0.14);
    ring->setLength(0.006);
    ring->setInstanceCount(2);
    ring->setInstanceSeparation(0.035);
    CenteringRing& rings = body.addChild(std::move(ring));
    rocket.enableEvents();

    EXPECT_EQ(rings.getInstanceCount(), 2) << rings.getName() << " not instanced correctly";
    // Singleton instances follow different code paths.
    rings.setInstanceCount(1);
    const Coordinate single = rings.getComponentLocations().at(0);
    EXPECT_NEAR(single.x, 0.21, 1e-8) << " position x fail";
    EXPECT_NEAR(single.y, 0.0, 1e-8) << " position y fail";
    EXPECT_NEAR(single.z, 0.0, 1e-8) << " position z fail";
    EXPECT_EQ(single, (Coordinate{0.21, 0, 0})) << rings.getName() << " not positioned correctly";

    rings.setInstanceCount(2);
    const std::vector<Coordinate> locations = rings.getComponentLocations();
    ASSERT_EQ(locations.size(), 2U);
    EXPECT_EQ(locations[0], (Coordinate{0.21, 0, 0})) << "first instance";
    EXPECT_EQ(rings.getInstanceCount(), 2) << rings.getName() << " not instanced correctly";
    EXPECT_EQ(locations[1], (Coordinate{0.245, 0, 0})) << "second instance";
}

}  // namespace
