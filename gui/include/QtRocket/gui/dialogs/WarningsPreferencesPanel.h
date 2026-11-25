#pragma once

#include <QObject>
#include <QWidget>

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class RecoveryWarningsPanel;

/// The Warnings page of the preferences dialog (OpenRocket's WarningsPreferencesPanel, swing
/// gui/dialogs/preferences/): the application-wide defaults of the recovery warning thresholds,
/// the thresholds' editor (RecoveryWarningsPanel) over the preferences (RecoverySpeedWarning,
/// RecoveryDrogueMainLowSpeedWarning, RecoveryDrogueMainHighSpeedWarning), as wide as the page
/// (Java: MigLayout("fillx", "[grow]"), the panel "growx, wmin 0"). New simulations take them
/// (DefaultSimulationOptionFactory::getDefault(), OpenRocket bug 38, fixed in the core), as the
/// Warnings tab's "Reset to default" does.
///
/// The editor's bindings are kept by @p scope, which the dialog commits before Apply and OK and
/// releases when it closes.
class WarningsPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The page over @p preferences, which must outlive it, its bindings kept by @p scope.
    WarningsPreferencesPanel(Preferences& preferences, BindingScope& scope, QWidget* parent);
    ~WarningsPreferencesPanel() override;
    WarningsPreferencesPanel(const WarningsPreferencesPanel&)            = delete;
    WarningsPreferencesPanel& operator=(const WarningsPreferencesPanel&) = delete;
    WarningsPreferencesPanel(WarningsPreferencesPanel&&)                 = delete;
    WarningsPreferencesPanel& operator=(WarningsPreferencesPanel&&)      = delete;

    /// The thresholds' editor.
    [[nodiscard]] RecoveryWarningsPanel& recoveryWarnings() const noexcept { return *m_recovery; }

private:
    RecoveryWarningsPanel* m_recovery;
};

}  // namespace QtRocket::Gui
