#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"

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
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

SeparationConfigurationPanel::SeparationConfigurationPanel(FlightConfigurationPanel& panel,
                                                           QWidget*                  parent)
  : FlightConfigurablePanel(panel, FlightConfigurableTableModel::Kind::STAGES, parent),
    //: edtmotorconfdlg.but.Selectseparation
    m_selectSeparation(makeAction(tr("Select separation"), QStringLiteral("selectSeparationAction"),
                                  [this] { selectSeparation(); })),
    //: edtmotorconfdlg.but.Resetseparation
    m_resetSeparation(makeAction(tr("Reset separation"), QStringLiteral("resetSeparationAction"),
                                 [this] { resetSeparation(); }))
{
    setObjectName(QStringLiteral("separationConfigurationPanel"));

    // Java's popupMenuFull.
    QMenu&                          menu  = fullPopupMenu();
    const FlightConfigurationPanel& owner = flightConfigurationPanel();
    menu.addAction(m_selectSeparation);
    menu.addAction(m_resetSeparation);
    menu.addSeparator();
    menu.addAction(owner.renameConfigurationAction());
    menu.addAction(owner.deleteConfigurationAction());
    menu.addAction(owner.duplicateConfigurationAction());

    // Java: the buttons start disabled; the selection's listeners enable them.
    m_selectSeparation->setEnabled(false);
    m_resetSeparation->setEnabled(false);

    // MigLayout("fill"): the table ("span, grow, pushy, wrap"), then the buttons on the right,
    // of one width.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(&table(), 1);
    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    m_selectSeparationButton =
        makeButton(*m_selectSeparation, QStringLiteral("selectSeparationButton"), row);
    // Java's field of the Reset separation button is named resetDeploymentButton.
    m_resetSeparationButton =
        makeButton(*m_resetSeparation, QStringLiteral("resetDeploymentButton"), row);
    rowLayout->addWidget(m_selectSeparationButton);
    rowLayout->addWidget(m_resetSeparationButton);
    const std::array<QPushButton*, 2> buttons{m_selectSeparationButton, m_resetSeparationButton};
    shareWidth(buttons);
    layout->addWidget(row);
}

SeparationConfigurationPanel::~SeparationConfigurationPanel() = default;

void SeparationConfigurationPanel::updateButtonState()
{
    const bool selected = selectedComponent() != nullptr;
    m_selectSeparation->setEnabled(selected);
    m_resetSeparation->setEnabled(selected);
}

bool SeparationConfigurationPanel::restructuresWith(const RocketComponent& source) const
{
    return dynamic_cast<const AxialStage*>(&source) != nullptr;
}

void SeparationConfigurationPanel::selectValue()
{
    selectSeparation();
}

void SeparationConfigurationPanel::selectSeparation()
{
    const std::vector<Uuid>                  stages = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (stages.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &stages] { return editing().selectSeparation(ids, stages); });
}

void SeparationConfigurationPanel::resetSeparation()
{
    const std::vector<Uuid>                  stages = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (stages.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &stages] { return editing().resetSeparation(ids, stages); });
}

}  // namespace QtRocket::Gui
