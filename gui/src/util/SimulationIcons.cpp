#include "QtRocket/gui/util/SimulationIcons.h"

#include <memory>
#include <string_view>

#include <QFile>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Status = Simulation::Status;
using Type   = FlightEvent::Type;

/// The directory of the status icons and the warnings' event images (OpenRocket's pix/icons/).
constexpr std::string_view kStatusDirectory = ":/icons/status/";

/// The directory of the flight events' images (OpenRocket's pix/eventicons/).
constexpr std::string_view kEventDirectory = ":/icons/events/";

/// Icons.java's SIM_ABORTED, which is also the EXCEPTION and SIM_ABORT events' image.
constexpr std::string_view kEventException = "event-exception.png";

QString resource(std::string_view directory, std::string_view file)
{
    return QtStrings::fromUtf8(directory) + QtStrings::fromUtf8(file);
}

/// EventGraphics' EVENT_IMAGES: the file of an event of @p type, or empty for a type without one.
std::string_view eventFile(Type type)
{
    switch (type)
    {
        case Type::LAUNCH:
            return "event-launch.png";
        case Type::LIFTOFF:
            return "event-liftoff.png";
        case Type::LAUNCHROD:
            return "event-launchrod.png";
        case Type::IGNITION:
            return "event-ignition.png";
        case Type::BURNOUT:
            return "event-burnout.png";
        case Type::EJECTION_CHARGE:
            return "event-ejection-charge.png";
        case Type::STAGE_SEPARATION:
            return "event-stage-separation.png";
        case Type::APOGEE:
            return "event-apogee.png";
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return "event-recovery-device-deployment.png";
        case Type::GROUND_HIT:
            return "event-ground-hit.png";
        case Type::SIMULATION_END:
            return "event-simulation-end.png";
        case Type::TUMBLE:
            return "event-tumble.png";
        case Type::EXCEPTION:
        case Type::SIM_ABORT:
            return kEventException;
        case Type::ALTITUDE:
        case Type::SIM_WARN:
            return {};
    }
    QTROCKET_UNREACHABLE();
}

/// The icon of the PNG @p path, or a null icon when the resource is missing.
QIcon pngIcon(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path))
    {
        return {};
    }
    return QIcon(path);
}

}  // namespace

QIcon SimulationIcons::statusIcon(Simulation::Status status)
{
    const QString path = statusResourcePath(status);
    if (!QFile::exists(path))
    {
        Guard::reportErrorCondition(QStringLiteral("Image file %1 not found, ignoring.").arg(path));
        return {};
    }
    return QIcon(path);
}

QString SimulationIcons::statusResourcePath(Simulation::Status status)
{
    switch (status)
    {
        case Status::UPTODATE:
        case Status::LOADED:
        case Status::EXTERNAL:
            return resource(kStatusDirectory, "tick.png");
        case Status::NOT_SIMULATED:
        case Status::OUTDATED:
            return resource(kStatusDirectory, "refresh.png");
        case Status::CANT_RUN:
            return resource(kStatusDirectory, "sim_cantrun.png");
        case Status::ABORTED:
            return resource(kEventDirectory, kEventException);
    }
    QTROCKET_UNREACHABLE();
}

QIcon SimulationIcons::eventIcon(FlightEvent::Type type)
{
    return pngIcon(eventResourcePath(type));
}

QString SimulationIcons::eventResourcePath(FlightEvent::Type type)
{
    const std::string_view file = eventFile(type);
    if (file.empty())
    {
        return {};
    }
    return resource(kEventDirectory, file);
}

QIcon SimulationIcons::eventIcon(const FlightEvent& event)
{
    if (event.getType() != Type::SIM_WARN)
    {
        return eventIcon(event.getType());
    }
    const std::shared_ptr<const Warning> pointer = event.getWarning();
    if (pointer == nullptr)
    {
        return {};
    }
    const Warning& warning = *pointer;
    return warningIcon(warning.priority());
}

QIcon SimulationIcons::warningIcon(MessagePriority priority)
{
    return pngIcon(warningResourcePath(priority));
}

QString SimulationIcons::warningResourcePath(MessagePriority priority)
{
    switch (priority)
    {
        case MessagePriority::LOW:
            return resource(kStatusDirectory, "warning_low.png");
        case MessagePriority::NORMAL:
            return resource(kStatusDirectory, "warning_normal.png");
        case MessagePriority::HIGH:
            return resource(kStatusDirectory, "warning_high.png");
    }
    QTROCKET_UNREACHABLE();
}

QStringList SimulationIcons::resourcePaths()
{
    QStringList paths;
    for (const Status status : Simulation::kAllStatuses)
    {
        paths.append(statusResourcePath(status));
    }
    for (const Type type : FlightEvent::kAllTypes)
    {
        const QString path = eventResourcePath(type);
        if (!path.isEmpty())
        {
            paths.append(path);
        }
    }
    for (const MessagePriority priority : kAllPriorities)
    {
        paths.append(warningResourcePath(priority));
    }
    paths.sort();
    paths.removeDuplicates();
    return paths;
}

}  // namespace QtRocket::Gui
