#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class FlightDataType;
class OpenRocketDocument;

/// Reads one <databranch> element of the stored flight data of a simulation (OpenRocket's
/// file/openrocket/importt/FlightDataBranchHandler): the columns its types attribute names,
/// its data points and its flight events, into a FlightDataBranch. The flight data handler
/// makes one per <databranch> element that has a name and types.
///
///     <databranch name="Sustainer" optimumAltitude="307.49" timeToOptimumAltitude="7.43"
///                 types="time,altitude,velocity_total">
///       <event time="0" type="launch" id="..." source="..."/>
///       <event time="4.863" type="simwarn" id="..." warnid="..."/>
///       <datapoint>0,0,0</datapoint>
///       <datapoint>0.01,0.002,0.4</datapoint>
///     </databranch>
///
/// The columns: the types attribute is split at its commas as Java's String.split() does (a
/// trailing comma counts for nothing, "" is one empty name, "," is no name at all) and each
/// name, not trimmed, is resolved by findFlightDataType(). The branch needs at least one type,
/// and no two that are equal (FlightDataType::equals(): the names, ignoring case); else the
/// load fails, as create() says.
///
/// findFlightDataType(), in this order:
/// 1. the built-in type with that save key ("drag_coeff"), which is what OpenRocket writes;
/// 2. the built-in type with exactly that name ("Drag coefficient (CD)"; files from before the
///    save keys), in the order of FlightDataType::allTypes();
/// 3. "Position upwind", a type that no longer exists: TYPE_POSITION_Y;
/// 4. the ten names that have since got a symbol in brackets: "Drag coefficient", "Axial drag
///    coefficient", "Friction drag coefficient", "Pressure drag coefficient", "Base drag
///    coefficient", "Normal force coefficient", "Pitch moment coefficient", "Roll rate",
///    "Pitch rate" and "Yaw rate";
/// 5. the type of the first custom expression of the document with exactly that name
///    (CustomExpression::getType(); the file's <datatypes> come before its simulations);
/// 6. else a type of that name with the symbol "Unknown" and no unit, in the CUSTOM group
///    (FlightDataType::getType(name, "Unknown", UnitGroupId::NONE)).
/// So an old name and the new key of one type in one branch ("drag_coeff,Drag coefficient")
/// are a type twice, and so are two unknown names that differ only in case.
///
/// The process-wide registry of flight data types: steps 5 and 6 go through
/// FlightDataType::getType(), which keeps one type per symbol. Every unknown column name
/// therefore replaces the type registered under "Unknown" (the types made before stay valid
/// and keep their names, so each column has its own name), as in OpenRocket, and two things
/// follow from that as they do there:
/// - an unknown name that is empty or blank takes the name of the type that is registered
///   under "Unknown" at that moment, which is the last unknown name any load of the process
///   resolved (the name stays "" in a process that has resolved none);
/// - whoever looks a type up by the symbol "Unknown" afterwards gets the last one.
/// Tests that resolve unknown names share that state with every other test of their process:
/// they compare the types of a branch by name and symbol, never by identity with a type of
/// another load, and register a name of their own first where the empty name matters.
///
/// A <datapoint> holds one number per column, separated by commas and split as the types are
/// ("6,7,8," is three numbers). A count other than the number of columns gives "Data point did
/// not contain correct amount of values, ignoring point."; a text that is no number
/// (DocumentConfig::stringToDouble(): "NaN", "Inf" and "-Inf" in any case, else
/// Double.parseDouble, which trims) gives "Data point format error, ignoring point.". A good
/// point is a new row. The numbers are stored as they are, the ones that are not finite
/// included: stored flight data is data, not an input of a run.
///
/// An <event> is added to the branch's events in the order of the file:
/// - time: stringToDouble(); a missing or unreadable one gives "Illegal event time
///   specification, ignoring: <message>" with the message of Java's NumberFormatException
///   ("null string" for none), and the event is dropped before anything else is looked at;
/// - type: the type's name as DocumentConfig::findEnum() matches it ("groundhit" for
///   GROUND_HIT); else "Illegal event specification, ignoring." and the event is dropped;
/// - id: the event's id (Uuid::javaFromString()); a random one without the attribute;
/// - source: the id of the component the event comes from, looked up in the document's rocket;
/// - warnid, read for a SIM_WARN event only: the id of the warning the event carries, looked
///   up in the warnings the flight data handler has read so far, so a warning that stands
///   behind the branch in the file is not found;
/// - cause, read for every type: the cause of an abort as findEnum() matches it
///   ("nomotorsdefined"). When it names one, a SimulationAbort with that cause is the event's
///   data in place of the warning, whatever the event's type.
/// An id, source or warnid that is no UUID fails the load, with the message of Java's
/// UUID.fromString() for the text (Uuid::javaFromString()). The event is then checked as
/// FlightEvent checks every event; a failure gives "Illegal parameters for FlightEvent:
/// <message>" (FlightEvent::validate() lists the messages) and the event is dropped. So a
/// SIM_WARN event needs a warning and must not have a source, a SIM_ABORT event needs a cause,
/// and an IGNITION, BURNOUT or EJECTION_CHARGE event must not have a cause.
///
/// A source id that names no component of the rocket is kept: the event has that id as its
/// source id and is checked as an event from OpenRocket's removed component is (its class is
/// not looked at, and for a SIM_WARN event the message names it "<i>Component Removed From
/// Rocket</i>").
///
/// An event after landing: for a SIM_WARN event whose warning is a Warning::EventAfterLanding
/// (and that has no cause, which would have taken the warning's place) and that has an
/// eventid attribute (which is then read, and fails the load when it is no UUID), the
/// warning's event becomes the event of this branch with that id, the first one when several
/// have it, the SIM_WARN event itself included; when the branch has none, the warning loses
/// the event it had. This is done whether or not the SIM_WARN event itself was dropped, and
/// to the warning of the flight data handler's set, so the flight data shows it. For any
/// other event, and for a warning of another class, an eventid is not read.
///
/// Any other child gives "Unknown element '<name>' encountered, ignoring." and is ignored
/// with everything in it; the text and the attributes of the elements around it then shift by
/// one (DelegatorHandler), so that the <flightdata> element ends with the attributes of this
/// <databranch>. A child element of an <event> or a <datapoint> does the same to that element:
/// the event is then read from the attributes of its child.
///
/// Deviations from OpenRocket:
/// - create() returns the failures Java's constructor throws, and makes them before a branch
///   exists (the FlightDataBranch constructor would throw BugError for them).
/// - Two types are a type twice when FlightDataType::equals() says so, and the load then
///   fails. Java asks its HashMap, whose hash of a name (the name in lower case) is not always
///   the same for two names that are equal ignoring case (see FlightDataType::hashCode()), so
///   OpenRocket loads a few such pairs as two columns where the load is refused here: names
///   that differ only by the micro sign and the Greek mu, the long s and the s, the sigma and
///   the final sigma, or the dotted capital I and the i (and the other letters
///   FlightDataType::hashCode() names). No file OpenRocket writes for its own types has such
///   a pair; two custom expressions or two unknown columns named so are what it takes.
/// - The events keep the id of their source and no pointer to the component
///   (FlightEvent::createDetached()), and a SIM_WARN event holds a copy of its warning as it
///   was when the event was read, where Java's events share the warning object: read the
///   warning of an event of loaded flight data through FlightData::findWarning().
/// - A source id that names no component is kept (see above); Java's event has its removed
///   component, whose id is another one.
/// - The handler takes the warnings of the flight data and the loading context, where Java's
///   takes the simulation's handler to ask for both.
class FlightDataBranchHandler final : public AbstractElementHandler
{
    /// Only create() makes a handler.
    struct Passkey
    {
        explicit Passkey() = default;
    };

public:
    /// A handler of a <databranch> element with the name @p name and the types attribute
    /// @p typeList (Java: the constructor). @p simulationWarnings are the warnings of the
    /// flight data read so far, in which the events find their warnings and which an event
    /// after landing changes; @p context gives the document, for its rocket and its custom
    /// expressions. Both must outlive the handler, and the context must have a document.
    ///
    /// Fails with ErrorCode::INVALID_ARGUMENT and Java's message, which fails the load:
    /// - "Must specify at least one data type." when @p typeList names no type (",");
    /// - "Value type <name> already exists." for the first type that equals one before it,
    ///   with the name of the later of the two.
    /// Every name is resolved before either is checked, so the unknown names of a list that
    /// fails are registered all the same.
    /// @throws BugError when @p context has no document
    [[nodiscard]] static Result<std::unique_ptr<FlightDataBranchHandler>> create(
        std::string name, std::string_view typeList, WarningSet& simulationWarnings,
        const DocumentLoadingContext& context);

    /// Only create() can call this (see Passkey).
    FlightDataBranchHandler(Passkey passkey, std::string name,
                            std::vector<const FlightDataType*> types,
                            WarningSet& simulationWarnings, const DocumentLoadingContext& context);

    /// The type the column name @p name of a file stands for, looked up as the class comment
    /// says, with the custom expressions of @p document (findFlightDataType()). It registers a
    /// type for a name it does not know.
    [[nodiscard]] static const FlightDataType& findFlightDataType(
        std::string_view name, const OpenRocketDocument& document);

    /// Sets the branch's time to the optimum altitude, in s.
    void setTimeToOptimumAltitude(double timeToOptimumAltitude) noexcept;

    /// Sets the branch's optimum altitude, in m.
    void setOptimumAltitude(double optimumAltitude) noexcept;

    /// The types of the columns, in the order of the types attribute.
    [[nodiscard]] std::span<const FlightDataType* const> getTypes() const noexcept
    {
        return m_types;
    }

    /// The branch, which is immutable from this call on (getBranch()): the flight data handler
    /// asks for it when the element has closed.
    [[nodiscard]] std::shared_ptr<FlightDataBranch> getBranch();

    /// <datapoint> and <event> are plain text; anything else is ignored with a warning.
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    /// An <event>: see the class comment.
    [[nodiscard]] Result<void> closeEvent(const Attributes& attributes, WarningSet& warnings);

    /// A <datapoint>: see the class comment.
    void closeDatapoint(std::string_view content, WarningSet& warnings);

    const DocumentLoadingContext*      m_context;
    WarningSet*                        m_simulationWarnings;
    std::vector<const FlightDataType*> m_types;
    std::shared_ptr<FlightDataBranch>  m_branch;
};

}  // namespace QtRocket
