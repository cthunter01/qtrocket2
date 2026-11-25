#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <QDialog>
#include <QObject>
#include <QString>

#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/util/Signal.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QTabWidget;
class QVBoxLayout;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ConfigurationComboBox;
class GuiServices;
class SimulationConditionsPanel;
class SimulationOptionsPanel;
class SimulationPlotPanel;
class SimulationWarningsPanel;
class StyledLabel;
class WindowGeometry;

/// The simulation settings dialog (OpenRocket's SimulationConfigDialog, swing gui/simulation/
/// SimulationConfigDialog.java): "Edit simulation" over one simulation, "Multi-simulation edit"
/// over several, modal to the design window it belongs to (Java: DOCUMENT_MODAL; here
/// Qt::WindowModal over its parent), opened by the Simulations tab through
/// SimulationDialogs::editSimulations() (edit() below for the application's).
///
/// What it shows, from the top (Java's content panel, "fill", "[grow]", "[][grow]"):
/// - the top rows (topPanel): "Name: " (simedtdlg.lbl.Simname and a space) and the first
///   simulation's name (nameField), every change of which renames it at once unless the field is
///   empty (Java's document listener); "Flight configuration:" (simedtdlg.lbl.Flightcfg, its
///   tooltip simedtdlg.lbl.ttip.Flightcfg) and the rocket's configurations
///   (ConfigurationComboBox with its own selection, configComboBox, its tooltip
///   simedtdlg.combo.ttip.Flightcfg), starting at the first simulation's configuration, whose
///   pick sets it at once (Simulation::setFlightConfigurationId()); then "Status:"
///   (simpanel.col.Status and ':', statusLabel) and the first simulation's status description in
///   its status colour (simStatus: "<html>", ColorConversion::formatHtmlColor() of
///   SimulationStatusColors' colour and SimulationStatusText::description(), "</html>"), worked
///   out once, when the dialog is made. In a multi-simulation edit the name and the
///   configuration are the first simulation's only, as in Java;
/// - the tabs (tabbedPane), in Java's order:
///   0. "Launch conditions" (SimulationConfigDialog.tab.Launchcond): SimulationConditionsPanel
///      over the first simulation's options, in a scroll area (launchConditionsScrollPane);
///   1. "Simulation options" (tab.Simopt): SimulationOptionsPanel, in a scroll area
///      (simulationOptionsScrollPane);
///   2. "Warnings" (tab.Warnings): SimulationWarningsPanel of the first simulation, its warnings
///      shown unless in a multi-simulation edit;
///   3. "Plot data" (tab.Plotdata): SimulationPlotPanel of the first simulation when it has
///      data, else an empty page (plotPlaceholder); disabled in a multi-simulation edit (tooltip
///      tab.plotDis.ttip, "Plotting not supported for multi-simulation editing") or without
///      data (tab.plotNoData.ttip, "Simulation has no data to plot");
///   4. "Export data" (tab.Exportdata): an empty page (exportPlaceholder) until the export panel
///      comes (Milestone 4's run 3, part I3), disabled, with Java's tooltips in a
///      multi-simulation edit (tab.expDis.ttip) or without data (tab.expNoData.ttip);
///   the tabs are made once, when the dialog is made (a run in the dialog enables no tab), and
///   never hold the dialog larger than the top rows and the buttons need (Java: their minimum
///   size 0, "wmin 0, hmin 0"): a tab without a scroll area of its own (the plot tab's) is cut
///   off instead;
/// - the buttons (bottomPanel): in a multi-simulation edit, at the left, "Multi-simulation edit"
///   (simedtdlg.title.MultiSimEdit, multiSimEditLabel: bold, one point smaller, in ThemeColors'
///   MULTI_COMP_EDIT colour, its tooltip simedtdlg.title.MultiSimEdit.ttip and the simulations'
///   names joined by ", "); at the right "Cancel" (cancelButton, dlg.but.cancel, its tooltip
///   SimulationConfigDialog.btn.Cancel.ttip) and "OK" (okButton, dlg.but.ok, its tooltip
///   btn.OK.ttip). The selected tab relabels them (Java's tab listener): "OK" and "Cancel" on the
///   tabs 0 to 2, "Plot" (SimulationConfigDialog.btn.plot) and "Close" (dlg.but.close) on the
///   tab 3, "Export" (btn.export) and "Close" on the tab 4; their tooltips stay. No button is the
///   default one (Java: setDisposableDialogOptions(this, null)): Return clicks none.
///
/// What it does:
/// - When it is made: the window's background estimate is paused (EditRequest::
///   pauseBackgroundRuns), the document gets the undo position "Edit <name>" (one simulation) or
///   "Edit simulations" (Java's literals, translatable here), and from then on the first change of
///   the first simulation (Simulation::changed(): a name, a configuration, an option, a plot
///   appearance, a run's result) marks the dialog modified (isModified()) and puts "* " before the
///   title, once. The panels edit the first simulation's options live, as Java's models.
/// - OK (ok(), the OK button): the fields' pending edits committed (BindingScope::commitAll()),
///   SimulationOptionsPanel::prepareForSimulation(), in a multi-simulation edit the first
///   simulation's conditions and extensions copied to the others (the options'
///   copyConditionsFrom(), the extension list replaced by clones of the first's), then every
///   simulation of the dialog that is not up to date (Simulation::isStatusUpToDate() of its
///   getStatus()) runs: UserPrompts::runSimulations() with the request of the design window
///   (the dialog's parent), the document, those simulations, the services' executor and
///   EditRequest::cancelBackgroundRuns (none runs without an executor). Then on the Plot data tab
///   the plot dialog of the plot tab's configuration opens (SimulationPlotPanel::doPlot(), over
///   this dialog) and the dialog stays open; on the Export data tab the export (the export panel,
///   part I3) and the dialog stays open; on either without its panel, and on the other tabs, the
///   dialog closes (accepted).
/// - Cancel (cancel(): the Cancel or Close button, Escape, the window's close button): on the
///   Plot data and Export data tabs the dialog closes and keeps every edit (Java: "Normal close
///   action"). On the other tabs (Java's cancelSimEdit()): an existing simulation that was not
///   modified just closes; otherwise, unless the preference ShowDiscardSimulationConfirmation is
///   off (then the edits are discarded at once), the question "Cancel operation"
///   (UserPrompts::askDiscardChanges(): SIMULATION_ADDITION for a new simulation,
///   SIMULATION_CHANGES otherwise), whose "Don't ask me again" turns the preference off whatever
///   the answer; Yes discards: the dialog closes first (its bindings released, so that no field
///   writes any more), then the document is undone back to the dialog's undo position, then its
///   saved state is put back as it was when the dialog was made, and the document tells its
///   listeners (a DOCUMENT event without a source, Java's DocumentChangeEvent(dialog)); No keeps
///   the dialog open.
/// - The size and the place are remembered (WindowGeometry under kGeometryName; Java:
///   GUIUtil.rememberWindowPosition() and rememberWindowSize() by the class); without a
///   remembered size the dialog takes its contents' (Java: pack()); both are capped to the
///   available area of its screen less kScreenMargin pixels (capDialogToScreenBounds()).
///
/// Lifetime: the dialog holds the simulations by their std::shared_ptr; the panels' bindings
/// and object selections are kept by the dialog's BindingScope, released as the dialog closes.
/// The document, the services and the simulations' document must outlive the dialog. It is meant
/// for one session: once closed (isClosed()) its actions do nothing.
///
/// OpenRocket bugs fixed here (documented where each is made):
/// - OpenRocket bug 29 (the dialog's half; the core's is SimulationOptions' equality): Cancel's
///   discard never undoes beyond the dialog's own undo position, so a change the user made
///   before the dialog opened survives even when the dialog's edits left the document as it was
///   at that position (an edit undone by hand, or one the document's equality ignores), where
///   Java's undo() then goes back one step too many.
/// - OpenRocket bug 30: Cancel of a new simulation after an edit undoes the addition too (Java's
///   single undo() only undoes the edits, so the simulation it asked to "undo adding" stays).
///
/// Deviations from OpenRocket:
/// - Escape and the window's close button act as Cancel: the question comes while the dialog is
///   still open, and No keeps it open. Java disposes of the dialog first and asks afterwards (a
///   No then keeps the edits of a closed dialog, uncopied to the other simulations of a
///   multi-simulation edit).
/// - The Export data tab is disabled until the export panel exists (run 3's part I3).
/// - The run dialog and the plot dialog are children of this dialog (Java gives them the main
///   window as owner): they stay above it and go with it.
/// - Plot after a run that left the simulation without data does nothing (Java's plot dialog
///   would fail on the missing data); the dialog stays open.
/// - The background estimate's pause (EditRequest::pauseBackgroundRuns) is an addition (O-M4-16).
/// - Without a remembered place the platform places the dialog (Qt centres it over its parent;
///   Java: setLocationByPlatform()).
class SimulationConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The dialog's objectName and the name its geometry is remembered under (WindowGeometry).
    static constexpr const char* kGeometryName = "SimulationConfigDialog";

    /// The tabs' indices (Java: LAUNCH_CONDITIONS_IDX, SIMULATION_OPTIONS_IDX, WARNINGS_IDX,
    /// PLOT_IDX, EXPORT_IDX).
    static constexpr int kLaunchConditionsTab  = 0;
    static constexpr int kSimulationOptionsTab = 1;
    static constexpr int kWarningsTab          = 2;
    static constexpr int kPlotTab              = 3;
    static constexpr int kExportTab            = 4;

    /// The margin kept free at the right and the bottom of the screen's available area (Java:
    /// DIALOG_SCREEN_MARGIN).
    static constexpr int kScreenMargin = 80;
    /// The scroll step of the tabs' scroll areas (Java: setUnitIncrement(16)).
    static constexpr int kScrollStep = 16;

    /// The dialog over @p document's simulations of @p request (at least one, none null), opening
    /// on the request's tab, asking its questions and running through @p services; @p parent is
    /// the design window (or null). Not shown yet. Pauses the background estimate and adds the
    /// undo position (see the class comment).
    /// @throws BugError when the request has no simulation or a null one
    SimulationConfigDialog(const GuiServices& services, OpenRocketDocument& document,
                           const SimulationDialogs::EditRequest& request,
                           QWidget*                              parent = nullptr);
    ~SimulationConfigDialog() override;
    SimulationConfigDialog(const SimulationConfigDialog&)            = delete;
    SimulationConfigDialog& operator=(const SimulationConfigDialog&) = delete;
    SimulationConfigDialog(SimulationConfigDialog&&)                 = delete;
    SimulationConfigDialog& operator=(SimulationConfigDialog&&)      = delete;

    /// The production SimulationDialogs::editSimulations(): the dialog of @p request, parented to
    /// @p parent's window (or none), shown modal (ModalDialog: Qt::WindowModal), returning when
    /// it has closed (or was destroyed with its window).
    static void edit(QWidget* parent, const GuiServices& services, OpenRocketDocument& document,
                     const SimulationDialogs::EditRequest& request);

    /// The index of @p tab's page.
    [[nodiscard]] static int tabIndex(SimulationDialogs::Tab tab) noexcept;

    /// The title of a dialog over @p count simulations: "Edit simulation" for one,
    /// "Multi-simulation edit" for more.
    [[nodiscard]] static QString titleFor(std::size_t count);

    // ------------------------------------------------------------------------- the state

    /// The simulations, the first the one edited.
    [[nodiscard]] const std::vector<std::shared_ptr<Simulation>>& simulations() const noexcept
    {
        return m_simulations;
    }
    [[nodiscard]] bool isMultiEdit() const noexcept { return m_simulations.size() > 1; }
    [[nodiscard]] bool isNewSimulation() const noexcept { return m_newSimulation; }
    /// Whether the first simulation changed since the dialog was made (the title's "* ").
    [[nodiscard]] bool isModified() const noexcept { return m_modified; }
    /// Whether the dialog has closed (OK, Cancel, Close): its bindings are released.
    [[nodiscard]] bool isClosed() const noexcept { return m_closed; }

    // ----------------------------------------------------------------------- the widgets

    [[nodiscard]] QWidget&   topPanel() const noexcept { return *m_topPanel; }
    [[nodiscard]] QLabel&    nameLabel() const noexcept { return *m_nameLabel; }
    [[nodiscard]] QLineEdit& nameField() const noexcept { return *m_nameField; }
    [[nodiscard]] QLabel&    configurationLabel() const noexcept { return *m_configurationLabel; }
    [[nodiscard]] ConfigurationComboBox& configurationComboBox() const noexcept
    {
        return *m_configurationComboBox;
    }
    [[nodiscard]] QLabel& statusLabel() const noexcept { return *m_statusLabel; }
    /// The status description (simStatus).
    [[nodiscard]] QLabel& statusText() const noexcept { return *m_statusText; }

    [[nodiscard]] QTabWidget&  tabs() const noexcept { return *m_tabs; }
    [[nodiscard]] QScrollArea& launchConditionsScrollArea() const noexcept
    {
        return *m_conditionsScroll;
    }
    [[nodiscard]] SimulationConditionsPanel& conditionsPanel() const noexcept
    {
        return *m_conditionsPanel;
    }
    [[nodiscard]] QScrollArea& simulationOptionsScrollArea() const noexcept
    {
        return *m_optionsScroll;
    }
    [[nodiscard]] SimulationOptionsPanel&  optionsPanel() const noexcept { return *m_optionsPanel; }
    [[nodiscard]] SimulationWarningsPanel& warningsPanel() const noexcept
    {
        return *m_warningsPanel;
    }
    /// The plot tab's panel; null when the first simulation had no data when the dialog was made.
    [[nodiscard]] SimulationPlotPanel* plotPanel() const noexcept { return m_plotPanel; }

    [[nodiscard]] QWidget& bottomPanel() const noexcept { return *m_bottomPanel; }
    /// "Multi-simulation edit"; null unless in a multi-simulation edit.
    [[nodiscard]] StyledLabel* multiSimEditLabel() const noexcept { return m_multiSimEditLabel; }
    [[nodiscard]] QPushButton& cancelButton() const noexcept { return *m_cancelButton; }
    [[nodiscard]] QPushButton& okButton() const noexcept { return *m_okButton; }

    // ----------------------------------------------------------------------- the actions

    /// Selects @p tab's page (Java: switchToSettingsTab(), switchToWarningsTab(),
    /// switchToPlotTab(), switchToExportTab()), also a disabled one.
    void switchToTab(SimulationDialogs::Tab tab);

    /// The OK, Plot or Export button (see the class comment). Nothing once closed.
    void ok();

    /// The Cancel or Close button (Java: cancelClose(), see the class comment). Nothing once
    /// closed.
    void cancel();

    /// Escape and the window's close button: cancel() (Java's window listener).
    void reject() override;

protected:
    /// Clears every button's default and auto-default state (see the class comment).
    void showEvent(QShowEvent* event) override;

private:
    /// The top rows (Java: addTopPanel()), in @p layout of @p content.
    void buildTopPanel(QWidget& content, QVBoxLayout& layout);
    /// The tabs (Java's constructor), in @p layout of @p content.
    void buildTabs(QWidget& content, QVBoxLayout& layout);
    /// The buttons (Java: generateBottomPanel()).
    void buildBottomPanel();
    /// The buttons' texts for the selected tab (Java's tab listener).
    void updateButtons(int index);

    /// The first change of the first simulation (Java's one-shot change listener).
    void firstSimulationChanged();
    /// The name field's new text (Java's document listener).
    void nameEdited(const QString& text);

    /// Java's cancelSimEdit().
    void cancelSimulationEdit();
    /// Java's discardChanges(): the dialog closed, then the undo, the saved state and the event.
    void discardChanges();
    /// Java's copyChangesToAllSims().
    void copyChangesToAllSimulations();
    /// Java's getOutdatedSimulations().
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> outdatedSimulations() const;
    /// Runs @p simulations (UserPrompts::runSimulations()); false when the dialog was destroyed
    /// meanwhile.
    [[nodiscard]] bool runSimulations(std::vector<std::shared_ptr<Simulation>> simulations);

    /// Closes the dialog with @p result (Java: closeDialog()): the bindings released, the
    /// connections into the simulation ended, hidden. Nothing when closed already.
    void dispose(int result);
    /// Resumes the background estimate (EditRequest::pauseBackgroundRuns), once.
    void resumeBackgroundRuns();

    /// Java's pack() and capDialogToScreenBounds(), then the remembered geometry.
    void sizeAndPlace();
    /// Java's capDialogToScreenBounds().
    void capDialogToScreenBounds();

    const GuiServices*                                                m_services;
    OpenRocketDocument*                                               m_document;
    std::vector<std::shared_ptr<Simulation>>                          m_simulations;
    bool                                                              m_newSimulation;
    bool                                                              m_initialIsSaved;
    std::function<void(std::span<const std::shared_ptr<Simulation>>)> m_cancelBackgroundRuns;
    std::function<void(bool)>                                         m_pauseBackgroundRuns;
    /// The document's undo position right after the dialog's own undo position.
    std::size_t m_undoPosition{0};

    bool m_modified{false};
    bool m_closed{false};
    bool m_resumed{false};

    BindingScope               m_scope;
    Signal<>::ScopedConnection m_firstChange;

    QWidget*                   m_topPanel{nullptr};
    QLabel*                    m_nameLabel{nullptr};
    QLineEdit*                 m_nameField{nullptr};
    QLabel*                    m_configurationLabel{nullptr};
    ConfigurationComboBox*     m_configurationComboBox{nullptr};
    QLabel*                    m_statusLabel{nullptr};
    QLabel*                    m_statusText{nullptr};
    QTabWidget*                m_tabs{nullptr};
    QScrollArea*               m_conditionsScroll{nullptr};
    SimulationConditionsPanel* m_conditionsPanel{nullptr};
    QScrollArea*               m_optionsScroll{nullptr};
    SimulationOptionsPanel*    m_optionsPanel{nullptr};
    SimulationWarningsPanel*   m_warningsPanel{nullptr};
    SimulationPlotPanel*       m_plotPanel{nullptr};
    QWidget*                   m_bottomPanel{nullptr};
    StyledLabel*               m_multiSimEditLabel{nullptr};
    QPushButton*               m_cancelButton{nullptr};
    QPushButton*               m_okButton{nullptr};
    WindowGeometry*            m_geometry{nullptr};
};

}  // namespace QtRocket::Gui
