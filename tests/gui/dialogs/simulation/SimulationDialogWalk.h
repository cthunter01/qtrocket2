#pragma once

// The simulation dialogs' panels as text, widget by widget in the order of their layouts, in the
// form of the walk of OpenRocket's panels that the probe
// tools/openrocket-probes/simulation-conditions prints (its WALK lines, without "WALK "), which is
// the M4 sim-config scout's dialog walk with SEPARATOR added (and the probe
// tools/openrocket-probes/csv-import-settings's, which prints FIELD, COMBO and TEXTAREA as the
// scout's walk); and a number row's state as its STEP lines print it.

#include <span>
#include <string>
#include <string_view>
#include <vector>

class QWidget;

namespace QtRocket::Gui::ConfigFields
{
struct NumberWidgets;
}  // namespace QtRocket::Gui::ConfigFields

namespace QtRocket::Gui::Test
{

/// The widgets of @p root that would be visible were it shown (a hidden card of a QStackedWidget
/// is left out, as Java's walk leaves out a hidden card), in the order of their layouts (a
/// container widget's layout in its place): a titled QGroupBox as "BORDER <title>" with the lines
/// of its widgets indented by two spaces; a UnitSpinBox (any QAbstractSpinBox) as
/// "SPINNER <text>"; a QRadioButton as "RADIO <text> selected=<true|false>"; a QCheckBox as
/// "CHECK <text> selected=<true|false>"; a UnitSelector as "LABEL <unit>" (Java's UnitSelector is
/// a label); another button as "BUTTON <text>", with " icon" when it has one; a QSlider as
/// "SLIDER <min>..<max> at <value>"; a horizontal line (QFrame::HLine) as "SEPARATOR"; a QLabel
/// as "LABEL <text>", or, word wrapped (Java's read-only JTextArea), as "TEXTAREA <text>"; a
/// QComboBox as "COMBO selected=<text> items=<item>|<item>..." with " editable" when it is; a
/// QLineEdit (not a spin box's or a combo box's) as "FIELD <text>"; a QScrollArea's widget in its
/// place (not its scroll bars); a QLabel with a picture with " icon" after its text (Java's
/// JLabel with an icon); a QPlainTextEdit as "TEXTAREA <text>" (Java's JTextArea); a QTabWidget
/// as "TABS [<title>] [<title>]... selected=<index>", then for each page "TAB <title>" and the
/// lines of the page (shown or not) indented by two spaces (the probe
/// tools/openrocket-probes/simulation-options's walk of a JTabbedPane). Each line but BORDER and
/// SEPARATOR ends with " disabled" when the widget is disabled and " tip=<tooltip>" when it has a
/// tooltip, line breaks written as "\n".
/// @p root itself is walked too (a titled box is a BORDER).
[[nodiscard]] std::vector<std::string> dialogWalk(const QWidget& root);

/// A number row's state as the probe's STEP lines print it: "<name>=[<spin box text>]", then
/// " at <slider position>" when the row has a slider, " disabled" when the spin box is disabled,
/// " outline=WARNING" or " outline=ERROR" when it is outlined (FieldValidator::outlineOf()), and
/// " tip=<the spin box's tooltip>" when it has one.
[[nodiscard]] std::string numberRowState(std::string_view                   name,
                                         const ConfigFields::NumberWidgets& row);

/// @p lines with every @p from replaced by @p to; the number of lines that held it must be
/// @p count (a test failure otherwise), so that an edit of a pin cannot outlive the pin it was
/// made for.
[[nodiscard]] std::vector<std::string> replacedIn(std::vector<std::string> lines,
                                                  std::string_view from, std::string_view to,
                                                  int count);

/// @p lines without the one line @p dropped (a test failure unless it is there exactly once).
[[nodiscard]] std::vector<std::string> without(std::vector<std::string> lines,
                                               std::string_view         dropped);

/// Expects @p actual to be @p expected, line by line: a test failure for each line that differs
/// (with its index) and for a different number of lines, where EXPECT_EQ of the two vectors would
/// print only the first 32 lines of each.
void expectSameLines(std::span<const std::string> actual, std::span<const std::string> expected);

/// @p lines without their prefix @p prefix ("WALK "), for the lines that have it; the others
/// are left out.
[[nodiscard]] std::vector<std::string> linesWithPrefix(std::span<const std::string_view> lines,
                                                       std::string_view                  prefix);

}  // namespace QtRocket::Gui::Test
