#pragma once

#include "QtRocket/models/GravityModel.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

/// Gravity on the WGS84 ellipsoid (OpenRocket's models/gravity/WGSGravityModel): Somigliana's
/// normal gravity at the latitude,
///   g0 = 9.7803267714 * (1 + 0.00193185138639 sin^2(lat)) / sqrt(1 - 0.00669437999013 sin^2(lat)),
/// scaled by the inverse square of the distance from the centre of a spherical Earth,
///   g = g0 * (R / (R + altitude))^2 with R = WorldCoordinate::kRearth.
/// The longitude plays no part, and neither does the mass of the atmosphere.
///
/// The model has no state, so modId() is always ModId::zero().
///
/// Deviation: Java caches the last result against the identity of the WorldCoordinate object it
/// was given. The computation is cheap and pure, so this recomputes every time: the result is the
/// same, and getGravity() stays const and free of shared mutable state.
class WgsGravityModel final : public GravityModel
{
public:
    WgsGravityModel()           = default;
    ~WgsGravityModel() override = default;

    WgsGravityModel(const WgsGravityModel&)            = default;
    WgsGravityModel& operator=(const WgsGravityModel&) = default;
    WgsGravityModel(WgsGravityModel&&)                 = default;
    WgsGravityModel& operator=(WgsGravityModel&&)      = default;

    /// The gravitational acceleration at @p wc in m/s^2 (see the class comment).
    [[nodiscard]] double getGravity(const WorldCoordinate& wc) const override;

    /// Always ModId::zero(): the model is immutable.
    [[nodiscard]] ModId modId() const noexcept override { return ModId::zero(); }
};

}  // namespace QtRocket
