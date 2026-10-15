// The air at the launch site (Simulation::validateInputs(), "The air at the launch site"), and
// flights at the ends of the ranges of the launch temperature, pressure and humidity.
//
// Each of the three numbers has a range (SimulationInputRanges), and numbers inside their
// ranges still ended Simulation::simulate() in a BugError, for a design that loaded without a
// warning, in two ways that the review of run 9d found:
// - humid air whose vapour pressure is more than the pressure there is has a negative density
//   in OpenRocket's atmospheric conditions, and the flight ended in the BugError of a NaN. The
//   validation now asks the atmospheric model for the density at the launch altitude and
//   refuses air that has none, with an ordinary error before anything runs. (Since decision
//   D3 the NaN of a flight is an ordinary error too, SIMULATION_ABORTED: what a flight meets
//   aloft, an air start at 50 km over humid air, ends there; nan_flight_tests.cpp.)
// - in very dense air (a launch temperature of 5 K) the step to the apogee leaves a horizontal
//   air speed between 1e-8 and 1e-7 m/s, which the steppers of the descent normalised and
//   Coordinate::normalize() refused ("Cannot normalize zero coordinate", as OpenRocket's
//   IllegalStateException). The steppers now leave the drag of such an air speed out
//   (AbstractEulerStepper), and the flight goes on.
// Both are QtRocket's own; OpenRocket ends such a run in an exception.

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using Texts = std::vector<std::string>;

/// The air of a launch site as the options hold it, without the ISA atmosphere.
struct Air
{
    double temperature;
    double pressure;
    double humidity;
    /// The launch altitude, m.
    double altitude{0.0};
};

/// Gives @p options the air @p air, entered by hand.
void setAir(SimulationOptions& options, const Air& air)
{
    options.setIsaAtmosphere(false);
    options.setLaunchAltitude(air.altitude);
    options.setLaunchTemperature(air.temperature);
    options.setLaunchPressure(air.pressure);
    // First another value: a setter stores nothing that equals what it has.
    options.setLaunchRelativeHumidity(air.humidity == 0.25 ? 0.75 : 0.25);
    options.setLaunchRelativeHumidity(air.humidity);
}

/// The message of validateInputs() of the Estes Alpha III's simulation in the air @p air;
/// "valid" when it passes.
[[nodiscard]] std::string validationIn(const Air& air)
{
    TestEstesAlphaIII alpha;
    Simulation        simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(testFcid(0));
    setAir(simulation.getOptions(), air);
    const Result<void> valid = simulation.validateInputs();
    return valid.has_value() ? std::string("valid") : valid.error().message;
}

/// What the refusal of the air @p air starts with: everything up to the density, which is
/// worked out with exp() and so has other last digits on another platform.
[[nodiscard]] std::string refusalOf(std::string_view temperature, std::string_view pressure,
                                    std::string_view humidity)
{
    return std::format(
        "Cannot simulate: the launch temperature ({} K), the launch pressure ({} Pa) and the "
        "launch relative humidity ({}) give air of no positive density at the launch altitude (",
        temperature, pressure, humidity);
}

/// Whether @p message is the refusal that starts with @p start and names a density below 0 (or
/// one of 0, which has no sign to speak of).
[[nodiscard]] bool isRefusal(const std::string& message, const std::string& start)
{
    return message.starts_with(start) && message.ends_with(" kg/m^3).") &&
           (message.substr(start.size()).starts_with('-') ||
            message.substr(start.size()).starts_with('0'));
}

// Ordinary air has a density, dry or humid, at sea level and high up.
TEST(LaunchSiteAir, OrdinaryAirIsValid)
{
    EXPECT_EQ(validationIn({.temperature = 288.15, .pressure = 101325, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 288.15, .pressure = 101325, .humidity = 0.5}), "valid");
    EXPECT_EQ(validationIn({.temperature = 303.15, .pressure = 101325, .humidity = 1}), "valid");
    // A hot and humid day, a frosty one, and a site at 4000 m.
    EXPECT_EQ(validationIn({.temperature = 318.15, .pressure = 100000, .humidity = 0.9}), "valid");
    EXPECT_EQ(validationIn({.temperature = 233.15, .pressure = 103000, .humidity = 1}), "valid");
    EXPECT_EQ(
        validationIn({.temperature = 270.0, .pressure = 61600, .humidity = 0.6, .altitude = 4000}),
        "valid");
}

// Dry air has a density at every end of the ranges of its temperature and its pressure: the
// density is the pressure over the temperature and the gas constant.
TEST(LaunchSiteAir, DryAirIsValidAtEveryEndOfItsRanges)
{
    EXPECT_EQ(validationIn({.temperature = 1, .pressure = 101325, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 1000, .pressure = 101325, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 288.15, .pressure = 0.001, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 288.15, .pressure = 1000000, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 1, .pressure = 1000000, .humidity = 0}), "valid");
    EXPECT_EQ(validationIn({.temperature = 1000, .pressure = 0.001, .humidity = 0}), "valid");
}

// Humid air whose vapour would have more pressure than there is: each number inside its range,
// and together no air. An ordinary error that names the three and the density.
TEST(LaunchSiteAir, HumidAirWithoutADensityIsRefused)
{
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 288.15, .pressure = 300, .humidity = 0.5}),
                          refusalOf("288.15", "300.0", "0.5")));
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 288.15, .pressure = 0.001, .humidity = 0.5}),
                          refusalOf("288.15", "0.001", "0.5")));
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 320, .pressure = 3000, .humidity = 1}),
                          refusalOf("320.0", "3000.0", "1.0")));
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 400, .pressure = 101325, .humidity = 1}),
                          refusalOf("400.0", "101325.0", "1.0")));
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 500, .pressure = 101325, .humidity = 0.5}),
                          refusalOf("500.0", "101325.0", "0.5")));
    EXPECT_TRUE(isRefusal(validationIn({.temperature = 1000, .pressure = 101325, .humidity = 0.5}),
                          refusalOf("1000.0", "101325.0", "0.5")));
    EXPECT_TRUE(
        isRefusal(validationIn({.temperature = 1000, .pressure = 101325, .humidity = 0.001}),
                  refusalOf("1000.0", "101325.0", "0.001")));
    // The whole text of one of them, but for the digits of the density.
    const std::string message =
        validationIn({.temperature = 288.15, .pressure = 300, .humidity = 0.5});
    EXPECT_TRUE(message.starts_with(
        "Cannot simulate: the launch temperature (288.15 K), the launch pressure (300.0 Pa) and "
        "the launch relative humidity (0.5) give air of no positive density at the launch "
        "altitude (-3.0"))
        << message;
    EXPECT_TRUE(message.ends_with("E-4 kg/m^3).")) << message;
}

// It is the model's conditions at the launch altitude that are asked, not the three numbers by
// themselves: at a site just below the 11 km at which the atmospheric model takes a site, the
// layers the model fits have a temperature of more than a thousand kelvin at the site, where
// a humidity of 0.01 is too much, although the three numbers alone give air of a density.
TEST(LaunchSiteAir, TheModelIsAskedAtTheLaunchAltitude)
{
    const Air humidAtTheTop{
        .temperature = 288.15, .pressure = 101325, .humidity = 0.01, .altitude = 11018};
    EXPECT_TRUE(isRefusal(validationIn(humidAtTheTop), refusalOf("288.15", "101325.0", "0.01")))
        << validationIn(humidAtTheTop);
    // The same air at sea level, and dry air at that site.
    EXPECT_EQ(validationIn({.temperature = 288.15, .pressure = 101325, .humidity = 0.01}), "valid");
    EXPECT_EQ(
        validationIn({.temperature = 288.15, .pressure = 101325, .humidity = 0, .altitude = 11018}),
        "valid");
    // A site a little lower, with the humidity of an ordinary day.
    EXPECT_EQ(validationIn(
                  {.temperature = 288.15, .pressure = 101325, .humidity = 0.5, .altitude = 10900}),
              "valid");
}

// What a run does not read is not asked: under the ISA atmosphere the three numbers are what
// they are. And what the atmospheric model itself refuses stays the model's error, which
// simulate() returns when it makes the conditions.
TEST(LaunchSiteAir, IsAskedOnlyOfAModelTheRunReads)
{
    TestEstesAlphaIII alpha;
    Simulation        simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(testFcid(0));
    setAir(simulation.getOptions(), {.temperature = 288.15, .pressure = 300, .humidity = 0.5});
    ASSERT_FALSE(simulation.validateInputs().has_value());
    simulation.getOptions().setIsaAtmosphere(true);
    EXPECT_TRUE(simulation.validateInputs().has_value());

    // A site at 5 km and 50 K has no positive temperature at sea level: the model refuses it,
    // and the validation leaves that to the model.
    setAir(simulation.getOptions(),
           {.temperature = 50, .pressure = 54000, .humidity = 0, .altitude = 5000});
    EXPECT_TRUE(simulation.validateInputs().has_value());
    const Result<void> result = simulation.simulate();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Temperature must be positive (Kelvin)");
}

// The refusal is simulate()'s too: an error of the caller's arguments, with nothing run.
TEST(LaunchSiteAir, SimulateRefusesAirWithoutADensityBeforeAnythingRuns)
{
    TestEstesAlphaIII alpha;
    Simulation        simulation(*alpha.rocket);
    simulation.setFlightConfigurationId(testFcid(0));
    setAir(simulation.getOptions(), {.temperature = 400, .pressure = 101325, .humidity = 1});

    const Result<void> result = simulation.simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_TRUE(isRefusal(result.error().message, refusalOf("400.0", "101325.0", "1.0")))
        << result.error().message;
    EXPECT_EQ(simulation.getSimulatedData(), nullptr);
}

// ------------------------------------------------------------------- flights of an example

/// How a run ended.
enum class Ending
{
    /// simulate() succeeded.
    FLEW,
    /// simulate() refused the inputs: ErrorCode::INVALID_ARGUMENT.
    REFUSED,
    /// simulate() failed with another ordinary error.
    FAILED,
    /// simulate() failed with the ordinary error of a NaN or an infinity in the flight
    /// (AbstractSimulationStepper::checkNaN(), checkFinite() and addRocketPosition(), and the
    /// engine's own NaN check; the first and the third were a BugError before decision D3).
    NOT_A_NUMBER,
    /// simulate() threw a BugError.
    BUG,
};

/// A flight of the first simulation of "A simple model rocket" in an air, and how it may end.
struct AirCase
{
    std::string_view name;
    Air              air;
    /// The one ending the case has, or FLEW for a case that may fly or end in an ordinary
    /// error of the computation (the ends of the ranges are no flights anyone plans, and
    /// whether "Simulation values exceeded limits" is met there may differ by platform); a
    /// BugError and the error of a NaN are never an ending.
    Ending expected;
};

/// How simulate() of @p simulation ends in the air @p air, and the text of a failure.
struct Flown
{
    Ending      ending{Ending::FLEW};
    std::string text;
};

[[nodiscard]] Flown flyIn(Simulation& simulation, const Air& air)
{
    setAir(simulation.getOptions(), air);
    simulation.getOptions().setRandomSeed(1);
    try
    {
        const Result<void> result = simulation.simulate();
        if (result.has_value())
        {
            return {};
        }
        if (result.error().code == ErrorCode::INVALID_ARGUMENT)
        {
            return {.ending = Ending::REFUSED, .text = result.error().message};
        }
        // checkNaN()'s and the engine's "... not-a-number (NaN) value ...", checkFinite()'s
        // "... a value that is not finite ..." and addRocketPosition()'s NaN location.
        const std::string& message = result.error().message;
        const bool notANumber      = message.starts_with("Simulation resulted in ") ||
                                     message.starts_with("addCoordinate resulted in NaN location");
        return {.ending = notANumber ? Ending::NOT_A_NUMBER : Ending::FAILED, .text = message};
    }
    catch (const BugError& error)
    {
        return {.ending = Ending::BUG, .text = error.what()};
    }
}

/// The name of @p ending.
[[nodiscard]] std::string_view nameOf(Ending ending)
{
    switch (ending)
    {
        case Ending::FLEW:
            return "flew";
        case Ending::REFUSED:
            return "refused";
        case Ending::FAILED:
            return "failed";
        case Ending::NOT_A_NUMBER:
            return "NaN";
        case Ending::BUG:
            return "BUG";
    }
    return "?";
}

/// Whether a case that expects @p expected may end in @p ending: as expected, and a case that
/// is expected to fly also in an ordinary failure of the computation.
[[nodiscard]] bool mayEnd(Ending expected, Ending ending)
{
    return ending == expected || (expected == Ending::FLEW && ending == Ending::FAILED);
}

/// Flies the first simulation of the example "A simple model rocket", as its file loads, in
/// every air of @p cases and returns the cases that did not end as they may, each as "<name>:
/// <ending> <text>".
[[nodiscard]] Texts wrongEndings(const std::vector<AirCase>& cases)
{
    const GoldenExample* example = findGoldenExample("A simple model rocket.ork");
    if (example == nullptr)
    {
        return {"the example is not there"};
    }
    const Result<LoadedDocument> loaded = loadGoldenExample(*example);
    if (!loaded.has_value())
    {
        return {"the example did not load: " + loaded.error().message};
    }
    if (loaded->document->getSimulationCount() == 0)
    {
        return {"the example has no simulation"};
    }
    const std::shared_ptr<Simulation> simulation = loaded->document->getSimulation(0);
    Texts                             wrong;
    for (const AirCase& airCase : cases)
    {
        const Flown flown = flyIn(*simulation, airCase.air);
        if (!mayEnd(airCase.expected, flown.ending))
        {
            wrong.push_back(
                std::format("{}: {} {}", airCase.name, nameOf(flown.ending), flown.text));
        }
    }
    return wrong;
}

// The example as it is, in ordinary humid air: the flight of the rows below is a flight.
TEST(LaunchSiteAirFlights, TheExampleFliesInOrdinaryHumidAir)
{
    const GoldenExample* example = findGoldenExample("A simple model rocket.ork");
    ASSERT_NE(example, nullptr);
    const Result<LoadedDocument> loaded = loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_GT(loaded->document->getSimulationCount(), 0U);
    const std::shared_ptr<Simulation> simulation = loaded->document->getSimulation(0);

    const Flown flown =
        flyIn(*simulation, {.temperature = 288.15, .pressure = 101325, .humidity = 0.5});
    EXPECT_EQ(nameOf(flown.ending), "flew") << flown.text;
    ASSERT_NE(simulation->getSimulatedData(), nullptr);
    EXPECT_GT(simulation->getSimulatedData()->getMaxAltitude(), 20.0);
    EXPECT_FALSE(simulation->hasErrors());
}

// Every end of every row of the atmosphere, one at a time, in dry air: a Result, never a
// BugError, nor the error of a NaN.
TEST(LaunchSiteAirFlights, NoEndOfARangeOfTheAtmosphereEndsInABugError)
{
    const std::vector<AirCase> cases{
        {.name     = "the lowest temperature",
         .air      = {.temperature = 1, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "the highest temperature",
         .air      = {.temperature = 1000, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "the lowest pressure",
         .air      = {.temperature = 288.15, .pressure = 0.001, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "the highest pressure",
         .air      = {.temperature = 288.15, .pressure = 1000000, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "no humidity",
         .air      = {.temperature = 288.15, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "saturated air",
         .air      = {.temperature = 288.15, .pressure = 101325, .humidity = 1},
         .expected = Ending::FLEW},
    };
    EXPECT_EQ(wrongEndings(cases), Texts{});
}

// The air in which the flight ended in "Cannot normalize zero coordinate": so dense that the
// horizontal air speed left at the apogee lies between 1e-8 and 1e-7 m/s. (5 K at sea level
// pressure ended there in 30 runs of 30, 1 K, 2 K and 10 K at that or at ten times the
// pressure in most.) Each is a flight now.
TEST(LaunchSiteAirFlights, VeryDenseAirIsFlownThrough)
{
    const std::vector<AirCase> cases{
        {.name     = "1 K",
         .air      = {.temperature = 1, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "5 K",
         .air      = {.temperature = 5, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "2 K at ten bar",
         .air      = {.temperature = 2, .pressure = 1000000, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "10 K at ten bar",
         .air      = {.temperature = 10, .pressure = 1000000, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "1 K at ten bar",
         .air      = {.temperature = 1, .pressure = 1000000, .humidity = 0},
         .expected = Ending::FLEW},
    };
    EXPECT_EQ(wrongEndings(cases), Texts{});
}

// The air in which the flight ended in the BugError of a NaN: humid air without a density, and
// the humid site at the top of the atmospheric model. Each is refused before anything runs.
TEST(LaunchSiteAirFlights, HumidAirWithoutADensityIsRefusedAndNotFlown)
{
    const std::vector<AirCase> cases{
        {.name     = "half saturated at 300 Pa",
         .air      = {.temperature = 288.15, .pressure = 300, .humidity = 0.5},
         .expected = Ending::REFUSED},
        {.name     = "half saturated at the lowest pressure",
         .air      = {.temperature = 288.15, .pressure = 0.001, .humidity = 0.5},
         .expected = Ending::REFUSED},
        {.name     = "saturated at 320 K and 3000 Pa",
         .air      = {.temperature = 320, .pressure = 3000, .humidity = 1},
         .expected = Ending::REFUSED},
        {.name     = "saturated at 400 K",
         .air      = {.temperature = 400, .pressure = 101325, .humidity = 1},
         .expected = Ending::REFUSED},
        {.name     = "half saturated at 500 K",
         .air      = {.temperature = 500, .pressure = 101325, .humidity = 0.5},
         .expected = Ending::REFUSED},
        {.name     = "a trace of humidity at the highest temperature",
         .air      = {.temperature = 1000, .pressure = 101325, .humidity = 0.001},
         .expected = Ending::REFUSED},
        {.name     = "half saturated at the highest temperature",
         .air      = {.temperature = 1000, .pressure = 101325, .humidity = 0.5},
         .expected = Ending::REFUSED},
        {.name = "a humid site at the top of the model",
         .air  = {.temperature = 288.15, .pressure = 101325, .humidity = 0.01, .altitude = 11018},
         .expected = Ending::REFUSED},
    };
    EXPECT_EQ(wrongEndings(cases), Texts{});
}

// Sites that are real, or nearly: each flies.
TEST(LaunchSiteAirFlights, RealSitesFly)
{
    const std::vector<AirCase> cases{
        {.name     = "a site at 1400 m",
         .air      = {.temperature = 290, .pressure = 85600, .humidity = 0.3, .altitude = 1400},
         .expected = Ending::FLEW},
        {.name     = "a site at 4000 m",
         .air      = {.temperature = 270, .pressure = 61600, .humidity = 0.6, .altitude = 4000},
         .expected = Ending::FLEW},
        {.name     = "the coldest air measured",
         .air      = {.temperature = 184, .pressure = 101325, .humidity = 0},
         .expected = Ending::FLEW},
        {.name     = "a hot and humid day",
         .air      = {.temperature = 318.15, .pressure = 100000, .humidity = 0.9},
         .expected = Ending::FLEW},
        {.name     = "a dry site at the top of the model",
         .air      = {.temperature = 288.15, .pressure = 101325, .humidity = 0, .altitude = 11018},
         .expected = Ending::FLEW},
    };
    EXPECT_EQ(wrongEndings(cases), Texts{});
}

}  // namespace
