#include "QtRocket/figure/FigureArc.h"

#include <array>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::FigureArc;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;
using QtRocket::Test::containsAsJava;
using QtRocket::Test::ContainsCase;
using QtRocket::Test::rectanglesNear;
using QtRocket::Test::segmentsNear;

using Type = FigureArc::Type;

// Every value below was printed by java.awt.geom (JDK 17, the probe ShapeProbe.java of the port's
// scratch directory) for the same arcs: Arc2D.Double's PathIterator (ArcIterator), getBounds2D,
// containsAngle and contains. The iterator and the bounds take sines and cosines, whose last bits
// differ between Java and the C libraries, so coordinates are compared within
// kTranscendentalTolerance; segment types, counts and contains() answers exactly.

/// Success when @p arc's segments and bounds are Java's (within the tolerance).
[[nodiscard]] ::testing::AssertionResult arcAsJava(const FigureArc&                arc,
                                                   const std::vector<PathSegment>& segments,
                                                   const Rectangle2D&              bounds)
{
    if (auto result = segmentsNear(arc.segments(), segments); !result)
    {
        return result;
    }
    return rectanglesNear(arc.bounds(), bounds);
}

TEST(FigureArc, AParachuteCanopyIsAPieOfTwoCurves)
{
    // ParachuteShapes: a PIE from 180 to 360 degrees, chute diameter 12.5 mm.
    constexpr double kDiameter = 0.0125;
    const FigureArc  arc{0.30 - (kDiameter / 2),
                         0.0254 - (kDiameter / 4),
                         kDiameter,
                         kDiameter,
                         180.0,
                         180.0,
                         Type::PIE};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({0.29375, 0.028525000000000002}),
        PathSegment::cubicTo({0.29375, 0.03197677968644246}, {0.29654822031355754, 0.034775},
                             {0.3, 0.034775}),
        PathSegment::cubicTo({0.30345177968644244, 0.034775},
                             {0.30624999999999997, 0.03197677968644246},
                             {0.30624999999999997, 0.028525000000000002}),
        PathSegment::lineTo({0.3, 0.028525000000000002}),
        PathSegment::close(),
    };
    const Rectangle2D bounds{
        .x = 0.29375, .y = 0.028525, .width = 0.0125, .height = 0.006250000000000002};
    constexpr std::array<ContainsCase, 4> kCases{{
        {{0.3, 0.029525000000000003}, true},
        {{0.3, 0.027525}, false},
        {{0.29425, 0.028625}, true},
        {{0.3, 0.034275}, true},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(arc, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{arc}, kCases));
}

TEST(FigureArc, ADeploymentChargeIsAHalfChord)
{
    // MassComponentShapes' deployment charge: a CHORD from 55 degrees over 180.
    const FigureArc arc{0.1, 0.2, 0.04, 0.04, 55.0, 180.0, Type::CHORD};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({0.13147152872702092, 0.20361695911422018}),
        PathSegment::cubicTo({0.12242342508995036, 0.19728140874104066},
                             {0.1099525094873997, 0.19948036763590848},
                             {0.10361695911422017, 0.20852847127297908}),
        PathSegment::cubicTo({0.09728140874104066, 0.21757657491004964},
                             {0.0994803676359085, 0.23004749051260032},
                             {0.10852847127297909, 0.23638304088577983}),
        PathSegment::close(),
    };
    const Rectangle2D bounds{
        .x = 0.1, .y = 0.2, .width = 0.03147152872702092, .height = 0.03638304088577983};
    constexpr std::array<ContainsCase, 7> kCases{{
        {{0.12, 0.22}, true},
        {{0.105, 0.22}, true},
        {{0.135, 0.22}, false},
        {{0.12, 0.205}, true},
        {{0.12, 0.235}, false},
        {{0.125, 0.212}, true},
        {{0.115, 0.228}, false},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(arc, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{arc}, kCases));
}

TEST(FigureArc, ARailButtonScrewIsAnOpenHalfEllipse)
{
    // RailButtonShapes' screw: an OPEN arc from 0 over -signum(h) * 180 degrees.
    const FigureArc arc{0.245, 0.03, 0.0097, 0.004, 0.0, -180.0, Type::OPEN};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({0.2547, 0.032}),
        PathSegment::cubicTo({0.2547, 0.03310456949966159}, {0.25252858103667936, 0.034},
                             {0.24985, 0.034}),
        PathSegment::cubicTo({0.24717141896332065, 0.034}, {0.245, 0.03310456949966159},
                             {0.245, 0.032}),
    };
    const Rectangle2D bounds{.x = 0.245, .y = 0.032, .width = 0.0097, .height = 0.002};
    // An OPEN arc is tested as if closed by its chord.
    constexpr std::array<ContainsCase, 4> kCases{{
        {{0.24985, 0.032}, true},
        {{0.24985, 0.0335}, true},
        {{0.24985, 0.0305}, false},
        {{0.246, 0.0321}, true},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(arc, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{arc}, kCases));
}

TEST(FigureArc, TrackerArcsAreSixtyDegreesWide)
{
    // MassComponentShapes' tracker: OPEN arcs from 330 and 150 degrees over 60.
    const FigureArc right{0.1, 0.1, 0.02, 0.02, 330.0, 60.0, Type::OPEN};
    const FigureArc left{0.1, 0.1, 0.02, 0.02, 150.0, 60.0, Type::OPEN};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> rightSegments{
        PathSegment::moveTo({0.11866025403784439, 0.115}),
        PathSegment::cubicTo({0.1204465819873852, 0.11190598923241497},
                             {0.12044658198738521, 0.10809401076758504},
                             {0.11866025403784439, 0.10500000000000001}),
    };
    const Rectangle2D              rightBounds{.x      = 0.11866025403784439,
                                               .y      = 0.10500000000000001,
                                               .width  = 0.0013397459621556163,
                                               .height = 0.010000000000000005};
    const std::vector<PathSegment> leftSegments{
        PathSegment::moveTo({0.10133974596215561, 0.105}),
        PathSegment::cubicTo({0.0995534180126148, 0.10809401076758503},
                             {0.0995534180126148, 0.11190598923241497},
                             {0.10133974596215561, 0.115}),
    };
    const Rectangle2D leftBounds{
        .x = 0.1, .y = 0.10500000000000001, .width = 0.0013397459621556141, .height = 0.01};
    constexpr std::array<ContainsCase, 4> kRightCases{{
        {{0.119, 0.11}, true},
        {{0.115, 0.11}, false},
        {{0.1185, 0.1035}, false},
        {{0.101, 0.11}, false},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(right, rightSegments, rightBounds));
    EXPECT_TRUE(arcAsJava(left, leftSegments, leftBounds));
    EXPECT_TRUE(containsAsJava(FigureShape{right}, kRightCases));
}

TEST(FigureArc, AFullTurnIsFourCurves)
{
    const FigureArc full{-1, -1, 2, 2, 30.0, 360.0, Type::OPEN};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({0.8660254037844387, -0.49999999999999994}),
        PathSegment::cubicTo({0.5898830288690421, -0.9782926234762004},
                             {-0.021707376523799282, -1.1421677786998352},
                             {-0.4999999999999998, -0.8660254037844387}),
        PathSegment::cubicTo({-0.9782926234762003, -0.5898830288690422},
                             {-1.1421677786998354, 0.021707376523799116},
                             {-0.8660254037844388, 0.4999999999999997}),
        PathSegment::cubicTo({-0.5898830288690423, 0.9782926234762004},
                             {0.021707376523798616, 1.1421677786998354},
                             {0.49999999999999933, 0.866025403784439}),
        PathSegment::cubicTo({0.9782926234762006, 0.5898830288690419},
                             {1.1421677786998352, -0.02170737652379956},
                             {0.8660254037844386, -0.5}),
    };
    constexpr std::array<ContainsCase, 2> kCases{{
        {{0.5, 0.5}, true},
        {{0.9, 0.9}, false},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(full, segments, Rectangle2D{.x = -1, .y = -1, .width = 2, .height = 2}));
    EXPECT_TRUE(containsAsJava(FigureShape{full}, kCases));
}

TEST(FigureArc, MoreThanAFullTurnBackwardsIsStillFourCurves)
{
    const FigureArc pie{0, 0, 4, 2, 10.0, -400.0, Type::PIE};
    // NOLINTBEGIN(modernize-use-std-numbers): Java's results, not approximations of constants
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({3.9696155060244163, 0.8263518223330697}),
        PathSegment::cubicTo({4.161421986747124, 1.3702461258368426},
                             {3.435084962341407, 1.8889045126508544},
                             {2.347296355333861, 1.9848077530122081}),
        PathSegment::cubicTo({1.2595077483263148, 2.080710993373562},
                             {0.22219097469829152, 1.7175424811707032},
                             {0.03038449397558396, 1.1736481776669303}),
        PathSegment::cubicTo({-0.16142198674712338, 0.6297538741631573},
                             {0.5649150376585934, 0.11109548734914576},
                             {1.6527036446661394, 0.01519224698779198}),
        PathSegment::cubicTo({2.740492251673685, -0.08071099337356169},
                             {3.7778090253017087, 0.2824575188292966},
                             {3.9696155060244163, 0.8263518223330696}),
        PathSegment::lineTo({2.0, 1.0}),
        PathSegment::close(),
    };
    // PINS-END
    // NOLINTEND(modernize-use-std-numbers)
    EXPECT_TRUE(arcAsJava(pie, segments, Rectangle2D{.x = 0, .y = 0, .width = 4, .height = 2}));
}

TEST(FigureArc, AZeroExtentIsAMoveAndItsClosure)
{
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> open{
        PathSegment::moveTo({0.8535533905932737, 0.14644660940672627}),
    };
    const std::vector<PathSegment> chord{
        PathSegment::moveTo({0.8535533905932737, 0.14644660940672627}),
        PathSegment::close(),
    };
    const std::vector<PathSegment> pie{
        PathSegment::moveTo({0.8535533905932737, 0.14644660940672627}),
        PathSegment::lineTo({0.5, 0.5}),
        PathSegment::close(),
    };
    const Rectangle2D pointBounds{
        .x = 0.8535533905932737, .y = 0.14644660940672627, .width = 0.0, .height = 0.0};
    const Rectangle2D pieBounds{.x      = 0.5,
                                .y      = 0.14644660940672627,
                                .width  = 0.3535533905932738,
                                .height = 0.35355339059327373};
    // PINS-END
    EXPECT_TRUE(arcAsJava(FigureArc{0, 0, 1, 1, 45.0, 0.0, Type::OPEN}, open, pointBounds));
    EXPECT_TRUE(arcAsJava(FigureArc{0, 0, 1, 1, 45.0, 0.0, Type::CHORD}, chord, pointBounds));
    EXPECT_TRUE(arcAsJava(FigureArc{0, 0, 1, 1, 45.0, 0.0, Type::PIE}, pie, pieBounds));
}

TEST(FigureArc, APieContainsOnlyItsSector)
{
    const FigureArc pie{0, 0, 1, 1, 10.0, 45.0, Type::PIE};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({0.9924038765061041, 0.41317591116653485}),
        PathSegment::cubicTo({0.9693766964321756, 0.2825822834562016},
                             {0.8954145330911296, 0.16648494240735368},
                             {0.786788218175523, 0.0904239778555041}),
        PathSegment::lineTo({0.5, 0.5}),
        PathSegment::close(),
    };
    const Rectangle2D                     bounds{.x      = 0.5,
                                                 .y      = 0.0904239778555041,
                                                 .width  = 0.492403876506104,
                                                 .height = 0.4095760221444959};
    constexpr std::array<ContainsCase, 5> kCases{{
        {{0.9, 0.3}, true},
        {{0.9, 0.45}, false},
        {{0.6, 0.45}, true},
        {{0.6, 0.55}, false},
        {{0.75, 0.25}, true},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(pie, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{pie}, kCases));
}

TEST(FigureArc, AChordOfMoreThanHalfATurnContainsThePieTriangle)
{
    const FigureArc chord{0, 0, 2, 1, -45.0, 270.0, Type::CHORD};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({1.7071067811865475, 0.8535533905932737}),
        PathSegment::cubicTo({2.0976310729378174, 0.6582912447176388},
                             {2.0976310729378174, 0.3417087552823612},
                             {1.7071067811865475, 0.14644660940672627}),
        PathSegment::cubicTo({1.3165824894352776, -0.04881553646890868},
                             {0.6834175105647224, -0.04881553646890868},
                             {0.29289321881345254, 0.1464466094067262}),
        PathSegment::cubicTo({-0.09763107293781736, 0.34170875528236117},
                             {-0.09763107293781759, 0.6582912447176388},
                             {0.2928932188134523, 0.8535533905932737}),
        PathSegment::close(),
    };
    const Rectangle2D bounds{.x = 0.0, .y = 0.0, .width = 2.0, .height = 0.8535533905932737};
    constexpr std::array<ContainsCase, 6> kCases{{
        {{1.0, 0.5}, true},
        {{1.9, 0.5}, true},
        {{1.6, 0.7}, true},
        {{1.6, 0.3}, true},
        {{0.2, 0.5}, true},
        {{1.75, 0.5}, true},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(chord, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{chord}, kCases));
}

TEST(FigureArc, ANegativeExtentRunsTheOtherWay)
{
    const FigureArc chord{0, 0, 2, 2, 90.0, -100.0, Type::CHORD};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({1.0, 0.0}),
        PathSegment::cubicTo({1.2955928835239199, 0.0}, {1.5760410000782858, 0.13077510446444662},
                             {1.766044443118978, 0.35721239031346075}),
        PathSegment::cubicTo({1.95604788615967, 0.5836496761624749},
                             {2.0361369185674496, 0.8825460142373396},
                             {1.9848077530122081, 1.1736481776669303}),
        PathSegment::close(),
    };
    const Rectangle2D bounds{
        .x = 1.0, .y = 0.0, .width = 0.9999999999999999, .height = 1.1736481776669303};
    constexpr std::array<ContainsCase, 4> kCases{{
        {{1.5, 0.5}, true},
        {{1.2, 0.8}, false},
        {{0.5, 0.5}, false},
        {{1.5, 1.5}, false},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(chord, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{chord}, kCases));
}

TEST(FigureArc, AStartBeyondATurnIsNormalised)
{
    const FigureArc pie{0, 0, 2, 2, 1000.0, 100.0, Type::PIE};
    // PINS-BEGIN (ShapeProbe.java)
    const std::vector<PathSegment> segments{
        PathSegment::moveTo({1.1736481776669312, 1.984807753012208}),
        PathSegment::cubicTo({1.4647503410965221, 1.9334785874569658},
                             {1.7182289620224787, 1.7559909463096093}, {1.8660254037844386, 1.5}),
        PathSegment::cubicTo({2.0138218455463983, 1.244009053690391},
                             {2.040791341174807, 0.9357463080785884},
                             {1.9396926207859089, 0.6579798566743323}),
        PathSegment::lineTo({1.0, 1.0}),
        PathSegment::close(),
    };
    const Rectangle2D bounds{
        .x = 1.0, .y = 0.6579798566743323, .width = 1.0, .height = 1.3268278963378757};
    constexpr std::array<ContainsCase, 4> kCases{{
        {{0.5, 1.5}, false},
        {{1.5, 1.5}, true},
        {{0.5, 0.5}, false},
        {{1.5, 0.5}, false},
    }};
    // PINS-END
    EXPECT_TRUE(arcAsJava(pie, segments, bounds));
    EXPECT_TRUE(containsAsJava(FigureShape{pie}, kCases));
}

TEST(FigureArc, ANegativeFrameIsEmpty)
{
    const FigureArc arc{0, 0, -2, 2, 0.0, 90.0, Type::PIE};
    EXPECT_TRUE(arc.isEmpty());
    EXPECT_TRUE(arc.segments().empty());
    EXPECT_FALSE(arc.contains({-1, 1}));
    // An empty arc's bounds are its frame, exactly.
    EXPECT_EQ(arc.bounds(), (Rectangle2D{.x = 0.0, .y = 0.0, .width = -2.0, .height = 2.0}));
}

/// One containsAngle() answer.
struct AngleCase
{
    constexpr AngleCase(double a, bool isContained) noexcept : angle(a), contained(isContained) { }

    double angle;
    bool   contained;
};

/// The angles that differ from Java's answers, empty when none does.
[[nodiscard]] std::string wrongAngles(const FigureArc& arc, std::span<const AngleCase> cases)
{
    std::string wrong;
    for (const AngleCase& c : cases)
    {
        if (arc.containsAngle(c.angle) != c.contained)
        {
            wrong += std::to_string(c.angle) + " ";
        }
    }
    return wrong;
}

TEST(FigureArc, ContainsAngleNormalisesBothAngles)
{
    // PINS-BEGIN (ShapeProbe.java)
    constexpr std::array<AngleCase, 15> kStart1000Extent100{{
        {-900.0, false},
        {-540.0, false},
        {-180.0, false},
        {-179.5, false},
        {0.0, true},
        {80.0, false},
        {279.99999999999994, false},
        {280.0, true},
        {281.0, true},
        {360.0, true},
        {900.0, false},
        {1000.0, true},
        {1099.0, true},
        {1100.0, false},
        {1260.0, false},
    }};
    constexpr std::array<AngleCase, 15> kStartMinus540ExtentMinus90{{
        {-900.0, true},
        {-540.0, true},
        {-180.0, true},
        {-179.5, false},
        {0.0, false},
        {80.0, false},
        {279.99999999999994, false},
        {280.0, false},
        {281.0, false},
        {360.0, false},
        {900.0, true},
        {1000.0, false},
        {1099.0, false},
        {1100.0, false},
        {1260.0, true},
    }};
    constexpr std::array<AngleCase, 15> kStart170Extent20{{
        {-900.0, true},
        {-540.0, true},
        {-180.0, true},
        {-179.5, true},
        {0.0, false},
        {80.0, false},
        {279.99999999999994, false},
        {280.0, false},
        {281.0, false},
        {360.0, false},
        {900.0, true},
        {1000.0, false},
        {1099.0, false},
        {1100.0, false},
        {1260.0, true},
    }};
    // PINS-END
    EXPECT_EQ(wrongAngles(FigureArc{0, 0, 1, 1, 1000, 100, Type::OPEN}, kStart1000Extent100), "");
    EXPECT_EQ(
        wrongAngles(FigureArc{0, 0, 1, 1, -540, -90, Type::OPEN}, kStartMinus540ExtentMinus90), "");
    EXPECT_EQ(wrongAngles(FigureArc{0, 0, 1, 1, 170, 20, Type::OPEN}, kStart170Extent20), "");
}

TEST(FigureArc, ContainsAngleOfAFullOrEmptyArc)
{
    // A full turn either way contains every angle; a zero extent none (Java's answers for the
    // same fifteen angles as above were all true and all false).
    constexpr std::array<double, 5> kAngles{-900.0, -179.5, 0.0, 279.99999999999994, 1260.0};
    for (const double angle : kAngles)
    {
        EXPECT_TRUE((FigureArc{0, 0, 1, 1, 10, 360, Type::OPEN}.containsAngle(angle))) << angle;
        EXPECT_TRUE((FigureArc{0, 0, 1, 1, 10, -360, Type::OPEN}.containsAngle(angle))) << angle;
        EXPECT_FALSE((FigureArc{0, 0, 1, 1, 0, 0, Type::OPEN}.containsAngle(angle))) << angle;
    }
}

}  // namespace
