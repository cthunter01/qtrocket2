#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

/// A concrete RocketComponent for the tests of the component model, standing in for the
/// concrete components that later groups port: its kind, mass, CG, length, radii, instances and
/// compatibility are all settable. It implements the interfaces the base classes look for
/// (Coaxial for a BODY_TUBE parent, RadiusPositionable, AnglePositionable, Instanceable) and is
/// an InsideColorComponent, like a body tube.
///
/// Setters of physical values fire AEROMASS_CHANGE, as a real component's do. componentChanged()
/// counts its calls.
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class TestComponent final : public RocketComponent,
                            public virtual Coaxial,
                            public virtual RadiusPositionable,
                            public virtual AnglePositionable,
                            public virtual Instanceable,
                            public InsideColorComponent
{
public:
    using RocketComponent::getRadiusOffset;
    using RocketComponent::isCompatible;
    using RocketComponent::setAxialOffset;  // the protected (method, offset) overload too

    /// A component of @p kind (a body tube by default) positioned by @p method, of length
    /// @p length.
    explicit TestComponent(ComponentKind kind = ComponentKind::BODY_TUBE,
                           AxialMethod method = AxialMethod::AFTER, double length = 0.0)
      : RocketComponent(method), m_kind(kind)
    {
        m_length = length;
    }

    /// A new TestComponent in a std::unique_ptr, for addChild().
    [[nodiscard]] static std::unique_ptr<TestComponent> make(
        double length = 0.0, ComponentKind kind = ComponentKind::BODY_TUBE,
        AxialMethod method = AxialMethod::AFTER)
    {
        return std::make_unique<TestComponent>(kind, method, length);
    }

    // ---- RocketComponent

    [[nodiscard]] ComponentKind kind() const noexcept override { return m_kind; }
    [[nodiscard]] double        getComponentMass() const override { return m_mass; }
    [[nodiscard]] Coordinate    getComponentCG() const override { return m_cg.setWeight(m_mass); }
    [[nodiscard]] double getLongitudinalUnitInertia() const override { return m_longitudinalUnit; }
    [[nodiscard]] double getRotationalUnitInertia() const override { return m_rotationalUnit; }
    [[nodiscard]] bool   allowsChildren() const override
    {
        return m_acceptsAll || !m_accepted.empty();
    }
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override
    {
        return m_acceptsAll || m_accepted.contains(kind);
    }
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override
    {
        std::vector<Coordinate> bounds;
        addBoundingBox(bounds, 0.0, m_length, m_outerRadius);
        return bounds;
    }
    [[nodiscard]] bool isAerodynamic() const override { return m_aerodynamic; }
    [[nodiscard]] bool isMassive() const override { return m_massive; }

    // ---- Coaxial

    [[nodiscard]] double getInnerRadius() const override { return m_innerRadius; }
    void                 setInnerRadius(double v) override
    {
        m_innerRadius = v;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    [[nodiscard]] double getOuterRadius() const override { return m_outerRadius; }
    void                 setOuterRadius(double v) override
    {
        m_outerRadius = v;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    [[nodiscard]] double getThickness() const override { return m_outerRadius - m_innerRadius; }

    // ---- RadiusPositionable

    [[nodiscard]] double getBoundingRadius() const override { return m_boundingRadius; }
    [[nodiscard]] double getRadiusOffset() const override { return m_radiusOffset; }
    void                 setRadiusOffset(double radius) override
    {
        m_radiusOffset = radius;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    [[nodiscard]] RadiusMethod getRadiusMethod() const override { return m_radiusMethod; }
    void                       setRadiusMethod(RadiusMethod method) override
    {
        m_radiusMethod = method;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    void setRadius(RadiusMethod method, double radius) override
    {
        setRadiusMethod(method);
        setRadiusOffset(radius);
    }

    // ---- AnglePositionable

    [[nodiscard]] double getAngleOffset() const override { return m_angleOffset; }
    void                 setAngleOffset(double angle) override
    {
        m_angleOffset = angle;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    [[nodiscard]] AngleMethod getAngleMethod() const override { return m_angleMethod; }
    void                      setAngleMethod(AngleMethod method) override
    {
        m_angleMethod = method;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }

    // ---- Instanceable

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override
    {
        return RocketComponent::getInstanceLocations();
    }
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override
    {
        return m_instanceOffsets;
    }
    [[nodiscard]] int getInstanceCount() const override
    {
        return static_cast<int>(m_instanceOffsets.size());
    }
    /// Makes @p newCount instances, all at the component's reference point, angle 0.
    void setInstanceCount(int newCount) override
    {
        m_instanceOffsets.assign(static_cast<std::size_t>(newCount), Coordinate::kZero);
        m_instanceAngles.assign(static_cast<std::size_t>(newCount), 0.0);
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    [[nodiscard]] std::string getPatternName() const override
    {
        return std::to_string(getInstanceCount()) + "-test";
    }
    [[nodiscard]] std::vector<double> getInstanceAngles() const override
    {
        return m_instanceAngles;
    }

    // ---- test controls

    void setLength(double length)
    {
        m_length = length;
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }
    void setMass(double mass)
    {
        m_mass = mass;
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
    void setCG(const Coordinate& cg)
    {
        m_cg = cg;
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
    void setUnitInertias(double longitudinal, double rotational)
    {
        m_longitudinalUnit = longitudinal;
        m_rotationalUnit   = rotational;
    }
    void setBoundingRadius(double radius) { m_boundingRadius = radius; }
    void setAerodynamic(bool aerodynamic) { m_aerodynamic = aerodynamic; }
    void setMassive(bool massive) { m_massive = massive; }
    /// Accepts children of @p kinds only.
    void setAccepted(std::set<ComponentKind> kinds)
    {
        m_acceptsAll = false;
        m_accepted   = std::move(kinds);
    }
    /// Accepts no children at all.
    void setAcceptsNothing()
    {
        m_acceptsAll = false;
        m_accepted.clear();
    }
    /// Sets the instances: their offsets and their angles (radians), which must be as many.
    void setInstances(std::vector<Coordinate> offsets, std::vector<double> angles)
    {
        m_instanceOffsets = std::move(offsets);
        m_instanceAngles  = std::move(angles);
        fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    }

    /// Runs @p hook from childAdded(), with each child added to this component.
    void setOnChildAdded(std::function<void(RocketComponent&)> hook)
    {
        m_onChildAdded = std::move(hook);
    }

    /// How many times componentChanged() ran.
    [[nodiscard]] int componentChangedCount() const noexcept { return m_componentChangedCount; }
    /// The last event componentChanged() received, as its type.
    [[nodiscard]] std::optional<int> lastChangeType() const noexcept { return m_lastChangeType; }

    // Expose the protected helpers the tests exercise.
    using RocketComponent::addBound;
    using RocketComponent::addBoundingBox;
    using RocketComponent::ringCG;
    using RocketComponent::ringLongitudinalUnitInertia;
    using RocketComponent::ringMass;
    using RocketComponent::ringRotationalUnitInertia;
    using RocketComponent::ringVolume;
    using RocketComponent::toDebugDetail;

    /// RocketComponent::copyFrom(), made callable (the fin set conversion's in-place load).
    std::vector<std::unique_ptr<RocketComponent>> loadFields(const RocketComponent& source)
    {
        return copyFrom(source);
    }

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override
    {
        return std::make_unique<TestComponent>(*this);
    }

    void componentChanged(const ComponentChangeEvent& event) override
    {
        ++m_componentChangedCount;
        m_lastChangeType = event.getType();
        RocketComponent::componentChanged(event);
    }

    void childAdded(RocketComponent& child) override
    {
        if (m_onChildAdded)
        {
            m_onChildAdded(child);
        }
    }

private:
    ComponentKind                         m_kind;
    double                                m_mass{0.0};
    Coordinate                            m_cg;
    double                                m_longitudinalUnit{0.0};
    double                                m_rotationalUnit{0.0};
    double                                m_innerRadius{0.0};
    double                                m_outerRadius{0.0};
    double                                m_boundingRadius{0.0};
    double                                m_radiusOffset{0.0};
    RadiusMethod                          m_radiusMethod{RadiusMethod::COAXIAL};
    double                                m_angleOffset{0.0};
    AngleMethod                           m_angleMethod{AngleMethod::RELATIVE};
    std::vector<Coordinate>               m_instanceOffsets{Coordinate::kZero};
    std::vector<double>                   m_instanceAngles{0.0};
    bool                                  m_aerodynamic{true};
    bool                                  m_massive{true};
    bool                                  m_acceptsAll{true};
    std::set<ComponentKind>               m_accepted;
    int                                   m_componentChangedCount{0};
    std::optional<int>                    m_lastChangeType;
    std::function<void(RocketComponent&)> m_onChildAdded;
};

}  // namespace QtRocket::Test
