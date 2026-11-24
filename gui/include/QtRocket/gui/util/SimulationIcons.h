#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui
{

/// OpenRocket's raster icons of the simulations, which have no SVG: the status icons of the
/// simulations table (Icons.SIMULATION_STATUS_ICON_MAP, pix/icons/) and the images of the flight
/// events in the plots (EventGraphics.getEventImage(): pix/eventicons/, and pix/icons/warning_*.png
/// for a SIM_WARN event). Each is one of OpenRocket's 16-pixel PNGs, at :/icons/status/ or
/// :/icons/events/, drawn as it is: neither recoloured nor with a dark variant, as OpenRocket
/// draws them in both its themes. The action icons and the warning levels of the tables
/// (WARNING_LOW/NORMAL/HIGH, SVGs in the theme's colours) are util/Icons.
///
/// A missing resource: statusIcon() reports it as an error condition (Guard) and gives a null
/// icon, as Icons.java's loadImageIcon() reports it and returns null; the event images give a null
/// icon without a report, as EventGraphics.loadImage() leaves the image out silently. Every
/// resource is compiled into the program (gui/resources/CMakeLists.txt), so neither happens but by
/// a mistake in the list, which the GUI tests' icon_resources_tests catch.
class SimulationIcons
{
public:
    SimulationIcons() = delete;

    /// The icon of @p status in the simulations table: tick.png for UPTODATE, LOADED and
    /// EXTERNAL, refresh.png for NOT_SIMULATED and OUTDATED, sim_cantrun.png for CANT_RUN and the
    /// flight event's event-exception.png for ABORTED.
    [[nodiscard]] static QIcon statusIcon(Simulation::Status status);

    /// The resource of statusIcon(), ":/icons/status/<file>.png" (ABORTED's is
    /// ":/icons/events/event-exception.png").
    [[nodiscard]] static QString statusResourcePath(Simulation::Status status);

    /// EventGraphics' image of an event of @p type: event-<type>.png (SIM_ABORT shares
    /// EXCEPTION's event-exception.png); a null icon for ALTITUDE, which has none, and for
    /// SIM_WARN, whose image is its warning's (eventIcon(const FlightEvent&)).
    [[nodiscard]] static QIcon eventIcon(FlightEvent::Type type);

    /// The resource of eventIcon(type), ":/icons/events/event-<type>.png"; empty for ALTITUDE and
    /// SIM_WARN.
    [[nodiscard]] static QString eventResourcePath(FlightEvent::Type type);

    /// EventGraphics.getEventImage(event): for a SIM_WARN event warningIcon() of the priority of
    /// its warning (a null icon when it carries none), for any other eventIcon() of its type.
    [[nodiscard]] static QIcon eventIcon(const FlightEvent& event);

    /// EventGraphics' image of a SIM_WARN event whose warning has @p priority (its
    /// MESSAGE_IMAGES): warning_low.png, warning_normal.png or warning_high.png, OpenRocket's
    /// raster icons, which only the plots show.
    [[nodiscard]] static QIcon warningIcon(MessagePriority priority);

    /// The resource of warningIcon(), ":/icons/status/warning_<priority>.png".
    [[nodiscard]] static QString warningResourcePath(MessagePriority priority);

    /// Every resource of this class, sorted, each once.
    [[nodiscard]] static QStringList resourcePaths();
};

}  // namespace QtRocket::Gui
