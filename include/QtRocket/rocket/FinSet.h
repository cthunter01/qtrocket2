#pragma once

#include <array>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class MaterialStorage;
class Preferences;
class SymmetricComponent;

/// The cross-section of a fin (OpenRocket's FinSet.CrossSection, FinSet::CrossSection here): the
/// shape of the fin's profile across its thickness, which scales the fin's volume
/// (relativeVolume()) and selects the drag model of its edges. Java's constants carry a
/// translated name; here displayKey() gives the key and displayName() the English text.
enum class FinCrossSection
{
    SQUARE,   ///< square edges: the full volume
    ROUNDED,  ///< rounded edges: 0.99 of the volume
    AIRFOIL,  ///< an airfoil profile: 0.85 of the volume
};

/// Every cross-section, in declaration order (CrossSection.values()).
inline constexpr std::array<FinCrossSection, 3> kAllFinCrossSections{
    FinCrossSection::SQUARE, FinCrossSection::ROUNDED, FinCrossSection::AIRFOIL};

/// The volume of a fin of this cross-section relative to a square-edged one
/// (CrossSection.getRelativeVolume()).
[[nodiscard]] constexpr double relativeVolume(FinCrossSection crossSection) noexcept
{
    switch (crossSection)
    {
        case FinCrossSection::SQUARE:
            return 1.00;
        case FinCrossSection::ROUNDED:
            return 0.99;
        case FinCrossSection::AIRFOIL:
            return 0.85;
    }
    return 1.00;
}

/// The constant's name, e.g. "ROUNDED" (Java: name()).
[[nodiscard]] std::string_view finCrossSectionName(FinCrossSection crossSection) noexcept;

/// The .ork spelling the fin set saver writes in <crosssection>: the lower-cased name ("square",
/// "rounded", "airfoil").
[[nodiscard]] std::string_view orkName(FinCrossSection crossSection) noexcept;

/// The cross-section @p text names, matched as DocumentConfig.findEnum() does; nullopt for
/// anything else.
[[nodiscard]] std::optional<FinCrossSection> finCrossSectionFromOrkName(std::string_view text);

/// The translation key of the cross-section's name, e.g. "FinSet.CrossSection.SQUARE".
[[nodiscard]] std::string_view displayKey(FinCrossSection crossSection) noexcept;

/// The English name (Java: toString()): "Square", "Rounded", "Airfoil".
[[nodiscard]] std::string_view displayName(FinCrossSection crossSection) noexcept;

/// A set of identical planar fins around a body component (OpenRocket's FinSet), the base of the
/// trapezoidal, elliptical and freeform fin sets: 1 to 8 fins spread evenly around the parent's
/// axis from a base rotation, each standing on the parent's surface, with a thickness, a
/// cross-section, a cant angle (at most kMaxCantRadians either way), an optional fin tab (the part
/// of the fin inside the body) and optional fillets of a material of their own.
///
/// Frames: a fin's outline (getFinPoints()) is in the fin frame, x aft from the leading edge of
/// the root and y outwards from the parent's surface at that point (getFinFront(), in the
/// parent's frame); the tab's y values are negative. The root follows the parent's profile:
/// getRootPoints() is a straight line on a tube or a conical transition and a polyline of up to
/// kMaxRootDivisions segments on a curved transition or for a canted fin. The parent is any
/// SymmetricComponent (a body tube, a transition or a nose cone); a detached fin set has a flat
/// root at y = 0.
///
/// Mass: calculateCM() integrates the outline over the root (calculateCurveIntegral()) for the
/// planform area and its centroid, adds the tab (full thickness, no cross-section factor) and the
/// fillets (their own material), in OpenRocket's order of summation. The results are cached until
/// componentChanged() sees an aerodynamic or mass change. As in OpenRocket, a component that is
/// not in a Rocket receives no events (see RocketComponent::fireComponentChangeEvent()), so once
/// computed its cache is not cleared by its own setters; compute after setting up, or put it in a
/// rocket.
///
/// The fin count is the instance count; the radius method is always SURFACE with offset 0 (the
/// radius setters do nothing), and the angle method is stored but does not change the angles.
///
/// A new fin set's material and fillet material are the built-in bulk default, "Cardboard"
/// (ExternalComponent::defaultMaterial()); Java's constructor asks the application preferences
/// for the default material of the component's class for both, which applyDefaultMaterial()
/// completes for whoever creates a fin set for the user: once the application has run
/// loadDefaultComponentMaterials() (material/MaterialPreferences.h) that is "Balsa" for every fin
/// set.
///
/// Kept from OpenRocket, on purpose:
/// - getFinRotationTransformation() is the identity until setFinCount() changes the count (a new
///   fin set has three fins and no rotation increment).
/// - setTabHeight() and setTabLength() store a negative request as it is (the comparison with the
///   current value uses max(request, 0)).
/// - calculateFilletVolumeCentroid() takes the body radius of a root segment at the segment's x in
///   the fin frame, not in the parent's frame.
/// - getComponentBounds() adds the fin set's absolute x location (its first component location)
///   to the upper x bound only, and starts that bound at Double.MIN_VALUE.
/// - copyFrom() copies the fin count, the rotation increment, the angle offset, the cant, the
///   thickness, the cross-section and the tab height, length, offset method and position; it does
///   not copy the angle method, the base rotation matrix (which a single fin's CG is rotated by),
///   the stored tab offset or the fillet radius and material.
///
/// Deviations from OpenRocket:
/// - FinSet.CrossSection is the FinCrossSection enum above with free functions, aliased as
///   FinSet::CrossSection.
/// - getParentFrontRadius() and getParentTrailingRadius() return a std::optional (Java: a Double
///   or null); getMountPoints() without a parent returns no points (Java: null).
/// - A parent that is not a SymmetricComponent throws BugError where Java throws
///   ClassCastException (no component other than a body component accepts a fin set), and so do
///   setFilletMaterial() of a material that is not BULK (Java: IllegalArgumentException) and a
///   root for which no point can be made because of the division count a caller of
///   getRootPoints(int) asks for (a negative one; Java: an IndexOutOfBoundsException).
/// - A canted fin set, or one on a curved body, whose length is negative has a root of its two
///   ends, as it has on a simple body. OpenRocket makes no root point for it and fails with an
///   IndexOutOfBoundsException wherever the fin's outline or the rocket's bounds are asked for.
///   An .ork file can hold such a fin set (an elliptical one with a negative root chord, a
///   freeform outline that runs forwards) next to a shock cord whose automatic length asks
///   for the rocket's length while the file loads: the loader must not fail on it.
/// - calculateCurveIntegral(), translatePoints() and calculateFilletVolumeCentroid() are public
///   (Java: protected, used by the tests of the same package), and reverse() is static.
/// - splitFins() hands the replaced fin set back (see RocketComponent::splitInstances()).
/// - getPointDescr() takes any contiguous list (Java has an array and a List overload).
/// - The multi-edit config listeners are not ported (see RocketComponent), nor is slf4j logging.
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class FinSet : public ExternalComponent,
               public virtual AxialPositionable,
               public virtual BoxBounded,
               public virtual RingInstanceable,
               public InsideColorComponent
{
public:
    using CrossSection = FinCrossSection;

    using RocketComponent::getAxialOffset;
    using RocketComponent::getRadiusOffset;
    using RocketComponent::isCompatible;
    using RocketComponent::setOverrideMass;

    /// The largest cant of the fins either way, 15 degrees (MAX_CANT_RADIANS).
    static constexpr double kMaxCantRadians = 15.0 * std::numbers::pi / 180;

    /// The most segments getRootPoints() divides the root into (MAX_ROOT_DIVISIONS).
    static constexpr int kMaxRootDivisions = 100;
    /// The same for the low-resolution outlines of the 3D view (MAX_ROOT_DIVISIONS_LOW_RES).
    static constexpr int kMaxRootDivisionsLowRes = kMaxRootDivisions / 5;

    /// Java's FinSet.setOverrideMass() without an argument: an empty method that nothing calls.
    static void setOverrideMass() noexcept { }

    /// False: a fin set is placed relative to its parent, not after a sibling.
    [[nodiscard]] bool isAfter() const override;

    // ---- the fins

    /// The number of fins in the set.
    [[nodiscard]] int getFinCount() const noexcept { return m_finCount; }

    /// Sets the number of fins, limited to 1 ... 8, and fires AEROMASS_CHANGE; nothing happens
    /// when @p n is the current count.
    void setFinCount(int n);

    /// The rotation about the x axis from one fin to the next, 2 pi / fin count; the identity
    /// until setFinCount() changes the count (see the class comment).
    [[nodiscard]] const Transformation& getFinRotationTransformation() const noexcept
    {
        return m_finRotationIncrement;
    }

    /// The rotation of the first fin about the x axis, in radians (getAngleOffset()); zero is the
    /// positive y axis.
    [[nodiscard]] double getBaseRotation() const;

    /// setAngleOffset(@p r).
    void setBaseRotation(double r);

    /// The cant angle, in radians.
    [[nodiscard]] double getCantAngle() const noexcept { return m_cantRadians; }

    /// Sets the cant angle, limited to +-kMaxCantRadians, and fires AERODYNAMIC_CHANGE, unless it
    /// equals the current angle (MathUtil::equals).
    void setCantAngle(double newCantRadians);

    /// The rotation about the y axis by the cant angle (the identity for no cant); cached until
    /// the next aerodynamic or mass change.
    [[nodiscard]] Transformation getCantRotation() const;

    /// The thickness of the fins.
    [[nodiscard]] double getThickness() const noexcept { return m_thickness; }

    /// Sets the thickness (negative values become 0, Java's Math.max) and fires AEROMASS_CHANGE,
    /// unless it is exactly the current thickness.
    void setThickness(double r);

    [[nodiscard]] CrossSection getCrossSection() const noexcept { return m_crossSection; }

    /// Sets the cross-section and fires AEROMASS_CHANGE when it changes.
    void setCrossSection(CrossSection cs);

    // ---- the fin tab

    /// How far the tab reaches into the body from the root, at the lower of the two ends.
    [[nodiscard]] double getTabHeight() const noexcept { return m_tabHeight; }

    /// Sets the tab height; with @p validateTabHeight it is limited to getMaxTabHeight() (the
    /// tab may not pass the parent's axis). Fires MASS_CHANGE, unless the current height equals
    /// max(@p newTabHeight, 0) (MathUtil::equals).
    void setTabHeight(double newTabHeight, bool validateTabHeight = true);

    [[nodiscard]] double getTabLength() const noexcept { return m_tabLength; }

    /// Sets the tab length and, with @p updatePosition, recomputes the tab position from the
    /// stored offset. Fires MASS_CHANGE, unless the current length equals max(@p lengthRequest, 0)
    /// (MathUtil::equals).
    void setTabLength(double lengthRequest, bool updatePosition = true);

    /// Recomputes the tab position (its front edge from the front of the fin) from the stored
    /// offset, its method, the tab length and the fin's length.
    void updateTabPosition();

    /// Sets the tab offset for the current method, recomputes the tab position and fires
    /// MASS_CHANGE (always).
    void setTabOffset(double offsetRequest);

    /// The tab position expressed as an offset of @p method.
    [[nodiscard]] double getTabOffset(AxialMethod method) const;

    /// getTabOffset(getTabOffsetMethod()).
    [[nodiscard]] double getTabOffset() const;

    /// The position the stored offset gives under @p method.
    [[nodiscard]] double getTabPosition(AxialMethod method) const;

    [[nodiscard]] AxialMethod getTabOffsetMethod() const noexcept { return m_tabOffsetMethod; }

    /// Changes how the tab offset is described, keeping the tab where it is (the offset is
    /// recomputed from the position), and fires NONFUNCTIONAL_CHANGE.
    void setTabOffsetMethod(AxialMethod newPositionMethod);

    /// The tab's front edge, from the front of the fin.
    [[nodiscard]] double getTabFrontEdge() const noexcept { return m_tabPosition; }

    /// The tab's trailing edge, from the front of the fin.
    [[nodiscard]] double getTabTrailingEdge() const noexcept { return m_tabPosition + m_tabLength; }

    /// Moves a tab position outside 0 ... fin length onto the nearer end; fires nothing.
    void validateFinTabPosition();

    /// Shortens a tab that ends behind the fin so that it ends with it (not below 0); fires
    /// nothing.
    void validateFinTabLength();

    /// The largest tab height the parent allows: the smaller of its radii at the tab's two ends
    /// (MathUtil::min, which skips a NaN), or the largest double without a symmetric parent.
    [[nodiscard]] double getMaxTabHeight() const;

    /// The radius of @p parent at the front edge of the tab, the fin being where it is on its
    /// own parent; nullopt when @p parent is not a SymmetricComponent (or null).
    [[nodiscard]] std::optional<double> getParentFrontRadius(const RocketComponent* parent) const;

    /// getParentFrontRadius(getParent()).
    [[nodiscard]] std::optional<double> getParentFrontRadius() const;

    /// As getParentFrontRadius(), at the trailing edge of the tab.
    [[nodiscard]] std::optional<double> getParentTrailingRadius(
        const RocketComponent* parent) const;

    /// getParentTrailingRadius(getParent()).
    [[nodiscard]] std::optional<double> getParentTrailingRadius() const;

    /// Whether the tab's area (length times height) is below 1e-8 m^2; such a tab has no mass.
    [[nodiscard]] bool isTabTrivial() const;

    /// Whether the tab's height and length are both positive.
    [[nodiscard]] bool hasTab() const;

    /// Whether the fin has a tab that lies entirely ahead of or entirely behind the fin.
    [[nodiscard]] bool isTabBeyondFin() const;

    // ---- fillets

    [[nodiscard]] const Material& getFilletMaterial() const noexcept { return m_filletMaterial; }

    /// Sets the fillet material; when it differs (Material's ==), clears the preset and fires
    /// MASS_CHANGE.
    /// @throws BugError when @p mat is not a BULK material (Java: IllegalArgumentException).
    void setFilletMaterial(const Material& mat);

    [[nodiscard]] double getFilletRadius() const noexcept { return m_filletRadius; }

    /// Sets the fillet radius; clears the preset and fires MASS_CHANGE, unless it equals the
    /// current radius (MathUtil::equals).
    void setFilletRadius(double r);

    /// The material and the fillet material become the preferences' default for the component's
    /// class (see the class comment and ExternalComponent::applyDefaultMaterial()); no event. The
    /// cached area, volume and CG are cleared, so the mass getters follow the new materials at
    /// once (Java's constructor assigns them before anything is cached).
    void applyDefaultMaterial(const Preferences&     preferences,
                              const MaterialStorage& storage) override;

    /// The volume (as the weight) and centroid of the two fillets of one fin: circular concave
    /// fillets tangent to the fin and the body, summed over the root segments; the y of the
    /// centroid is a heuristic (body radius + fillet radius / 5). For a single fin the centroid
    /// is rotated by the angle offset, otherwise its y is 0. Zero without a fillet radius or a
    /// symmetric parent.
    [[nodiscard]] Coordinate calculateFilletVolumeCentroid() const;

    // ---- the parent body

    /// The radius of the parent at the front of the root, 0 without a parent (getFinFront().y).
    [[nodiscard]] double getBodyRadius() const;

    /// The front of the root in the parent's frame: (axial front, parent radius there); (0, 0)
    /// without a parent.
    /// @throws BugError when the parent is not a SymmetricComponent.
    [[nodiscard]] Coordinate getFinFront() const;

    /// Whether the root is a straight line: true unless the parent is a transition or nose cone
    /// that is not conical.
    [[nodiscard]] bool isRootStraight() const;

    // ---- the outlines

    /// The outline of one fin in the fin frame, from the leading edge of the root (the first
    /// point, at the origin unless the fin is canted) along the fin to the trailing edge of the
    /// root; z is 0.
    [[nodiscard]] virtual std::vector<Coordinate> getFinPoints() const = 0;

    /// The root, front to back, in the fin frame: the parent's profile under the fin, in at most
    /// @p maximumBodyDivisionCount segments (one for a tube or a conical transition, unless the
    /// fin is canted), with a point where the parent begins or ends under the fin. {(0, 0, 0)}
    /// without a parent.
    [[nodiscard]] std::vector<Coordinate> getRootPoints(int maximumBodyDivisionCount) const;

    /// getRootPoints(kMaxRootDivisions).
    [[nodiscard]] std::vector<Coordinate> getRootPoints() const;

    /// The parent's whole profile in the parent's frame, sampled as getRootPoints() samples it;
    /// empty without a parent (Java: null).
    [[nodiscard]] std::vector<Coordinate> getMountPoints() const;

    /// getFinPoints() followed by the root points from back to front: the closed outline.
    [[nodiscard]] std::vector<Coordinate> getFinPointsWithRoot() const;

    /// getFinPointsWithRoot() with a root of at most kMaxRootDivisionsLowRes segments (3D view).
    [[nodiscard]] std::vector<Coordinate> getFinPointsWithLowResRoot() const;

    /// The four corners of the tab in the fin frame: the front edge on the root, the front and
    /// trailing edges at the bottom, the trailing edge on the root. The y values are NaN without
    /// a parent.
    [[nodiscard]] std::vector<Coordinate> getTabPoints() const;

    /// getTabPoints() closed along the root under the tab, back to front; empty without a tab.
    [[nodiscard]] std::vector<Coordinate> getTabPointsWithRoot() const;

    /// getTabPointsWithRoot() over the low-resolution root; empty when the tab height or length
    /// equals 0 (MathUtil::equals).
    [[nodiscard]] std::vector<Coordinate> getTabPointsWithRootLowRes() const;

    /// One uninterrupted outline of the fin and its tab: the fin points, then the root from the
    /// back with the tab let in. getFinPointsWithRoot() without a tab, or with one that lies
    /// beyond the fin.
    [[nodiscard]] std::vector<Coordinate> generateContinuousFinAndTabShape() const;

    /// A copy of @p points moved by (@p xDelta, @p yDelta), as unweighted (x, y) coordinates.
    [[nodiscard]] static std::vector<Coordinate> translatePoints(std::span<const Coordinate> points,
                                                                 double xDelta, double yDelta);

    /// A copy of @p points with @p delta added to each (Coordinate::add, weights included).
    [[nodiscard]] static std::vector<Coordinate> translatePoints(std::span<const Coordinate> points,
                                                                 const Coordinate&           delta);

    /// @p source back to front.
    [[nodiscard]] static std::vector<Coordinate> reverse(std::span<const Coordinate> source);

    // ---- areas and mass

    /// The area under the piecewise line @p points (the integral of y over x, so a closed curve
    /// run clockwise gives its area) as the weight, with the centroid of that area as x and y.
    /// Segments whose area equals 0 (MathUtil::equals) are skipped; a negative total is made
    /// positive with the centroid's y negated, as OpenRocket does.
    [[nodiscard]] static Coordinate calculateCurveIntegral(std::span<const Coordinate> points);

    /// The area of one side of a single fin, without the tab.
    [[nodiscard]] double getPlanformArea() const;

    /// The span of a single fin: the distance from the root to the tip.
    [[nodiscard]] virtual double getSpan() const = 0;

    /// The mass of the fins, tabs and fillets (the weight of getComponentCG()).
    [[nodiscard]] double getComponentMass() const override;

    /// The volume of the fins (with the cross-section factor), tabs and fillets.
    [[nodiscard]] double getComponentVolume() const override;

    /// The CG with the mass as the weight. x is from the front of the root; y is from the
    /// parent's axis, 0 for more than one fin (they balance), and for a single fin the CG is
    /// rotated by the base rotation.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// An approximation: each fin as a rectangular thin plate of the planform area, the mean of
    /// the inertias about the two axes across the fin, moved to the parent's axis for more than
    /// one fin.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// An approximation: each fin as a rectangular thin plate, moved to the parent's axis for
    /// more than one fin.
    [[nodiscard]] double getRotationalUnitInertia() const override;

    // ---- bounds

    /// The box around one fin's outline, thickness / 2 either way in z (BoxBounded).
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    /// Two opposite corners of a box around the axis: from the smallest x of the outline to the
    /// largest plus the fin set's absolute x location, as wide and high as twice the largest y
    /// of the outline plus the parent's radius at its front (see the class comment).
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    // ---- RocketComponent

    /// False: nothing can be attached to a fin set.
    [[nodiscard]] bool allowsChildren() const override;

    /// False.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    // ---- AxialPositionable (RocketComponent's, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }
    void                 setAxialMethod(AxialMethod newAxialMethod) override;
    [[nodiscard]] double getAxialOffset() const override;
    void                 setAxialOffset(double newOffset) override;

    // ---- AnglePositionable

    /// The rotation of the first fin about the x axis, in radians.
    [[nodiscard]] double getAngleOffset() const override;

    /// Sets the rotation of the first fin, reduced to -pi ... pi, and fires AEROMASS_CHANGE,
    /// unless it equals the current one (MathUtil::equals).
    void setAngleOffset(double angle) override;

    [[nodiscard]] AngleMethod getAngleMethod() const override;

    /// Stores the method and fires AEROMASS_CHANGE (always).
    void setAngleMethod(AngleMethod newMethod) override;

    // ---- RadiusPositionable: fins stand on the parent's surface

    /// 0.
    [[nodiscard]] double getBoundingRadius() const override;
    /// 0.
    [[nodiscard]] double getRadiusOffset() const override;
    /// Does nothing.
    void setRadiusOffset(double radius) override;
    /// SURFACE.
    [[nodiscard]] RadiusMethod getRadiusMethod() const override;
    /// Does nothing.
    void setRadiusMethod(RadiusMethod method) override;
    /// Does nothing.
    void setRadius(RadiusMethod method, double radius) override;

    // ---- Instanceable / RingInstanceable

    /// getFinCount().
    [[nodiscard]] int getInstanceCount() const override;

    /// setFinCount(@p newCount).
    void setInstanceCount(int newCount) override;

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// Where each fin stands relative to the fin set's reference point: (0, body radius, 0),
    /// canted about the middle of the root and rotated to the fin's angle.
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// "<count>-fin-ring".
    [[nodiscard]] std::string getPatternName() const override;

    /// 2 pi / fin count.
    [[nodiscard]] double getInstanceAngleIncrement() const override;

    /// The angle of each fin, base rotation included, reduced to 0 ... 2 pi.
    [[nodiscard]] std::vector<double> getInstanceAngles() const override;

    /// Splits the fin set into single fins (splitInstances()). The result owns the fin set this
    /// was called on once it has been replaced.
    [[nodiscard]] SplitResult splitFins(bool freezeRocket = true);

    // ---- debug

    /// "<indent>    >> <name>: <n> points\n" and one line "<indent>      ....[ i] (x, y)\n" per
    /// point, the index two wide and x and y as Java's "%6.4g".
    [[nodiscard]] static std::string getPointDescr(std::span<const Coordinate> points,
                                                   std::string_view name, std::string_view indent);

    /// RocketComponent's dump followed by the fin points, with a parent the parent's profile
    /// ("Body Points") and the root points, and for a tab that is not trivial its dimensions and
    /// points.
    // NOLINTNEXTLINE(misc-override-with-different-visibility): public in Java's FinSet too
    [[nodiscard]] std::string toDebugDetail(
        std::source_location where = std::source_location::current()) const override;

protected:
    using RocketComponent::setAxialOffset;

    /// A fin set of three fins positioned BOTTOM, 3 mm thick, square edges, no cant, no tab
    /// height (tab length 0.05 m, MIDDLE), no fillets; drawn in front of the bodies in the 2D
    /// views (display order 4).
    FinSet();

    /// setFinCount() without the event, for a subclass's constructor; true when the count
    /// changed.
    bool assignFinCount(int n);

    /// The end of every getFinPoints(): when the root has more than one point, moves the first
    /// and the last point of @p finPoints onto the ends of the root (which a canted fin's root
    /// leaves the fin's own ends by).
    void alignEndsWithRoot(std::vector<Coordinate>& finPoints) const;

    /// The parent as a SymmetricComponent; nullptr without a parent.
    /// @throws BugError when the parent is not a SymmetricComponent (Java: ClassCastException).
    [[nodiscard]] const SymmetricComponent* symmetricParent() const;

    /// Clears the cached area, volume, CG and cant rotation on an aerodynamic or mass change.
    void componentChanged(const ComponentChangeEvent& event) override;

    /// Copies the fin properties of @p source (see the class comment for which), which must be a
    /// FinSet, then ExternalComponent's and RocketComponent's.
    /// @throws BugError when @p source is not a FinSet (Java: ClassCastException), and as
    ///         RocketComponent::copyFrom().
    std::vector<std::unique_ptr<RocketComponent>> copyFrom(const RocketComponent& source) override;

    /// The thickness of the fins (Java: protected field thickness).
    double m_thickness{0.003};

private:
    /// A tab of less area than this has no mass (minimumTabArea).
    static constexpr double kMinimumTabArea = 1.0e-8;

    /// The parent's profile from @p xStart to @p xEnd (parent frame), moved by (@p xOffset,
    /// @p yOffset): one segment, or for a canted fin or a curved parent one per 2.5 mm up to
    /// @p maximumBodyDivisionCount, lowered by getFinCantYOffset(), with a point at the parent's
    /// front or end when the range crosses it.
    [[nodiscard]] std::vector<Coordinate> getMountPoints(double xStart, double xEnd, double xOffset,
                                                         double yOffset,
                                                         int    maximumBodyDivisionCount) const;

    /// getMountPoints() with kMaxRootDivisions.
    [[nodiscard]] std::vector<Coordinate> getMountPoints(double xStart, double xEnd, double xOffset,
                                                         double yOffset) const;

    /// Inserts (@p x, @p y) into @p points, which are sorted by x, unless a point already has
    /// that x (within MathUtil::kEpsilon).
    static void insertPointByX(std::vector<Coordinate>& points, double x, double y);

    /// How far the root of a canted fin drops at @p xCurr so that it still touches the round
    /// body (0 or negative), for a root from @p xStart to @p xEnd.
    [[nodiscard]] double getFinCantYOffset(double xStart, double xEnd, double xCurr) const;

    /// The tab corners (getTabPoints()) closed along @p rootPoints, the root points under the
    /// tab, back to front.
    [[nodiscard]] std::vector<Coordinate> generateTabPointsWithRoot(
        std::vector<Coordinate> rootPoints) const;

    /// @p c1 followed by @p c2 reversed: two forward curves as one closed curve.
    [[nodiscard]] static std::vector<Coordinate> combineCurves(std::span<const Coordinate> c1,
                                                               std::span<const Coordinate> c2);

    /// @p fromRoot (fin frame) with y from the parent's axis.
    [[nodiscard]] std::vector<Coordinate> translateToCenterline(
        std::span<const Coordinate> fromRoot) const;

    /// The cross-section area (both sides, as the weight) and y centroid of a fillet of
    /// @p filletRadius on a body of @p bodyRadius.
    [[nodiscard]] static Coordinate calculateFilletCrossSection(double filletRadius,
                                                                double bodyRadius);

    /// The planform area (as the weight) and centroid of one fin's tab, y from the parent's
    /// axis; zero for a trivial tab or without a symmetric parent.
    [[nodiscard]] Coordinate calculateTabCentroid() const;

    /// The planform area (as the weight) and centroid of one fin, y from the parent's axis.
    [[nodiscard]] Coordinate calculateSinglePlanformCentroid() const;

    /// Fills the cached area, volume and CG.
    void calculateCM() const;

    /// The rotation of the first fin about the x axis (Java: baseRotation).
    Transformation m_baseRotation{Transformation::kIdentity};
    /// The rotation from one fin to the next (Java: finRotationIncrement).
    Transformation m_finRotationIncrement{Transformation::kIdentity};
    /// The cached cant rotation; nullopt until asked for (Java: null).
    mutable std::optional<Transformation> m_cantRotation;
    /// The cached CG with the mass; NaN until calculated.
    mutable Coordinate m_centerOfMass{Coordinate::kNaN};
    Material           m_filletMaterial;
    double             m_firstFinOffsetRadians{0};
    double             m_cantRadians{0};
    double             m_tabHeight{0};
    double             m_tabLength{0.05};
    /// The tab's front edge from the front of the fin, whatever the offset method.
    double m_tabPosition{0.0};
    double m_tabOffset{0.0};
    double m_filletRadius{0};
    /// The cached planform area of one side of one fin, without the tab; NaN until calculated.
    mutable double m_singlePlanformArea{std::numeric_limits<double>::quiet_NaN()};
    /// The cached volume of the whole set; NaN until calculated.
    mutable double m_totalVolume{std::numeric_limits<double>::quiet_NaN()};
    int            m_finCount{3};
    AngleMethod    m_angleMethod{AngleMethod::RELATIVE};
    CrossSection   m_crossSection{CrossSection::SQUARE};
    AxialMethod    m_tabOffsetMethod{AxialMethod::MIDDLE};
};

}  // namespace QtRocket
