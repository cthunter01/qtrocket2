#include "RingPanelTestSupport.h"

#include <memory>
#include <span>

#include <QLabel>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <QtContainerFwd>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"

namespace QtRocket::Gui::Test
{

RingDesign::RingDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    RocketComponent& stage = *document->getRocket().getChildren().front();
    body                   = &stage.addChild(std::make_unique<BodyTube>());
    inner                  = &body->addChild(std::make_unique<InnerTube>());
    coupler                = &body->addChild(std::make_unique<TubeCoupler>());
    block                  = &inner->addChild(std::make_unique<EngineBlock>());
    centering              = &body->addChild(std::make_unique<CenteringRing>());
    bulkhead               = &body->addChild(std::make_unique<Bulkhead>());
    document->clearUndo();
}

FactoryPanel::FactoryPanel(OpenRocketDocument& document, const GuiServices& services,
                           RocketComponent& main, std::span<RocketComponent* const> others)
  : m_panel(ConfigPanelFactory::create(
        document, services, std::make_shared<ComponentSelection>(main, others), m_host.get()))
{
}

QStringList rowLabels(const RingComponentConfig::GeneralTab& tab)
{
    QStringList labels;
    for (const ConfigFields::NumberWidgets* row :
         {&tab.length, &tab.outerRadius, &tab.innerRadius, &tab.thickness})
    {
        labels.append(row->label != nullptr ? row->label->text() : QString());
    }
    return labels;
}

ComponentConfigDialog* showActiveDialog(QWidget& window, const GuiServices& services,
                                        OpenRocketDocument& document, RocketComponent& main,
                                        std::span<RocketComponent* const> others)
{
    ComponentConfigDialog::showDialog(window, services, document, main, others);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    if (dialog == nullptr)
    {
        ADD_FAILURE() << "no dialog";
        return nullptr;
    }
    dialog->activateWindow();
    if (!QTest::qWaitForWindowActive(dialog))
    {
        ADD_FAILURE() << "the dialog did not become active";
    }
    return dialog;
}

}  // namespace QtRocket::Gui::Test
