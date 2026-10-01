#include "QtRocket/aero/AerodynamicForces.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Java's `component instanceof ComponentAssembly`.
[[nodiscard]] bool isAssemblyComponent(const RocketComponent& component) noexcept
{
    return isAssembly(component.kind());
}

}  // namespace

void AerodynamicForces::set(double& field, double value)
{
    if (field == value)
    {
        return;
    }
    field   = value;
    m_modId = ModId{};
}

void AerodynamicForces::setAxisymmetric(bool isSym)
{
    if (m_axisymmetric == isSym)
    {
        return;
    }
    m_axisymmetric = isSym;
    m_modId        = ModId{};
}

void AerodynamicForces::setComponent(const RocketComponent* component)
{
    if (m_component == component)
    {
        return;
    }
    m_component = component;
    m_modId     = ModId{};
}

void AerodynamicForces::setCP(const Coordinate& cp)
{
    Coordinate newCpCNa;
    if (MathUtil::equals(0, cp.weight))
    {
        newCpCNa = Coordinate::kZero;
    }
    else
    {
        newCpCNa = Coordinate{cp.x * cp.weight, cp.y * cp.weight, cp.z * cp.weight, cp.weight};
    }

    if (m_cpCNa == newCpCNa)
    {
        return;
    }
    m_cpCNa = newCpCNa;
    m_modId = ModId{};
}

Coordinate AerodynamicForces::getCP() const noexcept
{
    if (MathUtil::equals(0, m_cpCNa.weight))
    {
        return Coordinate::kZero;
    }
    return Coordinate{m_cpCNa.x / m_cpCNa.weight, m_cpCNa.y / m_cpCNa.weight,
                      m_cpCNa.z / m_cpCNa.weight, m_cpCNa.weight};
}

void AerodynamicForces::setCN(double cN)
{
    set(m_cn, cN);
}

void AerodynamicForces::setCm(double cm)
{
    set(m_cm, cm);
}

void AerodynamicForces::setCside(double cside)
{
    set(m_cside, cside);
}

void AerodynamicForces::setCyaw(double cyaw)
{
    set(m_cyaw, cyaw);
}

void AerodynamicForces::setCroll(double croll)
{
    set(m_croll, croll);
}

void AerodynamicForces::setCrollDamp(double crollDamp)
{
    set(m_crollDamp, crollDamp);
}

void AerodynamicForces::setCrollForce(double crollForce)
{
    set(m_crollForce, crollForce);
}

void AerodynamicForces::setCDaxial(double cdaxial)
{
    set(m_cdAxial, cdaxial);
}

void AerodynamicForces::setCD(double cD)
{
    set(m_cd, cD);
}

double AerodynamicForces::getCD() const
{
    if (m_component == nullptr)
    {
        return m_cd;
    }
    if (m_component->isCDOverriddenByAncestor())
    {
        return 0;
    }
    // Assembly values are pre-aggregated with their descendants and direct override.
    if (m_component->isCDOverridden() && !isAssemblyComponent(*m_component))
    {
        return m_component->getOverrideCD();
    }
    return m_cd;
}

double AerodynamicForces::getCDTotal() const
{
    QTROCKET_ASSERT(m_component != nullptr);
    return getCD() * m_component->getInstanceCount();
}

bool AerodynamicForces::dragPartsOverridden() const
{
    return (m_component->isCDOverridden() && !isAssemblyComponent(*m_component)) ||
           m_component->isCDOverriddenByAncestor();
}

void AerodynamicForces::setPressureCD(double pressureCD)
{
    set(m_pressureCD, pressureCD);
}

double AerodynamicForces::getPressureCD() const
{
    if (m_component == nullptr)
    {
        return m_pressureCD;
    }
    return dragPartsOverridden() ? 0 : m_pressureCD;
}

void AerodynamicForces::setBaseCD(double baseCD)
{
    set(m_baseCD, baseCD);
}

double AerodynamicForces::getBaseCD() const
{
    if (m_component == nullptr)
    {
        return m_baseCD;
    }
    return dragPartsOverridden() ? 0 : m_baseCD;
}

void AerodynamicForces::setFrictionCD(double frictionCD)
{
    set(m_frictionCD, frictionCD);
}

double AerodynamicForces::getFrictionCD() const
{
    if (m_component == nullptr)
    {
        return m_frictionCD;
    }
    return dragPartsOverridden() ? 0 : m_frictionCD;
}

void AerodynamicForces::setOverrideCD(double overrideCD)
{
    set(m_overrideCD, overrideCD);
}

double AerodynamicForces::getOverrideCD() const
{
    if (m_component == nullptr)
    {
        return m_overrideCD;
    }
    if (m_component->isCDOverriddenByAncestor())
    {
        return 0;
    }
    // For assemblies this field contains all override contributions in the subtree.
    if (!isAssemblyComponent(*m_component) && !m_component->isCDOverridden())
    {
        return 0;
    }
    return m_overrideCD;
}

void AerodynamicForces::setPitchDampingMoment(double pitchDampingMoment)
{
    set(m_pitchDampingMoment, pitchDampingMoment);
}

void AerodynamicForces::setYawDampingMoment(double yawDampingMoment)
{
    set(m_yawDampingMoment, yawDampingMoment);
}

void AerodynamicForces::reset()
{
    setComponent(nullptr);

    // Java: setCP(null), a NullPointerException (see the header).
    if (!m_cpCNa.exactlyEquals(Coordinate::kNaN))
    {
        m_cpCNa = Coordinate::kNaN;
        m_modId = ModId{};
    }
    setCN(kNaN);
    setCm(kNaN);
    setCside(kNaN);
    setCyaw(kNaN);
    setCroll(kNaN);
    setCrollDamp(kNaN);
    setCrollForce(kNaN);
    setCDaxial(kNaN);
    setCD(kNaN);
    setPitchDampingMoment(kNaN);
    setYawDampingMoment(kNaN);
}

AerodynamicForces& AerodynamicForces::zero()
{
    // component untouched
    setAxisymmetric(true);
    setCP(Coordinate::kNul);
    setCN(0);
    setCm(0);
    setCside(0);
    setCyaw(0);
    setCroll(0);
    setCrollDamp(0);
    setCrollForce(0);
    setCDaxial(0);
    setCD(0);
    setPitchDampingMoment(0);
    setYawDampingMoment(0);
    return *this;
}

AerodynamicForces& AerodynamicForces::merge(const AerodynamicForces& other)
{
    m_cpCNa      = m_cpCNa.add(other.m_cpCNa);
    m_cn         = m_cn + other.getCN();
    m_cm         = m_cm + other.getCm();
    m_cside      = m_cside + other.getCside();
    m_cyaw       = m_cyaw + other.getCyaw();
    m_croll      = m_croll + other.getCroll();
    m_crollDamp  = m_crollDamp + other.getCrollDamp();
    m_crollForce = m_crollForce + other.getCrollForce();
    m_modId      = ModId{};
    return *this;
}

bool AerodynamicForces::operator==(const AerodynamicForces& other) const
{
    if (this == &other)
    {
        return true;
    }
    return MathUtil::equals(getCN(), other.getCN()) && MathUtil::equals(getCm(), other.getCm()) &&
           MathUtil::equals(getCside(), other.getCside()) &&
           MathUtil::equals(getCyaw(), other.getCyaw()) &&
           MathUtil::equals(getCroll(), other.getCroll()) &&
           MathUtil::equals(getCrollDamp(), other.getCrollDamp()) &&
           MathUtil::equals(getCrollForce(), other.getCrollForce()) &&
           MathUtil::equals(getCDaxial(), other.getCDaxial()) &&
           MathUtil::equals(getCD(), other.getCD()) &&
           MathUtil::equals(getPressureCD(), other.getPressureCD()) &&
           MathUtil::equals(getBaseCD(), other.getBaseCD()) &&
           MathUtil::equals(getFrictionCD(), other.getFrictionCD()) &&
           MathUtil::equals(getPitchDampingMoment(), other.getPitchDampingMoment()) &&
           MathUtil::equals(getYawDampingMoment(), other.getYawDampingMoment()) &&
           getCP() == other.getCP();
}

int AerodynamicForces::hashCode() const
{
    const Coordinate cp       = getCP();
    const int        dragHash = MathUtil::javaIntCast(1000 * (getCD() + getCDaxial() + cp.weight));
    // std::hash<Coordinate> is Coordinate.hashCode() widened to size_t; narrowing it back to int
    // is modular, which restores the Java int.
    const int cpHash = static_cast<int>(std::hash<Coordinate>{}(cp));
    return static_cast<int>(static_cast<std::uint32_t>(dragHash) +
                            static_cast<std::uint32_t>(cpHash));
}

std::string AerodynamicForces::toString() const
{
    std::string text = "AerodynamicForces[";

    if (m_component != nullptr)
    {
        text += "component:" + m_component->toString() + ',';
    }
    // Java tests getCP() != null, which always holds.
    text += "cp:" + getCP().toString() + ',';

    const auto append = [&text](const char* label, double value) {
        if (!std::isnan(value))
        {
            text += label;
            text += Strings::javaDoubleToString(value);
            text += ',';
        }
    };
    append("CN:", getCN());
    append("Cm:", getCm());
    append("Cside:", getCside());
    append("Cyaw:", getCyaw());
    append("Croll:", getCroll());
    append("CDaxial:", getCDaxial());
    append("CD:", getCD());

    if (text.back() == ',')
    {
        text.pop_back();
    }
    text += ']';
    return text;
}

}  // namespace QtRocket
