#include "QtRocket/gui/figure/RocketFigure.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QSignalSpy>
#include <QSize>
#include <QTest>
#include <QTransform>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/CGCaret.h"
#include "QtRocket/gui/figure/CPCaret.h"
#include "QtRocket/gui/figure/FigureElement.h"
#include "QtRocket/gui/figure/RocketFigurePainter.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "RecordingPaintDevice.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using RocketFigureTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();
/// GUIUtil.getDPI() when headless, for figures whose numbers a test compares.
constexpr double kDotsPerInch = 96.0;

/// Fits @p figure to 1000 x 700 pixels and gives it its canvas size, as
/// RocketPanel.create2DPreviewFigure() does.
void fitFigure(RocketFigure& figure)
{
    figure.scaleToFit(QSize(1000, 700));
    figure.updateFigure();
    figure.resize(figure.canvasSize());
    figure.updateFigure();
}

/// The part of @p figure the tests paint: at most 2000 x 2000 pixels of its top left corner (the
/// canvas of an absurd design is QWIDGETSIZE_MAX pixels wide).
[[nodiscard]] QRect paintedArea(const RocketFigure& figure)
{
    return figure.rect().intersected(QRect(0, 0, 2000, 2000));
}

/// What @p figure paints (QWidget::render() into a recording device), without its background.
[[nodiscard]] std::vector<RecordedPaint> paintRecords(RocketFigure& figure)
{
    const QRect          area = paintedArea(figure);
    RecordingPaintDevice device(area.size());
    figure.render(&device, QPoint(), QRegion(area), QWidget::RenderFlags());
    std::vector<RecordedPaint> records = device.records();
    EXPECT_FALSE(records.empty());
    if (!records.empty())
    {
        EXPECT_EQ(records.front().brush.color(), figure.backgroundColor());
        records.erase(records.begin());
    }
    return records;
}

/// Paints @p figure into an image, as a preview would.
void paintImage(RocketFigure& figure)
{
    const QRect area = paintedArea(figure);
    QImage      image(area.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    figure.render(&image, QPoint(), QRegion(area));
}

/// The answer to the rocket's change events of this pass of the event loop.
void deliverRocketChanges(RocketFigure& figure)
{
    QCoreApplication::sendPostedEvents(&figure, QEvent::MetaCall);
}

TEST_F(RocketFigureTest, StartsSideOnUnrotatedWithNothingSelectedAndCarets)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    const RocketFigure             figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    EXPECT_EQ(&figure.rocket(), beta.rocket.get());
    EXPECT_EQ(figure.view(), FigureView::SIDE);
    EXPECT_EQ(figure.rotation(), 0.0);
    EXPECT_TRUE(figure.selection().empty());
    EXPECT_TRUE(figure.isDrawCarets());
    EXPECT_DOUBLE_EQ(figure.baseScale(), kDotsPerInch * ScaleFigure::kInchesPerMeter);
    EXPECT_EQ(figure.userScale(), 1.0);
    // The subject is the rocket's: its length and its largest radius both ways.
    const Rectangle2D& subject = figure.subjectBounds();
    EXPECT_EQ(subject.x, 0.0);
    EXPECT_DOUBLE_EQ(subject.width, beta.rocket->getSelectedConfiguration().getLength());
    EXPECT_DOUBLE_EQ(subject.y, -subject.height / 2);
    EXPECT_EQ(figure.contentBounds(), subject);

    // The resolution of the preferences (DPI in tenths), as GUIUtil.getDPI().
    InMemoryPreferences tenths;
    tenths.putInt("DPI", 1500);
    const RocketFigure fromPreferences(*beta.rocket, tenths, nullptr);
    EXPECT_DOUBLE_EQ(fromPreferences.baseScale(), 150.0 * ScaleFigure::kInchesPerMeter);
}

TEST_F(RocketFigureTest, AnnouncesChangesOfViewRotationAndSelection)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    const QSignalSpy               changed(&figure, &ScaleFigure::changed);

    figure.setView(FigureView::TOP);
    EXPECT_EQ(changed.count(), 1);
    figure.setView(FigureView::TOP);
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(figure.figureGeometry().view(), FigureView::TOP);

    figure.setRotation(0.5);
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(figure.rotation(), 0.5);
    EXPECT_DOUBLE_EQ(figure.rotation(true), 0.5 - (std::numbers::pi / 2));
    EXPECT_EQ(figure.figureGeometry().rotation(), 0.5);
    // MathUtil.equals(): no change.
    figure.setRotation(0.5 + 1e-12);
    EXPECT_EQ(figure.rotation(), 0.5);
    // QtRocket's deviation: not a number, no change.
    figure.setRotation(kNaN);
    figure.setRotation(kInfinity);
    EXPECT_EQ(figure.rotation(), 0.5);
    EXPECT_EQ(changed.count(), 2);
    figure.setView(FigureView::SIDE);
    EXPECT_EQ(figure.rotation(true), 0.5);

    const std::vector<const RocketComponent*> selection{beta.body, beta.fins};
    figure.setSelection(selection);
    EXPECT_EQ(changed.count(), 4);
    EXPECT_TRUE(figure.isSelected(*beta.body));
    EXPECT_TRUE(figure.isSelected(*beta.fins));
    EXPECT_FALSE(figure.isSelected(*beta.nose));
    EXPECT_EQ(figure.selection(), (std::vector{beta.body->getId(), beta.fins->getId()}));
    figure.setSelection({});
    EXPECT_FALSE(figure.isSelected(*beta.body));
}

TEST_F(RocketFigureTest, KeepsItsGeometryUntilSomethingItShowsChanges)
{
    // A rocket whose events are switched off after it was built: a change it makes is seen only
    // by a geometry built after it.
    Rocket    rocket;
    BodyTube& body = rocket.addChild(std::make_unique<AxialStage>())
                         .addChild(std::make_unique<BodyTube>(0.3, 0.02));
    rocket.enableEvents();
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    ASSERT_TRUE(figure.hasFigureGeometry());
    EXPECT_DOUBLE_EQ(figure.subjectBounds().width, 0.3);

    rocket.enableEvents(false);
    body.setLength(0.5);
    // Resizing, painting and hit-testing bring the figure up to date with the same geometry.
    fitFigure(figure);
    paintImage(figure);
    static_cast<void>(figure.componentsAt(QPointF(10, 10)));
    EXPECT_DOUBLE_EQ(figure.subjectBounds().width, 0.3);
    EXPECT_DOUBLE_EQ(figure.figureGeometry().subjectBounds().width, 0.3);

    // What the figure shows changed: a new geometry.
    figure.setRotation(1.0);
    EXPECT_DOUBLE_EQ(figure.subjectBounds().width, 0.5);
    body.setLength(0.7);
    figure.refresh();
    EXPECT_DOUBLE_EQ(figure.subjectBounds().width, 0.7);
}

TEST_F(RocketFigureTest, AnswersTheRocketsEventsOncePerPassOfTheEventLoop)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    const QSignalSpy               rocketChanged(&figure, &RocketFigure::rocketChanged);
    const double                   length = figure.subjectBounds().width;

    // Every event drops the geometry at once; one update answers them all.
    beta.body->setLength(beta.body->getLength() + 0.1);
    EXPECT_FALSE(figure.hasFigureGeometry());
    beta.body->setLength(beta.body->getLength() + 0.1);
    beta.nose->setLength(beta.nose->getLength() + 0.1);
    EXPECT_EQ(rocketChanged.count(), 0);
    EXPECT_EQ(figure.subjectBounds().width, length);

    deliverRocketChanges(figure);
    EXPECT_EQ(rocketChanged.count(), 1);
    EXPECT_TRUE(figure.hasFigureGeometry());
    EXPECT_NEAR(figure.subjectBounds().width, length + 0.3, 1e-12);

    deliverRocketChanges(figure);
    EXPECT_EQ(rocketChanged.count(), 1);
}

TEST_F(RocketFigureTest, NoComponentPointerOutlivesATreeOrUndoEvent)
{
    // Under AddressSanitizer this is the proof: the figure painted and hit-tested before the
    // events must not touch a component they destroyed.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    Rocket&                   rocket = document->getRocket();
    RocketComponent&          stage  = rocket.getChild(0);
    BodyTube&                 first  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.02));
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    fitFigure(figure);
    const std::vector<const RocketComponent*> selection{&first};
    figure.setSelection(selection);
    paintImage(figure);
    const QPointF middle = figure.projection().map(QPointF(0.15, 0));
    ASSERT_EQ(figure.componentsAt(middle).size(), 1U);

    // A TREE event: the body tube removed and destroyed.
    document->addUndoPosition("Remove");
    static_cast<void>(stage.removeChild(&first));
    paintImage(figure);
    EXPECT_TRUE(figure.componentsAt(middle).empty());

    // An UNDO event: Rocket::loadFrom() replaces every component, with the same ids.
    document->undo();
    paintImage(figure);
    const std::vector<RocketComponent*> found = figure.componentsAt(middle);
    ASSERT_EQ(found.size(), 1U);
    EXPECT_NE(found.front(), nullptr);
    EXPECT_EQ(found.front()->kind(), ComponentKind::BODY_TUBE);
    // The selection is kept by id: the loaded body tube is still selected.
    EXPECT_TRUE(figure.isSelected(*found.front()));
    deliverRocketChanges(figure);
    paintImage(figure);
}

TEST_F(RocketFigureTest, ShowMarkersHidesTheMarkersOfUnselectedPodSets)
{
    const QtRocket::Test::TestEstesAlphaIIIWithPods pods;
    InMemoryPreferences                             preferences;
    RocketFigure figure(*pods.rocket, preferences, kDotsPerInch, nullptr);
    const auto   podSetShapes = [&figure, &pods] {
        std::size_t count = 0;
        for (const RocketFigureGeometry::InstanceShape& shape : figure.figureGeometry().shapes())
        {
            if (&shape.shape.component() == pods.podSet)
            {
                ++count;
            }
        }
        return count;
    };
    const std::size_t markers = podSetShapes();
    EXPECT_GT(markers, 0U);

    // The preference is read whenever the geometry is needed.
    preferences.setShowMarkers(true);
    EXPECT_EQ(podSetShapes(), 0U);
    const std::vector<const RocketComponent*> selection{pods.podSet};
    figure.setSelection(selection);
    EXPECT_EQ(podSetShapes(), markers);
}

TEST_F(RocketFigureTest, FindsOverlappingComponentsTheHighestDisplayOrderFirst)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    fitFigure(figure);

    // The middle of the engine block, inside the motor mount, inside the body tube.
    const RocketComponent& block = *beta.block;
    QRectF                 blockBounds;
    for (const RocketFigureGeometry::InstanceShape& shape : figure.figureGeometry().shapes())
    {
        if (&shape.shape.component() == &block)
        {
            const Rectangle2D bounds = shape.shape.shape().bounds();
            blockBounds              = QRectF(bounds.x, bounds.y, bounds.width, bounds.height);
            break;
        }
    }
    ASSERT_FALSE(blockBounds.isNull());
    const QPointF                       point = figure.projection().map(blockBounds.center());
    const std::vector<RocketComponent*> expected{beta.block, beta.inner, beta.body};
    EXPECT_EQ(figure.componentsAt(point), expected);

    // A hidden component is still found (Java's quirk), and nothing is found in empty space.
    beta.body->setVisible(false);
    EXPECT_EQ(figure.componentsAt(point), expected);
    EXPECT_TRUE(figure.componentsAt(QPointF(1, 1)).empty());
}

TEST_F(RocketFigureTest, HitTestsTheRocketAsItIsNowBeforeTheFigureHasCaughtUp)
{
    Rocket    rocket;
    BodyTube& body = rocket.addChild(std::make_unique<AxialStage>())
                         .addChild(std::make_unique<BodyTube>(0.4, 0.02));
    rocket.enableEvents();
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    // A figure wider than the rocket at either length: the rocket is centred in it.
    ASSERT_TRUE(figure.scaleTo(0.5, QSize(3000, 1000)));
    figure.resize(figure.canvasSize());
    figure.updateFigure();
    ASSERT_EQ(figure.size(), QSize(3000, 1000));
    const QPoint before = figure.subjectOrigin();

    // Twice as long: the origin moves left by half the added length. The figure has not caught
    // up with the event, but the hit test maps the pixel as the rocket is drawn now: 5 cm behind
    // the front of the body tube (with the old origin, 15 cm before it).
    body.setLength(0.8);
    ASSERT_FALSE(figure.hasFigureGeometry());
    const double scale  = figure.absoluteScale();
    const QPoint origin = ScaleFigure::viewOrigin(FigureView::SIDE, figure.size(),
                                                  figure.figureGeometry().subjectBounds(), scale);
    EXPECT_NEAR(before.x() - origin.x(), 0.2 * scale, 1.0);
    const QPointF                       pixel(origin.x() + (0.05 * scale), origin.y());
    const std::vector<RocketComponent*> expected{&body};
    EXPECT_EQ(figure.componentsAt(pixel), expected);
    EXPECT_EQ(figure.subjectOrigin(), origin);

    // The same once it has caught up.
    deliverRocketChanges(figure);
    EXPECT_EQ(figure.size(), QSize(3000, 1000));
    EXPECT_EQ(figure.subjectOrigin(), origin);
    EXPECT_EQ(figure.componentsAt(pixel), expected);
}

TEST_F(RocketFigureTest, ASelectionChangesTheGeometryOnlyThroughTheMarkers)
{
    // A rocket whose events are switched off after it was built: a change it makes is seen only
    // by a geometry built after it, so a geometry that was kept shows the old length.
    Rocket    rocket;
    BodyTube& body = rocket.addChild(std::make_unique<AxialStage>())
                         .addChild(std::make_unique<BodyTube>(0.3, 0.02));
    rocket.enableEvents();
    InMemoryPreferences preferences;
    RocketFigure        figure(rocket, preferences, kDotsPerInch, nullptr);
    rocket.enableEvents(false);
    const std::vector<const RocketComponent*> selection{&body};

    // ShowMarkers off: the selected components are drawn wider at paint time, and the geometry
    // is kept.
    body.setLength(0.5);
    figure.setSelection(selection);
    EXPECT_TRUE(figure.isSelected(body));
    EXPECT_DOUBLE_EQ(figure.figureGeometry().subjectBounds().width, 0.3);
    figure.setSelection({});
    EXPECT_DOUBLE_EQ(figure.figureGeometry().subjectBounds().width, 0.3);

    // ShowMarkers on: the markers of pod sets and booster sets depend on the selection. The
    // preference's change itself makes a new geometry.
    preferences.setShowMarkers(true);
    EXPECT_DOUBLE_EQ(figure.figureGeometry().subjectBounds().width, 0.5);
    body.setLength(0.7);
    figure.setSelection(selection);
    EXPECT_DOUBLE_EQ(figure.figureGeometry().subjectBounds().width, 0.7);
}

TEST_F(RocketFigureTest, AFittingScrollAreaShowsAnAbsurdlyLongRocket)
{
    // A body tube 1e200 m long loads from an .ork without a warning. Fitting it once recursed
    // until the stack overflowed (ScaleFigure::scaleTo()'s tolerance underflows below a scale of
    // about 1e-161).
    const QtRocket::Test::TestBeta beta;
    beta.body->setLength(1e200);
    const InMemoryPreferences preferences;
    QWidget                   holder;  // the figure's parent until the area takes it over
    auto* figure = makeChild<RocketFigure>(&holder, *beta.rocket, preferences, kDotsPerInch);
    ScaleScrollArea area(figure, nullptr);
    area.resize(800, 600);
    area.setFitting(true);
    const double fit = area.userScale();
    EXPECT_GT(fit, 0);
    EXPECT_LT(fit, 1e-200);
    EXPECT_EQ(fit, area.fitScale());

    // Every rocket change fits again (the design view's updateScaling()), and the view shows it.
    beta.body->setLength(1e300);
    deliverRocketChanges(*figure);
    area.updateScaling();
    EXPECT_GT(area.userScale(), 0);
    EXPECT_LT(area.userScale(), fit);
    EXPECT_EQ(area.userScale(), area.fitScale());
    area.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&area));
    EXPECT_FALSE(figure->figureGeometry().shapes().empty());
}

/// A figure element that records what it was painted with.
class RecordingElement final : public FigureElement
{
public:
    struct Call
    {
        const RecordingElement* element;
        double                  scale;
        QTransform              transform;
        QRect                   visible;
    };

    explicit RecordingElement(std::vector<Call>& calls) : m_calls(&calls) { }

    void paint(QPainter& painter, double scale, const QRect& visible) override
    {
        m_calls->push_back(Call{.element   = this,
                                .scale     = scale,
                                .transform = painter.worldTransform(),
                                .visible   = visible});
    }

private:
    std::vector<Call>* m_calls;
};

TEST_F(RocketFigureTest, PaintsTheExtrasInOpenRocketsOrderAndCoordinates)
{
    const QtRocket::Test::TestBeta      beta;
    const InMemoryPreferences           preferences;
    RocketFigure                        figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    std::vector<RecordingElement::Call> calls;
    RecordingElement                    relative(calls);
    RecordingElement                    absolute(calls);
    RecordingElement                    top(calls);
    figure.addRelativeTopExtra(top);
    figure.addAbsoluteExtra(absolute);
    figure.addRelativeExtra(relative);
    fitFigure(figure);

    paintImage(figure);
    ASSERT_EQ(calls.size(), 3U);
    EXPECT_EQ(calls[0].element, &relative);
    EXPECT_EQ(calls[0].scale, figure.absoluteScale());
    EXPECT_EQ(calls[0].transform, figure.projection());
    EXPECT_EQ(calls[0].visible, figure.rect());
    EXPECT_EQ(calls[1].element, &absolute);
    EXPECT_EQ(calls[1].scale, 1.0);
    EXPECT_TRUE(calls[1].transform.isIdentity());
    EXPECT_EQ(calls[2].element, &top);
    EXPECT_EQ(calls[2].transform, figure.projection());

    // Without the carets, only the absolute extras.
    calls.clear();
    figure.setDrawCarets(false);
    EXPECT_FALSE(figure.isDrawCarets());
    paintImage(figure);
    ASSERT_EQ(calls.size(), 1U);
    EXPECT_EQ(calls[0].element, &absolute);

    calls.clear();
    figure.setDrawCarets(true);
    figure.removeRelativeExtra(relative);
    figure.clearRelativeTopExtra();
    figure.removeAbsoluteExtra(absolute);
    paintImage(figure);
    EXPECT_TRUE(calls.empty());
    figure.addRelativeExtra(relative);
    figure.addAbsoluteExtra(absolute);
    figure.clearRelativeExtra();
    figure.clearAbsoluteExtra();
    paintImage(figure);
    EXPECT_TRUE(calls.empty());
}

TEST_F(RocketFigureTest, ANewDesignHasNoLengthAndNothingToPaint)
{
    // OpenRocketDocumentFactory.createNewRocket(): a stage without children; the bounding box is
    // (0, 0, 0) to (1, 0, 0) and RocketPanel puts the carets at NaN.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    for (const FigureView view : kAllFigureViews)
    {
        RocketFigure figure(document->getRocket(), preferences, kDotsPerInch, nullptr);
        figure.setView(view);
        CGCaret cg(kNaN, kNaN);
        CPCaret cp(kNaN, kNaN);
        figure.addRelativeExtra(cp);
        figure.addRelativeExtra(cg);
        fitFigure(figure);
        EXPECT_TRUE(figure.figureGeometry().shapes().empty());
        EXPECT_TRUE(paintRecords(figure).empty()) << figureViewName(view);
        paintImage(figure);
        EXPECT_TRUE(figure.componentsAt(QPointF(figure.subjectOrigin())).empty());
    }
}

TEST_F(RocketFigureTest, SizesThatAreNotNumbersNeverReachQt)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    CGCaret                        cg(kInfinity, 0);
    figure.addRelativeExtra(cg);
    fitFigure(figure);
    for (const double length : {kInfinity, kNaN, 1e300})
    {
        beta.body->setLength(length);
        deliverRocketChanges(figure);
        fitFigure(figure);
        for (const RecordedPaint& record : paintRecords(figure))
        {
            EXPECT_TRUE(RocketFigurePainter::isDrawable(record.devicePath())) << length;
        }
        // Qt's warnings fail the test.
        paintImage(figure);
    }
}

TEST_F(RocketFigureTest, SeesAllOfItselfOffScreenAndZoomsLeftOfTheOrigin)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    fitFigure(figure);
    EXPECT_EQ(figure.visibleRect(), figure.rect());
    EXPECT_EQ(figure.autoZoomPoint(), QPoint(std::max(0, figure.subjectOrigin().x() - 30), 0));
}

/// The views and rotations every design is painted in.
struct ViewCase
{
    FigureView view;
    double     rotation;
};

constexpr std::array kViewCases{
    ViewCase{.view = FigureView::SIDE, .rotation = 0.0},
    ViewCase{.view = FigureView::SIDE, .rotation = 5 * std::numbers::pi / 4},
    ViewCase{.view = FigureView::TOP, .rotation = 0.0},
    ViewCase{.view = FigureView::BACK, .rotation = 0.0},
    ViewCase{.view = FigureView::BACK, .rotation = std::numbers::pi / 6},
};

/// Paints @p rocket in every view of kViewCases, everything selected, with carets at the middle of
/// the subject: Qt's warnings fail the test, and something must be drawn.
void paintsInEveryView(Rocket& rocket)
{
    const InMemoryPreferences           preferences;
    std::vector<const RocketComponent*> everything;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        everything.push_back(&component);
    }
    for (const ViewCase& viewCase : kViewCases)
    {
        RocketFigure figure(rocket, preferences, kDotsPerInch, nullptr);
        figure.setView(viewCase.view);
        figure.setRotation(viewCase.rotation);
        figure.setSelection(everything);
        const Rectangle2D& subject = figure.subjectBounds();
        CGCaret            cg(subject.x + (subject.width / 2), 0);
        CPCaret            cp(subject.x + (subject.width / 3), 0);
        figure.addRelativeExtra(cp);
        figure.addRelativeExtra(cg);
        fitFigure(figure);
        paintImage(figure);
        EXPECT_FALSE(paintRecords(figure).empty()) << figureViewName(viewCase.view);
    }
}

class RocketFigureExamples : public Test::GuiTest,
                             public ::testing::WithParamInterface<QtRocket::Test::GoldenExample>
{ };

TEST_P(RocketFigureExamples, PaintEveryViewWithoutAQtWarning)
{
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(GetParam());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    paintsInEveryView(loaded->document->getRocket());
}

INSTANTIATE_TEST_SUITE_P(Examples, RocketFigureExamples,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

class RocketFigureTestRockets : public Test::GuiTest,
                                public ::testing::WithParamInterface<std::size_t>
{ };

TEST_P(RocketFigureTestRockets, PaintEveryViewWithoutAQtWarning)
{
    const QtRocket::Test::TestRocketMaker& maker  = QtRocket::Test::testRocketMakers()[GetParam()];
    const std::unique_ptr<Rocket>          rocket = maker.make();
    paintsInEveryView(*rocket);
}

INSTANTIATE_TEST_SUITE_P(TestRockets, RocketFigureTestRockets,
                         ::testing::Range<std::size_t>(0,
                                                       QtRocket::Test::testRocketMakers().size()),
                         [](const ::testing::TestParamInfo<std::size_t>& paramInfo) {
                             return std::string(
                                 QtRocket::Test::testRocketMakers()[paramInfo.param].method);
                         });

}  // namespace

}  // namespace QtRocket::Gui
