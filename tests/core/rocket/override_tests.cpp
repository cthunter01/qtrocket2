// OverrideTest.java (core/src/test/.../rocketcomponent), ported, on the Estes Alpha III of
// TestRockets.h.
//
// testOverriddenBy is ported in full. testCDAncestorOverrides is ported up to the two assertions
// that compare a BarrowmanCalculator's drag coefficient with the override CDs: they wait for the
// aero tier (marked HOOK(barrowman) below; RocketComponent.h, "Deferred to aero/").

#include <source_location>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::CenteringRing;
using QtRocket::EngineBlock;
using QtRocket::FinSet;
using QtRocket::InnerTube;
using QtRocket::LaunchLug;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestEstesAlphaIII;

/// The last child of @p parent that is a @p Component, or nullptr (the loops of
/// testCDAncestorOverrides: `for (c : parent.getChildren()) if (c instanceof X) x = (X) c;`).
template <class Component>
[[nodiscard]] Component* lastChildOfType(RocketComponent& parent)
{
    Component* found = nullptr;
    for (RocketComponent* const child : parent.getChildren())
    {
        if (auto* const typed = dynamic_cast<Component*>(child))
        {
            found = typed;
        }
    }
    return found;
}

/// The three assertions testOverriddenBy makes on a component: which component's override of the
/// mass, of the CG and of the CD covers it (nullptr: Java's assertNull). Java compares with
/// equals() (the same class and id); the same object is expected here.
void expectOverriddenBy(const RocketComponent& component, const RocketComponent* mass,
                        const RocketComponent* cg, const RocketComponent* cd,
                        std::source_location where = std::source_location::current())
{
    const ::testing::ScopedTrace trace(where.file_name(), static_cast<int>(where.line()),
                                       component.getName());
    EXPECT_EQ(component.getMassOverriddenBy(), mass);
    EXPECT_EQ(component.getCGOverriddenBy(), cg);
    EXPECT_EQ(component.getCDOverriddenBy(), cd);
}

// Java: testCDAncestorOverrides
TEST(OverrideTest, CDAncestorOverrides)
{
    // create test rocket
    const TestEstesAlphaIII alpha;
    Rocket&                 rocket = *alpha.rocket;

    // obtain sustainer, nose cone, body tube, and fin set
    auto* const sustainer = lastChildOfType<AxialStage>(rocket);
    ASSERT_NE(sustainer, nullptr);

    const auto* const nosecone = lastChildOfType<NoseCone>(*sustainer);
    auto* const       bodytube = lastChildOfType<BodyTube>(*sustainer);
    ASSERT_NE(nosecone, nullptr);
    ASSERT_NE(bodytube, nullptr);

    const auto* const finset = lastChildOfType<FinSet>(*bodytube);
    ASSERT_NE(finset, nullptr);

    // We start by just checking the override flags
    // Initially no overrides
    EXPECT_FALSE(sustainer->isCDOverridden());
    EXPECT_FALSE(sustainer->isSubcomponentsOverriddenCD());
    EXPECT_FALSE(sustainer->isCDOverriddenByAncestor());

    EXPECT_FALSE(bodytube->isCDOverridden());
    EXPECT_FALSE(bodytube->isSubcomponentsOverriddenCD());
    EXPECT_FALSE(bodytube->isCDOverriddenByAncestor());

    EXPECT_FALSE(finset->isCDOverridden());
    EXPECT_FALSE(finset->isSubcomponentsOverriddenCD());
    EXPECT_FALSE(finset->isCDOverriddenByAncestor());

    // Override sustainer CD and subcomponents
    sustainer->setSubcomponentsOverriddenCD(true);
    sustainer->setCDOverridden(true);
    sustainer->setOverrideCD(0.5);

    EXPECT_TRUE(bodytube->isCDOverriddenByAncestor());
    EXPECT_TRUE(finset->isCDOverriddenByAncestor());

    // Set body tube to override subcomponents, override its CD; it's still overridden by ancestor
    bodytube->setCDOverridden(true);
    bodytube->setSubcomponentsOverriddenCD(true);
    bodytube->setOverrideCD(0.25);

    EXPECT_TRUE(bodytube->isCDOverriddenByAncestor());

    // Now see if it's actually working
    // HOOK(barrowman): total CD should be overrideCD of sustainer:
    //   assertEquals(sustainer.getOverrideCD(),
    //                calc.getAerodynamicForces(configuration, conditions, warnings).getCD(),
    //                MathUtil.EPSILON)
    // with a BarrowmanCalculator, FlightConditions(null) and the selected configuration. Its
    // left side needs no calculator: the override CD of an overridden component is the stored one.
    EXPECT_EQ(sustainer->getOverrideCD(), 0.5);

    // Turn off sustainer subcomponents override; body tube and nose cone aren't overridden by
    // ancestor but fin set is
    sustainer->setSubcomponentsOverriddenCD(false);

    // HOOK(barrowman): CD of rocket should be overridden CD of sustainer plus body tube plus
    // calculated CD of nose cone:
    //   forceMap = calc.getForceAnalysis(configuration, conditions, warnings)
    //   assertEquals(sustainer.getOverrideCD() + bodytube.getOverrideCD()
    //                    + forceMap.get(nosecone).getCD(),
    //                forceMap.get(rocket).getCD(), MathUtil.EPSILON)
    // The two override CDs of its left side:
    EXPECT_EQ(sustainer->getOverrideCD(), 0.5);
    EXPECT_EQ(bodytube->getOverrideCD(), 0.25);
}

/// Test whether children components of a parent that has subcomponents overridden for mass, CG,
/// or CD have the correct overriddenBy object.
// Java: testOverriddenBy
TEST(OverrideTest, OverriddenBy)
{
    // Create test rocket
    const TestEstesAlphaIII alpha;
    const Rocket&           rocket = *alpha.rocket;

    // Obtain the necessary components
    AxialStage* const sustainerStage = rocket.getStage(0);
    ASSERT_NE(sustainerStage, nullptr);
    const AxialStage& sustainer   = *sustainerStage;
    auto&             noseCone    = dynamic_cast<NoseCone&>(sustainerStage->getChild(0));
    auto&             bodyTube    = dynamic_cast<BodyTube&>(sustainerStage->getChild(1));
    const auto&       finSet      = dynamic_cast<FinSet&>(bodyTube.getChild(0));
    const auto&       launchLug   = dynamic_cast<LaunchLug&>(bodyTube.getChild(1));
    auto&             innerTube   = dynamic_cast<InnerTube&>(bodyTube.getChild(2));
    const auto&       engineBlock = dynamic_cast<EngineBlock&>(innerTube.getChild(0));
    const auto&       parachute   = dynamic_cast<Parachute&>(bodyTube.getChild(3));
    const auto&       bulkhead    = dynamic_cast<CenteringRing&>(bodyTube.getChild(4));

    // Check initial override by components
    expectOverriddenBy(rocket, nullptr, nullptr, nullptr);
    expectOverriddenBy(sustainer, nullptr, nullptr, nullptr);
    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, nullptr, nullptr);
    expectOverriddenBy(launchLug, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(engineBlock, nullptr, nullptr, nullptr);
    expectOverriddenBy(parachute, nullptr, nullptr, nullptr);
    expectOverriddenBy(bulkhead, nullptr, nullptr, nullptr);

    // Override body tube mass, CG, and CD without for subcomponents
    bodyTube.setMassOverridden(true);
    bodyTube.setCGOverridden(true);
    bodyTube.setCDOverridden(true);

    expectOverriddenBy(sustainer, nullptr, nullptr, nullptr);
    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, nullptr, nullptr);
    expectOverriddenBy(launchLug, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(engineBlock, nullptr, nullptr, nullptr);
    expectOverriddenBy(parachute, nullptr, nullptr, nullptr);
    expectOverriddenBy(bulkhead, nullptr, nullptr, nullptr);

    // Override body tube mass for subcomponents
    bodyTube.setSubcomponentsOverriddenMass(true);

    expectOverriddenBy(sustainer, nullptr, nullptr, nullptr);
    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(launchLug, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(innerTube, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(engineBlock, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(parachute, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(bulkhead, &bodyTube, nullptr, nullptr);

    // Undo override body tube mass for subcomponents, do override of CG and CD for subcomponents
    bodyTube.setSubcomponentsOverriddenMass(false);
    bodyTube.setSubcomponentsOverriddenCG(true);
    bodyTube.setSubcomponentsOverriddenCD(true);

    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(launchLug, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(innerTube, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(engineBlock, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(parachute, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(bulkhead, nullptr, &bodyTube, &bodyTube);

    // Move the inner tube from the body tube to the nose cone
    noseCone.addChild(bodyTube.removeChild(&innerTube));

    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(engineBlock, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(launchLug, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(parachute, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(bulkhead, nullptr, &bodyTube, &bodyTube);

    // Override mass of nose cone
    noseCone.setMassOverridden(true);

    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(engineBlock, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(launchLug, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(parachute, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(bulkhead, nullptr, &bodyTube, &bodyTube);

    // Override nose cone mass for all subcomponents
    noseCone.setSubcomponentsOverriddenMass(true);

    expectOverriddenBy(sustainer, nullptr, nullptr, nullptr);
    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, &noseCone, nullptr, nullptr);
    expectOverriddenBy(engineBlock, &noseCone, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(launchLug, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(parachute, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(bulkhead, nullptr, &bodyTube, &bodyTube);

    // Override inner tube CG for all subcomponents
    innerTube.setCGOverridden(true);
    innerTube.setSubcomponentsOverriddenCG(true);

    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(innerTube, &noseCone, nullptr, nullptr);
    expectOverriddenBy(engineBlock, &noseCone, &innerTube, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(launchLug, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(parachute, nullptr, &bodyTube, &bodyTube);
    expectOverriddenBy(bulkhead, nullptr, &bodyTube, &bodyTube);

    // Set body tube mass override, reset CG & CD, and move inner tube back to body tube
    bodyTube.setMassOverridden(true);
    bodyTube.setSubcomponentsOverriddenMass(true);
    bodyTube.setCGOverridden(false);
    bodyTube.setCDOverridden(false);
    bodyTube.addChild(noseCone.removeChild(&innerTube));

    expectOverriddenBy(noseCone, nullptr, nullptr, nullptr);
    expectOverriddenBy(bodyTube, nullptr, nullptr, nullptr);
    expectOverriddenBy(finSet, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(launchLug, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(parachute, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(bulkhead, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(innerTube, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(engineBlock, &bodyTube, &innerTube, nullptr);

    // Toggle the body tube CG override for all subcomponents
    bodyTube.setCGOverridden(true);
    bodyTube.setSubcomponentsOverriddenCG(true);

    expectOverriddenBy(finSet, &bodyTube, &bodyTube, nullptr);
    expectOverriddenBy(launchLug, &bodyTube, &bodyTube, nullptr);
    expectOverriddenBy(parachute, &bodyTube, &bodyTube, nullptr);
    expectOverriddenBy(bulkhead, &bodyTube, &bodyTube, nullptr);
    expectOverriddenBy(innerTube, &bodyTube, &bodyTube, nullptr);
    expectOverriddenBy(engineBlock, &bodyTube, &bodyTube, nullptr);

    // Toggle back
    bodyTube.setSubcomponentsOverriddenCG(false);
    expectOverriddenBy(finSet, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(launchLug, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(parachute, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(bulkhead, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(innerTube, &bodyTube, nullptr, nullptr);
    expectOverriddenBy(engineBlock, &bodyTube, &innerTube, nullptr);
}

}  // namespace
