#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QChar>
#include <QDialog>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/GuiServices.h"

class QEvent;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QWidget;

namespace QtRocket
{
class MachAoALookup;
class SimulationOptions;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class FieldSeparatorComboBox;
class ObjectSelection;

/// The aerodynamic lookup tables of a simulation (OpenRocket's AerodynamicLookupDialog, swing
/// gui/simulation/): "Aerodynamic lookup tables", the dialog the simulation options tab's
/// "Configure..." opens, modal for the application, which replaces the Barrowman drag, the
/// Barrowman stability or both with tables read from CSV files (SimulationOptions'
/// setDragLookup() and setStabilityLookup(); the core reads them with CsvMachAoALookup and keeps
/// their rows to embed them in the .ork file).
///
/// Two tabs, "Drag CSV" and "Stability CSV" (Table::DRAG, Table::STABILITY), each with:
/// - "Load from file..." (chooseLookup()): the file question "Select CSV file"
///   (UserPrompts::askOpenFiles(), the filter "CSV files (*.csv, *.txt, *.dat)" chosen, then "All
///   Files"; in the default directory, with the name of the table's file filled in, as Java's
///   JFileChooser, whose directory is set to the default directory after its selected file);
///   the default directory then becomes the one chosen from, and the file is read at once with
///   the section's field separator (CsvMachAoALookup::fromCsv(): the columns "cd", or "cn",
///   "cm" and "cp"); a table that is read is given to the options at once, without rows (they
///   come from the file), as Java does (Cancel keeps it: SC's fidelity note B13); one that cannot
///   be read shows the error "Failed to load CSV" ("Failed to load <file>:" and the reason);
/// - a refresh button (Icons::REFRESH, "Reload data from the CSV file if it exists";
///   refreshLookup()), enabled while there is a table: the table's file is read again (and given
///   to the options), or the warning "The CSV file does not exist:" and the file when it is gone;
/// - "Clear" (clearLookup()), enabled while there is a table: the section forgets its table (the
///   options keep it until OK);
/// - the summary in the information colour: "Using built-in Barrowman data", or the file's name,
///   " - " and the table's ranges (summaryText(), formatLookupSummary()), with " (modified)"
///   (italic) once the text below is edited;
/// - "Field separator string:" with a FieldSeparatorComboBox (",", ";", SPACE, TAB, or a
///   character typed), the separator of a load, a refresh and OK's parse; a table that is read
///   chooses the separator it was read with in its section;
/// - a text area titled "Example format:", read-only, in grey, showing the example table of the
///   section with its commas replaced by the separator; or, while there is a table, titled
///   "Loaded data:", editable, in black: the table's rows as the options keep them, or else the
///   lines of its file (read as Files.readAllLines() reads them; when the file cannot be read, the
///   example again). Its font is the system's fixed-pitch font in the area's size, on white
///   (Java's colours in every theme, as Java does).
/// Below them "Cancel" (rejects: the options keep what the loads gave them) and "OK"
/// (applyAndClose()): an edited text area of a section with a table is parsed (its lines trimmed,
/// the empty ones and those starting with '#' left out of the parse, all of them kept as the
/// table's rows); a text that cannot be parsed shows the error "Failed to load CSV" for the
/// section's file and keeps the dialog open (nothing is given to the options then); otherwise
/// each section gives the options its table (setDragLookup(), setStabilityLookup(), with the rows
/// of the edit or the options' own) or clears theirs (clearDragLookup(), clearStabilityLookup())
/// and the dialog closes (accept()).
///
/// The edits of a section (isModified()) are counted only while it shows a table's data; any
/// text the area is given by the dialog itself (a load, a refresh, Clear, a separator chosen,
/// which shows the options' rows again) is no edit. As in Java a separator chosen while the
/// data is shown puts the options' rows (or the file's lines) back into the area over an edit,
/// whose "(modified)" stays.
///
/// The options: a simulation's (the dialog is opened from its options tab) or any others (the
/// tests of OpenRocket's AerodynamicLookupPreservationTest). They are held through a selection
/// (ObjectSelection) of their change signal, so that the dialog touches them no more should they
/// go while it is open (during one of its questions); its actions then do nothing. Every question
/// and message goes through the services' UserPrompts; the dialog itself is a ModalDialog child
/// of the options tab's window (SimulationOptionsPanel::openLookupDialog()), and an action stops
/// when the dialog goes during one of its questions.
///
/// Deviations from OpenRocket:
/// - The file question lists "CSV files (*.csv, *.txt, *.dat)" first, chosen, then "All Files"
///   (Java's JFileChooser lists its accept-all filter first and chooses the CSV filter), and its
///   filter has Qt's form "CSV files (*.csv *.txt *.dat)" (Qt reads the patterns from the
///   parentheses, separated by spaces).
/// - A field separator that is not an ASCII character cannot be used: the core reads a table's
///   fields at a byte (CsvMachAoALookup), so a load, a refresh or OK with such a separator shows
///   "Failed to load CSV" with the reason "The field separator must be an ASCII character."
///   (QtRocket's text). Java splits at any character.
/// - Escape cancels (a QDialog's Escape: reject()), where Java's dialog has no key for it; its
///   window's close button cancels in both.
/// - The text areas are QPlainTextEdits: at least 3 rows high (Java: setRows(3)), growing with the
///   dialog.
/// - A bug of OpenRocket's, found while porting, fixed: a load, a refresh or Clear of one table
///   shows both sections again from the options (updateDisplays()), which throws away an edit of
///   the other table's text (its "(modified)" goes, and OK keeps the options' rows). Here the
///   other section is left as it is while it has an edit.
/// - The stability table's columns are listed in their order in the table ("CN, CM, CP"); Java
///   lists them in the order of a Set.copyOf(), which differs from one run of the JVM to the
///   next.
class AerodynamicLookupDialog final : public QDialog
{
    Q_OBJECT

public:
    /// A section of the dialog.
    enum class Table
    {
        /// "Drag CSV": the drag table (the column "cd").
        DRAG,
        /// "Stability CSV": the stability table (the columns "cn", "cm" and "cp").
        STABILITY,
    };

    /// The rows the text areas show at least (Java: setRows(3)).
    static constexpr int kTextRows = 3;

    /// The dialog over @p options; @p services give the preferences (the default directory) and
    /// the prompts. Not shown; modal for the application.
    AerodynamicLookupDialog(SimulationOptions& options, const GuiServices& services,
                            QWidget* parent = nullptr);
    ~AerodynamicLookupDialog() override;
    AerodynamicLookupDialog(const AerodynamicLookupDialog&)            = delete;
    AerodynamicLookupDialog& operator=(const AerodynamicLookupDialog&) = delete;
    AerodynamicLookupDialog(AerodynamicLookupDialog&&)                 = delete;
    AerodynamicLookupDialog& operator=(AerodynamicLookupDialog&&)      = delete;

    // ---------------------------------------------------------------------------- the texts

    /// Java's static formatLookupSummary(): "Mach <min> - <max>; columns: <columns>" or, for a
    /// table with angles of attack, "Mach <min> - <max>; AoA <min> - <max>deg; columns:
    /// <columns>" (AerodynamicLookupDialog.summaryNoAoA, .summaryWithAoA; the dash is an en dash,
    /// "deg" the degree sign), the numbers as DecimalFormat "0.###" writes them, the columns
    /// upper-cased and joined with ", ".
    [[nodiscard]] static QString formatLookupSummary(const MachAoALookup& table);

    /// The summary of a table as the dialog and the options tab's tooltip show it (Java:
    /// SimulationOptionsPanel.buildLookupDetail(), the full text of updateSummaryLabel()):
    /// "Using built-in Barrowman data" without a path or a table, otherwise the file's name (the
    /// whole path when it has none), " - " and formatLookupSummary().
    [[nodiscard]] static QString summaryText(const std::optional<std::filesystem::path>& path,
                                             const MachAoALookup*                        table);

    /// The file question's filter, "CSV files (*.csv *.txt *.dat)" (see the deviations).
    [[nodiscard]] static QString csvNameFilter();

    /// The example of @p table's format (AerodynamicLookupDialog.example.drag, .example.stability)
    /// with its commas replaced by @p separator.
    [[nodiscard]] static QString exampleText(Table table, QChar separator);

    // ---------------------------------------------------------------------------- the widgets

    /// The two tabs (objectName tabbedPane).
    [[nodiscard]] QTabWidget& tabs() const noexcept { return *m_tabs; }
    /// "Load from file..." (objectName dragLoadButton, stabilityLoadButton).
    [[nodiscard]] QPushButton& loadButton(Table table) const noexcept;
    /// The refresh button (dragRefreshButton, stabilityRefreshButton: Java's fields).
    [[nodiscard]] QPushButton& refreshButton(Table table) const noexcept;
    /// "Clear" (clearDragButton, clearStabilityButton).
    [[nodiscard]] QPushButton& clearButton(Table table) const noexcept;
    /// The summary (dragSummaryLabel, stabilitySummaryLabel).
    [[nodiscard]] QLabel& summaryLabel(Table table) const noexcept;
    /// The separator's box (dragSeparatorCombo, stabilitySeparatorCombo).
    [[nodiscard]] FieldSeparatorComboBox& separatorComboBox(Table table) const noexcept;
    /// The titled box of the text area, "Example format:" or "Loaded data:" (dragExampleBorder,
    /// stabilityExampleBorder).
    [[nodiscard]] QGroupBox& textBox(Table table) const noexcept;
    /// The text area (dragExampleArea, stabilityExampleArea).
    [[nodiscard]] QPlainTextEdit& textArea(Table table) const noexcept;
    /// "Cancel" and "OK" (cancelButton, okButton).
    [[nodiscard]] QPushButton& cancelButton() const noexcept { return *m_cancelButton; }
    [[nodiscard]] QPushButton& okButton() const noexcept { return *m_okButton; }

    // ----------------------------------------------------------------------------- the state

    /// The section's file (Java: dragCsv, stabilityCsv), absolute and normalised; nullopt without
    /// a table.
    [[nodiscard]] const std::optional<std::filesystem::path>& csvPath(Table table) const noexcept;
    /// The section's table (Java: dragTable, stabilityTable); null without one.
    [[nodiscard]] const std::shared_ptr<const MachAoALookup>& lookupTable(
        Table table) const noexcept;
    /// Whether the section's text was edited since it showed its table's data (Java:
    /// dragModified, stabilityModified).
    [[nodiscard]] bool isModified(Table table) const noexcept;

    // --------------------------------------------------------------------------- the actions

    /// "Load from file..." of @p table (Java: chooseDragLookup(), chooseStabilityLookup()).
    void chooseLookup(Table table);
    /// The refresh button of @p table (Java: refreshDragLookup(), refreshStabilityLookup()).
    void refreshLookup(Table table);
    /// "Clear" of @p table.
    void clearLookup(Table table);
    /// Java's reloadDragLookup(), reloadStabilityLookup() (private there): what the refresh
    /// button does once the table's file exists; the file is read again with the section's
    /// separator and the table given to the options, or the error "Failed to load CSV" shown.
    /// Nothing without a table.
    void reloadLookup(Table table);
    /// "OK" (Java: applyAndClose()): true when the tables were given to the options and the
    /// dialog closed, false when a text could not be parsed (the dialog stays open) or the
    /// options have gone.
    bool applyAndClose();

protected:
    void changeEvent(QEvent* event) override;

private:
    /// A section: Java's fields of one table.
    struct Section
    {
        std::optional<std::filesystem::path> csv;
        std::shared_ptr<const MachAoALookup> table;
        /// The separator the table was read with (Java: dragSeparator).
        QChar separator{u','};
        bool  modified{false};
        /// Whether the text area shows the table's data and counts edits (Java: the document
        /// listener is installed).
        bool                    listening{false};
        QPushButton*            load{nullptr};
        QPushButton*            refresh{nullptr};
        QPushButton*            clear{nullptr};
        QLabel*                 summary{nullptr};
        FieldSeparatorComboBox* separatorCombo{nullptr};
        QGroupBox*              textBox{nullptr};
        QPlainTextEdit*         text{nullptr};
    };

    [[nodiscard]] Section&       section(Table table) noexcept;
    [[nodiscard]] const Section& section(Table table) const noexcept;
    /// The value columns of @p table's CSV ("cd", or "cn", "cm", "cp").
    [[nodiscard]] static const std::vector<std::string>& valueColumns(Table table);
    /// The options while their selection is valid; null otherwise.
    [[nodiscard]] SimulationOptions* liveOptions() const;

    QWidget* buildSection(Table table);
    /// Gives the options @p table's table @p read from the file @p path with @p separator (no
    /// rows: they come from the file), then shows it.
    void storeReadTable(Table table, const std::filesystem::path& path,
                        std::shared_ptr<const MachAoALookup> read, QChar separator);
    /// OK's parse of @p table's edited text (when it was edited and there is a file): its table
    /// becomes the section's, @p rows its lines; false, with the error shown, when the text
    /// cannot be parsed.
    [[nodiscard]] bool parseEdit(Table table, std::optional<std::vector<std::string>>& rows);
    /// Java's updateDisplays(): both sections shown again, but for the section other than
    /// @p changed while it has an edit (see the deviations).
    void updateDisplays(std::optional<Table> changed = std::nullopt);
    /// Java's updateSectionDisplay().
    void updateSectionDisplay(Table table);
    /// Java's updateExampleFormat().
    void updateExampleFormat(Table table);
    /// Java's markModified().
    void markModified(Table table);
    /// Java's updateSummaryLabel().
    void updateSummaryLabel(Table table);
    /// Gives the text area of @p table @p text, which counts as no edit.
    void setAreaText(Table table, const QString& text);
    /// Java's showLookupError(): ERROR "Failed to load CSV", "Failed to load <file>:" and
    /// @p reason.
    void showLookupError(const std::filesystem::path& path, const QString& reason);
    /// The separator of @p table's box as the core reads it, or nullopt (with the error shown)
    /// for one that is not ASCII.
    [[nodiscard]] std::optional<char> separatorFor(Table table, const std::filesystem::path& path);
    /// The text areas' and summaries' colours.
    void updateColors();

    GuiServices                      m_services;
    std::shared_ptr<ObjectSelection> m_options;
    std::array<Section, 2>           m_sections;
    QTabWidget*                      m_tabs{nullptr};
    QPushButton*                     m_cancelButton{nullptr};
    QPushButton*                     m_okButton{nullptr};
    /// Set while the dialog gives a text area its text.
    bool m_settingText{false};
};

}  // namespace QtRocket::Gui
