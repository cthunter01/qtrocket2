#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// How the reference length (and area) of the aerodynamic coefficients is chosen (OpenRocket's
/// ReferenceType).
///
/// Deferred to rocket-config: ReferenceType.getReferenceLength(FlightConfiguration), which walks
/// the configuration's active components: NOSECONE takes twice the fore radius of the first
/// symmetric component whose fore radius is at least 0.0005 m (else twice its aft radius when that
/// is), MAXIMUM twice the largest fore or aft radius of all symmetric components, CUSTOM the
/// rocket's custom reference length; NOSECONE and MAXIMUM fall back to
/// Rocket::kDefaultReferenceLength (MAXIMUM when twice the radius is below 0.001 m).
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

}  // namespace QtRocket
