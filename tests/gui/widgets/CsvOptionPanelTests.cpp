// The options of a CSV file being written (OpenRocket's CsvOptionPanel): its texts and tooltips
// as the export scout's Swing probe of the export tab shows them (swing-probe.txt), its values,
// its preferences and its two arrangements.

#include "QtRocket/gui/widgets/CsvOptionPanel.h"

#include <cstddef>
#include <string>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using CsvOptionPanelTest = Test::GuiTest;

/// The separator's tooltip (SimExpPan.lbl.longA1 + longA2), as the probe shows it.
[[nodiscard]] QString separatorTip()
{
    return QStringLiteral(
        "<html>The string used to separate the fields in the exported file. "
        "<br>Use ',' for a Comma Separated Values (CSV) file.");
}

/// The simulation data export's three comment options (SimulationExportPanel.create()).
[[nodiscard]] std::vector<CsvOptionPanel::CommentOption> exportOptions()
{
    return {{.text    = QStringLiteral("Include simulation description"),
             .toolTip = QStringLiteral(
                 "Include a comment at the beginning of the file describing the simulation.")},
            {.text    = QStringLiteral("Include field descriptions"),
             .toolTip = QStringLiteral(
                 "Include a comment line with the descriptions of the exported variables.")},
            {.text    = QStringLiteral("Include flight events"),
             .toolTip = QStringLiteral("Include a comment line for every flight event.")}};
}

/// The export tab's panel.
class ExportOptions
{
public:
    explicit ExportOptions(
        CsvOptionPanel::Arrangement arrangement = CsvOptionPanel::Arrangement::ON_NEW_ROWS)
      : m_panel(std::string(CsvOptionPreferences::kSimulationExportPanel), exportOptions(),
                arrangement, nullptr)
    {
    }

    [[nodiscard]] CsvOptionPanel& panel() noexcept { return m_panel; }

private:
    CsvOptionPanel m_panel;
};

/// The label whose buddy is @p field, among @p panel's children.
[[nodiscard]] const QLabel* labelOf(const CsvOptionPanel& panel, const QWidget* field)
{
    for (const QLabel* const label : panel.findChildren<QLabel*>())
    {
        if (label->buddy() == field)
        {
            return label;
        }
    }
    return nullptr;
}

/// The items of @p box.
[[nodiscard]] QStringList itemTexts(const QComboBox& box)
{
    QStringList items;
    for (int index = 0; index < box.count(); ++index)
    {
        items.append(box.itemText(index));
    }
    return items;
}

/// That the comment option @p index of @p panel is @p expected, in the Comments group.
void expectCommentOption(const CsvOptionPanel& panel, std::size_t index,
                         const CsvOptionPanel::CommentOption& expected)
{
    const QCheckBox* const checkBox = panel.optionCheckBox(index);
    EXPECT_EQ(checkBox->text(), expected.text);
    EXPECT_EQ(checkBox->toolTip(), expected.toolTip);
    EXPECT_EQ(checkBox->objectName(), QStringLiteral("options"));
    EXPECT_EQ(checkBox->parentWidget(), panel.commentsGroup());
}

/// Some values other than the defaults.
[[nodiscard]] CsvOptionValues someValues()
{
    CsvOptionValues values;
    values.fieldSeparator      = QStringLiteral("<->");
    values.decimalPlaces       = 7;
    values.exponentialNotation = false;
    values.commentCharacter    = QStringLiteral("//");
    values.commentOptions      = {false, true, false};
    return values;
}

}  // namespace

TEST_F(CsvOptionPanelTest, TheFormatSettingsAreJavas)
{
    ExportOptions         options;
    const CsvOptionPanel& panel = options.panel();
    EXPECT_EQ(panel.formatSettingsGroup()->title(), QStringLiteral("Format settings"));
    const FieldSeparatorComboBox* const separator = panel.fieldSeparatorComboBox();
    EXPECT_EQ(separator->objectName(), QStringLiteral("fieldSeparator"));
    EXPECT_EQ(separator->toolTip(), separatorTip());
    ASSERT_NE(labelOf(panel, separator), nullptr);
    EXPECT_EQ(labelOf(panel, separator)->text(), QStringLiteral("Field separator string:"));
    EXPECT_EQ(labelOf(panel, separator)->toolTip(), separatorTip());

    const QString decimalsTip = QStringLiteral(
        "The number of decimal places to use for the variables in the exported file.");
    const QSpinBox* const decimals = panel.decimalPlacesSpinBox();
    EXPECT_EQ(decimals->objectName(), QStringLiteral("decimalPlacesSpinner"));
    EXPECT_EQ(decimals->toolTip(), decimalsTip);
    // SpinnerNumberModel(value, 0, 15, 1).
    EXPECT_EQ(decimals->minimum(), 0);
    EXPECT_EQ(decimals->maximum(), 15);
    EXPECT_EQ(decimals->singleStep(), 1);
    ASSERT_NE(labelOf(panel, decimals), nullptr);
    EXPECT_EQ(labelOf(panel, decimals)->text(), QStringLiteral("Decimal places:"));
    EXPECT_EQ(labelOf(panel, decimals)->toolTip(), decimalsTip);

    const QCheckBox* const exponential = panel.exponentialNotationCheckBox();
    EXPECT_EQ(exponential->objectName(), QStringLiteral("exponentialNotationCheckbox"));
    EXPECT_EQ(exponential->text(), QStringLiteral("Use exponential notation"));
    EXPECT_EQ(exponential->toolTip(),
              QStringLiteral("Whether to use exponential notation for the variables (e.g. "
                             "1.5e-4) or not (e.g. 0.00015)."));
}

TEST_F(CsvOptionPanelTest, TheCommentsAreTheOwnersOptionsAndTheCommentCharacter)
{
    ExportOptions         options;
    const CsvOptionPanel& panel = options.panel();
    EXPECT_EQ(panel.commentsGroup()->title(), QStringLiteral("Comments"));
    ASSERT_EQ(panel.optionCount(), 3U);
    const std::vector<CsvOptionPanel::CommentOption> expected = exportOptions();
    expectCommentOption(panel, 0, expected[0]);
    expectCommentOption(panel, 1, expected[1]);
    expectCommentOption(panel, 2, expected[2]);
    EXPECT_THROW(static_cast<void>(panel.optionCheckBox(3)), BugError);
    EXPECT_THROW(static_cast<void>(panel.selectionOption(3)), BugError);

    // The probe: editable=true items=[#|%|;] selected='#', with its tooltip on the label too.
    const QString characterTip       = QStringLiteral("The character(s) that mark a comment line.");
    const QComboBox* const character = panel.commentCharacterComboBox();
    EXPECT_EQ(character->objectName(), QStringLiteral("commentCharacter"));
    EXPECT_EQ(itemTexts(*character),
              (QStringList{QStringLiteral("#"), QStringLiteral("%"), QStringLiteral(";")}));
    EXPECT_TRUE(character->isEditable());
    EXPECT_EQ(character->insertPolicy(), QComboBox::NoInsert);
    EXPECT_EQ(character->completer(), nullptr);
    EXPECT_EQ(character->toolTip(), characterTip);
    EXPECT_EQ(character->parentWidget(), panel.commentsGroup());
    ASSERT_NE(labelOf(panel, character), nullptr);
    EXPECT_EQ(labelOf(panel, character)->text(), QStringLiteral("Comment character:"));
    EXPECT_EQ(labelOf(panel, character)->toolTip(), characterTip);
}

TEST_F(CsvOptionPanelTest, StartsFromTheDefaults)
{
    // The probe's fresh store: ",", 3, exponential, "#", every option ticked.
    ExportOptions         options;
    const CsvOptionPanel& panel = options.panel();
    EXPECT_EQ(panel.baseClassName(), "SimulationExportPanel");
    EXPECT_EQ(panel.fieldSeparator(), QStringLiteral(","));
    EXPECT_EQ(panel.decimalPlaces(), 3);
    EXPECT_TRUE(panel.isExponentialNotation());
    EXPECT_EQ(panel.commentCharacter(), QStringLiteral("#"));
    EXPECT_TRUE(panel.selectionOption(0));
    EXPECT_TRUE(panel.selectionOption(1));
    EXPECT_TRUE(panel.selectionOption(2));
    EXPECT_EQ(panel.values(),
              CsvOptionPreferences::load(InMemoryPreferences(), panel.baseClassName(), 3));
}

TEST_F(CsvOptionPanelTest, ShowsAndGivesValues)
{
    ExportOptions   options;
    CsvOptionPanel& panel = options.panel();
    panel.setValues(someValues());
    EXPECT_EQ(panel.values(), someValues());
    EXPECT_EQ(panel.fieldSeparatorComboBox()->currentText(), QStringLiteral("<->"));
    EXPECT_EQ(panel.commentCharacterComboBox()->currentIndex(), -1);
    EXPECT_EQ(panel.commentCharacterComboBox()->currentText(), QStringLiteral("//"));

    // A space and a tab as their items, never their texts; an item's text as the item.
    CsvOptionValues values  = someValues();
    values.fieldSeparator   = QStringLiteral("\t");
    values.commentCharacter = QStringLiteral(";");
    panel.setValues(values);
    EXPECT_EQ(panel.fieldSeparatorComboBox()->currentText(), QStringLiteral("TAB"));
    EXPECT_EQ(panel.commentCharacterComboBox()->currentIndex(), 2);
    EXPECT_EQ(panel.values(), values);

    // Decimal places outside the spinner's range are clamped.
    values.decimalPlaces = 99;
    panel.setValues(values);
    EXPECT_EQ(panel.decimalPlaces(), 15);

    // One comment option for each of the panel's.
    values.commentOptions = {true};
    EXPECT_THROW(panel.setValues(values), BugError);
}

TEST_F(CsvOptionPanelTest, LoadsAndStoresItsPreferences)
{
    InMemoryPreferences preferences;
    CsvOptionPreferences::store(preferences, CsvOptionPreferences::kSimulationExportPanel,
                                someValues());
    ExportOptions   options;
    CsvOptionPanel& panel = options.panel();
    panel.loadPreferences(preferences);
    // Bug 50: "<->" whole.
    EXPECT_EQ(panel.values(), someValues());

    panel.fieldSeparatorComboBox()->setSeparatorChar(QChar(u' '));
    panel.decimalPlacesSpinBox()->setValue(0);
    panel.exponentialNotationCheckBox()->setChecked(true);
    panel.optionCheckBox(1)->setChecked(false);
    panel.storePreferences(preferences);
    EXPECT_EQ(preferences.get("ExportFieldSeparator"), " ");
    EXPECT_EQ(preferences.get("ExportDecimalPlaces"), "0");
    EXPECT_EQ(preferences.get("ExportExponentialNotation"), "true");
    EXPECT_EQ(preferences.get("ExportCommentCharacter"), "//");
    EXPECT_EQ(preferences.get("csvOptions.SimulationExportPanel.0"), "false");
    EXPECT_EQ(preferences.get("csvOptions.SimulationExportPanel.1"), "false");
    EXPECT_EQ(preferences.get("csvOptions.SimulationExportPanel.2"), "false");

    // Bug 51: a stored number of decimal places outside 0..15 opens the panel (clamped).
    preferences.put("ExportDecimalPlaces", "20");
    panel.loadPreferences(preferences);
    EXPECT_EQ(panel.decimalPlaces(), 15);
    preferences.put("ExportDecimalPlaces", "-1");
    panel.loadPreferences(preferences);
    EXPECT_EQ(panel.decimalPlaces(), 0);
}

TEST_F(CsvOptionPanelTest, TheTableExportsPanelHasNoCommentOptions)
{
    // SimulationTableCSVExport's panel (the file question's accessory): only the comment
    // character in its Comments group.
    const CsvOptionPanel panel(std::string(CsvOptionPreferences::kSimulationTableCsvExport), {},
                               CsvOptionPanel::Arrangement::ON_NEW_ROWS, nullptr);
    EXPECT_EQ(panel.optionCount(), 0U);
    EXPECT_TRUE(panel.values().commentOptions.empty());
    EXPECT_TRUE(panel.commentsGroup()->findChildren<QCheckBox*>().isEmpty());
    InMemoryPreferences preferences;
    panel.storePreferences(preferences);
    EXPECT_EQ(preferences.keys().size(), 4U);
}

TEST_F(CsvOptionPanelTest, PlacesTheGroupsOnNewRowsOrSideBySide)
{
    // Java's placeOnNewRows: the comments below the format settings, or right of them.
    ExportOptions   stacked;
    CsvOptionPanel& onNewRows = stacked.panel();
    onNewRows.adjustSize();
    onNewRows.layout()->activate();
    EXPECT_GE(onNewRows.commentsGroup()->geometry().top(),
              onNewRows.formatSettingsGroup()->geometry().bottom());
    EXPECT_EQ(onNewRows.commentsGroup()->geometry().left(),
              onNewRows.formatSettingsGroup()->geometry().left());

    ExportOptions   beside(CsvOptionPanel::Arrangement::SIDE_BY_SIDE);
    CsvOptionPanel& sideBySide = beside.panel();
    sideBySide.adjustSize();
    sideBySide.layout()->activate();
    EXPECT_GE(sideBySide.commentsGroup()->geometry().left(),
              sideBySide.formatSettingsGroup()->geometry().right());
    EXPECT_EQ(sideBySide.commentsGroup()->geometry().top(),
              sideBySide.formatSettingsGroup()->geometry().top());
}

}  // namespace QtRocket::Gui
