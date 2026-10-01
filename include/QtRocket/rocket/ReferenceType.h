#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

class FlightConfiguration;

/// How the reference length (and area) of the aerodynamic coefficients is chosen (OpenRocket's
/// ReferenceType). getReferenceLength() computes it for a flight configuration.
enum class ReferenceType
{
    NOSECONE,
    MAXIMUM,
    CUSTOM,
};

/// Every type, in declaration order (ReferenceType.values()).
inline constexpr std::array<ReferenceType, 3> kAllReferenceTypes{
    ReferenceType::NOSECONE, ReferenceType::MAXIMUM, ReferenceType::CUSTOM};

/// The constant's name, e.g. "MAXIMUM" (Java: name()).
[[nodiscard]] std::string_view referenceTypeName(ReferenceType type) noexcept;

/// The .ork spelling the rocket saver writes in <referencetype>: the lower-cased name
/// ("nosecone", "maximum", "custom").
[[nodiscard]] std::string_view orkName(ReferenceType type) noexcept;

/// The type @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything else.
[[nodiscard]] std::optional<ReferenceType> referenceTypeFromOrkName(std::string_view text);

/// The reference length of @p config for @p type (ReferenceType.getReferenceLength()), from the
/// symmetric components (nose cones, transitions, body tubes) among the configuration's active
/// components, in FlightConfiguration::getActiveComponents() order:
/// - NOSECONE: twice the fore radius of the first one whose fore radius is at least 0.0005 m, or
///   else twice its aft radius when that is; Rocket::kDefaultReferenceLength when none qualifies.
/// - MAXIMUM: twice the largest fore or aft radius of all of them (Java's Math.max, so a NaN
///   radius gives NaN), or Rocket::kDefaultReferenceLength when that is below 0.001 m.
/// - CUSTOM: the rocket's custom reference length.
/// The radii are SymmetricComponent::getForeRadius() and getAftRadius(), read in Java's order. A
/// component of a body kind that is a RadialParent but not a SymmetricComponent (only the test
/// fixtures' TestBodyComponent is one) counts too, with getOuterRadius(-1) and
/// getOuterRadius(getLength()) as its radii (HOOK(test-fixtures), see ReferenceType.cpp).
[[nodiscard]] double getReferenceLength(ReferenceType type, const FlightConfiguration& config);

}  // namespace QtRocket
