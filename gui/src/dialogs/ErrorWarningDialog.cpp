#include "QtRocket/gui/dialogs/ErrorWarningDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"

namespace QtRocket::Gui
{

ErrorWarningDialog::ErrorWarningDialog(const QStringList& message, const QString& title,
                                       const QStringList& errors, const QStringList& warnings,
                                       QWidget* parent)
  : OptionDialog(MessageType::WARNING, title, parent),
    m_content(makeChild<QWidget>(messageArea())),
    //: ErrorWarningDialog's text (not in messages.properties)
    m_errorsLabel(makeChild<StyledLabel>(m_content, tr("Errors"))),
    m_errors(makeChild<MessageList>(m_content, errors,
                                    ThemeColors::color(ThemeColors::Key::DARK_ERROR))),
    m_warnings(makeChild<MessageList>(m_content, warnings))
{
    setObjectName(QStringLiteral("errorWarningDialog"));
    addMessageLines(message);

    m_content->setObjectName(QStringLiteral("content"));
    auto* layout = makeChild<QVBoxLayout>(m_content);
    layout->setContentsMargins(0, 0, 0, 0);

    m_errorsLabel->setObjectName(QStringLiteral("errorsLabel"));
    m_errorsLabel->setFontColor(ThemeColors::color(ThemeColors::Key::DARK_ERROR));
    layout->addSpacing(kErrorsGapTop);
    layout->addWidget(m_errorsLabel);

    m_errors->setObjectName(QStringLiteral("errors"));
    m_errors->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_errors);

    auto* separator = makeChild<QFrame>(m_content);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    //: ErrorWarningDialog's text (not in messages.properties)
    auto* warningsLabel = makeChild<QLabel>(m_content, tr("Warnings:"));
    warningsLabel->setObjectName(QStringLiteral("warningsLabel"));
    layout->addWidget(warningsLabel);

    m_warnings->setObjectName(QStringLiteral("warnings"));
    layout->addWidget(m_warnings);

    addMessageWidget(m_content);
    setInitialOption(addOption(QDialogButtonBox::Ok));
}

ErrorWarningDialog::~ErrorWarningDialog() = default;

void ErrorWarningDialog::showErrorsAndWarnings(QWidget* parent, const QStringList& message,
                                               const QString& title, const QStringList& errors,
                                               const QStringList& warnings)
{
    ModalDialog<ErrorWarningDialog> dialog(parent, message, title, errors, warnings);
    dialog.exec();
}

}  // namespace QtRocket::Gui
