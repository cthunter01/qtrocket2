#pragma once

// The wind editor's table and profile as text, in the form of the probe
// tools/openrocket-probes/wind-editor (WindEditorPins.h): its HEADER, ROW, MODEL and TIP lines.

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/gui/plot/WindProfilePlot.h"

namespace QtRocket
{
class MultiLevelPinkNoiseWindModel;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class MultiLevelWindTableModel;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The probe's HEADER lines of @p table, one per column: "HEADER <column> [<name>]", then
/// " tip=<tooltip>" when the column has one, then " unit=[<unit>]" ("none" for a column without
/// a unit).
[[nodiscard]] std::vector<std::string> windTableHeaderLines(const MultiLevelWindTableModel& table);

/// The probe's ROW lines of @p table, one per row: "ROW <row> [<altitude>] [<speed>]
/// [<direction>] [<deviation>] [<turbulence>] intensity=[<description>] delete=<enabled|disabled>",
/// then its MODEL line of @p model (windModelLine()).
[[nodiscard]] std::vector<std::string> windTableRowLines(const MultiLevelWindTableModel&     table,
                                                         const MultiLevelPinkNoiseWindModel& model);

/// The probe's MODEL line: "MODEL" and the altitude of each level of @p model in the model's
/// order, as Java's Double.toString() writes it.
[[nodiscard]] std::string windModelLine(const MultiLevelPinkNoiseWindModel& model);

/// The probe's TIP lines of @p model's levels in the model's order: "TIP <title> <altitude as
/// Double.toString()> [<WindProfilePlot::toolTipText() in @p units>]".
[[nodiscard]] std::vector<std::string> windTipLines(std::string_view                    title,
                                                    const MultiLevelPinkNoiseWindModel& model,
                                                    const WindProfilePlot::Units&       units);

/// @p pins as strings.
[[nodiscard]] std::vector<std::string> windPinLines(std::span<const std::string_view> pins);

/// The lines of @p pins that start with one of @p prefixes ("ROW "), as strings.
[[nodiscard]] std::vector<std::string> windPinLinesWith(std::span<const std::string_view> pins,
                                                        std::span<const std::string_view> prefixes);

}  // namespace QtRocket::Gui::Test
