#include "exit/SimulationsScenario.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "ManualSimulationExecutor.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/dialogs/simulation/AerodynamicLookupDialog.h"
#include "QtRocket/gui/dialogs/simulation/CsvImportSettingsDialog.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindEditDialog.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"
#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConfigDialog.h"
#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"
#include "QtRocket/gui/dialogs/simulation/SimulationWarningsPanel.h"
#include "QtRocket/gui/dialogs/simulation/WindLevelSummary.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"
#include "QtRocket/gui/simulation/SimulationColumnVisibility.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationBatch.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/CollapsiblePanel.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/motor/MotorChooserTestSupport.h"
#include "dialogs/simulation/WindEditorTranscript.h"
#include "exit/FlightConfigurationScenario.h"
#include "exit/FromScratchScenario.h"
#include "exit/ScenarioInput.h"
#include "exit/SimulationsPrompts.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "goldens/GoldenGuiReference.h"
#include "simulation/SimulationTablePinChecks.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui::Test
{

namespace
{

using QtRocket::Test::GuiConfirmAnswer;
using QtRocket::Test::GuiCsvImportAnswer;
using QtRocket::Test::GuiCsvImportState;
using QtRocket::Test::GuiFileAnswer;
using QtRocket::Test::GuiLookupAction;
using QtRocket::Test::GuiLookupActionKind;
using QtRocket::Test::GuiLookupAnswer;
using QtRocket::Test::GuiLookupSectionState;
using QtRocket::Test::GuiLookupState;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiRowClick;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiSettingsAnswer;
using QtRocket::Test::GuiSettingsDialogState;
using QtRocket::Test::GuiSettingsEdit;
using QtRocket::Test::GuiSettingsEditKind;
using QtRocket::Test::GuiSettingsField;
using QtRocket::Test::GuiSettingsFieldKind;
using QtRocket::Test::GuiSimulationsTabState;
using QtRocket::Test::GuiSimulationTableRow;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::GuiWindEdit;
using QtRocket::Test::GuiWindEditKind;
using QtRocket::Test::GuiWindEditorAnswer;
using QtRocket::Test::GuiWindEditorState;
using QtRocket::Test::GuiWindImportAnswer;
using QtRocket::Test::kGuiSettingsTabs;
using ScenarioInput::activateWindowOf;
using ScenarioInput::focusOn;
using ScenarioInput::settle;
using ScenarioInput::settleAndDelete;
using ScenarioInput::shortClassName;
using Column      = SimulationColumns::Column;
using LookupTable = AerodynamicLookupDialog::Table;
using CsvRow      = CsvImportSettingsDialog::Row;

/// The name of the window's third tab in the records.
constexpr std::string_view kSimulationsTabName = "simulations";

/// The settings dialog's pages that hold the fields an edit sets: Launch conditions, Simulation
/// options, Warnings.
constexpr int kSettingsPages = 3;

/// The sections of the lookup dialog by their names in the records, in the order of its tabs.
constexpr std::array<std::string_view, 2> kLookupSections{"drag", "stability"};

/// The rows of the CSV import settings, in the order of kGuiCsvImportRows.
constexpr std::array<CsvRow, 4> kCsvRows{CsvRow::ALTITUDE, CsvRow::SPEED, CsvRow::DIRECTION,
                                         CsvRow::STANDARD_DEVIATION};

/// @p text as UTF-8.
[[nodiscard]] std::string utf8(const QString& text)
{
    return QtStrings::toUtf8(text);
}

/// The text a button or a tab shows for @p text: without its mnemonic's mark.
[[nodiscard]] std::string shownText(const QString& text)
{
    return utf8(FromScratchScenario::shownMenuText(text));
}

// ============================================================================ the records

/// What the settings dialog @p dialog shows (the tool's settingsState()): its title, the name
/// field, the configuration box's item, the status label, the current tab and every tab, the
/// multi-simulation label.
[[nodiscard]] GuiSettingsDialogState settingsState(const SimulationConfigDialog& dialog)
{
    GuiSettingsDialogState state;
    state.title             = utf8(dialog.windowTitle());
    state.name              = utf8(dialog.nameField().text());
    state.configuration     = utf8(dialog.configurationComboBox().currentText());
    state.status            = utf8(dialog.statusText().text());
    const QTabWidget& tabs  = dialog.tabs();
    const int         shown = tabs.currentIndex();
    state.tab = shown >= 0 && std::cmp_less(shown, kGuiSettingsTabs.size())
                    ? std::string(kGuiSettingsTabs.at(static_cast<std::size_t>(shown)))
                    : std::format("tab {}", shown);
    for (int index = 0; index < tabs.count(); ++index)
    {
        state.tabs.push_back(
            {.tab     = std::cmp_less(index, kGuiSettingsTabs.size())
                            ? std::string(kGuiSettingsTabs.at(static_cast<std::size_t>(index)))
                            : std::format("tab {}", index),
             .title   = shownText(tabs.tabText(index)),
             .enabled = tabs.isTabEnabled(index)});
    }
    if (const StyledLabel* const multi = dialog.multiSimEditLabel(); multi != nullptr)
    {
        state.multiSimEdit = utf8(multi->text());
    }
    return state;
}

/// What the wind editor @p editor of @p model shows (the tool's windState()): its title, the
/// altitude reference's radio button and the probe's lines.
[[nodiscard]] GuiWindEditorState windState(const MultiLevelWindEditDialog&     editor,
                                           const MultiLevelPinkNoiseWindModel& model)
{
    GuiWindEditorState state;
    state.title = utf8(editor.windowTitle());
    if (editor.mslButton().isChecked())
    {
        state.altitudeReference = "MSL";
    }
    else if (editor.aglButton().isChecked())
    {
        state.altitudeReference = "AGL";
    }
    state.lines                         = windTableHeaderLines(editor.tableModel());
    const std::vector<std::string> rows = windTableRowLines(editor.tableModel(), model);
    state.lines.insert(state.lines.end(), rows.begin(), rows.end());
    return state;
}

/// What the CSV import settings @p dialog show (the tool's csvState()).
[[nodiscard]] GuiCsvImportState csvState(const CsvImportSettingsDialog& dialog)
{
    GuiCsvImportState state;
    state.title     = utf8(dialog.windowTitle());
    state.hasHeader = dialog.hasHeaderCheckBox().isChecked();
    for (std::size_t at = 0; at < kCsvRows.size(); ++at)
    {
        const CsvRow row     = kCsvRows.at(at);
        state.columns.at(at) = {.name  = utf8(dialog.columnField(row).text()),
                                .index = dialog.columnSpinner(row).value(),
                                .unit  = dialog.unitSelector(row).getSelectedUnit().getUnit()};
    }
    state.separator = utf8(dialog.separatorComboBox().currentText());
    return state;
}

/// What the section @p table of the lookup dialog @p dialog shows (the tool's lookupState()).
[[nodiscard]] GuiLookupSectionState lookupSectionState(const AerodynamicLookupDialog& dialog,
                                                       LookupTable                    table)
{
    GuiLookupSectionState state;
    const QLabel&         summary = dialog.summaryLabel(table);
    state.summary                 = QtRocket::Test::guiSortedLookupColumns(utf8(summary.text()));
    if (!summary.toolTip().isEmpty())
    {
        state.summaryToolTip = QtRocket::Test::guiSortedLookupColumns(utf8(summary.toolTip()));
    }
    state.border         = utf8(dialog.textBox(table).title());
    state.text           = utf8(dialog.textArea(table).toPlainText());
    state.editable       = !dialog.textArea(table).isReadOnly();
    state.separator      = utf8(dialog.separatorComboBox(table).currentText());
    state.clearEnabled   = dialog.clearButton(table).isEnabled();
    state.refreshEnabled = dialog.refreshButton(table).isEnabled();
    return state;
}

/// What the lookup dialog @p dialog shows: its title, its section, both sections.
[[nodiscard]] GuiLookupState lookupState(const AerodynamicLookupDialog& dialog)
{
    const int shown = dialog.tabs().currentIndex();
    return {.title     = utf8(dialog.windowTitle()),
            .tab       = shown >= 0 && std::cmp_less(shown, kLookupSections.size())
                             ? std::string(kLookupSections.at(static_cast<std::size_t>(shown)))
                             : std::format("tab {}", shown),
            .drag      = lookupSectionState(dialog, LookupTable::DRAG),
            .stability = lookupSectionState(dialog, LookupTable::STABILITY)};
}

/// The texts and check boxes of the message parts in @p layout, in their order (the tool's
/// messageParts(): a label's text, a check box's, a panel's parts).
void addMessageParts(const QLayout& layout, GuiQuestion& question)
{
    for (int index = 0; index < layout.count(); ++index)
    {
        QLayoutItem* const   item   = layout.itemAt(index);
        const QWidget* const widget = item != nullptr ? item->widget() : nullptr;
        if (const auto* const box = qobject_cast<const QCheckBox*>(widget); box != nullptr)
        {
            question.checkBoxes.push_back(utf8(box->text()));
        }
        else if (const auto* const label = qobject_cast<const QLabel*>(widget); label != nullptr)
        {
            question.message.push_back(utf8(label->text()));
        }
        else if (widget != nullptr && widget->layout() != nullptr)
        {
            addMessageParts(*widget->layout(), question);
        }
        else if (item != nullptr && item->layout() != nullptr)
        {
            addMessageParts(*item->layout(), question);
        }
    }
}

/// The question of a confirmation @p dialog as it opened (the tool's confirm()): its title, the
/// texts of its message, its check boxes and its buttons.
[[nodiscard]] GuiQuestion confirmQuestion(const OptionDialog& dialog)
{
    GuiQuestion question;
    question.kind  = GuiQuestionKind::CONFIRM;
    question.title = utf8(dialog.windowTitle());
    if (const QLayout* const layout = dialog.messageArea()->layout(); layout != nullptr)
    {
        addMessageParts(*layout, question);
    }
    for (const QAbstractButton* const button : dialog.buttonBox()->buttons())
    {
        question.buttons.push_back(shownText(button->text()));
    }
    return question;
}

/// The warnings box of the cell @p index as OpenRocket's table shows it in text (the tool's row():
/// a count's text, an icon as [HIGH], [NORMAL] or [LOW]).
[[nodiscard]] std::string warningsBoxText(const QModelIndex& index)
{
    const std::optional<SimulationColumns::WarningCounts> counts =
        SimulationWarningsDelegate::counts(index);
    if (!counts.has_value())
    {
        return {};
    }
    std::string text;
    for (const SimulationWarningsDelegate::Part& part :
         SimulationWarningsDelegate::layOut(*counts, QRect(0, 0, 400, 20), QFont()))
    {
        if (!part.icon.has_value())
        {
            text += utf8(part.text);
        }
        else if (*part.icon == Icons::Id::WARNING_HIGH)
        {
            text += "[HIGH]";
        }
        else if (*part.icon == Icons::Id::WARNING_NORMAL)
        {
            text += "[NORMAL]";
        }
        else
        {
            text += "[LOW]";
        }
    }
    return text;
}

/// The row @p row of @p model as the records have it (the tool's row()).
[[nodiscard]] GuiSimulationTableRow tableRow(const SimulationTableModel& model, int row)
{
    GuiSimulationTableRow    tableRow;
    const Simulation::Status status = model.status(row);
    tableRow.status                 = std::string(name(status));
    const auto* const sortText = std::get_if<std::string>(&model.cellValue(row, Column::STATUS));
    tableRow.statusSortText    = sortText != nullptr ? *sortText : std::string("<none>");
    tableRow.description =
        utf8(SimulationColumns::statusDescription(status, *model.simulation(row)));
    const QModelIndex warnings = model.index(row, SimulationColumns::indexOf(Column::WARNINGS));
    tableRow.warnings          = warningsBoxText(warnings);
    if (const std::optional<SimulationColumns::WarningCounts> counts = model.warningCounts(row);
        counts.has_value())
    {
        tableRow.warningCounts = {static_cast<int>(counts->critical),
                                  static_cast<int>(counts->normal),
                                  static_cast<int>(counts->informational)};
    }
    tableRow.warningsToolTip = utf8(model.data(warnings, Qt::ToolTipRole).toString());
    for (int column = SimulationColumns::indexOf(Column::NAME);
         column < SimulationColumns::kColumnCount; ++column)
    {
        const QVariant shown = model.data(model.index(row, column), Qt::DisplayRole);
        tableRow.cells.push_back(shown.isValid() ? std::optional(utf8(shown.toString()))
                                                 : std::nullopt);
    }
    return tableRow;
}

// ============================================================================ the widgets

/// Why a user cannot click @p button, named @p name: it is not shown, or disabled; "" when a user
/// can.
[[nodiscard]] std::string unclickable(const QAbstractButton& button, std::string_view name)
{
    if (!button.isVisible())
    {
        return std::format("the button {} is not shown", name);
    }
    return button.isEnabled() ? std::string() : std::format("the button {} is disabled", name);
}

/// Where a user clicks @p button: a check box or a radio button on its indicator and text (its
/// style's click rectangle: its own rectangle can be wider, a click beside the text does nothing),
/// another button in its middle.
[[nodiscard]] QPoint clickPoint(const QAbstractButton& button)
{
    const bool checkBox = qobject_cast<const QCheckBox*>(&button) != nullptr;
    if (!checkBox && qobject_cast<const QRadioButton*>(&button) == nullptr)
    {
        return button.rect().center();
    }
    QStyleOptionButton option;
    option.initFrom(&button);
    option.text     = button.text();
    option.icon     = button.icon();
    option.iconSize = button.iconSize();
    return button.style()
        ->subElementRect(checkBox ? QStyle::SE_CheckBoxClickRect : QStyle::SE_RadioButtonClickRect,
                         &option, &button)
        .center();
}

/// A click on @p button, which must be shown and enabled; what went wrong, or "".
[[nodiscard]] std::string click(QAbstractButton& button, std::string_view name)
{
    if (std::string refused = unclickable(button, name); !refused.empty())
    {
        return refused;
    }
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, clickPoint(button));
    return {};
}

/// A click on the tab @p index of @p tabs, which must be enabled; nothing when it is the current
/// one.
[[nodiscard]] std::string showTab(const QTabWidget& tabs, int index)
{
    if (tabs.currentIndex() == index)
    {
        return {};
    }
    if (!tabs.isTabEnabled(index))
    {
        return std::format("the tab {} is disabled", shownText(tabs.tabText(index)));
    }
    QTabBar* const bar = tabs.tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(index).center());
    settle();
    return tabs.currentIndex() == index ? std::string()
                                        : std::format("a click on the tab {} shows another one",
                                                      shownText(tabs.tabText(index)));
}

/// Brings @p widget into view as a user does (the tool's bringIntoView()): from the outermost in,
/// each tab page that holds it chosen by a click on its tab, and each collapsed section around it
/// expanded by a click on its header. What went wrong, or "".
[[nodiscard]] std::string bringIntoView(QWidget& widget)
{
    std::vector<QWidget*> chain{&widget};
    while (!chain.back()->isWindow() && chain.back()->parentWidget() != nullptr)
    {
        chain.push_back(chain.back()->parentWidget());
    }
    for (QWidget* const at : std::views::reverse(chain))
    {
        QWidget* const    parent = at->parentWidget();
        const auto* const stack  = qobject_cast<const QStackedWidget*>(parent);
        const auto* const tabs =
            stack != nullptr ? qobject_cast<const QTabWidget*>(stack->parentWidget()) : nullptr;
        if (tabs != nullptr && tabs->indexOf(at) >= 0)
        {
            if (std::string problem = showTab(*tabs, tabs->indexOf(at)); !problem.empty())
            {
                return problem;
            }
        }
        if (const auto* const section = qobject_cast<const CollapsiblePanel*>(at);
            section != nullptr && !section->isExpanded())
        {
            QTest::mouseClick(section->headerButton(), Qt::LeftButton);
            settle();
            if (!section->isExpanded())
            {
                return "a click on a collapsed section's header leaves it collapsed";
            }
        }
    }
    return {};
}

/// The one widget of class W named @p name below @p root; null when there is none or more.
template <class W>
[[nodiscard]] W* onlyChild(const QWidget& root, const QString& name)
{
    const QList<W*> found = root.findChildren<W*>(name);
    return found.size() == 1 ? found.front() : nullptr;
}

/// The field of the settings dialog @p dialog that the Java property @p property names (its
/// objectName) on its first three pages: a number field, a check box or a combo box of @p kind;
/// null with what went wrong in @p problem when there is not exactly one.
[[nodiscard]] QWidget* settingsField(const SimulationConfigDialog& dialog,
                                     const GuiSettingsField& field, std::string& problem)
{
    const QString         name = QtStrings::fromUtf8(field.property);
    std::vector<QWidget*> found;
    for (int page = 0; page < kSettingsPages; ++page)
    {
        const QWidget* const content = dialog.tabs().widget(page);
        for (QWidget* const child : content->findChildren<QWidget*>(name))
        {
            const bool matches = (field.kind == GuiSettingsFieldKind::SPINNER &&
                                  qobject_cast<QAbstractSpinBox*>(child) != nullptr) ||
                                 (field.kind == GuiSettingsFieldKind::CHECK_BOX &&
                                  qobject_cast<QCheckBox*>(child) != nullptr) ||
                                 (field.kind == GuiSettingsFieldKind::COMBO_BOX &&
                                  qobject_cast<QComboBox*>(child) != nullptr);
            if (matches)
            {
                found.push_back(child);
            }
        }
    }
    if (found.size() != 1)
    {
        problem = std::format("the settings dialog has {} fields {}", found.size(), field.property);
        return nullptr;
    }
    return found.front();
}

/// The index of the item of the combo box @p combo whose value is the Java enum constant @p item
/// (@p name gives a value's constant); nullopt when @p combo is no combo box of E or has no such
/// item.
template <class E>
[[nodiscard]] std::optional<int> enumItemIndex(const EnumComboBox& combo, std::string_view item,
                                               std::string_view (*name)(E))
{
    const std::shared_ptr<EnumBinding<E>> binding = combo.enumBinding<E>();
    if (binding == nullptr)
    {
        return std::nullopt;
    }
    for (int index = 0; index < binding->size(); ++index)
    {
        const std::optional<E> value = binding->elementAt(index);
        if (value.has_value() && name(*value) == item)
        {
            return index;
        }
    }
    return std::nullopt;
}

/// The index of the item of a combo box of the settings dialog whose value is the Java enum
/// constant @p item; nullopt when there is none.
[[nodiscard]] std::optional<int> settingsItemIndex(const QComboBox& combo, std::string_view item)
{
    const auto* const enumCombo = qobject_cast<const EnumComboBox*>(&combo);
    if (enumCombo == nullptr)
    {
        return std::nullopt;
    }
    for (const std::optional<int> index : {
             enumItemIndex<SimulationStepperMethod>(*enumCombo, item, simulationStepperMethodName),
             enumItemIndex<GeodeticComputationStrategy>(*enumCombo, item, QtRocket::name),
             enumItemIndex<GravityModelType>(*enumCombo, item, gravityModelTypeName),
         })
    {
        if (index.has_value())
        {
            return index;
        }
    }
    return std::nullopt;
}

/// @p text typed over the number field @p spinBox named @p field, its unit chosen first in
/// @p selector (for a field without one, @p unit must be the unit it shows), committed with Enter;
/// a value the field shows already is refused (the tool's fieldEdit()). What went wrong, or "".
[[nodiscard]] std::string typeValue(UnitSpinBox& spinBox, UnitSelector* selector,
                                    const std::optional<std::string>& unit, const std::string& text,
                                    std::string_view field)
{
    const std::shared_ptr<DoubleBinding>& binding = spinBox.binding();
    if (selector != nullptr)
    {
        if (!unit.has_value())
        {
            return std::format("the field {} has a unit selector: the edit needs a unit", field);
        }
        if (std::string problem = ScenarioInput::chooseUnit(*selector, field, *unit);
            !problem.empty())
        {
            return problem;
        }
    }
    else if (unit.has_value() && binding->getCurrentUnit().getUnit() != *unit)
    {
        return std::format("the field {} shows {}, not {} (it has no unit selector)", field,
                           binding->getCurrentUnit().getUnit(), *unit);
    }
    const std::optional<double> typed = Strings::javaParseDouble(text);
    if (!typed.has_value())
    {
        return std::format("the field {} does not take '{}'", field, text);
    }
    if (binding->getCurrentUnit().fromUnit(*typed) == binding->value())
    {
        return std::format("the field {} shows that value already", field);
    }
    return ScenarioInput::typeNumber(spinBox, field, text);
}

/// @p text typed over the text field @p line (no Enter); what went wrong, or "".
[[nodiscard]] std::string typeText(QLineEdit& line, std::string_view field, const std::string& text)
{
    if (std::string problem = focusOn(line, field); !problem.empty())
    {
        return problem;
    }
    line.selectAll();
    if (text.empty())
    {
        QTest::keyClick(&line, Qt::Key_Delete);
    }
    else
    {
        QTest::keyClicks(&line, QtStrings::fromUtf8(text));
    }
    settle();
    return line.text() == QtStrings::fromUtf8(text)
               ? std::string()
               : std::format("the field {} shows '{}' after '{}' was typed", field,
                             utf8(line.text()), text);
}

/// The separator @p separator chosen in @p box (the tool's chooseSeparator()): the item SPACE
/// for " ", TAB for a tab, the item of its text, else the text typed into the box. A separator
/// the box shows already is refused. What went wrong, or "".
[[nodiscard]] std::string chooseSeparator(FieldSeparatorComboBox& box, const std::string& separator)
{
    QString item = QtStrings::fromUtf8(separator);
    if (separator == " ")
    {
        item = FieldSeparatorComboBox::spaceText();
    }
    else if (separator == "\t")
    {
        item = FieldSeparatorComboBox::tabText();
    }
    if (box.currentText() == item)
    {
        return std::format("the separator box shows {} already", utf8(item));
    }
    if (const int index = box.findText(item, Qt::MatchExactly); index >= 0)
    {
        chooseItem(box, index);
    }
    else
    {
        typeIntoComboBox(box, item);
    }
    settle();
    return box.currentText() == item ? std::string()
                                     : std::format("the separator box shows {} after {} was chosen",
                                                   utf8(box.currentText()), utf8(item));
}

/// The settings dialog's button of the page @p page ("Reset to default" when @p reset, else
/// "Save as default").
[[nodiscard]] QPushButton* defaultsButton(const SimulationConfigDialog& dialog,
                                          std::string_view page, bool reset)
{
    if (page == kGuiSettingsTabs.at(0))
    {
        return reset ? dialog.conditionsPanel().resetButton()
                     : dialog.conditionsPanel().saveButton();
    }
    if (page == kGuiSettingsTabs.at(1))
    {
        return reset ? &dialog.optionsPanel().resetButton() : &dialog.optionsPanel().saveButton();
    }
    if (page == kGuiSettingsTabs.at(2))
    {
        return reset ? &dialog.warningsPanel().resetButton() : &dialog.warningsPanel().saveButton();
    }
    return nullptr;
}

/// The state of @p dialog's first simulation that an edit must change (the tool's
/// simulationState(), with the dialog's tab): its name, configuration and options.
[[nodiscard]] std::string simulationState(const SimulationConfigDialog& dialog)
{
    const Simulation& simulation = *dialog.simulations().front();
    std::string       text       = simulation.getName() + "|" +
                                   simulation.getFlightConfigurationId().toString() + "|" +
                                   std::to_string(dialog.tabs().currentIndex());
    for (const std::string& line : QtRocket::Test::describeConditions(simulation.getOptions()))
    {
        text += "|" + line;
    }
    return text;
}

/// Whether @p action of the lookup dialog must change what the dialog shows (the tool's lookup()):
/// a separator, a text and a load whose file chooser is answered with Open do; Refresh and Clear
/// are clicks on buttons, refused while disabled, and a cancelled load changes nothing.
[[nodiscard]] bool lookupActionMustChange(const GuiLookupAction& action)
{
    switch (action.kind)
    {
        case GuiLookupActionKind::SEPARATOR:
        case GuiLookupActionKind::TEXT:
            return true;
        case GuiLookupActionKind::LOAD:
            return action.file.button == "open";
        case GuiLookupActionKind::REFRESH:
        case GuiLookupActionKind::CLEAR:
            return false;
    }
    return false;
}

/// The model column of the name, where the scenario clicks a row (the tool's clickColumn(): a
/// click on the status column runs a simulation).
constexpr Column kClickColumn = Column::NAME;

/// The failure of an answer whose button @p button left @p dialog open (the tool's answerInLoop():
/// a dialog its answer does not close fails the step).
[[nodiscard]] std::string leftOpen(std::string_view button, const QWidget& dialog)
{
    return std::format("the answer's {} left the dialog '{}' open", button,
                       utf8(dialog.windowTitle()));
}

/// How long the run dialog may stay open after its Cancel before the scenario closes it: only a
/// Cancel that never closes the dialog reaches it (the step would hang otherwise).
constexpr std::chrono::seconds kRunDialogSafetyNet{120};

/// Closes @p dialog, a dialog of a failed answer, so that the operation goes on.
void rejectDialog(QWidget& dialog)
{
    if (auto* const shown = qobject_cast<QDialog*>(&dialog); shown != nullptr)
    {
        shown->reject();
    }
}

/// The settings dialog's name field set to @p name (typed over its text); what went wrong, or "".
[[nodiscard]] std::string nameEdit(const SimulationConfigDialog& dialog, const std::string& name)
{
    if (std::string shown = bringIntoView(dialog.nameField()); !shown.empty())
    {
        return shown;
    }
    return typeText(dialog.nameField(), "nameField", name);
}

/// The configuration box set to the rocket's configuration of @p row (its place among the
/// rocket's); what went wrong, or "".
[[nodiscard]] std::string configurationEdit(const SimulationConfigDialog& dialog, int row)
{
    const std::vector<FlightConfigurationId> ids =
        dialog.simulations().front()->getRocket().getIds();
    if (row < 0 || std::cmp_greater_equal(row, ids.size()))
    {
        return std::format("the rocket has no configuration {}", row);
    }
    ConfigurationComboBox& box   = dialog.configurationComboBox();
    int                    index = -1;
    for (int item = 0; item < box.count(); ++item)
    {
        if (box.configurationIdAt(item) == ids.at(static_cast<std::size_t>(row)))
        {
            index = item;
        }
    }
    if (index < 0)
    {
        return std::format("the configuration box has no item of configuration {}", row);
    }
    if (!activateWindowOf(box))
    {
        return "the settings dialog does not become active";
    }
    chooseItem(box, index);
    return {};
}

/// A click on the settings dialog's tab @p tab, one of its first three that is not the current
/// one; what went wrong, or "".
[[nodiscard]] std::string tabEdit(const SimulationConfigDialog& dialog, std::string_view tab)
{
    const std::span<const std::string_view> pages(kGuiSettingsTabs.data(), kSettingsPages);
    const auto                              found = std::ranges::find(pages, tab);
    if (found == pages.end())
    {
        return std::format("the tab {} is none of the settings pages", tab);
    }
    const int index = static_cast<int>(found - pages.begin());
    if (dialog.tabs().currentIndex() == index)
    {
        return std::format("the tab {} is the current one already", tab);
    }
    return showTab(dialog.tabs(), index);
}

/// The wind model's radio button of @p model ("AVERAGE", "MULTI_LEVEL"), which must not be
/// chosen; what went wrong, or "".
[[nodiscard]] std::string windModelEdit(const SimulationConfigDialog& dialog,
                                        const std::string&            model)
{
    const SimulationConditionsPanel& conditions = dialog.conditionsPanel();
    QRadioButton* const              radio =
        model == "MULTI_LEVEL" ? conditions.multiLevelWindButton() : conditions.averageWindButton();
    if (radio == nullptr || (model != "MULTI_LEVEL" && model != "AVERAGE"))
    {
        return std::format("the launch conditions have no wind model {}", model);
    }
    if (std::string shown = bringIntoView(*radio); !shown.empty())
    {
        return shown;
    }
    if (radio->isChecked())
    {
        return std::format("the wind model is {} already", model);
    }
    return click(*radio, model);
}

/// "Fixed random seed" of the options page: cleared (@p fixed false), or ticked when it is not
/// and @p text typed into its field; what went wrong, or "".
[[nodiscard]] std::string randomSeedEdit(const SimulationConfigDialog& dialog, bool fixed,
                                         const std::optional<std::string>& text)
{
    const RandomSeedControls& seed = dialog.optionsPanel().randomSeed();
    if (std::string shown = bringIntoView(seed.checkBox()); !shown.empty())
    {
        return shown;
    }
    if (!fixed)
    {
        return seed.checkBox().isChecked() ? click(seed.checkBox(), "Fixed random seed")
                                           : std::string("the random seed is not fixed already");
    }
    if (!text.has_value())
    {
        return "a fixed seed needs its text";
    }
    if (!seed.checkBox().isChecked())
    {
        if (std::string problem = click(seed.checkBox(), "Fixed random seed"); !problem.empty())
        {
            return problem;
        }
        settle();
    }
    return typeText(seed.field(), "randomSeedField", *text);
}

/// "Reset to default" (@p what "reset") or "Save as default" ("save") of the page @p page; what
/// went wrong, or "".
[[nodiscard]] std::string defaultsEdit(const SimulationConfigDialog& dialog, std::string_view page,
                                       const std::string& what)
{
    QPushButton* const button = defaultsButton(dialog, page, what == "reset");
    if (button == nullptr || (what != "reset" && what != "save"))
    {
        return std::format("the page {} has no button {}", page, what);
    }
    if (std::string shown = bringIntoView(*button); !shown.empty())
    {
        return shown;
    }
    return click(*button, what);
}

/// The combo box @p combo of the field @p field set to the Java enum constant @p item; what went
/// wrong, or "".
[[nodiscard]] std::string chooseSettingsItem(QComboBox& combo, std::string_view field,
                                             const std::optional<std::string>& item)
{
    if (!item.has_value())
    {
        return std::format("the field {} is not set this way", field);
    }
    const std::optional<int> index = settingsItemIndex(combo, *item);
    if (!index.has_value())
    {
        return std::format("the combo box {} offers no {}", field, *item);
    }
    if (combo.currentIndex() == *index)
    {
        return std::format("the combo box {} shows {} already", field, *item);
    }
    chooseItem(combo, *index);
    return {};
}

/// The wind editor's radio button of @p reference ("MSL", "AGL"), which must not be chosen;
/// what went wrong, or "".
[[nodiscard]] std::string altitudeReferenceEdit(const MultiLevelWindEditDialog& editor,
                                                const std::string&              reference)
{
    if (reference != "MSL" && reference != "AGL")
    {
        return std::format("the altitude reference is MSL or AGL, not {}", reference);
    }
    QRadioButton& radio = reference == "MSL" ? editor.mslButton() : editor.aglButton();
    if (radio.isChecked())
    {
        return std::format("the altitude reference is {} already", reference);
    }
    return click(radio, reference);
}

/// The column of the wind table @p column names (one of kGuiWindColumns); -1 for none.
[[nodiscard]] int windColumn(std::string_view column)
{
    for (std::size_t at = 0; at < QtRocket::Test::kGuiWindColumns.size(); ++at)
    {
        if (QtRocket::Test::kGuiWindColumns.at(at) == column)
        {
            return static_cast<int>(at);
        }
    }
    return -1;
}

/// A click on the cell of @p row and @p column of the wind editor's table, scrolled into view;
/// what went wrong, or "".
[[nodiscard]] std::string clickWindCell(MultiLevelWindTableView& table, int row, int column)
{
    const MultiLevelWindTableModel& model = table.levelModel();
    if (row < 0 || row >= model.rowCount())
    {
        return std::format("the wind table has no row {}", row);
    }
    if (!activateWindowOf(table))
    {
        return "the wind editor does not become active";
    }
    const QModelIndex cell = model.index(row, column);
    table.scrollTo(cell);
    const QRect rect = table.visualRect(cell);
    if (!table.viewport()->rect().contains(rect.center()))
    {
        return std::format("the cell {} {} is not in view", row, column);
    }
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    settle();
    return {};
}

/// @p text typed into the editor of a wind table's cell a click opened (in its column's unit)
/// and committed with Enter; a value the cell shows already is refused. What went wrong, or "".
[[nodiscard]] std::string typeIntoWindCell(const MultiLevelWindTableView& table,
                                           const std::string&             text)
{
    auto* const spinBox = qobject_cast<UnitSpinBox*>(table.openEditor());
    if (spinBox == nullptr)
    {
        return "a click on the cell opens no editor";
    }
    const std::optional<double> typed = Strings::javaParseDouble(text);
    if (!typed.has_value())
    {
        return std::format("the cell does not take '{}'", text);
    }
    const std::shared_ptr<DoubleBinding>& binding = spinBox->binding();
    if (binding->getCurrentUnit().fromUnit(*typed) == binding->value())
    {
        return "the cell shows that value already";
    }
    typeInto(*spinBox, QtStrings::fromUtf8(text));
    QTest::keyClick(spinBox, Qt::Key_Return);
    settle();
    return {};
}

/// deleteLevel (the row selected by a click on its intensity cell, then "Delete level"),
/// deleteRow (a click on the row's delete icon) and cell (a click on the cell, the text typed)
/// of the wind editor @p editor; what went wrong, or "".
[[nodiscard]] std::string windRowEdit(const MultiLevelWindEditDialog& editor,
                                      const GuiWindEdit&              edit)
{
    MultiLevelWindTableView& table = editor.table();
    switch (edit.kind)
    {
        case GuiWindEditKind::DELETE_ROW:
        {
            const MultiLevelWindTableModel& model = editor.tableModel();
            if (edit.row >= 0 && edit.row < model.rowCount() &&
                !model.flags(model.index(edit.row, MultiLevelWindTableModel::kDeleteColumn))
                     .testFlag(Qt::ItemIsEnabled))
            {
                return "the row's delete button is disabled";
            }
            return clickWindCell(table, edit.row, MultiLevelWindTableModel::kDeleteColumn);
        }
        case GuiWindEditKind::DELETE_LEVEL:
        {
            if (std::string problem =
                    clickWindCell(table, edit.row, MultiLevelWindTableModel::kIntensityColumn);
                !problem.empty())
            {
                return problem;
            }
            if (table.selectedRow() != edit.row)
            {
                return std::format("a click on the row {} selects the row {}", edit.row,
                                   table.selectedRow());
            }
            return click(editor.deleteButton(), "Delete level");
        }
        default:
        {
            const int column = windColumn(edit.column);
            if (column < 0)
            {
                return std::format("the wind table has no column {}", edit.column);
            }
            if (std::string problem = clickWindCell(table, edit.row, column); !problem.empty())
            {
                return problem;
            }
            return typeIntoWindCell(table, edit.text);
        }
    }
}

/// @p unit chosen in the unit selector of the wind table's column @p column (its header's); what
/// went wrong, or "".
[[nodiscard]] std::string windColumnUnitEdit(const MultiLevelWindEditDialog& editor,
                                             const std::string& column, const std::string& unit)
{
    const int index = windColumn(column);
    if (index < 0 || index >= MultiLevelWindTableModel::kNumberColumnCount)
    {
        return std::format("the wind table has no column {}", column);
    }
    UnitSelector& selector = editor.table().unitSelector(index);
    if (selector.getSelectedUnit().getUnit() == unit)
    {
        return std::format("the column shows {} already", unit);
    }
    return ScenarioInput::chooseUnit(selector, column, unit);
}

/// Refuses to set @p widget, @p what of the CSV import settings, unless a user can (the tool's
/// requireSettable()): it is shown and enabled. What went wrong, or "".
[[nodiscard]] std::string csvSettable(const QWidget& widget, const std::string& what)
{
    if (!widget.isVisible())
    {
        return what + " is not shown";
    }
    return widget.isEnabled() ? std::string() : what + " is disabled";
}

/// One row of the CSV import settings @p dialog as @p column answers it (the tool's
/// csvColumn()): the column name typed, the index typed (committed as the focus leaves it:
/// Return would click OK, the dialog's default button) and the unit chosen, each only while it
/// is shown (the name with the header box on, the index with it off) and enabled, and refused
/// when it shows that value already; what went wrong, or "".
[[nodiscard]] std::string csvColumnEdit(const CsvImportSettingsDialog&            dialog,
                                        const QtRocket::Test::GuiCsvColumnAnswer& column)
{
    std::optional<CsvRow> found;
    for (std::size_t at = 0; at < QtRocket::Test::kGuiCsvImportRows.size(); ++at)
    {
        if (QtRocket::Test::kGuiCsvImportRows.at(at) == column.row)
        {
            found = kCsvRows.at(at);
        }
    }
    if (!found.has_value())
    {
        return "the CSV import settings have no row " + column.row;
    }
    const CsvRow row = *found;
    if (column.name.has_value())
    {
        QLineEdit& field = dialog.columnField(row);
        if (std::string problem = csvSettable(field, "the column name of " + column.row);
            !problem.empty())
        {
            return problem;
        }
        if (field.text() == QtStrings::fromUtf8(*column.name))
        {
            return std::format("the column name of {} is {} already", column.row, *column.name);
        }
        if (std::string problem = typeText(field, column.row + "ColumnField", *column.name);
            !problem.empty())
        {
            return problem;
        }
    }
    if (column.index.has_value())
    {
        QSpinBox& spinner = dialog.columnSpinner(row);
        if (std::string problem = csvSettable(spinner, "the column index of " + column.row);
            !problem.empty())
        {
            return problem;
        }
        if (spinner.value() == *column.index)
        {
            return std::format("the column index of {} is {} already", column.row, *column.index);
        }
        typeInto(spinner, QString::number(*column.index));
        spinner.clearFocus();
        settle();
        if (spinner.value() != *column.index)
        {
            return "the column index of " + column.row + " does not take its value";
        }
    }
    if (!column.unit.has_value())
    {
        return {};
    }
    UnitSelector& selector = dialog.unitSelector(row);
    if (std::string problem = csvSettable(selector, "the unit of " + column.row); !problem.empty())
    {
        return problem;
    }
    if (selector.getSelectedUnit().getUnit() == *column.unit)
    {
        return std::format("the unit of {} is {} already", column.row, *column.unit);
    }
    return ScenarioInput::chooseUnit(selector, column.row, *column.unit);
}

/// A click on @p ok or @p cancel of the dialog @p what as @p button says ("ok", "cancel"); what
/// went wrong, or "".
[[nodiscard]] std::string okOrCancel(QPushButton& ok, QPushButton& cancel,
                                     const std::string& button, std::string_view what)
{
    if (button == "ok")
    {
        return click(ok, "OK");
    }
    if (button == "cancel")
    {
        return click(cancel, "Cancel");
    }
    return std::format("the buttons of {} are ok and cancel", what);
}

/// The text of the button an answer to a confirmation names ("ok", "cancel", "yes", "no"), as
/// the button shows it; empty for another name.
[[nodiscard]] std::string_view confirmButtonText(std::string_view button)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kButtons{
        std::pair{std::string_view("ok"), std::string_view("OK")},
        std::pair{std::string_view("cancel"), std::string_view("Cancel")},
        std::pair{std::string_view("yes"), std::string_view("Yes")},
        std::pair{std::string_view("no"), std::string_view("No")}};
    for (const auto& [name, text] : kButtons)
    {
        if (name == button)
        {
            return text;
        }
    }
    return {};
}

/// A field of the settings dialog (the tool's fieldEdit()): found by its Java property, brought
/// into view, shown and enabled, then set as its kind is (see SimulationsScenario); what went
/// wrong, or "".
[[nodiscard]] std::string fieldEdit(const SimulationConfigDialog& dialog,
                                    const GuiSettingsEdit&        edit)
{
    const GuiSettingsField* const field = QtRocket::Test::guiSettingsField(edit.field);
    if (field == nullptr)
    {
        return std::format("{} is no field of the settings dialog", edit.field);
    }
    std::string    problem;
    QWidget* const widget = settingsField(dialog, *field, problem);
    if (widget == nullptr)
    {
        return problem;
    }
    if (problem = bringIntoView(*widget); !problem.empty())
    {
        return problem;
    }
    if (!widget->isVisible() || !widget->isEnabled())
    {
        return std::format("the field {} is not shown (another card of its panel) or disabled",
                           edit.field);
    }
    if (!activateWindowOf(*widget))
    {
        return "the settings dialog does not become active";
    }
    if (auto* const spinBox = qobject_cast<UnitSpinBox*>(widget); spinBox != nullptr)
    {
        return edit.text.has_value()
                   ? typeValue(*spinBox, onlyChild<UnitSelector>(dialog, widget->objectName()),
                               edit.unit, *edit.text, edit.field)
                   : std::format("the edit of the field {} has no text", edit.field);
    }
    if (auto* const box = qobject_cast<QCheckBox*>(widget); box != nullptr)
    {
        if (!edit.checked.has_value() || box->isChecked() == *edit.checked)
        {
            return std::format("the check box {} is in that state already", edit.field);
        }
        return click(*box, edit.field);
    }
    auto* const combo = qobject_cast<QComboBox*>(widget);
    return combo != nullptr ? chooseSettingsItem(*combo, edit.field, edit.item)
                            : std::format("the field {} is not set this way", edit.field);
}

}  // namespace

// ================================================================================ the scenario

SimulationsScenario::SimulationsScenario(std::filesystem::path directory)
  : FlightConfigurationScenario(
        std::move(directory),
        [](UserPrompts& scripted) { return std::make_unique<SimulationsPrompts>(scripted); },
        std::make_unique<ManualSimulationExecutor>(ManualSimulationExecutor::Mode::NEVER)),
    m_simulationPrompts(dynamic_cast<SimulationsPrompts*>(&scenarioPrompts())),
    m_executor(dynamic_cast<ManualSimulationExecutor*>(
        session().context().services().simulationExecutor()))
{
    QTROCKET_ASSERT(m_simulationPrompts != nullptr);
    QTROCKET_ASSERT(m_executor != nullptr);
    m_simulationPrompts->setOpenFileObserver(
        [this](const UserPrompts::OpenFileRequest& request) { fileChooserAsked(request.title); });
}

SimulationsScenario::~SimulationsScenario()
{
    // The application's questions go with the application, after this object.
    m_simulationPrompts->setOpenFileObserver({});
    static_cast<void>(m_simulationPrompts->takeUnusedAnswers());
}

SimulationPanel* SimulationsScenario::simulationsTab() const
{
    const MainWindow* const shown = window();
    return shown == nullptr ? nullptr : shown->componentSimulationPanel();
}

GuiStepRecord SimulationsScenario::record(const GuiScenarioStep& step) const
{
    GuiStepRecord           record = FlightConfigurationScenario::record(step);
    const MainWindow* const shown  = window();
    if (shown == nullptr)
    {
        return record;
    }
    if (shown->selectedTab() == MainWindow::kSimulationTab)
    {
        record.tab = std::string(kSimulationsTabName);
    }
    if (QtRocket::Test::isGuiSimulationStep(step.kind) ||
        (step.kind == GuiStepKind::TAB && step.tab == kSimulationsTabName))
    {
        record.questions = m_questions;
    }
    if (const SimulationPanel* const panel = simulationsTab(); panel != nullptr)
    {
        record.simulationsTab = simulationsTabState(*panel);
    }
    record.simulations   = QtRocket::Test::guiSimulationStates(shown->document());
    record.clipboardText = m_clipboardText;
    return record;
}

GuiSimulationsTabState SimulationsScenario::simulationsTabState(const SimulationPanel& panel)
{
    GuiSimulationsTabState      state;
    const SimulationTableModel& model = panel.tableModel();
    const SimulationTableView&  table = panel.table();
    state.showsHelp                   = panel.isHelpShown();
    for (const Column column : table.visibleColumns())
    {
        state.columns.push_back({.id     = std::string(SimulationColumns::id(column)),
                                 .header = utf8(model
                                                    .headerData(SimulationColumns::indexOf(column),
                                                                Qt::Horizontal, Qt::DisplayRole)
                                                    .toString())});
    }
    for (int row = 0; row < model.rowCount(); ++row)
    {
        state.rows.push_back(tableRow(model, row));
    }
    for (const std::shared_ptr<Simulation>& simulation : table.selectedSimulations())
    {
        state.selectedRows.push_back(model.rowOf(*simulation));
    }
    std::ranges::sort(state.selectedRows);
    for (const QAction* const action : simulationPanelPinActions(panel))
    {
        state.actions.emplace(simulationPanelActionName(*action), action->isEnabled());
    }
    if (const std::shared_ptr<const SimulationCopy> copy = panel.clipboard().simulations();
        copy != nullptr)
    {
        for (std::size_t index = 0; index < copy->size(); ++index)
        {
            state.clipboardSimulations.push_back(copy->getEntry(index).name);
        }
    }
    if (const DocumentPreferences& preferences = panel.document().getDocumentPreferences();
        preferences.contains(GuiPreferenceKeys::kSimulationTableHiddenColumns))
    {
        state.hiddenColumnsPreference =
            preferences.getString(GuiPreferenceKeys::kSimulationTableHiddenColumns, "");
    }
    return state;
}

std::string SimulationsScenario::perform(const GuiScenarioStep& step)
{
    m_questions.clear();
    m_clipboardText.reset();
    m_fileAnswers.clear();
    m_problem.clear();
    m_simulationStep = QtRocket::Test::isGuiSimulationStep(step.kind);
    if (step.kind == GuiStepKind::TAB && step.tab == kSimulationsTabName)
    {
        return showSimulationsTab(step);
    }
    switch (step.kind)
    {
        case GuiStepKind::SELECT_SIMULATIONS:
            return selectRows(step);
        case GuiStepKind::NEW_SIMULATION:
        case GuiStepKind::EDIT_SIMULATION:
            return settingsOperation(step);
        case GuiStepKind::DELETE_SIMULATIONS:
            return deleteOperation(step);
        case GuiStepKind::DUPLICATE_SIMULATIONS:
        case GuiStepKind::COPY_SIMULATIONS:
        case GuiStepKind::CUT_SIMULATIONS:
        case GuiStepKind::PASTE_SIMULATIONS:
            return menuOperation(step);
        case GuiStepKind::COLUMN_VISIBILITY:
            return columnVisibility(step);
        default:
            // The steps of versions 1 and 2.
            return FlightConfigurationScenario::perform(step);
    }
}

// =================================================================================== the steps

SimulationPanel* SimulationsScenario::shownPanel(std::string& problem) const
{
    SimulationPanel* const panel = simulationsTab();
    if (panel == nullptr)
    {
        problem = "the scenario's window has no Flight simulations tab";
        return nullptr;
    }
    if (scenarioWindow().selectedTab() != MainWindow::kSimulationTab)
    {
        problem = "the Flight simulations tab is not shown";
        return nullptr;
    }
    return panel;
}

std::string SimulationsScenario::showSimulationsTab(const GuiScenarioStep& /*step*/)
{
    if (window() == nullptr)
    {
        return "the scenario's window is not open";
    }
    const QTabWidget& tabs = *scenarioWindow().tabs();
    if (tabs.currentIndex() == MainWindow::kSimulationTab)
    {
        return "the Flight simulations tab is shown already";
    }
    if (!activateWindowOf(tabs))
    {
        return "the window does not become active";
    }
    if (std::string problem = showTab(tabs, MainWindow::kSimulationTab); !problem.empty())
    {
        return problem;
    }
    // The tab takes the spotlight in the next turn of the event loop (takeTheSpotlight()).
    settle();
    return {};
}

std::string SimulationsScenario::selectRows(const GuiScenarioStep& step)
{
    std::string                  problem;
    const SimulationPanel* const panel = shownPanel(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    SimulationTableView&        table = panel->table();
    const SimulationTableModel& model = panel->tableModel();
    if (!activateWindowOf(table))
    {
        return "the window does not become active";
    }
    if (step.rows.empty())
    {
        if (table.selectedSimulations().empty())
        {
            return "no row of the simulations table is selected: the click checks nothing";
        }
        // A click below the last row.
        const int last = model.rowCount() - 1;
        const int below =
            last < 0 ? 1 : table.rowViewportPosition(last) + table.rowHeight(last) + 2;
        const QPoint point(table.viewport()->width() / 2, below);
        if (!table.viewport()->rect().contains(point))
        {
            return "the table shows no space below its last row";
        }
        QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, point);
        settle();
        return {};
    }
    if (!table.isColumnVisible(kClickColumn))
    {
        return "the table does not show its name column";
    }
    for (const GuiRowClick& click : step.rows)
    {
        if (click.row < 0 || click.row >= model.rowCount())
        {
            return std::format("the simulations table has no row {}", click.row);
        }
        const QModelIndex cell = model.index(click.row, SimulationColumns::indexOf(kClickColumn));
        table.scrollTo(cell);
        const QRect rect = table.visualRect(cell);
        if (!table.viewport()->rect().contains(rect.center()))
        {
            return std::format("the row {} is not in view", click.row);
        }
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        if (click.with == "ctrl")
        {
            modifiers = Qt::ControlModifier;
        }
        else if (click.with == "shift")
        {
            modifiers = Qt::ShiftModifier;
        }
        QTest::mouseClick(table.viewport(), Qt::LeftButton, modifiers, rect.center());
        settle();
    }
    return {};
}

std::string SimulationsScenario::clickWithAnswers(QAbstractButton& button, std::string_view name)
{
    const std::size_t unanswered = m_simulationPrompts->unansweredSimulationQuestions();
    const std::size_t files      = m_unansweredFiles;
    std::string       problem    = click(button, name);
    settleAndDelete();
    if (problem.empty() && m_simulationPrompts->unansweredSimulationQuestions() != unanswered)
    {
        problem = std::format("{} asked a question the step has no answer for", name);
    }
    if (problem.empty() && m_unansweredFiles != files)
    {
        problem = std::format("{} asked for a file the step has no answer for", name);
    }
    return problem;
}

std::string SimulationsScenario::answerProblems()
{
    std::string problem = m_problem;
    if (const std::vector<std::string> unused = m_simulationPrompts->takeUnusedAnswers();
        problem.empty() && !unused.empty())
    {
        problem = std::format("no dialog took {}", unused.front());
    }
    if (problem.empty() && !m_fileAnswers.empty())
    {
        problem = "no file chooser took the answer for the file " + m_fileAnswers.front().file;
    }
    m_fileAnswers.clear();
    return problem;
}

void SimulationsScenario::fail(const std::string& problem)
{
    if (m_problem.empty() && !problem.empty())
    {
        m_problem = problem;
    }
}

std::string SimulationsScenario::settingsOperation(const GuiScenarioStep& step)
{
    std::string                  problem;
    const SimulationPanel* const panel = shownPanel(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    if (!step.settings.has_value())
    {
        return "the step has no answer to the settings dialog";
    }
    const bool  isNew  = step.kind == GuiStepKind::NEW_SIMULATION;
    IconButton* button = isNew ? panel->newButton() : panel->editButton();
    if (button == nullptr)
    {
        return "the tab has no such button";
    }
    const std::string_view name = isNew ? "New simulation" : "Edit simulation";
    // A button a user cannot click opens no dialog for the answer to wait for.
    if (std::string refused = unclickable(*button, name); !refused.empty())
    {
        return refused;
    }
    bool opened = false;
    {
        const QtRocket::Test::GuiSettingsAnswer& answer = *step.settings;
        const NextModalDialog                    dialog([this, &answer, &opened](QWidget& shown) {
            opened = true;
            answerSettings(shown, answer);
        });
        problem = clickWithAnswers(*button, name);
    }
    if (const std::string inside = answerProblems(); problem.empty())
    {
        problem = inside;
    }
    if (problem.empty() && !opened)
    {
        problem = "the button opened no settings dialog";
    }
    return problem;
}

std::string SimulationsScenario::deleteOperation(const GuiScenarioStep& step)
{
    std::string                  problem;
    const SimulationPanel* const panel = shownPanel(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    if (step.confirm.has_value())
    {
        const GuiConfirmAnswer answer = *step.confirm;
        m_simulationPrompts->queueAnswer(
            "the answer to the delete confirmation",
            [this, answer](QWidget& dialog) { answerConfirm(dialog, answer); });
    }
    IconButton* const button = panel->deleteButton();
    if (button == nullptr)
    {
        return "the tab has no Delete button";
    }
    problem = clickWithAnswers(*button, "Delete simulations");
    if (const std::string inside = answerProblems(); problem.empty())
    {
        problem = inside;
    }
    return problem;
}

std::string SimulationsScenario::menuOperation(const GuiScenarioStep& step)
{
    std::string                  problem;
    const SimulationPanel* const panel = shownPanel(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const MainMenus& menus = scenarioWindow().menus();
    QAction*         action{nullptr};
    std::string_view name;
    switch (step.kind)
    {
        case GuiStepKind::DUPLICATE_SIMULATIONS:
            action = menus.duplicateAction();
            name   = "Edit > Duplicate";
            break;
        case GuiStepKind::COPY_SIMULATIONS:
            action = menus.copyAction();
            name   = "Edit > Copy";
            break;
        case GuiStepKind::CUT_SIMULATIONS:
            action = menus.cutAction();
            name   = "Edit > Cut";
            break;
        default:
            action = menus.pasteAction();
            name   = "Edit > Paste";
            break;
    }
    if (step.confirm.has_value())
    {
        const GuiConfirmAnswer answer = *step.confirm;
        m_simulationPrompts->queueAnswer(
            "the answer to the delete confirmation",
            [this, answer](QWidget& dialog) { answerConfirm(dialog, answer); });
    }
    const std::size_t unanswered = m_simulationPrompts->unansweredSimulationQuestions();
    problem                      = ScenarioInput::triggerAction(action, name);
    settleAndDelete();
    if (problem.empty() && m_simulationPrompts->unansweredSimulationQuestions() != unanswered)
    {
        problem = std::format("{} asked a question the step has no answer for", name);
    }
    if (const std::string inside = answerProblems(); problem.empty())
    {
        problem = inside;
    }
    if (step.kind == GuiStepKind::COPY_SIMULATIONS || step.kind == GuiStepKind::CUT_SIMULATIONS)
    {
        m_clipboardText = utf8(QGuiApplication::clipboard()->text());
    }
    return problem;
}

std::string SimulationsScenario::columnVisibility(const GuiScenarioStep& step)
{
    std::string                  problem;
    const SimulationPanel* const panel = shownPanel(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const std::optional<Column> column = SimulationColumns::fromId(step.column);
    if (!column.has_value())
    {
        return std::format("the simulations table has no column {}", step.column);
    }
    const SimulationTableView& table  = panel->table();
    const QHeaderView&         header = *table.horizontalHeader();
    // A right click on the header: Qt's context menu event, whose menu the chooser shows.
    const int    visible = table.visibleColumns().empty()
                               ? -1
                               : SimulationColumns::indexOf(table.visibleColumns().front());
    const QPoint position(header.sectionViewportPosition(visible) + 2, header.height() / 2);
    const OffscreenPopupWarnings popup;
    QContextMenuEvent            event(QContextMenuEvent::Mouse, position,
                                       header.viewport()->mapToGlobal(position));
    QApplication::sendEvent(header.viewport(), &event);
    QMenu* menu = nullptr;
    for (QMenu* const candidate : table.findChildren<QMenu*>())
    {
        if (candidate->isVisible())
        {
            menu = candidate;
        }
    }
    if (menu == nullptr)
    {
        return "a right click on the table's header shows no menu";
    }
    auto* const item = menu->findChild<QAction*>(QtStrings::fromUtf8(step.column));
    if (item == nullptr)
    {
        menu->hide();
        return std::format("the column chooser has no item {}", step.column);
    }
    if (item->isChecked() == step.visible || !item->isEnabled())
    {
        menu->hide();
        return std::format("the column chooser's item {} is {} already, or disabled", step.column,
                           step.visible ? "shown" : "hidden");
    }
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(item).center());
    if (menu->isVisible())
    {
        menu->hide();
        return "the column chooser's menu stays open";
    }
    settle();
    return table.isColumnVisible(*column) == step.visible
               ? std::string()
               : std::format("the column {} is not {} after its item was clicked", step.column,
                             step.visible ? "shown" : "hidden");
}

// ================================================================================ the dialogs

void SimulationsScenario::answerSettings(QWidget& shown, const GuiSettingsAnswer& answer)
{
    auto* const dialog = qobject_cast<SimulationConfigDialog*>(&shown);
    if (dialog == nullptr)
    {
        fail("the button opened another dialog than the settings dialog: " + shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    GuiQuestion question;
    question.kind          = GuiQuestionKind::SIMULATION;
    question.settingsShown = settingsState(*dialog);
    question.title         = question.settingsShown.title;
    const std::size_t at   = m_questions.size();
    m_questions.push_back(question);
    std::string problem;
    for (std::size_t edit = 0; edit < answer.edits.size() && problem.empty(); ++edit)
    {
        problem = settingsEdit(*dialog, answer.edits[edit]);
        if (!problem.empty())
        {
            problem = std::format("edit {} of the settings dialog: {}", edit, problem);
        }
        // A user's next action comes after what this one queued.
        settle();
    }
    const QPointer<SimulationConfigDialog> alive(dialog);
    if (alive.isNull())
    {
        fail("the settings dialog went during its edits");
        return;
    }
    m_questions[at].settingsAnswered = settingsState(*dialog);
    fail(problem.empty() ? settingsButton(*dialog, answer) : problem);
    if (!alive.isNull() && !dialog->isClosed())
    {
        fail("the settings dialog stays open after its answer");
        closeSettingsDialog(*dialog);
    }
}

std::string SimulationsScenario::settingsButton(SimulationConfigDialog&  dialog,
                                                const GuiSettingsAnswer& answer)
{
    if (answer.button == "ok")
    {
        if (answer.run.has_value())
        {
            m_simulationPrompts->queueAnswer("the answer to the run dialog",
                                             [this](QWidget& run) { answerRun(run); });
        }
        return click(dialog.okButton(), "OK");
    }
    if (answer.button == "cancel")
    {
        if (answer.discard.has_value())
        {
            const GuiConfirmAnswer discard = *answer.discard;
            m_simulationPrompts->queueAnswer(
                "the answer to the discard question",
                [this, discard](QWidget& confirm) { answerConfirm(confirm, discard); });
        }
        return click(dialog.cancelButton(), "Cancel");
    }
    return "the settings dialog's buttons are ok and cancel";
}

void SimulationsScenario::closeSettingsDialog(SimulationConfigDialog& dialog)
{
    // Cancel, its discard question answered Yes, so that the operation goes on.
    const QPointer<SimulationConfigDialog> alive(&dialog);
    m_simulationPrompts->queueAnswer(
        "the discard question of a failed answer", [](QWidget& confirm) {
            if (const auto* const option = qobject_cast<const OptionDialog*>(&confirm);
                option != nullptr && !option->buttonBox()->buttons().empty())
            {
                option->buttonBox()->buttons().front()->click();
            }
        });
    dialog.cancel();
    if (!alive.isNull() && dialog.isVisible())
    {
        dialog.QDialog::reject();
    }
}

std::string SimulationsScenario::settingsEdit(SimulationConfigDialog& dialog,
                                              const GuiSettingsEdit&  edit)
{
    const std::string before     = simulationState(dialog);
    bool              mustChange = true;
    std::string       problem;
    switch (edit.kind)
    {
        case GuiSettingsEditKind::FIELD:
            problem = fieldEdit(dialog, edit);
            break;
        case GuiSettingsEditKind::NAME:
            problem = nameEdit(dialog, edit.name);
            break;
        case GuiSettingsEditKind::CONFIGURATION:
            problem = configurationEdit(dialog, edit.configuration);
            break;
        case GuiSettingsEditKind::TAB:
            problem = tabEdit(dialog, edit.tab);
            break;
        case GuiSettingsEditKind::WIND_MODEL:
            problem = windModelEdit(dialog, edit.windModel);
            break;
        case GuiSettingsEditKind::RANDOM_SEED:
            problem = randomSeedEdit(dialog, edit.seedFixed, edit.seedText);
            break;
        case GuiSettingsEditKind::DEFAULTS:
            problem    = defaultsEdit(dialog, edit.tab, edit.defaults);
            mustChange = edit.defaults == "reset";
            break;
        case GuiSettingsEditKind::WIND_LEVELS:
            problem    = windLevelsEdit(dialog, edit.windLevels);
            mustChange = false;
            break;
        case GuiSettingsEditKind::LOOKUP:
            problem    = lookupEdit(dialog, edit.lookup);
            mustChange = false;
            break;
    }
    if (problem.empty() && mustChange && before == simulationState(dialog))
    {
        problem = "the edit left the simulation and the tab as they were";
    }
    return problem;
}

std::string SimulationsScenario::windLevelsEdit(SimulationConfigDialog&    dialog,
                                                const GuiWindEditorAnswer& answer)
{
    const WindLevelSummary* const summary = dialog.conditionsPanel().windLevelSummary();
    if (summary == nullptr)
    {
        return "the launch conditions have no multi-level wind";
    }
    QPushButton& button = summary->editButton();
    if (std::string shown = bringIntoView(button); !shown.empty())
    {
        return shown;
    }
    if (std::string refused = unclickable(button, "Edit levels"); !refused.empty())
    {
        return refused;
    }
    bool        opened = false;
    std::string problem;
    {
        const NextModalDialog editor([this, &answer, &dialog, &opened](QWidget& shown) {
            opened = true;
            answerWindEditor(shown, answer, dialog);
        });
        problem = click(button, "Edit levels");
    }
    settle();
    if (problem.empty() && !opened)
    {
        problem = "Edit levels opened no wind editor";
    }
    return problem;
}

std::string SimulationsScenario::lookupEdit(SimulationConfigDialog& dialog,
                                            const GuiLookupAnswer&  answer)
{
    QPushButton& button = dialog.optionsPanel().configureLookupButton();
    if (std::string shown = bringIntoView(button); !shown.empty())
    {
        return shown;
    }
    if (std::string refused = unclickable(button, "Configure..."); !refused.empty())
    {
        return refused;
    }
    bool        opened = false;
    std::string problem;
    {
        const NextModalDialog lookup([this, &answer, &opened](QWidget& shown) {
            opened = true;
            answerLookup(shown, answer);
        });
        problem = click(button, "Configure...");
    }
    settle();
    if (problem.empty() && !opened)
    {
        problem = "Configure... opened no lookup dialog";
    }
    return problem;
}

// ------------------------------------------------------------------------------ the wind editor

void SimulationsScenario::answerWindEditor(QWidget& shown, const GuiWindEditorAnswer& answer,
                                           const SimulationConfigDialog& settings)
{
    auto* const editor = qobject_cast<MultiLevelWindEditDialog*>(&shown);
    if (editor == nullptr)
    {
        fail("Edit levels opened another dialog than the wind editor: " + shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    const MultiLevelPinkNoiseWindModel& model =
        settings.simulations().front()->getOptions().getMultiLevelWindModel();
    GuiQuestion question;
    question.kind        = GuiQuestionKind::WIND_LEVELS;
    question.windShown   = windState(*editor, model);
    question.title       = question.windShown.title;
    const std::size_t at = m_questions.size();
    m_questions.push_back(question);
    const QPointer<MultiLevelWindEditDialog> alive(editor);
    std::string                              problem;
    for (std::size_t edit = 0; edit < answer.edits.size() && problem.empty() && !alive.isNull();
         ++edit)
    {
        problem = windEdit(*editor, answer.edits[edit]);
        if (!problem.empty())
        {
            problem = std::format("edit {} of the wind editor: {}", edit, problem);
        }
        settle();
    }
    if (alive.isNull())
    {
        fail("the wind editor went during its edits");
        return;
    }
    m_questions[at].windAnswered = windState(*editor, model);
    if (problem.empty() && answer.button != "close")
    {
        problem = "the wind editor's button is close";
    }
    if (problem.empty())
    {
        problem = click(editor->closeButton(), "Close");
    }
    fail(problem);
    if (!alive.isNull() && editor->isVisible())
    {
        fail(leftOpen(answer.button, *editor));
        editor->reject();
    }
}

std::string SimulationsScenario::windEdit(MultiLevelWindEditDialog& editor, const GuiWindEdit& edit)
{
    switch (edit.kind)
    {
        case GuiWindEditKind::ALTITUDE_REFERENCE:
            return altitudeReferenceEdit(editor, edit.altitudeReference);
        case GuiWindEditKind::ADD_LEVEL:
            return click(editor.addButton(), "Add new level");
        case GuiWindEditKind::DELETE_LEVEL:
        case GuiWindEditKind::DELETE_ROW:
        case GuiWindEditKind::CELL:
            return windRowEdit(editor, edit);
        case GuiWindEditKind::COLUMN_UNIT:
            return windColumnUnitEdit(editor, edit.column, edit.unit);
        case GuiWindEditKind::RESET_LEVELS:
        {
            const GuiConfirmAnswer answer = edit.resetQuestion;
            m_simulationPrompts->queueAnswer(
                "the answer to the overwrite question",
                [this, answer](QWidget& dialog) { answerConfirm(dialog, answer); });
            return click(editor.resetButton(), "Reset levels");
        }
        case GuiWindEditKind::IMPORT_LEVELS:
            return importLevels(editor, edit.importLevels);
    }
    return "an edit of the wind editor the scenario does not know";
}

std::string SimulationsScenario::importLevels(MultiLevelWindEditDialog&  editor,
                                              const GuiWindImportAnswer& answer)
{
    const GuiCsvImportAnswer settings = answer.settings;
    m_simulationPrompts->queueAnswer(
        "the answer to the CSV import settings",
        [this, settings](QWidget& dialog) { answerCsvSettings(dialog, settings); });
    if (answer.file.has_value())
    {
        queueFileAnswer(*answer.file);
    }
    if (answer.overwrite.has_value())
    {
        const GuiConfirmAnswer overwrite = *answer.overwrite;
        m_simulationPrompts->queueAnswer(
            "the answer to the overwrite question",
            [this, overwrite](QWidget& dialog) { answerConfirm(dialog, overwrite); });
    }
    return click(editor.importButton(), "Import levels");
}

// ------------------------------------------------------------------- the CSV import settings

void SimulationsScenario::answerCsvSettings(QWidget& shown, const GuiCsvImportAnswer& answer)
{
    auto* const dialog = qobject_cast<CsvImportSettingsDialog*>(&shown);
    if (dialog == nullptr)
    {
        fail("Import levels opened another dialog than the CSV import settings: " +
             shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    GuiQuestion question;
    question.kind        = GuiQuestionKind::CSV_IMPORT_SETTINGS;
    question.csvShown    = csvState(*dialog);
    question.title       = question.csvShown.title;
    const std::size_t at = m_questions.size();
    m_questions.push_back(question);
    std::string problem;
    if (answer.hasHeader.has_value())
    {
        problem = dialog->hasHeaderCheckBox().isChecked() == *answer.hasHeader
                      ? std::string("the header box is in that state already")
                      : click(dialog->hasHeaderCheckBox(), "CSV file has a header row");
        settle();
        if (problem.empty() && dialog->hasHeaderCheckBox().isChecked() != *answer.hasHeader)
        {
            problem = "a click on the header box leaves it as it was";
        }
    }
    for (const QtRocket::Test::GuiCsvColumnAnswer& column : answer.columns)
    {
        if (problem.empty())
        {
            problem = csvColumnEdit(*dialog, column);
        }
    }
    if (problem.empty() && answer.separator.has_value())
    {
        problem = chooseSeparator(dialog->separatorComboBox(), *answer.separator);
    }
    m_questions[at].csvAnswered = csvState(*dialog);
    fail(problem.empty() ? okOrCancel(dialog->okButton(), dialog->cancelButton(), answer.button,
                                      "the CSV import settings")
                         : problem);
    if (dialog->isVisible())
    {
        fail(leftOpen(answer.button, *dialog));
        dialog->reject();
    }
}

// ----------------------------------------------------------------------- the lookup dialog

void SimulationsScenario::answerLookup(QWidget& shown, const GuiLookupAnswer& answer)
{
    auto* const dialog = qobject_cast<AerodynamicLookupDialog*>(&shown);
    if (dialog == nullptr)
    {
        fail("Configure... opened another dialog than the lookup dialog: " + shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    GuiQuestion question;
    question.kind        = GuiQuestionKind::LOOKUP;
    question.lookupShown = lookupState(*dialog);
    question.title       = question.lookupShown.title;
    const std::size_t at = m_questions.size();
    m_questions.push_back(question);
    const QPointer<AerodynamicLookupDialog> alive(dialog);
    std::string                             problem = lookupSection(*dialog, 0, answer.drag);
    if (problem.empty() && !alive.isNull())
    {
        problem = lookupSection(*dialog, 1, answer.stability);
    }
    if (alive.isNull())
    {
        fail("the lookup dialog went during its actions");
        return;
    }
    m_questions[at].lookupAnswered = lookupState(*dialog);
    fail(problem.empty() ? okOrCancel(dialog->okButton(), dialog->cancelButton(), answer.button,
                                      "the lookup dialog")
                         : problem);
    if (!alive.isNull() && dialog->isVisible())
    {
        fail(leftOpen(answer.button, *dialog));
        dialog->reject();
    }
}

std::string SimulationsScenario::lookupSection(
    AerodynamicLookupDialog& dialog, int section,
    const std::optional<std::vector<GuiLookupAction>>& actions)
{
    if (!actions.has_value())
    {
        return {};
    }
    const QPointer<AerodynamicLookupDialog> alive(&dialog);
    std::string                             problem = showTab(dialog.tabs(), section);
    for (const GuiLookupAction& action : *actions)
    {
        if (!problem.empty() || alive.isNull())
        {
            break;
        }
        const GuiLookupState before = lookupState(dialog);
        problem                     = lookupAction(dialog, section, action);
        settle();
        if (problem.empty() && !alive.isNull() && lookupActionMustChange(action) &&
            before == lookupState(dialog))
        {
            problem = std::format("the {} action left the lookup dialog as it was",
                                  kLookupSections.at(static_cast<std::size_t>(section)));
        }
    }
    return problem;
}

std::string SimulationsScenario::lookupAction(AerodynamicLookupDialog& dialog, int section,
                                              const GuiLookupAction& action)
{
    const LookupTable table = section == 0 ? LookupTable::DRAG : LookupTable::STABILITY;
    switch (action.kind)
    {
        case GuiLookupActionKind::SEPARATOR:
            return chooseSeparator(dialog.separatorComboBox(table), action.text);
        case GuiLookupActionKind::LOAD:
            queueFileAnswer(action.file);
            return click(dialog.loadButton(table), "Load from file...");
        case GuiLookupActionKind::TEXT:
        {
            QPlainTextEdit& area = dialog.textArea(table);
            if (area.isReadOnly())
            {
                return "the text is not editable (no table is loaded)";
            }
            if (std::string problem = focusOn(area, "the lookup text"); !problem.empty())
            {
                return problem;
            }
            // The text pasted over the area's: one edit, as OpenRocket's tool sets it.
            area.selectAll();
            area.insertPlainText(QtStrings::fromUtf8(action.text));
            return area.toPlainText() == QtStrings::fromUtf8(action.text)
                       ? std::string()
                       : "the text area does not show the text pasted";
        }
        case GuiLookupActionKind::REFRESH:
            return click(dialog.refreshButton(table), "Refresh");
        case GuiLookupActionKind::CLEAR:
            return click(dialog.clearButton(table), "Clear");
    }
    return "a lookup action the scenario does not know";
}

// -------------------------------------------------------------- the questions and the run

void SimulationsScenario::answerConfirm(QWidget& shown, const GuiConfirmAnswer& answer)
{
    auto* const dialog = qobject_cast<OptionDialog*>(&shown);
    if (dialog == nullptr)
    {
        fail("a confirmation is no OptionDialog: " + shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    m_questions.push_back(confirmQuestion(*dialog));
    std::string problem;
    if (answer.doNotAskAgain)
    {
        const QList<QCheckBox*> boxes = dialog->messageArea()->findChildren<QCheckBox*>();
        if (boxes.size() != 1)
        {
            problem = "doNotAskAgain needs the confirmation's one check box";
        }
        else if (!boxes.front()->isChecked())
        {
            problem = click(*boxes.front(), "Do not ask me again");
        }
    }
    QAbstractButton*       chosen = nullptr;
    const std::string_view text   = confirmButtonText(answer.button);
    for (QAbstractButton* const button : dialog->buttonBox()->buttons())
    {
        if (!text.empty() && shownText(button->text()) == text)
        {
            chosen = button;
        }
    }
    if (problem.empty() && chosen == nullptr)
    {
        problem = "the confirmation has no button " + answer.button;
    }
    if (problem.empty())
    {
        problem = click(*chosen, answer.button);
    }
    fail(problem);
    if (dialog->isVisible())
    {
        fail(leftOpen(answer.button, *dialog));
        dialog->reject();
    }
}

void SimulationsScenario::answerRun(QWidget& shown)
{
    auto* const dialog = qobject_cast<SimulationRunDialog*>(&shown);
    if (dialog == nullptr)
    {
        fail("OK opened another dialog than the run dialog: " + shortClassName(shown));
        rejectDialog(shown);
        return;
    }
    GuiQuestion question;
    question.kind                = GuiQuestionKind::RUN;
    question.title               = utf8(dialog->windowTitle());
    const SimulationBatch& batch = dialog->batch();
    for (std::size_t index = 0; index < batch.size(); ++index)
    {
        question.simulations.push_back(utf8(batch.simulationName(index)));
    }
    m_questions.push_back(std::move(question));
    // Before its Cancel no simulation of the dialog ran (the executor runs none).
    if (m_executor->runningJob() != SimulationExecutor::kNoJob ||
        m_executor->waitingJobs().size() != batch.size())
    {
        fail("a simulation of the run dialog started before its Cancel");
    }
    const QList<QPushButton*> buttons = dialog->findChildren<QPushButton*>();
    if (buttons.size() != 1)
    {
        fail("the run dialog has no Cancel button of its own");
        dialog->reject();
        return;
    }
    fail(click(*buttons.front(), "Cancel"));
    // Cancel closes the dialog once the executor has reported its cancelled jobs, in a later turn
    // of the dialog's event loop. A Cancel that does not would leave the step waiting for ever (the
    // executor runs nothing): the safety net, which only a hang reaches, closes the dialog and
    // fails the step. It goes with the dialog.
    QTimer::singleShot(kRunDialogSafetyNet, dialog, [this, dialog] {
        if (dialog->isVisible())
        {
            fail(leftOpen("cancel", *dialog));
            dialog->QDialog::reject();
        }
    });
}

// -------------------------------------------------------------------------- the file choosers

void SimulationsScenario::queueFileAnswer(const GuiFileAnswer& answer)
{
    m_fileAnswers.push_back(answer);
}

void SimulationsScenario::fileChooserAsked(const QString& title)
{
    if (!m_simulationStep)
    {
        // File > Open of the open step and of the reopening: the script's question alone.
        return;
    }
    GuiQuestion question;
    question.kind  = GuiQuestionKind::OPEN_FILE;
    question.title = utf8(title);
    if (m_fileAnswers.empty())
    {
        // The script answers it: no answer is scripted, a test failure.
        ++m_unansweredFiles;
        m_questions.push_back(std::move(question));
        return;
    }
    const GuiFileAnswer answer = m_fileAnswers.front();
    m_fileAnswers.pop_front();
    if (answer.button == "open")
    {
        const Result<std::filesystem::path> file = QtRocket::Test::guiRepositoryFile(answer.file);
        if (!file)
        {
            fail(file.error().message);
            prompts().answerOpenFiles(std::nullopt);
        }
        else
        {
            question.file = answer.file;
            prompts().answerOpenFiles(
                UserPrompts::OpenFileAnswer{.files = {*file}, .directory = file->parent_path()});
        }
    }
    else
    {
        prompts().answerOpenFiles(std::nullopt);
    }
    m_questions.push_back(std::move(question));
}

}  // namespace QtRocket::Gui::Test
