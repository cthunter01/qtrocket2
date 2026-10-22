#include "QtRocket/gui/dialogs/WarningDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

WarningDialog::WarningDialog(const QStringList& message, const QString& title,
                             const QStringList& warnings, QWidget* parent)
  : OptionDialog(MessageType::WARNING, title, parent),
    m_warnings(makeChild<MessageList>(messageArea(), warnings))
{
    setObjectName(QStringLiteral("warningDialog"));
    addMessageLines(message);
    m_warnings->setObjectName(QStringLiteral("warnings"));
    addMessageWidget(m_warnings);
    setInitialOption(addOption(QDialogButtonBox::Ok));
}

WarningDialog::~WarningDialog() = default;

void WarningDialog::showWarnings(QWidget* parent, const QStringList& message, const QString& title,
                                 const QStringList& warnings)
{
    ModalDialog<WarningDialog> dialog(parent, message, title, warnings);
    dialog.exec();
}

}  // namespace QtRocket::Gui
