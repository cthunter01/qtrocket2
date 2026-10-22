#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtCore/qtestsupport_core.h>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/FinPointTableModel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointFigure.h"
#include "QtRocket/gui/figure/FinPointScrollArea.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/CustomFinImporter.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DropdownButton.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "figure/FinPointTestSupport.h"
#include "util/FinImportReport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FreeformFinSetConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::finImage;
using Test::finPointsText;
using Test::focusOrder;
using Test::FreeformDesign;
using Test::hasLinearSlider;
using Test::missingNumberRows;
using Test::OffscreenPopupWarnings;
using Test::ScriptedUserPrompts;
using Test::showActive;
using Test::tabTitles;
using Test::takesFromZeroUp;

/// The default outline as the fin set gives it.
constexpr const char* kDefaultPoints =
    "(0.0,0.0) (0.025,0.05) (0.075,0.05) (0.04999999999999999,0.0)";

TEST_F(FreeformFinSetConfigTest, ShowsJavasTabs)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(
        tabTitles(*panel),
        (QStringList{QStringLiteral("General"), QStringLiteral("Shape"), QStringLiteral("Fin tabs"),
                     QStringLiteral("Override"), QStringLiteral("Comment")}));
    const QTabWidget& tabs = panel->tabs();
    EXPECT_EQ(tabs.tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(tabs.tabToolTip(1), QStringLiteral("Fin shape"));
    EXPECT_EQ(tabs.currentIndex(), 0);
    EXPECT_EQ(&panel->shapeTab(), tabs.widget(1));
    // "Split fins", no "Convert to freeform".
    ASSERT_NE(panel->splitButton(), nullptr);
    EXPECT_EQ(panel->convertButton(), nullptr);
}

TEST_F(FreeformFinSetConfigTest, TheGeneralTabHasJavasRows)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    const QWidget&                           general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"CantAngle", "Thickness", "AxialOffset", "BaseRotation",
                                          "FilletRadius"}),
              "");
    ASSERT_NE(panel->finCountSpinBox(), nullptr);
    EXPECT_TRUE(panel->finCountSpinBox()->toolTip().isEmpty());
    ASSERT_NE(panel->crossSectionComboBox(), nullptr);
    EXPECT_EQ(
        panel->cantWidgets().label->toolTip(),
        QStringLiteral("The angle that the fins are canted with respect to the rocket body."));
    EXPECT_TRUE(takesFromZeroUp(panel->thicknessWidgets()));
    EXPECT_TRUE(hasLinearSlider(panel->thicknessWidgets(), 0, 0.01));
}

/// The texts of the hints below the figure.
[[nodiscard]] QStringList hintTexts(const FreeformFinSetConfig& panel)
{
    QStringList hints;
    for (const StyledLabel* label : panel.shapeTab().findChildren<StyledLabel*>())
    {
        hints.append(label->text());
    }
    return hints;
}

TEST_F(FreeformFinSetConfigTest, TheShapeTabHoldsTheEditor)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    EXPECT_EQ(panel->pointTable().model(), &panel->pointTableModel());
    EXPECT_EQ(panel->pointTable().objectName(), QStringLiteral("FinPoints"));
    EXPECT_EQ(panel->figureArea().figure(), &panel->finPointFigure());
    EXPECT_EQ(panel->pointActionsButton().text(), QStringLiteral("Point Actions\u2026  | \u25BC "));
    // Java's order (Scale Fin, Milestone 5, would come first).
    EXPECT_EQ(panel->pointActionsButton().popupMenu().actions(),
              (QList<QAction*>{&panel->importImageAction(), &panel->exportCsvAction()}));
    EXPECT_EQ(panel->importImageAction().text(), QStringLiteral("Import from image"));
    EXPECT_EQ(panel->exportCsvAction().text(), QStringLiteral("Export CSV"));
    EXPECT_EQ(hintTexts(*panel),
              (QStringList{QStringLiteral("Ctrl+click: Delete point"),
                           QStringLiteral("Click+drag: Add and move points"),
                           QStringLiteral("Shift+click+drag: Lock angle to previous point"),
                           QStringLiteral("Ctrl+shift+click+drag: Lock angle to following "
                                          "point")}));
    EXPECT_EQ(panel->finPointUndoButton().toolTip(), QStringLiteral("Undo last point edit"));
    EXPECT_EQ(panel->finPointRedoButton().toolTip(), QStringLiteral("Redo last point edit"));
    EXPECT_EQ(panel->finPointUndoButton().focusPolicy(), Qt::NoFocus);
    EXPECT_FALSE(panel->finPointUndoButton().isEnabled());
    EXPECT_FALSE(panel->finPointRedoButton().isEnabled());
    EXPECT_EQ(panel->liveUpdateCheckBox().text(), QStringLiteral("Live update the rocket"));
    EXPECT_FALSE(panel->liveUpdateCheckBox().isChecked());
    EXPECT_EQ(panel->tablePointMenu().actions(),
              (QList<QAction*>{&panel->insertPointAction(), &panel->deletePointAction()}));
    EXPECT_EQ(panel->figurePointMenu().actions(), QList<QAction*>{&panel->deletePointAction()});
}

TEST_F(FreeformFinSetConfigTest, TheLiveUpdateCheckBoxIsThePreference)
{
    ConfigDialogServices services;
    services.preferences().putBoolean(GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, true);
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    EXPECT_TRUE(panel->liveUpdateCheckBox().isChecked());
    panel->liveUpdateCheckBox().click();
    EXPECT_FALSE(services.preferences().getBoolean(
        GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, true));
}

TEST_F(FreeformFinSetConfigTest, ACellsTextMovesThePointAsOneEdit)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    FinPointTableModel&                      model = panel->pointTableModel();
    EXPECT_TRUE(model.setData(model.index(1, 0), QStringLiteral("3"), Qt::EditRole));
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.03);
    EXPECT_EQ(panel->finPointFigure().editHistory().size(), 2U);
    EXPECT_TRUE(panel->finPointUndoButton().isEnabled());
    // A refused move: the point stays, and the history keeps no edit.
    EXPECT_TRUE(model.setData(model.index(1, 0), QStringLiteral("300"), Qt::EditRole));
    EXPECT_EQ(panel->finPointFigure().editHistory().size(), 2U);
}

TEST_F(FreeformFinSetConfigTest, APressOnACellEditsItWithItsTextSelected)
{
    const ConfigDialogServices services;
    const FreeformDesign       design;
    QWidget                    window;
    auto*                      layout = makeChild<QVBoxLayout>(&window);
    auto*                      panel =
        makeChild<FreeformFinSetConfig>(&window, design.document(), services.services(),
                                        std::make_shared<ComponentSelection>(design.fins()));
    layout->addWidget(panel);
    panel->setSelectedTabIndex(1);
    ASSERT_TRUE(showActive(window));
    const QTableView& table = panel->pointTable();
    const QPoint      cell  = table.visualRect(table.model()->index(1, 0)).center();
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, cell);
    auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->selectedText(), QStringLiteral("2.5"));
    QTest::keyClicks(editor, QStringLiteral("3"));
    QTest::keyClick(editor, Qt::Key_Return);
    // The delegate commits and closes the editor later (a queued call).
    QCoreApplication::processEvents();
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.03);
    EXPECT_EQ(panel->pointTableModel().valueText(1, 0), QStringLiteral("3"));
    // The Y of the first point is no editor's.
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier,
                      table.visualRect(table.model()->index(0, 1)).center());
    EXPECT_EQ(qobject_cast<QLineEdit*>(QApplication::focusWidget()), nullptr);
    panel->release();
}

TEST_F(FreeformFinSetConfigTest, InsertsAndDeletesTheSelectedPoint)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    panel->selectPointRow(1);
    EXPECT_TRUE(panel->insertPointAction().isEnabled());
    EXPECT_TRUE(panel->deletePointAction().isEnabled());
    panel->insertPointAction().trigger();
    EXPECT_EQ(finPointsText(design.fins()),
              "(0.0,0.0) (0.025,0.05) (0.05,0.05) (0.075,0.05) (0.04999999999999999,0.0)");
    panel->updateFields();
    panel->selectPointRow(2);
    panel->deletePointAction().trigger();
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    EXPECT_EQ(panel->finPointFigure().editHistory().size(), 3U);
    // Neither the first nor the last point.
    panel->updateFields();
    panel->selectPointRow(0);
    EXPECT_TRUE(panel->insertPointAction().isEnabled());
    EXPECT_FALSE(panel->deletePointAction().isEnabled());
    panel->selectPointRow(3);
    EXPECT_FALSE(panel->insertPointAction().isEnabled());
    EXPECT_FALSE(panel->deletePointAction().isEnabled());
    // The table selects the figure's point.
    EXPECT_EQ(panel->finPointFigure().selectedIndex(), 3);
}

TEST_F(FreeformFinSetConfigTest, TheButtonsUndoAndRedoAPointEdit)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    panel->selectPointRow(1);
    panel->insertPoint();
    EXPECT_EQ(design.fins().getPointCount(), 5U);
    panel->finPointUndoButton().click();
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    EXPECT_FALSE(panel->finPointUndoButton().isEnabled());
    EXPECT_TRUE(panel->finPointRedoButton().isEnabled());
    panel->finPointRedoButton().click();
    EXPECT_EQ(design.fins().getPointCount(), 5U);
}

TEST_F(FreeformFinSetConfigTest, NothingIsUndoneDuringADrag)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    panel->selectPointRow(1);
    panel->insertPoint();
    panel->figureArea().resize(600, 400);
    panel->figureArea().setFitting(true);
    const FinPointFigure& figure = panel->finPointFigure();
    const double          scale  = figure.absoluteScale();
    const QPoint          point(
        figure.subjectOrigin().x() + static_cast<int>(MathUtil::javaRound(0.025 * scale)),
        figure.subjectOrigin().y() - static_cast<int>(MathUtil::javaRound(0.05 * scale)));
    QTest::mousePress(&panel->finPointFigure(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(panel->dragIndex(), 1);
    EXPECT_FALSE(panel->finPointUndoButton().isEnabled());
    panel->performFinPointUndo();
    EXPECT_EQ(design.fins().getPointCount(), 5U);
    QTest::mouseRelease(&panel->finPointFigure(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_TRUE(panel->finPointUndoButton().isEnabled());
}

TEST_F(FreeformFinSetConfigTest, TheRightButtonOnTheTableSelectsTheRowAndShowsTheMenu)
{
    const ConfigDialogServices services;
    const FreeformDesign       design;
    QWidget                    window;
    auto*                      layout = makeChild<QVBoxLayout>(&window);
    auto*                      panel =
        makeChild<FreeformFinSetConfig>(&window, design.document(), services.services(),
                                        std::make_shared<ComponentSelection>(design.fins()));
    layout->addWidget(panel);
    panel->setSelectedTabIndex(1);
    ASSERT_TRUE(showActive(window));
    const QTableView& table = panel->pointTable();
    {
        const OffscreenPopupWarnings warnings;
        QTest::mouseClick(table.viewport(), Qt::RightButton, Qt::NoModifier,
                          table.visualRect(table.model()->index(2, 1)).center());
        EXPECT_EQ(panel->selectedPointRow(), 2);
        EXPECT_TRUE(panel->tablePointMenu().isVisible());
        EXPECT_EQ(qobject_cast<QLineEdit*>(QApplication::focusWidget()), nullptr);
        panel->tablePointMenu().hide();
    }
    // Below the rows: nothing.
    QTest::mouseClick(table.viewport(), Qt::RightButton, Qt::NoModifier,
                      QPoint(5, table.viewport()->height() - 2));
    EXPECT_FALSE(panel->tablePointMenu().isVisible());
    EXPECT_EQ(panel->selectedPointRow(), 2);
    // The middle button does nothing.
    QTest::mouseClick(table.viewport(), Qt::MiddleButton, Qt::NoModifier,
                      table.visualRect(table.model()->index(1, 0)).center());
    EXPECT_EQ(panel->selectedPointRow(), 2);
    panel->release();
}

TEST_F(FreeformFinSetConfigTest, ARightClickOnAPointShowsTheFiguresMenu)
{
    const ConfigDialogServices services;
    const FreeformDesign       design;
    QWidget                    window;
    auto*                      layout = makeChild<QVBoxLayout>(&window);
    auto*                      panel =
        makeChild<FreeformFinSetConfig>(&window, design.document(), services.services(),
                                        std::make_shared<ComponentSelection>(design.fins()));
    layout->addWidget(panel);
    panel->setSelectedTabIndex(1);
    ASSERT_TRUE(showActive(window));
    const FinPointFigure& figure = panel->finPointFigure();
    const double          scale  = figure.absoluteScale();
    const QPoint          point(
        figure.subjectOrigin().x() + static_cast<int>(MathUtil::javaRound(0.025 * scale)),
        figure.subjectOrigin().y() - static_cast<int>(MathUtil::javaRound(0.05 * scale)));
    {
        const OffscreenPopupWarnings warnings;
        QTest::mouseClick(&panel->finPointFigure(), Qt::RightButton, Qt::NoModifier, point);
        EXPECT_TRUE(panel->figurePointMenu().isVisible());
        panel->figurePointMenu().hide();
    }
    EXPECT_EQ(panel->selectedPointRow(), 1);
    EXPECT_TRUE(panel->deletePointAction().isEnabled());
    panel->deletePointAction().trigger();
    EXPECT_EQ(design.fins().getPointCount(), 3U);
    panel->release();
}

TEST_F(FreeformFinSetConfigTest, EditsTheOtherSelectedFinSetsAtTheirPoints)
{
    const ConfigDialogServices services;
    const FreeformDesign       design(
        {FreeformDesign::Outline{},
         FreeformDesign::Outline{0, 0, 0.115, 0.072, 0.255, 0.072, 0.255, 0.037, 0.150, 0}});
    const std::array<RocketComponent*, 1>    others{&design.fins(1)};
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins(0),
                                                   others);
    FinPointTableModel&                      model = panel->pointTableModel();
    EXPECT_TRUE(model.setData(model.index(3, 0), QStringLiteral("6"), Qt::EditRole));
    // The main fin set's last point is the other's last.
    EXPECT_EQ(design.fins(0).getFinPoints().at(3).x, 0.06);
    EXPECT_EQ(design.fins(1).getFinPoints().at(4).x, 0.06);
    // The history's undo sets the main fin set's snapshot on both (Java's setPoints()).
    panel->performFinPointUndo();
    EXPECT_EQ(finPointsText(design.fins(1)), kDefaultPoints);
}

TEST_F(FreeformFinSetConfigTest, ExportsTheTableAsCsv)
{
    const QtRocket::Test::TempDir directory;
    ConfigDialogServices          services;
    const FreeformDesign          design;
    services.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory.resolve("fin"),
                                    .directory          = directory.path(),
                                    .saveSimulationData = std::nullopt});
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    panel->exportCsvAction().trigger();
    const Result<std::string> text = readTextFile(directory.resolve("fin.csv"));
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "X / cm, Y / cm, \n0, 0, \n2.5, 5, \n7.5, 5, \n5, 0, \n");
    EXPECT_EQ(services.preferences().getDefaultDirectory(), directory.path());
    ASSERT_EQ(services.prompts().saveFileRequests().size(), 1U);
    EXPECT_EQ(services.prompts().saveFileRequests().front().nameFilters,
              QStringList{QStringLiteral("Comma Separated Files (*.csv)")});

    // An existing file: overwritten only when the user says so.
    ASSERT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    services.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory.resolve("fin.csv"),
                                    .directory          = directory.path(),
                                    .saveSimulationData = std::nullopt});
    services.prompts().answerOverwrite(false);
    panel->exportCsvAction().trigger();
    EXPECT_EQ(*readTextFile(directory.resolve("fin.csv")), *text);
    EXPECT_EQ(services.prompts().prompts().back().kind, ScriptedUserPrompts::Kind::OVERWRITE);
    EXPECT_EQ(services.prompts().prompts().back().texts, QStringList{QStringLiteral("fin.csv")});
}

TEST_F(FreeformFinSetConfigTest, ShowsAFailedExport)
{
    const QtRocket::Test::TempDir directory;
    ConfigDialogServices          services;
    const FreeformDesign          design;
    services.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory.resolve("missing/fin.csv"),
                                    .directory          = directory.path(),
                                    .saveSimulationData = std::nullopt});
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    panel->exportCsv();
    ASSERT_FALSE(services.prompts().prompts().empty());
    EXPECT_EQ(services.prompts().prompts().back().kind, ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(services.prompts().prompts().back().title, QStringLiteral("Error writing file"));
}

TEST_F(FreeformFinSetConfigTest, ForcesTheCsvExtension)
{
    // FileHelper.forceExtension(): unless the name ends with ".csv" (ignoring case).
    EXPECT_EQ(FreeformFinSetConfig::forceExtension(pathFromUtf8("dir/fin"), "csv"),
              pathFromUtf8("dir/fin.csv"));
    EXPECT_EQ(FreeformFinSetConfig::forceExtension(pathFromUtf8("dir/fin.CSV"), "csv"),
              pathFromUtf8("dir/fin.CSV"));
    EXPECT_EQ(FreeformFinSetConfig::forceExtension(pathFromUtf8("dir/a.b.csv"), "csv"),
              pathFromUtf8("dir/a.b.csv"));
    EXPECT_EQ(FreeformFinSetConfig::forceExtension(pathFromUtf8("dir/fin.txt"), "csv"),
              pathFromUtf8("dir/fin.txt.csv"));
    // A name that starts with its only dot has no extension.
    EXPECT_EQ(FreeformFinSetConfig::forceExtension(pathFromUtf8("dir/.csv"), "csv"),
              pathFromUtf8("dir/.csv.csv"));
}

/// CustomFinImport.description.
constexpr const char* kImportDescription =
    "The image will be converted internally to black and white image (black for the fin), so "
    "make sure you use a solid dark color for the fin, and white or a light color for the "
    "background. The fin must be touching the bottom of the image, which is the base of the fin.";

/// The answer of the open dialog that chose @p image of tests/data/fin-images.
[[nodiscard]] UserPrompts::OpenFileAnswer finImageAnswer(std::string_view image)
{
    return {.files = {finImage(image)}, .directory = finImage("").parent_path()};
}

TEST_F(FreeformFinSetConfigTest, ImportFromImageAsksForAnImageFileInTheDefaultDirectory)
{
    const QtRocket::Test::TempDir directory;
    ConfigDialogServices          services;
    services.preferences().setDefaultDirectory(directory.path());
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    services.prompts().answerOpenFiles(std::nullopt);
    panel->importImageAction().trigger();
    ASSERT_EQ(services.prompts().openFileRequests().size(), 1U);
    const UserPrompts::OpenFileRequest& request = services.prompts().openFileRequests().front();
    EXPECT_EQ(request.nameFilters,
              (QStringList{CustomFinImporter::imageNameFilter(), QStringLiteral("All Files (*)")}));
    EXPECT_EQ(request.directory, directory.path());
    EXPECT_FALSE(request.multipleFiles);
    EXPECT_EQ(request.description, QString::fromLatin1(kImportDescription));
    // Cancelled: nothing changes, and the default directory stays.
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    EXPECT_FALSE(design.document().isUndoAvailable());
    EXPECT_FALSE(panel->finPointFigure().editHistory().canUndo());
    EXPECT_EQ(services.preferences().getDefaultDirectory(), directory.path());
}

TEST_F(FreeformFinSetConfigTest, ImportsAnImageAsOneEditOfTheDocumentAndOfTheEditor)
{
    // FinImportDumper's "import trapezoid": Java's points and undo steps.
    ConfigDialogServices                     services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    services.prompts().answerOpenFiles(finImageAnswer("trapezoid.png"));
    panel->importImageAction().trigger();
    constexpr const char* kImported =
        "(0.0,0.0) (0.02,0.039) (0.058,0.039) (0.03899999999999998,0.0)";
    EXPECT_EQ(finPointsText(design.fins()), kImported);
    EXPECT_EQ(panel->pointTableModel().rowCount(), 4);
    EXPECT_EQ(design.document().getUndoDescription(), "Import freeform fin set");
    EXPECT_EQ(services.preferences().getDefaultDirectory(), finImage("").parent_path());
    // The editor's own history has the import as one edit.
    EXPECT_TRUE(panel->finPointUndoButton().isEnabled());
    panel->performFinPointUndo();
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    panel->performFinPointRedo();
    EXPECT_EQ(finPointsText(design.fins()), kImported);
    // (FinImportPanelGolden compares the document's undo and redo of an import with Java's.)
}

TEST_F(FreeformFinSetConfigTest, AnImageWithoutAFinOnItsBottomRowShowsJavasError)
{
    const QtRocket::Test::TempDir directory;
    ConfigDialogServices          services;
    services.preferences().setDefaultDirectory(directory.path());
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    services.prompts().answerOpenFiles(finImageAnswer("floating.png"));
    panel->importImageAction().trigger();
    ASSERT_FALSE(services.prompts().prompts().empty());
    const ScriptedUserPrompts::Prompt& message = services.prompts().prompts().back();
    EXPECT_EQ(message.kind, ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Error loading fin profile"));
    EXPECT_EQ(
        message.texts,
        (QStringList{QStringLiteral("ERROR"),
                     QStringLiteral("Invalid fin image. Make sure the fin is a solid black or "
                                    "dark color and touching the bottom of the image.")}));
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    EXPECT_FALSE(design.document().isUndoAvailable());
    EXPECT_FALSE(panel->finPointFigure().editHistory().canUndo());
    EXPECT_FALSE(panel->finPointFigure().editHistory().isEditInProgress());
    // Java's finally: the chooser's directory is the default one, after the failure too.
    EXPECT_EQ(services.preferences().getDefaultDirectory(), finImage("").parent_path());
}

TEST_F(FreeformFinSetConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices               services;
    const FreeformDesign                     design;
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins());
    EXPECT_EQ(focusOrder(*panel->tabs().widget(0),
                         {"FinCount", "CantAngle", "CrossSection", "Thickness", "AxialMethod",
                          "AxialOffset", "BaseRotation", "FilletRadius", "FilletMaterial"}),
              (std::vector<std::string>{"FinCount", "CantAngle", "CrossSection", "Thickness",
                                        "AxialMethod", "AxialOffset", "BaseRotation",
                                        "FilletRadius", "FilletMaterial"}));
    // Of the Shape tab only the table (Java's "order" list).
    EXPECT_EQ(focusOrder(panel->shapeTab(), {"FinPoints", "pointActionsButton",
                                             "finPointScrollArea", "UpdateRocketWhileDraggingPoint",
                                             "finPointUndoButton", "finPointRedoButton"}),
              std::vector<std::string>{"FinPoints"});
    EXPECT_EQ(focusOrder(*panel->tabs().widget(2), {"TabLength", "TabHeight", "TabOffset",
                                                    "TabOffsetMethod", "autoCalcButton"}),
              (std::vector<std::string>{"TabLength", "TabHeight", "TabOffset", "TabOffsetMethod",
                                        "autoCalcButton"}));
}

TEST_F(FreeformFinSetConfigTest, TheMainComponentMustBeAFreeformFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_THROW(static_cast<void>(makeChild<FreeformFinSetConfig>(&host, *design.document,
                                                                   services.services(), selection)),
                 BugError);
}

/// The dialog of @p design's fin set for @p window, on its Shape tab and active.
[[nodiscard]] FreeformFinSetConfig* openShapeTab(const ConfigDialogServices& services,
                                                 const FreeformDesign& design, QWidget& window)
{
    ComponentConfigDialog::showDialog(window, services.services(), design.document(),
                                      design.fins());
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    if (dialog == nullptr)
    {
        return nullptr;
    }
    auto* panel = qobject_cast<FreeformFinSetConfig*>(&dialog->configurator());
    if (panel != nullptr)
    {
        panel->setSelectedTab(QStringLiteral("Shape"));
    }
    dialog->activateWindow();
    // The dialog itself, not only its window chain (QWindow::isActive() is true for the dialog of
    // an active design window).
    return QTest::qWaitFor([dialog] { return QApplication::activeWindow() == dialog; }) ? panel
                                                                                        : nullptr;
}

TEST_F(FreeformFinSetConfigTest, CtrlZAndYUndoAPointEditInTheShapeTabOnly)
{
    // The editor's keys are the Shape tab's (Qt::WidgetWithChildrenShortcut); the design
    // window's Undo and Redo (window shortcuts) still act in the window.
    const ConfigDialogServices services;
    const FreeformDesign       design;
    MainWindow                 window(design.document(), services.services());
    ASSERT_TRUE(showActive(window));
    const QSignalSpy      documentUndo(window.menus().undoAction(), &QAction::triggered);
    FreeformFinSetConfig* panel = openShapeTab(services, design, window);
    ASSERT_NE(panel, nullptr);
    panel->selectPointRow(1);
    panel->insertPoint();
    ASSERT_EQ(design.fins().getPointCount(), 5U);

    panel->pointTable().setFocus();
    QTest::keyClick(&panel->pointTable(), Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(design.fins().getPointCount(), 4U);
    QTest::keyClick(&panel->pointTable(), Qt::Key_Y, Qt::ControlModifier);
    EXPECT_EQ(design.fins().getPointCount(), 5U);
    EXPECT_EQ(documentUndo.count(), 0);

    // Outside the Shape tab the keys are not the editor's.
    panel->setSelectedTab(QStringLiteral("Fin tabs"));
    panel->autoCalcButton().setFocus();
    QTest::keyClick(&panel->autoCalcButton(), Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(design.fins().getPointCount(), 5U);

    // In the design window, the document's Undo, which closes the dialog.
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitFor([&window] { return QApplication::activeWindow() == &window; }));
    QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(documentUndo.count(), 1);
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// The figure's pixel of the fin point (@p x, @p y) of @p panel.
[[nodiscard]] QPoint figurePixel(const FreeformFinSetConfig& panel, double x, double y)
{
    const FinPointFigure& figure = panel.finPointFigure();
    const double          scale  = figure.absoluteScale();
    return {figure.subjectOrigin().x() + static_cast<int>(MathUtil::javaRound(x * scale)),
            figure.subjectOrigin().y() - static_cast<int>(MathUtil::javaRound(y * scale))};
}

TEST_F(FreeformFinSetConfigTest, AnUndoDuringAFrozenDragClosesTheDialogAtTheNextMove)
{
    // The undo's event waits in the drag's freeze; the next move ends the drag, which thaws the
    // rocket, and the event closes the dialog (its panel goes later).
    const ConfigDialogServices services;
    const FreeformDesign       design;
    design.document().addUndoPosition("Move");
    ASSERT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    QWidget window;
    ASSERT_TRUE(showActive(window));
    const FreeformFinSetConfig* panel = openShapeTab(services, design, window);
    ASSERT_NE(panel, nullptr);
    QWidget&     figure = panel->finPointFigure();
    const QPoint start  = figurePixel(*panel, 0.03, 0.05);
    QTest::mousePress(&figure, Qt::LeftButton, Qt::NoModifier, start);
    ASSERT_EQ(panel->dragIndex(), 1);
    ASSERT_TRUE(design.rocket().isFrozen());
    design.document().undo();
    QTest::mouseMove(&figure, start + QPoint(10, 10));
    EXPECT_FALSE(design.rocket().isFrozen());
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QTest::mouseRelease(&figure, Qt::LeftButton, Qt::NoModifier, start + QPoint(10, 10));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(design.fins().getFinPoints().at(1).x, 0.025);
}

TEST_F(FreeformFinSetConfigTest, AnUndoDuringALiveDragClosesTheDialogAtOnce)
{
    ConfigDialogServices services;
    services.preferences().putBoolean(GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, true);
    const FreeformDesign design;
    design.document().addUndoPosition("Move");
    ASSERT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    QWidget window;
    ASSERT_TRUE(showActive(window));
    const FreeformFinSetConfig* panel = openShapeTab(services, design, window);
    ASSERT_NE(panel, nullptr);
    QWidget&     figure = panel->finPointFigure();
    const QPoint start  = figurePixel(*panel, 0.03, 0.05);
    QTest::mousePress(&figure, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&figure, start + QPoint(5, 5));
    design.document().undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_FALSE(panel->figureArea().isDragging());
    QTest::mouseRelease(&figure, Qt::LeftButton, Qt::NoModifier, start + QPoint(5, 5));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_FALSE(design.rocket().isFrozen());
}

TEST_F(FreeformFinSetConfigTest, TheDialogRefreshesTheTableOnEveryChange)
{
    const ConfigDialogServices services;
    const FreeformDesign       design;
    QWidget                    window;
    ASSERT_TRUE(showActive(window));
    const FreeformFinSetConfig* panel = openShapeTab(services, design, window);
    ASSERT_NE(panel, nullptr);
    ASSERT_TRUE(design.fins().setPoint(1, 0.03, 0.05).has_value());
    EXPECT_EQ(panel->pointTableModel().valueText(1, 0), QStringLiteral("3"));
    EXPECT_EQ(panel->selectedPointRow(), -1);
    ComponentConfigDialog::disposeDialog();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

TEST_F(FreeformFinSetConfigTest, ImportsIntoTheDialogsFinSet)
{
    ConfigDialogServices services;
    const FreeformDesign design;
    QWidget              window;
    ASSERT_TRUE(showActive(window));
    const FreeformFinSetConfig* panel = openShapeTab(services, design, window);
    ASSERT_NE(panel, nullptr);
    services.prompts().answerOpenFiles(finImageAnswer("rectangle.png"));
    panel->importImageAction().trigger();
    EXPECT_EQ(finPointsText(design.fins()),
              "(0.0,0.0) (0.0,0.019) (0.029,0.019) (0.029000000000000026,0.0)");
    // The dialog's refresh shows the new points.
    EXPECT_EQ(panel->pointTableModel().rowCount(), 4);
    EXPECT_EQ(panel->pointTableModel().valueText(2, 0), QStringLiteral("2.9"));
    EXPECT_EQ(design.document().getUndoDescription(), "Import freeform fin set");
    // The document's undo closes the dialog and gives the fin its points back.
    design.document().undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(finPointsText(design.fins()), kDefaultPoints);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

}  // namespace

}  // namespace QtRocket::Gui
