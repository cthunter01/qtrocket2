#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/UserPrompts.h"

class QAbstractButton;
class QLabel;
class QPushButton;
class QShowEvent;
class QVBoxLayout;
class QWidget;

namespace QtRocket::Gui
{

/// The dialog of OpenRocket's JOptionPane questions and messages (showConfirmDialog(),
/// showOptionDialog(), showMessageDialog() with an array of message objects): the message type's
/// icon on the left, the message beside it, its parts one under the other (each text line a label
/// of its own, plain text, or rich text when it starts with "<html>", as Swing reads a label's
/// text; and the panels a question shows, such as a "Don't ask me again" check box), and the
/// options (buttons) below. A click on an option closes the dialog with that option chosen;
/// closing it otherwise (Esc, the close button) chooses none (JOptionPane.CLOSED_OPTION).
///
/// The questions of WidgetUserPrompts and the dialogs built on this one (WarningDialog,
/// ErrorWarningDialog, DecalNotFoundDialog, StorageOptionsDialog) make one with ModalDialog, so
/// that it survives the destruction of its parent.
///
/// Deviations: the options are a QDialogButtonBox in the platform's order (JOptionPane puts them
/// in the order given); the standard ones (Yes, No, OK, Cancel) have the platform's texts.
class OptionDialog : public QDialog
{
    Q_OBJECT

public:
    /// JOptionPane's message types, which choose the icon.
    enum class MessageType
    {
        /// No icon (PLAIN_MESSAGE).
        PLAIN,
        INFORMATION,
        WARNING,
        ERROR,
        QUESTION,
    };

    /// The message type of a UserPrompts severity.
    [[nodiscard]] static MessageType messageType(UserPrompts::Severity severity);

    /// Whether @p line is rich text: it starts with "<html>", in any case (Swing's
    /// BasicHTML.isHTMLString()).
    [[nodiscard]] static bool isHtml(const QString& line);

    /// An empty dialog titled @p title with the icon of @p type; the message and the options are
    /// added next.
    OptionDialog(MessageType type, const QString& title, QWidget* parent = nullptr);
    ~OptionDialog() override;
    OptionDialog(const OptionDialog&)            = delete;
    OptionDialog& operator=(const OptionDialog&) = delete;
    OptionDialog(OptionDialog&&)                 = delete;
    OptionDialog& operator=(OptionDialog&&)      = delete;

    /// Adds the text @p line to the message, under what is there (see the class comment).
    QLabel* addMessageLine(const QString& line);
    /// Adds each of @p lines.
    void addMessageLines(const QStringList& lines);
    /// Adds @p widget (not null) to the message, under what is there; it becomes a child of the
    /// message.
    void addMessageWidget(QWidget* widget);
    /// Adds a gap of gapHeight() to the message, under what is there.
    void addMessageGap();
    /// MigLayout's gap between unrelated parts ("unrel", "para"): the style's vertical layout
    /// spacing.
    [[nodiscard]] int gapHeight() const;
    /// The widget the message's parts are in, for a panel made as its child.
    [[nodiscard]] QWidget* messageArea() const noexcept { return m_message; }

    /// Adds an option with @p text and @p role (its place in the platform's order).
    QPushButton* addOption(const QString& text, QDialogButtonBox::ButtonRole role);
    /// Adds the standard option @p button (Yes, No, OK, Cancel, ...).
    QPushButton* addOption(QDialogButtonBox::StandardButton button);
    /// Makes @p option the initial one (JOptionPane's initialValue): the default button, which
    /// Enter clicks, with the keyboard focus when the dialog is shown.
    void setInitialOption(QPushButton* option);

    /// The option that closed the dialog, or null when none did (closed otherwise, or not yet).
    [[nodiscard]] QAbstractButton* chosenOption() const noexcept { return m_chosen; }

    /// The labels of the message's text lines, in order.
    [[nodiscard]] const QList<QLabel*>& messageLabels() const noexcept { return m_messageLabels; }
    /// The texts of the message's lines.
    [[nodiscard]] QStringList messageLines() const;
    /// The label with the icon (empty for PLAIN).
    [[nodiscard]] QLabel* iconLabel() const noexcept { return m_icon; }
    /// The options.
    [[nodiscard]] QDialogButtonBox* buttonBox() const noexcept { return m_buttons; }

protected:
    void showEvent(QShowEvent* event) override;

private:
    /// Closes the dialog with @p option chosen.
    void choose(QAbstractButton* option);

    QLabel*           m_icon;
    QWidget*          m_message;
    QVBoxLayout*      m_messageLayout;
    QDialogButtonBox* m_buttons;
    QList<QLabel*>    m_messageLabels;
    QPushButton*      m_initial{nullptr};
    QAbstractButton*  m_chosen{nullptr};
};

}  // namespace QtRocket::Gui
