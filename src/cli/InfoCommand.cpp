#include "QtRocket/cli/InfoCommand.h"

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/cli/CliArguments.h"
#include "QtRocket/cli/CliEnvironment.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StabilityUnits.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// What is printed of one flight configuration.
struct ConfigurationNumbers
{
    double     length{0};
    Coordinate cp;
    Coordinate worstCp;
    WarningSet warnings;
    RigidBody  launch;
    RigidBody  structure;
    /// The configuration's stability units (RocketInfo's stabilityUnits); they convert the
    /// margin to calibers.
    std::unique_ptr<UnitGroup::StabilityUnitGroup> stabilityUnits;
};

/// The numbers of @p configuration at the Mach number @p mach (see InfoCommand), in the order of
/// RocketInfoContextHelper.computePhysics(): the length, the CP, the masses.
[[nodiscard]] ConfigurationNumbers computeNumbers(const FlightConfiguration& configuration,
                                                  double                     mach)
{
    const double length = configuration.getLength();

    // The point of aero.json: theta 0 (FlightConditions starts with it).
    BarrowmanCalculator calculator;
    FlightConditions    conditions(configuration);
    conditions.setMach(mach);
    conditions.setAOA(0);
    conditions.setRollRate(0);
    conditions.setTheta(0);
    WarningSet       cpWarnings;
    const Coordinate cp = calculator.getCP(configuration, conditions, &cpWarnings);

    // RocketInfoContextHelper.calculateCp() with useWorstCp, in RocketPanel's conditions.
    FlightConditions worstConditions(configuration);
    worstConditions.setMach(mach);
    worstConditions.setAOA(0);
    worstConditions.setRollRate(0);
    WarningSet warnings;
    calculator.checkGeometry(configuration, configuration.getRocket(), &warnings);
    const Coordinate worstCp = calculator.getWorstCP(configuration, worstConditions, &warnings);

    return ConfigurationNumbers{.length         = length,
                                .cp             = cp,
                                .worstCp        = worstCp,
                                .warnings       = std::move(warnings),
                                .launch         = MassCalculator::calculateLaunch(configuration),
                                .structure      = MassCalculator::calculateStructure(configuration),
                                .stabilityUnits = StabilityUnits::stabilityUnits(configuration)};
}

/// Whether @p cp is a CP: one with lift (RocketInfoContextHelper.applyCgAndCp()).
[[nodiscard]] bool hasLift(const Coordinate& cp)
{
    return cp.weight > MathUtil::kEpsilon;
}

/// Whether @p body has a centre of mass (RocketInfoContextHelper.applyCgAndCp()).
[[nodiscard]] bool hasMass(const RigidBody& body)
{
    return body.getMass() > MassCalculator::kMinMass;
}

/// The caliber unit of @p stabilityUnits.
[[nodiscard]] const Unit& calibers(const UnitGroup& stabilityUnits)
{
    const Unit* caliber = stabilityUnits.getUnit("cal");
    QTROCKET_ASSERT(caliber != nullptr);
    return *caliber;
}

/// Prints the numbers of a configuration, its heading excepted.
void printNumbers(const ConfigurationNumbers& numbers, double mach, CliOutput& output)
{
    output.outLine("  length: {} m", numbers.length);
    output.outLine("  mass with motors: {} kg", numbers.launch.getMass());
    output.outLine("  mass without motors: {} kg", numbers.structure.getMass());
    if (hasMass(numbers.launch))
    {
        output.outLine("  CG: {} m", numbers.launch.getCM().x);
    }
    else
    {
        output.outLine("  CG: none (no mass)");
    }
    if (hasLift(numbers.cp))
    {
        output.outLine("  CP at Mach {}, angle of attack 0: {} m", mach, numbers.cp.x);
    }
    else
    {
        output.outLine("  CP at Mach {}, angle of attack 0: none (no lift)", mach);
    }
    output.outLine("  CNa at Mach {}, angle of attack 0: {} per rad", mach, numbers.cp.weight);
    if (hasLift(numbers.worstCp))
    {
        output.outLine("  worst CP at Mach {}: {} m", mach, numbers.worstCp.x);
    }
    else
    {
        output.outLine("  worst CP at Mach {}: none (no lift)", mach);
    }
    if (hasLift(numbers.worstCp) && hasMass(numbers.launch))
    {
        // RocketInfo.getStabilityValue(): cp - cg, shown in calibers.
        const double margin = numbers.worstCp.x - numbers.launch.getCM().x;
        output.outLine("  stability: {} cal ({} m)",
                       calibers(*numbers.stabilityUnits).toUnit(margin), margin);
    }
    else
    {
        output.outLine("  stability: none");
    }
    if (numbers.warnings.empty())
    {
        output.outLine("  warnings: none");
        return;
    }
    output.outLine("  warnings:");
    for (const Warning& warning : numbers.warnings)
    {
        output.outLine("    {}", warning.toString());
    }
}

/// The index (for Rocket::getFlightConfigurationByIndex(i, true)) of the configuration
/// @p wanted names in @p rocket: the first with that exact name, else the number; nullopt for
/// none.
[[nodiscard]] std::optional<int> findConfiguration(Rocket& rocket, std::string_view wanted,
                                                   const Preferences& preferences)
{
    const int count = rocket.getConfigurationCount();
    for (int index = 0; index <= count; index++)
    {
        if (rocket.getFlightConfigurationByIndex(index, true).getName(preferences) == wanted)
        {
            return index;
        }
    }
    if (const std::optional<int> number = Strings::parseInt(wanted);
        number.has_value() && *number >= 0 && *number <= count)
    {
        return number;
    }
    return std::nullopt;
}

/// Prints the design's heading: its file, rocket, stages and simulations.
void printDesign(const CliArguments& arguments, const OpenRocketDocument& document,
                 CliOutput& output)
{
    const Rocket& rocket = document.getRocket();
    output.outLine("Design: {}", arguments.file);
    output.outLine("Rocket: {}", rocket.getName());
    output.outLine("Stages: {}", rocket.getStageCount());
    output.outLine("Simulations: {}", document.getSimulationCount());
    std::size_t number = 1;
    for (const auto& simulation : document.getSimulations())
    {
        output.outLine("  {}: {}", number, simulation->getName());
        number++;
    }
    output.outLine("Flight configurations: {}", rocket.getConfigurationCount() + 1);
}

}  // namespace

void InfoCommand::printConfiguration(Rocket& rocket, int index, double mach,
                                     const Preferences& preferences, CliOutput& output)
{
    const FlightConfiguration& configuration = rocket.getFlightConfigurationByIndex(index, true);
    std::string                heading =
        std::format("Configuration {}: {}", index, configuration.getName(preferences));
    if (index == 0)
    {
        heading += " (default)";
    }
    if (configuration.getId() == rocket.getSelectedConfiguration().getId())
    {
        heading += " (selected)";
    }
    output.outLine("{}", heading);
    printNumbers(computeNumbers(configuration, mach), mach, output);
}

ExitCode InfoCommand::run(const CliArguments& arguments, CliEnvironment& environment,
                          CliOutput& output)
{
    const Result<LoadedDocument> loaded =
        environment.load(DataDirectory::pathFromArgument(arguments.file));
    if (!loaded)
    {
        output.errLine("QtRocket_cli: cannot load '{}': {}", arguments.file,
                       loaded.error().message);
        return ExitCode::LOAD;
    }
    output.warningLines(loaded->warnings);
    OpenRocketDocument& document = *loaded->document;
    Rocket&             rocket   = document.getRocket();
    if (const Result<int> settled = settleAutomaticDimensions(rocket); !settled)
    {
        output.errLine("warning: {}", settled.error().message);
    }

    const Preferences& preferences = environment.preferences();
    const double       mach        = arguments.mach.value_or(preferences.getDefaultMach());
    std::vector<int>   indices;
    if (arguments.configuration.has_value())
    {
        const std::optional<int> index =
            findConfiguration(rocket, *arguments.configuration, preferences);
        if (!index.has_value())
        {
            output.errLine("QtRocket_cli: '{}' has no flight configuration '{}'", arguments.file,
                           *arguments.configuration);
            return ExitCode::USAGE;
        }
        indices.push_back(*index);
    }
    else
    {
        for (int index = 0; index <= rocket.getConfigurationCount(); index++)
        {
            indices.push_back(index);
        }
    }

    printDesign(arguments, document, output);
    for (const int index : indices)
    {
        printConfiguration(rocket, index, mach, preferences, output);
    }
    return ExitCode::OK;
}

double InfoCommand::caliber(const FlightConfiguration& configuration)
{
    return StabilityUnits::calculateCaliber(configuration);
}

}  // namespace QtRocket
