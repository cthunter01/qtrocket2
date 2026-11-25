#include "simulation/SimulationTablePinChecks.h"

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <QAction>
#include <QFont>
#include <QModelIndex>
#include <QRect>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/Simulation.h"
#include "simulation/SimulationTablePins.h"

namespace QtRocket::Gui::Test
{

namespace
{

using Column = SimulationColumns::Column;

/// The text of a QVariant (empty for none).
[[nodiscard]] std::string variantText(const QVariant& value)
{
    return QtStrings::toUtf8(value.toString());
}

/// The cell of @p model at @p row and @p column, for @p role.
[[nodiscard]] QVariant cellData(const SimulationTableModel& model, int row, Column column, int role)
{
    return model.data(model.index(row, SimulationColumns::indexOf(column)), role);
}

/// What OpenRocket's WarningsBox shows for the counts of the cell at @p index: the delegate's
/// parts, a text as it is and an icon as [HIGH], [NORMAL] or [LOW].
[[nodiscard]] std::string boxText(const QModelIndex& index)
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
            text += QtStrings::toUtf8(part.text);
        }
        else if (*part.icon == Icons::Id::WARNING_HIGH)
        {
            text += "[HIGH]";
        }
        else
        {
            text += *part.icon == Icons::Id::WARNING_NORMAL ? "[NORMAL]" : "[LOW]";
        }
    }
    return text;
}

/// The priority's name as OpenRocket prints it (MessagePriority.name()).
[[nodiscard]] std::string_view priorityName(MessagePriority priority)
{
    switch (priority)
    {
        case MessagePriority::HIGH:
            return "HIGH";
        case MessagePriority::NORMAL:
            return "NORMAL";
        case MessagePriority::LOW:
            return "LOW";
    }
    return {};
}

/// The warning counts of the row @p row as the pins hold them: critical, normal, informational.
[[nodiscard]] std::optional<std::array<std::size_t, 3>> countsOf(const SimulationTableModel& model,
                                                                 int                         row)
{
    const std::optional<SimulationColumns::WarningCounts> counts = model.warningCounts(row);
    if (!counts.has_value())
    {
        return std::nullopt;
    }
    return std::array{counts->critical, counts->normal, counts->informational};
}

/// The warnings of @p simulation, in the set's order: their priorities' names and texts.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> messagesOf(
    const Simulation& simulation)
{
    std::vector<std::pair<std::string, std::string>> messages;
    if (const WarningSet* warnings = simulation.getSimulatedWarnings(); warnings != nullptr)
    {
        for (const Warning& warning : *warnings)
        {
            messages.emplace_back(priorityName(warning.priority()), warning.toString());
        }
    }
    return messages;
}

/// The warnings column of the row @p row is the pin's @p pin.
void expectWarnings(const SimulationTableModel& model, int row,
                    const SimulationTablePinWarnings& pin)
{
    EXPECT_EQ(countsOf(model, row), pin.counts);
    const QModelIndex index = model.index(row, SimulationColumns::indexOf(Column::WARNINGS));
    EXPECT_EQ(boxText(index), pin.box);
    EXPECT_EQ(variantText(model.data(index, Qt::ToolTipRole)), pin.toolTip);
    EXPECT_EQ(messagesOf(*model.simulation(row)), pin.messages);
}

/// The status of the row @p row of @p model is the pin's row @p pin's.
void expectStatus(const SimulationTableModel& model, int row, const SimulationTablePinRow& pin)
{
    EXPECT_EQ(name(model.status(row)), pin.status);
    EXPECT_EQ(variantText(cellData(model, row, Column::STATUS, Qt::AccessibleTextRole)),
              pin.statusIcon);
    const auto* sortText = std::get_if<std::string>(&model.cellValue(row, Column::STATUS));
    EXPECT_EQ(sortText != nullptr ? *sortText : std::string("<none>"), pin.statusSortText);
    EXPECT_EQ(QtStrings::toUtf8(
                  SimulationColumns::statusDescription(model.status(row), *model.simulation(row))),
              pin.description);
    EXPECT_EQ(variantText(cellData(model, row, Column::NAME, Qt::ToolTipRole)), pin.statusToolTip);
    EXPECT_EQ(variantText(cellData(model, row, Column::STATUS, Qt::ToolTipRole)),
              pin.statusToolTip);
}

/// The cells of the model columns 2 to 13 of the row @p row as the pins hold them: the text,
/// nullopt for none.
[[nodiscard]] std::vector<std::optional<std::string>> cellsOf(const SimulationTableModel& model,
                                                              int                         row)
{
    std::vector<std::optional<std::string>> cells;
    for (int column = 2; column < SimulationColumns::kColumnCount; ++column)
    {
        const QVariant shown = model.data(model.index(row, column), Qt::DisplayRole);
        cells.push_back(shown.isValid() ? std::optional(variantText(shown)) : std::nullopt);
    }
    return cells;
}

/// The pins' text of the state of the action @p name in @p pinned: '1', '0', '?' for none.
[[nodiscard]] char pinnedState(const std::map<std::string, bool>& pinned, const std::string& name)
{
    const auto state = pinned.find(name);
    if (state == pinned.end())
    {
        return '?';
    }
    return state->second ? '1' : '0';
}

}  // namespace

void expectSimulationTablePinRow(const SimulationTableModel& model, int row,
                                 const SimulationTablePinRow& pin)
{
    SCOPED_TRACE(testing::Message() << "row " << row << " (" << pin.row << ")");
    expectStatus(model, row, pin);
    if (pin.fresh)
    {
        return;
    }
    EXPECT_TRUE(pin.warnings.has_value());
    if (pin.warnings.has_value())
    {
        expectWarnings(model, row, *pin.warnings);
    }
    EXPECT_EQ(cellsOf(model, row), pin.cells);
}

void expectSimulationTablePinRows(const SimulationTableModel&               model,
                                  const std::vector<SimulationTablePinRow>& pins)
{
    ASSERT_EQ(model.rowCount(), static_cast<int>(pins.size()));
    ASSERT_TRUE(model.sortKeys().keys().empty());
    for (int row = 0; row < model.rowCount(); ++row)
    {
        expectSimulationTablePinRow(model, row, pins.at(static_cast<std::size_t>(row)));
    }
}

std::vector<int> simulationTableDocumentOrder(const SimulationTableModel& model)
{
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(model.rowCount()));
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const std::optional<std::size_t> index =
            model.document().getSimulationIndex(*model.simulation(row));
        order.push_back(index.has_value() ? static_cast<int>(*index) : -1);
    }
    return order;
}

std::array<QAction*, 11> simulationPanelPinActions(const SimulationPanel& panel)
{
    return {panel.newSimulationAction(),
            panel.editSimulationAction(),
            panel.cutSimulationAction(),
            panel.copySimulationAction(),
            panel.pasteSimulationAction(),
            panel.runSimulationAction(),
            panel.plotSimulationAction(),
            panel.duplicateSimulationAction(),
            panel.deleteSimulationAction(),
            panel.exportSimulationTableAction(),
            panel.exportSelectedSimulationsAction()};
}

std::string simulationPanelActionName(const QAction& action)
{
    QString name = action.objectName();
    name.chop(QStringLiteral("Action").size());
    return QtStrings::toUtf8(name);
}

std::string simulationPanelActionStates(const SimulationPanel& panel)
{
    std::string text;
    for (const QAction* action : simulationPanelPinActions(panel))
    {
        text += (text.empty() ? "" : " ") + simulationPanelActionName(*action) + '=' +
                (action->isEnabled() ? '1' : '0');
    }
    return text;
}

std::string pinnedSimulationPanelActionStates(const SimulationPanel&             panel,
                                              const std::map<std::string, bool>& pinned)
{
    std::string text;
    for (const QAction* action : simulationPanelPinActions(panel))
    {
        const std::string name = simulationPanelActionName(*action);
        text += (text.empty() ? "" : " ") + name + '=' + pinnedState(pinned, name);
    }
    return text;
}

std::vector<std::string> simulationMenuStateDifferences(const MainMenus&                   menus,
                                                        const std::map<std::string, bool>& pinned,
                                                        bool editItems)
{
    std::vector<std::pair<std::string, const QAction*>> items{
        {"simTableExport", menus.exportSimulationTableAction()}};
    if (editItems)
    {
        items.insert(items.end(), {{"editSimulation", menus.editAction()},
                                   {"cutSimulation", menus.cutAction()},
                                   {"copySimulation", menus.copyAction()},
                                   {"pasteSimulation", menus.pasteAction()},
                                   {"duplicateSimulation", menus.duplicateAction()},
                                   {"deleteSimulation", menus.deleteAction()}});
    }
    std::vector<std::string> differences;
    for (const auto& [name, action] : items)
    {
        const auto found = pinned.find(name);
        if (found == pinned.end() || found->second != action->isEnabled())
        {
            differences.push_back(name);
        }
    }
    return differences;
}

}  // namespace QtRocket::Gui::Test
