#include "QtRocket/gui/figure/ScrollAreaZoomModel.h"

#include <memory>

#include <QComboBox>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/figure/ScaleSelector.h"
#include "QtRocket/gui/figure/ZoomModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/Geometry2D.h"
#include "TestScaleFigure.h"

namespace QtRocket::Gui
{

namespace
{

using ScrollAreaZoomModelTest = Test::GuiTest;
using Test::TestScaleFigure;

/// "A simple model rocket.ork" in the side view and OpenRocket's fit scale for it in 760 x 560
/// pixels, the fitted view of 800 x 600 (ScaleProbe).
constexpr Rectangle2D kSimpleSide{.x      = 0.0,
                                  .y      = -0.05589616641898764,
                                  .width  = 0.42539999999999994,
                                  .height = 0.11179233283797528};
constexpr double      kSimpleFit = 0.4353743143415312;

/// A TestScaleFigure in a fitted ScaleScrollArea of 800 x 600 pixels, shown (offscreen).
struct FittedArea
{
    FittedArea() : figure(makeChild<TestScaleFigure>(&holder, kSimpleSide, FigureView::SIDE))
    {
        area.resize(800, 600);
        area.show();
        area.setFitting(true);
    }

    QWidget          holder;  // the figure's parent until the area takes it over
    TestScaleFigure* figure;
    ScaleScrollArea  area{figure, nullptr};
};

TEST_F(ScrollAreaZoomModelTest, TheUserScaleOfTheFinEditor)
{
    FittedArea          fitted;
    ScrollAreaZoomModel model(&fitted.area, ScrollAreaZoomModel::Kind::USER_SCALE, nullptr);
    EXPECT_EQ(model.kind(), ScrollAreaZoomModel::Kind::USER_SCALE);
    EXPECT_EQ(model.scale(), kSimpleFit);
    EXPECT_TRUE(model.isFit());

    const QSignalSpy changes(&model, &ZoomModel::changed);
    model.setScale(2.0);
    EXPECT_EQ(fitted.area.userScale(), 2.0);
    EXPECT_EQ(model.scale(), 2.0);
    EXPECT_FALSE(model.isFit());
    EXPECT_GE(changes.count(), 1);

    model.setFit();
    EXPECT_TRUE(fitted.area.isFitting());
    EXPECT_EQ(model.scale(), kSimpleFit);
}

TEST_F(ScrollAreaZoomModelTest, TheScaleRelativeToTheFitOfTheDesignView)
{
    FittedArea          fitted;
    ScrollAreaZoomModel model(&fitted.area, ScrollAreaZoomModel::Kind::RELATIVE_TO_FIT, nullptr);
    EXPECT_EQ(model.scale(), 1.0);
    model.setScale(1.5);
    EXPECT_EQ(fitted.area.userScale(), 1.5 * kSimpleFit);
    EXPECT_DOUBLE_EQ(model.scale(), 1.5);
    EXPECT_FALSE(model.isFit());
    model.setFit();
    EXPECT_EQ(model.scale(), 1.0);
    EXPECT_TRUE(model.isFit());
}

TEST_F(ScrollAreaZoomModelTest, TheDesignViewsZoomControls)
{
    // RocketPanel's ScaleSelector: 100 % is the fit.
    FittedArea fitted;
    QWidget    panel;
    auto*      model  = makeChild<ScrollAreaZoomModel>(&panel, &fitted.area,
                                                       ScrollAreaZoomModel::Kind::RELATIVE_TO_FIT);
    auto*      layout = makeChild<QVBoxLayout>(&panel);
    auto*      selector = makeChild<ScaleSelector>(&panel, model);
    layout->addWidget(selector);
    EXPECT_EQ(selector->scaleCombo()->currentText(), QStringLiteral("100%(fit)"));

    QTest::mouseClick(selector->zoomInButton(), Qt::LeftButton);
    EXPECT_EQ(fitted.area.userScale(), 1.5 * kSimpleFit);
    EXPECT_EQ(selector->scaleCombo()->currentText(), QStringLiteral("150%"));

    // The area's own zoom shows in the box.
    fitted.area.setRelativeScaling(0.5);
    EXPECT_EQ(selector->scaleCombo()->currentText(), QStringLiteral("50%"));

    QTest::mouseClick(selector->zoomFitButton(), Qt::LeftButton);
    EXPECT_TRUE(fitted.area.isFitting());
    EXPECT_EQ(fitted.area.userScale(), kSimpleFit);
    EXPECT_EQ(selector->scaleCombo()->currentText(), QStringLiteral("100%(fit)"));
}

TEST_F(ScrollAreaZoomModelTest, DoesNothingOnceTheAreaIsGone)
{
    auto                fitted = std::make_unique<FittedArea>();
    ScrollAreaZoomModel model(&fitted->area, ScrollAreaZoomModel::Kind::USER_SCALE, nullptr);
    fitted.reset();
    EXPECT_EQ(model.scale(), 1.0);
    EXPECT_FALSE(model.isFit());
    model.setScale(2.0);
    model.setFit();
}

}  // namespace

}  // namespace QtRocket::Gui
