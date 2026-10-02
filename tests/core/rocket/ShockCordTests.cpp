// ShockCordTest.java (core/src/test/.../rocketcomponent), ported (but for the .ork round trip,
// which needs the document and the file format), and the shock cord's own behaviour.

#include "QtRocket/rocket/ShockCord.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestBodyComponent.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Material;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::ShockCord;
using QtRocket::Test::TestBodyComponent;

constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// ShockCordTest.createDocumentWithRecoveryComponents(): a new rocket (one stage) with a body
/// tube 1 m long of radius 0.05 m holding a parachute and a shock cord; events enabled.
class ShockCordTest : public ::testing::Test
{
protected:
    ShockCordTest()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_rocket.enableEvents();
        auto body = TestBodyComponent::make(0.0, 0.05);  // a BodyTube stand-in
        body->setLength(1.0);
        auto& added = stage.addChild(std::move(body));
        added.addChild(std::make_unique<Parachute>());
        m_cord = &added.addChild(std::make_unique<ShockCord>());
    }

    Rocket     m_rocket;
    ShockCord* m_cord{nullptr};
};

// Java: testShockCordLengthAutomaticDefault
TEST_F(ShockCordTest, ShockCordLengthAutomaticDefault)
{
    EXPECT_TRUE(m_cord->isCordLengthAutomatic()) << "Expected cord length automatic by default";
    EXPECT_EQ(m_rocket.getLength(), 1.0);
    EXPECT_NEAR(m_rocket.getLength() * 3.0, m_cord->getCordLength(), kEpsilon)
        << "Auto shock cord length should be 3x rocket length";
}

// Java: testShockCordLengthAutomaticSet
TEST_F(ShockCordTest, ShockCordLengthAutomaticSet)
{
    m_cord->setCordLength(1.2);
    m_cord->setCordLengthAutomatic(true);
    EXPECT_TRUE(m_cord->isCordLengthAutomatic())
        << "Cord length should be automatic after enabling";
    EXPECT_NEAR(m_rocket.getLength() * 3.0, m_cord->getCordLength(), kEpsilon)
        << "Auto shock cord length should be 3x rocket length";
}

// Java: testShockCordLengthManualDisablesAutomatic
TEST_F(ShockCordTest, ShockCordLengthManualDisablesAutomatic)
{
    m_cord->setCordLengthAutomatic(true);
    m_cord->setCordLength(1.2);
    EXPECT_FALSE(m_cord->isCordLengthAutomatic()) << "Manual cord length should disable automatic";
    EXPECT_NEAR(1.2, m_cord->getCordLength(), kEpsilon) << "Manual cord length should be retained";
}

TEST_F(ShockCordTest, AutomaticLengthFollowsTheRocket)
{
    auto& stage = dynamic_cast<AxialStage&>(m_rocket.getChild(0));
    stage.addChild(TestBodyComponent::make(0.5, 0.05));
    EXPECT_EQ(m_rocket.getLength(), 1.5);
    EXPECT_NEAR(m_cord->getCordLength(), 4.5, kEpsilon);
    // The mass follows the cord length.
    EXPECT_NEAR(m_cord->getComponentMass(), m_cord->getMaterial().getDensity() * 4.5, 1e-15);
}

TEST_F(ShockCordTest, AutomaticLengthFollowsEveryChangeOfTheRocketLength)
{
    // Java reads getRocket().getLength() at every call: a resized body, an added stage and a
    // removed body all show at once.
    auto& stage = dynamic_cast<AxialStage&>(m_rocket.getChild(0));
    auto& body  = dynamic_cast<TestBodyComponent&>(stage.getChild(0));
    body.setLength(2.0);
    EXPECT_EQ(m_rocket.getLength(), 2.0);
    EXPECT_NEAR(m_cord->getCordLength(), 6.0, kEpsilon);

    auto& booster     = m_rocket.addChild(std::make_unique<AxialStage>());
    auto& boosterBody = booster.addChild(TestBodyComponent::make(0.5, 0.05));
    EXPECT_NEAR(m_cord->getCordLength(), 7.5, kEpsilon);
    static_cast<void>(booster.removeChild(&boosterBody));
    EXPECT_NEAR(m_cord->getCordLength(), 6.0, kEpsilon);

    // A cord taken out of the rocket keeps the last length it computed.
    const std::unique_ptr<QtRocket::RocketComponent> detached = body.removeChild(m_cord);
    ASSERT_NE(detached, nullptr);
    EXPECT_NEAR(m_cord->getCordLength(), 6.0, kEpsilon);
}

TEST(ShockCord, Defaults)
{
    const ShockCord cord;
    EXPECT_EQ(cord.kind(), ComponentKind::SHOCK_CORD);
    EXPECT_EQ(cord.getName(), "Shock Cord");
    EXPECT_TRUE(cord.isCordLengthAutomatic());
    EXPECT_EQ(cord.getCordLength(), 0.4);  // outside a rocket: the stored length
    EXPECT_EQ(cord.getMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_EQ(cord.getMaterial().getType(), Material::Type::LINE);
    EXPECT_EQ(cord.getMaterial().getDensity(), 0.0018);
    EXPECT_NEAR(cord.getComponentMass(), 0.0018 * 0.4, 1e-18);
    EXPECT_EQ(cord.getDisplayOrderSide(), 12);
    EXPECT_EQ(cord.getDisplayOrderBack(), 7);
    EXPECT_EQ(cord.getLength(), 0.025);
    EXPECT_EQ(cord.getRadius(), 0.0125);
    EXPECT_EQ(cord.getPresetType(), std::nullopt);
    const std::vector<Material> materials = cord.getAllMaterials();
    ASSERT_EQ(materials.size(), 1U);
    EXPECT_EQ(materials.front(), cord.getMaterial());
}

TEST(ShockCord, HoldsNoChildren)
{
    ShockCord cord;
    EXPECT_FALSE(cord.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(cord.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(cord.addChild(std::make_unique<ShockCord>()), BugError);
}

TEST(ShockCord, CordLengthIsClamped)
{
    ShockCord cord;
    cord.setCordLength(-1.0);
    EXPECT_EQ(cord.getCordLength(), 0.0);
    EXPECT_FALSE(cord.isCordLengthAutomatic());
    cord.setCordLength(std::numeric_limits<double>::quiet_NaN());  // MathUtil.max(NaN, 0) is 0
    EXPECT_EQ(cord.getCordLength(), 0.0);
}

TEST(ShockCord, MaterialMustBeALineMaterial)
{
    ShockCord cord;
    EXPECT_THROW(cord.setMaterial(Material::newMaterial(Material::Type::BULK, "b", 1.0, true)),
                 BugError);
    const Material kevlar = Material::newMaterial(Material::Type::LINE, "Kevlar", 0.003, true);
    cord.setMaterial(kevlar);
    EXPECT_EQ(cord.getMaterial(), kevlar);
    EXPECT_NEAR(cord.getComponentMass(), 0.003 * 0.4, 1e-18);
}

TEST_F(ShockCordTest, SettersFireMassChangesOnChange)
{
    std::vector<int>                              types;
    const ComponentChangeSignal::ScopedConnection connection{m_rocket.addComponentChangeListener(
        [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};
    m_cord->setMaterial(m_cord->getMaterial());
    m_cord->setCordLengthAutomatic(true);
    EXPECT_TRUE(types.empty());
    m_cord->setMaterial(Material::newMaterial(Material::Type::LINE, "Kevlar", 0.003, true));
    m_cord->setCordLength(2.0);
    m_cord->setCordLength(2.0);  // manual and equal: nothing
    m_cord->setCordLengthAutomatic(true);
    EXPECT_EQ(types, std::vector<int>(3, ComponentChangeEvent::kMassChange));
}

}  // namespace
