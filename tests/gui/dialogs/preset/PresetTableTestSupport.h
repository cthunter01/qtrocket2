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
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/Md5.h"
#include "goldens/GoldenPresets.h"

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

/// The line presetDigestOfView() hashes for @p preset (the probe's viewKey()): its digest, but for
/// a preset whose digest is not the same on every platform "<type>|<manufacturer's simple
/// name>|<part number>" (the identity an .ork file stores for a preset, without the digest).
/// The rule: the digest covers the material's density, which ComponentPresetFactory derives for a
/// preset of hasTranscendentalDensity() (a NOSE_CONE with a HAACK or POWER Shape and a Mass: the
/// six nose cones of internal/semroc-legacy.orc) from the volume of that shape, through acos, sin
/// and pow, whose last bits differ between the platforms' libms (macOS gives BNC-55ACP a density
/// 1 ulp above Linux's and Java's). Every other preset's digest comes from + - * /, sqrt and the
/// files' texts (PresetGolden compares it exactly). The six part numbers differ (PresetGolden pins
/// them), so the lines still tell every row apart. The view orders and the text filter depend on
/// no such value: the only cell that shows one is the Material's, whose text rounds the density to
/// 0.001 g/cm3, none of the six within 3e-5 g/cm3 of a rounding boundary.
[[nodiscard]] inline std::string presetViewKey(const ComponentPreset& preset)
{
    if (QtRocket::Test::hasTranscendentalDensity(preset))
    {
        return std::string(componentPresetTypeName(preset.getType())) + "|" +
               preset.getManufacturer().getSimpleName() + "|" + preset.getPartNo();
    }
    return preset.getDigest();
}

/// "rows <n> md5 <hex>": the number of view rows and the MD5 of their presets' presetViewKey()
/// lines (the digests but for six presets) in view order, each followed by '\n' (the probe's
/// digestOfView()).
[[nodiscard]] inline std::string presetDigestOfView(const ComponentPresetFilterModel& filter)
{
    std::string text;
    for (int row = 0; row < filter.rowCount(); ++row)
    {
        text += presetViewKey(*filter.tableModel().preset(filter.modelRow(row))) + '\n';
    }
    const std::span<const std::byte> bytes = std::as_bytes(std::span(text));
    return "rows " + std::to_string(filter.rowCount()) + " md5 " + toHex(md5(bytes));
}

}  // namespace QtRocket::Gui::Test
