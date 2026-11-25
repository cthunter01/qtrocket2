#pragma once

#include <span>
#include <vector>

#include <QMainWindow>
#include <QString>

#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/util/GuiServices.h"
// RocketComponent is complete here, not declared: moc's metatype of a signal's
// RocketComponent* checks for completeness, and GCC's -Wsfinae-incomplete rejects a class that
// another moc file of the same compilation sees complete later.
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Signal.h"

class QCloseEvent;
class QSplitter;
class QTabWidget;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

class DesignPanel;
class FlightConfigurationPanel;
class MainMenus;
class RocketPanel;
class SimulationPanel;
class SplitPane;
class UndoRedoActions;
class WindowGeometry;

/// The window of one design (OpenRocket's BasicFrame): the menus, and a vertical splitter with
/// the tabs above ("Rocket design", the design panel of the component tree and the add buttons,
/// "Motors & Configuration", the flight configuration panel, and "Flight simulations", the
/// simulations panel) and the rocket's figure below (the rocket panel). The document belongs to
/// the WindowManager, which makes the window and destroys it before the document.
///
/// - The design panel, the flight configuration panel, the simulations panel and the rocket panel
///   are placeholders (empty widgets named designPanel, flightConfigurationPanel,
///   simulationPanel and rocketPanel) until a panel takes their places through setDesignPanel(),
///   setFlightConfigurationPanel(), setSimulationPanel() and setRocketPanel(): the WindowManager
///   puts a DesignPanel, a FlightConfigurationPanel, a SimulationPanel and a RocketPanel into
///   every window it opens.
/// - When another tab becomes the current one (the user's click, selectTab()), its panel takes
///   the spotlight (BasicFrame.stateChanged()): the design tab's DesignPanel gives the component
///   tree the keyboard focus, the Motors & Configuration tab's FlightConfigurationPanel gives its
///   current sub-tab's table the focus and makes the components of the table's selected columns
///   the design's selection (FlightConfigurationPanel::takeTheSpotlight()), the Flight
///   simulations tab's SimulationPanel gives its table the focus and selects the simulations it
///   remembered (or its first row) when none is selected, then runs the simulations that are out
///   of date when the preferences ask for it (SimulationPanel::takeTheSpotlight() and
///   activating()). A SimulationPanel whose tab stops being the current one remembers its
///   selection first (updatePreviousSelection()). A placeholder takes nothing, and nothing
///   happens while a panel takes a placeholder's place.
/// - The title is BasicFrame.setTitle()'s: the rocket's name, then " (<file name>)" when the
///   document has a file, marked modified (the "*" of BasicFrame, "[*]" in Qt's title) when the
///   document is not saved. It is renewed whenever the document changes (every rocket event comes
///   to the document too); setSaved() and setFile() change nothing that the document tells, so
///   whoever calls them calls refreshTitle(). The file is also the window's file path (the
///   proxy icon of macOS).
/// - The size is BasicFrame's: 90 % of the screen, then the geometry the last main window had
///   (WindowGeometry "MainWindow"), and the splitter's divider at 0.4 of its height when the
///   window is first shown (JSplitPane.setDividerLocation(0.4)); a change of its height goes half
///   to each side (resizeWeight 0.5: SplitPane). Where a further window opens is the
///   WindowManager's (WindowGeometry::cascade()).
/// - Closing the window (its close button, the platform) closes nothing by itself: it asks the
///   WindowManager through closeRequested(), as BasicFrame's DO_NOTHING_ON_CLOSE frame calls
///   closeAction().
/// - The component configuration dialog is the application's (ComponentConfigDialog, one at a
///   time); the window does not open it: every request for it, from the window's design panel
///   (the component tree, Edit, the add buttons) and from whoever calls requestConfigDialog(),
///   comes out as one signal, configDialogRequested(), and every request to close it as
///   closeConfigDialogRequested(), which the application connects to the dialog (decision O12:
///   ConfigDialogRequests).
/// - The simulations of the window run off the GUI thread as jobs of the window
///   (GuiServices::simulationExecutor(): the run dialog's, the design view's estimate); as the
///   window is destroyed, before the objects it owns go, it abandons them
///   (SimulationExecutor::abandonAll()), so that no callback of a job reaches the objects of a
///   window that is gone.
///
/// Deviations: an "[*]" in the rocket's name is shown with a zero-width space after the "*"
/// ("[*\u200B]"), since Qt takes every "[*]" of a title for the place of the modified mark; the
/// splitter's sides cannot be collapsed (JSplitPane keeps their minimum sizes too); the tab
/// "Motors & Configuration" has its "&" doubled in Qt's text (tabText() is "Motors &&
/// Configuration"), since Qt reads an "&" as the mark of a mnemonic, where Swing shows the text
/// as it is; a simulations table shows a change of the units or of a preference it shows, which
/// no document event tells, once it is read again (refreshSimulationTable(), which the
/// preferences dialog's refresh of the design views calls through the WindowManager: critic G6),
/// where Java's table renders its cells anew at every paint.
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /// JSplitPane.setDividerLocation(0.4) of the splitter.
    static constexpr double kDividerLocation = 0.4;
    /// JSplitPane.setResizeWeight(0.5) of the splitter: the top side's part of a change.
    static constexpr double kResizeWeight = 0.5;
    /// The tabs' indexes (BasicFrame.DESIGN_TAB, FLIGHT_CONFIGURATION_TAB and SIMULATION_TAB).
    static constexpr int kDesignTab              = 0;
    static constexpr int kFlightConfigurationTab = 1;
    static constexpr int kSimulationTab          = 2;

    /// The window of @p document, which must outlive it, with the application's @p services
    /// (whose objects outlive it too).
    MainWindow(OpenRocketDocument& document, const GuiServices& services);
    /// Abandons the window's simulation jobs (see the class comment), then goes with its parts.
    ~MainWindow() override;

    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&)                 = delete;
    MainWindow& operator=(MainWindow&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] const GuiServices&  services() const noexcept { return m_services; }
    [[nodiscard]] MainMenus&          menus() const noexcept { return *m_menus; }
    [[nodiscard]] UndoRedoActions&    undoRedoActions() const noexcept { return *m_undoRedo; }

    /// The tabs ("tabbedPane"): "Rocket design" (kDesignTab), "Motors & Configuration"
    /// (kFlightConfigurationTab) and "Flight simulations" (kSimulationTab).
    [[nodiscard]] QTabWidget* tabs() const noexcept { return m_tabs; }
    /// The vertical splitter ("splitPane", a SplitPane): the tabs and the rocket panel.
    [[nodiscard]] QSplitter* splitter() const noexcept;
    /// The widget of the "Rocket design" tab.
    [[nodiscard]] QWidget* designPanel() const noexcept { return m_designPanel; }
    /// The widget of the "Rocket design" tab when it is a DesignPanel (the WindowManager puts one
    /// into every window it opens); null otherwise.
    [[nodiscard]] DesignPanel* componentDesignPanel() const;
    /// The widget of the "Motors & Configuration" tab.
    [[nodiscard]] QWidget* flightConfigurationPanel() const noexcept
    {
        return m_flightConfigurationPanel;
    }
    /// The widget of the "Motors & Configuration" tab when it is a FlightConfigurationPanel (the
    /// WindowManager puts one into every window it opens); null otherwise.
    [[nodiscard]] FlightConfigurationPanel* componentFlightConfigurationPanel() const;
    /// The widget of the "Flight simulations" tab.
    [[nodiscard]] QWidget* simulationPanel() const noexcept { return m_simulationPanel; }
    /// The widget of the "Flight simulations" tab when it is a SimulationPanel (the WindowManager
    /// puts one into every window it opens); null otherwise.
    [[nodiscard]] SimulationPanel* componentSimulationPanel() const;
    /// The widget below the tabs.
    [[nodiscard]] QWidget* rocketPanel() const noexcept { return m_rocketPanel; }
    /// The widget below the tabs when it is a RocketPanel (the WindowManager puts one into every
    /// window it opens); null otherwise.
    [[nodiscard]] RocketPanel* componentRocketPanel() const;

    /// Puts @p panel (not null, no tab's widget) into the "Rocket design" tab, which takes it
    /// over, in place of the design panel there, which is deleted later. The current tab stays
    /// the current one.
    void setDesignPanel(QWidget* panel);
    /// Puts @p panel (not null, no tab's widget) into the "Motors & Configuration" tab in the
    /// same way, in place of the flight configuration panel there.
    void setFlightConfigurationPanel(QWidget* panel);
    /// Puts @p panel (not null, no tab's widget) into the "Flight simulations" tab in the same
    /// way, in place of the simulations panel there.
    void setSimulationPanel(QWidget* panel);
    /// Puts @p panel (not null) below the tabs, which takes it over, in place of the rocket
    /// panel there, which is deleted later.
    void setRocketPanel(QWidget* panel);

    /// Makes the tab @p tab (kDesignTab, kFlightConfigurationTab, kSimulationTab) the current one
    /// (BasicFrame.selectTab()); its panel takes the spotlight when it was not the current one
    /// (see the class comment).
    /// @throws BugError when @p tab is not a tab's index
    void selectTab(int tab);
    /// The index of the current tab (BasicFrame.getSelectedTab()).
    [[nodiscard]] int selectedTab() const;

    /// Renews the title, the modified mark and the file path from the document.
    void refreshTitle();

    /// Reads the simulations table of the "Flight simulations" tab again (SimulationTableModel::
    /// refresh(): its statuses and cells in the default units of now, its selection kept), after
    /// a change of the units or of a preference the table shows, which no document event tells;
    /// nothing while the tab holds no SimulationPanel. The WindowManager calls it whenever the
    /// preferences dialog refreshes the design views (critic G6).
    void refreshSimulationTable() const;

    /// The title of the window of @p document, with Qt's modified mark: "[*]<rocket name>" and
    /// " (<file name>)" when the document has a file (BasicFrame.setTitle()).
    [[nodiscard]] static QString titleFor(const OpenRocketDocument& document);

    /// Asks for the configuration dialog of @p component, a component of the window's rocket,
    /// with @p others (ComponentConfigDialog.showDialog()): emits configDialogRequested().
    /// @throws BugError when @p component or one of @p others (or a null among them) is not in
    ///         the window's rocket
    void requestConfigDialog(RocketComponent&                  component,
                             std::span<RocketComponent* const> others = {},
                             bool isNewComponent = false, bool rememberPreviousTab = true);

    /// Asks for the configuration dialog to close (ComponentConfigDialog.disposeDialog()):
    /// emits closeConfigDialogRequested().
    void requestConfigDialogClose();

Q_SIGNALS:
    /// The window was asked to close (its close button, the platform); the WindowManager closes
    /// it, asking first to save the design.
    void closeRequested();

    /// The configuration dialog of @p component (never null) is asked for, @p others being the
    /// other selected components its edits reach too (multi-edit, decision U3; none for a
    /// component's dialog alone), @p isNewComponent when the component was just added (its
    /// addition is the dialog's undo step: OpenRocket's isNewComponent) and
    /// @p rememberPreviousTab when the dialog is to open on the tab the previous one had
    /// (ComponentConfigDialog.showDialog(); false for a new component). The application
    /// connects it to the dialog (ConfigDialogRequests); without that nothing opens. The pointers
    /// are valid during the emission only: connect directly.
    void configDialogRequested(QtRocket::RocketComponent*                     component,
                               const std::vector<QtRocket::RocketComponent*>& others,
                               bool isNewComponent, bool rememberPreviousTab);

    /// The configuration dialog is to close (ComponentConfigDialog.disposeDialog()): before an
    /// action changes the design (RocketActions), before the question whether to save a design
    /// that is closing and after its window closed (BasicFrame.closeAction()). Connect directly.
    void closeConfigDialogRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    /// Puts @p panel into the tab of @p slot (m_designPanel, m_flightConfigurationPanel or
    /// m_simulationPanel) in place of its widget, which is deleted later; the current tab stays.
    void replaceTab(QWidget*& slot, QWidget* panel);
    /// The tab @p index became the current one (BasicFrame.stateChanged()).
    void tabChanged(int index);

    OpenRocketDocument*                                  m_document;
    GuiServices                                          m_services;
    UndoRedoActions*                                     m_undoRedo;
    MainMenus*                                           m_menus;
    SplitPane*                                           m_splitter;
    QTabWidget*                                          m_tabs;
    QWidget*                                             m_designPanel;
    QWidget*                                             m_flightConfigurationPanel;
    QWidget*                                             m_simulationPanel;
    QWidget*                                             m_rocketPanel;
    WindowGeometry*                                      m_geometry{nullptr};
    Signal<const DocumentChangeEvent&>::ScopedConnection m_documentChanged;
    /// While a panel takes a placeholder's place (its tab's changes take no spotlight).
    bool m_replacingTab{false};
    /// The tab that was the current one before the last change (BasicFrame's previousTab).
    int m_previousTab{kDesignTab};
};

}  // namespace QtRocket::Gui
