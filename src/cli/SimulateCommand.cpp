#include "QtRocket/cli/SimulateCommand.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/cli/CliArguments.h"
#include "QtRocket/cli/CliEnvironment.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// The indices of the simulations of @p document that @p arguments ask for; an empty list when
/// --sim names none.
[[nodiscard]] std::vector<std::size_t> selectSimulations(const CliArguments&       arguments,
                                                         const OpenRocketDocument& document)
{
    const std::size_t count = document.getSimulationCount();
    if (arguments.allSimulations)
    {
        std::vector<std::size_t> all;
        all.reserve(count);
        for (std::size_t index = 0; index < count; index++)
        {
            all.push_back(index);
        }
        return all;
    }
    if (!arguments.simulation.has_value())
    {
        return {0};
    }
    for (std::size_t index = 0; index < count; index++)
    {
        if (document.getSimulation(index)->getName() == *arguments.simulation)
        {
            return {index};
        }
    }
    if (const std::optional<int> number = Strings::parseInt(*arguments.simulation);
        number.has_value() && *number >= 1 && std::cmp_less_equal(*number, count))
    {
        return {static_cast<std::size_t>(*number) - 1};
    }
    return {};
}

/// The name of the component an event of @p simulation's flight @p data comes from: found by
/// its id in the rocket that flew (FlightData::getSimulatedRocket()), else in the simulation's
/// rocket; nullopt for an event without a source and for one whose source neither holds.
[[nodiscard]] std::optional<std::string> sourceName(const FlightEvent& event,
                                                    const FlightData&  data,
                                                    const Simulation&  simulation)
{
    const std::optional<Uuid>& id = event.getSourceId();
    if (!id.has_value())
    {
        return std::nullopt;
    }
    if (const std::shared_ptr<const Rocket>& flown = data.getSimulatedRocket(); flown != nullptr)
    {
        if (const RocketComponent* source = flown->findComponent(*id); source != nullptr)
        {
            return source->getName();
        }
    }
    if (const RocketComponent* source = simulation.getRocket().findComponent(*id);
        source != nullptr)
    {
        return source->getName();
    }
    return std::nullopt;
}

/// What an event of @p data says beside its type: the warning of a SIM_WARN event (as the
/// flight's warning set holds it, else the event's own copy, as CsvExport writes it) and the
/// cause of a SIM_ABORT event; nullopt for the others.
[[nodiscard]] std::optional<std::string> eventMessage(const FlightEvent& event,
                                                      const FlightData&  data)
{
    if (event.getType() == FlightEvent::Type::SIM_WARN)
    {
        if (const Warning* warning = data.findWarning(event); warning != nullptr)
        {
            return warning->toString();
        }
        if (const std::shared_ptr<const Warning> own = event.getWarning(); own != nullptr)
        {
            return own->toString();
        }
    }
    if (const SimulationAbort* abort = event.getAbort(); abort != nullptr)
    {
        return abort->toString();
    }
    return std::nullopt;
}

/// Prints the events of the first branch of @p data.
void printEvents(const FlightData& data, const Simulation& simulation, CliOutput& output)
{
    if (data.getBranchCount() == 0)
    {
        output.outLine("  events: none (no flight data)");
        return;
    }
    const FlightDataBranch& branch = data.getBranch(0);
    output.outLine("  events of branch '{}':", branch.getName());
    for (const FlightEvent& event : branch.getEvents())
    {
        std::string line = std::format("    {} s: {}", event.getTime(), name(event.getType()));
        if (const std::optional<std::string> source = sourceName(event, data, simulation))
        {
            line += std::format(" ({})", *source);
        }
        if (const std::optional<std::string> message = eventMessage(event, data))
        {
            line += ": " + *message;
        }
        output.outLine("{}", line);
    }
}

/// Prints the summary of the flight @p data.
void printSummary(const FlightData& data, CliOutput& output)
{
    output.outLine("  apogee: {} m", data.getMaxAltitude());
    output.outLine("  max velocity: {} m/s", data.getMaxVelocity());
    output.outLine("  max acceleration: {} m/s^2", data.getMaxAcceleration());
    output.outLine("  max Mach number: {}", data.getMaxMachNumber());
    output.outLine("  time to apogee: {} s", data.getTimeToApogee());
    output.outLine("  flight time: {} s", data.getFlightTime());
    output.outLine("  ground hit velocity: {} m/s", data.getGroundHitVelocity());
    output.outLine("  launch rod velocity: {} m/s", data.getLaunchRodVelocity());
    output.outLine("  deployment velocity: {} m/s", data.getDeploymentVelocity());
    output.outLine("  optimum delay: {} s", data.getOptimumDelay());
}

/// Prints the warnings of the flight @p data.
void printWarnings(const FlightData& data, CliOutput& output)
{
    const WarningSet& warnings = data.getWarningSet();
    if (warnings.empty())
    {
        output.outLine("  warnings: none");
        return;
    }
    output.outLine("  warnings:");
    for (const Warning& warning : warnings)
    {
        output.outLine("    {}", warning.toString());
    }
}

/// The cause of the first SIM_ABORT event of @p data, in the order of its branches.
[[nodiscard]] std::string abortCause(const FlightData& data)
{
    for (const std::shared_ptr<FlightDataBranch>& branch : data.getBranches())
    {
        for (const FlightEvent& event : branch->getEvents())
        {
            if (const SimulationAbort* abort = event.getAbort(); abort != nullptr)
            {
                return abort->toString();
            }
        }
    }
    return "unknown cause";
}

/// Checks the CSV file @p csv before any simulation of the design @p design runs (see
/// SimulateCommand): ExitCode::OK, or the code to stop with, its reason written.
[[nodiscard]] ExitCode checkCsvFile(const std::filesystem::path& csv,
                                    const std::filesystem::path& design, CliOutput& output)
{
    std::error_code             error;
    const std::filesystem::path directory = csv.parent_path();
    if (!directory.empty() && !std::filesystem::is_directory(directory, error))
    {
        output.errLine("QtRocket_cli: cannot write '{}': the directory '{}' does not exist",
                       pathToUtf8(csv), pathToUtf8(directory));
        return ExitCode::IO;
    }
    if (std::filesystem::is_directory(csv, error))
    {
        output.errLine("QtRocket_cli: cannot write '{}': it is a directory", pathToUtf8(csv));
        return ExitCode::IO;
    }
    // False, with an error, when either does not exist.
    if (std::filesystem::equivalent(csv, design, error))
    {
        output.errLine("QtRocket_cli: the CSV file '{}' is the design itself", pathToUtf8(csv));
        return ExitCode::USAGE;
    }
    return ExitCode::OK;
}

/// What a simulation came to.
struct Outcome
{
    /// Whether the simulation failed or aborted.
    bool failed{false};
    /// ExitCode::OK, or the code to stop the command with (a CSV that cannot be written).
    ExitCode stop{ExitCode::OK};
};

/// Runs the simulation at @p index of @p document as @p arguments say and prints it; @p csv is
/// its CSV file, if any.
[[nodiscard]] Outcome runSimulation(const CliArguments&       arguments,
                                    const OpenRocketDocument& document, std::size_t index,
                                    const std::optional<std::filesystem::path>& csv,
                                    const Preferences& preferences, CliOutput& output)
{
    // The document owns the simulation; the pointer keeps it whatever the flight does.
    const std::shared_ptr<Simulation> owner      = document.getSimulation(index);
    Simulation&                       simulation = *owner;
    SimulationOptions&                options    = simulation.getOptions();
    SimulateCommand::applyOptions(arguments, options);
    output.outLine("Simulation {}: {}", index + 1, simulation.getName());
    output.outLine("  configuration: {}", simulation.getActiveConfiguration().getName(preferences));
    output.outLine("  random seed: {}", options.getRandomSeed());
    output.outLine("  time step: {} s", options.getTimeStep());

    if (const Result<void> run = simulation.simulate(); !run)
    {
        output.errLine("QtRocket_cli: simulation {} '{}' failed: {}", index + 1,
                       simulation.getName(), run.error().message);
        return Outcome{.failed = true, .stop = ExitCode::OK};
    }
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        output.errLine("QtRocket_cli: simulation {} '{}' gave no flight data", index + 1,
                       simulation.getName());
        return Outcome{.failed = true, .stop = ExitCode::OK};
    }
    printSummary(*data, output);
    printEvents(*data, simulation, output);
    printWarnings(*data, output);
    const bool aborted = simulation.hasErrors();
    if (aborted)
    {
        output.errLine("QtRocket_cli: simulation {} '{}' was aborted: {}", index + 1,
                       simulation.getName(), abortCause(*data));
    }
    if (csv.has_value())
    {
        const ExitCode written = SimulateCommand::writeCsv(*csv, simulation, *data, output);
        if (written == ExitCode::SIMULATION)
        {
            return Outcome{.failed = true, .stop = ExitCode::OK};
        }
        if (written != ExitCode::OK)
        {
            return Outcome{.failed = aborted, .stop = written};
        }
    }
    return Outcome{.failed = aborted, .stop = ExitCode::OK};
}

}  // namespace

ExitCode SimulateCommand::run(const CliArguments& arguments, CliEnvironment& environment,
                              CliOutput& output)
{
    const std::filesystem::path  design = DataDirectory::pathFromArgument(arguments.file);
    const Result<LoadedDocument> loaded = environment.load(design);
    if (!loaded)
    {
        output.errLine("QtRocket_cli: cannot load '{}': {}", arguments.file,
                       loaded.error().message);
        return ExitCode::LOAD;
    }
    output.warningLines(loaded->warnings);
    const OpenRocketDocument& document = *loaded->document;
    if (document.getSimulationCount() == 0)
    {
        output.errLine("QtRocket_cli: '{}' has no simulations", arguments.file);
        return ExitCode::USAGE;
    }
    const std::vector<std::size_t> selected = selectSimulations(arguments, document);
    if (selected.empty())
    {
        output.errLine("QtRocket_cli: '{}' has no simulation '{}'", arguments.file,
                       arguments.simulation.value_or(""));
        return ExitCode::USAGE;
    }

    // The CSV file of each selected simulation, all checked before the first one runs.
    std::vector<std::optional<std::filesystem::path>> csvFiles(selected.size());
    if (arguments.csv.has_value())
    {
        const std::filesystem::path csv = DataDirectory::pathFromArgument(*arguments.csv);
        for (std::size_t i = 0; i < selected.size(); i++)
        {
            const std::filesystem::path file =
                selected.size() > 1 ? numberedCsvPath(csv, selected[i] + 1) : csv;
            if (const ExitCode checked = checkCsvFile(file, design, output);
                checked != ExitCode::OK)
            {
                return checked;
            }
            csvFiles[i] = file;
        }
    }

    ExitCode code = ExitCode::OK;
    for (std::size_t i = 0; i < selected.size(); i++)
    {
        const Outcome outcome = runSimulation(arguments, document, selected[i], csvFiles[i],
                                              environment.preferences(), output);
        if (outcome.stop != ExitCode::OK)
        {
            return outcome.stop;
        }
        if (outcome.failed)
        {
            code = ExitCode::SIMULATION;
        }
    }
    return code;
}

void SimulateCommand::applyOptions(const CliArguments& arguments, SimulationOptions& options)
{
    if (arguments.calm)
    {
        options.getAverageWindModel().setStandardDeviation(0);
        for (MultiLevelPinkNoiseWindModel::LevelWindModel* level :
             options.getMultiLevelWindModel().getLevels())
        {
            level->setStandardDeviation(0);
        }
    }
    if (arguments.seed.has_value())
    {
        options.setRandomSeedFixed(true);
        options.setRandomSeed(*arguments.seed);
    }
    else
    {
        // SimulationRunDialog.java:122.
        options.randomizeSeedIfNotFixed();
    }
    if (arguments.timeStep.has_value())
    {
        options.setTimeStep(*arguments.timeStep);
    }
}

ExitCode SimulateCommand::writeCsv(const std::filesystem::path& path, Simulation& simulation,
                                   const FlightData& data, CliOutput& output)
{
    const std::vector<const FlightDataType*> fields = data.getBranchCount() == 0
                                                          ? std::vector<const FlightDataType*>{}
                                                          : csvFields(data.getBranch(0));
    if (fields.empty())
    {
        output.errLine("QtRocket_cli: simulation '{}' has no flight data to write to '{}'",
                       simulation.getName(), pathToUtf8(path));
        return ExitCode::SIMULATION;
    }
    const FlightDataBranch&        branch = data.getBranch(0);
    const std::vector<const Unit*> units  = CsvExport::defaultUnits(fields);
    const Result<void>             written =
        CsvExport::exportCsv(path, simulation, branch, fields, units, CsvExport::Options{});
    if (!written)
    {
        output.errLine("QtRocket_cli: {}", written.error().message);
        return written.error().code == ErrorCode::IO ? ExitCode::IO : ExitCode::INTERNAL;
    }
    output.outLine("  CSV: {}", pathToUtf8(path));
    return ExitCode::OK;
}

std::filesystem::path SimulateCommand::numberedCsvPath(const std::filesystem::path& csv,
                                                       std::size_t                  number)
{
    std::filesystem::path fileName = csv.stem();
    fileName += std::format("-{}", number);
    fileName += csv.extension();
    return csv.parent_path() / fileName;
}

std::vector<const FlightDataType*> SimulateCommand::csvFields(const FlightDataBranch& branch)
{
    const FlightDataType* const computationTime =
        &FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME);
    std::vector<const FlightDataType*> fields;
    for (const FlightDataType* type : branch.getTypes())
    {
        if (type != computationTime)
        {
            fields.push_back(type);
        }
    }
    return fields;
}

}  // namespace QtRocket
