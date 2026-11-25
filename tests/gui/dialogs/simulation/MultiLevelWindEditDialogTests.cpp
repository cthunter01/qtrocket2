// The wind editor (OpenRocket's MultiLevelWindEditDialog): its controls are the walk of Java's
// dialog by the M4 sim-config scout (handoff/m4-scout, sim-config's java/dialog-walk.txt, "=== wind
// editor", OpenRocket 5f164fd0e under xvfb), pinned by hand, and its layout the relations of
// Java's dialog at 900 x 500 (the scout's walk and a layout probe of part S2: the table's 618 of
// 900 pixels to the left of the visualization's 277, the controls below the table, "Reset levels"
// below "Delete level", "Import levels" a paragraph gap to the right, Close at the bottom right);
// its buttons act as Java's, "Delete level" also with the mouse (OpenRocket bug 33), and its
// closing commits a pending edit.

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindEditDialog.h"

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QSize>
#include <QSplitter>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"
#include "QtRocket/gui/plot/WindProfilePlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "dialogs/simulation/WindEditorTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MultiLevelWindEditDialogTest = Gui::Test::GuiTest;
using Model                        = MultiLevelWindTableModel;

/// The levels 0, 500 and 1250.5 m of the probe tools/openrocket-probes/wind-editor.
MultiLevelPinkNoiseWindModel& threeLevels(MultiLevelPinkNoiseWindModel& model)
{
    model.clearLevels();
    EXPECT_TRUE(model.addWindLevel(0, 3, std::numbers::pi / 2, 0.3).has_value());
    EXPECT_TRUE(model.addWindLevel(500, 7.5, std::numbers::pi, 1.0).has_value());
    EXPECT_TRUE(model.addWindLevel(1250.5, 12.25, 0.0, 3.0).has_value());
    return model;
}

/// The editor of three levels with the tests' services (a fresh preference store, scripted
/// questions).
class WindEditor
{
public:
    [[nodiscard]] MultiLevelWindEditDialog&     dialog() noexcept { return m_dialog; }
    [[nodiscard]] MultiLevelPinkNoiseWindModel& wind() noexcept { return m_wind; }
    [[nodiscard]] Gui::Test::TestGuiServices&   services() noexcept { return m_services; }

    /// The middle of the table's cell @p row, @p column, in its viewport.
    [[nodiscard]] QPoint cell(int row, int column)
    {
        return m_dialog.table().visualRect(m_dialog.tableModel().index(row, column)).center();
    }

    /// A click of the left button on the table's cell.
    void click(int row, int column)
    {
        QTest::mouseClick(m_dialog.table().viewport(), Qt::LeftButton, {}, cell(row, column));
    }

    /// The table's open editor as its spin box (null, with a test failure, when none is open).
    [[nodiscard]] UnitSpinBox* editor()
    {
        auto* const spinBox = qobject_cast<UnitSpinBox*>(m_dialog.table().openEditor());
        if (spinBox == nullptr)
        {
            ADD_FAILURE() << "no cell editor is open";
        }
        return spinBox;
    }

private:
    Gui::Test::TestGuiServices   m_services;
    MultiLevelPinkNoiseWindModel m_wind{m_services.preferences()};
    MultiLevelWindEditDialog     m_dialog{threeLevels(m_wind), m_services.services()};
};

/// The geometry of @p widget in @p dialog's coordinates.
[[nodiscard]] QRect geometryIn(const QWidget& widget, const QWidget& dialog)
{
    return {widget.mapTo(&dialog, QPoint(0, 0)), widget.size()};
}

/// The child of @p dialog named @p name (a test failure when there is none).
[[nodiscard]] const QWidget& childNamed(const QWidget& dialog, const QString& name)
{
    const auto* const child = dialog.findChild<QWidget*>(name);
    if (child == nullptr)
    {
        ADD_FAILURE() << "no child " << QtStrings::toUtf8(name);
        return *dialog.window();
    }
    return *child;
}

/// No button of @p dialog is or becomes the default (Java's dialog has none).
void expectNoDefaultButton(const QDialog& dialog)
{
    for (const QPushButton* const button : dialog.findChildren<QPushButton*>())
    {
        EXPECT_FALSE(button->autoDefault()) << QtStrings::toUtf8(button->text());
        EXPECT_FALSE(button->isDefault()) << QtStrings::toUtf8(button->text());
    }
}

}  // namespace

TEST_F(MultiLevelWindEditDialogTest, TheDialogShowsOpenRocketsControls)
{
    WindEditor                      editor;
    const MultiLevelWindEditDialog& dialog = editor.dialog();
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Wind Profile Editor"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.size(), QSize(900, 500));
    EXPECT_EQ(dialog.minimumSize(), QSize(700, 400));
    // The scout's walk of Java's dialog, below the table and in the visualization's panel.
    EXPECT_EQ(Gui::Test::dialogWalk(childNamed(dialog, QStringLiteral("controlsPanel"))),
              (std::vector<std::string>{
                  "LABEL Altitude reference: tip=Set to which altitude the wind levels refer to.",
                  "RADIO Mean Sea Level (MSL) selected=true",
                  "RADIO Above Ground Level (AGL) selected=false",
                  "BUTTON Add new level icon",
                  "BUTTON Delete level icon disabled tip=Delete the selected wind level",
                  "BUTTON Import levels icon tip=Import wind levels from a CSV file",
                  "BUTTON Reset levels tip=Reset wind levels to default",
              }));
    EXPECT_EQ(Gui::Test::dialogWalk(childNamed(dialog, QStringLiteral("visPanel"))),
              (std::vector<std::string>{
                  "LABEL Wind Profile Visualization",
                  "CHECK Show wind direction vectors selected=true",
              }));
    EXPECT_EQ(dialog.closeButton().text(), QStringLiteral("Close"));
    EXPECT_TRUE(dialog.closeButton().icon().isNull());
    // Java: StyledLabel(..., 1.5f, BOLD).
    EXPECT_TRUE(dialog.visualizationTitle().font().bold());
    // No button is the default: Enter presses none.
    expectNoDefaultButton(dialog);
}

TEST_F(MultiLevelWindEditDialogTest, TheLayoutIsJavas)
{
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    const QRect table    = geometryIn(dialog.table(), dialog);
    const QRect profile  = geometryIn(dialog.visualization(), dialog);
    const QRect controls = geometryIn(childNamed(dialog, QStringLiteral("controlsPanel")), dialog);
    const QRect add      = geometryIn(dialog.addButton(), dialog);
    const QRect remove   = geometryIn(dialog.deleteButton(), dialog);
    const QRect import   = geometryIn(dialog.importButton(), dialog);
    const QRect reset    = geometryIn(dialog.resetButton(), dialog);
    const QRect close    = geometryIn(dialog.closeButton(), dialog);
    const QRect msl      = geometryIn(dialog.mslButton(), dialog);
    const QRect title    = geometryIn(dialog.visualizationTitle(), dialog);
    // Java: the table (618 pixels) left of the visualization (277).
    EXPECT_LT(table.right(), profile.left());
    EXPECT_GT(table.width(), profile.width());
    EXPECT_GE(controls.top(), table.bottom());
    EXPECT_LT(msl.bottom(), add.top());
    EXPECT_EQ(add.top(), remove.top());
    EXPECT_EQ(remove.top(), import.top());
    EXPECT_LT(add.right(), remove.left());
    // "gapleft para": a wider gap before "Import levels" than between the others.
    EXPECT_GT(import.left() - remove.right(), remove.left() - add.right());
    EXPECT_EQ(reset.left(), remove.left());
    EXPECT_EQ(reset.width(), remove.width());
    EXPECT_GT(reset.top(), remove.bottom());
    EXPECT_LT(title.bottom(), profile.top() + 1);
    EXPECT_GT(close.top(), table.bottom());
    EXPECT_GT(close.left(), profile.left());
    EXPECT_LE(close.right(), dialog.width());
}

TEST_F(MultiLevelWindEditDialogTest, TheAltitudeReferenceButtonsSetTheModels)
{
    WindEditor                      editor;
    const MultiLevelWindEditDialog& dialog = editor.dialog();
    dialog.aglButton().click();
    EXPECT_EQ(editor.wind().getAltitudeReference(), WindModel::AltitudeReference::AGL);
    EXPECT_TRUE(dialog.aglButton().isChecked());
    EXPECT_EQ(
        dialog.tableModel().headerData(Model::kAltitudeColumn, Qt::Horizontal, Qt::DisplayRole),
        QStringLiteral("Altitude AGL"));
    dialog.mslButton().click();
    EXPECT_EQ(editor.wind().getAltitudeReference(), WindModel::AltitudeReference::MSL);
    // The buttons follow the model.
    editor.wind().setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_TRUE(dialog.aglButton().isChecked());
    EXPECT_FALSE(dialog.mslButton().isChecked());
}

TEST_F(MultiLevelWindEditDialogTest, AddNewLevelAddsALevelAboveTheHighest)
{
    WindEditor editor;
    editor.dialog().addButton().click();
    EXPECT_EQ(Gui::Test::windModelLine(editor.wind()), "MODEL 0.0 500.0 1250.5 1350.5");
}

TEST_F(MultiLevelWindEditDialogTest, DeleteLevelIsEnabledWithASelectionOfOneOfSeveralLevels)
{
    WindEditor                      editor;
    const MultiLevelWindEditDialog& dialog = editor.dialog();
    EXPECT_FALSE(dialog.deleteButton().isEnabled());
    dialog.table().selectRow(1);
    EXPECT_TRUE(dialog.deleteButton().isEnabled());
    dialog.deleteButton().click();
    EXPECT_EQ(Gui::Test::windModelLine(editor.wind()), "MODEL 0.0 1250.5");
    EXPECT_FALSE(dialog.deleteButton().isEnabled());
    dialog.table().selectRow(0);
    ASSERT_TRUE(dialog.tableModel().deleteLevel(1));
    // One level is left: it cannot be deleted.
    EXPECT_EQ(dialog.table().selectedRow(), 0);
    EXPECT_FALSE(dialog.deleteButton().isEnabled());
}

TEST_F(MultiLevelWindEditDialogTest, DeleteLevelDeletesTheSelectedLevelWithTheMouse)
{
    // OpenRocket bug 33: the press takes the focus from the cell's spinner, which drops Java's
    // selection, so that the button is disabled before it is released and deletes nothing.
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    editor.click(1, Model::kSpeedColumn);
    ASSERT_NE(dialog.table().openEditor(), nullptr);
    ASSERT_TRUE(dialog.deleteButton().isEnabled());
    QTest::mouseClick(&dialog.deleteButton(), Qt::LeftButton);
    EXPECT_EQ(Gui::Test::windModelLine(editor.wind()), "MODEL 0.0 1250.5");
}

TEST_F(MultiLevelWindEditDialogTest, ResetLevelsAsksFirst)
{
    WindEditor                      editor;
    const MultiLevelWindEditDialog& dialog = editor.dialog();
    editor.services().prompts().answerYesNo(false);
    dialog.resetButton().click();
    EXPECT_EQ(Gui::Test::windModelLine(editor.wind()), "MODEL 0.0 500.0 1250.5");
    ASSERT_EQ(editor.services().prompts().prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& question =
        editor.services().prompts().prompts().front();
    EXPECT_EQ(question.kind, Gui::Test::ScriptedUserPrompts::Kind::YES_NO);
    EXPECT_EQ(question.title, QStringLiteral("Overwrite wind levels?"));
    EXPECT_EQ(question.texts,
              (QStringList{QStringLiteral("WARNING"),
                           QStringLiteral("Your current wind levels will be overwritten. Do you "
                                          "want to continue?")}));
    editor.services().prompts().answerYesNo(true);
    dialog.resetButton().click();
    // The one level of the preferences' average wind.
    EXPECT_EQ(
        Gui::Test::windTableRowLines(dialog.tableModel(), editor.wind()),
        (std::vector<std::string>{
            "ROW 0 [0] [2] [90] [0.2] [10] intensity=[Medium] delete=disabled", "MODEL 0.0"}));
}

TEST_F(MultiLevelWindEditDialogTest, TheProfileShowsTheTablesUnitsAndSelection)
{
    WindEditor                      editor;
    const MultiLevelWindEditDialog& dialog = editor.dialog();
    const WindProfilePlot&          plot   = dialog.visualization();
    EXPECT_EQ(plot.selectedLevel(), nullptr);
    dialog.table().selectRow(2);
    // The level at 1250.5 m.
    EXPECT_EQ(plot.selectedLevel(), editor.wind().getLevels().at(2));
    dialog.table().clearSelection();
    EXPECT_EQ(plot.selectedLevel(), nullptr);
    dialog.table()
        .unitSelector(Model::kSpeedColumn)
        .setSelectedUnit(*unitGroup(UnitGroupId::WINDSPEED).getUnit("mph"));
    EXPECT_EQ(plot.units().speed->getUnit(), "mph");
    EXPECT_EQ(plot.units().altitude->getUnit(), "m");
}

TEST_F(MultiLevelWindEditDialogTest, CloseAndEscapeCloseTheDialog)
{
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    dialog.closeButton().click();
    EXPECT_FALSE(dialog.isVisible());
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    dialog.table().setFocus();
    QTest::keyClick(&dialog.table(), Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
}

TEST_F(MultiLevelWindEditDialogTest, EscapeInACellDropsItsEditOnly)
{
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    editor.click(0, Model::kSpeedColumn);
    UnitSpinBox* const spinBox = editor.editor();
    ASSERT_NE(spinBox, nullptr);
    Gui::Test::typeInto(*spinBox, QStringLiteral("9"));
    QTest::keyClick(spinBox, Qt::Key_Escape);
    EXPECT_TRUE(QTest::qWaitFor([&dialog] { return dialog.table().openEditor() == nullptr; }));
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.tableModel().cellText(0, Model::kSpeedColumn), QStringLiteral("3"));
}

TEST_F(MultiLevelWindEditDialogTest, ClosingCommitsAPendingEdit)
{
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    editor.click(2, Model::kAltitudeColumn);
    UnitSpinBox* const spinBox = editor.editor();
    ASSERT_NE(spinBox, nullptr);
    Gui::Test::typeInto(*spinBox, QStringLiteral("2000"));
    // Java's spinner commits when the closing dialog takes its focus (a macOS button takes none).
    dialog.done(QDialog::Accepted);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(Gui::Test::windModelLine(editor.wind()), "MODEL 0.0 500.0 2000.0");
}

TEST_F(MultiLevelWindEditDialogTest, EnterPressesNoButton)
{
    WindEditor                editor;
    MultiLevelWindEditDialog& dialog = editor.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    dialog.resetButton().setFocus();
    QTest::keyClick(&dialog.resetButton(), Qt::Key_Return);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_TRUE(editor.services().prompts().prompts().empty());
}

TEST_F(MultiLevelWindEditDialogTest, TheModelMayGoWhileTheDialogShowsIt)
{
    Gui::Test::TestGuiServices services;
    auto wind = std::make_unique<MultiLevelPinkNoiseWindModel>(services.preferences());
    const MultiLevelWindEditDialog dialog(threeLevels(*wind), services.services());
    wind.reset();
    EXPECT_FALSE(dialog.tableModel().hasModel());
    EXPECT_FALSE(dialog.visualization().hasModel());
    dialog.aglButton().click();
    dialog.addButton().click();
    dialog.table().selectRow(1);
    dialog.deleteButton().click();
    services.prompts().answerYesNo(true);
    dialog.resetButton().click();
    EXPECT_EQ(dialog.tableModel().rowCount(), 3);
    dialog.closeButton().click();
}

TEST_F(MultiLevelWindEditDialogTest, TheDialogMayGoWithAnEditOpen)
{
    // Under ASan: the editor, its binding and the table go with the dialog.
    Gui::Test::TestGuiServices   services;
    MultiLevelPinkNoiseWindModel wind(services.preferences());
    {
        MultiLevelWindEditDialog dialog(threeLevels(wind), services.services());
        ASSERT_TRUE(Gui::Test::showActive(dialog));
        QTest::mouseClick(dialog.table().viewport(), Qt::LeftButton, {},
                          dialog.table()
                              .visualRect(dialog.tableModel().index(0, Model::kAltitudeColumn))
                              .center());
        auto* const spinBox = qobject_cast<UnitSpinBox*>(dialog.table().openEditor());
        ASSERT_NE(spinBox, nullptr);
        Gui::Test::typeInto(*spinBox, QStringLiteral("-50"));
    }
    EXPECT_EQ(wind.getLevels().size(), 3U);
}

}  // namespace QtRocket::Gui
