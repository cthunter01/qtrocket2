#include "QtRocket/file/openrocket/SingleSimulationHandler.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/FlightDataHandler.h"
#include "QtRocket/file/openrocket/LandingDispersionSettingsHandler.h"
#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"
#include "QtRocket/file/openrocket/SimulationPlotAppearanceHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The package of OpenRocket's classes up to 23.09, and the one they are in since.
constexpr std::string_view kLegacyPackage  = "net.sf.openrocket";
constexpr std::string_view kCurrentPackage = "info.openrocket.core";

/// Java's message for the flight configuration id a simulation cannot have.
constexpr std::string_view kErrorIdMessage =
    "Attempted to set the configuration to an error id. Not Allowed!";

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

/// Java's String.replace(target, replacement): @p text with every @p target replaced, from
/// the left, without looking at what was put in.
[[nodiscard]] std::string replaceAll(std::string_view text, std::string_view target,
                                     std::string_view replacement)
{
    std::string result;
    std::size_t from = 0;
    for (std::size_t at = text.find(target); at != std::string_view::npos;
         at             = text.find(target, from))
    {
        result += text.substr(from, at - from);
        result += replacement;
        from = at + target.size();
    }
    result += text.substr(from);
    return result;
}

/// The name of a simulation status as a design file writes it.
[[nodiscard]] std::string_view statusName(Simulation::Status status) noexcept
{
    return name(status);
}

}  // namespace

SingleSimulationHandler::SingleSimulationHandler(const DocumentLoadingContext& context)
  : m_context(&context), m_document(context.getOpenRocketDocument())
{
    if (m_document == nullptr)
    {
        bug("the loading context of a simulation handler has no document");
    }
    if (context.getPreferences() == nullptr)
    {
        bug("the loading context of a simulation handler has no preference store");
    }
}

Result<ElementHandler*> SingleSimulationHandler::openElement(std::string_view  element,
                                                             const Attributes& attributes,
                                                             WarningSet&       warnings)
{
    if (element == "name" || element == "simulator" || element == "calculator" ||
        element == "listener")
    {
        return &PlainTextHandler::instance();
    }
    if (element == "conditions")
    {
        m_conditionHandler = std::make_unique<SimulationConditionsHandler>(*m_context);
        return m_conditionHandler.get();
    }
    if (element == "extension")
    {
        m_configHandler = std::make_unique<ConfigHandler>();
        return m_configHandler.get();
    }
    if (element == "flightdata")
    {
        m_dataHandler = std::make_unique<FlightDataHandler>(*m_context);
        return m_dataHandler.get();
    }
    if (element == "plotappearance")
    {
        m_plotAppearanceHandler = std::make_unique<SimulationPlotAppearanceHandler>();
        return m_plotAppearanceHandler.get();
    }
    if (element == "landingdispersion")
    {
        m_landingDispersionSettingsHandler =
            std::make_unique<LandingDispersionSettingsHandler>(attributes);
        return m_landingDispersionSettingsHandler.get();
    }
    warnings.add("Unknown element '" + std::string(element) + "', ignoring.");
    return nullptr;
}

Result<void> SingleSimulationHandler::closeElement(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    if (element == "name")
    {
        m_name = std::string(content);
    }
    else if (element == "simulator")
    {
        const std::string_view simulator = Strings::trim(content);
        if (simulator != "RK4Simulator")
        {
            warnings.add("Unknown simulator '" + std::string(simulator) + "' specified, ignoring.");
        }
    }
    else if (element == "calculator")
    {
        const std::string_view calculator = Strings::trim(content);
        if (calculator != "BarrowmanCalculator")
        {
            warnings.add("Unknown calculator '" + std::string(calculator) +
                         "' specified, ignoring.");
        }
    }
    else if (element == "listener")
    {
        const std::string_view className = Strings::trim(content);
        if (!className.empty())
        {
            // Java: compatibilityExtension().
            auto extension = std::make_shared<JavaCode>();
            extension->setClassName(className);
            m_extensions.push_back(std::move(extension));
        }
    }
    else if (element == "extension")
    {
        closeExtension(attributes, warnings);
    }
    return {};
}

void SingleSimulationHandler::closeExtension(const Attributes& attributes, WarningSet& warnings)
{
    const std::optional<std::string_view> attributeId = attribute(attributes, "extensionid");
    if (!attributeId.has_value() || Strings::isEmpty(*attributeId))
    {
        return;
    }
    // closeElement() is called for the elements openElement() gave a handler for.
    QTROCKET_ASSERT(m_configHandler != nullptr);

    const std::string id = replaceAll(*attributeId, kLegacyPackage, kCurrentPackage);

    // Java asks its injector for the providers; here they are the registry of the context.
    const SimulationExtensionRegistry* const registry = m_context->getSimulationExtensionRegistry();
    std::shared_ptr<SimulationExtension>     extension;
    if (registry != nullptr)
    {
        extension = registry->create(id);
    }
    if (extension != nullptr)
    {
        extension->setConfig(m_configHandler->getConfig());
        m_extensions.push_back(std::move(extension));
        return;
    }
    warnings.add(UnknownSimulationExtension::notFoundText(id));
    // Deviation: Java drops the extension here.
    m_extensions.push_back(
        std::make_shared<UnknownSimulationExtension>(id, m_configHandler->getConfig()));
}

Result<void> SingleSimulationHandler::endHandler(std::string_view /*element*/,
                                                 const Attributes& attributes,
                                                 std::string_view /*content*/, WarningSet& warnings)
{
    const std::optional<Simulation::Status> fileStatus = DocumentConfig::findEnum(
        attribute(attributes, "status"), Simulation::kAllStatuses, &statusName);
    if (!fileStatus.has_value())
    {
        warnings.add("Simulation status unknown, assuming outdated.");
    }
    Simulation::Status status = fileStatus.value_or(Simulation::Status::OUTDATED);

    if (m_conditionHandler == nullptr)
    {
        // Java goes on with new SimulationOptions(); the missing id then fails the load below,
        // so those options are never looked at.
        warnings.add("Simulation conditions not defined, using defaults.");
    }

    // If the simulation was saved with flight data (which may just be a summary) mark it as
    // loaded from the file else as not simulated. If outdated data was saved, it'll be marked
    // as outdated (creating a new status for "loaded but outdated" seems excessive, and the
    // fact that it's outdated is the more important)
    std::shared_ptr<FlightData> data;
    if (m_dataHandler != nullptr)
    {
        data = m_dataHandler->getFlightData();
    }

    if (data == nullptr)
    {
        status = Simulation::Status::NOT_SIMULATED;
    }
    else if (status != Simulation::Status::OUTDATED)
    {
        status = Simulation::Status::LOADED;
    }

    // Java's Simulation.setFlightConfigurationId() throws an IllegalArgumentException for the
    // error id, which fails the load: after the constructor for the id of the file (the error
    // id without a <conditions> element), and inside the constructor for the id of the rocket's
    // selected configuration. Here both are a BugError, so they are asked about before the
    // simulation is made (decision L8).
    Rocket& rocket = m_document->getRocket();
    if (m_conditionHandler == nullptr || m_conditionHandler->getIdToSet().hasError() ||
        rocket.getSelectedConfiguration().getFlightConfigurationId().hasError())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string(kErrorIdMessage));
    }
    const FlightConfigurationId idToSet = m_conditionHandler->getIdToSet();

    std::map<std::string, PlotAppearance> plotAppearances;
    if (m_plotAppearanceHandler != nullptr)
    {
        plotAppearances = m_plotAppearanceHandler->getPlotAppearances();
    }

    // The options are the conditions handler's own object in Java, handed over as it is.
    const std::shared_ptr<Simulation> simulation =
        std::make_shared<Simulation>(m_document, rocket, status, m_name.value_or("Simulation"),
                                     std::move(m_conditionHandler->getConditions()), m_extensions,
                                     std::move(data), plotAppearances, m_context->getPreferences());
    simulation->setFlightConfigurationId(idToSet);
    if (m_landingDispersionSettingsHandler != nullptr)
    {
        simulation->setLandingDispersionSettings(m_landingDispersionSettingsHandler->getSettings());
    }

    m_document->addSimulation(simulation);
    m_simulation = simulation;
    return {};
}

}  // namespace QtRocket
