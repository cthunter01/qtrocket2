#pragma once

// The lines in which the Java probes of the configuration panels (BodyPanelProbe and
// LugPanelProbe of tools/openrocket-panel-pins, run on OpenRocket's Swing panels under Xvfb) print
// the widgets of a tab, made from QtRocket's widgets in the same form: what
// body_panel_golden_tests.cpp and lug_panel_golden_tests.cpp compare with the pinned transcripts.

#include <span>
#include <string>
#include <string_view>
#include <vector>

class QString;
class QWidget;

namespace QtRocket::Gui
{
class BoolCheckBox;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// @p text in quotes, as the probes print a String (q()).
[[nodiscard]] std::string quoted(const QString& text);

/// @p text in quotes, or "null" when it is empty: a probe's q() of a tooltip, which Swing keeps
/// as null when none was set (Qt has no null text).
[[nodiscard]] std::string quotedOrNull(const QString& text);

/// The probes' checkLine(): "check <property> "<text>" checked=<b> enabled=<b> tip=<tooltip>".
[[nodiscard]] std::string checkLine(const BoolCheckBox& box);

/// What the probes' walk() prints of the widgets of a tab.
struct ProbeLineOptions
{
    /// Whether the labels are printed (a probe leaves them out after an edit).
    bool labels = true;
    /// Whether a label's line ends in " tip=<tooltip>" (LugPanelProbe's; BodyPanelProbe has none).
    bool labelTips = false;
};

/// The line of one widget as the probes' walk() prints it, empty for a widget they do not print:
/// "spinner <property> "<text>" enabled=<b>" (a UnitSpinBox or an IntSpinBox), "unit <property>
/// "<unit>"", "slider <property> <position> enabled=<b>", checkLine(), "combo "<text>"
/// enabled=<b>" and "label "<text>" enabled=<b>" (a label with text).
[[nodiscard]] std::string widgetLine(const QWidget& widget, ProbeLineOptions options);

/// Adds the lines of the widgets under @p tab to @p lines, leaving out those of a MaterialPanel (as
/// the probes do).
void addTabWidgetLines(const QWidget& tab, ProbeLineOptions options,
                       std::vector<std::string>& lines);

/// The differences between QtRocket's lines @p actual and the pinned @p expected, one per line
/// ("-" pinned, "+" QtRocket's), by position; empty when they are the same.
[[nodiscard]] std::string pinnedLineDifferences(const std::vector<std::string>&   actual,
                                                std::span<const std::string_view> expected);

}  // namespace QtRocket::Gui::Test
