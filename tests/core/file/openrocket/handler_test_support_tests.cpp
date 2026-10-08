#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/simplesax/RecordingHandler.h"

// What the handler tests of the loader build on: the fixture, the scripted finder, the
// attachments by name and the runner of one handler.

namespace
{

using QtRocket::Attachment;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::Motor;
using QtRocket::ThrustCurveMotor;
using QtRocket::WarningSet;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::makeEmbeddedTestMotor;
using QtRocket::Test::MapAttachmentFactory;
using QtRocket::Test::RecordingHandler;
using QtRocket::Test::runHandler;
using QtRocket::Test::ScriptedMotorFinder;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

TEST(HandlerTestSupport, TheFixturesContextHasEveryPartALoaderSets)
{
    HandlerFixture                fixture;
    const DocumentLoadingContext& context = fixture.context();
    EXPECT_EQ(context.getFileVersion(), 111);
    EXPECT_EQ(context.getMotorFinder(), &fixture.motorFinder());
    EXPECT_EQ(context.getAttachmentFactory(), &fixture.attachments());
    EXPECT_EQ(context.getOpenRocketDocument(), &fixture.document());
    EXPECT_EQ(context.getApplicationMaterials(), &fixture.materials());
    EXPECT_EQ(context.getPreferences(), &fixture.preferences());
    // Left to the test that needs them.
    EXPECT_EQ(context.getComponentPresetDatabase(), nullptr);
    EXPECT_EQ(context.getSimulationExtensionRegistry(), nullptr);
}

TEST(HandlerTestSupport, TheFixturesDocumentIsTheOneALoaderStartsFrom)
{
    HandlerFixture fixture;
    // OpenRocketDocumentFactory::createEmptyRocket(): a rocket without a stage, no simulation.
    EXPECT_EQ(&fixture.rocket(), &fixture.document().getRocket());
    EXPECT_EQ(fixture.rocket().getChildCount(), 0U);
    // Its events are on, as while OpenRocket loads a file.
    EXPECT_TRUE(fixture.rocket().isEventsEnabled());
    EXPECT_EQ(fixture.document().getSimulationCount(), 0U);
    EXPECT_TRUE(fixture.preferences().keys().empty());
}

TEST(HandlerTestSupport, TheScriptedFinderAnswersEveryQueryAlikeAndRecordsIt)
{
    ScriptedMotorFinder finder;
    WarningSet          warnings;
    EXPECT_EQ(finder.findMotor(Motor::Type::SINGLE, "Estes", "C6", 0.018, 0.07, "abc", warnings),
              nullptr);
    EXPECT_TRUE(warnings.empty());

    const std::shared_ptr<const ThrustCurveMotor> motor = makeEmbeddedTestMotor("F12X", 12.0, "d");
    finder.setMotor(motor);
    finder.setWarning("one chosen");
    EXPECT_EQ(finder.findMotor(std::nullopt, std::nullopt, std::nullopt,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::quiet_NaN(), std::nullopt, warnings),
              motor);
    EXPECT_EQ(warningTexts(warnings), Texts{"one chosen"});
    EXPECT_EQ(finder.queries(),
              (Texts{"find(type=SINGLE, manufacturer=Estes, designation=C6, diameter=0.018, "
                     "length=0.07, digest=abc)",
                     "find(type=null, manufacturer=null, designation=null, diameter=NaN, "
                     "length=NaN, digest=null)"}));
}

TEST(HandlerTestSupport, TheAttachmentsAreTheOnesATestPut)
{
    MapAttachmentFactory factory;
    factory.put("decals/a.png", "image");
    factory.put("decals/b.png", QtRocket::stringToBytes("bytes"));
    factory.putFailure("decals/broken.png", ErrorCode::IO, "disk on fire");

    const std::shared_ptr<Attachment> a = factory.getAttachment("decals/a.png");
    EXPECT_EQ(a->getName(), "decals/a.png");
    EXPECT_EQ(QtRocket::bytesToString(a->getBytes().value()), "image");
    EXPECT_EQ(QtRocket::bytesToString(factory.getAttachment("decals/b.png")->getBytes().value()),
              "bytes");
    // One that is there and cannot be read, and one that is not there.
    const std::shared_ptr<Attachment> broken = factory.getAttachment("decals/broken.png");
    EXPECT_EQ(broken->getBytes().error().code, ErrorCode::IO);
    EXPECT_EQ(broken->getBytes().error().message, "disk on fire");
    const std::shared_ptr<Attachment> missing = factory.getAttachment("decals/c.png");
    EXPECT_EQ(missing->getName(), "decals/c.png");
    EXPECT_EQ(missing->getBytes().error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(factory.asked(),
              (Texts{"decals/a.png", "decals/b.png", "decals/broken.png", "decals/c.png"}));
}

TEST(HandlerTestSupport, RunsASnippetThroughOneHandler)
{
    // The handler is given the children of the snippet's element, and its end; the element's
    // own close goes to the stand-in for its parent and is recorded in the run.
    RecordingHandler handler;
    const HandlerRun run = runHandler(
        handler, "<motor configid='abc'><type a='1'>single</type><delay>3</delay>text</motor>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(handler.log, (Texts{"open type a=1", "end type", "close type a=1 [single]",
                                  "open delay", "end delay", "close delay [3]", "end motor"}));
    EXPECT_EQ(run.element, "motor");
    EXPECT_EQ(run.attributes, (ElementHandler::Attributes{{"configid", "abc"}}));
    EXPECT_EQ(run.content, "text");
    EXPECT_EQ(run.texts(), Texts{});
}

TEST(HandlerTestSupport, ARunKeepsTheFailureAndTheWarnings)
{
    RecordingHandler handler;
    handler.failOn          = "bad";
    const HandlerRun failed = runHandler(handler, "<x><good/><bad/></x>");
    ASSERT_FALSE(failed.result.has_value());
    EXPECT_EQ(failed.result.error().message, "fail bad");
    EXPECT_EQ(handler.log, (Texts{"open good", "end good", "close good []", "open bad"}));

    // Malformed XML is a failure of the run too, after the elements before it.
    RecordingHandler other;
    const HandlerRun malformed = runHandler(other, "<x><a/></y>");
    ASSERT_FALSE(malformed.result.has_value());
    EXPECT_EQ(malformed.result.error().code, ErrorCode::PARSE);
    EXPECT_EQ(other.log, (Texts{"open a", "end a", "close a []"}));
}

}  // namespace
