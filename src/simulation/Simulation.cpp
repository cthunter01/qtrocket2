#include "QtRocket/simulation/Simulation.h"

#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/BasicEventSimulationEngine.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/system/InterruptListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The numbers of a run as Simulation::validateInputs() goes through them: it keeps the first
/// one that is not finite.
class InputCheck
{
public:
    /// Notes @p value, which is called @p what ("the launch rod length"), when it is the first
    /// value that is neither finite nor @p alsoValid.
    void finite(std::string_view what, double value,
                double alsoValid = std::numeric_limits<double>::quiet_NaN())
    {
        if (m_problem.has_value() || std::isfinite(value) || value == alsoValid)
        {
            return;
        }
        m_problem = std::format("Cannot simulate: {} is not finite ({}).", what,
                                Strings::javaDoubleToString(value));
    }

    /// Whether a value was noted.
    [[nodiscard]] bool failed() const noexcept { return m_problem.has_value(); }

    /// What validateInputs() returns.
    [[nodiscard]] Result<void> result() const
    {
        if (m_problem.has_value())
        {
            return fail(ErrorCode::INVALID_ARGUMENT, *m_problem);
        }
        return {};
    }

private:
    std::optional<std::string> m_problem;
};

/// The wind model that @p options use.
void checkWind(const SimulationOptions& options, InputCheck& check)
{
    switch (options.getWindModelType())
    {
        case WindModelType::AVERAGE:
        {
            const PinkNoiseWindModel& wind = options.getAverageWindModel();
            check.finite("the average wind speed", wind.getAverage());
            check.finite("the standard deviation of the wind speed", wind.getStandardDeviation());
            check.finite("the wind direction", wind.getDirection());
            return;
        }
        case WindModelType::MULTI_LEVEL:
        {
            const MultiLevelPinkNoiseWindModel& wind   = options.getMultiLevelWindModel();
            std::size_t                         number = 0;
            for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : wind.getLevels())
            {
                number++;
                check.finite(std::format("the altitude of wind level {}", number),
                             level->getAltitude());
                check.finite(std::format("the wind speed of wind level {}", number),
                             level->getSpeed());
                check.finite(std::format("the wind direction of wind level {}", number),
                             level->getDirection());
                check.finite(
                    std::format("the standard deviation of the wind speed of wind level {}",
                                number),
                    level->getStandardDeviation());
            }
            return;
        }
    }
    bug("Unknown wind model type");
}

/// Every number of @p table, a lookup table of the options that is called @p name ("the drag
/// lookup table"); null: the options have none. Which rows a flight reads is not known
/// beforehand, so the whole table has to be finite.
void checkLookupTable(const std::shared_ptr<const MachAoALookup>& table, std::string_view name,
                      InputCheck& check)
{
    if (table == nullptr)
    {
        return;
    }
    const std::optional<MachAoALookup::NonFiniteNumber> number = table->findNonFinite();
    if (!number.has_value())
    {
        return;
    }
    const std::string mach = Strings::javaDoubleToString(number->mach);
    switch (number->kind)
    {
        case MachAoALookup::NonFiniteNumber::Kind::MACH:
            check.finite(std::format("a Mach number of {}", name), number->value);
            return;
        case MachAoALookup::NonFiniteNumber::Kind::AOA:
            check.finite(std::format("an angle of attack of {} at Mach {}", name, mach),
                         number->value);
            return;
        case MachAoALookup::NonFiniteNumber::Kind::VALUE:
        {
            // The angle of the row is no number in a table without angles of attack.
            const std::string angle = std::isnan(number->aoa)
                                          ? std::string{}
                                          : std::format(" and an angle of attack of {} degrees",
                                                        Strings::javaDoubleToString(number->aoa));
            check.finite(
                std::format("the '{}' of {} at Mach {}{}", number->column, name, mach, angle),
                number->value);
            return;
        }
    }
    bug("Unknown kind of a lookup table's number");
}

/// Every number SimulationOptions::toSimulationConditions() reads from @p options.
void checkOptions(const SimulationOptions& options, InputCheck& check)
{
    check.finite("the launch rod length", options.getLaunchRodLength());
    check.finite("the launch rod angle", options.getLaunchRodAngle());
    check.finite("the launch latitude", options.getLaunchLatitude());
    check.finite("the launch longitude", options.getLaunchLongitude());
    check.finite("the launch altitude", options.getLaunchAltitude());
    checkWind(options, check);
    if (check.failed())
    {
        // The rod direction of a launch into the wind is worked out from the wind model.
        return;
    }
    check.finite("the launch rod direction", options.getLaunchRodDirection());
    if (!options.isIsaAtmosphere())
    {
        check.finite("the launch temperature", options.getLaunchTemperature());
        check.finite("the launch pressure", options.getLaunchPressure());
        check.finite("the launch relative humidity", options.getLaunchRelativeHumidity());
    }
    if (options.getGravityModelType() == GravityModelType::CONSTANT)
    {
        check.finite("the constant gravity", options.getConstantGravity());
    }
    checkLookupTable(options.getStabilityLookupTable(), "the stability lookup table", check);
    checkLookupTable(options.getDragLookupTable(), "the drag lookup table", check);
    check.finite("the time step", options.getTimeStep());
    check.finite("the maximum simulation time", options.getMaxSimulationTime());
    check.finite("the maximum step angle", options.getMaximumStepAngle());
    check.finite("the recovery speed warning threshold", options.getRecoverySpeedWarning());
    check.finite("the drogue low speed warning threshold", options.getDrogueLowSpeedWarning());
    check.finite("the main high speed warning threshold",
                 options.getRecoveryDrogueMainHighSpeedWarning());
    check.finite("the main low speed warning threshold",
                 options.getRecoveryDrogueMainLowSpeedWarning());
}

/// The delays of the motor that @p component, when it is an acting motor mount, has in @p fcid,
/// and, when it is a recovery device, the deployment delay it has there and the deployment
/// altitude when that is what deploys it.
void checkComponent(const RocketComponent& component, const FlightConfigurationId& fcid,
                    InputCheck& check)
{
    if (const auto* mount = dynamic_cast<const MotorMount*>(&component);
        mount != nullptr && mount->isMotorMount())
    {
        const MotorConfiguration& motor = mount->getMotorConfig(fcid);
        if (!motor.isEmpty())
        {
            check.finite(
                std::format("the ignition delay of the motor in '{}'", component.getName()),
                motor.getIgnitionDelay());
            check.finite(
                std::format("the ejection delay of the motor in '{}'", component.getName()),
                motor.getEjectionDelay(), Motor::kPluggedDelay);
        }
    }
    if (const auto* device = dynamic_cast<const RecoveryDevice*>(&component))
    {
        const DeploymentConfiguration& deployment = device->getDeploymentConfigurations().get(fcid);
        check.finite(std::format("the deployment delay of '{}'", component.getName()),
                     deployment.getDeployDelay());
        // The engine reads the altitude of a deployment at an altitude only.
        if (deployment.getDeployEvent() == DeploymentConfiguration::DeployEvent::ALTITUDE)
        {
            check.finite(std::format("the deployment altitude of '{}'", component.getName()),
                         deployment.getDeployAltitude());
        }
    }
}

/// Whether the engine reads the separation altitude of a stage that separates at @p event.
[[nodiscard]] bool separatesAtAnAltitude(StageSeparationConfiguration::SeparationEvent event)
{
    using SeparationEvent = StageSeparationConfiguration::SeparationEvent;
    return event == SeparationEvent::ALTITUDE_ASCENDING ||
           event == SeparationEvent::ALTITUDE_DESCENDING;
}

/// The delays of @p config, the configuration @p fcid, and the altitudes the engine reads from
/// it: component by component, and then the stages.
void checkDesign(const FlightConfiguration& config, const FlightConfigurationId& fcid,
                 InputCheck& check)
{
    for (const RocketComponent* component : config.getActiveComponents())
    {
        checkComponent(*component, fcid, check);
    }
    for (const AxialStage* stage : config.getActiveStages())
    {
        if (stage->getStageNumber() == 0)
        {
            continue;
        }
        const StageSeparationConfiguration& separation =
            stage->getSeparationConfigurations().get(fcid);
        check.finite(std::format("the separation delay of '{}'", stage->getName()),
                     separation.getSeparationDelay());
        if (separatesAtAnAltitude(separation.getSeparationEvent()))
        {
            check.finite(std::format("the separation altitude of '{}'", stage->getName()),
                         separation.getSeparationAltitude());
        }
    }
}

/// The numbers that @p extensions, the extensions of a simulation in their order, say their part
/// of a run reads (SimulationExtension::getInputNumbers()).
void checkExtensions(const std::vector<std::shared_ptr<SimulationExtension>>& extensions,
                     InputCheck&                                              check)
{
    for (const std::shared_ptr<SimulationExtension>& extension : extensions)
    {
        if (extension == nullptr)
        {
            // Not an input: simulate() reports a null extension as the bug it is.
            continue;
        }
        for (const SimulationExtension::InputNumber& number : extension->getInputNumbers())
        {
            check.finite(number.what, number.value);
        }
    }
}

}  // namespace

Simulation::Simulation(Rocket& rocket) : Simulation(nullptr, rocket) { }

Simulation::Simulation(Rocket& rocket, Preferences& preferences)
  : Simulation(nullptr, rocket, preferences)
{
}

Simulation::Simulation(OpenRocketDocument* document, Rocket& rocket)
  : m_document(document), m_rocket(&rocket), m_status(Status::NOT_SIMULATED)
{
    // Java copies the conditions of the factory's default options here; without preferences the
    // options stay the built-in defaults (see the class comment).
    initialize();
}

Simulation::Simulation(OpenRocketDocument* document, Rocket& rocket, Preferences& preferences)
  : m_document(document),
    m_rocket(&rocket),
    m_preferences(&preferences),
    m_status(Status::NOT_SIMULATED),
    m_options(preferences)
{
    const DefaultSimulationOptionFactory f(preferences);
    m_options.copyConditionsFrom(f.getDefault());

    initialize();
}

Simulation::Simulation(OpenRocketDocument* document, Rocket& rocket, Status status,
                       std::string name, SimulationOptions options,
                       std::vector<std::shared_ptr<SimulationExtension>> extensions,
                       std::shared_ptr<FlightData>                       data,
                       const std::map<std::string, PlotAppearance>&      plotAppearances,
                       Preferences*                                      preferences)
  : m_document(document),
    m_rocket(&rocket),
    m_preferences(preferences),
    m_name(std::move(name)),
    m_status(status),
    m_options(std::move(options)),
    m_simulationExtensions(std::move(extensions)),
    m_simulatedData(std::move(data))
{
    m_simulatedConditions.emplace(m_options);
    // Java adds the document as a change listener here (addChangeListener(this.document)). The
    // document connects itself to changed() when the simulation enters its list (see the class
    // comment).

    connectConditionListener();

    const FlightConfiguration& config = m_rocket->getSelectedConfiguration();
    setFlightConfigurationId(config.getFlightConfigurationId());
    m_simulatedConfigurationModId = config.getModId();

    setPlotAppearancesInternal(plotAppearances, false);
}

Simulation::Simulation(CloneKey /*key*/, const Simulation& other)
  : m_document(other.m_document),
    m_rocket(other.m_rocket),
    m_ownedRocket(other.m_ownedRocket),
    m_preferences(other.m_preferences),
    m_configId(other.m_configId),
    m_name(other.m_name),
    m_status(other.m_status),
    m_options(other.m_options),
    // Java: Object.clone() copies the reference to the settings, which are immutable there.
    m_landingDispersionSettings(other.m_landingDispersionSettings),
    m_simulatedConditions(other.m_simulatedConditions),
    m_simulatedConfigurationDescription(other.m_simulatedConfigurationDescription),
    m_simulatedData(other.m_simulatedData),
    m_simulatedConfigurationModId(other.m_simulatedConfigurationModId),
    // Java: Object.clone() copies the reference to the map.
    m_plotAppearances(other.m_plotAppearances)
{
    m_simulationExtensions.reserve(other.m_simulationExtensions.size());
    for (const std::shared_ptr<SimulationExtension>& c : other.m_simulationExtensions)
    {
        if (c == nullptr)
        {
            bug("The simulation holds a null extension");
        }
        m_simulationExtensions.push_back(c->clone());
    }
}

Simulation::~Simulation() = default;

void Simulation::initialize()
{
    const FlightConfigurationId fcid =
        m_rocket->getSelectedConfiguration().getFlightConfigurationId();
    setFlightConfigurationId(fcid);

    connectConditionListener();
    // Java adds the document as a change listener here (addChangeListener(document)). The
    // document connects itself to changed() when the simulation enters its list, and
    // disconnects when it leaves (see the class comment).
}

void Simulation::connectConditionListener()
{
    m_options.changed().connect([this] { fireChangeEvent(); });
}

FlightConfiguration& Simulation::getActiveConfiguration()
{
    return m_rocket->getFlightConfiguration(m_configId);
}

const FlightConfiguration& Simulation::getActiveConfiguration() const
{
    const Rocket& rocket = *m_rocket;
    return rocket.getFlightConfiguration(m_configId);
}

void Simulation::setFlightConfigurationId(const FlightConfigurationId& fcid)
{
    if (fcid.hasError())
    {
        bug("Attempted to set the configuration to an error id. Not Allowed!");
    }
    if (!m_rocket->containsFlightConfigurationId(fcid))
    {
        m_rocket->createFlightConfiguration(fcid);
    }

    if (fcid == m_configId)
    {
        return;
    }

    m_configId = fcid;
    fireChangeEvent();
}

void Simulation::copySimulationOptionsFrom(const SimulationOptions& options)
{
    m_options.copyConditionsFrom(options);
}

void Simulation::setLandingDispersionSettings(std::optional<LandingDispersionSettings> settings)
{
    if (m_landingDispersionSettings == settings)
    {
        return;
    }
    m_landingDispersionSettings = std::move(settings);
    fireChangeEvent();
}

void Simulation::copyExtensionsFrom(
    const std::vector<std::shared_ptr<SimulationExtension>>& extensions)
{
    if (&extensions == &m_simulationExtensions)
    {
        // Java clears the list and then adds the (same, now empty) list to it.
        m_simulationExtensions.clear();
        return;
    }
    m_simulationExtensions = extensions;
}

void Simulation::setName(std::string_view name)
{
    if (m_name == name)
    {
        return;
    }

    m_name = name;

    fireChangeEvent();
}

Simulation::Status Simulation::getStatus()
{
    const FlightConfiguration config = m_rocket->getFlightConfiguration(getId()).clone();

    if (isStatusUpToDate(m_status) &&
        (config.getModId() != m_simulatedConfigurationModId ||
         !(m_simulatedConditions.has_value() && m_options == *m_simulatedConditions)))
    {
        m_status = Status::OUTDATED;
    }

    // if the id hasn't been set yet, skip.
    if (getId().hasError())
    {
        m_status = Status::CANT_RUN;
        return m_status;
    }

    // Make sure this simulation has motors.
    if (!config.hasMotors())
    {
        m_status = Status::CANT_RUN;
    }

    // If it has errors, it has aborted
    if (hasErrors())
    {
        m_status = Status::ABORTED;
    }

    return m_status;
}

std::string Simulation::getStatusDescription()
{
    return getDescription(getStatus(), *this);
}

bool Simulation::hasErrors() const
{
    const std::shared_ptr<FlightData>& data = getSimulatedData();
    if (data != nullptr)
    {
        for (std::size_t branchNo = 0; branchNo < data->getBranchCount(); branchNo++)
        {
            if (hasErrors(branchNo))
            {
                return true;
            }
        }
    }
    return false;
}

bool Simulation::hasErrors(std::size_t branch) const
{
    const std::shared_ptr<FlightData>& data = getSimulatedData();
    if (data == nullptr)
    {
        bug("The simulation has no simulated data");
    }
    const FlightData& simulated = *data;
    return simulated.getBranch(branch).getFirstEvent(FlightEvent::Type::SIM_ABORT) != nullptr;
}

void Simulation::syncModId()
{
    m_simulatedConfigurationModId = getActiveConfiguration().getModId();
    fireChangeEvent();
}

Result<void> Simulation::validateInputs() const
{
    InputCheck check;
    checkOptions(m_options, check);
    if (!check.failed())
    {
        // What the engine simulates: the configuration of the id in the rocket, and the
        // settings the components have for that id.
        checkDesign(getActiveConfiguration(), m_configId, check);
    }
    if (!check.failed())
    {
        checkExtensions(m_simulationExtensions, check);
    }
    return check.result();
}

Result<void> Simulation::simulate(
    std::span<const std::shared_ptr<SimulationListener>> additionalListeners)
{
    m_simulatedData = nullptr;
    // What the engine produced (Java: simulator.getFlightData()); null until it has run.
    std::shared_ptr<FlightData> flightData;
    Result<void>                result;
    try
    {
        result = runSimulation(additionalListeners, flightData);
    }
    catch (...)
    {
        // Java's finally block, on the way out of an exception that is not a
        // SimulationException (a BugError).
        recordSimulation(std::move(flightData));
        throw;
    }
    recordSimulation(std::move(flightData));
    return result;
}

Result<void> Simulation::simulate(
    std::initializer_list<std::shared_ptr<SimulationListener>> additionalListeners)
{
    return simulate(std::span<const std::shared_ptr<SimulationListener>>(
        additionalListeners.begin(), additionalListeners.size()));
}

Result<void> Simulation::simulate(
    const std::stop_token&                               stopToken,
    std::span<const std::shared_ptr<SimulationListener>> additionalListeners)
{
    std::vector<std::shared_ptr<SimulationListener>> listeners(additionalListeners.begin(),
                                                               additionalListeners.end());
    listeners.push_back(std::make_shared<InterruptListener>(stopToken));
    return simulate(std::span<const std::shared_ptr<SimulationListener>>(listeners));
}

Result<void> Simulation::runSimulation(
    std::span<const std::shared_ptr<SimulationListener>> additionalListeners,
    std::shared_ptr<FlightData>&                         flightData)
{
    try
    {
        if (m_status == Status::EXTERNAL)
        {
            throw SimulationException("Cannot simulate imported simulation.");
        }

        // Not in Java: a NaN or an infinity among the inputs is refused here, before an
        // extension or the engine sees it (see validateInputs()).
        if (Result<void> valid = validateInputs(); !valid.has_value())
        {
            return std::unexpected(std::move(valid.error()));
        }

        Result<SimulationConditions> conditions = m_options.toSimulationConditions();
        if (!conditions.has_value())
        {
            // Java: an IllegalArgumentException that leaves simulate() through the finally block.
            return std::unexpected(std::move(conditions.error()));
        }
        const std::shared_ptr<SimulationConditions> simulationConditions =
            std::make_shared<SimulationConditions>(std::move(*conditions));
        simulationConditions->setSimulation(this);

        for (const std::shared_ptr<SimulationExtension>& extension : m_simulationExtensions)
        {
            if (extension == nullptr)
            {
                bug("The simulation holds a null extension");
            }
            extension->initialize(*simulationConditions);
        }

        for (const std::shared_ptr<SimulationListener>& l : additionalListeners)
        {
            simulationConditions->getSimulationListenerList().push_back(l);
        }

        // Java: simulator.getFlightData() in the finally block, whatever way simulate() ends.
        BasicEventSimulationEngine simulator;
        try
        {
            simulator.simulate(simulationConditions);
        }
        catch (...)
        {
            flightData = simulator.getFlightData();
            throw;
        }
        flightData = simulator.getFlightData();
        return {};
    }
    catch (const SimulationCancelledException& e)
    {
        return fail(ErrorCode::CANCELLED, e.what());
    }
    catch (const SimulationException& e)
    {
        return fail(ErrorCode::SIMULATION_ABORTED, e.what());
    }
}

void Simulation::recordSimulation(std::shared_ptr<FlightData> flightData)
{
    // Set simulated info after simulation
    m_simulatedConditions.emplace(m_options);
    m_simulatedConfigurationDescription = describeConfiguration();
    m_simulatedConfigurationModId       = getActiveConfiguration().getModId();
    // Java: simulator.getFlightData() when there is a simulator; the data was dropped before.
    m_simulatedData = std::move(flightData);

    m_status = Status::UPTODATE;
    fireChangeEvent();
}

std::string Simulation::describeConfiguration() const
{
    // Java: descriptor.format(rocket, getId()), the name of the configuration run through the
    // substitutors once more, which finds nothing left to substitute.
    const FlightConfiguration& config = getActiveConfiguration();
    if (m_preferences != nullptr)
    {
        return config.getName(*m_preferences);
    }
    const InMemoryPreferences defaults;
    return config.getName(defaults);
}

WarningSet* Simulation::getSimulatedWarnings() noexcept
{
    if (m_simulatedData == nullptr)
    {
        return nullptr;
    }
    return &m_simulatedData->getWarningSet();
}

const WarningSet* Simulation::getSimulatedWarnings() const noexcept
{
    if (m_simulatedData == nullptr)
    {
        return nullptr;
    }
    const FlightData& data = *m_simulatedData;
    return &data.getWarningSet();
}

bool Simulation::hasSimulationData() const noexcept
{
    const std::shared_ptr<FlightData>& data = getSimulatedData();
    if (data == nullptr)
    {
        return false;
    }
    return data->getBranchCount() != 0;
}

std::unique_ptr<Simulation> Simulation::copy() const
{
    std::unique_ptr<Simulation> copy = std::make_unique<Simulation>(CloneKey{}, *this);

    copy->m_status = Status::NOT_SIMULATED;
    copy->m_simulatedConditions.reset();
    copy->m_simulatedConfigurationDescription.reset();
    copy->m_simulatedData               = nullptr;
    copy->m_simulatedConfigurationModId = ModId::invalid();

    return copy;
}

std::unique_ptr<Simulation> Simulation::clone() const
{
    return clone(true);
}

std::unique_ptr<Simulation> Simulation::clone(bool includeSimulatedData) const
{
    std::unique_ptr<Simulation> clone = std::make_unique<Simulation>(CloneKey{}, *this);

    clone->connectConditionListener();
    if (includeSimulatedData)
    {
        if (m_simulatedData != nullptr)
        {
            clone->m_simulatedData = std::make_shared<FlightData>(m_simulatedData->clone());
        }
    }
    else
    {
        clone->m_simulatedData = nullptr;
    }

    return clone;
}

std::unique_ptr<Simulation> Simulation::cloneForUndo() const
{
    return std::make_unique<Simulation>(CloneKey{}, *this);
}

void Simulation::loadFrom(const Simulation& simulation)
{
    m_name                              = simulation.m_name;
    m_configId                          = simulation.m_configId;
    m_simulatedConfigurationDescription = simulation.m_simulatedConfigurationDescription;
    m_simulatedConfigurationModId       = simulation.m_simulatedConfigurationModId;
    m_options.copyConditionsFrom(simulation.m_options);
    m_landingDispersionSettings = simulation.m_landingDispersionSettings;
    if (!simulation.m_simulatedConditions.has_value())
    {
        m_simulatedConditions.reset();
    }
    else if (!m_simulatedConditions.has_value())
    {
        m_simulatedConditions.emplace(*simulation.m_simulatedConditions);
    }
    else
    {
        m_simulatedConditions->copyConditionsFrom(*simulation.m_simulatedConditions);
    }
    copyExtensionsFrom(simulation.getSimulationExtensions());
    m_status        = simulation.m_status;
    m_simulatedData = simulation.m_simulatedData;
    if (isStatusUpToDate(m_status) && !m_configId.hasError())
    {
        m_simulatedConfigurationModId = getActiveConfiguration().getModId();
    }
}

std::unique_ptr<Simulation> Simulation::duplicateSimulation(Rocket& newRocket) const
{
    std::unique_ptr<Simulation> newSim =
        m_preferences != nullptr
            ? std::make_unique<Simulation>(m_document, newRocket, *m_preferences)
            : std::make_unique<Simulation>(m_document, newRocket);
    newSim->m_name     = m_name;
    newSim->m_configId = m_configId;
    newSim->m_options.copyConditionsFrom(m_options);
    newSim->m_landingDispersionSettings         = m_landingDispersionSettings;
    newSim->m_simulatedConfigurationDescription = m_simulatedConfigurationDescription;
    for (const std::shared_ptr<SimulationExtension>& c : m_simulationExtensions)
    {
        if (c == nullptr)
        {
            bug("The simulation holds a null extension");
        }
        newSim->m_simulationExtensions.push_back(c->clone());
    }
    // A map of its own, with copies of the appearances.
    *newSim->m_plotAppearances = *m_plotAppearances;

    return newSim;
}

std::unique_ptr<Simulation> Simulation::duplicateForIndependentSimulation() const
{
    std::shared_ptr<Rocket>     rocket = m_rocket->copyRocketWithOriginalId();
    std::unique_ptr<Simulation> copy =
        m_preferences != nullptr ? std::make_unique<Simulation>(nullptr, *rocket, *m_preferences)
                                 : std::make_unique<Simulation>(nullptr, *rocket);
    copy->m_ownedRocket = std::move(rocket);
    copy->m_name        = m_name;
    copy->m_configId    = m_configId;
    // A full options clone, not copyConditionsFrom: the latter copies only the
    // launch conditions, leaving the rest of the copy on preference defaults.
    copy->m_options = SimulationOptions(m_options);
    copy->connectConditionListener();
    copy->m_landingDispersionSettings = m_landingDispersionSettings;
    // Java: copy.listeners = new ArrayList<>(); the constructor's document listener is the only
    // one there could be, and there is no document.
    for (const std::shared_ptr<SimulationExtension>& extension : m_simulationExtensions)
    {
        if (extension == nullptr)
        {
            bug("The simulation holds a null extension");
        }
        copy->m_simulationExtensions.push_back(extension->clone());
    }

    return copy;
}

bool Simulation::operator==(const Simulation& other) const
{
    if (this == &other)
    {
        return true;
    }

    return m_name == other.m_name && m_configId == other.m_configId &&
           m_options == other.m_options &&
           m_landingDispersionSettings == other.m_landingDispersionSettings &&
           *m_plotAppearances == *other.m_plotAppearances &&
           simulationExtensionsEqual(m_simulationExtensions, other.m_simulationExtensions);
}

bool Simulation::simulationExtensionsEqual(
    const std::vector<std::shared_ptr<SimulationExtension>>& a,
    const std::vector<std::shared_ptr<SimulationExtension>>& b)
{
    if (&a == &b)
    {
        return true;
    }
    if (a.size() != b.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); i++)
    {
        const std::shared_ptr<SimulationExtension>& extA = a[i];
        const std::shared_ptr<SimulationExtension>& extB = b[i];
        if (extA == extB)
        {
            continue;
        }
        if (extA == nullptr || extB == nullptr)
        {
            return false;
        }
        if (extA->getId() != extB->getId())
        {
            return false;
        }
        // Java: configEqual(), the same keys with equal values.
        if (!extA->getConfig().sameEntries(extB->getConfig()))
        {
            return false;
        }
    }

    return true;
}

std::optional<PlotAppearance> Simulation::getPlotAppearance(const FlightDataType& type) const
{
    const std::string& symbol = type.getSymbol();
    if (Strings::isEmpty(symbol))
    {
        return std::nullopt;
    }
    const auto appearance = m_plotAppearances->find(symbol);
    if (appearance == m_plotAppearances->end())
    {
        return std::nullopt;
    }
    return appearance->second;
}

void Simulation::setPlotAppearance(const FlightDataType&                type,
                                   const std::optional<PlotAppearance>& appearance)
{
    const std::string& symbol = type.getSymbol();
    if (Strings::isEmpty(symbol))
    {
        return;
    }
    if (!appearance.has_value() || appearance->isEmpty())
    {
        m_plotAppearances->erase(symbol);
    }
    else
    {
        m_plotAppearances->insert_or_assign(symbol, *appearance);
    }
    fireChangeEvent();
}

std::map<std::string, PlotAppearance> Simulation::getPlotAppearances() const
{
    return *m_plotAppearances;
}

void Simulation::setPlotAppearancesInternal(
    const std::map<std::string, PlotAppearance>& appearances, bool notify)
{
    m_plotAppearances->clear();
    for (const auto& [symbol, appearance] : appearances)
    {
        if (!appearance.isEmpty())
        {
            m_plotAppearances->insert_or_assign(symbol, appearance);
        }
    }
    if (notify)
    {
        fireChangeEvent();
    }
}

std::string_view name(Simulation::Status status) noexcept
{
    switch (status)
    {
        case Simulation::Status::UPTODATE:
            return "UPTODATE";
        case Simulation::Status::LOADED:
            return "LOADED";
        case Simulation::Status::OUTDATED:
            return "OUTDATED";
        case Simulation::Status::EXTERNAL:
            return "EXTERNAL";
        case Simulation::Status::NOT_SIMULATED:
            return "NOT_SIMULATED";
        case Simulation::Status::CANT_RUN:
            return "CANT_RUN";
        case Simulation::Status::ABORTED:
            return "ABORTED";
    }
    return "NOT_SIMULATED";  // not reached: the switch covers every status
}

std::string_view displayName(Simulation::Status status) noexcept
{
    // The English texts of the keys Simulation.Status.<name>.
    switch (status)
    {
        case Simulation::Status::UPTODATE:
            return "Up To Date";
        case Simulation::Status::LOADED:
            return "Loaded From File";
        case Simulation::Status::OUTDATED:
            return "Out of Date";
        case Simulation::Status::EXTERNAL:
            return "Imported External Data";
        case Simulation::Status::NOT_SIMULATED:
            return "Not Simulated Yet";
        case Simulation::Status::CANT_RUN:
            return "Simulation Can't Be Run";
        case Simulation::Status::ABORTED:
            return "<i><b>ABORTED</b></i>";
    }
    return "Not Simulated Yet";  // not reached: the switch covers every status
}

std::string_view description(Simulation::Status status) noexcept
{
    // The English texts of the keys Simulation.Status.Description.<name>.
    switch (status)
    {
        case Simulation::Status::UPTODATE:
            return "<i>Up to date</i>";
        case Simulation::Status::LOADED:
            return "<i>Loaded from file</i>";
        case Simulation::Status::OUTDATED:
            return "<i>Out of date</i>";
        case Simulation::Status::EXTERNAL:
            return "<i>Imported data</i>";
        case Simulation::Status::NOT_SIMULATED:
            return "<i>Not simulated yet</i> <br>Click <i><b>Run simulations</b></i> to simulate.";
        case Simulation::Status::CANT_RUN:
            return "<i>Errors in simulation prevent running</i>";
        case Simulation::Status::ABORTED:
            return "<i><b>Simulation Aborted</b></i>";
    }
    return "<i>Not simulated yet</i>";  // not reached: the switch covers every status
}

std::string getDescription(Simulation::Status status, Simulation& simulation)
{
    const std::string_view text = description(status);
    if (simulation.getStatus() != Simulation::Status::ABORTED)
    {
        return std::string{text};
    }

    std::string builder;

    // We'll put every abort event on a new line (note that more than one branch can abort)
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data != nullptr)
    {
        const FlightData& simulated = *data;
        for (std::size_t b = 0; b < simulated.getBranchCount(); b++)
        {
            const FlightEvent* abortEvent =
                simulated.getBranch(b).getFirstEvent(FlightEvent::Type::SIM_ABORT);
            if (abortEvent != nullptr)
            {
                const SimulationAbort* abort = abortEvent->getAbort();
                QTROCKET_ASSERT(abort != nullptr);
                builder += text;
                builder += "<i>: ";
                builder += abort->toString();
                builder += "</i><br>";
            }
        }
    }

    // It shouldn't be possible to abort without an abort event. But just in case...
    if (!builder.empty())
    {
        return builder;
    }
    return std::string{text};
}

}  // namespace QtRocket
