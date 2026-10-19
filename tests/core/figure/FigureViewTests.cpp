#include "QtRocket/figure/FigureView.h"

#include <array>
#include <cstddef>
#include <numbers>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::figureRotation;
using QtRocket::FigureView;
using QtRocket::InnerTube;
using QtRocket::Point2D;
using QtRocket::Transformation;
using QtRocket::Test::figureValueNear;
using QtRocket::Test::kTranscendentalTolerance;

TEST(FigureView, NamesAndTextsFollowRocketPanel)
{
    EXPECT_EQ(QtRocket::kAllFigureViews,
              (std::array<FigureView, 3>{FigureView::TOP, FigureView::SIDE, FigureView::BACK}));
    EXPECT_EQ(QtRocket::kDefaultFigureView, FigureView::SIDE);
    EXPECT_EQ(QtRocket::figureViewName(FigureView::TOP), "TOP");
    EXPECT_EQ(QtRocket::figureViewName(FigureView::SIDE), "SIDE");
    EXPECT_EQ(QtRocket::figureViewName(FigureView::BACK), "BACK");
    EXPECT_EQ(QtRocket::displayKey(FigureView::TOP), "RocketPanel.FigTypeAct.TopView");
    EXPECT_EQ(QtRocket::displayKey(FigureView::SIDE), "RocketPanel.FigTypeAct.SideView");
    EXPECT_EQ(QtRocket::displayKey(FigureView::BACK), "RocketPanel.FigTypeAct.BackView");
    // messages.properties:78-80
    EXPECT_EQ(QtRocket::displayName(FigureView::TOP), "Top view");
    EXPECT_EQ(QtRocket::displayName(FigureView::SIDE), "Side view");
    EXPECT_EQ(QtRocket::displayName(FigureView::BACK), "Back view");
}

TEST(FigureView, TheBackViewAloneUsesTheBackShapes)
{
    static_assert(QtRocket::usesSideShapes(FigureView::TOP));
    static_assert(QtRocket::usesSideShapes(FigureView::SIDE));
    static_assert(!QtRocket::usesSideShapes(FigureView::BACK));
}

TEST(FigureView, ProjectionsTakeXOrZWithY)
{
    const Coordinate c{0.3, 0.02, -0.01};
    EXPECT_EQ(QtRocket::projectToView(FigureView::SIDE, c), (Point2D{0.3, 0.02}));
    EXPECT_EQ(QtRocket::projectToView(FigureView::TOP, c), (Point2D{0.3, 0.02}));
    EXPECT_EQ(QtRocket::projectToView(FigureView::BACK, c), (Point2D{-0.01, 0.02}));
}

TEST(FigureView, TheTopViewOffsetsTheRotationByAQuarterTurn)
{
    // RocketFigure.getRotation(true)
    EXPECT_EQ(QtRocket::rotationWithTopViewOffset(FigureView::SIDE, 0.5), 0.5);
    EXPECT_EQ(QtRocket::rotationWithTopViewOffset(FigureView::BACK, 0.5), 0.5);
    EXPECT_EQ(QtRocket::rotationWithTopViewOffset(FigureView::TOP, 0.5),
              0.5 - (std::numbers::pi / 2));
}

TEST(FigureView, DisplayOrdersFollowTheView)
{
    // BodyTube's display orders are 0 (side) and 1 (back), InnerTube's 5 and 14.
    const BodyTube  tube;
    const InnerTube inner;
    EXPECT_EQ(QtRocket::displayOrder(FigureView::SIDE, tube), 0);
    EXPECT_EQ(QtRocket::displayOrder(FigureView::TOP, tube), 0);
    EXPECT_EQ(QtRocket::displayOrder(FigureView::BACK, tube), 1);
    EXPECT_EQ(QtRocket::displayOrder(FigureView::SIDE, inner), 5);
    EXPECT_EQ(QtRocket::displayOrder(FigureView::BACK, inner), 14);
}

// RocketFigure.getFigureRotation for a figure rotated by `rotation`, as OpenRocket's
// Transformation computes it (JDK 17, the probe ShapeProbe.java of the port's scratch
// directory): the matrices of the side (and back) view, rotate_x(rotation), and of the top view,
// rotate_x(rotation).applyTransformation(rotate_x(-PI/2)), and where they take the coordinate
// (0.3, 0.02, -0.01). The rotations take sines and cosines, so values are compared within
// kTranscendentalTolerance.
struct ViewRotationCase
{
    double                  rotation;
    Transformation::Matrix3 side;
    Transformation::Matrix3 top;
    std::array<double, 3>   sidePoint;
    std::array<double, 3>   topPoint;
};

// NOLINTBEGIN(modernize-use-std-numbers): Java's results, not approximations of constants
// PINS-BEGIN (ShapeProbe.java)
constexpr std::array<ViewRotationCase, 6> kViewRotations{{
    {.rotation = 0.0,
     .side     = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}},
     .top =
         {{{1.0, 0.0, 0.0}, {0.0, 6.123233995736766E-17, 1.0}, {0.0, -1.0, 6.123233995736766E-17}}},
     .sidePoint = {0.3, 0.02, -0.01},
     .topPoint  = {0.3, -0.009999999999999998, -0.02}},
    {.rotation = 0.5235987755982988,
     .side     = {{{1.0, 0.0, 0.0},
                   {0.0, 0.8660254037844387, -0.49999999999999994},
                   {0.0, 0.49999999999999994, 0.8660254037844387}}},
     .top = {{{1.0, 0.0, 0.0}, {0.0, 0.5, 0.8660254037844387}, {0.0, -0.8660254037844387, 0.5}}},
     .sidePoint = {0.3, 0.02232050807568877, 0.001339745962155612},
     .topPoint  = {0.3, 0.0013397459621556137, -0.022320508075688774}},
    {.rotation = 1.5707963267948966,
     .side =
         {{{1.0, 0.0, 0.0}, {0.0, 6.123233995736766E-17, -1.0}, {0.0, 1.0, 6.123233995736766E-17}}},
     .top       = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}},
     .sidePoint = {0.3, 0.010000000000000002, 0.02},
     .topPoint  = {0.3, 0.02, -0.01}},
    {.rotation  = 1.0,
     .side      = {{{1.0, 0.0, 0.0},
                    {0.0, 0.5403023058681398, -0.8414709848078965},
                    {0.0, 0.8414709848078965, 0.5403023058681398}}},
     .top       = {{{1.0, 0.0, 0.0},
                    {0.0, 0.8414709848078965, 0.5403023058681398},
                    {0.0, -0.5403023058681398, 0.8414709848078965}}},
     .sidePoint = {0.3, 0.01922075596544176, 0.011426396637476533},
     .topPoint  = {0.3, 0.011426396637476533, -0.01922075596544176}},
    {.rotation  = 6.183185307179587,
     .side      = {{{1.0, 0.0, 0.0},
                    {0.0, 0.9950041652780258, 0.09983341664682804},
                    {0.0, -0.09983341664682804, 0.9950041652780258}}},
     .top       = {{{1.0, 0.0, 0.0},
                    {0.0, -0.09983341664682799, 0.9950041652780258},
                    {0.0, -0.9950041652780258, -0.09983341664682799}}},
     .sidePoint = {0.3, 0.018901749139092237, -0.011946709985716819},
     .topPoint  = {0.3, -0.011946709985716819, -0.018901749139092237}},
    {.rotation = 1.0E-10,
     .side     = {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}},
     .top =
         {{{1.0, 0.0, 0.0}, {0.0, 6.123233995736766E-17, 1.0}, {0.0, -1.0, 6.123233995736766E-17}}},
     .sidePoint = {0.3, 0.02, -0.01},
     .topPoint  = {0.3, -0.009999999999999998, -0.02}},
}};
// PINS-END
// NOLINTEND(modernize-use-std-numbers)

/// The entries of @p actual that differ from @p expected beyond the tolerance, as text.
[[nodiscard]] std::string matrixDifferences(const Transformation::Matrix3& actual,
                                            const Transformation::Matrix3& expected)
{
    std::string wrong;
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            const double a = actual.at(row).at(column);
            const double e = expected.at(row).at(column);
            if (!figureValueNear(a, e, kTranscendentalTolerance))
            {
                wrong += "[" + std::to_string(row) + "][" + std::to_string(column) + "] ";
            }
        }
    }
    return wrong;
}

/// The coordinates of @p actual that differ from @p expected beyond the tolerance, as text.
[[nodiscard]] std::string pointDifferences(const Coordinate&            actual,
                                           const std::array<double, 3>& expected)
{
    std::string wrong;
    if (!figureValueNear(actual.x, expected[0], kTranscendentalTolerance))
    {
        wrong += "x ";
    }
    if (!figureValueNear(actual.y, expected[1], kTranscendentalTolerance))
    {
        wrong += "y ";
    }
    if (!figureValueNear(actual.z, expected[2], kTranscendentalTolerance))
    {
        wrong += "z ";
    }
    return wrong;
}

/// Everything about @p c's rotations that differs from Java's, empty when nothing does.
[[nodiscard]] std::string viewRotationDifferences(const ViewRotationCase& c)
{
    const Coordinate     point{0.3, 0.02, -0.01};
    const Transformation side = figureRotation(FigureView::SIDE, c.rotation);
    const Transformation back = figureRotation(FigureView::BACK, c.rotation);
    const Transformation top  = figureRotation(FigureView::TOP, c.rotation);
    std::string          wrong;
    if (const std::string d = matrixDifferences(side.matrix(), c.side); !d.empty())
    {
        wrong += "side matrix " + d;
    }
    if (!(back == side))
    {
        wrong += "back differs from side ";
    }
    if (const std::string d = matrixDifferences(top.matrix(), c.top); !d.empty())
    {
        wrong += "top matrix " + d;
    }
    if (const std::string d = pointDifferences(side.transform(point), c.sidePoint); !d.empty())
    {
        wrong += "side point " + d;
    }
    if (const std::string d = pointDifferences(top.transform(point), c.topPoint); !d.empty())
    {
        wrong += "top point " + d;
    }
    return wrong;
}

TEST(FigureView, FigureRotationsMatchRocketFigure)
{
    for (const ViewRotationCase& c : kViewRotations)
    {
        EXPECT_EQ(viewRotationDifferences(c), "") << "rotation " << c.rotation;
    }
}

TEST(FigureView, ARotationBelowTheAngleEpsilonIsTheIdentity)
{
    // rotate_x skips angles below ANGLE_EPSILON, so the side view of 1e-10 is the identity
    // exactly and the top view is rotate_x(-PI/2) exactly.
    EXPECT_TRUE(figureRotation(FigureView::SIDE, 1e-10).isIdentity());
    EXPECT_EQ(figureRotation(FigureView::TOP, 1e-10),
              Transformation::rotateX(-std::numbers::pi / 2));
    EXPECT_EQ(figureRotation(FigureView::TOP, 0.0).matrix(),
              Transformation::rotateX(-std::numbers::pi / 2).matrix());
}

}  // namespace
