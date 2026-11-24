#pragma once

#include <optional>

#include <QColor>
#include <QIcon>

#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/simulation/FlightEvent.h"

class QPalette;

namespace QtRocket::Gui
{

/// How OpenRocket's simulation plots mark a flight event (swing gui/plot/EventGraphics.java): the
/// colour of an event's line and flag (getEventColor()) and the image of its icon
/// (getEventImage()). A group of events close together looks like its event of the highest
/// ordinal (SimulationPlotModel::EventGroup::lookType and lookWarningPriority), as Java's
/// SimulationPlot.fillEventLists() takes that event's colour and image.
///
/// The colours are Java's in a light palette. OpenRocket draws its charts on white in both its
/// themes; rocketplot draws a plot on the palette's dark base in a dark one (PlotTheme), where
/// QtRocket brightens them by the rule of the plots' curves (PlotColors::curveColor(): twice
/// java.awt.Color.brighter()), and draws the black ones (GROUND_HIT and the default) in the dark
/// theme's text colour (ThemeColors TEXT), which brighter() would leave almost black: the user's
/// decision U-M4-6. The images are OpenRocket's PNGs (util/SimulationIcons), the same in both
/// themes, as in OpenRocket.
class PlotEventGraphics
{
public:
    PlotEventGraphics() = delete;

    /// The opacity of an event's line and flag (SimulationPlot.plotVerticalLineMarkers():
    /// ValueMarker.setAlpha(0.7f)).
    static constexpr double kMarkerAlpha = 0.7;

    /// EventGraphics.getEventColor() of an event of @p type: its colour of EVENT_COLORS, or
    /// DEFAULT_EVENT_COLOR (black) for a type without one (ALTITUDE); opaque.
    [[nodiscard]] static QColor eventColor(FlightEvent::Type type);

    /// eventColor(@p type) as a plot in @p theme draws it: as it is in the LIGHT theme; in the
    /// DARK one ThemeColors::themeColor(TEXT, DARK) for black, and twice
    /// PlotColors::javaBrighter() of it for any other colour (see the class comment).
    [[nodiscard]] static QColor eventColor(FlightEvent::Type type, ThemeColors::Theme theme);

    /// eventColor() in the theme of @p palette (ThemeColors::theme(), PlotTheme's rule).
    [[nodiscard]] static QColor eventColor(FlightEvent::Type type, const QPalette& palette);

    /// eventColor(@p type, @p palette) with the opacity kMarkerAlpha: the colour of the line and
    /// the flag of a group of events that looks like @p type.
    [[nodiscard]] static QColor markerColor(FlightEvent::Type type, const QPalette& palette);

    /// EventGraphics.getEventImage() of an event of @p type whose warning has the priority
    /// @p warningPriority (only a SIM_WARN event has one): for SIM_WARN the image of the priority
    /// (SimulationIcons::warningIcon(): warning_low, _normal or _high), a null icon without one;
    /// for any other type SimulationIcons::eventIcon() (SIM_ABORT shares EXCEPTION's image, and
    /// ALTITUDE has none: a null icon). A null icon draws nothing, as Java's plotIconMarkers()
    /// skips a group without an image.
    [[nodiscard]] static QIcon eventIcon(FlightEvent::Type              type,
                                         std::optional<MessagePriority> warningPriority);
};

}  // namespace QtRocket::Gui
