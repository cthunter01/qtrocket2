#pragma once

#include <memory>
#include <optional>

#include <QObject>

#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"

class QLabel;
class QWidget;

namespace QtRocket::Gui
{

class DoubleBinding;
class EnumComboBox;
class UnitSpinBox;

/// OpenRocket's IgnitionSelectionDialog (swing gui/dialogs/flightconfiguration/
/// IgnitionSelectionDialog.java): "Select Ignition Configuration", which the Motors tab's "Select
/// ignition" opens over its first selected cell (UserPrompts::askIgnition()). Under the scope's
/// radio buttons (FlightConfigurationValueDialog: "Change all configurations using the default
/// ignition event" / "Override for the {0} flight configuration only") the rows of the values:
/// - "Ignition at:" and the combo box of the five ignition events ("IgnitionEvent", in their
///   order, "growx");
/// - under the combo box, indented ("gap indent, skip 1, span, split"), "plus", the delay's spin
///   box ("IgnitionDelay": short time in s, at least 0, no unit selector) and "seconds".
///
/// The dialog starts with the request's radio button, event and delay, and OK (also Escape)
/// answers the radio button chosen, the event and the delay (choice()); the caller applies them as
/// Java's OK action does (UserPrompts::IgnitionChoice). Cancel and the close button answer
/// nothing.
///
/// Deviations from OpenRocket: those of FlightConfigurationValueDialog (fixed OpenRocket bugs 2
/// and 3: the radio button the dialog starts on, Cancel and the close button); on a cell without
/// a motor configuration of its own Java's dialog edits the mount's default configuration, even
/// for "Override ...", where the caller makes the configuration's override (fixed OpenRocket bug
/// 4). The delay spin box is as wide as the others (Java's SpinnerEditor(spin, 3) makes it three
/// columns wide).
class IgnitionSelectionDialog final : public FlightConfigurationValueDialog
{
    Q_OBJECT

public:
    /// The dialog of @p request, not shown yet.
    explicit IgnitionSelectionDialog(const UserPrompts::IgnitionRequest& request,
                                     QWidget*                            parent = nullptr);
    ~IgnitionSelectionDialog() override;
    IgnitionSelectionDialog(const IgnitionSelectionDialog&)            = delete;
    IgnitionSelectionDialog& operator=(const IgnitionSelectionDialog&) = delete;
    IgnitionSelectionDialog(IgnitionSelectionDialog&&)                 = delete;
    IgnitionSelectionDialog& operator=(IgnitionSelectionDialog&&)      = delete;

    /// Asks @p request, modal, parented to @p parent (or none): the choice with OK or Escape,
    /// nullopt for Cancel, the close button and a parent destroyed while it is open (the dialog
    /// is made for the call: ModalDialog).
    [[nodiscard]] static std::optional<UserPrompts::IgnitionChoice> ask(
        QWidget* parent, const UserPrompts::IgnitionRequest& request);

    /// What the dialog shows: the scope, the event and the delay.
    [[nodiscard]] UserPrompts::IgnitionChoice choice() const;

    /// "Ignition at:".
    [[nodiscard]] QLabel&       eventLabel() const noexcept { return *m_eventLabel; }
    [[nodiscard]] EnumComboBox& eventBox() const noexcept { return *m_eventBox; }
    /// "plus".
    [[nodiscard]] QLabel&      plusLabel() const noexcept { return *m_plusLabel; }
    [[nodiscard]] UnitSpinBox& delaySpinBox() const noexcept { return *m_delaySpinBox; }
    /// "seconds".
    [[nodiscard]] QLabel& secondsLabel() const noexcept { return *m_secondsLabel; }

private:
    std::shared_ptr<EnumBinding<IgnitionEvent>> m_event;
    std::shared_ptr<DoubleBinding>              m_delay;
    QLabel*                                     m_eventLabel;
    EnumComboBox*                               m_eventBox;
    QLabel*                                     m_plusLabel{nullptr};
    UnitSpinBox*                                m_delaySpinBox{nullptr};
    QLabel*                                     m_secondsLabel{nullptr};
};

}  // namespace QtRocket::Gui
