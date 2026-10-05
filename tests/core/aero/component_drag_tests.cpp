// The free functions of aero/ComponentDrag.h: Java's RocketComponent.getComponentCD() and the
// calculating half of RocketComponent.getOverrideCD().
//
// The pinned values were printed by the Java probe CalcProbe.java (componentDrag()), which calls
// those two methods on every component of a rocket. OpenRocket's test preferences give a default
// Mach number of 0, so its getOverrideCD() is compared with getOverrideCD(component, 0.0).

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "aero/BarrowmanTestRockets.h"
#include "aero/ForcePins.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BarrowmanCalculator;
using QtRocket::BodyTube;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::allComponents;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::kRocketTolerance;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
namespace ComponentDrag = QtRocket::ComponentDrag;

struct ComponentCDPin
{
    /// The component's index in allComponents(rocket), and its name.
    int              index;
    std::string_view name;
    /// getComponentCD(0, 0, 0.3, 0).
    double cd;
    /// getComponentCD(0.1, 0.5, 0.8, 10).
    double cdInFlight;
    /// Java's getOverrideCD() with OpenRocket's test preferences (a default Mach number of 0).
    double overrideCD;
};

/// The differences between the CDs of the components of @p rocket and OpenRocket's.
[[nodiscard]] std::string componentCDDifferences(Rocket&                         rocket,
                                                 std::span<const ComponentCDPin> pins)
{
    JavaValueDifferences                diff;
    const std::vector<RocketComponent*> components = allComponents(rocket);
    if (components.size() != pins.size())
    {
        diff.problem(std::format("{} components, {} pins", components.size(), pins.size()));
        return diff.text();
    }
    for (const ComponentCDPin& pin : pins)
    {
        const RocketComponent& component = *components.at(static_cast<std::size_t>(pin.index));
        const std::string      what{pin.name};
        diff.name(std::format("component {}", pin.index), pin.name, component.getName());
        const double storedOverrideCD = component.getOverrideCD();
        diff.pinned(what + ": CD", pin.cd, ComponentDrag::getComponentCD(component, 0, 0, 0.3, 0),
                    kRocketTolerance);
        diff.pinned(what + ": CD in flight", pin.cdInFlight,
                    ComponentDrag::getComponentCD(component, 0.1, 0.5, 0.8, 10), kRocketTolerance);
        diff.pinned(what + ": override CD", pin.overrideCD,
                    ComponentDrag::getOverrideCD(component, 0.0), kRocketTolerance);
        if (component.getOverrideCD() != storedOverrideCD)
        {
            diff.problem(what + ": the stored override CD was changed");
        }
    }
    return diff.text();
}

TEST(ComponentDrag, CDsOfTheAlphasComponentsAreOpenRockets)
{
    // The rocket's and the stage's CD is the total; an internal component has none.
    static const std::array<ComponentCDPin, 10> kPins{{
        {.index      = 0,
         .name       = "Estes Alpha III / Code Verification Rocket",
         .cd         = 1.541755469408005,
         .cdInFlight = 1.7840261026658784,
         .overrideCD = 2.1164448288890725},
        {.index      = 1,
         .name       = "Stage",
         .cd         = 1.541755469408005,
         .cdInFlight = 1.7840261026658784,
         .overrideCD = 2.1164448288890725},
        {.index      = 2,
         .name       = "Nose Cone",
         .cd         = 0.048735737950262335,
         .cdInFlight = 0.04603092908319508,
         .overrideCD = 0.12232863559874205},
        {.index      = 3,
         .name       = "Body Tube",
         .cd         = 0.3369760949254841,
         .cdInFlight = 0.39708337522729875,
         .overrideCD = 0.6352511415525116},
        {.index      = 4,
         .name       = "3 Fin Set",
         .cd         = 0.3719224693071831,
         .cdInFlight = 0.4320997167627869,
         .overrideCD = 0.45295501724593956},
        {.index      = 5,
         .name       = "Launch Lugs",
         .cd         = 0.04027622861070913,
         .cdInFlight = 0.04461264806702381,
         .overrideCD = 0.0},
        {.index = 6, .name = "Motor Mount Tube", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 7, .name = "Engine Block", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 8, .name = "Parachute", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 9, .name = "Centering Rings", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
    }};
    const TestEstesAlphaIII                     alpha;
    EXPECT_EQ(componentCDDifferences(*alpha.rocket, kPins), "");
}

TEST(ComponentDrag, AnOverriddenComponentHasItsOverrideCD)
{
    // The body tube's CD is overridden (0.3); the fin set has a stored override CD (0.11) that
    // is not in use, so its CD is the calculated one.
    static const std::array<ComponentCDPin, 10> kBodyPins{{
        {.index      = 0,
         .name       = "Estes Alpha III / Code Verification Rocket",
         .cd         = 1.510694142066814,
         .cdInFlight = 1.6925292284061868,
         .overrideCD = 1.7960399886115888},
        {.index      = 1,
         .name       = "Stage",
         .cd         = 1.510694142066814,
         .cdInFlight = 1.6925292284061868,
         .overrideCD = 1.7960399886115888},
        {.index      = 2,
         .name       = "Nose Cone",
         .cd         = 0.05465050553455577,
         .cdInFlight = 0.051617430050802195,
         .overrideCD = 0.13717493687377005},
        {.index = 3, .name = "Body Tube", .cd = 0.3, .cdInFlight = 0.3, .overrideCD = 0.3},
        {.index      = 4,
         .name       = "3 Fin Set",
         .cd         = 0.3719224693071831,
         .cdInFlight = 0.4320997167627869,
         .overrideCD = 0.45295501724593956},
        {.index      = 5,
         .name       = "Launch Lugs",
         .cd         = 0.04027622861070913,
         .cdInFlight = 0.04461264806702381,
         .overrideCD = 0.0},
        {.index = 6, .name = "Motor Mount Tube", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 7, .name = "Engine Block", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 8, .name = "Parachute", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 9, .name = "Centering Rings", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
    }};
    // With the override covering what the body tube holds, those components have a CD of 0.
    static const std::array<ComponentCDPin, 10> kBodyAndChildrenPins{{
        {.index      = 0,
         .name       = "Estes Alpha III / Code Verification Rocket",
         .cd         = 0.35465050553455574,
         .cdInFlight = 0.3516174300508022,
         .overrideCD = 0.43717493687377007},
        {.index      = 1,
         .name       = "Stage",
         .cd         = 0.35465050553455574,
         .cdInFlight = 0.3516174300508022,
         .overrideCD = 0.43717493687377007},
        {.index      = 2,
         .name       = "Nose Cone",
         .cd         = 0.05465050553455577,
         .cdInFlight = 0.051617430050802195,
         .overrideCD = 0.13717493687377005},
        {.index = 3, .name = "Body Tube", .cd = 0.3, .cdInFlight = 0.3, .overrideCD = 0.3},
        {.index = 4, .name = "3 Fin Set", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 5, .name = "Launch Lugs", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 6, .name = "Motor Mount Tube", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 7, .name = "Engine Block", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 8, .name = "Parachute", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 9, .name = "Centering Rings", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
    }};
    const TestEstesAlphaIII                     alpha;
    alpha.body->setCDOverridden(true);
    alpha.body->setOverrideCD(0.3);
    alpha.fins->setOverrideCD(0.11);
    EXPECT_EQ(componentCDDifferences(*alpha.rocket, kBodyPins), "");

    alpha.body->setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(componentCDDifferences(*alpha.rocket, kBodyAndChildrenPins), "");
}

TEST(ComponentDrag, CDsOfTheFalconsComponentsAreOpenRockets)
{
    // The CD of a component with several instances is that of one instance; a stage's is that
    // of its descendants (the core stage's includes both boosters).
    static const std::array<ComponentCDPin, 16> kPins{{
        {.index      = 0,
         .name       = "Falcon9H Scale Rocket",
         .cd         = 0.822394311575928,
         .cdInFlight = 0.9749638440633636,
         .overrideCD = 2.104759356998336},
        {.index      = 1,
         .name       = "Payload Fairing Stage",
         .cd         = 0.13692523470946266,
         .cdInFlight = 0.16493440081909755,
         .overrideCD = 0.32288960312441617},
        {.index      = 2,
         .name       = "PL Fairing Nose",
         .cd         = 0.0142382921023787,
         .cdInFlight = 0.013448074074496934,
         .overrideCD = 0.04941156449150655},
        {.index      = 3,
         .name       = "PL Fairing Body",
         .cd         = 0.022477049702254344,
         .cdInFlight = 0.021229584784369388,
         .overrideCD = 0.0780027676743753},
        {.index      = 4,
         .name       = "PL Fairing Transition",
         .cd         = 0.062387934271228566,
         .cdInFlight = 0.0945338829480712,
         .overrideCD = 0.06422061381415273},
        {.index      = 5,
         .name       = "Upper Stage Body",
         .cd         = 0.022693175180160636,
         .cdInFlight = 0.02143371540729602,
         .overrideCD = 0.07875279428662893},
        {.index = 6, .name = "Parachute", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 7, .name = "Shock Cord", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index      = 8,
         .name       = "Interstage",
         .cd         = 0.015128783453440424,
         .cdInFlight = 0.014289143604864014,
         .overrideCD = 0.052501862857752615},
        {.index      = 9,
         .name       = "Core Stage",
         .cd         = 0.6854690768664653,
         .cdInFlight = 0.810029443244266,
         .overrideCD = 1.7818697538739197},
        {.index      = 10,
         .name       = "Core Stage Body",
         .cd         = 0.1730524635308257,
         .cdInFlight = 0.2066489751172394,
         .overrideCD = 0.41579274449547116},
        {.index      = 11,
         .name       = "Booster Stage",
         .cd         = 0.2562083066678198,
         .cdInFlight = 0.30169023406351325,
         .overrideCD = 0.6830385046892243},
        {.index      = 12,
         .name       = "Booster Nose",
         .cd         = 0.007222064355462051,
         .cdInFlight = 0.006821243427560525,
         .overrideCD = 0.025062942668672077},
        {.index      = 13,
         .name       = "Booster Body",
         .cd         = 0.1730524635308257,
         .cdInFlight = 0.2066489751172394,
         .overrideCD = 0.41579274449547116},
        {.index      = 14,
         .name       = "Booster Motor Tubes",
         .cd         = 0.0,
         .cdInFlight = 0.0,
         .overrideCD = 0.0},
        {.index      = 15,
         .name       = "Booster Fins",
         .cd         = 0.025311259593844008,
         .cdInFlight = 0.029406671839571116,
         .overrideCD = 0.0807276058416937},
    }};
    // The selected configuration decides: the components of an inactive stage have no CD.
    static const std::array<ComponentCDPin, 16> kWithoutBoostersPins{{
        {.index      = 0,
         .name       = "Falcon9H Scale Rocket",
         .cd         = 0.30997769824028837,
         .cdInFlight = 0.37158337593633695,
         .overrideCD = 0.7386823476198872},
        {.index      = 1,
         .name       = "Payload Fairing Stage",
         .cd         = 0.13692523470946266,
         .cdInFlight = 0.16493440081909755,
         .overrideCD = 0.32288960312441617},
        {.index      = 2,
         .name       = "PL Fairing Nose",
         .cd         = 0.0142382921023787,
         .cdInFlight = 0.013448074074496934,
         .overrideCD = 0.04941156449150655},
        {.index      = 3,
         .name       = "PL Fairing Body",
         .cd         = 0.022477049702254344,
         .cdInFlight = 0.021229584784369388,
         .overrideCD = 0.0780027676743753},
        {.index      = 4,
         .name       = "PL Fairing Transition",
         .cd         = 0.062387934271228566,
         .cdInFlight = 0.0945338829480712,
         .overrideCD = 0.06422061381415273},
        {.index      = 5,
         .name       = "Upper Stage Body",
         .cd         = 0.022693175180160636,
         .cdInFlight = 0.02143371540729602,
         .overrideCD = 0.07875279428662893},
        {.index = 6, .name = "Parachute", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 7, .name = "Shock Cord", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index      = 8,
         .name       = "Interstage",
         .cd         = 0.015128783453440424,
         .cdInFlight = 0.014289143604864014,
         .overrideCD = 0.052501862857752615},
        {.index      = 9,
         .name       = "Core Stage",
         .cd         = 0.1730524635308257,
         .cdInFlight = 0.2066489751172394,
         .overrideCD = 0.41579274449547116},
        {.index      = 10,
         .name       = "Core Stage Body",
         .cd         = 0.1730524635308257,
         .cdInFlight = 0.2066489751172394,
         .overrideCD = 0.41579274449547116},
        {.index = 11, .name = "Booster Stage", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 12, .name = "Booster Nose", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index = 13, .name = "Booster Body", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
        {.index      = 14,
         .name       = "Booster Motor Tubes",
         .cd         = 0.0,
         .cdInFlight = 0.0,
         .overrideCD = 0.0},
        {.index = 15, .name = "Booster Fins", .cd = 0.0, .cdInFlight = 0.0, .overrideCD = 0.0},
    }};
    const TestFalcon9Heavy                      falcon;
    EXPECT_EQ(componentCDDifferences(*falcon.rocket, kPins), "");

    falcon.rocket->getSelectedConfiguration().setStageActive(TestFalcon9Heavy::kBoosterStageNumber,
                                                             false);
    EXPECT_EQ(componentCDDifferences(*falcon.rocket, kWithoutBoostersPins), "");
}

TEST(ComponentDrag, AComponentWithoutARocketHasNoCD)
{
    // Java: getRocket() throws an IllegalStateException, which getComponentCD() catches.
    const BodyTube alone;
    EXPECT_EQ(ComponentDrag::getComponentCD(alone, 0, 0, 0.3, 0), 0.0);
    EXPECT_EQ(ComponentDrag::getOverrideCD(alone, 0.3), 0.0);

    AxialStage      stage;
    const BodyTube& inStage = stage.addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(ComponentDrag::getComponentCD(inStage, 0, 0, 0.3, 0), 0.0);
    EXPECT_EQ(ComponentDrag::getOverrideCD(inStage, 0.3), 0.0);
    EXPECT_EQ(ComponentDrag::getOverrideCD(stage, 0.3), 0.0);
}

TEST(ComponentDrag, ComponentCDIsTheForceAnalysisOfTheSelectedConfiguration)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    conditions.setAOA(0.1);
    conditions.setTheta(0.5);
    conditions.setMach(0.8);
    conditions.setRollRate(10);
    BarrowmanCalculator calculator;
    const ForceMap      analysis = calculator.getForceAnalysis(config, conditions, nullptr);

    ASSERT_NE(analysis.get(alpha.fins), nullptr);
    EXPECT_EQ(ComponentDrag::getComponentCD(*alpha.fins, 0.1, 0.5, 0.8, 10),
              analysis.get(alpha.fins)->getCD());
    ASSERT_NE(analysis.get(alpha.rocket.get()), nullptr);
    EXPECT_EQ(ComponentDrag::getComponentCD(*alpha.rocket, 0.1, 0.5, 0.8, 10),
              analysis.get(alpha.rocket.get())->getCD());
    // A component without an entry in the analysis: 0.
    EXPECT_EQ(analysis.get(alpha.inner), nullptr);
    EXPECT_EQ(ComponentDrag::getComponentCD(*alpha.inner, 0.1, 0.5, 0.8, 10), 0.0);
}

TEST(ComponentDrag, OverrideCDIsTheStoredOneOnlyWhileTheCDIsOverridden)
{
    const TestEstesAlphaIII alpha;
    RocketComponent&        fins = *alpha.fins;
    fins.setOverrideCD(0.42);

    // Not overridden: the calculated CD at the default Mach number, whatever is stored; the
    // stored value is left alone (Java stores the calculated one).
    ASSERT_FALSE(fins.isCDOverridden());
    const double calculated = ComponentDrag::getComponentCD(fins, 0, 0, 0.3, 0);
    EXPECT_GT(calculated, 0);
    EXPECT_NE(calculated, 0.42);
    EXPECT_EQ(ComponentDrag::getOverrideCD(fins, 0.3), calculated);
    EXPECT_NE(ComponentDrag::getOverrideCD(fins, 0.9), calculated);
    EXPECT_EQ(fins.getOverrideCD(), 0.42);

    // Overridden: the stored override CD, whatever the Mach number.
    fins.setCDOverridden(true);
    EXPECT_EQ(ComponentDrag::getOverrideCD(fins, 0.3), 0.42);
    EXPECT_EQ(ComponentDrag::getOverrideCD(fins, 0.9), 0.42);
    // ... which is the component's CD too.
    EXPECT_EQ(ComponentDrag::getComponentCD(fins, 0, 0, 0.3, 0), 0.42);

    // Switched off again, the calculated CD is back and the stored one stays.
    fins.setCDOverridden(false);
    EXPECT_EQ(ComponentDrag::getOverrideCD(fins, 0.3), calculated);
    EXPECT_EQ(fins.getOverrideCD(), 0.42);
}

TEST(ComponentDrag, TheDefaultMachNumberComesFromThePreferences)
{
    // What the GUI calls: the preferences' default Mach number (0.3 unless set).
    const QtRocket::InMemoryPreferences preferences;
    const TestEstesAlphaIII             alpha;
    EXPECT_EQ(preferences.getDefaultMach(), 0.3);
    EXPECT_EQ(ComponentDrag::getOverrideCD(*alpha.body, preferences.getDefaultMach()),
              ComponentDrag::getComponentCD(*alpha.body, 0, 0, 0.3, 0));
}

}  // namespace
