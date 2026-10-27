#pragma once

#include <QObject>
#include <QString>

#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"

class QAction;
class QGroupBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QWidget;

namespace QtRocket::Gui
{

class FlightConfigurationPanel;
class MotorCellDelegate;
class MotorMountConfigurationPanel;

/// The Motors tab of "Motors & Configuration" (OpenRocket's MotorConfigurationPanel, swing
/// gui/main/flightconfigpanel/MotorConfigurationPanel.java): on the left the box "Motor mounts"
/// (its title in bold) with the check table of the tubes that act as motor mounts
/// (MotorMountConfigurationPanel), and beside it ("gapleft para", growing) two cards:
/// - the help ("helpText"): "No motor mounts defined. Add a tube in the 'Rocket design' tab and
///   select it from the list on the left.", while the table has no mount column;
/// - the box "Motor configurations" (bold), with the table of the configurations and the acting
///   mounts (FlightConfigurablePanel, its motor cells drawn by MotorCellDelegate) and under it,
///   on the right and of one width, the buttons Select motor, Delete motor, then (12 pixels
///   further) Select ignition and Reset ignition, otherwise.
/// The four buttons act while a mount's cell is selected (the first selected cell is a mount's:
/// getSelectedComponent()); they are the panel's actions, which the popup menu over a mount's
/// column shows with the tab's Rename, Delete and Duplicate. Enter and a double click run Select
/// motor.
///
/// The operations (FlightConfigurationEditing's, over the selected rows and mount columns):
/// selectMotor() (the motor chooser for the first cell), deleteMotor(), selectIgnition() (the
/// ignition question), resetIgnition().
class MotorConfigurationPanel final : public FlightConfigurablePanel
{
    Q_OBJECT

public:
    /// The Motors panel of @p panel, a child of @p parent.
    MotorConfigurationPanel(FlightConfigurationPanel& panel, QWidget* parent);
    ~MotorConfigurationPanel() override;

    MotorConfigurationPanel(const MotorConfigurationPanel&)            = delete;
    MotorConfigurationPanel& operator=(const MotorConfigurationPanel&) = delete;
    MotorConfigurationPanel(MotorConfigurationPanel&&)                 = delete;
    MotorConfigurationPanel& operator=(MotorConfigurationPanel&&)      = delete;

    /// The box "Motor mounts" ("motorMountsBox") and its table.
    [[nodiscard]] QGroupBox* motorMountsBox() const noexcept { return m_mountsBox; }
    [[nodiscard]] MotorMountConfigurationPanel& motorMountConfigurationPanel() const noexcept
    {
        return *m_mountPanel;
    }
    /// The cards ("cards"), the help ("helpText") and the box "Motor configurations"
    /// ("configurationPanel").
    [[nodiscard]] QStackedWidget* cards() const noexcept { return m_cards; }
    [[nodiscard]] QLabel*         helpText() const noexcept { return m_helpText; }
    [[nodiscard]] QGroupBox*      configurationBox() const noexcept { return m_configurationBox; }
    /// Whether the help is the card shown (the table has no mount column).
    [[nodiscard]] bool isShowingHelp() const;
    /// The cell delegate of the table.
    [[nodiscard]] MotorCellDelegate& cellDelegate() const noexcept { return *m_delegate; }

    /// The actions ("selectMotorAction", "deleteMotorAction", "selectIgnitionAction",
    /// "resetIgnitionAction") and their buttons ("selectMotorButton", "deleteMotorButton",
    /// "selectIgnitionButton", "resetIgnitionButton").
    [[nodiscard]] QAction*     selectMotorAction() const noexcept { return m_selectMotor; }
    [[nodiscard]] QAction*     deleteMotorAction() const noexcept { return m_deleteMotor; }
    [[nodiscard]] QAction*     selectIgnitionAction() const noexcept { return m_selectIgnition; }
    [[nodiscard]] QAction*     resetIgnitionAction() const noexcept { return m_resetIgnition; }
    [[nodiscard]] QPushButton* selectMotorButton() const noexcept { return m_selectMotorButton; }
    [[nodiscard]] QPushButton* deleteMotorButton() const noexcept { return m_deleteMotorButton; }
    [[nodiscard]] QPushButton* selectIgnitionButton() const noexcept
    {
        return m_selectIgnitionButton;
    }
    [[nodiscard]] QPushButton* resetIgnitionButton() const noexcept
    {
        return m_resetIgnitionButton;
    }

    /// The help card while the table has no mount column, else the table's, with the buttons
    /// enabled while a mount's cell is selected.
    void updateButtonState() override;
    /// Select motor.
    void selectValue() override;

    /// Select motor: the motor chooser for the first selected cell, its choice for every one.
    void selectMotor();
    /// Delete motor: no motor in the selected cells.
    void deleteMotor();
    /// Select ignition: the ignition question for the first selected cell, its answer for every
    /// one.
    void selectIgnition();
    /// Reset ignition of the selected cells.
    void resetIgnition();

protected:
    /// A NONFUNCTIONAL change of a BodyTube or an InnerTube (its name: Java's motor table model's
    /// componentChanged()).
    [[nodiscard]] bool restructuresWith(const RocketComponent& source) const override;

private:
    /// The help's text (MotorConfigurationPanel.lbl.nomotors with the design tab's name).
    [[nodiscard]] static QString noMountsText();
    /// Makes the actions and the popup menu.
    void makeActions();
    /// Lays the box "Motor mounts" out.
    void arrangeMountsBox();
    /// Lays the box "Motor configurations" out: the table and the buttons.
    void arrangeConfigurationBox();

    MotorCellDelegate*            m_delegate;
    QAction*                      m_selectMotor{nullptr};
    QAction*                      m_deleteMotor{nullptr};
    QAction*                      m_selectIgnition{nullptr};
    QAction*                      m_resetIgnition{nullptr};
    QGroupBox*                    m_mountsBox;
    MotorMountConfigurationPanel* m_mountPanel;
    QStackedWidget*               m_cards;
    QLabel*                       m_helpText;
    QGroupBox*                    m_configurationBox;
    QPushButton*                  m_selectMotorButton{nullptr};
    QPushButton*                  m_deleteMotorButton{nullptr};
    QPushButton*                  m_selectIgnitionButton{nullptr};
    QPushButton*                  m_resetIgnitionButton{nullptr};
};

}  // namespace QtRocket::Gui
