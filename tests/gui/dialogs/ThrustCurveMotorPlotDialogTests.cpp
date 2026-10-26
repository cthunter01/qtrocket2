#include "QtRocket/gui/dialogs/ThrustCurveMotorPlotDialog.h"

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "TestGuiServices.h"
#include "file/openrocket/HandlerTestSupport.h"

// OpenRocket has no JUnit test of ThrustCurveMotorPlotDialog. What its constructor makes (the
// title, the modality, the chart's size after pack(), the Close button, Esc) is pinned from
// OpenRocket's own dialog in ThrustCurvePins.h and compared in thrust_curve_plot_golden_tests.cpp;
// the chart itself in ThrustCurvePlotTests.cpp. These tests are about how the dialog behaves.

namespace QtRocket::Gui
{

namespace
{

using ThrustCurveMotorPlotDialogTest = Gui::Test::GuiTest;
using Curves                         = std::vector<std::shared_ptr<const ThrustCurveMotor>>;

constexpr double kPeakThrust      = 14.0;
constexpr double kOtherPeakThrust = 12.0;
constexpr int    kGrowth          = 100;

/// Two Estes "C6" curves of three points each.
[[nodiscard]] Curves testCurves()
{
    return {QtRocket::Test::makeEmbeddedTestMotor("C6", kPeakThrust, "first"),
            QtRocket::Test::makeEmbeddedTestMotor("C6", kOtherPeakThrust, "second")};
}

/// @p widget's rectangle in @p dialog's coordinates.
[[nodiscard]] QRect rectIn(const QWidget& widget, const QDialog& dialog)
{
    return {widget.mapTo(&dialog, QPoint(0, 0)), widget.size()};
}

/// Delivers the deferred deletions.
void deleteLater()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_F(ThrustCurveMotorPlotDialogTest, ShowsTheCurvesGivenInADialogChartUnderJavasNames)
{
    const Gui::Test::TestGuiServices services;
    const ThrustCurveMotorPlotDialog dialog(testCurves(), 1, services.services());
    EXPECT_EQ(dialog.objectName(), QStringLiteral("plotDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Motor thrust curves"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_FALSE(dialog.isVisible());

    const ThrustCurvePlot& chart = dialog.chart();
    EXPECT_EQ(chart.chartStyle(), ThrustCurvePlot::Style::DIALOG);
    EXPECT_EQ(chart.curveCount(), 2);
    EXPECT_EQ(chart.selectedCurve(), 1);
    EXPECT_EQ(dialog.findChild<ThrustCurvePlot*>(QStringLiteral("chartPanel")), &chart);
    EXPECT_EQ(dialog.closeButton().objectName(), QStringLiteral("close"));
    EXPECT_EQ(dialog.closeButton().text(), QStringLiteral("Close"));
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("close")), &dialog.closeButton());
}

TEST_F(ThrustCurveMotorPlotDialogTest, OpensWithTheChartAtItsSmallestSizeAndGrowsIt)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurveMotorPlotDialog       dialog(testCurves(), 0, services.services());
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    const QSize smallest(ThrustCurveMotorPlotDialog::kChartWidth,
                         ThrustCurveMotorPlotDialog::kChartHeight);
    EXPECT_EQ(dialog.chart().size(), smallest);
    EXPECT_EQ(dialog.chart().minimumSize(), smallest);
    const QRect chart = rectIn(dialog.chart(), dialog);
    const QRect close = rectIn(dialog.closeButton(), dialog);
    EXPECT_EQ(close.right(), chart.right());
    EXPECT_EQ(close.top(), chart.bottom() + 1 + ThrustCurveMotorPlotDialog::kParagraphGap);

    // "grow": the chart takes what the dialog gains, the button stays at the bottom right.
    dialog.resize(dialog.size() + QSize(kGrowth, kGrowth));
    QCoreApplication::processEvents();
    EXPECT_EQ(dialog.chart().size(), smallest + QSize(kGrowth, kGrowth));
    EXPECT_EQ(rectIn(dialog.closeButton(), dialog).right(), rectIn(dialog.chart(), dialog).right());
    EXPECT_EQ(dialog.closeButton().size(), close.size());
    // The chart never gets smaller than its smallest size.
    EXPECT_GE(dialog.minimumSizeHint().width(), ThrustCurveMotorPlotDialog::kChartWidth);
    EXPECT_GE(dialog.minimumSizeHint().height(), ThrustCurveMotorPlotDialog::kChartHeight);
    dialog.close();
}

TEST_F(ThrustCurveMotorPlotDialogTest, ShowPlotShowsItModalOverItsParent)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          parent;
    parent.show();
    ThrustCurveMotorPlotDialog& dialog =
        ThrustCurveMotorPlotDialog::showPlot(testCurves(), 0, services.services(), parent);
    const QPointer<ThrustCurveMotorPlotDialog> shown(&dialog);
    EXPECT_EQ(dialog.parentWidget(), &parent);
    EXPECT_TRUE(dialog.isWindow());
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_TRUE(dialog.isModal());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.chart().curveCount(), 2);
    dialog.closeButton().click();
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(ThrustCurveMotorPlotDialogTest, CloseClosesThePlotAndDeletesIt)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          parent;
    ThrustCurveMotorPlotDialog&      dialog =
        ThrustCurveMotorPlotDialog::showPlot(testCurves(), 0, services.services(), parent);
    const QPointer<ThrustCurveMotorPlotDialog> shown(&dialog);
    const QSignalSpy                           finished(&dialog, &QDialog::finished);
    QTest::mouseClick(&dialog.closeButton(), Qt::LeftButton);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_FALSE(dialog.isVisible());
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(ThrustCurveMotorPlotDialogTest, EscapeClosesThePlotAndDeletesIt)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          parent;
    ThrustCurveMotorPlotDialog&      dialog =
        ThrustCurveMotorPlotDialog::showPlot(testCurves(), 0, services.services(), parent);
    const QPointer<ThrustCurveMotorPlotDialog> shown(&dialog);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    QTest::keyClick(&dialog.chart(), Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(ThrustCurveMotorPlotDialogTest, TheWindowsCloseButtonClosesThePlotAndDeletesIt)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          parent;
    ThrustCurveMotorPlotDialog&      dialog =
        ThrustCurveMotorPlotDialog::showPlot(testCurves(), 0, services.services(), parent);
    const QPointer<ThrustCurveMotorPlotDialog> shown(&dialog);
    EXPECT_TRUE(dialog.close());
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(ThrustCurveMotorPlotDialogTest, EnterClicksNoButton)
{
    // GUIUtil.setDisposableDialogOptions(dialog, null): no default button.
    const Gui::Test::TestGuiServices services;
    ThrustCurveMotorPlotDialog       dialog(testCurves(), 0, services.services());
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    EXPECT_FALSE(dialog.closeButton().isDefault());
    EXPECT_FALSE(dialog.closeButton().autoDefault());
    QTest::keyClick(&dialog, Qt::Key_Return);
    QTest::keyClick(&dialog, Qt::Key_Enter, Qt::KeypadModifier);
    dialog.closeButton().setFocus();
    QTest::keyClick(&dialog.closeButton(), Qt::Key_Return);
    EXPECT_TRUE(dialog.isVisible());
    // Space clicks the focused button, as in Swing.
    QTest::keyClick(&dialog.closeButton(), Qt::Key_Space);
    EXPECT_FALSE(dialog.isVisible());
}

TEST_F(ThrustCurveMotorPlotDialogTest, APlotOpenWhenItsParentGoesGoesWithIt)
{
    const Gui::Test::TestGuiServices services;
    auto                             parent = std::make_unique<QWidget>();
    parent->show();
    const QPointer<ThrustCurveMotorPlotDialog> shown(
        &ThrustCurveMotorPlotDialog::showPlot(testCurves(), 0, services.services(), *parent));
    ASSERT_TRUE(QTest::qWaitForWindowExposed(shown.data()));
    parent.reset();
    EXPECT_TRUE(shown.isNull());
    deleteLater();
}

TEST_F(ThrustCurveMotorPlotDialogTest, ASelectionOutsideTheCurvesOrANullMotorIsABug)
{
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = testCurves();
    EXPECT_THROW(ThrustCurveMotorPlotDialog(motors, 2, services.services()), BugError);
    const Curves withNull{motors.front(), nullptr};
    EXPECT_THROW(ThrustCurveMotorPlotDialog(withNull, 0, services.services()), BugError);
    QWidget parent;
    EXPECT_THROW(static_cast<void>(
                     ThrustCurveMotorPlotDialog::showPlot(motors, -2, services.services(), parent)),
                 BugError);
    deleteLater();
    EXPECT_TRUE(parent.findChildren<QDialog*>().isEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
