#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <QColor>
#include <QDialog>
#include <QObject>
#include <QString>
#include <QStringList>

#include "QtRocket/gui/util/UserPrompts.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QTextEdit;
class QWidget;

namespace QtRocket
{
class Preferences;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class FieldSeparatorComboBox;
class UnitSelector;

/// The settings of a wind CSV import (OpenRocket's CSVImportSettingsDialog, swing gui/simulation/):
/// "Wind Data Import Settings", how the wind editor's "Import levels" (MultiLevelWindEditDialog::
/// importLevels()) reads a CSV file. The application shows it for UserPrompts::
/// askWindCsvImportSettings() (main/WidgetUserPrompts), modal for the whole application (Java:
/// APPLICATION_MODAL), 600 x 550 pixels to begin with.
///
/// What it shows (Java's layout, in a scroll area): the description; a box "CSV Settings" with a
/// help text (1 point smaller), "CSV file has a header row" (hasHeaderCheckBox), a grid of the
/// data types Altitude, Speed, Direction and "Standard Deviation (optional)" (Row), each with its
/// column (a name field, altitudeColumnField ..., with a header row; an index spin box from 0,
/// altitudeColumnSpinner ..., without: the column's header "Column Name" or "Column Index",
/// columnTypeLabel, switches with the box) and the unit the file's values are in (a UnitSelector
/// over the row's unit group, altitudeUnitSelector ...), then a separator line and "Field
/// Separator" (separatorComboBox, a FieldSeparatorComboBox); a box "CSV Example Contents Preview"
/// with its help text (1 point smaller) and the preview (previewTextPane): what the first lines of
/// such a file look like (previewParts()), in a monospaced font, black, gray (the optional parts)
/// and dark green (the column mapping) on white, as Java's text pane in every theme. Below the
/// scroll area, at the right, Cancel and OK, the default button; Escape and the close button
/// cancel.
///
/// It starts with the settings it is given (the stored ones: storedSettings()) and answers the
/// settings chosen (settings()). Its fields are validated as they change, as Java's: with a header
/// row, an empty (or blank) name of a required column and two equal names (trimmed; the standard
/// deviation's only when it has one) have the error outline (FieldValidator::setOutline());
/// without, two equal indices do (the standard deviation's index counts only while its name is not
/// blank, though the import is always given it: Java's). OK refuses them with an error message
/// through @p prompts (UserPrompts::showMessage(): "Incorrect Data Input" and Java's lines) and
/// stays open; otherwise it accepts.
///
/// Deviations from OpenRocket:
/// - OpenRocket bug 34: the field separator the dialog answers is the separator itself (" " for
///   the SPACE item, "\t" for the TAB item: FieldSeparatorComboBox::separatorString()); Java's
///   getSeparator() gives the item's text "SPACE" or "TAB", at which the import then splits (and
///   fails: the probe's 'altitude column "altitude" not found in header row.'), and which the wind
///   editor stores as the export's separator (storeSettings() stores the separator).
/// - The separator box shows the stored separator whole, "SPACE" and "TAB" as their items (bug 50,
///   FieldSeparatorComboBox); Java shows the first character, so that a separator "SPACE" stored by
///   bug 34 comes back as "S".
/// - A stored negative column index starts the spin box at 0; Java's SpinnerNumberModel throws
///   IllegalArgumentException, so that the dialog cannot be opened (the import neither) until the
///   preferences are reset.
/// - The index spin boxes and the separator box are read as they are typed (the preview and the
///   outlines follow at once); Java's spinner and editable combo box commit a typed value on Enter
///   or when they lose the focus.
/// - Layout: QGroupBoxes for Java's titled borders, a QGridLayout for MigLayout's grid (the grid's
///   columns at their preferred widths, as Java's, the label columns at least 120 pixels), word
///   wrapped labels for Java's read-only text areas, a QStackedWidget for each row's CardLayout,
///   a read-only QTextEdit as tall as its text for the text pane.
class CsvImportSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    using Settings = UserPrompts::WindCsvImportSettings;
    using Column   = UserPrompts::WindCsvColumn;

    /// The data types, the grid's rows, in their order.
    enum class Row
    {
        ALTITUDE,
        SPEED,
        DIRECTION,
        STANDARD_DEVIATION,
    };
    static constexpr std::size_t kRowCount = 4;

    /// The colours of the preview's parts (Java's text pane styles).
    enum class PreviewStyle
    {
        /// The required columns and the values: black (normalStyle).
        NORMAL,
        /// The comments, the standard deviation's column and values: gray (optionalStyle).
        OPTIONAL,
        /// The column mapping of a file without a header row: dark green, (0, 100, 0)
        /// (indexStyle).
        INDEX,
    };

    /// A part of the preview's text, in one colour; "\n" ends a line.
    struct PreviewPart
    {
        QString      text;
        PreviewStyle style = PreviewStyle::NORMAL;

        [[nodiscard]] bool operator==(const PreviewPart& other) const = default;
    };

    /// The size the dialog opens with (Java: setPreferredSize(600, 550) and pack()).
    static constexpr int kWidth  = 600;
    static constexpr int kHeight = 550;
    /// The width of the label columns, "Data Type" and "Unit" (Java: "width 120lp").
    static constexpr int kLabelColumnWidth = 120;
    /// The preview's font size, points (Java: new Font("Monospaced", Font.PLAIN, 12)), and its
    /// margin, pixels (setMargin(new Insets(5, 5, 5, 5))).
    static constexpr int kPreviewFontSize = 12;
    static constexpr int kPreviewMargin   = 5;
    /// The scroll area's step, pixels (Java: setUnitIncrement(16)).
    static constexpr int kScrollStep = 16;

    /// The dialog (not shown), starting with @p initial: its header row, its columns' names,
    /// indices and units (a null unit leaves the row's selector at its group's default unit) and
    /// its separator (shown as FieldSeparatorComboBox::setSeparatorString() shows it; an empty one
    /// leaves ",", as Java's loadFromExportPreferences()); asking its error messages through
    /// @p prompts, which must outlive it.
    /// @throws BugError when a unit of @p initial is not one of its row's group
    CsvImportSettingsDialog(const Settings& initial, UserPrompts& prompts,
                            QWidget* parent = nullptr);
    ~CsvImportSettingsDialog() override;
    CsvImportSettingsDialog(const CsvImportSettingsDialog&)            = delete;
    CsvImportSettingsDialog& operator=(const CsvImportSettingsDialog&) = delete;
    CsvImportSettingsDialog(CsvImportSettingsDialog&&)                 = delete;
    CsvImportSettingsDialog& operator=(CsvImportSettingsDialog&&)      = delete;

    /// The settings shown (Java's getters): "CSV file has a header row"; each row's name
    /// (trimmed, as Java's String.trim(): the characters at or below U+0020), index and unit; the
    /// separator itself (bug 34, see the class comment).
    [[nodiscard]] Settings settings() const;

    /// Java's hasHeader().
    [[nodiscard]] bool hasHeader() const;

    /// The column of @p row as the import is given it (Java: getAltitudeColumn() ...): its name
    /// with a header row, its index as text without (the standard deviation's too, whatever its
    /// name).
    [[nodiscard]] QString importColumn(Row row) const;

    /// importColumn() of @p column in settings with a header row (@p hasHeader) or without.
    [[nodiscard]] static QString importColumn(const Column& column, bool hasHeader);

    /// The column of @p settings in @p row.
    [[nodiscard]] static const Column& columnOf(const Settings& settings, Row row);

    /// The unit group of @p row's values: DISTANCE for the altitude, WINDSPEED for the speed and
    /// the standard deviation, ANGLE for the direction (Java's DoubleModels).
    [[nodiscard]] static const UnitGroup& unitGroupOf(Row row);

    /// The settings stored in @p preferences (what Java's dialog starts with): the 12
    /// MultiLevelWindCsvImport... preferences and the separator stored as the export's
    /// ("ExportFieldSeparator", "," when none or an empty one is stored: Java's
    /// FieldSeparatorComboBox.loadFromExportPreferences()), as it is stored.
    [[nodiscard]] static Settings storedSettings(const Preferences& preferences);

    /// Stores @p settings in @p preferences, as the wind editor does once the dialog is accepted
    /// (MultiLevelWindEditDialog.importLevels()): the 12 MultiLevelWindCsvImport... preferences
    /// and the separator as the export's, the separator itself (OpenRocket bug 34: Java stores the
    /// SPACE or TAB item's text).
    /// @throws BugError when a column of @p settings has no unit
    static void storeSettings(Preferences& preferences, const Settings& settings);

    /// How the preview shows the separator chosen as @p comboText (the separator box's text): the
    /// SPACE item as "\s", the TAB item as "\t" (a backslash and a letter), any other text as it
    /// is.
    [[nodiscard]] static QString displayedSeparator(const QString& comboText);

    /// The preview of a file read with @p settings (Java's updatePreview()): the comment line
    /// "# You can add comments with a hash" (OPTIONAL); with a header row the header line, the
    /// names (importColumn()) joined by @p separator (NORMAL; the standard deviation's, after the
    /// separator, OPTIONAL and only when it has a name), without one "# Column mapping: "
    /// (OPTIONAL), "<i>=Altitude, <j>=Speed, <k>=Direction" (INDEX) and ", <l>=StdDev" (OPTIONAL);
    /// then four rows of the altitudes 100, 200, 300 and 400 m, the speeds 5, 7.5, 10 and 12.5 m/s,
    /// the directions pi/2, 5 pi/12, 7 pi/12 and 3 pi/4 and the standard deviations 1, 1.5, 2 and
    /// 2.5 m/s, each in its row's unit (Unit::toString()), the standard deviation as its name; and
    /// the ellipsis U+2026 (NORMAL). @p separator is displayedSeparator()'s.
    [[nodiscard]] static std::vector<PreviewPart> previewParts(const Settings& settings,
                                                               const QString&  separator);

    /// The text colour of @p style (Java: Color.BLACK, Color.GRAY, new Color(0, 100, 0)).
    [[nodiscard]] static QColor previewColor(PreviewStyle style);

    /// The preview shown now: previewParts() of settings() and the separator box's text.
    [[nodiscard]] std::vector<PreviewPart> preview() const;

    /// OK's action (Java's ActionListener of okButton): validates the fields; with a header row
    /// and an invalid name (hasInvalidColumnNames()) shows "Incorrect Data Input" with
    /// columnNameErrorLines(), without a header row and with two equal indices
    /// (hasDuplicateColumnIndices()) "Incorrect Data Input" with "Duplicate column indices found.
    /// Please ensure that each column index is unique.", as errors through the prompts, and stays
    /// open; otherwise accepts the dialog.
    void tryAccept();

    /// Whether a name field has the error outline (Java's private hasInvalidColumnNames()).
    [[nodiscard]] bool hasInvalidColumnNames() const;
    /// The lines of the message about invalid names (getColumnNameValidationErrorMessage()):
    /// "Missing required column name(s). Please fill in the highlighted fields." when a required
    /// name is blank, "Duplicate column names found. Please ensure that each column name is
    /// unique." when two names are equal, or both (Java joins them with a line break).
    [[nodiscard]] QStringList columnNameErrorLines() const;
    /// Validates the indices and tells whether a spin box has the error outline (Java's private
    /// hasDuplicateColumnIndices()).
    [[nodiscard]] bool hasDuplicateColumnIndices();

    [[nodiscard]] QScrollArea&    scrollArea() const noexcept { return *m_scrollArea; }
    [[nodiscard]] QLabel&         description() const noexcept { return *m_description; }
    [[nodiscard]] QGroupBox&      csvSettingsBox() const noexcept { return *m_csvSettings; }
    [[nodiscard]] QLabel&         columnMappingHelp() const noexcept { return *m_mappingHelp; }
    [[nodiscard]] QCheckBox&      hasHeaderCheckBox() const noexcept { return *m_hasHeader; }
    [[nodiscard]] QLabel&         dataTypeLabel() const noexcept { return *m_dataTypeLabel; }
    [[nodiscard]] QLabel&         columnTypeLabel() const noexcept { return *m_columnTypeLabel; }
    [[nodiscard]] QLabel&         unitLabel() const noexcept { return *m_unitLabel; }
    [[nodiscard]] QLabel&         rowLabel(Row row) const;
    [[nodiscard]] QStackedWidget& columnPanel(Row row) const;
    [[nodiscard]] QLineEdit&      columnField(Row row) const;
    [[nodiscard]] QSpinBox&       columnSpinner(Row row) const;
    [[nodiscard]] UnitSelector&   unitSelector(Row row) const;
    [[nodiscard]] QWidget&        separatorLine() const noexcept { return *m_separatorLine; }
    [[nodiscard]] QLabel&         separatorLabel() const noexcept { return *m_separatorLabel; }
    [[nodiscard]] FieldSeparatorComboBox& separatorComboBox() const noexcept
    {
        return *m_separatorComboBox;
    }
    [[nodiscard]] QGroupBox&   previewBox() const noexcept { return *m_previewBox; }
    [[nodiscard]] QLabel&      previewHelp() const noexcept { return *m_previewHelp; }
    [[nodiscard]] QTextEdit&   previewTextPane() const noexcept { return *m_previewTextPane; }
    [[nodiscard]] QPushButton& cancelButton() const noexcept { return *m_cancelButton; }
    [[nodiscard]] QPushButton& okButton() const noexcept { return *m_okButton; }

private:
    /// The widgets of a row.
    struct RowWidgets
    {
        QLabel*         label   = nullptr;
        QStackedWidget* panel   = nullptr;
        QLineEdit*      field   = nullptr;
        QSpinBox*       spinner = nullptr;
        UnitSelector*   unit    = nullptr;
    };

    [[nodiscard]] QWidget&          buildMainPanel(const Settings& initial);
    void                            buildCsvSettings(const Settings& initial);
    void                            buildRow(Row row, int gridRow, const Column& initial);
    void                            buildPreview();
    void                            connectFields();
    [[nodiscard]] const RowWidgets& widgetsOf(Row row) const;

    /// The column type label's text: "Column Name" with a header row, else "Column Index".
    [[nodiscard]] static QString columnTypeText(bool hasHeader);
    /// The header check box changed (Java's ItemListener).
    void headerChanged();
    /// Shows each row's name field with a header row, its spin box without
    /// (updateColumnFieldValidation()).
    void updateColumnFieldValidation();
    /// Java's validateColumnNames() and validateColumnIndices().
    void               validateColumnNames();
    void               validateColumnIndices();
    void               resetErrorOutlines();
    [[nodiscard]] bool hasDuplicateColumnNames() const;
    /// Shows preview().
    void updatePreview();
    /// The preview's height follows its text's.
    void fitPreviewHeight();

    UserPrompts*                      m_prompts;
    QScrollArea*                      m_scrollArea;
    QLabel*                           m_description{nullptr};
    QGroupBox*                        m_csvSettings{nullptr};
    QLabel*                           m_mappingHelp{nullptr};
    QCheckBox*                        m_hasHeader{nullptr};
    QLabel*                           m_dataTypeLabel{nullptr};
    QLabel*                           m_columnTypeLabel{nullptr};
    QLabel*                           m_unitLabel{nullptr};
    std::array<RowWidgets, kRowCount> m_rows{};
    QWidget*                          m_separatorLine{nullptr};
    QLabel*                           m_separatorLabel{nullptr};
    FieldSeparatorComboBox*           m_separatorComboBox{nullptr};
    QGroupBox*                        m_previewBox{nullptr};
    QLabel*                           m_previewHelp{nullptr};
    QTextEdit*                        m_previewTextPane{nullptr};
    QPushButton*                      m_cancelButton{nullptr};
    QPushButton*                      m_okButton{nullptr};
};

}  // namespace QtRocket::Gui
