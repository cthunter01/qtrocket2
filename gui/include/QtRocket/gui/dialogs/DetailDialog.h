#pragma once

#include <optional>

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/UserPrompts.h"

class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QWidget;

namespace QtRocket::Gui
{

/// OpenRocket's DetailDialog.showDetailedMessageDialog(): a message box (the severity's icon,
/// the message, OK) whose message is followed by details in a read-only text area that can be
/// selected and copied: 5 rows of 40 columns, its font 2 points smaller than the dialog's,
/// shown from its start, scrolling when the details are larger. Without details it is a plain
/// message box.
///
/// The message is JOptionPane's array of lines: each line is a label of its own, plain text,
/// or rich text when it starts with "<html>" (Swing's rule for a label's text). The parts are
/// open to the dialogs built on this one (InternalErrorDialog, which brings its own buttons)
/// and to the tests.
class DetailDialog : public QDialog
{
    Q_OBJECT

public:
    /// The size of the details (JTextArea(5, 40)).
    static constexpr int kDetailRows    = 5;
    static constexpr int kDetailColumns = 40;
    /// How much smaller the details' font is, in points (GUIUtil.changeFontSize(textArea, -2)).
    static constexpr double kDetailFontDecrease = 2.0;

    /// The dialog titled @p title with the @p message lines and, unless nullopt, the
    /// @p details, its icon the one of @p severity. Not shown yet.
    DetailDialog(const QStringList& message, const std::optional<QString>& details,
                 const QString& title, UserPrompts::Severity severity, QWidget* parent = nullptr);
    ~DetailDialog() override;
    DetailDialog(const DetailDialog&)            = delete;
    DetailDialog& operator=(const DetailDialog&) = delete;
    DetailDialog(DetailDialog&&)                 = delete;
    DetailDialog& operator=(DetailDialog&&)      = delete;

    /// Shows the dialog, modal, parented to @p parent (or none), and returns when it is closed.
    static void showDetailedMessageDialog(QWidget* parent, const QStringList& message,
                                          const std::optional<QString>& details,
                                          const QString& title, UserPrompts::Severity severity);

    /// The labels of the message lines, in order.
    [[nodiscard]] const QList<QLabel*>& messageLabels() const noexcept { return m_messageLabels; }
    /// The texts of the message lines.
    [[nodiscard]] QStringList messageLines() const;
    /// The label with the severity's icon.
    [[nodiscard]] QLabel* iconLabel() const noexcept { return m_icon; }
    /// The details, or null without details.
    [[nodiscard]] QPlainTextEdit* detailsView() const noexcept { return m_details; }
    /// The buttons (OK): Accept closes the dialog with accept(), Reject with reject().
    [[nodiscard]] QDialogButtonBox* buttonBox() const noexcept { return m_buttons; }

private:
    QList<QLabel*>    m_messageLabels;
    QLabel*           m_icon{nullptr};
    QPlainTextEdit*   m_details{nullptr};
    QDialogButtonBox* m_buttons{nullptr};
};

}  // namespace QtRocket::Gui
