#pragma once

namespace QtRocket
{

class Rocket;

/// Helpers over a whole rocket (OpenRocket's abstract class RocketUtils, a holder of static
/// methods; a namespace here).
namespace RocketUtils
{

/// The x extent of the selected configuration's bounds (FlightConfiguration::getBounds(), the
/// aerodynamic components): the largest x minus the smallest; 0 when there are no bounds.
[[nodiscard]] double getLength(const Rocket& rocket);

}  // namespace RocketUtils

}  // namespace QtRocket
