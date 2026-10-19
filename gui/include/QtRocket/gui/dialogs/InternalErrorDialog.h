#pragma once

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

/// The dialog of an internal error (OpenRocket's SwingExceptionHandler.showDialog(), in
/// QtRocket's words): what the error hook (Guard) was given, a BugError above all, shown as a
/// DetailDialog with the error's icon:
/// - the message: that QtRocket ran into an internal error, a bug; the failure's message (a
///   BugError's names its source file and line) and what the code was doing when it failed;
///   that the design may be inconsistent now, so it is best saved under a new name, and what
///   Quit does; and a request to report the bug with the details;
/// - the details, which can be selected and copied (or put on the clipboard with "Copy
///   details"): QtRocket's version, the Qt it runs with and was built with, the operating
///   system, and the failure (its kind, message, source file and line, function, context), in
///   English for the developers who read the report;
/// - the buttons "Continue" (the default; closing the dialog continues too) and "Quit QtRocket",
///   which ends the application at once, without saving any design (the document may be
///   inconsistent: saving it over its file could spoil the file).
///
/// Shown only through UserPrompts::showInternalError() (main/WidgetUserPrompts), so that a test
/// never shows it.
///
/// Deviations: OpenRocket's dialog offers a bug report ("View bug report", BugReportDialog, which
/// QtRocket gets in Milestone 9) or "Close", and cuts a message longer than 90 characters to 80;
/// this one offers Continue or Quit and shows the whole message. Its texts are QtRocket's own
/// (OpenRocket's are not in messages.properties). OpenRocket's dialogs for an out-of-memory
/// error and for a Java error that is no exception have no counterpart.
class InternalErrorDialog final : public DetailDialog
{
    Q_OBJECT

public:
    /// The dialog of @p failure, not shown yet.
    explicit InternalErrorDialog(const Guard::Failure& failure, QWidget* parent = nullptr);
    ~InternalErrorDialog() override;
    InternalErrorDialog(const InternalErrorDialog&)            = delete;
    InternalErrorDialog& operator=(const InternalErrorDialog&) = delete;
    InternalErrorDialog(InternalErrorDialog&&)                 = delete;
    InternalErrorDialog& operator=(InternalErrorDialog&&)      = delete;

    /// Shows the dialog of @p failure, modal, parented to @p parent (or none), and returns the
    /// user's answer.
    [[nodiscard]] static UserPrompts::InternalErrorAnswer ask(QWidget*              parent,
                                                              const Guard::Failure& failure);

    /// The message lines of the dialog of @p failure (see the class comment).
    [[nodiscard]] static QStringList messageFor(const Guard::Failure& failure);
    /// The details of @p failure (see the class comment), one item per line.
    [[nodiscard]] static QString detailsFor(const Guard::Failure& failure);

    /// The answer so far: QUIT once "Quit QtRocket" was clicked, else CONTINUE.
    [[nodiscard]] UserPrompts::InternalErrorAnswer answer() const noexcept { return m_answer; }

    [[nodiscard]] QPushButton* continueButton() const noexcept { return m_continue; }
    [[nodiscard]] QPushButton* copyButton() const noexcept { return m_copy; }
    [[nodiscard]] QPushButton* quitButton() const noexcept { return m_quit; }

private:
    QString                          m_details;
    UserPrompts::InternalErrorAnswer m_answer{UserPrompts::InternalErrorAnswer::CONTINUE};
    QPushButton*                     m_continue{nullptr};
    QPushButton*                     m_copy{nullptr};
    QPushButton*                     m_quit{nullptr};
};

}  // namespace QtRocket::Gui
