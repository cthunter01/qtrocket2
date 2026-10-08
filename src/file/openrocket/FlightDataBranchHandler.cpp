#include "QtRocket/file/openrocket/FlightDataBranchHandler.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// A name a type had before its symbol was added to it, and the type.
struct LegacyName
{
    std::string_view name;
    FlightDataTypeId type;
};

/// Legacy English name mappings: type names were updated to include symbol suffixes (e.g. "Drag
/// coefficient" -> "Drag coefficient (CD)"). Maps names from files saved before that rename so
/// they still load correctly.
constexpr std::array<LegacyName, 10> kLegacyNames{{
    {.name = "Drag coefficient", .type = FlightDataTypeId::TYPE_DRAG_COEFF},
    {.name = "Axial drag coefficient", .type = FlightDataTypeId::TYPE_AXIAL_DRAG_COEFF},
    {.name = "Friction drag coefficient", .type = FlightDataTypeId::TYPE_FRICTION_DRAG_COEFF},
    {.name = "Pressure drag coefficient", .type = FlightDataTypeId::TYPE_PRESSURE_DRAG_COEFF},
    {.name = "Base drag coefficient", .type = FlightDataTypeId::TYPE_BASE_DRAG_COEFF},
    {.name = "Normal force coefficient", .type = FlightDataTypeId::TYPE_NORMAL_FORCE_COEFF},
    {.name = "Pitch moment coefficient", .type = FlightDataTypeId::TYPE_PITCH_MOMENT_COEFF},
    {.name = "Roll rate", .type = FlightDataTypeId::TYPE_ROLL_RATE},
    {.name = "Pitch rate", .type = FlightDataTypeId::TYPE_PITCH_RATE},
    {.name = "Yaw rate", .type = FlightDataTypeId::TYPE_YAW_RATE},
}};

/// The attribute @p name of an element, or none (Java: attributes.get(name), null when absent).
[[nodiscard]] std::optional<std::string_view> attribute(
    const ElementHandler::Attributes& attributes, std::string_view name)
{
    const auto found = attributes.find(name);
    if (found == attributes.end())
    {
        return std::nullopt;
    }
    return std::string_view{found->second};
}

/// UUID.fromString() where Java lets its IllegalArgumentException fail the load.
[[nodiscard]] Result<Uuid> parseUuid(std::string_view text)
{
    Result<Uuid> parsed = Uuid::javaFromString(text);
    if (!parsed)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::move(parsed.error().message));
    }
    return parsed;
}

/// UUID.fromString() of the attribute @p name when the element has it; no id when it has not.
[[nodiscard]] Result<std::optional<Uuid>> parseUuidAttribute(
    const ElementHandler::Attributes& attributes, std::string_view name)
{
    const std::optional<std::string_view> text = attribute(attributes, name);
    if (!text.has_value())
    {
        return std::optional<Uuid>{};
    }
    const Result<Uuid> id = parseUuid(*text);
    if (!id)
    {
        return std::unexpected(id.error());
    }
    return std::optional<Uuid>{*id};
}

/// Where an event comes from, as an <event> element says it.
struct EventSource
{
    /// The id of the source attribute, or none.
    std::optional<Uuid> id;
    /// The component of the rocket with that id; null when there is none (and without an id).
    const RocketComponent* component{nullptr};
};

/// The event a file describes, checked as Java's constructor checks it, or the message of the
/// check that failed. The event keeps the id of its source only (FlightEvent::createDetached()).
[[nodiscard]] Result<FlightEvent> makeEvent(FlightEvent::Type type, double time,
                                            const EventSource& source, FlightEvent::Data data,
                                            std::optional<Uuid> id)
{
    if (source.component != nullptr || !source.id.has_value())
    {
        return FlightEvent::createDetached(type, time, source.component, std::move(data), id);
    }

    // An id that names no component: Java's removed component, of which FlightEvent checks
    // that it is there and not what it is.
    Result<FlightEvent> event = FlightEvent::create(type, time, *source.id, std::move(data), id);
    if (!event)
    {
        // FlightEvent names a source it knows by its id only by that id; Java's message has
        // the name of the removed component there.
        std::string&      message = event.error().message;
        const std::string idText  = source.id->toString();
        if (message.ends_with(idText))
        {
            message.replace(message.size() - idText.size(), idText.size(),
                            MessageSource::kRemovedComponentName);
        }
    }
    return event;
}

/// The source attribute of an <event> element and the component of @p rocket it names. Fails
/// when the attribute is no UUID.
[[nodiscard]] Result<EventSource> readEventSource(const ElementHandler::Attributes& attributes,
                                                  const Rocket&                     rocket)
{
    const Result<std::optional<Uuid>> id = parseUuidAttribute(attributes, "source");
    if (!id)
    {
        return std::unexpected(id.error());
    }
    EventSource source{.id = *id};
    if (source.id.has_value())
    {
        source.component = rocket.findComponent(*source.id);
    }
    return source;
}

/// The data of the event a file describes, and the warning of the flight data it stands for.
struct EventData
{
    FlightEvent::Data data;
    /// The warning of the flight data's set that is the event's data; null when the data is
    /// something else.
    Warning* warning{nullptr};
};

/// What an <event> element of the type @p type carries: for a SIM_WARN event the warning of
/// @p simulationWarnings its warnid names, and for an event of any type the abort its cause
/// names, which takes the place of the warning. Fails when the warnid of a SIM_WARN event is no
/// UUID.
[[nodiscard]] Result<EventData> readEventData(FlightEvent::Type                 type,
                                              const ElementHandler::Attributes& attributes,
                                              WarningSet&                       simulationWarnings)
{
    EventData eventData;

    // For warning events, get the warning
    if (type == FlightEvent::Type::SIM_WARN)
    {
        const Result<std::optional<Uuid>> warnId = parseUuidAttribute(attributes, "warnid");
        if (!warnId)
        {
            return std::unexpected(warnId.error());
        }
        if (warnId->has_value())
        {
            eventData.warning = simulationWarnings.findById(**warnId);
        }
        if (eventData.warning != nullptr)
        {
            eventData.data = FlightEvent::warningData(*eventData.warning);
        }
    }

    // For aborts, get the cause
    const std::optional<SimulationAbort::Cause> cause = DocumentConfig::findEnum(
        attribute(attributes, "cause"), SimulationAbort::kAllCauses, &causeName);
    if (cause.has_value())
    {
        eventData.data    = SimulationAbort(*cause);
        eventData.warning = nullptr;
    }
    return eventData;
}

/// Gives @p landing the event with the id @p eventId, or none when there is no such event
/// (Java: setEvent(branch.findEvent(eventID)), called when the SIM_WARN event that is being
/// read is already among the branch's events). The event is the first of @p branch with that
/// id, else @p newEvent, the event being read (null when it was dropped), when it has that id.
void setEventOfWarning(Warning::EventAfterLanding& landing, const Uuid& eventId,
                       const FlightDataBranch& branch, const FlightEvent* newEvent)
{
    const FlightEvent* target = branch.findEvent(eventId);
    if (target == nullptr && newEvent != nullptr && newEvent->getId() == eventId)
    {
        target = newEvent;
    }
    if (target != nullptr)
    {
        landing.setEvent(std::string{displayName(target->getType())}, target->getId());
    }
    else
    {
        landing.setEvent(std::nullopt, std::nullopt);
    }
}

}  // namespace

Result<std::unique_ptr<FlightDataBranchHandler>> FlightDataBranchHandler::create(
    std::string name, std::string_view typeList, WarningSet& simulationWarnings,
    const DocumentLoadingContext& context)
{
    const OpenRocketDocument* const document = context.getOpenRocketDocument();
    if (document == nullptr)
    {
        bug("the loading context of a flight data branch has no document");
    }

    std::vector<const FlightDataType*> types;
    for (const std::string& typeName : Strings::splitJava(typeList, ','))
    {
        types.push_back(&findFlightDataType(typeName, *document));
    }

    // What the constructor of Java's branch throws; FlightDataBranch's own would throw BugError.
    if (types.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Must specify at least one data type.");
    }
    // The types before a type, by their hash codes (equal types have equal hash codes).
    std::unordered_multimap<int, const FlightDataType*> seen;
    for (const FlightDataType* type : types)
    {
        const auto [first, last] = seen.equal_range(type->hashCode());
        if (std::ranges::any_of(first, last,
                                [type](const auto& entry) { return entry.second->equals(*type); }))
        {
            return fail(ErrorCode::INVALID_ARGUMENT,
                        "Value type " + type->getName() + " already exists.");
        }
        seen.emplace(type->hashCode(), type);
    }

    return std::make_unique<FlightDataBranchHandler>(Passkey{}, std::move(name), std::move(types),
                                                     simulationWarnings, context);
}

FlightDataBranchHandler::FlightDataBranchHandler(Passkey /*passkey*/, std::string name,
                                                 std::vector<const FlightDataType*> types,
                                                 WarningSet&                   simulationWarnings,
                                                 const DocumentLoadingContext& context)
  : m_context(&context),
    m_simulationWarnings(&simulationWarnings),
    m_types(std::move(types)),
    m_branch(std::make_shared<FlightDataBranch>(std::move(name),
                                                std::span<const FlightDataType* const>(m_types)))
{
}

// Find the full flight data type given the value stored in the XML file.
// Note: this way of doing it requires that custom expressions always come before flight data in
// the file, not the nicest but this is always the case anyway.
const FlightDataType& FlightDataBranchHandler::findFlightDataType(
    std::string_view name, const OpenRocketDocument& document)
{
    // 1. Try the stable save key (files saved with the current format store e.g. "drag_coeff")
    if (const FlightDataType* const byKey = FlightDataType::getTypeBySaveKey(name))
    {
        return *byKey;
    }

    // 2. Try matching by display name (files saved before save keys were introduced)
    if (const FlightDataType* const byName = FlightDataType::findByName(name))
    {
        return *byName;
    }

    // 3. Replace deprecated 'Position upwind' with new 'Position North of launch' option
    if (name == FlightDataType::kLegacyUpwindName)
    {
        return FlightDataType::builtin(FlightDataTypeId::TYPE_POSITION_Y);
    }

    // 4. Legacy English name mappings
    for (const LegacyName& legacy : std::span<const LegacyName>(kLegacyNames))
    {
        if (name == legacy.name)
        {
            return FlightDataType::builtin(legacy.type);
        }
    }

    // 5. Look in custom expressions
    for (const CustomExpression& expression : document.getCustomExpressions())
    {
        if (expression.getName() == name)
        {
            return expression.getType();
        }
    }

    // Could not find the flight data type used in the XML file: substituted with a type with
    // an unknown symbol and no units.
    return FlightDataType::getType(name, "Unknown", UnitGroupId::NONE);
}

void FlightDataBranchHandler::setTimeToOptimumAltitude(double timeToOptimumAltitude) noexcept
{
    m_branch->setTimeToOptimumAltitude(timeToOptimumAltitude);
}

void FlightDataBranchHandler::setOptimumAltitude(double optimumAltitude) noexcept
{
    m_branch->setOptimumAltitude(optimumAltitude);
}

std::shared_ptr<FlightDataBranch> FlightDataBranchHandler::getBranch()
{
    m_branch->immute();
    return m_branch;
}

Result<ElementHandler*> FlightDataBranchHandler::openElement(std::string_view element,
                                                             const Attributes& /*attributes*/,
                                                             WarningSet& warnings)
{
    if (element == "datapoint" || element == "event")
    {
        return &PlainTextHandler::instance();
    }

    warnings.add("Unknown element '" + std::string(element) + "' encountered, ignoring.");
    return nullptr;
}

Result<void> FlightDataBranchHandler::closeElement(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    if (element == "event")
    {
        return closeEvent(attributes, warnings);
    }

    if (element != "datapoint")
    {
        warnings.add("Unknown element '" + std::string(element) + "' encountered, ignoring.");
        return {};
    }

    closeDatapoint(content, warnings);
    return {};
}

Result<void> FlightDataBranchHandler::closeEvent(const Attributes& attributes, WarningSet& warnings)
{
    const Result<double> time = DocumentConfig::stringToDouble(attribute(attributes, "time"));
    if (!time)
    {
        warnings.add("Illegal event time specification, ignoring: " + time.error().message);
        return {};
    }

    const std::optional<FlightEvent::Type> type =
        DocumentConfig::findEnum(attribute(attributes, "type"), FlightEvent::kAllTypes,
                                 [](FlightEvent::Type constant) { return name(constant); });
    if (!type.has_value())
    {
        warnings.add("Illegal event specification, ignoring.");
        return {};
    }

    // Get the event ID
    const Result<std::optional<Uuid>> id = parseUuidAttribute(attributes, "id");
    if (!id)
    {
        return std::unexpected(id.error());
    }

    // Get the event source
    const Result<EventSource> source =
        readEventSource(attributes, m_context->getOpenRocketDocument()->getRocket());
    if (!source)
    {
        return std::unexpected(source.error());
    }

    // The warning of a warning event, or the abort of the cause
    const Result<EventData> eventData = readEventData(*type, attributes, *m_simulationWarnings);
    if (!eventData)
    {
        return std::unexpected(eventData.error());
    }

    Result<FlightEvent> event = makeEvent(*type, *time, *source, eventData->data, *id);
    if (!event)
    {
        warnings.add("Illegal parameters for FlightEvent: " + event.error().message);
    }

    // For EventAfterLanding warning events, hook the event up to the warning if possible. Java
    // does this after it has added the event, which shares the warning object. Here the event
    // holds a copy of the warning, so the warning is changed first and the event made anew
    // with it.
    auto* const landing = dynamic_cast<Warning::EventAfterLanding*>(eventData->warning);
    const std::optional<std::string_view> eventIdText = attribute(attributes, "eventid");
    if (landing != nullptr && eventIdText.has_value())
    {
        const Result<Uuid> eventId = parseUuid(*eventIdText);
        if (!eventId)
        {
            return std::unexpected(eventId.error());
        }
        setEventOfWarning(*landing, *eventId, *m_branch, event.has_value() ? &*event : nullptr);
        if (event.has_value())
        {
            event = makeEvent(*type, *time, *source, FlightEvent::warningData(*landing),
                              event->getId());
        }
    }

    if (event.has_value())
    {
        m_branch->addEvent(std::move(*event));
    }
    return {};
}

void FlightDataBranchHandler::closeDatapoint(std::string_view content, WarningSet& warnings)
{
    // Check line format
    const std::vector<std::string> split = Strings::splitJava(content, ',');
    if (split.size() != m_types.size())
    {
        warnings.add("Data point did not contain correct amount of values, ignoring point.");
        return;
    }

    // Parse the doubles
    std::vector<double> values;
    values.reserve(split.size());
    for (const std::string& text : split)
    {
        const Result<double> value = DocumentConfig::stringToDouble(text);
        if (!value)
        {
            warnings.add("Data point format error, ignoring point.");
            return;
        }
        values.push_back(*value);
    }

    // Add point to branch
    m_branch->addPoint();
    for (std::size_t i = 0; i < m_types.size(); i++)
    {
        m_branch->setValue(*m_types[i], values[i]);
    }
}

}  // namespace QtRocket
