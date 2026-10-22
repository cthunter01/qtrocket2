#include "AssemblyPanelTestSupport.h"

#include <memory>

#include <QComboBox>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/Streamer.h"

namespace QtRocket::Gui::Test
{

AssemblyDesign::AssemblyDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    stage    = dynamic_cast<AxialStage*>(document->getRocket().getChildren().front());
    body     = &stage->addChild(std::make_unique<BodyTube>());
    pods     = &body->addChild(std::make_unique<PodSet>());
    boosters = &body->addChild(std::make_unique<ParallelStage>());
    document->clearUndo();
}

RecoveryDesign::RecoveryDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    stage = dynamic_cast<AxialStage*>(document->getRocket().getChildren().front());
    body  = &stage->addChild(std::make_unique<BodyTube>());
    main  = &body->addChild(std::make_unique<Parachute>());
    main->setName("Main");
    boosters     = &body->addChild(std::make_unique<ParallelStage>());
    auto& tube   = boosters->addChild(std::make_unique<BodyTube>());
    boosterChute = &tube.addChild(std::make_unique<Parachute>());
    boosterChute->setName("Booster chute");
    streamer = &body->addChild(std::make_unique<Streamer>());
    streamer->setName("Streamer");
    document->clearUndo();
}

QStringList comboItemTexts(const QComboBox& combo)
{
    QStringList texts;
    for (int i = 0; i < combo.count(); ++i)
    {
        texts.append(combo.itemText(i));
    }
    return texts;
}

}  // namespace QtRocket::Gui::Test
