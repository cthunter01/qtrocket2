#include "QtRocket/aero/LookupTableStabilityCalculator.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

// The table columns (COLUMN_CN, COLUMN_CM, COLUMN_CP).
constexpr std::string_view kColumnCn = "cn";
constexpr std::string_view kColumnCm = "cm";
constexpr std::string_view kColumnCp = "cp";

}  // namespace

LookupTableStabilityCalculator::LookupTableStabilityCalculator(MachAoALookup table)
  : LookupTableStabilityCalculator(std::make_shared<const MachAoALookup>(std::move(table)))
{
}

LookupTableStabilityCalculator::LookupTableStabilityCalculator(
    std::shared_ptr<const MachAoALookup> table)
  : m_table(std::move(table))
{
    QTROCKET_ASSERT(m_table != nullptr);
    // We'll assume anything with an angle of attack greater than the greatest defined AOA value
    // is stalled.
    if (m_table->hasAoA())
    {
        m_stallAngle = MathUtil::javaToRadians(m_table->getMaxAoA());
    }
}

Result<LookupTableStabilityCalculator> LookupTableStabilityCalculator::fromCsv(
    const std::filesystem::path& csvPath)
{
    const std::vector<std::string> columns{std::string{kColumnCn}, std::string{kColumnCm},
                                           std::string{kColumnCp}};
    Result<MachAoALookup>          table = CsvMachAoALookup::fromCsv(csvPath, columns);
    if (!table)
    {
        return std::unexpected(std::move(table.error()));
    }
    return LookupTableStabilityCalculator{std::move(*table)};
}

std::unique_ptr<StabilityCalculator> LookupTableStabilityCalculator::newInstance() const
{
    // Java builds a new calculator from the same table, which gives the same stall angle.
    return std::make_unique<LookupTableStabilityCalculator>(*this);
}

Coordinate LookupTableStabilityCalculator::getCP(const FlightConfiguration& configuration,
                                                 const FlightConditions&    conditions,
                                                 WarningSet&                warnings)
{
    return calculateNonAxialForces(configuration, conditions, warnings).getCP();
}

AerodynamicForces LookupTableStabilityCalculator::calculateNonAxialForces(
    const FlightConfiguration& /*configuration*/, const FlightConditions& conditions,
    WarningSet& /*warnings*/)
{
    const double mach       = conditions.getMach();
    const double aoaDegrees = MathUtil::javaToDegrees(conditions.getAOA());

    const double cn = m_table->interpolate(mach, aoaDegrees, kColumnCn);
    const double cm = m_table->interpolate(mach, aoaDegrees, kColumnCm);
    const double cp = m_table->interpolate(mach, aoaDegrees, kColumnCp);

    AerodynamicForces forces;
    forces.zero();
    forces.setCN(cn);
    forces.setCm(cm);
    forces.setCP(Coordinate{cp, 0, 0, 1});
    forces.setCside(0);
    forces.setCyaw(0);
    forces.setCroll(0);
    forces.setCrollDamp(0);
    forces.setCrollForce(0);
    return forces;
}

StabilityForceBreakdown LookupTableStabilityCalculator::getForceAnalysis(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    WarningSet& warnings)
{
    AerodynamicForces total  = calculateNonAxialForces(configuration, conditions, warnings);
    const Rocket&     rocket = configuration.getRocket();
    total.setComponent(&rocket);

    ForceMap eachMap;
    ForceMap assemblyMap;

    for (const auto& [component, contexts] : configuration.getActiveInstances())
    {
        AerodynamicForces zero;
        zero.zero();
        zero.setComponent(component);
        zero.setCN(0);
        zero.setCm(0);
        zero.setCP(Coordinate::kZero);
        zero.setCside(0);
        zero.setCyaw(0);
        zero.setCroll(0);
        zero.setCrollDamp(0);
        zero.setCrollForce(0);

        if (isAssembly(component->kind()))
        {
            assemblyMap.put(component, zero);
        }
        else if (component->isAerodynamic())
        {
            eachMap.put(component, zero);
        }
    }

    assemblyMap.put(&rocket, total);

    return StabilityForceBreakdown{std::move(eachMap), std::move(assemblyMap)};
}

void LookupTableStabilityCalculator::calculateDampingMoments(
    const FlightConfiguration& /*configuration*/, const FlightConditions& /*conditions*/,
    AerodynamicForces& total)
{
    total.setPitchDampingMoment(0);
    total.setYawDampingMoment(0);
}

void LookupTableStabilityCalculator::checkGeometry(const FlightConfiguration& /*configuration*/,
                                                   const RocketComponent& /*component*/,
                                                   WarningSet& /*warnings*/)
{
    // Geometry validation is not required for lookup-based data
}

void LookupTableStabilityCalculator::voidAerodynamicCache()
{
    // no-op
}

}  // namespace QtRocket
