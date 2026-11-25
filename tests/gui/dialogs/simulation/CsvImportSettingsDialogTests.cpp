// The wind editor's CSV import settings (OpenRocket's CSVImportSettingsDialog, "Wind Data Import
// Settings"): what it shows, its preview, its outlines and what OK does are the transcript of the
// probe tools/openrocket-probes/csv-import-settings (CsvImportSettingsPins.h), replayed step by
// step on QtRocket's dialog and compared line by line, with documented edits for OpenRocket bugs 34
// and 50 and the negative index only; its layout is the relations of the probe's BOX lines;
// WidgetUserPrompts shows it for askWindCsvImportSettings(); the stored settings and the preview's
// parts are pinned as values.

#include "QtRocket/gui/dialogs/simulation/CsvImportSettingsDialog.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "dialogs/simulation/CsvImportSettingsPins.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using CsvImportSettingsDialogTest = Gui::Test::GuiTest;
using Row                         = CsvImportSettingsDialog::Row;
using Style                       = CsvImportSettingsDialog::PreviewStyle;
using Settings                    = UserPrompts::WindCsvImportSettings;
namespace Pins                    = Gui::Test::CsvImportSettingsPins;

constexpr std::array<Row, CsvImportSettingsDialog::kRowCount> kAllRows{
    Row::ALTITUDE, Row::SPEED, Row::DIRECTION, Row::STANDARD_DEVIATION};

/// The probe's clean(): line breaks as "\n", tabs as "\t".
[[nodiscard]] std::string probeText(const QString& text)
{
    QString cleaned = text;
    cleaned.replace(u'\n', QStringLiteral("\\n")).replace(u'\t', QStringLiteral("\\t"));
    return QtStrings::toUtf8(cleaned);
}

/// The probe's name of @p row (Java's field names).
[[nodiscard]] std::string_view rowKey(Row row)
{
    switch (row)
    {
        case Row::ALTITUDE:
            return "altitude";
        case Row::SPEED:
            return "speed";
        case Row::DIRECTION:
            return "direction";
        case Row::STANDARD_DEVIATION:
            return "stdDeviation";
    }
    return "?";
}

/// The probe's name of an outline.
[[nodiscard]] std::string_view outlineName(FieldValidator::Outline outline)
{
    switch (outline)
    {
        case FieldValidator::Outline::NONE:
            return "NONE";
        case FieldValidator::Outline::WARNING:
            return "WARNING";
        case FieldValidator::Outline::ERROR:
            return "ERROR";
    }
    return "?";
}

/// The probe's name of a preview run's colour.
[[nodiscard]] std::string styleName(const QColor& color)
{
    for (const Style style : {Style::NORMAL, Style::OPTIONAL, Style::INDEX})
    {
        if (color == CsvImportSettingsDialog::previewColor(style))
        {
            switch (style)
            {
                case Style::NORMAL:
                    return "normal";
                case Style::OPTIONAL:
                    return "optional";
                case Style::INDEX:
                    return "index";
            }
        }
    }
    return QtStrings::toUtf8(
        QStringLiteral("color(%1,%2,%3)").arg(color.red()).arg(color.green()).arg(color.blue()));
}

/// The dialog of the settings stored in its own services' preferences (a fresh store), asking
/// through their scripted prompts.
class ImportSettings
{
public:
    ImportSettings() { open(); }

    /// A new dialog of the preferences as they are now.
    void open()
    {
        m_dialog.reset();
        m_dialog = std::make_unique<CsvImportSettingsDialog>(
            CsvImportSettingsDialog::storedSettings(m_services.preferences()),
            m_services.prompts());
    }

    [[nodiscard]] CsvImportSettingsDialog& dialog() const noexcept { return *m_dialog; }
    [[nodiscard]] Preferences& preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }

private:
    Gui::Test::TestGuiServices               m_services;
    std::unique_ptr<CsvImportSettingsDialog> m_dialog;
};

// ------------------------------------------------------------------------------- the transcript

/// The preview's text pane line by line as the probe prints it: each run of one colour as
/// style[text].
[[nodiscard]] std::vector<std::string> previewLines(const CsvImportSettingsDialog& dialog)
{
    std::vector<std::string> lines;
    const QTextDocument&     document = *dialog.previewTextPane().document();
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
    {
        std::string line = "PREVIEW ";
        std::string runStyle;
        QString     run;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment = it.fragment();
            const std::string   style    = styleName(fragment.charFormat().foreground().color());
            if (!run.isEmpty() && style != runStyle)
            {
                line += runStyle + "[" + probeText(run) + "]";
                run.clear();
            }
            runStyle = style;
            run += fragment.text();
        }
        if (!run.isEmpty())
        {
            line += runStyle + "[" + probeText(run) + "]";
        }
        lines.push_back(line);
    }
    return lines;
}

/// The probe's STATE line: the column type, the columns as the import is given them, the names,
/// indices and units, and the separator answered (the separator itself, bug 34).
[[nodiscard]] std::string stateLine(const CsvImportSettingsDialog& dialog)
{
    const Settings settings = dialog.settings();
    std::string    columns;
    std::string    names;
    std::string    indices;
    std::string    units;
    for (const Row row : kAllRows)
    {
        const std::string                 separator = row == Row::ALTITUDE ? "" : "|";
        const UserPrompts::WindCsvColumn& column = CsvImportSettingsDialog::columnOf(settings, row);
        columns += separator + QtStrings::toUtf8(dialog.importColumn(row));
        names += separator + QtStrings::toUtf8(column.name);
        indices += separator + std::to_string(column.index);
        units += separator + (column.unit != nullptr ? column.unit->getUnit() : "null");
    }
    return "STATE columnType=[" + QtStrings::toUtf8(dialog.columnTypeLabel().text()) +
           "] header=" + (settings.hasHeader ? "true" : "false") + " columns=[" + columns +
           "] names=[" + names + "] indices=[" + indices + "] units=[" + units + "] separator=[" +
           probeText(settings.separator) + "]";
}

/// The probe's OUTLINE line: the error outlines of the name fields and the index spin boxes.
[[nodiscard]] std::string outlineLine(const CsvImportSettingsDialog& dialog)
{
    std::string line = "OUTLINE";
    for (const Row row : kAllRows)
    {
        line += " " + std::string(rowKey(row)) + "Field=" +
                std::string(outlineName(FieldValidator::outlineOf(dialog.columnField(row))));
    }
    for (const Row row : kAllRows)
    {
        line += " " + std::string(rowKey(row)) + "Spinner=" +
                std::string(outlineName(FieldValidator::outlineOf(dialog.columnSpinner(row))));
    }
    return line;
}

/// Clicks OK and tells what it did as the probe's OK line: the message it showed through
/// @p prompts ("OK <severity> [title] [lines]") or "OK approved".
[[nodiscard]] std::string clickOk(CsvImportSettingsDialog&              dialog,
                                  const Gui::Test::ScriptedUserPrompts& prompts)
{
    const std::size_t before = prompts.prompts().size();
    dialog.setResult(QDialog::Rejected);
    dialog.okButton().click();
    if (prompts.prompts().size() > before)
    {
        const Gui::Test::ScriptedUserPrompts::Prompt& shown = prompts.prompts().back();
        // A message's texts are its severity, then its lines (Java: ERROR_MESSAGE).
        if (shown.kind != Gui::Test::ScriptedUserPrompts::Kind::MESSAGE || shown.texts.isEmpty())
        {
            return "OK asked a question";
        }
        return "OK " + QtStrings::toUtf8(shown.texts.first()) + " [" +
               QtStrings::toUtf8(shown.title) + "] [" +
               probeText(shown.texts.sliced(1).join(u'\n')) + "]";
    }
    return dialog.result() == QDialog::Accepted ? "OK approved" : "OK did nothing";
}

/// The probe's lines of a state: STATE, OUTLINE, PREVIEW and OK (which is clicked).
[[nodiscard]] std::vector<std::string> stateLines(CsvImportSettingsDialog&              dialog,
                                                  const Gui::Test::ScriptedUserPrompts& prompts)
{
    std::vector<std::string> lines{stateLine(dialog), outlineLine(dialog)};
    for (std::string& line : previewLines(dialog))
    {
        lines.push_back(std::move(line));
    }
    lines.push_back(clickOk(dialog, prompts));
    return lines;
}

/// The probe's dialog lines without its BOX lines: DIALOG, WALK, UNITS and the state.
[[nodiscard]] std::vector<std::string> dialogLines(CsvImportSettingsDialog&              dialog,
                                                   const Gui::Test::ScriptedUserPrompts& prompts)
{
    const QPushButton* defaultButton = nullptr;
    for (const QPushButton* const button : dialog.findChildren<QPushButton*>())
    {
        if (button->isDefault())
        {
            defaultButton = button;
        }
    }
    // Escape cancels the dialog (QDialog's own; EscapeAndTheCloseButtonCancel shows it).
    std::vector<std::string> lines{
        "DIALOG title=[" + QtStrings::toUtf8(dialog.windowTitle()) + "] modality=" +
        (dialog.windowModality() == Qt::ApplicationModal ? "APPLICATION_MODAL" : "other") +
        " size=" + std::to_string(dialog.width()) + "x" + std::to_string(dialog.height()) +
        " default=[" +
        (defaultButton != nullptr ? QtStrings::toUtf8(defaultButton->text()) : std::string()) +
        "] escape=true"};
    for (const std::string& line : Gui::Test::dialogWalk(dialog))
    {
        lines.push_back("WALK " + line);
    }
    for (const Row row : kAllRows)
    {
        const UnitGroup& group = dialog.unitSelector(row).getUnitGroup();
        std::string      line  = "UNITS " + std::string(rowKey(row));
        for (int index = 0; index < group.getUnitCount(); ++index)
        {
            line += (index == 0 ? " " : "|") + group.getUnit(index).getUnit();
        }
        lines.push_back(line);
    }
    for (std::string& line : stateLines(dialog, prompts))
    {
        lines.push_back(std::move(line));
    }
    return lines;
}

/// The probe's lines without its BOX lines.
[[nodiscard]] std::vector<std::string> withoutBoxes(std::span<const std::string_view> pins)
{
    std::vector<std::string> lines;
    for (const std::string_view line : pins)
    {
        if (!line.starts_with("BOX "))
        {
            lines.emplace_back(line);
        }
    }
    return lines;
}

/// OpenRocket bug 34, fixed: the separator the dialog answers is the separator itself, where the
/// probe's getSeparator() gives the SPACE or TAB item's text: @p lines with the STATE lines'
/// "separator=[SPACE]" and "separator=[TAB]" as QtRocket's " " and the tab (written "\t").
[[nodiscard]] std::vector<std::string> withBug34Fixed(std::vector<std::string> lines)
{
    for (std::string& line : lines)
    {
        if (!line.starts_with("STATE "))
        {
            continue;
        }
        if (const std::size_t at = line.find("separator=[SPACE]"); at != std::string::npos)
        {
            line.replace(at, std::string_view("separator=[SPACE]").size(), "separator=[ ]");
        }
        if (const std::size_t at = line.find("separator=[TAB]"); at != std::string::npos)
        {
            line.replace(at, std::string_view("separator=[TAB]").size(), "separator=[\\t]");
        }
    }
    return lines;
}

/// Compares @p actual with @p expected line by line (a difference names its line).
void expectSameLines(const std::vector<std::string>& actual,
                     const std::vector<std::string>& expected)
{
    EXPECT_EQ(actual.size(), expected.size());
    const std::size_t count = std::min(actual.size(), expected.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        EXPECT_EQ(actual[index], expected[index]) << "line " << index;
    }
}

// ------------------------------------------------------------------------------- the probe's steps

/// The probe's steps on one dialog, each with its lines.
class Steps
{
public:
    Steps(CsvImportSettingsDialog& dialog, const Gui::Test::ScriptedUserPrompts& prompts) noexcept
      : m_dialog(&dialog), m_prompts(&prompts)
    {
    }

    [[nodiscard]] const std::vector<std::string>& lines() const noexcept { return m_lines; }

    /// The probe's step(): "STEP <what>" and the state's lines.
    void step(std::string_view what)
    {
        m_lines.push_back("STEP " + std::string(what));
        for (std::string& line : stateLines(*m_dialog, *m_prompts))
        {
            m_lines.push_back(std::move(line));
        }
    }

    void name(Row row, const QString& text) const { m_dialog->columnField(row).setText(text); }
    void index(Row row, int value) const { m_dialog->columnSpinner(row).setValue(value); }
    /// The header check box clicked (Java: doClick()).
    void clickHeader() const { m_dialog->hasHeaderCheckBox().click(); }

    /// The separator box's item @p text chosen, or the text typed when it is no item (Java:
    /// setSelectedItem()).
    void separator(const QString& text) const
    {
        FieldSeparatorComboBox& box   = m_dialog->separatorComboBox();
        const int               found = box.findText(text);
        if (found >= 0)
        {
            box.setCurrentIndex(found);
        }
        else
        {
            box.setEditText(text);
        }
    }

    /// The probe's walk of the dialog ("WALK ..." lines).
    void walk()
    {
        for (const std::string& line : Gui::Test::dialogWalk(*m_dialog))
        {
            m_lines.push_back("WALK " + line);
        }
    }

    /// Every unit of every row chosen in turn, each with "UNITSTEP <row> <unit>" and the preview;
    /// then the row's unit again.
    void unitSteps()
    {
        for (const Row row : kAllRows)
        {
            UnitSelector&    selector = m_dialog->unitSelector(row);
            const UnitGroup& group    = selector.getUnitGroup();
            const Unit&      original = selector.getSelectedUnit();
            for (int index = 0; index < group.getUnitCount(); ++index)
            {
                selector.setSelectedUnit(group.getUnit(index));
                m_lines.push_back("UNITSTEP " + std::string(rowKey(row)) + " " +
                                  group.getUnit(index).getUnit());
                for (std::string& line : previewLines(*m_dialog))
                {
                    m_lines.push_back(std::move(line));
                }
            }
            selector.setSelectedUnit(original);
        }
    }

private:
    CsvImportSettingsDialog*              m_dialog;
    const Gui::Test::ScriptedUserPrompts* m_prompts;
    std::vector<std::string>              m_lines;
};

/// The probe's steps of the column names (the header mode).
void nameSteps(Steps& steps)
{
    steps.name(Row::ALTITUDE, QString());
    steps.step("altitude name empty");
    steps.name(Row::ALTITUDE, QStringLiteral("   "));
    steps.step("altitude name blank");
    steps.name(Row::ALTITUDE, QStringLiteral("altitude"));
    steps.name(Row::SPEED, QStringLiteral("altitude"));
    steps.step("speed name = altitude");
    steps.name(Row::ALTITUDE, QString());
    steps.step("altitude name empty again (speed still named altitude)");
    steps.name(Row::DIRECTION, QStringLiteral("altitude"));
    steps.step("altitude empty, speed and direction named altitude");
    steps.name(Row::ALTITUDE, QStringLiteral(" alt "));
    steps.name(Row::SPEED, QStringLiteral("speed"));
    steps.name(Row::DIRECTION, QStringLiteral("direction"));
    steps.name(Row::STANDARD_DEVIATION, QStringLiteral("alt"));
    steps.step("altitude ' alt ', std dev 'alt' (trimmed duplicates)");
    steps.name(Row::STANDARD_DEVIATION, QString());
    steps.step("std dev name empty");
    steps.name(Row::STANDARD_DEVIATION, QStringLiteral("  "));
    steps.step("std dev name blank");
    steps.name(Row::ALTITUDE, QStringLiteral("altitude"));
    steps.name(Row::STANDARD_DEVIATION, QStringLiteral("stddev"));
    steps.step("names back to the defaults");
}

/// The probe's steps of the separators.
void separatorSteps(Steps& steps)
{
    for (const QString& item : {QStringLiteral(";"), QStringLiteral("SPACE"), QStringLiteral("TAB"),
                                QStringLiteral("|"), QStringLiteral("<->"), QString()})
    {
        steps.separator(item);
        steps.step("separator item " + QtStrings::toUtf8(item));
    }
    steps.separator(QStringLiteral(","));
}

/// The probe's steps of the index mode.
void indexSteps(Steps& steps)
{
    steps.clickHeader();
    steps.step("header row unchecked");
    steps.walk();
    steps.index(Row::ALTITUDE, 1);
    steps.step("altitude index 1");
    steps.index(Row::DIRECTION, 1);
    steps.step("direction index 1 too");
    steps.index(Row::ALTITUDE, 0);
    steps.index(Row::DIRECTION, 2);
    steps.index(Row::STANDARD_DEVIATION, 0);
    steps.step("std dev index 0 (its name stddev)");
    steps.clickHeader();
    steps.name(Row::STANDARD_DEVIATION, QString());
    steps.clickHeader();
    steps.step("std dev name emptied in the header mode, back to the index mode");
    steps.index(Row::STANDARD_DEVIATION, 3);
    steps.index(Row::SPEED, 7);
    steps.index(Row::ALTITUDE, 12345);
    steps.step("indices 12345, 7, 2, 3");
    steps.clickHeader();
    steps.step("header row checked again (std dev name empty)");
    steps.name(Row::SPEED, QString());
    steps.clickHeader();
    steps.step("speed name empty, then the index mode");
    steps.index(Row::SPEED, 0);
    steps.clickHeader();
    steps.step("speed index 0, back to the header mode (speed name empty)");
}

/// The preferences of the probe's section "stored preferences".
void storeProbePreferences(Preferences& preferences)
{
    preferences.setMultiLevelWindCsvImportHeader(false);
    preferences.setMultiLevelWindCsvImportAltitudeColumn("Alt (ft)");
    preferences.setMultiLevelWindCsvImportSpeedColumn("Wind");
    preferences.setMultiLevelWindCsvImportDirectionColumn("Dir");
    preferences.setMultiLevelWindCsvImportStddevColumn("");
    preferences.setMultiLevelWindCsvImportAltitudeColumnIndex(4);
    preferences.setMultiLevelWindCsvImportSpeedColumnIndex(2);
    preferences.setMultiLevelWindCsvImportDirectionColumnIndex(0);
    preferences.setMultiLevelWindCsvImportStddevColumnIndex(2);
    preferences.setMultiLevelWindCsvImportAltitudeUnit(
        *unitGroup(UnitGroupId::DISTANCE).getUnit("ft"));
    preferences.setMultiLevelWindCsvImportSpeedUnit(
        *unitGroup(UnitGroupId::WINDSPEED).getUnit("km/h"));
    preferences.setMultiLevelWindCsvImportDirectionUnit(
        *unitGroup(UnitGroupId::ANGLE).getUnit("rad"));
    preferences.setMultiLevelWindCsvImportStddevUnit(
        *unitGroup(UnitGroupId::WINDSPEED).getUnit("mph"));
    preferences.putString(PreferenceKeys::kExportFieldSeparator, ";");
}

/// The probe's steps of the "stored preferences" section up to the negative index, with
/// QtRocket's lines for the separator "SPACE" stored (see the test).
void storedSteps(ImportSettings& settings, std::vector<std::string>& lines)
{
    {
        Steps steps(settings.dialog(), settings.prompts());
        steps.clickHeader();
        steps.step("header row checked");
        lines = steps.lines();
    }
    for (const auto& [stored, what] :
         {std::pair{std::string_view("SPACE"), std::string_view("separator stored as SPACE")},
          std::pair{std::string_view("\t"), std::string_view("separator stored as a tab")},
          std::pair{std::string_view(""), std::string_view("separator stored empty")}})
    {
        settings.preferences().putString(PreferenceKeys::kExportFieldSeparator, stored);
        settings.open();
        Steps steps(settings.dialog(), settings.prompts());
        steps.step(what);
        lines.insert(lines.end(), steps.lines().begin(), steps.lines().end());
    }
}

/// OpenRocket bug 50: the probe's step "separator stored as SPACE" (Java shows the stored "SPACE"
/// as its first character "S", the separator it then splits at) replaced in @p lines by QtRocket's
/// (the SPACE item, the separator " ", displayed as "\s").
[[nodiscard]] std::vector<std::string> withQtRocketsSpaceStep(std::vector<std::string> lines)
{
    // clang-format off
    const std::vector<std::string> space{
        "STEP separator stored as SPACE",
        "STATE columnType=[Column Index] header=false columns=[4|2|0|2] names=[Alt (ft)|Wind|Dir|] indices=[4|2|0|2] units=[ft|km/h|rad|mph] separator=[ ]",
        "OUTLINE altitudeField=NONE speedField=NONE directionField=NONE stdDeviationField=NONE altitudeSpinner=NONE speedSpinner=NONE directionSpinner=NONE stdDeviationSpinner=NONE",
        "PREVIEW optional[# You can add comments with a hash]",
        "PREVIEW optional[# Column mapping: ]index[4=Altitude, 2=Speed, 0=Direction]optional[, 2=StdDev]",
        R"(PREVIEW normal[328\s18\s1.57]optional[\s2.24])",
        R"(PREVIEW normal[656\s27\s1.31]optional[\s3.36])",
        R"(PREVIEW normal[984\s36\s1.83]optional[\s4.47])",
        R"(PREVIEW normal[1312\s45\s2.36]optional[\s5.59])",
        "PREVIEW normal[\u2026]",
        "OK approved",
    };
    // clang-format on
    const auto first = std::ranges::find(lines, std::string("STEP separator stored as SPACE"));
    if (first == lines.end())
    {
        ADD_FAILURE() << "the probe has no step \"separator stored as SPACE\"";
        return lines;
    }
    auto last = first + 1;
    while (last != lines.end() && !last->starts_with("STEP "))
    {
        ++last;
    }
    EXPECT_EQ(static_cast<std::size_t>(last - first), space.size());
    const auto at = lines.erase(first, last);
    lines.insert(at, space.begin(), space.end());
    return lines;
}

/// @p shown as the settings dialog (a test failure and null when it is another).
[[nodiscard]] CsvImportSettingsDialog* asSettingsDialog(QWidget& shown)
{
    auto* const dialog = qobject_cast<CsvImportSettingsDialog*>(&shown);
    EXPECT_NE(dialog, nullptr) << "another dialog was shown";
    return dialog;
}

/// The settings dialog of WidgetUserPromptsAsksWithTheDialog: it is modal and shows the settings
/// given; the altitude's name " height " (trimmed in the answer), then OK.
void answerWithAnotherAltitudeName(QWidget& shown)
{
    const CsvImportSettingsDialog* const dialog = asSettingsDialog(shown);
    if (dialog == nullptr)
    {
        return;
    }
    EXPECT_EQ(dialog->windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog->columnField(Row::SPEED).text(), QStringLiteral("wind"));
    EXPECT_EQ(dialog->separatorComboBox().currentText(), QStringLiteral("TAB"));
    dialog->columnField(Row::ALTITUDE).setText(QStringLiteral("  height "));
    dialog->okButton().click();
}

/// Cancel of the settings dialog.
void clickCancel(QWidget& shown)
{
    if (const CsvImportSettingsDialog* const dialog = asSettingsDialog(shown); dialog != nullptr)
    {
        dialog->cancelButton().click();
    }
}

/// Escape in a dialog.
void pressEscape(QWidget& shown)
{
    QTest::keyClick(&shown, Qt::Key_Escape);
}

/// The geometry of @p widget in @p dialog's coordinates.
[[nodiscard]] QRect boxIn(const QWidget& widget, const QWidget& dialog)
{
    return {widget.mapTo(&dialog, QPoint(0, 0)), widget.size()};
}

}  // namespace

// ------------------------------------------------------------------------------- the probe

TEST_F(CsvImportSettingsDialogTest, TheDialogShowsWhatOpenRocketsShows)
{
    // The probe's dialog over a fresh preference store: its title, modality, size and default
    // button, its walk, its units, its state and preview, and OK, which accepts the defaults.
    ImportSettings settings;
    expectSameLines(dialogLines(settings.dialog(), settings.prompts()),
                    withoutBoxes(Pins::kFreshDialog));
    EXPECT_EQ(settings.dialog().columnField(Row::ALTITUDE).objectName(),
              QStringLiteral("altitudeColumnField"));
    EXPECT_EQ(settings.dialog().columnSpinner(Row::STANDARD_DEVIATION).objectName(),
              QStringLiteral("stdDeviationColumnSpinner"));
    EXPECT_EQ(settings.dialog().unitSelector(Row::SPEED).objectName(),
              QStringLiteral("speedUnitSelector"));
    EXPECT_EQ(settings.dialog().separatorComboBox().objectName(),
              QStringLiteral("separatorComboBox"));
    EXPECT_EQ(settings.dialog().previewTextPane().objectName(), QStringLiteral("previewTextPane"));
    EXPECT_TRUE(settings.dialog().previewTextPane().isReadOnly());
    EXPECT_FALSE(settings.dialog().cancelButton().isDefault());
    EXPECT_FALSE(settings.dialog().cancelButton().autoDefault());
}

TEST_F(CsvImportSettingsDialogTest, TheStepsOfTheProbeDoWhatOpenRocketsDo)
{
    // The probe's steps over a fresh preference store: names empty, blank, duplicated (also after
    // trimming), the separators, every unit of every row, the index mode and its duplicates, the
    // standard deviation's index counting only with a name. OpenRocket bug 34: the separator
    // answered is the separator itself.
    ImportSettings settings;
    static_cast<void>(clickOk(settings.dialog(), settings.prompts()));
    Steps steps(settings.dialog(), settings.prompts());
    nameSteps(steps);
    separatorSteps(steps);
    steps.unitSteps();
    indexSteps(steps);
    expectSameLines(steps.lines(),
                    withBug34Fixed({Pins::kFreshSteps.begin(), Pins::kFreshSteps.end()}));
}

TEST_F(CsvImportSettingsDialogTest, TheDialogStartsWithTheStoredSettings)
{
    // The probe's dialog over stored preferences: no header row, other names, indices and units,
    // the separator ";".
    ImportSettings settings;
    storeProbePreferences(settings.preferences());
    settings.open();
    expectSameLines(dialogLines(settings.dialog(), settings.prompts()),
                    withoutBoxes(Pins::kStoredDialog));

    // Then the steps of the section, but the last (the negative index: see
    // AStoredNegativeIndexStartsAtZero). OpenRocket bug 50: Java shows a stored "SPACE" (bug
    // 34's) as its first character "S", which it then splits at; QtRocket shows the SPACE item,
    // the separator " " (displayed as "\s"): the probe's lines of that step are QtRocket's here.
    std::vector<std::string> lines;
    storedSteps(settings, lines);
    std::vector<std::string> pinned(Pins::kStoredSteps.begin(), Pins::kStoredSteps.end());
    pinned.pop_back();
    std::vector<std::string> expected = withQtRocketsSpaceStep(std::move(pinned));
    expectSameLines(lines, withBug34Fixed(std::move(expected)));
    EXPECT_EQ(
        Pins::kStoredSteps.back(),
        "negative index: java.lang.IllegalArgumentException: (minimum <= value <= maximum) is "
        "false");
}

TEST_F(CsvImportSettingsDialogTest, AStoredNegativeIndexStartsAtZero)
{
    // Deviation: Java's SpinnerNumberModel refuses a stored index below 0 (the probe's
    // IllegalArgumentException), so that its dialog cannot be opened; the spin box starts at 0.
    ImportSettings settings;
    storeProbePreferences(settings.preferences());
    settings.preferences().setMultiLevelWindCsvImportSpeedColumnIndex(-1);
    settings.open();
    EXPECT_EQ(settings.dialog().columnSpinner(Row::SPEED).value(), 0);
    EXPECT_EQ(settings.dialog().settings().speed.index, 0);
}

TEST_F(CsvImportSettingsDialogTest, OkAcceptsAndCancelRejects)
{
    // The probe: "OK clicked: approved=true", "Cancel clicked: approved=false".
    ASSERT_EQ(Pins::kOkAndCancel.size(), 2U);
    EXPECT_EQ(Pins::kOkAndCancel[0], "OK clicked: approved=true displayable=false");
    EXPECT_EQ(Pins::kOkAndCancel[1], "Cancel clicked: approved=false displayable=false");
    ImportSettings settings;
    EXPECT_EQ(clickOk(settings.dialog(), settings.prompts()), "OK approved");
    settings.open();
    settings.dialog().setResult(QDialog::Accepted);
    settings.dialog().cancelButton().click();
    EXPECT_EQ(settings.dialog().result(), QDialog::Rejected);
    EXPECT_TRUE(settings.prompts().prompts().empty());
}

// ------------------------------------------------------------------------------- the keys and
// layout

TEST_F(CsvImportSettingsDialogTest, EscapeAndTheCloseButtonCancelEnterAccepts)
{
    ImportSettings           settings;
    CsvImportSettingsDialog& dialog = settings.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    // Java: installEscapeCloseButtonOperation(this, cancelButton).
    QTest::keyClick(&dialog, Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(dialog.result(), QDialog::Rejected);

    settings.open();
    CsvImportSettingsDialog& again = settings.dialog();
    ASSERT_TRUE(Gui::Test::showActive(again));
    EXPECT_TRUE(again.close());
    EXPECT_EQ(again.result(), QDialog::Rejected);

    // Java: getRootPane().setDefaultButton(okButton): Enter in a name field presses OK.
    settings.open();
    CsvImportSettingsDialog& third = settings.dialog();
    ASSERT_TRUE(Gui::Test::showActive(third));
    third.columnField(Row::SPEED).setFocus();
    QTest::keyClick(&third.columnField(Row::SPEED), Qt::Key_Return);
    EXPECT_FALSE(third.isVisible());
    EXPECT_EQ(third.result(), QDialog::Accepted);
}

TEST_F(CsvImportSettingsDialogTest, TheLayoutIsJavas)
{
    // The relations of the probe's BOX lines: the description above the two boxes, which share
    // their left edge and width; in "CSV Settings" the help text, the check box, the grid's
    // header and its four rows one below the other in three columns, the separator line and
    // "Field Separator" with its box in the columns of the labels and of the fields; in the
    // preview's box the help text above the preview; Cancel left of OK below the scroll area.
    const ImportSettings     settings;
    CsvImportSettingsDialog& dialog = settings.dialog();
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    const QRect description = boxIn(dialog.description(), dialog);
    const QRect csvBox      = boxIn(dialog.csvSettingsBox(), dialog);
    const QRect previewBox  = boxIn(dialog.previewBox(), dialog);
    EXPECT_LE(description.bottom(), csvBox.top());
    EXPECT_LE(csvBox.bottom(), previewBox.top());
    EXPECT_EQ(csvBox.left(), previewBox.left());
    EXPECT_EQ(csvBox.width(), previewBox.width());
    EXPECT_EQ(description.left(), csvBox.left());

    const QRect help     = boxIn(dialog.columnMappingHelp(), dialog);
    const QRect check    = boxIn(dialog.hasHeaderCheckBox(), dialog);
    const QRect dataType = boxIn(dialog.dataTypeLabel(), dialog);
    const QRect column   = boxIn(dialog.columnTypeLabel(), dialog);
    const QRect unit     = boxIn(dialog.unitLabel(), dialog);
    EXPECT_TRUE(csvBox.contains(help));
    EXPECT_LE(help.bottom(), check.top());
    EXPECT_LE(check.bottom(), dataType.top());
    EXPECT_EQ(dataType.top(), column.top());
    EXPECT_EQ(column.top(), unit.top());
    EXPECT_LT(dataType.right(), column.left());
    EXPECT_LT(column.right(), unit.left());
    EXPECT_GE(dataType.width(), CsvImportSettingsDialog::kLabelColumnWidth);
    EXPECT_GE(unit.width(), CsvImportSettingsDialog::kLabelColumnWidth);
    EXPECT_EQ(help.left(), dataType.left());
    EXPECT_EQ(check.left(), dataType.left());

    int previousBottom = dataType.bottom();
    for (const Row row : kAllRows)
    {
        const QRect label    = boxIn(dialog.rowLabel(row), dialog);
        const QRect field    = boxIn(dialog.columnField(row), dialog);
        const QRect selector = boxIn(dialog.unitSelector(row), dialog);
        EXPECT_LE(previousBottom, field.top()) << rowKey(row);
        EXPECT_EQ(label.left(), dataType.left()) << rowKey(row);
        EXPECT_EQ(field.left(), column.left()) << rowKey(row);
        EXPECT_EQ(selector.left(), unit.left()) << rowKey(row);
        EXPECT_LT(label.top(), field.bottom()) << rowKey(row);
        EXPECT_GT(label.bottom(), field.top()) << rowKey(row);
        previousBottom = field.bottom();
    }
    const QRect line     = boxIn(dialog.separatorLine(), dialog);
    const QRect sepLabel = boxIn(dialog.separatorLabel(), dialog);
    const QRect sepBox   = boxIn(dialog.separatorComboBox(), dialog);
    EXPECT_LE(previousBottom, line.top());
    EXPECT_LE(line.bottom(), sepLabel.top());
    EXPECT_EQ(line.left(), dataType.left());
    EXPECT_GE(line.right(), unit.right());
    EXPECT_EQ(sepLabel.left(), dataType.left());
    EXPECT_EQ(sepBox.left(), column.left());
    EXPECT_TRUE(csvBox.contains(sepBox));

    const QRect previewHelp = boxIn(dialog.previewHelp(), dialog);
    const QRect preview     = boxIn(dialog.previewTextPane(), dialog);
    EXPECT_TRUE(previewBox.contains(previewHelp));
    EXPECT_TRUE(previewBox.contains(preview));
    EXPECT_LE(previewHelp.bottom(), preview.top());
    EXPECT_EQ(previewHelp.left(), preview.left());

    const QRect scroll = boxIn(dialog.scrollArea(), dialog);
    const QRect cancel = boxIn(dialog.cancelButton(), dialog);
    const QRect ok     = boxIn(dialog.okButton(), dialog);
    EXPECT_LE(scroll.bottom(), cancel.top());
    EXPECT_EQ(cancel.top(), ok.top());
    EXPECT_LT(cancel.right(), ok.left());
    EXPECT_GT(ok.left(), dialog.width() / 2);
    // The preview is as tall as its text: it does not scroll.
    EXPECT_GE(dialog.previewTextPane().height(),
              static_cast<int>(dialog.previewTextPane().document()->size().height()));
}

// ------------------------------------------------------------------------------- the values

TEST_F(CsvImportSettingsDialogTest, TheStoredSettingsAreThePreferences)
{
    Gui::Test::TestGuiServices services;
    Preferences&               preferences = services.preferences();
    const Settings             fresh       = CsvImportSettingsDialog::storedSettings(preferences);
    // ApplicationPreferences' defaults and FieldSeparatorComboBox.loadFromExportPreferences()'s.
    EXPECT_TRUE(fresh.hasHeader);
    EXPECT_EQ(fresh.altitude.name, QStringLiteral("altitude"));
    EXPECT_EQ(fresh.speed.name, QStringLiteral("speed"));
    EXPECT_EQ(fresh.direction.name, QStringLiteral("direction"));
    EXPECT_EQ(fresh.standardDeviation.name, QStringLiteral("stddev"));
    EXPECT_EQ(fresh.altitude.index, 0);
    EXPECT_EQ(fresh.speed.index, 1);
    EXPECT_EQ(fresh.direction.index, 2);
    EXPECT_EQ(fresh.standardDeviation.index, 3);
    EXPECT_EQ(fresh.altitude.unit, &unitGroup(UnitGroupId::DISTANCE).getSIUnit());
    EXPECT_EQ(fresh.speed.unit, &unitGroup(UnitGroupId::WINDSPEED).getSIUnit());
    EXPECT_EQ(fresh.direction.unit, unitGroup(UnitGroupId::ANGLE).getUnit("\u00B0"));
    EXPECT_EQ(fresh.standardDeviation.unit, &unitGroup(UnitGroupId::WINDSPEED).getSIUnit());
    EXPECT_EQ(fresh.separator, QStringLiteral(","));

    // What the wind editor stores comes back.
    Settings chosen               = fresh;
    chosen.hasHeader              = false;
    chosen.altitude.name          = QStringLiteral("Alt");
    chosen.standardDeviation.name = QString();
    chosen.speed.index            = 7;
    chosen.direction.unit         = unitGroup(UnitGroupId::ANGLE).getUnit("rad");
    chosen.speed.unit             = unitGroup(UnitGroupId::WINDSPEED).getUnit("kt");
    chosen.separator              = QStringLiteral(" ");
    CsvImportSettingsDialog::storeSettings(preferences, chosen);
    EXPECT_EQ(CsvImportSettingsDialog::storedSettings(preferences), chosen);
    // OpenRocket bug 34: the separator itself is the export's separator, never "SPACE" or "TAB".
    EXPECT_EQ(preferences.getString(PreferenceKeys::kExportFieldSeparator, ""), " ");
    chosen.separator = QStringLiteral("\t");
    CsvImportSettingsDialog::storeSettings(preferences, chosen);
    EXPECT_EQ(preferences.getString(PreferenceKeys::kExportFieldSeparator, ""), "\t");
    // An empty stored separator reads as "," (loadFromExportPreferences() leaves it).
    preferences.putString(PreferenceKeys::kExportFieldSeparator, "");
    EXPECT_EQ(CsvImportSettingsDialog::storedSettings(preferences).separator, QStringLiteral(","));
}

TEST_F(CsvImportSettingsDialogTest, SettingsWithoutUnitsCannotBeStored)
{
    Gui::Test::TestGuiServices services;
    Settings settings       = CsvImportSettingsDialog::storedSettings(services.preferences());
    settings.direction.unit = nullptr;
    EXPECT_THROW(CsvImportSettingsDialog::storeSettings(services.preferences(), settings),
                 BugError);
}

TEST_F(CsvImportSettingsDialogTest, ThePreviewsPartsAreJavas)
{
    // Java's updatePreview(), its insertString() calls one by one (the probe merges them by
    // colour).
    Gui::Test::TestGuiServices services;
    Settings settings = CsvImportSettingsDialog::storedSettings(services.preferences());
    const std::vector<CsvImportSettingsDialog::PreviewPart> header =
        CsvImportSettingsDialog::previewParts(settings, QStringLiteral(","));
    ASSERT_EQ(header.size(), 28U);
    using Part = CsvImportSettingsDialog::PreviewPart;
    EXPECT_EQ(header[0], (Part{.text  = QStringLiteral("# You can add comments with a hash"),
                               .style = Style::OPTIONAL}));
    EXPECT_EQ(header[1], (Part{.text = QStringLiteral("\n"), .style = Style::OPTIONAL}));
    EXPECT_EQ(header[2], (Part{.text = QStringLiteral("altitude,"), .style = Style::NORMAL}));
    EXPECT_EQ(header[3], (Part{.text = QStringLiteral("speed,"), .style = Style::NORMAL}));
    EXPECT_EQ(header[4], (Part{.text = QStringLiteral("direction"), .style = Style::NORMAL}));
    EXPECT_EQ(header[5], (Part{.text = QStringLiteral(",stddev"), .style = Style::OPTIONAL}));
    EXPECT_EQ(header[6], (Part{.text = QStringLiteral("\n"), .style = Style::NORMAL}));
    EXPECT_EQ(header[7], (Part{.text = QStringLiteral("100,"), .style = Style::NORMAL}));
    EXPECT_EQ(header[10], (Part{.text = QStringLiteral(",1"), .style = Style::OPTIONAL}));
    EXPECT_EQ(header[27], (Part{.text = QStringLiteral("\u2026"), .style = Style::NORMAL}));

    // Without a header row the standard deviation's index is always shown (the import is always
    // given it).
    settings.hasHeader              = false;
    settings.standardDeviation.name = QString();
    const std::vector<CsvImportSettingsDialog::PreviewPart> indices =
        CsvImportSettingsDialog::previewParts(settings, QStringLiteral("\\t"));
    ASSERT_EQ(indices.size(), 29U);
    EXPECT_EQ(indices[2],
              (Part{.text = QStringLiteral("# Column mapping: "), .style = Style::OPTIONAL}));
    EXPECT_EQ(indices[3], (Part{.text = QStringLiteral("0=Altitude, "), .style = Style::INDEX}));
    EXPECT_EQ(indices[6], (Part{.text = QStringLiteral(", 3=StdDev"), .style = Style::OPTIONAL}));
    EXPECT_EQ(indices[8], (Part{.text = QStringLiteral("100\\t"), .style = Style::NORMAL}));

    EXPECT_EQ(CsvImportSettingsDialog::displayedSeparator(FieldSeparatorComboBox::spaceText()),
              QStringLiteral("\\s"));
    EXPECT_EQ(CsvImportSettingsDialog::displayedSeparator(FieldSeparatorComboBox::tabText()),
              QStringLiteral("\\t"));
    EXPECT_EQ(CsvImportSettingsDialog::displayedSeparator(QStringLiteral(" ")),
              QStringLiteral(" "));
    EXPECT_EQ(CsvImportSettingsDialog::previewColor(Style::NORMAL), QColor(0, 0, 0));
    EXPECT_EQ(CsvImportSettingsDialog::previewColor(Style::OPTIONAL), QColor(128, 128, 128));
    EXPECT_EQ(CsvImportSettingsDialog::previewColor(Style::INDEX), QColor(0, 100, 0));
}

TEST_F(CsvImportSettingsDialogTest, AStoredSpaceIsShownAsTheSpaceItem)
{
    // OpenRocket bug 50 (FieldSeparatorComboBox): a separator "SPACE" stored by bug 34 shows the
    // SPACE item, which means the space; the separator " " stored by QtRocket too.
    ImportSettings settings;
    for (const std::string_view stored : {std::string_view("SPACE"), std::string_view(" ")})
    {
        settings.preferences().putString(PreferenceKeys::kExportFieldSeparator, stored);
        settings.open();
        EXPECT_EQ(settings.dialog().separatorComboBox().currentText(), QStringLiteral("SPACE"));
        EXPECT_EQ(settings.dialog().settings().separator, QStringLiteral(" "));
    }
}

// ------------------------------------------------------------------------------- the question

TEST_F(CsvImportSettingsDialogTest, WidgetUserPromptsAsksWithTheDialog)
{
    // UserPrompts::askWindCsvImportSettings(): the dialog, modal, starting with the settings
    // given; OK answers the settings shown.
    Gui::Test::TestGuiServices services;
    Settings initial   = CsvImportSettingsDialog::storedSettings(services.preferences());
    initial.speed.name = QStringLiteral("wind");
    initial.separator  = QStringLiteral("\t");
    WidgetUserPrompts       prompts;
    std::optional<Settings> answer;
    {
        const Gui::Test::NextModalDialog ok(&answerWithAnotherAltitudeName);
        answer = prompts.askWindCsvImportSettings(nullptr, initial);
        EXPECT_TRUE(ok.acted());
    }
    Settings expected      = initial;
    expected.altitude.name = QStringLiteral("height");
    EXPECT_EQ(answer, std::optional<Settings>(expected));

    // Cancel and Escape answer nothing.
    {
        const Gui::Test::NextModalDialog cancel(&clickCancel);
        EXPECT_EQ(prompts.askWindCsvImportSettings(nullptr, initial), std::nullopt);
    }
    {
        const Gui::Test::NextModalDialog escape(&pressEscape);
        EXPECT_EQ(prompts.askWindCsvImportSettings(nullptr, initial), std::nullopt);
    }
}

}  // namespace QtRocket::Gui
