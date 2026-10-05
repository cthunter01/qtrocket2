#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

namespace
{

/// Java's warn(): a listener that is not a system listener affected the simulation.
void warn(SimulationStatus& status, const SimulationListener& listener)
{
    if (!listener.isSystemListener())
    {
        status.addWarning(Warning::kListenersAffected);
    }
}

/// The comparison of the status's modification id around each hook.
class ModIdWatch
{
public:
    explicit ModIdWatch(const SimulationStatus& status) noexcept : m_modId(status.getModId()) { }

    /// Warns about @p listener when the status changed since the last look.
    void check(SimulationStatus& status, const SimulationListener& listener)
    {
        if (m_modId != status.getModId())
        {
            warn(status, listener);
            m_modId = status.getModId();
        }
    }

private:
    ModId m_modId;
};

/// Java's `for (SimulationListener l : status.getSimulationConditions()
/// .getSimulationListenerList())`: calls @p call for the listeners of the status's conditions,
/// in order, until it returns false. The loop is that of ArrayList's iterator: it ends when the
/// index reaches the length of the list as it is then, and a list whose length has changed is a
/// BugError (Java: ConcurrentModificationException).
template <class Call>
void forEachListener(const SimulationStatus& status, Call call)
{
    // Held for the whole loop: a listener may give the status other conditions.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): the copy keeps them alive
    const std::shared_ptr<SimulationConditions> conditions = status.getSimulationConditions();
    if (conditions == nullptr)
    {
        bug("The simulation status has no simulation conditions");
    }
    const std::vector<std::shared_ptr<SimulationListener>>& listeners =
        conditions->getSimulationListenerList();
    const std::size_t expectedSize = listeners.size();
    for (std::size_t cursor = 0; cursor != listeners.size(); cursor++)
    {
        if (listeners.size() != expectedSize)
        {
            bug("A simulation listener changed the listener list it was called from");
        }
        // A copy, which keeps the listener alive while it is called.
        const std::shared_ptr<SimulationListener> listener = listeners[cursor];
        if (listener == nullptr)
        {
            bug("The simulation listener list holds a null listener");
        }
        if (!call(*listener))
        {
            return;
        }
    }
}

/// A hook of SimulationListener that every listener gets: @p hook(listener).
template <class Hook>
void fireListenerHook(SimulationStatus& status, Hook hook)
{
    ModIdWatch watch(status);
    forEachListener(status, [&status, &watch, &hook](SimulationListener& l) {
        hook(l);
        watch.check(status, l);
        return true;
    });
}

/// A hook of SimulationEventListener that may veto: @p hook(eventListener) for the listeners
/// that implement the interface, until one returns false.
template <class Hook>
[[nodiscard]] bool fireEventHook(SimulationStatus& status, Hook hook)
{
    ModIdWatch watch(status);
    bool       result = true;
    forEachListener(status, [&status, &watch, &hook, &result](SimulationListener& l) {
        auto* eventListener = dynamic_cast<SimulationEventListener*>(&l);
        if (eventListener == nullptr)
        {
            return true;
        }
        const bool b = hook(*eventListener);
        watch.check(status, l);
        if (!b)
        {
            warn(status, l);
            result = false;
            return false;
        }
        return true;
    });
    return result;
}

/// A `pre` hook of SimulationComputationListener: @p hook(computationListener) for the
/// listeners that implement the interface, until one returns a value.
template <class T, class Hook>
[[nodiscard]] std::optional<T> firePreHook(SimulationStatus& status, Hook hook)
{
    ModIdWatch       watch(status);
    std::optional<T> result;
    forEachListener(status, [&status, &watch, &hook, &result](SimulationListener& l) {
        auto* computationListener = dynamic_cast<SimulationComputationListener*>(&l);
        if (computationListener == nullptr)
        {
            return true;
        }
        std::optional<T> value = hook(*computationListener);
        watch.check(status, l);
        if (value.has_value())
        {
            warn(status, l);
            result = std::move(value);
            return false;
        }
        return true;
    });
    return result;
}

/// A `pre` hook that returns a double, NaN for "no change".
template <class Hook>
[[nodiscard]] double firePreNumberHook(SimulationStatus& status, Hook hook)
{
    ModIdWatch watch(status);
    double     result = std::numeric_limits<double>::quiet_NaN();
    forEachListener(status, [&status, &watch, &hook, &result](SimulationListener& l) {
        auto* computationListener = dynamic_cast<SimulationComputationListener*>(&l);
        if (computationListener == nullptr)
        {
            return true;
        }
        const double value = hook(*computationListener);
        watch.check(status, l);
        if (!std::isnan(value))
        {
            warn(status, l);
            result = value;
            return false;
        }
        return true;
    });
    return result;
}

/// A `post` hook of SimulationComputationListener: @p hook(computationListener, current) for
/// every listener that implements the interface; a value that differs from the current one
/// replaces it.
template <class T, class Hook>
[[nodiscard]] T firePostHook(SimulationStatus& status, T current, Hook hook)
{
    ModIdWatch watch(status);
    forEachListener(status, [&status, &watch, &hook, &current](SimulationListener& l) {
        auto* computationListener = dynamic_cast<SimulationComputationListener*>(&l);
        if (computationListener == nullptr)
        {
            return true;
        }
        std::optional<T> value = hook(*computationListener, std::as_const(current));
        watch.check(status, l);
        if (value.has_value() && !(*value == current))
        {
            warn(status, l);
            current = std::move(*value);
        }
        return true;
    });
    return current;
}

/// A `post` hook on a double: NaN is "no change", and so is a value within MathUtil::equals()
/// of the current one.
template <class Hook>
[[nodiscard]] double firePostNumberHook(SimulationStatus& status, double current, Hook hook)
{
    ModIdWatch watch(status);
    forEachListener(status, [&status, &watch, &hook, &current](SimulationListener& l) {
        auto* computationListener = dynamic_cast<SimulationComputationListener*>(&l);
        if (computationListener == nullptr)
        {
            return true;
        }
        const double value = hook(*computationListener, current);
        watch.check(status, l);
        if (!std::isnan(value) && !MathUtil::equals(value, current))
        {
            warn(status, l);
            current = value;
        }
        return true;
    });
    return current;
}

}  // namespace

//////// SimulationListener methods ////////

void SimulationListenerHelper::fireStartSimulation(SimulationStatus& status)
{
    fireListenerHook(status, [&status](SimulationListener& l) { l.startSimulation(status); });
}

void SimulationListenerHelper::fireEndSimulation(SimulationStatus&          status,
                                                 const SimulationException* exception)
{
    fireListenerHook(status, [&status, exception](SimulationListener& l) {
        l.endSimulation(status, exception);
    });
}

void SimulationListenerHelper::fireStartSimulationBranch(SimulationStatus& status)
{
    fireListenerHook(status, [&status](SimulationListener& l) { l.startSimulationBranch(status); });
}

void SimulationListenerHelper::fireEndSimulationBranch(SimulationStatus&          status,
                                                       const SimulationException* exception)
{
    fireListenerHook(status, [&status, exception](SimulationListener& l) {
        l.endSimulationBranch(status, exception);
    });
}

bool SimulationListenerHelper::firePreStep(SimulationStatus& status)
{
    ModIdWatch watch(status);
    bool       result = true;
    forEachListener(status, [&status, &watch, &result](SimulationListener& l) {
        const bool b = l.preStep(status);
        watch.check(status, l);
        if (!b)
        {
            warn(status, l);
            result = false;
            return false;
        }
        return true;
    });
    return result;
}

void SimulationListenerHelper::firePostStep(SimulationStatus& status)
{
    fireListenerHook(status, [&status](SimulationListener& l) { l.postStep(status); });
}

//////// SimulationEventListener methods ////////

bool SimulationListenerHelper::fireAddFlightEvent(SimulationStatus&  status,
                                                  const FlightEvent& event)
{
    return fireEventHook(status, [&status, &event](SimulationEventListener& l) {
        return l.addFlightEvent(status, event);
    });
}

bool SimulationListenerHelper::fireHandleFlightEvent(SimulationStatus&  status,
                                                     const FlightEvent& event)
{
    return fireEventHook(status, [&status, &event](SimulationEventListener& l) {
        return l.handleFlightEvent(status, event);
    });
}

bool SimulationListenerHelper::fireMotorIgnition(SimulationStatus&           status,
                                                 const MotorConfigurationId& motorId,
                                                 const MotorMount&           mount,
                                                 MotorClusterState&          instance)
{
    return fireEventHook(status,
                         [&status, &motorId, &mount, &instance](SimulationEventListener& l) {
                             return l.motorIgnition(status, motorId, mount, instance);
                         });
}

bool SimulationListenerHelper::fireRecoveryDeviceDeployment(SimulationStatus&     status,
                                                            const RecoveryDevice& device)
{
    return fireEventHook(status, [&status, &device](SimulationEventListener& l) {
        return l.recoveryDeviceDeployment(status, device);
    });
}

//////// SimulationComputationalListener methods ////////

std::optional<AtmosphericConditions> SimulationListenerHelper::firePreAtmosphericModel(
    SimulationStatus& status)
{
    return firePreHook<AtmosphericConditions>(status, [&status](SimulationComputationListener& l) {
        return l.preAtmosphericModel(status);
    });
}

AtmosphericConditions SimulationListenerHelper::firePostAtmosphericModel(
    SimulationStatus& status, const AtmosphericConditions& conditions)
{
    return firePostHook<AtmosphericConditions>(
        status, conditions,
        [&status](SimulationComputationListener& l, const AtmosphericConditions& current) {
            return l.postAtmosphericModel(status, current);
        });
}

std::optional<Coordinate> SimulationListenerHelper::firePreWindModel(SimulationStatus& status)
{
    return firePreHook<Coordinate>(
        status, [&status](SimulationComputationListener& l) { return l.preWindModel(status); });
}

Coordinate SimulationListenerHelper::firePostWindModel(SimulationStatus& status,
                                                       const Coordinate& wind)
{
    return firePostHook<Coordinate>(
        status, wind, [&status](SimulationComputationListener& l, const Coordinate& current) {
            return l.postWindModel(status, current);
        });
}

double SimulationListenerHelper::firePreGravityModel(SimulationStatus& status)
{
    return firePreNumberHook(
        status, [&status](SimulationComputationListener& l) { return l.preGravityModel(status); });
}

double SimulationListenerHelper::firePostGravityModel(SimulationStatus& status, double gravity)
{
    return firePostNumberHook(status, gravity,
                              [&status](SimulationComputationListener& l, double current) {
                                  return l.postGravityModel(status, current);
                              });
}

std::optional<FlightConditions> SimulationListenerHelper::firePreFlightConditions(
    SimulationStatus& status)
{
    return firePreHook<FlightConditions>(status, [&status](SimulationComputationListener& l) {
        return l.preFlightConditions(status);
    });
}

FlightConditions SimulationListenerHelper::firePostFlightConditions(SimulationStatus& status,
                                                                    FlightConditions  conditions)
{
    return firePostHook<FlightConditions>(
        status, std::move(conditions),
        [&status](SimulationComputationListener& l, const FlightConditions& current) {
            return l.postFlightConditions(status, current);
        });
}

std::optional<AerodynamicForces> SimulationListenerHelper::firePreAerodynamicCalculation(
    SimulationStatus& status)
{
    return firePreHook<AerodynamicForces>(status, [&status](SimulationComputationListener& l) {
        return l.preAerodynamicCalculation(status);
    });
}

AerodynamicForces SimulationListenerHelper::firePostAerodynamicCalculation(
    SimulationStatus& status, const AerodynamicForces& forces)
{
    return firePostHook<AerodynamicForces>(
        status, forces,
        [&status](SimulationComputationListener& l, const AerodynamicForces& current) {
            return l.postAerodynamicCalculation(status, current);
        });
}

std::optional<RigidBody> SimulationListenerHelper::firePreMassCalculation(SimulationStatus& status)
{
    return firePreHook<RigidBody>(status, [&status](SimulationComputationListener& l) {
        return l.preMassCalculation(status);
    });
}

RigidBody SimulationListenerHelper::firePostMassCalculation(SimulationStatus& status,
                                                            const RigidBody&  mass)
{
    return firePostHook<RigidBody>(
        status, mass, [&status](SimulationComputationListener& l, const RigidBody& current) {
            return l.postMassCalculation(status, current);
        });
}

double SimulationListenerHelper::firePreThrustCalculation(SimulationStatus& status)
{
    return firePreNumberHook(status, [&status](SimulationComputationListener& l) {
        return l.preSimpleThrustCalculation(status);
    });
}

double SimulationListenerHelper::firePostThrustCalculation(SimulationStatus& status, double thrust)
{
    return firePostNumberHook(status, thrust,
                              [&status](SimulationComputationListener& l, double current) {
                                  return l.postSimpleThrustCalculation(status, current);
                              });
}

std::optional<AccelerationData> SimulationListenerHelper::firePreAccelerationCalculation(
    SimulationStatus& status)
{
    return firePreHook<AccelerationData>(status, [&status](SimulationComputationListener& l) {
        return l.preAccelerationCalculation(status);
    });
}

AccelerationData SimulationListenerHelper::firePostAccelerationCalculation(
    SimulationStatus& status, const AccelerationData& acceleration)
{
    return firePostHook<AccelerationData>(
        status, acceleration,
        [&status](SimulationComputationListener& l, const AccelerationData& current) {
            return l.postAccelerationCalculation(status, current);
        });
}

}  // namespace QtRocket
