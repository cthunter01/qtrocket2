#pragma once

#include <QObject>

#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"

class QAction;
class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

class FlightConfigurationPanel;

/// The Stages tab of "Motors & Configuration" (OpenRocket's SeparationConfigurationPanel, swing
/// gui/main/flightconfigpanel/SeparationConfigurationPanel.java): the table of the configurations
/// and the stages of stage number above 0, boosters included (FlightConfigurablePanel), and under
/// it, on the right and of one width, the buttons Select separation and Reset separation. They act
/// while a stage's cell is selected (getSelectedComponent()), and are the panel's actions, which
/// the popup menu over a stage's column shows with the tab's Rename, Delete and Duplicate. Enter
/// and a double click run Select separation.
///
/// The operations (FlightConfigurationEditing's, over the selected rows and stage columns):
/// selectSeparation() (the separation question for the first cell), resetSeparation().
///
/// As in OpenRocket, the buttons start disabled and follow the selection from its first change on
/// (the table's first selection is made before its listeners: a switch to the tab refreshes
/// them). The Reset separation button is named as Java's field, "resetDeploymentButton".
class SeparationConfigurationPanel final : public FlightConfigurablePanel
{
    Q_OBJECT

public:
    /// The Stages panel of @p panel, a child of @p parent.
    SeparationConfigurationPanel(FlightConfigurationPanel& panel, QWidget* parent);
    ~SeparationConfigurationPanel() override;

    SeparationConfigurationPanel(const SeparationConfigurationPanel&)            = delete;
    SeparationConfigurationPanel& operator=(const SeparationConfigurationPanel&) = delete;
    SeparationConfigurationPanel(SeparationConfigurationPanel&&)                 = delete;
    SeparationConfigurationPanel& operator=(SeparationConfigurationPanel&&)      = delete;

    /// The actions ("selectSeparationAction", "resetSeparationAction") and their buttons
    /// ("selectSeparationButton", "resetDeploymentButton": Java's field names).
    [[nodiscard]] QAction* selectSeparationAction() const noexcept { return m_selectSeparation; }
    [[nodiscard]] QAction* resetSeparationAction() const noexcept { return m_resetSeparation; }
    [[nodiscard]] QPushButton* selectSeparationButton() const noexcept
    {
        return m_selectSeparationButton;
    }
    [[nodiscard]] QPushButton* resetSeparationButton() const noexcept
    {
        return m_resetSeparationButton;
    }

    /// The buttons enabled while a stage's cell is selected.
    void updateButtonState() override;
    /// Select separation.
    void selectValue() override;

    /// Select separation: the separation question for the first selected cell, its answer for
    /// every one.
    void selectSeparation();
    /// Reset separation of the selected cells.
    void resetSeparation();

protected:
    /// A NONFUNCTIONAL change of an AxialStage (its name: Java's stage table model's
    /// componentChanged()).
    [[nodiscard]] bool restructuresWith(const RocketComponent& source) const override;

private:
    QAction*     m_selectSeparation;
    QAction*     m_resetSeparation;
    QPushButton* m_selectSeparationButton{nullptr};
    QPushButton* m_resetSeparationButton{nullptr};
};

}  // namespace QtRocket::Gui
