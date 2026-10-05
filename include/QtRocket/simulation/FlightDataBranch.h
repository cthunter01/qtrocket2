#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/simulation/DataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class RocketComponent;

/// One branch of flight data (OpenRocket's simulation/FlightDataBranch): the time series of one
/// flight (the columns and rows of DataBranch, see there for the two-phase rows) together with
/// the flight events that happened during it, in the order they were handled. A simulation has
/// one branch for the sustainer and one for every stage that separates.
///
/// A branch is filled while it is mutable (addPoint(), setValue(), addEvent()) and then made
/// immutable with immute(); after that those three throw BugError. A FlightDataBranch always
/// has at least one type.
///
/// The events hold non-owning pointers to components of the simulated rocket (see FlightEvent);
/// a branch that outlives the simulation is kept in a FlightData, which keeps that rocket alive.
///
/// The events are handed out as they are stored: getEvents() is a reference to the branch's
/// list and getFirstEvent(), getLastEvent() and findEvent() are pointers into it. They are
/// valid while the branch lives and until its events change (addEvent(), an assignment to the
/// branch): whoever adds events while walking them, or keeps an event longer, copies it first
/// (an event is a value; the copy is the same event, with the same id).
///
/// Deviations from OpenRocket:
/// - getEvents() gives the list read-only (Java: a copy of the list, which protects the list
///   from the caller, as const does here), and getFirstEvent(), getLastEvent() and findEvent()
///   give a pointer to the stored event, null for Java's null (Java: the event object).
/// - getDataIndexOfTime() gives nullopt for Java's -1.
/// - Java's private constructor with a source component id is not needed: clone() copies the
///   object.
/// - The branch of a separated stage takes an event of its parent only when the event still
///   has its source pointer (an event read from a file, which knows its source by id only, is
///   not copied; Java has the component there).
/// As in Java, setOptimumAltitude() and setTimeToOptimumAltitude() neither check mutability
/// nor draw a modification id, and clone() copies neither the separation time nor the
/// immutability.
class FlightDataBranch : public DataBranch<FlightDataType>
{
public:
    /// A branch named @p name with columns for @p types, in that order, and no source
    /// component.
    /// @throws BugError when @p types is empty, holds a null pointer or holds a type twice
    FlightDataBranch(std::string name, std::span<const FlightDataType* const> types);

    /// The same with the types written in place:
    /// FlightDataBranch("b", {timeType, altitudeType}).
    FlightDataBranch(std::string                                                         name,
                     std::initializer_list<std::reference_wrapper<const FlightDataType>> types);

    /// A branch for the flight of @p srcComponent (the stage it describes; null: none), whose id
    /// it keeps.
    /// @throws BugError when @p types is empty, holds a null pointer or holds a type twice
    FlightDataBranch(std::string name, const RocketComponent* srcComponent,
                     std::span<const FlightDataType* const> types);

    /// The same with the types written in place.
    FlightDataBranch(std::string name, const RocketComponent* srcComponent,
                     std::initializer_list<std::reference_wrapper<const FlightDataType>> types);

    /// The branch of a stage that separates, started from the data of the branch it separates
    /// from, so that the data at the separation is in both branches. @p srcComponent is the
    /// separated stage (null: none), @p parent the branch to copy from (null: nothing is
    /// copied).
    ///
    /// The new branch has a TIME column, then every row of @p parent, written with addPoint()
    /// and setValue() for each of the parent's types in their sorted order (getTypes()); so the
    /// minima and maxima are those of the copied values, and a parent without rows gives a
    /// branch with the TIME column only. The modification id is drawn by those calls: it stays
    /// ModId::invalid() when no row is copied.
    ///
    /// Of the parent's events, in order, the new branch takes those that belong to the
    /// separated stage: not a STAGE_SEPARATION (the engine adds the one that made the branch),
    /// not when @p srcComponent or the event's source is null, only when both are in the same
    /// stage (the component itself when it is a stage, no stage for the Rocket, else
    /// RocketComponent::getStage(); compared by identity, so the events of a booster of the
    /// separated stage stay out, and so do the events whose sources are in another copy of the
    /// rocket), and then only when the source is @p srcComponent itself or one of its
    /// descendants. They are added directly: without a modification id, and the separation time
    /// stays NaN.
    /// @throws BugError when a source of an event of @p parent, or @p srcComponent, is neither
    ///         in a stage nor the Rocket
    FlightDataBranch(std::string name, const RocketComponent* srcComponent,
                     const FlightDataBranch* parent);

    /// The "empty" branch: named "Empty branch", without rows, with a column for every type of
    /// FlightDataType::allTypes(), and immutable.
    FlightDataBranch();

    /// The id of the component whose flight the branch describes; nullopt for imported or
    /// synthetic data (Java: null).
    [[nodiscard]] const std::optional<Uuid>& getSourceComponentId() const noexcept
    {
        return m_sourceComponentId;
    }

    /// The time at which the optimum altitude is reached, in s; NaN until it is set.
    [[nodiscard]] double getTimeToOptimumAltitude() const noexcept
    {
        return m_timeToOptimumAltitude;
    }
    void setTimeToOptimumAltitude(double timeToOptimumAltitude) noexcept
    {
        m_timeToOptimumAltitude = timeToOptimumAltitude;
    }

    /// The altitude the rocket would reach if no recovery device opened, in m; NaN until it is
    /// set.
    [[nodiscard]] double getOptimumAltitude() const noexcept { return m_optimumAltitude; }
    void                 setOptimumAltitude(double optimumAltitude) noexcept
    {
        m_optimumAltitude = optimumAltitude;
    }

    /// The ejection delay that would open the recovery device at the optimum altitude: the time
    /// to the optimum altitude minus the time of the last BURNOUT event; NaN while the time to
    /// the optimum altitude is NaN or the branch has no BURNOUT event.
    [[nodiscard]] double getOptimumDelay() const;

    /// Appends @p event; a STAGE_SEPARATION event also sets the separation time to its time.
    /// Draws a modification id. A refusal is raised at @p where, the call site by default.
    /// @throws BugError when the branch is immutable
    void addEvent(FlightEvent event, std::source_location where = std::source_location::current());

    /// The events, in the order they were added: the branch's own list, valid until the events
    /// change (see the class comment).
    [[nodiscard]] const std::vector<FlightEvent>& getEvents() const noexcept { return m_events; }

    /// The first event of type @p type, or null. Valid until the events change (see the class
    /// comment).
    [[nodiscard]] const FlightEvent* getFirstEvent(FlightEvent::Type type) const noexcept;

    /// The last event of type @p type, or null. Valid until the events change.
    [[nodiscard]] const FlightEvent* getLastEvent(FlightEvent::Type type) const noexcept;

    /// The first event with the id @p id, or null. Valid until the events change.
    [[nodiscard]] const FlightEvent* findEvent(const Uuid& id) const noexcept;

    /// The time of the last STAGE_SEPARATION event given to addEvent(), in s; NaN when there
    /// was none.
    [[nodiscard]] double getSeparationTime() const noexcept { return m_separationTime; }

    /// The index of the first row whose TIME is at or after @p time; nullopt when @p time is
    /// NaN, the branch has no TIME column or every row is earlier (Java: -1).
    [[nodiscard]] std::optional<std::size_t> getDataIndexOfTime(double time) const;

    /// Java's clone(): a mutable copy with the same name, source component id, columns, minima,
    /// maxima, events, optimum altitude, time to optimum altitude and modification id. As in
    /// Java, the separation time is not copied (it is NaN in the copy). It hides
    /// DataBranch::clone() on purpose: Java's clone() returns the subclass.
    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method): the subclass's clone()
    [[nodiscard]] FlightDataBranch clone() const;

private:
    /// A branch around the columns of @p columns, for clone().
    FlightDataBranch(DataBranch<FlightDataType>&& columns, std::optional<Uuid> sourceComponentId);

    /// Copies the rows of @p srcBranch and its events that belong to @p srcComponent (Java:
    /// copyValuesFromBranch(), after the TIME column has been made).
    void copyValuesFromBranch(const FlightDataBranch* srcBranch,
                              const RocketComponent*  srcComponent);

    std::optional<Uuid>      m_sourceComponentId;
    double                   m_timeToOptimumAltitude{std::numeric_limits<double>::quiet_NaN()};
    double                   m_optimumAltitude{std::numeric_limits<double>::quiet_NaN()};
    double                   m_separationTime{std::numeric_limits<double>::quiet_NaN()};
    std::vector<FlightEvent> m_events;
};

}  // namespace QtRocket
