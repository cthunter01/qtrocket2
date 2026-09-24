#pragma once

#include <memory>
#include <optional>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestComponent.h"

namespace QtRocket::Test
{

/// A TestComponent standing in for a symmetric body component (body tube, nose cone,
/// transition): it is BoxBounded, as OpenRocket's SymmetricComponent, so that FlightConfiguration
/// transforms its bounds with each instance, and a RadialParent with a fore and an aft radius
/// (both the outer radius unless set), which is how ReferenceType and
/// ComponentAssembly::getBoundingRadius() read a symmetric component until SymmetricComponent
/// exists. Its instance box is SymmetricComponent's: from (0, -r, -r) to (length, r, r) with r
/// the larger of the two radii.
class TestBodyComponent : public TestComponent,
                          public virtual BoxBounded,
                          public virtual RadialParent
{
public:
    using TestComponent::getInnerRadius;
    using TestComponent::getOuterRadius;

    explicit TestBodyComponent(ComponentKind kind = ComponentKind::BODY_TUBE,
                               AxialMethod method = AxialMethod::AFTER, double length = 0.0)
      : TestComponent(kind, method, length)
    {
    }

    /// A body of @p length and outer radius @p radius, in a std::unique_ptr for addChild().
    [[nodiscard]] static std::unique_ptr<TestBodyComponent> make(
        double length, double radius, ComponentKind kind = ComponentKind::BODY_TUBE,
        AxialMethod method = AxialMethod::AFTER)
    {
        auto body = std::make_unique<TestBodyComponent>(kind, method, length);
        body->setOuterRadius(radius);
        return body;
    }

    /// Sets the fore and aft radii (a transition's or nose cone's).
    void setForeAftRadii(double fore, double aft)
    {
        m_foreRadius = fore;
        m_aftRadius  = aft;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }

    [[nodiscard]] double getForeRadius() const { return m_foreRadius.value_or(getOuterRadius()); }
    [[nodiscard]] double getAftRadius() const { return m_aftRadius.value_or(getOuterRadius()); }

    // ---- RadialParent

    /// The fore radius before the body, the aft radius at and after its end, linear between.
    [[nodiscard]] double getOuterRadius(double x) const override
    {
        if (x < 0)
        {
            return getForeRadius();
        }
        if (x >= getLength())
        {
            return getAftRadius();
        }
        return getForeRadius() + ((getAftRadius() - getForeRadius()) * x / getLength());
    }
    [[nodiscard]] double getInnerRadius(double /*x*/) const override { return getInnerRadius(); }
    [[nodiscard]] double getLength() const override { return RocketComponent::getLength(); }

    // ---- BoxBounded

    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override
    {
        BoundingBox  box;
        const double r = MathUtil::javaMax(getForeRadius(), getAftRadius());
        box.update(Coordinate{getLength(), 0, 0});
        box.update(Coordinate{0, r, r});
        box.update(Coordinate{0, -r, -r});
        return box;
    }

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override
    {
        return std::make_unique<TestBodyComponent>(*this);
    }

private:
    std::optional<double> m_foreRadius;
    std::optional<double> m_aftRadius;
};

}  // namespace QtRocket::Test
