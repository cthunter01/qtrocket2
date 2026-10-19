#include "QtRocket/gui/main/WidgetUserPrompts.h"

#include <filesystem>
#include <optional>

#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/dialogs/InternalErrorDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

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

UserPrompts::InsertPositionAnswer WidgetUserPrompts::askInsertPosition(
    QWidget* /*parent*/, InsertPositionQuestion /*question*/)
{
    reportNotAvailable(QStringLiteral("askInsertPosition"));
    return {};
}

UserPrompts::DiscardAnswer WidgetUserPrompts::askDiscardChanges(QWidget* /*parent*/,
                                                                DiscardQuestion /*question*/)
{
    reportNotAvailable(QStringLiteral("askDiscardChanges"));
    return {};
}

UserPrompts::SaveChangesAnswer WidgetUserPrompts::askSaveChanges(QWidget*       parent,
                                                                 const QString& designName)
{
    // JOptionPane.showConfirmDialog(YES_NO_CANCEL_OPTION, QUESTION_MESSAGE); the design's name
    // is plain text, as in the Swing label.
    QMessageBox box(QMessageBox::Question,
                    //: BasicFrame.dlg.title
                    tr("Design not saved"), saveChangesText(designName),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("saveChangesQuestion"));
    // Qt's own dialog: a platform theme's native one (KDE's) shows the text as rich text and
    // drops the default button.
    box.setOption(QMessageBox::Option::DontUseNativeDialog);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(QMessageBox::Yes);
    box.setEscapeButton(QMessageBox::Cancel);
    switch (box.exec())
    {
        case QMessageBox::Yes:
            return SaveChangesAnswer::SAVE;
        case QMessageBox::No:
            return SaveChangesAnswer::DISCARD;
        default:
            return SaveChangesAnswer::CANCEL;
    }
}

bool WidgetUserPrompts::askOverwrite(QWidget* /*parent*/, const QString& /*fileName*/)
{
    reportNotAvailable(QStringLiteral("askOverwrite"));
    return false;
}

std::optional<bool> WidgetUserPrompts::askStorageOptions(QWidget* /*parent*/,
                                                         bool /*saveSimulationData*/)
{
    reportNotAvailable(QStringLiteral("askStorageOptions"));
    return std::nullopt;
}

bool WidgetUserPrompts::askYesNo(QWidget* /*parent*/, const Question& /*question*/)
{
    reportNotAvailable(QStringLiteral("askYesNo"));
    return false;
}

std::optional<UserPrompts::OpenFileAnswer> WidgetUserPrompts::askDecalFile(
    QWidget* /*parent*/, const QString& /*message*/, const std::filesystem::path& /*directory*/)
{
    reportNotAvailable(QStringLiteral("askDecalFile"));
    return std::nullopt;
}

std::optional<UserPrompts::OpenFileAnswer> WidgetUserPrompts::askOpenFiles(
    QWidget* /*parent*/, const OpenFileRequest& /*request*/)
{
    reportNotAvailable(QStringLiteral("askOpenFiles"));
    return std::nullopt;
}

std::optional<UserPrompts::SaveFileAnswer> WidgetUserPrompts::askSaveFile(
    QWidget* /*parent*/, const SaveFileRequest& /*request*/)
{
    reportNotAvailable(QStringLiteral("askSaveFile"));
    return std::nullopt;
}

void WidgetUserPrompts::editDesignInformation(QWidget* /*parent*/, OpenRocketDocument& /*document*/)
{
    reportNotAvailable(QStringLiteral("editDesignInformation"));
}

void WidgetUserPrompts::showMessage(QWidget* parent, const Message& message)
{
    DetailDialog::showDetailedMessageDialog(parent, message.lines, std::nullopt, message.title,
                                            message.severity);
}

void WidgetUserPrompts::showWarnings(QWidget* /*parent*/, const WarningsMessage& /*message*/)
{
    reportNotAvailable(QStringLiteral("showWarnings"));
}

UserPrompts::InternalErrorAnswer WidgetUserPrompts::showInternalError(QWidget*              parent,
                                                                      const Guard::Failure& failure)
{
    return InternalErrorDialog::ask(parent, failure);
}

void WidgetUserPrompts::reportNotAvailable(const QString& question)
{
    Guard::reportErrorCondition(
        QStringLiteral("UserPrompts::%1() is not available yet: its dialog comes later in "
                       "Milestone 2")
            .arg(question));
}

}  // namespace QtRocket::Gui
