#pragma once

#include <QDialog>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"

class QGridLayout;
class QKeyEvent;
class QLabel;
class QPushButton;
class QRadioButton;
class QVBoxLayout;
class QWidget;

namespace QtRocket::Gui
{

/// What the three value dialogs of the "Motors & Configuration" tab share, which OpenRocket
/// writes out in each of them (IgnitionSelectionDialog, DeploymentSelectionDialog and
/// SeparationSelectionDialog, swing gui/dialogs/flightconfiguration/): an application-modal dialog
/// (Dialog.ModalityType.APPLICATION_MODAL) with, under "Which flight configurations are
/// affected:", the radio buttons "Change all configurations using the default ... event"
/// (defaultButton) and "Override for the {0} flight configuration only" (overrideButton, {0} the
/// configuration's name as it is, an '&' included), both indented by MigLayout's paragraph gap
/// ("gapleft para"); then the subclass's rows of the values (valueGrid(), a paragraph gap below the
/// radio buttons: "wrap para"); then, a paragraph gap lower, OK and Cancel at the right, of one
/// width ("sizegroup btn"), OK the default button (GUIUtil.setDisposableDialogOptions(this,
/// okButton)).
///
/// - OK commits the pending edit of a spin box (bindings(): a button that takes no focus, as on
///   macOS, leaves it pending) and closes the dialog with QDialog::Accepted; the subclass's answer
///   is then what the dialog shows (its choice()).
/// - Escape does the same as OK (Java: GUIUtil.installEscapeCloseButtonOperation(this, okButton)
///   clicks OK, then closes the dialog), unless a field takes it first (a spin box reverts its
///   edit, a combo box closes its popup).
/// - Cancel and the window's close button close it with QDialog::Rejected: there is no answer.
///
/// The subclass makes its rows in valueGrid() in its constructor and then calls addButtons(), so
/// that the focus goes through the radio buttons, the values and then the buttons.
///
/// Deviations from OpenRocket (each dialog repeats them for its own):
/// - The dialogs edit values of their own and answer them (UserPrompts), which the caller applies
///   as the Java dialog's OK action does. Java's dialogs edit the rocket: so their Cancel and the
///   close button change nothing here, where OpenRocket's ignition dialog keeps its live edits on
///   the close button and marks the ignition overridden on Cancel, and its separation dialog keeps
///   the edits of an existing override on both (fixed OpenRocket bug 3).
/// - The radio button the dialog starts on is the request's, which the caller computes (fixed
///   OpenRocket bugs 1 and 2; see each request).
/// - OK and Escape commit a spin box's pending edit before the answer. Java's Escape clicks OK
///   while the edit is pending (a spinner commits on Enter and on the loss of the focus, which
///   the button's doClick() does not take), so that OK's action does not see it.
/// - The dialog is not told a component: a parent destroyed while it is open takes it along, and
///   the question then answers nothing (each subclass's ask(), ModalDialog).
class FlightConfigurationValueDialog : public QDialog
{
    Q_OBJECT

public:
    /// MigLayout's paragraph gap ("para", "wrap para", "gapleft para") and its unrelated gap
    /// ("wrap unrel"), as the other dialogs take them.
    static constexpr int kParagraphGap = 12;
    /// MigLayout's indent gap ("gap indent").
    static constexpr int kIndentGap = 12;

    ~FlightConfigurationValueDialog() override;
    FlightConfigurationValueDialog(const FlightConfigurationValueDialog&)            = delete;
    FlightConfigurationValueDialog& operator=(const FlightConfigurationValueDialog&) = delete;
    FlightConfigurationValueDialog(FlightConfigurationValueDialog&&)                 = delete;
    FlightConfigurationValueDialog& operator=(FlightConfigurationValueDialog&&)      = delete;

    /// The radio button chosen: THIS_CONFIGURATION for overrideButton (Java: isOverrideDefault()).
    [[nodiscard]] UserPrompts::ConfigurationScope scope() const noexcept;

    /// "Which flight configurations are affected:".
    [[nodiscard]] QLabel& scopeLabel() const noexcept { return *m_scopeLabel; }
    /// "Change all configurations using the default ... event" ("defaultButton").
    [[nodiscard]] QRadioButton& defaultButton() const noexcept { return *m_defaultButton; }
    /// "Override for the <name> flight configuration only" ("overrideButton").
    [[nodiscard]] QRadioButton& overrideButton() const noexcept { return *m_overrideButton; }
    /// OK ("okButton"); a BugError before addButtons().
    [[nodiscard]] QPushButton& okButton() const;
    /// Cancel ("cancel"); a BugError before addButtons().
    [[nodiscard]] QPushButton& cancelButton() const;

    /// OK (see the class comment).
    void ok();

protected:
    /// The texts of the scope's part of a dialog.
    struct ScopeTexts
    {
        /// "<Dialog>.opt.title".
        QString title;
        /// "<Dialog>.opt.default".
        QString allConfigurations;
        /// "<Dialog>.opt.override", with "{0}" where the configuration's name goes.
        QString thisConfiguration;
    };

    /// A dialog titled @p windowTitle with the radio buttons of @p texts (@p configurationName in
    /// place of "{0}"), the one of @p scope chosen; not shown yet.
    FlightConfigurationValueDialog(const QString& windowTitle, const ScopeTexts& texts,
                                   const QString&                  configurationName,
                                   UserPrompts::ConfigurationScope scope, QWidget* parent);

    /// The grid of the subclass's rows, under the radio buttons, set on valueWidget(), whose
    /// children the rows' widgets are (as ConfigFields::addNumberRow() makes them).
    [[nodiscard]] QGridLayout& valueGrid() const noexcept { return *m_valueGrid; }
    [[nodiscard]] QWidget&     valueWidget() const noexcept { return *m_values; }

    /// The bindings of the values (a scope without a selection: they hold values of their own),
    /// whose pending edits OK commits.
    [[nodiscard]] BindingScope& bindings() noexcept { return m_bindings; }

    /// Adds OK and Cancel under the values (see the class comment), once.
    /// @throws BugError when they are there already
    void addButtons();

    /// Escape is OK (see the class comment); every other key as QDialog.
    void keyPressEvent(QKeyEvent* event) override;

private:
    QVBoxLayout* m_layout;
    QLabel*      m_scopeLabel;
    /// The radio buttons' indented column.
    QWidget*      m_scopeButtons;
    QRadioButton* m_defaultButton;
    QRadioButton* m_overrideButton;
    QWidget*      m_values;
    QGridLayout*  m_valueGrid;
    QPushButton*  m_ok{nullptr};
    QPushButton*  m_cancel{nullptr};
    BindingScope  m_bindings;
};

}  // namespace QtRocket::Gui
