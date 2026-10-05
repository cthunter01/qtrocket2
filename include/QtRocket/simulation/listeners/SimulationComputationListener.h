#pragma once

#include <optional>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class SimulationStatus;

/// The hooks of a simulation listener for the computations of a simulation step (OpenRocket's
/// simulation/listeners/SimulationComputationListener): the atmospheric model, the wind model,
/// the gravity model, the flight conditions, the aerodynamic forces, the mass, the thrust and
/// the acceleration, each with a hook before the computation and one after it.
///
/// A `pre` hook returns the value to use in place of the computation, or "no change" to let
/// the computation run; the first listener that returns a value wins. A `post` hook is given
/// the computed value and returns the value to use in its place, or "no change"; the listeners
/// are asked in turn, each seeing what the ones before it made of the value. "No change" is
/// std::nullopt where Java returns null, and NaN for the two hooks that return a double
/// (gravity and thrust), as in Java.
///
/// It derives from SimulationListener, as in Java, virtually: AbstractSimulationListener
/// implements both.
///
/// Deviations from OpenRocket: the status is passed by reference and the computed value of a
/// `post` hook by reference to const (Java passes a clone the hook may change and return; here
/// the hook returns a changed copy).
class SimulationComputationListener : public virtual SimulationListener
{
public:
    ~SimulationComputationListener() override = default;

    SimulationComputationListener& operator=(const SimulationComputationListener&) = delete;
    SimulationComputationListener& operator=(SimulationComputationListener&&)      = delete;

    /// The acceleration to use in place of the computed one, or nullopt.
    [[nodiscard]] virtual std::optional<AccelerationData> preAccelerationCalculation(
        SimulationStatus& status) = 0;

    /// The acceleration to use in place of @p acceleration, or nullopt.
    [[nodiscard]] virtual std::optional<AccelerationData> postAccelerationCalculation(
        SimulationStatus& status, const AccelerationData& acceleration) = 0;

    /// The atmospheric conditions to use in place of the model's, or nullopt.
    [[nodiscard]] virtual std::optional<AtmosphericConditions> preAtmosphericModel(
        SimulationStatus& status) = 0;

    /// The atmospheric conditions to use in place of @p atmosphericConditions, or nullopt.
    [[nodiscard]] virtual std::optional<AtmosphericConditions> postAtmosphericModel(
        SimulationStatus& status, const AtmosphericConditions& atmosphericConditions) = 0;

    /// The wind velocity to use in place of the model's, or nullopt.
    [[nodiscard]] virtual std::optional<Coordinate> preWindModel(SimulationStatus& status) = 0;

    /// The wind velocity to use in place of @p wind, or nullopt.
    [[nodiscard]] virtual std::optional<Coordinate> postWindModel(SimulationStatus& status,
                                                                  const Coordinate& wind) = 0;

    /// The gravitational acceleration to use in place of the model's, or NaN.
    [[nodiscard]] virtual double preGravityModel(SimulationStatus& status) = 0;

    /// The gravitational acceleration to use in place of @p gravity, or NaN.
    [[nodiscard]] virtual double postGravityModel(SimulationStatus& status, double gravity) = 0;

    /// The flight conditions to use in place of the computed ones, or nullopt.
    [[nodiscard]] virtual std::optional<FlightConditions> preFlightConditions(
        SimulationStatus& status) = 0;

    /// The flight conditions to use in place of @p flightConditions, or nullopt.
    [[nodiscard]] virtual std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& status, const FlightConditions& flightConditions) = 0;

    /// The aerodynamic forces to use in place of the calculated ones, or nullopt.
    [[nodiscard]] virtual std::optional<AerodynamicForces> preAerodynamicCalculation(
        SimulationStatus& status) = 0;

    /// The aerodynamic forces to use in place of @p forces, or nullopt.
    [[nodiscard]] virtual std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& status, const AerodynamicForces& forces) = 0;

    /// The mass data to use in place of the calculated one, or nullopt.
    [[nodiscard]] virtual std::optional<RigidBody> preMassCalculation(SimulationStatus& status) = 0;

    /// The mass data to use in place of @p massData, or nullopt.
    [[nodiscard]] virtual std::optional<RigidBody> postMassCalculation(
        SimulationStatus& status, const RigidBody& massData) = 0;

    /// The thrust to use in place of the motors', in N, or NaN.
    [[nodiscard]] virtual double preSimpleThrustCalculation(SimulationStatus& status) = 0;

    /// The thrust to use in place of @p thrust, in N, or NaN.
    [[nodiscard]] virtual double postSimpleThrustCalculation(SimulationStatus& status,
                                                             double            thrust) = 0;

protected:
    SimulationComputationListener()                                     = default;
    SimulationComputationListener(const SimulationComputationListener&) = default;
    SimulationComputationListener(SimulationComputationListener&&)      = default;
};

}  // namespace QtRocket
