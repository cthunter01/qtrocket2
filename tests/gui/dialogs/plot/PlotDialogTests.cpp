#include "QtRocket/gui/dialogs/plot/PlotDialog.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFont>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
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
#include <rocketplot/Range.h>
#include <rocketplot/enums.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotDialog.h"
#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"
#include "dialogs/plot/PlotDialogTestSupport.h"
#include "plot/PlotReadback.h"

// OpenRocket has no JUnit test of PlotDialog. What its dialog is and does (the window, the
// controls in order, the chart's size after pack() and the gap below it, the default button, what
// the check boxes and the zoom buttons do and store, the remembered size and place) is OpenRocket's
// own dialog's, as tools/openrocket-gui-pins' plot-dialog probe printed it (PlotDialogProbe, its
// hand-pinned.txt), here through SimulationPlotDialog over the stored flight of "A simple model
// rocket" #1 and OpenRocket's first preset ("Vertical motion vs. time"). The simulation's extras
// (the warning, Show errors, the stages) are SimulationPlotDialogTests.cpp's.

namespace QtRocket::Gui
{

namespace
{

using PlotDialogTest = Gui::Test::GuiTest;
using Kind           = Gui::Test::ScriptedUserPrompts::Kind;

constexpr int kGrowth = 100;
/// A size another dialog of the kind left behind (within the offscreen screen's 800 x 600, which
/// a restored geometry is kept within).
constexpr int kRememberedWidth  = 700;
constexpr int kRememberedHeight = 500;

/// The stored flight of "A simple model rocket" #1 in a plot dialog, over the test's services.
class DialogUnderTest
{
public:
    DialogUnderTest() : m_design(Gui::Test::ExampleDesign::kSimpleModelRocket) { }

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }

    /// Makes the dialog of OpenRocket's preset @p preset (another one goes first): not shown.
    SimulationPlotDialog& make(std::size_t preset = 0)
    {
        m_dialog.reset();
        const std::shared_ptr<Simulation> simulation = m_design.simulation(0);
        if (simulation == nullptr)
        {
            throw std::runtime_error("the example has no simulation");
        }
        m_dialog = std::make_unique<SimulationPlotDialog>(m_services.services(), *simulation,
                                                          Gui::Test::presetConfiguration(preset));
        m_dialog->plot().setReadoutHook(Gui::Test::silentReadout());
        return *m_dialog;
    }

    /// make(), then shows it and waits until it is.
    SimulationPlotDialog& show(std::size_t preset = 0)
    {
        SimulationPlotDialog& dialog = make(preset);
        dialog.show();
        if (!QTest::qWaitForWindowExposed(&dialog))
        {
            ADD_FAILURE() << "the dialog is not shown";
        }
        return dialog;
    }

    [[nodiscard]] SimulationPlotDialog&   dialog() const noexcept { return *m_dialog; }
    [[nodiscard]] rocketplot::PlotWidget& plot() const noexcept
    {
        return m_dialog->chart().plotWidget();
    }

private:
    Gui::Test::TestGuiServices            m_services;
    Gui::Test::ExampleDesign              m_design;
    std::unique_ptr<SimulationPlotDialog> m_dialog;
};

/// A plot of one line without events, as Milestone 7's component analysis plot will be one.
class LinePlot final : public DataPlot
{
public:
    LinePlot(const GuiServices& services, QWidget* parent) : DataPlot(services, false, parent)
    {
        setTitles(QStringLiteral("A line"), QString());
        addSeries({0.0, 1.0, 2.0}, {0.0, 10.0, 5.0},
                  {.name        = QStringLiteral("Line"),
                   .legendGroup = {},
                   .axis        = 0,
                   .color       = QColor(Qt::blue),
                   .lineStyle   = LineStyle::SOLID,
                   .shape       = 0});
        setHomeRanges(rocketplot::Range{.min = -0.5, .max = 2.5},
                      rocketplot::Range{.min = -1.0, .max = 11.0}, std::nullopt);
    }

protected:
    [[nodiscard]] QString toolTipText(std::size_t /*series*/, std::size_t /*point*/) const override
    {
        return {};
    }
};

/// The plot dialog of a LinePlot without extras (as Milestone 7's CAPlotDialog: no hook of its
/// own), "Show events" checked at first.
class LinePlotDialog final : public PlotDialog
{
public:
    explicit LinePlotDialog(const GuiServices& services)
      : PlotDialog(services, QStringLiteral("LinePlotDialog"), nullptr),
        m_plot(makeChild<LinePlot>(this, services))
    {
        m_plot->setReadoutHook(Gui::Test::silentReadout());
        build(*m_plot, true, {.messages = {}, .controls = {}});
    }

private:
    LinePlot* m_plot;
};

/// @p widget's rectangle in @p dialog's coordinates.
[[nodiscard]] QRect rectIn(const QWidget& widget, const QWidget& dialog)
{
    return {widget.mapTo(&dialog, QPoint(0, 0)), widget.size()};
}

/// The objectNames of @p widgets that @p dialog shows, left to right.
[[nodiscard]] QStringList shownLeftToRight(const QWidget& dialog, std::vector<QWidget*> widgets)
{
    std::erase_if(widgets,
                  [&dialog](const QWidget* widget) { return !widget->isVisibleTo(&dialog); });
    std::ranges::sort(widgets, [&dialog](const QWidget* left, const QWidget* right) {
        return rectIn(*left, dialog).left() < rectIn(*right, dialog).left();
    });
    QStringList names;
    for (const QWidget* const widget : widgets)
    {
        names.append(widget->objectName());
    }
    return names;
}

/// Every widget of the row of @p dialog.
[[nodiscard]] std::vector<QWidget*> rowOf(const SimulationPlotDialog& dialog)
{
    return {&dialog.showPointsBox(), &dialog.showEventsBox(), &dialog.showErrorsBox(),
            &dialog.stageBox(),      &dialog.zoomInButton(),  &dialog.resetZoomButton(),
            &dialog.zoomOutButton(), &dialog.exportButton(),  &dialog.closeButton()};
}

/// Whether every widget of @p widgets that @p dialog shows lies on one line: each one's vertical
/// middle within the others' tops and bottoms.
[[nodiscard]] bool onOneLine(const QWidget& dialog, const std::vector<QWidget*>& widgets)
{
    const QRect first = rectIn(*widgets.front(), dialog);
    for (const QWidget* const widget : widgets)
    {
        const QRect rect = rectIn(*widget, dialog);
        if (widget->isVisibleTo(&dialog) &&
            (rect.center().y() < first.top() || rect.center().y() > first.bottom()))
        {
            return false;
        }
    }
    return true;
}

/// The marker of each series of @p plot.
[[nodiscard]] std::vector<rocketplot::Marker> markersOf(const rocketplot::PlotWidget& plot)
{
    std::vector<rocketplot::Marker> markers;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        markers.push_back(series.marker);
    }
    return markers;
}

/// The number of event lines @p plot draws.
[[nodiscard]] std::size_t eventLineCount(const rocketplot::PlotWidget& plot)
{
    return Gui::Test::readAnnotations(plot).events.size();
}

/// The x and y ranges of @p plot (min, max, min, max), and of its right y axis.
struct Ranges
{
    double xMin  = 0.0;
    double xMax  = 0.0;
    double yMin  = 0.0;
    double yMax  = 0.0;
    double y2Min = 0.0;
    double y2Max = 0.0;

    friend bool operator==(const Ranges&, const Ranges&) = default;
};

[[nodiscard]] Ranges rangesOf(const rocketplot::PlotWidget& plot)
{
    const Gui::Test::AxisReadback x  = Gui::Test::readAxis(*plot.xAxis());
    const Gui::Test::AxisReadback y  = Gui::Test::readAxis(*plot.yAxis());
    const Gui::Test::AxisReadback y2 = Gui::Test::readAxis(*plot.yAxis2());
    return {.xMin  = x.min,
            .xMax  = x.max,
            .yMin  = y.min,
            .yMax  = y.max,
            .y2Min = y2.min,
            .y2Max = y2.max};
}

/// The range [@p min, @p max] scaled by @p factor about its middle.
[[nodiscard]] std::pair<double, double> scaled(double min, double max, double factor)
{
    const double middle = (min + max) / 2.0;
    const double half   = (max - min) / 2.0 * factor;
    return {middle - half, middle + half};
}

/// Expects @p actual to be @p home with x scaled by @p xFactor and both y axes by @p yFactor
/// about their middles, within a millionth of their widths (the zoom computes in pixels).
void expectZoomed(const Ranges& actual, const Ranges& home, double xFactor, double yFactor)
{
    const auto expectRange = [](double min, double max, std::pair<double, double> expected) {
        const double tolerance = 1e-6 * (expected.second - expected.first);
        EXPECT_NEAR(min, expected.first, tolerance);
        EXPECT_NEAR(max, expected.second, tolerance);
    };
    expectRange(actual.xMin, actual.xMax, scaled(home.xMin, home.xMax, xFactor));
    expectRange(actual.yMin, actual.yMax, scaled(home.yMin, home.yMax, yFactor));
    expectRange(actual.y2Min, actual.y2Max, scaled(home.y2Min, home.y2Max, yFactor));
}

/// A save answer: @p name in @p directory, the PNG filter chosen.
[[nodiscard]] UserPrompts::SaveFileAnswer pngAnswer(const QtRocket::Test::TempDir& directory,
                                                    std::string_view               name)
{
    return {.file               = directory.resolve(pathFromUtf8(name)),
            .directory          = directory.path(),
            .saveSimulationData = std::nullopt,
            .selectedNameFilter = QStringLiteral("PNG image (*.png)")};
}

/// The first bytes of @p file (at most @p count), empty when it cannot be read.
[[nodiscard]] std::string firstBytesOf(const std::filesystem::path& file, std::size_t count)
{
    const Result<std::string> text = readTextFile(file);
    return text.has_value() ? text->substr(0, count) : std::string();
}

/// The PNG signature's readable part.
constexpr std::string_view kPngSignature = "\x89PNG";

TEST_F(PlotDialogTest, IsAWindowModalDialogTitledWithTheSimulation)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    // JDialog(parent, simulation.getName()), DOCUMENT_MODAL.
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Simulation 1"));
    EXPECT_EQ(dialog.windowModality(), Qt::WindowModal);
    EXPECT_EQ(dialog.objectName(), QStringLiteral("SimulationPlotDialog"));
    EXPECT_EQ(dialog.geometryName(), QStringLiteral("SimulationPlotDialog"));
    EXPECT_FALSE(dialog.isVisible());
}

TEST_F(PlotDialogTest, HasOpenRocketsControlsWithTheirTexts)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    EXPECT_EQ(&dialog.chart(), static_cast<const DataPlot*>(&dialog.plot()));
    EXPECT_EQ(dialog.findChild<DataPlot*>(QStringLiteral("chartPanel")), &dialog.chart());
    EXPECT_EQ(dialog.showPointsBox().text(), QStringLiteral("Show data points"));
    EXPECT_EQ(dialog.showEventsBox().text(), QStringLiteral("Show events"));
    EXPECT_EQ(dialog.exportButton().text(), QStringLiteral("Export Image"));
    EXPECT_EQ(dialog.closeButton().text(), QStringLiteral("Close"));
    EXPECT_EQ(dialog.findChild<QCheckBox*>(QStringLiteral("checkData")), &dialog.showPointsBox());
    EXPECT_EQ(dialog.findChild<QCheckBox*>(QStringLiteral("checkEvents")), &dialog.showEventsBox());
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("zoomIn")), &dialog.zoomInButton());
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("resetZoom")),
              &dialog.resetZoomButton());
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("zoomOut")), &dialog.zoomOutButton());
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("exportImage")),
              &dialog.exportButton());
    EXPECT_EQ(dialog.findChild<QPushButton*>(QStringLiteral("close")), &dialog.closeButton());
    EXPECT_EQ(dialog.findChild<StyledLabel*>(QStringLiteral("chartHelp")), &dialog.helpLabel());
}

TEST_F(PlotDialogTest, ADialogWithoutExtrasHasTheSharedControlsAlone)
{
    Gui::Test::TestGuiServices services;
    LinePlotDialog             dialog(services.services());
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("LinePlotDialog"));
    EXPECT_EQ(dialog.geometryName(), QStringLiteral("LinePlotDialog"));
    EXPECT_EQ(dialog.windowModality(), Qt::WindowModal);
    EXPECT_EQ(shownLeftToRight(
                  dialog, {&dialog.showPointsBox(), &dialog.showEventsBox(), &dialog.zoomInButton(),
                           &dialog.resetZoomButton(), &dialog.zoomOutButton(),
                           &dialog.exportButton(), &dialog.closeButton()}),
              (QStringList{QStringLiteral("checkData"), QStringLiteral("checkEvents"),
                           QStringLiteral("zoomIn"), QStringLiteral("resetZoom"),
                           QStringLiteral("zoomOut"), QStringLiteral("exportImage"),
                           QStringLiteral("close")}));
    EXPECT_EQ(dialog.chart().size(), QSize(DataPlot::kPreferredWidth, DataPlot::kPreferredHeight));
    // Its plot has no events (Plot.setShowEvents() does nothing); the choice is stored.
    QTest::mouseClick(&dialog.showEventsBox(), Qt::LeftButton);
    EXPECT_FALSE(PlotPreferences::isShowEvents(services.preferences()));
    EXPECT_TRUE(Gui::Test::readAnnotations(dialog.chart().plotWidget()).events.empty());
    // The zoom works on any plot.
    QTest::mouseClick(&dialog.zoomInButton(), Qt::LeftButton);
    const Gui::Test::AxisReadback x = Gui::Test::readAxis(*dialog.chart().plotWidget().xAxis());
    EXPECT_NEAR(x.min, 0.25, 1e-6);
    EXPECT_NEAR(x.max, 1.75, 1e-6);
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton);
    EXPECT_EQ(Gui::Test::readAxis(*dialog.chart().plotWidget().xAxis()).min, -0.5);
}

TEST_F(PlotDialogTest, TheZoomButtonsShowJavasIconsWithoutATextOrATooltip)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    EXPECT_TRUE(dialog.zoomInButton().text().isEmpty());
    EXPECT_TRUE(dialog.resetZoomButton().text().isEmpty());
    EXPECT_TRUE(dialog.zoomOutButton().text().isEmpty());
    EXPECT_EQ(Gui::Test::iconImage(dialog.zoomInButton().icon()),
              Gui::Test::iconImage(Icons::icon(Icons::Id::ZOOM_IN)));
    EXPECT_EQ(Gui::Test::iconImage(dialog.resetZoomButton().icon()),
              Gui::Test::iconImage(Icons::icon(Icons::Id::ZOOM_RESET)));
    EXPECT_EQ(Gui::Test::iconImage(dialog.zoomOutButton().icon()),
              Gui::Test::iconImage(Icons::icon(Icons::Id::ZOOM_OUT)));
    EXPECT_FALSE(dialog.zoomInButton().icon().isNull());
    EXPECT_TRUE(dialog.zoomInButton().toolTip().isEmpty());
    EXPECT_TRUE(dialog.exportButton().toolTip().isEmpty());
}

TEST_F(PlotDialogTest, TheHelpTextIsQtRocketsSmallerByTwoPoints)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    EXPECT_EQ(dialog.helpLabel().text(), PlotDialog::helpText());
    EXPECT_EQ(dialog.helpLabel().textFormat(), Qt::PlainText);
    // What the gestures of PlotInputBindings::simulationChart() do.
    EXPECT_EQ(PlotDialog::helpText(),
              QStringLiteral("left click drag to zoom area. shift- or ctrl-left click drag to pan. "
                             "mouse wheel to zoom, ctrl-mouse wheel x axis only, shift-mouse wheel "
                             "y axis only. double click to reset. right click for the menu."));
    // It wraps rather than widen the dialog (OpenRocket's one line sets its least width).
    EXPECT_TRUE(dialog.helpLabel().wordWrap());
    // StyledLabel(-2): "font size relative to a label's=-2.0".
    EXPECT_DOUBLE_EQ(dialog.helpLabel().font().pointSizeF(), dialog.font().pointSizeF() - 2.0);
}

TEST_F(PlotDialogTest, OpensWithTheChartAtChartPanelsPreferredSize)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    // "chart after pack=6,6 1024x768 gap below the chart=20".
    EXPECT_EQ(dialog.chart().size(), QSize(DataPlot::kPreferredWidth, DataPlot::kPreferredHeight));
    EXPECT_EQ(dialog.chart().sizeHint(), QSize(1024, 768));
    const QRect chart = rectIn(dialog.chart(), dialog);
    const QRect help  = rectIn(dialog.helpLabel(), dialog);
    EXPECT_EQ(help.top() - (chart.bottom() + 1), PlotDialog::kChartGap);
    EXPECT_EQ(PlotDialog::kChartGap, 20);
    EXPECT_EQ(help.left(), chart.left());
    // "Close's right edge is the chart's=true".
    EXPECT_EQ(rectIn(dialog.closeButton(), dialog).right(), chart.right());
}

TEST_F(PlotDialogTest, TheRowHasJavasOrderUnderTheHelpText)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    // "row, left to right: Show data points Show events ZOOM_IN ZOOM_RESET ZOOM_OUT Export Image
    // Close" (one stage, no abort: Show errors and the stage box hidden).
    EXPECT_EQ(shownLeftToRight(dialog, rowOf(dialog)),
              (QStringList{QStringLiteral("checkData"), QStringLiteral("checkEvents"),
                           QStringLiteral("zoomIn"), QStringLiteral("resetZoom"),
                           QStringLiteral("zoomOut"), QStringLiteral("exportImage"),
                           QStringLiteral("close")}));
    EXPECT_TRUE(onOneLine(dialog, rowOf(dialog)));
    EXPECT_GT(rectIn(dialog.showPointsBox(), dialog).top(),
              rectIn(dialog.helpLabel(), dialog).bottom());
    EXPECT_EQ(rectIn(dialog.showPointsBox(), dialog).left(),
              rectIn(dialog.helpLabel(), dialog).left());
}

TEST_F(PlotDialogTest, TheChartTakesWhatTheDialogGains)
{
    DialogUnderTest       test;
    SimulationPlotDialog& dialog = test.show();
    const QSize           chart  = dialog.chart().size();
    const QRect           close  = rectIn(dialog.closeButton(), dialog);
    dialog.resize(dialog.size() + QSize(kGrowth, kGrowth));
    QCoreApplication::processEvents();
    EXPECT_EQ(dialog.chart().size(), chart + QSize(kGrowth, kGrowth));
    EXPECT_EQ(rectIn(dialog.closeButton(), dialog).size(), close.size());
    EXPECT_EQ(rectIn(dialog.closeButton(), dialog).right(), rectIn(dialog.chart(), dialog).right());
}

TEST_F(PlotDialogTest, ShowDataPointsMarksEveryPointAndStoresTheChoice)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    // ShowPlotPoints: false by default.
    EXPECT_FALSE(dialog.showPointsBox().isChecked());
    EXPECT_EQ(markersOf(test.plot()), std::vector(3, rocketplot::Marker::NONE));

    QTest::mouseClick(&dialog.showPointsBox(), Qt::LeftButton);
    EXPECT_TRUE(dialog.showPointsBox().isChecked());
    EXPECT_TRUE(dialog.chart().showsPoints());
    EXPECT_EQ(markersOf(test.plot()), std::vector(3, DataPlot::pointShape(0)));
    EXPECT_TRUE(PlotPreferences::isShowPoints(test.services().preferences()));

    QTest::mouseClick(&dialog.showPointsBox(), Qt::LeftButton);
    EXPECT_FALSE(dialog.showPointsBox().isChecked());
    EXPECT_EQ(markersOf(test.plot()), std::vector(3, rocketplot::Marker::NONE));
    EXPECT_FALSE(PlotPreferences::isShowPoints(test.services().preferences()));
}

TEST_F(PlotDialogTest, ShowDataPointsOpensAsThePreferenceSays)
{
    DialogUnderTest test;
    PlotPreferences::setShowPoints(test.services().preferences(), true);
    const SimulationPlotDialog& dialog = test.make();
    EXPECT_TRUE(dialog.showPointsBox().isChecked());
    EXPECT_EQ(markersOf(test.plot()), std::vector(3, DataPlot::pointShape(0)));
}

TEST_F(PlotDialogTest, ShowEventsDrawsAndRemovesTheEventsAndStoresTheChoice)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    // ShowPlotEvents: true by default; the preset's five events ("at open: domainMarkers=5").
    EXPECT_TRUE(dialog.showEventsBox().isChecked());
    EXPECT_EQ(eventLineCount(test.plot()), 5U);

    QTest::mouseClick(&dialog.showEventsBox(), Qt::LeftButton);
    EXPECT_FALSE(dialog.showEventsBox().isChecked());
    EXPECT_EQ(eventLineCount(test.plot()), 0U);
    EXPECT_FALSE(PlotPreferences::isShowEvents(test.services().preferences()));

    QTest::mouseClick(&dialog.showEventsBox(), Qt::LeftButton);
    EXPECT_TRUE(dialog.showEventsBox().isChecked());
    EXPECT_EQ(eventLineCount(test.plot()), 5U);
    EXPECT_TRUE(PlotPreferences::isShowEvents(test.services().preferences()));
}

TEST_F(PlotDialogTest, ShowEventsOpensAsThePreferenceSays)
{
    DialogUnderTest test;
    PlotPreferences::setShowEvents(test.services().preferences(), false);
    const SimulationPlotDialog& dialog = test.make();
    EXPECT_FALSE(dialog.showEventsBox().isChecked());
    EXPECT_EQ(eventLineCount(test.plot()), 0U);
}

TEST_F(PlotDialogTest, ABoxActsOnlyWhenTheUserClicksIt)
{
    // Java's ActionListener: a box set by the program changes neither the plot nor the
    // preferences.
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    dialog.showEventsBox().setChecked(false);
    dialog.showPointsBox().setChecked(true);
    EXPECT_EQ(eventLineCount(test.plot()), 5U);
    EXPECT_FALSE(dialog.chart().showsPoints());
    EXPECT_FALSE(test.services().preferences().get(PreferenceKeys::kPlotShowEvents).has_value());
    EXPECT_FALSE(test.services().preferences().get(PreferenceKeys::kPlotShowPoints).has_value());
}

TEST_F(PlotDialogTest, TheZoomButtonsZoomAboutTheMiddleAndResetToTheHomeRanges)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    const Ranges                home   = rangesOf(test.plot());
    // The fitted ranges ("at open: ... x=[-0.5, 16.387999999999998] y0=[-5.8148599999999995,
    // 52.23286] y1=[-16.94345, 152.197103931479]").
    EXPECT_EQ(home.xMin, -0.5);
    EXPECT_EQ(home.y2Max, 152.197103931479);

    QTest::mouseClick(&dialog.zoomInButton(), Qt::LeftButton);
    expectZoomed(rangesOf(test.plot()), home, 0.5, 0.5);
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton);
    EXPECT_EQ(rangesOf(test.plot()), home);

    // Deviation (rocketplot's N17): OpenRocket's axes refuse a range beyond the home ranges, so
    // its Zoom out does nothing here ("zoom out: x=[-0.500000, 16.3880]").
    QTest::mouseClick(&dialog.zoomOutButton(), Qt::LeftButton);
    expectZoomed(rangesOf(test.plot()), home, 2.0, 2.0);
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton);
    EXPECT_EQ(rangesOf(test.plot()), home);
}

TEST_F(PlotDialogTest, AltWithAZoomButtonZoomsTheXAxisAlone)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    const Ranges                home   = rangesOf(test.plot());
    // ZOOM_IN_DOMAIN ("zoom in, Alt: x=[3.72200, 12.1660] y0=[-5.81486, 52.2329]").
    QTest::mouseClick(&dialog.zoomInButton(), Qt::LeftButton, Qt::AltModifier);
    expectZoomed(rangesOf(test.plot()), home, 0.5, 1.0);
    // The reset ignores Alt (ZOOM_RESET_BOTH).
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton, Qt::AltModifier);
    EXPECT_EQ(rangesOf(test.plot()), home);
    QTest::mouseClick(&dialog.zoomOutButton(), Qt::LeftButton, Qt::AltModifier);
    expectZoomed(rangesOf(test.plot()), home, 2.0, 1.0);
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton);

    // Alt is the click's: a click without it zooms both axes again, also from the program.
    QTest::mouseClick(&dialog.zoomInButton(), Qt::LeftButton, Qt::AltModifier);
    QTest::mouseClick(&dialog.resetZoomButton(), Qt::LeftButton);
    dialog.zoomInButton().click();
    expectZoomed(rangesOf(test.plot()), home, 0.5, 0.5);
}

TEST_F(PlotDialogTest, ExportImageAsksForAPngFileInTheDefaultDirectory)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    services.preferences().setDefaultDirectory(directory.path());
    const UserPrompts::SaveFileRequest request =
        PlotDialog::exportImageRequest(services.services().preferences());
    EXPECT_TRUE(request.title.isEmpty());
    EXPECT_EQ(request.nameFilters, QStringList{QStringLiteral("PNG image (*.png)")});
    EXPECT_EQ(request.directory, directory.path());
    EXPECT_TRUE(request.fileName.isEmpty());
    EXPECT_TRUE(request.refuseIllegalNames);
    EXPECT_FALSE(request.saveSimulationData.has_value());
    EXPECT_FALSE(request.csvOptions.has_value());
    const Gui::Test::TestGuiServices other;
    EXPECT_TRUE(PlotDialog::exportImageRequest(other.services().preferences()).directory.empty());
}

TEST_F(PlotDialogTest, ExportImageWritesThePlotAsAPng)
{
    DialogUnderTest               test;
    const QtRocket::Test::TempDir directory;
    const QtRocket::Test::TempDir elsewhere;
    test.services().preferences().setDefaultDirectory(elsewhere.path());
    const SimulationPlotDialog& dialog = test.show();
    test.services().prompts().answerSaveFile(pngAnswer(directory, "flight"));

    QTest::mouseClick(&dialog.exportButton(), Qt::LeftButton);

    const Gui::Test::ScriptedUserPrompts& prompts = test.services().prompts();
    ASSERT_EQ(prompts.saveFileRequests().size(), 1U);
    EXPECT_EQ(prompts.saveFileRequests().front().nameFilters,
              PlotDialog::exportImageRequest(test.services().preferences()).nameFilters);
    EXPECT_EQ(prompts.saveFileRequests().front().directory, elsewhere.path());
    // FileHelper.forceExtension(file, "png"); no pixel is compared.
    EXPECT_EQ(firstBytesOf(directory.resolve("flight.png"), kPngSignature.size()), kPngSignature);
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("flight")));
    // Nothing else was asked or shown, and the default directory is not stored.
    EXPECT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(test.services().preferences().getDefaultDirectory(), std::optional(elsewhere.path()));
    EXPECT_TRUE(dialog.isVisible());
}

TEST_F(PlotDialogTest, ExportImageKeepsAPngNameAndAsksBeforeReplacingAFile)
{
    DialogUnderTest               test;
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   existing = directory.resolve("old.PNG");
    ASSERT_TRUE(writeTextFile(existing, "old").has_value());
    SimulationPlotDialog& dialog = test.make();

    test.services().prompts().answerSaveFile(pngAnswer(directory, "old.PNG"));
    test.services().prompts().answerOverwrite(false);
    EXPECT_FALSE(dialog.exportImage());
    EXPECT_EQ(firstBytesOf(existing, 3), "old");

    test.services().prompts().answerSaveFile(pngAnswer(directory, "old.PNG"));
    test.services().prompts().answerOverwrite(true);
    EXPECT_TRUE(dialog.exportImage());
    EXPECT_EQ(firstBytesOf(existing, kPngSignature.size()), kPngSignature);
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("old.PNG.png")));
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& asked =
        test.services().prompts().prompts();
    ASSERT_EQ(asked.size(), 4U);
    EXPECT_EQ(asked[1].kind, Kind::OVERWRITE);
    EXPECT_EQ(asked[1].texts, QStringList{QStringLiteral("old.PNG")});
}

TEST_F(PlotDialogTest, ExportImageCancelledWritesNothing)
{
    DialogUnderTest               test;
    const QtRocket::Test::TempDir directory;
    SimulationPlotDialog&         dialog = test.make();
    test.services().prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(dialog.exportImage());
    EXPECT_TRUE(std::filesystem::is_empty(directory.path()));
    EXPECT_EQ(test.services().prompts().prompts().size(), 1U);
}

TEST_F(PlotDialogTest, CloseIsTheDefaultButtonAndClosesTheDialog)
{
    DialogUnderTest       test;
    SimulationPlotDialog& dialog = test.show();
    EXPECT_TRUE(dialog.closeButton().isDefault());
    EXPECT_FALSE(dialog.exportButton().autoDefault());
    EXPECT_FALSE(dialog.zoomInButton().autoDefault());
    EXPECT_FALSE(dialog.resetZoomButton().autoDefault());
    EXPECT_FALSE(dialog.zoomOutButton().autoDefault());
    const QSignalSpy finished(&dialog, &QDialog::finished);
    QTest::mouseClick(&dialog.closeButton(), Qt::LeftButton);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_FALSE(dialog.isVisible());
}

TEST_F(PlotDialogTest, EnterPressesCloseWhereverTheFocusIs)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    dialog.exportButton().setFocus();
    QTest::keyClick(&dialog.exportButton(), Qt::Key_Return);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_TRUE(test.services().prompts().prompts().empty());

    const SimulationPlotDialog& again = test.show();
    again.showEventsBox().setFocus();
    QTest::keyClick(&again.showEventsBox(), Qt::Key_Enter, Qt::KeypadModifier);
    EXPECT_FALSE(again.isVisible());
    EXPECT_TRUE(again.showEventsBox().isChecked());
}

TEST_F(PlotDialogTest, EscapeAndTheWindowsCloseButtonCloseTheDialog)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.show();
    QTest::keyClick(&dialog.chart(), Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    SimulationPlotDialog& again = test.show();
    EXPECT_TRUE(again.close());
    EXPECT_FALSE(again.isVisible());
}

TEST_F(PlotDialogTest, TheSizeAndPlaceAreRememberedForTheNextPlot)
{
    // "after setSize(900, 700), setLocation(40, 30): stored size=...900,700 stored position=
    // ...40,30"; "next dialog: size=900x700 location=40,30" (here a size the offscreen screen
    // holds).
    DialogUnderTest       test;
    SimulationPlotDialog& dialog = test.show();
    dialog.resize(kRememberedWidth, kRememberedHeight);
    QCoreApplication::processEvents();
    dialog.close();
    const Preferences* const windows =
        test.services().preferences().findNode(GuiPreferenceKeys::kNodeWindows);
    ASSERT_NE(windows, nullptr);
    EXPECT_TRUE(
        windows->get(WindowGeometry::key(QStringLiteral("SimulationPlotDialog"))).has_value());

    // The next dialog, of another preset: the size remembered.
    const SimulationPlotDialog& next = test.make(4);
    EXPECT_EQ(next.size(), QSize(kRememberedWidth, kRememberedHeight));
}

TEST_F(PlotDialogTest, WithoutARememberedSizeTheDialogIsItsContentsSize)
{
    DialogUnderTest             test;
    const SimulationPlotDialog& dialog = test.make();
    // pack(), not limited to the screen as QWidget::adjustSize() limits a window.
    EXPECT_EQ(dialog.size(), dialog.sizeHint());
    EXPECT_GE(dialog.width(), DataPlot::kPreferredWidth);
    EXPECT_GE(dialog.height(), DataPlot::kPreferredHeight + PlotDialog::kChartGap);
}

}  // namespace

}  // namespace QtRocket::Gui
