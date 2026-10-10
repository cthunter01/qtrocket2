// Every simulation of the committed golden data lies inside the ranges a simulation holds its
// inputs to (decision U9 of tier 9; SimulationInputRanges and Simulation::validateInputs()):
// the 50 simulations of the 13 test rockets, at their own time step and at the stable one, and
// the 54 simulations of the 16 example designs as they load; 104 simulations in all, which the
// manifest of the goldens lists twice each (the default-step and the stable-step set).
//
// The golden comparisons do not show this by passing: their harness turns the options into
// simulation conditions and hands them to the engine itself, as OpenRocket's golden harness
// does (tests/core/goldens/GoldenSimulations.h, runSimulation()), so a run of theirs never
// meets the validation of Simulation::simulate(). Here each of those simulations is made as
// the harness makes it, or loaded as the examples are loaded, and asked.

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenSimulations.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::InstallationDefaults;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::makeReproducible;
using QtRocket::Test::PlannedSimulation;
using QtRocket::Test::plannedSimulations;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::useStableTimeStep;

/// The time step of the stable-step set of the goldens (simulation_stable_golden_tests.cpp).
constexpr double kStableTimeStep = 0.01;

/// What a count of simulations found: how many there were, and what the validation said of
/// those it refused.
struct Asked
{
    std::size_t              simulations{0};
    std::vector<std::string> refused;
};

/// Asks the validation of @p simulation, which @p name stands for in a failure.
void ask(const Simulation& simulation, const std::string& name, Asked& asked)
{
    asked.simulations++;
    const Result<void> valid = simulation.validateInputs();
    if (!valid.has_value())
    {
        asked.refused.push_back(name + ": " + valid.error().message);
    }
}

/// The simulation of the golden harness for @p planned of @p rocket, as runSimulation() of
/// goldens/GoldenSimulations.h sets it up before the run: the installation's default options,
/// the variant's change, the harness's changes and, with @p stableTimeStep, the stable time
/// step last.
[[nodiscard]] std::unique_ptr<Simulation> harnessSimulation(Rocket&                  rocket,
                                                            const PlannedSimulation& planned,
                                                            std::optional<double>    stableTimeStep)
{
    const InstallationDefaults  defaults;
    std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(rocket);
    simulation->setFlightConfigurationId(
        rocket.getFlightConfigurationByIndex(planned.configuration, true).getId());
    simulation->setName(planned.name);
    simulation->getOptions().copyConditionsFrom(defaults.options);
    if (planned.variant != nullptr)
    {
        planned.variant->apply(simulation->getOptions());
    }
    // The harness object: what was changed, which the stable time step adds to.
    auto harness = makeReproducible(simulation->getOptions());
    if (stableTimeStep.has_value())
    {
        useStableTimeStep(simulation->getOptions(), harness, *stableTimeStep);
    }
    return simulation;
}

/// The simulations of the golden data of every test rocket, with the time step of @p
/// stableTimeStep or their own.
[[nodiscard]] Asked askTheTestRockets(std::optional<double> stableTimeStep)
{
    Asked asked;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const std::unique_ptr<Rocket> rocket = maker.make();
        for (const PlannedSimulation& planned : plannedSimulations(*rocket, maker.input))
        {
            const std::unique_ptr<Simulation> simulation =
                harnessSimulation(*rocket, planned, stableTimeStep);
            ask(*simulation, std::format("{} {}", maker.input, planned.name), asked);
        }
    }
    return asked;
}

TEST(GoldenSimulationInputs, EverySimulationOfTheTestRocketsLiesInsideTheRanges)
{
    const Asked asked = askTheTestRockets(std::nullopt);
    EXPECT_EQ(asked.simulations, 50U);
    EXPECT_EQ(asked.refused, std::vector<std::string>{});
}

TEST(GoldenSimulationInputs, EverySimulationOfTheTestRocketsLiesInsideTheRangesAtTheStableStep)
{
    const Asked asked = askTheTestRockets(kStableTimeStep);
    EXPECT_EQ(asked.simulations, 50U);
    EXPECT_EQ(asked.refused, std::vector<std::string>{});
}

/// The simulations of every example design as its file loads.
[[nodiscard]] Asked askTheExamples()
{
    Asked asked;
    for (const GoldenExample& example : goldenExamples())
    {
        const Result<LoadedDocument> loaded = loadGoldenExample(example);
        if (!loaded.has_value())
        {
            asked.refused.push_back(example.name + ": " + loaded.error().message);
            continue;
        }
        for (std::size_t i = 0; i < loaded->document->getSimulationCount(); i++)
        {
            const std::shared_ptr<Simulation> simulation = loaded->document->getSimulation(i);
            ask(*simulation, std::format("{} '{}'", example.name, simulation->getName()), asked);
        }
    }
    return asked;
}

// The examples hold their own time steps, wind and launch sites; what the reader leaves to the
// preference store of the load (that of OpenRocket's tests, whose time step is 0) none of them
// needs.
TEST(GoldenSimulationInputs, EverySimulationOfTheExamplesLiesInsideTheRanges)
{
    const Asked asked = askTheExamples();
    EXPECT_EQ(asked.simulations, 54U);
    EXPECT_EQ(asked.refused, std::vector<std::string>{});
}

}  // namespace
