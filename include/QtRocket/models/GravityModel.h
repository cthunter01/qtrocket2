#pragma once

#include "QtRocket/util/ModId.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

/// A model of the gravitational acceleration (OpenRocket's models/gravity/GravityModel): its
/// magnitude at a location on or above the Earth. The simulation asks it every step.
///
/// getGravity() is const and must be safe to call from several threads at once. Java's
/// GravityModel extends Monitorable; here modId() is a pure virtual member, so every model
/// satisfies the Monitorable concept. Models are polymorphic and held through pointers; copying
/// and moving are left to the concrete classes (protected here, so a model cannot be sliced).
class GravityModel
{
public:
    virtual ~GravityModel() = default;

    /// The gravitational acceleration at @p wc, in m/s^2.
    [[nodiscard]] virtual double getGravity(const WorldCoordinate& wc) const = 0;

    /// The id of the model's current state (Monitorable).
    [[nodiscard]] virtual ModId modId() const = 0;

protected:
    GravityModel()                               = default;
    GravityModel(const GravityModel&)            = default;
    GravityModel& operator=(const GravityModel&) = default;
    GravityModel(GravityModel&&)                 = default;
    GravityModel& operator=(GravityModel&&)      = default;
};

}  // namespace QtRocket
