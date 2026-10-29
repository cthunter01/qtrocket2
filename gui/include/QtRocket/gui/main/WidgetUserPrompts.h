#pragma once

#include <filesystem>
#include <optional>
#include <span>

#include <QChar>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket
{
class FlightConfigurationId;
class MotorMount;
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;

/// The application's UserPrompts: OpenRocket's JOptionPane questions, file choosers and dialogs,
/// modal, parented to the window or dialog that asks (or to none), with OpenRocket's texts:
/// - askInsertPosition(): ComponentAddButtons.askPosition()'s "Select component position" /
///   "Select stage position", a question with "Insert here" (the initial option), "Add to the
///   end" and "Cancel" (also closing it), "Do not ask me again" and, 2 points smaller, "You can
///   change the default operation in the preferences.";
/// - askDiscardChanges(): RocketComponentConfig's "Cancel operation" (discard the changes to a
///   component, or undo adding it) and PreferencesDialog's "Discard Preference Changes",
///   warnings with Yes and No and "Don't ask me again" (its state is answered whatever the
///   answer, as OpenRocket stores it as soon as it is ticked);
/// - askSaveChanges(): BasicFrame.closeAction()'s "Design not saved", with Yes (save), No
///   (discard) and Cancel (also Esc and the close button), a QMessageBox;
/// - askOverwrite(): FileHelper.confirmWrite()'s "File exists" ("File '<name>' exists.  Do you
///   want to overwrite it?"), Yes or No;
/// - askStorageOptions(): StorageOptionChooser.verifyStorageOptions()'s "Save options"
///   (StorageOptionsDialog);
/// - askYesNo(): a question with the caller's title and text, the severity's icon, Yes or No;
/// - askDecalFile(): DecalNotFoundDialog.showDialog(): the question (DecalNotFoundDialog), then on
///   Yes a file chooser for the image (any file, starting in the given directory);
/// - askOpenFiles(), askSaveFile(): the platform's file dialog (QFileDialog), titled as asked (the
///   platform's "Open" / "Save" title without one), with the name filters (the one asked for
///   chosen, else the first), starting in the given directory (the user's home directory without
///   one), the proposed name chosen, the accepting button named as asked (the platform's name
///   without a text); the save dialog answers the name filter chosen in it too (asked again, it
///   starts with that one), and does not ask itself whether to overwrite a
///   file (the caller forces the extension first and asks askOverwrite()), so on macOS, whose
///   save panel always asks, it is Qt's own dialog; with a description (the image import's), a
///   Qt file dialog with the description beside the files (the JFileChooser accessory); a save
///   that refuses illegal names (SaveFileChooser's) shows "Illegal filename" ("The filename
///   '<name>' may not contain the character ' <c> '. Please remove it.") for a name with one of
///   / \ : * ? " < > | (SaveFileNames, the name relative to the dialog's directory) and asks
///   again, in that directory with that name; a save that offers a storage option asks "Save
///   options" (StorageOptionsDialog) once the file is chosen, whose Cancel cancels it; a request
///   for a file or a directory (OpenFileSelection::FILES_AND_DIRECTORIES: the preferences' Add)
///   first opens a menu "File..." / "Directory..." below the widget that asks (the Add button;
///   at the mouse pointer when a window asks), then the platform's dialog for the choice: the
///   file dialog with the request's filters, or the folder dialog; the menu closed without a
///   choice gives no file and shows no dialog;
/// - editDesignInformation(): BasicFrame.showSaveRocketInfoDialog()'s "Save Design Info"
///   (SaveDesignInfoDialog, with the services the caller gives), whose outcome changes nothing of
///   the Save As that asks it (the user's decision R3-3);
/// - askIgnition(), askDeployment(), askSeparation(), askRenameConfiguration(): the value dialogs
///   of the "Motors & Configuration" tab, "Select Ignition Configuration"
///   (IgnitionSelectionDialog), "Select Deployment Configuration" (DeploymentSelectionDialog),
///   "Select Separation Configuration" (SeparationSelectionDialog) and "Rename Configuration"
///   (RenameConfigDialog);
/// - showMessage(): JOptionPane.showMessageDialog() with the lines of the message (DetailDialog
///   without details);
/// - showWarnings(), showErrorsAndWarnings(): WarningDialog, ErrorWarningDialog;
/// - showDatabaseProblems(): the warnings of OpenRocket's MotorDatabaseLoader and
///   ComponentPresetDatabaseLoader ("Error upon thrust curve import", "Error upon component
///   preset import") in one message box (DetailDialog without details): databaseProblemsTitle()
///   and databaseProblemLines();
/// - showInternalError(): the internal error dialog (InternalErrorDialog).
///
/// Milestone 3's value dialogs, motor chooser (MotorChooserDialog, one per window) and parts
/// library (ComponentPresetChooserDialog) answer their questions.
///
/// Each dialog is made for its question (ModalDialog), and so is the menu of a file or a
/// directory, so that it survives its parent being destroyed while it is open: the question then
/// gives its cancelling answer (Cancel, No, no file), and a question of two dialogs (or of the
/// menu and a dialog) does not show the second one.
///
/// Deviations: OpenRocket asks the insert position over no window (JOptionPane with a null
/// parent) and looks for a decal file over none either; here every question has the caller's
/// parent. The overwrite question puts the file's name into its text as it is: OpenRocket's
/// L10N.replace() reads the name as a regular expression's replacement, so a name with "$"
/// throws (the probe: "a$b.ork" IllegalArgumentException, "a$1.ork" IndexOutOfBoundsException)
/// and one with "\" loses it ("a\b.ork" shows "ab.ork"). The file dialogs are Qt's
/// (QFileDialog), with the platform's own where it has one (Swing's JFileChooser); the storage
/// option is a question after the save dialog rather than beside its files. A file or a directory
/// is chosen in two steps, the menu and then a dialog for one of the two, as Qt's dialogs choose
/// either files or a directory (the user's decision U-M3-6c), where JFileChooser shows both at
/// once; the menu's texts are QtRocket's. The start-up's database problems are one modal message
/// for every file (O-M3-21), where OpenRocket's loaders show a modeless warning, always on top,
/// for each; its lines put the file and the loader's message in as they are (escaped), where
/// OpenRocket's HTML reads a '<' or '&' in them as markup, and are not wrapped at 400 pixels
/// (Qt's rich text has no paragraph width).
///
/// Its own state is none: one object serves every window (app/ApplicationContext owns it).
class WidgetUserPrompts final : public UserPrompts
{
    Q_DECLARE_TR_FUNCTIONS(WidgetUserPrompts)

public:
    WidgetUserPrompts()                                    = default;
    ~WidgetUserPrompts() override                          = default;
    WidgetUserPrompts(const WidgetUserPrompts&)            = delete;
    WidgetUserPrompts& operator=(const WidgetUserPrompts&) = delete;
    WidgetUserPrompts(WidgetUserPrompts&&)                 = delete;
    WidgetUserPrompts& operator=(WidgetUserPrompts&&)      = delete;

    /// The text of the "Design not saved" question about the design @p designName.
    [[nodiscard]] static QString saveChangesText(const QString& designName);
    /// The text of the "File exists" question about the file named @p fileName.
    [[nodiscard]] static QString overwriteText(const QString& fileName);
    /// The text of the "Illegal filename" warning about the name @p fileName, which holds the
    /// character @p character.
    [[nodiscard]] static QString illegalFileNameText(const QString& fileName, QChar character);
    /// The title of the message of the start-up's database problems @p problems: the title of
    /// OpenRocket's warning for their database (MotorDbLoaderDlg.title "Error upon thrust curve
    /// import", ComponentDbLoaderDlg.title "Error upon component preset import"), both joined
    /// by " / " when files of both failed.
    [[nodiscard]] static QString databaseProblemsTitle(std::span<const DatabaseProblem> problems);
    /// The lines of that message: for each problem in order, the text of OpenRocket's warning
    /// about its file (rich text: the loader's message in italics and a period, an empty line,
    /// "Thrust curve <b><file></b> will be ignored during import." (MotorDbLoaderDlg.message1
    /// as MessageFormat reads it) and "You can try replacing, deleting or manually editing the
    /// thrust curve file to fix this issue." (.message2); ComponentDbLoaderDlg's for a component
    /// preset file).
    [[nodiscard]] static QStringList databaseProblemLines(
        std::span<const DatabaseProblem> problems);

    [[nodiscard]] InsertPositionAnswer askInsertPosition(QWidget*               parent,
                                                         InsertPositionQuestion question) override;
    [[nodiscard]] DiscardAnswer        askDiscardChanges(QWidget*        parent,
                                                         DiscardQuestion question) override;
    [[nodiscard]] SaveChangesAnswer    askSaveChanges(QWidget*       parent,
                                                      const QString& designName) override;
    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override;
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* parent,
                                                        bool     saveSimulationData) override;
    [[nodiscard]] bool                askYesNo(QWidget* parent, const Question& question) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override;
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) override;
    void editDesignInformation(QWidget* parent, const GuiServices& services,
                               OpenRocketDocument& document) override;
    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(
        QWidget* parent, const IgnitionRequest& request) override;
    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override;
    [[nodiscard]] std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) override;
    [[nodiscard]] RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) override;
    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override;
    [[nodiscard]] PresetAnswer choosePreset(QWidget* parent, const GuiServices& services,
                                            const PresetRequest& request) override;
    void                       showMessage(QWidget* parent, const Message& message) override;
    void showWarnings(QWidget* parent, const WarningsMessage& message) override;
    void showErrorsAndWarnings(QWidget* parent, const ErrorsAndWarningsMessage& message) override;
    void showDatabaseProblems(QWidget* parent, std::span<const DatabaseProblem> problems) override;
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override;

private:
    /// The file chooser of askSaveFile(), asked again after the warning while the request
    /// refuses the name chosen: the file, the directory and the name filter chosen (no storage
    /// option yet); nullopt when it was cancelled (or its parent destroyed).
    [[nodiscard]] std::optional<SaveFileAnswer> askLegalFileToSave(QWidget*               parent,
                                                                   const SaveFileRequest& request);
};

}  // namespace QtRocket::Gui
