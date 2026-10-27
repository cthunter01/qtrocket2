#pragma once

#include <memory>
#include <optional>

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QPushButton;
class QWidget;

namespace QtRocket
{
class FlightConfigurationId;
class MotorMount;
class ThrustCurveMotor;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ThrustCurveMotorSelectionPanel;

/// The motor chooser (OpenRocket's MotorChooserDialog, swing gui/dialogs/motor/
/// MotorChooserDialog.java, with its interfaces CloseableDialog and MotorSelector, which
/// ThrustCurveMotorSelectionPanel implements): "Select a rocket motor", modal for the whole
/// application, the selection panel (ThrustCurveMotorSelectionPanel) and under it OK and Cancel at
/// the right, in the platform's order (MigLayout's button tags ok and cancel).
///
/// A design window keeps one chooser for its life (OpenRocket's MotorConfigurationPanel makes one
/// and only hides it on closing), so that its search, filters, sort and table selection survive
/// between questions: forWindow() finds the window's chooser among its children or makes it the
/// first time, and choose() is the question UserPrompts::chooseMotor() asks
/// (main/WidgetUserPrompts): setMotorMountAndConfig(), then runModal() (Java's open():
/// selectMotorFromTable(), then shown until it closes), then the answer.
///
/// - OK (and a double click of the left button on the panel's table, Java's dialog.close(true))
///   commits what is being typed in the delay and nozzle fields (ThrustCurveMotorSelectionPanel::
///   commitEdits(): a click on a macOS button takes no focus), closes the dialog and stores the
///   selected curve as its set's preferred one (selectedMotor()); getSelectedMotor() then gives
///   it, with the delay and the nozzle exit diameter.
/// - Cancel, Escape (GUIUtil.installEscapeCloseOperation()) and the window's close button close it
///   without a motor.
/// - No button of the dialog is a default button, and none becomes one when it is shown or has the
///   focus (setAutoDefault(false) on every QPushButton, also the filter panel's "Clear All" and
///   "Select All"): OpenRocket's dialog has none, and Return in the search field, the delay editor
///   or the manufacturer list, which they pass on, must neither close it nor clear the list.
/// - The search field has the focus when the dialog is first shown (Java grabs it once in the
///   constructor); later the dialog gives it back to the widget that had it when it closed.
///
/// The object's name is "motorChooserDialog" (MotorConfigurationPanel's field), its buttons'
/// "okButton" and "cancelButton".
///
/// Deviations from OpenRocket:
/// - The close button cancels, also after an earlier OK (the user's bug 7, plan 10.3): Java's
///   dialog is only hidden on closing (HIDE_ON_CLOSE) and keeps okClicked from the OK of an
///   earlier question, so its call site takes the motor as chosen.
/// - The dialog first takes Qt's size of a window, its size hint (the panel's, of which the
///   table's 500 pixels are not part) but at most two thirds of the screen's width and height
///   (Java packs it to the panel's preferred size, whatever the screen), and is placed over its
///   window as Qt places a dialog (Java: setLocationByPlatform()). As in Java it has no least size
///   but its buttons' (the panel's is none: selectionPanel.setMinimumSize(0, 0)); smaller than its
///   size hint, it squeezes the panel's widgets, where OpenRocket's layout clips them.
/// - forWindow() keeps one chooser per window and per services (the motor database, the
///   preferences and the prompts it was made with): a question with other services gets a
///   chooser of its own. A question without a parent gets a chooser for that question alone, and
///   one asked while the window's chooser is open answers nothing at once (the chooser is modal:
///   only code run from its own event loop can ask).
class MotorChooserDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The chooser over the motor sets of @p services (motors()), with its preferences and
    /// prompts; the services must outlive it. No motor is selected.
    MotorChooserDialog(const GuiServices& services, QWidget* parent);
    ~MotorChooserDialog() override;
    MotorChooserDialog(const MotorChooserDialog&)            = delete;
    MotorChooserDialog& operator=(const MotorChooserDialog&) = delete;
    MotorChooserDialog(MotorChooserDialog&&)                 = delete;
    MotorChooserDialog& operator=(MotorChooserDialog&&)      = delete;

    /// The chooser of the top-level window @p window, its child, made with @p services the first
    /// time (one per window and services: see the class comment).
    [[nodiscard]] static MotorChooserDialog& forWindow(QWidget&           window,
                                                       const GuiServices& services);

    /// UserPrompts::chooseMotor(): the chooser of @p parent's window (forWindow(); without a
    /// parent one for this question), opened for @p mount's motor configuration in
    /// @p configurationId (setMotorMountAndConfig(), runModal()): the motor, delay and nozzle exit
    /// diameter chosen with OK, or nullopt (Cancel, Escape, the close button, OK without a motor,
    /// the chooser destroyed with its window while it was open, or the window's chooser open
    /// already). The mount is read only before the dialog shows.
    [[nodiscard]] static std::optional<UserPrompts::MotorChoice> choose(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId);

    /// setMotorMountAndConfig(fcid, mount) of the selection panel.
    void setMotorMountAndConfig(const FlightConfigurationId& fcid, const MotorMount& mount);

    /// Java's open(): selectMotorFromTable() (a chooser opened for a configuration without a motor
    /// selects the table's row of the question before), then the dialog shown modal (exec()) until
    /// it closes; no OK yet. The dialog may be destroyed before this returns (with its window): a
    /// caller that goes on holds a QPointer to it.
    void runModal();

    /// close(ok) (CloseableDialog): OK (@p ok, after commitEdits()) or Cancel.
    void closeDialog(bool ok);

    /// The motor chosen with OK, or null (no OK, or OK without a motor selected).
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> getSelectedMotor() const;
    /// The selected ejection delay in s (Motor::kPluggedDelay for plugged).
    [[nodiscard]] double getSelectedDelay() const;
    /// The selected nozzle exit diameter in m, 0 when unknown.
    [[nodiscard]] double getSelectedNozzleExitDiameter() const;

    /// The selection panel.
    [[nodiscard]] ThrustCurveMotorSelectionPanel& selectionPanel() const noexcept
    {
        return *m_selectionPanel;
    }

    /// Whether the chooser was made with @p services' motor database, preferences and prompts.
    [[nodiscard]] bool uses(const GuiServices& services) const noexcept;

    /// Every close (OK, Cancel, Escape, the close button): whether it was OK, and on OK the
    /// selected curve stored as its set's preferred one.
    void done(int result) override;

private:
    GuiServices                     m_services;
    ThrustCurveMotorSelectionPanel* m_selectionPanel;
    QPushButton*                    m_okButton{nullptr};
    QPushButton*                    m_cancelButton{nullptr};
    /// Whether the last close was OK (Java's okClicked, which only OK and Cancel set).
    bool m_okClicked = false;
};

}  // namespace QtRocket::Gui
