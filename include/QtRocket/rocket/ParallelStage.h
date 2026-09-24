#pragma once

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightConfiguration;

/// A set of identical boosters strapped around the core (OpenRocket's ParallelStage, the "Booster
/// Set"): a stage of its own (it has a stage number and a separation configuration) whose
/// instances are spread around its parent's axis. It sits inside a body component of another
/// stage and holds body components.
///
/// Instances: getInstanceCount() boosters, the i-th at angle getAngleOffset() + i * 2 pi / count
/// and at the radius getRadiusMethod() gives for getRadiusOffset() (with RadiusMethod::SURFACE or
/// RELATIVE, the parent tube's outer radius plus getBoundingRadius(), the largest outer radius of
/// the booster's own body tubes and transitions: the boosters touch the core); instance offsets
/// are (0, r cos(angle), r sin(angle)).
///
/// Positioning: TOP, MIDDLE, BOTTOM or ABSOLUTE relative to the parent (BOTTOM by default); AFTER
/// is refused (ComponentAssembly::setAxialMethod()).
///
/// Deviations from OpenRocket: the multi-edit config listeners are not ported, by decision (see
/// RocketComponent); setAxialMethod() throws BugError without a parent (Java:
/// NullPointerException).
class ParallelStage : public AxialStage, public virtual RingInstanceable
{
public:
    using RocketComponent::getRadiusOffset;

    /// Two boosters, BOTTOM, radius RELATIVE with offset 0, angle RELATIVE with offset 0.
    ParallelStage();

    /// @p count boosters (not checked, as in Java).
    explicit ParallelStage(int count);

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::PARALLEL_STAGE;
    }

    /// An estimate of the booster set's extent for the vehicle's size: four points around x_min
    /// and four around x_max of the instances (x_max starts at Java's Double.MIN_VALUE, the
    /// smallest positive double), at the radius offset.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// False: a booster set is placed relative to its parent, not after a sibling.
    [[nodiscard]] bool isAfter() const override;

    /// Whether the booster set flies in @p config (its own stage is active): boosters are always
    /// launch stages.
    [[nodiscard]] bool isLaunchStage(const FlightConfiguration& config) const override;

    // ---- Instanceable

    [[nodiscard]] int getInstanceCount() const override;

    /// Sets the number of boosters and the angle between them (2 pi / count) and fires
    /// AEROMASS_CHANGE; a count below 1 is ignored.
    void setInstanceCount(int newCount) override;

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// "<count>-ring".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- RingInstanceable

    [[nodiscard]] double              getInstanceAngleIncrement() const override;
    [[nodiscard]] std::vector<double> getInstanceAngles() const override;

    // ---- AnglePositionable

    [[nodiscard]] double getAngleOffset() const override;
    /// Sets the angle offset, reduced to -pi ... pi (MathUtil::reducePi), and fires
    /// AEROMASS_CHANGE.
    void setAngleOffset(double angle) override;

    [[nodiscard]] AngleMethod getAngleMethod() const override;
    /// Sets the angle method and fires AEROMASS_CHANGE.
    void setAngleMethod(AngleMethod newMethod) override;

    // ---- RadiusPositionable

    /// ComponentAssembly::getBoundingRadius(): the largest outer radius of the direct children.
    [[nodiscard]] double getBoundingRadius() const override;

    [[nodiscard]] double getRadiusOffset() const override;
    /// Sets the radius offset (0 when the method clamps to zero) and fires AEROMASS_CHANGE;
    /// nothing happens when it is exactly the current offset.
    void setRadiusOffset(double radius) override;

    [[nodiscard]] RadiusMethod getRadiusMethod() const override;
    /// Switches to @p method keeping the current radius (setRadius() with the radius the current
    /// method gives); nothing happens for the current method.
    void setRadiusMethod(RadiusMethod method) override;

    /// Sets the method and the offset that gives @p radius under it (0 when @p method clamps to
    /// zero), and fires AEROMASS_CHANGE.
    void setRadius(RadiusMethod method, double radius) override;

    // ---- AxialPositionable

    /// ComponentAssembly::setAxialMethod() (AFTER becomes TOP), then fires NONFUNCTIONAL_CHANGE
    /// once more, as Java does.
    /// @throws BugError without a parent.
    void setAxialMethod(AxialMethod newMethod) override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    int          m_instanceCount{2};
    AngleMethod  m_angleMethod{AngleMethod::RELATIVE};
    double       m_angleSeparation{std::numbers::pi};
    double       m_angleOffset{0};
    RadiusMethod m_radiusMethod{RadiusMethod::RELATIVE};
    double       m_radiusOffset{0};
};

}  // namespace QtRocket
