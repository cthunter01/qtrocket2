#pragma once

#include <QObject>
#include <QWidget>

class QCheckBox;
class QPushButton;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

class UserPrompts;

/// The General page of the preferences dialog (OpenRocket's GeneralPreferencesPanel), with its
/// items of Milestone 2, each written to the preferences at once:
/// - "Open last design file on startup" (AutoOpenLastDesign);
/// - "Show confirmation dialog when discarding preferences" (IgnoreDiscardPreferencesWarning),
///   which survives the dialog's Cancel;
/// - "Reset all preferences" at the bottom right: asks "Reset preferences?" (UserPrompts::
///   askYesNo()); on Yes every preference is removed and the default units reset and stored
///   again (PreferencesStore::clearPreferences()), the message "Reload QtRocket" says that QtRocket
///   may need a restart, and preferencesCleared() tells the dialog, which then shows the
///   preferences anew (Java: PreferencesDialog.showPreferences()).
///
/// Left out: the interface language and its note (QtRocket is English only), the user-defined
/// thrust curves and component presets with their buttons and description (Milestone 3: the motor
/// and preset databases), the update checks and their buttons (QtRocket checks for no updates),
/// "Show warning when saving in RASAero format" (no RASAero format), "Show warning when saving in
/// RockSim format" (Milestone 8), "Automatically open the Parts Library dialog" (Milestone 3),
/// Import preferences and Export preferences (OpenRocket's java.util.prefs XML files), and the
/// separators that would then stand between nothing.
///
/// Deviations: the texts that name OpenRocket name QtRocket ("Reload QtRocket"); the message
/// after the reset is shown at once (Java: invokeLater()).
class GeneralPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The page over @p preferences, asking through @p prompts; both must outlive it.
    GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts, QWidget* parent);
    ~GeneralPreferencesPanel() override;
    GeneralPreferencesPanel(const GeneralPreferencesPanel&)            = delete;
    GeneralPreferencesPanel& operator=(const GeneralPreferencesPanel&) = delete;
    GeneralPreferencesPanel(GeneralPreferencesPanel&&)                 = delete;
    GeneralPreferencesPanel& operator=(GeneralPreferencesPanel&&)      = delete;

    [[nodiscard]] QCheckBox*   openLastDesign() const noexcept { return m_openLast; }
    [[nodiscard]] QCheckBox*   showDiscardConfirmation() const noexcept { return m_discard; }
    [[nodiscard]] QPushButton* resetAllPreferencesButton() const noexcept { return m_reset; }

    /// What "Reset all preferences" does (see the class comment): true when the preferences were
    /// reset.
    bool resetAllPreferences();

Q_SIGNALS:
    /// Every preference was reset ("Reset all preferences", answered Yes).
    void preferencesCleared();

private:
    Preferences* m_preferences;
    UserPrompts* m_prompts;
    QCheckBox*   m_openLast{nullptr};
    QCheckBox*   m_discard{nullptr};
    QPushButton* m_reset{nullptr};
};

}  // namespace QtRocket::Gui
