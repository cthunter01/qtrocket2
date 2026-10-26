#pragma once

#include <memory>
#include <optional>

#include <QObject>

#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"

class QLabel;
class QWidget;

namespace QtRocket::Gui
{

class DoubleBinding;
class EnumComboBox;
class UnitSpinBox;

/// OpenRocket's DeploymentSelectionDialog (swing gui/dialogs/flightconfiguration/
/// DeploymentSelectionDialog.java): "Select Deployment Configuration", which the Recovery tab's
/// "Select deployment" opens over its first selected cell (UserPrompts::askDeployment()). Under
/// the scope's radio buttons (FlightConfigurationValueDialog: "Change all configurations using the
/// default deployment event" / "Override for the {0} flight configuration only") the rows of the
/// values, in four columns:
/// - "Deploys at:" and the combo box of the deployment events ("DeployEvent", "spanx 3, growx"),
///   without "Lower stage separation" when the device is in the bottom stage (Java removes the
///   item when stage number + 1 == the rocket's stage count; a configuration that has it still
///   shows it, as no item);
/// - "Altitude:", its spin box, its unit selector and its slider ("DeployAltitude": a distance, at
///   least 0, the slider linear from 100 to 1000 m), enabled only for "Specific altitude during
///   descent" (updateState());
/// - "plus" at the right, the delay's spin box ("DeployDelay": short time in s, at least 0, no
///   unit selector) and "seconds".
///
/// The dialog starts with the request's radio button and deployment, and OK (also Escape) answers
/// the radio button chosen and the deployment shown (choice(): the request's configuration with
/// the event, the altitude and the delay set, as Java edits a clone of it); the caller applies it
/// as Java's OK action does (UserPrompts::DeploymentChoice). Cancel and the close button answer
/// nothing.
///
/// Deviations from OpenRocket: those of FlightConfigurationValueDialog (fixed OpenRocket bug 1:
/// Java always starts on "Override ...", as its isDefault(id) compares by identity).
class DeploymentSelectionDialog final : public FlightConfigurationValueDialog
{
    Q_OBJECT

public:
    /// The dialog of @p request, not shown yet.
    explicit DeploymentSelectionDialog(const UserPrompts::DeploymentRequest& request,
                                       QWidget*                              parent = nullptr);
    ~DeploymentSelectionDialog() override;
    DeploymentSelectionDialog(const DeploymentSelectionDialog&)            = delete;
    DeploymentSelectionDialog& operator=(const DeploymentSelectionDialog&) = delete;
    DeploymentSelectionDialog(DeploymentSelectionDialog&&)                 = delete;
    DeploymentSelectionDialog& operator=(DeploymentSelectionDialog&&)      = delete;

    /// Asks @p request, modal, parented to @p parent (or none): the choice with OK or Escape,
    /// nullopt for Cancel, the close button and a parent destroyed while it is open (the dialog
    /// is made for the call: ModalDialog).
    [[nodiscard]] static std::optional<UserPrompts::DeploymentChoice> ask(
        QWidget* parent, const UserPrompts::DeploymentRequest& request);

    /// What the dialog shows: the scope and the deployment.
    [[nodiscard]] UserPrompts::DeploymentChoice choice() const;

    /// Whether the altitude's row is enabled: the event shown is ALTITUDE.
    [[nodiscard]] bool isAltitudeEnabled() const noexcept;

    /// "Deploys at:".
    [[nodiscard]] QLabel&       eventLabel() const noexcept { return *m_eventLabel; }
    [[nodiscard]] EnumComboBox& eventBox() const noexcept { return *m_eventBox; }
    /// "Altitude:" and its spin box, unit selector and slider.
    [[nodiscard]] const ConfigFields::NumberWidgets& altitude() const noexcept
    {
        return m_altitude;
    }
    /// "plus".
    [[nodiscard]] QLabel&      plusLabel() const noexcept { return *m_plusLabel; }
    [[nodiscard]] UnitSpinBox& delaySpinBox() const noexcept { return *m_delaySpinBox; }
    /// "seconds".
    [[nodiscard]] QLabel& secondsLabel() const noexcept { return *m_secondsLabel; }

private:
    /// updateState(): the altitude's row follows the event.
    void updateState() const;

    using DeployEvent = DeploymentConfiguration::DeployEvent;

    DeploymentConfiguration                   m_initial;
    std::shared_ptr<EnumBinding<DeployEvent>> m_event;
    std::shared_ptr<DoubleBinding>            m_altitudeBinding;
    std::shared_ptr<DoubleBinding>            m_delay;
    QLabel*                                   m_eventLabel{nullptr};
    EnumComboBox*                             m_eventBox{nullptr};
    ConfigFields::NumberWidgets               m_altitude;
    QLabel*                                   m_plusLabel{nullptr};
    UnitSpinBox*                              m_delaySpinBox{nullptr};
    QLabel*                                   m_secondsLabel{nullptr};
};

}  // namespace QtRocket::Gui
