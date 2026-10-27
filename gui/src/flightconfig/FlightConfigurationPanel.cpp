#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QScopedValueRollback>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motors & Configuration");
}

/// The number of acting motor mounts and of recovery devices of @p rocket (Java: ListMotorMounts
/// and ListComponents<>(RecoveryDevice.class)).
struct ComponentCounts
{
    std::size_t motorMounts     = 0;
    std::size_t recoveryDevices = 0;
};

[[nodiscard]] ComponentCounts countComponents(const Rocket& rocket)
{
    ComponentCounts counts;
    rocket.forEach([&counts](const RocketComponent& component) {
        if (const auto* mount = dynamic_cast<const MotorMount*>(&component);
            mount != nullptr && mount->isMotorMount())
        {
            ++counts.motorMounts;
        }
        if (dynamic_cast<const RecoveryDevice*>(&component) != nullptr)
        {
            ++counts.recoveryDevices;
        }
    });
    return counts;
}

}  // namespace

FlightConfigurationPanel::FlightConfigurationPanel(OpenRocketDocument& document,
                                                   const GuiServices& services, QWidget* parent)
  : QWidget(parent),
    m_document(&document),
    m_services(services),
    m_editing(makeChild<FlightConfigurationEditing>(this, document, services,
                                                    static_cast<QWidget*>(this))),
    //: edtmotorconfdlg.but.Newconfiguration
    m_newAction(
        makeChild<QAction>(this, Icons::icon(Icons::Id::FILE_NEW), tr("New Configuration"))),
    //: edtmotorconfdlg.but.Renameconfiguration
    m_renameAction(
        makeChild<QAction>(this, Icons::icon(Icons::Id::EDIT_RENAME), tr("Rename Configuration"))),
    //: edtmotorconfdlg.but.Deleteconfiguration
    m_deleteAction(
        makeChild<QAction>(this, Icons::icon(Icons::Id::EDIT_DELETE), tr("Delete Configuration"))),
    //: edtmotorconfdlg.but.Duplicateconfiguration
    m_duplicateAction(makeChild<QAction>(this, Icons::icon(Icons::Id::EDIT_DUPLICATE),
                                         tr("Duplicate Configuration"))),
    m_configPopup(makeChild<QMenu>(this)),
    // The sub-tabs: their tables listen to the rocket before the panel does.
    m_tabs(makeChild<QTabWidget>(this)),
    m_motorPanel(makeChild<MotorConfigurationPanel>(m_tabs, *this)),
    m_recoveryPanel(makeChild<RecoveryConfigurationPanel>(m_tabs, *this)),
    m_separationPanel(makeChild<SeparationConfigurationPanel>(m_tabs, *this)),
    m_previousPanel(m_motorPanel)
{
    setObjectName(QStringLiteral("flightConfigurationPanel"));
    configureActions();

    // The popup menu of column 0.
    m_configPopup->setObjectName(QStringLiteral("popupMenuConfig"));
    m_configPopup->addAction(m_newAction);
    m_configPopup->addAction(m_renameAction);
    m_configPopup->addAction(m_deleteAction);
    m_configPopup->addAction(m_duplicateAction);

    m_tabs->setObjectName(QStringLiteral("tabs"));
    //: edtmotorconfdlg.lbl.Motortab
    m_tabs->addTab(m_motorPanel, tr("Motors"));
    //: edtmotorconfdlg.lbl.Recoverytab
    m_tabs->addTab(m_recoveryPanel, tr("Recovery"));
    //: edtmotorconfdlg.lbl.Stagetab
    m_tabs->addTab(m_separationPanel, tr("Stages"));

    // MigLayout("fill", "[grow][][][][][grow]"): the buttons centred in a row ("skip 1, gapright
    // para"), the tabs under them ("spanx, grow, pushy").
    auto* layout = makeChild<QVBoxLayout>(this);
    auto* row    = makeChild<QWidget>(this);
    row->setObjectName(QStringLiteral("configurationButtons"));
    makeButtons(*row);
    layout->addWidget(row);
    layout->addWidget(m_tabs, 1);

    connect(m_tabs, &QTabWidget::currentChanged, this,
            guarded([this](int index) { tabChanged(index); }, context()));
    updateButtonState();
    m_changes = rocket().addComponentChangeListener(
        guarded([this](const ComponentChangeEvent& event) { rocketChanged(event); }, context()));
}

FlightConfigurationPanel::~FlightConfigurationPanel() = default;

Rocket& FlightConfigurationPanel::rocket() const noexcept
{
    return m_document->getRocket();
}

void FlightConfigurationPanel::configureActions()
{
    m_newAction->setObjectName(QStringLiteral("newConfigAction"));
    // Java's action has a large icon only, which a menu item does not show.
    m_newAction->setIconVisibleInMenu(false);
    m_renameAction->setObjectName(QStringLiteral("renameConfigAction"));
    m_deleteAction->setObjectName(QStringLiteral("deleteConfigAction"));
    m_duplicateAction->setObjectName(QStringLiteral("duplicateConfigAction"));

    connect(m_newAction, &QAction::triggered, this,
            guarded([this] { newConfiguration(); }, m_newAction->text()));
    connect(m_renameAction, &QAction::triggered, this,
            guarded([this] { renameConfigurations(); }, m_renameAction->text()));
    connect(m_deleteAction, &QAction::triggered, this,
            guarded([this] { removeConfigurations(); }, m_deleteAction->text()));
    connect(m_duplicateAction, &QAction::triggered, this,
            guarded([this] { duplicateConfigurations(); }, m_duplicateAction->text()));
}

void FlightConfigurationPanel::makeButtons(QWidget& row)
{
    auto* layout = makeChild<QHBoxLayout>(&row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch(1);
    const auto add = [&row, layout](QAction* action, const QString& name, bool last) {
        auto* button = makeChild<IconButton>(&row);
        button->setObjectName(name);
        button->setAction(action);
        layout->addWidget(button);
        if (!last)
        {
            layout->addSpacing(FlightConfigurablePanel::kParagraphGap);
        }
        return button;
    };
    m_newButton       = add(m_newAction, QStringLiteral("newConfButton"), false);
    m_renameButton    = add(m_renameAction, QStringLiteral("renameConfButton"), false);
    m_deleteButton    = add(m_deleteAction, QStringLiteral("removeConfButton"), false);
    m_duplicateButton = add(m_duplicateAction, QStringLiteral("duplicateConfButton"), true);
    layout->addStretch(1);
}

FlightConfigurablePanel& FlightConfigurationPanel::currentPanel() const
{
    FlightConfigurablePanel* const panel = panelAt(m_tabs->currentIndex());
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

FlightConfigurablePanel* FlightConfigurationPanel::panelAt(int index) const
{
    switch (index)
    {
        case kMotorTabIndex:
            return m_motorPanel;
        case kRecoveryTabIndex:
            return m_recoveryPanel;
        case kSeparationTabIndex:
            return m_separationPanel;
        default:
            return nullptr;
    }
}

// ----------------------------------------------------------------------- the window's parts

void FlightConfigurationPanel::setDesignSelection(DocumentSelectionModel* selection)
{
    m_designSelection = selection;
}

DocumentSelectionModel* FlightConfigurationPanel::designSelection() const noexcept
{
    return m_designSelection.data();
}

void FlightConfigurationPanel::setSelectedComponents(std::span<const Uuid> components)
{
    if (m_designSelection.isNull())
    {
        return;
    }
    std::vector<RocketComponent*> found;
    for (const Uuid& id : components)
    {
        found.push_back(rocket().findComponent(id));
    }
    std::erase(found, nullptr);
    if (!found.empty())
    {
        m_designSelection->setSelectedComponents(found);
    }
}

void FlightConfigurationPanel::takeTheSpotlight() const
{
    FlightConfigurablePanel& panel = currentPanel();
    panel.takeTheSpotlight();
    panel.updateRocketViewSelection();
}

void FlightConfigurationPanel::showConfigurationPopup(const QPoint& globalPosition)
{
    m_configPopup->popup(globalPosition);
}

// ------------------------------------------------------------------------ the operations

void FlightConfigurationPanel::newConfiguration()
{
    const QPointer<FlightConfigurationPanel>   alive(this);
    const std::optional<FlightConfigurationId> id =
        m_editing->newConfiguration([this](const FlightConfigurationId& /*id*/) {
            // Java: configurationChanged(MOTOR_CHANGE, ids), stateChanged(null), then the current
            // sub-tab's Select operation over the new row.
            restoreAll();
            updateButtonState();
            synchronizeAll();
            currentPanel().selectValue();
        });
    if (alive.isNull())
    {
        return;  // the tab went during the follow-up's question
    }
    if (id.has_value())
    {
        // configurationChanged(NONFUNCTIONAL_CHANGE): "trigger select".
        restoreAll();
    }
}

void FlightConfigurationPanel::duplicateConfigurations()
{
    const std::vector<FlightConfigurationId> ids = currentPanel().selectedConfigurationIds();
    if (ids.empty())
    {
        return;
    }
    if (m_editing->duplicateConfigurations(ids).empty())
    {
        return;
    }
    // Java: configurationChanged(MOTOR_CHANGE, ids), stateChanged(null),
    // configurationChanged(NONFUNCTIONAL_CHANGE).
    restoreAll();
    updateButtonState();
    synchronizeAll();
    restoreAll();
}

void FlightConfigurationPanel::renameConfigurations()
{
    const std::vector<FlightConfigurationId> ids = currentPanel().selectedConfigurationIds();
    if (ids.empty())
    {
        return;
    }
    const QPointer<FlightConfigurationPanel> alive(this);
    if (m_editing->renameConfigurations(ids) && !alive.isNull())
    {
        restoreAll();
    }
}

void FlightConfigurationPanel::removeConfigurations()
{
    const std::vector<FlightConfigurationId> ids = currentPanel().selectedConfigurationIds();
    if (ids.empty())
    {
        return;
    }
    m_editing->removeConfigurations(ids);
    restoreAll();
    takeTheSpotlight();
}

// --------------------------------------------------------------------- the rocket and tabs

void FlightConfigurationPanel::tabChanged(int index)
{
    FlightConfigurablePanel* const panel = panelAt(index);
    if (panel == nullptr)
    {
        return;
    }
    // Java's synchronizePanelSelection(previousPanel, panel): the rows the previous sub-tab had
    // selected.
    if (m_previousPanel != nullptr && m_previousPanel != panel)
    {
        const std::vector<FlightConfigurationId> ids = m_previousPanel->selectedConfigurationIds();
        if (ids.empty())
        {
            panel->clearSelection();
        }
        else
        {
            panel->setSelectedConfigurationIds(ids);
        }
    }
    panel->updateButtonState();
    panel->takeTheSpotlight();
    panel->updateRocketViewSelection();
    m_previousPanel = panel;
}

void FlightConfigurationPanel::rocketChanged(const ComponentChangeEvent& event)
{
    const QScopedValueRollback<int> handling(m_rocketChanges, m_rocketChanges + 1);
    // Java's table models answer a MOTOR or TREE change (and the motor and stage tables a
    // NONFUNCTIONAL change of a component whose name is a header) with
    // fireTableStructureChanged(), which clears the table (they listen to the rocket before the
    // panel).
    for (FlightConfigurablePanel* panel : panels())
    {
        if (panel->isStructureChange(event))
        {
            panel->restructure();
        }
    }
    // Java's stateChanged().
    updateButtonState();
    synchronizeAll();
}

void FlightConfigurationPanel::updateButtonState()
{
    const FlightConfigurationId current = rocket().getSelectedConfiguration().getId();
    // Rename, Delete and Duplicate only while a configuration is selected.
    const bool selected = current.isValid() && !current.isDefaultId();
    m_deleteAction->setEnabled(selected);
    m_renameAction->setEnabled(selected);
    m_duplicateAction->setEnabled(selected);

    const ComponentCounts counts      = countComponents(rocket());
    const std::size_t     stageCount  = rocket().getStageCount();
    const bool            hasMounts   = counts.motorMounts > 0;
    const bool            hasRecovery = hasMounts && counts.recoveryDevices > 0;
    const bool            hasStages   = hasMounts && stageCount > 1;
    // New only while there is a motor mount.
    m_newAction->setEnabled(hasMounts);

    // A current sub-tab that is to be disabled moves to the Motors tab first (Java moves there
    // when the recovery devices or the stages but one go; U-M3-6 f for the rest).
    const int tab = m_tabs->currentIndex();
    if ((tab == kRecoveryTabIndex && !hasRecovery) || (tab == kSeparationTabIndex && !hasStages))
    {
        m_tabs->setCurrentIndex(kMotorTabIndex);
    }
    m_tabs->setTabEnabled(kRecoveryTabIndex, hasRecovery);
    m_tabs->setTabEnabled(kSeparationTabIndex, hasStages);
}

std::array<FlightConfigurablePanel*, 3> FlightConfigurationPanel::panels() const noexcept
{
    return {m_motorPanel, m_recoveryPanel, m_separationPanel};
}

void FlightConfigurationPanel::synchronizeAll()
{
    for (FlightConfigurablePanel* panel : panels())
    {
        panel->synchronizeConfigurationSelection();
    }
}

void FlightConfigurationPanel::restoreAll()
{
    for (FlightConfigurablePanel* panel : panels())
    {
        panel->restoreSelection();
    }
}

}  // namespace QtRocket::Gui
