#include "simulation/extension/example/ExtensionFlightSupport.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/simulation/BasicEventSimulationEngine.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestRockets.h"
#include "simulation/JitterRemoval.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

namespace
{

/// Double.toString(@p value), as the probe prints a number.
[[nodiscard]] std::string javaDouble(double value)
{
    return Strings::javaDoubleToString(value);
}

/// @p text as the probe's escape() prints it: every character outside printable ASCII as a
/// backslash, 'u' and the four hexadecimal digits of its UTF-16 code unit.
[[nodiscard]] std::string escaped(std::string_view text)
{
    std::string result;
    for (const char16_t unit : Strings::toUtf16(text))
    {
        if (unit >= 0x20 && unit < 0x7F)
        {
            result += static_cast<char>(unit);
        }
        else
        {
            result += std::format("\\u{:04X}", static_cast<unsigned>(unit));
        }
    }
    return result;
}

/// What the spy listener and its clones share.
struct SpyHolder
{
    FinSet* finset{nullptr};
    int     steps{0};
    double  cantStart{std::numeric_limits<double>::quiet_NaN()};
    double  cantLast{std::numeric_limits<double>::quiet_NaN()};
    double  cantMaxAbs{0};
    double  cantEnd{std::numeric_limits<double>::quiet_NaN()};
    int     ends{0};
};

/// The probe's Spy: watches the cant of the fin set of a name in the simulated rocket, after
/// the listeners of the extensions (it is added after them).
class Spy final : public CloneableSimulationListener<Spy>
{
public:
    Spy(std::shared_ptr<SpyHolder> holder, std::string name)
      : m_holder(std::move(holder)), m_name(std::move(name))
    {
    }

    void startSimulation(SimulationStatus& status) override
    {
        for (RocketComponent* c : status.getConfiguration().getActiveComponents())
        {
            auto* finset = dynamic_cast<FinSet*>(c);
            if (finset != nullptr && c->getName() == m_name)
            {
                m_holder->finset = finset;
                break;
            }
        }
        if (m_holder->finset != nullptr)
        {
            m_holder->cantStart = m_holder->finset->getCantAngle();
        }
    }

    void postStep(SimulationStatus& /*status*/) override
    {
        if (m_holder->finset == nullptr)
        {
            return;
        }
        m_holder->steps++;
        m_holder->cantLast = m_holder->finset->getCantAngle();
        m_holder->cantMaxAbs =
            MathUtil::javaMax(m_holder->cantMaxAbs, std::abs(m_holder->cantLast));
    }

    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
        m_holder->ends++;
        if (m_holder->finset != nullptr)
        {
            m_holder->cantEnd = m_holder->finset->getCantAngle();
        }
    }

private:
    std::shared_ptr<SpyHolder> m_holder;
    std::string                m_name;
};

/// A RollControl with the configuration the probe's rollControl() gives it.
[[nodiscard]] std::shared_ptr<RollControl> rollControl()
{
    return std::make_shared<RollControl>();
}

/// The Iso-Haisu with an M1350 in its inner tube and its main fins canted by 0.02 rad.
[[nodiscard]] ExtensionFlightScenario haisu(std::string_view id)
{
    TestIsoHaisu haisu;
    haisu.fins->setCantAngle(0.02);
    const FlightConfigurationId fcid =
        FlightConfigurationId::fromString("11111111-2222-3333-4444-555555555555");
    haisu.rocket->createFlightConfiguration(fcid);
    haisu.inner->setMotorMount(true);
    MotorConfiguration mc(*haisu.inner, fcid);
    mc.setMotor(motorM1350());
    haisu.inner->setMotorConfig(mc, fcid);
    haisu.rocket->setSelectedConfiguration(fcid);
    haisu.rocket->enableEvents();
    ExtensionFlightScenario s;
    s.id            = std::string(id);
    s.rocket        = std::move(haisu.rocket);
    s.fcid          = fcid;
    s.rodLength     = 2.5;
    s.watchedFinSet = "CONTROL";
    return s;
}

/// The Estes Alpha III in TEST_FCID_2 (a C6 with an ejection delay of 3 s), its fins canted by
/// 0.02 rad.
[[nodiscard]] ExtensionFlightScenario alpha(std::string_view id)
{
    TestEstesAlphaIII alpha;
    alpha.fins->setCantAngle(0.02);
    const FlightConfigurationId fcid = testFcid(2);
    alpha.rocket->setSelectedConfiguration(fcid);
    ExtensionFlightScenario s;
    s.id            = std::string(id);
    s.rocket        = std::move(alpha.rocket);
    s.fcid          = fcid;
    s.rodLength     = 1.0;
    s.watchedFinSet = "3 Fin Set";
    return s;
}

/// The two-stage Beta in TEST_FCID_1 with the fins of the booster (@p boosterFinsCanted) or of
/// the sustainer canted by 0.02 rad; the fin set @p watched is the one the probe watches.
[[nodiscard]] ExtensionFlightScenario beta(std::string_view id, bool boosterFinsCanted,
                                           std::string_view watched)
{
    TestBeta beta;
    if (boosterFinsCanted)
    {
        beta.boosterFins->setCantAngle(0.02);
    }
    else
    {
        beta.fins->setCantAngle(0.02);
    }
    ExtensionFlightScenario s;
    s.id            = std::string(id);
    s.rocket        = std::move(beta.rocket);
    s.fcid          = testFcid(1);
    s.rodLength     = 1.0;
    s.watchedFinSet = std::string(watched);
    return s;
}

/// The scenarios on the Iso-Haisu.
[[nodiscard]] std::optional<ExtensionFlightScenario> makeHaisuScenario(std::string_view id)
{
    if (!id.starts_with("haisu-"))
    {
        return std::nullopt;
    }
    ExtensionFlightScenario s = haisu(id);
    if (id == "haisu-roll" || id == "haisu-roll-nojitter" || id == "haisu-roll-airstart")
    {
        s.extensions.push_back(rollControl());
    }
    if (id == "haisu-roll-airstart")
    {
        s.extensions.push_back(std::make_shared<AirStart>());
    }
    if (id == "haisu-airstart-250-20")
    {
        const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
        airStart->setLaunchAltitude(250.0);
        airStart->setLaunchVelocity(20.0);
        s.extensions.push_back(airStart);
    }
    if (id == "haisu-roll-custom")
    {
        const std::shared_ptr<RollControl> roll = rollControl();
        roll->setStartTime(0.25);
        roll->setSetPoint(1.0);
        roll->setFinRate(0.5);
        roll->setMaxFinAngle(0.05);
        roll->setKP(0.01);
        roll->setKI(0.1);
        s.extensions.push_back(roll);
        s.startTime = 0.25;
    }
    if (id == "haisu-roll-missing")
    {
        const std::shared_ptr<RollControl> roll = rollControl();
        roll->setControlFinName("NOPE");
        s.extensions.push_back(roll);
    }
    s.jitterRemoved = id.ends_with("-nojitter");
    return s;
}

/// The scenarios of ExtPins3 (all on the Iso-Haisu): an AirStart on a launch rod that is not
/// vertical, and a flight in a steady wind with RK6, WGS84 and a launch site of its own, without
/// the extensions and with both.
[[nodiscard]] std::optional<ExtensionFlightScenario> makeReviewScenario(std::string_view id)
{
    if (id != "haisu-airstart-tilted" && id != "haisu-plain-windy" &&
        id != "haisu-roll-airstart-windy")
    {
        return std::nullopt;
    }
    ExtensionFlightScenario s = haisu(id);
    if (id == "haisu-airstart-tilted")
    {
        s.rodAngle     = 0.3;
        s.rodDirection = 1.0;
    }
    else
    {
        s.rodAngle     = 0.1;
        s.rodDirection = 2.0;
        s.windy        = true;
    }
    if (id == "haisu-roll-airstart-windy")
    {
        s.extensions.push_back(rollControl());
    }
    if (id != "haisu-plain-windy")
    {
        const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
        airStart->setLaunchAltitude(250.0);
        airStart->setLaunchVelocity(20.0);
        s.extensions.push_back(airStart);
    }
    return s;
}

/// The scenarios on the Estes Alpha III and on the Beta.
[[nodiscard]] std::optional<ExtensionFlightScenario> makeSmallRocketScenario(std::string_view id)
{
    if (id == "alpha-plain")
    {
        return alpha(id);
    }
    if (id == "alpha-roll")
    {
        ExtensionFlightScenario            s    = alpha(id);
        const std::shared_ptr<RollControl> roll = rollControl();
        roll->setControlFinName("3 Fin Set");
        s.extensions.push_back(roll);
        return s;
    }
    if (id == "beta-plain-boostercant")
    {
        return beta(id, true, "3 Fin Set");
    }
    if (id == "beta-roll-sustainer")
    {
        ExtensionFlightScenario            s    = beta(id, true, "3 Fin Set");
        const std::shared_ptr<RollControl> roll = rollControl();
        roll->setControlFinName("3 Fin Set");
        s.extensions.push_back(roll);
        return s;
    }
    if (id == "beta-plain-sustainercant")
    {
        return beta(id, false, "Booster Fins");
    }
    if (id == "beta-roll-booster")
    {
        ExtensionFlightScenario            s    = beta(id, false, "Booster Fins");
        const std::shared_ptr<RollControl> roll = rollControl();
        roll->setControlFinName("Booster Fins");
        s.extensions.push_back(roll);
        return s;
    }
    return std::nullopt;
}

/// The options of a "windy" scenario that differ from the probe's (ExtPins3.windyOptions()).
/// The wind is steady: its turbulence is a random sequence, and QtRocket's is drawn from another
/// generator than Java's (a documented deviation of PinkNoise), so a turbulent flight cannot be
/// compared number by number (it differs from OpenRocket's in the fourth digit).
void setWindyOptions(SimulationOptions& o)
{
    o.getAverageWindModel().setAverage(3.0);
    o.getAverageWindModel().setStandardDeviation(0.0);
    o.getAverageWindModel().setDirection(0.7);
    o.setLaunchAltitude(350);
    o.setLaunchLatitude(28.61);
    o.setLaunchLongitude(-80.6);
    o.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    o.setIsaAtmosphere(false);
    o.setLaunchTemperature(293.15);
    o.setLaunchPressure(97000);
    o.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
}

/// The options the probe gives the simulation of @p scenario.
void setProbeOptions(SimulationOptions& o, const ExtensionFlightScenario& scenario, double timeStep)
{
    o.setLaunchRodLength(scenario.rodLength);
    o.setLaunchIntoWind(false);
    o.setLaunchRodAngle(scenario.rodAngle);
    o.setLaunchRodDirection(scenario.rodDirection);
    o.setWindModelType(WindModelType::AVERAGE);
    o.getAverageWindModel().setAverage(0);
    o.getAverageWindModel().setStandardDeviation(0);
    o.getAverageWindModel().setDirection(0);
    o.setLaunchAltitude(0);
    o.setLaunchLatitude(45);
    o.setLaunchLongitude(0);
    o.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    o.setIsaAtmosphere(true);
    o.setTimeStep(timeStep);
    o.setMaxSimulationTime(1200);
    o.setRandomSeed(0);
    o.setRandomSeedFixed(true);
    o.setGravityModelType(GravityModelType::WGS);
    o.setSimulationStepperMethodChoice(SimulationStepperMethod::RK4);
    if (scenario.windy)
    {
        setWindyOptions(o);
    }
}

/// The cant of the fin set named @p name in @p rocket (the last one of that name, as the
/// probe's loop keeps it), or NaN.
[[nodiscard]] double cantOf(const Rocket& rocket, std::string_view name)
{
    double cant = std::numeric_limits<double>::quiet_NaN();
    for (const RocketComponent* c : rocket.getSelectedConfiguration().getAllComponents())
    {
        const auto* finset = dynamic_cast<const FinSet*>(c);
        if (finset != nullptr && c->getName() == name)
        {
            cant = finset->getCantAngle();
        }
    }
    return cant;
}

/// What a run of a scenario gives.
struct RunResult
{
    std::string                 result{"ok"};
    std::shared_ptr<FlightData> data;
    std::optional<long>         jitterReplacements;
};

/// Simulation::simulate() with @p listeners.
[[nodiscard]] RunResult simulateDirectly(
    Simulation& sim, const std::vector<std::shared_ptr<SimulationListener>>& listeners)
{
    RunResult          run;
    const Result<void> result = sim.simulate(listeners);
    if (!result.has_value())
    {
        run.result = "error:" + result.error().message;
    }
    run.data = sim.getSimulatedData();
    return run;
}

/// As the golden harness runs a simulation (SimulationDumper.dump()): the engine on conditions
/// with the forces listener of the jitter removal first, the listeners of the extensions, then
/// @p listeners, and the conditions listener last.
[[nodiscard]] RunResult simulateWithoutJitter(
    Simulation& sim, const std::vector<std::shared_ptr<SimulationListener>>& listeners)
{
    RunResult                    run;
    Result<SimulationConditions> made = sim.getOptions().toSimulationConditions();
    if (!made.has_value())
    {
        bug("the probe's options make no conditions: " + made.error().message);
    }
    const std::shared_ptr<SimulationConditions> conditions =
        std::make_shared<SimulationConditions>(std::move(*made));
    conditions->setSimulation(&sim);
    const JitterRemoval jitterRemoval;
    conditions->getSimulationListenerList().push_back(jitterRemoval.forcesListener());
    BasicEventSimulationEngine engine;
    try
    {
        for (const std::shared_ptr<SimulationExtension>& extension : sim.getSimulationExtensions())
        {
            extension->initialize(*conditions);
        }
        for (const std::shared_ptr<SimulationListener>& listener : listeners)
        {
            conditions->getSimulationListenerList().push_back(listener);
        }
        conditions->getSimulationListenerList().push_back(jitterRemoval.conditionsListener());
        engine.simulate(conditions);
    }
    catch (const SimulationException& e)
    {
        run.result = std::string("error:") + e.what();
    }
    run.data               = engine.getFlightData();
    run.jitterReplacements = jitterRemoval.replacements();
    return run;
}

/// The probe's line for the column of @p type in @p branch, called @p label.
[[nodiscard]] std::string columnLine(const FlightDataBranch& branch, const FlightDataType& type,
                                     std::string_view label)
{
    const std::vector<double>* values = branch.getView(type);
    if (values == nullptr)
    {
        return std::format("    column {} absent", label);
    }
    std::size_t       nan   = 0;
    std::ptrdiff_t    first = -1;
    const std::size_t size  = values->size();
    for (std::size_t i = 0; i < size; i++)
    {
        if (std::isnan((*values)[i]))
        {
            nan++;
        }
        else if (first < 0)
        {
            first = static_cast<std::ptrdiff_t>(i);
        }
    }
    std::string line =
        std::format("    column {} min={} max={} last={} nan={} first={}", label,
                    javaDouble(branch.getMinimum(type)), javaDouble(branch.getMaximum(type)),
                    javaDouble(values->back()), nan, first);
    for (const std::size_t k : {std::size_t{150}, std::size_t{300}, std::size_t{600}})
    {
        line += std::format(" at{}={}", k, size > k ? javaDouble((*values)[k]) : "none");
    }
    line += " symbol=" + escaped(type.getSymbol());
    return line;
}

/// The probe's "finCantRows" line: the rows of the fin cant column @p cant that hold no value
/// although their time is not before @p startTime, and those that hold one although it is.
[[nodiscard]] std::string finCantRowsLine(const std::vector<double>& time,
                                          const std::vector<double>& cant, double startTime)
{
    int late  = 0;
    int early = 0;
    for (std::size_t i = 0; i < cant.size(); i++)
    {
        const bool nan = std::isnan(cant[i]);
        if (nan && time[i] >= startTime)
        {
            late++;
        }
        if (!nan && time[i] < startTime)
        {
            early++;
        }
    }
    return std::format("    finCantRows nanAtOrAfterStart={} valueBeforeStart={}", late, early);
}

/// The probe's lines for branch @p b of @p data.
void addBranchLines(std::vector<std::string>& lines, const FlightData& data, std::size_t b,
                    double startTime)
{
    const FlightDataBranch& br = data.getBranch(b);
    lines.push_back(std::format(
        "  branch {} rows={} types={} optimumAltitude={} timeToOptimumAltitude={} name={}", b,
        br.getLength(), br.getTypes().size(), javaDouble(br.getOptimumAltitude()),
        javaDouble(br.getTimeToOptimumAltitude()), br.getName()));
    for (const FlightEvent& e : br.getEvents())
    {
        lines.push_back(
            std::format("    event {} t={}", name(e.getType()), javaDouble(e.getTime())));
    }
    if (br.getLength() == 0)
    {
        return;
    }
    const FlightDataType& time = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    lines.push_back(columnLine(br, time, "time"));
    lines.push_back(
        columnLine(br, FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE), "altitude"));
    lines.push_back(
        columnLine(br, FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_TOTAL), "velocity"));
    lines.push_back(
        columnLine(br, FlightDataType::builtin(FlightDataTypeId::TYPE_ROLL_RATE), "rollRate"));
    for (const FlightDataType* t : br.getTypes())
    {
        if (t->getName() == "Control fin cant")
        {
            lines.push_back(columnLine(br, *t, "finCant"));
            const std::vector<double>* times = br.getView(time);
            const std::vector<double>* cants = br.getView(*t);
            if (times == nullptr || cants == nullptr)
            {
                bug("a branch has the time column and the column of every type it lists");
            }
            lines.push_back(finCantRowsLine(*times, *cants, startTime));
        }
    }
}

/// The probe's lines for the flight data @p data: the summary, the warnings and the branches.
void addDataLines(std::vector<std::string>& lines, const FlightData& data, double startTime)
{
    lines.push_back(std::format(
        "  summary maxAltitude={} maxVelocity={} maxAcceleration={} maxMach={} timeToApogee={} "
        "flightTime={} groundHitVelocity={} launchRodVelocity={} deploymentVelocity={} "
        "optimumDelay={}",
        javaDouble(data.getMaxAltitude()), javaDouble(data.getMaxVelocity()),
        javaDouble(data.getMaxAcceleration()), javaDouble(data.getMaxMachNumber()),
        javaDouble(data.getTimeToApogee()), javaDouble(data.getFlightTime()),
        javaDouble(data.getGroundHitVelocity()), javaDouble(data.getLaunchRodVelocity()),
        javaDouble(data.getDeploymentVelocity()), javaDouble(data.getOptimumDelay())));
    for (const Warning& w : data.getWarningSet())
    {
        lines.push_back(std::format("  warning {} {}", exportLabel(w.priority()), w.toString()));
    }
    for (std::size_t b = 0; b < data.getBranchCount(); b++)
    {
        addBranchLines(lines, data, b, startTime);
    }
}

}  // namespace

const std::vector<std::string>& extensionFlightScenarioIds()
{
    static const std::vector<std::string> kIds{
        "haisu-plain",
        "haisu-roll",
        "haisu-roll-airstart",
        "haisu-airstart-250-20",
        "haisu-roll-custom",
        "haisu-roll-nojitter",
        "haisu-plain-nojitter",
        "haisu-roll-missing",
        "alpha-plain",
        "alpha-roll",
        "beta-plain-boostercant",
        "beta-roll-sustainer",
        "beta-plain-sustainercant",
        "beta-roll-booster",
        "haisu-airstart-tilted",
        "haisu-plain-windy",
        "haisu-roll-airstart-windy",
    };
    return kIds;
}

ExtensionFlightScenario makeExtensionFlightScenario(std::string_view id)
{
    if (std::optional<ExtensionFlightScenario> scenario = makeReviewScenario(id))
    {
        return std::move(*scenario);
    }
    if (std::optional<ExtensionFlightScenario> scenario = makeHaisuScenario(id))
    {
        return std::move(*scenario);
    }
    if (std::optional<ExtensionFlightScenario> scenario = makeSmallRocketScenario(id))
    {
        return std::move(*scenario);
    }
    bug(std::format("the probe has no scenario '{}'", id));
}

std::vector<std::string> runExtensionFlightScenario(
    ExtensionFlightScenario& scenario, double timeStep,
    const std::shared_ptr<SimulationListener>& lastListener)
{
    Rocket&             rocket = *scenario.rocket;
    JavaTestPreferences preferences;
    Simulation          sim(rocket, preferences.store);
    sim.setFlightConfigurationId(scenario.fcid);
    setProbeOptions(sim.getOptions(), scenario, timeStep);
    for (const std::shared_ptr<SimulationExtension>& extension : scenario.extensions)
    {
        sim.getSimulationExtensions().push_back(extension);
    }

    const double cantBefore = cantOf(rocket, scenario.watchedFinSet);

    std::vector<std::string> lines;
    lines.push_back(std::format("SCENARIO {} dt={}", scenario.id, javaDouble(timeStep)));
    std::vector<std::string> names;
    for (const std::shared_ptr<SimulationExtension>& extension : sim.getSimulationExtensions())
    {
        names.push_back(extension->getName());
    }
    lines.push_back("  extensions=" + Strings::join("|", names));

    const std::shared_ptr<SpyHolder>                 holder = std::make_shared<SpyHolder>();
    std::vector<std::shared_ptr<SimulationListener>> listeners{
        std::make_shared<Spy>(holder, scenario.watchedFinSet)};
    if (lastListener != nullptr)
    {
        listeners.push_back(lastListener);
    }
    const RunResult run = scenario.jitterRemoved ? simulateWithoutJitter(sim, listeners)
                                                 : simulateDirectly(sim, listeners);
    if (run.jitterReplacements.has_value())
    {
        lines.push_back(std::format("  jitterReplacements={}", *run.jitterReplacements));
    }
    lines.push_back("  result=" + run.result);
    lines.push_back(std::format("  caller cantBefore={} cantAfter={}", javaDouble(cantBefore),
                                javaDouble(cantOf(rocket, scenario.watchedFinSet))));
    lines.push_back(
        std::format("  spy steps={} cantStart={} cantLast={} cantMaxAbs={} cantEnd={} ends={}",
                    holder->steps, javaDouble(holder->cantStart), javaDouble(holder->cantLast),
                    javaDouble(holder->cantMaxAbs), javaDouble(holder->cantEnd), holder->ends));
    if (run.data == nullptr)
    {
        lines.emplace_back("  nodata");
        return lines;
    }
    addDataLines(lines, *run.data, scenario.startTime);
    return lines;
}

std::vector<std::string_view> probeTokens(std::string_view line)
{
    std::vector<std::string_view> tokens;
    std::size_t                   position = 0;
    while (position < line.size())
    {
        const std::size_t start = line.find_first_not_of(' ', position);
        if (start == std::string_view::npos)
        {
            break;
        }
        std::size_t end = line.find(' ', start);
        if (end == std::string_view::npos)
        {
            end = line.size();
        }
        tokens.push_back(line.substr(start, end - start));
        position = end;
    }
    return tokens;
}

std::string probeLineKind(const std::vector<std::string_view>& tokens)
{
    if (tokens.empty())
    {
        return "";
    }
    if ((tokens[0] == "event" || tokens[0] == "column") && tokens.size() > 1)
    {
        return std::format("{} {}", tokens[0], tokens[1]);
    }
    return std::string(tokens[0].substr(0, tokens[0].find('=')));
}

}  // namespace QtRocket::Test
