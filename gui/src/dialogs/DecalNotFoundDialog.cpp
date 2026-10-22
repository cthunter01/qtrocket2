#include "QtRocket/gui/dialogs/DecalNotFoundDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"

namespace QtRocket::Gui
{

DecalNotFoundDialog::DecalNotFoundDialog(const QString& message, QWidget* parent)
  : OptionDialog(MessageType::WARNING,
                 //: ExportDecalDialog.source.title
                 tr("No decal source file"), parent),
    m_yes(addOption(QDialogButtonBox::Yes)),
    m_no(addOption(QDialogButtonBox::No))
{
    setObjectName(QStringLiteral("decalNotFoundDialog"));
    // JOptionPane.showConfirmDialog(YES_NO_OPTION, WARNING_MESSAGE) of the HTML paragraph of
    // MessageWidthUtil.setMessageWidth(message, 400).
    QLabel* const label = addMessageLine(message);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setFixedWidth(kMessageWidth);
    setInitialOption(m_yes);
}

DecalNotFoundDialog::~DecalNotFoundDialog() = default;

bool DecalNotFoundDialog::askToLookForFile(QWidget* parent, const QString& message)
{
    ModalDialog<DecalNotFoundDialog> dialog(parent, message);
    dialog.exec();
    const DecalNotFoundDialog* const question = dialog.get();
    return question != nullptr && question->isYes();
}

bool DecalNotFoundDialog::isYes() const noexcept
{
    return chosenOption() == m_yes;
}

}  // namespace QtRocket::Gui
