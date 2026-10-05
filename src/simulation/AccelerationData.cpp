#include "QtRocket/simulation/AccelerationData.h"

#include <optional>
#include <string>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"

namespace QtRocket
{

namespace
{

/// A coordinate as Java's string concatenation prints it: "null" or toString().
[[nodiscard]] std::string describe(const std::optional<Coordinate>& coordinate)
{
    return coordinate.has_value() ? coordinate->toString() : "null";
}

}  // namespace

AccelerationData::AccelerationData(const std::optional<Coordinate>& linearAccelerationRC,
                                   const std::optional<Coordinate>& rotationalAccelerationRC,
                                   const std::optional<Coordinate>& linearAccelerationWC,
                                   const std::optional<Coordinate>& rotationalAccelerationWC,
                                   const Quaternion&                rotation)
  : m_linearAccelerationRC(linearAccelerationRC),
    m_rotationalAccelerationRC(rotationalAccelerationRC),
    m_linearAccelerationWC(linearAccelerationWC),
    m_rotationalAccelerationWC(rotationalAccelerationWC),
    m_rotation(rotation)
{
    if ((!linearAccelerationRC.has_value() && !linearAccelerationWC.has_value()) ||
        (!rotationalAccelerationRC.has_value() && !rotationalAccelerationWC.has_value()))
    {
        bug("Parameter is null:  linearAccelerationRC=" + describe(linearAccelerationRC) +
            " linearAccelerationWC=" + describe(linearAccelerationWC) +
            " rotationalAccelerationRC=" + describe(rotationalAccelerationRC) +
            " rotationalAccelerationWC=" + describe(rotationalAccelerationWC) +
            " rotation=" + rotation.toString());
    }
}

Coordinate AccelerationData::getLinearAccelerationRC() const
{
    if (!m_linearAccelerationRC.has_value())
    {
        // The constructor guarantees the other frame.
        QTROCKET_ASSERT(m_linearAccelerationWC.has_value());
        m_linearAccelerationRC = m_rotation.invRotate(*m_linearAccelerationWC);
    }
    return *m_linearAccelerationRC;
}

Coordinate AccelerationData::getRotationalAccelerationRC() const
{
    if (!m_rotationalAccelerationRC.has_value())
    {
        QTROCKET_ASSERT(m_rotationalAccelerationWC.has_value());
        m_rotationalAccelerationRC = m_rotation.invRotate(*m_rotationalAccelerationWC);
    }
    return *m_rotationalAccelerationRC;
}

Coordinate AccelerationData::getLinearAccelerationWC() const
{
    if (!m_linearAccelerationWC.has_value())
    {
        QTROCKET_ASSERT(m_linearAccelerationRC.has_value());
        m_linearAccelerationWC = m_rotation.rotate(*m_linearAccelerationRC);
    }
    return *m_linearAccelerationWC;
}

Coordinate AccelerationData::getRotationalAccelerationWC() const
{
    if (!m_rotationalAccelerationWC.has_value())
    {
        QTROCKET_ASSERT(m_rotationalAccelerationRC.has_value());
        m_rotationalAccelerationWC = m_rotation.rotate(*m_rotationalAccelerationRC);
    }
    return *m_rotationalAccelerationWC;
}

bool AccelerationData::operator==(const AccelerationData& other) const
{
    if (this == &other)
    {
        return true;
    }
    return getLinearAccelerationRC() == other.getLinearAccelerationRC() &&
           getRotationalAccelerationRC() == other.getRotationalAccelerationRC();
}

}  // namespace QtRocket
