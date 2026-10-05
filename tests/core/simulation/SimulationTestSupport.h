#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "rocket/TestRockets.h"

/// What the tests of simulation/ share: a rocket to take event sources from and two helpers.
namespace QtRocket::Test
{

/// The rocket the tests of the flight events and the flight data take their event sources from,
/// the same one the Java probe that pinned their expectations built (probes/events-data-impl/
/// MiscProbe.java):
///
///     "Probe Rocket"
///       "Sustainer" (stage 0)
///         "Sustainer Body" (a body tube)
///           "Chute" (a parachute)
///           "Sustainer Mount" (an inner tube with an A8, ejection delay 3 s, in testFcid(0))
///       "Booster" (stage 1)
///         "Booster Body" (a body tube)
///           "Strap-ons" (a parallel stage, stage 2)
///             "Strap-on Body" (a body tube)
///
/// `state` is the motor state of the sustainer mount's motor, as an event carries it.
struct EventTestRocket
{
    Rocket                             rocket;
    AxialStage*                        sustainer{nullptr};
    BodyTube*                          sustainerBody{nullptr};
    Parachute*                         chute{nullptr};
    InnerTube*                         sustainerMount{nullptr};
    AxialStage*                        booster{nullptr};
    BodyTube*                          boosterBody{nullptr};
    ParallelStage*                     strapOns{nullptr};
    BodyTube*                          strapOnBody{nullptr};
    FlightConfigurationId              fcid{testFcid(0)};
    std::shared_ptr<MotorClusterState> state;

    EventTestRocket()
    {
        rocket.setName("Probe Rocket");
        sustainer = &rocket.addChild(std::make_unique<AxialStage>());
        sustainer->setName("Sustainer");
        sustainerBody = &sustainer->addChild(std::make_unique<BodyTube>(0.2, 0.02));
        sustainerBody->setName("Sustainer Body");
        chute = &sustainerBody->addChild(std::make_unique<Parachute>());
        chute->setName("Chute");
        sustainerMount = &sustainerBody->addChild(std::make_unique<InnerTube>());
        sustainerMount->setName("Sustainer Mount");
        booster = &rocket.addChild(std::make_unique<AxialStage>());
        booster->setName("Booster");
        boosterBody = &booster->addChild(std::make_unique<BodyTube>(0.3, 0.02));
        boosterBody->setName("Booster Body");
        strapOns = &boosterBody->addChild(std::make_unique<ParallelStage>());
        strapOns->setName("Strap-ons");
        strapOnBody = &strapOns->addChild(std::make_unique<BodyTube>(0.1, 0.01));
        strapOnBody->setName("Strap-on Body");

        rocket.createFlightConfiguration(fcid);
        addMotor(*sustainerMount, fcid, motorA8(), 3.0);
        state = std::make_shared<MotorClusterState>(sustainerMount->getMotorConfig(fcid));
    }
};

/// Runs @p body and returns the message of the BugError it throws, without BugError's "BUG: "
/// prefix and " (file:line)" suffix; "<none>" when nothing is thrown.
[[nodiscard]] inline std::string bugText(const std::function<void()>& body)
{
    try
    {
        body();
    }
    catch (const BugError& error)
    {
        std::string       text   = error.what();
        const std::string prefix = "BUG: ";
        if (text.starts_with(prefix))
        {
            text.erase(0, prefix.size());
        }
        const std::size_t location = text.rfind(" (");
        if (location != std::string::npos)
        {
            text.erase(location);
        }
        return text;
    }
    return "<none>";
}

/// Expects @p actual to be exactly @p expected, NaN matching NaN.
inline void expectSame(double actual, double expected)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << "expected NaN, got " << actual;
    }
    else
    {
        EXPECT_EQ(actual, expected);
    }
}

/// Expects @p actual to hold exactly the values of @p expected, NaN matching NaN.
inline void expectSame(const std::vector<double>& actual, const std::vector<double>& expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); i++)
    {
        SCOPED_TRACE(i);
        expectSame(actual[i], expected[i]);
    }
}

}  // namespace QtRocket::Test
