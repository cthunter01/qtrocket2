#include "QtRocket/aero/LookupTableDragCalculator.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PolyInterpolator.h"

namespace QtRocket
{

namespace
{

/// The table column (COLUMN_CD).
constexpr std::string_view kColumnCd = "cd";

/// 17 degrees in radians, as Java writes it (17 * Math.PI / 180).
constexpr double kSeventeenDegrees = 17 * std::numbers::pi / 180;

/// The axial drag multiplier below 17 degrees (AXIAL_POLY1): 1 at 0 and 1.3 at 17 degrees, both
/// with zero slope.
[[nodiscard]] const std::vector<double>& axialPoly1()
{
    static const std::vector<double> kPoly =
        PolyInterpolator{{0, kSeventeenDegrees}, {0, kSeventeenDegrees}}.interpolator(
            {1, 1.3, 0, 0});
    return kPoly;
}

/// The axial drag multiplier from 17 to 90 degrees (AXIAL_POLY2): 1.3 at 17 and 0 at 90
/// degrees, zero slope at both and zero curvature at 90.
[[nodiscard]] const std::vector<double>& axialPoly2()
{
    static const std::vector<double> kPoly =
        PolyInterpolator{{kSeventeenDegrees, std::numbers::pi / 2},
                         {kSeventeenDegrees, std::numbers::pi / 2},
                         {std::numbers::pi / 2}}
            .interpolator({1.3, 0, 0, 0, 0});
    return kPoly;
}

/// Zeroes every drag value of the entries of @p forces.
void zeroDrag(ForceMap& forces)
{
    for (auto& entry : forces)
    {
        AerodynamicForces& entryForces = entry.second;
        entryForces.setFrictionCD(0);
        entryForces.setPressureCD(0);
        entryForces.setBaseCD(0);
        entryForces.setOverrideCD(0);
        entryForces.setCD(0);
        entryForces.setCDaxial(0);
    }
}

}  // namespace

LookupTableDragCalculator::LookupTableDragCalculator(MachAoALookup table)
  : LookupTableDragCalculator(std::make_shared<const MachAoALookup>(std::move(table)))
{
}

LookupTableDragCalculator::LookupTableDragCalculator(std::shared_ptr<const MachAoALookup> table)
  : m_table(std::move(table))
{
    QTROCKET_ASSERT(m_table != nullptr);
}

Result<LookupTableDragCalculator> LookupTableDragCalculator::fromCsv(
    const std::filesystem::path& csvPath)
{
    const std::vector<std::string> columns{std::string{kColumnCd}};
    Result<MachAoALookup>          table = CsvMachAoALookup::fromCsv(csvPath, columns);
    if (!table)
    {
        return std::unexpected(std::move(table.error()));
    }
    return LookupTableDragCalculator{std::move(*table)};
}

std::unique_ptr<DragCalculator> LookupTableDragCalculator::newInstance() const
{
    return std::make_unique<LookupTableDragCalculator>(*this);  // shares the table
}

void LookupTableDragCalculator::calculateDrag(const FlightConfiguration& /*configuration*/,
                                              const FlightConditions& conditions,
                                              ForceMap* componentForces, ForceMap* assemblyForces,
                                              AerodynamicForces& totalForces,
                                              WarningSet& /*warnings*/)
{
    const double mach       = conditions.getMach();
    const double aoaDegrees = MathUtil::javaToDegrees(conditions.getAOA());
    const double cd         = m_table->interpolate(mach, aoaDegrees, kColumnCd);

    if (componentForces != nullptr)
    {
        zeroDrag(*componentForces);
    }
    if (assemblyForces != nullptr)
    {
        zeroDrag(*assemblyForces);
    }

    totalForces.setFrictionCD(cd);
    totalForces.setPressureCD(0);
    totalForces.setBaseCD(0);
    totalForces.setOverrideCD(0);
    totalForces.setCD(cd);
    totalForces.setCDaxial(toAxialDrag(conditions, cd));
}

double LookupTableDragCalculator::toAxialDrag(const FlightConditions& conditions, double cd) const
{
    double     aoa      = MathUtil::clamp(conditions.getAOA(), 0, std::numbers::pi);
    const bool positive = aoa <= std::numbers::pi / 2;
    if (!positive)
    {
        aoa = std::numbers::pi - aoa;
    }
    double mul = 0;
    if (aoa < kSeventeenDegrees)
    {
        mul = PolyInterpolator::eval(aoa, axialPoly1());
    }
    else
    {
        mul = PolyInterpolator::eval(aoa, axialPoly2());
    }
    const double axial = mul * cd;
    return positive ? axial : -axial;
}

void LookupTableDragCalculator::voidAerodynamicCache()
{
    // no-op
}

}  // namespace QtRocket
