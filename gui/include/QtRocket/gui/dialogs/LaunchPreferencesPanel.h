#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

namespace QtRocket
{
class LaunchPreferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class SimulationConditionsPanel;
class StyledLabel;
class UserPrompts;

/// The Launch page of the preferences dialog (OpenRocket's LaunchPreferencesPanel, swing
/// gui/dialogs/preferences/): the launch conditions new simulations start from (Java's
/// MigLayout("fillx, ins 30lp n n n")):
/// - the bold dark-red NOTE "the settings below apply only to newly created simulations. To
///   change the settings for an existing simulation, select that simulation and edit it."
///   (launchWarning, half a point larger, its tooltip "You would not be the first one to be
///   confused by this :)"), a paragraph above the conditions;
/// - the launch conditions of the preferences: SimulationConditionsPanel with
///   Variant::LAUNCH_PREFERENCES (Java: addSimulationConditionsPanel(page, preferences, false)):
///   the average wind alone, no default buttons, over @p launch, the launch preferences
///   (LaunchPreferences, the core's SimulationOptionsInterface over the preferences).
///
/// Lifetime: the page shares @p launch, which must not be null, with whoever made it (the
/// preferences dialog makes it before its snapshot: LaunchPreferences stores the average wind it
/// loads); the conditions' bindings are kept by @p scope, which the dialog commits before Apply
/// and OK and releases when it closes. @p prompts (the conditions' easter egg) must outlive the
/// page.
class LaunchPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// Java's top inset "ins 30lp".
    static constexpr int kTopInset = 30;
    /// The size of the NOTE, in points above the default font's (Java: StyledLabel(..., 0.5f,
    /// BOLD)).
    static constexpr double kWarningSize = 0.5;

    /// The page over @p launch (see the class comment).
    /// @throws BugError when @p launch is null
    LaunchPreferencesPanel(std::shared_ptr<LaunchPreferences> launch, UserPrompts& prompts,
                           BindingScope& scope, QWidget* parent);
    ~LaunchPreferencesPanel() override;
    LaunchPreferencesPanel(const LaunchPreferencesPanel&)            = delete;
    LaunchPreferencesPanel& operator=(const LaunchPreferencesPanel&) = delete;
    LaunchPreferencesPanel(LaunchPreferencesPanel&&)                 = delete;
    LaunchPreferencesPanel& operator=(LaunchPreferencesPanel&&)      = delete;

    /// The launch preferences the page edits.
    [[nodiscard]] const std::shared_ptr<LaunchPreferences>& launchPreferences() const noexcept
    {
        return m_launch;
    }
    /// The NOTE (launchWarning).
    [[nodiscard]] StyledLabel& launchWarningLabel() const noexcept { return *m_launchWarning; }
    /// The launch conditions.
    [[nodiscard]] SimulationConditionsPanel& conditions() const noexcept { return *m_conditions; }

private:
    std::shared_ptr<LaunchPreferences> m_launch;
    StyledLabel*                       m_launchWarning{nullptr};
    SimulationConditionsPanel*         m_conditions{nullptr};
};

}  // namespace QtRocket::Gui
