#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

#include "QtRocket/gui/util/Guard.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

/// Every modal question the GUI asks and every message box it shows, as one interface, so that
/// no code below the main windows opens a modal dialog itself: an offscreen test would hang in
/// it. The application's implementation shows OpenRocket's dialogs, with their texts (main/,
/// WidgetUserPrompts); the tests answer from a script (tests/gui/ScriptedUserPrompts.h). The
/// caller passes the dialog's parent window (or nullptr) and does what OpenRocket does with the
/// answer: storing a "do not ask again" preference, the chosen directory as the default
/// directory, ...
///
/// The texts of the fixed questions (insert position, discard, save changes, overwrite, storage
/// options, design information) belong to the implementation; the caller gives the texts of the
/// others.
class UserPrompts
{
public:
    // ------------------------------------------------------------------ the answers and requests

    /// Which add button asks where to insert the new component (ComponentAddButtons.askPosition):
    /// a body component's ("compaddbuttons.lbl.insertcomp", title "compaddbuttons.Selectcomppos")
    /// or a stage's ("compaddbuttons.lbl.insertstage", "compaddbuttons.Selectstagepos").
    enum class InsertPositionQuestion
    {
        BODY_COMPONENT,
        STAGE,
    };

    /// Insert here, Add to the end, or Cancel (closing the dialog cancels too).
    enum class InsertPosition
    {
        INSERT_HERE,
        ADD_TO_END,
        CANCEL,
    };

    struct InsertPositionAnswer
    {
        InsertPosition position = InsertPosition::CANCEL;
        /// "Do not ask me again" was ticked; OpenRocket stores the position as the preference
        /// unless the answer is CANCEL.
        bool doNotAskAgain = false;
    };

    /// Whose changes Cancel would discard: the configuration dialog's, editing a component
    /// ("RocketCompCfg.CancelOperation.msg.discardChanges") or a component just added
    /// ("RocketCompCfg.CancelOperation.msg.undoAdd"), both titled
    /// "RocketCompCfg.CancelOperation.title"; or the preferences dialog's
    /// ("PreferencesDialog.CancelOperation.*").
    enum class DiscardQuestion
    {
        COMPONENT_CHANGES,
        COMPONENT_ADDITION,
        PREFERENCE_CHANGES,
    };

    struct DiscardAnswer
    {
        /// Yes: discard the changes.
        bool discard = false;
        /// "Do not ask me again" was ticked. OpenRocket turns the confirmation off as soon as the
        /// box is ticked, whatever the answer.
        bool doNotAskAgain = false;
    };

    /// The answer to "Design '<name>' has not been saved. Do you want to save it?"
    /// (BasicFrame.dlg.*); closing the dialog is CANCEL.
    enum class SaveChangesAnswer
    {
        SAVE,
        DISCARD,
        CANCEL,
    };

    /// What the user chose in the internal error dialog.
    enum class InternalErrorAnswer
    {
        /// Go on working; closing the dialog answers this too.
        CONTINUE,
        /// End the application at once, without saving any design.
        QUIT,
    };

    /// The icon and kind of a message or question.
    enum class Severity
    {
        INFORMATION,
        WARNING,
        ERROR,
    };

    /// A yes/no question with the caller's texts (JOptionPane.showConfirmDialog YES_NO), such
    /// as "pref.dlg.clearCachedPreferences.*".
    struct Question
    {
        Severity severity = Severity::WARNING;
        QString  title;
        /// Plain or rich text (QMessageBox decides).
        QString text;
    };

    /// A message box with the caller's texts (JOptionPane.showMessageDialog): each line is a
    /// paragraph, as OpenRocket passes an Object[] of strings.
    struct Message
    {
        Severity    severity = Severity::INFORMATION;
        QString     title;
        QStringList lines;
    };

    /// OpenRocket's WarningDialog: the message lines above a list of warnings
    /// ("BasicFrame.WarningDialog.*" after a load or a save), each as Warning::toString().
    struct WarningsMessage
    {
        QString     title;
        QStringList lines;
        QStringList warnings;
    };

    /// The files to open (a JFileChooser in OPEN mode).
    struct OpenFileRequest
    {
        QString title;
        /// Qt's name filters, "OpenRocket designs (*.ork *.ork.gz)", the selected one first.
        QStringList           nameFilters;
        std::filesystem::path directory;
        bool                  multipleFiles = false;
        /// A text shown beside the files (the image import's "CustomFinImport.description");
        /// empty for none.
        QString description;
    };

    struct OpenFileAnswer
    {
        /// At least one file.
        std::vector<std::filesystem::path> files;
        /// The directory the dialog was in, which OpenRocket stores as the default directory.
        std::filesystem::path directory;
    };

    /// The file to save to (a JFileChooser in SAVE mode). The dialog never asks itself whether
    /// to overwrite a file: the caller forces the extension first and then asks askOverwrite().
    struct SaveFileRequest
    {
        QString               title;
        QStringList           nameFilters;
        std::filesystem::path directory;
        /// The proposed name, "<design>.ork".
        QString fileName;
        /// For a design: the storage option the dialog shows beside the files
        /// (StorageOptionChooser, "Simulated data to store"), true for all simulated data;
        /// nullopt for no such option.
        std::optional<bool> saveSimulationData;
    };

    struct SaveFileAnswer
    {
        std::filesystem::path file;
        std::filesystem::path directory;
        /// The storage option chosen, when the request had one; the caller stores it in the
        /// document's storage options and marks them explicitly set.
        std::optional<bool> saveSimulationData;
    };

    // ------------------------------------------------------------------ the questions

    UserPrompts()                              = default;
    virtual ~UserPrompts()                     = default;
    UserPrompts(const UserPrompts&)            = delete;
    UserPrompts& operator=(const UserPrompts&) = delete;
    UserPrompts(UserPrompts&&)                 = delete;
    UserPrompts& operator=(UserPrompts&&)      = delete;

    /// Where the new component goes (ComponentAddButtons.askPosition), with "Do not ask me
    /// again" and "You can change the default operation in the preferences.".
    [[nodiscard]] virtual InsertPositionAnswer askInsertPosition(
        QWidget* parent, InsertPositionQuestion question) = 0;

    /// Whether Cancel discards the changes (RocketComponentConfig, PreferencesDialog), with
    /// "Do not ask me again".
    [[nodiscard]] virtual DiscardAnswer askDiscardChanges(QWidget*        parent,
                                                          DiscardQuestion question) = 0;

    /// Whether to save the design named @p designName before its window closes
    /// (BasicFrame.closeAction).
    [[nodiscard]] virtual SaveChangesAnswer askSaveChanges(QWidget*       parent,
                                                           const QString& designName) = 0;

    /// Whether to overwrite the existing file named @p fileName (FileHelper.confirmWrite,
    /// "error.fileExists.*"). True for yes.
    [[nodiscard]] virtual bool askOverwrite(QWidget* parent, const QString& fileName) = 0;

    /// The storage options before a save whose options are not set explicitly
    /// (StorageOptionChooser.verifyStorageOptions): true for all simulated data, false for the
    /// summary only, nullopt when cancelled. @p saveSimulationData is the current option.
    [[nodiscard]] virtual std::optional<bool> askStorageOptions(QWidget* parent,
                                                                bool     saveSimulationData) = 0;

    /// A yes/no question; true for yes.
    [[nodiscard]] virtual bool askYesNo(QWidget* parent, const Question& question) = 0;

    /// A decal's source file that is missing (DecalNotFoundDialog): asks whether to look for
    /// it, with @p message (the Error's text, rich text), and if so the file, starting in
    /// @p directory. nullopt when the user does not look for it or cancels the file dialog.
    [[nodiscard]] virtual std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) = 0;

    /// The files to open; nullopt when cancelled.
    [[nodiscard]] virtual std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) = 0;

    /// The file to save to; nullopt when cancelled.
    [[nodiscard]] virtual std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) = 0;

    /// The "Save Design Info" dialog before a Save As (BasicFrame.showSaveRocketInfoDialog):
    /// the rocket's designer, revision and comment of @p document, edited in place, with
    /// "Don't show this dialog again".
    virtual void editDesignInformation(QWidget* parent, OpenRocketDocument& document) = 0;

    // ------------------------------------------------------------------ the messages

    virtual void showMessage(QWidget* parent, const Message& message) = 0;

    virtual void showWarnings(QWidget* parent, const WarningsMessage& message) = 0;

    /// The internal error dialog (OpenRocket's SwingExceptionHandler.showDialog(), in QtRocket's
    /// words: dialogs/InternalErrorDialog) for a failure the error hook (Guard) was given, with
    /// Continue and Quit. The application's error hook shows it (app/ApplicationSession) and
    /// acts on the answer.
    [[nodiscard]] virtual InternalErrorAnswer showInternalError(QWidget*              parent,
                                                                const Guard::Failure& failure) = 0;
};

}  // namespace QtRocket::Gui
