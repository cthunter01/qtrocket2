#pragma once

// Removing the pitch/yaw jitter from a simulation, as the harness that computed the golden data
// does (tools/openrocket-goldens: JitterRemoval.java). Test-only.

#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Test
{

/// Removes OpenRocket's pitch/yaw moment jitter from a simulation, exactly as the golden harness
/// does in Java, so that a run can be compared with the golden data (which were computed without
/// the jitter) and so that the steppers can be pinned with and without it.
///
/// AbstractRkSimulationStepper::calculateForces() adds up to +/-0.0005 to Cm and Cyaw after
/// every aerodynamic calculation and then fires the post-aerodynamic-calculation hook. The
/// forces listener answers that hook with the calculator's result for the same configuration
/// and the flight conditions the conditions listener captured, which is what the stepper had
/// before it added the random terms. The random numbers are drawn all the same.
///
/// Placement, as in the harness: the forces listener is the first simulation listener (later
/// listeners then see the jitter-free forces) and the conditions listener the last one (it
/// captures the flight conditions after every other listener has had its say). Both are system
/// listeners: they stay in the nested optimum-coast simulation and add no "listeners affected
/// the simulation" warning. Their clones share the captured conditions and the counter.
///
/// One difference from the harness: Java asks the call stack whether the hook was fired by the
/// Runge-Kutta stepper, since the landing and tumble steppers fire it too (with forces built
/// from a drag coefficient, without jitter, which must be left alone). Here those forces are
/// recognised by their normal force coefficient, which the Euler steppers leave NaN and the
/// aerodynamic calculator never does.
///
/// As the harness notes, the simulation keeps the jittered forces when the listener's forces
/// equal them within MathUtil::kEpsilon, which needs two random terms within about 1e-8 of zero
/// in the same calculation.
class JitterRemoval
{
public:
    /// The listener that replaces the jittered forces; install it first.
    [[nodiscard]] std::shared_ptr<SimulationListener> forcesListener() const
    {
        return std::make_shared<ForcesListener>(m_holder);
    }

    /// The listener that captures the flight conditions; install it last.
    [[nodiscard]] std::shared_ptr<SimulationListener> conditionsListener() const
    {
        return std::make_shared<ConditionsListener>(m_holder);
    }

    /// Puts the forces listener at the front of @p listeners and the conditions listener at
    /// their end.
    void install(std::vector<std::shared_ptr<SimulationListener>>& listeners) const
    {
        listeners.insert(listeners.begin(), forcesListener());
        listeners.push_back(conditionsListener());
    }

    /// How many jittered force results were replaced (the aerodynamic calculations of the
    /// Runge-Kutta steppers).
    [[nodiscard]] long replacements() const noexcept { return m_holder->replacements; }

private:
    /// The state the two listeners and their clones share.
    struct Holder
    {
        std::optional<FlightConditions> conditions;
        long                            replacements{0};
    };

    class ConditionsListener final : public CloneableSimulationListener<ConditionsListener>
    {
    public:
        explicit ConditionsListener(std::shared_ptr<Holder> holder)
          : m_holder(std::move(holder)) { }

        [[nodiscard]] bool isSystemListener() const override { return true; }

        [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
            SimulationStatus& /*status*/, const FlightConditions& flightConditions) override
        {
            // The argument is the stepper's conditions after all earlier listeners.
            m_holder->conditions = flightConditions;
            return std::nullopt;
        }

    private:
        std::shared_ptr<Holder> m_holder;
    };

    class ForcesListener final : public CloneableSimulationListener<ForcesListener>
    {
    public:
        explicit ForcesListener(std::shared_ptr<Holder> holder) : m_holder(std::move(holder)) { }

        [[nodiscard]] bool isSystemListener() const override { return true; }

        [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
            SimulationStatus& status, const AerodynamicForces& forces) override
        {
            if (std::isnan(forces.getCN()))
            {
                // The forces of a landing or tumble step: no jitter to remove.
                return std::nullopt;
            }
            if (!m_holder->conditions.has_value())
            {
                bug("Jitter removal: no flight conditions were captured before the aerodynamic "
                    "calculation (did a listener override preFlightConditions?)");
            }
            const FlightConditions conditions = std::move(*m_holder->conditions);
            m_holder->conditions.reset();
            m_holder->replacements++;
            const SimulationConditions& simulationConditions = *status.getSimulationConditions();
            AerodynamicCalculator& calculator = *simulationConditions.getAerodynamicCalculator();
            return calculator.getAerodynamicForces(status.getConfiguration(), conditions, nullptr);
        }

    private:
        std::shared_ptr<Holder> m_holder;
    };

    std::shared_ptr<Holder> m_holder{std::make_shared<Holder>()};
};

}  // namespace QtRocket::Test
