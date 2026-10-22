#pragma once

#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <QDialog>
#include <QObject>

class QPushButton;
class QTabWidget;
class QWidget;

namespace QtRocket
{
class InMemoryPreferences;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class DesignPreferencesPanel;
class GeneralPreferencesPanel;
class RocketPanel;
class UnitsPreferencesPanel;
class UserPrompts;

/// OpenRocket's preferences dialog (PreferencesDialog): modal for the whole application, titled
/// "Preferences", with the tabs General, Design and Units (GeneralPreferencesPanel,
/// DesignPreferencesPanel, UnitsPreferencesPanel) in Java's order and the buttons Cancel, Apply
/// and OK in the platform's order (MigLayout's button tags; QDialogButtonBox's roles).
///
/// The pages write the preferences (and the process-wide default units) at once. A snapshot of
/// the whole store is taken when the dialog opens and after every Apply:
/// - Apply stores the default units and refreshes the design view of every design window
///   (storeAndApplyPreferences()), then takes a new snapshot: a later Cancel goes back to here.
/// - OK, and so the Escape key and the close button too (Java disposes the dialog on Escape and on
///   closing, and its windowClosed listener stores), stores and refreshes as Apply does and
///   closes the dialog.
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
/// (reopen()): it is closed as by OK and shown anew on the General tab, with new pages and a new
/// snapshot.
///
/// The design views are those of rocketPanels(), asked whenever they are refreshed (the
/// WindowManager gives every window's; none in a test). A design view's refresh is
/// refreshRocketPanels(): Java's refreshParentFrame() and the configuration combo box's names.
///
/// Deviations from OpenRocket:
/// - The tabs of later milestones are left out: UI (Milestone 5), Simulation, Warnings and Launch
///   (Milestone 4), Materials and Graphics (Milestone 5); Java's dead Display page too.
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
/// - A dialog destroyed while it is open (with its window, which no user can close while the
///   dialog is modal) neither stores nor restores anything: the pages' changes stay as made.
class PreferencesDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The tabs, in their order (Java's TAB_GENERAL, TAB_DESIGN, TAB_UNITS).
    enum class Tab
    {
        GENERAL,
        DESIGN,
        UNITS,
    };

    /// The design views to refresh, asked each time (see the class comment).
    using RocketPanels = std::function<std::vector<RocketPanel*>()>;

    /// The dialog over @p preferences (the application's store: a SettingsPreferences, or a
    /// test's InMemoryPreferences), asking through @p prompts, both of which must outlive it, on
    /// the tab @p selectedTab. Not shown yet.
    /// @throws BugError for a store of another kind (PreferencesStore)
    PreferencesDialog(Preferences& preferences, UserPrompts& prompts, RocketPanels rocketPanels,
                      Tab selectedTab, QWidget* parent);
    ~PreferencesDialog() override;
    PreferencesDialog(const PreferencesDialog&)            = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&)                 = delete;
    PreferencesDialog& operator=(PreferencesDialog&&)      = delete;

    /// Java's showPreferences(): makes the dialog as a child of @p parent, shows it modal for the
    /// application and returns at once (Java's setVisible() returns when it is closed); the
    /// dialog is deleted once it is closed.
    static PreferencesDialog& showPreferences(Preferences& preferences, UserPrompts& prompts,
                                              RocketPanels rocketPanels, QWidget& parent,
                                              Tab selectedTab = Tab::GENERAL);

    /// Java's storeAndApplyPreferences(): stores the default units (Preferences::
    /// storeDefaultUnits()) and refreshes @p rocketPanels (refreshRocketPanels()).
    static void storeAndApplyPreferences(Preferences&                  preferences,
                                         std::span<RocketPanel* const> rocketPanels);

    /// Java's refreshParentFrame() for each of @p rocketPanels (null ones skipped):
    /// RocketPanel::updateExtras(), updateFigures() and updateRulers(), then the configuration
    /// combo box's names (ConfigurationComboBox::refresh()).
    static void refreshRocketPanels(std::span<RocketPanel* const> rocketPanels);

    [[nodiscard]] QTabWidget*              tabs() const noexcept { return m_tabs; }
    [[nodiscard]] GeneralPreferencesPanel* generalPage() const noexcept { return m_general; }
    [[nodiscard]] DesignPreferencesPanel*  designPage() const noexcept { return m_design; }
    [[nodiscard]] UnitsPreferencesPanel*   unitsPage() const noexcept { return m_units; }
    [[nodiscard]] QPushButton*             cancelButton() const noexcept { return m_cancel; }
    [[nodiscard]] QPushButton*             applyButton() const noexcept { return m_apply; }
    [[nodiscard]] QPushButton*             okButton() const noexcept { return m_ok; }
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
    /// Makes the three pages (and the binding scope of their fields) on @p selectedTab.
    void buildPages(Tab selectedTab);
    /// Java's windowClosed listener: stores and refreshes (@p store), or restores the snapshot
    /// and the units and refreshes; then writes the confirmation's preference back.
    void finishSession(bool store);
    /// Java's closeDialog(): finishSession(@p store), then closes.
    void closeDialog(bool store);
    /// The design views now (rocketPanels()).
    [[nodiscard]] std::vector<RocketPanel*> rocketPanels() const;
    /// The figure of every design view brought up to date (the Design page's markers).
    void updateFigures() const;

    Preferences*                         m_preferences;
    UserPrompts*                         m_prompts;
    RocketPanels                         m_rocketPanels;
    std::unique_ptr<InMemoryPreferences> m_snapshot;
    std::unique_ptr<BindingScope>        m_scope;
    QTabWidget*                          m_tabs{nullptr};
    GeneralPreferencesPanel*             m_general{nullptr};
    DesignPreferencesPanel*              m_design{nullptr};
    UnitsPreferencesPanel*               m_units{nullptr};
    QPushButton*                         m_cancel{nullptr};
    QPushButton*                         m_apply{nullptr};
    QPushButton*                         m_ok{nullptr};
    bool                                 m_closed{false};
};

}  // namespace QtRocket::Gui
