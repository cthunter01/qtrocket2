#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/CsvOptionValues.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QSpinBox;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

class FieldSeparatorComboBox;

/// The options of a CSV file being written (OpenRocket's CsvOptionPanel, swing
/// gui/components/CsvOptionPanel.java): two titled groups, one above the other (Java's
/// placeOnNewRows, the default) or side by side (Arrangement::SIDE_BY_SIDE):
/// - "Format settings": "Field separator string:" with a FieldSeparatorComboBox (",", ";",
///   SPACE, TAB or typed), "Decimal places:" with a spin box from 0 to 15, and the check box
///   "Use exponential notation", each with OpenRocket's tooltip (the separator's is HTML, on the
///   label and the box alike);
/// - "Comments": a check box for each comment option the owner gives (the simulation data
///   export: "Include simulation description", "Include field descriptions", "Include flight
///   events"; the simulation table's export: none), then "Comment character:" with an editable
///   combo box of "#", "%" and ";", which inserts and completes nothing typed, as the separator's.
///
/// The values: values() and setValues() (util/CsvOptionValues: the separator string, never the
/// text "SPACE" or "TAB"), or straight from and to the preferences of the panel's base class name
/// (Java's getSimpleName() of the class given to it, "SimulationExportPanel") with
/// loadPreferences() and storePreferences() (settings/CsvOptionPreferences, with its keys and its
/// fixes of OpenRocket's bugs 50 and 51). Java's constructor reads the preferences itself; this one
/// starts from the defaults (CsvOptionPreferences::load() of an empty store: ",", 3 decimal
/// places, exponential notation, "#", every comment option ticked), so that the owner calls
/// loadPreferences(), or setValues() with the values of a question (the table export's options,
/// UserPrompts::SaveFileRequest::csvOptions).
///
/// The object names are Java's fields: "fieldSeparator", "decimalPlacesSpinner",
/// "exponentialNotationCheckbox", "options" (each comment option's check box) and
/// "commentCharacter".
class CsvOptionPanel final : public QWidget
{
    Q_OBJECT

public:
    /// A comment option: its check box's text and tooltip (Java: every second string of
    /// includeComments).
    struct CommentOption
    {
        QString text;
        QString toolTip;
    };

    /// Where the two groups go.
    enum class Arrangement
    {
        /// The comments below the format settings (Java: placeOnNewRows true, the default).
        ON_NEW_ROWS,
        /// The comments right of the format settings (placeOnNewRows false).
        SIDE_BY_SIDE,
    };

    /// The panel of the class @p baseClassName (the preferences' "csvOptions.<baseClassName>.<i>"
    /// of its comment options) with the comment options @p includeComments, in their order.
    CsvOptionPanel(std::string baseClassName, const std::vector<CommentOption>& includeComments,
                   Arrangement arrangement, QWidget* parent);

    /// The base class name given to the constructor.
    [[nodiscard]] const std::string& baseClassName() const noexcept { return m_baseClassName; }

    /// The number of comment options.
    [[nodiscard]] std::size_t optionCount() const noexcept { return m_options.size(); }

    /// Java's getFieldSeparator(): the separator string
    /// (FieldSeparatorComboBox::separatorString()).
    [[nodiscard]] QString fieldSeparator() const;
    /// Java's getDecimalPlaces().
    [[nodiscard]] int decimalPlaces() const;
    /// Java's isExponentialNotation().
    [[nodiscard]] bool isExponentialNotation() const;
    /// Java's getCommentCharacter(): the comment box's text as it is.
    [[nodiscard]] QString commentCharacter() const;
    /// Java's getSelectionOption(@p index): whether the comment option @p index is ticked.
    /// @throws BugError when there is no such option
    [[nodiscard]] bool selectionOption(std::size_t index) const;

    /// Everything the panel shows, as values.
    [[nodiscard]] CsvOptionValues values() const;

    /// Shows @p values: the separator as FieldSeparatorComboBox::setSeparatorString() shows it,
    /// the decimal places within 0 to 15, and the comment character as the item whose text it is
    /// or as the box's text.
    /// @throws BugError when @p values does not have one comment option for each of the panel's
    void setValues(const CsvOptionValues& values);

    /// setValues() of CsvOptionPreferences::load() for the panel's base class name and options
    /// (what Java's constructor reads).
    void loadPreferences(const Preferences& preferences);

    /// Java's storePreferences(): CsvOptionPreferences::store() of values() for the panel's base
    /// class name.
    void storePreferences(Preferences& preferences) const;

    /// The widgets, for the tests.
    [[nodiscard]] QGroupBox*              formatSettingsGroup() const noexcept { return m_format; }
    [[nodiscard]] QGroupBox*              commentsGroup() const noexcept { return m_comments; }
    [[nodiscard]] FieldSeparatorComboBox* fieldSeparatorComboBox() const noexcept
    {
        return m_fieldSeparator;
    }
    [[nodiscard]] QSpinBox*  decimalPlacesSpinBox() const noexcept { return m_decimalPlaces; }
    [[nodiscard]] QCheckBox* exponentialNotationCheckBox() const noexcept
    {
        return m_exponentialNotation;
    }
    /// The check box of the comment option @p index.
    /// @throws BugError when there is no such option
    [[nodiscard]] QCheckBox* optionCheckBox(std::size_t index) const;
    [[nodiscard]] QComboBox* commentCharacterComboBox() const noexcept
    {
        return m_commentCharacter;
    }

private:
    /// The "Format settings" group, made with its fields.
    [[nodiscard]] QGroupBox* makeFormatSettings();
    /// The "Comments" group, made with a check box for each of @p includeComments.
    [[nodiscard]] QGroupBox* makeComments(const std::vector<CommentOption>& includeComments);

    std::string             m_baseClassName;
    FieldSeparatorComboBox* m_fieldSeparator{nullptr};
    QSpinBox*               m_decimalPlaces{nullptr};
    QCheckBox*              m_exponentialNotation{nullptr};
    std::vector<QCheckBox*> m_options;
    QComboBox*              m_commentCharacter{nullptr};
    QGroupBox*              m_format{nullptr};
    QGroupBox*              m_comments{nullptr};
};

}  // namespace QtRocket::Gui
