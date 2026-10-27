#pragma once

// The transcripts of the parts library's table tests (ComponentPresetTableModelTests.cpp,
// ComponentPresetFilterModelTests.cpp, ComponentPresetTableTests.cpp), written as
// tools/openrocket-gui-pins/preset-chooser/probe/ComponentPresetTableProbe.java prints them, so
// that its lines in hand-pinned.txt compare as they are.

#include <cstddef>
#include <span>
#include <string>

#include <QAction>
#include <QMenu>
#include <QString>

#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/Md5.h"

namespace QtRocket::Gui::Test
{

/// The view's rows as their model rows, separated by spaces (the probe's viewRows()).
[[nodiscard]] inline std::string presetViewRows(const ComponentPresetFilterModel& filter)
{
    std::string line;
    for (int row = 0; row < filter.rowCount(); ++row)
    {
        line += (row == 0 ? "" : " ") + std::to_string(filter.modelRow(row));
    }
    return line;
}

/// The sort keys as "[2 asc, 5 desc]" (the probe's sortKeys()).
[[nodiscard]] inline std::string presetSortKeys(const ComponentPresetFilterModel& filter)
{
    std::string line;
    for (const TableSortKeys::SortKey& key : filter.sortKeys().keys())
    {
        line += (line.empty() ? "" : ", ") + std::to_string(key.column) +
                (key.order == TableSortKeys::SortOrder::ASCENDING ? " asc" : " desc");
    }
    return "[" + line + "]";
}

/// The texts of the cells of @p row that are not empty, as "<column>=<text>" separated by " | "
/// (the probe's cells()).
[[nodiscard]] inline std::string presetCellsLine(const ComponentPresetTableModel& model, int row)
{
    std::string line;
    for (int column = 0; column < ComponentPresetTableModel::kColumnCount; ++column)
    {
        const std::string text = QtStrings::toUtf8(model.cellText(row, column));
        if (!text.empty())
        {
            line += (line.empty() ? "" : " | ") + std::to_string(column) + "=" + text;
        }
    }
    return line;
}

/// The shown columns in view order as "<header> [<model column>]" separated by " | " (the probe's
/// viewColumns()).
[[nodiscard]] inline std::string presetViewColumns(const ComponentPresetTable& table)
{
    std::string line;
    for (const int column : table.visibleColumns())
    {
        line += (line.empty() ? "" : " | ") +
                QtStrings::toUtf8(ComponentPresetTableModel::columnName(column)) + " [" +
                std::to_string(column) + "]";
    }
    return line;
}

/// The items of @p menu as the probe's menuItems() writes them: "[x]" or "[ ]" before a check box
/// item, "|" for a separator, "[Units: mm *cm ...]" for the units menu (its checked item starred).
[[nodiscard]] inline std::string presetMenuItems(const QMenu& menu)
{
    std::string line;
    for (const QAction* action : menu.actions())
    {
        if (!line.empty())
        {
            line += ' ';
        }
        if (action->isSeparator())
        {
            line += '|';
        }
        else if (action->menu() != nullptr)
        {
            std::string units;
            for (const QAction* unit : action->menu()->actions())
            {
                units += std::string(" ") + (unit->isChecked() ? "*" : "") +
                         QtStrings::toUtf8(unit->text());
            }
            line += "[" + QtStrings::toUtf8(action->text()) + ":" + units + "]";
        }
        else if (action->isCheckable())
        {
            line += (action->isChecked() ? "[x]" : "[ ]") + QtStrings::toUtf8(action->text());
        }
        else
        {
            line += QtStrings::toUtf8(action->text());
        }
    }
    return line;
}

/// The action of @p menu with the text @p text, nullptr for none (the probe's item()).
[[nodiscard]] inline QAction* presetMenuItem(const QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// "rows <n> md5 <hex>": the number of view rows and the MD5 of their presets' digests in view
/// order, each followed by '\n' (the probe's digestOfView()).
[[nodiscard]] inline std::string presetDigestOfView(const ComponentPresetFilterModel& filter)
{
    std::string text;
    for (int row = 0; row < filter.rowCount(); ++row)
    {
        text += filter.tableModel().preset(filter.modelRow(row))->getDigest() + '\n';
    }
    const std::span<const std::byte> bytes = std::as_bytes(std::span(text));
    return "rows " + std::to_string(filter.rowCount()) + " md5 " + toHex(md5(bytes));
}

}  // namespace QtRocket::Gui::Test
