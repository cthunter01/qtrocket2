#include "QtRocket/gui/plot/ThrustCurvePlot.h"

#include <algorithm>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPalette>
#include <QPoint>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <Qt>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/Theme.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/PlotExport.h"
#include "QtRocket/gui/plot/PlotInputBindings.h"
#include "QtRocket/gui/plot/PlotTheme.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "ScopedApplicationPalette.h"
#include "TestGuiServices.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "motor/TestMotorDatabase.h"
#include "plot/PlotReadback.h"
#include "unit/DefaultUnitsGuard.h"
#include "widgets/WidgetTestSupport.h"

// OpenRocket has no JUnit test of its thrust-curve charts. The expected values are what its own
// classes show (MotorInformationPanel's preview and ThrustCurveMotorPlotDialog's chart, dumped
// through JFreeChart's API under JDK 17 by the M3 scouting's ThrustCurveChartProbe): the series'
// keys, paints and strokes, and the axes' ranges as Double.toHexString() gives them, for the
// Estes C6 (two curves), Klima A6 (one) and AeroTech G80T (four, the most of any motor set) of the
// bundled database, each curve list in the probe's order (ThrustCurveMotorComparator's), which the
// tests build from the curves' digests.

namespace QtRocket::Gui
{

namespace
{

using ThrustCurvePlotTest = Gui::Test::GuiTest;
using Curves              = std::vector<std::shared_ptr<const ThrustCurveMotor>>;
using Palette             = Gui::Test::ScopedApplicationPalette;

/// OpenRocket's ranges (Double.toHexString() of its axes' upper bounds; the lower ones are 0).
constexpr double kEstesC6TimeMax     = 0x1.10068db8bac71p1;   // 2.1252
constexpr double kEstesC6ThrustMax   = 0x1.d96c8b439581p3;    // 14.7945
constexpr double kKlimaA6TimeMax     = 0x1.7db22d0e56041p-1;  // 0.7454999999999999
constexpr double kKlimaA6ThrustMax   = 0x1.6006f69446738p3;   // 11.00085
constexpr double kG80TTimeMax        = 0x1.e5f6a93f290acp0;   // 1.898295
constexpr double kG80TThrustMax      = 0x1.d1323a29c779ap6;   // 116.29905
constexpr double kEmptyRangeMax      = 0x1.0cccccccccccdp0;   // 1.05
constexpr int    kPlotWidth          = 600;
constexpr int    kPlotHeight         = 400;
constexpr double kPeakThrust         = 14.0;
constexpr double kOtherPeakThrust    = 12.0;
constexpr QRgb   kUserColor          = 0xff123456;
constexpr QRgb   kDarkBase           = 0xff202020;
constexpr double kInfiniteZeroShare  = std::numeric_limits<double>::infinity();
constexpr double kLbfPerNewtonFactor = 4.44822162;

/// ThrustCurveMotorSelectionPanel.getColor(0) to getColor(3) in OpenRocket's light and dark
/// themes.
constexpr QRgb kLight0 = 0xff0072bd;
constexpr QRgb kLight1 = 0xffd95319;
constexpr QRgb kLight2 = 0xffedb120;
constexpr QRgb kLight3 = 0xff7e318e;
constexpr QRgb kDark0  = 0xff00e7ff;
constexpr QRgb kDark1  = 0xffffa832;

/// What the tests compare of a curve: its name (as given to rocketplot::literalText()), colour,
/// line width and marker.
struct CurveLook
{
    QString            name;
    QRgb               color     = 0;
    double             lineWidth = 0.0;
    rocketplot::Marker marker    = rocketplot::Marker::NONE;

    friend bool operator==(const CurveLook&, const CurveLook&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const CurveLook& look, std::ostream* out)
{
    *out << std::format("{{{} #{:08x} {} marker {}}}", QtStrings::toUtf8(look.name), look.color,
                        look.lineWidth, static_cast<int>(look.marker));
}

/// The looks of @p plot's series in its order.
[[nodiscard]] std::vector<CurveLook> looksOf(const ThrustCurvePlot& plot)
{
    std::vector<CurveLook> looks;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot.plotWidget()))
    {
        looks.push_back({.name      = series.name,
                         .color     = series.color.rgba(),
                         .lineWidth = series.lineWidth,
                         .marker    = series.marker});
    }
    return looks;
}

/// A curve's look as the test expects it: @p name through literalText().
[[nodiscard]] CurveLook look(const QString& name, QRgb color, double lineWidth,
                             rocketplot::Marker marker = rocketplot::Marker::NONE)
{
    return {.name      = rocketplot::literalText(name),
            .color     = color,
            .lineWidth = lineWidth,
            .marker    = marker};
}

/// An axis fitting the data from 0 to @p max as JFreeChart's NumberAxis, labelled @p label.
[[nodiscard]] Gui::Test::AxisReadback fittedAxis(double max, const QString& label = {})
{
    return {.label                 = label.isEmpty() ? QString() : rocketplot::literalText(label),
            .min                   = 0.0,
            .max                   = max,
            .autoscale             = true,
            .autoscaleMargin       = ThrustCurvePlot::kAxisMargin,
            .autoscaleIncludesZero = kInfiniteZeroShare,
            .gridVisible           = true,
            .tickLabelsVisible     = true};
}

/// The axis of a chart without curves: JFreeChart's [0, 1.05], not autoscaling.
[[nodiscard]] Gui::Test::AxisReadback emptyAxis(const QString& label = {})
{
    Gui::Test::AxisReadback axis = fittedAxis(kEmptyRangeMax, label);
    axis.autoscale               = false;
    return axis;
}

/// @p motors in the order of @p digests, each found by its digest (a test failure for one that
/// is missing or a curve that is left over).
[[nodiscard]] Curves inDigestOrder(const Curves& motors, const std::vector<std::string>& digests)
{
    Curves ordered;
    for (const std::string& digest : digests)
    {
        const auto found = std::ranges::find_if(
            motors, [&digest](const auto& motor) { return motor->getDigest() == digest; });
        if (found == motors.end())
        {
            ADD_FAILURE() << "no curve with the digest " << digest;
            continue;
        }
        ordered.push_back(*found);
    }
    EXPECT_EQ(ordered.size(), motors.size());
    return ordered;
}

/// The curves of the bundled database's motor set of @p manufacturer (its display name) and
/// @p designation, in the order of @p digests.
[[nodiscard]] Curves bundledCurves(std::string_view manufacturer, std::string_view designation,
                                   const std::vector<std::string>& digests)
{
    for (const ThrustCurveMotorSet& set : QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        if (set.getManufacturer() != nullptr &&
            set.getManufacturer()->getDisplayName() == manufacturer &&
            set.getDesignation() == designation)
        {
            return inDigestOrder(set.getMotors(), digests);
        }
    }
    ADD_FAILURE() << "no motor set " << manufacturer << ' ' << designation;
    return {};
}

/// The Estes C6's two curves, the AeroTech G80T's four and the Klima A6's one, in the probe's
/// order.
[[nodiscard]] Curves estesC6()
{
    return bundledCurves("Estes", "C6",
                         {"2967cd7a160b396ef96f09695429d8e9", "9b06acb7d875d299f0784b0aec86451f"});
}

[[nodiscard]] Curves aeroTechG80T()
{
    return bundledCurves("AeroTech", "G80T",
                         {"6438359c302fd7c031af855f17d8d27e", "7615c7e3ae9e0072e1fd1ffbf823abc0",
                          "3d8d52cbd5a4774fdc9380846aeace3d", "11451b10e5228e503f764de829fe9399"});
}

[[nodiscard]] Curves klimaA6()
{
    return bundledCurves("Klima", "A6", {"418fa5ae09fdb53a19cd751fe289ba5b"});
}

/// Two Estes "C6" curves of three points each, 1 s long, peaking at 14 N and 12 N.
[[nodiscard]] Curves testCurves()
{
    return {QtRocket::Test::makeEmbeddedTestMotor("C6", kPeakThrust, "first"),
            QtRocket::Test::makeEmbeddedTestMotor("C6", kOtherPeakThrust, "second")};
}

/// The data of @p plot's series in its order (x and y), and of @p motors in the order expected.
struct CurveData
{
    std::vector<double> x;
    std::vector<double> y;

    friend bool operator==(const CurveData&, const CurveData&) = default;
};

[[nodiscard]] std::vector<CurveData> dataOf(const ThrustCurvePlot& plot)
{
    std::vector<CurveData> data;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot.plotWidget()))
    {
        data.push_back({.x = series.x, .y = series.y});
    }
    return data;
}

[[nodiscard]] CurveData dataOf(const ThrustCurveMotor& motor)
{
    return {.x = motor.getTimePoints(), .y = motor.getThrustPoints()};
}

/// The context menu of @p plot's plot area, as a right click in its middle opens it (null for
/// none); the caller closes it.
[[nodiscard]] QMenu* openContextMenu(rocketplot::PlotWidget& plot)
{
    const QPoint      middle = plot.plotArea().center().toPoint();
    QContextMenuEvent event(QContextMenuEvent::Mouse, middle, plot.mapToGlobal(middle));
    QApplication::sendEvent(&plot, &event);
    return qobject_cast<QMenu*>(QApplication::activePopupWidget());
}

/// A marker as @p plot's SVG export draws it: the side of a square path (Gui::Test::
/// centredSquareSide()) and its fill.
struct SquareMarker
{
    double  side = 0.0;
    QString fill;

    friend bool operator==(const SquareMarker&, const SquareMarker&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const SquareMarker& marker, std::ostream* out)
{
    *out << std::format("{{side {} fill {}}}", marker.side, QtStrings::toUtf8(marker.fill));
}

/// The square paths of @p plot's SVG export (its square markers, the legend's too), in order,
/// after painting @p plot (QWidget::grab(), which lays out the hidden widgets around it, so that
/// the plot has its size).
[[nodiscard]] std::vector<SquareMarker> squareMarkers(rocketplot::PlotWidget& plot)
{
    static_cast<void>(plot.grab());
    std::vector<SquareMarker> squares;
    for (const Gui::Test::SvgPath& path : Gui::Test::svgPaths(plot))
    {
        if (const std::optional<double> side = Gui::Test::centredSquareSide(path))
        {
            squares.push_back({.side = *side, .fill = path.fill});
        }
    }
    return squares;
}

/// The application palette of a light window around dark views: ThemeColors calls it light (dark
/// text on a light window), rocketplot's ThemeMode::SYSTEM would call it dark (its Base).
[[nodiscard]] QPalette lightWindowDarkBase()
{
    QPalette palette = Palette::light();
    palette.setColor(QPalette::Base, QColor::fromRgb(kDarkBase));
    return palette;
}

TEST_F(ThrustCurvePlotTest, PreviewShowsTheEstesC6AsOpenRocket)
{
    const Curves    motors = estesC6();
    ThrustCurvePlot plot;
    plot.setCurves(motors, 0);

    const Gui::Test::PlotReadback read = Gui::Test::readPlot(plot.plotWidget());
    EXPECT_EQ(read.title, rocketplot::literalText(QStringLiteral("Thrust curve:")));
    EXPECT_EQ(read.xAxis, fittedAxis(kEstesC6TimeMax));
    EXPECT_EQ(read.yAxis, fittedAxis(kEstesC6ThrustMax));
    // Added from the last curve, so that "Thrust (0)" is drawn on top.
    EXPECT_EQ(looksOf(plot), (std::vector<CurveLook>{
                                 look(QStringLiteral("Thrust (1)"), kLight1, 1.0),
                                 look(QStringLiteral("Thrust (0)"), kLight0, 3.0),
                             }));
    ASSERT_EQ(motors.size(), 2U);
    EXPECT_EQ(dataOf(plot), (std::vector<CurveData>{dataOf(*motors[1]), dataOf(*motors[0])}));
    EXPECT_FALSE(plot.plotWidget().legend()->isVisible());
    EXPECT_EQ(plot.chartStyle(), ThrustCurvePlot::Style::PREVIEW);
    EXPECT_EQ(plot.curveCount(), 2);
    EXPECT_EQ(plot.selectedCurve(), 0);
}

TEST_F(ThrustCurvePlotTest, PreviewShowsTheSelectionOfTheAeroTechG80TsLastCurve)
{
    const Curves    motors = aeroTechG80T();
    ThrustCurvePlot plot;
    plot.setCurves(motors, 3);

    EXPECT_EQ(Gui::Test::readAxis(*plot.plotWidget().xAxis()), fittedAxis(kG80TTimeMax));
    EXPECT_EQ(Gui::Test::readAxis(*plot.plotWidget().yAxis()), fittedAxis(kG80TThrustMax));
    EXPECT_EQ(looksOf(plot), (std::vector<CurveLook>{
                                 look(QStringLiteral("Thrust (3)"), kLight3, 3.0),
                                 look(QStringLiteral("Thrust (2)"), kLight2, 1.0),
                                 look(QStringLiteral("Thrust (1)"), kLight1, 1.0),
                                 look(QStringLiteral("Thrust (0)"), kLight0, 1.0),
                             }));
    EXPECT_EQ(plot.selectedCurve(), 3);
}

TEST_F(ThrustCurvePlotTest, DialogShowsTheEstesC6AsOpenRocket)
{
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = estesC6();
    ThrustCurvePlot                  plot(services.services());
    plot.setCurves(motors, 0);

    const rocketplot::PlotWidget& widget = plot.plotWidget();
    EXPECT_EQ(widget.title(), rocketplot::literalText(QStringLiteral("Motor thrust curves")));
    EXPECT_EQ(Gui::Test::readAxis(*widget.xAxis()),
              fittedAxis(kEstesC6TimeMax, QStringLiteral("Time / s")));
    EXPECT_EQ(Gui::Test::readAxis(*widget.yAxis()),
              fittedAxis(kEstesC6ThrustMax, QStringLiteral("Thrust / N")));
    // The selected curve first, then the others numbered from 1.
    EXPECT_EQ(looksOf(plot),
              (std::vector<CurveLook>{
                  look(QStringLiteral("Estes C6"), kLight0, 1.5, rocketplot::Marker::SQUARE),
                  look(QStringLiteral("Estes C6 (1)"), kLight1, 1.5),
              }));
    ASSERT_EQ(motors.size(), 2U);
    EXPECT_EQ(dataOf(plot), (std::vector<CurveData>{dataOf(*motors[0]), dataOf(*motors[1])}));
    const Gui::Test::SeriesReadback selected = Gui::Test::readSeries(*widget.series().front());
    EXPECT_EQ(selected.markerSize, ThrustCurvePlot::kDialogMarkerSize);
    EXPECT_EQ(selected.markerRingWidth, 0.0);
    EXPECT_TRUE(selected.markersAlwaysVisible);
    EXPECT_TRUE(widget.legend()->isVisible());
    EXPECT_EQ(widget.legend()->anchor(), rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
    EXPECT_EQ(plot.chartStyle(), ThrustCurvePlot::Style::DIALOG);
}

TEST_F(ThrustCurvePlotTest, DialogPutsTheAeroTechG80TsSelectedCurveFirstInItsOwnColour)
{
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = aeroTechG80T();
    ThrustCurvePlot                  plot(services.services());
    plot.setCurves(motors, 3);

    EXPECT_EQ(Gui::Test::readAxis(*plot.plotWidget().xAxis()),
              fittedAxis(kG80TTimeMax, QStringLiteral("Time / s")));
    EXPECT_EQ(Gui::Test::readAxis(*plot.plotWidget().yAxis()),
              fittedAxis(kG80TThrustMax, QStringLiteral("Thrust / N")));
    EXPECT_EQ(looksOf(plot),
              (std::vector<CurveLook>{
                  look(QStringLiteral("AeroTech G80T"), kLight3, 1.5, rocketplot::Marker::SQUARE),
                  look(QStringLiteral("AeroTech G80T (1)"), kLight0, 1.5),
                  look(QStringLiteral("AeroTech G80T (2)"), kLight1, 1.5),
                  look(QStringLiteral("AeroTech G80T (3)"), kLight2, 1.5),
              }));

    // The first curve selected: the others in their order.
    plot.setCurves(motors, 0);
    EXPECT_EQ(looksOf(plot),
              (std::vector<CurveLook>{
                  look(QStringLiteral("AeroTech G80T"), kLight0, 1.5, rocketplot::Marker::SQUARE),
                  look(QStringLiteral("AeroTech G80T (1)"), kLight1, 1.5),
                  look(QStringLiteral("AeroTech G80T (2)"), kLight2, 1.5),
                  look(QStringLiteral("AeroTech G80T (3)"), kLight3, 1.5),
              }));
    EXPECT_EQ(Gui::Test::readAxis(*plot.plotWidget().yAxis()),
              fittedAxis(kG80TThrustMax, QStringLiteral("Thrust / N")));
}

TEST_F(ThrustCurvePlotTest, BothChartsOfTheKlimaA6HaveOpenRocketsRanges)
{
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = klimaA6();
    ThrustCurvePlot                  preview;
    ThrustCurvePlot                  dialog(services.services());
    preview.setCurves(motors, 0);
    dialog.setCurves(motors, 0);

    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().xAxis()), fittedAxis(kKlimaA6TimeMax));
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().yAxis()), fittedAxis(kKlimaA6ThrustMax));
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().xAxis()),
              fittedAxis(kKlimaA6TimeMax, QStringLiteral("Time / s")));
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().yAxis()),
              fittedAxis(kKlimaA6ThrustMax, QStringLiteral("Thrust / N")));
    EXPECT_EQ(looksOf(preview),
              (std::vector<CurveLook>{look(QStringLiteral("Thrust (0)"), kLight0, 3.0)}));
    EXPECT_EQ(looksOf(dialog), (std::vector<CurveLook>{look(QStringLiteral("Klima A6"), kLight0,
                                                            1.5, rocketplot::Marker::SQUARE)}));
    // JFreeChart shows the legend for one curve too.
    EXPECT_TRUE(dialog.plotWidget().legend()->isVisible());
}

TEST_F(ThrustCurvePlotTest, ChartsInADarkPaletteShowOpenRocketsDarkColours)
{
    const Palette                    dark(Palette::dark());
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = testCurves();
    ThrustCurvePlot                  preview;
    ThrustCurvePlot                  dialog(services.services());
    preview.setCurves(motors, 0);
    dialog.setCurves(motors, 0);

    EXPECT_EQ(looksOf(preview), (std::vector<CurveLook>{
                                    look(QStringLiteral("Thrust (1)"), kDark1, 1.0),
                                    look(QStringLiteral("Thrust (0)"), kDark0, 3.0),
                                }));
    EXPECT_EQ(looksOf(dialog),
              (std::vector<CurveLook>{
                  look(QStringLiteral("Estes C6"), kDark0, 1.5, rocketplot::Marker::SQUARE),
                  look(QStringLiteral("Estes C6 (1)"), kDark1, 1.5),
              }));
}

TEST_F(ThrustCurvePlotTest, CurvesChangeTheirColoursWithThePalette)
{
    const Palette                    light(Palette::light());
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(testCurves(), 1);
    const std::vector<CurveLook> lightLooks{
        look(QStringLiteral("Estes C6"), kLight1, 1.5, rocketplot::Marker::SQUARE),
        look(QStringLiteral("Estes C6 (1)"), kLight0, 1.5),
    };
    ASSERT_EQ(looksOf(dialog), lightLooks);
    {
        const Palette dark(Palette::dark());
        // Qt hands a new application palette to the widgets through posted events.
        QCoreApplication::sendPostedEvents();
        EXPECT_EQ(looksOf(dialog),
                  (std::vector<CurveLook>{
                      look(QStringLiteral("Estes C6"), kDark1, 1.5, rocketplot::Marker::SQUARE),
                      look(QStringLiteral("Estes C6 (1)"), kDark0, 1.5),
                  }));
    }
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(looksOf(dialog), lightLooks);
}

TEST_F(ThrustCurvePlotTest, ACurveTheUserRecolouredOrRemovedIsLeftAloneByAPaletteChange)
{
    const Palette   light(Palette::light());
    ThrustCurvePlot preview;
    preview.setCurves(testCurves(), 0);
    const QList<rocketplot::Series*> series = preview.plotWidget().series();
    ASSERT_EQ(series.size(), 2);
    // "Thrust (1)" recoloured, "Thrust (0)" removed, as the legend's menu does (Theme color,
    // Remove).
    series.front()->setColor(QColor::fromRgba(kUserColor));
    preview.plotWidget().removeSeries(series.back());

    const Palette dark(Palette::dark());
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(looksOf(preview),
              (std::vector<CurveLook>{look(QStringLiteral("Thrust (1)"), kUserColor, 1.0)}));
    EXPECT_EQ(preview.curveCount(), 2);
}

TEST_F(ThrustCurvePlotTest, DialogPlotsTheThrustInTheUnitItsAxisNames)
{
    // OpenRocket labels the axis "Thrust / lbf" and plots newtons (bug 12, fixed).
    const QtRocket::Test::DefaultUnitsGuard units;
    ASSERT_TRUE(unitGroup(UnitGroupId::FORCE).setDefaultUnit("lbf"));
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = testCurves();
    ThrustCurvePlot                  dialog(services.services());
    ThrustCurvePlot                  preview;
    dialog.setCurves(motors, 0);
    preview.setCurves(motors, 0);

    const Unit&               lbf = unitGroup(UnitGroupId::FORCE).getDefaultUnit();
    const std::vector<double> lbfThrust{lbf.toUnit(0.0), lbf.toUnit(kPeakThrust), lbf.toUnit(0.0)};
    EXPECT_EQ(lbf.toUnit(kLbfPerNewtonFactor), 1.0);
    ASSERT_EQ(dataOf(dialog).size(), 2U);
    EXPECT_EQ(dataOf(dialog).front(), (CurveData{.x = motors[0]->getTimePoints(), .y = lbfThrust}));
    // The fit of rocketplot (and of JFreeChart): the span from 0 and 5 % of it.
    const double maxThrust = lbf.toUnit(kPeakThrust);
    const double pad       = maxThrust * ThrustCurvePlot::kAxisMargin;
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().yAxis()),
              fittedAxis(maxThrust + pad, QStringLiteral("Thrust / lbf")));
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().xAxis()),
              fittedAxis(kEmptyRangeMax, QStringLiteral("Time / s")));
    // The preview has no axis label, and plots newtons as OpenRocket's.
    EXPECT_EQ(dataOf(preview).back(), dataOf(*motors[0]));
}

TEST_F(ThrustCurvePlotTest, ChartsWithoutCurvesShowJFreeChartsDefaultRange)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  preview;
    const ThrustCurvePlot            dialog(services.services());
    EXPECT_EQ(ThrustCurvePlot::kEmptyRangeMax, kEmptyRangeMax);
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().xAxis()), emptyAxis());
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().yAxis()), emptyAxis());
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().xAxis()),
              emptyAxis(QStringLiteral("Time / s")));
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().yAxis()),
              emptyAxis(QStringLiteral("Thrust / N")));
    EXPECT_TRUE(preview.plotWidget().series().isEmpty());
    EXPECT_EQ(preview.curveCount(), 0);
    EXPECT_EQ(preview.selectedCurve(), -1);

    // clearData(), and the curves again: autoscale is back on.
    preview.setCurves(testCurves(), 1);
    preview.clear();
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().xAxis()), emptyAxis());
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().yAxis()), emptyAxis());
    EXPECT_TRUE(preview.plotWidget().series().isEmpty());
    EXPECT_EQ(preview.curveCount(), 0);
    EXPECT_EQ(preview.selectedCurve(), -1);
    preview.setCurves(testCurves(), 1);
    const double pad = kPeakThrust * ThrustCurvePlot::kAxisMargin;
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().xAxis()), fittedAxis(kEmptyRangeMax));
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().yAxis()), fittedAxis(kPeakThrust + pad));
}

TEST_F(ThrustCurvePlotTest, ANullMotorOrASelectionOutsideTheCurvesIsABug)
{
    ThrustCurvePlot plot;
    const Curves    motors = testCurves();
    EXPECT_THROW(plot.setCurves(motors, 2), BugError);
    EXPECT_THROW(plot.setCurves(motors, -2), BugError);
    const Curves withNull{motors.front(), nullptr};
    EXPECT_THROW(plot.setCurves(withNull, 0), BugError);
    // No selection: every curve as an unselected one, the dialog's numbered from 1 (Java's n).
    plot.setCurves(motors, -1);
    EXPECT_EQ(looksOf(plot), (std::vector<CurveLook>{
                                 look(QStringLiteral("Thrust (1)"), kLight1, 1.0),
                                 look(QStringLiteral("Thrust (0)"), kLight0, 1.0),
                             }));
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(motors, -1);
    EXPECT_EQ(looksOf(dialog), (std::vector<CurveLook>{
                                   look(QStringLiteral("Estes C6 (1)"), kLight0, 1.5),
                                   look(QStringLiteral("Estes C6 (2)"), kLight1, 1.5),
                               }));
}

TEST_F(ThrustCurvePlotTest, PreviewTakesNoGestureAndHasNoContextMenu)
{
    const ThrustCurvePlot         preview;
    const rocketplot::PlotWidget& widget = preview.plotWidget();
    EXPECT_EQ(widget.inputBindings(), PlotInputBindings::none());
    EXPECT_EQ(widget.contextMenuPolicy(), Qt::NoContextMenu);
    EXPECT_FALSE(widget.legend()->isInteractive());
    EXPECT_FALSE(widget.isCrosshairEnabled());
    // The plot leaves its menu to its parent, which shows none and passes none on.
    EXPECT_EQ(preview.contextMenuPolicy(), Qt::PreventContextMenu);
    EXPECT_EQ(preview.objectName(), QStringLiteral("chartPanel"));
    EXPECT_EQ(widget.objectName(), QStringLiteral("chart"));
    // The theme of the curve colours' rule (PlotTheme), not rocketplot's SYSTEM rule.
    EXPECT_EQ(widget.themeMode(), rocketplot::ThemeMode::CUSTOM);
    EXPECT_TRUE(widget.theme() == PlotTheme::forPalette(preview.palette()));
}

TEST_F(ThrustCurvePlotTest, DialogZoomsAsAChartPanelAndExportsThroughQtRocketsPrompts)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(testCurves(), 0);
    dialog.resize(kPlotWidth, kPlotHeight);
    rocketplot::PlotWidget& widget = dialog.plotWidget();
    EXPECT_EQ(widget.inputBindings(), PlotInputBindings::chartPanel());
    EXPECT_FALSE(widget.isCrosshairEnabled());
    EXPECT_EQ(widget.crosshairMode(), rocketplot::CrosshairMode::SNAP);
    EXPECT_EQ(widget.themeMode(), rocketplot::ThemeMode::CUSTOM);
    EXPECT_TRUE(widget.theme() == PlotTheme::forPalette(dialog.palette()));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("chartPanel"));

    const Gui::Test::OffscreenPopupWarnings warnings;
    QMenu* const                            menu = openContextMenu(widget);
    ASSERT_NE(menu, nullptr);
    const QList<QAction*> exports = menu->findChildren<QAction*>(PlotExport::exportActionName());
    menu->close();
    EXPECT_EQ(exports.size(), 1);
}

TEST_F(ThrustCurvePlotTest, DialogsLegendListsTheSelectedCurveFirst)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(testCurves(), 1);
    dialog.resize(kPlotWidth, kPlotHeight);
    EXPECT_EQ(Gui::Test::readPlotTexts(dialog.plotWidget()).legendEntries,
              (QStringList{QStringLiteral("Estes C6"), QStringLiteral("Estes C6 (1)")}));
}

TEST_F(ThrustCurvePlotTest, NamesShowTheirCharactersAsTheyAre)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    const Curves motors{QtRocket::Test::makeEmbeddedTestMotor("<b>$5 & A6", kPeakThrust, "x")};
    dialog.setCurves(motors, 0);
    EXPECT_EQ(looksOf(dialog),
              (std::vector<CurveLook>{look(QStringLiteral("Estes <b>$5 & A6"), kLight0, 1.5,
                                           rocketplot::Marker::SQUARE)}));
    dialog.resize(kPlotWidth, kPlotHeight);
    EXPECT_EQ(Gui::Test::readPlotTexts(dialog.plotWidget()).legendEntries,
              (QStringList{QStringLiteral("Estes <b>$5 & A6")}));
}

TEST_F(ThrustCurvePlotTest, DialogDrawsTheSelectedCurvesMarkersAsJFreeChartsSixPixelSquares)
{
    // JFreeChart's Rectangle2D.Double(-3, -3, 6, 6) at every point of the selected curve; the
    // legend's icon draws one too. rocketplot shrinks a SQUARE to 0.85 of its size, so the size
    // is the side over that (a rocketplot upgrade that changes the scale fails here).
    const Gui::Test::TestGuiServices services;
    const Curves                     motors = estesC6();
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(motors, 1);
    dialog.resize(kPlotWidth, kPlotHeight);
    ASSERT_EQ(motors.size(), 2U);
    const QString fill = QColor::fromRgba(kLight1).name();
    EXPECT_EQ(squareMarkers(dialog.plotWidget()),
              std::vector<SquareMarker>(motors[1]->getTimePoints().size() + 1,
                                        SquareMarker{.side = 6.0, .fill = fill}));
    EXPECT_EQ(ThrustCurvePlot::kDialogMarkerSide, 6.0);
    EXPECT_EQ(ThrustCurvePlot::kDialogMarkerSize * ThrustCurvePlot::kRocketplotSquareScale, 6.0);
    // The preview draws no marker.
    ThrustCurvePlot preview;
    preview.setCurves(motors, 1);
    preview.resize(kPlotWidth, kPlotHeight);
    EXPECT_TRUE(squareMarkers(preview.plotWidget()).empty());
}

TEST_F(ThrustCurvePlotTest, NoCurvesGiveTheChartOfClear)
{
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  preview;
    ThrustCurvePlot                  dialog(services.services());
    preview.setCurves(testCurves(), 0);
    dialog.setCurves(testCurves(), 0);
    // JFreeChart's empty dataset, [0, 1.05] (rocketplot autoscales no data to [0, 1]).
    preview.setCurves({}, -1);
    dialog.setCurves({}, -1);
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().xAxis()), emptyAxis());
    EXPECT_EQ(Gui::Test::readAxis(*preview.plotWidget().yAxis()), emptyAxis());
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().xAxis()),
              emptyAxis(QStringLiteral("Time / s")));
    EXPECT_EQ(Gui::Test::readAxis(*dialog.plotWidget().yAxis()),
              emptyAxis(QStringLiteral("Thrust / N")));
    EXPECT_TRUE(preview.plotWidget().series().isEmpty());
    EXPECT_TRUE(dialog.plotWidget().series().isEmpty());
    EXPECT_EQ(dialog.curveCount(), 0);
    EXPECT_EQ(dialog.selectedCurve(), -1);
    EXPECT_THROW(dialog.setCurves({}, 0), BugError);
}

TEST_F(ThrustCurvePlotTest, ThePlotsThemeFollowsTheRuleOfItsCurveColours)
{
    // OpenRocket takes the chart's look and its curves' colours from one flag; a palette whose
    // window and Base disagree must not give light curves on a dark theme or the reverse.
    const Palette                    mixed(lightWindowDarkBase());
    const Gui::Test::TestGuiServices services;
    ThrustCurvePlot                  dialog(services.services());
    dialog.setCurves(testCurves(), 0);
    EXPECT_EQ(looksOf(dialog),
              (std::vector<CurveLook>{
                  look(QStringLiteral("Estes C6"), kLight0, 1.5, rocketplot::Marker::SQUARE),
                  look(QStringLiteral("Estes C6 (1)"), kLight1, 1.5),
              }));
    // rocketplot's light theme on its own background: its dark text would not read on the dark
    // Base.
    EXPECT_TRUE(dialog.plotWidget().theme() == rocketplot::Theme::light());
    {
        const Palette dark(Palette::dark());
        QCoreApplication::sendPostedEvents();
        const rocketplot::Theme& theme = dialog.plotWidget().theme();
        EXPECT_EQ(dialog.plotWidget().themeMode(), rocketplot::ThemeMode::CUSTOM);
        EXPECT_TRUE(theme.seriesColors == rocketplot::Theme::dark().seriesColors);
        EXPECT_EQ(theme.text.rgba(), rocketplot::Theme::dark().text.rgba());
        // On the dark palette's Base, as rocketplot's SYSTEM look.
        EXPECT_EQ(theme.background.rgba(),
                  QGuiApplication::palette().color(QPalette::Active, QPalette::Base).rgba());
        EXPECT_EQ(looksOf(dialog).front().color, kDark0);
    }
    QCoreApplication::sendPostedEvents();
    EXPECT_TRUE(dialog.plotWidget().theme() == rocketplot::Theme::light());
    EXPECT_EQ(looksOf(dialog).front().color, kLight0);
}

}  // namespace

}  // namespace QtRocket::Gui
