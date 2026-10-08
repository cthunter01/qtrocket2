#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The bound on what a file can make a rocket hold (DocumentConfig::kMaxInstances), which the
// review of the loader's rocket side asked for: decision L6 bounds every count of a file by
// itself, but the instance counts of nested components multiply, the ones of components side
// by side add up, and every flight configuration keeps the instances of the whole rocket, so
// that a file of a few hundred bytes ended a load in std::bad_alloc (a pod set of 10000
// instances with a launch lug of 10000; inner tubes in a "9-grid" cluster nested nine deep,
// with no number above 9 in the file; 800 empty flight configurations beside one launch lug of
// 10000). OpenRocket has no bound and runs out of memory, so nothing here has a Java original.
//
// The cases that OpenRocket can still read are in the case tables of the handlers, with its
// answer beside QtRocket's (ComponentHandlerTests.cpp, MotorConfigurationHandlerTests.cpp,
// MotorMountHandlerTests.cpp: the cases named "...-fix-..."). Here are the rule itself and the
// documents of the review, which OpenRocket cannot read.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ClusterConfiguration;
using QtRocket::DocumentConfig;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::LaunchLug;
using QtRocket::PodSet;
using QtRocket::Rocket;
using QtRocket::TrapezoidFinSet;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::whatUsingTheRocketThrows;

constexpr std::uint64_t kSaturated = std::numeric_limits<std::uint64_t>::max();

// ------------------------------------------------------------------------------- the rule

TEST(InstanceBudget, TheBoundIsTenTimesTheBoundOfOneCount)
{
    EXPECT_EQ(DocumentConfig::kMaxInstances, 100000U);
    EXPECT_EQ(DocumentConfig::kMaxInstances,
              10U * static_cast<std::uint64_t>(DocumentConfig::kMaxCount));
}

TEST(InstanceBudget, TheLoadIsEveryInstanceOfEveryComponentTimesTheFlightConfigurations)
{
    Rocket rocket;
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 1U) << "the rocket itself";

    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   tube  = stage.addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 3U);
    EXPECT_EQ(DocumentConfig::instanceLoad(tube), 3U) << "asked of any component of the tree";

    // A pod set of 2 pods, each with a tube that has a launch lug of 3 instances and a fin set
    // of 3 fins: 2 + 2 + 6 + 6.
    PodSet&    pods    = tube.addChild(std::make_unique<PodSet>());
    BodyTube&  podTube = pods.addChild(std::make_unique<BodyTube>());
    LaunchLug& lug     = podTube.addChild(std::make_unique<LaunchLug>());
    lug.setInstanceCount(3);
    podTube.addChild(std::make_unique<TrapezoidFinSet>());
    ASSERT_EQ(pods.getInstanceCount(), 2);
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 19U);

    // Components side by side add up; the tubes of a cluster hold what is in the tube.
    InnerTube&                        inner = tube.addChild(std::make_unique<InnerTube>());
    const ClusterConfiguration* const grid  = ClusterConfiguration::fromXmlName("9-grid");
    ASSERT_NE(grid, nullptr);
    inner.setClusterConfiguration(*grid);
    inner.addChild(std::make_unique<InnerTube>());
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 19U + 9U + 9U);

    // Every flight configuration keeps all of them, the default one too.
    rocket.createFlightConfiguration(FlightConfigurationId{});
    EXPECT_EQ(DocumentConfig::instanceLoad(rocket), 2U * 37U);
    rocket.createFlightConfiguration(FlightConfigurationId{});
    EXPECT_EQ(DocumentConfig::instanceLoad(lug), 3U * 37U);
}

TEST(InstanceBudget, ATreeWithoutARocketHasOneConfiguration)
{
    BodyTube   tube;
    LaunchLug& lug = tube.addChild(std::make_unique<LaunchLug>());
    lug.setInstanceCount(4);
    EXPECT_EQ(DocumentConfig::instanceLoad(lug), 5U);
    EXPECT_TRUE(DocumentConfig::flightConfigurationFits(lug));
}

// 2147483647 pods, each with as many, each with as many: 2^93, which no 64-bit number holds.
// The tree has no rocket, so nothing builds those instances.
TEST(InstanceBudget, TheArithmeticSaturates)
{
    constexpr int kMost = std::numeric_limits<int>::max();
    PodSet        outer;
    outer.setInstanceCount(kMost);
    BodyTube& tube   = outer.addChild(std::make_unique<BodyTube>());
    PodSet&   middle = tube.addChild(std::make_unique<PodSet>());
    middle.setInstanceCount(kMost);
    BodyTube& middleTube = middle.addChild(std::make_unique<BodyTube>());
    PodSet&   inner      = middleTube.addChild(std::make_unique<PodSet>());
    EXPECT_LT(DocumentConfig::instanceLoad(outer), kSaturated);
    EXPECT_FALSE(DocumentConfig::instanceCountFits(inner, kMost));
    inner.setInstanceCount(kMost);
    EXPECT_EQ(DocumentConfig::instanceLoad(outer), kSaturated);
    EXPECT_EQ(DocumentConfig::instanceLoad(inner), kSaturated);

    EXPECT_TRUE(DocumentConfig::instanceCountFits(inner, kMost)) << "the count it has";
    EXPECT_FALSE(DocumentConfig::childFits(inner, BodyTube{}));
    EXPECT_FALSE(DocumentConfig::flightConfigurationFits(inner));
    // A smaller rocket is always let be made, however large it is.
    EXPECT_TRUE(DocumentConfig::instanceCountFits(inner, kMost - 1));
    EXPECT_TRUE(DocumentConfig::instanceCountFits(outer, 1));
}

/// A rocket of a stage, a body tube and a launch lug, with @p configurations flight
/// configurations beside the default one.
struct LugRocket
{
    explicit LugRocket(int configurations = 0)
    {
        for (int i = 0; i < configurations; ++i)
        {
            rocket.createFlightConfiguration(FlightConfigurationId{});
        }
    }

    Rocket      rocket;
    AxialStage* stage = &rocket.addChild(std::make_unique<AxialStage>());
    BodyTube*   tube  = &stage->addChild(std::make_unique<BodyTube>());
    LaunchLug*  lug   = &tube->addChild(std::make_unique<LaunchLug>());
};

TEST(InstanceBudget, ACountFitsUpToTheBoundExactly)
{
    // Ten configurations with the default one; three components beside the lug.
    const LugRocket made(9);
    EXPECT_TRUE(DocumentConfig::instanceCountFits(*made.lug, 9997));
    EXPECT_FALSE(DocumentConfig::instanceCountFits(*made.lug, 9998));
    // A count the component would not take (LaunchLug::setInstanceCount()) is no larger rocket.
    EXPECT_TRUE(DocumentConfig::instanceCountFits(*made.lug, 0));
    EXPECT_TRUE(DocumentConfig::instanceCountFits(*made.lug, -5));

    made.lug->setInstanceCount(9997);
    ASSERT_EQ(DocumentConfig::instanceLoad(made.rocket), DocumentConfig::kMaxInstances);
    EXPECT_TRUE(DocumentConfig::instanceCountFits(*made.lug, 9997)) << "the count it has";
    EXPECT_FALSE(DocumentConfig::childFits(*made.tube, LaunchLug{}));
    EXPECT_FALSE(DocumentConfig::flightConfigurationFits(made.rocket));
}

TEST(InstanceBudget, AChildAndAConfigurationFitUpToTheBoundExactly)
{
    const LugRocket made(9);
    made.lug->setInstanceCount(9996);
    EXPECT_TRUE(DocumentConfig::childFits(*made.tube, LaunchLug{})) << "one instance, ten times";
    EXPECT_FALSE(DocumentConfig::childFits(*made.tube, TrapezoidFinSet{})) << "three fins";
    EXPECT_FALSE(DocumentConfig::childFits(*made.lug, LaunchLug{}))
        << "a child of the lug would be in each of its instances";
    EXPECT_FALSE(DocumentConfig::flightConfigurationFits(*made.lug));
}

TEST(InstanceBudget, AChildCountsWithWhatStandsBelowIt)
{
    const LugRocket made;
    made.lug->setInstanceCount(DocumentConfig::kMaxCount);
    PodSet     pods;
    BodyTube&  tube = pods.addChild(std::make_unique<BodyTube>());
    LaunchLug& lug  = tube.addChild(std::make_unique<LaunchLug>());
    // 10003 instances so far; the pod set brings 2 + 2 + 2 * its lug's count.
    lug.setInstanceCount(44996);
    EXPECT_TRUE(DocumentConfig::childFits(*made.tube, pods));
    lug.setInstanceCount(44997);
    EXPECT_FALSE(DocumentConfig::childFits(*made.tube, pods));
}

// ------------------------------------------------------------- through the handlers

/// The content of a rocket element with @p inner in a body tube of a stage.
[[nodiscard]] std::string inTube(std::string_view inner)
{
    return std::format(
        "<subcomponents><stage><subcomponents><bodytube><subcomponents>{}"
        "</subcomponents></bodytube></subcomponents></stage></subcomponents>",
        inner);
}

/// A launch lug of @p count instances.
[[nodiscard]] std::string lugOf(std::string_view count)
{
    return std::format("<launchlug><instancecount>{}</instancecount></launchlug>", count);
}

/// @p count empty flight configurations, each with an id of its own.
[[nodiscard]] std::string configurations(int count)
{
    std::string text;
    for (int i = 1; i <= count; ++i)
    {
        text += std::format(
            R"(<motorconfiguration configid="{:08x}-0000-0000-0000-000000000001"/>)", i);
    }
    return text;
}

/// Inner tubes in a "9-grid" cluster, one in the other, @p levels deep, in a body tube.
[[nodiscard]] std::string nestedClusters(int levels)
{
    std::string inner;
    for (int i = 0; i < levels; ++i)
    {
        inner = std::format(
            "<innertube><clusterconfiguration>9-grid</clusterconfiguration>{}"
            "</innertube>",
            inner.empty() ? std::string()
                          : std::format("<subcomponents>{}</subcomponents>", inner));
    }
    return inTube(inner);
}

/// What reading @p xml leaves: the warnings, each on a line, then "load <instanceLoad()>" and
/// "use <what using the rocket throws>". Nothing may be thrown, and the load stays in bounds.
[[nodiscard]] std::string whatIsLeftOf(std::string_view xml)
{
    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(xml);
    std::string       text;
    for (const std::string& warning : run.texts())
    {
        text += warning + "\n";
    }
    text += std::format("load {}\n", DocumentConfig::instanceLoad(fixture.rocket()));
    text += std::format("use {}", whatUsingTheRocketThrows(fixture));
    return text;
}

constexpr std::string_view kInvalid = "Invalid parameter encountered, ignoring.\n";

// The review's first document: 337 bytes that were 10^8 instances and std::bad_alloc.
TEST(InstanceBudget, NestedCountsDoNotMultiplyBeyondTheBound)
{
    const std::string xml =
        inTube(std::format("<podset><instancecount>10000</instancecount><subcomponents><bodytube>"
                           "<subcomponents>{}</subcomponents></bodytube></subcomponents></podset>",
                           lugOf("10000")));
    // The rocket, the stage, the tube, 10000 pods, their 10000 tubes and a lug in each.
    EXPECT_EQ(whatIsLeftOf(xml), std::string(kInvalid) + "load 30003\nuse ");
}

TEST(InstanceBudget, TheCountOfAParentIsRefusedForWhatStandsBelowIt)
{
    // The count comes behind the children here: 10 pods would each hold the 10000 lugs.
    const std::string xml =
        inTube(std::format("<podset><subcomponents><bodytube><subcomponents>{}</subcomponents>"
                           "</bodytube></subcomponents><instancecount>10</instancecount>"
                           "<instancecount>4</instancecount></podset>",
                           lugOf("10000")));
    // 3, then 4 pods, 4 tubes and 40000 lugs: the 10 were refused, the 4 fit.
    EXPECT_EQ(whatIsLeftOf(xml), std::string(kInvalid) + "load 40011\nuse ");
}

// Nine levels were 9^9 instances and std::bad_alloc after 28 s; 99 levels are what
// ComponentHandler::kMaxDepth lets a file nest.
TEST(InstanceBudget, NestedClustersDoNotMultiplyBeyondTheBound)
{
    // 3, then 9 + 81 + 729 + 6561 + 59049 tubes. A tube in each of the 59049 does not fit, so
    // the sixth level is ignored with everything in it.
    const std::string ignored =
        "Inner Tube would give the rocket too many component instances; ignoring this "
        "component and its subcomponents.\nload 66432\nuse ";
    EXPECT_EQ(whatIsLeftOf(nestedClusters(9)), ignored);
    EXPECT_EQ(whatIsLeftOf(nestedClusters(99)), ignored);
}

// With a second flight configuration the fifth level's layout is what does not fit (9^5 tubes
// for each of the two): it is refused, the tube stays single, and so does every tube below it
// as long as one more in each of the 6561 fits. The eleventh level is one too many.
TEST(InstanceBudget, ALayoutIsRefusedAndTheTubeStaysSingle)
{
    // 3 + 9 + 81 + 729 + 6561, then six single tubes in each of the 6561, twice.
    EXPECT_EQ(whatIsLeftOf(configurations(1) + nestedClusters(12)),
              std::string(kInvalid) +
                  "Inner Tube would give the rocket too many component instances; ignoring this "
                  "component and its subcomponents.\nload 93498\nuse ");
}

// The fins a fin set keeps of a number are what counts (8), and a component is ignored when
// the instances its constructor gives it are too many (a tube fin set has six tubes).
TEST(InstanceBudget, FinsCountAsInstances)
{
    const std::string xml = inTube(
        "<podset><instancecount>10000</instancecount><subcomponents><bodytube><subcomponents>"
        "<trapezoidfinset><fincount>8</fincount></trapezoidfinset>"
        "<trapezoidfinset><fincount>2147483647</fincount></trapezoidfinset>"
        "<tubefinset><instancecount>4</instancecount></tubefinset><launchlug/>"
        "</subcomponents></bodytube></subcomponents></podset>");
    // 3, 10000 pods and tubes, two fin sets of three fins and a launch lug in each.
    EXPECT_EQ(whatIsLeftOf(xml),
              std::string(kInvalid) +
                  "Tube Fin Set would give the rocket too many component instances; ignoring "
                  "this component and its subcomponents.\nload 90003\nuse ");
}

// The verifier's document: every path within a bound of its own, the sum beyond any.
TEST(InstanceBudget, ComponentsSideBySideAddUp)
{
    std::string lugs;
    for (int i = 0; i < 12; ++i)
    {
        lugs += lugOf("10000");
    }
    // Nine lugs of 10000 fit; the other three stay single.
    EXPECT_EQ(whatIsLeftOf(inTube(lugs)), std::string(kInvalid) + "load 90006\nuse ");
}

TEST(InstanceBudget, FlightConfigurationsAreAFactor)
{
    // 10003 instances: eight configurations beside the default one fit, the ninth does not.
    EXPECT_EQ(whatIsLeftOf(inTube(lugOf("10000")) + configurations(40)),
              std::string(kInvalid) + "load 90027\nuse ");
    // The other way round: 41 configurations leave room for 2439 instances.
    EXPECT_EQ(whatIsLeftOf(configurations(40) + inTube(lugOf("10000"))),
              std::string(kInvalid) + "load 164\nuse ");
    EXPECT_EQ(whatIsLeftOf(configurations(40) + inTube(lugOf("2436"))), "load 99999\nuse ");
    EXPECT_EQ(whatIsLeftOf(configurations(40) + inTube(lugOf("2437"))),
              std::string(kInvalid) + "load 164\nuse ");
}

TEST(InstanceBudget, TheMotorsOfAMountMakeNoConfigurationBeyondTheBound)
{
    std::string motors;
    for (int i = 1; i <= 12; ++i)
    {
        motors += std::format(R"(<motor configid="{:08x}-0000-0000-0000-000000000001">)"
                              "<designation>C6</designation><delay>3</delay></motor>",
                              i);
    }
    const std::string xml = std::format(
        "<subcomponents><stage><subcomponents><bodytube><subcomponents>{}</subcomponents>"
        "<motormount>{}</motormount></bodytube></subcomponents></stage></subcomponents>",
        lugOf("10000"), motors);
    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(xml);
    EXPECT_EQ(run.texts(), std::vector<std::string>{"Invalid parameter encountered, ignoring."});
    EXPECT_EQ(fixture.rocket().getFlightConfigurationCount(), 8);
    EXPECT_EQ(DocumentConfig::instanceLoad(fixture.rocket()), 90027U);
    // A motor that was refused is in no configuration, the default one least of all.
    EXPECT_FALSE(fixture.rocket().getEmptyConfiguration().hasMotors());
    EXPECT_FALSE(fixture.rocket()
                     .getFlightConfiguration(FlightConfigurationId::defaultValueId())
                     .hasMotors());
    EXPECT_EQ(whatUsingTheRocketThrows(fixture), "");
}

// A file may always make a rocket smaller, and what it has is never taken away.
TEST(InstanceBudget, ASmallerCountAndAKnownConfigurationAreNeverRefused)
{
    const std::string known =
        R"(<motorconfiguration configid="00000003-0000-0000-0000-000000000001" default="true">)"
        "<name>again</name></motorconfiguration>";
    RocketLoadFixture fixture;
    const HandlerRun  run =
        fixture.load(inTube(lugOf("10000") + "<launchlug><instancecount>10000</instancecount>"
                                             "<instancecount>5</instancecount></launchlug>") +
                     configurations(4) + known);
    EXPECT_TRUE(run.texts().empty());
    EXPECT_EQ(fixture.rocket().getFlightConfigurationCount(), 4);
    EXPECT_EQ(fixture.rocket().getSelectedConfiguration().getName(fixture.fixture().preferences()),
              "again");
    EXPECT_EQ(DocumentConfig::instanceLoad(fixture.rocket()), 5U * 10008U);
}

// At the bound the ninth configuration is not made, and the element of one the rocket has is
// read as ever: its name, and the selection.
TEST(InstanceBudget, AKnownConfigurationIsReadAtTheBound)
{
    const std::string known =
        R"(<motorconfiguration configid="00000003-0000-0000-0000-000000000001" default="true">)"
        "<name>again</name></motorconfiguration>";
    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(inTube(lugOf("10000")) + configurations(9) + known);
    EXPECT_EQ(run.texts(), std::vector<std::string>{"Invalid parameter encountered, ignoring."});
    EXPECT_EQ(fixture.rocket().getFlightConfigurationCount(), 8);
    EXPECT_EQ(fixture.rocket().getSelectedConfiguration().getName(fixture.fixture().preferences()),
              "again");
    EXPECT_EQ(DocumentConfig::instanceLoad(fixture.rocket()), 90027U);
    EXPECT_EQ(whatUsingTheRocketThrows(fixture), "");
}

}  // namespace
