#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class Rocket;

/// Reads one <warning> element of the stored flight data of a simulation (OpenRocket's
/// file/openrocket/importt/WarningHandler) and adds the warning it describes to a set, the set
/// of the flight data handler that made it. What OpenRocket writes:
///
///     <warning type="HighSpeedDeployment">
///       <id>76a35ffa-fd95-40a7-9c8e-bcc900bbdb48</id>
///       <description>Recovery device deployment at high speed (27.5 m/s)</description>
///       <priority>NORMAL</priority>
///       <source>0c0060c1-55bf-47e8-acb8-a6db9242f14e</source>
///       <parameter>27.529900134801906</parameter>
///       Recovery device deployment at high speed (27.5 m/s):  "Parachute"
///     </warning>
///
/// The children, each of which may come several times, the last one counting (but for <source>,
/// which adds up):
/// - <id>: the warning's id, read as java.util.UUID.fromString() reads it (Uuid::
///   javaFromString(): "1-2-3-4-5" is an id, a padded one is not). A text that is no id fails
///   the load. Without the element the warning has a random id.
/// - <description>: the text of a warning that is only its text, trimmed. An empty description
///   is one.
/// - <priority>: "LOW", "NORMAL" or "HIGH", compared exactly and not trimmed; anything else is
///   NORMAL.
/// - <source>: the id of a component that caused the warning, read as <id> is, with the same
///   failure. The sources are kept in the order of the elements; a component named twice is
///   there twice.
/// - <parameter>: the number of the warning (see the types), trimmed and read with
///   Double.parseDouble. A text that is no number fails the load, whatever the type of the
///   warning: "NaN" and "Infinity" are numbers, "Inf" is not.
/// - any other child: "Unknown element '<name>', ignoring.".
/// A child element inside one of these is ignored with PlainTextHandler's warning ("Unknown
/// element <name>, ignoring."), and as everywhere in the loader the text and the attributes
/// of the elements around it then shift by one (DelegatorHandler): the child of the warning
/// is read with the text behind the ignored element only, and the <warning> element ends
/// with the attributes and the text of that child in place of its own, so its type is lost.
///
/// The element's own text is the description when there is no <description> child. OpenRocket
/// writes the whole text of the warning there (with its sources) for older versions of itself,
/// and files of those versions have nothing else: for them the text is the warning.
///
/// When the element closes, the type attribute, compared exactly, decides the warning:
/// - "LargeAOA": a Warning::LargeAOA with the parameter as its angle (NaN without one);
/// - "HighSpeedDeployment": a Warning::RecoveryHighSpeedDeployment with the parameter as its
///   speed. That is the name of the class in OpenRocket 24.12, whose files the examples are;
/// - "EventAfterLanding": a Warning::EventAfterLanding without an event. The flight data
///   branch's handler gives it its event when it reads the SIM_WARN event that names both;
/// - anything else, and no attribute: a Warning::Other with the description. This is also what
///   becomes of the names this OpenRocket's saver writes for the other classes
///   ("RecoveryHighSpeedDeployment", "HighSpeedMainDeployment", "MissingMotor", ...): such a
///   warning keeps its text, its sources and its priority and loses its number.
/// The description is used for the last kind only; the first three make their own text.
///
/// The warning then gets the id, the priority and the sources that were read. The priority is
/// NORMAL without a <priority> child whatever the class would have on its own, so a LargeAOA
/// (LOW by itself) or an EventAfterLanding (HIGH) of a file that does not say is NORMAL.
///
/// The set holds at most one warning of a kind (Message::equals()), so a warning equal to one
/// read before is not added: a second "HighSpeedDeployment" for the same sources and priority,
/// a second "Other" with the same text, sources and priority. Of two equal LargeAOA warnings
/// the first stays and takes the larger angle. A warning that is not added leaves its id
/// behind: a SIM_WARN event that names that id finds no warning and is dropped by the branch's
/// handler with a warning of the load.
///
/// A source id that names no component of the rocket is kept as the source
/// MessageSource::removed(id): the id as read, under the name OpenRocket's removed component
/// has, which the warning's text then shows.
///
/// Deviations from OpenRocket:
/// - Java's source for an id that names no component is the one RocketComponent.REMOVED
///   object, so there two warnings that differ only in such ids are one, and a save writes
///   the id of that object and not the id read. Here the ids are kept, and the two warnings
///   are two.
/// - Two EventAfterLanding warnings with the same id are one here. In Java they are two, since
///   its equals() compares the UUID objects by reference (see Warning::EventAfterLanding).
/// - The failure of a text that is no id is Uuid::javaFromString()'s, under
///   ErrorCode::INVALID_ARGUMENT.
class WarningHandler final : public AbstractElementHandler
{
public:
    /// A handler of a <warning> element that looks the sources up in @p rocket and adds the
    /// warning to @p warningSet when the element closes. Both must outlive the handler.
    WarningHandler(const Rocket& rocket, WarningSet& warningSet);

    /// Every child is plain text.
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Makes the warning and adds it to the set (see the class comment). Fails, with
    /// ErrorCode::INVALID_ARGUMENT and the message of Java's NumberFormatException, when the
    /// <parameter> is no number; nothing is added then.
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    const Rocket*              m_rocket;
    WarningSet*                m_warningSet;
    std::optional<Uuid>        m_id;
    MessagePriority            m_priority{MessagePriority::NORMAL};
    MessageSources             m_sources;
    std::optional<std::string> m_warningText;
    std::optional<std::string> m_parameter;
};

}  // namespace QtRocket
