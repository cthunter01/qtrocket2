#include "QtRocket/document/OpenRocketDocumentFactory.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "document/DocumentTestSupport.h"
#include "rocket/TestRockets.h"

// OpenRocket has no test of OpenRocketDocumentFactory. The expectations are what OpenRocket's
// three functions give: DocumentProbe.out, section 7, of the tier 9 scout, and DocumentProbe4.txt,
// section A, of this part (probes/tier9a-document-d3/logs), whose lines the comments quote.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::FlightConfiguration;
using QtRocket::NoseCone;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Rocket;
using QtRocket::StorageOptions;
using QtRocket::Test::DocumentRecorder;
using QtRocket::Test::stateWithConfigs;
using QtRocket::Test::TestEstesAlphaIII;

/// The state of a document nothing has happened to, with @p configs flight configurations.
[[nodiscard]] std::string untouched(int configs)
{
    return std::format(
        "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false "
        "undoDesc=null redoAvail=false redoDesc=null saved=true sims=0 configs={}",
        configs);
}

// "new: stages=1 stageName='Sustainer' rocketName='Rocket' eventsEnabled=true rocketDocument=true
// selectedIsDefault=true configs=0"
// "new: isStageActive(0)=false activeStageCount=0 stageCount=1"
TEST(OpenRocketDocumentFactory, ANewRocketHasOneStageNamedSustainer)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createNewRocket();
    Rocket&                                   r = d->getRocket();

    ASSERT_EQ(r.getChildCount(), 1U);
    EXPECT_EQ(r.getChild(0).getName(), "Sustainer");
    EXPECT_EQ(r.getChild(0).getName(), OpenRocketDocumentFactory::kSustainerName);
    EXPECT_NE(dynamic_cast<AxialStage*>(&r.getChild(0)), nullptr);
    EXPECT_EQ(r.getName(), "Rocket");
    EXPECT_TRUE(r.isEventsEnabled());
    EXPECT_EQ(r.getDocument(), d.get());
    EXPECT_EQ(r.getFlightConfigurationCount(), 0);
    EXPECT_EQ(r.getStageCount(), 1U);

    // The selected configuration is the default one. Its stage is set active, and counts as
    // active once it has a child (a stage without children is never active).
    const FlightConfiguration& config = d->getSelectedConfiguration();
    EXPECT_TRUE(config.getId().isDefaultId());
    EXPECT_FALSE(config.isStageActive(0));
    EXPECT_EQ(config.getActiveStageCount(), 0);
}

// "new: file=null saved=true sims=0 decals=0 expressions=0 photo=0 prefs=0 materials=0 types=71"
// "new: storage fileType=OPENROCKET saveData=false explicit=false preview=null"
// "new: pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null
// redoAvail=false redoDesc=null saved=true sims=0 configs=0", "new: nextName=Simulation 1"
TEST(OpenRocketDocumentFactory, ANewRocketIsSavedAndEmpty)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createNewRocket();

    EXPECT_EQ(d->getFile(), std::nullopt);
    EXPECT_TRUE(d->isSaved());
    EXPECT_EQ(d->getSimulationCount(), 0U);
    EXPECT_TRUE(d->getDecalList().empty());
    EXPECT_TRUE(d->getCustomExpressions().empty());
    EXPECT_TRUE(d->getPhotoSettings().empty());
    EXPECT_EQ(d->getDocumentPreferences().size(), 0U);
    EXPECT_EQ(d->getDocumentMaterials().totalMaterialCount(), 0U);
    EXPECT_EQ(d->getFlightDataTypes().size(), 71U);

    const StorageOptions& options = d->getDefaultStorageOptions();
    EXPECT_EQ(options.getFileType(), StorageOptions::FileType::OPENROCKET);
    EXPECT_FALSE(options.getSaveSimulationData());
    EXPECT_FALSE(options.isExplicitlySet());
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);

    EXPECT_EQ(stateWithConfigs(*d), untouched(0));
    EXPECT_EQ(d->getNextSimulationName(), "Simulation 1");
}

// "stage.addChild(BodyTube) | events: U D(AxialStage,saved) D(AxialStage)" and the undo after it.
TEST(OpenRocketDocumentFactory, ANewRocketIsEditedAndUndone)
{
    const std::unique_ptr<OpenRocketDocument> d = OpenRocketDocumentFactory::createNewRocket();
    DocumentRecorder                          events(*d, true);

    d->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(events.take(), "U D(AxialStage,saved) D(AxialStage)");
    EXPECT_EQ(stateWithConfigs(*d),
              "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=true undoDesc=null "
              "redoAvail=false redoDesc=null saved=false sims=0 configs=0");
    // "isStageActive(0)=true activeStageCount=1": setAllStages() of the factory.
    EXPECT_TRUE(d->getSelectedConfiguration().isStageActive(0));
    EXPECT_EQ(d->getSelectedConfiguration().getActiveStageCount(), 1);

    d->undo();
    EXPECT_EQ(events.take(), "U D(Rocket) D(Rocket) S(OpenRocketDocument) U");
    EXPECT_EQ(stateWithConfigs(*d),
              "pos=0 hist=2 desc=[null, null] next=null stored=null undoAvail=false undoDesc=null "
              "redoAvail=true redoDesc=null saved=false sims=0 configs=0");
    // "children of the stage=0"
    EXPECT_EQ(d->getRocket().getChild(0).getChildCount(), 0U);
}

// "empty: stages=0 rocketName='Rocket' eventsEnabled=true rocketDocument=true file=null"
TEST(OpenRocketDocumentFactory, AnEmptyRocketHasNoStage)
{
    const std::unique_ptr<OpenRocketDocument> e = OpenRocketDocumentFactory::createEmptyRocket();

    EXPECT_EQ(e->getRocket().getChildCount(), 0U);
    EXPECT_EQ(e->getRocket().getName(), "Rocket");
    EXPECT_TRUE(e->getRocket().isEventsEnabled());
    EXPECT_EQ(e->getRocket().getDocument(), e.get());
    EXPECT_EQ(e->getFile(), std::nullopt);
    // Saved, though createEmptyRocket() does not call setSaved(true): nothing has changed.
    EXPECT_EQ(stateWithConfigs(*e), untouched(0));
}

// "from rocket: same rocket=true eventsBefore=true eventsEnabled=true rocketDocument=true"
TEST(OpenRocketDocumentFactory, ADocumentFromARocketKeepsThatRocket)
{
    TestEstesAlphaIII   alpha;
    const Rocket* const rocket = alpha.rocket.get();
    ASSERT_TRUE(rocket->isEventsEnabled());

    const std::unique_ptr<OpenRocketDocument> f =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));

    EXPECT_EQ(&f->getRocket(), rocket);
    EXPECT_TRUE(rocket->isEventsEnabled());
    EXPECT_EQ(rocket->getDocument(), f.get());
    EXPECT_EQ(stateWithConfigs(*f), untouched(5));
}

// "plain rocket: eventsEnabled before=false", "after=true": the document's constructor enables
// the events of a rocket that was built with them disabled, and the undo history starts with
// the rocket as the enabling left it (so the document is clean and saved).
TEST(OpenRocketDocumentFactory, ADocumentEnablesTheEventsOfItsRocket)
{
    std::unique_ptr<Rocket> plain = std::make_unique<Rocket>();
    AxialStage&             stage = plain->addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<NoseCone>());
    ASSERT_FALSE(plain->isEventsEnabled());

    const std::unique_ptr<OpenRocketDocument> g =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(plain));

    EXPECT_TRUE(g->getRocket().isEventsEnabled());
    EXPECT_EQ(stateWithConfigs(*g), untouched(0));
}

// Java: a NullPointerException from the document's constructor.
TEST(OpenRocketDocumentFactory, ANullRocketIsABug)
{
    EXPECT_THROW(static_cast<void>(OpenRocketDocumentFactory::createDocumentFromRocket(nullptr)),
                 BugError);
}

}  // namespace
