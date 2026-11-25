#pragma once

#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/util/CsvOptionValues.h"

class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

class CsvOptionPanel;

/// The CSV options of a file being saved, asked once the file is chosen: OpenRocket's
/// CsvOptionPanel(SimulationTableCSVExport.class), which SimulationPanel.
/// setUpSimExportCSVFileChooser() makes the accessory of the simulation table's save file chooser
/// (WidgetUserPrompts::askSaveFile() with UserPrompts::SaveFileRequest::csvOptions; the user's
/// decision U-M4-6). A question titled as the file question it follows (the chooser that holds
/// Java's panel: "Save as CSV file"), without an icon, whose message is the panel ("csvOptions"):
/// "Format settings" (the field separator, the decimal places, the exponential notation) and
/// "Comments", which shows the comment character alone, as the table export's panel has no
/// comment options (Java's table export stores the comment character and never uses it); then OK,
/// the initial option, and Cancel. OK answers the values shown; Cancel and closing the dialog
/// (Esc, the close button) cancel the save.
///
/// The panel starts with the values asked (the caller's, from the preferences:
/// CsvOptionPreferences::load()); it neither reads nor stores the preferences itself (the caller
/// stores what is answered, as Java's exportSimulationsToCSV() calls storePreferences()). Its base
/// class name is the table export's (CsvOptionPreferences::kSimulationTableCsvExport), the only
/// save with CSV options of Milestone 4. The values' comment options, which no check box shows,
/// come back as they were given.
///
/// Deviation: the options are not beside the files of the file dialog (a JFileChooser accessory,
/// which a platform's file dialog cannot hold) but this question after it, as the storage options
/// of a design's Save As (StorageOptionsDialog).
class CsvOptionsDialog final : public OptionDialog
{
    Q_OBJECT

public:
    /// The question titled @p title (the file question's) with the panel showing @p values, not
    /// shown yet.
    CsvOptionsDialog(const QString& title, const CsvOptionValues& values,
                     QWidget* parent = nullptr);
    ~CsvOptionsDialog() override;
    CsvOptionsDialog(const CsvOptionsDialog&)            = delete;
    CsvOptionsDialog& operator=(const CsvOptionsDialog&) = delete;
    CsvOptionsDialog(CsvOptionsDialog&&)                 = delete;
    CsvOptionsDialog& operator=(CsvOptionsDialog&&)      = delete;

    /// Asks the question, modal, parented to @p parent (or none): the values chosen, or nullopt
    /// when it was cancelled, closed, or destroyed with its parent.
    [[nodiscard]] static std::optional<CsvOptionValues> ask(QWidget* parent, const QString& title,
                                                            const CsvOptionValues& values);

    /// The values the panel shows, with the comment options given to the constructor.
    [[nodiscard]] CsvOptionValues values() const;
    /// Whether OK closed the question.
    [[nodiscard]] bool isOk() const noexcept;

    /// The panel ("csvOptions").
    [[nodiscard]] CsvOptionPanel& optionPanel() const noexcept { return *m_panel; }
    [[nodiscard]] QPushButton*    okButton() const noexcept { return m_ok; }
    [[nodiscard]] QPushButton*    cancelButton() const noexcept { return m_cancel; }

private:
    /// The comment options given, which the panel does not show.
    std::vector<bool> m_commentOptions;
    CsvOptionPanel*   m_panel;
    QPushButton*      m_ok;
    QPushButton*      m_cancel;
};

}  // namespace QtRocket::Gui
