#pragma once

#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"

class QPushButton;
class QTabWidget;
class QWidget;

namespace QtRocket
{
class InMemoryPreferences;
class LaunchPreferences;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class DesignPreferencesPanel;
class LaunchPreferencesPanel;
class RocketPanel;
class SimulationPreferencesPanel;
class UnitsPreferencesPanel;
class UserPrompts;
class WarningsPreferencesPanel;

/// OpenRocket's preferences dialog (PreferencesDialog): modal for the whole application, titled
/// "Preferences", with the tabs General, Design, Simulation, Warnings, Launch and Units
/// (GeneralPreferencesPanel, DesignPreferencesPanel, SimulationPreferencesPanel,
/// WarningsPreferencesPanel, LaunchPreferencesPanel, UnitsPreferencesPanel) in Java's order and
/// the buttons Cancel, Apply and OK in the platform's order (MigLayout's button tags;
/// QDialogButtonBox's roles).
///
/// The pages write the preferences (and the process-wide default units) at once. A snapshot of
/// the whole store is taken when the dialog opens and after every Apply:
/// - Apply stores the default units and refreshes the design view of every design window
///   (storeAndApplyPreferences()), then takes a new snapshot: a later Cancel goes back to here.
/// - OK, and so the Escape key and the close button too (Java disposes the dialog on Escape and on
///   closing, and its windowClosed listener stores), resolves the Simulation page's seed
///   (SimulationPreferencesPanel::prepareForSave()), stores and refreshes as Apply does and closes
///   the dialog. (Apply leaves the seed's field as it is, as Java's.)
/// - Cancel asks "Discard Preference Changes" (UserPrompts::askDiscardChanges(),
///   PREFERENCE_CHANGES) unless "Show confirmation dialog when discarding preferences" is off
///   ("Don't ask me again" turns it off whatever the answer); on discarding, the snapshot is
///   restored (PreferencesStore::restore()), the default units are reset and loaded from it
///   (UnitGroup::resetDefaultUnits(), Preferences::loadDefaultUnits()), every design view is
///   refreshed and the dialog closes.
/// Whichever way it closes, the confirmation's preference keeps the value it had at the end
/// (Java writes it back after the snapshot is restored, so it survives a Cancel).
///
/// The fields' pending edits are committed before Apply and OK (BindingScope::commitAll(): a
/// button that takes no focus leaves them pending on macOS); when the dialog closes the scope is
/// released, so that a late commit stores nothing.
///
/// "Reset all preferences" on the General page empties the store and resets the default units;
/// then the dialog does what Java's PreferencesDialog.showPreferences() does to the open dialog
/// (reopen()): it is closed as by OK and shown anew on the General tab, with new pages (the
/// Simulation, Warnings and Launch pages too) and a new snapshot. Each General page is made in the
/// dialog's General environment (the default directories of the user's files and the open
/// documents: GeneralPreferencesPanel::Environment).
///
/// The Launch page edits the launch preferences through the core's adapter (LaunchPreferences),
/// which the dialog makes before it takes its snapshot (and again, after the reset, before the
/// new snapshot): the adapter loads the average wind from the preferences and stores what it
/// loaded (its wind keys, where they differ from what the store holds), as Java's preferences
/// load their wind model once and store it; so a Cancel keeps what it stored, as Java's import of
/// its snapshot keeps it.
///
/// The design views are those of rocketPanels(), asked whenever they are refreshed (the
/// WindowManager gives every window's; none in a test). A design view's refresh is
/// refreshRocketPanels(): Java's refreshParentFrame() and the configuration combo box's names.
///
/// Deviations from OpenRocket:
/// - The tabs of later milestones are left out: UI (Milestone 5), Materials and Graphics
///   (Milestone 5); Java's dead Display page too.
/// - "Reset all preferences" does not resolve the old Simulation page's seed (prepareForSave()):
///   Java's old dialog does it in its windowClosed listener, which runs after the new dialog has
///   read the store and taken its snapshot, so that its new page shows an unfixed seed while the
///   store holds the old page's fixed one until the new dialog's OK unfixes it or its Cancel
///   restores the snapshot; here the reset store is what the new pages show and keep.
/// - Apply, OK and Cancel refresh the design view of every design window (Java: the parent frame
///   only), and also the names of the flight configurations in each view's combo box, which a
///   change of the default configuration name or the motor naming preference does not tell
///   (ConfigurationComboBox::refresh()).
/// - Cancel resets the default units before it loads them from the snapshot (the user's decision
///   U4): Java only loads them, which leaves a unit changed on the Units page in force when the
///   snapshot holds no units (a store that was never applied).
/// - Cancel restores the store exactly (PreferencesStore): a key or node made after the snapshot
///   is removed, where Java's import of its snapshot file keeps it.
/// - Reset all preferences keeps this dialog (with new pages and a new snapshot) where Java
///   disposes it and opens a new one; what the user sees and the preferences are the same.
/// - Java's dialog is one for the application (showPreferences() disposes the previous one);
///   here whoever opens the dialog keeps it (the WindowManager closes its previous one as Java
///   does), so the application has no global dialog.
/// - showPreferences() returns as soon as the dialog is shown (Java's setVisible() of the modal
///   dialog returns once it is closed): no event loop of its own runs under the menu item.
/// - A dialog destroyed while it is open neither stores nor restores anything: the pages' changes
///   stay as made. The WindowManager closes a dialog as by OK before its window closes (Java's
///   dialog, owned by its frame, is disposed with it and stores), as a window can close while
///   the dialog is open (macOS: a design file opened from the Finder in place of a New design,
///   the Dock's Quit).
class PreferencesDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The tabs, in their order (Java's TAB_GENERAL, TAB_DESIGN, TAB_SIMULATION, TAB_WARNINGS,
    /// TAB_LAUNCH, TAB_UNITS; Java's TAB_UI comes with Milestone 5).
    enum class Tab
    {
        GENERAL,
        DESIGN,
        SIMULATION,
        WARNINGS,
        LAUNCH,
        UNITS,
    };

    /// The design views to refresh, asked each time (see the class comment).
    using RocketPanels = std::function<std::vector<RocketPanel*>()>;

    /// The dialog over @p preferences (the application's store: a SettingsPreferences, or a
    /// test's InMemoryPreferences), asking through @p prompts, both of which must outlive it, with
    /// the General page in @p general, on the tab @p selectedTab. Not shown yet.
    /// @throws BugError for a store of another kind (PreferencesStore)
    PreferencesDialog(Preferences& preferences, UserPrompts& prompts, RocketPanels rocketPanels,
                      GeneralPreferencesPanel::Environment general, Tab selectedTab,
                      QWidget* parent);
    ~PreferencesDialog() override;
    PreferencesDialog(const PreferencesDialog&)            = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&)                 = delete;
    PreferencesDialog& operator=(PreferencesDialog&&)      = delete;

    /// Java's showPreferences(): makes the dialog as a child of @p parent, shows it modal for the
    /// application and returns at once (Java's setVisible() returns when it is closed); the
    /// dialog is deleted once it is closed.
    static PreferencesDialog& showPreferences(Preferences& preferences, UserPrompts& prompts,
                                              RocketPanels                         rocketPanels,
                                              GeneralPreferencesPanel::Environment general,
                                              QWidget& parent, Tab selectedTab = Tab::GENERAL);

    /// Java's storeAndApplyPreferences(): stores the default units (Preferences::
    /// storeDefaultUnits()) and refreshes @p rocketPanels (refreshRocketPanels()).
    static void storeAndApplyPreferences(Preferences&                  preferences,
                                         std::span<RocketPanel* const> rocketPanels);

    /// Java's refreshParentFrame() for each of @p rocketPanels (null ones skipped):
    /// RocketPanel::updateExtras(), updateFigures() and updateRulers(), then the configuration
    /// combo box's names (ConfigurationComboBox::refresh()).
    static void refreshRocketPanels(std::span<RocketPanel* const> rocketPanels);

    [[nodiscard]] QTabWidget*                 tabs() const noexcept { return m_tabs; }
    [[nodiscard]] GeneralPreferencesPanel*    generalPage() const noexcept { return m_generalPage; }
    [[nodiscard]] DesignPreferencesPanel*     designPage() const noexcept { return m_design; }
    [[nodiscard]] SimulationPreferencesPanel* simulationPage() const noexcept
    {
        return m_simulation;
    }
    [[nodiscard]] WarningsPreferencesPanel* warningsPage() const noexcept { return m_warnings; }
    [[nodiscard]] LaunchPreferencesPanel*   launchPage() const noexcept { return m_launch; }
    [[nodiscard]] UnitsPreferencesPanel*    unitsPage() const noexcept { return m_units; }
    [[nodiscard]] QPushButton*              cancelButton() const noexcept { return m_cancel; }
    [[nodiscard]] QPushButton*              applyButton() const noexcept { return m_apply; }
    [[nodiscard]] QPushButton*              okButton() const noexcept { return m_ok; }
    /// The snapshot Cancel goes back to.
    [[nodiscard]] const InMemoryPreferences& snapshot() const noexcept { return *m_snapshot; }
    /// Whether the dialog was closed (by OK, Cancel, Escape or its close button).
    [[nodiscard]] bool isClosed() const noexcept { return m_closed; }

    /// Apply (Java: applyChanges()).
    void applyChanges();
    /// Cancel: asks first unless the confirmation is off (see the class comment); true when the
    /// dialog was closed.
    bool cancel();
    /// OK (Java: closeDialog(true)).
    void accept() override;
    /// Escape and the close button: as OK (see the class comment).
    void reject() override;
    /// What "Reset all preferences" asks of the dialog (Java: showPreferences() of the open
    /// dialog's parent): closed as by OK, then shown anew on the General tab with new pages and a
    /// new snapshot.
    void reopen();

private:
    /// Makes the pages (and the binding scope of their fields) on @p selectedTab.
    void buildPages(Tab selectedTab);
    /// Java's windowClosed listener: resolves the Simulation page's seed (unless the session ends
    /// for a reset: @p resolveSeed false, see the class comment), stores and refreshes (@p store),
    /// or restores the snapshot and the units and refreshes; then writes the confirmation's
    /// preference back.
    void finishSession(bool store, bool resolveSeed = true);
    /// Java's closeDialog(): finishSession(@p store), then closes.
    void closeDialog(bool store);
    /// The design views now (rocketPanels()).
    [[nodiscard]] std::vector<RocketPanel*> rocketPanels() const;
    /// The figure of every design view brought up to date (the Design page's markers).
    void updateFigures() const;

    Preferences*                         m_preferences;
    UserPrompts*                         m_prompts;
    RocketPanels                         m_rocketPanels;
    GeneralPreferencesPanel::Environment m_general;
    /// The Launch page's launch preferences, made before the snapshot (see the class comment).
    std::shared_ptr<LaunchPreferences>   m_launchPreferences;
    std::unique_ptr<InMemoryPreferences> m_snapshot;
    std::unique_ptr<BindingScope>        m_scope;
    QTabWidget*                          m_tabs{nullptr};
    GeneralPreferencesPanel*             m_generalPage{nullptr};
    DesignPreferencesPanel*              m_design{nullptr};
    SimulationPreferencesPanel*          m_simulation{nullptr};
    WarningsPreferencesPanel*            m_warnings{nullptr};
    LaunchPreferencesPanel*              m_launch{nullptr};
    UnitsPreferencesPanel*               m_units{nullptr};
    QPushButton*                         m_cancel{nullptr};
    QPushButton*                         m_apply{nullptr};
    QPushButton*                         m_ok{nullptr};
    bool                                 m_closed{false};
};

}  // namespace QtRocket::Gui
