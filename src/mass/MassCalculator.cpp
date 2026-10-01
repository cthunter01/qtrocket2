#include "QtRocket/mass/MassCalculator.h"

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MassCalculation.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

RigidBody MassCalculator::calculateStructure(const FlightConfiguration& config)
{
    return calculate(MassCalculation::Type::STRUCTURE, config, Motor::kPseudoTimeEmpty);
}

RigidBody MassCalculator::calculateBurnout(const FlightConfiguration& config)
{
    return calculate(MassCalculation::Type::BURNOUT, config, Motor::kPseudoTimeBurnout);
}

RigidBody MassCalculator::calculateMotor(const FlightConfiguration& config)
{
    return calculate(MassCalculation::Type::MOTOR, config, Motor::kPseudoTimeLaunch);
}

RigidBody MassCalculator::calculateLaunch(const FlightConfiguration& config)
{
    return calculate(MassCalculation::Type::LAUNCH, config, Motor::kPseudoTimeLaunch);
}

RigidBody MassCalculator::calculateMotor(const FlightConfiguration& config, double simulationTime,
                                         std::span<const MotorClusterState* const> activeMotors)
{
    return calculate(MassCalculation::Type::MOTOR, config, simulationTime, activeMotors);
}

RigidBody MassCalculator::calculate(MassCalculation::Type type, const FlightConfiguration& config,
                                    double                                    simulationTime,
                                    std::span<const MotorClusterState* const> activeMotors)
{
    MassCalculation calculation{
        type,   config, simulationTime, activeMotors, config.getRocket(), Transformation::kIdentity,
        nullptr};
    calculation.calculateAssembly();
    return calculation.calculateMomentOfInertia();
}

RigidBody MassCalculator::calculate(MassCalculation::Type type, const FlightConfiguration& config,
                                    double                             simulationTime,
                                    std::span<const MotorClusterState> activeMotors)
{
    std::vector<const MotorClusterState*> pointers;
    pointers.reserve(activeMotors.size());
    for (const MotorClusterState& state : activeMotors)
    {
        pointers.push_back(&state);
    }
    return calculate(type, config, simulationTime,
                     std::span<const MotorClusterState* const>{pointers});
}

RigidBody MassCalculator::calculate(MassCalculation::Type type, const FlightConfiguration& config,
                                    double time)
{
    MassCalculation calculation{
        type, config, time, std::nullopt, config.getRocket(), Transformation::kIdentity, nullptr};
    calculation.calculateAssembly();
    return calculation.calculateMomentOfInertia();
}

CMAnalysisMap MassCalculator::getCMAnalysis(const FlightConfiguration& config)
{
    CMAnalysisMap analysisMap;

    MassCalculation calculation{MassCalculation::Type::LAUNCH,
                                config,
                                Motor::kPseudoTimeLaunch,
                                std::nullopt,
                                config.getRocket(),
                                Transformation::kIdentity,
                                &analysisMap};
    calculation.calculateAssembly();

    const Rocket&   rocket = config.getRocket();
    CMAnalysisEntry totals{rocket};
    totals.totalCM  = calculation.getCM();
    totals.eachMass = calculation.getCM().weight;
    analysisMap.insert_or_assign(CMAnalysisEntry::keyOf(rocket), std::move(totals));

    return analysisMap;
}

}  // namespace QtRocket
