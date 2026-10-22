#pragma once

#include <optional>

#include "QtRocket/gui/dialogs/OptionDialog.h"

class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QWidget;

namespace QtRocket::Gui
{

/// OpenRocket's question of StorageOptionChooser.verifyStorageOptions(): "Save options"
/// (StorageOptChooser.lbl.Saveopt), a question with OK and Cancel whose message is the storage
/// option chooser (StorageOptionChooser): in a titled border "Save options", indented 10 pixels,
/// the label "Simulated data to store:", the choice between "All simulated data" and "Only
/// summary data" (each with its tooltip), and below them the estimate of the file's size for the
/// choice ("Largest file size" or "Smallest file size", with its tooltip). OK keeps the choice,
/// Cancel and closing the dialog cancel the save.
///
/// The file operations ask it before a save of a design that holds simulated data and whose
/// storage options are not set explicitly, and after the file of a Save As of such a design
/// (WidgetUserPrompts::askStorageOptions(), askSaveFile()).
///
/// Deviations: the chooser is not beside the files of the Save As file dialog (a JFileChooser
/// accessory, which a platform's file dialog cannot hold) but this question after it
/// (WidgetUserPrompts::askSaveFile()). The third choice and the time step spinner that Java's
/// StorageOptionChooser declares but never makes are not here (nor its estimate "Smaller file
/// size", which only that third choice would show).
class StorageOptionsDialog final : public OptionDialog
{
    Q_OBJECT

public:
    /// The indentation of the chooser (BorderFactory.createEmptyBorder(0, 10, 0, 0)).
    static constexpr int kIndent = 10;
    /// The gap below "Only summary data" (MigLayout "wrap 20lp").
    static constexpr int kInfoGap = 20;

    /// The question with "All simulated data" chosen when @p saveSimulationData, else "Only
    /// summary data" (loadOptions()), not shown yet.
    explicit StorageOptionsDialog(bool saveSimulationData, QWidget* parent = nullptr);
    ~StorageOptionsDialog() override;
    StorageOptionsDialog(const StorageOptionsDialog&)            = delete;
    StorageOptionsDialog& operator=(const StorageOptionsDialog&) = delete;
    StorageOptionsDialog(StorageOptionsDialog&&)                 = delete;
    StorageOptionsDialog& operator=(StorageOptionsDialog&&)      = delete;

    /// Asks the question, modal, parented to @p parent (or none), starting from
    /// @p saveSimulationData: true for all simulated data, false for the summary only, nullopt
    /// when it was cancelled (verifyStorageOptions() returning false).
    [[nodiscard]] static std::optional<bool> ask(QWidget* parent, bool saveSimulationData);

    /// The choice: whether "All simulated data" is chosen (storeOptions()).
    [[nodiscard]] bool saveSimulationData() const noexcept;
    /// Whether OK closed the question.
    [[nodiscard]] bool isOk() const noexcept;

    /// The titled border ("storageOptionChooser").
    [[nodiscard]] QGroupBox&    chooser() const noexcept { return *m_chooser; }
    [[nodiscard]] QLabel&       dataLabel() const noexcept { return *m_dataLabel; }
    [[nodiscard]] QRadioButton& allButton() const noexcept { return *m_all; }
    [[nodiscard]] QRadioButton& summaryButton() const noexcept { return *m_summary; }
    /// The estimate of the file's size ("infoLabel").
    [[nodiscard]] QLabel&      infoLabel() const noexcept { return *m_info; }
    [[nodiscard]] QPushButton* okButton() const noexcept { return m_ok; }
    [[nodiscard]] QPushButton* cancelButton() const noexcept { return m_cancel; }

private:
    /// updateInfoLabel(): the estimate of the choice.
    void updateInfoLabel();

    /// The empty border around the chooser.
    QWidget*      m_indent;
    QGroupBox*    m_chooser;
    QLabel*       m_dataLabel;
    QRadioButton* m_all;
    QRadioButton* m_summary;
    QLabel*       m_info;
    QPushButton*  m_ok;
    QPushButton*  m_cancel;
};

}  // namespace QtRocket::Gui
