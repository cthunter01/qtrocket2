// Flights whose numbers become NaN end with an ordinary error of Simulation::simulate(),
// ErrorCode::SIMULATION_ABORTED, not with a BugError (decision D3 of 2026-10-14). The values
// that get there lie inside every range the simulation checks (the dimensions of a design have
// none) or come from the atmospheric model itself: a nose cone of 1e150 m, a parachute of
// 1e300 m across, a mass at a radial position of 1e200 m, and an air start at 50 km over humid
// air, where OpenRocket's atmospheric model has a negative density (the humidity is carried up
// unchanged). In OpenRocket each ends in the BugException "Simulation resulted in not-a-number
// (NaN) value for ..., please report a bug" of AbstractSimulationStepper.checkNaN(); here the
// check throws a SimulationCalculationException with Java's text without its plea, which the
// engine records as an EXCEPTION event of the branch and simulate() returns as its error.
// (The cases are those of the scout of tier 10, probe nanprobe.cpp, on the same design: the
// first simulation of the example "A simple model rocket".) The last test is the stepper's
// check of QtRocket's own (checkFinite()) that keeps an infinity in the flight from becoming a
// BugError further on.

#include <format>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FlightEvent;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::loadGoldenExample;

/// What a change does to the example's document and its first simulation before the flight.
using Change = std::function<void(OpenRocketDocument&, Simulation&)>;

/// The last event of the first branch of @p simulation's data, "<type>: <message>" for an
/// EXCEPTION; "no data" without a branch.
[[nodiscard]] std::string lastEventOf(const Simulation& simulation)
{
    const std::shared_ptr<QtRocket::FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr || data->getBranchCount() == 0 || data->getBranch(0).getEvents().empty())
    {
        return "no data";
    }
    const FlightEvent& last    = data->getBranch(0).getEvents().back();
    const auto*        message = std::get_if<std::string>(&last.getData());
    return std::format("{}: {}", QtRocket::name(last.getType()),
                       message != nullptr ? *message : std::string("-"));
}

/// How the first simulation of "A simple model rocket" ends after @p change: "flew", or the
/// code and the message of simulate()'s error and, behind " | ", the last event of the first
/// branch, or "BUG: " and the BugError's text.
[[nodiscard]] std::string flightAfter(const Change& change)
{
    const GoldenExample* example = findGoldenExample("A simple model rocket.ork");
    if (example == nullptr)
    {
        return "the example is not there";
    }
    const Result<LoadedDocument> loaded = loadGoldenExample(*example);
    if (!loaded.has_value() || loaded->document->getSimulationCount() == 0)
    {
        return "the example did not load";
    }
    OpenRocketDocument&               document   = *loaded->document;
    const std::shared_ptr<Simulation> simulation = document.getSimulation(0);
    change(document, *simulation);
    try
    {
        const Result<void> flown = simulation->simulate();
        if (flown.has_value())
        {
            return "flew";
        }
        return std::format("{}: {} | {}", QtRocket::toString(flown.error().code),
                           flown.error().message, lastEventOf(*simulation));
    }
    catch (const BugError& error)
    {
        return std::string("BUG: ") + error.what();
    }
}

/// A change that calls @p set on every component of the rocket that is a @p Component.
template <class Component>
[[nodiscard]] Change everyComponent(std::function<void(Component&)> set)
{
    return [set = std::move(set)](OpenRocketDocument& document, Simulation& /*simulation*/) {
        document.getRocket().forEach([&set](RocketComponent& component) {
            if (auto* const wanted = dynamic_cast<Component*>(&component))
            {
                set(*wanted);
            }
        });
    };
}

/// Every nose cone @p length long.
[[nodiscard]] Change noseConesOfLength(double length)
{
    return everyComponent<QtRocket::NoseCone>(
        [length](QtRocket::NoseCone& nose) { nose.setLength(length); });
}

/// Every parachute @p diameter across.
[[nodiscard]] Change parachutesOfDiameter(double diameter)
{
    return everyComponent<QtRocket::Parachute>(
        [diameter](QtRocket::Parachute& chute) { chute.setDiameter(diameter); });
}

/// Every mass object at the radial position @p position.
[[nodiscard]] Change massesAtRadialPosition(double position)
{
    return everyComponent<QtRocket::MassObject>(
        [position](QtRocket::MassObject& mass) { mass.setRadialPosition(position); });
}

/// Half saturated air of 288.15 K at sea level pressure, without the ISA atmosphere, and an
/// air start at @p altitude with 10 m/s.
[[nodiscard]] Change airStartOverHumidAirAt(double altitude)
{
    return [altitude](OpenRocketDocument& /*document*/, Simulation& simulation) {
        QtRocket::SimulationOptions& options = simulation.getOptions();
        options.setIsaAtmosphere(false);
        options.setLaunchTemperature(288.15);
        options.setLaunchPressure(101325);
        options.setLaunchRelativeHumidity(0.5);
        const auto start = std::make_shared<QtRocket::AirStart>();
        start->setLaunchAltitude(altitude);
        start->setLaunchVelocity(10);
        simulation.getSimulationExtensions().push_back(start);
    };
}

// One test for the one design (each case loads it afresh):
// - with a mass at a radial position of 1e150 m the example still flies (the square of the
//   position is finite; LaunchSiteAirFlights.TheExampleFliesInOrdinaryHumidAir flies it as it
//   is);
// - absurd finite dimensions: the mass calculation of the first step gives a NaN centre of mass
//   or inertia, and the flight ends with the ordinary error of the NaN;
// - half saturated air of 288.15 K at sea level pressure has a density, but OpenRocket's model
//   carries the humidity up unchanged, and from about 48.5 km to 55.5 km the pressure of the
//   water vapour it takes is more than the pressure there is: the density is negative (-2.1e-4
//   kg/m^3 at 50 km, far from 0 on any platform), and an air start there ends the flight with
//   the ordinary error of the NaN acceleration. (The launch site itself has air, so the
//   validation of simulate() lets the flight start.)
TEST(NanFlight, TheExampleEndsAFlightOfNaNsWithAnOrdinaryError)
{
    EXPECT_EQ(flightAfter(massesAtRadialPosition(1e150)), "flew");

    EXPECT_EQ(flightAfter(noseConesOfLength(1e150)),
              "SIMULATION_ABORTED: Simulation resulted in not-a-number (NaN) value for "
              "structureMass.getCenterOfMass(), c=(NaN,NaN,NaN) | EXCEPTION: Simulation resulted "
              "in not-a-number (NaN) value for structureMass.getCenterOfMass(), c=(NaN,NaN,NaN)");
    EXPECT_EQ(flightAfter(parachutesOfDiameter(1e300)),
              "SIMULATION_ABORTED: Simulation resulted in not-a-number (NaN) value for "
              "structureMass.getCenterOfMass(), c=(NaN,NaN,NaN,w=Infinity) | EXCEPTION: "
              "Simulation resulted in not-a-number (NaN) value for "
              "structureMass.getCenterOfMass(), c=(NaN,NaN,NaN,w=Infinity)");
    EXPECT_EQ(flightAfter(massesAtRadialPosition(1e200)),
              "SIMULATION_ABORTED: Simulation resulted in not-a-number (NaN) value for "
              "structureMass.getRotationalInertia(). | EXCEPTION: Simulation resulted in "
              "not-a-number (NaN) value for structureMass.getRotationalInertia().");

    EXPECT_EQ(flightAfter(airStartOverHumidAirAt(50000)),
              "SIMULATION_ABORTED: Simulation resulted in not-a-number (NaN) value for params.a, "
              "c=(NaN,NaN,NaN) | EXCEPTION: Simulation resulted in not-a-number (NaN) value for "
              "params.a, c=(NaN,NaN,NaN)");
}

// Not in Java (decision D3, checkFinite()): a motor of an absurd diameter (1e200 m, its nozzle
// exit as wide) gives the thrusting motors a nozzle exit area that overflows to infinity, which
// FlightConditions refuses (Java: an IllegalArgumentException that leaves the simulation; here
// it was a BugError). The stepper checks the area first and ends the flight with an ordinary
// error. (The motor builder takes any diameter, and a motor file can hold one.)
TEST(NanFlight, AMotorOfAnAbsurdDiameterEndsInAnOrdinaryError)
{
    QtRocket::Test::TestEstesAlphaIII     alpha;
    const QtRocket::FlightConfigurationId fcid = QtRocket::Test::testFcid(0);
    QtRocket::MotorConfiguration          motorConfig{*alpha.inner, fcid};
    motorConfig.setMotor(QtRocket::Test::makeTestMotor("Estes", "X8", "Desc", "case", {0}, 1e200,
                                                       0.070, {0, 1, 2}, {0, 9, 0},
                                                       {0.0164, 0.0145, 0.0131}));
    ASSERT_TRUE(motorConfig.setNozzleExitDiameter(1e200).has_value());
    alpha.inner->setMotorConfig(std::move(motorConfig), fcid);
    Simulation simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(fcid);
    simulation.getOptions().setRandomSeed(0);

    const Result<void> flown = simulation.simulate();
    ASSERT_FALSE(flown.has_value());
    EXPECT_EQ(flown.error().code, QtRocket::ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(flown.error().message,
              "Simulation resulted in a value that is not finite for thrustingNozzleExitArea.");
    EXPECT_EQ(lastEventOf(simulation), "EXCEPTION: " + flown.error().message);
}

}  // namespace
