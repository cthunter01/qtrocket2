#include "QtRocket/figure/ComponentShape.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::Color;
using QtRocket::ComponentShape;
using QtRocket::FigureEllipse;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::LineStyle;

TEST(ComponentShape, AVisibleShapeCopiesTheComponentsStyle)
{
    BodyTube tube;
    // A component without a colour or line style of its own: the painter takes the defaults.
    const ComponentShape plain{FigureShape{FigureRect{0, 0, 1, 1}}, tube};
    EXPECT_TRUE(plain.isVisible());
    EXPECT_EQ(&plain.component(), &tube);
    EXPECT_FALSE(plain.color().has_value());
    EXPECT_FALSE(plain.lineStyle().has_value());
    EXPECT_EQ(plain.shape(), (FigureShape{FigureRect{0, 0, 1, 1}}));

    tube.setColor(Color{10, 20, 30});
    tube.setLineStyle(LineStyle::DOTTED);
    const ComponentShape styled{FigureShape{FigureEllipse{0, 0, 1, 1}}, tube};
    EXPECT_EQ(styled.color(), (Color{10, 20, 30}));
    EXPECT_EQ(styled.lineStyle(), LineStyle::DOTTED);
    // The style is copied when the shape is made, as RocketComponentShapes' constructor does.
    tube.setColor(Color{1, 2, 3});
    EXPECT_EQ(styled.color(), (Color{10, 20, 30}));
}

TEST(ComponentShape, AnInvisibleShapeHasJavasInvisibleColour)
{
    BodyTube tube;
    tube.setColor(Color{10, 20, 30});
    tube.setLineStyle(LineStyle::DASHED);
    const ComponentShape hit = ComponentShape::invisible(FigureShape{FigureRect{0, 0, 1, 1}}, tube);
    EXPECT_FALSE(hit.isVisible());
    EXPECT_EQ(hit.color(), Color::invisible());
    EXPECT_EQ(hit.lineStyle(), LineStyle::DASHED);
    EXPECT_EQ(&hit.component(), &tube);
}

TEST(ComponentShape, FromShapesKeepsTheOrder)
{
    const BodyTube                    tube;
    const std::vector<ComponentShape> shapes = ComponentShape::fromShapes(
        {FigureShape{FigureRect{0, 0, 1, 1}}, FigureShape{FigureEllipse{0, 0, 1, 1}}}, tube);
    ASSERT_EQ(shapes.size(), 2U);
    EXPECT_EQ(shapes[0].shape().type(), FigureShape::Type::RECT);
    EXPECT_EQ(shapes[1].shape().type(), FigureShape::Type::ELLIPSE);
    EXPECT_TRUE(shapes[0].isVisible());
    EXPECT_TRUE(shapes[1].isVisible());
    EXPECT_EQ(&shapes[1].component(), &tube);
    EXPECT_TRUE(ComponentShape::fromShapes({}, tube).empty());
}

}  // namespace
