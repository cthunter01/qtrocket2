#pragma once

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/util/GuiServices.h"

class QCheckBox;
class QKeyEvent;
class QPushButton;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class RocketConfig;
class StyledLabel;

/// The "Save Design Info" dialog that Save as shows first while the preferences ask for it
/// (Preferences::isShowSaveRocketInfo(); OpenRocket's SaveDesignInfoPanel, swing
/// gui/configdialog/SaveDesignInfoPanel.java, in the modal JDialog of
/// BasicFrame.showSaveRocketInfoDialog()): a RocketConfig of the document's rocket (its rows, no
/// info button and no mass label) below the bold "(Optional) Fill in the design information for
/// this file", and a row of its own: "Don't show this dialog again", Cancel and OK.
///
/// - The fields edit the rocket in place, as the rocket's configuration does; no undo position is
///   added (Java's dialog adds none, so the edits join the document's current undo step).
/// - "Don't show this dialog again" stores Preferences::setShowSaveRocketInfo() of the opposite of
///   its state whenever it is toggled (Java: its action listener), also when the dialog is then
///   cancelled.
/// - OK commits the pending edits and closes; the window's close button does the same (Java: it
///   hides the plain JDialog, and Save as goes on). Esc does nothing, as in Java, whose plain
///   JDialog binds nothing to it.
/// - Cancel commits the pending edits first (Java: the Cancel button takes the focus, whose loss
///   commits the field), then always asks (UserPrompts::askDiscardChanges() with the component
///   dialog's "discard your changes" question; Java's SaveDesignInfoPanel asks without looking at
///   Preferences::isShowDiscardConfirmation()); "Don't ask me again" turns that preference off,
///   whatever the answer. On yes the dialog closes, and every edit stays (Java's Cancel undoes
///   nothing); on no it stays open.
///
/// edit() shows the dialog and returns whether the save is to go on: true after OK (and the close
/// button), false after a Cancel the user confirmed. Run 4 calls it from Save as through
/// UserPrompts::editDesignInformation() (main/WidgetUserPrompts), with the rocket selected in the
/// tree first (Java: setSelectedComponent(rocket)).
///
/// Deviations from OpenRocket:
/// - The result: Java's Save as goes on to the file chooser after Cancel too
///   (BasicFrame.saveAsAction() does not look at how the dialog closed); edit() reports the Cancel,
///   so that Save as can stop there.
/// - The design name field shows the rocket's name. Java's SaveDesignInfoPanel overrides
///   updateFields(), which fills the field, with an empty method, so the field starts empty and a
///   focus loss of it names the rocket "" (which a component takes as its default name). This fix
///   is not among the user's decision U4's: it is for the user to confirm.
class SaveDesignInfoDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The dialog of @p document's rocket, not shown yet, modal; @p services gives the
    /// preferences, the materials and the questions.
    SaveDesignInfoDialog(const GuiServices& services, OpenRocketDocument& document,
                         QWidget* parent = nullptr);
    ~SaveDesignInfoDialog() override;
    SaveDesignInfoDialog(const SaveDesignInfoDialog&)            = delete;
    SaveDesignInfoDialog& operator=(const SaveDesignInfoDialog&) = delete;
    SaveDesignInfoDialog(SaveDesignInfoDialog&&)                 = delete;
    SaveDesignInfoDialog& operator=(SaveDesignInfoDialog&&)      = delete;

    /// Shows the dialog of @p document's rocket, modal, parented to @p parent (or none), and
    /// returns whether the save is to go on (see the class comment).
    [[nodiscard]] static bool edit(QWidget* parent, const GuiServices& services,
                                   OpenRocketDocument& document);

    [[nodiscard]] RocketConfig& panel() const noexcept { return *m_panel; }
    [[nodiscard]] StyledLabel&  headingLabel() const noexcept { return *m_heading; }
    [[nodiscard]] QCheckBox&    dontShowAgainBox() const noexcept { return *m_dontShowAgain; }
    [[nodiscard]] QPushButton&  cancelButton() const noexcept { return *m_cancel; }
    [[nodiscard]] QPushButton&  okButton() const noexcept { return *m_ok; }

    /// OK: commits the pending edits and closes; the save goes on.
    void ok();
    /// Cancel (see the class comment).
    void cancel();
    /// The window's close button: as OK.
    void reject() override;

protected:
    /// Esc does nothing (see the class comment); every other key as QDialog.
    void keyPressEvent(QKeyEvent* event) override;

private:
    GuiServices   m_services;
    RocketConfig* m_panel{nullptr};
    StyledLabel*  m_heading{nullptr};
    QCheckBox*    m_dontShowAgain{nullptr};
    QPushButton*  m_cancel{nullptr};
    QPushButton*  m_ok{nullptr};
};

}  // namespace QtRocket::Gui
