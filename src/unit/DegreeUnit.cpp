#include "QtRocket/unit/DegreeUnit.h"

#include <cmath>
#include <numbers>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/DecimalFormat.h"

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
    const double               val = toUnit(value);
    static const DecimalFormat kFormat("0.#");
    return kFormat.format(val);
}

}  // namespace QtRocket
