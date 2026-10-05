#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class MotorClusterState;
class RocketComponent;
class Warning;

/// An event during the flight of a rocket (OpenRocket's simulation/FlightEvent): a type, the
/// simulation time, the component it comes from and a payload that depends on the type. An event
/// is immutable; the simulation queues events (EventQueue), handles them in order and stores the
/// ones that happened in the FlightDataBranch of the flight.
///
/// Identity: every event has an id (a random UUID unless one is given, as the .ork loader does).
/// Java hands event objects around by reference; here an event is a value, and a copy of an
/// event is the same event: it has the same id. sameEvent() is that identity.
///
/// Equality: Java declares `equals(FlightEvent)`, an overload that does not override
/// Object.equals(Object). Nothing in OpenRocket calls the overload: every Java code path that
/// compares events (PriorityQueue.remove(Object) and contains(), List.contains() and indexOf(),
/// JUnit's assertEquals()) goes through Object.equals(Object), which is object identity. So
/// equals() below is ported for completeness and no container uses it; EventQueue::remove() and
/// contains() compare with sameEvent().
///
/// The source: a non-owning pointer to a component of the simulated rocket, or null (Java: the
/// component reference). The pointer is valid only while that component lives; compareTo(),
/// validate() and toString() dereference it. Whoever keeps events longer than the simulation
/// (the FlightData of a finished simulation) keeps the rocket alive: see
/// FlightData::setSimulatedRocket(); the branch a SimulationCalculationException carries out of
/// a simulation is under the same rule (see there). The event also copies the source's id when
/// it is made
/// (getSourceId()), so that it can be saved, shown and matched against another copy of the rocket
/// once the pointer is no longer usable; RocketComponent::equals() (class and id) is how Java
/// compares the source of an event with a component of another copy of the rocket.
///
/// The data (Java: Object): exactly the payloads OpenRocket puts into an event.
/// - nothing (std::monostate; Java: null);
/// - the MotorClusterState of the motor that ignites, burns out or fires its charge (IGNITION,
///   BURNOUT, EJECTION_CHARGE), shared: the simulation status, the queued events and the events
///   of every branch hold the same state object, and the event keeps it alive after the status
///   is gone (see MotorClusterState, "Identity");
/// - the Warning of a SIM_WARN event, shared between the copies of the event, so that its
///   dynamic type and its id survive a copy. Java stores the very object its WarningSet holds,
///   and a set changes that object in place when a worse warning of the same kind arrives (the
///   angle of a Warning::LargeAOA grows during the flight). A WarningSet here owns copies, so the
///   event holds a copy with the same id, as it was when the event was made; the warning as the
///   set has it now is WarningSet::findById(event.getWarning()->id()), which is what
///   FlightData::findWarning() returns;
/// - the SimulationAbort of a SIM_ABORT event, a value;
/// - the altitudes before and after a step of an ALTITUDE event (Java: Pair<Double, Double>);
/// - the message of the exception of an EXCEPTION event (Java: String).
/// A null pointer given as data is no data.
///
/// Deviations from OpenRocket:
/// - validate() throws BugError where Java throws IllegalStateException; create() reports the
///   same failure as a Result, for the .ork loader, which Java lets catch the exception and turn
///   it into a warning.
/// - Java's RocketComponent.REMOVED sentinel does not exist here (findComponent() returns null),
///   so validate()'s exemption for it is the null source.
/// - The constructors that take a source id without a component are additions (events read from
///   a file refer to components by id). Such an event has a source for validate() and
///   getSourceId(), but compareTo() sees only the pointer and so orders it as an event without
///   one: its stage number is not known. toString() prints the id in place of the name.
/// - createDetached() is an addition for the same reader: it checks the event against the
///   component the id names, as Java's loader does through the constructor, and keeps the id
///   only. Flight data read from a file co-owns no rocket (see FlightData), and the components
///   of the document's rocket need not live as long as the data (OpenRocket's undo loads the
///   rocket anew), so its events must not point into a rocket.
/// - compareTo() throws BugError when a source has no stage above it and is not the Rocket (Java:
///   the IllegalStateException of getStage()).
class FlightEvent
{
public:
    /// The type of a flight event, in OpenRocket's order: compareTo() sorts by the ordinal, and
    /// the constant's name is what .ork files and the goldens store.
    enum class Type
    {
        LAUNCH,                      ///< rocket launch
        IGNITION,                    ///< a motor ignites; source: its mount, data: its state
        LIFTOFF,                     ///< the rocket has lifted off the ground
        LAUNCHROD,                   ///< the launch rod has been cleared
        BURNOUT,                     ///< a motor burns out; source: its mount, data: its state
        EJECTION_CHARGE,             ///< a motor's ejection charge fires; source: its stage
        STAGE_SEPARATION,            ///< a stage separates; source: the stage dropped
        APOGEE,                      ///< apogee has been reached
        RECOVERY_DEVICE_DEPLOYMENT,  ///< a recovery device opens; source: the device
        GROUND_HIT,                  ///< the ground has been hit after the flight
        SIMULATION_END,              ///< end of the simulation; queueing it ends the simulation
        ALTITUDE,                    ///< the altitude changed; data: an AltitudeChange
        TUMBLE,                      ///< the rocket begins to tumble
        SIM_WARN,                    ///< a warning was raised; data: the Warning
        SIM_ABORT,                   ///< the simulation cannot proceed; data: a SimulationAbort
        EXCEPTION,                   ///< a simulation exception was thrown; data: its message
    };

    /// Type.values(), in declaration order.
    static constexpr std::array<Type, 16> kAllTypes{
        Type::LAUNCH,           Type::IGNITION,       Type::LIFTOFF,
        Type::LAUNCHROD,        Type::BURNOUT,        Type::EJECTION_CHARGE,
        Type::STAGE_SEPARATION, Type::APOGEE,         Type::RECOVERY_DEVICE_DEPLOYMENT,
        Type::GROUND_HIT,       Type::SIMULATION_END, Type::ALTITUDE,
        Type::TUMBLE,           Type::SIM_WARN,       Type::SIM_ABORT,
        Type::EXCEPTION};

    /// The data of an ALTITUDE event (Java: Pair<Double, Double>): the altitude before the step
    /// (Java: getU()) and after it (getV()).
    struct AltitudeChange
    {
        double previous{0.0};
        double current{0.0};

        /// Java's Pair.equals() over two Doubles: each pair of values equal as Double.equals()
        /// has it (NaN equals NaN, 0.0 differs from -0.0).
        [[nodiscard]] bool operator==(const AltitudeChange& other) const noexcept;
    };

    /// What an event carries (Java: Object); see the class comment.
    using Data =
        std::variant<std::monostate, std::shared_ptr<MotorClusterState>,
                     std::shared_ptr<const Warning>, SimulationAbort, AltitudeChange, std::string>;

    /// An event without a source and without data.
    /// @throws BugError as validate()
    FlightEvent(Type type, double time);

    /// An event from @p source (null: none) without data.
    /// @throws BugError as validate()
    FlightEvent(Type type, double time, const RocketComponent* source);

    /// An event of the type and time of @p sourceEvent, from @p source with @p data; it is a new
    /// event, with an id of its own.
    /// @throws BugError as validate()
    FlightEvent(const FlightEvent& sourceEvent, const RocketComponent* source, Data data);

    /// An event from @p source (null: none) with @p data.
    /// @throws BugError as validate()
    FlightEvent(Type type, double time, const RocketComponent* source, Data data);

    /// The same with the id @p id; nullopt draws a random one (Java: a null id).
    /// @throws BugError as validate()
    FlightEvent(Type type, double time, const RocketComponent* source, Data data,
                std::optional<Uuid> id);

    /// An event whose source is known by its id only (an addition: see the class comment), with
    /// the id @p id, or a random one for nullopt.
    /// @throws BugError as validate()
    FlightEvent(Type type, double time, const Uuid& sourceId, Data data,
                std::optional<Uuid> id = std::nullopt);

    /// The event the constructor with the same arguments makes, or the failure of validate() as
    /// ErrorCode::INVALID_ARGUMENT with the same message (without BugError's prefix and
    /// location). For events made from data the program does not control (a file).
    [[nodiscard]] static Result<FlightEvent> create(Type type, double time,
                                                    const RocketComponent* source, Data data = {},
                                                    std::optional<Uuid> id = std::nullopt);

    /// create() for an event whose source is known by its id only.
    [[nodiscard]] static Result<FlightEvent> create(Type type, double time, const Uuid& sourceId,
                                                    Data                data = {},
                                                    std::optional<Uuid> id   = std::nullopt);

    /// create() for an event that must not point into a rocket (see the class comment): the
    /// event is checked as create(type, time, source, data, id) checks it, the class of
    /// @p source included, and then keeps the id of @p source only: getSource() is null and
    /// getSourceId() is the id of @p source. A null @p source gives an event without a source.
    /// @p source is not kept and need not outlive the call. For the .ork loader, which finds
    /// the component an event names in the document's rocket (Java: FlightDataBranchHandler).
    [[nodiscard]] static Result<FlightEvent> createDetached(Type type, double time,
                                                            const RocketComponent* source,
                                                            Data                   data = {},
                                                            std::optional<Uuid> id = std::nullopt);

    /// The data of a SIM_WARN event for @p warning: a copy of it (Message::clone(), so with the
    /// same dynamic type and the same id), which the copies of the event share. An addition:
    /// Java passes the warning object itself (see the class comment).
    [[nodiscard]] static Data warningData(const Warning& warning);

    [[nodiscard]] Type getType() const noexcept { return m_type; }

    /// The simulation time of the event, in s; never NaN.
    [[nodiscard]] double getTime() const noexcept { return m_time; }

    /// The component the event comes from, or null: for an event without a source and for one
    /// whose source is known by its id only. Valid while that component lives (see the class
    /// comment).
    [[nodiscard]] const RocketComponent* getSource() const noexcept { return m_source; }

    /// The id of the source, copied when the event was made; nullopt for an event without a
    /// source (an addition: Java reads getSource().getID()).
    [[nodiscard]] const std::optional<Uuid>& getSourceId() const noexcept { return m_sourceId; }

    /// Whether the event has a source, by pointer or by id.
    [[nodiscard]] bool hasSource() const noexcept { return m_sourceId.has_value(); }

    [[nodiscard]] const Data& getData() const noexcept { return m_data; }

    /// Whether the event carries data (Java: getData() != null).
    [[nodiscard]] bool hasData() const noexcept
    {
        return !std::holds_alternative<std::monostate>(m_data);
    }

    /// The motor state the event carries, or null when its data is something else. The state is
    /// the simulation's own object: igniting it through this pointer ignites it for every holder.
    [[nodiscard]] std::shared_ptr<MotorClusterState> getMotorState() const;

    /// The warning the event carries, or null when its data is something else: the warning as
    /// it was when the event was made. Whoever shows or stores the warning of an event of a
    /// simulation reads it through FlightData::findWarning(), which gives the warning as the
    /// warning set has it now (see the class comment).
    [[nodiscard]] std::shared_ptr<const Warning> getWarning() const;

    /// The abort the event carries, or null when its data is something else. The pointer is
    /// valid while this event lives.
    [[nodiscard]] const SimulationAbort* getAbort() const noexcept;

    /// The altitudes the event carries, or nullopt when its data is something else.
    [[nodiscard]] std::optional<AltitudeChange> getAltitudeChange() const noexcept;

    /// The text the event carries (the message of an EXCEPTION event), or null when its data is
    /// something else. The pointer is valid while this event lives.
    [[nodiscard]] const std::string* getMessage() const noexcept;

    /// The event's id (Java: getID()).
    [[nodiscard]] const Uuid& getId() const noexcept { return m_id; }

    /// Whether @p other is this event: the same id (Java: the same object; see the class
    /// comment).
    [[nodiscard]] bool sameEvent(const FlightEvent& other) const noexcept
    {
        return m_id == other.m_id;
    }

    /// Java's compareTo(), the order of the event queue. Negative when this event comes first:
    /// - the earlier time first;
    /// - at the same time, an event without a source (pointer) before one with a source;
    /// - with two sources, the higher stage number first (the lower stage; the Rocket itself has
    ///   stage number -1 and so comes last);
    /// - then by type: the difference of the ordinals (which is not limited to -1, 0 and 1).
    /// @throws BugError when a source is not in a stage (see the class comment)
    [[nodiscard]] int compareTo(const FlightEvent& other) const;

    /// Java's equals(FlightEvent): two SIM_WARN events are equal when their warnings are
    /// (Message::equals()); any other two when compareTo() is 0. Not an identity and not used by
    /// any container: see the class comment.
    [[nodiscard]] bool equals(const FlightEvent& other) const;

    /// "FlightEvent[type=<name>,time=<time>,source=<source>,data=<data>]": the type's constant
    /// name, the time as Java prints a double, the source's name (RocketComponent::toString())
    /// or "null", and the data: "null", the motor's designation, the warning's or the abort's
    /// text (Message::toString()), "[<previous>;<current>]" or the message.
    [[nodiscard]] std::string toString() const;

    /// Checks that the event is well formed (Java: validate(), which every constructor calls):
    /// - the time is not NaN;
    /// - IGNITION and BURNOUT: the source, if there is one, is a MotorMount; the data, if there
    ///   is any, is a MotorClusterState;
    /// - EJECTION_CHARGE: the source, if there is one, is an AxialStage; the data, if there is
    ///   any, is a MotorClusterState;
    /// - SIM_WARN: there is no source (the sources are those of the warning) and the data is a
    ///   Warning;
    /// - SIM_ABORT: the data is a SimulationAbort.
    /// The other types take any source and any data. The class of a source that is known by its
    /// id only cannot be checked (createDetached() checks it before the pointer is dropped).
    /// @throws BugError with Java's message when a check fails
    void validate() const;

private:
    /// Tag of the constructor that does not validate.
    struct Unchecked
    {
        explicit Unchecked() = default;
    };

    FlightEvent(Unchecked tag, Type type, double time, const RocketComponent* source,
                std::optional<Uuid> sourceId, Data data, std::optional<Uuid> id);

    /// The message of the first check of validate() that fails, or nullopt.
    [[nodiscard]] std::optional<std::string> validationError() const;

    /// The message of the first check on the source that fails for this type, or nullopt.
    [[nodiscard]] std::optional<std::string> sourceError() const;

    /// The message of the check on the data that fails for this type, or nullopt.
    [[nodiscard]] std::optional<std::string> dataError() const;

    /// The data as toString() prints it (Java: String.valueOf(data)).
    [[nodiscard]] std::string dataToString() const;

    Uuid                   m_id;
    Type                   m_type;
    double                 m_time;
    const RocketComponent* m_source{nullptr};
    std::optional<Uuid>    m_sourceId;
    Data                   m_data;
};

/// The constant's name, e.g. "EJECTION_CHARGE" (Java: Type.name()); the goldens store it.
[[nodiscard]] std::string_view name(FlightEvent::Type type) noexcept;

/// The type whose name() is @p name, compared exactly, or nullopt (Java: Type.valueOf(), which
/// throws IllegalArgumentException).
[[nodiscard]] std::optional<FlightEvent::Type> flightEventTypeFromName(
    std::string_view name) noexcept;

/// The position of @p type in the declaration (Java: ordinal()), LAUNCH being 0.
[[nodiscard]] constexpr int ordinal(FlightEvent::Type type) noexcept
{
    return static_cast<int>(type);
}

/// The name the type has in .ork files (the type attribute of <event>): name() in lower case
/// without underscores, e.g. "ejectioncharge".
[[nodiscard]] std::string_view orkName(FlightEvent::Type type) noexcept;

/// The type @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else.
[[nodiscard]] std::optional<FlightEvent::Type> flightEventTypeFromOrkName(std::string_view text);

/// The translation key of the display name, "FlightEvent.Type." + name().
[[nodiscard]] std::string_view displayKey(FlightEvent::Type type) noexcept;

/// The English display name (Java: Type.toString() with OpenRocket's English messages), e.g.
/// "Ejection charge". Warning::EventAfterLanding takes it as its event type.
[[nodiscard]] std::string_view displayName(FlightEvent::Type type) noexcept;

}  // namespace QtRocket
