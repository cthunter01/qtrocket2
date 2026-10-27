#pragma once

#include <span>
#include <string>
#include <string_view>

#include <QDialog>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/util/Uuid.h"

class QMoveEvent;
class QShowEvent;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class RocketComponentConfig;

/// The configuration dialog of the selected components (OpenRocket's ComponentConfigDialog, swing
/// gui/configdialog/ComponentConfigDialog.java): a modeless dialog with the panel of the selection
/// (ConfigPanelFactory), ONE for the whole application at a time, as Java's static dialog (the plan
/// allows this global, section 9.2): showDialog() closes the open one before it opens the next.
/// The window it is shown for owns it (a window that closes takes it along).
///
/// - Titles: "<Name> configuration", "(Multi-component) <Name> configuration" for several
///   components of one class, "Multi-component configuration" for several classes (<Name> is the
///   type name of the main component's kind, "Body Tube").
/// - Undo: one undo step per dialog session. Once the dialog is shown, it adds the undo position
///   "Modify <Name>" (several components: "Modify components"), none for a component just added
///   (the add button did "Add <Name>" before it). A preset chosen in the session adds its own
///   ("Use Preset <Name>", PresetBinding::choose()), which only names the session's step while
///   nothing has changed since its position.
/// - The parts library for a new component (Java's auto-open): after the undo position, a dialog
///   for one component just added (no others) of a kind that takes presets opens the parts library
///   one turn of the event loop later (RocketComponentConfig::selectPreset()), when the
///   preferences say so: GuiPreferenceKeys::alwaysOpenPresetKey() of the kind (the parts library's
///   "Always open this dialog when creating a new <component>") and
///   GuiPreferenceKeys::kAutoOpenPartsLibrary (the General preferences' "Automatically open the
///   Parts Library dialog"), both true when not stored (opensPartsLibrary()).
/// - The rocket: the dialog has one connection to the rocket, made before the selection and the
///   panel (Java's order), so that its slot runs first. A TREE or UNDO change closes the dialog at
///   once, inside the event: its bindings end (BindingScope::release(): the selection lets go of
///   the components silently, before its own slot could tell the fields; an undo replaces every
///   component) and it hides; any other change refreshes the panel's fields (updateFields()) and
///   marks the dialog modified, with a '*' before the title, once.
/// - No default button (Java: setDisposableDialogOptions(this, null)): every QPushButton of the
///   dialog, a panel's own included, is made no auto-default one when the dialog is shown, so that
///   Enter in a combo box or a check box clicks nothing.
/// - OK, Esc and the window's close button keep the changes: the pending edits are committed
///   (BindingScope::commitAll(), as a button that takes no focus leaves them pending), the bindings
///   end, and the dialog goes. Cancel: an existing component not modified closes; otherwise, unless
///   the preferences say not to ask (Preferences::isShowDiscardConfirmation(); "Don't ask me again"
///   turns it off, whatever the answer), the user is asked (UserPrompts::askDiscardChanges(): undo
///   adding the component, or discard the changes), and on yes the dialog closes, its pending
///   edits dropped, BEFORE OpenRocketDocument::undo() goes back to the session's undo position;
///   for a new component the undo goes on until the component is gone (see the deviations).
/// - The position is remembered (kPositionKey, on every move of the shown dialog, as Java's
///   GUIUtil.rememberWindowPosition(); a remembered place on no screen is not used), the size is
///   the contents' (Java: pack()). The selected tab is remembered by its title for the next dialog,
///   unless that dialog is the Rocket's, or a stage's and the next component is no stage, unless
///   showDialog() is told not to and never with the preference "always open the leftmost tab"
///   (Preferences::isAlwaysOpenLeftmostTab()). A dialog of several classes opens on its first tab.
///
/// Deviations from OpenRocket:
/// - Modifications count only after the session's undo position (the user's decision U4): what
///   the panel changes while it is made (the panels switch an automatic radius off when no
///   neighbour gives one) neither marks the dialog modified nor puts the '*' in the title, so an
///   unchanged Cancel closes without an undo. Java counts those changes, and its unchanged Cancel
///   then undid the step before the dialog.
/// - Cancel asks at once (Java: invokeLater) and undoes at once (Java: after 100 ms); when the
///   dialog was closed meanwhile (an undo while the question was open) it undoes nothing. The
///   question's parent is the dialog (Java: the panel), and a dialog closed while the question is
///   open is deleted only once the question has its answer; a dialog deleted meanwhile with its
///   window is not touched afterwards.
/// - Cancel of a new component removes it, as its question says ("Are you sure you want to undo
///   adding this component?"), also after a preset was chosen (bug 15 of the plan's section 10.3,
///   fixed): the preset's undo position is a step of its own after the add, so Java's one undo()
///   undoes only "Use Preset <Name>" and leaves the component in the rocket (probe:
///   tools/openrocket-gui-pins/preset-chooser/probe/NewComponentCancelProbe.java, "with a preset,
///   after Cancel's undo": undo 'Add Body Tube', redo 'Use Preset Body Tube'). Here the undo goes
///   on, one step at a time, until the rocket has no component of the new component's id (or
///   nothing is left to undo), which is the probe's "after a second undo"; any other step added
///   after the add while the dialog was open goes with it. An undo that neither goes back a step
///   nor reaches the state of its step (a slot of its events changed the document again, so that
///   every further undo would load the same step again) ends it there. An existing component's
///   Cancel undoes once, as Java's: a preset chosen after another edit is undone alone, the edit
///   stays.
/// - The auto-open reads its two preferences without storing them; Java's
///   SwingPreferences.getBoolean() stores the default of each it does not find
///   ("<name>AlwaysOpenPreset" first, then AutoOpenPartsLibrary, as its condition reaches them).
/// - Java moves the dialog onto the screen of the start-up window; here a remembered place on no
///   screen is not used, and Qt places the dialog over its window.
/// - A different-class selection opens on its first tab (Java selects tab 1, the Appearance tab of
///   Milestone 5).
///
/// Who shows it: the parts of a design window ask their window by signal (the tree, the figure,
/// Edit, the add buttons, File > Properties, the examples), and the application's
/// main/ConfigDialogRequests calls showDialog(), disposeDialog() and commitPendingEdits() for
/// them; reopening the dialog when the selection changes (Java's DesignPanel) is the window's
/// design panel's (main/DesignPanel), queued.
class ComponentConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The object name of the dialog.
    static constexpr const char* kGeometryName = "ComponentConfigDialog";
    /// The key of the dialog's position in the preferences' node of the windows
    /// (GuiPreferenceKeys::kNodeWindows): Java's "position." and the dialog's class, its value
    /// "x,y" (SwingPreferences.setWindowPosition()).
    static constexpr std::string_view kPositionKey = "position.ComponentConfigDialog";

    /// Closes the open dialog, if any, and shows the configuration of @p main (the component the
    /// fields show) and @p others (the other selected components every edit reaches), components
    /// of @p document's rocket, as a dialog of @p window (Java: showDialog()). @p isNewComponent:
    /// the component was just added (no undo position; Cancel undoes adding it).
    /// @p rememberPreviousTab: select the tab the previous dialog had selected (see the class
    /// comment).
    /// @throws BugError when @p main or an entry of @p others is not in @p document's rocket
    static void showDialog(QWidget& window, const GuiServices& services,
                           OpenRocketDocument& document, RocketComponent& main,
                           std::span<RocketComponent* const> others = {},
                           bool isNewComponent = false, bool rememberPreviousTab = true);

    /// Closes the open dialog, keeping its changes after committing its pending edits (Java:
    /// disposeDialog(), whose focus loss commits). Nothing when none is open.
    static void disposeDialog();

    /// Whether a dialog is open and visible (Java: isDialogVisible()).
    [[nodiscard]] static bool isDialogVisible();

    /// The open dialog, or null (Java: getDialog()); a dialog that has been closed is null too.
    [[nodiscard]] static ComponentConfigDialog* dialog();

    /// Forgets what the dialogs closed so far leave for the next one (the tab to select): the
    /// state the application starts with. For the tests, which leave no process-wide state
    /// behind; an open dialog stays open.
    static void forgetPreviousDialogs();

    /// Commits the pending edits of the open dialog (before a save or an undo of the main window:
    /// a menu or a shortcut may leave them pending); true when a binding took a value.
    static bool commitPendingEdits();

    /// Whether the dialog of a new component of @p kind opens the parts library (see the class
    /// comment): the kind takes presets (presetTypeOf()) and @p preferences hold true, or nothing,
    /// for GuiPreferenceKeys::alwaysOpenPresetKey(@p kind) and kAutoOpenPartsLibrary (Java:
    /// getBoolean(getComponentName() + "AlwaysOpenPreset", true) && isAutoOpenPartsLibrary()).
    [[nodiscard]] static bool opensPartsLibrary(const Preferences& preferences, ComponentKind kind);

    ~ComponentConfigDialog() override;
    ComponentConfigDialog(const ComponentConfigDialog&)            = delete;
    ComponentConfigDialog& operator=(const ComponentConfigDialog&) = delete;
    ComponentConfigDialog(ComponentConfigDialog&&)                 = delete;
    ComponentConfigDialog& operator=(ComponentConfigDialog&&)      = delete;

    /// The panel of the selection.
    [[nodiscard]] RocketComponentConfig& configurator() const noexcept { return *m_configurator; }

    /// Whether a change of the rocket reached the dialog since its undo position (Java:
    /// isModified()).
    [[nodiscard]] bool isModified() const noexcept { return m_modified; }

    [[nodiscard]] bool isNewComponent() const noexcept { return m_newComponent; }

    /// Whether the dialog has been closed (it is deleted later).
    [[nodiscard]] bool isDisposed() const noexcept { return m_disposed; }

    /// The title of the selected tab (Java: getSelectedTabName()).
    [[nodiscard]] QString selectedTabName() const;

    /// OK: keeps the changes and closes.
    void ok();

    /// Cancel (see the class comment).
    void cancel();

    /// Esc and the window's close button: as OK (Java's dialog closes and keeps the changes).
    void reject() override;

protected:
    /// Makes no QPushButton of the dialog an auto-default one (see the class comment).
    void showEvent(QShowEvent* event) override;
    /// Remembers the position of the shown dialog.
    void moveEvent(QMoveEvent* event) override;

public:
    /// What only the class can make: the key of its constructor, which showDialog() calls
    /// through makeChild() (the passkey idiom: one dialog for the application).
    class Key
    {
        friend class ComponentConfigDialog;
        Key() = default;
    };

    /// The dialog of @p main and @p others, components of @p document's rocket, owned by
    /// @p window (see showDialog()); it makes their ComponentSelection once it listens to the
    /// rocket.
    /// @throws BugError when an entry of @p others is not in the rocket of @p main
    ComponentConfigDialog(Key key, const GuiServices& services, OpenRocketDocument& document,
                          RocketComponent& main, std::span<RocketComponent* const> others,
                          bool isNewComponent, QWidget* window);

private:
    /// The rocket changed (Java: componentChanged()).
    void componentChanged(const ComponentChangeEvent& event);

    /// From now on a change of the rocket marks the dialog modified (after the undo position).
    void startCountingModifications() noexcept { m_counting = true; }

    /// Closes the dialog: the bindings end (pending edits dropped), the rocket connection goes,
    /// and the dialog hides and is deleted later (once the question of cancel() has its answer,
    /// while one is open). Nothing when it is closed already.
    void dispose();

    /// Closes the dialog, then undoes back to the session's undo position.
    void discardChanges();

    /// The question of cancel() has its answer: a dialog closed meanwhile is deleted later now.
    /// Whether it was closed.
    bool endQuestion();

    /// kPositionKey as a key.
    [[nodiscard]] static std::string positionKey();
    /// Moves the dialog to the remembered position, when there is one on a screen.
    void restorePosition();

    GuiServices                             m_services;
    OpenRocketDocument*                     m_document;
    RocketComponentConfig*                  m_configurator{nullptr};
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
    ComponentKind                           m_mainKind;
    bool                                    m_newComponent;
    bool                                    m_counting{false};
    bool                                    m_modified{false};
    bool                                    m_disposed{false};
    /// Set while cancel() asks its question: dispose() leaves the deletion to cancel().
    bool m_asking{false};
    /// The main component's id: a new component's Cancel undoes until it is gone.
    Uuid m_mainId;
};

}  // namespace QtRocket::Gui
