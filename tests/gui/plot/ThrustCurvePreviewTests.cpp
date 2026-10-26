#include "QtRocket/gui/plot/ThrustCurvePreview.h"

#include <memory>
#include <vector>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <QWindow>
#include <Qt>
#include <QtTypes>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/PlotWidget.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "ScopedApplicationPalette.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "plot/PlotReadback.h"

// OpenRocket has no JUnit test of its thrust-curve preview. What is expected is
// MotorInformationPanel's layered pane (MotorInformationPanel.java:233-271, 446-459) as the M3
// scouting's probes found it under JDK 17 and xvfb: the zoom icon's label at (w - 50, 12, 50,
// 50), a hand cursor over all of it, and a click of the left button on the icon, the plot area,
// the tick labels or the title opening the plot dialog (java.awt.Robot clicks). The clicks go to
// the preview's window (QWindow), so that Qt finds the widget under the pointer as for a user's.

namespace QtRocket::Gui
{

namespace
{

using ThrustCurvePreviewTest = Gui::Test::GuiTest;
using Palette                = Gui::Test::ScopedApplicationPalette;

constexpr int kWiderWidth   = 420;
constexpr int kTallerHeight = 240;
constexpr int kDragDistance = 40;

/// Where a user can click on the preview, in its coordinates: the middle of the plot area, the
/// y axis' tick labels (left of the plot area), the title (above it) and the zoom icon.
struct PreviewPlaces
{
    QPoint plotArea;
    QPoint tickLabels;
    QPoint title;
    QPoint zoomIcon;
};

/// The places of @p preview, after painting it (the plot area is where the plot was last laid
/// out).
[[nodiscard]] PreviewPlaces placesOf(ThrustCurvePreview& preview)
{
    static_cast<void>(preview.grab());
    rocketplot::PlotWidget& plot = preview.chart().plotWidget();
    const QRectF            area = plot.plotArea();
    const auto toPreview = [&](QPointF point) { return plot.mapTo(&preview, point.toPoint()); };
    return {.plotArea   = toPreview(area.center()),
            .tickLabels = toPreview(QPointF(area.left() / 2.0, area.center().y())),
            .title      = toPreview(QPointF(area.center().x(), area.top() / 2.0)),
            .zoomIcon   = preview.zoomIcon().geometry().center()};
}

/// @p preview shown (offscreen) with two curves: whether its window was exposed.
[[nodiscard]] bool showWithCurves(ThrustCurvePreview& preview)
{
    const std::vector<std::shared_ptr<const ThrustCurveMotor>> motors{
        QtRocket::Test::makeEmbeddedTestMotor("C6", 14.0, "first"),
        QtRocket::Test::makeEmbeddedTestMotor("C6", 12.0, "second")};
    preview.chart().setCurves(motors, 0);
    preview.resize(ThrustCurvePreview::kWidth, ThrustCurvePreview::kHeight);
    preview.show();
    return QTest::qWaitForWindowExposed(&preview);
}

TEST_F(ThrustCurvePreviewTest, LaysOutTheChartAndTheZoomIconAsOpenRocketsLayeredPane)
{
    // A hidden widget gets its resize events when it is shown.
    ThrustCurvePreview preview;
    preview.resize(ThrustCurvePreview::kWidth, ThrustCurvePreview::kHeight);
    preview.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&preview));
    EXPECT_EQ(preview.chart().geometry(),
              QRect(0, 0, ThrustCurvePreview::kWidth, ThrustCurvePreview::kHeight));
    EXPECT_EQ(preview.zoomIcon().geometry(), QRect(250, 12, 50, 50));
    preview.resize(kWiderWidth, kTallerHeight);
    EXPECT_EQ(preview.chart().geometry(), QRect(0, 0, kWiderWidth, kTallerHeight));
    EXPECT_EQ(preview.zoomIcon().geometry(), QRect(kWiderWidth - 50, 12, 50, 50));

    EXPECT_EQ(preview.sizeHint(), QSize(300, 180));
    EXPECT_EQ(preview.minimumSizeHint(), QSize(300, 180));
    EXPECT_EQ(preview.frameShape(), QFrame::NoFrame);
    EXPECT_EQ(preview.chart().chartStyle(), ThrustCurvePlot::Style::PREVIEW);
    EXPECT_EQ(preview.chart().curveCount(), 0);
    EXPECT_EQ(preview.chart().objectName(), QStringLiteral("chartPanel"));
    EXPECT_EQ(preview.zoomIcon().objectName(), QStringLiteral("zoomIcon"));
    EXPECT_EQ(preview.zoomIcon().alignment(), Qt::AlignCenter);
    const int iconSize = Icons::iconHeight(Icons::Id::ZOOM_IN, preview.font());
    EXPECT_EQ(preview.zoomIcon().pixmap().deviceIndependentSize().toSize(),
              QSize(iconSize, iconSize));
    // The chart and the icon let every press through to the frame (rocketplot's N6).
    EXPECT_TRUE(preview.chart().testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_TRUE(preview.zoomIcon().testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_EQ(preview.cursor().shape(), Qt::PointingHandCursor);
    preview.close();
}

TEST_F(ThrustCurvePreviewTest, AClickOfTheLeftButtonAnywhereRequestsThePlot)
{
    ThrustCurvePreview preview;
    ASSERT_TRUE(showWithCurves(preview));
    const PreviewPlaces places = placesOf(preview);
    QWindow* const      window = preview.windowHandle();
    ASSERT_NE(window, nullptr);
    const QSignalSpy requested(&preview, &ThrustCurvePreview::plotRequested);

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, places.plotArea);
    EXPECT_EQ(requested.count(), 1);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, places.tickLabels);
    EXPECT_EQ(requested.count(), 2);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, places.title);
    EXPECT_EQ(requested.count(), 3);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, places.zoomIcon);
    EXPECT_EQ(requested.count(), 4);
    preview.close();
}

TEST_F(ThrustCurvePreviewTest, OtherButtonsAndADragRequestNothing)
{
    ThrustCurvePreview preview;
    ASSERT_TRUE(showWithCurves(preview));
    const PreviewPlaces places = placesOf(preview);
    QWindow* const      window = preview.windowHandle();
    ASSERT_NE(window, nullptr);
    const QSignalSpy              requested(&preview, &ThrustCurvePreview::plotRequested);
    const rocketplot::Axis&       xAxis  = *preview.chart().plotWidget().xAxis();
    const Gui::Test::AxisReadback before = Gui::Test::readAxis(xAxis);

    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, places.plotArea);
    QTest::mouseClick(window, Qt::MiddleButton, Qt::NoModifier, places.plotArea);
    const QPoint end = places.plotArea + QPoint(kDragDistance, 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, places.plotArea);
    QTest::mouseMove(window, end);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
    EXPECT_EQ(requested.count(), 0);
    // Nor did the plot pan or zoom: it takes no gesture.
    EXPECT_EQ(Gui::Test::readAxis(xAxis), before);
    preview.close();
}

TEST_F(ThrustCurvePreviewTest, ShowsAHandCursorOverAllOfIt)
{
    ThrustCurvePreview preview;
    ASSERT_TRUE(showWithCurves(preview));
    const PreviewPlaces places = placesOf(preview);
    QWindow* const      window = preview.windowHandle();
    ASSERT_NE(window, nullptr);
    QTest::mouseMove(window, places.plotArea);
    EXPECT_EQ(window->cursor().shape(), Qt::PointingHandCursor);
    QTest::mouseMove(window, places.tickLabels);
    EXPECT_EQ(window->cursor().shape(), Qt::PointingHandCursor);
    QTest::mouseMove(window, places.zoomIcon);
    EXPECT_EQ(window->cursor().shape(), Qt::PointingHandCursor);
    preview.close();
}

TEST_F(ThrustCurvePreviewTest, ShowsNoContextMenuAndPassesNoneOnToItsParent)
{
    QWidget parent;
    parent.setContextMenuPolicy(Qt::CustomContextMenu);
    const QSignalSpy  parentMenu(&parent, &QWidget::customContextMenuRequested);
    auto* const       preview = makeChild<ThrustCurvePreview>(&parent);
    const QPoint      middle(ThrustCurvePreview::kWidth / 2, ThrustCurvePreview::kHeight / 2);
    QContextMenuEvent event(QContextMenuEvent::Mouse, middle, preview->mapToGlobal(middle));
    QApplication::sendEvent(preview, &event);
    EXPECT_EQ(parentMenu.count(), 0);
    EXPECT_EQ(QApplication::activePopupWidget(), nullptr);
}

TEST_F(ThrustCurvePreviewTest, RedrawsTheZoomIconWhenThePaletteChanges)
{
    const Palette            light(Palette::light());
    const ThrustCurvePreview preview;
    const qint64             lightIcon = preview.zoomIcon().pixmap().cacheKey();
    const Palette            dark(Palette::dark());
    // Qt hands a new application palette to the widgets through posted events.
    QCoreApplication::sendPostedEvents();
    EXPECT_NE(preview.zoomIcon().pixmap().cacheKey(), lightIcon);
    EXPECT_FALSE(preview.zoomIcon().pixmap().isNull());
}

}  // namespace

}  // namespace QtRocket::Gui
