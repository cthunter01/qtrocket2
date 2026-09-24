#include "QtRocket/unit/RadianUnit.h"

#include <cmath>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/util/DecimalFormat.h"

namespace QtRocket
{

RadianUnit::RadianUnit() : GeneralUnit(1, "rad") { }

double RadianUnit::round(double v) const
{
    return std::nearbyint(v * 10.0) / 10.0;
}

std::string RadianUnit::toString(double value) const
{
    const double               val = toUnit(value);
    static const DecimalFormat kFormat("0.0");
    return kFormat.format(val);
}

}  // namespace QtRocket
