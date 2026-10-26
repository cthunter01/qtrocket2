#include "QtRocket/motor/chooser/MotorHolder.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

MotorHolder::MotorHolder(std::shared_ptr<const ThrustCurveMotor> motor, std::size_t index)
  : m_motor(std::move(motor)), m_index(index)
{
    QTROCKET_ASSERT(m_motor != nullptr);
}

std::string MotorHolder::toString(const Preferences& preferences) const
{
    return m_motor->getMotorName(preferences);
}

}  // namespace QtRocket
