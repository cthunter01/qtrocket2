#pragma once

#include <array>
#include <span>

#include <QObject>
#include <QPointer>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Uuid.h"

class QAction;
class QMenu;
class QPoint;
class QTabWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DocumentSelectionModel;
class FlightConfigurablePanel;
class FlightConfigurationEditing;
class IconButton;
class MotorConfigurationPanel;
class RecoveryConfigurationPanel;
class SeparationConfigurationPanel;

/// The "Motors & Configuration" tab of a design window (OpenRocket's FlightConfigurationPanel,
/// swing gui/main/FlightConfigurationPanel.java): a row of four buttons, New Configuration,
/// Rename Configuration, Delete Configuration and Duplicate Configuration (IconButtons tied to
/// the tab's actions, centred, 12 pixels apart: "skip 1, gapright para" in "[grow][][][][][grow]"),
/// and under them the sub-tabs Motors (MotorConfigurationPanel), Recovery
/// (RecoveryConfigurationPanel) and Stages (SeparationConfigurationPanel). Its operations are
/// FlightConfigurationEditing's (editing()), which the window connects (its requests to close the
/// configuration dialog, the open documents of "Save as default"); the design's selection it sets
/// is the window's (setDesignSelection()).
///
/// THE ENABLE RULES (updateButtonState(), at its making and at every change of the rocket):
/// Rename, Delete and Duplicate act while the rocket's selected configuration is not the default
/// one; New while the rocket has an acting motor mount; the Recovery tab is enabled while there are
/// an acting motor mount and a recovery device, the Stages tab while there are an acting motor
/// mount and more than one stage. The actions are what the buttons and the popup items show:
/// a disabled button's popup item is disabled too.
///
/// THE SUB-TABS: a switch selects in the new sub-tab's table the rows the previous one had
/// selected (in column 1, or nothing; synchronizePanelSelection()), refreshes its buttons, gives
/// its table the focus and makes the components of its selected columns the design's selection
/// (takeTheSpotlight(), updateRocketViewSelection()). At every change of the rocket each panel's
/// table follows the rocket's selected configuration (synchronizeConfigurationSelection()), once
/// a change that Java's table models answer with a change of their structure (a MOTOR or TREE
/// change, an undo among them) has cleared it (FlightConfigurablePanel::restructure()): the
/// rocket's selected configuration in the last column then.
///
/// THE OPERATIONS (the actions' slots: newConfigurationAction() and the others; Java's
/// newOrDuplicateConfigAction(), renameConfigurationAction(), removeConfigurationAction()) act on
/// the current sub-tab's selected rows, then leave each table's selection as Java's
/// configurationChanged() does (FlightConfigurablePanel::restoreSelection() of the three panels,
/// in order): New, then the current sub-tab's Select operation over the new row (the motor chooser,
/// the deployment or the separation question), Duplicate, Rename (when a name changed), Delete
/// (then the focus: takeTheSpotlight()).
///
/// The panel keeps no component (the models and the operations look them up by id). The document
/// and the services must outlive it.
///
/// Deviations from OpenRocket:
/// - The popup items are the buttons' actions, so a disabled button's item is disabled too (U-M3-6
///   d); Java disables the buttons, not their actions, and its popup items always act.
/// - A disabled current sub-tab moves to the Motors tab (U-M3-6 f): Java moves there only when the
///   last recovery device goes from the Recovery tab or the last stage but one from the Stages tab,
///   and leaves a sub-tab current that is disabled for want of an acting motor mount.
/// - New's menu item shows no icon (Java: the action has a large icon only, which menus do not
///   show), as Qt's QAction::setIconVisibleInMenu(false) does.
class FlightConfigurationPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The sub-tabs' indexes (MOTOR_TAB_INDEX, RECOVERY_TAB_INDEX, SEPARATION_TAB_INDEX).
    static constexpr int kMotorTabIndex      = 0;
    static constexpr int kRecoveryTabIndex   = 1;
    static constexpr int kSeparationTabIndex = 2;

    /// The tab of @p document, asking through @p services (both outlive it).
    FlightConfigurationPanel(OpenRocketDocument& document, const GuiServices& services,
                             QWidget* parent = nullptr);
    ~FlightConfigurationPanel() override;

    FlightConfigurationPanel(const FlightConfigurationPanel&)            = delete;
    FlightConfigurationPanel& operator=(const FlightConfigurationPanel&) = delete;
    FlightConfigurationPanel(FlightConfigurationPanel&&)                 = delete;
    FlightConfigurationPanel& operator=(FlightConfigurationPanel&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&             rocket() const noexcept;
    [[nodiscard]] const GuiServices&  services() const noexcept { return m_services; }
    /// The tab's operations (their dialog parent is the panel).
    [[nodiscard]] FlightConfigurationEditing& editing() const noexcept { return *m_editing; }

    // ------------------------------------------------------------------ the widgets

    /// The four actions ("newConfigAction", "renameConfigAction", "deleteConfigAction",
    /// "duplicateConfigAction").
    [[nodiscard]] QAction* newConfigurationAction() const noexcept { return m_newAction; }
    [[nodiscard]] QAction* renameConfigurationAction() const noexcept { return m_renameAction; }
    [[nodiscard]] QAction* deleteConfigurationAction() const noexcept { return m_deleteAction; }
    [[nodiscard]] QAction* duplicateConfigurationAction() const noexcept
    {
        return m_duplicateAction;
    }
    /// The four buttons ("newConfButton", "renameConfButton", "removeConfButton",
    /// "duplicateConfButton").
    [[nodiscard]] IconButton* newConfigurationButton() const noexcept { return m_newButton; }
    [[nodiscard]] IconButton* renameConfigurationButton() const noexcept { return m_renameButton; }
    [[nodiscard]] IconButton* deleteConfigurationButton() const noexcept { return m_deleteButton; }
    [[nodiscard]] IconButton* duplicateConfigurationButton() const noexcept
    {
        return m_duplicateButton;
    }
    /// The popup menu of column 0 ("popupMenuConfig"): New, Rename, Delete, Duplicate.
    [[nodiscard]] QMenu& configurationPopupMenu() const noexcept { return *m_configPopup; }
    /// The sub-tabs ("tabs").
    [[nodiscard]] QTabWidget*              tabs() const noexcept { return m_tabs; }
    [[nodiscard]] MotorConfigurationPanel& motorConfigurationPanel() const noexcept
    {
        return *m_motorPanel;
    }
    [[nodiscard]] RecoveryConfigurationPanel& recoveryConfigurationPanel() const noexcept
    {
        return *m_recoveryPanel;
    }
    [[nodiscard]] SeparationConfigurationPanel& separationConfigurationPanel() const noexcept
    {
        return *m_separationPanel;
    }
    /// The panel of the current sub-tab.
    [[nodiscard]] FlightConfigurablePanel& currentPanel() const;

    // ------------------------------------------------------------------ the window's parts

    /// Sets the design's selection that the tables' columns set (the window's: DesignPanel's
    /// selection(); BasicFrame.setSelectedComponents()); null (the default): none.
    void setDesignSelection(DocumentSelectionModel* selection);
    /// The design's selection; null when none is set (or it went).
    [[nodiscard]] DocumentSelectionModel* designSelection() const noexcept;
    /// Selects the components @p components (by id; those that are not in the rocket left out) in
    /// the design (setSelectedComponents()); nothing without a design selection.
    void setSelectedComponents(std::span<const Uuid> components);

    /// Gives the current sub-tab's table the focus and makes the components of its selected
    /// columns the design's selection (takeTheSpotlight(): the window calls it when the tab is
    /// chosen).
    void takeTheSpotlight() const;

    /// Whether the panel is answering a change event of the rocket (the panels' re-entrancy
    /// guard).
    [[nodiscard]] bool isHandlingRocketChange() const noexcept { return m_rocketChanges > 0; }

    // ------------------------------------------------------------------ the operations

    /// New Configuration (see the class comment).
    void newConfiguration();
    /// Duplicate Configuration of the current sub-tab's selected rows.
    void duplicateConfigurations();
    /// Rename Configuration of the current sub-tab's selected rows.
    void renameConfigurations();
    /// Delete Configuration of the current sub-tab's selected rows.
    void removeConfigurations();

    /// Opens the popup menu of column 0 at @p globalPosition (doPopupConfig()).
    void showConfigurationPopup(const QPoint& globalPosition);

private:
    /// Names the actions and connects them.
    void configureActions();
    void makeButtons(QWidget& row);
    /// The sub-tab @p index became the current one.
    void tabChanged(int index);
    /// The rocket changed (stateChanged()).
    void rocketChanged(const ComponentChangeEvent& event);
    /// Enables the actions and the sub-tabs (updateButtonState()).
    void updateButtonState();
    /// Every panel's synchronizeConfigurationSelection().
    void synchronizeAll();
    /// Every panel's restoreSelection() (configurationChanged()).
    void restoreAll();
    /// The three panels, in the order of the sub-tabs.
    [[nodiscard]] std::array<FlightConfigurablePanel*, 3> panels() const noexcept;
    /// The panel of the sub-tab @p index; null for none.
    [[nodiscard]] FlightConfigurablePanel* panelAt(int index) const;

    OpenRocketDocument*                     m_document;
    GuiServices                             m_services;
    FlightConfigurationEditing*             m_editing;
    QAction*                                m_newAction;
    QAction*                                m_renameAction;
    QAction*                                m_deleteAction;
    QAction*                                m_duplicateAction;
    QMenu*                                  m_configPopup;
    QTabWidget*                             m_tabs;
    MotorConfigurationPanel*                m_motorPanel;
    RecoveryConfigurationPanel*             m_recoveryPanel;
    SeparationConfigurationPanel*           m_separationPanel;
    IconButton*                             m_newButton{nullptr};
    IconButton*                             m_renameButton{nullptr};
    IconButton*                             m_deleteButton{nullptr};
    IconButton*                             m_duplicateButton{nullptr};
    FlightConfigurablePanel*                m_previousPanel;
    QPointer<DocumentSelectionModel>        m_designSelection;
    int                                     m_rocketChanges{0};
    ComponentChangeSignal::ScopedConnection m_changes;
};

}  // namespace QtRocket::Gui
