#include "ConfigDialogTestSupport.h"

#include <cstddef>
#include <memory>
#include <span>

#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtContainerFwd>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// Adds a new component of @p kind to @p parent and records it in @p byKind when it is the
/// first of its kind.
RocketComponent& addComponent(std::span<RocketComponent*, kAllComponentKinds.size()> byKind,
                              RocketComponent& parent, ComponentKind kind)
{
    RocketComponent&  added = parent.addChild(ComponentFactory::createWithBuiltinMaterials(kind));
    RocketComponent*& slot  = byKind[static_cast<std::size_t>(kind)];
    if (slot == nullptr)
    {
        slot = &added;
    }
    return added;
}

}  // namespace

EveryKindDesign::EveryKindDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    Rocket& rocket                                               = document->getRocket();
    byKind[static_cast<std::size_t>(ComponentKind::ROCKET)]      = &rocket;
    RocketComponent& sustainer                                   = *rocket.getChildren().front();
    byKind[static_cast<std::size_t>(ComponentKind::AXIAL_STAGE)] = &sustainer;

    addComponent(byKind, sustainer, ComponentKind::NOSE_CONE);
    RocketComponent& body = addComponent(byKind, sustainer, ComponentKind::BODY_TUBE);
    body.setName("Body");
    addComponent(byKind, sustainer, ComponentKind::TRANSITION);
    for (const ComponentKind kind :
         {ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::ELLIPTICAL_FIN_SET,
          ComponentKind::FREEFORM_FIN_SET, ComponentKind::TUBE_FIN_SET, ComponentKind::LAUNCH_LUG,
          ComponentKind::RAIL_BUTTON, ComponentKind::INNER_TUBE, ComponentKind::TUBE_COUPLER,
          ComponentKind::CENTERING_RING, ComponentKind::BULKHEAD, ComponentKind::MASS_COMPONENT,
          ComponentKind::SHOCK_CORD, ComponentKind::PARACHUTE, ComponentKind::STREAMER,
          ComponentKind::POD_SET, ComponentKind::PARALLEL_STAGE})
    {
        addComponent(byKind, body, kind);
    }
    addComponent(byKind, component(ComponentKind::INNER_TUBE), ComponentKind::ENGINE_BLOCK);
    addComponent(byKind, component(ComponentKind::POD_SET), ComponentKind::BODY_TUBE);
    addComponent(byKind, component(ComponentKind::PARALLEL_STAGE), ComponentKind::BODY_TUBE);

    auto& second =
        dynamic_cast<AxialStage&>(addComponent(byKind, rocket, ComponentKind::AXIAL_STAGE));
    second.setName("Booster");
    booster    = &second;
    auto& tube = dynamic_cast<BodyTube&>(addComponent(byKind, second, ComponentKind::BODY_TUBE));
    tube.setName("Booster tube");
    boosterTube = &tube;
    document->clearUndo();
}

RocketComponent& EveryKindDesign::component(ComponentKind kind) const
{
    RocketComponent* found = byKind.at(static_cast<std::size_t>(kind));
    if (found == nullptr)
    {
        bug("The design has no component of that kind");
    }
    return *found;
}

ConfigDialogServices::ConfigDialogServices()
{
    addBuiltinMaterials(services().materials());
}

void ConfigDialogTest::SetUp()
{
    GuiTest::SetUp();
    ComponentConfigDialog::forgetPreviousDialogs();
}

void ConfigDialogTest::TearDown()
{
    ComponentConfigDialog::forgetPreviousDialogs();
    GuiTest::TearDown();
}

QStringList tabTitles(const RocketComponentConfig& panel)
{
    QStringList       titles;
    const QTabWidget& tabs = panel.tabs();
    for (int i = 0; i < tabs.count(); ++i)
    {
        titles.append(tabs.tabText(i));
    }
    return titles;
}

PanelHost::PanelHost(RocketComponent& main, std::span<RocketComponent* const> others)
  : m_layout(makeChild<QVBoxLayout>(&m_root)),
    m_scope(m_root, std::make_shared<ComponentSelection>(main, others))
{
}

PanelHost::~PanelHost()
{
    m_scope.release();
}

}  // namespace QtRocket::Gui::Test
