#include "QtRocket/gui/dialogs/InternalErrorDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QSysInfo>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtVersion>

#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The kind of a failure for the details.
[[nodiscard]] QString kindText(Guard::Failure::Kind kind)
{
    switch (kind)
    {
        case Guard::Failure::Kind::BUG:
            return QStringLiteral("BugError (a broken invariant)");
        case Guard::Failure::Kind::EXCEPTION:
            return QStringLiteral("exception");
        case Guard::Failure::Kind::UNKNOWN:
            return QStringLiteral("unknown exception (no std::exception)");
        case Guard::Failure::Kind::ERROR_CONDITION:
            return QStringLiteral("error condition (no exception)");
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

InternalErrorDialog::InternalErrorDialog(const Guard::Failure& failure, QWidget* parent)
  : DetailDialog(messageFor(failure), detailsFor(failure),
                 // QtRocket's own text (OpenRocket's title "Uncaught exception" is no key).
                 tr("Internal error"), UserPrompts::Severity::ERROR, parent),
    m_details(detailsFor(failure))
{
    setObjectName(QStringLiteral("internalErrorDialog"));
    // The failure's message and context are never markup, and long lines wrap.
    for (QLabel* label : messageLabels())
    {
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
    }

    buttonBox()->setStandardButtons(QDialogButtonBox::NoButton);
    m_continue = buttonBox()->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    m_continue->setObjectName(QStringLiteral("continueButton"));
    m_continue->setDefault(true);
    m_copy = buttonBox()->addButton(tr("Copy details"), QDialogButtonBox::ActionRole);
    m_copy->setObjectName(QStringLiteral("copyButton"));
    m_quit = buttonBox()->addButton(tr("Quit QtRocket"), QDialogButtonBox::DestructiveRole);
    m_quit->setObjectName(QStringLiteral("quitButton"));
    connect(m_copy, &QPushButton::clicked, this,
            [this] { QGuiApplication::clipboard()->setText(m_details); });
    connect(m_quit, &QPushButton::clicked, this, [this] {
        m_answer = UserPrompts::InternalErrorAnswer::QUIT;
        accept();
    });
}

InternalErrorDialog::~InternalErrorDialog() = default;

UserPrompts::InternalErrorAnswer InternalErrorDialog::ask(QWidget*              parent,
                                                          const Guard::Failure& failure)
{
    // Made for the question, so that it survives its parent being destroyed while it is open
    // (the answer is then CONTINUE, as for a dialog that was closed).
    ModalDialog<InternalErrorDialog> dialog(parent, failure);
    dialog.exec();
    const InternalErrorDialog* const answered = dialog.get();
    return answered != nullptr ? answered->answer() : UserPrompts::InternalErrorAnswer::CONTINUE;
}

QStringList InternalErrorDialog::messageFor(const Guard::Failure& failure)
{
    // QtRocket's own texts (OpenRocket's are not in messages.properties).
    QStringList lines{tr("QtRocket ran into an internal error. This is a bug in QtRocket."),
                      failure.message};
    if (!failure.context.isEmpty())
    {
        lines.append(tr("In: %1").arg(failure.context));
    }
    lines.append(
        tr("You can continue, but the design may be in an inconsistent state now: save "
           "it under a new name (File > Save as) rather than over its file. Quit QtRocket "
           "ends QtRocket at once, without saving any design."));
    lines.append(
        tr("Please report this bug to the developers of QtRocket, with the details "
           "below."));
    return lines;
}

QString InternalErrorDialog::detailsFor(const Guard::Failure& failure)
{
    // In English, for the developers who read the report.
    QStringList lines{
        BuildInfo::versionText(),
        QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion())),
        QStringLiteral("%1 (%2 %3, %4)")
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(),
                 QSysInfo::currentCpuArchitecture()),
        QString(),
        QStringLiteral("Kind: ") + kindText(failure.kind),
        QStringLiteral("Message: ") + failure.message,
    };
    if (!failure.file.isEmpty())
    {
        lines.append(QStringLiteral("Source: %1, line %2").arg(failure.file).arg(failure.line));
    }
    if (!failure.function.isEmpty())
    {
        lines.append(QStringLiteral("Function: ") + failure.function);
    }
    if (!failure.context.isEmpty())
    {
        lines.append(QStringLiteral("In: ") + failure.context);
    }
    return lines.join(QStringLiteral("\n"));
}

}  // namespace QtRocket::Gui
