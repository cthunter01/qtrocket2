#pragma once

#include <filesystem>
#include <optional>

#include <QCoreApplication>
#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

/// The application's UserPrompts: OpenRocket's JOptionPane questions and its dialogs, modal,
/// parented to the window that asks (or to none).
///
/// Done so far (Milestone 2, run 1): what the application shows before the dialogs and file
/// operations exist:
/// - showMessage(): JOptionPane.showMessageDialog() with the lines of the message
///   (DetailDialog without details);
/// - showInternalError(): the internal error dialog (InternalErrorDialog);
/// - askSaveChanges(): BasicFrame.closeAction()'s question "Design '<name>' has not been saved.
///   Do you want to save it?", titled "Design not saved", with Yes (save), No (discard) and
///   Cancel (also Esc and the close button).
/// The other questions come with the dialogs and file operations of run 3 (the insert position,
/// discard, storage-option, decal, file, design-information and warnings dialogs): until then
/// each of them reports an error condition to the error hook (Guard) and gives the cancelling
/// answer (Cancel, No, no file).
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
    void editDesignInformation(QWidget* parent, OpenRocketDocument& document) override;
    void showMessage(QWidget* parent, const Message& message) override;
    void showWarnings(QWidget* parent, const WarningsMessage& message) override;
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override;

private:
    /// Reports that the question @p question is not available yet (see the class comment).
    static void reportNotAvailable(const QString& question);
};

}  // namespace QtRocket::Gui
