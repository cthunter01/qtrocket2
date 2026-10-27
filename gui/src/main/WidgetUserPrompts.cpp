#include "QtRocket/gui/main/WidgetUserPrompts.h"

#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLayout>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/DecalNotFoundDialog.h"
#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/dialogs/ErrorWarningDialog.h"
#include "QtRocket/gui/dialogs/InternalErrorDialog.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"
#include "QtRocket/gui/dialogs/StorageOptionsDialog.h"
#include "QtRocket/gui/dialogs/WarningDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"
#include "QtRocket/gui/main/SaveFileNames.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The rows of the description beside the files (FreeformFinSetConfig: DescriptionArea(text, 5,
/// 0)) and its least width ("wmin 100lp").
constexpr int kDescriptionRows     = 5;
constexpr int kDescriptionMinWidth = 100;
/// The size of the small note of the insert position question (StyledLabel(text, -2)).
constexpr double kNoteSize = -2.0;

/// Whether the question's parent was destroyed while a dialog of it was open: it had one and its
/// pointer is null now.
[[nodiscard]] bool parentGone(const QWidget* parent, const QPointer<QWidget>& guard) noexcept
{
    return parent != nullptr && guard.isNull();
}

/// The directory a file dialog starts in: @p directory, or the user's home directory without one
/// (JFileChooser's default directory, for a null current directory).
[[nodiscard]] QString startDirectory(const std::filesystem::path& directory)
{
    return directory.empty() ? QDir::homePath() : QtStrings::fromPath(directory);
}

/// Puts @p description beside the files of @p dialog (a JFileChooser accessory), which is then
/// Qt's own file dialog.
void addDescription(QFileDialog& dialog, const QString& description)
{
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    auto* area = makeChild<DescriptionArea>(&dialog, description, kDescriptionRows, 0.0);
    area->setObjectName(QStringLiteral("description"));
    area->setMinimumWidth(kDescriptionMinWidth);
    if (auto* grid = qobject_cast<QGridLayout*>(dialog.layout()); grid != nullptr)
    {
        grid->addWidget(area, 0, grid->columnCount(), grid->rowCount(), 1);
    }
    else if (QLayout* const layout = dialog.layout(); layout != nullptr)
    {
        layout->addWidget(area);
    }
}

/// The files @p chooser was accepted with and the directory it was in, or nullopt for none.
[[nodiscard]] std::optional<UserPrompts::OpenFileAnswer> acceptedFiles(const QFileDialog& chooser)
{
    UserPrompts::OpenFileAnswer answer;
    // With the platform's separators, as Java's File has them: Qt names files with '/' on
    // Windows too.
    for (const QString& file : chooser.selectedFiles())
    {
        answer.files.push_back(QtStrings::toPath(QDir::toNativeSeparators(file)));
    }
    if (answer.files.empty())
    {
        return std::nullopt;
    }
    answer.directory =
        QtStrings::toPath(QDir::toNativeSeparators(chooser.directory().absolutePath()));
    return answer;
}

/// The files @p dialog was accepted with and the directory it was in, or nullopt when it was not
/// accepted (or destroyed).
[[nodiscard]] std::optional<UserPrompts::OpenFileAnswer> chosenFiles(
    ModalDialog<QFileDialog>& dialog)
{
    if (dialog.exec() != QDialog::Accepted || dialog.get() == nullptr)
    {
        return std::nullopt;
    }
    return acceptedFiles(*dialog.get());
}

/// A file chooser in OPEN mode (JFileChooser.showOpenDialog()) for @p request.
[[nodiscard]] std::optional<UserPrompts::OpenFileAnswer> chooseFilesToOpen(
    QWidget* parent, const UserPrompts::OpenFileRequest& request, const QString& objectName)
{
    ModalDialog<QFileDialog> dialog(parent);
    QFileDialog&             chooser = *dialog.get();
    chooser.setObjectName(objectName);
    if (!request.title.isEmpty())
    {
        chooser.setWindowTitle(request.title);
    }
    chooser.setAcceptMode(QFileDialog::AcceptOpen);
    chooser.setFileMode(request.multipleFiles ? QFileDialog::ExistingFiles
                                              : QFileDialog::ExistingFile);
    if (!request.nameFilters.isEmpty())
    {
        chooser.setNameFilters(request.nameFilters);
        chooser.selectNameFilter(request.selectedNameFilter.isEmpty() ? request.nameFilters.front()
                                                                      : request.selectedNameFilter);
    }
    chooser.setDirectory(startDirectory(request.directory));
    if (!request.approveButtonText.isEmpty())
    {
        chooser.setLabelText(QFileDialog::Accept, request.approveButtonText);
    }
    if (!request.description.isEmpty())
    {
        addDescription(chooser, request.description);
    }
    return chosenFiles(dialog);
}

/// Whether the platform's own save dialog asks itself before it replaces a file, whatever the
/// dialog is told: macOS's save panel (QFileDialog::DontConfirmOverwrite is not supported there).
[[nodiscard]] constexpr bool platformSaveDialogConfirmsOverwrite() noexcept
{
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

/// A file chooser in SAVE mode (JFileChooser.showSaveDialog()) for @p request, starting in
/// @p directory with the name @p fileName proposed and the name filter @p nameFilter chosen (the
/// first for an empty one): the file, the directory and the name filter it was accepted with
/// (no storage option yet), or nullopt when it was not accepted (or destroyed).
[[nodiscard]] std::optional<UserPrompts::SaveFileAnswer> chooseFileToSave(
    QWidget* parent, const UserPrompts::SaveFileRequest& request,
    const std::filesystem::path& directory, const QString& fileName, const QString& nameFilter)
{
    ModalDialog<QFileDialog> dialog(parent);
    QFileDialog&             chooser = *dialog.get();
    chooser.setObjectName(QStringLiteral("saveFileDialog"));
    // The caller forces the extension and then asks askOverwrite(): the dialog must not ask
    // first, so on macOS, whose save panel always asks, it is Qt's own dialog (as Swing's
    // JFileChooser is no platform dialog either).
    chooser.setOption(QFileDialog::DontUseNativeDialog, platformSaveDialogConfirmsOverwrite());
    chooser.setOption(QFileDialog::DontConfirmOverwrite, true);
    if (!request.title.isEmpty())
    {
        chooser.setWindowTitle(request.title);
    }
    chooser.setAcceptMode(QFileDialog::AcceptSave);
    chooser.setFileMode(QFileDialog::AnyFile);
    if (!request.nameFilters.isEmpty())
    {
        chooser.setNameFilters(request.nameFilters);
        chooser.selectNameFilter(nameFilter.isEmpty() ? request.nameFilters.front() : nameFilter);
    }
    chooser.setDirectory(startDirectory(directory));
    if (!fileName.isEmpty())
    {
        chooser.selectFile(fileName);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.get() == nullptr)
    {
        return std::nullopt;
    }
    const QFileDialog&                               accepted = *dialog.get();
    const std::optional<UserPrompts::OpenFileAnswer> files    = acceptedFiles(accepted);
    if (!files.has_value())
    {
        return std::nullopt;
    }
    return UserPrompts::SaveFileAnswer{.file               = files->files.front(),
                                       .directory          = files->directory,
                                       .saveSimulationData = std::nullopt,
                                       .selectedNameFilter = request.nameFilters.isEmpty()
                                                                 ? QString()
                                                                 : accepted.selectedNameFilter()};
}

/// A question with Yes and No (JOptionPane.showConfirmDialog(YES_NO_OPTION)) of @p type, titled
/// @p title, with @p text: whether Yes closed it.
[[nodiscard]] bool askYesOrNo(QWidget* parent, OptionDialog::MessageType type, const QString& title,
                              const QString& text, const QString& objectName)
{
    ModalDialog<OptionDialog> dialog(parent, type, title);
    OptionDialog&             question = *dialog.get();
    question.setObjectName(objectName);
    question.addMessageLine(text);
    QPushButton* const yes = question.addOption(QDialogButtonBox::Yes);
    question.addOption(QDialogButtonBox::No);
    question.setInitialOption(yes);
    dialog.exec();
    const OptionDialog* const answered = dialog.get();
    return answered != nullptr && answered->chosenOption() == yes;
}

/// The title and the message of @p question (ComponentAddButtons.askPosition()).
struct InsertPositionTexts
{
    QString title;
    QString message;
};

[[nodiscard]] InsertPositionTexts insertPositionTexts(UserPrompts::InsertPositionQuestion question)
{
    switch (question)
    {
        case UserPrompts::InsertPositionQuestion::BODY_COMPONENT:
            return {//: compaddbuttons.Selectcomppos
                    .title = WidgetUserPrompts::tr("Select component position"),
                    //: compaddbuttons.lbl.insertcomp
                    .message = WidgetUserPrompts::tr("Insert the component after the current "
                                                     "component or as the last component?")};
        case UserPrompts::InsertPositionQuestion::STAGE:
            return {//: compaddbuttons.Selectstagepos
                    .title = WidgetUserPrompts::tr("Select stage position"),
                    //: compaddbuttons.lbl.insertstage
                    .message = WidgetUserPrompts::tr(
                        "Insert the stage after the current stage or as the last stage?")};
    }
    QTROCKET_UNREACHABLE();
}

/// The title, the message and the check box's text of @p question
/// (RocketComponentConfig.createCancelOperationContent(), PreferencesDialog's).
struct DiscardTexts
{
    QString title;
    QString message;
    QString doNotAskAgain;
};

[[nodiscard]] DiscardTexts discardTexts(UserPrompts::DiscardQuestion question)
{
    switch (question)
    {
        case UserPrompts::DiscardQuestion::COMPONENT_CHANGES:
            return {
                //: RocketCompCfg.CancelOperation.title
                .title = WidgetUserPrompts::tr("Cancel operation"),
                //: RocketCompCfg.CancelOperation.msg.discardChanges
                .message = WidgetUserPrompts::tr("<html>Are you sure you want to <b>discard</b> "
                                                 "your <b>changes</b> to this component?</html>"),
                //: RocketCompCfg.CancelOperation.checkbox.dontAskAgain
                .doNotAskAgain = WidgetUserPrompts::tr("Don't ask me again")};
        case UserPrompts::DiscardQuestion::COMPONENT_ADDITION:
            return {//: RocketCompCfg.CancelOperation.title
                    .title = WidgetUserPrompts::tr("Cancel operation"),
                    //: RocketCompCfg.CancelOperation.msg.undoAdd
                    .message = WidgetUserPrompts::tr(
                        "<html>Are you sure you want to <b>undo adding</b> this component?</html>"),
                    //: RocketCompCfg.CancelOperation.checkbox.dontAskAgain
                    .doNotAskAgain = WidgetUserPrompts::tr("Don't ask me again")};
        case UserPrompts::DiscardQuestion::PREFERENCE_CHANGES:
            return {
                //: PreferencesDialog.CancelOperation.title
                .title = WidgetUserPrompts::tr("Discard Preference Changes"),
                //: PreferencesDialog.CancelOperation.msg.discardChanges
                .message = WidgetUserPrompts::tr("<html>Are you sure you want to <b>discard the "
                                                 "preference changes</b>?</html>"),
                //: SimulationConfigDialog.CancelOperation.checkbox.dontAskAgain
                .doNotAskAgain = WidgetUserPrompts::tr("Don't ask me again")};
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

QString WidgetUserPrompts::saveChangesText(const QString& designName)
{
    // BasicFrame.closeAction(): lbl1 + name + lbl2 + "  " + lbl3.
    //: BasicFrame.dlg.lbl1
    return tr("Design '") + designName +
           //: BasicFrame.dlg.lbl2
           tr("' has not been saved.") + QStringLiteral("  ") +
           //: BasicFrame.dlg.lbl3
           tr("Do you want to save it?");
}

QString WidgetUserPrompts::overwriteText(const QString& fileName)
{
    // FileHelper.confirmWrite(): L10N.replace(desc, "{filename}", name), here a plain
    // replacement (see the class comment).
    //: error.fileExists.desc
    QString text = tr("File '{filename}' exists.  Do you want to overwrite it?");
    return text.replace(QStringLiteral("{filename}"), fileName);
}

QString WidgetUserPrompts::illegalFileNameText(const QString& fileName, QChar character)
{
    // String.format(message, fileName, c): both put in at once, so that a '%' of the name stays.
    //: SaveAsFileChooser.illegalFilename.message
    return tr("The filename '%1' may not contain the character ' %2 '. Please remove it.")
        .arg(fileName, QString(character));
}

UserPrompts::InsertPositionAnswer WidgetUserPrompts::askInsertPosition(
    QWidget* parent, InsertPositionQuestion question)
{
    const InsertPositionTexts texts = insertPositionTexts(question);
    // JOptionPane.showOptionDialog(DEFAULT_OPTION, QUESTION_MESSAGE) with the options and a
    // panel of the check box and the note under the message.
    ModalDialog<OptionDialog> dialog(parent, OptionDialog::MessageType::QUESTION, texts.title);
    OptionDialog&             asking = *dialog.get();
    asking.setObjectName(QStringLiteral("insertPositionQuestion"));
    asking.addMessageLine(texts.message);
    //: compaddbuttons.Donotaskmeagain
    auto* check = makeChild<QCheckBox>(asking.messageArea(), tr("Do not ask me again"));
    check->setObjectName(QStringLiteral("doNotAskAgain"));
    asking.addMessageWidget(check);
    auto* note = makeChild<StyledLabel>(
        asking.messageArea(),
        //: compaddbuttons.lbl.Youcanchange
        tr("You can change the default operation in the preferences."), kNoteSize);
    note->setObjectName(QStringLiteral("note"));
    asking.addMessageWidget(note);
    //: compaddbuttons.askPosition.Inserthere
    QPushButton* const insert = asking.addOption(tr("Insert here"), QDialogButtonBox::AcceptRole);
    const QPushButton* const addToEnd =
        //: compaddbuttons.askPosition.Addtotheend
        asking.addOption(tr("Add to the end"), QDialogButtonBox::AcceptRole);
    //: compaddbuttons.askPosition.Cancel
    asking.addOption(tr("Cancel"), QDialogButtonBox::RejectRole);
    asking.setInitialOption(insert);
    dialog.exec();

    const OptionDialog* const answered = dialog.get();
    if (answered == nullptr)
    {
        return {};
    }
    InsertPositionAnswer answer;
    if (answered->chosenOption() == insert)
    {
        answer.position = InsertPosition::INSERT_HERE;
    }
    else if (answered->chosenOption() == addToEnd)
    {
        answer.position = InsertPosition::ADD_TO_END;
    }
    answer.doNotAskAgain = check->isChecked();
    return answer;
}

UserPrompts::DiscardAnswer WidgetUserPrompts::askDiscardChanges(QWidget*        parent,
                                                                DiscardQuestion question)
{
    const DiscardTexts texts = discardTexts(question);
    // JOptionPane.showConfirmDialog(YES_NO_OPTION, WARNING_MESSAGE) of the panel of the message
    // and the check box ("gaptop para").
    ModalDialog<OptionDialog> dialog(parent, OptionDialog::MessageType::WARNING, texts.title);
    OptionDialog&             asking = *dialog.get();
    asking.setObjectName(QStringLiteral("discardChangesQuestion"));
    asking.addMessageLine(texts.message);
    asking.addMessageGap();
    auto* check = makeChild<QCheckBox>(asking.messageArea(), texts.doNotAskAgain);
    check->setObjectName(QStringLiteral("dontAskAgain"));
    asking.addMessageWidget(check);
    QPushButton* const yes = asking.addOption(QDialogButtonBox::Yes);
    asking.addOption(QDialogButtonBox::No);
    asking.setInitialOption(yes);
    dialog.exec();

    const OptionDialog* const answered = dialog.get();
    if (answered == nullptr)
    {
        return {};
    }
    return {.discard = answered->chosenOption() == yes, .doNotAskAgain = check->isChecked()};
}

UserPrompts::SaveChangesAnswer WidgetUserPrompts::askSaveChanges(QWidget*       parent,
                                                                 const QString& designName)
{
    // JOptionPane.showConfirmDialog(YES_NO_CANCEL_OPTION, QUESTION_MESSAGE); the design's name
    // is plain text, as in the Swing label.
    ModalDialog<QMessageBox> dialog(parent, QMessageBox::Question,
                                    //: BasicFrame.dlg.title
                                    tr("Design not saved"), saveChangesText(designName),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    QMessageBox&             box = *dialog.get();
    box.setObjectName(QStringLiteral("saveChangesQuestion"));
    // Qt's own dialog: a platform theme's native one (KDE's) shows the text as rich text and
    // drops the default button.
    box.setOption(QMessageBox::Option::DontUseNativeDialog);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(QMessageBox::Yes);
    box.setEscapeButton(QMessageBox::Cancel);
    switch (dialog.exec())
    {
        case QMessageBox::Yes:
            return SaveChangesAnswer::SAVE;
        case QMessageBox::No:
            return SaveChangesAnswer::DISCARD;
        default:
            return SaveChangesAnswer::CANCEL;
    }
}

bool WidgetUserPrompts::askOverwrite(QWidget* parent, const QString& fileName)
{
    // JOptionPane.showConfirmDialog(YES_NO_OPTION, QUESTION_MESSAGE).
    return askYesOrNo(parent, OptionDialog::MessageType::QUESTION,
                      //: error.fileExists.title
                      tr("File exists"), overwriteText(fileName),
                      QStringLiteral("overwriteQuestion"));
}

std::optional<bool> WidgetUserPrompts::askStorageOptions(QWidget* parent, bool saveSimulationData)
{
    return StorageOptionsDialog::ask(parent, saveSimulationData);
}

bool WidgetUserPrompts::askYesNo(QWidget* parent, const Question& question)
{
    return askYesOrNo(parent, OptionDialog::messageType(question.severity), question.title,
                      question.text, QStringLiteral("yesNoQuestion"));
}

std::optional<UserPrompts::OpenFileAnswer> WidgetUserPrompts::askDecalFile(
    QWidget* parent, const QString& message, const std::filesystem::path& directory)
{
    const QPointer<QWidget> guard(parent);
    if (!DecalNotFoundDialog::askToLookForFile(parent, message) || parentGone(parent, guard))
    {
        return std::nullopt;
    }
    // DecalNotFoundDialog: a JFileChooser in OPEN mode, without filters, in the default
    // directory.
    OpenFileRequest request;
    request.directory = directory;
    return chooseFilesToOpen(parent, request, QStringLiteral("decalFileDialog"));
}

std::optional<UserPrompts::OpenFileAnswer> WidgetUserPrompts::askOpenFiles(
    QWidget* parent, const OpenFileRequest& request)
{
    if (request.selection == OpenFileSelection::FILES_AND_DIRECTORIES)
    {
        // Run 3 of Milestone 3 (the preferences' user files): the menu "File..." /
        // "Directory..." and the platform's dialog for the choice. Until then: cancelled.
        return std::nullopt;
    }
    return chooseFilesToOpen(parent, request, QStringLiteral("openFileDialog"));
}

std::optional<UserPrompts::SaveFileAnswer> WidgetUserPrompts::askSaveFile(
    QWidget* parent, const SaveFileRequest& request)
{
    const QPointer<QWidget>       guard(parent);
    std::optional<SaveFileAnswer> chosen = askLegalFileToSave(parent, request);
    if (!chosen.has_value())
    {
        return std::nullopt;
    }
    SaveFileAnswer answer = std::move(*chosen);
    if (request.saveSimulationData.has_value())
    {
        // The StorageOptionChooser accessory, asked once the file is chosen.
        if (parentGone(parent, guard))
        {
            return std::nullopt;
        }
        answer.saveSimulationData = StorageOptionsDialog::ask(parent, *request.saveSimulationData);
        if (!answer.saveSimulationData.has_value())
        {
            return std::nullopt;
        }
    }
    return answer;
}

std::optional<UserPrompts::SaveFileAnswer> WidgetUserPrompts::askLegalFileToSave(
    QWidget* parent, const SaveFileRequest& request)
{
    const QPointer<QWidget> guard(parent);
    std::filesystem::path   directory = request.directory;
    QString                 fileName  = request.fileName;
    QString                 nameFilter;
    while (true)
    {
        std::optional<SaveFileAnswer> chosen =
            chooseFileToSave(parent, request, directory, fileName, nameFilter);
        if (!chosen.has_value() || !request.refuseIllegalNames)
        {
            return chosen;
        }
        // SaveFileChooser.approveSelection(): the name as the chooser reads it
        // (getFilenameInput(), relative to the directory the dialog ended in).
        const QString name = SaveFileNames::filenameInput(chosen->file, chosen->directory);
        const std::optional<QChar> illegal = SaveFileNames::illegalCharacter(name);
        if (!illegal.has_value())
        {
            return chosen;
        }
        // showIllegalFilenameWarning(); Java's chooser stays open: here it is shown again, in its
        // directory with the name and the name filter chosen.
        showMessage(parent, {.severity = Severity::WARNING,
                             //: SaveAsFileChooser.illegalFilename.title
                             .title = tr("Illegal filename"),
                             .lines = {illegalFileNameText(name, *illegal)}});
        if (parentGone(parent, guard))
        {
            return std::nullopt;
        }
        directory  = chosen->directory;
        fileName   = name;
        nameFilter = chosen->selectedNameFilter;
    }
}

void WidgetUserPrompts::editDesignInformation(QWidget* parent, const GuiServices& services,
                                              OpenRocketDocument& document)
{
    // BasicFrame.saveAsAction() goes on to the file chooser however the dialog closed.
    static_cast<void>(SaveDesignInfoDialog::edit(parent, services, document));
}

std::optional<UserPrompts::IgnitionChoice> WidgetUserPrompts::askIgnition(
    QWidget* /*parent*/, const IgnitionRequest& /*request*/)
{
    // Run 2 of Milestone 3: IgnitionSelectionDialog. Until then: cancelled.
    return std::nullopt;
}

std::optional<UserPrompts::DeploymentChoice> WidgetUserPrompts::askDeployment(
    QWidget* /*parent*/, const DeploymentRequest& /*request*/)
{
    // Run 2 of Milestone 3: DeploymentSelectionDialog. Until then: cancelled.
    return std::nullopt;
}

std::optional<UserPrompts::SeparationChoice> WidgetUserPrompts::askSeparation(
    QWidget* /*parent*/, const SeparationRequest& /*request*/)
{
    // Run 2 of Milestone 3: SeparationSelectionDialog. Until then: cancelled.
    return std::nullopt;
}

UserPrompts::RenameConfigurationAnswer WidgetUserPrompts::askRenameConfiguration(
    QWidget* /*parent*/, const RenameConfigurationRequest& /*request*/)
{
    // Run 2 of Milestone 3: RenameConfigDialog. Until then: cancelled.
    return {.action = RenameAction::CANCEL, .name = QString()};
}

std::optional<UserPrompts::MotorChoice> WidgetUserPrompts::chooseMotor(
    QWidget* parent, const GuiServices& services, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    // MotorChooserDialog, one per top-level window (its child, made at its first question), so
    // that its search, filters, sort and selection survive between questions as in OpenRocket
    // (MotorConfigurationPanel keeps one); without a parent, one for this question alone.
    return MotorChooserDialog::choose(parent, services, mount, configurationId);
}

UserPrompts::PresetAnswer WidgetUserPrompts::choosePreset(QWidget* /*parent*/,
                                                          const GuiServices& /*services*/,
                                                          const PresetRequest& /*request*/)
{
    // Run 2 of Milestone 3: ComponentPresetChooserDialog. Until then: closed without a choice.
    return {.outcome = PresetOutcome::CLOSED, .preset = nullptr};
}

void WidgetUserPrompts::showMessage(QWidget* parent, const Message& message)
{
    DetailDialog::showDetailedMessageDialog(parent, message.lines, std::nullopt, message.title,
                                            message.severity);
}

void WidgetUserPrompts::showWarnings(QWidget* parent, const WarningsMessage& message)
{
    WarningDialog::showWarnings(parent, message.lines, message.title, message.warnings);
}

void WidgetUserPrompts::showErrorsAndWarnings(QWidget*                        parent,
                                              const ErrorsAndWarningsMessage& message)
{
    ErrorWarningDialog::showErrorsAndWarnings(parent, message.lines, message.title, message.errors,
                                              message.warnings);
}

void WidgetUserPrompts::showDatabaseProblems(QWidget* /*parent*/,
                                             std::span<const DatabaseProblem> /*problems*/)
{
    // Run 3 of Milestone 3 (the start-up): one message with OpenRocket's MotorDbLoaderDlg and
    // ComponentDbLoaderDlg texts. Until then: nothing is shown.
}

UserPrompts::InternalErrorAnswer WidgetUserPrompts::showInternalError(QWidget*              parent,
                                                                      const Guard::Failure& failure)
{
    return InternalErrorDialog::ask(parent, failure);
}

}  // namespace QtRocket::Gui
