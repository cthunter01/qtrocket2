#include "QtRocket/figure/TransitionShapes.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/SymmetricComponentShapes.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigureEllipse;
using QtRocket::FigurePath;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::FigureView;
using QtRocket::NoseCone;
using QtRocket::PathSegment;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::Test::handBuiltAsJava;
using QtRocket::Test::handBuiltCasesAsJava;
using QtRocket::Test::handBuiltPin;
using QtRocket::Test::HandBuiltPin;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::onlyPathOf;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::shapesMatchPins;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace TransitionShapes = QtRocket::TransitionShapes;

/// TransitionShapes::getShapesSide() as the figure calls it (scale factor 1).
std::vector<ComponentShape> transitionSide(const RocketComponent& component,
                                           const Transformation&  transformation)
{
    return TransitionShapes::getShapesSide(component, transformation);
}

/// The Transition of the probe's transition(shape, param, length, fore, aft).
std::unique_ptr<Transition> makeTransition(TransitionShape shape, double param, double length,
                                           double fore, double aft)
{
    auto t = std::make_unique<Transition>();
    t->setShapeType(shape);
    t->setShapeParameter(param);
    t->setLength(length);
    t->setForeRadius(fore);
    t->setAftRadius(aft);
    return t;
}

/// @p v rounded to a float, as a Path2D.Float stores it.
double asFloat(double v)
{
    return static_cast<double>(static_cast<float>(v));
}

/// Success when the case @p name in @p view is what TransitionShapes returned in Java.
::testing::AssertionResult transitionAsJava(const RocketComponent& component, const char* name,
                                            FigureView view, double tolerance = 0)
{
    return handBuiltAsJava(&transitionSide, &TransitionShapes::getShapesBack, component, kHandBuilt,
                           name, view, tolerance);
}

TEST(TransitionShapes, ConicalTransitionsAsJava)
{
    // A closed Path2D.Float, plus the shoulders longer than 0.0005 m: the fore one here...
    std::unique_ptr<Transition> conical =
        makeTransition(TransitionShape::CONICAL, 0, 0.05, 0.01, 0.02);
    conical->setForeShoulderLength(0.02);
    conical->setForeShoulderRadius(0.009);
    conical->setAftShoulderLength(0.0005);
    conical->setAftShoulderRadius(0.019);
    EXPECT_TRUE(handBuiltCasesAsJava(&transitionSide, &TransitionShapes::getShapesBack, *conical,
                                     kHandBuilt, "transition-conical-fore-shoulder"));

    // ... the aft one here.
    std::unique_ptr<Transition> conical2 =
        makeTransition(TransitionShape::CONICAL, 0, 0.05, 0.02, 0.01);
    conical2->setForeShoulderLength(0.0005);
    conical2->setForeShoulderRadius(0.019);
    conical2->setAftShoulderLength(0.0006);
    conical2->setAftShoulderRadius(0.009);
    EXPECT_TRUE(handBuiltCasesAsJava(&transitionSide, &TransitionShapes::getShapesBack, *conical2,
                                     kHandBuilt, "transition-conical-aft-shoulder"));
}

TEST(TransitionShapes, SampledTransitionsAsJava)
{
    // The other shapes are SymmetricComponentShapes' outline: without a transformation its
    // points are the sampled and refined profile.
    EXPECT_TRUE(transitionAsJava(*makeTransition(TransitionShape::OGIVE, 0.6, 0.08, 0.005, 0.02),
                                 "transition-ogive", FigureView::SIDE));
    EXPECT_TRUE(
        transitionAsJava(*makeTransition(TransitionShape::ELLIPSOID, 0.0, 0.08, 0.005, 0.02),
                         "transition-ellipsoid", FigureView::SIDE));
    // The Haack radius goes through acos and sin, whose last bits differ between Java and the C
    // libraries.
    EXPECT_TRUE(transitionAsJava(*makeTransition(TransitionShape::HAACK, 0.0, 0.08, 0.005, 0.02),
                                 "transition-haack", FigureView::SIDE, kTranscendentalTolerance));
    // Samples closer than 1 mm are never refined.
    EXPECT_TRUE(transitionAsJava(*makeTransition(TransitionShape::OGIVE, 1.0, 0.0005, 0.01, 0.02),
                                 "transition-short", FigureView::SIDE));
    EXPECT_TRUE(transitionAsJava(*makeTransition(TransitionShape::ELLIPSOID, 0, 0, 0.01, 0.02),
                                 "transition-zero-length", FigureView::BACK));
}

TEST(TransitionShapes, RefinedOutlinesAsJava)
{
    // Steep tips, where the refinement inserts points (the power profile through pow).
    EXPECT_TRUE(transitionAsJava(NoseCone{TransitionShape::ELLIPSOID, 0.5, 0.1},
                                 "refine-ellipsoid-nose", FigureView::SIDE));
    NoseCone steepPower{TransitionShape::POWER, 0.3, 0.05};
    steepPower.setShapeParameter(0.1);
    EXPECT_TRUE(transitionAsJava(steepPower, "refine-power-nose", FigureView::SIDE,
                                 kTranscendentalTolerance));
    // Samples 1.06 mm apart, just above the 1 mm below which no segment is split: two points at
    // the tip.
    EXPECT_TRUE(transitionAsJava(NoseCone{TransitionShape::ELLIPSOID, 0.095, 0.05},
                                 "refine-short-ellipsoid-nose", FigureView::SIDE));
}

TEST(TransitionShapes, ABoattailWithBothShouldersAsJava)
{
    std::unique_ptr<Transition> boattail =
        makeTransition(TransitionShape::OGIVE, 1.0, 0.05, 0.02, 0.012);
    boattail->setForeShoulderLength(0.01);
    boattail->setForeShoulderRadius(0.019);
    boattail->setAftShoulderLength(0.01);
    boattail->setAftShoulderRadius(0.011);
    EXPECT_TRUE(transitionAsJava(*boattail, "transition-boattail/axial", FigureView::SIDE));
    EXPECT_TRUE(transitionAsJava(*boattail, "transition-boattail/axial", FigureView::BACK));
    EXPECT_TRUE(transitionAsJava(*boattail, "transition-boattail/off", FigureView::BACK));
}

TEST(TransitionShapes, NoseConesAsJava)
{
    NoseCone ogive{TransitionShape::OGIVE, 0.1, 0.0125};
    ogive.setShoulderLength(0.02);
    ogive.setShoulderRadius(0.0115);
    EXPECT_TRUE(transitionAsJava(ogive, "nose-ogive", FigureView::BACK));

    // A tail cone: the shoulder is in front.
    NoseCone tail{TransitionShape::ELLIPSOID, 0.1, 0.025};
    tail.setShoulderLength(0.03);
    tail.setShoulderRadius(0.024);
    tail.setFlipped(true);
    EXPECT_TRUE(transitionAsJava(tail, "nose-ellipsoid-flipped", FigureView::BACK));

    const NoseCone conical{TransitionShape::CONICAL, 0.07, 0.01};
    EXPECT_TRUE(transitionAsJava(conical, "nose-conical", FigureView::SIDE));
    EXPECT_TRUE(transitionAsJava(conical, "nose-conical", FigureView::BACK));
}

TEST(TransitionShapes, APrintedNoseConeIsScaledAsJava)
{
    // PrintableNoseCone: TransitionShapes.getShapesSide(cone, IDENTITY, the points of a metre),
    // PrintUnit.METERS.toPoints(1) = 2834.6456664000002 in Java.
    NoseCone ogive{TransitionShape::OGIVE, 0.1, 0.0125};
    ogive.setShoulderLength(0.02);
    ogive.setShoulderRadius(0.0115);
    const HandBuiltPin& pin = handBuiltPin(kHandBuilt, "nose-ogive-printed", FigureView::SIDE);
    EXPECT_EQ(pin.scale, 2834.6456664000002);
    EXPECT_TRUE(shapesMatchPins(
        TransitionShapes::getShapesSide(ogive, pinnedTransformation(pin.transform), pin.scale),
        pin.shapes, ogive));
}

TEST(TransitionShapes, AConicalTransitionIsAClosedFloatPathOfFourPoints)
{
    const std::unique_ptr<Transition> conical =
        makeTransition(TransitionShape::CONICAL, 0, 0.05, 0.01, 0.02);
    const std::vector<ComponentShape> shapes =
        TransitionShapes::getShapesSide(*conical, Transformation::translation(0.1, 0, 0));
    const FigurePath& path = onlyPathOf(shapes);
    EXPECT_EQ(path.precision(), FigurePath::Precision::FLOAT);
    EXPECT_EQ(path.segments(), (std::vector<PathSegment>{
                                   PathSegment::moveTo({asFloat(0.1), asFloat(0 + 0.01)}),
                                   PathSegment::lineTo({asFloat(0.1 + 0.05), asFloat(0 + 0.02)}),
                                   PathSegment::lineTo({asFloat(0.1 + 0.05), asFloat(0 - 0.02)}),
                                   PathSegment::lineTo({asFloat(0.1), asFloat(0 - 0.01)}),
                                   PathSegment::close(),
                               }));
}

TEST(TransitionShapes, TheOtherShapesAreTheSymmetricComponentOutline)
{
    const std::unique_ptr<Transition> ogive =
        makeTransition(TransitionShape::OGIVE, 1.0, 0.08, 0.005, 0.02);
    const Transformation              t      = Transformation::translation(0.1, 0.002, 0);
    const std::vector<ComponentShape> shapes = TransitionShapes::getShapesSide(*ogive, t);
    const std::vector<ComponentShape> outline =
        QtRocket::SymmetricComponentShapes::getShapesSide(*ogive, t);
    ASSERT_EQ(shapes.size(), 1U);
    ASSERT_EQ(outline.size(), 1U);
    EXPECT_EQ(shapes[0].shape(), outline[0].shape());
}

TEST(TransitionShapes, ShouldersAreTubesBeforeAndBehindTheTransitionInThatOrder)
{
    std::unique_ptr<Transition> conical =
        makeTransition(TransitionShape::CONICAL, 0, 0.05, 0.01, 0.02);
    conical->setForeShoulderLength(0.02);
    conical->setForeShoulderRadius(0.009);
    conical->setAftShoulderLength(0.03);
    conical->setAftShoulderRadius(0.019);
    const std::vector<ComponentShape> shapes =
        TransitionShapes::getShapesSide(*conical, Transformation::translation(0.1, 0, 0));
    ASSERT_EQ(shapes.size(), 3U);
    EXPECT_EQ(shapes[1].shape(),
              (FigureShape{FigureRect{0.1 + -0.02, 0 - 0.009, 0.02, 2 * 0.009}}));
    EXPECT_EQ(shapes[2].shape(), (FigureShape{FigureRect{0.1 + 0.05, 0 - 0.019, 0.03, 2 * 0.019}}));

    // Up to 0.0005 m, a shoulder is not drawn.
    conical->setForeShoulderLength(TransitionShapes::kMinimumShoulderLength);
    conical->setAftShoulderLength(TransitionShapes::kMinimumShoulderLength);
    EXPECT_EQ(TransitionShapes::getShapesSide(*conical, Transformation::kIdentity).size(), 1U);
}

TEST(TransitionShapes, TheBackIsTheForeAndTheAftCircle)
{
    const std::unique_ptr<Transition> conical =
        makeTransition(TransitionShape::CONICAL, 0, 0.05, 0.01, 0.02);
    const std::vector<ComponentShape> shapes =
        TransitionShapes::getShapesBack(*conical, Transformation::translation(0.1, 0.003, -0.004));
    ASSERT_EQ(shapes.size(), 2U);
    EXPECT_EQ(shapes[0].shape(),
              (FigureShape{FigureEllipse{-0.004 - 0.01, 0.003 - 0.01, 2 * 0.01, 2 * 0.01}}));
    EXPECT_EQ(shapes[1].shape(),
              (FigureShape{FigureEllipse{-0.004 - 0.02, 0.003 - 0.02, 2 * 0.02, 2 * 0.02}}));
}

TEST(TransitionShapes, AnotherComponentIsABug)
{
    // Java's cast to Transition throws a ClassCastException.
    const BodyTube tube;
    EXPECT_THROW((void)TransitionShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)TransitionShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
