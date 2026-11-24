#include "SimulationRunTestSupport.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The document of the example @p fileName, loaded in @p environment; a new rocket's, with a
/// test failure, when it cannot be.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> loadExampleDesign(
    const QtRocket::Test::DesignFileEnvironment& environment, const std::string& fileName)
{
    Result<LoadedDocument> loaded = GeneralRocketLoader(environment.context())
                                        .load(QtRocket::Test::dataDir() / "examples" / fileName);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << "cannot load the example " << fileName << ": " << loaded.error().message;
        return OpenRocketDocumentFactory::createNewRocket();
    }
    return std::move(loaded->document);
}

}  // namespace

ExampleDesign::ExampleDesign(const std::string& fileName)
  : m_document(loadExampleDesign(m_environment, fileName))
{
}

ExampleDesign::~ExampleDesign() = default;

std::shared_ptr<Simulation> ExampleDesign::simulation(std::size_t index) const
{
    if (index >= m_document->getSimulationCount())
    {
        ADD_FAILURE() << "the design has no simulation " << index;
        return nullptr;
    }
    return m_document->getSimulation(index);
}

std::shared_ptr<Simulation> ExampleDesign::addCopyOf(std::size_t index, const std::string& name)
{
    const std::shared_ptr<Simulation> original = simulation(index);
    if (original == nullptr)
    {
        return nullptr;
    }
    std::shared_ptr<Simulation> copy = original->duplicateSimulation(m_document->getRocket());
    copy->setName(name);
    m_document->addSimulation(copy);
    return copy;
}

UserPrompts::RunSimulationsRequest runRequestOf(
    OpenRocketDocument& document, std::vector<std::shared_ptr<Simulation>> simulations,
    SimulationExecutor& executor, const QObject* window)
{
    return {.window               = window,
            .document             = &document,
            .simulations          = std::move(simulations),
            .executor             = &executor,
            .cancelBackgroundRuns = {}};
}

std::shared_ptr<SimulationListener> actionAtStep(int step, std::function<void()> action)
{
    return std::make_shared<ProgressListener>(
        [step, action = std::move(action),
         steps = std::make_shared<int>(0)](const SimulationStatus& /*status*/) {
            if (++*steps == step)
            {
                action();
            }
        });
}

std::shared_ptr<SimulationListener> failingRunListener(RunFailure failure)
{
    return std::make_shared<ProgressListener>([failure](const SimulationStatus& status) {
        switch (failure)
        {
            case RunFailure::SIMULATION_EXCEPTION:
                throw SimulationException("the listener gave up");
            case RunFailure::CALCULATION_WITH_BRANCH:
                throw SimulationCalculationException("the numbers went wrong",
                                                     status.getFlightDataBranch());
            case RunFailure::OTHER_EXCEPTION:
                throw std::runtime_error("not a simulation exception");
        }
    });
}

RunSampleListener::Clock frozenSampleClock()
{
    return [] { return std::chrono::steady_clock::time_point{}; };
}

}  // namespace QtRocket::Gui::Test
