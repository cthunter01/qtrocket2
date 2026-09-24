#include "QtRocket/unit/CaliberUnit.h"

#include <functional>
#include <memory>
#include <utility>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

CaliberUnit::CaliberUnit() : GeneralUnit(1.0, "cal") { }

CaliberUnit::CaliberUnit(double reference)
  : GeneralUnit(1.0, "cal"), m_referenceLengthProvider([reference] { return reference; })
{
    if (reference <= 0)
    {
        bug("Illegal reference = " + Strings::javaDoubleToString(reference));
    }
}

CaliberUnit::CaliberUnit(std::function<double()> referenceLengthProvider)
  : GeneralUnit(1.0, "cal"), m_referenceLengthProvider(std::move(referenceLengthProvider))
{
}

double CaliberUnit::getReferenceLength() const
{
    // OpenRocket: BugException("Both rocket and configuration are null")
    QTROCKET_ASSERT(m_referenceLengthProvider != nullptr);
    return m_referenceLengthProvider();
}

double CaliberUnit::fromUnit(double value) const
{
    return value * getReferenceLength();
}

double CaliberUnit::toUnit(double value) const
{
    return value / getReferenceLength();
}

std::unique_ptr<Unit> CaliberUnit::clone() const
{
    return std::make_unique<CaliberUnit>(*this);
}

}  // namespace QtRocket
