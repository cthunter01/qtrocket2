#include "QtRocket/simulation/FlightEvent.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

using Type = FlightEvent::Type;

/// @p data with a null pointer turned into "no data" (Java: a null payload).
[[nodiscard]] FlightEvent::Data normalized(FlightEvent::Data data)
{
    if (const auto* state = std::get_if<std::shared_ptr<MotorClusterState>>(&data);
        state != nullptr && *state == nullptr)
    {
        return {};
    }
    if (const auto* warning = std::get_if<std::shared_ptr<const Warning>>(&data);
        warning != nullptr && *warning == nullptr)
    {
        return {};
    }
    return data;
}

/// String.valueOf() of each kind of data.
struct DataPrinter
{
    [[nodiscard]] std::string operator()(std::monostate /*none*/) const { return "null"; }
    [[nodiscard]] std::string operator()(const std::shared_ptr<MotorClusterState>& state) const
    {
        return state->toString();
    }
    [[nodiscard]] std::string operator()(const std::shared_ptr<const Warning>& warning) const
    {
        return warning->toString();
    }
    [[nodiscard]] std::string operator()(const SimulationAbort& abort) const
    {
        return abort.toString();
    }
    [[nodiscard]] std::string operator()(const FlightEvent::AltitudeChange& change) const
    {
        // Java: Pair.toString(), "[" + u + ";" + v + "]".
        return "[" + Strings::javaDoubleToString(change.previous) + ";" +
               Strings::javaDoubleToString(change.current) + "]";
    }
    [[nodiscard]] std::string operator()(const std::string& text) const { return text; }
};

}  // namespace

bool FlightEvent::AltitudeChange::operator==(const AltitudeChange& other) const noexcept
{
    return MathUtil::javaDoubleCompare(previous, other.previous) == 0 &&
           MathUtil::javaDoubleCompare(current, other.current) == 0;
}

FlightEvent::FlightEvent(Type type, double time)
  : FlightEvent(type, time, nullptr, Data{}, std::nullopt)
{
}

FlightEvent::FlightEvent(Type type, double time, const RocketComponent* source)
  : FlightEvent(type, time, source, Data{}, std::nullopt)
{
}

FlightEvent::FlightEvent(const FlightEvent& sourceEvent, const RocketComponent* source, Data data)
  : FlightEvent(sourceEvent.m_type, sourceEvent.m_time, source, std::move(data), std::nullopt)
{
}

FlightEvent::FlightEvent(Type type, double time, const RocketComponent* source, Data data)
  : FlightEvent(type, time, source, std::move(data), std::nullopt)
{
}

FlightEvent::FlightEvent(Type type, double time, const RocketComponent* source, Data data,
                         std::optional<Uuid> id)
  : FlightEvent(Unchecked{}, type, time, source,
                source != nullptr ? std::optional<Uuid>{source->getId()} : std::nullopt,
                std::move(data), id)
{
    validate();
}

FlightEvent::FlightEvent(Type type, double time, const Uuid& sourceId, Data data,
                         std::optional<Uuid> id)
  : FlightEvent(Unchecked{}, type, time, nullptr, sourceId, std::move(data), id)
{
    validate();
}

FlightEvent::FlightEvent(Unchecked /*tag*/, Type type, double time, const RocketComponent* source,
                         std::optional<Uuid> sourceId, Data data, std::optional<Uuid> id)
  : m_id(id.has_value() ? *id : Uuid::random()),
    m_type(type),
    m_time(time),
    m_source(source),
    m_sourceId(sourceId),
    m_data(normalized(std::move(data)))
{
}

Result<FlightEvent> FlightEvent::create(Type type, double time, const RocketComponent* source,
                                        Data data, std::optional<Uuid> id)
{
    FlightEvent event(Unchecked{}, type, time, source,
                      source != nullptr ? std::optional<Uuid>{source->getId()} : std::nullopt,
                      std::move(data), id);
    if (std::optional<std::string> error = event.validationError())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::move(*error));
    }
    return event;
}

Result<FlightEvent> FlightEvent::create(Type type, double time, const Uuid& sourceId, Data data,
                                        std::optional<Uuid> id)
{
    FlightEvent event(Unchecked{}, type, time, nullptr, sourceId, std::move(data), id);
    if (std::optional<std::string> error = event.validationError())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::move(*error));
    }
    return event;
}

FlightEvent::Data FlightEvent::warningData(const Warning& warning)
{
    // unique_ptr<Message> to shared_ptr, then down to the Warning it is.
    const std::shared_ptr<const Message> copy  = warning.clone();
    std::shared_ptr<const Warning>       typed = std::dynamic_pointer_cast<const Warning>(copy);
    if (typed == nullptr)
    {
        bug("Warning::clone() returned a message that is not a warning");
    }
    return Data{std::move(typed)};
}

std::shared_ptr<MotorClusterState> FlightEvent::getMotorState() const
{
    const auto* state = std::get_if<std::shared_ptr<MotorClusterState>>(&m_data);
    return state != nullptr ? *state : nullptr;
}

std::shared_ptr<const Warning> FlightEvent::getWarning() const
{
    const auto* warning = std::get_if<std::shared_ptr<const Warning>>(&m_data);
    return warning != nullptr ? *warning : nullptr;
}

const SimulationAbort* FlightEvent::getAbort() const noexcept
{
    return std::get_if<SimulationAbort>(&m_data);
}

std::optional<FlightEvent::AltitudeChange> FlightEvent::getAltitudeChange() const noexcept
{
    const AltitudeChange* change = std::get_if<AltitudeChange>(&m_data);
    return change != nullptr ? std::optional<AltitudeChange>{*change} : std::nullopt;
}

const std::string* FlightEvent::getMessage() const noexcept
{
    return std::get_if<std::string>(&m_data);
}

int FlightEvent::compareTo(const FlightEvent& other) const
{
    // first, sort on time
    if (m_time < other.m_time)
    {
        return -1;
    }
    if (m_time > other.m_time)
    {
        return 1;
    }

    // second, sort on stage presence.  Events with no source go first
    if (m_source == nullptr && other.m_source != nullptr)
    {
        return -1;
    }
    if (m_source != nullptr && other.m_source == nullptr)
    {
        return 1;
    }

    // third, sort on stage order.  Bigger stage number goes first
    if (m_source != nullptr && other.m_source != nullptr)
    {
        const int stage      = m_source->getStageNumber();
        const int otherStage = other.m_source->getStageNumber();
        if (stage > otherStage)
        {
            return -1;
        }
        if (stage < otherStage)
        {
            return 1;
        }
    }

    // finally, sort on event type
    return ordinal(m_type) - ordinal(other.m_type);
}

bool FlightEvent::equals(const FlightEvent& other) const
{
    if (m_type == Type::SIM_WARN && other.m_type == Type::SIM_WARN)
    {
        const std::shared_ptr<const Warning> warning      = getWarning();
        const std::shared_ptr<const Warning> otherWarning = other.getWarning();
        // validate() guarantees both warnings.
        QTROCKET_ASSERT(warning != nullptr && otherWarning != nullptr);
        return warning->equals(*otherWarning);
    }
    return compareTo(other) == 0;
}

std::string FlightEvent::toString() const
{
    std::string source = "null";
    if (m_source != nullptr)
    {
        source = m_source->toString();
    }
    else if (m_sourceId.has_value())
    {
        source = m_sourceId->toString();
    }
    return "FlightEvent[type=" + std::string{name(m_type)} +
           ",time=" + Strings::javaDoubleToString(m_time) + ",source=" + source +
           ",data=" + dataToString() + "]";
}

void FlightEvent::validate() const
{
    if (const std::optional<std::string> error = validationError())
    {
        bug(*error);
    }
}

std::optional<std::string> FlightEvent::validationError() const
{
    if (std::isnan(m_time))
    {
        return std::string{name(m_type)} + " event has a NaN time!";
    }
    if (std::optional<std::string> error = sourceError())
    {
        return error;
    }
    return dataError();
}

std::optional<std::string> FlightEvent::sourceError() const
{
    const std::string typeName{name(m_type)};
    switch (m_type)
    {
        case Type::BURNOUT:
        case Type::IGNITION:
            if (m_source != nullptr && dynamic_cast<const MotorMount*>(m_source) == nullptr)
            {
                return typeName + " events should have MotorMount type data payloads, instead of " +
                       std::string{className(m_source->kind())};
            }
            break;
        case Type::EJECTION_CHARGE:
            if (m_source != nullptr && dynamic_cast<const AxialStage*>(m_source) == nullptr)
            {
                return typeName + " events should have AxialStage type data payloads, instead of " +
                       std::string{className(m_source->kind())};
            }
            break;
        case Type::SIM_WARN:
            // rather than making event sources take sets of components, or trying to keep them
            // in sync with the sources of Warnings, we'll require the event source to be null
            // and pull the actual sources from the Warning
            if (m_source != nullptr)
            {
                return typeName + " event requires null source component; was " +
                       m_source->toString();
            }
            if (m_sourceId.has_value())
            {
                return typeName + " event requires null source component; was " +
                       m_sourceId->toString();
            }
            break;
        case Type::LAUNCH:
        case Type::LIFTOFF:
        case Type::LAUNCHROD:
        case Type::STAGE_SEPARATION:
        case Type::APOGEE:
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
        case Type::GROUND_HIT:
        case Type::SIMULATION_END:
        case Type::ALTITUDE:
        case Type::TUMBLE:
        case Type::SIM_ABORT:
        case Type::EXCEPTION:
            break;
    }
    return std::nullopt;
}

std::optional<std::string> FlightEvent::dataError() const
{
    const std::string typeName{name(m_type)};
    const bool isMotorState = std::holds_alternative<std::shared_ptr<MotorClusterState>>(m_data);
    switch (m_type)
    {
        case Type::BURNOUT:
        case Type::EJECTION_CHARGE:
            if (hasData() && !isMotorState)
            {
                return typeName + " events should have MotorClusterState type data payloads";
            }
            break;
        case Type::IGNITION:
            if (hasData() && !isMotorState)
            {
                // Java's text has no space after the type's name here.
                return typeName + "events should have MotorClusterState type data payloads";
            }
            break;
        case Type::SIM_WARN:
            if (!std::holds_alternative<std::shared_ptr<const Warning>>(m_data))
            {
                return typeName + " events require Warning objects";
            }
            break;
        case Type::SIM_ABORT:
            if (!std::holds_alternative<SimulationAbort>(m_data))
            {
                return typeName + " events require SimulationAbort objects";
            }
            break;
        case Type::LAUNCH:
        case Type::LIFTOFF:
        case Type::LAUNCHROD:
        case Type::STAGE_SEPARATION:
        case Type::APOGEE:
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
        case Type::GROUND_HIT:
        case Type::SIMULATION_END:
        case Type::ALTITUDE:
        case Type::TUMBLE:
        case Type::EXCEPTION:
            break;
    }
    return std::nullopt;
}

std::string FlightEvent::dataToString() const
{
    return std::visit(DataPrinter{}, m_data);
}

std::string_view name(FlightEvent::Type type) noexcept
{
    switch (type)
    {
        case Type::LAUNCH:
            return "LAUNCH";
        case Type::IGNITION:
            return "IGNITION";
        case Type::LIFTOFF:
            return "LIFTOFF";
        case Type::LAUNCHROD:
            return "LAUNCHROD";
        case Type::BURNOUT:
            return "BURNOUT";
        case Type::EJECTION_CHARGE:
            return "EJECTION_CHARGE";
        case Type::STAGE_SEPARATION:
            return "STAGE_SEPARATION";
        case Type::APOGEE:
            return "APOGEE";
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return "RECOVERY_DEVICE_DEPLOYMENT";
        case Type::GROUND_HIT:
            return "GROUND_HIT";
        case Type::SIMULATION_END:
            return "SIMULATION_END";
        case Type::ALTITUDE:
            return "ALTITUDE";
        case Type::TUMBLE:
            return "TUMBLE";
        case Type::SIM_WARN:
            return "SIM_WARN";
        case Type::SIM_ABORT:
            return "SIM_ABORT";
        case Type::EXCEPTION:
            break;
    }
    return "EXCEPTION";
}

std::optional<FlightEvent::Type> flightEventTypeFromName(std::string_view name) noexcept
{
    for (const Type type : FlightEvent::kAllTypes)
    {
        if (QtRocket::name(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view orkName(FlightEvent::Type type) noexcept
{
    switch (type)
    {
        case Type::LAUNCH:
            return "launch";
        case Type::IGNITION:
            return "ignition";
        case Type::LIFTOFF:
            return "liftoff";
        case Type::LAUNCHROD:
            return "launchrod";
        case Type::BURNOUT:
            return "burnout";
        case Type::EJECTION_CHARGE:
            return "ejectioncharge";
        case Type::STAGE_SEPARATION:
            return "stageseparation";
        case Type::APOGEE:
            return "apogee";
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return "recoverydevicedeployment";
        case Type::GROUND_HIT:
            return "groundhit";
        case Type::SIMULATION_END:
            return "simulationend";
        case Type::ALTITUDE:
            return "altitude";
        case Type::TUMBLE:
            return "tumble";
        case Type::SIM_WARN:
            return "simwarn";
        case Type::SIM_ABORT:
            return "simabort";
        case Type::EXCEPTION:
            break;
    }
    return "exception";
}

std::optional<FlightEvent::Type> flightEventTypeFromOrkName(std::string_view text)
{
    for (const Type type : FlightEvent::kAllTypes)
    {
        if (Strings::orkEnumNameMatches(text, name(type)))
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(FlightEvent::Type type) noexcept
{
    switch (type)
    {
        case Type::LAUNCH:
            return "FlightEvent.Type.LAUNCH";
        case Type::IGNITION:
            return "FlightEvent.Type.IGNITION";
        case Type::LIFTOFF:
            return "FlightEvent.Type.LIFTOFF";
        case Type::LAUNCHROD:
            return "FlightEvent.Type.LAUNCHROD";
        case Type::BURNOUT:
            return "FlightEvent.Type.BURNOUT";
        case Type::EJECTION_CHARGE:
            return "FlightEvent.Type.EJECTION_CHARGE";
        case Type::STAGE_SEPARATION:
            return "FlightEvent.Type.STAGE_SEPARATION";
        case Type::APOGEE:
            return "FlightEvent.Type.APOGEE";
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return "FlightEvent.Type.RECOVERY_DEVICE_DEPLOYMENT";
        case Type::GROUND_HIT:
            return "FlightEvent.Type.GROUND_HIT";
        case Type::SIMULATION_END:
            return "FlightEvent.Type.SIMULATION_END";
        case Type::ALTITUDE:
            return "FlightEvent.Type.ALTITUDE";
        case Type::TUMBLE:
            return "FlightEvent.Type.TUMBLE";
        case Type::SIM_WARN:
            return "FlightEvent.Type.SIM_WARN";
        case Type::SIM_ABORT:
            return "FlightEvent.Type.SIM_ABORT";
        case Type::EXCEPTION:
            break;
    }
    return "FlightEvent.Type.EXCEPTION";
}

std::string_view displayName(FlightEvent::Type type) noexcept
{
    switch (type)
    {
        case Type::LAUNCH:
            return "Launch";
        case Type::IGNITION:
            return "Motor ignition";
        case Type::LIFTOFF:
            return "Lift-off";
        case Type::LAUNCHROD:
            return "Launch rod clearance";
        case Type::BURNOUT:
            return "Motor burnout";
        case Type::EJECTION_CHARGE:
            return "Ejection charge";
        case Type::STAGE_SEPARATION:
            return "Stage separation";
        case Type::APOGEE:
            return "Apogee";
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return "Recovery device deployment";
        case Type::GROUND_HIT:
            return "Ground hit";
        case Type::SIMULATION_END:
            return "Simulation end";
        case Type::ALTITUDE:
            return "Altitude change";
        case Type::TUMBLE:
            return "Tumbling";
        case Type::SIM_WARN:
            return "Warning";
        case Type::SIM_ABORT:
            return "Simulation abort";
        case Type::EXCEPTION:
            break;
    }
    return "Exception";
}

}  // namespace QtRocket
