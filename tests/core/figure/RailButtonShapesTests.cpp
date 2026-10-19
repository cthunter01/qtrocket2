#include "QtRocket/figure/RailButtonShapes.h"

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/AffineTransform2D.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::AffineTransform2D;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentShape;
using QtRocket::FigureArc;
using QtRocket::FigurePath;
using QtRocket::FigureShape;
using QtRocket::FigureView;
using QtRocket::PathSegment;
using QtRocket::RailButton;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltPin;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace RailButtonShapes = QtRocket::RailButtonShapes;

/// Success when the hand-built case @p name of @p button is as Java's (the view angle goes
/// through atan2, sin and cos).
[[nodiscard]] ::testing::AssertionResult buttonAsJava(const RailButton& button, const char* name)
{
    return handBuiltVariantsAsJava(&RailButtonShapes::getShapesSide,
                                   &RailButtonShapes::getShapesBack, button, kHandBuilt, name,
                                   kTranscendentalTolerance);
}

/// The probe's button with a screw head 2 mm high.
[[nodiscard]] RailButton buttonWithScrew()
{
    RailButton screw;
    screw.setScrewHeight(0.002);
    return screw;
}

TEST(RailButtonShapes, AsJava)
{
    const RailButton button;
    EXPECT_TRUE(buttonAsJava(button, "rail-default"));

    EXPECT_TRUE(buttonAsJava(buttonWithScrew(), "rail-screw"));

    RailButton noBase;
    noBase.setBaseHeight(0);
    noBase.setScrewHeight(0.0015);
    EXPECT_TRUE(buttonAsJava(noBase, "rail-no-base"));

    RailButton bare;
    bare.setFlangeHeight(0);
    bare.setScrewHeight(0);
    EXPECT_TRUE(buttonAsJava(bare, "rail-no-flange"));

    RailButton angled;
    angled.setAngleOffset(0.3);
    angled.setScrewHeight(0.002);
    EXPECT_TRUE(buttonAsJava(angled, "rail-angle"));
}

TEST(RailButtonShapes, TurnedAboutTheAxisAsJava)
{
    // Turned by 1.6, PI and 3 PI / 2 about the axis: the cosine of the view angle is negative,
    // or the screw head hides in the flange's face.
    const RailButton screw = buttonWithScrew();
    for (const char* name : {"rail-screw/rot1", "rail-screw/rot3", "rail-screw/rot5"})
    {
        EXPECT_TRUE(buttonAsJava(screw, name));
    }
}

TEST(RailButtonShapes, TheSideViewEndsWithTheInvisibleHitTestRectangles)
{
    const RailButton                  screw = buttonWithScrew();
    const std::vector<ComponentShape> side =
        RailButtonShapes::getShapesSide(screw, Transformation::translation(0.25, 0, 0));
    ASSERT_EQ(side.size(), 3U);
    EXPECT_TRUE(side[0].isVisible());
    EXPECT_NE(side[0].shape().getIf<FigurePath>(), nullptr);
    const auto* head = side[1].shape().getIf<FigureArc>();
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->type(), FigureArc::Type::OPEN);
    EXPECT_FALSE(side[2].isVisible());
    EXPECT_EQ(side[2].color(), Color::invisible());
    // One rectangle (move, four lines, close) for each of the three cylinders.
    const auto* rectangles = side[2].shape().getIf<FigurePath>();
    ASSERT_NE(rectangles, nullptr);
    EXPECT_EQ(rectangles->segments().size(), 3U * 6U);

    // Without a screw there is no head.
    const RailButton bare;
    EXPECT_EQ(RailButtonShapes::getShapesSide(bare, Transformation::kIdentity).size(), 2U);
}

TEST(RailButtonShapes, AHeadHiddenInTheFlangeHasNoExtent)
{
    // rot1 turns the button by 1.6 rad: |screw height x cos| is below the flange face's half
    // height, so Java draws the head with an extent of -signum(0) * 180 = -0 degrees.
    const RailButton screw = buttonWithScrew();
    const auto&      pin   = handBuiltPin(kHandBuilt, "rail-screw/rot1", FigureView::SIDE);
    const auto side = RailButtonShapes::getShapesSide(screw, pinnedTransformation(pin.transform));
    ASSERT_EQ(side.size(), 3U);
    const auto* head = side[1].shape().getIf<FigureArc>();
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->angleExtent(), 0.0);
    EXPECT_TRUE(std::signbit(head->angleExtent()));
    EXPECT_EQ(head->height(), 0.0);
}

TEST(RailButtonShapes, TheRotatedRectangleStandsOnItsBase)
{
    // Upright: from the lower left (x - r, y) up to (x - r, y + h), across and down.
    const FigurePath upright = RailButtonShapes::getRotatedRectangle(0.5, 0.25, 0.125, 1, 0);
    EXPECT_EQ(upright.segments(),
              (std::vector<PathSegment>{PathSegment::moveTo({0.375, 0.25}),
                                        PathSegment::lineTo({0.375, 1.25}),
                                        PathSegment::lineTo({0.625, 1.25}),
                                        PathSegment::lineTo({0.625, 0.25}), PathSegment::close()}));
}

TEST(RailButtonShapes, TheRotatedShapeIsAffineTransformsRotation)
{
    const FigureShape arc{FigureArc{0.1, 0.2, 0.04, 0.02, 0, -180, FigureArc::Type::OPEN}};
    EXPECT_EQ(RailButtonShapes::getRotatedShape(arc, 0.12, 0.2, 0.4),
              AffineTransform2D::rotation(0.4, 0.12, 0.2).createTransformedShape(arc));
}

TEST(RailButtonShapes, AnotherComponentIsABug)
{
    const BodyTube tube;
    EXPECT_THROW((void)RailButtonShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)RailButtonShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
