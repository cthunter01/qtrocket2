#pragma once

#include <functional>

#include <QDialog>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/UserPrompts.h"

class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

class StyledLabel;

/// OpenRocket's RenameConfigDialog (swing gui/dialogs/flightconfiguration/RenameConfigDialog.java):
/// "Rename Configuration", which the tab's "Rename Configuration" opens for the first selected
/// configuration (UserPrompts::askRenameConfiguration()). Application-modal, from the top:
/// - "Name for flight configuration: †" ("nameLabel");
/// - the text field ("textbox"), at least 200 pixels wide ("w 200lp"), holding the request's name
///   (the configuration's raw name, "[{motors}]");
/// - a paragraph gap lower, at the right: OK ("okButton", the default button), "Save as default"
///   ("saveAsDefaultButton"), "Reset to default" ("resetToDefaultButton") and Cancel
///   ("cancel");
/// - a paragraph gap lower, the information (infoText(): what "{motors}", "{manufacturers}",
///   "{cases}" and their combinations become), 2 points smaller, in the theme's dim text colour
///   (ThemeColors::TEXT_DIM, "info").
///
/// - OK answers RENAME with the text field's text, "Reset to default" RESET; both close the
///   dialog.
/// - "Save as default" calls the request's saveAsDefault with the dialog and the text field's text
///   (which shows "This change will only affect new flight configurations." and stores the
///   preference) and leaves the dialog open.
/// - Cancel, Escape and the window's close button answer CANCEL (Java's
///   GUIUtil.setDisposableDialogOptions(): Escape closes the dialog).
///
/// The dialog is not told a component: a parent destroyed while it is open (also during "Save as
/// default"'s message) takes it along, and the question then answers CANCEL (ask()).
///
/// Deviations from OpenRocket: the dialog answers what was chosen (UserPrompts) and the caller
/// renames, where Java's renames the configuration itself.
class RenameConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    /// MigLayout's paragraph gap ("wrap para"), under the text field and under the buttons.
    static constexpr int kParagraphGap = 12;
    /// The text field's least width ("w 200lp").
    static constexpr int kTextFieldWidth = 200;
    /// How much smaller the information is (StyledLabel(text, -2)).
    static constexpr double kInfoSize = -2.0;

    /// The dialog of @p request, not shown yet.
    /// @throws BugError when the request has no saveAsDefault
    explicit RenameConfigDialog(const UserPrompts::RenameConfigurationRequest& request,
                                QWidget*                                       parent = nullptr);
    ~RenameConfigDialog() override;
    RenameConfigDialog(const RenameConfigDialog&)            = delete;
    RenameConfigDialog& operator=(const RenameConfigDialog&) = delete;
    RenameConfigDialog(RenameConfigDialog&&)                 = delete;
    RenameConfigDialog& operator=(RenameConfigDialog&&)      = delete;

    /// Asks @p request, modal, parented to @p parent (or none): RENAME with the text, RESET, or
    /// CANCEL for Cancel, Escape, the close button and a parent destroyed while it is open (the
    /// dialog is made for the call: ModalDialog).
    /// @throws BugError when the request has no saveAsDefault
    [[nodiscard]] static UserPrompts::RenameConfigurationAnswer ask(
        QWidget* parent, const UserPrompts::RenameConfigurationRequest& request);

    /// The information under the buttons: "<html>† " and the four texts
    /// RenameConfigDialog.lbl.infoMotors, infoManufacturers, infoCases and infoCombination.
    [[nodiscard]] static QString infoText();

    /// How the dialog was left so far: CANCEL until OK or "Reset to default"; the name for
    /// RENAME.
    [[nodiscard]] UserPrompts::RenameConfigurationAnswer answer() const;

    [[nodiscard]] QLabel&      nameLabel() const noexcept { return *m_nameLabel; }
    [[nodiscard]] QLineEdit&   textField() const noexcept { return *m_textbox; }
    [[nodiscard]] QPushButton& okButton() const noexcept { return *m_ok; }
    [[nodiscard]] QPushButton& saveAsDefaultButton() const noexcept { return *m_saveAsDefault; }
    [[nodiscard]] QPushButton& resetToDefaultButton() const noexcept { return *m_resetToDefault; }
    [[nodiscard]] QPushButton& cancelButton() const noexcept { return *m_cancel; }
    [[nodiscard]] StyledLabel& infoLabel() const noexcept { return *m_info; }

    /// OK: RENAME with the text field's text, and close.
    void ok();
    /// "Save as default": the request's saveAsDefault, the dialog stays open.
    void saveAsDefault();
    /// "Reset to default": RESET, and close.
    void resetToDefault();

protected:
    /// The information's colour follows the palette (UITheme's TEXT_DIM, read again when the
    /// theme changes).
    void changeEvent(QEvent* event) override;

private:
    void updateColors();

    std::function<void(QWidget* dialog, const QString& name)> m_saveAsDefaultAction;
    UserPrompts::RenameAction m_action{UserPrompts::RenameAction::CANCEL};
    QLabel*                   m_nameLabel;
    QLineEdit*                m_textbox;
    QPushButton*              m_ok;
    QPushButton*              m_saveAsDefault;
    QPushButton*              m_resetToDefault;
    QPushButton*              m_cancel;
    StyledLabel*              m_info;
};

}  // namespace QtRocket::Gui
