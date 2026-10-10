// The bounds on what one design file may ask of the loader (decision U6 of tier 9, the user's
// numbers): the components of the rocket, its flight configurations, the simulations of the
// document, the levels of one wind model and the warnings of one warning set. They are
// QtRocket's own. OpenRocket has none, and its time and memory grow with what a file says: a
// megabyte of small elements took minutes or hours to load, there and here (the measurements
// are at the end of this file). So nothing here has a Java original.
//
// Two more bounds are the loader's own, with numbers no decision gave: the stages, booster
// sets and pod sets of a rocket (a hundred; they cost far more than other components) and the
// decal images of a document (a thousand). The review of run 9d measured what asks for them.
//
// Each bound is pinned with a document that reaches it exactly and goes one beyond. Reading up
// to a bound is itself slow in a debug build where every element is a change of the rocket that
// every flight configuration answers (ten thousand components, a thousand flight
// configurations), so there the rocket is filled up to just below the bound before the file is
// read, and the file brings the last ones. The components and the warnings are read through
// GeneralRocketLoader. The flight configurations and the simulations are read through the
// handlers alone (the content of the rocket element, the <simulations> element): a whole load
// of a rocket with a thousand flight configurations took half a minute in a debug build, which
// is no time for a test (the measurements at the end of this file are whole loads).
//
// The bound on the levels of a wind model is in WindHandlerTests.cpp. The bounds that run 9b
// made (a count, the component instances, the nesting of components) are in
// IntSetterTests.cpp, instance_budget_tests.cpp and ComponentHandlerTests.cpp.

#include <chrono>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/RawZip.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentHandler;
using QtRocket::DocumentConfig;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::GeneralRocketLoader;
using QtRocket::LaunchLug;
using QtRocket::LoadedDocument;
using QtRocket::LoaderWarnings;
using QtRocket::MassComponent;
using QtRocket::OpenRocketDocument;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationsHandler;
using QtRocket::stringToBytes;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kSimulationMotorFcid;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// --------------------------------------------------------------------------- building blocks

/// A design of format 1.10 with @p rocket as what its rocket element holds and @p simulations
/// as what its simulations element holds.
[[nodiscard]] std::string designOf(std::string_view rocket, std::string_view simulations = {})
{
    return std::format(
        "<openrocket version=\"1.10\"><rocket>{}</rocket><simulations>{}"
        "</simulations></openrocket>",
        rocket, simulations);
}

/// A stage that holds @p inner.
[[nodiscard]] std::string inStage(std::string_view inner)
{
    return std::format(
        "<subcomponents><stage><subcomponents>{}</subcomponents></stage>"
        "</subcomponents>",
        inner);
}

/// @p count times @p element, one behind the other.
[[nodiscard]] std::string repeated(std::string_view element, std::size_t count)
{
    std::string all;
    all.reserve(count * element.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        all += element;
    }
    return all;
}

/// The id of the flight configuration number @p n of these tests, as a file spells it.
[[nodiscard]] std::string idText(int n)
{
    return std::format("{:08x}-0000-0000-0000-00000000000a", n);
}

/// The id idText(@p n) spells.
[[nodiscard]] FlightConfigurationId idOf(int n)
{
    return FlightConfigurationId::fromString(idText(n));
}

/// The design @p xml, loaded with @p options in the environment @p environment.
[[nodiscard]] Result<LoadedDocument> load(LoaderEnvironment& environment, std::string_view xml,
                                          GeneralRocketLoader::Options options = {})
{
    const GeneralRocketLoader loader(environment.context(), std::move(options));
    return loader.load(stringToBytes(xml));
}

/// The warnings of a load of the design @p xml, or the failure of the load as the one text.
[[nodiscard]] Texts warningsOfLoading(std::string_view xml)
{
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, xml);
    if (!loaded)
    {
        return {"FAILED " + loaded.error().message};
    }
    return warningTexts(loaded->warnings);
}

// ----------------------------------------------------------------------------- the numbers

TEST(LoaderBounds, TheBoundsAreTheNumbersThatWereDecided)
{
    EXPECT_EQ(DocumentConfig::kMaxComponents, 10000U);
    EXPECT_EQ(DocumentConfig::kMaxFlightConfigurations, 1000U);
    EXPECT_EQ(DocumentConfig::kMaxSimulations, 10000U);
    EXPECT_EQ(DocumentConfig::kMaxWindLevels, 10000U);
    EXPECT_EQ(DocumentConfig::kMaxWarnings, 1000U);
    // The loader's own numbers (the fix of run 9d).
    EXPECT_EQ(DocumentConfig::kMaxAssemblies, 100U);
    EXPECT_EQ(DocumentConfig::kMaxDecalImages, 1000U);
    // The bounds of run 9b stay as they were.
    EXPECT_EQ(DocumentConfig::kMaxCount, 10000);
    EXPECT_EQ(DocumentConfig::kMaxInstances, 100000U);
    EXPECT_EQ(ComponentHandler::kMaxDepth, 100);
}

// ---------------------------------------------------------------------------- components

/// A stage in no rocket that holds @p components components: body tubes of ninety-nine mass
/// components each, and a last tube with the rest. Building it fires nothing, and every tube
/// is filled before it is put into the stage, so that no component is added to a large tree.
[[nodiscard]] std::unique_ptr<AxialStage> stageOf(std::size_t components)
{
    constexpr std::size_t kTubeAndMasses = 100;
    auto                  stage          = std::make_unique<AxialStage>();
    while (components > 0)
    {
        const std::size_t here = components < kTubeAndMasses ? components : kTubeAndMasses;
        auto              tube = std::make_unique<BodyTube>();
        for (std::size_t i = 1; i < here; ++i)
        {
            tube->addChild(std::make_unique<MassComponent>());
        }
        stage->addChild(std::move(tube));
        components -= here;
    }
    return stage;
}

TEST(LoaderBounds, TheComponentsOfATreeAreCountedWithoutItsRoot)
{
    Rocket rocket;
    EXPECT_EQ(DocumentConfig::componentCount(rocket), 0U);

    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   tube  = stage.addChild(std::make_unique<BodyTube>());
    tube.addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(DocumentConfig::componentCount(rocket), 3U);
    EXPECT_EQ(DocumentConfig::componentCount(tube), 3U) << "asked of any component of the tree";

    // A component is one, however many instances it has, and whatever the flight
    // configurations are.
    LaunchLug& lug = tube.addChild(std::make_unique<LaunchLug>());
    lug.setInstanceCount(5);
    rocket.createFlightConfiguration(FlightConfigurationId{});
    EXPECT_EQ(DocumentConfig::componentCount(lug), 4U);

    // A tree without a rocket: its root is not counted either.
    EXPECT_EQ(DocumentConfig::componentCount(*stageOf(7)), 7U);
    EXPECT_EQ(DocumentConfig::componentCount(*stageOf(250)), 250U);
    EXPECT_EQ(DocumentConfig::componentCount(BodyTube{}), 0U);
}

TEST(LoaderBounds, AComponentFitsUpToTheBoundExactly)
{
    const std::unique_ptr<AxialStage> stage = stageOf(DocumentConfig::kMaxComponents - 2);
    ASSERT_EQ(DocumentConfig::componentCount(*stage), DocumentConfig::kMaxComponents - 2);
    // A body tube with a mass component in it: two components.
    BodyTube pair;
    pair.addChild(std::make_unique<MassComponent>());

    EXPECT_TRUE(DocumentConfig::componentFits(*stage, BodyTube{}));
    EXPECT_TRUE(DocumentConfig::componentFits(*stage, pair)) << "the two reach the bound";

    BodyTube& last = stage->addChild(std::make_unique<BodyTube>());
    ASSERT_EQ(DocumentConfig::componentCount(*stage), DocumentConfig::kMaxComponents - 1);
    EXPECT_TRUE(DocumentConfig::componentFits(*stage, BodyTube{}));
    EXPECT_TRUE(DocumentConfig::componentFits(last, MassComponent{}))
        << "wherever in the tree it would stand";
    EXPECT_FALSE(DocumentConfig::componentFits(*stage, pair)) << "a child counts with its own";

    last.addChild(std::make_unique<MassComponent>());
    ASSERT_EQ(DocumentConfig::componentCount(*stage), DocumentConfig::kMaxComponents);
    EXPECT_FALSE(DocumentConfig::componentFits(*stage, BodyTube{}));
    EXPECT_FALSE(DocumentConfig::componentFits(last, MassComponent{}));
}

/// Options that give the rocket of a load @p components components before the file is read: a
/// stage with what stageOf() puts into it, made outside the rocket and attached with one change
/// of it.
[[nodiscard]] GeneralRocketLoader::Options withComponents(std::size_t components)
{
    GeneralRocketLoader::Options options;
    options.beforeReading = [components](const DocumentLoadingContext& context) {
        context.getOpenRocketDocument()->getRocket().addChild(stageOf(components - 1));
    };
    return options;
}

// The rocket has three components fewer than the bound when the file is read. The file brings
// a stage, a body tube and a mass component, which is the ten thousandth, and then an inner
// tube with a mass component in it and one more mass component.
TEST(LoaderBounds, ARocketTakesComponentsUpToTheBoundAndTheNextIsIgnoredWithWhatItHolds)
{
    const std::string xml =
        designOf(inStage("<bodytube><subcomponents>"
                         "<masscomponent><name>the last one</name></masscomponent>"
                         "<innertube><name>one too many</name><subcomponents><masscomponent/>"
                         "</subcomponents></innertube>"
                         "<masscomponent><name>another too many</name></masscomponent>"
                         "</subcomponents></bodytube>"));
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded =
        load(environment, xml, withComponents(DocumentConfig::kMaxComponents - 3));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;

    const Rocket& rocket = loaded->document->getRocket();
    EXPECT_EQ(DocumentConfig::componentCount(rocket), DocumentConfig::kMaxComponents);
    // The stage and the tube of the file, and in the tube the one component that fitted.
    ASSERT_EQ(rocket.getChildCount(), 2U);
    ASSERT_EQ(rocket.getChild(1).getChildCount(), 1U);
    const RocketComponent& tube = rocket.getChild(1).getChild(0);
    ASSERT_EQ(tube.getChildCount(), 1U);
    EXPECT_EQ(tube.getChild(0).getName(), "the last one");
    // One warning for each component that was ignored, by the name of its kind: none for the
    // mass component in the inner tube, which was never looked at.
    EXPECT_EQ(warningTexts(loaded->warnings),
              (Texts{LoaderWarnings::tooManyComponents("Inner Tube"),
                     LoaderWarnings::tooManyComponents("Mass Component")}));
}

// ------------------------------------------------------------------------------ assemblies
//
// Stages, booster sets and pod sets (the assemblies) are bounded by themselves: at every change
// of the rocket each of them finds its place again by walking over its siblings, so the time to
// read N of them grows with more than the cube of N (DocumentConfig::kMaxAssemblies has the
// measurements). The number, 100, is the loader's own.

TEST(LoaderBounds, TheAssembliesOfATreeAreCountedWithoutItsRoot)
{
    Rocket rocket;
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), 0U) << "the rocket is the root";

    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), 1U);
    // A component that is no assembly is none.
    BodyTube& tube = stage.addChild(std::make_unique<BodyTube>());
    tube.addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), 1U);
    // A pod set and a booster set are, wherever they stand, and whatever their instances.
    PodSet& pods = tube.addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(4);
    tube.addChild(std::make_unique<ParallelStage>());
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), 3U);
    EXPECT_EQ(DocumentConfig::assemblyCount(pods), 3U) << "asked of any component of the tree";
    rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(DocumentConfig::assemblyCount(tube), 4U);

    // A tree without a rocket: its root is not counted either.
    EXPECT_EQ(DocumentConfig::assemblyCount(AxialStage{}), 0U);
    BodyTube alone;
    alone.addChild(std::make_unique<PodSet>());
    EXPECT_EQ(DocumentConfig::assemblyCount(alone), 1U);
}

/// A rocket with @p stages empty stages.
[[nodiscard]] std::unique_ptr<Rocket> rocketOfStages(std::size_t stages)
{
    auto rocket = std::make_unique<Rocket>();
    for (std::size_t i = 0; i < stages; ++i)
    {
        rocket->addChild(std::make_unique<AxialStage>());
    }
    return rocket;
}

TEST(LoaderBounds, AnAssemblyFitsUpToTheBoundExactly)
{
    const std::unique_ptr<Rocket> rocket = rocketOfStages(DocumentConfig::kMaxAssemblies - 2);
    BodyTube&                     tube = rocket->getChild(0).addChild(std::make_unique<BodyTube>());
    ASSERT_EQ(DocumentConfig::assemblyCount(*rocket), DocumentConfig::kMaxAssemblies - 2);
    // A body tube with a pod set on it: one assembly, which counts with the tube that holds it.
    BodyTube tubeWithPods;
    tubeWithPods.addChild(std::make_unique<PodSet>());
    // A stage with such a tube: two.
    AxialStage stageWithPods;
    stageWithPods.addChild(std::make_unique<BodyTube>()).addChild(std::make_unique<PodSet>());

    EXPECT_TRUE(DocumentConfig::assemblyFits(*rocket, AxialStage{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(tube, PodSet{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(*rocket, stageWithPods)) << "the two reach the bound";

    tube.addChild(std::make_unique<ParallelStage>());
    ASSERT_EQ(DocumentConfig::assemblyCount(*rocket), DocumentConfig::kMaxAssemblies - 1);
    EXPECT_TRUE(DocumentConfig::assemblyFits(*rocket, AxialStage{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(tube, PodSet{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(tube, ParallelStage{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(rocket->getChild(1), tubeWithPods));
    EXPECT_FALSE(DocumentConfig::assemblyFits(*rocket, stageWithPods))
        << "a child counts with its own";

    rocket->addChild(std::make_unique<AxialStage>());
    ASSERT_EQ(DocumentConfig::assemblyCount(*rocket), DocumentConfig::kMaxAssemblies);
    EXPECT_FALSE(DocumentConfig::assemblyFits(*rocket, AxialStage{}));
    EXPECT_FALSE(DocumentConfig::assemblyFits(tube, PodSet{}));
    EXPECT_FALSE(DocumentConfig::assemblyFits(tube, ParallelStage{}));
    EXPECT_FALSE(DocumentConfig::assemblyFits(rocket->getChild(1), tubeWithPods));
    // What is no assembly and holds none always fits: this bound is not its business.
    EXPECT_TRUE(DocumentConfig::assemblyFits(tube, MassComponent{}));
    EXPECT_TRUE(DocumentConfig::assemblyFits(rocket->getChild(1), BodyTube{}));
}

/// The elements of a rocket with a first stage whose body tube carries a pod set and a
/// booster set, and @p emptyStages empty stages behind it: three assemblies and those.
[[nodiscard]] std::string stagesWithPodsAndBoosters(std::size_t emptyStages)
{
    return "<stage><name>first</name><subcomponents><bodytube><subcomponents>"
           "<podset><name>a pod set</name></podset>"
           "<parallelstage><name>a booster set</name></parallelstage>"
           "</subcomponents></bodytube></subcomponents></stage>" +
           repeated("<stage/>", emptyStages);
}

// At the bound: a hundred assemblies of the three kinds load, and nothing is said.
TEST(LoaderBounds, ARocketTakesAssembliesUpToTheBound)
{
    const std::string xml =
        designOf(std::format("<subcomponents>{}</subcomponents>",
                             stagesWithPodsAndBoosters(DocumentConfig::kMaxAssemblies - 3)));
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, xml);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;

    const Rocket& rocket = loaded->document->getRocket();
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), DocumentConfig::kMaxAssemblies);
    EXPECT_EQ(rocket.getChildCount(), DocumentConfig::kMaxAssemblies - 2);
    EXPECT_EQ(warningTexts(loaded->warnings), Texts{});
}

// One above: the hundredth assembly is a stage, and what comes after it is one too many
// whatever its kind and wherever it would stand. It is ignored with what it holds; a component
// that is no assembly still finds its place.
TEST(LoaderBounds, AnAssemblyBeyondTheBoundIsIgnoredWithWhatItHolds)
{
    const std::string            xml = designOf(std::format(
        "<subcomponents>{}"
        "<stage><name>the hundredth</name><subcomponents><bodytube><subcomponents>"
        "<podset><name>a pod set too many</name><subcomponents><bodytube/></subcomponents>"
        "</podset>"
        "<parallelstage><name>a booster set too many</name></parallelstage>"
        "<masscomponent><name>still fits</name></masscomponent>"
        "</subcomponents></bodytube></subcomponents></stage>"
        "<stage><name>a stage too many</name><subcomponents><bodytube/></subcomponents></stage>"
        "</subcomponents>",
        stagesWithPodsAndBoosters(DocumentConfig::kMaxAssemblies - 4)));
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, xml);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;

    const Rocket& rocket = loaded->document->getRocket();
    EXPECT_EQ(DocumentConfig::assemblyCount(rocket), DocumentConfig::kMaxAssemblies);
    // The stages: the first, the empty ones and the hundredth assembly.
    ASSERT_EQ(rocket.getChildCount(), DocumentConfig::kMaxAssemblies - 2);
    const RocketComponent& last = rocket.getChild(rocket.getChildCount() - 1);
    EXPECT_EQ(last.getName(), "the hundredth");
    ASSERT_EQ(last.getChildCount(), 1U);
    ASSERT_EQ(last.getChild(0).getChildCount(), 1U);
    EXPECT_EQ(last.getChild(0).getChild(0).getName(), "still fits");
    // One warning for each assembly that was ignored, by the name of its kind, in the order of
    // the file: none for the body tubes in them, which were never looked at.
    EXPECT_EQ(warningTexts(loaded->warnings),
              (Texts{LoaderWarnings::tooManyAssemblies("Pod Set"),
                     LoaderWarnings::tooManyAssemblies("Booster Set"),
                     LoaderWarnings::tooManyAssemblies("Stage")}));
}

// ----------------------------------------------------------------------------- decal images
//
// The decal images of a document are bounded: its registry looks through all its images for
// every new one, so the time to read N decals of N images grows with N squared
// (DocumentConfig::kMaxDecalImages has the measurements). The number, 1000, is the loader's own.

/// A <decal> of the image named "d<number>.png".
[[nodiscard]] std::string decalOf(std::size_t number)
{
    return std::format(R"(<decal name="d{}.png" rotation="0" edgemode="REPEAT"/>)", number);
}

/// A design whose one body tube has an appearance with a decal of each of @p images images,
/// "d0.png" on, and then the elements @p more.
[[nodiscard]] std::string designOfDecals(std::size_t images, std::string_view more = {})
{
    std::string decals;
    for (std::size_t i = 0; i < images; ++i)
    {
        decals += decalOf(i);
    }
    return designOf(
        inStage(std::format("<bodytube><appearance>{}{}</appearance></bodytube>", decals, more)));
}

/// How a design of decals reaches the loader: as a plain document, whose images are files, or
/// as the entry of an archive, whose images are entries of it that are found by their names.
enum class DecalsOf
{
    FILES,
    AN_ARCHIVE,
};

/// What a load of a design of decals gave: the images of the document, the decal of the body
/// tube as "<image> <rotation> <edge mode>", and the warnings.
struct DecalLoad
{
    std::string failure;
    std::size_t images{0};
    std::string decal;
    Texts       warnings;
};

[[nodiscard]] DecalLoad loadDecals(const std::string& xml, DecalsOf source)
{
    DecalLoad                    load;
    LoaderEnvironment            environment;
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> loaded =
        source == DecalsOf::FILES
            ? loader.load(stringToBytes(xml))
            : loader.load(QtRocket::Test::archive({QtRocket::Test::stored("rocket.ork", xml)}));
    if (!loaded.has_value())
    {
        load.failure = loaded.error().message;
        return load;
    }
    load.images          = loaded->document->getDecalImageCount();
    load.warnings        = warningTexts(loaded->warnings);
    const Rocket& rocket = loaded->document->getRocket();
    if (rocket.getChildCount() == 1 && rocket.getChild(0).getChildCount() == 1)
    {
        const std::optional<QtRocket::Appearance>& appearance =
            rocket.getChild(0).getChild(0).getAppearance();
        if (appearance.has_value() && appearance->getTexture().has_value())
        {
            const QtRocket::Decal& decal = *appearance->getTexture();
            load.decal = std::format("{} {} {}", decal.getImageName(), decal.getRotation(),
                                     QtRocket::edgeModeName(decal.getEdgeMode()));
        }
    }
    return load;
}

/// The name the registry gives the image "d<number>.png" of a design that reaches the loader
/// as @p source: a file's is put into the folder of the decals, an entry's is the entry's.
[[nodiscard]] std::string imageName(std::size_t number, DecalsOf source)
{
    return std::format("{}d{}.png", source == DecalsOf::FILES ? "decals/" : "", number);
}

class LoaderDecalBound : public ::testing::TestWithParam<DecalsOf>
{ };

// At the bound: a thousand images load, and nothing is said.
TEST_P(LoaderDecalBound, ADocumentTakesDecalImagesUpToTheBound)
{
    const DecalLoad load = loadDecals(designOfDecals(DocumentConfig::kMaxDecalImages), GetParam());
    EXPECT_EQ(load.failure, "");
    EXPECT_EQ(load.images, DocumentConfig::kMaxDecalImages);
    EXPECT_EQ(load.warnings, Texts{});
    EXPECT_EQ(load.decal, imageName(DocumentConfig::kMaxDecalImages - 1, GetParam()) + " 0 REPEAT");
}

// One above: the decal of one image more is ignored with one warning and nothing else is said
// of it, whatever its rotation and its edge mode are (one that is no number or none would fail
// the load of a decal that is read). The decal the appearance has is the one before it.
TEST_P(LoaderDecalBound, TheDecalOfAnImageBeyondTheBoundIsIgnored)
{
    const DecalLoad load =
        loadDecals(designOfDecals(DocumentConfig::kMaxDecalImages,
                                  R"(<decal name="one-too-many.png" rotation="x" edgemode="NONE">)"
                                  R"(<center x="0.1" y="0.2"/><unknown/></decal>)"),
                   GetParam());
    EXPECT_EQ(load.failure, "");
    EXPECT_EQ(load.images, DocumentConfig::kMaxDecalImages);
    EXPECT_EQ(load.warnings, Texts{LoaderWarnings::tooManyDecalImages()});
    EXPECT_EQ(load.decal, imageName(DocumentConfig::kMaxDecalImages - 1, GetParam()) + " 0 REPEAT");
}

// A decal of an image the document has already is read as ever when the bound is reached: it
// makes no image. And every further decal of a new image gets the one warning again (a
// warning set holds a text once).
TEST_P(LoaderDecalBound, ADecalOfAKnownImageIsReadAfterTheBoundIsReached)
{
    const DecalLoad load = loadDecals(
        designOfDecals(DocumentConfig::kMaxDecalImages,
                       R"(<decal name="one-too-many.png" rotation="0" edgemode="REPEAT"/>)"
                       R"(<decal name="d5.png" rotation="1.5" edgemode="MIRROR"/>)"
                       R"(<decal name="another-too-many.png" rotation="0" edgemode="REPEAT"/>)"),
        GetParam());
    EXPECT_EQ(load.failure, "");
    EXPECT_EQ(load.images, DocumentConfig::kMaxDecalImages);
    EXPECT_EQ(load.warnings, Texts{LoaderWarnings::tooManyDecalImages()});
    EXPECT_EQ(load.decal, imageName(5, GetParam()) + " 1.5 MIRROR");
}

/// The name of the test of a source of decals.
[[nodiscard]] std::string decalsTestName(const ::testing::TestParamInfo<DecalsOf>& info)
{
    std::string name = "Files";
    if (info.param == DecalsOf::AN_ARCHIVE)
    {
        name = "AnArchive";
    }
    return name;
}

INSTANTIATE_TEST_SUITE_P(Sources, LoaderDecalBound,
                         ::testing::Values(DecalsOf::FILES, DecalsOf::AN_ARCHIVE), decalsTestName);

// ------------------------------------------------------------------ flight configurations

/// A <motor> of the flight configuration number @p n, without an ejection charge.
[[nodiscard]] std::string motorOf(int n)
{
    return std::format(
        "<motor configid=\"{}\"><designation>X</designation><delay>none</delay></motor>",
        idText(n));
}

/// @p count empty flight configurations, number 1 to @p count.
[[nodiscard]] std::string motorConfigurations(std::size_t count)
{
    std::string elements;
    for (std::size_t i = 0; i < count; ++i)
    {
        elements +=
            std::format("<motorconfiguration configid=\"{}\"/>", idText(static_cast<int>(i) + 1));
    }
    return elements;
}

/// A <simulation> named @p name of the flight configuration with the id @p configId, with
/// stored data.
[[nodiscard]] std::string simulationOf(std::string_view name, std::string_view configId)
{
    return std::format(
        "<simulation status=\"uptodate\"><name>{}</name><conditions><configid>{}"
        "</configid></conditions><flightdata maxaltitude=\"1\"/></simulation>",
        name, configId);
}

/// The names of the simulations of @p document, in order.
[[nodiscard]] Texts simulationNames(const OpenRocketDocument& document)
{
    Texts names;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        names.push_back(simulation->getName());
    }
    return names;
}

/// Which of the flight configurations number 1 to @p count the rocket has, as their numbers
/// separated by blanks.
[[nodiscard]] std::string configurationsOf(const Rocket& rocket, int count)
{
    std::string numbers;
    for (int n = 1; n <= count; ++n)
    {
        if (rocket.containsFlightConfigurationId(idOf(n)))
        {
            numbers += std::format("{}{}", numbers.empty() ? "" : " ", n);
        }
    }
    return numbers;
}

/// Gives @p rocket @p count flight configurations more, each of a new id. (Every one of them
/// updates all there are, whether or not the rocket's events are on: this is what takes the
/// time of the test below.)
void addFlightConfigurations(Rocket& rocket, std::size_t count)
{
    rocket.enableEvents(false);
    for (std::size_t i = 0; i < count; ++i)
    {
        rocket.createFlightConfiguration(FlightConfigurationId{});
    }
    rocket.enableEvents();
}

/// How many flight configurations @p rocket has, the default one not counted.
[[nodiscard]] std::size_t configurationCount(const Rocket& rocket)
{
    return static_cast<std::size_t>(rocket.getFlightConfigurationCount());
}

// The rocket has two flight configurations fewer than the bound when the content of the rocket
// element is read. Its motors make the configurations 1 and 2, the second of which is the
// thousandth. Every later element that would make one more is ignored, each in its own words:
// a motor, a motor configuration, a simulation of an id the rocket lacks and one without an id,
// which is a new one. What names a configuration the rocket has is read as ever.
TEST(LoaderBounds, ARocketTakesFlightConfigurationsUpToTheBoundAndWhatWouldMakeMoreIsIgnored)
{
    RocketLoadFixture fixture;
    Rocket&           rocket = fixture.rocket();
    addFlightConfigurations(rocket, DocumentConfig::kMaxFlightConfigurations - 2);
    ASSERT_EQ(configurationCount(rocket), DocumentConfig::kMaxFlightConfigurations - 2);
    EXPECT_TRUE(DocumentConfig::flightConfigurationCountFits(rocket));

    const HandlerRun rocketRun = fixture.load(
        inStage(std::format("<bodytube><motormount>{}{}{}{}</motormount></bodytube>", motorOf(1),
                            motorOf(2), motorOf(3), motorOf(1))) +
        std::format("<motorconfiguration configid=\"{}\"/>"
                    "<motorconfiguration configid=\"{}\"><name>known</name></motorconfiguration>",
                    idText(4), idText(2)));
    ASSERT_TRUE(rocketRun.result.has_value()) << rocketRun.result.error().message;
    EXPECT_EQ(configurationCount(rocket), DocumentConfig::kMaxFlightConfigurations);
    EXPECT_EQ(configurationsOf(rocket, 5), "1 2");
    const FlightConfiguration& second = rocket.getFlightConfiguration(idOf(2));
    EXPECT_EQ(second.getName(fixture.fixture().preferences()), "known");
    EXPECT_EQ(rocketRun.texts(), (Texts{LoaderWarnings::motorBeyondConfigurationBound(),
                                        LoaderWarnings::motorConfigurationBeyondBound()}));
    EXPECT_FALSE(DocumentConfig::flightConfigurationCountFits(rocket));
    EXPECT_FALSE(DocumentConfig::flightConfigurationCountFits(rocket.getChild(0)))
        << "asked of any component of the tree";

    SimulationsHandler handler(fixture.context());
    const HandlerRun   simulationsRun = runHandler(
        handler,
        std::format("<simulations>{}{}{}{}</simulations>", simulationOf("of the first", idText(1)),
                    simulationOf("of a fifth", idText(5)), simulationOf("of a new one", ""),
                    simulationOf("of the second", idText(2))));
    ASSERT_TRUE(simulationsRun.result.has_value()) << simulationsRun.result.error().message;
    EXPECT_EQ(configurationCount(rocket), DocumentConfig::kMaxFlightConfigurations);
    EXPECT_EQ(configurationsOf(rocket, 5), "1 2");
    EXPECT_EQ(simulationNames(fixture.document()), (Texts{"of the first", "of the second"}));
    EXPECT_EQ(simulationsRun.texts(), Texts{LoaderWarnings::simulationBeyondConfigurationBound()});
}

TEST(LoaderBounds, ATreeWithoutARocketHasNoFlightConfigurationsToCount)
{
    EXPECT_TRUE(DocumentConfig::flightConfigurationCountFits(BodyTube{}));
    const Rocket rocket;
    EXPECT_TRUE(DocumentConfig::flightConfigurationCountFits(rocket));
}

// The component instances over all flight configurations (DocumentConfig::kMaxInstances) hold
// for the simulations too. Before run 9d a simulation of an id the rocket lacks made its flight
// configuration without being asked, which was a way around that bound. Here: a launch lug of
// 10000 instances and eight flight configurations beside the default one are 9 * 10003
// instances, and a ninth would be 100030.
TEST(LoaderBounds, ASimulationMakesNoFlightConfigurationBeyondTheInstanceBudget)
{
    const std::string rocketXml =
        inStage(
            "<bodytube><subcomponents><launchlug><instancecount>10000</instancecount>"
            "</launchlug></subcomponents></bodytube>") +
        motorConfigurations(8);
    const std::string simulations =
        simulationOf("of the eighth", idText(8)) + simulationOf("of a ninth", idText(9));

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, designOf(rocketXml, simulations));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    const Rocket& rocket = loaded->document->getRocket();
    EXPECT_EQ(rocket.getFlightConfigurationCount(), 8);
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 90027U);
    EXPECT_EQ(simulationNames(*loaded->document), Texts{"of the eighth"});
    EXPECT_EQ(warningTexts(loaded->warnings),
              Texts{std::string(LoaderWarnings::kSimulationBeyondInstances)});
}

// ----------------------------------------------------------------------------- simulations

/// @p count simulations without stored data, all of the flight configuration with the id
/// @p configId.
[[nodiscard]] std::string simulationsOfOneConfiguration(std::size_t      count,
                                                        std::string_view configId)
{
    return repeated(std::format("<simulation status=\"notsimulated\"><conditions><configid>{}"
                                "</configid></conditions></simulation>",
                                configId),
                    count);
}

// Ten thousand simulations and one more. The last would fail the load if anything of it were
// read (a branch without a type), and it has an attribute and a text that a simulation is
// warned of. It is ignored with everything in it and one warning, and the <simulations>
// element closes with its own attributes and text (the parent of the handler is told of them).
TEST(LoaderBounds, ADocumentTakesSimulationsUpToTheBoundAndTheNextIsIgnoredWithWhatItHolds)
{
    const std::string oneTooMany =
        "<simulation status=\"uptodate\" extra=\"attribute\"><name>one too many</name>text"
        "<flightdata><databranch name=\"b\" types=\"\"/></flightdata></simulation>";
    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run =
        runHandler(handler, std::format("<simulations own=\"mark\">{}{}</simulations>",
                                        simulationsOfOneConfiguration(
                                            DocumentConfig::kMaxSimulations, kSimulationMotorFcid),
                                        oneTooMany));
    ASSERT_TRUE(run.result.has_value()) << run.result.error().message;
    EXPECT_EQ(fixture.document().getSimulationCount(), DocumentConfig::kMaxSimulations);
    EXPECT_EQ(fixture.document().getSimulation(DocumentConfig::kMaxSimulations - 1)->getName(),
              "Simulation");
    EXPECT_EQ(run.texts(), Texts{LoaderWarnings::tooManySimulations()});
    EXPECT_EQ(run.element, "simulations");
    EXPECT_EQ(run.attributes, (ElementHandler::Attributes{{"own", "mark"}}));
    EXPECT_EQ(run.content, "");
}

// -------------------------------------------------------------------------------- warnings

/// A design with @p count elements the loader does not know, each of a name of its own, so
/// that each gives a warning of its own.
[[nodiscard]] std::string designOfUnknownElements(std::size_t count)
{
    std::string elements;
    for (std::size_t i = 0; i < count; ++i)
    {
        elements += std::format("<u{}/>", i);
    }
    return std::format("<openrocket version=\"1.10\">{}</openrocket>", elements);
}

/// The warning the element number @p index of designOfUnknownElements() gives.
[[nodiscard]] std::string unknownElementWarning(std::size_t index)
{
    return std::format("Unknown element u{}, ignoring.", index);
}

TEST(LoaderBounds, ALoadKeepsWarningsUpToTheBoundAndSaysOnceThatMoreWereDropped)
{
    const std::size_t bound = DocumentConfig::kMaxWarnings;

    const Texts atTheBound = warningsOfLoading(designOfUnknownElements(bound));
    ASSERT_EQ(atTheBound.size(), bound);
    EXPECT_EQ(atTheBound.front(), unknownElementWarning(0));
    EXPECT_EQ(atTheBound.back(), unknownElementWarning(bound - 1));

    const Texts oneMore = warningsOfLoading(designOfUnknownElements(bound + 1));
    ASSERT_EQ(oneMore.size(), bound + 1);
    EXPECT_EQ(oneMore.at(bound - 1), unknownElementWarning(bound - 1));
    EXPECT_EQ(oneMore.back(), LoaderWarnings::tooManyWarnings());

    const Texts manyMore = warningsOfLoading(designOfUnknownElements(5 * bound));
    ASSERT_EQ(manyMore.size(), bound + 1);
    EXPECT_EQ(manyMore.at(bound - 1), unknownElementWarning(bound - 1));
    EXPECT_EQ(manyMore.back(), LoaderWarnings::tooManyWarnings());
}

/// A design with one simulation whose stored flight data has @p count warnings, each of a
/// text of its own.
[[nodiscard]] std::string designOfStoredWarnings(std::size_t count)
{
    std::string warnings;
    for (std::size_t i = 0; i < count; ++i)
    {
        warnings += std::format("<warning>stored {}</warning>", i);
    }
    return designOf(motorConfigurations(1),
                    std::format("<simulation status=\"uptodate\"><conditions><configid>{}"
                                "</configid></conditions><flightdata maxaltitude=\"1\">{}"
                                "</flightdata></simulation>",
                                idText(1), warnings));
}

/// The stored warnings of the one simulation of the design @p xml, then, behind the line
/// "LOAD", the warnings of the load itself; a failure of the load as the one text.
[[nodiscard]] Texts storedWarningsOfLoading(std::string_view xml)
{
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, xml);
    if (!loaded || loaded->document->getSimulationCount() != 1)
    {
        return {"FAILED"};
    }
    const std::shared_ptr<FlightData>& data =
        loaded->document->getSimulation(0)->getSimulatedData();
    if (data == nullptr)
    {
        return {"NO DATA"};
    }
    Texts texts = warningTexts(data->getWarningSet());
    texts.emplace_back("LOAD");
    for (const std::string& text : warningTexts(loaded->warnings))
    {
        texts.push_back(text);
    }
    return texts;
}

// The stored warnings of a flight have a set of their own with the same bound. What says that
// more were dropped is the last warning of that set, a warning of the simulation; the load
// itself has nothing to warn of.
TEST(LoaderBounds, AFlightKeepsStoredWarningsUpToTheBoundAndSaysOnceThatMoreWereDropped)
{
    const std::size_t bound = DocumentConfig::kMaxWarnings;

    const Texts atTheBound = storedWarningsOfLoading(designOfStoredWarnings(bound));
    ASSERT_EQ(atTheBound.size(), bound + 1);
    EXPECT_EQ(atTheBound.at(bound - 1), std::format("stored {}", bound - 1));
    EXPECT_EQ(atTheBound.back(), "LOAD");

    const Texts oneMore = storedWarningsOfLoading(designOfStoredWarnings(bound + 1));
    ASSERT_EQ(oneMore.size(), bound + 2);
    EXPECT_EQ(oneMore.at(bound - 1), std::format("stored {}", bound - 1));
    EXPECT_EQ(oneMore.at(bound), LoaderWarnings::tooManyStoredWarnings());
    EXPECT_EQ(oneMore.back(), "LOAD");

    const Texts manyMore = storedWarningsOfLoading(designOfStoredWarnings(5 * bound));
    ASSERT_EQ(manyMore.size(), bound + 2);
    EXPECT_EQ(manyMore.at(bound), LoaderWarnings::tooManyStoredWarnings());
    EXPECT_EQ(manyMore.back(), "LOAD");
}

/// A design with one simulation whose stored flight has one warning more than the bound, the
/// first with the id @p keptId and the last, which is dropped, with the id @p droppedId, and a
/// branch with a warning event for each of the two.
[[nodiscard]] std::string designOfEventsOfStoredWarnings(std::string_view keptId,
                                                         std::string_view droppedId)
{
    std::string warnings = std::format("<warning><id>{}</id>kept</warning>", keptId);
    for (std::size_t i = 1; i < DocumentConfig::kMaxWarnings; ++i)
    {
        warnings += std::format("<warning>stored {}</warning>", i);
    }
    warnings += std::format("<warning><id>{}</id>dropped</warning>", droppedId);
    const std::string branch = std::format(
        "<databranch name=\"b\" types=\"time\"><datapoint>0</datapoint>"
        "<event time=\"0\" type=\"simwarn\" warnid=\"{}\"/>"
        "<event time=\"0\" type=\"simwarn\" warnid=\"{}\"/></databranch>",
        keptId, droppedId);
    return designOf(motorConfigurations(1),
                    std::format("<simulation status=\"uptodate\"><conditions><configid>{}"
                                "</configid></conditions><flightdata>{}{}</flightdata>"
                                "</simulation>",
                                idText(1), warnings, branch));
}

// A warning event of a stored flight names its warning by the warning's id. One whose warning
// was dropped finds none, like an event that names a warning the file does not have: the event
// is refused with the load's warning for it, and the branch keeps its other events.
TEST(LoaderBounds, AnEventOfADroppedStoredWarningIsRefusedAsOneWithoutAWarning)
{
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded =
        load(environment, designOfEventsOfStoredWarnings("0000000a-0000-0000-0000-000000000001",
                                                         "0000000a-0000-0000-0000-000000000002"));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ASSERT_EQ(loaded->document->getSimulationCount(), 1U);
    const std::shared_ptr<FlightData>& data =
        loaded->document->getSimulation(0)->getSimulatedData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getWarningSet().size(), DocumentConfig::kMaxWarnings + 1);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightDataBranch& branch = data->getBranch(0);
    EXPECT_EQ(branch.getEvents().size(), 1U) << "the event of the warning that is kept";
    EXPECT_EQ(warningTexts(loaded->warnings).size(), 1U) << "the event that was refused";
}

// The set a load returns keeps its bound: whoever adds to it afterwards is held to it too.
TEST(LoaderBounds, TheWarningSetOfALoadKeepsItsBound)
{
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = load(environment, designOfUnknownElements(1));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ASSERT_TRUE(loaded->warnings.capacity().has_value());
    EXPECT_EQ(loaded->warnings.capacity().value_or(0), DocumentConfig::kMaxWarnings);

    // A design without a warning has the bound as well.
    const Result<LoadedDocument> clean = load(environment, designOf(""));
    ASSERT_TRUE(clean.has_value()) << clean.error().message;
    EXPECT_TRUE(clean->warnings.empty());
    EXPECT_EQ(clean->warnings.capacity().value_or(0), DocumentConfig::kMaxWarnings);
}

// ---------------------------------------------------------------------------- measurements

/// The seconds a load of the design @p xml takes; negative when it fails.
[[nodiscard]] double secondsToLoad(std::string_view xml)
{
    LoaderEnvironment            environment;
    const auto                   start  = std::chrono::steady_clock::now();
    const Result<LoadedDocument> loaded = load(environment, xml);
    const double                 seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return loaded.has_value() ? seconds : -1.0;
}

// The documents the reviews of run 9b measured (gen_scale.py of the rocket side's review, gen.py
// of the simulation side's), as whole designs of a number of elements.

/// Body tubes side by side in a stage, each with a length.
[[nodiscard]] std::string designOfTubes(std::size_t count)
{
    return designOf(inStage(repeated("<bodytube><length>0.1</length></bodytube>", count)));
}

/// Mass components side by side in one body tube.
[[nodiscard]] std::string designOfMasses(std::size_t count)
{
    return designOf(inStage(
        std::format("<bodytube><length>1</length><subcomponents>{}</subcomponents></bodytube>",
                    repeated("<masscomponent><mass>0.1</mass></masscomponent>", count))));
}

/// Empty flight configurations in front of a stage of twenty body tubes.
[[nodiscard]] std::string designOfConfigurations(std::size_t count)
{
    return designOf(motorConfigurations(count) +
                    inStage(repeated("<bodytube><length>0.1</length></bodytube>", 20)));
}

/// Motors in one mount, each of a flight configuration of its own.
[[nodiscard]] std::string designOfMotors(std::size_t count)
{
    std::string motors;
    for (std::size_t i = 0; i < count; ++i)
    {
        motors += motorOf(static_cast<int>(i) + 1);
    }
    return designOf(inStage(std::format(
        "<bodytube><length>0.1</length><motormount>{}</motormount></bodytube>", motors)));
}

/// Simulations that each name a flight configuration the rocket lacks.
[[nodiscard]] std::string designOfSimulations(std::size_t count)
{
    std::string simulations;
    for (std::size_t i = 0; i < count; ++i)
    {
        simulations += simulationOf("s", idText(static_cast<int>(i) + 1));
    }
    return designOf("", simulations);
}

/// Simulations without stored data, all of one flight configuration the rocket has.
[[nodiscard]] std::string designOfSimulationsOfOneConfiguration(std::size_t count)
{
    return designOf(motorConfigurations(1), simulationsOfOneConfiguration(count, idText(1)));
}

/// The levels of one wind model, from the highest down (the order in which a level is put in
/// front of all the others).
[[nodiscard]] std::string designOfWindLevels(std::size_t count)
{
    std::string levels;
    for (std::size_t i = 0; i < count; ++i)
    {
        levels += std::format(
            R"(<windlevel altitude="{}" speed="1" direction="1" standarddeviation="0.1"/>)",
            count - i);
    }
    return designOf(
        motorConfigurations(1),
        std::format("<simulation status=\"notsimulated\"><conditions><configid>{}</configid>"
                    "<wind model=\"multilevel\">{}</wind></conditions></simulation>",
                    idText(1), levels));
}

/// Empty stages, one behind the other.
[[nodiscard]] std::string designOfStages(std::size_t count)
{
    return designOf(std::format("<subcomponents>{}</subcomponents>", repeated("<stage/>", count)));
}

/// Stages of one body tube each.
[[nodiscard]] std::string designOfStagesOfOneTube(std::size_t count)
{
    return designOf(
        std::format("<subcomponents>{}</subcomponents>",
                    repeated("<stage><subcomponents><bodytube><length>0.1</length></bodytube>"
                             "</subcomponents></stage>",
                             count)));
}

/// Booster sets on one body tube.
[[nodiscard]] std::string designOfBoosterSets(std::size_t count)
{
    return designOf(inStage(
        std::format("<bodytube><length>1</length><subcomponents>{}</subcomponents></bodytube>",
                    repeated("<parallelstage/>", count))));
}

/// Pod sets on one body tube.
[[nodiscard]] std::string designOfPodSets(std::size_t count)
{
    return designOf(inStage(
        std::format("<bodytube><length>1</length><subcomponents>{}</subcomponents></bodytube>",
                    repeated("<podset/>", count))));
}

/// @p count elements that each set the length of one body tube anew (two lengths in turn: a
/// setter that is given the value it has changes nothing).
[[nodiscard]] std::string settersOfALength(std::size_t count)
{
    return std::format("<bodytube>{}</bodytube>",
                       repeated("<length>0.1</length><length>0.2</length>", count / 2));
}

/// 999 flight configurations in front of a stage of 90 body tubes, and one more tube whose
/// length @p count elements set: every one of them is a change of the rocket that every flight
/// configuration answers.
[[nodiscard]] std::string designOfSettersAmongConfigurations(std::size_t count)
{
    return designOf(motorConfigurations(999) +
                    inStage(repeated("<bodytube><length>0.1</length></bodytube>", 90) +
                            settersOfALength(count)));
}

/// The same without a flight configuration, in a stage of 1000 body tubes.
[[nodiscard]] std::string designOfSettersAmongTubes(std::size_t count)
{
    return designOf(inStage(repeated("<bodytube><length>0.1</length></bodytube>", 1000) +
                            settersOfALength(count)));
}

/// Decals of as many images, in the appearance of one body tube.
[[nodiscard]] std::string designOfDecalImages(std::size_t count)
{
    return designOfDecals(count);
}

/// What makes the design of a measurement for a number of elements.
using Design = std::string (*)(std::size_t);

/// Prints the times of loading the designs @p design makes for @p counts, each as a line
/// "<what> <count>: <seconds> s" as soon as it is taken; a load that fails has a negative time.
void printTimesOf(std::string_view what, Design design, std::initializer_list<std::size_t> counts)
{
    for (const std::size_t count : counts)
    {
        std::cout << std::format("{} {}: {:.2f} s\n", what, count, secondsToLoad(design(count)))
                  << std::flush;
    }
}

// Measurements, not tests: the time of loading the documents that the reviews of run 9b found
// slow, below each bound, at the bound and far beyond it (what is beyond a bound is refused and
// has to cost little). Run them by name in a release build
// (--gtest_also_run_disabled_tests --gtest_filter='LoaderBounds.DISABLED_Prints*'); the two of
// the components take from minutes to half an hour.
//
// The numbers of 2026-10-09: the ci-gcc build (GCC, -O3), whole designs through
// GeneralRocketLoader, on a machine that other builds kept busy (20 threads, a load of 10 to
// 35), so they are high and differ by up to a factor of two between runs. "Before" is what the
// reviews of run 9b measured without the bounds, through the handlers alone.
//   flight configurations in front of twenty tubes
//       250: 1.0 s, 500: 2.7 s, 1000: 20 s, 1600: 16 s, 4000: 11 s      before: 65 s for 1600
//   motors of one mount
//       250: 0.25 s, 500: 1.0 s, 1000: 4.4 s, 1600: 4.3 s, 4000: 4.3 s  before: 10 s for 1600
//   simulations of new configurations
//       500: 0.17 s, 1000: 0.65 s, 2000: 0.73 s, 8000: 0.87 s           before: 6 s for 2000,
//                                                                        140 s for 8000
//   simulations of one configuration
//       5000: 0.12 s, 10000: 0.23 s, 40000: 0.37 s
//   unknown elements, a warning each
//       1000: 0.01 s, 40000: 1.5 s, 80000: 3.0 s                        before: 28 s for 80000
//   stored warnings of one flight
//       1000: 0.03 s, 10000: 0.34 s, 20000: 0.72 s, 40000: 1.5 s        before: 13 s for 40000
//   wind levels from the highest down
//       10000: 0.18 s, 200000: 1.2 s                                     before: 19 s for 200000
//   mass components in one tube
//       1250: 3.3 s, 2500: 25 s, 5000: 59 s, 10000: 301 s, 40000: 191 s
//   body tubes side by side
//       625: 2.4 s, 1250: 12 s, 2500: 65 s, 5000: 264 s, 10000: 1824 s  before: 34 s for 3200
// So every bound ends the growth, and what is beyond one costs little. Up to its bound a
// document costs what it cost before, which is seconds for all of these but the components:
// ten thousand mass components are minutes (the two last numbers of their line are the same
// load, the second on a quieter machine; 149 s on an idle one). Body tubes side by side cost
// more still, because each finds its place behind its siblings by looking for itself among
// them (RocketComponent::setAfter() and getChildPosition(), as in OpenRocket, which took 108 s
// for 1600): twice as many take four to five times as long, nearly the cube, and the 10000 of
// the bound took 1824 s, HALF AN HOUR. The bound on the components does not make a load
// short. What it should be to end such a load in seconds (about a thousand, by these numbers),
// or whether the rocket should answer the changes of a load more cheaply, is an open question
// for the user.
//
// The numbers of 2026-10-10, for what the review of run 9d found (the same build, a machine with
// nothing else to do; "before" is the review's own measurement, without the bound that was
// added for it):
//   empty stages
//       50: 0.00 s, 100: 0.01 s, 500: 0.01 s, 2000: 0.01 s, 10000: 0.02 s
//                                  before: 500: 0.9 s, 1000: 12 s, 2000: 139 to 165 s, 3000: 607 s
//   stages of one tube
//       50: 0.01 s, 100: 0.04 s, 500: 0.04 s, 2000: 0.05 s
//                                  before: 250: 0.23 s, 500: 1.75 s, 2000: 195 s
//   booster sets on one tube
//       50: 0.01 s, 100: 0.04 s, 500: 0.04 s, 1000: 0.04 s
//                                  before: 500: 6 to 8 s, 1000: 61 to 74 s
//   pod sets on one tube
//       50: 0.01 s, 100: 0.03 s, 500: 0.03 s, 1000: 0.03 s
//                                  before: 500: 4 s, 1000: 37 s
//   decals of as many images
//       500: 0.01 s, 1000: 0.03 s, 15000: 0.33 s, 30000: 0.66 s
//                                  before: 3750: 1.5 s, 7500: 6.3 s, 15000: 28 to 43 s
//   unknown elements, a warning each (a warning set compares a hash before it asks equals())
//       40000: 0.04 s, 80000: 0.07 s                  before: 1.5 s and 3.0 s (2026-10-09)
//   stored warnings of one flight
//       20000: 0.02 s, 40000: 0.04 s                  before: 0.72 s and 1.5 s (2026-10-09)
//   elements that set a length, with 999 flight configurations and 91 body tubes
//       none: 18 s, 300: 37 s, 1000: 75 s                           56 ms an element
//   elements that set a length, with 1001 body tubes and no flight configuration
//       none: 1.2 s, 10000: 14 s, 40000: 54 s                       1.3 ms an element
// The two last lines are what no bound ends: the bounds on the components, the instances and
// the flight configurations limit what one change of the rocket costs, and the number of
// changes a file makes is limited by its size alone (see DocumentConfig::kMaxInstances). A
// budget on the work of a whole load is an open question for the user.
TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingBodyTubesSideBySide)
{
    printTimesOf("body tubes side by side", &designOfTubes, {625, 1250, 2500, 5000, 10000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingMassComponentsInOneTube)
{
    printTimesOf("mass components in one tube", &designOfMasses, {1250, 2500, 5000, 10000, 40000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingFlightConfigurations)
{
    printTimesOf("flight configurations", &designOfConfigurations, {250, 500, 1000, 1600, 4000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingMotorsOfOneMount)
{
    printTimesOf("motors of one mount", &designOfMotors, {250, 500, 1000, 1600, 4000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingSimulationsOfNewConfigurations)
{
    printTimesOf("simulations of new configurations", &designOfSimulations,
                 {250, 500, 1000, 2000, 8000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingSimulationsOfOneConfiguration)
{
    printTimesOf("simulations of one configuration", &designOfSimulationsOfOneConfiguration,
                 {5000, 10000, 40000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingUnknownElements)
{
    printTimesOf("unknown elements", &designOfUnknownElements, {500, 1000, 40000, 80000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingStoredWarnings)
{
    printTimesOf("stored warnings", &designOfStoredWarnings, {500, 1000, 10000, 20000, 40000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingWindLevels)
{
    printTimesOf("wind levels from the highest down", &designOfWindLevels, {5000, 10000, 200000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingStages)
{
    printTimesOf("empty stages", &designOfStages, {50, 100, 500, 2000, 10000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingStagesOfOneTube)
{
    printTimesOf("stages of one tube", &designOfStagesOfOneTube, {50, 100, 500, 2000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingBoosterSets)
{
    printTimesOf("booster sets on one tube", &designOfBoosterSets, {50, 100, 500, 1000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingPodSets)
{
    printTimesOf("pod sets on one tube", &designOfPodSets, {50, 100, 500, 1000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingDecalImages)
{
    printTimesOf("decals of as many images", &designOfDecalImages, {500, 1000, 15000, 30000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingSettersAmongFlightConfigurations)
{
    printTimesOf("setters among 999 flight configurations and 91 tubes",
                 &designOfSettersAmongConfigurations, {0, 300, 1000});
}

TEST(LoaderBounds, DISABLED_PrintsTheTimeOfLoadingSettersAmongTubes)
{
    printTimesOf("setters among 1001 tubes", &designOfSettersAmongTubes, {0, 10000, 40000});
}

}  // namespace
