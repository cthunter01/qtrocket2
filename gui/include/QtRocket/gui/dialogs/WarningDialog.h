#pragma once

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/OptionDialog.h"

class QWidget;

namespace QtRocket::Gui
{

class MessageList;

/// OpenRocket's WarningDialog.showWarnings(): a warning message (JOptionPane.showMessageDialog()
/// with WARNING_MESSAGE and OK) whose message is the given lines followed by the list of the
/// warnings (MessageList, each warning's Warning::toString()) in a scroll pane. The file
/// operations show it after a load with warnings ("Warnings while opening file") and, for
/// QtRocket, after a save with warnings ("Warnings while saving file"), through
/// UserPrompts::showWarnings().
class WarningDialog final : public OptionDialog
{
    Q_OBJECT

public:
    /// The dialog titled @p title with the @p message lines and the @p warnings, not shown yet.
    WarningDialog(const QStringList& message, const QString& title, const QStringList& warnings,
                  QWidget* parent = nullptr);
    ~WarningDialog() override;
    WarningDialog(const WarningDialog&)            = delete;
    WarningDialog& operator=(const WarningDialog&) = delete;
    WarningDialog(WarningDialog&&)                 = delete;
    WarningDialog& operator=(WarningDialog&&)      = delete;

    /// Shows the dialog, modal, parented to @p parent (or none), and returns when it is closed
    /// (WarningDialog.showWarnings(parent, message, title, warnings)).
    static void showWarnings(QWidget* parent, const QStringList& message, const QString& title,
                             const QStringList& warnings);

    /// The list of the warnings ("warnings").
    [[nodiscard]] MessageList& warningList() const noexcept { return *m_warnings; }

private:
    MessageList* m_warnings;
};

}  // namespace QtRocket::Gui
