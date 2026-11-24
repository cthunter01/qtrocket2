#include "QtRocket/gui/util/SimulationStatusText.h"

#include <cstddef>
#include <memory>
#include <string>

#include <QString>

#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The text of @p abort: its cause's translated text with the abort's sources, as
/// Message::toString() appends them to the English text (Java: abortEvent.getData().toString()).
[[nodiscard]] QString abortText(const SimulationAbort& abort)
{
    const std::string english = abort.messageDescription();
    const std::string full    = abort.toString();
    // toString() is the description followed by the sources' names (or the description alone).
    const std::string sources = full.substr(english.size());
    return translatedEnumText(displayKey(abort.cause()), displayName(abort.cause())) +
           QtStrings::fromUtf8(sources);
}

}  // namespace

QString SimulationStatusText::description(Simulation& simulation)
{
    const Simulation::Status status = simulation.getStatus();
    if (status != Simulation::Status::ABORTED)
    {
        return translatedEnumText(descriptionKey(status), QtRocket::description(status));
    }

    const QString text = translatedEnumText(descriptionKey(status), QtRocket::description(status));
    QString       builder;

    // We'll put every abort event on a new line (note that more than one branch can abort)
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data != nullptr)
    {
        const FlightData& simulated = *data;
        for (std::size_t b = 0; b < simulated.getBranchCount(); b++)
        {
            const FlightEvent* abortEvent =
                simulated.getBranch(b).getFirstEvent(FlightEvent::Type::SIM_ABORT);
            if (abortEvent != nullptr && abortEvent->getAbort() != nullptr)
            {
                builder += text + QStringLiteral("<i>: ") + abortText(*abortEvent->getAbort()) +
                           QStringLiteral("</i><br>");
            }
        }
    }

    // It shouldn't be possible to abort without an abort event. But just in case...
    return builder.isEmpty() ? text : builder;
}

QString SimulationStatusText::plainDescription(Simulation& simulation)
{
    return QtStrings::fromUtf8(Strings::removeHtmlTags(QtStrings::toUtf8(description(simulation))));
}

}  // namespace QtRocket::Gui
