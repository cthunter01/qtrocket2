#include "QtRocket/aero/AerodynamicForces.h"

#include <cmath>
#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::ModId;
using QtRocket::Rocket;
using QtRocket::Test::TestComponent;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

static_assert(QtRocket::Monitorable<AerodynamicForces>);

/// A rocket with a stage holding a body (a TestComponent) for the override tests.
struct OverrideRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    TestComponent*          body{nullptr};

    OverrideRocket()
    {
        stage = &rocket->addChild(std::make_unique<AxialStage>());
        body  = &stage->addChild(TestComponent::make(0.3));
        body->setName("Body");
        rocket->enableEvents();
    }
};

/// Forces with every drag value set: CD 0.5, pressure 0.1, base 0.2, friction 0.3, override 0.4.
[[nodiscard]] AerodynamicForces dragForces()
{
    AerodynamicForces forces;
    forces.zero();
    forces.setCD(0.5);
    forces.setPressureCD(0.1);
    forces.setBaseCD(0.2);
    forces.setFrictionCD(0.3);
    forces.setOverrideCD(0.4);
    return forces;
}

TEST(AerodynamicForces, DefaultsAreNaN)
{
    const AerodynamicForces forces;
    EXPECT_EQ(forces.getComponent(), nullptr);
    EXPECT_TRUE(forces.isAxisymmetric());
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(std::isnan(forces.getCN()));
    EXPECT_TRUE(std::isnan(forces.getCm()));
    EXPECT_TRUE(std::isnan(forces.getCside()));
    EXPECT_TRUE(std::isnan(forces.getCyaw()));
    EXPECT_TRUE(std::isnan(forces.getCroll()));
    EXPECT_TRUE(std::isnan(forces.getCrollDamp()));
    EXPECT_TRUE(std::isnan(forces.getCrollForce()));
    EXPECT_TRUE(std::isnan(forces.getCDaxial()));
    EXPECT_TRUE(std::isnan(forces.getCD()));
    EXPECT_TRUE(std::isnan(forces.getPressureCD()));
    EXPECT_TRUE(std::isnan(forces.getBaseCD()));
    EXPECT_TRUE(std::isnan(forces.getFrictionCD()));
    EXPECT_TRUE(std::isnan(forces.getOverrideCD()));
    EXPECT_TRUE(std::isnan(forces.getPitchDampingMoment()));
    EXPECT_TRUE(std::isnan(forces.getYawDampingMoment()));
    EXPECT_EQ(forces.modId(), ModId::invalid());
}

TEST(AerodynamicForces, CpIsStoredAsTheMomentWithCNa)
{
    AerodynamicForces forces;
    forces.setCP(Coordinate{0.375, 0.0, 0.0, 2.5});
    EXPECT_EQ(forces.getCP().x, 0.375);
    EXPECT_EQ(forces.getCP().weight, 2.5);

    forces.setCP(Coordinate{0.1, 0.2, 0.3, 0.7});
    EXPECT_NEAR(forces.getCP().x, 0.1, 1e-16);
    EXPECT_NEAR(forces.getCP().y, 0.2, 1e-16);
    EXPECT_NEAR(forces.getCP().z, 0.3, 1e-16);
    EXPECT_EQ(forces.getCP().weight, 0.7);

    // A CNa within MathUtil::equals of zero stores a zero moment, and the CP reads as zero.
    forces.setCP(Coordinate{1.0, 2.0, 3.0, 1e-10});
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    forces.setCP(Coordinate{5.0, 0.0, 0.0, 0.0});
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
}

TEST(AerodynamicForces, SettingAnEqualCpChangesNothing)
{
    AerodynamicForces forces;
    forces.setCP(Coordinate{0.5, 0, 0, 2});
    const ModId before = forces.modId();
    forces.setCP(Coordinate{0.5 * (1 + 1e-12), 0, 0, 2});  // within Coordinate's tolerance
    EXPECT_EQ(forces.modId(), before);
    EXPECT_EQ(forces.getCP().x, 0.5);
    forces.setCP(Coordinate{0.6, 0, 0, 2});
    EXPECT_GT(forces.modId(), before);
}

TEST(AerodynamicForces, SettersDrawANewIdOnlyOnAnExactChange)
{
    AerodynamicForces forces;
    ModId             last = forces.modId();

    forces.setCN(0.1);
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();
    forces.setCN(0.1);
    EXPECT_EQ(forces.modId(), last);
    forces.setCN(0.1 * (1 + 1e-15));  // exact comparison: a change
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();

    // NaN never equals itself, so setting it is always a change.
    forces.setCm(kNaN);
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();
    forces.setCm(kNaN);
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();

    forces.setAxisymmetric(true);
    EXPECT_EQ(forces.modId(), last);
    forces.setAxisymmetric(false);
    EXPECT_FALSE(forces.isAxisymmetric());
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();

    TestComponent component;
    forces.setComponent(&component);
    EXPECT_GT(forces.modId(), last);
    last = forces.modId();
    forces.setComponent(&component);
    EXPECT_EQ(forces.modId(), last);
    EXPECT_EQ(forces.getComponent(), &component);
}

TEST(AerodynamicForces, EverySetterStoresItsValue)
{
    AerodynamicForces forces;
    forces.setCN(1);
    forces.setCm(2);
    forces.setCside(3);
    forces.setCyaw(4);
    forces.setCroll(5);
    forces.setCrollDamp(6);
    forces.setCrollForce(7);
    forces.setCDaxial(8);
    forces.setCD(9);
    forces.setPressureCD(10);
    forces.setBaseCD(11);
    forces.setFrictionCD(12);
    forces.setOverrideCD(13);
    forces.setPitchDampingMoment(14);
    forces.setYawDampingMoment(15);
    EXPECT_EQ(forces.getCN(), 1);
    EXPECT_EQ(forces.getCm(), 2);
    EXPECT_EQ(forces.getCside(), 3);
    EXPECT_EQ(forces.getCyaw(), 4);
    EXPECT_EQ(forces.getCroll(), 5);
    EXPECT_EQ(forces.getCrollDamp(), 6);
    EXPECT_EQ(forces.getCrollForce(), 7);
    EXPECT_EQ(forces.getCDaxial(), 8);
    EXPECT_EQ(forces.getCD(), 9);
    EXPECT_EQ(forces.getPressureCD(), 10);
    EXPECT_EQ(forces.getBaseCD(), 11);
    EXPECT_EQ(forces.getFrictionCD(), 12);
    EXPECT_EQ(forces.getOverrideCD(), 13);
    EXPECT_EQ(forces.getPitchDampingMoment(), 14);
    EXPECT_EQ(forces.getYawDampingMoment(), 15);
}

TEST(AerodynamicForces, ZeroLeavesTheDragPartsAndTheComponent)
{
    TestComponent     component;
    AerodynamicForces forces;
    forces.setComponent(&component);
    forces.setAxisymmetric(false);
    forces.setCP(Coordinate{1, 0, 0, 3});

    AerodynamicForces& returned = forces.zero();
    EXPECT_EQ(&returned, &forces);
    EXPECT_EQ(forces.getComponent(), &component);
    EXPECT_TRUE(forces.isAxisymmetric());
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(forces.getCN(), 0);
    EXPECT_EQ(forces.getCm(), 0);
    EXPECT_EQ(forces.getCside(), 0);
    EXPECT_EQ(forces.getCyaw(), 0);
    EXPECT_EQ(forces.getCroll(), 0);
    EXPECT_EQ(forces.getCrollDamp(), 0);
    EXPECT_EQ(forces.getCrollForce(), 0);
    EXPECT_EQ(forces.getCDaxial(), 0);
    EXPECT_EQ(forces.getCD(), 0);
    EXPECT_EQ(forces.getPitchDampingMoment(), 0);
    EXPECT_EQ(forces.getYawDampingMoment(), 0);
    // Java's zero() does not touch these (the override CD reads as 0 for a component that is
    // neither overridden nor an assembly).
    EXPECT_TRUE(std::isnan(forces.getPressureCD()));
    EXPECT_TRUE(std::isnan(forces.getBaseCD()));
    EXPECT_TRUE(std::isnan(forces.getFrictionCD()));
    EXPECT_EQ(forces.getOverrideCD(), 0);
    forces.setComponent(nullptr);
    EXPECT_TRUE(std::isnan(forces.getOverrideCD()));
}

TEST(AerodynamicForces, MergeAddsTheCpMomentsAndTheNonAxialCoefficients)
{
    // Values pinned with OpenRocket's AerodynamicForces on JDK 17.
    AerodynamicForces first;
    first.zero();
    first.setCP(Coordinate{0.375, 0.0, 0.0, 2.5});
    first.setCN(0.1);
    first.setCm(-0.25);
    first.setCD(0.45);
    AerodynamicForces second;
    second.zero();
    second.setCP(Coordinate{1.25, 0.0, 0.0, 1.5});
    second.setCN(0.2);
    second.setCm(0.125);
    second.setCD(7.0);
    second.setCrollDamp(1.5);
    second.setCrollForce(-0.5);

    const ModId        before   = first.modId();
    AerodynamicForces& returned = first.merge(second);
    EXPECT_EQ(&returned, &first);
    EXPECT_GT(first.modId(), before);
    EXPECT_EQ(first.getCP().x, 0.70312500000000000);
    EXPECT_EQ(first.getCP().weight, 4.0);
    EXPECT_EQ(first.getCN(), 0.30000000000000004);
    EXPECT_EQ(first.getCm(), -0.125);
    EXPECT_EQ(first.getCrollDamp(), 1.5);
    EXPECT_EQ(first.getCrollForce(), -0.5);
    EXPECT_EQ(first.getCD(), 0.45);  // the drag is not merged

    // NaN coefficients stay NaN.
    AerodynamicForces fresh;
    fresh.merge(second);
    EXPECT_TRUE(std::isnan(fresh.getCN()));
    EXPECT_EQ(fresh.getCP().x, 1.25);  // the moments start at zero
}

TEST(AerodynamicForces, ComponentWithoutOverridesUsesTheStoredDrag)
{
    const OverrideRocket r;
    AerodynamicForces    forces = dragForces();
    forces.setComponent(r.body);
    EXPECT_EQ(forces.getCD(), 0.5);
    EXPECT_EQ(forces.getPressureCD(), 0.1);
    EXPECT_EQ(forces.getBaseCD(), 0.2);
    EXPECT_EQ(forces.getFrictionCD(), 0.3);
    EXPECT_EQ(forces.getOverrideCD(), 0);  // neither overridden nor an assembly
}

TEST(AerodynamicForces, OverriddenComponentReportsItsOverrideCD)
{
    const OverrideRocket r;
    r.body->setCDOverridden(true);
    r.body->setOverrideCD(0.75);
    AerodynamicForces forces = dragForces();
    forces.setComponent(r.body);
    EXPECT_EQ(forces.getCD(), 0.75);
    EXPECT_EQ(forces.getPressureCD(), 0);
    EXPECT_EQ(forces.getBaseCD(), 0);
    EXPECT_EQ(forces.getFrictionCD(), 0);
    EXPECT_EQ(forces.getOverrideCD(), 0.4);  // the stored value
}

TEST(AerodynamicForces, ComponentOverriddenByAnAncestorHasNoDrag)
{
    const OverrideRocket r;
    r.stage->setCDOverridden(true);
    r.stage->setSubcomponentsOverriddenCD(true);
    ASSERT_TRUE(r.body->isCDOverriddenByAncestor());
    r.body->setCDOverridden(true);  // its own override does not count then

    AerodynamicForces forces = dragForces();
    forces.setComponent(r.body);
    EXPECT_EQ(forces.getCD(), 0);
    EXPECT_EQ(forces.getPressureCD(), 0);
    EXPECT_EQ(forces.getBaseCD(), 0);
    EXPECT_EQ(forces.getFrictionCD(), 0);
    EXPECT_EQ(forces.getOverrideCD(), 0);
}

TEST(AerodynamicForces, AssemblyValuesAreAlreadyAggregated)
{
    const OverrideRocket r;
    r.stage->setCDOverridden(true);
    r.stage->setOverrideCD(0.9);
    AerodynamicForces forces = dragForces();
    forces.setComponent(r.stage);
    EXPECT_EQ(forces.getCD(), 0.5);  // not the override CD
    EXPECT_EQ(forces.getPressureCD(), 0.1);
    EXPECT_EQ(forces.getBaseCD(), 0.2);
    EXPECT_EQ(forces.getFrictionCD(), 0.3);
    EXPECT_EQ(forces.getOverrideCD(), 0.4);

    // An assembly that is not overridden still reports its subtree's override contributions.
    r.stage->setCDOverridden(false);
    EXPECT_EQ(forces.getOverrideCD(), 0.4);

    // An assembly component kind counts as an assembly whatever its class.
    TestComponent podLike{ComponentKind::POD_SET};
    podLike.setCDOverridden(true);
    forces.setComponent(&podLike);
    EXPECT_EQ(forces.getCD(), 0.5);
    EXPECT_EQ(forces.getPressureCD(), 0.1);
}

TEST(AerodynamicForces, CDTotalCountsTheInstances)
{
    const OverrideRocket r;
    r.body->setInstanceCount(3);
    AerodynamicForces forces = dragForces();
    forces.setComponent(r.body);
    EXPECT_EQ(forces.getCDTotal(), 1.5);

    forces.setComponent(nullptr);
    EXPECT_THROW((void)forces.getCDTotal(), BugError);  // Java: NullPointerException
}

TEST(AerodynamicForces, ResetClearsTheComponentAndTheCoefficients)
{
    TestComponent     component;
    AerodynamicForces forces = dragForces();
    forces.setComponent(&component);
    forces.setCP(Coordinate{1, 0, 0, 1});
    forces.setAxisymmetric(false);
    const ModId before = forces.modId();

    forces.reset();
    EXPECT_GT(forces.modId(), before);
    EXPECT_EQ(forces.getComponent(), nullptr);
    EXPECT_TRUE(forces.getCP().isNaN());
    EXPECT_TRUE(std::isnan(forces.getCN()));
    EXPECT_TRUE(std::isnan(forces.getCm()));
    EXPECT_TRUE(std::isnan(forces.getCside()));
    EXPECT_TRUE(std::isnan(forces.getCyaw()));
    EXPECT_TRUE(std::isnan(forces.getCroll()));
    EXPECT_TRUE(std::isnan(forces.getCrollDamp()));
    EXPECT_TRUE(std::isnan(forces.getCrollForce()));
    EXPECT_TRUE(std::isnan(forces.getCDaxial()));
    EXPECT_TRUE(std::isnan(forces.getCD()));
    EXPECT_TRUE(std::isnan(forces.getPitchDampingMoment()));
    EXPECT_TRUE(std::isnan(forces.getYawDampingMoment()));
    // As in Java, these stay.
    EXPECT_EQ(forces.getPressureCD(), 0.1);
    EXPECT_EQ(forces.getOverrideCD(), 0.4);
    EXPECT_FALSE(forces.isAxisymmetric());
}

TEST(AerodynamicForces, CopyIsJavasClone)
{
    TestComponent     component;
    AerodynamicForces forces = dragForces();
    forces.setComponent(&component);
    forces.setCP(Coordinate{0.25, 0, 0, 2});

    AerodynamicForces copy = forces;
    EXPECT_EQ(copy, forces);
    EXPECT_EQ(copy.modId(), forces.modId());
    EXPECT_EQ(copy.getComponent(), &component);

    copy.setCN(3);
    EXPECT_EQ(forces.getCN(), 0);
}

TEST(AerodynamicForces, EqualityComparesTheGettersWithinTolerance)
{
    AerodynamicForces a = dragForces();
    AerodynamicForces b = dragForces();
    EXPECT_EQ(a, b);

    b.setCN(1e-12);  // MathUtil::equals compares to zero absolutely within 5e-9
    EXPECT_EQ(a, b);
    b.setCN(0.01);
    EXPECT_NE(a, b);

    // The override CD, the component pointer and the axisymmetric flag do not count.
    AerodynamicForces c = dragForces();
    c.setOverrideCD(99);
    c.setAxisymmetric(false);
    EXPECT_EQ(a, c);

    // The CP does.
    AerodynamicForces d = dragForces();
    d.setCP(Coordinate{0.5, 0, 0, 1});
    EXPECT_NE(a, d);

    // The getters apply the component's override.
    const OverrideRocket r;
    r.body->setCDOverridden(true);
    r.body->setOverrideCD(0.5);
    AerodynamicForces e = dragForces();
    e.setComponent(r.body);
    EXPECT_NE(a, e);  // pressure, base and friction read as zero

    // NaN coefficients never compare equal, except for an object with itself.
    const AerodynamicForces nan;
    const AerodynamicForces nanCopy = nan;
    EXPECT_FALSE(nan == nanCopy);
    EXPECT_TRUE(nan == nan);
}

TEST(AerodynamicForces, HashCodeIsJavas)
{
    // Values pinned with OpenRocket's AerodynamicForces on JDK 17.
    AerodynamicForces forces;
    EXPECT_EQ(forces.hashCode(), 0);  // (int) NaN is 0
    forces.zero();
    EXPECT_EQ(forces.hashCode(), 0);

    forces.setCP(Coordinate{0.375, 0.0, 0.0, 2.5});
    forces.setCN(0.1);
    forces.setCm(-0.25);
    forces.setCside(1e-5);
    forces.setCyaw(1234567.0);
    forces.setCroll(1.0E7);
    forces.setCDaxial(0.4);
    forces.setCD(0.45);
    EXPECT_EQ(forces.hashCode(), 40850);

    AerodynamicForces other;
    other.setCP(Coordinate{0.1, 0.2, 0.3, 0.7});
    other.setCD(1.5);
    other.setCDaxial(-0.75);
    EXPECT_EQ(other.hashCode(), 61450);
}

TEST(AerodynamicForces, ToStringIsJavas)
{
    // Strings pinned with OpenRocket's AerodynamicForces on JDK 17.
    AerodynamicForces forces;
    EXPECT_EQ(forces.toString(), "AerodynamicForces[cp:(0.00000,0.00000,0.00000)]");

    forces.zero();
    EXPECT_EQ(forces.toString(),
              "AerodynamicForces[cp:(0.00000,0.00000,0.00000),CN:0.0,Cm:0.0,Cside:0.0,Cyaw:0.0,"
              "Croll:0.0,CDaxial:0.0,CD:0.0]");

    forces.setCP(Coordinate{0.375, 0.0, 0.0, 2.5});
    forces.setCN(0.1);
    forces.setCm(-0.25);
    forces.setCside(1e-5);
    forces.setCyaw(1234567.0);
    forces.setCroll(1.0E7);
    forces.setCDaxial(0.4);
    forces.setCD(0.45);
    EXPECT_EQ(forces.toString(),
              "AerodynamicForces[cp:(0.37500,0.00000,0.00000,w=2.50000),CN:0.1,Cm:-0.25,"
              "Cside:1.0E-5,Cyaw:1234567.0,Croll:1.0E7,CDaxial:0.4,CD:0.45]");

    AerodynamicForces other;
    other.setCP(Coordinate{0.1, 0.2, 0.3, 0.7});
    other.setCD(1.5);
    other.setCDaxial(-0.75);
    EXPECT_EQ(other.toString(),
              "AerodynamicForces[cp:(0.10000,0.20000,0.30000,w=0.70000),CDaxial:-0.75,CD:1.5]");

    // The component is printed by its name (Java's RocketComponent.toString()).
    const OverrideRocket r;
    other.setComponent(r.body);
    EXPECT_EQ(other.toString(),
              "AerodynamicForces[component:Body,cp:(0.10000,0.20000,0.30000,"
              "w=0.70000),CDaxial:-0.75,CD:1.5]");
}

}  // namespace
