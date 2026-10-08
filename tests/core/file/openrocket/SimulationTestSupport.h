#pragma once

// What the tests of the handlers of a file's simulations share (SimulationsHandler,
// SingleSimulationHandler, SimulationPlotAppearanceHandler, LandingDispersionSettingsHandler):
// a document whose rocket has a flight configuration with a motor, a way to run a
// <simulations> element (or one <simulation>) through its handler, and a description of the
// outcome in the notation of the Java probe the expectations come from. Test-only.
//
// The probe is SimProbe (run 9b, part S3): it runs the same element through OpenRocket's
// SimulationsHandler (or SingleSimulationHandler) alone, as runSimulationsCase() does here,
// under OpenRocket's test preferences, and prints the warnings of the load, the exception,
// what the parent's closeElement() was given, the flight configurations of the rocket, and
// every simulation of the document. Everything is printed in ASCII (see ascii()), so that an
// expectation is a plain string literal.

#include <cstddef>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/file/openrocket/SingleSimulationHandler.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/EntryTestSupport.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// The flight configuration of SimulationFixture's rocket that has a motor.
inline constexpr std::string_view kSimulationMotorFcid = "11111111-1111-1111-1111-111111111111";

/// The class of @p extension as SimProbe prints it (Java: getClass().getSimpleName()).
[[nodiscard]] inline std::string_view extensionClassName(const SimulationExtension& extension)
{
    if (dynamic_cast<const AirStart*>(&extension) != nullptr)
    {
        return "AirStart";
    }
    if (dynamic_cast<const RollControl*>(&extension) != nullptr)
    {
        return "RollControl";
    }
    if (dynamic_cast<const JavaCode*>(&extension) != nullptr)
    {
        return "JavaCode";
    }
    if (dynamic_cast<const ScriptingExtension*>(&extension) != nullptr)
    {
        return "ScriptingExtension";
    }
    if (dynamic_cast<const UnknownSimulationExtension*>(&extension) != nullptr)
    {
        return "UnknownSimulationExtension";
    }
    return "?";
}

/// @p text with every line indented by two more spaces.
[[nodiscard]] inline std::string indented(std::string_view text)
{
    std::string result;
    for (const std::string& line : Strings::split(text, '\n'))
    {
        if (!line.empty())
        {
            result += "  " + line + "\n";
        }
    }
    return result;
}

/// The extensions of @p simulation as SimProbe prints them, one line each, indented by four
/// spaces: "ext <class> id=<id> name='<name>' config=<the entries, see describe(Config)>".
[[nodiscard]] inline std::string describeExtensions(const Simulation& simulation)
{
    std::string text;
    for (const std::shared_ptr<SimulationExtension>& extension :
         simulation.getSimulationExtensions())
    {
        text += ascii(std::format("    ext {} id={} name='{}' config={}",
                                  extensionClassName(*extension), extension->getId(),
                                  extension->getName(), describe(extension->getConfig()))) +
                "\n";
    }
    return text;
}

/// The plot appearances of @p simulation as SimProbe prints them, in the order of their
/// symbols, one line each, indented by four spaces: "plot '<symbol>': color=<red>,<green>,
/// <blue>,<alpha> style=<LineStyle constant>", with "null" for what is not overridden.
[[nodiscard]] inline std::string describePlotAppearances(const Simulation& simulation)
{
    std::string text;
    for (const auto& [symbol, appearance] : simulation.getPlotAppearances())
    {
        const std::optional<Color>&     color = appearance.getColor();
        const std::optional<LineStyle>& style = appearance.getLineStyle();
        text += ascii(std::format("    plot '{}': color={} style={}", symbol,
                                  color.has_value()
                                      ? std::format("{},{},{},{}", color->red(), color->green(),
                                                    color->blue(), color->alpha())
                                      : "null",
                                  style.has_value() ? lineStyleName(*style) : "null")) +
                "\n";
    }
    return text;
}

/// One simulation as SimProbe prints it, every line ended:
/// - "  sim[<index>] name='<name>' stored=<status> presync=<status> status=<status> fcid=<id>
///   simulated=<...>": the status as the handler stored it (Simulation::getStoredStatus());
///   the status getStatus() would give at that moment (asked of a clone(), because asking
///   changes the stored status); the status after Simulation::syncModId(), the next thing the
///   loader does with every simulation (getStatus()); the flight configuration id ("(random)"
///   for a random one, see describeId()); and whether the simulated conditions equal the
///   options ("equal", "differs", or "null" without simulated conditions);
/// - indented by four spaces, the lines of describeConditions() that differ from
///   kBaselineOptions, the options of a <conditions> element that has nothing but a
///   <configid>;
/// - describeExtensions() and describePlotAppearances();
/// - "    dispersion: <LandingDispersionSettings::toString()>" when the simulation has landing
///   dispersion settings (SimProbe prints OpenRocket's settings in the same form: the number
///   of runs and the seed, and the uncertainties in the order of OpenRocket's parameters);
/// - describeFlightData() of the simulated data, indented by two more spaces.
/// Calling this changes the simulation as the loader's next step does (syncModId() and
/// getStatus()).
[[nodiscard]] inline std::string describeSimulation(std::size_t index, Simulation& simulation,
                                                    const Rocket&            rocket,
                                                    const FlightDescription& how = {})
{
    const std::string_view stored  = name(simulation.getStoredStatus());
    const std::string_view presync = name(simulation.clone()->getStatus());
    simulation.syncModId();
    const std::string_view   status    = name(simulation.getStatus());
    const SimulationOptions* simulated = simulation.getSimulatedConditions();
    std::string_view         same      = "null";
    if (simulated != nullptr)
    {
        same = *simulated == simulation.getOptions() ? "equal" : "differs";
    }
    std::string text =
        ascii(std::format("  sim[{}] name='{}' stored={} presync={} status={} fcid={} simulated={}",
                          index, simulation.getName(), stored, presync, status,
                          describeId(simulation.getId().key(), how), same)) +
        "\n";
    const std::vector<std::string> options = describeConditions(simulation.getOptions());
    for (std::size_t i = 0; i < options.size(); i++)
    {
        if (i >= kBaselineOptions.size() || options.at(i) != kBaselineOptions.at(i))
        {
            text += "    " + options.at(i) + "\n";
        }
    }
    text += describeExtensions(simulation);
    text += describePlotAppearances(simulation);
    if (const std::optional<LandingDispersionSettings>& settings =
            simulation.getLandingDispersionSettings())
    {
        text += ascii("    dispersion: " + settings->toString()) + "\n";
    }
    text += indented(describeFlightData(simulation.getSimulatedData().get(), rocket, how));
    return text;
}

/// A FlightDataFixture (the rocket "R" with the stage "S", the body tube "B" and the
/// parachute "P") for the handlers of simulations:
/// - the preference store holds what OpenRocket's tests run under
///   (storeJavaTestPreferences()), the store SimProbe ran with;
/// - the rocket has one flight configuration, kSimulationMotorFcid, in which the body tube
///   holds a motor, so that a simulation of that configuration can be run and one of any other
///   configuration cannot (Simulation::getStatus() is CANT_RUN);
/// - the context has the registry of the extensions QtRocket ships
///   (SimulationExtensionRegistry::bundled()); OpenRocket's own providers know the same four
///   ids and four more.
class SimulationFixture : public FlightDataFixture
{
public:
    SimulationFixture()
    {
        storeJavaTestPreferences(preferences());
        const FlightConfigurationId fcid = FlightConfigurationId::fromString(kSimulationMotorFcid);
        rocket().createFlightConfiguration(fcid);
        addMotor(tube(), fcid, makeEmbeddedTestMotor("ZZ99", 10.0, "digest"));
        context().setSimulationExtensionRegistry(&m_registry);
    }

private:
    SimulationExtensionRegistry m_registry{SimulationExtensionRegistry::bundled()};
};

/// What the rocket and the document of @p fixture hold after a run, as SimProbe prints it,
/// every line ended:
/// - "  configs: <id> <id> ..." the flight configurations of the rocket, in their order;
/// - "  sims: <number of simulations>";
/// - describeSimulation() of every simulation of the document, in order.
[[nodiscard]] inline std::string describeDocumentSimulations(SimulationFixture& fixture)
{
    std::string text = "  configs:";
    for (const FlightConfigurationId& fcid : fixture.rocket().getIds())
    {
        text += " " + describeId(fcid.key(), fixture.description());
    }
    text += std::format("\n  sims: {}\n", fixture.document().getSimulationCount());
    for (std::size_t i = 0; i < fixture.document().getSimulationCount(); i++)
    {
        text += describeSimulation(i, *fixture.document().getSimulation(i), fixture.rocket(),
                                   fixture.description());
    }
    return text;
}

/// Whether @p setup has the directive "@element": the XML of the case is one <simulation>
/// element for a SingleSimulationHandler, and not a <simulations> element.
[[nodiscard]] inline bool isSingleSimulation(std::string_view setup)
{
    return setup.contains("@element\n");
}

/// @p setup without its "@element" line, which is no directive of the fixture.
[[nodiscard]] inline std::string fixtureDirectives(std::string_view setup)
{
    return replaceAll(std::string(setup), "@element\n", "");
}

/// Runs @p xml through its handler in @p fixture, to which @p setup is applied first, and
/// describes the outcome as SimProbe prints it: a line end (so that an expectation written as
/// a raw string starts on a line of its own), describeRun(), and
/// describeDocumentSimulations(). The handler is a SimulationsHandler, or, with the directive
/// "@element" in @p setup, a SingleSimulationHandler.
[[nodiscard]] inline std::string runSimulationsCase(SimulationFixture& fixture,
                                                    std::string_view setup, std::string_view xml)
{
    fixture.apply(fixtureDirectives(setup));
    std::string text = "\n";
    if (isSingleSimulation(setup))
    {
        SingleSimulationHandler handler(fixture.context());
        text += describeRun(runHandler(handler, xml));
    }
    else
    {
        SimulationsHandler handler(fixture.context());
        text += describeRun(runHandler(handler, xml));
    }
    return text + describeDocumentSimulations(fixture);
}

/// runSimulationsCase() in a new SimulationFixture.
[[nodiscard]] inline std::string runSimulationsCase(std::string_view setup, std::string_view xml)
{
    SimulationFixture fixture;
    return runSimulationsCase(fixture, setup, xml);
}

/// One <simulations> element (or <simulation> element) that SimProbe ran through OpenRocket,
/// with what OpenRocket made of it and, where QtRocket's handlers do something else on
/// purpose, what they make of it. The tables of the handler tests are written by the probe's
/// script (gen_tables.py) from the probe's output; what QtRocket does differently is written
/// by hand from the decision that asks for it.
struct SimulationCase
{
    /// The probe's name of the case ("sim: ..." and "sim2: ..." for the cases of the scout's
    /// EdgeProbe, "s3: ..." for the ones added with the handlers).
    std::string_view name;
    /// The directives of the case, one per line (FlightDataFixture::apply(), and "@element").
    std::string_view setup;
    /// The element.
    std::string_view xml;
    /// What OpenRocket makes of it, as runSimulationsCase() prints an outcome.
    std::string_view java;
    /// What QtRocket makes of it, when that is not what OpenRocket makes of it; else empty.
    std::string_view qtrocket;
    /// Why QtRocket differs; empty when it does not.
    std::string_view why;
};

/// What GoogleTest prints for the case of a test that failed: its name (and not its bytes).
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
inline void PrintTo(const SimulationCase& simulationCase, std::ostream* out)
{
    *out << simulationCase.name;
}

/// The name of the test of a case, for INSTANTIATE_TEST_SUITE_P (conditionsTestName()).
[[nodiscard]] inline std::string simulationCaseTestName(
    const ::testing::TestParamInfo<SimulationCase>& info)
{
    return conditionsTestName(info.param.name);
}

/// Expects that running the element of @p simulationCase gives what the case says:
/// OpenRocket's outcome, or QtRocket's own where the case states one with its reason.
inline void expectSimulationCase(const SimulationCase& simulationCase)
{
    const std::string_view expected =
        simulationCase.qtrocket.empty() ? simulationCase.java : simulationCase.qtrocket;
    EXPECT_EQ(runSimulationsCase(simulationCase.setup, simulationCase.xml), std::string(expected))
        << simulationCase.name;
    // A deviation comes with its reason, and only a deviation has one.
    EXPECT_EQ(simulationCase.qtrocket.empty(), simulationCase.why.empty()) << simulationCase.name;
    EXPECT_NE(simulationCase.qtrocket, simulationCase.java) << simulationCase.name;
}

}  // namespace QtRocket::Test
