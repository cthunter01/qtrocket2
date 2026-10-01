#pragma once

#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;

/// An axially symmetric body component, the solid of revolution of a profile r = f(x) >= 0
/// around the x axis (OpenRocket's SymmetricComponent, together with its only base class
/// BodyComponent, which is folded in here: setLength(), allowsChildren(), getInnerRadius(),
/// getRadius(x, theta) and getInnerRadius(x, theta)). Body tubes, transitions and nose cones.
///
/// Geometry: calculateProperties() integrates the profile numerically over kDivisions = 128
/// slices exactly as OpenRocket does (the same formulas in the same order): each slice is a
/// hollow conical frustum whose wall height is the thickness projected onto the yz plane, which
/// gives the volume, the full (filled) volume, the CG, the unit inertias, the wetted area and the
/// planform area and centre. The results are cached until componentChanged() sees an
/// aerodynamic or mass change. As in OpenRocket, a component that is not in a Rocket receives no
/// events (see RocketComponent::fireComponentChangeEvent()), so once computed its cache is not
/// cleared by its own setters; compute after setting up, or put it in a rocket.
///
/// Neighbours: getPreviousSymmetricComponent() and getNextSymmetricComponent() walk the tree as
/// OpenRocket does (siblings, then the previous/next stages, inline pod sets and booster sets,
/// flush assemblies inside them), and the automatic radii of the subclasses read their
/// neighbours' getFrontAutoRadius() / getRearAutoRadius().
///
/// Deviations from OpenRocket:
/// - getFrontAutoRadius() and getRearAutoRadius() are public (Java: protected, which C++ would not
///   let a subclass call on another component).
/// - The neighbour searches come in a const and a non-const overload.
/// - The multi-edit config listeners are not ported (see RocketComponent).
class SymmetricComponent : public ExternalComponent,
                           public virtual BoxBounded,
                           public virtual RadialParent
{
public:
    /// The radius of a new component (DEFAULT_RADIUS), in m.
    static constexpr double kDefaultRadius = 0.025;
    /// The wall thickness of a new component (DEFAULT_THICKNESS), in m.
    static constexpr double kDefaultThickness = 0.002;
    /// The number of slices calculateProperties() integrates over (DIVISIONS).
    static constexpr int kDivisions = 128;

    // ---- BodyComponent

    /// Sets the length (negative values become 0, Java's Math.max) and fires AEROMASS_CHANGE,
    /// when it changes. A body tube keeps its preset; Transition overrides this to clear it.
    virtual void setLength(double length);

    /// True: body components accept children (subject to isCompatible()).
    [[nodiscard]] bool allowsChildren() const override;

    /// BodyComponent.getInnerRadius(): 0, from a field OpenRocket never assigns; a body tube
    /// overrides it with its real inner radius. (MassObject reads it for its parent.)
    [[nodiscard]] virtual double getInnerRadius() const;

    // ---- the profile

    /// The outer radius at @p x along the component; the fore radius ahead of it and the aft
    /// radius behind it.
    [[nodiscard]] virtual double getRadius(double x) const = 0;

    /// getRadius(x): the component is symmetric, so @p theta does not matter (Java: final).
    [[nodiscard]] double getRadius(double x, double /*theta*/) const { return getRadius(x); }

    /// The inner radius at @p x (RadialParent).
    [[nodiscard]] double getInnerRadius(double x) const override = 0;

    /// getInnerRadius(x) (Java: final).
    [[nodiscard]] double getInnerRadius(double x, double /*theta*/) const
    {
        return getInnerRadius(x);
    }

    /// getRadius(x) (RadialParent; Java: final).
    [[nodiscard]] double getOuterRadius(double x) const final { return getRadius(x); }

    /// RocketComponent::getLength() (RadialParent declares it too).
    [[nodiscard]] double getLength() const override { return RocketComponent::getLength(); }

    [[nodiscard]] virtual double getForeRadius() const         = 0;
    [[nodiscard]] virtual bool   isForeRadiusAutomatic() const = 0;
    [[nodiscard]] virtual double getAftRadius() const          = 0;
    [[nodiscard]] virtual bool   isAftRadiusAutomatic() const  = 0;

    /// The larger of the fore and aft radii (OpenRocket's MathUtil.max: a NaN radius is
    /// skipped).
    [[nodiscard]] double getMaxRadius() const;

    /// The wall thickness; the larger of the fore and aft radii when the component is filled
    /// (Java's Math.max).
    [[nodiscard]] virtual double getThickness() const;

    /// Sets the wall thickness (clamped to 0 ... getMaxRadius() when @p doClamping) and makes the
    /// component hollow; fires MASS_CHANGE and clears the preset, unless the thickness is already
    /// @p thickness and the component is not filled.
    void setThickness(double thickness, bool doClamping);

    /// setThickness(thickness, true).
    void setThickness(double thickness);

    /// Whether the component is solid; the wall thickness then has no effect.
    [[nodiscard]] bool isFilled() const noexcept { return m_filled; }

    /// Sets whether the component is solid; fires MASS_CHANGE and clears the preset when it
    /// changes.
    void setFilled(bool filled);

    /// True: body components follow one another.
    [[nodiscard]] bool isAfter() const override;

    // ---- bounds

    /// From (0, -r, -r) to (length, r, r) with r the larger of the fore and aft radii (Java's
    /// Math.max).
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    /// Four points (RocketComponent::addBound()) at each of x = n * length / 5, n = 0 ... 5, at
    /// radius getRadius(x).
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    // ---- the integrated properties (see the class comment)

    /// The volume of the material (Transition adds its shoulders).
    [[nodiscard]] double getComponentVolume() const override;

    /// The volume of the solid of revolution, as if the component were filled (no shoulders).
    [[nodiscard]] double getFullVolume() const;

    /// The wetted (outer surface) area.
    [[nodiscard]] virtual double getComponentWetArea() const;

    /// The planform area: the area of the profile's silhouette, integrate(2 r(x)).
    [[nodiscard]] virtual double getComponentPlanformArea() const;

    /// The planform centre: integrate(x * 2 r(x)) / planform area.
    [[nodiscard]] double getComponentPlanformCenter() const;

    /// The CG with the mass (density times the integrated volume) as the weight; a component
    /// with (almost) no volume has its CG at half its length, weight 0.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// Iyy = Izz per unit mass about the CG.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// Ixx per unit mass.
    [[nodiscard]] double getRotationalUnitInertia() const override;

    // ---- automatic radii

    /// The radius this component offers the component behind it for an automatic radius: a
    /// positive value is the preferred radius, a negative one means none (the search does not go
    /// towards the rear).
    [[nodiscard]] virtual double getFrontAutoRadius() const = 0;

    /// The radius this component offers the component ahead of it for an automatic radius
    /// (negative: none; the search does not go towards the front).
    [[nodiscard]] virtual double getRearAutoRadius() const = 0;

    /// The symmetric component ahead of this one, or nullptr: the nearest symmetric sibling in
    /// line ahead of it, else the last one of an earlier stage; when that one has flush inline
    /// assemblies at its end, the largest of their last symmetric children (recursively). With
    /// none, a symmetric grandparent in line (this component in a pod set or booster set) when it
    /// starts ahead of this one, else the grandparent's own previous component.
    [[nodiscard]] const SymmetricComponent* getPreviousSymmetricComponent() const;
    [[nodiscard]] SymmetricComponent*       getPreviousSymmetricComponent();

    /// The symmetric component behind this one, or nullptr: the nearest symmetric sibling in line
    /// behind it, else the first one of a later stage; when that one has flush inline assemblies
    /// at its front, the largest of their first symmetric children (recursively). With none, the
    /// first symmetric component in line in an assembly among this component's children when it
    /// ends behind this one, else that component's own next component.
    [[nodiscard]] const SymmetricComponent* getNextSymmetricComponent() const;
    [[nodiscard]] SymmetricComponent*       getNextSymmetricComponent();

    /// Whether the component takes its automatic diameter from the previous symmetric
    /// component.
    [[nodiscard]] virtual bool usesPreviousCompAutomatic() const = 0;

    /// Whether the component takes its automatic diameter from the next symmetric component.
    [[nodiscard]] virtual bool usesNextCompAutomatic() const = 0;

protected:
    /// A component positioned AFTER (BodyComponent's constructor), hollow, kDefaultThickness.
    SymmetricComponent();

    /// Loads ExternalComponent's properties, then the THICKNESS (directly, making the component
    /// hollow) and FILLED, without events.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    /// Clears the integrated properties on an aerodynamic or mass change.
    void componentChanged(const ComponentChangeEvent& event) override;

    /// Integrates over the length and fills the caches (see the class comment). Transition
    /// overrides it to add its shoulders, calling this version first.
    virtual void calculateProperties() const;

    /// Whether the component is solid (Java: protected field filled).
    bool m_filled{false};
    /// The wall thickness (Java: protected field thickness).
    double m_thickness{kDefaultThickness};

    // The cached integrated values; NaN (or nullopt) until calculated. Transition's
    // calculateProperties() replaces the first four with the values including its shoulders.
    mutable double m_volume{std::numeric_limits<double>::quiet_NaN()};
    mutable double m_longitudinalUnitInertia{std::numeric_limits<double>::quiet_NaN()};
    mutable double m_rotationalUnitInertia{std::numeric_limits<double>::quiet_NaN()};
    mutable std::optional<Coordinate> m_cg;

private:
    /// A pointer to a SymmetricComponent with the constness of @p Self.
    template <class Self>
    using SymmetricPointer =
        std::conditional_t<std::is_const_v<Self>, const SymmetricComponent*, SymmetricComponent*>;

    /// getPreviousSymmetricComponent() for both constnesses.
    template <class Self>
    [[nodiscard]] static SymmetricPointer<Self> previousSymmetricOf(Self& self);

    /// getNextSymmetricComponent() for both constnesses.
    template <class Self>
    [[nodiscard]] static SymmetricPointer<Self> nextSymmetricOf(Self& self);

    /// The last part of getNextSymmetricComponent(): the first symmetric component in line in an
    /// assembly among @p self's children when it ends behind @p self (in @p parent), else that
    /// component's own next component; nullptr without one.
    template <class Self>
    [[nodiscard]] static SymmetricPointer<Self> nextInOwnAssemblies(Self&                  self,
                                                                    const RocketComponent& parent);

    /// getPreviousSymmetricComponentFromComponentAssembly(): the largest last symmetric child in
    /// line and flush with the end of @p parent among its assemblies (recursively), or
    /// @p previous.
    template <class Symmetric>
    [[nodiscard]] static Symmetric* previousFromAssemblies(Symmetric* parent, Symmetric* previous,
                                                           double flushOffset);

    /// getNextSymmetricComponentFromComponentAssembly(): the mirror image at the front.
    template <class Symmetric>
    [[nodiscard]] static Symmetric* nextFromAssemblies(Symmetric* parent, Symmetric* next,
                                                       double flushOffset);

    /// inline(): whether @p candidate is on this component's axis (same parent, or no radial
    /// offset of a ring-instanced parent on either side).
    [[nodiscard]] bool isInline(const RocketComponent& candidate) const;

    /// The cached CG, calculated when needed (getSymmetricComponentCG(); Transition's includes
    /// its shoulders).
    [[nodiscard]] Coordinate getSymmetricComponentCG() const;

    mutable double m_wetArea{std::numeric_limits<double>::quiet_NaN()};
    mutable double m_planArea{std::numeric_limits<double>::quiet_NaN()};
    mutable double m_planCenter{std::numeric_limits<double>::quiet_NaN()};
    mutable double m_fullVolume{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
