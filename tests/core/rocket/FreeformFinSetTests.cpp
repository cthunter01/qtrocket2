#include "QtRocket/rocket/FreeformFinSet.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/AxialOffsetSupport.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::Appearance;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::ErrorCode;
using QtRocket::Finish;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::LineStyle;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Point2D;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::MathUtil::javaToRadians;
using QtRocket::Test::setAxialOffset;

/// FreeformFinSetTest's tolerance.
constexpr double kEpsilon = 1.0E-6;

/// Java's float literals 0.1f and 0.3f as the doubles they are passed as.
constexpr double kTenthF       = static_cast<double>(0.1F);
constexpr double kThreeTenthsF = static_cast<double>(0.3F);

/// Indices far beyond every point list: the largest index, and what Java's negative ints (-1 for
/// "no point", -2, Integer.MIN_VALUE) become as a std::size_t.
constexpr std::array<std::size_t, 4> kIndicesFromNegatives{
    std::numeric_limits<std::size_t>::max(), static_cast<std::size_t>(-1),
    static_cast<std::size_t>(-2), static_cast<std::size_t>(std::numeric_limits<int>::min())};

/// @p result is the failure OpenRocket's IllegalFinPointException(@p message) becomes.
void expectIllegalFinPoint(const Result<void>& result, std::string_view message, std::size_t index)
{
    ASSERT_FALSE(result.has_value()) << "index " << index;
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT) << "index " << index;
    EXPECT_EQ(result.error().message, message) << "index " << index;
}

/// @p actual has the (x, y) points @p expected, each within @p tolerance.
void expectPointsNear(const std::vector<Coordinate>& expected,
                      const std::vector<Coordinate>& actual, double tolerance,
                      std::string_view what)
{
    ASSERT_EQ(expected.size(), actual.size()) << what;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(expected[i].x, actual[i].x, tolerance) << what << " point " << i << " x";
        EXPECT_NEAR(expected[i].y, actual[i].y, tolerance) << what << " point " << i << " y";
    }
}

/// FreeformFinSetTest.createTemplateRocket(): a stage with an ellipsoid nose fairing (1 m long,
/// base radius 1 m), a body tube (2 m, radius 1 m, automatic), a conical tail cone (1 m, radius
/// 1 m to 0.5 m) and a phantom body tube of no length (radius 0.5 m); events enabled.
class TemplateRocket : public ::testing::Test
{
protected:
    TemplateRocket()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());

        auto nose = std::make_unique<NoseCone>();
        nose->setForeRadius(0.0);
        nose->setLength(1.0);
        nose->setAftRadius(1.0);
        nose->setShapeType(TransitionShape::ELLIPSOID);
        nose->setShapeParameter(0.5);
        nose->setName("Nose Fairing");
        m_nose = &stage.addChild(std::move(nose));

        auto body = std::make_unique<BodyTube>(2.0, 1.0, 0.01);
        body->setName("Body Tube");
        m_body = &stage.addChild(std::move(body));

        auto tail = std::make_unique<Transition>();
        tail->setShapeType(TransitionShape::CONICAL);
        tail->setForeRadius(1.0);
        tail->setLength(1.0);
        tail->setAftRadius(0.5);
        // slope = .5/1.0 = 0.5
        tail->setName("Tail Cone");
        m_tailCone = &stage.addChild(std::move(tail));

        // zero-length body tube -- triggers a whole other class of errors
        auto phantom = std::make_unique<BodyTube>(0.0, 0.5, 0.01);
        phantom->setName("Phantom Body Tube");
        // (As in the Java test, it is the body tube whose radius becomes automatic.)
        m_body->setOuterRadiusAutomatic(true);
        m_phantomBody = &stage.addChild(std::move(phantom));

        m_rocket.enableEvents();
    }

    /// createFinOnEllipsoidNose(): one fin 0.02 m behind the nose's tip whose last point snaps
    /// to the body.
    FreeformFinSet& createFinOnEllipsoidNose()
    {
        auto fins = std::make_unique<FreeformFinSet>();
        fins->setName("test-freeform-finset");
        fins->setFinCount(1);
        setAxialOffset(*fins, AxialMethod::TOP, 0.02);
        const std::vector<Coordinate> points{
            Coordinate{0.0, 0.0}, Coordinate{0.4, 1.0}, Coordinate{0.6, 1.0}, Coordinate{0.8, 0.788}
            // y-value should be automatically adjusted to snap to body
        };
        fins->setPoints(points);
        return m_nose->addChild(std::move(fins));
    }

    /// createFinOnTube(): one fin at the end of the body tube, a trapezoid of height 1, root
    /// chord 1, tip chord 1/2 and sweep 1/2:
    ///     +--+
    ///    /.  |
    ///   / .  |
    ///  +=====+
    FreeformFinSet& createFinOnTube()
    {
        auto fins = std::make_unique<FreeformFinSet>();
        fins->setName("TubeBodyFins");
        fins->setFinCount(1);
        const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.5, 1}, Coordinate{1, 1},
                                             Coordinate{1, 0}};
        fins->setPoints(points);
        setAxialOffset(*fins, AxialMethod::BOTTOM, 0.0);

        return m_body->addChild(std::move(fins));
    }

    /// createFinOnConicalTransition(): one triangular fin 0.4 m behind the front of the tail
    /// cone:
    ///            ----+ (1)
    ///  (0)  -----    |
    ///  ---+          |
    ///       -----    |
    ///            ----+ (2)
    FreeformFinSet& createFinOnConicalTransition()
    {
        auto fins = std::make_unique<FreeformFinSet>();
        fins->setName("test-freeform-finset");
        fins->setFinCount(1);
        fins->setThickness(0.005);
        setAxialOffset(*fins, AxialMethod::TOP, 0.4);
        const std::vector<Coordinate> initPoints{Coordinate{0.0, 0.0}, Coordinate{0.4, 0.2},
                                                 Coordinate{0.4, -0.2}};
        fins->setPoints(initPoints);

        return m_tailCone->addChild(std::move(fins));
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
    NoseCone*                               m_nose{nullptr};
    BodyTube*                               m_body{nullptr};
    Transition*                             m_tailCone{nullptr};
    BodyTube*                               m_phantomBody{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// FreeformFinSetTest.testFreeformConvert(): gives @p sourceSet a value for every property the
/// conversion carries over.
void prepareForConversion(FinSet& sourceSet)
{
    sourceSet.setName("test-convert-finset");
    sourceSet.setBaseRotation(1.1);
    sourceSet.setCantAngle(0.001);
    sourceSet.setCGOverridden(true);
    sourceSet.setColor(Color::black());
    sourceSet.setComment("cmt");
    sourceSet.setCrossSection(FinSet::CrossSection::ROUNDED);
    sourceSet.setFinCount(5);

    if (auto* elliptical = dynamic_cast<EllipticalFinSet*>(&sourceSet))
    {
        elliptical->setLength(0.1);
    }
    else if (auto* trapezoid = dynamic_cast<TrapezoidFinSet*>(&sourceSet))
    {
        trapezoid->setRootChord(0.1);
    }

    sourceSet.setFinish(Finish::ROUGH);
    sourceSet.setLineStyle(LineStyle::DASHDOT);
    sourceSet.setMassOverridden(true);
    sourceSet.setMaterial(Material::newMaterial(Material::Type::BULK, "test-material", 0.1, true));
    sourceSet.setOverrideCGX(0.012);
    sourceSet.setOverrideMass(0.0123);
    sourceSet.setSubcomponentsOverriddenMass(true);
    sourceSet.setSubcomponentsOverriddenCG(true);
    sourceSet.setAxialMethod(AxialMethod::ABSOLUTE);
    sourceSet.setAxialOffset(0.1);
    sourceSet.setTabHeight(0.01);
    sourceSet.setTabLength(0.02);
    sourceSet.setTabOffsetMethod(AxialMethod::BOTTOM);
    sourceSet.setTabOffset(-0.015);
    sourceSet.setThickness(0.005);
}

// ======================================================================= FreeformFinSetTest

TEST(FreeformFinSet, ConvertEllipticalToFreeform)
{
    EllipticalFinSet source;
    prepareForConversion(source);
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(source);
    ASSERT_NE(result.freeform, nullptr);
    const FreeformFinSet& finSet = *result.freeform;

    EXPECT_EQ(finSet.getName(), "test-convert-finset");

    EXPECT_NEAR(1.1, finSet.getBaseRotation(), kEpsilon);
    EXPECT_NEAR(0.001, finSet.getCantAngle(), kEpsilon);
    EXPECT_TRUE(finSet.isCGOverridden());
    EXPECT_TRUE(finSet.isMassOverridden());
    EXPECT_EQ(Color::black(), finSet.getColor());
    EXPECT_EQ(finSet.getComment(), "cmt");
    EXPECT_EQ(FinSet::CrossSection::ROUNDED, finSet.getCrossSection());
    EXPECT_EQ(5, finSet.getFinCount());
    EXPECT_EQ(Finish::ROUGH, finSet.getFinish());
    EXPECT_EQ(LineStyle::DASHDOT, finSet.getLineStyle());

    const Material& mat = finSet.getMaterial();
    EXPECT_EQ(Material::Type::BULK, mat.getType());
    EXPECT_EQ(mat.getName(), "test-material");
    EXPECT_NEAR(0.1, mat.getDensity(), kEpsilon);
    EXPECT_TRUE(mat.isUserDefined());

    EXPECT_NEAR(0.012, finSet.getOverrideCGX(), kEpsilon);
    EXPECT_NEAR(0.0123, finSet.getOverrideMass(), kEpsilon);
    EXPECT_TRUE(finSet.isSubcomponentsOverriddenMass());
    EXPECT_TRUE(finSet.isSubcomponentsOverriddenCG());

    EXPECT_EQ(AxialMethod::ABSOLUTE, finSet.getAxialMethod());
    EXPECT_NEAR(0.1, finSet.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.01, finSet.getTabHeight(), kEpsilon);
    EXPECT_NEAR(0.02, finSet.getTabLength(), kEpsilon);
    EXPECT_EQ(AxialMethod::BOTTOM, finSet.getTabOffsetMethod());
    EXPECT_NEAR(-0.015, finSet.getTabOffset(), kEpsilon);
    EXPECT_NEAR(0.005, finSet.getThickness(), kEpsilon);

    // A detached fin set: the caller owns the new one and keeps the old one.
    EXPECT_EQ(result.detached.get(), result.freeform);
    EXPECT_EQ(result.original, nullptr);
    EXPECT_EQ(result.freeform->getPointCount(), 31U);
}

TEST(FreeformFinSet, ConvertTrapezoidToFreeform)
{
    TrapezoidFinSet source;
    prepareForConversion(source);
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(source);
    ASSERT_NE(result.freeform, nullptr);
    const FreeformFinSet& finSet = *result.freeform;

    EXPECT_EQ(finSet.getName(), "test-convert-finset");

    EXPECT_NEAR(1.1, finSet.getBaseRotation(), kEpsilon);
    EXPECT_NEAR(0.001, finSet.getCantAngle(), kEpsilon);
    EXPECT_TRUE(finSet.isCGOverridden());
    EXPECT_TRUE(finSet.isMassOverridden());
    EXPECT_EQ(Color::black(), finSet.getColor());
    EXPECT_EQ(finSet.getComment(), "cmt");
    EXPECT_EQ(FinSet::CrossSection::ROUNDED, finSet.getCrossSection());
    EXPECT_EQ(5, finSet.getFinCount());
    EXPECT_EQ(Finish::ROUGH, finSet.getFinish());
    EXPECT_EQ(LineStyle::DASHDOT, finSet.getLineStyle());

    const Material& mat = finSet.getMaterial();
    EXPECT_EQ(Material::Type::BULK, mat.getType());
    EXPECT_EQ(mat.getName(), "test-material");
    EXPECT_NEAR(0.1, mat.getDensity(), kEpsilon);
    EXPECT_TRUE(mat.isUserDefined());

    EXPECT_NEAR(0.012, finSet.getOverrideCGX(), kEpsilon);
    EXPECT_NEAR(0.0123, finSet.getOverrideMass(), kEpsilon);
    EXPECT_TRUE(finSet.isSubcomponentsOverriddenMass());
    EXPECT_TRUE(finSet.isSubcomponentsOverriddenCG());

    EXPECT_EQ(AxialMethod::ABSOLUTE, finSet.getAxialMethod());
    EXPECT_NEAR(0.1, finSet.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.01, finSet.getTabHeight(), kEpsilon);
    EXPECT_NEAR(0.02, finSet.getTabLength(), kEpsilon);
    EXPECT_EQ(AxialMethod::BOTTOM, finSet.getTabOffsetMethod());
    EXPECT_NEAR(-0.015, finSet.getTabOffset(), kEpsilon);
    EXPECT_NEAR(0.005, finSet.getThickness(), kEpsilon);
    EXPECT_EQ(result.freeform->getPointCount(), 4U);
}

/// The root points of the conical-transition fin canted by 15 degrees (the Java test's table).
[[nodiscard]] std::vector<Coordinate> cantedRootPointsOnTailCone()
{
    return {Coordinate{0, -0.001683625, 0},     Coordinate{0.004, -0.003620824, 0},
            Coordinate{0.008, -0.005559077, 0}, Coordinate{0.012, -0.00749839, 0},
            Coordinate{0.016, -0.009438771, 0}, Coordinate{0.02, -0.011380227, 0},
            Coordinate{0.024, -0.013322767, 0}, Coordinate{0.028, -0.015266398, 0},
            Coordinate{0.032, -0.017211128, 0}, Coordinate{0.036, -0.019156966, 0},
            Coordinate{0.04, -0.021103918, 0},  Coordinate{0.044, -0.023051993, 0},
            Coordinate{0.048, -0.0250012, 0},   Coordinate{0.052, -0.026951546, 0},
            Coordinate{0.056, -0.028903041, 0}, Coordinate{0.06, -0.030855692, 0},
            Coordinate{0.064, -0.032809508, 0}, Coordinate{0.068, -0.034764497, 0},
            Coordinate{0.072, -0.036720669, 0}, Coordinate{0.076, -0.038678032, 0},
            Coordinate{0.08, -0.040636596, 0},  Coordinate{0.084, -0.042596368, 0},
            Coordinate{0.088, -0.044557359, 0}, Coordinate{0.092, -0.046519577, 0},
            Coordinate{0.096, -0.048483032, 0}, Coordinate{0.1, -0.050447733, 0},
            Coordinate{0.104, -0.052413689, 0}, Coordinate{0.108, -0.054380911, 0},
            Coordinate{0.112, -0.056349408, 0}, Coordinate{0.116, -0.05831919, 0},
            Coordinate{0.12, -0.060290266, 0},  Coordinate{0.124, -0.062262648, 0},
            Coordinate{0.128, -0.064236344, 0}, Coordinate{0.132, -0.066211365, 0},
            Coordinate{0.136, -0.068187722, 0}, Coordinate{0.14, -0.070165425, 0},
            Coordinate{0.144, -0.072144484, 0}, Coordinate{0.148, -0.074124911, 0},
            Coordinate{0.152, -0.076106716, 0}, Coordinate{0.156, -0.07808991, 0},
            Coordinate{0.16, -0.080074505, 0},  Coordinate{0.164, -0.082060511, 0},
            Coordinate{0.168, -0.08404794, 0},  Coordinate{0.172, -0.086036803, 0},
            Coordinate{0.176, -0.088027112, 0}, Coordinate{0.18, -0.090018879, 0},
            Coordinate{0.184, -0.092012115, 0}, Coordinate{0.188, -0.094006834, 0},
            Coordinate{0.192, -0.096003045, 0}, Coordinate{0.196, -0.098000763, 0},
            Coordinate{0.2, -0.1, 0},           Coordinate{0.204, -0.102000768, 0},
            Coordinate{0.208, -0.104003079, 0}, Coordinate{0.212, -0.106006948, 0},
            Coordinate{0.216, -0.108012386, 0}, Coordinate{0.22, -0.110019407, 0},
            Coordinate{0.224, -0.112028025, 0}, Coordinate{0.228, -0.114038253, 0},
            Coordinate{0.232, -0.116050104, 0}, Coordinate{0.236, -0.118063594, 0},
            Coordinate{0.24, -0.120078734, 0},  Coordinate{0.244, -0.122095541, 0},
            Coordinate{0.248, -0.124114028, 0}, Coordinate{0.252, -0.126134209, 0},
            Coordinate{0.256, -0.1281561, 0},   Coordinate{0.26, -0.130179716, 0},
            Coordinate{0.264, -0.132205071, 0}, Coordinate{0.268, -0.134232181, 0},
            Coordinate{0.272, -0.136261062, 0}, Coordinate{0.276, -0.138291728, 0},
            Coordinate{0.28, -0.140324197, 0},  Coordinate{0.284, -0.142358484, 0},
            Coordinate{0.288, -0.144394605, 0}, Coordinate{0.292, -0.146432578, 0},
            Coordinate{0.296, -0.148472418, 0}, Coordinate{0.3, -0.150514143, 0},
            Coordinate{0.304, -0.152557769, 0}, Coordinate{0.308, -0.154603316, 0},
            Coordinate{0.312, -0.156650799, 0}, Coordinate{0.316, -0.158700236, 0},
            Coordinate{0.32, -0.160751647, 0},  Coordinate{0.324, -0.16280505, 0},
            Coordinate{0.328, -0.164860462, 0}, Coordinate{0.332, -0.166917903, 0},
            Coordinate{0.336, -0.168977392, 0}, Coordinate{0.34, -0.171038948, 0},
            Coordinate{0.344, -0.173102591, 0}, Coordinate{0.348, -0.175168341, 0},
            Coordinate{0.352, -0.177236218, 0}, Coordinate{0.356, -0.179306243, 0},
            Coordinate{0.36, -0.181378435, 0},  Coordinate{0.364, -0.183452818, 0},
            Coordinate{0.368, -0.18552941, 0},  Coordinate{0.372, -0.187608235, 0},
            Coordinate{0.376, -0.189689315, 0}, Coordinate{0.38, -0.191772671, 0},
            Coordinate{0.384, -0.193858326, 0}, Coordinate{0.388, -0.195946303, 0},
            Coordinate{0.392, -0.198036625, 0}, Coordinate{0.396, -0.200129317, 0},
            Coordinate{0.4, -0.202224401, 0}};
}

/// @p actual has the points @p expected, x, y and z within FreeformFinSetTest's tolerance.
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

TEST_F(TemplateRocket, GenerateTrapezoidalPointsWithCant)
{
    FreeformFinSet& fins = createFinOnConicalTransition();
    fins.setCantAngle(javaToRadians(15));

    const std::vector<Coordinate> expPoints{Coordinate{0, -0.001683625, 0}, Coordinate{0.4, 0.2, 0},
                                            Coordinate{0.4, -0.202224401, 0}};

    expectCantedPointsMatch(expPoints, fins.getFinPoints(), "Canted fin point");
    expectCantedPointsMatch(cantedRootPointsOnTailCone(), fins.getRootPoints(),
                            "Canted root point");
}

TEST_F(TemplateRocket, FreeformCMComputationTrapezoidOnTube)
{
    const FreeformFinSet& fins = createFinOnTube();

    // assert pre-condition:
    const std::vector<Coordinate> finPoints = fins.getFinPoints();
    ASSERT_EQ(4U, finPoints.size());
    EXPECT_EQ(finPoints[0], Coordinate::kZero);
    EXPECT_EQ(finPoints[1], (Coordinate{0.5, 1.0}));
    EXPECT_EQ(finPoints[2], (Coordinate{1.0, 1.0}));
    EXPECT_EQ(finPoints[3], (Coordinate{1.0, 0.0}));

    const double x0 = fins.getAxialFront();
    EXPECT_NEAR(1.0, x0, kEpsilon);
    EXPECT_NEAR(1.0, m_body->getRadius(x0), kEpsilon);

    // NOTE: this will be relative to the center of the finset -- which is at the
    // center of it's mounted body
    const Coordinate coords = fins.getCG();
    EXPECT_NEAR(0.75, fins.getPlanformArea(), kEpsilon);
    EXPECT_NEAR(0.611111, coords.x, kEpsilon);
    EXPECT_NEAR(1.444444, coords.y, kEpsilon);
}

TEST_F(TemplateRocket, FreeformCMComputationTriangleOnTransition)
{
    FreeformFinSet& fins = createFinOnConicalTransition();

    // assert pre-condition:
    const std::vector<Coordinate> finPoints = fins.getFinPoints();
    ASSERT_EQ(3U, finPoints.size());
    EXPECT_EQ(finPoints[0], Coordinate::kZero);
    EXPECT_EQ(finPoints[1], (Coordinate{0.4, 0.2}));
    EXPECT_EQ(finPoints[2], (Coordinate{0.4, -0.2}));

    const double x0 = fins.getAxialFront();
    EXPECT_NEAR(0.4, x0, kEpsilon);
    EXPECT_NEAR(0.8, m_tailCone->getRadius(x0), kEpsilon);

    // vv Test target vv
    const Coordinate coords = fins.getCG();
    // ^^ Test target ^^

    // in fin-mount frame coordinates
    const double expectedWettedArea = 0.08;
    EXPECT_NEAR(expectedWettedArea, fins.getPlanformArea(), kEpsilon);
    EXPECT_NEAR(0.266666, coords.x, kEpsilon);
    EXPECT_NEAR(0.8, coords.y, kEpsilon);

    // now, add a tab
    fins.setTabOffsetMethod(AxialMethod::TOP);
    fins.setTabOffset(0.1);
    fins.setTabHeight(0.2);
    fins.setTabLength(0.2);

    // fin is a simple trapezoid against a linearly changing body...
    // height is set s.t. the tab trailing edge height == 0
    const double tabLength           = fins.getFinFront().x + fins.getTabOffset();
    const double expectedTabArea     = fins.getTabHeight() * 0.5 * tabLength;
    const double expectedTotalVolume = (expectedWettedArea + expectedTabArea) * fins.getThickness();
    EXPECT_NEAR(expectedTotalVolume, fins.getComponentVolume(), kEpsilon)
        << "Calculated fin volume is wrong: ";

    const Coordinate tcg = fins.getCG();  // relative to parent. also includes fin tab CG.
    EXPECT_NEAR(0.238461, tcg.x, kEpsilon) << "Calculated fin centroid is wrong! ";
    EXPECT_NEAR(0.714102, tcg.y, kEpsilon) << "Calculated fin centroid is wrong! ";
}

TEST_F(TemplateRocket, FreeformCMComputationTriangleOnEllipsoid)
{
    const FinSet& fins = createFinOnEllipsoidNose();

    // assert preconditions::Mount
    EXPECT_EQ(TransitionShape::ELLIPSOID, m_nose->getShapeType());
    EXPECT_NEAR(1.0, m_nose->getLength(), kEpsilon);

    // Assert fin shape
    //              [1]       [2]
    //                 +======+
    //               /          \   [3]
    //            /            ---+----
    //         /       --------
    //  [0] /  --------
    // ---+----
    //
    //  [0]  ( 0.0, 0.0)
    //  [1]  ( 0.4, 1.0)
    //  [2]  ( 0.6, 1.0)
    //  [3]  ( 0.8, 0.7847)

    EXPECT_EQ(AxialMethod::TOP, fins.getAxialMethod());
    EXPECT_NEAR(0.02, fins.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.8, fins.getLength(), kEpsilon);

    const std::vector<Coordinate> finPoints = fins.getFinPoints();
    ASSERT_EQ(4U, finPoints.size());
    EXPECT_EQ(Coordinate::kZero, finPoints[0]);
    EXPECT_EQ((Coordinate{0.4, 1.0}), finPoints[1]);
    EXPECT_EQ((Coordinate{0.6, 1.0}), finPoints[2]);
    EXPECT_EQ((Coordinate{0.8, 0.78466912}), finPoints[3]);

    const double expectedPlanformArea = 0.13397384;
    const double actualPlanformArea   = fins.getPlanformArea();
    EXPECT_NEAR(expectedPlanformArea, actualPlanformArea, kEpsilon)
        << "Calculated fin planform area is wrong: ";

    const Coordinate wcg = fins.getCG();  // relative to parent
    EXPECT_NEAR(0.2733066, wcg.weight, kEpsilon) << "Calculated fin weight is wrong! ";
    EXPECT_NEAR(0.4793588, wcg.x, kEpsilon) << "Calculated fin centroid is wrong! ";
    EXPECT_NEAR(0.996741, wcg.y, kEpsilon) << "Calculated fin centroid is wrong! ";
}

TEST_F(TemplateRocket, FreeformCMComputationTrapezoidExtraPoints)
{
    FreeformFinSet& fins = createFinOnTube();

    // This is the same trapezoid as previous free form, but it has
    // some extra points along the lines.
    const std::vector<Coordinate> points{
        Coordinate{0.0, 0.0},                                              // original
        Coordinate{0.5, 1.0},                                              // original
        Coordinate{0.6, 1.0}, Coordinate{0.8, 1.0}, Coordinate{1.0, 1.0},  // original
        Coordinate{1.0, 0.6}, Coordinate{1.0, 0.0}                         // original
    };
    fins.setPoints(points);

    const Coordinate coords = fins.getCG();
    EXPECT_NEAR(0.75, fins.getPlanformArea(), kEpsilon);
    EXPECT_NEAR(0.611111, coords.x, kEpsilon);
    EXPECT_NEAR(1.444444, coords.y, kEpsilon);
}

TEST_F(TemplateRocket, FreeformCMComputationAdjacentPoints)
{
    FreeformFinSet& fins = createFinOnTube();

    // This is the same trapezoid as previous free form, but it has
    // some extra points which are very close to previous points.
    // in particular for points 0 & 1,
    // y0 + y1 is very small.
    const double                  permutation = 1.0e-15;
    const std::vector<Coordinate> points{
        Coordinate{0.0, 0.0},  // original
        Coordinate{0, permutation},
        Coordinate{0.5, 1.0},  // original
        Coordinate{0.5 + permutation, 1.0},
        Coordinate{1.0, 1.0},  // original
        Coordinate{1.0, permutation},
        Coordinate{1.0, 0.0}  // original
    };
    fins.setPoints(points);

    const Coordinate coords = fins.getCG();
    EXPECT_NEAR(0.75, fins.getPlanformArea(), kEpsilon);
    EXPECT_NEAR(0.611111, coords.x, kEpsilon);
    EXPECT_NEAR(1.444444, coords.y, kEpsilon);
}

TEST_F(TemplateRocket, FreeformFinAddPoint)
{
    FreeformFinSet& fin = createFinOnTube();

    EXPECT_EQ(4U, fin.getPointCount());

    //     +--+
    //    /   |x
    //   /    |
    //  +=====+
    const Point2D      toAdd{1.01, 0.8};
    const Result<void> added = fin.addPoint(3, toAdd);
    EXPECT_TRUE(added.has_value()) << "IllegalFinPointException thrown";

    EXPECT_EQ(5U, fin.getPointCount());
    const Coordinate addedPoint = fin.getFinPoints()[3];
    EXPECT_NEAR(1.1, addedPoint.x, 0.1);
    EXPECT_NEAR(0.8, addedPoint.y, 0.1);
}

/// The preconditions of testSetFirstPoint and testSetLastPoint.
void expectInitialTailConeFin(const FreeformFinSet& fins, const Transition& tailCone,
                              const std::vector<Coordinate>& initialPoints)
{
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);
    EXPECT_EQ(initialPoints, (std::vector<Coordinate>{Coordinate::kZero, Coordinate{0.4, 0.2},
                                                      Coordinate{0.4, -0.2}}));
    EXPECT_NEAR(1.0, tailCone.getLength(), kEpsilon);
    EXPECT_NEAR(0.8, tailCone.getRadius(fins.getAxialFront()), kEpsilon);
}

// testSetFirstPoint, testSetLastPoint and testGenerateBodyPointsWhenFinOutsideParentBounds run
// their cases one after the other on the same fin set, as the blocks of the JUnit methods do:
// each case starts from what the earlier ones left behind (the axial method and offset, the
// clamped points). The assertions stay in the test bodies: a helper function may hold only a
// handful of assertion macros before clang-tidy's cognitive-complexity limit.

TEST_F(TemplateRocket, SetFirstPoint)
{
    // more transitions trigger more complicated positioning math:
    FreeformFinSet&               fins          = createFinOnConicalTransition();
    const std::vector<Coordinate> initialPoints = fins.getFinPoints();

    // assert pre-conditions:
    expectInitialTailConeFin(fins, *m_tailCone, initialPoints);

    {  // case 1:
        setAxialOffset(fins, AxialMethod::TOP, 0.1);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.1, fins.getAxialOffset(), kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, 0.2, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.3, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.2, fins.getLength(), kEpsilon);

        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.85, fins.getFinFront().y, kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // middle point:
        EXPECT_NEAR(0.2, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.3, postPoints[1].y, kEpsilon);

        // final point
        EXPECT_NEAR(0.2, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.1, postPoints[2].y, kEpsilon);
    }
    {  // case 2:
        setAxialOffset(fins, AxialMethod::TOP, 0.1);
        fins.setPoints(initialPoints);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, -0.2, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(-0.1, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(1.0, fins.getFinFront().y, kEpsilon);

        EXPECT_NEAR(-0.1, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.6, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        EXPECT_NEAR(0.6, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.15, postPoints[1].y, kEpsilon);

        EXPECT_NEAR(0.6, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.25, postPoints[2].y, kEpsilon);
    }
    {  // case 3:
        setAxialOffset(fins, AxialMethod::MIDDLE, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.0, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, 0.1, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.05, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.3, fins.getLength(), kEpsilon);

        EXPECT_NEAR(0.4, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.8, fins.getFinFront().y, kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid-point
        EXPECT_NEAR(0.3, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.25, postPoints[1].y, kEpsilon);

        EXPECT_NEAR(0.3, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.15, postPoints[2].y, kEpsilon);
    }
    {  // case 4:
        setAxialOffset(fins, AxialMethod::MIDDLE, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.85, fins.getFinFront().y, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, -0.1, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.2, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.9, fins.getFinFront().y, kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid point
        EXPECT_NEAR(0.5, postPoints[1].x, kEpsilon);

        EXPECT_NEAR(0.5, postPoints[2].x, kEpsilon);

        EXPECT_NEAR(-0.05, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.5, fins.getLength(), kEpsilon);
    }
    {  // case 5:
        setAxialOffset(fins, AxialMethod::BOTTOM, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.6, fins.getFinFront().x, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, 0.1, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.7, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.65, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.0, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.3, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid-point
        EXPECT_NEAR(0.3, postPoints[1].x, kEpsilon);

        EXPECT_NEAR(0.3, postPoints[2].x, kEpsilon);
    }
    {  // case 6:
        setAxialOffset(fins, AxialMethod::BOTTOM, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.6, fins.getFinFront().x, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(0, -0.1, kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.5, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.75, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.5, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(3U, postPoints.size());

        EXPECT_NEAR(0.5, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.15, postPoints[1].y, kEpsilon);

        EXPECT_NEAR(0.5, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.25, postPoints[2].y, kEpsilon);
    }
}

TEST_F(TemplateRocket, SetLastPoint)
{
    FreeformFinSet&               fins          = createFinOnConicalTransition();
    const std::vector<Coordinate> initialPoints = fins.getFinPoints();
    const std::size_t             lastIndex     = initialPoints.size() - 1;
    const double                  xf            = initialPoints[lastIndex].x;
    const double                  yf            = initialPoints[lastIndex].y;

    // assert pre-conditions:
    expectInitialTailConeFin(fins, *m_tailCone, initialPoints);

    {  // case 1:
        setAxialOffset(fins, AxialMethod::TOP, 0.1);
        fins.setPoints(initialPoints);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf + 0.2, yf - kThreeTenthsF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.1, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.95, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.6, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // middle point:
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // last point:
        EXPECT_NEAR(0.6, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.3, postPoints[2].y, kEpsilon);
    }
    {  // case 2:
        setAxialOffset(fins, AxialMethod::TOP, 0.1);
        fins.setPoints(initialPoints);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf - 0.2, yf + kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.1, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.95, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.2, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // middle point:
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // last point:
        EXPECT_NEAR(0.2, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.1, postPoints[2].y, kEpsilon);
    }
    {  // case 3:
        setAxialOffset(fins, AxialMethod::MIDDLE, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf + 0.1, yf + kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.85, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.5, fins.getLength(), kEpsilon);
        EXPECT_NEAR(0.05, fins.getAxialOffset(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid-point
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // last point
        EXPECT_NEAR(0.5, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.25, postPoints[2].y, kEpsilon);
    }
    {  // case 4:
        setAxialOffset(fins, AxialMethod::MIDDLE, 0.0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf - 0.1, yf + kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.3, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.85, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.3, fins.getLength(), kEpsilon);
        EXPECT_NEAR(-0.05, fins.getAxialOffset(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid point
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // last point
        EXPECT_NEAR(0.3, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.15, postPoints[2].y, kEpsilon);
    }
    {  // case 5:
        setAxialOffset(fins, AxialMethod::BOTTOM, 0.0);
        fins.setPoints(initialPoints);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf + 0.1, yf + kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.6, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.7, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(0.1, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.5, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid-point
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // pseudo last point
        EXPECT_NEAR(0.5, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.2, postPoints[2].y, kEpsilon);
    }
    {  // case 6:
        setAxialOffset(fins, AxialMethod::BOTTOM, 0.0);
        fins.setPoints(initialPoints);

        // vvvv function under test vvvv
        EXPECT_TRUE(fins.setPoint(lastIndex, xf - 0.1, yf + kTenthF).has_value());
        // ^^^^ function under test ^^^^

        EXPECT_NEAR(0.6, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.7, fins.getFinFront().y, kEpsilon);
        EXPECT_NEAR(-0.1, fins.getAxialOffset(), kEpsilon);
        EXPECT_NEAR(0.3, fins.getLength(), kEpsilon);

        const std::vector<Coordinate> postPoints = fins.getFinPoints();
        ASSERT_EQ(postPoints.size(), 3U);

        // mid-point
        EXPECT_NEAR(0.4, postPoints[1].x, kEpsilon);
        EXPECT_NEAR(0.2, postPoints[1].y, kEpsilon);

        // last point
        EXPECT_NEAR(0.3, postPoints[2].x, kEpsilon);
        EXPECT_NEAR(-0.15, postPoints[2].y, kEpsilon);
    }
}

TEST_F(TemplateRocket, SetInteriorPoint)
{
    FreeformFinSet& fins = createFinOnConicalTransition();

    // preconditions // initial points
    const std::vector<Coordinate> initialPoints = fins.getFinPoints();
    ASSERT_EQ(initialPoints.size(), 3U);
    EXPECT_EQ(initialPoints[0], Coordinate::kZero);
    EXPECT_EQ(initialPoints[1], (Coordinate{0.4, 0.2}));
    EXPECT_EQ(initialPoints[2], (Coordinate{0.4, -0.2}));
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);

    // preconditions // mount
    EXPECT_NEAR(0.4, fins.getFinFront().x, kEpsilon);
    EXPECT_NEAR(0.8, fins.getFinFront().y, kEpsilon);
    EXPECT_NEAR(0.8, m_tailCone->getRadius(fins.getAxialFront()), kEpsilon);

    EXPECT_EQ(AxialMethod::TOP, fins.getAxialMethod());
    EXPECT_NEAR(0.4, fins.getAxialOffset(), kEpsilon);

    // test target
    const Coordinate p1 = fins.getFinPoints()[1];

    // vvvv function under test vvvv
    EXPECT_TRUE(fins.setPoint(1, p1.x + 0.1, p1.y + kTenthF).has_value());
    // ^^^^ function under test ^^^^

    // postconditions
    const std::vector<Coordinate> postPoints = fins.getFinPoints();
    ASSERT_EQ(postPoints.size(), 3U);

    // middle point:
    EXPECT_NEAR(0.5, postPoints[1].x, kEpsilon);
    EXPECT_NEAR(0.3, postPoints[1].y, kEpsilon);

    // last point:
    EXPECT_NEAR(0.4, postPoints[2].x, kEpsilon);
    EXPECT_NEAR(-0.2, postPoints[2].y, kEpsilon);

    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);
    EXPECT_NEAR(0.4, fins.getFinFront().x, kEpsilon);
    EXPECT_NEAR(0.8, fins.getFinFront().y, kEpsilon);
}

TEST_F(TemplateRocket, SetAllPointsOnPhantomBody)
{
    // setup // mount
    EXPECT_NEAR(0.5, m_phantomBody->getOuterRadius(), kEpsilon);
    EXPECT_NEAR(0.0, m_phantomBody->getLength(), kEpsilon);

    {
        //  (1)---------(2)
        //   |    Fin    |
        //   |           |
        //  (0)----+----(3)
        //         |
        //       (body)
        auto newFins = std::make_unique<FreeformFinSet>();
        newFins->setName("SquareFin");
        FreeformFinSet& added = m_phantomBody->addChild(std::move(newFins));
        setAxialOffset(added, AxialMethod::MIDDLE, 0.0);
        const std::vector<Coordinate> points{Coordinate{-0.5, 0.0}, Coordinate{-0.5, 1.0},
                                             Coordinate{0.5, 1.0}, Coordinate{0.5, 0.0}};
        added.setPoints(points);
    }

    // postconditions
    const auto* fins = dynamic_cast<const FreeformFinSet*>(&m_phantomBody->getChild(0));
    ASSERT_NE(fins, nullptr);

    EXPECT_EQ(AxialMethod::MIDDLE, fins->getAxialMethod());
    EXPECT_NEAR(0.0, fins->getAxialOffset(), kEpsilon);

    EXPECT_NEAR(-0.5, fins->getFinFront().x, kEpsilon);
    EXPECT_NEAR(0.5, fins->getFinFront().y, kEpsilon);

    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{0.0, 1.0}, Coordinate{1.0, 1.0}, Coordinate{1.0, 0.0}},
        fins->getFinPoints(), kEpsilon, "p");

    EXPECT_NEAR(1.0, fins->getLength(), kEpsilon);
}

TEST_F(TemplateRocket, SetFirstPointClampToLast)
{
    FreeformFinSet& fins = createFinOnConicalTransition();

    EXPECT_EQ(1, fins.getFinCount());
    EXPECT_EQ(3U, fins.getPointCount());
    EXPECT_EQ(AxialMethod::TOP, fins.getAxialMethod());
    EXPECT_NEAR(0.4, fins.getAxialOffset(), kEpsilon);  // pre-condition
    EXPECT_NEAR(1.0, m_tailCone->getLength(), kEpsilon);

    // fin offset: 0.4 -> 0.59 (just short of prev fin end)
    // fin end: 0.4 ~> min root chord
    // vv Test Target vv
    EXPECT_TRUE(fins.setPoint(0, 0.6, 0).has_value());
    // ^^ Test Target ^^

    EXPECT_EQ(fins.getFinPoints()[0], Coordinate::kZero);

    // setting the first point actually offsets the whole fin by that amount:
    const double expFinOffset = 0.8;
    EXPECT_NEAR(expFinOffset, fins.getAxialOffset(), kEpsilon)
        << "Resultant fin offset does not match!";

    EXPECT_EQ(3U, fins.getPointCount());
    const Coordinate actualLastPoint = fins.getFinPoints()[2];
    EXPECT_NEAR(0, actualLastPoint.x, kEpsilon);
    EXPECT_NEAR(0, actualLastPoint.y, kEpsilon);
    EXPECT_NEAR(0.0, fins.getLength(), kEpsilon) << "New fin length is wrong: ";
}

TEST_F(TemplateRocket, SetPointOtherPoint)
{
    // combine the simple case with the complicated to ensure that the simple case
    // is flagged, tested, and debugged before running the more complicated case...
    {
        // setting points on a Tube Body is the simpler case. Test this first:
        FreeformFinSet& fins = createFinOnConicalTransition();

        // all points are restricted to be outside the parent body:
        const Coordinate expPt = fins.getFinPoints()[0];
        EXPECT_TRUE(fins.setPoint(0, -0.6, 0).has_value());
        const Coordinate actPt = fins.getFinPoints()[0];
        EXPECT_NEAR(expPt.x, actPt.x, kEpsilon);
        // the last point is already clamped to the body; It should remain so.
        EXPECT_NEAR(0.0, actPt.y, kEpsilon);
    }
    {
        // more transitions trigger more complicated positioning math:
        FreeformFinSet& fins = createFinOnConicalTransition();

        const std::size_t testIndex = 1;
        // move point, and verify that it is coerced to be outside the parent body:
        const Coordinate expPl{0.2, -0.1, 0.0};
        // incorrect y-val. The function should correct the y-value to above.
        EXPECT_TRUE(fins.setPoint(testIndex, 0.2, -0.2).has_value());

        const Coordinate actPl = fins.getFinPoints()[testIndex];
        EXPECT_NEAR(expPl.x, actPl.x, kEpsilon);
        EXPECT_NEAR(expPl.y, actPl.y, kEpsilon);
    }
}

TEST_F(TemplateRocket, SetOffsetTriggerLeadingClampCorrection)
{
    // test correction of last point due to moving entire fin:
    FreeformFinSet& fins = createFinOnConicalTransition();

    const std::size_t lastIndex = fins.getPointCount() - 1;

    // pre-condition
    EXPECT_EQ(AxialMethod::TOP, fins.getAxialMethod());
    EXPECT_NEAR(0.4, fins.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);

    // vv Test Target vv
    fins.setAxialOffset(-0.2);
    // ^^ Test Target ^^

    // fin start: 0.4 => 0.8 [body]
    // fin end: 0.8 => 1.2 [body]
    EXPECT_NEAR(-0.2, fins.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);

    // SHOULD DEFINITELY CHANGE
    const Coordinate actualLastPoint = fins.getFinPoints()[lastIndex];
    EXPECT_NEAR(0.4, actualLastPoint.x, kEpsilon);
    EXPECT_NEAR(-0.1, actualLastPoint.y, kEpsilon);
}

TEST_F(TemplateRocket, SetOffsetTriggerTrailingClampCorrection)
{
    // test correction of last point due to moving entire fin:
    FreeformFinSet& fins = createFinOnConicalTransition();

    const std::size_t lastIndex = fins.getPointCount() - 1;

    // pre-condition
    EXPECT_EQ(AxialMethod::TOP, fins.getAxialMethod());
    EXPECT_NEAR(0.4, fins.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);

    // vv Test Target vv
    fins.setAxialOffset(0.8);
    // ^^ Test Target ^^

    // fin start: 0.4 => 0.8 [body]
    // fin end: 0.8 => 1.2 [body]
    EXPECT_NEAR(0.8, fins.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.4, fins.getLength(), kEpsilon);

    // SHOULD DEFINITELY CHANGE
    const Coordinate actualLastPoint = fins.getFinPoints()[lastIndex];
    EXPECT_NEAR(0.4, actualLastPoint.x, kEpsilon);
    EXPECT_NEAR(-0.1, actualLastPoint.y, kEpsilon);
}

TEST(FreeformFinSet, ComputeCMMountlessFin)
{
    // This is a trapezoid.  Height 1, root 1, tip 1/2 no sweep.
    // It can be decomposed into a rectangle followed by a triangle
    //  +---+
    //  |    \ .
    //  |     \ .
    //  +------+
    FreeformFinSet fins;
    fins.setFinCount(1);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0, 1}, Coordinate{0.5, 1},
                                         Coordinate{1, 0}};
    fins.setPoints(points);
    const Coordinate coords = fins.getCG();
    EXPECT_NEAR(0.75, fins.getPlanformArea(), kEpsilon);

    EXPECT_NEAR(0.388889, coords.x, kEpsilon);
    EXPECT_NEAR(0.444444, coords.y, kEpsilon);
}

/// One case of testTranslatePoints: the tube fin (createFinOnTube()) positioned by @p method at
/// @p offset has its points, moved to the parent's frame, @p expectedOffset behind the front of
/// the body.
void expectFinPointsAt(FreeformFinSet& fin, AxialMethod method, double offset,
                       double expectedOffset)
{
    setAxialOffset(fin, method, offset);

    EXPECT_EQ(fin.getAxialMethod(), method);
    EXPECT_NEAR(fin.getAxialOffset(), offset, kEpsilon);

    const double xDelta = fin.getAxialOffset(AxialMethod::TOP);

    const std::vector<Coordinate> actualPoints = fin.getFinPoints();

    const std::string rawPointDescr =
        "\n" + fin.toDebugDetail() + "\n>> axial offset: " + std::to_string(xDelta);

    const std::vector<Coordinate> expected{
        Coordinate{expectedOffset, 0}, Coordinate{0.5 + expectedOffset, 1},
        Coordinate{1 + expectedOffset, 1}, Coordinate{1 + expectedOffset, 0}};
    expectPointsNear(expected, FinSet::translatePoints(actualPoints, xDelta, 0), kEpsilon,
                     "Bad Fin Position (" + std::to_string(offset) + " via:" +
                         std::string{QtRocket::axialMethodName(method)} + ") " + rawPointDescr);
}

TEST_F(TemplateRocket, TranslatePoints)
{
    FreeformFinSet& fin = createFinOnTube();

    ASSERT_NE(fin.getParent(), nullptr);

    // Fin length = 1
    // Body Length = 2
    //          +--+
    //         /   |
    //        /    |
    //   +---+-----+---+
    const std::vector<Coordinate> expectPoints{Coordinate::kZero, Coordinate{0.5, 1},
                                               Coordinate{1, 1}, Coordinate{1, 0}};

    const std::vector<Coordinate> finPoints = fin.getFinPoints();
    ASSERT_EQ(4U, finPoints.size());
    EXPECT_EQ(expectPoints[1], finPoints[1]);
    EXPECT_EQ(expectPoints[2], finPoints[2]);
    EXPECT_EQ(expectPoints[3], finPoints[3]);

    // mounting body:
    EXPECT_NEAR(m_body->getLength(), 2.0, kEpsilon);

    EXPECT_EQ(fin.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_NEAR(fin.getAxialOffset(), 0.0, kEpsilon);

    // (The Java test loops over these four cases.)
    expectFinPointsAt(fin, AxialMethod::TOP, 1.0, 1.0);
    expectFinPointsAt(fin, AxialMethod::MIDDLE, 0.0, 0.5);
    expectFinPointsAt(fin, AxialMethod::MIDDLE, 0.4, 0.9);
    expectFinPointsAt(fin, AxialMethod::BOTTOM, -0.2, 0.8);
}

TEST_F(TemplateRocket, ForIntersectionFalse)
{
    const FreeformFinSet& fins = createFinOnTube();

    // Fin length = 1
    // Body Length = 2
    //          +--+
    //         /   |
    //        /    |
    //   +---+-----+---+
    const std::vector<Coordinate> finPoints = fins.getFinPoints();
    ASSERT_EQ(4U, finPoints.size());
    EXPECT_EQ(finPoints[0], Coordinate::kZero);
    EXPECT_EQ(finPoints[1], (Coordinate{0.5, 1.0}));
    EXPECT_EQ(finPoints[2], (Coordinate{1.0, 1.0}));
    EXPECT_EQ(finPoints[3], (Coordinate{1.0, 0.0}));

    EXPECT_FALSE(fins.intersects()) << " Fin detects false positive intersection in fin points: ";
}

TEST_F(TemplateRocket, ForIntersectionTrue)
{
    FreeformFinSet& fins = createFinOnConicalTransition();

    // An obviously intersecting fin:
    //   [2] +-----+ [1]
    //        \   /
    //         \ /
    //          X
    //         / \ .
    //    [0] /   \ [3]
    //   +---+-----+---+
    // = +x =>
    const std::vector<Coordinate> initPoints{Coordinate{0, 0}, Coordinate{1, 1}, Coordinate{0, 1},
                                             Coordinate{1, 0}};
    // this line throws an exception?
    fins.setPoints(initPoints);

    // this *already* has detected the intersection, and aborted...
    const Coordinate p1 = fins.getFinPoints()[1];
    // ... which makes a rather hard-to-test functionality...
    EXPECT_NE(p1.x, initPoints[1].x) << "Fin Set failed to detect an intersection! ";
    EXPECT_NE(p1.y, initPoints[1].y) << "Fin Set failed to detect an intersection! ";
}

TEST_F(TemplateRocket, ForIntersectionAtFirstLast)
{
    FreeformFinSet& fins = createFinOnConicalTransition();

    // An obviously intersecting fin:
    //   [2] +---+ [1]
    //       |  /
    //       | /
    //    [0]|/ [3]
    //   +---+-----+---+
    // = +x =>
    const std::vector<Coordinate> initPoints{Coordinate{0, 0}, Coordinate{0, 1}, Coordinate{1, 1},
                                             Coordinate{0, 0}};
    // this line throws an exception?
    fins.setPoints(initPoints);

    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{0.0, 1.0}, Coordinate{1.0, 1.0}, Coordinate{0.0, 0.0}},
        fins.getFinPoints(), kEpsilon, "incorrect body points! ");
}

// The geometric half of testWildmanVindicatorShape; its second half waits for
// aero/barrowman/FinSetCalc.
// HOOK(fin-set-calc): add the second half: the fin set on a BodyTube(0.1, 0.1), then
// FinSetCalc(fins).calculateNonaxialForces(FlightConditions(null), transform, forces, warnings)
// with the transform {{1, 0, 0}, {0, 0, -1}, {0, 1, 1}} of FreeformFinSetTest.java:1370, and
// forces.getCP().x == 0.023409 +- 1e-4.
TEST(FreeformFinSet, WildmanVindicatorShape)
{
    // This fin shape is similar to the aft fins on the Wildman Vindicator.
    // A user noticed that if the y values are similar but not equal,
    // the computation of CP was incorrect because of numerical instability.
    //
    //     +-----------------+
    //      \                 \ .
    //       \                 \ .
    //        +                 \         +x
    //       /                   \        <=+
    //      +---------------------+
    //
    FreeformFinSet fins;
    fins.setFinCount(1);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02143125, 0.01143},
                                         Coordinate{0.009524999999999999, 0.032543749999999996},
                                         Coordinate{0.041275, 0.032537399999999994},
                                         Coordinate{0.066675, 0}};
    fins.setPoints(points);
    const Coordinate coords = fins.getCG();

    EXPECT_NEAR(0.00130708, fins.getPlanformArea(), kEpsilon);

    EXPECT_NEAR(0.03423168, coords.x, kEpsilon);
    EXPECT_NEAR(0.01427544, coords.y, kEpsilon);
}

// The set-up of testFinsOnTransitions with the outlines it gives (pinned with OpenRocket,
// ProbeFins2); the mean aerodynamic chords the JUnit test asserts wait for
// aero/barrowman/FinSetCalc.
// HOOK(fin-set-calc): add the JUnit assertions: FinSetCalc(fins).getMACLength() == 0.075 +- 1e-6
// after test 1 and == 0.05053191489361704 +- 1e-6 after test 2 (a new FinSetCalc for each).
TEST(FreeformFinSet, FinsOnTransitionsOutlines)
{
    // Rocket consisting of just a transition and a freeform fin set (its events stay disabled,
    // as in the Java test)
    Rocket rocket;

    auto& stage = rocket.addChild(std::make_unique<AxialStage>());

    Transition& trans = stage.addChild(std::make_unique<Transition>());

    FreeformFinSet& fins = trans.addChild(std::make_unique<FreeformFinSet>());

    // set the finset at the beginning of the transition
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.0);

    // Test 1: transition getting smaller
    trans.setForeRadius(trans.getForeRadius() * 2.0);
    const std::vector<Coordinate> smaller{
        Coordinate{0, 0}, Coordinate{trans.getLength(), 0},
        Coordinate{trans.getLength(), trans.getAftRadius() - trans.getForeRadius()}};
    fins.setPoints(smaller);

    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.07500000000000001, 0.0},
                      Coordinate{0.07500000000000001, -0.025}},
                     fins.getFinPoints(), 1e-15, "transition getting smaller");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.07500000000000001, -0.025}},
                     fins.getRootPoints(), 1e-15, "root of the transition getting smaller");
    EXPECT_NEAR(fins.getLength(), 0.07500000000000001, 1e-16);
    EXPECT_NEAR(fins.getSpan(), 0.025, 1e-16);
    EXPECT_NEAR(fins.getPlanformArea(), 9.375E-4, 1e-16);
    EXPECT_NEAR(fins.getComponentCG().x, 0.05000000000000002, 1e-15);
    EXPECT_NEAR(fins.getFinFront().y, 0.05, 1e-16);

    // Test 2: transition getting larger
    trans.setForeRadius(trans.getAftRadius());
    trans.setAftRadius(trans.getAftRadius() * 2.0);
    const std::vector<Coordinate> larger{
        Coordinate{0, 0}, Coordinate{0, trans.getAftRadius() - trans.getForeRadius()},
        Coordinate{trans.getLength(), trans.getAftRadius() - trans.getForeRadius()}};
    fins.setPoints(larger);

    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{0.0, 0.025}, Coordinate{0.07500000000000001, 0.025}},
        fins.getFinPoints(), 1e-15, "transition getting larger");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.07500000000000001, 0.025}},
                     fins.getRootPoints(), 1e-15, "root of the transition getting larger");
    EXPECT_NEAR(fins.getSpan(), 0.025, 1e-16);
    EXPECT_NEAR(fins.getFinFront().y, 0.025, 1e-16);
}

TEST_F(TemplateRocket, GenerateRootPointsOnBodyTube)
{
    const FreeformFinSet& fins = createFinOnTube();

    // root points (relative to fin-front)
    const std::vector<Coordinate> finPoints  = fins.getFinPoints();
    const std::vector<Coordinate> rootPoints = fins.getRootPoints();

    ASSERT_EQ(2U, rootPoints.size())
        << "Method should only generate minimal points for a conical transition fin body! ";
    EXPECT_NEAR(finPoints[0].x, rootPoints[0].x, kEpsilon) << "incorrect body point: 0::x ! ";
    EXPECT_NEAR(finPoints[0].y, rootPoints[0].y, kEpsilon) << "incorrect body point: 0::y ! ";
    EXPECT_NEAR(finPoints[finPoints.size() - 1].x, rootPoints[1].x, kEpsilon)
        << "incorrect body point: -1::x !";
    EXPECT_NEAR(finPoints[finPoints.size() - 1].y, rootPoints[1].y, kEpsilon)
        << "incorrect body point: -1::y !";
}

TEST_F(TemplateRocket, GenerateBodyPointsOnConicalTransition)
{
    const FreeformFinSet& fins = createFinOnConicalTransition();

    // body points (relative to root)
    const std::vector<Coordinate> finPoints  = fins.getFinPoints();
    const std::vector<Coordinate> rootPoints = fins.getRootPoints();

    ASSERT_EQ(2U, rootPoints.size())
        << "Method should only generate minimal points for a conical transition fin body! ";
    EXPECT_NEAR(finPoints[0].x, rootPoints[0].x, kEpsilon) << "incorrect body points! ";
    EXPECT_NEAR(finPoints[0].y, rootPoints[0].y, kEpsilon) << "incorrect body points! ";
    EXPECT_NEAR(finPoints[finPoints.size() - 1].x, rootPoints[1].x, kEpsilon)
        << "incorrect body points! ";
    EXPECT_NEAR(finPoints[finPoints.size() - 1].y, rootPoints[1].y, kEpsilon)
        << "incorrect body points! ";
}

/// The root point @p rootPoint of a fin whose front is at @p finFront on @p nose lies at
/// @p expectedX on the nose's surface.
void expectRootPointOnNose(const NoseCone& nose, const Coordinate& finFront,
                           const Coordinate& rootPoint, double expectedX)
{
    EXPECT_NEAR(expectedX, rootPoint.x, kEpsilon) << "Root points :: x coordinate mismatch!";
    EXPECT_NEAR(nose.getRadius(rootPoint.x + finFront.x) - finFront.y, rootPoint.y, kEpsilon)
        << "Root points @ x = " << expectedX << " :: y coordinate mismatch!";
}

TEST_F(TemplateRocket, GenerateBodyPointsOnEllipsoidNose)
{
    const FreeformFinSet& fins = createFinOnEllipsoidNose();

    const Coordinate              finFront  = fins.getFinFront();
    const std::vector<Coordinate> finPoints = fins.getFinPoints();

    // fin points (relative to fin) // preconditions
    ASSERT_EQ(4U, finPoints.size());

    EXPECT_NEAR(0.0, finPoints[0].x, kEpsilon) << "incorrect fin points! ";
    EXPECT_NEAR(0.0, finPoints[0].y, kEpsilon) << "incorrect fin points! ";

    EXPECT_NEAR(0.8, finPoints[3].x, kEpsilon) << "incorrect fin points! ";

    EXPECT_NEAR(m_nose->getRadius(0.8 + finFront.x) - finFront.y, finPoints[3].y, kEpsilon);

    EXPECT_NEAR(0.78466912, finPoints[3].y, kEpsilon) << "incorrect fin points! ";

    // body points (relative to fin)
    const std::vector<Coordinate> rootPoints = fins.getRootPoints();
    ASSERT_EQ(101U, rootPoints.size());
    const std::size_t lastIndex = 100;

    // trivial, and uninteresting:
    EXPECT_NEAR(finPoints[0].x, rootPoints[0].x, kEpsilon) << "incorrect body points! ";
    EXPECT_NEAR(finPoints[0].y, rootPoints[0].y, kEpsilon) << "incorrect body points! ";

    // n.b.: This should match EXACTLY the end point of the fin. (in fin coordinates)
    EXPECT_NEAR(finPoints[finPoints.size() - 1].x, rootPoints[lastIndex].x, kEpsilon)
        << "incorrect body points! ";
    EXPECT_NEAR(finPoints[finPoints.size() - 1].y, rootPoints[lastIndex].y, kEpsilon)
        << "incorrect body points! ";

    // the tests below are rather fragile, and may break for reasons other than bugs: the number
    // of points is somewhat arbitrary, but if this test fails, the rest *definitely* will.
    // (The Java test loops over these four points.)
    expectRootPointOnNose(*m_nose, finFront, rootPoints[2], 0.016);
    expectRootPointOnNose(*m_nose, finFront, rootPoints[5], 0.04);
    expectRootPointOnNose(*m_nose, finFront, rootPoints[61], 0.488);
    expectRootPointOnNose(*m_nose, finFront, rootPoints[88], 0.704);
}

TEST_F(TemplateRocket, GenerateBodyPointsWhenFinOutsideParentBounds)
{
    FreeformFinSet&               fins          = createFinOnConicalTransition();
    const std::vector<Coordinate> initialPoints = fins.getFinPoints();

    EXPECT_NEAR(1.0, m_tailCone->getLength(), kEpsilon);

    {  // move first point out of bounds, keep last point in bounds
        setAxialOffset(fins, AxialMethod::TOP, 0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.0, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(1.0, fins.getFinFront().y, kEpsilon);

        // Move first point
        EXPECT_TRUE(fins.setPoint(0, -0.1, kTenthF).has_value());

        expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, 0.0}, Coordinate{0.5, -0.2}},
                         fins.getRootPoints(), kEpsilon, "incorrect body points! ");

        EXPECT_NEAR(0.306, fins.getMass(), kEpsilon) << "incorrect fin mass! ";
    }
    {  // move both first and last point out of bounds to the left
        setAxialOffset(fins, AxialMethod::TOP, 0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0.0, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(1.0, fins.getFinFront().y, kEpsilon);

        // Move first and last point
        EXPECT_TRUE(fins.setPoint(0, -0.2, kTenthF).has_value());
        EXPECT_TRUE(fins.setPoint(fins.getPointCount() - 1, 0.1, kTenthF).has_value());

        expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, 0.0}}, fins.getRootPoints(),
                         kEpsilon, "incorrect body points! ");

        EXPECT_NEAR(0.034, fins.getMass(), kEpsilon) << "incorrect fin mass! ";
    }
    {  // move last point out of bounds, keep first point in bounds
        fins.setPoints(initialPoints);
        setAxialOffset(fins, AxialMethod::BOTTOM, fins.getLength());
        EXPECT_NEAR(1.0, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.5, fins.getFinFront().y, kEpsilon);

        // Move first point
        EXPECT_TRUE(fins.setPoint(0, -static_cast<double>(0.1F), kTenthF).has_value());
        EXPECT_TRUE(
            fins.setPoint(fins.getPointCount() - 1, static_cast<double>(0.2F), 0.0).has_value());

        expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, -0.05}, Coordinate{0.2, -0.05}},
                         fins.getRootPoints(), kEpsilon, "incorrect body points! ");

        EXPECT_NEAR(0.102, fins.getMass(), kEpsilon) << "incorrect fin mass! ";
    }
    {  // move both first and last point out of bounds to the right
        fins.setPoints(initialPoints);
        setAxialOffset(fins, AxialMethod::BOTTOM, fins.getLength());
        EXPECT_NEAR(1.0, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(0.5, fins.getFinFront().y, kEpsilon);

        // Move first and last point
        EXPECT_TRUE(fins.setPoint(0, 0.1, kTenthF).has_value());
        EXPECT_TRUE(fins.setPoint(fins.getPointCount() - 1, 0.2, kTenthF).has_value());

        expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.2, 0.0}}, fins.getRootPoints(),
                         kEpsilon, "incorrect body points! ");

        EXPECT_NEAR(0.068, fins.getMass(), kEpsilon) << "incorrect fin mass! ";
    }
    {  // move first point out of bounds to the left, and last point out of bounds to the right
        setAxialOffset(fins, AxialMethod::TOP, 0);
        fins.setPoints(initialPoints);
        EXPECT_NEAR(0, fins.getFinFront().x, kEpsilon);
        EXPECT_NEAR(1, fins.getFinFront().y, kEpsilon);

        // Move first and last point
        EXPECT_TRUE(fins.setPoint(0, -0.1, kTenthF).has_value());
        EXPECT_TRUE(fins.setPoint(fins.getPointCount() - 1, 1.2, -kTenthF).has_value());

        expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, 0.0}, Coordinate{1.1, -0.5},
                          Coordinate{1.2, -0.5}},
                         fins.getRootPoints(), kEpsilon, "incorrect body points! ");

        EXPECT_NEAR(0.833, fins.getMass(), kEpsilon) << "incorrect fin mass! ";
    }
}

TEST_F(TemplateRocket, FreeFormCMWithNegativeY)
{
    // A user submitted an ork file which could not be simulated.
    // This Fin set is constructed to have the same problem.  It is a square and rectangle
    // where the two trailing edge corners of the rectangle satisfy y_0 = -y_1
    //
    //    +=> +x
    //    0    1    2    3
    //    |    |    |    |
    //
    //    +---------+      - +1
    //    |         |             ^
    //    |         |             | +y
    //----+----+    |      -  0   +
    //         |    |
    //         |    |
    //         +----+      - -1
    //         |
    //         |
    FreeformFinSet& fins = m_body->addChild(std::make_unique<FreeformFinSet>());
    setAxialOffset(fins, AxialMethod::TOP, 1.0);
    fins.setFinCount(1);

    const std::vector<Coordinate> points{Coordinate{0.0, 0},     Coordinate{0.0, 1},
                                         Coordinate{2.0, 1},     Coordinate{2.0, -1},
                                         Coordinate{1.0001, -1}, Coordinate{1.0, 0}};
    fins.setPoints(points);

    fins.setPoints(points);
    fins.setFilletRadius(0.0);
    fins.setTabHeight(0.0);
    fins.setCrossSection(FinSet::CrossSection::SQUARE);  // to ensure uniform density
    fins.setMaterial(Material::newMaterial(Material::Type::BULK, "dummy", 1.0, true));

    EXPECT_NEAR(3.0, fins.getPlanformArea(), 0.0001);

    const Coordinate cg = fins.getCG();
    EXPECT_NEAR(1.1666, cg.x, 0.0001);
    EXPECT_NEAR(1.1666, cg.y, 0.0001);
    EXPECT_NEAR(0.0, cg.z, kEpsilon);
    EXPECT_NEAR(0.009, cg.weight, kEpsilon);
}

TEST_F(TemplateRocket, FreeFormCMWithTooManyPoints)
{
    auto fins = std::make_unique<FreeformFinSet>();

    const std::vector<Coordinate> setPoints{Coordinate{0.006349996571001852, 0.0},
                                            Coordinate{0.00635, 0.022224999999999998},
                                            Coordinate{0.0067056, 0.02716387422039681},
                                            Coordinate{0.007619999999999999, 0.03174998285500926},
                                            Coordinate{0.0093472, 0.036159702695982766},
                                            Coordinate{0.0110998, 0.03951108977512263},
                                            Coordinate{0.028134012585410983, 0.06508746485276898},
                                            Coordinate{0.030427066902717206, 0.06843885193190885},
                                            Coordinate{0.03298470441048184, 0.07170204461422924},
                                            Coordinate{0.0351895643309686, 0.073906904534716},
                                            Coordinate{0.03801178502919164, 0.0756707924711054},
                                            Coordinate{0.04101039452105363, 0.07672912523293904},
                                            Coordinate{0.04409719840973508, 0.07743468040749481},
                                            Coordinate{0.04762497428251389, 0.07787565239159215},
                                            Coordinate{0.0511527501552927, 0.07797799999999999},
                                            Coordinate{0.08021280390730812, 0.07797799999999999},
                                            Coordinate{0.08127113666914176, 0.07796384678841163},
                                            Coordinate{0.08206488624051698, 0.07787565239159215},
                                            Coordinate{0.08281453861348248, 0.07747877760590453},
                                            Coordinate{0.08316731620076037, 0.07681731962975852},
                                            Coordinate{0.08325551059757984, 0.07584718126474434},
                                            Coordinate{0.083312, 0.07487704289973017},
                                            Coordinate{0.08329960779598958, 0.033293384799349984},
                                            Coordinate{0.08325551059757984, 0.03254373242638449},
                                            Coordinate{0.08307912180394089, 0.03174998285500926},
                                            Coordinate{0.08263814981984355, 0.031132622077272968},
                                            Coordinate{0.08180030305005857, 0.030691650093175617},
                                            Coordinate{0.0806978730898152, 0.030479999999999997},
                                            Coordinate{0.06178017497203885, 0.030479999999999997},
                                            Coordinate{0.05635621956764143, 0.030479999999999997},
                                            Coordinate{0.05344580447259892, 0.030225999999999996},
                                            Coordinate{0.051461430544160844, 0.0292862},
                                            Coordinate{0.050006222996639586, 0.027711399999999997},
                                            Coordinate{0.04921247342526435, 0.0261112},
                                            Coordinate{0.048683307044347535, 0.024002999999999997},
                                            Coordinate{0.048768, 0.022098},
                                            Coordinate{0.048768, 0.0}};
    fins->setPoints(setPoints);
    const FreeformFinSet& added = m_phantomBody->addChild(std::move(fins));

    // fin points (relative to fin) // preconditions
    const std::vector<Coordinate> finPoints = added.getFinPoints();
    ASSERT_EQ(37U, finPoints.size());

    // fin root length:
    EXPECT_NEAR(0.04241800, added.getLength(), kEpsilon);

    // p_first
    EXPECT_NEAR(0.0, finPoints[0].x, kEpsilon);
    EXPECT_NEAR(0.0, finPoints[0].y, kEpsilon);

    // p_last
    EXPECT_NEAR(0.042418, finPoints[36].x, kEpsilon);
    EXPECT_NEAR(0.0, finPoints[36].y, kEpsilon);
}

// ============================================================ beyond OpenRocket's own tests

TEST(FreeformFinSet, Defaults)
{
    const FreeformFinSet fins;
    EXPECT_EQ(fins.kind(), ComponentKind::FREEFORM_FIN_SET);
    EXPECT_EQ(fins.getComponentName(), "Freeform Fin Set");
    EXPECT_EQ(fins.getFinCount(), 3);
    EXPECT_EQ(fins.getPointCount(), 4U);
    EXPECT_EQ(fins.getLength(), 0.05);
    EXPECT_EQ(fins.getSpan(), 0.05);
    EXPECT_FALSE(fins.intersects());
    EXPECT_EQ(FreeformFinSet::kSnapLargerThan, 2.5);

    const std::vector<Coordinate> points = fins.getFinPoints();
    ASSERT_EQ(points.size(), 4U);
    EXPECT_TRUE(points[0].exactlyEquals(Coordinate{0.0, 0.0}));
    EXPECT_TRUE(points[1].exactlyEquals(Coordinate{0.025, 0.05}));
    EXPECT_TRUE(points[2].exactlyEquals(Coordinate{0.075, 0.05}));
    EXPECT_TRUE(points[3].exactlyEquals(Coordinate{0.05, 0.0}));

    // Pinned with OpenRocket (ProbeFins S9).
    EXPECT_NEAR(fins.getPlanformArea(), 0.0025000000000000005, 1e-17);
    const Coordinate cg = fins.getComponentCG();
    EXPECT_NEAR(cg.x, 0.03749999999999999, 1e-16);
    EXPECT_EQ(cg.y, 0.0);
    EXPECT_NEAR(cg.weight, 0.015300000000000005, 1e-16);
}

TEST_F(TemplateRocket, AddPointChecksTheIndex)
{
    FreeformFinSet& fin = createFinOnTube();
    recordEvents();

    const Result<void> first = fin.addPoint(0, Point2D{0.1, 0.1});
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(first.error().message,
              "Cannot add new point before the first or after the last point");
    const Result<void> beyond = fin.addPoint(4, Point2D{0.1, 0.1});
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error().message,
              "Cannot add new point before the first or after the last point");
    EXPECT_EQ(fin.getPointCount(), 4U);
    EXPECT_TRUE(takeEvents().empty());

    // An index that was negative before it became a std::size_t fails with the message Java
    // gives for the negative index, and nothing is touched (pinned with OpenRocket, ProbeFix S1).
    const std::vector<Coordinate> before = fin.getFinPoints();
    for (const std::size_t index : kIndicesFromNegatives)
    {
        expectIllegalFinPoint(fin.addPoint(index, Point2D{0.1, 0.1}),
                              "Cannot add new point before the first or after the last point",
                              index);
    }
    EXPECT_EQ(fin.getPointCount(), 4U);
    expectPointsNear(before, fin.getFinPoints(), 0.0, "after the rejected additions");
    EXPECT_TRUE(takeEvents().empty());

    // Before the last point is the last place a point can go.
    EXPECT_TRUE(fin.addPoint(3, Point2D{1.0, 0.5}).has_value());
    EXPECT_TRUE(fin.addPoint(1, Point2D{0.25, 0.5}).has_value());
    EXPECT_EQ(takeEvents(), (std::vector<int>{ComponentChangeEvent::kNonFunctionalChange,
                                              ComponentChangeEvent::kNonFunctionalChange}));
    expectPointsNear({Coordinate{0, 0}, Coordinate{0.25, 0.5}, Coordinate{0.5, 1}, Coordinate{1, 1},
                      Coordinate{1, 0.5}, Coordinate{1, 0}},
                     fin.getFinPoints(), 1e-15, "after adding");
}

TEST(FreeformFinSet, RemovePointChecksTheIndexAndTheOutline)
{
    // Pinned with OpenRocket (ProbeFins S7).
    FreeformFinSet                fins;
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.6, 0.6},
                                         Coordinate{0.1, 0.9}, Coordinate{0.9, 0.9},
                                         Coordinate{1, 0}};
    fins.setPoints(points);
    ASSERT_EQ(fins.getPointCount(), 5U);
    EXPECT_FALSE(fins.intersects());

    const Result<void> first = fins.removePoint(0);
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(first.error().message, "cannot remove first or last point");
    const Result<void> last = fins.removePoint(4);
    ASSERT_FALSE(last.has_value());
    EXPECT_EQ(last.error().message, "cannot remove first or last point");
    const Result<void> beyond = fins.removePoint(5);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error().message, "index out of range");
    EXPECT_EQ(fins.getPointCount(), 5U);

    // As Java's negative indices (ProbeFix S1): out of range, and nothing is touched.
    for (const std::size_t index : kIndicesFromNegatives)
    {
        expectIllegalFinPoint(fins.removePoint(index), "index out of range", index);
    }
    EXPECT_EQ(fins.getPointCount(), 5U);
    expectPointsNear(points, fins.getFinPoints(), 0.0, "after the rejected indices");

    // Without point 3 the outline would cross itself: the points stay.
    EXPECT_TRUE(fins.removePoint(3).has_value());
    EXPECT_EQ(fins.getPointCount(), 5U);
    expectPointsNear(points, fins.getFinPoints(), 0.0, "after the rejected removal");

    EXPECT_TRUE(fins.removePoint(2).has_value());
    expectPointsNear(
        {Coordinate{0, 0}, Coordinate{0.6, 0.6}, Coordinate{0.9, 0.9}, Coordinate{1, 0}},
        fins.getFinPoints(), 0.0, "after the removal");

    EXPECT_TRUE(fins.addPoint(1, Point2D{0.1, 0.3}).has_value());
    expectPointsNear({Coordinate{0, 0}, Coordinate{0.1, 0.3}, Coordinate{0.6, 0.6},
                      Coordinate{0.9, 0.9}, Coordinate{1, 0}},
                     fins.getFinPoints(), 0.0, "after adding");
    EXPECT_EQ(fins.getLength(), 1.0);
}

TEST_F(TemplateRocket, RemovePointFiresEvenWhenItRollsBack)
{
    FreeformFinSet&               fins = m_body->addChild(std::make_unique<FreeformFinSet>());
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.6, 0.6},
                                         Coordinate{0.1, 0.9}, Coordinate{0.9, 0.9},
                                         Coordinate{1, 0}};
    fins.setPoints(points);
    recordEvents();

    EXPECT_TRUE(fins.removePoint(3).has_value());
    EXPECT_EQ(fins.getPointCount(), 5U);
    EXPECT_TRUE(fins.removePoint(2).has_value());
    EXPECT_EQ(fins.getPointCount(), 4U);
    EXPECT_EQ(takeEvents(), (std::vector<int>{ComponentChangeEvent::kAeromassChange,
                                              ComponentChangeEvent::kAeromassChange}));
    EXPECT_FALSE(fins.removePoint(0).has_value());
    EXPECT_TRUE(takeEvents().empty()) << "a rejected index fires nothing";
}

TEST(FreeformFinSet, SetPointsMovesTheOutlineToTheOriginAndLimitsIt)
{
    // Pinned with OpenRocket (ProbeFins S7).
    FreeformFinSet                fins;
    const std::vector<Coordinate> far{Coordinate{1, 1}, Coordinate{4, 5}, Coordinate{3.2, 1.5},
                                      Coordinate{6.0, 1.0}};
    fins.setPoints(far);
    // Kept from OpenRocket: a point with x and y both above the limit keeps its x.
    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{3.0, 2.5}, Coordinate{2.2, 0.5}, Coordinate{2.5, 0.0}},
        fins.getFinPoints(), 1e-15, "limited");
    EXPECT_EQ(fins.getLength(), 2.5);
    EXPECT_EQ(fins.getSpan(), 2.5);

    // The weights and z are translated with the points (Coordinate::add).
    const std::vector<Coordinate> weighted{Coordinate{0.5, 0.25, 0, 3.0},
                                           Coordinate{0.75, 1, 0.5, 2.0}, Coordinate{1, 0}};
    fins.setPoints(weighted);
    const std::vector<Coordinate> moved = fins.getFinPoints();
    ASSERT_EQ(moved.size(), 3U);
    EXPECT_TRUE(moved[0].exactlyEquals(Coordinate{0.0, 0.0, 0.0, 0.0}));
    EXPECT_TRUE(moved[1].exactlyEquals(Coordinate{0.25, 0.75, 0.5, -1.0}));
    EXPECT_TRUE(moved[2].exactlyEquals(Coordinate{0.5, -0.25, 0.0, -3.0}));

    // An outline that starts at the origin is taken as it is.
    const std::vector<Coordinate> atOrigin{Coordinate{0, 0, 0, 3.0}, Coordinate{0.5, 1, 0.5, 2.0},
                                           Coordinate{1, 0}};
    fins.setPoints(atOrigin);
    const std::vector<Coordinate> kept = fins.getFinPoints();
    ASSERT_EQ(kept.size(), 3U);
    EXPECT_TRUE(kept[0].exactlyEquals(atOrigin[0]));
    EXPECT_TRUE(kept[1].exactlyEquals(atOrigin[1]));
    EXPECT_TRUE(kept[2].exactlyEquals(atOrigin[2]));
}

TEST(FreeformFinSet, SetPointsRejectsAnIntersectingOutline)
{
    // Pinned with OpenRocket (ProbeFins S7).
    FreeformFinSet                fins;
    const std::vector<Coordinate> crossing{Coordinate{0, 0}, Coordinate{1, 1}, Coordinate{0, 1},
                                           Coordinate{1, 0}};
    fins.setPoints(crossing);
    EXPECT_EQ(fins.getPointCount(), 4U);
    EXPECT_EQ(fins.getLength(), 0.05) << "the previous points and length";
    EXPECT_TRUE(fins.getFinPoints()[1].exactlyEquals(Coordinate{0.025, 0.05}));
    EXPECT_FALSE(fins.intersects());

    // An outline that touches itself counts as intersecting.
    const std::vector<Coordinate> touching{Coordinate{0, 0},      Coordinate{0.5, 1},
                                           Coordinate{1, 0},      Coordinate{0.5, 0},
                                           Coordinate{0.25, 0.5}, Coordinate{2, 0}};
    fins.setPoints(touching);
    EXPECT_EQ(fins.getPointCount(), 4U);
    EXPECT_EQ(fins.getLength(), 0.05);

    // A closed outline (the last point on the first) does not.
    const std::vector<Coordinate> closed{Coordinate{0, 0}, Coordinate{0, 1}, Coordinate{1, 1},
                                         Coordinate{0, 0}};
    fins.setPoints(closed);
    EXPECT_EQ(fins.getPointCount(), 4U);
    EXPECT_EQ(fins.getLength(), 0.0);
    EXPECT_FALSE(fins.intersects());
}

TEST(FreeformFinSet, SetPointsOfOnePointOrNone)
{
    // Pinned with OpenRocket (ProbeFins S7).
    FreeformFinSet                fins;
    const std::vector<Coordinate> single{Coordinate{0.3, 0.2}};
    fins.setPoints(single);
    EXPECT_EQ(fins.getPointCount(), 1U);
    EXPECT_TRUE(fins.getFinPoints().front().exactlyEquals(Coordinate{0.0, 0.0}));
    EXPECT_EQ(fins.getLength(), 0.0);
    EXPECT_EQ(fins.getSpan(), 0.0);
    EXPECT_FALSE(fins.intersects());
    EXPECT_EQ(fins.getPlanformArea(), 0.0);

    // Java: an IndexOutOfBoundsException.
    EXPECT_THROW(fins.setPoints({}), BugError);
    EXPECT_EQ(fins.getPointCount(), 1U);
}

TEST(FreeformFinSet, IndexChecksOnAnOutlineOfOnePoint)
{
    // Pinned with OpenRocket (ProbeFix S1): the only point is the first and the last.
    FreeformFinSet                fins;
    const std::vector<Coordinate> single{Coordinate{0.3, 0.2}};
    fins.setPoints(single);
    ASSERT_EQ(fins.getPointCount(), 1U);

    constexpr std::string_view kAddMessage =
        "Cannot add new point before the first or after the last point";
    for (const std::size_t index : {std::size_t{0}, std::size_t{1}, std::size_t{2}})
    {
        expectIllegalFinPoint(fins.addPoint(index, Point2D{0.1, 0.1}), kAddMessage, index);
    }
    expectIllegalFinPoint(fins.removePoint(0), "cannot remove first or last point", 0);
    expectIllegalFinPoint(fins.removePoint(1), "index out of range", 1);
    expectIllegalFinPoint(fins.removePoint(2), "index out of range", 2);
    for (const std::size_t index : kIndicesFromNegatives)
    {
        expectIllegalFinPoint(fins.addPoint(index, Point2D{0.1, 0.1}), kAddMessage, index);
        expectIllegalFinPoint(fins.removePoint(index), "index out of range", index);
    }
    EXPECT_EQ(fins.getPointCount(), 1U);
}

TEST(FreeformFinSet, SpanCountsALastPointBelowTheRoot)
{
    // Pinned with OpenRocket (ProbeFins S7).
    FreeformFinSet                fins;
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.5, 1},
                                         Coordinate{1, -0.25}};
    fins.setPoints(points);
    EXPECT_EQ(fins.getSpan(), 1.25);
    EXPECT_EQ(fins.getLength(), 1.0);
}

TEST_F(TemplateRocket, SetPointsFiresWhetherOrNotItKeepsThePoints)
{
    FreeformFinSet& fins = createFinOnTube();
    recordEvents();

    const std::vector<Coordinate> wider{Coordinate{0, 0}, Coordinate{0.5, 1.5}, Coordinate{1, 1.5},
                                        Coordinate{1, 0}};
    fins.setPoints(wider);
    const std::vector<Coordinate> crossing{Coordinate{0, 0}, Coordinate{1, 1}, Coordinate{0, 1},
                                           Coordinate{1, 0}};
    fins.setPoints(crossing);
    EXPECT_EQ(takeEvents(), (std::vector<int>{ComponentChangeEvent::kAeromassChange,
                                              ComponentChangeEvent::kAeromassChange}));
    expectPointsNear(wider, fins.getFinPoints(), 1e-15, "the previous points");
}

TEST_F(TemplateRocket, SetPointsCanLeaveTheTabAlone)
{
    FreeformFinSet& fins = createFinOnTube();  // 1 m long
    fins.setTabLength(0.8);
    fins.setTabHeight(0.1);
    fins.setTabOffsetMethod(AxialMethod::TOP);
    fins.setTabOffset(0.1);
    EXPECT_NEAR(fins.getTabTrailingEdge(), 0.9, 1e-15);

    // Half as long, without validation: the tab now ends behind the fin.
    const std::vector<Coordinate> shorter{Coordinate{0, 0}, Coordinate{0.25, 1}, Coordinate{0.5, 1},
                                          Coordinate{0.5, 0}};
    fins.setPoints(shorter, false);
    EXPECT_NEAR(fins.getLength(), 0.5, 1e-15);
    EXPECT_NEAR(fins.getTabLength(), 0.8, 1e-15);

    // With validation (the default, and what the next change event does) it is shortened.
    const std::vector<Coordinate> shortest{Coordinate{0, 0}, Coordinate{0.25, 1},
                                           Coordinate{0.4, 1}, Coordinate{0.4, 0}};
    fins.setPoints(shortest);
    EXPECT_NEAR(fins.getLength(), 0.4, 1e-15);
    EXPECT_NEAR(fins.getTabLength(), 0.3, 1e-15);
}

TEST(FreeformFinSet, SetPointDoesNothingWithoutAParent)
{
    FreeformFinSet fins;
    EXPECT_TRUE(fins.setPoint(1, 0.03, 0.06).has_value());
    EXPECT_TRUE(fins.getFinPoints()[1].exactlyEquals(Coordinate{0.025, 0.05}));
    // Not even the index is checked (as in OpenRocket).
    EXPECT_TRUE(fins.setPoint(99, 0.03, 0.06).has_value());
}

TEST_F(TemplateRocket, SetPointChecksTheIndex)
{
    FreeformFinSet& fins = createFinOnTube();
    recordEvents();
    const Result<void> beyond = fins.setPoint(4, 0.5, 0.5);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(beyond.error().message, "index out of range");
    EXPECT_TRUE(takeEvents().empty());

    // As Java's negative indices (ProbeFix S1): out of range, and nothing is touched.
    const std::vector<Coordinate> before = fins.getFinPoints();
    for (const std::size_t index : kIndicesFromNegatives)
    {
        expectIllegalFinPoint(fins.setPoint(index, 0.5, 0.5), "index out of range", index);
    }
    expectPointsNear(before, fins.getFinPoints(), 0.0, "after the rejected indices");
    EXPECT_TRUE(takeEvents().empty());

    // A point put where it already is fires nothing.
    EXPECT_TRUE(fins.setPoint(1, 0.5, 1.0).has_value());
    EXPECT_TRUE(takeEvents().empty());

    EXPECT_TRUE(fins.setPoint(1, 0.4, 1.0).has_value());
    EXPECT_EQ(takeEvents(), std::vector<int>{ComponentChangeEvent::kAeromassChange});
}

// Pinned with OpenRocket (ProbeFins S8): a default freeform fin set at the end of a tube 0.2 m
// long with a radius of 0.012 m.
class FreeformOnSmallTube : public ::testing::Test
{
protected:
    FreeformOnSmallTube()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body = &stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
        m_rocket.enableEvents();
        m_fins = &m_body->addChild(std::make_unique<FreeformFinSet>());
    }

    Rocket          m_rocket;
    BodyTube*       m_body{nullptr};
    FreeformFinSet* m_fins{nullptr};
};

TEST_F(FreeformOnSmallTube, GeometryAndMassMatchOpenRocket)
{
    const FreeformFinSet& fins = *m_fins;
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(fins.getAxialOffset(), 0.0);
    EXPECT_NEAR(fins.getPosition().x, 0.15000000000000002, 1e-16);
    EXPECT_NEAR(fins.getPlanformArea(), 0.0024999999999999996, 1e-17);
    EXPECT_NEAR(fins.getComponentVolume(), 2.2499999999999998E-5, 1e-19);
    EXPECT_NEAR(fins.getComponentMass(), 0.015299999999999998, 1e-16);
    EXPECT_NEAR(fins.getComponentCG().x, 0.0375, 1e-16);
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 9.969999999999996E-4, 1e-17);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.0015773333333333325, 1e-17);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.025, 0.05}, Coordinate{0.075, 0.05},
                      Coordinate{0.04999999999999999, 0.0}},
                     fins.getFinPoints(), 1e-16, "fin");
    const std::vector<Coordinate> bounds = fins.getComponentBounds();
    ASSERT_EQ(bounds.size(), 2U);
    EXPECT_NEAR(bounds[1].x, 0.29500000000000004, 1e-15);
    EXPECT_NEAR(bounds[1].y, 0.062, 1e-16);
}

TEST_F(FreeformOnSmallTube, SetPointLimitsAndClampsAsOpenRocketDoes)
{
    FreeformFinSet& fins = *m_fins;

    // An interior point is limited to 2.5 m in x and y.
    EXPECT_TRUE(fins.setPoint(1, 3.0, 4.0).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{2.5, 2.5}, Coordinate{0.075, 0.05},
                      Coordinate{0.04999999999999999, 0.0}},
                     fins.getFinPoints(), 1e-15, "after setPoint(1, 3, 4)");

    // The first point: a request that would make the fin longer than 2.5 m is limited (to
    // 2.5 - the last x, as OpenRocket computes it), and the fin cannot start behind its end.
    EXPECT_TRUE(fins.setPoint(0, -5.0, 0.0).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{2.45, 2.5},
                      Coordinate{0.024999999999999994, 0.05}, Coordinate{0.0, 0.0}},
                     fins.getFinPoints(), 1e-15, "after setPoint(0, -5, 0)");
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(fins.getAxialOffset(), 0.0);
    EXPECT_NEAR(fins.getPosition().x, 0.2, 1e-15);
    EXPECT_EQ(fins.getLength(), 0.0);

    // A move of the last point that makes the outline cross itself is taken back, but what the
    // clamping changed meanwhile (the length, the offset) stays, as in OpenRocket.
    EXPECT_TRUE(fins.setPoint(3, 1.0, 0.7).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{2.45, 2.5},
                      Coordinate{0.024999999999999994, 0.05}, Coordinate{1.0, 0.0}},
                     fins.getFinPoints(), 1e-15, "after setPoint(3, 1, 0.7)");
    EXPECT_EQ(fins.getAxialOffset(), 1.0);
    EXPECT_NEAR(fins.getPosition().x, 0.19999999999999996, 1e-15);
    EXPECT_EQ(fins.getLength(), 1.0);
}

TEST_F(FreeformOnSmallTube, InteriorPointsBehindTheBodyAreNotClamped)
{
    FreeformFinSet& fins = *m_fins;
    // The fin ends with the tube; x = 0.1 is behind it, so the point may go below the root.
    EXPECT_TRUE(fins.setPoint(1, 0.1, -0.01).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, -0.01}, Coordinate{0.075, 0.05},
                      Coordinate{0.04999999999999999, 0.0}},
                     fins.getFinPoints(), 1e-15, "behind the body");
    EXPECT_TRUE(fins.setPoint(2, 0.0, 0.02).has_value());
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.1, -0.01}, Coordinate{0.0, 0.02},
                      Coordinate{0.04999999999999999, 0.0}},
                     fins.getFinPoints(), 1e-15, "moved forwards");
    EXPECT_NEAR(fins.getLength(), 0.05, 1e-15);
    EXPECT_EQ(fins.getAxialOffset(), 0.0);

    // A point above the body that lies inside it is lifted onto its surface.
    EXPECT_TRUE(fins.setPoint(2, 0.03, -0.004).has_value());
    EXPECT_NEAR(fins.getFinPoints()[2].y, 0.0, 1e-15);
}

TEST_F(FreeformOnSmallTube, CopiesHaveTheirOwnPoints)
{
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.01, 0.03},
                                         Coordinate{0.02, 0.04}, Coordinate{0.03, 0.03},
                                         Coordinate{0.04, 0}};
    m_fins->setPoints(points);

    const std::unique_ptr<RocketComponent> copied = m_fins->copyWithOriginalId();
    auto*                                  copy   = dynamic_cast<FreeformFinSet*>(copied.get());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getPointCount(), 5U);
    EXPECT_EQ(copy->getId(), m_fins->getId());
    EXPECT_NEAR(copy->getLength(), 0.04, 1e-16);
    // Detached, so the points are the raw ones.
    expectPointsNear(points, copy->getFinPoints(), 1e-16, "copy");

    EXPECT_TRUE(copy->removePoint(2).has_value());
    EXPECT_EQ(copy->getPointCount(), 4U);
    EXPECT_EQ(m_fins->getPointCount(), 5U);
}

// ============================================================================ convertFinSet

// Pinned with OpenRocket (ProbeFins S6): a single canted fin at 1.1 rad with a tab and fillets,
// first of two fin sets on the tube.
class ConvertInRocket : public ::testing::Test
{
protected:
    ConvertInRocket()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body = &stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));

        auto finset = std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05);
        finset->setThickness(0.0032);
        finset->setAxialMethod(AxialMethod::BOTTOM);
        finset->setName("3 Fin Set");
        m_fins = &m_body->addChild(std::move(finset));
        m_rocket.enableEvents();
        m_extra = &m_body->addChild(std::make_unique<EllipticalFinSet>());

        m_fins->setFinCount(1);
        m_fins->setBaseRotation(1.1);
        m_fins->setCantAngle(0.02);
        m_fins->setTabHeight(0.004);
        m_fins->setTabLength(0.02);
        m_fins->setTabOffsetMethod(AxialMethod::BOTTOM);
        m_fins->setTabOffset(-0.015);
        m_fins->setFilletRadius(0.002);
        m_fins->setAngleMethod(AngleMethod::FIXED);
        m_fins->setName("Trapezoidal Fin Set of mine");

        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    BodyTube*                               m_body{nullptr};
    TrapezoidFinSet*                        m_fins{nullptr};
    EllipticalFinSet*                       m_extra{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST_F(ConvertInRocket, TheFreeformFinSetTakesTheOldOnesPlace)
{
    const QtRocket::Uuid id = m_fins->getId();
    EXPECT_NEAR(m_fins->getComponentMass(), 0.004572496832598701, 1e-15);

    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(*m_fins);

    // The old fin set left the tree and is handed back; the new one is where it was.
    ASSERT_NE(result.freeform, nullptr);
    EXPECT_EQ(result.detached, nullptr);
    ASSERT_NE(result.original, nullptr);
    EXPECT_EQ(result.original.get(), m_fins);
    EXPECT_EQ(m_fins->getParent(), nullptr);
    ASSERT_EQ(m_body->getChildCount(), 2U);
    EXPECT_EQ(&m_body->getChild(0), result.freeform);
    EXPECT_EQ(&m_body->getChild(1), m_extra);
    EXPECT_EQ(result.freeform->getParent(), m_body);

    // One event for the whole conversion: the rocket was frozen meanwhile.
    ASSERT_EQ(m_types.size(), 1U);
    EXPECT_EQ(m_types.front(),
              ComponentChangeEvent::kTreeChange | ComponentChangeEvent::kAeromassChange);

    const FreeformFinSet& free = *result.freeform;
    EXPECT_EQ(free.getId(), id) << "the id is carried over";
    EXPECT_EQ(free.getName(), "Freeform Fin Set of mine");
    EXPECT_EQ(free.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(free.getAxialOffset(), 0.0);
    EXPECT_NEAR(free.getPosition().x, 0.15000000000000002, 1e-16);
    EXPECT_EQ(free.getFinCount(), 1);
    EXPECT_EQ(free.getBaseRotation(), 1.1);
    EXPECT_EQ(free.getCantAngle(), 0.02);
    EXPECT_EQ(free.getThickness(), 0.0032);
    EXPECT_EQ(free.getTabHeight(), 0.004);
    EXPECT_EQ(free.getTabLength(), 0.02);
    EXPECT_EQ(free.getTabOffsetMethod(), AxialMethod::BOTTOM);
    EXPECT_NEAR(free.getTabOffset(), -0.015, 1e-16);
    EXPECT_NEAR(free.getTabFrontEdge(), 0.015000000000000003, 1e-16);

    // Kept from OpenRocket: copyFrom() leaves out the fillets, the angle method, the stored
    // tab offset and the base rotation matrix, so the single fin's CG is not rotated.
    EXPECT_EQ(free.getFilletRadius(), 0.0);
    EXPECT_EQ(free.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(free.getTabPosition(AxialMethod::TOP), 0.0);
    EXPECT_NEAR(free.getTabPosition(AxialMethod::BOTTOM), 0.030000000000000002, 1e-16);
    EXPECT_NEAR(free.getPlanformArea(), 0.0020000703038678173, 1e-15);
    EXPECT_NEAR(free.getComponentVolume(), 6.656211641718338E-6, 1e-18);
    EXPECT_NEAR(free.getComponentMass(), 0.00452622391636847, 1e-15);
    const Coordinate cg = free.getComponentCG();
    EXPECT_NEAR(cg.x, 0.029407865425041372, 1e-14);
    EXPECT_NEAR(cg.y, 0.033956800161849016, 1e-14);
    EXPECT_EQ(cg.z, 0.0);

    // The outline is the trapezoid's, on the canted root.
    expectPointsNear(
        {Coordinate{0.0, -1.0419801696325004E-5}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
         Coordinate{0.04999999999999999, -1.0419801696325004E-5}},
        free.getFinPoints(), 1e-14, "fin");
    EXPECT_EQ(free.getRootPoints().size(), 21U);
}

TEST_F(ConvertInRocket, WithoutFreezingEveryStepFires)
{
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(*m_fins, false);
    ASSERT_NE(result.freeform, nullptr);
    EXPECT_FALSE(m_types.empty());
    for (const int type : m_types)
    {
        EXPECT_NE(type & ComponentChangeEvent::kTreeChange, 0);
    }
    EXPECT_EQ(m_types.size(), 2U) << "taken out, put in";
    EXPECT_EQ(&m_body->getChild(0), result.freeform);
}

TEST_F(ConvertInRocket, TheLastChildStaysTheLast)
{
    const QtRocket::Uuid id = m_extra->getId();
    m_extra->setAppearance(Appearance{Color{10, 20, 30}, 0.5});
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(*m_extra);
    ASSERT_NE(result.freeform, nullptr);
    ASSERT_EQ(m_body->getChildCount(), 2U);
    EXPECT_EQ(&m_body->getChild(0), m_fins);
    EXPECT_EQ(&m_body->getChild(1), result.freeform);
    EXPECT_EQ(result.freeform->getId(), id);
    EXPECT_EQ(result.freeform->getName(), "Freeform Fin Set");
    EXPECT_EQ(result.freeform->getPointCount(), 31U);
    EXPECT_EQ(result.freeform->getFinCount(), 3);
    ASSERT_TRUE(result.freeform->getAppearance().has_value());
    EXPECT_EQ(result.freeform->getAppearance(), m_extra->getAppearance());
    EXPECT_NEAR(result.freeform->getLength(), 0.05, 1e-15);
}

TEST_F(ConvertInRocket, AFrozenRocketIsABugAndNothingChanges)
{
    m_rocket.freeze();
    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(*m_fins)), BugError);

    // Still frozen by its owner, and the fin set where it was.
    EXPECT_TRUE(m_rocket.isFrozen());
    ASSERT_EQ(m_body->getChildCount(), 2U);
    EXPECT_EQ(&m_body->getChild(0), m_fins);
    EXPECT_EQ(m_fins->getParent(), m_body);
    m_rocket.thaw();
    EXPECT_TRUE(m_types.empty());

    // Without freezing, the conversion goes ahead inside the caller's freeze.
    m_rocket.freeze();
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(*m_fins, false);
    EXPECT_EQ(&m_body->getChild(0), result.freeform);
    EXPECT_TRUE(m_types.empty());
    m_rocket.thaw();
    EXPECT_EQ(m_types.size(), 1U);
}

/// A rocket (events disabled, as a freshly made one) whose tube carries a freeform fin set
/// between two others; the freeform one is positioned TOP and has had its first point set to
/// x = NaN, which OpenRocket accepts and which leaves it with an axial offset of NaN.
class ConvertFailure : public ::testing::Test
{
protected:
    ConvertFailure()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_tube      = &stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
        m_first     = &m_tube->addChild(std::make_unique<TrapezoidFinSet>());
        m_fins      = &m_tube->addChild(std::make_unique<FreeformFinSet>());
        m_last      = &m_tube->addChild(std::make_unique<EllipticalFinSet>());
        m_fins->setAxialMethod(AxialMethod::TOP);
        m_fins->setName("Freeform Fin Set of mine");
    }

    /// The tube's children, in order.
    [[nodiscard]] std::vector<const RocketComponent*> children() const
    {
        std::vector<const RocketComponent*> children;
        children.reserve(m_tube->getChildCount());
        for (std::size_t i = 0; i < m_tube->getChildCount(); ++i)
        {
            children.push_back(&m_tube->getChild(i));
        }
        return children;
    }

    /// The fin set is where it was, alive and unchanged, and the rocket is not frozen.
    void expectFinSetBackInPlace() const
    {
        EXPECT_EQ(children(), (std::vector<const RocketComponent*>{m_first, m_fins, m_last}));
        EXPECT_EQ(m_fins->getParent(), m_tube);
        EXPECT_FALSE(m_rocket.isFrozen());
        EXPECT_EQ(m_fins->getName(), "Freeform Fin Set of mine");
        EXPECT_EQ(m_fins->getPointCount(), 4U);
        EXPECT_TRUE(std::isnan(m_fins->getAxialOffset()));
    }

    Rocket            m_rocket;
    BodyTube*         m_tube{nullptr};
    TrapezoidFinSet*  m_first{nullptr};
    FreeformFinSet*   m_fins{nullptr};
    EllipticalFinSet* m_last{nullptr};
};

// OpenRocket (ProbeFix S2): the conversion throws a BugException ("setAxialOffset is broken --
// attempted to update as NaN") and leaves the fin set out of the tree, a detached object its
// caller still holds. Here it would die with the lost result, so it is put back instead.
TEST_F(ConvertFailure, AFailedConversionPutsTheFinSetBack)
{
    EXPECT_TRUE(m_fins->setPoint(0, std::numeric_limits<double>::quiet_NaN(), 0.0).has_value());
    ASSERT_TRUE(std::isnan(m_fins->getAxialOffset()));
    ASSERT_TRUE(std::isnan(m_fins->getPosition().x));

    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(*m_fins)), BugError);
    expectFinSetBackInPlace();

    // The same without freezing the rocket.
    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(*m_fins, false)), BugError);
    expectFinSetBackInPlace();

    // Its neighbours still convert, each in its place.
    const FreeformFinSet::ConversionResult first = FreeformFinSet::convertFinSet(*m_first);
    const FreeformFinSet::ConversionResult last  = FreeformFinSet::convertFinSet(*m_last);
    EXPECT_EQ(children(),
              (std::vector<const RocketComponent*>{first.freeform, m_fins, last.freeform}));
}

/// A fin set without an outline, which no freeform fin set can take over (setPoints() refuses
/// an empty list): converting it fails once it is out of the tree.
class FinSetWithoutPoints : public TrapezoidFinSet
{
public:
    [[nodiscard]] std::vector<Coordinate> getFinPoints() const override { return {}; }
};

TEST_F(ConvertInRocket, AFailedConversionFiresTheTreeEventsOnly)
{
    FinSetWithoutPoints& broken = m_body->addChild(std::make_unique<FinSetWithoutPoints>(), 1);
    m_types.clear();

    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(broken)), BugError);
    ASSERT_EQ(m_body->getChildCount(), 3U);
    EXPECT_EQ(&m_body->getChild(0), m_fins);
    EXPECT_EQ(&m_body->getChild(1), &broken);
    EXPECT_EQ(&m_body->getChild(2), m_extra);
    EXPECT_EQ(broken.getParent(), m_body);
    EXPECT_FALSE(m_rocket.isFrozen());
    // Taken out and put back while the rocket was frozen: one combined event.
    ASSERT_EQ(m_types.size(), 1U);
    EXPECT_NE(m_types.front() & ComponentChangeEvent::kTreeChange, 0);

    // Without freezing: one event for each.
    m_types.clear();
    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(broken, false)), BugError);
    EXPECT_EQ(&m_body->getChild(1), &broken);
    EXPECT_EQ(m_types.size(), 2U);
}

TEST(FreeformFinSet, AFailedConversionOfADetachedFinSetLeavesItAlone)
{
    // Nothing was taken out of a tree, so there is nothing to put back.
    FinSetWithoutPoints lone;
    EXPECT_THROW(static_cast<void>(FreeformFinSet::convertFinSet(lone)), BugError);
    EXPECT_EQ(lone.getParent(), nullptr);
    EXPECT_EQ(lone.getFinCount(), 3);
}

TEST(FreeformFinSet, ConvertADetachedDefaultFinSet)
{
    // Pinned with OpenRocket (ProbeFins S6).
    TrapezoidFinSet                        lone;
    const FreeformFinSet::ConversionResult result = FreeformFinSet::convertFinSet(lone);
    ASSERT_NE(result.detached, nullptr);
    EXPECT_EQ(result.detached.get(), result.freeform);
    EXPECT_EQ(result.original, nullptr);
    const FreeformFinSet& free = *result.detached;
    EXPECT_EQ(free.getName(), "Freeform Fin Set");
    EXPECT_EQ(free.getId(), lone.getId());
    EXPECT_EQ(free.getPointCount(), 4U);
    EXPECT_EQ(free.getLength(), 0.05);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.025, 0.03},
                      Coordinate{0.07500000000000001, 0.03}, Coordinate{0.05, 0.0}},
                     free.getFinPoints(), 0.0, "outline");
    EXPECT_EQ(free.getParent(), nullptr);

    // A name that does not start with the component name is kept; converting a freeform fin
    // set gives a freeform copy.
    FreeformFinSet named;
    named.setName("My Trapezoidal Fin Set");
    const FreeformFinSet::ConversionResult again = FreeformFinSet::convertFinSet(named);
    ASSERT_NE(again.detached, nullptr);
    EXPECT_EQ(again.detached->getName(), "My Trapezoidal Fin Set");
    EXPECT_NE(again.detached.get(), &named);
}

}  // namespace
