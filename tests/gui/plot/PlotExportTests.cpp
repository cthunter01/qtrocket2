#include "QtRocket/gui/plot/PlotExport.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>
#include <rocketplot/Annotation.h>
#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/ReferenceLine.h>
#include <rocketplot/Series.h>
#include <rocketplot/text.h>

#include "ExpectedQtMessage.h"
#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"
#include "widgets/WidgetTestSupport.h"

// OpenRocket has no JUnit test of its plot exports. What is expected is OpenRocket's way of
// exporting a plot (PlotDialog.doPNGExport(), LandingDispersionResultsDialog.exportPlot():
// SaveFileChooser, FileHelper.forceExtension(), FileHelper.confirmWrite()) over rocketplot's kinds
// of file, and rocketplot v0.6.0's menus (PlotWidget::showContextMenu(), showEntryMenu(),
// showAnnotationEntryMenu()) without their dialogs.

namespace QtRocket::Gui
{

namespace
{

using PlotExportTest = Gui::Test::GuiTest;
using Kind           = Gui::Test::ScriptedUserPrompts::Kind;

constexpr int kPlotWidth  = 400;
constexpr int kPlotHeight = 300;

/// Gives @p plot a thrust curve named "Thrust (0)" in the first plot colour, OpenRocket's axis
/// labels and its size.
void addThrustCurve(rocketplot::PlotWidget& plot)
{
    const std::vector<double>     time{0.0, 0.1, 0.5, 1.0};
    const std::vector<double>     thrust{0.0, 12.0, 5.0, 0.0};
    rocketplot::LineSeries* const curve =
        plot.addLine(time, thrust, rocketplot::literalText(QStringLiteral("Thrust (0)")));
    curve->setColor(PlotColors::plotColor(0));
    plot.xAxis()->setLabel(rocketplot::literalText(QStringLiteral("Time / s")));
    plot.yAxis()->setLabel(rocketplot::literalText(QStringLiteral("Thrust / N")));
    plot.resize(kPlotWidth, kPlotHeight);
}

/// The texts of @p menu's actions in order, without its separators (a submenu by its title).
[[nodiscard]] QStringList actionTexts(const QMenu& menu)
{
    QStringList           texts;
    const QList<QAction*> actions = menu.actions();
    for (const QAction* const action : actions)
    {
        if (!action->isSeparator())
        {
            texts.append(action->text());
        }
    }
    return texts;
}

/// The objectName of @p menu's last action, empty for an empty menu.
[[nodiscard]] QString lastActionName(const QMenu& menu)
{
    const QList<QAction*> actions = menu.actions();
    return actions.isEmpty() ? QString() : actions.constLast()->objectName();
}

/// Opens @p widget's context menu at @p position as a right click does: the menu shown, null for
/// none.
[[nodiscard]] QMenu* openContextMenu(QWidget& widget, QPoint position)
{
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, widget.mapToGlobal(position));
    QApplication::sendEvent(&widget, &event);
    return qobject_cast<QMenu*>(QApplication::activePopupWidget());
}

/// The middle of @p rect as a whole point.
[[nodiscard]] QPoint middleOf(const QRectF& rect)
{
    return rect.center().toPoint();
}

/// The texts of the context menu that a right click in the middle of @p plot's plot area opens,
/// which is closed again.
[[nodiscard]] QStringList contextMenuTexts(rocketplot::PlotWidget& plot)
{
    const Gui::Test::OffscreenPopupWarnings warnings;
    QMenu* const                            menu = openContextMenu(plot, middleOf(plot.plotArea()));
    if (menu == nullptr)
    {
        ADD_FAILURE() << "no context menu";
        return {};
    }
    const QStringList texts = actionTexts(*menu);
    menu->close();
    return texts;
}

/// The texts of the menu that a right click on @p plot's only legend entry opens, which is closed
/// again. The plot is painted first: the legend's entries are where it was last drawn.
[[nodiscard]] QStringList legendMenuTexts(rocketplot::PlotWidget& plot)
{
    static_cast<void>(plot.grab());
    if (plot.legendArea().isEmpty())
    {
        ADD_FAILURE() << "no legend";
        return {};
    }
    const Gui::Test::OffscreenPopupWarnings warnings;
    QMenu* const menu = openContextMenu(plot, middleOf(plot.legendArea()));
    if (menu == nullptr)
    {
        ADD_FAILURE() << "no legend menu";
        return {};
    }
    const QStringList texts = actionTexts(*menu);
    menu->close();
    return texts;
}

/// rocketplot's context menu of a plot area: its English texts, Export... last.
[[nodiscard]] QStringList rocketplotMenu()
{
    return {
        QStringLiteral("Back"),       QStringLiteral("Forward"),    QStringLiteral("Reset view"),
        QStringLiteral("Crosshair"),  QStringLiteral("Free"),       QStringLiteral("Snap to data"),
        QStringLiteral("Trace data"), QStringLiteral("Copy image"), QStringLiteral("Export\u2026")};
}

/// A save answer: @p name in @p directory, with @p filter chosen.
[[nodiscard]] UserPrompts::SaveFileAnswer saveAnswer(const QtRocket::Test::TempDir& directory,
                                                     std::string_view name, const QString& filter)
{
    return {.file               = directory.resolve(pathFromUtf8(name)),
            .directory          = directory.path(),
            .saveSimulationData = std::nullopt,
            .selectedNameFilter = filter};
}

/// The first bytes of @p file (at most @p count), empty when it cannot be read.
[[nodiscard]] std::string headOf(const std::filesystem::path& file, std::size_t count)
{
    const Result<std::string> text = readTextFile(file);
    return text.has_value() ? text->substr(0, count) : std::string();
}

/// The PNG signature's readable part.
constexpr std::string_view kPngSignature = "\x89PNG";

/// rocketplot's name filters with the given one chosen: helpers for the tests of withSuffix().
[[nodiscard]] QString filterAt(int index)
{
    return PlotExport::nameFilters().at(index);
}

TEST_F(PlotExportTest, OffersRocketplotsKindsOfFileWithOpenRocketsTexts)
{
    EXPECT_EQ(
        PlotExport::nameFilters(),
        (QStringList{QStringLiteral("PNG image (*.png)"), QStringLiteral("JPEG image (*.jpg)"),
                     QStringLiteral("SVG files (*.svg)"), QStringLiteral("PDF files (*.pdf)"),
                     QStringLiteral("Comma Separated Files (*.csv)")}));
}

TEST_F(PlotExportTest, AsksAsOpenRocketsPlotExportsAskInTheDefaultDirectory)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    services.preferences().setDefaultDirectory(directory.path());
    const UserPrompts::SaveFileRequest request =
        PlotExport::exportRequest(services.services().preferences());
    EXPECT_EQ(request.title, QStringLiteral("Export plot"));
    EXPECT_EQ(request.nameFilters, PlotExport::nameFilters());
    EXPECT_EQ(request.directory, directory.path());
    EXPECT_TRUE(request.fileName.isEmpty());
    EXPECT_EQ(request.saveSimulationData, std::nullopt);
    EXPECT_TRUE(request.refuseIllegalNames);

    // Without a default directory: none (the dialog starts in the home directory).
    const Gui::Test::TestGuiServices other;
    EXPECT_TRUE(PlotExport::exportRequest(other.services().preferences()).directory.empty());
}

TEST_F(PlotExportTest, RocketplotsOwnMenusHaveItsExportAndColorDialogs)
{
    // What install() replaces, as rocketplot v0.6.0 has it: its Export... and its legend's
    // Color... (found by these texts).
    rocketplot::PlotWidget plot;
    addThrustCurve(plot);
    EXPECT_EQ(contextMenuTexts(plot), rocketplotMenu());
    plot.legend()->setVisible(true);
    EXPECT_TRUE(legendMenuTexts(plot).contains(QStringLiteral("Color\u2026")));
}

TEST_F(PlotExportTest, TheContextMenuHasQtRocketsExportInPlaceOfRocketplots)
{
    Gui::Test::TestGuiServices services;
    rocketplot::PlotWidget     plot;
    addThrustCurve(plot);
    PlotExport::install(plot, services.services());

    const Gui::Test::OffscreenPopupWarnings warnings;
    QMenu* const                            menu = openContextMenu(plot, middleOf(plot.plotArea()));
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(actionTexts(*menu), rocketplotMenu());
    // The last is QtRocket's, at rocketplot's place: rocketplot's is gone from the menu.
    EXPECT_EQ(lastActionName(*menu), PlotExport::exportActionName());
    EXPECT_EQ(menu->findChildren<QAction*>(PlotExport::exportActionName()).size(), 1);
    menu->close();
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(PlotExportTest, TheMenusExportAsksForTheFileAndWritesTheKindChosen)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    const QtRocket::Test::TempDir elsewhere;
    services.preferences().setDefaultDirectory(elsewhere.path());
    rocketplot::PlotWidget plot;
    addThrustCurve(plot);
    PlotExport::install(plot, services.services());
    services.prompts().answerSaveFile(saveAnswer(directory, "thrust", filterAt(2)));

    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        QMenu* const menu = openContextMenu(plot, middleOf(plot.plotArea()));
        ASSERT_NE(menu, nullptr);
        auto* const action = menu->findChild<QAction*>(PlotExport::exportActionName());
        ASSERT_NE(action, nullptr);
        menu->close();
        action->trigger();
    }

    ASSERT_EQ(services.prompts().saveFileRequests().size(), 1U);
    const UserPrompts::SaveFileRequest& request = services.prompts().saveFileRequests().front();
    EXPECT_EQ(request.title, QStringLiteral("Export plot"));
    EXPECT_EQ(request.nameFilters, PlotExport::nameFilters());
    EXPECT_EQ(request.directory, elsewhere.path());
    EXPECT_TRUE(request.refuseIllegalNames);
    // "thrust" with "SVG files (*.svg)" chosen: an SVG drawing.
    EXPECT_TRUE(headOf(directory.resolve("thrust.svg"), 64).starts_with("<?xml"));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("thrust")));
    // Nothing else was asked or shown, and the default directory is not stored (as OpenRocket's
    // plot exports leave it).
    EXPECT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_EQ(services.preferences().getDefaultDirectory(), std::optional(elsewhere.path()));
}

TEST_F(PlotExportTest, ANameWithTheSuffixOfAKindKeepsItWhateverTheKindChosen)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    rocketplot::PlotWidget        plot;
    addThrustCurve(plot);
    services.prompts().answerSaveFile(saveAnswer(directory, "data.CSV", filterAt(0)));
    EXPECT_TRUE(PlotExport::exportPlot(plot, services.services()));
    // The data, as rocketplot writes it: the header row is the x axis' label and the names.
    constexpr std::string_view kHeader = "Time / s,Thrust (0)\n";
    EXPECT_EQ(headOf(directory.resolve("data.CSV"), kHeader.size()), kHeader);
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("data.CSV.png")));
}

TEST_F(PlotExportTest, ANameWithoutTheSuffixOfAKindGetsTheFirstKindsWithoutAChoice)
{
    // A dialog that does not tell its filter (an empty one): the first kind, PNG.
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    rocketplot::PlotWidget        plot;
    addThrustCurve(plot);
    services.prompts().answerSaveFile(saveAnswer(directory, "plot.v2", QString()));
    EXPECT_TRUE(PlotExport::exportPlot(plot, services.services()));
    EXPECT_EQ(headOf(directory.resolve("plot.v2.png"), kPngSignature.size()), kPngSignature);
}

TEST_F(PlotExportTest, AnExistingFileIsReplacedOnlyWhenTheUserAgrees)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   file = directory.write("plot.png", "old");
    rocketplot::PlotWidget        plot;
    addThrustCurve(plot);

    services.prompts().answerSaveFile(saveAnswer(directory, "plot", filterAt(0)));
    services.prompts().answerOverwrite(false);
    EXPECT_FALSE(PlotExport::exportPlot(plot, services.services()));
    EXPECT_EQ(headOf(file, 16), "old");
    ASSERT_EQ(services.prompts().prompts().size(), 2U);
    // FileHelper.confirmWrite(): the file's name.
    EXPECT_EQ(services.prompts().prompts().back().kind, Kind::OVERWRITE);
    EXPECT_EQ(services.prompts().prompts().back().texts, QStringList{QStringLiteral("plot.png")});

    services.prompts().answerSaveFile(saveAnswer(directory, "plot", filterAt(0)));
    services.prompts().answerOverwrite(true);
    EXPECT_TRUE(PlotExport::exportPlot(plot, services.services()));
    EXPECT_EQ(headOf(file, kPngSignature.size()), kPngSignature);
}

TEST_F(PlotExportTest, ACancelledFileQuestionWritesNothing)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    rocketplot::PlotWidget        plot;
    addThrustCurve(plot);
    services.prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(PlotExport::exportPlot(plot, services.services()));
    EXPECT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_TRUE(std::filesystem::is_empty(directory.path()));
}

TEST_F(PlotExportTest, AFailedWriteShowsOpenRocketsErrorWithTheFile)
{
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    rocketplot::PlotWidget        plot;
    addThrustCurve(plot);
    const std::filesystem::path file = directory.resolve("missing") / "plot.png";
    services.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = file,
                                    .directory          = file.parent_path(),
                                    .saveSimulationData = std::nullopt,
                                    .selectedNameFilter = filterAt(0)});
    {
        // rocketplot's own warning of the failed export.
        const Gui::Test::ExpectedQtMessage cannotWrite(
            QStringLiteral("^PlotWidget::exportImage: cannot write "));
        EXPECT_FALSE(PlotExport::exportPlot(plot, services.services()));
    }
    ASSERT_EQ(services.prompts().prompts().size(), 2U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message = services.prompts().prompts().back();
    EXPECT_EQ(message.kind, Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Error writing file"));
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("ERROR"),
                           QStringLiteral("An error occurred while writing to the file:"),
                           QtStrings::fromPath(file)}));
}

/// The questions of a test's ScriptedUserPrompts, of which the file question first destroys the
/// plot (its window closed while the modal dialog's event loop runs), then gives the scripted
/// answer.
class PlotClosingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    PlotClosingPrompts(UserPrompts& delegate, std::unique_ptr<rocketplot::PlotWidget>& plot)
      : ForwardingUserPrompts(delegate), m_plot(&plot)
    {
    }

    // The parent is not passed on: it has gone.
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget* /*parent*/,
                                                            const SaveFileRequest& request) override
    {
        m_plot->reset();
        return delegate().askSaveFile(nullptr, request);
    }

private:
    std::unique_ptr<rocketplot::PlotWidget>* m_plot;
};

TEST_F(PlotExportTest, APlotGoneDuringTheFileQuestionIsLeftAlone)
{
    // Nothing of the plot is used once it has gone (ASan: the plot read after the question), and
    // nothing is written or asked.
    Gui::Test::TestGuiServices              services;
    const QtRocket::Test::TempDir           directory;
    std::unique_ptr<rocketplot::PlotWidget> plot = std::make_unique<rocketplot::PlotWidget>();
    addThrustCurve(*plot);
    services.prompts().answerSaveFile(saveAnswer(directory, "plot", filterAt(0)));
    PlotClosingPrompts prompts(services.prompts(), plot);
    EXPECT_FALSE(
        PlotExport::exportPlot(*plot, Gui::Test::withPrompts(services.services(), prompts)));
    EXPECT_EQ(plot, nullptr);
    EXPECT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_TRUE(std::filesystem::is_empty(directory.path()));
}

TEST_F(PlotExportTest, ASeriesLegendEntrysMenuHasNoColorChoice)
{
    const Gui::Test::TestGuiServices services;
    rocketplot::PlotWidget           plot;
    addThrustCurve(plot);
    plot.legend()->setVisible(true);
    PlotExport::install(plot, services.services());
    // rocketplot's entry menu without Color...: Theme color stays (the curve has a colour of its
    // own), as do its line width and marker submenus.
    EXPECT_EQ(legendMenuTexts(plot),
              (QStringList{QStringLiteral("Visible"), QStringLiteral("Show only this"),
                           QStringLiteral("Show all"), QStringLiteral("Theme color"),
                           QStringLiteral("Line width"), QStringLiteral("Marker"),
                           QStringLiteral("Remove")}));
}

TEST_F(PlotExportTest, AnAnnotationLegendEntrysMenuHasNoColorChoice)
{
    const Gui::Test::TestGuiServices services;
    rocketplot::PlotWidget           plot;
    plot.resize(kPlotWidth, kPlotHeight);
    rocketplot::ReferenceLine* const limit =
        plot.addHorizontalLine(5.0, rocketplot::literalText(QStringLiteral("Limit")));
    limit->setLegendName(rocketplot::literalText(QStringLiteral("Limit")));
    plot.legend()->setVisible(true);
    PlotExport::install(plot, services.services());
    EXPECT_EQ(legendMenuTexts(plot),
              (QStringList{QStringLiteral("Visible"), QStringLiteral("Remove")}));
}

TEST_F(PlotExportTest, AMenuWithoutRocketplotsExportGetsQtRocketsAtTheEnd)
{
    const Gui::Test::TestGuiServices services;
    rocketplot::PlotWidget           plot;
    QMenu                            menu;
    menu.addAction(QStringLiteral("Back"));
    PlotExport::replaceExportAction(menu, plot, services.services());
    EXPECT_EQ(actionTexts(menu),
              (QStringList{QStringLiteral("Back"), QStringLiteral("Export\u2026")}));
    EXPECT_EQ(lastActionName(menu), PlotExport::exportActionName());
}

TEST_F(PlotExportTest, EveryColorChoiceIsRemoved)
{
    QMenu menu;
    menu.addAction(QStringLiteral("Visible"));
    menu.addAction(QStringLiteral("Color\u2026"));
    menu.addAction(QStringLiteral("Remove"));
    menu.addAction(QStringLiteral("Color\u2026"));
    PlotExport::removeColorActions(menu);
    EXPECT_EQ(actionTexts(menu),
              (QStringList{QStringLiteral("Visible"), QStringLiteral("Remove")}));
}

TEST_F(PlotExportTest, ReadsTheSuffixesOfANameFilter)
{
    EXPECT_EQ(PlotExport::suffixes(QStringLiteral("PNG image (*.png)")),
              QStringList{QStringLiteral("png")});
    EXPECT_EQ(PlotExport::suffixes(QStringLiteral("Images (*.png *.JPG)")),
              (QStringList{QStringLiteral("png"), QStringLiteral("JPG")}));
    EXPECT_EQ(PlotExport::suffixes(QStringLiteral("*.ork")), QStringList{QStringLiteral("ork")});
    EXPECT_TRUE(PlotExport::suffixes(QStringLiteral("All Files (*)")).isEmpty());
    // Wildcards after the dot name no suffix.
    EXPECT_TRUE(PlotExport::suffixes(QStringLiteral("Backups (*.b?k *.*)")).isEmpty());
}

TEST_F(PlotExportTest, GivesANameTheSuffixOfTheKindChosenUnlessItHasOneOfAKind)
{
    const QStringList           filters = PlotExport::nameFilters();
    const std::filesystem::path plot    = pathFromUtf8("dir/plot");
    EXPECT_EQ(PlotExport::withSuffix(plot, filters, filterAt(2)), pathFromUtf8("dir/plot.svg"));
    EXPECT_EQ(PlotExport::withSuffix(plot, filters, QString()), pathFromUtf8("dir/plot.png"));
    // A filter that is none of the request's: the first's.
    EXPECT_EQ(PlotExport::withSuffix(plot, filters, QStringLiteral("Text (*.txt)")),
              pathFromUtf8("dir/plot.png"));
    // The suffix of any kind, in any case, stays.
    EXPECT_EQ(PlotExport::withSuffix(pathFromUtf8("dir/plot.PDF"), filters, filterAt(0)),
              pathFromUtf8("dir/plot.PDF"));
    // FileHelper.forceExtension(): the text after the last dot, none when it is the first
    // character.
    EXPECT_EQ(PlotExport::withSuffix(pathFromUtf8("dir/plot.v2"), filters, filterAt(4)),
              pathFromUtf8("dir/plot.v2.csv"));
    EXPECT_EQ(PlotExport::withSuffix(pathFromUtf8("dir/.png"), filters, filterAt(0)),
              pathFromUtf8("dir/.png.png"));
    // No filter names a suffix: the name as it is.
    EXPECT_EQ(PlotExport::withSuffix(plot, QStringList{QStringLiteral("All Files (*)")}, QString()),
              plot);
}

}  // namespace

}  // namespace QtRocket::Gui
