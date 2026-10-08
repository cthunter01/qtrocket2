#include "QtRocket/file/openrocket/WarningHandler.h"

#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

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

/// The warning the type attribute @p type of a <warning> element asks for, with @p parameter
/// as its number and @p text as its description.
[[nodiscard]] std::unique_ptr<Warning> makeWarning(std::string_view type, double parameter,
                                                   std::string text)
{
    if (type == "LargeAOA")
    {
        return std::make_unique<Warning::LargeAOA>(parameter);
    }
    if (type == "HighSpeedDeployment")
    {
        return std::make_unique<Warning::RecoveryHighSpeedDeployment>(parameter);
    }
    if (type == "EventAfterLanding")
    {
        return std::make_unique<Warning::EventAfterLanding>();
    }
    return std::make_unique<Warning::Other>(Warning::fromString(std::move(text)));
}

}  // namespace

WarningHandler::WarningHandler(const Rocket& rocket, WarningSet& warningSet)
  : m_rocket(&rocket), m_warningSet(&warningSet)
{
}

Result<ElementHandler*> WarningHandler::openElement(std::string_view /*element*/,
                                                    const Attributes& /*attributes*/,
                                                    WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> WarningHandler::closeElement(std::string_view element,
                                          const Attributes& /*attributes*/,
                                          std::string_view content, WarningSet& warnings)
{
    if (element == "id")
    {
        const Result<Uuid> id = parseUuid(content);
        if (!id)
        {
            return std::unexpected(id.error());
        }
        m_id = *id;
    }
    else if (element == "description")
    {
        m_warningText = std::string(Strings::trim(content));
    }
    else if (element == "priority")
    {
        m_priority = priorityFromExportLabel(content);
    }
    else if (element == "source")
    {
        const Result<Uuid> id = parseUuid(content);
        if (!id)
        {
            return std::unexpected(id.error());
        }
        // Java's findComponent() gives its removed component for an id that names none; here
        // such a source keeps the id that was read.
        const RocketComponent* const component = m_rocket->findComponent(*id);
        m_sources.push_back(component != nullptr ? MessageSource::of(*component)
                                                 : MessageSource::removed(*id));
    }
    else if (element == "parameter")
    {
        m_parameter = std::string(Strings::trim(content));
    }
    else
    {
        warnings.add("Unknown element '" + std::string(element) + "', ignoring.");
    }
    return {};
}

Result<void> WarningHandler::endHandler(std::string_view /*element*/, const Attributes& attributes,
                                        std::string_view content, WarningSet& /*warnings*/)
{
    const auto             typeAttribute = attributes.find("type");
    const std::string_view type =
        typeAttribute != attributes.end() ? std::string_view{typeAttribute->second} : "Other";

    if (!m_warningText.has_value())
    {
        m_warningText = std::string(Strings::trim(content));
    }

    double parameter = std::numeric_limits<double>::quiet_NaN();
    if (m_parameter.has_value())
    {
        // Java does not catch what Double.parseDouble throws here: the load fails.
        const Result<double> parsed = DocumentConfig::parseDouble(*m_parameter);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        parameter = *parsed;
    }

    const std::unique_ptr<Warning> warning = makeWarning(type, parameter, *m_warningText);
    if (m_id.has_value())
    {
        warning->setId(*m_id);
    }
    // Also when the element has no <priority>: the handler's NORMAL replaces the priority the
    // warning's class starts with.
    warning->setPriority(m_priority);
    warning->setSources(m_sources);

    m_warningSet->add(*warning);
    return {};
}

}  // namespace QtRocket
