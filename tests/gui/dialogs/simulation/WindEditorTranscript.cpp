#include "dialogs/simulation/WindEditorTranscript.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/plot/WindProfilePlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui::Test
{

namespace
{

using Model = MultiLevelWindTableModel;

}  // namespace

std::vector<std::string> windTableHeaderLines(const MultiLevelWindTableModel& table)
{
    std::vector<std::string> lines;
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        std::string line =
            "HEADER " + std::to_string(column) + " [" +
            QtStrings::toUtf8(
                table.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString()) +
            "]";
        if (const QString tip =
                table.headerData(column, Qt::Horizontal, Qt::ToolTipRole).toString();
            !tip.isEmpty())
        {
            line += " tip=" + QtStrings::toUtf8(tip);
        }
        line += " unit=[" +
                (column < Model::kNumberColumnCount ? table.columnUnit(column).getUnit()
                                                    : std::string("none")) +
                "]";
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> windTableRowLines(const MultiLevelWindTableModel&     table,
                                           const MultiLevelPinkNoiseWindModel& model)
{
    std::vector<std::string> lines;
    for (int row = 0; row < table.rowCount(); ++row)
    {
        std::string line = "ROW " + std::to_string(row);
        for (int column = 0; column < Model::kNumberColumnCount; ++column)
        {
            line += " [" + QtStrings::toUtf8(table.cellText(row, column)) + "]";
        }
        const bool deletable =
            table.flags(table.index(row, Model::kDeleteColumn)).testFlag(Qt::ItemIsEnabled);
        line += " intensity=[" + QtStrings::toUtf8(table.cellText(row, Model::kIntensityColumn)) +
                "] delete=" + (deletable ? "enabled" : "disabled");
        lines.push_back(line);
    }
    lines.push_back(windModelLine(model));
    return lines;
}

std::string windModelLine(const MultiLevelPinkNoiseWindModel& model)
{
    std::string line = "MODEL";
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* const level : model.getLevels())
    {
        line += " " + Strings::javaDoubleToString(level->getAltitude());
    }
    return line;
}

std::vector<std::string> windTipLines(std::string_view                    title,
                                      const MultiLevelPinkNoiseWindModel& model,
                                      const WindProfilePlot::Units&       units)
{
    std::vector<std::string> lines;
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* const level : model.getLevels())
    {
        lines.push_back("TIP " + std::string(title) + " " +
                        Strings::javaDoubleToString(level->getAltitude()) + " [" +
                        QtStrings::toUtf8(WindProfilePlot::toolTipText(
                            *level, units, model.getAltitudeReference())) +
                        "]");
    }
    return lines;
}

std::vector<std::string> windPinLines(std::span<const std::string_view> pins)
{
    return {pins.begin(), pins.end()};
}

std::vector<std::string> windPinLinesWith(std::span<const std::string_view> pins,
                                          std::span<const std::string_view> prefixes)
{
    std::vector<std::string> lines;
    for (const std::string_view pin : pins)
    {
        if (std::ranges::any_of(prefixes,
                                [pin](std::string_view prefix) { return pin.starts_with(prefix); }))
        {
            lines.emplace_back(pin);
        }
    }
    return lines;
}

}  // namespace QtRocket::Gui::Test
