#pragma once

#include <QObject>

#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"

class QAction;
class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

class FlightConfigurationPanel;

/// The Recovery tab of "Motors & Configuration" (OpenRocket's RecoveryConfigurationPanel, swing
/// gui/main/flightconfigpanel/RecoveryConfigurationPanel.java): the table of the configurations
/// and every recovery device (FlightConfigurablePanel), and under it, on the right and of one
/// width, the buttons Select deployment and Reset deployment. They act while a device's cell is
/// selected (getSelectedComponent()), and are the panel's actions, which the popup menu over a
/// device's column shows with the tab's Rename, Delete and Duplicate. Enter and a double click run
/// Select deployment.
///
/// The operations (FlightConfigurationEditing's, over the selected rows and device columns):
/// selectDeployment() (the deployment question for the first cell), resetDeployment().
///
/// As in OpenRocket, the buttons start disabled and follow the selection from its first change on
/// (the table's first selection is made before its listeners: a switch to the tab refreshes
/// them).
class RecoveryConfigurationPanel final : public FlightConfigurablePanel
{
    Q_OBJECT

public:
    /// The Recovery panel of @p panel, a child of @p parent.
    RecoveryConfigurationPanel(FlightConfigurationPanel& panel, QWidget* parent);
    ~RecoveryConfigurationPanel() override;

    RecoveryConfigurationPanel(const RecoveryConfigurationPanel&)            = delete;
    RecoveryConfigurationPanel& operator=(const RecoveryConfigurationPanel&) = delete;
    RecoveryConfigurationPanel(RecoveryConfigurationPanel&&)                 = delete;
    RecoveryConfigurationPanel& operator=(RecoveryConfigurationPanel&&)      = delete;

    /// The actions ("selectDeploymentAction", "resetDeploymentAction") and their buttons
    /// ("selectDeploymentButton", "resetDeploymentButton").
    [[nodiscard]] QAction* selectDeploymentAction() const noexcept { return m_selectDeployment; }
    [[nodiscard]] QAction* resetDeploymentAction() const noexcept { return m_resetDeployment; }
    [[nodiscard]] QPushButton* selectDeploymentButton() const noexcept
    {
        return m_selectDeploymentButton;
    }
    [[nodiscard]] QPushButton* resetDeploymentButton() const noexcept
    {
        return m_resetDeploymentButton;
    }

    /// The buttons enabled while a device's cell is selected.
    void updateButtonState() override;
    /// Select deployment.
    void selectValue() override;

    /// Select deployment: the deployment question for the first selected cell, its answer for
    /// every one.
    void selectDeployment();
    /// Reset deployment of the selected cells.
    void resetDeployment();

private:
    QAction*     m_selectDeployment;
    QAction*     m_resetDeployment;
    QPushButton* m_selectDeploymentButton{nullptr};
    QPushButton* m_resetDeploymentButton{nullptr};
};

}  // namespace QtRocket::Gui
