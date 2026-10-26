#pragma once

#include <memory>
#include <optional>

#include <QObject>

#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

class QLabel;
class QWidget;

namespace QtRocket::Gui
{

class DoubleBinding;
class EnumComboBox;
class UnitSpinBox;

/// OpenRocket's SeparationSelectionDialog (swing gui/dialogs/flightconfiguration/
/// SeparationSelectionDialog.java): "Select Separation Configuration", which the Stages tab's
/// "Select separation" opens over its first selected cell (UserPrompts::askSeparation()). Under
/// the scope's radio buttons (FlightConfigurationValueDialog: "Change all configurations using the
/// default separation event" / "Override for the {0} flight configuration only") the rows of the
/// values, in four columns:
/// - "Upper stage separates at:" and the combo box of the nine separation events
///   ("SeparationEvent", "spanx 3, growx");
/// - "Altitude:", its spin box, its unit selector and its slider ("SeparationAltitude": a
///   distance, at least 0, the slider linear from 100 to 1000 m), enabled only for "Specific
///   altitude during ascent" and "Specific altitude during descent" (updateState());
/// - "plus" at the right ("alignx 100%"), the delay's spin box ("SeparationDelay": short time in
///   s, at least 0, no unit selector) and "seconds".
///
/// The dialog starts with the request's radio button and separation, and OK (also Escape) answers
/// the radio button chosen and the separation shown (choice(): the request's configuration with
/// the event, the altitude and the delay set, the delay 0 for "Never", as Java's OK action sets
/// it); the caller applies it as that action does (UserPrompts::SeparationChoice). Cancel and the
/// close button answer nothing.
///
/// Deviations from OpenRocket: those of FlightConfigurationValueDialog (fixed OpenRocket bugs 1
/// and 3: Java always starts on "Override ...", as its isDefault(id) compares by identity, and
/// edits an existing override in place, so that its Cancel and close button keep the edits).
class SeparationSelectionDialog final : public FlightConfigurationValueDialog
{
    Q_OBJECT

public:
    /// The dialog of @p request, not shown yet.
    explicit SeparationSelectionDialog(const UserPrompts::SeparationRequest& request,
                                       QWidget*                              parent = nullptr);
    ~SeparationSelectionDialog() override;
    SeparationSelectionDialog(const SeparationSelectionDialog&)            = delete;
    SeparationSelectionDialog& operator=(const SeparationSelectionDialog&) = delete;
    SeparationSelectionDialog(SeparationSelectionDialog&&)                 = delete;
    SeparationSelectionDialog& operator=(SeparationSelectionDialog&&)      = delete;

    /// Asks @p request, modal, parented to @p parent (or none): the choice with OK or Escape,
    /// nullopt for Cancel, the close button and a parent destroyed while it is open (the dialog
    /// is made for the call: ModalDialog).
    [[nodiscard]] static std::optional<UserPrompts::SeparationChoice> ask(
        QWidget* parent, const UserPrompts::SeparationRequest& request);

    /// What OK answers: the scope and the separation shown, its delay 0 for NEVER.
    [[nodiscard]] UserPrompts::SeparationChoice choice() const;

    /// Whether the altitude's row is enabled: the event shown is ALTITUDE_ASCENDING or
    /// ALTITUDE_DESCENDING.
    [[nodiscard]] bool isAltitudeEnabled() const noexcept;

    /// "Upper stage separates at:".
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

    using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

    StageSeparationConfiguration                  m_initial;
    std::shared_ptr<EnumBinding<SeparationEvent>> m_event;
    std::shared_ptr<DoubleBinding>                m_altitudeBinding;
    std::shared_ptr<DoubleBinding>                m_delay;
    QLabel*                                       m_eventLabel{nullptr};
    EnumComboBox*                                 m_eventBox{nullptr};
    ConfigFields::NumberWidgets                   m_altitude;
    QLabel*                                       m_plusLabel{nullptr};
    UnitSpinBox*                                  m_delaySpinBox{nullptr};
    QLabel*                                       m_secondsLabel{nullptr};
};

}  // namespace QtRocket::Gui
