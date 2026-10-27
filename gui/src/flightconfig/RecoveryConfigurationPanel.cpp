#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"

#include <array>
#include <vector>

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

RecoveryConfigurationPanel::RecoveryConfigurationPanel(FlightConfigurationPanel& panel,
                                                       QWidget*                  parent)
  : FlightConfigurablePanel(panel, FlightConfigurableTableModel::Kind::RECOVERY_DEVICES, parent),
    //: edtmotorconfdlg.but.Selectdeployment
    m_selectDeployment(makeAction(tr("Select deployment"), QStringLiteral("selectDeploymentAction"),
                                  [this] { selectDeployment(); })),
    //: edtmotorconfdlg.but.Resetdeployment
    m_resetDeployment(makeAction(tr("Reset deployment"), QStringLiteral("resetDeploymentAction"),
                                 [this] { resetDeployment(); }))
{
    setObjectName(QStringLiteral("recoveryConfigurationPanel"));

    // Java's popupMenuFull.
    QMenu&                          menu  = fullPopupMenu();
    const FlightConfigurationPanel& owner = flightConfigurationPanel();
    menu.addAction(m_selectDeployment);
    menu.addAction(m_resetDeployment);
    menu.addSeparator();
    menu.addAction(owner.renameConfigurationAction());
    menu.addAction(owner.deleteConfigurationAction());
    menu.addAction(owner.duplicateConfigurationAction());

    // Java: the buttons start disabled; the selection's listeners enable them.
    m_selectDeployment->setEnabled(false);
    m_resetDeployment->setEnabled(false);

    // MigLayout("fill"): the table ("span, grow, pushy, wrap"), then the buttons on the right,
    // of one width.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(&table(), 1);
    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    m_selectDeploymentButton =
        makeButton(*m_selectDeployment, QStringLiteral("selectDeploymentButton"), row);
    m_resetDeploymentButton =
        makeButton(*m_resetDeployment, QStringLiteral("resetDeploymentButton"), row);
    rowLayout->addWidget(m_selectDeploymentButton);
    rowLayout->addWidget(m_resetDeploymentButton);
    const std::array<QPushButton*, 2> buttons{m_selectDeploymentButton, m_resetDeploymentButton};
    shareWidth(buttons);
    layout->addWidget(row);
}

RecoveryConfigurationPanel::~RecoveryConfigurationPanel() = default;

void RecoveryConfigurationPanel::updateButtonState()
{
    const bool selected = selectedComponent() != nullptr;
    m_selectDeployment->setEnabled(selected);
    m_resetDeployment->setEnabled(selected);
}

void RecoveryConfigurationPanel::selectValue()
{
    selectDeployment();
}

void RecoveryConfigurationPanel::selectDeployment()
{
    const std::vector<Uuid>                  devices = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids     = selectedConfigurationIds();
    if (devices.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &devices] { return editing().selectDeployment(ids, devices); });
}

void RecoveryConfigurationPanel::resetDeployment()
{
    const std::vector<Uuid>                  devices = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids     = selectedConfigurationIds();
    if (devices.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &devices] { return editing().resetDeployment(ids, devices); });
}

}  // namespace QtRocket::Gui
