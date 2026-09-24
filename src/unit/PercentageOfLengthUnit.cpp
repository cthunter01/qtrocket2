#include "QtRocket/unit/PercentageOfLengthUnit.h"

#include <functional>
#include <memory>
#include <utility>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

PercentageOfLengthUnit::PercentageOfLengthUnit() : GeneralUnit(0.01, "%") { }

PercentageOfLengthUnit::PercentageOfLengthUnit(double reference)
  : GeneralUnit(0.01, "%"), m_referenceLengthProvider([reference] { return reference; })
{
    if (reference <= 0)
    {
        bug("Illegal reference = " + Strings::javaDoubleToString(reference));
    }
}

PercentageOfLengthUnit::PercentageOfLengthUnit(std::function<double()> referenceLengthProvider)
  : GeneralUnit(0.01, "%"), m_referenceLengthProvider(std::move(referenceLengthProvider))
{
}

double PercentageOfLengthUnit::getReferenceLength() const
{
    // OpenRocket: BugException("Both rocket and configuration are null")
    QTROCKET_ASSERT(m_referenceLengthProvider != nullptr);
    return m_referenceLengthProvider();
}

double PercentageOfLengthUnit::fromUnit(double value) const
{
    return value * getReferenceLength() * getMultiplier();
}

double PercentageOfLengthUnit::toUnit(double value) const
{
    return value / getReferenceLength() / getMultiplier();
}

std::unique_ptr<Unit> PercentageOfLengthUnit::clone() const
{
    return std::make_unique<PercentageOfLengthUnit>(*this);
}

}  // namespace QtRocket
