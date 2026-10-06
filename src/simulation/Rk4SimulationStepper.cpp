#include "QtRocket/simulation/Rk4SimulationStepper.h"

#include <array>
#include <optional>

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

void Rk4SimulationStepper::step(SimulationStatus& status, double maxTimeStep)
{
    ////////  Perform RK4 integration:  ////////

    //// First position, k1 = f(t, y)

    const std::optional<RkParameters> first = startStep(status, maxTimeStep);
    if (!first.has_value())
    {
        // maxTimeStep is NaN: the simulation parameters are recorded and that is all
        return;
    }
    const RkParameters& k1       = *first;
    const double        timeStep = getStore().timeStep;

    //// Second position, k2 = f(t + h/2, y + k1*h/2)

    const RkParameters k2 = computeStage(status, status.getSimulationTime() + (timeStep / 2),
                                         std::array{StageTerm(k1, timeStep / 2)});

    //// Third position, k3 = f(t + h/2, y + k2*h/2)

    const RkParameters k3 = computeStage(status, status.getSimulationTime() + (timeStep / 2),
                                         std::array{StageTerm(k2, timeStep / 2)});

    //// Fourth position, k4 = f(t + h, y + k3*h)

    const RkParameters k4 = computeStage(status, status.getSimulationTime() + timeStep,
                                         std::array{StageTerm(k3, timeStep)});

    //// Sum all together,  y(n+1) = y(n) + h*(k1 + 2*k2 + 2*k3 + k4)/6
    const Coordinate deltaV =
        Coordinate::kZero.addScaled(k2.a, 2).addScaled(k3.a, 2).add(k1.a).add(k4.a).multiply(
            timeStep / 6);
    const Coordinate deltaP =
        Coordinate::kZero.addScaled(k2.v, 2).addScaled(k3.v, 2).add(k1.v).add(k4.v).multiply(
            timeStep / 6);
    const Coordinate deltaR =
        Coordinate::kZero.addScaled(k2.ra, 2).addScaled(k3.ra, 2).add(k1.ra).add(k4.ra).multiply(
            timeStep / 6);
    const Coordinate deltaO =
        Coordinate::kZero.addScaled(k2.rv, 2).addScaled(k3.rv, 2).add(k1.rv).add(k4.rv).multiply(
            timeStep / 6);

    finishStep(status, deltaV, deltaP, deltaR, deltaO);
}

}  // namespace QtRocket
