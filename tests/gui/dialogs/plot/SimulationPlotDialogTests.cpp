#include "QtRocket/gui/dialogs/plot/SimulationPlotDialog.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QPalette>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/TextAnnotation.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/PlotDialog.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "dialogs/plot/PlotDialogTestSupport.h"
#include "plot/PlotReadback.h"

// OpenRocket has no JUnit test of SimulationPlotDialog. What its dialog adds to PlotDialog (the
// warning over another x axis than the time, Show errors, the stage box) and what those do is
// OpenRocket's own dialog's, as tools/openrocket-gui-pins' plot-dialog probe printed it
// (PlotDialogProbe, its hand-pinned.txt: "A simple model rocket" #1 over the time and over the
// Mach number, "Three stage low power rocket" #1, and that flight with aborts added to its first
// and last stage), here over the same stored flights. PlotDialogTests.cpp has the controls the
// plot dialogs share.

namespace QtRocket::Gui
{

namespace
{

using SimulationPlotDialogTest = Gui::Test::GuiTest;
using Gui::Test::ExampleDesign;

/// OpenRocket's presets the tests plot: "Vertical motion vs. time" and "Drag coefficients vs.
/// Mach number".
constexpr std::size_t kVerticalMotion   = 0;
constexpr std::size_t kDragCoefficients = 5;

/// The stored flight of the example @p example's first simulation, plotted.
class StoredFlight
{
public:
    explicit StoredFlight(const char* example) : m_design(example) { }

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] const Simulation&           simulation() const
    {
        const std::shared_ptr<Simulation> first = m_design.simulation(0);
        QTROCKET_ASSERT(first != nullptr);
        return *first;
    }

    /// The dialog of the flight under preset @p preset.
    [[nodiscard]] std::unique_ptr<SimulationPlotDialog> dialog(std::size_t preset) const
    {
        auto made = std::make_unique<SimulationPlotDialog>(m_services.services(), simulation(),
                                                           Gui::Test::presetConfiguration(preset));
        made->plot().setReadoutHook(Gui::Test::silentReadout());
        return made;
    }

    /// The dialog of a copy of the flight named "Aborted copy" with aborts in the stages
    /// @p aborted (Gui::Test::withAborts()), under the first preset.
    [[nodiscard]] std::unique_ptr<SimulationPlotDialog> abortedDialog(
        std::span<const std::size_t> aborted) const
    {
        auto made = std::make_unique<SimulationPlotDialog>(
            m_services.services(),
            Gui::Test::presetPlotModelOf(
                "Aborted copy", Gui::Test::withAborts(*simulation().getSimulatedData(), aborted),
                kVerticalMotion));
        made->plot().setReadoutHook(Gui::Test::silentReadout());
        return made;
    }

private:
    Gui::Test::TestGuiServices m_services;
    ExampleDesign              m_design;
};

/// The texts of @p box's items.
[[nodiscard]] QStringList itemsOf(const QComboBox& box)
{
    QStringList items;
    for (int i = 0; i < box.count(); ++i)
    {
        items.append(box.itemText(i));
    }
    return items;
}

/// Whether each series of @p plot is visible, as "1" and "0", series after series.
[[nodiscard]] std::string visibleSeries(const rocketplot::PlotWidget& plot)
{
    std::string visible;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        visible += series.visible ? '1' : '0';
    }
    return visible;
}

/// The number of event lines @p plot draws.
[[nodiscard]] std::size_t eventLines(const rocketplot::PlotWidget& plot)
{
    return Gui::Test::readAnnotations(plot).events.size();
}

/// What a stage chosen in the stage box shows.
struct StageView
{
    /// visibleSeries().
    std::string series;
    std::size_t events            = 0;
    bool        showErrorsEnabled = false;
    /// The abort box's text as the plot gave it to rocketplot; empty without a box.
    QString abortText;

    friend bool operator==(const StageView&, const StageView&) = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const StageView& view, std::ostream* out)
{
    *out << "{" << view.series << ", " << view.events << " events, errors "
         << (view.showErrorsEnabled ? "enabled" : "disabled") << ", abort '"
         << QtStrings::toUtf8(view.abortText) << "'}";
}

/// Chooses stage item @p index of @p dialog's stage box (as a user does, offscreen: CLAUDE.md's
/// trap) and what it shows then.
[[nodiscard]] StageView chooseStage(const SimulationPlotDialog& dialog, int index)
{
    dialog.stageBox().setCurrentIndex(index);
    const rocketplot::TextAnnotation* const box = dialog.plot().abortBox();
    return {.series            = visibleSeries(dialog.plot().plotWidget()),
            .events            = eventLines(dialog.plot().plotWidget()),
            .showErrorsEnabled = dialog.showErrorsBox().isEnabled(),
            .abortText         = box == nullptr ? QString() : box->text()};
}

/// What the stage box shows for each of its items in turn, then "All Stages" again.
[[nodiscard]] std::vector<StageView> everyStage(const SimulationPlotDialog& dialog)
{
    std::vector<StageView> views;
    for (int i = 1; i < dialog.stageBox().count(); ++i)
    {
        views.push_back(chooseStage(dialog, i));
    }
    views.push_back(chooseStage(dialog, 0));
    return views;
}

/// The abort box's text of the model's stage @p branch (or of all), as the plot gives it.
[[nodiscard]] QString abortTextOf(const SimulationPlotDialog& dialog,
                                  std::optional<std::size_t>  branch)
{
    const std::string text = dialog.plot().model().getAbortText(branch);
    return text.empty() ? QString() : rocketplot::literalText(QtStrings::fromUtf8(text));
}

/// The left edge of @p widget in @p dialog's coordinates.
[[nodiscard]] int leftIn(const QWidget& widget, const QWidget& dialog)
{
    return widget.mapTo(&dialog, QPoint(0, 0)).x();
}

/// Where @p plot drew the legend's entry @p entry, after it was drawn anew (a test failure and a
/// point off the plot when it has no such entry).
[[nodiscard]] QPoint legendEntryPoint(rocketplot::PlotWidget& plot, qsizetype entry)
{
    static_cast<void>(plot.grab());
    const QList<rocketplot::Legend::Entry> entries = plot.legend()->entries();
    if (entry >= entries.size() || !entries.at(entry).isShown())
    {
        ADD_FAILURE() << "the legend shows no entry " << entry;
        return {-1, -1};
    }
    return entries.at(entry).rect.center().toPoint();
}

/// Clicks the legend's entry @p entry of @p plot.
void clickLegendEntry(rocketplot::PlotWidget& plot, qsizetype entry)
{
    QTest::mouseClick(&plot, Qt::LeftButton, Qt::NoModifier, legendEntryPoint(plot, entry));
}

/// Double-clicks the legend's entry @p entry of @p plot (the double click alone: CLAUDE.md's
/// trap).
void doubleClickLegendEntry(rocketplot::PlotWidget& plot, qsizetype entry)
{
    QTest::mouseDClick(&plot, Qt::LeftButton, Qt::NoModifier, legendEntryPoint(plot, entry));
}

/// Delivers the deferred deletions.
void deleteLater()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_F(SimulationPlotDialogTest, IsTitledWithTheSimulationItPlots)
{
    const StoredFlight                          flight(ExampleDesign::kSimpleModelRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    EXPECT_EQ(dialog->windowTitle(), QtStrings::fromUtf8(flight.simulation().getName()));
    EXPECT_EQ(dialog->plot().model().getTitle(), flight.simulation().getName());
    // The plot shares the simulation's flight data.
    EXPECT_EQ(&dialog->plot().model().getData(), flight.simulation().getSimulatedData().get());
    EXPECT_EQ(dialog->objectName(), QString::fromLatin1(SimulationPlotDialog::kGeometryName));
}

TEST_F(SimulationPlotDialogTest, ASimulationWithoutDataIsABug)
{
    const Gui::Test::TestGuiServices  services;
    ExampleDesign                     design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> copy = design.addCopyOf(0, "Not simulated");
    ASSERT_NE(copy, nullptr);
    EXPECT_THROW(SimulationPlotDialog(services.services(), *copy,
                                      Gui::Test::presetConfiguration(kVerticalMotion)),
                 BugError);
    QWidget parent;
    EXPECT_THROW(
        static_cast<void>(SimulationPlotDialog::showPlot(
            services.services(), *copy, Gui::Test::presetConfiguration(kVerticalMotion), parent)),
        BugError);
    deleteLater();
    EXPECT_TRUE(parent.findChildren<QDialog*>().isEmpty());
}

TEST_F(SimulationPlotDialogTest, OverAnotherXAxisThanTheTimeAWarningShowsUnderTheHelpText)
{
    const StoredFlight                          flight(ExampleDesign::kSimpleModelRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog  = flight.dialog(kDragCoefficients);
    const StyledLabel* const                    warning = dialog->timeSeriesWarning();
    ASSERT_NE(warning, nullptr);
    // "The data is plotted in time order even though the X axis type is not time. " (its
    // trailing space), "font size relative to a label's=-2.0 foreground=ffc80000".
    EXPECT_EQ(warning->text(),
              QStringLiteral("The data is plotted in time order even though the X axis type is "
                             "not time. "));
    EXPECT_EQ(warning->objectName(), QStringLiteral("timeSeriesWarning"));
    EXPECT_EQ(warning->palette().color(QPalette::WindowText),
              ThemeColors::color(ThemeColors::Key::DARK_ERROR));
    EXPECT_DOUBLE_EQ(warning->font().pointSizeF(), dialog->font().pointSizeF() - 2.0);
    EXPECT_TRUE(warning->isVisibleTo(dialog.get()));
    // Between the help text and the row.
    dialog->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(dialog.get()));
    const QPoint warningTop = warning->mapTo(dialog.get(), QPoint(0, 0));
    EXPECT_GT(warningTop.y(), dialog->helpLabel().mapTo(dialog.get(), QPoint(0, 0)).y());
    EXPECT_LT(warningTop.y(), dialog->showPointsBox().mapTo(dialog.get(), QPoint(0, 0)).y());
}

TEST_F(SimulationPlotDialogTest, OverTheTimeNoWarningShows)
{
    const StoredFlight                          flight(ExampleDesign::kSimpleModelRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    EXPECT_EQ(dialog->timeSeriesWarning(), nullptr);
    EXPECT_EQ(dialog->findChild<StyledLabel*>(QStringLiteral("timeSeriesWarning")), nullptr);
}

TEST_F(SimulationPlotDialogTest, ShowErrorsShowsOnlyForAFlightWithAnAbort)
{
    const StoredFlight                          flight(ExampleDesign::kSimpleModelRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    // "Show errors' ... selected=true visible=false enabled=true".
    EXPECT_FALSE(dialog->hasErrors());
    EXPECT_FALSE(dialog->showErrorsBox().isVisibleTo(dialog.get()));
    EXPECT_TRUE(dialog->showErrorsBox().isChecked());
    EXPECT_EQ(dialog->showErrorsBox().text(), QStringLiteral("Show errors"));
    EXPECT_EQ(dialog->showErrorsBox().objectName(), QStringLiteral("checkErrors"));

    constexpr std::array<std::size_t, 1>        kFirst{0};
    const std::unique_ptr<SimulationPlotDialog> aborted = flight.abortedDialog(kFirst);
    EXPECT_TRUE(aborted->hasErrors());
    EXPECT_TRUE(aborted->showErrorsBox().isVisibleTo(aborted.get()));
    EXPECT_TRUE(aborted->showErrorsBox().isChecked());
    EXPECT_TRUE(aborted->showErrorsBox().isEnabled());
    EXPECT_EQ(aborted->windowTitle(), QStringLiteral("Aborted copy"));
}

TEST_F(SimulationPlotDialogTest, TheAbortBoxShowsAtOnceAndShowErrorsHidesIt)
{
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    constexpr std::array<std::size_t, 2>        kFirstAndLast{0, 2};
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.abortedDialog(kFirstAndLast);
    dialog->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(dialog.get()));
    // OpenRocket bug 44: Java's box is gone at the open ("at open: ... annotations=0"), and
    // comes back only after Show errors is clicked twice ("annotations=1").
    const rocketplot::TextAnnotation* box = dialog->plot().abortBox();
    ASSERT_NE(box, nullptr);
    EXPECT_TRUE(box->isVisible());
    EXPECT_EQ(box->text(), abortTextOf(*dialog, std::nullopt));

    QTest::mouseClick(&dialog->showErrorsBox(), Qt::LeftButton);
    EXPECT_FALSE(dialog->showErrorsBox().isChecked());
    EXPECT_FALSE(dialog->plot().showsErrors());
    box = dialog->plot().abortBox();
    ASSERT_NE(box, nullptr);
    EXPECT_FALSE(box->isVisible());

    QTest::mouseClick(&dialog->showErrorsBox(), Qt::LeftButton);
    EXPECT_TRUE(dialog->plot().showsErrors());
    box = dialog->plot().abortBox();
    ASSERT_NE(box, nullptr);
    EXPECT_TRUE(box->isVisible());
}

TEST_F(SimulationPlotDialogTest, TheStageBoxListsTheStagesOfAFlightOfSeveralStages)
{
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    // "items=[All Stages | Sustainer | Booster stage(1) | Booster stage(2)] selected=0".
    EXPECT_EQ(
        itemsOf(dialog->stageBox()),
        (QStringList{QStringLiteral("All Stages"), QStringLiteral("Sustainer"),
                     QStringLiteral("Booster stage(1)"), QStringLiteral("Booster stage(2)")}));
    EXPECT_EQ(dialog->stageBox().currentIndex(), 0);
    EXPECT_TRUE(dialog->stageBox().isVisibleTo(dialog.get()));
    EXPECT_EQ(dialog->stageBox().objectName(), QStringLiteral("stageSelection"));
    EXPECT_FALSE(dialog->plot().shownBranch().has_value());
}

TEST_F(SimulationPlotDialogTest, AFlightOfOneStageHasNoStageBox)
{
    const StoredFlight                          flight(ExampleDesign::kSimpleModelRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    // "no stage box in the panel": Java makes it with "All Stages" and the one stage.
    EXPECT_FALSE(dialog->stageBox().isVisibleTo(dialog.get()));
    EXPECT_EQ(dialog->stageBox().count(), 2);
}

TEST_F(SimulationPlotDialogTest, TheStageBoxSitsBeforeTheZoomButtons)
{
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    constexpr std::array<std::size_t, 1>        kFirst{0};
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.abortedDialog(kFirst);
    dialog->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(dialog.get()));
    // "row, left to right: Show data points Show events Show errors JComboBox ZOOM_IN ...".
    EXPECT_LT(leftIn(dialog->showEventsBox(), *dialog), leftIn(dialog->showErrorsBox(), *dialog));
    EXPECT_LT(leftIn(dialog->showErrorsBox(), *dialog), leftIn(dialog->stageBox(), *dialog));
    EXPECT_LT(leftIn(dialog->stageBox(), *dialog), leftIn(dialog->zoomInButton(), *dialog));
}

TEST_F(SimulationPlotDialogTest, ChoosingAStageShowsItsSeriesAndEventsAlone)
{
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    // "at open: domainMarkers=13 ... seriesVisible=111111|111"; "stage 1 'Sustainer': errors
    // enabled=false domainMarkers=7 ... seriesVisible=100100|100", stage 2: 5, "010010|010",
    // stage 3: 5, "001001|001"; "stage 0 'All Stages': ... domainMarkers=13".
    EXPECT_EQ(visibleSeries(dialog->plot().plotWidget()), "111111111");
    EXPECT_EQ(eventLines(dialog->plot().plotWidget()), 13U);
    EXPECT_EQ(
        everyStage(*dialog),
        (std::vector<StageView>{
            {.series = "100100100", .events = 7, .showErrorsEnabled = false, .abortText = {}},
            {.series = "010010010", .events = 5, .showErrorsEnabled = false, .abortText = {}},
            {.series = "001001001", .events = 5, .showErrorsEnabled = false, .abortText = {}},
            {.series = "111111111", .events = 13, .showErrorsEnabled = false, .abortText = {}},
        }));
}

TEST_F(SimulationPlotDialogTest, TheLegendShowsAndHidesOnlyTheSeriesOfTheStageChosen)
{
    // A legend entry stands for its row's series of every stage (U-M4-6), and Java's stage box is
    // its plot's only control of which series show (setShowBranch()): with the Sustainer chosen,
    // the entry "Altitude (m)" hides and shows the Sustainer's altitude alone (a click), shows it
    // alone (a double click) and every series of the stage again (a second one), and nothing
    // shows another stage's series (the entry menu's "Show all"); choosing a stage shows each of
    // its series again.
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    dialog->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(dialog.get()));
    rocketplot::PlotWidget& plot = dialog->plot().plotWidget();
    ASSERT_EQ(chooseStage(*dialog, 1).series, "100100100");
    clickLegendEntry(plot, 0);
    EXPECT_EQ(visibleSeries(plot), "000100100");
    clickLegendEntry(plot, 0);
    EXPECT_EQ(visibleSeries(plot), "100100100");
    doubleClickLegendEntry(plot, 0);
    EXPECT_EQ(visibleSeries(plot), "100000000");
    doubleClickLegendEntry(plot, 0);
    EXPECT_EQ(visibleSeries(plot), "100100100");
    rocketplot::LineSeries* const booster = dialog->plot().series(1);
    ASSERT_NE(booster, nullptr);
    booster->setVisible(true);
    EXPECT_EQ(visibleSeries(plot), "100100100");
    clickLegendEntry(plot, 1);
    EXPECT_EQ(visibleSeries(plot), "100000100");
    EXPECT_EQ(chooseStage(*dialog, 2).series, "010010010");
    EXPECT_EQ(chooseStage(*dialog, 1).series, "100100100");
    // All Stages: an entry stands for its row's series of every stage, as before.
    EXPECT_EQ(chooseStage(*dialog, 0).series, "111111111");
    clickLegendEntry(plot, 0);
    EXPECT_EQ(visibleSeries(plot), "000111111");
}

TEST_F(SimulationPlotDialogTest, ShowErrorsIsEnabledForAStageWithAnAbort)
{
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    constexpr std::array<std::size_t, 2>        kFirstAndLast{0, 2};
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.abortedDialog(kFirstAndLast);
    // "stage 1 'Sustainer': errors enabled=true domainMarkers=8 ... annotations=1", stage 2:
    // false, 5, 0; stage 3: true, 6, 1; "stage 0 'All Stages': errors enabled=true
    // domainMarkers=15 annotations=1".
    EXPECT_EQ(everyStage(*dialog),
              (std::vector<StageView>{
                  {.series            = "100100100",
                   .events            = 8,
                   .showErrorsEnabled = true,
                   .abortText         = abortTextOf(*dialog, 0)},
                  {.series = "010010010", .events = 5, .showErrorsEnabled = false, .abortText = {}},
                  {.series            = "001001001",
                   .events            = 6,
                   .showErrorsEnabled = true,
                   .abortText         = abortTextOf(*dialog, 2)},
                  {.series            = "111111111",
                   .events            = 15,
                   .showErrorsEnabled = true,
                   .abortText         = abortTextOf(*dialog, std::nullopt)},
              }));
    EXPECT_TRUE(dialog->hasErrors(0));
    EXPECT_FALSE(dialog->hasErrors(1));
    EXPECT_TRUE(dialog->hasErrors(2));
    EXPECT_THROW(static_cast<void>(dialog->hasErrors(3)), BugError);
}

TEST_F(SimulationPlotDialogTest, AStageDrawsItsEventsWhileShowEventsIsUnchecked)
{
    // Kept as Java: "Show events unchecked, stage 1: domainMarkers=7", "..., stage 0:
    // domainMarkers=13".
    const StoredFlight                          flight(ExampleDesign::kThreeStageLowPowerRocket);
    const std::unique_ptr<SimulationPlotDialog> dialog = flight.dialog(kVerticalMotion);
    dialog->showEventsBox().click();
    ASSERT_EQ(eventLines(dialog->plot().plotWidget()), 0U);
    EXPECT_EQ(chooseStage(*dialog, 1).events, 7U);
    EXPECT_EQ(chooseStage(*dialog, 0).events, 13U);
    EXPECT_FALSE(dialog->showEventsBox().isChecked());
}

TEST_F(SimulationPlotDialogTest, ShowPlotOpensItModalOverItsParentAndDeletesItWhenClosed)
{
    StoredFlight flight(ExampleDesign::kSimpleModelRocket);
    QWidget      parent;
    parent.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&parent));
    SimulationPlotDialog& dialog =
        SimulationPlotDialog::showPlot(flight.services().services(), flight.simulation(),
                                       Gui::Test::presetConfiguration(kVerticalMotion), parent);
    dialog.plot().setReadoutHook(Gui::Test::silentReadout());
    const QPointer<SimulationPlotDialog> shown(&dialog);
    EXPECT_EQ(dialog.parentWidget(), &parent);
    EXPECT_TRUE(dialog.isWindow());
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_TRUE(dialog.isModal());
    EXPECT_EQ(dialog.windowModality(), Qt::WindowModal);
    dialog.closeButton().click();
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(SimulationPlotDialogTest, EscapeClosesAPlotShownAndDeletesIt)
{
    StoredFlight          flight(ExampleDesign::kSimpleModelRocket);
    QWidget               parent;
    SimulationPlotDialog& dialog =
        SimulationPlotDialog::showPlot(flight.services().services(), flight.simulation(),
                                       Gui::Test::presetConfiguration(kVerticalMotion), parent);
    dialog.plot().setReadoutHook(Gui::Test::silentReadout());
    const QPointer<SimulationPlotDialog> shown(&dialog);
    QTest::keyClick(&dialog.chart(), Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    deleteLater();
    EXPECT_TRUE(shown.isNull());
}

TEST_F(SimulationPlotDialogTest, APlotOpenWhenItsParentGoesGoesWithIt)
{
    StoredFlight flight(ExampleDesign::kSimpleModelRocket);
    auto         parent = std::make_unique<QWidget>();
    parent->show();
    SimulationPlotDialog& dialog =
        SimulationPlotDialog::showPlot(flight.services().services(), flight.simulation(),
                                       Gui::Test::presetConfiguration(kVerticalMotion), *parent);
    dialog.plot().setReadoutHook(Gui::Test::silentReadout());
    const QPointer<SimulationPlotDialog> shown(&dialog);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(shown.data()));
    parent.reset();
    EXPECT_TRUE(shown.isNull());
    deleteLater();
}

}  // namespace

}  // namespace QtRocket::Gui
