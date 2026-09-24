#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class AxialStage;
class ComponentAssembly;
class ComponentPreset;
class Rocket;

/// The base of every rocket component (OpenRocket's RocketComponent): the component tree, the
/// properties every component has, positioning and instancing, mass/CG/CD overrides and the
/// change events.
///
/// Ownership: a component owns its children (std::unique_ptr) and knows its parent through a
/// non-owning pointer. addChild() takes ownership; removeChild() hands it back, so the caller
/// (undo, drag and drop, the clipboard) decides whether the component lives on: dropping the
/// returned pointer destroys the component (removeChild() and splitInstances() are
/// [[nodiscard]], so that a Java-style call that ignores the result does not compile silently
/// into a dangling reference). A component without a parent is detached (or the root);
/// OpenRocket's REMOVED sentinel is not ported: findComponent() returns nullptr instead.
///
/// Changes: setters fire a ComponentChangeEvent through fireComponentChangeEvent(), which goes to
/// the root when it is a Rocket (see Rocket::fireComponentChangeEvent() for the algorithm) and is
/// dropped for a detached component or while setBypassChangeEvent(true) is in force (the loader
/// uses that). Cached absolute locations are cleared in componentChanged() only, as in
/// OpenRocket: a component moved while events are off (a new Rocket, a detached subtree) keeps
/// its cached getComponentLocations() until the next event reaches it.
///
/// Copying: copyWithOriginalId() makes a deep copy of the subtree that keeps the ids (undo,
/// simulations), copyWithNewIds() one with fresh ids (copy/paste; Java: copy()). Both are built
/// on cloneShallow(), which every concrete class implements as
/// `return std::make_unique<ThisClass>(*this);`: the copy constructor copies the class's fields
/// (Java's clone() plus the field fix-ups of the class's copyWithOriginalID() override: a member
/// that refers back to its component, such as a MotorConfigurationSet, is rebuilt there), and
/// RocketComponent's own copy constructor leaves out the parent, the children, the cached
/// locations and the event bypass. So a component class declares no copy or move operations and
/// no destructor, and the implicit (public) copy constructor does the work; a class whose members
/// need more writes a public copy constructor (a class with virtual bases initialises them from
/// the source too, as Rocket does). Copy and move assignment are deleted throughout: a component
/// is never assigned. The copies of the overriddenBy pointers are remapped to the
/// copied components; one that points outside the copied subtree becomes null (Java keeps a
/// reference to the original component there).
///
/// Subclasses: a concrete class implements the pure virtuals below (kind(), cloneShallow(), the
/// mass/CG/inertia API, allowsChildren(), isCompatible(), getComponentBounds(), isAerodynamic(),
/// isMassive()); getComponentName() defaults to displayName(kind()). A class that overrides one
/// of several overloads of the same name (getAxialOffset(), setAxialOffset(), getRadiusOffset(),
/// fireComponentChangeEvent(), isCompatible()) adds `using RocketComponent::<name>;` to keep the
/// others visible (and -Woverloaded-virtual quiet). Interfaces (AxialPositionable, Instanceable,
/// ...) are inherited `public virtual`; see AxialPositionable.
///
/// Deviations from OpenRocket:
/// - Java's SafetyMutex (concurrent access checks) is dropped: a rocket is single-threaded (plan,
///   section 3.1).
/// - Java's Invalidator is replaced by ownership: a component that Rocket::loadFrom() replaces is
///   destroyed after the change event has been delivered, so nothing can use it afterwards, and
///   checkState() calls are not ported.
/// - The multi-edit config listeners (addConfigListener(), removeConfigListener(),
///   clearConfigListeners(), getConfigListeners() and the loops over them in every setter) are not
///   ported, by decision: the GUI applies an edit to each selected component inside one undo step
///   with the rocket frozen. setBypassChangeEvent() stays, for the loader.
/// - getRocket(), getStage() and getAssembly() throw BugError where Java throws
///   IllegalStateException; findRocket(), findStage() and findAssembly() return nullptr instead.
///   addChild() and removeChild() skip the stage map, and setAfter() counts every sibling as
///   active, when the tree is not in a Rocket (Java's getRocket() would throw there).
/// - moveChild() checks the index before it takes the child out (Java loses the child on a bad
///   index); splitInstances() hands the replaced original back (SplitResult) instead of dropping
///   it; indices are std::size_t and getChildPosition() gives nullopt for Java's -1.
/// - addChild() checks its argument before it takes ownership, so a rejected component (null,
///   already in a tree, the root of this tree, incompatible) stays with the caller.
/// - The overriddenBy pointers never outlive their target. Java's removeChild() clears only the
///   pointers of the removed subtree that refer to the parent or the parent's overrider, and keeps
///   stale references elsewhere (OpenRocket's tree-order walk, see
///   updateChildrenMassOverriddenBy(), also hands an overrider to its later siblings). Here
///   removeChild() also clears every pointer left in the tree that refers into the removed
///   subtree, and every pointer of the removed subtree that refers outside it; pointers within the
///   removed subtree are kept, as in Java. Otherwise destroying the removed component would leave
///   them dangling.
/// - removeChild() drops the removed stages from the Rocket's stage map by identity whatever the
///   StageTracking, so that the map never holds a destroyed stage (see StageTracking).
/// - The subtree() iteration also fails fast on a change of any child list it is walking, in a
///   detached tree or a rocket with events disabled too (Java checks the rocket's tree
///   modification id, and each ArrayList iterator its own list).
/// - The component name is stored only when the user sets one: getName() gives
///   getComponentName() otherwise, since a C++ constructor cannot call the subclass's override the
///   way Java's does. The result is the same, getComponentName() being constant per class.
/// - accept(RocketComponentVisitor) is not ported (no visitor exists in QtRocket), nor
///   updateChildren() (private and unused in Java).
///
/// Deferred to aero/ (they need AerodynamicCalculator, BarrowmanCalculator and
/// FlightConditions, and rocket/ does not include aero/):
/// - getComponentCD(AOA, theta, mach, rollRate): the component's CD from a Barrowman force
///   analysis of the selected configuration.
/// - The CD refresh in getOverrideCD() and setCDOverridden(false): Java recomputes overrideCD as
///   getComponentCD(0, 0, preferences.getDefaultMach(), 0) whenever the CD is not overridden.
///   Here the stored value is returned and kept; it only matters to the GUI, since the saver
///   writes the override CD only when it is overridden.
///
/// Deferred to the preset group (they need the ComponentPreset type):
/// - loadPreset(preset, params...) and loadFromPreset(preset, params...) (the base version copies
///   ComponentPreset.LENGTH into the length), and getPresetType(). The preset pointer is kept
///   (getPresetComponent(), clearPreset(), setIgnorePresetClearing(), and the protected
///   setPresetComponent() for loadPreset()).
///
/// Deferred to rocket-config (they need FlightConfiguration or MotorMount/MotorConfiguration):
/// - toDebugMountNode() and its call in toDebugTreeNode() for an active motor mount.
/// - setAfter()'s activeness test of the previous sibling reads
///   Rocket::isComponentActiveInSelectedConfiguration(), an interim hook (see there).
class RocketComponent
{
public:
    /// Whether addChild() and removeChild() update the Rocket's stage map when the component is a
    /// stage or holds stages (Java's boolean trackStage; SKIP is for moving detached copies
    /// around, as the clipboard does). With SKIP, addChild() does not register an added stage,
    /// and removeChild() does not forget the removed stages by number (Java's forgetStage()).
    /// Deviation: removeChild() still drops every map entry that holds a removed stage, in both
    /// modes: Java's SKIP leaves the entry behind, a stale but live object there, which would
    /// dangle here once the caller destroys the removed component.
    enum class StageTracking
    {
        TRACK,
        SKIP,
    };

    template <class Component>
    class BasicIterator;
    template <class Component>
    class BasicRange;

    /// A pre-order (depth-first) iterator over a subtree (Java: iterator()).
    using Iterator      = BasicIterator<RocketComponent>;
    using ConstIterator = BasicIterator<const RocketComponent>;

    /// The result of splitInstances(): the single-instance components now in the tree, and the
    /// original component when it was taken out of the tree (it had more than one instance).
    /// Dropping it destroys the original, the object splitInstances() was called on.
    struct [[nodiscard]] SplitResult
    {
        std::vector<RocketComponent*>    components;
        std::unique_ptr<RocketComponent> original;
    };

    RocketComponent& operator=(const RocketComponent&) = delete;
    RocketComponent(RocketComponent&&)                 = delete;
    RocketComponent& operator=(RocketComponent&&)      = delete;
    virtual ~RocketComponent();

    // ================================================================ methods to implement

    /// The concrete kind of this component (replaces Java's getClass()).
    [[nodiscard]] virtual ComponentKind kind() const noexcept = 0;

    /// The static name of the component type, the default name of a new component: "Body Tube",
    /// "Stage", ... It must not depend on the component's state. Defaults to
    /// displayName(kind()).
    [[nodiscard]] virtual std::string getComponentName() const;

    /// The mass of the component, regardless of any mass override.
    [[nodiscard]] virtual double getComponentMass() const = 0;

    /// The CG of the component with its mass as the weight, regardless of any override.
    [[nodiscard]] virtual Coordinate getComponentCG() const = 0;

    /// The longitudinal (about the y or z axis) moment of inertia for a mass of 1 kg, about the
    /// non-overridden CG.
    [[nodiscard]] virtual double getLongitudinalUnitInertia() const = 0;

    /// The rotational (about the x axis) moment of inertia for a mass of 1 kg, about the
    /// non-overridden CG.
    [[nodiscard]] virtual double getRotationalUnitInertia() const = 0;

    /// Whether this component can have children at all; true exactly when isCompatible() is true
    /// for some kind.
    [[nodiscard]] virtual bool allowsChildren() const = 0;

    /// Whether a component of @p kind can be added as a child now (the answer may depend on the
    /// component's state). addChild() enforces it.
    [[nodiscard]] virtual bool isCompatible(ComponentKind kind) const = 0;

    /// Coordinates whose convex hull encloses this component alone (not its children), in its
    /// own frame.
    [[nodiscard]] virtual std::vector<Coordinate> getComponentBounds() const = 0;

    /// Whether the component may have an aerodynamic effect on the rocket.
    [[nodiscard]] virtual bool isAerodynamic() const = 0;

    /// Whether the component may have an effect on the rocket's mass.
    [[nodiscard]] virtual bool isMassive() const = 0;

    /// isCompatible(component.kind()) (Java: isCompatible(RocketComponent)).
    [[nodiscard]] bool isCompatible(const RocketComponent& component) const
    {
        return isCompatible(component.kind());
    }

    // ================================================================= overridable behaviour

    /// Whether the component is positioned AFTER its previous sibling (overridden by classes that
    /// are always, or never, positioned that way).
    [[nodiscard]] virtual bool isAfter() const;

    /// Whether the component is axially symmetric (true unless overridden).
    [[nodiscard]] virtual bool isAxisymmetric() const;

    /// All materials of this component; empty when it has none (Java: null).
    [[nodiscard]] virtual std::vector<Material> getAllMaterials() const;

    /// How many instances this component represents on its own (1 unless overridden).
    [[nodiscard]] virtual int getInstanceCount() const;

    /// Sets the instance count; does nothing unless overridden (Java logs a warning).
    virtual void setInstanceCount(int count);

    /// The user-visible name, or getComponentName() when none is set (Java: toString()).
    [[nodiscard]] std::string toString() const;

    // ==================================================================== common properties

    /// The realistic appearance, or nullopt for the material's default.
    [[nodiscard]] const std::optional<Appearance>& getAppearance() const noexcept
    {
        return m_appearance;
    }

    /// Sets the realistic appearance (nullopt for the default) and fires NONFUNCTIONAL_CHANGE.
    /// (Java also subscribes to the decal image; see InsideColorComponentHandler.)
    void setAppearance(std::optional<Appearance> appearance);

    /// The colour in 2D figures, or nullopt for the default.
    [[nodiscard]] const std::optional<Color>& getColor() const noexcept { return m_color; }

    /// Sets the 2D figure colour; fires NONFUNCTIONAL_CHANGE when it changes.
    void setColor(std::optional<Color> color);

    /// The line style in 2D figures, or nullopt for the default.
    [[nodiscard]] std::optional<LineStyle> getLineStyle() const noexcept { return m_lineStyle; }

    /// Sets the line style; fires NONFUNCTIONAL_CHANGE when it changes.
    void setLineStyle(std::optional<LineStyle> style);

    /// The user-given name, or getComponentName() when the user gave none.
    [[nodiscard]] std::string getName() const;

    /// Sets the name; a blank one (only Java regex \s characters: space, tab, newline, vertical
    /// tab, form feed, carriage return) restores getComponentName(). Fires TREE_CHANGE for a
    /// stage (the component tree shows stage names) and NONFUNCTIONAL_CHANGE otherwise, when the
    /// name changes.
    void setName(std::string_view name);

    /// The comment, possibly several lines separated by '\n'.
    [[nodiscard]] const std::string& getComment() const noexcept { return m_comment; }

    /// Sets the comment; fires NONFUNCTIONAL_CHANGE when it changes.
    void setComment(std::string_view comment);

    /// The unique id.
    [[nodiscard]] const Uuid& getId() const noexcept { return m_id; }

    /// Sets the id. Normally ids are assigned automatically; the loader restores them.
    void setId(const Uuid& newId) noexcept { m_id = newId; }

    /// Parses @p newId as java.util.UUID.fromString() does (Uuid::javaFromString(), which also
    /// takes shortened groups such as "1-2-3-4-5") and sets it (Java: setID(String)); a malformed
    /// id fails with ErrorCode::PARSE and changes nothing (Java: IllegalArgumentException).
    [[nodiscard]] Result<void> setId(std::string_view newId);

    /// "name/xxxxxxxx", the name and the first eight characters of the id.
    [[nodiscard]] std::string getDebugName() const;

    /// The preset this component is based on, or nullptr. Presets are owned by their database.
    [[nodiscard]] const ComponentPreset* getPresetComponent() const noexcept
    {
        return m_presetComponent;
    }

    /// Forgets the preset (the component's values stay) and fires NONFUNCTIONAL_CHANGE, unless
    /// there is none or setIgnorePresetClearing(true) is in force.
    void clearPreset();

    /// While true, clearPreset() keeps the preset.
    void setIgnorePresetClearing(bool ignorePresetClearing) noexcept
    {
        m_ignorePresetClearing = ignorePresetClearing;
    }

    /// Whether the component is drawn; it does not affect simulations.
    [[nodiscard]] bool isVisible() const noexcept { return m_isVisible; }

    /// Sets the visibility and fires GRAPHIC_CHANGE (always, as Java).
    void setVisible(bool value);

    /// The drawing order in the 2D side view: higher values are drawn in front (default 100).
    [[nodiscard]] int getDisplayOrderSide() const noexcept { return m_displayOrderSide; }
    void setDisplayOrderSide(int displayOrder) noexcept { m_displayOrderSide = displayOrder; }

    /// The drawing order in the 2D back view (default 100).
    [[nodiscard]] int getDisplayOrderBack() const noexcept { return m_displayOrderBack; }
    void setDisplayOrderBack(int displayOrder) noexcept { m_displayOrderBack = displayOrder; }

    /// Whether the component is a motor mount that holds motors (false unless overridden).
    [[nodiscard]] virtual bool isMotorMount() const;

    // ============================================================================ overrides

    /// The override mass, whether or not it is in use; while the mass is not overridden this is
    /// the component mass (the stored value is refreshed as Java does).
    [[nodiscard]] double getOverrideMass() const;

    /// Sets the override mass (negative values become 0) without switching the override on;
    /// fires MASS_CHANGE when it changes and the mass is overridden.
    void setOverrideMass(double m);

    /// Whether this component's mass is overridden (a parent's override is not considered).
    [[nodiscard]] bool isMassOverridden() const noexcept { return m_massOverridden; }

    /// Switches the mass override on or off (off stores the component mass as the override
    /// mass), updates the children's getMassOverriddenBy() and fires MASS_CHANGE, when it
    /// changes.
    void setMassOverridden(bool overridden);

    /// The override CG: the component CG with x replaced by the override x (refreshed to the
    /// component CG's x while the CG is not overridden).
    [[nodiscard]] Coordinate getOverrideCG() const;

    /// The x of the override CG (refreshed while the CG is not overridden).
    [[nodiscard]] double getOverrideCGX() const;

    /// Sets the override CG x; fires MASS_CHANGE when the CG is overridden, else
    /// NONFUNCTIONAL_CHANGE, when it changes.
    void setOverrideCGX(double x);

    [[nodiscard]] bool isCGOverridden() const noexcept { return m_cgOverridden; }

    /// Switches the CG override on or off (off stores the component CG's x), updates the
    /// children's getCGOverriddenBy() and fires MASS_CHANGE, when it changes.
    void setCGOverridden(bool overridden);

    /// The override CD, whether or not it is in use. See the class comment: Java refreshes it
    /// from an aerodynamic analysis while the CD is not overridden; that is deferred to aero/.
    [[nodiscard]] double getOverrideCD() const noexcept { return m_overrideCD; }

    /// Sets the override CD; when the CD is overridden fires AERODYNAMIC_CHANGE (after
    /// overrideSubcomponentsCD(true) when it overrides the subcomponents too), else
    /// NONFUNCTIONAL_CHANGE, when it changes.
    void setOverrideCD(double x);

    [[nodiscard]] bool isCDOverridden() const noexcept { return m_cdOverridden; }

    /// Switches the CD override on or off, updates the children's getCDOverriddenBy() and fires
    /// AERODYNAMIC_CHANGE, when it changes.
    void setCDOverridden(bool overridden);

    /// Whether an ancestor overrides the CD of its subcomponents.
    [[nodiscard]] bool isCDOverriddenByAncestor() const;

    /// Whether the mass override also covers the subcomponents. A subclass that always or never
    /// overrides its subcomponents overrides this and isOverrideSubcomponentsEnabled().
    [[nodiscard]] virtual bool isSubcomponentsOverriddenMass() const;
    /// Sets it; updates the children's getMassOverriddenBy() and fires MASS_CHANGE |
    /// TREE_CHANGE_CHILDREN, when it changes.
    virtual void setSubcomponentsOverriddenMass(bool override);

    /// Whether the CG override also covers the subcomponents.
    [[nodiscard]] virtual bool isSubcomponentsOverriddenCG() const;
    /// Sets it; updates the children's getCGOverriddenBy() and fires MASS_CHANGE |
    /// TREE_CHANGE_CHILDREN, when it changes.
    virtual void setSubcomponentsOverriddenCG(bool override);

    /// Whether the CD override also covers the subcomponents.
    [[nodiscard]] virtual bool isSubcomponentsOverriddenCD() const;
    /// Sets it; updates the children's getCDOverriddenBy(), walks overrideSubcomponentsCD() and
    /// fires AERODYNAMIC_CHANGE | TREE_CHANGE_CHILDREN, when it changes.
    virtual void setSubcomponentsOverriddenCD(bool override);

    /// Sets all three "override subcomponents" flags (for files written by OpenRocket 15.03).
    virtual void setSubcomponentsOverridden(bool override);

    /// Whether the "override subcomponents" option can be used: true while any of the mass, CG
    /// or CD is overridden, unless overridden.
    [[nodiscard]] virtual bool isOverrideSubcomponentsEnabled() const;

    /// The (super-)parent that overrides this component's mass, or nullptr.
    [[nodiscard]] RocketComponent* getMassOverriddenBy() const noexcept
    {
        return m_massOverriddenBy;
    }
    /// The (super-)parent that overrides this component's CG, or nullptr.
    [[nodiscard]] RocketComponent* getCGOverriddenBy() const noexcept { return m_cgOverriddenBy; }
    /// The (super-)parent that overrides this component's CD, or nullptr.
    [[nodiscard]] RocketComponent* getCDOverriddenBy() const noexcept { return m_cdOverriddenBy; }

    // ============================================================================= position

    /// The characteristic length of the component (a body tube's length, a fin's root chord),
    /// used to position it and its siblings. A class with a settable length defines the setter.
    [[nodiscard]] virtual double getLength() const;

    /// How the component is positioned relative to its parent.
    [[nodiscard]] AxialMethod getAxialMethod() const noexcept { return m_axialMethod; }

    /// Changes how the position is described, keeping the physical position: the offset is
    /// recomputed for @p newAxialMethod. Fires nothing (it is not a physical change).
    virtual void setAxialMethod(AxialMethod newAxialMethod);

    /// The offset of the current position expressed in @p asMethod, without changing anything:
    /// ABSOLUTE is the x of the first absolute location, the others use the parent's length (0
    /// under a Rocket).
    [[nodiscard]] virtual double getAxialOffset(AxialMethod asMethod) const;

    /// The stored offset for getAxialMethod().
    [[nodiscard]] virtual double getAxialOffset() const;

    /// The x of the component's front in its parent's frame (getPosition().x).
    [[nodiscard]] double getAxialFront() const noexcept { return m_position.x; }

    /// Sets the offset for the current method and fires AEROMASS_CHANGE.
    virtual void setAxialOffset(double newOffset);

    /// The radial offset (0 unless overridden).
    [[nodiscard]] virtual double getRadiusOffset() const;

    /// The current radius expressed as an offset of @p method.
    [[nodiscard]] double getRadiusOffset(RadiusMethod method) const;

    /// How the radius is given (COAXIAL unless overridden).
    [[nodiscard]] virtual RadiusMethod getRadiusMethod() const;

    /// The angular offset in radians (0 unless overridden).
    [[nodiscard]] virtual double getAngleOffset() const;

    /// The position of this component relative to its parent: the front center; (0, 0, 0) for the
    /// root.
    [[nodiscard]] const Coordinate& getPosition() const noexcept { return m_position; }

    /// Places the component directly after its previous active sibling (at 0 when it is the
    /// first child, or no earlier sibling is active), and switches it to AFTER with offset 0.
    /// Does nothing without a parent. Public because assemblies call it on their children (Java:
    /// protected, reachable in the package).
    void setAfter();

    // ============================================================================ instances

    /// The location of each instance relative to the parent: getPosition() plus each instance
    /// offset (overridable). Always getInstanceCount() long.
    [[nodiscard]] virtual std::vector<Coordinate> getInstanceLocations() const;

    /// The location of each instance relative to this component's reference point; {(0,0,0)}
    /// unless overridden. Always getInstanceCount() long.
    [[nodiscard]] virtual std::vector<Coordinate> getInstanceOffsets() const;

    /// The rotation of each instance about the x axis, in radians; zeros unless overridden.
    [[nodiscard]] virtual std::vector<double> getInstanceAngles() const;

    /// The absolute location (from the rocket's origin) of every instance, counting the parents'
    /// instances too: a two-instance rail button in a three-instance pod set gives six. Each
    /// parent location p (with its angles a) and own instance location l gives
    /// p + rotation(a about this position).transform(l); the result for parent instance i and own
    /// instance j is at index i + parentCount * j. Cached until componentChanged().
    [[nodiscard]] std::vector<Coordinate> getComponentLocations() const;

    /// The absolute rotation of every instance (x = about the x axis; y and z are 0), counting the
    /// parents' instances: the parent's angles plus each own angle, in the same order as
    /// getComponentLocations(). OpenRocket's rotations follow the left-hand rule. Cached until
    /// componentChanged().
    [[nodiscard]] std::vector<Coordinate> getComponentAngles() const;

    /// @p c (relative to this component's reference point) in absolute coordinates, once for
    /// every absolute instance location.
    [[nodiscard]] std::vector<Coordinate> toAbsolute(const Coordinate& c) const;

    /// @p c (relative to this component's first instance) in the frame of @p dest, once for each
    /// of @p dest's absolute instance locations (rotations are not supported).
    [[nodiscard]] std::vector<Coordinate> toRelative(const Coordinate&      c,
                                                     const RocketComponent& dest) const;

    // ========================================================================= mass and CG

    /// The override mass while the mass is overridden, else getComponentMass().
    [[nodiscard]] double getMass() const;

    /// The mass of this component and all its subcomponents (stopping at an override that covers
    /// the subcomponents).
    [[nodiscard]] double getSectionMass() const;

    /// The CG with the (possibly overridden) mass as the weight; the CG and the mass are
    /// overridden separately.
    [[nodiscard]] Coordinate getCG() const;

    /// getLongitudinalUnitInertia() times getMass().
    [[nodiscard]] double getLongitudinalInertia() const;

    /// getRotationalUnitInertia() times getMass().
    [[nodiscard]] double getRotationalInertia() const;

    // ================================================================================= tree

    /// Adds @p component (which must be detached and compatible) as the last child and returns
    /// it. Children inherit this component's overriddenBy pointers; a stage is registered with
    /// the Rocket (unless @p tracking is SKIP or the tree is not in a Rocket). Fires TREE_CHANGE,
    /// with MASS_CHANGE / AERODYNAMIC_CHANGE when the added subtree holds massive / aerodynamic
    /// components, and then calls childAdded() on this component.
    /// @throws BugError when @p component is null, already has a parent, equals() the root of
    ///         this tree (a cycle) or is not compatible (Java: IllegalArgumentException /
    ///         IllegalStateException). The component is checked before it is taken, so on an
    ///         error it stays in the caller's pointer.
    template <std::derived_from<RocketComponent> T>
    T& addChild(std::unique_ptr<T>&& component, StageTracking tracking = StageTracking::TRACK)
    {
        checkAddable(component.get());
        T* added = component.get();
        insertChild(std::unique_ptr<RocketComponent>{std::move(component)}, m_children.size(),
                    tracking);
        return *added;
    }

    /// As addChild(), inserting at @p index (0 to getChildCount()).
    /// @throws BugError as addChild(), and when @p index is out of range (the component then
    ///         stays with the caller too). A bool index is rejected at compile time, so that
    ///         Java's addChild(c, trackStage) cannot silently become an index: pass a
    ///         StageTracking.
    template <std::derived_from<RocketComponent> T, std::integral Index>
        requires(!std::same_as<Index, bool>)
    T& addChild(std::unique_ptr<T>&& component, Index index,
                StageTracking tracking = StageTracking::TRACK)
    {
        checkAddable(component.get());
        const std::size_t position = checkedIndex(index);
        T*                added    = component.get();
        insertChild(std::unique_ptr<RocketComponent>{std::move(component)}, position, tracking);
        return *added;
    }

    /// Removes the child @p index and returns it, detached (Java: removeChild(int)); dropping
    /// the result destroys it. A template so that removeChild(0) is not ambiguous with the
    /// pointer overload (0 is also a null pointer constant).
    /// @throws BugError when @p index is out of range.
    template <std::integral Index>
        requires(!std::same_as<Index, bool>)
    [[nodiscard]] std::unique_ptr<RocketComponent> removeChild(
        Index index, StageTracking tracking = StageTracking::TRACK)
    {
        if (std::cmp_less(index, 0) || std::cmp_greater_equal(index, m_children.size()))
        {
            bug("child index out of range");
        }
        return removeChild(m_children[static_cast<std::size_t>(index)].get(), tracking);
    }

    /// Removes @p component when it is a child of this one and returns it, detached; nullptr (and
    /// nothing done) otherwise. Dropping the result destroys the component (write
    /// `static_cast<void>(parent.removeChild(c))` to delete it on purpose). Clears every
    /// overriddenBy pointer between the removed subtree and the rest of the tree, in either
    /// direction (see the class comment), drops the removed stages from the Rocket's stage map
    /// (see StageTracking), fires the same event as addChild() and updates the bounds.
    [[nodiscard]] std::unique_ptr<RocketComponent> removeChild(
        const RocketComponent* component, StageTracking tracking = StageTracking::TRACK);

    /// Moves the child @p component to @p index (counted after its removal); nothing happens when
    /// it is not a child. Updates the bounds and fires as addChild().
    /// @throws BugError when @p index is out of range.
    void moveChild(const RocketComponent* component, std::size_t index);

    [[nodiscard]] std::size_t getChildCount() const noexcept { return m_children.size(); }

    /// The child @p n.
    /// @throws BugError when @p n is out of range.
    [[nodiscard]] RocketComponent&       getChild(std::size_t n);
    [[nodiscard]] const RocketComponent& getChild(std::size_t n) const;

    /// The direct children, in order (a new list).
    [[nodiscard]] std::vector<RocketComponent*>       getChildren();
    [[nodiscard]] std::vector<const RocketComponent*> getChildren() const;

    /// All descendants in tree order (pre-order: each child followed by its own descendants),
    /// this component excluded.
    [[nodiscard]] std::vector<RocketComponent*>       getAllChildren();
    [[nodiscard]] std::vector<const RocketComponent*> getAllChildren() const;

    /// Whether @p component is a descendant of this one.
    [[nodiscard]] bool containsChild(const RocketComponent* component) const;

    /// The index of @p child among the children, or nullopt when it is not a child (Java: -1).
    [[nodiscard]] std::optional<std::size_t> getChildPosition(
        const RocketComponent* child) const noexcept;

    /// The parent, or nullptr for a root.
    [[nodiscard]] RocketComponent*       getParent() noexcept { return m_parent; }
    [[nodiscard]] const RocketComponent* getParent() const noexcept { return m_parent; }

    /// Every ancestor, the parent first.
    [[nodiscard]] std::vector<RocketComponent*>       getParents();
    [[nodiscard]] std::vector<const RocketComponent*> getParents() const;

    /// Whether any ancestor of @p component is in @p components.
    [[nodiscard]] static bool listContainsParent(std::span<const RocketComponent* const> components,
                                                 const RocketComponent&                  component);

    /// Whether every component of @p components has this component's class (an empty list
    /// gives true).
    [[nodiscard]] bool checkAllClassesEqual(
        std::span<const RocketComponent* const> components) const;

    /// Whether this component is an ancestor of @p testComponent.
    [[nodiscard]] bool isAncestor(const RocketComponent& testComponent) const noexcept;

    /// The root of the tree (this component when it has no parent).
    [[nodiscard]] RocketComponent&       getRoot() noexcept;
    [[nodiscard]] const RocketComponent& getRoot() const noexcept;

    /// The Rocket at the root of the tree.
    /// @throws BugError when the root is not a Rocket (Java: IllegalStateException).
    [[nodiscard]] Rocket&       getRocket();
    [[nodiscard]] const Rocket& getRocket() const;

    /// The Rocket at the root of the tree, or nullptr when the root is not a Rocket.
    [[nodiscard]] Rocket*       findRocket() noexcept;
    [[nodiscard]] const Rocket* findRocket() const noexcept;

    /// The nearest AxialStage at or above this component.
    /// @throws BugError when there is none (Java: IllegalStateException).
    [[nodiscard]] AxialStage&       getStage();
    [[nodiscard]] const AxialStage& getStage() const;

    /// The nearest AxialStage at or above this component, or nullptr.
    [[nodiscard]] AxialStage*       findStage() noexcept;
    [[nodiscard]] const AxialStage* findStage() const noexcept;

    /// Every stage below this component, in tree order.
    [[nodiscard]] std::vector<AxialStage*>       getSubStages();
    [[nodiscard]] std::vector<const AxialStage*> getSubStages() const;

    /// The innermost ComponentAssembly (pod set, stage, ...) at or above this component.
    /// @throws BugError when there is none (Java: IllegalStateException).
    [[nodiscard]] ComponentAssembly&       getAssembly();
    [[nodiscard]] const ComponentAssembly& getAssembly() const;

    /// The innermost ComponentAssembly at or above this component, or nullptr.
    [[nodiscard]] ComponentAssembly*       findAssembly() noexcept;
    [[nodiscard]] const ComponentAssembly* findAssembly() const noexcept;

    /// Every assembly below this component, in tree order.
    [[nodiscard]] std::vector<ComponentAssembly*>       getAllChildAssemblies();
    [[nodiscard]] std::vector<const ComponentAssembly*> getAllChildAssemblies() const;

    /// The assemblies among the direct children.
    [[nodiscard]] std::vector<ComponentAssembly*>       getDirectChildAssemblies();
    [[nodiscard]] std::vector<const ComponentAssembly*> getDirectChildAssemblies() const;

    /// Every stage below this component, in tree order (the same list as getSubStages()).
    [[nodiscard]] std::vector<AxialStage*>       getAllChildStages();
    [[nodiscard]] std::vector<const AxialStage*> getAllChildStages() const;

    /// The stages below this component that are not below another stage.
    [[nodiscard]] std::vector<AxialStage*>       getTopLevelChildStages();
    [[nodiscard]] std::vector<const AxialStage*> getTopLevelChildStages() const;

    /// Every assembly above this component, the nearest first.
    [[nodiscard]] std::vector<RocketComponent*>       getParentAssemblies();
    [[nodiscard]] std::vector<const RocketComponent*> getParentAssemblies() const;

    /// The number of the stage this component belongs to (stages count from zero).
    /// @throws BugError when there is no stage above (see getStage()).
    [[nodiscard]] virtual int getStageNumber() const;

    /// The component with the id @p idToFind in this subtree (this component included), or
    /// nullptr (Java: REMOVED).
    [[nodiscard]] RocketComponent*       findComponent(const Uuid& idToFind) noexcept;
    [[nodiscard]] const RocketComponent* findComponent(const Uuid& idToFind) const noexcept;

    /// The next component in tree order: the first child, else the next sibling of the nearest
    /// ancestor that has one; nullptr at the end.
    [[nodiscard]] RocketComponent*       getNextComponent() noexcept;
    [[nodiscard]] const RocketComponent* getNextComponent() const noexcept;

    /// The previous component in tree order: the last descendant of the previous sibling, else
    /// the parent; nullptr for a root.
    /// @throws BugError when the parent does not hold this component (a broken tree).
    [[nodiscard]] RocketComponent*       getPreviousComponent();
    [[nodiscard]] const RocketComponent* getPreviousComponent() const;

    /// Splits a multi-instance component into single-instance copies, one per instance, in its
    /// place: each copy has an instance count of 1, its angle offset (for an AnglePositionable)
    /// advanced by 2 pi i / count, its name suffixed " #i" and the override mass divided by the
    /// count. The rocket is frozen meanwhile when @p freezeRocket is true. A single-instance
    /// component is left alone. Then fires TREE_CHANGE from this component (which does nothing
    /// once it has been taken out of the tree, as in Java). When the component was split, the
    /// result owns it: dropping the result destroys this component.
    /// @throws BugError when the component is not in a Rocket.
    [[nodiscard]] SplitResult splitInstances(bool freezeRocket = true);

    // ============================================================================ iteration

    /// This subtree in pre-order, this component first when @p includeSelf (Java: iterator()).
    /// The iteration fails fast: once the rocket's tree changes, or the child list of any
    /// component the iteration is inside of (in any tree, events enabled or not), using the
    /// iterator throws BugError (Java: IllegalStateException "Rocket modified while being
    /// iterated", or ConcurrentModificationException). The component the iteration started from
    /// must outlive the iterator.
    [[nodiscard]] BasicRange<RocketComponent>       subtree(bool includeSelf = true);
    [[nodiscard]] BasicRange<const RocketComponent> subtree(bool includeSelf = true) const;

    /// Calls @p visitor for every component of this subtree in pre-order (this component first
    /// when @p includeSelf). The visitor is taken by value, as std::for_each does, and the same
    /// object visits the whole subtree. It must not change the tree.
    template <std::invocable<RocketComponent&> Visitor>
    void forEach(Visitor visitor, bool includeSelf = true)
    {
        forEachImpl(*this, visitor, includeSelf);
    }
    template <std::invocable<const RocketComponent&> Visitor>
    void forEach(Visitor visitor, bool includeSelf = true) const
    {
        forEachImpl(*this, visitor, includeSelf);
    }

    // ============================================================================== events

    /// Adds a listener to the Rocket at the root (events of every component go to it).
    /// @throws BugError when the root is not a Rocket.
    virtual ComponentChangeSignal::Connection addComponentChangeListener(
        ComponentChangeSignal::Slot slot);

    /// Removes a listener; true when it was connected (Java: removeComponentChangeListener(),
    /// which does nothing for a detached component: a connection can always be removed).
    static bool removeComponentChangeListener(const ComponentChangeSignal::Connection& connection);

    /// addComponentChangeListener() (Java: ChangeSource.addChangeListener(), which wraps the
    /// StateChangeListener in a ComponentChangeAdapter; a slot takes the event directly).
    ComponentChangeSignal::Connection addChangeListener(ComponentChangeSignal::Slot slot);

    /// removeComponentChangeListener().
    static bool removeChangeListener(const ComponentChangeSignal::Connection& connection);

    /// Fires a change event of @p type from this component (a mask of the
    /// ComponentChangeEvent::k...Change constants).
    void fireComponentChangeEvent(int type);

    /// While true, this component's events are dropped (the loader sets it; OpenRocket's config
    /// listeners set it too, which are not ported).
    void setBypassChangeEvent(bool newValue) noexcept { m_bypassComponentChangeEvent = newValue; }

    [[nodiscard]] bool isBypassComponentChangeEvent() const noexcept
    {
        return m_bypassComponentChangeEvent;
    }

    // ============================================================================= copying

    /// A deep copy of this subtree that keeps every id (undo, simulations; Java:
    /// copyWithOriginalID()). The copy has no parent and fires no events; see the class comment.
    /// Rocket overrides it to rebuild its stage map.
    [[nodiscard]] virtual std::unique_ptr<RocketComponent> copyWithOriginalId() const;

    /// A deep copy of this subtree in which every component has a new id (copy/paste; Java:
    /// copy()).
    [[nodiscard]] std::unique_ptr<RocketComponent> copyWithNewIds() const;

    // ============================================================================ identity

    /// Java's equals(): the same class and the same id. Pointer identity is the C++ equality.
    [[nodiscard]] bool equals(const RocketComponent& other) const noexcept;

    /// Java's hashCode(): the id's java.util.UUID.hashCode().
    [[nodiscard]] std::int32_t hashCode() const noexcept { return m_id.hashCode(); }

    /// Checks that this component is in its parent's children and that its children point back
    /// to it.
    /// @throws BugError when the structure is inconsistent.
    void checkComponentStructure() const;

    // =============================================================================== debug

    /// "ClassName@address[\"name\"; child; child]" for this subtree.
    [[nodiscard]] std::string toDebugString() const;

    /// "name<ClassName>(xxxxxxxx)" with the first eight characters of the id.
    [[nodiscard]] std::string toDebugName() const;

    /// A table of this subtree: name, length, offset and absolute location of every component
    /// (and of every instance of an instanced one).
    [[nodiscard]] std::string toDebugTree() const;

    /// Appends this component's toDebugTreeNode() and its children's, each level indented by
    /// "....".
    void toDebugTreeHelper(std::string& buffer, const std::string& indent) const;

    /// Appends this component's line(s) of toDebugTree().
    /// @throws BugError for a multi-instance component that is not an Instanceable (a developer
    ///         error, as in Java).
    virtual void toDebugTreeNode(std::string& buffer, const std::string& indent) const;

    // ========================================================================= maintenance

    /// Recomputes the position from the stored method and offset (setAxialOffset(method,
    /// offset)); componentChanged() calls it. Public, as in Rocket and FreeformFinSet (Java's
    /// base version is protected), so that every override can be.
    virtual void update();

    /// Updates cached bounds after the children change; does nothing unless overridden
    /// (assemblies recompute their length). Public, as ComponentAssembly's is in Java.
    virtual void updateBounds();

protected:
    /// A component positioned by @p axialMethod, with a new random id and the default name.
    explicit RocketComponent(AxialMethod axialMethod);

    /// Java's clone(): copies every field except the tree links (no parent, no children), the
    /// cached locations and the event bypass (cleared). The overriddenBy pointers are copied as
    /// they are and remapped by copyWithOriginalId().
    RocketComponent(const RocketComponent& other);

    /// A copy of this component alone, of its concrete class, made with its copy constructor:
    /// `return std::make_unique<ThisClass>(*this);` (see the class comment).
    [[nodiscard]] virtual std::unique_ptr<RocketComponent> cloneShallow() const = 0;

    /// Called on every component of the rocket when any component fires an event (not while the
    /// rocket is frozen). Clears the cached locations and calls update(); an override must call
    /// this version.
    virtual void componentChanged(const ComponentChangeEvent& event);

    /// Called at the very end of every addChild(), on the new parent, after the child has been
    /// linked and the change event fired (Java: an override of addChild(c, index, trackStage)
    /// that calls super.addChild() first, as BodyTube's, which gives an added TubeFinSet with
    /// no thickness yet the tube's). Does nothing unless overridden.
    virtual void childAdded(RocketComponent& child);

    /// Clears the cached absolute locations and angles.
    void clearCoordinateCaches() const noexcept;

    /// Sets the position from @p requestedOffset in @p requestedMethod without firing: relative
    /// to nothing without a parent, to the parent's absolute location for ABSOLUTE, through
    /// setAfter() for a component positioned AFTER, else with the enum's arithmetic. Positions
    /// within 1e-6 of zero snap to zero.
    /// @throws BugError when the position comes out NaN.
    void setAxialOffset(AxialMethod requestedMethod, double requestedOffset);

    /// Fires @p event: hands it to the root when this component has a parent and is not
    /// bypassing events (a detached tree has no listeners). Rocket overrides it.
    virtual void fireComponentChangeEvent(const ComponentChangeEvent& event);

    /// Descends the tree for a CD override of the subcomponents (Java's package-private
    /// overrideSubcomponentsCD()); it changes no state, as in Java, where it only walks the
    /// children whose inherited override differs from @p override.
    void overrideSubcomponentsCD(bool override);

    /// Loads the fields of @p source into this root component in place (undo/redo, fin set
    /// conversion): the children become copies (with the original ids) of @p source's children,
    /// and length, axial method, position, colour, line style, override mass/CG and the three
    /// "override subcomponents" flags, name, comment, id, display orders and the inside
    /// appearance are copied, as in Java (the offset, CD override, appearance, visibility and
    /// preset are not). Returns the previous children, which the caller keeps alive until the
    /// change event has been delivered (Java returns the components to invalidate). A subclass
    /// copies its own fields and then calls this version.
    /// @throws BugError when this component has a parent.
    virtual std::vector<std::unique_ptr<RocketComponent>> copyFrom(const RocketComponent& source);

    /// Stores the preset without firing (for loadPreset(), deferred to the preset group).
    void setPresetComponent(const ComponentPreset* preset) noexcept { m_presetComponent = preset; }

    /// A detailed multi-line dump: " >> Dumping Detailed Information from: <caller>" and the
    /// name, class, position, offset, method and length.
    [[nodiscard]] virtual std::string toDebugDetail(
        std::source_location where = std::source_location::current()) const;

    // ---- helpers for subclasses

    /// Adds two opposite corners of a box around the centerline, from x = @p xMin to @p xMax and
    /// 2 @p r wide and high.
    static void addBoundingBox(std::vector<Coordinate>& bounds, double xMin, double xMax, double r);

    /// Adds four points at x = @p x around the centerline at radius @p r, 90 degrees apart.
    static void addBound(std::vector<Coordinate>& bounds, double x, double r);

    /// The CG of a ring from @p x1 to @p x2, with its mass as the weight.
    [[nodiscard]] static Coordinate ringCG(double outerRadius, double innerRadius, double x1,
                                           double x2, double density);

    /// The volume of a ring: ringMass() at density 1.
    [[nodiscard]] static double ringVolume(double outerRadius, double innerRadius, double length);

    /// pi * max(outer^2 - inner^2, 0) * length * density.
    [[nodiscard]] static double ringMass(double outerRadius, double innerRadius, double length,
                                         double density);

    /// (3 (inner^2 + outer^2) + length^2) / 12, about the ring's CG.
    [[nodiscard]] static double ringLongitudinalUnitInertia(double outerRadius, double innerRadius,
                                                            double length);

    /// (inner^2 + outer^2) / 2.
    [[nodiscard]] static double ringRotationalUnitInertia(double outerRadius, double innerRadius);

    // The fields are ordered by alignment to keep the object compact.

    /// The characteristic length (Java: protected field length).
    double m_length{0.0};
    /// The offset for m_axialMethod (Java: axialOffset).
    double m_axialOffset{0.0};
    /// The override mass (Java: overrideMass); mutable because getOverrideMass() refreshes it.
    mutable double m_overrideMass{0.0};
    /// The parent, or nullptr (Java: parent).
    RocketComponent* m_parent{nullptr};
    /// The children (Java: children).
    std::vector<std::unique_ptr<RocketComponent>> m_children;
    /// The position relative to the parent; (0, 0, 0) for the root (Java: position).
    Coordinate m_position;
    /// The user-given name, empty for the default (Java: name, see the class comment).
    std::string m_name;
    /// How the component is positioned (Java: axialMethod).
    AxialMethod m_axialMethod;
    /// Whether the mass is overridden (Java: massOverridden).
    bool m_massOverridden{false};

private:
    friend class Rocket;

    /// Validates an addChild() index.
    template <std::integral Index>
    [[nodiscard]] std::size_t checkedIndex(Index index) const
    {
        if (std::cmp_less(index, 0) || std::cmp_greater(index, m_children.size()))
        {
            bug("child index out of range");
        }
        return static_cast<std::size_t>(index);
    }

    /// The recursion of forEach(), for both constnesses.
    template <class Component, class Visitor>
    static void forEachImpl(Component& component, Visitor& visitor, bool includeSelf)
    {
        if (includeSelf)
        {
            std::invoke(visitor, component);
        }
        for (const auto& child : component.m_children)
        {
            Component& next = *child;
            forEachImpl(next, visitor, true);
        }
    }

    /// getNextComponent() and getPreviousComponent() for both constnesses (defined in the .cpp).
    template <class Component>
    [[nodiscard]] static Component* nextComponentOf(Component& component) noexcept;
    template <class Component>
    [[nodiscard]] static Component* previousComponentOf(Component& component);

    /// The argument checks of addChild(), made before it takes ownership.
    /// @throws BugError when @p component is null, already has a parent, equals() the root of
    ///         this tree or is not compatible.
    void checkAddable(const RocketComponent* component) const;

    /// The body of every addChild(), once checkAddable() has passed.
    void insertChild(std::unique_ptr<RocketComponent> component, std::size_t index,
                     StageTracking tracking);

    /// Clears the overriddenBy pointers between @p removed (just detached from this component)
    /// and the tree it left, in both directions.
    void clearOverriddenByAcross(RocketComponent& removed);

    /// Fires TREE_CHANGE plus AERODYNAMIC_CHANGE / MASS_CHANGE for @p component's subtree.
    void fireAddRemoveEvent(const RocketComponent& component);

    void updateChildrenMassOverriddenBy();
    void updateChildrenCGOverriddenBy();
    void updateChildrenCDOverriddenBy();

    /// Draws a new random id.
    void newId() { m_id = Uuid::random(); }

    /// Copies this subtree, recording original -> copy in @p copies.
    [[nodiscard]] std::unique_ptr<RocketComponent> copySubtree(
        std::vector<std::pair<const RocketComponent*, RocketComponent*>>& copies) const;

    /// Points every overriddenBy of this subtree at the copy of its target in @p copies, or at
    /// nothing when the target was not copied.
    void remapOverriddenBy(
        const std::vector<std::pair<const RocketComponent*, RocketComponent*>>& copies);

    /// Bumped whenever the child list changes (add, remove, move, copyFrom()), for the
    /// fail-fast iteration.
    std::uint64_t                                  m_childListModCount{0};
    mutable double                                 m_overrideCGX{0.0};
    double                                         m_overrideCD{0.0};
    RocketComponent*                               m_massOverriddenBy{nullptr};
    RocketComponent*                               m_cgOverriddenBy{nullptr};
    RocketComponent*                               m_cdOverriddenBy{nullptr};
    const ComponentPreset*                         m_presetComponent{nullptr};
    Uuid                                           m_id;
    std::string                                    m_comment;
    mutable std::optional<std::vector<Coordinate>> m_cachedComponentLocations;
    mutable std::optional<std::vector<Coordinate>> m_cachedComponentAngles;
    std::optional<Appearance>                      m_appearance;
    int                                            m_displayOrderSide{100};
    int                                            m_displayOrderBack{100};
    std::optional<LineStyle>                       m_lineStyle;
    std::optional<Color>                           m_color;
    bool                                           m_cgOverridden{false};
    bool                                           m_overrideSubcomponentsMass{false};
    bool                                           m_overrideSubcomponentsCG{false};
    bool                                           m_overrideSubcomponentsCD{false};
    bool                                           m_cdOverridden{false};
    bool                                           m_ignorePresetClearing{false};
    bool                                           m_bypassComponentChangeEvent{false};
    bool                                           m_isVisible{true};
};

/// The pre-order iterator of RocketComponent::subtree(), for RocketComponent and
/// const RocketComponent. A default-constructed iterator is the end. It remembers the rocket's
/// tree modification id and the child list modification counts of the components it is inside
/// of, and throws BugError when it is used after any of them changed.
template <class Component>
class RocketComponent::BasicIterator
{
public:
    // NOLINTBEGIN(readability-identifier-naming): the names std::iterator_traits requires
    using value_type        = std::remove_const_t<Component>;
    using difference_type   = std::ptrdiff_t;
    using reference         = Component&;
    using pointer           = Component*;
    using iterator_category = std::forward_iterator_tag;
    // NOLINTEND(readability-identifier-naming)

    /// The end iterator.
    BasicIterator() = default;

    /// An iterator at the first component of @p start's subtree: @p start itself when
    /// @p includeSelf, else its first child (or the end).
    BasicIterator(Component& start, bool includeSelf);

    [[nodiscard]] reference operator*() const;
    [[nodiscard]] pointer   operator->() const;
    BasicIterator&          operator++();
    BasicIterator           operator++(int);

    [[nodiscard]] bool operator==(const BasicIterator& other) const noexcept
    {
        return m_current == other.m_current;
    }

private:
    /// A component whose children are being visited: the index of the next child, and the
    /// component's child list modification count when the visit began.
    struct Level
    {
        Component*    parent{nullptr};
        std::size_t   next{0};
        std::uint64_t modCount{0};
    };

    /// Makes @p component the current one.
    void setCurrent(Component* component) noexcept;

    /// @throws BugError when the rocket's tree, or the child list of a component on the stack or
    ///         of the current one, changed since the iterator reached it.
    void checkTree() const;

    Component* m_current{nullptr};
    /// The current component's child list modification count when it became current.
    std::uint64_t m_currentModCount{0};
    /// The components whose children are being visited, outermost first.
    std::vector<Level> m_stack;
    const Rocket*      m_rocket{nullptr};
    ModId              m_treeModId{ModId::invalid()};
};

/// The range RocketComponent::subtree() returns.
template <class Component>
class RocketComponent::BasicRange
{
public:
    BasicRange(Component& start, bool includeSelf) noexcept
      : m_start(&start), m_includeSelf(includeSelf)
    {
    }

    [[nodiscard]] BasicIterator<Component> begin() const
    {
        return BasicIterator<Component>{*m_start, m_includeSelf};
    }
    [[nodiscard]] static BasicIterator<Component> end() noexcept { return {}; }

private:
    Component* m_start;
    bool       m_includeSelf;
};

/// Moves ownership of @p component to a std::unique_ptr<T> when it is a T; otherwise returns
/// nullptr and leaves @p component untouched. For copies: componentCast<AxialStage>(
/// stage.copyWithNewIds()).
template <class T>
    requires std::derived_from<T, RocketComponent>
[[nodiscard]] std::unique_ptr<T> componentCast(std::unique_ptr<RocketComponent>& component) noexcept
{
    if (dynamic_cast<T*>(component.get()) == nullptr)
    {
        return nullptr;
    }
    return std::unique_ptr<T>{dynamic_cast<T*>(component.release())};
}

/// componentCast() of a temporary; a component of another class is destroyed.
template <class T>
    requires std::derived_from<T, RocketComponent>
[[nodiscard]] std::unique_ptr<T> componentCast(
    std::unique_ptr<RocketComponent>&& component) noexcept
{
    std::unique_ptr<RocketComponent> owned = std::move(component);
    return componentCast<T>(owned);
}

}  // namespace QtRocket
