#include "QtRocket/document/OpenRocketDocumentFactory.h"

#include <memory>
#include <utility>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket
{

std::unique_ptr<OpenRocketDocument> OpenRocketDocumentFactory::createNewRocket()
{
    std::unique_ptr<Rocket>     rocket = std::make_unique<Rocket>();
    std::unique_ptr<AxialStage> stage  = std::make_unique<AxialStage>();
    //// Sustainer
    stage->setName(kSustainerName);
    rocket->addChild(std::move(stage));
    rocket->getSelectedConfiguration().setAllStages();
    std::unique_ptr<OpenRocketDocument> doc =
        std::make_unique<OpenRocketDocument>(std::move(rocket));
    doc->setSaved(true);
    return doc;
}

std::unique_ptr<OpenRocketDocument> OpenRocketDocumentFactory::createDocumentFromRocket(
    std::unique_ptr<Rocket> rocket)
{
    return std::make_unique<OpenRocketDocument>(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> OpenRocketDocumentFactory::createEmptyRocket()
{
    return std::make_unique<OpenRocketDocument>(std::make_unique<Rocket>());
}

}  // namespace QtRocket
