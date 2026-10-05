#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class FlightConfiguration;
class RocketComponent;

/// The calculation objects of a Barrowman calculator: one RocketComponentCalc for every
/// aerodynamic component and every assembly of a rocket (OpenRocket's
/// `Map<RocketComponent, RocketComponentCalc> calcMap` with its ensureCalcMap() and
/// buildCalcMap(), which BarrowmanStabilityCalculator and BarrowmanDragCalculator each have a
/// copy of; each of the two calculators holds a map of its own here too).
///
/// The map is built once, from every component of the configuration's rocket, active or not
/// (FlightConfiguration::getAllComponents()), with CalcFactory::create(); it then stays as it is
/// until clear(), whatever happens to the rocket, with one exception: the calculation of a
/// component that was renamed is made anew (see "Renamed components"). The calculators clear it
/// in voidAerodynamicCache(), which BarrowmanCalculator calls when the rocket's aerodynamic or
/// tree modification id has changed.
///
/// Pointers: the keys are the addresses of the components, non-owning (Java's map keeps its
/// components alive). A key is only compared, never dereferenced, so a component may leave its
/// rocket and be destroyed while the map still has its entry. The calculations are another
/// matter: a RailButtonCalc reads its button at every call. A calculation is therefore handed
/// out only for the very component it was made from: get() takes the entry at the component's
/// address and checks it against the component's id and kind. The keys and calculations are
/// dropped by clear(), by build() and by the destructor.
///
/// Component objects replaced by equal ones: Java's map compares components with equals(), by
/// class and id, so it also finds the calculation of a component for a copy of it with the same
/// id. That matters where a rocket gets new component objects while its modification ids stay
/// what they were: Rocket::loadFrom() (an undo or redo to a state whose aerodynamic and tree ids
/// the calculator saw last, for example after a change of mass only) and a
/// Rocket::copyWithOriginalId() used with the same calculator. get() covers it: when the map has
/// no entry at the component's address but knows its id and kind under another address, the map
/// is rebuilt from the component's own tree and the new calculation is returned. A component
/// whose id the map does not know has no calculation, as in Java (one that joined the rocket
/// after the map was built).
///
/// Renamed components: FinSetCalc and TubeFinSetCalc make their geometry warnings at
/// construction, with a snapshot of the component's id and name as the source (see
/// MessageSource), where Java's warning keeps the component and prints its current name. A
/// rename changes neither the aerodynamic nor the tree modification id of the rocket, so the
/// calculators do not void their cache for it. ensureBuilt() covers it: every entry remembers
/// the name its calculation was made with, and when the rocket's modification id
/// (Rocket::getModId(), which every change event renews) is not the one the names were last
/// compared at, the calculation of each component of the configuration's rocket whose name has
/// changed is replaced by a new one. get() does not compare names: every public method of the
/// calculators calls ensureBuilt() first. A rename made while the rocket's events are disabled
/// is seen once they are enabled again.
///
/// Deviations from OpenRocket:
/// - build() is all or nothing: when a calculation's constructor throws (BugError), the map is
///   left as it was (Java leaves the calculations made so far in a map it counts as built).
/// - When a component object has been replaced by an equal one, every calculation is made anew,
///   from the components as they are then (Java goes on with the calculations made from the
///   replaced objects, which it keeps alive).
/// - Java keeps the calculation of a renamed component, whose warnings read the component's
///   current name; here that calculation is made anew at the next ensureBuilt(), from the
///   component as it is then (its geometry is the one the old calculation had, unless the
///   rocket was also changed behind the calculator's back, without the cache being voided).
/// - A map that was moved from is not built and is empty: the next ensureBuilt() builds it.
class ComponentCalcMap
{
public:
    ComponentCalcMap()                                   = default;
    ComponentCalcMap(const ComponentCalcMap&)            = delete;
    ComponentCalcMap& operator=(const ComponentCalcMap&) = delete;
    /// Takes the calculations of @p other, which is left cleared (not built, empty).
    ComponentCalcMap(ComponentCalcMap&& other) noexcept;
    ComponentCalcMap& operator=(ComponentCalcMap&& other) noexcept;
    ~ComponentCalcMap() = default;

    /// Whether the map has been built since it was made, cleared or moved from (Java:
    /// calcMap != null).
    [[nodiscard]] bool isBuilt() const noexcept { return m_built; }

    /// Builds the map unless it is built (Java: ensureCalcMap()). A built map gets a new
    /// calculation for each component of @p configuration's rocket that was renamed since its
    /// calculation was made (see the class comment, "Renamed components").
    /// @throws BugError when a calculation cannot be made (see build()); the map, or the entry
    ///         of the renamed component, is then unchanged.
    void ensureBuilt(const FlightConfiguration& configuration);

    /// Makes a calculation for every component of @p configuration's rocket that is aerodynamic
    /// or an assembly, replacing what the map held (Java: buildCalcMap()).
    /// @throws BugError when a calculation cannot be made (see CalcFactory::create() and the
    ///         constructors of the calculations); the map is then unchanged.
    void build(const FlightConfiguration& configuration);

    /// Drops every calculation and every key; the map is not built afterwards (Java:
    /// calcMap = null).
    void clear() noexcept;

    /// The number of calculations.
    [[nodiscard]] std::size_t size() const noexcept { return m_calcs.size(); }

    /// The calculation of @p component, or nullptr when the map has none (Java: calcMap.get()):
    /// always for a component that is neither aerodynamic nor an assembly, and for one the map
    /// was not built with. See the class comment for a component object that replaced the one
    /// the map was built with. The pointer is valid until the next call of get(), ensureBuilt(),
    /// build() or clear().
    /// @throws BugError when the map must be rebuilt and a calculation cannot be made.
    [[nodiscard]] RocketComponentCalc* get(const RocketComponent& component);

private:
    /// A calculation, the identity of the component it was made from and the name the
    /// component had then (the one in the sources of the calculation's warnings).
    struct Entry
    {
        Uuid                                 id;
        ComponentKind                        kind;
        std::string                          name;
        std::unique_ptr<RocketComponentCalc> calc;
    };

    using Calcs = std::unordered_map<const RocketComponent*, Entry>;

    /// Whether @p entry was made from a component with the id and kind of @p component.
    [[nodiscard]] static bool isFor(const Entry& entry, const RocketComponent& component) noexcept;

    /// Replaces the map by the calculations of @p components (those that have one).
    void rebuild(std::span<const RocketComponent* const> components);

    /// Makes the calculation of every component of @p configuration's rocket anew whose name is
    /// not the one its calculation was made with.
    void refreshRenamed(const FlightConfiguration& configuration);

    Calcs m_calcs;
    bool  m_built{false};
    /// The modification id of the rocket at which the names of the entries were last those of
    /// its components: at build() and at the last ensureBuilt().
    ModId m_namesModId{ModId::invalid()};
};

}  // namespace QtRocket
