#include "QtRocket/rocket/EllipticalFinSet.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TransitionShape;

/// The tolerance for the values pinned with OpenRocket (they pass through sin and cos, whose
/// last bit differs between math libraries).
constexpr double kPinned = 1e-14;

/// @p actual within @p tolerance of @p expected in x, y, z and the weight.
void expectNear(const Coordinate& expected, const Coordinate& actual, double tolerance,
                std::string_view what)
{
    EXPECT_NEAR(expected.x, actual.x, tolerance) << what << " x";
    EXPECT_NEAR(expected.y, actual.y, tolerance) << what << " y";
    EXPECT_NEAR(expected.z, actual.z, tolerance) << what << " z";
    EXPECT_NEAR(expected.weight, actual.weight, tolerance) << what << " weight";
}

/// How far @p points are from the half ellipse over a root of @p length with the height
/// @p height: the largest |u^2 + v^2 - 1| (u and v the point in the ellipse's own units), or of
/// |z|.
[[nodiscard]] double largestDeviationFromEllipse(const std::vector<Coordinate>& points,
                                                 double length, double height)
{
    double largest = 0;
    for (const Coordinate& point : points)
    {
        const double u = (point.x - (length / 2)) / (length / 2);
        const double v = point.y / height;
        largest        = std::max({largest, std::abs((u * u) + (v * v) - 1.0), std::abs(point.z)});
    }
    return largest;
}

/// A rocket with one stage, an ogive nose cone (0.07 m, radius 0.012 m) and a body tube (0.2 m,
/// radius 0.012 m) carrying a default elliptical fin set; events enabled and recorded.
class EllipticalFinsOnTube : public ::testing::Test
{
protected:
    EllipticalFinsOnTube()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body = &stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
        m_fins = &m_body->addChild(std::make_unique<EllipticalFinSet>());
        m_rocket.enableEvents();
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
    EllipticalFinSet*                       m_fins{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(EllipticalFinSet, Defaults)
{
    const EllipticalFinSet fins;
    EXPECT_EQ(fins.kind(), ComponentKind::ELLIPTICAL_FIN_SET);
    EXPECT_EQ(fins.getComponentName(), "Elliptical Fin Set");
    EXPECT_EQ(fins.getName(), "Elliptical Fin Set");
    EXPECT_EQ(fins.getFinCount(), 3);
    EXPECT_EQ(fins.getLength(), 0.05);
    EXPECT_EQ(fins.getHeight(), 0.05);
    EXPECT_EQ(fins.getSpan(), 0.05);
    EXPECT_EQ(fins.getThickness(), 0.003);
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(EllipticalFinSet::kPoints, 31);
}

TEST(EllipticalFinSet, OutlineIsHalfAnEllipse)
{
    const EllipticalFinSet        fins;
    const std::vector<Coordinate> points = fins.getFinPoints();
    ASSERT_EQ(points.size(), 31U);

    // The ends are exact.
    EXPECT_TRUE(points.front().exactlyEquals(Coordinate{0.0, 0.0}));
    EXPECT_TRUE(points.back().exactlyEquals(Coordinate{0.05, 0.0}));

    // Pinned with OpenRocket (ProbeFins S4).
    expectNear(Coordinate{1.36952615793165E-4, 0.005226423163382665}, points[1], kPinned, "1");
    expectNear(Coordinate{5.463099816548578E-4, 0.010395584540887966}, points[2], kPinned, "2");
    expectNear(Coordinate{0.012500000000000006, 0.04330127018922194}, points[10], kPinned, "10");
    expectNear(Coordinate{0.02500000000000001, 0.05}, points[15], kPinned, "15");
    expectNear(Coordinate{0.037500000000000006, 0.04330127018922193}, points[20], kPinned, "20");
    expectNear(Coordinate{0.049863047384206834, 0.005226423163382673}, points[29], kPinned, "29");

    // Every point lies on the ellipse, and the x values run aft.
    EXPECT_LT(largestDeviationFromEllipse(points, 0.05, 0.05), 1e-12);
    EXPECT_TRUE(std::ranges::is_sorted(points, std::ranges::less{}, &Coordinate::x));

    EXPECT_NEAR(fins.getPlanformArea(), 0.001959908686268503, 1e-15);
    expectNear(Coordinate{0.025, 0.0, 0.0, 0.011994641159963241}, fins.getComponentCG(), 1e-14,
               "CG");
}

// Pinned with OpenRocket (ProbeFins S4): 0.06 m long and 0.04 m high at the end of the tube.
TEST_F(EllipticalFinsOnTube, GeometryAndMassMatchOpenRocket)
{
    m_fins->setHeight(0.04);
    m_fins->setLength(0.06);
    const EllipticalFinSet& fins = *m_fins;

    EXPECT_EQ(fins.getLength(), 0.06);
    EXPECT_EQ(fins.getSpan(), 0.04);
    expectNear(Coordinate{0.14, 0.012}, fins.getFinFront(), 1e-16, "fin front");
    EXPECT_NEAR(fins.getPlanformArea(), 0.0018815123388177624, 1e-15);
    EXPECT_NEAR(fins.getComponentVolume(), 1.6933611049359862E-5, 1e-17);
    EXPECT_NEAR(fins.getComponentMass(), 0.011514855513564706, 1e-14);
    expectNear(Coordinate{0.029999999999999995, 0.0, 0.0, 0.011514855513564706},
               fins.getComponentCG(), 1e-14, "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 7.287460774325026E-4, 1e-15);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 9.871140701605645E-4, 1e-15);

    const std::vector<Coordinate> points = fins.getFinPoints();
    ASSERT_EQ(points.size(), 31U);
    expectNear(Coordinate{0.0, 0.0}, points[0], kPinned, "0");
    expectNear(Coordinate{1.6434313895179797E-4, 0.004181138530706132}, points[1], kPinned, "1");
    expectNear(Coordinate{0.030000000000000006, 0.04}, points[15], kPinned, "15");
    expectNear(Coordinate{0.0598356568610482, 0.004181138530706139}, points[29], kPinned, "29");
    expectNear(Coordinate{0.06, 0.0}, points[30], kPinned, "30");

    const std::vector<Coordinate> bounds = fins.getComponentBounds();
    ASSERT_EQ(bounds.size(), 2U);
    expectNear(Coordinate{0.0, -0.052000000000000005, -0.052000000000000005}, bounds[0], kPinned,
               "bound 0");
    expectNear(Coordinate{0.27, 0.052000000000000005, 0.052000000000000005}, bounds[1], kPinned,
               "bound 1");
    const BoundingBox box = fins.getInstanceBoundingBox();
    expectNear(Coordinate{0.0, 0.0, -0.0015}, box.min(), kPinned, "box min");
    expectNear(Coordinate{0.06, 0.04, 0.0015}, box.max(), kPinned, "box max");

    // Kept from OpenRocket: setLength() does not move the tab, so its offset changes.
    EXPECT_EQ(fins.getTabFrontEdge(), 0.0);
    EXPECT_NEAR(fins.getTabOffset(), -0.0049999999999999975, 1e-17);
}

TEST_F(EllipticalFinsOnTube, SettersFireUnlessEqual)
{
    const int both = ComponentChangeEvent::kBothChange;

    m_fins->setHeight(0.05);
    m_fins->setLength(0.05);
    m_fins->setHeight(0.05 + 1e-11);
    m_fins->setLength(0.05 - 1e-11);
    EXPECT_TRUE(takeEvents().empty()) << "equal within MathUtil's tolerance";
    EXPECT_EQ(m_fins->getHeight(), 0.05);
    EXPECT_EQ(m_fins->getLength(), 0.05);

    m_fins->setHeight(0.04);
    m_fins->setLength(0.06);
    EXPECT_EQ(takeEvents(), (std::vector<int>{both, both}));

    // Kept from OpenRocket: negative values are stored as they are.
    m_fins->setHeight(-0.01);
    EXPECT_EQ(m_fins->getHeight(), -0.01);
    EXPECT_EQ(m_fins->getSpan(), -0.01);
    m_fins->setLength(-0.02);
    EXPECT_EQ(m_fins->getLength(), -0.02);
}

// Pinned with OpenRocket (ProbeFins S4).
TEST_F(EllipticalFinsOnTube, SetLengthShortensATabThatEndsBehindTheFin)
{
    m_fins->setHeight(0.04);
    m_fins->setLength(0.06);
    m_fins->setTabHeight(0.003);
    m_fins->setTabLength(0.05);
    m_fins->setTabOffsetMethod(AxialMethod::BOTTOM);
    m_fins->setTabOffset(0.0);
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.009999999999999995, 1e-17);

    m_fins->setLength(0.03);
    EXPECT_NEAR(m_fins->getTabLength(), 0.020000000000000004, 1e-17);
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.009999999999999995, 1e-17) << "the tab stays";

    m_fins->setLength(-0.02);
    EXPECT_EQ(m_fins->getTabLength(), 0.0);
}

TEST(EllipticalFinSet, AVeryShortFinIsDrawnAtATenthOfAMillimetre)
{
    EllipticalFinSet fins;
    fins.setLength(0.0);
    const std::vector<Coordinate> points = fins.getFinPoints();
    ASSERT_EQ(points.size(), 31U);
    EXPECT_TRUE(points.back().exactlyEquals(Coordinate{0.0001, 0.0}));
    EXPECT_NEAR(points[15].x, 0.00005, 1e-18);
    EXPECT_NEAR(points[15].y, 0.05, 1e-16);
}

TEST_F(EllipticalFinsOnTube, CopyKeepsTheDimensions)
{
    m_fins->setHeight(0.04);
    m_fins->setLength(0.06);
    const std::unique_ptr<RocketComponent> copied = m_fins->copyWithOriginalId();
    const auto* copy = dynamic_cast<const EllipticalFinSet*>(copied.get());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getHeight(), 0.04);
    EXPECT_EQ(copy->getLength(), 0.06);
    EXPECT_EQ(copy->getId(), m_fins->getId());
    EXPECT_EQ(copy->getParent(), nullptr);

    // A transition takes freeform fin sets only.
    EXPECT_FALSE(QtRocket::Transition{}.isCompatible(ComponentKind::ELLIPTICAL_FIN_SET));
}

}  // namespace
