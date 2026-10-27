// The parts library (Java: ComponentPresetChooserDialog, swing gui/dialogs/preset/). OpenRocket has
// no JUnit test of it. What OpenRocket's own dialog shows for the components of two designs over
// the bundled presets, as a window and in its table, is pinned in PresetChooserPins.h
// (tools/openrocket-gui-pins/preset-chooser/probe/PresetChooserProbe.java) and compared in
// preset_chooser_golden_tests.cpp; these tests are about how the dialog behaves, over the presets
// of smallPresetDatabase(): its 12 body tubes, none of them legacy, all of Estes, 30615 and 85866
// 50.8 mm wide, 31361 48.768 mm, 31380, 31390 and 85865 63.5 mm; one of its two streamers is
// legacy.

#include "QtRocket/gui/dialogs/preset/ComponentPresetChooserDialog.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"
#include "TestGuiServices.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentPresetChooserDialogTest = Gui::Test::GuiTest;
using Dialog                           = ComponentPresetChooserDialog;
using Request                          = UserPrompts::PresetRequest;
using Outcome                          = UserPrompts::PresetOutcome;
using Gui::Test::NextModalDialog;
using Gui::Test::TestGuiServices;

/// The diameter of the body tubes 30615 and 85866 (31361, 48.768 mm, is within its 5 %).
constexpr double kTubeDiameter = 0.0508;
/// The diameter of the body tubes 31380, 31390 and 85865.
constexpr double kWideTubeDiameter = 0.0635;
/// The size of the main window of WindowChain.
constexpr int kMainWidth  = 1000;
constexpr int kMainHeight = 800;

[[nodiscard]] Request request(ComponentKind kind, std::optional<double> fore = std::nullopt,
                              std::optional<double> aft = std::nullopt, bool flipped = false)
{
    return {.componentKind   = kind,
            .foreDiameter    = fore,
            .aftDiameter     = aft,
            .flippedNoseCone = flipped};
}

/// Test services whose presets are the small database's.
struct ChooserServices
{
    TestGuiServices test;

    ChooserServices() { test.presets() = QtRocket::Test::smallPresetDatabase(); }

    [[nodiscard]] const GuiServices&   services() const noexcept { return test.services(); }
    [[nodiscard]] InMemoryPreferences& preferences() noexcept { return test.preferences(); }
};

/// The number of rows the dialog's table shows.
[[nodiscard]] int rows(const Dialog& dialog)
{
    return dialog.table().filterModel().rowCount();
}

/// The keys of the "tables" node (the stored table layouts), sorted.
[[nodiscard]] std::vector<std::string> tableKeys(const Preferences& preferences)
{
    const Preferences* const node = preferences.findNode(GuiPreferenceKeys::kNodeTables);
    return node == nullptr ? std::vector<std::string>{} : node->keys();
}

/// A main window of kMainWidth x kMainHeight with a configuration dialog holding a panel: the
/// parent the parts library is asked for (Java: the dialog's owner is the configuration dialog,
/// whose owner is the main window).
struct WindowChain
{
    QWidget  main;
    QDialog* config = makeChild<QDialog>(&main);
    QWidget* panel  = makeChild<QWidget>(config);

    WindowChain() { main.resize(kMainWidth, kMainHeight); }
};

/// Double-clicks (press, release, double click, release) the cell of view row @p row in view
/// column @p viewColumn of the shown dialog's table, then presses Escape when the dialog is still
/// open, and tells whether it was (@p stillOpen).
struct DoubleClickCell
{
    int   row;
    int   viewColumn;
    bool* stillOpen;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<Dialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        ComponentPresetTable& table = dialog->table();
        const int         column = table.visibleColumns().at(static_cast<std::size_t>(viewColumn));
        const QModelIndex index  = table.filterModel().index(row, column);
        table.scrollTo(index);
        const QPoint point = table.visualRect(index).center();
        QTest::mousePress(table.viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QTest::mouseRelease(table.viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QTest::mouseDClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QTest::mouseRelease(table.viewport(), Qt::LeftButton, Qt::NoModifier, point);
        *stillOpen = dialog->isVisible();
        if (*stillOpen)
        {
            QTest::keyClick(&table, Qt::Key_Escape);
        }
    }
};

/// Selects view row @p row of the shown dialog's table (-1: none), then presses @p key in the
/// widget @p objectName, then Escape when the dialog is still open; tells whether it was and the
/// table's selected row then.
struct PressKey
{
    int         row;
    const char* objectName;
    Qt::Key     key;
    bool*       stillOpen;
    int*        selectedRow;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<Dialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        if (row >= 0)
        {
            dialog->table().selectRow(row);
        }
        auto* const target = dialog->findChild<QWidget*>(QString::fromLatin1(objectName));
        ASSERT_NE(target, nullptr) << objectName;
        target->setFocus();
        QTest::keyClick(target, key);
        *stillOpen   = dialog->isVisible();
        *selectedRow = dialog->table().selectionModel()->hasSelection()
                           ? dialog->table().selectionModel()->selectedRows().front().row()
                           : -1;
        if (*stillOpen)
        {
            QTest::keyClick(&dialog->table(), Qt::Key_Escape);
        }
    }
};

/// Destroys @p parent, the dialog's parent, while the dialog is open.
struct DestroyParent
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget& /*dialog*/) const { parent->reset(); }
};

/// Closes the shown dialog through its window's close button (QWidget::close()).
struct CloseWindow
{
    void operator()(QWidget& widget) const { static_cast<void>(widget.close()); }
};

/// Selects view row @p row of the shown dialog's table and clicks Close.
struct SelectAndClose
{
    int row;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<Dialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->table().selectRow(row);
        dialog->closeButton().click();
    }
};

TEST_F(ComponentPresetChooserDialogTest, ShowsJavasTextsUnderJavasNames)
{
    ChooserServices services;
    const Dialog    dialog(services.services(), request(ComponentKind::BODY_TUBE));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("ComponentPresetChooserDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Choose component preset"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_FALSE(dialog.isVisible());

    EXPECT_EQ(dialog.filterLabel().text(), QStringLiteral("Filter by text:"));
    EXPECT_EQ(dialog.filterLabel().objectName(), QStringLiteral("filterLabel"));
    EXPECT_EQ(dialog.filterText().objectName(), QStringLiteral("filterText"));
    EXPECT_TRUE(dialog.filterText().text().isEmpty());
    EXPECT_EQ(dialog.filterText().minimumWidth(), Dialog::kFilterTextMinimumWidth);
    EXPECT_EQ(dialog.table().objectName(), QStringLiteral("componentSelectionTable"));
    EXPECT_EQ(dialog.showLegacyCheckBox().text(), QStringLiteral("Show Legacy Database"));
    EXPECT_EQ(dialog.showLegacyCheckBox().objectName(), QStringLiteral("showLegacyCheckBox"));
    EXPECT_TRUE(dialog.showLegacyCheckBox().toolTip().isEmpty());
    EXPECT_FALSE(dialog.showLegacyCheckBox().isChecked());
    // No diameters asked for: no diameter boxes.
    EXPECT_EQ(dialog.foreDiameterFilterCheckBox(), nullptr);
    EXPECT_EQ(dialog.aftDiameterFilterCheckBox(), nullptr);
    EXPECT_EQ(dialog.favoritesLabel().text(),
              QStringLiteral("<html>\u2191 Check to add preset to the preset drop-down menu in the "
                             "component edit dialog <br>Directly apply a preset by double-clicking "
                             "it or by selecting it and closing this window.</html>"));
    EXPECT_EQ(dialog.favoritesLabel().objectName(), QStringLiteral("favoritesLabel"));
    EXPECT_EQ(dialog.favoritesLabel().textFormat(), Qt::RichText);
    EXPECT_EQ(dialog.alwaysOpenPresetCheckBox().text(),
              QStringLiteral("Always open this dialog when creating a new Body Tube"));
    EXPECT_EQ(dialog.alwaysOpenPresetCheckBox().objectName(), QStringLiteral("alwaysOpenPreset"));
    EXPECT_TRUE(dialog.alwaysOpenPresetCheckBox().isChecked());
    EXPECT_EQ(dialog.closeButton().text(), QStringLiteral("Close"));
    EXPECT_EQ(dialog.closeButton().objectName(), QStringLiteral("closeButton"));
    EXPECT_TRUE(dialog.closeButton().isDefault());
    // The focus starts in the text field.
    EXPECT_EQ(dialog.focusWidget(), &dialog.filterText());

    EXPECT_EQ(rows(dialog), 12);
    EXPECT_EQ(dialog.answer().outcome, Outcome::CLOSED);
    EXPECT_EQ(dialog.answer().preset, nullptr);
    // Opening stores nothing.
    EXPECT_TRUE(tableKeys(services.preferences()).empty());
    EXPECT_FALSE(services.preferences().get(
        GuiPreferenceKeys::alwaysOpenPresetKey(ComponentKind::BODY_TUBE)));
}

TEST_F(ComponentPresetChooserDialogTest, ListsThePresetsOfTheKindsPresetType)
{
    const ChooserServices services;
    // An inner tube takes body tubes; its name is its own.
    const Dialog inner(services.services(), request(ComponentKind::INNER_TUBE));
    EXPECT_EQ(rows(inner), 12);
    EXPECT_EQ(inner.alwaysOpenPresetCheckBox().text(),
              QStringLiteral("Always open this dialog when creating a new Inner Tube"));
    // One of the two streamers is legacy, which the dialog hides as it opens.
    const Dialog streamers(services.services(), request(ComponentKind::STREAMER));
    EXPECT_EQ(rows(streamers), 1);
    EXPECT_EQ(streamers.table().tableModel().rowCount(), 2);
    // A kind that takes no presets.
    EXPECT_THROW(Dialog(services.services(), request(ComponentKind::FREEFORM_FIN_SET)), BugError);
    EXPECT_THROW(static_cast<void>(Dialog::choose(nullptr, services.services(),
                                                  request(ComponentKind::MASS_COMPONENT))),
                 BugError);
}

TEST_F(ComponentPresetChooserDialogTest, ADiameterBoxNeedsItsDiameterAndTheTypesColumn)
{
    ChooserServices services;
    // A body tube between two symmetric components: both boxes, the fore one first, below the
    // legacy box.
    const Dialog tube(services.services(),
                      request(ComponentKind::BODY_TUBE, kTubeDiameter, kWideTubeDiameter));
    const QCheckBox* const fore = tube.foreDiameterFilterCheckBox();
    const QCheckBox* const aft  = tube.aftDiameterFilterCheckBox();
    ASSERT_NE(fore, nullptr);
    ASSERT_NE(aft, nullptr);
    EXPECT_EQ(fore->text(), QStringLiteral("Match fore diameter"));
    EXPECT_EQ(fore->toolTip(),
              QStringLiteral("Search for components with the same fore diameter as the previous "
                             "component"));
    EXPECT_EQ(fore->objectName(), QStringLiteral("foreDiameterFilterCheckBox"));
    EXPECT_EQ(aft->text(), QStringLiteral("Match aft diameter"));
    EXPECT_EQ(aft->toolTip(),
              QStringLiteral("Search for components with the same aft diameter as the next "
                             "component"));
    EXPECT_EQ(aft->objectName(), QStringLiteral("aftDiameterFilterCheckBox"));
    EXPECT_TRUE(fore->isChecked());
    EXPECT_TRUE(aft->isChecked());
    const QWidget* const boxes = tube.showLegacyCheckBox().parentWidget();
    ASSERT_NE(boxes, nullptr);
    ASSERT_NE(boxes->layout(), nullptr);
    EXPECT_EQ(boxes->layout()->indexOf(&tube.showLegacyCheckBox()), 0);
    EXPECT_EQ(boxes->layout()->indexOf(fore), 1);
    EXPECT_EQ(boxes->layout()->indexOf(aft), 2);

    // A nose cone's type has no fore diameter column: no fore box, whatever the request says.
    const Dialog nose(services.services(),
                      request(ComponentKind::NOSE_CONE, kTubeDiameter, kTubeDiameter));
    EXPECT_EQ(nose.foreDiameterFilterCheckBox(), nullptr);
    ASSERT_NE(nose.aftDiameterFilterCheckBox(), nullptr);
    EXPECT_EQ(nose.aftDiameterFilterCheckBox()->text(), QStringLiteral("Match aft diameter"));
    // A flipped nose cone's aft box has the fore box's texts.
    const Dialog flipped(services.services(),
                         request(ComponentKind::NOSE_CONE, std::nullopt, kTubeDiameter, true));
    ASSERT_NE(flipped.aftDiameterFilterCheckBox(), nullptr);
    EXPECT_EQ(flipped.aftDiameterFilterCheckBox()->text(), QStringLiteral("Match fore diameter"));
    EXPECT_EQ(flipped.aftDiameterFilterCheckBox()->toolTip(),
              QStringLiteral("Search for components with the same fore diameter as the previous "
                             "component"));
    EXPECT_EQ(flipped.aftDiameterFilterCheckBox()->objectName(),
              QStringLiteral("aftDiameterFilterCheckBox"));
    // A streamer's type has no diameter columns at all.
    const Dialog streamer(services.services(),
                          request(ComponentKind::STREAMER, kTubeDiameter, kTubeDiameter));
    EXPECT_EQ(streamer.foreDiameterFilterCheckBox(), nullptr);
    EXPECT_EQ(streamer.aftDiameterFilterCheckBox(), nullptr);

    // The boxes start as the preferences MatchForeDiameter and MatchAftDiameter say.
    services.preferences().setMatchForeDiameter(false);
    const Dialog unticked(services.services(),
                          request(ComponentKind::BODY_TUBE, kTubeDiameter, kWideTubeDiameter));
    ASSERT_NE(unticked.foreDiameterFilterCheckBox(), nullptr);
    ASSERT_NE(unticked.aftDiameterFilterCheckBox(), nullptr);
    EXPECT_FALSE(unticked.foreDiameterFilterCheckBox()->isChecked());
    EXPECT_TRUE(unticked.aftDiameterFilterCheckBox()->isChecked());
}

TEST_F(ComponentPresetChooserDialogTest, EachDiameterBoxFiltersTheRowsAndStoresItsPreference)
{
    ChooserServices services;
    services.preferences().setMatchAftDiameter(false);
    const Dialog dialog(services.services(),
                        request(ComponentKind::BODY_TUBE, kTubeDiameter, kWideTubeDiameter));
    const ComponentPresetFilterModel& filter = dialog.table().filterModel();
    QCheckBox* const                  fore   = dialog.foreDiameterFilterCheckBox();
    QCheckBox* const                  aft    = dialog.aftDiameterFilterCheckBox();
    ASSERT_NE(fore, nullptr);
    ASSERT_NE(aft, nullptr);
    // At open: the fore box ticked (30615, 31361, 85866), the aft box not.
    EXPECT_EQ(filter.foreDiameterFilter(), std::optional<double>(kTubeDiameter));
    EXPECT_EQ(filter.aftDiameterFilter(), std::nullopt);
    EXPECT_EQ(rows(dialog), 3);
    // Opening stored nothing; each toggle stores its box's state.
    EXPECT_FALSE(services.preferences().get("MatchForeDiameter"));
    fore->setChecked(false);
    EXPECT_EQ(filter.foreDiameterFilter(), std::nullopt);
    EXPECT_EQ(rows(dialog), 12);
    EXPECT_FALSE(services.preferences().isMatchForeDiameter());
    aft->click();
    EXPECT_EQ(filter.aftDiameterFilter(), std::optional<double>(kWideTubeDiameter));
    EXPECT_EQ(rows(dialog), 3);
    EXPECT_TRUE(services.preferences().isMatchAftDiameter());
    fore->click();
    EXPECT_EQ(rows(dialog), 0);
    EXPECT_TRUE(services.preferences().isMatchForeDiameter());
}

TEST_F(ComponentPresetChooserDialogTest, TheTextFieldFiltersTheRowsOnEveryChange)
{
    const ChooserServices services;
    const Dialog          dialog(services.services(), request(ComponentKind::BODY_TUBE));
    QTest::keyClicks(&dialog.filterText(), QStringLiteral("8586"));
    EXPECT_EQ(dialog.table().filterModel().textFilter(), QStringLiteral("8586"));
    EXPECT_EQ(rows(dialog), 4);
    QTest::keyClick(&dialog.filterText(), Qt::Key_7);
    EXPECT_EQ(rows(dialog), 1);
    dialog.filterText().clear();
    EXPECT_EQ(dialog.table().filterModel().textFilter(), QString());
    EXPECT_EQ(rows(dialog), 12);
}

TEST_F(ComponentPresetChooserDialogTest, TheLegacyBoxAndTheLegacyColumnFollowEachOther)
{
    const ChooserServices services;
    const Dialog          dialog(services.services(), request(ComponentKind::STREAMER));
    ComponentPresetTable& table  = dialog.table();
    QCheckBox&            legacy = dialog.showLegacyCheckBox();
    EXPECT_FALSE(table.isLegacyShown());
    legacy.click();
    EXPECT_TRUE(table.isLegacyShown());
    EXPECT_TRUE(table.isColumnVisible(ComponentPresetTableModel::kLegacyColumn));
    EXPECT_EQ(rows(dialog), 2);
    // The header menu's Legacy item.
    table.setColumnVisible(ComponentPresetTableModel::kLegacyColumn, false);
    EXPECT_FALSE(legacy.isChecked());
    EXPECT_EQ(rows(dialog), 1);
    table.setColumnVisible(ComponentPresetTableModel::kLegacyColumn, true);
    EXPECT_TRUE(legacy.isChecked());
    EXPECT_EQ(rows(dialog), 2);
    legacy.setChecked(false);
    EXPECT_FALSE(table.isColumnVisible(ComponentPresetTableModel::kLegacyColumn));
    EXPECT_EQ(rows(dialog), 1);
}

TEST_F(ComponentPresetChooserDialogTest, AStoredLayoutThatShowsLegacyStillOpensWithoutIt)
{
    ChooserServices services;
    services.preferences()
        .getNode(GuiPreferenceKeys::kNodeTables)
        .putBoolean(GuiPreferenceKeys::tableColumnVisibilityKey("CmpPrst.Streamer", "Legacy"),
                    true);
    const Dialog dialog(services.services(), request(ComponentKind::STREAMER));
    EXPECT_FALSE(dialog.showLegacyCheckBox().isChecked());
    EXPECT_FALSE(dialog.table().isLegacyShown());
    EXPECT_FALSE(dialog.table().isColumnVisible(ComponentPresetTableModel::kLegacyColumn));
    EXPECT_EQ(rows(dialog), 1);
}

TEST_F(ComponentPresetChooserDialogTest, AlwaysOpenThisDialogIsStoredOnEveryClick)
{
    ChooserServices   services;
    const std::string key = GuiPreferenceKeys::alwaysOpenPresetKey(ComponentKind::LAUNCH_LUG);
    EXPECT_EQ(key, "Launch LugAlwaysOpenPreset");
    {
        const Dialog dialog(services.services(), request(ComponentKind::LAUNCH_LUG));
        QCheckBox&   box = dialog.alwaysOpenPresetCheckBox();
        EXPECT_TRUE(box.isChecked());
        box.click();
        EXPECT_EQ(services.preferences().get(key), std::optional<std::string>("false"));
        box.click();
        EXPECT_EQ(services.preferences().get(key), std::optional<std::string>("true"));
        // Only a click stores it (Java's ActionListener).
        box.setChecked(false);
        EXPECT_EQ(services.preferences().get(key), std::optional<std::string>("true"));
    }
    services.preferences().putBoolean(key, false);
    const Dialog dialog(services.services(), request(ComponentKind::LAUNCH_LUG));
    EXPECT_FALSE(dialog.alwaysOpenPresetCheckBox().isChecked());
    // Another kind's preference is its own.
    const Dialog tube(services.services(), request(ComponentKind::BODY_TUBE));
    EXPECT_TRUE(tube.alwaysOpenPresetCheckBox().isChecked());
}

TEST_F(ComponentPresetChooserDialogTest, CloseStoresTheLayoutAndAnswersTheSelectedPreset)
{
    ChooserServices services;
    const Dialog    dialog(services.services(), request(ComponentKind::BODY_TUBE));
    dialog.table().selectRow(2);
    const std::shared_ptr<const ComponentPreset> selected = dialog.table().selectedPreset();
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->getPartNo(), "31361");
    dialog.closeButton().click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.answer().outcome, Outcome::CHOSEN);
    EXPECT_EQ(dialog.answer().preset, selected);
    // The layout of the shown columns and the visibility of every column (Legacy hidden).
    const std::vector<std::string> keys = tableKeys(services.preferences());
    EXPECT_EQ(keys.size(), 22U);
    const Preferences& tables = services.preferences().getNode(GuiPreferenceKeys::kNodeTables);
    EXPECT_EQ(tables.get("CmpPrst.Body Tube.co.Manufacturer"), std::optional<std::string>("1"));
    EXPECT_EQ(tables.get("CmpPrst.Body Tube.cv.Legacy"), std::optional<std::string>("false"));
    EXPECT_TRUE(tables.get("CmpPrst.Body Tube.cw.Description").has_value());
}

TEST_F(ComponentPresetChooserDialogTest, CloseWithoutASelectionAnswersNone)
{
    ChooserServices services;
    const Dialog    dialog(services.services(), request(ComponentKind::BODY_TUBE));
    dialog.closeButton().click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.answer().outcome, Outcome::NONE);
    EXPECT_EQ(dialog.answer().preset, nullptr);
    EXPECT_EQ(tableKeys(services.preferences()).size(), 22U);
}

TEST_F(ComponentPresetChooserDialogTest, ADoubleClickAppliesARowWithoutStoringTheLayout)
{
    ChooserServices           services;
    bool                      stillOpen = true;
    UserPrompts::PresetAnswer answer;
    {
        const NextModalDialog click(
            DoubleClickCell{.row = 1, .viewColumn = 1, .stillOpen = &stillOpen});
        answer = Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
    }
    EXPECT_FALSE(stillOpen);
    EXPECT_EQ(answer.outcome, Outcome::CHOSEN);
    ASSERT_NE(answer.preset, nullptr);
    EXPECT_EQ(answer.preset->getPartNo(), "31360");
    EXPECT_TRUE(tableKeys(services.preferences()).empty());
}

TEST_F(ComponentPresetChooserDialogTest, ADoubleClickInTheFirstShownColumnDoesNothing)
{
    const ChooserServices     services;
    bool                      stillOpen = false;
    UserPrompts::PresetAnswer answer;
    {
        const NextModalDialog click(
            DoubleClickCell{.row = 1, .viewColumn = 0, .stillOpen = &stillOpen});
        answer = Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
    }
    // Escape closed it afterwards.
    EXPECT_TRUE(stillOpen);
    EXPECT_EQ(answer.outcome, Outcome::CLOSED);
    EXPECT_EQ(answer.preset, nullptr);
}

TEST_F(ComponentPresetChooserDialogTest, EscapeAndTheWindowsCloseButtonCloseWithoutAPreset)
{
    ChooserServices services;
    bool            stillOpen   = true;
    int             selectedRow = -1;
    {
        // Escape in the table, a row selected: the table does not keep the key.
        const NextModalDialog           escape(PressKey{.row         = 3,
                                                        .objectName  = "componentSelectionTable",
                                                        .key         = Qt::Key_Escape,
                                                        .stillOpen   = &stillOpen,
                                                        .selectedRow = &selectedRow});
        const UserPrompts::PresetAnswer answer =
            Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
        EXPECT_EQ(answer.outcome, Outcome::CLOSED);
        EXPECT_EQ(answer.preset, nullptr);
    }
    EXPECT_FALSE(stillOpen);
    {
        const NextModalDialog           close{CloseWindow{}};
        const UserPrompts::PresetAnswer answer =
            Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
        EXPECT_EQ(answer.outcome, Outcome::CLOSED);
        EXPECT_EQ(answer.preset, nullptr);
    }
    EXPECT_TRUE(tableKeys(services.preferences()).empty());
}

TEST_F(ComponentPresetChooserDialogTest, EnterPressesCloseButNotInTheTable)
{
    ChooserServices services;
    bool            stillOpen   = true;
    int             selectedRow = -1;
    {
        // Enter in the text field: Close, the default button, with view row 2 selected (probe:
        // DialogKeysProbe, "Enter in the filter field" applies the selected row's preset).
        const NextModalDialog           enter(PressKey{.row         = 2,
                                                       .objectName  = "filterText",
                                                       .key         = Qt::Key_Return,
                                                       .stillOpen   = &stillOpen,
                                                       .selectedRow = &selectedRow});
        const UserPrompts::PresetAnswer answer =
            Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
        EXPECT_EQ(answer.outcome, Outcome::CHOSEN);
        ASSERT_NE(answer.preset, nullptr);
        EXPECT_EQ(answer.preset->getPartNo(), "31361");
    }
    EXPECT_FALSE(stillOpen);
    EXPECT_EQ(tableKeys(services.preferences()).size(), 22U);
    {
        // Enter in the table moves to the next row (JTable's selectNextRowCell); Escape closes
        // (DialogKeysProbe: "Enter", "Escape in the table").
        const NextModalDialog           enter(PressKey{.row         = 2,
                                                       .objectName  = "componentSelectionTable",
                                                       .key         = Qt::Key_Return,
                                                       .stillOpen   = &stillOpen,
                                                       .selectedRow = &selectedRow});
        const UserPrompts::PresetAnswer answer =
            Dialog::choose(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
        EXPECT_EQ(answer.outcome, Outcome::CLOSED);
    }
    EXPECT_TRUE(stillOpen);
    EXPECT_EQ(selectedRow, 3);
}

TEST_F(ComponentPresetChooserDialogTest, OpensAtSeventyPercentOfTheMainWindow)
{
    const ChooserServices services;
    WindowChain           chain;
    const Dialog dialog(services.services(), request(ComponentKind::BODY_TUBE), chain.panel);
    EXPECT_EQ(dialog.size(), QSize(700, 560));
    // Without a main window above the parent's window: the contents' size.
    const Dialog packed(services.services(), request(ComponentKind::BODY_TUBE), &chain.main);
    EXPECT_NE(packed.size(), QSize(700, 560));
    const Dialog alone(services.services(), request(ComponentKind::BODY_TUBE));
    EXPECT_EQ(alone.size(), packed.size());
}

TEST_F(ComponentPresetChooserDialogTest, ARememberedGeometryComesFirst)
{
    ChooserServices services;
    {
        QWidget shown;
        shown.setGeometry(100, 120, 640, 480);
        shown.show();
        WindowGeometry::save(services.preferences(), QString::fromLatin1(Dialog::kGeometryName),
                             shown);
    }
    const WindowChain chain;
    const Dialog      dialog(services.services(), request(ComponentKind::BODY_TUBE), chain.panel);
    EXPECT_EQ(dialog.size(), QSize(640, 480));
}

TEST_F(ComponentPresetChooserDialogTest, WidgetUserPromptsAsksThroughIt)
{
    const ChooserServices           services;
    WidgetUserPrompts               prompts;
    const NextModalDialog           close(SelectAndClose{.row = 4});
    const UserPrompts::PresetAnswer answer =
        prompts.choosePreset(nullptr, services.services(), request(ComponentKind::BODY_TUBE));
    EXPECT_EQ(answer.outcome, Outcome::CHOSEN);
    ASSERT_NE(answer.preset, nullptr);
    EXPECT_EQ(answer.preset->getPartNo(), "31390");
}

TEST_F(ComponentPresetChooserDialogTest, AParentDestroyedWhileItIsOpenGivesClosed)
{
    const ChooserServices           services;
    auto                            parent = std::make_unique<QWidget>();
    QWidget* const                  raw    = parent.get();
    const NextModalDialog           destroy(DestroyParent{.parent = &parent});
    const UserPrompts::PresetAnswer answer =
        Dialog::choose(raw, services.services(), request(ComponentKind::BODY_TUBE));
    EXPECT_EQ(parent, nullptr);
    EXPECT_EQ(answer.outcome, Outcome::CLOSED);
    EXPECT_EQ(answer.preset, nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
