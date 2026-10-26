// OpenRocket's thrust-curve charts (ThrustCurvesProbe, OpenRocket 5f164fd0e:
// MotorInformationPanel's preview and ThrustCurveMotorPlotDialog, made by OpenRocket's classes for
// every motor set of the bundled database and read back through JFreeChart's API), against
// QtRocket's ThrustCurvePlot and ThrustCurveMotorPlotDialog given the same curves in the same
// order: the axes' ranges bit for bit, the series' names, colours, widths, markers and points, the
// dialog's legend items, the texts, the empty preview, the dialog's modality, keys, chart size and
// button, and the thrust axis in each force unit. Ticks and fonts are not compared (rocketplot
// picks its own). The pins are ThrustCurvePins.h, generated from the probe's output; each curve
// list is built from the curves' keys (manufacturer, designation, digest), so that the test does
// not depend on QtRocket's port of ThrustCurveMotorComparator.
//
// What QtRocket does differently on purpose (ThrustCurvePlot.h): the preview adds its curves from
// the last, so its series are Java's in reverse (rocketplot draws the last on top, JFreeChart's
// REVERSE order series 0); the dialog plots the thrust in the unit its axis names (OpenRocket plots
// newtons whatever the label says: bug 12 of the plan's section 10.3, fixed).

#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/ThrustCurveMotorPlotDialog.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "ScopedApplicationPalette.h"
#include "TestGuiServices.h"
#include "motor/TestMotorDatabase.h"
#include "plot/PlotReadback.h"
#include "plot/ThrustCurvePins.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using ThrustCurvePlotPinsGolden = Gui::Test::GuiTest;
using Curves                    = std::vector<std::shared_ptr<const ThrustCurveMotor>>;
using Palette                   = Gui::Test::ScopedApplicationPalette;
using Gui::Test::ThrustCurveChartPin;
using Gui::Test::ThrustCurveKeyPin;
using Gui::Test::ThrustCurveMarker;
using Gui::Test::ThrustCurveRangePin;
using Gui::Test::ThrustCurveSeriesPin;
using Gui::Test::ThrustCurveSetPin;

/// The OpenRocket commit of the goldens (tests/data/goldens/manifest.json).
constexpr std::string_view kGoldensCommit = "5f164fd0e05d6e188857b0185601293b7a6e9eaf";
/// The size the dialog's chart is given for its legend to be laid out (the dialog's own).
constexpr int kChartWidth  = 600;
constexpr int kChartHeight = 400;
/// How far QtRocket's thrust range in another unit may be from OpenRocket's newton range
/// converted to it (the same fit, its rounding in another order), relative.
constexpr double kUnitRangeTolerance = 1e-14;
/// The opaque alpha of a QRgb.
constexpr QRgb kOpaque = 0xff000000U;

/// A curve's key in the pins: its manufacturer's display name, its designation and its digest.
using CurveKey = std::tuple<std::string, std::string, std::string>;

/// The bundled database's curves by their keys.
class BundledCurves
{
public:
    BundledCurves()
    {
        for (const ThrustCurveMotorSet& set : QtRocket::Test::bundledMotorDatabase().getMotorSets())
        {
            for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
            {
                ++m_count;
                m_curves.emplace(CurveKey{motor->getManufacturer().getDisplayName(),
                                          motor->getDesignation(), motor->getDigest()},
                                 motor);
            }
        }
    }

    /// The number of curves of the database, and of distinct keys.
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }
    [[nodiscard]] std::size_t keyCount() const noexcept { return m_curves.size(); }

    /// The curve of @p pin, or null.
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> find(const ThrustCurveKeyPin& pin) const
    {
        const auto found = m_curves.find(CurveKey{
            std::string(pin.manufacturer), std::string(pin.designation), std::string(pin.digest)});
        return found == m_curves.end() ? nullptr : found->second;
    }

    /// The curves of @p set in the pins' order (a test failure for one that is missing).
    [[nodiscard]] Curves curvesOf(const ThrustCurveSetPin& set) const
    {
        Curves curves;
        for (const ThrustCurveKeyPin& pin : std::span(Gui::Test::kThrustCurves)
                                                .subspan(static_cast<std::size_t>(set.firstCurve),
                                                         static_cast<std::size_t>(set.curveCount)))
        {
            std::shared_ptr<const ThrustCurveMotor> motor = find(pin);
            if (motor == nullptr)
            {
                ADD_FAILURE() << "no curve " << pin.manufacturer << ' ' << pin.designation << ' '
                              << pin.digest;
                continue;
            }
            curves.push_back(std::move(motor));
        }
        return curves;
    }

private:
    std::map<CurveKey, std::shared_ptr<const ThrustCurveMotor>> m_curves;
    std::size_t                                                 m_count = 0;
};

/// "<manufacturer> <designation>" of the set of @p pin.
[[nodiscard]] std::string setName(const ThrustCurveSetPin& pin)
{
    return std::format("set {} ({} {})", pin.index, pin.manufacturer, pin.designation);
}

/// The ranges of @p plot's axes.
[[nodiscard]] ThrustCurveRangePin rangesOf(const ThrustCurvePlot& plot)
{
    const Gui::Test::AxisReadback x = Gui::Test::readAxis(*plot.plotWidget().xAxis());
    const Gui::Test::AxisReadback y = Gui::Test::readAxis(*plot.plotWidget().yAxis());
    return {.xMin = x.min, .xMax = x.max, .yMin = y.min, .yMax = y.max};
}

[[nodiscard]] bool sameRanges(const ThrustCurveRangePin& a, const ThrustCurveRangePin& b)
{
    return a.xMin == b.xMin && a.xMax == b.xMax && a.yMin == b.yMin && a.yMax == b.yMax;
}

[[nodiscard]] std::string rangeText(const ThrustCurveRangePin& range)
{
    return std::format("x [{:a}, {:a}] y [{:a}, {:a}]", range.xMin, range.xMax, range.yMin,
                       range.yMax);
}

/// A mismatch of @p what: what QtRocket shows and what OpenRocket does.
[[nodiscard]] std::string rangeMismatch(const std::string& what, const ThrustCurveRangePin& actual,
                                        const ThrustCurveRangePin& expected)
{
    return std::format("{}: {} (OpenRocket: {})", what, rangeText(actual), rangeText(expected));
}

/// What the tests compare of a series: its name (as given to rocketplot::literalText()), colour,
/// line width, marker (its size, its ring and whether it is drawn at every point only for a
/// marker) and points.
struct SeriesLook
{
    QString             name;
    QRgb                color      = 0;
    double              lineWidth  = 0.0;
    rocketplot::Marker  marker     = rocketplot::Marker::NONE;
    double              markerSize = 0.0;
    double              markerRing = 0.0;
    bool                everyPoint = false;
    std::vector<double> x;
    std::vector<double> y;

    friend bool operator==(const SeriesLook&, const SeriesLook&) = default;
};

[[nodiscard]] std::string lookText(const SeriesLook& look)
{
    return std::format("{{'{}' #{:08x} {} marker {} {} ring {} {} points {}}}",
                       QtStrings::toUtf8(look.name), look.color, look.lineWidth,
                       static_cast<int>(look.marker), look.markerSize, look.markerRing,
                       look.everyPoint ? "every" : "-", look.x.size());
}

[[nodiscard]] std::string looksText(const std::vector<SeriesLook>& looks)
{
    std::string text;
    for (const SeriesLook& look : looks)
    {
        text += lookText(look);
    }
    return text;
}

/// The looks of @p plot's series in its order.
[[nodiscard]] std::vector<SeriesLook> looksOf(const ThrustCurvePlot& plot)
{
    std::vector<SeriesLook> looks;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot.plotWidget()))
    {
        const bool marked = series.marker != rocketplot::Marker::NONE;
        looks.push_back({.name       = series.name,
                         .color      = series.color.rgba(),
                         .lineWidth  = series.lineWidth,
                         .marker     = series.marker,
                         .markerSize = marked ? series.markerSize : 0.0,
                         .markerRing = marked ? series.markerRingWidth : 0.0,
                         .everyPoint = marked && series.markersAlwaysVisible,
                         .x          = series.x,
                         .y          = series.y});
    }
    return looks;
}

/// What OpenRocket's series @p pin is, plotting @p motor (in seconds and newtons): a filled
/// square of JFreeChart, the pin's size its side, is rocketplot's SQUARE as wide, drawn at every
/// point without a ring (rocketplot's marker size is the side over
/// ThrustCurvePlot::kRocketplotSquareScale, since rocketplot shrinks its squares by it).
[[nodiscard]] SeriesLook expectedLook(const ThrustCurveSeriesPin& pin,
                                      const ThrustCurveMotor&     motor)
{
    const bool   marked = pin.marker == ThrustCurveMarker::FILLED_SQUARE;
    const double size =
        marked ? pin.markerSize / ThrustCurvePlot::kRocketplotSquareScale : pin.markerSize;
    return {.name       = rocketplot::literalText(QtStrings::fromUtf8(pin.name)),
            .color      = kOpaque | pin.color,
            .lineWidth  = pin.lineWidth,
            .marker     = marked ? rocketplot::Marker::SQUARE : rocketplot::Marker::NONE,
            .markerSize = size,
            .markerRing = 0.0,
            .everyPoint = marked,
            .x          = motor.getTimePoints(),
            .y          = motor.getThrustPoints()};
}

/// The series pins of a chart, kThrustCurveSeries[first] onwards.
[[nodiscard]] std::span<const ThrustCurveSeriesPin> seriesPins(int first, int count)
{
    return std::span(Gui::Test::kThrustCurveSeries)
        .subspan(static_cast<std::size_t>(first), static_cast<std::size_t>(count));
}

/// OpenRocket's preview of @p curves for @p chart, in QtRocket's order: Java's series from the last
/// (series i plots curve i).
[[nodiscard]] std::vector<SeriesLook> expectedPreview(const ThrustCurveChartPin& chart,
                                                      const Curves&              curves)
{
    const std::span<const ThrustCurveSeriesPin> pins =
        seriesPins(chart.firstPreview, chart.previewCount);
    std::vector<SeriesLook> looks;
    looks.reserve(pins.size());
    for (std::size_t k = pins.size(); k-- > 0;)
    {
        looks.push_back(expectedLook(pins[k], *curves[k]));
    }
    return looks;
}

/// OpenRocket's dialog of @p curves for @p chart: series 0 plots the selected curve, the others the
/// other curves in their order.
[[nodiscard]] std::vector<SeriesLook> expectedDialog(const ThrustCurveChartPin& chart,
                                                     const Curves&              curves)
{
    const auto selected = static_cast<std::size_t>(chart.selected);
    Curves     plotted{curves[selected]};
    for (std::size_t k = 0; k < curves.size(); ++k)
    {
        if (k != selected)
        {
            plotted.push_back(curves[k]);
        }
    }
    const std::span<const ThrustCurveSeriesPin> pins =
        seriesPins(chart.firstDialog, chart.dialogCount);
    std::vector<SeriesLook> looks;
    looks.reserve(pins.size());
    for (std::size_t k = 0; k < pins.size(); ++k)
    {
        looks.push_back(expectedLook(pins[k], *plotted[k]));
    }
    return looks;
}

/// The charts of @p chart's set in both styles, made with its curve selected, against OpenRocket's:
/// a line for each that differs.
[[nodiscard]] std::vector<std::string> seriesMismatches(const BundledCurves&       bundled,
                                                        const ThrustCurveChartPin& chart,
                                                        ThrustCurvePlot&           preview,
                                                        ThrustCurvePlot&           dialog)
{
    const ThrustCurveSetPin& set =
        Gui::Test::kThrustCurveSets.at(static_cast<std::size_t>(chart.set));
    const Curves curves = bundled.curvesOf(set);
    if (std::ssize(curves) != set.curveCount)
    {
        return {setName(set) + ": curves missing"};
    }
    std::vector<std::string> mismatches;
    preview.setCurves(curves, chart.selected);
    dialog.setCurves(curves, chart.selected);
    const std::vector<SeriesLook> previewLooks = looksOf(preview);
    const std::vector<SeriesLook> dialogLooks  = looksOf(dialog);
    const std::vector<SeriesLook> javaPreview  = expectedPreview(chart, curves);
    const std::vector<SeriesLook> javaDialog   = expectedDialog(chart, curves);
    if (previewLooks != javaPreview)
    {
        mismatches.push_back(std::format("{} selected {}: preview {} (OpenRocket: {})",
                                         setName(set), chart.selected, looksText(previewLooks),
                                         looksText(javaPreview)));
    }
    if (dialogLooks != javaDialog)
    {
        mismatches.push_back(std::format("{} selected {}: dialog {} (OpenRocket: {})", setName(set),
                                         chart.selected, looksText(dialogLooks),
                                         looksText(javaDialog)));
    }
    return mismatches;
}

/// seriesMismatches() of every chart pin of the light or the @p dark theme.
[[nodiscard]] std::vector<std::string> allSeriesMismatches(bool dark)
{
    const Gui::Test::TestGuiServices services;
    const BundledCurves              bundled;
    ThrustCurvePlot                  preview;
    ThrustCurvePlot                  dialog(services.services());
    std::vector<std::string>         mismatches;
    int                              charts = 0;
    for (const ThrustCurveChartPin& chart : Gui::Test::kThrustCurveCharts)
    {
        if (chart.dark != dark)
        {
            continue;
        }
        ++charts;
        for (std::string& mismatch : seriesMismatches(bundled, chart, preview, dialog))
        {
            mismatches.push_back(std::move(mismatch));
        }
    }
    if (charts == 0)
    {
        mismatches.emplace_back("no chart pins");
    }
    return mismatches;
}

/// Every motor set's ranges in both charts with its first curve selected, against OpenRocket's.
[[nodiscard]] std::vector<std::string> rangeMismatches()
{
    const Gui::Test::TestGuiServices services;
    const BundledCurves              bundled;
    ThrustCurvePlot                  preview;
    ThrustCurvePlot                  dialog(services.services());
    std::vector<std::string>         mismatches;
    for (const ThrustCurveSetPin& set : Gui::Test::kThrustCurveSets)
    {
        const Curves curves = bundled.curvesOf(set);
        if (std::ssize(curves) != set.curveCount)
        {
            mismatches.push_back(setName(set) + ": curves missing");
            continue;
        }
        preview.setCurves(curves, 0);
        dialog.setCurves(curves, 0);
        if (!sameRanges(rangesOf(preview), set.preview))
        {
            mismatches.push_back(
                rangeMismatch(setName(set) + " preview", rangesOf(preview), set.preview));
        }
        if (!sameRanges(rangesOf(dialog), set.dialog))
        {
            mismatches.push_back(
                rangeMismatch(setName(set) + " dialog", rangesOf(dialog), set.dialog));
        }
    }
    return mismatches;
}

/// The pinned curves QtRocket's bundled database lacks, or whose number of points differs.
[[nodiscard]] std::vector<std::string> curveMismatches(const BundledCurves& bundled)
{
    std::vector<std::string> mismatches;
    for (const ThrustCurveKeyPin& pin : Gui::Test::kThrustCurves)
    {
        const std::shared_ptr<const ThrustCurveMotor> motor = bundled.find(pin);
        if (motor == nullptr)
        {
            mismatches.push_back(
                std::format("missing: {} {} {}", pin.manufacturer, pin.designation, pin.digest));
        }
        else if (std::ssize(motor->getTimePoints()) != pin.points)
        {
            mismatches.push_back(std::format("{} {} {}: {} points (OpenRocket: {})",
                                             pin.manufacturer, pin.designation, pin.digest,
                                             motor->getTimePoints().size(), pin.points));
        }
    }
    return mismatches;
}

/// The colours of PlotColors::curveColor() against OpenRocket's.
[[nodiscard]] std::vector<std::string> colorMismatches()
{
    std::vector<std::string> mismatches;
    for (const Gui::Test::ThrustCurveColorPin& pin : Gui::Test::kThrustCurveColors)
    {
        const QRgb light = PlotColors::curveColor(pin.index, ThemeColors::Theme::LIGHT).rgba();
        const QRgb dark  = PlotColors::curveColor(pin.index, ThemeColors::Theme::DARK).rgba();
        if (light != (kOpaque | pin.light) || dark != (kOpaque | pin.dark))
        {
            mismatches.push_back(std::format("{}: #{:08x} #{:08x} (OpenRocket: #{:06x} #{:06x})",
                                             pin.index, light, dark, pin.light, pin.dark));
        }
    }
    return mismatches;
}

/// The legend entries of the dialog's chart for @p chart, against OpenRocket's legend items.
[[nodiscard]] std::vector<std::string> legendMismatches(const BundledCurves&       bundled,
                                                        const ThrustCurveChartPin& chart,
                                                        ThrustCurvePlot&           dialog)
{
    const ThrustCurveSetPin& set =
        Gui::Test::kThrustCurveSets.at(static_cast<std::size_t>(chart.set));
    dialog.setCurves(bundled.curvesOf(set), chart.selected);
    QStringList expected;
    for (const std::string_view label : std::span(Gui::Test::kThrustCurveLegend)
                                            .subspan(static_cast<std::size_t>(chart.firstLegend),
                                                     static_cast<std::size_t>(chart.legendCount)))
    {
        expected.append(QtStrings::fromUtf8(label));
    }
    const QStringList actual = Gui::Test::readPlotTexts(dialog.plotWidget()).legendEntries;
    if (actual == expected)
    {
        return {};
    }
    return {std::format("{} selected {}: legend '{}' (OpenRocket: '{}')", setName(set),
                        chart.selected, QtStrings::toUtf8(actual.join(QStringLiteral("', '"))),
                        QtStrings::toUtf8(expected.join(QStringLiteral("', '"))))};
}

/// legendMismatches() of every light chart pin.
[[nodiscard]] std::vector<std::string> allLegendMismatches()
{
    const Gui::Test::TestGuiServices services;
    const BundledCurves              bundled;
    ThrustCurvePlot                  dialog(services.services());
    dialog.resize(kChartWidth, kChartHeight);
    std::vector<std::string> mismatches;
    int                      charts = 0;
    for (const ThrustCurveChartPin& chart : Gui::Test::kThrustCurveCharts)
    {
        if (chart.dark)
        {
            continue;
        }
        ++charts;
        for (std::string& mismatch : legendMismatches(bundled, chart, dialog))
        {
            mismatches.push_back(std::move(mismatch));
        }
    }
    if (charts == 0)
    {
        mismatches.emplace_back("no chart pins");
    }
    return mismatches;
}

/// The texts of QtRocket's push buttons in @p dialog, in the order of the button box.
[[nodiscard]] QStringList buttonTexts(const QDialog& dialog)
{
    QStringList texts;
    for (const QPushButton* button : dialog.findChildren<QPushButton*>())
    {
        texts.append(button->text());
    }
    return texts;
}

/// The texts of kThrustCurveDialogButtons.
[[nodiscard]] QStringList pinnedButtonTexts()
{
    QStringList texts;
    for (const std::string_view text : Gui::Test::kThrustCurveDialogButtons)
    {
        texts.append(QtStrings::fromUtf8(text));
    }
    return texts;
}

/// The curves of the set the force units are pinned with.
[[nodiscard]] Curves unitSetCurves()
{
    return BundledCurves().curvesOf(
        Gui::Test::kThrustCurveSets.at(static_cast<std::size_t>(Gui::Test::kThrustCurveUnitSet)));
}

/// The force units' names in their order.
[[nodiscard]] std::vector<std::string> forceUnits()
{
    const UnitGroup&         force = unitGroup(UnitGroupId::FORCE);
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(force.getUnitCount()));
    for (int k = 0; k < force.getUnitCount(); ++k)
    {
        names.push_back(force.getUnit(k).getUnit());
    }
    return names;
}

/// The names of kThrustCurveUnits in their order.
[[nodiscard]] std::vector<std::string> pinnedForceUnits()
{
    std::vector<std::string> names;
    names.reserve(Gui::Test::kThrustCurveUnits.size());
    for (const Gui::Test::ThrustCurveUnitPin& pin : Gui::Test::kThrustCurveUnits)
    {
        names.emplace_back(pin.unit);
    }
    return names;
}

/// QtRocket's dialog chart with @p pin's force unit as the default unit against OpenRocket's: the
/// label equal, the range OpenRocket's newton range in that unit (bug 12, fixed; exactly for
/// newtons, which OpenRocket plots in every unit).
[[nodiscard]] std::vector<std::string> unitMismatches(const Gui::Test::ThrustCurveUnitPin& pin,
                                                      const Curves& curves, ThrustCurvePlot& dialog)
{
    const QtRocket::Test::DefaultUnitsGuard guard;
    if (!unitGroup(UnitGroupId::FORCE).setDefaultUnit(pin.unit))
    {
        return {std::format("no force unit {}", pin.unit)};
    }
    dialog.setCurves(curves, 0);
    const Gui::Test::AxisReadback axis     = Gui::Test::readAxis(*dialog.plotWidget().yAxis());
    const Unit&                   unit     = unitGroup(UnitGroupId::FORCE).getDefaultUnit();
    const double                  expected = unit.toUnit(pin.yMax);
    const bool                    inRange =
        pin.unit == "N" ? axis.max == expected
                        : std::abs(axis.max - expected) <= kUnitRangeTolerance * std::abs(expected);
    if (axis.label == rocketplot::literalText(QtStrings::fromUtf8(pin.thrustLabel)) &&
        axis.min == unit.toUnit(pin.yMin) && inRange)
    {
        return {};
    }
    return {std::format("{}: '{}' [{:a}, {:a}] (OpenRocket: '{}' [{:a}, {:a}] N)", pin.unit,
                        QtStrings::toUtf8(axis.label), axis.min, axis.max, pin.thrustLabel,
                        pin.yMin, pin.yMax)};
}

/// unitMismatches() of every force unit.
[[nodiscard]] std::vector<std::string> allUnitMismatches()
{
    const Gui::Test::TestGuiServices services;
    const Curves                     curves = unitSetCurves();
    ThrustCurvePlot                  dialog(services.services());
    std::vector<std::string>         mismatches;
    for (const Gui::Test::ThrustCurveUnitPin& pin : Gui::Test::kThrustCurveUnits)
    {
        for (std::string& mismatch : unitMismatches(pin, curves, dialog))
        {
            mismatches.push_back(std::move(mismatch));
        }
    }
    return mismatches;
}

TEST_F(ThrustCurvePlotPinsGolden, ThePinsComeFromTheGoldensCommit)
{
    EXPECT_EQ(Gui::Test::kThrustCurvePinsCommit, kGoldensCommit);
}

TEST_F(ThrustCurvePlotPinsGolden, EveryCurveOfTheBundledDatabaseIsPinnedOnce)
{
    const BundledCurves bundled;
    // No two curves of QtRocket's database share a key, and every one is pinned.
    EXPECT_EQ(bundled.keyCount(), bundled.count());
    EXPECT_EQ(bundled.count(), Gui::Test::kThrustCurves.size());
    EXPECT_EQ(curveMismatches(bundled), std::vector<std::string>{});
    EXPECT_EQ(QtRocket::Test::bundledMotorDatabase().getMotorSets().size(),
              Gui::Test::kThrustCurveSets.size());
}

TEST_F(ThrustCurvePlotPinsGolden, CurveColoursAreOpenRocketsInBothThemes)
{
    EXPECT_EQ(colorMismatches(), std::vector<std::string>{});
}

TEST_F(ThrustCurvePlotPinsGolden, EveryMotorSetHasOpenRocketsRangesInBothCharts)
{
    EXPECT_EQ(rangeMismatches(), std::vector<std::string>{});
}

TEST_F(ThrustCurvePlotPinsGolden, EverySetOfSeveralCurvesShowsOpenRocketsSeries)
{
    const Palette light(Palette::light());
    EXPECT_EQ(allSeriesMismatches(false), std::vector<std::string>{});
}

TEST_F(ThrustCurvePlotPinsGolden, TheDarkThemeShowsOpenRocketsDarkSeries)
{
    const Palette dark(Palette::dark());
    EXPECT_EQ(allSeriesMismatches(true), std::vector<std::string>{});
}

TEST_F(ThrustCurvePlotPinsGolden, TheDialogsLegendListsOpenRocketsItems)
{
    const Palette light(Palette::light());
    EXPECT_EQ(allLegendMismatches(), std::vector<std::string>{});
}

TEST_F(ThrustCurvePlotPinsGolden, TheChartsHaveOpenRocketsTexts)
{
    const Gui::Test::TestGuiServices services;
    const ThrustCurvePlot            preview;
    const ThrustCurveMotorPlotDialog dialog(unitSetCurves(), 0, services.services());
    const rocketplot::PlotWidget&    chart = dialog.chart().plotWidget();
    EXPECT_EQ(preview.plotWidget().title(),
              rocketplot::literalText(QtStrings::fromUtf8(Gui::Test::kThrustCurvePreviewTitle)));
    EXPECT_EQ(dialog.windowTitle(), QtStrings::fromUtf8(Gui::Test::kThrustCurveDialogWindowTitle));
    EXPECT_EQ(chart.title(), rocketplot::literalText(
                                 QtStrings::fromUtf8(Gui::Test::kThrustCurveDialogChartTitle)));
    EXPECT_EQ(chart.xAxis()->label(),
              rocketplot::literalText(QtStrings::fromUtf8(Gui::Test::kThrustCurveDialogTimeLabel)));
    EXPECT_EQ(
        chart.yAxis()->label(),
        rocketplot::literalText(QtStrings::fromUtf8(Gui::Test::kThrustCurveDialogThrustLabel)));
    EXPECT_EQ(dialog.closeButton().text(), QtStrings::fromUtf8(Gui::Test::kThrustCurveCloseButton));
    // The preview has no axis labels.
    EXPECT_TRUE(preview.plotWidget().xAxis()->label().isEmpty());
    EXPECT_TRUE(preview.plotWidget().yAxis()->label().isEmpty());
}

TEST_F(ThrustCurvePlotPinsGolden, TheEmptyPreviewHasJFreeChartsRange)
{
    ThrustCurvePlot preview;
    preview.setCurves(unitSetCurves(), 0);
    preview.clear();
    EXPECT_PRED2(sameRanges, rangesOf(preview), Gui::Test::kThrustCurveEmptyRange)
        << rangeText(rangesOf(preview));
}

TEST_F(ThrustCurvePlotPinsGolden, TheDialogIsOpenRocketsDialog)
{
    const Gui::Test::ThrustCurveDialogPin& pin = Gui::Test::kThrustCurveDialog;
    ASSERT_EQ(pin.modality, "APPLICATION_MODAL");
    ASSERT_EQ(pin.defaultCloseOperation, "DISPOSE_ON_CLOSE");
    const Gui::Test::TestGuiServices services;
    QWidget                          parent;
    ThrustCurveMotorPlotDialog&      dialog =
        ThrustCurveMotorPlotDialog::showPlot(unitSetCurves(), 0, services.services(), parent);
    const QPointer<ThrustCurveMotorPlotDialog> shown(&dialog);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(buttonTexts(dialog), pinnedButtonTexts());
    EXPECT_EQ(dialog.closeButton().isDefault(), pin.defaultButton);

    // The chart's size as the dialog opens, and the button's place against it.
    const QRect chart(dialog.chart().mapTo(&dialog, QPoint(0, 0)), dialog.chart().size());
    const QRect close(dialog.closeButton().mapTo(&dialog, QPoint(0, 0)),
                      dialog.closeButton().size());
    EXPECT_EQ(chart.width(), pin.chartWidth);
    EXPECT_EQ(chart.height(), pin.chartHeight);
    EXPECT_EQ(close.x() + close.width() == chart.x() + chart.width(), pin.closeRightAligned);
    EXPECT_EQ(close.y() >= chart.y() + chart.height(), pin.closeBelowChart);

    // Esc closes it (installEscapeCloseOperation()), and it goes (DISPOSE_ON_CLOSE).
    ASSERT_TRUE(pin.escapeCloses);
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(shown.isNull());
}

TEST_F(ThrustCurvePlotPinsGolden, TheThrustAxisIsInEachForceUnit)
{
    EXPECT_EQ(forceUnits(), pinnedForceUnits());
    EXPECT_EQ(allUnitMismatches(), std::vector<std::string>{});
}

}  // namespace

}  // namespace QtRocket::Gui
