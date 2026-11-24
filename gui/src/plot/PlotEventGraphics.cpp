#include "QtRocket/gui/plot/PlotEventGraphics.h"

#include <optional>

#include <QColor>
#include <QIcon>
#include <QPalette>

#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/SimulationIcons.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/simulation/FlightEvent.h"

namespace QtRocket::Gui
{

QColor PlotEventGraphics::eventColor(FlightEvent::Type type)
{
    using Type = FlightEvent::Type;
    // EventGraphics.EVENT_COLORS; DEFAULT_EVENT_COLOR for the types it has no colour of.
    switch (type)
    {
        case Type::LAUNCH:
            return QColor::fromRgb(255, 0, 0);
        case Type::LIFTOFF:
            return QColor::fromRgb(0, 80, 196);
        case Type::LAUNCHROD:
            return QColor::fromRgb(0, 100, 80);
        case Type::IGNITION:
            return QColor::fromRgb(230, 130, 15);
        case Type::BURNOUT:
        case Type::EJECTION_CHARGE:
        case Type::STAGE_SEPARATION:
            return QColor::fromRgb(80, 55, 40);
        case Type::APOGEE:
            return QColor::fromRgb(15, 120, 15);
        case Type::RECOVERY_DEVICE_DEPLOYMENT:
            return QColor::fromRgb(0, 0, 128);
        case Type::GROUND_HIT:
            return QColor::fromRgb(0, 0, 0);
        case Type::SIMULATION_END:
            return QColor::fromRgb(128, 0, 0);
        case Type::TUMBLE:
            return QColor::fromRgb(196, 0, 255);
        case Type::EXCEPTION:
        case Type::SIM_ABORT:
            return QColor::fromRgb(255, 0, 0);
        case Type::SIM_WARN:
            return QColor::fromRgb(127, 127, 0);
        case Type::ALTITUDE:
            break;
    }
    return QColor::fromRgb(0, 0, 0);
}

QColor PlotEventGraphics::eventColor(FlightEvent::Type type, ThemeColors::Theme theme)
{
    const QColor color = eventColor(type);
    if (theme == ThemeColors::Theme::LIGHT)
    {
        return color;
    }
    if (color.red() == 0 && color.green() == 0 && color.blue() == 0)
    {
        return ThemeColors::themeColor(ThemeColors::Key::TEXT, ThemeColors::Theme::DARK);
    }
    return PlotColors::javaBrighter(PlotColors::javaBrighter(color));
}

QColor PlotEventGraphics::eventColor(FlightEvent::Type type, const QPalette& palette)
{
    return eventColor(type, ThemeColors::theme(palette));
}

QColor PlotEventGraphics::markerColor(FlightEvent::Type type, const QPalette& palette)
{
    QColor color = eventColor(type, palette);
    color.setAlphaF(static_cast<float>(kMarkerAlpha));
    return color;
}

QIcon PlotEventGraphics::eventIcon(FlightEvent::Type              type,
                                   std::optional<MessagePriority> warningPriority)
{
    if (type == FlightEvent::Type::SIM_WARN)
    {
        return warningPriority.has_value() ? SimulationIcons::warningIcon(*warningPriority)
                                           : QIcon();
    }
    return SimulationIcons::eventIcon(type);
}

}  // namespace QtRocket::Gui
