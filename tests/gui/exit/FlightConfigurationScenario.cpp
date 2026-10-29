#include "exit/FlightConfigurationScenario.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/DeploymentSelectionDialog.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/dialogs/IgnitionSelectionDialog.h"
#include "QtRocket/gui/dialogs/RenameConfigDialog.h"
#include "QtRocket/gui/dialogs/SeparationSelectionDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/motor/MotorChooserTestSupport.h"
#include "exit/FlightConfigurationPrompts.h"
#include "exit/FromScratchPrompts.h"
#include "exit/FromScratchScenario.h"
#include "exit/ScenarioInput.h"
#include "goldens/GoldenGuiReference.h"

namespace QtRocket::Gui::Test
{

namespace
{

using QtRocket::Test::GuiAnswer;
using QtRocket::Test::GuiCell;
using QtRocket::Test::GuiChooserState;
using QtRocket::Test::GuiComponentPath;
using QtRocket::Test::GuiConfigurationTable;
using QtRocket::Test::GuiFlightConfigurationState;
using QtRocket::Test::GuiMotor;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::GuiTableRow;
using QtRocket::Test::GuiTypedValue;
using ScenarioInput::activateWindowOf;
using ScenarioInput::focusOn;
using ScenarioInput::settle;
using ScenarioInput::settleAndDelete;
using ScenarioInput::shortClassName;
using DeployEvent     = DeploymentConfiguration::DeployEvent;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

/// The names of the tab's sub-tabs in the records, by index (FlightConfigurationPanel's
/// kMotorTabIndex, kRecoveryTabIndex, kSeparationTabIndex).
constexpr std::array<std::string_view, 3> kConfigTabNames{"motors", "recovery", "stages"};

/// The names of the window's tabs in the records ("design", "flightConfiguration").
constexpr std::string_view kDesignTabName              = "design";
constexpr std::string_view kFlightConfigurationTabName = "flightConfiguration";

// ================================================================================= the records

/// @p value as Java 17's Double.toString writes it ("Infinity" for Plugged), as the records give
/// every number of a question.
[[nodiscard]] std::string javaText(double value)
{
    return FloatingDecimal::toJavaFormatString(value);
}

/// The row of @p id among @p ids, -1 for none (the default configuration, one the rocket does not
/// have).
[[nodiscard]] int rowOf(std::span<const FlightConfigurationId> ids, const FlightConfigurationId& id)
{
    const auto found = std::ranges::find(ids, id);
    return found == ids.end() ? -1 : static_cast<int>(found - ids.begin());
}

/// The enabled state of @p buttons by their names.
[[nodiscard]] std::map<std::string, bool, std::less<>> buttonStates(
    std::initializer_list<std::pair<std::string_view, const QPushButton*>> buttons)
{
    std::map<std::string, bool, std::less<>> states;
    for (const auto& [name, button] : buttons)
    {
        states.emplace(std::string(name), button != nullptr && button->isEnabled());
    }
    return states;
}

/// Whether the configuration of @p row of @p model has an override of its own of the deployment
/// or the separation of the component of @p column (the state behind a Recovery or Stages cell,
/// read from the rocket as the tool reads OpenRocket's: containsId()).
[[nodiscard]] bool hasOverride(const FlightConfigurableTableModel& model, int row, int column)
{
    const std::optional<FlightConfigurationId> id        = model.configurationId(row);
    const RocketComponent* const               component = model.component(column);
    if (!id.has_value() || component == nullptr)
    {
        return false;
    }
    if (const auto* const device = dynamic_cast<const RecoveryDevice*>(component))
    {
        return device->getDeploymentConfigurations().containsId(*id);
    }
    if (const auto* const stage = dynamic_cast<const AxialStage*>(component))
    {
        return stage->getSeparationConfigurations().containsId(*id);
    }
    return false;
}

/// The row @p row of @p model as the records have it: column 0's text and the other cells, as
/// motor cells (their motor's text, their ignition's and whether the ignition is shaded:
/// MotorCellDelegate draws it italic and dim for FlightConfigurableTableModel::kDefaultRole) or
/// as value cells (their text, whether they are shaded, the italic and dim text of
/// kDefaultRole, and whether their configuration has an override of its own).
[[nodiscard]] GuiTableRow tableRow(const FlightConfigurableTableModel& model, int row)
{
    GuiTableRow tableRow;
    tableRow.configuration = QtStrings::toUtf8(model.data(model.index(row, 0)).toString());
    for (int column = 1; column < model.columnCount(); ++column)
    {
        const QModelIndex cell = model.index(row, column);
        const std::string text = QtStrings::toUtf8(model.data(cell).toString());
        const bool shaded = model.data(cell, FlightConfigurableTableModel::kDefaultRole).toBool();
        if (model.kind() == FlightConfigurableTableModel::Kind::MOTOR_MOUNTS)
        {
            tableRow.motorCells.push_back(
                {.motor    = text,
                 .ignition = QtStrings::toUtf8(
                     model.data(cell, FlightConfigurableTableModel::kIgnitionTextRole).toString()),
                 .ignitionDefault = shaded});
        }
        else
        {
            tableRow.cells.push_back(
                {.text = text, .shaded = shaded, .override = hasOverride(model, row, column)});
        }
    }
    return tableRow;
}

/// The table of @p panel as the records have it, with the enabled states of @p buttons.
[[nodiscard]] GuiConfigurationTable tableOf(const FlightConfigurablePanel&           panel,
                                            std::map<std::string, bool, std::less<>> buttons)
{
    GuiConfigurationTable               table;
    const FlightConfigurableTableModel& model = panel.tableModel();
    table.buttons                             = std::move(buttons);
    for (int column = 0; column < model.columnCount(); ++column)
    {
        table.header.push_back(
            QtStrings::toUtf8(model.headerData(column, Qt::Horizontal).toString()));
    }
    for (int row = 0; row < model.rowCount(); ++row)
    {
        table.rows.push_back(tableRow(model, row));
    }
    const RowColumnSelectionModel::Lines& lines = panel.tableSelection().lines();
    table.selectedRows                          = lines.rows;
    table.selectedColumns                       = lines.columns;
    return table;
}

/// The motor mount table of @p panel as the records have it: each row's component, its text and
/// its check box.
[[nodiscard]] std::vector<QtRocket::Test::GuiMountRow> mountRows(
    const MotorConfigurationPanel& panel)
{
    std::vector<QtRocket::Test::GuiMountRow> rows;
    const MotorMountTableModel&              model  = panel.motorMountConfigurationPanel().model();
    const Rocket&                            rocket = panel.rocket();
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const std::optional<Uuid>    id        = model.mountId(row);
        const RocketComponent* const component = id ? rocket.findComponent(*id) : nullptr;
        rows.push_back(
            {.component = component != nullptr ? QtRocket::Test::guiComponentPathOf(*component)
                                               : GuiComponentPath{},
             .text      = QtStrings::toUtf8(
                 model.data(model.index(row, MotorMountTableModel::kNameColumn)).toString()),
             .motorMount = model
                               .data(model.index(row, MotorMountTableModel::kMotorMountColumn),
                                     Qt::CheckStateRole)
                               .toInt() == Qt::Checked});
    }
    return rows;
}

/// The chooser's state as the records have it (the tool's chooserState()): the search field's
/// text, the table's rows, the selected thrust curve, the delay box's text (none when empty), the
/// delay and the nozzle exit diameter.
[[nodiscard]] GuiChooserState chooserState(const MotorChooserDialog& chooser)
{
    const ThrustCurveMotorSelectionPanel& panel = chooser.selectionPanel();
    const auto* const search = panel.findChild<QLineEdit*>(QStringLiteral("searchField"));
    const auto* const delays = panel.findChild<QComboBox*>(QStringLiteral("delayBox"));
    GuiChooserState   state;
    state.search = search != nullptr ? QtStrings::toUtf8(search->text()) : std::string("?");
    state.rows   = panel.tableModel().rowCount();
    if (const std::shared_ptr<const ThrustCurveMotor> motor = panel.getSelectedMotor())
    {
        state.motor = GuiMotor{.manufacturer = motor->getManufacturer().getDisplayName(),
                               .designation  = motor->getDesignation(),
                               .digest       = motor->getDigest()};
    }
    if (delays != nullptr && !delays->currentText().isEmpty())
    {
        state.delayText = QtStrings::toUtf8(delays->currentText());
    }
    state.delay              = javaText(panel.getSelectedDelay());
    state.nozzleExitDiameter = javaText(panel.getSelectedNozzleExitDiameter());
    return state;
}

/// The texts of a value dialog's question as it opened (the tool's valueQuestion()): the title,
/// the scope it starts on, the radio buttons' texts.
[[nodiscard]] GuiQuestion valueQuestion(const FlightConfigurationValueDialog& dialog,
                                        GuiQuestionKind                       kind)
{
    GuiQuestion question;
    question.kind         = kind;
    question.title        = QtStrings::toUtf8(dialog.windowTitle());
    question.scope        = dialog.defaultButton().isChecked() ? "all" : "this";
    question.defaultText  = QtStrings::toUtf8(dialog.defaultButton().text());
    question.overrideText = QtStrings::toUtf8(dialog.overrideButton().text());
    return question;
}

/// The event box @p box's current event by its Java name (@p name gives it), and its items in
/// @p events when it is given.
template <class E>
[[nodiscard]] std::string eventOf(const EnumComboBox&       box, std::string_view (*name)(E),
                                  std::vector<std::string>* events = nullptr)
{
    const std::shared_ptr<EnumBinding<E>> binding = box.enumBinding<E>();
    if (binding == nullptr)
    {
        return "?";
    }
    if (events != nullptr)
    {
        for (int index = 0; index < box.count(); ++index)
        {
            const std::optional<E> value = binding->elementAt(index);
            events->emplace_back(value.has_value() ? name(*value) : "?");
        }
    }
    return std::string(name(binding->value()));
}

/// The question of a deployment or separation dialog @p dialog as it opened, its event's names
/// given by @p name.
template <class Dialog, class E>
[[nodiscard]] GuiQuestion altitudeQuestion(const Dialog& dialog, GuiQuestionKind kind,
                                           std::string_view (*name)(E))
{
    GuiQuestion question     = valueQuestion(dialog, kind);
    question.event           = eventOf<E>(dialog.eventBox(), name, &question.events);
    question.altitude        = javaText(dialog.altitude().spinBox->binding()->value());
    question.altitudeEnabled = dialog.isAltitudeEnabled();
    question.delay           = javaText(dialog.delaySpinBox().binding()->value());
    return question;
}

/// The message @p message as the records have it (its title and its text).
[[nodiscard]] GuiQuestion messageQuestion(const UserPrompts::Message& message)
{
    GuiQuestion question;
    question.kind  = GuiQuestionKind::MESSAGE;
    question.title = QtStrings::toUtf8(message.title);
    question.text  = QtStrings::toUtf8(message.lines.join(u'\n'));
    return question;
}

// ================================================================================= the answers

/// The radio button of @p scope ("all", "this") of @p dialog clicked when it is not chosen; what
/// went wrong, or "".
[[nodiscard]] std::string chooseScope(const FlightConfigurationValueDialog& dialog,
                                      const std::optional<std::string>&     scope)
{
    if (!scope.has_value() || (*scope != "all" && *scope != "this"))
    {
        return "the answer's scope is all or this";
    }
    QRadioButton& radio = *scope == "all" ? dialog.defaultButton() : dialog.overrideButton();
    if (!radio.isChecked())
    {
        QTest::mouseClick(&radio, Qt::LeftButton);
        settle();
    }
    return radio.isChecked() ? std::string()
                             : std::format("the radio button of {} is not chosen", *scope);
}

/// The event @p event (a Java name, which @p name gives) chosen in @p box when it is not the
/// current item; what went wrong, or "".
template <class E>
[[nodiscard]] std::string chooseEvent(EnumComboBox& box, const std::optional<std::string>& event,
                                      std::string_view (*name)(E))
{
    const std::shared_ptr<EnumBinding<E>> binding = box.enumBinding<E>();
    if (!event.has_value() || binding == nullptr)
    {
        return "the answer has no event, or the dialog no event box";
    }
    for (int index = 0; index < box.count(); ++index)
    {
        const std::optional<E> value = binding->elementAt(index);
        if (value.has_value() && name(*value) == *event)
        {
            if (box.currentIndex() != index)
            {
                chooseItem(box, index);
                settle();
            }
            return name(binding->value()) == *event
                       ? std::string()
                       : std::format("the event box shows another event than {}", *event);
        }
    }
    return std::format("the event box offers no {}", *event);
}

/// @p value typed into the number field @p spinBox named @p field, its unit chosen first in
/// @p selector, or for a field without one the unit the field shows; not typed when the field
/// shows that value already (the tool's type()). What went wrong, or "".
[[nodiscard]] std::string typeValue(UnitSpinBox& spinBox, UnitSelector* selector,
                                    const std::optional<GuiTypedValue>& value,
                                    std::string_view                    field)
{
    if (!value.has_value())
    {
        return std::format("the answer has no {}", field);
    }
    if (!spinBox.isEnabled())
    {
        return std::format("the field {} is disabled", field);
    }
    const std::shared_ptr<DoubleBinding>& binding = spinBox.binding();
    if (selector != nullptr)
    {
        if (std::string problem = ScenarioInput::chooseUnit(*selector, field, value->unit);
            !problem.empty())
        {
            return problem;
        }
    }
    else if (binding->getCurrentUnit().getUnit() != value->unit)
    {
        return std::format("the field {} shows {}, not {} (it has no unit selector)", field,
                           binding->getCurrentUnit().getUnit(), value->unit);
    }
    const std::optional<double> typed = Strings::javaParseDouble(value->text);
    if (!typed.has_value())
    {
        return std::format("the field {} does not take '{}'", field, value->text);
    }
    if (binding->getCurrentUnit().fromUnit(*typed) == binding->value())
    {
        return {};
    }
    return ScenarioInput::typeNumber(spinBox, field, value->text);
}

/// The keys a click on @p cell holds: Ctrl ("ctrl"), Shift ("shift") or none.
[[nodiscard]] Qt::KeyboardModifiers modifiersOf(const GuiCell& cell)
{
    if (cell.with == "ctrl")
    {
        return Qt::ControlModifier;
    }
    return cell.with == "shift" ? Qt::ShiftModifier : Qt::NoModifier;
}

/// A click on @p button of a dialog; what went wrong (it is disabled), or "".
[[nodiscard]] std::string clickButton(QPushButton& button, std::string_view name)
{
    if (!button.isEnabled())
    {
        return std::format("the button {} is disabled", name);
    }
    QTest::mouseClick(&button, Qt::LeftButton);
    return {};
}

/// The OK or Cancel button of a value dialog @p dialog that @p answer names (Cancel the answer
/// with the button alone); null with what went wrong in @p problem for another button.
[[nodiscard]] QPushButton* valueDialogButton(const FlightConfigurationValueDialog& dialog,
                                             const GuiAnswer& answer, std::string& problem)
{
    if (answer.button == "cancel")
    {
        return &dialog.cancelButton();
    }
    if (answer.button != "ok")
    {
        problem = "the answer's button is ok or cancel";
        return nullptr;
    }
    return &dialog.okButton();
}

/// Types @p text over the text field @p line (no Enter: the dialog's default button would take
/// it), as a user replacing its text; what went wrong, or "".
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
                             QtStrings::toUtf8(line.text()), text);
}

/// The one child of @p root of class W named @p name; null when there is none or more.
template <class W>
[[nodiscard]] W* namedChild(const QWidget& root, const QString& name)
{
    const QList<W*> found = root.findChildren<W*>(name);
    return found.size() == 1 ? found.front() : nullptr;
}

/// A click on the row of the chooser's table that shows the motor set @p motor (the Manufacturer
/// column's text and the set's designation), the row scrolled into view first; what went wrong,
/// or "".
[[nodiscard]] std::string clickMotorRow(const ThrustCurveMotorSelectionPanel& panel,
                                        const GuiMotor&                       motor)
{
    auto* const table = namedChild<QTableView>(panel, QStringLiteral("table"));
    if (table == nullptr)
    {
        return "the chooser has no table";
    }
    const MotorTableModel& model = panel.tableModel();
    int                    found = -1;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const ThrustCurveMotorSet& set = model.motorSet(row);
        if (set.getManufacturer() != nullptr &&
            set.getManufacturer()->getDisplayName() == motor.manufacturer &&
            set.getDesignation() == motor.designation)
        {
            if (found >= 0)
            {
                return std::format("two rows of the chooser show {} {}", motor.manufacturer,
                                   motor.designation);
            }
            found = row;
        }
    }
    if (found < 0)
    {
        return std::format("the chooser shows no row of {} {}", motor.manufacturer,
                           motor.designation);
    }
    const QModelIndex cell = model.index(found, 0);
    table->scrollTo(cell);
    const QRect rect = table->visualRect(cell);
    if (!table->viewport()->rect().contains(rect.center()))
    {
        return std::format("the row of {} {} is not in view", motor.manufacturer,
                           motor.designation);
    }
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    settle();
    return {};
}

/// The curve of @p digest chosen in the chooser's curve box when it is not the current one; what
/// went wrong, or "".
[[nodiscard]] std::string chooseCurve(ThrustCurveMotorSelectionPanel& panel,
                                      const std::string&              digest)
{
    auto* const curves = namedChild<QComboBox>(panel, QStringLiteral("curveSelectionBox"));
    if (curves == nullptr || !curves->isEnabled())
    {
        return "the curve box is disabled";
    }
    const std::vector<MotorHolder> items = panel.selection().getCurveItems();
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (items[index].getMotor()->getDigest() == digest)
        {
            if (curves->currentIndex() != static_cast<int>(index))
            {
                chooseComboItem(*curves, static_cast<int>(index));
                settle();
            }
            return {};
        }
    }
    return std::format("the curve box has no curve {}", digest);
}

/// @p text chosen in the chooser's delay box when it is one of its items, else typed into its
/// editor and committed with Enter (the tool's setSelectedItem(), as the editor commits); what
/// went wrong, or "".
[[nodiscard]] std::string_view chooseDelay(const ThrustCurveMotorSelectionPanel& panel,
                                           std::string_view                      text)
{
    auto* const delays = namedChild<QComboBox>(panel, QStringLiteral("delayBox"));
    if (delays == nullptr || !delays->isEnabled())
    {
        return "the delay box is disabled (no motor is selected)";
    }
    if (!activateWindowOf(*delays))
    {
        return "the chooser does not become active";
    }
    const int item = delays->findText(QtStrings::fromUtf8(text), Qt::MatchExactly);
    if (item >= 0)
    {
        chooseComboItem(*delays, item);
    }
    else
    {
        typeIntoComboBox(*delays, QtStrings::fromUtf8(text));
        QTest::keyClick(delays, Qt::Key_Return);
    }
    settle();
    return {};
}

}  // namespace

// ================================================================================ the scenario

FlightConfigurationScenario::FlightConfigurationScenario(std::filesystem::path directory)
  : FromScratchScenario(std::move(directory),
                        [](UserPrompts& scripted) {
                            return std::make_unique<FlightConfigurationPrompts>(scripted);
                        }),
    m_tabPrompts(dynamic_cast<FlightConfigurationPrompts*>(&scenarioPrompts()))
{
    QTROCKET_ASSERT(m_tabPrompts != nullptr);
    m_tabPrompts->setMessageObserver([this](const UserPrompts::Message& message) {
        m_questions.push_back(messageQuestion(message));
    });
}

FlightConfigurationScenario::~FlightConfigurationScenario()
{
    // The application's questions go with the application, after this object.
    m_tabPrompts->setMessageObserver({});
    m_tabPrompts->setDialogAnswer({});
}

FlightConfigurationPanel* FlightConfigurationScenario::tab() const
{
    const MainWindow* const shown = window();
    return shown == nullptr ? nullptr : shown->componentFlightConfigurationPanel();
}

GuiStepRecord FlightConfigurationScenario::record(const GuiScenarioStep& step) const
{
    GuiStepRecord           record = FromScratchScenario::record(step);
    const MainWindow* const shown  = window();
    if (shown == nullptr)
    {
        return record;
    }
    record.tab       = std::string(shown->selectedTab() == MainWindow::kFlightConfigurationTab
                                       ? kFlightConfigurationTabName
                                       : kDesignTabName);
    record.questions = m_questions;
    if (const FlightConfigurationPanel* const panel = tab(); panel != nullptr)
    {
        record.flightConfiguration = tabState(*panel);
    }
    return record;
}

GuiFlightConfigurationState FlightConfigurationScenario::tabState(
    const FlightConfigurationPanel& panel)
{
    GuiFlightConfigurationState state;
    const QTabWidget&           tabs = *panel.tabs();
    for (int index = 0; index < tabs.count(); ++index)
    {
        const std::string name =
            std::cmp_less(index, kConfigTabNames.size())
                ? std::string(kConfigTabNames.at(static_cast<std::size_t>(index)))
                : std::format("tab {}", index);
        state.configTabs.push_back({.tab   = name,
                                    .title = QtStrings::toUtf8(shownMenuText(tabs.tabText(index))),
                                    .enabled = tabs.isTabEnabled(index)});
        if (index == tabs.currentIndex())
        {
            state.configTab = name;
        }
    }
    state.buttons = buttonStates({{"newConfButton", panel.newConfigurationButton()},
                                  {"renameConfButton", panel.renameConfigurationButton()},
                                  {"removeConfButton", panel.deleteConfigurationButton()},
                                  {"duplicateConfButton", panel.duplicateConfigurationButton()}});
    const Rocket&                            rocket = panel.rocket();
    const std::vector<FlightConfigurationId> ids    = rocket.getIds();
    state.selectedConfiguration = rowOf(ids, rocket.getSelectedConfiguration().getId());
    for (const std::shared_ptr<Simulation>& simulation : panel.document().getSimulations())
    {
        state.simulations.push_back(
            {.name          = simulation->getName(),
             .configuration = rowOf(ids, simulation->getFlightConfigurationId())});
    }

    const MotorConfigurationPanel& motors = panel.motorConfigurationPanel();
    state.motors =
        tableOf(motors, buttonStates({{"selectMotorButton", motors.selectMotorButton()},
                                      {"deleteMotorButton", motors.deleteMotorButton()},
                                      {"selectIgnitionButton", motors.selectIgnitionButton()},
                                      {"resetIgnitionButton", motors.resetIgnitionButton()}}));
    state.motors.showsHelp = motors.isShowingHelp();
    state.motors.mounts    = mountRows(motors);

    const RecoveryConfigurationPanel& recovery = panel.recoveryConfigurationPanel();
    state.recovery                             = tableOf(
        recovery, buttonStates({{"selectDeploymentButton", recovery.selectDeploymentButton()},
                                {"resetDeploymentButton", recovery.resetDeploymentButton()}}));

    const SeparationConfigurationPanel& stages = panel.separationConfigurationPanel();
    state.stages =
        tableOf(stages, buttonStates({{"selectSeparationButton", stages.selectSeparationButton()},
                                      {"resetDeploymentButton", stages.resetSeparationButton()}}));
    return state;
}

std::string FlightConfigurationScenario::perform(const GuiScenarioStep& step)
{
    m_questions.clear();
    switch (step.kind)
    {
        case GuiStepKind::OPEN:
            return open(step);
        case GuiStepKind::TAB:
            return windowTab(step);
        case GuiStepKind::CONFIG_TAB:
            return configTab(step);
        case GuiStepKind::SELECT_CELLS:
            return selectCells(step);
        case GuiStepKind::MOUNT_CHECK:
            return mountCheck(step);
        case GuiStepKind::NEW_CONFIGURATION:
        case GuiStepKind::RENAME_CONFIGURATION:
        case GuiStepKind::DELETE_CONFIGURATION:
        case GuiStepKind::DUPLICATE_CONFIGURATION:
        case GuiStepKind::SELECT_MOTOR:
        case GuiStepKind::DELETE_MOTOR:
        case GuiStepKind::SELECT_IGNITION:
        case GuiStepKind::RESET_IGNITION:
        case GuiStepKind::SELECT_DEPLOYMENT:
        case GuiStepKind::RESET_DEPLOYMENT:
        case GuiStepKind::SELECT_SEPARATION:
        case GuiStepKind::RESET_SEPARATION:
            return operation(step);
        default:
            // The steps of version 1.
            return FromScratchScenario::perform(step);
    }
}

std::string FlightConfigurationScenario::open(const GuiScenarioStep& step)
{
    if (window() != nullptr)
    {
        return "the scenario's window is open already";
    }
    const WindowManager& windows = session().windows();
    if (windows.windowCount() != 1)
    {
        return std::format("{} windows are open at the start, not one", windows.windowCount());
    }
    // Through a plain pointer that is checked (GCC's -Wnull-dereference sees no check through
    // QPointer's operator->), used while the window is open.
    MainWindow* const first = windows.windows().front();
    QTROCKET_ASSERT(first != nullptr);
    const QPointer<MainWindow>          start = first;
    const Result<std::filesystem::path> file  = QtRocket::Test::guiRepositoryFile(step.file);
    if (!file)
    {
        return file.error().message;
    }
    prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {*file}, .directory = file->parent_path()});
    if (std::string problem =
            ScenarioInput::triggerAction(first->menus().openAction(), "File > Open");
        !problem.empty())
    {
        return problem;
    }
    settleAndDelete();
    // The New design of the start was replaced (BasicFrame.closeIfReplaceable()).
    if (windows.windowCount() != 1 || !start.isNull())
    {
        return std::format("{} windows after File > Open, the start's closed: {}",
                           windows.windowCount(), start.isNull());
    }
    setScenarioWindow(windows.windows().front());
    return {};
}

FlightConfigurationPanel* FlightConfigurationScenario::shownTab(std::string& problem) const
{
    FlightConfigurationPanel* const panel = tab();
    if (panel == nullptr)
    {
        problem = "the scenario's window has no Motors & Configuration tab";
        return nullptr;
    }
    if (scenarioWindow().selectedTab() != MainWindow::kFlightConfigurationTab)
    {
        problem = "the Motors & Configuration tab is not shown";
        return nullptr;
    }
    return panel;
}

std::string FlightConfigurationScenario::windowTab(const GuiScenarioStep& step)
{
    if (window() == nullptr)
    {
        return "the scenario's window is not open";
    }
    const QTabWidget& tabs = *scenarioWindow().tabs();
    if (step.tab != kDesignTabName && step.tab != kFlightConfigurationTabName)
    {
        return std::format("the window has no tab {}", step.tab);
    }
    const int index =
        step.tab == kDesignTabName ? MainWindow::kDesignTab : MainWindow::kFlightConfigurationTab;
    if (tabs.currentIndex() == index || !tabs.isTabEnabled(index))
    {
        return std::format("the tab {} is shown already, or disabled", step.tab);
    }
    if (!activateWindowOf(tabs))
    {
        return "the window does not become active";
    }
    QTabBar* const bar = tabs.tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(index).center());
    settle();
    return tabs.currentIndex() == index
               ? std::string()
               : std::format("a click on the tab {} shows another one", step.tab);
}

std::string FlightConfigurationScenario::configTab(const GuiScenarioStep& step)
{
    std::string                           problem;
    const FlightConfigurationPanel* const panel = shownTab(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const QTabWidget&                       tabs = *panel->tabs();
    const std::span<const std::string_view> names(kConfigTabNames);
    const auto                              found = std::ranges::find(names, step.tab);
    if (found == names.end())
    {
        return std::format("the tab has no sub-tab {}", step.tab);
    }
    const int index = static_cast<int>(found - names.begin());
    if (tabs.currentIndex() == index || !tabs.isTabEnabled(index))
    {
        return std::format("the sub-tab {} is shown already, or disabled", step.tab);
    }
    QTabBar* const bar = tabs.tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(index).center());
    settle();
    return tabs.currentIndex() == index
               ? std::string()
               : std::format("a click on the sub-tab {} shows another one", step.tab);
}

std::string FlightConfigurationScenario::selectCells(const GuiScenarioStep& step)
{
    std::string                           problem;
    const FlightConfigurationPanel* const panel = shownTab(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const FlightConfigurablePanel&      current = panel->currentPanel();
    QTableView&                         table   = current.table();
    const FlightConfigurableTableModel& model   = current.tableModel();
    if (!activateWindowOf(table))
    {
        return "the window does not become active";
    }
    for (const GuiCell& cell : step.cells)
    {
        int column = 0;
        if (cell.column.has_value())
        {
            const Result<const RocketComponent*> component =
                QtRocket::Test::resolveGuiComponentPath(panel->rocket(), *cell.column);
            if (!component)
            {
                return component.error().message;
            }
            column = model.columnOf((*component)->getId());
        }
        if (cell.row < 0 || cell.row >= model.rowCount() || column < 0)
        {
            return std::format("the table has no cell {} {}", cell.row,
                               cell.column ? QtRocket::Test::guiComponentPathText(*cell.column)
                                           : std::string("Configuration"));
        }
        const Qt::KeyboardModifiers modifiers = modifiersOf(cell);
        const QModelIndex           index     = model.index(cell.row, column);
        table.scrollTo(index);
        const QRect rect = table.visualRect(index);
        if (!table.viewport()->rect().contains(rect.center()))
        {
            return std::format("the cell {} {} is not in view", cell.row, column);
        }
        QTest::mouseClick(table.viewport(), Qt::LeftButton, modifiers, rect.center());
        settle();
    }
    return {};
}

std::string FlightConfigurationScenario::mountCheck(const GuiScenarioStep& step)
{
    std::string                           problem;
    const FlightConfigurationPanel* const panel = shownTab(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const MotorMountConfigurationPanel& mounts =
        panel->motorConfigurationPanel().motorMountConfigurationPanel();
    QTableView&                 table = mounts.table();
    const MotorMountTableModel& model = mounts.model();
    if (!table.isVisible())
    {
        return "the motor mount table is not shown (the Motors sub-tab is not the current one)";
    }
    const Result<const RocketComponent*> component =
        QtRocket::Test::resolveGuiComponentPath(panel->rocket(), step.mount);
    if (!component)
    {
        return component.error().message;
    }
    const int row = model.rowOf((*component)->getId());
    if (row < 0)
    {
        return std::format("the motor mount table does not list {}",
                           QtRocket::Test::guiComponentPathText(step.mount));
    }
    const QModelIndex box = model.index(row, MotorMountTableModel::kMotorMountColumn);
    if ((model.data(box, Qt::CheckStateRole).toInt() == Qt::Checked) == step.checked)
    {
        return std::format("the check box of {} is in that state already",
                           QtRocket::Test::guiComponentPathText(step.mount));
    }
    if (!activateWindowOf(table))
    {
        return "the window does not become active";
    }
    table.scrollTo(box);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier,
                      table.visualRect(box).center());
    settle();
    // The row of the mount after the change (the table lists the same mounts).
    const int after = model.rowOf((*component)->getId());
    return after >= 0 && (model
                              .data(model.index(after, MotorMountTableModel::kMotorMountColumn),
                                    Qt::CheckStateRole)
                              .toInt() == Qt::Checked) == step.checked
               ? std::string()
               : "a click on the check box leaves it as it was";
}

std::string FlightConfigurationScenario::operation(const GuiScenarioStep& step)
{
    std::string                           problem;
    const FlightConfigurationPanel* const panel = shownTab(problem);
    if (panel == nullptr)
    {
        return problem;
    }
    const MotorConfigurationPanel&            motors   = panel->motorConfigurationPanel();
    const RecoveryConfigurationPanel&         recovery = panel->recoveryConfigurationPanel();
    const SeparationConfigurationPanel&       stages   = panel->separationConfigurationPanel();
    const std::map<GuiStepKind, QPushButton*> buttons{
        {GuiStepKind::NEW_CONFIGURATION, panel->newConfigurationButton()},
        {GuiStepKind::RENAME_CONFIGURATION, panel->renameConfigurationButton()},
        {GuiStepKind::DELETE_CONFIGURATION, panel->deleteConfigurationButton()},
        {GuiStepKind::DUPLICATE_CONFIGURATION, panel->duplicateConfigurationButton()},
        {GuiStepKind::SELECT_MOTOR, motors.selectMotorButton()},
        {GuiStepKind::DELETE_MOTOR, motors.deleteMotorButton()},
        {GuiStepKind::SELECT_IGNITION, motors.selectIgnitionButton()},
        {GuiStepKind::RESET_IGNITION, motors.resetIgnitionButton()},
        {GuiStepKind::SELECT_DEPLOYMENT, recovery.selectDeploymentButton()},
        {GuiStepKind::RESET_DEPLOYMENT, recovery.resetDeploymentButton()},
        {GuiStepKind::SELECT_SEPARATION, stages.selectSeparationButton()},
        {GuiStepKind::RESET_SEPARATION, stages.resetSeparationButton()},
    };
    const auto found = buttons.find(step.kind);
    QTROCKET_ASSERT(found != buttons.end());
    QPushButton* const button = found->second;
    const std::string  name   = button != nullptr ? QtStrings::toUtf8(button->objectName()) : "?";
    if (button == nullptr || !button->isVisible())
    {
        return std::format("the button {} is not shown", name);
    }
    if (!button->isEnabled())
    {
        return std::format("the button {} is disabled", name);
    }
    m_answer                     = step.answer.has_value() ? &*step.answer : nullptr;
    m_dialogs                    = 0;
    m_dialogProblem              = {};
    const std::size_t unanswered = m_tabPrompts->unansweredQuestions();
    if (m_answer != nullptr)
    {
        m_tabPrompts->setDialogAnswer([this](QWidget& dialog) { answerDialog(dialog); });
    }
    QTest::mouseClick(button, Qt::LeftButton);
    m_tabPrompts->setDialogAnswer({});
    m_answer = nullptr;
    settleAndDelete();
    if (!m_dialogProblem.empty())
    {
        return m_dialogProblem;
    }
    if (m_tabPrompts->unansweredQuestions() != unanswered)
    {
        return "the operation asked a question the step has no answer for";
    }
    if (step.answer.has_value() && m_dialogs == 0)
    {
        return "the step has an answer, but the operation asked no question";
    }
    return {};
}

// ================================================================================== the dialogs

void FlightConfigurationScenario::answerDialog(QWidget& dialog)
{
    ++m_dialogs;
    std::string problem;
    if (m_answer == nullptr || m_dialogs > 1)
    {
        problem = std::format("the operation asked a second question ({})", shortClassName(dialog));
    }
    else if (auto* const chooser = qobject_cast<MotorChooserDialog*>(&dialog))
    {
        problem = answerChooser(*chooser, *m_answer);
    }
    else if (auto* const ignition = qobject_cast<IgnitionSelectionDialog*>(&dialog))
    {
        problem = answerIgnition(*ignition, *m_answer);
    }
    else if (auto* const deployment = qobject_cast<DeploymentSelectionDialog*>(&dialog))
    {
        problem = answerDeployment(*deployment, *m_answer);
    }
    else if (auto* const separation = qobject_cast<SeparationSelectionDialog*>(&dialog))
    {
        problem = answerSeparation(*separation, *m_answer);
    }
    else if (auto* const rename = qobject_cast<RenameConfigDialog*>(&dialog))
    {
        problem = answerRename(*rename, *m_answer);
    }
    else
    {
        problem = "an unexpected dialog opened: " + shortClassName(dialog);
    }
    if (!problem.empty())
    {
        if (m_dialogProblem.empty())
        {
            m_dialogProblem = problem;
        }
        // A failed answer leaves its dialog: closed, so that the operation goes on.
        if (auto* const open = qobject_cast<QDialog*>(&dialog);
            open != nullptr && open->isVisible())
        {
            open->reject();
        }
    }
}

std::string FlightConfigurationScenario::answerIgnition(IgnitionSelectionDialog& dialog,
                                                        const GuiAnswer&         answer)
{
    GuiQuestion question = valueQuestion(dialog, GuiQuestionKind::IGNITION);
    question.event       = eventOf<IgnitionEvent>(dialog.eventBox(), QtRocket::name);
    question.delay       = javaText(dialog.delaySpinBox().binding()->value());
    m_questions.push_back(std::move(question));
    std::string        problem;
    QPushButton* const close = valueDialogButton(dialog, answer, problem);
    if (close == nullptr)
    {
        return problem;
    }
    if (answer.button == "ok")
    {
        if (problem = chooseScope(dialog, answer.scope); !problem.empty())
        {
            return problem;
        }
        if (problem = chooseEvent<IgnitionEvent>(dialog.eventBox(), answer.event, QtRocket::name);
            !problem.empty())
        {
            return problem;
        }
        if (problem = typeValue(dialog.delaySpinBox(), nullptr, answer.delay, "IgnitionDelay");
            !problem.empty())
        {
            return problem;
        }
    }
    return clickButton(*close, answer.button);
}

std::string FlightConfigurationScenario::answerDeployment(DeploymentSelectionDialog& dialog,
                                                          const GuiAnswer&           answer)
{
    m_questions.push_back(altitudeQuestion<DeploymentSelectionDialog, DeployEvent>(
        dialog, GuiQuestionKind::DEPLOYMENT, deployEventName));
    std::string        problem;
    QPushButton* const close = valueDialogButton(dialog, answer, problem);
    if (close == nullptr)
    {
        return problem;
    }
    if (answer.button == "ok")
    {
        if (problem = chooseScope(dialog, answer.scope); !problem.empty())
        {
            return problem;
        }
        if (problem = chooseEvent<DeployEvent>(dialog.eventBox(), answer.event, deployEventName);
            !problem.empty())
        {
            return problem;
        }
        // DeploymentSelectionDialog.updateState(): the altitude for the altitude event alone.
        if (dialog.isAltitudeEnabled() != answer.altitude.has_value())
        {
            return "the answer gives the altitude exactly when its field is enabled";
        }
        if (answer.altitude.has_value())
        {
            const ConfigFields::NumberWidgets& altitude = dialog.altitude();
            if (problem = typeValue(*altitude.spinBox, altitude.unitSelector, answer.altitude,
                                    "DeployAltitude");
                !problem.empty())
            {
                return problem;
            }
        }
        if (problem = typeValue(dialog.delaySpinBox(), nullptr, answer.delay, "DeployDelay");
            !problem.empty())
        {
            return problem;
        }
    }
    return clickButton(*close, answer.button);
}

std::string FlightConfigurationScenario::answerSeparation(SeparationSelectionDialog& dialog,
                                                          const GuiAnswer&           answer)
{
    m_questions.push_back(altitudeQuestion<SeparationSelectionDialog, SeparationEvent>(
        dialog, GuiQuestionKind::SEPARATION, separationEventName));
    std::string        problem;
    QPushButton* const close = valueDialogButton(dialog, answer, problem);
    if (close == nullptr)
    {
        return problem;
    }
    if (answer.button == "ok")
    {
        if (problem = chooseScope(dialog, answer.scope); !problem.empty())
        {
            return problem;
        }
        if (problem =
                chooseEvent<SeparationEvent>(dialog.eventBox(), answer.event, separationEventName);
            !problem.empty())
        {
            return problem;
        }
        // SeparationSelectionDialog.updateState(): the altitude for the two altitude events.
        if (dialog.isAltitudeEnabled() != answer.altitude.has_value())
        {
            return "the answer gives the altitude exactly when its field is enabled";
        }
        if (answer.altitude.has_value())
        {
            const ConfigFields::NumberWidgets& altitude = dialog.altitude();
            if (problem = typeValue(*altitude.spinBox, altitude.unitSelector, answer.altitude,
                                    "SeparationAltitude");
                !problem.empty())
            {
                return problem;
            }
        }
        if (problem = typeValue(dialog.delaySpinBox(), nullptr, answer.delay, "SeparationDelay");
            !problem.empty())
        {
            return problem;
        }
    }
    return clickButton(*close, answer.button);
}

std::string FlightConfigurationScenario::answerRename(RenameConfigDialog& dialog,
                                                      const GuiAnswer&    answer)
{
    GuiQuestion question;
    question.kind  = GuiQuestionKind::RENAME;
    question.title = QtStrings::toUtf8(dialog.windowTitle());
    question.name  = QtStrings::toUtf8(dialog.textField().text());
    m_questions.push_back(std::move(question));
    if (answer.name.has_value())
    {
        if (std::string problem = typeText(dialog.textField(), "textbox", *answer.name);
            !problem.empty())
        {
            return problem;
        }
    }
    if (answer.saveAsDefault)
    {
        // Its message goes to the script (FlightConfigurationPrompts: a question of the record).
        if (std::string problem = clickButton(dialog.saveAsDefaultButton(), "Save as default");
            !problem.empty())
        {
            return problem;
        }
        settle();
    }
    if (answer.button == "ok")
    {
        return clickButton(dialog.okButton(), "OK");
    }
    if (answer.button == "reset")
    {
        return clickButton(dialog.resetToDefaultButton(), "Reset to default");
    }
    if (answer.button == "cancel")
    {
        return clickButton(dialog.cancelButton(), "Cancel");
    }
    return "the rename dialog's buttons are ok, reset and cancel";
}

std::string FlightConfigurationScenario::answerChooser(MotorChooserDialog& dialog,
                                                       const GuiAnswer&    answer)
{
    GuiQuestion question;
    question.kind  = GuiQuestionKind::MOTOR;
    question.title = QtStrings::toUtf8(dialog.windowTitle());
    question.shown = chooserState(dialog);
    m_questions.push_back(question);
    ThrustCurveMotorSelectionPanel& panel = dialog.selectionPanel();
    std::string                     problem;
    if (answer.search.has_value())
    {
        auto* const search = namedChild<QLineEdit>(panel, QStringLiteral("searchField"));
        if (search == nullptr)
        {
            return "the chooser has no search field";
        }
        if (search->text() != QtStrings::fromUtf8(*answer.search))
        {
            problem = typeText(*search, "searchField", *answer.search);
        }
    }
    if (problem.empty() && answer.motor.has_value())
    {
        problem = clickMotorRow(panel, *answer.motor);
    }
    if (problem.empty() && answer.curve.has_value())
    {
        problem = chooseCurve(panel, *answer.curve);
    }
    if (problem.empty() && answer.chooserDelay.has_value())
    {
        problem = chooseDelay(panel, *answer.chooserDelay);
    }
    if (problem.empty() && answer.nozzleExitDiameter.has_value())
    {
        auto* const spinBox =
            namedChild<UnitSpinBox>(panel, QStringLiteral("nozzleExitDiameterSpinner"));
        auto* const selector =
            namedChild<UnitSelector>(panel, QStringLiteral("nozzleExitDiameterUnitSelector"));
        problem =
            spinBox == nullptr || selector == nullptr
                ? "the chooser has no nozzle exit diameter field"
                : typeValue(*spinBox, selector, answer.nozzleExitDiameter, "nozzleExitDiameter");
    }
    if (!problem.empty())
    {
        return problem;
    }
    m_questions.back().answered = chooserState(dialog);
    if (answer.button != "ok" && answer.button != "cancel")
    {
        return "the chooser's buttons are ok and cancel";
    }
    auto* const close =
        namedChild<QPushButton>(dialog, answer.button == "ok" ? QStringLiteral("okButton")
                                                              : QStringLiteral("cancelButton"));
    if (close == nullptr)
    {
        return "the chooser has no button " + answer.button;
    }
    return clickButton(*close, answer.button);
}

}  // namespace QtRocket::Gui::Test
