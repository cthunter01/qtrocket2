#pragma once

#include <QObject>
#include <QWidget>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/motor/IgnitionEvent.h"

class QLabel;

namespace QtRocket::Gui
{

class BindingScope;
class BoolCheckBox;
class EnumComboBox;
class StyledLabel;

/// The motor mount tab of a body tube or an inner tube (OpenRocket's MotorConfig, swing
/// gui/configdialog/MotorConfig.java): "This component is a motor mount" and, enabled only while
/// it is checked, the motor overhang (with a slider from -2 cm to 6 cm), the ignition event and
/// delay of the mount's DEFAULT motor configuration (each marked with the dagger: a flight
/// configuration can override them), the dagger's note and a note on the number of stages
/// ("The current design has only one stage. ..." or "The current design has <n> stages.", counted
/// when the panel is made, as Java). No motor is chosen here (the flight configurations, M3).
///
/// Every edit of the default ignition reaches each selected mount's own default configuration and
/// fires MotorConfigurationSet::kDefaultMotorEventType on that mount when the configuration
/// changed, as the core prescribes (rocket/MotorConfiguration.h). Deviation from OpenRocket (the
/// user's decision U4): Java's MotorConfiguration setters notify nobody, so the dialog is not
/// marked modified, the document stays saved and Cancel cannot undo the change.
///
/// The main component of the scope's selection must be a MotorMount.
class MotorConfig : public QWidget
{
    Q_OBJECT

public:
    /// The motor mount tab of the components of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component is not a
    ///         MotorMount
    MotorConfig(BindingScope& scope, QWidget* parent);

    [[nodiscard]] BoolCheckBox& mountCheckBox() const noexcept { return *m_mount; }
    /// The widget that holds every field but the check box, enabled while it is checked.
    [[nodiscard]] QWidget& mountPanel() const noexcept { return *m_panel; }
    [[nodiscard]] const ConfigFields::NumberWidgets& overhangWidgets() const noexcept
    {
        return m_overhang;
    }
    [[nodiscard]] EnumComboBox& ignitionEventComboBox() const noexcept { return *m_event; }
    [[nodiscard]] const ConfigFields::NumberWidgets& ignitionDelayWidgets() const noexcept
    {
        return m_delay;
    }
    /// The note on the number of stages.
    [[nodiscard]] StyledLabel& stagesLabel() const noexcept { return *m_stages; }

    /// Whether a component acts as a motor mount (Java: BooleanModel(mount, "MotorMount")).
    [[nodiscard]] static BoolSource motorMountSource();
    /// The motor overhang of a mount (Java: DoubleModel(mount, "MotorOverhang")).
    [[nodiscard]] static DoubleSource motorOverhangSource();
    /// The ignition event of a mount's default motor configuration (Java: EnumModel(
    /// getDefaultMotorConfig(), "IgnitionEvent")); see the class comment for the event it fires.
    [[nodiscard]] static EnumSource<IgnitionEvent> ignitionEventSource();
    /// The ignition delay of a mount's default motor configuration (Java: DoubleModel(
    /// getDefaultMotorConfig(), "IgnitionDelay", 0)).
    [[nodiscard]] static DoubleSource ignitionDelaySource();

private:
    BoolCheckBox*               m_mount{nullptr};
    QWidget*                    m_panel{nullptr};
    ConfigFields::NumberWidgets m_overhang;
    EnumComboBox*               m_event{nullptr};
    ConfigFields::NumberWidgets m_delay;
    StyledLabel*                m_stages{nullptr};
};

}  // namespace QtRocket::Gui
