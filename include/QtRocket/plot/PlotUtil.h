#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{

class FlightData;

/// The text helpers of OpenRocket's plots (swing gui/plot/Util): the names of the branches of a
/// simulation as the plot dialog's and the export panel's stage boxes list them, and the plain
/// form of a name that may hold HTML. (Util's colours are the GUI's PlotColors, its axis choice
/// is the GUI's.)
///
/// Placement: OpenRocket keeps Util in its swing package; these are in the core's plot model, so
/// that the plot dialog (dialogs/plot/) and the export panel (dialogs/simulation/) share them
/// (the M4 scouting's critic, C13).
namespace PlotUtil
{

/// Util.formatHTMLString(): @p input without its HTML tags (Strings::removeHtmlTags(): every
/// "<...>" run removed, a '<' without a closing '>' kept).
[[nodiscard]] std::string formatHtmlString(std::string_view input);

/// Util.generateSeriesLabels(): the label of each branch, in order: its name without HTML tags
/// (formatHtmlString()), and a name that several branches have numbered "name(1)", "name(2)", ...
/// in their order, without a space ({"Sustainer", "Booster stage", "Booster stage"} gives
/// {"Sustainer", "Booster stage(1)", "Booster stage(2)"}). The names are taken in turn, each with
/// the labels as they are by then, so a label "A(1)" made for one name counts against a branch
/// named "A(1)" ({"A(1)", "A", "A"} gives {"A(1)", "A(1)", "A(2)"}, as in Java).
///
/// Deviation from OpenRocket: when a name's duplicates are not all after it (a label numbered
/// for an earlier name equals a later branch's name: {"A", "A(1)", "A"} or {"A", "A", "A(1)"}),
/// Java's numbering runs past the last branch and throws IndexOutOfBoundsException, so its plot
/// dialog and export panel cannot open for such stage names (an OpenRocket crash, found while
/// porting). Here the numbering stops at the last branch: {"A(1)", "A(1)(1)", "A(2)"} and
/// {"A(1)", "A(2)", "A(1)(1)"}.
[[nodiscard]] std::vector<std::string> generateSeriesLabels(std::span<const std::string> names);

/// generateSeriesLabels() of the names of the branches of @p data.
[[nodiscard]] std::vector<std::string> generateSeriesLabels(const FlightData& data);

}  // namespace PlotUtil

}  // namespace QtRocket
