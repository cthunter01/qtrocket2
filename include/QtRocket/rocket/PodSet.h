#pragma once

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A set of identical pods around a body (OpenRocket's PodSet): like a booster set, but part of
/// its parent's stage rather than a stage of its own. It sits inside a body component and holds
/// body components; its instances are spread around its parent's axis as ParallelStage's are.
///
/// OpenRocket's PodSet differs from ParallelStage in details this port keeps: setAngleMethod()
/// does nothing (the angle method stays RELATIVE); setAngleOffset() stores the angle as given
/// (ParallelStage reduces it to -pi ... pi); setRadius() decides whether to clamp the radius to
/// zero by the method in force before the call (ParallelStage by the requested one); and
/// getAxialOffset() snaps an offset within MathUtil::kEpsilon of zero to zero.
///
/// Deviations from OpenRocket: the multi-edit config listeners are not ported, by decision (see
/// RocketComponent); setAxialMethod() and getRelativeToStage() throw BugError where Java throws
/// a NullPointerException (no parent, no grandparent).
class PodSet : public ComponentAssembly, public virtual RingInstanceable
{
public:
    using RocketComponent::getRadiusOffset;
    using RocketComponent::isCompatible;

    /// Two pods, BOTTOM, radius RELATIVE with offset 0, angle RELATIVE with offset 0.
    PodSet();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::POD_SET; }

    /// A pod set accepts body components only.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    /// False: a pod set is placed relative to its parent, not after a sibling.
    [[nodiscard]] bool isAfter() const override;

    /// The position of the parent among the grandparent's children when the parent is a pod set,
    /// else -1 (also without a parent).
    /// @throws BugError when the parent pod set has no parent.
    [[nodiscard]] int getRelativeToStage() const;

    // ---- Instanceable

    [[nodiscard]] int getInstanceCount() const override;

    /// Sets the number of pods and the angle between them (2 pi / count) and fires
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
    /// Sets the angle offset as given and fires AEROMASS_CHANGE.
    void setAngleOffset(double angle) override;

    [[nodiscard]] AngleMethod getAngleMethod() const override;
    /// Does nothing, as in Java.
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

    /// Sets the method and the offset that gives @p radius under it (0 when the method in force
    /// before the call clamps to zero), and fires AEROMASS_CHANGE.
    void setRadius(RadiusMethod method, double radius) override;

    // ---- AxialPositionable

    /// getAxialOffset(getAxialMethod()).
    [[nodiscard]] double getAxialOffset() const override;

    /// RocketComponent::getAxialOffset(@p method), 0 when within MathUtil::kEpsilon of zero.
    [[nodiscard]] double getAxialOffset(AxialMethod method) const override;

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
