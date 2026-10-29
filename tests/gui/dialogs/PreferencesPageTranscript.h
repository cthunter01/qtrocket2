#pragma once

// The preferences dialog's pages as text, item by item in the order of their layouts, in the form
// of the transcript of OpenRocket's pages that PreferencesProbe.java writes
// (PreferencesPagePins.h), and the choice of a combo box item as a user makes it.

#include <span>
#include <string>
#include <string_view>
#include <vector>

class QComboBox;
class QWidget;

namespace QtRocket::Gui::Test
{

/// The items of @p page: each label, check box, combo box, spin box, button, text field,
/// description and separator in its layout's order (a panel's layout in its place), as
/// "LABEL <text>", "CHECK <text> selected=<true|false>", "COMBO selected=<current>
/// items=<a|b|c>", "SPINNER <text> value=<text>" (Java's spinner model gives its text as its
/// value), "BUTTON <text>", "FIELD <text>" (a QLineEdit), each followed by " disabled" when it is
/// disabled and " tip=<tooltip>" when it has one, "TEXT <text>" (a DescriptionArea: the text it
/// was given) and "SEPARATOR" (a horizontal line, QFrame::HLine).
[[nodiscard]] std::vector<std::string> pageTranscript(const QWidget& page);

/// @p lines as strings.
[[nodiscard]] std::vector<std::string> transcriptOf(std::span<const std::string_view> lines);

/// @p lines without the lines that start with one of @p prefixes (OpenRocket's items that
/// QtRocket leaves out until a later milestone, or for good).
[[nodiscard]] std::vector<std::string> transcriptWithout(
    std::span<const std::string_view> lines, std::span<const std::string_view> prefixes);

/// The lines of @p lines that start with one of @p prefixes (OpenRocket's items that QtRocket
/// has, where it leaves out most of a page).
[[nodiscard]] std::vector<std::string> transcriptKeeping(
    std::span<const std::string_view> lines, std::span<const std::string_view> prefixes);

/// One of QtRocket's deliberate differences from a line of OpenRocket's transcript: the line
/// @p openRocket, which QtRocket's page has as @p qtRocket.
struct TranscriptEdit
{
    std::string_view openRocket;
    std::string_view qtRocket;
};

/// @p lines with the line equal to each edit's openRocket replaced by its qtRocket. An edit whose
/// line is not in @p lines exactly once fails the test and changes nothing, so that an edit
/// cannot outlive the pin it was made for.
[[nodiscard]] std::vector<std::string> transcriptEdited(std::vector<std::string>        lines,
                                                        std::span<const TranscriptEdit> edits);

/// Chooses the item @p index of @p comboBox as the user does (its popup, its keys): makes it the
/// current item and tells QComboBox::activated().
void chooseItem(QComboBox& comboBox, int index);

}  // namespace QtRocket::Gui::Test
