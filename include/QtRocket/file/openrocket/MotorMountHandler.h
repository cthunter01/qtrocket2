#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/openrocket/IgnitionConfigurationHandler.h"
#include "QtRocket/file/openrocket/MotorHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class MotorMount;

/// Reads the <motormount> element of a body tube or an inner tube: the motors of its flight
/// configurations and when they ignite (OpenRocket's
/// file/openrocket/importt/MotorMountHandler). Making the handler makes the component act as a
/// motor mount (MotorMount::setMotorMount(true)), whatever the element holds.
///
/// The children, applied as they close, in the order of the file:
/// - <ignitionevent>, <ignitiondelay>: the ignition of the mount's DEFAULT motor
///   configuration, from which every motor configuration made afterwards starts. The event is
///   the one whose .ork name is exactly the text, which is not trimmed
///   (ignitionEventFromOrkName(); else "Unknown ignition event type '<text>', ignoring."); the
///   delay is Double.parseDouble (else "Illegal ignition delay specified, ignoring.").
/// - <overhang>: MotorMount::setMotorOverhang(), Double.parseDouble (else "Illegal overhang
///   specified, ignoring.").
/// - <motor configid="...">: the motor of one flight configuration, read by a MotorHandler. A
///   new MotorConfiguration is made from the mount's default one (so it has the default's
///   ignition as it is at that moment), given the motor the handler finds (MotorHandler::
///   getMotor(), possibly none), then its ejection delay (getDelay()) and its nozzle exit
///   diameter (getNozzleExitDiameter(); one the configuration refuses gives "Invalid nozzle
///   exit diameter, assuming unknown: <reason>" and leaves it unknown), in this order, which is
///   the order of their warnings. It becomes the mount's configuration of that id, the rocket
///   gets a flight configuration of that id when it has none (a TREE_CHANGE event), and that
///   flight configuration is told of the motor.
/// - <ignitionconfiguration configid="...">: the ignition of one flight configuration, read
///   by an IgnitionConfigurationHandler and set on MotorMount::getMotorConfig() of that id.
///   That is the mount's DEFAULT configuration when the id has no motor in this mount, so such
///   an element changes the ignition of every configuration made later, as in OpenRocket.
///   The rocket's flight configuration of that id is then told of the change
///   (FlightConfiguration::refreshMotor()), so that its motors have the ignition the mount
///   has, as in OpenRocket, where they are the mount's own objects: the saver writes the
///   <ignitionconfiguration> behind the <motor> of its id, and nothing else would renew the
///   flight configuration's copy before the next change of the rocket.
/// - any other child is ignored with "Unknown element '<name>' encountered, ignoring.".
///
/// The id of a <motor> and of an <ignitionconfiguration> is DocumentConfig::configurationId():
/// an element without the attribute gets a new random id, so its motor belongs to a flight
/// configuration nothing else names.
///
/// Deviations from OpenRocket:
/// - An ignition delay or an overhang that is a NaN or an infinity is refused with the warning
///   of a text that is no number (OpenRocket stores it).
/// - An <ignitionconfiguration> applies what it gives: the delay when one was read, then the
///   event when one was read. Without a delay it also adds Warning::kFileInvalidParameter:
///   OpenRocket dies there of a NullPointerException, before it looks at the event. Without an
///   event OpenRocket stores null as the configuration's event; here the event stays.
/// - "Illegal motor specification, ignoring." is given for an id that is not valid, as the
///   Java code has it. There it cannot happen (an id made from a text never is the error id,
///   see FlightConfigurationId); here it happens for a configid that spells out the error
///   id's key. A <motor> whose configid spells out the key of the default id gets the same
///   warning and is ignored: OpenRocket puts its configuration in the place of the mount's
///   default one, which a MotorConfigurationSet here does not allow (its default is always
///   "no motor").
/// - A mount that is not in a rocket is a BugError at the first <motor> (Java: an
///   IllegalStateException); the component handler attaches a component before it reads it.
/// - A <motor> of a flight configuration the rocket does not have yet is ignored with
///   Warning::kFileInvalidParameter, before its motor is looked for, when that configuration
///   would take the rocket beyond DocumentConfig::kMaxInstances component instances over all
///   its flight configurations (DocumentConfig::flightConfigurationFits(); see
///   MotorConfigurationHandler). OpenRocket has no bound and runs out of memory.
/// - The flight configuration gets a copy of the motor configuration the mount stored (Java:
///   the object itself), which the handler keeps in step with the mount's as said above. One
///   case stays apart: two <motor> elements of one configid in one mount and no change of the
///   rocket between the second and an <ignitionconfiguration> of that id. OpenRocket's list
///   of the active motors then still holds the first motor's object, which the ignition does
///   not reach, until the next change of the rocket; here both lists have the second motor
///   with the new ignition at once.
class MotorMountHandler final : public AbstractElementHandler
{
public:
    /// The handler of the <motormount> element of @p mount, which becomes an acting motor
    /// mount at once (a MOTOR_CHANGE event when it was none). Both @p mount and @p context
    /// must outlive the handler.
    MotorMountHandler(MotorMount& mount, const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    MotorMountHandler(MotorMount& mount, const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    // closeElement() in parts, one per child.

    /// A <motor> closes: the motor configuration of its flight configuration.
    void closeMotor(const Attributes& attributes, WarningSet& warnings);
    /// An <ignitionconfiguration> closes: the ignition of its flight configuration.
    void closeIgnitionConfiguration(const Attributes& attributes, WarningSet& warnings);
    /// An <ignitionevent> closes: the default ignition event.
    void closeIgnitionEvent(std::string_view content, WarningSet& warnings);
    /// An <ignitiondelay> closes: the default ignition delay.
    void closeIgnitionDelay(std::string_view content, WarningSet& warnings);
    /// An <overhang> closes.
    void closeOverhang(std::string_view content, WarningSet& warnings);

    MotorMount*                   m_mount;
    const DocumentLoadingContext* m_context;
    /// The handler of the <motor> read last; the next one replaces it.
    std::unique_ptr<MotorHandler> m_motorHandler;
    /// The handler of the <ignitionconfiguration> read last; the next one replaces it.
    std::unique_ptr<IgnitionConfigurationHandler> m_ignitionConfigHandler;
};

}  // namespace QtRocket
