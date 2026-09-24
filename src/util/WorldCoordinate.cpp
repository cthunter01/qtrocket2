#include "QtRocket/util/WorldCoordinate.h"

#include <cstddef>
#include <format>
#include <functional>
#include <numbers>
#include <ostream>
#include <string>

#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

WorldCoordinate::WorldCoordinate(double latitudeDeg, double longitudeDeg, double altitude) noexcept
  : m_lat{MathUtil::clamp(MathUtil::deg2rad(latitudeDeg), -std::numbers::pi / 2,
                          std::numbers::pi / 2)},
    m_lon{MathUtil::reducePi(MathUtil::deg2rad(longitudeDeg))},
    m_alt{altitude}
{
}

double WorldCoordinate::getLongitudeDeg() const noexcept
{
    return MathUtil::rad2deg(m_lon);
}

double WorldCoordinate::getLatitudeDeg() const noexcept
{
    return MathUtil::rad2deg(m_lat);
}

bool WorldCoordinate::operator==(const WorldCoordinate& other) const noexcept
{
    return MathUtil::equals(m_lat, other.m_lat) && MathUtil::equals(m_lon, other.m_lon) &&
           MathUtil::equals(m_alt, other.m_alt);
}

std::string WorldCoordinate::toString() const
{
    // The header lists how this differs from Java's Double.toString.
    return std::format("WorldCoordinate[lat={}, lon={}, alt={}]", getLatitudeDeg(),
                       getLongitudeDeg(), getAltitude());
}

std::ostream& operator<<(std::ostream& os, const WorldCoordinate& wc)
{
    return os << wc.toString();
}

}  // namespace QtRocket

std::size_t std::hash<QtRocket::WorldCoordinate>::operator()(
    const QtRocket::WorldCoordinate& wc) const noexcept
{
    return static_cast<std::size_t>(QtRocket::MathUtil::javaIntCast(
        1000 * (wc.getLatitudeRad() + wc.getLongitudeRad() + wc.getAltitude())));
}
