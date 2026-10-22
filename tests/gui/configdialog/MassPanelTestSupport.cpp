#include "MassPanelTestSupport.h"

#include <memory>

#include <QLabel>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"

namespace QtRocket::Gui::Test
{

MassDesign::MassDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    stage     = dynamic_cast<AxialStage*>(document->getRocket().getChildren().front());
    body      = &stage->addChild(std::make_unique<BodyTube>());
    mass      = &body->addChild(std::make_unique<MassComponent>());
    cord      = &body->addChild(std::make_unique<ShockCord>());
    parachute = &body->addChild(std::make_unique<Parachute>());
    streamer  = &body->addChild(std::make_unique<Streamer>());
    document->clearUndo();
}

BodyTube& addBoosterStage(OpenRocketDocument& document)
{
    auto& booster = document.getRocket().addChild(std::make_unique<AxialStage>());
    auto& tube    = booster.addChild(std::make_unique<BodyTube>());
    document.clearUndo();
    return tube;
}

QStringList labelTextsUnder(const QWidget& widget)
{
    QStringList texts;
    for (const QLabel* label : widget.findChildren<QLabel*>())
    {
        texts.append(label->text());
    }
    return texts;
}

}  // namespace QtRocket::Gui::Test
