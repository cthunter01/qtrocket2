#include "QtRocket/simulation/Rk6SimulationStepper.h"

#include <array>
#include <optional>

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

void Rk6SimulationStepper::step(SimulationStatus& status, double maxTimeStep)
{
    ////////  Perform RK6 integration:  ////////

    //// First position, k1 = f(t, y)

    const std::optional<RkParameters> first = startStep(status, maxTimeStep);
    if (!first.has_value())
    {
        // maxTimeStep is NaN: the simulation parameters are recorded and that is all
        return;
    }
    const RkParameters& k1       = *first;
    const double        timeStep = getStore().timeStep;
    const double        time     = status.getSimulationTime();

    // The weights are written as in Java (1.0/3, not a rounded literal), and each scale is
    // timeStep * weight, in that order.

    //// Second position, k2 = f(t + h/3, y + 1/3*h*k1)
    const RkParameters k2 = computeStage(status, time + (timeStep / 3),
                                         std::array{StageTerm(k1, timeStep * (1.0 / 3))});

    //// Third position, k3 = f(t + h*2/3, y + 2/3*h*k2)
    const RkParameters k3 = computeStage(status, time + (timeStep * 2 / 3),
                                         std::array{StageTerm(k2, timeStep * (2.0 / 3))});

    //// Fourth position, k4 = f(t + h*1/3, y + 1/12*h*k1 + 1/3*h*k2 - 1/12*h*k3)
    const RkParameters k4 = computeStage(
        status, time + (timeStep * 1 / 3),
        std::array{StageTerm(k1, timeStep * (1.0 / 12)), StageTerm(k2, timeStep * (1.0 / 3)),
                   StageTerm(k3, timeStep * (-1.0 / 12))});

    //// Fifth position, k5 = f(t + h*1/2, y - 1/16*h*k1 + 9/8*h*k2 - 3/16*h*k3 - 3/8*h*k4)
    const RkParameters k5 = computeStage(
        status, time + (timeStep * 1 / 2),
        std::array{StageTerm(k1, timeStep * (-1.0 / 16)), StageTerm(k2, timeStep * (9.0 / 8)),
                   StageTerm(k3, timeStep * (-3.0 / 16)), StageTerm(k4, timeStep * (-3.0 / 8))});

    //// Sixth position, k6 = f(t + h*1/2, y + 9/8*h*k2 - 3/8*h*k3 - 3/4*h*k4 + 1/2*h*k5)
    const RkParameters k6 = computeStage(
        status, time + (timeStep * 1 / 2),
        std::array{StageTerm(k2, timeStep * (9.0 / 8)), StageTerm(k3, timeStep * (-3.0 / 8)),
                   StageTerm(k4, timeStep * (-3.0 / 4)), StageTerm(k5, timeStep * (1.0 / 2))});

    //// Seventh position,
    //// k7 = f(t + h, y + 9/44*h*k1 - 9/11*h*k2 + 63/44*h*k3 + 18/11*h*k4 - 16/11*h*k6)
    const RkParameters k7 = computeStage(
        status, time + timeStep,
        std::array{StageTerm(k1, timeStep * (9.0 / 44)), StageTerm(k2, timeStep * (-9.0 / 11)),
                   StageTerm(k3, timeStep * (63.0 / 44)), StageTerm(k4, timeStep * (18.0 / 11)),
                   StageTerm(k6, timeStep * (-16.0 / 11))});

    //// Sum all together,
    //// y(n+1) = y(n) + dt*(11/120*k1 + 27/40*k3 + 27/40*k4 - 4/15*k5 - 4/15*k6 + 11/120*k7)
    const Coordinate deltaV = Coordinate::kZero.addScaled(k1.a, 11.0 / 120)
                                  .addScaled(k3.a, 27.0 / 40)
                                  .addScaled(k4.a, 27.0 / 40)
                                  .addScaled(k5.a, -4.0 / 15)
                                  .addScaled(k6.a, -4.0 / 15)
                                  .addScaled(k7.a, 11.0 / 120)
                                  .multiply(timeStep);
    const Coordinate deltaP = Coordinate::kZero.addScaled(k1.v, 11.0 / 120)
                                  .addScaled(k3.v, 27.0 / 40)
                                  .addScaled(k4.v, 27.0 / 40)
                                  .addScaled(k5.v, -4.0 / 15)
                                  .addScaled(k6.v, -4.0 / 15)
                                  .addScaled(k7.v, 11.0 / 120)
                                  .multiply(timeStep);
    const Coordinate deltaR = Coordinate::kZero.addScaled(k1.ra, 11.0 / 120)
                                  .addScaled(k3.ra, 27.0 / 40)
                                  .addScaled(k4.ra, 27.0 / 40)
                                  .addScaled(k5.ra, -4.0 / 15)
                                  .addScaled(k6.ra, -4.0 / 15)
                                  .addScaled(k7.ra, 11.0 / 120)
                                  .multiply(timeStep);
    const Coordinate deltaO = Coordinate::kZero.addScaled(k1.rv, 11.0 / 120)
                                  .addScaled(k3.rv, 27.0 / 40)
                                  .addScaled(k4.rv, 27.0 / 40)
                                  .addScaled(k5.rv, -4.0 / 15)
                                  .addScaled(k6.rv, -4.0 / 15)
                                  .addScaled(k7.rv, 11.0 / 120)
                                  .multiply(timeStep);

    finishStep(status, deltaV, deltaP, deltaR, deltaO);
}

}  // namespace QtRocket
