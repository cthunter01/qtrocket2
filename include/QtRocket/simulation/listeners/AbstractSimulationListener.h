#pragma once

#include <memory>
#include <optional>
#include <typeinfo>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightEvent;
class MotorClusterState;
class MotorConfigurationId;
class MotorMount;
class RecoveryDevice;
class SimulationException;
class SimulationStatus;

/// The base of the simulation listeners (OpenRocket's simulation/listeners/
/// AbstractSimulationListener): it implements all three listener interfaces with hooks that
/// have no effect on the simulation, so that a listener overrides only the hooks it wants.
/// None of the default hooks touches the status. The defaults:
/// - the start, end and postStep() hooks do nothing;
/// - preStep(), addFlightEvent(), handleFlightEvent(), motorIgnition() and
///   recoveryDeviceDeployment() return true (go on);
/// - isSystemListener() returns false;
/// - the computation hooks return "no change": std::nullopt, and NaN for the gravity and
///   thrust hooks.
///
/// As in Java the class is not abstract despite its name: an AbstractSimulationListener is a
/// listener that does nothing.
///
/// clone(): Java's Object.clone() makes a shallow copy of the object's dynamic type, whatever
/// subclass it is. In C++ a class has to say how it is copied, so a listener derives from
/// CloneableSimulationListener<ItsClass> (below) in place of AbstractSimulationListener, which
/// gives it a clone() that copy-constructs ItsClass; that is all the boilerplate a listener
/// needs. A subclass that derives from AbstractSimulationListener directly must override
/// clone() itself: the clone() here refuses to slice it (BugError).
///
/// Deviation: the copy and move constructors are protected, so that a listener cannot be
/// sliced by copying it through this class; copies are made by clone().
class AbstractSimulationListener : public virtual SimulationListener,
                                   public SimulationComputationListener,
                                   public SimulationEventListener
{
    /// The key to the copying constructor that clone() calls through std::make_shared, which
    /// cannot reach the protected copy constructor; only this class can make one.
    struct CopyKey
    {
        explicit CopyKey() = default;
    };

public:
    AbstractSimulationListener() = default;

    /// A copy of @p other (clone()); the CopyKey keeps it private to this class.
    AbstractSimulationListener(CopyKey /*key*/, const AbstractSimulationListener& other)
      : AbstractSimulationListener(other)
    {
    }

    AbstractSimulationListener& operator=(const AbstractSimulationListener&) = delete;
    AbstractSimulationListener& operator=(AbstractSimulationListener&&)      = delete;
    ~AbstractSimulationListener() override                                   = default;

    //// SimulationListener ////

    void startSimulation(SimulationStatus& status) override;
    void endSimulation(SimulationStatus& status, const SimulationException* exception) override;
    void startSimulationBranch(SimulationStatus& status) override;
    void endSimulationBranch(SimulationStatus&          status,
                             const SimulationException* exception) override;
    /// Always true.
    [[nodiscard]] bool preStep(SimulationStatus& status) override;
    void               postStep(SimulationStatus& status) override;
    /// Always false.
    [[nodiscard]] bool isSystemListener() const override;

    /// A copy of this listener (Java: clone()).
    /// @throws BugError when the object is of a subclass that does not override clone() (see
    ///         the class comment)
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override;

    //// SimulationEventListener ////

    /// Always true.
    [[nodiscard]] bool addFlightEvent(SimulationStatus& status, const FlightEvent& event) override;
    /// Always true.
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override;
    /// Always true.
    [[nodiscard]] bool motorIgnition(SimulationStatus& status, const MotorConfigurationId& motorId,
                                     const MotorMount& mount, MotorClusterState& instance) override;
    /// Always true.
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus&     status,
                                                const RecoveryDevice& recoveryDevice) override;

    //// SimulationComputationListener ////

    /// Always nullopt.
    [[nodiscard]] std::optional<AccelerationData> preAccelerationCalculation(
        SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<AerodynamicForces> preAerodynamicCalculation(
        SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<AtmosphericConditions> preAtmosphericModel(
        SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<FlightConditions> preFlightConditions(
        SimulationStatus& status) override;
    /// Always NaN.
    [[nodiscard]] double preGravityModel(SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<RigidBody> preMassCalculation(SimulationStatus& status) override;
    /// Always NaN.
    [[nodiscard]] double preSimpleThrustCalculation(SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<Coordinate> preWindModel(SimulationStatus& status) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<AccelerationData> postAccelerationCalculation(
        SimulationStatus& status, const AccelerationData& acceleration) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& status, const AerodynamicForces& forces) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<AtmosphericConditions> postAtmosphericModel(
        SimulationStatus& status, const AtmosphericConditions& atmosphericConditions) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& status, const FlightConditions& flightConditions) override;
    /// Always NaN.
    [[nodiscard]] double postGravityModel(SimulationStatus& status, double gravity) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<RigidBody> postMassCalculation(SimulationStatus& status,
                                                               const RigidBody&  massData) override;
    /// Always NaN.
    [[nodiscard]] double postSimpleThrustCalculation(SimulationStatus& status,
                                                     double            thrust) override;
    /// Always nullopt.
    [[nodiscard]] std::optional<Coordinate> postWindModel(SimulationStatus& status,
                                                          const Coordinate& wind) override;

protected:
    AbstractSimulationListener(const AbstractSimulationListener&) = default;
    AbstractSimulationListener(AbstractSimulationListener&&)      = default;
};

/// The base of a concrete simulation listener: AbstractSimulationListener (or @p Base, another
/// listener class to build on) with the clone() that Java's Object.clone() gives every
/// listener, a copy of the object made with @p Derived's copy constructor. A listener is
/// written as
///
///     class CountingListener final : public CloneableSimulationListener<CountingListener>
///     {
///     public:
///         void postStep(SimulationStatus& status) override { m_steps++; }
///     private:
///         int m_steps{0};
///     };
///
/// and used as std::make_shared<CountingListener>(). @p Derived must be the class that derives
/// from this one, must be copy-constructible, and is the class clone() copies: a class derived
/// further from @p Derived needs its own clone() (derive it through
/// CloneableSimulationListener<Further, Derived>), which clone() checks (BugError).
///
/// The constructors are private and @p Derived is a friend, so that only @p Derived can be
/// built on CloneableSimulationListener<Derived> (a class that names another class there does
/// not compile). @p Base must be default-constructible.
template <class Derived, class Base = AbstractSimulationListener>
class CloneableSimulationListener : public Base
{
public:
    CloneableSimulationListener& operator=(const CloneableSimulationListener&) = delete;
    CloneableSimulationListener& operator=(CloneableSimulationListener&&)      = delete;
    ~CloneableSimulationListener() override                                    = default;

    /// A copy of this listener, made with @p Derived's copy constructor (Java: clone()).
    /// @throws BugError when the object is of a class derived from @p Derived that does not
    ///         override clone()
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override
    {
        // Java: Object.clone(), a shallow copy of the object's own class.
        const auto* self = dynamic_cast<const Derived*>(this);
        if (self == nullptr || typeid(*this) != typeid(Derived))
        {
            bug("clone() is not overridden by a class derived from a cloneable listener");
        }
        return std::make_shared<Derived>(*self);
    }

private:
    friend Derived;
    CloneableSimulationListener()                                   = default;
    CloneableSimulationListener(const CloneableSimulationListener&) = default;
    CloneableSimulationListener(CloneableSimulationListener&&)      = default;
};

}  // namespace QtRocket
