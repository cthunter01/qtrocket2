#include "QtRocket/gui/figure/Caret.h"

#include <cmath>
#include <limits>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QTransform>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/figure/CGCaret.h"
#include "QtRocket/gui/figure/CPCaret.h"
#include "QtRocket/gui/figure/FigureElement.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using CaretTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// A figure's projection: the origin at (100, 200) pixels, @p scale pixels per metre, y up.
[[nodiscard]] QTransform figureProjection(double scale)
{
    QTransform projection;
    projection.translate(100, 200);
    projection.scale(scale, -scale);
    return projection;
}

/// What @p caret paints at @p scale with the world transform @p transform, checking that the
/// painter's state is restored.
[[nodiscard]] std::vector<RecordedPaint> paintCaret(const Caret& caret, double scale,
                                                    const QTransform& transform)
{
    RecordingPaintDevice device(QSize(400, 400));
    QPainter             painter(&device);
    painter.setWorldTransform(transform);
    painter.setPen(QColor(1, 2, 3));
    caret.paint(painter, scale);
    EXPECT_EQ(painter.worldTransform(), transform);
    EXPECT_EQ(painter.pen().color(), QColor(1, 2, 3));
    EXPECT_EQ(painter.brush().style(), Qt::NoBrush);
    painter.end();
    return device.records();
}

TEST_F(CaretTest, KeepsItsPosition)
{
    CGCaret caret(1.5, -2.5);
    EXPECT_EQ(caret.x(), 1.5);
    EXPECT_EQ(caret.y(), -2.5);
    caret.setPosition(kNaN, 3);
    EXPECT_TRUE(std::isnan(caret.x()));
    EXPECT_EQ(caret.y(), 3);
}

/// Success when the CP caret at (0.5, 0.1) metres in a figure of @p scale pixels per metre is
/// filled in its colour, centred where the projection puts that point, 14 pixels across.
[[nodiscard]] ::testing::AssertionResult caretAtScale(double scale)
{
    const CPCaret                    caret(0.5, 0.1);
    const std::vector<RecordedPaint> records = paintCaret(caret, scale, figureProjection(scale));
    if (records.size() != 1)
    {
        return ::testing::AssertionFailure() << records.size() << " records";
    }
    const RecordedPaint& record = records.front();
    if (record.kind != RecordedPaint::Kind::PATH || record.pen.style() != Qt::NoPen ||
        record.brush.color() != ThemeColors::color(ThemeColors::Key::CP) || !record.antialiased)
    {
        return ::testing::AssertionFailure() << "not an antialiased fill in the CP colour";
    }
    const QRectF     bounds = record.devicePath().controlPointRect();
    const QPointF    expected(100 + (0.5 * scale), 200 - (0.1 * scale));
    constexpr double kTolerance = 1e-9;
    if (std::abs(bounds.center().x() - expected.x()) > kTolerance ||
        std::abs(bounds.center().y() - expected.y()) > kTolerance ||
        std::abs(bounds.width() - 14) > kTolerance || std::abs(bounds.height() - 14) > kTolerance)
    {
        return ::testing::AssertionFailure()
               << "bounds (" << bounds.x() << ", " << bounds.y() << ", " << bounds.width() << ", "
               << bounds.height() << ")";
    }
    return ::testing::AssertionSuccess();
}

TEST_F(CaretTest, IsSevenPixelsInRadiusAtAnyScaleOfTheFigure)
{
    // Caret.paint(): AffineTransform(1/scale, 0, 0, 1/scale, x, y) before the figure's projection.
    EXPECT_TRUE(caretAtScale(37.0));
    EXPECT_TRUE(caretAtScale(1000.0));
    EXPECT_TRUE(caretAtScale(2209.685002350729));
}

TEST_F(CaretTest, IsUpsideDownInTheFigureAndUprightInItsOwnPixels)
{
    const CGCaret caret(0, 0);
    // In the figure, y up: the quarter x > 0, y < 0 of the caret is below right on the screen.
    const std::vector<RecordedPaint> figure = paintCaret(caret, 100, figureProjection(100));
    ASSERT_EQ(figure.size(), 1U);
    const QPainterPath flipped = figure.front().devicePath();
    EXPECT_TRUE(flipped.contains(QPointF(103, 203)));
    EXPECT_TRUE(flipped.contains(QPointF(97, 197)));
    EXPECT_FALSE(flipped.contains(QPointF(103, 197)));
    EXPECT_FALSE(flipped.contains(QPointF(97, 203)));

    // RocketInfo's carets, in the widget's pixels at the scale 1.7: upright and 7/1.7 in radius.
    const CGCaret                    overlay(50, 60);
    const std::vector<RecordedPaint> upright = paintCaret(overlay, 1.7, QTransform());
    ASSERT_EQ(upright.size(), 1U);
    const QPainterPath area = upright.front().devicePath();
    EXPECT_NEAR(area.controlPointRect().width(), 14 / 1.7, 1e-9);
    EXPECT_TRUE(area.contains(QPointF(51.5, 58.5)));
    EXPECT_FALSE(area.contains(QPointF(51.5, 61.5)));
    EXPECT_EQ(upright.front().brush.color(), ThemeColors::color(ThemeColors::Key::CG));
}

TEST_F(CaretTest, PaintsNothingWhereQtWouldBeGivenNoNumber)
{
    // RocketPanel puts the carets at NaN in the back view and for a design without length.
    CGCaret caret(kNaN, kNaN);
    EXPECT_TRUE(paintCaret(caret, 100, figureProjection(100)).empty());
    caret.setPosition(0.5, kInfinity);
    EXPECT_TRUE(paintCaret(caret, 100, figureProjection(100)).empty());
    caret.setPosition(0.5, 0.1);
    for (const double scale : {0.0, -1.0, kNaN, kInfinity})
    {
        EXPECT_TRUE(paintCaret(caret, scale, figureProjection(100)).empty()) << scale;
    }
    // Finite, but far beyond any canvas.
    caret.setPosition(1e300, 0);
    EXPECT_TRUE(paintCaret(caret, 100, figureProjection(100)).empty());
    caret.setPosition(0.5, 0.1);
    EXPECT_EQ(paintCaret(caret, 100, figureProjection(100)).size(), 1U);
}

TEST_F(CaretTest, NeedsNoVisibleRectangle)
{
    CPCaret              caret(0.5, 0.1);
    RecordingPaintDevice device(QSize(400, 400));
    QPainter             painter(&device);
    painter.setWorldTransform(figureProjection(100));
    FigureElement& element = caret;
    element.paint(painter, 100, QRect());
    painter.end();
    ASSERT_EQ(device.records().size(), 1U);
    EXPECT_NEAR(device.records().front().devicePath().controlPointRect().center().x(), 150, 1e-9);
}

}  // namespace

}  // namespace QtRocket::Gui
