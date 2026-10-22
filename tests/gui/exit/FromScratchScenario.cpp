#include "exit/FromScratchScenario.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractSpinBox>
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
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtTypes>
#include <QtWidgets/qtestsupport_widgets.h>

#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/FinPointTableModel.h"
#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"
#include "QtRocket/gui/main/ComponentAddButtons.h"
#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "exit/FromScratchPrompts.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenGuiReference.h"
#include "main/FileMenusTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui::Test
{

namespace
{

using QtRocket::Test::GuiComponentPath;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::GuiStepRecord;

/// The name of the preferences file in the scenario's directory.
constexpr std::string_view kPreferencesFile = "QtRocket.ini";

/// Delivers the posted events until the windows, the dialogs and their layouts are done.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

/// settle(), then deletes what was given to deleteLater(), which processEvents() leaves: a
/// window or a dialog that was closed.
void settleAndDelete()
{
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// The class name of @p object without its namespace ("NoseConeConfig").
[[nodiscard]] std::string shortClassName(const QObject& object)
{
    const std::string_view name = object.metaObject()->className();
    const std::size_t      at   = name.rfind("::");
    return std::string(at == std::string_view::npos ? name : name.substr(at + 2));
}

/// Makes the window of @p widget the active one, which the keyboard focus needs (offscreen as
/// well); whether it is.
[[nodiscard]] bool activateWindowOf(const QWidget& widget)
{
    QWidget* const window = widget.window();
    if (!window->isActiveWindow())
    {
        window->activateWindow();
        return QTest::qWaitForWindowActive(window);
    }
    return true;
}

/// Chooses, by a click on its tab, every tab page that holds @p widget and is not the current
/// one, from the outermost in: what a user does to reach a field of another tab.
void showTabsOf(const QWidget& widget)
{
    std::vector<std::pair<QTabWidget*, const QWidget*>> pages;
    for (const QWidget* page = &widget; page->parentWidget() != nullptr;
         page                = page->parentWidget())
    {
        const auto* const stack = qobject_cast<const QStackedWidget*>(page->parentWidget());
        auto* const       tabs =
            stack != nullptr ? qobject_cast<QTabWidget*>(stack->parentWidget()) : nullptr;
        if (tabs != nullptr && tabs->indexOf(page) >= 0)
        {
            pages.emplace_back(tabs, page);
        }
    }
    for (const auto& [tabs, page] : std::views::reverse(pages))
    {
        const int index = tabs->indexOf(page);
        if (tabs->currentIndex() != index)
        {
            QTabBar* const bar = tabs->tabBar();
            QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(index).center());
        }
    }
}

/// The widgets named @p name below @p root that are of class W, hidden or not.
template <class W>
[[nodiscard]] std::vector<W*> namedChildren(const QWidget& root, const std::string& name)
{
    std::vector<W*> found;
    for (QWidget* const child : root.findChildren<QWidget*>(QtStrings::fromUtf8(name)))
    {
        if (auto* const typed = qobject_cast<W*>(child))
        {
            found.push_back(typed);
        }
    }
    return found;
}

/// The one widget of class W named @p field below @p root, its tabs chosen (showTabsOf()) and
/// shown; null with what went wrong in @p problem otherwise.
template <class W>
[[nodiscard]] W* fieldOf(const QWidget& root, const std::string& field, std::string_view what,
                         std::string& problem)
{
    const std::vector<W*> found = namedChildren<W>(root, field);
    if (found.size() != 1)
    {
        problem = std::format("the dialog has {} {} named {}", found.size(), what, field);
        return nullptr;
    }
    W* const widget = found.front();
    showTabsOf(*widget);
    if (!widget->isVisible())
    {
        problem = std::format("the {} {} is not shown", what, field);
        return nullptr;
    }
    if (!widget->isEnabled())
    {
        problem = std::format("the {} {} is disabled", what, field);
        return nullptr;
    }
    return widget;
}

/// Gives @p widget the keyboard focus, as a click does; what went wrong, or "".
[[nodiscard]] std::string focusOn(QWidget& widget, const std::string& field)
{
    if (!activateWindowOf(widget))
    {
        return std::format("the window of the field {} does not become active", field);
    }
    widget.setFocus(Qt::MouseFocusReason);
    settle();
    return widget.hasFocus() ? std::string()
                             : std::format("the field {} does not take the focus", field);
}

/// Chooses @p unit in the unit selector of the number field @p field below @p root, through its
/// menu when it shows another unit; what went wrong, or "".
[[nodiscard]] std::string chooseUnit(const QWidget& root, const std::string& field,
                                     const std::string& unit)
{
    const std::vector<UnitSelector*> selectors = namedChildren<UnitSelector>(root, field);
    if (selectors.size() != 1)
    {
        return std::format("the field {} has {} unit selectors", field, selectors.size());
    }
    UnitSelector& selector = *selectors.front();
    const QString wanted   = QtStrings::fromUtf8(unit);
    if (selector.text() == wanted)
    {
        return {};
    }
    const OffscreenPopupWarnings popup;
    QTest::mouseClick(&selector, Qt::LeftButton);
    QMenu* const menu = selector.unitMenu();
    if (menu == nullptr || !menu->isVisible())
    {
        return std::format("the unit selector of {} opens no menu", field);
    }
    QAction* const item = menuItem(*menu, wanted);
    if (item == nullptr)
    {
        menu->hide();
        return std::format("the unit selector of {} offers no {}", field, unit);
    }
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(item).center());
    if (selector.text() != wanted)
    {
        return std::format("the unit selector of {} shows {} after {} was chosen", field,
                           QtStrings::toUtf8(selector.text()), unit);
    }
    return {};
}

/// Types @p text over the number field @p spinBox and commits it with Enter.
[[nodiscard]] std::string typeNumber(QAbstractSpinBox& spinBox, const std::string& field,
                                     const std::string& text)
{
    if (!activateWindowOf(spinBox))
    {
        return std::format("the window of the field {} does not become active", field);
    }
    typeInto(spinBox, QtStrings::fromUtf8(text));
    QTest::keyClick(&spinBox, Qt::Key_Return);
    settle();
    return {};
}

/// Types @p text over the text field @p line and commits it with Enter.
[[nodiscard]] std::string typeLine(QLineEdit& line, const std::string& field,
                                   const std::string& text)
{
    if (const std::string problem = focusOn(line, field); !problem.empty())
    {
        return problem;
    }
    line.selectAll();
    QTest::keyClicks(&line, QtStrings::fromUtf8(text));
    QTest::keyClick(&line, Qt::Key_Return);
    settle();
    return {};
}

/// Types @p text over the text area @p area and commits it as its focus loss does (Enter is a
/// new line there).
[[nodiscard]] std::string typeArea(QPlainTextEdit& area, const std::string& field,
                                   const std::string& text)
{
    if (const std::string problem = focusOn(area, field); !problem.empty())
    {
        return problem;
    }
    area.selectAll();
    QTest::keyClicks(&area, QtStrings::fromUtf8(text));
    area.clearFocus();
    settle();
    return {};
}

/// The index of the item of @p combo whose value is the Java enum constant @p item (@p name
/// gives a value's constant): nullopt when @p combo is no combo box of E or has no such item.
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

/// The index of the item of @p combo, an enum's combo box of one of the dialogs of the scenario,
/// whose value is the Java enum constant @p item; nullopt when there is none.
[[nodiscard]] std::optional<int> enumItemIndex(const EnumComboBox& combo, std::string_view item)
{
    using DeployEvent = DeploymentConfiguration::DeployEvent;
    using MassType    = MassComponent::MassComponentType;
    for (const std::optional<int> index : {
             enumItemIndex<AxialMethod>(combo, item, axialMethodName),
             enumItemIndex<Finish>(combo, item, finishName),
             enumItemIndex<FinCrossSection>(combo, item, finCrossSectionName),
             enumItemIndex<TransitionShape>(combo, item, transitionShapeName),
             enumItemIndex<DeployEvent>(combo, item, deployEventName),
             enumItemIndex<MassType>(combo, item, massComponentTypeName),
         })
    {
        if (index.has_value())
        {
            return index;
        }
    }
    return std::nullopt;
}

/// The index of the material named @p item among the items of @p combo; nullopt when there is
/// none.
[[nodiscard]] std::optional<int> materialItemIndex(const MaterialComboBox& combo,
                                                   std::string_view        item)
{
    const std::vector<Material>& materials = combo.binding()->materials();
    for (std::size_t index = 0; index < materials.size(); ++index)
    {
        if (materials[index].getName() == item)
        {
            return static_cast<int>(index);
        }
    }
    return std::nullopt;
}

/// The column of the fin point table that @p name names ("X", "Y"); -1 for another name.
[[nodiscard]] int pointColumn(std::string_view name)
{
    if (name == "X")
    {
        return FinPointTableModel::kXColumn;
    }
    return name == "Y" ? FinPointTableModel::kYColumn : -1;
}

/// Triggers @p action, the item named @p name, as its menu item or its keys do; what went wrong
/// (it is disabled), or "".
[[nodiscard]] std::string triggerAction(QAction* action, std::string_view name)
{
    if (action == nullptr || !action->isEnabled())
    {
        return std::format("{} is not enabled", name);
    }
    action->trigger();
    settle();
    return {};
}

/// The panel of the visible configuration dialog when it is a freeform fin set's; null otherwise.
[[nodiscard]] const FreeformFinSetConfig* openFreeformPanel()
{
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    return dialog != nullptr && dialog->isVisible()
               ? dynamic_cast<const FreeformFinSetConfig*>(&dialog->configurator())
               : nullptr;
}

/// editPoint: a click on the cell @p step names, which edits it with its text selected, the text
/// typed and Enter; what went wrong, or "".
[[nodiscard]] std::string editPoint(const GuiScenarioStep& step)
{
    const FreeformFinSetConfig* const panel = openFreeformPanel();
    if (panel == nullptr)
    {
        return "no dialog of a freeform fin set is open";
    }
    QTableView&               table  = panel->pointTable();
    const FinPointTableModel& model  = panel->pointTableModel();
    const int                 column = pointColumn(step.column);
    const QModelIndex         cell   = model.index(step.row, column);
    if (!cell.isValid() || !model.flags(cell).testFlag(Qt::ItemIsEditable))
    {
        return std::format("the table has no editable cell {} {}", step.row, step.column);
    }
    const QString header = model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    if (step.unit.has_value() && header != QtStrings::fromUtf8(step.column + " / " + *step.unit))
    {
        return std::format("the column is {}", QtStrings::toUtf8(header));
    }
    showTabsOf(table);
    if (!activateWindowOf(table))
    {
        return "the dialog does not become active";
    }
    table.scrollTo(cell);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier,
                      table.visualRect(cell).center());
    auto* const editor = qobject_cast<QLineEdit*>(table.indexWidget(cell));
    if (editor == nullptr)
    {
        return "a click on the cell opens no editor";
    }
    if (editor->selectedText() != editor->text())
    {
        editor->selectAll();
    }
    QTest::keyClicks(editor, QtStrings::fromUtf8(step.text));
    QTest::keyClick(editor, Qt::Key_Return);
    // The delegate commits and closes the editor later (a queued call).
    settle();
    return {};
}

/// insertPoint: a right click on the row @p step names and "Insert point" of the menu it shows;
/// what went wrong, or "".
[[nodiscard]] std::string insertPoint(const GuiScenarioStep& step)
{
    const FreeformFinSetConfig* const panel = openFreeformPanel();
    if (panel == nullptr)
    {
        return "no dialog of a freeform fin set is open";
    }
    QTableView&       table = panel->pointTable();
    const QModelIndex cell  = panel->pointTableModel().index(step.row, 0);
    if (!cell.isValid())
    {
        return std::format("the table has no row {}", step.row);
    }
    showTabsOf(table);
    if (!activateWindowOf(table))
    {
        return "the dialog does not become active";
    }
    table.scrollTo(cell);
    const OffscreenPopupWarnings popup;
    QTest::mouseClick(table.viewport(), Qt::RightButton, Qt::NoModifier,
                      table.visualRect(cell).center());
    QMenu& menu = panel->tablePointMenu();
    if (!menu.isVisible() || panel->selectedPointRow() != step.row)
    {
        menu.hide();
        return "a right click on the row shows no menu or selects another row";
    }
    QAction& insert = panel->insertPointAction();
    if (!insert.isEnabled() || !menu.actions().contains(&insert))
    {
        menu.hide();
        return "the menu's Insert point is not enabled";
    }
    QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, menu.actionGeometry(&insert).center());
    if (menu.isVisible())
    {
        menu.hide();
        return "the menu stays open";
    }
    settle();
    return {};
}

/// ok: a click on OK of the configuration dialog; what went wrong, or "".
[[nodiscard]] std::string okStep()
{
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    if (dialog == nullptr || !dialog->isVisible())
    {
        return "no configuration dialog is open";
    }
    QTest::mouseClick(&dialog->configurator().okButton(), Qt::LeftButton);
    settleAndDelete();
    return ComponentConfigDialog::isDialogVisible() ? "the dialog stays open" : std::string();
}

}  // namespace

FromScratchScenario::FromScratchScenario(std::filesystem::path directory)
  : m_directory(std::move(directory))
{
    auto prompts = std::make_unique<FromScratchPrompts>(m_scripted);
    m_prompts    = prompts.get();
    auto context = std::make_unique<ApplicationContext>(
        std::make_unique<SettingsPreferences>(
            QtStrings::fromPath(m_directory / std::filesystem::path(kPreferencesFile))),
        DataPaths::forApplication(), std::move(prompts));
    ApplicationSession::Options options;
    options.lastWindowBehaviour    = WindowManager::LastWindowBehaviour::KEEP_RUNNING;
    Application* const application = Application::current();
    QTROCKET_ASSERT(application != nullptr);
    m_session = std::make_unique<ApplicationSession>(*application, std::move(context), options);
    settle();
}

FromScratchScenario::~FromScratchScenario()
{
    // The windows, the documents and the services go first, then the closed windows and dialogs
    // they gave to deleteLater().
    m_session = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

std::string FromScratchScenario::run(const GuiScenario& scenario, const Observer& observer)
{
    m_steps    = &scenario.steps;
    m_next     = 0;
    m_observer = observer;
    std::string failure;
    while (failure.empty() && m_next < m_steps->size())
    {
        const GuiScenarioStep& step    = (*m_steps)[m_next++];
        const std::string      problem = perform(step);
        if (!problem.empty())
        {
            failure = stepText(step) + ": " + problem;
        }
        else if (step.kind != GuiStepKind::SAVE_AS)  // Save as records its steps itself
        {
            observe(step);
        }
    }
    m_steps    = nullptr;
    m_observer = nullptr;
    return failure;
}

std::string FromScratchScenario::perform(const GuiScenarioStep& step)
{
    if (step.kind != GuiStepKind::NEW && m_window.isNull())
    {
        return "the scenario's window is not open";
    }
    switch (step.kind)
    {
        case GuiStepKind::NEW:
            return newDesign();
        case GuiStepKind::SELECT:
            return select(step);
        case GuiStepKind::ADD:
            return add(step);
        case GuiStepKind::EDIT:
            return triggerAction(scenarioWindow().menus().editAction(), "Edit > Edit");
        case GuiStepKind::TYPE:
            return type(step);
        case GuiStepKind::CHOOSE:
            return choose(step);
        case GuiStepKind::CHECK:
            return check(step);
        case GuiStepKind::EDIT_POINT:
            return editPoint(step);
        case GuiStepKind::INSERT_POINT:
            return insertPoint(step);
        case GuiStepKind::BUTTON:
            return button(step);
        case GuiStepKind::OK:
            return okStep();
        case GuiStepKind::UNDO:
            return triggerAction(scenarioWindow().undoRedoActions().undoAction(), "Undo");
        case GuiStepKind::REDO:
            return triggerAction(scenarioWindow().undoRedoActions().redoAction(), "Redo");
        case GuiStepKind::COPY:
            return triggerAction(designPanel().rocketActions().copyAction(), "Copy");
        case GuiStepKind::PASTE:
            return triggerAction(designPanel().rocketActions().pasteAction(), "Paste");
        case GuiStepKind::SAVE_AS:
            return saveAs(step);
        case GuiStepKind::CHOOSE_FILE:
            return "no file chooser is open";
    }
    QTROCKET_UNREACHABLE();
}

std::string FromScratchScenario::newDesign()
{
    if (!m_window.isNull())
    {
        return "the scenario's window is open already";
    }
    const WindowManager& windows = m_session->windows();
    if (windows.windowCount() != 1)
    {
        return std::format("{} windows are open at the start, not one", windows.windowCount());
    }
    const QPointer<MainWindow> start = windows.windows().front();
    if (const std::string problem = triggerAction(start->menus().newAction(), "File > New");
        !problem.empty())
    {
        return problem;
    }
    settleAndDelete();
    // The New design of the start was replaced (BasicFrame.closeIfReplaceable()).
    if (windows.windowCount() != 1 || !start.isNull())
    {
        return std::format("{} windows after File > New, the start's closed: {}",
                           windows.windowCount(), start.isNull());
    }
    m_window = windows.windows().front();
    return {};
}

std::string FromScratchScenario::select(const GuiScenarioStep& step)
{
    const DesignPanel&        panel = designPanel();
    ComponentTreeView&        tree  = panel.tree();
    const ComponentTreeModel& model = panel.treeModel();
    if (!activateWindowOf(tree))
    {
        return "the window does not become active";
    }
    bool first = true;
    for (const GuiComponentPath& path : step.paths)
    {
        const Result<const RocketComponent*> component =
            QtRocket::Test::resolveGuiComponentPath(scenarioWindow().document().getRocket(), path);
        if (!component)
        {
            return component.error().message;
        }
        const QModelIndex index = model.indexOf(**component);
        tree.scrollTo(index);
        const QRect row = tree.visualRect(index);
        if (!index.isValid() || !tree.viewport()->rect().contains(row.center()))
        {
            return std::format("the row of {} is not in view",
                               QtRocket::Test::guiComponentPathText(path));
        }
        QTest::mouseClick(tree.viewport(), Qt::LeftButton,
                          first ? Qt::NoModifier : Qt::ControlModifier, row.center());
        first = false;
    }
    settle();
    return {};
}

std::string FromScratchScenario::add(const GuiScenarioStep& step)
{
    const DesignPanel& panel = designPanel();
    ComponentButton*   found = nullptr;
    for (ComponentButton* const candidate : panel.addButtons().buttons())
    {
        if (QtStrings::toUtf8(candidate->objectName()) == step.component)
        {
            found = candidate;
        }
    }
    if (found == nullptr)
    {
        return std::format("there is no add button {}", step.component);
    }
    if (!found->isEnabled())
    {
        return std::format("the add button {} is disabled", step.component);
    }
    panel.addButtonsScrollArea()->ensureWidgetVisible(found);
    QTest::mouseClick(found, Qt::LeftButton);
    settle();
    return {};
}

std::string FromScratchScenario::type(const GuiScenarioStep& step)
{
    const QWidget* const dialog = openDialog();
    if (dialog == nullptr)
    {
        return "no dialog is open";
    }
    const std::vector<QAbstractSpinBox*> numbers =
        namedChildren<QAbstractSpinBox>(*dialog, step.field);
    const std::vector<QLineEdit*>      lines = namedChildren<QLineEdit>(*dialog, step.field);
    const std::vector<QPlainTextEdit*> areas = namedChildren<QPlainTextEdit>(*dialog, step.field);
    std::string                        problem;
    if (numbers.size() + lines.size() + areas.size() != 1)
    {
        return std::format("the dialog has {} fields named {}",
                           numbers.size() + lines.size() + areas.size(), step.field);
    }
    if (!numbers.empty())
    {
        auto* const spinBox =
            fieldOf<QAbstractSpinBox>(*dialog, step.field, "number field", problem);
        if (spinBox == nullptr)
        {
            return problem;
        }
        if (step.unit.has_value())
        {
            problem = chooseUnit(*dialog, step.field, *step.unit);
        }
        return problem.empty() ? typeNumber(*spinBox, step.field, step.text) : problem;
    }
    if (step.unit.has_value())
    {
        return std::format("the text field {} has no unit", step.field);
    }
    if (!lines.empty())
    {
        auto* const line = fieldOf<QLineEdit>(*dialog, step.field, "text field", problem);
        return line == nullptr ? problem : typeLine(*line, step.field, step.text);
    }
    auto* const area = fieldOf<QPlainTextEdit>(*dialog, step.field, "text area", problem);
    return area == nullptr ? problem : typeArea(*area, step.field, step.text);
}

std::string FromScratchScenario::choose(const GuiScenarioStep& step)
{
    const QWidget* const dialog = openDialog();
    if (dialog == nullptr)
    {
        return "no dialog is open";
    }
    std::string problem;
    auto* const combo = fieldOf<QComboBox>(*dialog, step.field, "combo box", problem);
    if (combo == nullptr)
    {
        return problem;
    }
    std::optional<int> index;
    if (const auto* const enumCombo = qobject_cast<const EnumComboBox*>(combo))
    {
        index = enumItemIndex(*enumCombo, step.item);
    }
    else if (const auto* const materials = qobject_cast<const MaterialComboBox*>(combo))
    {
        index = materialItemIndex(*materials, step.item);
    }
    if (!index.has_value())
    {
        return std::format("the combo box {} offers no {}", step.field, step.item);
    }
    if (!activateWindowOf(*combo))
    {
        return "the dialog does not become active";
    }
    chooseItem(*combo, *index);
    settle();
    return {};
}

std::string FromScratchScenario::check(const GuiScenarioStep& step)
{
    const QWidget* const dialog = openDialog();
    if (dialog == nullptr)
    {
        return "no dialog is open";
    }
    std::string problem;
    auto* const box = fieldOf<QCheckBox>(*dialog, step.field, "check box", problem);
    if (box == nullptr)
    {
        return problem;
    }
    if (box->isChecked() != step.checked)
    {
        box->click();
        settle();
    }
    return box->isChecked() == step.checked
               ? std::string()
               : std::format("the check box {} stays {}", step.field,
                             box->isChecked() ? "checked" : "unchecked");
}

std::string FromScratchScenario::button(const GuiScenarioStep& step)
{
    const QWidget* const dialog = openDialog();
    if (dialog == nullptr)
    {
        return "no dialog is open";
    }
    const std::span<const ButtonName> buttons(kButtons);
    const auto name = std::ranges::find(buttons, step.button, &ButtonName::step);
    if (name == buttons.end())
    {
        return std::format("no button is known as {}", step.button);
    }
    std::string problem;
    auto* const found =
        fieldOf<QPushButton>(*dialog, std::string(name->objectName), "button", problem);
    if (found == nullptr)
    {
        return problem;
    }
    QTest::mouseClick(found, Qt::LeftButton);
    settleAndDelete();
    return {};
}

std::string FromScratchScenario::saveAs(const GuiScenarioStep& step)
{
    if (openDialog() != nullptr)
    {
        return "a dialog is open";
    }
    QAction* const saveAsItem = scenarioWindow().menus().saveAsAction();
    if (saveAsItem == nullptr || !saveAsItem->isEnabled())
    {
        return "File > Save as is not enabled";
    }
    m_saveAsProblem.clear();
    m_chosenFile = nullptr;
    bool shown   = false;
    {
        const NextModalDialog answer([this, &step, &shown](QWidget& widget) {
            auto* const dialog = qobject_cast<SaveDesignInfoDialog*>(&widget);
            if (dialog == nullptr)
            {
                m_saveAsProblem = "Save as shows the dialog " + shortClassName(widget);
                widget.close();
                return;
            }
            shown               = true;
            m_designInformation = dialog;
            observe(step);
            m_saveAsProblem     = designInformationSteps(*dialog);
            m_designInformation = nullptr;
            if (!m_saveAsProblem.empty() && dialog->isVisible())
            {
                dialog->reject();  // as its OK: the save goes on
            }
        });
        m_prompts->setSaveFileChooser(
            [this](const UserPrompts::SaveFileRequest&) { return chooseFile(); });
        saveAsItem->trigger();
        m_prompts->setSaveFileChooser({});
    }
    settle();
    if (!m_saveAsProblem.empty())
    {
        return m_saveAsProblem;
    }
    if (!shown)
    {
        return "Save as shows no design information dialog";
    }
    if (m_chosenFile == nullptr)
    {
        return "Save as asks for no file";
    }
    m_savedFile = scenarioWindow().document().getFile();
    observe(*m_chosenFile);
    return {};
}

std::string FromScratchScenario::designInformationSteps(SaveDesignInfoDialog& dialog)
{
    while (m_next < m_steps->size())
    {
        const GuiScenarioStep& step = (*m_steps)[m_next++];
        if (step.kind == GuiStepKind::OK)
        {
            m_pendingOk = &step;
            QTest::mouseClick(&dialog.okButton(), Qt::LeftButton);
            return dialog.isVisible() ? stepText(step) + ": the dialog stays open" : std::string();
        }
        if (step.kind != GuiStepKind::TYPE)
        {
            return stepText(step) + ": no step of the design information dialog";
        }
        if (const std::string problem = type(step); !problem.empty())
        {
            return stepText(step) + ": " + problem;
        }
        observe(step);
    }
    return "the scenario ends in the design information dialog";
}

std::optional<UserPrompts::SaveFileAnswer> FromScratchScenario::chooseFile()
{
    m_fileChooserOpen = true;
    if (m_pendingOk != nullptr)
    {
        observe(*m_pendingOk);
        m_pendingOk = nullptr;
    }
    m_fileChooserOpen = false;
    if (m_next >= m_steps->size() || (*m_steps)[m_next].kind != GuiStepKind::CHOOSE_FILE)
    {
        if (m_saveAsProblem.empty())
        {
            m_saveAsProblem = "the file chooser is open, the next step is none of it";
        }
        return std::nullopt;
    }
    m_chosenFile = &(*m_steps)[m_next++];
    // The design as it is saved.
    const OpenRocketDocument& document = scenarioWindow().document();
    m_savedTree = QtRocket::Test::describeRocket(document.getRocket(),
                                                 m_session->context().services().preferences(), {});
    m_savedPreferences = QtRocket::Test::documentPreferenceLines(document);
    return UserPrompts::SaveFileAnswer{.file      = m_directory / pathFromUtf8(m_chosenFile->file),
                                       .directory = m_directory,
                                       .saveSimulationData = std::nullopt};
}

std::string FromScratchScenario::closeWindow()
{
    if (m_window.isNull())
    {
        return "the scenario's window is not open";
    }
    if (const std::string problem =
            triggerAction(scenarioWindow().menus().closeAction(), "File > Close design");
        !problem.empty())
    {
        return problem;
    }
    settleAndDelete();
    if (!m_window.isNull() || m_session->windows().windowCount() != 0)
    {
        return std::format("{} windows are open after File > Close design",
                           m_session->windows().windowCount());
    }
    return {};
}

MainWindow* FromScratchScenario::openSavedFile(std::string& problem)
{
    const MainMenus* const menus = m_session->windows().noWindowMenus();
    if (!m_savedFile.has_value() || menus == nullptr)
    {
        problem = "nothing was saved, or the application has no menus of no window";
        return nullptr;
    }
    m_scripted.answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {*m_savedFile}, .directory = m_directory});
    if (problem = triggerAction(menus->openAction(), "File > Open"); !problem.empty())
    {
        return nullptr;
    }
    settleAndDelete();
    const std::vector<MainWindow*> windows = m_session->windows().windows();
    if (windows.size() != 1)
    {
        problem = std::format("{} windows are open after File > Open", windows.size());
        return nullptr;
    }
    return windows.front();
}

GuiStepRecord FromScratchScenario::record(const GuiScenarioStep& step) const
{
    GuiStepRecord record;
    record.index = step.index;
    record.kind  = step.kind;
    if (m_window.isNull())
    {
        return record;
    }
    const UndoRedoActions& actions = scenarioWindow().undoRedoActions();
    record.undo = {.text    = QtStrings::toUtf8(shownMenuText(actions.undoAction()->text())),
                   .enabled = actions.undoAction()->isEnabled()};
    record.redo = {.text    = QtStrings::toUtf8(shownMenuText(actions.redoAction()->text())),
                   .enabled = actions.redoAction()->isEnabled()};
    const OpenRocketDocument& document = scenarioWindow().document();
    record.saved                       = document.isSaved();
    for (const RocketComponent* const component : designPanel().selection().selectedComponents())
    {
        record.selection.push_back(QtRocket::Test::guiComponentPathOf(*component));
    }
    if (m_fileChooserOpen)
    {
        record.window = "file chooser";
    }
    else if (m_designInformation != nullptr)
    {
        // OpenRocket's panel of the dialog, of the rocket.
        record.window          = "SaveDesignInfoPanel";
        record.windowComponent = QtRocket::Test::guiComponentPathOf(document.getRocket());
    }
    else if (ComponentConfigDialog::isDialogVisible())
    {
        const RocketComponentConfig& panel = ComponentConfigDialog::dialog()->configurator();
        record.window                      = shortClassName(panel);
        record.windowComponent = QtRocket::Test::guiComponentPathOf(panel.selection()->getMain());
    }
    std::vector<std::string> tree = QtRocket::Test::describeRocket(
        document.getRocket(), m_session->context().services().preferences(), {});
    record.treeSha256 = QtRocket::Test::guiTreeSha256(tree);
    record.tree       = std::move(tree);
    return record;
}

std::string FromScratchScenario::stepText(const GuiScenarioStep& step)
{
    std::string text =
        std::format("step {} ({}", step.index, QtRocket::Test::guiStepName(step.kind));
    for (const GuiComponentPath& path : step.paths)
    {
        text += " " + QtRocket::Test::guiComponentPathText(path);
    }
    for (const std::string* const part :
         {&step.component, &step.field, &step.text, &step.item, &step.button, &step.file})
    {
        if (!part->empty())
        {
            text += " " + *part;
        }
    }
    if (step.kind == GuiStepKind::EDIT_POINT || step.kind == GuiStepKind::INSERT_POINT)
    {
        text += std::format(" row {}", step.row);
        text += step.column.empty() ? std::string() : " " + step.column;
    }
    if (step.unit.has_value())
    {
        text += " " + *step.unit;
    }
    if (step.kind == GuiStepKind::CHECK)
    {
        text += step.checked ? " checked" : " unchecked";
    }
    return text + ")";
}

QString FromScratchScenario::shownMenuText(const QString& text)
{
    QString shown;
    for (qsizetype at = 0; at < text.size(); ++at)
    {
        if (text.at(at) == u'&' && at + 1 < text.size())
        {
            ++at;  // the mark goes; "&&" shows one '&'
        }
        shown += text.at(at);
    }
    return shown;
}

void FromScratchScenario::observe(const GuiScenarioStep& step) const
{
    if (m_observer)
    {
        m_observer(step, record(step));
    }
}

MainWindow& FromScratchScenario::scenarioWindow() const
{
    // Through a plain pointer that is checked: GCC's -Wnull-dereference sees no check through
    // QPointer's operator->.
    MainWindow* const window = m_window.data();
    QTROCKET_ASSERT(window != nullptr);
    return *window;
}

DesignPanel& FromScratchScenario::designPanel() const
{
    DesignPanel* const panel = scenarioWindow().componentDesignPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

QWidget* FromScratchScenario::openDialog() const
{
    if (m_designInformation != nullptr)
    {
        return m_designInformation;
    }
    ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    return dialog != nullptr && dialog->isVisible() ? dialog : nullptr;
}

}  // namespace QtRocket::Gui::Test
