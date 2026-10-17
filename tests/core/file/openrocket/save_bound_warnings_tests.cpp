#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/XmlScanner.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "document/TestAttachments.h"
#include "file/RocketLoaderTestSupport.h"

// The save warnings for the bounds of QtRocket's loader that a saved design exceeds (decision
// O12 of tier 10 and the user's answer of 2026-10-16; OpenRocketSaver's class comment): for each
// bound of the text, a design at the bound saves without a warning and one beyond it with
// exactly one, whose text names the count, and the design beyond the bound, loaded back, shows
// the loader refusing or cutting there, which proves that the saver counts as the loader does.
// The bound on the components is OpenRocketSaverTests' (WarnsOfMoreComponentsThanTheLoaderReads),
// the bounds of the archive are GeneralRocketSaverTests'.

namespace
{

using QtRocket::Config;
using QtRocket::DocumentConfig;
using QtRocket::FlightConfigurationId;
using QtRocket::GeneralRocketLoader;
using QtRocket::InMemoryPreferences;
using QtRocket::LoadedDocument;
using QtRocket::LoaderWarnings;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::WarningSet;
using QtRocket::Test::LoaderEnvironment;

/// A new design (OpenRocketDocumentFactory::createNewRocket(): a rocket with one stage) whose
/// rocket's events are off, so that a test builds it fast, with a preference store for its
/// simulations.
struct Design
{
    /// Declared first, destroyed last: the simulations keep the store.
    InMemoryPreferences                 preferences;
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();

    Design() { document->getRocket().enableEvents(false); }

    [[nodiscard]] QtRocket::Rocket& rocket() const { return document->getRocket(); }
    [[nodiscard]] RocketComponent&  stage() const { return rocket().getChild(0); }

    /// A body tube added to the stage.
    [[nodiscard]] QtRocket::BodyTube& bodyTube() const
    {
        return stage().addChild(std::make_unique<QtRocket::BodyTube>());
    }

    /// A simulation of the rocket added to the document, with @p data as its flight data.
    Simulation& addSimulation(std::shared_ptr<QtRocket::FlightData> data = nullptr)
    {
        const auto simulation = std::make_shared<Simulation>(
            document.get(), rocket(), Simulation::Status::NOT_SIMULATED, "Flight",
            QtRocket::SimulationOptions(preferences),
            std::vector<std::shared_ptr<QtRocket::SimulationExtension>>{}, std::move(data),
            std::map<std::string, QtRocket::PlotAppearance>{}, &preferences);
        document->addSimulation(simulation);
        return *simulation;
    }
};

/// What OpenRocketSaver::save() gave: the text (empty for an Error) and the texts of the
/// warnings.
struct Saved
{
    std::string              text;
    std::vector<std::string> warnings;
};

/// Saves @p design with the document's default options.
[[nodiscard]] Saved saveOf(const Design& design)
{
    WarningSet                warnings;
    QtRocket::ErrorSet        errors;
    const Result<std::string> text =
        OpenRocketSaver::save(*design.document, design.preferences, warnings, errors);
    EXPECT_TRUE(text.has_value()) << text.error().toString();
    Saved saved{.text = text.value_or(""), .warnings = {}};
    for (const QtRocket::Warning& warning : warnings)
    {
        saved.warnings.push_back(warning.toString());
    }
    return saved;
}

/// A design loaded back from a saved text, in an environment of its own.
struct Reloaded
{
    /// Declared first, destroyed last: the document keeps its preference store.
    LoaderEnvironment        environment;
    Result<LoadedDocument>   loaded;
    std::vector<std::string> warnings;

    explicit Reloaded(const std::string& text)
      : loaded(GeneralRocketLoader(environment.context()).load(QtRocket::stringToBytes(text)))
    {
        EXPECT_TRUE(loaded.has_value()) << loaded.error().toString();
        if (loaded.has_value())
        {
            for (const QtRocket::Warning& warning : loaded->warnings)
            {
                warnings.push_back(warning.toString());
            }
        }
    }

    [[nodiscard]] const OpenRocketDocument& document() const { return *loaded->document; }

    /// Whether the load gave the warning @p text.
    [[nodiscard]] bool warned(const std::string& text) const
    {
        return std::ranges::find(warnings, text) != warnings.end();
    }
};

// The texts take their numbers from the constants of the loader's bounds.
TEST(SaveBoundWarnings, NameTheCountAndTheLoadersBound)
{
    EXPECT_EQ(OpenRocketSaver::nestingBeyondLoaderBound(101),
              "The saved rocket has components 101 levels below the rocket; QtRocket loads "
              "components at most 100 levels deep and will ignore the deeper ones, with "
              "everything in them, when it opens this file.");
    EXPECT_EQ(OpenRocketSaver::assembliesBeyondLoaderBound(101),
              "The saved rocket has 101 stages, booster sets and pod sets; QtRocket loads at most "
              "100 and will ignore the ones beyond, with everything in them, when it opens this "
              "file.");
    EXPECT_EQ(OpenRocketSaver::countsBeyondLoaderBound(2, 10002),
              "Counts of instances or of parachute lines in the saved rocket are above what "
              "QtRocket loads: 2 above 10000, the largest 10002. QtRocket refuses such a count "
              "when it opens this file, and the component keeps the count it is made with.");
    EXPECT_EQ(OpenRocketSaver::instancesBeyondLoaderBound(100010),
              "The saved rocket comes to 100010 component instances over its flight "
              "configurations while it is read; QtRocket loads at most 100000 and will ignore the "
              "components, counts, cluster layouts and flight configurations that would go beyond, "
              "with the motors and simulations of those configurations, when it opens this file.");
    EXPECT_EQ(OpenRocketSaver::flightConfigurationsBeyondLoaderBound(1001),
              "The saved design has 1001 flight configurations; QtRocket loads at most 1000 and "
              "will ignore the ones beyond, with their motors and simulations, when it opens this "
              "file.");
    EXPECT_EQ(OpenRocketSaver::simulationsBeyondLoaderBound(10001),
              "The saved design has 10001 simulations; QtRocket loads at most 10000 and will "
              "ignore the ones beyond when it opens this file.");
    EXPECT_EQ(OpenRocketSaver::windLevelsBeyondLoaderBound(10001),
              "A simulation of the saved design has a wind model of 10001 levels; QtRocket loads "
              "at most 10000 levels of a wind model and will ignore the ones beyond when it opens "
              "this file.");
    EXPECT_EQ(OpenRocketSaver::decalImagesBeyondLoaderBound(1001),
              "The saved rocket shows 1001 decal images; QtRocket loads at most 1000 and will "
              "ignore the decals of the images beyond when it opens this file.");
    EXPECT_EQ(OpenRocketSaver::storedWarningsBeyondLoaderBound(1001),
              "A simulation of the saved design stores 1001 warnings with its flight; QtRocket "
              "loads at most 1000 warnings of a flight and will drop the ones beyond when it opens "
              "this file.");
    EXPECT_EQ(OpenRocketSaver::listNestingBeyondLoaderBound(33),
              "A simulation extension of the saved design has lists nested 33 deep in its "
              "configuration; QtRocket loads lists nested at most 32 deep and will ignore the "
              "deeper ones when it opens this file.");
    // The numbers of the texts above.
    EXPECT_EQ(QtRocket::ComponentHandler::kMaxDepth, 100);
    EXPECT_EQ(DocumentConfig::kMaxAssemblies, 100U);
    EXPECT_EQ(DocumentConfig::kMaxCount, 10000);
    EXPECT_EQ(DocumentConfig::kMaxInstances, 100000U);
    EXPECT_EQ(DocumentConfig::kMaxFlightConfigurations, 1000U);
    EXPECT_EQ(DocumentConfig::kMaxSimulations, 10000U);
    EXPECT_EQ(DocumentConfig::kMaxWindLevels, 10000U);
    EXPECT_EQ(DocumentConfig::kMaxDecalImages, 1000U);
    EXPECT_EQ(DocumentConfig::kMaxWarnings, 1000U);
    EXPECT_EQ(QtRocket::ConfigHandler::kMaxListDepth, 32);
}

// ------------------------------------------------------------------------------- the nesting

/// A design whose stage holds a body tube with inner tubes in inner tubes, the deepest at level
/// @p depth (a stage at 1, the body tube at 2).
[[nodiscard]] std::unique_ptr<Design> nestedDesign(int depth)
{
    auto             design = std::make_unique<Design>();
    RocketComponent* parent = &design->bodyTube();
    for (int level = 3; level <= depth; ++level)
    {
        parent = &parent->addChild(std::make_unique<QtRocket::InnerTube>());
    }
    return design;
}

/// The level of the deepest component of @p component's tree below it.
[[nodiscard]] int deepestBelow(const RocketComponent& component)
{
    int deepest = 0;
    for (const RocketComponent* child : component.getChildren())
    {
        deepest = std::max(deepest, 1 + deepestBelow(*child));
    }
    return deepest;
}

// ComponentHandler ignores a component more than kMaxDepth levels below the rocket.
TEST(SaveBoundWarnings, WarnsOfComponentsNestedDeeperThanTheLoaderReads)
{
    constexpr int kBound = QtRocket::ComponentHandler::kMaxDepth;
    EXPECT_TRUE(saveOf(*nestedDesign(kBound)).warnings.empty());

    const Saved beyond = saveOf(*nestedDesign(kBound + 1));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::nestingBeyondLoaderBound(kBound + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(
        reloaded.warned(LoaderWarnings::nestedTooDeeply(QtRocket::InnerTube().getComponentName())));
    EXPECT_EQ(deepestBelow(reloaded.document().getRocket()), kBound);
}

// ---------------------------------------------------------------------------- the assemblies

/// A design of @p stages stages.
[[nodiscard]] std::unique_ptr<Design> designOfStages(std::size_t stages)
{
    auto design = std::make_unique<Design>();
    for (std::size_t i = 1; i < stages; ++i)
    {
        design->rocket().addChild(std::make_unique<QtRocket::AxialStage>());
    }
    return design;
}

// ComponentHandler ignores a stage, a booster set or a pod set beyond kMaxAssemblies.
TEST(SaveBoundWarnings, WarnsOfMoreAssembliesThanTheLoaderReads)
{
    constexpr std::size_t kBound = DocumentConfig::kMaxAssemblies;
    EXPECT_TRUE(saveOf(*designOfStages(kBound)).warnings.empty());

    const Saved beyond = saveOf(*designOfStages(kBound + 1));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::assembliesBeyondLoaderBound(kBound + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(
        LoaderWarnings::tooManyAssemblies(QtRocket::AxialStage().getComponentName())));
    EXPECT_EQ(reloaded.document().getRocket().getChildCount(), kBound);
}

// -------------------------------------------------------------------------------- the counts

/// How often @p text holds @p part.
[[nodiscard]] std::size_t occurrences(std::string_view text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string_view::npos;
         at             = text.find(part, at + part.size()))
    {
        ++count;
    }
    return count;
}

/// The instance counts of the components of the body tube of @p rocket's first stage, and the
/// line count of a parachute, in their order.
[[nodiscard]] std::vector<int> countsOfTheTube(const QtRocket::Rocket& rocket)
{
    std::vector<int> counts;
    for (const RocketComponent* component : rocket.getChild(0).getChild(0).getChildren())
    {
        const auto* const parachute = dynamic_cast<const QtRocket::Parachute*>(component);
        counts.push_back(parachute != nullptr ? parachute->getLineCount()
                                              : component->getInstanceCount());
    }
    return counts;
}

/// A design whose body tube holds a launch lug of @p lugs instances and a parachute of @p lines
/// lines.
[[nodiscard]] std::unique_ptr<Design> designWithCounts(int lugs, int lines)
{
    auto             design = std::make_unique<Design>();
    RocketComponent& tube   = design->bodyTube();
    tube.addChild(std::make_unique<QtRocket::LaunchLug>()).setInstanceCount(lugs);
    tube.addChild(std::make_unique<QtRocket::Parachute>()).setLineCount(lines);
    return design;
}

// IntSetter refuses a count above kMaxCount, and the component keeps the count it is made with.
TEST(SaveBoundWarnings, WarnsOfCountsTheLoaderRefuses)
{
    constexpr int kBound = DocumentConfig::kMaxCount;
    EXPECT_TRUE(saveOf(*designWithCounts(kBound, kBound)).warnings.empty());

    const Saved beyond = saveOf(*designWithCounts(kBound + 1, kBound + 2));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::countsBeyondLoaderBound(2, kBound + 2)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(LoaderWarnings::countAboveBound(kBound)));
    EXPECT_EQ(countsOfTheTube(reloaded.document().getRocket()),
              (std::vector<int>{QtRocket::LaunchLug().getInstanceCount(),
                                QtRocket::Parachute().getLineCount()}));
}

// The counts the saver holds to kMaxCount are those the loader refuses above it: of each kind
// whose setter has that maximum, and the lines of a parachute, a count of kMaxCount + 1 is
// written and comes back as the count the component is made with.
TEST(SaveBoundWarnings, CountsTheCountsOfEveryKindTheLoaderRefuses)
{
    constexpr int    kBeyond = DocumentConfig::kMaxCount + 1;
    const Design     design;
    RocketComponent& tube = design.bodyTube();
    tube.addChild(std::make_unique<QtRocket::LaunchLug>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::RailButton>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::CenteringRing>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::Bulkhead>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::PodSet>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::ParallelStage>()).setInstanceCount(kBeyond);
    tube.addChild(std::make_unique<QtRocket::Parachute>()).setLineCount(kBeyond);
    ASSERT_EQ(countsOfTheTube(design.rocket()), std::vector<int>(7, kBeyond));

    const Saved saved = saveOf(design);
    EXPECT_EQ(saved.warnings,
              std::vector<std::string>{OpenRocketSaver::countsBeyondLoaderBound(7, kBeyond)});
    EXPECT_EQ(occurrences(saved.text, "<instancecount>10001</instancecount>"), 6U);
    EXPECT_EQ(occurrences(saved.text, "<linecount>10001</linecount>"), 1U);
    const Reloaded reloaded(saved.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_EQ(
        countsOfTheTube(reloaded.document().getRocket()),
        (std::vector<int>{
            QtRocket::LaunchLug().getInstanceCount(), QtRocket::RailButton().getInstanceCount(),
            QtRocket::CenteringRing().getInstanceCount(), QtRocket::Bulkhead().getInstanceCount(),
            QtRocket::PodSet().getInstanceCount(), QtRocket::ParallelStage().getInstanceCount(),
            QtRocket::Parachute().getLineCount()}));
}

/// A design of its own whose component of @p kind, made as the loader makes it
/// (DocumentConfig::createComponent()), has the instance count DocumentConfig::kMaxCount + 1,
/// in the first of the body tube, the stage and the rocket that takes it. Null for the rocket
/// and for a component that does not keep that count (it clamps it, or has no count of its own).
[[nodiscard]] std::unique_ptr<Design> designOfACountBeyondTheBound(QtRocket::ComponentKind kind)
{
    std::unique_ptr<RocketComponent> component =
        DocumentConfig::createComponent(QtRocket::xmlName(kind));
    if (component == nullptr)
    {
        return nullptr;
    }
    component->setInstanceCount(DocumentConfig::kMaxCount + 1);
    if (component->getInstanceCount() != DocumentConfig::kMaxCount + 1)
    {
        return nullptr;
    }
    auto             design = std::make_unique<Design>();
    RocketComponent& tube   = design->bodyTube();
    for (RocketComponent* const parent :
         {&tube, &design->stage(), static_cast<RocketComponent*>(&design->rocket())})
    {
        if (parent->isCompatible(*component))
        {
            parent->addChild(std::move(component));
            return design;
        }
    }
    return nullptr;
}

/// The component of @p kind in the tree of @p rocket.
[[nodiscard]] const RocketComponent* componentOfKind(const QtRocket::Rocket& rocket,
                                                     QtRocket::ComponentKind kind)
{
    for (const RocketComponent& component : rocket.subtree(true))
    {
        if (component.kind() == kind)
        {
            return &component;
        }
    }
    return nullptr;
}

/// What the saver and the loader say of an instance count of kMaxCount + 1 for each kind of
/// component that keeps one (designOfACountBeyondTheBound()).
struct CountVerdicts
{
    /// The kinds whose count the loader refuses (the count comes back as the one the component
    /// is made with, and the load warns countAboveBound()).
    std::vector<std::string> refused;
    /// "<kind>: saver <warns>, loader <refuses>" for each kind of which the two disagree.
    std::vector<std::string> disagreements;
};

[[nodiscard]] CountVerdicts countVerdicts()
{
    constexpr int kBeyond = DocumentConfig::kMaxCount + 1;
    CountVerdicts verdicts;
    for (const QtRocket::ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        const std::unique_ptr<Design> design = designOfACountBeyondTheBound(kind);
        if (design == nullptr)
        {
            continue;
        }
        const Saved saved = saveOf(*design);
        const bool  warns =
            saved.warnings ==
            std::vector<std::string>{OpenRocketSaver::countsBeyondLoaderBound(1, kBeyond)};
        const Reloaded               reloaded(saved.text);
        const RocketComponent* const loaded =
            reloaded.loaded.has_value() ? componentOfKind(reloaded.document().getRocket(), kind)
                                        : nullptr;
        const bool refuses = loaded != nullptr && loaded->getInstanceCount() != kBeyond &&
                             reloaded.warned(LoaderWarnings::countAboveBound(kBeyond - 1));
        if (refuses)
        {
            verdicts.refused.emplace_back(QtRocket::className(kind));
        }
        if (warns != refuses || loaded == nullptr)
        {
            verdicts.disagreements.push_back(
                std::format("{}: saver {}, loader {}", QtRocket::className(kind), warns, refuses));
        }
    }
    return verdicts;
}

// The saver warns of an instance count exactly where the loader refuses it: for every kind of
// component that keeps an instance count above kMaxCount, made as the loader makes it, a count
// of kMaxCount + 1 is saved and loaded back. (The saver asks the loader's setter table, see
// OpenRocketSaver; this compares the two ends.)
TEST(SaveBoundWarnings, WarnsOfAnInstanceCountExactlyWhereTheLoaderRefusesIt)
{
    const CountVerdicts verdicts = countVerdicts();
    EXPECT_TRUE(verdicts.disagreements.empty()) << ::testing::PrintToString(verdicts.disagreements);
    EXPECT_EQ(verdicts.refused,
              (std::vector<std::string>{"ParallelStage", "PodSet", "LaunchLug", "RailButton",
                                        "CenteringRing", "Bulkhead"}));
}

// ----------------------------------------------------------------------------- the instances

/// A design of @p configurations flight configurations whose body tube holds a launch lug of
/// @p lugs instances: (3 + @p lugs) * (@p configurations + 1) instances, the rocket, the stage
/// and the body tube one each, the default configuration counted.
[[nodiscard]] std::unique_ptr<Design> designOfInstances(int lugs, int configurations)
{
    auto design = std::make_unique<Design>();
    for (int i = 0; i < configurations; ++i)
    {
        design->rocket().createFlightConfiguration();
    }
    design->bodyTube().addChild(std::make_unique<QtRocket::LaunchLug>()).setInstanceCount(lugs);
    return design;
}

// The loader refuses what would give the rocket more than kMaxInstances instances over all its
// flight configurations: here the count of the launch lug, which keeps the count it is made with.
TEST(SaveBoundWarnings, WarnsOfMoreInstancesThanTheLoaderTakes)
{
    // 10 * (3 + 9997) = 100000.
    EXPECT_TRUE(saveOf(*designOfInstances(9997, 9)).warnings.empty());

    const Saved beyond = saveOf(*designOfInstances(9998, 9));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::instancesBeyondLoaderBound(100010)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(std::string(LoaderWarnings::kCountBeyondInstances)));
    EXPECT_EQ(
        reloaded.document().getRocket().getChild(0).getChild(0).getChild(0).getInstanceCount(), 1);
}

/// designOfInstances(@p lugs, 9) and whether the loader attaches a pod set after the launch
/// lug: what ComponentHandler asks (DocumentConfig::childFits()) of a pod set as the loader makes
/// it (DocumentConfig::createComponent(), 2 instances), with the rocket as the loader has it then.
/// The pod set is added after that, with one instance: 10 * (3 + @p lugs + 2) instances while
/// it is attached, 10 * (3 + @p lugs + 1) once it is read.
[[nodiscard]] std::pair<std::unique_ptr<Design>, bool> designOfALatePodSet(int lugs)
{
    std::unique_ptr<Design>                design = designOfInstances(lugs, 9);
    RocketComponent&                       tube   = design->rocket().getChild(0).getChild(0);
    const std::unique_ptr<RocketComponent> made   = DocumentConfig::createComponent("podset");
    const bool                             fits   = DocumentConfig::childFits(tube, *made);
    tube.addChild(std::make_unique<QtRocket::PodSet>()).setInstanceCount(1);
    return {std::move(design), fits};
}

// The loader attaches a component with the count its constructor gives it (a pod set 2) and
// sets the component's count after that: a pod set of one instance that brings the rocket to
// exactly kMaxInstances takes it beyond while it is attached, and the loader refuses it. One
// that takes the rocket to exactly kMaxInstances while it is attached is attached. (The round
// trip of that design takes seconds, every change of the rocket after the pod set rebuilding
// 100000 instances: the loader's rule is asked of the rocket as the loader has it then.)
TEST(SaveBoundWarnings, WarnsOfAComponentBeyondTheInstancesWhileItIsAttached)
{
    // 10 * (3 + 9995 + 2) = 100000 while the pod set is attached: the bound.
    const auto [atTheBound, attached] = designOfALatePodSet(9995);
    EXPECT_TRUE(attached);
    EXPECT_TRUE(saveOf(*atTheBound).warnings.empty());

    // 10 * (3 + 9996 + 1) = 100000, and 10 * (3 + 9996 + 2) while the pod set is attached.
    const auto [design, fits] = designOfALatePodSet(9996);
    EXPECT_FALSE(fits);
    ASSERT_EQ(DocumentConfig::instanceLoad(design->rocket()), DocumentConfig::kMaxInstances);

    const Saved saved = saveOf(*design);
    EXPECT_EQ(saved.warnings,
              std::vector<std::string>{OpenRocketSaver::instancesBeyondLoaderBound(100010)});
    const Reloaded reloaded(saved.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(
        reloaded.warned(LoaderWarnings::tooManyInstances(QtRocket::PodSet().getComponentName())));
    EXPECT_EQ(reloaded.document().getRocket().getChild(0).getChild(0).getChildCount(), 1U);
}

/// Gives @p design a simulation of a flight configuration the rocket no longer has.
void addSimulationOfARemovedConfiguration(Design& design)
{
    const FlightConfigurationId removed;
    Simulation&                 simulation = design.addSimulation();
    simulation.setFlightConfigurationId(removed);
    design.rocket().removeFlightConfiguration(removed);
    ASSERT_FALSE(design.rocket().containsFlightConfigurationId(removed));
}

/// designOfInstances(9997, 8) with @p simulations simulations of flight configurations the rocket
/// lacks: (3 + 9997) * (8 + 1 + @p simulations) instances once they are loaded. (The simulations
/// are made before the launch lug and the flight configurations of the rocket: a simulation of a
/// configuration the rocket lacks makes and removes that configuration while it is set up here,
/// which costs time in proportion to the instances of every configuration.)
[[nodiscard]] std::unique_ptr<Design> designOfSimulatedConfigurations(int simulations)
{
    auto design = std::make_unique<Design>();
    for (int i = 0; i < simulations; ++i)
    {
        addSimulationOfARemovedConfiguration(*design);
    }
    for (int i = 0; i < 8; ++i)
    {
        design->rocket().createFlightConfiguration();
    }
    design->bodyTube().addChild(std::make_unique<QtRocket::LaunchLug>()).setInstanceCount(9997);
    return design;
}

// A simulation of a flight configuration the rocket lacks makes the configuration when it is
// loaded, after the components: (3 + 9997) * (8 + 1 + 1) instances are the bound, and one more
// such simulation takes the rocket beyond it. (The round trip is
// DISABLED_TheLoaderIgnoresTheSimulationOfAConfigurationBeyondTheInstances.)
TEST(SaveBoundWarnings, CountsTheInstancesOfTheConfigurationsTheSimulationsMake)
{
    EXPECT_TRUE(saveOf(*designOfSimulatedConfigurations(1)).warnings.empty());
    EXPECT_EQ(saveOf(*designOfSimulatedConfigurations(2)).warnings,
              std::vector<std::string>{OpenRocketSaver::instancesBeyondLoaderBound(110000)});
}

// The round trip of CountsTheInstancesOfTheConfigurationsTheSimulationsMake: the loader ignores
// the second simulation. (1 to 2.5 s in a debug build and half a minute under the address
// sanitizer: every change of the rocket while it is read rebuilds 90000 instances. Run by hand
// with --gtest_also_run_disabled_tests; LoaderBounds tests the loader's side with a file of its
// own, and WarnsOfMoreInstancesThanTheLoaderTakes makes the round trip of the bound.)
TEST(SaveBoundWarnings, DISABLED_TheLoaderIgnoresTheSimulationOfAConfigurationBeyondTheInstances)
{
    const Reloaded reloaded(saveOf(*designOfSimulatedConfigurations(2)).text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(std::string(LoaderWarnings::kSimulationBeyondInstances)));
    EXPECT_EQ(reloaded.document().getSimulationCount(), 1U);
}

// ------------------------------------------------------------------ the flight configurations

/// A design of @p own flight configurations and, after them, @p ofSimulations simulations of
/// flight configurations the rocket lacks; nothing in its stage.
[[nodiscard]] std::unique_ptr<Design> designOfConfigurations(int own, int ofSimulations)
{
    auto design = std::make_unique<Design>();
    for (int i = 0; i < own; ++i)
    {
        design->rocket().createFlightConfiguration();
    }
    for (int i = 0; i < ofSimulations; ++i)
    {
        addSimulationOfARemovedConfiguration(*design);
    }
    return design;
}

// The loader makes a flight configuration for each <motorconfiguration> of the rocket and for
// each simulation of a configuration the rocket lacks (SingleSimulationHandler), and ignores
// what would make one beyond kMaxFlightConfigurations: two of the rocket and 998 of simulations
// are the bound. (The simulations make the test cheap: every configuration of a rocket costs
// time in proportion to the configurations it has. The round trip of a rocket of 1001 is
// DISABLED_TheLoaderIgnoresAFlightConfigurationBeyondTheBound; LoaderBounds tests the loader's
// side with a file of its own.)
TEST(SaveBoundWarnings, WarnsOfMoreFlightConfigurationsThanTheLoaderReads)
{
    constexpr int kBound = static_cast<int>(DocumentConfig::kMaxFlightConfigurations);
    const std::unique_ptr<Design> design = designOfConfigurations(2, kBound - 2);
    const Saved                   saved  = saveOf(*design);
    EXPECT_TRUE(saved.warnings.empty());
    EXPECT_EQ(occurrences(saved.text, "<motorconfiguration "), 2U);

    addSimulationOfARemovedConfiguration(*design);
    EXPECT_EQ(OpenRocketSaver::beyondLoaderBounds(*design->document),
              std::vector<std::string>{
                  OpenRocketSaver::flightConfigurationsBeyondLoaderBound(kBound + 1)});
}

// The round trip of a rocket of kMaxFlightConfigurations + 1 flight configurations: the loader
// ignores the last <motorconfiguration>. (4 to 6 s in a debug build, too long for every run of
// the suite: run by hand with --gtest_also_run_disabled_tests.)
TEST(SaveBoundWarnings, DISABLED_TheLoaderIgnoresAFlightConfigurationBeyondTheBound)
{
    constexpr int kBound = static_cast<int>(DocumentConfig::kMaxFlightConfigurations);
    const Saved   beyond = saveOf(*designOfConfigurations(kBound + 1, 0));
    EXPECT_EQ(beyond.warnings, std::vector<std::string>{
                                   OpenRocketSaver::flightConfigurationsBeyondLoaderBound(1001)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(LoaderWarnings::motorConfigurationBeyondBound()));
    EXPECT_EQ(reloaded.document().getRocket().getFlightConfigurationCount(), kBound);
}

// ------------------------------------------------------------------------------ the simulations

// SimulationsHandler ignores a simulation beyond kMaxSimulations. The function save() takes its
// warnings from is asked here: the text of ten thousand simulations is 40 MB. (The loader asks
// the document's getSimulationCount() before it reads each <simulation>, as the saver does.)
TEST(SaveBoundWarnings, WarnsOfMoreSimulationsThanTheLoaderReads)
{
    Design design;
    for (std::size_t i = 0; i < DocumentConfig::kMaxSimulations; ++i)
    {
        design.addSimulation();
    }
    EXPECT_TRUE(OpenRocketSaver::beyondLoaderBounds(*design.document).empty());

    design.addSimulation();
    EXPECT_EQ(OpenRocketSaver::beyondLoaderBounds(*design.document),
              std::vector<std::string>{OpenRocketSaver::simulationsBeyondLoaderBound(
                  DocumentConfig::kMaxSimulations + 1)});
}

// The round trip of kMaxSimulations + 1 simulations: the save warns, and the loader ignores the
// last simulation. (A text of 40 MB, 4 to 5 s in a debug build: run by hand with
// --gtest_also_run_disabled_tests. LoaderBounds tests the loader's side with a file of its own.)
TEST(SaveBoundWarnings, DISABLED_TheLoaderIgnoresASimulationBeyondTheBound)
{
    Design design;
    for (std::size_t i = 0; i <= DocumentConfig::kMaxSimulations; ++i)
    {
        design.addSimulation();
    }
    const Saved beyond = saveOf(design);
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::simulationsBeyondLoaderBound(
                  DocumentConfig::kMaxSimulations + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(LoaderWarnings::tooManySimulations()));
    EXPECT_EQ(reloaded.document().getSimulationCount(), DocumentConfig::kMaxSimulations);
}

// --------------------------------------------------------------------------- the wind levels

/// A design of one simulation whose multi-level wind model has @p levels levels, at 0 m, 1 m, ...
[[nodiscard]] std::unique_ptr<Design> designOfWindLevels(std::size_t levels)
{
    auto                                    design = std::make_unique<Design>();
    QtRocket::MultiLevelPinkNoiseWindModel& wind =
        design->addSimulation().getOptions().getMultiLevelWindModel();
    wind.clearLevels();
    for (std::size_t i = 0; i < levels; ++i)
    {
        EXPECT_TRUE(wind.addWindLevel(static_cast<double>(i), 5.0, 0.5, 0.1).has_value());
    }
    return design;
}

// WindHandler ignores a level beyond kMaxWindLevels of one model, and passes over, without
// counting it, a level whose numbers lie outside the ranges of a simulation: here one of 2000
// m/s (kWindSpeed), which the loaded design does not have either.
TEST(SaveBoundWarnings, WarnsOfAWindModelOfMoreLevelsThanTheLoaderReads)
{
    constexpr std::size_t                   kBound = DocumentConfig::kMaxWindLevels;
    const std::unique_ptr<Design>           design = designOfWindLevels(kBound);
    QtRocket::MultiLevelPinkNoiseWindModel& wind =
        design->document->getSimulation(0)->getOptions().getMultiLevelWindModel();
    ASSERT_TRUE(wind.addWindLevel(1.0e5, 2000.0, 0.0, 0.0).has_value());
    EXPECT_TRUE(OpenRocketSaver::beyondLoaderBounds(*design->document).empty());

    ASSERT_TRUE(wind.addWindLevel(2.0e4, 5.0, 0.5, 0.1).has_value());
    const Saved beyond = saveOf(*design);
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::windLevelsBeyondLoaderBound(kBound + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(LoaderWarnings::tooManyWindLevels()));
    EXPECT_TRUE(reloaded.warned(std::string(LoaderWarnings::kWindLevelOutOfRange)));
    ASSERT_EQ(reloaded.document().getSimulationCount(), 1U);
    EXPECT_EQ(reloaded.document()
                  .getSimulation(0)
                  ->getOptions()
                  .getMultiLevelWindModel()
                  .getLevels()
                  .size(),
              kBound);
}

// ------------------------------------------------------------------------------ the decals

/// An appearance with a decal of the image @p name.
[[nodiscard]] QtRocket::Appearance appearanceOf(std::string name)
{
    return {QtRocket::Color(10, 20, 30), 0.5,
            QtRocket::Decal(QtRocket::Coordinate(0, 0), QtRocket::Coordinate(0, 0),
                            QtRocket::Coordinate(1, 1), 0.0, std::move(name),
                            QtRocket::Decal::EdgeMode::REPEAT)};
}

/// A design whose body tube holds inner tubes that show @p images decal images ("decals/<i>.png",
/// each of the bytes "<i>"), two each, on their outside and on their inside.
[[nodiscard]] std::unique_ptr<Design> designOfDecalImages(std::size_t images)
{
    auto                 design = std::make_unique<Design>();
    RocketComponent&     tube   = design->bodyTube();
    QtRocket::InnerTube* inner  = nullptr;
    for (std::size_t i = 0; i < images; ++i)
    {
        const std::string name = std::format("decals/{}.png", i);
        std::ignore            = design->document->getDecalImage(
            std::make_shared<QtRocket::Test::MemoryAttachment>(name, std::to_string(i)));
        if (i % 2 == 0)
        {
            inner = &tube.addChild(std::make_unique<QtRocket::InnerTube>());
            inner->setAppearance(appearanceOf(name));
        }
        else
        {
            inner->getInsideColorComponentHandler().setInsideAppearance(appearanceOf(name));
        }
    }
    return design;
}

/// The archive GeneralRocketSaver makes of @p design, and the texts of its warnings.
struct SavedArchive
{
    std::vector<std::byte>   bytes;
    std::vector<std::string> warnings;
};

[[nodiscard]] SavedArchive archiveOf(const Design& design)
{
    QtRocket::GeneralRocketSaver         saver(design.preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    EXPECT_TRUE(bytes.has_value()) << bytes.error().toString();
    SavedArchive saved{.bytes = bytes.value_or(std::vector<std::byte>{}), .warnings = {}};
    for (const QtRocket::Warning& warning : saver.getWarnings())
    {
        saved.warnings.push_back(warning.toString());
    }
    return saved;
}

// AppearanceHandler ignores a decal whose image would be one more than kMaxDecalImages. The
// loader registers one image per name of an entry of the archive, which is the name rocket.ork
// gives the image: two names that rocket.ork writes as one (the control character is left out)
// are one image. (The round trip is DISABLED_TheLoaderIgnoresADecalImageBeyondTheBound;
// LoaderDecalBound tests the loader's side with a file of its own.)
TEST(SaveBoundWarnings, WarnsOfMoreDecalImagesThanTheLoaderReads)
{
    constexpr std::size_t         kBound     = DocumentConfig::kMaxDecalImages;
    const std::unique_ptr<Design> atTheBound = designOfDecalImages(kBound);
    EXPECT_TRUE(OpenRocketSaver::beyondLoaderBounds(*atTheBound->document).empty());
    atTheBound->rocket().setAppearance(
        appearanceOf("decals/\x01"
                     "0.png"));
    EXPECT_TRUE(OpenRocketSaver::beyondLoaderBounds(*atTheBound->document).empty());

    EXPECT_EQ(archiveOf(*designOfDecalImages(kBound + 1)).warnings,
              std::vector<std::string>{OpenRocketSaver::decalImagesBeyondLoaderBound(kBound + 1)});
}

// The round trip of a design of kMaxDecalImages + 1 decal images, loaded from its archive (a
// plain design would name files): the loader ignores the decal of the last image. (4 to 5 s in
// a debug build, for the 501 inner tubes: run by hand with --gtest_also_run_disabled_tests.)
TEST(SaveBoundWarnings, DISABLED_TheLoaderIgnoresADecalImageBeyondTheBound)
{
    constexpr std::size_t        kBound = DocumentConfig::kMaxDecalImages;
    const SavedArchive           saved  = archiveOf(*designOfDecalImages(kBound + 1));
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded =
        GeneralRocketLoader(environment.context()).load(saved.bytes);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_TRUE(loaded->warnings.contains(
        QtRocket::Warning::fromString(LoaderWarnings::tooManyDecalImages())));
    EXPECT_EQ(loaded->document->getDecalImageCount(), kBound);
}

// ---------------------------------------------------------------------- the stored warnings

/// A design of one simulation whose flight data store @p warnings warnings ("Warning <i>").
[[nodiscard]] std::unique_ptr<Design> designOfStoredWarnings(std::size_t warnings)
{
    auto             design = std::make_unique<Design>();
    constexpr double kNaN   = std::numeric_limits<double>::quiet_NaN();
    const auto data = std::make_shared<QtRocket::FlightData>(100.0, 50.0, kNaN, kNaN, 5.0, 20.0,
                                                             kNaN, kNaN, kNaN, kNaN);
    for (std::size_t i = 0; i < warnings; ++i)
    {
        data->getWarningSet().add(std::format("Warning {}", i));
    }
    design->addSimulation(data);
    return design;
}

// FlightDataHandler keeps kMaxWarnings stored warnings of a flight and drops the others; the
// message it adds in their place is not counted, also when a saved flight holds it.
TEST(SaveBoundWarnings, WarnsOfAFlightThatStoresMoreWarningsThanTheLoaderReads)
{
    constexpr std::size_t         kBound     = DocumentConfig::kMaxWarnings;
    const std::unique_ptr<Design> atTheBound = designOfStoredWarnings(kBound);
    EXPECT_TRUE(saveOf(*atTheBound).warnings.empty());
    atTheBound->document->getSimulation(0)->getSimulatedData()->getWarningSet().add(
        LoaderWarnings::tooManyStoredWarnings());
    EXPECT_TRUE(saveOf(*atTheBound).warnings.empty());

    const Saved beyond = saveOf(*designOfStoredWarnings(kBound + 1));
    EXPECT_EQ(beyond.warnings, std::vector<std::string>{
                                   OpenRocketSaver::storedWarningsBeyondLoaderBound(kBound + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    ASSERT_EQ(reloaded.document().getSimulationCount(), 1U);
    const WarningSet& stored =
        reloaded.document().getSimulation(0)->getSimulatedData()->getWarningSet();
    EXPECT_EQ(stored.size(), kBound + 1);
    EXPECT_TRUE(
        stored.contains(QtRocket::Warning::fromString(LoaderWarnings::tooManyStoredWarnings())));
}

// The saver counts the stored warnings as the loader reads them back: two whose descriptions
// differ only in their line ends (the XML parser hands "\r\n" and "\r" on as "\n") or in white
// space at their ends (WarningHandler trims a description) are one. kMaxWarnings - 1 warnings and
// four such texts are the bound, and the flight comes back with all of them; one more warning is
// beyond it.
TEST(SaveBoundWarnings, CountsTheStoredWarningsAsTheLoaderReadsThemBack)
{
    constexpr std::size_t         kBound = DocumentConfig::kMaxWarnings;
    const std::unique_ptr<Design> design = designOfStoredWarnings(kBound - 1);
    WarningSet& stored = design->document->getSimulation(0)->getSimulatedData()->getWarningSet();
    stored.add("One line\nand another");
    stored.add("One line\r\nand another");
    stored.add("One line\rand another");
    stored.add(" One line\nand another\t");
    ASSERT_EQ(stored.size(), kBound + 3);
    const Saved atTheBound = saveOf(*design);
    EXPECT_TRUE(atTheBound.warnings.empty());
    const Reloaded reloaded(atTheBound.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    ASSERT_EQ(reloaded.document().getSimulationCount(), 1U);
    const WarningSet& read =
        reloaded.document().getSimulation(0)->getSimulatedData()->getWarningSet();
    EXPECT_EQ(read.size(), kBound);
    EXPECT_TRUE(read.contains(QtRocket::Warning::fromString("One line\nand another")));
    EXPECT_FALSE(
        read.contains(QtRocket::Warning::fromString(LoaderWarnings::tooManyStoredWarnings())));

    stored.add(std::format("Warning {}", kBound - 1));
    EXPECT_EQ(
        saveOf(*design).warnings,
        std::vector<std::string>{OpenRocketSaver::storedWarningsBeyondLoaderBound(kBound + 1)});
}

// ---------------------------------------------------------------------- the nested lists

/// A design of one simulation with an extension whose configuration has a list nested @p depth
/// deep under the key "list": a list of a list of ... of the number 1.
[[nodiscard]] std::unique_ptr<Design> designOfNestedLists(std::size_t depth)
{
    auto          design = std::make_unique<Design>();
    Config::Value value(std::int32_t{1});
    for (std::size_t i = 0; i < depth; ++i)
    {
        value = Config::Value(Config::List{value});
    }
    Config config;
    config.put("list", value);
    design->addSimulation().getSimulationExtensions().push_back(
        std::make_shared<QtRocket::UnknownSimulationExtension>("x.y.Lists", config));
    return design;
}

// ConfigHandler reads lists nested at most kMaxListDepth deep and ignores a deeper one.
TEST(SaveBoundWarnings, WarnsOfListsNestedDeeperThanTheLoaderReads)
{
    constexpr int kBound = QtRocket::ConfigHandler::kMaxListDepth;
    EXPECT_TRUE(saveOf(*designOfNestedLists(kBound)).warnings.empty());

    const Saved beyond = saveOf(*designOfNestedLists(kBound + 1));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::listNestingBeyondLoaderBound(kBound + 1)});
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_TRUE(reloaded.warned(std::string(LoaderWarnings::kListNestedTooDeeply)));
}

// ------------------------------------------------------- the landing dispersion settings

using DispersionAttributes = QtRocket::LandingDispersionSettings::Attributes;

/// The attributes "runs" and "seed", then @p others attributes "a0", "a1", ... of the value "1".
[[nodiscard]] DispersionAttributes settingsAttributes(std::size_t others)
{
    DispersionAttributes attributes{{"runs", "500"}, {"seed", "12345"}};
    for (std::size_t i = 0; i < others; ++i)
    {
        attributes.emplace(std::format("a{}", i), "1");
    }
    return attributes;
}

/// The attributes "parameter", "distribution" and "spread" of an uncertainty, then @p others
/// attributes "a0", "a1", ... of the value "1".
[[nodiscard]] DispersionAttributes uncertaintyAttributes(std::size_t others)
{
    DispersionAttributes attributes{
        {"parameter", "windspeed"}, {"distribution", "normal"}, {"spread", "0.5"}};
    for (std::size_t i = 0; i < others; ++i)
    {
        attributes.emplace(std::format("a{}", i), "1");
    }
    return attributes;
}

/// A design of one simulation with landing dispersion settings of the attributes @p attributes
/// and of one uncertainty of the attributes @p uncertainty.
[[nodiscard]] std::unique_ptr<Design> designOfDispersion(DispersionAttributes attributes,
                                                         DispersionAttributes uncertainty)
{
    auto design = std::make_unique<Design>();
    design->addSimulation().setLandingDispersionSettings(
        QtRocket::LandingDispersionSettings(std::move(attributes), {std::move(uncertainty)}));
    return design;
}

/// The landing dispersion settings of the one simulation of a loaded design; none when it has
/// none, or not one simulation.
[[nodiscard]] std::optional<QtRocket::LandingDispersionSettings> dispersionOf(
    const OpenRocketDocument& document)
{
    if (document.getSimulationCount() != 1)
    {
        return std::nullopt;
    }
    return document.getSimulation(0)->getLandingDispersionSettings();
}

/// The <landingdispersion> element of the saved text @p text, its children included; empty when
/// it has none.
[[nodiscard]] std::string_view dispersionElementOf(std::string_view text)
{
    constexpr std::string_view kEnd   = "</landingdispersion>";
    const std::size_t          start  = text.find("<landingdispersion");
    const std::size_t          finish = text.find(kEnd, start);
    return start == std::string_view::npos || finish == std::string_view::npos
               ? std::string_view{}
               : text.substr(start, finish + kEnd.size() - start);
}

// The landing dispersion settings are the one place where the design names attributes. An
// element may have kMaxAttributes of them, the most the loader's XML scanner reads; the first
// ones written ("runs" and "seed", "parameter", "distribution" and "spread") count first. An
// attribute beyond them, which would make the loader refuse the whole file, is left out with
// the warning dispersionAttributesLeftOut(): of the element and of an uncertainty the one
// written last ("a9998" and "a9997", the others being written by name), so that the design
// beyond the bound is written as the design at the bound. That file loads.
TEST(SaveBoundWarnings, WritesTheMostAttributesOfLandingDispersionSettingsTheLoaderReads)
{
    constexpr std::size_t         kBound = QtRocket::XmlScanner::kMaxAttributes;
    const std::unique_ptr<Design> atTheBound =
        designOfDispersion(settingsAttributes(kBound - 2), uncertaintyAttributes(kBound - 3));
    const Saved kept = saveOf(*atTheBound);
    EXPECT_TRUE(kept.warnings.empty());
    ASSERT_FALSE(dispersionElementOf(kept.text).empty());

    const Saved beyond = saveOf(
        *designOfDispersion(settingsAttributes(kBound - 1), uncertaintyAttributes(kBound - 2)));
    EXPECT_EQ(beyond.warnings,
              std::vector<std::string>{OpenRocketSaver::dispersionAttributesLeftOut(2)});
    EXPECT_EQ(dispersionElementOf(beyond.text), dispersionElementOf(kept.text));
    const Reloaded reloaded(beyond.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    EXPECT_EQ(dispersionOf(reloaded.document()),
              atTheBound->document->getSimulation(0)->getLandingDispersionSettings());
}

// An attribute whose name the loader cannot read back is left out with the warning
// dispersionAttributesLeftOut(), which counts them all: a name longer than kMaxNameLength, one
// with a colon (a namespace prefix), "xmlns" (a namespace declaration), one that is no name of
// XML 1.0 (U+00D7, and U+2070, a name character of XML 1.1 only) and one that is not UTF-8.
// The file loads, with the others.
TEST(SaveBoundWarnings, LeavesOutTheAttributesOfLandingDispersionSettingsTheLoaderCannotRead)
{
    const std::string    longest(QtRocket::XmlScanner::kMaxNameLength, 'n');
    DispersionAttributes attributes = settingsAttributes(0);
    attributes.emplace(longest, "1");
    attributes.emplace(longest + "n", "1");
    attributes.emplace("a:b", "1");
    attributes.emplace("xmlns", "1");
    attributes.emplace("\xC3\x97", "1");
    attributes.emplace("x\xFF", "1");
    DispersionAttributes uncertainty = uncertaintyAttributes(0);
    uncertainty.emplace("\xE2\x81\xB0", "1");
    const Saved saved = saveOf(*designOfDispersion(attributes, uncertainty));
    EXPECT_EQ(saved.warnings,
              std::vector<std::string>{OpenRocketSaver::dispersionAttributesLeftOut(6)});
    const Reloaded reloaded(saved.text);
    ASSERT_TRUE(reloaded.loaded.has_value());
    DispersionAttributes kept = settingsAttributes(0);
    kept.emplace(longest, "1");
    EXPECT_EQ(dispersionOf(reloaded.document()),
              QtRocket::LandingDispersionSettings(kept, {uncertaintyAttributes(0)}));
}

/// @p text, a saved design whose <landingdispersion> has the attribute x="1", as a design file of
/// XML 1.1 in which that attribute is named U+2070, a name character of XML 1.1 only; empty
/// when the text is not of that form.
[[nodiscard]] std::string asXml11WithAWiderName(std::string text)
{
    constexpr std::string_view kDeclaration = "<?xml version='1.0'";
    const std::size_t          element      = text.find("<landingdispersion ");
    const std::size_t          attribute    = text.find(R"( x="1")", element);
    if (!text.starts_with(kDeclaration) || element == std::string::npos ||
        attribute == std::string::npos)
    {
        return {};
    }
    text.replace(attribute, std::string_view(R"( x="1")").size(), " \xE2\x81\xB0=\"1\"");
    text.replace(0, kDeclaration.size(), "<?xml version='1.1'");
    return text;
}

// A design file of XML 1.1, whose names are wider, can give landing dispersion settings an
// attribute that XML 1.0 cannot name, and the loader keeps it. The saver, which writes XML 1.0,
// leaves it out with the warning dispersionAttributesLeftOut(), so that the file it writes
// loads.
TEST(SaveBoundWarnings, LeavesOutAnAttributeOfAnXml11FileThatXml10CannotName)
{
    DispersionAttributes attributes = settingsAttributes(0);
    attributes.emplace("x", "1");
    const Saved saved = saveOf(*designOfDispersion(attributes, uncertaintyAttributes(0)));
    ASSERT_TRUE(saved.warnings.empty());
    const std::string xml11 = asXml11WithAWiderName(saved.text);
    ASSERT_FALSE(xml11.empty());
    Reloaded loaded(xml11);
    ASSERT_TRUE(loaded.loaded.has_value());
    DispersionAttributes wider = settingsAttributes(0);
    wider.emplace("\xE2\x81\xB0", "1");
    EXPECT_EQ(dispersionOf(loaded.document()),
              QtRocket::LandingDispersionSettings(wider, {uncertaintyAttributes(0)}));

    WarningSet                warnings;
    QtRocket::ErrorSet        errors;
    const Result<std::string> resaved = OpenRocketSaver::save(
        *loaded.loaded->document, loaded.environment.preferences(), warnings, errors);
    ASSERT_TRUE(resaved.has_value()) << resaved.error().toString();
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.contains(
        QtRocket::Warning::fromString(OpenRocketSaver::dispersionAttributesLeftOut(1))));
    const Reloaded again(*resaved);
    ASSERT_TRUE(again.loaded.has_value());
    EXPECT_EQ(
        dispersionOf(again.document()),
        QtRocket::LandingDispersionSettings(settingsAttributes(0), {uncertaintyAttributes(0)}));
}

}  // namespace
