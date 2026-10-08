#include "QtRocket/file/openrocket/FlightDataHandler.h"

#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/FlightDataBranchHandler.h"
#include "QtRocket/file/openrocket/WarningHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The attributes of a <flightdata> element that are the summary values of a flight, in the
/// order of FlightData's constructor.
constexpr std::array<std::string_view, 10> kSummaryAttributes{
    "maxaltitude",        "maxvelocity", "maxacceleration",   "maxmach",
    "timetoapogee",       "flighttime",  "groundhitvelocity", "launchrodvelocity",
    "deploymentvelocity", "optimumdelay"};

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

}  // namespace

FlightDataHandler::FlightDataHandler(const DocumentLoadingContext& context) : m_context(&context)
{
    if (context.getOpenRocketDocument() == nullptr)
    {
        bug("the loading context of a flight data handler has no document");
    }
}

Result<ElementHandler*> FlightDataHandler::openElement(std::string_view  element,
                                                       const Attributes& attributes,
                                                       WarningSet&       warnings)
{
    if (element == "warning")
    {
        m_warningHandler = std::make_unique<WarningHandler>(
            m_context->getOpenRocketDocument()->getRocket(), m_warningSet);
        return m_warningHandler.get();
    }
    if (element == "databranch")
    {
        const std::optional<std::string_view> name  = attribute(attributes, "name");
        const std::optional<std::string_view> types = attribute(attributes, "types");
        if (!name.has_value() || !types.has_value())
        {
            warnings.add("Illegal flight data definition, ignoring.");
            return nullptr;
        }
        // A type list that Java's branch refuses fails the load.
        Result<std::unique_ptr<FlightDataBranchHandler>> handler =
            FlightDataBranchHandler::create(std::string(*name), *types, m_warningSet, *m_context);
        if (!handler)
        {
            return std::unexpected(std::move(handler.error()));
        }
        m_dataHandler = std::move(*handler);

        if (const std::optional<std::string_view> text = attribute(attributes, "optimumAltitude"))
        {
            m_dataHandler->setOptimumAltitude(Strings::javaParseDouble(*text).value_or(kNaN));
        }
        if (const std::optional<std::string_view> text =
                attribute(attributes, "timeToOptimumAltitude"))
        {
            m_dataHandler->setTimeToOptimumAltitude(Strings::javaParseDouble(*text).value_or(kNaN));
        }
        return m_dataHandler.get();
    }

    warnings.add("Unknown element '" + std::string(element) + "' encountered, ignoring.");
    return nullptr;
}

Result<void> FlightDataHandler::closeElement(std::string_view element,
                                             const Attributes& /*attributes*/,
                                             std::string_view /*content*/, WarningSet& /*warnings*/)
{
    if (element == "databranch")
    {
        // closeElement() is called for the elements openElement() gave a handler for.
        QTROCKET_ASSERT(m_dataHandler != nullptr);
        std::shared_ptr<FlightDataBranch> branch = m_dataHandler->getBranch();
        if (branch->getLength() > 0)
        {
            m_branches.push_back(std::move(branch));
        }
    }
    return {};
}

Result<void> FlightDataHandler::endHandler(std::string_view /*element*/,
                                           const Attributes& attributes,
                                           std::string_view /*content*/, WarningSet& /*warnings*/)
{
    if (!m_branches.empty())
    {
        m_data = std::make_shared<FlightData>(
            std::span<const std::shared_ptr<FlightDataBranch>>(m_branches));
    }
    else
    {
        std::array<double, kSummaryAttributes.size()> summary{};
        for (std::size_t i = 0; i < summary.size(); i++)
        {
            summary.at(i) =
                DocumentConfig::stringToDouble(attribute(attributes, kSummaryAttributes.at(i)))
                    .value_or(kNaN);
        }
        m_data = std::make_shared<FlightData>(summary[0], summary[1], summary[2], summary[3],
                                              summary[4], summary[5], summary[6], summary[7],
                                              summary[8], summary[9]);
    }

    m_data->getWarningSet().addAll(m_warningSet);
    m_data->immute();
    return {};
}

}  // namespace QtRocket
