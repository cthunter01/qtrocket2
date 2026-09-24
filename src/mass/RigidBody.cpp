#include "QtRocket/mass/RigidBody.h"

#include <format>
#include <string>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

RigidBody::RigidBody(const Coordinate& cm, double axialInertia, double longitudinalInertia)
  : RigidBody(cm, axialInertia, longitudinalInertia, longitudinalInertia)
{
}

RigidBody::RigidBody(const Coordinate& cm, double ixx, double iyy, double izz)
  : m_cm(cm), m_ixx(ixx), m_iyy(iyy), m_izz(izz)
{
    if ((0 > ixx) || (0 > iyy) || (0 > izz))
    {
        bug("  attempted to initialize an InertiaMatrix with a negative inertia value.");
    }
}

RigidBody RigidBody::add(const RigidBody& that) const
{
    const Coordinate newCM = m_cm.average(that.m_cm);

    const RigidBody movedThis = rebase(newCM);
    const RigidBody movedThat = that.rebase(newCM);

    const double newIxx = movedThis.m_ixx + movedThat.m_ixx;
    const double newIyy = movedThis.m_iyy + movedThat.m_iyy;
    const double newIzz = movedThis.m_izz + movedThat.m_izz;

    return RigidBody{newCM, newIxx, newIyy, newIzz};
}

RigidBody RigidBody::scaleMass(double factor) const
{
    return RigidBody{m_cm.setWeight(m_cm.weight * factor), m_ixx * factor, m_iyy * factor,
                     m_izz * factor};
}

bool RigidBody::isEmpty() const noexcept
{
    if (&kEmpty == this)
    {
        return true;
    }
    return kEmpty == *this;
}

bool RigidBody::operator==(const RigidBody& other) const noexcept
{
    if (this == &other)
    {
        return true;  // as Java, so a body equals itself even with a NaN field
    }
    return m_cm == other.m_cm && MathUtil::equals(m_ixx, other.m_ixx) &&
           MathUtil::equals(m_iyy, other.m_iyy) && MathUtil::equals(m_izz, other.m_izz);
}

RigidBody RigidBody::rebase(const Coordinate& newLocation) const
{
    const Coordinate delta = m_cm.sub(newLocation).setWeight(0.0);
    const double     x2    = MathUtil::pow2(delta.x);
    const double     y2    = MathUtil::pow2(delta.y);
    const double     z2    = MathUtil::pow2(delta.z);

    // Parallel axis theorem: I = I + m L^2 with L the distance from the axis, e.g.
    // L^2 = y^2 + z^2 for the x axis.
    const double newIxx = m_ixx + (m_cm.weight * (y2 + z2));
    const double newIyy = m_iyy + (m_cm.weight * (x2 + z2));
    const double newIzz = m_izz + (m_cm.weight * (x2 + y2));

    // MOI about the reference point
    return RigidBody{newLocation, newIxx, newIyy, newIzz};
}

RigidBody RigidBody::translateInertia(const Coordinate& delta) const
{
    const Coordinate newLocation = m_cm.add(delta);
    return rebase(newLocation);
}

std::string RigidBody::toString() const
{
    return toCMString() + " // " + toMOIString();
}

std::string RigidBody::toCMString() const
{
    return std::format("CoM: {}g @[{},{},{}]", Strings::formatFixed(m_cm.weight, 8),
                       Strings::formatFixed(m_cm.x, 8), Strings::formatFixed(m_cm.y, 8),
                       Strings::formatFixed(m_cm.z, 8));
}

std::string RigidBody::toMOIString() const
{
    return std::format("MOI: [ {}, {}, {}]", Strings::formatFixed(m_ixx, 8),
                       Strings::formatFixed(m_iyy, 8), Strings::formatFixed(m_izz, 8));
}

}  // namespace QtRocket
