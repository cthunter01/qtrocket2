#include "QtRocket/unit/RadianUnit.h"

#include <cmath>
#include <memory>
#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
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

std::unique_ptr<Unit> RadianUnit::clone() const
{
    return std::make_unique<RadianUnit>(*this);
}

}  // namespace QtRocket
