// Tests of OpenRocketLoader, which reads a design document and runs the steps that follow
// (OpenRocket's file/openrocket/importt/OpenRocketLoader with file/AbstractRocketLoader).
// OpenRocket has no test of the class. What the steps leave is pinned with OpenRocket in
// GeneralRocketLoaderTests.cpp (the cases p-* and m-* of the probe's table); here each step is
// made visible by itself: the same document read by the root handler alone has not had it.

#include "QtRocket/file/openrocket/OpenRocketLoader.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/OpenRocketHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketHandler;
using QtRocket::OpenRocketLoader;
using QtRocket::Result;
using QtRocket::ScriptingExtension;
using QtRocket::Simulation;
using QtRocket::SimulationExtensionRegistry;
using QtRocket::StorageOptions;
using QtRocket::stringToBytes;
using QtRocket::WarningSet;
using QtRocket::Test::documentOfLoadCase;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::warningTexts;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

/// A document read into the document of a HandlerFixture whose extension registry is the
/// bundled one and whose motor finder finds every motor: by the loader, or by the root handler
/// alone, which is the loader without the steps that follow the reading.
struct Reading
{
    SimulationExtensionRegistry registry = SimulationExtensionRegistry::bundled();
    HandlerFixture              fixture;
    WarningSet                  warnings;
    Result<void>                result;

    Reading(std::string_view document, bool withTheSteps)
    {
        fixture.context().setSimulationExtensionRegistry(&registry);
        fixture.context().setFileVersion(0);
        fixture.motorFinder().setMotor(QtRocket::Test::makeEmbeddedTestMotor("A8", 10.0, "d"));
        const std::vector<std::byte> bytes = stringToBytes(document);
        if (withTheSteps)
        {
            result = OpenRocketLoader::load(fixture.context(), bytes, warnings);
        }
        else
        {
            OpenRocketHandler handler(fixture.context());
            result =
                QtRocket::SimpleSax::readXml(std::span<const std::byte>(bytes), handler, warnings);
        }
    }

    [[nodiscard]] OpenRocketDocument&      document() noexcept { return fixture.document(); }
    [[nodiscard]] std::vector<std::string> texts() const { return warningTexts(warnings); }
};

/// The document of the case @p name loaded, and read by the root handler alone.
[[nodiscard]] Reading loaded(std::string_view name)
{
    return {documentOfLoadCase(name), true};
}
[[nodiscard]] Reading onlyRead(std::string_view name)
{
    return {documentOfLoadCase(name), false};
}

/// 't' or 'f' for each of the two stages of @p configuration.
[[nodiscard]] std::string activeStages(const QtRocket::FlightConfiguration& configuration)
{
    return std::string{configuration.isStageActive(0) ? 't' : 'f',
                       configuration.isStageActive(1) ? 't' : 'f'};
}

/// activeStages() of the default configuration of @p rocket and of each of its others.
[[nodiscard]] std::vector<std::string> activeStagesOfAll(const QtRocket::Rocket& rocket)
{
    std::vector<std::string> all{activeStages(rocket.getEmptyConfiguration())};
    for (const QtRocket::FlightConfigurationId& id : rocket.getIds())
    {
        all.push_back(activeStages(rocket.getFlightConfiguration(id)));
    }
    return all;
}

// --------------------------------------------------------------------------- the reading

// The document is read into the document of the context, the file version is set, and the
// warnings of the handlers are added to the set that is given, behind what it holds.
TEST(OpenRocketLoader, ReadsTheDocumentIntoTheDocumentOfTheContext)
{
    HandlerFixture fixture;
    fixture.context().setFileVersion(0);
    WarningSet warnings;
    warnings.add("An earlier warning.");
    const std::vector<std::byte> bytes = stringToBytes(
        R"(<openrocket version="1.4"><rocket><name>R</name></rocket><foo/></openrocket>)");

    const Result<void> result = OpenRocketLoader::load(fixture.context(), bytes, warnings);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fixture.rocket().getName(), "R");
    EXPECT_EQ(fixture.context().getFileVersion(), 104);
    EXPECT_THAT(warningTexts(warnings),
                ElementsAre("An earlier warning.", "Unknown element foo, ignoring."));
}

// The bytes are read as the JDK's parser reads a stream: the encoding is the document's.
TEST(OpenRocketLoader, FindsTheEncodingOfTheDocument)
{
    HandlerFixture               fixture;
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(
        "<?xml version='1.0' encoding='ISO-8859-1'?><openrocket version=\"1.10\"><rocket>"
        "<name>caf\xE9</name></rocket></openrocket>");

    ASSERT_TRUE(OpenRocketLoader::load(fixture.context(), bytes, warnings).has_value());
    EXPECT_EQ(fixture.rocket().getName(), "caf\xC3\xA9");
}

/// Whether the loader loads a document of nothing but its root element with @p context.
[[nodiscard]] bool loadsAnEmptyDocument(DocumentLoadingContext& context)
{
    const std::vector<std::byte> bytes = stringToBytes(R"(<openrocket version="1.10"/>)");
    WarningSet                   warnings;
    return OpenRocketLoader::load(context, bytes, warnings).has_value();
}

// What the handlers need whenever a file has the element for it is asked for before the file
// is read: a document, a motor finder and a preference store.
TEST(OpenRocketLoader, TheContextMustHaveADocumentAMotorFinderAndAPreferenceStore)
{
    HandlerFixture fixture;
    EXPECT_TRUE(loadsAnEmptyDocument(fixture.context()));

    DocumentLoadingContext noDocument = fixture.context();
    noDocument.setOpenRocketDocument(nullptr);
    EXPECT_THROW(static_cast<void>(loadsAnEmptyDocument(noDocument)), BugError);
    DocumentLoadingContext noFinder = fixture.context();
    noFinder.setMotorFinder(nullptr);
    EXPECT_THROW(static_cast<void>(loadsAnEmptyDocument(noFinder)), BugError);
    DocumentLoadingContext noPreferences = fixture.context();
    noPreferences.setPreferences(nullptr);
    EXPECT_THROW(static_cast<void>(loadsAnEmptyDocument(noPreferences)), BugError);
}

// ------------------------------------------------------------------------------ failures

/// What the loader makes of @p document: "loaded", or the failure as "<code>: <message>".
[[nodiscard]] std::string failure(std::string_view document)
{
    HandlerFixture               fixture;
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(document);
    const Result<void> result          = OpenRocketLoader::load(fixture.context(), bytes, warnings);
    return result.has_value()
               ? std::string("loaded")
               : std::string(toString(result.error().code)) + ": " + result.error().message;
}

// Malformed XML is Java's SAXException, an encoding that is not read its IOException, each
// with the text OpenRocket's loader gives it; a failure of a handler is returned as it is
// (Java: the IllegalArgumentException leaves the loader, and GeneralRocketLoader words it).
TEST(OpenRocketLoader, FailsWithJavasTextsAndLeavesAHandlersFailureAsItIs)
{
    EXPECT_EQ(failure(R"(<openrocket version="1.10"><rocket></openrocket>)"),
              "PARSE: Malformed XML in input.");
    EXPECT_EQ(failure(""), "PARSE: Malformed XML in input.");
    EXPECT_EQ(failure("<openrocket version=\"1.10\"><rocket><name>caf\xE9</name></rocket>"
                      "</openrocket>"),
              "PARSE: Malformed XML in input.");
    EXPECT_EQ(failure("<?xml version='1.0' encoding='klingon'?><openrocket version=\"1.10\"/>"),
              "IO: I/O error: klingon");
    EXPECT_EQ(failure(R"(<openrocket version="1.10"><rocket><id>not-a-uuid</id></rocket>)"
                      R"(</openrocket>)"),
              "INVALID_ARGUMENT: Invalid UUID string: not-a-uuid");
    EXPECT_EQ(failure(R"(<openrocket version="1.10"><rocket><id>1--3-4-5</id></rocket>)"
                      R"(</openrocket>)"),
              "INVALID_ARGUMENT: ");
    // Not OpenRocket's, which reads such a document: XmlScanner's refusal, as it is.
    EXPECT_EQ(failure("<!DOCTYPE openrocket [<!ENTITY e 'x'>]><openrocket version=\"1.10\"/>"),
              "UNSUPPORTED_FORMAT: Unsupported document type declaration: markup declarations");
}

// After a failure none of the steps has run: the document holds what was read before, as the
// handlers left it, and the undo history still has the states of the reading.
TEST(OpenRocketLoader, AFailureLeavesTheDocumentAsTheHandlersLeftIt)
{
    HandlerFixture fixture;
    fixture.document().getDefaultStorageOptions().setExplicitlySet(true);
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(
        R"(<openrocket version="1.10"><rocket><name>Half</name><id>x</id></rocket></openrocket>)");

    const Result<void> result = OpenRocketLoader::load(fixture.context(), bytes, warnings);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(fixture.rocket().getName(), "Half");
    EXPECT_TRUE(fixture.document().getDefaultStorageOptions().isExplicitlySet());
    EXPECT_FALSE(fixture.document().getUndoDetail().clean);
}

// ------------------------------------------------------------- the steps after the reading

// Step 1: the stage activeness the flight configurations of the file name is applied when the
// whole file has been read, to every configuration. Until then every stage is active. The
// five configurations of the case: stage 1 inactive, stage 0 inactive, nothing said, both
// inactive (stage 0 is then active all the same) and one that names a stage the rocket does
// not have.
TEST(OpenRocketLoader, AppliesTheStageActivenessOfTheFlightConfigurations)
{
    Reading read = onlyRead("p-stage-activeness");
    ASSERT_TRUE(read.result.has_value());
    EXPECT_THAT(activeStagesOfAll(read.document().getRocket()),
                ElementsAre("tt", "tt", "tt", "tt", "tt", "tt"));

    Reading load = loaded("p-stage-activeness");
    ASSERT_TRUE(load.result.has_value());
    EXPECT_THAT(activeStagesOfAll(load.document().getRocket()),
                ElementsAre("tt", "tf", "ft", "tt", "tf", "tf"));
    EXPECT_THAT(load.texts(), IsEmpty());
}

/// The first step of the loader, by itself.
void applyStageActiveness(QtRocket::Rocket& rocket)
{
    for (QtRocket::FlightConfiguration& configuration : rocket.getFlightConfigurations().values())
    {
        configuration.applyPreloadedStageActiveness();
    }
}

// Step 2: the simulations take the modification id their flight configuration has now, so
// that a simulation with stored data is not out of date for what the rest of the file did to
// the rocket: here the stage activeness, which was applied after the simulation was read.
TEST(OpenRocketLoader, SynchronisesTheSimulationsWithTheirFlightConfigurations)
{
    Reading read = onlyRead("m-sim-uptodate");
    ASSERT_TRUE(read.result.has_value());
    ASSERT_EQ(read.document().getSimulationCount(), 1U);
    // What the loader does first, and what the simulation says without the second step.
    applyStageActiveness(read.document().getRocket());
    EXPECT_EQ(read.document().getSimulation(0)->getStatus(), Simulation::Status::OUTDATED);

    Reading load = loaded("m-sim-uptodate");
    ASSERT_TRUE(load.result.has_value());
    EXPECT_EQ(load.document().getSimulation(0)->getStatus(), Simulation::Status::LOADED);
}

// Step 2, the storage options: a simulation that came with stored data makes the document
// save simulated data; without one the option stays as it was (it is never switched off).
TEST(OpenRocketLoader, ASimulationWithStoredDataMakesTheDocumentSaveSimulatedData)
{
    EXPECT_FALSE(
        onlyRead("m-sim-uptodate").document().getDefaultStorageOptions().getSaveSimulationData());
    EXPECT_TRUE(
        loaded("m-sim-uptodate").document().getDefaultStorageOptions().getSaveSimulationData());
    // A summary without a branch, no flight data at all, a first branch without a time
    // column, and a rocket without simulations.
    EXPECT_FALSE(
        loaded("m-sim-summary-only").document().getDefaultStorageOptions().getSaveSimulationData());
    EXPECT_FALSE(loaded("m-sim-no-flightdata")
                     .document()
                     .getDefaultStorageOptions()
                     .getSaveSimulationData());
    EXPECT_FALSE(
        loaded("m-sim-no-time").document().getDefaultStorageOptions().getSaveSimulationData());
    EXPECT_FALSE(loaded("m-sim-second-branch-has-time")
                     .document()
                     .getDefaultStorageOptions()
                     .getSaveSimulationData());
    EXPECT_FALSE(loaded("p-undo").document().getDefaultStorageOptions().getSaveSimulationData());
    // One of several is enough.
    EXPECT_TRUE(
        loaded("m-sim-three").document().getDefaultStorageOptions().getSaveSimulationData());
}

// Step 3: the storage options are those of a design file that nobody chose: not explicitly
// set, and of the type OPENROCKET, whatever the document had. Saving simulated data is not
// switched off.
TEST(OpenRocketLoader, MarksTheStorageOptionsAsThoseOfADesignFile)
{
    HandlerFixture  fixture;
    StorageOptions& options = fixture.document().getDefaultStorageOptions();
    options.setExplicitlySet(true);
    options.setFileType(StorageOptions::FileType::ROCKSIM);
    options.setSaveSimulationData(true);
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(documentOfLoadCase("p-undo"));

    ASSERT_TRUE(OpenRocketLoader::load(fixture.context(), bytes, warnings).has_value());

    EXPECT_FALSE(options.isExplicitlySet());
    EXPECT_EQ(options.getFileType(), StorageOptions::FileType::OPENROCKET);
    EXPECT_TRUE(options.getSaveSimulationData());
}

/// The scripting extension of simulation @p index of @p document.
[[nodiscard]] const ScriptingExtension& scriptOf(const OpenRocketDocument& document,
                                                 std::size_t               index)
{
    const auto* const script = dynamic_cast<const ScriptingExtension*>(
        document.getSimulation(index)->getSimulationExtensions().front().get());
    if (script == nullptr)
    {
        QtRocket::bug("the simulation has no scripting extension");
    }
    return *script;
}

// Step 4: every extension of every simulation is told that the document was loaded, with the
// load's warnings. The scripting extension disables an enabled script there and says so, once
// for all the scripts of a file; a script that was disabled gives no warning.
TEST(OpenRocketLoader, TellsEveryExtensionThatTheDocumentWasLoaded)
{
    Reading read = onlyRead("m-ext-script-enabled");
    ASSERT_TRUE(read.result.has_value());
    EXPECT_TRUE(scriptOf(read.document(), 0).isEnabled());
    EXPECT_THAT(read.texts(), IsEmpty());

    Reading load = loaded("m-ext-script-enabled");
    ASSERT_TRUE(load.result.has_value());
    EXPECT_FALSE(scriptOf(load.document(), 0).isEnabled());
    EXPECT_THAT(load.texts(), ElementsAre(std::string(ScriptingExtension::kDisabledWarning)));

    Reading three = loaded("p-ext-script-twice");
    ASSERT_TRUE(three.result.has_value());
    ASSERT_EQ(three.document().getSimulationCount(), 2U);
    EXPECT_FALSE(scriptOf(three.document(), 0).isEnabled());
    EXPECT_FALSE(scriptOf(three.document(), 1).isEnabled());
    EXPECT_THAT(three.texts(), ElementsAre(std::string(ScriptingExtension::kDisabledWarning)));

    EXPECT_THAT(loaded("p-ext-script-disabled").texts(), IsEmpty());
}

// The warnings of the extensions come behind those of the handlers.
TEST(OpenRocketLoader, TheWarningsOfTheExtensionsFollowThoseOfTheHandlers)
{
    std::string document(documentOfLoadCase("m-ext-script-enabled"));
    document.insert(document.rfind("</openrocket>"), "<foo/>");

    const Reading load(document, true);

    ASSERT_TRUE(load.result.has_value());
    EXPECT_THAT(load.texts(), ElementsAre("Unknown element foo, ignoring.",
                                          std::string(ScriptingExtension::kDisabledWarning)));
}

// Step 5: the undo history is cleared. While the file is read every change of the rocket is a
// change of the document, which is then not in the state its undo history starts from; the
// loaded document is, and has nothing to undo.
TEST(OpenRocketLoader, ClearsTheUndoHistory)
{
    Reading read = onlyRead("m-sim-uptodate");
    ASSERT_TRUE(read.result.has_value());
    EXPECT_FALSE(read.document().getUndoDetail().clean);

    Reading load = loaded("m-sim-uptodate");
    ASSERT_TRUE(load.result.has_value());
    const OpenRocketDocument::UndoDetail undo = load.document().getUndoDetail();
    EXPECT_TRUE(undo.clean);
    EXPECT_EQ(undo.position, 0U);
    EXPECT_EQ(undo.descriptions.size(), 1U);
    EXPECT_FALSE(load.document().isUndoAvailable());
    EXPECT_FALSE(load.document().isRedoAvailable());
    // The document is not saved: the reading changed it, and the loader does not say
    // otherwise (who opens a file calls setSaved()).
    EXPECT_FALSE(load.document().isSaved());
}

// The events of the rocket are on while the file is read and afterwards: the loader neither
// switches them off nor bypasses them (decision D3).
TEST(OpenRocketLoader, LeavesTheEventsOfTheRocketEnabled)
{
    HandlerFixture fixture;
    int            events = 0;
    const auto     listening =
        fixture.rocket().addComponentChangeListener([&events](const auto& /*event*/) { events++; });
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(documentOfLoadCase("p-undo"));

    ASSERT_TRUE(OpenRocketLoader::load(fixture.context(), bytes, warnings).has_value());

    // OpenRocket's count for this document (the case p-undo of the probe).
    EXPECT_EQ(events, 10);
    EXPECT_TRUE(fixture.rocket().isEventsEnabled());
}

}  // namespace
