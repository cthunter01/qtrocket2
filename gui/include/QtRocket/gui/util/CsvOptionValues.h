#pragma once

#include <vector>

#include <QString>

#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

/// What OpenRocket's CSV option panel (components/CsvOptionPanel: "Format settings" and
/// "Comments") holds, as a value: the table export's options, which its save question asks after
/// the file (UserPrompts::SaveFileRequest::csvOptions), and the export tab's. The defaults are the
/// panel's when nothing is stored (the core's CsvExport::Options has the same).
///
/// The panel reads its values from the preferences and stores them back (storePreferences());
/// here whoever asks loads and stores them (settings/CsvOptionPreferences): "ExportFieldSeparator",
/// "ExportDecimalPlaces", "ExportExponentialNotation" and "ExportCommentCharacter" (the core's
/// PreferenceKeys::kExport...), and each comment option under
/// GuiPreferenceKeys::csvOptionKey(<the panel's class name>, <index>).
struct CsvOptionValues
{
    /// "Field separator string:": the separator itself, as written between two fields ("," ";",
    /// " " for the combo box's SPACE, "\t" for its TAB, or what the user typed: the
    /// FieldSeparatorComboBox's separator string, never its text "SPACE" or "TAB").
    QString fieldSeparator = QStringLiteral(",");
    /// "Decimal places:", 0 to 15 (TextUtil.DEFAULT_DECIMAL_PLACES by default).
    int decimalPlaces = Strings::kDefaultDecimalPlaces;
    /// "Use exponential notation".
    bool exponentialNotation = true;
    /// "Comment character:" ("#", "%", ";" or what the user typed).
    QString commentCharacter = QStringLiteral("#");
    /// The comment options' check boxes, in the panel's order (CsvOptionPanel's includeComments:
    /// the export tab's "Include simulation description", "Include field descriptions" and
    /// "Include flight events"; none for the table export), each ticked by default.
    std::vector<bool> commentOptions;

    [[nodiscard]] bool operator==(const CsvOptionValues& other) const = default;
};

}  // namespace QtRocket::Gui
