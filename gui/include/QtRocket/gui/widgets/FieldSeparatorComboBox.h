#pragma once

#include <string_view>

#include <QChar>
#include <QComboBox>
#include <QObject>
#include <QString>

class QWidget;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The choice of a CSV field separator (OpenRocket's FieldSeparatorComboBox, swing
/// gui/components/FieldSeparatorComboBox.java): an editable combo box with the items ",", ";",
/// "SPACE" and "TAB" (CsvOptionPanel.separator.space and .tab, spaceText() and tabText()), in
/// which the user may type any other separator. The GUI's one separator box: the export options
/// (widgets/CsvOptionPanel), the wind levels' CSV import (CSVImportSettingsDialog) and the
/// aerodynamic lookup dialog use it.
///
/// What it holds is read in two ways:
/// - separatorString() (Java: getSeparatorString()): the separator itself, " " for the SPACE item
///   and "\t" for the TAB item (also when the user typed "SPACE" or "TAB"), otherwise the text
///   as it is (several characters, or none). This is what is written between two fields and what
///   is stored (the preference "ExportFieldSeparator"), never the item's text: OpenRocket's
///   multi-level wind CSV import stores the text "SPACE" or "TAB", of which its next export's box
///   shows the first character, "S" or "T", as the separator (bug 34; the import's fix is
///   CSVImportSettingsDialog's).
/// - separatorChar() (Java: getSeparatorChar()): one character, ' ', '\t', the text's first
///   character, or ',' for an empty text (the lookup dialog's CSV reader).
///
/// The combo box inserts nothing the user types into its list and completes nothing (so that "s"
/// stays "s" and does not become "SPACE"): QComboBox::NoInsert and no completer (Swing's editable
/// JComboBox does neither). Its current text is read live: a typed separator counts at once, where
/// Swing's getSelectedItem() takes it once committed (Return, or the focus leaving the box, as
/// it does before any button acts). currentTextChanged() tells of every change (Java's item
/// listeners hear of a selected item or a committed text).
///
/// Deviations from OpenRocket:
/// - Bug 50: loadFromPreferences() shows the whole stored separator; Java's shows its first
///   character only ("<->" comes back as "<", and a stored "SPACE" as "S").
class FieldSeparatorComboBox final : public QComboBox
{
    Q_OBJECT

public:
    /// The box with its four items, "," chosen.
    explicit FieldSeparatorComboBox(QWidget* parent);

    /// The SPACE item's text (CsvOptionPanel.separator.space).
    [[nodiscard]] static QString spaceText();
    /// The TAB item's text (CsvOptionPanel.separator.tab).
    [[nodiscard]] static QString tabText();

    /// Java's getSeparatorString(): see the class comment.
    [[nodiscard]] QString separatorString() const;

    /// Java's getSeparatorChar(): see the class comment.
    [[nodiscard]] QChar separatorChar() const;

    /// Java's setSeparatorChar(): chooses the item of @p separator (',', ';', ' ' for SPACE, '\t'
    /// for TAB), or shows any other character as the text.
    void setSeparatorChar(QChar separator);

    /// Shows the separator @p separator: the SPACE item for " ", the TAB item for "\t", the item
    /// whose text it is, or else the text as it is (several characters, or none). Then
    /// separatorString() gives @p separator again (but " " for "SPACE" and "\t" for "TAB").
    void setSeparatorString(const QString& separator);

    /// Java's loadFromPreferences(): shows the separator stored in @p preferences under @p key
    /// (@p defaultValue when absent) with setSeparatorString(), whole (bug 50); an empty one
    /// changes nothing.
    void loadFromPreferences(const Preferences& preferences, std::string_view key,
                             std::string_view defaultValue);

    /// Java's loadFromExportPreferences(): loadFromPreferences() of "ExportFieldSeparator"
    /// (PreferenceKeys::kExportFieldSeparator) with the default ",".
    void loadFromExportPreferences(const Preferences& preferences);

private:
    /// Chooses the item @p text, or shows @p text as the box's text when no item has it (Java's
    /// setSelectedItem() of an editable box).
    void selectText(const QString& text);
};

}  // namespace QtRocket::Gui
