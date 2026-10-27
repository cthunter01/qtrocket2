#include "flightconfig/FlightConfigurationTabSupport.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "motor/TestMotorDatabase.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// "[0, 1]" of @p values.
[[nodiscard]] std::string listOf(const std::vector<int>& values)
{
    std::string text = "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        text += (i > 0 ? ", " : "") + std::to_string(values[i]);
    }
    return text + "]";
}

/// Gives @p mount a motor configuration for @p id with Estes' @p motor and @p delay.
void setMotor(MotorMount& mount, const FlightConfigurationId& id,
              std::shared_ptr<const Motor> motor, double delay)
{
    MotorConfiguration configuration{mount, id};
    configuration.setMotor(std::move(motor));
    configuration.setEjectionDelay(delay);
    mount.setMotorConfig(std::move(configuration), id);
}

}  // namespace

FlightConfigurationTab::FlightConfigurationTab(Design design)
  : m_motorDatabase(QtRocket::Test::smallMotorDatabase()),
    m_document(OpenRocketDocumentFactory::createNewRocket())
{
    if (design == Design::TWO_STAGES)
    {
        buildTwoStages();
    }
    m_document->clearUndo();
    m_treeModel = std::make_unique<ComponentTreeModel>(*m_document, m_services.preferences());
    m_window    = std::make_unique<QWidget>();
    m_window->setObjectName(QStringLiteral("flightConfigurationTabWindow"));
    auto* layout = makeChild<QHBoxLayout>(m_window.get());
    m_tree       = makeChild<ComponentTreeView>(m_window.get(), *m_treeModel);
    m_selection  = makeChild<DocumentSelectionModel>(m_tree);
    m_panel =
        makeChild<FlightConfigurationPanel>(m_window.get(), *m_document, m_services.services());
    m_panel->setDesignSelection(m_selection);
    layout->addWidget(m_tree);
    layout->addWidget(m_panel, 1);
}

FlightConfigurationTab::~FlightConfigurationTab() = default;

void FlightConfigurationTab::buildTwoStages() const
{
    Rocket& r         = rocket();
    auto&   sustainer = dynamic_cast<AxialStage&>(r.getChild(0));
    sustainer.setName("Sustainer");
    auto& nose = sustainer.addChild(std::make_unique<NoseCone>());
    nose.setName("Nose");
    auto& body = sustainer.addChild(std::make_unique<BodyTube>());
    body.setName("Body");
    body.setMotorMount(true);
    auto& inner = body.addChild(std::make_unique<InnerTube>());
    inner.setName("Motor tube");
    inner.setMotorMount(true);
    auto& chute = body.addChild(std::make_unique<Parachute>());
    chute.setName("Chute");
    auto& booster = r.addChild(std::make_unique<AxialStage>());
    booster.setName("Booster");
    auto& boosterTube = booster.addChild(std::make_unique<BodyTube>());
    boosterTube.setName("Booster tube");
    boosterTube.setMotorMount(true);
    std::vector<FlightConfigurationId> ids;
    ids.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        ids.push_back(r.createFlightConfiguration().getId());
    }
    setMotor(body, ids[0], estes("C6"), 5);
    setMotor(inner, ids[0], estes("A8"), 3);
    setMotor(boosterTube, ids[0], estes("C6"), 0);
    setMotor(body, ids[1], estes("C6"), 5);
    r.setSelectedConfiguration(ids[0]);
}

MotorConfigurationPanel& FlightConfigurationTab::motors() const
{
    return m_panel->motorConfigurationPanel();
}

RecoveryConfigurationPanel& FlightConfigurationTab::recovery() const
{
    return m_panel->recoveryConfigurationPanel();
}

SeparationConfigurationPanel& FlightConfigurationTab::stages() const
{
    return m_panel->separationConfigurationPanel();
}

Rocket& FlightConfigurationTab::rocket() const
{
    return m_document->getRocket();
}

void FlightConfigurationTab::show() const
{
    m_window->resize(1200, 600);
    m_window->show();
    QCoreApplication::processEvents();
}

std::shared_ptr<const Motor> FlightConfigurationTab::estes(std::string_view designation) const
{
    for (const ThrustCurveMotorSet& set : m_motorDatabase.getMotorSets())
    {
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
        {
            if (motor->getManufacturer().getSimpleName() == "Estes" &&
                motor->getDesignation() == designation)
            {
                return motor;
            }
        }
    }
    ADD_FAILURE() << "no Estes " << designation;
    return nullptr;
}

RocketComponent& FlightConfigurationTab::find(std::string_view name) const
{
    RocketComponent* found = nullptr;
    rocket().forEach([&found, name](RocketComponent& component) {
        if (found == nullptr && component.getName() == name)
        {
            found = &component;
        }
    });
    QTROCKET_ASSERT(found != nullptr);
    return *found;
}

FlightConfigurationId FlightConfigurationTab::id(int row) const
{
    return rocket().getFlightConfigurationId(row);
}

int FlightConfigurationTab::selectedRow() const
{
    const FlightConfigurationId              selected = rocket().getSelectedConfiguration().getId();
    const std::vector<FlightConfigurationId> ids      = rocket().getIds();
    for (std::size_t row = 0; row < ids.size(); ++row)
    {
        if (ids[row] == selected)
        {
            return static_cast<int>(row);
        }
    }
    return -1;
}

std::string FlightConfigurationTab::designSelectionNames() const
{
    std::string text  = "[";
    bool        first = true;
    for (const RocketComponent* component : m_selection->selectedComponents())
    {
        text += (first ? "" : ", ") + component->getName();
        first = false;
    }
    return text + "]";
}

std::string FlightConfigurationTab::lines(const FlightConfigurablePanel& panel)
{
    const RowColumnSelectionModel::Lines& lines = panel.tableSelection().lines();
    return "rows=" + listOf(lines.rows) + " cols=" + listOf(lines.columns);
}

QPoint FlightConfigurationTab::cellCenter(const FlightConfigurablePanel& panel, int row, int column)
{
    const QTableView& table = panel.table();
    return table.visualRect(table.model()->index(row, column)).center();
}

void FlightConfigurationTab::click(const FlightConfigurablePanel& panel, int row, int column,
                                   Qt::KeyboardModifiers modifiers)
{
    QTest::mouseClick(panel.table().viewport(), Qt::LeftButton, modifiers,
                      cellCenter(panel, row, column));
}

void FlightConfigurationTab::doubleClick(const FlightConfigurablePanel& panel, int row, int column)
{
    doubleClickAt(*panel.table().viewport(), cellCenter(panel, row, column));
}

QMenu* FlightConfigurationTab::rightClickAt(const FlightConfigurablePanel& panel,
                                            const QPoint&                  position)
{
    QWidget&   viewport = *panel.table().viewport();
    const bool onACell  = panel.table().indexAt(position).isValid();
    QMenu*     menu     = nullptr;
    if (onACell)
    {
        // The offscreen platform cannot raise the menu or grab the keyboard for it.
        const ExpectedQtMessage noRaise(QStringLiteral("does not support raise"));
        const ExpectedQtMessage noGrab(QStringLiteral("grabbing the keyboard"));
        Gui::Test::rightClickAt(viewport, position);
        menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu != nullptr)
        {
            menu->hide();
        }
    }
    else
    {
        Gui::Test::rightClickAt(viewport, position);
        menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    }
    return menu;
}

QMenu* FlightConfigurationTab::rightClick(const FlightConfigurablePanel& panel, int row, int column)
{
    return rightClickAt(panel, cellCenter(panel, row, column));
}

std::string FlightConfigurationTab::items(const QMenu& menu)
{
    std::string text;
    for (const QAction* action : menu.actions())
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += action->isSeparator() ? std::string("-") : QtStrings::toUtf8(action->objectName());
        if (!action->isSeparator() && !action->isEnabled())
        {
            text += " (disabled)";
        }
    }
    return text;
}

}  // namespace QtRocket::Gui::Test
