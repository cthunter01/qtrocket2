#include "QtRocket/gui/figure/FinPointFigure.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <QColor>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QSize>
#include <QWidget>

#include <gtest/gtest.h>

#include "FinPointTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using FinPointFigureTest = Test::GuiTest;
using Test::FreeformDesign;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;

/// FreeformPanelProbe's figure of the default outline on its body tube, fitted into 600 x 400
/// pixels at 96 dots per inch: the fitted view 560 x 360, the user scale and the pixels per metre.
constexpr double kDotsPerInch = 96;
const QSize      kFittedView{560, 360};
constexpr double kProbeUserScale = 1.693332418933827;
constexpr double kProbeScale     = 6399.999999999999;

/// The figure of @p design's first fin set, fitted as the probe's.
struct FittedFigure
{
    FinPointFigure figure;

    explicit FittedFigure(const FreeformDesign& design)
      : figure(design.selection(), kDotsPerInch, nullptr)
    {
        figure.scaleTo(figure.fitScale(kFittedView), kFittedView);
        figure.updateFigure();
    }

    /// The pixel of the point (@p x, @p y) of the fin frame, as the probe computes it.
    [[nodiscard]] QPoint pixel(double x, double y) const
    {
        const QPoint origin = figure.subjectOrigin();
        const double scale  = figure.absoluteScale();
        return {origin.x() + static_cast<int>(MathUtil::javaRound(x * scale)),
                origin.y() - static_cast<int>(MathUtil::javaRound(y * scale))};
    }
};

TEST_F(FinPointFigureTest, BoundsTheFinAndItsBodyTubeAsOpenRocket)
{
    // FreeformPanelProbe's "body tube" figures: "figure subject=(0.0,0.0,0.075,0.05)", "figure
    // content=(-0.25,-0.021,0.325,0.07100000000000001)".
    const FreeformDesign design;
    const FittedFigure   fitted(design);
    const Rectangle2D    subject = fitted.figure.subjectBounds();
    EXPECT_EQ(subject, (Rectangle2D{.x = 0.0, .y = 0.0, .width = 0.075, .height = 0.05}));
    const Rectangle2D content = fitted.figure.contentBounds();
    EXPECT_EQ(
        content,
        (Rectangle2D{.x = -0.25, .y = -0.021, .width = 0.325, .height = 0.07100000000000001}));
    EXPECT_EQ(fitted.figure.mountBoundsMax().min().y, -0.021);
    EXPECT_DOUBLE_EQ(fitted.figure.mountBoundsMax().max().x, 0.05);
    EXPECT_EQ(fitted.figure.mountBoundsMin().span().y, 0.021);
}

TEST_F(FinPointFigureTest, FitsAndPlacesItsOriginAsOpenRocket)
{
    // "figure fitting=true userScale=1.693332418933827 scale=6399.999999999999",
    // "figure originX=1629", "figure autoZoomX=1599".
    const FreeformDesign design;
    const FittedFigure   fitted(design);
    EXPECT_EQ(fitted.figure.userScale(), kProbeUserScale);
    EXPECT_EQ(fitted.figure.absoluteScale(), kProbeScale);
    EXPECT_EQ(fitted.figure.subjectOrigin().x(), 1629);
    EXPECT_EQ(fitted.figure.autoZoomPoint(), QPoint(1599, 0));
}

TEST_F(FinPointFigureTest, FindsThePointWithinFifteenPixels)
{
    // getIndexByPoint(): the nearest point within 12 * 1.25 pixels.
    const FreeformDesign design;
    const FittedFigure   fitted(design);
    const QPoint         second = fitted.pixel(0.025, 0.05);
    EXPECT_EQ(fitted.figure.indexAt(second), 1);
    EXPECT_EQ(fitted.figure.indexAt(second + QPoint(14, 0)), 1);
    EXPECT_EQ(fitted.figure.indexAt(second + QPoint(16, 0)), -1);
    EXPECT_EQ(fitted.figure.indexAt(second + QPoint(10, 10)), 1);
    EXPECT_EQ(fitted.figure.indexAt(second + QPoint(11, 11)), -1);
    EXPECT_EQ(fitted.figure.indexAt(fitted.pixel(0.05, 0) + QPoint(-14, 0)), 3);
}

TEST_F(FinPointFigureTest, FindsTheSegmentWithinTwelvePixelsOfItsLine)
{
    // getSegmentByPoint(): the end point's index of the first segment whose line passes within
    // 12 pixels while both its ends are nearer than its length; the root is no segment.
    const FreeformDesign design;
    const FittedFigure   fitted(design);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.0125, 0.025)), 1);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.05, 0.05)), 2);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.0625, 0.025)), 3);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.03, 0)), -1);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.0125, 0.03)), -1);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.0, 0.05)), -1);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.2, 0.2)), -1);
}

TEST_F(FinPointFigureTest, MapsPixelsToTheFinFrameLimitedToTwoAndAHalfMetres)
{
    const FreeformDesign design;
    const FittedFigure   fitted(design);
    const QPoint         origin = fitted.figure.subjectOrigin();
    const Point2D        p      = fitted.figure.toFinPoint(origin + QPoint(64, -32));
    EXPECT_EQ(p.x, 64 / kProbeScale);
    EXPECT_EQ(p.y, -32 / -kProbeScale);
    const Point2D beyond = fitted.figure.toFinFrame(QPointF(origin + QPoint(20000, 20000)));
    EXPECT_GT(beyond.x, FreeformFinSet::kSnapLargerThan);
    const Point2D limited = fitted.figure.toFinPoint(origin + QPoint(20000, 20000));
    EXPECT_EQ(limited.x, FreeformFinSet::kSnapLargerThan);
    EXPECT_EQ(limited.y, -FreeformFinSet::kSnapLargerThan);
}

TEST_F(FinPointFigureTest, AFinSetThatLeftTheRocketIsHitNoMore)
{
    const FreeformDesign design;
    FittedFigure         fitted(design);
    const QPoint         second  = fitted.pixel(0.025, 0.05);
    RocketComponent&     body    = *design.fins().getParent();
    const auto           removed = body.removeChild(&design.fins());
    EXPECT_EQ(fitted.figure.indexAt(second), -1);
    EXPECT_EQ(fitted.figure.segmentAt(fitted.pixel(0.0125, 0.025)), -1);
    // Painting draws the background only.
    RecordingPaintDevice device(QSize(200, 200));
    fitted.figure.render(&device, QPoint(), QRegion(QRect(0, 0, 200, 200)), {});
    EXPECT_EQ(device.records().size(), 1U);
}

TEST_F(FinPointFigureTest, KeepsTheEditorsHistory)
{
    const FreeformDesign design;
    FittedFigure         fitted(design);
    FinPointEditHistory& history = fitted.figure.editHistory();
    EXPECT_EQ(history.size(), 1U);
    history.beginEdit();
    EXPECT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    history.commitEdit();
    EXPECT_TRUE(history.canUndo());
}

/// The records of @p kind whose path's device bounds are @p side pixels wide.
[[nodiscard]] std::vector<RecordedPaint> boxes(const std::vector<RecordedPaint>& records,
                                               double                            side)
{
    std::vector<RecordedPaint> found;
    for (const RecordedPaint& record : records)
    {
        if (record.kind == RecordedPaint::Kind::RECT &&
            std::abs(record.devicePath().boundingRect().width() - side) < 1e-6)
        {
            found.push_back(record);
        }
    }
    return found;
}

/// The records of the kind @p kind drawn with the pen of @p color and @p width.
[[nodiscard]] std::size_t countDrawn(const std::vector<RecordedPaint>& records,
                                     RecordedPaint::Kind kind, const QColor& color, double width)
{
    std::size_t count = 0;
    for (const RecordedPaint& record : records)
    {
        if (record.kind == kind && record.pen.color() == color && record.pen.widthF() == width &&
            record.pen.isCosmetic())
        {
            ++count;
        }
    }
    return count;
}

TEST_F(FinPointFigureTest, PaintsTheBodyTheFinTheHighlightAndTheHandles)
{
    const FreeformDesign design;
    FittedFigure         fitted(design);
    fitted.figure.resize(fitted.figure.canvasSize());
    fitted.figure.setSelectedIndex(2);
    fitted.figure.setHighlightIndex(1);
    const QRect          area(QPoint(1500, 0), QSize(600, 400));
    RecordingPaintDevice device(area.size());
    fitted.figure.render(&device, QPoint(), QRegion(area), {});
    const std::vector<RecordedPaint>& records = device.records();

    // Four 12 pixel boxes and the selected point's 16 pixel box in its colour.
    EXPECT_EQ(boxes(records, FinPointFigure::kBoxWidth).size(), 4U);
    const std::vector<RecordedPaint> selected = boxes(records, FinPointFigure::kSelectedBoxWidth);
    ASSERT_EQ(selected.size(), 1U);
    EXPECT_EQ(selected.front().pen.color(),
              ThemeColors::color(ThemeColors::Key::FIN_SELECTED_POINT));
    // The highlight: the segment from point 1 to point 2, 3 pixels.
    EXPECT_EQ(countDrawn(records, RecordedPaint::Kind::LINE,
                         ThemeColors::color(ThemeColors::Key::FIN_SNAP_HIGHLIGHT), 3),
              1U);
    // The body (2 pixels) and the fin with its root (1 pixel), each a path.
    const QColor line = ThemeColors::color(ThemeColors::Key::FIN_BODY_LINE);
    EXPECT_EQ(countDrawn(records, RecordedPaint::Kind::PATH, line, 2), 1U);
    EXPECT_EQ(countDrawn(records, RecordedPaint::Kind::PATH, line, 1), 1U);
    // The grid's lines, minor and major.
    EXPECT_GT(countDrawn(records, RecordedPaint::Kind::LINE,
                         ThemeColors::color(ThemeColors::Key::FIN_GRID_MINOR), 1),
              0U);
    EXPECT_GT(countDrawn(records, RecordedPaint::Kind::LINE,
                         ThemeColors::color(ThemeColors::Key::FIN_GRID_MAJOR), 1),
              0U);
}

/// Rocket > Stage > a conical transition 10 cm long of the radii 2 and 3 cm > a freeform fin set
/// at its bottom (FreeformPanelProbe's "transition").
[[nodiscard]] std::unique_ptr<OpenRocketDocument> finsOnTransition()
{
    auto        rocket     = std::make_unique<Rocket>();
    AxialStage& stage      = rocket->addChild(std::make_unique<AxialStage>());
    auto        transition = std::make_unique<Transition>();
    transition->setShapeType(TransitionShape::CONICAL);
    transition->setForeRadiusAutomatic(false);
    transition->setAftRadiusAutomatic(false);
    transition->setForeRadius(0.02);
    transition->setAftRadius(0.03);
    transition->setLength(0.1);
    auto fins = std::make_unique<FreeformFinSet>();
    fins->setAxialMethod(AxialMethod::BOTTOM);
    fins->setAxialOffset(0);
    stage.addChild(std::move(transition)).addChild(std::move(fins));
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

TEST_F(FinPointFigureTest, PaintsATransitionsProfileInCentimetreSteps)
{
    // paintBodyTransition(): the front cap, a point every 0.01f m while below the float length
    // (ten of them for 10 cm: Java's float steps add up to just below 0.1f), the end cap.
    const std::unique_ptr<OpenRocketDocument> document = finsOnTransition();
    RocketComponent& transition = document->getRocket().getChild(0).getChild(0);
    const auto&      fins       = dynamic_cast<const FreeformFinSet&>(transition.getChild(0));
    FinPointFigure figure(FinPointSelection(document->getRocket(), fins.getId(), {}), kDotsPerInch,
                          nullptr);
    figure.scaleTo(figure.fitScale(kFittedView), kFittedView);
    figure.updateFigure();
    figure.resize(figure.canvasSize());
    RecordingPaintDevice device(figure.canvasSize());
    figure.render(&device, QPoint(), QRegion(QRect(QPoint(), figure.canvasSize())), {});
    const QColor               line = ThemeColors::color(ThemeColors::Key::FIN_BODY_LINE);
    std::vector<RecordedPaint> bodies;
    std::ranges::copy_if(device.records(), std::back_inserter(bodies),
                         [&line](const RecordedPaint& record) {
                             return record.kind == RecordedPaint::Kind::PATH &&
                                    record.pen.color() == line && record.pen.widthF() == 2;
                         });
    ASSERT_EQ(bodies.size(), 1U);
    EXPECT_EQ(bodies.front().path.elementCount(), 14);
    // From the body's front at the axis to its end at the axis, in the fin frame.
    const double xFinStart = fins.getAxialOffset(AxialMethod::TOP);
    EXPECT_EQ(bodies.front().path.elementAt(0).x, -xFinStart);
    EXPECT_EQ(bodies.front().path.elementAt(13).x,
              -xFinStart + static_cast<double>(static_cast<float>(0.1)));
}

}  // namespace

}  // namespace QtRocket::Gui
