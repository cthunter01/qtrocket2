#pragma once

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/OptionDialog.h"

class QWidget;

namespace QtRocket::Gui
{

class MessageList;
class StyledLabel;

/// OpenRocket's ErrorWarningDialog.showErrorsAndWarnings(): a warning message
/// (JOptionPane.showMessageDialog() with WARNING_MESSAGE and OK) whose message is the given
/// lines followed by the errors and the warnings of a save: the label "Errors" in the theme's
/// dark error colour (15 pixels above it), the list of the errors (one at a time selectable,
/// their texts in that colour), a separator, the label "Warnings:" and the list of the warnings
/// (MessageList each; a press below the last row clears the list's selection). The texts
/// "Errors" and "Warnings:" are OpenRocket's, written in its code. The file operations show it
/// after a save whose saver reported errors (UserPrompts::showErrorsAndWarnings(), "Errors/
/// Warnings while saving file").
///
/// Deviation: the dark error colour is ThemeColors::Key::DARK_ERROR of the application's
/// palette (the theme of its lightness), read when the dialog is made.
class ErrorWarningDialog final : public OptionDialog
{
    Q_OBJECT

public:
    /// The space above the "Errors" label (MigLayout "gaptop 15lp").
    static constexpr int kErrorsGapTop = 15;

    /// The dialog titled @p title with the @p message lines, the @p errors and the @p warnings,
    /// not shown yet.
    ErrorWarningDialog(const QStringList& message, const QString& title, const QStringList& errors,
                       const QStringList& warnings, QWidget* parent = nullptr);
    ~ErrorWarningDialog() override;
    ErrorWarningDialog(const ErrorWarningDialog&)            = delete;
    ErrorWarningDialog& operator=(const ErrorWarningDialog&) = delete;
    ErrorWarningDialog(ErrorWarningDialog&&)                 = delete;
    ErrorWarningDialog& operator=(ErrorWarningDialog&&)      = delete;

    /// Shows the dialog, modal, parented to @p parent (or none), and returns when it is closed.
    static void showErrorsAndWarnings(QWidget* parent, const QStringList& message,
                                      const QString& title, const QStringList& errors,
                                      const QStringList& warnings);

    /// The label "Errors" ("errorsLabel").
    [[nodiscard]] StyledLabel& errorsLabel() const noexcept { return *m_errorsLabel; }
    /// The list of the errors ("errors").
    [[nodiscard]] MessageList& errorList() const noexcept { return *m_errors; }
    /// The list of the warnings ("warnings").
    [[nodiscard]] MessageList& warningList() const noexcept { return *m_warnings; }

private:
    /// The panel of the errors and the warnings.
    QWidget*     m_content;
    StyledLabel* m_errorsLabel;
    MessageList* m_errors;
    MessageList* m_warnings;
};

}  // namespace QtRocket::Gui
