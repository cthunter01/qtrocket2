#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AccelerationData;
using QtRocket::AerodynamicForces;
using QtRocket::AtmosphericConditions;
using QtRocket::CloneableSimulationListener;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfigurationId;
using QtRocket::MotorMount;
using QtRocket::Quaternion;
using QtRocket::RecoveryDevice;
using QtRocket::RigidBody;
using QtRocket::SimulationConditions;
using QtRocket::SimulationEventListener;
using QtRocket::SimulationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationListenerHelper;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::bugText;
using QtRocket::Test::newBranch;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

// OpenRocket has no test of SimulationListenerHelper. The expectations are what
// probes/tier8b-status/HelperProbe.java printed for the same scripted listeners: which
// listeners were called, in which order and with which value, what the helper returned, and how
// many warnings and SIM_WARN events the status had afterwards.

using Log = std::shared_ptr<std::vector<std::string>>;

/// A double as the probe prints it: Java's Double.toString(), and "kNaN".
[[nodiscard]] std::string text(double value)
{
    if (std::isnan(value))
    {
        return "kNaN";
    }
    return QtRocket::Strings::javaDoubleToString(value);
}

/// A boolean as Java prints it.
[[nodiscard]] std::string boolText(bool value)
{
    return std::format("{}", value);
}

// The values the scripted listeners return, each made from one number (HelperProbe's
// atmosphere(), flight(), forces(), mass() and acceleration()).

[[nodiscard]] AtmosphericConditions atmosphere(double v)
{
    return {v, 101325};
}

[[nodiscard]] FlightConditions flight(double v)
{
    FlightConditions c;
    c.setMach(v);
    return c;
}

[[nodiscard]] AerodynamicForces forces(double v)
{
    AerodynamicForces f = AerodynamicForces{}.zero();
    f.setCN(v);
    return f;
}

[[nodiscard]] RigidBody mass(double v)
{
    return {Coordinate{v, 0, 0, 1}, 1, 1, 1};
}

[[nodiscard]] AccelerationData acceleration(double v)
{
    return {Coordinate{v, 0, 0}, Coordinate{0, 0, 0}, std::nullopt, std::nullopt, Quaternion{}};
}

/// The value of a `pre` or `post` hook made from @p v, or nullopt for NaN ("no change").
template <class T>
[[nodiscard]] std::optional<T> valueOrNone(double v, T (*make)(double))
{
    if (std::isnan(v))
    {
        return std::nullopt;
    }
    return make(v);
}

/// HelperProbe.Script: a listener whose every hook is scripted. It logs "<name>.<hook>" (the
/// `post` hooks with the value they are given), changes the status when it is to `touch`, and
/// throws from the hook named by `throwIn`; the boolean hooks answer `!veto`, the others the
/// value made from `value`.
class Script final : public CloneableSimulationListener<Script>
{
public:
    Script(std::string name, bool system, Log log)
      : m_name(std::move(name)), m_system(system), m_log(std::move(log))
    {
    }

    // The script, set by the tests (public members, as the probe's fields).
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    bool        touch{false};
    bool        veto{false};
    double      value{std::numeric_limits<double>::quiet_NaN()};
    std::string throwIn;
    /// What startSimulation() does after it has logged, if anything.
    std::function<void(SimulationStatus&)> onStart;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    [[nodiscard]] bool isSystemListener() const override { return m_system; }

    void startSimulation(SimulationStatus& status) override
    {
        hook("startSimulation", status);
        if (onStart)
        {
            onStart(status);
        }
    }
    void endSimulation(SimulationStatus& status, const SimulationException* exception) override
    {
        quietHook("endSimulation(" + messageOf(exception) + ")", status);
    }
    void startSimulationBranch(SimulationStatus& status) override
    {
        hook("startSimulationBranch", status);
    }
    void endSimulationBranch(SimulationStatus&          status,
                             const SimulationException* exception) override
    {
        quietHook("endSimulationBranch(" + messageOf(exception) + ")", status);
    }
    [[nodiscard]] bool preStep(SimulationStatus& status) override
    {
        hook("preStep", status);
        return !veto;
    }
    void postStep(SimulationStatus& status) override { hook("postStep", status); }

    [[nodiscard]] bool addFlightEvent(SimulationStatus& status,
                                      const FlightEvent& /*event*/) override
    {
        hook("addFlightEvent", status);
        return !veto;
    }
    [[nodiscard]] bool handleFlightEvent(SimulationStatus& status,
                                         const FlightEvent& /*event*/) override
    {
        hook("handleFlightEvent", status);
        return !veto;
    }
    [[nodiscard]] bool motorIgnition(SimulationStatus& status,
                                     const MotorConfigurationId& /*motorId*/,
                                     const MotorMount& /*mount*/,
                                     MotorClusterState& /*instance*/) override
    {
        hook("motorIgnition", status);
        return !veto;
    }
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus& status,
                                                const RecoveryDevice& /*device*/) override
    {
        hook("recoveryDeviceDeployment", status);
        return !veto;
    }

    [[nodiscard]] std::optional<AtmosphericConditions> preAtmosphericModel(
        SimulationStatus& status) override
    {
        hook("preAtmosphericModel", status);
        return valueOrNone(value, atmosphere);
    }
    [[nodiscard]] std::optional<AtmosphericConditions> postAtmosphericModel(
        SimulationStatus& status, const AtmosphericConditions& given) override
    {
        hook("postAtmosphericModel(" + text(given.getTemperature()) + ")", status);
        return valueOrNone(value, atmosphere);
    }
    [[nodiscard]] std::optional<Coordinate> preWindModel(SimulationStatus& status) override
    {
        hook("preWindModel", status);
        return valueOrNone(value, wind);
    }
    [[nodiscard]] std::optional<Coordinate> postWindModel(SimulationStatus& status,
                                                          const Coordinate& given) override
    {
        hook("postWindModel(" + text(given.x) + ")", status);
        return valueOrNone(value, wind);
    }
    [[nodiscard]] double preGravityModel(SimulationStatus& status) override
    {
        hook("preGravityModel", status);
        return value;
    }
    [[nodiscard]] double postGravityModel(SimulationStatus& status, double gravity) override
    {
        hook("postGravityModel(" + text(gravity) + ")", status);
        return value;
    }
    [[nodiscard]] std::optional<FlightConditions> preFlightConditions(
        SimulationStatus& status) override
    {
        hook("preFlightConditions", status);
        return valueOrNone(value, flight);
    }
    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& status, const FlightConditions& given) override
    {
        hook("postFlightConditions(" + text(given.getMach()) + ")", status);
        return valueOrNone(value, flight);
    }
    [[nodiscard]] std::optional<AerodynamicForces> preAerodynamicCalculation(
        SimulationStatus& status) override
    {
        hook("preAerodynamicCalculation", status);
        return valueOrNone(value, forces);
    }
    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& status, const AerodynamicForces& given) override
    {
        hook("postAerodynamicCalculation(" + text(given.getCN()) + ")", status);
        return valueOrNone(value, forces);
    }
    [[nodiscard]] std::optional<RigidBody> preMassCalculation(SimulationStatus& status) override
    {
        hook("preMassCalculation", status);
        return valueOrNone(value, mass);
    }
    [[nodiscard]] std::optional<RigidBody> postMassCalculation(SimulationStatus& status,
                                                               const RigidBody&  given) override
    {
        hook("postMassCalculation(" + text(given.getCM().x) + ")", status);
        return valueOrNone(value, mass);
    }
    [[nodiscard]] double preSimpleThrustCalculation(SimulationStatus& status) override
    {
        hook("preSimpleThrustCalculation", status);
        return value;
    }
    [[nodiscard]] double postSimpleThrustCalculation(SimulationStatus& status,
                                                     double            thrust) override
    {
        hook("postSimpleThrustCalculation(" + text(thrust) + ")", status);
        return value;
    }
    [[nodiscard]] std::optional<AccelerationData> preAccelerationCalculation(
        SimulationStatus& status) override
    {
        hook("preAccelerationCalculation", status);
        return valueOrNone(value, acceleration);
    }
    [[nodiscard]] std::optional<AccelerationData> postAccelerationCalculation(
        SimulationStatus& status, const AccelerationData& given) override
    {
        hook("postAccelerationCalculation(" + text(given.getLinearAccelerationRC().x) + ")",
             status);
        return valueOrNone(value, acceleration);
    }

private:
    [[nodiscard]] static Coordinate wind(double v) { return Coordinate{v, 0, 0}; }

    [[nodiscard]] static std::string messageOf(const SimulationException* exception)
    {
        return exception == nullptr ? std::string{"null"} : std::string{exception->what()};
    }

    /// A hook that may throw.
    void hook(const std::string& hookText, SimulationStatus& status) const
    {
        quietHook(hookText, status);
        if (hookText == throwIn)
        {
            throw SimulationException(m_name + " failed in " + hookText);
        }
    }

    /// A hook that must not throw (the two end hooks).
    void quietHook(const std::string& hookText, SimulationStatus& status) const
    {
        m_log->push_back(m_name + "." + hookText);
        if (touch)
        {
            status.setSimulationTime(status.getSimulationTime());
        }
    }

    std::string m_name;
    bool        m_system;
    Log         m_log;
};

/// HelperProbe.Plain: a listener that implements SimulationListener only.
class Plain : public SimulationListener
{
public:
    explicit Plain(Log log) : m_log(std::move(log)) { }

    void startSimulation(SimulationStatus& /*status*/) override { log("P.startSimulation"); }
    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
        log("P.endSimulation");
    }
    void startSimulationBranch(SimulationStatus& /*status*/) override
    {
        log("P.startSimulationBranch");
    }
    void endSimulationBranch(SimulationStatus& /*status*/,
                             const SimulationException* /*exception*/) override
    {
        log("P.endSimulationBranch");
    }
    [[nodiscard]] bool preStep(SimulationStatus& /*status*/) override
    {
        log("P.preStep");
        return true;
    }
    void               postStep(SimulationStatus& /*status*/) override { log("P.postStep"); }
    [[nodiscard]] bool isSystemListener() const override { return false; }
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override
    {
        return std::make_shared<Plain>(m_log);
    }

protected:
    void log(const std::string& line) const { m_log->push_back(line); }

private:
    Log m_log;
};

/// HelperProbe.EventOnly: a listener that implements SimulationListener and
/// SimulationEventListener, but not SimulationComputationListener.
class EventOnly final : public Plain, public SimulationEventListener
{
public:
    using Plain::Plain;

    [[nodiscard]] bool addFlightEvent(SimulationStatus& /*status*/,
                                      const FlightEvent& /*event*/) override
    {
        log("E.addFlightEvent");
        return true;
    }
    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& /*event*/) override
    {
        log("E.handleFlightEvent");
        return true;
    }
    [[nodiscard]] bool motorIgnition(SimulationStatus& /*status*/,
                                     const MotorConfigurationId& /*motorId*/,
                                     const MotorMount& /*mount*/,
                                     MotorClusterState& /*instance*/) override
    {
        log("E.motorIgnition");
        return true;
    }
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus& /*status*/,
                                                const RecoveryDevice& /*device*/) override
    {
        log("E.recoveryDeviceDeployment");
        return true;
    }
};

/// HelperProbe.fresh(): a status of the Estes Alpha III with a flight data branch and the three
/// scripted listeners A, B and C in its conditions' list.
struct Fixture
{
    TestEstesAlphaIII                     alpha;
    Log                                   log = std::make_shared<std::vector<std::string>>();
    std::shared_ptr<Script>               a;
    std::shared_ptr<Script>               b;
    std::shared_ptr<Script>               c;
    std::shared_ptr<SimulationConditions> conditions = std::make_shared<SimulationConditions>();
    std::shared_ptr<FlightDataBranch>     branch     = newBranch("b");
    std::unique_ptr<SimulationStatus>     status;
    std::shared_ptr<MotorClusterState>    motorState;

    Fixture(bool aSystem, bool bSystem, bool cSystem)
      : a(std::make_shared<Script>("A", aSystem, log)),
        b(std::make_shared<Script>("B", bSystem, log)),
        c(std::make_shared<Script>("C", cSystem, log)),
        status(std::make_unique<SimulationStatus>(statusConfiguration(*alpha.rocket, testFcid(0)),
                                                  conditions)),
        motorState(status->getMotors().front())
    {
        conditions->getSimulationListenerList().push_back(a);
        conditions->getSimulationListenerList().push_back(b);
        conditions->getSimulationListenerList().push_back(c);
        status->setFlightDataBranch(branch);
    }

    /// The log, the result and the state of the status afterwards, as the probe prints them.
    [[nodiscard]] std::string summary(const std::string& result) const
    {
        std::string line;
        for (const std::string& entry : *log)
        {
            if (!line.empty())
            {
                line += ' ';
            }
            line += entry;
        }
        return line + " => " + result +
               " warnings=" + std::to_string(status->getWarnings()->size()) +
               " events=" + std::to_string(branch->getEvents().size());
    }
};

/// The text of a `pre` hook's result: the number the value was made from, or "null".
template <class T, class Number>
[[nodiscard]] std::string textOf(const std::optional<T>& value, Number number)
{
    if (!value.has_value())
    {
        return "null";
    }
    return text(number(*value));
}

/// One hook of the helper, fired on a fixture; the result as text (HelperProbe.fire()).
struct Hook
{
    using Fire = std::string (*)(const Fixture&);

    constexpr Hook(std::string_view hookName, Fire hookFire) noexcept
      : name(hookName), fire(hookFire)
    {
    }

    std::string_view name;
    Fire             fire;
};

[[nodiscard]] std::span<const Hook> hooks()
{
    using Helper = SimulationListenerHelper;
    static const std::array<Hook, 26> kHooks{{
        {"startSimulation",
         [](const Fixture& f) {
             Helper::fireStartSimulation(*f.status);
             return std::string{"void"};
         }},
        {"endSimulation",
         [](const Fixture& f) {
             const SimulationException cause("the cause");
             Helper::fireEndSimulation(*f.status, &cause);
             return std::string{"void"};
         }},
        {"startSimulationBranch",
         [](const Fixture& f) {
             Helper::fireStartSimulationBranch(*f.status);
             return std::string{"void"};
         }},
        {"endSimulationBranch",
         [](const Fixture& f) {
             Helper::fireEndSimulationBranch(*f.status, nullptr);
             return std::string{"void"};
         }},
        {"preStep", [](const Fixture& f) { return boolText(Helper::firePreStep(*f.status)); }},
        {"postStep",
         [](const Fixture& f) {
             Helper::firePostStep(*f.status);
             return std::string{"void"};
         }},
        {"addFlightEvent",
         [](const Fixture& f) {
             const FlightEvent event(FlightEvent::Type::APOGEE, 1.0);
             return boolText(Helper::fireAddFlightEvent(*f.status, event));
         }},
        {"handleFlightEvent",
         [](const Fixture& f) {
             const FlightEvent event(FlightEvent::Type::APOGEE, 1.0);
             return boolText(Helper::fireHandleFlightEvent(*f.status, event));
         }},
        {"motorIgnition",
         [](const Fixture& f) {
             return boolText(Helper::fireMotorIgnition(*f.status, f.motorState->getId(),
                                                       f.motorState->getMount(), *f.motorState));
         }},
        {"recoveryDeviceDeployment",
         [](const Fixture& f) {
             return boolText(Helper::fireRecoveryDeviceDeployment(*f.status, *f.alpha.chute));
         }},
        {"preAtmosphericModel",
         [](const Fixture& f) {
             return textOf(Helper::firePreAtmosphericModel(*f.status),
                           [](const AtmosphericConditions& r) { return r.getTemperature(); });
         }},
        {"postAtmosphericModel",
         [](const Fixture& f) {
             return text(
                 Helper::firePostAtmosphericModel(*f.status, atmosphere(250)).getTemperature());
         }},
        {"preWindModel",
         [](const Fixture& f) {
             return textOf(Helper::firePreWindModel(*f.status),
                           [](const Coordinate& r) { return r.x; });
         }},
        {"postWindModel",
         [](const Fixture& f) {
             return text(Helper::firePostWindModel(*f.status, Coordinate{250, 0, 0}).x);
         }},
        {"preGravityModel",
         [](const Fixture& f) { return text(Helper::firePreGravityModel(*f.status)); }},
        {"postGravityModel",
         [](const Fixture& f) { return text(Helper::firePostGravityModel(*f.status, 250)); }},
        {"preFlightConditions",
         [](const Fixture& f) {
             return textOf(Helper::firePreFlightConditions(*f.status),
                           [](const FlightConditions& r) { return r.getMach(); });
         }},
        {"postFlightConditions",
         [](const Fixture& f) {
             return text(Helper::firePostFlightConditions(*f.status, flight(250)).getMach());
         }},
        {"preAerodynamicCalculation",
         [](const Fixture& f) {
             return textOf(Helper::firePreAerodynamicCalculation(*f.status),
                           [](const AerodynamicForces& r) { return r.getCN(); });
         }},
        {"postAerodynamicCalculation",
         [](const Fixture& f) {
             return text(Helper::firePostAerodynamicCalculation(*f.status, forces(250)).getCN());
         }},
        {"preMassCalculation",
         [](const Fixture& f) {
             return textOf(Helper::firePreMassCalculation(*f.status),
                           [](const RigidBody& r) { return r.getCM().x; });
         }},
        {"postMassCalculation",
         [](const Fixture& f) {
             return text(Helper::firePostMassCalculation(*f.status, mass(250)).getCM().x);
         }},
        {"preSimpleThrustCalculation",
         [](const Fixture& f) { return text(Helper::firePreThrustCalculation(*f.status)); }},
        {"postSimpleThrustCalculation",
         [](const Fixture& f) { return text(Helper::firePostThrustCalculation(*f.status, 250)); }},
        {"preAccelerationCalculation",
         [](const Fixture& f) {
             return textOf(Helper::firePreAccelerationCalculation(*f.status),
                           [](const AccelerationData& r) { return r.getLinearAccelerationRC().x; });
         }},
        {"postAccelerationCalculation",
         [](const Fixture& f) {
             return text(Helper::firePostAccelerationCalculation(*f.status, acceleration(250))
                             .getLinearAccelerationRC()
                             .x);
         }},
    }};
    return kHooks;
}

/// Fires @p hookName on @p f and gives the probe's line: the log, then what came back (the
/// result, or the exception), then the warnings and events of the status (HelperProbe.run()).
[[nodiscard]] std::string run(const Fixture& f, std::string_view hookName)
{
    for (const Hook& hook : hooks())
    {
        if (hook.name != hookName)
        {
            continue;
        }
        std::string result;
        try
        {
            result = hook.fire(f);
        }
        catch (const SimulationException& e)
        {
            result = std::string{"SimulationException("} + e.what() + ")";
        }
        return f.summary(result);
    }
    return "no such hook";
}

/// A scenario of the probe: the listeners of a fresh fixture, scripted for @p hook.
struct Scenario
{
    using Script = void (*)(const Fixture&, std::string_view hook);

    constexpr Scenario(std::string_view scenarioName, bool systemA, bool systemB, bool systemC,
                       Script scenarioScript) noexcept
      : name(scenarioName),
        aSystem(systemA),
        bSystem(systemB),
        cSystem(systemC),
        script(scenarioScript)
    {
    }

    std::string_view name;
    bool             aSystem;
    bool             bSystem;
    bool             cSystem;
    Script           script;
};

/// HelperProbe's "throwing" scenario names the hook B throws from by the text it logs, so of
/// the `post` hooks only postStep() and postAtmosphericModel() throw.
void scriptThrowing(const Fixture& f, std::string_view hook)
{
    if (hook == "postAtmosphericModel")
    {
        f.b->throwIn = "postAtmosphericModel(250.0)";
    }
    else if (!hook.starts_with("post") || hook == "postStep")
    {
        f.b->throwIn = std::string{hook};
    }
}

/// HelperProbe's "kinds" scenario: a plain listener, an event listener and a full one.
void scriptKinds(const Fixture& f, std::string_view /*hook*/)
{
    std::vector<std::shared_ptr<SimulationListener>>& list =
        f.conditions->getSimulationListenerList();
    list.clear();
    list.push_back(std::make_shared<Plain>(f.log));
    list.push_back(std::make_shared<EventOnly>(f.log));
    list.push_back(f.a);
}

[[nodiscard]] std::span<const Scenario> scenarios()
{
    static const std::array<Scenario, 14> kScenarios{{
        // A user, B system, C user; nobody acts
        {"passive", false, true, false, [](const Fixture& /*f*/, std::string_view /*hook*/) { }},
        // B changes the status in every hook
        {"touchUser", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->touch = true; }},
        {"touchSystem", false, true, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->touch = true; }},
        // B answers false
        {"vetoUser", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->veto = true; }},
        {"vetoSystem", false, true, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->veto = true; }},
        // B returns the value 7
        {"valueUser", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->value = 7; }},
        {"valueSystem", false, true, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->value = 7; }},
        // B returns the value 250, which the post hooks are given
        {"sameValue", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->value = 250; }},
        // B returns a value within the tolerance of 250
        {"nearValue", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) { f.b->value = 250.000000001; }},
        // A returns 3, B returns nothing, C returns 5 (system listeners: no warning)
        {"chain", true, true, true,
         [](const Fixture& f, std::string_view /*hook*/) {
             f.a->value = 3;
             f.c->value = 5;
         }},
        {"throwing", false, false, false, scriptThrowing},
        // B changes the status and answers false (or returns 7)
        {"touchAndVeto", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) {
             f.b->touch = true;
             f.b->veto  = true;
             f.b->value = 7;
         }},
        {"kinds", false, false, false, scriptKinds},
        {"none", false, false, false,
         [](const Fixture& f, std::string_view /*hook*/) {
             f.conditions->getSimulationListenerList().clear();
         }},
    }};
    return kScenarios;
}

/// What the probe printed for one scenario and one hook.
struct Pin
{
    constexpr Pin(std::string_view pinScenario, std::string_view pinHook,
                  std::string_view pinExpected) noexcept
      : scenario(pinScenario), hook(pinHook), expected(pinExpected)
    {
    }

    std::string_view scenario;
    std::string_view hook;
    std::string_view expected;
};

// clang-format off
constexpr std::array<Pin, 341> kPins{{
    {"passive", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"passive", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"passive", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"passive", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"passive", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"passive", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"passive", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"passive", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"passive", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"passive", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"passive", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel C.preAtmosphericModel => null warnings=0 events=0"},
    {"passive", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preWindModel", "A.preWindModel B.preWindModel C.preWindModel => null warnings=0 events=0"},
    {"passive", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preGravityModel", "A.preGravityModel B.preGravityModel C.preGravityModel => kNaN warnings=0 events=0"},
    {"passive", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preFlightConditions", "A.preFlightConditions B.preFlightConditions C.preFlightConditions => null warnings=0 events=0"},
    {"passive", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation C.preAerodynamicCalculation => null warnings=0 events=0"},
    {"passive", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preMassCalculation", "A.preMassCalculation B.preMassCalculation C.preMassCalculation => null warnings=0 events=0"},
    {"passive", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation C.preSimpleThrustCalculation => kNaN warnings=0 events=0"},
    {"passive", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"passive", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation C.preAccelerationCalculation => null warnings=0 events=0"},
    {"passive", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"touchUser", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=1 events=1"},
    {"touchUser", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=1 events=1"},
    {"touchUser", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=1 events=1"},
    {"touchUser", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=1 events=1"},
    {"touchUser", "preStep", "A.preStep B.preStep C.preStep => true warnings=1 events=1"},
    {"touchUser", "postStep", "A.postStep B.postStep C.postStep => void warnings=1 events=1"},
    {"touchUser", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=1 events=1"},
    {"touchUser", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=1 events=1"},
    {"touchUser", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=1 events=1"},
    {"touchUser", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=1 events=1"},
    {"touchUser", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel C.preAtmosphericModel => null warnings=1 events=1"},
    {"touchUser", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preWindModel", "A.preWindModel B.preWindModel C.preWindModel => null warnings=1 events=1"},
    {"touchUser", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preGravityModel", "A.preGravityModel B.preGravityModel C.preGravityModel => kNaN warnings=1 events=1"},
    {"touchUser", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preFlightConditions", "A.preFlightConditions B.preFlightConditions C.preFlightConditions => null warnings=1 events=1"},
    {"touchUser", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation C.preAerodynamicCalculation => null warnings=1 events=1"},
    {"touchUser", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preMassCalculation", "A.preMassCalculation B.preMassCalculation C.preMassCalculation => null warnings=1 events=1"},
    {"touchUser", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation C.preSimpleThrustCalculation => kNaN warnings=1 events=1"},
    {"touchUser", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=1 events=1"},
    {"touchUser", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation C.preAccelerationCalculation => null warnings=1 events=1"},
    {"touchUser", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=1 events=1"},
    {"touchSystem", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"touchSystem", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"touchSystem", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"touchSystem", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"touchSystem", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"touchSystem", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"touchSystem", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"touchSystem", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"touchSystem", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"touchSystem", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"touchSystem", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel C.preAtmosphericModel => null warnings=0 events=0"},
    {"touchSystem", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preWindModel", "A.preWindModel B.preWindModel C.preWindModel => null warnings=0 events=0"},
    {"touchSystem", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preGravityModel", "A.preGravityModel B.preGravityModel C.preGravityModel => kNaN warnings=0 events=0"},
    {"touchSystem", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preFlightConditions", "A.preFlightConditions B.preFlightConditions C.preFlightConditions => null warnings=0 events=0"},
    {"touchSystem", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation C.preAerodynamicCalculation => null warnings=0 events=0"},
    {"touchSystem", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preMassCalculation", "A.preMassCalculation B.preMassCalculation C.preMassCalculation => null warnings=0 events=0"},
    {"touchSystem", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation C.preSimpleThrustCalculation => kNaN warnings=0 events=0"},
    {"touchSystem", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"touchSystem", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation C.preAccelerationCalculation => null warnings=0 events=0"},
    {"touchSystem", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"vetoUser", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"vetoUser", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"vetoUser", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"vetoUser", "preStep", "A.preStep B.preStep => false warnings=1 events=1"},
    {"vetoUser", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"vetoUser", "addFlightEvent", "A.addFlightEvent B.addFlightEvent => false warnings=1 events=1"},
    {"vetoUser", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent => false warnings=1 events=1"},
    {"vetoUser", "motorIgnition", "A.motorIgnition B.motorIgnition => false warnings=1 events=1"},
    {"vetoUser", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment => false warnings=1 events=1"},
    {"vetoUser", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel C.preAtmosphericModel => null warnings=0 events=0"},
    {"vetoUser", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preWindModel", "A.preWindModel B.preWindModel C.preWindModel => null warnings=0 events=0"},
    {"vetoUser", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preGravityModel", "A.preGravityModel B.preGravityModel C.preGravityModel => kNaN warnings=0 events=0"},
    {"vetoUser", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preFlightConditions", "A.preFlightConditions B.preFlightConditions C.preFlightConditions => null warnings=0 events=0"},
    {"vetoUser", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation C.preAerodynamicCalculation => null warnings=0 events=0"},
    {"vetoUser", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preMassCalculation", "A.preMassCalculation B.preMassCalculation C.preMassCalculation => null warnings=0 events=0"},
    {"vetoUser", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation C.preSimpleThrustCalculation => kNaN warnings=0 events=0"},
    {"vetoUser", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoUser", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation C.preAccelerationCalculation => null warnings=0 events=0"},
    {"vetoUser", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"vetoSystem", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"vetoSystem", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"vetoSystem", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"vetoSystem", "preStep", "A.preStep B.preStep => false warnings=0 events=0"},
    {"vetoSystem", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"vetoSystem", "addFlightEvent", "A.addFlightEvent B.addFlightEvent => false warnings=0 events=0"},
    {"vetoSystem", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent => false warnings=0 events=0"},
    {"vetoSystem", "motorIgnition", "A.motorIgnition B.motorIgnition => false warnings=0 events=0"},
    {"vetoSystem", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment => false warnings=0 events=0"},
    {"vetoSystem", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel C.preAtmosphericModel => null warnings=0 events=0"},
    {"vetoSystem", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preWindModel", "A.preWindModel B.preWindModel C.preWindModel => null warnings=0 events=0"},
    {"vetoSystem", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preGravityModel", "A.preGravityModel B.preGravityModel C.preGravityModel => kNaN warnings=0 events=0"},
    {"vetoSystem", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preFlightConditions", "A.preFlightConditions B.preFlightConditions C.preFlightConditions => null warnings=0 events=0"},
    {"vetoSystem", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation C.preAerodynamicCalculation => null warnings=0 events=0"},
    {"vetoSystem", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preMassCalculation", "A.preMassCalculation B.preMassCalculation C.preMassCalculation => null warnings=0 events=0"},
    {"vetoSystem", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation C.preSimpleThrustCalculation => kNaN warnings=0 events=0"},
    {"vetoSystem", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"vetoSystem", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation C.preAccelerationCalculation => null warnings=0 events=0"},
    {"vetoSystem", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"valueUser", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"valueUser", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"valueUser", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"valueUser", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"valueUser", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"valueUser", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"valueUser", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"valueUser", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"valueUser", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"valueUser", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"valueUser", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel => 7.0 warnings=1 events=1"},
    {"valueUser", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preWindModel", "A.preWindModel B.preWindModel => 7.0 warnings=1 events=1"},
    {"valueUser", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preGravityModel", "A.preGravityModel B.preGravityModel => 7.0 warnings=1 events=1"},
    {"valueUser", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preFlightConditions", "A.preFlightConditions B.preFlightConditions => 7.0 warnings=1 events=1"},
    {"valueUser", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation => 7.0 warnings=1 events=1"},
    {"valueUser", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preMassCalculation", "A.preMassCalculation B.preMassCalculation => 7.0 warnings=1 events=1"},
    {"valueUser", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation => 7.0 warnings=1 events=1"},
    {"valueUser", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(7.0) => 7.0 warnings=1 events=1"},
    {"valueUser", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation => 7.0 warnings=1 events=1"},
    {"valueUser", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(7.0) => 7.0 warnings=1 events=1"},
    {"valueSystem", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"valueSystem", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"valueSystem", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"valueSystem", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"valueSystem", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"valueSystem", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"valueSystem", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"valueSystem", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"valueSystem", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"valueSystem", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"valueSystem", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel => 7.0 warnings=0 events=0"},
    {"valueSystem", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preWindModel", "A.preWindModel B.preWindModel => 7.0 warnings=0 events=0"},
    {"valueSystem", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preGravityModel", "A.preGravityModel B.preGravityModel => 7.0 warnings=0 events=0"},
    {"valueSystem", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preFlightConditions", "A.preFlightConditions B.preFlightConditions => 7.0 warnings=0 events=0"},
    {"valueSystem", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation => 7.0 warnings=0 events=0"},
    {"valueSystem", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preMassCalculation", "A.preMassCalculation B.preMassCalculation => 7.0 warnings=0 events=0"},
    {"valueSystem", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation => 7.0 warnings=0 events=0"},
    {"valueSystem", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(7.0) => 7.0 warnings=0 events=0"},
    {"valueSystem", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation => 7.0 warnings=0 events=0"},
    {"valueSystem", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(7.0) => 7.0 warnings=0 events=0"},
    {"sameValue", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"sameValue", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"sameValue", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"sameValue", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"sameValue", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"sameValue", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"sameValue", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"sameValue", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"sameValue", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"sameValue", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"sameValue", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel => 250.0 warnings=1 events=1"},
    {"sameValue", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preWindModel", "A.preWindModel B.preWindModel => 250.0 warnings=1 events=1"},
    {"sameValue", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preGravityModel", "A.preGravityModel B.preGravityModel => 250.0 warnings=1 events=1"},
    {"sameValue", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preFlightConditions", "A.preFlightConditions B.preFlightConditions => 250.0 warnings=1 events=1"},
    {"sameValue", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation => 250.0 warnings=1 events=1"},
    {"sameValue", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=1 events=1"},
    {"sameValue", "preMassCalculation", "A.preMassCalculation B.preMassCalculation => 250.0 warnings=1 events=1"},
    {"sameValue", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation => 250.0 warnings=1 events=1"},
    {"sameValue", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"sameValue", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation => 250.0 warnings=1 events=1"},
    {"sameValue", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"nearValue", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"nearValue", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"nearValue", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"nearValue", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"nearValue", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"nearValue", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"nearValue", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"nearValue", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"nearValue", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"nearValue", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) C.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preWindModel", "A.preWindModel B.preWindModel => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preGravityModel", "A.preGravityModel B.preGravityModel => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preFlightConditions", "A.preFlightConditions B.preFlightConditions => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.000000001) => 250.000000001 warnings=1 events=1"},
    {"nearValue", "preMassCalculation", "A.preMassCalculation B.preMassCalculation => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"nearValue", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation => 250.000000001 warnings=1 events=1"},
    {"nearValue", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"chain", "startSimulation", "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0"},
    {"chain", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"chain", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch C.startSimulationBranch => void warnings=0 events=0"},
    {"chain", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"chain", "preStep", "A.preStep B.preStep C.preStep => true warnings=0 events=0"},
    {"chain", "postStep", "A.postStep B.postStep C.postStep => void warnings=0 events=0"},
    {"chain", "addFlightEvent", "A.addFlightEvent B.addFlightEvent C.addFlightEvent => true warnings=0 events=0"},
    {"chain", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent C.handleFlightEvent => true warnings=0 events=0"},
    {"chain", "motorIgnition", "A.motorIgnition B.motorIgnition C.motorIgnition => true warnings=0 events=0"},
    {"chain", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment C.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"chain", "preAtmosphericModel", "A.preAtmosphericModel => 3.0 warnings=0 events=0"},
    {"chain", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(3.0) C.postAtmosphericModel(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preWindModel", "A.preWindModel => 3.0 warnings=0 events=0"},
    {"chain", "postWindModel", "A.postWindModel(250.0) B.postWindModel(3.0) C.postWindModel(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preGravityModel", "A.preGravityModel => 3.0 warnings=0 events=0"},
    {"chain", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(3.0) C.postGravityModel(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preFlightConditions", "A.preFlightConditions => 3.0 warnings=0 events=0"},
    {"chain", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(3.0) C.postFlightConditions(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preAerodynamicCalculation", "A.preAerodynamicCalculation => 3.0 warnings=0 events=0"},
    {"chain", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(3.0) C.postAerodynamicCalculation(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preMassCalculation", "A.preMassCalculation => 3.0 warnings=0 events=0"},
    {"chain", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(3.0) C.postMassCalculation(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation => 3.0 warnings=0 events=0"},
    {"chain", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(3.0) C.postSimpleThrustCalculation(3.0) => 5.0 warnings=0 events=0"},
    {"chain", "preAccelerationCalculation", "A.preAccelerationCalculation => 3.0 warnings=0 events=0"},
    {"chain", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(3.0) C.postAccelerationCalculation(3.0) => 5.0 warnings=0 events=0"},
    {"throwing", "startSimulation", "A.startSimulation B.startSimulation => SimulationException(B failed in startSimulation) warnings=0 events=0"},
    {"throwing", "endSimulation", "A.endSimulation(the cause) B.endSimulation(the cause) C.endSimulation(the cause) => void warnings=0 events=0"},
    {"throwing", "startSimulationBranch", "A.startSimulationBranch B.startSimulationBranch => SimulationException(B failed in startSimulationBranch) warnings=0 events=0"},
    {"throwing", "endSimulationBranch", "A.endSimulationBranch(null) B.endSimulationBranch(null) C.endSimulationBranch(null) => void warnings=0 events=0"},
    {"throwing", "preStep", "A.preStep B.preStep => SimulationException(B failed in preStep) warnings=0 events=0"},
    {"throwing", "postStep", "A.postStep B.postStep => SimulationException(B failed in postStep) warnings=0 events=0"},
    {"throwing", "addFlightEvent", "A.addFlightEvent B.addFlightEvent => SimulationException(B failed in addFlightEvent) warnings=0 events=0"},
    {"throwing", "handleFlightEvent", "A.handleFlightEvent B.handleFlightEvent => SimulationException(B failed in handleFlightEvent) warnings=0 events=0"},
    {"throwing", "motorIgnition", "A.motorIgnition B.motorIgnition => SimulationException(B failed in motorIgnition) warnings=0 events=0"},
    {"throwing", "recoveryDeviceDeployment", "A.recoveryDeviceDeployment B.recoveryDeviceDeployment => SimulationException(B failed in recoveryDeviceDeployment) warnings=0 events=0"},
    {"throwing", "preAtmosphericModel", "A.preAtmosphericModel B.preAtmosphericModel => SimulationException(B failed in preAtmosphericModel) warnings=0 events=0"},
    {"throwing", "postAtmosphericModel", "A.postAtmosphericModel(250.0) B.postAtmosphericModel(250.0) => SimulationException(B failed in postAtmosphericModel(250.0)) warnings=0 events=0"},
    {"throwing", "preWindModel", "A.preWindModel B.preWindModel => SimulationException(B failed in preWindModel) warnings=0 events=0"},
    {"throwing", "postWindModel", "A.postWindModel(250.0) B.postWindModel(250.0) C.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preGravityModel", "A.preGravityModel B.preGravityModel => SimulationException(B failed in preGravityModel) warnings=0 events=0"},
    {"throwing", "postGravityModel", "A.postGravityModel(250.0) B.postGravityModel(250.0) C.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preFlightConditions", "A.preFlightConditions B.preFlightConditions => SimulationException(B failed in preFlightConditions) warnings=0 events=0"},
    {"throwing", "postFlightConditions", "A.postFlightConditions(250.0) B.postFlightConditions(250.0) C.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preAerodynamicCalculation", "A.preAerodynamicCalculation B.preAerodynamicCalculation => SimulationException(B failed in preAerodynamicCalculation) warnings=0 events=0"},
    {"throwing", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) B.postAerodynamicCalculation(250.0) C.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preMassCalculation", "A.preMassCalculation B.preMassCalculation => SimulationException(B failed in preMassCalculation) warnings=0 events=0"},
    {"throwing", "postMassCalculation", "A.postMassCalculation(250.0) B.postMassCalculation(250.0) C.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation B.preSimpleThrustCalculation => SimulationException(B failed in preSimpleThrustCalculation) warnings=0 events=0"},
    {"throwing", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) B.postSimpleThrustCalculation(250.0) C.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"throwing", "preAccelerationCalculation", "A.preAccelerationCalculation B.preAccelerationCalculation => SimulationException(B failed in preAccelerationCalculation) warnings=0 events=0"},
    {"throwing", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) B.postAccelerationCalculation(250.0) C.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"touchAndVeto", "preStep", "A.preStep B.preStep => false warnings=1 events=1"},
    {"touchAndVeto", "addFlightEvent", "A.addFlightEvent B.addFlightEvent => false warnings=1 events=1"},
    {"touchAndVeto", "preGravityModel", "A.preGravityModel B.preGravityModel => 7.0 warnings=1 events=1"},
    {"kinds", "startSimulation", "P.startSimulation P.startSimulation A.startSimulation => void warnings=0 events=0"},
    {"kinds", "endSimulation", "P.endSimulation P.endSimulation A.endSimulation(the cause) => void warnings=0 events=0"},
    {"kinds", "startSimulationBranch", "P.startSimulationBranch P.startSimulationBranch A.startSimulationBranch => void warnings=0 events=0"},
    {"kinds", "endSimulationBranch", "P.endSimulationBranch P.endSimulationBranch A.endSimulationBranch(null) => void warnings=0 events=0"},
    {"kinds", "preStep", "P.preStep P.preStep A.preStep => true warnings=0 events=0"},
    {"kinds", "postStep", "P.postStep P.postStep A.postStep => void warnings=0 events=0"},
    {"kinds", "addFlightEvent", "E.addFlightEvent A.addFlightEvent => true warnings=0 events=0"},
    {"kinds", "handleFlightEvent", "E.handleFlightEvent A.handleFlightEvent => true warnings=0 events=0"},
    {"kinds", "motorIgnition", "E.motorIgnition A.motorIgnition => true warnings=0 events=0"},
    {"kinds", "recoveryDeviceDeployment", "E.recoveryDeviceDeployment A.recoveryDeviceDeployment => true warnings=0 events=0"},
    {"kinds", "preAtmosphericModel", "A.preAtmosphericModel => null warnings=0 events=0"},
    {"kinds", "postAtmosphericModel", "A.postAtmosphericModel(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preWindModel", "A.preWindModel => null warnings=0 events=0"},
    {"kinds", "postWindModel", "A.postWindModel(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preGravityModel", "A.preGravityModel => kNaN warnings=0 events=0"},
    {"kinds", "postGravityModel", "A.postGravityModel(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preFlightConditions", "A.preFlightConditions => null warnings=0 events=0"},
    {"kinds", "postFlightConditions", "A.postFlightConditions(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preAerodynamicCalculation", "A.preAerodynamicCalculation => null warnings=0 events=0"},
    {"kinds", "postAerodynamicCalculation", "A.postAerodynamicCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preMassCalculation", "A.preMassCalculation => null warnings=0 events=0"},
    {"kinds", "postMassCalculation", "A.postMassCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preSimpleThrustCalculation", "A.preSimpleThrustCalculation => kNaN warnings=0 events=0"},
    {"kinds", "postSimpleThrustCalculation", "A.postSimpleThrustCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"kinds", "preAccelerationCalculation", "A.preAccelerationCalculation => null warnings=0 events=0"},
    {"kinds", "postAccelerationCalculation", "A.postAccelerationCalculation(250.0) => 250.0 warnings=0 events=0"},
    {"none", "startSimulation", " => void warnings=0 events=0"},
    {"none", "endSimulation", " => void warnings=0 events=0"},
    {"none", "startSimulationBranch", " => void warnings=0 events=0"},
    {"none", "endSimulationBranch", " => void warnings=0 events=0"},
    {"none", "preStep", " => true warnings=0 events=0"},
    {"none", "postStep", " => void warnings=0 events=0"},
    {"none", "addFlightEvent", " => true warnings=0 events=0"},
    {"none", "handleFlightEvent", " => true warnings=0 events=0"},
    {"none", "motorIgnition", " => true warnings=0 events=0"},
    {"none", "recoveryDeviceDeployment", " => true warnings=0 events=0"},
    {"none", "preAtmosphericModel", " => null warnings=0 events=0"},
    {"none", "postAtmosphericModel", " => 250.0 warnings=0 events=0"},
    {"none", "preWindModel", " => null warnings=0 events=0"},
    {"none", "postWindModel", " => 250.0 warnings=0 events=0"},
    {"none", "preGravityModel", " => kNaN warnings=0 events=0"},
    {"none", "postGravityModel", " => 250.0 warnings=0 events=0"},
    {"none", "preFlightConditions", " => null warnings=0 events=0"},
    {"none", "postFlightConditions", " => 250.0 warnings=0 events=0"},
    {"none", "preAerodynamicCalculation", " => null warnings=0 events=0"},
    {"none", "postAerodynamicCalculation", " => 250.0 warnings=0 events=0"},
    {"none", "preMassCalculation", " => null warnings=0 events=0"},
    {"none", "postMassCalculation", " => 250.0 warnings=0 events=0"},
    {"none", "preSimpleThrustCalculation", " => kNaN warnings=0 events=0"},
    {"none", "postSimpleThrustCalculation", " => 250.0 warnings=0 events=0"},
    {"none", "preAccelerationCalculation", " => null warnings=0 events=0"},
    {"none", "postAccelerationCalculation", " => 250.0 warnings=0 events=0"},
}};
// clang-format on

/// The line of @p pin as this port produces it.
[[nodiscard]] std::string lineOf(const Pin& pin)
{
    for (const Scenario& scenario : scenarios())
    {
        if (scenario.name != pin.scenario)
        {
            continue;
        }
        const Fixture f(scenario.aSystem, scenario.bSystem, scenario.cSystem);
        scenario.script(f, pin.hook);
        return run(f, pin.hook);
    }
    return "no such scenario";
}

/// The pins of @p scenario whose line differs from Java's, one per line.
[[nodiscard]] std::string mismatches(std::string_view scenario)
{
    std::string report;
    int         compared = 0;
    for (const Pin& pin : kPins)
    {
        if (pin.scenario != scenario)
        {
            continue;
        }
        compared++;
        const std::string actual = lineOf(pin);
        if (actual != pin.expected)
        {
            report += std::string{pin.hook} + ":\n  expected " + std::string{pin.expected} +
                      "\n  actual   " + actual + "\n";
        }
    }
    if (compared == 0)
    {
        report += "no pins for the scenario\n";
    }
    return report;
}

// ============================================================================ Java's lines

TEST(SimulationListenerHelperAgainstJava, PassiveListenersAreAllCalledInOrderAndChangeNothing)
{
    EXPECT_EQ(mismatches("passive"), "");
}

TEST(SimulationListenerHelperAgainstJava, AUserListenerThatChangesTheStatusIsWarnedAbout)
{
    EXPECT_EQ(mismatches("touchUser"), "");
}

TEST(SimulationListenerHelperAgainstJava, ASystemListenerThatChangesTheStatusIsNot)
{
    EXPECT_EQ(mismatches("touchSystem"), "");
}

TEST(SimulationListenerHelperAgainstJava, AVetoStopsTheListenersAfterItAndWarns)
{
    EXPECT_EQ(mismatches("vetoUser"), "");
    EXPECT_EQ(mismatches("vetoSystem"), "");
}

TEST(SimulationListenerHelperAgainstJava, AValueStopsThePreHooksAndReplacesInThePostHooks)
{
    EXPECT_EQ(mismatches("valueUser"), "");
    EXPECT_EQ(mismatches("valueSystem"), "");
}

TEST(SimulationListenerHelperAgainstJava, APostValueThatEqualsTheCurrentOneChangesNothing)
{
    // Of the `post` hooks only the aerodynamic forces differ for the same number: forces made
    // with zero() keep NaN drag components, and a NaN equals nothing (as in Java).
    EXPECT_EQ(mismatches("sameValue"), "");
    EXPECT_EQ(mismatches("nearValue"), "");
}

TEST(SimulationListenerHelperAgainstJava, EachPostHookSeesWhatTheListenersBeforeItLeft)
{
    EXPECT_EQ(mismatches("chain"), "");
}

TEST(SimulationListenerHelperAgainstJava, AnExceptionLeavesAtOnce)
{
    EXPECT_EQ(mismatches("throwing"), "");
}

TEST(SimulationListenerHelperAgainstJava, AListenerThatChangesTheStatusAndVetoesIsWarnedAboutOnce)
{
    EXPECT_EQ(mismatches("touchAndVeto"), "");
}

TEST(SimulationListenerHelperAgainstJava, OnlyTheListenersOfTheRightKindAreAsked)
{
    EXPECT_EQ(mismatches("kinds"), "");
}

TEST(SimulationListenerHelperAgainstJava, WithoutListenersEveryHookLetsTheSimulationGoOn)
{
    EXPECT_EQ(mismatches("none"), "");
}

TEST(SimulationListenerHelperAgainstJava, EveryHookOfEveryScenarioIsPinned)
{
    // 26 hooks in thirteen scenarios, and three hooks of "touchAndVeto".
    EXPECT_EQ(hooks().size(), 26U);
    EXPECT_EQ(scenarios().size(), 14U);
    EXPECT_EQ(kPins.size(), (13U * 26U) + 3U);
}

// ============================================================================ the listener list
//
// HelperProbe, "the listener list".

TEST(SimulationListenerHelper, TheListenersAreThoseOfTheConditionsTheStatusHadWhenTheCallBegan)
{
    // "replaced conditions: A.startSimulation B.startSimulation C.startSimulation => void
    // warnings=0 events=0", "next call: X.startSimulation => void warnings=0 events=0"
    const Fixture f(false, false, false);
    const auto    other = std::make_shared<SimulationConditions>();
    other->getSimulationListenerList().push_back(std::make_shared<Script>("X", false, f.log));
    f.a->onStart = [other](SimulationStatus& s) { s.setSimulationConditions(other); };
    EXPECT_EQ(run(f, "startSimulation"),
              "A.startSimulation B.startSimulation C.startSimulation => void warnings=0 events=0");
    f.a->onStart = nullptr;
    f.log->clear();
    EXPECT_EQ(run(f, "startSimulation"), "X.startSimulation => void warnings=0 events=0");
}

TEST(SimulationListenerHelper, TheConditionsAreKeptAliveWhileTheirListenersAreCalled)
{
    // The status is the only owner of its conditions, and the first listener gives it others:
    // the helper keeps the ones the call began with, and their list, until the call is over.
    Fixture f(false, false, false);
    f.a->onStart = [](SimulationStatus& s) {
        s.setSimulationConditions(std::make_shared<SimulationConditions>());
    };
    const std::weak_ptr<SimulationConditions> watched = f.conditions;
    f.conditions.reset();
    ASSERT_FALSE(watched.expired());
    SimulationListenerHelper::fireStartSimulation(*f.status);
    EXPECT_EQ(*f.log, (std::vector<std::string>{"A.startSimulation", "B.startSimulation",
                                                "C.startSimulation"}));
    EXPECT_TRUE(watched.expired()) << "released when the call was over";
}

/// The line of startSimulation() on a fixture whose listener @p who runs @p action when it is
/// called; a BugError's text in place of the result.
[[nodiscard]] std::string runWithAction(const Fixture& f, Script& who,
                                        std::function<void(SimulationStatus&)> action)
{
    who.onStart = std::move(action);
    std::string       result;
    const std::string bug = bugText([&] { result = run(f, "startSimulation"); });
    if (bug != "<none>")
    {
        return f.summary("BugError(" + bug + ")");
    }
    return result;
}

/// An action that adds a listener X to the list of the status's conditions.
[[nodiscard]] std::function<void(SimulationStatus&)> addX(const Log& log)
{
    return [log](SimulationStatus& s) {
        s.getSimulationConditions()->getSimulationListenerList().push_back(
            std::make_shared<Script>("X", false, log));
    };
}

/// An action that removes the listener at @p index from the list of the status's conditions.
[[nodiscard]] std::function<void(SimulationStatus&)> removeAt(std::size_t index)
{
    return [index](SimulationStatus& s) {
        std::vector<std::shared_ptr<SimulationListener>>& list =
            s.getSimulationConditions()->getSimulationListenerList();
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
    };
}

/// An action that puts a listener X in the place of the listener at @p index.
[[nodiscard]] std::function<void(SimulationStatus&)> replaceByX(std::size_t index, const Log& log)
{
    return [index, log](SimulationStatus& s) {
        s.getSimulationConditions()->getSimulationListenerList()[index] =
            std::make_shared<Script>("X", false, log);
    };
}

/// What a change of the list ends in (Java: ConcurrentModificationException).
constexpr std::string_view kListChanged =
    "BugError(A simulation listener changed the listener list it was called from)";

/// The probe's line for the listeners called (@p called) before the change of the list was
/// noticed.
[[nodiscard]] std::string listChangedAfter(std::string_view called)
{
    return std::string{called} + " => " + std::string{kListChanged} + " warnings=0 events=0";
}

TEST(SimulationListenerHelper, AListenerThatLengthensOrShortensItsListIsABug)
{
    // "A adds X: A.startSimulation => ConcurrentModificationException warnings=0 events=0"
    const Fixture adds(false, false, false);
    EXPECT_EQ(runWithAction(adds, *adds.a, addX(adds.log)), listChangedAfter("A.startSimulation"));

    // "A removes C: A.startSimulation => ConcurrentModificationException ..."
    const Fixture removes(false, false, false);
    EXPECT_EQ(runWithAction(removes, *removes.a, removeAt(2)),
              listChangedAfter("A.startSimulation"));

    // "C adds X: A.startSimulation B.startSimulation C.startSimulation =>
    // ConcurrentModificationException ..."
    const Fixture lastAdds(false, false, false);
    EXPECT_EQ(runWithAction(lastAdds, *lastAdds.c, addX(lastAdds.log)),
              listChangedAfter("A.startSimulation B.startSimulation C.startSimulation"));

    // "C removes A: A.startSimulation B.startSimulation C.startSimulation =>
    // ConcurrentModificationException ..."
    const Fixture lastRemoves(false, false, false);
    EXPECT_EQ(runWithAction(lastRemoves, *lastRemoves.c, removeAt(0)),
              listChangedAfter("A.startSimulation B.startSimulation C.startSimulation"));
}

TEST(SimulationListenerHelper, TheTwoChangesOfTheListThatJavaDoesNotNotice)
{
    // "B removes C: A.startSimulation B.startSimulation => void warnings=0 events=0": the list
    // is then as long as the number of listeners called, and Java's loop ends quietly.
    const Fixture removes(false, false, false);
    EXPECT_EQ(runWithAction(removes, *removes.b, removeAt(2)),
              "A.startSimulation B.startSimulation => void warnings=0 events=0");

    // "A replaces C by X: A.startSimulation B.startSimulation X.startSimulation => void
    // warnings=0 events=0": a replacement keeps the length, and the list is read as it is.
    const Fixture replaces(false, false, false);
    EXPECT_EQ(runWithAction(replaces, *replaces.a, replaceByX(2, replaces.log)),
              "A.startSimulation B.startSimulation X.startSimulation => void warnings=0 events=0");
}

TEST(SimulationListenerHelper, AStatusWithoutConditionsOrWithANullListenerIsABug)
{
    // "no conditions:  => NullPointerException warnings=0 events=0"
    Fixture f(false, false, false);
    f.status->setSimulationConditions(nullptr);
    EXPECT_EQ(bugText([&] { SimulationListenerHelper::fireStartSimulation(*f.status); }),
              "The simulation status has no simulation conditions");
    EXPECT_TRUE(f.log->empty());

    // "null listener: A.startSimulation => NullPointerException warnings=0 events=0"
    Fixture g(false, false, false);
    g.conditions->getSimulationListenerList()[1] = nullptr;
    EXPECT_EQ(bugText([&] { static_cast<void>(SimulationListenerHelper::firePreStep(*g.status)); }),
              "The simulation listener list holds a null listener");
    EXPECT_EQ(*g.log, (std::vector<std::string>{"A.preStep"}));
}

TEST(SimulationListenerHelper, TheWarningNeedsAFlightDataBranch)
{
    // "warning without a branch: A.startSimulation B.startSimulation => NullPointerException
    // warnings=1": the warning is in the set by the time the missing branch is noticed.
    Fixture f(false, false, false);
    f.status->setFlightDataBranch(nullptr);
    f.b->touch = true;
    EXPECT_EQ(bugText([&] { SimulationListenerHelper::fireStartSimulation(*f.status); }),
              "A warning was added to a simulation status without a flight data branch");
    EXPECT_EQ(*f.log, (std::vector<std::string>{"A.startSimulation", "B.startSimulation"}));
    EXPECT_EQ(f.status->getWarnings()->size(), 1U);
}

TEST(SimulationListenerHelper, TheWarningIsListenersAffectedWithAnEventAtTheSimulationTime)
{
    // "the warning event: FlightEvent[type=SIM_WARN,time=2.5,source=null,data=Listeners
    // modified the flight simulation]"
    Fixture f(false, false, false);
    f.status->setSimulationTime(2.5);
    f.b->touch = true;
    SimulationListenerHelper::firePostStep(*f.status);
    ASSERT_EQ(f.branch->getEvents().size(), 1U);
    EXPECT_EQ(f.branch->getEvents().front().toString(),
              "FlightEvent[type=SIM_WARN,time=2.5,source=null,data=Listeners modified the flight "
              "simulation]");
    const WarningSet& warnings = *f.status->getWarnings();
    EXPECT_TRUE(warnings.contains(Warning::kListenersAffected));

    // A second listener that affects the simulation adds nothing: the set holds the warning.
    f.c->touch = true;
    SimulationListenerHelper::firePostStep(*f.status);
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_EQ(f.branch->getEvents().size(), 1U);
}

TEST(SimulationListenerHelper, TheEndHooksPassTheExceptionOrNull)
{
    Fixture                   f(true, true, true);
    const SimulationException cause("boom");
    SimulationListenerHelper::fireEndSimulation(*f.status, &cause);
    SimulationListenerHelper::fireEndSimulation(*f.status, nullptr);
    SimulationListenerHelper::fireEndSimulationBranch(*f.status, &cause);
    SimulationListenerHelper::fireEndSimulationBranch(*f.status, nullptr);
    EXPECT_EQ(*f.log, (std::vector<std::string>{
                          "A.endSimulation(boom)", "B.endSimulation(boom)", "C.endSimulation(boom)",
                          "A.endSimulation(null)", "B.endSimulation(null)", "C.endSimulation(null)",
                          "A.endSimulationBranch(boom)", "B.endSimulationBranch(boom)",
                          "C.endSimulationBranch(boom)", "A.endSimulationBranch(null)",
                          "B.endSimulationBranch(null)", "C.endSimulationBranch(null)"}));
}

/// Remembers what the event hooks were given, in a record its clones share.
class Recorder final : public CloneableSimulationListener<Recorder>
{
public:
    struct Seen
    {
        std::vector<FlightEvent>             events;
        const MotorConfigurationId*          motorId{nullptr};
        const MotorMount*                    mount{nullptr};
        const MotorClusterState*             instance{nullptr};
        const RecoveryDevice*                device{nullptr};
        std::vector<const SimulationStatus*> statuses;
    };

    [[nodiscard]] bool addFlightEvent(SimulationStatus& status, const FlightEvent& event) override
    {
        m_seen->statuses.push_back(&status);
        m_seen->events.push_back(event);
        return true;
    }
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override
    {
        m_seen->statuses.push_back(&status);
        m_seen->events.push_back(event);
        return true;
    }
    [[nodiscard]] bool motorIgnition(SimulationStatus& status, const MotorConfigurationId& motorId,
                                     const MotorMount& mount, MotorClusterState& instance) override
    {
        m_seen->statuses.push_back(&status);
        m_seen->motorId  = &motorId;
        m_seen->mount    = &mount;
        m_seen->instance = &instance;
        return true;
    }
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus&     status,
                                                const RecoveryDevice& device) override
    {
        m_seen->statuses.push_back(&status);
        m_seen->device = &device;
        return true;
    }

    [[nodiscard]] const std::shared_ptr<Seen>& seen() const noexcept { return m_seen; }

private:
    std::shared_ptr<Seen> m_seen{std::make_shared<Seen>()};
};

TEST(SimulationListenerHelper, TheHooksAreGivenTheArgumentsOfTheCall)
{
    Fixture    f(false, false, false);
    const auto recorder = std::make_shared<Recorder>();
    f.conditions->getSimulationListenerList().push_back(recorder);
    const FlightEvent           added(FlightEvent::Type::APOGEE, 1.0);
    const FlightEvent           handled(FlightEvent::Type::TUMBLE, 2.0);
    const MotorConfigurationId& motorId = f.motorState->getId();

    EXPECT_TRUE(SimulationListenerHelper::fireAddFlightEvent(*f.status, added));
    EXPECT_TRUE(SimulationListenerHelper::fireHandleFlightEvent(*f.status, handled));
    EXPECT_TRUE(SimulationListenerHelper::fireMotorIgnition(
        *f.status, motorId, f.motorState->getMount(), *f.motorState));
    EXPECT_TRUE(SimulationListenerHelper::fireRecoveryDeviceDeployment(*f.status, *f.alpha.chute));

    const Recorder::Seen& seen = *recorder->seen();
    ASSERT_EQ(seen.events.size(), 2U);
    EXPECT_TRUE(seen.events[0].sameEvent(added));
    EXPECT_TRUE(seen.events[1].sameEvent(handled));
    EXPECT_EQ(seen.motorId, &motorId);
    EXPECT_EQ(seen.mount, &f.motorState->getMount());
    EXPECT_EQ(seen.instance, f.motorState.get());
    EXPECT_EQ(seen.device, f.alpha.chute);
    EXPECT_EQ(seen.statuses, (std::vector<const SimulationStatus*>(4, &*f.status)));
}

TEST(SimulationListenerHelper, TheValueOfAPostHookComesBackWhole)
{
    // What the steppers rely on: the value a listener returns is what the helper returns, not
    // only the number the scripted listeners make it from.
    Fixture f(true, true, true);
    f.b->value = 0.7;

    const AtmosphericConditions air =
        SimulationListenerHelper::firePostAtmosphericModel(*f.status, atmosphere(250));
    EXPECT_EQ(air, atmosphere(0.7));
    const FlightConditions conditions =
        SimulationListenerHelper::firePostFlightConditions(*f.status, flight(0.3));
    EXPECT_EQ(conditions.getMach(), 0.7);
    const RigidBody body = SimulationListenerHelper::firePostMassCalculation(*f.status, mass(250));
    EXPECT_EQ(body, mass(0.7));
    const AccelerationData a =
        SimulationListenerHelper::firePostAccelerationCalculation(*f.status, acceleration(250));
    EXPECT_TRUE(a == acceleration(0.7));
    const Coordinate w =
        SimulationListenerHelper::firePostWindModel(*f.status, Coordinate{1, 2, 3});
    EXPECT_TRUE(w.exactlyEquals(Coordinate{0.7, 0, 0}));

    // Without a value the one given comes back untouched.
    f.b->value = std::numeric_limits<double>::quiet_NaN();
    const Coordinate same =
        SimulationListenerHelper::firePostWindModel(*f.status, Coordinate{1, 2, 3});
    EXPECT_TRUE(same.exactlyEquals(Coordinate{1, 2, 3}));
    EXPECT_EQ(SimulationListenerHelper::firePostGravityModel(*f.status, 9.81), 9.81);
    EXPECT_EQ(SimulationListenerHelper::firePostThrustCalculation(*f.status, 12.5), 12.5);
}

}  // namespace
