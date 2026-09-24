#include "QtRocket/unit/DegreeUnit.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Chars.h"

namespace QtRocket
{

DegreeUnit::DegreeUnit() : GeneralUnit(std::numbers::pi / 180.0, std::string(Chars::kDegree)) { }

bool DegreeUnit::hasSpace() const
{
    return false;
}

double DegreeUnit::round(double v) const
{
    return std::nearbyint(v);
}

std::string DegreeUnit::toString(double value) const
{
    const double val = toUnit(value);
    return formatDecimal(val, 0, 1);  // DecimalFormat("0.#")
}

std::unique_ptr<Unit> DegreeUnit::clone() const
{
    return std::make_unique<DegreeUnit>(*this);
}

}  // namespace QtRocket
