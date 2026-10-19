#include "QtRocket/figure/RocketFigureGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketComponentShapeProvider.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"
#include "figure/DesignPinSupport.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::ComponentKind;
using QtRocket::FigureView;
using QtRocket::FlightConfiguration;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Point2D;
using QtRocket::Rectangle2D;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketFigureGeometry;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;

/// The figure of @p rocket's selected configuration in @p view, unrotated, without selection and
/// with the markers preference off.
[[nodiscard]] RocketFigureGeometry figureOf(const Rocket& rocket,
                                            FigureView    view = FigureView::SIDE)
{
    return RocketFigureGeometry::build(rocket.getSelectedConfiguration(), view, 0, {}, false);
}

/// How many shapes of @p geometry belong to components of @p kind.
[[nodiscard]] std::size_t shapesOfKind(const RocketFigureGeometry& geometry, ComponentKind kind)
{
    std::size_t count = 0;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        if (shape.shape.component().kind() == kind)
        {
            ++count;
        }
    }
    return count;
}

/// Success when @p bounds has only finite fields.
[[nodiscard]] ::testing::AssertionResult finiteBounds(const Rectangle2D& bounds)
{
    if (std::isfinite(bounds.x) && std::isfinite(bounds.y) && std::isfinite(bounds.width) &&
        std::isfinite(bounds.height))
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << bounds;
}

TEST(RocketFigureGeometry, ANewDesignHasFiniteBoundsAndNoShapes)
{
    // RocketFigure.updateSubjectDimensions of a design without components: the bounding box is
    // (0, 0, 0) to (1, 0, 0), so maxR is 0.
    const auto                 document = OpenRocketDocumentFactory::createNewRocket();
    const RocketFigureGeometry side     = figureOf(document->getRocket(), FigureView::SIDE);
    const RocketFigureGeometry top      = figureOf(document->getRocket(), FigureView::TOP);
    const RocketFigureGeometry back     = figureOf(document->getRocket(), FigureView::BACK);
    EXPECT_TRUE(side.shapes().empty());
    EXPECT_TRUE(side.motors().empty());
    EXPECT_TRUE(back.shapes().empty());
    EXPECT_TRUE(finiteBounds(side.subjectBounds()));
    EXPECT_TRUE(finiteBounds(back.subjectBounds()));
    EXPECT_EQ(side.subjectBounds(), (Rectangle2D{.x = 0, .y = -0.0, .width = 1, .height = 0}));
    EXPECT_EQ(top.subjectBounds(), side.subjectBounds());
    EXPECT_EQ(back.subjectBounds(), (Rectangle2D{.x = -0.0, .y = -0.0, .width = 0, .height = 0}));
}

/// Success when @p a and @p b hold the same shapes of the same instances in the same order, and
/// the display orders never rise.
[[nodiscard]] ::testing::AssertionResult sameDrawingOrder(const RocketFigureGeometry& a,
                                                          const RocketFigureGeometry& b)
{
    if (a.shapes().size() != b.shapes().size() || a.shapes().empty())
    {
        return ::testing::AssertionFailure() << "other shape counts";
    }
    for (std::size_t i = 0; i < a.shapes().size(); ++i)
    {
        const auto& x = a.shapes()[i];
        const auto& y = b.shapes()[i];
        if (&x.shape.component() != &y.shape.component() || x.shape.shape() != y.shape.shape() ||
            x.contextIndex != y.contextIndex)
        {
            return ::testing::AssertionFailure() << "shape " << i << " differs";
        }
        if (i > 0 && QtRocket::displayOrder(a.view(), a.shapes()[i - 1].shape.component()) <
                         QtRocket::displayOrder(a.view(), x.shape.component()))
        {
            return ::testing::AssertionFailure() << "shape " << i << " is out of order";
        }
    }
    return ::testing::AssertionSuccess();
}

TEST(RocketFigureGeometry, TheShapesAreDrawnInAStableOrder)
{
    // Deviation (U4): Java's PriorityQueue orders only by display order; here equal display
    // orders keep the order of the instance maps, so two builds give the same list.
    const TestEstesAlphaIII alpha;
    for (const FigureView view : QtRocket::kAllFigureViews)
    {
        EXPECT_TRUE(sameDrawingOrder(figureOf(*alpha.rocket, view), figureOf(*alpha.rocket, view)));
    }
}

/// Success when every shape of @p geometry, made from @p config, is one its provider gives for its
/// instance in @p view, from the active instances.
[[nodiscard]] ::testing::AssertionResult providersShapes(const RocketFigureGeometry& geometry,
                                                         const FlightConfiguration&  config)
{
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        const RocketComponent& component = shape.shape.component();
        const auto contexts = config.getActiveInstances().getInstanceContexts(component);
        const auto index    = static_cast<std::size_t>(shape.contextIndex);
        if (shape.extraRender || index >= contexts.size() ||
            contexts[index].instanceNumber != shape.instanceNumber)
        {
            return ::testing::AssertionFailure() << component.getName() << ": no such instance";
        }
        const auto expected = QtRocket::RocketComponentShapeProvider::getShapes(
            geometry.view(), component,
            QtRocket::figureRotation(geometry.view(), geometry.rotation())
                .applyTransformation(contexts[index].transform));
        const bool found = std::ranges::any_of(expected, [&shape](const auto& candidate) {
            return candidate.shape() == shape.shape.shape();
        });
        if (!found)
        {
            return ::testing::AssertionFailure() << component.getName() << ": another shape";
        }
    }
    return ::testing::AssertionSuccess();
}

TEST(RocketFigureGeometry, EveryInstanceGetsItsProvidersShapes)
{
    // The fin set's three fins, each with the fin set's three polylines in the side view.
    const TestEstesAlphaIII    alpha;
    const RocketFigureGeometry geometry = figureOf(*alpha.rocket);
    EXPECT_EQ(shapesOfKind(geometry, ComponentKind::TRAPEZOID_FIN_SET), 3U * 3U);
    // The rocket and its stage have none.
    EXPECT_EQ(shapesOfKind(geometry, ComponentKind::ROCKET), 0U);
    EXPECT_EQ(shapesOfKind(geometry, ComponentKind::AXIAL_STAGE), 0U);
    // Each shape is the provider's for its instance's transformation.
    EXPECT_TRUE(providersShapes(geometry, alpha.rocket->getSelectedConfiguration()));
    EXPECT_TRUE(
        providersShapes(RocketFigureGeometry::build(alpha.rocket->getSelectedConfiguration(),
                                                    FigureView::TOP, 0.4, {}, false),
                        alpha.rocket->getSelectedConfiguration()));
}

TEST(RocketFigureGeometry, TheMarkersPreferenceShowsOnlySelectedBoosterSets)
{
    const TestFalcon9Heavy     falcon;
    const FlightConfiguration& config   = falcon.rocket->getSelectedConfiguration();
    const RocketComponent&     boosters = *falcon.boosterStage;

    // Off: every booster set has its marker, five shapes and the selection square per instance.
    const auto all = RocketFigureGeometry::build(config, FigureView::SIDE, 0, {}, false);
    EXPECT_EQ(shapesOfKind(all, ComponentKind::PARALLEL_STAGE), 2U * 6U);

    // On: only when the booster set is selected; its children are drawn either way.
    const auto none = RocketFigureGeometry::build(config, FigureView::SIDE, 0, {}, true);
    EXPECT_EQ(shapesOfKind(none, ComponentKind::PARALLEL_STAGE), 0U);
    EXPECT_EQ(none.shapes().size(), all.shapes().size() - std::size_t{12});
    const std::vector<const RocketComponent*> selection{&boosters};
    const auto selected = RocketFigureGeometry::build(config, FigureView::SIDE, 0, selection, true);
    EXPECT_EQ(shapesOfKind(selected, ComponentKind::PARALLEL_STAGE), 2U * 6U);
}

TEST(RocketFigureGeometry, ABoosterSetWithoutChildrenIsAnExtraRenderInstance)
{
    const std::unique_ptr<Rocket> rocket   = QtRocket::Test::makeEmptyBoosterSetsRocket();
    const RocketFigureGeometry    geometry = figureOf(*rocket);
    std::size_t                   extra    = 0;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        if (shape.extraRender)
        {
            ++extra;
            EXPECT_EQ(shape.shape.component().kind(), ComponentKind::PARALLEL_STAGE);
            EXPECT_EQ(shape.shape.component().getChildCount(), 0U);
        }
    }
    // Its three instances' markers.
    EXPECT_EQ(extra, 3U * 6U);
}

TEST(RocketFigureGeometry, HiddenComponentsKeepTheirShapesButTheirMotorsGo)
{
    // The painter skips a hidden component's shapes; the hit test still finds them (Java), so
    // the geometry keeps them. drawMotors skips the motors of a hidden mount.
    const TestEstesAlphaIII alpha;
    alpha.rocket->setSelectedConfiguration(QtRocket::Test::testFcid(0));
    const RocketFigureGeometry shown = figureOf(*alpha.rocket);
    ASSERT_FALSE(shown.motors().empty());
    alpha.body->setVisible(false);
    alpha.inner->setVisible(false);
    const RocketFigureGeometry hidden = figureOf(*alpha.rocket);
    EXPECT_EQ(hidden.shapes().size(), shown.shapes().size());
    EXPECT_TRUE(hidden.motors().empty());
}

TEST(RocketFigureGeometry, TheMotorsSitAtTheMountsLocations)
{
    // The C6 of the first test configuration (the default one has no motors).
    const TestEstesAlphaIII alpha;
    alpha.rocket->setSelectedConfiguration(QtRocket::Test::testFcid(0));
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    for (const FigureView view : QtRocket::kAllFigureViews)
    {
        const RocketFigureGeometry geometry =
            RocketFigureGeometry::build(config, view, 0.5, {}, false);
        ASSERT_EQ(geometry.motors().size(), 1U);
        const RocketFigureGeometry::MotorShape& motor = geometry.motors()[0];
        EXPECT_EQ(motor.mount, alpha.inner);
        EXPECT_EQ(motor.locationIndex, 0);
        EXPECT_EQ(motor.shape.type(), QtRocket::usesSideShapes(view)
                                          ? QtRocket::FigureShape::Type::RECT
                                          : QtRocket::FigureShape::Type::ELLIPSE);
    }
}

TEST(RocketFigureGeometry, ComponentsAtAnswersInDrawingOrder)
{
    // A point on the axis inside the body tube: the inner tube and its engine block lie there
    // too; the body tube, of the lowest display order, is drawn last and comes last.
    const TestEstesAlphaIII    alpha;
    const RocketFigureGeometry geometry = figureOf(*alpha.rocket);
    const double               x        = alpha.body->getComponentLocations().front().x + 0.15;
    const auto                 found    = geometry.componentsAt(Point2D{x, 0.001});
    ASSERT_FALSE(found.empty());
    EXPECT_EQ(found.back(), alpha.body);
    for (std::size_t i = 1; i < found.size(); ++i)
    {
        EXPECT_GE(QtRocket::displayOrder(FigureView::SIDE, *found[i - 1]),
                  QtRocket::displayOrder(FigureView::SIDE, *found[i]));
        for (std::size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(found[i], found[j]);
        }
    }
    EXPECT_TRUE(geometry.componentsAt(Point2D{-1, 0}).empty());
}

}  // namespace
