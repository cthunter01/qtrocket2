#include "QtRocket/rocket/FlightConfiguration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/RocketUtils.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestComponent.h"
#include "rocket/TestMotorMount.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BoundingBox;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InMemoryPreferences;
using QtRocket::InstanceContext;
using QtRocket::InstanceMap;
using QtRocket::ModId;
using QtRocket::MotorConfiguration;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::ReferenceType;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::motorC6;
using QtRocket::Test::motorD21;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestBodyComponent;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestMotorMount;

// FlightConfigurationTest's tolerance: MathUtil.EPSILON * 1000.
constexpr double kEpsilon = 1e-8 * 1e3;

// Java's Chars.TIMES, U+00D7.
constexpr std::string_view kTimes = "\xC3\x97";

::testing::AssertionResult near(const Coordinate& actual, const Coordinate& expected,
                                double tolerance = kEpsilon)
{
    if (std::abs(actual.x - expected.x) <= tolerance &&
        std::abs(actual.y - expected.y) <= tolerance &&
        std::abs(actual.z - expected.z) <= tolerance)
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << actual.toPreciseString() << " is not " << expected.toPreciseString();
}

// ======================================================== ported from FlightConfigurationTest

TEST(FlightConfigurationTest, EmptyRocket)
{
    const TestEstesAlphaIII    r1;
    const FlightConfiguration& config      = r1.rocket->getSelectedConfiguration();
    const FlightConfiguration  configClone = config.clone();
    EXPECT_EQ(&config.getRocket(), &configClone.getRocket());
}

TEST(FlightConfigurationTest, FlightConfigurationRocketLength)
{
    const TestBeta       beta;
    Rocket&              rocket = *beta.rocket;
    FlightConfiguration& config = rocket.getEmptyConfiguration();
    rocket.setSelectedConfiguration(config.getId());

    config.setAllStages();

    EXPECT_EQ(config.getActiveStageCount(), 2) << "active stage count doesn't match";

    const double expectedLength = 0.335;
    EXPECT_NEAR(expectedLength, config.getLengthAerodynamic(), kEpsilon);

    const double expectedReferenceLength = 0.024;
    EXPECT_NEAR(expectedReferenceLength, config.getReferenceLength(), kEpsilon);

    const double expectedReferenceArea =
        std::pow(expectedReferenceLength / 2, 2) * std::numbers::pi;
    EXPECT_NEAR(expectedReferenceArea, config.getReferenceArea(), kEpsilon);
}

TEST(FlightConfigurationTest, CloneBasic)
{
    const TestBeta       beta;
    FlightConfiguration& config1 = beta.rocket->getSelectedConfiguration();

    config1.setAllStages();
    EXPECT_EQ(config1.getActiveStageCount(), 2);
    EXPECT_EQ(config1.getActiveMotors().size(), 2U);
    const double expectedLength = 0.335;
    EXPECT_NEAR(expectedLength, config1.getLengthAerodynamic(), kEpsilon);
    const double expectedReferenceLength = 0.024;
    EXPECT_NEAR(expectedReferenceLength, config1.getReferenceLength(), kEpsilon);
    const double expectedReferenceArea =
        std::pow(expectedReferenceLength / 2, 2) * std::numbers::pi;
    EXPECT_NEAR(expectedReferenceArea, config1.getReferenceArea(), kEpsilon);

    const FlightConfiguration config2 = config1.clone();

    EXPECT_EQ(config2.getActiveStageCount(), 2);
    EXPECT_EQ(config2.getActiveMotors().size(), 2U);
    EXPECT_NEAR(expectedLength, config2.getLengthAerodynamic(), kEpsilon);
    EXPECT_NEAR(expectedReferenceLength, config2.getReferenceLength(), kEpsilon);
    EXPECT_NEAR(expectedReferenceArea, config2.getReferenceArea(), kEpsilon);
}

TEST(FlightConfigurationTest, CloneIndependence)
{
    const TestBeta       beta;
    FlightConfiguration& config1 = beta.rocket->getSelectedConfiguration();

    // Cloned configurations change independently.
    config1.setAllStages();
    const FlightConfiguration config2 = config1.clone();
    config1.clearAllStages();

    EXPECT_EQ(config1.getActiveStageCount(), 0);
    EXPECT_EQ(config1.getActiveMotors().size(), 0U);

    EXPECT_EQ(config2.getActiveStageCount(), 2);
    EXPECT_EQ(config2.getActiveMotors().size(), 2U);
}

TEST(FlightConfigurationTest, SingleStageRocket)
{
    const TestEstesAlphaIII r1;
    FlightConfiguration&    config = r1.rocket->getSelectedConfiguration();

    // Only the first stage active.
    config.clearAllStages();
    config.setOnlyStage(0);

    EXPECT_EQ(config.getStageCount(), 1);
    EXPECT_EQ(config.getActiveStageCount(), 1);

    config.setOnlyStage(0);
    config.setAllStages();
    EXPECT_EQ(config.getActiveStageCount(), 1);
}

TEST(FlightConfigurationTest, DefaultConfigurationIsEmpty)
{
    const TestEstesAlphaIII    r1;
    const FlightConfiguration& defaultConfig = r1.rocket->getSelectedConfiguration();

    EXPECT_EQ(r1.rocket->getEmptyConfiguration().getActiveMotors().size(), 0U)
        << "Empty configuration has motors! it should be empty!";
    EXPECT_EQ(defaultConfig.getActiveMotors().size(), 0U)
        << "Default configuration is not the empty configuration. It should be!";
}

TEST(FlightConfigurationTest, CreateConfigurationNullId)
{
    const TestEstesAlphaIII rkt;
    EXPECT_EQ(rkt.rocket->getConfigurationCount(), 5)
        << "number of loaded configuration counts doesn't actually match.";

    // Java: createFlightConfiguration(null).
    rkt.rocket->createFlightConfiguration();
    EXPECT_EQ(rkt.rocket->getConfigurationCount(), 6)
        << "createFlightConfiguration with null: doesn't actually work.";
}

TEST(FlightConfigurationTest, MotorConfigurations)
{
    const TestEstesAlphaIII rkt;
    const auto&             smmt =
        dynamic_cast<const TestMotorMount&>(rkt.rocket->getChild(0).getChild(1).getChild(2));
    EXPECT_EQ(smmt.getMotorConfigurationSet().size(), 5U)
        << "number of motor configurations doesn't match.";
}

TEST(FlightConfigurationTest, FlightConfigurationGetters)
{
    const TestEstesAlphaIII rkt;
    EXPECT_EQ(rkt.rocket->getConfigurationCount(), 5);
    EXPECT_EQ(rkt.rocket->getIds().size(), 5U);

    // Upon success these complete silently.
    EXPECT_NO_THROW(static_cast<void>(rkt.rocket->getFlightConfigurationByIndex(4)));
    EXPECT_NO_THROW(static_cast<void>(rkt.rocket->getFlightConfigurationByIndex(5, true)));
}

TEST(FlightConfigurationTest, GetFlightConfigurationOutOfBounds)
{
    const TestEstesAlphaIII rkt;
    EXPECT_EQ(rkt.rocket->getConfigurationCount(), 5);
    // No configuration at index 5 (Java: IndexOutOfBoundsException).
    EXPECT_THROW(static_cast<void>(rkt.rocket->getFlightConfigurationByIndex(5)), BugError);
}

TEST(FlightConfigurationTest, MultiStageRocket)
{
    const TestBeta       beta;
    Rocket&              rkt    = *beta.rocket;
    FlightConfiguration& config = rkt.getSelectedConfiguration();

    EXPECT_EQ(config.getStageCount(), 2) << "stage count doesn't match";

    config.clearAllStages();
    EXPECT_FALSE(config.isStageActive(0)) << " clear all stages: check #0: ";
    EXPECT_FALSE(config.isStageActive(1)) << " clear all stages: check #1: ";

    // Only the first stage active.
    config.setOnlyStage(0);
    EXPECT_EQ(config.getActiveStageCount(), 1) << "active stage count doesn't match";
    EXPECT_TRUE(config.isStageActive(0)) << " setting single stage active: ";

    // Only the second stage active.
    config.setOnlyStage(1);
    EXPECT_FALSE(config.isStageActive(0)) << "Setting single stage active: ";
    EXPECT_TRUE(config.isStageActive(1)) << "Setting single stage active: ";

    config.clearStage(0);
    EXPECT_FALSE(config.isStageActive(0)) << " deactivate stage #0: ";
    EXPECT_TRUE(config.isStageActive(1)) << "     active stage #1: ";

    // Both stages active.
    config.setAllStages();
    EXPECT_TRUE(config.isStageActive(0)) << " activate all stages: check stage #0: ";
    EXPECT_TRUE(config.isStageActive(1)) << " activate all stages: check stage #1: ";

    // Toggling a single stage.
    config.setAllStages();
    config.toggleStage(0);
    EXPECT_FALSE(config.isStageActive(0)) << " toggle stage #0: ";
    config.toggleStage(0);
    EXPECT_TRUE(config.isStageActive(0)) << " toggle stage #0: ";
    config.toggleStage(0);
    EXPECT_FALSE(config.isStageActive(0)) << " toggle stage #0: ";

    AxialStage* sustainer = rkt.getTopmostStage(config);
    AxialStage* booster   = rkt.getBottomCoreStage(config);
    ASSERT_NE(sustainer, nullptr);
    ASSERT_NE(booster, nullptr);
    EXPECT_EQ(sustainer->getStageNumber(), 1) << " sustainer stage is stage #1: ";
    EXPECT_EQ(booster->getStageNumber(), 1) << " booster stage is stage #1: ";

    config.setAllStages();
    config.setStageActive(1, false);
    sustainer = rkt.getTopmostStage(config);
    booster   = rkt.getBottomCoreStage(config);
    EXPECT_EQ(sustainer->getStageNumber(), 0) << " sustainer stage is stage #1: ";
    EXPECT_EQ(booster->getStageNumber(), 0) << " booster stage is stage #1: ";

    config.setAllStages();
    sustainer = rkt.getTopmostStage(config);
    booster   = rkt.getBottomCoreStage(config);
    EXPECT_EQ(sustainer->getStageNumber(), 0) << " sustainer stage is stage #0: ";
    EXPECT_EQ(booster->getStageNumber(), 1) << " booster stage is stage #1: ";

    config.clearAllStages();
    config.activateStagesThrough(*sustainer);
    EXPECT_TRUE(config.isStageActive(sustainer->getStageNumber()))
        << " sustainer stage is active: ";
    EXPECT_FALSE(config.isStageActive(booster->getStageNumber())) << " booster stage is inactive: ";

    config.clearAllStages();
    config.activateStagesThrough(*booster);
    EXPECT_TRUE(config.isStageActive(sustainer->getStageNumber()))
        << " sustainer stage is active: ";
    EXPECT_TRUE(config.isStageActive(booster->getStageNumber())) << " booster stage is active: ";
}

TEST(FlightConfigurationTest, MotorClusters)
{
    const TestBeta       beta;
    FlightConfiguration& config = beta.rocket->getSelectedConfiguration();

    config.clearAllStages();
    EXPECT_EQ(config.getActiveMotors().size(), 0U) << "active motor count doesn't match";

    config.setOnlyStage(0);
    EXPECT_EQ(config.getActiveMotors().size(), 1U) << "active motor count doesn't match: ";

    config.setOnlyStage(1);
    EXPECT_EQ(config.getActiveMotors().size(), 1U) << "active motor count doesn't match: ";

    config.setAllStages();
    EXPECT_EQ(config.getActiveMotors().size(), 2U) << "active motor count doesn't match: ";
}

/// Whether @p context is instance @p instanceNumber of an inner tube at @p location.
::testing::AssertionResult isMotorTube(const InstanceContext& context, int instanceNumber,
                                       const Coordinate& location)
{
    if (context.component->kind() != ComponentKind::INNER_TUBE)
    {
        return ::testing::AssertionFailure() << "not an inner tube";
    }
    if (context.instanceNumber != instanceNumber)
    {
        return ::testing::AssertionFailure() << "instance " << context.instanceNumber;
    }
    return near(context.getLocation(), location);
}

TEST(FlightConfigurationTest, IterateComponents)
{
    const TestFalcon9Heavy f9h;
    Rocket&                rocket   = *f9h.rocket;
    FlightConfiguration&   selected = rocket.getSelectedConfiguration();

    selected.clearAllStages();
    selected.toggleStage(1);

    const InstanceMap& instances = selected.getActiveInstances();

    const AxialStage& coreStage = *f9h.coreStage;
    {  // Core Stage
        const std::span<const InstanceContext> coreStageContextList =
            instances.getInstanceContexts(coreStage);
        ASSERT_FALSE(coreStageContextList.empty());
        const InstanceContext& coreStageContext = coreStageContextList[0];
        EXPECT_EQ(coreStageContext.component->kind(), ComponentKind::AXIAL_STAGE);
        EXPECT_EQ(coreStageContext.component->getId(), rocket.getChild(1).getId());
        EXPECT_EQ(coreStageContext.component->getInstanceCount(), 1);

        const Coordinate coreLocation = coreStageContext.getLocation();
        EXPECT_NEAR(coreLocation.x, 0.564, kEpsilon);
        EXPECT_NEAR(coreLocation.y, 0.0, kEpsilon);
        EXPECT_NEAR(coreLocation.z, 0.0, kEpsilon);
    }

    // Booster Stage
    {
        const ParallelStage&                   boosterStage = *f9h.boosterStage;
        const std::span<const InstanceContext> boosterStageContextList =
            instances.getInstanceContexts(boosterStage);
        ASSERT_EQ(boosterStageContextList.size(), 2U);
        const InstanceContext& boosterStage0Context = boosterStageContextList[0];
        EXPECT_EQ(boosterStage0Context.component->kind(), ComponentKind::PARALLEL_STAGE);
        EXPECT_EQ(boosterStage0Context.component->getId(), boosterStage.getId());
        EXPECT_EQ(boosterStage0Context.instanceNumber, 0);
        EXPECT_TRUE(near(boosterStage0Context.getLocation(), Coordinate{0.484, 0.077, 0.0}));

        const InstanceContext& boosterStage1Context = boosterStageContextList[1];
        EXPECT_EQ(boosterStage1Context.component->kind(), ComponentKind::PARALLEL_STAGE);
        EXPECT_EQ(boosterStage1Context.component->getId(), boosterStage.getId());
        EXPECT_EQ(boosterStage1Context.instanceNumber, 1);
        EXPECT_TRUE(near(boosterStage1Context.getLocation(), Coordinate{0.484, -0.077, 0.0}));

        {  // Booster Body
            const TestBodyComponent&               boosterBody = *f9h.boosterBody;
            const std::span<const InstanceContext> boosterBodyContextList =
                instances.getInstanceContexts(boosterBody);
            ASSERT_EQ(boosterBodyContextList.size(), 2U);

            // the instance number rocket-wide
            const InstanceContext& boosterBodyContext = boosterBodyContextList[1];
            // the instance number per parent
            EXPECT_EQ(boosterBodyContext.instanceNumber, 0);
            EXPECT_EQ(boosterBodyContext.component->kind(), ComponentKind::BODY_TUBE);
            EXPECT_TRUE(near(boosterBodyContext.getLocation(), Coordinate{0.564, -0.077, 0.0}));

            {  // Booster::Motor Tubes ( x2 x4)
                const std::span<const InstanceContext> mmtContextList =
                    instances.getInstanceContexts(*f9h.boosterMotorTubes);
                ASSERT_EQ(8U, mmtContextList.size());

                EXPECT_TRUE(isMotorTube(mmtContextList[4], 0, Coordinate{1.214, -0.062, -0.015}));
                EXPECT_TRUE(isMotorTube(mmtContextList[5], 1, Coordinate{1.214, -0.092, -0.015}));
                EXPECT_TRUE(isMotorTube(mmtContextList[6], 2, Coordinate{1.214, -0.092, 0.015}));
                EXPECT_TRUE(isMotorTube(mmtContextList[7], 3, Coordinate{1.214, -0.062, 0.015}));
            }
            {  // Booster::Fins::Instances ( x2 x3)
                const std::span<const InstanceContext> finContextList =
                    instances.getInstanceContexts(*f9h.boosterFins);
                ASSERT_EQ(6U, finContextList.size());

                const InstanceContext& boosterFinContext0 = finContextList[3];
                EXPECT_EQ(boosterFinContext0.component->kind(), ComponentKind::TRAPEZOID_FIN_SET);
                EXPECT_EQ(boosterFinContext0.instanceNumber, 0);
                EXPECT_TRUE(
                    near(boosterFinContext0.getLocation(), Coordinate{1.044, -0.1155, 0.0}));

                const InstanceContext& boosterFinContext1 = finContextList[4];
                EXPECT_EQ(boosterFinContext1.instanceNumber, 1);
                EXPECT_TRUE(near(boosterFinContext1.getLocation(),
                                 Coordinate{1.044, -0.05775, -0.033341978}));

                const InstanceContext& boosterFinContext2 = finContextList[5];
                EXPECT_EQ(boosterFinContext2.instanceNumber, 2);
                EXPECT_TRUE(
                    near(boosterFinContext2.getLocation(), Coordinate{1.044, -0.05775, 0.03334}));
            }
        }
    }
}

TEST(FlightConfigurationTest, GetLowestMotorInstancesReturnsEmptyForMotorlessConfiguration)
{
    const TestEstesAlphaIII    rocket;
    const FlightConfiguration& config = rocket.rocket->getSelectedConfiguration();
    EXPECT_TRUE(config.getLowestMotorInstances().isEmpty());
}

TEST(FlightConfigurationTest, GetLowestMotorInstancesReturnsCoreMotorWhenOnlyCoreIsActive)
{
    const TestFalcon9Heavy f9h;
    FlightConfiguration&   config = f9h.rocket->getSelectedConfiguration();

    config.setOnlyStage(1);

    const InstanceMap lowestMotorInstances = config.getLowestMotorInstances();
    EXPECT_EQ(1U, lowestMotorInstances.size());
    EXPECT_TRUE(lowestMotorInstances.containsKey(*f9h.coreBody));
    EXPECT_FALSE(lowestMotorInstances.containsKey(*f9h.boosterMotorTubes));

    const std::span<const InstanceContext> coreMotorContexts =
        lowestMotorInstances.getInstanceContexts(*f9h.coreBody);
    ASSERT_EQ(1U, coreMotorContexts.size());
    EXPECT_NEAR(0.0, coreMotorContexts[0].getLocation().z, kEpsilon);
}

TEST(FlightConfigurationTest, GetLowestMotorInstancesReturnsLowestBoosterMotorRow)
{
    const TestBeta       beta;
    FlightConfiguration& config = beta.rocket->getSelectedConfiguration();

    config.setAllStages();

    const InstanceMap lowestMotorInstances = config.getLowestMotorInstances();
    EXPECT_EQ(1U, lowestMotorInstances.size());
    EXPECT_TRUE(lowestMotorInstances.containsKey(*beta.boosterMmt));
    EXPECT_FALSE(lowestMotorInstances.containsKey(*beta.inner));

    const std::span<const InstanceContext> lowestContexts =
        lowestMotorInstances.getInstanceContexts(*beta.boosterMmt);
    ASSERT_EQ(1U, lowestContexts.size());
    EXPECT_NEAR(0.285, lowestContexts[0].getLocation().x, kEpsilon);
}

TEST(FlightConfigurationTest, GetLowestMotorInstancesReturnsAllMotorsOnLowestAftRow)
{
    const TestFalcon9Heavy f9h;
    FlightConfiguration&   config = f9h.rocket->getSelectedConfiguration();

    config.setAllStages();

    const InstanceMap lowestMotorInstances = config.getLowestMotorInstances();
    EXPECT_EQ(1U, lowestMotorInstances.size());
    EXPECT_TRUE(lowestMotorInstances.containsKey(*f9h.boosterMotorTubes));
    EXPECT_FALSE(lowestMotorInstances.containsKey(*f9h.coreBody));

    const std::span<const InstanceContext> lowestContexts =
        lowestMotorInstances.getInstanceContexts(*f9h.boosterMotorTubes);
    ASSERT_EQ(8U, lowestContexts.size());
    for (const InstanceContext& context : lowestContexts)
    {
        EXPECT_NEAR(1.214, context.getLocation().x, kEpsilon);
    }
}

TEST(FlightConfigurationTest, IterateCoreComponentsAllStagesActive)
{
    const TestFalcon9Heavy f9h;
    FlightConfiguration&   selected = f9h.rocket->getSelectedConfiguration();

    selected.setAllStages();

    const std::vector<RocketComponent*> components = selected.getCoreComponents();
    ASSERT_EQ(components.size(), 10U);

    EXPECT_EQ(components[0]->kind(), ComponentKind::AXIAL_STAGE);
    EXPECT_EQ(components[0]->getName(), "Payload Fairing Stage");
    EXPECT_EQ(components[1]->kind(), ComponentKind::AXIAL_STAGE);
    EXPECT_EQ(components[1]->getName(), "Core Stage");
    EXPECT_EQ(components[2]->kind(), ComponentKind::NOSE_CONE);
    EXPECT_EQ(components[3]->kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(components[3]->getName(), "PL Fairing Body");
    EXPECT_EQ(components[4]->kind(), ComponentKind::TRANSITION);
    EXPECT_EQ(components[5]->kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(components[5]->getName(), "Upper Stage Body");
    EXPECT_EQ(components[6]->kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(components[6]->getName(), "Interstage");
    EXPECT_EQ(components[7]->kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(components[7]->getName(), "Core Stage Body");
    EXPECT_EQ(components[8]->kind(), ComponentKind::PARACHUTE);
    EXPECT_EQ(components[9]->kind(), ComponentKind::SHOCK_CORD);
}

TEST(FlightConfigurationTest, IterateCoreComponentsActiveOnly)
{
    const TestFalcon9Heavy f9h;
    FlightConfiguration&   selected = f9h.rocket->getSelectedConfiguration();

    selected.clearAllStages();
    selected.toggleStage(2);  // booster only
    EXPECT_EQ(selected.getCoreComponents().size(), 0U);

    selected.clearAllStages();
    selected.toggleStage(1);  // the core stage and its booster set
    const std::vector<RocketComponent*> components = selected.getCoreComponents();
    ASSERT_EQ(components.size(), 2U);
    EXPECT_EQ(components[0]->kind(), ComponentKind::AXIAL_STAGE);
    EXPECT_EQ(components[0]->getName(), "Core Stage");
    EXPECT_EQ(components[1]->kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(components[1]->getName(), "Core Stage Body");
}

/// The names of FlightConfigurationTest.testName for a booster set of @p boosters instances,
/// with OpenRocket's English messages instead of the debug translator's keys: "None" for
/// Rocket.motorCount.noStageMotors and "No motors" for Rocket.motorCount.Nomotor.
void expectNamesWithBoosters(FlightConfiguration& selected, ParallelStage& boosterStage,
                             int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);

    // Only motors
    selected.setName("[{motors}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; M1350-0; {} G77-0]", count), selected.getName(prefs));

    // Only manufacturers
    selected.setName("[{manufacturers}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; AeroTech; {} AeroTech]", count), selected.getName(prefs));
}

/// The two-key names of FlightConfigurationTest.testName (see expectNamesWithBoosters()).
void expectTwoTagNamesWithBoosters(FlightConfiguration& selected, ParallelStage& boosterStage,
                                   int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);

    // Only cases
    selected.setName("[{cases}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; SU 75/512; {} SU 29/180]", count), selected.getName(prefs));

    // Only motors or only manufacturers
    selected.setName("[{motors}] - [{manufacturers}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; M1350-0; {0} G77-0] - [None; AeroTech; {0} AeroTech]", count),
              selected.getName(prefs));
}

/// The two-tag names of FlightConfigurationTest.testName with some stages inactive.
void expectTwoTagNamesOfSomeStages(FlightConfiguration& selected, ParallelStage& boosterStage,
                                   int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);
    selected.setName("[{motors}] - [{manufacturers}]");

    selected.setOnlyStage(0);
    EXPECT_EQ("[No motors] - [No motors]", selected.getName(prefs));

    selected.setOnlyStage(1);
    EXPECT_EQ("[; M1350-0; ] - [; AeroTech; ]", selected.getName(prefs));

    selected.setAllStages();
    selected.setStageActive(0, false);
    EXPECT_EQ(std::format("[; M1350-0; {0} G77-0] - [; AeroTech; {0} AeroTech]", count),
              selected.getName(prefs));
}

/// The rest of the combinations of FlightConfigurationTest.testName (see above).
void expectCombinedNamesWithBoosters(FlightConfiguration& selected, ParallelStage& boosterStage,
                                     int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);

    // Combination of motors and manufacturers
    selected.setName("[{motors  manufacturers}] -- [{manufacturers}] - [{motors}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; M1350-0  AeroTech; {0} G77-0  AeroTech] -- [None; AeroTech; {0} "
                          "AeroTech] - [None; M1350-0; {0} G77-0]",
                          count),
              selected.getName(prefs));

    selected.setOnlyStage(0);
    EXPECT_EQ("[No motors] -- [No motors] - [No motors]", selected.getName(prefs));

    selected.setOnlyStage(1);
    EXPECT_EQ("[; M1350-0  AeroTech; ] -- [; AeroTech; ] - [; M1350-0; ]", selected.getName(prefs));

    selected.setAllStages();
    selected.setStageActive(0, false);
    EXPECT_EQ(std::format("[; M1350-0  AeroTech; {0} G77-0  AeroTech] -- [; AeroTech; {0} "
                          "AeroTech] - [; M1350-0; {0} G77-0]",
                          count),
              selected.getName(prefs));
}

/// The key combinations of FlightConfigurationTest.testName in one tag.
void expectOneTagCombinationsWithBoosters(FlightConfiguration& selected,
                                          ParallelStage& boosterStage, int boosters,
                                          const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);

    // Combination of manufacturers and motors
    selected.setName("[{manufacturers | motors}]");
    selected.setAllStages();
    EXPECT_EQ(std::format("[None; AeroTech | M1350-0; {} AeroTech | G77-0]", count),
              selected.getName(prefs));
}

/// The "[{manufacturers | motors}]" name of FlightConfigurationTest.testName with some stages
/// inactive.
void expectOneTagCombinationOfSomeStages(FlightConfiguration& selected, ParallelStage& boosterStage,
                                         int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);
    selected.setName("[{manufacturers | motors}]");

    selected.setOnlyStage(0);
    EXPECT_EQ("[No motors]", selected.getName(prefs));

    selected.setOnlyStage(1);
    EXPECT_EQ("[; AeroTech | M1350-0; ]", selected.getName(prefs));

    selected.setAllStages();
    selected.setStageActive(0, false);
    EXPECT_EQ(std::format("[; AeroTech | M1350-0; {} AeroTech | G77-0]", count),
              selected.getName(prefs));
}

/// The three-key tag of FlightConfigurationTest.testName.
void expectThreeKeysWithBoosters(FlightConfiguration& selected, ParallelStage& boosterStage,
                                 int boosters, const QtRocket::Preferences& prefs)
{
    boosterStage.setInstanceCount(boosters);
    const std::string count = std::format("{}{}", 4 * boosters, kTimes);

    // Combination of motors, manufacturers and cases
    selected.setName("[{motors manufacturers | cases}]");
    selected.setAllStages();
    EXPECT_EQ(
        std::format("[None; M1350-0 AeroTech | SU 75/512; {} G77-0 AeroTech | SU 29/180]", count),
        selected.getName(prefs));
}

/// Expects @p name to be kept as it is, whatever stages are active.
void expectNameKept(FlightConfiguration& selected, const std::string& name,
                    const QtRocket::Preferences& prefs)
{
    selected.setName(name);
    selected.setAllStages();
    EXPECT_EQ(name, selected.getName(prefs));
    selected.setOnlyStage(0);
    EXPECT_EQ(name, selected.getName(prefs));
    selected.setOnlyStage(1);
    EXPECT_EQ(name, selected.getName(prefs));
    selected.setAllStages();
    selected.setStageActive(0, false);
    EXPECT_EQ(name, selected.getName(prefs));
}

TEST(FlightConfigurationTest, Name)
{
    const TestFalcon9Heavy    f9h;
    FlightConfiguration&      selected = f9h.rocket->getSelectedConfiguration();
    const InMemoryPreferences prefs;

    // Over different instance counts.
    for (const int boosters : {1, 2})
    {
        expectNamesWithBoosters(selected, *f9h.boosterStage, boosters, prefs);
        expectTwoTagNamesWithBoosters(selected, *f9h.boosterStage, boosters, prefs);
        expectTwoTagNamesOfSomeStages(selected, *f9h.boosterStage, boosters, prefs);
        expectCombinedNamesWithBoosters(selected, *f9h.boosterStage, boosters, prefs);
        expectOneTagCombinationsWithBoosters(selected, *f9h.boosterStage, boosters, prefs);
        expectOneTagCombinationOfSomeStages(selected, *f9h.boosterStage, boosters, prefs);
        expectThreeKeysWithBoosters(selected, *f9h.boosterStage, boosters, prefs);
    }

    // Empty tags
    expectNameKept(selected, "{}", prefs);

    // Invalid tags (1)
    expectNameKept(selected, "{motorms}", prefs);
    expectNameKept(selected, "{motor}", prefs);

    // Invalid tags (2)
    expectNameKept(selected, "{mot'ors manuf'acturers '}", prefs);
}

/// Whether each stage is active in @p config, by stage number.
std::vector<bool> stageActiveness(const FlightConfiguration& config)
{
    std::vector<bool> active;
    active.reserve(static_cast<std::size_t>(config.getStageCount()));
    for (int i = 0; i < config.getStageCount(); i++)
    {
        active.push_back(config.isStageActive(i));
    }
    return active;
}

TEST(FlightConfigurationTest, Copy)
{
    const TestFalcon9Heavy    f9h;
    FlightConfiguration&      original = f9h.rocket->getSelectedConfiguration();
    const InMemoryPreferences prefs;
    original.setName("[{motors}] - [{manufacturers}]");
    original.setOnlyStage(0);

    // Java: copy(null), a new random id.
    const FlightConfiguration copy = original.copy(FlightConfigurationId{});

    EXPECT_NE(original, copy);
    EXPECT_NE(&original, &copy);
    EXPECT_EQ(original.getName(prefs), copy.getName(prefs));
    EXPECT_NE(original.getFlightConfigurationId(), copy.getFlightConfigurationId());

    // The preloaded activeness is copied (none here).
    EXPECT_EQ(original.getPreloadedStageActiveness(), copy.getPreloadedStageActiveness());
    EXPECT_FALSE(copy.getPreloadedStageActiveness().has_value());

    // The modification id is copied. (Java also checks the private cached bounds and the invalid
    // bounds and reference length ids through reflection; they are private here.)
    EXPECT_EQ(original.getModId(), copy.getModId());

    EXPECT_EQ(stageActiveness(original), stageActiveness(copy));
    EXPECT_EQ(stageActiveness(copy), (std::vector<bool>{true, false, false}));
}

TEST(FlightConfigurationTest, Clone)
{
    const TestFalcon9Heavy    f9h;
    FlightConfiguration&      original = f9h.rocket->getSelectedConfiguration();
    const InMemoryPreferences prefs;
    original.setOnlyStage(0);

    const FlightConfiguration clone = original.clone();

    EXPECT_EQ(original, clone);
    EXPECT_NE(&original, &clone);
    EXPECT_EQ(original.getName(prefs), clone.getName(prefs));
    EXPECT_EQ(original.getFlightConfigurationId(), clone.getFlightConfigurationId());
    EXPECT_EQ(original.getPreloadedStageActiveness(), clone.getPreloadedStageActiveness());
    EXPECT_EQ(original.getModId(), clone.getModId());
    EXPECT_EQ(stageActiveness(original), stageActiveness(clone));
}

// ================================================================= QtRocket's own cases

/// A rocket of two stages, each with a body tube (0.3 m and 0.2 m, radius 0.02 m); events
/// enabled.
class ConfigurationTest : public ::testing::Test
{
protected:
    ConfigurationTest()
    {
        m_top     = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_topBody = &m_top->addChild(TestBodyComponent::make(0.3, 0.02));
        m_bottom  = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_mount   = &m_bottom->addChild(TestMotorMount::make(0.2, 0.02));
        m_mount->setMotorMount(true);
        m_rocket.enableEvents();
    }

    Rocket              m_rocket;
    AxialStage*         m_top{nullptr};
    TestBodyComponent*  m_topBody{nullptr};
    AxialStage*         m_bottom{nullptr};
    TestMotorMount*     m_mount{nullptr};
    InMemoryPreferences m_prefs;
};

TEST_F(ConfigurationTest, DefaultConfiguration)
{
    const FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_EQ(&config, &m_rocket.getEmptyConfiguration());
    EXPECT_TRUE(config.getId().isDefaultId());
    EXPECT_EQ(&config.getRocket(), &m_rocket);
    EXPECT_EQ(config.getStageCount(), 2);
    EXPECT_EQ(config.getActiveStageCount(), 2) << "stages start active";
    EXPECT_FALSE(config.hasMotors());
    EXPECT_EQ(config.getNameRaw(m_prefs), "[{motors}]");
    EXPECT_FALSE(config.isNameOverridden(m_prefs));
    EXPECT_EQ(config.getName(m_prefs), "[No motors]");
    EXPECT_EQ(config.toString(m_prefs), config.getName(m_prefs));
    EXPECT_EQ(FlightConfiguration::kDefaultConfigName, "[{motors}]");
    EXPECT_EQ(m_rocket.getConfigurationCount(), 0);
    EXPECT_EQ(m_rocket.getFlightConfigurationCount(), 0);
    EXPECT_TRUE(m_rocket.getIds().empty());
}

TEST_F(ConfigurationTest, InstanceIdsCountUp)
{
    const FlightConfiguration a{m_rocket, FlightConfigurationId{}};
    const FlightConfiguration b{m_rocket, FlightConfigurationId{}};
    EXPECT_EQ(b.getConfigurationInstanceId(), a.getConfigurationInstanceId() + 1);
    EXPECT_EQ(a.getModId(), ModId::zero()) << "a new configuration's modification id";
}

TEST_F(ConfigurationTest, StageActivation)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_TRUE(config.isStageActive(-1)) << "the rocket itself";
    EXPECT_FALSE(config.isStageActive(2)) << "no such stage";

    config.clearStagesBelow(1);
    EXPECT_TRUE(config.isStageActive(0));
    EXPECT_FALSE(config.isStageActive(1));
    config.setAllStages();
    config.clearStagesAbove(1);
    EXPECT_FALSE(config.isStageActive(0));
    EXPECT_TRUE(config.isStageActive(1));
    EXPECT_EQ(config.getBottomStage(), m_bottom);
    EXPECT_EQ(config.getActiveStages(), std::vector<AxialStage*>{m_bottom});
    EXPECT_EQ(config.getAllStages(), (std::vector<AxialStage*>{m_top, m_bottom}));

    // Unknown and negative stage numbers are ignored.
    const ModId before = config.getModId();
    config.setStageActive(5, true);
    config.setStageActive(-1, false);
    config.toggleStage(7);
    EXPECT_EQ(config.getModId(), before);
    EXPECT_FALSE(config.isStageActive(0));

    config.clearAllStages();
    EXPECT_EQ(config.getBottomStage(), nullptr);
    EXPECT_TRUE(config.getActiveStages().empty());
    EXPECT_TRUE(config.isStageActive(-1));
}

TEST_F(ConfigurationTest, AStageWithoutChildrenIsInactive)
{
    AxialStage&          empty  = m_rocket.addChild(std::make_unique<AxialStage>());
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_EQ(config.getStageCount(), 3);
    EXPECT_FALSE(config.isStageActive(empty.getStageNumber()));
    EXPECT_FALSE(empty.isStageActive());
    empty.addChild(TestBodyComponent::make(0.1, 0.02));
    EXPECT_TRUE(config.isStageActive(empty.getStageNumber()));
    EXPECT_TRUE(empty.isStageActive());
    EXPECT_TRUE(empty.isStageActive(config));
}

TEST_F(ConfigurationTest, FlagsFollowTheirStageById)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    config.setStageActive(0, false);
    // Move the top stage to the bottom: its flag follows it to its new number.
    m_rocket.moveChild(m_top, 1);
    EXPECT_EQ(m_top->getStageNumber(), 1);
    EXPECT_FALSE(config.isStageActive(1));
    EXPECT_TRUE(config.isStageActive(0));
}

TEST_F(ConfigurationTest, SubStagesFollowTheirStage)
{
    ParallelStage& boosters = m_mount->addChild(std::make_unique<ParallelStage>());
    boosters.addChild(TestBodyComponent::make(0.1, 0.01));
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    ASSERT_EQ(boosters.getStageNumber(), 2);

    config.setStageActive(1, false);
    EXPECT_FALSE(config.isStageActive(2)) << "the sub-stage too";
    config.setStageActive(1, true, false);
    EXPECT_TRUE(config.isStageActive(1));
    EXPECT_FALSE(config.isStageActive(2)) << "without its sub-stages";
    config.toggleStage(1);
    EXPECT_FALSE(config.isStageActive(1));
    EXPECT_FALSE(config.isStageActive(2));
    config.toggleStage(1);
    EXPECT_TRUE(config.isStageActive(2)) << "toggling takes the sub-stages along";

    // setOnlyStage() and clearStagesBelow() leave the sub-stages' own flags.
    config.setOnlyStage(1);
    EXPECT_FALSE(config.isStageActive(2));
}

TEST_F(ConfigurationTest, AnInactiveBoosterSetIsRenderedWhenItsFlagIsSet)
{
    ParallelStage&       boosters = m_mount->addChild(std::make_unique<ParallelStage>());
    FlightConfiguration& config   = m_rocket.getSelectedConfiguration();
    // No children: inactive, but still drawn (OpenRocket issue #1980).
    EXPECT_FALSE(config.isStageActive(boosters.getStageNumber()));
    EXPECT_FALSE(config.getActiveInstances().containsKey(boosters));
    EXPECT_EQ(config.getExtraRenderInstances().count(boosters), 2);

    config.setStageActive(boosters.getStageNumber(), false);
    EXPECT_TRUE(config.getExtraRenderInstances().isEmpty());
}

TEST_F(ConfigurationTest, PreloadedActivenessIsAppliedOnce)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    config.preloadStageActiveness(1, false);
    config.preloadStageActiveness(0, true);
    ASSERT_TRUE(config.getPreloadedStageActiveness().has_value());
    EXPECT_EQ(config.getPreloadedStageActiveness(),
              (std::optional<std::map<int, bool>>{std::map<int, bool>{{0, true}, {1, false}}}));
    EXPECT_TRUE(config.isStageActive(1)) << "not applied yet";

    const FlightConfiguration clone = config.clone();
    EXPECT_EQ(clone.getPreloadedStageActiveness(), config.getPreloadedStageActiveness());

    config.applyPreloadedStageActiveness();
    EXPECT_TRUE(config.isStageActive(0));
    EXPECT_FALSE(config.isStageActive(1));
    EXPECT_FALSE(config.getPreloadedStageActiveness().has_value());
    config.applyPreloadedStageActiveness();  // nothing left
    EXPECT_FALSE(config.isStageActive(1));
}

TEST_F(ConfigurationTest, ComponentLists)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_EQ(config.getAllComponents(),
              (std::vector<RocketComponent*>{&m_rocket, m_top, m_topBody, m_bottom, m_mount}));
    config.setStageActive(0, false);
    EXPECT_EQ(config.getAllActiveComponents(),
              (std::vector<RocketComponent*>{&m_rocket, m_bottom, m_mount}));
    EXPECT_EQ(config.getActiveComponents(), (std::vector<RocketComponent*>{m_bottom, m_mount}));
    EXPECT_FALSE(config.isComponentActive(*m_topBody));
    EXPECT_TRUE(config.isComponentActive(*m_mount));
    EXPECT_TRUE(config.isComponentActive(m_rocket));
}

TEST_F(ConfigurationTest, ActiveComponentsAreBreadthFirstOverTheActiveStages)
{
    TestBodyComponent&   inner  = m_topBody->addChild(TestBodyComponent::make(0.05, 0.01));
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    // Both stages first, then their children, then the grandchildren (Java's queue).
    EXPECT_EQ(config.getActiveComponents(),
              (std::vector<RocketComponent*>{m_top, m_bottom, m_topBody, m_mount, &inner}));
}

TEST_F(ConfigurationTest, MotorsFollowTheMountsAndStages)
{
    const FlightConfigurationId fcid;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(fcid);
    EXPECT_FALSE(config.hasMotors());

    m_mount->addMotor(fcid, motorD21(), 3);
    m_rocket.fireComponentChangeEvent(QtRocket::MotorConfigurationSet::kDefaultMotorEventType);
    ASSERT_TRUE(config.hasMotors());
    ASSERT_EQ(config.getAllMotors().size(), 1U);
    EXPECT_EQ(config.getActiveMotors().size(), 1U);
    EXPECT_EQ(config.getActiveMotors().front().getMid(), m_mount->getMotorConfig(fcid).getMid());
    EXPECT_TRUE(m_rocket.hasMotors(fcid));
    EXPECT_FALSE(m_rocket.hasMotors(FlightConfigurationId::defaultValueId()));
    EXPECT_FALSE(m_rocket.hasMotors(FlightConfigurationId::errorId()));

    // An inactive stage's motors are not flown.
    config.setStageActive(1, false);
    EXPECT_TRUE(config.getActiveMotors().empty());
    EXPECT_TRUE(config.getAllMotors().empty()) << "Java collects the active stages' mounts only";

    // A mount that does not act as one has no motors.
    config.setAllStages();
    m_mount->setMotorMount(false);
    EXPECT_FALSE(config.hasMotors());
    EXPECT_FALSE(m_rocket.hasMotors(fcid));
    m_mount->setMotorMount(true);
    EXPECT_TRUE(config.hasMotors());

    // clearAllMotors() resets the mounts for this configuration.
    config.clearAllMotors();
    EXPECT_FALSE(config.hasMotors());
    EXPECT_TRUE(m_mount->getMotorConfig(fcid).isEmpty());
}

TEST_F(ConfigurationTest, AddMotorReplacesByIdAndBumpsTheModId)
{
    FlightConfiguration&        config = m_rocket.getSelectedConfiguration();
    const FlightConfigurationId fcid   = config.getId();
    MotorConfiguration          motor{*m_mount, fcid};
    motor.setMotor(motorC6());
    const ModId before = config.getModId();
    config.addMotor(motor);
    EXPECT_NE(config.getModId(), before);
    motor.setEjectionDelay(5);
    config.addMotor(motor);
    ASSERT_EQ(config.getAllMotors().size(), 1U) << "one entry per motor configuration id";
    EXPECT_EQ(config.getAllMotors().front().getEjectionDelay(), 5.0);
    EXPECT_TRUE(config.getActiveMotors().empty()) << "addMotor() does not touch the active motors";
}

TEST_F(ConfigurationTest, CopyCopiesTheMotorsIntoTheMounts)
{
    const FlightConfigurationId source;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(source);
    m_mount->addMotor(source, motorD21(), 3).setIgnitionDelay(1);
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    config.setName("Named");
    config.setStageActive(0, false, false);

    const FlightConfigurationId target;
    FlightConfiguration         copy = config.copy(target);
    EXPECT_EQ(copy.getId(), target);
    EXPECT_EQ(copy.getNameRaw(m_prefs), "Named");
    EXPECT_FALSE(copy.isStageActive(0));
    EXPECT_TRUE(copy.isStageActive(1));
    // The mount now holds a copy of the motor under the new id.
    const MotorConfiguration& copied = m_mount->getMotorConfig(target);
    EXPECT_FALSE(copied.isEmpty());
    EXPECT_EQ(copied.getFcid(), target);
    EXPECT_EQ(copied.getMotor(), m_mount->getMotorConfig(source).getMotor());
    EXPECT_EQ(copied.getIgnitionDelay(), 1.0);
    ASSERT_EQ(copy.getActiveMotors().size(), 1U);
    EXPECT_EQ(copy.getActiveMotors().front().getMid(), copied.getMid());

    // Stored in the rocket, it is a configuration like the others.
    m_rocket.setFlightConfiguration(target, std::move(copy));
    EXPECT_TRUE(m_rocket.containsFlightConfigurationId(target));
    EXPECT_EQ(m_rocket.getFlightConfiguration(target).getActiveMotors().size(), 1U);
}

TEST_F(ConfigurationTest, CloneForAnotherRocketMatchesStagesByNumber)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    config.setStageActive(1, false);
    config.setName("Custom");

    const std::unique_ptr<Rocket> copy  = m_rocket.copyRocketWithOriginalId();
    const FlightConfiguration     clone = config.clone(*copy);
    EXPECT_EQ(&clone.getRocket(), copy.get());
    EXPECT_EQ(clone.getId(), config.getId());
    EXPECT_EQ(clone.getNameRaw(m_prefs), "Custom");
    EXPECT_TRUE(clone.isStageActive(0));
    EXPECT_FALSE(clone.isStageActive(1));
}

TEST_F(ConfigurationTest, Names)
{
    FlightConfiguration& config = m_rocket.createFlightConfiguration(FlightConfigurationId{});
    EXPECT_FALSE(config.getStoredName().has_value());
    config.setName("My flight");
    EXPECT_EQ(config.getNameRaw(m_prefs), "My flight");
    EXPECT_EQ(config.getName(m_prefs), "My flight");
    EXPECT_TRUE(config.isNameOverridden(m_prefs));
    config.setName("");
    EXPECT_EQ(config.getNameRaw(m_prefs), "[{motors}]") << "empty: back to the default";
    EXPECT_FALSE(config.isNameOverridden(m_prefs));

    // The default is the preference's.
    InMemoryPreferences prefs;
    prefs.setDefaultFlightConfigName("{motors} flight");
    EXPECT_EQ(config.getNameRaw(prefs), "{motors} flight");
    EXPECT_EQ(config.getName(prefs), "No motors flight");
    config.setName("[{motors}]");
    EXPECT_TRUE(config.isNameOverridden(prefs));
    EXPECT_FALSE(config.isNameOverridden(m_prefs)) << "equal to the default name";

    // The error id cannot be named.
    FlightConfiguration error{m_rocket, FlightConfigurationId::errorId()};
    error.setName("Broken");
    EXPECT_FALSE(error.getStoredName().has_value());
}

TEST_F(ConfigurationTest, NameSubstitutionDetails)
{
    const FlightConfigurationId fcid;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(fcid);
    m_mount->addMotor(fcid, motorC6(), 5);
    TestMotorMount& second =
        m_topBody->addChild(TestMotorMount::make(0.1, 0.01, ComponentKind::INNER_TUBE));
    second.setMotorCount(2);
    second.addMotor(fcid, motorD21(), 3);
    TestMotorMount& third =
        m_topBody->addChild(TestMotorMount::make(0.1, 0.01, ComponentKind::INNER_TUBE));
    third.addMotor(fcid, motorC6(), 3);
    TestMotorMount& empty =
        m_topBody->addChild(TestMotorMount::make(0.1, 0.01, ComponentKind::INNER_TUBE));
    empty.setMotorMount(true);
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);

    // Different motors in one stage: counted and sorted (Java's String order), the empty mount
    // left out.
    config.setName("{motors}");
    EXPECT_EQ(config.getName(m_prefs), std::format("C6-3, 2{} D21-3; C6-5", kTimes));

    // A tag without a key is replaced too while another tag has one.
    config.setName("{foo} {motors}");
    EXPECT_EQ(config.getName(m_prefs), std::format("No motors C6-3, 2{} D21-3; C6-5", kTimes));
    config.setName("{foo}");
    EXPECT_EQ(config.getName(m_prefs), "{foo}");

    // The key must end right before the closing brace, ignoring blanks at the start only.
    config.setName("{ motors}");
    EXPECT_EQ(config.getName(m_prefs), std::format("C6-3, 2{} D21-3; C6-5", kTimes));
    config.setName("{motors }");
    EXPECT_EQ(config.getName(m_prefs), "{motors }");

    // A tag cannot span lines.
    config.setName("{mo\ntors} {cases}");
    EXPECT_EQ(config.getName(m_prefs),
              std::format("{{mo\ntors}} 3{} SU 18.0x70.0; SU 18.0x70.0", kTimes));

    // The common name when the preferences say so (the same here).
    InMemoryPreferences commonNames;
    commonNames.setMotorNameColumn(false);
    config.setName("{motors}");
    EXPECT_EQ(config.getName(commonNames), std::format("C6-3, 2{} D21-3; C6-5", kTimes));

    // Words containing a key are not keys.
    config.setName("{xmotors}");
    EXPECT_EQ(config.getName(m_prefs), "No motors") << "contains 'motors': substituted, no key";
}

TEST_F(ConfigurationTest, InstanceMapOrderIsPreOrder)
{
    const FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_EQ(config.getActiveInstances().keys(),
              (std::vector<RocketComponent*>{&m_rocket, m_top, m_topBody, m_bottom, m_mount}));
}

TEST_F(ConfigurationTest, ModIdsFollowFunctionalChanges)
{
    FlightConfiguration&        selected = m_rocket.getSelectedConfiguration();
    const FlightConfigurationId otherId;
    FlightConfiguration&        other = m_rocket.createFlightConfiguration(otherId);

    ModId selectedBefore = selected.getModId();
    ModId otherBefore    = other.getModId();
    m_rocket.setDesigner("Nobody");  // NONFUNCTIONAL_CHANGE
    EXPECT_EQ(selected.getModId(), selectedBefore);
    EXPECT_EQ(other.getModId(), otherBefore);

    m_topBody->setLength(0.4);  // a functional change: every configuration
    EXPECT_NE(selected.getModId(), selectedBefore);
    EXPECT_NE(other.getModId(), otherBefore);

    selectedBefore = selected.getModId();
    otherBefore    = other.getModId();
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, otherId);
    EXPECT_EQ(selected.getModId(), selectedBefore) << "only the configurations given";
    EXPECT_NE(other.getModId(), otherBefore);

    // A flag change draws a new id.
    otherBefore = other.getModId();
    other.toggleStage(0);
    EXPECT_NE(other.getModId(), otherBefore);
    otherBefore = other.getModId();
    other.updateModId();
    EXPECT_NE(other.getModId(), otherBefore);
    EXPECT_EQ(other.modId(), other.getModId());
}

TEST_F(ConfigurationTest, LengthAndBounds)
{
    const FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_NEAR(config.getLength(), 0.5, 1e-12);
    EXPECT_NEAR(config.getLengthAerodynamic(), 0.5, 1e-12);
    EXPECT_NEAR(m_rocket.getLength(), 0.5, 1e-12);
    EXPECT_NEAR(QtRocket::RocketUtils::getLength(m_rocket), 0.5, 1e-12);

    const BoundingBox box = m_rocket.getBoundingBox();
    EXPECT_TRUE(near(box.min(), Coordinate{0, -0.02, -0.02}, 1e-12));
    EXPECT_TRUE(near(box.max(), Coordinate{0.5, 0.02, 0.02}, 1e-12));
    EXPECT_EQ(config.getBounds(), box.toCollection());

    // A non-aerodynamic component counts in the bounds only (the lengths are cached until the
    // rocket changes).
    m_mount->setAerodynamic(false);
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
    EXPECT_NEAR(config.getLengthAerodynamic(), 0.3, 1e-12);
    EXPECT_NEAR(config.getLength(), 0.5, 1e-12);
    m_mount->setAerodynamic(true);
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);

    // The length follows the tree.
    m_topBody->setLength(0.4);
    EXPECT_NEAR(m_rocket.getLength(), 0.6, 1e-12);

    // Only active stages count.
    m_rocket.getSelectedConfiguration().setStageActive(1, false);
    EXPECT_NEAR(m_rocket.getLength(), 0.4, 1e-12);

    // As in Java, the "all stages" setters keep the cached length (they draw no new id)...
    m_rocket.getSelectedConfiguration().clearAllStages();
    EXPECT_NEAR(m_rocket.getLength(), 0.4, 1e-12);
    // ...until the bounds are computed again: without any active component, no length and a
    // unit box.
    EXPECT_EQ(config.getBoundingBox(), (BoundingBox{Coordinate::kZero, Coordinate::kXUnit}));
    EXPECT_EQ(m_rocket.getLength(), 0.0);
    EXPECT_EQ(config.getBoundingBoxAerodynamic(),
              (BoundingBox{Coordinate::kZero, Coordinate::kXUnit}));
    EXPECT_EQ(QtRocket::RocketUtils::getLength(m_rocket), 1.0);
}

TEST_F(ConfigurationTest, LegacyBoundsAreTheComponentsOwnUntransformed)
{
    // A component that is not BoxBounded (OpenRocket's mass objects) counts with its own
    // untransformed bounds, once (Java's calculateBounds() quirk).
    TestComponent& mass = m_mount->addChild(
        TestComponent::make(0.05, ComponentKind::MASS_COMPONENT, AxialMethod::BOTTOM));
    mass.setOuterRadius(0.5);
    mass.setAerodynamic(false);
    mass.setInstances({Coordinate{0, 0.1, 0}, Coordinate{0, -0.1, 0}}, {0, 0});
    const FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    const BoundingBox          box    = config.getBoundingBox();
    EXPECT_TRUE(near(box.min(), Coordinate{0, -0.5, -0.5}, 1e-12));
    EXPECT_TRUE(near(box.max(), Coordinate{0.5, 0.5, 0.5}, 1e-12));
    EXPECT_NEAR(config.getLengthAerodynamic(), 0.5, 1e-12);
}

TEST_F(ConfigurationTest, ReferenceLength)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    // MAXIMUM: twice the largest radius.
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.04);
    EXPECT_DOUBLE_EQ(config.getReferenceArea(), std::numbers::pi * 0.02 * 0.02);

    m_mount->setOuterRadius(0.03);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.06) << "recomputed after a change";

    m_rocket.setReferenceType(ReferenceType::NOSECONE);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.04) << "the first body's fore radius";

    m_topBody->setForeAftRadii(0.0001, 0.025);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.05) << "else its aft radius";
    m_topBody->setForeAftRadii(0.0001, 0.0001);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.06) << "else the next body";

    m_rocket.setCustomReferenceLength(0.123);
    m_rocket.setReferenceType(ReferenceType::CUSTOM);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), 0.123);

    // Without symmetric components: the default.
    config.clearAllStages();
    m_rocket.setReferenceType(ReferenceType::MAXIMUM);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), Rocket::kDefaultReferenceLength);
    m_rocket.setReferenceType(ReferenceType::NOSECONE);
    EXPECT_DOUBLE_EQ(config.getReferenceLength(), Rocket::kDefaultReferenceLength);
    EXPECT_DOUBLE_EQ(QtRocket::getReferenceLength(ReferenceType::MAXIMUM, config),
                     Rocket::kDefaultReferenceLength);
}

TEST_F(ConfigurationTest, RemovedComponentsLeaveTheConfigurationsAtOnce)
{
    const FlightConfigurationId fcid;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(fcid);
    m_mount->addMotor(fcid, motorD21());
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    ASSERT_TRUE(config.hasMotors());

    // Even with events disabled, when no update follows.
    m_rocket.enableEvents(false);
    std::unique_ptr<RocketComponent> removed = m_rocket.removeChild(m_bottom);
    ASSERT_NE(removed, nullptr);
    EXPECT_FALSE(config.getActiveInstances().containsKey(*m_bottom));
    EXPECT_FALSE(config.getActiveInstances().containsKey(*m_mount));
    EXPECT_FALSE(config.hasMotors());
    EXPECT_TRUE(config.getActiveMotors().empty());
    removed.reset();  // destroys the components
    EXPECT_NEAR(config.getLength(), 0.3, 1e-12);
}

TEST_F(ConfigurationTest, DebugStrings)
{
    FlightConfiguration& config = m_rocket.getSelectedConfiguration();
    EXPECT_EQ(config.toDebug(m_prefs),
              std::format("DefaultKey (#{}) [No motors]", config.getConfigurationInstanceId()));

    const std::string stages = config.toStageListDetail(m_prefs);
    EXPECT_TRUE(stages.starts_with(
        std::format("\nDumping 2 stages for config: [No motors]: (DefaultKey)(#: {})\n"
                    "    [# ][?actv]:   Name \n",
                    config.getConfigurationInstanceId())))
        << stages;
    EXPECT_NE(stages.find("    [" + m_top->getId().toString() + "][  on]:  Stage \n"),
              std::string::npos)
        << stages;

    const std::string motors = config.toMotorDetail(m_prefs);
    EXPECT_EQ(motors, std::format("\nDumping  0 Motors for configuration [No motors] "
                                  "(DefaultKey)(#: {})\n\n",
                                  config.getConfigurationInstanceId()));
}

// ----------------------------------------------------------------- instance composition

/// Composes the instances through two levels of instanced assemblies: a three-booster set whose
/// bodies each carry a two-pod set.
class NestedInstancesTest : public ::testing::Test
{
protected:
    NestedInstancesTest()
    {
        AxialStage& core = m_rocket.addChild(std::make_unique<AxialStage>());
        m_coreBody       = &core.addChild(TestBodyComponent::make(1.0, 0.05));
        m_boosters       = &m_coreBody->addChild(std::make_unique<ParallelStage>(3));
        m_boosterBody    = &m_boosters->addChild(TestBodyComponent::make(0.6, 0.03));
        m_pods           = &m_boosterBody->addChild(std::make_unique<PodSet>());
        m_podBody        = &m_pods->addChild(TestBodyComponent::make(0.2, 0.01));
        m_rocket.enableEvents();
        m_boosters->setRadius(RadiusMethod::FREE, 0.2);
        m_boosters->setAngleOffset(0.3);
        m_pods->setRadius(RadiusMethod::FREE, 0.05);
        m_pods->setAngleOffset(0.1);
    }

    Rocket             m_rocket;
    TestBodyComponent* m_coreBody{nullptr};
    ParallelStage*     m_boosters{nullptr};
    TestBodyComponent* m_boosterBody{nullptr};
    PodSet*            m_pods{nullptr};
    TestBodyComponent* m_podBody{nullptr};
};

TEST_F(NestedInstancesTest, CountsMultiply)
{
    const InstanceMap& instances = m_rocket.getSelectedConfiguration().getActiveInstances();
    EXPECT_EQ(instances.count(*m_boosters), 3);
    EXPECT_EQ(instances.count(*m_boosterBody), 3);
    EXPECT_EQ(instances.count(*m_pods), 6);
    EXPECT_EQ(instances.count(*m_podBody), 6);
}

/// Whether @p context is pod @p j of booster @p i: booster i sits at angle 0.3 + 2 pi i / 3,
/// radius 0.2, and its pod j at angle 0.1 + pi j around it, radius 0.05, both angles about the
/// rocket's axis (the pod offset turns with the booster), at x = @p podX; its parent transform is
/// the booster body's instance @p boosterBody.
::testing::AssertionResult isPod(const InstanceContext& context, const InstanceContext& boosterBody,
                                 std::size_t i, std::size_t j, double podX)
{
    if (std::cmp_not_equal(context.instanceNumber, j))
    {
        return ::testing::AssertionFailure() << "instance " << context.instanceNumber;
    }
    const double     boosterAngle = 0.3 + (2 * std::numbers::pi * static_cast<double>(i) / 3);
    const double     angle = boosterAngle + 0.1 + (std::numbers::pi * static_cast<double>(j));
    const Coordinate expected{podX, (0.2 * std::cos(boosterAngle)) + (0.05 * std::cos(angle)),
                              (0.2 * std::sin(boosterAngle)) + (0.05 * std::sin(angle))};
    const ::testing::AssertionResult atExpected = near(context.getLocation(), expected, 1e-12);
    if (!atExpected)
    {
        return atExpected;
    }
    return near(context.getParentTransform().transform(Coordinate::kZero),
                boosterBody.getLocation(), 1e-12);
}

TEST_F(NestedInstancesTest, ContextsComposeParentPositionOffsetAndAngle)
{
    const InstanceMap& instances = m_rocket.getSelectedConfiguration().getActiveInstances();
    const std::span<const InstanceContext> pods = instances.getInstanceContexts(*m_pods);
    ASSERT_EQ(pods.size(), 6U);

    const double boosterX = m_boosters->getComponentLocations().at(0).x;
    const double podX     = boosterX + m_boosterBody->getPosition().x + m_pods->getPosition().x;
    const std::span<const InstanceContext> bodies = instances.getInstanceContexts(*m_boosterBody);
    ASSERT_EQ(bodies.size(), 3U);
    for (std::size_t i = 0; i < 3; i++)
    {
        for (std::size_t j = 0; j < 2; j++)
        {
            EXPECT_TRUE(isPod(pods[(i * 2) + j], bodies[i], i, j, podX)) << i << ", " << j;
        }
    }
}

TEST_F(NestedInstancesTest, ContextsMatchTheAbsoluteLocations)
{
    // The configuration's instance walk and RocketComponent::getComponentLocations() place every
    // instance at the same points (in different orders).
    const InstanceMap& instances = m_rocket.getSelectedConfiguration().getActiveInstances();
    for (const auto& [component, contexts] : instances)
    {
        std::vector<Coordinate> fromContexts;
        for (const InstanceContext& context : contexts)
        {
            fromContexts.push_back(context.getLocation());
        }
        std::vector<Coordinate> fromLocations = component->getComponentLocations();
        ASSERT_EQ(fromContexts.size(), fromLocations.size()) << component->getName();
        const auto order = [](const Coordinate& a, const Coordinate& b) {
            return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
        };
        std::ranges::sort(fromContexts, order);
        std::ranges::sort(fromLocations, order);
        for (std::size_t k = 0; k < fromContexts.size(); k++)
        {
            EXPECT_TRUE(near(fromContexts[k], fromLocations[k], 1e-12)) << component->getName();
        }
    }
}

TEST_F(NestedInstancesTest, BoundsCoverEveryInstance)
{
    const BoundingBox box = m_rocket.getSelectedConfiguration().getBoundingBox();
    // The farthest pod reaches 0.2 + 0.05 + 0.01 from the axis at most.
    EXPECT_LE(box.max().y, 0.2 + 0.05 + 0.015) << "a rotated box grows by its corners";
    EXPECT_GT(box.max().y, 0.2);
    EXPECT_NEAR(box.max().x, 1.0, 1e-12) << "the core body is the longest";
}

}  // namespace
