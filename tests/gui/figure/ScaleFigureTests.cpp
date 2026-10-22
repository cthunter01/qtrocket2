#include "QtRocket/gui/figure/ScaleFigure.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QSize>
#include <QTransform>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/Geometry2D.h"
#include "TestScaleFigure.h"

namespace QtRocket::Gui
{

namespace
{

using ScaleFigureTest = Test::GuiTest;
using Test::TestScaleFigure;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// A subject 1 m and 2 m long, 0.2 m across, from the nose; "A simple model rocket.ork" in the side
/// view.
constexpr Rectangle2D kMetreSubject{.x = 0, .y = -0.1, .width = 1, .height = 0.2};
constexpr Rectangle2D kTwoMetreSubject{.x = 0, .y = -0.1, .width = 2, .height = 0.2};
constexpr Rectangle2D kSimpleSide{.x      = 0.0,
                                  .y      = -0.05589616641898764,
                                  .width  = 0.42539999999999994,
                                  .height = 0.11179233283797528};

/// OpenRocket's RocketFigure showing a subject in a visible area: the probe ScaleProbe.figures()
/// (OpenRocket 5f164fd0e, headless, so at 96 dots per inch) for "A simple model rocket.ork"
/// ("simple"), "Two stage high power rocket.ork" ("twoStage") and a new Rocket ("empty"), each
/// view, each visible size, on a new figure: getFitScale(visible), then scaleTo(visible) and the
/// user scale after it, the preferred size, and after setSize(max(preferred, visible + 3)) and
/// updateFigure() the size, getSubjectOrigin() and getAutoZoomPoint(). The subject is the
/// figure's subjectBounds_m.
struct FigureRow
{
    const char* design;
    FigureView  view;
    Rectangle2D subject;
    QSize       visible;
    double      fit;
    bool        changed;
    double      user;
    QSize       canvas;
    QSize       size;
    QPoint      origin;
    QPoint      zoomPoint;
    /// Java accepted the fit scale 0 of a 60 x 40 area (no pixels left inside the border), which
    /// QtRocket refuses (ScaleFigure's deviation): the row holds the values of a refused fit,
    /// those of the 59 x 100 row.
    bool zeroFitRefused;
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr auto kFigureRows = std::to_array<FigureRow>({
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {760, 560}, 0.4353743143415312, true, 0.4353743143415312, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {1000, 700}, 0.5846455078300562, true, 0.5846455078300562, {1000, 700}, {1003, 703}, {31, 351}, {1, 0}, false},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {300, 900}, 0.14927119348852497, true, 0.14927119348852497, {300, 900}, {303, 903}, {32, 451}, {2, 0}, false},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {61, 41}, 6.219633062021874E-4, true, 6.219633062021874E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {60, 40}, 0.0, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, true},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {59, 100}, -6.219633062021874E-4, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, false},
    {"simple", FigureView::SIDE, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {0, 100}, 1.0, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {760, 560}, 0.4353743143415312, true, 0.4353743143415312, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {1000, 700}, 0.5846455078300562, true, 0.5846455078300562, {1000, 700}, {1003, 703}, {31, 351}, {1, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {300, 900}, 0.14927119348852497, true, 0.14927119348852497, {300, 900}, {303, 903}, {32, 451}, {2, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {61, 41}, 6.219633062021874E-4, true, 6.219633062021874E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {60, 40}, 0.0, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, true},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {59, 100}, -6.219633062021874E-4, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, false},
    {"simple", FigureView::TOP, {0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, {0, 100}, 1.0, false, 1.0, {1667, 462}, {1667, 462}, {30, 231}, {0, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {760, 560}, 1.2307038912747077, true, 1.2307038912747077, {760, 560}, {763, 563}, {381, 301}, {351, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {1000, 700}, 1.562047246617898, true, 1.562047246617898, {1000, 700}, {1003, 703}, {501, 371}, {471, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {300, 900}, 0.5680171805883266, true, 0.5680171805883266, {300, 900}, {303, 903}, {151, 471}, {121, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {61, 41}, 0.0023667382524513608, true, 0.0023667382524513608, {61, 41}, {64, 44}, {32, 42}, {2, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {60, 40}, 0.0, false, 1.0, {482, 462}, {482, 462}, {241, 251}, {211, 0}, true},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {59, 100}, -0.0023667382524513608, false, 1.0, {482, 462}, {482, 462}, {241, 251}, {211, 0}, false},
    {"simple", FigureView::BACK, {-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, {0, 100}, 1.0, false, 1.0, {482, 462}, {482, 462}, {241, 251}, {211, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {760, 560}, 0.09029922884419557, true, 0.09029922884419557, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {1000, 700}, 0.12125896444791977, true, 0.12125896444791977, {1000, 700}, {1003, 703}, {32, 351}, {2, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {300, 900}, 0.030959735603724198, true, 0.030959735603724198, {300, 900}, {303, 903}, {32, 451}, {2, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {61, 41}, 1.2899889834885083E-4, true, 1.2899889834885083E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {60, 40}, 0.0, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, true},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {59, 100}, -1.2899889834885083E-4, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, false},
    {"twoStage", FigureView::SIDE, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {0, 100}, 1.0, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {760, 560}, 0.09029922884419557, true, 0.09029922884419557, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {1000, 700}, 0.12125896444791977, true, 0.12125896444791977, {1000, 700}, {1003, 703}, {32, 351}, {2, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {300, 900}, 0.030959735603724198, true, 0.030959735603724198, {300, 900}, {303, 903}, {32, 451}, {2, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {61, 41}, 1.2899889834885083E-4, true, 1.2899889834885083E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {60, 40}, 0.0, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, true},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {59, 100}, -1.2899889834885083E-4, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, false},
    {"twoStage", FigureView::TOP, {0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, {0, 100}, 1.0, false, 1.0, {7812, 2195}, {7812, 2195}, {30, 1097}, {0, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {760, 560}, 0.2412738809407475, true, 0.2412738809407475, {760, 560}, {763, 563}, {381, 301}, {351, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {1000, 700}, 0.306232233501718, true, 0.306232233501718, {1000, 700}, {1003, 703}, {501, 371}, {471, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {300, 900}, 0.11135717581880654, true, 0.11135717581880654, {300, 900}, {303, 903}, {151, 471}, {121, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {61, 41}, 4.639882325783606E-4, true, 4.639882325783606E-4, {61, 41}, {64, 44}, {32, 42}, {2, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {60, 40}, 0.0, false, 1.0, {2215, 2195}, {2215, 2195}, {1107, 1117}, {1077, 0}, true},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {59, 100}, -4.639882325783606E-4, false, 1.0, {2215, 2195}, {2215, 2195}, {1107, 1117}, {1077, 0}, false},
    {"twoStage", FigureView::BACK, {-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, {0, 100}, 1.0, false, 1.0, {2215, 2195}, {2215, 2195}, {1107, 1117}, {1077, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {760, 560}, 0.18520823332088734, true, 0.18520823332088734, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {1000, 700}, 0.24870819903090588, true, 0.24870819903090588, {1000, 700}, {1003, 703}, {31, 351}, {1, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {300, 900}, 0.06349996571001852, true, 0.06349996571001852, {300, 900}, {303, 903}, {31, 451}, {1, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {61, 41}, 2.645831904584105E-4, true, 2.645831904584105E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {60, 40}, kNaN, false, 1.0, {3839, 40}, {3839, 43}, {30, 21}, {0, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {59, 100}, -2.645831904584105E-4, false, 1.0, {3839, 40}, {3839, 103}, {30, 51}, {0, 0}, false},
    {"empty", FigureView::SIDE, {0.0, -0.0, 1.0, 0.0}, {0, 100}, 1.0, false, 1.0, {3839, 40}, {3839, 103}, {30, 51}, {0, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {760, 560}, 0.18520823332088734, true, 0.18520823332088734, {760, 560}, {763, 563}, {31, 281}, {1, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {1000, 700}, 0.24870819903090588, true, 0.24870819903090588, {1000, 700}, {1003, 703}, {31, 351}, {1, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {300, 900}, 0.06349996571001852, true, 0.06349996571001852, {300, 900}, {303, 903}, {31, 451}, {1, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {61, 41}, 2.645831904584105E-4, true, 2.645831904584105E-4, {61, 41}, {64, 44}, {32, 22}, {2, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {60, 40}, kNaN, false, 1.0, {3839, 40}, {3839, 43}, {30, 21}, {0, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {59, 100}, -2.645831904584105E-4, false, 1.0, {3839, 40}, {3839, 103}, {30, 51}, {0, 0}, false},
    {"empty", FigureView::TOP, {0.0, -0.0, 1.0, 0.0}, {0, 100}, 1.0, false, 1.0, {3839, 40}, {3839, 103}, {30, 51}, {0, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {760, 560}, kInfinity, false, 1.0, {60, 40}, {763, 563}, {381, 301}, {351, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {1000, 700}, kInfinity, false, 1.0, {60, 40}, {1003, 703}, {501, 371}, {471, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {300, 900}, kInfinity, false, 1.0, {60, 40}, {303, 903}, {151, 471}, {121, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {61, 41}, kInfinity, false, 1.0, {60, 40}, {64, 44}, {32, 42}, {2, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {60, 40}, kNaN, false, 1.0, {60, 40}, {63, 43}, {31, 41}, {1, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {59, 100}, -kInfinity, false, 1.0, {60, 40}, {62, 103}, {31, 71}, {1, 0}, false},
    {"empty", FigureView::BACK, {-0.0, -0.0, 0.0, 0.0}, {0, 100}, 1.0, false, 1.0, {60, 40}, {60, 103}, {30, 71}, {0, 0}, false},
});
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

/// EXPECT_EQ, NaN equal to NaN.
void expectSameDouble(double actual, double expected)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << actual;
    }
    else
    {
        EXPECT_EQ(actual, expected);
    }
}

/// The fit of @p row: the fit scale, scaleToFit() and the canvas after it.
void expectFit(TestScaleFigure& figure, const FigureRow& row)
{
    expectSameDouble(figure.fitScale(row.visible), row.fit);
    EXPECT_EQ(figure.scaleToFit(row.visible), row.changed);
    EXPECT_EQ(figure.userScale(), row.user);
    EXPECT_EQ(figure.canvasSize(), row.canvas);
    EXPECT_EQ(figure.sizeHint(), row.canvas);
}

[[nodiscard]] std::string describeRow(const FigureRow& row)
{
    return std::string(row.design) + " " + std::string(figureViewName(row.view)) + " " +
           std::to_string(row.visible.width()) + "x" + std::to_string(row.visible.height()) +
           (row.zeroFitRefused ? " (a zero fit refused)" : "");
}

void expectFigureRow(const FigureRow& row)
{
    SCOPED_TRACE(describeRow(row));
    TestScaleFigure figure(row.subject, row.view, nullptr);
    expectFit(figure, row);

    figure.resize(row.canvas.expandedTo(row.visible + QSize(3, 3)));
    figure.updateFigure();
    EXPECT_EQ(figure.size(), row.size);
    EXPECT_EQ(figure.subjectOrigin(), row.origin);
    EXPECT_EQ(figure.autoZoomPoint(), row.zoomPoint);
}

TEST_F(ScaleFigureTest, FitsAndPlacesTheSubjectAsOpenRocket)
{
    for (const FigureRow& row : kFigureRows)
    {
        expectFigureRow(row);
    }
}

TEST_F(ScaleFigureTest, BaseScaleIsTheResolutionInPixelsPerMetre)
{
    const TestScaleFigure figure(kMetreSubject, FigureView::SIDE, nullptr);
    // OpenRocket's headless value (ScaleProbe): 96 * 39.3701.
    EXPECT_EQ(figure.baseScale(), 3779.5296);
    EXPECT_EQ(figure.baseScale(), TestScaleFigure::kDotsPerInch * ScaleFigure::kInchesPerMeter);
    EXPECT_EQ(figure.userScale(), 1.0);
    EXPECT_EQ(figure.absoluteScale(), figure.baseScale());
    EXPECT_EQ(ScaleFigure::kMetersPerInch, 0.0254);
}

TEST_F(ScaleFigureTest, DotsPerInchFollowsGuiUtilGetDpi)
{
    // The preference in tenths of a dot per inch when at least 10, else the screen's, else 96.
    EXPECT_EQ(ScaleFigure::dotsPerInch(0, 96), 96.0);
    EXPECT_EQ(ScaleFigure::dotsPerInch(955, 72), 95.5);
    EXPECT_EQ(ScaleFigure::dotsPerInch(10, 72), 1.0);
    EXPECT_EQ(ScaleFigure::dotsPerInch(9, 120), 120.0);
    EXPECT_EQ(ScaleFigure::dotsPerInch(-5, 0), 96.0);
    EXPECT_EQ(ScaleFigure::dotsPerInch(0, 0), 96.0);

    InMemoryPreferences preferences;
    preferences.putInt(GuiPreferenceKeys::kDpi, 1200);
    EXPECT_EQ(ScaleFigure::dotsPerInch(preferences), 120.0);
    preferences.putInt(GuiPreferenceKeys::kDpi, 0);
    EXPECT_GE(ScaleFigure::dotsPerInch(preferences), 1.0);
}

TEST_F(ScaleFigureTest, ExplicitScalesAreRefusedAsInOpenRocket)
{
    // ScaleProbe: a new figure, scaleTo(request, 500 x 400) in turn, and whether it changed. The
    // tolerance is newScale * 0.01 relative to newScale: 0.506 is too close to 0.504, 11 to 10.
    // Java accepts 0 (and then 10 from 0); QtRocket refuses it, and 10 follows 2 as well.
    TestScaleFigure  figure(kSimpleSide, FigureView::SIDE, nullptr);
    const QSignalSpy changes(&figure, &ScaleFigure::changed);
    const QSize      visible(500, 400);
    EXPECT_TRUE(figure.scaleTo(0.5, visible));
    EXPECT_TRUE(figure.scaleTo(0.504, visible));
    EXPECT_FALSE(figure.scaleTo(0.506, visible));
    EXPECT_EQ(figure.userScale(), 0.504);
    EXPECT_TRUE(figure.scaleTo(2.0, visible));
    EXPECT_FALSE(figure.scaleTo(kNaN, visible));
    EXPECT_FALSE(figure.scaleTo(kInfinity, visible));
    EXPECT_FALSE(figure.scaleTo(-kInfinity, visible));
    EXPECT_FALSE(figure.scaleTo(-1.0, visible));
    EXPECT_FALSE(figure.scaleTo(0.0, visible));
    // An absolute scale that overflows is refused too.
    EXPECT_FALSE(figure.scaleTo(1e306, visible));
    EXPECT_EQ(figure.userScale(), 2.0);
    EXPECT_TRUE(figure.scaleTo(10.0, visible));
    EXPECT_FALSE(figure.scaleTo(11.0, visible));
    EXPECT_EQ(figure.userScale(), 10.0);
    EXPECT_TRUE(figure.scaleTo(12.0, visible));
    EXPECT_TRUE(figure.scaleTo(1.0, visible));
    EXPECT_EQ(figure.userScale(), 1.0);
    EXPECT_EQ(figure.absoluteScale(), figure.baseScale());
    // One changed() per accepted scale.
    EXPECT_EQ(changes.count(), 6);
}

TEST_F(ScaleFigureTest, TheSameScaleInAnotherVisibleSizeIsAChange)
{
    // ScaleProbe: scaleTo(1, 500 x 400) twice, then scaleTo(1, 501 x 400): true, false, true.
    TestScaleFigure figure(kMetreSubject, FigureView::SIDE, nullptr);
    EXPECT_TRUE(figure.scaleTo(1.0, {500, 400}));
    EXPECT_FALSE(figure.scaleTo(1.0, {500, 400}));
    EXPECT_TRUE(figure.scaleTo(1.0, {501, 400}));
    EXPECT_EQ(figure.visibleBounds(), QSize(501, 400));
}

/// ScaleProbe: at scale 2 for 500 x 400, the preferred size, and the origin after
/// setSize(max(preferred, 503 x 403)).
struct ZoomedRow
{
    Rectangle2D subject;
    FigureView  view;
    QSize       canvas;
    QPoint      origin;
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr auto kZoomedRows = std::to_array<ZoomedRow>({
    {{0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, FigureView::SIDE, {3275, 885}, {30, 442}},
    {{0.0, -0.05589616641898764, 0.42539999999999994, 0.11179233283797528}, FigureView::TOP, {3275, 885}, {30, 442}},
    {{-0.05589616641898764, -0.05589616641898764, 0.11179233283797528, 0.11179233283797528}, FigureView::BACK, {905, 885}, {452, 462}},
    {{0.0, -0.2851184274524962, 2.05105, 0.5702368549049924}, FigureView::SIDE, {15564, 4350}, {30, 2175}},
    {{-0.2851184274524962, -0.2851184274524962, 0.5702368549049924, 0.5702368549049924}, FigureView::BACK, {4370, 4350}, {2185, 2195}},
    {{0.0, -0.0, 1.0, 0.0}, FigureView::SIDE, {7619, 400}, {30, 201}},
    {{-0.0, -0.0, 0.0, 0.0}, FigureView::BACK, {500, 400}, {251, 221}},
});
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

void expectZoomedRow(const ZoomedRow& row)
{
    SCOPED_TRACE(std::string(figureViewName(row.view)) + " " + std::to_string(row.subject.width));
    TestScaleFigure figure(row.subject, row.view, nullptr);
    ASSERT_TRUE(figure.scaleTo(2.0, {500, 400}));
    EXPECT_EQ(figure.canvasSize(), row.canvas);
    figure.resize(row.canvas.expandedTo({503, 403}));
    figure.updateFigure();
    EXPECT_EQ(figure.subjectOrigin(), row.origin);
}

TEST_F(ScaleFigureTest, ZoomedCanvasAndOriginAsOpenRocket)
{
    for (const ZoomedRow& row : kZoomedRows)
    {
        expectZoomedRow(row);
    }
}

TEST_F(ScaleFigureTest, ProjectionMapsMetresToPixelsWithYUp)
{
    TestScaleFigure figure(kSimpleSide, FigureView::SIDE, nullptr);
    ASSERT_TRUE(figure.scaleTo(2.0, {500, 400}));
    figure.resize(figure.canvasSize());
    figure.updateFigure();
    const QPoint origin = figure.subjectOrigin();
    const double scale  = figure.absoluteScale();
    EXPECT_EQ(scale, 2.0 * 3779.5296);
    EXPECT_EQ(figure.projection().map(QPointF(0, 0)), QPointF(origin));
    const QPointF metre = figure.projection().map(QPointF(1, 1));
    EXPECT_DOUBLE_EQ(metre.x(), origin.x() + scale);
    EXPECT_DOUBLE_EQ(metre.y(), origin.y() - scale);
    const QPointF back = figure.toModel(QPointF(origin.x() + (scale / 2), origin.y() + scale));
    EXPECT_DOUBLE_EQ(back.x(), 0.5);
    EXPECT_DOUBLE_EQ(back.y(), -1.0);
}

TEST_F(ScaleFigureTest, ViewOriginCentresTheSubject)
{
    // Side and top views: the subject centred horizontally from its left edge, the axis at half
    // the height (at least the subject's plus the border); back view: the axis in the middle,
    // 20 pixels down.
    const Rectangle2D subject{.x = -0.1, .y = -0.05, .width = 0.4, .height = 0.1};
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::SIDE, {600, 300}, subject, 1000),
              QPoint(200, 150));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::TOP, {600, 100}, subject, 1000), QPoint(200, 70));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::SIDE, {300, 100}, subject, 1000),
              QPoint(100, 70));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::BACK, {600, 300}, subject, 1000),
              QPoint(300, 170));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::BACK, {300, 301}, subject, 1000),
              QPoint(200, 170));
}

TEST_F(ScaleFigureTest, AbsurdScalesSaturateInsteadOfOverflowing)
{
    const Rectangle2D subject{.x = -1, .y = -1, .width = 2, .height = 2};
    EXPECT_EQ(ScaleFigure::canvasSizeFor({10, 10}, subject, 1e300),
              QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    EXPECT_EQ(ScaleFigure::canvasSizeFor({10, 10}, subject, kNaN), QSize(60, 40));
    constexpr int kIntMax = std::numeric_limits<int>::max();
    // Java's int arithmetic would wrap: (INT_MAX + 40) / 2 and INT_MAX / 2 - INT_MAX / 2 + 2^31.
    // The subject counts as large as the largest canvas, QWIDGETSIZE_MAX less the border
    // (16777155 x 16777175 pixels); the origin's x is -(int)(-1e300) = 2^31, saturated.
    constexpr int kLargestSubjectWidth  = QWIDGETSIZE_MAX - 60;
    constexpr int kLargestSubjectHeight = QWIDGETSIZE_MAX - 40;
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::SIDE, {100, 100}, subject, 1e300),
              QPoint(kIntMax, (kLargestSubjectHeight + 40) / 2));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::BACK, {100, 100}, subject, 1e300),
              QPoint(kLargestSubjectWidth / 2, 70));
}

TEST_F(ScaleFigureTest, TheOriginOfAnAbsurdlyLargeSubjectStaysInsideTheCanvas)
{
    // A design whose radius or fin is absurdly large, at the user scale 1 (3779.5296 pixels per
    // metre): the canvas is QWIDGETSIZE_MAX high, and the axis is in its middle, where Java's
    // int arithmetic and an unbounded subject would put it at (INT_MAX + 40) / 2, far below it.
    const Rectangle2D tall{.x = 0, .y = -5e299, .width = 1, .height = 1e300};
    const double      scale  = 3779.5296;
    const QSize       canvas = ScaleFigure::canvasSizeFor({764, 564}, tall, scale);
    EXPECT_EQ(canvas, QSize(3839, QWIDGETSIZE_MAX));
    const QPoint origin = ScaleFigure::viewOrigin(FigureView::SIDE, canvas, tall, scale);
    EXPECT_EQ(origin, QPoint(30, QWIDGETSIZE_MAX / 2));
    EXPECT_LT(origin.y(), canvas.height());

    // As wide: the back view's axis in the middle of the canvas, too.
    const Rectangle2D wide{.x = -5e299, .y = -5e299, .width = 1e300, .height = 1e300};
    const QSize       square = ScaleFigure::canvasSizeFor({764, 564}, wide, scale);
    EXPECT_EQ(square, QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    EXPECT_EQ(ScaleFigure::viewOrigin(FigureView::BACK, square, wide, scale),
              QPoint(QWIDGETSIZE_MAX / 2, 20 + (QWIDGETSIZE_MAX / 2)));
}

TEST_F(ScaleFigureTest, TheCurrentScaleIsNoChangeEvenWhereJavasToleranceUnderflows)
{
    // Java's MathUtil.equals(user, new, new * 0.01) compares |user - new| with
    // new * 0.01 * |new|, which is 0 below about 1e-161: an identical scale would be a change, and
    // a fitting scroll area would fit again without end (ScaleScrollAreaTests). 1e-150 is above
    // that limit, the others below it.
    TestScaleFigure  figure(kMetreSubject, FigureView::SIDE, nullptr);
    const QSignalSpy changed(&figure, &ScaleFigure::changed);
    for (const double tiny : {1e-150, 1e-170, 1e-201, 1e-300})
    {
        SCOPED_TRACE(tiny);
        EXPECT_TRUE(figure.scaleTo(tiny, {760, 560}));
        EXPECT_EQ(figure.userScale(), tiny);
        EXPECT_FALSE(figure.scaleTo(tiny, {760, 560}));
    }
    EXPECT_EQ(changed.count(), 4);
}

TEST_F(ScaleFigureTest, FitScaleOfAnEmptyAreaIsTheUserScale)
{
    TestScaleFigure figure(kMetreSubject, FigureView::SIDE, nullptr);
    ASSERT_TRUE(figure.scaleTo(0.5, {100, 100}));
    EXPECT_EQ(figure.fitScale({0, 100}), 0.5);
    EXPECT_EQ(figure.fitScale({100, -1}), 0.5);
    EXPECT_FALSE(figure.scaleToFit({0, 0}));
}

TEST_F(ScaleFigureTest, FitScaleBringsTheSubjectUpToDate)
{
    TestScaleFigure figure(kMetreSubject, FigureView::SIDE, nullptr);
    figure.setSubjectQuietly(kTwoMetreSubject);
    EXPECT_EQ(figure.fitScale({660, 1000}), 600 / (2 * 3779.5296));
    EXPECT_EQ(figure.subjectBounds(), kTwoMetreSubject);
}

TEST_F(ScaleFigureTest, BackgroundIsTheThemeOrTheCustomColour)
{
    TestScaleFigure figure(kMetreSubject, FigureView::SIDE, nullptr);
    EXPECT_EQ(figure.backgroundColor(), ThemeColors::color(ThemeColors::Key::BACKGROUND));
    figure.setCustomBackgroundColor(QColor(10, 20, 30));
    EXPECT_EQ(figure.backgroundColor(), QColor(10, 20, 30));
    figure.setCustomBackgroundColor(QColor());
    EXPECT_EQ(figure.backgroundColor(), ThemeColors::color(ThemeColors::Key::BACKGROUND));
    // It paints with no painting of its own.
    EXPECT_FALSE(figure.grab().toImage().isNull());
}

TEST_F(ScaleFigureTest, ResizeUpdatesTheOriginAndSignals)
{
    TestScaleFigure figure(kMetreSubject, FigureView::BACK, nullptr);
    ASSERT_TRUE(figure.scaleTo(0.1, {800, 600}));
    ASSERT_EQ(figure.canvasSize(), QSize(800, 600));
    const QSignalSpy resized(&figure, &ScaleFigure::resized);
    figure.show();
    figure.resize(800, 600);
    EXPECT_GE(resized.count(), 1);
    EXPECT_EQ(figure.subjectOrigin(), QPoint(400, 320));
    figure.resize(900, 700);
    EXPECT_EQ(figure.subjectOrigin(), QPoint(450, 370));
    figure.hide();
}

}  // namespace

}  // namespace QtRocket::Gui
