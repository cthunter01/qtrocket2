#include "QtRocket/file/openrocket/MotorMountHandler.h"

#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/IgnitionConfigurationHandler.h"
#include "QtRocket/file/openrocket/MotorHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

MotorMountHandler::MotorMountHandler(MotorMount& mount, const DocumentLoadingContext& context)
  : m_mount(&mount), m_context(&context)
{
    m_mount->setMotorMount(true);
}

Result<ElementHandler*> MotorMountHandler::openElement(std::string_view element,
                                                       const Attributes& /*attributes*/,
                                                       WarningSet& warnings)
{
    if (element == "motor")
    {
        m_motorHandler = std::make_unique<MotorHandler>(*m_context);
        return m_motorHandler.get();
    }

    if (element == "ignitionconfiguration")
    {
        m_ignitionConfigHandler = std::make_unique<IgnitionConfigurationHandler>(*m_context);
        return m_ignitionConfigHandler.get();
    }

    if (element == "ignitionevent" || element == "ignitiondelay" || element == "overhang")
    {
        return &PlainTextHandler::instance();
    }

    warnings.add(
        Warning::fromString(std::format("Unknown element '{}' encountered, ignoring.", element)));
    return nullptr;
}

Result<void> MotorMountHandler::closeElement(std::string_view element, const Attributes& attributes,
                                             std::string_view content, WarningSet& warnings)
{
    if (element == "motor")
    {
        closeMotor(attributes, warnings);
        return {};
    }
    if (element == "ignitionconfiguration")
    {
        closeIgnitionConfiguration(attributes, warnings);
        return {};
    }
    if (element == "ignitionevent")
    {
        closeIgnitionEvent(content, warnings);
        return {};
    }
    if (element == "ignitiondelay")
    {
        closeIgnitionDelay(content, warnings);
        return {};
    }
    if (element == "overhang")
    {
        closeOverhang(content, warnings);
        return {};
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

void MotorMountHandler::closeMotor(const Attributes& attributes, WarningSet& warnings)
{
    // Only an element this handler opened closes here, so the motor's handler is there.
    QTROCKET_ASSERT(m_motorHandler != nullptr);

    const FlightConfigurationId fcid = DocumentConfig::configurationId(attributes);
    // The default id: see the class comment.
    if (!fcid.isValid() || fcid.isDefaultId())
    {
        warnings.add(Warning::fromString("Illegal motor specification, ignoring."));
        return;
    }
    // Not OpenRocket's: a motor whose flight configuration would take the rocket beyond its
    // instance budget is ignored (see the class comment), before anything is set: the flight
    // configuration of an id the rocket does not have is its default one, which must not be
    // told of a motor.
    Rocket& rocket = asComponent(*m_mount).getRocket();
    if (!rocket.containsFlightConfigurationId(fcid) &&
        !DocumentConfig::flightConfigurationFits(rocket))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return;
    }
    std::shared_ptr<const Motor> motor = m_motorHandler->getMotor(warnings);
    MotorConfiguration           motorConfig(*m_mount, fcid, m_mount->getDefaultMotorConfig());
    motorConfig.setMotor(std::move(motor));
    motorConfig.setEjectionDelay(m_motorHandler->getDelay(warnings));
    if (const Result<void> set =
            motorConfig.setNozzleExitDiameter(m_motorHandler->getNozzleExitDiameter());
        !set)
    {
        warnings.add(Warning::fromString(std::format(
            "Invalid nozzle exit diameter, assuming unknown: {}", set.error().message)));
    }

    m_mount->setMotorConfig(std::move(motorConfig), fcid);

    rocket.createFlightConfiguration(fcid);
    // Java hands the flight configuration the object the mount holds; here it gets a copy of
    // the one the mount stored.
    rocket.getFlightConfiguration(fcid).addMotor(m_mount->getMotorConfig(fcid));
}

void MotorMountHandler::closeIgnitionConfiguration(const Attributes& attributes,
                                                   WarningSet&       warnings)
{
    QTROCKET_ASSERT(m_ignitionConfigHandler != nullptr);

    const FlightConfigurationId fcid = DocumentConfig::configurationId(attributes);
    if (!fcid.isValid())
    {
        warnings.add(Warning::fromString("Illegal motor specification, ignoring."));
        return;
    }

    // The default configuration when the id has no motor in this mount, as in Java.
    MotorConfiguration&         inst  = m_mount->getMotorConfig(fcid);
    const std::optional<double> delay = m_ignitionConfigHandler->getIgnitionDelay();
    if (delay.has_value())
    {
        inst.setIgnitionDelay(*delay);
    }
    else
    {
        // Java: a NullPointerException, the delay being a null Double.
        warnings.add(Warning::kFileInvalidParameter);
    }
    if (const std::optional<IgnitionEvent> event = m_ignitionConfigHandler->getIgnitionEvent())
    {
        inst.setIgnitionEvent(*event);
    }

    // Java's flight configuration holds `inst` itself and so has the new ignition already;
    // here it holds a copy, made when the <motor> of this id closed or at the last update,
    // and neither setter fires an event that would renew it. (Nothing to do for the mount's
    // default configuration, which no flight configuration holds.)
    if (auto* const rocket = dynamic_cast<Rocket*>(&asComponent(*m_mount).getRoot());
        rocket != nullptr)
    {
        rocket->getFlightConfiguration(fcid).refreshMotor(inst);
    }
}

void MotorMountHandler::closeIgnitionEvent(std::string_view content, WarningSet& warnings)
{
    const std::optional<IgnitionEvent> event = ignitionEventFromOrkName(content);
    if (!event.has_value())
    {
        warnings.add(Warning::fromString(
            std::format("Unknown ignition event type '{}', ignoring.", content)));
        return;
    }
    m_mount->getDefaultMotorConfig().setIgnitionEvent(*event);
}

void MotorMountHandler::closeIgnitionDelay(std::string_view content, WarningSet& warnings)
{
    // Java: Double.parseDouble, a NaN and an infinity included.
    const std::optional<double> delay = DocumentConfig::parseFiniteDouble(content);
    if (!delay.has_value())
    {
        warnings.add(Warning::fromString("Illegal ignition delay specified, ignoring."));
        return;
    }
    m_mount->getDefaultMotorConfig().setIgnitionDelay(*delay);
}

void MotorMountHandler::closeOverhang(std::string_view content, WarningSet& warnings)
{
    // Java: Double.parseDouble, a NaN and an infinity included.
    const std::optional<double> overhang = DocumentConfig::parseFiniteDouble(content);
    if (!overhang.has_value())
    {
        warnings.add(Warning::fromString("Illegal overhang specified, ignoring."));
        return;
    }
    m_mount->setMotorOverhang(*overhang);
}

}  // namespace QtRocket
