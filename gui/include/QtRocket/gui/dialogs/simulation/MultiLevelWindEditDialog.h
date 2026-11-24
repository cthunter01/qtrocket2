#pragma once

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/util/Signal.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QSplitter;
class QWidget;

namespace QtRocket
{
class MultiLevelPinkNoiseWindModel;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class MultiLevelWindTableModel;
class MultiLevelWindTableView;
class WindProfilePlot;

/// The wind editor (OpenRocket's MultiLevelWindEditDialog, swing gui/simulation/): "Wind Profile
/// Editor", which edits the levels of a MultiLevelPinkNoiseWindModel live (the launch conditions'
/// "Edit levels", SimulationConditionsPanel, opens it over the simulation's multi-level wind; the
/// summary there follows every change, WindLevelSummary). Modal for the whole application (Java:
/// APPLICATION_MODAL), 900 x 500 pixels to begin with and at least 700 x 400; a ModalDialog child
/// of the window that opens it, opened directly, not through UserPrompts (the critic's G14).
///
/// What it shows (Java's layout): a split pane (splitPane) whose left side (the table's columns'
/// width to begin with; 60 % of a change of the width, Java's resize weight) holds
/// - the table of the levels (MultiLevelWindTableView over a MultiLevelWindTableModel: the
///   columns, their unit selectors, the editing, the sorting, the deletion);
/// - "Altitude reference:" (tooltip "Set to which altitude the wind levels refer to.") with the
///   radio buttons "Mean Sea Level (MSL)" (butMSL) and "Above Ground Level (AGL)" (butAGL), which
///   set the model's altitude reference (the table's first column and the plot's altitude follow);
/// - the buttons "Add new level" (Icons FILE_NEW; MultiLevelWindTableView::addLevel()), "Delete
///   level" (EDIT_DELETE, tooltip "Delete the selected wind level"; enabled while a row is selected
///   and there is more than one level), "Import levels" (FILE_IMPORT, tooltip "Import wind levels
///   from a CSV file"; importLevels()) and below "Delete level" "Reset levels" (tooltip "Reset wind
///   levels to default"; resetLevels());
/// and whose right side holds the title "Wind Profile Visualization" (1.5 points larger, bold)
/// above the profile (WindProfilePlot), which shows the table's units and its selected level.
/// Below the split pane, at the right, "Close" (closeButton), which closes the dialog, as do Esc
/// and the window's close button (Java: GUIUtil.installEscapeCloseButtonOperation()); no button is
/// the default, so Enter presses none.
///
/// Every button and the closing first commit a cell editor's pending text
/// (MultiLevelWindTableView::commitEditing(); Java's spinner commits when its focus goes, which a
/// macOS button does not take).
///
/// The model must stay at its address while the dialog shows it (a simulation's options keep their
/// multi-level wind for their life); the parts learn that it has gone from its change signal and
/// then leave it alone.
///
/// Deviations from OpenRocket: the table (MultiLevelWindTableView: U-M4-6, bugs 31 to 33) and the
/// profile (WindProfilePlot: rocketplot) are QtRocket's; the radio buttons follow the model's
/// altitude reference when it changes elsewhere (Java sets them once).
class MultiLevelWindEditDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The size the dialog opens with, and its smallest (Java's setSize() and setMinimumSize()).
    static constexpr int kWidth         = 900;
    static constexpr int kHeight        = 500;
    static constexpr int kMinimumWidth  = 700;
    static constexpr int kMinimumHeight = 400;
    /// The width the profile asks for when the dialog opens (Java: setPreferredSize(300, 400)).
    static constexpr int kVisualizationWidth = 300;
    /// The left side's share of a change of the dialog's width (Java: setResizeWeight(0.6)), as
    /// stretch factors.
    static constexpr int kTableStretch         = 3;
    static constexpr int kVisualizationStretch = 2;

    /// The editor of @p model's levels (not shown), asking its questions through @p services'
    /// prompts and taking a new level's wind from @p services' preferences; @p services must
    /// outlive it.
    MultiLevelWindEditDialog(MultiLevelPinkNoiseWindModel& model, const GuiServices& services,
                             QWidget* parent = nullptr);
    ~MultiLevelWindEditDialog() override;
    MultiLevelWindEditDialog(const MultiLevelWindEditDialog&)            = delete;
    MultiLevelWindEditDialog& operator=(const MultiLevelWindEditDialog&) = delete;
    MultiLevelWindEditDialog(MultiLevelWindEditDialog&&)                 = delete;
    MultiLevelWindEditDialog& operator=(MultiLevelWindEditDialog&&)      = delete;

    [[nodiscard]] MultiLevelWindTableModel& tableModel() const noexcept { return *m_tableModel; }
    [[nodiscard]] MultiLevelWindTableView&  table() const noexcept { return *m_table; }
    [[nodiscard]] WindProfilePlot&          visualization() const noexcept { return *m_plot; }
    [[nodiscard]] QSplitter&                splitPane() const noexcept { return *m_splitPane; }
    [[nodiscard]] QLabel& visualizationTitle() const noexcept { return *m_visualizationTitle; }
    [[nodiscard]] QLabel& altitudeReferenceLabel() const noexcept { return *m_altitudeRefLabel; }
    [[nodiscard]] QRadioButton& mslButton() const noexcept { return *m_mslButton; }
    [[nodiscard]] QRadioButton& aglButton() const noexcept { return *m_aglButton; }
    [[nodiscard]] QPushButton&  addButton() const noexcept { return *m_addButton; }
    [[nodiscard]] QPushButton&  deleteButton() const noexcept { return *m_deleteButton; }
    [[nodiscard]] QPushButton&  importButton() const noexcept { return *m_importButton; }
    [[nodiscard]] QPushButton&  resetButton() const noexcept { return *m_resetButton; }
    [[nodiscard]] QPushButton&  closeButton() const noexcept { return *m_closeButton; }

    /// The question before levels are overwritten (Java's JOptionPane.showConfirmDialog(YES_NO,
    /// WARNING) before Reset levels and before an import over levels): "Overwrite wind levels?",
    /// "Your current wind levels will be overwritten. Do you want to continue?"
    /// (WindProfileEditorDlg.dlg.overwriteLevels.title and .msg).
    [[nodiscard]] static UserPrompts::Question overwriteLevelsQuestion();

    /// "Reset levels": asks overwriteLevelsQuestion() (UserPrompts::askYesNo()) and on Yes gives
    /// the model the one level of the preferences' average wind
    /// (MultiLevelWindTableModel::resetLevels()).
    void resetLevels();

    /// "Import levels" (Java's importLevels(): the CSV import settings, the file, the overwrite
    /// question, the import; part S3 of Milestone 4's run 2). Does nothing yet.
    void importLevels();

    /// Commits a cell editor's pending text, then closes the dialog (QDialog::done()).
    void done(int result) override;

private:
    void                   buildTablePanel(QWidget& panel);
    void                   buildControls(QWidget& panel);
    [[nodiscard]] QWidget& buildVisualizationPanel();
    void                   connectParts();
    /// The radio buttons show the model's altitude reference.
    void showAltitudeReference();
    /// The model takes @p reference (Java: the radio buttons' listeners).
    void chooseAltitudeReference(WindModel::AltitudeReference reference);
    /// "Delete level" is enabled while a row is selected and there is more than one level.
    void updateDeleteButton();
    /// The profile shows the table's units and its selected level.
    void showUnits();
    void showSelectedLevel();

    GuiServices                   m_services;
    MultiLevelPinkNoiseWindModel* m_model;
    Signal<>::ScopedConnection    m_connection;
    MultiLevelWindTableModel*     m_tableModel;
    QSplitter*                    m_splitPane;
    MultiLevelWindTableView*      m_table{nullptr};
    WindProfilePlot*              m_plot{nullptr};
    QLabel*                       m_visualizationTitle{nullptr};
    QLabel*                       m_altitudeRefLabel{nullptr};
    QRadioButton*                 m_mslButton{nullptr};
    QRadioButton*                 m_aglButton{nullptr};
    QPushButton*                  m_addButton{nullptr};
    QPushButton*                  m_deleteButton{nullptr};
    QPushButton*                  m_importButton{nullptr};
    QPushButton*                  m_resetButton{nullptr};
    QPushButton*                  m_closeButton{nullptr};
};

}  // namespace QtRocket::Gui
