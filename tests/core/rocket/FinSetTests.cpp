#include "QtRocket/rocket/FinSet.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/AxialOffsetSupport.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialPositionable;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::BoxBounded;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::FinCrossSection;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::InsideColorComponent;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::RadiusMethod;
using QtRocket::RingInstanceable;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::MathUtil::javaToDegrees;
using QtRocket::MathUtil::javaToRadians;
using QtRocket::Test::setAxialOffset;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;

/// FinSetTest's tolerance.
constexpr double kEpsilon = 1.0E-8;

/// The tolerance for the values pinned with OpenRocket (they pass through sin, cos and friends,
/// whose last bit differs between math libraries).
constexpr double kPinned = 1e-14;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// @p actual within @p tolerance of @p expected in x, y, z and the weight.
void expectNear(const Coordinate& expected, const Coordinate& actual, double tolerance,
                std::string_view what)
{
    EXPECT_NEAR(expected.x, actual.x, tolerance) << what << " x";
    EXPECT_NEAR(expected.y, actual.y, tolerance) << what << " y";
    EXPECT_NEAR(expected.z, actual.z, tolerance) << what << " z";
    EXPECT_NEAR(expected.weight, actual.weight, tolerance) << what << " weight";
}

/// @p actual has the points @p expected, each within @p tolerance.
void expectPointsNear(const std::vector<Coordinate>& expected,
                      const std::vector<Coordinate>& actual, double tolerance,
                      std::string_view what)
{
    ASSERT_EQ(expected.size(), actual.size()) << what;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        expectNear(expected[i], actual[i], tolerance,
                   std::string{what} + " point " + std::to_string(i));
    }
}

/// Whether the y of every point of @p points is NaN.
[[nodiscard]] bool everyYIsNaN(const std::vector<Coordinate>& points)
{
    return std::ranges::all_of(points, [](const Coordinate& point) { return std::isnan(point.y); });
}

/// FinSetTest.createSimpleFin(): one trapezoidal fin (root 0.06, tip 0.02, sweep 0.02, height
/// 0.05), 5 mm thick, of density 1, with a 0.02 m tab 0.02 m behind the front (TOP), rotated by
/// 90 degrees (FIXED) and canted by 3 degrees.
std::unique_ptr<TrapezoidFinSet> createSimpleFin()
{
    auto fins = std::make_unique<TrapezoidFinSet>(1, 0.06, 0.02, 0.02, 0.05);
    fins->setName("test fins");
    setAxialOffset(*fins, AxialMethod::MIDDLE, 0.0);
    fins->setMaterial(Material::newMaterial(Material::Type::BULK, "Fin-Test-Material", 1.0, true));
    fins->setThickness(0.005);  // == 5 mm

    fins->setTabLength(0.02);
    fins->setTabOffsetMethod(AxialMethod::TOP);
    fins->setTabOffset(0.02);

    fins->setFilletRadius(0.0);

    fins->setAngleMethod(AngleMethod::FIXED);
    fins->setAngleOffset(javaToRadians(90.0));

    fins->setCantAngle(javaToRadians(3.0));

    return fins;
}

/// The nose cone, body tube and fin set of TestRockets.makeEstesAlphaIII() in a rocket with one
/// stage; events enabled and recorded. It is the rocket of the OpenRocket programs that computed
/// the values pinned in the suite's tests, which have no JUnit counterpart. (FinSetTest's own
/// case on makeEstesAlphaIII(), testTabSetLength, runs on the whole rocket: TestEstesAlphaIII
/// of TestRockets.h.)
class FinSetOnAlpha : public ::testing::Test
{
protected:
    FinSetOnAlpha()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_stage->setName("Stage");

        auto nosecone = std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012);
        nosecone->setAftShoulderLength(0.02);
        nosecone->setAftShoulderThickness(0);
        nosecone->setAftShoulderRadius(0.011);
        nosecone->setName("Nose Cone");
        m_nose = &m_stage->addChild(std::move(nosecone));

        auto bodytube = std::make_unique<BodyTube>(0.20, 0.012, 0.0003);
        bodytube->setName("Body Tube");
        m_body = &m_stage->addChild(std::move(bodytube));

        auto finset = std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.02, 0.05);
        finset->setThickness(0.0032);
        finset->setAxialMethod(AxialMethod::BOTTOM);
        finset->setName("3 Fin Set");
        m_fins = &m_body->addChild(std::move(finset));

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
    AxialStage*                             m_stage{nullptr};
    NoseCone*                               m_nose{nullptr};
    BodyTube*                               m_body{nullptr};
    TrapezoidFinSet*                        m_fins{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

constexpr int kNonFunctional = ComponentChangeEvent::kNonFunctionalChange;
constexpr int kMass          = ComponentChangeEvent::kMassChange;
constexpr int kAerodynamic   = ComponentChangeEvent::kAerodynamicChange;
constexpr int kBoth          = ComponentChangeEvent::kBothChange;

// ============================================================================== FinSetTest

TEST(FinSet, Multiplicity)
{
    const EllipticalFinSet fins;
    EXPECT_EQ(3, fins.getFinCount());

    const FreeformFinSet freeFins;
    EXPECT_EQ(3, freeFins.getFinCount());

    const TrapezoidFinSet trapFins;
    EXPECT_EQ(3, trapFins.getFinCount());
}

TEST(FinSet, AngleOffset)
{
    const std::unique_ptr<TrapezoidFinSet> fins = createSimpleFin();

    EXPECT_NEAR(std::numbers::pi / 2, fins->getAngleOffset(), kEpsilon)
        << "Angle Offset Doesn't match!";
    EXPECT_NEAR(90.0, javaToDegrees(fins->getAngleOffset()), kEpsilon)
        << "Angle Offset Doesn't match!";

    EXPECT_NEAR(std::numbers::pi / 60, fins->getCantAngle(), kEpsilon)
        << "Cant angle doesn't match!";
    EXPECT_NEAR(3.0, javaToDegrees(fins->getCantAngle()), kEpsilon) << "Cant angle doesn't match!";
}

TEST(FinSet, TabLocation)
{
    const std::unique_ptr<TrapezoidFinSet> fins = createSimpleFin();
    EXPECT_NEAR(0.06, fins->getLength(), kEpsilon) << "incorrect fin length:";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "incorrect fin tab length:";

    const double                       expFront = 0.02;
    const std::array<double, 4>        expShift{0.02, 0.02, 0.0, -0.02};
    const std::span<const double>      shifts{expShift};
    const std::span<const AxialMethod> methods{QtRocket::kAxialOffsetMethods};
    for (std::size_t caseIndex = 0; caseIndex < methods.size(); ++caseIndex)
    {
        const double actFront = fins->getTabFrontEdge();
        EXPECT_NEAR(expFront, actFront, kEpsilon) << " Front edge doesn't match!";

        // update
        fins->setTabOffsetMethod(methods[caseIndex]);

        // query
        const double actShift = fins->getTabOffset();
        EXPECT_NEAR(shifts[caseIndex], actShift, kEpsilon)
            << "Offset doesn't match for: " << QtRocket::axialMethodName(methods[caseIndex]);
    }
}

TEST(FinSet, TabGetAs)
{
    const std::unique_ptr<TrapezoidFinSet> fins = createSimpleFin();
    EXPECT_NEAR(0.06, fins->getLength(), kEpsilon) << "incorrect fin length:";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "incorrect fin tab length:";

    // TOP -> native(TOP)
    fins->setTabOffsetMethod(AxialMethod::TOP);
    fins->setTabOffset(0.0);

    EXPECT_NEAR(0.0, fins->getTabFrontEdge(), kEpsilon) << "Setting by TOP method failed!";
    EXPECT_NEAR(0.0, fins->getTabOffset(), kEpsilon) << "Setting by TOP method failed!";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "Setting by TOP method failed!";

    // MIDDLE -> native
    fins->setTabOffsetMethod(AxialMethod::MIDDLE);
    fins->setTabOffset(0.0);
    EXPECT_NEAR(0.02, fins->getTabFrontEdge(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.0, fins->getTabOffset(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "Setting by MIDDLE method failed!";

    // BOTTOM -> native
    fins->setTabOffsetMethod(AxialMethod::BOTTOM);
    fins->setTabOffset(0.0);

    EXPECT_NEAR(0.04, fins->getTabFrontEdge(), kEpsilon) << "Setting by BOTTOM method failed!";
    EXPECT_NEAR(0.0, fins->getTabOffset(), kEpsilon) << "Setting by BOTTOM method failed!";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "Setting by BOTTOM method failed!";
}

// On TestRockets.makeEstesAlphaIII() itself (the shared TestEstesAlphaIII), as in Java.
TEST(FinSet, TabSetLength)
{
    const TestEstesAlphaIII alpha;

    auto& body = dynamic_cast<BodyTube&>(alpha.rocket->getChild(0).getChild(1));
    EXPECT_NEAR(0.20, body.getLength(), kEpsilon) << "incorrect body tube length:";

    auto& fins = dynamic_cast<FinSet&>(body.getChild(0));
    fins.setTabHeight(0.01);
    fins.setTabLength(0.02);
    EXPECT_NEAR(0.05, fins.getLength(), kEpsilon) << "incorrect fin length:";
    EXPECT_NEAR(0.01, fins.getTabHeight(), kEpsilon) << "incorrect fin tab height:";
    EXPECT_NEAR(0.02, fins.getTabLength(), kEpsilon) << "incorrect fin tab length:";
    EXPECT_NEAR(0.015, fins.getTabFrontEdge(), kEpsilon) << "incorrect fin location";

    // MIDDLE -> native
    fins.setTabOffsetMethod(AxialMethod::MIDDLE);
    fins.setTabOffset(0.0);

    EXPECT_NEAR(0.015, fins.getTabFrontEdge(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.0, fins.getTabOffset(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.02, fins.getTabLength(), kEpsilon) << "Setting by MIDDLE method failed!";

    fins.setTabLength(0.04);

    EXPECT_NEAR(0.005, fins.getTabFrontEdge(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.0, fins.getTabOffset(), kEpsilon) << "Setting by MIDDLE method failed!";
    EXPECT_NEAR(0.04, fins.getTabLength(), kEpsilon) << "Setting by MIDDLE method failed!";
}

TEST(FinSet, TabLocationUpdate)
{
    const std::unique_ptr<TrapezoidFinSet> fins = createSimpleFin();
    EXPECT_NEAR(0.06, fins->getLength(), kEpsilon) << "incorrect fin length:";
    EXPECT_NEAR(0.02, fins->getTabLength(), kEpsilon) << "incorrect fin tab length:";

    // TOP -> native(TOP)
    fins->setTabOffsetMethod(AxialMethod::MIDDLE);
    fins->setTabOffset(0.0);

    EXPECT_NEAR(0.0, fins->getTabOffset(), kEpsilon) << "Setting by TOP method failed!";
    EXPECT_NEAR(0.02, fins->getTabFrontEdge(), kEpsilon) << "Setting by TOP method failed!";

    fins->setRootChord(0.08);

    EXPECT_NEAR(0.0, fins->getTabOffset(), kEpsilon)
        << "Offset doesn't match after adjusting root chord....";
    EXPECT_NEAR(0.03, fins->getTabFrontEdge(), kEpsilon)
        << "Front edge doesn't match after adjusting root chord...";
}

TEST(FinSet, AreaCalculationsSingleIncrement)
{
    //    [1] +
    //       /|
    //      / |
    // [0] +--+ [2]
    //    [3]
    const std::vector<Coordinate> basicPoints{Coordinate{0.00, 0.0}, Coordinate{0.06, 0.06},
                                              Coordinate{0.06, 0.0}, Coordinate{0.00, 0.0}};

    const double     expArea     = 0.06 * 0.06 * 0.5;
    const Coordinate actCentroid = FinSet::calculateCurveIntegral(basicPoints);
    EXPECT_NEAR(expArea, actCentroid.weight, kEpsilon) << " basic area doesn't match...";
    EXPECT_NEAR(0.04, actCentroid.x, 1.0e-8) << " basic centroid x doesn't match: ";
    EXPECT_NEAR(0.02, actCentroid.y, 1.0e-8) << " basic centroid y doesn't match: ";
}

TEST(FinSet, AreaCalculationsDoubleIncrement)
{
    //    [1] +
    //       / \ .
    //      /   \ .
    // [0] +-----+ [2]
    //    [3]
    const std::vector<Coordinate> basicPoints{Coordinate{0.00, 0.0}, Coordinate{0.06, 0.06},
                                              Coordinate{0.12, 0.0}, Coordinate{0.00, 0.0}};

    const double     expArea     = 0.06 * 0.12 * 0.5;
    const Coordinate actCentroid = FinSet::calculateCurveIntegral(basicPoints);
    EXPECT_NEAR(expArea, actCentroid.weight, kEpsilon) << " basic area doesn't match...";
    EXPECT_NEAR(0.06, actCentroid.x, 1.0e-8) << " basic centroid x doesn't match: ";
    EXPECT_NEAR(0.02, actCentroid.y, 1.0e-8) << " basic centroid y doesn't match: ";
}

TEST(FinSet, AreaCalculations)
{
    //   [1] +--+ [2]
    //      /    \ .
    //     /      \ .
    // [0] +--------+ [3]
    //    [4]
    const std::vector<Coordinate> basicPoints{Coordinate{0.00, 0.0}, Coordinate{0.02, 0.05},
                                              Coordinate{0.04, 0.05}, Coordinate{0.06, 0.0},
                                              Coordinate{0.00, 0.0}};
    const double                  expArea     = 0.04 * 0.05;
    const Coordinate              actCentroid = FinSet::calculateCurveIntegral(basicPoints);
    EXPECT_NEAR(expArea, actCentroid.weight, kEpsilon) << " basic area doesn't match...";
    EXPECT_NEAR(0.03000, actCentroid.x, 1.0e-8) << " basic centroid x doesn't match: ";
    EXPECT_NEAR(0.020833333, actCentroid.y, 1.0e-8) << " basic centroid y doesn't match: ";
}

TEST(FinSet, FinInstanceAngles)
{
    const std::unique_ptr<TrapezoidFinSet> fins = createSimpleFin();
    fins->setBaseRotation(std::numbers::pi / 6);  // == 30d
    fins->setInstanceCount(3);                    // == 120d between each fin
                                                  // => [ 30, 150, 270 ]
                                                  // => PI*[ 1/6, 5/6, 9/6 ]
                                                  // => [ .523, 2.61, 4.71 ]

    const std::vector<double> instanceAngles = fins->getInstanceAngles();
    EXPECT_NEAR((1.0 / 6.0) * std::numbers::pi, fins->getBaseRotation(), kEpsilon);
    EXPECT_NEAR((1.0 / 6.0) * std::numbers::pi, fins->getAngleOffset(), kEpsilon);

    ASSERT_EQ(instanceAngles.size(), 3U);
    EXPECT_NEAR((1.0 / 6.0) * std::numbers::pi, instanceAngles[0], kEpsilon);
    EXPECT_NEAR((5.0 / 6.0) * std::numbers::pi, instanceAngles[1], kEpsilon);
    EXPECT_NEAR((9.0 / 6.0) * std::numbers::pi, instanceAngles[2], kEpsilon);
}

TEST(FinSet, GenerateContinuousFinAndTabShape)
{
    BodyTube parent;
    FinSet&  fins = parent.addChild(createSimpleFin());
    // (Java clears the cant before adding the fin set; neither has events to react to.)
    fins.setCantAngle(0);
    std::vector<Coordinate>       finShapeContinuous = fins.generateContinuousFinAndTabShape();
    const std::vector<Coordinate> finShape           = fins.getFinPointsWithRoot();

    ASSERT_EQ(finShape.size(), finShapeContinuous.size()) << "incorrect fin shape length";

    EXPECT_EQ(finShape, finShapeContinuous) << "incorrect fin shape";

    // Set the tab
    fins.setTabHeight(0.02);

    finShapeContinuous = fins.generateContinuousFinAndTabShape();

    ASSERT_EQ(finShape.size() + 3, finShapeContinuous.size()) << "incorrect fin shape length";

    // (The Java test compares the first points one by one in a loop.)
    const std::span<const Coordinate> unchanged = std::span{finShape}.first(finShape.size() - 2);
    const std::span<const Coordinate> head = std::span{finShapeContinuous}.first(unchanged.size());
    EXPECT_EQ((std::vector<Coordinate>{unchanged.begin(), unchanged.end()}),
              (std::vector<Coordinate>{head.begin(), head.end()}))
        << "incorrect fin shape points";

    std::size_t idx = finShape.size() - 2;
    EXPECT_EQ((Coordinate{0.04, 0.0}), finShapeContinuous[idx]) << "incorrect fin shape point";
    idx++;
    EXPECT_EQ((Coordinate{0.04, -0.02}), finShapeContinuous[idx]) << "incorrect fin shape point";
    idx++;
    EXPECT_EQ((Coordinate{0.02, -0.02}), finShapeContinuous[idx]) << "incorrect fin shape point";
    idx++;
    EXPECT_EQ((Coordinate{0.02, 0.0}), finShapeContinuous[idx]) << "incorrect fin shape point";
    idx++;
    EXPECT_EQ((Coordinate{0.0, 0.0}), finShapeContinuous[idx]) << "incorrect fin shape point";
}

// ========================================================================== FinCrossSection

TEST(FinCrossSection, NamesAndRelativeVolumes)
{
    EXPECT_EQ(QtRocket::kAllFinCrossSections.size(), 3U);

    EXPECT_EQ(QtRocket::relativeVolume(FinCrossSection::SQUARE), 1.00);
    EXPECT_EQ(QtRocket::relativeVolume(FinCrossSection::ROUNDED), 0.99);
    EXPECT_EQ(QtRocket::relativeVolume(FinCrossSection::AIRFOIL), 0.85);

    EXPECT_EQ(QtRocket::finCrossSectionName(FinCrossSection::SQUARE), "SQUARE");
    EXPECT_EQ(QtRocket::finCrossSectionName(FinCrossSection::ROUNDED), "ROUNDED");
    EXPECT_EQ(QtRocket::finCrossSectionName(FinCrossSection::AIRFOIL), "AIRFOIL");

    // FinSetSaver: getCrossSection().name().toLowerCase(Locale.ENGLISH)
    EXPECT_EQ(QtRocket::orkName(FinCrossSection::SQUARE), "square");
    EXPECT_EQ(QtRocket::orkName(FinCrossSection::ROUNDED), "rounded");
    EXPECT_EQ(QtRocket::orkName(FinCrossSection::AIRFOIL), "airfoil");

    EXPECT_EQ(QtRocket::displayKey(FinCrossSection::SQUARE), "FinSet.CrossSection.SQUARE");
    EXPECT_EQ(QtRocket::displayKey(FinCrossSection::ROUNDED), "FinSet.CrossSection.ROUNDED");
    EXPECT_EQ(QtRocket::displayKey(FinCrossSection::AIRFOIL), "FinSet.CrossSection.AIRFOIL");
    EXPECT_EQ(QtRocket::displayName(FinCrossSection::SQUARE), "Square");
    EXPECT_EQ(QtRocket::displayName(FinCrossSection::ROUNDED), "Rounded");
    EXPECT_EQ(QtRocket::displayName(FinCrossSection::AIRFOIL), "Airfoil");

    // The nested name is the same type.
    EXPECT_EQ(FinSet::CrossSection::AIRFOIL, FinCrossSection::AIRFOIL);
}

// Pinned with OpenRocket (ProbeFix S8): DocumentConfig.findEnum(text, FinSet.CrossSection.class).
TEST(FinCrossSection, FromOrkNameMatchesAsTheLoaderDoes)
{
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("square"), FinCrossSection::SQUARE);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("rounded"), FinCrossSection::ROUNDED);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("airfoil"), FinCrossSection::AIRFOIL);
    // DocumentConfig.findEnum() trims the text and compares exactly.
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("  airfoil \n"), FinCrossSection::AIRFOIL);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName(" square"), FinCrossSection::SQUARE);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("Airfoil"), std::nullopt);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("ROUNDED"), std::nullopt);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName(""), std::nullopt);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("round"), std::nullopt);
    EXPECT_EQ(QtRocket::finCrossSectionFromOrkName("air_foil"), std::nullopt);
}

TEST(FinCrossSection, TheLoaderReadsWhatTheSaverWrites)
{
    for (const FinCrossSection crossSection : QtRocket::kAllFinCrossSections)
    {
        EXPECT_EQ(QtRocket::finCrossSectionFromOrkName(QtRocket::orkName(crossSection)),
                  crossSection);
    }
}

// ============================================================================= a new fin set

TEST(FinSet, DefaultsOfANewFinSet)
{
    const TrapezoidFinSet fins;

    EXPECT_EQ(fins.getFinCount(), 3);
    EXPECT_EQ(fins.getInstanceCount(), 3);
    EXPECT_EQ(fins.getThickness(), 0.003);
    EXPECT_EQ(fins.getCrossSection(), FinSet::CrossSection::SQUARE);
    EXPECT_EQ(fins.getCantAngle(), 0.0);
    EXPECT_EQ(fins.getBaseRotation(), 0.0);
    EXPECT_EQ(fins.getAngleOffset(), 0.0);
    EXPECT_EQ(fins.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(fins.getRadiusMethod(), RadiusMethod::SURFACE);
    EXPECT_EQ(fins.getRadiusOffset(), 0.0);
    EXPECT_EQ(fins.getBoundingRadius(), 0.0);
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(fins.getAxialOffset(), 0.0);

    EXPECT_EQ(fins.getTabHeight(), 0.0);
    EXPECT_EQ(fins.getTabLength(), 0.05);
    EXPECT_EQ(fins.getTabOffsetMethod(), AxialMethod::MIDDLE);
    EXPECT_EQ(fins.getTabOffset(), 0.0);
    EXPECT_EQ(fins.getTabFrontEdge(), 0.0);
    EXPECT_EQ(fins.getTabTrailingEdge(), 0.05);
    EXPECT_FALSE(fins.hasTab());
    EXPECT_TRUE(fins.isTabTrivial());
    EXPECT_FALSE(fins.isTabBeyondFin());

    EXPECT_EQ(fins.getFilletRadius(), 0.0);
    // The built-in default, as OpenRocket's test preferences give (and the goldens show).
    EXPECT_EQ(fins.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(fins.getFilletMaterial().getName(), "Cardboard");
    EXPECT_EQ(fins.getFilletMaterial(), fins.getMaterial());
    EXPECT_EQ(fins.getFilletMaterial().getDensity(), 680.0);

    // Drawn in front of the bodies.
    EXPECT_EQ(fins.getDisplayOrderSide(), 4);
    EXPECT_EQ(fins.getDisplayOrderBack(), 4);

    EXPECT_FALSE(fins.isAfter());
    EXPECT_FALSE(fins.allowsChildren());
    EXPECT_TRUE(fins.isAerodynamic());
    EXPECT_TRUE(fins.isMassive());
    EXPECT_EQ(fins.getPatternName(), "3-fin-ring");
    EXPECT_TRUE(fins.isRootStraight());
    EXPECT_EQ(fins.getPresetType(), std::nullopt);

    // Kept from OpenRocket: no rotation increment until the count changes.
    EXPECT_EQ(fins.getFinRotationTransformation(), Transformation::kIdentity);
    EXPECT_EQ(fins.getCantRotation(), Transformation::kIdentity);
}

/// The kinds @p component accepts as children.
[[nodiscard]] std::vector<ComponentKind> acceptedKinds(const RocketComponent& component)
{
    std::vector<ComponentKind> accepted;
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        if (component.isCompatible(kind))
        {
            accepted.push_back(kind);
        }
    }
    return accepted;
}

TEST(FinSet, NothingCanBeAttached)
{
    const FreeformFinSet fins;
    EXPECT_TRUE(acceptedKinds(fins).empty());
    EXPECT_TRUE(acceptedKinds(TrapezoidFinSet{}).empty());
    EXPECT_TRUE(acceptedKinds(EllipticalFinSet{}).empty());
    const TrapezoidFinSet other;
    EXPECT_FALSE(fins.isCompatible(other));

    FreeformFinSet parent;
    auto           child = std::make_unique<TestComponent>(ComponentKind::MASS_COMPONENT);
    EXPECT_THROW(parent.addChild(std::move(child)), BugError);
}

TEST(FinSet, ParentsAcceptFinSetsAsInOpenRocket)
{
    // BodyTube: every external component that is not a body component.
    const BodyTube tube;
    EXPECT_TRUE(tube.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::ELLIPTICAL_FIN_SET));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::FREEFORM_FIN_SET));

    // Transition (and so NoseCone): freeform fin sets only.
    const Transition transition;
    EXPECT_FALSE(transition.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(transition.isCompatible(ComponentKind::ELLIPTICAL_FIN_SET));
    EXPECT_TRUE(transition.isCompatible(ComponentKind::FREEFORM_FIN_SET));
    const NoseCone nose;
    EXPECT_FALSE(nose.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(nose.isCompatible(ComponentKind::ELLIPTICAL_FIN_SET));
    EXPECT_TRUE(nose.isCompatible(ComponentKind::FREEFORM_FIN_SET));

    // Stages take body components only.
    const AxialStage stage;
    EXPECT_FALSE(stage.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(stage.isCompatible(ComponentKind::FREEFORM_FIN_SET));
}

TEST(FinSet, IsEveryInterfaceOpenRocketGivesIt)
{
    TrapezoidFinSet  fins;
    RocketComponent& component = fins;
    EXPECT_NE(dynamic_cast<AxialPositionable*>(&component), nullptr);
    EXPECT_NE(dynamic_cast<BoxBounded*>(&component), nullptr);
    EXPECT_NE(dynamic_cast<RingInstanceable*>(&component), nullptr);
    EXPECT_NE(dynamic_cast<InsideColorComponent*>(&component), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::ExternalComponent*>(&component), nullptr);

    EXPECT_TRUE(QtRocket::isFinSet(fins.kind()));
    EXPECT_EQ(TrapezoidFinSet{}.kind(), ComponentKind::TRAPEZOID_FIN_SET);
    EXPECT_EQ(EllipticalFinSet{}.kind(), ComponentKind::ELLIPTICAL_FIN_SET);
    EXPECT_EQ(FreeformFinSet{}.kind(), ComponentKind::FREEFORM_FIN_SET);
    EXPECT_EQ(TrapezoidFinSet{}.getComponentName(), "Trapezoidal Fin Set");
    EXPECT_EQ(EllipticalFinSet{}.getComponentName(), "Elliptical Fin Set");
    EXPECT_EQ(FreeformFinSet{}.getComponentName(), "Freeform Fin Set");
    EXPECT_EQ(TrapezoidFinSet{}.getName(), "Trapezoidal Fin Set");

    // Through the interfaces: the axial position is RocketComponent's.
    auto* axial = dynamic_cast<AxialPositionable*>(&component);
    ASSERT_NE(axial, nullptr);
    axial->setAxialMethod(AxialMethod::TOP);
    axial->setAxialOffset(0.25);
    EXPECT_EQ(axial->getAxialMethod(), AxialMethod::TOP);
    EXPECT_EQ(axial->getAxialOffset(), 0.25);
    EXPECT_EQ(fins.getAxialOffset(AxialMethod::TOP), 0.25);
}

// ================================================================================== setters

TEST_F(FinSetOnAlpha, FinCountIsLimitedAndFires)
{
    m_fins->setFinCount(3);
    EXPECT_TRUE(takeEvents().empty()) << "the current count";

    m_fins->setFinCount(4);
    EXPECT_EQ(m_fins->getFinCount(), 4);
    EXPECT_EQ(takeEvents(), std::vector<int>{kBoth});
    EXPECT_NEAR(m_fins->getInstanceAngleIncrement(), std::numbers::pi / 2, kPinned);
    expectNear(Coordinate{0, 0, 1},
               m_fins->getFinRotationTransformation().transform(Coordinate{0, 1, 0}), kPinned,
               "a quarter turn");

    m_fins->setFinCount(0);
    EXPECT_EQ(m_fins->getFinCount(), 1);
    m_fins->setFinCount(-7);
    EXPECT_EQ(m_fins->getFinCount(), 1) << "limited to the current count: still an event";
    EXPECT_EQ(takeEvents(), (std::vector<int>{kBoth, kBoth}));

    m_fins->setInstanceCount(100);
    EXPECT_EQ(m_fins->getFinCount(), 8);
    EXPECT_EQ(m_fins->getInstanceCount(), 8);
    EXPECT_EQ(m_fins->getPatternName(), "8-fin-ring");
    EXPECT_EQ(m_fins->getInstanceAngles().size(), 8U);
    EXPECT_EQ(m_fins->getInstanceOffsets().size(), 8U);
    EXPECT_EQ(m_fins->getInstanceLocations().size(), 8U);
}

TEST_F(FinSetOnAlpha, CantIsLimitedAndFiresAerodynamic)
{
    m_fins->setCantAngle(0.1);
    EXPECT_EQ(m_fins->getCantAngle(), 0.1);
    EXPECT_EQ(takeEvents(), std::vector<int>{kAerodynamic});

    m_fins->setCantAngle(0.1 + 1e-10);
    EXPECT_TRUE(takeEvents().empty()) << "equal within MathUtil's tolerance";
    EXPECT_EQ(m_fins->getCantAngle(), 0.1);

    m_fins->setCantAngle(1.0);
    EXPECT_EQ(m_fins->getCantAngle(), FinSet::kMaxCantRadians);
    EXPECT_NEAR(m_fins->getCantAngle(), 0.2617993877991494, 1e-16);
    m_fins->setCantAngle(-1.0);
    EXPECT_EQ(m_fins->getCantAngle(), -FinSet::kMaxCantRadians);
    EXPECT_EQ(takeEvents(), (std::vector<int>{kAerodynamic, kAerodynamic}));

    // The cached rotation follows the event.
    m_fins->setCantAngle(javaToRadians(5));
    expectNear(Coordinate{0.9961946980917455, 0.0, -0.08715574274765817},
               m_fins->getCantRotation().transform(Coordinate{1, 0, 0}), kPinned, "cant rotation");
    m_fins->setCantAngle(0);
    EXPECT_EQ(m_fins->getCantRotation(), Transformation::kIdentity);
}

TEST(FinSet, CantRotationOfADetachedFinSetIsNotRefreshed)
{
    // As in OpenRocket: the cache is cleared by change events, which a detached component does
    // not receive.
    TrapezoidFinSet fins;
    EXPECT_EQ(fins.getCantRotation(), Transformation::kIdentity);
    fins.setCantAngle(0.1);
    EXPECT_EQ(fins.getCantRotation(), Transformation::kIdentity);
}

TEST_F(FinSetOnAlpha, ThicknessAndCrossSection)
{
    m_fins->setThickness(0.0032);
    EXPECT_TRUE(takeEvents().empty());

    m_fins->setThickness(0.004);
    EXPECT_EQ(m_fins->getThickness(), 0.004);
    m_fins->setThickness(-1);
    EXPECT_EQ(m_fins->getThickness(), 0.0);
    EXPECT_EQ(takeEvents(), (std::vector<int>{kBoth, kBoth}));
    // Java's Math.max(NaN, 0) is NaN.
    m_fins->setThickness(kNaN);
    EXPECT_TRUE(std::isnan(m_fins->getThickness()));
    m_fins->setThickness(0.0032);
    static_cast<void>(takeEvents());

    m_fins->setCrossSection(FinSet::CrossSection::SQUARE);
    EXPECT_TRUE(takeEvents().empty());
    m_fins->setCrossSection(FinSet::CrossSection::AIRFOIL);
    EXPECT_EQ(m_fins->getCrossSection(), FinSet::CrossSection::AIRFOIL);
    EXPECT_EQ(takeEvents(), std::vector<int>{kBoth});
    EXPECT_NEAR(m_fins->getComponentVolume(), 1.9200000000000003E-5 * 0.85, 1e-18);
}

TEST_F(FinSetOnAlpha, AngleOffsetIsReducedAndFires)
{
    m_fins->setAngleOffset(0);
    EXPECT_TRUE(takeEvents().empty());

    m_fins->setBaseRotation(0.3);
    EXPECT_EQ(m_fins->getAngleOffset(), 0.3);
    EXPECT_EQ(m_fins->getBaseRotation(), 0.3);
    EXPECT_EQ(takeEvents(), std::vector<int>{kBoth});

    // Reduced to -pi ... pi.
    m_fins->setAngleOffset((2 * std::numbers::pi) + 0.5);
    EXPECT_NEAR(m_fins->getAngleOffset(), 0.5, 1e-15);
    m_fins->setAngleOffset(std::numbers::pi + 0.5);
    EXPECT_NEAR(m_fins->getAngleOffset(), 0.5 - std::numbers::pi, 1e-15);
    static_cast<void>(takeEvents());

    // The angle method is stored and always fires; it does not change the angles.
    m_fins->setAngleMethod(AngleMethod::RELATIVE);
    m_fins->setAngleMethod(AngleMethod::MIRROR_XY);
    EXPECT_EQ(m_fins->getAngleMethod(), AngleMethod::MIRROR_XY);
    EXPECT_EQ(takeEvents(), (std::vector<int>{kBoth, kBoth}));

    // The radius is fixed to the parent's surface.
    m_fins->setRadiusMethod(RadiusMethod::FREE);
    m_fins->setRadiusOffset(0.5);
    m_fins->setRadius(RadiusMethod::RELATIVE, 0.25);
    EXPECT_EQ(m_fins->getRadiusMethod(), RadiusMethod::SURFACE);
    EXPECT_EQ(m_fins->getRadiusOffset(), 0.0);
    EXPECT_TRUE(takeEvents().empty());
}

TEST(FinSet, InstanceAnglesAreReducedToAFullTurn)
{
    TrapezoidFinSet fins;
    fins.setFinCount(4);
    fins.setBaseRotation(-0.25);
    const std::vector<double> angles = fins.getInstanceAngles();
    ASSERT_EQ(angles.size(), 4U);
    EXPECT_NEAR(angles[0], (2 * std::numbers::pi) - 0.25, 1e-15);
    EXPECT_NEAR(angles[1], (std::numbers::pi / 2) - 0.25, 1e-15);
    EXPECT_NEAR(angles[2], std::numbers::pi - 0.25, 1e-15);
    EXPECT_NEAR(angles[3], (1.5 * std::numbers::pi) - 0.25, 1e-15);
    EXPECT_NEAR(fins.getInstanceAngleIncrement(), std::numbers::pi / 2, 1e-16);
}

// ================================================================================== the tab

TEST_F(FinSetOnAlpha, TabSettersFollowOpenRocket)
{
    // Pinned with OpenRocket (ProbeFins S1b).
    m_fins->setTabHeight(0.005);
    m_fins->setTabLength(0.02);
    EXPECT_EQ(takeEvents(), (std::vector<int>{kMass, kMass}));
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    EXPECT_EQ(takeEvents(), std::vector<int>{kNonFunctional});
    m_fins->setTabOffset(0.01);
    m_fins->setTabOffset(0.01);
    EXPECT_EQ(takeEvents(), (std::vector<int>{kMass, kMass})) << "setTabOffset() always fires";

    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.01, 1e-17);
    EXPECT_NEAR(m_fins->getTabTrailingEdge(), 0.03, 1e-17);
    EXPECT_NEAR(m_fins->getTabOffset(), 0.01, 1e-17);
    EXPECT_NEAR(m_fins->getTabOffset(AxialMethod::MIDDLE), -0.005, 1e-17);
    EXPECT_NEAR(m_fins->getTabOffset(AxialMethod::BOTTOM), -0.02, 1e-17);
    EXPECT_NEAR(m_fins->getTabPosition(AxialMethod::BOTTOM), 0.04, 1e-17);
    EXPECT_TRUE(m_fins->hasTab());
    EXPECT_FALSE(m_fins->isTabTrivial());
    EXPECT_FALSE(m_fins->isTabBeyondFin());

    // The height is limited to the parent's radius; unchanged values fire nothing.
    m_fins->setTabHeight(1.0);
    EXPECT_EQ(m_fins->getTabHeight(), 0.012);
    static_cast<void>(takeEvents());
    m_fins->setTabHeight(0.012);
    m_fins->setTabLength(0.02);
    EXPECT_TRUE(takeEvents().empty());

    // Kept from OpenRocket: a negative request is stored as it is.
    m_fins->setTabHeight(-0.5);
    EXPECT_EQ(m_fins->getTabHeight(), -0.5);
    m_fins->setTabLength(-0.25);
    EXPECT_EQ(m_fins->getTabLength(), -0.25);
    EXPECT_NEAR(m_fins->getTabFrontEdge(), 0.01, 1e-17);
    EXPECT_FALSE(m_fins->hasTab());
    EXPECT_FALSE(m_fins->isTabTrivial()) << "the product of the two negative values is positive";

    m_fins->validateFinTabLength();
    EXPECT_EQ(m_fins->getTabLength(), 0.0);
    EXPECT_TRUE(m_fins->isTabTrivial());
}

TEST_F(FinSetOnAlpha, TabValidation)
{
    // Pinned with OpenRocket (ProbeFins S1b).
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    m_fins->setTabLength(0.2);
    m_fins->setTabOffset(0.03);
    static_cast<void>(takeEvents());
    m_fins->validateFinTabLength();
    EXPECT_NEAR(m_fins->getTabLength(), 0.020000000000000018, 1e-17);
    EXPECT_TRUE(takeEvents().empty()) << "validation fires nothing";

    m_fins->setTabOffset(-0.01);
    m_fins->validateFinTabPosition();
    EXPECT_EQ(m_fins->getTabFrontEdge(), 0.0);
    m_fins->setTabOffset(0.09);
    m_fins->validateFinTabPosition();
    EXPECT_EQ(m_fins->getTabFrontEdge(), 0.05);
    EXPECT_FALSE(m_fins->hasTab());
    EXPECT_FALSE(m_fins->isTabBeyondFin()) << "no tab, so not beyond the fin";

    // A tab that starts at the fin's end is not beyond it; one that starts behind it is.
    m_fins->setTabHeight(0.004);
    EXPECT_TRUE(m_fins->hasTab());
    EXPECT_FALSE(m_fins->isTabBeyondFin());
    m_fins->setTabOffset(0.06);
    EXPECT_TRUE(m_fins->isTabBeyondFin());
    EXPECT_EQ(m_fins->generateContinuousFinAndTabShape(), m_fins->getFinPointsWithRoot());
    EXPECT_EQ(m_fins->generateContinuousFinAndTabShape().size(), 6U);

    // Entirely ahead of the fin.
    m_fins->setTabOffset(-0.05);
    EXPECT_TRUE(m_fins->isTabBeyondFin());
}

TEST(FinSet, SetTabLengthCanLeaveThePositionAlone)
{
    TrapezoidFinSet fins;  // length 0.05, tab length 0.05, MIDDLE
    fins.setTabLength(0.01, false);
    EXPECT_EQ(fins.getTabLength(), 0.01);
    EXPECT_EQ(fins.getTabFrontEdge(), 0.0) << "not recomputed";
    EXPECT_NEAR(fins.getTabOffset(), -0.02, 1e-17);

    fins.updateTabPosition();
    EXPECT_NEAR(fins.getTabFrontEdge(), 0.02, 1e-17);
    EXPECT_NEAR(fins.getTabOffset(), 0.0, 1e-17);

    // Without validation the height is taken as given.
    fins.setTabHeight(7.5, false);
    EXPECT_EQ(fins.getTabHeight(), 7.5);
}

TEST_F(FinSetOnAlpha, ParentRadiiAndMaxTabHeight)
{
    EXPECT_EQ(m_fins->getParentFrontRadius(), std::optional<double>{0.012});
    EXPECT_EQ(m_fins->getParentTrailingRadius(), std::optional<double>{0.012});
    EXPECT_EQ(m_fins->getMaxTabHeight(), 0.012);
    EXPECT_EQ(m_fins->getBodyRadius(), 0.012);
    expectNear(Coordinate{0.15000000000000002, 0.012}, m_fins->getFinFront(), 1e-17, "fin front");

    // Another candidate parent: its radius where the tab would be, the fin staying where it is.
    EXPECT_NEAR(m_fins->getParentFrontRadius(m_nose).value_or(kNaN), 0.012, 1e-17)
        << "behind the nose cone's end: its aft radius";
    EXPECT_EQ(m_fins->getParentFrontRadius(m_stage), std::nullopt) << "not a body";
    EXPECT_EQ(m_fins->getParentTrailingRadius(m_stage), std::nullopt);
    EXPECT_EQ(m_fins->getParentFrontRadius(nullptr), std::nullopt);
    EXPECT_EQ(m_fins->getParentTrailingRadius(nullptr), std::nullopt);

    const TrapezoidFinSet lonely;
    EXPECT_EQ(lonely.getParentFrontRadius(), std::nullopt);
    EXPECT_EQ(lonely.getParentTrailingRadius(), std::nullopt);
    EXPECT_EQ(lonely.getMaxTabHeight(), std::numeric_limits<double>::max());
    EXPECT_EQ(lonely.getBodyRadius(), 0.0);
    EXPECT_EQ(lonely.getFinFront(), Coordinate::kZero);
}

// ================================================================================== fillets

TEST_F(FinSetOnAlpha, FilletSetters)
{
    const Material fillet = Material::newMaterial(Material::Type::BULK, "Fillet", 1100.0, true);
    m_fins->setFilletMaterial(fillet);
    EXPECT_EQ(m_fins->getFilletMaterial(), fillet);
    m_fins->setFilletMaterial(fillet);
    EXPECT_EQ(takeEvents(), std::vector<int>{kMass}) << "an equal material fires nothing";

    m_fins->setFilletRadius(0.004);
    EXPECT_EQ(m_fins->getFilletRadius(), 0.004);
    m_fins->setFilletRadius(0.004);
    EXPECT_EQ(takeEvents(), std::vector<int>{kMass});

    const Material surface = Material::newMaterial(Material::Type::SURFACE, "Cloth", 0.1, true);
    EXPECT_THROW(m_fins->setFilletMaterial(surface), BugError);
    EXPECT_EQ(m_fins->getFilletMaterial(), fillet);

    // The fillet material is not one of the component's materials (as in OpenRocket).
    EXPECT_EQ(m_fins->getAllMaterials(), std::vector<Material>{m_fins->getMaterial()});
}

TEST(FinSet, NoFilletVolumeWithoutARadiusOrABody)
{
    TrapezoidFinSet fins;
    EXPECT_EQ(fins.calculateFilletVolumeCentroid(), Coordinate::kZero);
    fins.setFilletRadius(0.01);
    EXPECT_EQ(fins.calculateFilletVolumeCentroid(), Coordinate::kZero) << "no parent";

    BodyTube tube(0.2, 0.05);
    FinSet&  attached = tube.addChild(std::make_unique<TrapezoidFinSet>());
    EXPECT_EQ(attached.calculateFilletVolumeCentroid(), Coordinate::kZero) << "no radius";
    attached.setFilletRadius(0.01);
    EXPECT_GT(attached.calculateFilletVolumeCentroid().weight, 0.0);
}

TEST_F(FinSetOnAlpha, ApplyDefaultMaterialSetsTheFilletMaterialToo)
{
    // SwingPreferences.loadDefaultComponentMaterials() makes Balsa the default of every FinSet;
    // Java's FinSet constructor reads the fillet material from the same preference.
    QtRocket::InMemoryPreferences prefs;
    QtRocket::MaterialStorage     storage;
    QtRocket::addBuiltinMaterials(storage);
    QtRocket::loadDefaultComponentMaterials(prefs, storage);

    // The mass of the fins and of 3 mm fillets is computed, and so cached, before the materials
    // change (the values are pinned with OpenRocket, ProbeFix S7).
    m_fins->setFilletRadius(0.003);
    static_cast<void>(takeEvents());
    EXPECT_EQ(m_fins->getMaterial().getName(), "Cardboard");
    EXPECT_NEAR(m_fins->getComponentMass(), 0.013338573570469348, kPinned);

    QtRocket::ExternalComponent& external = *m_fins;
    external.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(m_fins->getMaterial().getName(), "Balsa");
    EXPECT_EQ(m_fins->getFilletMaterial().getName(), "Balsa");
    EXPECT_EQ(m_fins->getFilletMaterial(), m_fins->getMaterial());
    EXPECT_TRUE(takeEvents().empty()) << "assigned as the constructor does";

    // No event follows, yet the mass is that of the new materials at once.
    EXPECT_NEAR(m_fins->getComponentMass(), 0.003334643392617337, kPinned);
    expectNear(Coordinate{0.029486236829137525, 0.0, 0.0, 0.003334643392617337},
               m_fins->getComponentCG(), kPinned, "CG in balsa");
    EXPECT_NEAR(m_fins->getPlanformArea(), 0.002, 1e-17);

    // A default for the concrete class comes first, for both.
    const Material custom =
        Material::newMaterial(Material::Type::BULK, "Plywood (test)", 630, true);
    QtRocket::setDefaultComponentMaterial(prefs, "TrapezoidFinSet", custom);
    m_fins->applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(m_fins->getMaterial().getName(), "Plywood (test)");
    EXPECT_EQ(m_fins->getFilletMaterial().getName(), "Plywood (test)");
    EXPECT_NEAR(m_fins->getComponentMass(), 0.012357796102052482, kPinned);
    EXPECT_TRUE(takeEvents().empty());
    EllipticalFinSet other;
    other.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(other.getMaterial().getName(), "Balsa");
    EXPECT_EQ(other.getFilletMaterial().getName(), "Balsa");
}

// ================================================================== geometry, mass, inertia

// Pinned with OpenRocket (ProbeFins S1): the Alpha III fin set.
TEST_F(FinSetOnAlpha, GeometryAndMassMatchOpenRocket)
{
    const FinSet& fins = *m_fins;
    EXPECT_EQ(fins.getSpan(), 0.05);
    EXPECT_NEAR(fins.getPlanformArea(), 0.002, 1e-17);
    EXPECT_NEAR(fins.getComponentVolume(), 1.9200000000000003E-5, 1e-19);
    EXPECT_NEAR(fins.getComponentMass(), 0.013056, 1e-16);
    expectNear(Coordinate{0.029583333333333336, 0.0, 0.0, 0.013056}, fins.getComponentCG(), 1e-16,
               "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 8.403281572999748E-4, 1e-17);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.0013473229812666163, 1e-17);
    EXPECT_NEAR(fins.getMass(), 0.013056, 1e-16);

    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
                      Coordinate{0.04999999999999999, 0.0}},
                     fins.getFinPoints(), 1e-16, "fin");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.04999999999999999, 0.0}},
                     fins.getRootPoints(), 1e-16, "root");
    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{0.0, 0.0}, Coordinate{0.05, 0.0}, Coordinate{0.05, 0.0}},
        fins.getTabPoints(), 1e-16, "tab");
    expectPointsNear({Coordinate{0.0, 0.012}, Coordinate{0.2, 0.012}}, fins.getMountPoints(), 1e-16,
                     "mount");
    EXPECT_TRUE(fins.getTabPointsWithRoot().empty());
    EXPECT_TRUE(fins.getTabPointsWithRootLowRes().empty());

    expectPointsNear(
        {Coordinate{0.0, 0.012, 0.0}, Coordinate{0.0, -0.0059999999999999975, 0.010392304845413265},
         Coordinate{0.0, -0.006000000000000005, -0.01039230484541326}},
        fins.getInstanceOffsets(), kPinned, "instance offset");
    const std::vector<double> angles = fins.getInstanceAngles();
    ASSERT_EQ(angles.size(), 3U);
    EXPECT_EQ(angles[0], 0.0);
    EXPECT_NEAR(angles[1], 2.0943951023931953, kPinned);
    EXPECT_NEAR(angles[2], 4.1887902047863905, kPinned);

    expectPointsNear({Coordinate{0.0, -0.062, -0.062}, Coordinate{0.27, 0.062, 0.062}},
                     fins.getComponentBounds(), 1e-15, "bound");
    const BoundingBox box = fins.getInstanceBoundingBox();
    expectNear(Coordinate{0.0, 0.0, -0.0016}, box.min(), 1e-17, "box min");
    expectNear(Coordinate{0.05, 0.05, 0.0016}, box.max(), 1e-17, "box max");
}

// Pinned with OpenRocket (ProbeFins S1b): the same with a tab 0.01 m behind the front.
TEST_F(FinSetOnAlpha, TabGeometryAndMassMatchOpenRocket)
{
    m_fins->setTabHeight(0.005);
    m_fins->setTabLength(0.02);
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    m_fins->setTabOffset(0.01);
    const FinSet& fins = *m_fins;

    EXPECT_NEAR(fins.getPlanformArea(), 0.002, 1e-17) << "the tab is not part of the area";
    EXPECT_NEAR(fins.getComponentVolume(), 2.0160000000000003E-5, 1e-19);
    EXPECT_NEAR(fins.getComponentMass(), 0.0137088, 1e-16);
    expectNear(Coordinate{0.02912698412698413, 0.0, 0.0, 0.0137088}, fins.getComponentCG(), 1e-16,
               "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 8.403281572999748E-4, 1e-17);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.0013473229812666163, 1e-17);

    const std::vector<Coordinate> tab{Coordinate{0.01, 0.0}, Coordinate{0.01, -0.005},
                                      Coordinate{0.03, -0.005}, Coordinate{0.03, 0.0}};
    expectPointsNear(tab, fins.getTabPoints(), 1e-17, "tab");
    std::vector<Coordinate> closed = tab;
    closed.emplace_back(0.01, 0.0);
    expectPointsNear(closed, fins.getTabPointsWithRoot(), 1e-17, "tab with root");
    expectPointsNear(closed, fins.getTabPointsWithRootLowRes(), 1e-17, "tab with low-res root");

    expectPointsNear(
        {Coordinate{0.0, 0.0}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
         Coordinate{0.04999999999999999, 0.0}, Coordinate{0.03, 0.0}, Coordinate{0.03, -0.005},
         Coordinate{0.01, -0.005}, Coordinate{0.01, 0.0}, Coordinate{0.0, 0.0}},
        fins.generateContinuousFinAndTabShape(), 1e-16, "continuous");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
                      Coordinate{0.04999999999999999, 0.0}, Coordinate{0.04999999999999999, 0.0},
                      Coordinate{0.0, 0.0}},
                     fins.getFinPointsWithRoot(), 1e-16, "fin with root");
    EXPECT_EQ(fins.getFinPointsWithLowResRoot(), fins.getFinPointsWithRoot());
}

/// The middle of the canted root (ProbeFins S2) and its ends.
void expectCantedRoot(const std::vector<Coordinate>& root)
{
    ASSERT_EQ(root.size(), 21U);
    expectNear(Coordinate{0.0, -1.9947362121862725E-4}, root[0], kPinned, "root 0");
    expectNear(Coordinate{0.0025000000000000022, -1.6131500202766838E-4}, root[1], kPinned,
               "root 1");
    expectNear(Coordinate{0.0050000000000000044, -1.2727703505040038E-4}, root[2], kPinned,
               "root 2");
    expectNear(Coordinate{0.025000000000000022, 0.0}, root[10], kPinned, "root 10");
    expectNear(Coordinate{0.04750000000000004, -1.6131500202766838E-4}, root[19], kPinned,
               "root 19");
    expectNear(Coordinate{0.04999999999999999, -1.9947362121862725E-4}, root[20], kPinned,
               "root 20");
}

// Pinned with OpenRocket (ProbeFins S2): four airfoil fins canted by 5 degrees from 0.3 rad.
TEST_F(FinSetOnAlpha, CantedFinsMatchOpenRocket)
{
    m_fins->setFinCount(4);
    m_fins->setBaseRotation(0.3);
    m_fins->setCantAngle(javaToRadians(5));
    m_fins->setCrossSection(FinSet::CrossSection::AIRFOIL);
    const FinSet& fins = *m_fins;

    EXPECT_NEAR(fins.getPlanformArea(), 0.0020013354551141564, 1e-15);
    EXPECT_NEAR(fins.getComponentVolume(), 2.1774529751642024E-5, 1e-17);
    EXPECT_NEAR(fins.getComponentMass(), 0.014806680231116576, 1e-14);
    expectNear(Coordinate{0.029598547838164284, 0.0, 0.0, 0.014806680231116576},
               fins.getComponentCG(), 1e-14, "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 8.407515911813872E-4, 1e-15);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.0013479472731770816, 1e-15);

    // The root sinks towards its ends, and the fin's ends follow it.
    expectCantedRoot(fins.getRootPoints());
    expectCantedRoot(fins.getRootPoints(20));
    expectPointsNear(
        {Coordinate{0.0, -1.9947362121862725E-4}, Coordinate{0.02, 0.05}, Coordinate{0.05, 0.05},
         Coordinate{0.04999999999999999, -1.9947362121862725E-4}},
        fins.getFinPoints(), kPinned, "fin");
    EXPECT_EQ(fins.getFinPointsWithRoot().size(), 25U);
    EXPECT_EQ(fins.getFinPointsWithLowResRoot().size(), 25U);
    EXPECT_TRUE(fins.isRootStraight()) << "a statement about the parent's profile only";

    expectPointsNear(
        {Coordinate{9.513254770636068E-5, 0.01082013079179451, 0.005627819012028131},
         Coordinate{9.513254770636068E-5, -0.005627819012028131, 0.01082013079179451},
         Coordinate{9.513254770636068E-5, -0.010820130791794513, -0.005627819012028128},
         Coordinate{9.513254770636068E-5, 0.005627819012028128, -0.010820130791794513}},
        fins.getInstanceOffsets(), kPinned, "instance offset");
    const std::vector<double> angles = fins.getInstanceAngles();
    ASSERT_EQ(angles.size(), 4U);
    EXPECT_NEAR(angles[0], 0.3, kPinned);
    EXPECT_NEAR(angles[1], 1.8707963267948966, kPinned);
    EXPECT_NEAR(angles[2], 3.441592653589793, kPinned);
    EXPECT_NEAR(angles[3], 5.0123889803846895, kPinned);

    expectPointsNear(
        {Coordinate{0.0, -0.062, -0.062}, Coordinate{0.2700951325477064, 0.062, 0.062}},
        fins.getComponentBounds(), kPinned, "bound");
    const BoundingBox box = fins.getInstanceBoundingBox();
    expectNear(Coordinate{0.0, -1.9947362121862725E-4, -0.0016}, box.min(), kPinned, "box min");
    expectNear(Coordinate{0.05, 0.05, 0.0016}, box.max(), kPinned, "box max");
}

// Pinned with OpenRocket (ProbeFins S3): one rounded fin at 1 rad with a tab and fillets.
TEST_F(FinSetOnAlpha, SingleFinWithTabAndFilletsMatchesOpenRocket)
{
    m_fins->setFinCount(1);
    m_fins->setBaseRotation(1.0);
    m_fins->setFilletRadius(0.004);
    m_fins->setFilletMaterial(Material::newMaterial(Material::Type::BULK, "Fillet", 1100.0, true));
    m_fins->setTabHeight(0.005);
    m_fins->setTabLength(0.02);
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    m_fins->setTabOffset(0.01);
    m_fins->setCrossSection(FinSet::CrossSection::ROUNDED);
    const FinSet& fins = *m_fins;

    EXPECT_NEAR(fins.getPlanformArea(), 0.002, 1e-17);
    EXPECT_NEAR(fins.getComponentVolume(), 6.880595982620715E-6, 1e-18);
    EXPECT_NEAR(fins.getComponentMass(), 0.004773135580882786, 1e-15);
    // The CG of a single fin is rotated to the fin.
    expectNear(Coordinate{0.028909212232464808, 0.016987322960005984, 0.02748800621874694,
                          0.004773135580882786},
               fins.getComponentCG(), 1e-14, "CG");
    // A single fin's inertia is about its own centre.
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 2.5E-4, 1e-17);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 1.6666666666666666E-4, 1e-17);
    expectNear(Coordinate{0.024999999999999994, 0.006915869515112188, 0.010770828605541074,
                          2.2459598262071336E-7},
               fins.calculateFilletVolumeCentroid(), 1e-14, "fillet");
    expectPointsNear({Coordinate{0.0, 0.006483627670417678, 0.010097651817694758}},
                     fins.getInstanceOffsets(), kPinned, "instance offset");

    // Three of them: the y of the CG and of the fillets is 0 (the fins balance).
    m_fins->setFinCount(3);
    EXPECT_NEAR(fins.getComponentVolume(), 2.0641787947862144E-5, 1e-17);
    EXPECT_NEAR(fins.getComponentMass(), 0.014319406742648358, 1e-14);
    expectNear(Coordinate{0.028909212232464808, 0.0, 0.0, 0.014319406742648358},
               fins.getComponentCG(), 1e-14, "CG of three");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 8.403281572999748E-4, 1e-16);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.0013473229812666163, 1e-16);
    expectNear(Coordinate{0.024999999999999994, 0.0, 0.0, 2.2459598262071336E-7},
               fins.calculateFilletVolumeCentroid(), 1e-14, "fillets of three");
    expectPointsNear({Coordinate{0.0, 0.006483627670417678, 0.010097651817694758},
                      Coordinate{0.0, -0.011986636827902611, 5.661603624140515E-4},
                      Coordinate{0.0, 0.00550300915748493, -0.01066381218010881}},
                     fins.getInstanceOffsets(), kPinned, "instance offset of three");
}

// Pinned with OpenRocket (ProbeFins S9): a fin set that is not on a body.
TEST(FinSet, DetachedFinSetMatchesOpenRocket)
{
    const TrapezoidFinSet fins;
    EXPECT_NEAR(fins.getPlanformArea(), 0.0015000000000000002, 1e-18);
    EXPECT_NEAR(fins.getComponentVolume(), 1.3500000000000003E-5, 1e-20);
    EXPECT_NEAR(fins.getComponentMass(), 0.009180000000000002, 1e-17);
    expectNear(Coordinate{0.0375, 0.0, 0.0, 0.009180000000000002}, fins.getComponentCG(), 1e-16,
               "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 3.5833333333333344E-4, 1e-18);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 3.0E-4, 1e-18);

    expectPointsNear({Coordinate{0.0, -0.03, -0.03}, Coordinate{0.07500000000000001, 0.03, 0.03}},
                     fins.getComponentBounds(), 1e-17, "bound");
    const BoundingBox box = fins.getInstanceBoundingBox();
    expectNear(Coordinate{0.0, 0.0, -0.0015}, box.min(), 1e-17, "box min");
    expectNear(Coordinate{0.07500000000000001, 0.03, 0.0015}, box.max(), 1e-17, "box max");

    EXPECT_EQ(fins.getInstanceOffsets(), (std::vector<Coordinate>(3, Coordinate::kZero)));
    EXPECT_EQ(fins.getRootPoints(), std::vector<Coordinate>{Coordinate::kZero});
    EXPECT_TRUE(fins.getMountPoints().empty()) << "Java: null";
    EXPECT_EQ(fins.getFinPointsWithRoot().size(), 5U);

    // Without a body the tab has no y.
    const std::vector<Coordinate> tab = fins.getTabPoints();
    ASSERT_EQ(tab.size(), 4U);
    EXPECT_EQ(tab[0].x, 0.0);
    EXPECT_EQ(tab[3].x, 0.05);
    EXPECT_TRUE(everyYIsNaN(tab));
}

TEST(FinSet, InertiaOfAFinWithoutHeightIsThatOfASquareOfItsArea)
{
    // ProbeFins S9: the area is 0 too, so everything is 0.
    const TrapezoidFinSet fins(1, 0.05, 0.03, 0.02, 0.0);
    EXPECT_EQ(fins.getPlanformArea(), 0.0);
    EXPECT_EQ(fins.getLongitudinalUnitInertia(), 0.0);
    EXPECT_EQ(fins.getRotationalUnitInertia(), 0.0);
}

TEST_F(FinSetOnAlpha, CachedValuesFollowTheRocketsEvents)
{
    EXPECT_NEAR(m_fins->getComponentMass(), 0.013056, 1e-16);
    EXPECT_NEAR(m_fins->getPlanformArea(), 0.002, 1e-17);

    // A change of the fin set itself.
    m_fins->setThickness(0.0064);
    EXPECT_NEAR(m_fins->getComponentMass(), 2 * 0.013056, 1e-16);
    EXPECT_NEAR(m_fins->getComponentVolume(), 2 * 1.9200000000000003E-5, 1e-19);

    // A change of another component: the body's radius moves the fin's CG outwards (one fin).
    m_fins->setFinCount(1);
    const double before = m_fins->getComponentCG().y;
    m_body->setOuterRadius(0.02);
    EXPECT_NEAR(m_fins->getComponentCG().y, before + 0.008, 1e-15);
    EXPECT_EQ(m_fins->getBodyRadius(), 0.02);

    // The material.
    m_fins->setMaterial(Material::newMaterial(Material::Type::BULK, "Heavy", 1360.0, true));
    EXPECT_NEAR(m_fins->getComponentMass(), 2 * 2 * 0.013056 / 3, 1e-16);
}

TEST(FinSet, ADetachedFinSetKeepsItsCachedValues)
{
    // As in OpenRocket: without a rocket no event clears the cache.
    TrapezoidFinSet fins;
    EXPECT_NEAR(fins.getPlanformArea(), 0.0015000000000000002, 1e-18);
    fins.setHeight(0.06);
    EXPECT_NEAR(fins.getPlanformArea(), 0.0015000000000000002, 1e-18);
}

// ================================================================================= outlines

TEST(FinSet, CurveIntegralEdgeCases)
{
    // Pinned with OpenRocket (ProbeFins S10).
    EXPECT_TRUE(FinSet::calculateCurveIntegral({}).exactlyEquals(Coordinate::kZero));
    const std::vector<Coordinate> one{Coordinate{1, 2}};
    EXPECT_TRUE(FinSet::calculateCurveIntegral(one).exactlyEquals(Coordinate::kZero));

    // An outline run the other way round: the area is made positive and y negated.
    const std::vector<Coordinate> reversed{Coordinate{0, 0}, Coordinate{0.06, 0},
                                           Coordinate{0.06, 0.06}, Coordinate{0, 0}};
    expectNear(Coordinate{0.04, -0.02, 0.0, 0.0018}, FinSet::calculateCurveIntegral(reversed),
               1e-16, "reversed");

    // A curve below the x axis.
    const std::vector<Coordinate> below{Coordinate{0, 0}, Coordinate{1, -1}, Coordinate{2, 0}};
    expectNear(Coordinate{1.0, 0.3333333333333333, 0.0, 1.0}, FinSet::calculateCurveIntegral(below),
               1e-15, "below");

    // A segment whose area is below the tolerance is skipped.
    const std::vector<Coordinate> tiny{Coordinate{0, 0}, Coordinate{1e-5, 1e-4}, Coordinate{1, 1},
                                       Coordinate{1, 0}};
    expectNear(Coordinate{0.6666366703329667, 0.33333333666633336, 0.0, 0.5000449995},
               FinSet::calculateCurveIntegral(tiny), 1e-15, "tiny");
}

TEST(FinSet, TranslateAndReversePoints)
{
    const std::vector<Coordinate> points{Coordinate{1, 2, 3, 4}, Coordinate{-1, 0.5, 0, 0}};

    // By x and y: unweighted (x, y) coordinates.
    const std::vector<Coordinate> moved = FinSet::translatePoints(points, 0.5, -1);
    ASSERT_EQ(moved.size(), 2U);
    EXPECT_TRUE(moved[0].exactlyEquals(Coordinate{1.5, 1.0, 0, 0}));
    EXPECT_TRUE(moved[1].exactlyEquals(Coordinate{-0.5, -0.5, 0, 0}));

    // By a coordinate: Coordinate::add, weights included.
    const std::vector<Coordinate> added = FinSet::translatePoints(points, Coordinate{1, 1, 1, 1});
    ASSERT_EQ(added.size(), 2U);
    EXPECT_TRUE(added[0].exactlyEquals(Coordinate{2, 3, 4, 5}));
    EXPECT_TRUE(added[1].exactlyEquals(Coordinate{0, 1.5, 1, 1}));

    const std::vector<Coordinate> backwards = FinSet::reverse(points);
    ASSERT_EQ(backwards.size(), 2U);
    EXPECT_TRUE(backwards[0].exactlyEquals(points[1]));
    EXPECT_TRUE(backwards[1].exactlyEquals(points[0]));
    EXPECT_TRUE(FinSet::reverse({}).empty());
    EXPECT_TRUE(FinSet::translatePoints({}, 1, 1).empty());
}

/// A rocket with one stage holding an ogive transition (0.2 m, radii 0.1 m and 0.3 m) with two
/// freeform fins on it (ProbeFins S5).
class FinSetOnOgive : public ::testing::Test
{
protected:
    FinSetOnOgive()
    {
        auto& stage      = m_rocket.addChild(std::make_unique<AxialStage>());
        auto  transition = std::make_unique<Transition>();
        transition->setLength(0.2);
        transition->setForeRadius(0.1);
        transition->setAftRadius(0.3);
        transition->setShapeType(TransitionShape::OGIVE);
        m_transition = &stage.addChild(std::move(transition));
        auto fins    = std::make_unique<FreeformFinSet>();
        fins->setFinCount(2);
        m_fins = &m_transition->addChild(std::move(fins));
        m_rocket.enableEvents();
        m_fins->setAxialMethod(AxialMethod::MIDDLE);
        m_fins->setAxialOffset(0.01);
        const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02, 0.08},
                                             Coordinate{0.09, 0.09}, Coordinate{0.08, 0.0}};
        m_fins->setPoints(points);
    }

    Rocket          m_rocket;
    Transition*     m_transition{nullptr};
    FreeformFinSet* m_fins{nullptr};
};

// Pinned with OpenRocket (ProbeFins S5): the root follows the curved body.
TEST_F(FinSetOnOgive, RootFollowsACurvedBody)
{
    const FinSet& fins = *m_fins;
    EXPECT_FALSE(fins.isRootStraight());
    EXPECT_NEAR(fins.getLength(), 0.08, 1e-16);
    EXPECT_NEAR(fins.getSpan(), 0.09, 1e-16);
    expectNear(Coordinate{0.07, 0.25198684153570666}, fins.getFinFront(), kPinned, "fin front");
    EXPECT_NEAR(fins.getBodyRadius(), 0.25198684153570666, kPinned);

    const std::vector<Coordinate> root = fins.getRootPoints();
    ASSERT_EQ(root.size(), 34U) << "one segment per 2.5 mm";
    expectNear(Coordinate{0.0, 0.0}, root[0], kPinned, "root 0");
    expectNear(Coordinate{0.002424242424242426, 0.0020405134340243936}, root[1], kPinned, "root 1");
    expectNear(Coordinate{0.04121212121212124, 0.027224521330460272}, root[17], kPinned, "root 17");
    expectNear(Coordinate{0.07757575757575749, 0.041020150063676264}, root[32], kPinned, "root 32");
    expectNear(Coordinate{0.0799999999999999, 0.041662325774664166}, root[33], kPinned, "root 33");

    // The low-resolution root has at most 20 segments.
    const std::vector<Coordinate> lowRes = fins.getRootPoints(FinSet::kMaxRootDivisionsLowRes);
    ASSERT_EQ(lowRes.size(), 21U);
    expectNear(Coordinate{0.08000000000000007, 0.04166232577466422}, lowRes[20], kPinned,
               "low-res root 20");
    EXPECT_EQ(fins.getFinPointsWithLowResRoot().size(), 25U);
    EXPECT_EQ(fins.getFinPointsWithRoot().size(), 38U);

    // The whole body, in its own frame.
    const std::vector<Coordinate> mount = fins.getMountPoints();
    ASSERT_EQ(mount.size(), 81U);
    expectNear(Coordinate{0.0025, 0.13152380053229626}, mount[1], kPinned, "mount 1");

    // The last fin point lies on the body.
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.08}, Coordinate{0.09, 0.09},
                      Coordinate{0.0799999999999999, 0.041662325774664166}},
                     fins.getFinPoints(), kPinned, "fin");

    EXPECT_NEAR(fins.getPlanformArea(), 0.004114928983155319, 1e-14);
    EXPECT_NEAR(fins.getComponentVolume(), 2.4689573898931914E-5, 1e-16);
    EXPECT_NEAR(fins.getComponentMass(), 0.0167889102512737, 1e-13);
    expectNear(Coordinate{0.044871953373131435, 0.0, 0.0, 0.0167889102512737},
               fins.getComponentCG(), 1e-13, "CG");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 0.04139749960472799, 1e-13);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.08218538010084038, 1e-13);

    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.0, 0.0}, Coordinate{0.05, 0.0},
                      Coordinate{0.05, 0.03131618626252697}},
                     fins.getTabPoints(), kPinned, "tab");
    EXPECT_NEAR(fins.getMaxTabHeight(), 0.25198684153570666, kPinned);
    EXPECT_NEAR(fins.getParentTrailingRadius().value_or(kNaN), 0.28330302779823363, kPinned);
    expectPointsNear({Coordinate{0.0, -0.19, -0.19}, Coordinate{0.16, 0.19, 0.19}},
                     fins.getComponentBounds(), kPinned, "bound");
}

// Pinned with OpenRocket (ProbeFins S5b, S5c): a root point where the body begins or ends.
TEST_F(FinSetOnOgive, RootOfAFinThatOverhangsTheBody)
{
    m_fins->setAxialMethod(AxialMethod::TOP);
    m_fins->setAxialOffset(-0.03);
    expectNear(Coordinate{-0.03, 0.1}, m_fins->getFinFront(), kPinned, "fin front");

    std::vector<Coordinate> root = m_fins->getRootPoints();
    ASSERT_EQ(root.size(), 33U);
    expectNear(Coordinate{0.0, 0.0}, root[0], kPinned, "front root 0");
    expectNear(Coordinate{0.0024999999999999988, 0.0}, root[1], kPinned, "front root 1");
    expectNear(Coordinate{0.039999999999999994, 0.06244997998398405}, root[16], kPinned,
               "front root 16");
    expectNear(Coordinate{0.08000000000000002, 0.13228756555322957}, root[32], kPinned,
               "front root 32");
    EXPECT_NEAR(m_fins->getPlanformArea(), 0.0027061397004134904, 1e-14);
    expectNear(Coordinate{0.028762168157187684, 0.0, 0.0, 0.011041049977687041},
               m_fins->getComponentCG(), 1e-13, "front CG");
    EXPECT_NEAR(m_fins->getSpan(), 0.14282856857085702, kPinned)
        << "the interior point was lifted out of the body";
    expectPointsNear({Coordinate{0.0, -0.24282856857085702, -0.24282856857085702},
                      Coordinate{0.06, 0.24282856857085702, 0.24282856857085702}},
                     m_fins->getComponentBounds(), kPinned, "front bound");

    m_fins->setAxialOffset(0.15);
    expectNear(Coordinate{0.15, 0.29364916731037083}, m_fins->getFinFront(), kPinned, "fin front");
    root = m_fins->getRootPoints();
    ASSERT_EQ(root.size(), 33U);
    expectNear(Coordinate{0.0025000000000000022, 6.283403944584709E-4}, root[1], kPinned,
               "aft root 1");
    expectNear(Coordinate{0.040000000000000036, 0.006100676244011005}, root[16], kPinned,
               "aft root 16");
    expectNear(Coordinate{0.07750000000000007, 0.00635083268962916}, root[31], kPinned,
               "aft root 31");
    expectNear(Coordinate{0.08000000000000007, 0.00635083268962916}, root[32], kPinned,
               "aft root 32");
    EXPECT_NEAR(m_fins->getPlanformArea(), 0.007450336470299523, 1e-14);
    expectNear(Coordinate{0.05123514967776812, 0.0, 0.0, 0.030397372798822056},
               m_fins->getComponentCG(), 1e-13, "aft CG");
    EXPECT_NEAR(m_fins->getParentTrailingRadius().value_or(kNaN), 0.3, kPinned);
}

/// The tab outlines of ProbeFins3: a tab from 0.02 m to 0.05 m, 0.04 m deep, under the curved
/// root.
void expectTabOutlinesOnOgive(const FinSet& fins)
{
    expectPointsNear(
        {Coordinate{0.02, 0.015046089349193992}, Coordinate{0.02, -0.02495391065080601},
         Coordinate{0.05, -0.02495391065080601}, Coordinate{0.05, 0.03131618626252697}},
        fins.getTabPoints(), kPinned, "tab");

    // The tab closed along the root points under it, back to front.
    const std::vector<Coordinate> withRoot = fins.getTabPointsWithRoot();
    ASSERT_EQ(withRoot.size(), 17U);
    expectNear(Coordinate{0.05, 0.03131618626252697}, withRoot[3], kPinned, "with root 3");
    expectNear(Coordinate{0.04848484848484852, 0.030647438032675278}, withRoot[4], kPinned,
               "with root 4");
    expectNear(Coordinate{0.038787878787878816, 0.02600290239350217}, withRoot[8], kPinned,
               "with root 8");
    expectNear(Coordinate{0.021818181818181834, 0.016229371163533624}, withRoot[15], kPinned,
               "with root 15");
    expectNear(Coordinate{0.02, 0.015046089349193992}, withRoot[16], kPinned, "with root 16");

    const std::vector<Coordinate> lowRes = fins.getTabPointsWithRootLowRes();
    ASSERT_EQ(lowRes.size(), 13U);
    expectNear(Coordinate{0.04800000000000004, 0.03043026292547052}, lowRes[4], kPinned,
               "low-res 4");
    expectNear(Coordinate{0.020000000000000018, 0.015046089349193992}, lowRes[11], kPinned,
               "low-res 11");
    expectNear(Coordinate{0.02, 0.015046089349193992}, lowRes[12], kPinned, "low-res 12");

    // OpenRocket's continuous outline: the fin, the tab from the back, then the root points
    // ahead of the tab from the front.
    const std::vector<Coordinate> continuous = fins.generateContinuousFinAndTabShape();
    ASSERT_EQ(continuous.size(), 17U);
    expectNear(Coordinate{0.0799999999999999, 0.041662325774664166}, continuous[3], kPinned,
               "continuous 3");
    expectNear(Coordinate{0.05, 0.03131618626252697}, continuous[4], kPinned, "continuous 4");
    expectNear(Coordinate{0.05, -0.02495391065080601}, continuous[5], kPinned, "continuous 5");
    expectNear(Coordinate{0.0, 0.0}, continuous[8], kPinned, "continuous 8");
    expectNear(Coordinate{0.014545454545454556, 0.011323439628353305}, continuous[14], kPinned,
               "continuous 14");
    expectNear(Coordinate{0.019393939393939408, 0.01464538631115514}, continuous[16], kPinned,
               "continuous 16");
}

// Pinned with OpenRocket (ProbeFins3): a tab and fillets on the curved body.
TEST_F(FinSetOnOgive, TabAndFilletsOnACurvedBodyMatchOpenRocket)
{
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    m_fins->setTabLength(0.03);
    m_fins->setTabOffset(0.02);
    m_fins->setTabHeight(0.04);
    m_fins->setFilletRadius(0.006);
    m_fins->setFilletMaterial(Material::newMaterial(Material::Type::BULK, "Fillet", 1100.0, true));
    const FinSet& fins = *m_fins;

    EXPECT_EQ(fins.getTabHeight(), 0.04);
    EXPECT_NEAR(fins.getTabFrontEdge(), 0.02, 1e-17);
    EXPECT_NEAR(fins.getTabTrailingEdge(), 0.05, 1e-17);
    EXPECT_NEAR(fins.getMaxTabHeight(), 0.26703293088490065, kPinned);
    expectTabOutlinesOnOgive(fins);

    EXPECT_NEAR(fins.getPlanformArea(), 0.004114928983155319, 1e-14);
    EXPECT_NEAR(fins.getComponentVolume(), 3.623898780053381E-5, 1e-16);
    EXPECT_NEAR(fins.getComponentMass(), 0.025771149089840104, 1e-13);
    expectNear(Coordinate{0.04203821294741054, 0.0, 0.0, 0.025771149089840104},
               fins.getComponentCG(), 1e-13, "CG");
    expectNear(Coordinate{0.03861408389457766, 0.0, 0.0, 1.343615935091809E-6},
               fins.calculateFilletVolumeCentroid(), 1e-13, "fillets");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 0.04139749960472799, 1e-13);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 0.08218538010084038, 1e-13);

    // A single fin, rotated: the CG and the fillets turn with it.
    m_fins->setFinCount(1);
    m_fins->setBaseRotation(-0.7);
    EXPECT_NEAR(fins.getComponentMass(), 0.012885574544920052, 1e-13);
    expectNear(Coordinate{0.04203821294741054, 0.20155641957288126, -0.19013979551503346,
                          0.012885574544920052},
               fins.getComponentCG(), 1e-13, "single CG");
    expectNear(Coordinate{0.03861408389457766, 0.16127344192148957, -0.13583874620775793,
                          1.343615935091809E-6},
               fins.calculateFilletVolumeCentroid(), 1e-13, "single fillet");
    EXPECT_NEAR(fins.getLongitudinalUnitInertia(), 4.97696850393207E-4, 1e-15);
    EXPECT_NEAR(fins.getRotationalUnitInertia(), 3.857745921708111E-4, 1e-15);

    // A tab that starts ahead of the fin: the outline is closed at the fin's first point.
    m_fins->setTabOffset(-0.01);
    EXPECT_FALSE(fins.isTabBeyondFin());
    expectPointsNear(
        {Coordinate{-0.01, -0.00915827296484964}, Coordinate{-0.01, -0.04915827296484964},
         Coordinate{0.019999999999999997, -0.04915827296484964},
         Coordinate{0.019999999999999997, 0.015046089349193992}},
        fins.getTabPoints(), kPinned, "tab ahead");
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.08}, Coordinate{0.09, 0.09},
                      Coordinate{0.0799999999999999, 0.041662325774664166},
                      Coordinate{0.019999999999999997, 0.015046089349193992},
                      Coordinate{0.019999999999999997, -0.04915827296484964},
                      Coordinate{-0.01, -0.04915827296484964},
                      Coordinate{-0.01, -0.00915827296484964}, Coordinate{0.0, 0.0}},
                     fins.generateContinuousFinAndTabShape(), kPinned, "continuous ahead");
    EXPECT_NEAR(fins.getComponentMass(), 0.01315607228543259, 1e-13);
    expectNear(Coordinate{0.034498581035509, 0.19721795327147887, -0.18606671082956577,
                          0.01315607228543259},
               fins.getComponentCG(), 1e-13, "CG with the tab ahead");
}

TEST(FinSet, RootOfAStraightFinThatOverhangsATube)
{
    // A fin that starts 0.02 m ahead of the tube and one that ends 0.02 m behind it: the extra
    // root point is where the tube begins or ends, at the radius of the nearest end.
    BodyTube tube(0.1, 0.03);
    FinSet&  fins = tube.addChild(std::make_unique<TrapezoidFinSet>());  // 0.05 m long
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(-0.02);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.02, 0.0}, Coordinate{0.05, 0.0}},
                     fins.getRootPoints(), 1e-15, "front");

    fins.setAxialOffset(0.07);
    expectPointsNear({Coordinate{0.0, 0.0}, Coordinate{0.03, 0.0}, Coordinate{0.05, 0.0}},
                     fins.getRootPoints(), 1e-15, "aft");

    // Entirely within the tube, and ending exactly at its end: no extra point.
    fins.setAxialOffset(0.05);
    EXPECT_EQ(fins.getRootPoints().size(), 2U);
    fins.setAxialOffset(0.01);
    EXPECT_EQ(fins.getRootPoints().size(), 2U);
}

TEST(FinSet, AParentThatIsNotABodyIsABug)
{
    // No component other than a body component accepts a fin set; Java's casts would throw a
    // ClassCastException.
    TestComponent parent(ComponentKind::MASS_COMPONENT);
    FinSet&       fins = parent.addChild(std::make_unique<TrapezoidFinSet>());
    EXPECT_THROW(static_cast<void>(fins.getFinFront()), BugError);
    EXPECT_THROW(static_cast<void>(fins.getBodyRadius()), BugError);
    EXPECT_THROW(static_cast<void>(fins.getRootPoints()), BugError);
    EXPECT_THROW(static_cast<void>(fins.getFinPoints()), BugError);
    EXPECT_THROW(static_cast<void>(fins.getTabPoints()), BugError);
    EXPECT_THROW(static_cast<void>(fins.getComponentMass()), BugError);
    // The questions Java asks with instanceof are answered.
    EXPECT_EQ(fins.getParentFrontRadius(), std::nullopt);
    EXPECT_EQ(fins.getMaxTabHeight(), std::numeric_limits<double>::max());
    EXPECT_TRUE(fins.isRootStraight());
    fins.setFilletRadius(0.01);
    EXPECT_EQ(fins.calculateFilletVolumeCentroid(), Coordinate::kZero);
}

// Deviation: a freeform outline that runs forwards has a negative length; OpenRocket then finds
// no root point for a canted fin and fails with an IndexOutOfBoundsException wherever the fin's
// outline is asked for. Here the root is its two ends, as it is without the cant: an .ork file
// can hold such a fin set, and the rocket must stay usable.
TEST(FinSet, ACantedRootOfNegativeLengthHasItsTwoEnds)
{
    BodyTube        tube(0.5, 0.03);
    FreeformFinSet& fins = tube.addChild(std::make_unique<FreeformFinSet>());
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.3);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{-0.05, 0.05},
                                         Coordinate{-0.1, 0}};
    fins.setPoints(points);
    EXPECT_NEAR(fins.getLength(), -0.1, 1e-16);
    EXPECT_EQ(fins.getRootPoints().size(), 2U) << "a straight root has its two ends";

    fins.setCantAngle(0.1);
    const std::vector<Coordinate> root = fins.getRootPoints();
    ASSERT_EQ(root.size(), 2U);
    // From the fin's front to its end, which lies before it.
    EXPECT_EQ(root.front().x, 0.0);
    EXPECT_NEAR(root.back().x, -0.1, 1e-15);
    EXPECT_NO_THROW(static_cast<void>(fins.getFinPoints()));
    EXPECT_NO_THROW(static_cast<void>(fins.getInstanceBoundingBox()));

    // The division count is the caller's to get right: a negative one is still a bug.
    EXPECT_THROW(static_cast<void>(fins.getRootPoints(-1)), BugError);
}

// The same for the fin set a file can give a negative length most easily, and for what asks a
// rocket for its bounds while a file loads: a shock cord that takes its length from the rocket's
// (OpenRocket: an IndexOutOfBoundsException out of ShockCord.setCordLengthAutomatic()).
TEST(FinSet, ARocketWithACantedFinSetOfNegativeLengthStillHasBounds)
{
    Rocket            rocket;
    AxialStage&       stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    EllipticalFinSet& fins  = tube.addChild(std::make_unique<EllipticalFinSet>());
    fins.setLength(-0.1);
    fins.setCantAngle(0.05);
    ASSERT_EQ(fins.getLength(), -0.1);

    EXPECT_EQ(fins.getRootPoints().size(), 2U);
    EXPECT_NO_THROW(static_cast<void>(fins.getFinPoints()));
    EXPECT_NO_THROW(static_cast<void>(rocket.getLength()));

    QtRocket::ShockCord& cord = tube.addChild(std::make_unique<QtRocket::ShockCord>());
    EXPECT_NO_THROW(cord.setCordLengthAutomatic(true));
    EXPECT_TRUE(cord.isCordLengthAutomatic());
}

// ============================================================================ copies, splits

TEST_F(FinSetOnAlpha, CopiesCarryEveryProperty)
{
    m_fins->setFinCount(5);
    m_fins->setBaseRotation(0.4);
    m_fins->setAngleMethod(AngleMethod::FIXED);
    m_fins->setCantAngle(0.05);
    m_fins->setThickness(0.004);
    m_fins->setCrossSection(FinSet::CrossSection::ROUNDED);
    m_fins->setTabHeight(0.004);
    m_fins->setTabLength(0.02);
    m_fins->setTabOffsetMethod(AxialMethod::BOTTOM);
    m_fins->setTabOffset(-0.01);
    m_fins->setFilletRadius(0.002);
    m_fins->setFilletMaterial(Material::newMaterial(Material::Type::BULK, "Fillet", 1100.0, true));
    m_fins->getInsideColorComponentHandler().setSeparateInsideOutside(true);
    m_fins->getInsideColorComponentHandler().setEdgesSameAsInside(true);
    const double mass = m_fins->getComponentMass();

    const std::unique_ptr<RocketComponent> copied = m_fins->copyWithOriginalId();
    const auto* copy = dynamic_cast<const TrapezoidFinSet*>(copied.get());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getId(), m_fins->getId());
    EXPECT_EQ(copy->getParent(), nullptr);

    EXPECT_EQ(copy->getFinCount(), 5);
    EXPECT_EQ(copy->getBaseRotation(), 0.4);
    EXPECT_EQ(copy->getAngleMethod(), AngleMethod::FIXED);
    EXPECT_EQ(copy->getCantAngle(), 0.05);
    EXPECT_EQ(copy->getThickness(), 0.004);
    EXPECT_EQ(copy->getCrossSection(), FinSet::CrossSection::ROUNDED);
    EXPECT_EQ(copy->getTabHeight(), 0.004);
    EXPECT_EQ(copy->getTabLength(), 0.02);
    EXPECT_EQ(copy->getTabOffsetMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(copy->getTabOffset(), m_fins->getTabOffset());
    EXPECT_EQ(copy->getTabFrontEdge(), m_fins->getTabFrontEdge());
    EXPECT_EQ(copy->getTabPosition(AxialMethod::TOP), m_fins->getTabPosition(AxialMethod::TOP));
    EXPECT_EQ(copy->getFilletRadius(), 0.002);
    EXPECT_EQ(copy->getFilletMaterial(), m_fins->getFilletMaterial());
    EXPECT_EQ(copy->getMaterial(), m_fins->getMaterial());
    EXPECT_EQ(copy->getRootChord(), 0.05);
    EXPECT_EQ(copy->getTipChord(), 0.03);
    EXPECT_EQ(copy->getSweep(), 0.02);
    EXPECT_EQ(copy->getHeight(), 0.05);
    EXPECT_EQ(copy->getFinRotationTransformation(), m_fins->getFinRotationTransformation());
    EXPECT_EQ(copy->getCantRotation(), m_fins->getCantRotation());
    EXPECT_EQ(copy->getName(), "3 Fin Set");
    EXPECT_TRUE(copy->getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_TRUE(copy->getInsideColorComponentHandler().isEdgesSameAsInside());
    EXPECT_NE(&copy->getInsideColorComponentHandler(), &m_fins->getInsideColorComponentHandler());
    // The cached values came along (the copy has no body to recompute them on).
    EXPECT_EQ(copy->getComponentMass(), mass);

    const std::unique_ptr<RocketComponent> fresh = m_fins->copyWithNewIds();
    EXPECT_NE(fresh->getId(), m_fins->getId());
    EXPECT_EQ(fresh->kind(), ComponentKind::TRAPEZOID_FIN_SET);
}

TEST_F(FinSetOnAlpha, InsideColourHandlerFiresOnTheFinSet)
{
    m_fins->getInsideColorComponentHandler().setSeparateInsideOutside(true);
    EXPECT_EQ(takeEvents(), std::vector<int>{ComponentChangeEvent::kGraphicChange});
    m_fins->getInsideColorComponentHandler().setInsideAppearance(std::nullopt);
    EXPECT_EQ(takeEvents(), std::vector<int>{kNonFunctional});
}

/// One of the single fins splitFins() makes of the Alpha III fin set: a trapezoidal fin set of
/// one fin named @p name at @p angle, with a third of the override mass.
void expectSingleFin(const RocketComponent* component, std::string_view name, double angle)
{
    const auto* fin = dynamic_cast<const TrapezoidFinSet*>(component);
    ASSERT_NE(fin, nullptr);
    EXPECT_EQ(fin->getFinCount(), 1);
    EXPECT_NEAR(fin->getAngleOffset(), angle, 1e-15);
    EXPECT_EQ(fin->getName(), name);
    EXPECT_NEAR(fin->getOverrideMass(), 0.01, 1e-17);
    EXPECT_EQ(fin->getRootChord(), 0.05);
}

TEST_F(FinSetOnAlpha, SplitFinsMakesSingleFins)
{
    m_fins->setBaseRotation(0.25);
    m_fins->setMassOverridden(true);
    m_fins->setOverrideMass(0.03);
    const QtRocket::Uuid original = m_fins->getId();

    const RocketComponent::SplitResult result = m_fins->splitFins();
    ASSERT_EQ(result.components.size(), 3U);
    ASSERT_NE(result.original, nullptr);
    EXPECT_EQ(result.original.get(), m_fins) << "the replaced fin set is handed back";
    EXPECT_EQ(result.original->getParent(), nullptr);
    ASSERT_EQ(m_body->getChildCount(), 3U);

    // The fins take the fin set's place, each a third of a turn further (reduced to -pi ... pi).
    EXPECT_EQ(result.components[0], &m_body->getChild(0));
    EXPECT_EQ(result.components[1], &m_body->getChild(1));
    EXPECT_EQ(result.components[2], &m_body->getChild(2));
    expectSingleFin(result.components[0], "3 Fin Set #1", 0.25);
    expectSingleFin(result.components[1], "3 Fin Set #2", 0.25 + (2 * std::numbers::pi / 3));
    expectSingleFin(result.components[2], "3 Fin Set #3",
                    0.25 + (4 * std::numbers::pi / 3) - (2 * std::numbers::pi));
    EXPECT_NE(result.components[0]->getId(), original);
    EXPECT_NE(result.components[1]->getId(), original);
    EXPECT_NE(result.components[2]->getId(), result.components[1]->getId());
}

TEST_F(FinSetOnAlpha, SplitFinsLeavesASingleFinAlone)
{
    m_fins->setFinCount(1);
    const RocketComponent::SplitResult result = m_fins->splitFins(false);
    ASSERT_EQ(result.components.size(), 1U);
    EXPECT_EQ(result.components.front(), m_fins);
    EXPECT_EQ(result.original, nullptr);
    EXPECT_EQ(m_body->getChildCount(), 1U);
}

TEST(FinSet, SetOverrideMassWithoutAnArgumentDoesNothing)
{
    // Java's FinSet declares an empty setOverrideMass(); the one-argument setter is
    // RocketComponent's.
    TrapezoidFinSet fins;
    fins.setMassOverridden(true);
    fins.setOverrideMass(0.5);
    FinSet::setOverrideMass();
    EXPECT_EQ(fins.getOverrideMass(), 0.5);
}

// ==================================================================================== debug

TEST(FinSet, PointDescriptionMatchesJavasFormat)
{
    // Pinned with OpenRocket (ProbeFormat): "%s    >> %s: %d points\n" and
    // "      ....[%2d] (%6.4g, %6.4g)\n".
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.025, 0.05},
                                         Coordinate{0.075, 0.05}, Coordinate{0.05, 0},
                                         Coordinate{12.5, -0.0001234}};
    EXPECT_EQ(FinSet::getPointDescr(points, "Fin Points", ".."),
              "..    >> Fin Points: 5 points\n"
              "..      ....[ 0] ( 0.000,  0.000)\n"
              "..      ....[ 1] (0.02500, 0.05000)\n"
              "..      ....[ 2] (0.07500, 0.05000)\n"
              "..      ....[ 3] (0.05000,  0.000)\n"
              "..      ....[ 4] ( 12.50, -0.0001234)\n");
    EXPECT_EQ(FinSet::getPointDescr({}, "None", ""), "    >> None: 0 points\n");

    const std::vector<Coordinate> odd{Coordinate{kNaN, 123456.0}};
    EXPECT_EQ(FinSet::getPointDescr(odd, "Odd", ""),
              "    >> Odd: 1 points\n"
              "      ....[ 0] (   NaN, 1.235e+05)\n");
}

/// @p detail from its second line on (the first names the calling function, which depends on
/// the compiler).
std::string afterFirstLine(const std::string& detail)
{
    const std::size_t end = detail.find('\n');
    return end == std::string::npos ? std::string{} : detail.substr(end + 1);
}

TEST_F(FinSetOnAlpha, DebugDetailMatchesOpenRocket)
{
    // Pinned with OpenRocket (ProbeFins S1 and S1b).
    const std::string detail = m_fins->toDebugDetail();
    EXPECT_TRUE(detail.starts_with(" >> Dumping Detailed Information from: ")) << detail;
    EXPECT_EQ(afterFirstLine(detail),
              "      At Component: 3 Fin Set, of class: TrapezoidFinSet \n"
              "      position: 0.150000    at offset: 0.0000 via: BOTTOM\n"
              "      length: 0.0500\n"
              "    >> Fin Points: 4 points\n"
              "      ....[ 0] ( 0.000,  0.000)\n"
              "      ....[ 1] (0.02000, 0.05000)\n"
              "      ....[ 2] (0.05000, 0.05000)\n"
              "      ....[ 3] (0.05000,  0.000)\n"
              "    >> Body Points: 2 points\n"
              "      ....[ 0] ( 0.000, 0.01200)\n"
              "      ....[ 1] (0.2000, 0.01200)\n"
              "    >> Root Points: 2 points\n"
              "      ....[ 0] ( 0.000,  0.000)\n"
              "      ....[ 1] (0.05000,  0.000)\n");

    m_fins->setTabHeight(0.005);
    m_fins->setTabLength(0.02);
    m_fins->setTabOffsetMethod(AxialMethod::TOP);
    m_fins->setTabOffset(0.01);
    EXPECT_TRUE(afterFirstLine(m_fins->toDebugDetail())
                    .ends_with("    >> Root Points: 2 points\n"
                               "      ....[ 0] ( 0.000,  0.000)\n"
                               "      ....[ 1] (0.05000,  0.000)\n"
                               "    TabLength: 0.0200 TabHeight: 0.0050 @ 0.0100    via: Top of "
                               "the parent component\n"
                               "    >> Tab Points: 5 points\n"
                               "      ....[ 0] (0.01000,  0.000)\n"
                               "      ....[ 1] (0.01000, -0.005000)\n"
                               "      ....[ 2] (0.03000, -0.005000)\n"
                               "      ....[ 3] (0.03000,  0.000)\n"
                               "      ....[ 4] (0.01000,  0.000)\n"))
        << m_fins->toDebugDetail();

    // A detached fin set has no body and no root to list.
    const TrapezoidFinSet lonely;
    EXPECT_EQ(afterFirstLine(lonely.toDebugDetail()),
              "      At Component: Trapezoidal Fin Set, of class: TrapezoidFinSet \n"
              "      position: 0.000000    at offset: 0.0000 via: BOTTOM\n"
              "      length: 0.0500\n"
              "    >> Fin Points: 4 points\n"
              "      ....[ 0] ( 0.000,  0.000)\n"
              "      ....[ 1] (0.02500, 0.03000)\n"
              "      ....[ 2] (0.07500, 0.03000)\n"
              "      ....[ 3] (0.05000,  0.000)\n");
}

}  // namespace
