#pragma once

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliEnvironment;
class CliOutput;
class FlightConfiguration;
class Preferences;
class Rocket;
struct CliArguments;

/// QtRocket_cli info FILE [--config NAME|NUMBER] [--mach M] [--db FILE]: what OpenRocket's
/// design window shows of a design (swing RocketInfoContextHelper.computePhysics(), RocketPanel
/// and RocketInfo), for each flight configuration as text (QtRocket's own command).
///
/// It loads FILE (the load warnings go to the standard error), settles its automatic dimensions
/// (settleAutomaticDimensions(), so that the numbers are those of the goldens, aero.json and
/// mass.json, which describe the settled design; a design that does not settle gives a warning
/// on the standard error and is printed as it is), and prints:
///
///     Design: <FILE>
///     Rocket: <name>
///     Stages: <count>
///     Simulations: <count>
///       <number>: <name>                      (numbered from 1, as --sim takes them)
///     Flight configurations: <count>
///     Configuration <number>: <name>[ (default)][ (selected)]
///       length: <m> m
///       mass with motors: <kg> kg
///       mass without motors: <kg> kg
///       CG: <m> m
///       CP at Mach <M>, angle of attack 0: <m> m
///       CNa at Mach <M>, angle of attack 0: <CNa> per rad
///       worst CP at Mach <M>: <m> m
///       stability: <cal> cal (<m> m)
///       warnings: none                        (or "warnings:" and a line "    <text>" each)
///
/// for every configuration in the order of Rocket::getFlightConfigurationByIndex(i, true): the
/// default configuration is number 0, the others follow from 1 in the rocket's order; --config
/// takes a configuration's exact name (FlightConfiguration::getName() with the factory
/// preferences, the first that has it) or else its number. Every value is in SI units, written in
/// the shortest form that reads back as the same double (std::format("{}")), beside its unit.
///
/// The numbers, as OpenRocket computes them:
/// - the length: FlightConfiguration::getLength();
/// - the masses: MassCalculator::calculateLaunch() (with motors) and calculateStructure()
///   (without), and the CG the x of the launch centre of mass;
/// - the CP and CNa: BarrowmanCalculator::getCP() in FlightConditions of the configuration with
///   the Mach number M (--mach, or Preferences::getDefaultMach(), 0.3), an angle of attack of 0,
///   a roll rate of 0 and a lateral wind direction (theta) of 0: the point of the goldens'
///   aero.json at those conditions;
/// - the worst CP, as OpenRocket's design window shows it (RocketInfoContextHelper.calculateCp()
///   with useWorstCp): the warnings of AerodynamicCalculator::checkGeometry() on the whole rocket,
///   then AerodynamicCalculator::getWorstCP() at the Mach number M, an angle of attack of 0 and a
///   roll rate of 0, which adds its own warnings; those warnings are the configuration's
///   "warnings";
/// - the stability, as the design window shows it (RocketInfo.getStabilityValue()): the worst CP
///   less the CG, in calibers of the configuration (CaliberUnit.calculateCaliber(), caliber())
///   and in metres.
///
/// Deviations from OpenRocket's design window:
/// - A CP whose CNa is not above MathUtil::kEpsilon is "none (no lift)" (a lone body tube): the
///   window shows a CP of 0 instead of the sentinel getWorstCP() returns (Double.MAX_VALUE), and a
///   stability computed from that 0; here there is no stability then ("stability: none"). A CG of
///   a configuration without mass (not above MassCalculator::kMinMass) is "none (no mass)" (the
///   window: 0), with no stability either.
/// - Every configuration, not only the selected one, and the CP at theta 0 beside the worst CP.
/// - The automatic dimensions are settled first (the window shows the design in whatever state
///   its getters last left it).
class InfoCommand
{
public:
    InfoCommand() = delete;

    /// Runs the command @p arguments (Command::INFO) in @p environment, writing to @p output.
    /// ExitCode::LOAD when FILE does not load, ExitCode::USAGE when --config names no
    /// configuration of the design, ExitCode::OK otherwise.
    [[nodiscard]] static ExitCode run(const CliArguments& arguments, CliEnvironment& environment,
                                      CliOutput& output);

    /// Prints the configuration at @p index of @p rocket (Rocket::getFlightConfigurationByIndex(
    /// index, true)) as run() does, its heading included, with the CP at the Mach number @p mach;
    /// @p preferences name the configuration. The design is printed as it is: run() settles it
    /// first.
    static void printConfiguration(Rocket& rocket, int index, double mach,
                                   const Preferences& preferences, CliOutput& output);

    /// The caliber of @p configuration (CaliberUnit.calculateCaliber(FlightConfiguration)): the
    /// largest fore or aft diameter of the symmetric components among its active components
    /// (FlightConfiguration::getActiveComponents()), or CaliberUnit::kDefaultCaliber when that is
    /// below 0.1 mm.
    [[nodiscard]] static double caliber(const FlightConfiguration& configuration);
};

}  // namespace QtRocket
