#pragma once

#include <string>

#include "QtRocket/models/GravityModel.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

/// The same gravitational acceleration everywhere (OpenRocket's models/gravity/
/// ConstantGravityModel, a Java record). Any value is accepted, zero and negative included, as
/// in Java.
///
/// The model is immutable, so modId() is always ModId::zero().
class ConstantGravityModel final : public GravityModel
{
public:
    /// @p gravity in m/s^2.
    explicit ConstantGravityModel(double gravity) noexcept : m_gravity(gravity) { }
    ~ConstantGravityModel() override = default;

    ConstantGravityModel(const ConstantGravityModel&)            = default;
    ConstantGravityModel& operator=(const ConstantGravityModel&) = default;
    ConstantGravityModel(ConstantGravityModel&&)                 = default;
    ConstantGravityModel& operator=(ConstantGravityModel&&)      = default;

    /// The constant gravity, wherever @p wc is.
    [[nodiscard]] double getGravity(const WorldCoordinate& wc) const override;

    /// Always ModId::zero(): the model is immutable.
    [[nodiscard]] ModId modId() const noexcept override { return ModId::zero(); }

    /// The constant gravity in m/s^2 (getConstantGravity()).
    [[nodiscard]] double getConstantGravity() const noexcept { return m_gravity; }

    /// The record accessor gravity(): the same as getConstantGravity().
    [[nodiscard]] double gravity() const noexcept { return m_gravity; }

    /// The record's equals(): the gravities compare equal under Double.compare, so NaN equals NaN
    /// and 0.0 differs from -0.0.
    [[nodiscard]] bool operator==(const ConstantGravityModel& other) const noexcept;

    /// The record's hashCode(): Double.hashCode(gravity) (MathUtil::javaDoubleHashCode).
    [[nodiscard]] int hashCode() const noexcept;

    /// The record's toString(): "ConstantGravityModel[gravity=<Double.toString(gravity)>]", e.g.
    /// "ConstantGravityModel[gravity=9.807]".
    [[nodiscard]] std::string toString() const;

private:
    double m_gravity;
};

}  // namespace QtRocket
