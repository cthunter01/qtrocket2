#include "QtRocket/file/openrocket/OpenRocketLoader.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/OpenRocketHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// What the handlers ask of the context whenever a file has the element for it.
void requireEnvironment(const DocumentLoadingContext& context)
{
    if (context.getOpenRocketDocument() == nullptr)
    {
        bug("The loading context of the loader has no document");
    }
    if (context.getMotorFinder() == nullptr)
    {
        bug("The loading context of the loader has no motor finder");
    }
    if (context.getPreferences() == nullptr)
    {
        bug("The loading context of the loader has no preference store");
    }
}

/// The failure of the load for the failure @p error of reading the document.
[[nodiscard]] std::unexpected<Error> readFailure(Error error)
{
    switch (error.code)
    {
        case ErrorCode::PARSE:
            // Java: the SAXException.
            return fail(ErrorCode::PARSE, "Malformed XML in input.");
        case ErrorCode::IO:
            // Java: the IOException of an encoding the parser does not know
            // (AbstractRocketLoader.load()).
            return fail(ErrorCode::IO, "I/O error: " + error.message);
        default:
            // A handler's failure (Java: the IllegalArgumentException, which leaves the loader),
            // and the document type declaration that XmlScanner does not read.
            return std::unexpected(std::move(error));
    }
}

/// Whether the stored data of @p simulation is worth saving again: its first branch has a
/// time column.
[[nodiscard]] bool hasStoredFlight(const Simulation& simulation)
{
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        return false;
    }
    if (data->getBranchCount() == 0)
    {
        return false;
    }
    // Java: branch.get(TYPE_TIME) is null when the branch has no such column.
    return data->getBranch(0).hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
}

}  // namespace

Result<void> OpenRocketLoader::load(DocumentLoadingContext&    context,
                                    std::span<const std::byte> source, WarningSet& warnings)
{
    requireEnvironment(context);
    OpenRocketDocument& doc = *context.getOpenRocketDocument();

    OpenRocketHandler handler(context);
    if (Result<void> read = SimpleSax::readXml(source, handler, warnings); !read)
    {
        return readFailure(std::move(read.error()));
    }

    // load the stage activeness
    for (FlightConfiguration& config : doc.getRocket().getFlightConfigurations().values())
    {
        config.applyPreloadedStageActiveness();
    }

    // If we saved data for a simulation before, we'll use that as our default option this time.
    // Also, update all the sims' modIDs to agree with flight config
    for (const std::shared_ptr<Simulation>& s : doc.getSimulations())
    {
        // The config's modID can be out of sync with the simulation's after the whole loading
        // process
        s->syncModId();
        if (s->getStatus() == Simulation::Status::EXTERNAL ||
            s->getStatus() == Simulation::Status::NOT_SIMULATED)
        {
            continue;
        }
        if (!hasStoredFlight(*s))
        {
            continue;
        }
        doc.getDefaultStorageOptions().setSaveSimulationData(true);
    }

    doc.getDefaultStorageOptions().setExplicitlySet(false);
    doc.getDefaultStorageOptions().setFileType(StorageOptions::FileType::OPENROCKET);

    // Call simulation extensions
    for (const std::shared_ptr<Simulation>& sim : doc.getSimulations())
    {
        // A copy of the list: an extension may change the simulation it is told of.
        const std::vector<std::shared_ptr<SimulationExtension>> extensions =
            sim->getSimulationExtensions();
        for (const std::shared_ptr<SimulationExtension>& ext : extensions)
        {
            ext->documentLoaded(doc, *sim, warnings);
        }
    }

    doc.clearUndo();
    return {};
}

}  // namespace QtRocket
