#include "QtRocket/rocket/TrapezoidFinSet.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::Material;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::MathUtil::javaToRadians;

/// TrapezoidFinSetTest's tolerance.
constexpr double kEpsilon = 1.0E-8;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Java's protected setAxialOffset(method, offset), which the JUnit tests (in the same package)
/// call: here the public setAxialMethod() and setAxialOffset(), which end in the same place.
void setAxialOffset(RocketComponent& component, AxialMethod method, double offset)
{
    component.setAxialMethod(method);
    component.setAxialOffset(offset);
}

/// TrapezoidFinSetTest.createSimpleTrapezoidalFin(): a rocket with one stage, a body tube (0.2 m
/// long, radius 0.1 m) and one trapezoidal fin in its middle:
///
///     sweep= 0.02 | tipChord = 0.02
///            |    |      |
///            |    +------+  ----------
///            |   /        \ .
///            |  /          \     height = 0.05
///            | /            \ .
///             /              \ .
///  __________/________________\_____   length == rootChord == 0.06
///
/// 5 mm thick, of density 1, without tab or fillets.
class SimpleTrapezoidalFin : public ::testing::Test
{
protected:
    SimpleTrapezoidalFin()
    {
        auto& stg = m_rocket.addChild(std::make_unique<AxialStage>());
        m_body    = &stg.addChild(std::make_unique<BodyTube>(0.2, 0.1));
        auto fins = std::make_unique<TrapezoidFinSet>(1, 0.06, 0.02, 0.02, 0.05);
        setAxialOffset(*fins, AxialMethod::MIDDLE, 0.0);
        fins->setMaterial(
            Material::newMaterial(Material::Type::BULK, "Fin-Test-Material", 1.0, true));
        fins->setThickness(0.005);  // == 5 mm

        m_fins = &m_body->addChild(std::move(fins));

        m_fins->setTabLength(0.00);

        m_fins->setFilletRadius(0.0);

        m_rocket.enableEvents();
    }

    /// Records the types of the events fired from now on.
    void recordEvents()
    {
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    /// The event types fired since the last call.
    [[nodiscard]] std::vector<int> takeEvents()
    {
        std::vector<int> types;
        types.swap(m_types);
        return types;
    }

    Rocket                                  m_rocket;
    BodyTube*                               m_body{nullptr};
    TrapezoidFinSet*                        m_fins{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// TrapezoidFinSetTest.createFreeformFinOnTransition(): a rocket with one stage, an ogive
/// transition (0.2 m long, radii 0.1 m and 0.3 m) and one default freeform fin in its middle,
/// 5 mm thick, of density 1.
class FreeformFinOnTransition : public ::testing::Test
{
protected:
    FreeformFinOnTransition()
    {
        auto& stg        = m_rocket.addChild(std::make_unique<AxialStage>());
        auto  transition = std::make_unique<Transition>();
        transition->setLength(0.2);
        transition->setForeRadius(0.1);
        transition->setAftRadius(0.3);
        transition->setShapeType(TransitionShape::OGIVE);
        m_transition = &stg.addChild(std::move(transition));
        auto fins    = std::make_unique<FreeformFinSet>();
        fins->setFinCount(1);
        setAxialOffset(*fins, AxialMethod::MIDDLE, 0.0);
        fins->setMaterial(
            Material::newMaterial(Material::Type::BULK, "Fin-Test-Material", 1.0, true));
        fins->setThickness(0.005);  // == 5 mm

        m_fins = &m_transition->addChild(std::move(fins));

        m_fins->setTabLength(0.00);

        m_fins->setFilletRadius(0.0);

        m_rocket.enableEvents();
    }

    Rocket          m_rocket;
    Transition*     m_transition{nullptr};
    FreeformFinSet* m_fins{nullptr};
};

// ====================================================================== TrapezoidFinSetTest

TEST(TrapezoidFinSet, Multiplicity)
{
    const TrapezoidFinSet trapFins;
    EXPECT_EQ(3, trapFins.getFinCount());
}

TEST_F(SimpleTrapezoidalFin, GenerateTrapezoidalPoints)
{
    const FinSet& fins = *m_fins;

    const std::vector<Coordinate> actPoints = fins.getFinPoints();

    const std::vector<Coordinate> expPoints{Coordinate{0.00, 0.0}, Coordinate{0.02, 0.05},
                                            Coordinate{0.04, 0.05}, Coordinate{0.06, 0.0},
                                            Coordinate{0.00, 0.0}};

    // (The Java test compares the x coordinates only, three times; the y coordinates hold too.)
    ASSERT_EQ(actPoints.size(), 4U);
    for (std::size_t index = 0; index < actPoints.size(); ++index)
    {
        EXPECT_NEAR(expPoints[index].x, actPoints[index].x, kEpsilon)
            << " generated fin point [" << index << "] doesn't match! ";
        EXPECT_NEAR(expPoints[index].y, actPoints[index].y, kEpsilon)
            << " generated fin point [" << index << "] doesn't match! ";
    }
}

/// @p actual has the points @p expected, x, y and z within TrapezoidFinSetTest's tolerance.
void expectCantedPointsMatch(const std::vector<Coordinate>& expected,
                             const std::vector<Coordinate>& actual, std::string_view what)
{
    ASSERT_EQ(expected.size(), actual.size()) << what << " number of points doesn't match! ";
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_NEAR(expected[i].x, actual[i].x, kEpsilon) << what << " [" << i << "]";
        EXPECT_NEAR(expected[i].y, actual[i].y, kEpsilon) << what << " [" << i << "]";
        EXPECT_NEAR(expected[i].z, actual[i].z, kEpsilon) << what << " [" << i << "]";
    }
}

/// The root points of the simple fin canted by 15 degrees (the Java test's table).
[[nodiscard]] std::vector<Coordinate> cantedRootPoints()
{
    return {Coordinate{0.0000, -0.000301899, 0.0000}, Coordinate{0.0025, -0.000253617, 0.0000},
            Coordinate{0.0050, -0.000209555, 0.0000}, Coordinate{0.0075, -0.000169706, 0.0000},
            Coordinate{0.0100, -0.000134064, 0.0000}, Coordinate{0.0125, -0.000102627, 0.0000},
            Coordinate{0.0150, -0.000075389, 0.0000}, Coordinate{0.0175, -0.000052348, 0.0000},
            Coordinate{0.0200, -0.000033499, 0.0000}, Coordinate{0.0225, -0.000018842, 0.0000},
            Coordinate{0.0250, -0.000008374, 0.0000}, Coordinate{0.0275, -0.000002093, 0.0000},
            Coordinate{0.0300, 0.0000, 0.0000},       Coordinate{0.0325, -0.000002093, 0.0000},
            Coordinate{0.0350, -0.000008374, 0.0000}, Coordinate{0.0375, -0.000018842, 0.0000},
            Coordinate{0.0400, -0.000033499, 0.0000}, Coordinate{0.0425, -0.000052348, 0.0000},
            Coordinate{0.0450, -0.000075389, 0.0000}, Coordinate{0.0475, -0.000102627, 0.0000},
            Coordinate{0.0500, -0.000134064, 0.0000}, Coordinate{0.0525, -0.000169706, 0.0000},
            Coordinate{0.0550, -0.000209555, 0.0000}, Coordinate{0.0575, -0.000253617, 0.0000},
            Coordinate{0.0600, -0.000301899, 0.0000}};
}

TEST_F(SimpleTrapezoidalFin, GenerateTrapezoidalPointsWithCant)
{
    FinSet& fins = *m_fins;
    fins.setCantAngle(javaToRadians(15));

    const std::vector<Coordinate> expPoints{
        Coordinate{0.00, -0.00030189855, 0.00}, Coordinate{0.02, 0.05, 0.00},
        Coordinate{0.04, 0.05, 0.00}, Coordinate{0.06, -0.00030189855, 0.00}};

    expectCantedPointsMatch(expPoints, fins.getFinPoints(), "Canted fin point");
    expectCantedPointsMatch(cantedRootPoints(), fins.getRootPoints(), "Canted root point");
}

TEST_F(SimpleTrapezoidalFin, CGCalculationSimpleSquareFin)
{
    TrapezoidFinSet& fins = *m_fins;

    // This is a simple square fin with sides of 0.1.
    fins.setFinShape(0.1, 0.1, 0.0, 0.1, 0.005);

    // should return a single-fin-planform area
    EXPECT_NEAR(0.01, fins.getPlanformArea(), 0.00001) << "area calculation doesn't match: ";

    const double     expSingleMass = 0.00005;
    const Coordinate singleCG      = fins.getComponentCG();
    EXPECT_NEAR(expSingleMass, singleCG.weight, kEpsilon) << "Fin mass is wrong! ";
    EXPECT_NEAR(0.05, singleCG.x, kEpsilon) << "Centroid x coordinate is wrong! ";
    EXPECT_NEAR(0.15, singleCG.y, kEpsilon) << "Centroid y coordinate is wrong! ";

    // should still return a single-fin-wetted area
    EXPECT_NEAR(0.00005, fins.getComponentVolume(), 0.0000001);

    // test instancing code: this should also trigger a recalculation
    fins.setFinCount(2);

    // should still return a single-fin-planform area
    EXPECT_NEAR(0.01, fins.getPlanformArea(), 0.00001);

    const Coordinate doubleCG      = fins.getComponentCG();
    const double     expDoubleMass = expSingleMass * 2;
    EXPECT_NEAR(expDoubleMass, doubleCG.weight, kEpsilon)
        << "Fin x2 mass does not change from single fin instance! ";
    EXPECT_NEAR(0.05, doubleCG.x, kEpsilon);
    EXPECT_NEAR(0.0, doubleCG.y, kEpsilon);
}

TEST_F(SimpleTrapezoidalFin, CGCalculationsFinWithTab)
{
    FinSet& fins = *m_fins;

    fins.setTabLength(0.02);
    fins.setTabHeight(0.02);
    fins.setTabOffsetMethod(AxialMethod::MIDDLE);
    fins.setTabOffset(0.0);

    EXPECT_NEAR(0.0020, fins.getPlanformArea(), kEpsilon) << "Wetted Area does not match!";

    const double expVol1 = 0.00001200;
    const double actVol1 = fins.getComponentVolume();
    EXPECT_NEAR(expVol1, actVol1, kEpsilon) << " fin volume is incorrect";

    const Coordinate actCentroid1 = fins.getCG();
    EXPECT_NEAR(0.03000, actCentroid1.x, kEpsilon) << " basic centroid x doesn't match: ";
    EXPECT_NEAR(0.11569444, actCentroid1.y, kEpsilon) << " basic centroid y doesn't match: ";

    fins.setFinCount(2);
    const double expVol2 = expVol1 * 2;
    const double actVol2 = fins.getComponentVolume();
    EXPECT_NEAR(expVol2, actVol2, kEpsilon) << " fin volume is incorrect";

    const Coordinate actCentroid2 = fins.getCG();
    // x coordinate will be the same....
    EXPECT_NEAR(0.0, actCentroid2.y, kEpsilon) << " basic centroid y doesn't match: ";
}

TEST_F(SimpleTrapezoidalFin, FilletCalculations)
{
    FinSet& fins = *m_fins;

    fins.setFilletRadius(0.005);
    fins.setFilletMaterial(
        Material::newMaterial(Material::Type::BULK, "Fillet-Test-Material", 1.0, true));

    // used for fillet and edge calculations:
    //
    //      [1] +--+ [2]
    //         /    \ .
    //        /      \ .
    //   [0] +--------+ [3]
    //
    EXPECT_NEAR(0.06, fins.getLength(), kEpsilon);
    EXPECT_NEAR(0.1, m_body->getOuterRadius(), kEpsilon) << "Body radius doesn't match: ";

    const Coordinate actVolume = fins.calculateFilletVolumeCentroid();

    EXPECT_NEAR(5.973e-07, actVolume.weight, kEpsilon) << "Fin volume doesn't match: ";
    EXPECT_NEAR(0.03, actVolume.x, kEpsilon) << "Fin mass center.getX() doesn't match: ";
    EXPECT_NEAR(0.101, actVolume.y, kEpsilon) << "Fin mass center.getY() doesn't match: ";

    // and then, check that the fillet volume feeds into a correct overall CG:
    const Coordinate actCentroid = fins.getCG();
    EXPECT_NEAR(0.03000, actCentroid.x, kEpsilon) << "Complete centroid x doesn't match: ";
    EXPECT_NEAR(0.11971548, actCentroid.y, kEpsilon) << "Complete centroid y doesn't match: ";
}

TEST_F(FreeformFinOnTransition, FilletCalculationsOnTransition)
{
    FinSet& fins = *m_fins;

    fins.setFilletRadius(0.005);
    fins.setFilletMaterial(
        Material::newMaterial(Material::Type::BULK, "Fillet-Test-Material", 1.0, true));

    EXPECT_NEAR(0.05, fins.getLength(), kEpsilon);
    EXPECT_NEAR(0.1, m_transition->getForeRadius(), kEpsilon)
        << "Transition fore radius doesn't match: ";
    EXPECT_NEAR(0.3, m_transition->getAftRadius(), kEpsilon)
        << "Transition aft radius doesn't match: ";

    const Coordinate actVolume = fins.calculateFilletVolumeCentroid();

    EXPECT_NEAR(5.973e-07, actVolume.weight, kEpsilon) << "Fin volume doesn't match: ";
    EXPECT_NEAR(0.024393025, actVolume.x, kEpsilon) << "Fin mass center.getX() doesn't match: ";
    EXPECT_NEAR(0.190479957, actVolume.y, kEpsilon) << "Fin mass center.getY() doesn't match: ";
}

TEST(TrapezoidFinSet, TrapezoidCGComputation)
{
    {
        // This is a simple square fin with sides of 1.0.
        TrapezoidFinSet fins;
        fins.setFinCount(1);
        fins.setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);

        const Coordinate coords = fins.getCG();
        EXPECT_NEAR(1.0, fins.getPlanformArea(), 0.001);
        EXPECT_NEAR(0.5, coords.x, 0.001);
        EXPECT_NEAR(0.5, coords.y, 0.001);
    }
    {
        // This is a trapezoid.  Height 1, root 1, tip 1/2 no sweep.
        // It can be decomposed into a rectangle followed by a triangle
        //  +---+
        //  |    \ .
        //  |     \ .
        //  +------+
        TrapezoidFinSet fins;
        fins.setFinCount(1);
        fins.setFinShape(1.0, 0.5, 0.0, 1.0, 0.005);

        const Coordinate coords = fins.getCG();
        EXPECT_NEAR(0.75, fins.getPlanformArea(), 0.001);
        EXPECT_NEAR(0.3889, coords.x, 0.001);
        EXPECT_NEAR(0.4444, coords.y, 0.001);
    }
}

TEST_F(SimpleTrapezoidalFin, GetBodyPointsPhantomMount)
{
    // set mount to have zero-dimensions:
    BodyTube& mount = *m_body;
    mount.setLength(0.0);
    mount.setOuterRadius(0.0);
    EXPECT_NEAR(0, mount.getLength(), 0.00001);
    EXPECT_NEAR(0, mount.getOuterRadius(), 0.00001);
    EXPECT_NEAR(0, mount.getInnerRadius(), 0.00001);

    const std::vector<Coordinate> mountPoints = m_fins->getMountPoints();

    ASSERT_EQ(2U, mountPoints.size());
    EXPECT_NEAR(0.00, mountPoints[0].x, 0.00001);
    EXPECT_NEAR(0.00, mountPoints[0].y, 0.00001);
    EXPECT_NEAR(0.00, mountPoints[1].x, 0.00001);
    EXPECT_NEAR(0.00, mountPoints[1].y, 0.00001);
}

TEST_F(SimpleTrapezoidalFin, GetBodyPointsZeroLengthMount)
{
    // set mount to have zero-dimensions:
    BodyTube& mount = *m_body;
    mount.setLength(0.0);
    mount.setOuterRadius(0.1);
    mount.setInnerRadius(0.08);
    EXPECT_NEAR(0, mount.getLength(), 0.00001);
    EXPECT_NEAR(0.1, mount.getOuterRadius(), 0.00001);
    EXPECT_NEAR(0.08, mount.getInnerRadius(), 0.00001);

    const std::vector<Coordinate> mountPoints = m_fins->getMountPoints();

    ASSERT_EQ(2U, mountPoints.size());
    EXPECT_NEAR(0.0, mountPoints[0].x, 0.00001);
    EXPECT_NEAR(0.1, mountPoints[0].y, 0.00001);
    EXPECT_NEAR(0.0, mountPoints[1].x, 0.00001);
    EXPECT_NEAR(0.1, mountPoints[1].y, 0.00001);
}

TEST_F(SimpleTrapezoidalFin, TrapezoidCGComputationPhantomMount)
{
    // set mount to have zero-dimensions:
    BodyTube& mount = *m_body;
    mount.setLength(0.0);
    mount.setOuterRadius(0.0);

    EXPECT_NEAR(0, mount.getLength(), 0.00001);
    EXPECT_NEAR(0, mount.getOuterRadius(), 0.00001);
    EXPECT_NEAR(0, mount.getInnerRadius(), 0.00001);

    const TrapezoidFinSet& fins = *m_fins;

    EXPECT_NEAR(0.06, fins.getLength(), 0.00001);
    EXPECT_NEAR(0.05, fins.getHeight(), 0.00001);
    EXPECT_NEAR(0.06, fins.getRootChord(), 0.00001);
    EXPECT_NEAR(0.02, fins.getTipChord(), 0.00001);

    const Coordinate coords = fins.getCG();
    EXPECT_NEAR(0.002, fins.getPlanformArea(), 0.001);
    EXPECT_NEAR(0.03, coords.x, 0.001);
    EXPECT_NEAR(0.02, coords.y, 0.001);
}

TEST(TrapezoidFinSet, InstancePointsPi2BaseRotation)
{
    // This is a simple square fin with sides of 1.0.
    auto fins = std::make_unique<TrapezoidFinSet>();
    fins->setFinCount(4);
    fins->setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    fins->setBaseRotation(std::numbers::pi / 2);

    BodyTube      body(1.0, 0.05);
    const FinSet& attached = body.addChild(std::move(fins));

    const std::vector<Coordinate> points = attached.getInstanceOffsets();

    ASSERT_EQ(points.size(), 4U);
    EXPECT_NEAR(0, points[0].x, 0.00001);
    EXPECT_NEAR(0, points[0].y, 0.00001);
    EXPECT_NEAR(0.05, points[0].z, 0.00001);

    EXPECT_NEAR(0, points[1].x, 0.00001);
    EXPECT_NEAR(-0.05, points[1].y, 0.00001);
    EXPECT_NEAR(0, points[1].z, 0.00001);
}

TEST(TrapezoidFinSet, InstancePointsPi4BaseRotation)
{
    // This is a simple square fin with sides of 1.0.
    auto fins = std::make_unique<TrapezoidFinSet>();
    fins->setFinCount(4);
    fins->setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    fins->setBaseRotation(std::numbers::pi / 4);

    BodyTube      body(1.0, 0.05);
    const FinSet& attached = body.addChild(std::move(fins));

    const std::vector<Coordinate> points = attached.getInstanceOffsets();

    ASSERT_EQ(points.size(), 4U);
    EXPECT_NEAR(0, points[0].x, 0.0001);
    EXPECT_NEAR(0.03535, points[0].y, 0.0001);
    EXPECT_NEAR(0.03535, points[0].z, 0.0001);

    EXPECT_NEAR(0, points[1].x, 0.0001);
    EXPECT_NEAR(-0.03535, points[1].y, 0.0001);
    EXPECT_NEAR(0.03535, points[1].z, 0.0001);
}

TEST(TrapezoidFinSet, InstanceAnglesZeroBaseRotation)
{
    // This is a simple square fin with sides of 1.0.
    TrapezoidFinSet fins;
    fins.setFinCount(4);
    fins.setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    fins.setBaseRotation(0.0);

    const std::vector<double> angles = fins.getInstanceAngles();

    ASSERT_EQ(angles.size(), 4U);
    EXPECT_NEAR(angles[0], 0, 0.000001);
    EXPECT_NEAR(angles[1], std::numbers::pi / 2, 0.000001);
    EXPECT_NEAR(angles[2], std::numbers::pi, 0.000001);
    EXPECT_NEAR(angles[3], 1.5 * std::numbers::pi, 0.000001);
}

TEST(TrapezoidFinSet, InstanceAngles90BaseRotation)
{
    // This is a simple square fin with sides of 1.0.
    TrapezoidFinSet fins;
    fins.setFinCount(4);
    fins.setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    fins.setBaseRotation(std::numbers::pi / 2);

    const std::vector<double> angles = fins.getInstanceAngles();

    ASSERT_EQ(angles.size(), 4U);
    EXPECT_NEAR(angles[0], std::numbers::pi / 2, 0.000001);
    EXPECT_NEAR(angles[1], std::numbers::pi, 0.000001);
    EXPECT_NEAR(angles[2], 1.5 * std::numbers::pi, 0.000001);
    EXPECT_NEAR(angles[3], 0, 0.000001);
}

// ============================================================ beyond OpenRocket's own tests

TEST(TrapezoidFinSet, Defaults)
{
    const TrapezoidFinSet fins;
    EXPECT_EQ(fins.kind(), ComponentKind::TRAPEZOID_FIN_SET);
    EXPECT_EQ(fins.getComponentName(), "Trapezoidal Fin Set");
    EXPECT_EQ(fins.getRootChord(), 0.05);
    EXPECT_EQ(fins.getLength(), 0.05);
    EXPECT_EQ(fins.getTipChord(), 0.05);
    EXPECT_EQ(fins.getSweep(), 0.025);
    EXPECT_EQ(fins.getHeight(), 0.03);
    EXPECT_EQ(fins.getSpan(), 0.03);
    // Pinned with OpenRocket (ProbeFins S9).
    EXPECT_NEAR(fins.getSweepAngle(), 0.6947382761967033, 1e-15);

    const std::vector<Coordinate> points = fins.getFinPoints();
    ASSERT_EQ(points.size(), 4U);
    EXPECT_TRUE(points[0].exactlyEquals(Coordinate{0.0, 0.0}));
    EXPECT_TRUE(points[1].exactlyEquals(Coordinate{0.025, 0.03}));
    EXPECT_TRUE(points[2].exactlyEquals(Coordinate{0.07500000000000001, 0.03}));
    EXPECT_TRUE(points[3].exactlyEquals(Coordinate{0.05, 0.0}));

    EXPECT_EQ(TrapezoidFinSet::kMaxSweepAngle, 89 * std::numbers::pi / 180.0);
}

TEST(TrapezoidFinSet, ConstructorLimitsTheFinCountOnly)
{
    const TrapezoidFinSet many(12, 0.1, 0.2, -0.3, 0.4);
    EXPECT_EQ(many.getFinCount(), 8);
    EXPECT_EQ(many.getRootChord(), 0.1);
    EXPECT_EQ(many.getTipChord(), 0.2);
    EXPECT_EQ(many.getSweep(), -0.3);
    EXPECT_EQ(many.getHeight(), 0.4);

    const TrapezoidFinSet none(0, 0.1, 0.2, 0.3, 0.4);
    EXPECT_EQ(none.getFinCount(), 1);

    // The dimensions are stored as given, as in OpenRocket.
    const TrapezoidFinSet negative(2, -0.1, -0.2, 0.0, -0.4);
    EXPECT_EQ(negative.getRootChord(), -0.1);
    EXPECT_EQ(negative.getTipChord(), -0.2);
    EXPECT_EQ(negative.getHeight(), -0.4);
}

TEST_F(SimpleTrapezoidalFin, SettersLimitAndFire)
{
    const int both = ComponentChangeEvent::kBothChange;
    recordEvents();

    m_fins->setRootChord(0.06);
    m_fins->setTipChord(0.02);
    m_fins->setSweep(0.02);
    m_fins->setHeight(0.05);
    EXPECT_TRUE(takeEvents().empty()) << "the current values";

    m_fins->setRootChord(0.08);
    EXPECT_EQ(m_fins->getRootChord(), 0.08);
    EXPECT_EQ(m_fins->getLength(), 0.08);
    m_fins->setTipChord(0.03);
    EXPECT_EQ(m_fins->getTipChord(), 0.03);
    m_fins->setSweep(-0.01);
    EXPECT_EQ(m_fins->getSweep(), -0.01);
    m_fins->setHeight(0.07);
    EXPECT_EQ(m_fins->getHeight(), 0.07);
    EXPECT_EQ(m_fins->getSpan(), 0.07);
    EXPECT_EQ(takeEvents(), (std::vector<int>{both, both, both, both}));

    // Negative values become 0 (the sweep may be negative).
    m_fins->setRootChord(-1);
    EXPECT_EQ(m_fins->getRootChord(), 0.0);
    m_fins->setTipChord(-1);
    EXPECT_EQ(m_fins->getTipChord(), 0.0);
    m_fins->setHeight(-1);
    EXPECT_EQ(m_fins->getHeight(), 0.0);
    EXPECT_EQ(takeEvents(), (std::vector<int>{both, both, both}));

    // Java's Math.max(NaN, 0) is NaN.
    m_fins->setTipChord(kNaN);
    EXPECT_TRUE(std::isnan(m_fins->getTipChord()));
}

TEST_F(SimpleTrapezoidalFin, RootChordMovesTheTab)
{
    m_fins->setTabLength(0.02);  // MIDDLE, offset 0
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.02, 1e-16);
    m_fins->setRootChord(0.1);
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.04, 1e-16);
    EXPECT_NEAR(m_fins->getTabOffset(), 0.0, 1e-16);

    // setFinShape() does not: the tab stays where it was, and its offset changes.
    m_fins->setFinShape(0.06, 0.02, 0.02, 0.05, 0.005);
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.04, 1e-16);
    EXPECT_NEAR(m_fins->getTabOffset(), 0.02, 1e-16);
}

TEST_F(SimpleTrapezoidalFin, SetFinShapeStoresEverythingAsGiven)
{
    recordEvents();
    m_fins->setFinShape(0.06, 0.02, 0.02, 0.05, 0.005);
    EXPECT_TRUE(takeEvents().empty()) << "nothing changed";

    m_fins->setFinShape(0.06, 0.02, 0.02, 0.05, 0.004);
    EXPECT_EQ(m_fins->getThickness(), 0.004);
    EXPECT_EQ(takeEvents(), std::vector<int>{ComponentChangeEvent::kBothChange});

    m_fins->setFinShape(-0.1, -0.2, -0.3, -0.4, -0.5);
    EXPECT_EQ(m_fins->getRootChord(), -0.1);
    EXPECT_EQ(m_fins->getTipChord(), -0.2);
    EXPECT_EQ(m_fins->getSweep(), -0.3);
    EXPECT_EQ(m_fins->getHeight(), -0.4);
    EXPECT_EQ(m_fins->getThickness(), -0.5);
}

TEST(TrapezoidFinSet, SweepAngle)
{
    // Pinned with OpenRocket (ProbeFins S9).
    TrapezoidFinSet flat(1, 0.05, 0.03, 0.02, 0.0);
    EXPECT_EQ(flat.getSweepAngle(), std::numbers::pi / 2);
    flat.setSweep(-0.02);
    EXPECT_EQ(flat.getSweepAngle(), -std::numbers::pi / 2);
    flat.setSweep(0);
    EXPECT_EQ(flat.getSweepAngle(), 0.0);
    // Without a height every angle gives no sweep.
    flat.setSweep(0.02);
    flat.setSweepAngle(0.5);
    EXPECT_EQ(flat.getSweep(), 0.0);

    TrapezoidFinSet fins;  // height 0.03
    fins.setSweepAngle(javaToRadians(45));
    EXPECT_NEAR(fins.getSweep(), 0.029999999999999995, 1e-15);
    EXPECT_NEAR(fins.getSweepAngle(), std::numbers::pi / 4, 1e-15);

    // Limited to 89 degrees either way.
    fins.setSweepAngle(2.0);
    EXPECT_NEAR(fins.getSweep(), 1.7186988489227961, 1e-12);
    EXPECT_NEAR(fins.getSweepAngle(), TrapezoidFinSet::kMaxSweepAngle, 1e-15);
    fins.setSweepAngle(-2.0);
    EXPECT_NEAR(fins.getSweep(), -1.7186988489227961, 1e-12);

    // A sweep that would be NaN or infinite is not set.
    fins.setSweepAngle(kNaN);
    EXPECT_NEAR(fins.getSweep(), -1.7186988489227961, 1e-12);
    fins.setHeight(std::numeric_limits<double>::infinity());
    fins.setSweepAngle(0.5);
    EXPECT_NEAR(fins.getSweep(), -1.7186988489227961, 1e-12);
}

TEST(TrapezoidFinSet, OutlineOfSmallChords)
{
    // A tip chord of 0.1 mm or less gives a triangle, and a root chord below 0.1 mm is drawn as
    // 0.1 mm (ProbeFins S9).
    const TrapezoidFinSet         tiny(2, 0.00005, 0.0001, 0.01, 0.02);
    const std::vector<Coordinate> points = tiny.getFinPoints();
    ASSERT_EQ(points.size(), 3U);
    EXPECT_TRUE(points[0].exactlyEquals(Coordinate{0.0, 0.0}));
    EXPECT_TRUE(points[1].exactlyEquals(Coordinate{0.01, 0.02}));
    EXPECT_TRUE(points[2].exactlyEquals(Coordinate{1.0E-4, 0.0}));

    const TrapezoidFinSet justOver(2, 0.05, 0.00010001, 0.01, 0.02);
    EXPECT_EQ(justOver.getFinPoints().size(), 4U);
}

TEST_F(SimpleTrapezoidalFin, CopyKeepsTheDimensions)
{
    m_fins->setFinShape(0.07, 0.03, 0.01, 0.04, 0.002);
    const std::unique_ptr<RocketComponent> copied = m_fins->copyWithOriginalId();
    const auto* copy = dynamic_cast<const TrapezoidFinSet*>(copied.get());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getRootChord(), 0.07);
    EXPECT_EQ(copy->getTipChord(), 0.03);
    EXPECT_EQ(copy->getSweep(), 0.01);
    EXPECT_EQ(copy->getHeight(), 0.04);
    EXPECT_EQ(copy->getThickness(), 0.002);
    EXPECT_EQ(copy->getFinCount(), 1);
    EXPECT_EQ(copy->getId(), m_fins->getId());
}

}  // namespace
