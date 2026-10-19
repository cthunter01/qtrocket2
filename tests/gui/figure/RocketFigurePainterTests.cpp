#include "QtRocket/gui/figure/RocketFigurePainter.h"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <QColor>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTransform>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Uuid.h"
#include "RecordingPaintDevice.h"
#include "ScopedApplicationPalette.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using RocketFigurePainterTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Test::ScopedApplicationPalette;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// The pen's dashes in pixels: its pattern times its width.
[[nodiscard]] std::vector<double> dashesInPixels(const QPen& pen)
{
    std::vector<double> dashes;
    for (const qreal dash : pen.dashPattern())
    {
        dashes.push_back(dash * pen.widthF());
    }
    return dashes;
}

/// Expects the component pens of @p style, normal and selected, to draw @p pixels (OpenRocket's
/// LineStyle dashes in pixels; none for a solid line).
void expectDashes(LineStyle style, const std::vector<double>& pixels)
{
    for (const bool selected : {false, true})
    {
        const QPen pen = RocketFigurePainter::componentPen(Qt::black, style, selected);
        EXPECT_EQ(pen.style(), pixels.empty() ? Qt::SolidLine : Qt::CustomDashLine);
        EXPECT_EQ(dashesInPixels(pen), pixels)
            << lineStyleName(style) << (selected ? " selected" : "");
    }
}

TEST_F(RocketFigurePainterTest, ComponentPensAreCosmeticOneOrTwoPixelsFlatAndBevelled)
{
    const QColor color(10, 20, 30);
    const QPen   normal = RocketFigurePainter::componentPen(color, LineStyle::SOLID, false);
    EXPECT_EQ(normal.color(), color);
    EXPECT_EQ(normal.widthF(), 1.0);
    EXPECT_TRUE(normal.isCosmetic());
    EXPECT_EQ(normal.capStyle(), Qt::FlatCap);
    EXPECT_EQ(normal.joinStyle(), Qt::BevelJoin);
    EXPECT_EQ(normal.style(), Qt::SolidLine);

    const QPen selected = RocketFigurePainter::componentPen(color, LineStyle::SOLID, true);
    EXPECT_EQ(selected.widthF(), 2.0);
    EXPECT_TRUE(selected.isCosmetic());
    EXPECT_EQ(selected.capStyle(), Qt::FlatCap);
    EXPECT_EQ(selected.joinStyle(), Qt::BevelJoin);
}

TEST_F(RocketFigurePainterTest, DashesAreInPixelsWhateverTheWidth)
{
    // RocketFigure scales LineStyle.getDashes() by 1/scale, as the stroke's width: Java's dashes
    // are pixels whether the line is 1 or 2 pixels wide.
    expectDashes(LineStyle::SOLID, {});
    expectDashes(LineStyle::DASHED, {6, 4});
    expectDashes(LineStyle::DOTTED, {2, 3});
    expectDashes(LineStyle::DASHDOT, {8, 3, 2, 3});
    EXPECT_TRUE(RocketFigurePainter::dashPattern(LineStyle::SOLID, 1).isEmpty());
    EXPECT_EQ(RocketFigurePainter::dashPattern(LineStyle::DASHED, 2), QList<qreal>({3, 2}));
}

TEST_F(RocketFigurePainterTest, MotorPensAreOnePixelSolid)
{
    const QPen pen = RocketFigurePainter::motorPen(QColor(0, 0, 0, 200));
    EXPECT_EQ(pen.color(), QColor(0, 0, 0, 200));
    EXPECT_EQ(pen.widthF(), 1.0);
    EXPECT_TRUE(pen.isCosmetic());
    EXPECT_EQ(pen.style(), Qt::SolidLine);
    EXPECT_EQ(pen.capStyle(), Qt::FlatCap);
    EXPECT_EQ(pen.joinStyle(), Qt::BevelJoin);
}

/// A kind's default colour and line style in the light theme (OR.defaults.component.*,
/// ApplicationPreferences' DEFAULT_LINE_STYLES).
struct DefaultCase
{
    ComponentKind kind{ComponentKind::ROCKET};
    QColor        color;
    LineStyle     style{LineStyle::SOLID};
};

TEST_F(RocketFigurePainterTest, ShapesWithoutAColourOrStyleTakeTheirClassDefaults)
{
    const InMemoryPreferences preferences;
    const RocketFigurePainter painter(preferences, ScopedApplicationPalette::light());
    const QColor              body(0, 0, 240);
    const QColor              fins(0, 0, 200);
    const QColor              lugs(0, 0, 180);
    const QColor              internal(170, 0, 100);
    const QColor              mass(0, 0, 0);
    const QColor              recovery(255, 0, 0);
    const std::array          cases{
        DefaultCase{.kind  = ComponentKind::PARALLEL_STAGE,
                    .color = QColor(198, 163, 184),
                    .style = LineStyle::SOLID},
        DefaultCase{.kind  = ComponentKind::POD_SET,
                    .color = QColor(160, 160, 215),
                    .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::BODY_TUBE, .color = body, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::TRANSITION, .color = body, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::NOSE_CONE, .color = body, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::TRAPEZOID_FIN_SET, .color = fins, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::ELLIPTICAL_FIN_SET, .color = fins, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::FREEFORM_FIN_SET, .color = fins, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::TUBE_FIN_SET, .color = fins, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::LAUNCH_LUG, .color = lugs, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::RAIL_BUTTON, .color = lugs, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::INNER_TUBE, .color = internal, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::TUBE_COUPLER, .color = internal, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::ENGINE_BLOCK, .color = internal, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::CENTERING_RING, .color = internal, .style = LineStyle::SOLID},
        DefaultCase{.kind = ComponentKind::BULKHEAD, .color = internal, .style = LineStyle::SOLID},
        DefaultCase{
            .kind = ComponentKind::MASS_COMPONENT, .color = mass, .style = LineStyle::DASHED},
        DefaultCase{.kind = ComponentKind::SHOCK_CORD, .color = mass, .style = LineStyle::DASHED},
        DefaultCase{
            .kind = ComponentKind::PARACHUTE, .color = recovery, .style = LineStyle::DASHED},
        DefaultCase{.kind = ComponentKind::STREAMER, .color = recovery, .style = LineStyle::DASHED},
    };
    for (const DefaultCase& defaultCase : cases)
    {
        const std::unique_ptr<RocketComponent> component =
            ComponentFactory::createWithBuiltinMaterials(defaultCase.kind);
        const ComponentShape shape(FigureShape{FigureRect{0, 0, 1, 1}}, *component);
        EXPECT_EQ(painter.shapeColor(shape), defaultCase.color) << component->getName();
        EXPECT_EQ(painter.shapeLineStyle(shape), defaultCase.style) << component->getName();
    }
}

TEST_F(RocketFigurePainterTest, ShapesKeepTheColourAndStyleTheirComponentHad)
{
    InMemoryPreferences                    preferences;
    const RocketFigurePainter              painter(preferences, ScopedApplicationPalette::light());
    const std::unique_ptr<RocketComponent> tube =
        ComponentFactory::createWithBuiltinMaterials(ComponentKind::BODY_TUBE);
    tube->setColor(Color(10, 20, 30, 40));
    tube->setLineStyle(LineStyle::DOTTED);
    const ComponentShape shape(FigureShape{FigureRect{0, 0, 1, 1}}, *tube);
    // The shape copied them when it was made (RocketComponentShapes' constructor).
    tube->setColor(Color(1, 2, 3));
    tube->setLineStyle(LineStyle::DASHDOT);
    EXPECT_EQ(painter.shapeColor(shape), QColor(10, 20, 30, 40));
    EXPECT_EQ(painter.shapeLineStyle(shape), LineStyle::DOTTED);

    // The preferences' default of the class (QtRocket's deviation: Java's theme value wins).
    const std::unique_ptr<RocketComponent> plain =
        ComponentFactory::createWithBuiltinMaterials(ComponentKind::BODY_TUBE);
    preferences.setDefaultLineStyle("BodyTube", LineStyle::DOTTED);
    preferences.setDefaultColor("BodyComponent", Color(1, 2, 3));
    const ComponentShape plainShape(FigureShape{FigureRect{0, 0, 1, 1}}, *plain);
    EXPECT_EQ(painter.shapeLineStyle(plainShape), LineStyle::DOTTED);
    EXPECT_EQ(painter.shapeColor(plainShape), QColor(1, 2, 3));
}

TEST_F(RocketFigurePainterTest, MotorColoursFollowThePalettesTheme)
{
    const InMemoryPreferences preferences;
    const RocketFigurePainter light(preferences, ScopedApplicationPalette::light());
    EXPECT_EQ(light.motorFillColor(), QColor(0, 0, 0, 0x64));
    EXPECT_EQ(light.motorBorderColor(), QColor(0, 0, 0, 0xc8));
    const RocketFigurePainter dark(preferences, ScopedApplicationPalette::dark());
    EXPECT_EQ(dark.motorFillColor(), QColor(0, 0, 0, 0x32));
    EXPECT_EQ(dark.motorBorderColor(), QColor(0, 0, 0, 0x64));
}

/// The projection of a figure at 100 pixels per metre with the origin at (10, 20).
[[nodiscard]] QTransform testProjection()
{
    QTransform projection;
    projection.translate(10, 20);
    projection.scale(100, -100);
    return projection;
}

TEST_F(RocketFigurePainterTest, DevicePathsMapEverySegmentWithTheWindingRule)
{
    const std::optional<QPainterPath> rect =
        RocketFigurePainter::devicePath(FigureShape{FigureRect{1, 2, 3, 4}}, testProjection());
    ASSERT_TRUE(rect.has_value());
    const QPainterPath rectPath = rect.value_or(QPainterPath());
    EXPECT_EQ(rectPath.fillRule(), Qt::WindingFill);
    // Java's RectIterator: the four corners, back to the first (QPainterPath keeps no line to
    // where it is already), and the close.
    ASSERT_EQ(rectPath.elementCount(), 5);
    EXPECT_EQ(QPointF(rectPath.elementAt(0)), QPointF(110, -180));
    EXPECT_EQ(QPointF(rectPath.elementAt(1)), QPointF(410, -180));
    EXPECT_EQ(QPointF(rectPath.elementAt(2)), QPointF(410, -580));
    EXPECT_EQ(QPointF(rectPath.elementAt(3)), QPointF(110, -580));
    EXPECT_EQ(rectPath.controlPointRect(), QRectF(110, -580, 300, 400));

    FigurePath evenOdd(FigurePath::Precision::DOUBLE, WindingRule::EVEN_ODD);
    evenOdd.moveTo(0, 0);
    evenOdd.lineTo(1, 0);
    evenOdd.quadTo(1, 1, 0, 1);
    evenOdd.curveTo(0, 0.5, 0.5, 0.5, 0, 0);
    evenOdd.closePath();
    const std::optional<QPainterPath> path =
        RocketFigurePainter::devicePath(FigureShape{evenOdd}, testProjection());
    ASSERT_TRUE(path.has_value());
    const QPainterPath curves = path.value_or(QPainterPath());
    EXPECT_EQ(curves.fillRule(), Qt::OddEvenFill);
    EXPECT_EQ(curves.elementAt(2).type, QPainterPath::CurveToElement);
    EXPECT_EQ(curves.controlPointRect(), QRectF(10, -80, 100, 100));
}

TEST_F(RocketFigurePainterTest, DevicePathsRefuseWhatQtMustNotSee)
{
    const QTransform projection = testProjection();
    EXPECT_FALSE(
        RocketFigurePainter::devicePath(FigureShape{FigureRect{kNaN, 0, 1, 1}}, projection));
    EXPECT_FALSE(
        RocketFigurePainter::devicePath(FigureShape{FigureRect{0, 0, kInfinity, 1}}, projection));
    EXPECT_FALSE(
        RocketFigurePainter::devicePath(FigureShape{FigureEllipse{0, 0, 1, kNaN}}, projection));
    // Finite metres beyond kMaxDeviceCoordinate pixels, and a projection that is not finite.
    EXPECT_FALSE(
        RocketFigurePainter::devicePath(FigureShape{FigureRect{1e300, 0, 1, 1}}, projection));
    EXPECT_FALSE(
        RocketFigurePainter::devicePath(FigureShape{FigureRect{1e7, 0, 1, 1}}, projection));
    EXPECT_TRUE(RocketFigurePainter::devicePath(FigureShape{FigureRect{1e6, 0, 1, 1}}, projection));
    QTransform notFinite;
    notFinite.scale(kInfinity, 1);
    EXPECT_FALSE(RocketFigurePainter::devicePath(FigureShape{FigureRect{1, 0, 1, 1}}, notFinite));

    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(RocketFigurePainter::kMaxDeviceCoordinate,
                -RocketFigurePainter::kMaxDeviceCoordinate);
    EXPECT_TRUE(RocketFigurePainter::isDrawable(path));
    path.lineTo(0, 2 * RocketFigurePainter::kMaxDeviceCoordinate);
    EXPECT_FALSE(RocketFigurePainter::isDrawable(path));
}

/// Paints @p geometry into a recording device with @p selection, with the painter's state
/// checked to be restored.
[[nodiscard]] std::vector<RecordedPaint> paintRecords(const RocketFigurePainter&  painter,
                                                      const RocketFigureGeometry& geometry,
                                                      std::span<const Uuid>       selection = {})
{
    RecordingPaintDevice device(QSize(1000, 700));
    QPainter             qpainter(&device);
    qpainter.translate(5, 7);
    painter.paint(qpainter, geometry, testProjection(), selection);
    EXPECT_EQ(qpainter.worldTransform(), QTransform::fromTranslate(5, 7));
    EXPECT_FALSE(qpainter.testRenderHint(QPainter::Antialiasing));
    qpainter.end();
    return device.records();
}

/// The components of the shapes the painter draws, in order: the visible shapes of visible
/// components, of a colour with some alpha, with some extent.
[[nodiscard]] std::vector<const RocketComponent*> drawnComponents(
    const RocketFigurePainter& painter, const RocketFigureGeometry& geometry)
{
    std::vector<const RocketComponent*> components;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        const std::optional<QPainterPath> path =
            RocketFigurePainter::devicePath(shape.shape.shape(), testProjection());
        if (shape.shape.isVisible() && shape.shape.component().isVisible() &&
            painter.shapeColor(shape.shape).alpha() != 0 &&
            !path.value_or(QPainterPath()).isEmpty())
        {
            components.push_back(&shape.shape.component());
        }
    }
    return components;
}

/// Success when @p records are @p shapes outlines, then a fill and an outline per motor, all
/// paths in device pixels (the painter's translation is in the path, not the transform),
/// antialiased.
[[nodiscard]] ::testing::AssertionResult outlinesThenMotors(
    const std::vector<RecordedPaint>& records, std::size_t shapes)
{
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const RecordedPaint& record  = records[index];
        const bool           outline = index < shapes || (index - shapes) % 2 == 1;
        const bool drawn = record.kind == RecordedPaint::Kind::PATH && record.antialiased &&
                           record.transform.isIdentity();
        const bool asOutline =
            record.brush.style() == Qt::NoBrush && record.pen.style() != Qt::NoPen;
        const bool asMotorFill =
            record.pen.style() == Qt::NoPen && record.brush.color() == QColor(0, 0, 0, 0x64);
        if (!drawn || (outline ? !asOutline : !asMotorFill))
        {
            return ::testing::AssertionFailure() << "record " << index;
        }
    }
    return ::testing::AssertionSuccess();
}

TEST_F(RocketFigurePainterTest, OutlinesTheShapesThenFillsAndOutlinesTheMotors)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    const RocketFigurePainter      painter(preferences, ScopedApplicationPalette::light());
    const RocketFigureGeometry     geometry = RocketFigureGeometry::build(
        beta.rocket->getSelectedConfiguration(), FigureView::SIDE, 0.0, {}, false);
    ASSERT_EQ(geometry.motors().size(), 2U);

    const std::vector<RecordedPaint> records = paintRecords(painter, geometry);
    const std::size_t                shapes  = drawnComponents(painter, geometry).size();
    ASSERT_EQ(records.size(), shapes + (2 * geometry.motors().size()));
    EXPECT_TRUE(outlinesThenMotors(records, shapes));
    // The first motor, filled and outlined, where the projection and the translation put it.
    const Rectangle2D bounds = geometry.motors().front().shape.bounds();
    const QRectF      motor(bounds.x, bounds.y, bounds.width, bounds.height);
    const QRectF expected = (testProjection() * QTransform::fromTranslate(5, 7)).mapRect(motor);
    EXPECT_EQ(records[shapes].path.controlPointRect(), expected);
    EXPECT_EQ(records[shapes + 1].path.controlPointRect(), expected);
    EXPECT_EQ(records[shapes + 1].pen.color(), QColor(0, 0, 0, 0xc8));
}

TEST_F(RocketFigurePainterTest, SkipsHiddenComponentsInvisibleShapesAndTransparentColours)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    const RocketFigurePainter      painter(preferences, ScopedApplicationPalette::light());
    const auto                     shapesOf = [&beta] {
        return RocketFigureGeometry::build(beta.rocket->getSelectedConfiguration(),
                                           FigureView::SIDE, 0.0, {}, false);
    };
    const std::size_t all = paintRecords(painter, shapesOf()).size();

    // The body tube hidden: its one shape is not drawn (the inner tube in it, and its motor, are
    // other components).
    beta.body->setVisible(false);
    const std::size_t withoutBody = paintRecords(painter, shapesOf()).size();
    EXPECT_EQ(withoutBody, all - 1);

    // The nose cone in a colour of alpha 0: neither of its two shapes (nose and shoulder).
    beta.nose->setColor(Color(200, 100, 50, 0));
    const RocketFigureGeometry geometry   = shapesOf();
    std::size_t                noseShapes = 0;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        if (&shape.shape.component() == beta.nose)
        {
            ++noseShapes;
        }
    }
    EXPECT_GT(noseShapes, 0U);
    EXPECT_EQ(paintRecords(painter, geometry).size(), withoutBody - noseShapes);
}

TEST_F(RocketFigurePainterTest, SelectedComponentsAreDrawnTwoPixelsWide)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    const RocketFigurePainter      painter(preferences, ScopedApplicationPalette::light());
    const RocketFigureGeometry     geometry = RocketFigureGeometry::build(
        beta.rocket->getSelectedConfiguration(), FigureView::BACK, 0.0, {}, false);
    const std::array<Uuid, 1>        selection{beta.fins->getId()};
    const std::vector<RecordedPaint> records = paintRecords(painter, geometry, selection);

    const std::vector<const RocketComponent*> drawn = drawnComponents(painter, geometry);
    ASSERT_EQ(records.size(), drawn.size() + (2 * geometry.motors().size()));
    std::size_t wide = 0;
    for (std::size_t index = 0; index < drawn.size(); ++index)
    {
        const bool selected = drawn[index] == beta.fins;
        EXPECT_EQ(records[index].pen.widthF(), selected ? 2.0 : 1.0) << index;
        if (selected)
        {
            ++wide;
        }
    }
    EXPECT_GT(wide, 0U);
}

}  // namespace

}  // namespace QtRocket::Gui
